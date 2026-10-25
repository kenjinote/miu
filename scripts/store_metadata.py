# -*- coding: utf-8 -*-
"""
Store listing metadata for all 10 expanded languages (Tier 1 & Tier 2).
"""

METADATA_10_LANGS = {
    "zh-tw": {
        "title": "miu",
        "description": """miu 是一款專為 Windows 設計的高效能、極度輕量的純文字編輯器。
無論是快速記筆記、查看超大紀錄檔（Log），還是日常撰寫程式碼，miu 都能提供毫秒級的冷啟動速度與絲滑流暢的操作體驗。

【核心亮點】
• 極致效能：基於現代 Piece Table 資料結構與記憶體映射技術，開啟並編輯數 GB 等級的龐大文字檔案依然游刃有餘。
• 管理員權限提權保存（UAC）：編輯位於 C 槽根目錄或受保護系統區域的檔案時，可透過安全性對話框提升為管理員權限儲存，且取消時絕不遺失任何已編輯的文字與 Undo 歷史。
• 現代美學介面：完美支援 Windows 深色模式與淺色模式，結合 Direct2D / DirectWrite 硬體加速技術，字型清晰銳利、滾動流暢。
• 強大編輯功能：支援多游標同時編輯、矩形區塊選擇（Alt+拖曳）、正規表示式（Regex）尋找與取代、括號自動配對跳轉。
• 程式碼語法標記：內建 C/C++、Python、JavaScript、HTML、CSS、Markdown 等多種常見語言語法高亮。
• 智慧編碼辨識：精準自動偵測 UTF-8、Big5、ANSI 及各國編碼，徹底告別文字亂碼問題。
• 隱私安全保證：完全離線運作，絕無任何資料收集或後台連線，輕巧純淨。""",
        "features": [
            "多游標與矩形區塊選取：支援多點游標同步輸入與區塊編輯。",
            "極致 Piece Table 架構：流暢開啟與即時編輯數 GB 等級超大檔案。",
            "UAC 管理員權限提權保存：安全儲存 C 槽或受保護的系統檔案。",
            "深色與淺色模式：完美適配 Windows 主題，搭配清晰銳利的硬體加速文字渲染。",
            "全方位編碼偵測：精準自動識別 UTF-8、Big5 及各式編碼。",
            "強大搜尋與取代：支援正規表達式、大小寫區分與全字比對。"
        ],
        "keywords": [
            "文字編輯器",
            "記事本",
            "編輯器",
            "程式碼編輯器",
            "Notepad",
            "Text Editor",
            "UTF-8"
        ],
        "releaseNotes": """miu v1.0.38 更新日誌
🌐 新增 WebView2 即時預覽（Alt+P）與 CommonMark / GFM Markdown 精準渲染

・內建 WebView2 分割畫面即時預覽（按 Alt+P 開啟）：
  支援 HTML、SVG 與 Markdown 的分割即時預覽，具備自動防抖重新整理、F5 手動重整、可拖曳極簡分隔線、自適應淺色/深色主題、疊加式捲軸以及本機圖片/CSS/JS 資源映射。
・忠實還原 CommonMark 與 GitHub 風格 Markdown (GFM)：
  全面支援表格（對齊與跳脫直線）、工作清單（- [ ] / - [x]）、多層巢狀清單、多行與巢狀引用、GFM 提示框（[!NOTE]、[!TIP]、[!WARNING] 等）、多重反引號程式碼、snake_case 底線保護、自動連結及 <kbd> 等行內 HTML 標籤。
・超大檔案極速安全預覽與 F3 循環尋找修復：
  無需任何磁碟複製即可按 UTF-8 與行邊界極速預覽前 512 KB 內容（附全 28 種語言截斷提示），並修復了大檔案末尾按 F3 折返尋找時的停頓問題。""",
        "image_captions": [
            "淺色模式下的多功能文字編輯",
            "深色模式下的代碼語法高亮",
            "F1 快速鍵與說明導覽視窗"
        ],
        "prefix": "zh_tw"
    },

    "de-de": {
        "title": "miu",
        "description": """miu ist ein extrem schneller, ressourcenschonender und moderner Texteditor für Windows.
Vom schnellen Notieren bis zum Durchsuchen und Bearbeiten riesiger Protokolldateien (Logs) bietet miu eine kompromisslose Leistung mit blitzschnellem Start.

【Highlights】
• Überragende Leistung: Basierend auf einer fortschrittlichen Piece-Table-Architektur lassen sich selbst gigabytelange Dateien ohne spürbare Ladezeiten flüssig bearbeiten.
• Speichern mit Administratorrechten (UAC): Bearbeiten Sie geschützte Dateien (z. B. auf C:\\) und speichern Sie diese sicher per UAC-Berechtigung – ohne Risiko von Datenverlust bei Abbruch.
• Elegantes Windows-Design: Vollständige Unterstützung von Dark Mode und Light Mode sowie Direct2D/DirectWrite-Hardwarebeschleunigung für gestochen scharfen Text.
• Moderne Editor-Werkzeuge: Multi-Cursor-Unterstützung, Rechteckauswahl (Alt+Ziehen), Regex-Suche und -Ersetzen, Zeilensprung und Klammerpaare.
• Syntaxhervorhebung: Integrierte Syntaxfarben für C/C++, Python, Webformate, Markdown und mehr.
• Zuverlässige Zeichencodierung: Automatische und präzise Erkennung von UTF-8, ANSI und regionalen Zeichensätzen.
• Privatsphäre & Sicherheit: Vollständig offline, ohne Telemetrie, ohne Tracker – schlank und zuverlässig.""",
        "features": [
            "Multi-Cursor & Blockauswahl: Gleichzeitiges Bearbeiten an mehreren Textstellen.",
            "Piece-Table-Architektur: Müheloses Öffnen und Bearbeiten gigantischer Dateien.",
            "UAC-Rechteerweiterung: Sicheres Speichern geschützter Systemdateien.",
            "Dunkel- und Hellmodus: Nahtlose Integration in das moderne Windows-Erscheinungsbild.",
            "Automatische Codierungserkennung: Sofortige Erkennung von UTF-8, ANSI und mehr.",
            "Leistungsstarke Suche: Regex-Unterstützung, Ganzwortsuche und Ersetzen."
        ],
        "keywords": [
            "Texteditor",
            "Notepad Alternative",
            "Editor",
            "Quelltext",
            "Schnell",
            "Leicht",
            "Code Editor"
        ],
        "releaseNotes": """miu v1.0.38 Versionshinweise
🌐 WebView2-Live-Vorschau (Alt+P) & originalgetreues CommonMark / GFM Markdown-Rendering

• Integrierte WebView2-Live-Vorschau für HTML, SVG & Markdown (Alt+P):
  Öffnen Sie mit Alt+P eine geteilte Live-Vorschau mit automatischer Aktualisierung, F5-Neuladen, verschiebbarem Splitter, Overlay-Bildlaufleisten, Dark-/Light-Mode-Anpassung und lokaler Ressourcen-Zuordnung (Bilder, CSS, JS).
• Originalgetreues CommonMark & GitHub Flavored Markdown (GFM):
  Volle Unterstützung für GFM-Tabellen (Ausrichtung & maskierte Pipes), Aufgabenlisten (- [ ] / - [x]), verschachtelte Listen, mehrzeilige & verschachtelte Zitate, GFM-Hinweise ([!NOTE], [!TIP], [!WARNING] usw.), Mehrfach-Backticks, snake_case-Schutz, Autolinks und Inline-HTML (<kbd>, <br>).
• Schnelle Vorschau für riesige Dateien & verzögerungsfreie F3-Suche:
  Zeigt bei großen Dateien sofort die ersten 512 KB ohne Dateikopien an (lokalisiert in 28 Sprachen) und behebt ein Einfrieren beim Weitersuchen (F3) am Dateiende.""",
        "image_captions": [
            "Heller Modus mit Markdown- und Code-Bearbeitung",
            "Dunkler Modus mit lebendiger Syntaxhervorhebung",
            "F1 Hilfe und Tastaturkürzel-Übersicht"
        ],
        "prefix": "de"
    },

    "fr-fr": {
        "title": "miu",
        "description": """miu est un éditeur de texte ultra-rapide, léger et moderne conçu pour Windows.
Idéal pour la prise de notes instantanée, l'analyse de fichiers journaux volumineux (logs) ou le développement au quotidien, miu allie minimalisme et puissance absolue.

【Points forts】
• Performances inégalées : Grâce à son architecture Piece Table et à la mémoire paginée, éditez des fichiers de plusieurs gigaoctets avec une fluidité totale.
• Enregistrement administrateur (UAC) : Modifiez et sauvegardez en toute sécurité des fichiers protégés (comme la racine C:\\) sans jamais perdre votre travail en cours en cas d'annulation.
• Design épuré et moderne : Intégration parfaite du mode sombre et du mode clair de Windows 11 avec accélération matérielle Direct2D pour une netteté de police irréprochable.
• Outils d'édition avancés : Multi-curseurs, sélection en bloc rectangulaire (Alt+glisser), recherche et remplacement avec expressions régulières (Regex).
• Coloration syntaxique : Prise en charge native de C/C++, Python, formats Web, Markdown et bien d'autres.
• Détection intelligente des encodages : Reconnaissance automatique sans faille de l'UTF-8, ANSI et des pages de codes régionales.
• Confidentialité absolue : Aucun suivi télémétrique, fonctionnement 100 % hors ligne, rapide et discret.""",
        "features": [
            "Multi-curseurs et sélection en bloc pour des retouches simultanées.",
            "Architecture Piece Table ultra-performante pour les fichiers géants.",
            "Élévation UAC sécurisée pour enregistrer des fichiers système protégés.",
            "Modes clair et sombre adaptés au thème Windows avec rendu Direct2D haute précision.",
            "Détection d'encodage universelle et automatique (UTF-8, ANSI, etc.).",
            "Recherche avancée avec prise en charge intégrale des expressions régulières (Regex)."
        ],
        "keywords": [
            "éditeur de texte",
            "bloc-notes",
            "notepad",
            "éditeur léger",
            "code",
            "ultra rapide",
            "texte"
        ],
        "releaseNotes": """Notes de version miu v1.0.38
🌐 Aperçu en direct WebView2 (Alt+P) et rendu Markdown CommonMark / GFM fidèle

• Aperçu fractionné en direct WebView2 pour HTML, SVG et Markdown (Alt+P) :
  Appuyez sur Alt+P pour afficher un volet d’aperçu en temps réel avec mise à jour automatique, rechargement F5, séparateur redimensionnable, barres de défilement superposées, adaptation au thème clair/sombre et chargement des ressources locales (images, CSS, JS).
• Rendu fidèle CommonMark et GitHub Flavored Markdown (GFM) :
  Prise en charge complète des tableaux GFM, listes de tâches (- [ ] / - [x]), listes imbriquées, citations multilignes et alertes GFM ([!NOTE], [!TIP], [!WARNING], etc.), blocs de code, protection snake_case, liens automatiques et balises HTML en ligne (<kbd>).
• Optimisation pour fichiers géants et recherche circulaire (F3) instantanée :
  Prévisualise instantanément les premiers 512 Ko sans copie disque (traduit en 28 langues) et corrige le blocage lors du retour au début du fichier avec F3.""",
        "image_captions": [
            "Mode clair avec rendu précis des polices et indentation",
            "Mode sombre élégant avec coloration syntaxique",
            "Superposition d'aide F1 et raccourcis clavier"
        ],
        "prefix": "fr"
    },

    "es-es": {
        "title": "miu",
        "description": """miu es un editor de texto ultrarrápido, minimalista y de alto rendimiento para Windows.
Diseñado para abrirse en una fracción de segundo, es perfecto tanto para notas rápidas como para inspeccionar enormes archivos de registro (logs) o programar con total comodidad.

【Aspectos destacados】
• Rendimiento extremo: Construido sobre una estructura Piece Table que permite abrir y editar archivos de varios gigabytes sin congelamientos ni esperas.
• Guardado con permisos de administrador (UAC): Guarde archivos del sistema o protegidos (como en la raíz C:\\) mediante elevación segura, conservando siempre sus cambios si cancela.
• Interfaz nativa y elegante: Totalmente adaptado al Modo Claro y Oscuro de Windows 11 con renderizado acelerado por hardware Direct2D/DirectWrite.
• Edición avanzada: Multicursor simultáneo, selección rectangular en bloque (Alt+arrastrar), búsqueda y reemplazo con expresiones regulares (Regex).
• Resaltado de sintaxis: Compatible con C/C++, Python, HTML, CSS, JavaScript, Markdown y más.
• Detección automática de codificación: Reconocimiento inteligente de UTF-8, ANSI y codificaciones regionales para evitar caracteres dañados.
• Privacidad total: Completamente sin conexión, sin telemetría ni recopilación de datos.""",
        "features": [
            "Multicursor y selección por bloques rectangulares para edición simultánea.",
            "Arquitectura Piece Table para editar archivos gigantes de varios gigabytes.",
            "Guardado con privilegios de administrador UAC para archivos protegidos.",
            "Soporte de temas claro y oscuro con renderizado nítido Direct2D.",
            "Detección inteligente de codificaciones UTF-8, ANSI y tradicionales.",
            "Búsqueda potente con expresiones regulares y concordancia exacta."
        ],
        "keywords": [
            "editor de texto",
            "bloc de notas",
            "notepad",
            "editor ligero",
            "codigo",
            "rapido",
            "texto"
        ],
        "releaseNotes": """Notas de la versión miu v1.0.38
🌐 Vista previa en vivo con WebView2 (Alt+P) y renderizado fiel de Markdown (CommonMark / GFM)

• Vista previa dividida en tiempo real para HTML, SVG y Markdown (Alt+P):
  Pulsa Alt+P para abrir un panel de vista previa con actualización automática, recarga con F5, divisor redimensionable, barras de desplazamiento superpuestas, modo claro/oscuro y carga de recursos locales (imágenes, CSS, JS).
• Renderizado fiel de CommonMark y GitHub Flavored Markdown (GFM):
  Soporte completo para tablas GFM, listas de tareas (- [ ] / - [x]), listas anidadas, citas multinivel, alertas GFM ([!NOTE], [!TIP], [!WARNING], etc.), protección snake_case, enlaces automáticos y etiquetas HTML en línea (<kbd>).
• Vista previa ultrarrápida en archivos gigantes y búsqueda F3 optimizada:
  Previsualiza al instante los primeros 512 KB sin copias en disco (en 28 idiomas) y corrige el bloqueo al reiniciar la búsqueda con F3 al final del archivo.""",
        "image_captions": [
            "Modo claro con edición de texto y código",
            "Modo oscuro con resaltado de sintaxis",
            "Ventana de ayuda F1 con lista de atajos"
        ],
        "prefix": "es"
    },

    "pt-br": {
        "title": "miu",
        "description": """miu é um editor de texto ultraleve, incrivelmente rápido e moderno para Windows.
Projetado para inicializar em milissegundos, é a escolha perfeita para anotações rápidas, análise de logs gigantescos e edição de código sem distrações.

【Destaques】
• Desempenho Imbatível: Graças à arquitetura Piece Table, manipule arquivos imensos de vários gigabytes com uso mínimo de memória e resposta imediata.
• Salvamento com privilégio de Administrador (UAC): Edite e salve arquivos protegidos (como na raiz C:\\) com segurança, sem risco de perder o texto digitado ao cancelar.
• Visual Nativo do Windows 11: Suporte completo aos modos Claro e Escuro, com renderização gráfica acelerada por hardware via Direct2D/DirectWrite.
• Recursos Avançados de Edição: Múltiplos cursores, seleção em bloco retangular (Alt+arrastar), busca e substituição com expressões regulares (Regex).
• Realce de Sintaxe: Destaque inteligente de sintaxe para C/C++, Python, Web (HTML, CSS, JS), Markdown e outros formatos.
• Detecção Universal de Codificação: Reconhece com precisão UTF-8, ANSI e codificações regionais sem corromper caracteres.
• Privacidade Total: 100% offline, sem telemetria, sem processos em segundo plano desnecessários.""",
        "features": [
            "Multicursores e seleção em bloco retangular para edições simultâneas.",
            "Arquitetura Piece Table que suporta arquivos gigantescos de vários gigabytes.",
            "Elevação UAC segura para salvar arquivos de sistema protegidos.",
            "Modos Claro e Escuro com renderização de texto nítida e fluida via Direct2D.",
            "Detecção precisa de codificação para UTF-8, ANSI e conjuntos legados.",
            "Busca avançada com suporte completo a Expressões Regulares (Regex)."
        ],
        "keywords": [
            "editor de texto",
            "bloco de notas",
            "notepad",
            "editor leve",
            "rapido",
            "programacao",
            "texto"
        ],
        "releaseNotes": """Notas de lançamento do miu v1.0.38
🌐 Pré-visualização ao vivo WebView2 (Alt+P) e renderização fiel de Markdown (CommonMark / GFM)

• Pré-visualização dividida em tempo real para HTML, SVG e Markdown (Alt+P):
  Pressione Alt+P para abrir o painel de pré-visualização com atualização automática, recarregamento F5, divisor ajustável, barras de rolagem sobrepostas, tema claro/escuro e mapeamento de arquivos locais (imagens, CSS, JS).
• Renderização fiel de CommonMark e GitHub Flavored Markdown (GFM):
  Suporte completo para tabelas GFM, listas de tarefas (- [ ] / - [x]), listas aninhadas, citações em vários níveis, alertas GFM ([!NOTE], [!TIP], [!WARNING], etc.), proteção snake_case, links automáticos e HTML inline (<kbd>).
• Prévia rápida para arquivos gigantes e busca F3 instantânea:
  Exibe instantaneamente os primeiros 512 KB sem cópias em disco (em 28 idiomas) e corrige o travamento ao reiniciar a busca com F3 no final de arquivos grandes.""",
        "image_captions": [
            "Modo claro com edição limpa e visualização nítida",
            "Modo escuro com realce de sintaxe colorido",
            "Painel de ajuda F1 com atalhos de teclado"
        ],
        "prefix": "pt_br"
    },

    "ru-ru": {
        "title": "miu",
        "description": """miu — это сверхбыстрый, компактный и современный текстовый редактор для Windows.
Он запускается мгновенно и идеально подходит как для быстрых заметок, так и для анализа огромных лог-файлов или комфортного написания кода.

【Главные преимущества】
• Непревзойденная скорость: Архитектура Piece Table в сочетании с отображением файлов в память позволяет легко открывать и редактировать файлы размером в несколько гигабайт.
• Сохранение с правами администратора (UAC): Надежное сохранение защищенных файлов (например, в корне диска C:\\) через повышение привилегий без потери введенного текста.
• Современный интерфейс Windows 11: Полная поддержка светлой и темной тем, аппаратное ускорение Direct2D/DirectWrite для четкого отображения шрифтов.
• Продвинутое редактирование: Мультикурсор, блочное прямоугольное выделение (Alt+перетаскивание), поиск и замена с поддержкой регулярных выражений (Regex).
• Подсветка синтаксиса: Встроенная подсветка для C/C++, Python, Web (HTML, CSS, JS), Markdown и других языков.
• Автоматическое определение кодировок: Безошибочное распознавание UTF-8, ANSI, CP1251 и региональных кодировок.
• Полная конфиденциальность: Никакой телеметрии и сбора данных — редактор работает на 100% локально.""",
        "features": [
            "Мультикурсор и прямоугольное выделение для одновременного редактирования.",
            "Архитектура Piece Table для мгновенной работы с гигабайтными файлами.",
            "Повышение привилегий UAC для сохранения защищенных системных файлов.",
            "Светлая и темная темы с кристально четким рендерингом текста Direct2D.",
            "Надежное автоматическое определение кодировок UTF-8, CP1251, ANSI.",
            "Мощный поиск и замена с поддержкой регулярных выражений (Regex)."
        ],
        "keywords": [
            "текстовый редактор",
            "блокнот",
            "быстрый редактор",
            "notepad",
            "код",
            "легкий редактор",
            "текст"
        ],
        "releaseNotes": """Примечания к выпуску miu v1.0.38
🌐 Живой предпросмотр WebView2 (Alt+P) и точный рендеринг Markdown (CommonMark / GFM)

• Встроенный предпросмотр HTML, SVG и Markdown в реальном времени (Alt+P):
  Нажмите Alt+P для открытия панели предпросмотра с автообновлением, перезагрузкой по F5, перетаскиваемым разделителем, оверлейными полосами прокрутки, поддержкой тёмной/светлой темы и локальных ресурсов (изображений, CSS, JS).
• Точная поддержка CommonMark и GitHub Flavored Markdown (GFM):
  Таблицы GFM (выравнивание и экранирование `|`), списки задач (- [ ] / - [x]), вложенные списки, многострочные цитаты, блоки оповещений GFM ([!NOTE], [!TIP], [!WARNING] и др.), защита snake_case, автоссылки и теги `<kbd>`.
• Быстрый предпросмотр гигантских файлов и мгновенный поиск (F3):
  Мгновенный предпросмотр первых 512 КБ без копирования на диск (на 28 языках) и исправление зависания при переходе поиска F3 с конца файла в начало.""",
        "image_captions": [
            "Светлая тема оформления с удобной нумерацией строк",
            "Темная тема с яркой подсветкой синтаксиса",
            "Окно справки F1 со списком горячих клавиш"
        ],
        "prefix": "ru"
    },

    "it-it": {
        "title": "miu",
        "description": """miu è un editor di testo ultrarapido, essenziale e moderno per Windows.
Progettato per avviarsi in un istante, è ideale sia per prendere note veloci, sia per analizzare file di log di grandi dimensioni o scrivere codice con la massima fluidità.

【Punti di forza】
• Prestazioni Straordinarie: Basato su un'efficiente architettura Piece Table, apre e gestisce file giganteschi da svariati gigabyte senza rallentamenti.
• Salvataggio con Elevazione UAC: Salva file di sistema o protetti (come nella radice C:\\) con diritti di amministratore senza rischiare di perdere le modifiche correnti.
• Design Nativo Windows 11: Supporto completo alla Modalità Chiara e Scura con rendering Direct2D/DirectWrite con accelerazione hardware.
• Strumenti Avanzati di Editing: Multi-cursore simultaneo, selezione rettangolare a blocchi (Alt+trascina), cerca e sostituisci con espressioni regolari (Regex).
• Evidenziazione della Sintassi: Supporto per C/C++, Python, formati Web, Markdown e altri linguaggi.
• Rilevamento Intelligente delle Codifiche: Riconoscimento automatico e infallibile di UTF-8, ANSI e codifiche legacy.
• Privacy Totale: Completamente offline, privo di telemetria e senza processi in background superflui.""",
        "features": [
            "Multi-cursore e selezione rettangolare per modifiche simultanee su più righe.",
            "Architettura Piece Table per la gestione istantanea di file enormi.",
            "Salvataggio con elevazione UAC per file protetti o di sistema.",
            "Modalità Chiara e Scura perfettamente sincronizzate con Windows.",
            "Rilevamento automatico e accurato delle codifiche UTF-8 e ANSI.",
            "Funzione di ricerca e sostituzione avanzata con espressioni regolari."
        ],
        "keywords": [
            "editor di testo",
            "blocco note",
            "notepad",
            "editor leggero",
            "veloce",
            "codice",
            "testo"
        ],
        "releaseNotes": """Note di rilascio di miu v1.0.38
🌐 Anteprima dal vivo WebView2 (Alt+P) e rendering fedele Markdown (CommonMark / GFM)

• Anteprima divisa in tempo reale per HTML, SVG e Markdown (Alt+P):
  Premi Alt+P per aprire il pannello di anteprima con aggiornamento automatico, ricarica F5, divisore ridimensionabile, barre di scorrimento sovrapposte, tema chiaro/scuro e caricamento di risorse locali (immagini, CSS, JS).
• Rendering fedele CommonMark e GitHub Flavored Markdown (GFM):
  Supporto completo per tabelle GFM, elenchi di attività (- [ ] / - [x]), elenchi nidificati, citazioni multilinea, avvisi GFM ([!NOTE], [!TIP], [!WARNING], ecc.), protezione snake_case, link automatici e tag HTML inline (<kbd>).
• Anteprima rapida per file enormi e ricerca F3 istantanea:
  Mostra istantaneamente i primi 512 KB senza copie su disco (in 28 lingue) e risolve il blocco durante la ricerca circolare con F3 a fine file.""",
        "image_captions": [
            "Modalità chiara con formattazione e testo nitido",
            "Modalità scura elegante con evidenziazione del codice",
            "Schermata di aiuto F1 con scorciatoie da tastiera"
        ],
        "prefix": "it"
    },

    "pl-pl": {
        "title": "miu",
        "description": """miu to niesamowicie szybki, ultralekki i nowoczesny edytor tekstu dla systemu Windows.
Uruchamia się w ułamku sekundy i sprawdza się idealnie zarówno do szybkich notatek, jak i analizy wielogigabajtowych plików dziennika (logów) czy programowania.

【Kluczowe zalety】
• Niezrównana wydajność: Dzięki architekturze Piece Table możesz płynnie otwierać i edytować pliki o wielkości wielu gigabajtów przy minimalnym zużyciu pamięci RAM.
• Zapis z uprawnieniami administratora (UAC): Bezpieczne zapisywanie plików w chronionych lokalizacjach (np. na dysku C:\\) bez ryzyka utraty wpisanego tekstu w razie rezygnacji.
• Elegancki wygląd Windows 11: Pełna obsługa motywu jasnego i ciemnego z akceleracją sprzętową Direct2D/DirectWrite zapewniającą doskonałą ostrość czcionek.
• Zaawansowane narzędzia edycji: Wiele kursorów, zaznaczanie blokowe (Alt+przeciągnij), zaawansowane wyszukiwanie i zamiana z obsługą wyrażeń regularnych (Regex).
• Kolorowanie składni: Wbudowane reguły dla języków C/C++, Python, technologii Web, Markdown i innych.
• Inteligentne rozpoznawanie kodowania: Niezawodne automatyczne wykrywanie UTF-8, ANSI oraz regionalnych stron kodowych.
• Pełna prywatność: Działa w 100% offline, bez telemetrii i zbierania jakichkolwiek danych.""",
        "features": [
            "Wiele kursorów i zaznaczanie prostokątne do jednoczesnej edycji wielu linii.",
            "Architektura Piece Table zapewniająca płynną pracę z gigantycznymi plikami.",
            "Zapis z podniesieniem uprawnień UAC dla plików systemowych i chronionych.",
            "Motyw jasny i ciemny z perfekcyjnie wygładzonym tekstem Direct2D.",
            "Niezawodne automatyczne rozpoznawanie kodowania UTF-8 i ANSI.",
            "Zaawansowane wyszukiwanie z obsługą wyrażeń regularnych (Regex)."
        ],
        "keywords": [
            "edytor tekstu",
            "notatnik",
            "notepad",
            "szybki edytor",
            "lekki edytor",
            "kod",
            "tekst"
        ],
        "releaseNotes": """Informacje o wersji miu v1.0.38
🌐 Podgląd na żywo WebView2 (Alt+P) oraz wierne renderowanie Markdown (CommonMark / GFM)

• Wbudowany podgląd na żywo dla HTML, SVG i Markdown (Alt+P):
  Naciśnij Alt+P, aby otworzyć panel podglądu z automatycznym odświeżaniem, przeładowaniem F5, przesuwanym separatorem, paskami przewijania, trybem ciemnym/jasnym i obsługą lokalnych zasobów (obrazy, CSS, JS).
• Wierne renderowanie CommonMark i GitHub Flavored Markdown (GFM):
  Pełna obsługa tabel GFM, list zadań (- [ ] / - [x]), list zagnieżdżonych, cytatów wielowierszowych, alertów GFM ([!NOTE], [!TIP], [!WARNING] itp.), ochrona snake_case, autolinki oraz znaczniki HTML (<kbd>).
• Szybki podgląd ogromnych plików i płynne wyszukiwanie (F3):
  Błyskawiczny podgląd pierwszych 512 KB bez kopiowania na dysk (w 28 językach) oraz poprawka zawieszania się przy zawijaniu wyszukiwania F3 na końcu pliku.""",
        "image_captions": [
            "Motyw jasny z przejrzystym widokiem dokumentu",
            "Motyw ciemny z kolorowaniem składni kodu",
            "Okno pomocy F1 z listą skrótów klawiszowych"
        ],
        "prefix": "pl"
    },

    "nl-nl": {
        "title": "miu",
        "description": """miu is een ultrasnelle, gestroomlijnde en moderne teksteditor voor Windows.
Ontworpen om direct binnen een fractie van een seconde op te starten; perfect voor snelle aantekeningen, het inspecteren van enorme logbestanden of dagelijks programmeerwerk.

【Belangrijkste voordelen】
• Ongeëvenaarde snelheid: Dankzij de geavanceerde Piece Table-architectuur bewerkt u enorme bestanden van meerdere gigabytes met minimale belasting van het systeem.
• Opslaan met beheerdersrechten (UAC): Bewerk en bewaar beveiligde bestanden (zoals op de C:\\ schijf) veilig met UAC-rechten zonder tekst te verliezen bij annulering.
• Stijlvol Windows 11-ontwerp: Naadloze ondersteuning voor de lichte en donkere modus met Direct2D/DirectWrite hardwareversnelling voor haarscherpe letters.
• Geavanceerde bewerkingsfuncties: Meerdere cursors, rechthoekige blokselectie (Alt+slepen), zoeken en vervangen met reguliere expressies (Regex).
• Syntaxisaccentuering: Ingebouwde kleuraccentuering voor C/C++, Python, Webformaten, Markdown en meer.
• Betrouwbare codering: Automatische en nauwkeurige herkenning van UTF-8, ANSI en traditionele tekensets.
• Volledige privacy: Werkt 100% offline, bevat geen telemetrie en verbruikt geen onnodige achtergrondprocessen.""",
        "features": [
            "Meerdere cursors en rechthoekige blokselectie voor gelijktijdige bewerking.",
            "Piece Table-architectuur voor soepele verwerking van gigantische bestanden.",
            "Veilig opslaan met UAC-beheerdersrechten voor beveiligde systeembestanden.",
            "Lichte en donkere modus met haarscherpe Direct2D-tekstweergave.",
            "Automatische detectie van bestandscoderingen zoals UTF-8 en ANSI.",
            "Krachtige zoek- en vervangfunctie met volledige Regex-ondersteuning."
        ],
        "keywords": [
            "teksteditor",
            "kladblok",
            "notepad",
            "snelle editor",
            "lichtgewicht",
            "code editor",
            "tekst"
        ],
        "releaseNotes": """Releaseopmerkingen miu v1.0.38
🌐 WebView2 live-voorbeeld (Alt+P) & getrouwe CommonMark / GFM Markdown-weergave

• Ingebouwd gesplitst live-voorbeeld voor HTML, SVG & Markdown (Alt+P):
  Druk op Alt+P voor een live-voorbeeldvenster met automatische updates, F5-herladen, versleepbare splitslijn, overlay-schuifbalken, donker/licht-thema en lokale bestanden (afbeeldingen, CSS, JS).
• Getrouwe CommonMark & GitHub Flavored Markdown (GFM) weergave:
  Volledige ondersteuning voor GFM-tabellen, takenlijsten (- [ ] / - [x]), geneste lijsten, citaten, GFM-meldingen ([!NOTE], [!TIP], [!WARNING], enz.), snake_case-bescherming, autolinks en inline HTML (<kbd>).
• Snelle weergave van enorme bestanden & directe F3-zoekopdracht:
  Toont direct de eerste 512 KB zonder schijfkopieën (in 28 talen) en verhelpt het vastlopen bij het rondzoeken met F3 aan het einde van grote bestanden.""",
        "image_captions": [
            "Lichte modus met overzichtelijke documentweergave",
            "Donkere modus met kleurrijke syntaxisaccentuering",
            "F1 Helpscherm met overzicht van sneltoetsen"
        ],
        "prefix": "nl"
    },

    "tr-tr": {
        "title": "miu",
        "description": """miu, Windows için geliştirilmiş ultra hızlı, sade ve yüksek performanslı bir metin düzenleyicidir.
Göz açıp kapayıncaya kadar açılan yapısıyla hem hızlı notlar almak, hem devasa günlük (log) dosyalarını incelemek hem de kod yazmak için idealdir.

【Öne Çıkan Özellikler】
• Üstün Performans: Modern Piece Table mimarisi ve bellek eşleme teknolojisi sayesinde gigabaytlarca büyüklükteki dosyaları donmadan akıcı biçimde düzenleyin.
• Yönetici İzniyle Kaydetme (UAC): C:\\ kök dizini gibi korumalı alanlardaki sistem dosyalarını yönetici izniyle güvenle kaydedin; iptal durumunda yazdıklarınız asla kaybolmaz.
• Windows 11 ile Kusursuz Uyum: Açık ve Koyu tema desteği, Direct2D/DirectWrite donanım hızlandırmalı net ve pürüzsüz yazı tipi oluşturma.
• Gelişmiş Düzenleme Araçları: Çoklu imleç, dikdörtgen blok seçimi (Alt+sürükle), düzenli ifadeler (Regex) ile gelişmiş arama ve değiştirme.
• Sözdizimi Vurgulama: C/C++, Python, Web dilleri, Markdown ve daha fazlası için dahili renkli sözdizimi desteği.
• Akıllı Kodlama Tespiti: UTF-8, ANSI ve yerel karakter kodlamalarını otomatik ve hatasız tanıma.
• Tam Gizlilik ve Güvenlik: Sıfır telemetri, %100 çevrimdışı çalışma ve sıfır arka plan yükü.""",
        "features": [
            "Çoklu imleç ve dikdörtgen blok seçimi ile aynı anda birden çok satırı düzenleme.",
            "Piece Table mimarisi ile devasa dosyaları kasmadan açma ve düzenleme.",
            "Korumalı sistem dosyaları için güvenli UAC yönetici izniyle kaydetme.",
            "Koyu ve Açık tema desteği ile yüksek çözünürlüklü Direct2D metin işleme.",
            "UTF-8 ve ANSI kodlamalarını hatasız otomatik tespit etme.",
            "Düzenli ifadeler (Regex) destekli güçlü arama ve değiştirme motoru."
        ],
        "keywords": [
            "metin düzenleyici",
            "not defteri",
            "notepad",
            "hızlı düzenleyici",
            "hafif düzenleyici",
            "kod",
            "metin"
        ],
        "releaseNotes": """miu v1.0.38 Sürüm Notları
🌐 WebView2 Canlı Önizleme (Alt+P) ve Tam Uyumlu CommonMark / GFM Markdown Görünümü

• HTML, SVG ve Markdown için Bölünmüş Canlı Önizleme (Alt+P):
  Otomatik yenileme, F5 ile yeniden yükleme, sürüklenebilir ayırıcı, kaplama kaydırma çubukları, Koyu/Açık tema uyumu ve yerel dosya (resim, CSS, JS) desteğiyle canlı önizleme panelini açmak için Alt+P'ye basın.
• Tam Uyumlu CommonMark ve GitHub Flavored Markdown (GFM):
  GFM tabloları, görev listeleri (- [ ] / - [x]), iç içe listeler, çok satırlı alıntılar, GFM uyarı kutuları ([!NOTE], [!TIP], [!WARNING] vb.), snake_case koruması, otomatik bağlantılar ve satır içi HTML (<kbd>) desteği.
• Dev Dosyalarda Hızlı Önizleme ve Anında F3 Arama Döngüsü:
  Disk kopyası olmadan ilk 512 KB'ı anında önizler (28 dilde bildirim ile) ve büyük dosyaların sonunda F3 ile başa dönerken oluşan donmayı giderir.""",
        "image_captions": [
            "Açık temada temiz ve akıcı metin düzenleme",
            "Koyu temada renkli kod sözdizimi vurgulama",
            "F1 Yardım ekranı ve kısayol tuşları listesi"
        ],
        "prefix": "tr"
    },
    "vi-vn": {
        "title": "miu",
        "description": """miu là trình soạn thảo văn bản siêu nhanh, nhẹ và hiện đại được thiết kế riêng cho Windows.
Từ việc ghi chú tức thì, duyệt qua các tệp nhật ký (log) khổng lồ cho đến viết mã nguồn hàng ngày, miu mang lại hiệu suất tối đa cùng tốc độ mở tệp trong chớp mắt.

【Tính năng nổi bật】
• Hiệu suất vượt trội: Dựa trên kiến trúc Piece Table hiện đại và kỹ thuật ánh xạ bộ nhớ, mở và chỉnh sửa mượt mà các tệp văn bản lớn hàng gigabyte mà không bị giật lag.
• Lưu với quyền quản trị viên (UAC): Chỉnh sửa và lưu an toàn các tệp nằm trong thư mục gốc C:\\ hoặc thư mục hệ thống mà không sợ mất nội dung khi hủy.
• Giao diện Windows 11 hiện đại: Hỗ trợ hoàn hảo chế độ Sáng (Light) và Tối (Dark), tăng tốc phần cứng Direct2D/DirectWrite cho chữ hiển thị sắc nét.
• Công cụ chỉnh sửa mạnh mẽ: Hỗ trợ nhiều con trỏ (multi-cursor), chọn khối chữ nhật (Alt+kéo), tìm kiếm và thay thế bằng biểu thức chính quy (Regex).
• Tô màu cú pháp thông minh: Tích hợp sẵn quy tắc tô màu cho C/C++, Python, Web (HTML, CSS, JS), Markdown và nhiều ngôn ngữ khác.
• Nhận diện bảng mã tự động: Tự động phát hiện chính xác UTF-8, ANSI và các bảng mã ký tự quốc tế, ngăn chặn hoàn toàn lỗi vỡ phông chữ.
• Bảo mật và riêng tư tuyệt đối: Hoạt động hoàn toàn ngoại tuyến (offline), không thu thập dữ liệu hay chạy tiến trình ngầm.""",
        "features": [
            "Đa con trỏ và chọn khối chữ nhật để chỉnh sửa đồng thời nhiều dòng.",
            "Kiến trúc Piece Table mượt mà khi xử lý tệp văn bản lớn nhiều gigabyte.",
            "Lưu với quyền quản trị UAC an toàn cho các tệp hệ thống được bảo vệ.",
            "Chế độ Sáng và Tối hiển thị sắc nét nhờ tăng tốc Direct2D.",
            "Tự động nhận diện chính xác các bảng mã ký tự UTF-8 và ANSI.",
            "Tìm kiếm và thay thế mạnh mẽ với hỗ trợ đầy đủ biểu thức chính quy (Regex)."
        ],
        "keywords": [
            "trình soạn thảo văn bản",
            "notepad",
            "sổ tay",
            "soạn thảo mã",
            "nhẹ",
            "nhanh",
            "văn bản"
        ],
        "releaseNotes": """Ghi chú phát hành miu v1.0.38
🌐 Xem trước trực tiếp WebView2 (Alt+P) & Hiển thị chuẩn CommonMark / GFM Markdown

• Xem trước trực tiếp chia đôi màn hình cho HTML, SVG & Markdown (Alt+P):
  Nhấn Alt+P để mở khung xem trước trực tiếp với tự động cập nhật, tải lại bằng F5, thanh chia kéo thả tinh tế, thanh cuộn nổi, đồng bộ giao diện Sáng/Tối và ánh xạ tài nguyên cục bộ (ảnh, CSS, JS).
• Hiển thị chuẩn CommonMark & GitHub Flavored Markdown (GFM):
  Hỗ trợ đầy đủ bảng GFM, danh sách công việc (- [ ] / - [x]), danh sách lồng nhau, trích dẫn nhiều dòng, hộp cảnh báo GFM ([!NOTE], [!TIP], [!WARNING], v.v.), bảo vệ snake_case, liên kết tự động và thẻ HTML nội tuyến (<kbd>).
• Tối ưu xem trước tệp khổng lồ & Sửa lỗi tìm kiếm vòng lặp F3:
  Xem trước tức thì 512 KB đầu tiên không cần sao chép đĩa (hỗ trợ 28 ngôn ngữ) và khắc phục triệt để lỗi treo khi nhấn F3 quay lại đầu tệp lớn.""",
        "image_captions": [
            "Chế độ Sáng với định dạng và căn lề sắc nét",
            "Chế độ Tối với tô màu cú pháp mã nguồn",
            "Màn hình trợ giúp F1 và danh sách phím tắt"
        ],
        "prefix": "vi"
    },

    "uk-ua": {
        "title": "miu",
        "description": """miu — це надзвичайно швидкий, компактний і сучасний текстовий редактор для Windows.
Він запускається миттєво й ідеально підходить як для швидких нотаток, так і для аналізу гігабайтних лог-файлів або комфортного написання коду.

【Головні переваги】
• Неперевершена швидкодія: Завдяки архітектурі Piece Table та відображенню файлів у пам'ять, miu легко відкриває та редагує файли розміром у кілька гігабайтів без підвисань.
• Збереження з правами адміністратора (UAC): Надійне збереження захищених файлів (наприклад, у корені диска C:\\) через підвищення привілеїв без втрати введеного тексту при скасуванні.
• Сучасний дизайн Windows 11: Повна підтримка світлої та темної тем, апаратне прискорення Direct2D/DirectWrite для кришталево чіткого відображення шрифтів.
• Розширене редагування: Мультикурсор, прямокутне блокове виділення (Alt+перетягування), пошук і заміна з підтримкою регулярних виразів (Regex).
• Підсвічування синтаксису: Вбудоване підсвічування для C/C++, Python, Web (HTML, CSS, JS), Markdown та інших форматів.
• Автоматичне визначення кодувань: Безпомилкове розпізнавання UTF-8, ANSI, CP1251 та регіональних кодувань.
• Повна конфіденційність: Жодної телеметрії чи збору даних — редактор працює на 100% локально.""",
        "features": [
            "Мультикурсор та прямокутне виділення для одночасного редагування.",
            "Архітектура Piece Table для миттєвої роботи з гігабайтними файлами.",
            "Підвищення привілеїв UAC для збереження захищених системних файлів.",
            "Світла та темна теми з чітким рендерингом тексту через Direct2D.",
            "Надійне автоматичне розпізнавання кодувань UTF-8, CP1251, ANSI.",
            "Потужний пошук та заміна з підтримкою регулярних виразів (Regex)."
        ],
        "keywords": [
            "текстовий редактор",
            "блокнот",
            "notepad",
            "швидкий редактор",
            "легкий редактор",
            "код",
            "текст"
        ],
        "releaseNotes": """Примітки до випуску miu v1.0.38
🌐 Живий попередній перегляд WebView2 (Alt+P) та точний рендеринг Markdown (CommonMark / GFM)

• Вбудований попередній перегляд HTML, SVG і Markdown у реальному часі (Alt+P):
  Натисніть Alt+P для відкриття панелі перегляду з автооновленням, перезавантаженням F5, розділювачем, оверлейними смугами прокручування, темною/світлою темою та підтримкою локальних ресурсів (зображень, CSS, JS).
• Точна підтримка CommonMark і GitHub Flavored Markdown (GFM):
  Таблиці GFM, списки завдань (- [ ] / - [x]), вкладені списки, багаторядкові цитати, блоки сповіщень GFM ([!NOTE], [!TIP], [!WARNING] тощо), захист snake_case, автопосилання та теги `<kbd>`.
• Швидкий перегляд гігантських файлів і миттєвий пошук (F3):
  Миттєвий перегляд перших 512 КБ без копіювання на диск (28 мовами) та виправлення зависання під час циклічного пошуку F3 наприкінці файлу.""",
        "image_captions": [
            "Світла тема з чистим відображенням тексту",
            "Темна тема з підсвічуванням синтаксису коду",
            "Вікно довідки F1 зі списком гарячих клавіш"
        ],
        "prefix": "uk"
    },

    "id-id": {
        "title": "miu",
        "description": """miu adalah editor teks ultra-cepat, ringan, dan modern yang dirancang khusus untuk Windows.
Mulai dari mencatat cepat, memeriksa berkas log raksasa berukuran gigabita, hingga menulis kode sehari-hari, miu menawarkan kinerja maksimal dengan waktu pembukaan sekejap mata.

【Keunggulan Utama】
• Performa Luar Biasa: Berkat arsitektur Piece Table yang canggih, buka dan edit berkas teks berukuran beberapa gigabita tanpa jeda atau hambatan.
• Simpan dengan Hak Administrator (UAC): Edit dan simpan berkas sistem yang dilindungi (seperti di root C:\\) dengan aman melalui elevasi hak akses tanpa risiko kehilangan data jika dibatalkan.
• Tampilan Modern Windows 11: Dukungan penuh untuk Mode Terang dan Gelap dengan akselerasi perangkat keras Direct2D/DirectWrite untuk teks yang sangat tajam dan jernih.
• Fitur Pengeditan Canggih: Multi-kursor, seleksi blok kolom (Alt+tarik), pencarian dan penggantian dengan ekspresi reguler (Regex).
• Pewarnaan Sintaksis: Pewarnaan kode bawaan untuk C/C++, Python, Web (HTML, CSS, JS), Markdown, dan format lainnya.
• Deteksi Enkod Cerdas: Mengenali secara otomatis dan akurat pengkodean UTF-8, ANSI, dan halaman kode karakter regional.
• Privasi Mutlak: 100% offline, tanpa telemetri, dan tanpa proses latar belakang yang tidak perlu.""",
        "features": [
            "Multi-kursor dan seleksi blok kolom untuk pengeditan multi-baris sekaligus.",
            "Arsitektur Piece Table yang sangat lancar untuk berkas raksasa berukuran gigabita.",
            "Elevasi hak akses UAC yang aman untuk menyimpan berkas sistem yang dilindungi.",
            "Mode Terang dan Gelap yang tersinkronisasi rapi dengan tema Windows 11.",
            "Deteksi otomatis akurat untuk enkod berkas UTF-8 dan ANSI.",
            "Fitur cari dan ganti yang andal dengan dukungan penuh ekspresi reguler (Regex)."
        ],
        "keywords": [
            "editor teks",
            "catatan",
            "notepad",
            "editor cepat",
            "editor ringan",
            "kode",
            "teks"
        ],
        "releaseNotes": """Catatan Rilis miu v1.0.38
🌐 Pratinjau Langsung WebView2 (Alt+P) & Rendering Markdown CommonMark / GFM Akurat

• Pratinjau Langsung Layar Terpisah untuk HTML, SVG & Markdown (Alt+P):
  Tekan Alt+P untuk membuka panel pratinjau langsung dengan pembaruan otomatis, muat ulang F5, pembagi yang dapat digeser, bilah gulir overlay, tema Gelap/Terang, dan pemetaan aset lokal (gambar, CSS, JS).
• Rendering CommonMark & GitHub Flavored Markdown (GFM) yang Akurat:
  Dukungan penuh untuk tabel GFM, daftar tugas (- [ ] / - [x]), daftar bersarang, kutipan multi-baris, kotak peringatan GFM ([!NOTE], [!TIP], [!WARNING], dll.), perlindungan snake_case, tautan otomatis, dan tag HTML sebaris (<kbd>).
• Optimasi Pratinjau File Raksasa & Pencarian F3 Instan:
  Menampilkan pratinjau 512 KB pertama secara instan tanpa salinan disk (dalam 28 bahasa) dan memperbaiki masalah macet saat menekan F3 di akhir file besar.""",
        "image_captions": [
            "Mode Terang dengan tampilan dokumen yang rapi dan bersih",
            "Mode Gelap dengan pewarnaan sintaksis kode yang jelas",
            "Layar Bantuan F1 dengan daftar pintasan keyboard lengkap"
        ],
        "prefix": "id"
    },

    "cs-cz": {
        "title": "miu",
        "description": """miu je bleskově rychlý, ultralehký a moderní textový editor pro systém Windows.
Spouští se během zlomku sekundy a je ideální pro rychlé poznámky, analýzu obřích logovacích souborů o velikosti několika gigabajtů i každodenní psaní kódu.

【Hlavní přednosti】
• Špičkový výkon: Díky moderní architektuře Piece Table a mapování paměti plynule otevírá a upravuje vícgigabajtové soubory s minimální spotřebou RAM.
• Ukládání s oprávněním správce (UAC): Bezpečné ukládání chráněných systémových souborů (např. na disku C:\\) bez ztráty rozepsaného textu při zrušení.
• Moderní vzhled Windows 11: Plná podpora světlého a tmavého režimu s hardwarovou akcelerací Direct2D/DirectWrite pro dokonale ostré písmo.
• Pokročilé nástroje úprav: Více kurzorů (multi-cursor), sloupcový výběr (Alt+tažení), pokročilé hledání a nahrazování s regulárními výrazy (Regex).
• Zvýrazňování syntaxe: Vestavěná podpora pro C/C++, Python, webové jazyky (HTML, CSS, JS), Markdown a další.
• Inteligentní detekce kódování: Spolehlivé automatické rozpoznání UTF-8, UTF-16, ANSI a středoevropských znakových sad.
• Naprosté soukromí: Pracuje 100% offline, bez telemetrie a bez zbytečných procesů na pozadí.""",
        "features": [
            "Více kurzorů a sloupcový výběr pro současnou úpravu více řádků.",
            "Architektura Piece Table pro plynulou práci s obřími soubory.",
            "Bezpečné ukládání chráněných systémových souborů s oprávněním správce (UAC).",
            "Světlý a tmavý režim s ostrým vykreslováním textu přes Direct2D.",
            "Spolehlivá automatická detekce kódování UTF-8, UTF-16 a ANSI.",
            "Pokročilé hledání a nahrazování s plnou podporou regulárních výrazů (Regex)."
        ],
        "keywords": [
            "textový editor",
            "poznámkový blok",
            "notepad",
            "rychlý editor",
            "lehký editor",
            "kód",
            "text"
        ],
        "releaseNotes": """Poznámky k verzi miu v1.0.38
🌐 Živý náhled WebView2 (Alt+P) a věrné vykreslování Markdownu (CommonMark / GFM)

• Vestavěný živý náhled pro HTML, SVG a Markdown (Alt+P):
  Stisknutím Alt+P otevřete panel živého náhledu s automatickou aktualizací, obnovením klávesou F5, posuvným oddělovačem, překryvnými posuvníky, tmavým/světlým motivem a podporou lokálních souborů (obrázky, CSS, JS).
• Věrné vykreslování CommonMark a GitHub Flavored Markdown (GFM):
  Plná podpora tabulek GFM, seznamů úkolů (- [ ] / - [x]), vnořených seznamů, víceřádkových citací, upozornění GFM ([!NOTE], [!TIP], [!WARNING] atd.), ochrany snake_case, automatických odkazů a inline HTML (<kbd>).
• Rychlý náhled obřích souborů a okamžité hledání (F3):
  Okamžitý náhled prvních 512 KB bez kopírování na disk (ve 28 jazycích) a oprava zamrzání při přechodu hledání F3 z konce souboru na začátek.""",
        "image_captions": [
            "Světlý režim s čistým a přehledným zobrazením dokumentu",
            "Tmavý režim s barevným zvýrazňováním syntaxe kódu",
            "Obrazovka nápovědy F1 s přehledem klávesových zkratek"
        ],
        "prefix": "cs"
    },

    "sv-se": {
        "title": "miu",
        "description": """miu är en blixtsnabb, ultralätt och modern textredigerare för Windows.
Den startar på ett ögonblick och är perfekt för snabba anteckningar, granskning av enorma loggfiler på flera gigabyte och vardaglig kodning.

【Viktiga fördelar】
• Oslagbar prestanda: Med Piece Table-arkitektur och minnesmappning öppnar och redigerar du gigantiska filer utan fördröjning och med minimal RAM-förbrukning.
• Spara som administratör (UAC): Spara skyddade systemfiler (t.ex. direkt på C:\\) säkert via UAC-behörighet utan att förlora osparad text om du avbryter.
• Elegant Windows 11-design: Fullt stöd för ljust och mörkt läge med Direct2D/DirectWrite-hårdvaruacceleration för knivskarp text.
• Kraftfulla redigeringsverktyg: Multimarkör, kolumnmarkering (Alt+dra), samt sök och ersätt med reguljära uttryck (Regex).
• Syntaxmarkering: Inbyggd färgkodning för C/C++, Python, webbformat (HTML, CSS, JS), Markdown med mera.
• Smart kodningsdetektering: Automatisk och exakt identifiering av UTF-8, UTF-16, ANSI och internationella teckentabeller.
• Fullständig integritet: 100 % offline, ingen telemetri och inga onödiga bakgrundsprocesser.""",
        "features": [
            "Multimarkör och kolumnmarkering för samtidig redigering av flera rader.",
            "Piece Table-arkitektur för smidig hantering av filer på flera gigabyte.",
            "Säker sparning med UAC-administratörsbehörighet för skyddade systemfiler.",
            "Ljust och mörkt läge med knivskarp Direct2D-textrendering.",
            "Automatisk identifiering av UTF-8, UTF-16 och ANSI-kodning.",
            "Kraftfull sök- och ersättningsfunktion med fullt stöd för Regex."
        ],
        "keywords": [
            "textredigerare",
            "anteckningar",
            "notepad",
            "snabb redigerare",
            "lättviktig",
            "kodredigerare",
            "text"
        ],
        "releaseNotes": """Versionsinformation för miu v1.0.38
🌐 WebView2 direktförhandsgranskning (Alt+P) & exakt CommonMark / GFM Markdown-rendering

• Inbyggd delad förhandsgranskning för HTML, SVG & Markdown (Alt+P):
  Tryck på Alt+P för att öppna en panel med automatisk uppdatering, F5-omladdning, justerbar delningslinje, överliggande rullningslister, mörkt/ljust tema och lokala resurser (bilder, CSS, JS).
• Exakt CommonMark & GitHub Flavored Markdown (GFM):
  Fullt stöd för GFM-tabeller, uppgiftslistor (- [ ] / - [x]), kapslade listor, flerradiga citat, GFM-notiser ([!NOTE], [!TIP], [!WARNING] m.fl.), snake_case-skydd, autolänkar och inline-HTML (<kbd>).
• Snabb förhandsgranskning av jättefiler & omedelbar F3-sökning:
  Förhandsgranskar de första 512 KB direkt utan diskkopior (på 28 språk) och åtgärdar frysning vid F3-sökning i slutet av stora filer.""",
        "image_captions": [
            "Ljust läge med ren och tydlig dokumentvy",
            "Mörkt läge med tydlig syntaxmarkering för kod",
            "F1-hjälpskärm med lista över kortkommandon"
        ],
        "prefix": "sv"
    },

    "th-th": {
        "title": "miu",
        "description": """miu คือโปรแกรมแก้ไขข้อความ (Text Editor) ความเร็วสูง ขนาดเบาพิเศษ และทันสมัยสำหรับ Windows
เปิดใช้งานได้ทันทีภายในเสี้ยววินาที เหมาะอย่างยิ่งสำหรับการจดบันทึกด่วน การเปิดไฟล์บันทึกข้อมูล (Log) ขนาดใหญ่ระดับกิกะไบต์ และการเขียนโค้ดในชีวิตประจำวัน

【จุดเด่นสำคัญ】
• ประสิทธิภาพสูงสุด: ด้วยสถาปัตยกรรม Piece Table และการจัดการหน่วยความจำขั้นสูง ช่วยให้เปิดและแก้ไขไฟล์ขนาดหลายกิกะไบต์ได้อย่างลื่นไหลโดยใช้ RAM น้อยที่สุด
• บันทึกด้วยสิทธิ์ผู้ดูแลระบบ (UAC): บันทึกไฟล์ระบบที่ได้รับการป้องกัน (เช่น ในไดรฟ์ C:\\) ได้อย่างปลอดภัยโดยไม่สูญเสียข้อความที่พิมพ์ไว้หากกดยกเลิก
• ดีไซน์ทันสมัยสำหรับ Windows 11: รองรับโหมดสว่างและโหมดมืดอย่างสมบูรณ์ พร้อมการเร่งความเร็วฮาร์ดแวร์ Direct2D/DirectWrite เพื่อตัวอักษรภาษาไทยที่คมชัดสวยงาม
• เครื่องมือแก้ไขขั้นสูง: รองรับหลายเคอร์เซอร์ (Multi-cursor), การเลือกแบบคอลัมน์ (Alt+ลาก), และการค้นหา/แทนที่ด้วยนิพจน์ทั่วไป (Regex)
• ไฮไลต์ไวยากรณ์โค้ด: รองรับการแสดงสีโค้ดสำหรับ C/C++, Python, Web (HTML, CSS, JS), Markdown และอื่นๆ
• ตรวจสอบรหัสอักขระอัตโนมัติ: ตรวจจับ UTF-8, UTF-16 และรหัสภาษาต่างๆ ได้อย่างแม่นยำ
• ความเป็นส่วนตัว 100%: ทำงานแบบออฟไลน์ทั้งหมด ไม่มีการเก็บข้อมูลผู้ใช้หรือรันโปรเซสเบื้องหลัง""",
        "features": [
            "รองรับหลายเคอร์เซอร์และการเลือกแบบคอลัมน์เพื่อแก้ไขหลายบรรทัดพร้อมกัน",
            "สถาปัตยกรรม Piece Table ที่รวดเร็วสำหรับการจัดการไฟล์ขนาดใหญ่ระดับกิกะไบต์",
            "บันทึกไฟล์ระบบที่ได้รับการป้องกันอย่างปลอดภัยด้วยสิทธิ์ผู้ดูแลระบบ (UAC)",
            "โหมดสว่างและโหมดมืดพร้อมการแสดงผลข้อความที่คมชัดด้วย Direct2D",
            "ตรวจจับรหัสอักขระ UTF-8, UTF-16 และ ANSI โดยอัตโนมัติ",
            "ค้นหาและแทนที่อย่างทรงพลังพร้อมรองรับนิพจน์ทั่วไป (Regex) เต็มรูปแบบ"
        ],
        "keywords": [
            "โปรแกรมแก้ไขข้อความ",
            "สมุดบันทึก",
            "notepad",
            "แก้ไขโค้ด",
            "โปรแกรมเบา",
            "ความเร็วสูง",
            "ข้อความ"
        ],
        "releaseNotes": """บันทึกประจำรุ่น miu v1.0.38
🌐 ดูตัวอย่างแบบสดด้วย WebView2 (Alt+P) และการแสดงผล Markdown (CommonMark / GFM) อย่างสมบูรณ์

• ดูตัวอย่างแบบแบ่งหน้าจอเรียลไทม์สำหรับ HTML, SVG และ Markdown (Alt+P):
  กด Alt+P เพื่อเปิดหน้าต่างดูตัวอย่างพร้อมอัปเดตอัตโนมัติ รีโหลดด้วย F5 เส้นแบ่งปรับขนาดได้ แถบเลื่อนแบบโอเวอร์เลย์ รองรับโหมดมืด/สว่าง และการโหลดไฟล์ในเครื่อง (รูปภาพ, CSS, JS)
• รองรับ CommonMark และ GitHub Flavored Markdown (GFM) อย่างครบถ้วน:
  รองรับตาราง GFM รายการงาน (- [ ] / - [x]) รายการซ้อนหลายระดับ การอ้างอิงหลายบรรทัด กล่องแจ้งเตือน GFM ([!NOTE], [!TIP], [!WARNING] ฯลฯ) การป้องกัน snake_case ลิงก์อัตโนมัติ และแท็ก HTML (<kbd>)
• ดูตัวอย่างไฟล์ขนาดใหญ่พิเศษอย่างรวดเร็วและแก้ไขการค้นหา F3:
  แสดงตัวอย่าง 512 KB แรกทันทีโดยไม่คัดลอกไฟล์ลงดิสก์ (รองรับ 28 ภาษา) และแก้ไขอาการค้างเมื่อกด F3 ค้นหาวนกลับจากท้ายไฟล์""",
        "image_captions": [
            "โหมดสว่างพร้อมการแสดงผลเอกสารที่สะอาดตาและคมชัด",
            "โหมดมืดพร้อมการไฮไลต์สีไวยากรณ์โค้ดอย่างชัดเจน",
            "หน้าจอวิธีใช้ F1 พร้อมรายการคีย์ลัดทั้งหมด"
        ],
        "prefix": "th"
    },

    "hi-in": {
        "title": "miu",
        "description": """miu Windows के लिए एक अत्यंत तेज़, अल्ट्रा-लाइटवेट और आधुनिक टेक्स्ट एडिटर है।
यह पलक झपकते ही खुलता है और त्वरित नोट्स लेने, गीगाबाइट आकार की विशाल लॉग फ़ाइलों का विश्लेषण करने तथा दैनिक कोडिंग के लिए एकदम उपयुक्त है।

【मुख्य विशेषताएँ】
• बेजोड़ प्रदर्शन: आधुनिक Piece Table आर्किटेक्चर और मेमोरी मैपिंग की मदद से न्यूनतम RAM उपयोग के साथ कई गीगाबाइट की फ़ाइलों को बिना किसी रुकावट के खोलें और संपादित करें।
• व्यवस्थापक अधिकार (UAC) के साथ सहेजें: C:\\ ड्राइव जैसे सुरक्षित सिस्टम फ़ोल्डरों में फ़ाइलों को सुरक्षित रूप से सहेजें; रद्द करने पर भी आपका लिखा हुआ टेक्स्ट कभी नष्ट नहीं होता।
• आधुनिक Windows 11 डिज़ाइन: लाइट और डार्क मोड का पूर्ण समर्थन, तथा स्पष्ट देवनागरी और लैटिन फ़ॉन्ट रेंडरिंग के लिए Direct2D/DirectWrite हार्डवेयर त्वरण।
• उन्नत संपादन उपकरण: मल्टी-कर्सर, कॉलम ब्लॉक चयन (Alt+Drag), और रेगुलर एक्सप्रेशन (Regex) के साथ खोजें और बदलें।
• सिंटैक्स हाइलाइटिंग: C/C++, Python, Web (HTML, CSS, JS), Markdown और अन्य भाषाओं के लिए अंतर्निहित रंगीन हाइलाइटिंग।
• स्मार्ट एन्कोडिंग पहचान: UTF-8, UTF-16 और ANSI एन्कोडिंग की सटीक स्वचालित पहचान।
• पूर्ण गोपनीयता: 100% ऑफ़लाइन कार्य करता है, कोई टेलीमेट्री या अनावश्यक बैकग्राउंड प्रोसेस नहीं।""",
        "features": [
            "एक साथ कई पंक्तियों को संपादित करने के लिए मल्टी-कर्सर और कॉलम चयन।",
            "गीगाबाइट आकार की विशाल फ़ाइलों के सुगम संपादन के लिए Piece Table आर्किटेक्चर।",
            "सुरक्षित सिस्टम फ़ाइलों को सहेजने के लिए UAC व्यवस्थापक अधिकार समर्थन।",
            "Direct2D हार्डवेयर त्वरण के साथ आकर्षक लाइट और डार्क मोड।",
            "UTF-8, UTF-16 और ANSI वर्ण एन्कोडिंग की स्वचालित पहचान।",
            "रेगुलर एक्सप्रेशन (Regex) समर्थन के साथ शक्तिशाली खोजें और बदलें सुविधा।"
        ],
        "keywords": [
            "टेक्स्ट एडिटर",
            "नोटपैड",
            "notepad",
            "फास्ट एडिटर",
            "लाइटवेट",
            "कोड एडिटर",
            "टेक्स्ट"
        ],
        "releaseNotes": """miu v1.0.38 रिलीज़ नोट्स
🌐 WebView2 लाइव पूर्वावलोकन (Alt+P) और सटीक CommonMark / GFM Markdown रेंडरिंग

• HTML, SVG और Markdown के लिए इन-बिल्ट स्प्लिट लाइव पूर्वावलोकन (Alt+P):
  ऑटो-अपडेट, F5 रीलोड, ड्रैग करने योग्य स्प्लिटर, ओवरले स्क्रॉलबार, डार्क/लाइट थीम और लोकल फ़ाइल सपोर्ट (इमेज, CSS, JS) के साथ लाइव प्रीव्यू पैनल खोलने के लिए Alt+P दबाएँ।
• सटीक CommonMark और GitHub Flavored Markdown (GFM) रेंडरिंग:
  GFM टेबल, टास्क लिस्ट (- [ ] / - [x]), नेस्टेड लिस्ट, बहु-पंक्ति उद्धरण, GFM अलर्ट ([!NOTE], [!TIP], [!WARNING] आदि), snake_case सुरक्षा, ऑटोलिंक और इनलाइन HTML (<kbd>) का पूर्ण समर्थन।
• विशाल फ़ाइलों का तेज़ पूर्वावलोकन और त्वरित F3 खोज सुधार:
  बिना डिस्क कॉपी के पहले 512 KB का तुरंत पूर्वावलोकन दिखाता है (28 भाषाओं में सूचना सहित) और बड़ी फ़ाइलों के अंत में F3 दबाने पर होने वाली फ़्रीज़ समस्या को ठीक करता है।""",
        "image_captions": [
            "साफ़ और स्पष्ट दस्तावेज़ दृश्य के साथ लाइट मोड",
            "कोड सिंटैक्स हाइलाइटिंग के साथ आकर्षक डार्क मोड",
            "कीबोर्ड शॉर्टकट सूची के साथ F1 सहायता स्क्रीन"
        ],
        "prefix": "hi"
    },

    "ro-ro": {
        "title": "miu",
        "description": """miu este un editor de text ultra-rapid, extrem de ușor și modern, conceput special pentru Windows.
Pornește instantaneu și este ideal pentru notițe rapide, analizarea fișierelor jurnal (log) de ordinul gigaocteților și scrierea zilnică de cod.

【Avantaje principale】
• Performanță excepțională: Datorită arhitecturii Piece Table și mapării în memorie, deschideți și editați fluent fișiere uriașe cu un consum minim de memorie RAM.
• Salvare cu drepturi de administrator (UAC): Salvați în siguranță fișierele protejate ale sistemului (de exemplu, în rădăcina C:\\) fără a pierde textul scris în caz de anulare.
• Design modern Windows 11: Suport complet pentru modurile Luminos și Întunecat, cu accelerare hardware Direct2D/DirectWrite pentru un text impecabil de clar.
• Instrumente avansate de editare: Cursoare multiple, selecție pe coloană (Alt+tragere), căutare și înlocuire cu expresii regulate (Regex).
• Evidențierea sintaxei: Reguli integrate de colorare pentru C/C++, Python, tehnologii Web (HTML, CSS, JS), Markdown și multe altele.
• Detectare inteligentă a codificării: Recunoaștere automată și precisă pentru UTF-8, UTF-16, ANSI și diacritice românești.
• Confidențialitate deplină: Funcționează 100% offline, fără telemetrie și fără procese inutile în fundal.""",
        "features": [
            "Cursoare multiple și selecție pe coloană pentru editarea simultană a mai multor linii.",
            "Arhitectură Piece Table rapidă pentru fișiere uriașe de mai mulți gigaocteți.",
            "Salvare sigură cu drepturi de administrator (UAC) pentru fișierele protejate.",
            "Mod Luminos și Întunecat cu redare clară a textului prin Direct2D.",
            "Detectare automată și fiabilă a codificărilor UTF-8, UTF-16 și ANSI.",
            "Căutare și înlocuire avansată cu suport complet pentru expresii regulate (Regex)."
        ],
        "keywords": [
            "editor de text",
            "notepad",
            "carnet de note",
            "editor rapid",
            "editor usor",
            "cod",
            "text"
        ],
        "releaseNotes": """Note de lansare miu v1.0.38
🌐 Previzualizare live WebView2 (Alt+P) și redare fidelă Markdown (CommonMark / GFM)

• Previzualizare divizată în timp real pentru HTML, SVG și Markdown (Alt+P):
  Apăsați Alt+P pentru a deschide panoul de previzualizare cu actualizare automată, reîncărcare F5, separator reglabil, bare de defilare suprapuse, temă întunecată/luminoasă și încărcare de resurse locale (imagini, CSS, JS).
• Redare fidelă CommonMark și GitHub Flavored Markdown (GFM):
  Suport complet pentru tabele GFM, liste de sarcini (- [ ] / - [x]), liste imbricate, citate pe mai multe rânduri, alerte GFM ([!NOTE], [!TIP], [!WARNING] etc.), protecție snake_case, linkuri automate și HTML inline (<kbd>).
• Previzualizare rapidă pentru fișiere uriașe și căutare F3 instantanee:
  Afișează instantaneu primii 512 KB fără copiere pe disc (în 28 de limbi) și remediază blocarea la reluarea căutării cu F3 la sfârșitul fișierelor mari.""",
        "image_captions": [
            "Mod Luminos cu afișare clară și ordonată a documentului",
            "Mod Întunecat cu evidențierea sintaxei codului sursă",
            "Ecranul de ajutor F1 cu lista scurtăturilor de la tastatură"
        ],
        "prefix": "ro"
    },

    "hu-hu": {
        "title": "miu",
        "description": """A miu egy villámgyors, ultrakönnyű és modern szövegszerkesztő Windows rendszerhez.
A másodperc törtrésze alatt elindul, így tökéletes választás gyors jegyzeteléshez, több gigabájtos naplófájlok (logok) elemzéséhez és mindennapi programozáshoz.

【Főbb előnyök】
• Kimagasló teljesítmény: A korszerű Piece Table architektúrának köszönhetően akadozás nélkül, minimális RAM-használattal nyithat meg és szerkeszthet hatalmas fájlokat.
• Mentés rendszergazdai jogosultsággal (UAC): Biztonságosan mentheti a védett rendszerfájlokat (pl. a C:\\ gyökérkönyvtárban) anélkül, hogy megszakítás esetén elveszne a beírt szöveg.
• Elegáns Windows 11 megjelenés: Teljes világos és sötét mód támogatás, Direct2D/DirectWrite hardveres gyorsítással a borotvaéles betűképekért.
• Fejlett szerkesztési funkciók: Több kurzor (multi-cursor), oszlopos kijelölés (Alt+húzás), valamint reguláris kifejezéseket (Regex) támogató keresés és csere.
• Szintaxiskiemelés: Beépített színezés C/C++, Python, webes nyelvek (HTML, CSS, JS), Markdown és egyéb formátumokhoz.
• Intelligens karakterkódolás-felismerés: Az UTF-8, UTF-16, ANSI és közép-európai kódlapok pontos, automatikus felismerése.
• Teljes adatvédelem: 100%-ban offline működik, nincs telemetria és nincsenek felesleges háttérfolyamatok.""",
        "features": [
            "Több kurzor és oszlopos kijelölés több sor egyidejű szerkesztéséhez.",
            "Piece Table architektúra a több gigabájtos fájlok zökkenőmentes kezeléséhez.",
            "Biztonságos mentés UAC rendszergazdai jogosultsággal védett fájlokhoz.",
            "Világos és sötét mód éles Direct2D szövegrendereléssel.",
            "UTF-8, UTF-16 és ANSI karakterkódolások automatikus felismerése.",
            "Hatékony keresés és csere teljes reguláris kifejezés (Regex) támogatással."
        ],
        "keywords": [
            "szövegszerkesztő",
            "jegyzettömb",
            "notepad",
            "gyors szerkesztő",
            "könnyű",
            "kódszerkesztő",
            "szöveg"
        ],
        "releaseNotes": """miu v1.0.38 kiadási megjegyzések
🌐 WebView2 élő előnézet (Alt+P) és hű CommonMark / GFM Markdown-megjelenítés

• Beépített osztott élő előnézet HTML, SVG és Markdown fájlokhoz (Alt+P):
  Nyomja meg az Alt+P billentyűkombinációt az automatikusan frissülő előnézeti panel megnyitásához F5 újratöltéssel, átméretezhető elválasztóval, átfedő görgetősávokkal, sötét/világos témával és helyi erőforrások (képek, CSS, JS) támogatásával.
• Hű CommonMark és GitHub Flavored Markdown (GFM) megjelenítés:
  Teljes támogatás GFM-táblázatokhoz, feladatlistákhoz (- [ ] / - [x]), egymásba ágyazott listákhoz, többsoros idézetekhez, GFM-figyelmeztetésekhez ([!NOTE], [!TIP], [!WARNING] stb.), snake_case védelemhez, automatikus hivatkozásokhoz és `<kbd>` elemekhez.
• Óriásfájlok gyors előnézete és azonnali F3 keresés:
  Lemezmásolás nélkül, azonnal megjeleníti az első 512 KB-ot (28 nyelven), és javítja a nagy fájlok végén történő F3 keresési lefagyást.""",
        "image_captions": [
            "Világos mód letisztult és jól áttekinthető dokumentumnézettel",
            "Sötét mód színes kódszintaxis-kiemeléssel",
            "F1 súgóképernyő a gyorsbillentyűk teljes listájával"
        ],
        "prefix": "hu"
    },

    "el-gr": {
        "title": "miu",
        "description": """Το miu είναι ένας εξαιρετικά γρήγορος, ελαφρύς και σύγχρονος επεξεργαστής κειμένου για τα Windows.
Ξεκινά σε κλάσματα δευτερολέπτου και είναι ιδανικός για γρήγορες σημειώσεις, ανάλυση τεράστιων αρχείων καταγραφής (logs) πολλών gigabyte και καθημερινό προγραμματισμό.

【Κύρια πλεονεκτήματα】
• Κορυφαία απόδοση: Χάρη στην αρχιτεκτονική Piece Table, ανοίγει και επεξεργάζεται ομαλά αρχεία πολλών gigabyte με ελάχιστη κατανάλωση μνήμης RAM.
• Αποθήκευση με δικαιώματα διαχειριστή (UAC): Αποθηκεύστε με ασφάλεια προστατευμένα αρχεία συστήματος (π.χ. στον δίσκο C:\\) χωρίς απώλεια κειμένου σε περίπτωση ακύρωσης.
• Σύγχρονος σχεδιασμός Windows 11: Πλήρης υποστήριξη Φωτεινής και Σκοτεινής λειτουργίας με επιτάχυνση υλικού Direct2D/DirectWrite για κρυστάλλινη ευκρίνεια γραμματοσειρών.
• Προηγμένα εργαλεία επεξεργασίας: Πολλαπλοί δρομείς, επιλογή στήλης (Alt+σύρσιμο), εύρεση και αντικατάσταση με κανονικές εκφράσεις (Regex).
• Επισήμανση σύνταξης: Ενσωματωμένος χρωματισμός κώδικα για C/C++, Python, Web (HTML, CSS, JS), Markdown και άλλα.
• Έξυπνη αναγνώριση κωδικοποίησης: Αυτόματη και ακριβής ανίχνευση UTF-8, UTF-16, ANSI και ελληνικών κωδικοσελίδων.
• Απόλυτη ιδιωτικότητα: Λειτουργεί 100% εκτός σύνδεσης (offline), χωρίς τηλεμετρία ή διεργασίες παρασκηνίου.""",
        "features": [
            "Πολλαπλοί δρομείς και επιλογή στήλης για ταυτόχρονη επεξεργασία πολλών γραμμών.",
            "Αρχιτεκτονική Piece Table για άνετη διαχείριση τεράστιων αρχείων.",
            "Ασφαλής αποθήκευση με δικαιώματα διαχειριστή (UAC) για αρχεία συστήματος.",
            "Φωτεινή και Σκοτεινή λειτουργία με ευκρινή απεικόνιση κειμένου Direct2D.",
            "Αυτόματη αναγνώριση κωδικοποίησης UTF-8, UTF-16 και ANSI.",
            "Ισχυρή εύρεση και αντικατάσταση με πλήρη υποστήριξη Regex."
        ],
        "keywords": [
            "επεξεργαστής κειμένου",
            "σημειωματάριο",
            "notepad",
            "γρήγορος",
            "ελαφρύς",
            "κώδικας",
            "κείμενο"
        ],
        "releaseNotes": """Σημειώσεις έκδοσης miu v1.0.38
🌐 Ζωντανή προεπισκόπηση WebView2 (Alt+P) & πιστή απόδοση Markdown (CommonMark / GFM)

• Ενσωματωμένη ζωντανή προεπισκόπηση για HTML, SVG & Markdown (Alt+P):
  Πατήστε Alt+P για να ανοίξετε το παράθυρο προεπισκόπησης με αυτόματη ενημέρωση, ανανέωση F5, ρυθμιζόμενο διαχωριστικό, μπάρες κύλισης, σκοτεινό/φωτεινό θέμα και υποστήριξη τοπικών αρχείων (εικόνες, CSS, JS).
• Πιστή απόδοση CommonMark & GitHub Flavored Markdown (GFM):
  Πλήρης υποστήριξη για πίνακες GFM, λίστες εργασιών (- [ ] / - [x]), ένθετες λίστες, παραθέσεις, ειδοποιήσεις GFM ([!NOTE], [!TIP], [!WARNING] κ.λπ.), προστασία snake_case, αυτόματους συνδέσμους και στοιχεία HTML (<kbd>).
• Γρήγορη προεπισκόπηση τεράστιων αρχείων & άμεση αναζήτηση F3:
  Προβάλλει αμέσως τα πρώτα 512 KB χωρίς αντιγραφή στον δίσκο (σε 28 γλώσσες) και διορθώνει το πάγωμα κατά την αναζήτηση F3 στο τέλος μεγάλων αρχείων.""",
        "image_captions": [
            "Φωτεινή λειτουργία με καθαρή και ευανάγνωστη προβολή εγγράφου",
            "Σκοτεινή λειτουργία με χρωματική επισήμανση σύνταξης κώδικα",
            "Οθόνη βοήθειας F1 με τη λίστα συντομεύσεων πληκτρολογίου"
        ],
        "prefix": "el"
    },

    "da-dk": {
        "title": "miu",
        "description": """miu er et lynhurtigt, ultralet og moderne tekstredigeringsprogram til Windows.
Det starter på et splitsekund og er perfekt til hurtige noter, analyse af enorme logfiler på flere gigabyte samt daglig programmering.

【Vigtigste fordele】
• Uovertruffen ydeevne: Takket være Piece Table-arkitektur åbner og redigerer du gigantiske filer helt uden forsinkelse og med minimalt RAM-forbrug.
• Gem som administrator (UAC): Gem beskyttede systemfiler (f.eks. direkte på C:\\) sikkert med administratorrettigheder uden at miste dine ændringer ved annullering.
• Elegant Windows 11-design: Fuld understøttelse af lys og mørk tilstand med Direct2D/DirectWrite-hardwareacceleration for knivskarp tekst.
• Avancerede redigeringsværktøjer: Multimarkør, kolonnevalg (Alt+træk) samt søg og erstat med regulære udtryk (Regex).
• Syntaksfremhævning: Indbygget farvekodning til C/C++, Python, webformater (HTML, CSS, JS), Markdown m.fl.
• Intelligent tegnsætregistrering: Automatisk og præcis genkendelse af UTF-8, UTF-16, ANSI og nordiske tegnsæt.
• Fuldt privatliv: Fungerer 100 % offline, uden telemetri og uden unødvendige baggrundsprocesser.""",
        "features": [
            "Multimarkør og kolonnevalg til samtidig redigering af flere linjer.",
            "Piece Table-arkitektur til lynhurtig håndtering af filer på flere gigabyte.",
            "Sikker gemning med UAC-administratorrettigheder til beskyttede systemfiler.",
            "Lys og mørk tilstand med knivskarp Direct2D-tekstrendering.",
            "Automatisk registrering af UTF-8-, UTF-16- og ANSI-tegnsæt.",
            "Kraftfuld søg og erstat-funktion med fuld Regex-understøttelse."
        ],
        "keywords": [
            "teksteditor",
            "notesblok",
            "notepad",
            "hurtig editor",
            "letvægts",
            "kodeeditor",
            "tekst"
        ],
        "releaseNotes": """Udgivelsesnoter for miu v1.0.38
🌐 WebView2 live-forhåndsvisning (Alt+P) & præcis CommonMark / GFM Markdown-gengivelse

• Indbygget delt live-forhåndsvisning til HTML, SVG & Markdown (Alt+P):
  Tryk på Alt+P for at åbne et forhåndsvisningspanel med automatisk opdatering, F5-genindlæsning, justerbar deler, overlay-rullepaneler, mørkt/lyst tema og lokale ressourcer (billeder, CSS, JS).
• Præcis CommonMark & GitHub Flavored Markdown (GFM):
  Fuld understøttelse af GFM-tabeller, opgavelister (- [ ] / - [x]), indlejrede lister, citater, GFM-advarsler ([!NOTE], [!TIP], [!WARNING] m.fl.), snake_case-beskyttelse, autolinks og inline-HTML (<kbd>).
• Hurtig forhåndsvisning af enorme filer & øjeblikkelig F3-søgning:
  Viser straks de første 512 KB uden diskkopiering (på 28 sprog) og retter frysning ved F3-søgning i slutningen af store filer.""",
        "image_captions": [
            "Lys tilstand med ren og overskuelig dokumentvisning",
            "Mørk tilstand med tydelig syntaksfremhævning af kode",
            "F1-hjælpeskærm med oversigt over tastaturgenveje"
        ],
        "prefix": "da"
    },

    "fi-fi": {
        "title": "miu",
        "description": """miu on salamannopea, erittäin kevyt ja moderni tekstieditori Windowsille.
Se käynnistyy silmänräpäyksessä ja sopii täydellisesti nopeisiin muistiinpanoihin, useiden gigatavujen kokoisten lokitiedostojen tutkimiseen sekä päivittäiseen ohjelmointiin.

【Keskeiset edut】
• Huippuluokan suorituskyky: Piece Table -arkkitehtuurin ansiosta avaat ja muokkaat valtavia tiedostoja sujuvasti erittäin pienellä RAM-muistinkulutuksella.
• Tallennus järjestelmänvalvojana (UAC): Tallenna suojatut järjestelmätiedostot (esim. C:\\-juuressa) turvallisesti menettämättä kirjoitettua tekstiä peruutustilanteessa.
• Tyylikäs Windows 11 -ulkoasu: Täysi vaalean ja tumman tilan tuki sekä Direct2D/DirectWrite-laitteistokiihdytys veitsenterävään tekstin piirtoon.
• Monipuoliset muokkaustyökalut: Useita kohdistimia (multi-cursor), sarakevalinta (Alt+vedä) sekä haku ja korvaus säännöllisillä lausekkeilla (Regex).
• Syntaksin korostus: Sisäänrakennettu värikorostus C/C++-, Python-, Web (HTML, CSS, JS)- ja Markdown-tiedostoille.
• Älykäs merkistöntunnistus: UTF-8-, UTF-16-, ANSI- ja pohjoismaisten merkistöjen tarkka automaattinen tunnistus.
• Täysi yksityisyys: Toimii 100 % offline-tilassa ilman telemetriaa tai turhia taustaprosesseja.""",
        "features": [
            "Useita kohdistimia ja sarakevalinta usean rivin samanaikaiseen muokkaukseen.",
            "Piece Table -arkkitehtuuri gigatavuluokan tiedostojen sujuvaan käsittelyyn.",
            "Turvallinen UAC-järjestelmänvalvojatallennus suojatuille järjestelmätiedostoille.",
            "Vaalea ja tumma tila tarkalla Direct2D-tekstinpiirrolla.",
            "Automaattinen UTF-8-, UTF-16- ja ANSI-merkistöjen tunnistus.",
            "Tehokas etsi ja korvaa -toiminto täydellä Regex-tuella."
        ],
        "keywords": [
            "tekstieditori",
            "muistio",
            "notepad",
            "nopea editori",
            "kevyt editori",
            "koodieditori",
            "teksti"
        ],
        "releaseNotes": """miu v1.0.38 julkaisutiedot
🌐 WebView2-reaaliaikainen esikatselu (Alt+P) ja tarkka CommonMark / GFM Markdown -renderöinti

• Sisäänrakennettu jaettu esikatselu HTML-, SVG- ja Markdown-tiedostoille (Alt+P):
  Avaa automaattisesti päivittyvä esikatselupaneeli painamalla Alt+P. Sisältää F5-päivityksen, säädettävän jakajan, vierityspalkit, tumman/vaalean teeman ja paikallisten tiedostojen (kuvat, CSS, JS) tuen.
• Tarkka CommonMark ja GitHub Flavored Markdown (GFM):
  Täysi tuki GFM-taulukoille, tehtävälistoille (- [ ] / - [x]), sisäkkäisille listoille, lainauksille, GFM-huomautuksille ([!NOTE], [!TIP], [!WARNING] jne.), snake_case-suojaukselle, automaattilinkeille ja `<kbd>`-tunnisteille.
• Suurten tiedostojen nopea esikatselu ja välitön F3-haku:
  Esikatselee ensimmäiset 512 kt välittömästi ilman levykopioita (28 kielellä) ja korjaa jäätymisen F3-haun kiertäessä tiedoston lopusta alkuun.""",
        "image_captions": [
            "Vaalea tila selkeällä ja puhtaalla asiakirjanäkymällä",
            "Tumma tila koodin syntaksin värikorostuksella",
            "F1-ohjenäyttö ja pikanäppäinten luettelo"
        ],
        "prefix": "fi"
    },

    "nb-no": {
        "title": "miu",
        "description": """miu er et lynraskt, ultralett og moderne tekstredigeringsprogram for Windows.
Det starter på et øyeblikk og passer perfekt til raske notater, analyse av enorme loggfiler på flere gigabyte og daglig koding.

【Viktige fordeler】
• Overlegen ytelse: Takket være Piece Table-arkitektur åpner og redigerer du gigantiske filer helt uten forsinkelse og med minimalt RAM-forbruk.
• Lagre som administrator (UAC): Lagre beskyttede systemfiler (f.eks. direkte på C:\\) trygt med administratorrettigheter uten å miste tekst ved avbrudd.
• Moderne Windows 11-design: Full støtte for lys og mørk modus med Direct2D/DirectWrite-maskinvareakselerasjon for sylskarp tekst.
• Kraftige redigeringsverktøy: Multimarkør, kolonnevalg (Alt+dra), samt søk og erstatt med regulære uttrykk (Regex).
• Syntaksutheving: Innebygd fargekoding for C/C++, Python, webformater (HTML, CSS, JS), Markdown med mer.
• Smart tegnsettgjenkjenning: Automatisk og nøyaktig gjenkjenning av UTF-8, UTF-16, ANSI og norske tegn.
• Fullt personvern: Fungerer 100 % offline, uten telemetri og uten unødvendige bakgrunnsprosesser.""",
        "features": [
            "Multimarkør og kolonnevalg for samtidig redigering av flere linjer.",
            "Piece Table-arkitektur for smidig håndtering av filer på flere gigabyte.",
            "Trygg lagring med UAC-administratorrettigheter for beskyttede systemfiler.",
            "Lys og mørk modus med sylskarp Direct2D-tekstgjengivelse.",
            "Automatisk gjenkjenning av UTF-8-, UTF-16- og ANSI-koding.",
            "Kraftig søk og erstatt-funksjon med full støtte for Regex."
        ],
        "keywords": [
            "teksteditor",
            "notisblokk",
            "notepad",
            "rask editor",
            "lettvekts",
            "kodeeditor",
            "tekst"
        ],
        "releaseNotes": """Utgivelsesnotater for miu v1.0.38
🌐 WebView2 direkte forhåndsvisning (Alt+P) og nøyaktig CommonMark / GFM Markdown-gjengivelse

• Innebygd delt forhåndsvisning for HTML, SVG og Markdown (Alt+P):
  Trykk Alt+P for å åpne et forhåndsvisningspanel med automatisk oppdatering, F5-oppdatering, justerbar deler, rullefelt, mørkt/lyst tema og lokale ressurser (bilder, CSS, JS).
• Nøyaktig CommonMark og GitHub Flavored Markdown (GFM):
  Full støtte for GFM-tabeller, oppgavelister (- [ ] / - [x]), nestede lister, sitater, GFM-varsler ([!NOTE], [!TIP], [!WARNING] osv.), snake_case-beskyttelse, autolenker og inline-HTML (<kbd>).
• Rask forhåndsvisning av enorme filer og umiddelbar F3-søking:
  Forhåndsviser de første 512 KB umiddelbart uten diskkopiering (på 28 språk) og løser frysingen ved F3-søk på slutten av store filer.""",
        "image_captions": [
            "Lys modus med ren og oversiktlig dokumentvisning",
            "Mørk modus med tydelig syntaksutheving av kode",
            "F1-hjelpeskjerm med oversikt over hurtigtaster"
        ],
        "prefix": "nb"
    },

    "ms-my": {
        "title": "miu",
        "description": """miu ialah penyunting teks yang ultra-pantas, sangat ringan dan moden yang direka khas untuk Windows.
Ia dilancarkan dalam sekelip mata dan sangat sesuai untuk nota pantas, memeriksa fail log bersaiz gigabait serta penulisan kod harian.

【Kelebihan Utama】
• Prestasi Luar Biasa: Dengan seni bina Piece Table yang canggih, buka dan sunting fail teks bersaiz berbilang gigabait dengan lancar menggunakan RAM yang minimum.
• Simpan sebagai Pentadbir (UAC): Simpan fail sistem yang dilindungi (seperti dalam pemacu C:\\) dengan selamat melalui peningkatan UAC tanpa kehilangan teks jika dibatalkan.
• Reka Bentuk Moden Windows 11: Sokongan penuh untuk Mod Cerah dan Gelap dengan pecutan perkakasan Direct2D/DirectWrite untuk paparan fon yang tajam.
• Alat Penyuntingan Pintar: Berbilang kursor (multi-cursor), pilihan blok lajur (Alt+seret), serta cari dan ganti menggunakan ungkapan biasa (Regex).
• Penyerlahan Sintaks: Penyerlahan warna terbina dalam untuk C/C++, Python, Web (HTML, CSS, JS), Markdown dan banyak lagi.
• Pengesanan Pengekodan Automatik: Mengenal pasti UTF-8, UTF-16 dan ANSI secara automatik dengan tepat.
• Privasi Sepenuhnya: Berfungsi 100% di luar talian (offline), tiada telemetri dan tiada proses latar belakang yang membebankan.""",
        "features": [
            "Berbilang kursor dan pilihan lajur untuk menyunting banyak baris serentak.",
            "Seni bina Piece Table yang lancar untuk fail besar bersaiz gigabait.",
            "Simpanan selamat dengan hak pentadbir UAC untuk fail sistem yang dilindungi.",
            "Mod Cerah dan Gelap dengan paparan teks Direct2D yang tajam.",
            "Pengesanan automatik yang tepat untuk pengekodan UTF-8, UTF-16 dan ANSI.",
            "Fungsi cari dan ganti yang berkuasa dengan sokongan penuh Regex."
        ],
        "keywords": [
            "penyunting teks",
            "buku nota",
            "notepad",
            "penyunting pantas",
            "ringan",
            "penyunting kod",
            "teks"
        ],
        "releaseNotes": """Nota Keluaran miu v1.0.38
🌐 Pratonton Langsung WebView2 (Alt+P) & Paparan Markdown CommonMark / GFM Tepat

• Pratonton Langsung Skrin Terbahagi untuk HTML, SVG & Markdown (Alt+P):
  Tekan Alt+P untuk membuka panel pratonton langsung dengan kemas kini automatik, muat semula F5, pembahagi boleh laras, bar skrol tindanan, tema Gelap/Cerah dan pemetaan aset tempatan (imej, CSS, JS).
• Paparan CommonMark & GitHub Flavored Markdown (GFM) yang Tepat:
  Sokongan penuh untuk jadual GFM, senarai tugas (- [ ] / - [x]), senarai bersarang, petikan berbilang baris, kotak makluman GFM ([!NOTE], [!TIP], [!WARNING], dll.), perlindungan snake_case, pautan automatik dan tag HTML sebaris (<kbd>).
• Pratonton Pantas Fail Gergasi & Carian F3 Segera:
  Memaparkan pratonton 512 KB pertama secara serta-merta tanpa salinan cakera (dalam 28 bahasa) dan membetulkan masalah kaku apabila menekan F3 di penghujung fail besar.""",
        "image_captions": [
            "Mod Cerah dengan paparan dokumen yang kemas dan jelas",
            "Mod Gelap dengan penyerlahan sintaks kod yang terang",
            "Skrin Bantuan F1 dengan senarai lengkap pintasan papan kekunci"
        ],
        "prefix": "ms"
    }
}
METADATA_LANGS = METADATA_10_LANGS
