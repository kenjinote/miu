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
        "releaseNotes": """miu v1.0.36 更新日誌
🚀 執行檔體積大幅縮減 19.9% & 編碼自動偵測提速 15.6%

・執行檔體積縮減約 20%（1,053.5 KB → 844.0 KB / 減少 209.5 KB）：
  在 100% 保持原生單一執行檔（EXE）便攜性與安全性的前提下，透過移除冗餘 C++ iostream/locale 庫開銷、將 15 種語言的語法高亮表轉為編譯期 constexpr 靜態常數（啟動時零堆積記憶體配置）及整合 28 國語言對話框模板，成功縮減 214,528 位元組（-19.9%）。
・字元編碼自動偵測提速 15.6% 並強化 25+ 種編碼支援：
  最佳化 CED 編碼識別引擎，在不犧牲任何識別精度的前提下將連續識別速度提升 15.6%（50,000 次識別由 158.28 ms 降至 133.52 ms，單檔僅需約 2.67 微秒），並全面強化 Big5、GB18030、EUC-JP、ISO-2022-JP、無 BOM UTF-16 及多國字集映射。
・全 28 種語言 × 高 DPI（100%~250%）動態自適應排版：
  尋找/取代（Ctrl+F / Ctrl+H）與跳至行（Ctrl+G）對話框現可根據當前語言文字寬度與螢幕 DPI 動態調整按鈕與視窗尺寸，徹底消除文字截斷。""",
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
        "releaseNotes": """miu v1.0.36 Versionshinweise
🚀 19,9 % kleinere Programmdatei & 15,6 % schnellere Zeichencodierungserkennung

• 19,9 % geringere Dateigröße (1.053,5 KB → 844,0 KB / -209,5 KB):
  Die Größe der eigenständigen EXE-Datei wurde um 214.528 Bytes (-19,9 %) reduziert – durch Entfernung von C++-iostream/locale-Overhead, Umstellung aller 15 Syntaxhervorhebungstabellen auf kompilierzeitkonstantes constexpr (null Heap-Allokationen beim Start) und Zusammenführung der 28 Sprachvorlagen.
• 15,6 % schnellere automatische Codierungserkennung & über 25 Zeichensätze:
  Beschleunigung der Compact Encoding Detection (CED) um 15,6 % (158,28 ms → 133,52 ms bei 50.000 Prüfungen; ~2,67 µs/Datei) ohne Genauigkeitsverlust sowie erweiterte Unterstützung für EUC-JP, ISO-2022-JP, UTF-16 ohne BOM, GB18030, KOI8-R und europäische Codepages.
• Dynamisches High-DPI-Layout für alle 28 Sprachen (100 %–250 % DPI):
  Dialoge für Suchen/Ersetzen (Strg+F / Strg+H) und Gehe zu Zeile (Strg+G) passen Schaltflächen- und Fensterbreiten automatisch an Textlänge und Monitor-DPI an.""",
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
        "releaseNotes": """Notes de version miu v1.0.36
🚀 Taille de l'exécutable réduite de 19,9 % & détection d'encodage 15,6 % plus rapide

• Réduction de 19,9 % de la taille du binaire (1 053,5 Ko → 844,0 Ko / -209,5 Ko) :
  Allègement de l'exécutable autonome de 214 528 octets (-19,9 %) grâce à la suppression des flux C++ iostream/locale, au passage des 15 tables de coloration syntaxique en constantes constexpr (zéro allocation dynamique au démarrage) et à l'unification des ressources pour 28 langues.
• Détection automatique d'encodage 15,6 % plus rapide & 25+ encodages pris en charge :
  Accélération du moteur CED de 15,6 % (158,28 ms → 133,52 ms sur 50 000 analyses ; ~2,67 µs/fichier) sans perte de précision, avec prise en charge étendue de EUC-JP, ISO-2022-JP, UTF-16 sans BOM, GB18030, KOI8-R et des pages de codes européennes.
• Mise en page dynamique Haute-DPI (100 % à 250 %) pour les 28 langues :
  Les boîtes de dialogue Rechercher/Remplacer (Ctrl+F / Ctrl+H) et Atteindre la ligne (Ctrl+G) ajustent automatiquement leurs dimensions selon la langue et le DPI.""",
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
        "releaseNotes": """Notas de la versión miu v1.0.36
🚀 Tamaño del ejecutable reducido un 19,9 % y detección de codificación un 15,6 % más rápida

• Reducción del 19,9 % en el tamaño del archivo (1.053,5 KB → 844,0 KB / -209,5 KB):
  Se redujo el ejecutable autónomo en 214.528 bytes (-19,9 %) eliminando la sobrecarga de C++ iostream/locale, convirtiendo las 15 tablas de sintaxis en constantes constexpr en tiempo de compilación (cero asignaciones dinámicas al iniciar) y unificando plantillas de 28 idiomas.
• Detección automática de codificación un 15,6 % más rápida y más de 25 codificaciones:
  Optimización del motor CED con una mejora del 15,6 % en velocidad (158,28 ms → 133,52 ms en 50.000 detecciones; ~2,67 µs/archivo) sin pérdida de precisión, ampliando el soporte para EUC-JP, ISO-2022-JP, UTF-16 sin BOM, GB18030, KOI8-R y páginas de códigos europeas.
• Diseño automático dinámico en Alta-DPI (100 %–250 %) para los 28 idiomas:
  Los diálogos Buscar/Reemplazar (Ctrl+F / Ctrl+H) e Ir a la línea (Ctrl+G) ajustan dinámicamente el ancho de botones y ventanas evitando cualquier recorte de texto.""",
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
        "releaseNotes": """Notas de versão miu v1.0.36
🚀 Executável 19,9% menor e detecção automática de codificação 15,6% mais rápida

• Redução de 19,9% no tamanho do binário (1.053,5 KB → 844,0 KB / -209,5 KB):
  Redução de 214.528 bytes (-19,9%) no executável portátil eliminando sobrecargas de C++ iostream/locale, convertendo todas as 15 tabelas de sintaxe em constantes constexpr (zero alocações de heap na inicialização) e unificando recursos de 28 idiomas.
• Detecção de codificação 15,6% mais rápida e suporte a mais de 25 codificações:
  Aceleração do motor CED em 15,6% (158,28 ms → 133,52 ms em 50.000 detecções; ~2,67 µs/arquivo) com precisão 100% preservada, além de suporte aprimorado para EUC-JP, ISO-2022-JP, UTF-16 sem BOM, GB18030, KOI8-R e páginas de código europeias.
• Layout automático dinâmico em Alto-DPI (100%–250%) para todos os 28 idiomas:
  As janelas Localizar/Substituir (Ctrl+F / Ctrl+H) e Ir para a linha (Ctrl+G) dimensionam botões e rótulos dinamicamente sem cortes de texto.""",
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
        "releaseNotes": """Примечания к выпуску miu v1.0.36
🚀 Уменьшение размера EXE на 19,9 % и ускорение автоопределения кодировки на 15,6 %

• Размер исполняемого файла уменьшен на 19,9 % (1 053,5 КБ → 844,0 КБ / -209,5 КБ):
  Размер автономного файла miu.exe сокращен на 214 528 байт (-19,9 %) за счет удаления накладных расходов C++ iostream/locale, перевода таблиц подсветки синтаксиса 15 языков в константы времени компиляции constexpr (ноль аллокаций кучи при запуске) и объединения шаблонов диалогов 28 языков.
• Ускорение автоопределения кодировки на 15,6 % и поддержка 25+ кодировок:
  Движок Compact Encoding Detection (CED) ускорен на 15,6 % (158,28 мс → 133,52 мс на 50 000 проверок; ~2,67 мкс/файл) без потери точности. Расширена поддержка KOI8-R, KOI8-U, CP1251, IBM866, ISO-8859-5, EUC-JP, ISO-2022-JP, UTF-16 без BOM и GB18030.
• Динамическая авторазметка диалогов для 28 языков и высокого DPI (100 %–250 %):
  Окна поиска/замены (Ctrl+F / Ctrl+H) и перехода к строке (Ctrl+G) автоматически подстраивают размеры кнопок и полей под длину перевода и масштаб экрана.""",
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
        "releaseNotes": """Note di rilascio miu v1.0.36
🚀 Dimensioni dell'eseguibile ridotte del 19,9% e rilevamento codifica più veloce del 15,6%

• Riduzione del 19,9% delle dimensioni del file (1.053,5 KB → 844,0 KB / -209,5 KB):
  L'eseguibile autonomo è stato ridotto di 214.528 byte (-19,9%) eliminando l'overhead C++ iostream/locale, spostando tutte le 15 tabelle di sintassi in costanti constexpr (zero allocazioni heap all'avvio) e unificando le risorse per 28 lingue.
• Rilevamento automatico codifica più rapido del 15,6% e supporto per oltre 25 codifiche:
  Accelerato il motore CED del 15,6% (158,28 ms → 133,52 ms su 50.000 rilevamenti; ~2,67 µs/file) senza alcuna perdita di precisione, con supporto esteso per EUC-JP, ISO-2022-JP, UTF-16 senza BOM, GB18030, KOI8-R e codepage europee.
• Layout automatico dinamico ad alto DPI (100%–250%) per tutte le 28 lingue:
  Le finestre Trova/Sostituisci (Ctrl+F / Ctrl+H) e Vai alla riga (Ctrl+G) adattano dinamicamente pulsanti e dimensioni in base alla lingua e al DPI.""",
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
        "releaseNotes": """Informacje o wydaniu miu v1.0.36
🚀 Rozmiar pliku EXE mniejszy o 19,9% oraz o 15,6% szybsze wykrywanie kodowania

• Zmniejszenie rozmiaru pliku wykonywalnego o 19,9% (1 053,5 KB → 844,0 KB / -209,5 KB):
  Rozmiar samodzielnego pliku miu.exe zmniejszono o 214 528 bajtów (-19,9%) dzięki usunięciu narzutu C++ iostream/locale, przeniesieniu wszystkich 15 tablic kolorowania składni do stałych constexpr (zero alokacji sterty przy starcie) oraz ujednoliceniu zasobów dla 28 języków.
• O 15,6% szybsze automatyczne wykrywanie kodowania i obsługa ponad 25 stron kodowych:
  Przyspieszono silnik CED o 15,6% (158,28 ms → 133,52 ms przy 50 000 testów; ~2,67 µs/plik) przy zachowaniu 100% dokładności oraz rozszerzono obsługę stron kodowych Europy Środkowo-Wschodniej (CP1250, ISO-8859-2), UTF-16 bez BOM, EUC-JP, ISO-2022-JP, GB18030 i KOI8-R.
• Dynamiczny układ okien dialogowych High-DPI (100%–250%) dla 28 języków:
  Okna Znajdź/Zamień (Ctrl+F / Ctrl+H) oraz Przejdź do linii (Ctrl+G) automatycznie dopasowują szerokość przycisków i okien do długości tekstu i skali DPI.""",
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
        "releaseNotes": """Versienotities miu v1.0.36
🚀 19,9% kleiner uitvoerbaar bestand & 15,6% snellere coderingsdetectie

• 19,9% kleinere bestandsgrootte (1.053,5 KB → 844,0 KB / -209,5 KB):
  Het zelfstandige uitvoerbare bestand is met 214.528 bytes (-19,9%) verkleind door C++ iostream/locale-overhead te verwijderen, alle 15 syntaxisaccentueringstabellen om te zetten naar compile-time constexpr (nul heap-allocaties bij het opstarten) en dialoogbronnen voor 28 talen te verenigen.
• 15,6% snellere automatische coderingsdetectie & ondersteuning voor 25+ coderingen:
  De CED-coderingsengine is 15,6% versneld (158,28 ms → 133,52 ms over 50.000 controles; ~2,67 µs/bestand) zonder verlies van nauwkeurigheid, met uitgebreide ondersteuning voor EUC-JP, ISO-2022-JP, UTF-16 zonder BOM, GB18030, KOI8-R en Europese tekensets.
• Dynamische High-DPI auto-layout (100%–250%) voor alle 28 talen:
  De vensters Zoeken/Vervangen (Ctrl+F / Ctrl+H) en Ga naar regel (Ctrl+G) passen knop- en vensterbreedtes dynamisch aan op basis van tekstlengte en DPI.""",
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
        "releaseNotes": """miu v1.0.36 Sürüm Notları
🚀 %19,9 Daha Küçük EXE Boyutu ve %15,6 Daha Hızlı Karakter Kodlama Tespiti

• %19,9 Dosya Boyutu Azaltımı (1.053,5 KB → 844,0 KB / -209,5 KB):
  C++ iostream/locale yükünün kaldırılması, 15 sözdizimi vurgulama tablosunun derleme zamanı constexpr sabitlerine dönüştürülmesi (başlangıçta sıfır bellek ayırma) ve 28 dil iletişim kutusu şablonunun birleştirilmesiyle tekil EXE boyutu 214.528 bayt (-%19,9) küçültüldü.
• %15,6 Daha Hızlı Otomatik Kodlama Tespiti ve 25+ Kodlama Desteği:
  CED kodlama tespit motoru doğruluk kaybı olmadan %15,6 hızlandırıldı (50.000 tespitte 158,28 ms → 133,52 ms; dosya başına ~2,67 µs) ve Türkçe (CP1254 / ISO-8859-9), UTF-16 (BOM'suz), EUC-JP, ISO-2022-JP, GB18030, KOI8-R desteği güçlendirildi.
• 28 Dil × Yüksek DPI (%100–%250) Dinamik Otomatik Yerleşim:
  Bul/Değiştir (Ctrl+F / Ctrl+H) ve Satıra Git (Ctrl+G) pencereleri, metin genişliğine ve monitör DPI değerine göre düğme ve pencere boyutlarını otomatik ayarlar.""",
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
        "releaseNotes": """Ghi chú phát hành miu v1.0.36
🚀 Giảm 19,9% dung lượng tệp EXE & Tăng 15,6% tốc độ nhận diện bảng mã

• Giảm gần 20% dung lượng tệp thực thi (1.053,5 KB → 844,0 KB / -209,5 KB):
  Giảm 214.528 byte (-19,9%) dung lượng tệp miu.exe độc lập bằng cách loại bỏ thư viện C++ iostream/locale dư thừa, chuyển toàn bộ 15 bảng tô màu cú pháp sang hằng số constexpr lúc biên dịch (0 cấp phát bộ nhớ heap khi khởi động) và hợp nhất tài nguyên 28 ngôn ngữ.
• Tăng 15,6% tốc độ tự động nhận diện bảng mã & Hỗ trợ hơn 25 bảng mã:
  Tối ưu hóa bộ máy CED giúp tăng tốc độ nhận diện thêm 15,6% (158,28 ms → 133,52 ms cho 50.000 lần kiểm tra; ~2,67 µs/tệp) mà không giảm độ chính xác, đồng thời mở rộng hỗ trợ CP1258 (Tiếng Việt), UTF-16 không BOM, EUC-JP, ISO-2022-JP, GB18030, KOI8-R.
• Bố cục tự động co giãn theo DPI cao (100%–250%) cho cả 28 ngôn ngữ:
  Hộp thoại Tìm/Thay thế (Ctrl+F / Ctrl+H) và Đi tới dòng (Ctrl+G) tự động tính toán kích thước nút và cửa sổ theo độ dài văn bản dịch và DPI màn hình.""",
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
        "releaseNotes": """Примітки до випуску miu v1.0.36
🚀 Зменшення розміру EXE на 19,9 % та прискорення автовизначення кодування на 15,6 %

• Розмір виконуваного файлу зменшено на 19,9 % (1 053,5 КБ → 844,0 КБ / -209,5 КБ):
  Розмір автономного файлу miu.exe скорочено на 214 528 байтів (-19,9 %) завдяки усуненню накладних витрат C++ iostream/locale, переведенню всіх 15 таблиць підсвічування синтаксису в константи часу компіляції constexpr (нуль алокацій купи під час запуску) та об'єднанню шаблонів для 28 мов.
• Прискорення автовизначення кодування на 15,6 % та підтримка 25+ кодувань:
  Рушій CED прискорено на 15,6 % (158,28 мс → 133,52 мс на 50 000 перевірок; ~2,67 мкс/файл) без втрати точності, а також розширено підтримку KOI8-U, KOI8-R, CP1251, IBM866, UTF-16 без BOM, EUC-JP, ISO-2022-JP і GB18030.
• Динамічна авторозмітка діалогів для 28 мов і високого DPI (100 %–250 %):
  Вікна пошуку/заміни (Ctrl+F / Ctrl+H) і переходу до рядка (Ctrl+G) автоматично підлаштовують розміри кнопок і вікон під довжину тексту та масштаб екрана.""",
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
        "releaseNotes": """Catatan Rilis miu v1.0.36
🚀 Ukuran EXE 19,9% Lebih Kecil & Deteksi Enkod Otomatis 15,6% Lebih Cepat

• Pengurangan Ukuran Berkas 19,9% (1.053,5 KB → 844,0 KB / -209,5 KB):
  Ukuran berkas miu.exe mandiri diperkecil sebesar 214.528 byte (-19,9%) dengan menghapus overhead C++ iostream/locale, memindahkan 15 tabel pewarnaan sintaksis ke konstanta constexpr waktu kompilasi (nol alokasi heap saat mulai), dan menyatukan templat dialog 28 bahasa.
• Deteksi Enkod Otomatis 15,6% Lebih Cepat & Dukungan 25+ Enkod:
  Mempercepat mesin CED sebesar 15,6% (158,28 mdtk → 133,52 mdtk pada 50.000 deteksi; ~2,67 µs/berkas) tanpa mengurangi akurasi, serta memperluas dukungan UTF-16 tanpa BOM, EUC-JP, ISO-2022-JP, GB18030, KOI8-R, dan halaman kode internasional.
• Tata Letak Otomatis DPI Tinggi (100%–250%) di Semua 28 Bahasa:
  Dialog Cari/Ganti (Ctrl+F / Ctrl+H) dan Ke Baris (Ctrl+G) secara dinamis menyesuaikan ukuran tombol dan jendela agar teks tidak terpotong.""",
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
        "releaseNotes": """Poznámky k vydání miu v1.0.36
🚀 O 19,9 % menší spustitelný soubor a o 15,6 % rychlejší detekce kódování

• Zmenšení velikosti binárního souboru o 19,9 % (1 053,5 KB → 844,0 KB / -209,5 KB):
  Samostatný soubor miu.exe byl zmenšen o 214 528 bajtů (-19,9 %) odstraněním režie C++ iostream/locale, převedením všech 15 tabulek zvýrazňování syntaxe na konstanty constexpr v době kompilace (nulové alokace haldy při startu) a sjednocením šablon pro 28 jazyků.
• O 15,6 % rychlejší automatická detekce kódování a podpora 25+ znakových sad:
  Engine CED byl zrychlen o 15,6 % (158,28 ms → 133,52 ms při 50 000 detekcích; ~2,67 µs/soubor) při zachování 100% přesnosti a rozšířena podpora středoevropských sad (CP1250, ISO-8859-2), UTF-16 bez BOM, EUC-JP, ISO-2022-JP, GB18030 a KOI8-R.
• Dynamické rozvržení dialogů pro vysoké DPI (100 %–250 %) ve 28 jazycích:
  Dialogy Najít/Nahradit (Ctrl+F / Ctrl+H) a Přejít na řádek (Ctrl+G) automaticky přizpůsobují šířku tlačítek a oken délce překladu a DPI monitoru.""",
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
        "releaseNotes": """Versionsinformation för miu v1.0.36
🚀 19,9 % mindre EXE-fil & 15,6 % snabbare automatisk kodningsdetektering

• 19,9 % mindre filstorlek (1 053,5 KB → 844,0 KB / -209,5 KB):
  Den fristående EXE-filen har minskats med 214 528 byte (-19,9 %) genom att eliminera C++ iostream/locale-overhead, flytta alla 15 syntaxmarkeringstabeller till kompileringstida constexpr-konstanter (noll heap-allokeringar vid start) och förena dialogresurser för 28 språk.
• 15,6 % snabbare kodningsdetektering & stöd för över 25 teckenkodningar:
  CED-motorn har accelererats med 15,6 % (158,28 ms → 133,52 ms över 50 000 kontroller; ~2,67 µs/fil) utan förlust av precision, med utökat stöd för UTF-16 utan BOM, EUC-JP, ISO-2022-JP, GB18030, KOI8-R och europeiska teckentabeller.
• Dynamisk High-DPI-layout (100 %–250 %) för alla 28 språk:
  Dialogrutorna Sök/Ersätt (Ctrl+F / Ctrl+H) och Gå till rad (Ctrl+G) anpassar automatiskt knapp- och fönsterbredd efter textlängd och skärmens DPI.""",
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
        "releaseNotes": """บันทึกประจำรุ่น miu v1.0.36
🚀 ลดขนาดไฟล์ EXE ลง 19.9% และตรวจจับรหัสอักขระเร็วขึ้น 15.6%

• ลดขนาดไฟล์โปรแกรมลงเกือบ 20% (1,053.5 KB → 844.0 KB / -209.5 KB):
  ลดขนาดไฟล์ miu.exe ลง 214,528 ไบต์ (-19.9%) โดยคงความสามารถในการพกพาแบบไฟล์เดียว 100% ด้วยการตัดส่วนเกินของ C++ iostream/locale เปลี่ยนตารางไฮไลต์ไวยากรณ์ทั้ง 15 ภาษาเป็นค่าคงที่ constexpr ตอนคอมไพล์ (ไม่เสียเวลาจองหน่วยความจำ Heap ตอนเปิดโปรแกรม) และรวมเทมเพลต 28 ภาษา
• ตรวจจับรหัสอักขระอัตโนมัติเร็วขึ้น 15.6% และรองรับกว่า 25 รหัสภาษา:
  ปรับปรุงเอนจิน CED ให้ทำงานเร็วขึ้น 15.6% (158.28 ms → 133.52 ms จากการทดสอบ 50,000 ครั้ง; ~2.67 µs/ไฟล์) โดยความแม่นยำครบถ้วน 100% พร้อมเสริมการรองรับภาษาไทย (CP874 / TIS-620), UTF-16 แบบไม่มี BOM, EUC-JP, ISO-2022-JP และ GB18030
• ปรับขนาดหน้าต่างอัตโนมัติตาม DPI สูง (100%–250%) สำหรับทั้ง 28 ภาษา:
  หน้าต่างค้นหา/แทนที่ (Ctrl+F / Ctrl+H) และไปที่บรรทัด (Ctrl+G) คำนวณขนาดปุ่มและหน้าต่างตามความยาวข้อความแปลและ DPI หน้าจอโดยอัตโนมัติ""",
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
        "releaseNotes": """miu v1.0.36 रिलीज़ नोट्स
🚀 EXE फ़ाइल का आकार 19.9% कम और एन्कोडिंग पहचान की गति में 15.6% सुधार

• निष्पादन योग्य फ़ाइल (EXE) के आकार में 19.9% की कमी (1,053.5 KB → 844.0 KB / -209.5 KB):
  अनावश्यक C++ iostream/locale ओवरहेड को हटाकर, सभी 15 सिंटैक्स हाइलाइटिंग तालिकाओं को कंपाइल-टाइम constexpr स्थिरांक में बदलकर (स्टार्टअप पर शून्य हीप आवंटन) और 28 भाषाओं के संवाद संसाधनों को एकीकृत करके miu.exe का आकार 214,528 बाइट्स (-19.9%) कम किया गया है।
• 15.6% तेज़ स्वचालित एन्कोडिंग पहचान और 25+ एन्कोडिंग का विस्तारित समर्थन:
  सटीकता से समझौता किए बिना CED एन्कोडिंग इंजन की गति को 15.6% (50,000 परीक्षणों में 158.28 ms → 133.52 ms; ~2.67 µs/फ़ाइल) बढ़ाया गया है तथा बिना BOM के UTF-16, EUC-JP, ISO-2022-JP, GB18030 और KOI8-R समर्थन को मजबूत किया गया है।
• सभी 28 भाषाओं और उच्च DPI (100%–250%) के लिए डायनामिक ऑटो-लेआउट:
  खोजें/बदलें (Ctrl+F / Ctrl+H) और पंक्ति पर जाएँ (Ctrl+G) संवाद बॉक्स अब अनुवादित टेक्स्ट की लंबाई और मॉनिटर DPI के अनुसार बटन और विंडो का आकार स्वचालित रूप से समायोजित करते हैं।""",
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
        "releaseNotes": """Note de lansare miu v1.0.36
🚀 Dimensiune EXE redusă cu 19,9% și detectare a codificării cu 15,6% mai rapidă

• Reducerea cu 19,9% a dimensiunii fișierului executabil (1.053,5 KB → 844,0 KB / -209,5 KB):
  Fișierul portabil miu.exe a fost redus cu 214.528 octeți (-19,9%) prin eliminarea overhead-ului C++ iostream/locale, mutarea tuturor celor 15 tabele de sintaxă în constante constexpr la compilare (zero alocări heap la pornire) și unificarea șabloanelor pentru 28 de limbi.
• Detectare automată a codificării cu 15,6% mai rapidă și suport pentru 25+ codificări:
  Motorul CED a fost accelerat cu 15,6% (158,28 ms → 133,52 ms la 50.000 de verificări; ~2,67 µs/fișier) fără pierderi de precizie, cu suport extins pentru UTF-16 fără BOM, CP1250/ISO-8859-2, EUC-JP, ISO-2022-JP, GB18030 și KOI8-R.
• Aspect dinamic automat pentru DPI ridicat (100%–250%) în toate cele 28 de limbi:
  Ferestrele Căutare/Înlocuire (Ctrl+F / Ctrl+H) și Salt la linie (Ctrl+G) își ajustează automat lățimea butoanelor și a ferestrei în funcție de text și DPI.""",
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
        "releaseNotes": """miu v1.0.36 kiadási megjegyzések
🚀 19,9%-kal kisebb EXE fájlméret és 15,6%-kal gyorsabb karakterkódolás-felismerés

• 19,9%-os fájlméret-csökkentés (1 053,5 KB → 844,0 KB / -209,5 KB):
  Az önálló miu.exe mérete 214 528 bájttal (-19,9%) csökkent a felesleges C++ iostream/locale többletterhelés eltávolításával, mind a 15 szintaxiskiemelő tábla fordítási idejű constexpr konstansokká alakításával (nulla heap-allokáció indításkor), valamint a 28 nyelvi sablon egyesítésével.
• 15,6%-kal gyorsabb automatikus kódolásfelismerés és 25+ karakterkódolás támogatása:
  A CED motor sebessége 15,6%-kal nőtt (158,28 ms → 133,52 ms 50 000 vizsgálatnál; ~2,67 µs/fájl) pontosságvesztés nélkül, kibővített közép-európai (CP1250 / ISO-8859-2), BOM nélküli UTF-16, EUC-JP, ISO-2022-JP, GB18030 és KOI8-R támogatással.
• Dinamikus magas DPI-s (100%–250%) automatikus elrendezés mind a 28 nyelven:
  A Keresés/Csere (Ctrl+F / Ctrl+H) és az Ugrás sorra (Ctrl+G) ablakok automatikusan a szöveg hosszához és a DPI-hez igazítják a gombok és az ablak méretét.""",
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
        "releaseNotes": """Σημειώσεις έκδοσης miu v1.0.36
🚀 19,9% μικρότερο μέγεθος EXE & 15,6% ταχύτερη ανίχνευση κωδικοποίησης

• Μείωση μεγέθους εκτελέσιμου αρχείου κατά 19,9% (1.053,5 KB → 844,0 KB / -209,5 KB):
  Το μέγεθος του αυτόνομου miu.exe μειώθηκε κατά 214.528 byte (-19,9%) αφαιρώντας την επιβάρυνση C++ iostream/locale, μετατρέποντας τους 15 πίνακες επισήμανσης σύνταξης σε σταθερές constexpr χρόνου μεταγλώττισης (μηδενικές δεσμεύσεις σωρού κατά την εκκίνηση) και ενοποιώντας τους πόρους 28 γλωσσών.
• 15,6% ταχύτερη αυτόματη ανίχνευση κωδικοποίησης & υποστήριξη 25+ κωδικοποιήσεων:
  Η μηχανή CED επιταχύνθηκε κατά 15,6% (158,28 ms → 133,52 ms σε 50.000 ελέγχους· ~2,67 µs/αρχείο) χωρίς απώλεια ακρίβειας, με ενισχυμένη υποστήριξη για Ελληνικά (CP1253 / ISO-8859-7), UTF-16 χωρίς BOM, EUC-JP, ISO-2022-JP, GB18030 και KOI8-R.
• Δυναμική αυτόματη διάταξη υψηλού DPI (100%–250%) και στις 28 γλώσσες:
  Τα παράθυρα Εύρεση/Αντικατάσταση (Ctrl+F / Ctrl+H) και Μετάβαση σε γραμμή (Ctrl+G) προσαρμόζουν αυτόματα το πλάτος κουμπιών και παραθύρων.""",
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
        "releaseNotes": """Udgivelsesnoter for miu v1.0.36
🚀 19,9 % mindre EXE-fil & 15,6 % hurtigere automatisk tegnsætregistrering

• 19,9 % reduktion af filstørrelsen (1.053,5 KB → 844,0 KB / -209,5 KB):
  Den selvstændige miu.exe er reduceret med 214.528 byte (-19,9 %) ved at fjerne C++ iostream/locale-overhead, flytte alle 15 syntaksfremhævningstabeller til kompileringstids-constexpr (nul heap-allokeringer ved opstart) og samle dialogressourcer for 28 sprog.
• 15,6 % hurtigere tegnsætregistrering & understøttelse af 25+ tegnsæt:
  CED-motoren er accelereret med 15,6 % (158,28 ms → 133,52 ms over 50.000 kontroller; ~2,67 µs/fil) uden tab af præcision, med udvidet understøttelse af UTF-16 uden BOM, EUC-JP, ISO-2022-JP, GB18030, KOI8-R og europæiske tegnsæt.
• Dynamisk High-DPI-layout (100 %–250 %) på alle 28 sprog:
  Dialogboksene Søg/Erstat (Ctrl+F / Ctrl+H) og Gå til linje (Ctrl+G) tilpasser automatisk knap- og vinduesbredde efter tekstlængde og skærm-DPI.""",
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
        "releaseNotes": """miu v1.0.36 -julkaisutiedot
🚀 19,9 % pienempi EXE-tiedosto ja 15,6 % nopeampi merkistöntunnistus

• 19,9 % pienempi suoritettava tiedosto (1 053,5 KB → 844,0 KB / -209,5 KB):
  Itsenäisen miu.exe-tiedoston kokoa pienennettiin 214 528 tavulla (-19,9 %) poistamalla C++ iostream/locale -kuorma, muuntamalla kaikki 15 syntaksinkorostustaulukkoa käännösaikaisiksi constexpr-vakioiksi (nolla kekovarausta käynnistyksessä) ja yhdistämällä 28 kielen valintaikkunapohjat.
• 15,6 % nopeampi automaattinen merkistöntunnistus ja yli 25 merkistön tuki:
  CED-tunnistusmoottoria nopeutettiin 15,6 % (158,28 ms → 133,52 ms 50 000 tarkistuksessa; ~2,67 µs/tiedosto) tarkkuudesta tinkimättä sekä laajennettiin BOM-merkittömän UTF-16:n, EUC-JP:n, ISO-2022-JP:n, GB18030:n, KOI8-R:n ja eurooppalaisten koodisivujen tukea.
• Dynaaminen korkean DPI:n (100 %–250 %) automaattiasettelu kaikilla 28 kielellä:
  Etsi/Korvaa (Ctrl+F / Ctrl+H)- ja Siirry riville (Ctrl+G) -ikkunat mitoittavat painikkeet ja ikkunan leveyden automaattisesti käännöksen ja DPI:n mukaan.""",
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
        "releaseNotes": """Utgivelsesnotater for miu v1.0.36
🚀 19,9 % mindre EXE-fil og 15,6 % raskere automatisk tegnsettgjenkjenning

• 19,9 % reduksjon i filstørrelse (1 053,5 KB → 844,0 KB / -209,5 KB):
  Den frittstående miu.exe-filen er redusert med 214 528 byte (-19,9 %) ved å fjerne C++ iostream/locale-overhead, gjøre alle 15 syntaksuthevingstabeller til kompileringstids constexpr-konstanter (null heap-allokeringer ved oppstart) og samle dialogressurser for 28 språk.
• 15,6 % raskere tegnsettgjenkjenning og støtte for over 25 tegnsett:
  CED-motoren er akselerert med 15,6 % (158,28 ms → 133,52 ms over 50 000 kontroller; ~2,67 µs/fil) uten tap av presisjon, med utvidet støtte for UTF-16 uten BOM, EUC-JP, ISO-2022-JP, GB18030, KOI8-R og europeiske tegnsett.
• Dynamisk High-DPI-oppsett (100 %–250 %) for alle 28 språk:
  Dialogboksene Søk/Erstatt (Ctrl+F / Ctrl+H) og Gå til linje (Ctrl+G) tilpasser automatisk knapp- og vindusbredde etter tekstlengde og skjerm-DPI.""",
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
        "releaseNotes": """Nota Keluaran miu v1.0.36
🚀 Saiz Fail EXE 19.9% Lebih Kecil & Pengesanan Pengekodan 15.6% Lebih Pantas

• Pengurangan Saiz Binari 19.9% (1,053.5 KB → 844.0 KB / -209.5 KB):
  Saiz fail boleh laku miu.exe dikurangkan sebanyak 214,528 bait (-19.9%) dengan menghapuskan beban C++ iostream/locale, menukar kesemua 15 jadual penyerlahan sintaks kepada pemalar constexpr masa kompilasi (sifar peruntukan timbunan semasa permulaan) dan menyatukan templat dialog 28 bahasa.
• Pengesanan Pengekodan Automatik 15.6% Lebih Pantas & Sokongan 25+ Pengekodan:
  Enjin CED dipercepatkan sebanyak 15.6% (158.28 ms → 133.52 ms dalam 50,000 ujian; ~2.67 µs/fail) tanpa kehilangan ketepatan, serta memperkukuh sokongan UTF-16 tanpa BOM, EUC-JP, ISO-2022-JP, GB18030 dan KOI8-R.
• Susun Atur Automatik DPI Tinggi (100%–250%) Merentasi 28 Bahasa:
  Kotak dialog Cari/Ganti (Ctrl+F / Ctrl+H) dan Pergi ke Baris (Ctrl+G) melaraskan lebar butang dan tetingkap secara dinamik mengikut panjang teks dan DPI.""",
        "image_captions": [
            "Mod Cerah dengan paparan dokumen yang kemas dan jelas",
            "Mod Gelap dengan penyerlahan sintaks kod yang terang",
            "Skrin Bantuan F1 dengan senarai lengkap pintasan papan kekunci"
        ],
        "prefix": "ms"
    }
}
METADATA_LANGS = METADATA_10_LANGS
