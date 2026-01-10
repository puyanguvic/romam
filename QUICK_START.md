# Quick Start Guide (Standalone)

本 Quick Start 面向 **不依赖 ns-3** 的 Standalone 版本：可在 Linux / Mininet / Docker 上运行。

## Prerequisites

- Linux（Ubuntu 20.04+ 建议）
- CMake + C++17 编译器
- `iproute2`（提供 `ip`/`tc`）
- （可选）Mininet：用于更方便的拓扑/链路 emulation

## Build

```bash
cmake -S standalone -B build-standalone
cmake --build build-standalone -j
```

产物：`build-standalone/daemon/romamd`

## Run (Single Router)

```bash
sudo ./build-standalone/daemon/romamd --config standalone/examples/romamd.conf
```

## Run (containerlab)

```bash
docker build -f standalone/docker/Dockerfile.clab -t romam:clab .
python3 standalone/containerlab/gen.py --spec standalone/mininet/specs/linear5.toml --out /tmp/romam-clab-linear5
containerlab deploy -t /tmp/romam-clab-linear5/topo.clab.yml
```

## Run (Pure Linux netns, no Mininet)

```bash
sudo python3 standalone/linuxns/run.py --spec standalone/mininet/specs/linear5.toml
```

更多说明见 `standalone/README.md` 和 `MANUAL.zh-CN.md`。
