
对于 4090，在确保有足够上下文长度的情况下，推荐使用以下模型

- qwen2.5-coder:14b
- deepseek-coder-v2 （不支持 tools 调用）

```shell
ollama run qwen2.5-coder:14b
```

对于其他显卡，可以使用 （不支持 tools 调用）

```shell
ollama run deepseek-r1:8b
```