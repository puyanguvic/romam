# ROMAM Lab Scripts（可编辑示例）

这些脚本用于把工程按“拓扑 / 流量 / 观测”拆开，让你用 Python 快速拼装实验流程（主机侧执行，节点侧不需要 Python）。

## 1) 生成 containerlab 拓扑

```bash
python3 lab/01_topology_containerlab.py \
  --spec standalone/mininet/specs/linear5.toml \
  --out /tmp/romam-clab-linear5 \
  --image romam:clab
```

## 2) 跑一条 UDP/TCP 流

```bash
python3 lab/02_traffic_flow.py \
  --topo /tmp/romam-clab-linear5/topo.clab.yml \
  --spec standalone/mininet/specs/linear5.toml \
  --proto udp --server r1 --client r5 --duration 10 --rate-mbps 10
```

## 2.5) 一键 demo（Linux netns，无需 containerlab）

会自动：创建拓扑（netns+veth+tc）→ 启动 `romamd` → 跑两条流（UDP + TCP）→ 自动清理。

```bash
sudo python3 lab/04_demo_linuxns_two_flows.py --spec standalone/mininet/specs/linear5.toml
```

## 3) 采集路由表（JSON）

```bash
python3 lab/03_observe_routes.py \
  --topo /tmp/romam-clab-linear5/topo.clab.yml \
  --spec standalone/mininet/specs/linear5.toml \
  --out /tmp/romam-clab-linear5/results \
  --table 100
```
