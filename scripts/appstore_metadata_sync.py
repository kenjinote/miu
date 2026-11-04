#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Synchronize App Store Connect localized metadata (description & whatsNew)
for miu (jp.hack.miu) across iOS and macOS platforms.
"""

import os
import sys
import time
import json
import jwt
import requests

BUNDLE_ID = "jp.hack.miu"
TARGET_VERSION = "1.0.47"
BASE_URL = "https://api.appstoreconnect.apple.com/v1"

# ============================================================================
# Localized App Store Metadata (Simple, polite, and likable tone)
# ============================================================================

JA_DESC = """miuは、毎日のちょっとしたメモから、Markdownでの文章作成、プログラミングまで、心地よく使えるシンプルなテキストエディタです。

アプリを開いた瞬間にすぐ書き始められる軽快さと、長時間の作業でも目が疲れにくい、すっきりとしたデザインを大切にして作られました。iPhone、iPad、Macのどの端末でも、自然でなめらかな書き心地をお届けします。

【主な特徴】

・すぐに書ける、軽快な動作
思いついたアイデアを逃さないよう、起動も文字入力も軽やかに動くよう設計されています。大きなテキストファイルもスムーズに開いて編集できます。

・文章作成もコード編集も見やすく
HTML、CSS、JavaScript、Python、Swift、C/C++、Markdown、JSONなど、49カテゴリ（120種類以上の拡張子）のシンタックスハイライトに対応しています。EmmetによるHTML/CSSの省略入力もそのままお使いいただけます。

・リアルタイムで確認できるプレビュー機能
MarkdownやHTML、SVGに加えて、Mermaidによる図表やLaTeXの数式も、インターネット接続なしでその場ですぐにプレビュー表示できます。

・日本語や多言語の文字化けを防止
UTF-8やShift_JIS、EUC-JPなど、さまざまな文字コードを自動で判別して正しく開きます。

・細部まで使いやすい編集サポート
検索・置換（正規表現対応）、マルチカーソル編集、行番号表示、折り返し切り替え、ダークモード・ライトモードなど、日々の編集を支える機能を丁寧に揃えました。

・安心のプライバシー設計
広告表示やトラッキング、外部へのデータ送信は一切ありません。すべての機能がお手元の端末内で完結します。

毎日のメモ帳としても、頼れるコードエディタとしても。
miuが、あなたの「書く時間」を少しでも快適にするお手伝いができれば幸いです。"""

JA_WHATS_NEW = """いつも miu をご利用いただきありがとうございます。
v1.0.47 では、編集・検索置換・リアルタイムプレビューの全面的な高速化を行いました。

・文字入力・スクロール・大容量ファイル編集の高速化：大きなファイルや長時間の編集作業でも、タイピング・カーソル移動・スクロールがこれまで以上に軽快でなめらかに反応するよう高速化しました。
・検索・「すべて置換」の劇的な高速化：キーワード検索や正規表現検索の照合処理、および数千〜数万箇所におよぶ一括置換（すべて置換）を大幅に高速化し、待ち時間なく瞬時に完了します。
・Markdown / LaTeX数式 / Mermaid図表 ライブプレビューの高速化：長文ドキュメントや複雑な図表・数式をリアルタイムプレビューしながら執筆する際の描画レスポンスを向上させました。"""

EN_DESC = """miu is a simple, lightweight text and code editor designed to make everything from quick daily notes to Markdown writing and programming feel effortless.

Built with a focus on instant responsiveness and a clean, distraction-free interface that is easy on the eyes, miu offers a natural and comfortable writing experience across iPhone, iPad, and Mac.

[Key Features]

• Fast and Responsive
Open the app and start typing right away. Whether you are jotting down a quick thought or editing a large text file, miu stays smooth and responsive.

• Clear and Readable for Both Writing and Coding
Supports syntax highlighting for 49 language categories (over 120 file extensions), including HTML, CSS, JavaScript, Python, Swift, C/C++, Markdown, and JSON. Built-in Emmet support lets you expand HTML and CSS abbreviations with a single tap.

• Built-in Live Preview
Preview Markdown, HTML, SVG, Mermaid diagrams, and LaTeX math equations right beside your text—completely offline.

• Automatic Character Encoding Detection
Automatically detects UTF-8, Shift_JIS, EUC-JP, UTF-16, and other international encodings so your files open cleanly without garbled text.

• Thoughtful Editing Tools
Includes Find & Replace (with Regular Expressions), multi-cursor editing, line numbers, word wrap toggle, and full support for both Light and Dark Mode.

• Privacy First
No ads, no tracking, and no data collection. Everything works 100% offline on your device.

Whether you use it as an everyday notepad or a reliable code editor, we hope miu makes your writing time a little more enjoyable."""

EN_WHATS_NEW = """Thank you for using miu.
Version 1.0.47 delivers major speed and responsiveness improvements across editing, search, and Live Preview:

• Faster Typing, Scrolling & Large-File Editing: Accelerated text input, cursor movement, and scrolling so even large files and long editing sessions remain smooth and responsive.
• Ultra-Fast Find & "Replace All": Dramatically sped up keyword search, regular expression matching, and bulk "Replace All" across thousands of matches so replacements finish in an instant.
• Faster Markdown, LaTeX & Mermaid Live Preview: Optimized real-time preview rendering for long Markdown documents, LaTeX math equations, and Mermaid diagrams while typing."""

ZH_HANS_DESC = """miu 是一款简洁、轻快的文本与代码编辑器。无论是记录日常随笔、撰写 Markdown 文档，还是编写程序代码，都能为您提供舒适顺畅的书写体验。

我们注重即开即写的轻盈感，以及清爽护眼的界面设计，让您在 iPhone、iPad 和 Mac 上都能自然、专注地进行创作。

【主要特点】

・启动迅速，轻快流畅
打开应用即可立即开始输入，不错过任何灵感。即使面对较大的文本文件，也能流畅地打开与编辑。

・兼顾文章写作与代码编辑
支持 HTML、CSS、JavaScript、Python、Swift、C/C++、Markdown、JSON 等 49 个类别（超过 120 种文件扩展名）的语法高亮显示，并内置 Emmet 缩写展开功能。

・实时预览功能
无需连接网络，即可直接在应用内实时预览 Markdown、HTML、SVG，以及 Mermaid 图表和 LaTeX 数学公式。

・自动识别字符编码
自动识别 UTF-8、Shift_JIS、EUC-JP、UTF-16 等多种字符编码，有效防止乱码问题。

・贴心的编辑辅助工具
提供查找与替换（支持正则表达式）、多光标编辑、行号显示、自动换行切换，并完美适配浅色与深色模式。

・安心的隐私保护
无广告、无追踪、不上传任何个人数据。所有功能均在您的设备本地离线运行。

无论是作为日常便签，还是得力的代码编辑器，希望 miu 能陪伴您度过每一段愉快的书写时光。"""

ZH_HANS_WHATS_NEW = """感谢您使用 miu。
v1.0.47 全面提升了文本编辑、查找替换与实时预览的运行速度：

・大幅提升输入、滚动与大文件编辑速度：深度优化文本处理速度，即使是超大文件或长时间连续编辑，打字输入、光标移动与页面滚动都更加轻快流畅。
・查找与“全部替换”速度大幅跃升：显著加速关键词查找、正则表达式匹配以及大批量“全部替换”的处理速度，成千上万处替换也能瞬间完成。
・Markdown / LaTeX 公式 / Mermaid 图表实时预览提速：优化实时预览的转换与渲染速度，在撰写长篇文档或复杂图表公式时预览响应更加迅速。"""

ZH_HANT_DESC = """miu 是一款簡潔、輕快的純文字與程式碼編輯器。無論是記錄日常隨手筆記、撰寫 Markdown 文章，還是編寫程式碼，都能為您帶來舒適順暢的書寫體驗。

我們重視打開即寫的輕盈感，以及清爽護眼的介面設計，讓您在 iPhone、iPad 與 Mac 上都能自然、專注地進行創作。

【主要特點】

・啟動迅速，輕快流暢
打開 App 即可立即開始輸入，不錯過任何靈感。即使面對較大的文字檔案，也能流暢地開啟與編輯。

・兼顧文章寫作與程式碼編輯
支援 HTML、CSS、JavaScript、Python、Swift、C/C++、Markdown、JSON 等 49 個類別（超過 120 種副檔名）的語法突顯，並內建 Emmet 縮寫展開功能。

・即時預覽功能
無需連線網路，即可直接在 App 內即時預覽 Markdown、HTML、SVG，以及 Mermaid 圖表與 LaTeX 數學公式。

・自動辨識字元編碼
自動判別 UTF-8、Shift_JIS、EUC-JP、UTF-16 等多種字元編碼，避免產生亂碼。

・貼心的編輯輔助功能
提供尋找與取代（支援正規表示式）、多游標編輯、行號顯示、自動換行切換，並完整支援淺色與深色模式。

・安心的隱私保護
無廣告、無追蹤、不傳送任何個人資料。所有功能皆在您的裝置本機離線完成。

無論是作為日常記事本，還是可靠的程式碼編輯器，希望 miu 能讓您的書寫時光更加愉快。"""

ZH_HANT_WHATS_NEW = """感謝您使用 miu。
v1.0.47 全面提升了文字編輯、尋找取代與即時預覽的執行速度：

・大幅提升輸入、捲動與大檔案編輯速度：深度最佳化文字處理速度，即使是超大檔案或長時間連續編輯，打字輸入、游標移動與頁面捲動都更加輕快流暢。
・尋找與「全部取代」速度大幅躍升：顯著加速關鍵字尋找、正規表示式比對以及大批量「全部取代」的處理速度，成千上萬處取代也能瞬間完成。
・Markdown / LaTeX 數學公式 / Mermaid 圖表即時預覽提速：最佳化即時預覽的轉換與繪製速度，在撰寫長篇文件或複雜圖表公式時預覽反應更加迅速。"""

KO_DESC = """miu는 일상의 가벼운 메모부터 Markdown 글쓰기, 프로그래밍까지 편안하게 사용할 수 있는 심플한 텍스트 및 코드 에디터입니다.

앱을 여는 순간 바로 글을 쓸 수 있는 가벼움과 장시간 작업에도 눈이 편안한 깔끔한 디자인을 담아, iPhone, iPad, Mac 어디서나 자연스럽고 매끄러운 작성 환경을 제공합니다.

【주요 특징】

・기다림 없이 바로 쓰는 가벼운 동작
떠오른 생각을 놓치지 않도록 빠른 실행과 부드러운 입력 반응성을 갖추었습니다. 용량이 큰 텍스트 파일도 쾌적하게 열고 편집할 수 있습니다.

・글쓰기와 코딩 모두 보기 편하게
HTML, CSS, JavaScript, Python, Swift, C/C++, Markdown, JSON 등 49개 카테고리(120종 이상의 확장자)의 구문 강조를 지원합니다. Emmet을 통한 HTML/CSS 약어 자동 완성도 바로 사용할 수 있습니다.

・실시간 미리보기 지원
Markdown과 HTML, SVG는 물론 Mermaid 다이어그램과 LaTeX 수식까지 인터넷 연결 없이 그 자리에서 바로 미리보기로 확인할 수 있습니다.

・자동 문자 인코딩 감지
UTF-8, Shift_JIS, EUC-JP, UTF-16 등 다양한 문자 인코딩을 자동으로 인식하여 글자 깨짐 없이 안전하게 엽니다.

・세심하게 준비된 편집 도구
찾기 및 바꾸기(정규식 지원), 다중 커서 편집, 줄 번호 표시, 자동 줄바꿈 전환, 라이트 모드 및 다크 모드 등 매일 쓰기 좋은 편집 기능을 갖추었습니다.

・안심할 수 있는 개인정보 보호
광고나 추적, 외부 서버로의 데이터 전송이 전혀 없습니다. 모든 기능은 기기 내에서 100% 오프라인으로 작동합니다.

매일 쓰는 메모장으로도, 든든한 코드 에디터로도.
miu가 당신의 글쓰기 시간을 조금 더 기분 좋게 만들어 드릴 수 있기를 바랍니다."""

KO_WHATS_NEW = """언제나 miu를 이용해 주셔서 감사합니다.
v1.0.47에서는 텍스트 편집, 찾기/바꾸기 및 실시간 미리보기의 처리 속도를 대폭 향상했습니다.

・타이핑, 스크롤 및 대용량 파일 편집 속도 향상: 대용량 파일이나 장시간 편집 작업에서도 글자 입력, 커서 이동, 스크롤이 한층 더 가볍고 매끄럽게 반응하도록 고속화했습니다.
・찾기 및 '모두 바꾸기' 초고속화: 키워드 검색, 정규식 매칭 및 수천~수만 건의 '모두 바꾸기' 처리 속도를 비약적으로 높여 대기 시간 없이 즉시 완료됩니다.
・Markdown / LaTeX 수식 / Mermaid 다이어그램 실시간 미리보기 고속화: 긴 문서나 복잡한 수식·다이어그램 작성 시 실시간 미리보기 렌더링 반응 속도를 높였습니다."""

DE_DESC = """miu ist ein schlichter, leichtgewichtiger Text- und Code-Editor, mit dem alltägliche Notizen, Markdown-Texte und Programmierarbeiten gleichermaßen angenehm von der Hand gehen.

Entwickelt mit Fokus auf sofortige Einsatzbereitschaft und eine aufgeräumte, augenschonende Oberfläche, bietet miu ein natürliches und flüssiges Schreibgefühl auf iPhone, iPad und Mac.

[Hauptmerkmale]

• Sofort startklar und flüssig
Öffnen Sie die App und schreiben Sie direkt los. Ob kurze Gedankenstütze oder große Textdatei – miu reagiert stets schnell und zuverlässig.

• Übersichtliches Schreiben und Programmieren
Unterstützt Syntaxhervorhebung für 49 Kategorien (über 120 Dateiendungen), darunter HTML, CSS, JavaScript, Python, Swift, C/C++, Markdown und JSON. Dank integriertem Emmet lassen sich HTML- und CSS-Abkürzungen mühelos erweitern.

• Integrierte Live-Vorschau
Betrachten Sie Markdown, HTML, SVG sowie Mermaid-Diagramme und LaTeX-Formeln direkt neben Ihrem Text – komplett offline.

• Automatische Zeichenkodierung
Erkennt UTF-8, Shift_JIS, EUC-JP, UTF-16 und weitere internationale Kodierungen automatisch, um fehlerhafte Zeichen zuverlässig zu vermeiden.

• Durchdachte Bearbeitungswerkzeuge
Mit Suchen & Ersetzen (inklusive regulärer Ausdrücke), Multi-Cursor-Bearbeitung, Zeilennummern, Zeilenumbruch sowie Unterstützung für den hellen und dunklen Modus.

• Privatsphäre an erster Stelle
Keine Werbung, kein Tracking und keine Datenübertragung. Alle Funktionen arbeiten zu 100 % lokal auf Ihrem Gerät.

Ob als täglicher Notizblock oder als verlässlicher Code-Editor – wir hoffen, dass miu Ihnen das Schreiben jeden Tag ein Stück angenehmer macht."""

DE_WHATS_NEW = """Vielen Dank, dass Sie miu nutzen.
Version 1.0.47 bringt deutliche Geschwindigkeitssteigerungen beim Bearbeiten, Suchen/Ersetzen und in der Live-Vorschau:

• Schnelleres Tippen, Scrollen und Bearbeiten großer Dateien: Texteingabe, Cursorbewegung und Bildlauf wurden spürbar beschleunigt, sodass auch große Dateien jederzeit flüssig reagieren.
• Ultraschnelles Suchen & „Alle ersetzen“: Stichwortsuche, reguläre Ausdrücke und „Alle ersetzen“ bei tausenden Treffern werden nun nahezu verzögerungsfrei ausgeführt.
• Schnellere Live-Vorschau für Markdown, LaTeX & Mermaid: Die Vorschau-Darstellung für lange Markdown-Dokumente, LaTeX-Formeln und Mermaid-Diagramme reagiert beim Schreiben noch schneller."""

FR_DESC = """miu est un éditeur de texte et de code simple et léger, conçu pour rendre aussi agréables la prise de notes quotidienne, la rédaction en Markdown et la programmation.

