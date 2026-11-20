# -*- coding: utf-8 -*-
"""
28-Language Minimalist Store Screenshot Localization Data for miu.
Covers all 34 App Store Connect locales via ASC_LOCALE_TO_PREFIX.
"""

# Exact approved Japanese baseline + 27 native localized language packs
LANG_PACKS = {
    "ja": {
        "html_lang": "ja",
        "ipad_date": "11月20日(金)",
        "duo_mc_status": "3 個のカーソル同時編集中  ·  TypeScript  ·  UTF-8",
        "repl_val": "$1-$2-$3 [確認済]",
        "headlines": {
            "s1": (
                [("Markdownを", True), ("リアルタイム分割プレビュー", False)],
                [[("Markdownを", True)], [("リアルタイムプレビュー", False)]],
            ),
            "s2": (
                [("Mermaid図表も", True), ("書いた瞬間にグラフ化", False)],
                [[("Mermaid図表も", True)], [("書いた瞬間にグラフ化", False)]],
            ),
            "s3": (
                [("LaTeX数式も", True), ("オフラインで瞬時に描画", False)],
                [[("LaTeX数式も", True)], [("オフラインで瞬時に描画", False)]],
            ),
            "s4": (
                [("正規表現による", False), ("高速検索と一括置換", True)],
                [[("正規表現による", False)], [("高速検索と一括置換", True)]],
            ),
            "s5": (
                [("65言語ハイライトと", False), ("マルチカーソル編集", True)],
                [[("65言語ハイライトと", False)], [("マルチカーソル編集", True)]],
            ),
        },
        "s1": (
            "# プロジェクト設計ノート\n\n"
            "> [!TIP]\n"
            "> **高速ライブプレビュー**：編集と同時に差分のみを瞬時に描画します。\n\n"
            "| モジュール | 主な役割 | 状態 |\n"
            "| :--- | :--- | :---: |\n"
            "| CoreEngine | PieceTable 高速編集 | 完了 |\n"
            "| LivePreview | CommonMark / GFM 描画 | 完了 |\n"
            "| Math & Diagram | 数式・33種図表エンジン | 完了 |\n\n"
            "## リリース前チェックリスト\n"
            "- [x] ダーク / ライトモード自動連動\n"
            "- [x] オフライン環境での完結動作\n"
            "- [ ] パフォーマンス計測レポート出力"
        ),
        "s2": (
            "# リクエスト処理フロー\n\n"
            "```mermaid\n"
            "flowchart TD\n"
            "  Client[\"クライアント\"] --> Gateway{\"API ゲートウェイ\"}\n"
            "  Gateway -->|認証OK| Service[\"アプリサーバー\"]\n"
            "  Gateway -->|高速応答| Cache[(\"メモリキャッシュ\")]\n"
            "  Service --> DB[(\"プライマリ DB\")]\n"
            "  style Gateway fill:#e8f0fe,stroke:#0969da,stroke-width:2px\n"
            "  style Service fill:#e6f4ea,stroke:#1a7f37,stroke-width:2px\n"
            "```\n\n"
            "- シーケンス図・フローチャート・ER図など **全33種の図表** をオフラインで即時SVG描画"
        ),
        "s3": (
            "# 数理モデル・アルゴリズム覚書\n\n"
            "ガウス積分と正規分布の基本関係式：\n\n"
            "$$\\int_{-\\infty}^{\\infty} e^{-\\alpha x^2}\\,dx = \\sqrt{\\frac{\\pi}{\\alpha}}$$\n\n"
            "固有値方程式と2次元回転行列：\n\n"
            "$$\\mathbf{A}\\mathbf{v} = \\lambda\\mathbf{v}, \\quad \\mathbf{R}(\\theta) = \\begin{pmatrix} \\cos\\theta & -\\sin\\theta \\\\ \\sin\\theta & \\cos\\theta \\end{pmatrix}$$\n\n"
            "- インライン数式 $E = mc^2$ や $\\nabla \\cdot \\mathbf{B} = 0$ もネイティブ MathML で鮮明に表示"
        ),
        "s4": (
            "# サーバー監査ログ整形 (access_audit.log)\n"
            "2026/11/20 [INFO] gateway: 接続プール初期化完了 (workers=16)\n"
            "2026/11/20 [WARN] auth-api: トークン有効期限が残り5分です (uid=4821)\n"
            "2026/11/20 [INFO] storage: スナップショット保存完了 (12.4ms)\n"
            "2026/11/20 [WARN] sync-job: 再試行キューへ切り替えました (retry=1)\n"
            "2026/11/20 [INFO] metrics: レイテンシ p99=1.8ms / error=0.00%\n"
            "2026/11/20 [WARN] cache-db: メモリ使用率が閾値80%に到達 (node=02)\n"
            "2026/11/20 [INFO] gateway: ヘルスチェック正常終了 (status=200)\n"
            "2026/11/20 [WARN] worker-3: バッチ処理タイムアウト延長 (job=9104)\n"
            "2026/11/20 [INFO] router: ルーティングテーブル更新完了 (routes=48)\n"
            "2026/11/20 [INFO] backup: 増分バックアップ検証完了 (sha256=ok)\n"
            "2026/11/20 [WARN] rate-lim: APIリクエスト上限の85%に到達 (ip=10.0.4.12)\n"
            "2026/11/20 [INFO] session: セッションキャッシュ最適化完了 (evicted=0)\n"
            "2026/11/20 [WARN] queue-02: 優先ジョブの待機時間が増加中 (wait=42ms)\n"
            "2026/11/20 [INFO] tls-cert: 証明書チェーン検証正常 (days_left=84)\n"
            "2026/11/20 [INFO] indexer: 全文検索インデックス同期完了 (docs=12800)\n"
            "2026/11/20 [INFO] auditor: 整合性チェック完了 (verified=100%)"
        ),
        "s5_comment": "// テーマ連動レンダリング制御 (theme_engine.ts)",
    },
}


def _make_pack(
    html_lang: str,
    ipad_date: str,
    duo_mc_status: str,
    repl_tag: str,
    hl: dict,
    md: tuple,
    mm: tuple,
    math_t: tuple,
    log_hdr: str,
    ts_comment: str,
    log_msgs: tuple = None,
) -> dict:
    (
        md_h1,
        tip_bold,
        tip_rest,
        col_mod,
        col_role,
        col_stat,
        r1,
        r2,
        r3,
        done_w,
        md_h2,
        c1,
        c2,
        c3,
    ) = md
    s1_str = (
        f"# {md_h1}\n\n"
        f"> [!TIP]\n"
        f"> **{tip_bold}**: {tip_rest}\n\n"
        f"| {col_mod} | {col_role} | {col_stat} |\n"
        f"| :--- | :--- | :---: |\n"
        f"| CoreEngine | {r1} | {done_w} |\n"
        f"| LivePreview | {r2} | {done_w} |\n"
        f"| Math & Diagram | {r3} | {done_w} |\n\n"
        f"## {md_h2}\n"
        f"- [x] {c1}\n"
        f"- [x] {c2}\n"
        f"- [ ] {c3}"
    )

    mm_h1, n_cli, n_gw, e_ok, e_fast, n_srv, n_cache, n_db, mm_b1, mm_b_em, mm_b2 = mm
    s2_str = (
        f"# {mm_h1}\n\n"
        f"```mermaid\n"
        f"flowchart TD\n"
        f"  Client[\"{n_cli}\"] --> Gateway{{\"{n_gw}\"}}\n"
        f"  Gateway -->|{e_ok}| Service[\"{n_srv}\"]\n"
        f"  Gateway -->|{e_fast}| Cache[(\"{n_cache}\")]\n"
        f"  Service --> DB[(\"{n_db}\")]\n"
        f"  style Gateway fill:#e8f0fe,stroke:#0969da,stroke-width:2px\n"
        f"  style Service fill:#e6f4ea,stroke:#1a7f37,stroke-width:2px\n"
        f"```\n\n"
        f"- {mm_b1.strip()} **{mm_b_em.strip()}** {mm_b2.strip()}"
    )

    m_h1, m_p1, m_p2, m_b1, m_b2 = math_t
    s3_str = (
        f"# {m_h1}\n\n"
        f"{m_p1}\n\n"
        + "$$\\int_{-\\infty}^{\\infty} e^{-\\alpha x^2}\\,dx = \\sqrt{\\frac{\\pi}{\\alpha}}$$\n\n"
        + f"{m_p2}\n\n"
        + "$$\\mathbf{A}\\mathbf{v} = \\lambda\\mathbf{v}, \\quad \\mathbf{R}(\\theta) = \\begin{pmatrix} \\cos\\theta & -\\sin\\theta \\\\ \\sin\\theta & \\cos\\theta \\end{pmatrix}$$\n\n"
        + f"- {m_b1} $E = mc^2$ & $\\nabla \\cdot \\mathbf{{B}} = 0$ {m_b2}"
    )

    if log_msgs is None:
        log_msgs = (
            "Connection pool initialized (workers=16)",
            "Token expires in 5m (uid=4821)",
            "Snapshot saved cleanly (12.4ms)",
            "Switched to retry queue (retry=1)",
            "Latency p99=1.8ms / error=0.00%",
            "Memory usage reached 80% threshold (node=02)",
            "Health check completed (status=200)",
            "Batch timeout extended (job=9104)",
            "Routing table refreshed (routes=48)",
            "Incremental backup verified (sha256=ok)",
            "API rate limit at 85% capacity (ip=10.0.4.12)",
            "Session cache compacted (evicted=0)",
            "Priority queue wait increasing (wait=42ms)",
            "Certificate chain valid (days_left=84)",
            "Full-text index synced (docs=12800)",
            "Integrity check passed (verified=100%)",
        )

    s4_str = (
        f"# {log_hdr} (access_audit.log)\n"
        f"2026/11/20 [INFO] gateway: {log_msgs[0]}\n"
        f"2026/11/20 [WARN] auth-api: {log_msgs[1]}\n"
        f"2026/11/20 [INFO] storage: {log_msgs[2]}\n"
        f"2026/11/20 [WARN] sync-job: {log_msgs[3]}\n"
        f"2026/11/20 [INFO] metrics: {log_msgs[4]}\n"
        f"2026/11/20 [WARN] cache-db: {log_msgs[5]}\n"
        f"2026/11/20 [INFO] gateway: {log_msgs[6]}\n"
        f"2026/11/20 [WARN] worker-3: {log_msgs[7]}\n"
        f"2026/11/20 [INFO] router: {log_msgs[8]}\n"
        f"2026/11/20 [INFO] backup: {log_msgs[9]}\n"
        f"2026/11/20 [WARN] rate-lim: {log_msgs[10]}\n"
        f"2026/11/20 [INFO] session: {log_msgs[11]}\n"
        f"2026/11/20 [WARN] queue-02: {log_msgs[12]}\n"
        f"2026/11/20 [INFO] tls-cert: {log_msgs[13]}\n"
        f"2026/11/20 [INFO] indexer: {log_msgs[14]}\n"
        f"2026/11/20 [INFO] auditor: {log_msgs[15]}"
    )

    return {
        "html_lang": html_lang,
        "ipad_date": ipad_date,
        "duo_mc_status": duo_mc_status,
        "repl_val": f"$1-$2-$3 [{repl_tag}]",
        "headlines": hl,
        "s1": s1_str,
        "s2": s2_str,
        "s3": s3_str,
        "s4": s4_str,
        "s5_comment": f"// {ts_comment} (theme_engine.ts)",
    }


