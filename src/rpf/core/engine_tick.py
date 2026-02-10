from __future__ import annotations

from typing import Iterable, List

from rpf.core.convergence import ConvergenceTracker, hash_routes
from rpf.core.logging import JsonlLogger
from rpf.core.network_model import NetworkModel
from rpf.core.runtime import RouterRuntime
from rpf.core.types import ExternalEvent, RunResult


class TickEngine:
    def __init__(
        self,
        runtime: RouterRuntime,
        network_model: NetworkModel,
        max_ticks: int,
        events: Iterable[ExternalEvent] | None = None,
        logger: JsonlLogger | None = None,
        convergence_window: int = 5,
    ) -> None:
        self.runtime = runtime
        self.network_model = network_model
        self.max_ticks = int(max_ticks)
        self.events = sorted(list(events or []), key=lambda e: e.tick)
        self.logger = logger or JsonlLogger(path=None)
        self.tracker = ConvergenceTracker(stable_window=convergence_window)

    def run(self) -> RunResult:
        route_hashes: List[str] = []
        event_idx = 0

        self.runtime.bootstrap()
        for msg in self.runtime.consume_outbound():
            self.network_model.send(msg, now_tick=0)

        for tick in range(self.max_ticks):
            while event_idx < len(self.events) and self.events[event_idx].tick == tick:
                event = self.events[event_idx]
                self.runtime.handle_event(tick, event)
                self.logger.log("event_applied", tick=tick, action=event.action, params=event.params)
                event_idx += 1

            incoming = self.network_model.deliver(tick)
            self.runtime.process_tick(tick, incoming)

            for msg in self.runtime.consume_outbound():
                self.network_model.send(msg, now_tick=tick)

            route_hash = hash_routes(self.runtime.route_tables)
            route_hashes.append(route_hash)
            self.tracker.observe(tick, self.runtime.route_tables)
            self.logger.log(
                "tick",
                tick=tick,
                route_hash=route_hash,
                delivered=self.network_model.delivered_messages,
                dropped=self.network_model.dropped_messages,
                flaps=self.runtime.route_flaps,
            )

        self.logger.close()
        return RunResult(
            converged_tick=self.tracker.converged_tick,
            route_hashes=route_hashes,
            route_tables=self.runtime.route_tables,
            delivered_messages=self.network_model.delivered_messages,
            dropped_messages=self.network_model.dropped_messages,
            events_applied=event_idx,
            route_flaps=self.runtime.route_flaps,
        )
