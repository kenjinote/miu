import os
import sys
import json
import time
import zipfile
import subprocess
import requests

DEFAULT_RELEASE_NOTES = {
    "1.0.39": {
        "ja-jp": """miu v1.0.39 リリースノート
📊 Mermaid 図表・グラフのリアルタイムプレビュー対応（Alt+P）

・Mermaid 図表（全33種）の高速ライブ SVG 描画（Pele エンジン連携）:
  Markdown 内の ```mermaid コードブロックや .mmd / .mermaid ファイル（および未保存タブの構文自動判定）に対応し、シーケンス図・フローチャート・XY グラフなど全33種の図表をリアルタイムに SVG 描画します。
・インプレース差分更新（<1ms）＆ ダーク／ライトモード完全連動:
  入力中のチラつきがない高速 DOM 差分更新と、ダーク／ライトテーマ自動連動に対応しました。
・.mmd / .mermaid 構文ハイライト ＆ ファイル関連付け対応:
  Mermaid 専用のシンタックスハイライトとファイル関連付けを追加しました。""",

        "en-us": """miu v1.0.39 Release Notes
📊 Instant Mermaid Diagram & Chart Live Preview (Alt+P)

- Real-Time Mermaid SVG Preview Across 33 Diagram Types (Powered by Pele):
  Renders sequence diagrams, flowcharts, XY charts, ER/class/state diagrams, mindmaps, and more inside ```mermaid fenced blocks, .mmd / .mermaid files, and auto-detected plain buffers.
- Flicker-Free In-Place Updates (<1ms) & Auto Dark/Light Theme Sync:
  Updates diagrams in-place as you type with zero page reloads and adapts automatically to Dark and Light modes.
- Mermaid (.mmd / .mermaid) Syntax Highlighting & File Associations:
  Added dedicated syntax highlighting for Mermaid keywords, arrows, and comments while keeping the ultra-lightweight single-EXE design.""",

        "zh-hans": """miu v1.0.39 更新日志
📊 新增 Mermaid 流程图与图表实时预览（Alt+P）

- 支持 33 种 Mermaid 图表极速 SVG 实时渲染（集成 Pele 引擎）：
  全面支持 Markdown 中的 ```mermaid 代码块、.mmd / .mermaid 文件及未保存标签页的语法自动识别，实时渲染时序图、流程图、XY 图表、思维导图等 33 种图表。
- 毫秒级无闪烁增量刷新（<1ms）与深浅色主题自动联动：
  输入时通过 DOM 增量更新即时重绘图表，自动适配深色与浅色模式配色。
- 新增 .mmd / .mermaid 语法高亮与文件关联支持。""",

        "ko": """miu v1.0.39 릴리스 노트
📊 Mermaid 다이어그램 및 차트 실시간 미리보기 지원 (Alt+P)

- 33종 Mermaid 다이어그램 초고속 SVG 실시간 렌더링 (Pele 엔진 연동):
  마크다운 내 ```mermaid 코드 블록, .mmd / .mermaid 파일 및 새 탭의 구문 자동 감지를 통해 시퀀스 다이어그램, 플로우차트, XY 차트 등 33종 다이어그램을 즉시 렌더링합니다.
- 깜빡임 없는 인플레이스 업데이트(<1ms) 및 다크/라이트 모드 자동 연동:
  입력 중 페이지 새로고침 없이 즉시 그래프를 갱신하며 다크/라이트 테마 배색에 자동 연동됩니다.
- .mmd / .mermaid 구문 강조 및 파일 연결 지원 추가."""
    },
    "1.0.38": {
        "ja-jp": """miu v1.0.38 リリースノート
🌐 WebView2 リアルタイムプレビュー（Alt+P）＆ Markdown (GFM) 忠実レンダリング対応

・WebView2 による HTML / SVG / Markdown リアルタイム分割プレビュー（Alt+P）:
  Alt+P でエディタ右側にライブプレビューを表示できるようになりました。編集に応じた自動更新、F5 リロード、スプリッターのドラッグによる幅調整、ローカル画像・CSS・JS の読み込み、ダーク／ライトモード連動、洗練されたオーバーレイ・スクロールバーに対応しています。
・CommonMark ＆ GitHub Flavored Markdown (GFM) 忠実プレビュー:
  テーブル（列揃え・パイプエスケープ）、タスクリスト（- [ ] / - [x]）、多段ネストリスト、複数行・ネスト引用、GFM アラート（[!NOTE] / [!TIP] / [!WARNING] 等）、複数バッククォート、snake_case の保護、オートリンク、<kbd> 等のインライン HTML タグに対応しました。
・巨大ファイルでの高速・安全プレビュー ＆ 検索（F3）折り返しの高速化:
  巨大ファイルでもディスクコピーなしで先頭 512KB を瞬時にプレビュー表示（全28言語の省略案内付き）し、F3 検索でファイル末尾から先頭へ折り返す際の動作を大幅に高速化しました。""",

        "en-us": """miu v1.0.38 Release Notes
🌐 WebView2 Live Preview (Alt+P) & Faithful CommonMark / GFM Markdown Rendering

- Built-in WebView2 Split Live Preview for HTML, SVG & Markdown (Alt+P):
  Press Alt+P to open a side-by-side live preview pane with automatic debounced updates, F5 reload, draggable subtle splitter, overlay scrollbars, Dark/Light theme adaptation, and local asset mapping (images, CSS, JS).
- Faithful CommonMark & GitHub Flavored Markdown (GFM) Rendering:
  Full support for GFM tables (alignment & escaped pipes), task lists (- [ ] / - [x]), nested lists, multi-line & nested blockquotes, GFM alerts ([!NOTE], [!TIP], [!IMPORTANT], [!WARNING], [!CAUTION]), multi-backtick code spans, snake_case protection, autolinks, and safe inline HTML (<kbd>, <br>, etc.).
- Huge File Preview Optimization & Instant Find (F3) Wrap-Around:
  Safely previews the first 512 KB on clean UTF-8/line boundaries with zero disk copies (localized across all 28 languages) and fixes a freeze when wrapping around the end of large files with F3.""",

        "zh-hans": """miu v1.0.38 更新日志
🌐 新增 WebView2 实时预览（Alt+P）与 CommonMark / GFM Markdown 精准渲染

- 内置 WebView2 分屏实时预览（按 Alt+P 开启）：
  支持 HTML、SVG 与 Markdown 的分屏实时预览，具备自动防抖刷新、F5 手动刷新、可拖拽极简分隔条、自适应浅色/深色主题、叠加式滚动条以及本地图片/CSS/JS 资源映射。
- 忠实还原 CommonMark 与 GitHub 风格 Markdown (GFM)：
  全面支持表格（对齐与转义竖线）、任务列表（- [ ] / - [x]）、多级嵌套列表、多行与嵌套引用、GFM 提示框（[!NOTE]、[!TIP]、[!WARNING] 等）、多重反引号代码、snake_case 下划线保护、自动链接及 <kbd> 等内联 HTML 标签。
- 超大文件极速安全预览与 F3 循环查找修复：
  无需任何磁盘拷贝即可按 UTF-8 与行边界极速预览前 512 KB 内容（附全 28 种语言截断提示），并修复了大文件末尾按 F3 折返查找时的卡顿问题。""",

        "ko": """miu v1.0.38 릴리스 노트
🌐 WebView2 실시간 미리보기(Alt+P) 및 CommonMark / GFM 마크다운 완벽 렌더링

- WebView2 기반 HTML / SVG / Markdown 분할 실시간 미리보기 (Alt+P):
  Alt+P를 눌러 편집기 오른쪽에 실시간 미리보기 창을 열 수 있습니다. 자동 업데이트, F5 새로고침, 드래그 가능한 분할선, 다크/라이트 모드 연동, 오버레이 스크롤바 및 로컬 이미지·CSS·JS 매핑을 지원합니다.
- CommonMark 및 GitHub Flavored Markdown (GFM) 충실한 렌더링:
  표(정렬 및 파이프 이스케이프), 작업 목록(- [ ] / - [x]), 다단 중첩 목록, 다중 행·중첩 인용구, GFM 알림([!NOTE], [!TIP], [!WARNING] 등), 다중 백틱 코드, snake_case 보호, 자동 링크 및 <kbd> 등 인라인 HTML 태그를 완벽 지원합니다.
- 대용량 파일 고속 미리보기 및 찾기(F3) 순환 검색 최적화:
  디스크 복사 없이 첫 512 KB를 UTF-8 및 줄 경계에 맞춰 즉시 미리보기로 표시하며(28개 언어 안내 지원), 파일 끝에서 F3으로 처음 위치로 순환 검색할 때의 멈춤 현상을 해결했습니다."""
    },
    "1.0.37": {
        "ja-jp": """miu v1.0.37 リリースノート
⚡ ネイティブ Emmet 完全対応（超高速展開・フィルター構文・全角対応・スマート改行）

・超高速ネイティブ Emmet エンジン搭載（Tab キーで即時展開）:
  HTML / JSX / XML / CSS の Emmet 略記展開に完全対応しました（1回あたり約 29.7 μs の超高速動作）。親子・兄弟・グループ・連番（$）・属性・テキスト（{...}）・暗黙タグ補完・BEM/コメント/エスケープフィルター（|bem, |c, |e）・CSS 数値/カラー/ベンダープレフィックス（-）展開に対応し、展開後は Tab / Shift+Tab で入力箇所を順番にジャンプできます。
・空タグ <div>|</div> 内のスマート Enter 自動インデント改行:
  開始タグと終了タグの間で Enter を押すと、自動的にインデント付きの3行ブロックへ展開します。
・全角文字（日本語・全角スペース・全角英数記号）＆ 未保存タブ対応:
  日本語テキストや全角スペースインデント、IME ON のまま入力された全角英数記号（ｄｉｖ＞ｐ＊２ 等）の自動正規化に対応し、保存前の新規タブや <style> / style="" 内でも自動で HTML/CSS を切り替えて動作します。""",

        "en-us": """miu v1.0.37 Release Notes
⚡ Full Native Emmet Support (Ultra-Fast Expansion, Filters, Smart Enter & Full-Width Support)

- Ultra-Fast Native C++ Emmet Engine (Expand with Tab):
  Full support for HTML, JSX, XML, and CSS Emmet abbreviations (~29.7 µs per expansion). Supports child/sibling/climb-up/grouping/multiplication operators, numbering ($), attributes, text nodes ({...}), implicit tags, BEM/comment/escape filters (|bem, |c, |e), CSS unitless/color/vendor-prefix (-) expansions, multi-cursor expansion, and Tab / Shift+Tab tabstop navigation.
- Smart Enter Auto-Indent Inside Empty Tags (<div>|</div>):
  Pressing Enter between matching opening and closing tags automatically splits and indents them into a 3-line block while preserving the file's newline format (CRLF/LF).
- Context-Aware Mode Switching & Full-Width Character Support:
  Works immediately in unsaved tabs and 45+ file types, automatically switches between HTML and CSS inside <style> blocks and style="" attributes, and supports CJK text and full-width alphanumeric normalization.""",

        "zh-hans": """miu v1.0.37 更新日志
⚡ 全面支持原生 Emmet（超高速展开、过滤器语法、全角兼容与智能回车缩进）

- 搭载超高速 C++ 原生 Emmet 引擎（按 Tab 键即时展开）：
  全面支持 HTML / JSX / XML / CSS 的 Emmet 缩写展开（单次展开仅需约 29.7 微秒）。支持层级/兄弟/分组/乘法运算、自动编号（$）、属性、文本节点（{...}）、隐式标签推导、过滤器（|bem、|c、|e）、CSS 数值/颜色/浏览器前缀（-）展开及多光标同步展开，展开后可按 Tab / Shift+Tab 在输入点间快速跳转。
- 空标签 <div>|</div> 内按 Enter 智能换行缩进：
  在成对起止标签之间按 Enter 键时，将自动拆分为带缩进的三行结构并保持文件原有换行符格式。
- 智能上下文切换与全角字符支持：
  支持未保存的新建标签页及 45+ 种文件扩展名，在 <style> 与 style="" 内自动切换 CSS 模式，并支持中文文本及全角字母符号（如 ｄｉｖ＞ｐ＊２）的自动归一化展开。""",

        "ko": """miu v1.0.37 릴리스 노트
⚡ 네이티브 Emmet 완벽 지원 (초고속 전개, 필터 구문, 전각 문자 호환 및 스마트 Enter 줄바꿈)

- 초고속 C++ 네이티브 Emmet 엔진 탑재 (Tab 키로 즉시 전개):
  HTML / JSX / XML / CSS Emmet 약어 전개를 완벽 지원합니다(1회당 약 29.7 μs 초고속 동작). 계층·형제·그룹·반복 연산자, 자동 번호 매기기($), 속성, 텍스트({…}), 암시적 태그 추론, 필터(|bem, |c, |e), CSS 수치·색상·벤더 프리픽스(-) 전개 및 다중 커서 전개를 지원하며, 전개 후 Tab / Shift+Tab으로 입력 위치를 순서대로 이동할 수 있습니다.
- 빈 태그 <div>|</div> 내 스마트 Enter 자동 들여쓰기:
  시작 태그와 종료 태그 사이에서 Enter를 누르면 파일의 줄바꿈 형식(CRLF/LF)을 유지하며 자동으로 3줄 들여쓰기 블록으로 분할됩니다.
- 자동 문맥 전환 및 전각 문자 지원:
  저장 전인 새 탭과 45종 이상의 확장자에서 즉시 작동하며, <style> 및 style="" 내부에서 자동으로 CSS 모드로 전환되고 전각 영숫자·기호 자동 정규화를 지원합니다."""
    },
    "1.0.36": {
        "ja-jp": """miu v1.0.36 リリースノート
🚀 バイナリサイズの大幅削減（-19.9%）＆ 実行速度のさらなる高速化（+15.6%）

・実行ファイルサイズを約 20% 削減（1,053.5 KB → 844.0 KB / -209.5 KB）:
  単一EXEのポータビリティと安全性を100%維持したまま、不要なC++標準ストリームの排除、全15言語の構文ハイライト定義テーブルのコンパイル時定数化（constexpr静的配置・起動時ヒープ確保ゼロ化）、リソース統合により、バイナリサイズを 214,528 bytes（-19.9%）削減しました。
・文字コード自動判定を約 15.6% 高速化 ＆ 25種以上のエンコーディング対応強化:
  文字コード判定エンジン（CED）の定数畳み込み最適化により、判定精度を一切犠牲にすることなく連続判定速度を約 15.6% 高速化（50,000回判定: 158.28 ms → 133.52 ms / 1ファイル約 2.67 μs）。さらに EUC-JP、ISO-2022-JP、BOMなしUTF-16、GB18030、KOI8-R など25種類以上の文字コード自動変換を強化しました。
・全28言語 × 高DPI（100%〜250%）動的オートレイアウト対応:
  検索・置換（Ctrl+F / Ctrl+H）および行移動（Ctrl+G）ダイアログで、翻訳文字列の実測幅とモニターDPIに応じてボタンやウィンドウ幅を自動調整し、文字見切れを完全に防止しました。""",

        "en-us": """miu v1.0.36 Release Notes
🚀 19.9% Smaller Binary Size & 15.6% Faster Encoding Detection

- 19.9% Binary Size Reduction (1,053.5 KB -> 844.0 KB / -209.5 KB):
  Reduced the standalone executable size by 214,528 bytes (-19.9%) while keeping 100% native single-EXE portability—achieved by eliminating C++ iostream/locale overhead, moving all 15 syntax highlighting tables to compile-time constexpr storage (zero startup heap allocations), lossless icon optimization, and unifying 28-language dialog resources.
- 15.6% Faster Character Encoding Auto-Detection & Expanded Codepage Support:
  Accelerated the Compact Encoding Detection (CED) engine by 15.6% (158.28 ms -> 133.52 ms across 50,000 detections; ~2.67 µs/file) via compile-time constant folding with zero loss in accuracy. Enhanced decoding support across 25+ encodings including EUC-JP, ISO-2022-JP, BOM-less UTF-16LE/BE, GB18030, KOI8-R, and European/Middle Eastern codepages.
- Dynamic High-DPI Auto-Layout Across All 28 Languages:
  Find/Replace (Ctrl+F / Ctrl+H) and Go to Line (Ctrl+G) dialogs now dynamically measure localized text and scale controls across 100%-250% DPI with zero text clipping.""",

        "zh-hans": """miu v1.0.36 更新日志
🚀 执行文件体积大幅缩减 19.9% & 编码自动识别提速 15.6%

- 程序体积缩减约 20%（1,053.5 KB → 844.0 KB / 减少 209.5 KB）：
  在 100% 保持原生单文件便携性与安全性的前提下，通过移除冗余 C++ iostream/locale 库开销、将 15 种语言的语法高亮表转为编译期 constexpr 静态常量（启动时零堆内存分配）、无损压缩图标及整合 28 国语言对话框模板，成功缩减 214,528 字节（-19.9%）。
- 字符编码自动识别提速 15.6% 并强化 25+ 种编码支持：
  通过编译期常量折叠优化 CED 编码识别引擎，在不降低任何识别精度的前提下将连续识别速度提升 15.6%（50,000 次识别由 158.28 ms 降至 133.52 ms，单文件仅需约 2.67 微秒），并全面强化 GB18030、Big5、EUC-JP、ISO-2022-JP、无 BOM UTF-16 及多国代码页映射。
- 全 28 种语言 × 高 DPI（100%~250%）动态自适应布局：
  查找/替换（Ctrl+F / Ctrl+H）与跳转到行（Ctrl+G）对话框现可根据当前语言文本宽度与显示器 DPI 动态调整按钮与窗口尺寸，彻底消除文字截断。""",

        "ko": """miu v1.0.36 릴리스 노트
🚀 실행 파일 크기 19.9% 대폭 축소 및 문자 인코딩 자동 감지 15.6% 고속화

- 실행 파일 크기 약 20% 축소 (1,053.5 KB → 844.0 KB / -209.5 KB):
  단일 EXE의 휴대성과 안전성을 100% 유지하면서 불필요한 C++ iostream/locale 오버헤드 제거, 15개 언어 구문 강조 테이블의 컴파일 타임 constexpr 상수화(시작 시 힙 할당 제로화), 리소스 통합을 통해 바이너리 크기를 214,528 바이트(-19.9%) 줄였습니다.
- 문자 인코딩 자동 감지 속도 15.6% 향상 및 25종 이상의 인코딩 지원 강화:
  인코딩 감지 엔진(CED)의 컴파일 타임 상수 최적화로 정확도 손실 없이 감지 속도를 약 15.6% 향상(50,000회 감지: 158.28 ms → 133.52 ms / 파일당 약 2.67 μs)시켰으며, EUC-KR, EUC-JP, ISO-2022-JP, BOM 없는 UTF-16, GB18030 등 25종 이상의 인코딩 변환을 강화했습니다.
- 28개 전 언어 × 고해상도 DPI(100%~250%) 동적 자동 레이아웃:
  찾기/바꾸기(Ctrl+F / Ctrl+H) 및 줄 이동(Ctrl+G) 대화 상자에서 번역 텍스트 길이와 모니터 DPI에 맞춰 버튼 및 창 크기를 자동으로 조절하여 글자 잘림을 완벽히 방지했습니다."""
    },
    "1.0.35": {
        "ja-jp": """miu v1.0.35 リリースノート
✨ 新機能・改善点

Ctrl + ダブルクリックによる追加選択（マルチ選択）に対応

・Ctrl + ダブルクリックでの単語追加選択:
  Ctrl キーを押しながらテキストをダブルクリック（またはトリプルクリック）した際、既存の選択範囲を残したまま新しい単語・行を追加選択できるようになりました。
・一括コピー＆同時入力編集:
  追加選択した複数の単語をそのまま一括コピー（Ctrl+C）したり、同時入力で一括書き換えしたりできます。
・単語末尾のダブルクリック判定改善:
  単語の最後の文字の右半分をダブルクリックした際にも、直後の空白ではなくその単語が確実に選択されるよう改善しました。""",

        "en-us": """miu v1.0.35 Release Notes
✨ What's New & Improvements

Additive Word Selection via Ctrl + Double-Click

- Ctrl + Double-Click Additive Selection:
  Holding Ctrl while double-clicking a word (or triple-clicking a line) now adds the newly selected word or line to your existing selections instead of replacing them.
- Multi-Selection Copy & Simultaneous Editing:
  Easily copy all selected words at once (Ctrl+C) or type to replace all selected words simultaneously.
- Refined Word Boundary Hit-Testing:
  Double-clicking the trailing half of a word's last character now reliably selects the word itself rather than the following space.""",

        "zh-hans": """miu v1.0.35 更新日志
✨ 新功能与改进

支持按住 Ctrl 双击进行追加选择（多选）

- Ctrl + 双击追加选择单词：
  按住 Ctrl 键双击单词（或三击整行）时，现在会保留已有选区并将新选中的单词或行追加到多选列表中。
- 批量复制与同步编辑：
  可直接对追加选中的多个单词进行批量复制（Ctrl+C）或同时输入替换。
- 优化单词末尾双击判定：
  双击单词最后一个字符的右半部分时，现在能精准选中该单词本身而非其后的空格。""",

        "ko": """miu v1.0.35 릴리스 노트
✨ 새로운 기능 및 개선 사항

Ctrl + 더블 클릭을 통한 단어 추가 선택(다중 선택) 지원

- Ctrl + 더블 클릭 추가 선택:
  Ctrl 키를 누른 상태에서 단어를 더블 클릭(또는 줄을 트리플 클릭)하면 기존 선택 영역을 유지한 채 새로운 단어·줄을 추가로 선택할 수 있습니다.
- 일괄 복사 및 동시 편집:
  추가 선택한 여러 단어를 한 번에 복사(Ctrl+C)하거나 동시에 입력하여 일괄 수정할 수 있습니다.
- 단어 끝부분 더블 클릭 판정 개선:
  단어의 마지막 글자 오른쪽 절반을 더블 클릭할 때도 뒤따르는 공백이 아닌 해당 단어가 정확히 선택되도록 개선했습니다."""
    },
    "1.0.34": {
        "ja-jp": """miu v1.0.34 リリースノート
✨ 新機能・改善点

タスクバーのジャンプリスト（最近開いたファイル履歴）連携の強化・常時最前面表示 (Alt+T)・コメント切替 (Ctrl+/)

・タスクバーのファイル履歴（ジャンプリスト）表示の改善:
  ストア版アプリにおいて、タスクバーアイコン右クリック時に最近開いたファイルの履歴（ジャンプリスト）が確実に表示・起動されるよう改善しました。また、主要なテキスト・コード拡張子のファイル関連付けを追加しました。
・常に最前面に表示 (Alt+T):
  Alt+T でウィンドウの最前面固定を切り替えられるようになりました（タイトルバー 📌 表示付き）。
・コメントアウト切替 (Ctrl+/):
  言語に応じた行コメント・ブロックコメントを最小インデント揃えで一括切り替えできるようになりました。
・カーソル位置・選択範囲・スクロール位置の自動記憶＆復元と読み込み高速化:
  最近開いたファイルの編集位置と選択状態を記憶・自動復元し、ファイル読み込みをさらに高速化しました。""",

        "en-us": """miu v1.0.34 Release Notes
✨ What's New & Improvements

Improved Taskbar Jump List (Recent Files), Always on Top (Alt+T) & Toggle Comment (Ctrl+/)

- Improved Taskbar Jump List & File Associations:
  Fixed and enhanced Taskbar right-click Jump List integration in the Microsoft Store edition so recently opened files reliably appear and launch across app updates, and registered common text/code file type associations.
- Always on Top (Alt+T):
  Toggle pinning the editor window on top of all other windows with Alt+T, complete with a 📌 indicator in the title bar.
- Toggle Comment (Ctrl+/):
  Comment or uncomment lines and selections with minimum-indent alignment across supported languages using Ctrl+/.
- Automatic Cursor, Selection & Scroll Restoration:
  Automatically remembers and restores your caret position, selections, and scroll offset when reopening recent files, alongside faster file loading.""",

        "zh-hans": """miu v1.0.34 更新日志
✨ 新功能与改进

任务栏跳转列表（最近打开的文件历史）优化、窗口置顶 (Alt+T) 与注释切换 (Ctrl+/)

- 任务栏跳转列表与文件关联优化：
  修复并增强了商店版应用在任务栏图标右键菜单中显示“最近打开的文件”历史记录的功能，并新增常见文本与代码文件扩展名关联。
- 窗口置顶快捷键 (Alt+T)：
  按 Alt+T 即可将编辑器窗口固定在最前端，置顶时标题栏会显示 📌 标记。
- 切换注释快捷键 (Ctrl+/)：
  支持根据文件语言按最小缩进对齐批量添加或取消行注释与块注释。
- 自动恢复光标位置、选区与滚动位置：
  重新打开最近文件时自动恢复上次的编辑位置与选区，并进一步提升了文件加载速度。""",

        "ko": """miu v1.0.34 릴리스 노트
✨ 새로운 기능 및 개선 사항

작업 표시줄 점프 목록(최근 파일 기록) 개선, 항상 위에 표시 (Alt+T) 및 주석 전환 (Ctrl+/)

- 작업 표시줄 점프 목록 및 파일 연결 개선:
  스토어 버전에서 작업 표시줄 아이콘 우클릭 시 최근에 연 파일 기록(점프 목록)이 정확히 표시되고 실행되도록 개선하였으며, 주요 텍스트 및 코드 확장자 연결을 추가했습니다.
- 항상 위에 표시 단축키 (Alt+T):
  Alt+T를 눌러 창을 항상 맨 위에 고정할 수 있으며, 활성화 시 제목 표시줄에 📌 아이콘이 표시됩니다.
- 주석 전환 단축키 (Ctrl+/):
  언어별 행 주석 및 블록 주석을 최소 들여쓰기 위치에 맞춰 일괄 전환할 수 있습니다.
- 커서 위치·선택 영역·스크롤 위치 자동 복원 및 로딩 고속화:
  최근 파일의 편집 위치와 선택 상태를 자동으로 복원하고 파일 로딩 속도를 높였습니다."""
    },
    "1.0.33": {
        "ja-jp": """miu v1.0.33 リリースノート
✨ 新機能・改善点

常に最前面に表示 (Alt+T)・コメント切替 (Ctrl+/)・カーソル位置と選択状態の自動復元＆読み込み高速化

・常に最前面に表示 (Alt+T):
  Alt+T でウィンドウの最前面固定を切り替えられるようになりました。固定中はタイトルバーに 📌 が表示されます。
・コメントアウト切替 (Ctrl+/):
  言語に応じた行コメント・ブロックコメントを最小インデント揃えで一括切り替えできるようになりました。
・カーソル位置・選択範囲・スクロール位置の自動記憶＆復元:
  最近開いたファイルの編集位置と選択状態を記憶し、次回開いた際に自動復元します（外部編集時の安全フォールバック対応）。
・ファイル読み込みの高速化＆ダークモード改行表示の改善:
  SSE2 SIMD スキャンと非同期処理により読み込みを高速化し、ダークモード時の改行記号の視認性を改善しました。""",

        "en-us": """miu v1.0.33 Release Notes
✨ What's New & Improvements

Always on Top (Alt+T), Toggle Comment (Ctrl+/), Cursor/Selection State Restoration & Faster File Loading

- Always on Top (Alt+T):
  Toggle pinning the editor window on top of all other windows with Alt+T, complete with a 📌 indicator in the title bar.
- Toggle Comment (Ctrl+/):
  Comment or uncomment lines and selections with minimum-indent alignment across supported languages using Ctrl+/.
- Automatic Cursor, Selection & Scroll Restoration:
  Automatically remembers and restores your caret position, selections (including multi-cursor), and scroll offset when reopening recent files, with safe fallback if edited externally.
- Faster File Loading & Dark Mode Newline Visibility:
  Accelerated file loading via SSE2 SIMD line scanning and background shell updates, plus subtly visible newline marks in Dark Mode.""",

        "zh-hans": """miu v1.0.33 更新日志
✨ 新功能与改进

新增窗口置顶 (Alt+T)、注释切换 (Ctrl+/)、光标与选区位置自动恢复及加载提速

- 窗口置顶快捷键 (Alt+T)：
  按 Alt+T 即可将编辑器窗口固定在最前端，置顶时标题栏会显示 📌 标记。
- 切换注释快捷键 (Ctrl+/)：
  支持根据文件语言按最小缩进对齐批量添加或取消行注释与块注释。
- 自动记忆并恢复光标位置、选区与滚动位置：
  重新打开最近文件时自动恢复上次的光标、选区及滚动位置（即使被外部程序修改也能安全回退）。
- 文件加载提速与深色模式换行符显示优化：
  通过 SSE2 SIMD 扫描与后台异步更新大幅提升文件打开速度，并优化了深色模式下换行符的柔和可视效果。""",

        "ko": """miu v1.0.33 릴리스 노트
✨ 새로운 기능 및 개선 사항

항상 위에 표시 (Alt+T), 주석 전환 (Ctrl+/), 커서·선택 영역 자동 복원 및 파일 로딩 속도 향상

- 항상 위에 표시 단축키 (Alt+T):
  Alt+T를 눌러 창을 항상 맨 위에 고정할 수 있으며, 활성화 시 제목 표시줄에 📌 아이콘이 표시됩니다.
- 주석 전환 단축키 (Ctrl+/):
  언어별 행 주석 및 블록 주석을 최소 들여쓰기 위치에 맞춰 일괄 전환할 수 있습니다.
- 커서 위치·선택 영역·스크롤 위치 자동 기억 및 복원:
  최근에 연 파일의 편집 위치와 선택 상태를 기억하여 다시 열 때 자동으로 복원합니다(외부 편집 시 안전 폴백 지원).
- 파일 읽기 고속화 및 다크 모드 줄바꿈 기호 가시성 개선:
  SSE2 SIMD 스캔 및 비동기 처리로 로딩 속도를 높이고 다크 모드에서 줄바꿈 표시가 은은하게 보이도록 개선했습니다."""
    },
    "1.0.32": {
        "ja-jp": """miu v1.0.32 リリースノート
✨ 新機能・改善点

右クリック・コンテキストメニューの追加（ライト／ダークモード・全28言語対応）

・右クリックメニューに対応:
  エディタ画面上の右クリック（および Shift+F10 / Apps キー）で「元に戻す / やり直し / 切り取り / コピー / 貼り付け / すべて選択」のポップアップメニューを表示できるようになりました。
・ライトモード・ダークモード配色連動:
  現在のテーマに合わせてメニューの背景色・文字色・ハイライト・グレーアウト表示が美しく切り替わります。
・スマートなカーソル移動と選択維持:
  未選択状態で右クリックした際はその位置にカーソルが移動し、選択範囲がある場合は選択を維持したままコピーや切り取りが可能です。
・世界28言語に完全対応:
  すべてのメニュー項目とショートカット表記を全28言語にローカライズしました。""",

        "en-us": """miu v1.0.32 Release Notes
✨ What's New & Improvements

Right-Click Context Menu Added (Light/Dark Mode & 28 Languages)

- Right-Click Context Menu:
  Right-clicking in the editor (or pressing Shift+F10 / Menu key) now opens a clean popup menu with Undo, Redo, Cut, Copy, Paste, and Select All.
- Seamless Light & Dark Mode Styling:
  Menu colors, hover highlights, separators, and disabled states automatically adapt to the active Light or Dark theme.
- Smart Cursor Positioning:
  Right-clicking without an active selection moves the caret to the clicked position for effortless pasting, while preserving existing selections for Cut/Copy.
- Localized in All 28 Languages:
  Full native translations and keyboard shortcut hints across all 28 supported languages.""",

        "zh-hans": """miu v1.0.32 更新日志
✨ 新功能与改进

新增右键上下文菜单（支持浅色/深色模式与全部 28 种语言）

- 新增右键快捷菜单：
  在编辑区点击鼠标右键（或按 Shift+F10 / 菜单键），即可弹出包含“撤销 / 重做 / 剪切 / 复制 / 粘贴 / 全选”的快捷菜单。
- 浅色与深色模式色彩联动：
  菜单背景、文字、悬停高亮及禁用状态均与当前浅色或深色主题完美匹配。
- 智能光标定位：
  未选中文本时右键点击会自动将光标移动至点击处以便直接粘贴；已选中文本时则保留选区以便剪切或复制。
- 全面支持 28 种语言：
  所有菜单项与快捷键提示均已针对全球 28 种语言完成本地化。""",

        "ko": """miu v1.0.32 릴리스 노트
✨ 새로운 기능 및 개선 사항

마우스 우클릭 컨텍스트 메뉴 추가 (라이트/다크 모드 및 28개 언어 완벽 지원)

- 우클릭 팝업 메뉴 지원:
  편집 화면에서 마우스 오른쪽 버튼을 클릭(또는 Shift+F10 / 메뉴 키)하면 '실행 취소 / 다시 실행 / 잘라내기 / 복사 / 붙여넣기 / 모두 선택' 메뉴가 표시됩니다.
- 라이트 및 다크 모드 배색 연동:
  현재 테마에 맞춰 메뉴 배경, 텍스트, 강조 표시 및 비활성 항목 색상이 자연스럽게 전환됩니다.
- 스마트 커서 이동 및 선택 유지:
  선택 영역이 없을 때 우클릭하면 해당 위치로 커서가 이동하여 바로 붙여넣기가 가능하며, 선택 영역이 있을 때는 선택을 유지합니다.
- 전 세계 28개 언어 현지화:
  모든 메뉴 항목과 단축키 안내가 28개 언어로 완벽히 번역되었습니다."""
    },
    "1.0.31": {
        "ja-jp": """miu v1.0.31 リリースノート
✨ 新機能・改善点

新たに11言語を追加し、世界28言語に完全対応

・世界28言語への対応拡大:
  チェコ語、スウェーデン語、タイ語、ヒンディー語、ルーマニア語、ハンガリー語、ギリシャ語、デンマーク語、フィンランド語、ノルウェー語、マレー語の計11言語をアプリ本体（メニュー・F1ヘルプ・ダイアログ）およびストアに完全追加しました。
・美しいDirect2D多言語レンダリング:
  タイ文字、デーヴァナーガリー文字（ヒンディー語）、ギリシャ文字、北欧・中東欧のダイアクリティカルマークもDirect2D/DirectWriteにより鮮明に描画されます。
・管理者権限が必要なファイルの保存（UAC昇格保存）:
  Cドライブ直下やシステム保護領域、読み取り専用ファイルの上書き保存に対応。""",

        "en-us": """miu v1.0.31 Release Notes
✨ What's New & Improvements

Expanded Global Support to 28 Languages (11 New Languages Added)

- 11 New Languages Added:
  Added full native UI localization (menus, F1 help, shortcuts, dialogs) and Store listings for Czech, Swedish, Thai, Hindi, Romanian, Hungarian, Greek, Danish, Finnish, Norwegian, and Malay.
- Crisp Direct2D Multilingual Typography:
  Thai script, Devanagari (Hindi), Greek, and Nordic/Central European diacriticals render with pristine clarity via Direct2D/DirectWrite hardware acceleration.
- Save Protected Files (UAC Elevation):
  Save files in protected folders (such as C:\\ root) or read-only files safely with administrator privileges.""",

        "zh-hans": """miu v1.0.31 更新日志
✨ 新功能与改进

新增 11 种语言支持，全面覆盖全球 28 种语言

- 多语言支持扩展至 28 种语言：
  新增对捷克语、瑞典语、泰语、印地语、罗马尼亚语、匈牙利语、希腊语、丹麦语、芬兰语、挪威语和马来语的完整应用内（菜单、F1 帮助、对话框）及商店页面本地化支持。
- 高清 Direct2D 多语言文字渲染：
  通过 Direct2D/DirectWrite 硬件加速，清晰锐利地呈现泰文、天城文（印地语）、希腊文及欧洲特殊字符。
- 管理员权限保存（UAC 提权保存）：
  支持以管理员权限安全保存位于 C 盘根目录或系统保护目录中的文件。""",

        "ko": """miu v1.0.31 릴리스 노트
✨ 새로운 기능 및 개선 사항

11개 신규 언어 추가 — 전 세계 28개 언어 완벽 지원

- 28개 언어로 다국어 지원 확대:
  체코어, 스웨덴어, 태국어, 힌디어, 루마니아어, 헝가리어, 그리스어, 덴마크어, 핀란드어, 노르웨이어, 말레이어 등 11개 언어의 앱 UI(메뉴, F1 도움말, 대화상자) 및 스토어 지원이 추가되었습니다.
- 선명한 Direct2D 다국어 렌더링:
  태국 문자, 데바나가리 문자(힌디어), 그리스 문자 및 유럽 악센트 기호가 Direct2D/DirectWrite 가속을 통해 선명하게 표시됩니다.
- 관리자 권한 파일 저장(UAC 승격):
  C 드라이브 루트 및 시스템 보호 폴더의 파일 덮어쓰기 저장을 안전하게 지원합니다."""
    },
    "1.0.30": {
        "ja-jp": """miu v1.0.30 リリースノート
✨ 新機能・改善点

ベトナム語・ウクライナ語・インドネシア語の完全対応

・多言語対応の拡大:
  急速に成長するIT開発者大国・新興市場であるベトナム語（vi-vn）、ウクライナ語（uk-ua）、インドネシア語（id-id）のUI翻訳（メニュー・F1ヘルプ・ダイアログ等）をアプリ本体およびストアに完全追加しました。
・美しいDirect2D描画:
  声調記号付きラテン文字（ベトナム語）やキリル文字（ウクライナ語）もDirect2D/DirectWriteにより美しく鮮明に描画されます。
・管理者権限が必要なファイルの保存（UAC昇格保存）:
  Cドライブ直下やシステム保護領域、読み取り専用ファイルの編集・上書き保存に対応。""",

        "en-us": """miu v1.0.30 Release Notes
✨ What's New & Improvements

Full Support for Vietnamese, Ukrainian & Indonesian

- Expanded Multilingual Support:
  Added complete native UI localization (menus, F1 help, shortcuts, dialogs) and Store listings for Vietnamese (vi-vn), Ukrainian (uk-ua), and Indonesian (id-id).
- Crisp Direct2D Typography:
  Tone-marked diacriticals (Vietnamese) and Cyrillic glyphs (Ukrainian) render with pristine clarity via Direct2D/DirectWrite hardware acceleration.
- Save Protected Files (UAC Elevation):
  Save files in protected folders (such as C:\\ root) or read-only files safely with administrator privileges.""",

        "zh-hans": """miu v1.0.30 更新日志
✨ 新功能与改进

全面支持越南语、乌克兰语与印度尼西亚语

- 多语言支持拓展：
  新增对越南语（vi-vn）、乌克兰语（uk-ua）与印度尼西亚语（id-id）的完整本地化支持（涵盖菜单、F1 帮助指南、对话框与商店页面）。
- 高清 Direct2D 文字渲染：
  通过 Direct2D/DirectWrite 硬件加速，清晰锐利地呈现越南语声调符号与乌克兰语西里尔字母。
- 管理员权限保存（UAC 提权保存）：
  支持以管理员权限保存位于 C 盘根目录、系统保护目录或具有只读属性的文件。""",

        "ko": """miu v1.0.30 릴리스 노트
✨ 새로운 기능 및 개선 사항

베트남어, 우크라이나어, 인도네시아어 완전 지원

- 다국어 지원 대폭 확대:
  베트남어(vi-vn), 우크라이나어(uk-ua), 인도네시아어(id-id)에 대한 앱 UI(메뉴, F1 도움말, 대화상자) 및 스토어 완벽 지원이 추가되었습니다.
- 선명한 Direct2D 텍스트 렌더링:
  베트남어 성조 기호 및 우크라이나어 키릴 문자가 Direct2D/DirectWrite 가속을 통해 선명하고 부드럽게 렌더링됩니다.
- 관리자 권한 파일 저장(UAC 승격):
  C 드라이브 루트, 시스템 보호 폴더 및 읽기 전용 속성의 파일 덮어쓰기 저장을 지원합니다."""
    },
    "1.0.29": {
        "ja-jp": """miu v1.0.29 リリースノート
✨ 新機能・改善点

管理者権限が必要なファイルの保存に対応（UAC昇格保存）

・保護されたファイルの上書き保存に対応:
  Cドライブ直下やシステム保護領域、アクセス権が制限されたファイル、読み取り専用ファイルを編集した際、管理者権限での上書き保存が可能になりました。
・安全な確認ダイアログとUAC連携:
  アクセス拒否を検知するとセキュリティシールド付きの確認ダイアログを表示し、ユーザーの許可を経てバックグラウンドで安全にファイルを更新します。
・編集中の内容を保護する安全設計:
  保存確認ダイアログで「いいえ」を選んだ場合やWindowsのUAC画面をキャンセルした場合でも、入力・編集中のテキストやUndo履歴は一切失われず、そのまま編集を継続できます。
・「名前を付けて保存」のパス処理改善:
  Cドライブ直下（C:\\）などのルートディレクトリを対象とした際の初期フォルダ指定を適正化しました。
・多言語対応:
  新ダイアログを日本語を含む全14言語に完全ローカライズしました。""",

        "en-us": """miu v1.0.29 Release Notes
✨ What's New & Improvements

Save Protected Files with Administrator Privileges (UAC Elevation)

- Save Protected & System Files:
  Files located in protected locations (such as C:\\ drive root, system folders, or read-only files) can now be saved safely via administrator elevation.
- Seamless UAC Confirmation:
  When access denial is detected, a security shield dialog prompts for confirmation, launching an elevated helper to update the file upon user approval.
- Edit Preservation Guarantee:
  If the elevation dialog is declined or the UAC prompt is cancelled, in-memory buffer edits, piece table state, and undo history are 100% preserved without any data loss.
- Improved 'Save As' Path Handling:
  Fixed initial directory handling when saving into root drive paths like C:\\.
- Full Multilingual Support:
  Localized prompt messages across all 14 supported languages.""",

        "zh-hans": """miu v1.0.29 更新日志
✨ 新功能与改进

支持以管理员权限保存受保护文件（UAC 提权保存）

- 保存受保护与系统文件：
  现在支持以管理员权限保存位于 C 盘根目录、系统保护目录或具有只读属性的文件。
- 无缝 UAC 提权确认：
  检测到访问被拒绝时，将弹出带有安全盾牌图标的提示对话框，在用户许可后通过后台提权助手安全完成文件保存。
- 绝不丢失编辑内容的保护机制：
  如果在提示框中选择“否”或取消了 Windows UAC 提权弹窗，内存中的文本编辑内容与撤销历史将被 100% 完整保留，绝不丢失任何修改。
- 优化“另存为”根目录路径：
  修复了在 C:\\ 等根目录下另存为时初始文件夹路径的设置问题。
- 多语言全面支持：
  新增提示已完整本地化支持包括中文在内的全部 14 种语言。""",

        "ko": """miu v1.0.29 릴리스 노트
✨ 새로운 기능 및 개선 사항

관리자 권한이 필요한 파일 저장 지원 (UAC 승격 저장)

- 보호된 파일 및 시스템 파일 저장 지원:
  C 드라이브 루트, 시스템 보호 폴더 및 읽기 전용 속성의 파일을 편집한 경우 관리자 권한으로 안전하게 덮어쓸 수 있습니다.
- 원활한 UAC 확인 대화상자:
  액세스 거부 감지 시 보안 쉴드 아이콘이 포함된 확인 창이 표시되며, 사용자 동의 시 백그라운드 관리자 프로세스를 통해 파일을 업데이트합니다.
- 편집 내용 보호 설계:
  저장 확인 창에서 '아니요'를 선택하거나 UAC 승격을 취소하더라도 메모리 상의 편집 내용과 실행 취소(Undo) 기록이 완벽하게 보존됩니다.
- '다른 이름으로 저장' 경로 처리 개선:
  C:\\ 등 루트 디렉터리에 저장할 때 초기 폴더 경로가 올바르게 지정되도록 수정했습니다.
- 다국어 지원:
  새로운 확인 메시지를 한국어를 포함한 14개 언어로 완전 현지화했습니다."""
    },
    "1.0.28": {
        "ja-jp": """miu v1.0.28 リリースノート
✨ 新機能・改善点

テキスト折り返しモード時のカーソル移動を大幅に改善

・右矢印（→）キーの挙動改善:
  折り返し行の行末へ進めた際、表示上の行末にカーソルが確実に表示され、続けて押下した際は次行の2文字目先頭へ直観的に移動するようになりました。
・HOME / END キーのスマートジャンプ:
  1回目のキー押下で表示行の行頭・行末へジャンプし、すでに表示行頭・行末にある状態での再押下で論理行頭・行末へジャンプする2段階移動に対応しました。
・上下（↑ / ↓）キー移動の正確化:
  表示行末にカーソルがある状態からの上下移動時、文字位置がズレることなく自然に直上・直下の行へ移動するよう調整しました。
・スクロール追従の強化:
  長大な折り返し行で Ctrl+END 等により行末へジャンプした際も、カーソルが可視領域に確実に収まるよう自動スクロールを強化しました。

F1 ヘルプ画面のダークモード表示を最適化

・ダークモード時の背景色をより目に優しい暗色半透明スタイルに調整し、テキストの視認性を向上させました。""",

        "en-us": """miu v1.0.28 Release Notes
✨ What's New & Improvements

Refined Cursor Navigation in Word Wrap Mode

- Enhanced Right Arrow (->) Navigation:
  When advancing across wrapped lines, the cursor is now clearly displayed at the visual line end before stepping forward to the next line.
- Smart Home / End Key Jumps:
  Pressing Home/End now first jumps to the visual line start/end. Pressing it again jumps to the logical line beginning/end.
- Accurate Up / Down Arrow Navigation:
  Moving up or down from visual line ends now transitions accurately to the corresponding position on adjacent visual lines.
- Improved Caret Scroll Tracking:
  Jumping to the line end (e.g. via Ctrl+End) on long wrapped lines reliably scrolls the caret into view.

F1 Help Overlay Dark Mode Polish

- Restored and optimized the elegant dark semi-transparent styling for the F1 help overlay in dark mode, ensuring superior readability.""",

        "zh-hans": """miu v1.0.28 更新日志
✨ 新功能与改进

自动换行模式下的光标导航体验全面优化

- 优化右方向键（->）光标移动：
  在换行边界处，光标现在会清晰显示在当前视觉行末，再次右移即可平滑过渡至下一行。
- 智能 Home / End 跳转：
  首次按下 Home/End 键跳转至当前屏幕视觉行的行首/行末，再次按下跳转至逻辑行的首尾。
- 修复上下方向键（↑ / ↓）移动：
  从视觉行末向上或向下移动时，光标能精准定位至相邻视觉行。
- 增强光标可视滚动追踪：
  在超长换行文本中按 Ctrl+End 跳转至行尾时，视图会自动准确滚动并保持光标可见。

F1 帮助界面深色模式显示优化

- 优化了深色模式下的半透明暗色背景与边框效果，文字对比度更高、阅读体验更舒适。""",

        "ko": """miu v1.0.28 릴리스 노트
✨ 새로운 기능 및 개선 사항

자동 줄 바꿈 모드의 커서 이동 대폭 개선

- 오른쪽 화살표(->) 이동 개선:
  줄 바꿈 경계에서 시각적 행 끝에 커서가 정확히 표시된 후 다음 줄로 자연스럽게 이동합니다.
- 스마트 Home / End 키 이동:
  첫 번째 누름 시 화면 표시 행의 시작/끝으로 이동하고, 다시 누르면 논리적 줄의 시작/끝으로 이동하는 2단계 이동을 지원합니다.
- 위/아래 화살표(↑ / ↓) 이동 정밀화:
  시각적 행 끝에서 위/아래 키를 누를 때 인접 행으로 자연스럽고 정확하게 이동하도록 개선했습니다.
- 스크롤 추적 강화:
  긴 줄 바꿈 텍스트에서 Ctrl+End 등으로 줄 끝으로 이동할 때 커서가 항상 화면에 보이도록 자동 스크롤을 개선했습니다.

F1 도움말 다크 모드 표시 개선

- 다크 모드에서 배경을 은은한 어두운 반투명 스타일로 최적화하여 가독性を 높였습니다."""
    },
    "1.0.27": {
        "ja-jp": """miu v1.0.27 リリースノート
✨ 新機能・改善点

各種ダイアログのダークモード対応

検索・置換・指定行へ移動ダイアログが、Windowsのダークモード設定に合わせて自動でダークテーマ表示されるようになりました。

TaskDialog（保存確認・メッセージ画面）の刷新

ファイル保存確認などのダイアログを刷新し、ダークモードに対応しました。
OSの標準テーマAPIからフォント情報を動的取得することで、Windows標準のダイアログと完全に一致する美しい文字配置とレイアウトを実現しています。

F1 ヘルプ画面のライトモード表示を改善

ライトモード時、白背景のエディタ上でも見やすい淡いグレー背景の半透明スタイルに改良し、黒テキストの高い可読性とモダンなデザインを両立させました。""",

        "en-us": """miu v1.0.27 Release Notes
✨ What's New & Improvements

Dark Mode Support for All Dialogs

Find, Replace, and Go to Line dialogs now seamlessly adapt to Windows Dark Mode settings.

Refined TaskDialog & Confirmation Prompts

Save confirmation and message dialogs have been redesigned with full Dark Mode support.
Font metrics and styles are dynamically retrieved from Windows theme APIs, achieving a pixel-perfect match with standard Windows dialogs.

Improved F1 Help in Light Mode

In Light Mode, the help overlay now features a refined semi-transparent soft gray background, ensuring optimal contrast and readability for black text against bright editor backgrounds.""",

        "zh-hans": """miu v1.0.27 更新日志
✨ 新功能与改进

各种对话框支持深色模式

查找、替换和跳转行号对话框现已全面适配 Windows 深色模式设置。

TaskDialog 与保存确认对话框优化

全新重构了文件保存确认等对话框，并完美支持深色模式。
通过系统原生主题 API 动态获取字体与度量信息，实现了与 Windows 原生对话框像素级一致的排版与布局。

F1 帮助菜单浅色模式优化

在浅色模式下，帮助菜单采用更易阅读的淡灰色半透明背景，确保在白色编辑器背景下黑色文字保持清晰易读与现代质感。""",

        "ko": """miu v1.0.27 릴리스 노트
✨ 새로운 기능 및 개선 사항

각종 대화상자 다크 모드 지원

찾기, 바꾸기, 지정 줄로 이동 대화상자가 Windows 다크 모드 설정에 맞춰 자동으로 어두운 테마로 표시됩니다.

TaskDialog(저장 확인 및 메시지 창) 개편

파일 저장 확인 등의 대화상자를 새롭게 개편하여 다크 모드를 완벽 지원합니다.
OS 표준 테마 API로부터 폰트 정보를 동적으로 가져와 Windows 표준 대화상자와 픽셀 단위로 정확하게 일치하는 레이아웃을 구현했습니다.

F1 도움말 라이트 모드 표시 개선

라이트 모드에서 흰색背景의 에디터 위에서도 가독성이 뛰어나도록 은은한 회색 반투명 스타일을 적용하여 선명한 텍스트와 현대적인 디자인을 제공합니다."""
    }
}