Pensé pour s’ouvrir instantanément et offrir une interface épurée et reposante pour les yeux, miu procure une expérience d’écriture fluide et naturelle sur iPhone, iPad et Mac.

[Caractéristiques principales]

• Rapide et réactif dès l’ouverture
Ouvrez l’application et commencez à écrire sans attendre. Qu’il s’agisse d’une note rapide ou d’un fichier volumineux, miu reste fluide en toute circonstance.

• Agréable pour rédiger comme pour coder
Prend en charge la coloration syntaxique pour 49 catégories de langages (plus de 120 extensions de fichiers), dont HTML, CSS, JavaScript, Python, Swift, C/C++, Markdown et JSON. Le support intégré d’Emmet permet également de développer rapidement vos abréviations HTML et CSS.

• Aperçu en direct intégré
Prévisualisez vos documents Markdown, HTML, SVG, ainsi que vos diagrammes Mermaid et formules mathématiques LaTeX en temps réel, entièrement hors ligne.

• Détection automatique de l’encodage
Reconnaît automatiquement les encodages UTF-8, Shift_JIS, EUC-JP, UTF-16 et bien d’autres afin d’éviter tout problème d’affichage des caractères.

• Des outils d’édition soignés
Rechercher et remplacer (avec expressions régulières), édition multi-curseur, numéros de ligne, retour à la ligne automatique, ainsi que les modes Clair et Sombre.

• Respect total de votre vie privée
Aucune publicité, aucun suivi et aucune collecte de données. Tout fonctionne à 100 % hors ligne sur votre appareil.

Comme bloc-notes de tous les jours ou comme éditeur de code fiable, nous espérons que miu rendra vos moments d’écriture plus agréables."""

FR_WHATS_NEW = """Merci d’utiliser miu.
La version 1.0.47 apporte une accélération majeure de l’édition, de la recherche/remplacement et de l’aperçu en direct :

• Frappe, défilement et édition de gros fichiers plus rapides : la saisie de texte, le déplacement du curseur et le défilement ont été considérablement accélérés pour offrir une fluidité optimale, même sur de très grands fichiers.
• Recherche et « Tout remplacer » ultra-rapides : les recherches par mot-clé ou expression régulière ainsi que le remplacement massif de milliers d’occurrences s’exécutent désormais instantanément.
• Aperçu en direct Markdown, LaTeX et Mermaid plus réactif : le rendu en temps réel des longs documents Markdown, formules LaTeX et diagrammes Mermaid est encore plus rapide pendant la frappe."""

ES_DESC = """miu es un editor de texto y código sencillo y ligero, pensado para que desde tus notas rápidas del día a día hasta la escritura en Markdown y la programación resulten cómodas y agradables.

Diseñado para abrirse al instante y ofrecer una interfaz limpia que descansa la vista, miu brinda una experiencia de escritura natural y fluida en iPhone, iPad y Mac.

[Características principales]

• Rápido y ligero en todo momento
Abre la aplicación y empieza a escribir al instante. Tanto si apuntas una idea rápida como si editas un archivo de texto grande, miu responde con total fluidez.

• Cómodo para escribir y programar
Compatible con resaltado de sintaxis para 49 categorías de lenguajes (más de 120 extensiones de archivo), incluyendo HTML, CSS, JavaScript, Python, Swift, C/C++, Markdown y JSON. Además, cuenta con soporte integrado para expandir abreviaturas Emmet en HTML y CSS.

• Vista previa en tiempo real
Visualiza al instante documentos Markdown, HTML, SVG, diagramas Mermaid y fórmulas matemáticas en LaTeX sin necesidad de conexión a internet.

• Detección automática de codificación
Reconoce automáticamente UTF-8, Shift_JIS, EUC-JP, UTF-16 y otras codificaciones internacionales para abrir tus archivos sin caracteres dañados.

• Herramientas de edición cuidadas al detalle
Incluye búsqueda y reemplazo (con expresiones regulares), edición multicursor, números de línea, ajuste de línea y soporte completo para modo claro y modo oscuro.

• Privacidad ante todo
Sin anuncios, sin rastreo y sin envío de datos. Todo funciona 100 % sin conexión directamente en tu dispositivo.

Ya sea como tu bloc de notas diario o como un editor de código de confianza, esperamos que miu haga más agradable tu tiempo de escritura."""

ES_WHATS_NEW = """Gracias por usar miu.
La versión 1.0.47 ofrece una gran mejora de velocidad en la edición, búsqueda/reemplazo y vista previa en vivo:

• Escritura, desplazamiento y edición de archivos grandes más rápidos: se aceleró notablemente la entrada de texto, el movimiento del cursor y el desplazamiento para que incluso los archivos grandes respondan con total fluidez.
• Búsqueda y «Reemplazar todo» ultrarrápidos: la búsqueda de texto, expresiones regulares y el reemplazo masivo de miles de coincidencias ahora se completan al instante.
• Vista previa en vivo más rápida para Markdown, LaTeX y Mermaid: se optimizó la visualización en tiempo real de documentos Markdown extensos, fórmulas LaTeX y diagramas Mermaid."""

IT_DESC = """miu è un editor di testo e codice semplice e leggero, pensato per rendere piacevole ogni attività: dagli appunti quotidiani alla scrittura in Markdown, fino alla programmazione.

Progettato per avviarsi all’istante e offrire un’interfaccia pulita e riposante per gli occhi, miu garantisce un’esperienza di scrittura naturale e fluida su iPhone, iPad e Mac.

[Caratteristiche principali]

• Veloce e pronto all’uso
Apri l’app e inizia subito a scrivere. Che si tratti di un appunto veloce o di un file di testo di grandi dimensioni, miu rimane sempre fluido e reattivo.

• Ideale per scrivere e programmare
Supporta l’evidenziazione della sintassi per 49 categorie di linguaggi (oltre 120 estensioni di file), tra cui HTML, CSS, JavaScript, Python, Swift, C/C++, Markdown e JSON, con supporto Emmet integrato per espandere rapidamente le abbreviazioni HTML e CSS.

• Anteprima dal vivo integrata
Visualizza in tempo reale Markdown, HTML, SVG, diagrammi Mermaid e formule matematiche LaTeX, completamente offline.

• Rilevamento automatico della codifica
Riconosce automaticamente UTF-8, Shift_JIS, EUC-JP, UTF-16 e altre codifiche internazionali per evitare caratteri illeggibili.

• Strumenti di modifica curati
Include Trova e sostituisci (con espressioni regolari), modifica multi-cursore, numeri di riga, a capo automatico e supporto per la modalità Chiara e Scura.

• Massima riservatezza
Nessuna pubblicità, nessun tracciamento e nessuna raccolta dati. Tutto funziona al 100% offline sul tuo dispositivo.

Come blocco note quotidiano o come affidabile editor di codice, speriamo che miu renda più piacevole il tuo tempo dedicato alla scrittura."""

IT_WHATS_NEW = """Grazie per aver scelto miu.
La versione 1.0.47 offre un notevole aumento di velocità per modifica, trova/sostituisci e anteprima dal vivo:

• Digitazione, scorrimento e modifica di file grandi più veloci: l’inserimento del testo, il movimento del cursore e lo scorrimento sono stati velocizzati per offrire la massima fluidità anche sui file più grandi.
• Ricerca e «Sostituisci tutto» ultraveloci: la ricerca per parola chiave o espressione regolare e la sostituzione simultanea di migliaia di occorrenze vengono ora completate all’istante.
• Anteprima live più rapida per Markdown, LaTeX e Mermaid: rendering in tempo reale ancora più reattivo per lunghi documenti Markdown, formule LaTeX e diagrammi Mermaid."""

PT_DESC = """O miu é um editor de texto e código simples e leve, criado para tornar desde as suas anotações diárias até a escrita em Markdown e a programação muito mais confortáveis.

Desenvolvido com foco em uma inicialização instantânea e um visual limpo que não cansa a vista, o miu oferece uma experiência de escrita fluida e natural no iPhone, iPad e Mac.

[Principais Características]

• Rápido e sempre pronto
Abra o aplicativo e comece a digitar imediatamente. Seja para anotar uma ideia rápida ou editar arquivos grandes, o miu funciona sempre com leveza.

• Ótimo para escrever e programar
Compatível com destaque de sintaxe para 49 categorias de linguagens (mais de 120 extensões de arquivo), incluindo HTML, CSS, JavaScript, Python, Swift, C/C++, Markdown e JSON. Conta também com suporte integrado ao Emmet para expandir abreviações HTML e CSS.

• Pré-visualização em tempo real
Visualize documentos Markdown, HTML, SVG, além de diagramas Mermaid e fórmulas matemáticas em LaTeX na hora, totalmente offline.

• Detecção automática de codificação
Reconhece automaticamente UTF-8, Shift_JIS, EUC-JP, UTF-16 e outras codificações para abrir seus arquivos sem erros de caracteres.

• Recursos de edição cuidadosos
Inclui Localizar e Substituir (com expressões regulares), edição com múltiplos cursores, números de linha, quebra automática de linha e suporte aos modos Claro e Escuro.

• Privacidade em primeiro lugar
Sem anúncios, sem rastreamento e sem coleta de dados. Tudo funciona 100% offline no seu dispositivo.

Seja como seu bloco de notas do dia a dia ou como um editor de código confiável, esperamos que o miu torne o seu momento de escrever ainda mais agradável."""

PT_WHATS_NEW = """Obrigado por usar o miu.
A versão 1.0.47 traz um grande ganho de velocidade na edição, localização/substituição e pré-visualização em tempo real:

• Digitação, rolagem e edição de arquivos grandes mais rápidas: a entrada de texto, a movimentação do cursor e a rolagem foram aceleradas para garantir máxima fluidez mesmo em arquivos extensos.
• Localizar e «Substituir tudo» ultrarrápidos: a busca por palavras-chave, expressões regulares e a substituição em massa de milhares de ocorrências agora são concluídas instantaneamente.
• Pré-visualização em tempo real mais rápida para Markdown, LaTeX e Mermaid: renderização ainda mais ágil para documentos Markdown longos, fórmulas LaTeX e diagramas Mermaid."""

RU_DESC = """miu — это простой и лёгкий текстовый и кодовый редактор, созданный для комфортной работы: от быстрых повседневных заметок и текстов в Markdown до программирования.

Мгновенный запуск и чистый, приятный для глаз интерфейс делают работу с текстом естественной и удобной на iPhone, iPad и Mac.

[Основные возможности]

• Быстрая и плавная работа
Откройте приложение и сразу начинайте писать. Будь то короткая заметка или большой текстовый файл, miu работает легко и отзывчиво.

• Удобно для текста и кода
Поддерживается подсветка синтаксиса для 49 категорий языков (более 120 расширений файлов), включая HTML, CSS, JavaScript, Python, Swift, C/C++, Markdown и JSON, а также быстрое разворачивание сокращений HTML и CSS с помощью Emmet.

• Встроенный предпросмотр
Просматривайте Markdown, HTML, SVG, диаграммы Mermaid и математические формулы LaTeX прямо во время работы — полностью офлайн.

• Автоматическое определение кодировки
Надёжно распознаёт UTF-8, Shift_JIS, EUC-JP, UTF-16 и другие кодировки, предотвращая появление нечитаемых символов.

• Продуманные инструменты редактирования
Поиск и замена (с поддержкой регулярных выражений), мультикурсорное редактирование, номера строк, перенос по словам, а также светлая и тёмная темы оформления.

• Полная конфиденциальность
Никакой рекламы, отслеживания или передачи данных. Все функции работают на 100 % локально на вашем устройстве.

В качестве ежедневного блокнота или надёжного редактора кода — надеемся, что miu сделает вашу работу с текстом ещё приятнее."""

RU_WHATS_NEW = """Спасибо, что пользуетесь miu.
В версии 1.0.47 значительно ускорены редактирование, поиск с заменой и живой предпросмотр:

• Более быстрый ввод, прокрутка и работа с большими файлами: ускорены набор текста, перемещение курсора и прокрутка — даже большие файлы и длительные сессии редактирования работают максимально плавно.
• Мгновенный поиск и «Заменить все»: поиск по тексту, регулярным выражениям и массовая замена тысяч совпадений теперь выполняются мгновенно.
• Ускоренный предпросмотр Markdown, формул LaTeX и диаграмм Mermaid: оптимизирована отрисовка предпросмотра в реальном времени для длинных документов Markdown, математических формул и диаграмм."""

NL_DESC = """miu is een eenvoudige, lichtgewicht tekst- en code-editor waarmee u prettig werkt aan dagelijkse notities, Markdown-documenten en programmeerprojecten.

Ontworpen om direct op te starten en voorzien van een rustige, overzichtelijke interface, biedt miu een natuurlijke en soepele schrijfervaring op iPhone, iPad en Mac.

[Belangrijkste kenmerken]

• Direct klaar voor gebruik
Open de app en begin meteen met typen. Of het nu gaat om een korte aantekening of een groot tekstbestand, miu blijft snel en soepel.

• Prettig voor zowel tekst als code
Ondersteunt syntaxisaccentuering voor 49 categorieën (meer dan 120 bestandsextensies), waaronder HTML, CSS, JavaScript, Python, Swift, C/C++, Markdown en JSON, inclusief ingebouwde Emmet-ondersteuning.

• Ingebouwde live-voorvertoning
Bekijk Markdown, HTML, SVG, Mermaid-diagrammen en LaTeX-formules direct naast uw tekst, volledig offline.

• Automatische tekensetherkenning
Herkennt automatisch UTF-8, Shift_JIS, EUC-JP, UTF-16 en andere coderingen om onleesbare tekens te voorkomen.

• Doordachte bewerkingsfuncties
Voorzien van Zoeken en vervangen (met reguliere expressies), meerdere cursors, regelnummers, tekstterugloop en ondersteuning voor zowel de lichte als donkere modus.

• Uw privacy voorop
Geen advertenties, geen tracking en geen gegevensverzameling. Alles werkt 100% offline op uw apparaat.

Of u miu nu gebruikt als dagelijks notitieblok of als betrouwbare code-editor, we hopen dat het uw schrijfmomenten net wat aangenamer maakt."""

NL_WHATS_NEW = """Bedankt voor het gebruiken van miu.
Versie 1.0.47 biedt grote snelheidsverbeteringen voor bewerken, zoeken/vervangen en de live-voorvertoning:

• Sneller typen, scrollen en grote bestanden bewerken: tekstinvoer, cursorbeweging en scrollen zijn aanzienlijk versneld voor een vloeiende ervaring, zelfs in grote bestanden.
• Razendsnel zoeken en ‘Alles vervangen’: zoekopdrachten, reguliere expressies en het in één keer vervangen van duizenden overeenkomsten worden nu direct uitgevoerd.
• Snellere live-voorvertoning voor Markdown, LaTeX en Mermaid: de weergave van lange Markdown-documenten, LaTeX-formules en Mermaid-diagrammen reageert nog sneller tijdens het typen."""

SV_DESC = """miu är en enkel och lättviktig text- och kodredigerare som gör allt från dagliga anteckningar till Markdown-skrivande och programmering bekvämt och smidigt.

Med omedelbar start och ett rent, behagligt gränssnitt ger miu en naturlig skrivupplevelse på iPhone, iPad och Mac.

[Huvudfunktioner]

• Snabb och följsam
Öppna appen och börja skriva direkt. Oavsett om du skriver en kort anteckning eller redigerar en stor textfil är miu alltid lika snabb.

• Tydligt för både text och kod
Stöder syntaxmarkering för 49 kategorier (över 120 filändelser), inklusive HTML, CSS, JavaScript, Python, Swift, C/C++, Markdown och JSON, samt inbyggt stöd för Emmet-förkortningar.

• Förhandsvisning i realtid
Förhandsgranska Markdown, HTML, SVG, Mermaid-diagram och matematiska LaTeX-formler direkt i appen – helt offline.