LANG_PACKS["en"] = _make_pack(
    "en", "Fri Nov 20", "3 cursors active  ·  TypeScript  ·  UTF-8", "FIXED",
    {
        "s1": ([("Live Split Preview", True), ("for Markdown", False)], [[("Live Split Preview", True)], [("for Markdown & GFM", False)]]),
        "s2": ([("Mermaid Diagrams", True), ("Rendered as You Type", False)], [[("Mermaid Diagrams", True)], [("Rendered as You Type", False)]]),
        "s3": ([("Crisp LaTeX Math", True), ("100% Offline", False)], [[("Crisp LaTeX Math", True)], [("Rendered 100% Offline", False)]]),
        "s4": ([("Fast Search & Replace", True), ("with Regex", False)], [[("Fast Search &", False)], [("Batch Regex Replace", True)]]),
        "s5": ([("65 Languages &", False), ("Multi-Cursor Editing", True)], [[("65 Languages &", False)], [("Multi-Cursor Editing", True)]]),
    },
    ("Project Design Notes", "Instant Live Preview", "Renders only changed blocks in milliseconds as you type.", "Module", "Primary Role", "Status", "PieceTable fast editing", "CommonMark / GFM", "Math & 33 diagram types", "Done", "Release Checklist", "Auto Dark / Light Mode sync", "100% offline local execution", "Export benchmark report"),
    ("Request Processing Flow", "Client", "API Gateway", "Auth OK", "Fast Hit", "App Server", "Memory Cache", "Primary DB", "Render", "33 diagram types", "offline as crisp SVGs"),
    ("Mathematical Model Notes", "Gaussian integral & normal distribution:", "Eigenvalue equation & 2D rotation matrix:", "Inline math", "rendered sharply via native MathML"),
    "Server Audit Log", "Theme-aware frame renderer",
)

LANG_PACKS["zh_cn"] = _make_pack(
    "zh-CN", "11月20日 周五", "3 个光标同步编辑中  ·  TypeScript  ·  UTF-8", "已确认",
    {
        "s1": ([("Markdown", True), ("实时分屏预览", False)], [[("Markdown", True)], [("实时分屏预览", False)]]),
        "s2": ([("Mermaid 图表", True), ("随写随看即刻成图", False)], [[("Mermaid 图表", True)], [("随写随看即刻成图", False)]]),
        "s3": ([("LaTeX 数学公式", True), ("完全离线瞬时渲染", False)], [[("LaTeX 数学公式", True)], [("完全离线瞬时渲染", False)]]),
        "s4": ([("正则表达式", False), ("高速查找与批量替换", True)], [[("正则表达式", False)], [("高速查找与批量替换", True)]]),
        "s5": ([("65 种语言高亮与", False), ("多光标同步编辑", True)], [[("65 种语言高亮与", False)], [("多光标同步编辑", True)]]),
    },
    ("项目设计笔记", "高速实时预览", "输入同时仅对变更区块进行毫秒级渲染。", "模块", "主要职责", "状态", "PieceTable 高速编辑", "CommonMark / GFM 渲染", "公式与 33 种图表引擎", "完成", "发布前检查清单", "深色 / 浅色模式自动联动", "完全离线本地运行", "导出性能基准报告"),
    ("请求处理流程", "客户端", "API 网关", "认证通过", "高速命中", "应用服务器", "内存缓存", "主数据库", "离线即时渲染时序图、流程图等", "全部 33 种图表", "高清矢量 SVG"),
    ("数理模型与算法笔记", "高斯积分与正态分布基本关系式：", "特征值方程与二维旋转矩阵：", "行内公式", "均由原生 MathML 引擎清晰呈现"),
    "服务器审计日志整理", "主题联动渲染控制",
    (
        "连接池初始化完成 (workers=16)",
        "令牌有效期仅剩 5 分钟 (uid=4821)",
        "快照保存完成 (12.4ms)",
        "已切换至重试队列 (retry=1)",
        "延迟 p99=1.8ms / error=0.00%",
        "内存使用率达到 80% 阈值 (node=02)",
        "健康检查正常结束 (status=200)",
        "批处理超时时间已延长 (job=9104)",
        "路由表更新完成 (routes=48)",
        "增量备份校验通过 (sha256=ok)",
        "API 请求达到上限 85% (ip=10.0.4.12)",
        "会话缓存压缩完成 (evicted=0)",
        "优先队列等待时间增加 (wait=42ms)",
        "证书链校验正常 (days_left=84)",
        "全文索引同步完成 (docs=12800)",
        "完整性检查通过 (verified=100%)",
    ),
)

LANG_PACKS["zh_tw"] = _make_pack(
    "zh-TW", "11月20日 週五", "3 個游標同步編輯中  ·  TypeScript  ·  UTF-8", "已確認",
    {
        "s1": ([("Markdown", True), ("即時分割預覽", False)], [[("Markdown", True)], [("即時分割預覽", False)]]),
        "s2": ([("Mermaid 圖表", True), ("隨寫隨看瞬間成圖", False)], [[("Mermaid 圖表", True)], [("隨寫隨看瞬間成圖", False)]]),
        "s3": ([("LaTeX 數學公式", True), ("完全離線瞬間渲染", False)], [[("LaTeX 數學公式", True)], [("完全離線瞬間渲染", False)]]),
        "s4": ([("正規表示式", False), ("高速尋找與批次取代", True)], [[("正規表示式", False)], [("高速尋找與批次取代", True)]]),
        "s5": ([("65 種語法突顯與", False), ("多游標同步編輯", True)], [[("65 種語法突顯與", False)], [("多游標同步編輯", True)]]),
    },
    ("專案設計筆記", "高速即時預覽", "編輯同時僅針對變更區塊進行毫秒級渲染。", "模組", "主要職責", "狀態", "PieceTable 高速編輯", "CommonMark / GFM 渲染", "公式與 33 種圖表引擎", "完成", "發佈前檢查清單", "深色 / 淺色模式自動連動", "完全離線本機運作", "輸出效能基準報告"),
    ("請求處理流程", "用戶端", "API 閘道器", "驗證通過", "高速快取", "應用伺服器", "記憶體快取", "主資料庫", "離線即時繪製循序圖、流程圖等", "全部 33 種圖表", "高畫質 SVG"),
    ("數理模型與演算法筆記", "高斯積分與常態分佈基本關係式：", "特徵值方程式與二維旋轉矩陣：", "行內公式", "皆由原生 MathML 引擎清晰呈現"),
    "伺服器稽核紀錄整理", "主題連動渲染控制",
    (
        "連線池初始化完成 (workers=16)",
        "權杖有效期限剩餘 5 分鐘 (uid=4821)",
        "快照儲存完成 (12.4ms)",
        "已切換至重試佇列 (retry=1)",
        "延遲 p99=1.8ms / error=0.00%",
        "記憶體使用率達 80% 門檻 (node=02)",
        "健康檢查正常結束 (status=200)",
        "批次處理逾時已延長 (job=9104)",
        "路由表更新完成 (routes=48)",
        "增量備份驗證通過 (sha256=ok)",
        "API 請求達上限 85% (ip=10.0.4.12)",
        "工作階段快取最佳化完成 (evicted=0)",
        "優先佇列等待時間增加 (wait=42ms)",
        "憑證鏈驗證正常 (days_left=84)",
        "全文索引同步完成 (docs=12800)",
        "完整性檢查通過 (verified=100%)",
    ),
)

