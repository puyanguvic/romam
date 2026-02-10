from __future__ import annotations

from pathlib import Path
from typing import Any, Dict

from rpf.backends.emu import EmuBackend
from rpf.cli.validate import validate_config
from rpf.utils.io import deep_merge, load_yaml


def load_effective_config(config_path: str) -> Dict[str, Any]:
    cfg_path = Path(config_path).resolve()
    parts = cfg_path.parts
    if "configs" in parts:
        cfg_idx = parts.index("configs")
        root = Path(*parts[:cfg_idx]) if cfg_idx > 0 else Path("/")
    else:
        root = cfg_path.parent
    defaults_path = root / "configs" / "defaults.yaml"

    cfg = {}
    if defaults_path.exists():
        cfg = load_yaml(defaults_path)

    exp = load_yaml(config_path)
    cfg = deep_merge(cfg, exp)

    if "protocol_config" in cfg:
        pfile = Path(cfg["protocol_config"])
        if not pfile.is_absolute():
            pfile = root / pfile
        if pfile.exists():
            proto_cfg = load_yaml(pfile)
            cfg["protocol_params"] = deep_merge(cfg.get("protocol_params", {}), proto_cfg)

    if "engine_config" in cfg:
        efile = Path(cfg["engine_config"])
        if not efile.is_absolute():
            efile = root / efile
        if efile.exists():
            engine_cfg = load_yaml(efile)
            cfg["engine"] = deep_merge(engine_cfg, cfg.get("engine", {}))

    return cfg


def run_emu(config_path: str) -> Dict[str, Any]:
    cfg = load_effective_config(config_path)
    errors = validate_config(cfg)
    if errors:
        raise ValueError("Invalid config: " + "; ".join(errors))
    return EmuBackend().run(cfg)
