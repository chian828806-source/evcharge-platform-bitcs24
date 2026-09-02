# EVCharge 开发环境复刻与 GPT 交接手册

本文用于在另一台 Windows 电脑上复刻当前的 VMware、Ubuntu、SSH、VS Code、Qt/qmake、SQLite 和 Python ML 工作流，并让新的 GPT 在不丢失上下文的情况下继续开发。

> 安全约束：压缩包和 Git 仓库不包含 SSH 私钥、GitHub 凭据或明文系统密码。新电脑必须重新生成密钥；虚拟机密码由项目所有者通过私密渠道提供或在控制台中重设。

## 1. 当前冻结状态

| 项目 | 当前值 |
| --- | --- |
| GitHub | `https://github.com/chian828806-source/evcharge-platform-bitcs24.git` |
| 基线 | `origin/develop`，提交 `4d925c3` |
| 当前分支 | `feature/ml-foundation` |
| 当前提交 | `26da464` |
| 虚拟机项目目录 | `/home/bit/workspace/evcharge-platform-bitcs24-develop-ml` |
| Windows 大文件根目录 | `E:\\CodexP\\Project9.1`；新电脑应改成非 C 盘目录 |
| VM 用户 | `bit` |
| 当前 VM IP | `192.168.88.128`，NAT 地址可能变化，不能写死 |

分支关系：

```text
origin/develop 4d925c3
└── cdd72b3  feat: add ML prediction foundation
    └── 26da464  feat: add database-backed ML history workflow
```

当前功能状态：

- Qt 用户端、Qt 管理端和 Qt/C++ 服务端已拆为三个独立模块；
- TCP JSON Lines、半包/粘包、Session、Dispatcher、错误响应和 WebSocket 基础已建立；
- SQLite 当前为 11 张业务/数据表和 20 个显式索引；
- Cary CC0 数据、模拟数据库、标准小时数据和三个模型已随分支提交；
- 1h、6h、24h 模型相对七天周期基线的测试集 MAE 提升约为 51.5%、26.4%、14.9%；
- Qt 后端业务 Service、数据库 Repository、正式 ML 导出/导入 Service 和两个客户端 UI 仍需要队友继续实现。

## 2. 当前实测版本

### Windows 主机

| 软件 | 版本 |
| --- | --- |
| VMware Workstation | 17.5.2 build-23775571 |
| Visual Studio Code | 1.110.0 x64 |
| OpenSSH Client | Windows 自带版本 |

当前使用的 VS Code 扩展：

```text
ms-vscode-remote.remote-ssh
ms-vscode-remote.remote-ssh-edit
ms-vscode.remote-explorer
ms-vscode.cpptools
ms-vscode.cpptools-extension-pack
ms-vscode.cmake-tools
ms-python.python
ms-python.vscode-pylance
ms-python.vscode-python-envs
```

### Ubuntu 虚拟机

| 软件 | 版本 |
| --- | --- |
| Ubuntu | 22.04.3 LTS x86_64 |
| Qt | 6.2.4 |
| qmake | 3.1 |
| G++ | 11.4.0 |
| CMake | 3.22.1 |
| GNU Make | 4.3 |
| Git | 2.34.1 |
| Python | 3.10.12 |

## 3. 大文件目录原则

不要把以下内容放在 C 盘：

- VMware 安装包和 Ubuntu 分卷压缩包；
- 解压后的 `.vmx`、`.vmdk`、快照和内存文件；
- 项目工作区、训练数据、数据库和模型；
- 本交接压缩包。

推荐目录：

```text
D:\\VMs\\Ubuntu2204Bit\\
D:\\Projects\\evcharge-platform-bitcs24\\
D:\\ProjectBackups\\
```

C 盘只保留 VS Code、VMware 程序本体、Windows 用户配置和体积很小的 SSH 密钥。

## 4. VMware 与 Ubuntu 复刻

原始学习资料包含：

```text
学生资料/01.开发环境/
├── vmware17.5.2/
├── vmware17.5.2.rar
├── Ubuntu2204Bit.zip.001
└── Ubuntu2204Bit.zip.002
```