LANG_PACKS["ko"] = _make_pack(
    "ko", "11월 20일 (금)", "커서 3개 동시 편집 중  ·  TypeScript  ·  UTF-8", "확인됨",
    {
        "s1": ([("Markdown", True), ("실시간 분할 미리보기", False)], [[("Markdown", True)], [("실시간 분할 미리보기", False)]]),
        "s2": ([("Mermaid 다이어그램도", True), ("작성 즉시 그래프화", False)], [[("Mermaid 다이어그램", True)], [("작성 즉시 그래프화", False)]]),
        "s3": ([("LaTeX 수식도", True), ("오프라인에서 즉시 렌더링", False)], [[("LaTeX 수식도", True)], [("오프라인 즉시 렌더링", False)]]),
        "s4": ([("정규식 기반의", False), ("빠른 검색과 일괄 바꾸기", True)], [[("정규식 기반의", False)], [("빠른 검색과 일괄 바꾸기", True)]]),
        "s5": ([("65개 언어 강조와", False), ("다중 커서 편집", True)], [[("65개 언어 강조와", False)], [("다중 커서 동시 편집", True)]]),
    },
    ("프로젝트 설계 노트", "고속 라이브 미리보기", "편집과 동시에 변경된 영역만 즉시 렌더링합니다.", "모듈", "주요 역할", "상태", "PieceTable 고속 편집", "CommonMark / GFM 렌더링", "수식 · 33종 다이어그램 엔진", "완료", "출시 전 체크리스트", "다크 / 라이트 모드 자동 연동", "오프라인 환경 완벽 동작", "성능 측정 리포트 출력"),
    ("요청 처리 흐름", "클라이언트", "API 게이트웨이", "인증 OK", "고속 응답", "앱 서버", "메모리 캐시", "프라이머리 DB", "시퀀스·플로우차트 등", "총 33종 다이어그램", "오프라인 즉시 SVG 렌더링"),
    ("수리 모델 및 알고리즘 노트", "가우스 적분과 정규분포의 기본 관계식:", "고유값 방정식과 2차원 회전 행렬:", "인라인 수식", "네이티브 MathML로 선명하게 표시"),
    "서버 감사 로그 정리", "테마 연동 렌더링 제어",
    (
        "연결 풀 초기화 완료 (workers=16)",
        "토큰 만료 5분 전입니다 (uid=4821)",
        "스냅샷 저장 완료 (12.4ms)",
        "재시도 큐로 전환했습니다 (retry=1)",
        "지연 시간 p99=1.8ms / error=0.00%",
        "메모리 사용률 80% 임계치 도달 (node=02)",
        "상태 확인 정상 종료 (status=200)",
        "배치 처리 타임아웃 연장 (job=9104)",
        "라우팅 테이블 갱신 완료 (routes=48)",
        "증분 백업 검증 완료 (sha256=ok)",
        "API 요청 한도 85% 도달 (ip=10.0.4.12)",
        "세션 캐시 최적화 완료 (evicted=0)",
        "우선 작업 대기 시간 증가 중 (wait=42ms)",
        "인증서 체인 검증 정상 (days_left=84)",
        "전체 텍스트 인덱스 동기화 완료 (docs=12800)",
        "무결성 검사 완료 (verified=100%)",
    ),
)

LANG_PACKS["de"] = _make_pack(
    "de", "Fr. 20. Nov.", "3 Cursor aktiv  ·  TypeScript  ·  UTF-8", "FIXED",
    {
        "s1": ([("Markdown", True), ("in Echtzeit-Teilvorschau", False)], [[("Markdown live", True)], [("in Echtzeit-Vorschau", False)]]),
        "s2": ([("Mermaid-Diagramme", True), ("sofort beim Tippen", False)], [[("Mermaid-Diagramme", True)], [("sofort beim Tippen", False)]]),
        "s3": ([("LaTeX-Formeln", True), ("100 % offline gerendert", False)], [[("LaTeX-Formeln", True)], [("100 % offline gerendert", False)]]),
        "s4": ([("Schnelle Suche &", False), ("Regex- Stapelersetzung", True)], [[("Schnelle Suche &", False)], [("Regex-Ersetzung", True)]]),
        "s5": ([("65 Sprachen &", False), ("Multi-Cursor-Editor", True)], [[("65 Sprachen &", False)], [("Multi-Cursor-Editor", True)]]),
    },
    ("Projekt-Designnotizen", "Schnelle Live-Vorschau", "Aktualisiert beim Tippen sofort nur geänderte Blöcke.", "Modul", "Aufgabe", "Status", "PieceTable-Editor", "CommonMark / GFM", "Mathe & 33 Diagramme", "Fertig", "Release-Checkliste", "Dark / Light Mode automatisch", "100 % offline auf dem Gerät", "Benchmark-Bericht exportieren"),
    ("Anfrageverarbeitung", "Client", "API-Gateway", "Auth OK", "Cache-Hit", "App-Server", "Speicher-Cache", "Primäre DB", "Alle", "33 Diagrammtypen", "sofort offline als scharfe SVGs"),
    ("Mathematische Notizen", "Gaußsches Integral & Normalverteilung:", "Eigenwertgleichung & 2D-Rotationsmatrix:", "Inline-Formeln", "gestochen scharf per nativem MathML"),
    "Server-Audit-Log", "Themenabhängige Bildsteuerung",
)
# Fix minor space in German s4 landscape
LANG_PACKS["de"]["headlines"]["s4"] = (
    [("Schnelle Suche &", False), ("Regex-Ersetzung", True)],
    [[("Schnelle Suche &", False)], [("Regex-Ersetzung", True)]],
)

LANG_PACKS["fr"] = _make_pack(
    "fr", "Ven. 20 nov.", "3 curseurs actifs  ·  TypeScript  ·  UTF-8", "FIXED",
    {
        "s1": ([("Aperçu en direct", True), ("pour Markdown", False)], [[("Aperçu en direct", True)], [("pour Markdown", False)]]),
        "s2": ([("Diagrammes Mermaid", True), ("tracés en temps réel", False)], [[("Diagrammes Mermaid", True)], [("tracés en temps réel", False)]]),
        "s3": ([("Formules LaTeX", True), ("rendues 100 % hors ligne", False)], [[("Formules LaTeX", True)], [("100 % hors ligne", False)]]),
        "s4": ([("Recherche rapide &", False), ("remplacement Regex", True)], [[("Recherche rapide &", False)], [("remplacement Regex", True)]]),
        "s5": ([("65 langages &", False), ("édition multi-curseurs", True)], [[("65 langages &", False)], [("édition multi-curseurs", True)]]),
    },
    ("Notes d'architecture", "Aperçu instantané", "Ne recalcule que les blocs modifiés à la volée.", "Module", "Rôle principal", "État", "Édition rapide PieceTable", "CommonMark / GFM", "Maths & 33 diagrammes", "Prêt", "Liste de vérification", "Mode sombre / clair auto", "Fonctionnement 100 % hors ligne", "Exporter le rapport perf"),
    ("Flux de traitement", "Client", "Passerelle API", "Auth OK", "Cache", "Serveur App", "Cache mémoire", "Base primaire", "Rendu hors ligne de", "33 types de diagrammes", "en SVG net"),
    ("Notes mathématiques", "Intégrale de Gauss et loi normale :", "Valeurs propres et matrice de rotation 2D :", "Formules en ligne", "rendues avec précision via MathML natif"),
    "Journal d'audit serveur", "Rendu synchronisé au thème",
)

LANG_PACKS["es"] = _make_pack(
    "es", "Vie 20 nov", "3 cursores activos  ·  TypeScript  ·  UTF-8", "FIXED",
    {
        "s1": ([("Vista previa en vivo", True), ("para Markdown", False)], [[("Vista previa en vivo", True)], [("para Markdown", False)]]),
        "s2": ([("Diagramas Mermaid", True), ("al instante al escribir", False)], [[("Diagramas Mermaid", True)], [("al instante al escribir", False)]]),
        "s3": ([("Fórmulas LaTeX", True), ("100 % sin conexión", False)], [[("Fórmulas LaTeX", True)], [("100 % sin conexión", False)]]),
        "s4": ([("Búsqueda rápida y", False), ("reemplazo con Regex", True)], [[("Búsqueda rápida y", False)], [("reemplazo con Regex", True)]]),
        "s5": ([("65 lenguajes y", False), ("edición multicursor", True)], [[("65 lenguajes y", False)], [("edición multicursor", True)]]),
    },
    ("Notas de diseño", "Vista previa veloz", "Renderiza solo los cambios al instante mientras escribes.", "Módulo", "Función", "Estado", "Edición PieceTable", "CommonMark / GFM", "Fórmulas y 33 diagramas", "Listo", "Lista de lanzamiento", "Modo oscuro / claro automático", "Ejecución 100 % sin conexión", "Exportar informe de rendimiento"),
    ("Flujo de solicitudes", "Cliente", "API Gateway", "Auth OK", "Rápido", "Servidor App", "Caché RAM", "BD Primaria", "Renderiza", "33 tipos de diagramas", "en SVG nítido sin conexión"),
    ("Notas de modelos matemáticos", "Integral de Gauss y distribución normal:", "Ecuación de autovalores y rotación 2D:", "Fórmulas en línea", "nítidas mediante MathML nativo"),
    "Registro de auditoría", "Control de renderizado por tema",
)

LANG_PACKS["pt_br"] = _make_pack(
    "pt-BR", "Sex, 20 nov", "3 cursores ativos  ·  TypeScript  ·  UTF-8", "FIXED",
    {
        "s1": ([("Pré-visualização viva", True), ("para Markdown", False)], [[("Pré-visualização", True)], [("ao vivo em Markdown", False)]]),
        "s2": ([("Diagramas Mermaid", True), ("em tempo real", False)], [[("Diagramas Mermaid", True)], [("gerados em tempo real", False)]]),
        "s3": ([("Fórmulas LaTeX", True), ("100% offline na hora", False)], [[("Fórmulas LaTeX", True)], [("100% offline na hora", False)]]),
        "s4": ([("Busca rápida e", False), ("substituição Regex", True)], [[("Busca rápida e", False)], [("substituição Regex", True)]]),
        "s5": ([("65 linguagens e", False), ("edição multicursor", True)], [[("65 linguagens e", False)], [("edição multicursor", True)]]),
    },
    ("Notas do Projeto", "Prévia Instantânea", "Renderiza apenas blocos alterados em milissegundos.", "Módulo", "Função", "Status", "Edição PieceTable", "CommonMark / GFM", "Matemática e 33 diagramas", "Pronto", "Checklist de Release", "Modo Escuro / Claro automático", "Execução 100% offline", "Exportar relatório de performance"),
    ("Fluxo de Requisição", "Cliente", "API Gateway", "Auth OK", "Cache", "Servidor App", "Cache RAM", "BD Primário", "Renderize", "33 tipos de diagramas", "offline em SVG nítido"),
    ("Notas de Matemática", "Integral de Gauss e distribuição normal:", "Autovalores e matriz de rotação 2D:", "Equações inline", "nítidas com motor MathML nativo"),
    "Log de auditoria", "Renderização sincronizada ao tema",
)

