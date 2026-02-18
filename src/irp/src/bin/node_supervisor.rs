use std::collections::BTreeMap;
use std::fs::{self, OpenOptions};
use std::path::{Path, PathBuf};
use std::process::{Child, Command, Stdio};
use std::sync::{
    atomic::{AtomicBool, Ordering},
    Arc,
};
use std::thread;
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};

use anyhow::{Context, Result};
use clap::Parser;
use serde::{Deserialize, Serialize};

#[derive(Debug, Parser)]
#[command(name = "node_supervisor")]
#[command(about = "Node-level process supervisor for routingd and app processes")]
struct Cli {
    #[arg(long)]
    config: PathBuf,
}

#[derive(Debug, Clone, Deserialize)]
struct SupervisorConfig {
    #[serde(default)]
    node_id: String,
    #[serde(default = "default_tick_ms")]
    tick_ms: u64,
    #[serde(default = "default_state_file")]
    state_file: String,
    #[serde(default)]
    routerd: Option<ProcessSpec>,
    #[serde(default)]
    apps: Vec<ProcessSpec>,
}

#[derive(Debug, Clone, Deserialize)]
struct ProcessSpec {
    name: String,
    #[serde(default)]
    kind: String,
    bin: String,
    #[serde(default)]
    args: Vec<String>,
    #[serde(default)]
    env: BTreeMap<String, String>,
    #[serde(default = "default_enabled")]
    enabled: bool,
    #[serde(default)]
    restart: Option<RestartPolicy>,
    #[serde(default)]
    max_restarts: Option<i32>,
    #[serde(default)]
    start_delay_s: f64,
    #[serde(default)]
    log_file: Option<String>,
}

#[derive(Debug, Clone, Copy, Deserialize, Serialize, PartialEq, Eq)]
#[serde(rename_all = "kebab-case")]
enum RestartPolicy {
    Never,
    OnFailure,
    Always,
}

fn default_tick_ms() -> u64 {
    500
}

fn default_state_file() -> String {
    "/tmp/node_supervisor_state.json".to_string()
}

fn default_enabled() -> bool {
    true
}

fn default_policy_routerd() -> RestartPolicy {
    RestartPolicy::Always
}

fn default_policy_app() -> RestartPolicy {
    RestartPolicy::Never
}

fn default_max_restarts(policy: RestartPolicy) -> i32 {
    match policy {
        RestartPolicy::Never => 0,
        RestartPolicy::OnFailure => 10,
        RestartPolicy::Always => -1,
    }
}

#[derive(Debug)]
struct ManagedProcess {
    spec: ProcessSpec,
    policy: RestartPolicy,
    max_restarts: i32,
    log_file: String,
    start_after: Duration,
    child: Option<Child>,
    started_once: bool,
    pending_restart: bool,
    launch_count: u32,
    restart_count: u32,
    last_exit_code: Option<i32>,
    last_error: String,
}

impl ManagedProcess {
    fn from_spec(spec: ProcessSpec, default_policy: RestartPolicy) -> Self {
        let policy = spec.restart.unwrap_or(default_policy);
        let max_restarts = spec
            .max_restarts
            .unwrap_or_else(|| default_max_restarts(policy));
        let log_file = spec
            .log_file
            .clone()
            .unwrap_or_else(|| format!("/tmp/{}.log", spec.name));
        let start_after = Duration::from_secs_f64(spec.start_delay_s.max(0.0));

        Self {
            spec,
            policy,
            max_restarts,
            log_file,
            start_after,
            child: None,
            started_once: false,
            pending_restart: true,
            launch_count: 0,
            restart_count: 0,
            last_exit_code: None,
            last_error: String::new(),
        }
    }

    fn is_running(&mut self) -> Result<bool> {
        if let Some(child) = self.child.as_mut() {
            if child.try_wait()?.is_none() {
                return Ok(true);
            }
        }
        Ok(false)
    }

    fn handle_exit(&mut self) -> Result<()> {
        let Some(mut child) = self.child.take() else {
            return Ok(());
        };
        let status = child.wait().context("failed waiting child process")?;
        let code = status.code().unwrap_or(255);
        self.last_exit_code = Some(code);

        let should_retry = match self.policy {
            RestartPolicy::Never => false,
            RestartPolicy::Always => true,
            RestartPolicy::OnFailure => code != 0,
        };

        if should_retry && self.allow_restart() {
            self.pending_restart = true;
        } else {
            self.pending_restart = false;
        }
        Ok(())
    }

    fn allow_restart(&self) -> bool {
        if self.max_restarts < 0 {
            return true;
        }
        (self.restart_count as i32) < self.max_restarts
    }

    fn maybe_start(&mut self, node_id: &str, start_instant: Instant, now: Instant) -> Result<()> {
        if !self.spec.enabled {
            return Ok(());
        }
        if self.child.is_some() {
            return Ok(());
        }
        if !self.pending_restart {
            return Ok(());
        }
        if !self.started_once && now.duration_since(start_instant) < self.start_after {
            return Ok(());
        }

        let mut cmd = Command::new(&self.spec.bin);
        cmd.args(&self.spec.args)
            .envs(self.spec.env.iter().map(|(k, v)| (k, v)));

        if !self.spec.env.contains_key("NODE_ID") && !node_id.is_empty() {
            cmd.env("NODE_ID", node_id);
        }
        if !self.spec.env.contains_key("APP_ID") {
            cmd.env("APP_ID", &self.spec.name);
        }
        if !self.spec.env.contains_key("APP_ROLE") {
            cmd.env("APP_ROLE", &self.spec.kind);
        }

        let log_path = PathBuf::from(&self.log_file);
        if let Some(parent) = log_path.parent() {
            fs::create_dir_all(parent).with_context(|| {
                format!("failed to create log directory for {}", log_path.display())
            })?;
        }
        let stdout = OpenOptions::new()
            .create(true)
            .append(true)
            .open(&log_path)
            .with_context(|| format!("failed to open log file: {}", log_path.display()))?;
        let stderr = stdout
            .try_clone()
            .with_context(|| format!("failed to clone log file handle: {}", log_path.display()))?;

        cmd.stdout(Stdio::from(stdout)).stderr(Stdio::from(stderr));

        match cmd.spawn() {
            Ok(child) => {
                if self.started_once {
                    self.restart_count = self.restart_count.saturating_add(1);
                }
                self.started_once = true;
                self.launch_count = self.launch_count.saturating_add(1);
                self.pending_restart = false;
                self.last_error.clear();
                self.child = Some(child);
                Ok(())
            }
            Err(err) => {
                self.last_exit_code = Some(127);
                self.last_error = format!("spawn failed: {err}");
                self.pending_restart = self.allow_restart() && self.policy != RestartPolicy::Never;
                Ok(())
            }
        }
    }

