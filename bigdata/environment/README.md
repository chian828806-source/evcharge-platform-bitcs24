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
bash bigdata/environment/manage_services.sh start
bash bigdata/environment/check_environment.sh
```

脚本只创建可重复使用的 `/evcharge/_healthcheck` 目录，不修改一期数据库或业务数据。

当前单节点验收基线：

| 组件 | 版本 |
| --- | --- |
| Java | 1.8.0_261 |
| Hadoop | 3.2.1 |
| Python | 3.11.x |
| PySpark | 3.5.9 |

服务管理：

```bash
bash bigdata/environment/manage_services.sh status
bash bigdata/environment/manage_services.sh stop
bash bigdata/environment/manage_services.sh start
```

HDFS Web 页面监听 9870。YARN 8088 保持在虚拟机回环地址，需要从宿主机查看时使用临时端口转发：

```bash
ssh -L 8088:127.0.0.1:8088 bit@<vm-ip>
```

随后在宿主机浏览器打开 `http://127.0.0.1:8088`。

完整演示命令见 [B-PIPELINE-RUNBOOK.md](B-PIPELINE-RUNBOOK.md)。
## Acceptance
Raw CSV 能写入 HDFS ODS，且版本与健康检查可复现。