LANG_PACKS["ru"] = _make_pack(
    "ru", "Пт, 20 нояб.", "Активно курсоров: 3  ·  TypeScript  ·  UTF-8", "FIXED",
    {
        "s1": ([("Живой предпросмотр", True), ("для Markdown", False)], [[("Живой предпросмотр", True)], [("для Markdown", False)]]),
        "s2": ([("Диаграммы Mermaid", True), ("сразу при вводе", False)], [[("Диаграммы Mermaid", True)], [("сразу при вводе", False)]]),
        "s3": ([("Формулы LaTeX", True), ("мгновенно и офлайн", False)], [[("Формулы LaTeX", True)], [("100% офлайн", False)]]),
        "s4": ([("Быстрый поиск и", False), ("замена через Regex", True)], [[("Быстрый поиск и", False)], [("замена через Regex", True)]]),
        "s5": ([("65 языков кода и", False), ("мультикурсор", True)], [[("65 языков кода и", False)], [("мультикурсор", True)]]),
    },
    ("Заметки по проекту", "Мгновенный просмотр", "Обновляет только изменённые блоки во время набора.", "Модуль", "Назначение", "Статус", "Редактор PieceTable", "CommonMark / GFM", "Формулы и 33 диаграммы", "Готово", "Чек-лист релиза", "Автопереключение тёмной темы", "Полностью офлайн-работа", "Экспорт отчёта скорости"),
    ("Обработка запросов", "Клиент", "API-шлюз", "Вход ОК", "Кэш", "Сервер", "Кэш ОЗУ", "Основная БД", "Мгновенный офлайн-рендеринг", "33 типов диаграмм", "в чёткий SVG"),
    ("Математические заметки", "Интеграл Гаусса и нормальное распределение:", "Собственные значения и матрица поворота 2D:", "Строчные формулы", "чётко отображаются через нативный MathML"),
    "Журнал аудита сервера", "Рендеринг с учётом темы",
)

LANG_PACKS["it"] = _make_pack(
    "it", "Ven 20 nov", "3 cursori attivi  ·  TypeScript  ·  UTF-8", "FIXED",
    {
        "s1": ([("Anteprima live", True), ("per Markdown", False)], [[("Anteprima live", True)], [("per Markdown", False)]]),
        "s2": ([("Diagrammi Mermaid", True), ("mentre scrivi", False)], [[("Diagrammi Mermaid", True)], [("disegnati mentre scrivi", False)]]),
        "s3": ([("Formule LaTeX", True), ("100% offline all'istante", False)], [[("Formule LaTeX", True)], [("100% offline all'istante", False)]]),
        "s4": ([("Ricerca rapida e", False), ("sostituzione Regex", True)], [[("Ricerca rapida e", False)], [("sostituzione Regex", True)]]),
        "s5": ([("65 linguaggi e", False), ("modifica multi-cursore", True)], [[("65 linguaggi e", False)], [("modifica multi-cursore", True)]]),
    },
    ("Note di Progetto", "Anteprima Istantanea", "Aggiorna solo i blocchi modificati mentre scrivi.", "Modulo", "Ruolo", "Stato", "Editing PieceTable", "CommonMark / GFM", "Formule e 33 diagrammi", "Fatto", "Checklist Rilascio", "Tema Scuro / Chiaro automatico", "Funzionamento 100% offline", "Esporta report prestazioni"),
    ("Flusso Richieste", "Client", "API Gateway", "Auth OK", "Rapido", "App Server", "Cache RAM", "DB Primario", "Renderizza", "33 tipi di diagrammi", "offline in SVG nitidi"),
    ("Note Matematiche", "Integrale di Gauss e distribuzione normale:", "Autovalori e matrice di rotazione 2D:", "Formule inline", "nitide tramite motore MathML nativo"),
    "Log di audit server", "Rendering sincronizzato col tema",
)

LANG_PACKS["pl"] = _make_pack(
    "pl", "Pt., 20 lis", "Aktywne kursory: 3  ·  TypeScript  ·  UTF-8", "FIXED",
    {
        "s1": ([("Podgląd na żywo", True), ("dla Markdown", False)], [[("Podgląd na żywo", True)], [("dla Markdown", False)]]),
        "s2": ([("Diagramy Mermaid", True), ("rysowane podczas pisania", False)], [[("Diagramy Mermaid", True)], [("podczas pisania", False)]]),
        "s3": ([("Wzory LaTeX", True), ("w 100% offline na żywo", False)], [[("Wzory LaTeX", True)], [("w 100% offline", False)]]),
        "s4": ([("Szybkie szukanie i", False), ("zamiana z Regex", True)], [[("Szybkie szukanie i", False)], [("zamiana z Regex", True)]]),
        "s5": ([("65 języków kodu i", False), ("edycja wielokursorowa", True)], [[("65 języków kodu i", False)], [("edycja wielokursorowa", True)]]),
    },
    ("Notatki projektowe", "Szybki podgląd", "Renderuje tylko zmienione fragmenty w ułamku sekundy.", "Moduł", "Rola", "Status", "Edycja PieceTable", "CommonMark / GFM", "Wzory i 33 diagramy", "Gotowe", "Lista kontrolna", "Automatyczny tryb ciemny / jasny", "Działanie w 100% offline", "Eksport raportu wydajności"),
    ("Przepływ żądań", "Klient", "Brama API", "Auth OK", "Szybko", "Serwer App", "Pamięć Cache", "Główna DB", "Renderuj offline", "33 typy diagramów", "jako ostre obrazy SVG"),
    ("Notatki matematyczne", "Całka Gaussa i rozkład normalny:", "Wartości własne i macierz obrotu 2D:", "Wzory w linii", "wyraźnie renderowane przez natywny MathML"),
    "Dziennik audytu serwera", "Renderowanie zgodne z motywem",
)

LANG_PACKS["nl"] = _make_pack(
    "nl", "Vr 20 nov", "3 cursors actief  ·  TypeScript  ·  UTF-8", "FIXED",
    {
        "s1": ([("Directe liveweergave", True), ("voor Markdown", False)], [[("Directe liveweergave", True)], [("voor Markdown", False)]]),
        "s2": ([("Mermaid-diagrammen", True), ("live tijdens typen", False)], [[("Mermaid-diagrammen", True)], [("live tijdens typen", False)]]),
        "s3": ([("LaTeX-formules", True), ("100% offline gerenderd", False)], [[("LaTeX-formules", True)], [("100% offline gerenderd", False)]]),
        "s4": ([("Snel zoeken en", False), ("vervangen met Regex", True)], [[("Snel zoeken en", False)], [("vervangen met Regex", True)]]),
        "s5": ([("65 talen en", False), ("multi-cursorbewerking", True)], [[("65 talen en", False)], [("multi-cursorbewerking", True)]]),
    },
    ("Projectnotities", "Snelle liveweergave", "Rendert direct alleen gewijzigde blokken tijdens typen.", "Module", "Functie", "Status", "PieceTable-editor", "CommonMark / GFM", "Formules & 33 diagrammen", "Klaar", "Release-checklist", "Donkere / lichte modus synchroon", "Werkt 100% offline lokaal", "Prestatierapport exporteren"),
    ("Aanvraagverwerking", "Client", "API-gateway", "Auth OK", "Snel", "App-server", "Geheugencache", "Primaire DB", "Render", "33 diagramtypen", "offline als haarscherpe SVG"),
    ("Wiskundige notities", "Gauss-integraal & normale verdeling:", "Eigenwaarden & 2D-rotatiematrix:", "Inline-formules", "haarscherp via ingebouwde MathML"),
    "Server-auditlogboek", "Thema-bewuste weergave",
)

LANG_PACKS["tr"] = _make_pack(
    "tr", "20 Kas Cum", "3 imleç etkin  ·  TypeScript  ·  UTF-8", "FIXED",
    {
        "s1": ([("Markdown İçin", True), ("Anında Canlı Önizleme", False)], [[("Markdown İçin", True)], [("Canlı Önizleme", False)]]),
        "s2": ([("Mermaid Şemaları", True), ("Yazarken Anında Çizilir", False)], [[("Mermaid Şemaları", True)], [("Anında Çizilir", False)]]),
        "s3": ([("LaTeX Formülleri", True), ("%100 Çevrimdışı Net Çizim", False)], [[("LaTeX Formülleri", True)], [("%100 Çevrimdışı Çizim", False)]]),
        "s4": ([("Regex ile Hızlı", False), ("Arama ve Değiştirme", True)], [[("Regex ile Hızlı", False)], [("Arama ve Değiştirme", True)]]),
        "s5": ([("65 Dil Vurgulama ve", False), ("Çoklu İmleç Düzenleme", True)], [[("65 Dil Vurgulama ve", False)], [("Çoklu İmleç Düzenleme", True)]]),
    },
    ("Proje Tasarım Notları", "Hızlı Canlı Önizleme", "Yazarken yalnızca değişen blokları anında çizer.", "Modül", "Görev", "Durum", "PieceTable hızlı düzenleme", "CommonMark / GFM", "Matematik ve 33 şema", "Tamam", "Sürüm Kontrol Listesi", "Koyu / Açık mod otomatik uyum", "%100 çevrimdışı yerel çalışma", "Performans raporu çıktısı"),
    ("İstek İşleme Akışı", "İstemci", "API Geçidi", "Yetki OK", "Hızlı", "Uygulama Sunucusu", "Bellek Önbelleği", "Ana Veritabanı", "Çevrimdışı", "33 şema türünü", "anında keskin SVG olarak çizin"),
    ("Matematiksel Notlar", "Gauss integrali ve normal dağılım:", "Özdeğer denklemi ve 2B dönüş matrisi:", "Satır içi formüller", "yerel MathML ile net görüntülenir"),
    "Sunucu Denetim Günlüğü", "Temaya duyarlı çizim kontrolü",
)

