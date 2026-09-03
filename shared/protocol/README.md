# protocol — Socket公共协议

本目录把 docs/03-API.md 中已经确定的消息外壳、消息名称和错误码写成可复用的C++代码。

## 文件说明

| 文件 | 功能 |
| --- | --- |
| errorcodes.h | 集中定义文档中的错误码，避免代码中出现无含义数字 |
| messagetypes.h/.cpp | 集中定义30种TCP消息、WebSocket消息和4个大屏主题 |
| protocolmessage.h/.cpp | RequestMessage与ResponseMessage的JSON转换和基础校验 |
| jsonlinecodec.h/.cpp | 处理TCP半包、粘包、换行分帧和紧凑JSON编码 |
| protocol.pri | 供多个qmake工程复用本目录源码 |

## 数据流

~~~text
TCP字节
  → JsonLineCodec按换行切成完整帧
  → QJsonDocument解析JSON
  → RequestMessage校验公共外壳
  → MessageDispatcher交给服务端业务Handler
  → ResponseMessage生成统一响应
  → JsonLineCodec编码后写回Socket
~~~

## 边界

- 本模块只校验 requestId、type、sessionId、payload 这些公共字段。
- 手机号、pileId、orderId等业务参数由服务端对应Handler或Service校验。
- 本模块不得写SQL、访问界面或实现订单状态变化。
