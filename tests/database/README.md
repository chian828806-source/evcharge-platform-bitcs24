# tests/database — 数据库与 ML 数据桥接测试

本目录验证新增历史表、Cary 导入和小时指标 CSV 导出，不依赖 Qt 图形界面。

```bash
python -m unittest discover -s tests/database -v
```

测试只在系统临时目录创建小型数据库，不覆盖仓库中的预构建演示数据库。
