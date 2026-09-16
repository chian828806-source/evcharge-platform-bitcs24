# Environment

## Module Responsibility
定义 Hadoop、Spark、Java、Python 的可复现实验环境。
## Owner
B（Data Pipeline）。
## Input
版本约束与基础设施配置。
## Output
环境说明和健康检查证据。
## Allowed Dependencies
公开环境变量和 Raw/ODS Contract。
## Forbidden Dependencies
密钥、生产数据、机器绝对路径或业务实现。
## Public Contract
HDFS 可写与 `spark-submit` 可运行的环境能力。
## Run

在 Hadoop 虚拟机执行：

```bash
bash bigdata/environment/manage_services.sh start all
bash bigdata/environment/check_environment.sh
```

`check_environment.sh` 只创建可重复使用的 `/evcharge/_healthcheck` 目录，不修改一期数据库或业务数据。

`manage_services.sh` 来源于 B 的单节点环境成果，并在集成分支扩展了两种 profile：

- `all`：检查和管理 NameNode、DataNode、SecondaryNameNode、ResourceManager、NodeManager，
  用于 B 的 Hadoop 3.2.1 + PySpark 3.5.9 标准教学环境。
- `hdfs`：只管理三个 HDFS 服务，用于不依赖 YARN 的本地 `spark-submit` 演示机，例如当前
  UrbanEV 集成环境。它不会把缺少 YARN 误报成 HDFS 故障。

```bash
# 五服务标准环境
bash bigdata/environment/manage_services.sh status all

# 只使用 HDFS 的兼容环境
bash bigdata/environment/manage_services.sh status hdfs
```

首次 `start` 会幂等创建 `/evcharge/raw`、`ods`、`quality`、`rejects`、`dwd`、`dws`、
`ads` 和 `ml`，不会删除或覆盖已有批次。`stop` 只在操作者明确执行时停止所选 profile 的服务。

当前已验证的环境存在两套版本，代码 Contract 保持一致，但不要混用虚拟环境目录：

| 环境 | Java | Hadoop/Spark | Python | 服务 profile |
| --- | --- | --- | --- | --- |
| B 标准教学机 `/home/bit/EVCharge/.venv` | 8u261 | Hadoop 3.2.1 / PySpark 3.5.9 | 3.11.16 | `all` |
| UrbanEV 集成机 `node100` | Java 8 | Hadoop 3.3.0 / Spark 3.4.1 | 3.11.11 | `hdfs` |

提交代码时不要写死 `/home/bit`、`/home/hadoop` 或虚拟机 IP；README 中的路径只是部署示例。
HDFS 9870 页面与 YARN 8088 页面属于环境状态，不是 Flask Dashboard API。

HDFS Web 页面监听 9870。若 YARN 8088 只监听虚拟机回环地址，可在宿主机临时转发：

```bash
ssh -L 8088:127.0.0.1:8088 bit@<vm-ip>
```

完整演示命令见 [B-PIPELINE-RUNBOOK.md](B-PIPELINE-RUNBOOK.md)。
## Acceptance
Raw CSV 能写入 HDFS ODS，且版本与健康检查可复现。
