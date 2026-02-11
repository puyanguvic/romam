from __future__ import annotations

import select
import socket
from typing import Optional, Tuple


class UdpTransport:
    def __init__(self, bind_address: str, bind_port: int, recv_buf_size: int = 65535) -> None:
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._sock.bind((bind_address, bind_port))
        self._recv_buf_size = recv_buf_size

    def recv(self, timeout_s: float) -> Optional[Tuple[bytes, tuple[str, int]]]:
        readable, _, _ = select.select([self._sock], [], [], max(0.0, timeout_s))
        if not readable:
            return None
        data, addr = self._sock.recvfrom(self._recv_buf_size)
        return data, (str(addr[0]), int(addr[1]))

    def send(self, payload: bytes, address: str, port: int) -> None:
        self._sock.sendto(payload, (address, int(port)))

    def close(self) -> None:
        self._sock.close()
