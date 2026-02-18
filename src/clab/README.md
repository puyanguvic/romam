# clab Workspace

Central place for containerlab resources and scripts:

- `topologies/*.clab.yaml`: topology files
- `export-templates/auto.tmpl`: `topology-data.json` template
- `topology-data.json`: default experiment input
- `scripts/deploy.py`: deploy topology (reads `topology-data.json`)
- `scripts/run_apps.py`: start multiple apps on nodes (`clab exec`)
- `scripts/collect.py`: collect routingd APIs + `clab inspect` output
- `scripts/generate_routerd_lab.py`: generate per-node routingd configs
- `scripts/run_routerd_lab.py`: deploy/bootstrap/check/destroy lifecycle
- `scripts/check_routerd_lab.py`: runtime health checks
- `scripts/run_unified_experiment.py`: YAML-driven scenario/benchmark runner
- `scripts/run_traffic_app.py`: run one traffic_app command
- `scripts/run_traffic_plan.py`: run ordered traffic tasks from YAML
- `scripts/run_traffic_probe.py`: one-shot sender/sink probe
- `scripts/install_traffic_app_bin.py`: copy traffic_app into nodes
- `scripts/ospf_convergence_exp.py`: OSPF benchmark wrapper

## Minimal topology-data.json

```json
{
  "topology_file": "src/clab/topologies/ring6.clab.yaml",
  "lab_name": "ring6-demo",
  "sudo": false,
  "reconfigure": true,
  "env": {},
  "management_http_ports": {}
}
```

Optional fields:

- `config_dir`: per-node config directory (`collect.py` auto-loads management ports)
- `nodes`: explicit node list (otherwise parsed from topology)

## Usage

```bash
python src/clab/scripts/deploy.py --topology-data src/clab/topology-data.json
```

```bash
python src/clab/scripts/run_apps.py --topology-data src/clab/topology-data.json --plan <apps.yaml>
```

```bash
python src/clab/scripts/collect.py --topology-data src/clab/topology-data.json
```
