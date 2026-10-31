# ⚡ miu v1.0.38 — Markdown 实时分屏预览

按 <kbd>Alt</kbd> + <kbd>P</kbd> 即刻开启 **CommonMark & GFM** 实时分屏渲染（`150ms` 自动防抖刷新 / 无需外部浏览器）。

> [!TIP]
> **超过 1GB 的超大文件也能瞬间预览**
> 无需任何磁盘拷贝，按 UTF-8 字符边界安全截取前 `512KB` 极速渲染。

| 功能模块 | 语法示例 | 渲染引擎 | 状态 |
| :--- | :---: | :--- | ---: |
| **GFM 表格** | `\| A \| B \|` / `\| :--- \|` | Native C++ + WebView2 | ✅ 已完成 |
| **代码与下划线保护** | `` `a | b` `` / `snake_case` | 边界转义解析 | ⚡ 0.8ms |
| **GFM 提示框** | `> [!NOTE]` / `> [!TIP]` | 深浅色自动联动 | 🎨 已适配 |

- [x] **任务列表与多级嵌套列表**
  - 根据缩进自动生成 `<ul>` / `<ol>` 层级并支持 <kbd>F5</kbd> 刷新
- [ ] 下一版本：支持自定义 CSS 预览主题

```cpp
// Native C++20 Direct2D + WebView2 Split Preview
void Editor::togglePreview() {
    isPreviewOpen = !isPreviewOpen;
    updatePreviewContent(); // Zero temp-file overhead
}
```
