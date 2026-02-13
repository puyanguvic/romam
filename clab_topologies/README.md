# Containerlab Topologies

These `clab_topologies/*.clab.yaml` files are the single source of truth and are deployed directly by containerlab.

## Direct Usage

```bash
sudo containerlab deploy -t clab_topologies/ring6.clab.yaml --name ring6-demo --reconfigure
sudo containerlab destroy -t clab_topologies/ring6.clab.yaml --name ring6-demo --cleanup
```

## Supported Variable Overrides

Topology files support environment-variable overrides (containerlab native expansion):

- `CLAB_LAB_NAME`
- `CLAB_NODE_IMAGE`
- `CLAB_MGMT_NETWORK`
- `CLAB_MGMT_IPV4_SUBNET`
- `CLAB_MGMT_IPV6_SUBNET`
- `CLAB_MGMT_EXTERNAL_ACCESS`

Example:

```bash
CLAB_NODE_IMAGE=ghcr.io/srl-labs/network-multitool:latest \
CLAB_MGMT_NETWORK=clab-mgmt-custom \
sudo env CLAB_NODE_IMAGE="$CLAB_NODE_IMAGE" CLAB_MGMT_NETWORK="$CLAB_MGMT_NETWORK" \
  containerlab deploy -t clab_topologies/ring6.clab.yaml --name ring6-custom --reconfigure
```

## Included Topologies

- `line5.clab.yaml`: 5-node line
- `ring6.clab.yaml`: 6-node ring
- `star6.clab.yaml`: 1 hub + 5 spokes
- `fullmesh4.clab.yaml`: 4-node full mesh
- `spineleaf2x4.clab.yaml`: 2 spines + 4 leaves
