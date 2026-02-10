from __future__ import annotations

import random


def set_seed(seed: int) -> None:
    random.seed(int(seed))
