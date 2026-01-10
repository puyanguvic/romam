# ROMAM + containerlab

你希望用 containerlab 负责网络拓扑（不使用 Mininet）。推荐流程是：

1) 用 `standalone/containerlab/gen.py` 把 Spec 转成 containerlab topo + 每个节点的 `romamd.conf`/启动脚本  
2) 用 containerlab `deploy/destroy` 起拓扑  
3) 每个容器启动时自动配置接口地址、route table rule，并启动 `romamd`

> Spec 里的 `tc`（`delay/loss/bw`）也会在容器内通过 `tc netem` 应用到对应接口。

## 0. 准备

- 安装 containerlab（本机）
- Docker 可用

## 1. 构建镜像

使用专门给 containerlab 的镜像（默认入口是 `/etc/romam/start.sh`）：

```bash
docker build -f standalone/docker/Dockerfile.clab -t romam:clab .
```

该镜像额外内置了一个轻量的流量生成器：`romam-traffic`（UDP/TCP client/server）。

## 2. 生成 topo + 节点配置

复用现有 Spec（同 `standalone/mininet/specs/*.toml`）：

```bash
python3 standalone/containerlab/gen.py \
  --spec standalone/mininet/specs/linear5.toml \
  --out /tmp/romam-clab-linear5 \
  --image romam:clab
```

会生成：

- `/tmp/romam-clab-linear5/topo.clab.yml`
- `/tmp/romam-clab-linear5/configs/<node>.conf`
- `/tmp/romam-clab-linear5/configs/<node>.start.sh`

## 3. 启动/销毁拓扑

```bash
containerlab deploy -t /tmp/romam-clab-linear5/topo.clab.yml
```

查看：

```bash
containerlab inspect -t /tmp/romam-clab-linear5/topo.clab.yml
```

进入某个节点（示例）：

```bash
docker exec -it clab-romam-linear5-r1 bash
```

销毁：

```bash
containerlab destroy -t /tmp/romam-clab-linear5/topo.clab.yml
```

## 4. 常见问题

- 需要 `NET_ADMIN`：topo 已默认加了 `cap-add: NET_ADMIN`，用于写路由表与 `ip rule`。
- 接口命名：脚本假设 containerlab 的 mgmt 为 `eth0`，链路接口从 `eth1` 开始。

## 5. 跑一条流量（示例）

```bash
containerlab exec -t /tmp/romam-clab-linear5/topo.clab.yml --node r1 --cmd "romam-traffic --mode server --proto udp --bind 0.0.0.0:5001 --duration 10"
containerlab exec -t /tmp/romam-clab-linear5/topo.clab.yml --node r5 --cmd "romam-traffic --mode client --proto udp --connect 10.255.0.5:5001 --duration 10 --rate-mbps 10 --size 1200"
```

更推荐用主机侧脚本编排：`lab/02_traffic_flow.py`。
