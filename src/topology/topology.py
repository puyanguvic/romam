from __future__ import annotations

import random
from dataclasses import dataclass
from typing import Dict, Iterable, List, Optional, Tuple

NodeId = int
Metric = float


@dataclass
class Edge:
    u: NodeId
    v: NodeId
    metric: Metric


class Topology:
    def __init__(self) -> None:
        self._adj: Dict[NodeId, Dict[NodeId, Metric]] = {}

    def add_node(self, node: NodeId) -> None:
        self._adj.setdefault(node, {})

    def nodes(self) -> List[NodeId]:
        return sorted(self._adj.keys())

    def neighbors(self, node: NodeId) -> Dict[NodeId, Metric]:
        return dict(self._adj.get(node, {}))

    def has_link(self, u: NodeId, v: NodeId) -> bool:
        return v in self._adj.get(u, {})

    def metric(self, u: NodeId, v: NodeId) -> Optional[Metric]:
        return self._adj.get(u, {}).get(v)

    def add_link(self, u: NodeId, v: NodeId, metric: Metric = 1.0) -> None:
        self.add_node(u)
        self.add_node(v)
        self._adj[u][v] = float(metric)
        self._adj[v][u] = float(metric)

    def remove_link(self, u: NodeId, v: NodeId) -> None:
        self._adj.get(u, {}).pop(v, None)
        self._adj.get(v, {}).pop(u, None)

    def update_metric(self, u: NodeId, v: NodeId, metric: Metric) -> None:
        if not self.has_link(u, v):
            self.add_link(u, v, metric)
            return
        self._adj[u][v] = float(metric)
        self._adj[v][u] = float(metric)

    def edge_list(self) -> List[Edge]:
        edges: List[Edge] = []
        seen: set[Tuple[int, int]] = set()
        for u in self.nodes():
            for v, m in self._adj[u].items():
                key = tuple(sorted((u, v)))
                if key in seen:
                    continue
                seen.add(key)
                edges.append(Edge(u=key[0], v=key[1], metric=m))
        return sorted(edges, key=lambda e: (e.u, e.v))

    def snapshot(self) -> Dict[NodeId, Dict[NodeId, Metric]]:
        return {n: dict(nei) for n, nei in self._adj.items()}

    def copy(self) -> "Topology":
        other = Topology()
        for e in self.edge_list():
            other.add_link(e.u, e.v, e.metric)
        return other

    @classmethod
    def from_edges(cls, edges: Iterable[Tuple[int, int, float]]) -> "Topology":
        t = cls()
        for u, v, m in edges:
            t.add_link(int(u), int(v), float(m))
        return t

    @classmethod
    def line(cls, n_nodes: int, metric: float = 1.0) -> "Topology":
        t = cls()
        if n_nodes <= 0:
            return t
        for i in range(n_nodes):
            t.add_node(i)
        for i in range(n_nodes - 1):
            t.add_link(i, i + 1, metric)
        return t

    @classmethod
    def ring(cls, n_nodes: int, metric: float = 1.0) -> "Topology":
        t = cls()
        if n_nodes <= 0:
            return t
        for i in range(n_nodes):
            t.add_link(i, (i + 1) % n_nodes, metric)
        return t

    @classmethod
    def star(cls, n_nodes: int, metric: float = 1.0, center: int = 0) -> "Topology":
        t = cls()
        if n_nodes <= 0:
            return t
        center = max(0, min(center, n_nodes - 1))
        for i in range(n_nodes):
            t.add_node(i)
        for i in range(n_nodes):
            if i == center:
                continue
            t.add_link(center, i, metric)
        return t

    @classmethod
    def fullmesh(cls, n_nodes: int, metric: float = 1.0) -> "Topology":
        t = cls()
        if n_nodes <= 0:
            return t
        for i in range(n_nodes):
            t.add_node(i)
        for u in range(n_nodes):
            for v in range(u + 1, n_nodes):
                t.add_link(u, v, metric)
        return t

    @classmethod
    def spineleaf(cls, n_spines: int, n_leaves: int, metric: float = 1.0) -> "Topology":
        t = cls()
        n_spines = max(0, n_spines)
        n_leaves = max(0, n_leaves)
        n_nodes = n_spines + n_leaves
        for i in range(n_nodes):
            t.add_node(i)
        for spine in range(n_spines):
            for leaf in range(n_spines, n_nodes):
                t.add_link(spine, leaf, metric)
        return t

    @classmethod
    def grid(cls, rows: int, cols: int, metric: float = 1.0) -> "Topology":
        t = cls()
        def idx(r: int, c: int) -> int:
            return r * cols + c
        for r in range(rows):
            for c in range(cols):
                u = idx(r, c)
                t.add_node(u)
                if c + 1 < cols:
                    t.add_link(u, idx(r, c + 1), metric)
                if r + 1 < rows:
                    t.add_link(u, idx(r + 1, c), metric)
        return t

    @classmethod
    def er(cls, n_nodes: int, p: float, metric: float = 1.0, seed: int = 0) -> "Topology":
        rng = random.Random(seed)
        t = cls()
        for n in range(n_nodes):
            t.add_node(n)
        for u in range(n_nodes):
            for v in range(u + 1, n_nodes):
                if rng.random() <= p:
                    t.add_link(u, v, metric)
        for u in range(1, n_nodes):
            if len(t.neighbors(u)) == 0:
                v = rng.randrange(0, u)
                t.add_link(u, v, metric)
        return t

    @classmethod
    def ba(cls, n_nodes: int, m: int, metric: float = 1.0, seed: int = 0) -> "Topology":
        rng = random.Random(seed)
        t = cls()
        if n_nodes <= 0:
            return t
        m = max(1, min(m, n_nodes - 1))
        for i in range(m + 1):
            for j in range(i + 1, m + 1):
                t.add_link(i, j, metric)
        for new_node in range(m + 1, n_nodes):
            t.add_node(new_node)
            degrees: List[int] = []
            nodes = t.nodes()
            for n in nodes:
                degrees.extend([n] * max(1, len(t.neighbors(n))))
            chosen: set[int] = set()
            while len(chosen) < m:
                chosen.add(rng.choice(degrees))
            for v in chosen:
                t.add_link(new_node, v, metric)
        return t

    @classmethod
    def from_config(cls, cfg: Dict, seed: int = 0) -> "Topology":
        tp = cfg.get("type", "ring")
        metric = float(cfg.get("default_metric", 1.0))
        if tp == "line":
            return cls.line(int(cfg.get("n_nodes", 8)), metric)
        if tp == "ring":
            return cls.ring(int(cfg.get("n_nodes", 8)), metric)
        if tp == "star":
            return cls.star(int(cfg.get("n_nodes", 8)), metric, int(cfg.get("center", 0)))
        if tp == "fullmesh":
            return cls.fullmesh(int(cfg.get("n_nodes", 8)), metric)
        if tp in {"spineleaf", "spineleaf2x4"}:
            return cls.spineleaf(
                int(cfg.get("n_spines", 2)),
                int(cfg.get("n_leaves", 4)),
                metric,
            )
        if tp == "grid":
            return cls.grid(int(cfg.get("rows", 4)), int(cfg.get("cols", 4)), metric)
        if tp == "er":
            return cls.er(int(cfg.get("n_nodes", 40)), float(cfg.get("p", 0.05)), metric, seed=seed)
        if tp == "ba":
            return cls.ba(int(cfg.get("n_nodes", 100)), int(cfg.get("m", 2)), metric, seed=seed)
        raise ValueError(f"Unsupported topology type: {tp}")
