from __future__ import annotations

import logging
import signal
import time
from dataclasses import dataclass
from typing import Dict

from rpf.model.messages import decode_message, encode_message
from rpf.model.routing import ForwardingTable, RouteTable
from rpf.model.state import NeighborInfo, NeighborTable
from rpf.protocols.base import ProtocolContext, ProtocolEngine, RouterLink
from rpf.protocols.ospf import OspfProtocol, OspfTimers
from rpf.protocols.rip import RipProtocol, RipTimers
from rpf.runtime.config import DaemonConfig
from rpf.runtime.forwarding import LinuxForwardingApplier, NullForwardingApplier
from rpf.runtime.transport import UdpTransport


@dataclass
class DaemonRuntime:
    daemon: "RouterDaemon"

    def run_forever(self) -> None:
        self.daemon.run_forever()


class RouterDaemon:
    def __init__(self, config: DaemonConfig, logger: logging.Logger | None = None) -> None:
        self._cfg = config
        self._log = logger or logging.getLogger("rpf.routerd")
        self._transport = UdpTransport(bind_address=config.bind_address, bind_port=config.bind_port)
        self._neighbor_table = NeighborTable(
            [
                NeighborInfo(
                    router_id=neighbor.router_id,
                    address=neighbor.address,
                    port=neighbor.port,
                    cost=neighbor.cost,
                )
                for neighbor in config.neighbors
            ]
        )
        self._protocol = self._build_protocol(config)
        self._route_table = RouteTable()
        self._forwarding_table = ForwardingTable()
        if config.forwarding.enabled:
            self._applier = LinuxForwardingApplier(config.forwarding, self._log)
        else:
            self._applier = NullForwardingApplier()
        self._running = True

    def run_forever(self) -> None:
        self._install_signal_handlers()
        self._log.info(
            "routerd start: router_id=%s protocol=%s bind=%s:%s neighbors=%s",
            self._cfg.router_id,
            self._cfg.protocol,
            self._cfg.bind_address,
            self._cfg.bind_port,
            [n.router_id for n in self._cfg.neighbors],
        )

        start_outputs = self._protocol.start(self._context(time.monotonic()))
        self._apply_outputs(start_outputs)

        next_tick = time.monotonic() + self._cfg.tick_interval
        try:
            while self._running:
                now = time.monotonic()
                timeout_s = max(0.0, next_tick - now)
                incoming = self._transport.recv(timeout_s=timeout_s)
                if incoming is not None:
                    self._handle_packet(incoming[0], now=time.monotonic())

                now = time.monotonic()
                if now >= next_tick:
                    self._neighbor_table.refresh_liveness(
                        now,
                        dead_interval=self._cfg.dead_interval,
                    )
                    timer_outputs = self._protocol.on_timer(self._context(now))
                    self._apply_outputs(timer_outputs)
                    next_tick = now + self._cfg.tick_interval
        finally:
            self._transport.close()
            self._log.info("routerd stopped")

    def stop(self) -> None:
        self._running = False

    def _handle_packet(self, payload: bytes, now: float) -> None:
        try:
            message = decode_message(payload)
        except Exception as exc:  # noqa: BLE001
            self._log.warning("drop invalid packet: %s", exc)
            return
        if message.protocol != self._protocol.name:
            return
        if self._neighbor_table.get(message.src_router_id) is None:
            self._log.debug("drop packet from unknown router_id=%s", message.src_router_id)
            return
        self._neighbor_table.mark_seen(message.src_router_id, now)
        outputs = self._protocol.on_message(self._context(now), message)
        self._apply_outputs(outputs)

    def _apply_outputs(self, outputs) -> None:
        for neighbor_id, message in outputs.outbound:
            neighbor = self._neighbor_table.get(neighbor_id)
            if neighbor is None:
                continue
            self._transport.send(encode_message(message), neighbor.address, neighbor.port)
        if outputs.routes is None:
            return
        updated = self._route_table.replace_protocol_routes(self._protocol.name, outputs.routes)
        if not updated:
            return
        fib_updated = self._forwarding_table.sync_from_routes(self._route_table.snapshot())
        if not fib_updated:
            return
        fib_entries = self._forwarding_table.snapshot()
        self._applier.apply(fib_entries)
        summary = [(e.destination, e.next_hop, e.metric) for e in fib_entries]
        self._log.info("RIB/FIB updated: %s", summary)

    def _context(self, now: float) -> ProtocolContext:
        links: Dict[int, RouterLink] = {}
        for rid, info in self._neighbor_table.items():
            links[rid] = RouterLink(
                neighbor_id=rid,
                cost=info.cost,
                address=info.address,
                port=info.port,
                is_up=bool(info.is_up),
            )
        return ProtocolContext(router_id=self._cfg.router_id, now=now, links=links)

    def _build_protocol(self, config: DaemonConfig) -> ProtocolEngine:
        protocol = config.protocol.lower()
        params = dict(config.protocol_params)
        if protocol == "ospf":
            return OspfProtocol(
                OspfTimers(
                    hello_interval=float(params.get("hello_interval", 1.0)),
                    lsa_interval=float(params.get("lsa_interval", 3.0)),
                    lsa_max_age=float(
                        params.get("lsa_max_age", max(10.0, config.dead_interval * 3.0))
                    ),
                )
            )
        if protocol == "rip":
            return RipProtocol(
                timers=RipTimers(
                    update_interval=float(params.get("update_interval", 5.0)),
                    neighbor_timeout=float(
                        params.get("neighbor_timeout", max(15.0, config.dead_interval))
                    ),
                ),
                infinity_metric=float(params.get("infinity_metric", 16.0)),
                poison_reverse=bool(params.get("poison_reverse", True)),
            )
        raise ValueError(f"Unsupported protocol: {config.protocol}")

    def _install_signal_handlers(self) -> None:
        def _handle_signal(signum, _frame) -> None:  # type: ignore[no-untyped-def]
            self._log.info("received signal %s, stopping", signum)
            self.stop()

        signal.signal(signal.SIGINT, _handle_signal)
        signal.signal(signal.SIGTERM, _handle_signal)
