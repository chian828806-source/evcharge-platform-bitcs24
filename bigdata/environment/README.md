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
## Future Implementation
增加容器/虚拟机配置与检查脚本。
## Acceptance
Raw CSV 能写入 HDFS ODS，且版本与健康检查可复现。
