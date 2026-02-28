use std::collections::{BTreeMap, BTreeSet};
use std::str::FromStr;

use anyhow::Result;

use crate::runtime::config::{QdiscConfig, QdiscProfileConfig};
use crate::runtime::qdisc::driver::{QdiscDriver, QdiscRuntimeStats};
use crate::runtime::qdisc::strategy::{QdiscKind, QdiscProfile};

pub struct QdiscController {
    driver: Box<dyn QdiscDriver>,
    default_profile: Option<QdiscProfile>,
    interface_profiles: BTreeMap<String, QdiscProfile>,
}

impl QdiscController {
    pub fn new(driver: Box<dyn QdiscDriver>, cfg: &QdiscConfig) -> Result<Self> {
        let default_profile = cfg.default.as_ref().map(parse_profile).transpose()?;
        let mut interface_profiles = BTreeMap::new();
        for (iface, raw) in &cfg.interfaces {
            interface_profiles.insert(iface.clone(), parse_profile(raw)?);
        }
        Ok(Self {
            driver,
            default_profile,
            interface_profiles,
        })
    }

    pub fn apply_to_interfaces(&self, ifaces: &[String]) -> Result<()> {
        let unique_ifaces: BTreeSet<String> = ifaces
            .iter()
            .map(String::as_str)
            .filter(|name| !name.trim().is_empty())
            .map(str::to_string)
            .collect();

        for iface in unique_ifaces {
            self.apply_for_interface(&iface)?;
        }
        Ok(())
    }

    pub fn apply_for_interface(&self, iface: &str) -> Result<()> {
        if let Some(profile) = self.interface_profiles.get(iface) {
            return self.driver.replace_root(iface, profile);
        }
        if let Some(profile) = &self.default_profile {
            return self.driver.replace_root(iface, profile);
        }
        Ok(())
    }

    pub fn clear_for_interface(&self, iface: &str) -> Result<()> {
        self.driver.delete_root(iface)
    }

    pub fn apply_profile_for_interface(&self, iface: &str, profile: &QdiscProfile) -> Result<()> {
        self.driver.replace_root(iface, profile)
    }

    pub fn apply_custom_for_interface(
        &self,
        iface: &str,
        kind: &str,
        handle: Option<&str>,
        params: &BTreeMap<String, String>,
    ) -> Result<()> {
        let kind = QdiscKind::from_str(kind).map_err(anyhow::Error::msg)?;
        let profile = QdiscProfile {
            kind,
            handle: handle.map(str::to_string),
            parent: None,
            params: params.clone(),
        };
        self.apply_profile_for_interface(iface, &profile)
    }

    pub fn stats_for_interface(&self, iface: &str) -> Result<QdiscRuntimeStats> {
        self.driver.show_stats(iface)
    }
}

fn parse_profile(raw: &QdiscProfileConfig) -> Result<QdiscProfile> {
    let kind = QdiscKind::from_str(&raw.kind).map_err(anyhow::Error::msg)?;
    Ok(QdiscProfile {
        kind,
        handle: raw.handle.clone(),
        parent: raw.parent.clone(),
        params: raw.params.clone(),
    })
}

#[cfg(test)]
mod tests {
    use std::sync::{Arc, Mutex};

    use super::*;

    #[derive(Default)]
    struct DriverCalls {
        replaced: Vec<(String, QdiscProfile)>,
        deleted: Vec<String>,
    }

    struct FakeDriver {
        calls: Arc<Mutex<DriverCalls>>,
    }

    impl QdiscDriver for FakeDriver {
        fn replace_root(&self, iface: &str, profile: &QdiscProfile) -> Result<()> {
            self.calls
                .lock()
                .expect("driver lock poisoned")
                .replaced
                .push((iface.to_string(), profile.clone()));
            Ok(())
        }

        fn delete_root(&self, iface: &str) -> Result<()> {
            self.calls
                .lock()
                .expect("driver lock poisoned")
                .deleted
                .push(iface.to_string());
            Ok(())
        }

        fn show_stats(&self, _iface: &str) -> Result<QdiscRuntimeStats> {
            Ok(QdiscRuntimeStats::default())
        }
    }

    #[test]
    fn apply_custom_for_interface_uses_requested_kind() {
        let calls = Arc::new(Mutex::new(DriverCalls::default()));
        let driver = Box::new(FakeDriver {
            calls: Arc::clone(&calls),
        });
        let controller = QdiscController::new(driver, &QdiscConfig::default())
            .expect("controller should initialize");

        let mut params = BTreeMap::new();
        params.insert("bands".to_string(), "3".to_string());
        controller
            .apply_custom_for_interface("eth1", "prio", Some("1:"), &params)
            .expect("custom profile should apply");

        let guard = calls.lock().expect("driver lock poisoned");
        assert_eq!(guard.replaced.len(), 1);
        assert_eq!(guard.replaced[0].0, "eth1");
        assert_eq!(guard.replaced[0].1.kind, QdiscKind::Prio);
        assert_eq!(guard.replaced[0].1.handle.as_deref(), Some("1:"));
        assert_eq!(guard.replaced[0].1.params.get("bands"), Some(&"3".to_string()));
    }

    #[test]
    fn apply_for_interface_prefers_interface_specific_profile() {
        let calls = Arc::new(Mutex::new(DriverCalls::default()));
        let driver = Box::new(FakeDriver {
            calls: Arc::clone(&calls),
        });
        let mut cfg = QdiscConfig::default();
        cfg.default = Some(QdiscProfileConfig {
            kind: "fifo".to_string(),
            handle: None,
            parent: None,
            params: BTreeMap::new(),
        });
        cfg.interfaces.insert(
            "eth1".to_string(),
            QdiscProfileConfig {
                kind: "prio".to_string(),
                handle: None,
                parent: None,
                params: BTreeMap::new(),
            },
        );

        let controller = QdiscController::new(driver, &cfg).expect("controller should initialize");
        controller
            .apply_for_interface("eth1")
            .expect("interface profile should apply");

        let guard = calls.lock().expect("driver lock poisoned");
        assert_eq!(guard.replaced.len(), 1);
        assert_eq!(guard.replaced[0].0, "eth1");
        assert_eq!(guard.replaced[0].1.kind, QdiscKind::Prio);
    }
}
