# 文档翻译系统 (i18n Translation System)

基于 LLM 的增量文档翻译系统，为 Glyphix 文档项目提供自动化的多语言翻译能力。

## 特性

- **增量翻译**：基于内容 hash 的段落级缓存，只翻译新增或修改的内容
- **VuePress 感知**：正确处理 `:::` 容器指令、`<Glyphix>` 等自定义组件、frontmatter
- **格式保留**：保持 Markdown 格式、代码块、链接、行内代码等不变
- **术语表**：可自定义词汇表确保翻译一致性
- **自定义 Prompt**：可编辑的 prompt 模板，灵活调整翻译风格
- **人工校对支持**：检测译文的人工修改并保护，生成 review 报告
- **质量检查**：自动检测格式丢失、结构变化等翻译质量问题
- **可扩展**：语言无关设计，支持替换 LLM 模型或添加新语言

## 快速开始

### 1. 安装依赖

```bash
cd web-docs
pnpm install
```

### 2. 配置 API Key

```bash
# 方法一：环境变量
export TRANSLATE_API_KEY="sk-your-api-key"

# 方法二：.env 文件
cp .env.example .env
# 编辑 .env 填入你的 API Key
```

### 3. 运行翻译

```bash
# 翻译所有变更的文件
pnpm translate

# 翻译单个文件
pnpm translate tutorials/getting-started.md

# 翻译整个目录（两种写法等价）
pnpm translate tutorials
pnpm translate tutorials/

# 同时指定多个目录 / 文件
pnpm translate tutorials api cookbook

# 使用 glob 模式
pnpm translate "tutorials/*.md"         # 目录一层
pnpm translate "framework/**/*.md"      # 递归匹配

# 强制重新翻译（忽略缓存）
pnpm translate --force
pnpm translate --force tutorials        # 只强制翻译某个目录

# 预览（不写入文件）
pnpm translate --dry-run

# 仅检查翻译状态
pnpm translate --status

# 仅生成 review 报告
pnpm translate --review-only
```

### 4. 修复链接与锚点

翻译完成后，译文中的文档链接和锚点通常需要额外的修复。这也是本翻译工具的特色功能——它不仅修改路径，还能智能**解决中英锚点对不齐**的问题。

#### 工具能做什么？

1. **绝对路径 → 相对路径**
   源文档中通常使用绝对路径（如 `/framework/component/README.md`）。工具会将其转换为当前文件的相对路径（如 `../component/README.md`），确保在托管时链接不丢失。它能够正确分辨哪些文件在译文目录中存在，如果不在，它会降级指向主语言源以作 fallback。
2. **多语言锚点自动对齐**
   源文件中的引用往往直接使用源语言的 Header，如 `[生命周期](./component.md#生命周期)`。由于目标文件的标题已经被翻译成英文，这个锚点也就失效了。本工具拥有强大的锚点翻译和推断能力：
   - **基于位置**：首先会尝试根据原文和译文 DOM tree 中同位置的 Header（例如都在第 5 行）自动更新 `slug`。
   - **LLM 辅助验证**：当结构匹配不上时，工具会自动发起基于 LLM 的查询，让模型在一众外语 Header 里为当前锚点挑出正确的翻译匹配项！

#### 如何使用？

运行以下命令将其转换为相对路径和合法锚点，以确保译文目录内的跳转正确：

```bash
# 预览会修改哪些文件，不在磁盘上实际写入
pnpm translate --fix-links --dry-run
pnpm translate --fix-links --dry-run "en/tutorials/*.md"

# 应用修复（支持全部处理或统配特定文件）
pnpm translate --fix-links
pnpm translate --fix-links "en/api/**/*.md"
```

其中，`en/` 是译文目录的前缀，意味着我们只想修复译文目录中的链接。通常我们不需要修复源文档中的链接，因为它们已经被正确地手工维护了。

> **注意**：对于译文目录中不存在的资源（如图片、SVG），工具会保留原始绝对路径，由文档路由继续提供主语言版本。该过程建议配置 API Key 以便启用 LLM 锚点处理的高级特性。

## 目录结构

```
i18n/
├── cli.ts                    # CLI 入口
├── pipeline.ts               # 翻译流水线编排
├── config.ts                 # 配置加载
├── types.ts                  # TypeScript 类型定义
├── translate.config.json     # 翻译配置文件
├── glossary.json             # 术语表
├── markdown/
│   ├── parser.ts             # Markdown 解析器（段落分割）
│   └── reassembler.ts        # 译文重组
├── translator/
│   ├── index.ts              # 翻译器工厂
│   ├── openai.ts             # OpenAI 兼容 API 实现
│   └── prompt-builder.ts     # Prompt 模板构建
├── cache/
│   └── index.ts              # 缓存管理器
├── review/
│   └── reporter.ts           # Review 报告生成器
├── utils/
│   └── logger.ts             # 日志工具
└── prompts/
    └── translate.md          # 可编辑的 Prompt 模板
```