LANG_PACKS["vi"] = _make_pack(
    "vi", "T6, 20 thg 11", "3 con trỏ đang sửa  ·  TypeScript  ·  UTF-8", "FIXED",
    {
        "s1": ([("Xem trước trực tiếp", True), ("cho Markdown", False)], [[("Xem trước trực tiếp", True)], [("cho Markdown", False)]]),
        "s2": ([("Sơ đồ Mermaid", True), ("vẽ ngay khi bạn gõ", False)], [[("Sơ đồ Mermaid", True)], [("vẽ ngay khi bạn gõ", False)]]),
        "s3": ([("Công thức LaTeX", True), ("100% ngoại tuyến", False)], [[("Công thức LaTeX", True)], [("100% ngoại tuyến", False)]]),
        "s4": ([("Tìm kiếm nhanh &", False), ("thay thế bằng Regex", True)], [[("Tìm kiếm nhanh &", False)], [("thay thế bằng Regex", True)]]),
        "s5": ([("65 ngôn ngữ &", False), ("chỉnh sửa đa con trỏ", True)], [[("65 ngôn ngữ &", False)], [("chỉnh sửa đa con trỏ", True)]]),
    },
    ("Ghi chú thiết kế", "Xem trước tức thì", "Chỉ kết xuất phần thay đổi ngay khi đang gõ.", "Mô-đun", "Vai trò", "Trạng thái", "Soạn thảo PieceTable", "CommonMark / GFM", "Toán & 33 loại sơ đồ", "Xong", "Danh sách kiểm tra", "Tự động đồng bộ Sáng / Tối", "Hoạt động 100% ngoại tuyến", "Xuất báo cáo hiệu năng"),
    ("Luồng xử lý yêu cầu", "Máy khách", "Cổng API", "Hợp lệ", "Nhanh", "Máy chủ App", "Bộ nhớ đệm", "CSDL chính", "Kết xuất ngoại tuyến", "33 loại sơ đồ", "thành SVG sắc nét"),
    ("Ghi chú toán học", "Tích phân Gauss và phân phối chuẩn:", "Phương trình trị riêng & ma trận quay 2D:", "Công thức nội dòng", "hiển thị sắc nét bằng MathML gốc"),
    "Nhật ký kiểm toán máy chủ", "Điều khiển kết xuất theo giao diện",
)

LANG_PACKS["uk"] = _make_pack(
    "uk", "Пт, 20 лист.", "Активних курсорів: 3  ·  TypeScript  ·  UTF-8", "FIXED",
    {
        "s1": ([("Живий перегляд", True), ("для Markdown", False)], [[("Живий перегляд", True)], [("для Markdown", False)]]),
        "s2": ([("Діаграми Mermaid", True), ("одразу під час вводу", False)], [[("Діаграми Mermaid", True)], [("під час введення", False)]]),
        "s3": ([("Формули LaTeX", True), ("миттєво й 100% офлайн", False)], [[("Формули LaTeX", True)], [("100% офлайн", False)]]),
        "s4": ([("Швидкий пошук і", False), ("заміна через Regex", True)], [[("Швидкий пошук і", False)], [("заміна через Regex", True)]]),
        "s5": ([("65 мов коду та", False), ("мультикурсор", True)], [[("65 мов коду та", False)], [("мультикурсор", True)]]),
    },
    ("Нотатки проєкту", "Миттєвий перегляд", "Оновлює лише змінені блоки під час набору тексту.", "Модуль", "Роль", "Статус", "Редактор PieceTable", "CommonMark / GFM", "Формули та 33 діаграми", "Готово", "Список перед релізом", "Автосинхронізація темної теми", "Повністю офлайн на пристрої", "Експорт звіту швидкодії"),
    ("Обробка запитів", "Клієнт", "API-шлюз", "Вхід ОК", "Кеш", "Сервер", "Кеш пам'яті", "Основна БД", "Миттєвий офлайн-рендеринг", "33 типів діаграм", "у чіткий SVG"),
    ("Математичні нотатки", "Інтеграл Гаусса та нормальний розподіл:", "Власні значення та матриця повороту 2D:", "Рядкові формули", "чітко зображуються через нативний MathML"),
    "Журнал аудиту сервера", "Рендеринг із підтримкою теми",
)

LANG_PACKS["id"] = _make_pack(
    "id", "Jum 20 Nov", "3 kursor aktif  ·  TypeScript  ·  UTF-8", "FIXED",
    {
        "s1": ([("Pratinjau Langsung", True), ("untuk Markdown", False)], [[("Pratinjau Langsung", True)], [("untuk Markdown", False)]]),
        "s2": ([("Diagram Mermaid", True), ("Terlukis Saat Mengetik", False)], [[("Diagram Mermaid", True)], [("Saat Anda Mengetik", False)]]),
        "s3": ([("Rumus LaTeX Tajam", True), ("100% Offline Instan", False)], [[("Rumus LaTeX Tajam", True)], [("100% Offline Instan", False)]]),
        "s4": ([("Pencarian Cepat &", False), ("Ganti Massal Regex", True)], [[("Pencarian Cepat &", False)], [("Ganti Massal Regex", True)]]),
        "s5": ([("65 Bahasa Kode &", False), ("Edit Multi-Kursor", True)], [[("65 Bahasa Kode &", False)], [("Edit Multi-Kursor", True)]]),
    },
    ("Catatan Desain Proyek", "Pratinjau Instan", "Merender blok yang berubah secara instan saat mengetik.", "Modul", "Peran Utama", "Status", "Edit cepat PieceTable", "CommonMark / GFM", "Rumus & 33 jenis diagram", "Selesai", "Daftar Periksa Rilis", "Sinkron otomatis Mode Gelap / Terang", "Berjalan 100% offline di perangkat", "Ekspor laporan performa"),
    ("Alur Pemrosesan", "Klien", "API Gateway", "Auth OK", "Cepat", "Server App", "Cache Memori", "DB Utama", "Render", "33 jenis diagram", "secara offline ke SVG tajam"),
    ("Catatan Model Matematika", "Integral Gauss & distribusi normal:", "Persamaan nilai eigen & matriks rotasi 2D:", "Rumus sebaris", "tampil tajam lewat mesin MathML bawaan"),
    "Log Audit Server", "Kontrol render selaras tema",
)

LANG_PACKS["cs"] = _make_pack(
    "cs", "Pá 20. 11.", "Aktivní kurzory: 3  ·  TypeScript  ·  UTF-8", "FIXED",
    {
        "s1": ([("Živý náhled", True), ("pro Markdown", False)], [[("Živý náhled", True)], [("pro Markdown", False)]]),
        "s2": ([("Diagramy Mermaid", True), ("ihned při psaní", False)], [[("Diagramy Mermaid", True)], [("ihned při psaní", False)]]),
        "s3": ([("Vzorce LaTeX", True), ("okamžitě a 100% offline", False)], [[("Vzorce LaTeX", True)], [("100% offline", False)]]),
        "s4": ([("Rychlé hledání a", False), ("nahrazování Regexem", True)], [[("Rychlé hledání a", False)], [("nahrazování Regexem", True)]]),
        "s5": ([("65 jazyků a", False), ("úpravy multi-kurzorem", True)], [[("65 jazyků a", False)], [("multi-kurzor", True)]]),
    },
    ("Poznámky k návrhu", "Rychlý živý náhled", "Při psaní okamžitě překresluje jen změněné bloky.", "Modul", "Úloha", "Stav", "Rychlé úpravy PieceTable", "CommonMark / GFM", "Matematika a 33 diagramů", "Hotovo", "Kontrolní seznam", "Automatický tmavý / světlý režim", "100% offline provoz", "Export přehledu výkonu"),
    ("Zpracování požadavku", "Klient", "API brána", "Ověřeno", "Rychle", "App server", "Paměť cache", "Primární DB", "Vykreslí", "33 typů diagramů", "offline jako ostré SVG"),
    ("Matematické poznámky", "Gaussův integrál a normální rozdělení:", "Vlastní čísla a 2D matice rotace:", "Řádkové vzorce", "ostře vykreslené nativním MathML"),
    "Auditní protokol serveru", "Vykreslování podle motivu",
)

LANG_PACKS["sv"] = _make_pack(
    "sv", "Fre 20 nov.", "3 markörer aktiva  ·  TypeScript  ·  UTF-8", "FIXED",
    {
        "s1": ([("Liveförhandsvisning", True), ("för Markdown", False)], [[("Livevisning", True)], [("för Markdown", False)]]),
        "s2": ([("Mermaid-diagram", True), ("ritas medan du skriver", False)], [[("Mermaid-diagram", True)], [("medan du skriver", False)]]),
        "s3": ([("LaTeX-formler", True), ("100 % offline direkt", False)], [[("LaTeX-formler", True)], [("100 % offline direkt", False)]]),
        "s4": ([("Snabb sökning &", False), ("ersättning med Regex", True)], [[("Snabb sökning &", False)], [("ersättning med Regex", True)]]),
        "s5": ([("65 språk &", False), ("multimarkörredigering", True)], [[("65 språk &", False)], [("multimarkör", True)]]),
    },
    ("Designanteckningar", "Snabb livevisning", "Renderar endast ändrade block direkt när du skriver.", "Modul", "Roll", "Status", "PieceTable-redigering", "CommonMark / GFM", "Matematik & 33 diagram", "Klart", "Checklista inför släpp", "Automatiskt mörkt / ljust läge", "100 % offline på enheten", "Exportera prestandarapport"),
    ("Begärandeflöde", "Klient", "API-gateway", "Auth OK", "Snabb", "Appserver", "Minnescache", "Primär DB", "Rendera", "33 diagramtyper", "offline som knivskarpa SVG"),
    ("Matematiska anteckningar", "Gaussintegral och normalfördelning:", "Egenvärdesekvation och 2D-rotationsmatris:", "Inline-formler", "visas skarpt med inbyggd MathML"),
    "Servergranskningslogg", "Temastyrd rendering",
)

