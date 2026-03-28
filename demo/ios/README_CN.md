# mk-parser iOS 演示应用

[mk-parser](../../README_CN.md) 的 SwiftUI 演示 App — 面向 AI 流式输出场景的增量 Markdown 解析器。

## 环境要求

| 工具    | 版本   |
|---------|--------|
| Xcode   | 15+    |
| iOS     | 16+    |
| macOS   | 13+（Catalyst） |
| Swift   | 5.9+   |

## 在 Xcode 中打开

```
File → Open → demo/ios/Package.swift
```

Xcode 会自动从项目根目录解析 `MkParser` 本地包依赖。

## 功能

| 标签页  | 说明 |
|---------|------|
| Events  | 解析器实时产生的 OPEN / CLOSE / TEXT / MODIFY 事件流 |
| Preview | 渲染后的 Markdown 块（Catppuccin Mocha 配色） |
| LLM     | 接入真实 OpenAI 兼容 API 或使用离线 Mock 进行流式演示 |

## 架构

```
App/
├── MkParserDemoApp.swift     @main 入口
├── ContentView.swift         输入框 + 操作栏 + TabView
├── DemoViewModel.swift       ObservableObject 状态 + 解析逻辑
├── EventsView.swift          彩色事件列表
├── render/
│   ├── MkBlock.swift         Swift enum（对标 Kotlin sealed class）
│   ├── MkBlockParser.swift   推送事件 → [MkBlock] 状态机
│   └── MkRenderView.swift    SwiftUI List 渲染器
└── llm/
    ├── LlmProvider.swift     协议 + LlmConfig
    ├── MockProvider.swift    离线 Mock（无需 API Key）
    └── OpenAIProvider.swift  URLSession async/await SSE 流式接入
```

## 流式处理管线

```
用户输入 / LLM token
        ↓
   MkParser.feed()           （C core 推送事件）
        ↓
   DemoViewModel              累积文本
        ↓
   MkBlockParser.parse()      状态机 → [MkBlock]
        ↓
   MkRenderView               List { blockRow($block) }
```

## 使用 LLM 标签页

1. 关闭「Use Mock Provider」开关
2. 填入 OpenAI API Key
3. 输入提示词，点击 **▶ Start LLM Stream**

Mock 模式无需 API Key，完全离线运行。
