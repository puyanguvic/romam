# ROMAM Standalone (Linux/Mininet/Docker)

这是一个**脱离 ns-3** 的最小原型：每个路由器/namespace 运行一个 `romamd`，通过 UDP 多播交换 LSA，基于 Dijkstra 计算下一跳，并用 rtnetlink 写入 Linux 路由表。

## 构建

```bash
cmake -S standalone -B build-standalone
cmake --build build-standalone -j
```

产物：`build-standalone/daemon/romamd`
流量生成器：`build-standalone/traffic/romam-traffic`

## 运行

1) 准备配置文件（示例见 `standalone/examples/romamd.conf`）

2) 以 root 或具备 `CAP_NET_ADMIN` 权限运行：

```bash
sudo ./build-standalone/daemon/romamd --config standalone/examples/romamd.conf
```

默认会写入 `route_table`（配置项，默认 100）。如果你要让系统优先查这张表，请添加规则：

```bash
sudo ip -4 rule add pref 1000 lookup 100
```

不改系统路由表（只打印预期下发的路由）：

```bash
./build-standalone/daemon/romamd --config standalone/examples/romamd.conf --dry-run
```

### 可扩展路由算法

配置文件支持 `routing_algo=<name>`（默认 `spf`）。当你新增路由算法实现时，只需要：

- 在 `standalone/protocol/core` 增加实现
- 在 `romam::ComputeRoutes()` 注册一个名字
- 在配置文件或 Spec 的 `romam.routing_algo` 里选择

## containerlab 实验（推荐）

如果你希望用“容器/网络拓扑编排”来跑实验，推荐使用 containerlab：

- 生成 topo + 节点配置：`standalone/containerlab/gen.py`
- 说明：`standalone/containerlab/README.md`

示例（linear5）：

```bash
docker build -f standalone/docker/Dockerfile.clab -t romam:clab .
python3 standalone/containerlab/gen.py --spec standalone/mininet/specs/linear5.toml --out /tmp/romam-clab-linear5
containerlab deploy -t /tmp/romam-clab-linear5/topo.clab.yml
```

## Lab 脚本（拓扑/流量/观测）

`lab/*` 提供可直接编辑的 Python 脚本模板，用于主机侧编排实验（生成拓扑、跑流量、采集路由表等），见 `lab/README.md`。

## Mininet 实验（可选）

Mininet runner 仍然可用（如果你不使用 containerlab），说明见 `standalone/mininet/README.md`。

## 纯 Linux netns 实验（不依赖 Mininet）

如果你不想装 Mininet，可以用 `ip netns + veth + tc` 直接在本机创建实验拓扑：

```bash
sudo python3 standalone/linuxns/run.py --spec standalone/mininet/specs/linear5.toml
```

更多说明见 `standalone/linuxns/README.md`。

## Docker

仓库提供了一个只打包 `romamd` 的 Dockerfile：`standalone/docker/Dockerfile`。

构建：

```bash
docker build -f standalone/docker/Dockerfile -t romam:standalone .
```

运行（单个路由器示例，需写路由表所以需要 `NET_ADMIN`）：

```bash
docker run --rm -it --cap-add NET_ADMIN --network host \
  -v "$PWD/standalone/examples/romamd.conf:/romamd.conf:ro" \
  romam:standalone --config /romamd.conf
```

> 多路由器/拓扑实验更建议用 `standalone/mininet/run.py` 或 `standalone/linuxns/run.py` 在宿主机直接起多个 namespace。

## 协议与限制（当前原型）

- 邻居发现：同一二层/同一广播域内通过多播 HELLO 发现。
- 链路代价：通过 `iface_cost=<ifname>:<cost>` 配置并随 LSA 泛洪（Mininet 脚本会按 Spec 自动生成）。
- LSA：周期性刷新；未实现 LSA aging/认证/分片等。
- 数据面优先级（DGR/DDR/队列）：尚未接入（真机一般用 DSCP + `tc`/eBPF 实现）。
