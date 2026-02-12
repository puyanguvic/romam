from __future__ import annotations

import logging
import subprocess
from typing import Dict, Iterable

from irp.model.routing import ForwardingEntry
from irp.runtime.config import ForwardingConfig


class ForwardingApplier:
    def apply(self, entries: Iterable[ForwardingEntry]) -> None:
        raise NotImplementedError


class NullForwardingApplier(ForwardingApplier):
    def apply(self, entries: Iterable[ForwardingEntry]) -> None:
        _ = list(entries)


class LinuxForwardingApplier(ForwardingApplier):
    def __init__(self, cfg: ForwardingConfig, logger: logging.Logger) -> None:
        self._cfg = cfg
        self._log = logger
        self._installed: Dict[int, ForwardingEntry] = {}

    def apply(self, entries: Iterable[ForwardingEntry]) -> None:
        desired: Dict[int, ForwardingEntry] = {entry.destination: entry for entry in entries}
        stale_destinations = sorted(set(self._installed.keys()) - set(desired.keys()))
        for destination in stale_destinations:
            stale = self._installed[destination]
            self._delete_route(stale)
            self._installed.pop(destination, None)

        for destination, entry in sorted(desired.items()):
            current = self._installed.get(destination)
            if current == entry:
                continue
            self._replace_route(entry)
            self._installed[destination] = entry

    def _replace_route(self, entry: ForwardingEntry) -> None:
        prefix = self._cfg.destination_prefixes.get(entry.destination)
        via = self._cfg.next_hop_ips.get(entry.next_hop)
        if prefix is None or via is None:
            self._log.warning(
                "Skip FIB install for dst=%s next_hop=%s: missing prefix/via mapping",
                entry.destination,
                entry.next_hop,
            )
            return
        cmd = ["ip", "route", "replace", prefix, "via", via, "table", str(self._cfg.table)]
        self._run_cmd(cmd)

    def _delete_route(self, entry: ForwardingEntry) -> None:
        prefix = self._cfg.destination_prefixes.get(entry.destination)
        if prefix is None:
            return
        cmd = ["ip", "route", "del", prefix, "table", str(self._cfg.table)]
        self._run_cmd(cmd, ignore_error=True)

    def _run_cmd(self, cmd: list[str], ignore_error: bool = False) -> None:
        if self._cfg.dry_run:
            self._log.info("FIB dry-run: %s", " ".join(cmd))
            return
        try:
            subprocess.run(cmd, check=True, capture_output=True, text=True)
        except subprocess.CalledProcessError as exc:
            if ignore_error:
                self._log.debug("FIB command ignored failure: %s -> %s", " ".join(cmd), exc.stderr)
                return
            raise RuntimeError(f"FIB command failed: {' '.join(cmd)}\n{exc.stderr}") from exc