def get_store_credentials(root_dir):
    creds_file = os.path.join(root_dir, "scripts", "store_credentials.json")
    if os.path.exists(creds_file):
        with open(creds_file, "r", encoding="utf-8") as f:
            data = json.load(f)
            return (
                data.get("tenant_id"),
                data.get("client_id"),
                data.get("client_secret"),
                data.get("product_id")
            )
    return (
        os.environ.get("STORE_TENANT_ID"),
        os.environ.get("STORE_CLIENT_ID"),
        os.environ.get("STORE_CLIENT_SECRET"),
        os.environ.get("STORE_PRODUCT_ID")
    )

def build_store_package(root_dir, version, msbuild_path):
    print("=== Building Store Package (.appxbundle) ===")
    wapproj_path = os.path.join(root_dir, "package", "package.wapproj")
    cmd = [
        msbuild_path,
        wapproj_path,
        "/p:Configuration=Release",
        "/p:Platform=x64",
        "/p:AppxBundle=Always",
        "/p:UapAppxPackageBuildMode=StoreUpload"
    ]
    res = subprocess.run(cmd)
    if res.returncode != 0:
        raise RuntimeError(f"Building Store package failed with code {res.returncode}")

    bundle_dir = os.path.join(root_dir, "package", "AppPackages", f"package_{version}.0_Test")
    bundle_path = os.path.join(bundle_dir, f"package_{version}.0_x64.appxbundle")
    if not os.path.exists(bundle_path):
        candidates = [
            os.path.join(root_dir, "package", "AppPackages", f"package_{version}.0_x64.appxbundle"),
            os.path.join(bundle_dir, f"package_{version}.0_x64.msixbundle")
        ]
        for c in candidates:
            if os.path.exists(c):
                bundle_path = c
                break
    if not os.path.exists(bundle_path):
        raise FileNotFoundError(f"Store bundle not found at {bundle_path}")

    print(f"Store bundle built: {bundle_path} ({os.path.getsize(bundle_path)} bytes)")
    return bundle_path