## 配置说明

### `translate.config.json`

| 字段 | 说明 | 默认值 |
|------|------|--------|
| `sourceLanguage` | 源语言代码 | `"zh-CN"` |
| `targetLanguage` | 目标语言代码 | `"en"` |
| `sourceDir` | 源文件目录 | `"src"` |
| `outputDir` | 译文输出目录 | `"src/en"` |
| `cacheDir` | 缓存目录 | `".translation-cache"` |
| `reviewDir` | Review 报告目录 | `".translation-review"` |
| `glossaryPath` | 术语表路径 | `"i18n/glossary.json"` |
| `promptTemplatePath` | Prompt 模板路径 | `"i18n/prompts/translate.md"` |
| `include` | 要翻译的文件 glob | `["**/*.md"]` |
| `exclude` | 排除的文件 glob | VuePress 配置等 |
| `llm.provider` | LLM 提供商 | `"openai"` |
| `llm.model` | 模型名称 | `"gemini-3-flash"` |
| `llm.temperature` | 生成温度 | `0.1` |
| `llm.maxTokens` | 最大 token 数 | `8192` |
| `preserveHumanEdits` | 保护人工编辑 | `true` |
| `maxBatchSize` | 每次翻译最大字符数 | `4000` |
| `concurrency` | 并行 API 请求数 | `5` |
| `blockComponents` | 块级组件（内容不翻译） | `["Glyphix", "glyphix"]` |

### 环境变量优先级

环境变量 > `.env` 文件 > `translate.config.json` > 默认值

| 环境变量 | 说明 |
|----------|------|
| `TRANSLATE_API_KEY` | LLM API 密钥 |
| `TRANSLATE_BASE_URL` | API 基础 URL |
| `TRANSLATE_MODEL` | 覆盖模型名称 |

### 术语表 (`glossary.json`)

```json
{
  "entries": [
    { "source": "组件", "target": "component" },
    { "source": "快应用", "target": "Quick App" },
    { "source": "属性", "target": "property", "context": "component property" }
  ]
}
```

### 自定义 Prompt (`prompts/translate.md`)

Prompt 模板支持以下变量：

- `{{sourceLanguage}}` — 源语言名称
- `{{targetLanguage}}` — 目标语言名称
- `{{glossary}}` — 格式化后的术语表

## 工作流程

### 增量翻译流程

```
1. 遍历 src/ 目录收集 .md 文件
2. 对每个文件计算内容 hash，与缓存比较
3. 文件无变化 → 跳过
4. 文件有变化 → 解析为段落（segments）
5. 每个可翻译段落计算 hash，查询段落级缓存
6. 命中缓存 → 复用已有译文
7. 未命中 → 批量发送到 LLM 翻译
8. 执行质量检查
9. 写入 src/en/ 对应路径
10. 更新缓存和文件清单
11. 生成 Review 报告
12. （推荐）运行 `pnpm fix-paths` 将译文中的绝对路径转换为相对路径
```

### 人工校对流程

1. 运行 `pnpm translate` 生成初始译文
2. 运行 `pnpm fix-paths` 修复译文中的绝对路径链接
3. 查看 `.translation-review/` 中的 review 报告
4. 直接编辑 `src/en/` 中的译文文件
5. 再次运行 `pnpm translate` 时，系统会检测人工修改：
   - 默认跳过已修改的文件（保护人工编辑）
   - 使用 `--force` 强制覆盖
   - 修改会记录在 review 报告中

### 扩展新语言

1. 在 `translate.config.json` 中修改 `targetLanguage`（如 `"ja"`）
2. 修改 `outputDir`（如 `"src/ja"`）
3. 可选：创建新的术语表和 prompt 模板
4. 运行 `pnpm translate`

### 替换 LLM 模型

系统使用 OpenAI 兼容 API，可以连接：

- OpenAI（GPT-5、GPT-5-mini 等）
- Azure OpenAI
- Anthropic Claude（通过兼容代理）
- 本地模型（通过 Ollama、vLLM 等提供 OpenAI 兼容接口）

```bash
# 使用不同模型
pnpm translate --model gpt-5-mini

# 使用自定义 API 端点
TRANSLATE_BASE_URL=http://localhost:11434/v1 pnpm translate --model llama3
```

### 输出错乱与模型选择

翻译结果出现结构错乱（格式丢失、内容混淆、乱码等）时，可以按以下思路排查：

#### 1. 缩小每批次的字符数（优先尝试）

