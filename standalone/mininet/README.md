# ROMAM Mininet Runner

`standalone/mininet/run.py` 会根据 Spec 文件自动：

- 创建 Mininet 拓扑（TCLink 支持 delay/loss/bw）
- 给每个节点配置 loopback `router_id`、链路地址
- 启动每个节点的 `romamd`（每个 host/namespace 一个进程）
- 可选：收敛后 ping 自检、打印路由表

## 依赖

- Mininet（Python 包 + 相关系统依赖）
- `iproute2`（`ip`/`tc`）
- Python3（TOML 需要 Python3.11+ 或安装 `tomli`；也可以用 `.json` spec）

## 使用

```bash
sudo python3 standalone/mininet/run.py --spec standalone/mininet/specs/linear5.toml --show-routes
```

进入 CLI：

```bash
sudo python3 standalone/mininet/run.py --spec standalone/mininet/specs/abilene.toml --cli
```

常用参数：

- `--out <dir>`：保存生成配置/日志的目录（默认临时目录）
- `--romamd <path>`：指定 `romamd` 路径（默认 `./build-standalone/daemon/romamd`）
- `--dry-run`：不写系统路由表
- `--duration <sec>`：跑指定秒数后自动退出

## Spec

- Schema：`standalone/mininet/spec.schema.json`
- Examples：`standalone/mininet/specs/*.toml`

额外字段：

- `romam.routing_algo`：选择路由算法（默认 `spf`，对应配置文件 `routing_algo=...`）。