复刻方式二选一。

### 方式 A：从分卷压缩包重建

1. 把 `.001` 和 `.002` 放在同一目录；
2. 使用 7-Zip 只对 `.001` 执行解压，程序会自动读取 `.002`；
3. 解压目标选择非 C 盘，例如 `D:\\VMs\\Ubuntu2204Bit`；
4. 安装 VMware Workstation 17.5.2；
5. 在 VMware 中打开解压目录下的 `BitDev.vmx`；
6. VMware 询问移动或复制时，新电脑选择“我已复制该虚拟机”；
7. 网络模式使用 NAT。

### 方式 B：复制当前完整虚拟机

必须先在 Ubuntu 中正常关机，确认 VMware 显示已关闭，然后复制整个 `Ubuntu2204Bit` 目录。禁止在虚拟机运行或挂起时复制 `.vmdk`。`.lck` 表示文件仍被占用；只有确认没有任何 VMware 进程使用该虚拟机时才处理锁文件。

启动后查询新 IP：

```bash
hostname -I
```

## 5. Ubuntu 基础环境

在虚拟机终端执行：

```bash
sudo apt update
sudo apt install -y \
  openssh-server curl wget git build-essential cmake zip sqlite3 \
  python3 python3-venv \
  qt6-base-dev qt6-base-dev-tools qmake6 \
  libqt6sql6-sqlite libqt6websockets6-dev
sudo systemctl enable --now ssh
```

后续 UI 使用 QChart、QWebEngineView 时再安装对应 Qt6 开发包，避免为了尚未实现的页面引入无关依赖。

验证：

```bash
qmake6 -v
g++ --version
git --version
python3 --version
systemctl is-active ssh
```

VS Code Remote SSH 如果提示远端无法下载 Server，先确认 Ubuntu 已安装 `curl` 或 `wget`，并检查虚拟机能访问网络。

## 6. SSH 密钥与 Windows 配置

在新电脑 PowerShell 中生成新密钥，不要复制或提交旧电脑的私钥：

```powershell
ssh-keygen -t ed25519 -f "$env:USERPROFILE\.ssh\id_ed25519_bitdev"
```

把 `.pub` 公钥加入虚拟机用户的 `~/.ssh/authorized_keys`。第一次可使用虚拟机密码交互登录完成，密码不得写入配置文件或项目 README。

Windows 的 `%USERPROFILE%\\.ssh\\config`：

```sshconfig
Host BitDev
  HostName <hostname -I 得到的新IP>
  User bit
  Port 22
  IdentityFile C:\Users\<Windows用户名>\.ssh\id_ed25519_bitdev
  IdentitiesOnly yes
  ServerAliveInterval 30
  ServerAliveCountMax 3
```

验证：

```powershell
ssh BitDev
ssh BitDev "hostname -I && whoami"
```

如果 IP 改变，只修改 `HostName`。不要重新生成项目、密钥或 VS Code 配置。

## 7. VS Code Remote SSH

在新电脑安装 VS Code 后执行：

```powershell
code --install-extension ms-vscode-remote.remote-ssh
code --install-extension ms-vscode.cpptools-extension-pack
code --install-extension ms-vscode.cmake-tools
code --install-extension ms-python.python
code --install-extension ms-python.vscode-pylance
```

连接步骤：

1. 打开命令面板；
2. 运行 `Remote-SSH: Connect to Host...`；
3. 选择 `BitDev`；
4. 连接后选择“打开文件夹”；
5. 打开 `/home/bit/workspace/evcharge-platform-bitcs24-develop-ml`；
6. Python 解释器选择 `${workspaceFolder}/.venv/bin/python`。

以后所有正式开发都在这个 SSH 目录完成，不再以 Windows 同步副本作为主工作区。

## 8. 恢复项目

### 有 GitHub 权限时

先接受仓库邀请，然后在虚拟机执行：

