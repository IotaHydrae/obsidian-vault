# Ollama 模型运行记录

> 用 `ollama run <模型名>` 下载并运行模型；选型时需结合显存、量化方式、上下文长度和实际任务测试。

## 运行

```bash
ollama run qwen2.5-coder:14b
ollama run deepseek-r1:8b
```

## 注意

- 候选模型包括 `qwen2.5-coder:14b`、`deepseek-r1:8b` 和 `deepseek-coder-v2`；此处没有针对 RTX 4090 或其他显卡建立性能基线。
- 工具调用支持取决于具体模型、模板和运行环境，使用前验证实际调用链。
- 可用上下文长度与显存占用需在选定模型、量化方式及运行配置下测量。
