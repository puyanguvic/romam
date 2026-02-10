from __future__ import annotations

from rpf.core.convergence import hash_routes


def test_convergence_hash_stable_against_dict_order():
    a = {
        1: {1: [1], 3: [2], 2: [2]},
        2: {2: [2], 1: [1], 3: [3]},
    }
    b = {
        2: {3: [3], 1: [1], 2: [2]},
        1: {2: [2], 1: [1], 3: [2]},
    }
    assert hash_routes(a) == hash_routes(b)
