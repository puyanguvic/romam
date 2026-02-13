from __future__ import annotations

import pytest

from irp.apps.traffic_app import _build_parser, _payload, _validate_args


def test_payload_size_matches_request() -> None:
    payload = _payload(packet_size=128, seq=1)
    assert len(payload) == 128


def test_validate_send_args_ok() -> None:
    parser = _build_parser()
    args = parser.parse_args(
        [
            "send",
            "--proto",
            "udp",
            "--target",
            "10.0.0.2",
            "--port",
            "9000",
            "--count",
            "10",
        ]
    )
    _validate_args(args)


def test_validate_args_rejects_bad_port() -> None:
    parser = _build_parser()
    args = parser.parse_args(
        [
            "sink",
            "--proto",
            "udp",
            "--bind",
            "0.0.0.0",
            "--port",
            "70000",
        ]
    )
    with pytest.raises(ValueError):
        _validate_args(args)

