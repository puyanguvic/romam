from __future__ import annotations

import argparse
import importlib.util
import sys
from pathlib import Path


def _load_module():
    module_path = Path("tools/generate_routerd_lab.py").resolve()
    spec = importlib.util.spec_from_file_location("generate_routerd_lab", module_path)
    if spec is None or spec.loader is None:
        raise RuntimeError("Unable to load tools/generate_routerd_lab.py")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def test_resolve_source_topology_file_from_profile() -> None:
    module = _load_module()
    args = argparse.Namespace(profile="ring6", topology_file="")
    resolved = module.resolve_source_topology_file(args)
    assert resolved.name == "ring6.clab.yaml"
    assert resolved.parent.name == "topologies"


def test_resolve_source_topology_file_from_abilene_profile() -> None:
    module = _load_module()
    args = argparse.Namespace(profile="abilene", topology_file="")
    resolved = module.resolve_source_topology_file(args)
    assert resolved.name == "abilene.clab.yaml"
    assert resolved.parent.name == "topologies"


def test_resolve_source_topology_file_from_geant_profile() -> None:
    module = _load_module()
    args = argparse.Namespace(profile="geant", topology_file="")
    resolved = module.resolve_source_topology_file(args)
    assert resolved.name == "geant.clab.yaml"
    assert resolved.parent.name == "topologies"


def test_resolve_source_topology_file_from_uunet_profile() -> None:
    module = _load_module()
    args = argparse.Namespace(profile="uunet", topology_file="")
    resolved = module.resolve_source_topology_file(args)
    assert resolved.name == "uunet.clab.yaml"
    assert resolved.parent.name == "topologies"


def test_resolve_source_topology_file_from_cernet_profile() -> None:
    module = _load_module()
    args = argparse.Namespace(profile="cernet", topology_file="")
    resolved = module.resolve_source_topology_file(args)
    assert resolved.name == "cernet.clab.yaml"
    assert resolved.parent.name == "topologies"


def test_resolve_source_topology_file_prefers_explicit_file(tmp_path: Path) -> None:
    module = _load_module()
    source = tmp_path / "x.clab.yaml"
    source.write_text(
        "name: x\ntopology:\n  nodes: {a: {kind: linux}}\n  links: []\n",
        encoding="utf-8",
    )
    args = argparse.Namespace(profile="ring6", topology_file=str(source))
    assert module.resolve_source_topology_file(args) == source.resolve()
