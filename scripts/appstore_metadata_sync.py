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
TARGET_VERSION = "1.0.44"
BASE_URL = "https://api.appstoreconnect.apple.com/v1"

# ============================================================================
# Localized App Store Metadata (Simple, polite, and likable tone)
# ============================================================================

JA_DESC = """miu（ミュウ）は、毎日のちょっとしたメモから、Markdownでの文章作成、プログラミングまで、心地よく使えるシンプルなテキストエディタです。

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
今回のアップデートでは、より快適にお使いいただけるよう、以下の機能追加と改善を行いました。

・対応言語の拡充：シンタックスハイライトに新しく25種類の言語・設定ファイルを追加し、合計49カテゴリ（120種類以上の拡張子）に対応しました。
・日本語入力の改善：変換確定前の文字が見やすく表示されるようになり、全角文字を含む行でも行番号がきれいに揃うよう調整しました。
・入力操作とプレビューの改善：iOSでのEmmet展開（Tabキー入力）や、ソフトウェアキーボード使用時の自動スクロール、プレビュー切り替え時の描画を改善しました。
・画面レイアウトの最適化：iPadで画面を縦横に回転させた際や、さまざまな画面サイズでも、エディタが画面の端までぴったり収まるよう修正しました。
・保存機能の改善：「別名で保存」の際に、指定した拡張子のまま正しく保存できるよう改善しました。"""

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
This update brings new features and refinements to make your editing experience even smoother:

• Expanded Language Support: Added 25 new syntax highlighting languages and configuration formats, now supporting 49 categories (120+ file extensions).
• Improved Text Input & Alignment: Improved live display for uncommitted IME text (such as Japanese input) and refined line number alignment for lines containing full-width characters.
• Better Typing & Preview Experience: Improved Emmet abbreviation expansion on iOS, smoother auto-scrolling with the on-screen keyboard, and cleaner rendering when toggling Live Preview.
• Refined Screen Layout: Fixed layout adjustments when rotating iPad between portrait and landscape so the editor always fits cleanly to the edges of the screen.
• Improved File Saving: "Save As" now preserves your custom file extension without automatically appending .txt."""

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
本次更新带来了以下功能扩充与体验优化，为您提供更加顺畅的编辑环境：

・新增语法高亮语言：新增 25 种编程语言与配置文件格式，目前共支持 49 个类别（120 多种文件扩展名）。
・输入体验优化：改进了中日韩输入法候选词确认前的实时显示效果，并优化了包含全角字符时的行号对齐精度。
・键盘与预览改进：修复了 iOS 上的 Emmet 缩写展开（Tab 键），优化了使用软键盘换行时的自动滚动，以及切换实时预览时的画面渲染。
・屏幕布局优化：修复了 iPad 横竖屏旋转时的界面自适应问题，使编辑区域始终完美贴合屏幕边缘。
・保存功能改进：使用“另存为”时，可完整保留您输入的自定义扩展名，不再强制附加 .txt。"""

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
本次更新帶來了以下功能擴充與細節改善，讓操作更加順暢舒適：

・擴充支援語言：新增 25 種語法突顯語言與設定檔格式，總計支援 49 個類別（超過 120 種副檔名）。
・輸入體驗改善：修正了輸入法組字（未確定文字）在編輯器上的即時顯示，並微調了全形字元與行號的對齊位置。
・操作與預覽最佳化：改善了 iOS 上的 Emmet 縮寫展開（Tab 鍵）、虛擬鍵盤換行時的自動捲動，以及切換預覽畫面時的繪製表現。
・畫面佈局最佳化：修正了 iPad 直向與橫向旋轉時的版面配置，讓編輯區域始終完美貼合螢幕邊緣。
・儲存功能改善：使用「另存新檔」時，能正確保留您指定的自訂副檔名，不再自動加上 .txt。"""

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
이번 업데이트에서는 더욱 쾌적한 사용을 위해 다음과 같은 기능 추가와 개선을 진행했습니다.

・지원 언어 확대: 구문 강조에 25종의 언어 및 설정 파일 형식을 새롭게 추가하여 총 49개 카테고리(120종 이상의 확장자)를 지원합니다.
・입력 및 줄 번호 정렬 개선: 조합 중인 글자(IME 미확정 문자)가 에디터에 자연스럽게 표시되도록 수정하고, 전각 문자가 포함된 줄에서도 줄 번호가 깔끔하게 정렬되도록 개선했습니다.
・입력 조작 및 미리보기 개선: iOS에서의 Emmet 약어 전개(Tab 키), 화면 키보드 사용 시의 자동 스크롤, 미리보기 전환 시의 화면 표시를 개선했습니다.
・화면 레이아웃 최적화: iPad를 세로와 가로로 회전할 때나 다양한 화면 크기에서도 에디터 영역이 화면 가장자리에 꼭 맞게 표시되도록 수정했습니다.
・저장 기능 개선: '다른 이름으로 저장' 시 지정한 확장자 그대로 저장되도록 개선했습니다."""

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
Dieses Update bringt neue Funktionen und Verbesserungen für ein noch angenehmeres Arbeiten:

