from __future__ import annotations

import argparse
import logging

from rpf.runtime import RouterDaemon, load_daemon_config


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Router daemon for intelligent routing protocol framework."
    )
    parser.add_argument("--config", required=True, help="YAML config path for this router daemon.")
    parser.add_argument(
        "--log-level",
        default="INFO",
        choices=["DEBUG", "INFO", "WARNING", "ERROR"],
        help="Logging verbosity.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    logging.basicConfig(
        level=getattr(logging, args.log_level),
        format="%(asctime)s %(levelname)s %(name)s: %(message)s",
    )
    cfg = load_daemon_config(args.config)
    daemon = RouterDaemon(cfg)
    daemon.run_forever()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
