# miu Server Diagnostic Log — Regex Find & Replace (Ctrl+H)
# Pattern:     (\d{4})/(\d{2})/(\d{2}) \[WARN\]
# Replacement: $1-$2-$3 [FIXED]

2026/10/21 [INFO]  CoreEngine: Direct2D renderer initialized (0.4ms)
2026/10/22 [WARN]  PreviewHost: Recomputing 512KB UTF-8 preview boundary
2026/10/23 [INFO]  SyntaxHighlighter: Loaded 28 language resource tables
2026/10/24 [WARN]  JumpListSync: Updated taskbar recent file jump list
2026/10/25 [WARN]  MarkdownParser: Normalized GFM table column alignment
2026/10/25 [INFO]  PieceTable: Zero-copy memory-mapped 1.2GB file ready
2026/10/26 [WARN]  RegexEngine: Capture group ($1-$3) batch replacement

[Configuration]
max_preview_bytes = 524288
debounce_interval = 150ms
backdrop_material = "TransientAcrylic"
