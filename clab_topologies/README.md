# Containerlab Classic Topologies

These topologies are hand-written `containerlab` files and can be used directly
without running the topology generator.

## Usage

```bash
sudo containerlab deploy -t clab_topologies/ring6.clab.yaml --reconfigure
sudo containerlab destroy -t clab_topologies/ring6.clab.yaml --cleanup
```

Routerd variant is now generated from one profile + protocol switch:

```bash
make build-routerd-node-image
make gen-classic-routerd-lab CLASSIC_PROFILE=ring6 CLASSIC_PROTOCOL=ospf
sudo containerlab deploy -t clab_topologies/generated/ring6-routerd-ospf/ring6-routerd-ospf.clab.yaml --reconfigure
sudo containerlab destroy -t clab_topologies/generated/ring6-routerd-ospf/ring6-routerd-ospf.clab.yaml --cleanup
```

Supported routerd profiles:
- `line5`
- `ring6`
- `star6`
- `fullmesh4`
- `spineleaf2x4`

Supported protocols:
- `ospf`
- `rip`

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

Generated routerd labs use image: `romam/network-multitool-routerd:latest`
(built locally from `exps/container_images/routerd-multitool/Dockerfile`) and each node auto-runs:

```bash
PYTHONPATH=/irp/src python3 -m irp.routerd --config /irp/configs/<node>.yaml
```

Generated files are written to:
- `clab_topologies/generated/<lab-name>/<lab-name>.clab.yaml`
- `clab_topologies/generated/<lab-name>/configs/r*.yaml`
