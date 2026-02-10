from __future__ import annotations

from abc import ABC, abstractmethod
from typing import Any, Dict


class Backend(ABC):
    @abstractmethod
    def run(self, config: Dict[str, Any]) -> Dict[str, Any]:
        raise NotImplementedError
