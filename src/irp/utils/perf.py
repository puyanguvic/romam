from __future__ import annotations

import time
from dataclasses import dataclass, field
from typing import Dict


@dataclass
class CounterSet:
    counters: Dict[str, int] = field(default_factory=dict)

    def inc(self, name: str, value: int = 1) -> None:
        self.counters[name] = self.counters.get(name, 0) + value


class Timer:
    def __init__(self) -> None:
        self._start = time.perf_counter()

    def elapsed(self) -> float:
        return time.perf_counter() - self._start
