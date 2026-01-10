# ROMAM Linux Network Namespace Runner

`standalone/linuxns/run.py` 用 **Linux network namespace + veth + tc** 复刻一套“无 Mininet 依赖”的实验环境：每个节点一个 netns，每个 netns 内启动一个 `romamd`。

它复用 `standalone/mininet/specs/*.toml` 这套 Spec（同一份 spec 既能跑 Mininet，也能跑纯 Linux netns）。

## 依赖

- Linux（需要内核支持 netns/veth/tc）
- `iproute2`（提供 `ip`/`tc`）
- Python3（TOML 需要 Python3.11+ 或安装 `tomli`；也可以用 `.json` spec）

## 使用

先编译 `romamd`：

```bash
cmake -S standalone -B build-standalone
cmake --build build-standalone -j
```

运行（需要 root）：

```bash
sudo python3 standalone/linuxns/run.py --spec standalone/mininet/specs/linear5.toml
```

Abilene：

```bash
sudo python3 standalone/linuxns/run.py --spec standalone/mininet/specs/abilene.toml
```

常用参数：

- `--out <dir>`：把生成的配置/日志输出到指定目录
- `--dry-run`：不写系统路由表，只打印预期下发路由
- `--show-routes`：收敛后打印每个节点的 `route_table` 路由
- `--cleanup`：运行前删除同 prefix 的旧 netns
- `--prefix <p>`：netns 名称前缀（默认 `romam-`）