def get_oauth_token(tenant_id, client_id, client_secret):
    print("Acquiring Microsoft Store API OAuth token...")
    token_url = f"https://login.microsoftonline.com/{tenant_id}/oauth2/token"
    data = {
        "grant_type": "client_credentials",
        "client_id": client_id,
        "client_secret": client_secret,
        "resource": "https://manage.devcenter.microsoft.com"
    }
    resp = requests.post(token_url, data=data)
    resp.raise_for_status()
    return resp.json()["access_token"]

def get_or_create_submission(headers, product_id):
    app_url = f"https://manage.devcenter.microsoft.com/v1.0/my/applications/{product_id}"
    app_resp = requests.get(app_url, headers=headers)
    app_resp.raise_for_status()
    app_data = app_resp.json()

    for _ in range(40):
        app_data = requests.get(app_url, headers=headers).json()
        pending = app_data.get("pendingApplicationSubmission")
        if not pending or not pending.get("id"):
            break
        sub_id = pending["id"]
        st_resp = requests.get(f"{app_url}/submissions/{sub_id}/status", headers=headers).json()
        st = st_resp.get("status", "")
        print(f"Existing pending submission {sub_id} status: {st}")
        if st == "CommitStarted":
            time.sleep(8)
            continue
        if st in ("PendingCommit", ""):
            sub_resp = requests.get(f"{app_url}/submissions/{sub_id}", headers=headers)
            sub_resp.raise_for_status()
            return sub_resp.json()
        # In PreProcessing / Certification / CommitFailed -> delete so we can create a fresh draft
        del_resp = requests.delete(f"{app_url}/submissions/{sub_id}", headers=headers)
        print(f"Deleted previous in-flight submission {sub_id}: {del_resp.status_code}")
        time.sleep(3)
        break

    print("Creating new draft submission...")
    create_url = f"{app_url}/submissions"
    create_resp = requests.post(create_url, headers=headers)
    create_resp.raise_for_status()
    new_sub = create_resp.json()
    print(f"Created new draft submission: {new_sub.get('id')}")
    return new_sub

