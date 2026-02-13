FROM ghcr.io/srl-labs/network-multitool:latest

# Pre-install runtime dependency for irp.routerd to avoid per-node install at lab startup.
RUN python3 -m ensurepip --upgrade \
    && python3 -m pip install --no-cache-dir pyyaml