```bash
mkdir -p /home/bit/workspace
cd /home/bit/workspace
git clone --branch feature/ml-foundation \
  https://github.com/chian828806-source/evcharge-platform-bitcs24.git \
  evcharge-platform-bitcs24-develop-ml
cd evcharge-platform-bitcs24-develop-ml
git log --oneline --decorate -3
```

私有仓库需要 GitHub 凭据。推荐在新电脑配置 GitHub SSH key，并把仓库 remote 改为 SSH；不要把 token 写进命令历史或 README。

### 只有本压缩包时

压缩包内同时包含完整工作树和 `repository.bundle`。可以直接阅读工作树；需要恢复 Git 历史时，在解压目录的上一级执行：

```bash
git clone evcharge-platform-bitcs24/repository.bundle restored-evcharge
cd restored-evcharge
git switch feature/ml-foundation
git remote remove origin
git remote add origin https://github.com/chian828806-source/evcharge-platform-bitcs24.git
```

## 9. Python 与 ML 环境

虚拟环境不打包，因为它包含机器相关绝对路径。重建方法：

```bash
cd /home/bit/workspace/evcharge-platform-bitcs24-develop-ml
python3 -m venv .venv
.venv/bin/python -m pip install --upgrade pip
.venv/bin/python -m pip install -r ml/requirements.txt
```

`ml/requirements.txt` 固定了与已提交 `.joblib` 模型兼容的版本。依赖版本变化后必须重训全部三个模型。

ML 黑盒输入是服务端导出的连续小时 CSV，必填字段：

```text
timestamp
station_id
total_pile_count
session_starts
energy_kwh
station_load
```

输出是：

```text
ml/output/<batchId>/predictions.json
```

每个站点包含 1h、6h、24h 三条预测，字段对应数据库 `prediction` 表。详细契约见 `ml/README.md` 和 `docs/03-API.md`。

## 10. 数据库与 ML 完整闭环

重新生成模拟数据库和标准输入：

```bash
.venv/bin/python -m database.simulation.build_cary_database \
  --input ml/data/raw/cary_ev_charging_sessions.csv \
  --database database/evcharge_cary_simulation.db \
  --from-date 2019-01-01 \
  --batch-no CARY-2019-V1

.venv/bin/python -m database.simulation.export_ml_history \
  --database database/evcharge_cary_simulation.db \
  --batch-no CARY-2019-V1 \
  --output ml/data/processed/station_hourly_load.csv
```

训练和预测：

```bash
.venv/bin/python -m ml.train \
  --input ml/data/processed/station_hourly_load.csv \
  --model-dir ml/models \
  --report ml/reports/training_report.json

.venv/bin/python -m ml.predict \
  --history ml/data/processed/station_hourly_load.csv \
  --model-dir ml/models \
  --batch-id HANDOFF-DEMO \
  --output ml/output/HANDOFF-DEMO/predictions.json
```

开发期的 `database/simulation` 脚本用于可复现数据构建。生产系统必须保持：

```text
SQLite → Qt/C++ 服务端导出 CSV → ML → 预测 JSON → 服务端事务导入 SQLite
```

ML 不得直接连接生产 SQLite，Web、Qt 用户端和 Qt 管理端也不得绕过服务端访问数据库。

## 11. Qt/qmake 构建与验证

使用仓库外构建目录：

```bash
mkdir -p /home/bit/workspace/build-evcharge
cd /home/bit/workspace/build-evcharge
qmake6 /home/bit/workspace/evcharge-platform-bitcs24-develop-ml/evcharge-platform.pro
make -j2
./tests/network/network-protocol-tests
```

当前实测结果为 Qt 全量构建成功，网络协议测试 `TOTAL_FAILURES=0`，共 19 项通过。

Python 测试：

```bash
cd /home/bit/workspace/evcharge-platform-bitcs24-develop-ml
.venv/bin/python -m unittest discover -s tests/database -v
.venv/bin/python -m unittest discover -s ml/tests -v
```

当前实测为数据库桥接 2 项、ML 6 项全部通过。

## 12. Git 工作流