LANG_PACKS["th"] = _make_pack(
    "th", "ศ. 20 พ.ย.", "แก้ไขพร้อมกัน 3 เคอร์เซอร์  ·  TypeScript  ·  UTF-8", "FIXED",
    {
        "s1": ([("พรีวิว Markdown", True), ("แบบแบ่งหน้าจอเรียลไทม์", False)], [[("พรีวิว Markdown", True)], [("แบบเรียลไทม์", False)]]),
        "s2": ([("แผนภาพ Mermaid", True), ("วาดกราฟทันทีที่พิมพ์", False)], [[("แผนภาพ Mermaid", True)], [("วาดกราฟทันทีที่พิมพ์", False)]]),
        "s3": ([("สมการ LaTeX คมชัด", True), ("เรนเดอร์ออฟไลน์ทันที", False)], [[("สมการ LaTeX คมชัด", True)], [("ออฟไลน์ 100%", False)]]),
        "s4": ([("ค้นหาและแทนที่ไว", True), ("ด้วย Regex", False)], [[("ค้นหาและแทนที่ไว", True)], [("ด้วย Regex", False)]]),
        "s5": ([("ไฮไลต์ 65 ภาษา &", False), ("แก้ไขด้วยมัลติเคอร์เซอร์", True)], [[("ไฮไลต์ 65 ภาษา &", False)], [("มัลติเคอร์เซอร์", True)]]),
    },
    ("บันทึกการออกแบบระบบ", "พรีวิวสดความเร็วสูง", "เรนเดอร์เฉพาะส่วนที่แก้ไขทันทีขณะพิมพ์", "โมดูล", "หน้าที่หลัก", "สถานะ", "แก้ไขไวด้วย PieceTable", "CommonMark / GFM", "สมการและแผนภาพ 33 แบบ", "เสร็จ", "รายการตรวจสอบก่อนปล่อย", "ซิงค์โหมดมืด / สว่างอัตโนมัติ", "ทำงานออฟไลน์ 100%", "ส่งออกรายงานประสิทธิภาพ"),
    ("ลำดับการประมวลผล", "ไคลเอนต์", "API เกตเวย์", "ยืนยันผ่าน", "แคชไว", "แอปเซิร์ฟเวอร์", "แคชหน่วยความจำ", "ฐานข้อมูลหลัก", "เรนเดอร์", "แผนภาพทั้ง 33 รูปแบบ", "เป็น SVG คมชัดแบบออฟไลน์"),
    ("บันทึกแบบจำลองคณิตศาสตร์", "อินทิกรัลเกาส์และการแจกแจงปกติ:", "สมการค่าเจาะจงและเมทริกซ์หมุน 2 มิติ:", "สมการในบรรทัด", "แสดงผลคมชัดด้วย MathML เนทีฟ"),
    "บันทึกตรวจสอบเซิร์ฟเวอร์", "ควบคุมการเรนเดอร์ตามธีม",
)

LANG_PACKS["hi"] = _make_pack(
    "hi", "शुक्र 20 नव॰", "3 कर्सर सक्रिय  ·  TypeScript  ·  UTF-8", "FIXED",
    {
        "s1": ([("Markdown के लिए", False), ("लाइव स्प्लिट प्रीव्यू", True)], [[("Markdown के लिए", False)], [("लाइव स्प्लिट प्रीव्यू", True)]]),
        "s2": ([("Mermaid डायग्राम", True), ("टाइप करते ही ग्राफ़ में", False)], [[("Mermaid डायग्राम", True)], [("टाइप करते ही ग्राफ़", False)]]),
        "s3": ([("LaTeX गणित सूत्र", True), ("100% ऑफ़लाइन रेंडर", False)], [[("LaTeX गणित सूत्र", True)], [("100% ऑफ़लाइन रेंडर", False)]]),
        "s4": ([("Regex द्वारा तेज़", False), ("खोज और एक साथ बदलाव", True)], [[("Regex द्वारा तेज़", False)], [("खोज और बदलाव", True)]]),
        "s5": ([("65 भाषाएं और", False), ("मल्टी-कर्सर एडिटिंग", True)], [[("65 भाषाएं और", False)], [("मल्टी-कर्सर एडिटिंग", True)]]),
    },
    ("प्रोजेक्ट डिज़ाइन नोट्स", "तेज़ लाइव प्रीव्यू", "टाइप करते समय केवल बदले गए हिस्सों को तुरंत रेंडर करता है।", "मॉड्यूल", "मुख्य भूमिका", "स्थिति", "PieceTable तेज़ संपादन", "CommonMark / GFM", "गणित व 33 डायग्राम इंजन", "पूर्ण", "रिलीज़ चेकलिस्ट", "डार्क / लाइट मोड ऑटो सिंक", "100% ऑफ़लाइन कार्यप्रणाली", "प्रदर्शन रिपोर्ट निर्यात"),
    ("अनुरोध प्रोसेसिंग फ़्लो", "क्लाइंट", "API गेटवे", "सत्यापित", "तेज़ कैश", "ऐप सर्वर", "मेमोरी कैश", "प्राइमरी DB", "ऑफ़लाइन रेंडर करें", "सभी 33 प्रकार के डायग्राम", "स्पष्ट SVG में"),
    ("गणितीय मॉडल नोट्स", "गॉसियन समाकलन और सामान्य वितरण:", "आइगेनवैल्यू समीकरण और 2D रोटेशन मैट्रिक्स:", "इनलाइन सूत्र", "नेटिव MathML द्वारा स्पष्ट रूप से प्रदर्शित"),
    "सर्वर ऑडिट लॉग", "थीम-आधारित रेंडर नियंत्रण",
)

LANG_PACKS["ro"] = _make_pack(
    "ro", "Vin, 20 nov.", "3 cursoare active  ·  TypeScript  ·  UTF-8", "FIXED",
    {
        "s1": ([("Previzualizare live", True), ("pentru Markdown", False)], [[("Previzualizare live", True)], [("pentru Markdown", False)]]),
        "s2": ([("Diagrame Mermaid", True), ("redate în timp real", False)], [[("Diagrame Mermaid", True)], [("redate în timp real", False)]]),
        "s3": ([("Formule LaTeX", True), ("100% offline instant", False)], [[("Formule LaTeX", True)], [("100% offline instant", False)]]),
        "s4": ([("Căutare rapidă și", False), ("înlocuire cu Regex", True)], [[("Căutare rapidă și", False)], [("înlocuire cu Regex", True)]]),
        "s5": ([("65 de limbaje și", False), ("editare multi-cursor", True)], [[("65 de limbaje și", False)], [("editare multi-cursor", True)]]),
    },
    ("Note de proiectare", "Previzualizare rapidă", "Redă instantaneu doar blocurile modificate.", "Modul", "Rol principal", "Stare", "Editare PieceTable", "CommonMark / GFM", "Matematică & 33 diagrame", "Gata", "Listă verificare", "Mod întunecat / luminos auto", "Funcționare 100% offline", "Export raport performanță"),
    ("Flux procesare cereri", "Client", "API Gateway", "Auth OK", "Rapid", "Server App", "Cache RAM", "DB Primar", "Redare offline pentru", "33 tipuri de diagrame", "în SVG clar"),
    ("Note matematice", "Integrala Gauss și distribuția normală:", "Valori proprii și matricea de rotație 2D:", "Formulele inline", "afișate clar prin MathML nativ"),
    "Jurnal audit server", "Control redare adaptat temei",
)

LANG_PACKS["hu"] = _make_pack(
    "hu", "Nov. 20., P", "3 aktív kurzor  ·  TypeScript  ·  UTF-8", "FIXED",
    {
        "s1": ([("Élő osztott előnézet", True), ("Markdownhoz", False)], [[("Élő előnézet", True)], [("Markdownhoz", False)]]),
        "s2": ([("Mermaid diagramok", True), ("kirajzolása gépeléskor", False)], [[("Mermaid diagramok", True)], [("gépelés közben", False)]]),
        "s3": ([("LaTeX képletek", True), ("100% offline azonnal", False)], [[("LaTeX képletek", True)], [("100% offline azonnal", False)]]),
        "s4": ([("Gyors keresés és", False), ("csoportos Regex csere", True)], [[("Gyors keresés és", False)], [("Regex csere", True)]]),
        "s5": ([("65 nyelv és", False), ("többkurzoros szerkesztés", True)], [[("65 nyelv és", False)], [("többkurzoros írás", True)]]),
    },
    ("Tervezési jegyzetek", "Azonnali előnézet", "Gépelés közben csak a módosult részeket frissíti.", "Modul", "Feladat", "Állapot", "PieceTable szerkesztő", "CommonMark / GFM", "Képletek és 33 diagram", "Kész", "Kiadási ellenőrzőlista", "Automatikus sötét / világos mód", "100% offline működés", "Teljesítményjelentés exportja"),
    ("Kérésfeldolgozás", "Kliens", "API átjáró", "Auth OK", "Gyors", "App szerver", "Memória cache", "Elsődleges DB", " mind a", "33 diagramtípus", "azonnali offline SVG rajzolása"),
    ("Matematikai jegyzetek", "Gauss-integrál és normális eloszlás:", "Sajátérték-egyenlet és 2D forgatásmátrix:", "Szövegközi képletek", "tűélesen natív MathML motorral"),
    "Szerver auditnapló", "Témához igazodó megjelenítés",
)

