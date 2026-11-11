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

AR_WHATS_NEW = """شكراً لاستخدامك miu. يعزّز الإصدار 1.0.51 الأمان والاستقرار والدقة:
• حماية المعاينة الفورية دون اتصال: سياسة CSP صارمة وحظر الشبكة الخارجية وتنقية HTML وعرض آمن للملفات التي تبدأ بـ (<) وتحليل سريع لـ Markdown.
• دقة تمييز الصياغة وMermaid: تحسين علامات الاقتباس الثلاثية في Python والتعليقات في 65 فئة لغوية واستقرار مخططات stateDiagram-v2 وblock-beta.
• تحسين حذف الرموز التعبيرية عبر لوحة المفاتيح ودقة عرض الخطوط."""

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

HE_WHATS_NEW = """תודה שאתם משתמשים ב-miu. גרסה 1.0.51 משפרת את האבטחה, היציבות והדיוק:
• הגנת תצוגה מקדימה אופליין: מדיניות CSP מחמירה, חסימת רשת חיצונית, סינון HTML/URL, הצגה בטוחה של קבצים המתחילים ב-(<) וניתוח מהיר של Markdown.
• דיוק תחביר ו-Mermaid: שיפור מרכאות משולשות ב-Python והערות בלוק ב-65 קטגוריות שפה ויציבות דיאגרמות stateDiagram-v2 ו-block-beta.
• תיקון מחיקת אימוג'י במקלדת ושיפור חדות הצגת גופנים."""

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

SK_WHATS_NEW = """Ďakujeme, že používate miu. Verzia 1.0.51 zvyšuje bezpečnosť, presnosť a stabilitu:
• Ochrana živého náhľadu: prísna offline CSP, blokovanie externej siete, čistenie HTML/URL, bezpečné zobrazenie textov od „<“ a rýchly Markdown.
• Presnejšia syntax a Mermaid: lepšie trojité úvodzovky Pythonu a blokové komentáre (65 kategórií) a stabilita stateDiagram-v2 a block-beta.
• Opravené mazanie emoji v klávesnici a ostrejšie písmo."""

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


