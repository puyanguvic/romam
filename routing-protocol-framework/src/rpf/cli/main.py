from __future__ import annotations

import argparse
import json

from rpf.cli.run_emu import run_emu
from rpf.cli.run_mininet import run_mininet
from rpf.cli.validate import validate_config
from rpf.utils.io import load_yaml


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="rpf", description="Routing Protocol Framework CLI")
    sub = parser.add_subparsers(dest="cmd", required=True)

    p_run = sub.add_parser("run", help="Run emulation experiment")
    p_run.add_argument("--config", required=True)

    p_run_emu = sub.add_parser("run-emu", help="Run emulation experiment")
    p_run_emu.add_argument("--config", required=True)

    p_run_mn = sub.add_parser("run-mininet", help="Run Mininet experiment")
    p_run_mn.add_argument("--config", required=True)

    p_validate = sub.add_parser("validate", help="Validate a config file")
    p_validate.add_argument("--config", required=True)

    return parser


def main() -> None:
    args = build_parser().parse_args()

    if args.cmd in {"run", "run-emu"}:
        result = run_emu(args.config)
        print(json.dumps(result, indent=2, ensure_ascii=False, sort_keys=True))
        return

    if args.cmd == "run-mininet":
        result = run_mininet(args.config)
        print(json.dumps(result, indent=2, ensure_ascii=False, sort_keys=True))
        return

    if args.cmd == "validate":
        cfg = load_yaml(args.config)
        errors = validate_config(cfg)
        if errors:
            print(json.dumps({"ok": False, "errors": errors}, ensure_ascii=False, indent=2))
        else:
            print(json.dumps({"ok": True}, ensure_ascii=False, indent=2))
        return


if __name__ == "__main__":
    main()
