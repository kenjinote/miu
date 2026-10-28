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
        "releaseNotes": """miu v1.0.41 更新日誌
🎯 100% 原版相容 Pele 圖表引擎、預覽與編輯器核心極速最佳化及全方位安全加固

・100% 相容原版 Pele 的 C++ 原生圖表引擎（官方 522/522 範例完全一致）：
  1:1 完整移植全部 33 種圖表、55 種節點形狀、分層與複合佈局演算法及區塊箭頭語法，輸出與 pele.run 完全一致的 SVG，並保持 100% 離線與原生 LaTeX 數學公式支援。
・預覽與編輯器核心大幅提速（最高提速 319 倍）：
  引入 Mermaid/MathML 快取與 DOM 增量更新（即時預覽提速 9.8 倍）、O(1) UTF-16 偏移映射與 Direct2D 資源快取（提速 282 倍）、單行編輯增量索引更新（提速 319 倍）及 SIMD 括號配對。
・多層安全加固：
  強化 WebView2 與嚴格 CSP 政策、SVG/Markdown/公式安全過濾、DLL 預載入防護、暫存檔獨佔鎖定及 CFG 與 CET 影子堆疊保護。""",
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
        "releaseNotes": """miu v1.0.41 Versionshinweise
🎯 100 % kompatible C++-Pele-Diagramm-Engine, bis zu 319× höheres Tempo & Sicherheits-Hardening

• 100 % Upstream-kompatible C++-Pele-Diagramm-Engine (522/522 Corpus-Tests exakt identisch):
  Vollständige 1:1-Portierung aller 33 Diagrammtypen, 55 Knotenformen, Layout-Algorithmen und Blockpfeile nach C++17 – liefert exakt dieselbe SVG-Ausgabe wie pele.run (100 % offline inkl. LaTeX-MathML).
• Massive Beschleunigung für Vorschau & Editor-Kern (bis zu 319× schneller):
  Mermaid/MathML-Memoization & DOM-Diff-Updates (bis zu 9,8× schnellere Live-Vorschau), O(1)-UTF-16-Offsets & Direct2D-Ressourcen-Cache (bis zu 282× schneller), inkrementelle Zeilenindex-Updates (bis zu 319× schneller) und SIMD-Klammernsuche.
• Mehrschichtige Sicherheits-Härtung:
  Gehärtete WebView2- & CSP-Richtlinien, SVG/Markdown/MathML-Bereinigung, DLL-Hijacking-Schutz, exklusives Dateisperren sowie CFG (/guard:cf) & CET Shadow Stack (/CETCOMPAT).""",
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
        "releaseNotes": """Notes de version miu v1.0.41
🎯 Moteur de diagrammes C++ 100 % compatible Pele, accélération jusqu'à 319× et sécurité renforcée

• Moteur C++ 100 % compatible avec Pele officiel (522/522 tests officiels identiques à l'octet près) :
  Portage C++17 1:1 des 33 types de diagrammes, 55 formes, moteurs de mise en page et flèches blocs, produisant un SVG strictement identique à pele.run tout en restant 100 % hors ligne avec support LaTeX MathML.
• Accélération majeure de l'aperçu et du cœur de l'éditeur (jusqu'à 319× plus rapide) :
  Mémoïsation Mermaid/MathML et mise à jour DOM différentielle (aperçu 9,8× plus rapide), table UTF-16 O(1) et cache Direct2D (282× plus rapide), indexation incrémentale des lignes (319× plus rapide) et recherche SIMD des parenthèses.
• Sécurité multicouche renforcée :
  Durcissement WebView2 et CSP stricte, assainissement SVG/Markdown/MathML, protection contre le détournement de DLL, verrouillage exclusif des fichiers et protections CFG + CET Shadow Stack.""",
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
        "releaseNotes": """Notas de la versión miu v1.0.41
🎯 Motor C++ 100 % compatible con Pele, aceleración de hasta 319× y refuerzo de seguridad

• Motor de diagramas C++ 100 % compatible con Pele oficial (522/522 casos idénticos byte a byte):
  Portabilidad C++17 1:1 de los 33 tipos de diagramas, 55 formas de nodos, algoritmos de diseño y flechas de bloque, generando un SVG idéntico a pele.run 100 % sin conexión con soporte matemático LaTeX.
• Gran aceleración de vista previa y núcleo del editor (hasta 319× más rápido):
  Memoización Mermaid/MathML y actualización diferencial del DOM (vista previa 9,8× más rápida), mapeo UTF-16 O(1) y caché Direct2D (282× más rápido), actualización incremental de líneas (319× más rápida) y búsqueda SIMD de paréntesis.
• Seguridad multicapa reforzada:
  Endurecimiento de WebView2 y CSP estricta, saneamiento SVG/Markdown/MathML, protección contra secuestro de DLL, bloqueo exclusivo de archivos y CFG + CET Shadow Stack.""",
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
        "releaseNotes": """Notas de lançamento do miu v1.0.41
🎯 Motor C++ 100% compatível com Pele, aceleração de até 319× e reforço de segurança

• Motor de diagramas C++ 100% compatível com o Pele oficial (522/522 exemplos idênticos byte a byte):
  Portabilidade C++17 1:1 de todos os 33 tipos de diagramas, 55 formas, algoritmos de layout e setas em bloco, gerando SVG idêntico ao pele.run 100% offline com suporte a fórmulas LaTeX.
• Grande aceleração da pré-visualização e do editor (até 319× mais rápido):
  Memoização Mermaid/MathML e atualização diferencial do DOM (pré-visualização 9,8× mais rápida), tabela UTF-16 O(1) e cache Direct2D (282× mais rápido), atualização incremental de linhas (319× mais rápida) e busca SIMD de parênteses.
• Segurança multicamada reforçada:
  Endurecimento do WebView2 e CSP estrita, sanitização de SVG/Markdown/MathML, proteção contra sequestro de DLL, bloqueio exclusivo de arquivos e CFG + CET Shadow Stack.""",
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
        "releaseNotes": """Примечания к выпуску miu v1.0.41
🎯 100% совместимый с Pele движок диаграмм C++, ускорение до 319 раз и усиление безопасности

• 100% совместимый с оригинальным Pele нативный C++ движок (522/522 тестов совпадают побайтово):
  Полный перенос 1:1 на C++17 всех 33 типов диаграмм, 55 фигур узлов, алгоритмов компоновки и блочных стрелок — идентичный pele.run вывод SVG на 100% офлайн с поддержкой формул LaTeX.
• Масштабное ускорение предпросмотра и ядра редактора (до 319 раз быстрее):
  Мемоизация Mermaid/MathML и инкрементальное обновление DOM (предпросмотр до 9,8× быстрее), таблица смещений UTF-16 за O(1) и кэш ресурсов Direct2D (до 282× быстрее), инкрементальное обновление индексов строк (до 319× быстрее) и SIMD-поиск скобок.
• Многоуровневая защита безопасности:
  Усиление WebView2 и строгая политика CSP, санитизация SVG/Markdown/MathML, защита от подмены DLL, эксклюзивная блокировка временных файлов, а также защита CFG (/guard:cf) и CET Shadow Stack.""",
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
        "releaseNotes": """Note di rilascio di miu v1.0.41
🎯 Motore C++ 100% compatibile con Pele, velocità fino a 319× superiore e sicurezza potenziata

• Motore di diagrammi C++ 100% compatibile con Pele ufficiale (522/522 test identici byte per byte):
  Porting C++17 1:1 di tutti i 33 tipi di diagrammi, 55 forme, algoritmi di layout e frecce a blocchi, con output SVG identico a pele.run, 100% offline e con supporto LaTeX MathML.
• Enorme accelerazione di anteprima ed editor (fino a 319× più veloce):
  Memoizzazione Mermaid/MathML e aggiornamento DOM differenziale (anteprima fino a 9,8× più veloce), mappa UTF-16 O(1) e cache Direct2D (282× più veloce), indici di riga incrementali (319× più veloci) e ricerca parentesi SIMD.
• Sicurezza multilivello potenziata:
  Hardening WebView2 e CSP rigorosa, sanitizzazione SVG/Markdown/MathML, protezione DLL hijacking, blocco file esclusivo e protezioni CFG + CET Shadow Stack.""",
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
        "releaseNotes": """Informacje o wersji miu v1.0.41
🎯 Silnik C++ w 100% zgodny z Pele, przyspieszenie do 319× oraz wielowarstwowe zabezpieczenia

• Natywny silnik diagramów C++ w 100% zgodny z oryginalnym Pele (522/522 testów identycznych co do bajtu):
  Pełny port C++17 1:1 wszystkich 33 typów diagramów, 55 kształtów, algorytmów układu i strzałek blokowych — generuje SVG identyczny z pele.run w 100% offline z obsługą wzorów LaTeX.
• Ogromne przyspieszenie podglądu i rdzenia edytora (do 319× szybciej):
  Memoizacja Mermaid/MathML i różnicowa aktualizacja DOM (podgląd do 9,8× szybszy), mapowanie UTF-16 O(1) i pamięć podręczna Direct2D (do 282× szybciej), przyrostowa aktualizacja wierszy (do 319× szybciej) oraz wyszukiwanie nawiasów SIMD.
• Wielowarstwowe wzmocnienie bezpieczeństwa:
  Utwardzenie WebView2 i ścisłe CSP, sanityzacja SVG/Markdown/MathML, ochrona przed przejęciem DLL, wyłączne blokowanie plików oraz ochrona CFG i CET Shadow Stack.""",
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
        "releaseNotes": """Releaseopmerkingen miu v1.0.41
🎯 100% Pele-compatibele C++-diagram-engine, tot 319× sneller & uitgebreide beveiliging

• 100% met originele Pele compatibele C++-diagram-engine (522/522 tests byte-voor-byte identiek):
  Volledige 1:1 C++17-port van alle 33 diagramtypen, 55 vormen, lay-out-engines en blokpijlen — levert exact dezelfde SVG-uitvoer als pele.run, 100% offline met LaTeX MathML-ondersteuning.
• Enorme snelheidswinst voor voorbeeldweergave & editor (tot 319× sneller):
  Mermaid/MathML-memoisatie en DOM-diff-updates (tot 9,8× snellere live-weergave), O(1) UTF-16-tabel en Direct2D-cache (tot 282× sneller), incrementele regelindex-updates (tot 319× sneller) en SIMD-haakjeszoeken.
• Meerlaagse beveiligingsverbeteringen:
  Verscherpte WebView2- & CSP-regels, SVG/Markdown/MathML-sanitisatie, DLL-hijacking-bescherming, exclusieve bestandsvergrendeling en CFG + CET Shadow Stack.""",
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
        "releaseNotes": """miu v1.0.41 Sürüm Notları
🎯 %100 Pele Uyumlu C++ Diyagram Motoru, 319 Kata Kadar Hız Artışı ve Çok Katmanlı Güvenlik

• %100 Orijinal Pele Uyumlu C++ Diyagram Motoru (522/522 Resmi Test İle Birebir Aynı):
  33 diyagram türü, 55 düğüm şekli, yerleşim algoritmaları ve blok okların 1:1 C++17 portu sayesinde pele.run ile bayt-bayt aynı SVG çıktısını %100 çevrimdışı ve LaTeX matematik desteğiyle üretir.
• Önizleme ve Düzenleyici Çekirdeğinde Dev Hızlandırma (319 Kata Kadar Daha Hızlı):
  Mermaid/MathML önbellekleme ve DOM fark güncellemesi (9,8× daha hızlı canlı önizleme), O(1) UTF-16 tablosu ve Direct2D önbelleği (282× daha hızlı), artımlı satır dizini güncellemesi (319× daha hızlı) ve SIMD parantez eşleştirme.
• Çok Katmanlı Güvenlik Güçlendirmesi:
  Güçlendirilmiş WebView2 ve sıkı CSP, SVG/Markdown/MathML temizleme, DLL ele geçirme koruması, özel dosya kilitleme ve CFG + CET Shadow Stack koruması.""",
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
        "releaseNotes": """Ghi chú phát hành miu v1.0.41
🎯 Nhân C++ tương thích 100% với Pele gốc, tăng tốc tới 319 lần & tăng cường bảo mật đa lớp

• Nhân sơ đồ C++ tương thích 100% với Pele gốc (khớp tuyệt đối 522/522 mẫu thử nghiệm):
  Chuyển đổi 1:1 sang C++17 toàn bộ 33 loại sơ đồ, 55 hình dạng nút, thuật toán bố cục và mũi tên khối — xuất SVG giống hệt pele.run từng byte, hoạt động 100% ngoại tuyến kèm hỗ trợ công thức LaTeX.
• Tăng tốc vượt trội cho Xem trước & Lõi trình soạn thảo (nhanh hơn tới 319 lần):
  Bộ nhớ đệm Mermaid/MathML và cập nhật DOM vi sai (xem trước nhanh hơn 9,8 lần), bảng ánh xạ UTF-16 O(1) & bộ đệm Direct2D (nhanh hơn 282 lần), cập nhật chỉ mục dòng tăng dần (nhanh hơn 319 lần) và khớp ngoặc SIMD.
• Gia cố bảo mật đa lớp:
  Siết chặt WebView2 & CSP nghiêm ngặt, làm sạch SVG/Markdown/MathML, chống chiếm quyền DLL, khóa tệp độc quyền cùng bảo vệ CFG và CET Shadow Stack.""",
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
        "releaseNotes": """Примітки до випуску miu v1.0.41
🎯 На 100% сумісний із Pele рушій діаграм C++, прискорення до 319 разів і посилення безпеки

• На 100% сумісний з оригінальним Pele нативний C++ рушій (522/522 тестів збігаються побайтово):
  Повне перенесення 1:1 на C++17 усіх 33 типів діаграм, 55 фігур вузлів, алгоритмів компонування та блокових стрілок — ідентичний pele.run вивід SVG на 100% офлайн із підтримкою формул LaTeX.
• Масштабне прискорення перегляду та ядра редактора (до 319 разів швидше):
  Мемоізація Mermaid/MathML та інкрементальне оновлення DOM (перегляд до 9,8× швидше), таблиця зміщень UTF-16 за O(1) і кеш Direct2D (до 282× швидше), інкрементальне оновлення рядків (до 319× швидше) і SIMD-пошук дужок.
• Багаторівневий захист безпеки:
  Посилення WebView2 і сувора політика CSP, санітизація SVG/Markdown/MathML, захист від підміни DLL, ексклюзивне блокування файлів, а також захист CFG і CET Shadow Stack.""",
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
        "releaseNotes": """Catatan Rilis miu v1.0.41
🎯 Mesin Diagram C++ 100% Kompatibel Pele, Kecepatan Hingga 319× Lebih Cepat & Keamanan Berlapis

• Mesin Diagram C++ 100% Kompatibel dengan Pele Resmi (522/522 Uji Korpus Identik Byte-demi-Byte):
  Porting C++17 1:1 untuk seluruh 33 jenis diagram, 55 bentuk node, algoritma tata letak, dan panah blok — menghasilkan SVG yang identik dengan pele.run secara 100% offline dengan dukungan rumus LaTeX.
• Peningkatan Kecepatan Pratinjau & Inti Editor (Hingga 319× Lebih Cepat):
  Memoisasi Mermaid/MathML dan pembaruan diff DOM (pratinjau 9,8× lebih cepat), tabel UTF-16 O(1) & cache Direct2D (282× lebih cepat), pembaruan indeks baris inkremental (319× lebih cepat), serta pencarian kurung SIMD.
• Penguatan Keamanan Berlapis:
  Pengamanan WebView2 & kebijakan CSP ketat, sanitasi SVG/Markdown/MathML, perlindungan pembajakan DLL, penguncian berkas eksklusif, serta perlindungan CFG & CET Shadow Stack.""",
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
        "releaseNotes": """Poznámky k verzi miu v1.0.41
🎯 100% kompatibilní C++ engine Pele, zrychlení až 319× a vícevrstvé zabezpečení

• 100% kompatibilní nativní C++ engine diagramů Pele (522/522 testů shodných bajt po bajtu):
  Kompletní port 1:1 do C++17 všech 33 typů diagramů, 55 tvarů uzlů, rozvrhovacích algoritmů a blokových šipek — generuje SVG identické s pele.run 100% offline s podporou LaTeX vzorců.
• Masivní zrychlení náhledu a jádra editoru (až 319× rychlejší):
  Memoizace Mermaid/MathML a rozdílové aktualizace DOM (náhled až 9,8× rychlejší), tabulka UTF-16 O(1) a mezipaměť Direct2D (až 282× rychlejší), přírůstková aktualizace řádků (až 319× rychlejší) a SIMD hledání závorek.
• Vícevrstvé posílení bezpečnosti:
  Zabezpečení WebView2 a přísná politika CSP, sanitizace SVG/Markdown/MathML, ochrana proti podvržení DLL, výhradní zamykání souborů a ochrana CFG + CET Shadow Stack.""",
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
        "releaseNotes": """Versionsinformation för miu v1.0.41
🎯 100 % Pele-kompatibel C++-diagrammotor, upp till 319× snabbare & flerskiktat säkerhetsskydd

• 100 % kompatibel C++-diagrammotor med officiella Pele (522/522 tester identiska byte för byte):
  Fullständig 1:1 C++17-portning av alla 33 diagramtyper, 55 nodformer, layoutmotorer och blockpilar — ger exakt samma SVG-utdata som pele.run 100 % offline med LaTeX MathML-stöd.
• Massiv hastighetsökning för förhandsgranskning och redigerare (upp till 319× snabbare):
  Mermaid/MathML-memoisering och DOM-diff-uppdateringar (upp till 9,8× snabbare livevy), O(1) UTF-16-tabell & Direct2D-cache (upp till 282× snabbare), inkrementella radindexuppdateringar (upp till 319× snabbare) och SIMD-parentessökning.
• Flerskiktad säkerhetsförstärkning:
  Härdad WebView2 och strikt CSP, sanering av SVG/Markdown/MathML, skydd mot DLL-kapning, exklusiv fillåsning samt CFG och CET Shadow Stack.""",
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
        "releaseNotes": """บันทึกประจำรุ่น miu v1.0.41
🎯 เอนจินแผนภาพ C++ ที่เข้ากันได้กับ Pele ต้นฉบับ 100%, เร็วขึ้นสูงสุด 319 เท่า และยกระดับความปลอดภัยหลายชั้น

• เอนจินแผนภาพ C++ ที่เข้ากันได้กับ Pele ต้นฉบับ 100% (ตรงกันทุกไบต์ 522/522 ชุดทดสอบ):
  พอร์ตโค้ด 1:1 สู่ C++17 สำหรับแผนภาพทั้ง 33 รูปแบบ, รูปทรงโหนด 55 แบบ, อัลกอริทึมจัดวาง และลูกศรบล็อก ให้ผลลัพธ์ SVG เหมือน pele.run ทุกประการโดยทำงานออฟไลน์ 100% พร้อมรองรับสมการ LaTeX
• เร่งความเร็วพรีวิวและแกนหลักของเอดิเตอร์ครั้งใหญ่ (เร็วขึ้นสูงสุด 319 เท่า):
  เพิ่มแคช Mermaid/MathML และอัปเดต DOM เฉพาะส่วนที่เปลี่ยน (พรีวิวเร็วขึ้น 9.8 เท่า), ตาราง UTF-16 แบบ O(1) และแคช Direct2D (เร็วขึ้น 282 เท่า), อัปเดตดัชนีบรรทัดแบบส่วนเพิ่ม (เร็วขึ้น 319 เท่า) และค้นหาวงเล็บด้วย SIMD
• การป้องกันความปลอดภัยหลายชั้น:
  ยกระดับความเข้มงวดของ WebView2 และ CSP, คัดกรองความปลอดภัย SVG/Markdown/MathML, ป้องกันการปลอมแปลง DLL, ล็อกไฟล์ชั่วคราวแบบเอกสิทธิ์ และเปิดใช้งาน CFG + CET Shadow Stack""",
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
        "releaseNotes": """miu v1.0.41 रिलीज़ नोट्स
🎯 100% Pele-संगत C++ डायग्राम इंजन, 319 गुना तक तेज़ प्रदर्शन और बहु-स्तरीय सुरक्षा

• मूल Pele के साथ 100% संगत C++ डायग्राम इंजन (522/522 आधिकारिक परीक्षण बाइट-दर-बाइट समान):
  सभी 33 डायग्राम प्रकारों, 55 नोड आकृतियों, लेआउट एल्गोरिदम और ब्लॉक तीरों का 1:1 C++17 पोर्ट — LaTeX गणित समर्थन के साथ 100% ऑफ़लाइन रहते हुए pele.run के समान सटीक SVG आउटपुट देता है।
• प्रीव्यू और एडिटर कोर में भारी गति वृद्धि (319 गुना तक तेज़):
  Mermaid/MathML मेमोइज़ेशन और DOM डिफ़ अपडेट (9.8× तेज़ लाइव प्रीव्यू), O(1) UTF-16 ऑफ़सेट मैप और Direct2D कैश (282× तेज़), इंक्रीमेंटल लाइन इंडेक्स अपडेट (319× तेज़) और SIMD ब्रैकेट मैचिंग।
• बहु-स्तरीय सुरक्षा सुदृढ़ीकरण:
  कठोर WebView2 और सख्त CSP नीतियां, SVG/Markdown/MathML सैनिटाइजेशन, DLL हाइजैकिंग सुरक्षा, विशेष फ़ाइल लॉकिंग और CFG + CET Shadow Stack सुरक्षा।""",
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
        "releaseNotes": """Note de lansare miu v1.0.41
🎯 Motor C++ 100% compatibil cu Pele, accelerare de până la 319× și securitate multistrat

• Motor de diagrame C++ 100% compatibil cu Pele oficial (522/522 teste identice octet cu octet):
  Portare C++17 1:1 a tuturor celor 33 de tipuri de diagrame, 55 de forme, algoritmi de așezare în pagină și săgeți bloc — generând SVG identic cu pele.run, 100% offline, cu suport LaTeX MathML.
• Accelerare masivă pentru previzualizare și nucleul editorului (de până la 319× mai rapid):
  Memoizare Mermaid/MathML și actualizare diferențială DOM (previzualizare de 9,8× mai rapidă), mapare UTF-16 O(1) și cache Direct2D (de 282× mai rapid), actualizare incrementală a rândurilor (de 319× mai rapidă) și căutare SIMD a parantezelor.
• Securitate multistrat consolidată:
  Politici WebView2 și CSP stricte, sanitizare SVG/Markdown/MathML, protecție împotriva deturnării DLL, blocare exclusivă a fișierelor și protecții CFG + CET Shadow Stack.""",
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
        "releaseNotes": """miu v1.0.41 kiadási megjegyzések
🎯 100%-ban Pele-kompatibilis C++ diagrammotor, akár 319× nagyobb sebesség és többrétegű biztonság

• Az eredeti Pele-lel 100%-ban kompatibilis natív C++ diagrammotor (522/522 teszt bájtról bájtra azonos):
  Mind a 33 diagramtípus, 55 alakzat, elrendezési algoritmus és blokknyíl 1:1 C++17 portja — a pele.run-nal teljesen azonos SVG-kimenetet biztosít 100% offline módban, LaTeX képlettámogatással.
• Hatalmas gyorsulás az előnézetben és a szerkesztőmotorban (akár 319× gyorsabb):
  Mermaid/MathML gyorsítótárazás és DOM-különbségfrissítés (9,8× gyorsabb élő előnézet), O(1) UTF-16 tábla és Direct2D gyorsítótár (282× gyorsabb), növekményes sorindex-frissítés (319× gyorsabb) és SIMD zárójelkeresés.
• Többrétegű biztonsági megerősítés:
  Megerősített WebView2 és szigorú CSP, SVG/Markdown/MathML szűrés, DLL-eltérítés elleni védelem, kizárólagos fájlzárolás, valamint CFG és CET Shadow Stack védelem.""",
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
        "releaseNotes": """Σημειώσεις έκδοσης miu v1.0.41
🎯 Μηχανή C++ 100% συμβατή με το Pele, επιτάχυνση έως 319× και πολυεπίπεδη ασφάλεια

• Εγγενής μηχανή διαγραμμάτων C++ 100% συμβατή με το επίσημο Pele (522/522 δοκιμές πανομοιότυπες byte προς byte):
  Πλήρης μεταφορά 1:1 σε C++17 και των 33 τύπων διαγραμμάτων, 55 σχημάτων, αλγορίθμων διάταξης και βελών μπλοκ — παράγει SVG πανομοιότυπο με το pele.run 100% εκτός σύνδεσης με υποστήριξη LaTeX MathML.
• Τεράστια επιτάχυνση προεπισκόπησης και πυρήνα επεξεργαστή (έως 319× ταχύτερα):
  Απομνημόνευση Mermaid/MathML και διαφορική ενημέρωση DOM (προεπισκόπηση 9,8× ταχύτερη), πίνακας UTF-16 O(1) & κρυφή μνήμη Direct2D (282× ταχύτερα), αυξητική ενημέρωση γραμμών (319× ταχύτερα) και αναζήτηση παρενθέσεων SIMD.
• Πολυεπίπεδη θωράκιση ασφαλείας:
  Ενίσχυση WebView2 και αυστηρό CSP, εξυγίανση SVG/Markdown/MathML, προστασία από DLL hijacking, αποκλειστικό κλείδωμα αρχείων και προστασία CFG + CET Shadow Stack.""",
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
        "releaseNotes": """Udgivelsesnoter for miu v1.0.41
🎯 100 % Pele-kompatibel C++-diagrammotor, op til 319× hurtigere & flerlaget sikkerhed

• 100 % kompatibel C++-diagrammotor med officielle Pele (522/522 tests identiske byte for byte):
  Fuld 1:1 C++17-portering af alle 33 diagramtyper, 55 figurformer, layout-algoritmer og blokpile — leverer nøjagtig samme SVG-output som pele.run 100 % offline med LaTeX MathML-understøttelse.
• Massiv hastighedsforøgelse i forhåndsvisning og editor-kerne (op til 319× hurtigere):
  Mermaid/MathML-memoisering og DOM-diff-opdatering (op til 9,8× hurtigere live-visning), O(1) UTF-16-tabel og Direct2D-cache (op til 282× hurtigere), inkrementel linjeindeksopdatering (op til 319× hurtigere) samt SIMD-parentessøgning.
• Flerlaget sikkerhedsforstærkning:
  Hærdet WebView2 og striks CSP, SVG/Markdown/MathML-rensning, beskyttelse mod DLL-kapring, eksklusiv fillåsning samt CFG og CET Shadow Stack.""",
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
        "releaseNotes": """miu v1.0.41 julkaisutiedot
🎯 100 % Pele-yhteensopiva C++-kaaviomoottori, jopa 319× nopeampi suorituskyky ja monikerroksinen tietoturva

• 100 % virallisen Pelen kanssa yhteensopiva C++-kaaviomoottori (522/522 testiä tavulleen identtisiä):
  Täydellinen 1:1 C++17-käännös kaikista 33 kaaviotyypistä, 55 muodosta, asettelualgoritmeista ja lohkonuolista — tuottaa täsmälleen saman SVG-tulosteen kuin pele.run 100 % offline-tilassa LaTeX MathML -tuella.
• Huima nopeutus esikatseluun ja editorin ytimeen (jopa 319× nopeampi):
  Mermaid/MathML-välimuisti ja DOM-eropäivitys (jopa 9,8× nopeampi esikatselu), O(1) UTF-16-taulukko ja Direct2D-välimuisti (jopa 282× nopeampi), inkrementaalinen rivi-indeksin päivitys (jopa 319× nopeampi) sekä SIMD-sulkumerkkihaku.
• Monikerroksinen tietoturvavahvistus:
  Kovennettu WebView2 ja tiukka CSP, SVG/Markdown/MathML-suodatus, DLL-kaappaussuoja, tiedostojen yksinoikeuslukitus sekä CFG- ja CET Shadow Stack -suojaus.""",
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
        "releaseNotes": """Utgivelsesnotater for miu v1.0.41
🎯 100 % Pele-kompatibel C++-diagrammotor, opptil 319× raskere og flerlags sikkerhet

• 100 % kompatibel C++-diagrammotor med offisielle Pele (522/522 tester identiske byte for byte):
  Fullstendig 1:1 C++17-portering av alle 33 diagramtyper, 55 nodeformer, layoutmotorer og blokkpiler — gir nøyaktig samme SVG-utdata som pele.run 100 % offline med LaTeX MathML-støtte.
• Kraftig hastighetsøkning for forhåndsvisning og redigeringskjerne (opptil 319× raskere):
  Mermaid/MathML-memoisering og DOM-diff-oppdatering (opptil 9,8× raskere sanntidsvisning), O(1) UTF-16-tabell og Direct2D-hurtigbuffer (opptil 282× raskere), inkrementell linjeindeksoppdatering (opptil 319× raskere) og SIMD-parentessøk.
• Flerlags sikkerhetsforsterkning:
  Herdet WebView2 og streng CSP, rensing av SVG/Markdown/MathML, beskyttelse mot DLL-kapring, eksklusiv fillåsing samt CFG og CET Shadow Stack.""",
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
        "releaseNotes": """Nota Keluaran miu v1.0.41
🎯 Enjin Rajah C++ 100% Serasi Pele, Kelajuan Sehingga 319× Lebih Pantas & Keselamatan Berlapis

• Enjin Rajah C++ 100% Serasi dengan Pele Rasmi (522/522 Ujian Korpus Sepadan Bait-demi-Bait):
  Port C++17 1:1 lengkap bagi kesemua 33 jenis rajah, 55 bentuk nod, enjin susun atur dan anak panah blok — menghasilkan output SVG yang sepadan sepenuhnya dengan pele.run secara 100% luar talian dengan sokongan formula LaTeX.
• Pecutan Hebat bagi Pratonton & Teras Penyunting (Sehingga 319× Lebih Pantas):
  Memoisasi Mermaid/MathML dan kemas kini diff DOM (pratonton 9.8× lebih pantas), jadual UTF-16 O(1) & cache Direct2D (282× lebih pantas), kemas kini indeks baris tokokan (319× lebih pantas) serta padanan kurungan SIMD.
• Pengukuhan Keselamatan Berlapis:
  Tetapan WebView2 & dasar CSP yang ketat, sanitasi SVG/Markdown/MathML, perlindungan rampasan DLL, penguncian fail eksklusif serta perlindungan CFG & CET Shadow Stack.""",
        "image_captions": [
            "Mod Cerah dengan paparan dokumen yang kemas dan jelas",
            "Mod Gelap dengan penyerlahan sintaks kod yang terang",
            "Skrin Bantuan F1 dengan senarai lengkap pintasan papan kekunci"
        ],
        "prefix": "ms"
    }
}
METADATA_LANGS = METADATA_10_LANGS
