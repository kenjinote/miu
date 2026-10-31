# ⚡ miu v1.0.38 — Markdown Live Preview

Press <kbd>Alt</kbd> + <kbd>P</kbd> for instant **CommonMark & GFM** split preview (`150ms` debounce / zero external browser).

> [!TIP]
> **Instant Preview Even on 1GB+ Files**
> Safely slices the first `512KB` on a clean UTF-8 boundary with zero disk copies.

| Feature | Syntax Sample | Engine | Status |
| :--- | :---: | :--- | ---: |
| **GFM Tables** | `\| A \| B \|` / `\| :--- \|` | Native C++ + WebView2 | ✅ Ready |
| **Code & Math** | `` `a | b` `` / `snake_case` | Token Escaping | ⚡ 0.8ms |
| **GFM Alerts** | `> [!NOTE]` / `> [!TIP]` | Auto Dark/Light | 🎨 Synced |

- [x] **Task Lists & Multi-Level Nested Lists**
  - Automatic `<ul>` / `<ol>` hierarchy with <kbd>F5</kbd> reload
- [ ] Next update: Custom CSS preview themes

```cpp
// Native C++20 Direct2D + WebView2 Split Preview
void Editor::togglePreview() {
    isPreviewOpen = !isPreviewOpen;
    updatePreviewContent(); // Zero temp-file overhead
}
```
