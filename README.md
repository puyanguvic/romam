# ROMAM: Traffic-aware Routing Framework (Standalone-first)

## Overview

ROMAM (ROuting Module Architecture for MAchine learning) is an innovative intra-autonomous system (AS) routing architecture designed to accelerate the research and development of intelligent routing protocols. It offers a modular, highly adaptable framework that integrates both static and dynamic network information, enabling swift prototyping and assessment of advanced routing solutions.

## Key Features

- Modular Architecture for flexible routing protocol development
- Integration of static and dynamic network information
- Support for rapid prototyping and evaluation of routing protocols
- Comprehensive monitoring toolchain for pre-deployment evaluation
- Significant reduction in development efforts
- Native support for machine learning integration in routing decisions

## Core Components

1. Information Collection Module (ICM)
2. Route Discovery Module (RDM)
3. Traffic Detection Module (TDM)
4. Intelligent Forwarding Module (IFM)

## Implemented Protocols

- OSPF (Open Shortest Path First)
- K-Shortest Path Routing
- Octopus (MAB-based Intelligent Forwarding)
- DGR (Delay Guaranteed Routing)
- DDR (Deadline-Driven Routing)

## Quick Start

本仓库当前推荐使用 **Standalone 版本**（Linux / Mininet / Docker），不依赖 ns-3：

- `standalone/README.md`：构建、单机运行、Mininet、纯 Linux netns、Docker
- `MANUAL.zh-CN.md`：中文运行手册（Standalone）

## Module Layout (Refactored)

为了方便“用脚本组装实验”，代码按以下 4 个模块组织（入口保持兼容）：

1) **网络拓扑构建（containerlab-first）**
   - 入口：`standalone/containerlab/gen.py`
   - Host 侧库：`romam_lab/topology/*`

2) **网络协议块（重点，便于扩展路由算法）**
   - Standalone 协议实现：`standalone/protocol/*`（产物仍在 `build-standalone/daemon/romamd`）
   - 新增配置项：`routing_algo=...`（默认 `spf`）

3) **流量生成器（节点侧应用层）**
   - Standalone 产物：`build-standalone/traffic/romam-traffic`
   - Docker 镜像会内置该二进制，便于 containerlab 节点直接跑流量

4) **网络观测工具（Host 侧采集/整理）**
   - Host 侧库：`romam_lab/observe/*`
   - 可编辑示例脚本：`lab/*`

## Documentation

Comprehensive documentation is available in the [docs](docs/) directory.

## Contributing

We welcome contributions! Please see our [Contributing Guidelines](CONTRIBUTING.md) for more details.

## License

This project is licensed under the [MIT License](LICENSE).
