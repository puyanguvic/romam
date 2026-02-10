from __future__ import annotations

from typing import Any, Dict


def validate_config(cfg: Dict[str, Any]) -> list[str]:
    errors: list[str] = []

    if "topology" not in cfg:
        errors.append("Missing 'topology' config")
    if "protocol" not in cfg:
        errors.append("Missing 'protocol' config")

    topo = cfg.get("topology", {})
    if not isinstance(topo, dict):
        errors.append("'topology' must be a dict")
    elif "type" not in topo:
        errors.append("topology.type is required")

    engine = cfg.get("engine", {})
    if isinstance(engine, dict) and int(engine.get("max_ticks", 1)) <= 0:
        errors.append("engine.max_ticks must be > 0")

    return errors