• Mehr unterstützte Sprachen: 25 neue Sprachen und Konfigurationsformate für die Syntaxhervorhebung hinzugefügt – insgesamt werden nun 49 Kategorien (über 120 Dateiendungen) unterstützt.
• Verbesserte Texteingabe & Ausrichtung: Optimierte Darstellung während der IME-Zeicheneingabe sowie präzise Ausrichtung der Zeilennummern bei Zeichen mit voller Breite.
• Optimierte Bedienung & Vorschau: Verbesserte Emmet-Erweiterung unter iOS, sanfteres automatisches Scrollen mit der Bildschirmtastatur und saubere Darstellung beim Umschalten der Live-Vorschau.
• Angepasstes Bildschirmlayout: Korrigierte Anpassung beim Drehen des iPads zwischen Hoch- und Querformat, sodass der Editor stets bündig mit dem Bildschirmrand abschließt.
• Verbessertes Speichern: „Speichern unter“ behält nun Ihre gewählte Dateiendung bei, ohne automatisch .txt anzuhängen."""

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
Cette mise à jour apporte de nouvelles fonctionnalités et des améliorations pour rendre votre expérience encore plus confortable :

• Prise en charge de nouveaux langages : ajout de 25 nouveaux langages et formats de configuration pour la coloration syntaxique, portant le total à 49 catégories (plus de 120 extensions).
• Saisie et alignement améliorés : meilleur affichage en direct lors de la saisie IME et alignement précis des numéros de ligne avec les caractères pleine chasse.
• Saisie au clavier et aperçu optimisés : amélioration du développement des abréviations Emmet sur iOS, du défilement automatique avec le clavier virtuel et de l’affichage lors du basculement de l’aperçu en direct.
• Mise en page ajustée : correction de l’adaptation de l’écran lors de la rotation entre les modes portrait et paysage sur iPad.
• Enregistrement amélioré : « Enregistrer sous » conserve désormais l’extension personnalisée que vous avez choisie sans ajouter .txt automatiquement."""

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
En esta actualización hemos añadido nuevas funciones y mejoras para ofrecerte una experiencia aún más cómoda:

• Más lenguajes compatibles: se añadieron 25 nuevos lenguajes y formatos de configuración al resaltado de sintaxis, alcanzando un total de 49 categorías (más de 120 extensiones).
• Mejoras en la entrada de texto y alineación: mejor visualización del texto en composición (IME) y alineación precisa de los números de línea al usar caracteres de ancho completo.
• Mejor experiencia de teclado y vista previa: se mejoró la expansión de Emmet en iOS, el desplazamiento automático con el teclado en pantalla y el dibujado al alternar la vista previa.
• Diseño de pantalla optimizado: se corrigió el ajuste del área de edición al girar el iPad entre vertical y horizontal para que siempre ocupe toda la pantalla de forma limpia.
• Mejora al guardar archivos: ahora «Guardar como» conserva la extensión personalizada que indiques sin añadir .txt automáticamente."""

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
Questo aggiornamento introduce nuove funzionalità e miglioramenti per rendere l’utilizzo ancora più confortevole:

• Nuovi linguaggi supportati: aggiunti 25 nuovi linguaggi e formati di configurazione per l’evidenziazione della sintassi, per un totale di 49 categorie (oltre 120 estensioni).
• Miglioramenti all’inserimento del testo: perfezionata la visualizzazione del testo durante la composizione IME e l’allineamento dei numeri di riga con i caratteri a larghezza intera.
• Tastiera e anteprima migliorate: ottimizzata l’espansione Emmet su iOS, lo scorrimento automatico con la tastiera su schermo e la resa grafica durante l’attivazione dell’anteprima.
• Layout dello schermo ottimizzato: risolto il ridimensionamento dell’editor durante la rotazione dell’iPad tra verticale e orizzontale.
• Salvataggio migliorato: "Salva con nome" ora mantiene correttamente l’estensione personalizzata senza aggiungere automaticamente .txt."""

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
Esta atualização traz novos recursos e melhorias para deixar sua experiência ainda mais confortável:

