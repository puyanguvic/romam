use std::collections::BTreeMap;
use std::process::Command;

use anyhow::{Context, Result};
use tracing::{debug, info, warn};

use crate::model::routing::ForwardingEntry;
use crate::runtime::config::ForwardingConfig;

pub trait ForwardingApplier: Send {
    fn apply(&mut self, entries: &[ForwardingEntry]) -> Result<()>;
}

pub struct NullForwardingApplier;

impl ForwardingApplier for NullForwardingApplier {
    fn apply(&mut self, _entries: &[ForwardingEntry]) -> Result<()> {
        Ok(())
    }
}

pub struct LinuxForwardingApplier {
    cfg: ForwardingConfig,
    installed: BTreeMap<u32, ForwardingEntry>,
}

impl LinuxForwardingApplier {
    pub fn new(cfg: ForwardingConfig) -> Self {
        Self {
            cfg,
            installed: BTreeMap::new(),
        }
    }

    fn replace_route(&self, entry: &ForwardingEntry) -> Result<()> {
        let Some(prefix) = self.cfg.destination_prefixes.get(&entry.destination) else {
            warn!(
                "skip FIB install for dst={} next_hop={}: missing destination_prefixes mapping",
                entry.destination, entry.next_hop
            );
            return Ok(());
        };
        let Some(via) = self.cfg.next_hop_ips.get(&entry.next_hop) else {
            warn!(
                "skip FIB install for dst={} next_hop={}: missing next_hop_ips mapping",
                entry.destination, entry.next_hop
            );
            return Ok(());
        };

        let cmd = [
            "ip".to_string(),
            "route".to_string(),
            "replace".to_string(),
            prefix.to_string(),
            "via".to_string(),
            via.to_string(),
            "table".to_string(),
            self.cfg.table.to_string(),
        ];
        self.run_cmd(&cmd, false)
    }

    fn delete_route(&self, entry: &ForwardingEntry) -> Result<()> {
        let Some(prefix) = self.cfg.destination_prefixes.get(&entry.destination) else {
            return Ok(());
        };
        let cmd = [
            "ip".to_string(),
            "route".to_string(),
            "del".to_string(),
            prefix.to_string(),
            "table".to_string(),
            self.cfg.table.to_string(),
        ];
        self.run_cmd(&cmd, true)
    }

    fn run_cmd(&self, cmd: &[String], ignore_error: bool) -> Result<()> {
        if self.cfg.dry_run {
            info!("FIB dry-run: {}", cmd.join(" "));
            return Ok(());
        }

        let output = Command::new(&cmd[0])
            .args(&cmd[1..])
            .output()
            .with_context(|| format!("failed to execute command: {}", cmd.join(" ")))?;

        if output.status.success() {
            return Ok(());
        }

        if ignore_error {
            debug!(
                "ignore FIB cmd failure: {} -> {}",
                cmd.join(" "),
                String::from_utf8_lossy(&output.stderr)
            );
            return Ok(());
        }

        anyhow::bail!(
            "FIB command failed: {}\n{}",
            cmd.join(" "),
            String::from_utf8_lossy(&output.stderr)
        )
    }
}

impl ForwardingApplier for LinuxForwardingApplier {
    fn apply(&mut self, entries: &[ForwardingEntry]) -> Result<()> {
        let desired: BTreeMap<u32, ForwardingEntry> = entries
            .iter()
            .map(|entry| (entry.destination, entry.clone()))
            .collect();

        let stale_destinations: Vec<u32> = self
            .installed
            .keys()
            .filter(|destination| !desired.contains_key(destination))
            .copied()
            .collect();

        for destination in stale_destinations {
            if let Some(stale) = self.installed.get(&destination) {
                self.delete_route(stale)?;
            }
            self.installed.remove(&destination);
        }

        for (destination, entry) in &desired {
            let current = self.installed.get(destination);
            if current == Some(entry) {
                continue;
            }
            self.replace_route(entry)?;
            self.installed.insert(*destination, entry.clone());
        }

        Ok(())
    }
}