def upload_and_update_submission(headers, product_id, submission_data, bundle_path, version, release_notes=None):
    sub_id = submission_data["id"]
    file_upload_url = submission_data["fileUploadUrl"]
    bundle_filename = os.path.basename(bundle_path)
    root_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

    # 1. Create zip package with bundle and all 30 screenshots
    zip_path = os.path.splitext(bundle_path)[0] + ".zip"
    print(f"Packaging bundle and screenshots into zip: {zip_path}...")
    screenshots_dir = os.path.join(root_dir, "package", "screenshots")

    with zipfile.ZipFile(zip_path, 'w', compression=zipfile.ZIP_DEFLATED) as zf:
        zf.write(bundle_path, arcname=bundle_filename)
        if os.path.exists(screenshots_dir):
            for img_file in sorted(os.listdir(screenshots_dir)):
                if img_file.endswith(".png"):
                    full_p = os.path.join(screenshots_dir, img_file)
                    zf.write(full_p, arcname=img_file)

    # 2. Upload zip to SAS URL
    print(f"Uploading zip ({os.path.getsize(zip_path)} bytes) to Store Blob Storage...")
    upload_url = file_upload_url.replace("+", "%2B")
    with open(zip_path, "rb") as f:
        up_resp = requests.put(
            upload_url,
            data=f,
            headers={"x-ms-blob-type": "BlockBlob"}
        )
    up_resp.raise_for_status()
    print("Zip package uploaded successfully.")

    # 3. Update applicationPackages
    existing_packages = submission_data.get("applicationPackages", [])
    pkg_filenames = [p.get("fileName") for p in existing_packages]

    if bundle_filename not in pkg_filenames:
        new_packages = []
        for pkg in existing_packages:
            if pkg.get("id"):  # Only existing packages with an ID can be marked PendingDelete
                pkg_copy = dict(pkg)
                pkg_copy["fileStatus"] = "PendingDelete"
                new_packages.append(pkg_copy)
        new_packages.append({
            "fileName": bundle_filename,
            "fileStatus": "PendingUpload",
            "minimumDirectXVersion": "None",
            "minimumSystemRam": "None"
        })
        submission_data["applicationPackages"] = new_packages

    # 4. Multi-language listings and screenshots
    listings = submission_data.setdefault("listings", {})

    try:
        from .store_metadata import METADATA_10_LANGS
    except ImportError:
        from store_metadata import METADATA_10_LANGS

    for lang, meta in METADATA_10_LANGS.items():
        lang_entry = listings.setdefault(lang, {})
        base = lang_entry.setdefault("baseListing", {})
        base["title"] = meta["title"]
        base["description"] = meta["description"]
        base["features"] = meta["features"]
        base["keywords"] = meta["keywords"]
        base["releaseNotes"] = meta["releaseNotes"]
        base["copyrightAndTrademarkInfo"] = "Copyright (C) 2026 kenjinote"
        base["supportContact"] = "https://github.com/kenjinote/miu"
        base["websiteUrl"] = "https://github.com/kenjinote/miu"
        base["privacyPolicy"] = "https://github.com/kenjinote/miu"
        base["licenseTerms"] = ""

        prefix = meta["prefix"]
        old_caps = meta.get("image_captions", ["", "", ""])
        caps5 = [
            old_caps[1] if len(old_caps) > 1 else "",
            old_caps[0] if len(old_caps) > 0 else "",
            "Multi-Cursor (Ctrl+D)",
            "Regex Find & Replace (Ctrl+H)",
            "HTML5 / JS Live Preview (Alt+P)",
            "Mermaid Diagrams & Charts Live Preview (Alt+P)"
        ]
        img_list = []
        for i in range(1, 7):
            fname = f"{prefix}_{i}.png"
            img_list.append({
                "fileName": fname,
                "fileStatus": "PendingUpload",
                "imageType": "Screenshot",
                "description": caps5[i-1]
            })
        base["images"] = img_list
        print(f"Configured listing and 6 screenshots for [{lang}]")

    base_4_prefixes = {
        "ja-jp": ("ja", [
            "ダークモード × Markdown リアルタイム分割プレビュー (Alt+P)",
            "ライトモード × SVG 機械図面 (CAD) ライブプレビュー",
            "ダークモード × マルチカーソル一括高速編集 (Ctrl+D)",
            "ライトモード × 正規表現を使った検索と一括置換 (Ctrl+H)",
            "ダークモード × HTML5 / JS ライブ実行 (Alt+P)",
            "ダークモード × Mermaid 図表・グラフ リアルタイムプレビュー (Alt+P)",
        ]),
        "en-us": ("en", [
            "Dark Mode × Markdown & GFM Live Split Preview (Alt+P)",
            "Light Mode × SVG Technical CAD Drawing Live Preview",
            "Dark Mode × Multi-Cursor Simultaneous Editing (Ctrl+D)",
            "Light Mode × Regex Find & Batch Replace (Ctrl+H)",
            "Dark Mode × HTML5 & JavaScript Live Execution (Alt+P)",
            "Dark Mode × Mermaid Diagrams & Charts Live Preview (Alt+P)",
        ]),
        "zh-hans": ("zh_cn", [
            "深色模式 × Markdown 与 GFM 实时分屏预览 (Alt+P)",
            "浅色模式 × SVG 工程制图实时预览",
            "深色模式 × 多光标同步高速编辑 (Ctrl+D)",
            "浅色模式 × 正则表达式查找与批量替换 (Ctrl+H)",
            "深色模式 × HTML5 / JS 编辑器内实时运行 (Alt+P)",
            "深色模式 × Mermaid 流程图与图表实时预览 (Alt+P)",
        ]),
        "ko": ("ko", [
            "다크 모드 × Markdown & GFM 실시간 분할 미리보기 (Alt+P)",
            "라이트 모드 × SVG 기계 도면 실시간 미리보기",
            "다크 모드 × 다중 커서 동시 고속 편집 (Ctrl+D)",
            "라이트 모드 × 정규식 검색 및 일괄 바꾸기 (Ctrl+H)",
            "다크 모드 × HTML5 / JS 편집기 내 실시간 실행 (Alt+P)",
            "다크 모드 × Mermaid 다이어그램 및 차트 실시간 미리보기 (Alt+P)",
        ]),
    }
    for lang, (prefix, caps5) in base_4_prefixes.items():
        if lang in listings and "baseListing" in listings[lang]:
            listings[lang]["baseListing"]["images"] = [
                {
                    "fileName": f"{prefix}_{i}.png",
                    "fileStatus": "PendingUpload",
                    "imageType": "Screenshot",
                    "description": caps5[i-1]
                }
                for i in range(1, 7)
            ]
            print(f"Configured 6 screenshots for [{lang}]")

    if "ko" in listings and "baseListing" in listings["ko"]:
        if not listings["ko"]["baseListing"].get("title"):
            listings["ko"]["baseListing"]["title"] = "miu"

    # Multi-language release notes for existing default languages
    notes_dict = release_notes
    if not notes_dict:
        notes_dict = DEFAULT_RELEASE_NOTES.get(version)
    if notes_dict:
        for lang, notes in notes_dict.items():
            if lang in listings and "baseListing" in listings[lang]:
                listings[lang]["baseListing"]["releaseNotes"] = notes
                print(f"Updated release notes for [{lang}]")

    # 5. PUT update to Dev Center API
    print(f"Updating submission {sub_id}...")
    put_url = f"https://manage.devcenter.microsoft.com/v1.0/my/applications/{product_id}/submissions/{sub_id}"
    put_resp = requests.put(put_url, headers=headers, json=submission_data)
    if put_resp.status_code != 200:
        raise RuntimeError(f"Failed to update submission: {put_resp.status_code} - {put_resp.text}")
    print("Submission updated successfully.")

