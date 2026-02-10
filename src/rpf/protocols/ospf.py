from __future__ import annotations

from dataclasses import dataclass
from typing import Dict, Mapping

from rpf.protocols.base import RoutingProtocol


@dataclass(frozen=True)
class LsaRecord:
    origin: int
    seq: int
    neighbors: Dict[int, float]
    arrived_tick: int


class OspfProtocol(RoutingProtocol):
    """OSPF style link-state protocol with LSA flooding and periodic SPF."""

    MSG_HELLO = "OSPF_HELLO"
    MSG_LSA = "OSPF_LSA"

    def __init__(self, node_id: int, config: dict | None = None) -> None:
        super().__init__(node_id, config)
        self.hello_interval = max(1, int(self.config.get("hello_interval", 5)))
        self.dead_interval = max(self.hello_interval, int(self.config.get("dead_interval", self.hello_interval * 4)))
        self.spf_interval = max(0, int(self.config.get("spf_interval", 3)))
        self.lsa_refresh = max(1, int(self.config.get("lsa_refresh", 20)))
        self.lsa_jitter = max(0, int(self.config.get("lsa_jitter", self.config.get("jitter", 0))))
        self.lsa_max_age = max(
            self.lsa_refresh,
            int(self.config.get("lsa_max_age", self.lsa_refresh * 3)),
        )
        self.max_paths = max(1, int(self.config.get("max_paths", 4)))

        self.state_set("lsa_seq", 0)
        self.state_set("lsdb", {})
        self.state_set("neighbor_last_seen", {})
        self.state_set("next_hello_tick", 0)
        self.state_set("next_lsa_tick", 0)
        self.state_set("next_spf_tick", 0)
        self.register_control_handler(self.MSG_HELLO, self._on_hello_message)
        self.register_control_handler(self.MSG_LSA, self._on_lsa_message)

    def on_start(self, ctx) -> None:
        self._send_hello(ctx)
        self._originate_lsa(ctx)
        self._run_spf(ctx)

    def on_tick(self, ctx) -> None:
        if int(ctx.tick) >= int(self.state_get("next_hello_tick", 0)):
            self._send_hello(ctx)

        neighbor_changed = self._expire_dead_neighbors(ctx)
        lsdb_changed = self._expire_lsdb(ctx)
        if neighbor_changed:
            self._originate_lsa(ctx)
            self._schedule_spf(ctx)
        elif lsdb_changed:
            self._schedule_spf(ctx)

        if int(ctx.tick) >= int(self.state_get("next_lsa_tick", 0)):
            self._originate_lsa(ctx)

        next_spf_tick = self.state_get("next_spf_tick", None)
        if next_spf_tick is not None and int(ctx.tick) >= int(next_spf_tick):
            self._run_spf(ctx)
            self.state_set("next_spf_tick", None)

    def on_message(self, ctx, msg) -> None:
        self.dispatch_control_message(ctx, msg)

    def on_link_change(self, ctx, neighbor: int, new_metric: float | None) -> None:
        _ = (neighbor, new_metric)
        self._prune_neighbor_state(ctx)
        self._originate_lsa(ctx)
        self._schedule_spf(ctx)

    def _on_hello_message(self, ctx, msg) -> None:
        src = int(msg.src)
        if src == self.node_id:
            return
        if ctx.get_link_metric(src) is None:
            return

        neighbor_last_seen: Dict[int, int] = self.state_get("neighbor_last_seen", {})
        was_down = src not in neighbor_last_seen
        neighbor_last_seen[src] = int(ctx.tick)
        self.state_set("neighbor_last_seen", neighbor_last_seen)
        if was_down:
            self._originate_lsa(ctx)
            self._schedule_spf(ctx)

    def _on_lsa_message(self, ctx, msg) -> None:
        payload = msg.payload if isinstance(msg.payload, Mapping) else {}
        if not payload:
            return

        origin = int(payload.get("origin", -1))
        seq = int(payload.get("seq", -1))
        raw_neighbors = payload.get("neighbors", {})
        if origin < 0 or seq < 0 or not isinstance(raw_neighbors, Mapping):
            return

        neighbors = {int(n): float(m) for n, m in raw_neighbors.items() if float(m) >= 0}
        if not self._install_lsa(
            origin=origin,
            seq=seq,
            neighbors=neighbors,
            arrived_tick=int(ctx.tick),
        ):
            return

        flood_payload = {
            "origin": origin,
            "seq": seq,
            "neighbors": {int(k): float(v) for k, v in neighbors.items()},
        }
        self.broadcast_control(ctx, self.MSG_LSA, flood_payload, exclude=int(msg.src))
        self._schedule_spf(ctx)

    def _send_hello(self, ctx) -> None:
        payload = {"router_id": int(self.node_id)}
        self.broadcast_control(ctx, self.MSG_HELLO, payload)
        self.state_set("next_hello_tick", int(ctx.tick) + self.hello_interval)

    def _install_lsa(self, origin: int, seq: int, neighbors: Dict[int, float], arrived_tick: int) -> bool:
        lsdb: Dict[int, LsaRecord] = self.state_get("lsdb", {})
        old = lsdb.get(origin)
        if old is not None and seq <= old.seq:
            return False
        lsdb[origin] = LsaRecord(
            origin=origin,
            seq=seq,
            neighbors=dict(neighbors),
            arrived_tick=arrived_tick,
        )
        self.state_set("lsdb", lsdb)
        return True

    def _originate_lsa(self, ctx) -> None:
        neighbors = {int(n): float(m) for n, m in ctx.get_neighbors().items() if float(m) >= 0}
        seq = int(self.state_get("lsa_seq", 0)) + 1
        self.state_set("lsa_seq", seq)
        self._install_lsa(
            origin=self.node_id,
            seq=seq,
            neighbors=neighbors,
            arrived_tick=int(ctx.tick),
        )

        payload = {
            "origin": self.node_id,
            "seq": seq,
            "neighbors": {int(k): float(v) for k, v in neighbors.items()},
        }
        self.broadcast_control(ctx, self.MSG_LSA, payload)
        self.state_set("next_lsa_tick", int(ctx.tick) + self.lsa_refresh + self._refresh_jitter())

    def _expire_dead_neighbors(self, ctx) -> bool:
        neighbor_last_seen: Dict[int, int] = self.state_get("neighbor_last_seen", {})
        active_neighbors = set(int(n) for n in ctx.get_neighbors())
        changed = False
        for nbr in list(neighbor_last_seen):
            dead_by_link = nbr not in active_neighbors
            dead_by_timer = int(ctx.tick) - int(neighbor_last_seen[nbr]) > self.dead_interval
            if dead_by_link or dead_by_timer:
                neighbor_last_seen.pop(nbr, None)
                changed = True
        if changed:
            self.state_set("neighbor_last_seen", neighbor_last_seen)
        return changed

    def _prune_neighbor_state(self, ctx) -> None:
        _ = self._expire_dead_neighbors(ctx)

    def _expire_lsdb(self, ctx) -> bool:
        lsdb: Dict[int, LsaRecord] = self.state_get("lsdb", {})
        changed = False
        for origin, rec in list(lsdb.items()):
            if origin == self.node_id:
                continue
            if int(ctx.tick) - int(rec.arrived_tick) > self.lsa_max_age:
                lsdb.pop(origin, None)
                changed = True
        if changed:
            self.state_set("lsdb", lsdb)
        return changed

    def _schedule_spf(self, ctx) -> None:
        target_tick = int(ctx.tick) + self.spf_interval
        current = self.state_get("next_spf_tick", None)
        if current is None or target_tick < int(current):
            self.state_set("next_spf_tick", target_tick)

    def _refresh_jitter(self) -> int:
        if self.lsa_jitter <= 0:
            return 0
        return int(self.node_id % (self.lsa_jitter + 1))

    def _build_graph(self, ctx) -> Dict[int, Dict[int, float]]:
        graph: Dict[int, Dict[int, float]] = {}
        lsdb: Dict[int, LsaRecord] = self.state_get("lsdb", {})

        for rec in lsdb.values():
            u = int(rec.origin)
            graph.setdefault(u, {})
            for v, metric in rec.neighbors.items():
                v = int(v)
                metric = float(metric)
                if metric < 0:
                    continue
                graph.setdefault(v, {})
                old_uv = graph[u].get(v)
                if old_uv is None or metric < old_uv:
                    graph[u][v] = metric
                old_vu = graph[v].get(u)
                if old_vu is None or metric < old_vu:
                    graph[v][u] = metric

        graph.setdefault(self.node_id, {})
        for neighbor, metric in ctx.get_neighbors().items():
            v = int(neighbor)
            w = float(metric)
            if w < 0:
                continue
            graph.setdefault(v, {})
            old_uv = graph[self.node_id].get(v)
            if old_uv is None or w < old_uv:
                graph[self.node_id][v] = w
            old_vu = graph[v].get(self.node_id)
            if old_vu is None or w < old_vu:
                graph[v][self.node_id] = w
        return graph

    def _run_spf(self, ctx) -> None:
        graph = self._build_graph(ctx)
        distances, predecessors = self.dijkstra_with_predecessors(graph, self.node_id)

        self.clear_routes(keep_self=True)
        for dst in sorted(distances):
            if dst == self.node_id:
                continue
            first_hops = self.compute_shortest_first_hops(
                start=self.node_id,
                dst=dst,
                predecessors=predecessors,
                max_paths=self.max_paths,
            )
            if not first_hops:
                continue
            self.upsert_route(
                dst=dst,
                metric=float(distances[dst]),
                candidate_next_hops=first_hops,
                source="ospf",
            )
        self.publish_routing_table(ctx)
