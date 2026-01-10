#!/usr/bin/env python3

import os
import sys

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
sys.path.insert(0, REPO_ROOT)

from romam_lab.topology.spec import (  # noqa: F401
    compute_router_id,
    link_subnet_allocator,
    load_spec,
    parse_multicast,
    validate_and_normalize,
)
