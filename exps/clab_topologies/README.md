# Containerlab Classic Topologies

These topologies are hand-written `containerlab` files and can be used directly
without running the topology generator.

## Usage

```bash
sudo containerlab deploy -t exps/clab_topologies/ring6.clab.yaml --reconfigure
sudo containerlab destroy -t exps/clab_topologies/ring6.clab.yaml --cleanup
```

Routerd auto-start variant:

```bash
make build-routerd-node-image
sudo containerlab deploy -t exps/clab_topologies/ring6-routerd.clab.yaml --reconfigure
sudo containerlab destroy -t exps/clab_topologies/ring6-routerd.clab.yaml --cleanup
```

Note: `*-routerd.clab.yaml` bind mounts are relative to the topology file location:
- `../../src -> /irp/src`
- `./routerd_configs/<topology> -> /irp/configs`

All files use:
- `kind: linux`
- base image: `ghcr.io/srl-labs/network-multitool:latest`
- pre-configured data-plane interface IPs
- `net.ipv4.ip_forward=1`

## Included Topologies

- `line5.clab.yaml`: 5-node line
- `ring6.clab.yaml`: 6-node ring
- `star6.clab.yaml`: 1 hub + 5 spokes
- `fullmesh4.clab.yaml`: 4-node full mesh
- `spineleaf2x4.clab.yaml`: 2 spines + 4 leaves

## Routerd Auto-Start Variants

- `line5-routerd.clab.yaml`
- `ring6-routerd.clab.yaml`
- `star6-routerd.clab.yaml`
- `fullmesh4-routerd.clab.yaml`
- `spineleaf2x4-routerd.clab.yaml`

Routerd variants use image: `romam/network-multitool-routerd:latest`
(built locally from `exps/container_images/routerd-multitool/Dockerfile`).

Per-node configs are under:

- `exps/clab_topologies/routerd_configs/line5/`
- `exps/clab_topologies/routerd_configs/ring6/`
- `exps/clab_topologies/routerd_configs/star6/`
- `exps/clab_topologies/routerd_configs/fullmesh4/`
- `exps/clab_topologies/routerd_configs/spineleaf2x4/`

Each node auto-runs:

```bash
PYTHONPATH=/irp/src python3 -m irp.routerd --config /irp/configs/<node>.yaml
```

Before starting `routerd`, startup command runs `/irp/src/irp/runtime/bootstrap_routerd.sh`.
With the prebuilt image, `pyyaml` is already present; script keeps a fallback install path
for portability, then starts `routerd`.

```bash
/irp/src/irp/runtime/bootstrap_routerd.sh /irp/configs/<node>.yaml INFO
```

If dependency bootstrap fails, check:

```bash
docker exec <node> tail -n 100 /tmp/routerd-bootstrap.log
```
