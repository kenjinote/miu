#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Generate localized Android Play Store screenshots (Phone, 7-inch Tablet, 10-inch Tablet)
and synchronize localized Store Listings (title, shortDescription, fullDescription) and
Track Release Notes (What's New) to Google Play Console for miu (jp.hack.miu).
"""

import os
import re
import sys
import time
import json
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import requests
from PIL import Image, ImageDraw, ImageFilter, ImageFont

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.dirname(SCRIPT_DIR)
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)

from playstore_upload import get_access_token, PACKAGE_NAME, DEFAULT_KEY_PATH
import appstore_metadata_sync as ams
import appstore_screenshots_sync as ass

BASE_URL = f"https://androidpublisher.googleapis.com/androidpublisher/v3/applications/{PACKAGE_NAME}"
UPLOAD_BASE_URL = f"https://androidpublisher.googleapis.com/upload/androidpublisher/v3/applications/{PACKAGE_NAME}"

# ============================================================================
# Additional Languages for Google Play (Arabic, Hebrew, Slovak)
# ============================================================================

AR_DESC = """miu هو محرر نصوص وأكواد بسيط وخفيف، مصمم ليجعل كل شيء بدءاً من الملاحظات اليومية السريعة وحتى كتابة مستندات Markdown والبرمجة تجربة مريحة وسلسة.

بفضل سرعة التشغيل الفورية والواجهة النظيفة المريحة للعين، يمنحك miu تجربة كتابة طبيعية على الهواتف الذكية والأجهزة اللوحية التي تعمل بنظام Android.

[المميزات الرئيسية]

• سريع وخفيف في كل لحظة
افتح التطبيق وابدأ الكتابة فوراً. سواء كنت تدوّن فكرة سريعة أو تحرر ملفاً نصياً كبيراً، يعمل miu بسلاسة واستجابة عالية.

• واضح ومريح للكتابة والبرمجة
يدعم تمييز الصياغة البرمجية لـ 49 فئة من اللغات (أكثر من 120 امتداد ملف)، بما في ذلك HTML وCSS وJavaScript وPython وKotlin وC/C++ وMarkdown وJSON، مع دعم مدمج لاختصارات Emmet.

• معاينة فورية مباشرة (Live Preview)
عاين مستندات Markdown وHTML وSVG ومخططات Mermaid والمعادلات الرياضية بصيغة LaTeX مباشرة بجانب النص — بالكامل دون اتصال بالإنترنت، مع دعم التكبير والتصغير باللمس.

• كشف تلقائي لترميز الأحرف
يتعرف تلقائياً على UTF-8 وShift_JIS وEUC-JP وUTF-16 والعديد من الترميزات العالمية لمنع ظهور رموز غير مفهومة.

• أدوات تحرير متكاملة
يتضمن البحث والاستبدال (مع دعم التعبيرات النمطية Regex)، والتحرير بمؤشرات متعددة، وأرقام الأسطر، والتفاف النص، ودعم الوضع الفاتح والداكن.

• خصوصية تامة
بدون إعلانات، وبدون تتبع، وبدون جمع بيانات. جميع الميزات تعمل بنسبة 100% دون اتصال بالإنترنت على جهازك."""

AR_WHATS_NEW = """شكراً لاستخدامك miu. يتضمن هذا التحديث الميزات والتحسينات التالية:
• دعم 49 فئة من لغات تمييز الصياغة البرمجية (أكثر من 120 امتداد ملف) وتوسيع اختصارات Emmet.
• معاينة مباشرة منقسمة لمستندات Markdown وHTML وSVG ومخططات Mermaid ومعادلات LaTeX (تقسيم علوي/سفلي في الوضع العمودي ويمين/يسار في الوضع الأفقي).
• دعم التكبير والتصغير بالإصبعين (Pinch Zoom) في شاشة المعاينة وتوحيد شكل شريط التمرير مع المحرر.
• تحسين محاذاة أرقام الأسطر ودعم المؤشرات المتعددة والكشف التلقائي عن الترميز."""

HE_DESC = """miu הוא עורך טקסט וקוד פשוט וקליל, שנועד להפוך כל משימה – מרישום הערות יומיות מהירות ועד כתיבת מסמכי Markdown ותכנות – לנוחה ונעימה.

עם הפעלה מיידית וממשק נקי ונעים לעין, miu מעניק חוויית כתיבה טבעית וחלקה בטלפונים ובטאבלטים של Android.

[תכונות עיקריות]

• מהיר וזריז
פתחו את האפליקציה והתחילו להקליד מיד. בין אם מדובר בהערה קצרה או בקובץ טקסט גדול, miu נשאר מהיר וחלק.

• קריא וברור לכתיבה ולקוד
תומך בהדגשת תחביר עבור 49 קטגוריות שפה (מעל 120 סיומות קבצים), כולל HTML, CSS, JavaScript, Python, C/C++, Markdown ו-JSON, יחד עם הרחבת קיצורי Emmet מובנית.

• תצוגה מקדימה חיה (Live Preview)
צפו בזמן אמת במסמכי Markdown, HTML, SVG, דיאגרמות Mermaid ונוסחאות מתמטיות ב-LaTeX ללא צורך בחיבור לאינטרנט, כולל תמיכה בזום בצביטה.

• זיהוי קידוד תווים אוטומטי
מזהה אוטומטית UTF-8, Shift_JIS, EUC-JP, UTF-16 וקידודים נוספים כדי למנוע טקסט משובש.

• כלי עריכה מוקפדים
כולל חיפוש והחלפה (עם ביטויים רגולריים Regex), עריכה מרובת סמנים, מספרי שורות, גלישת שורות ותמיכה במצב בהיר וכהה.

• פרטיות מלאה
ללא פרסומות, ללא מעקב וללא איסוף נתונים. הכל פועל 100% אופליין במכשיר שלכם."""

HE_WHATS_NEW = """תודה שאתם משתמשים ב-miu. עדכון זה כולל:
• תמיכה ב-49 קטגוריות של הדגשת תחביר (מעל 120 סיומות קבצים) והרחבת קיצורי Emmet.
• תצוגה מקדימה חיה במסך מפוצל עבור Markdown, HTML, SVG, דיאגרמות Mermaid ונוסחאות LaTeX (פיצול עליון/תחתון במצב אנכי ושמאל/ימין במצב אופקי).
• תמיכה בזום בצביטה (Pinch Zoom) בחלון התצוגה המקדימה ואיחוד עיצוב פס הגלילה עם העורך.
• שיפור יישור מספרי שורות, עריכה מרובת סמנים וזיהוי קידוד אוטומטי."""

SK_DESC = """miu je jednoduchý a ľahký textový a kódový editor, s ktorým je písanie každodenných poznámok, textov v Markdowne aj programovanie pohodlné a príjemné.

Vďaka okamžitému spusteniu a čistému, pre oči šetrnému rozhraniu ponúka miu prirodzený a plynulý zážitok z písania na telefónoch aj tabletoch so systémom Android.

[Hlavné vlastnosti]

• Rýchly a okamžite pripravený
Otvorte aplikáciu a môžete ihneď písať. Či už si zapisujete rýchlu poznámku alebo upravujete veľký textový súbor, miu reaguje ľahko a plynule.

• Prehľadné písanie textu aj kódu
Podporuje zvýrazňovanie syntaxe pre 49 kategórií jazykov (vyše 120 prípon súborov) vrátane HTML, CSS, JavaScriptu, Pythonu, C/C++, Markdownu a JSONu, ako aj vstavané rozbaľovanie skratiek Emmet.

• Živý náhľad priamo v aplikácii
Zobrazujte si náhľad dokumentov Markdown, HTML, SVG, diagramov Mermaid a matematických vzorcov LaTeX v reálnom čase – úplne offline, vrátane priblíženia pomocou gesta pinch-to-zoom.

• Automatické rozpoznanie kódovania
Automaticky rozpozná UTF-8, Shift_JIS, EUC-JP, UTF-16 a ďalšie kódovania, aby sa text vždy zobrazil správne.

• Premyslené nástroje na úpravy
Hľadanie a nahrádzanie (s regulárnymi výrazmi), viac kurzorov, čísla riadkov, zalamovanie riadkov a plná podpora svetlého aj tmavého režimu.

• Súkromie na prvom mieste
Žiadne reklamy, žiadne sledovanie a žiadny zber dát. Všetko funguje 100 % offline priamo vo vašom zariadení."""

SK_WHATS_NEW = """Ďakujeme, že používate miu.
• Zvýrazňovanie syntaxe pre 49 kategórií (120+ prípon), Emmet, viac kurzorov a automatické rozpoznanie kódovania.
• Živý náhľad na rozdelenej obrazovke pre Markdown, HTML, SVG, 33 typov diagramov Mermaid a vzorce LaTeX offline (hore/dole na výšku, vľavo/vpravo na šírku).
• Priblíženie gestom pinch-to-zoom v náhľade a zjednotený posuvník s editorom."""

# ============================================================================
# Short Descriptions (Strictly <= 80 characters for Google Play)
# ============================================================================

PLAY_SHORT_DESC = {
    "ja-JP": "49言語ハイライトとMarkdown・Mermaid図表・LaTeX数式のオフラインプレビューを備えた軽快なテキスト＆コードエディタ",
    "en-US": "Fast text & code editor with 49 syntax languages, Markdown, Mermaid & LaTeX.",
    "en-GB": "Fast text & code editor with 49 syntax languages, Markdown, Mermaid & LaTeX.",
    "en-AU": "Fast text & code editor with 49 syntax languages, Markdown, Mermaid & LaTeX.",
    "en-CA": "Fast text & code editor with 49 syntax languages, Markdown, Mermaid & LaTeX.",
    "zh-CN": "支持49种语法高亮与Markdown、Mermaid图表、LaTeX公式离线实时预览的轻快文本与代码编辑器",
    "zh-TW": "支援49種語法突顯與Markdown、Mermaid圖表、LaTeX公式離線即時預覽的輕快文字與程式碼編輯器",
    "zh-HK": "支援49種語法突顯與Markdown、Mermaid圖表、LaTeX公式離線即時預覽的輕快文字與程式碼編輯器",
    "ko-KR": "49개 언어 구문 강조와 Markdown·Mermaid 다이어그램·LaTeX 수식 실시간 미리보기를 갖춘 가벼운 에디터",
    "de-DE": "Schneller Text- & Code-Editor mit 49 Sprachen, Markdown-, Mermaid- & LaTeX-Vorschau",
    "fr-FR": "Éditeur de texte et code rapide : 49 langages, aperçu Markdown, Mermaid et LaTeX.",
    "fr-CA": "Éditeur de texte et code rapide : 49 langages, aperçu Markdown, Mermaid et LaTeX.",
    "es-ES": "Editor de texto y código rápido con 49 lenguajes y vista previa Markdown y LaTeX.",
    "es-419": "Editor de texto y código rápido con 49 lenguajes y vista previa Markdown y LaTeX.",
    "it-IT": "Editor di testo e codice leggero con 49 linguaggi e anteprima Markdown e LaTeX.",
    "pt-BR": "Editor de texto e código rápido com 49 linguagens e prévia Markdown e LaTeX.",
    "pt-PT": "Editor de texto e código rápido com 49 linguagens e prévia Markdown e LaTeX.",
    "ru-RU": "Быстрый редактор текста и кода: 49 языков, предпросмотр Markdown, Mermaid и LaTeX",
    "nl-NL": "Snelle tekst- en code-editor met 49 talen en live Markdown-, Mermaid- & LaTeX-weergave",
    "sv-SE": "Snabb text- och kodredigerare med 49 språk samt Markdown-, Mermaid- & LaTeX-vy.",
    "da-DK": "Hurtig tekst- og kodeeditor med 49 sprog samt live Markdown-, Mermaid- & LaTeX-visning",
    "no-NO": "Rask tekst- og koderedigerer med 49 språk og live Markdown-, Mermaid- og LaTeX-visning",
    "fi-FI": "Nopea teksti- ja koodieditori: 49 kieltä sekä Markdown-, Mermaid- ja LaTeX-esikatselu",
    "pl-PL": "Szybki edytor tekstu i kodu: 49 języków oraz podgląd Markdown, Mermaid i LaTeX.",
    "cs-CZ": "Rychlý textový a kódový editor se 49 jazyky a živým náhledem Markdown a LaTeX.",
    "sk": "Rýchly textový a kódový editor so 49 jazykmi a živým náhľadom Markdown a LaTeX.",
    "hu-HU": "Gyors szöveg- és kódszerkesztő 49 nyelvvel, Markdown-, Mermaid- és LaTeX-előnézettel.",
    "ro": "Editor rapid de text și cod cu 49 de limbaje și previzualizare Markdown și LaTeX.",
    "el-GR": "Γρήγορος επεξεργαστής κειμένου & κώδικα με 49 γλώσσες και προεπισκόπηση Markdown.",
    "tr-TR": "49 dil desteği, Markdown, Mermaid ve LaTeX canlı önizlemeli hızlı metin/kod düzenleyici",
    "uk": "Швидкий редактор тексту й коду: 49 мов і живий перегляд Markdown, Mermaid та LaTeX",
    "vi": "Trình sửa văn bản & code nhẹ với 49 ngôn ngữ, xem trước Markdown, Mermaid & LaTeX",
    "th": "ตัวแก้ไขข้อความและโค้ดที่รวดเร็ว รองรับ 49 ภาษา พร้อมพรีวิว Markdown, Mermaid และ LaTeX",
    "id": "Editor teks & kode cepat dengan 49 bahasa serta pratinjau langsung Markdown & LaTeX",
    "ms": "Penyunting teks & kod pantas dengan 49 bahasa serta pratonton Markdown & LaTeX",
    "hi-IN": "49 भाषाओं और ऑफ़लाइन Markdown, Mermaid व LaTeX लाइव प्रीव्यू वाला तेज़ टेक्स्ट व कोड एडिटर",
    "ar": "محرر نصوص وأكواد سريع يدعم 49 لغة ومعاينة فورية لـ Markdown وMermaid وLaTeX",
    "iw-IL": "עורך טקסט וקוד מהיר עם 49 שפות ותצוגה מקדימה חיה ל-Markdown, Mermaid ו-LaTeX",
}

for _loc, _sd in list(PLAY_SHORT_DESC.items()):
    if len(_sd) > 80:
        PLAY_SHORT_DESC[_loc] = _sd[:79] + "…"


# ============================================================================
# Adapt App Store Descriptions & Release Notes for Android / Google Play
# ============================================================================

def adapt_desc_for_android(text: str) -> str:
    """Replace Apple-specific device mentions with Android smartphones & tablets."""
    replacements = [
        ("iPhone、iPad、Macのどの端末でも", "スマートフォンからタブレットまで、どの端末でも"),
        ("across iPhone, iPad, and Mac", "across Android phones and tablets"),
        ("在 iPhone、iPad 和 Mac 上", "在手机和平板电脑上"),
        ("在 iPhone、iPad 與 Mac 上", "在手機與平板電腦上"),
        ("iPhone, iPad, Mac 어디서나", "스마트폰과 태블릿 어디서나"),
        ("auf iPhone, iPad und Mac", "auf Smartphones und Tablets"),
        ("sur iPhone, iPad et Mac", "sur smartphone et tablette"),
        ("en iPhone, iPad y Mac", "en teléfonos y tablets"),
        ("su iPhone, iPad e Mac", "su smartphone e tablet"),
        ("no iPhone, iPad e Mac", "em smartphones e tablets"),
        ("на iPhone, iPad и Mac", "на смартфонах и планшетах"),
        ("op iPhone, iPad en Mac", "op smartphones en tablets"),
        ("på iPhone, iPad och Mac", "på mobiler och surfplattor"),
        ("på iPhone, iPad og Mac", "på telefoner og tablets"),
        ("iPhonella, iPadilla ja Macilla", "puhelimilla ja tableteilla"),
        ("na iPhonie, iPadzie oraz komputerze Mac", "na smartfonach i tabletach"),
        ("na iPhonu, iPadu i Macu", "na telefonech a tabletech"),
        ("iPhone, iPad és Mac készülékeken", "telefonokon és táblagépeken"),
        ("pe iPhone, iPad și Mac", "pe telefoane și tablete"),
        ("σε iPhone, iPad και Mac", "σε τηλέφωνα και tablet"),
        ("iPhone, iPad ve Mac'te", "telefon ve tabletlerde"),
        ("на iPhone, iPad і Mac", "на смартфонах і планшетах"),
        ("trên cả iPhone, iPad và Mac", "trên cả điện thoại và máy tính bảng"),
        ("ทั้งบน iPhone, iPad และ Mac", "ทั้งบนสมาร์ทโฟนและแท็บเล็ต"),
        ("di iPhone, iPad, maupun Mac", "di ponsel maupun tablet"),
        ("pada iPhone, iPad dan Mac", "pada telefon pintar dan tablet"),
        ("iPhone, iPad और Mac पर", "स्मार्टफ़ोन और टैबलेट पर"),
    ]
    out = text
    for old, new in replacements:
        out = out.replace(old, new)
    return out


# Localized Android Release Notes for v1.0.45 (versionCode 47), strictly <= 500 chars each
ANDROID_RELEASE_NOTES = {
    "ja-JP": """いつも miu をご利用いただきありがとうございます。
・49言語カテゴリのシンタックスハイライト、Emmet展開、マルチカーソル・矩形選択、文字コード自動判別に対応しました。
・Markdown / HTML / SVG / 33種Mermaid図表 / LaTeX数式の分割ライブプレビューを搭載しました（縦画面では上下分割、横画面では左右分割に自動対応）。
・プレビュー画面でのピンチイン・ピンチアウトによる拡大縮小に対応し、スクロールバーの表示をエディタと統一しました。
・ヘルプ表示時のキーボード自動収納やタップで閉じる操作など、使い勝手を改善しました。""",

    "en-US": """Thank you for using miu.
• Added syntax highlighting for 49 language categories (120+ extensions), Emmet expansion, multi-cursor & rectangular selection, and 40+ encoding auto-detection.
• Added split-screen Live Preview for Markdown, HTML, SVG, 33 Mermaid diagram types, and offline LaTeX math (top/bottom split in portrait, left/right in landscape).
• Added pinch-to-zoom in the Live Preview pane and unified the preview scrollbar with the editor.
• Improved help overlay & keyboard handling.""",

    "zh-CN": """感谢您使用 miu。
・支持 49 大类（120+ 扩展名）语法高亮、Emmet 缩写展开、多光标与矩形块选择，以及 40+ 字符编码自动识别。
・新增 Markdown、HTML、SVG、33 种 Mermaid 图表与离线 LaTeX 数学公式的分屏实时预览（竖屏上下分屏、横屏左右分屏）。
・支持在预览窗口中双指捏合缩放，并将预览滚动条样式与编辑器统一。
・优化了帮助弹窗自动收起软键盘及点击关闭等交互细节。""",

    "zh-TW": """感謝您使用 miu。
・支援 49 大類（120+ 副檔名）語法突顯、Emmet 縮寫展開、多游標與矩形區塊選取，以及 40+ 字元編碼自動辨識。
・新增 Markdown、HTML、SVG、33 種 Mermaid 圖表與離線 LaTeX 數學公式的分割畫面即時預覽（直向時上下分割、橫向時左右分割）。
・支援於預覽視窗中雙指縮放，並將預覽捲軸樣式與編輯器統一。
・最佳化說明視窗自動收起鍵盤及點選關閉等操作體驗。""",

    "ko-KR": """언제나 miu를 이용해 주셔서 감사합니다.
・49개 카테고리(120종 이상 확장자) 구문 강조, Emmet 약어 전개, 다중 커서·사각형 선택 및 40종 이상의 문자 인코딩 자동 감지를 지원합니다.
・Markdown, HTML, SVG, 33종 Mermaid 다이어그램 및 오프라인 LaTeX 수식의 화면 분할 실시간 미리보기를 탑재했습니다(세로 화면 상하 분할, 가로 화면 좌우 분할).
・미리보기 화면의 두 손가락 핀치 줌(확대/축소)을 지원하고 스크롤바 모양을 에디터와 통일했습니다.
・도움말 표시 시 키보드 자동 닫기 등 조작성을 개선했습니다.""",

    "de-DE": """Vielen Dank, dass Sie miu nutzen.
• Syntaxhervorhebung für 49 Sprachkategorien (120+ Endungen), Emmet-Erweiterung, Multi-Cursor- & Blockauswahl sowie automatische Zeichenkodierung.
• Geteilte Live-Vorschau für Markdown, HTML, SVG, 33 Mermaid-Diagrammtypen und Offline-LaTeX-Formeln (Hochformat oben/unten, Querformat links/rechts).
• Pinch-to-Zoom in der Live-Vorschau hinzugefügt und Bildlaufleiste mit dem Editor vereinheitlicht.""",

    "fr-FR": """Merci d’utiliser miu.
• Coloration syntaxique pour 49 catégories (120+ extensions), développement Emmet, multi-curseurs et détection automatique de l’encodage.
• Aperçu en direct en écran partagé pour Markdown, HTML, SVG, 33 types de diagrammes Mermaid et formules LaTeX hors ligne (haut/bas en portrait, gauche/droite en paysage).
• Zoom par pincement dans l’aperçu en direct et barre de défilement unifiée avec l’éditeur.""",

    "es-ES": """Gracias por usar miu.
• Resaltado de sintaxis para 49 categorías (120+ extensiones), expansión Emmet, multicursor y detección automática de codificación.
• Vista previa en vivo en pantalla dividida para Markdown, HTML, SVG, 33 tipos de diagramas Mermaid y fórmulas LaTeX sin conexión (arriba/abajo en vertical, izquierda/derecha en horizontal).
• Zoom con dos dedos en la vista previa y barra de desplazamiento unificada con el editor.""",

    "it-IT": """Grazie per aver scelto miu.
• Evidenziazione della sintassi per 49 categorie (120+ estensioni), espansione Emmet, multi-cursore e rilevamento automatico della codifica.
• Anteprima live a schermo diviso per Markdown, HTML, SVG, 33 tipi di diagrammi Mermaid e formule LaTeX offline (sopra/sotto in verticale, sinistra/destra in orizzontale).
• Zoom con pizzico nell’anteprima e barra di scorrimento unificata con l’editor.""",

    "pt-BR": """Obrigado por usar o miu.
• Destaque de sintaxe para 49 categorias (120+ extensões), expansão Emmet, multicursor e detecção automática de codificação.
• Pré-visualização ao vivo em tela dividida para Markdown, HTML, SVG, 33 tipos de diagramas Mermaid e fórmulas LaTeX offline (acima/abaixo no modo retrato, esquerda/direita no modo paisagem).
• Zoom de pinça na pré-visualização e barra de rolagem unificada com o editor.""",

    "ru-RU": """Спасибо, что пользуетесь miu.
• Подсветка синтаксиса для 49 категорий (120+ расширений), Emmet, мультикурсор и автоопределение кодировки.
• Разделённый экран живого предпросмотра для Markdown, HTML, SVG, 33 типов диаграмм Mermaid и формул LaTeX офлайн (сверху/снизу в портретном режиме, слева/справа в альбомном).
• Масштабирование щипком (pinch-to-zoom) в окне предпросмотра и единый стиль полосы прокрутки с редактором.""",

    "nl-NL": """Bedankt voor het gebruiken van miu.
• Syntaxisaccentuering voor 49 categorieën (120+ extensies), Emmet, multi-cursor en automatische tekensetherkenning.
• Gesplitste live-weergave voor Markdown, HTML, SVG, 33 Mermaid-diagramtypen en offline LaTeX-formules (boven/onder in portretstand, links/rechts in liggende stand).
• Knijp-om-te-zoomen in het voorbeeldvenster en uniforme schuifbalk met de editor.""",

    "sv-SE": """Tack för att du använder miu.
• Syntaxmarkering för 49 kategorier (120+ filändelser), Emmet, multimarkör och automatisk teckenkodning.
• Delad live-förhandsgranskning för Markdown, HTML, SVG, 33 Mermaid-diagramtyper och LaTeX-formler offline (uppe/nere i stående läge, vänster/höger i liggande läge).
• Nyp-för-att-zooma i förhandsgranskningen och enhetlig rullningslist med redigeraren.""",

    "da-DK": """Tak fordi du bruger miu.
• Syntaksfremhævning for 49 kategorier (120+ filendelser), Emmet, multimarkør og automatisk tegnsætregistrering.
• Delt live-visning for Markdown, HTML, SVG, 33 Mermaid-diagramtyper og LaTeX-formler offline (oppe/nede i stående format, venstre/højre i liggende format).
• Knib-for-at-zoome i forhåndsvisningen og ensartet rullepanel med editoren.""",

    "no-NO": """Takk for at du bruker miu.
• Syntaksutheving for 49 kategorier (120+ filtyper), Emmet, multimarkør og automatisk tegnsettgjenkjenning.
• Delt sanntidsvisning for Markdown, HTML, SVG, 33 Mermaid-diagramtyper og LaTeX-formler offline (oppe/nede i stående, venstre/højre i liggende).
• Klyp-for-å-zoome i forhåndsvisningen og enhetlig rullefelt med redigereren.""",

    "fi-FI": """Kiitos, että käytät miuta.
• 49 kategorian syntaksinkorostus (yli 120 tiedostopäätettä), Emmet, monikohdistin ja automaattinen merkistöntunnistus.
• Jaetun näytön reaaliaikainen esikatselu Markdown-, HTML-, SVG-, 33 Mermaid-kaavio- ja LaTeX-kaavoille offline-tilassa (pystysuunnassa ylä/ala, vaakasuunnassa vasen/oikea).
• Nipistyszoomaus esikatselussa ja yhtenäinen vierityspalkki editorin kanssa.""",

    "pl-PL": """Dziękujemy za korzystanie z miu.
• Kolorowanie składni dla 49 kategorii (120+ rozszerzeń), Emmet, wielokursor i automatyczne wykrywanie kodowania.
• Podgląd na żywo na podzielonym ekranie dla Markdown, HTML, SVG, 33 typów diagramów Mermaid i wzorów LaTeX offline (góra/dół w pionie, lewo/prawo w poziomie).
• Powiększanie gestem uszczypnięcia w podglądzie oraz ujednolicony pasek przewijania z edytorem.""",

    "cs-CZ": """Děkujeme, že používáte miu.
• Zvýrazňování syntaxe pro 49 kategorií (120+ přípon), Emmet, více kurzorů a automatické rozpoznání kódování.
• Živý náhled na rozdělené obrazovce pro Markdown, HTML, SVG, 33 typů diagramů Mermaid a vzorce LaTeX offline (nahoře/dole na výšku, vlevo/vpravo na šířku).
• Přiblížení gestem štípnutí v náhledu a sjednocený posuvník s editorem.""",

    "sk": SK_WHATS_NEW,

    "hu-HU": """Köszönjük, hogy a miu alkalmazást használja.
• Szintaxiskiemelés 49 kategóriában (120+ kiterjesztés), Emmet, több kurzor és automatikus karakterkódolás-felismerés.
• Osztott képernyős élő előnézet Markdown, HTML, SVG, 33 Mermaid diagramtípus és offline LaTeX képletek számára (álló módban fent/lent, fekvőben bal/jobb).
• Kétujjas nagyítás/kicsinyítés az előnézetben és egységesített görgetősáv a szerkesztővel.""",

    "ro": """Vă mulțumim că folosiți miu.
• Evidențierea sintaxei pentru 49 de categorii (120+ extensii), Emmet, cursoare multiple și detectarea automată a codificării.
• Previzualizare live pe ecran divizat pentru Markdown, HTML, SVG, 33 de tipuri de diagrame Mermaid și formule LaTeX offline (sus/jos în portret, stânga/dreapta în peisaj).
• Zoom prin ciupire în previzualizare și bară de derulare unificată cu editorul.""",

    "el-GR": """Σας ευχαριστούμε που χρησιμοποιείτε το miu.
• Επισήμανση σύνταξης για 49 κατηγορίες (120+ επεκτάσεις), Emmet, πολλαπλοί δρομείς και αυτόματη αναγνώριση κωδικοποίησης.
• Ζωντανή προεπισκόπηση σε διαιρεμένη οθόνη για Markdown, HTML, SVG, 33 τύπους διαγραμμάτων Mermaid και τύπους LaTeX εκτός σύνδεσης.
• Ζουμ με δύο δάχτυλα στην προεπισκόπηση και ενοποιημένη γραμμή κύλισης με τον επεξεργαστή.""",

    "tr-TR": """miu'yu kullandığınız için teşekkür ederiz.
• 49 dil kategorisinde (120+ uzantı) sözdizimi vurgulama, Emmet, çoklu imleç ve otomatik karakter kodlaması algılama.
• Markdown, HTML, SVG, 33 Mermaid diyagram türü ve çevrimdışı LaTeX formülleri için bölünmüş ekran canlı önizleme (dikeyde üst/alt, yatayda sol/sağ).
• Önizleme ekranında iki parmakla yakınlaştırma/uzaklaştırma ve düzenleyiciyle birleştirilmiş kaydırma çubuğu.""",

    "uk": """Дякуємо, що користуєтеся miu.
• Підсвічування синтаксису для 49 категорій (120+ розширень), Emmet, мультикурсор і автовизначення кодування.
• Живий попередній перегляд на розділеному екрані для Markdown, HTML, SVG, 33 типів діаграм Mermaid і формул LaTeX офлайн (зверху/знизу у книжковій, зліва/справа в альбомній орієнтації).
• Масштабування двома пальцями в перегляді та єдиний стиль смуги прокрутки з редактором.""",

    "vi": """Cảm ơn bạn đã sử dụng miu.
• Tô màu cú pháp cho 49 danh mục (120+ phần mở rộng), gõ tắt Emmet, đa con trỏ và tự động nhận diện bảng mã.
• Xem trước trực tiếp chia đôi màn hình cho Markdown, HTML, SVG, 33 loại sơ đồ Mermaid và công thức LaTeX ngoại tuyến (chia trên/dưới khi xoay dọc, trái/phải khi xoay ngang).
• Hỗ trợ phóng to/thu nhỏ bằng hai ngón tay trong cửa sổ xem trước và đồng bộ thanh cuộn với trình soạn thảo.""",

    "th": """ขอบคุณที่ใช้บริการ miu
• ไฮไลต์ไวยากรณ์ 49 หมวดหมู่ (120+ นามสกุลไฟล์), ขยายตัวย่อ Emmet, มัลติเคอร์เซอร์ และตรวจจับรหัสอักขระอัตโนมัติ
• พรีวิวแบบแบ่งหน้าจอสำหรับ Markdown, HTML, SVG, แผนภาพ Mermaid 33 รูปแบบ และสมการ LaTeX แบบออฟไลน์ (แบ่งบน/ล่างในแนวตั้ง และซ้าย/ขวาในแนวนอน)
• รองรับการจีบนิ้วซูมเข้า/ออกในหน้าต่างพรีวิว และปรับแถบเลื่อนให้เป็นรูปแบบเดียวกับเอดิเตอร์""",

    "id": """Terima kasih telah menggunakan miu.
• Sorotan sintaks untuk 49 kategori bahasa (120+ ekstensi), ekspansi Emmet, multi-kursor, dan deteksi pengkodean otomatis.
• Pratinjau Langsung layar terpisah untuk Markdown, HTML, SVG, 33 jenis diagram Mermaid, dan rumus LaTeX offline (atas/bawah saat vertikal, kiri/kanan saat horizontal).
• Dukungan cubit untuk zoom (pinch-to-zoom) pada pratinjau dan tampilan bilah gulir yang disatukan dengan editor.""",

    "ms": """Terima kasih kerana menggunakan miu.
• Penyerlahan sintaks untuk 49 kategori bahasa (120+ sambungan), Emmet, berbilang kursor dan pengesanan pengekodan automatik.
• Pratonton Langsung skrin pisah untuk Markdown, HTML, SVG, 33 jenis rajah Mermaid dan formula LaTeX luar talian (atas/bawah dalam mod potret, kiri/kanan dalam mod landskap).
• Sokongan cubit untuk zum pada pratonton dan bar skrol yang diseragamkan dengan penyunting.""",

    "hi-IN": """miu का उपयोग करने के लिए धन्यवाद।
• 49 भाषा श्रेणियों (120+ एक्सटेंशन) के लिए सिंटैक्स हाइलाइटिंग, Emmet विस्तार, मल्टी-कर्सर और स्वचालित एन्कोडिंग पहचान।
• Markdown, HTML, SVG, 33 Mermaid डायग्राम और ऑफ़लाइन LaTeX सूत्रों के लिए स्प्लिट-स्क्रीन लाइव प्रीव्यू (पोर्ट्रेट में ऊपर/नीचे, लैंडस्केप में बाएँ/दाएँ)।
• प्रीव्यू में पिंच-टू-ज़ूम समर्थन और एडिटर के समान एकीकृत स्क्रॉलबार।""",

    "ar": AR_WHATS_NEW,
    "iw-IL": HE_WHATS_NEW,
}

# Map regional variants
for _loc, _base in [
    ("en-GB", "en-US"),
    ("en-AU", "en-US"),
    ("en-CA", "en-US"),
    ("fr-CA", "fr-FR"),
    ("es-419", "es-ES"),
    ("pt-PT", "pt-BR"),
    ("zh-HK", "zh-TW"),
]:
    ANDROID_RELEASE_NOTES[_loc] = ANDROID_RELEASE_NOTES[_base]

for _loc, _rn in list(ANDROID_RELEASE_NOTES.items()):
    assert len(_rn) <= 500, f"Release notes for {_loc} exceed 500 chars ({len(_rn)})"


# Map Google Play BCP-47 locale -> (fullDescription, screenshot_prefix)
PLAY_LOCALE_CONFIG = {
    "ja-JP": (adapt_desc_for_android(ams.JA_DESC), "ja"),
    "en-US": (adapt_desc_for_android(ams.EN_DESC), "en"),
    "en-GB": (adapt_desc_for_android(ams.EN_DESC), "en"),
    "en-AU": (adapt_desc_for_android(ams.EN_DESC), "en"),
    "en-CA": (adapt_desc_for_android(ams.EN_DESC), "en"),
    "zh-CN": (adapt_desc_for_android(ams.ZH_HANS_DESC), "zh_cn"),
    "zh-TW": (adapt_desc_for_android(ams.ZH_HANT_DESC), "zh_tw"),
    "zh-HK": (adapt_desc_for_android(ams.ZH_HANT_DESC), "zh_tw"),
    "ko-KR": (adapt_desc_for_android(ams.KO_DESC), "ko"),
    "de-DE": (adapt_desc_for_android(ams.DE_DESC), "de"),
    "fr-FR": (adapt_desc_for_android(ams.FR_DESC), "fr"),
    "fr-CA": (adapt_desc_for_android(ams.FR_DESC), "fr"),
    "es-ES": (adapt_desc_for_android(ams.ES_DESC), "es"),
    "es-419": (adapt_desc_for_android(ams.ES_DESC), "es"),
    "it-IT": (adapt_desc_for_android(ams.IT_DESC), "it"),
    "pt-BR": (adapt_desc_for_android(ams.PT_DESC), "pt_br"),
    "pt-PT": (adapt_desc_for_android(ams.PT_DESC), "pt_br"),
    "ru-RU": (adapt_desc_for_android(ams.RU_DESC), "ru"),
    "nl-NL": (adapt_desc_for_android(ams.NL_DESC), "nl"),
    "sv-SE": (adapt_desc_for_android(ams.SV_DESC), "sv"),
    "da-DK": (adapt_desc_for_android(ams.DA_DESC), "da"),
    "no-NO": (adapt_desc_for_android(ams.NO_DESC), "nb"),
    "fi-FI": (adapt_desc_for_android(ams.FI_DESC), "fi"),
    "pl-PL": (adapt_desc_for_android(ams.PL_DESC), "pl"),
    "cs-CZ": (adapt_desc_for_android(ams.CS_DESC), "cs"),
    "sk": (SK_DESC, "cs"),
    "hu-HU": (adapt_desc_for_android(ams.HU_DESC), "hu"),
    "ro": (adapt_desc_for_android(ams.RO_DESC), "ro"),
    "el-GR": (adapt_desc_for_android(ams.EL_DESC), "el"),
    "tr-TR": (adapt_desc_for_android(ams.TR_DESC), "tr"),
    "uk": (adapt_desc_for_android(ams.UK_DESC), "uk"),
    "vi": (adapt_desc_for_android(ams.VI_DESC), "vi"),
    "th": (adapt_desc_for_android(ams.TH_DESC), "th"),
    "id": (adapt_desc_for_android(ams.ID_DESC), "id"),
    "ms": (adapt_desc_for_android(ams.MS_DESC), "ms"),
    "hi-IN": (adapt_desc_for_android(ams.HI_DESC), "hi"),
    "ar": (AR_DESC, "en"),
    "iw-IL": (HE_DESC, "en"),
}


# ============================================================================
# Android Screenshot Renderers (Phone 9:16 1080x1920 & Tablet 16:10 1920x1200)
# ============================================================================

def get_android_slide_specs(prefix: str):
    """Adapt slide specs so badges and copy use Android/PC shortcuts (Alt+P, Ctrl+F, Ctrl+D)."""
    specs = ass.get_slide_specs(prefix)
    for s in specs:
        s["badge"] = (
            s["badge"]
            .replace("Opt + P", "Alt + P")
            .replace("Cmd + F", "Ctrl + F")
            .replace("Cmd + D", "Ctrl + D")
        )
        s["title"] = [
            t.replace("Opt+Cmd+F", "Ctrl+H")
            .replace("Cmd+F", "Ctrl+F")
            .replace("Cmd+D", "Ctrl+D")
            .replace("Opt+", "Alt+")
            .replace("Opt", "Alt")
            .replace("Cmd", "Ctrl")
            for t in s["title"]
        ]
        s["sub"] = [
            sub.replace("Opt+Cmd+F", "Ctrl+H")
            .replace("Cmd+F", "Ctrl+F")
            .replace("Cmd+D", "Ctrl+D")
            .replace("Opt+", "Alt+")
            .replace("Opt", "Alt")
            .replace("Cmd", "Ctrl")
            for sub in s["sub"]
        ]
    return specs


ANDROID_PHONE_BG_CACHE = {}
ANDROID_TABLET_BG_CACHE = {}


def render_android_phone_slide(spec, font_bold_path, font_reg_path, target_size=(1080, 1920)):
    """Render an Android Phone Portrait marketing screenshot (9:16, 1080x1920) with top/bottom split preview."""
    W, H = 900, 1600
    pal_key = (id(spec["palette"]), W, H)
    if pal_key not in ANDROID_PHONE_BG_CACHE:
        ANDROID_PHONE_BG_CACHE[pal_key] = ass.make_mesh_background(W, H, spec["palette"])
    bg = ANDROID_PHONE_BG_CACHE[pal_key]
    is_dark = spec["is_dark"]

    canvas = bg.convert("RGBA")
    cdraw = ImageDraw.Draw(canvas)

    # Top marketing header (y = 40 .. 380)
    max_text_w = W - 80
    f_badge = ImageFont.truetype(font_bold_path, 16)

    title_sz = 44
    while title_sz > 26:
        f_title = ImageFont.truetype(font_bold_path, title_sz)
        if all((cdraw.textbbox((0, 0), line, font=f_title)[2] - cdraw.textbbox((0, 0), line, font=f_title)[0]) <= max_text_w for line in spec["title"]):
            break
        title_sz -= 2

    sub_sz = 19
    while sub_sz > 12:
        f_sub = ImageFont.truetype(font_reg_path, sub_sz)
        if all((cdraw.textbbox((0, 0), sline, font=f_sub)[2] - cdraw.textbbox((0, 0), sline, font=f_sub)[0]) <= max_text_w for sline in spec["sub"]):
            break
        sub_sz -= 1

    ty = 38
    bb = cdraw.textbbox((0, 0), spec["badge"], font=f_badge)
    bw = (bb[2] - bb[0]) + 34
    bh = 34
    bx = (W - bw) // 2
    if is_dark:
        cdraw.rounded_rectangle([bx, ty, bx + bw, ty + bh], radius=17, fill=(56, 189, 248, 42), outline=(56, 189, 248, 155), width=2)
        cdraw.text((bx + 17, ty + 7), spec["badge"], font=f_badge, fill=(125, 211, 252, 255))
    else:
        cdraw.rounded_rectangle([bx, ty, bx + bw, ty + bh], radius=17, fill=(255, 255, 255, 175), outline=(29, 78, 216, 120), width=2)
        cdraw.text((bx + 17, ty + 7), spec["badge"], font=f_badge, fill=(23, 58, 148, 255))

    ty += 46
    line_step = int(title_sz * 1.32)
    title_col = (255, 255, 255, 255) if is_dark else (18, 42, 92, 255)
    for line in spec["title"]:
        lb = cdraw.textbbox((0, 0), line, font=f_title)
        lw = lb[2] - lb[0]
        cdraw.text(((W - lw) // 2, ty), line, font=f_title, fill=title_col)
        ty += line_step

    ty += 10
    sub_step = int(sub_sz * 1.50)
    sub_col = (203, 213, 225, 245) if is_dark else (45, 72, 122, 240)
    for sline in spec["sub"]:
        sb = cdraw.textbbox((0, 0), sline, font=f_sub)
        sw = sb[2] - sb[0]
        cdraw.text(((W - sw) // 2, ty), sline, font=f_sub, fill=sub_col)
        ty += sub_step

    # Android flagship phone frame geometry
    dev_w, dev_h = 748, 1144
    bezel = 14
    scr_w, scr_h = dev_w - bezel * 2, dev_h - bezel * 2  # 720 x 1116
    dx = (W - dev_w) // 2
    dy = 414

    shadow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(shadow).rounded_rectangle(
        [dx - 8, dy + 14, dx + dev_w + 8, dy + dev_h + 24],
        radius=48,
        fill=(4, 8, 22, 150 if is_dark else 90),
    )
    shadow = shadow.filter(ImageFilter.GaussianBlur(radius=24))
    canvas = Image.alpha_composite(canvas, shadow)
    cdraw = ImageDraw.Draw(canvas)

    cdraw.rounded_rectangle(
        [dx, dy, dx + dev_w - 1, dy + dev_h - 1],
        radius=44,
        fill=(26, 31, 44, 255),
        outline=(148, 163, 184, 150),
        width=3,
    )

    screen = Image.new("RGBA", (scr_w, scr_h), (16, 20, 34, 255) if is_dark else (248, 251, 255, 255))
    sdraw = ImageDraw.Draw(screen)

    # Android status bar + title row + top action bar (top 108px)
    stat_h = 64
    hdr_col = (18, 23, 38, 255) if is_dark else (241, 245, 251, 255)
    sdraw.rectangle([0, 0, scr_w, stat_h], fill=hdr_col)

    # Android punch-hole front camera
    cam_r = 9
    sdraw.ellipse([(scr_w // 2) - cam_r, 12, (scr_w // 2) + cam_r, 12 + cam_r * 2], fill=(8, 10, 16, 255))

    f_stat = ImageFont.truetype(font_bold_path, 14)
    fg_stat = (241, 245, 249, 255) if is_dark else (15, 23, 42, 255)
    sdraw.text((28, 13), "9:41", font=f_stat, fill=fg_stat)
    bx_stat = scr_w - 58
    sdraw.rounded_rectangle([bx_stat, 15, bx_stat + 24, 27], radius=3, outline=fg_stat, width=2)
    sdraw.rectangle([bx_stat + 3, 18, bx_stat + 19, 24], fill=fg_stat)

    f_doc = ImageFont.truetype(font_bold_path, 13)
    doc_col = (180, 194, 214, 255) if is_dark else (71, 85, 105, 255)
    db = sdraw.textbbox((0, 0), spec["doc_title"], font=f_doc)
    sdraw.text(((scr_w - (db[2] - db[0])) // 2, 39), spec["doc_title"], font=f_doc, fill=doc_col)

    # Android top quick-action toolbar (44px right below title row)
    acc_bar = ass.draw_ios_accessory_bar(scr_w, is_dark, spec["ui_strings"], font_bold_path, active_preview=spec["has_preview"])
    screen.alpha_composite(acc_bar, (0, stat_h))
    header_h = stat_h + 44  # 108px

    nav_h = 22
    body_h = scr_h - header_h - nav_h  # 986px

    panel_raw = Image.open(spec["panel_path"]).convert("RGB")
    panel_raw = panel_raw.crop((0, 0, panel_raw.width, panel_raw.height - 16))
    pw, ph = panel_raw.size

    if spec["has_preview"]:
        split_bar_h = 10
        top_h = (body_h - split_bar_h) // 2
        bot_h = body_h - split_bar_h - top_h

        left_editor = panel_raw.crop((0, 0, pw // 2 - 4, ph))
        right_preview = panel_raw.crop((pw // 2 + 4, 0, pw, ph))

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
        frosted_body = ass.apply_frosted_glass_panel(combined_body, bg_crop, is_dark, has_right_preview=True, is_vertical_split=True)
        screen.paste(frosted_body.convert("RGBA"), (0, header_h))

        sp_y = header_h + top_h
        sp_bg = (30, 36, 52, 255) if is_dark else (226, 232, 240, 255)
        sdraw.rectangle([0, sp_y, scr_w, sp_y + split_bar_h], fill=sp_bg)
        pill_col = (148, 163, 184, 220) if is_dark else (100, 116, 139, 200)
        sdraw.rounded_rectangle([(scr_w - 48) // 2, sp_y + 3, (scr_w + 48) // 2, sp_y + 7], radius=2, fill=pill_col)
    else:
        crop_w = 660
        ed_part = panel_raw.crop((0, 0, crop_w, ph))
        ed_scaled_h = int(ph * (scr_w / float(crop_w)))
        ed_scaled = ed_part.resize((scr_w, ed_scaled_h), Image.Resampling.LANCZOS)

        slice_2px = ed_scaled.crop((0, ed_scaled_h - 6, scr_w, ed_scaled_h - 4))
        full_ed = slice_2px.resize((scr_w, body_h), Image.Resampling.NEAREST)
        full_ed.paste(ed_scaled, (0, 0))

        bg_crop = bg.crop((dx + bezel, dy + bezel + header_h, dx + bezel + scr_w, dy + bezel + header_h + body_h))
        frosted_body = ass.apply_frosted_glass_panel(full_ed, bg_crop, is_dark, has_right_preview=False, is_vertical_split=False)
        screen.paste(frosted_body.convert("RGBA"), (0, header_h))

        if spec["overlay"] == "regex":
            find_bar = ass.draw_ios_find_replace_bar(scr_w, spec["ui_strings"], font_bold_path, font_reg_path)
            screen.alpha_composite(find_bar, (0, header_h + body_h - find_bar.height))

    # Android gesture navigation bar at bottom
    nav_y = header_h + body_h
    sdraw.rectangle([0, nav_y, scr_w, scr_h], fill=hdr_col)
    hi_col = (203, 213, 225, 210) if is_dark else (71, 85, 105, 190)
    sdraw.rounded_rectangle([(scr_w - 130) // 2, nav_y + 8, (scr_w + 130) // 2, nav_y + 13], radius=3, fill=hi_col)

    scr_mask = Image.new("L", (scr_w, scr_h), 0)
    ImageDraw.Draw(scr_mask).rounded_rectangle([0, 0, scr_w - 1, scr_h - 1], radius=34, fill=255)
    screen.putalpha(scr_mask)
    canvas.alpha_composite(screen, (dx + bezel, dy + bezel))

    out_rgb = canvas.convert("RGB")
    if out_rgb.size != target_size:
        out_rgb = out_rgb.resize(target_size, Image.Resampling.LANCZOS)
    return out_rgb


def render_android_tablet_slide(spec, font_bold_path, font_reg_path, target_size=(1920, 1200)):
    """Render an Android Tablet Landscape screenshot (16:10, 1920x1200) with left/right split preview."""
    W, H = 1440, 900
    pal_key = (id(spec["palette"]), W, H)
    if pal_key not in ANDROID_TABLET_BG_CACHE:
        ANDROID_TABLET_BG_CACHE[pal_key] = ass.make_mesh_background(W, H, spec["palette"])
    bg = ANDROID_TABLET_BG_CACHE[pal_key]
    is_dark = spec["is_dark"]

    canvas = bg.convert("RGBA")
    cdraw = ImageDraw.Draw(canvas)

    f_badge = ImageFont.truetype(font_bold_path, 15)
    bb = cdraw.textbbox((0, 0), spec["badge"], font=f_badge)
    bw = (bb[2] - bb[0]) + 32
    bh = 32
    bx = (W - bw) // 2
    ty = 24
    if is_dark:
        cdraw.rounded_rectangle([bx, ty, bx + bw, ty + bh], radius=16, fill=(56, 189, 248, 42), outline=(56, 189, 248, 155), width=2)
        cdraw.text((bx + 16, ty + 6), spec["badge"], font=f_badge, fill=(125, 211, 252, 255))
    else:
        cdraw.rounded_rectangle([bx, ty, bx + bw, ty + bh], radius=16, fill=(255, 255, 255, 175), outline=(29, 78, 216, 120), width=2)
        cdraw.text((bx + 16, ty + 6), spec["badge"], font=f_badge, fill=(23, 58, 148, 255))

    is_cjk = any(ord(ch) > 0x2E80 for line in spec["title"] for ch in line)
    sep = "" if is_cjk else " "
    full_title = sep.join(spec["title"])
    max_tw = W - 120

    title_sz = 38
    while title_sz > 20:
        f_title = ImageFont.truetype(font_bold_path, title_sz)
        tb = cdraw.textbbox((0, 0), full_title, font=f_title)
        if (tb[2] - tb[0]) <= max_tw:
            break
        title_sz -= 2

    ty += 42
    title_col = (255, 255, 255, 255) if is_dark else (18, 42, 92, 255)
    tb = cdraw.textbbox((0, 0), full_title, font=f_title)
    cdraw.text(((W - (tb[2] - tb[0])) // 2, ty), full_title, font=f_title, fill=title_col)

    ty += int(title_sz * 1.35)
    sub_col = (203, 213, 225, 245) if is_dark else (45, 72, 122, 240)
    f_sub = ImageFont.truetype(font_reg_path, 16)
    for sline in spec["sub"]:
        sb = cdraw.textbbox((0, 0), sline, font=f_sub)
        cdraw.text(((W - (sb[2] - sb[0])) // 2, ty), sline, font=f_sub, fill=sub_col)
        ty += 24

    dev_w, dev_h = 1216, 668
    bezel = 14
    scr_w, scr_h = dev_w - bezel * 2, dev_h - bezel * 2
    dx = (W - dev_w) // 2
    dy = 198

    shadow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ImageDraw.Draw(shadow).rounded_rectangle(
        [dx - 8, dy + 14, dx + dev_w + 8, dy + dev_h + 24],
        radius=34,
        fill=(4, 8, 22, 145 if is_dark else 85),
    )
    shadow = shadow.filter(ImageFilter.GaussianBlur(radius=22))
    canvas = Image.alpha_composite(canvas, shadow)
    cdraw = ImageDraw.Draw(canvas)

    cdraw.rounded_rectangle(
        [dx, dy, dx + dev_w - 1, dy + dev_h - 1],
        radius=30,
        fill=(26, 31, 44, 255),
        outline=(148, 163, 184, 150),
        width=3,
    )

    screen = Image.new("RGBA", (scr_w, scr_h), (16, 20, 34, 255) if is_dark else (248, 251, 255, 255))
    sdraw = ImageDraw.Draw(screen)

    stat_h = 34
    hdr_col = (20, 25, 40, 255) if is_dark else (241, 245, 251, 255)
    sdraw.rectangle([0, 0, scr_w, stat_h], fill=hdr_col)
    f_stat = ImageFont.truetype(font_bold_path, 13)
    fg_stat = (226, 232, 240, 255) if is_dark else (30, 41, 59, 255)
    sdraw.text((22, 8), "9:41", font=f_stat, fill=fg_stat)
    db = sdraw.textbbox((0, 0), spec["doc_title"], font=f_stat)
    sdraw.text(((scr_w - (db[2] - db[0])) // 2, 8), spec["doc_title"], font=f_stat, fill=fg_stat)
    sdraw.text((scr_w - 64, 8), "100%", font=f_stat, fill=fg_stat)

    acc_bar = ass.draw_ios_accessory_bar(scr_w, is_dark, spec["ui_strings"], font_bold_path, active_preview=spec["has_preview"])
    screen.alpha_composite(acc_bar, (0, stat_h))
    hdr_h = stat_h + 44
    body_h = scr_h - hdr_h

    panel_raw = Image.open(spec["panel_path"]).convert("RGB")
    panel_raw = panel_raw.crop((0, 0, panel_raw.width, panel_raw.height - 16))
    panel_resized = panel_raw.resize((scr_w, body_h), Image.Resampling.LANCZOS)
    bg_crop = bg.crop((dx + bezel, dy + bezel + hdr_h, dx + bezel + scr_w, dy + bezel + hdr_h + body_h))
    frosted_body = ass.apply_frosted_glass_panel(panel_resized, bg_crop, is_dark, spec["has_preview"], is_vertical_split=False)
    screen.paste(frosted_body.convert("RGBA"), (0, hdr_h))

    if spec["overlay"] == "regex":
        find_bar = ass.draw_ios_find_replace_bar(scr_w, spec["ui_strings"], font_bold_path, font_reg_path)
        screen.alpha_composite(find_bar, (0, hdr_h + body_h - find_bar.height))

    scr_mask = Image.new("L", (scr_w, scr_h), 0)
    ImageDraw.Draw(scr_mask).rounded_rectangle([0, 0, scr_w - 1, scr_h - 1], radius=20, fill=255)
    screen.putalpha(scr_mask)
    canvas.alpha_composite(screen, (dx + bezel, dy + bezel))

    out_rgb = canvas.convert("RGB")
    if out_rgb.size != target_size:
        out_rgb = out_rgb.resize(target_size, Image.Resampling.LANCZOS)
    return out_rgb


def render_android_prefix_sets(prefix: str, out_dir: str):
    """Render 5 phone screenshots and 5 tablet screenshots for a language prefix."""
    os.makedirs(out_dir, exist_ok=True)
    fb, fr = ass.resolve_font_paths(prefix)
    specs = get_android_slide_specs(prefix)
    phone_paths = []
    tablet_paths = []
    for spec in specs:
        p_path = os.path.join(out_dir, f"miu_android_{prefix}_phone_{spec['idx']}.png")
        if not os.path.exists(p_path):
            img_p = render_android_phone_slide(spec, fb, fr, (1080, 1920))
            img_p.save(p_path, "PNG", optimize=False)
        phone_paths.append(p_path)

        t_path = os.path.join(out_dir, f"miu_android_{prefix}_tablet_{spec['idx']}.png")
        if not os.path.exists(t_path):
            img_t = render_android_tablet_slide(spec, fb, fr, (1920, 1200))
            img_t.save(t_path, "PNG", optimize=False)
        tablet_paths.append(t_path)
    return phone_paths, tablet_paths


# ============================================================================
# Google Play Developer API Sync Engine
# ============================================================================

def play_request(method: str, url: str, token: str, json_data=None, data=None, content_type=None, retries=4):
    for attempt in range(retries):
        headers = {"Authorization": f"Bearer {token}"}
        if content_type:
            headers["Content-Type"] = content_type
        elif json_data is not None:
            headers["Content-Type"] = "application/json"
        try:
            r = requests.request(method, url, headers=headers, json=json_data, data=data, timeout=120)
            if r.status_code in (429, 500, 502, 503, 504) and attempt < retries - 1:
                time.sleep(2.0 * (attempt + 1))
                continue
            return r
        except Exception:
            if attempt == retries - 1:
                raise
            time.sleep(2.0 * (attempt + 1))
    return None


def sync_play_store(out_dir: str, key_path: str, skip_screenshots: bool = False):
    print("=== Google Play Store Metadata, Release Notes & Screenshot Synchronizer ===")
    token = get_access_token(key_path)

    # 1. Pre-render all unique language screenshot sets first (before opening the Play edit)
    rendered_sets = {}
    if not skip_screenshots:
        needed_prefixes = sorted(set(cfg[1] for cfg in PLAY_LOCALE_CONFIG.values()))
        print(f"Rendering Android Phone & Tablet screenshots for {len(needed_prefixes)} language sets into {out_dir}...")
        t0 = time.time()
        for prefix in needed_prefixes:
            rendered_sets[prefix] = render_android_prefix_sets(prefix, out_dir)
            print(f"  [Rendered] {prefix} (5 phone + 5 tablet)", flush=True)
        print(f"Finished rendering {len(rendered_sets) * 10} screenshots in {time.time() - t0:.1f}s.\n")

    # 2. Refresh token & create a new Google Play Edit
    token = get_access_token(key_path)
    r_edit = play_request("POST", f"{BASE_URL}/edits", token, json_data={})
    r_edit.raise_for_status()
    edit_id = r_edit.json()["id"]
    print(f"Created Google Play edit ID: {edit_id}")

    # 3. Discover existing listings so we know which locales are already enabled
    r_list = play_request("GET", f"{BASE_URL}/edits/{edit_id}/listings", token)
    r_list.raise_for_status()
    existing_listings = {l["language"]: l for l in r_list.json().get("listings", [])}
    print(f"Existing Play Store listings ({len(existing_listings)}): {', '.join(sorted(existing_listings.keys()))}")

    # 4. Update/create store listings for all target locales
    print("\n--- Syncing Store Listings (Title, Short Description, Full Description) ---")
    synced_locales = []
    for locale, (full_desc, prefix) in sorted(PLAY_LOCALE_CONFIG.items()):
        short_desc = PLAY_SHORT_DESC[locale]
        body = {
            "language": locale,
            "title": "miu",
            "shortDescription": short_desc,
            "fullDescription": full_desc,
        }
        r_put = play_request("PUT", f"{BASE_URL}/edits/{edit_id}/listings/{locale}", token, json_data=body)
        if r_put.status_code == 200:
            synced_locales.append(locale)
            print(f"  [OK] Listing {locale} updated (short={len(short_desc)}c, full={len(full_desc)}c)")
        else:
            print(f"  [WARN] Listing {locale} skipped (HTTP {r_put.status_code}: {r_put.text[:200]})")

    # 5. Update Release Notes ("What's New") on 'production' and 'internal' Tracks
    print("\n--- Syncing Localized Release Notes on 'production' & 'internal' Tracks ---")
    notes_list = [
        {"language": locale, "text": ANDROID_RELEASE_NOTES[locale]}
        for locale in synced_locales
        if locale in ANDROID_RELEASE_NOTES
    ]
    for track_name in ("production", "internal"):
        r_tr = play_request("GET", f"{BASE_URL}/edits/{edit_id}/tracks/{track_name}", token)
        if r_tr.status_code == 200:
            tr_data = r_tr.json()
            releases = tr_data.get("releases", [])
            if releases:
                rel = releases[0]
                rel["releaseNotes"] = notes_list
                r_put_tr = play_request("PUT", f"{BASE_URL}/edits/{edit_id}/tracks/{track_name}", token, json_data=tr_data)
                if r_put_tr.status_code == 200:
                    print(f"  [OK] Updated '{track_name}' track (versionCodes={rel.get('versionCodes')}, name={rel.get('name')}) with {len(notes_list)} localized releaseNotes!")
                else:
                    print(f"  [WARN] Failed to update '{track_name}' track releaseNotes: HTTP {r_put_tr.status_code} {r_put_tr.text[:240]}")

    # 6. Upload Screenshots (phoneScreenshots, sevenInchScreenshots, tenInchScreenshots) for all synced locales
    if not skip_screenshots:
        print(f"\n--- Uploading Phone, 7-inch & 10-inch Tablet Screenshots across {len(synced_locales)} Locales ---")
        image_types_map = lambda p_paths, t_paths: [
            ("phoneScreenshots", p_paths),
            ("sevenInchScreenshots", t_paths),
            ("tenInchScreenshots", t_paths),
        ]

        for idx_loc, locale in enumerate(synced_locales, 1):
            _, prefix = PLAY_LOCALE_CONFIG[locale]
            p_paths, t_paths = rendered_sets[prefix]
            ok_imgs = 0
            for img_type, paths in image_types_map(p_paths, t_paths):
                # Delete existing images in this slot
                play_request("DELETE", f"{BASE_URL}/edits/{edit_id}/listings/{locale}/{img_type}", token)
                for fpath in paths:
                    with open(fpath, "rb") as f:
                        png_bytes = f.read()
                    r_up = play_request(
                        "POST",
                        f"{UPLOAD_BASE_URL}/edits/{edit_id}/listings/{locale}/{img_type}?uploadType=media",
                        token,
                        data=png_bytes,
                        content_type="image/png",
                    )
                    if r_up is not None and r_up.status_code in (200, 201):
                        ok_imgs += 1
                    else:
                        code = r_up.status_code if r_up is not None else "None"
                        txt = r_up.text[:160] if r_up is not None else ""
                        print(f"  [ERR] [{locale}/{img_type}] Upload failed ({code}): {txt}", flush=True)
            print(f"  [{idx_loc}/{len(synced_locales)}] {locale} ({prefix}): Uploaded {ok_imgs}/15 screenshots (Phone + 7\" + 10\")", flush=True)

    # 7. Commit the Google Play Edit
    print(f"\nCommitting Google Play edit {edit_id}...")
    r_commit = play_request("POST", f"{BASE_URL}/edits/{edit_id}:commit", token)
    if r_commit.status_code == 200:
        print(f"=== Successfully committed all listings, release notes, and screenshots to Google Play Console! ===")
    else:
        raise RuntimeError(f"Failed to commit edit {edit_id}: HTTP {r_commit.status_code} {r_commit.text}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Sync Google Play Store localized metadata, release notes, and screenshots")
    parser.add_argument("--out-dir", default=os.path.join(REPO_ROOT, "build_screenshots_android"), help="Directory for generated Android PNGs")
    parser.add_argument("--key", default=DEFAULT_KEY_PATH, help="Service Account JSON key path")
    parser.add_argument("--skip-screenshots", action="store_true", help="Sync only text listings and release notes")
    args = parser.parse_args()

    sync_play_store(out_dir=args.out_dir, key_path=args.key, skip_screenshots=args.skip_screenshots)
