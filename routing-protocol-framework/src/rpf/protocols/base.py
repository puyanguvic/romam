from __future__ import annotations

from typing import Any


class RoutingProtocol:
    def __init__(self, node_id: int, config: dict | None = None) -> None:
        self.node_id = node_id
        self.config = config or {}

    def on_start(self, ctx: Any) -> None:
        pass

    def on_tick(self, ctx: Any) -> None:
        pass

    def on_message(self, ctx: Any, msg: Any) -> None:
        pass

    def on_link_change(self, ctx: Any, neighbor: int, new_metric: float | None) -> None:
        pass
