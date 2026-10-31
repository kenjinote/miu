# miu サーバー監視ログ — 正規表現による一括検索・置換 (Ctrl+H)
# 検索パターン: (\d{4})/(\d{2})/(\d{2}) \[WARN\]
# 置換フォーマット: $1-$2-$3 [FIXED]

2026/10/21 [INFO]  CoreEngine: Direct2D レンダラー初期化完了 (0.4ms)
2026/10/22 [WARN]  PreviewHost: キャッシュ上限 (512KB) を再計算しました
2026/10/23 [INFO]  SyntaxHighlighter: 28言語リソースをロードしました
2026/10/24 [WARN]  JumpListSync: タスクバー履歴を更新しました
2026/10/25 [WARN]  MarkdownParser: GFM テーブル列揃えを補正しました
2026/10/25 [INFO]  PieceTable: 1.2GB ファイルのゼロコピー読込完了
2026/10/26 [WARN]  RegexEngine: キャプチャグループ ($1-$3) 置換テスト

[設定パラメータ]
max_preview_bytes = 524288
debounce_interval = 150ms
backdrop_material = "TransientAcrylic"
