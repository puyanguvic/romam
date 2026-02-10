from __future__ import annotations

import json
from pathlib import Path
from typing import Any


class JsonlLogger:
    def __init__(self, path: str | Path | None = None) -> None:
        self._path = Path(path) if path else None
        self._fh = None
        if self._path:
            self._path.parent.mkdir(parents=True, exist_ok=True)
            self._fh = self._path.open("w", encoding="utf-8")

    def log(self, event: str, **kwargs: Any) -> None:
        if not self._fh:
            return
        row = {"event": event, **kwargs}
        self._fh.write(json.dumps(row, sort_keys=True) + "\n")
        self._fh.flush()

    def close(self) -> None:
        if self._fh:
            self._fh.close()
            self._fh = None