• Automatisk teckenkodning
Känner automatiskt igen UTF-8, Shift_JIS, EUC-JP, UTF-16 och andra kodningar så att texten alltid visas korrekt.

• Smidiga redigeringsverktyg
Sök och ersätt (med reguljära uttryck), multimarkör, radnummer, radbrytning samt fullt stöd för ljust och mörkt läge.

• Integritet i fokus
Inga annonser, ingen spårning och ingen datainsamling. Allt fungerar 100 % offline på din enhet.

Som dagligt anteckningsblock eller pålitlig kodredigerare – vi hoppas att miu gör din skrivstund lite trevligare."""

SV_WHATS_NEW = """Tack för att du använder miu.
Version 1.0.47 ger stora hastighetsförbättringar för redigering, sök/ersätt och förhandsvisning i realtid:

• Snabbare skrivning, rullning och redigering av stora filer: textinmatning, markörförflyttning och rullning har snabbats upp rejält så att även stora filer känns följsamma och snabba.
• Blixtsnabb sökning och ”Ersätt alla”: sökning med sökord eller reguljära uttryck samt ”Ersätt alla” för tusentals träffar slutförs nu omedelbart.
• Snabbare förhandsvisning för Markdown, LaTeX och Mermaid: långa Markdown-dokument, LaTeX-formler och Mermaid-diagram ritas upp ännu snabbare medan du skriver."""

DA_DESC = """miu er en enkel og let tekst- og kodeeditor, der gør alt fra daglige noter til Markdown-skrivning og programmering behageligt og ubesværet.

Designet til at åbne øjeblikkeligt og med en ren, rolig brugerflade giver miu en naturlig skriveoplevelse på iPhone, iPad og Mac.

[Hovedfunktioner]

• Hurtig og klar med det samme
Åbn appen, og begynd at skrive med det samme. Uanset om du tager en hurtig note eller redigerer en stor tekstfil, føles miu altid let og hurtig.

• Behagelig til både tekst og kode
Understøtter syntaksfremhævning for 49 kategorier (over 120 filendelser), herunder HTML, CSS, JavaScript, Python, Swift, C/C++, Markdown og JSON, samt indbygget Emmet-udvidelse.

• Indbygget live-forhåndsvisning
Se Markdown, HTML, SVG, Mermaid-diagrammer og matematiske LaTeX-formler direkte ved siden af din tekst – helt offline.

• Automatisk tegnsætregistrering
Genkender automatisk UTF-8, Shift_JIS, EUC-JP, UTF-16 og andre tegnsæt, så dine filer åbnes uden tegnfejl.

• Gennemtænkte redigeringsværktøjer
Søg og erstat (med regulære udtryk), multimarkør, linjenumre, tekstombrydning samt lys og mørk tilstand.

• Privatliv i centrum
Ingen reklamer, ingen sporing og ingen dataindsamling. Alt fungerer 100 % offline på din enhed.

Uanset om du bruger miu som daglig notesblok eller som kodeeditor, håber vi, at den gør din skrivetid lidt mere behagelig."""

DA_WHATS_NEW = """Tak fordi du bruger miu.
Version 1.0.47 giver markant højere hastighed ved redigering, søg/erstat og live-forhåndsvisning:

• Hurtigere indtastning, rulning og redigering af store filer: tekstindtastning, markørbevægelse og rulning er optimeret, så selv store filer føles hurtige og flydende.
• Lynhurtig søgning og "Erstat alle": søgning med søgeord eller regulære udtryk samt "Erstat alle" på tværs af tusindvis af forekomster udføres nu øjeblikkeligt.
• Hurtigere live-forhåndsvisning af Markdown, LaTeX og Mermaid: lange Markdown-dokumenter, LaTeX-formler og Mermaid-diagrammer opdateres endnu hurtigere, mens du skriver."""

NO_DESC = """miu er et enkelt og lett tekst- og koderedigeringsprogram som gjør alt fra daglige notater til Markdown-skriving og programmering behagelig og uanstrengt.

Med øyeblikkelig oppstart og et rent, øyevennlig grensesnitt gir miu en naturlig skriveopplevelse på iPhone, iPad og Mac.

[Hovedfunksjoner]

• Rask og responsiv
Åpne appen og begynn å skrive med det samme. Enten du noterer en rask tanke eller redigerer en stor tekstfil, er miu alltid jevn og rask.

• Oversiktlig for både skriving og koding
Støtter syntaksutheving for 49 kategorier (over 120 filtyper), inkludert HTML, CSS, JavaScript, Python, Swift, C/C++, Markdown og JSON, samt innebygd Emmet-støtte.

• Innebygd forhåndsvisning i sanntid
Forhåndsvis Markdown, HTML, SVG, Mermaid-diagrammer og LaTeX-formler direkte i appen – helt offline.

• Automatisk tegnsettgjenkjenning
Gjenkjenner automatisk UTF-8, Shift_JIS, EUC-JP, UTF-16 og andre tegnsett slik at filene dine åpnes uten tegnfeil.

• Gjennomtenkte redigeringsverktøy
Søk og erstatt (med regulære uttrykk), multimarkør, linjenumre, tekstbryting samt støtte for lys og mørk modus.

• Personvern først
Ingen reklame, ingen sporing og ingen datainnsamling. Alt fungerer 100 % offline på enheten din.

Enten du bruker miu som daglig notisblokk eller som en pålitelig kodeeditor, håper vi den gjør skrivetiden din litt hyggeligere."""

NO_WHATS_NEW = """Takk for at du bruker miu.
Versjon 1.0.47 gir betydelig høyere hastighet for redigering, søk/erstatt og forhåndsvisning i sanntid:

• Raskere skriving, rulling og redigering av store filer: tekstinnlasting, markørbevegelse og rulling er kraftig akselerert slik at selv store filer oppleves raske og jevne.
• Lynraskt søk og «Erstatt alle»: nøkkelordsøk, regulære uttrykk og «Erstatt alle» for tusenvis av treff fullføres nå umiddelbart.
• Raskere sanntidsvisning for Markdown, LaTeX og Mermaid: lange Markdown-dokumenter, LaTeX-formler og Mermaid-diagrammer oppdateres enda raskere mens du skriver."""

FI_DESC = """miu on yksinkertainen ja kevyt teksti- ja koodieditori, joka tekee päivittäisistä muistiinpanoista, Markdown-kirjoittamisesta ja ohjelmoinnista miellyttävää ja vaivatonta.

Välittömästi käynnistyvä ja ulkoasultaan rauhallinen miu tarjoaa luonnollisen kirjoituskokemuksen iPhonella, iPadilla ja Macilla.

[Tärkeimmät ominaisuudet]

• Nopea ja kevyt käyttää
Avaa sovellus ja aloita kirjoittaminen heti. Olipa kyseessä lyhyt muistiinpano tai suuri tekstitiedosto, miu toimii aina sujuvasti.

• Selkeä sekä kirjoittamiseen että koodaukseen
Tukee 49 kategorian (yli 120 tiedostopäätteen) syntaksinkorostusta, mukaan lukien HTML, CSS, JavaScript, Python, Swift, C/C++, Markdown ja JSON, sekä sisäänrakennettua Emmet-lyhenteiden laajennusta.

• Reaaliaikainen esikatselu
Esikatsele Markdown-, HTML- ja SVG-tiedostoja sekä Mermaid-kaavioita ja LaTeX-kaavoja suoraan tekstin rinnalla – täysin offline-tilassa.

• Automaattinen merkistöntunnistus
Tunnistaa automaattisesti UTF-8-, Shift_JIS-, EUC-JP-, UTF-16- ja muut merkistöt, jotta tiedostot avautuvat oikein.

• Huolellisesti suunnitellut muokkaustyökalut
Etsi ja korvaa (säännöllisillä lausekkeilla), monikohdistinmuokkaus, rivinumerot, rivitys sekä vaalea ja tumma tila.

• Yksityisyys etusijalla
Ei mainoksia, ei seurantaa eikä tiedonkeruuta. Kaikki toimii 100 % offline-tilassa laitteellasi.

Käytitpä miuta päivittäisenä muistiona tai luotettavana koodieditorina, toivomme sen tekevän kirjoitushetkistäsi hieman mukavampia."""

FI_WHATS_NEW = """Kiitos, että käytät miuta.
Versio 1.0.47 tuo merkittävästi lisää nopeutta muokkaukseen, etsi/korvaa-toimintoihin ja reaaliaikaiseen esikatseluun:

• Nopeampi kirjoitus, vieritys ja suurten tiedostojen muokkaus: tekstinsyöttöä, kohdistimen liikettä ja vieritystä on nopeutettu huomattavasti, jotta suuretkin tiedostot toimivat sulavasti.
• Salamannopea haku ja ”Korvaa kaikki”: avainsana- ja regex-haku sekä tuhansien osumien ”Korvaa kaikki” -toiminto valmistuvat nyt välittömästi.
• Nopeampi Markdown-, LaTeX- ja Mermaid-esikatselu: pitkien Markdown-asiakirjojen, LaTeX-kaavojen ja Mermaid-kaavioiden reaaliaikainen esikatselu päivittyy entistä nopeammin kirjoittaessasi."""

PL_DESC = """miu to prosty, lekki edytor tekstu i kodu, zaprojektowany tak, aby codzienne notatki, pisanie w Markdown oraz programowanie były wygodne i przyjemne.

Dzięki błyskawicznemu uruchamianiu oraz przejrzystemu, przyjaznemu dla oczu interfejsowi, miu zapewnia naturalny komfort pisania na iPhonie, iPadzie oraz komputerze Mac.

[Główne cechy]

• Szybki i zawsze gotowy do pracy
Otwórz aplikację i od razu zacznij pisać. Niezależnie od tego, czy zapisujesz krótką myśl, czy edytujesz duży plik tekstowy, miu działa płynnie i lekko.

• Czytelny podczas pisania tekstu i kodu
Obsługuje kolorowanie składni dla 49 kategorii języków (ponad 120 rozszerzeń plików), w tym HTML, CSS, JavaScript, Python, Swift, C/C++, Markdown i JSON, a także wbudowane rozwijanie skrótów Emmet.

• Podgląd na żywo
Przeglądaj dokumenty Markdown, HTML, SVG oraz diagramy Mermaid i wzory matematyczne LaTeX bezpośrednio w aplikacji – całkowicie offline.

• Automatyczne wykrywanie kodowania znaków
Automatycznie rozpoznaje UTF-8, Shift_JIS, EUC-JP, UTF-16 oraz inne kodowania, zapobiegając błędnemu wyświetlaniu znaków.

• Starannie dobrane narzędzia edycji
Wyszukiwanie i zamiana (z obsługą wyrażeń regularnych), edycja wielokursorowa, numery wierszy, zawijanie wierszy oraz obsługa trybu jasnego i ciemnego.

• Pełna prywatność
Brak reklam, brak śledzenia i brak gromadzenia danych. Wszystko działa w 100% lokalnie na Twoim urządzeniu.

Zarówno jako codzienny notatnik, jak i niezawodny edytor kodu – mamy nadzieję, że miu uprzyjemni Twój czas spędzony na pisaniu."""

PL_WHATS_NEW = """Dziękujemy za korzystanie z aplikacji miu.
Wersja 1.0.47 przynosi znaczące przyspieszenie edycji, wyszukiwania/zamiany oraz podglądu na żywo:

• Szybsze pisanie, przewijanie i edycja dużych plików: przyspieszono wprowadzanie tekstu, ruch kursora oraz przewijanie, dzięki czemu nawet duże pliki działają wyjątkowo płynnie.
• Błyskawiczne wyszukiwanie i „Zamień wszystko”: wyszukiwanie słów kluczowych, wyrażeń regularnych oraz masowa zamiana tysięcy wystąpień wykonują się teraz natychmiastowo.
• Szybszy podgląd na żywo dla Markdown, LaTeX i Mermaid: zoptymalizowano renderowanie długich dokumentów Markdown, wzorów LaTeX oraz diagramów Mermaid podczas pisania."""

CS_DESC = """miu je jednoduchý a lehký textový a kódový editor, se kterým je psaní každodenních poznámek, textů v Markdownu i programování pohodlné a příjemné.

Díky okamžitému spuštění a čistému, pro oči šetrnému rozhraní nabízí miu přirozený a plynulý zážitek z psaní na iPhonu, iPadu i Macu.

[Hlavní vlastnosti]

• Rychlý a okamžitě připravený
Otevřete aplikaci a můžete ihned psát. Ať už si zapisujete rychlou poznámku nebo upravujete velký textový soubor, miu reaguje lehce a plynule.

• Přehledné psaní textu i kódu
Podporuje zvýrazňování syntaxe pro 49 kategorií jazyků (přes 120 přípon souborů), včetně HTML, CSS, JavaScriptu, Pythonu, Swiftu, C/C++, Markdownu a JSONu, a také vestavěné rozbalování zkratek Emmet.

• Živý náhled přímo v aplikaci
Zobrazujte si náhled dokumentů Markdown, HTML, SVG, diagramů Mermaid a matematických vzorců LaTeX v reálném čase – zcela offline.

• Automatické rozpoznání kódování
Automaticky rozpozná UTF-8, Shift_JIS, EUC-JP, UTF-16 a další kódování, aby se text vždy zobrazil správně.

• Promyšlené nástroje pro úpravy
Hledání a nahrazování (s regulárními výrazy), více kurzorů, čísla řádků, zalamování řádků a plná podpora světlého i tmavého režimu.

• Soukromí na prvním místě
Žádné reklamy, žádné sledování a žádný sběr dat. Vše funguje 100% offline přímo ve vašem zařízení.

Ať už miu používáte jako každodenní zápisník nebo jako spolehlivý editor kódu, věříme, že vám zpříjemní každou chvíli u psaní."""

CS_WHATS_NEW = """Děkujeme, že používáte miu.
Verze 1.0.47 přináší výrazné zrychlení úprav, vyhledávání/nahrazování a živého náhledu:

• Rychlejší psaní, posouvání a úpravy velkých souborů: zadávání textu, pohyb kurzoru a posouvání byly výrazně zrychleny pro maximálně plynulou práci i s velkými soubory.
• Bleskové vyhledávání a „Nahradit vše“: vyhledávání textu, regulárních výrazů i hromadné nahrazení tisíců výskytů nyní proběhne okamžitě.
• Rychlejší živý náhled pro Markdown, LaTeX a Mermaid: zrychlené vykreslování dlouhých dokumentů Markdown, vzorců LaTeX a diagramů Mermaid v reálném čase."""

HU_DESC = """A miu egy egyszerű, könnyed szöveg- és kódszerkesztő, amely a napi gyors jegyzeteléstől a Markdown-szövegíráson át a programozásig minden helyzetben kényelmes használatot nyújt.

Az azonnali indulásra és a letisztult, szemkímélő megjelenésre tervezett miu természetes és gördülékeny írásélményt biztosít iPhone, iPad és Mac készülékeken.

[Főbb jellemzők]

• Gyors és azonnal használatra kész
Nyissa meg az alkalmazást, és máris kezdhet gépelni. Legyen szó rövid feljegyzésről vagy nagyobb szövegfájlról, a miu mindig folyamatosan működik.

• Átlátható szövegírás és kódolás
49 kategóriában (több mint 120 fájlkiterjesztéshez) támogatja a szintaxiskiemelést – többek között HTML, CSS, JavaScript, Python, Swift, C/C++, Markdown és JSON fájlokhoz –, beépített Emmet-rövidítéskibontással.

• Beépített élő előnézet
Tekintse meg a Markdown, HTML és SVG dokumentumokat, valamint a Mermaid diagramokat és LaTeX matematikai képleteket közvetlenül az alkalmazásban, teljesen offline.

• Automatikus karakterkódolás-felismerés
Automatikusan felismeri az UTF-8, Shift_JIS, EUC-JP, UTF-16 és más kódolásokat, így elkerülhetők a hibásan megjelenő karakterek.

• Gondosan kialakított szerkesztőeszközök
Keresés és csere (reguláris kifejezésekkel), többkurzoros szerkesztés, sorszámozás, sortörés, valamint világos és sötét mód.

