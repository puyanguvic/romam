# ROMAM 运行手册（不依赖 ns-3）

你当前的目标是 **完全放弃 ns-3 / Mininet**，因此本手册只覆盖 Standalone 方案：

- Linux 真机/虚拟机（单进程或多 namespace）
- containerlab（推荐，负责拓扑）
- Docker（打包 `romamd`，或在特权容器内跑 netns/mininet）

---

## 1. 编译

```bash
cmake -S standalone -B build-standalone
cmake --build build-standalone -j
```

产物：`build-standalone/daemon/romamd`

## 2. 单机运行（单个路由器进程）

准备配置文件（示例见 `standalone/examples/romamd.conf`），然后以 root 或具备 `CAP_NET_ADMIN` 权限运行：

```bash
sudo ./build-standalone/daemon/romamd --config standalone/examples/romamd.conf
```

不改系统路由表（只打印预期下发的路由）：

```bash
./build-standalone/daemon/romamd --config standalone/examples/romamd.conf --dry-run
```

## 3. containerlab 实验（推荐）

containerlab 负责拓扑，ROMAM 只负责在每个节点里跑 `romamd`。流程是：

1) 构建 containerlab 镜像（入口默认执行 `/etc/romam/start.sh`）：`standalone/docker/Dockerfile.clab`  
2) 用 Spec 生成 topo + 每节点配置：`standalone/containerlab/gen.py`  
3) `containerlab deploy/destroy`

示例（linear5）：

```bash
docker build -f standalone/docker/Dockerfile.clab -t romam:clab .
python3 standalone/containerlab/gen.py --spec standalone/mininet/specs/linear5.toml --out /tmp/romam-clab-linear5
containerlab deploy -t /tmp/romam-clab-linear5/topo.clab.yml
```

说明文档：`standalone/containerlab/README.md`

## 4. 纯 Linux netns（不依赖 containerlab）

如果你不想安装 Mininet，可直接用 netns/veth/tc 起拓扑：

```bash
sudo python3 standalone/linuxns/run.py --spec standalone/mininet/specs/abilene.toml --show-routes
```

更多说明见 `standalone/linuxns/README.md`。

## 5. Docker

Standalone 的详细说明见 `standalone/README.md`；这里给最短路径：

构建 `romamd` 镜像：

```bash
docker build -f standalone/docker/Dockerfile -t romam:standalone .
```

运行（单路由器示例）：

```bash
docker run --rm -it --cap-add NET_ADMIN --network host \
  -v "$PWD/standalone/examples/romamd.conf:/romamd.conf:ro" \
  romam:standalone --config /romamd.conf
```

---

## 6. 常见问题排查

- 需要 root：Mininet / netns / 写路由表都需要 root 或 `CAP_NET_ADMIN`。
- TOML 解析：Python3.11+ 原生支持；老版本请 `pip install tomli` 或改用 `.json` spec。
- 只想看路由，不改系统：运行时加 `--dry-run`。