```bash
git status --short --branch
git fetch origin
git switch feature/ml-foundation
git pull --ff-only
```

规则：

- 不直接向 `main` 或 `develop` 开发；
- 新工作从远端基线创建 `feature/...` 分支；
- 不使用 `git reset --hard`、`git checkout --` 清除未知改动；
- 修改公共协议、数据库 schema 或 ML 文件契约时必须同步文档和测试；
- 提交前运行 `git diff --check` 及相关测试；
- 推送前检查大数据、虚拟环境、私钥、token 和预测临时输出没有误入暂存区。

当前虚拟机的 HTTPS remote 可以拉取公开内容，但没有保存 GitHub 推送凭据。需要在新电脑配置 GitHub SSH 身份，或者从已经登录 GitHub 的 Windows 环境推送。

## 13. 给接替 GPT 的工作上下文

接替后先执行：

```bash
pwd
git status --short --branch
git log --oneline --decorate -5
qmake6 -v
.venv/bin/python --version
```

必须遵守的架构边界：

- `qt-user` 和 `qt-admin` 是两个独立客户端；
- `qt-server` 是无界面的独立服务端；
- `shared/protocol` 只放公共协议、消息和错误码，不放业务 Service；
- 客户端页面只能调用各自的 NetworkClient，不直接操作 `QTcpSocket` 或 SQLite；
- Handler 负责消息字段和调用 Service，Service 负责业务规则，Repository 负责参数化 SQL 和事务；
- Python ML 只消费标准 CSV 并输出标准 JSON；
- `charging_session_history` 保存公开会话事实，不伪造用户和支付；
- `station_hourly_metric` 是服务端到模型的稳定小时级数据契约。

下一阶段优先事项：

1. 后端实现 `MLHistoryExportService`，输出与参考导出器相同的 CSV；
2. 后端实现 `PredictionImportService`，整批校验并事务写入 `prediction`；
3. 实现 Repository 和核心业务 Service；
4. 为用户端和管理端补齐 UI 与业务调用；
5. 增加从未见测试窗口截取小段、滚动预测并与真实值比较的 backtest 演示；
6. Web 大屏订阅 `prediction` topic 并展示模型结果。

不要把 `database/simulation` 当作生产服务端实现；它是数据库构建和双方契约的参考工具。

## 14. 故障排查

### VMware 看不到虚拟机

使用“打开虚拟机”选择 `BitDev.vmx`。库列表为空不代表虚拟机文件丢失。

### VMware 提示虚拟机正在使用

先确认其他 VMware 进程和旧电脑实例已经关闭。移动后的虚拟机可选择“获取所有权”；不要在虚拟机仍运行时删除锁或复制磁盘。

### SSH 连接失败

依次检查：

```bash
hostname -I
systemctl status ssh
```

然后检查 VMware NAT、Windows SSH 配置中的 IP、虚拟机防火墙和公钥。

### VS Code Server 下载失败

在虚拟机安装 `curl`、`wget`，检查 DNS 和 HTTPS；之后重新执行 Remote SSH 连接。

### Python 模型无法加载

删除并重建 `.venv`，严格安装 `ml/requirements.txt`。不要用不同版本的 scikit-learn 强行加载模型。

### GitHub 推送要求用户名或失败

虚拟机没有保存 GitHub 凭据。配置 GitHub SSH key 并使用 `git@github.com:chian828806-source/evcharge-platform-bitcs24.git`，或在已登录 GitHub 的主机中推送。

## 15. 压缩包范围

压缩包包含：

- 当前提交的全部源码和文档；
- Cary 原始数据和标准小时数据；
- 预构建 SQLite 模拟数据库；
- 1h、6h、24h 模型和评估报告；
- `repository.bundle` 完整 Git 历史；
- 本交接手册。

压缩包不包含：

- VMware 虚拟磁盘和内存文件；
- `.venv`、编译产物、缓存和运行时预测；
- SSH 私钥、系统密码、GitHub token 和其他凭据；
- 已淘汰的 `station_hourly_load_legacy.csv`。
