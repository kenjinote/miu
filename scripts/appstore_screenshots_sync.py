#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Generate high-quality localized App Store screenshots for miu (jp.hack.miu)
across macOS (APP_DESKTOP), iPhone (APP_IPHONE_67 / APP_IPHONE_65), and
iPad (APP_IPAD_PRO_3GEN_129), and synchronize them to App Store Connect
for all 34 supported locales (deleting old screenshots and uploading new ones).
"""

import os
import re
import sys
import time
import json
import hashlib
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MACOS_DIR = os.path.join(REPO_ROOT, "macOS")
ASSETS_DIR = os.path.join(MACOS_DIR, "store_assets")

BUNDLE_ID = "jp.hack.miu"
TARGET_VERSION = "1.0.44"
BASE_URL = "https://api.appstoreconnect.apple.com/v1"

# ============================================================================
# Color Palettes (Mesh Gradient Backgrounds)
# ============================================================================

DARK_COBALT = (
    (16, 18, 44),
    [
        (0.28, 0.72, 0.38, 0.45, (38, 68, 210, 235)),
        (0.68, 0.22, 0.42, 0.48, (32, 92, 235, 220)),
        (0.82, 0.78, 0.35, 0.40, (88, 38, 175, 190)),
        (0.15, 0.20, 0.28, 0.32, (24, 32, 82, 210)),
    ],
)

DARK_EMERALD_COBALT = (
    (14, 19, 42),
    [
        (0.26, 0.72, 0.38, 0.45, (28, 82, 218, 235)),
        (0.68, 0.22, 0.42, 0.48, (16, 138, 212, 220)),
        (0.82, 0.78, 0.35, 0.40, (78, 42, 185, 195)),
        (0.15, 0.20, 0.28, 0.32, (20, 42, 96, 210)),
    ],
)

LIGHT_ROYAL_INDIGO = (
    (194, 208, 238),
    [
        (0.64, 0.20, 0.38, 0.45, (96, 126, 214, 230)),
        (0.24, 0.76, 0.36, 0.44, (116, 142, 212, 220)),
        (0.50, 0.54, 0.42, 0.48, (242, 246, 255, 240)),
        (0.14, 0.24, 0.32, 0.38, (176, 194, 232, 220)),
    ],
)

LIGHT_AZURE = (
    (192, 212, 236),
    [
        (0.58, 0.20, 0.40, 0.44, (86, 138, 206, 225)),
        (0.22, 0.75, 0.35, 0.42, (124, 152, 208, 220)),
        (0.52, 0.58, 0.44, 0.48, (242, 247, 255, 245)),
    ],
)

DARK_VIOLET_CYAN = (
    (15, 17, 40),
    [
        (0.24, 0.68, 0.38, 0.46, (68, 42, 196, 230)),
        (0.65, 0.20, 0.40, 0.44, (14, 135, 215, 210)),
        (0.75, 0.80, 0.36, 0.42, (36, 62, 190, 215)),
    ],
)

# ============================================================================
# 28 Language Copy Decks (Apple-native shortcuts: ⌥ + P, ⌘D, ⌥⌘F)
# ============================================================================

LANG_DECKS = {
    "ja": {
        "lproj": "ja.lproj",
        "s1": (["リアルタイム", "プレビューで", "思考を止めない"], ["Markdown（テーブル・アラート・タスクリスト）を", "すりガラスの美しいダークモードで即座に分割描画"]),
        "s2": (["Mermaid 図表も", "書いた瞬間に", "美しくグラフ化"], ["シーケンス図・フローチャート・グラフ等 33 種の図表を", "ダーク / ライト自動連動の高速 SVG でリアルタイム描画"]),
        "s3": (["LaTeX 数式も", "オフラインで", "瞬時に美しく描画"], ["Markdown の $$ 数式や Mermaid 図表内の数式に対応", "外部通信不要のネイティブ数式エンジンでリアルタイム表示"]),
        "s4": (["正規表現による", "強力な検索と", "一括置換"], ["キャプチャグループ ($1, $2) を活用した高度な置換や", "大規模ログの整形も軽快に処理"]),
        "s5": (["49言語ハイライトと", "マルチカーソルで", "一括高速編集"], ["⌘D による連続選択や ⌥+ドラッグ矩形選択で", "日々のコーディングと繰り返し作業を数秒で完結"]),
    },
    "en": {
        "lproj": "en.lproj",
        "s1": (["Instant Live", "Preview for", "Markdown & GFM"], ["Render GFM tables, alerts, and task lists side-by-side", "in an elegant frosted-glass Dark Mode window"]),
        "s2": (["Turn Mermaid", "Syntax into Crisp", "Live Diagrams"], ["Render 33 diagram types—sequence, flowcharts & XY charts—", "as instant theme-aware SVGs right as you type"]),
        "s3": (["Crisp LaTeX Math", "Rendered Live", "100% Offline"], ["Write $...$ and $$...$$ equations in Markdown & Mermaid", "rendered instantly via the built-in native MathML engine"]),
        "s4": (["Powerful Regex", "Search & Batch", "Replacement"], ["Reshape massive logs and codebases effortlessly", "using regex capture groups ($1, $2) with ⌥⌘F"]),
        "s5": (["49 Languages &", "Multi-Cursor", "Fast Editing"], ["Select recurring words with ⌘D or ⌥+Drag block", "selection to finish repetitive code edits in seconds"]),
    },
    "zh_cn": {
        "lproj": "zh-Hans.lproj",
        "s1": (["实时分屏预览", "让创作与思考", "一气呵成"], ["在优雅的磨砂玻璃深色模式下，即时渲染", "Markdown 表格、GFM 提示框与任务列表"]),
        "s2": (["Mermaid 图表", "随写随看", "即刻优雅成图"], ["支持时序图、流程图、XY 图表等 33 种 Mermaid 语法", "深浅色主题自动联动，毫秒级生成高清矢量 SVG"]),
        "s3": (["LaTeX 数学公式", "完全离线", "毫秒级高清渲染"], ["全面支持 Markdown 行内/块级公式与 Mermaid 图表内公式", "内置原生数学排版引擎，无需联网即可实时预览"]),
        "s4": (["强大的正则表达式", "精准查找与", "批量替换"], ["支持捕获组 ($1, $2) 替换与实时高亮匹配", "轻松处理海量服务器日志与数据格式化"]),
        "s5": (["49种语言高亮与", "多光标同步编辑", "瞬间完成"], ["支持 ⌘D 连续选中相同词或 ⌥+拖拽列块选择", "让繁琐的批量代码修改在数秒内完成"]),
    },
    "zh_tw": {
        "lproj": "zh-Hant.lproj",
        "s1": (["即時分割預覽", "讓寫作與思考", "行雲流水"], ["在細膩的毛玻璃深色模式下，即時渲染", "Markdown 表格、GFM 提示框與工作清單"]),
        "s2": (["Mermaid 圖表", "隨寫隨看", "瞬間優雅成圖"], ["支援循序圖、流程圖、XY 圖表等 33 種 Mermaid 語法", "深淺色主題自動連動，毫秒級繪製高畫質 SVG"]),
        "s3": (["LaTeX 數學公式", "完全離線", "毫秒級高畫質渲染"], ["全面支援 Markdown 行內/區塊公式與 Mermaid 圖表內公式", "內建原生數學排版引擎，無需連網即可即時預覽"]),
        "s4": (["強大正規表示式", "精準尋找與", "批次取代"], ["支援擷取群組 ($1, $2) 進階取代與即時標記", "輕鬆整理龐大伺服器紀錄檔與結構化資料"]),
        "s5": (["49種語法突顯與", "多游標同步編輯", "瞬間完成"], ["透過 ⌘D 連續選取或 ⌥+拖曳矩形區塊選取", "短短幾秒內完成重複性的程式碼修改"]),
    },
    "ko": {
        "lproj": "ko.lproj",
        "s1": (["실시간 분할", "미리보기로", "끊김 없는 작업"], ["세련된 반투명 글래스 다크 모드에서", "Markdown 표·알림·작업 목록을 즉시 렌더링"]),
        "s2": (["Mermaid 다이어그램", "작성하는 즉시", "아름다운 그래프로"], ["시퀀스·플로우차트·차트 등 33종 다이어그램을", "다크/라이트 테마 연동 고속 SVG로 실시간 렌더링"]),
        "s3": (["LaTeX 수식도", "오프라인에서", "즉시 선명하게 렌더링"], ["Markdown의 $$ 수식과 Mermaid 다이어그램 내 수식 완벽 지원", "외부 통신 없는 네이티브 수식 엔진으로 실시간 미리보기"]),
        "s4": (["정규식 기반의", "강력한 검색과", "일괄 바꾸기"], ["캡처 그룹($1, $2)을 활용한 정교한 치환과", "대용량 로그 정리를 빠르고 쾌적하게 처리"]),
        "s5": (["49개 언어 구문 강조", "다중 커서로", "한 번에 고속 편집"], ["⌘D 연속 선택과 ⌥+드래그 열 블록 선택으로", "반복되는 코드 수정을 단 몇 초 만에 완료"]),
    },
    "de": {
        "lproj": "de.lproj",
        "s1": (["Echtzeit-Vorschau", "für Markdown", "& GFM-Dokumente"], ["Tabellen, Hinweise und Aufgabenlisten sofort im", "eleganten Dark Mode mit Milchglas-Effekt betrachten"]),
        "s2": (["Mermaid-Diagramme", "in Echtzeit als", "elegante Grafiken"], ["33 Diagrammtypen wie Sequenz-, Fluss- und XY-Diagramme", "sofort als gestochen scharfe SVGs im Dark & Light Mode"]),
        "s3": (["LaTeX-Formeln", "100 % offline", "live gerendert"], ["Unterstützt $...$- und $$...$$-Formeln in Markdown & Mermaid", "sofort dargestellt durch die native MathML-Engine"]),
        "s4": (["Starke Regex-", "Suche & Stapel-", "Ersetzung"], ["Große Logdateien mit Capture-Gruppen ($1, $2)", "und Echtzeit-Hervorhebung mühelos umformen"]),
        "s5": (["Multi-Cursor &", "49 Sprachen", "Syntaxhervorhebung"], ["Mit ⌘D oder ⌥+Ziehen mehrere Textstellen", "markieren und in Sekunden simultan ändern"]),
    },
    "fr": {
        "lproj": "fr.lproj",
        "s1": (["Aperçu en direct", "pour Markdown", "et GFM"], ["Tableaux, alertes et listes de tâches rendus en direct", "dans un élégant mode sombre à effet verre dépoli"]),
        "s2": (["Vos diagrammes", "Mermaid tracés", "en temps réel"], ["33 types de diagrammes (séquence, flux, graphiques XY)", "rendus instantanément en SVG adapté au thème actif"]),
        "s3": (["Formules LaTeX", "rendues en direct", "100 % hors ligne"], ["Équations $...$ et $$...$$ dans Markdown et Mermaid", "affichées instantanément via le moteur MathML natif"]),
        "s4": (["Recherche et", "remplacement", "Regex puissants"], ["Transformez vos journaux et codes volumineux", "grâce aux groupes de capture ($1, $2) via ⌥⌘F"]),
        "s5": (["49 langages &", "édition rapide", "multi-curseurs"], ["Sélectionnez plusieurs occurrences avec ⌘D", "ou ⌥+glisser pour tout modifier en un instant"]),
    },
    "es": {
        "lproj": "es.lproj",
        "s1": (["Vista previa", "en vivo para", "Markdown y GFM"], ["Tablas, alertas y listas de tareas al instante", "en un elegante modo oscuro con cristal esmerilado"]),
        "s2": (["Diagramas Mermaid", "renderizados", "al instante"], ["33 tipos de diagramas: secuencia, flujo y gráficos XY", "dibujados en vivo como SVG adaptados al modo oscuro/claro"]),
        "s3": (["Fórmulas LaTeX", "en tiempo real", "100 % sin conexión"], ["Soporta ecuaciones $...$ y $$...$$ en Markdown y Mermaid", "renderizadas al instante con el motor MathML nativo"]),
        "s4": (["Búsqueda y", "reemplazo con", "Regex avanzado"], ["Formatea enormes archivos de registro fácilmente", "usando grupos de captura ($1, $2) con ⌥⌘F"]),
        "s5": (["49 lenguajes y", "edición rápida", "con multicursor"], ["Selecciona múltiples coincidencias con ⌘D", "o ⌥+arrastrar para editar todo en segundos"]),
    },
    "pt_br": {
        "lproj": "pt.lproj",
        "s1": (["Pré-visualização", "ao vivo para", "Markdown e GFM"], ["Tabelas, alertas e listas de tarefas renderizados", "no elegante Modo Escuro com efeito de vidro fosco"]),
        "s2": (["Diagramas Mermaid", "desenhados em", "tempo real"], ["33 tipos de diagramas — sequência, fluxo e gráficos XY —", "renderizados instantaneamente em SVG com tema automático"]),
        "s3": (["Fórmulas LaTeX", "em tempo real", "100% offline"], ["Suporte a equações $...$ e $$...$$ no Markdown e Mermaid", "renderizadas instantaneamente pelo motor MathML nativo"]),
        "s4": (["Busca potente", "e substituição", "com Regex"], ["Transforme grandes arquivos de log rapidamente", "usando grupos de captura ($1, $2) com ⌥⌘F"]),
        "s5": (["49 linguagens e", "edição rápida", "com multicursores"], ["Selecione múltiplas ocorrências com ⌘D ou", "⌥+arrastar para editar várias linhas de uma vez"]),
    },
    "ru": {
        "lproj": "ru.lproj",
        "s1": (["Мгновенный", "предпросмотр", "Markdown и GFM"], ["Таблицы, оповещения и списки задач в стильном", "тёмном режиме с эффектом матового стекла"]),
        "s2": (["Диаграммы Mermaid", "оживают прямо", "во время ввода"], ["33 типа диаграмм: последовательности, блок-схемы и графики", "мгновенно рендерятся в чёткий SVG под текущую тему"]),
        "s3": (["Формулы LaTeX", "в реальном времени", "100% офлайн"], ["Поддержка формул $...$ и $$...$$ в Markdown и диаграммах Mermaid", "мгновенный рендеринг встроенным нативным движком MathML"]),
        "s4": (["Мощный поиск", "и замена с", "Regex ($1, $2)"], ["Форматируйте огромные лог-файлы и исходный код", "с помощью групп захвата регулярных выражений"]),
        "s5": (["49 языков кода и", "быстрая правка", "мультикурсором"], ["Выделяйте совпадения через ⌘D или блоком", "⌥+мышь для мгновенного редактирования кода"]),
    },
    "it": {
        "lproj": "it.lproj",
        "s1": (["Anteprima live", "istantanea per", "Markdown e GFM"], ["Tabelle, avvisi ed elenchi attività renderizzati", "in modalità scura con elegante vetro satinato"]),
        "s2": (["Diagrammi Mermaid", "disegnati dal vivo", "mentre scrivi"], ["33 tipi di diagrammi tra sequenze, flussi e grafici XY", "renderizzati all'istante in SVG sincronizzati col tema"]),
        "s3": (["Formule LaTeX", "rese dal vivo", "100% offline"], ["Supporta equazioni $...$ e $$...$$ in Markdown e Mermaid", "renderizzate all'istante dal motore nativo MathML"]),
        "s4": (["Ricerca e", "sostituzione", "Regex avanzata"], ["Riformatta facilmente grandi file di log usando", "i gruppi di cattura ($1, $2) con ⌥⌘F"]),
        "s5": (["49 linguaggi e", "modifica rapida", "con multi-cursore"], ["Seleziona più occorrenze con ⌘D o ⌥+trascina", "per completare modifiche ripetitive in pochi secondi"]),
    },
    "pl": {
        "lproj": "pl.lproj",
        "s1": (["Podgląd na żywo", "dla dokumentów", "Markdown i GFM"], ["Tabele, alerty i listy zadań renderowane na żywo", "w eleganckim trybie ciemnym z efektem przydymionego szkła"]),
        "s2": (["Diagramy Mermaid", "rysowane na żywo", "podczas pisania"], ["33 typy diagramów — sekwencji, przepływu i wykresy XY —", "błyskawicznie renderowane w SVG dopasowanym do motywu"]),
        "s3": (["Wzory LaTeX", "na żywo i w 100%", "trybie offline"], ["Obsługa równań $...$ oraz $$...$$ w Markdown i Mermaid", "błyskawicznie renderowanych przez natywny silnik MathML"]),
        "s4": (["Zaawansowane", "wyszukiwanie i", "zamiana Regex"], ["Przekształcaj duże pliki logów z użyciem grup", "przechwytujących ($1, $2) w oknie ⌥⌘F"]),
        "s5": (["49 języków kodu", "i szybka edycja", "wieloma kursorami"], ["Zaznaczaj powtórzenia przez ⌘D lub blokowo", "⌥+przeciągnij, by edytować wiele linii naraz"]),
    },
    "nl": {
        "lproj": "nl.lproj",
        "s1": (["Directe live-", "weergave voor", "Markdown & GFM"], ["Tabellen, meldingen en takenlijsten direct naast elkaar", "in een stijlvolle donkere modus met matglaseffect"]),
        "s2": (["Mermaid-diagrammen", "direct live", "gevisualiseerd"], ["33 diagramtypen zoals sequentie-, stroom- en XY-grafieken", "razendsnel gerenderd als scherpe, thema-bewuste SVG's"]),
        "s3": (["LaTeX-formules", "direct live", "100% offline"], ["Ondersteunt $...$- en $$...$$-vergelijkingen in Markdown & Mermaid", "razendsnel weergegeven met de ingebouwde MathML-engine"]),
        "s4": (["Krachtig zoeken", "en vervangen", "met Regex"], ["Herstructureer grote logbestanden moeiteloos", "met behulp van capture-groepen ($1, $2) via ⌥⌘F"]),
        "s5": (["49 talen &", "meerdere regels", "met multi-cursor"], ["Selecteer herhaalde woorden met ⌘D of blokselectie", "om repetitieve aanpassingen in seconden te voltooien"]),
    },
    "tr": {
        "lproj": "tr.lproj",
        "s1": (["Markdown ve GFM", "İçin Anında", "Canlı Önizleme"], ["Tabloları, uyarıları ve görev listelerini buzlu cam", "efektli şık Koyu Modda anında yan yana görün"]),
        "s2": (["Mermaid Şemaları", "Siz Yazarken", "Anında Çizilir"], ["Akış, sıralama ve XY grafikleri dahil 33 diyagram türü", "koyu/açık temayla uyumlu keskin SVG olarak canlı çizilir"]),
        "s3": (["LaTeX Formülleri", "%100 Çevrimdışı", "Canlı Önizleme"], ["Markdown ve Mermaid içinde $...$ ve $$...$$ denklemleri", "yerleşik yerel MathML motoruyla anında net olarak çizilir"]),
        "s4": (["Güçlü Regex", "Arama ve Toplu", "Değiştirme"], ["Yakalama grupları ($1, $2) ile dev log dosyalarını", "⌥⌘F penceresinden zahmetsizce biçimlendirin"]),
        "s5": (["49 Dil Desteği ve", "Çoklu İmleç ile", "Hızlı Düzenleme"], ["⌘D veya ⌥+sürükle dikdörtgen seçimiyle", "tekrarlayan düzenlemeleri saniyeler içinde bitirin"]),
    },
    "vi": {
        "lproj": "vi.lproj",
        "s1": (["Xem trước", "trực tiếp cho", "Markdown & GFM"], ["Hiển thị bảng, hộp cảnh báo và danh sách công việc", "trong chế độ Tối hiệu ứng kính mờ sang trọng"]),
        "s2": (["Sơ đồ Mermaid", "vẽ trực tiếp", "ngay khi bạn gõ"], ["Hỗ trợ 33 loại sơ đồ tuần tự, lưu đồ và biểu đồ XY", "kết xuất tức thì thành SVG sắc nét theo giao diện Sáng/Tối"]),
        "s3": (["Công thức LaTeX", "hiển thị trực tiếp", "100% ngoại tuyến"], ["Hỗ trợ công thức $...$ và $$...$$ trong Markdown & Mermaid", "kết xuất tức thì bằng nhân MathML gốc tích hợp sẵn"]),
        "s4": (["Tìm kiếm &", "Thay thế mạnh mẽ", "bằng Regex"], ["Định dạng lại tệp nhật ký lớn dễ dàng với", "nhóm chụp ($1, $2) và tô sáng trực tiếp qua ⌥⌘F"]),
        "s5": (["49 ngôn ngữ &", "Chỉnh sửa nhanh", "với Đa con trỏ"], ["Chọn nhanh các từ lặp lại bằng ⌘D hoặc", "⌥+kéo để sửa hàng loạt dòng chỉ trong vài giây"]),
    },
    "uk": {
        "lproj": "uk.lproj",
        "s1": (["Миттєвий живий", "перегляд для", "Markdown і GFM"], ["Таблиці, сповіщення та списки завдань у стильному", "темному режимі з ефектом матового скла"]),
        "s2": (["Діаграми Mermaid", "малюються наживо", "під час введення"], ["33 типи діаграм: послідовності, блок-схеми та графіки XY", "миттєво перетворюються на чіткий SVG у поточній темі"]),
        "s3": (["Формули LaTeX", "наживо та на 100%", "повністю офлайн"], ["Підтримка формул $...$ і $$...$$ у Markdown та Mermaid", "миттєвий рендеринг вбудованим нативним рушієм MathML"]),
        "s4": (["Потужний пошук", "і заміна за", "допомогою Regex"], ["Форматуйте великі лог-файли за лічені секунди", "завдяки групам захоплення ($1, $2) у вікні ⌥⌘F"]),
        "s5": (["49 мов коду та", "швидка правка", "мультикурсором"], ["Виділяйте збіги через ⌘D або прямокутним", "блоком ⌥+миша для миттєвої пакетної правки"]),
    },
    "id": {
        "lproj": "id.lproj",
        "s1": (["Pratinjau Langsung", "Instan untuk", "Markdown & GFM"], ["Render tabel, kotak info, dan daftar tugas secara", "berdampingan dalam Mode Gelap efek kaca buram"]),
        "s2": (["Diagram Mermaid", "Langsung Terlukis", "Saat Anda Mengetik"], ["33 jenis diagram seperti urutan, alir, dan grafik XY", "dirender seketika menjadi SVG tajam sesuai tema aktif"]),
        "s3": (["Rumus LaTeX", "Tampil Langsung", "100% Offline"], ["Mendukung persamaan $...$ dan $$...$$ di Markdown & Mermaid", "dirender seketika dengan mesin MathML bawaan"]),
        "s4": (["Pencarian &", "Penggantian", "Regex Canggih"], ["Rapikan file log besar dengan mudah menggunakan", "grup tangkapan ($1, $2) melalui dialog ⌥⌘F"]),
        "s5": (["49 Bahasa Kode &", "Edit Cepat dengan", "Multi-Kursor"], ["Pilih kata berulang dengan ⌘D atau blok ⌥+seret", "untuk menyelesaikan editan massal dalam hitungan detik"]),
    },
    "cs": {
        "lproj": "cs.lproj",
        "s1": (["Okamžitý živý", "náhled pro", "Markdown a GFM"], ["Tabulky, upozornění a seznamy úkolů přehledně", "v elegantním tmavém režimu s efektem matného skla"]),
        "s2": (["Diagramy Mermaid", "vykreslené živě", "během psaní"], ["33 typů diagramů — sekvenční, vývojové a XY grafy —", "okamžitě vykreslených jako ostré SVG podle motivu"]),
        "s3": (["Vzorce LaTeX", "vykreslené živě", "100% offline"], ["Podpora rovnic $...$ a $$...$$ v Markdownu i diagramech Mermaid", "okamžitě vykreslených vestavěným nativním MathML enginem"]),
        "s4": (["Pokročilé hledání", "a nahrazování", "pomocí Regexu"], ["Snadno transformujte rozsáhlé logy s využitím", "zachytávacích skupin ($1, $2) v dialogu ⌥⌘F"]),
        "s5": (["49 jazyků a", "rychlé úpravy", "s multi-kurzorem"], ["Označte výskyty pomocí ⌘D nebo blokově ⌥+myš", "a upravte desítky řádků během několika sekund"]),
    },
    "sv": {
        "lproj": "sv.lproj",
        "s1": (["Direkt live-", "förhandsgranskning", "för Markdown"], ["Tabeller, notiser och uppgiftslistor renderas", "direkt i ett stilrent mörkt läge med frostat glas"]),
        "s2": (["Mermaid-diagram", "ritas upp live", "medan du skriver"], ["33 diagramtyper som sekvens-, flödes- och XY-diagram", "renderas direkt som knivskarpa temanpassade SVG-bilder"]),
        "s3": (["LaTeX-formler", "renderade live", "100 % offline"], ["Stöd för $...$- och $$...$$-ekvationer i Markdown och Mermaid", "visas direkt med den inbyggda nativa MathML-motorn"]),
        "s4": (["Kraftfull sökning", "och ersättning", "med Regex"], ["Omforma stora loggfiler enkelt med hjälp av", "fångstgrupper ($1, $2) i dialogrutan ⌥⌘F"]),
        "s5": (["49 språk &", "redigera samtidigt", "med multipekare"], ["Markera upprepade ord med ⌘D eller ⌥+dra", "för att slutföra repetitiva ändringar på sekunder"]),
    },
    "th": {
        "lproj": "th.lproj",
        "s1": (["พรีวิวแบบเรียลไทม์", "สำหรับ Markdown", "และ GFM"], ["แสดงตาราง กล่องข้อความ และรายการงานทันที", "ในโหมดมืดดีไซน์กระจกฝ้าสุดหรู"]),
        "s2": (["วาดกราฟ Mermaid", "แบบเรียลไทม์", "ทันทีที่พิมพ์"], ["รองรับแผนภาพ 33 รูปแบบ ทั้ง Sequence, Flowchart และกราฟ XY", "เรนเดอร์เป็น SVG คมชัดสูงที่ปรับตามโหมดมืด/สว่างอัตโนมัติ"]),
        "s3": (["สมการ LaTeX", "เรนเดอร์สดทันที", "ออฟไลน์ 100%"], ["รองรับสมการ $...$ และ $$...$$ ใน Markdown และแผนภาพ Mermaid", "แสดงผลคมชัดทันทีด้วยเอนจิน MathML เนทีฟในตัว"]),
        "s4": (["ค้นหาและแทนที่", "ขั้นสูงด้วย", "Regex ($1, $2)"], ["จัดรูปแบบไฟล์ล็อกขนาดใหญ่ได้อย่างง่ายดาย", "ด้วยกลุ่มจับข้อความและไฮไลต์แบบเรียลไทม์"]),
        "s5": (["ไฮไลต์ 49 ภาษา", "แก้ไขหลายจุดพร้อมกัน", "ด้วยมัลติเคอร์เซอร์"], ["เลือกคำซ้ำด้วย ⌘D หรือลากคลุมบล็อก ⌥", "เพื่อแก้ไขโค้ดหลายบรรทัดเสร็จในไม่กี่วินาที"]),
    },
    "hi": {
        "lproj": "hi.lproj",
        "s1": (["Markdown और GFM", "के लिए तुरंत", "लाइव प्रीव्यू"], ["आकर्षक फ्रॉस्टेड-ग्लास डार्क मोड में टेबल,", "अलर्ट और टास्क लिस्ट को तुरंत साथ-साथ देखें"]),
        "s2": (["Mermaid डायग्राम", "टाइप करते ही", "तुरंत लाइव ग्राफ़ में"], ["सीक्वेंस, फ़्लोचार्ट और XY चार्ट सहित 33 प्रकार के डायग्राम", "डार्क और लाइट थीम के साथ तुरंत स्पष्ट SVG में रेंडर करें"]),
        "s3": (["LaTeX गणित सूत्र", "100% ऑफ़लाइन", "तुरंत लाइव प्रीव्यू"], ["Markdown और Mermaid डायग्राम में $...$ तथा $$...$$ सूत्रों का समर्थन", "इन-बिल्ट नेटिव MathML इंजन द्वारा तुरंत स्पष्ट रेंडरिंग"]),
        "s4": (["शक्तिशाली Regex", "खोज और एक साथ", "बदलाव (⌥⌘F)"], ["कैप्चर ग्रुप ($1, $2) का उपयोग करके विशाल", "लॉग फ़ाइलों को आसानी से फ़ॉर्मेट करें"]),
        "s5": (["49 भाषाएं और", "मल्टी-कर्सर से", "तेज़ कोड एडिटिंग"], ["⌘D या ⌥+ड्रैग ब्लॉक चयन के साथ", "दोहराए जाने वाले बदलाव सेकंडों में पूरे करें"]),
    },
    "ro": {
        "lproj": "ro.lproj",
        "s1": (["Previzualizare", "live pentru", "Markdown și GFM"], ["Tabele, alerte și liste de sarcini redate instantaneu", "într-un mod întunecat cu efect de sticlă sablată"]),
        "s2": (["Diagrame Mermaid", "redate în direct", "pe măsură ce scrii"], ["33 de tipuri de diagrame — secvențe, fluxuri și grafice XY —", "transformate instantaneu în SVG clar adaptat la temă"]),
        "s3": (["Formule LaTeX", "redate în direct", "100% offline"], ["Suportă ecuații $...$ și $$...$$ în Markdown și diagrame Mermaid", "afisate instantaneu prin motorul MathML nativ integrat"]),
        "s4": (["Căutare și", "înlocuire Regex", "puternică"], ["Reformați fișiere mari de jurnal cu ușurință", "folosind grupuri de captură ($1, $2) prin ⌥⌘F"]),
        "s5": (["49 de limbaje și", "editare rapidă", "cu multi-cursor"], ["Selectați apariții multiple cu ⌘D sau ⌥+tragere", "pentru a finaliza modificările în câteva secunde"]),
    },
    "hu": {
        "lproj": "hu.lproj",
        "s1": (["Azonnali élő", "előnézet Markdown", "és GFM fájlokhoz"], ["Táblázatok, figyelmeztetések és feladatlisták", "tejüveg hatású, elegáns sötét módban"]),
        "s2": (["Mermaid diagramok", "élő kirajzolása", "gépelés közben"], ["33 diagramtípus — szekvencia-, folyamat- és XY-grafikonok —", "azonnali, témához igazodó éles SVG megjelenítéssel"]),
        "s3": (["LaTeX képletek", "élő megjelenítése", "100% offline"], ["$...$ és $$...$$ egyenletek támogatása Markdownban és Mermaidben", "azonnali, tűéles megjelenítés a natív MathML motorral"]),
        "s4": (["Hatékony Regex", "keresés és", "csoportos csere"], ["Alakítsa át a hatalmas naplófájlokat könnyedén", "befogócsoportokkal ($1, $2) az ⌥⌘F ablakban"]),
        "s5": (["49 nyelv és", "villámgyors írás", "több kurzorral"], ["Jelöljön ki több előfordulást a ⌘D-vel vagy", "⌥+húzással, és írja át őket másodpercek alatt"]),
    },
    "el": {
        "lproj": "el.lproj",
        "s1": (["Ζωντανή", "προεπισκόπηση", "Markdown & GFM"], ["Πίνακες, ειδοποιήσεις και λίστες εργασιών άμεσα", "σε κομψή σκοτεινή λειτουργία με εφέ αμμοβολής"]),
        "s2": (["Διαγράμματα Mermaid", "σχεδιασμένα ζωντανά", "καθώς πληκτρολογείτε"], ["33 τύποι διαγραμμάτων, ροής, ακολουθίας και γραφήματα XY", "αποδίδονται αμέσως σε ευκρινή SVG προσαρμοσμένα στο θέμα"]),
        "s3": (["Εξισώσεις LaTeX", "ζωντανή απόδοση", "100% εκτός σύνδεσης"], ["Υποστήριξη τύπων $...$ και $$...$$ σε Markdown & Mermaid", "άμεση απόδοση μέσω της εγγενούς μηχανής MathML"]),
        "s4": (["Ισχυρή αναζήτηση", "και αντικατάσταση", "με Regex"], ["Μορφοποιήστε μεγάλα αρχεία καταγραφής εύκολα", "με ομάδες σύλληψης ($1, $2) μέσω του ⌥⌘F"]),
        "s5": (["49 γλώσσες &", "ταυτόχρονη γραφή", "πολλαπλών δρομέων"], ["Επιλέξτε επαναλαμβανόμενες λέξεις με ⌘D", "ή ⌥+σύρσιμο για άμεσες μαζικές αλλαγές"]),
    },
    "da": {
        "lproj": "da.lproj",
        "s1": (["Øjeblikkelig", "live-visning af", "Markdown & GFM"], ["Tabeller, bokse og opgavelister gengives straks", "i en elegant mørk tilstand med matteret glas"]),
        "s2": (["Mermaid-diagrammer", "tegnes live mens", "du skriver"], ["33 diagramtyper som sekvens-, flow- og XY-grafer", "gengives straks som skarpe SVG'er tilpasset temaet"]),
        "s3": (["LaTeX-formler", "gengivet live", "100 % offline"], ["Understøtter $...$- og $$...$$-ligninger i Markdown og Mermaid", "vises øjeblikkeligt med den indbyggede native MathML-motor"]),
        "s4": (["Kraftfuld Regex-", "søgning og", "masse-erstatning"], ["Omform store logfiler nemt ved hjælp af", "capture-grupper ($1, $2) i ⌥⌘F-vinduet"]),
        "s5": (["49 sprog &", "rediger samtidigt", "med multi-markør"], ["Marker gentagne ord med ⌘D eller ⌥+træk", "for at klare gentagne rettelser på få sekunder"]),
    },
    "fi": {
        "lproj": "fi.lproj",
        "s1": (["Reaaliaikainen", "esikatselu", "Markdownille"], ["Taulukot, huomautukset ja tehtävälistat heti", "tyylikkäässä huurrelasi-tehosteisessa tummassa tilassa"]),
        "s2": (["Mermaid-kaaviot", "piirtyvät livenä", "kirjoittaessasi"], ["33 kaaviotyyppiä kuten sekvenssi-, vuo- ja XY-kaaviot", "renderöidään heti tarkaksi teemaan mukautuvaksi SVG:ksi"]),
        "s3": (["LaTeX-kaavat", "piirtyvät livenä", "100 % offline"], ["Tukee $...$- ja $$...$$-yhtälöitä Markdownissa ja Mermaidissa", "renderöidään heti sisäänrakennetulla natiivilla MathML-moottorilla"]),
        "s4": (["Tehokas Regex-", "haku ja joukko-", "korvaaminen"], ["Muotoile suuret lokitiedostot vaivattomasti", "sieppausryhmien ($1, $2) avulla ⌥⌘F-ikkunassa"]),
        "s5": (["49 kieltä &", "muokkaa kerralla", "monikohdistimella"], ["Valitse toistuvat sanat ⌘D:llä tai ⌥+vedolla", "ja tee toistuvat muutokset muutamassa sekunnissa"]),
    },
    "nb": {
        "lproj": "nb.lproj",
        "s1": (["Øyeblikkelig", "live-forhåndsvisning", "for Markdown"], ["Tabeller, varsler og oppgavelister vises direkte", "i en elegant mørk modus med frostet glass"]),
        "s2": (["Mermaid-diagrammer", "tegnes live mens", "du skriver"], ["33 diagramtyper som sekvens-, flyt- og XY-grafer", "vises øyeblikkelig som skarpe, tematilpassede SVG-er"]),
        "s3": (["LaTeX-formler", "gjengitt live", "100 % offline"], ["Støtter $...$- og $$...$$-ligninger i Markdown og Mermaid", "vises øyeblikkelig med den innebygde native MathML-motoren"]),
        "s4": (["Kraftig Regex-", "søk og rask", "masseerstatning"], ["Formater store loggfiler enkelt ved hjelp av", "fangstgrupper ($1, $2) i ⌥⌘F-dialogen"]),
        "s5": (["49 språk og", "rediger samtidig", "med multimarkør"], ["Marker gjentatte ord med ⌘D eller ⌥+dra", "for å fullføre repeterende endringer på sekunder"]),
    },
    "ms": {
        "lproj": "ms.lproj",
        "s1": (["Pratonton Langsung", "Segera untuk", "Markdown & GFM"], ["Paparkan jadual, kotak makluman dan senarai tugas", "dalam Mod Gelap kesan kaca berkabus yang elegan"]),
        "s2": (["Rajah Mermaid", "Dilukis Secara", "Langsung Semasa Menaip"], ["33 jenis rajah termasuk urutan, carta alir dan graf XY", "dipaparkan serta-merta sebagai SVG tajam mengikut tema"]),
        "s3": (["Formula LaTeX", "Dipapar Langsung", "100% Luar Talian"], ["Menyokong persamaan $...$ dan $$...$$ dalam Markdown & Mermaid", "dipaparkan serta-merta melalui enjin MathML asli terbina dalam"]),
        "s4": (["Carian & Gantian", "Kelompok Regex", "Berkuasa Tinggi"], ["Format semula fail log besar dengan mudah", "menggunakan kumpulan tangkapan ($1, $2) melalui ⌥⌘F"]),
        "s5": (["49 Bahasa &", "Edit Serentak", "Multi-Kursor"], ["Pilih perkataan berulang dengan ⌘D atau ⌥+seret", "untuk menyiapkan suntingan pukal dalam beberapa saat"]),
    },
}

# Mapping from App Store Connect 34 locale codes to our 28 language prefixes
ASC_LOCALE_TO_PREFIX = {
    "ja": "ja",
    "en-US": "en",
    "en-GB": "en",
    "en-AU": "en",
    "en-CA": "en",
    "zh-Hans": "zh_cn",
    "zh-Hant": "zh_tw",
    "ko": "ko",
    "de-DE": "de",
    "fr-FR": "fr",
    "fr-CA": "fr",
    "es-ES": "es",
    "es-MX": "es",
    "it": "it",
    "pt-BR": "pt_br",
    "pt-PT": "pt_br",
    "ru": "ru",
    "nl-NL": "nl",
    "sv": "sv",
    "da": "da",
    "no": "nb",
    "fi": "fi",
    "pl": "pl",
    "cs": "cs",
    "hu": "hu",
    "ro": "ro",
    "el": "el",
    "tr": "tr",
    "uk": "uk",
    "vi": "vi",
    "th": "th",
    "id": "id",
    "ms": "ms",
    "hi": "hi",
}


# ============================================================================
# Font Resolution (Windows & macOS compatible)
# ============================================================================

def resolve_font_paths(prefix):
    """Return (bold_path, regular_path) for a given language prefix on Windows or macOS."""
    win_fonts = r"C:\Windows\Fonts"
    if os.path.isdir(win_fonts):
        win_map = {
            "ja": ("YuGothB.ttc", "YuGothM.ttc"),
            "zh_cn": ("msyhbd.ttc", "msyh.ttc"),
            "zh_tw": ("msjhbd.ttc", "msjh.ttc"),
            "ko": ("malgunbd.ttf", "malgun.ttf"),
            "th": ("LeelaUIb.ttf", "LeelawUI.ttf"),
            "hi": ("NirmalaB.ttf", "Nirmala.ttf"),
        }
        b_file, r_file = win_map.get(prefix, ("segoeuib.ttf", "seguisb.ttf"))
        bp = os.path.join(win_fonts, b_file)
        rp = os.path.join(win_fonts, r_file)
        if os.path.exists(bp) and os.path.exists(rp):
            return bp, rp

    # macOS font candidates
    mac_candidates = {
        "ja": [
            ("/System/Library/Fonts/ヒラギノ角ゴシック W6.ttc", "/System/Library/Fonts/ヒラギノ角ゴシック W3.ttc"),
            ("/System/Library/Fonts/Hiragino Sans GB.ttc", "/System/Library/Fonts/Hiragino Sans GB.ttc"),
        ],
        "zh_cn": [
            ("/System/Library/Fonts/STHeiti Medium.ttc", "/System/Library/Fonts/STHeiti Light.ttc"),
            ("/System/Library/Fonts/Hiragino Sans GB.ttc", "/System/Library/Fonts/Hiragino Sans GB.ttc"),
        ],
        "zh_tw": [
            ("/System/Library/Fonts/STHeiti Medium.ttc", "/System/Library/Fonts/STHeiti Light.ttc"),
            ("/System/Library/Fonts/Hiragino Sans GB.ttc", "/System/Library/Fonts/Hiragino Sans GB.ttc"),
        ],
        "ko": [
            ("/System/Library/Fonts/AppleSDGothicNeo.ttc", "/System/Library/Fonts/AppleSDGothicNeo.ttc"),
        ],
        "th": [
            ("/System/Library/Fonts/Thonburi.ttc", "/System/Library/Fonts/Thonburi.ttc"),
            ("/System/Library/Fonts/Supplemental/Ayuthaya.ttf", "/System/Library/Fonts/Supplemental/Ayuthaya.ttf"),
        ],
        "hi": [
            ("/System/Library/Fonts/Kohinoor.ttc", "/System/Library/Fonts/Kohinoor.ttc"),
            ("/System/Library/Fonts/Supplemental/Devanagari Sangam MN.ttc", "/System/Library/Fonts/Supplemental/Devanagari Sangam MN.ttc"),
        ],
    }
    default_mac = [
        ("/System/Library/Fonts/Supplemental/Arial Bold.ttf", "/System/Library/Fonts/Supplemental/Arial.ttf"),
        ("/System/Library/Fonts/HelveticaNeue.ttc", "/System/Library/Fonts/HelveticaNeue.ttc"),
        ("/System/Library/Fonts/SFNS.ttf", "/System/Library/Fonts/SFNS.ttf"),
    ]
    for bp, rp in mac_candidates.get(prefix, []) + default_mac:
        if os.path.exists(bp) and os.path.exists(rp):
            return bp, rp

    raise RuntimeError(f"Could not find suitable font files for prefix '{prefix}'")


def resolve_code_font_path():
    """Return a clean Latin/ASCII font path so backslashes (\\) never render as Yen (¥)."""
    candidates = [
        r"C:\Windows\Fonts\consola.ttf",
        r"C:\Windows\Fonts\segoeui.ttf",
        "/System/Library/Fonts/Menlo.ttc",
        "/System/Library/Fonts/Supplemental/Courier New.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/System/Library/Fonts/SFNS.ttf",
    ]
    for p in candidates:
        if os.path.exists(p):
            return p
    return resolve_font_paths("en")[1]


def parse_localizable_strings(lproj_name):
    """Parse macOS/<lproj>/Localizable.strings into a dict."""
    path = os.path.join(MACOS_DIR, lproj_name, "Localizable.strings")
    result = {}
    if not os.path.exists(path):
        return result
    with open(path, "r", encoding="utf-8") as f:
        content = f.read()
    for m in re.finditer(r'"((?:[^"\\]|\\.)*)"\s*=\s*"((?:[^"\\]|\\.)*)"\s*;', content):
        k = m.group(1).replace('\\"', '"').replace('\\n', '\n')
        v = m.group(2).replace('\\"', '"').replace('\\n', '\n')
        result[k] = v
    return result


# ============================================================================
# Shared Visual Helpers (Mesh Background, Frosted Glass, Apple Chrome)
# ============================================================================

def make_mesh_background(w, h, theme_palette):
    base_col, blobs = theme_palette
    cw, ch = max(64, w // 4), max(64, h // 4)
    canvas = Image.new("RGB", (cw, ch), base_col)
    draw = ImageDraw.Draw(canvas, "RGBA")
    for (cx, cy, rx, ry, col) in blobs:
        sx, sy = int(cx * cw), int(cy * ch)
        srx, sry = int(rx * cw), int(ry * ch)
        draw.ellipse([sx - srx, sy - sry, sx + srx, sy + sry], fill=col)
    canvas = canvas.filter(ImageFilter.GaussianBlur(radius=42))
    return canvas.resize((w, h), Image.Resampling.BICUBIC)


def apply_frosted_glass_panel(panel_img, bg_crop, is_dark, has_right_preview=False, is_vertical_split=False):
    """Apply translucent frosted glass to the editor portion of a panel."""
    arr = np.asarray(panel_img, dtype=np.float32)
    acrylic_bg = bg_crop.filter(ImageFilter.GaussianBlur(radius=28))
    bg_arr = np.asarray(acrylic_bg, dtype=np.float32)

    h, w, _ = arr.shape
    sample_y = min(h - 1, max(0, h - 24 if not is_vertical_split else (h // 2 - 20)))
    sample_x = min(w - 1, 180)
    sample_bg = arr[sample_y, sample_x].copy()

    glass_mask = np.ones((h, w, 1), dtype=np.float32)
    if has_right_preview:
        if is_vertical_split:
            glass_mask[h // 2:, :, 0] = 0.12
        else:
            glass_mask[:, w // 2:, 0] = 0.12

    if is_dark:
        delta = arr - sample_bg
        diff_mag = np.max(np.abs(delta), axis=2, keepdims=True)
        is_fg = np.clip((diff_mag - 8.0) / 35.0, 0.0, 1.0)
        dark_tint = np.array([16.0, 20.0, 34.0], dtype=np.float32)
        acrylic_surface = 0.58 * dark_tint + 0.42 * bg_arr
        fg_boosted = np.clip(acrylic_surface + delta * 1.22, 0.0, 255.0)
        gutter_w = min(w, 46)
        gutter_h = h // 2 if is_vertical_split else h
        gutter_fg = np.clip((diff_mag[:gutter_h, :gutter_w] - 2.0) / 14.0, 0.0, 1.0)
        is_fg[:gutter_h, :gutter_w] = np.maximum(is_fg[:gutter_h, :gutter_w], gutter_fg)
        fg_boosted[:gutter_h, :gutter_w] = np.clip(
            acrylic_surface[:gutter_h, :gutter_w] + delta[:gutter_h, :gutter_w] * 2.8, 0.0, 255.0
        )
        blended = acrylic_surface * (1.0 - is_fg) + fg_boosted * is_fg
        out = arr * (1.0 - glass_mask) + blended * glass_mask
    else:
        delta = sample_bg - arr
        diff_mag = np.max(np.abs(delta), axis=2, keepdims=True)
        is_fg = np.clip((diff_mag - 6.0) / 28.0, 0.0, 1.0)
        light_tint = np.array([248.0, 251.0, 255.0], dtype=np.float32)
        acrylic_surface = 0.66 * light_tint + 0.34 * bg_arr
        fg_ink = np.clip(acrylic_surface - delta * 1.08, 0.0, 255.0)
        blended = acrylic_surface * (1.0 - is_fg) + fg_ink * is_fg
        out = arr * (1.0 - glass_mask) + blended * glass_mask

    return Image.fromarray(np.clip(out, 0, 255).astype(np.uint8))


def draw_macos_find_replace_sheet(ui_strings, font_bold_path, font_reg_path):
    """Render an authentic macOS Find & Replace child window (NSPanel) in Light Mode."""
    dw, dh = 410, 206
    sheet = Image.new("RGBA", (dw, dh), (0, 0, 0, 0))
    draw = ImageDraw.Draw(sheet)

    # Rounded macOS NSPanel background
    draw.rounded_rectangle([0, 0, dw - 1, dh - 1], radius=10, fill=(246, 248, 252, 250), outline=(195, 205, 220, 255), width=1)
    # Title bar separator
    draw.rounded_rectangle([1, 1, dw - 2, 28], radius=9, fill=(236, 240, 246, 255))
    draw.line([(1, 28), (dw - 2, 28)], fill=(214, 220, 230, 255), width=1)

    # Traffic lights (small panel size)
    for idx, col in enumerate([(255, 95, 86), (255, 189, 46), (200, 205, 212)]):
        cx = 16 + idx * 16
        cy = 14
        draw.ellipse([cx - 5, cy - 5, cx + 5, cy + 5], fill=col)

    f_title = ImageFont.truetype(font_bold_path, 12)
    f_label = ImageFont.truetype(font_reg_path, 12)
    f_code = ImageFont.truetype(resolve_code_font_path(), 12)
    f_btn = ImageFont.truetype(font_bold_path, 11)

    title_str = ui_strings.get("Replace", "Replace")
    tb = draw.textbbox((0, 0), title_str, font=f_title)
    draw.text(((dw - (tb[2] - tb[0])) // 2, 7), title_str, font=f_title, fill=(45, 55, 72, 255))

    find_lbl = ui_strings.get("Find:", "Find:")
    repl_lbl = ui_strings.get("Replace:", "Replace:")
    draw.text((14, 42), find_lbl, font=f_label, fill=(55, 65, 81, 255))
    draw.text((14, 74), repl_lbl, font=f_label, fill=(55, 65, 81, 255))

    # Input fields
    fx = 96
    fw = dw - fx - 14
    draw.rounded_rectangle([fx, 38, fx + fw, 62], radius=5, fill=(255, 255, 255, 255), outline=(59, 130, 246, 220), width=2)
    draw.text((fx + 8, 42), r"(\d{4})/(\d{2})/(\d{2}) \[WARN\]", font=f_code, fill=(15, 23, 42, 255))

    draw.rounded_rectangle([fx, 70, fx + fw, 94], radius=5, fill=(255, 255, 255, 255), outline=(203, 213, 225, 255), width=1)
    draw.text((fx + 8, 74), r"$1-$2-$3 [FIXED]", font=f_code, fill=(15, 23, 42, 255))

    # Checkboxes row
    chk_y = 108
    mc_lbl = ui_strings.get("Match Case", "Match Case")
    ww_lbl = ui_strings.get("Whole Word", "Whole Word")
    rx_lbl = ui_strings.get("Regex", "Regex")

    # Match Case (unchecked)
    draw.rounded_rectangle([14, chk_y, 28, chk_y + 14], radius=3, fill=(255, 255, 255, 255), outline=(160, 174, 192, 255), width=1)
    draw.text((34, chk_y - 1), mc_lbl[:18], font=f_label, fill=(55, 65, 81, 255))

    # Regex (checked in macOS Accent Blue #007AFF)
    rx_x = 14
    chk_y2 = 130
    draw.rounded_rectangle([rx_x, chk_y2, rx_x + 14, chk_y2 + 14], radius=3, fill=(0, 122, 255, 255), outline=(0, 122, 255, 255), width=1)
    draw.line([(rx_x + 3, chk_y2 + 7), (rx_x + 6, chk_y2 + 10), (rx_x + 11, chk_y2 + 4)], fill=(255, 255, 255, 255), width=2)
    draw.text((rx_x + 20, chk_y2 - 1), rx_lbl[:22], font=f_btn, fill=(0, 102, 225, 255))

    # Whole Word (unchecked on right)
    ww_x = 205
    draw.rounded_rectangle([ww_x, chk_y, ww_x + 14, chk_y + 14], radius=3, fill=(255, 255, 255, 255), outline=(160, 174, 192, 255), width=1)
    draw.text((ww_x + 20, chk_y - 1), ww_lbl[:18], font=f_label, fill=(55, 65, 81, 255))

    # Action buttons at bottom right
    btn_y = 162
    btn_h = 28
    ra_str = ui_strings.get("Replace All", "Replace All")
    rp_str = ui_strings.get("Replace", "Replace")
    fn_str = ui_strings.get("Find Next", "Find Next")

    # Find Next (Default blue button)
    fn_w = max(84, (draw.textbbox((0, 0), fn_str, font=f_btn)[2]) + 22)
    fn_x = dw - 14 - fn_w
    draw.rounded_rectangle([fn_x, btn_y, fn_x + fn_w, btn_y + btn_h], radius=6, fill=(0, 122, 255, 255))
    fb = draw.textbbox((0, 0), fn_str, font=f_btn)
    draw.text((fn_x + (fn_w - (fb[2] - fb[0])) // 2, btn_y + 6), fn_str, font=f_btn, fill=(255, 255, 255, 255))

    # Replace button
    rp_w = max(76, (draw.textbbox((0, 0), rp_str, font=f_btn)[2]) + 20)
    rp_x = fn_x - 8 - rp_w
    draw.rounded_rectangle([rp_x, btn_y, rp_x + rp_w, btn_y + btn_h], radius=6, fill=(255, 255, 255, 255), outline=(190, 200, 214, 255), width=1)
    rb = draw.textbbox((0, 0), rp_str, font=f_btn)
    draw.text((rp_x + (rp_w - (rb[2] - rb[0])) // 2, btn_y + 6), rp_str, font=f_btn, fill=(30, 41, 59, 255))

    # Replace All button
    ra_w = max(88, (draw.textbbox((0, 0), ra_str, font=f_btn)[2]) + 20)
    ra_x = rp_x - 8 - ra_w
    draw.rounded_rectangle([ra_x, btn_y, ra_x + ra_w, btn_y + btn_h], radius=6, fill=(255, 255, 255, 255), outline=(190, 200, 214, 255), width=1)
    rab = draw.textbbox((0, 0), ra_str, font=f_btn)
    draw.text((ra_x + (ra_w - (rab[2] - rab[0])) // 2, btn_y + 6), ra_str, font=f_btn, fill=(30, 41, 59, 255))

    return sheet


def draw_ios_find_replace_bar(bar_w, ui_strings, font_bold_path, font_reg_path):
    """Render the authentic iOS 3-row Find & Replace bar from ViewController.mm."""
    bar_h = 132
    bar = Image.new("RGBA", (bar_w, bar_h), (242, 244, 248, 252))
    draw = ImageDraw.Draw(bar)
    draw.line([(0, 0), (bar_w, 0)], fill=(203, 213, 225, 255), width=2)
    draw.line([(0, bar_h - 1), (bar_w, bar_h - 1)], fill=(203, 213, 225, 255), width=1)

    f_code = ImageFont.truetype(resolve_code_font_path(), 14)
    f_btn = ImageFont.truetype(font_bold_path, 15)
    f_pill = ImageFont.truetype(font_bold_path, 13)

    done_str = ui_strings.get("Done", "Done")
    repl_str = ui_strings.get("Replace", "Replace")
    all_str = ui_strings.get("All", "All")

    right_area_w = 165
    tf_w = bar_w - 24 - right_area_w

    # Row 1: Search field + <  >  Done
    draw.rounded_rectangle([14, 10, 14 + tf_w, 44], radius=7, fill=(255, 255, 255, 255), outline=(59, 130, 246, 220), width=2)
    draw.text((24, 17), r"(\d{4})/(\d{2})/(\d{2}) \[WARN\]", font=f_code, fill=(15, 23, 42, 255))
    bx = 14 + tf_w + 14
    draw.text((bx, 17), "<", font=f_btn, fill=(0, 122, 255, 255))
    draw.text((bx + 32, 17), ">", font=f_btn, fill=(0, 122, 255, 255))
    draw.text((bx + 64, 17), done_str[:10], font=f_btn, fill=(0, 122, 255, 255))

    # Row 2: Replace field + Replace + All
    draw.rounded_rectangle([14, 52, 14 + tf_w, 86], radius=7, fill=(255, 255, 255, 255), outline=(203, 213, 225, 255), width=1)
    draw.text((24, 59), r"$1-$2-$3 [FIXED]", font=f_code, fill=(15, 23, 42, 255))
    draw.text((bx, 59), repl_str[:10], font=f_btn, fill=(0, 122, 255, 255))
    draw.text((bx + 78, 59), all_str[:8], font=f_btn, fill=(0, 122, 255, 255))

    # Row 3: Option pills (Match Case, Whole Word, selected blue Regex)
    mc_str = ui_strings.get("Match Case", "Match Case")
    ww_str = ui_strings.get("Whole Word", "Whole Word")
    rx_str = ui_strings.get("Regex", "Regex")

    px = 14
    py = 94
    for label, selected in [(mc_str, False), (ww_str, False), (rx_str, True)]:
        short_lbl = label if len(label) <= 20 else label[:18] + "…"
        bb = draw.textbbox((0, 0), short_lbl, font=f_pill)
        pw = (bb[2] - bb[0]) + 20
        ph = 28
        if selected:
            draw.rounded_rectangle([px, py, px + pw, py + ph], radius=6, fill=(0, 122, 255, 255), outline=(0, 122, 255, 255), width=1)
            draw.text((px + 10, py + 5), short_lbl, font=f_pill, fill=(255, 255, 255, 255))
        else:
            draw.rounded_rectangle([px, py, px + pw, py + ph], radius=6, fill=(255, 255, 255, 255), outline=(190, 200, 214, 255), width=1)
            draw.text((px + 10, py + 5), short_lbl, font=f_pill, fill=(51, 65, 85, 255))
        px += pw + 8

    return bar


def draw_ios_accessory_bar(bar_w, is_dark, ui_strings, font_bold_path, active_preview=False):
    """Render the localized iOS quick-action toolbar from ViewController.mm."""
    bar_h = 44
    bg_col = (24, 28, 38, 250) if is_dark else (241, 245, 249, 250)
    bar = Image.new("RGBA", (bar_w, bar_h), bg_col)
    draw = ImageDraw.Draw(bar)
    sep_col = (51, 65, 85, 255) if is_dark else (203, 213, 225, 255)
    draw.line([(0, 0), (bar_w, 0)], fill=sep_col, width=1)

    f_btn = ImageFont.truetype(font_bold_path, 13)
    keys = ["New", "Open", "Save", "Find", "Replace", "Live Preview", "Word Wrap", "Toggle Comment", "Tab", "Done"]
    px = 10
    for k in keys:
        lbl = ui_strings.get(k, k)
        bb = draw.textbbox((0, 0), lbl, font=f_btn)
        bw = (bb[2] - bb[0]) + 20
        if px + bw > bar_w - 8:
            break
        is_hi = (k == "Live Preview" and active_preview)
        if is_hi:
            btn_bg = (14, 165, 233, 55) if is_dark else (0, 122, 255, 28)
            btn_fg = (56, 189, 248, 255) if is_dark else (0, 102, 225, 255)
            draw.rounded_rectangle([px, 7, px + bw, bar_h - 7], radius=6, fill=btn_bg, outline=btn_fg, width=1)
        else:
            btn_bg = (255, 255, 255, 22) if is_dark else (15, 23, 42, 12)
            btn_fg = (226, 232, 240, 255) if is_dark else (30, 41, 59, 255)
            draw.rounded_rectangle([px, 7, px + bw, bar_h - 7], radius=6, fill=btn_bg)
        draw.text((px + 10, 13), lbl, font=f_btn, fill=btn_fg)
        px += bw + 6

    return bar


# ============================================================================
# Slide Asset Loader
# ============================================================================

def get_slide_specs(prefix):
    """Return list of 5 slide dicts for a given language prefix."""
    deck = LANG_DECKS[prefix]
    ui_strings = parse_localizable_strings(deck["lproj"])

    # Determine base asset tag for Mermaid / Math (5 languages captured natively) and MC / Regex
    mm_tag = prefix if prefix in ("ja", "en", "zh_cn", "zh_tw", "ko") else "en"
    rx_tag = "ja" if prefix == "ja" else "en"

    return [
        {
            "idx": 1,
            "is_dark": True,
            "has_preview": True,
            "palette": DARK_COBALT,
            "badge": "Opt + P   LIVE PREVIEW",
            "title": [t.replace("⌥", "Opt").replace("⌘", "Cmd") for t in deck["s1"][0]],
            "sub": [s.replace("⌥", "Opt").replace("⌘", "Cmd") for s in deck["s1"][1]],
            "doc_title": "sample_preview.md — miu",
            "panel_path": os.path.join(ASSETS_DIR, f"md_{prefix}.png"),
            "overlay": None,
            "ui_strings": ui_strings,
        },
        {
            "idx": 2,
            "is_dark": True,
            "has_preview": True,
            "palette": DARK_EMERALD_COBALT,
            "badge": "Opt + P   MERMAID × PELE",
            "title": [t.replace("⌥", "Opt").replace("⌘", "Cmd") for t in deck["s2"][0]],
            "sub": [s.replace("⌥", "Opt").replace("⌘", "Cmd") for s in deck["s2"][1]],
            "doc_title": "architecture.md — miu",
            "panel_path": os.path.join(ASSETS_DIR, f"mermaid_{mm_tag}.png"),
            "overlay": None,
            "ui_strings": ui_strings,
        },
        {
            "idx": 3,
            "is_dark": False,
            "has_preview": True,
            "palette": LIGHT_ROYAL_INDIGO,
            "badge": "Opt + P   LATEX MATH × PELE",
            "title": [t.replace("⌥", "Opt").replace("⌘", "Cmd") for t in deck["s3"][0]],
            "sub": [s.replace("⌥", "Opt").replace("⌘", "Cmd") for s in deck["s3"][1]],
            "doc_title": "math_notes.md — miu",
            "panel_path": os.path.join(ASSETS_DIR, f"math_{mm_tag}.png"),
            "overlay": None,
            "ui_strings": ui_strings,
        },
        {
            "idx": 4,
            "is_dark": False,
            "has_preview": False,
            "palette": LIGHT_AZURE,
            "badge": "Cmd + F   REGEX FIND & REPLACE",
            "title": [t.replace("⌥", "Opt").replace("⌘", "Cmd") for t in deck["s4"][0]],
            "sub": [s.replace("⌥⌘F", "Cmd+F").replace("⌥", "Opt").replace("⌘", "Cmd") for s in deck["s4"][1]],
            "doc_title": "server_audit.py — miu",
            "panel_path": os.path.join(ASSETS_DIR, f"regex_{rx_tag}.png"),
            "overlay": "regex",
            "ui_strings": ui_strings,
        },
        {
            "idx": 5,
            "is_dark": True,
            "has_preview": False,
            "palette": DARK_VIOLET_CYAN,
            "badge": "Cmd + D   49 LANGUAGES × MULTI-CURSOR",
            "title": [t.replace("⌥", "Opt").replace("⌘", "Cmd") for t in deck["s5"][0]],
            "sub": [s.replace("⌘D", "Cmd+D").replace("⌥+", "Opt+").replace("⌥", "Opt").replace("⌘", "Cmd") for s in deck["s5"][1]],
            "doc_title": "theme_engine.ts — miu",
            "panel_path": os.path.join(ASSETS_DIR, f"mc_{rx_tag}.png"),
            "overlay": None,
            "ui_strings": ui_strings,
        },
    ]


# ============================================================================
# Compositor 1: macOS Desktop (APP_DESKTOP: 1440 x 900, 16:10)
# ============================================================================

MAC_BG_CACHE = {}

def render_macos_slide(spec, font_bold_path, font_reg_path):
    W, H = 1440, 900
    pal_key = id(spec["palette"])
    if pal_key not in MAC_BG_CACHE:
        MAC_BG_CACHE[pal_key] = make_mesh_background(W, H, spec["palette"])
    bg = MAC_BG_CACHE[pal_key]
    is_dark = spec["is_dark"]

    # Window frame geometry on right side
    win_w, win_h = 884, 600
    tb_h = 34
    content_h = win_h - tb_h
    wx = W - win_w - 42
    wy = (H - win_h) // 2

    panel_raw = Image.open(spec["panel_path"]).convert("RGB")
    # Trim bottom 16px scrollbar strip so bottom of window is completely clean
    panel_raw = panel_raw.crop((0, 0, panel_raw.width, panel_raw.height - 16))
    panel_resized = panel_raw.resize((win_w, content_h), Image.Resampling.LANCZOS)

    bg_crop = bg.crop((wx, wy + tb_h, wx + win_w, wy + win_h))
    frosted_body = apply_frosted_glass_panel(panel_resized, bg_crop, is_dark, spec["has_preview"], is_vertical_split=False)

    # Assemble full macOS window (titlebar + frosted body)
    win_img = Image.new("RGBA", (win_w, win_h), (0, 0, 0, 0))
    wdraw = ImageDraw.Draw(win_img)
    tb_col = (24, 29, 44, 245) if is_dark else (241, 245, 251, 248)
    wdraw.rectangle([0, 0, win_w, tb_h], fill=tb_col)
    win_img.paste(frosted_body.convert("RGBA"), (0, tb_h))

    # Traffic lights
    for idx, col in enumerate([(255, 95, 86), (255, 189, 46), (39, 201, 63)]):
        cx = 20 + idx * 20
        cy = tb_h // 2
        wdraw.ellipse([cx - 6, cy - 6, cx + 6, cy + 6], fill=col)

    # Window title
    f_wtitle = ImageFont.truetype(font_bold_path, 13)
    wt_col = (203, 213, 225, 255) if is_dark else (51, 65, 85, 255)
    wt_bbox = wdraw.textbbox((0, 0), spec["doc_title"], font=f_wtitle)
    wdraw.text(((win_w - (wt_bbox[2] - wt_bbox[0])) // 2, 9), spec["doc_title"], font=f_wtitle, fill=wt_col)
    sep_col = (255, 255, 255, 28) if is_dark else (15, 23, 42, 22)
    wdraw.line([(0, tb_h), (win_w, tb_h)], fill=sep_col, width=1)

    # Optional macOS Find & Replace sheet overlay (Slide 4)
    if spec["overlay"] == "regex":
        sheet = draw_macos_find_replace_sheet(spec["ui_strings"], font_bold_path, font_reg_path)
        sw, sh = sheet.size
        sx, sy = win_w - sw - 22, tb_h + 16
        sh_layer = Image.new("RGBA", (win_w, win_h), (0, 0, 0, 0))
        ImageDraw.Draw(sh_layer).rounded_rectangle([sx - 2, sy + 6, sx + sw + 2, sy + sh + 10], radius=12, fill=(15, 23, 42, 68))
        sh_layer = sh_layer.filter(ImageFilter.GaussianBlur(12))
        win_img = Image.alpha_composite(win_img, sh_layer)
        win_img.alpha_composite(sheet, (sx, sy))

    # Rounded window mask + drop shadow onto background
    radius = 12
    mask = Image.new("L", (win_w, win_h), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, win_w - 1, win_h - 1], radius=radius, fill=255)

    canvas = bg.convert("RGBA")
    shadow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(shadow).rounded_rectangle(
        [wx - 6, wy + 16, wx + win_w + 6, wy + win_h + 26],
        radius=radius + 4,
        fill=(4, 8, 22, 135 if is_dark else 75),
    )
    shadow = shadow.filter(ImageFilter.GaussianBlur(radius=24))
    canvas = Image.alpha_composite(canvas, shadow)

    win_img.putalpha(mask)
    canvas.alpha_composite(win_img, (wx, wy))

    cdraw = ImageDraw.Draw(canvas)
    rim_col = (255, 255, 255, 72) if is_dark else (255, 255, 255, 185)
    cdraw.rounded_rectangle([wx, wy, wx + win_w - 1, wy + win_h - 1], radius=radius, outline=rim_col, width=2)

    # Left marketing copy
    tx = 48
    max_text_w = wx - tx - 28  # ~438px

    title_sz = 42
    while title_sz > 24:
        f_title = ImageFont.truetype(font_bold_path, title_sz)
        if all((cdraw.textbbox((0, 0), line, font=f_title)[2] - cdraw.textbbox((0, 0), line, font=f_title)[0]) <= max_text_w for line in spec["title"]):
            break
        title_sz -= 2

    sub_sz = 16
    while sub_sz > 11:
        f_sub = ImageFont.truetype(font_reg_path, sub_sz)
        if all((cdraw.textbbox((0, 0), sline, font=f_sub)[2] - cdraw.textbbox((0, 0), sline, font=f_sub)[0]) <= max_text_w for sline in spec["sub"]):
            break
        sub_sz -= 1

    f_badge = ImageFont.truetype(font_bold_path, 14)
    line_step = int(title_sz * 1.42)
    sub_step = int(sub_sz * 1.68)

    total_text_h = 44 + len(spec["title"]) * line_step + 16 + len(spec["sub"]) * sub_step
    ty = (H - total_text_h) // 2

    bbox = cdraw.textbbox((0, 0), spec["badge"], font=f_badge)
    bw = (bbox[2] - bbox[0]) + 28
    bh = 32
    if is_dark:
        cdraw.rounded_rectangle([tx, ty, tx + bw, ty + bh], radius=16, fill=(56, 189, 248, 38), outline=(56, 189, 248, 145), width=2)
        cdraw.text((tx + 14, ty + 6), spec["badge"], font=f_badge, fill=(125, 211, 252, 255))
    else:
        cdraw.rounded_rectangle([tx, ty, tx + bw, ty + bh], radius=16, fill=(255, 255, 255, 165), outline=(29, 78, 216, 115), width=2)
        cdraw.text((tx + 14, ty + 6), spec["badge"], font=f_badge, fill=(23, 58, 148, 255))

    ty += 48
    title_col = (255, 255, 255, 255) if is_dark else (18, 42, 92, 255)
    for line in spec["title"]:
        cdraw.text((tx, ty), line, font=f_title, fill=title_col)
        ty += line_step

    ty += 14
    sub_col = (203, 213, 225, 245) if is_dark else (45, 72, 122, 240)
    for sline in spec["sub"]:
        cdraw.text((tx, ty), sline, font=f_sub, fill=sub_col)
        ty += sub_step

    return canvas.convert("RGB")


# ============================================================================
# Compositor 2: iPhone Portrait (APP_IPHONE_67: 1290 x 2796 / 65: 1284 x 2778)
# ============================================================================

IPHONE_BG_CACHE = {}

def render_iphone_slide(spec, font_bold_path, font_reg_path, target_size=(1290, 2796)):
    # Work at 860 x 1864 (exact 2/3 scale) for speed, then LANCZOS upscale
    W, H = 860, 1864
    pal_key = id(spec["palette"])
    if pal_key not in IPHONE_BG_CACHE:
        IPHONE_BG_CACHE[pal_key] = make_mesh_background(W, H, spec["palette"])
    bg = IPHONE_BG_CACHE[pal_key]
    is_dark = spec["is_dark"]

    canvas = bg.convert("RGBA")
    cdraw = ImageDraw.Draw(canvas)

    # Top centered marketing header (y = 52 .. 425)
    max_text_w = W - 84  # 776px
    f_badge = ImageFont.truetype(font_bold_path, 17)

    title_sz = 48
    while title_sz > 28:
        f_title = ImageFont.truetype(font_bold_path, title_sz)
        if all((cdraw.textbbox((0, 0), line, font=f_title)[2] - cdraw.textbbox((0, 0), line, font=f_title)[0]) <= max_text_w for line in spec["title"]):
            break
        title_sz -= 2

    sub_sz = 20
    while sub_sz > 13:
        f_sub = ImageFont.truetype(font_reg_path, sub_sz)
        if all((cdraw.textbbox((0, 0), sline, font=f_sub)[2] - cdraw.textbbox((0, 0), sline, font=f_sub)[0]) <= max_text_w for sline in spec["sub"]):
            break
        sub_sz -= 1

    ty = 52
    bb = cdraw.textbbox((0, 0), spec["badge"], font=f_badge)
    bw = (bb[2] - bb[0]) + 36
    bh = 38
    bx = (W - bw) // 2
    if is_dark:
        cdraw.rounded_rectangle([bx, ty, bx + bw, ty + bh], radius=19, fill=(56, 189, 248, 42), outline=(56, 189, 248, 155), width=2)
        cdraw.text((bx + 18, ty + 8), spec["badge"], font=f_badge, fill=(125, 211, 252, 255))
    else:
        cdraw.rounded_rectangle([bx, ty, bx + bw, ty + bh], radius=19, fill=(255, 255, 255, 175), outline=(29, 78, 216, 120), width=2)
        cdraw.text((bx + 18, ty + 8), spec["badge"], font=f_badge, fill=(23, 58, 148, 255))

    ty += 54
    line_step = int(title_sz * 1.34)
    title_col = (255, 255, 255, 255) if is_dark else (18, 42, 92, 255)
    for line in spec["title"]:
        lb = cdraw.textbbox((0, 0), line, font=f_title)
        lw = lb[2] - lb[0]
        cdraw.text(((W - lw) // 2, ty), line, font=f_title, fill=title_col)
        ty += line_step

    ty += 12
    sub_step = int(sub_sz * 1.55)
    sub_col = (203, 213, 225, 245) if is_dark else (45, 72, 122, 240)
    for sline in spec["sub"]:
        sb = cdraw.textbbox((0, 0), sline, font=f_sub)
        sw = sb[2] - sb[0]
        cdraw.text(((W - sw) // 2, ty), sline, font=f_sub, fill=sub_col)
        ty += sub_step

    # iPhone 16 Pro Titanium frame geometry
    dev_w, dev_h = 748, 1344
    bezel = 14
    scr_w, scr_h = dev_w - bezel * 2, dev_h - bezel * 2  # 720 x 1316
    dx = (W - dev_w) // 2
    dy = 468

    # Drop shadow
    shadow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(shadow).rounded_rectangle(
        [dx - 8, dy + 16, dx + dev_w + 8, dy + dev_h + 26],
        radius=58,
        fill=(4, 8, 22, 150 if is_dark else 90),
    )
    shadow = shadow.filter(ImageFilter.GaussianBlur(radius=26))
    canvas = Image.alpha_composite(canvas, shadow)
    cdraw = ImageDraw.Draw(canvas)

    # Outer Titanium bezel
    cdraw.rounded_rectangle(
        [dx, dy, dx + dev_w - 1, dy + dev_h - 1],
        radius=54,
        fill=(30, 35, 48, 255),
        outline=(148, 163, 184, 150),
        width=3,
    )

    # Build inner iPhone screen (scr_w x scr_h = 720 x 1316)
    screen = Image.new("RGBA", (scr_w, scr_h), (16, 20, 34, 255) if is_dark else (248, 251, 255, 255))
    sdraw = ImageDraw.Draw(screen)

    # Status bar + Dynamic Island + miu Titlebar (top 78px)
    header_h = 78
    hdr_col = (18, 23, 38, 255) if is_dark else (241, 245, 251, 255)
    sdraw.rectangle([0, 0, scr_w, header_h], fill=hdr_col)

    # Dynamic Island pill
    di_w, di_h = 152, 34
    di_x = (scr_w - di_w) // 2
    sdraw.rounded_rectangle([di_x, 10, di_x + di_w, 10 + di_h], radius=17, fill=(8, 10, 16, 255))

    # Time "9:41" on left & status icons on right
    f_stat = ImageFont.truetype(font_bold_path, 15)
    fg_stat = (241, 245, 249, 255) if is_dark else (15, 23, 42, 255)
    sdraw.text((34, 17), "9:41", font=f_stat, fill=fg_stat)
    # Battery icon on right
    bx_stat = scr_w - 64
    sdraw.rounded_rectangle([bx_stat, 20, bx_stat + 26, 33], radius=3, outline=fg_stat, width=2)
    sdraw.rectangle([bx_stat + 3, 23, bx_stat + 21, 30], fill=fg_stat)

    # Centered document title row (y = 50..74)
    f_doc = ImageFont.truetype(font_bold_path, 14)
    doc_col = (180, 194, 214, 255) if is_dark else (71, 85, 105, 255)
    db = sdraw.textbbox((0, 0), spec["doc_title"], font=f_doc)
    sdraw.text(((scr_w - (db[2] - db[0])) // 2, 51), spec["doc_title"], font=f_doc, fill=doc_col)
    sdraw.line([(0, header_h - 1), (scr_w, header_h - 1)], fill=(51, 65, 85, 160) if is_dark else (203, 213, 225, 220), width=1)

    # Footer: accessory bar (44px) + home indicator area (26px) = 70px
    acc_h = 44
    home_h = 26
    footer_h = acc_h + home_h
    body_h = scr_h - header_h - footer_h  # 1316 - 78 - 70 = 1168px

    panel_raw = Image.open(spec["panel_path"]).convert("RGB")
    # Trim bottom 16px scrollbar strip
    panel_raw = panel_raw.crop((0, 0, panel_raw.width, panel_raw.height - 16))
    pw, ph = panel_raw.size

    if spec["has_preview"]:
        # Authentic iOS vertical split (top = Editor, bottom = WKWebView Preview)
        split_bar_h = 10
        top_h = (body_h - split_bar_h) // 2      # 579px
        bot_h = body_h - split_bar_h - top_h    # 579px

        left_editor = panel_raw.crop((0, 0, pw // 2 - 4, ph))
        right_preview = panel_raw.crop((pw // 2 + 4, 0, pw, ph))

        # Scale width to scr_w (720) while keeping natural aspect ratio
        ed_scaled_h = int(ph * (scr_w / float(left_editor.width)))
        ed_scaled = left_editor.resize((scr_w, ed_scaled_h), Image.Resampling.LANCZOS)
        ed_crop = ed_scaled.crop((0, 0, scr_w, min(top_h, ed_scaled_h)))
        if ed_crop.height < top_h:
            strip = ed_crop.crop((0, ed_crop.height - 4, scr_w, ed_crop.height - 2)).resize((scr_w, top_h - ed_crop.height))
            pad = Image.new("RGB", (scr_w, top_h))
            pad.paste(ed_crop, (0, 0))
            pad.paste(strip, (0, ed_crop.height))
            ed_crop = pad

        pr_scaled_h = int(ph * (scr_w / float(right_preview.width)))
        pr_scaled = right_preview.resize((scr_w, pr_scaled_h), Image.Resampling.LANCZOS)
        pr_crop = pr_scaled.crop((0, 0, scr_w, min(bot_h, pr_scaled_h)))
        if pr_crop.height < bot_h:
            strip = pr_crop.crop((0, pr_crop.height - 4, scr_w, pr_crop.height - 2)).resize((scr_w, bot_h - pr_crop.height))
            pad = Image.new("RGB", (scr_w, bot_h))
            pad.paste(pr_crop, (0, 0))
            pad.paste(strip, (0, pr_crop.height))
            pr_crop = pad

        combined_body = Image.new("RGB", (scr_w, body_h))
        combined_body.paste(ed_crop, (0, 0))
        combined_body.paste(pr_crop, (0, top_h + split_bar_h))

        bg_crop = bg.crop((dx + bezel, dy + bezel + header_h, dx + bezel + scr_w, dy + bezel + header_h + body_h))
        frosted_body = apply_frosted_glass_panel(combined_body, bg_crop, is_dark, has_right_preview=True, is_vertical_split=True)
        screen.paste(frosted_body.convert("RGBA"), (0, header_h))

        # Draw iOS vertical splitter bar with pill handle
        sp_y = header_h + top_h
        sp_bg = (30, 36, 52, 255) if is_dark else (226, 232, 240, 255)
        sdraw.rectangle([0, sp_y, scr_w, sp_y + split_bar_h], fill=sp_bg)
        pill_col = (148, 163, 184, 220) if is_dark else (100, 116, 139, 200)
        sdraw.rounded_rectangle([(scr_w - 44) // 2, sp_y + 3, (scr_w + 44) // 2, sp_y + 7], radius=2, fill=pill_col)
    else:
        # Full editor view (Slide 4 Regex or Slide 5 Multi-Cursor)
        crop_w = 660
        ed_part = panel_raw.crop((0, 0, crop_w, ph))
        ed_scaled_h = int(ph * (scr_w / float(crop_w)))
        ed_scaled = ed_part.resize((scr_w, ed_scaled_h), Image.Resampling.LANCZOS)

        # Extend bottom seamlessly using a clean 2px horizontal slice (preserving gutter + editor bg)
        slice_2px = ed_scaled.crop((0, ed_scaled_h - 6, scr_w, ed_scaled_h - 4))
        full_ed = slice_2px.resize((scr_w, body_h), Image.Resampling.NEAREST)
        full_ed.paste(ed_scaled, (0, 0))

        bg_crop = bg.crop((dx + bezel, dy + bezel + header_h, dx + bezel + scr_w, dy + bezel + header_h + body_h))
        frosted_body = apply_frosted_glass_panel(full_ed, bg_crop, is_dark, has_right_preview=False, is_vertical_split=False)
        screen.paste(frosted_body.convert("RGBA"), (0, header_h))

        if spec["overlay"] == "regex":
            find_bar = draw_ios_find_replace_bar(scr_w, spec["ui_strings"], font_bold_path, font_reg_path)
            screen.alpha_composite(find_bar, (0, header_h + body_h - find_bar.height))

    # Draw localized iOS quick-action accessory bar + Home Indicator
    acc_bar = draw_ios_accessory_bar(scr_w, is_dark, spec["ui_strings"], font_bold_path, active_preview=spec["has_preview"])
    screen.alpha_composite(acc_bar, (0, header_h + body_h))

    home_y = header_h + body_h + acc_h
    sdraw.rectangle([0, home_y, scr_w, scr_h], fill=hdr_col)
    hi_col = (203, 213, 225, 220) if is_dark else (71, 85, 105, 200)
    sdraw.rounded_rectangle([(scr_w - 160) // 2, home_y + 10, (scr_w + 160) // 2, home_y + 16], radius=3, fill=hi_col)

    # Mask inner screen with rounded corners (radius = 42)
    scr_mask = Image.new("L", (scr_w, scr_h), 0)
    ImageDraw.Draw(scr_mask).rounded_rectangle([0, 0, scr_w - 1, scr_h - 1], radius=42, fill=255)
    screen.putalpha(scr_mask)
    canvas.alpha_composite(screen, (dx + bezel, dy + bezel))

    out_rgb = canvas.convert("RGB")
    if out_rgb.size != target_size:
        out_rgb = out_rgb.resize(target_size, Image.Resampling.LANCZOS)
    return out_rgb


# ============================================================================
# Compositor 3: iPad Pro 12.9" Landscape (APP_IPAD_PRO_3GEN_129: 2732 x 2048)
# ============================================================================

IPAD_BG_CACHE = {}

def render_ipad_slide(spec, font_bold_path, font_reg_path, target_size=(2732, 2048)):
    # Work at 1366 x 1024 (exact 1/2 scale) and upscale with LANCZOS
    W, H = 1366, 1024
    pal_key = id(spec["palette"])
    if pal_key not in IPAD_BG_CACHE:
        IPAD_BG_CACHE[pal_key] = make_mesh_background(W, H, spec["palette"])
    bg = IPAD_BG_CACHE[pal_key]
    is_dark = spec["is_dark"]

    canvas = bg.convert("RGBA")
    cdraw = ImageDraw.Draw(canvas)

    # Top marketing header (y = 32 .. 220): Badge + single-line joined headline + single-line subtitle
    f_badge = ImageFont.truetype(font_bold_path, 15)
    bb = cdraw.textbbox((0, 0), spec["badge"], font=f_badge)
    bw = (bb[2] - bb[0]) + 32
    bh = 34
    bx = (W - bw) // 2
    ty = 30
    if is_dark:
        cdraw.rounded_rectangle([bx, ty, bx + bw, ty + bh], radius=17, fill=(56, 189, 248, 42), outline=(56, 189, 248, 155), width=2)
        cdraw.text((bx + 16, ty + 7), spec["badge"], font=f_badge, fill=(125, 211, 252, 255))
    else:
        cdraw.rounded_rectangle([bx, ty, bx + bw, ty + bh], radius=17, fill=(255, 255, 255, 175), outline=(29, 78, 216, 120), width=2)
        cdraw.text((bx + 16, ty + 7), spec["badge"], font=f_badge, fill=(23, 58, 148, 255))

    # Join title lines with spaces (or no space for CJK)
    is_cjk = any(ord(ch) > 0x2E80 for line in spec["title"] for ch in line)
    sep = "" if is_cjk else " "
    full_title = sep.join(spec["title"])
    max_tw = W - 120

    title_sz = 42
    while title_sz > 22:
        f_title = ImageFont.truetype(font_bold_path, title_sz)
        tb = cdraw.textbbox((0, 0), full_title, font=f_title)
        if (tb[2] - tb[0]) <= max_tw:
            break
        title_sz -= 2

    ty += 48
    title_col = (255, 255, 255, 255) if is_dark else (18, 42, 92, 255)
    tb = cdraw.textbbox((0, 0), full_title, font=f_title)
    cdraw.text(((W - (tb[2] - tb[0])) // 2, ty), full_title, font=f_title, fill=title_col)

    ty += int(title_sz * 1.38)
    sub_col = (203, 213, 225, 245) if is_dark else (45, 72, 122, 240)
    f_sub = ImageFont.truetype(font_reg_path, 17)
    for sline in spec["sub"]:
        sb = cdraw.textbbox((0, 0), sline, font=f_sub)
        cdraw.text(((W - (sb[2] - sb[0])) // 2, ty), sline, font=f_sub, fill=sub_col)
        ty += 26

    # iPad Pro 12.9" landscape frame
    dev_w, dev_h = 1148, 760
    bezel = 16
    scr_w, scr_h = dev_w - bezel * 2, dev_h - bezel * 2  # 1116 x 728
    dx = (W - dev_w) // 2
    dy = 224

    shadow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(shadow).rounded_rectangle(
        [dx - 8, dy + 16, dx + dev_w + 8, dy + dev_h + 26],
        radius=38,
        fill=(4, 8, 22, 145 if is_dark else 85),
    )
    shadow = shadow.filter(ImageFilter.GaussianBlur(radius=24))
    canvas = Image.alpha_composite(canvas, shadow)
    cdraw = ImageDraw.Draw(canvas)

    cdraw.rounded_rectangle(
        [dx, dy, dx + dev_w - 1, dy + dev_h - 1],
        radius=34,
        fill=(28, 33, 46, 255),
        outline=(148, 163, 184, 150),
        width=3,
    )

    screen = Image.new("RGBA", (scr_w, scr_h), (16, 20, 34, 255) if is_dark else (248, 251, 255, 255))
    sdraw = ImageDraw.Draw(screen)

    # iPadOS status + title bar (36px)
    hdr_h = 36
    hdr_col = (20, 25, 40, 255) if is_dark else (241, 245, 251, 255)
    sdraw.rectangle([0, 0, scr_w, hdr_h], fill=hdr_col)
    f_stat = ImageFont.truetype(font_bold_path, 13)
    fg_stat = (226, 232, 240, 255) if is_dark else (30, 41, 59, 255)
    sdraw.text((22, 9), "9:41  iPad Pro", font=f_stat, fill=fg_stat)
    db = sdraw.textbbox((0, 0), spec["doc_title"], font=f_stat)
    sdraw.text(((scr_w - (db[2] - db[0])) // 2, 9), spec["doc_title"], font=f_stat, fill=fg_stat)
    sdraw.text((scr_w - 68, 9), "100%", font=f_stat, fill=fg_stat)

    # Bottom accessory + home bar (44px)
    foot_h = 44
    body_h = scr_h - hdr_h - foot_h  # 648px

    panel_raw = Image.open(spec["panel_path"]).convert("RGB")
    panel_raw = panel_raw.crop((0, 0, panel_raw.width, panel_raw.height - 16))
    panel_resized = panel_raw.resize((scr_w, body_h), Image.Resampling.LANCZOS)
    bg_crop = bg.crop((dx + bezel, dy + bezel + hdr_h, dx + bezel + scr_w, dy + bezel + hdr_h + body_h))
    frosted_body = apply_frosted_glass_panel(panel_resized, bg_crop, is_dark, spec["has_preview"], is_vertical_split=False)
    screen.paste(frosted_body.convert("RGBA"), (0, hdr_h))

    if spec["overlay"] == "regex":
        find_bar = draw_ios_find_replace_bar(scr_w, spec["ui_strings"], font_bold_path, font_reg_path)
        screen.alpha_composite(find_bar, (0, hdr_h + body_h - find_bar.height))

    acc_bar = draw_ios_accessory_bar(scr_w, is_dark, spec["ui_strings"], font_bold_path, active_preview=spec["has_preview"])
    screen.alpha_composite(acc_bar, (0, hdr_h + body_h))

    scr_mask = Image.new("L", (scr_w, scr_h), 0)
    ImageDraw.Draw(scr_mask).rounded_rectangle([0, 0, scr_w - 1, scr_h - 1], radius=22, fill=255)
    screen.putalpha(scr_mask)
    canvas.alpha_composite(screen, (dx + bezel, dy + bezel))

    out_rgb = canvas.convert("RGB")
    if out_rgb.size != target_size:
        out_rgb = out_rgb.resize(target_size, Image.Resampling.LANCZOS)
    return out_rgb


# ============================================================================
# Batch Renderer for All Needed Display Types & Prefixes
# ============================================================================

DISPLAY_TYPE_RENDERERS = {
    "APP_DESKTOP": lambda spec, fb, fr: render_macos_slide(spec, fb, fr),
    "APP_IPHONE_67": lambda spec, fb, fr: render_iphone_slide(spec, fb, fr, (1290, 2796)),
    "APP_IPHONE_65": lambda spec, fb, fr: render_iphone_slide(spec, fb, fr, (1284, 2778)),
    "APP_IPAD_PRO_3GEN_129": lambda spec, fb, fr: render_ipad_slide(spec, fb, fr, (2732, 2048)),
    "APP_IPAD_PRO_129": lambda spec, fb, fr: render_ipad_slide(spec, fb, fr, (2732, 2048)),
}


def render_prefix_display_type(prefix, display_type, out_dir):
    """Render all 5 slides for (prefix, display_type) into out_dir and return list of file paths."""
    os.makedirs(out_dir, exist_ok=True)
    fb, fr = resolve_font_paths(prefix)
    specs = get_slide_specs(prefix)
    renderer = DISPLAY_TYPE_RENDERERS[display_type]
    paths = []
    for spec in specs:
        fname = f"miu_{prefix}_{display_type}_{spec['idx']}.png"
        fpath = os.path.join(out_dir, fname)
        if not os.path.exists(fpath):
            img = renderer(spec, fb, fr)
            img.save(fpath, "PNG", optimize=False)
        paths.append(fpath)
    return paths


# ============================================================================
# App Store Connect API Upload Engine
# ============================================================================

def generate_token():
    import jwt
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


def api_request(method, url, headers=None, json_data=None, data=None, extra_headers=None, retries=4):
    import requests
    for attempt in range(retries):
        hdrs = headers if headers is not None else get_headers()
        if extra_headers:
            hdrs = dict(hdrs)
            hdrs.update(extra_headers)
        try:
            r = requests.request(method, url, headers=hdrs, json=json_data, data=data, timeout=60)
            if r.status_code in (401, 403) and headers is None and attempt < retries - 1:
                time.sleep(1.0)
                continue
            if r.status_code in (429, 500, 502, 503, 504) and attempt < retries - 1:
                time.sleep(2.0 * (attempt + 1))
                continue
            return r
        except Exception as e:
            if attempt == retries - 1:
                raise
            time.sleep(2.0 * (attempt + 1))
    return None


def get_app_and_editable_versions():
    r = api_request("GET", f"{BASE_URL}/apps?filter[bundleId]={BUNDLE_ID}")
    if r.status_code != 200 or not r.json().get("data"):
        raise RuntimeError(f"Failed to find app {BUNDLE_ID}: {r.status_code} {r.text}")
    app_id = r.json()["data"][0]["id"]

    r_ver = api_request("GET", f"{BASE_URL}/apps/{app_id}/appStoreVersions?limit=50")
    if r_ver.status_code != 200:
        raise RuntimeError(f"Failed to list versions: {r_ver.status_code} {r_ver.text}")

    editable_states = {
        "PREPARE_FOR_SUBMISSION",
        "DEVELOPER_REJECTED",
        "REJECTED",
        "METADATA_REJECTED",
        "WAITING_FOR_REVIEW",
        "IN_REVIEW",
        "INVALID_BINARY",
    }
    targets = []
    for v in r_ver.json().get("data", []):
        attrs = v.get("attributes", {})
        if attrs.get("appStoreState") in editable_states:
            targets.append((attrs.get("platform"), v["id"], attrs.get("versionString")))
    return app_id, targets


def upload_single_screenshot(set_id, file_path):
    """Reserve, upload binary chunks, and commit a single screenshot to an appScreenshotSet."""
    import requests
    file_size = os.path.getsize(file_path)
    file_name = os.path.basename(file_path)
    with open(file_path, "rb") as f:
        file_bytes = f.read()
    md5_hex = hashlib.md5(file_bytes).hexdigest()

    # 1. Reserve screenshot slot
    reserve_payload = {
        "data": {
            "type": "appScreenshots",
            "attributes": {
                "fileName": file_name,
                "fileSize": file_size,
            },
            "relationships": {
                "appScreenshotSet": {
                    "data": {
                        "type": "appScreenshotSets",
                        "id": set_id,
                    }
                }
            },
        }
    }
    r_res = api_request("POST", f"{BASE_URL}/appScreenshots", json_data=reserve_payload)
    if r_res.status_code not in (200, 201):
        return False, f"Reserve failed ({r_res.status_code}): {r_res.text[:240]}"

    ss_data = r_res.json()["data"]
    ss_id = ss_data["id"]
    upload_ops = ss_data.get("attributes", {}).get("uploadOperations", [])

    # 2. Execute PUT upload operations
    for op in upload_ops:
        op_method = op.get("method", "PUT")
        op_url = op["url"]
        offset = op.get("offset", 0)
        length = op.get("length", file_size)
        chunk = file_bytes[offset : offset + length]
        op_headers = {h["name"]: h["value"] for h in op.get("requestHeaders", [])}
        for attempt in range(4):
            put_r = requests.request(op_method, op_url, headers=op_headers, data=chunk, timeout=120)
            if put_r.status_code in (200, 201, 204):
                break
            time.sleep(1.5 * (attempt + 1))
        else:
            return False, f"Chunk PUT failed ({put_r.status_code})"

    # 3. Commit screenshot upload
    commit_payload = {
        "data": {
            "type": "appScreenshots",
            "id": ss_id,
            "attributes": {
                "uploaded": True,
                "sourceFileChecksum": md5_hex,
            },
        }
    }
    r_com = api_request("PATCH", f"{BASE_URL}/appScreenshots/{ss_id}", json_data=commit_payload)
    if r_com.status_code != 200:
        return False, f"Commit failed ({r_com.status_code}): {r_com.text[:240]}"

    return True, ss_id


def clean_all_existing_screenshots_for_locale(loc_id, locale):
    """Delete all existing screenshots across ALL screenshot sets of a localization."""
    r_sets = api_request("GET", f"{BASE_URL}/appStoreVersionLocalizations/{loc_id}/appScreenshotSets?limit=50")
    if r_sets.status_code != 200:
        return 0
    total_deleted = 0
    for s in r_sets.json().get("data", []):
        set_id = s["id"]
        r_ss = api_request("GET", f"{BASE_URL}/appScreenshotSets/{set_id}/appScreenshots?limit=50")
        if r_ss.status_code == 200:
            for old_ss in r_ss.json().get("data", []):
                old_id = old_ss["id"]
                r_del = api_request("DELETE", f"{BASE_URL}/appScreenshots/{old_id}")
                if r_del.status_code in (200, 204, 404):
                    total_deleted += 1
    return total_deleted


def sync_localization_screenshot_set(loc_id, locale, display_type, file_paths):
    """Ensure the screenshot set exists for (loc_id, display_type), delete any remaining screenshots, and upload 5 new ones."""
    # 1. List existing sets for this localization
    r_sets = api_request("GET", f"{BASE_URL}/appStoreVersionLocalizations/{loc_id}/appScreenshotSets?limit=50")
    if r_sets.status_code != 200:
        return False, f"[{locale}/{display_type}] Failed to list sets: {r_sets.status_code}"

    set_id = None
    for s in r_sets.json().get("data", []):
        if s.get("attributes", {}).get("screenshotDisplayType") == display_type:
            set_id = s["id"]
            break

    if not set_id:
        create_payload = {
            "data": {
                "type": "appScreenshotSets",
                "attributes": {
                    "screenshotDisplayType": display_type,
                },
                "relationships": {
                    "appStoreVersionLocalization": {
                        "data": {
                            "type": "appStoreVersionLocalizations",
                            "id": loc_id,
                        }
                    }
                },
            }
        }
        r_create = api_request("POST", f"{BASE_URL}/appScreenshotSets", json_data=create_payload)
        if r_create.status_code in (200, 201):
            set_id = r_create.json()["data"]["id"]
        elif r_create.status_code == 409:
            r_sets2 = api_request("GET", f"{BASE_URL}/appStoreVersionLocalizations/{loc_id}/appScreenshotSets?limit=50")
            for s in r_sets2.json().get("data", []):
                if s.get("attributes", {}).get("screenshotDisplayType") == display_type:
                    set_id = s["id"]
                    break
        if not set_id:
            return False, f"[{locale}/{display_type}] Failed to create set ({r_create.status_code}): {r_create.text[:200]}"

    # 2. Ensure set is empty before uploading
    r_existing = api_request("GET", f"{BASE_URL}/appScreenshotSets/{set_id}/appScreenshots?limit=50")
    deleted_count = 0
    if r_existing.status_code == 200:
        for old_ss in r_existing.json().get("data", []):
            old_id = old_ss["id"]
            r_del = api_request("DELETE", f"{BASE_URL}/appScreenshots/{old_id}")
            if r_del.status_code in (200, 204, 404):
                deleted_count += 1

    # 3. Upload the 5 new screenshots in order (1..5)
    uploaded_ids = []
    for fpath in file_paths:
        ok, info = upload_single_screenshot(set_id, fpath)
        if not ok:
            return False, f"[{locale}/{display_type}] {info}"
        uploaded_ids.append(info)

    # 4. Explicitly set the ordering relationship on the set
    order_payload = {
        "data": [{"type": "appScreenshots", "id": sid} for sid in uploaded_ids]
    }
    api_request("PATCH", f"{BASE_URL}/appScreenshotSets/{set_id}/relationships/appScreenshots", json_data=order_payload)

    return True, f"[{locale}/{display_type}] Uploaded {len(uploaded_ids)} screenshots"


def discover_platform_display_types(version_id, plat, existing_locs):
    """Discover active screenshotDisplayTypes configured on primary locales, ensuring both iPhone & iPad on IOS."""
    if plat == "MAC_OS":
        return ["APP_DESKTOP"]

    discovered_iphone = None
    discovered_ipad = None
    for probe_loc in ("ja", "en-US"):
        if probe_loc in existing_locs:
            loc_id = existing_locs[probe_loc]
            r = api_request("GET", f"{BASE_URL}/appStoreVersionLocalizations/{loc_id}/appScreenshotSets?limit=50")
            if r.status_code == 200:
                for s in r.json().get("data", []):
                    dt = s.get("attributes", {}).get("screenshotDisplayType", "")
                    if dt in ("APP_IPHONE_67", "APP_IPHONE_65") and not discovered_iphone:
                        discovered_iphone = dt
                    elif dt in ("APP_IPAD_PRO_3GEN_129", "APP_IPAD_PRO_129") and not discovered_ipad:
                        discovered_ipad = dt
    return [
        discovered_iphone or "APP_IPHONE_67",
        discovered_ipad or "APP_IPAD_PRO_3GEN_129",
    ]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--local-test", action="store_true", help="Render ja & en-US screenshots locally without uploading")
    parser.add_argument("--out-dir", default=os.path.join(REPO_ROOT, "build_screenshots"), help="Output directory for generated PNGs")
    args = parser.parse_args()

    if args.local_test:
        print(f"=== Local Test Mode: Rendering ja & en into {args.out_dir} ===")
        for prefix in ("ja", "en"):
            for dt in ("APP_DESKTOP", "APP_IPHONE_67", "APP_IPAD_PRO_3GEN_129"):
                t0 = time.time()
                paths = render_prefix_display_type(prefix, dt, args.out_dir)
                print(f"  [OK] {prefix} / {dt}: {len(paths)} files in {time.time() - t0:.2f}s")
        return

    print("=== App Store Connect Screenshot Generator & Synchronizer for miu ===")
    app_id, targets = get_app_and_editable_versions()
    print(f"Found App ID: {app_id}, Editable Versions: {targets}")

    for plat, version_id, version_str in targets:
        print(f"\n================ {plat} v{version_str} (ID: {version_id}) ================")
        r_locs = api_request("GET", f"{BASE_URL}/appStoreVersions/{version_id}/appStoreVersionLocalizations?limit=200")
        if r_locs.status_code != 200:
            print(f"[{plat}] Failed to fetch localizations: {r_locs.status_code} {r_locs.text}")
            continue

        existing_locs = {
            loc.get("attributes", {}).get("locale"): loc["id"]
            for loc in r_locs.json().get("data", [])
        }
        display_types = discover_platform_display_types(version_id, plat, existing_locs)
        print(f"[{plat}] Target screenshotDisplayTypes: {display_types}")

        # Delete all existing screenshots across all sets for all locales first
        print(f"[{plat}] Cleaning existing screenshots across {len(existing_locs)} locales...")
        with ThreadPoolExecutor(max_workers=8) as clean_pool:
            clean_futs = {
                clean_pool.submit(clean_all_existing_screenshots_for_locale, loc_id, loc): loc
                for loc, loc_id in existing_locs.items()
            }
            total_del = sum(f.result() for f in as_completed(clean_futs))
        print(f"[{plat}] Deleted {total_del} existing screenshot(s).")

        # Also clean any other existing screenshot sets not in display_types if needed
        # First pre-render all 28 language prefixes for each display_type in display_types
        rendered_map = {}  # (prefix, dt) -> list of 5 paths
        needed_prefixes = sorted(set(ASC_LOCALE_TO_PREFIX[loc] for loc in existing_locs if loc in ASC_LOCALE_TO_PREFIX))
        print(f"[{plat}] Rendering {len(needed_prefixes)} language sets for {display_types}...")
        t_render = time.time()
        for prefix in needed_prefixes:
            for dt in display_types:
                rendered_map[(prefix, dt)] = render_prefix_display_type(prefix, dt, args.out_dir)
        print(f"[{plat}] Finished rendering {len(rendered_map) * 5} screenshots in {time.time() - t_render:.1f}s.")

        # Upload across all localizations in parallel
        tasks = []
        for locale, loc_id in sorted(existing_locs.items()):
            prefix = ASC_LOCALE_TO_PREFIX.get(locale, "en")
            for dt in display_types:
                tasks.append((loc_id, locale, dt, rendered_map[(prefix, dt)]))

        print(f"[{plat}] Uploading {len(tasks)} screenshot sets ({len(tasks) * 5} screenshots) with ThreadPoolExecutor...")
        ok_count = 0
        fail_count = 0
        with ThreadPoolExecutor(max_workers=8) as pool:
            future_map = {
                pool.submit(sync_localization_screenshot_set, loc_id, locale, dt, fpaths): (locale, dt)
                for (loc_id, locale, dt, fpaths) in tasks
            }
            for fut in as_completed(future_map):
                locale, dt = future_map[fut]
                try:
                    ok, msg = fut.result()
                    if ok:
                        ok_count += 1
                        print(f"  [OK] {msg}", flush=True)
                    else:
                        fail_count += 1
                        print(f"  [ERR] {msg}", flush=True)
                except Exception as ex:
                    fail_count += 1
                    print(f"  [EXC] [{locale}/{dt}] {ex}", flush=True)

        print(f"[{plat}] Summary: {ok_count} sets succeeded, {fail_count} failed.")


if __name__ == "__main__":
    main()