输出错乱最常见的原因是单次请求包含的 segment 过多：模型需要在输出端按顺序逐一对应，批次越大越容易出现段落跳号、合并或错乱问题。用 `--max-chars` 调小批次大小往往能立竿见影：

```bash
# 默认约 2000 字符/批，先试试减半
pnpm translate --max-chars 1000 tutorials/getting-started.md

# 仍有问题可以继续调小
pnpm translate --max-chars 500 tutorials/getting-started.md
```

批次稍小时，翻译质量更稳定，但 API 调用次数会相应增多，价格也会增加。建议只在出现问题的文件上调小批次。

#### 2. 换用更强的模型

如果缩小批次后仍有问题，可以尝试换用更强的模型：

```bash
pnpm translate tutorials/getting-started.md --model gemini-3.1-pro 
```

注意，大模型不仅成本更高，而且会非常慢，小模型够用的话不建议强行用。

#### 3. 查看警告日志

翻译完成后，注意观察控制台中的 `quality_warning` 警告，它们会指出具体哪些段落出现了格式丢失或结构变化，方便定向排查。

#### 推荐模型

日常使用优先选择 `gemini-3-flash`，性价比最优。翻译质量不佳时再按上述步骤逐步升级。

## 指定翻译范围

日常工作中翻译部分文件是最高效的方式。工具支持三种模式：

| 形式 | 示例 | 说明 |
|------|------|------|
| 精确文件 | `tutorials/getting-started.md` | 只翻译该文件 |
| 目录 | `tutorials` 或 `tutorials/` | 翻译该目录下的全部文件 |
| Glob | `"tutorials/*.md"` | 通配符匹配，需引号包裹以防 shell 展开 |

路径既可以相对于 `src/`（如 `tutorials`），也可以带 `src/` 前缀（如 `src/tutorials`）。

可以同时传多个参数，它们取 **并集**：

```bash
# 翻译 tutorials 目录和 api 目录下所有变更的文件
pnpm translate tutorials api

# 强制重翻某两个文件
pnpm translate --force tutorials/getting-started.md api/overview.md

# 递归匹配某子树中所有 markdown
pnpm translate "framework/**/*.md"
```

## 缓存说明

缓存文件存储在 `.translation-cache/` 目录中：

- `manifest.json` — 文件级跟踪（源文件 hash、输出文件 hash）
- `segments/zh-CN_en.json` — 段落级翻译缓存

缓存目录应加入 `.gitignore`。清除缓存：

```bash
rm -rf .translation-cache
```

### 从已有翻译填充缓存

如果你已经有一些翻译好的文件（比如手工翻译或之前的翻译系统产出），可以用 `--seed-cache` 从这些文件中填充缓存：

```bash
# 扫描所有已有的翻译文件，填充缓存
pnpm translate --seed-cache

# 强制覆盖已有缓存条目
pnpm translate --seed-cache --force

# 预览操作但不实际写入cache
pnpm translate --seed-cache --dry-run
```

这个功能会：
1. 扫描输出目录 (`src/en/`) 中的所有翻译文件
2. 找到对应的源文件并解析为segments  
3. 将源文件和翻译文件的segment对应关系存入缓存
4. 更新文件manifest，避免重复翻译

适用场景：
- 第一次使用此翻译系统，但已有部分翻译文件
- 重建缓存但不想重新翻译所有内容
- 多人协作时同步缓存状态

### 调试翻译 Prompt

如果翻译质量不理想，可以使用 `--debug-prompt` 查看实际发送给 LLM 的 prompt：

```bash
# 查看默认示例的 prompt （用于测试 prompt 模板）
pnpm translate --debug-prompt

# 调试指定文件的 prompt
pnpm translate --debug-prompt tutorials/getting-started.md

# 只调试第 3 个片段（索引从 0 开始）
pnpm translate --debug-prompt tutorials/intro.md --segment 2

# 调试更多片段
pnpm translate --debug-prompt tutorials/intro.md --max-segments 5

# 输出 API 格式（可直接用于测试）
pnpm translate --debug-prompt --api-format
```

这个功能会显示：
1. 文件解析的 segment 信息（类型、是否可翻译）
2. 完整的 system prompt（包含格式化后的术语表）
3. 用户消息（包含待翻译的内容）
4. Token 数量估算

适用场景：
- 翻译结果不符合预期，检查 prompt 是否正确
- 调整术语表后验证其在 prompt 中的效果  
- 优化自定义 prompt 模板
- 排查特定文件或片段的翻译问题

## 注意事项

- 首次翻译全部文件需要较多 API 调用，建议先小范围测试
- `blockComponents` 中定义的组件（如 `<Glyphix>`）内容不会被翻译
- Review 报告中的 `quality_warning` 条目应重点关注
- 翻译完成后建议人工审查关键页面
