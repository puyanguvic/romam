from __future__ import annotations

from collections import defaultdict
from typing import Dict, Tuple


class StateStore:
    """State container for link metrics and abstract traffic signals."""

    def __init__(self) -> None:
        self.link_metrics: Dict[Tuple[int, int], float] = {}
        self.traffic_signals: Dict[Tuple[int, int], dict] = defaultdict(dict)

    @staticmethod
    def _key(u: int, v: int) -> Tuple[int, int]:
        return tuple(sorted((u, v)))

    def set_link_metric(self, u: int, v: int, metric: float) -> None:
        self.link_metrics[self._key(u, v)] = float(metric)

    def get_link_metric(self, u: int, v: int) -> float | None:
        return self.link_metrics.get(self._key(u, v))

    def set_traffic_signal(self, u: int, v: int, signal: dict) -> None:
        self.traffic_signals[self._key(u, v)] = dict(signal)

    def get_traffic_signal(self, u: int, v: int) -> dict:
        return dict(self.traffic_signals.get(self._key(u, v), {}))
