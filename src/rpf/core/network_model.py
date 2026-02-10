from __future__ import annotations

import random
from collections import defaultdict
from typing import DefaultDict, List

from rpf.core.types import Message


class NetworkModel:
    def __init__(
        self,
        base_delay: int = 1,
        jitter: int = 0,
        loss_prob: float = 0.0,
        seed: int = 0,
    ) -> None:
        self.base_delay = max(0, int(base_delay))
        self.jitter = max(0, int(jitter))
        self.loss_prob = max(0.0, min(1.0, float(loss_prob)))
        self.rng = random.Random(seed)
        self._inflight: DefaultDict[int, List[Message]] = defaultdict(list)
        self.delivered_messages = 0
        self.dropped_messages = 0

    def send(self, msg: Message, now_tick: int) -> None:
        if self.rng.random() < self.loss_prob:
            self.dropped_messages += 1
            return
        extra = self.rng.randint(0, self.jitter) if self.jitter > 0 else 0
        due_tick = now_tick + self.base_delay + extra
        self._inflight[due_tick].append(msg)

    def deliver(self, tick: int) -> List[Message]:
        msgs = self._inflight.pop(tick, [])
        msgs.sort(key=lambda m: m.sort_key())
        self.delivered_messages += len(msgs)
        return msgs