    fn shutdown(&mut self) {
        if let Some(mut child) = self.child.take() {
            let _ = child.kill();
            let _ = child.wait();
        }
    }

    fn status(&self) -> ProcessStatus {
        ProcessStatus {
            name: self.spec.name.clone(),
            kind: self.spec.kind.clone(),
            bin: self.spec.bin.clone(),
            args: self.spec.args.clone(),
            enabled: self.spec.enabled,
            running: self.child.is_some(),
            pid: self.child.as_ref().map(|c| c.id()),
            restart_policy: self.policy,
            max_restarts: self.max_restarts,
            restart_count: self.restart_count,
            launch_count: self.launch_count,
            pending_restart: self.pending_restart,
            last_exit_code: self.last_exit_code,
            last_error: self.last_error.clone(),
            log_file: self.log_file.clone(),
        }
    }
}

#[derive(Debug, Serialize)]
struct ProcessStatus {
    name: String,
    kind: String,
    bin: String,
    args: Vec<String>,
    enabled: bool,
    running: bool,
    pid: Option<u32>,
    restart_policy: RestartPolicy,
    max_restarts: i32,
    restart_count: u32,
    launch_count: u32,
    pending_restart: bool,
    last_exit_code: Option<i32>,
    last_error: String,
    log_file: String,
}

#[derive(Debug, Serialize)]
struct SupervisorState {
    node_id: String,
    timestamp_unix_ms: u128,
    tick_ms: u64,
    processes: Vec<ProcessStatus>,
}

fn load_config(path: &Path) -> Result<SupervisorConfig> {
    let text = fs::read_to_string(path)
        .with_context(|| format!("failed to read config: {}", path.display()))?;
    let cfg: SupervisorConfig =
        serde_yaml::from_str(&text).with_context(|| "failed to parse supervisor config")?;
    Ok(cfg)
}

fn now_unix_ms() -> u128 {
    match SystemTime::now().duration_since(UNIX_EPOCH) {
        Ok(dur) => dur.as_millis(),
        Err(_) => 0,
    }
}

fn write_state(path: &Path, state: &SupervisorState) -> Result<()> {
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent)
            .with_context(|| format!("failed to create state dir: {}", parent.display()))?;
    }
    let payload = serde_json::to_vec_pretty(state).context("failed to encode state json")?;
    let tmp_path = path.with_extension("json.tmp");
    fs::write(&tmp_path, payload)
        .with_context(|| format!("failed to write temp state file: {}", tmp_path.display()))?;
    fs::rename(&tmp_path, path)
        .with_context(|| format!("failed to rename state file: {}", path.display()))?;
    Ok(())
}

fn main() -> Result<()> {
    let cli = Cli::parse();
    let cfg = load_config(&cli.config)?;
    let node_id = cfg.node_id.trim().to_string();

    let mut managed: Vec<ManagedProcess> = Vec::new();
    if let Some(mut spec) = cfg.routerd {
        if spec.kind.is_empty() {
            spec.kind = "routingd".to_string();
        }
        managed.push(ManagedProcess::from_spec(spec, default_policy_routerd()));
    }
    for mut spec in cfg.apps {
        if spec.kind.is_empty() {
            spec.kind = "app".to_string();
        }
        managed.push(ManagedProcess::from_spec(spec, default_policy_app()));
    }

    let stop = Arc::new(AtomicBool::new(false));
    let stop_signal = Arc::clone(&stop);
    ctrlc::set_handler(move || {
        stop_signal.store(true, Ordering::SeqCst);
    })
    .context("failed to install signal handler")?;

    let tick = Duration::from_millis(cfg.tick_ms.max(100));
    let state_path = PathBuf::from(cfg.state_file);
    let start_instant = Instant::now();

    while !stop.load(Ordering::SeqCst) {
        let now = Instant::now();

        for proc in &mut managed {
            if proc.is_running()? {
                continue;
            }
            if proc.child.is_some() {
                proc.handle_exit()?;
            }
            proc.maybe_start(&node_id, start_instant, now)?;
        }

        let state = SupervisorState {
            node_id: node_id.clone(),
            timestamp_unix_ms: now_unix_ms(),
            tick_ms: cfg.tick_ms,
            processes: managed.iter().map(ManagedProcess::status).collect(),
        };
        let _ = write_state(&state_path, &state);

        thread::sleep(tick);
    }

    for proc in &mut managed {
        proc.shutdown();
    }

    let final_state = SupervisorState {
        node_id,
        timestamp_unix_ms: now_unix_ms(),
        tick_ms: cfg.tick_ms,
        processes: managed.iter().map(ManagedProcess::status).collect(),
    };
    let _ = write_state(&state_path, &final_state);

    Ok(())
}
