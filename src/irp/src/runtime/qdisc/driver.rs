use std::process::Command;

use anyhow::{Context, Result};
use tracing::info;

use crate::runtime::qdisc::strategy::QdiscProfile;

#[derive(Debug, Clone, Default)]
pub struct QdiscRuntimeStats {
    pub kind: Option<String>,
    pub backlog_bytes: Option<u64>,
    pub backlog_packets: Option<u64>,
    pub drops: Option<u64>,
    pub overlimits: Option<u64>,
    pub requeues: Option<u64>,
    pub raw: String,
}

impl QdiscRuntimeStats {
    pub fn parse(tc_output: &str) -> Self {
        let mut out = Self {
            raw: tc_output.to_string(),
            ..Self::default()
        };
        for line in tc_output.lines() {
            let tokens: Vec<&str> = line.split_whitespace().collect();
            if out.kind.is_none() && tokens.len() >= 2 && tokens[0] == "qdisc" {
                out.kind = Some(tokens[1].to_string());
            }
            for (idx, token) in tokens.iter().enumerate() {
                if *token == "backlog" {
                    for candidate in tokens.iter().skip(idx + 1).take(6) {
                        if let Some(raw) = candidate.strip_suffix('b') {
                            if let Ok(v) = raw.parse::<u64>() {
                                out.backlog_bytes = Some(v);
                            }
                        }
                        if let Some(raw) = candidate.strip_suffix('p') {
                            if let Ok(v) = raw.parse::<u64>() {
                                out.backlog_packets = Some(v);
                            }
                        }
                    }
                } else if *token == "(dropped" && idx + 1 < tokens.len() {
                    out.drops = parse_u64_token(tokens[idx + 1].trim_end_matches(','));
                } else if *token == "overlimits" && idx + 1 < tokens.len() {
                    out.overlimits = parse_u64_token(tokens[idx + 1].trim_end_matches(','));
                } else if *token == "requeues" && idx + 1 < tokens.len() {
                    out.requeues = parse_u64_token(tokens[idx + 1].trim_end_matches(','));
                }
            }
        }
        out
    }
}

fn parse_u64_token(s: &str) -> Option<u64> {
    s.trim().parse::<u64>().ok()
}

pub trait QdiscDriver: Send {
    fn replace_root(&self, iface: &str, profile: &QdiscProfile) -> Result<()>;
    fn delete_root(&self, iface: &str) -> Result<()>;
    fn show_stats(&self, iface: &str) -> Result<QdiscRuntimeStats>;
}

pub struct LinuxTcQdiscDriver {
    dry_run: bool,
}

impl LinuxTcQdiscDriver {
    pub fn new(dry_run: bool) -> Self {
        Self { dry_run }
    }

    fn run_tc(&self, args: &[String], ignore_error: bool) -> Result<String> {
        if self.dry_run {
            info!("qdisc dry-run: tc {}", args.join(" "));
            return Ok(String::new());
        }
        let output = Command::new("tc")
            .args(args)
            .output()
            .with_context(|| format!("failed to execute tc {}", args.join(" ")))?;
        let stdout = String::from_utf8_lossy(&output.stdout).to_string();
        let stderr = String::from_utf8_lossy(&output.stderr).to_string();
        if output.status.success() || ignore_error {
            return Ok(stdout);
        }
        anyhow::bail!("tc {} failed: {}", args.join(" "), stderr.trim())
    }

    fn build_replace_root_args(&self, iface: &str, profile: &QdiscProfile) -> Vec<String> {
        let mut args = vec![
            "qdisc".to_string(),
            "replace".to_string(),
            "dev".to_string(),
            iface.to_string(),
            "root".to_string(),
        ];
        if let Some(handle) = &profile.handle {
            args.push("handle".to_string());
            args.push(handle.clone());
        }
        args.push(profile.kind.as_tc_name().to_string());
        for (k, v) in &profile.params {
            args.push(k.clone());
            if !v.is_empty() {
                args.push(v.clone());
            }
        }
        args
    }
}

impl QdiscDriver for LinuxTcQdiscDriver {
    fn replace_root(&self, iface: &str, profile: &QdiscProfile) -> Result<()> {
        let args = self.build_replace_root_args(iface, profile);
        let _ = self.run_tc(&args, false)?;
        Ok(())
    }

    fn delete_root(&self, iface: &str) -> Result<()> {
        let args = vec![
            "qdisc".to_string(),
            "del".to_string(),
            "dev".to_string(),
            iface.to_string(),
            "root".to_string(),
        ];
        let _ = self.run_tc(&args, true)?;
        Ok(())
    }

    fn show_stats(&self, iface: &str) -> Result<QdiscRuntimeStats> {
        let args = vec![
            "-s".to_string(),
            "qdisc".to_string(),
            "show".to_string(),
            "dev".to_string(),
            iface.to_string(),
        ];
        let text = self.run_tc(&args, false)?;
        Ok(QdiscRuntimeStats::parse(&text))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parse_tc_stats_backlog() {
        let text = "qdisc fq_codel 0: dev eth1 root refcnt 2 limit 10240p\n\
 Sent 1234 bytes 12 pkt (dropped 3, overlimits 5 requeues 7)\n\
 backlog 8192b 6p requeues 7\n";
        let stats = QdiscRuntimeStats::parse(text);
        assert_eq!(stats.kind.as_deref(), Some("fq_codel"));
        assert_eq!(stats.backlog_bytes, Some(8192));
        assert_eq!(stats.backlog_packets, Some(6));
        assert_eq!(stats.drops, Some(3));
        assert_eq!(stats.overlimits, Some(5));
        assert_eq!(stats.requeues, Some(7));
    }
}