• Mais linguagens compatíveis: adicionamos 25 novas linguagens e formatos de configuração ao destaque de sintaxe, totalizando 49 categorias (mais de 120 extensões).
• Melhorias na digitação e alinhamento: aprimoramos a exibição de texto durante a composição (IME) e o alinhamento dos números de linha com caracteres de largura total.
• Teclado e pré-visualização aprimorados: melhoramos a expansão do Emmet no iOS, a rolagem automática com o teclado na tela e a renderização ao alternar a pré-visualização.
• Layout de tela otimizado: corrigimos o ajuste da área de edição ao girar o iPad entre os modos retrato e paisagem.
• Melhoria ao salvar: a opção "Salvar como" agora preserva a extensão personalizada informada sem adicionar .txt automaticamente."""

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
В этом обновлении мы добавили новые возможности и улучшения для ещё более комфортной работы:

• Расширена поддержка языков: добавлено 25 новых языков и форматов конфигурации для подсветки синтаксиса (всего 49 категорий и более 120 расширений файлов).
• Улучшен ввод текста и выравнивание: доработано отображение вводимых символов (IME) до подтверждения и выравнивание номеров строк при использовании полноширинных символов.
• Улучшены работа с клавиатурой и предпросмотр: исправлено разворачивание сокращений Emmet на iOS, улучшена автопрокрутка при вводе с экранной клавиатуры и отрисовка при переключении предпросмотра.
• Оптимизирована разметка экрана: исправлено масштабирование области редактора при повороте iPad между портретной и альбомной ориентацией.
• Улучшено сохранение файлов: при использовании «Сохранить как» теперь корректно сохраняется указанное вами расширение файла без автоматического добавления .txt."""

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
Deze update bevat nieuwe functies en verbeteringen voor nog meer gebruiksgemak:

• Meer ondersteunde talen: 25 nieuwe talen en configuratieformaten toegevoegd voor syntaxisaccentuering (nu in totaal 49 categorieën en 120+ extensies).
• Verbeterde tekstinvoer en uitlijning: betere weergave tijdens IME-tekstinvoer en nauwkeurige uitlijning van regelnummers bij tekens met volledige breedte.
• Soepeler typen en voorvertonen: verbeterde Emmet-uitbreiding op iOS, automatisch scrollen met het schermtoetsenbord en nettere weergave bij het schakelen van de voorvertoning.
• Geoptimaliseerde scherminrichting: de editor past nu altijd netjes tot aan de schermranden bij het draaien van de iPad tussen staand en liggend formaat.
• Verbeterd opslaan: 'Bewaar als' behoudt nu uw eigen bestandsextensie zonder automatisch .txt toe te voegen."""

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
Den här uppdateringen innehåller nya funktioner och förbättringar för en ännu smidigare upplevelse:

• Fler språk stöds: 25 nya språk och konfigurationsformat har lagts till för syntaxmarkering, totalt 49 kategorier (120+ filändelser).
• Förbättrad textinmatning och justering: Tydligare visning vid IME-inmatning samt exakt justering av radnummer för helbreddstecken.
• Bättre tangentbords- och förhandsvisningsfunktion: Förbättrad Emmet-utökning på iOS, mjukare rullning med skärmtangentbordet och snyggare uppritning vid växling av förhandsvisning.
• Optimerad skärmlayout: Redigeringsytan anpassar sig nu perfekt till skärmens kanter när iPad roteras mellan stående och liggande läge.
• Förbättrad filsparning: "Spara som" behåller nu din valda filändelse utan att automatiskt lägga till .txt."""

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
Denne opdatering indeholder nye funktioner og forbedringer for en endnu bedre oplevelse:

• Flere understøttede sprog: Tilføjet 25 nye sprog og konfigurationsformater til syntaksfremhævning – nu i alt 49 kategorier (over 120 filendelser).
• Forbedret tekstindtastning og justering: Bedre visning under IME-indtastning samt præcis justering af linjenumre ved fuldbreddetegn.
• Bedre tastatur- og forhåndsvisningsoplevelse: Forbedret Emmet-udvidelse på iOS, mere jævn automatisk rulning med skærmtastaturet og pænere visning ved skift af forhåndsvisning.
• Optimeret skærmlayout: Rettet tilpasning ved rotation af iPad mellem stående og liggende format.
• Forbedret arkivering: "Gem som" bevarer nu din valgte filendelse uden automatisk at tilføje .txt."""

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
Denne oppdateringen gir nye funksjoner og forbedringer for en enda bedre brukeropplevelse:

• Flere støttede språk: Lagt til 25 nye språk og konfigurasjonsformater for syntaksutheving, totalt 49 kategorier (over 120 filtyper).
• Forbedret tekstinndata og justering: Bedre visning under IME-skriving og nøyaktig justering av linjenumre for fullbreddetegn.
• Bedre tastatur- og forhåndsvisningsopplevelse: Forbedret Emmet-utvidelse på iOS, jevnere rulling med skjermtastaturet og renere opptegning ved bytte av forhåndsvisning.
• Optimalisert skjermoppsett: Rettet tilpasning når iPad roteres mellom stående og liggende retning.
• Forbedret lagring: «Arkiver som» beholder nå filtypen du oppgir uten å legge til .txt automatisk."""

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
Tämä päivitys tuo uusia ominaisuuksia ja parannuksia entistä sujuvampaan käyttöön:

• Laajempi kielituki: Lisätty 25 uutta syntaksinkorostuskieltä ja asetustiedostomuotoa – yhteensä 49 kategoriaa (yli 120 tiedostopäätettä).
• Parannettu tekstinsyöttö ja kohdistus: Selkeämpi IME-syötön esikatselu sekä tarkka rivinumeroiden kohdistus täysleveiden merkkien kanssa.
• Sujuvampi kirjoitus ja esikatselu: Parannettu Emmet-laajennus iOS:llä, automaattinen vieritys näyttönäppäimistöllä ja siistimpi piirto esikatselua vaihdettaessa.
• Optimoitu näytön asettelu: Korjattu muokkausalueen sovitus iPadia pysty- ja vaakasuuntaan käännettäessä.
• Parannettu tallennus: "Tallenna nimellä" säilyttää nyt valitsemasi tiedostopäätteen lisäämättä .txt-päätettä automaattisesti."""

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
W tej aktualizacji wprowadziliśmy nowe funkcje i ulepszenia zwiększające komfort pracy:

• Rozszerzona obsługa języków: dodano 25 nowych języków i formatów konfiguracji do kolorowania składni (łącznie 49 kategorii i ponad 120 rozszerzeń).
• Ulepszone wprowadzanie tekstu i wyrównanie: poprawiono wyświetlanie tekstu w trakcie składania znaków (IME) oraz wyrównanie numerów wierszy przy znakach pełnej szerokości.
• Lepsza obsługa klawiatury i podglądu: usprawniono rozwijanie skrótów Emmet w systemie iOS, automatyczne przewijanie z klawiaturą ekranową oraz renderowanie przy przełączaniu podglądu.
• Zoptymalizowany układ ekranu: poprawiono dopasowanie obszaru edytora podczas obracania iPada między orientacją pionową a poziomą.
• Ulepszone zapisywanie plików: opcja „Zapisz jako” zachowuje teraz wybrane rozszerzenie pliku bez automatycznego dodawania .txt."""

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
Tato aktualizace přináší nové funkce a vylepšení pro ještě pohodlnější práci:

