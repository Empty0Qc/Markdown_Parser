# Q&A 07 — 范围最终确认

**时间**：2026-03-22
**阶段**：Phase 2 → Phase 3 过渡

---

## 两项核心需求确认

### 1. 解析与渲染完全解耦 ✅
- C 核心层只产出 AST 事件，不含任何渲染逻辑
- 渲染适配器按平台独立实现，消费 AST 事件

### 2. 解析层跨平台 ✅

目标平台：

| 平台 | 绑定方式 | 优先级 |
|---|---|---|
| 浏览器 / Node.js | WASM（Emscripten） | 高（Web 端快速验证） |
| Server 原生 | .so / .dylib | 高（主要运行场景） |
| Android | JNI via NDK | 中 |
| iOS | .a / xcframework | 中 |

### C 核心 API 设计原则（由跨平台需求推导）
- 纯 C 接口（`extern "C"`）
- 无全局状态（多实例安全）
- 无平台相关头文件
- Arena 支持注入自定义 malloc/free

---

## 交付范围（A 方案）

- C 核心（Parser + AST + Arena + Plugin 系统）
- 四个平台绑定（WASM、Server native、Android JNI、iOS）
- Web Demo（浏览器端快速验证，流式场景可视化）
- 测试套件（单元 + 集成 + GFM 合规 + 性能）
