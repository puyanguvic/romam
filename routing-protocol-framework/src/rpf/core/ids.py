from __future__ import annotations


class IdMap:
    """Compact string<->int mapping for topology/protocol IDs."""

    def __init__(self) -> None:
        self._s2i: dict[str, int] = {}
        self._i2s: dict[int, str] = {}

    def encode(self, value: str) -> int:
        if value in self._s2i:
            return self._s2i[value]
        idx = len(self._s2i)
        self._s2i[value] = idx
        self._i2s[idx] = value
        return idx

    def decode(self, idx: int) -> str:
        return self._i2s[idx]

    def __len__(self) -> int:
        return len(self._s2i)
