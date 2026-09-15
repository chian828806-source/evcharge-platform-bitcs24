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
bash bigdata/environment/check_environment.sh
```

脚本只创建可重复使用的 `/evcharge/_healthcheck` 目录，不修改一期数据库或业务数据。

完整演示命令见 [B-PIPELINE-RUNBOOK.md](B-PIPELINE-RUNBOOK.md)。
## Acceptance
Raw CSV 能写入 HDFS ODS，且版本与健康检查可复现。
