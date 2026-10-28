#pragma once
#include "text.h"

namespace Pele {

// ============================================================================
// src/svg/builder.ts
// ============================================================================

inline bool isInvalidXmlCodepoint(uint32_t cp) noexcept {
    return (cp <= 0x08) || cp == 0x0b || cp == 0x0c || (cp >= 0x0e && cp <= 0x1f) || cp == 0x7f || (cp >= 0xd800 && cp <= 0xdfff) || cp == 0xfffe || cp == 0xffff;
}

inline bool hasInvalidXml(const std::string& text) noexcept {
    size_t i = 0;
    while (i < text.size()) {
        uint32_t cp = nextCodepoint(text, i);
        if (isInvalidXmlCodepoint(cp)) return true;
    }
    return false;
}

PELE_NOINLINE inline std::string esc(const std::string& text) noexcept {
    std::string out;
    out.reserve(text.size() + 8);
    size_t i = 0;
    while (i < text.size()) {
        size_t s = i;
        uint32_t cp = nextCodepoint(text, i);
        if (isInvalidXmlCodepoint(cp)) continue;
        if (cp == '&') out += "&amp;";
        else if (cp == '<') out += "&lt;";
        else if (cp == '>') out += "&gt;";
        else if (cp == '"') out += "&quot;";
        else out.append(text, s, i - s);
    }
    return out;
}

PELE_NOINLINE inline std::string escText(const std::string& text) noexcept {
    return esc(decodeEntities(text));
}

PELE_NOINLINE inline std::string num(double value) noexcept {
    if (!std::isfinite(value)) return "0";
    long long n = static_cast<long long>(std::floor(value * 100.0 + 0.5));
    if (n == 0) return "0";
    char buf[32];
    char* end = buf + sizeof(buf);
    char* p = end;
    unsigned long long m = (n < 0) ? (0ULL - static_cast<unsigned long long>(n)) : static_cast<unsigned long long>(n);
    unsigned long long q = m / 100ULL;
    unsigned int r = static_cast<unsigned int>(m % 100ULL);
    if (r != 0) {
        if (r % 10 == 0) {
            *--p = static_cast<char>('0' + (r / 10));
        } else {
            *--p = static_cast<char>('0' + (r % 10));
            *--p = static_cast<char>('0' + (r / 10));
        }
        *--p = '.';
    }
    if (q == 0) {
        *--p = '0';
    } else {
        while (q > 0) {
            *--p = static_cast<char>('0' + (q % 10ULL));
            q /= 10ULL;
        }
    }
    if (n < 0) *--p = '-';
    return std::string(p, static_cast<size_t>(end - p));
}

inline std::string spanStyle(int style) noexcept {
    std::string out;
    if (style & Style::Bold) out += " font-weight=\"var(--_w)\"";
    if (style & Style::Italic) out += " font-style=\"italic\"";
    if (style & Style::Mono) out += " font-family=\"var(--_fm)\"";
    if (style & Style::Underline) out += " text-decoration=\"underline\"";
    if (style & Style::Sub) out += " baseline-shift=\"sub\" font-size=\"smaller\"";
    else if (style & Style::Sup) out += " baseline-shift=\"super\" font-size=\"smaller\"";
    return out;
}

inline int sharedStyle(const Label& label) noexcept {
    int shared = Style::Bold | Style::Italic | Style::Mono;
    bool any = false;
    for (const auto& line : label.lines) {
        for (const auto& span : line) {
            if (span.hasIcon || span.text.empty()) continue;
            shared &= span.style;
            any = true;
        }
    }
    return any ? shared : 0;
}

inline std::string textStyle(const std::string& attrs, int shared) noexcept {
    std::string out = attrs;
    if ((shared & Style::Bold) && attrs.find(" font-weight=\"") == std::string::npos) out += " font-weight=\"var(--_w)\"";
    if ((shared & Style::Italic) && attrs.find(" font-style=\"") == std::string::npos) out += " font-style=\"italic\"";
    if ((shared & Style::Mono) && attrs.find(" font-family=\"") == std::string::npos) out += " font-family=\"var(--_fm)\"";
    return out;
}

struct MathLook {
    std::string attrs;
    std::string halo;
    bool hasHalo = false;
};

inline std::string extractAttrVal(const std::string& s, const std::string& key) noexcept {
    std::string needle = Pele::cat({" ", key, "=\""});
    size_t pos = s.find(needle);
    if (pos == std::string::npos) return "";
    size_t start = pos + needle.size();
    size_t end = s.find('"', start);
    if (end == std::string::npos) return "";
    return s.substr(start, end - start);
}

inline std::string extractCssProp(const std::string& css, const std::string& prop) noexcept {
    for (const auto& decl : splitStr(css, ';')) {
        size_t colon = decl.find(':');
        if (colon == std::string::npos) continue;
        if (trimStr(decl.substr(0, colon)) == prop) {
            return trimStr(decl.substr(colon + 1));
        }
    }
    return "";
}

inline MathLook computeMathLook(const std::string& attrs) noexcept {
    std::string style = extractAttrVal(attrs, "style");
    std::string fill = extractCssProp(style, "fill");
    if (fill.empty()) fill = trimStr(extractAttrVal(attrs, "fill"));
    if (fill == "none") {
        std::string stroke = extractAttrVal(attrs, "stroke");
        return {"", stroke.empty() ? "none" : stroke, true};
    }
    std::string opacity = extractCssProp(style, "opacity");
    std::string res = (fill.empty() ? "" : Pele::cat({" color=\"", fill, "\""})) + (opacity.empty() ? "" : Pele::cat({" opacity=\"", opacity, "\""}));
    return {res, "", false};
}

inline std::string mathSvg(const MathMarkup& math, const std::string& box, double size, const std::string& attrs) noexcept {
    if (math.hasSvg) {
        return Pele::cat({"<svg class=\"pele-math\" ", box, " overflow=\"visible\"", attrs, ">", math.svg, "</svg>"});
    }
    return "<foreignObject class=\"pele-math\" " + box + " overflow=\"visible\"" + attrs + ">"
        "<div xmlns=\"http://www.w3.org/1999/xhtml\" style=\"display:flex;align-items:center;justify-content:center;width:100%;height:100%;font-size:" + num(size) + "px;line-height:normal;white-space:nowrap\">" + math.html + "</div>"
        "</foreignObject>";
}

inline std::string labelSvg(const Label& label, double cx, double cy, std::string attrs, const IconResolver& icons = nullptr) noexcept {
    const auto& lines = label.lines;
    if (lines.empty()) return "";
    double lh = label.lineHeight;
    double shift = label.size * 0.35;
    double top = cy - label.height / 2.0;

    if (lines.size() == 1 && lines[0].size() == 1) {
        const auto& only = lines[0][0];
        if (!only.hasIcon && !only.hasMath && only.style == 0) {
            return Pele::cat({"<text", attrs, " x=\"", num(cx), "\" y=\"", num(top + lh / 2.0 + shift), "\" text-anchor=\"middle\">", esc(only.text), "</text>"});
        }
    }

    int shared = sharedStyle(label);
    std::string given = attrs;
    bool hasMathLook = false;
    MathLook mLook;
    attrs = textStyle(attrs, shared);
    std::string body;
    std::string extra;
    for (size_t i = 0; i < lines.size(); i++) {
        const auto& line = lines[i];
        double h = label.hasHeights ? label.heights[i] : lh;
        double mid = top + h / 2.0;
        double y = mid + shift;
        bool placed = false;
        for (const auto& span : line) {
            if (span.hasIcon || span.hasMath) { placed = true; break; }
        }
        if (placed) {
            double x = cx - label.widths[i] / 2.0;
            for (const auto& span : line) {
                if (span.hasMath) {
                    std::string box = Pele::cat({"x=\"", num(x), "\" y=\"", num(mid - span.math.height / 2.0), "\" width=\"", num(span.math.width), "\" height=\"", num(span.math.height), "\""});
                    if (!hasMathLook) { mLook = computeMathLook(given); hasMathLook = true; }
                    extra += !mLook.hasHalo ? mathSvg(span.math, box, label.size, mLook.attrs) : Pele::cat({"<rect ", box, " rx=\"2\" fill=\"", mLook.halo, "\"/>"});
                } else if (span.hasIcon) {
                    double side = label.size * 1.1;
                    std::string inner = icons ? icons(span.icon) : "";
                    Pele::appendCat(extra, {"<svg class=\"pele-icon\" data-icon=\"", escText(span.icon), "\" x=\"", num(x), "\" y=\"", num(mid - side / 2.0), "\" width=\"", num(side), "\" height=\"", num(side), "\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\">", inner, "</svg>"});
                } else if (!span.text.empty()) {
                    Pele::appendCat(body, {"<tspan x=\"", num(x), "\" y=\"", num(y), "\" text-anchor=\"start\"", spanStyle(span.style & ~shared), ">", esc(span.text), "</tspan>"});
                }
                x += span.width;
            }
        } else {
            for (size_t j = 0; j < line.size(); j++) {
                const auto& span = line[j];
                std::string pos = (j == 0) ? Pele::cat({" x=\"", num(cx), "\" y=\"", num(y), "\""}) : "";
                Pele::appendCat(body, {"<tspan", pos, spanStyle(span.style & ~shared), ">", esc(span.text), "</tspan>"});
            }
            if (line.empty()) Pele::appendCat(body, {"<tspan x=\"", num(cx), "\" y=\"", num(y), "\"> </tspan>"});
        }
        top += h;
    }
    return Pele::cat({"<text", attrs, " text-anchor=\"middle\" xml:space=\"preserve\">", body, "</text>", extra});
}

inline std::string iconSvg(const std::string& name, double x, double y, double side, const IconResolver& icons = nullptr) noexcept {
    return Pele::cat({"<svg class=\"pele-icon\" data-icon=\"", escText(name), "\" x=\"", num(x), "\" y=\"", num(y), "\" width=\"", num(side), "\" height=\"", num(side), "\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\">", (icons ? icons(name) : ""), "</svg>"});
}

inline std::string leftLabelSvg(const Label& label, double x, double top, const std::string& attrs) noexcept {
    int shared = sharedStyle(label);
    std::string open = Pele::cat({"<text", textStyle(attrs, shared)});
    std::string body;
    for (size_t i = 0; i < label.lines.size(); i++) {
        const auto& spans = label.lines[i];
        double cy = top + (static_cast<double>(i) + 0.5) * label.lineHeight;
        bool anyIcon = false;
        for (const auto& s : spans) if (s.hasIcon) { anyIcon = true; break; }
        if (anyIcon) {
            double width = label.widths[i];
            Label sub = label;
            sub.lines = {spans};
            sub.widths = {width};
            sub.width = width;
            sub.height = label.lineHeight;
            body += labelSvg(sub, x + width / 2.0, cy, attrs);
            continue;
        }
        std::string text;
        for (const auto& span : spans) {
            std::string own = spanStyle(span.style & ~shared);
            text += !own.empty() ? Pele::cat({"<tspan", own, ">", esc(span.text), "</tspan>"}) : esc(span.text);
        }
        if (!text.empty()) {
            Pele::appendCat(body, {open, " x=\"", num(x), "\" y=\"", num(cy + label.size * 0.35), "\" text-anchor=\"start\" xml:space=\"preserve\">", text, "</text>"});
        }
    }
    return body;
}

// ============================================================================
// src/svg/theme.ts
// ============================================================================

inline const std::string ROOT_STYLE =
    "--_bg:var(--pele-bg,#fff);--_fg:var(--pele-fg,#1f1f1f);--_m:var(--pele-muted,#6e6e6e);"
    "--_l:var(--pele-line,#1f1f1f);--_s:var(--pele-surface,#f3f3f3);--_a:var(--pele-surface-alt,#e6e6e6);"
    "--_b:var(--pele-border,#8a8a8a);--_c:var(--pele-accent,#5b7bd5);color:var(--_fg)";

inline const std::string FONT_MONO = "var(--_fm)";
inline const std::string RADIUS = "var(--pele-radius,4px)";
inline const std::string ROUNDED = " style=\"rx:var(--_r)\"";
inline const std::string CRITICAL = "var(--pele-critical,#e45756)";

inline bool setsFill(const std::string& shape) noexcept {
    return shape.find("\"fill:") != std::string::npos || shape.find(";fill:") != std::string::npos;
}

inline std::string rounded(const std::string& attrs) noexcept {
    size_t at = attrs.find(" style=\"");
    if (at == std::string::npos) return attrs + ROUNDED;
    return Pele::cat({attrs.substr(0, at + 8), "rx:var(--_r);", attrs.substr(at + 8)});
}

inline bool isSafeFontFamilies(const std::string& list) noexcept {
    if (trimStr(list).empty()) return false;
    for (char c : list) {
        if (c == '\\' || c == '\n' || c == '\r' || c == '<' || c == '>' || c == '{' || c == '}' || c == '(' || c == ')' || c == ';' || c == ':' || c == '@') return false;
    }
    return true;
}

inline std::string fontFamilies(const std::string& list, const std::string& generic) noexcept {
    return isSafeFontFamilies(list) ? list : generic;
}

inline std::string fontStyle(double size, const std::string& family, const std::string& mono) noexcept {
    return Pele::cat({";--_fm:var(--pele-font-mono,", fontFamilies(mono, "monospace"), ");font:", num(size), "px sans-serif;"
        "letter-spacing:normal;word-spacing:normal;text-transform:none;direction:ltr;"
        "font-family:var(--pele-font,", fontFamilies(family, "sans-serif"), ")"});
}

struct ResolvedStyle {
    std::string shape;
    std::string line;
    std::string text;
    std::string stroke;
    bool hasStroke = false;
    bool bold = false;
    bool italic = false;
    double fontSize = 0.0;
    bool hasFontSize = false;
};

inline bool isUnsafeCssValue(const std::string& v) noexcept {
    std::string low = toLowerAscii(v);
    if (low.find("url(") != std::string::npos || low.find("url ") != std::string::npos ||
        low.find("expression") != std::string::npos || low.find("javascript:") != std::string::npos ||
        low.find("@import") != std::string::npos || low.find("image-set") != std::string::npos ||
        low.find("/*") != std::string::npos) return true;
    for (char c : v) {
        if (c == '<' || c == '>' || c == '{' || c == '}' || c == ';' || c == '\\') return true;
    }
    return hasInvalidXml(v);
}

inline ResolvedStyle resolveStyle(const std::vector<std::string>& declarations) noexcept {
    ResolvedStyle none;
    if (declarations.empty()) return none;
    static const Pele::FlatSet<std::string> SHAPE_PROPS = {
        "fill", "fill-opacity", "stroke", "stroke-width", "stroke-dasharray",
        "stroke-dashoffset", "stroke-linecap", "stroke-linejoin", "stroke-opacity",
        "opacity", "rx", "ry"
    };
    static const Pele::FlatSet<std::string> TEXT_PROPS = {
        "color", "font-size", "font-family", "font-weight", "font-style",
        "text-decoration", "letter-spacing", "word-spacing", "opacity"
    };

    std::vector<std::string> shapeOrder;
    Pele::FlatMap<std::string, std::string> shape;
    std::vector<std::string> textOrder;
    Pele::FlatMap<std::string, std::string> text;

    auto setMap = [](std::vector<std::string>& order, Pele::FlatMap<std::string, std::string>& m, const std::string& k, const std::string& v) {
        if (!m.count(k)) order.push_back(k);
        m[k] = v;
    };

    for (const auto& decl : declarations) {
        size_t at = decl.find(':');
        if (at == std::string::npos || at == 0) continue;
        std::string prop = toLowerAscii(trimStr(decl.substr(0, at)));
        std::string val = decl.substr(at + 1);
        // strip !important case-insensitively
        std::string lowVal = toLowerAscii(val);
        size_t impPos = 0;
        while ((impPos = lowVal.find("!important", impPos)) != std::string::npos) {
            val.erase(impPos, 10);
            lowVal.erase(impPos, 10);
        }
        val = trimStr(val);
        if (val.empty() || isUnsafeCssValue(val)) continue;
        if (SHAPE_PROPS.count(prop)) setMap(shapeOrder, shape, prop, val);
        if (TEXT_PROPS.count(prop)) setMap(textOrder, text, prop == "color" ? "fill" : prop, val);
    }
    if (shape.empty() && text.empty()) return none;

    std::string shapeCss;
    std::string lineCss;
    for (const auto& prop : shapeOrder) {
        const auto& val = shape[prop];
        Pele::appendCat(shapeCss, {prop, ":", val, ";"});
        if (prop != "fill" && prop != "fill-opacity") Pele::appendCat(lineCss, {prop, ":", val, ";"});
    }
    std::string textCss;
    for (const auto& prop : textOrder) {
        Pele::appendCat(textCss, {prop, ":", text[prop], ";"});
    }

    ResolvedStyle res;
    if (!shapeCss.empty()) res.shape = " style=\"" + esc(shapeCss) + "\"";
    if (!lineCss.empty()) res.line = " style=\"" + esc(lineCss) + "\"";
    if (!textCss.empty()) res.text = " style=\"" + esc(textCss) + "\"";
    if (shape.count("stroke")) {
        res.stroke = shape["stroke"];
        res.hasStroke = true;
    }
    if (text.count("font-weight")) {
        const std::string& w = text["font-weight"];
        double wn = 0;
        try { wn = std::stod(w); } catch (...) {}
        res.bold = (w == "bold" || w == "bolder" || wn >= 600);
    }
    if (text.count("font-style")) {
        res.italic = (text["font-style"] == "italic");
    }
    if (text.count("font-size")) {
        const std::string& sz = text["font-size"];
        bool validPx = !sz.empty();
        size_t numEnd = sz.size();
        if (endsWith(sz, "px")) numEnd -= 2;
        for (size_t k = 0; k < numEnd; k++) {
            if (!isDigit(static_cast<unsigned char>(sz[k])) && sz[k] != '.') { validPx = false; break; }
        }
        if (validPx && numEnd > 0) {
            try {
                double px = std::stod(sz.substr(0, numEnd));
                if (px > 0 && px <= 200) {
                    res.fontSize = px;
                    res.hasFontSize = true;
                }
            } catch (...) {}
        }
    }
    return res;
}

inline std::string classNames(const std::string& raw) noexcept {
    std::string cleaned;
    bool inSpace = false;
    for (char c : raw) {
        if (isWord(static_cast<unsigned char>(c)) || c == '-') {
            cleaned.push_back(c);
            inSpace = false;
        } else {
            if (!inSpace && !cleaned.empty()) {
                cleaned.push_back(' ');
                inSpace = true;
            }
        }
    }
    cleaned = trimStr(cleaned);
    return cleaned.empty() ? "" : Pele::cat({" ", cleaned});
}

inline std::string seriesColor(int index) noexcept {
    static const char* SERIES[8] = {"#4c78a8", "#f58518", "#54a24b", "#e45756", "#72b7b2", "#eeca3b", "#b279a2", "#9d755d"};
    int k = ((index % 8) + 8) % 8;
    return "var(--pele-series-" + std::to_string(k + 1) + "," + SERIES[k] + ")";
}

// ============================================================================
// src/svg/ink.ts
// ============================================================================

inline const std::string ON_LIGHT = "var(--pele-fg-on-light,#1f1f1f)";
inline const std::string ON_DARK = "var(--pele-fg-on-dark,#fff)";

inline double srgbLinear(double c) noexcept {
    return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
}

inline double srgbLuminance(double r, double g, double b) noexcept {
    return 0.2126 * srgbLinear(r) + 0.7152 * srgbLinear(g) + 0.0722 * srgbLinear(b);
}

inline bool parseColorLumAlpha(const std::string& raw, double& outLum, double& outAlpha) noexcept {
    static const Pele::FlatSet<std::string> DARK = {
        "black","blue","blueviolet","brown","crimson","darkblue","darkcyan","darkgreen","darkmagenta","darkolivegreen",
        "darkorchid","darkred","darkslateblue","darkslategray","darkviolet","dimgray","firebrick","forestgreen","green",
        "indigo","maroon","mediumblue","mediumslateblue","mediumvioletred","midnightblue","navy","olive","purple",
        "rebeccapurple","royalblue","saddlebrown","seagreen","sienna","slateblue","steelblue","teal"
    };
    static const Pele::FlatSet<std::string> LIGHT = {
        "aliceblue","antiquewhite","aqua","aquamarine","azure","beige","bisque","blanchedalmond","burlywood","cadetblue",
        "chartreuse","chocolate","coral","cornflowerblue","cornsilk","cyan","darkgoldenrod","darkgray","darkkhaki",
        "darkorange","darksalmon","darkseagreen","darkturquoise","deeppink","deepskyblue","dodgerblue","floralwhite",
        "fuchsia","gainsboro","ghostwhite","gold","goldenrod","gray","greenyellow","honeydew","hotpink","indianred",
        "ivory","khaki","lavender","lavenderblush","lawngreen","lemonchiffon","lightblue","lightcoral","lightcyan",
        "lightgoldenrodyellow","lightgray","lightgreen","lightpink","lightsalmon","lightseagreen","lightskyblue",
        "lightslategray","lightsteelblue","lightyellow","lime","limegreen","linen","magenta","mediumaquamarine",
        "mediumorchid","mediumpurple","mediumseagreen","mediumspringgreen","mediumturquoise","mintcream","mistyrose",
        "moccasin","navajowhite","oldlace","olivedrab","orange","orangered","orchid","palegoldenrod","palegreen",
        "paleturquoise","palevioletred","papayawhip","peachpuff","peru","pink","plum","powderblue","red","rosybrown",
        "salmon","sandybrown","seashell","silver","skyblue","slategray","snow","springgreen","tan","thistle","tomato",
        "turquoise","violet","wheat","white","whitesmoke","yellow","yellowgreen"
    };
    std::string value = toLowerAscii(trimStr(raw));
    std::string name = replaceAllStr(value, "grey", "gray");
    if (DARK.count(name)) { outLum = 0.0; outAlpha = 1.0; return true; }
    if (LIGHT.count(name)) { outLum = 1.0; outAlpha = 1.0; return true; }
    if (!value.empty() && value[0] == '#') {
        size_t len = value.size() - 1;
        if (len == 3 || len == 4 || len == 6 || len == 8) {
            bool allHex = true;
            for (size_t i = 1; i < value.size(); i++) if (!isHex(static_cast<unsigned char>(value[i]))) { allHex = false; break; }
            if (allHex) {
                std::string digits;
                if (len < 6) {
                    for (size_t i = 1; i < value.size(); i++) { digits.push_back(value[i]); digits.push_back(value[i]); }
                } else {
                    digits = value.substr(1);
                }
                auto ch = [&](size_t i) {
                    return static_cast<double>(std::stoul(digits.substr(2 * i, 2), nullptr, 16)) / 255.0;
                };
                outLum = srgbLuminance(ch(0), ch(1), ch(2));
                outAlpha = (digits.size() == 8) ? ch(3) : 1.0;
                return true;
            }
        }
        return false;
    }
    size_t lp = value.find('(');
    if (lp != std::string::npos && value.back() == ')') {
        std::string fn = value.substr(0, lp);
        std::string body = trimStr(value.substr(lp + 1, value.size() - lp - 2));
        if (fn == "rgb" || fn == "rgba" || fn == "hsl" || fn == "hsla") {
            bool isHsl = startsWith(fn, "hsl");
            std::vector<std::string> parts;
            if (body.find(',') != std::string::npos) {
                for (const auto& p : splitStr(body, ',')) parts.push_back(trimStr(p));
            } else {
                std::string mainPart = body;
                std::string alphaPart;
                size_t sl = body.find('/');
                if (sl != std::string::npos) {
                    mainPart = body.substr(0, sl);
                    alphaPart = trimStr(body.substr(sl + 1));
                }
                std::istringstream iss(mainPart);
                std::string tok;
                while (iss >> tok) parts.push_back(tok == "none" ? "0" : tok);
                if (!alphaPart.empty()) parts.push_back(alphaPart == "none" ? "0" : alphaPart);
            }
            if (parts.size() < 3 || parts.size() > 4) return false;
            auto parseAmount = [](const std::string& s, double full) -> double {
                bool pct = endsWith(s, "%");
                try {
                    double v = std::stod(pct ? s.substr(0, s.size() - 1) : s);
                    return std::min(1.0, std::max(0.0, v / (pct ? 100.0 : full)));
                } catch (...) { return std::numeric_limits<double>::quiet_NaN(); }
            };
            double alpha = (parts.size() == 4) ? parseAmount(parts[3], 1.0) : 1.0;
            double r = 0, g = 0, b = 0;
            if (!isHsl) {
                r = parseAmount(parts[0], 255.0);
                g = parseAmount(parts[1], 255.0);
                b = parseAmount(parts[2], 255.0);
            } else {
                double hVal = 0;
                try { hVal = std::stod(parts[0]); } catch (...) { return false; }
                if (endsWith(parts[0], "turn")) hVal *= 360.0;
                else if (endsWith(parts[0], "rad")) hVal *= 180.0 / 3.14159265358979323846;
                else if (endsWith(parts[0], "grad")) hVal *= 0.9;
                double h = std::fmod(std::fmod(hVal, 360.0) + 360.0, 360.0) / 30.0;
                double s = parseAmount(parts[1], 100.0);
                double l = parseAmount(parts[2], 100.0);
                double a = s * std::min(l, 1.0 - l);
                auto chan = [&](double k) {
                    double t = std::fmod(k + h, 12.0);
                    return l - a * std::max(-1.0, std::min({t - 3.0, 9.0 - t, 1.0}));
                };
                r = chan(0.0);
                g = chan(8.0);
                b = chan(4.0);
            }
            outLum = srgbLuminance(r, g, b);
            outAlpha = alpha;
            return !std::isnan(outLum + outAlpha);
        }
    }
    return false;
}

inline std::string findStyleProp(const std::string& styleAttr, const std::string& prop) noexcept {
    for (const char* prefix : {"\"", ";"}) {
        std::string needle = std::string(prefix) + prop + ":";
        size_t pos = styleAttr.find(needle);
        if (pos != std::string::npos) {
            size_t start = pos + needle.size();
            size_t end = styleAttr.find_first_of(";\"", start);
            return trimStr(styleAttr.substr(start, end == std::string::npos ? std::string::npos : end - start));
        }
    }
    return "";
}

inline double shapeOpacityProduct(const std::string& shape) noexcept {
    double prod = 1.0;
    for (const char* prop : {"fill-opacity", "opacity"}) {
        std::string val = findStyleProp(shape, prop);
        if (!val.empty()) {
            bool pct = endsWith(val, "%");
            try {
                size_t idx = 0;
                std::string numPart = pct ? val.substr(0, val.size() - 1) : val;
                double v = std::stod(numPart, &idx);
                if (idx != numPart.size()) return std::numeric_limits<double>::quiet_NaN();
                prod *= std::min(1.0, std::max(0.0, v / (pct ? 100.0 : 1.0)));
            } catch (...) {
                return std::numeric_limits<double>::quiet_NaN();
            }
        }
    }
    return prod;
}

inline std::string ink(const std::string& shape, const std::string& text = "") noexcept {
    std::string fillVal = findStyleProp(shape, "fill");
    if (fillVal.empty() || !findStyleProp(text, "fill").empty()) return "";
    double lum = 0, alpha = 1;
    if (!parseColorLumAlpha(fillVal, lum, alpha)) return "";
    double op = shapeOpacityProduct(shape);
    if (!(alpha * op >= 0.5)) return "";
    static const double EVEN = std::sqrt(1.05 * (srgbLuminance(31.0 / 255.0, 31.0 / 255.0, 31.0 / 255.0) + 0.05)) - 0.05;
    return lum > EVEN ? ON_LIGHT : ON_DARK;
}

inline bool paints(const std::string& shape) noexcept {
    std::string fill = toLowerAscii(findStyleProp(shape, "fill"));
    if (fill.empty() || fill == "none" || fill == "transparent") return false;
    double scale = shapeOpacityProduct(shape);
    double lum = 0, alpha = 1;
    if (!parseColorLumAlpha(fill, lum, alpha)) alpha = 1.0;
    return alpha * (std::isnan(scale) ? 1.0 : scale) > 0;
}

inline std::string inkFill(const ResolvedStyle& style, const std::string& fallback = "") noexcept {
    std::string color = ink(style.shape, style.text);
    if (color.empty()) color = fallback;
    return color.empty() ? "" : Pele::cat({" fill=\"", color, "\""});
}

// ============================================================================
// src/svg/shapes.ts
// ============================================================================

struct ShapeSize {
    double w = 0.0;
    double h = 0.0;
    double dy = 0.0;
};

constexpr double SHAPE_PAD_X = 18.0;
constexpr double SHAPE_PAD_Y = 12.0;
constexpr double SHAPE_CORNER = 12.0;

inline double shapeBulge(double t, double half, double depth) noexcept {
    double k = std::min(1.0, std::abs(t) / (half == 0 ? 1.0 : half));
    return depth * (1.0 - std::sqrt(1.0 - k * k));
}

inline ShapeSize triShapeSize(double tw, double th, bool flip) noexcept {
    double w = std::max(1.6 * tw + 40.0, 80.0);
    double h = (th + 10.0) / (1.0 - (tw + 8.0) / w);
    double dy = h / 2.0 - 10.0 - th / 2.0;
    return {w, h, flip ? -dy : dy};
}

inline std::string svgPoly(const std::vector<double>& points, const std::string& a) noexcept {
    std::string d;
    for (size_t i = 0; i + 1 < points.size(); i += 2) {
        if (i > 0) d.push_back(' ');
        Pele::appendCat(d, {num(points[i]), ",", num(points[i + 1])});
    }
    return Pele::cat({"<polygon points=\"", d, "\"", a, "/>"});
}

inline std::string svgRectCentered(double w, double h, const std::string& a, const std::string& rx = "") noexcept {
    return Pele::cat({"<rect x=\"", num(-w / 2.0), "\" y=\"", num(-h / 2.0), "\" width=\"", num(w), "\" height=\"", num(h), "\"", rx, a, "/>"});
}

inline std::string svgBrace(double x, double h, double dir, const std::string& line) noexcept {
    double y = h / 2.0;
    double k = 6.0 * dir;
    return Pele::cat({"<path d=\"M", num(x), ",", num(-y), "q", num(k), ",0 ", num(k), ",6V-6q0,6 ", num(k), ",6q", num(-k), ",0 ", num(-k), ",6V", num(y - 6.0), "q0,6 ", num(-k), ",6\" fill=\"none\"", line, "/>"});
}

inline std::string svgWave(double w, double h, const std::string& a) noexcept {
    double x = w / 2.0;
    double top = -h / 2.0;
    double base = h / 2.0 - 10.0;
    double q = w / 4.0;
    return Pele::cat({"<path d=\"M", num(-x), ",", num(top), "H", num(x), "V", num(base), "q", num(-q), ",-12 ", num(-2.0 * q), ",0t", num(-2.0 * q), ",0Z\"", a, "/>"});
}

inline ShapeSize shapeSize(const std::string& shape, double tw, double th) noexcept {
    const double PX = SHAPE_PAD_X;
    const double PY = SHAPE_PAD_Y;
    if (shape == "stadium") {
        double h = th + 2 * PY;
        return {tw + h * 0.5 + 2 * PX - 8, h, 0};
    }
    if (shape == "fr-rect") return {tw + 2 * PX + 16, th + 2 * PY, 0};
    if (shape == "cyl") {
        double w = std::max(tw + 2 * PX, 56.0);
        double ry = std::min(12.0, std::max(6.0, w / 14.0));
        return {w, th + 2 * PY + 2 * ry, ry / 2.0};
    }
    if (shape == "circle") {
        double d = std::max(std::sqrt(tw * tw + th * th) + 16.0, 44.0);
        return {d, d, 0};
    }
    if (shape == "dbl-circ") {
        double d = std::max(std::sqrt(tw * tw + th * th) + 26.0, 54.0);
        return {d, d, 0};
    }
    if (shape == "ellipse") return {tw * 1.3 + 2 * PX, th * 1.2 + 2 * PY, 0};
    if (shape == "diam") {
        double a = tw + 16.0;
        double b = th + 8.0;
        return {a + 1.6 * b, a / 1.6 + b, 0};
    }
    if (shape == "hex" || shape == "lean-r" || shape == "lean-l" || shape == "trap-b" || shape == "trap-t") {
        double h = th + 2 * PY;
        return {tw + 2 * PX + h / 2.0, h, 0};
    }
    if (shape == "odd" || shape == "delay") {
        double h = th + 2 * PY;
        return {tw + 2 * PX + h / 4.0, h, 0};
    }
    if (shape == "text") return {tw + 8, th + 8, 0};
    if (shape == "sm-circ" || shape == "f-circ") return {14, 14, 0};
    if (shape == "fr-circ") return {18, 18, 0};
    if (shape == "fork") return {72, 8, 0};
    if (shape == "lin-rect") return {tw + 2 * PX + 8, th + 2 * PY, 0};
    if (shape == "div-rect") return {tw + 2 * PX, th + 2 * PY + 8, 4};
    if (shape == "win-pane" || shape == "st-rect") return {tw + 2 * PX + 8, th + 2 * PY + 8, 4};
    if (shape == "tri") return triShapeSize(tw, th, false);
    if (shape == "flip-tri") return triShapeSize(tw, th, true);
    if (shape == "sl-rect") return {tw + 2 * PX, th + 2 * PY + 10, 5};
    if (shape == "notch-pent") return {tw + 2 * PX, th + 2 * PY, 0};
    if (shape == "hourglass") return {32, 40, 0};
    if (shape == "h-cyl") {
        double h = th + 2 * PY;
        double rx = std::min(12.0, h / 4.0);
        return {tw + 2 * PX + 2 * rx, h, 0};
    }
    if (shape == "cross-circ") return {44, 44, 0};
    if (shape == "doc" || shape == "tag-doc") return {tw + 2 * PX, th + 2 * PY + 10, -5};
    if (shape == "flag") return {tw + 2 * PX, th + 2 * PY + 16, 0};
    if (shape == "folder") return {std::max(tw + 2 * PX, 64.0), th + 2 * PY + 8, 4};
    if (shape == "bucket") return {tw + 2 * PX + 20, th + 2 * PY + 10, 3};
    if (shape == "console" || shape == "browser") return {std::max(tw + 2 * PX, 72.0), th + 2 * PY + 12, 6};
    if (shape == "person") return {std::max(tw + 2 * PX, 56.0), th + 2 * PY + 26, 13};
    if (shape == "bang") return {tw + 2 * PX + 28, th + 2 * PY + 24, 0};
    if (shape == "cloud") return {tw + 2 * PX + 32, th + 2 * PY + 24, 0};
    if (shape == "brace" || shape == "brace-r") return {tw + 2 * PX + 12, th + 2 * PY, 0};
    if (shape == "braces") return {tw + 2 * PX + 24, th + 2 * PY, 0};
    if (shape == "bolt") return {36, 56, 0};
    if (shape == "lin-cyl") {
        double w = std::max(tw + 2 * PX, 56.0);
        double ry = std::min(12.0, std::max(6.0, w / 14.0));
        return {w, th + 2 * PY + 2 * ry + 8, ry / 2.0 + 4.0};
    }
    if (shape == "curv-trap") {
        double h = th + 2 * PY;
        return {tw + 2 * PX + h * 0.6, h, 0};
    }
    if (shape == "docs") return {tw + 2 * PX + 8, th + 2 * PY + 18, -1};
    if (shape == "bow-rect") {
        double h = th + 2 * PY;
        return {tw + 2 * PX + h * 0.4, h, 0};
    }
    if (shape == "lin-doc") return {tw + 2 * PX + 8, th + 2 * PY + 10, -5};
    if (shape == "collapsedGroup") return {tw + 2 * PX + 8, th + 2 * PY + 8, -2};
    // Default box (rect, rounded, notch-rect, datastore, tag-rect, etc.)
    return {std::max(tw + 2 * PX, 2 * PX + 12.0), th + 2 * PY, 0};
}

inline double shapeInset(const std::string& shape, double w, double h, int side, double t = 0.0) noexcept {
    if (shape == "stadium") return (side & 1) ? shapeBulge(t, h / 2.0, h / 2.0) : 0.0;
    if (shape == "cyl" || shape == "lin-cyl") return (side & 1) ? 0.0 : shapeBulge(t, w / 2.0, std::min(12.0, std::max(6.0, w / 14.0)));
    if (shape == "circle" || shape == "dbl-circ" || shape == "ellipse" || shape == "sm-circ" || shape == "f-circ" || shape == "fr-circ" || shape == "cross-circ") {
        return (side & 1) ? shapeBulge(t, h / 2.0, w / 2.0) : shapeBulge(t, w / 2.0, h / 2.0);
    }
    if (shape == "diam") return (side & 1) ? ((std::abs(t) * w) / h) : ((std::abs(t) * h) / w);
    if (shape == "hex") return (side & 1) ? (std::abs(t) / 2.0) : 0.0;
    if (shape == "odd") return (side == 3) ? (h / 4.0) : 0.0;
    if (shape == "lean-r" || shape == "lean-l" || shape == "trap-b" || shape == "trap-t") return (side & 1) ? (h / 8.0) : 0.0;
    if (shape == "tri" || shape == "flip-tri" || shape == "bolt") return (side & 1) ? (w / 4.0) : 0.0;
    if (shape == "sl-rect") return (side == 0) ? 5.0 : 0.0;
    if (shape == "hourglass") return (side & 1) ? (w / 2.0) : 0.0;
    if (shape == "doc" || shape == "docs" || shape == "tag-doc" || shape == "lin-doc") return (side == 2) ? 10.0 : 0.0;
    if (shape == "flag") return (side & 1) ? 0.0 : 8.0;
    if (shape == "bucket") return (side & 1) ? (h / 12.0) : 0.0;
    if (shape == "bang") return 3.0;
    if (shape == "cloud") return (side & 1) ? 2.0 : 1.0;
    if (shape == "bow-rect") return (side == 1) ? (h * 0.2) : 0.0;
    return 0.0;
}

inline double shapeSpan(const std::string& shape, double w, double h, bool sideways) noexcept {
    const double C = SHAPE_CORNER;
    double res = 0.0;
    if (shape == "rect" || shape == "rounded" || shape == "fr-rect" || shape == "text" || shape == "notch-rect" || shape == "lin-rect" || shape == "div-rect" || shape == "win-pane" || shape == "notch-pent" || shape == "datastore" || shape == "console" || shape == "browser" || shape == "tag-rect") {
        res = (sideways ? h : w) - 2.0 * C;
    } else if (shape == "stadium") {
        res = sideways ? (h * 0.6) : (w - h);
    } else if (shape == "cyl") {
        res = sideways ? (h - 2.0 * C - 12.0) : (w * 0.6);
    } else if (shape == "lin-cyl") {
        res = sideways ? (h - 2.0 * C - 20.0) : (w * 0.6);
    } else if (shape == "circle" || shape == "dbl-circ" || shape == "ellipse" || shape == "diam" || shape == "sm-circ" || shape == "f-circ" || shape == "fr-circ" || shape == "cross-circ") {
        res = (sideways ? h : w) * 0.6;
    } else if (shape == "hex") {
        res = sideways ? (h * 0.5) : (w - h / 2.0 - C);
    } else if (shape == "odd") {
        res = sideways ? 0.0 : (w - 2.0 * C);
    } else if (shape == "lean-r" || shape == "lean-l" || shape == "trap-b" || shape == "trap-t") {
        res = sideways ? 0.0 : (w - 2.0 * (h / 4.0) - 2.0 * C);
    } else if (shape == "fork") {
        res = (sideways ? h : w) - 8.0;
    } else if (shape == "st-rect") {
        res = (sideways ? h : w) - 2.0 * C - 8.0;
    }
    return std::max(0.0, res);
}

inline bool shapeHasLabel(const std::string& shape) noexcept {
    return !(shape == "sm-circ" || shape == "f-circ" || shape == "fr-circ" || shape == "fork" || shape == "hourglass" || shape == "cross-circ" || shape == "bolt");
}

inline bool shapeFilled(const std::string& shape) noexcept {
    return !(shape == "text" || shape == "brace" || shape == "brace-r" || shape == "braces");
}

inline std::string drawShape(const std::string& shape, double w, double h, const std::string& a, const std::string& line) noexcept {
    if (shape == "rounded") return svgRectCentered(w,  h,  a, Pele::cat({" rx=\"", num(std::min(12.0, h / 2.0)), "\""}));
    if (shape == "stadium") return svgRectCentered(w,  h,  a, Pele::cat({" rx=\"", num(h / 2.0), "\""}));
    if (shape == "fr-rect") {
        double x = w / 2.0 - 8.0;
        return Pele::cat({svgRectCentered(w, h, rounded(a)), "<path d=\"M", num(-x), ",", num(-h / 2.0), "V", num(h / 2.0), "M", num(x), ",", num(-h / 2.0), "V", num(h / 2.0), "\" fill=\"none\"", line, "/>"});
    }
    if (shape == "cyl") {
        double rx = w / 2.0;
        double ry = std::min(12.0, std::max(6.0, w / 14.0));
        double top = -h / 2.0 + ry;
        double bottom = h / 2.0 - ry;
        return Pele::cat({"<path d=\"M", num(-rx), ",", num(top), "A", num(rx), ",", num(ry), " 0 0 1 ", num(rx), ",", num(top), "V", num(bottom), "A", num(rx), ",", num(ry), " 0 0 1 ", num(-rx), ",", num(bottom), "Z\"", a, "/>"
            "<path d=\"M", num(-rx), ",", num(top), "A", num(rx), ",", num(ry), " 0 0 0 ", num(rx), ",", num(top), "\" fill=\"none\"", line, "/>"});
    }
    if (shape == "circle" || shape == "sm-circ") return Pele::cat({"<circle r=\"", num(w / 2.0), "\"", a, "/>"});
    if (shape == "dbl-circ") return Pele::cat({"<circle r=\"", num(w / 2.0), "\"", a, "/><circle r=\"", num(w / 2.0 - 5.0), "\" fill=\"none\"", line, "/>"});
    if (shape == "ellipse") return Pele::cat({"<ellipse rx=\"", num(w / 2.0), "\" ry=\"", num(h / 2.0), "\"", a, "/>"});
    if (shape == "diam") return svgPoly({0, -h / 2.0, w / 2.0, 0, 0, h / 2.0, -w / 2.0, 0}, a);
    if (shape == "hex") {
        double m = h / 4.0;
        return svgPoly({-w / 2.0 + m, -h / 2.0, w / 2.0 - m, -h / 2.0, w / 2.0, 0, w / 2.0 - m, h / 2.0, -w / 2.0 + m, h / 2.0, -w / 2.0, 0}, a);
    }
    if (shape == "odd") {
        double notch = h / 4.0;
        return svgPoly({-w / 2.0, -h / 2.0, w / 2.0, -h / 2.0, w / 2.0, h / 2.0, -w / 2.0, h / 2.0, -w / 2.0 + notch, 0}, a);
    }
    if (shape == "lean-r") {
        double s = h / 4.0;
        return svgPoly({-w / 2.0 + s, -h / 2.0, w / 2.0, -h / 2.0, w / 2.0 - s, h / 2.0, -w / 2.0, h / 2.0}, a);
    }
    if (shape == "lean-l") {
        double s = h / 4.0;
        return svgPoly({-w / 2.0, -h / 2.0, w / 2.0 - s, -h / 2.0, w / 2.0, h / 2.0, -w / 2.0 + s, h / 2.0}, a);
    }
    if (shape == "trap-b") {
        double s = h / 4.0;
        return svgPoly({-w / 2.0 + s, -h / 2.0, w / 2.0 - s, -h / 2.0, w / 2.0, h / 2.0, -w / 2.0, h / 2.0}, a);
    }
    if (shape == "trap-t") {
        double s = h / 4.0;
        return svgPoly({-w / 2.0, -h / 2.0, w / 2.0, -h / 2.0, w / 2.0 - s, h / 2.0, -w / 2.0 + s, h / 2.0}, a);
    }
    if (shape == "text") return svgRectCentered(w, h, " fill=\"none\" stroke=\"none\"");
    if (shape == "f-circ") return Pele::cat({"<circle r=\"", num(w / 2.0), "\" fill=\"var(--_l)\"", line, "/>"});
    if (shape == "fr-circ") return Pele::cat({"<circle r=\"", num(w / 2.0), "\"", a, "/><circle r=\"", num(w / 2.0 - 4.0), "\" fill=\"var(--_l)\"", line, "/>"});
    if (shape == "fork") return svgRectCentered(w,  h, Pele::cat({" fill=\"var(--_l)\"", line}),  " rx=\"2\"");
    if (shape == "notch-rect") {
        double c = 10.0;
        return svgPoly({-w / 2.0 + c, -h / 2.0, w / 2.0, -h / 2.0, w / 2.0, h / 2.0, -w / 2.0, h / 2.0, -w / 2.0, -h / 2.0 + c}, a);
    }
    if (shape == "lin-rect") return Pele::cat({svgRectCentered(w, h, rounded(a)), "<path d=\"M", num(-w / 2.0 + 8.0), ",", num(-h / 2.0), "V", num(h / 2.0), "\" fill=\"none\"", line, "/>"});
    if (shape == "div-rect") return Pele::cat({svgRectCentered(w, h, rounded(a)), "<path d=\"M", num(-w / 2.0), ",", num(-h / 2.0 + 8.0), "H", num(w / 2.0), "\" fill=\"none\"", line, "/>"});
    if (shape == "win-pane") return Pele::cat({svgRectCentered(w, h, rounded(a)), "<path d=\"M", num(-w / 2.0 + 8.0), ",", num(-h / 2.0), "V", num(h / 2.0), "M", num(-w / 2.0), ",", num(-h / 2.0 + 8.0), "H", num(w / 2.0), "\" fill=\"none\"", line, "/>"});
    if (shape == "tri") return svgPoly({0, -h / 2.0, w / 2.0, h / 2.0, -w / 2.0, h / 2.0}, a);
    if (shape == "flip-tri") return svgPoly({-w / 2.0, -h / 2.0, w / 2.0, -h / 2.0, 0, h / 2.0}, a);
    if (shape == "sl-rect") return svgPoly({-w / 2.0, -h / 2.0 + 10.0, w / 2.0, -h / 2.0, w / 2.0, h / 2.0, -w / 2.0, h / 2.0}, a);
    if (shape == "notch-pent") {
        double c = 10.0;
        return svgPoly({-w / 2.0 + c, -h / 2.0, w / 2.0 - c, -h / 2.0, w / 2.0, -h / 2.0 + c, w / 2.0, h / 2.0, -w / 2.0, h / 2.0, -w / 2.0, -h / 2.0 + c}, a);
    }
    if (shape == "hourglass") return svgPoly({-w / 2.0, -h / 2.0, w / 2.0, -h / 2.0, -w / 2.0, h / 2.0, w / 2.0, h / 2.0}, a);
    if (shape == "delay") {
        double r = h / 2.0;
        return Pele::cat({"<path d=\"M", num(-w / 2.0), ",", num(-r), "H", num(w / 2.0 - r), "A", num(r), ",", num(r), " 0 0 1 ", num(w / 2.0 - r), ",", num(r), "H", num(-w / 2.0), "Z\"", a, "/>"});
    }
    if (shape == "h-cyl") {
        double ry = h / 2.0;
        double rx = std::min(12.0, h / 4.0);
        double l = -w / 2.0 + rx;
        double r = w / 2.0 - rx;
        return Pele::cat({"<path d=\"M", num(l), ",", num(-ry), "H", num(r), "A", num(rx), ",", num(ry), " 0 0 1 ", num(r), ",", num(ry), "H", num(l), "A", num(rx), ",", num(ry), " 0 0 1 ", num(l), ",", num(-ry), "Z\"", a, "/>"
            "<path d=\"M", num(r), ",", num(-ry), "A", num(rx), ",", num(ry), " 0 0 0 ", num(r), ",", num(ry), "\" fill=\"none\"", line, "/>"});
    }
    if (shape == "cross-circ") {
        double r = w / 2.0;
        double k = r * 0.70710678118654752440;
        return Pele::cat({"<circle r=\"", num(r), "\"", a, "/><path d=\"M", num(-k), ",", num(-k), "L", num(k), ",", num(k), "M", num(k), ",", num(-k), "L", num(-k), ",", num(k), "\" fill=\"none\"", line, "/>"});
    }
    if (shape == "st-rect") {
        double bw = w - 8.0;
        double bh = h - 8.0;
        std::string out;
        for (int i = 0; i < 3; i++) {
            double x = -w / 2.0 + (2 - i) * 4.0;
            double y = -h / 2.0 + i * 4.0;
            Pele::appendCat(out, {"<rect x=\"", num(x), "\" y=\"", num(y), "\" width=\"", num(bw), "\" height=\"", num(bh), "\"", rounded(a), "/>"});
        }
        return out;
    }
    if (shape == "doc") return svgWave(w, h, a);
    if (shape == "flag") {
        double x = w / 2.0;
        double y = h / 2.0 - 8.0;
        double q = w / 4.0;
        return Pele::cat({"<path d=\"M", num(-x), ",", num(-y), "q", num(q), ",-10 ", num(2.0 * q), ",0t", num(2.0 * q), ",0V", num(y), "q", num(-q), ",10 ", num(-2.0 * q), ",0t", num(-2.0 * q), ",0Z\"", a, "/>"});
    }
    if (shape == "datastore") {
        std::string aNoStroke = replaceAllStr(a, "stroke=\"var(--_b)\"", "stroke=\"none\"");
        return Pele::cat({svgRectCentered(w, h, aNoStroke), "<path d=\"M", num(-w / 2.0), ",", num(-h / 2.0), "H", num(w / 2.0), "M", num(-w / 2.0), ",", num(h / 2.0), "H", num(w / 2.0), "\" fill=\"none\"", line, "/>"});
    }
    if (shape == "folder") {
        double x = w / 2.0;
        double y = h / 2.0;
        double tab = std::min(w * 0.4, 48.0);
        return Pele::cat({"<path d=\"M", num(-x), ",", num(-y), "H", num(-x + tab), "L", num(-x + tab + 8.0), ",", num(-y + 8.0), "H", num(x), "V", num(y), "H", num(-x), "Z\"", a, "/>"});
    }
    if (shape == "bucket") {
        double x = w / 2.0;
        double y = h / 2.0;
        double ry = 5.0;
        double taper = h / 6.0;
        return Pele::cat({"<path d=\"M", num(-x), ",", num(-y + ry), "A", num(x), ",", num(ry), " 0 0 1 ", num(x), ",", num(-y + ry), "L", num(x - taper), ",", num(y - ry), "A", num(x - taper), ",", num(ry), " 0 0 1 ", num(-x + taper), ",", num(y - ry), "Z\"", a, "/>"
            "<path d=\"M", num(-x), ",", num(-y + ry), "A", num(x), ",", num(ry), " 0 0 0 ", num(x), ",", num(-y + ry), "\" fill=\"none\"", line, "/>"});
    }
    if (shape == "console") {
        return Pele::cat({svgRectCentered(w, h, rounded(a)), "<path d=\"M", num(-w / 2.0), ",", num(-h / 2.0 + 12.0), "H", num(w / 2.0), "M", num(-w / 2.0 + 7.0), ",", num(-h / 2.0 + 4.0), "l3,2.5l-3,2.5M", num(-w / 2.0 + 13.0), ",", num(-h / 2.0 + 9.0), "h4\" fill=\"none\"", line, "/>"});
    }
    if (shape == "browser") {
        std::string dots;
        for (int i = 0; i < 3; i++) dots += "<circle cx=\"" + num(-w / 2.0 + 8.0 + i * 7.0) + "\" cy=\"" + num(-h / 2.0 + 6.0) + "\" r=\"1.5\" fill=\"none\"" + line + "/>";
        return Pele::cat({svgRectCentered(w, h, rounded(a)), "<path d=\"M", num(-w / 2.0), ",", num(-h / 2.0 + 12.0), "H", num(w / 2.0), "\" fill=\"none\"", line, "/>", dots});
    }
    if (shape == "person") {
        double top = -h / 2.0;
        return Pele::cat({"<rect x=\"", num(-w / 2.0), "\" y=\"", num(top + 26.0), "\" width=\"", num(w), "\" height=\"", num(h - 26.0), "\" rx=\"12\"", a, "/><circle cy=\"", num(top + 14.0), "\" r=\"13\"", a, "/>"});
    }
    if (shape == "bang") {
        const int spikes = 14;
        std::vector<double> pts;
        for (int i = 0; i < spikes * 2; i++) {
            double angle = (3.14159265358979323846 * i) / spikes - 3.14159265358979323846 / 2.0;
            double k = (i % 2 == 0) ? 1.0 : 0.82;
            pts.push_back(std::cos(angle) * (w / 2.0) * k);
            pts.push_back(std::sin(angle) * (h / 2.0) * k);
        }
        return svgPoly(pts, a);
    }
    if (shape == "cloud") {
        double x = w / 2.0;
        double y = h / 2.0;
        double r = y * 0.55;
        return Pele::cat({"<path d=\"M", num(-x + r), ",", num(y), "A", num(r), ",", num(r), " 0 0 1 ", num(-x + r * 0.6), ",", num(-y * 0.1), "A", num(x * 0.42), ",", num(y * 0.7), " 0 0 1 ", num(-x * 0.1), ",", num(-y * 0.72), "A", num(x * 0.4), ",", num(y * 0.62), " 0 0 1 ", num(x * 0.62), ",", num(-y * 0.42), "A", num(r * 1.05), ",", num(r * 1.05), " 0 0 1 ", num(x - r), ",", num(y), "Z\"", a, "/>"});
    }
    if (shape == "brace") return svgRectCentered(w, h, " fill=\"none\" stroke=\"none\"") + svgBrace(-w / 2.0 + 12.0, h, -1.0, line);
    if (shape == "brace-r") return svgRectCentered(w, h, " fill=\"none\" stroke=\"none\"") + svgBrace(w / 2.0 - 12.0, h, 1.0, line);
    if (shape == "braces") return svgRectCentered(w, h, " fill=\"none\" stroke=\"none\"") + svgBrace(-w / 2.0 + 12.0, h, -1.0, line) + svgBrace(w / 2.0 - 12.0, h, 1.0, line);
    if (shape == "bolt") return svgPoly({w * 0.15, -h / 2.0, -w / 2.0, h * 0.08, -w * 0.05, h * 0.08, -w * 0.15, h / 2.0, w / 2.0, -h * 0.08, w * 0.05, -h * 0.08}, a);
    if (shape == "lin-cyl") {
        double rx = w / 2.0;
        double ry = std::min(12.0, std::max(6.0, w / 14.0));
        double top = -h / 2.0 + ry;
        double bottom = h / 2.0 - ry;
        auto arc = [&](double y) { return "M" + num(-rx) + "," + num(y) + "A" + num(rx) + "," + num(ry) + " 0 0 0 " + num(rx) + "," + num(y); };
        return Pele::cat({"<path d=\"M", num(-rx), ",", num(top), "A", num(rx), ",", num(ry), " 0 0 1 ", num(rx), ",", num(top), "V", num(bottom), "A", num(rx), ",", num(ry), " 0 0 1 ", num(-rx), ",", num(bottom), "Z\"", a, "/>"
            "<path d=\"", arc(top), arc(top + 8.0), "\" fill=\"none\"", line, "/>"});
    }
    if (shape == "curv-trap") {
        double x = w / 2.0;
        double y = h / 2.0;
        double k = h * 0.3;
        return Pele::cat({"<path d=\"M", num(-x + k), ",", num(-y), "H", num(x - k), "A", num(k), ",", num(y), " 0 0 1 ", num(x - k), ",", num(y), "H", num(-x + k), "L", num(-x), ",0Z\"", a, "/>"});
    }
    if (shape == "docs") {
        std::string out;
        for (int i = 0; i < 3; i++) {
            out += "<g transform=\"translate(" + std::to_string((1 - i) * 4) + "," + std::to_string((i - 1) * 4) + ")\">" + svgWave(w - 8.0, h - 8.0, a) + "</g>";
        }
        return out;
    }
    if (shape == "bow-rect") {
        double x = w / 2.0;
        double y = h / 2.0;
        double k = h * 0.2;
        return Pele::cat({"<path d=\"M", num(-x + k), ",", num(-y), "H", num(x), "A", num(k), ",", num(y), " 0 0 0 ", num(x), ",", num(y), "H", num(-x + k), "A", num(k), ",", num(y), " 0 0 1 ", num(-x + k), ",", num(-y), "Z\"", a, "/>"});
    }
    if (shape == "tag-rect") return Pele::cat({svgRectCentered(w, h, rounded(a)), "<path d=\"M", num(w / 2.0 - 12.0), ",", num(h / 2.0), "L", num(w / 2.0), ",", num(h / 2.0 - 12.0), "\" fill=\"none\"", line, "/>"});
    if (shape == "tag-doc") return Pele::cat({svgWave(w, h, a), "<path d=\"M", num(w / 2.0 - 12.0), ",", num(h / 2.0 - 14.0), "L", num(w / 2.0), ",", num(h / 2.0 - 26.0), "\" fill=\"none\"", line, "/>"});
    if (shape == "lin-doc") return Pele::cat({svgWave(w, h, a), "<path d=\"M", num(-w / 2.0 + 8.0), ",", num(-h / 2.0), "V", num(h / 2.0 - 6.0), "\" fill=\"none\"", line, "/>"});
    if (shape == "collapsedGroup") {
        return Pele::cat({"<rect x=\"", num(-w / 2.0 + 6.0), "\" y=\"", num(-h / 2.0 + 8.0), "\" width=\"", num(w - 12.0), "\" height=\"", num(h - 8.0), "\"", rounded(a), "/>"
            "<rect x=\"", num(-w / 2.0), "\" y=\"", num(-h / 2.0), "\" width=\"", num(w), "\" height=\"", num(h - 4.0), "\"", rounded(a), "/>"});
    }
    return svgRectCentered(w, h, rounded(a));
}

// ============================================================================
// src/svg/edges.ts
// ============================================================================

constexpr double ARROW = 8.0;
constexpr double LEAD = 6.0;

struct EdgePath {
    std::string d;
    std::vector<double> tokens;
    bool hasTokens = false;
    double sx = 0.0;
    double sy = 0.0;
    double sdx = 0.0;
    double sdy = 0.0;
    double ex = 0.0;
    double ey = 0.0;
    double edx = 0.0;
    double edy = 0.0;
};

inline double easeCurve(double across, double along) noexcept {
    double den = std::abs(across);
    if (den == 0.0) den = 1.0;
    return std::max(0.25, std::min(0.5, (4.0 * std::abs(along)) / den));
}

inline double markerTrim(const std::string& type) noexcept {
    if (type == "arrow_point") return ARROW - 1.0;
    if (type == "arrow_circle") return ARROW;
    if (type == "arrow_cross") return 4.0;
    return 0.0;
}

inline std::string marker(const std::string& type, double x, double y, double dx, double dy, const std::string& color, const std::string& stroke = "none") noexcept {
    double nx = -dy;
    double ny = dx;
    if (type == "arrow_point") {
        double bx = x - dx * ARROW;
        double by = y - dy * ARROW;
        double half = ARROW * 0.42;
        return Pele::cat({"<path class=\"pele-marker\" d=\"M", num(x), ",", num(y), "L", num(bx + nx * half), ",", num(by + ny * half), "L", num(bx - nx * half), ",", num(by - ny * half), "Z\" fill=\"", color, "\" stroke=\"", stroke, "\"/>"});
    }
    if (type == "arrow_circle") {
        return Pele::cat({"<circle class=\"pele-marker\" cx=\"", num(x - dx * 4.0), "\" cy=\"", num(y - dy * 4.0), "\" r=\"3.5\" fill=\"", color, "\" stroke=\"", stroke, "\"/>"});
    }
    if (type == "arrow_cross") {
        double cx = x - dx * 4.0;
        double cy = y - dy * 4.0;
        double a = 3.5;
        double ux = (dx + nx) * a;
        double uy = (dy + ny) * a;
        double vx = (dx - nx) * a;
        double vy = (dy - ny) * a;
        return Pele::cat({"<path class=\"pele-marker\" d=\"M", num(cx - ux), ",", num(cy - uy), "L", num(cx + ux), ",", num(cy + uy), "M", num(cx - vx), ",", num(cy - vy), "L", num(cx + vx), ",", num(cy + vy), "\"/>"});
    }
    return "";
}

struct SideReach {
    Pele::FlatMap<int, double> reach;
    void add(int node, int side, const std::string& type) {
        double trim = markerTrim(type);
        if (side >= 0 && trim > 0) {
            int key = node * 4 + side;
            if (trim + LEAD > reach[key]) reach[key] = trim + LEAD;
        }
    }
    double of(int node, int side) const {
        if (side < 0) return 0.0;
        auto it = reach.find(node * 4 + side);
        return it == reach.end() ? 0.0 : it->second;
    }
};

inline double writtenCoord(double v) noexcept {
    long long n = static_cast<long long>(std::llround(v * 100.0));
    return n == 0 ? 0.0 : static_cast<double>(n) / 100.0;
}

inline EdgePath routePath(
    const std::vector<double>& route,
    const std::string& curve,
    double startTrim,
    double endTrim,
    double startReach = -1.0,
    double endReach = -1.0
) {
    if (startReach < 0.0) startReach = startTrim > 0 ? startTrim + LEAD : 0.0;
    if (endReach < 0.0) endReach = endTrim > 0 ? endTrim + LEAD : 0.0;
    bool linear = (curve == "linear");
    bool stepped = (curve == "step" || curve == "stepBefore" || curve == "stepAfter");
    std::vector<double> xs;
    std::vector<double> ys;
    std::vector<double> axes;
    auto nearPt = [](double ax, double ay, double bx, double by) {
        return std::abs(ax - bx) < 0.01 && std::abs(ay - by) < 0.01;
    };
    for (size_t i = 0; i + 2 < route.size(); i += 3) {
        double x = route[i];
        double y = route[i + 1];
        double axis = route[i + 2];
        if (stepped && !xs.empty() && std::abs(x - xs.back()) >= 0.01 && std::abs(y - ys.back()) >= 0.01) {
            double px = xs.back();
            double py = ys.back();
            double my = (curve == "stepBefore") ? py : (curve == "stepAfter") ? y : (py + y) / 2.0;
            double mx = (curve == "stepBefore") ? px : (curve == "stepAfter") ? x : (px + x) / 2.0;
            double turns[4] = {
                axis == 0 ? px : mx,
                axis == 0 ? my : py,
                axis == 0 ? x : mx,
                axis == 0 ? my : y
            };
            for (int k = 0; k < 4; k += 2) {
                if (nearPt(turns[k], turns[k + 1], px, py) || nearPt(turns[k], turns[k + 1], x, y)) continue;
                xs.push_back(turns[k]);
                ys.push_back(turns[k + 1]);
                axes.push_back(axis);
            }
        }
        xs.push_back(x);
        ys.push_back(y);
        axes.push_back(axis);
    }
    int last = static_cast<int>(xs.size()) - 1;
    auto aligned = [&](int i) {
        return std::abs(xs[i] - xs[i - 1]) < 0.01 || std::abs(ys[i] - ys[i - 1]) < 0.01;
    };
    auto straight = [&](int i) {
        return linear || aligned(i);
    };
    auto unit = [&](int i, int j, double axis) -> std::pair<double, double> {
        if (!straight(std::max(i, j))) {
            double sgnY = (ys[j] - ys[i]) > 0 ? 1.0 : (ys[j] - ys[i]) < 0 ? -1.0 : 1.0;
            double sgnX = (xs[j] - xs[i]) > 0 ? 1.0 : (xs[j] - xs[i]) < 0 ? -1.0 : 1.0;
            return axis == 0 ? std::make_pair(0.0, sgnY) : std::make_pair(sgnX, 0.0);
        }
        double dx = xs[j] - xs[i];
        double dy = ys[j] - ys[i];
        double len = std::hypot(dx, dy);
        if (len == 0.0) len = 1.0;
        return {dx / len, dy / len};
    };
    auto [sdx, sdy] = unit(0, 1, axes[1]);
    auto [edx, edy] = unit(last - 1, last, axes[last]);
    double sx = xs[0];
    double sy = ys[0];
    double ex = xs[last];
    double ey = ys[last];
    xs[0] += sdx * startTrim;
    ys[0] += sdy * startTrim;
    xs[last] -= edx * endTrim;
    ys[last] -= edy * endTrim;
    auto length = [&](int i) {
        return std::abs(xs[i] - xs[i - 1]) + std::abs(ys[i] - ys[i - 1]);
    };
    auto turns = [&](int i) {
        return !linear && i > 0 && i < last && aligned(i) && aligned(i + 1) && length(i) >= 0.01 && length(i + 1) >= 0.01 && ((std::abs(xs[i] - xs[i - 1]) < 0.01) != (std::abs(xs[i + 1] - xs[i]) < 0.01));
    };
    auto along = [&](int i, double ux, double uy) {
        return std::abs(xs[i] - xs[i - 1]) * std::abs(ux) + std::abs(ys[i] - ys[i - 1]) * std::abs(uy);
    };
    auto lead = [&](double reach, double trim, double run) {
        double room = std::min(std::max(0.0, reach - LEAD - trim), run / 4.0);
        return room + std::min(reach - trim - room, (run - room) / 4.0);
    };
    double arrives = (endReach > endTrim && (!straight(last) || turns(last - 1))) ? lead(endReach, endTrim, along(last, edx, edy)) : 0.0;
    double leaves = (startReach > startTrim && (!straight(1) || turns(1))) ? lead(startReach, startTrim, along(1, sdx, sdy)) : 0.0;
    if (arrives >= 1.0) {
        xs.insert(xs.begin() + last, xs[last] - edx * arrives);
        ys.insert(ys.begin() + last, ys[last] - edy * arrives);
        axes.insert(axes.begin() + last, axes[last]);
        last++;
    }
    if (leaves >= 1.0) {
        xs.insert(xs.begin() + 1, xs[0] + sdx * leaves);
        ys.insert(ys.begin() + 1, ys[0] + sdy * leaves);
        axes.insert(axes.begin() + 1, axes[1]);
        last++;
    }

    std::vector<double> tokens;
    auto tok = [&](double v) {
        tokens.push_back(writtenCoord(v));
        return num(v);
    };
    tokens.push_back(77);
    tokens.push_back(2);
    std::string d = Pele::cat({"M", tok(xs[0]), ",", tok(ys[0])});
    double atX = xs[0];
    double atY = ys[0];
    auto lineTo = [&](double x, double y) {
        if (nearPt(x, y, atX, atY)) return;
        tokens.push_back(76);
        tokens.push_back(2);
        Pele::appendCat(d, {"L", tok(x), ",", tok(y)});
        atX = x;
        atY = y;
    };
    constexpr double HANDLE = 0.62;
    for (int i = 1; i <= last; i++) {
        double x = xs[i];
        double y = ys[i];
        if (!straight(i)) {
            bool down = (axes[i] == 0);
            double al = down ? (y - atY) : (x - atX);
            double k = std::abs(al) * easeCurve(down ? (x - atX) : (y - atY), al);
            double sgnAl = al > 0 ? 1.0 : al < 0 ? -1.0 : 0.0;
            double ux = down ? 0.0 : sgnAl * k;
            double uy = down ? sgnAl * k : 0.0;
            tokens.push_back(67);
            tokens.push_back(6);
            Pele::appendCat(d, {"C", tok(atX + ux), ",", tok(atY + uy), " ", tok(x - ux), ",", tok(y - uy), " ", tok(x), ",", tok(y)});
            atX = x;
            atY = y;
            continue;
        }
        if (!turns(i)) {
            if (i < last && aligned(i) && aligned(i + 1) && ((x - xs[i - 1]) * (xs[i + 1] - x) + (y - ys[i - 1]) * (ys[i + 1] - y) > 0)) continue;
            lineTo(x, y);
            continue;
        }
        double before = turns(i - 1) ? (length(i) / 2.0) : length(i);
        double after = turns(i + 1) ? (length(i + 1) / 2.0) : length(i + 1);
        if (stepped) before = after = std::min({12.0, before, after});
        auto [ux, uy] = unit(i - 1, i, axes[i]);
        auto [vx, vy] = unit(i, i + 1, axes[i + 1]);
        lineTo(x - ux * before, y - uy * before);
        atX = x + vx * after;
        atY = y + vy * after;
        if (stepped) {
            tokens.push_back(81);
            tokens.push_back(4);
            Pele::appendCat(d, {"Q", tok(x), ",", tok(y), " ", tok(atX), ",", tok(atY)});
        } else {
            tokens.push_back(67);
            tokens.push_back(6);
            Pele::appendCat(d, {"C", tok(x - ux * before * (1.0 - HANDLE)), ",", tok(y - uy * before * (1.0 - HANDLE)), " ", tok(x + vx * after * (1.0 - HANDLE)), ",", tok(y + vy * after * (1.0 - HANDLE)), " ", tok(atX), ",", tok(atY)});
        }
    }
    return {d, std::move(tokens), true, sx, sy, -sdx, -sdy, ex, ey, edx, edy};
}

inline std::vector<double> tokenizeSvgPath(const std::string& d) noexcept {
    std::vector<double> out;
    int at = -1;
    size_t i = 0;
    while (i < d.size()) {
        unsigned char c = static_cast<unsigned char>(d[i]);
        if (c >= 65 && c <= 90 && c != 69) {
            at = static_cast<int>(out.size()) + 1;
            out.push_back(static_cast<double>(c));
            out.push_back(0.0);
            i++;
        } else if (c == '-' || c == '.' || (c >= '0' && c <= '9')) {
            size_t j = (c == '-') ? i + 1 : i;
            bool seenDot = false;
            while (j < d.size()) {
                char k = d[j];
                if (k >= '0' && k <= '9') j++;
                else if (k == '.' && !seenDot) { seenDot = true; j++; }
                else if ((k == 'e' || k == 'E') && j + 1 < d.size()) {
                    j++;
                    if (d[j] == '+' || d[j] == '-') j++;
                } else break;
            }
            double v = 0.0;
            try { v = std::stod(d.substr(i, j - i)); } catch (...) {}
            if (at >= 0) {
                out.push_back(v);
                out[at] += 1.0;
            }
            i = j;
        } else {
            i++;
        }
    }
    return out;
}

inline void sampleSvgArc(std::vector<double>& out, double x1, double y1, double rx, double ry, double large, double sweep, double x2, double y2) noexcept {
    double hx = (x1 - x2) / 2.0;
    double hy = (y1 - y2) / 2.0;
    if (rx == 0 || ry == 0 || (hx == 0 && hy == 0)) {
        out.push_back(x2);
        out.push_back(y2);
        return;
    }
    double scale = std::max(1.0, std::sqrt((hx * hx) / (rx * rx) + (hy * hy) / (ry * ry)));
    rx *= scale;
    ry *= scale;
    double k = (large == sweep ? -1.0 : 1.0) * std::sqrt(std::max(0.0, (rx * rx * ry * ry - rx * rx * hy * hy - ry * ry * hx * hx) / (rx * rx * hy * hy + ry * ry * hx * hx)));
    double cx = (k * rx * hy) / ry + (x1 + x2) / 2.0;
    double cy = (-k * ry * hx) / rx + (y1 + y2) / 2.0;
    double from = std::atan2((y1 - cy) / ry, (x1 - cx) / rx);
    double turn = std::atan2((y2 - cy) / ry, (x2 - cx) / rx) - from;
    constexpr double PI = 3.14159265358979323846;
    if (sweep != 0.0 && turn < 0) turn += 2.0 * PI;
    else if (sweep == 0.0 && turn > 0) turn -= 2.0 * PI;
    for (int i = 1; i <= 8; i++) {
        out.push_back(cx + rx * std::cos(from + (turn * i) / 8.0));
        out.push_back(cy + ry * std::sin(from + (turn * i) / 8.0));
    }
}

inline std::vector<double> sampleSvgTokens(const std::vector<double>& tokens) noexcept {
    std::vector<double> out;
    double x = 0, y = 0;
    size_t i = 0;
    while (i + 1 < tokens.size()) {
        int cmd = static_cast<int>(tokens[i]);
        size_t count = static_cast<size_t>(tokens[i + 1]);
        size_t s = i + 2;
        size_t e = std::min(tokens.size(), s + count);
        i = e;
        if (cmd == 72 || cmd == 86) {
            for (size_t k = s; k < e; k++) {
                if (cmd == 72) x = tokens[k];
                else y = tokens[k];
                out.push_back(x);
                out.push_back(y);
            }
        } else if (cmd == 67 || cmd == 81) {
            bool cubic = (cmd == 67);
            size_t step = cubic ? 6 : 4;
            for (size_t k = s; k + step <= e; k += step) {
                double x1 = tokens[k], y1 = tokens[k + 1], x2 = tokens[k + 2], y2 = tokens[k + 3];
                double x3 = cubic ? tokens[k + 4] : x2;
                double y3 = cubic ? tokens[k + 5] : y2;
                for (double t = 0.125; t <= 1.0; t += 0.125) {
                    double ax = x + (x1 - x) * t;
                    double ay = y + (y1 - y) * t;
                    double bx = x1 + (x2 - x1) * t;
                    double by = y1 + (y2 - y1) * t;
                    if (cubic) {
                        double cx = x2 + (x3 - x2) * t;
                        double cy = y2 + (y3 - y2) * t;
                        ax += (bx - ax) * t;
                        ay += (by - ay) * t;
                        bx += (cx - bx) * t;
                        by += (cy - by) * t;
                    }
                    out.push_back(ax + (bx - ax) * t);
                    out.push_back(ay + (by - ay) * t);
                }
                x = x3;
                y = y3;
            }
        } else if (cmd == 65) {
            for (size_t k = s; k + 7 <= e; k += 7) {
                sampleSvgArc(out, x, y, tokens[k], tokens[k + 1], tokens[k + 3], tokens[k + 4], tokens[k + 5], tokens[k + 6]);
                x = tokens[k + 5];
                y = tokens[k + 6];
            }
        } else {
            for (size_t k = s; k + 2 <= e; k += 2) {
                if (cmd == 77 && k == s && !out.empty()) {
                    out.push_back(std::numeric_limits<double>::quiet_NaN());
                    out.push_back(std::numeric_limits<double>::quiet_NaN());
                }
                x = tokens[k];
                y = tokens[k + 1];
                out.push_back(x);
                out.push_back(y);
            }
        }
    }
    return out;
}

inline bool clipsSegmentBox(double x, double y, double dx, double dy, double l, double t, double r, double b) noexcept {
    double lo = 0.0, hi = 1.0;
    for (int side = 0; side < 4; side++) {
        double p = (side == 0) ? -dx : (side == 1) ? dx : (side == 2) ? -dy : dy;
        double q = (side == 0) ? (x - l) : (side == 1) ? (r - x) : (side == 2) ? (y - t) : (b - y);
        if (p == 0.0) {
            if (q < 0.0) hi = -1.0;
        } else if (p < 0.0) {
            lo = std::max(lo, q / p);
        } else {
            hi = std::min(hi, q / p);
        }
    }
    return lo < hi;
}

inline bool entersPointsBox(const std::vector<double>& points, double l, double t, double r, double b) noexcept {
    for (size_t k = 0; k + 3 < points.size(); k += 2) {
        double x = points[k], y = points[k + 1], x2 = points[k + 2], y2 = points[k + 3];
        if (std::isnan(x) || std::isnan(x2)) continue;
        if ((x < l && x2 < l) || (x > r && x2 > r) || (y < t && y2 < t) || (y > b && y2 > b)) continue;
        if (clipsSegmentBox(x, y, x2 - x, y2 - y, l, t, r, b)) return true;
    }
    return false;
}

struct Fill {
    double box[4] = {0, 0, 0, 0}; // left, top, right, bottom
    std::string shape;
};

struct FillLookup {
    enum State { None, CrossesBorder, Found } state = None;
    Fill fill;
};

inline FillLookup fillUnder(const std::string& d, const std::vector<double>& tips, const std::vector<Fill>& fills, const std::vector<double>* tokens = nullptr) noexcept {
    if (fills.empty()) return {FillLookup::None, {}};
    std::vector<double> points = tokens ? sampleSvgTokens(*tokens) : sampleSvgTokens(tokenizeSvgPath(d));
    double minX = std::numeric_limits<double>::infinity();
    double minY = std::numeric_limits<double>::infinity();
    double maxX = -std::numeric_limits<double>::infinity();
    double maxY = -std::numeric_limits<double>::infinity();
    for (size_t k = 0; k + 1 < points.size() + tips.size(); k += 2) {
        double x = (k < points.size()) ? points[k] : tips[k - points.size()];
        double y = (k < points.size()) ? points[k + 1] : tips[k + 1 - points.size()];
        if (std::isnan(x) || std::isnan(y)) continue;
        if (x < minX) minX = x;
        if (x > maxX) maxX = x;
        if (y < minY) minY = y;
        if (y > maxY) maxY = y;
    }
    const Fill* holder = nullptr;
    double least = std::numeric_limits<double>::infinity();
    for (const auto& f : fills) {
        double l = f.box[0], t = f.box[1], r = f.box[2], b = f.box[3];
        if (maxX < l + 0.5 || minX > r - 0.5 || maxY < t + 0.5 || minY > b - 0.5) continue;
        bool touched = entersPointsBox(points, l + 0.5, t + 0.5, r - 0.5, b - 0.5);
        for (size_t k = 0; k + 1 < tips.size() && !touched; k += 2) {
            touched = tips[k] > l + 0.5 && tips[k] < r - 0.5 && tips[k + 1] > t + 0.5 && tips[k + 1] < b - 0.5;
        }
        if (!touched) continue;
        if (minX < l || maxX > r || minY < t || maxY > b) return {FillLookup::CrossesBorder, {}};
        double area = (r - l) * (b - t);
        if (area < least) {
            least = area;
            holder = &f;
        }
    }
    if (holder) return {FillLookup::Found, *holder};
    return {FillLookup::None, {}};
}

inline std::string edgeInk(const Fill& fill, const std::string& line = "") noexcept {
    if (!findStyleProp(line, "stroke").empty()) return "";
    return ink(fill.shape);
}

inline std::string hollowOn(const std::string& markers, const Fill& fill) noexcept {
    std::string color = findStyleProp(fill.shape, "fill");
    return !color.empty() ? replaceAllStr(markers,  " fill=\"var(--_bg)\"", Pele::cat({" fill=\"", color, "\""})) : markers;
}

// Returns: hasValue=false (undefined off fills), or string ('' when crosses border or unknown ink, or ink color)
struct OverFillResult {
    bool onFill = false;
    std::string value;
};

inline OverFillResult overFills(const std::string& d, const std::vector<double>& tips, const std::vector<Fill>& fills, const std::string& line = "", const std::vector<double>* tokens = nullptr) noexcept {
    auto res = fillUnder(d, tips, fills, tokens);
    if (res.state == FillLookup::None) return {false, ""};
    if (res.state == FillLookup::CrossesBorder) return {true, ""};
    return {true, edgeInk(res.fill, line)};
}

inline std::string casing(const std::string& d, const std::string& markers, const std::string& line = "", bool thick = false) noexcept {
    constexpr double CASE = 4.0;
    std::string stroke = findStyleProp(line, "stroke");
    if (stroke == "none" || stroke == "transparent") return "";
    std::string width = thick ? "2.5" : findStyleProp(line, "stroke-width");
    std::string attrs;
    std::string style;
    if (!width.empty()) {
        std::string numPart = endsWith(width, "px") ? width.substr(0, width.size() - 2) : width;
        bool isPlainNum = !numPart.empty();
        for (char c : numPart) if (!isDigit(static_cast<unsigned char>(c)) && c != '.') { isPlainNum = false; break; }
        if (isPlainNum) {
            double wVal = std::stod(numPart);
            if (wVal != 1.0) attrs = " stroke-width=\"" + num(wVal + CASE) + "\"";
        } else {
            Pele::appendCat(style, {"stroke-width:calc(", width, " + 4px);"});
        }
    }
    for (const char* prop : {"opacity", "stroke-opacity"}) {
        std::string val = findStyleProp(line, prop);
        if (!val.empty()) style += std::string(prop) + ":" + val + ";";
    }
    std::string body = Pele::cat({"<path d=\"", d, "\"/>", replaceAllStr(markers, " class=\"pele-marker\"", "")});
    return (!attrs.empty() || !style.empty()) ? Pele::cat({"<g", attrs, (!style.empty() ? Pele::cat({" style=\"", style, "\""}) : ""), ">", body, "</g>"}) : body;
}

inline std::string edgeCasing(const EdgePath& path, const std::string& startType, const std::string& endType, const std::string& line = "", bool thick = false) noexcept {
    return casing(
        path.d,
        marker(startType, path.sx, path.sy, path.sdx, path.sdy, "var(--_bg)", "var(--_bg)") +
        marker(endType, path.ex, path.ey, path.edx, path.edy, "var(--_bg)", "var(--_bg)"),
        line,
        thick
    );
}

inline std::string casingsSvg(const std::string& casings) noexcept {
    return !casings.empty()
        ? Pele::cat({"<g class=\"pele-casings\" fill=\"none\" stroke=\"var(--_bg)\" stroke-width=\"5\" stroke-linecap=\"round\" stroke-linejoin=\"round\" pointer-events=\"none\">", casings, "</g>"})
        : "";
}

inline std::string edgeLabelSvg(const std::string& id, const Label& label, double x, double y, const std::string& attrs, const IconResolver& icons = nullptr) noexcept {
    double w = label.width + 8.0;
    double h = label.height;
    return Pele::cat({"<g class=\"pele-edge-label\" data-id=\"", escText(id), "\">"
        "<rect x=\"", num(x - w / 2.0), "\" y=\"", num(y - h / 2.0), "\" width=\"", num(w), "\" height=\"", num(h), "\" rx=\"3\" fill=\"var(--_bg)\"/>", labelSvg(label, x, y, attrs, icons), "</g>"});
}

// ============================================================================
// src/svg/title.ts & src/svg/root.ts & src/util/url.ts
// ============================================================================

inline Label titleLabel(const std::string& title, const TextMeasurer& measurer, double size, double maxWidth) noexcept {
    return withoutMath([&]() { return layoutLabel(title, false, measurer, size, maxWidth, Style::Bold); });
}

struct Titled {
    double width = 0.0;
    double height = 0.0;
    std::string content;
};

inline Titled withTitle(const std::string& title, const TextMeasurer& measurer, double size, double pad, double width, double height, const std::string& content) noexcept {
    Label label = titleLabel(title, measurer, size, 4000.0);
    if (label.height == 0.0) return {width, height, content};
    double shift = label.height + 12.0;
    double full = std::max(width, std::ceil(label.width + 2.0 * pad));
    return {
        full, 
        std::ceil(height + shift), Pele::cat({labelSvg(label, full / 2.0, pad + label.height / 2.0, " class=\"pele-title\" font-weight=\"var(--_tw)\""), "<g transform=\"translate(", num((full - width) / 2.0), ",", num(shift), ")\">", content, "</g>"})};
}

inline std::string svgDocument(
    const std::string& type,
    double width,
    double height,
    double fontSize,
    const RenderOptions& options,
    const DiagramMeta& acc,
    const std::string& content
) {
    std::string prefix = "pele";
    std::string head;
    std::string aria;
    if (!acc.accTitle.empty()) {
        Pele::appendCat(head, {"<title id=\"", prefix, "-title\">", escText(acc.accTitle), "</title>"});
        Pele::appendCat(aria, {" aria-labelledby=\"", prefix, "-title\""});
    }
    if (!acc.accDescr.empty()) {
        Pele::appendCat(head, {"<desc id=\"", prefix, "-desc\">", escText(acc.accDescr), "</desc>"});
        Pele::appendCat(aria, {" aria-describedby=\"", prefix, "-desc\""});
    }
    std::string fit = (options.hasResponsive && !options.responsive) ? "" : "max-width:100%;height:auto;";
    std::string weights;
    if (content.find("var(--_r)") != std::string::npos) Pele::appendCat(weights, {";--_r:", RADIUS});
    if (content.find("var(--_tw)") != std::string::npos) weights += ";--_tw:var(--pele-title-weight,bold)";
    if (content.find("var(--_hw)") != std::string::npos) weights += ";--_hw:var(--pele-heading-weight,bold)";
    if (content.find("var(--_w)") != std::string::npos) weights += ";--_w:var(--pele-bold-weight,bold)";

    std::string wStr = num(width);
    std::string hStr = num(height);
    return "<svg xmlns=\"http://www.w3.org/2000/svg\" class=\"pele pele-" + type + "\" viewBox=\"0 0 " + wStr + " " + hStr + "\" width=\"" + wStr + "\" height=\"" + hStr + "\" style=\"" + fit + ROOT_STYLE + weights + esc(fontStyle(fontSize, options.fontFamily, options.fontFamilyMono)) + "\" fill=\"var(--_fg)\" role=\"graphics-document document\" aria-roledescription=\"" + type + "\"" + aria + ">" + head + content + "</svg>";
}

inline std::string sanitizeUrl(const std::string& rawUrl) noexcept {
    std::string u = trimStr(decodeEntities(rawUrl));
    if (u.empty()) return "about:blank";
    std::string low = toLowerAscii(u);
    if (startsWith(low, "javascript:") || startsWith(low, "data:") || startsWith(low, "vbscript:")) return "about:blank";
    return u;
}

inline std::string linkUrl(const std::string& url, const RenderOptions& options) noexcept {
    if (!options.links) return "";
    std::string s = sanitizeUrl(url);
    if (s == "about:blank" || startsWith(s, "//") || startsWith(s, "\\\\")) return "";
    size_t colon = s.find(':');
    if (colon != std::string::npos) {
        bool allScheme = colon > 0;
        for (size_t i = 0; i < colon; i++) {
            char c = s[i];
            if (!isLetter(c) && !(i > 0 && (isDigit(c) || c == '+' || c == '.' || c == '-'))) { allScheme = false; break; }
        }
        if (allScheme) {
            std::string sc = toLowerAscii(s.substr(0, colon));
            if (sc != "http" && sc != "https" && sc != "mailto" && sc != "tel") return "";
        }
    }
    return s;
}

inline std::string imageUrl(const std::string& url, const RenderOptions& options) noexcept {
    if (!options.images) return "";
    std::string s = sanitizeUrl(url);
    if (s == "about:blank" || startsWith(s, "//") || startsWith(s, "\\\\")) return "";
    size_t colon = s.find(':');
    if (colon != std::string::npos) {
        std::string sc = toLowerAscii(s.substr(0, colon));
        if (sc != "http" && sc != "https") return "";
    }
    return s;
}

inline std::string relAttr(const RenderOptions& options) noexcept {
    return options.linkRel.empty() ? "" : Pele::cat({" rel=\"", esc(options.linkRel), "\""});
}

inline std::string linked(const std::string& body, const std::string& url, const std::string& id, std::vector<LinkInfo>& links, const RenderOptions& options, const std::string& attrs = "") noexcept {
    std::string href = linkUrl(url, options);
    if (href.empty()) return body;
    links.push_back({id, href, false});
    return Pele::cat({"<a href=\"", esc(href), "\"", attrs, relAttr(options), ">", body, "</a>"});
}

} // namespace Pele