LANG_PACKS["el"] = _make_pack(
    "el", "Παρ 20 Νοε", "3 ενεργοί δρομείς  ·  TypeScript  ·  UTF-8", "FIXED",
    {
        "s1": ([("Ζωντανή προεπισκόπηση", True), ("για Markdown", False)], [[("Ζωντανή προβολή", True)], [("για Markdown", False)]]),
        "s2": ([("Διαγράμματα Mermaid", True), ("καθώς πληκτρολογείτε", False)], [[("Διαγράμματα Mermaid", True)], [("σε πραγματικό χρόνο", False)]]),
        "s3": ([("Εξισώσεις LaTeX", True), ("100% εκτός σύνδεσης", False)], [[("Εξισώσεις LaTeX", True)], [("100% εκτός σύνδεσης", False)]]),
        "s4": ([("Γρήγορη αναζήτηση &", False), ("αντικατάσταση Regex", True)], [[("Αναζήτηση &", False)], [("αντικατάσταση Regex", True)]]),
        "s5": ([("65 γλώσσες &", False), ("πολλαπλοί δρομείς", True)], [[("65 γλώσσες &", False)], [("πολλαπλοί δρομείς", True)]]),
    },
    ("Σημειώσεις Σχεδίασης", "Άμεση προεπισκόπηση", "Αποδίδει μόνο τις αλλαγές σε χιλιοστά δευτερολέπτου.", "Μονάδα", "Ρόλος", "Κατάσταση", "Επεξεργασία PieceTable", "CommonMark / GFM", "Μαθηματικά & 33 διαγράμματα", "Έτοιμο", "Λίστα Ελέγχου", "Αυτόματο Σκοτεινό / Φωτεινό θέμα", "Λειτουργία 100% εκτός σύνδεσης", "Εξαγωγή αναφοράς απόδοσης"),
    ("Ροή Αιτημάτων", "Πελάτης", "Πύλη API", "Auth OK", "Ταχεία", "Διακομιστής", "Μνήμη Cache", "Κύρια ΒΔ", "Άμεση απόδοση", "33 τύπων διαγραμμάτων", "σε ευκρινή SVG"),
    ("Μαθηματικές Σημειώσεις", "Ολοκλήρωμα Gauss & κανονική κατανομή:", "Ιδιοτιμές & πίνακας περιστροφής 2D:", "Ενσωματωμένοι τύποι", "με ευκρίνεια μέσω εγγενούς MathML"),
    "Αρχείο ελέγχου διακομιστή", "Απόδοση συγχρονισμένη με το θέμα",
)

LANG_PACKS["da"] = _make_pack(
    "da", "Fre. 20. nov.", "3 markører aktive  ·  TypeScript  ·  UTF-8", "FIXED",
    {
        "s1": ([("Live-forhåndsvisning", True), ("af Markdown", False)], [[("Live-visning", True)], [("af Markdown", False)]]),
        "s2": ([("Mermaid-diagrammer", True), ("tegnes mens du skriver", False)], [[("Mermaid-diagrammer", True)], [("mens du skriver", False)]]),
        "s3": ([("LaTeX-formler", True), ("100 % offline med det samme", False)], [[("LaTeX-formler", True)], [("100 % offline", False)]]),
        "s4": ([("Hurtig søgning og", False), ("erstatning med Regex", True)], [[("Hurtig søgning og", False)], [("Regex-erstatning", True)]]),
        "s5": ([("65 sprog og", False), ("multimarkør-redigering", True)], [[("65 sprog og", False)], [("multimarkør", True)]]),
    },
    ("Designnoter", "Hurtig live-visning", "Gengiver kun ændrede blokke straks mens du skriver.", "Modul", "Rolle", "Status", "PieceTable-redigering", "CommonMark / GFM", "Matematik & 33 diagrammer", "Klar", "Tjekliste til udgivelse", "Automatisk mørk / lys tilstand", "100 % offline på enheden", "Eksporter ydelsesrapport"),
    ("Anmodningsflow", "Klient", "API-gateway", "Auth OK", "Hurtig", "App-server", "Hukommelsescache", "Primær DB", "Gengiv", "33 diagramtyper", "offline som skarpe SVG'er"),
    ("Matematiske noter", "Gauss-integral og normalfordeling:", "Egenværdiligning og 2D-rotationsmatrix:", "Inline-formler", "vises skarpt via indbygget MathML"),
    "Serverrevisionslog", "Temastyret gengivelse",
)

LANG_PACKS["fi"] = _make_pack(
    "fi", "Pe 20.11.", "3 kohdistinta käytössä  ·  TypeScript  ·  UTF-8", "FIXED",
    {
        "s1": ([("Reaaliaikainen esikatselu", True), ("Markdownille", False)], [[("Live-esikatselu", True)], [("Markdownille", False)]]),
        "s2": ([("Mermaid-kaaviot", True), ("piirtyvät kirjoittaessa", False)], [[("Mermaid-kaaviot", True)], [("kirjoittaessasi", False)]]),
        "s3": ([("LaTeX-kaavat", True), ("100 % offline heti", False)], [[("LaTeX-kaavat", True)], [("100 % offline heti", False)]]),
        "s4": ([("Nopea haku ja", False), ("korvaus Regexillä", True)], [[("Nopea haku ja", False)], [("Regex-korvaus", True)]]),
        "s5": ([("65 kieltä ja", False), ("monikohdistinmuokkaus", True)], [[("65 kieltä ja", False)], [("monikohdistin", True)]]),
    },
    ("Suunnittelumuistiinpanot", "Välitön esikatselu", "Renderöi vain muuttuneet lohkot heti kirjoittaessa.", "Moduuli", "Tehtävä", "Tila", "PieceTable-muokkaus", "CommonMark / GFM", "Kaavat & 33 kaaviota", "Valmis", "Julkaisun tarkistuslista", "Automaattinen tumma / vaalea tila", "100 % offline-toiminta", "Vie suorituskykyraportti"),
    ("Pyyntöjen käsittely", "Asiakas", "API-yhdyskäytävä", "Auth OK", "Nopea", "Sovelluspalvelin", "Välimuisti", "Ensisijainen DB", "Renderöi", "33 kaaviotyyppiä", "offline tarkkana SVG:nä"),
    ("Matemaattiset muistiinpanot", "Gaussin integraali ja normaalijakauma:", "Ominaisarvoyhtälö ja 2D-kiertomatriisi:", "Rivinsisäiset kaavat", "terävästi natiivilla MathML:llä"),
    "Palvelimen tarkastusloki", "Teemaan mukautuva piirto",
)

LANG_PACKS["nb"] = _make_pack(
    "nb", "Fre. 20. nov.", "3 markører aktive  ·  TypeScript  ·  UTF-8", "FIXED",
    {
        "s1": ([("Live-forhåndsvisning", True), ("for Markdown", False)], [[("Live-visning", True)], [("for Markdown", False)]]),
        "s2": ([("Mermaid-diagrammer", True), ("tegnes mens du skriver", False)], [[("Mermaid-diagrammer", True)], [("mens du skriver", False)]]),
        "s3": ([("LaTeX-formler", True), ("100 % offline på stedet", False)], [[("LaTeX-formler", True)], [("100 % offline", False)]]),
        "s4": ([("Raskt søk og", False), ("erstatning med Regex", True)], [[("Raskt søk og", False)], [("Regex-erstatning", True)]]),
        "s5": ([("65 språk og", False), ("multimarkør-redigering", True)], [[("65 språk og", False)], [("multimarkør", True)]]),
    },
    ("Designnotater", "Rask live-visning", "Gjengir kun endrede blokker umiddelbart mens du skriver.", "Modul", "Rolle", "Status", "PieceTable-redigering", "CommonMark / GFM", "Matematikk & 33 diagrammer", "Klar", "Sjekkliste før lansering", "Automatisk mørk / lys modus", "100 % offline på enheten", "Eksporter ytelsesrapport"),
    ("Forespørselsflyt", "Klient", "API-gateway", "Auth OK", "Rask", "App-server", "Minnecache", "Primær DB", "Gjengi", "33 diagramtyper", "offline som skarpe SVG-er"),
    ("Matematiske notater", "Gauss-integral og normalfordeling:", "Egenverdiligning og 2D-rotasjonsmatrise:", "Inline-formler", "vises skarpt med innebygd MathML"),
    "Serverrevisjonslogg", "Temastyrt gjengivelse",
)

LANG_PACKS["ms"] = _make_pack(
    "ms", "Jum 20 Nov", "3 kursor aktif  ·  TypeScript  ·  UTF-8", "FIXED",
    {
        "s1": ([("Pratonton Langsung", True), ("untuk Markdown", False)], [[("Pratonton Langsung", True)], [("untuk Markdown", False)]]),
        "s2": ([("Rajah Mermaid", True), ("Dilukis Semasa Menaip", False)], [[("Rajah Mermaid", True)], [("Semasa Menaip", False)]]),
        "s3": ([("Formula LaTeX Tajam", True), ("100% Luar Talian", False)], [[("Formula LaTeX", True)], [("100% Luar Talian", False)]]),
        "s4": ([("Carian Pantas &", False), ("Gantian Kelompok Regex", True)], [[("Carian Pantas &", False)], [("Gantian Regex", True)]]),
        "s5": ([("65 Bahasa Kod &", False), ("Suntingan Multi-Kursor", True)], [[("65 Bahasa Kod &", False)], [("Multi-Kursor", True)]]),
    },
    ("Nota Reka Bentuk", "Pratonton Segera", "Memaparkan blok berubah secara serta-merta semasa menaip.", "Modul", "Peranan", "Status", "Suntingan pantas PieceTable", "CommonMark / GFM", "Matematik & 33 jenis rajah", "Siap", "Senarai Semak Keluaran", "Mod Gelap / Cerah automatik", "Berfungsi 100% luar talian", "Eksport laporan prestasi"),
    ("Aliran Permintaan", "Klien", "API Gateway", "Sah OK", "Pantas", "Pelayan App", "Cache Memori", "DB Utama", "Paparkan", "33 jenis rajah", "luar talian sebagai SVG tajam"),
    ("Nota Model Matematik", "Kamiran Gauss & taburan normal:", "Persamaan nilai eigen & matriks putaran 2D:", "Formula sebaris", "dipapar jelas melalui MathML asli"),
    "Log Audit Pelayan", "Kawalan paparan mengikut tema",
)

