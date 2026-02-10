from __future__ import annotations

from pathlib import Path
from typing import Any, Dict, List

from rpf.backends.base import Backend
from rpf.core.engine_tick import TickEngine
from rpf.core.logging import JsonlLogger
from rpf.core.network_model import NetworkModel
from rpf.core.runtime import RouterRuntime
from rpf.core.topology import Topology
from rpf.core.types import ExternalEvent
from rpf.utils.io import dump_json, ensure_dir, now_tag


class EmuBackend(Backend):
    def run(self, config: Dict[str, Any]) -> Dict[str, Any]:
        seed = int(config.get("seed", 42))
        protocol_name = str(config.get("protocol", "ospf_like"))
        protocol_cfg = config.get("protocol_params", {})

        topology = Topology.from_config(config.get("topology", {}), seed=seed)
        runtime = RouterRuntime(
            topology=topology,
            protocol_name=protocol_name,
            protocol_config=protocol_cfg,
        )

        engine_cfg = config.get("engine", {})
        network_cfg = config.get("network", engine_cfg.get("network", {}))
        network = NetworkModel(
            base_delay=int(network_cfg.get("base_delay", 1)),
            jitter=int(network_cfg.get("jitter", 0)),
            loss_prob=float(network_cfg.get("loss_prob", 0.0)),
            seed=seed,
        )

        events = self._parse_events(config.get("failures", []))
        max_ticks = int(engine_cfg.get("max_ticks", config.get("max_ticks", 80)))

        output_dir = Path(config.get("output_dir", "results/runs"))
        ensure_dir(output_dir)
        run_id = f"{config.get('name', 'run')}_{now_tag()}"
        run_dir = ensure_dir(output_dir / run_id)
        logger = JsonlLogger(run_dir / "events.jsonl")

        engine = TickEngine(
            runtime=runtime,
            network_model=network,
            max_ticks=max_ticks,
            events=events,
            logger=logger,
            convergence_window=int(config.get("convergence_window", 5)),
        )
        result = engine.run()

        result_payload = {
            "run_id": run_id,
            "name": config.get("name", "run"),
            "seed": seed,
            "protocol": protocol_name,
            "converged_tick": result.converged_tick,
            "route_hashes": result.route_hashes,
            "route_tables": result.route_tables,
            "delivered_messages": result.delivered_messages,
            "dropped_messages": result.dropped_messages,
            "events_applied": result.events_applied,
            "route_flaps": result.route_flaps,
            "topology_edges": [e.__dict__ for e in topology.edge_list()],
        }
        dump_json(run_dir / "result.json", result_payload)
        dump_json(run_dir / "config.effective.json", config)

        return result_payload

    @staticmethod
    def _parse_events(events_cfg: List[Dict[str, Any]]) -> List[ExternalEvent]:
        events = []
        for row in events_cfg:
            tick = int(row["tick"])
            action = str(row["action"])
            params = {k: v for k, v in row.items() if k not in {"tick", "action"}}
            events.append(ExternalEvent(tick=tick, action=action, params=params))
        return sorted(events, key=lambda e: e.tick)