def commit_submission(headers, product_id, submission_id):
    print(f"Committing submission {submission_id} to Microsoft Store...")
    commit_url = f"https://manage.devcenter.microsoft.com/v1.0/my/applications/{product_id}/submissions/{submission_id}/commit"
    res = requests.post(commit_url, headers=headers)
    if res.status_code not in (200, 202):
        raise RuntimeError(f"Failed to commit submission: {res.status_code} - {res.text}")
    print("Submission committed! It is now queued for certification.")

def poll_submission_status(headers, product_id, submission_id, max_attempts=15, interval=10):
    status_url = f"https://manage.devcenter.microsoft.com/v1.0/my/applications/{product_id}/submissions/{submission_id}/status"
    for i in range(max_attempts):
        time.sleep(interval)
        res = requests.get(status_url, headers=headers)
        if res.status_code == 200:
            status_data = res.json()
            st = status_data.get("status")
            print(f"Status check ({i+1}/{max_attempts}): {st}")
            if st not in ("CommitStarted", "PendingCommit"):
                return st
    return "CommitProcessing"

def release_to_store(root_dir, version, msbuild_path, skip_build=False, custom_notes=None, do_commit=True):
    tenant_id, client_id, client_secret, product_id = get_store_credentials(root_dir)
    if not all([tenant_id, client_id, client_secret, product_id]):
        print("[Notice] Microsoft Store credentials not found. Skipping Store release.")
        return False

    print(f"\n{'='*50}\nStarting Microsoft Store Release for v{version}\n{'='*50}")

    if not skip_build:
        bundle_path = build_store_package(root_dir, version, msbuild_path)
    else:
        bundle_path = os.path.join(root_dir, "package", "AppPackages", f"package_{version}.0_Test", f"package_{version}.0_x64.appxbundle")
        if not os.path.exists(bundle_path):
            raise FileNotFoundError(f"Bundle not found: {bundle_path}")

    token = get_oauth_token(tenant_id, client_id, client_secret)
    headers = {
        "Authorization": f"Bearer {token}",
        "Content-Type": "application/json"
    }

    sub_data = get_or_create_submission(headers, product_id)
    sub_id = sub_data["id"]

    upload_and_update_submission(headers, product_id, sub_data, bundle_path, version, custom_notes)

    if do_commit:
        commit_submission(headers, product_id, sub_id)
        final_status = poll_submission_status(headers, product_id, sub_id)
        print(f"Microsoft Store submission complete. Current status: {final_status}")
    else:
        print(f"Submission {sub_id} prepared as draft. Commit skipped (--no-store-commit).")

    return True

if __name__ == "__main__":
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    release_to_store(root, "1.0.27", "MSBuild.exe", skip_build=True, do_commit=False)