LANG_PACKS["sk"] = _make_pack(
    "sk", "Pi 20. 11.", "Aktívne kurzory: 3  ·  TypeScript  ·  UTF-8", "FIXED",
    {
        "s1": ([("Živý náhľad", True), ("pre Markdown", False)], [[("Živý náhľad", True)], [("pre Markdown", False)]]),
        "s2": ([("Diagramy Mermaid", True), ("ihneď pri písaní", False)], [[("Diagramy Mermaid", True)], [("ihneď pri písaní", False)]]),
        "s3": ([("Vzorce LaTeX", True), ("100 % offline naživo", False)], [[("Vzorce LaTeX", True)], [("100 % offline", False)]]),
        "s4": ([("Rýchle hľadanie a", False), ("nahrádzanie Regexom", True)], [[("Rýchle hľadanie a", False)], [("nahrádzanie Regexom", True)]]),
        "s5": ([("65 jazykov a", False), ("úpravy multi-kurzorom", True)], [[("65 jazykov a", False)], [("multi-kurzor", True)]]),
    },
    ("Poznámky k návrhu", "Rýchly živý náhľad", "Pri písaní okamžite prekresľuje len zmenené bloky.", "Modul", "Úloha", "Stav", "Rýchle úpravy PieceTable", "CommonMark / GFM", "Matematika a 33 diagramov", "Hotovo", "Kontrolný zoznam", "Automatický tmavý / svetlý režim", "100 % offline prevádzka", "Export prehľadu výkonu"),
    ("Spracovanie požiadavky", "Klient", "API brána", "Overené", "Rýchlo", "App server", "Pamäť cache", "Primárna DB", "Vykreslí", "33 typov diagramov", "offline ako ostré SVG"),
    ("Matematické poznámky", "Gaussov integrál a normálne rozdelenie:", "Vlastné čísla a 2D matica rotácie:", "Riadkové vzorce", "ostro vykreslené natívnym MathML"),
    "Auditný záznam servera", "Vykresľovanie podľa motívu",
)

LANG_PACKS["ar"] = _make_pack(
    "ar", "الجمعة 20 نوفمبر", "3 مؤشرات نشطة  ·  TypeScript  ·  UTF-8", "تم",
    {
        "s1": ([("معاينة فورية منقسمة", True), ("لملفات Markdown", False)], [[("معاينة فورية منقسمة", True)], [("لملفات Markdown", False)]]),
        "s2": ([("مخططات Mermaid", True), ("تُرسَم فور الكتابة", False)], [[("مخططات Mermaid", True)], [("تُرسَم فور الكتابة", False)]]),
        "s3": ([("معادلات LaTeX واضحة", True), ("100% دون إنترنت", False)], [[("معادلات LaTeX واضحة", True)], [("100% دون إنترنت", False)]]),
        "s4": ([("بحث سريع واستبدال", True), ("بتعبيرات Regex", False)], [[("بحث سريع واستبدال", True)], [("بتعبيرات Regex", False)]]),
        "s5": ([("إبراز 65 لغة برمجة", False), ("وتحرير متعدد المؤشرات", True)], [[("إبراز 65 لغة برمجة", False)], [("وتحرير متعدد المؤشرات", True)]]),
    },
    ("ملاحظات تصميم المشروع", "معاينة حية سريعة", "تُحدِّث الأجزاء المعدَّلة فقط خلال أجزاء من الثانية.", "الوحدة", "الدور الأساسي", "الحالة", "تحرير PieceTable السريع", "CommonMark / GFM", "معادلات و33 نوع مخطط", "مكتمل", "قائمة ما قبل الإصدار", "مزامنة الوضع الداكن والفاتح", "يعمل 100% دون إنترنت", "تصدير تقرير الأداء"),
    ("مسار معالجة الطلبات", "Client", "API Gateway", "Auth OK", "Fast Hit", "App Server", "Memory Cache", "Primary DB", "عرض فوري دون إنترنت لـ", "33 نوعاً من المخططات", "بدقة SVG عالية"),
    ("ملاحظات النماذج الرياضية", "تكامل غاوس والتوزيع الطبيعي:", "معادلة القيم الذاتية ومصفوفة الدوران:", "المعادلات السطرية", "تُعرض بوضوح عبر محرك MathML الأصلي"),
    "Server Audit Log", "Theme-aware frame renderer",
)
LANG_PACKS["ar"]["rtl"] = True

LANG_PACKS["he"] = _make_pack(
    "he", "יום ו׳, 20 בנוב׳", "3 סמנים פעילים  ·  TypeScript  ·  UTF-8", "תוקן",
    {
        "s1": ([("תצוגה מקדימה חיה", True), ("עבור Markdown", False)], [[("תצוגה מקדימה חיה", True)], [("עבור Markdown", False)]]),
        "s2": ([("דיאגרמות Mermaid", True), ("מצוירות תוך כדי הקלדה", False)], [[("דיאגרמות Mermaid", True)], [("תוך כדי הקלדה", False)]]),
        "s3": ([("נוסחאות LaTeX חדות", True), ("100% אופליין מיידית", False)], [[("נוסחאות LaTeX חדות", True)], [("100% אופליין", False)]]),
        "s4": ([("חיפוש מהיר והחלפה", True), ("באמצעות Regex", False)], [[("חיפוש מהיר והחלפה", True)], [("באמצעות Regex", False)]]),
        "s5": ([("הדגשה ל-65 שפות", False), ("ועריכה מרובת סמנים", True)], [[("הדגשה ל-65 שפות", False)], [("ועריכה מרובת סמנים", True)]]),
    },
    ("הערות תכנון פרויקט", "תצוגה חיה מהירה", "מרנדרת רק בלוקים שהשתנו באלפיות שנייה.", "מודול", "תפקיד עיקרי", "מצב", "עריכת PieceTable מהירה", "CommonMark / GFM", "נוסחאות ו-33 דיאגרמות", "הושלם", "רשימת בדיקה לשחרור", "סנכרון אוטומטי למצב כהה/בהיר", "פעולה 100% אופליין במכשיר", "ייצוא דוח ביצועים"),
    ("זרימת עיבוד בקשות", "Client", "API Gateway", "Auth OK", "Fast Hit", "App Server", "Memory Cache", "Primary DB", "רינדור אופליין של", "33 סוגי דיאגרמות", "כקובצי SVG חדים"),
    ("הערות מודלים מתמטיים", "אינטגרל גאוס והתפלגות נורמלית:", "משוואת ערכים עצמיים ומטריצת סיבוב:", "נוסחאות בשורה", "מוצגות בחדות באמצעות מנוע MathML מובנה"),
    "Server Audit Log", "Theme-aware frame renderer",
)
LANG_PACKS["he"]["rtl"] = True


EXTRA_UI_STRINGS = {
    "sk": {
        "File": "Súbor", "Edit": "Upraviť", "Find": "Hľadať", "View": "Zobraziť", "Tab": "Tab",
        "Replace": "Nahradiť", "Find:": "Hľadať:", "Replace:": "Nahradiť:",
        "Match Case": "Rozlišovať veľkosť", "Whole Word": "Celé slová", "Regex": "Regex",
        "Replace All": "Nahradiť všetko", "Find Next": "Hľadať ďalšie", "Done": "Hotovo", "All": "Všetko",
    },
    "ar": {
        "File": "ملف", "Edit": "تحرير", "Find": "بحث", "View": "عرض", "Tab": "جدولة",
        "Replace": "استبدال", "Find:": "بحث:", "Replace:": "استبدال:",
        "Match Case": "مطابقة الحالة", "Whole Word": "كلمة كاملة", "Regex": "Regex",
        "Replace All": "استبدال الكل", "Find Next": "بحث عن التالي", "Done": "تم", "All": "الكل",
    },
    "he": {
        "File": "קובץ", "Edit": "עריכה", "Find": "חיפוש", "View": "תצוגה", "Tab": "טאב",
        "Replace": "החלפה", "Find:": "חיפוש:", "Replace:": "החלפה:",
        "Match Case": "התאמת רישיות", "Whole Word": "מילה שלמה", "Regex": "Regex",
        "Replace All": "החלף הכל", "Find Next": "חפש הבא", "Done": "סיום", "All": "הכל",
    },
}


TS_BODY_TEMPLATE = """{comment}
export interface ViewportMetrics {{
  readonly width: number;
  readonly height: number;
  readonly scale: number;
}}

export class DocumentCanvas {{
  private dirty = false;

  public renderFrame(vp: ViewportMetrics): void {{
    if (!this.dirty || vp.width <= 0) return;
    this.dirty = false;
  }}

  public resize(vp: ViewportMetrics): void {{
    this.dirty = true;
    this.renderFrame(vp);
  }}

  public refreshAll(list: ViewportMetrics[]): void {{
    for (const vp of list) this.renderFrame(vp);
  }}
}}"""


def get_lang_pack(prefix: str) -> dict:
    pack = LANG_PACKS.get(prefix, LANG_PACKS["en"])
    out = dict(pack)
    out["s5"] = TS_BODY_TEMPLATE.format(comment=pack["s5_comment"])
    return out
