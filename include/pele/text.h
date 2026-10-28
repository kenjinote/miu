#pragma once
#include "util.h"
#include "math_engine.h"

namespace Pele {

// ============================================================================
// src/text/metrics.ts & src/text/measurer.ts
// ============================================================================

namespace Style {
    constexpr int Bold = 1;
    constexpr int Italic = 2;
    constexpr int Mono = 4;
    constexpr int Sub = 8;
    constexpr int Sup = 16;
    constexpr int Underline = 32;
}

inline const std::vector<uint16_t>& regularMetricsTable() noexcept {
    static const std::vector<uint16_t> t = []() {
        const std::string REGULAR =
            "7q7q9vfgfgopij5b9999atg87q997q7qfgfgfgfgfgfgfgfgfgfg7q7qg8g8g8fgs7ijijk2k2ijgzlmk27qdwijfgn5k2lmijlmk2ijgzk2ijq8ijijgz7q7q7qd1fg99fgfgdwfgfg7qfgfg6666dw66n5fgfgfgfg99dw7qfgdwk2dwdwdw9a789ag8";
        std::vector<uint16_t> out(95);
        for (size_t i = 0; i < 95; i++) {
            out[i] = static_cast<uint16_t>(std::stoul(REGULAR.substr(i * 2, 2), nullptr, 36));
        }
        return out;
    }();
    return t;
}

inline const std::vector<uint16_t>& boldMetricsTable() noexcept {
    static const std::vector<uint16_t> t = []() {
        const std::string BOLD =
            "7q99d6fgfgopk26m9999atg87q997q7qfgfgfgfgfgfgfgfgfgfg9999g8g8g8gzr3k2k2k2k2ijgzlmk27qfgk2gzn5k2lmijlmk2ijgzk2ijq8ijijgz997q99g8fg99fggzfggzfg99gzgz7q7qfg7qopgzgzgzgzatfg99gzfglmfgfgdwat7satg8";
        std::vector<uint16_t> out(95);
        for (size_t i = 0; i < 95; i++) {
            out[i] = static_cast<uint16_t>(std::stoul(BOLD.substr(i * 2, 2), nullptr, 36));
        }
        return out;
    }();
    return t;
}

inline bool isWideCodeUnit(uint16_t c) noexcept {
    return (
        (c >= 0x1100 && c <= 0x115f) ||
        (c >= 0x2e80 && c <= 0xa4cf) ||
        (c >= 0xac00 && c <= 0xd7a3) ||
        (c >= 0xf900 && c <= 0xfaff) ||
        (c >= 0xfe30 && c <= 0xfe4f) ||
        (c >= 0xff00 && c <= 0xff60) ||
        (c >= 0xffe0 && c <= 0xffe6) ||
        (c >= 0xd800 && c <= 0xdbff)
    );
}

inline bool isZeroWidthCodeUnit(uint16_t c) noexcept {
    return (
        c == 0xad ||
        (c >= 0x300 && c <= 0x36f) ||
        (c >= 0x200b && c <= 0x200f) ||
        (c >= 0xdc00 && c <= 0xdfff) ||
        (c >= 0xfe00 && c <= 0xfe0f) ||
        c == 0xfeff
    );
}

class MetricsMeasurer : public TextMeasurer {
public:
    double width(const std::string& text, double size, int style = 0) const override {
        constexpr double SMALLER = 1.2;
        bool mono = (style & Style::Mono) != 0;
        double eff = (style & (Style::Sub | Style::Sup)) ? (size / SMALLER) : size;
        const auto& table = (style & Style::Bold) ? boldMetricsTable() : regularMetricsTable();
        auto u16 = toUtf16(text);
        double units = 0;
        for (uint16_t c : u16) {
            if (c < 32) continue;
            if (c < 127) units += mono ? 600 : table[c - 32];
            else if (isZeroWidthCodeUnit(c)) continue;
            else if (isWideCodeUnit(c)) units += mono ? 1200 : 1000;
            else units += mono ? 600 : 580;
        }
        return (units * eff) / 1000.0;
    }
};

inline const TextMeasurer& defaultMeasurer() noexcept {
    static const MetricsMeasurer instance;
    return instance;
}

inline const TextMeasurer& getMeasurer(const RenderOptions& options) noexcept {
    return options.measurer ? *options.measurer : defaultMeasurer();
}

// ============================================================================
// src/text/icons.ts & src/text/math.ts
// ============================================================================

inline IconResolver& activeIconResolver() noexcept {
    static thread_local IconResolver resolver;
    return resolver;
}

inline Pele::FlatMap<std::string, std::string>& activeIconCache() noexcept {
    static thread_local Pele::FlatMap<std::string, std::string> cache;
    return cache;
}

template <typename Fn>
inline auto withIcons(const IconResolver& icons, Fn&& run) -> decltype(run()) {
    auto outer = activeIconResolver();
    auto outerCache = activeIconCache();
    activeIconResolver() = icons;
    activeIconCache().clear();
    struct Guard {
        IconResolver outer;
        Pele::FlatMap<std::string, std::string> outerCache;
        ~Guard() {
            activeIconResolver() = std::move(outer);
            activeIconCache() = std::move(outerCache);
        }
    } guard{std::move(outer), std::move(outerCache)};
    return run();
}

inline std::string iconMarkup(const std::string& name) noexcept {
    auto& resolver = activeIconResolver();
    if (!resolver) return "";
    auto& cache = activeIconCache();
    auto it = cache.find(name);
    if (it != cache.end()) return it->second;
    std::string m = resolver(name);
    cache[name] = m;
    return m;
}

inline MathMarkupResolver& activeMathResolver() noexcept {
    static thread_local MathMarkupResolver resolver;
    return resolver;
}

inline bool mathActive() noexcept {
    return static_cast<bool>(activeMathResolver());
}

template <typename Fn>
inline auto withMath(const MathMarkupResolver& math, Fn&& run) -> decltype(run()) {
    auto outer = activeMathResolver();
    activeMathResolver() = math;
    struct Guard {
        MathMarkupResolver outer;
        ~Guard() { activeMathResolver() = std::move(outer); }
    } guard{std::move(outer)};
    return run();
}

template <typename Fn>
inline auto withoutMath(Fn&& run) -> decltype(run()) {
    auto outer = activeMathResolver();
    activeMathResolver() = nullptr;
    struct Guard {
        MathMarkupResolver outer;
        ~Guard() { activeMathResolver() = std::move(outer); }
    } guard{std::move(outer)};
    return run();
}

inline bool typesetMathSpan(const std::string& tex, double size, MathMarkup& out) noexcept {
    auto& resolver = activeMathResolver();
    if (!resolver) return false;
    MathMarkup res;
    if (resolver(tex, size, res) && std::isfinite(res.width) && res.width > 0 && std::isfinite(res.height) && res.height > 0 && (!res.svg.empty() || !res.html.empty())) {
        out = res;
        return true;
    }
    return false;
}

// ============================================================================
// src/text/label.ts
// ============================================================================

struct Span {
    std::string text;
    int style = 0;
    std::string icon;
    bool hasIcon = false;
    MathMarkup math;
    bool hasMath = false;
    double width = 0.0;
};

struct Label {
    std::vector<std::vector<Span>> lines;
    std::vector<double> widths;
    double width = 0.0;
    double height = 0.0;
    double size = 0.0;
    double lineHeight = 0.0;
    std::vector<double> heights;
    bool hasHeights = false;
};

constexpr double LINE_HEIGHT = 1.5;
constexpr double WRAPPED_LINE_HEIGHT = 1.3;

inline bool isPlainSpan(const Span& s) noexcept {
    return !s.hasIcon && !s.hasMath;
}

inline bool isCjkCodepoint(uint32_t cp) noexcept {
    return (cp >= 0x2e80 && cp <= 0x9fff) || (cp >= 0xf900 && cp <= 0xfaff) || (cp >= 0xff00 && cp <= 0xff60);
}

inline bool isHintCodepoint(uint32_t cp) noexcept {
    return cp == 0x200b || cp == 0x00ad;
}

inline bool isOpenCodepoint(uint32_t cp) noexcept {
    switch (cp) {
        case 0xFF08: case 0xFF3B: case 0xFF5B: case 0x300C: case 0x300E:
        case 0x3010: case 0x3014: case 0x3008: case 0x300A: case 0x3018:
        case 0x3016: case '(': case '[': case '{':
            return true;
        default:
            return false;
    }
}

inline bool isCloseCodepoint(uint32_t cp) noexcept {
    // 、。，．：；！？）］｝」』】〕〉》〙〗ー…‥・ゝゞぁぃぅぇぉっゃゅょゎァィゥェォッャュョヮ,.:;!?)\\]}
    switch (cp) {
        case 0x3001: case 0x3002: case 0xFF0C: case 0xFF0E: case 0xFF1A: case 0xFF1B:
        case 0xFF01: case 0xFF1F: case 0xFF09: case 0xFF3D: case 0xFF5D: case 0x300D:
        case 0x300F: case 0x3011: case 0x3015: case 0x3009: case 0x300B: case 0x3019:
        case 0x3017: case 0x30FC: case 0x2026: case 0x2025: case 0x30FB: case 0x309D:
        case 0x309E: case 0x3041: case 0x3043: case 0x3045: case 0x3047: case 0x3049:
        case 0x3063: case 0x3083: case 0x3085: case 0x3087: case 0x308E: case 0x30A1:
        case 0x30A3: case 0x30A5: case 0x30A7: case 0x30A9: case 0x30C3: case 0x30E3:
        case 0x30E5: case 0x30E7: case 0x30EE:
        case ',': case '.': case ':': case ';': case '!': case '?': case ')': case ']': case '}':
            return true;
        default:
            return false;
    }
}

inline bool hasCjkOrHint(const std::string& s) noexcept {
    size_t i = 0;
    while (i < s.size()) {
        uint32_t cp = nextCodepoint(s, i);
        if (isCjkCodepoint(cp) || isHintCodepoint(cp)) return true;
    }
    return false;
}

// Matches RE_PIECE = /\s*(?:[OPEN]*[CJK][CLOSE]*|[^\sCJKHINT]+[HINT]*[CLOSE]*|[HINT]+)\s*|\s+/g
inline std::vector<std::string> splitWrapPieces(const std::string& text) noexcept {
    struct CpSlice {
        uint32_t cp;
        size_t start;
        size_t end;
    };
    std::vector<CpSlice> cps;
    size_t idx = 0;
    while (idx < text.size()) {
        size_t s = idx;
        uint32_t cp = nextCodepoint(text, idx);
        cps.push_back({cp, s, idx});
    }
    std::vector<std::string> pieces;
    size_t i = 0;
    size_t n = cps.size();
    while (i < n) {
        size_t pieceStart = cps[i].start;
        while (i < n && isSpace(static_cast<int>(cps[i].cp))) i++;
        if (i >= n) {
            pieces.push_back(text.substr(pieceStart));
            break;
        }
        // Try [OPEN]*[CJK][CLOSE]*
        size_t j = i;
        while (j < n && isOpenCodepoint(cps[j].cp)) j++;
        if (j < n && isCjkCodepoint(cps[j].cp)) {
            j++;
            while (j < n && isCloseCodepoint(cps[j].cp)) j++;
            i = j;
        } else if (!isSpace(static_cast<int>(cps[i].cp)) && !isCjkCodepoint(cps[i].cp) && !isHintCodepoint(cps[i].cp)) {
            while (i < n && !isSpace(static_cast<int>(cps[i].cp)) && !isCjkCodepoint(cps[i].cp) && !isHintCodepoint(cps[i].cp)) i++;
            while (i < n && isHintCodepoint(cps[i].cp)) i++;
            while (i < n && isCloseCodepoint(cps[i].cp)) i++;
        } else if (isHintCodepoint(cps[i].cp)) {
            while (i < n && isHintCodepoint(cps[i].cp)) i++;
        }
        while (i < n && isSpace(static_cast<int>(cps[i].cp))) i++;
        size_t pieceEnd = (i < n) ? cps[i].start : text.size();
        pieces.push_back(text.substr(pieceStart, pieceEnd - pieceStart));
    }
    return pieces;
}

inline std::string dedentLabel(const std::string& text) noexcept {
    auto lines = splitStr(text, '\n');
    size_t minIndent = std::numeric_limits<size_t>::max();
    for (const auto& line : lines) {
        if (trimStr(line).empty()) continue;
        size_t indent = line.size() - trimStartStr(line).size();
        if (indent < minIndent) minIndent = indent;
    }
    if (minIndent == std::numeric_limits<size_t>::max() || minIndent == 0) return text;
    std::string out;
    for (size_t i = 0; i < lines.size(); i++) {
        if (i > 0) out.push_back('\n');
        size_t maxTrim = lines[i].size() - trimStartStr(lines[i]).size();
        out += lines[i].substr(std::min(minIndent, maxTrim));
    }
    return out;
}

inline const Pele::FlatMap<std::string, int>& tagStylesMap() noexcept {
    static const Pele::FlatMap<std::string, int> m = {
        {"b", Style::Bold}, {"strong", Style::Bold},
        {"i", Style::Italic}, {"em", Style::Italic}, {"cite", Style::Italic}, {"dfn", Style::Italic}, {"var", Style::Italic},
        {"code", Style::Mono}, {"kbd", Style::Mono}, {"samp", Style::Mono}, {"tt", Style::Mono},
        {"sub", Style::Sub}, {"sup", Style::Sup},
        {"u", Style::Underline}, {"ins", Style::Underline}
    };
    return m;
}

inline const Pele::FlatSet<std::string>& htmlTagsSet() noexcept {
    static const Pele::FlatSet<std::string> s = {
        "a","abbr","address","article","aside","audio","b","bdi","bdo","big","blockquote","body","br","button","caption","center","cite","code","col",
        "colgroup","data","dd","del","details","dfn","dialog","div","dl","dt","em","embed","fieldset","figcaption","figure","font","footer","form","h1","h2",
        "h3","h4","h5","h6","head","header","hr","html","i","iframe","img","input","ins","kbd","label","legend","li","link","main","map","mark","meta","meter","nav",
        "noscript","object","ol","optgroup","option","output","p","picture","pre","progress","q","rp","rt","ruby","s","samp","script","section","select",
        "small","source","span","strike","strong","style","sub","summary","sup","table","tbody","td","template","textarea","tfoot","th","thead","time",
        "title","tr","track","tt","u","ul","var","video","wbr"
    };
    return s;
}

inline void pushPlainSpan(std::vector<Span>& line, const std::string& text, int style) noexcept {
    if (text.empty()) return;
    std::string decoded = decodeEntities(text);
    if (!line.empty() && isPlainSpan(line.back()) && line.back().style == style) {
        line.back().text += decoded;
    } else {
        Span s;
        s.text = decoded;
        s.style = style;
        line.push_back(std::move(s));
    }
}

struct MathMarks {
    std::vector<std::string> tex;
    std::vector<MathMarkup> math;
    std::vector<bool> hasMath;
};

inline void pushTextSpan(std::vector<Span>& line, const std::string& text, int style, const MathMarks* marks) noexcept {
    if (text.empty()) return;
    if (marks && !marks->tex.empty()) {
        std::string plainBuf;
        size_t i = 0;
        while (i < text.size()) {
            size_t s = i;
            uint32_t cp = nextCodepoint(text, i);
            if (cp >= 0xe000 && cp <= 0xf8ff) {
                if (!plainBuf.empty()) {
                    pushPlainSpan(line, plainBuf, style);
                    plainBuf.clear();
                }
                size_t at = cp - 0xe000;
                if (at < marks->tex.size()) {
                    if (marks->hasMath[at]) {
                        Span sp;
                        sp.style = style;
                        sp.math = marks->math[at];
                        sp.hasMath = true;
                        sp.width = sp.math.width;
                        line.push_back(std::move(sp));
                    } else {
                        pushPlainSpan(line, "$$" + marks->tex[at] + "$$", style);
                    }
                }
            } else {
                plainBuf.append(text, s, i - s);
            }
        }
        if (!plainBuf.empty()) pushPlainSpan(line, plainBuf, style);
        return;
    }
    pushPlainSpan(line, text, style);
}

inline void pushIconsSpan(std::vector<Span>& line, const std::string& text, int style, const MathMarks* marks) noexcept {
    if (text.find(":fa-") == std::string::npos) {
        pushTextSpan(line, text, style, marks);
        return;
    }
    // RE_ICON = /(fa[bklrs]?):fa-([\w-]+)/g
    size_t last = 0;
    size_t pos = 0;
    while ((pos = text.find(":fa-", pos)) != std::string::npos) {
        size_t pStart = pos;
        if (pStart >= 3 && text.substr(pStart - 3, 2) == "fa") {
            char c = text[pStart - 1];
            if (c == 'b' || c == 'k' || c == 'l' || c == 'r' || c == 's') pStart -= 3;
            else pStart = std::string::npos;
        } else if (pStart >= 2 && text.substr(pStart - 2, 2) == "fa") {
            pStart -= 2;
        } else {
            pStart = std::string::npos;
        }
        if (pStart != std::string::npos && pStart >= last) {
            size_t nameStart = pos + 4;
            size_t nameEnd = nameStart;
            while (nameEnd < text.size() && (isWord(static_cast<unsigned char>(text[nameEnd])) || text[nameEnd] == '-')) nameEnd++;
            if (nameEnd > nameStart) {
                std::string icon = text.substr(pStart, nameEnd - pStart);
                pushTextSpan(line, text.substr(last, pStart - last), style, marks);
                last = nameEnd;
                if (!iconMarkup(icon).empty()) {
                    Span sp;
                    sp.style = style;
                    sp.icon = icon;
                    sp.hasIcon = true;
                    line.push_back(std::move(sp));
                } else if (last < text.size() && text[last] == ' ') {
                    last++;
                } else if (last == text.size()) {
                    if (!line.empty() && isPlainSpan(line.back())) {
                        line.back().text = trimEndStr(line.back().text);
                    }
                }
                pos = last;
                continue;
            }
        }
        pos += 4;
    }
    pushTextSpan(line, text.substr(last), style, marks);
}

struct OpenStyleState {
    int style = 0;
    std::vector<int> shifts;
};

inline void parseInlineSpans(std::vector<Span>& line, const std::string& source, bool markdown, int base, OpenStyleState& open, const MathMarks* marks) noexcept {
    int style = open.style;
    int marked = 0;
    std::string text;
    size_t i = 0;
    size_t n = source.size();
    const auto& tagStyles = tagStylesMap();
    const auto& htmlTags = htmlTagsSet();
    constexpr int SHIFT = Style::Sub | Style::Sup;

    auto flush = [&]() {
        int shift = open.shifts.empty() ? 0 : open.shifts.back();
        pushIconsSpan(line, text, style | shift, marks);
        text.clear();
    };

    while (i < n) {
        char ch = source[i];
        if (ch == '<') {
            // RE_TAG = /<\/?([a-zA-Z][\w-]*)(?:\s[^<>]*)?\/?>/y
            size_t p = i + 1;
            bool closing = false;
            if (p < n && source[p] == '/') { closing = true; p++; }
            size_t tagStart = p;
            if (p < n && isLetter(static_cast<unsigned char>(source[p]))) {
                p++;
                while (p < n && (isWord(static_cast<unsigned char>(source[p])) || source[p] == '-')) p++;
                std::string rawTag = source.substr(tagStart, p - tagStart);
                bool validRest = false;
                if (p < n && source[p] == '>') {
                    validRest = true;
                    p++;
                } else if (p + 1 < n && source[p] == '/' && source[p + 1] == '>') {
                    validRest = true;
                    p += 2;
                } else if (p < n && isSpace(static_cast<unsigned char>(source[p]))) {
                    while (p < n && source[p] != '<' && source[p] != '>') p++;
                    if (p < n && source[p] == '>') {
                        validRest = true;
                        p++;
                    }
                }
                if (validRest) {
                    std::string tag = toLowerAscii(rawTag);
                    auto it = tagStyles.find(tag);
                    int bit = (it != tagStyles.end()) ? it->second : 0;
                    if (bit & SHIFT) {
                        flush();
                        if (!closing) {
                            open.shifts.push_back(bit);
                        } else {
                            for (int k = static_cast<int>(open.shifts.size()) - 1; k >= 0; k--) {
                                if (open.shifts[k] == bit) {
                                    open.shifts.erase(open.shifts.begin() + k);
                                    break;
                                }
                            }
                        }
                    } else if (bit) {
                        flush();
                        open.style = closing ? ((open.style & ~bit) | (base & bit)) : (open.style | bit);
                        style = closing ? ((style & ~bit) | (base & bit)) : (style | bit);
                    }
                    if (bit || htmlTags.count(tag)) {
                        i = p;
                        continue;
                    }
                }
            }
        } else if (markdown && (ch == '*' || ch == '_')) {
            bool dbl = (i + 1 < n && source[i + 1] == ch);
            int bit = dbl ? Style::Bold : Style::Italic;
            size_t len = dbl ? 2 : 1;
            bool isOpen = (marked & bit) == 0;
            char prev = i > 0 ? source[i - 1] : ' ';
            char next = (i + len < n) ? source[i + len] : ' ';
            std::string needle = dbl ? std::string(2, ch) : std::string(1, ch);
            bool canOpen = isOpen && !isSpace(static_cast<unsigned char>(next)) && source.find(needle, i + len) != std::string::npos;
            bool canClose = !isOpen && !isSpace(static_cast<unsigned char>(prev));
            bool wordy = (ch == '_') && isWord(static_cast<unsigned char>(prev)) && isWord(static_cast<unsigned char>(next));
            if ((canOpen || canClose) && !wordy) {
                flush();
                marked = canOpen ? (marked | bit) : (marked & ~bit);
                style = canOpen ? (style | bit) : ((style & ~bit) | (base & bit));
                i += len;
                continue;
            }
        }
        text.push_back(ch);
        i++;
    }
    flush();
}

inline double measureSpans(std::vector<Span>& line, const TextMeasurer& measurer, double size) noexcept {
    double total = 0;
    for (auto& span : line) {
        span.width = span.hasIcon ? (size * 1.25) : span.hasMath ? span.math.width : measurer.width(span.text, size, span.style);
        total += span.width;
    }
    return total;
}

inline void wrapLineSpans(const std::vector<Span>& line, const TextMeasurer& measurer, double size, double max, std::vector<std::vector<Span>>& out, std::vector<double>& widths) noexcept {
    std::vector<Span> current;
    double width = 0;
    const std::string SHY_UTF8 = "\xC2\xAD";

    auto commit = [&](bool hyphen = false) {
        if (!current.empty() && isPlainSpan(current.back())) {
            auto& last = current.back();
            std::string trimmed = trimEndStr(last.text);
            if (hyphen && endsWith(last.text, SHY_UTF8)) {
                trimmed = Pele::cat({trimmed.substr(0, last.text.size() - SHY_UTF8.size()), "-"});
            }
            if (trimmed != last.text) {
                last.text = trimmed;
                double w = measurer.width(trimmed, size, last.style);
                width += w - last.width;
                last.width = w;
            }
        }
        out.push_back(std::move(current));
        widths.push_back(width);
        current.clear();
        width = 0;
    };

    for (size_t sIdx = 0; sIdx < line.size(); sIdx++) {
        const auto& span = line[sIdx];
        if (span.hasIcon || span.hasMath) {
            double w = span.hasMath ? span.math.width : (size * 1.25);
            if (width > 0 && width + w > max) commit();
            Span copy = span;
            copy.width = w;
            current.push_back(std::move(copy));
            width += w;
            continue;
        }
        auto pieces = splitWrapPieces(span.text);
        bool soft = span.text.find(SHY_UTF8) != std::string::npos;
        size_t n = soft ? pieces.size() : 0;
        std::vector<double> own(n, 0.0);
        std::vector<double> rest(n + 1, 0.0);
        if (soft) {
            for (int q = static_cast<int>(n) - 1; q >= 0; q--) {
                bool shy = endsWith(pieces[q], SHY_UTF8);
                std::string end = trimEndStr(pieces[q]);
                own[q] = measurer.width(shy ? end.substr(0, end.size() - SHY_UTF8.size()) : end, size, span.style);
                rest[q] = !shy ? own[q] : (static_cast<size_t>(q) == n - 1 ? (sIdx == line.size() - 1 ? own[q] : std::numeric_limits<double>::infinity()) : own[q] + rest[q + 1]);
            }
        }
        for (size_t p = 0; p < pieces.size(); p++) {
            std::string part = pieces[p];
            double partWidth = soft ? own[p] : measurer.width(trimEndStr(part), size, span.style);
            bool fits = (!soft || !endsWith(part, SHY_UTF8))
                ? (width + partWidth <= max)
                : (width + measurer.width(Pele::cat({part.substr(0, part.size() - SHY_UTF8.size()), "-"}),  size,  span.style) <= max || width + rest[p] <= max);
            if (width > 0 && !fits) {
                commit(trimStartStr(part) == part);
                part = trimStartStr(part);
            }
            if (!current.empty() && isPlainSpan(current.back()) && current.back().style == span.style) {
                auto& last = current.back();
                last.text += part;
                if (partWidth > 0 || trimEndStr(part) != part) {
                    double before = last.width;
                    last.width = measurer.width(last.text, size, last.style);
                    width += last.width - before;
                }
            } else {
                double w = measurer.width(part, size, span.style);
                Span sp;
                sp.text = part;
                sp.style = span.style;
                sp.width = w;
                current.push_back(std::move(sp));
                width += w;
            }
        }
    }
    if (!current.empty() || out.empty()) commit();
}

// Splits string by RE_BREAK = /<br\s*\/?>|\\n|\n/gi
inline std::vector<std::string> splitLabelBreaks(const std::string& s) noexcept {
    std::vector<std::string> parts;
    size_t last = 0;
    size_t i = 0;
    while (i < s.size()) {
        if (s[i] == '\n') {
            parts.push_back(s.substr(last, i - last));
            i++;
            last = i;
        } else if (i + 1 < s.size() && s[i] == '\\' && (s[i + 1] == 'n' || s[i + 1] == 'N')) {
            parts.push_back(s.substr(last, i - last));
            i += 2;
            last = i;
        } else if (i + 3 < s.size() && s[i] == '<' && (s[i + 1] == 'b' || s[i + 1] == 'B') && (s[i + 2] == 'r' || s[i + 2] == 'R')) {
            size_t p = i + 3;
            while (p < s.size() && isSpace(static_cast<unsigned char>(s[p]))) p++;
            if (p < s.size() && s[p] == '/') p++;
            if (p < s.size() && s[p] == '>') {
                parts.push_back(s.substr(last, i - last));
                i = p + 1;
                last = i;
                continue;
            }
            i++;
        } else {
            i++;
        }
    }
    parts.push_back(s.substr(last));
    return parts;
}

inline Label withHeights(Label label) noexcept {
    std::vector<double> heights;
    bool needed = false;
    for (size_t i = 0; i < label.lines.size(); i++) {
        double h = label.lineHeight;
        for (const auto& span : label.lines[i]) {
            if (span.hasMath) h = std::max(h, std::ceil(span.math.height + label.size / 4.0));
        }
        if (h != label.lineHeight && !needed) {
            needed = true;
            heights.assign(label.lines.size(), label.lineHeight);
        }
        if (needed) heights[i] = h;
    }
    if (!needed) {
        if (label.hasHeights) {
            label.heights.clear();
            label.hasHeights = false;
            label.height = static_cast<double>(label.lines.size()) * label.lineHeight;
        }
        return label;
    }
    double total = 0;
    for (double h : heights) total += h;
    label.heights = std::move(heights);
    label.hasHeights = true;
    label.height = total;
    return label;
}

inline Label layoutLines(const std::string& raw, bool markdown, const TextMeasurer& measurer, double size, double maxWidth, int base, double lineHeight, const MathMarks* marks = nullptr) noexcept {
    std::string source = markdown ? trimStr(dedentLabel(normalizeLineBreaks(raw))) : trimStr(raw);
    std::vector<std::vector<Span>> lines;
    std::vector<double> widths;
    double width = 0;
    OpenStyleState open{base, {}};
    for (const auto& part : splitLabelBreaks(source)) {
        std::vector<Span> line;
        parseInlineSpans(line, trimStr(part), markdown, base, open, marks);
        double w = measureSpans(line, measurer, size);
        if (w > maxWidth) {
            wrapLineSpans(line, measurer, size, maxWidth, lines, widths);
        } else {
            lines.push_back(std::move(line));
            widths.push_back(w);
        }
    }
    for (double w : widths) if (w > width) width = w;
    double snug = lines.size() > 1 ? std::round(size * WRAPPED_LINE_HEIGHT) : lineHeight;
    Label res;
    res.lines = std::move(lines);
    res.widths = std::move(widths);
    res.width = width;
    res.height = static_cast<double>(res.lines.size()) * snug;
    res.size = size;
    res.lineHeight = snug;
    return marks ? withHeights(std::move(res)) : res;
}

inline bool hasMarkupChars(const std::string& s) noexcept {
    // RE_MARKUP = /[<\\\n*_]|fa[bklrs]?:fa-/
    for (char c : s) {
        if (c == '<' || c == '\\' || c == '\n' || c == '*' || c == '_' || c == '&') return true;
    }
    if (s.find(":fa-") != std::string::npos) return true;
    if (s.find("\xEF\xAC\x82") != std::string::npos) return true; // 'ﬂ'
    return false;
}

inline Label layoutLabel(const std::string& raw, bool markdown, const TextMeasurer& measurer, double size, double maxWidth, int base = 0) noexcept {
    if (raw.empty()) return Label{};
    double lineHeight = std::round(size * LINE_HEIGHT);

    if (mathActive() && raw.find("$$") != std::string::npos) {
        MathMarks found;
        std::string marked;
        size_t i = 0;
        while (i < raw.size()) {
            size_t mStart = raw.find("$$", i);
            if (mStart == std::string::npos) {
                marked += raw.substr(i);
                break;
            }
            size_t mEnd = raw.find("$$", mStart + 2);
            if (mEnd == std::string::npos || mEnd == mStart + 2 || found.tex.size() >= 0x1900) {
                marked += raw.substr(i);
                break;
            }
            marked += raw.substr(i, mStart - i);
            std::string tex = raw.substr(mStart + 2, mEnd - (mStart + 2));
            found.tex.push_back(tex);
            appendCodepoint(marked, static_cast<uint32_t>(0xe000 + found.tex.size() - 1));
            i = mEnd + 2;
        }
        if (!found.tex.empty()) {
            for (const auto& tex : found.tex) {
                MathMarkup mm;
                bool ok = typesetMathSpan(decodeEntities(tex), size, mm);
                found.math.push_back(std::move(mm));
                found.hasMath.push_back(ok);
            }
            return layoutLines(marked, markdown, measurer, size, maxWidth, base, lineHeight, &found);
        }
    }

    if (!hasMarkupChars(raw)) {
        std::string text = trimStr(raw);
        double width = measurer.width(text, size, base);
        if (width <= maxWidth || (text.find(' ') == std::string::npos && !hasCjkOrHint(text))) {
            Span sp;
            sp.text = text;
            sp.style = base;
            sp.width = width;
            Label res;
            res.lines = {{std::move(sp)}};
            res.widths = {width};
            res.width = width;
            res.height = lineHeight;
            res.size = size;
            res.lineHeight = lineHeight;
            return res;
        }
        std::vector<std::vector<Span>> lines;
        std::vector<double> widths;
        Span sp;
        sp.text = text;
        sp.style = base;
        sp.width = width;
        wrapLineSpans({sp}, measurer, size, maxWidth, lines, widths);
        double widest = 0;
        for (double w : widths) if (w > widest) widest = w;
        double snug = lines.size() > 1 ? std::round(size * WRAPPED_LINE_HEIGHT) : lineHeight;
        Label res;
        res.lines = std::move(lines);
        res.widths = std::move(widths);
        res.width = widest;
        res.height = static_cast<double>(res.lines.size()) * snug;
        res.size = size;
        res.lineHeight = snug;
        return res;
    }

    return layoutLines(raw, markdown, measurer, size, maxWidth, base, lineHeight, nullptr);
}

inline bool hasMath(const std::vector<Span>& line) noexcept {
    for (const auto& s : line) if (s.hasMath) return true;
    return false;
}

inline double lineMiddle(const Label& label, size_t i) noexcept {
    if (!label.hasHeights) return (static_cast<double>(i) + 0.5) * label.lineHeight;
    double top = 0;
    for (size_t k = 0; k < i; k++) top += label.heights[k];
    return top + label.heights[i] / 2.0;
}

inline Label labelLine(const Label& label, size_t i) noexcept {
    double width = label.widths[i];
    double height = label.hasHeights ? label.heights[i] : label.lineHeight;
    Label line;
    line.lines = {label.lines[i]};
    line.widths = {width};
    line.width = width;
    line.height = height;
    line.size = label.size;
    line.lineHeight = label.lineHeight;
    if (height != label.lineHeight) {
        line.heights = {height};
        line.hasHeights = true;
    }
    return line;
}

// ============================================================================
// src/diagrams/common/literal-label.ts (literalLabel, breakLabel)
// ============================================================================

inline Label literalLabel(const std::string& text, const TextMeasurer& measurer, double size, double room, int style = 0) noexcept {
    std::vector<std::string> lines;
    for (const auto& raw : splitStr(text, '\n')) {
        std::vector<std::string> chars;
        size_t idx = 0;
        while (idx < raw.size()) {
            size_t s = idx;
            nextCodepoint(raw, idx);
            chars.push_back(raw.substr(s, idx - s));
        }
        if (chars.empty()) lines.push_back("");
        size_t from = 0;
        while (from < chars.size()) {
            auto joinSlice = [&](size_t f, size_t len) {
                std::string res;
                for (size_t k = f; k < f + len && k < chars.size(); k++) res += chars[k];
                return res;
            };
            auto fits = [&](size_t length) {
                return measurer.width(joinSlice(from, length), size, style) <= room;
            };
            size_t hi = 1;
            while (hi < chars.size() - from && fits(hi)) hi = std::min(chars.size() - from, hi * 2);
            size_t lo = 1;
            while (lo < hi) {
                size_t mid = (lo + hi + 1) / 2;
                if (fits(mid)) lo = mid;
                else hi = mid - 1;
            }
            size_t take = lo;
            bool wraps = (from + take < chars.size());
            auto isSpaceChar = [&](const std::string& ch) {
                size_t d = 0;
                return isSpace(charCodeAtUtf8(ch, 0, &d));
            };
            if (wraps && !isSpaceChar(chars[from + take])) {
                for (size_t i = take - 1; i > 0; i--) {
                    if (isSpaceChar(chars[from + i])) { take = i + 1; break; }
                }
            }
            std::string line = joinSlice(from, take);
            if (wraps) {
                std::string t = trimEndStr(line);
                lines.push_back(t.empty() ? line : t);
            } else {
                lines.push_back(line);
            }
            from += take;
            while (wraps && from < chars.size() && isSpaceChar(chars[from])) from++;
        }
    }
    if (text.empty()) lines.clear();
    Label out;
    out.size = size;
    out.lineHeight = std::round(size * 1.5);
    for (const auto& l : lines) {
        double w = measurer.width(l, size, style);
        Span sp;
        sp.text = l;
        sp.style = style;
        sp.width = w;
        out.lines.push_back({std::move(sp)});
        out.widths.push_back(w);
        out.width = std::max(out.width, w);
    }
    out.height = static_cast<double>(lines.size()) * out.lineHeight;
    return out;
}

inline Label breakLabel(const Label& label, const TextMeasurer& measurer, double room) noexcept {
    if (label.width <= room) return label;
    std::vector<std::vector<Span>> lines;
    std::vector<double> widths;
    for (size_t i = 0; i < label.lines.size(); i++) {
        if (label.widths[i] <= room) {
            lines.push_back(label.lines[i]);
            widths.push_back(label.widths[i]);
            continue;
        }
        std::vector<Span> current;
        double width = 0;
        auto commit = [&]() {
            lines.push_back(std::move(current));
            widths.push_back(width);
            current.clear();
            width = 0;
        };
        for (const auto& span : label.lines[i]) {
            if (span.hasIcon || span.hasMath) {
                if (!current.empty() && width + span.width > room) commit();
                current.push_back(span);
                width += span.width;
                continue;
            }
            size_t idx = 0;
            while (idx < span.text.size()) {
                size_t s = idx;
                nextCodepoint(span.text, idx);
                std::string ch = span.text.substr(s, idx - s);
                bool joined = !current.empty() && isPlainSpan(current.back()) && current.back().style == span.style;
                double nextWidth = measurer.width(joined ? (current.back().text + ch) : ch, label.size, span.style);
                double added = nextWidth - (joined ? current.back().width : 0.0);
                if (!current.empty() && width + added > room) {
                    commit();
                    joined = false;
                    added = measurer.width(ch, label.size, span.style);
                }
                if (!current.empty() && isPlainSpan(current.back()) && current.back().style == span.style) {
                    current.back().text += ch;
                    current.back().width += added;
                } else {
                    Span sp;
                    sp.text = ch;
                    sp.style = span.style;
                    sp.width = added;
                    current.push_back(std::move(sp));
                }
                width += added;
            }
        }
        if (!current.empty() || label.lines[i].empty()) commit();
    }
    double maxW = 0;
    for (double w : widths) maxW = std::max(maxW, w);
    Label broken = label;
    broken.lines = std::move(lines);
    broken.widths = std::move(widths);
    broken.width = maxW;
    broken.height = static_cast<double>(broken.lines.size()) * label.lineHeight;
    return label.hasHeights ? withHeights(std::move(broken)) : broken;
}

} // namespace Pele