• Teljes adatvédelem
Nincsenek reklámok, nincs követés és nincs adatgyűjtés. Minden funkció 100%-ban offline, az Ön készülékén működik.

Akár mindennapi jegyzettömbként, akár megbízható kódszerkesztőként használja, reméljük, hogy a miu kellemesebbé teszi az írással töltött időt."""

HU_WHATS_NEW = """Köszönjük, hogy a miu alkalmazást használja.
Az 1.0.47-es verzió jelentős sebességnövekedést hoz a szerkesztés, a keresés/csere és az élő előnézet terén:

• Gyorsabb gépelés, görgetés és nagy fájlok szerkesztése: a szövegbevitel, a kurzormozgatás és a görgetés sokkal gyorsabb lett, így a nagy fájlok szerkesztése is rendkívül folyamatos.
• Villámgyors keresés és „Összes cseréje”: a kulcsszavas és reguláris kifejezéses keresés, valamint több ezer találat egyidejű cseréje mostantól pillanatok alatt elkészül.
• Gyorsabb Markdown, LaTeX és Mermaid élő előnézet: a hosszú Markdown-dokumentumok, LaTeX-képletek és Mermaid-diagramok valós idejű megjelenítése még gyorsabb gépelés közben."""

RO_DESC = """miu este un editor de text și cod simplu și ușor, creat pentru ca notițele zilnice, redactarea în Markdown și programarea să fie cât mai plăcute și confortabile.

Conceput să pornească instantaneu și să ofere o interfață curată, odihnitoare pentru ochi, miu oferă o experiență de scriere naturală și fluidă pe iPhone, iPad și Mac.

[Caracteristici principale]

• Rapid și mereu pregătit
Deschideți aplicația și începeți să scrieți imediat. Fie că notați o idee rapidă sau editați un fișier text mare, miu răspunde rapid și fără întreruperi.

• Claritate atât pentru text, cât și pentru cod
Suportă evidențierea sintaxei pentru 49 de categorii de limbaje (peste 120 de extensii de fișiere), inclusiv HTML, CSS, JavaScript, Python, Swift, C/C++, Markdown și JSON, plus extinderea abreviunilor Emmet.

• Previzualizare în timp real
Previzualizați documente Markdown, HTML, SVG, diagrame Mermaid și formule matematice LaTeX direct în aplicație, complet offline.

• Detectare automată a codificării caracterelor
Recunoaște automat UTF-8, Shift_JIS, EUC-JP, UTF-16 și alte codificări pentru a afișa textul corect de fiecare dată.

• Instrumente de editare atent concepute
Căutare și înlocuire (cu expresii regulate), editare cu cursoare multiple, numere de rând, încadrarea textului, precum și suport pentru modurile Luminos și Întunecat.

• Confidențialitate deplină
Fără reclame, fără urmărire și fără colectare de date. Totul funcționează 100% offline pe dispozitivul dumneavoastră.

Fie ca agendă de zi cu zi, fie ca editor de cod de încredere, sperăm că miu vă va face timpul petrecut scriind mai plăcut."""

RO_WHATS_NEW = """Vă mulțumim că folosiți miu.
Versiunea 1.0.47 aduce o creștere majoră a vitezei pentru editare, căutare/înlocuire și previzualizarea în timp real:

• Tastare, derulare și editare mai rapidă a fișierelor mari: introducerea textului, mișcarea cursorului și derularea au fost accelerate semnificativ pentru o experiență fluidă chiar și în fișierele mari.
• Căutare și „Înlocuiește tot” ultra-rapide: căutarea cuvintelor cheie, expresiilor regulate și înlocuirea în masă a mii de apariții se finalizează acum instantaneu.
• Previzualizare în timp real mai rapidă pentru Markdown, LaTeX și Mermaid: redarea documentelor Markdown lungi, formulelor LaTeX și diagramelor Mermaid este și mai rapidă în timp ce tastați."""

EL_DESC = """Το miu είναι ένας απλός και ελαφρύς επεξεργαστής κειμένου και κώδικα, σχεδιασμένος ώστε οι καθημερινές σημειώσεις, η συγγραφή σε Markdown και ο προγραμματισμός να γίνονται άνετα και ευχάριστα.

Με άμεση εκκίνηση και καθαρό, ξεκούραστο για τα μάτια περιβάλλον, το miu προσφέρει μια φυσική και ομαλή εμπειρία γραφής σε iPhone, iPad και Mac.

[Κύρια χαρακτηριστικά]

• Γρήγορο και άμεσα έτοιμο
Ανοίξτε την εφαρμογή και ξεκινήστε αμέσως να γράφετε. Είτε κρατάτε μια γρήγορη σημείωση είτε επεξεργάζεστε ένα μεγάλο αρχείο, το miu παραμένει γρήγορο και ομαλό.

• Ευανάγνωστο για κείμενο και κώδικα
Υποστηρίζει επισήμανση σύνταξης για 49 κατηγορίες γλωσσών (πάνω από 120 επεκτάσεις αρχείων), όπως HTML, CSS, JavaScript, Python, Swift, C/C++, Markdown και JSON, καθώς και ενσωματωμένη ανάπτυξη συντομογραφιών Emmet.

• Ενσωματωμένη ζωντανή προεπισκόπηση
Δείτε σε πραγματικό χρόνο έγγραφα Markdown, HTML, SVG, διαγράμματα Mermaid και μαθηματικούς τύπους LaTeX, εντελώς εκτός σύνδεσης.

• Αυτόματη αναγνώριση κωδικοποίησης
Αναγνωρίζει αυτόματα UTF-8, Shift_JIS, EUC-JP, UTF-16 και άλλες κωδικοποιήσεις, αποτρέποντας την εμφάνιση αλλοιωμένων χαρακτήρων.

• Προσεγμένα εργαλεία επεξεργασίας
Αναζήτηση και αντικατάσταση (με κανονικές εκφράσεις), πολλαπλοί δρομείς, αριθμοί γραμμών, αναδίπλωση κειμένου, καθώς και Φωτεινή και Σκοτεινή λειτουργία.

• Απόλυτος σεβασμός στην ιδιωτικότητα
Χωρίς διαφημίσεις, χωρίς παρακολούθηση και χωρίς συλλογή δεδομένων. Όλα λειτουργούν 100% εκτός σύνδεσης στη συσκευή σας.

Είτε ως καθημερινό σημειωματάριο είτε ως αξιόπιστος επεξεργαστής κώδικα, ελπίζουμε το miu να κάνει τον χρόνο που αφιερώνετε στο γράψιμο πιο ευχάριστο."""

EL_WHATS_NEW = """Σας ευχαριστούμε που χρησιμοποιείτε το miu.
Η έκδοση 1.0.47 προσφέρει μεγάλη αύξηση ταχύτητας στην επεξεργασία, την εύρεση/αντικατάσταση και τη ζωντανή προεπισκόπηση:

• Ταχύτερη πληκτρολόγηση, κύλιση και επεξεργασία μεγάλων αρχείων: η εισαγωγή κειμένου, η κίνηση του δρομέα και η κύλιση επιταχύνθηκαν σημαντικά ώστε ακόμη και τα μεγάλα αρχεία να αποκρίνονται άμεσα.
• Εξαιρετικά γρήγορη αναζήτηση και «Αντικατάσταση όλων»: η αναζήτηση λέξεων-κλειδιών, εκφράσεων regex και η μαζική αντικατάσταση χιλιάδων αποτελεσμάτων ολοκληρώνονται πλέον ακαριαία.
• Ταχύτερη ζωντανή προεπισκόπηση για Markdown, LaTeX και Mermaid: η απόδοση μεγάλων εγγράφων Markdown, εξισώσεων LaTeX και διαγραμμάτων Mermaid ενημερώνεται ακόμη πιο γρήγορα καθώς πληκτρολογείτε."""

TR_DESC = """miu; günlük kısa notlardan Markdown yazılarına ve programlamaya kadar her türlü metin çalışmasını keyifli ve rahat hale getirmek için tasarlanmış sade ve hafif bir metin ve kod düzenleyicidir.

Uygulamayı açtığınız anda yazmaya başlayabileceğiniz hızı ve göz yormayan ferah arayüzüyle iPhone, iPad ve Mac'te doğal ve akıcı bir yazma deneyimi sunar.

[Öne Çıkan Özellikler]

• Hızlı ve Akıcı Çalışma
Uygulamayı açın ve hemen yazmaya başlayın. İster hızlı bir not alın ister büyük bir metin dosyasını düzenleyin, miu her zaman akıcı çalışır.

