# clab Workspace

Central place for containerlab resources.
Automation scripts live under the repository-root `tools/` directory:

- `topologies/*.clab.yaml`: topology files
- `export-templates/auto.tmpl`: `topology-data.json` template
- `topology-data.json`: default experiment input
- `tools/deploy.py`: deploy topology (reads `topology-data.json`)
- `tools/run_apps.py`: start multiple apps on nodes (`clab exec`)
- `tools/collect.py`: collect routingd APIs + `clab inspect` output
- `tools/generate_routerd_lab.py`: generate per-node routingd configs
- `tools/run_routerd_lab.py`: deploy/bootstrap/check/destroy lifecycle
- `tools/check_routerd_lab.py`: runtime health checks
- `tools/run_unified_experiment.py`: YAML-driven scenario/benchmark runner
- `tools/run_traffic_app.py`: run one traffic_app command
- `tools/run_traffic_plan.py`: run ordered traffic tasks from YAML
- `tools/run_traffic_probe.py`: one-shot sender/sink probe
- `tools/install_traffic_app_bin.py`: copy traffic_app into nodes
- `tools/ospf_convergence_exp.py`: OSPF benchmark wrapper

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
python tools/deploy.py --topology-data src/clab/topology-data.json
```

```bash
python tools/run_apps.py --topology-data src/clab/topology-data.json --plan <apps.yaml>
```

```bash
python tools/collect.py --topology-data src/clab/topology-data.json
```
