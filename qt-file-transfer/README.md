# Qt Socket 文件传输工具

本目录是课程作业的独立 Qt 6 工程，包含一个客户端和一个服务端。二者使用 TCP 连接，支持文本、图片及其他二进制文件传输。

## 构建

在 Ubuntu 22.04（Qt 6）中执行：

```bash
cd qt-file-transfer
qmake6 qt-file-transfer.pro
make -j2
```

生成的程序分别位于 `client/file-transfer-client` 和 `server/file-transfer-server`。

## 运行

先启动服务端：

```bash
./server/file-transfer-server
```

输入监听端口（默认 45454）和接收目录，点击“开始监听”。再启动客户端：

```bash
./client/file-transfer-client
```

输入服务端 IP（同一台机器可用 `127.0.0.1`）和端口，点击“连接”，然后选择文件或把文件拖到拖拽区域。服务端会自动将文件保存到接收目录。

## 传输协议

每个文件由一个固定头、UTF-8 文件名和文件数据组成，所有整数使用大端序：

| 字段 | 大小 |
| --- | ---: |
| Magic (`FTR1`) | 4 字节 |
| 文件名长度 | 2 字节 |
| 文件大小 | 8 字节 |
| 文件名 | 可变长度 |
| 文件数据 | 文件大小 |

接收端持续累积 `readyRead()` 数据，只有在固定头、完整文件名和声明的文件数据都到齐后才落盘，因此可以正确处理 TCP 分包和粘包。

## 作业要求对应

- `.ui`：客户端主窗口、连接设置对话框、服务端主窗口。
- Qt 资源/QSS：`resources/resources.qrc` 和 `resources/style.qss`。
- 信号与槽：连接状态、进度、错误、设置对话框参数和拖拽文件信号。
- 事件重写：`DropZone` 重写 `dragEnterEvent`、`dragLeaveEvent`、`dropEvent`。
- 架构分离：`common/FileTransfer` 负责协议和文件读写，窗口类只负责界面交互。

## 验收建议

在服务端接收目录中对收到的文件执行 `sha256sum`，与客户端发送前的校验值比较。建议至少测试 `.txt`、中文文件名图片、空文件和较大的二进制文件。