• Hem Yazı Hem Kod İçin Okunaklı
HTML, CSS, JavaScript, Python, Swift, C/C++, Markdown ve JSON dahil 49 dil kategorisinde (120'den fazla dosya uzantısı) sözdizimi vurgulamayı destekler. Dahili Emmet desteğiyle HTML ve CSS kısaltmalarını kolayca genişletebilirsiniz.

• Gerçek Zamanlı Canlı Önizleme
Markdown, HTML ve SVG belgelerinin yanı sıra Mermaid diyagramlarını ve LaTeX matematik formüllerini internet bağlantısına ihtiyaç duymadan anında önizleyin.

• Otomatik Karakter Kodlaması Algılama
UTF-8, Shift_JIS, EUC-JP, UTF-16 ve diğer karakter kodlamalarını otomatik olarak tanıyarak karakter bozulmalarını önler.

• Özenle Hazırlanmış Düzenleme Araçları
Bul ve Değiştir (düzenli ifade destekli), çoklu imleç, satır numaraları, satır kaydırma ve Açık/Koyu tema desteği sunar.

• Gizliliğe Saygılı Tasarım
Reklam, takip veya veri toplama yoktur. Tüm özellikler cihazınızda %100 çevrimdışı olarak çalışır.

İster günlük not defteriniz ister güvenilir kod düzenleyiciniz olarak, miu'nun yazma zamanınızı daha keyifli hale getirmesini dileriz."""

TR_WHATS_NEW = """miu'yu kullandığınız için teşekkür ederiz.
1.0.47 sürümü; düzenleme, arama/değiştirme ve Canlı Önizleme genelinde büyük hız artışları sunar:

• Daha Hızlı Yazma, Kaydırma ve Büyük Dosya Düzenleme: Metin girişi, imleç hareketi ve kaydırma önemli ölçüde hızlandırıldı; büyük dosyalarda bile akıcı ve hızlı yanıt verir.
• Ultra Hızlı Arama ve "Tümünü Değiştir": Anahtar kelime araması, düzenli ifadeler (regex) ve binlerce eşleşmeyi kapsayan "Tümünü Değiştir" işlemleri artık anında tamamlanır.
• Daha Hızlı Markdown, LaTeX ve Mermaid Canlı Önizlemesi: Uzun Markdown belgeleri, LaTeX formülleri ve Mermaid diyagramları siz yazarken çok daha hızlı görüntülenir."""

UK_DESC = """miu — це простий і легкий редактор тексту та коду, створений для того, щоб щоденні нотатки, написання текстів у Markdown і програмування були максимально зручними та приємними.

Миттєвий запуск і чистий, лагідний для очей інтерфейс забезпечують природну та плавну роботу з текстом на iPhone, iPad і Mac.

[Основні можливості]

• Швидка та легка робота
Відкрийте програму й одразу починайте писати. Чи то коротка нотатка, чи великий текстовий файл — miu працює швидко та плавно.

• Зручно для текстів і коду
Підтримується підсвічування синтаксису для 49 категорій мов (понад 120 розширень файлів), зокрема HTML, CSS, JavaScript, Python, Swift, C/C++, Markdown і JSON, а також розгортання скорочень HTML/CSS за допомогою Emmet.

• Вбудований попередній перегляд
Переглядайте документи Markdown, HTML, SVG, діаграми Mermaid і математичні формули LaTeX у реальному часі — повністю офлайн.

• Автоматичне визначення кодування
Автоматично розпізнає UTF-8, Shift_JIS, EUC-JP, UTF-16 та інші кодування, запобігаючи спотворенню символів.

• Продумані інструменти редагування
Пошук і заміна (з регулярними виразами), мультикурсорне редагування, номери рядків, перенесення рядків, а також світла й темна теми.

• Повна конфіденційність
Жодної реклами, відстеження чи збору даних. Усі функції працюють на 100 % офлайн на вашому пристрої.

Як щоденний записник або надійний редактор коду — сподіваємося, що miu зробить ваш час за письмом приємнішим."""

UK_WHATS_NEW = """Дякуємо, що користуєтеся miu.
У версії 1.0.47 значно прискорено редагування, пошук із заміною та живий попередній перегляд:

• Швидше введення тексту, прокручування та робота з великими файлами: прискорено набір тексту, переміщення курсора та прокручування для максимально плавної роботи навіть із великими файлами.
• Миттєвий пошук і «Замінити все»: пошук ключових слів, регулярних виразів і масова заміна тисяч збігів тепер виконуються миттєво.
• Швидший живий перегляд Markdown, LaTeX і Mermaid: оптимізовано відображення довгих документів Markdown, формул LaTeX і діаграм Mermaid під час набору тексту."""

VI_DESC = """miu là trình soạn thảo văn bản và mã nguồn đơn giản, gọn nhẹ, mang lại cảm giác thoải mái cho mọi nhu cầu từ ghi chú nhanh hàng ngày, viết tài liệu Markdown cho đến lập trình.

Được chăm chút để khởi động tức thì cùng giao diện tinh gọn, dịu mắt, miu đem đến trải nghiệm viết tự nhiên và mượt mà trên cả iPhone, iPad và Mac.

[Tính năng nổi bật]

• Nhanh nhẹn và mượt mà
Mở ứng dụng là có thể bắt đầu viết ngay. Dù là vài dòng ghi chú nhanh hay tệp văn bản dung lượng lớn, miu luôn phản hồi nhẹ nhàng và ổn định.

• Dễ nhìn cho cả soạn thảo văn bản và viết code
Hỗ trợ tô màu cú pháp cho 49 danh mục ngôn ngữ (hơn 120 phần mở rộng tệp) như HTML, CSS, JavaScript, Python, Swift, C/C++, Markdown, JSON, cùng tính năng gõ tắt Emmet tích hợp sẵn.

• Xem trước trực tiếp (Live Preview)
Xem trước ngay lập tức tài liệu Markdown, HTML, SVG cũng như sơ đồ Mermaid và công thức toán học LaTeX mà không cần kết nối mạng.

• Tự động nhận diện bảng mã
Tự động nhận biết UTF-8, Shift_JIS, EUC-JP, UTF-16 và nhiều bảng mã khác để mở tệp chính xác, không bị lỗi phông chữ.

• Công cụ chỉnh sửa chu đáo
Hỗ trợ Tìm kiếm & Thay thế (có biểu thức chính quy Regex), đa con trỏ, hiển thị số dòng, tự động xuống dòng và giao diện Sáng / Tối.

• An tâm về quyền riêng tư
Không quảng cáo, không theo dõi và không thu thập dữ liệu. Mọi tính năng hoạt động 100% ngoại tuyến ngay trên thiết bị của bạn.

Dù dùng làm sổ tay hàng ngày hay trình sửa code đáng tin cậy, hy vọng miu sẽ giúp khoảng thời gian viết lách của bạn thêm phần dễ chịu."""

VI_WHATS_NEW = """Cảm ơn bạn đã luôn sử dụng miu.
Phiên bản 1.0.47 tăng tốc toàn diện cho chỉnh sửa văn bản, Tìm kiếm/Thay thế và Xem trước trực tiếp:

• Gõ phím, cuộn trang và chỉnh sửa tệp lớn nhanh hơn: Tăng tốc đáng kể thao tác nhập liệu, di chuyển con trỏ và cuộn trang, giúp chỉnh sửa mượt mà ngay cả với các tệp dung lượng lớn.
• Tìm kiếm & "Thay thế tất cả" siêu tốc: Tối ưu hóa tốc độ tìm kiếm từ khóa, biểu thức chính quy (regex) và "Thay thế tất cả" hàng nghìn vị trí chỉ trong chớp mắt.
• Xem trước trực tiếp Markdown, LaTeX & Mermaid nhanh hơn: Hiển thị thời gian thực các tài liệu Markdown dài, công thức LaTeX và sơ đồ Mermaid nhanh và mượt hơn khi đang gõ."""

TH_DESC = """miu คือแอปแก้ไขข้อความและโค้ดที่เรียบง่ายและทำงานได้อย่างรวดเร็ว ออกแบบมาเพื่อให้ทั้งการจดบันทึกประจำวัน การเขียนบทความด้วย Markdown และการเขียนโปรแกรมเป็นเรื่องที่สบายตาและน่าใช้งาน

ด้วยการเปิดใช้งานที่รวดเร็วทันใจและหน้าตาที่สะอาดตา ไม่รบกวนสมาธิ miu จึงมอบประสบการณ์การเขียนที่เป็นธรรมชาติและลื่นไหลทั้งบน iPhone, iPad และ Mac

[คุณสมบัติหลัก]

• เปิดปุ๊บ เขียนได้ปั๊บ
พร้อมให้คุณบันทึกทุกไอเดียได้ทันทีที่เปิดแอป ไม่ว่าจะเป็นโน้ตสั้นๆ หรือไฟล์ข้อความขนาดใหญ่ก็เปิดและแก้ไขได้อย่างราบรื่น

• อ่านง่ายทั้งงานเขียนและงานโค้ด
รองรับการไฮไลต์ไวยากรณ์ 49 หมวดหมู่ (มากกว่า 120 นามสกุลไฟล์) เช่น HTML, CSS, JavaScript, Python, Swift, C/C++, Markdown และ JSON พร้อมรองรับการพิมพ์ตัวย่อ Emmet สำหรับ HTML และ CSS

• พรีวิวผลลัพธ์ได้ทันที
ดูตัวอย่าง Markdown, HTML, SVG รวมถึงแผนภาพ Mermaid และสมการคณิตศาสตร์ LaTeX ได้แบบเรียลไทม์โดยไม่ต้องเชื่อมต่ออินเทอร์เน็ต

• ตรวจสอบรหัสอักขระอัตโนมัติ
ตรวจจับ UTF-8, Shift_JIS, EUC-JP, UTF-16 และรหัสภาษาอื่นๆ โดยอัตโนมัติ เพื่อป้องกันปัญหาข้อความอ่านไม่ออก

• เครื่องมือแก้ไขที่ใส่ใจในรายละเอียด
มีฟีเจอร์ค้นหาและแทนที่ (รองรับ Regex), การแก้ไขหลายเคอร์เซอร์, การแสดงเลขบรรทัด, การตัดคำอัตโนมัติ และรองรับทั้งโหมดสว่างและโหมดมืด

• ปลอดภัยและเป็นส่วนตัว
ไม่มีโฆษณา ไม่มีการติดตาม และไม่มีการส่งข้อมูลออกภายนอก ทุกฟีเจอร์ทำงานแบบออฟไลน์ 100% บนอุปกรณ์ของคุณ

ไม่ว่าจะใช้เป็นสมุดโน้ตประจำวันหรือเครื่องมือเขียนโค้ดคู่ใจ เราหวังว่า miu จะช่วยให้ช่วงเวลาแห่งการเขียนของคุณน่าเพลิดเพลินยิ่งขึ้น"""

TH_WHATS_NEW = """ขอบคุณที่ใช้บริการ miu เสมอมา
ในเวอร์ชัน 1.0.47 เราได้เพิ่มความเร็วครั้งใหญ่ทั้งการแก้ไขข้อความ การค้นหา/แทนที่ และโหมดพรีวิวสด:

• พิมพ์ เลื่อนหน้าจอ และแก้ไขไฟล์ขนาดใหญ่ได้เร็วขึ้น: เร่งความเร็วการพิมพ์ การเลื่อนเคอร์เซอร์ และการเลื่อนหน้าจอให้ลื่นไหลและตอบสนองทันใจแม้เปิดไฟล์ขนาดใหญ่
• ค้นหาและ "แทนที่ทั้งหมด" เร็วทันใจ: ค้นหาคำสำคัญ นิพจน์ทั่วไป (Regex) และสั่ง "แทนที่ทั้งหมด" นับพันรายการได้เสร็จสิ้นในพริบตา
• พรีวิวสด Markdown, สมการ LaTeX และแผนภาพ Mermaid เร็วขึ้น: แสดงผลเอกสาร Markdown ขนาดยาว สมการ LaTeX และแผนภาพ Mermaid ได้รวดเร็วขึ้นขณะพิมพ์"""

ID_DESC = """miu adalah editor teks dan kode yang sederhana serta ringan, dirancang agar nyaman digunakan mulai dari mencatat ide harian, menulis dokumen Markdown, hingga pemrograman.

Dibuat dengan mengutamakan kecepatan saat dibuka dan tampilan bersih yang nyaman di mata, miu menghadirkan pengalaman menulis yang alami dan mulus di iPhone, iPad, maupun Mac.

[Fitur Utama]

• Cepat dan Ringan
Buka aplikasi dan langsung mulai mengetik. Baik untuk catatan singkat maupun membuka file teks berukuran besar, miu tetap terasa ringan dan responsif.

• Nyaman untuk Menulis Teks dan Kode
Mendukung sorotan sintaks untuk 49 kategori bahasa (lebih dari 120 ekstensi file) seperti HTML, CSS, JavaScript, Python, Swift, C/C++, Markdown, dan JSON, lengkap dengan dukungan ekspansi singkatan Emmet.

• Pratinjau Langsung (Live Preview)
Lihat hasil pratinjau Markdown, HTML, SVG, diagram Mermaid, hingga rumus matematika LaTeX secara langsung tanpa perlu koneksi internet.

• Deteksi Pengkodean Karakter Otomatis
Mengenali UTF-8, Shift_JIS, EUC-JP, UTF-16, dan pengkodean lainnya secara otomatis agar teks tampil rapi tanpa karakter rusak.

• Alat Pengeditan yang Lengkap & Rapi
Dilengkapi fitur Cari & Ganti (mendukung Regex), multi-kursor, nomor baris, pelipatan baris (word wrap), serta dukungan Mode Terang dan Mode Gelap.

• Privasi Terjaga Sepenuhnya
Tanpa iklan, tanpa pelacakan, dan tanpa pengumpulan data. Semua fitur berjalan 100% offline di perangkat Anda.

Baik sebagai buku catatan sehari-hari maupun editor kode andalan, semoga miu dapat membuat waktu menulis Anda menjadi lebih menyenangkan."""

ID_WHATS_NEW = """Terima kasih telah menggunakan miu.
Versi 1.0.47 menghadirkan peningkatan kecepatan besar pada pengeditan, Cari/Ganti, serta Pratinjau Langsung:

• Pengetikan, Pengguliran & Pengeditan File Besar Lebih Cepat: Mempercepat input teks, pergerakan kursor, dan pengguliran secara signifikan sehingga file berukuran besar sekalipun terasa sangat mulus dan responsif.
• Pencarian & "Ganti Semua" Super Cepat: Pencarian kata kunci, ekspresi reguler (regex), serta operasi "Ganti Semua" untuk ribuan kecocokan kini selesai secara instan.
• Pratinjau Langsung Markdown, LaTeX & Mermaid Lebih Cepat: Menampilkan dokumen Markdown panjang, rumus LaTeX, dan diagram Mermaid lebih cepat saat Anda mengetik."""

MS_DESC = """miu ialah penyunting teks dan kod yang ringkas serta ringan, direka untuk memberikan keselesaan sama ada ketika mencatat nota harian, menulis dokumen Markdown mahupun melakukan pengaturcaraan.

Dibina dengan tumpuan pada kelancaran sebaik sahaja dibuka dan antara muka bersih yang selesa pada mata, miu menawarkan pengalaman menulis yang semula jadi pada iPhone, iPad dan Mac.

[Ciri-ciri Utama]

• Pantas dan Lancar
Buka aplikasi dan terus mula menaip. Sama ada untuk catatan ringkas atau menyunting fail teks yang besar, miu kekal ringan dan responsif.

• Kemas untuk Penulisan dan Kod
Menyokong penyerlahan sintaks bagi 49 kategori bahasa (lebih 120 sambungan fail) termasuk HTML, CSS, JavaScript, Python, Swift, C/C++, Markdown dan JSON, serta sokongan pengembangan singkatan Emmet.

• Pratonton Langsung Terbina Dalam
Lihat pratonton Markdown, HTML, SVG, rajah Mermaid dan formula matematik LaTeX secara terus tanpa memerlukan sambungan internet.

• Pengesanan Pengekodan Aksara Automatik
Mengenal pasti UTF-8, Shift_JIS, EUC-JP, UTF-16 dan pengekodan lain secara automatik bagi mengelakkan aksara rosak.

• Alat Penyuntingan yang Teliti
Lengkap dengan fungsi Cari & Ganti (menyokong Regex), berbilang kursor, nombor baris, pembalutan teks serta Mod Cerah dan Mod Gelap.

• Privasi Diutamakan
Tiada iklan, tiada penjejakan dan tiada pengumpulan data. Semua ciri berfungsi 100% di luar talian pada peranti anda.

Sama ada sebagai buku nota harian atau penyunting kod yang boleh dipercayai, kami berharap miu menjadikan masa menulis anda lebih menyenangkan."""

MS_WHATS_NEW = """Terima kasih kerana menggunakan miu.
Versi 1.0.47 membawakan peningkatan kelajuan besar untuk penyuntingan, Cari/Ganti dan Pratonton Langsung:

• Penaipan, Penatalan & Penyuntingan Fail Besar Lebih Pantas: Input teks, pergerakan kursor dan penatalan dipercepatkan dengan ketara supaya fail besar sekalipun kekal lancar dan responsif.
• Carian & "Ganti Semua" Ultra-Pantas: Carian kata kunci, ungkapan biasa (regex) dan operasi "Ganti Semua" untuk ribuan padanan kini selesai serta-merta.
• Pratonton Langsung Markdown, LaTeX & Mermaid Lebih Pantas: Dokumen Markdown yang panjang, formula LaTeX dan rajah Mermaid dipaparkan dengan lebih pantas semasa anda menaip."""

HI_DESC = """miu एक सरल और हल्का टेक्स्ट व कोड एडिटर है, जिसे रोज़मर्रा के छोटे नोट्स से लेकर Markdown लेखन और प्रोग्रामिंग तक हर काम को सहज और आरामदायक बनाने के लिए डिज़ाइन किया गया है।

ऐप खोलते ही तुरंत लिखने की सुविधा और आँखों को सुकून देने वाले साफ़-सुथरे डिज़ाइन के साथ, miu आपको iPhone, iPad और Mac पर एक सहज लेखन अनुभव देता है।

[मुख्य विशेषताएँ]

• तेज़ और हल्का अनुभव
ऐप खोलें और तुरंत लिखना शुरू करें। चाहे कोई त्वरित विचार लिखना हो या बड़ी टेक्स्ट फ़ाइल संपादित करनी हो, miu हमेशा सुगम रहता है।

• लेखन और कोडिंग दोनों के लिए स्पष्ट
HTML, CSS, JavaScript, Python, Swift, C/C++, Markdown और JSON सहित 49 भाषा श्रेणियों (120 से अधिक फ़ाइल एक्सटेंशन) के लिए सिंटैक्स हाइलाइटिंग और इन-बिल्ट Emmet संक्षिप्त रूप विस्तार का समर्थन करता है।

• रीयल-टाइम लाइव प्रीव्यू
बिना इंटरनेट कनेक्शन के सीधे ऐप के भीतर Markdown, HTML, SVG के साथ-साथ Mermaid आरेख और LaTeX गणितीय सूत्रों का प्रीव्यू देखें।

• स्वचालित कैरेक्टर एन्कोडिंग पहचान
UTF-8, Shift_JIS, EUC-JP, UTF-16 और अन्य एन्कोडिंग को अपने आप पहचानता है ताकि अक्षर टूटे या गड़बड़ न दिखें।

• उपयोगी संपादन सुविधाएँ
खोजें और बदलें (Regex समर्थन के साथ), मल्टी-कर्सर संपादन, लाइन नंबर, वर्ड रैप और लाइट व डार्क मोड की सुविधा।

• पूर्ण गोपनीयता
कोई विज्ञापन नहीं, कोई ट्रैकिंग नहीं और कोई डेटा संग्रह नहीं। सभी सुविधाएँ आपके डिवाइस पर 100% ऑफ़लाइन काम करती हैं।

चाहे आप इसे दैनिक नोटपैड के रूप में उपयोग करें या एक भरोसेमंद कोड एडिटर के रूप में, हमें उम्मीद है कि miu आपके लिखने के समय को और सुखद बनाएगा।"""

HI_WHATS_NEW = """miu का उपयोग करने के लिए धन्यवाद।
संस्करण 1.0.47 में संपादन, खोजें/बदलें और लाइव प्रीव्यू की गति में बड़ा सुधार किया गया है:

• तेज़ टाइपिंग, स्क्रॉलिंग और बड़ी फ़ाइलों का संपादन: टेक्स्ट इनपुट, कर्सर मूवमेंट और स्क्रॉलिंग को काफी तेज़ किया गया है ताकि बड़ी फ़ाइलों में भी काम करना बेहद सहज और तेज़ रहे।
• अल्ट्रा-फ़ास्ट खोज और "सभी बदलें" (Replace All): कीवर्ड खोज, रेगेक्स (Regex) मिलान और हज़ारों स्थानों पर एक साथ "सभी बदलें" अब पलक झपकते ही पूरा हो जाता है।
• तेज़ Markdown, LaTeX और Mermaid लाइव प्रीव्यू: लंबे Markdown दस्तावेज़ों, LaTeX सूत्रों और Mermaid आरेखों का लाइव प्रीव्यू टाइप करते समय और भी तेज़ी से अपडेट होता है।"""

SUPPORT_URL = "https://github.com/kenjinote/miu"

# Localized keywords per App Store Connect locale (strictly <= 100 characters each)
ASC_KEYWORDS = {
    "ja": "テキストエディタ,メモ帳,コードエディタ,Markdown,マークダウン,プレビュー,プログラミング,正規表現,Mermaid,LaTeX,文字コード,軽量",
    "en-US": "text editor,code editor,markdown,notepad,notes,syntax,mermaid,latex,regex,html,json,swift,python",
    "en-GB": "text editor,code editor,markdown,notepad,notes,syntax,mermaid,latex,regex,html,json,swift,python",
    "en-AU": "text editor,code editor,markdown,notepad,notes,syntax,mermaid,latex,regex,html,json,swift,python",
    "en-CA": "text editor,code editor,markdown,notepad,notes,syntax,mermaid,latex,regex,html,json,swift,python",
    "zh-Hans": "文本编辑器,代码编辑器,记事本,笔记,Markdown,实时预览,语法高亮,编程,正则表达式,Mermaid,LaTeX,轻量",
    "zh-Hant": "文字編輯器,程式碼編輯器,記事本,筆記,Markdown,即時預覽,語法突顯,正規表示式,Mermaid,LaTeX,輕量",
    "ko": "텍스트 에디터,코드 에디터,메모장,노트,마크다운,Markdown,실시간 미리보기,구문 강조,정규식,프로그래밍,Mermaid,LaTeX",
    "de-DE": "Texteditor,Code-Editor,Notizen,Markdown,Vorschau,Syntax,Programmieren,Regex,LaTeX,Mermaid,HTML",
    "fr-FR": "éditeur de texte,code,bloc-notes,notes,markdown,aperçu,syntaxe,programmation,regex,latex,mermaid",
    "fr-CA": "éditeur de texte,code,bloc-notes,notes,markdown,aperçu,syntaxe,programmation,regex,latex,mermaid",
    "es-ES": "editor de texto,código,bloc de notas,notas,markdown,vista previa,sintaxis,regex,latex,mermaid",
    "es-MX": "editor de texto,código,bloc de notas,notas,markdown,vista previa,sintaxis,regex,latex,mermaid",
    "it": "editor di testo,codice,blocco note,note,markdown,anteprima,sintassi,programmazione,regex,latex",
    "pt-BR": "editor de texto,código,bloco de notas,notas,markdown,prévia,sintaxe,programação,regex,latex",
    "pt-PT": "editor de texto,código,bloco de notas,notas,markdown,prévia,sintaxe,programação,regex,latex",
    "ru": "текстовый редактор,редактор кода,блокнот,заметки,markdown,предпросмотр,синтаксис,код,regex,latex",
    "nl-NL": "teksteditor,code-editor,kladblok,notities,markdown,voorbeeld,syntaxis,code,regex,latex,mermaid",
    "sv": "textredigerare,kodredigerare,anteckningar,markdown,förhandsgranskning,syntax,programmering,regex",
    "da": "teksteditor,kodeeditor,notesblok,noter,markdown,forhåndsvisning,syntaks,programmering,regex,latex",
    "no": "tekstredigerer,koderedigerer,notatblokk,notater,markdown,forhåndsvisning,syntaks,kode,regex,latex",
    "fi": "tekstieditori,koodieditori,muistio,muistiinpanot,markdown,esikatselu,syntaksi,ohjelmointi,regex",
    "pl": "edytor tekstu,edytor kodu,notatnik,notatki,markdown,podgląd,składnia,kod,regex,latex,mermaid",
    "cs": "textový editor,editor kódu,poznámky,markdown,náhled,syntaxe,programování,regex,latex,mermaid",
    "hu": "szövegszerkesztő,kódszerkesztő,jegyzetek,markdown,előnézet,szintaxis,programozás,regex,latex",
    "ro": "editor de text,editor cod,notițe,markdown,previzualizare,sintaxă,programare,regex,latex,mermaid",
    "el": "επεξεργαστής κειμένου,κώδικας,σημειώσεις,markdown,προεπισκόπηση,σύνταξη,regex,latex,mermaid",
    "tr": "metin düzenleyici,kod düzenleyici,not defteri,notlar,markdown,önizleme,sözdizimi,regex,latex",
    "uk": "текстовий редактор,редактор коду,блокнот,нотатки,markdown,перегляд,синтаксис,код,regex,latex",
    "vi": "trình soạn thảo,code,ghi chú,sổ tay,markdown,xem trước,cú pháp,lập trình,regex,latex,mermaid",
    "th": "แก้ไขข้อความ,แก้ไขโค้ด,สมุดบันทึก,โน้ต,Markdown,พรีวิว,เขียนโปรแกรม,Regex,LaTeX,Mermaid",
    "id": "editor teks,editor kode,catatan,notepad,markdown,pratinjau,sintaks,kode,regex,latex,mermaid",
    "ms": "penyunting teks,penyunting kod,nota,notepad,markdown,pratonton,sintaks,kod,regex,latex,mermaid",
    "hi": "टेक्स्ट एडिटर,कोड एडिटर,नोटपैड,नोट्स,Markdown,प्रीव्यू,प्रोग्रामिंग,Regex,LaTeX,Mermaid",
}

for _loc, _kw in ASC_KEYWORDS.items():
    assert len(_kw) <= 100, f"Keywords for {_loc} exceed 100 chars ({len(_kw)}): {_kw}"

# Localized Promotional Text per App Store Connect locale (strictly <= 170 characters each)
ASC_PROMOTIONAL_TEXT = {
    "ja": "毎日のちょっとしたメモから、Markdownでの文章作成、プログラミングまで。49言語のコードハイライトや、オフラインで使える数式・図表のリアルタイムプレビューを備えた、軽快で目に優しいテキストエディタです。",
    "en-US": "A fast, clean text & code editor for daily notes, Markdown, and coding—with 49 syntax languages and offline live preview for Mermaid diagrams & LaTeX math.",
    "en-GB": "A fast, clean text & code editor for daily notes, Markdown, and coding—with 49 syntax languages and offline live preview for Mermaid diagrams & LaTeX math.",
    "en-AU": "A fast, clean text & code editor for daily notes, Markdown, and coding—with 49 syntax languages and offline live preview for Mermaid diagrams & LaTeX math.",
    "en-CA": "A fast, clean text & code editor for daily notes, Markdown, and coding—with 49 syntax languages and offline live preview for Mermaid diagrams & LaTeX math.",
    "zh-Hans": "从日常随笔、Markdown 写作到程序开发，miu 为您带来轻快护眼的书写体验。支持 49 种语法高亮，并内置完全离线的 Mermaid 图表与 LaTeX 数学公式实时预览。",
    "zh-Hant": "從日常隨手筆記、Markdown 寫作到程式開發，miu 為您帶來輕快護眼的書寫體驗。支援 49 種語法突顯，並內建完全離線的 Mermaid 圖表與 LaTeX 數學公式即時預覽。",
    "ko": "일상의 가벼운 메모부터 Markdown 글쓰기, 코딩까지. 49개 언어 구문 강조와 오프라인 Mermaid 다이어그램·LaTeX 수식 실시간 미리보기를 갖춘 가볍고 편안한 텍스트 에디터입니다.",
    "de-DE": "Der schnelle, übersichtliche Text- & Code-Editor für Notizen, Markdown und Code – mit 49 Sprachen sowie Offline-Live-Vorschau für Mermaid und LaTeX.",
    "fr-FR": "Un éditeur de texte et de code léger et épuré pour vos notes, Markdown et code : 49 langages et aperçu hors ligne en direct pour Mermaid et LaTeX.",
    "fr-CA": "Un éditeur de texte et de code léger et épuré pour vos notes, Markdown et code : 49 langages et aperçu hors ligne en direct pour Mermaid et LaTeX.",
    "es-ES": "Un editor de texto y código rápido y limpio para notas, Markdown y programación, con 49 lenguajes y vista previa en vivo sin conexión para Mermaid y LaTeX.",
    "es-MX": "Un editor de texto y código rápido y limpio para notas, Markdown y programación, con 49 lenguajes y vista previa en vivo sin conexión para Mermaid y LaTeX.",
    "it": "Un editor di testo e codice leggero e pulito per note, Markdown e programmazione, con 49 linguaggi e anteprima live offline per diagrammi Mermaid e LaTeX.",
    "pt-BR": "Um editor de texto e código leve e limpo para notas, Markdown e programação, com 49 linguagens e pré-visualização offline ao vivo para Mermaid e LaTeX.",
    "pt-PT": "Um editor de texto e código leve e limpo para notas, Markdown e programação, com 49 linguagens e pré-visualização offline ao vivo para Mermaid e LaTeX.",
    "ru": "Лёгкий и удобный редактор текста и кода для заметок, Markdown и программирования: 49 языков подсветки и офлайн-предпросмотр диаграмм Mermaid и формул LaTeX.",
    "nl-NL": "Een snelle, overzichtelijke tekst- en code-editor voor notities, Markdown en code, met 49 talen en offline live-weergave voor Mermaid-diagrammen en LaTeX.",
    "sv": "En snabb och stilren text- och kodredigerare för anteckningar, Markdown och kod – med 49 språk samt live-förhandsgranskning offline för Mermaid och LaTeX.",
    "da": "En hurtig og enkel tekst- og kodeeditor til noter, Markdown og kodning – med 49 sprog og offline live-visning af Mermaid-diagrammer og LaTeX-formler.",
    "no": "En rask og oversiktlig tekst- og koderedigerer for notater, Markdown og koding – med 49 språk og offline live-forhåndsvisning av Mermaid og LaTeX.",
    "fi": "Nopea ja selkeä teksti- ja koodieditori muistiinpanoihin, Markdowniin ja ohjelmointiin – 49 kieltä sekä offline-reaaliaikainen Mermaid- ja LaTeX-esikatselu.",
    "pl": "Lekki i przejrzysty edytor tekstu i kodu do notatek, Markdown i programowania — 49 języków składni oraz podgląd na żywo offline dla Mermaid i LaTeX.",
    "cs": "Rychlý a přehledný textový a kódový editor pro poznámky, Markdown i programování – 49 jazyků a živý offline náhled pro diagramy Mermaid a vzorce LaTeX.",
    "hu": "Gyors, letisztult szöveg- és kódszerkesztő jegyzetekhez, Markdownhoz és kódoláshoz – 49 nyelv támogatásával, valamint offline Mermaid és LaTeX előnézettel.",
    "ro": "Un editor de text și cod rapid și curat pentru notițe, Markdown și programare — 49 de limbaje și previzualizare live offline pentru Mermaid și LaTeX.",
    "el": "Γρήγορος και καθαρός επεξεργαστής κειμένου και κώδικα για σημειώσεις, Markdown και κώδικα, με 49 γλώσσες και ζωντανή προεπισκόπηση εκτός σύνδεσης.",
    "tr": "Günlük notlar, Markdown ve kodlama için hızlı ve sade metin/kod düzenleyici; 49 dil desteği ve Mermaid şemaları ile LaTeX için çevrimdışı canlı önizleme.",
    "uk": "Швидкий і зручний редактор тексту та коду для нотаток, Markdown і програмування: 49 мов підсвічування та живий офлайн-перегляд Mermaid і формул LaTeX.",
    "vi": "Trình soạn thảo văn bản & code nhẹ, thoáng mắt cho ghi chú, Markdown và lập trình — hỗ trợ 49 ngôn ngữ cùng xem trước trực tiếp ngoại tuyến Mermaid & LaTeX.",
    "th": "ตัวแก้ไขข้อความและโค้ดที่รวดเร็วสบายตา สำหรับจดบันทึก เขียน Markdown และเขียนโปรแกรม รองรับไฮไลต์ 49 ภาษา พร้อมพรีวิวกราฟ Mermaid และสมการ LaTeX แบบออฟไลน์",
    "id": "Editor teks & kode yang cepat dan nyaman untuk catatan harian, Markdown, dan pemrograman—mendukung 49 bahasa serta pratinjau langsung offline Mermaid & LaTeX.",
    "ms": "Penyunting teks & kod yang pantas dan kemas untuk nota harian, Markdown dan pengaturcaraan—49 bahasa serta pratonton langsung luar talian Mermaid & LaTeX.",
    "hi": "दैनिक नोट्स, Markdown और कोडिंग के लिए तेज़ और सरल टेक्स्ट व कोड एडिटर—49 भाषाओं के सिंटैक्स हाइलाइट और ऑफ़लाइन Mermaid व LaTeX लाइव प्रीव्यू के साथ।",
}

for _loc, _pt in ASC_PROMOTIONAL_TEXT.items():
    assert len(_pt) <= 170, f"Promotional text for {_loc} exceeds 170 chars ({len(_pt)}): {_pt}"

# Map of App Store Connect locale codes -> AppInfo subtitle (<= 30 characters, direct "Text Editor" in each language)
ASC_SUBTITLES = {
    "ja": "テキストエディタ",
    "en-US": "Text Editor",
    "en-GB": "Text Editor",
    "en-AU": "Text Editor",
    "en-CA": "Text Editor",
    "zh-Hans": "文本编辑器",
    "zh-Hant": "文字編輯器",
    "ko": "텍스트 에디터",
    "de-DE": "Texteditor",
    "fr-FR": "Éditeur de texte",
    "fr-CA": "Éditeur de texte",
    "es-ES": "Editor de texto",
    "es-MX": "Editor de texto",
    "it": "Editor di testo",
    "pt-BR": "Editor de Texto",
    "pt-PT": "Editor de texto",
    "ru": "Текстовый редактор",
    "nl-NL": "Teksteditor",
    "sv": "Textredigerare",
    "da": "Teksteditor",
    "no": "Tekstredigerer",
    "fi": "Tekstieditori",
    "pl": "Edytor tekstu",
    "cs": "Textový editor",
    "hu": "Szövegszerkesztő",
    "ro": "Editor de text",
    "el": "Επεξεργαστής κειμένου",
    "tr": "Metin Düzenleyici",
    "uk": "Текстовий редактор",
    "vi": "Trình soạn thảo văn bản",
    "th": "โปรแกรมแก้ไขข้อความ",
    "id": "Editor Teks",
    "ms": "Penyunting Teks",
    "hi": "टेक्स्ट एडिटर",
}

for _loc, _sub in ASC_SUBTITLES.items():
    assert len(_sub) <= 30, f"Subtitle for {_loc} exceeds 30 chars ({len(_sub)}): {_sub}"

# Map of App Store Connect locale codes -> Fallback App Name when "miu" is already taken (<= 30 characters)
ASC_FALLBACK_NAMES = {
    "ja": "miu – シンプルなテキストエディタ",
    "en-US": "miu – Minimal Text Editor",
    "en-GB": "miu – Minimal Text Editor",
    "en-AU": "miu – Minimal Text Editor",
    "en-CA": "miu – Minimal Text Editor",
    "zh-Hans": "miu – 极简文本编辑器",
    "zh-Hant": "miu – 極簡文字編輯器",
    "ko": "miu – 심플 텍스트 에디터",
    "de-DE": "miu – Einfacher Texteditor",
    "fr-FR": "miu – Éditeur de texte simple",
    "fr-CA": "miu – Éditeur de texte simple",
    "es-ES": "miu – Editor de texto simple",
    "es-MX": "miu – Editor de texto simple",
    "it": "miu – Editor di testo semplice",
    "pt-BR": "miu – Editor de texto simples",
    "pt-PT": "miu – Editor de texto simples",
    "ru": "miu – Лёгкий редактор текста",
    "nl-NL": "miu – Simpele teksteditor",
    "sv": "miu – Enkel textredigerare",
    "da": "miu – Enkel teksteditor",
    "no": "miu – Enkel tekstredigerer",
    "fi": "miu – Selkeä tekstieditori",
    "pl": "miu – Prosty edytor tekstu",
    "cs": "miu – Jednoduchý editor textu",
    "hu": "miu – Könnyű szövegszerkesztő",
    "ro": "miu – Editor de text simplu",
    "el": "miu – Επεξεργαστής κειμένου",
    "tr": "miu – Basit Metin Düzenleyici",
    "uk": "miu – Простий редактор тексту",
    "vi": "miu – Soạn thảo văn bản nhẹ",
    "th": "miu – ตัวแก้ไขข้อความอย่างง่าย",
    "id": "miu – Editor Teks Sederhana",
    "ms": "miu – Penyunting Teks Ringkas",
    "hi": "miu – सरल टेक्स्ट एडिटर",
}

for _loc, _nm in ASC_FALLBACK_NAMES.items():
    assert len(_nm) <= 30, f"Fallback name for {_loc} exceeds 30 chars ({len(_nm)}): {_nm}"

# Map of App Store Connect locale codes -> (description, whatsNew)
ASC_LOCALIZATIONS = {
    "ja": (JA_DESC, JA_WHATS_NEW),
    "en-US": (EN_DESC, EN_WHATS_NEW),
    "en-GB": (EN_DESC, EN_WHATS_NEW),
    "en-AU": (EN_DESC, EN_WHATS_NEW),
    "en-CA": (EN_DESC, EN_WHATS_NEW),
    "zh-Hans": (ZH_HANS_DESC, ZH_HANS_WHATS_NEW),
    "zh-Hant": (ZH_HANT_DESC, ZH_HANT_WHATS_NEW),
    "ko": (KO_DESC, KO_WHATS_NEW),
    "de-DE": (DE_DESC, DE_WHATS_NEW),
    "fr-FR": (FR_DESC, FR_WHATS_NEW),
    "fr-CA": (FR_DESC, FR_WHATS_NEW),
    "es-ES": (ES_DESC, ES_WHATS_NEW),
    "es-MX": (ES_DESC, ES_WHATS_NEW),
    "it": (IT_DESC, IT_WHATS_NEW),
    "pt-BR": (PT_DESC, PT_WHATS_NEW),
    "pt-PT": (PT_DESC, PT_WHATS_NEW),
    "ru": (RU_DESC, RU_WHATS_NEW),
    "nl-NL": (NL_DESC, NL_WHATS_NEW),
    "sv": (SV_DESC, SV_WHATS_NEW),
    "da": (DA_DESC, DA_WHATS_NEW),
    "no": (NO_DESC, NO_WHATS_NEW),
    "fi": (FI_DESC, FI_WHATS_NEW),
    "pl": (PL_DESC, PL_WHATS_NEW),
    "cs": (CS_DESC, CS_WHATS_NEW),
    "hu": (HU_DESC, HU_WHATS_NEW),
    "ro": (RO_DESC, RO_WHATS_NEW),
    "el": (EL_DESC, EL_WHATS_NEW),
    "tr": (TR_DESC, TR_WHATS_NEW),
    "uk": (UK_DESC, UK_WHATS_NEW),
    "vi": (VI_DESC, VI_WHATS_NEW),
    "th": (TH_DESC, TH_WHATS_NEW),
    "id": (ID_DESC, ID_WHATS_NEW),
    "ms": (MS_DESC, MS_WHATS_NEW),
    "hi": (HI_DESC, HI_WHATS_NEW),
}


def generate_token():
    key_id = os.environ.get("APPSTORE_KEY_ID", "").strip()
    issuer_id = os.environ.get("APPSTORE_ISSUER_ID", "").strip()
    private_key = os.environ.get("APPSTORE_PRIVATE_KEY", "").strip()

    if not private_key and key_id:
        key_path = os.path.expanduser(f"~/.appstoreconnect/private_keys/AuthKey_{key_id}.p8")
        if os.path.exists(key_path):
            with open(key_path, "r", encoding="utf-8") as f:
                private_key = f.read().strip()

    if not key_id or not issuer_id or not private_key:
        raise RuntimeError("Missing APPSTORE_KEY_ID, APPSTORE_ISSUER_ID, or APPSTORE_PRIVATE_KEY.")

    now = int(time.time())
    payload = {
        "iss": issuer_id,
        "iat": now,
        "exp": now + 900,
        "aud": "appstoreconnect-v1",
    }
    return jwt.encode(
        payload,
        private_key,
        algorithm="ES256",
        headers={"kid": key_id, "typ": "JWT"},
    )


def get_headers():
    return {
        "Authorization": f"Bearer {generate_token()}",
        "Content-Type": "application/json",
        "Accept": "application/json",
    }


def get_app_id(headers):
    r = requests.get(f"{BASE_URL}/apps?filter[bundleId]={BUNDLE_ID}", headers=headers)
    if r.status_code != 200:
        raise RuntimeError(f"Failed to query apps: HTTP {r.status_code} {r.text}")
    data = r.json().get("data", [])
    if not data:
        raise RuntimeError(f"App with bundleId '{BUNDLE_ID}' not found in App Store Connect.")
    app_id = data[0]["id"]
    app_name = data[0].get("attributes", {}).get("name", BUNDLE_ID)
    print(f"Found App: {app_name} (ID: {app_id}, Bundle ID: {BUNDLE_ID})")
    return app_id, app_name


def sync_app_info_localizations(app_id, app_name, headers):
    """Ensure App Info level localizations exist and sync privacyPolicyUrl from en-US to all target locales."""
    r = requests.get(f"{BASE_URL}/apps/{app_id}/appInfos", headers=headers)
    if r.status_code != 200:
        print(f"[Warn] Could not fetch appInfos: HTTP {r.status_code}")
        return

    app_infos = r.json().get("data", [])
    if not app_infos:
        return

    editable_states = {
        "PREPARE_FOR_SUBMISSION",
        "READY_FOR_SUBMISSION",
        "WAITING_FOR_REVIEW",
        "IN_REVIEW",
        "DEVELOPER_REJECTED",
        "REJECTED",
        "METADATA_REJECTED",
    }

    # First, discover the English (en-US) privacyPolicyUrl across all appInfos
    en_privacy_url = None
    fallback_privacy_url = None
    info_locs_map = {}

    for info in app_infos:
        iid = info["id"]
        r_locs = requests.get(f"{BASE_URL}/appInfos/{iid}/appInfoLocalizations?limit=200", headers=headers)
        if r_locs.status_code == 200:
            locs_data = r_locs.json().get("data", [])
            info_locs_map[iid] = locs_data
            for loc in locs_data:
                attrs = loc.get("attributes", {})
                loc_code = attrs.get("locale")
                p_url = (attrs.get("privacyPolicyUrl") or "").strip()
                if p_url:
                    if loc_code == "en-US":
                        en_privacy_url = p_url
                    elif not fallback_privacy_url:
                        fallback_privacy_url = p_url

    privacy_url = en_privacy_url or fallback_privacy_url or SUPPORT_URL
    print(f"\nDiscovered English privacyPolicyUrl: {privacy_url}")

    # Update all editable appInfos (or all appInfos if none match editable_states)
    target_infos = [
        info for info in app_infos
        if (info.get("attributes", {}).get("appStoreState") or info.get("attributes", {}).get("state", "")) in editable_states
    ]
    if not target_infos:
        target_infos = app_infos

    for target_info in target_infos:
        info_id = target_info["id"]
        info_state = target_info.get("attributes", {}).get("appStoreState") or target_info.get("attributes", {}).get("state", "")
        print(f"\nSyncing AppInfo (ID: {info_id}, State: {info_state})...")

        locs_data = info_locs_map.get(info_id, [])
        existing_locales = {
            loc.get("attributes", {}).get("locale"): loc
            for loc in locs_data
        }
        print(f"Existing AppInfo locales ({len(existing_locales)}): {', '.join(sorted(existing_locales.keys()))}")

        updated_count = 0
        created_count = 0
        for locale in ASC_LOCALIZATIONS.keys():
            subtitle = ASC_SUBTITLES[locale]
            fallback_name = ASC_FALLBACK_NAMES[locale]
            if locale in existing_locales:
                loc_obj = existing_locales[locale]
                loc_id = loc_obj["id"]
                attrs = loc_obj.get("attributes", {})
                cur_name = (attrs.get("name") or "").strip()
                cur_url = (attrs.get("privacyPolicyUrl") or "").strip()
                cur_sub = (attrs.get("subtitle") or "").strip()

                # Keep "miu" if this locale was able to claim "miu"; otherwise use the localized "miu – ..." name
                target_name = "miu" if cur_name == "miu" else fallback_name

                if cur_name == target_name and cur_url == privacy_url and cur_sub == subtitle:
                    updated_count += 1
                    print(f"  [OK] AppInfo {locale} name='{cur_name}', subtitle='{subtitle}' already set")
                    continue

                patch_attrs = {
                    "subtitle": subtitle,
                    "privacyPolicyUrl": privacy_url,
                }
                if cur_name != target_name:
                    patch_attrs["name"] = target_name

                patch_payload = {
                    "data": {
                        "type": "appInfoLocalizations",
                        "id": loc_id,
                        "attributes": patch_attrs,
                    }
                }
                res_p = requests.patch(f"{BASE_URL}/appInfoLocalizations/{loc_id}", headers=headers, json=patch_payload)
                if res_p.status_code == 200:
                    updated_count += 1
                    print(f"  [OK] Updated AppInfo {locale} name='{target_name}' (was '{cur_name}'), subtitle='{subtitle}'")
                else:
                    print(f"  [ERR] Failed to update AppInfo {locale} (HTTP {res_p.status_code}: {res_p.text[:200]})")
            else:
                payload = {
                    "data": {
                        "type": "appInfoLocalizations",
                        "attributes": {
                            "locale": locale,
                            "name": "miu",
                            "subtitle": subtitle,
                            "privacyPolicyUrl": privacy_url,
                        },
                        "relationships": {
                            "appInfo": {
                                "data": {
                                    "type": "appInfos",
                                    "id": info_id,
                                }
                            }
                        },
                    }
                }
                res = requests.post(f"{BASE_URL}/appInfoLocalizations", headers=headers, json=payload)
                if res.status_code in (200, 201):
                    created_count += 1
                    print(f"  [+] Created AppInfo {locale} (name: 'miu', subtitle: '{subtitle}', privacyPolicyUrl: {privacy_url})")
                else:
                    payload["data"]["attributes"]["name"] = fallback_name
                    res2 = requests.post(f"{BASE_URL}/appInfoLocalizations", headers=headers, json=payload)
                    if res2.status_code in (200, 201):
                        created_count += 1
                        print(f"  [+] Created AppInfo {locale} (name: '{fallback_name}', subtitle: '{subtitle}', privacyPolicyUrl: {privacy_url})")
                    else:
                        print(f"  [!] Skipped AppInfo {locale} (HTTP {res2.status_code}: {res2.text[:200]})")
        print(f"AppInfo Summary (ID: {info_id}): {updated_count} updated/verified, {created_count} created.")


def get_or_create_editable_versions(app_id, headers, target_platforms=None):
    """Return list of (platform, version_id, version_string, state, has_live_version) to update."""
    r = requests.get(f"{BASE_URL}/apps/{app_id}/appStoreVersions?limit=50", headers=headers)
    if r.status_code != 200:
        raise RuntimeError(f"Failed to query appStoreVersions: HTTP {r.status_code} {r.text}")

    versions = r.json().get("data", [])
    by_platform = {}
    for v in versions:
        plat = v.get("attributes", {}).get("platform", "")
        by_platform.setdefault(plat, []).append(v)

    editable_states = {
        "PREPARE_FOR_SUBMISSION",
        "DEVELOPER_REJECTED",
        "REJECTED",
        "METADATA_REJECTED",
        "WAITING_FOR_REVIEW",
        "IN_REVIEW",
        "INVALID_BINARY",
    }
    live_states = {
        "READY_FOR_SALE",
        "PENDING_DEVELOPER_RELEASE",
        "PROCESSING_FOR_APP_STORE",
        "PREORDER_READY_FOR_SALE",
    }

    targets = []
    for plat in (target_platforms or ["IOS", "MAC_OS"]):
        plat_versions = by_platform.get(plat, [])
        if not plat_versions:
            print(f"\n[{plat}] No existing App Store versions found; attempting to create {TARGET_VERSION}...")
            create_payload = {
                "data": {
                    "type": "appStoreVersions",
                    "attributes": {
                        "platform": plat,
                        "versionString": TARGET_VERSION,
                    },
                    "relationships": {
                        "app": {
                            "data": {
                                "type": "apps",
                                "id": app_id,
                            }
                        }
                    },
                }
            }
            res = requests.post(f"{BASE_URL}/appStoreVersions", headers=headers, json=create_payload)
            if res.status_code in (200, 201):
                new_v = res.json()["data"]
                targets.append((plat, new_v["id"], TARGET_VERSION, "PREPARE_FOR_SUBMISSION", False))
                print(f"[{plat}] Created new version {TARGET_VERSION} (ID: {new_v['id']})")
            else:
                print(f"[{plat}] Could not create version: HTTP {res.status_code} {res.text}")
            continue

        has_live = any(v.get("attributes", {}).get("appStoreState") in live_states for v in plat_versions)
        editable_v = None
        for v in plat_versions:
            if v.get("attributes", {}).get("appStoreState") in editable_states:
                editable_v = v
                break

        if editable_v:
            v_id = editable_v["id"]
            v_str = editable_v.get("attributes", {}).get("versionString", "")
            v_state = editable_v.get("attributes", {}).get("appStoreState", "")
            print(f"\n[{plat}] Found editable version {v_str} (ID: {v_id}, State: {v_state}, HasLive: {has_live})")

            if v_state in ("WAITING_FOR_REVIEW", "IN_REVIEW"):
                print(f"[{plat}] Version {v_str} is currently {v_state}; canceling active reviewSubmission to allow metadata update and re-submission...")
                r_subs = requests.get(
                    f"{BASE_URL}/apps/{app_id}/reviewSubmissions?filter[platform]={plat}&limit=50",
                    headers=headers,
                )
                if r_subs.status_code == 200:
                    for sub in r_subs.json().get("data", []):
                        sub_id = sub["id"]
                        sub_state = sub.get("attributes", {}).get("state", "")
                        if sub_state in ("WAITING_FOR_REVIEW", "IN_REVIEW", "READY_FOR_REVIEW"):
                            cancel_payload = {
                                "data": {
                                    "type": "reviewSubmissions",
                                    "id": sub_id,
                                    "attributes": {"canceled": True},
                                }
                            }
                            cr = requests.patch(f"{BASE_URL}/reviewSubmissions/{sub_id}", headers=headers, json=cancel_payload)
                            print(f"[{plat}] Cancel reviewSubmission {sub_id} ({sub_state}): HTTP {cr.status_code}")
                            if cr.status_code in (200, 204):
                                time.sleep(4)
                                v_state = "DEVELOPER_REJECTED"

            if v_str != TARGET_VERSION:
                # Detach any existing build from the previous versionString first so App Store Connect allows changing versionString
                detach_r = requests.patch(
                    f"{BASE_URL}/appStoreVersions/{v_id}/relationships/build",
                    headers=headers,
                    json={"data": None},
                )
                print(f"[{plat}] Detached previous build before versionString update: HTTP {detach_r.status_code}")

                patch_payload = {
                    "data": {
                        "type": "appStoreVersions",
                        "id": v_id,
                        "attributes": {
                            "versionString": TARGET_VERSION,
                        },
                    }
                }
                pr = requests.patch(f"{BASE_URL}/appStoreVersions/{v_id}", headers=headers, json=patch_payload)
                if pr.status_code == 200:
                    print(f"[{plat}] Updated versionString from {v_str} -> {TARGET_VERSION}")
                    v_str = TARGET_VERSION
                else:
                    print(f"[{plat}] Could not update versionString from {v_str} -> {TARGET_VERSION}: HTTP {pr.status_code} {pr.text[:500]}")
            targets.append((plat, v_id, v_str, v_state, has_live))
        else:
            # Only live/non-editable versions exist; create a new version for TARGET_VERSION
            print(f"\n[{plat}] No editable version found (HasLive: {has_live}). Creating version {TARGET_VERSION}...")
            create_payload = {
                "data": {
                    "type": "appStoreVersions",
                    "attributes": {
                        "platform": plat,
                        "versionString": TARGET_VERSION,
                    },
                    "relationships": {
                        "app": {
                            "data": {
                                "type": "apps",
                                "id": app_id,
                            }
                        }
                    },
                }
            }
            res = requests.post(f"{BASE_URL}/appStoreVersions", headers=headers, json=create_payload)
            if res.status_code in (200, 201):
                new_v = res.json()["data"]
                targets.append((plat, new_v["id"], TARGET_VERSION, "PREPARE_FOR_SUBMISSION", has_live))
                print(f"[{plat}] Created new version {TARGET_VERSION} (ID: {new_v['id']})")
            else:
                # Fallback to first available version if creation is not allowed
                first_v = plat_versions[0]
                v_id = first_v["id"]
                v_str = first_v.get("attributes", {}).get("versionString", "")
                v_state = first_v.get("attributes", {}).get("appStoreState", "")
                print(f"[{plat}] Could not create new version (HTTP {res.status_code}); using existing {v_str} ({v_state})")
                targets.append((plat, v_id, v_str, v_state, has_live))

    return targets


def sync_version_localizations(plat, version_id, version_str, state, has_live, headers):
    print(f"\n=== Syncing {plat} v{version_str} (ID: {version_id}, State: {state}) ===")
    r = requests.get(f"{BASE_URL}/appStoreVersions/{version_id}/appStoreVersionLocalizations?limit=200", headers=headers)
    if r.status_code != 200:
        print(f"[{plat}] Failed to fetch localizations: HTTP {r.status_code} {r.text}")
        return 0, 0

    existing_locs = {
        loc.get("attributes", {}).get("locale"): loc["id"]
        for loc in r.json().get("data", [])
    }
    print(f"[{plat}] Existing locales ({len(existing_locs)}): {', '.join(sorted(existing_locs.keys()))}")

    updated_count = 0
    created_count = 0
    failed_count = 0

    for locale, (desc, whats_new) in ASC_LOCALIZATIONS.items():
        kw = ASC_KEYWORDS[locale]
        promo = ASC_PROMOTIONAL_TEXT[locale]
        if locale in existing_locs:
            loc_id = existing_locs[locale]
            attrs = {
                "description": desc,
                "whatsNew": whats_new,
                "promotionalText": promo,
                "keywords": kw,
                "supportUrl": SUPPORT_URL,
            }
            payload = {
                "data": {
                    "type": "appStoreVersionLocalizations",
                    "id": loc_id,
                    "attributes": attrs,
                }
            }
            res = requests.patch(f"{BASE_URL}/appStoreVersionLocalizations/{loc_id}", headers=headers, json=payload)
            if res.status_code == 200:
                updated_count += 1
                print(f"  [OK] Updated {locale} (description + whatsNew + promotionalText + keywords + supportUrl)")
            elif res.status_code == 409:
                # Initial v1.0 submissions do not allow whatsNew; retry with description + promotionalText + keywords + supportUrl
                payload_desc_only = {
                    "data": {
                        "type": "appStoreVersionLocalizations",
                        "id": loc_id,
                        "attributes": {
                            "description": desc,
                            "promotionalText": promo,
                            "keywords": kw,
                            "supportUrl": SUPPORT_URL,
                        },
                    }
                }
                res2 = requests.patch(f"{BASE_URL}/appStoreVersionLocalizations/{loc_id}", headers=headers, json=payload_desc_only)
                if res2.status_code == 200:
                    updated_count += 1
                    print(f"  [OK] Updated {locale} (description + promotionalText + keywords + supportUrl; whatsNew skipped)")
                else:
                    # Or if version is READY_FOR_SALE and only whatsNew/promotionalText/supportUrl is editable
                    payload_wn_only = {
                        "data": {
                            "type": "appStoreVersionLocalizations",
                            "id": loc_id,
                            "attributes": {
                                "whatsNew": whats_new,
                                "promotionalText": promo,
                                "supportUrl": SUPPORT_URL,
                            },
                        }
                    }
                    res3 = requests.patch(f"{BASE_URL}/appStoreVersionLocalizations/{loc_id}", headers=headers, json=payload_wn_only)
                    if res3.status_code == 200:
                        updated_count += 1
                        print(f"  [OK] Updated {locale} (whatsNew + promotionalText + supportUrl)")
                    else:
                        payload_wn_promo = {
                            "data": {
                                "type": "appStoreVersionLocalizations",
                                "id": loc_id,
                                "attributes": {
                                    "whatsNew": whats_new,
                                    "promotionalText": promo,
                                },
                            }
                        }
                        res4 = requests.patch(f"{BASE_URL}/appStoreVersionLocalizations/{loc_id}", headers=headers, json=payload_wn_promo)
                        if res4.status_code == 200:
                            updated_count += 1
                            print(f"  [OK] Updated {locale} (whatsNew + promotionalText)")
                        else:
                            failed_count += 1
                            print(f"  [ERR] Failed to update {locale}: HTTP {res4.status_code} {res4.text[:240]}")
            else:
                failed_count += 1
                print(f"  [ERR] Failed to update {locale}: HTTP {res.status_code} {res.text}")
        else:
            # Create new localization for this version
            payload = {
                "data": {
                    "type": "appStoreVersionLocalizations",
                    "attributes": {
                        "locale": locale,
                        "description": desc,
                        "whatsNew": whats_new,
                        "promotionalText": promo,
                        "keywords": kw,
                        "supportUrl": SUPPORT_URL,
                    },
                    "relationships": {
                        "appStoreVersion": {
                            "data": {
                                "type": "appStoreVersions",
                                "id": version_id,
                            }
                        }
                    },
                }
            }
            res = requests.post(f"{BASE_URL}/appStoreVersionLocalizations", headers=headers, json=payload)
            if res.status_code in (200, 201):
                created_count += 1
                print(f"  [NEW] Created {locale} (description + whatsNew + promotionalText + keywords + supportUrl)")
            elif res.status_code == 409:
                # Retry without whatsNew (for initial release where whatsNew is not allowed)
                payload_desc_only = {
                    "data": {
                        "type": "appStoreVersionLocalizations",
                        "attributes": {
                            "locale": locale,
                            "description": desc,
                            "promotionalText": promo,
                            "keywords": kw,
                            "supportUrl": SUPPORT_URL,
                        },
                        "relationships": {
                            "appStoreVersion": {
                                "data": {
                                    "type": "appStoreVersions",
                                    "id": version_id,
                                }
                            }
                        },
                    }
                }
                res2 = requests.post(f"{BASE_URL}/appStoreVersionLocalizations", headers=headers, json=payload_desc_only)
                if res2.status_code in (200, 201):
                    created_count += 1
                    print(f"  [NEW] Created {locale} (description + promotionalText + keywords + supportUrl; whatsNew skipped)")
                else:
                    failed_count += 1
                    print(f"  [ERR] Failed to create {locale}: HTTP {res2.status_code} {res2.text}")
            else:
                failed_count += 1
                print(f"  [ERR] Failed to create {locale}: HTTP {res.status_code} {res.text}")

    print(f"[{plat}] Summary: {updated_count} updated, {created_count} created, {failed_count} failed.")
    return updated_count + created_count, failed_count


def attach_build_and_submit(app_id, targets):
    print(f"\n=== Attaching Build & Submitting {TARGET_VERSION} for App Store Review ===")
    for plat, v_id, v_str, state, _ in targets:
        headers = get_headers()
        print(f"\n[{plat}] Looking for VALID build for v{v_str} (version_id={v_id}, state={state})...")
        chosen_build_id = None
        chosen_build_ver = None
        for attempt in range(20):
            headers = get_headers()
            r_b = requests.get(
                f"{BASE_URL}/builds?filter[app]={app_id}&filter[preReleaseVersion.platform]={plat}&filter[preReleaseVersion.version]={v_str}&sort=-uploadedDate&limit=10",
                headers=headers,
            )
            if r_b.status_code != 200:
                print(f"[{plat}] Failed to query builds: HTTP {r_b.status_code} {r_b.text[:200]}")
                break
            builds = r_b.json().get("data", [])
            if not builds:
                print(f"[{plat}] Attempt {attempt+1}/20: No builds found yet for {v_str}; waiting 15s...")
                time.sleep(15)
                continue
            b0 = builds[0]
            b_id = b0["id"]
            b_ver = b0.get("attributes", {}).get("version", "")
            b_state = b0.get("attributes", {}).get("processingState", "")
            print(f"[{plat}] Latest build: version={b_ver} (id={b_id}, processingState={b_state})")
            if b_state == "VALID":
                chosen_build_id = b_id
                chosen_build_ver = b_ver
                break
            elif b_state == "PROCESSING":
                print(f"[{plat}] Build {b_ver} is still PROCESSING; waiting 15s ({attempt+1}/20)...")
                time.sleep(15)
            else:
                print(f"[{plat}] Build {b_ver} is in state {b_state}.")
                break

        if not chosen_build_id:
            print(f"[{plat}] No VALID build available to attach for {v_str}.")
            continue

        # Ensure encryption declaration (usesNonExemptEncryption = False)
        enc_payload = {
            "data": {
                "type": "builds",
                "id": chosen_build_id,
                "attributes": {"usesNonExemptEncryption": False},
            }
        }
        requests.patch(f"{BASE_URL}/builds/{chosen_build_id}", headers=headers, json=enc_payload)

        # Attach build to appStoreVersion
        rel_payload = {
            "data": {
                "type": "builds",
                "id": chosen_build_id,
            }
        }
        r_rel = requests.patch(
            f"{BASE_URL}/appStoreVersions/{v_id}/relationships/build",
            headers=headers,
            json=rel_payload,
        )
        if r_rel.status_code in (200, 204):
            print(f"[{plat}] Attached build {chosen_build_ver} ({chosen_build_id}) to version {v_str} ({v_id})!")
        else:
            print(f"[{plat}] Could not attach build: HTTP {r_rel.status_code} {r_rel.text[:240]}")

        # Submit for review via reviewSubmissions API
        r_subs = requests.get(
            f"{BASE_URL}/apps/{app_id}/reviewSubmissions?filter[platform]={plat}&limit=20",
            headers=headers,
        )
        sub_id = None
        if r_subs.status_code == 200:
            for s in r_subs.json().get("data", []):
                st = s.get("attributes", {}).get("state", "")
                if st == "READY_FOR_REVIEW":
                    sub_id = s["id"]
                    break
                elif st in ("WAITING_FOR_REVIEW", "IN_REVIEW"):
                    print(f"[{plat}] Already in {st} (reviewSubmission {s['id']}).")
                    sub_id = "ALREADY_SUBMITTED"
                    break

        if sub_id == "ALREADY_SUBMITTED":
            continue

        if not sub_id:
            create_sub = {
                "data": {
                    "type": "reviewSubmissions",
                    "attributes": {"platform": plat},
                    "relationships": {
                        "app": {"data": {"type": "apps", "id": app_id}}
                    },
                }
            }
            r_cs = requests.post(f"{BASE_URL}/reviewSubmissions", headers=headers, json=create_sub)
            if r_cs.status_code in (200, 201):
                sub_id = r_cs.json()["data"]["id"]
                print(f"[{plat}] Created reviewSubmission {sub_id}")
            else:
                print(f"[{plat}] Could not create reviewSubmission: HTTP {r_cs.status_code} {r_cs.text[:240]}")
                continue

        # Add appStoreVersion item to reviewSubmission
        item_payload = {
            "data": {
                "type": "reviewSubmissionItems",
                "relationships": {
                    "reviewSubmission": {"data": {"type": "reviewSubmissions", "id": sub_id}},
                    "appStoreVersion": {"data": {"type": "appStoreVersions", "id": v_id}},
                },
            }
        }
        r_item = requests.post(f"{BASE_URL}/reviewSubmissionItems", headers=headers, json=item_payload)
        if r_item.status_code in (200, 201, 409):
            print(f"[{plat}] Added appStoreVersion {v_id} to reviewSubmission {sub_id} (HTTP {r_item.status_code})")
        else:
            print(f"[{plat}] Note on reviewSubmissionItem: HTTP {r_item.status_code} {r_item.text[:240]}")

        # Submit reviewSubmission
        submit_payload = {
            "data": {
                "type": "reviewSubmissions",
                "id": sub_id,
                "attributes": {"submitted": True},
            }
        }
        r_sub_patch = requests.patch(f"{BASE_URL}/reviewSubmissions/{sub_id}", headers=headers, json=submit_payload)
        if r_sub_patch.status_code == 200:
            new_st = r_sub_patch.json().get("data", {}).get("attributes", {}).get("state", "")
            print(f"[{plat}] Successfully submitted {v_str} (build {chosen_build_ver}) for App Review! State: {new_st}")
        else:
            print(f"[{plat}] Could not submit reviewSubmission {sub_id}: HTTP {r_sub_patch.status_code} {r_sub_patch.text[:300]}")


def prepare_testflight_builds_only(app_id):
    print(f"\n=== Preparing {TARGET_VERSION} Builds for TestFlight Only (No App Store Review Submission) ===")
    for plat in ("IOS", "MAC_OS"):
        print(f"\n[{plat}] Looking for uploaded build for v{TARGET_VERSION} on TestFlight...")
        chosen_build_id = None
        chosen_build_ver = None
        for attempt in range(20):
            headers = get_headers()
            r_b = requests.get(
                f"{BASE_URL}/builds?filter[app]={app_id}&filter[preReleaseVersion.platform]={plat}&filter[preReleaseVersion.version]={TARGET_VERSION}&sort=-uploadedDate&limit=10",
                headers=headers,
            )
            if r_b.status_code != 200:
                print(f"[{plat}] Failed to query builds: HTTP {r_b.status_code} {r_b.text[:200]}")
                break
            builds = r_b.json().get("data", [])
            if not builds:
                print(f"[{plat}] Attempt {attempt+1}/20: No builds found yet for {TARGET_VERSION}; waiting 15s...")
                time.sleep(15)
                continue
            b0 = builds[0]
            b_id = b0["id"]
            b_ver = b0.get("attributes", {}).get("version", "")
            b_state = b0.get("attributes", {}).get("processingState", "")
            print(f"[{plat}] Latest build: version={b_ver} (id={b_id}, processingState={b_state})")
            if b_state == "VALID":
                chosen_build_id = b_id
                chosen_build_ver = b_ver
                break
            elif b_state == "PROCESSING":
                print(f"[{plat}] Build {b_ver} is still PROCESSING; waiting 15s ({attempt+1}/20)...")
                time.sleep(15)
            else:
                print(f"[{plat}] Build {b_ver} is in state {b_state}.")
                break

        if not chosen_build_id:
            print(f"[{plat}] Build for {TARGET_VERSION} is still processing on Apple servers; TestFlight will finish processing shortly.")
            continue

        headers = get_headers()
        enc_payload = {
            "data": {
                "type": "builds",
                "id": chosen_build_id,
                "attributes": {"usesNonExemptEncryption": False},
            }
        }
        r_enc = requests.patch(f"{BASE_URL}/builds/{chosen_build_id}", headers=headers, json=enc_payload)
        print(f"[{plat}] Set usesNonExemptEncryption=False on TestFlight build {chosen_build_ver} ({chosen_build_id}): HTTP {r_enc.status_code}")

        # Sync TestFlight betaBuildLocalizations (What to Test) across locales
        r_bbloc = requests.get(f"{BASE_URL}/builds/{chosen_build_id}/betaBuildLocalizations?limit=200", headers=headers)
        existing_bbloc = {}
        if r_bbloc.status_code == 200:
            for item in r_bbloc.json().get("data", []):
                loc_code = item.get("attributes", {}).get("locale")
                if loc_code:
                    existing_bbloc[loc_code] = item["id"]
        bb_ok = 0
        for locale, (_desc, whats_new) in ASC_LOCALIZATIONS.items():
            if locale in existing_bbloc:
                bb_id = existing_bbloc[locale]
                bb_payload = {
                    "data": {
                        "type": "betaBuildLocalizations",
                        "id": bb_id,
                        "attributes": {"whatsNew": whats_new},
                    }
                }
                rb = requests.patch(f"{BASE_URL}/betaBuildLocalizations/{bb_id}", headers=headers, json=bb_payload)
                if rb.status_code in (200, 204):
                    bb_ok += 1
            else:
                bb_payload = {
                    "data": {
                        "type": "betaBuildLocalizations",
                        "attributes": {"locale": locale, "whatsNew": whats_new},
                        "relationships": {
                            "build": {"data": {"type": "builds", "id": chosen_build_id}}
                        },
                    }
                }
                rb = requests.post(f"{BASE_URL}/betaBuildLocalizations", headers=headers, json=bb_payload)
                if rb.status_code in (200, 201):
                    bb_ok += 1
        print(f"[{plat}] Synced {bb_ok} TestFlight betaBuildLocalizations on build {chosen_build_ver}")


def main():
    testflight_only = "--testflight-only" in sys.argv
    ios_only = "--ios-only" in sys.argv
    print("=== App Store Connect Metadata Synchronizer for miu ===")
    headers = get_headers()
    app_id, app_name = get_app_id(headers)

    if testflight_only:
        prepare_testflight_builds_only(app_id)
        print(f"\n=== Completed TestFlight build preparation for v{TARGET_VERSION} (No App Store submission, release notes untouched) ===")
        return

    if not ios_only:
        sync_app_info_localizations(app_id, app_name, headers)
    target_platforms = ["IOS"] if ios_only else ["IOS", "MAC_OS"]
    print(f"Target platforms for version sync & submission: {target_platforms}")
    targets = get_or_create_editable_versions(app_id, headers, target_platforms=target_platforms)

    total_ok = 0
    total_fail = 0
    for plat, v_id, v_str, state, has_live in targets:
        # Refresh token before each platform sync
        headers = get_headers()
        ok, fail = sync_version_localizations(plat, v_id, v_str, state, has_live, headers)
        total_ok += ok
        total_fail += fail

    attach_build_and_submit(app_id, targets)

    print(f"\n=== Completed: {total_ok} localizations synced across {len(targets)} platform(s) (Failures: {total_fail}) ===")
    if total_ok == 0 and total_fail > 0:
        sys.exit(1)


if __name__ == "__main__":
    main()