# Localized Android Release Notes for v1.0.51 (versionCode 58), strictly <= 500 chars each
ANDROID_RELEASE_NOTES = {
    "ja-JP": """いつも miu をご利用いただきありがとうございます。v1.0.51 では安全性・描画精度・入力安定性を強化しました。
・ライブプレビューのオフライン保護強化：厳格な CSP・外部通信遮断・HTMLサニタイズ・チェックリストの暗号検証を適用し、「<」で始まるテキストの安全な表示と高速 Markdown 解析に対応。
・全65言語シンタックス＆Mermaid精度向上：Python 三重引用符・ブロックコメント解析、CRLF コードブロック高速強調、stateDiagram-v2・block-beta の安定性を改善。
・絵文字・サロゲートペアの IME 削除修正とフォント描画精度の向上。""",

    "en-US": """Thank you for using miu. Version 1.0.51 enhances offline security, precision & stability:
• Live Preview Security: Strict offline CSP, network blocking, HTML/URL sanitization, safe display of '<'-prefixed text files & fast O(N) Markdown parsing.
• Syntax & Mermaid Precision: Improved Python triple quotes & block comments across 65 categories, fast CRLF code-fence highlighting, and stateDiagram-v2 & block-beta stability.
• Fixed emoji IME deletion & sharper glyph rendering.""",

    "zh-CN": """感谢您使用 miu。v1.0.51 全面强化了离线安全防护、渲染精度与输入稳定性：
・实时预览离线安全加固：启用严格离线 CSP、拦截外部网络请求、过滤 HTML/URL、任务列表勾选加密校验，安全显示以 `<` 开头的文本并加速 Markdown 解析。
・65 种语言语法高亮与 Mermaid 精度提升：优化 Python 三引号与块注释识别、CRLF 未闭合代码块高速高亮及 stateDiagram-v2、block-beta 稳定性。
・修复表情符号与四字节字符输入法退格删除问题，并优化字体渲染清晰度。""",

    "zh-TW": """感謝您使用 miu。v1.0.51 全面強化了離線安全防護、渲染精準度與輸入穩定性：
・即時預覽離線安全加固：啟用嚴格離線 CSP、攔截外部網路請求、過濾 HTML/URL、工作清單核取加密驗證，安全顯示以 `<` 開頭的文字並加速 Markdown 解析。
・65 種語言語法突顯與 Mermaid 精準度提升：最佳化 Python 三引號與區塊註解識別、CRLF 未閉合程式碼區塊高速突顯及 stateDiagram-v2、block-beta 穩定性。
・修復表情符號與四位元組字元輸入法退格刪除問題，並最佳化字體渲染清晰度。""",

    "ko-KR": """언제나 miu를 이용해 주셔서 감사합니다. v1.0.51에서는 오프라인 보안·렌더링 정밀도·입력 안정성을 강화했습니다.
・실시간 미리보기 오프라인 보안 강화: 엄격한 CSP·외부 네트워크 차단·HTML 살균·체크리스트 암호화 검증을 적용하고 '<'로 시작하는 텍스트 안전 표시 및 고속 Markdown 파싱 지원.
・65개 언어 구문 강조 및 Mermaid 정밀도 향상: Python 삼중 따옴표·블록 주석 인식, CRLF 코드 블록 고속 강조 및 stateDiagram-v2·block-beta 안정성 개선.
・이모지(서로게이트 페어) IME 삭제 수정 및 글꼴 렌더링 선명도 향상.""",

    "de-DE": """Vielen Dank, dass Sie miu nutzen. Version 1.0.51 verbessert Sicherheit, Präzision & Stabilität:
• Live-Vorschau-Schutz: Strenge Offline-CSP, Netzwerkblockierung, HTML/URL-Bereinigung, sichere Anzeige von Texten mit „<“ und schnelles Markdown-Parsing.
• Syntax & Mermaid: Bessere Python-Dreifachzitate & Blockkommentare (65 Kategorien), schnelles CRLF-Codeblock-Highlighting sowie stabilere stateDiagram-v2 & block-beta.
• Emoji-Löschen per Tastatur korrigiert & schärfere Schrift.""",

    "fr-FR": """Merci d’utiliser miu. La version 1.0.51 renforce sécurité hors ligne, précision et stabilité :
• Aperçu en direct sécurisé : CSP stricte hors ligne, blocage réseau, assainissement HTML/URL, affichage sûr des textes débutant par « < » et Markdown rapide.
• Syntaxe & Mermaid : meilleurs triples guillemets Python et commentaires de bloc (65 langages) et stabilité accrue de stateDiagram-v2 et block-beta.
• Suppression d’emojis au clavier corrigée et police plus nette.""",

    "es-ES": """Gracias por usar miu. La versión 1.0.51 refuerza la seguridad sin conexión, precisión y estabilidad:
• Vista Previa segura: CSP estricta sin conexión, bloqueo de red, saneamiento HTML/URL, vista segura de textos iniciados con '<' y análisis rápido de Markdown.
• Sintaxis y Mermaid: mejoras en comillas triples Python y comentarios de bloque (65 lenguajes) y estabilidad en stateDiagram-v2 y block-beta.
• Borrado de emojis en teclado corregido y fuentes más nítidas.""",

    "it-IT": """Grazie per aver scelto miu. La versione 1.0.51 migliora sicurezza offline, precisione e stabilità:
• Anteprima sicura: CSP offline rigorosa, blocco rete esterna, sanificazione HTML/URL, vista sicura dei testi che iniziano con '<' e Markdown veloce.
• Sintassi e Mermaid: migliorate triple virgolette Python e commenti a blocco (65 linguaggi) e stabilità di stateDiagram-v2 e block-beta.
• Cancellazione di emoji da tastiera corretta e caratteri più nitidi.""",

    "pt-BR": """Obrigado por usar o miu. A versão 1.0.51 reforça a segurança offline, precisão e estabilidade:
• Segurança na Pré-visualização: CSP offline rigorosa, bloqueio de rede externa, sanitização HTML/URL, exibição segura de textos iniciados por '<' e Markdown rápido.
• Sintaxe e Mermaid: melhorias em aspas triplas Python e comentários de bloco (65 categorias) e estabilidade em stateDiagram-v2 e block-beta.
• Exclusão de emojis no teclado corrigida e fontes mais nítidas.""",

    "ru-RU": """Спасибо, что пользуетесь miu. В версии 1.0.51 усилены офлайн-безопасность, точность и стабильность:
• Защита предпросмотра: строгая CSP, блокировка внешних запросов, очистка HTML/URL, безопасный показ текстов с «<» и быстрый разбор Markdown.
• Синтаксис и Mermaid: улучшен разбор тройных кавычек Python и блочных комментариев (65 категорий языков), повышена стабильность stateDiagram-v2 и block-beta.
• Исправлено удаление эмодзи при вводе с клавиатуры и улучшена чёткость шрифтов.""",

    "nl-NL": """Bedankt voor het gebruiken van miu. Versie 1.0.51 verbetert offline beveiliging, precisie en stabiliteit:
• Beveiliging Live Voorbeeld: strikte offline CSP, netwerkblokkering, HTML/URL-opschoning, veilige weergave van teksten beginnend met '<' en snelle Markdown-verwerking.
• Syntax & Mermaid: betere Python-triple-quotes en blokcommentaar in 65 taalcategorieën en stabielere stateDiagram-v2 & block-beta.
• Emoji-verwijdering via toetsenbord hersteld en scherpere lettertypeweergave.""",

    "sv-SE": """Tack för att du använder miu. Version 1.0.51 stärker offlinesäkerhet, precision och stabilitet:
• Säker förhandsgranskning: strikt offline-CSP, blockering av externa anrop, HTML/URL-sanering, säker visning av texter som börjar med '<' och snabb Markdown-tolkning.
• Syntax & Mermaid: bättre trippelcitat i Python och blockkommentarer i 65 språkkategorier samt stabilare stateDiagram-v2 och block-beta.
• Åtgärdad borttagning av emojis via tangentbord och skarpare teckenåtergivning.""",

    "da-DK": """Tak fordi du bruger miu. Version 1.0.51 styrker offlinesikkerhed, præcision og stabilitet:
• Sikker forhåndsvisning: striks offline-CSP, blokering af eksterne netværkskald, HTML/URL-rensning, sikker visning af tekster der starter med '<' og hurtig Markdown-analyse.
• Syntaks & Mermaid: bedre tredobbelte anførselstegn i Python og blokkommentarer i 65 sprogkategorier samt mere stabile stateDiagram-v2 og block-beta.
• Rettet sletning af emojis via tastatur og skarpere skriftgengivelse.""",

    "no-NO": """Takk for at du bruker miu. Versjon 1.0.51 styrker offlinesikkerhet, presisjon og stabilitet:
• Sikker forhåndsvisning: streng offline-CSP, blokkering av eksterne nettverkskall, HTML/URL-rensing, trygg visning av tekster som starter med «<» og rask Markdown-analyse.
• Syntaks og Mermaid: bedre trippelanførselstegn i Python og blokkommentarer i 65 språkkategorier samt mer stabile stateDiagram-v2 og block-beta.
• Rettet sletting av emojier via tastatur og skarpere skriftgjengivelse.""",

    "fi-FI": """Kiitos, että käytät miuta. Versio 1.0.51 parantaa offline-tietoturvaa, tarkkuutta ja vakautta:
• Esikatselun suoja: tiukka offline-CSP, ulkoisten yhteyksien esto, HTML/URL-puhdistus, '<'-alkuisten tekstien turvallinen näyttö ja nopea Markdown-jäsennys.
• Syntaksi ja Mermaid: parempi Pythonin kolmoislainausmerkkien ja lohkokommenttien tuki (65 kielikategoriaa) sekä vakaammat stateDiagram-v2 ja block-beta.
• Korjattu emojien poisto näppäimistöllä ja terävöitetty fonttien piirtoa.""",

    "pl-PL": """Dziękujemy za korzystanie z miu. Wersja 1.0.51 wzmacnia bezpieczeństwo offline, precyzję i stabilność:
• Ochrona podglądu na żywo: ścisła CSP offline, blokada sieci zewnętrznej, czyszczenie HTML/URL, bezpieczne wyświetlanie tekstów od „<” i szybkie parsowanie Markdown.
• Składnia i Mermaid: ulepszone potrójne cudzysłowy Pythona i komentarze blokowe (65 kategorii) oraz stabilniejsze stateDiagram-v2 i block-beta.
• Poprawione usuwanie emoji z klawiatury i wyraźniejsze renderowanie czcionek.""",

    "cs-CZ": """Děkujeme, že používáte miu. Verze 1.0.51 posiluje offline zabezpečení, přesnost a stabilitu:
• Ochrana živého náhledu: přísná offline CSP, blokování externí sítě, čištění HTML/URL, bezpečné zobrazení textů začínajících „<“ a rychlé zpracování Markdownu.
• Syntaxe a Mermaid: lepší trojité uvozovky v Pythonu a blokové komentáře (65 kategorií jazyků) a stabilnější stateDiagram-v2 a block-beta.
• Opraveno mazání emoji na klávesnici a ostřejší vykreslování písma.""",

    "sk": SK_WHATS_NEW,

    "hu-HU": """Köszönjük, hogy a miu alkalmazást használja. Az 1.0.51-es verzió növeli az offline biztonságot és a pontosságot:
• Élő előnézet védelme: szigorú offline CSP, külső hálózat tiltása, HTML/URL-tisztítás, a „<” jellel kezdődő szövegek biztonságos megjelenítése és gyors Markdown-elemzés.
• Szintaxis és Mermaid: pontosabb Python tripla idézőjelek és blokk-kommentek (65 nyelvi kategória), stabilabb stateDiagram-v2 és block-beta.
• Javított emoji-törlés billentyűzeten és élesebb betűmegjelenítés.""",

    "ro": """Vă mulțumim că folosiți miu. Versiunea 1.0.51 crește securitatea offline, precizia și stabilitatea:
• Protecție Previzualizare: CSP offline strictă, blocare rețea externă, igienizare HTML/URL, afișare sigură a textelor cu „<” și Markdown rapid.
• Sintaxă și Mermaid: ghilimele triple Python și comentarii bloc îmbunătățite (65 de categorii) și stabilitate în stateDiagram-v2 și block-beta.
• Ștergere corectă a emoji-urilor din tastatură și fonturi mai clare.""",

    "el-GR": """Σας ευχαριστούμε που χρησιμοποιείτε το miu. Η έκδοση 1.0.51 ενισχύει την ασφάλεια, την ακρίβεια και τη σταθερότητα:
• Ασφάλεια προεπισκόπησης: αυστηρή CSP εκτός σύνδεσης, φραγή δικτύου, καθαρισμός HTML/URL, ασφαλής εμφάνιση κειμένων με «<» και γρήγορη ανάλυση Markdown.
• Σύνταξη & Mermaid: βελτιωμένα τριπλά εισαγωγικά Python και σχόλια μπλοκ (65 κατηγορίες) και σταθερότητα σε stateDiagram-v2 & block-beta.
• Διόρθωση διαγραφής emoji στο πληκτρολόγιο και ευκρινέστερη απόδοση γραμματοσειρών.""",

    "tr-TR": """miu'yu kullandığınız için teşekkür ederiz. 1.0.51 sürümü çevrimdışı güvenliği, hassasiyeti ve kararlılığı artırır:
• Canlı Önizleme Güvenliği: Sıkı çevrimdışı CSP, dış ağ engelleme, HTML/URL temizleme, '<' ile başlayan metinlerin güvenli gösterimi ve hızlı Markdown ayrıştırma.
• Sözdizimi ve Mermaid: 65 dil kategorisinde Python üçlü tırnak ve blok yorum iyileştirmeleri ile stateDiagram-v2 ve block-beta kararlılığı.
• Klavyede emoji silme düzeltmesi ve daha net yazı tipi görünümü.""",

    "uk": """Дякуємо, що користуєтеся miu. У версії 1.0.51 посилено офлайн-безпеку, точність і стабільність:
• Захист перегляду наживо: сувора CSP, блокування зовнішньої мережі, очищення HTML/URL, безпечний показ текстів із «<» та швидкий розбір Markdown.
• Синтаксис і Mermaid: покращено потрійні лапки Python і блокові коментарі (65 категорій мов) та стабільність stateDiagram-v2 і block-beta.
• Виправлено видалення емодзі з клавіатури та покращено чіткість шрифтів.""",

    "vi": """Cảm ơn bạn đã sử dụng miu. Phiên bản 1.0.51 tăng cường bảo mật ngoại tuyến, độ chính xác và ổn định:
• Bảo mật Xem trước: CSP ngoại tuyến nghiêm ngặt, chặn mạng ngoài, làm sạch HTML/URL, hiển thị an toàn văn bản bắt đầu bằng '<' và phân tích Markdown nhanh.
• Cú pháp & Mermaid: cải thiện chuỗi ba dấu nháy Python và chú thích khối (65 nhóm ngôn ngữ) cùng độ ổn định stateDiagram-v2, block-beta.
• Sửa lỗi xóa emoji trên bàn phím và hiển thị phông chữ sắc nét hơn.""",

    "th": """ขอบคุณที่ใช้บริการ miu เวอร์ชัน 1.0.51 ยกระดับความปลอดภัยออฟไลน์ ความแม่นยำ และความเสถียร:
• ความปลอดภัยของพรีวิวสด: CSP ออฟไลน์เข้มงวด บล็อกเครือข่ายภายนอก คัดกรอง HTML/URL แสดงข้อความที่ขึ้นต้นด้วย '<' อย่างปลอดภัย และแปลง Markdown เร็วขึ้น
• ไวยากรณ์และ Mermaid: ปรับปรุงอัญประกาศสามชั้นของ Python และความคิดเห็นบล็อก (65 หมวดหมู่ภาษา) พร้อมความเสถียรของ stateDiagram-v2 และ block-beta
• แก้ไขการลบอีโมจิด้วยแป้นพิมพ์และแสดงฟอนต์คมชัดยิ่งขึ้น""",

    "id": """Terima kasih telah menggunakan miu. Versi 1.0.51 meningkatkan keamanan offline, akurasi, dan stabilitas:
• Keamanan Pratinjau: CSP offline ketat, blokir jaringan eksternal, sanitasi HTML/URL, tampilan aman teks berawalan '<', dan penguraian Markdown cepat.
• Sintaks & Mermaid: perbaikan tanda kutip tiga Python & komentar blok (65 kategori bahasa) serta stabilitas stateDiagram-v2 & block-beta.
• Perbaikan penghapusan emoji pada keyboard dan tampilan font lebih tajam.""",

    "ms": """Terima kasih kerana menggunakan miu. Versi 1.0.51 memperkukuh keselamatan luar talian, ketepatan dan kestabilan:
• Keselamatan Pratonton: CSP luar talian ketat, sekatan rangkaian luaran, pembersihan HTML/URL, paparan selamat teks bermula '<' dan penghuraian Markdown pantas.
• Sintaks & Mermaid: penambahbaikan petikan tiga kali Python & komen blok (65 kategori) serta kestabilan stateDiagram-v2 & block-beta.
• Pembaikan pemadaman emoji papan kekunci dan paparan fon lebih tajam.""",

    "hi-IN": """miu का उपयोग करने के लिए धन्यवाद। संस्करण 1.0.51 ऑफ़लाइन सुरक्षा, सटीकता और स्थिरता को बढ़ाता है:
• लाइव प्रीव्यू सुरक्षा: सख्त ऑफ़लाइन CSP, बाहरी नेटवर्क रोक, HTML/URL स्वच्छता, '<' से शुरू होने वाले टेक्स्ट का सुरक्षित प्रदर्शन और तेज़ Markdown पार्सिंग।
• सिंटैक्स व Mermaid: 65 भाषा श्रेणियों में Python ट्रिपल-कोट्स व ब्लॉक कमेंट सुधार तथा stateDiagram-v2 और block-beta स्थिरता।
• कीबोर्ड से इमोजी हटाने में सुधार और अधिक स्पष्ट फ़ॉन्ट रेंडरिंग।""",

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
