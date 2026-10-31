# ⚡ miu v1.0.38 — Markdown Live Preview

<kbd>Alt</kbd> + <kbd>P</kbd> で **CommonMark** ＆ **GFM** をリアルタイム描画（`150ms` デバウンス / ~~外部ブラウザ不要~~）。

> [!TIP]
> **1GB 超の巨大ファイルでも瞬時にプレビュー**
> ディスクコピーなしで先頭 `512KB` を UTF-8 境界で安全に切り出し、わずか数ミリ秒で描画します。

| 機能カテゴリ | 記法サンプル | 描画エンジン | 状態 |
| :--- | :---: | :---: | ---: |
| **GFM テーブル** | `\| A \| B \|` / `\| :--- \|` | Native C++ + WebView2 | ✅ 完了 |
| **コード＆数式保護** | `` `a \| b` `` / `snake_case` | 境界・エスケープ解析 | ⚡ 0.8ms |
| **GFM アラート** | `> [!NOTE]` / `[!TIP]` | ダーク／ライト自動連動 | 🎨 対応 |

- [x] **タスクリスト＆多段ネストリスト**
  - インデントに応じた `<ul>` / `<ol>` 自動階層化と <kbd>F5</kbd> リロード
- [ ] 次期アップデート: カスタム CSS テーマ拡張

```cpp
// Native C++20 Direct2D + WebView2 Split Preview
void Editor::togglePreview() {
    isPreviewOpen = !isPreviewOpen;
    updatePreviewContent(); // Zero temp-file overhead
}
```