• Rozšířená podpora jazyků: přidáno 25 nových jazyků a konfiguračních formátů pro zvýrazňování syntaxe, celkem nyní 49 kategorií (přes 120 přípon).
• Vylepšené zadávání textu a zarovnání: lepší zobrazení rozepsaného textu (IME) a přesné zarovnání čísel řádků u znaků s plnou šířkou.
• Lepší psaní a náhled: vylepšeno rozbalování zkratek Emmet na iOS, plynulejší automatické posouvání se softwarovou klávesnicí a čistší vykreslování při přepínání náhledu.
• Optimalizované rozvržení obrazovky: opraveno přizpůsobení editoru při otáčení iPadu mezi zobrazením na výšku a na šířku.
• Vylepšené ukládání: funkce „Uložit jako“ nyní zachová vámi zadanou příponu souboru bez automatického přidávání .txt."""

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
Ez a frissítés új funkciókat és finomításokat tartalmaz a még kényelmesebb használat érdekében:

• Bővített nyelvi támogatás: 25 új nyelvvel és konfigurációs formátummal bővült a szintaxiskiemelés, így már összesen 49 kategóriát (több mint 120 kiterjesztést) támogat.
• Javított szövegbevitel és igazítás: pontosabb megjelenítés IME-szövegbevitel közben, valamint szépen igazított sorszámok a teljes szélességű karaktereket tartalmazó soroknál.
• Jobb gépelési és előnézeti élmény: javított Emmet-kibontás iOS-en, egyenletesebb automatikus görgetés a képernyő-billentyűzettel, és tisztább megjelenítés az előnézet váltásakor.
• Optimalizált képernyőelrendezés: javítottuk a szerkesztőterület illeszkedését az iPad álló és fekvő tájolása közötti forgatáskor.
• Fejlesztett mentés: a „Mentés másként” funkció mostantól megtartja a megadott egyéni fájlkiterjesztést anélkül, hogy automatikusan .txt-t fűzne hozzá."""

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
Această actualizare aduce funcții noi și îmbunătățiri pentru o experiență și mai confortabilă:

• Mai multe limbaje suportate: am adăugat 25 de limbaje și formate de configurare noi pentru evidențierea sintaxei, ajungând la un total de 49 de categorii (peste 120 de extensii).
• Introducere de text și aliniere îmbunătățite: afișare mai clară în timpul compoziției IME și aliniere precisă a numerelor de rând pentru caracterele cu lățime completă.
• Tastare și previzualizare optimizate: extindere Emmet îmbunătățită pe iOS, derulare automată mai lină cu tastatura pe ecran și afișare mai curată la comutarea previzualizării.
• Aspect optimizat al ecranului: am corectat adaptarea zonei de editare la rotirea iPad-ului între modurile portret și peisaj.
• Salvare îmbunătățită: opțiunea „Salvează ca” păstrează acum extensia personalizată fără a adăuga automat .txt."""

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
Αυτή η ενημέρωση φέρνει νέες δυνατότητες και βελτιώσεις για ακόμα πιο άνετη εμπειρία:

• Υποστήριξη περισσότερων γλωσσών: προστέθηκαν 25 νέες γλώσσες και μορφές αρχείων ρυθμίσεων στην επισήμανση σύνταξης, φτάνοντας συνολικά τις 49 κατηγορίες (120+ επεκτάσεις).
• Βελτιωμένη εισαγωγή κειμένου και ευθυγράμμιση: καλύτερη εμφάνιση κατά τη σύνθεση χαρακτήρων (IME) και ακριβής ευθυγράμμιση αριθμών γραμμής με χαρακτήρες πλήρους πλάτους.
• Καλύτερη εμπειρία πληκτρολόγησης και προεπισκόπησης: βελτιώθηκε η ανάπτυξη Emmet στο iOS, η αυτόματη κύλιση με το πληκτρολόγιο οθόνης και η απεικόνιση κατά την εναλλαγή της προεπισκόπησης.
• Βελτιστοποιημένη διάταξη οθόνης: διορθώθηκε η προσαρμογή της περιοχής επεξεργασίας κατά την περιστροφή του iPad μεταξύ κατακόρυφου και οριζόντιου προσανατολισμού.
• Βελτιωμένη αποθήκευση: η «Αποθήκευση ως» διατηρεί πλέον την προσαρμοσμένη επέκταση αρχείου χωρίς αυτόματη προσθήκη του .txt."""

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
Bu güncellemede daha rahat bir kullanım için aşağıdaki yenilikleri ve iyileştirmeleri yaptık:

