from __future__ import annotations

import argparse
import importlib.util
import sys
from pathlib import Path


def _load_module():
    module_path = Path("exps/generate_routerd_lab.py").resolve()
    spec = importlib.util.spec_from_file_location("generate_routerd_lab", module_path)
    if spec is None or spec.loader is None:
        raise RuntimeError("Unable to load exps/generate_routerd_lab.py")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def test_resolve_topology_shape_without_profile() -> None:
    module = _load_module()
    args = argparse.Namespace(
        profile="",
        topology="fullmesh",
        n_nodes=4,
        n_spines=2,
        n_leaves=4,
    )
    assert module.resolve_topology_shape(args) == ("fullmesh", 4, 2, 4)


def test_resolve_topology_shape_profile_overrides_shape_fields() -> None:
    module = _load_module()
    args = argparse.Namespace(
        profile="spineleaf2x4",
        topology="ring",
        n_nodes=100,
        n_spines=10,
        n_leaves=10,
    )
    assert module.resolve_topology_shape(args) == ("spineleaf", 6, 2, 4)
