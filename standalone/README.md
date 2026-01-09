# ROMAM Standalone (Linux/Mininet/Docker)

这是一个**脱离 ns-3** 的最小原型：每个路由器/namespace 运行一个 `romamd`，通过 UDP 多播交换 LSA，基于 Dijkstra 计算下一跳，并用 rtnetlink 写入 Linux 路由表。

## 构建

```bash
cmake -S standalone -B build-standalone
cmake --build build-standalone -j
```

产物：`build-standalone/daemon/romamd`

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

## Mininet 实验（推荐）

Mininet 使用统一的“实验规范（Spec）”文件来定义节点、链路、链路 cost 与 `tc` 参数（delay/loss/bw）。

Spec 支持 `TOML`（推荐）或 `JSON`，字段语义见 `standalone/mininet/spec.schema.json`。

一键起拓扑/起守护进程/跑一个简单的 ping 自检：

```bash
sudo python3 standalone/mininet/run.py --spec standalone/mininet/specs/linear5.toml
```

Abilene 示例：

```bash
sudo python3 standalone/mininet/run.py --spec standalone/mininet/specs/abilene.toml
```

## 协议与限制（当前原型）

- 邻居发现：同一二层/同一广播域内通过多播 HELLO 发现。
- 链路代价：通过 `iface_cost=<ifname>:<cost>` 配置并随 LSA 泛洪（Mininet 脚本会按 Spec 自动生成）。
- LSA：周期性刷新；未实现 LSA aging/认证/分片等。
- 数据面优先级（DGR/DDR/队列）：尚未接入（真机一般用 DSCP + `tc`/eBPF 实现）。