• Genişletilmiş Dil Desteği: Sözdizimi vurgulamaya 25 yeni dil ve yapılandırma biçimi eklendi; artık toplam 49 kategori (120'den fazla uzantı) destekleniyor.
• Geliştirilmiş Metin Girişi ve Hizalama: IME ile yazım sırasındaki onaylanmamış metin görünümü iyileştirildi ve tam genişlikli karakter içeren satırlarda satır numarası hizalaması düzeltildi.
• Klavye ve Önizleme İyileştirmeleri: iOS'te Emmet kısaltma genişletmesi, ekran klavyesiyle yazarken otomatik kaydırma ve önizleme geçişlerindeki çizim performansı iyileştirildi.
• Optimize Edilmiş Ekran Düzeni: iPad'i dikey ve yatay konumlar arasında döndürürken düzenleme alanının ekran kenarlarına tam oturması sağlandı.
• Kaydetme İyileştirmesi: "Farklı Kaydet" kullanılırken belirlediğiniz dosya uzantısının sonuna otomatik .txt eklenmeden korunması sağlandı."""

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
У цьому оновленні ми додали нові функції та покращення для ще комфортнішої роботи:

• Розширена підтримка мов: додано 25 нових мов і форматів конфігурації для підсвічування синтаксису — загалом 49 категорій (понад 120 розширень).
• Покращене введення тексту та вирівнювання: вдосконалено відображення тексту під час набору через IME та вирівнювання номерів рядків із повноширинними символами.
• Покращена робота з клавіатурою та переглядом: виправлено розгортання Emmet на iOS, покращено автопрокрутку з екранною клавіатурою та відображення під час перемикання попереднього перегляду.
• Оптимізована розмітка екрана: виправлено масштабування області редактора під час повороту iPad між книжковою та альбомною орієнтацією.
• Покращене збереження: функція «Зберегти як» тепер зберігає вказане вами розширення файлу без автоматичного додавання .txt."""

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
Bản cập nhật này bổ sung thêm tính năng mới và các cải tiến giúp việc soạn thảo thuận tiện hơn:

• Mở rộng ngôn ngữ hỗ trợ: Thêm 25 ngôn ngữ và định dạng tệp cấu hình mới cho tô màu cú pháp, nâng tổng số lên 49 danh mục (hơn 120 phần mở rộng tệp).
• Cải thiện bộ gõ và căn lề: Hiển thị rõ ràng các ký tự đang gõ chưa xác nhận (IME) và căn chỉnh số dòng thẳng hàng với các ký tự full-width.
• Cải thiện thao tác bàn phím và Xem trước: Khắc phục mở rộng gõ tắt Emmet trên iOS, tự động cuộn mượt hơn khi dùng bàn phím ảo và hiển thị ổn định khi bật/tắt chế độ xem trước.
• Tối ưu bố cục màn hình: Điều chỉnh vùng soạn thảo vừa khít viền màn hình khi xoay dọc/ngang trên iPad.
• Cải thiện lưu tệp: Tính năng "Lưu thành" giữ nguyên phần mở rộng tệp tùy chỉnh của bạn mà không tự động thêm đuôi .txt."""

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
ในการอัปเดตครั้งนี้ เราได้เพิ่มฟีเจอร์และปรับปรุงการทำงานเพื่อให้คุณใช้งานได้สะดวกสบายยิ่งขึ้น:

• เพิ่มภาษาที่รองรับ: เพิ่มการไฮไลต์ไวยากรณ์อีก 25 ภาษาและไฟล์ตั้งค่า รวมเป็น 49 หมวดหมู่ (มากกว่า 120 นามสกุลไฟล์)
• ปรับปรุงการพิมพ์และการจัดแนวบรรทัด: แสดงข้อความระหว่างกำลังพิมพ์ (IME) ได้อย่างชัดเจน และปรับตำแหน่งเลขบรรทัดให้ตรงกับอักษรแบบเต็มความกว้างอย่างสวยงาม
• ปรับปรุงการพิมพ์และโหมดพรีวิว: ปรับปรุงการขยายตัวย่อ Emmet บน iOS การเลื่อนหน้าจออัตโนมัติเมื่อใช้แป้นพิมพ์บนหน้าจอ และการแสดงผลเมื่อสลับโหมดพรีวิว
• ปรับแต่งเลย์เอาต์หน้าจอ: แก้ไขพื้นที่แสดงผลของเอดิเตอร์ให้พอดีกับขอบหน้าจอเมื่อหมุน iPad สลับระหว่างแนวตั้งและแนวนอน
• ปรับปรุงการบันทึกไฟล์: "บันทึกเป็น" สามารถใช้นามสกุลไฟล์ที่คุณกำหนดเองได้โดยไม่เติม .txt ต่อท้ายโดยอัตโนมัติ"""

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
Pembaruan kali ini menghadirkan penambahan fitur dan peningkatan agar pengalaman menulis Anda semakin nyaman:

• Penambahan Bahasa: Menambahkan 25 bahasa dan format konfigurasi baru pada sorotan sintaks, kini mendukung total 49 kategori (lebih dari 120 ekstensi file).
• Peningkatan Input Teks & Perataan Baris: Memperbaiki tampilan teks saat proses pengetikan IME serta merapikan perataan nomor baris pada karakter lebar penuh.
• Peningkatan Keyboard & Pratinjau: Memperbaiki ekspansi Emmet di iOS, pengguliran otomatis saat mengetik dengan keyboard layar, serta tampilan saat mengalihkan pratinjau.
• Optimalisasi Tata Letak Layar: Memperbaiki ukuran area editor agar selalu pas hingga ke tepi layar saat memutar iPad antara posisi vertikal dan horizontal.
• Peningkatan Penyimpanan File: Fitur "Simpan Sebagai" kini mempertahankan ekstensi file pilihan Anda tanpa menambahkan .txt secara otomatis."""

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
Kemas kini ini membawakan ciri baharu dan penambahbaikan untuk pengalaman yang lebih selesa:

• Sokongan Bahasa Diperluas: Menambah 25 bahasa dan format konfigurasi baharu untuk penyerlahan sintaks, kini menyokong 49 kategori (lebih 120 sambungan fail).
• Penambahbaikan Input Teks & Penjajaran: Memperbaiki paparan teks semasa menaip melalui IME dan menjajarkan nombor baris dengan kemas bagi aksara lebar penuh.
• Papan Kekunci & Pratonton Lebih Baik: Memperbaiki pengembangan Emmet pada iOS, tatalan automatik dengan papan kekunci skrin dan paparan semasa menukar pratonton.
• Susun Atur Skrin Dioptimumkan: Memastikan ruang penyunting sentiasa muat dengan kemas pada tepi skrin apabila memutar iPad antara mod potret dan landskap.
• Penambahbaikan Penyimpanan: "Simpan Sebagai" kini mengekalkan sambungan fail pilihan anda tanpa menambah .txt secara automatik."""

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
इस अपडेट में आपके अनुभव को और बेहतर बनाने के लिए निम्नलिखित नई सुविधाएँ और सुधार किए गए हैं:

• अधिक भाषाओं का समर्थन: सिंटैक्स हाइलाइटिंग में 25 नई भाषाएँ और कॉन्फ़िगरेशन फ़ॉर्मेट जोड़े गए हैं, जिससे अब कुल 49 श्रेणियाँ (120+ एक्सटेंशन) समर्थित हैं।
• बेहतर टेक्स्ट इनपुट और संरेखण: IME इनपुट के दौरान अक्षरों के लाइव प्रदर्शन में सुधार किया गया है और पूर्ण-चौड़ाई (full-width) वाले अक्षरों के साथ लाइन नंबरों को सटीक रूप से संरेखित किया गया है।
• कीबोर्ड और प्रीव्यू में सुधार: iOS पर Emmet विस्तार, ऑन-स्क्रीन कीबोर्ड के साथ ऑटो-स्क्रॉलिंग और लाइव प्रीव्यू बदलते समय रेंडरिंग को बेहतर बनाया गया है।
• स्क्रीन लेआउट अनुकूलन: iPad को पोर्ट्रेट और लैंडस्केप में घुमाने पर एडिटर क्षेत्र अब स्क्रीन के किनारों तक पूरी तरह फ़िट रहता है।
• फ़ाइल सहेजने में सुधार: "इस रूप में सहेजें" (Save As) में अब आपके द्वारा दिया गया कस्टम एक्सटेंशन सुरक्षित रहता है और अपने आप .txt नहीं जुड़ता।"""

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
            if locale in existing_locales:
                loc_obj = existing_locales[locale]
                loc_id = loc_obj["id"]
                cur_url = (loc_obj.get("attributes", {}).get("privacyPolicyUrl") or "").strip()
                if cur_url == privacy_url:
                    updated_count += 1
                    print(f"  [OK] AppInfo {locale} privacyPolicyUrl already set ({privacy_url})")
                    continue
                patch_payload = {
                    "data": {
                        "type": "appInfoLocalizations",
                        "id": loc_id,
                        "attributes": {
                            "privacyPolicyUrl": privacy_url,
                        },
                    }
                }
                res_p = requests.patch(f"{BASE_URL}/appInfoLocalizations/{loc_id}", headers=headers, json=patch_payload)
                if res_p.status_code == 200:
                    updated_count += 1
                    print(f"  [OK] Updated AppInfo {locale} privacyPolicyUrl -> {privacy_url}")
                else:
                    print(f"  [ERR] Failed to update AppInfo {locale} (HTTP {res_p.status_code}: {res_p.text[:200]})")
            else:
                payload = {
                    "data": {
                        "type": "appInfoLocalizations",
                        "attributes": {
                            "locale": locale,
                            "name": "miu",
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
                    print(f"  [+] Created AppInfo {locale} (name: 'miu', privacyPolicyUrl: {privacy_url})")
                else:
                    payload["data"]["attributes"]["name"] = app_name
                    res2 = requests.post(f"{BASE_URL}/appInfoLocalizations", headers=headers, json=payload)
                    if res2.status_code in (200, 201):
                        created_count += 1
                        print(f"  [+] Created AppInfo {locale} (name: '{app_name}', privacyPolicyUrl: {privacy_url})")
                    else:
                        print(f"  [!] Skipped AppInfo {locale} (HTTP {res2.status_code}: {res2.text[:200]})")
        print(f"AppInfo Summary (ID: {info_id}): {updated_count} updated/verified, {created_count} created.")


def get_or_create_editable_versions(app_id, headers):
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
    for plat in ["IOS", "MAC_OS"]:
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
            if v_str != TARGET_VERSION and v_state == "PREPARE_FOR_SUBMISSION":
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
                        failed_count += 1
                        print(f"  [ERR] Failed to update {locale}: HTTP {res2.status_code} {res2.text}")
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


def main():
    print("=== App Store Connect Metadata Synchronizer for miu ===")
    headers = get_headers()
    app_id, app_name = get_app_id(headers)

    sync_app_info_localizations(app_id, app_name, headers)
    targets = get_or_create_editable_versions(app_id, headers)

    total_ok = 0
    total_fail = 0
    for plat, v_id, v_str, state, has_live in targets:
        # Refresh token before each platform sync
        headers = get_headers()
        ok, fail = sync_version_localizations(plat, v_id, v_str, state, has_live, headers)
        total_ok += ok
        total_fail += fail

    print(f"\n=== Completed: {total_ok} localizations synced across {len(targets)} platform(s) (Failures: {total_fail}) ===")
    if total_ok == 0 and total_fail > 0:
        sys.exit(1)


if __name__ == "__main__":
    main()
