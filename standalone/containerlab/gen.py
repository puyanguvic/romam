#!/usr/bin/env python3

import os
import sys

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
sys.path.insert(0, REPO_ROOT)

from romam_lab.topology.containerlab import main  # noqa: E402


if __name__ == "__main__":
    raise SystemExit(main())
