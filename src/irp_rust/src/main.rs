use std::path::PathBuf;

use anyhow::Result;
use clap::Parser;
use irp_rust::runtime::config::load_daemon_config;
use irp_rust::runtime::daemon::RouterDaemon;
use tracing::Level;
use tracing_subscriber::EnvFilter;

#[derive(Debug, Parser)]
#[command(name = "irp-routerd-rs")]
#[command(about = "Rust router daemon for Intelligent Routing Protocol")]
struct Args {
    #[arg(long)]
    config: PathBuf,
    #[arg(long, default_value = "INFO")]
    log_level: String,
}

fn main() -> Result<()> {
    let args = Args::parse();
    init_logging(&args.log_level)?;

    let cfg = load_daemon_config(&args.config)?;
    let mut daemon = RouterDaemon::new(cfg)?;
    daemon.run_forever()?;
    Ok(())
}

fn init_logging(level: &str) -> Result<()> {
    let level = level.parse::<Level>()?;
    tracing_subscriber::fmt()
        .with_env_filter(EnvFilter::from_default_env().add_directive(level.into()))
        .with_target(true)
        .with_thread_ids(false)
        .with_thread_names(false)
        .compact()
        .init();
    Ok(())
}
