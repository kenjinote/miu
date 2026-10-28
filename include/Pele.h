#pragma once
// Pele C++ Native Port - Zero-dependency Mermaid-to-SVG rendering engine
// Faithfully ported from Pele (https://pele.run / MIT License)

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <iomanip>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Pele {

struct MathBox {
    std::string svg;
    std::string html;
    double width = 0.0;
    double height = 0.0;
};

using MathResolver = std::function<MathBox(const std::string& tex, double fontSize)>;

struct RenderOptions {
    int fontSize = 16;
    int padding = 8;
    double maxWidth = 0.0;
    std::string idPrefix = "pele";
    std::string curve = "basis";
    MathResolver math = nullptr;
};

struct RenderResult {
    bool ok = false;
    std::string svg;
    std::string type;
    std::string mermaidType;
    double width = 0.0;
    double height = 0.0;
    std::string error;
};

namespace detail {

static constexpr double PI = 3.14159265358979323846;
static constexpr double kPi = PI;

inline std::string num(double n) {
    if (!std::isfinite(n)) return "0";
    double r = std::round(n * 100.0) / 100.0;
    if (std::abs(r) < 1e-9) return "0";
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.2f", r);
    std::string s(buf);
    while (!s.empty() && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
    return s;
}

inline std::string esc(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            default: out += c; break;
        }
    }
    return out;
}

inline std::string escText(const std::string& s) {
    return esc(s);
}

inline std::string trim(const std::string& s) {
    size_t a = 0;
    while (a < s.size() && (unsigned char)s[a] <= ' ') a++;
    size_t b = s.size();
    while (b > a && (unsigned char)s[b - 1] <= ' ') b--;
    return s.substr(a, b - a);
}

inline std::string trimRight(const std::string& s) {
    size_t b = s.size();
    while (b > 0 && (unsigned char)s[b - 1] <= ' ') b--;
    return s.substr(0, b);
}

inline std::string toLower(const std::string& s) {
    std::string out = s;
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
    }
    return out;
}

inline bool startsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

inline bool startsWithIgnoreCase(const std::string& s, const std::string& prefix) {
    if (s.size() < prefix.size()) return false;
    for (size_t i = 0; i < prefix.size(); i++) {
        char a = s[i];
        char b = prefix[i];
        if (a >= 'A' && a <= 'Z') a = char(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = char(b - 'A' + 'a');
        if (a != b) return false;
    }
    return true;
}

inline bool endsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

inline std::vector<std::string> splitLines(const std::string& s) {
    std::vector<std::string> lines;
    size_t start = 0;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '\n') {
            lines.push_back(s.substr(start, i - start));
            start = i + 1;
        }
    }
    lines.push_back(s.substr(start));
    return lines;
}

inline std::vector<std::string> splitChar(const std::string& s, char sep) {
    std::vector<std::string> parts;
    size_t start = 0;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == sep) {
            parts.push_back(s.substr(start, i - start));
            start = i + 1;
        }
    }
    parts.push_back(s.substr(start));
    return parts;
}

inline std::string unquote(const std::string& s) {
    std::string t = trim(s);
    if (t.size() >= 2 && ((t.front() == '"' && t.back() == '"') || (t.front() == '\'' && t.back() == '\''))) {
        return t.substr(1, t.size() - 2);
    }
    return t;
}

inline void appendUtf8(std::string& out, uint32_t cp) {
    if (cp <= 0x7F) {
        out += char(cp);
    } else if (cp <= 0x7FF) {
        out += char(0xC0 | (cp >> 6));
        out += char(0x80 | (cp & 0x3F));
    } else if (cp <= 0xFFFF) {
        out += char(0xE0 | (cp >> 12));
        out += char(0x80 | ((cp >> 6) & 0x3F));
        out += char(0x80 | (cp & 0x3F));
    } else if (cp <= 0x10FFFF) {
        out += char(0xF0 | (cp >> 18));
        out += char(0x80 | ((cp >> 12) & 0x3F));
        out += char(0x80 | ((cp >> 6) & 0x3F));
        out += char(0x80 | (cp & 0x3F));
    }
}

inline uint32_t nextUtf8(const std::string& s, size_t& i) {
    unsigned char c = (unsigned char)s[i++];
    if (c < 0x80) return c;
    if ((c & 0xE0) == 0xC0 && i < s.size()) {
        uint32_t cp = (c & 0x1F) << 6;
        cp |= ((unsigned char)s[i++] & 0x3F);
        return cp;
    }
    if ((c & 0xF0) == 0xE0 && i + 1 < s.size()) {
        uint32_t cp = (c & 0x0F) << 12;
        cp |= ((unsigned char)s[i++] & 0x3F) << 6;
        cp |= ((unsigned char)s[i++] & 0x3F);
        return cp;
    }
    if ((c & 0xF8) == 0xF0 && i + 2 < s.size()) {
        uint32_t cp = (c & 0x07) << 18;
        cp |= ((unsigned char)s[i++] & 0x3F) << 12;
        cp |= ((unsigned char)s[i++] & 0x3F) << 6;
        cp |= ((unsigned char)s[i++] & 0x3F);
        return cp;
    }
    return c;
}

inline std::string decodeEntities(const std::string& input) {
    static const std::unordered_map<std::string, uint32_t> named = {
        {"amp", 38}, {"lt", 60}, {"gt", 62}, {"quot", 34}, {"apos", 39}, {"nbsp", 160},
        {"sect", 167}, {"copy", 169}, {"reg", 174}, {"deg", 176}, {"plusmn", 177},
        {"sup2", 178}, {"sup3", 179}, {"micro", 181}, {"para", 182}, {"middot", 183},
        {"sup1", 185}, {"times", 215}, {"divide", 247}, {"ndash", 8211}, {"mdash", 8212},
        {"lsquo", 8216}, {"rsquo", 8217}, {"ldquo", 8220}, {"rdquo", 8221}, {"bull", 8226},
        {"hellip", 8230}, {"prime", 8242}, {"Prime", 8243}, {"euro", 8364}, {"trade", 8482},
        {"larr", 8592}, {"uarr", 8593}, {"rarr", 8594}, {"darr", 8595}, {"harr", 8596},
        {"lArr", 8656}, {"uArr", 8657}, {"rArr", 8658}, {"dArr", 8659}, {"hArr", 8660},
        {"forall", 8704}, {"part", 8706}, {"exist", 8707}, {"empty", 8709}, {"nabla", 8711},
        {"isin", 8712}, {"notin", 8713}, {"ni", 8715}, {"prod", 8719}, {"sum", 8721},
        {"minus", 8722}, {"radic", 8730}, {"infin", 8734}, {"ang", 8736}, {"and", 8743},
        {"or", 8744}, {"cap", 8745}, {"cup", 8746}, {"int", 8747}, {"sim", 8764},
        {"cong", 8773}, {"asymp", 8776}, {"ne", 8800}, {"equiv", 8801}, {"le", 8804},
        {"ge", 8805}, {"sub", 8834}, {"sup", 8835}, {"nsub", 8836}, {"sube", 8838},
        {"supe", 8839}, {"oplus", 8853}, {"otimes", 8855}, {"perp", 8869}, {"sdot", 8901},
        {"hearts", 9829}, {"spades", 9824}, {"clubs", 9827}, {"diams", 9830}
    };
    std::string out;
    out.reserve(input.size());
    for (size_t i = 0; i < input.size(); ) {
        if ((input[i] == '&' || input[i] == '#') && i + 2 < input.size()) {
            bool isAmp = (input[i] == '&');
            size_t semi = input.find(';', i + 1);
            if (semi != std::string::npos && semi - i <= 12) {
                std::string body = input.substr(i + 1, semi - i - 1);
                if (!isAmp && !body.empty()) {
                    // Mermaid #35; or #quot; entity
                    bool allDigits = true;
                    for (char c : body) if (!std::isdigit((unsigned char)c)) { allDigits = false; break; }
                    if (allDigits) {
                        uint32_t cp = (uint32_t)std::strtoul(body.c_str(), nullptr, 10);
                        if (cp > 0 && cp <= 0x10FFFF) { appendUtf8(out, cp); i = semi + 1; continue; }
                    } else {
                        auto it = named.find(body);
                        if (it != named.end()) { appendUtf8(out, it->second); i = semi + 1; continue; }
                    }
                } else if (isAmp && !body.empty()) {
                    if (body[0] == '#' && body.size() > 1) {
                        uint32_t cp = 0;
                        if (body[1] == 'x' || body[1] == 'X') {
                            cp = (uint32_t)std::strtoul(body.c_str() + 2, nullptr, 16);
                        } else {
                            cp = (uint32_t)std::strtoul(body.c_str() + 1, nullptr, 10);
                        }
                        if (cp > 0 && cp <= 0x10FFFF) { appendUtf8(out, cp); i = semi + 1; continue; }
                    } else {
                        auto it = named.find(body);
                        if (it != named.end()) { appendUtf8(out, it->second); i = semi + 1; continue; }
                    }
                }
            }
        }
        out += input[i++];
    }
    return out;
}

// ============================================================================
// Preprocessor & Directive Extractor (src/preprocess.ts)
// ============================================================================
struct Preprocessed {
    std::string code;
    std::string title;
    std::string displayMode;
    std::map<std::string, std::string> config;
};

inline Preprocessed preprocess(const std::string& rawInput) {
    Preprocessed res;
    std::string s;
    s.reserve(rawInput.size());
    for (size_t i = 0; i < rawInput.size(); i++) {
        if (rawInput[i] == '\r') {
            if (i + 1 < rawInput.size() && rawInput[i + 1] == '\n') i++;
            s += '\n';
        } else {
            s += rawInput[i];
        }
    }

    // 1. YAML Frontmatter --- ... ---
    size_t pos = 0;
    while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\n')) pos++;
    if (s.compare(pos, 3, "---") == 0 && (pos + 3 == s.size() || s[pos + 3] == '\n' || s[pos + 3] == ' ' || s[pos + 3] == '\t')) {
        size_t lineEnd = s.find('\n', pos);
        if (lineEnd != std::string::npos) {
            size_t endMarker = s.find("\n---", lineEnd);
            if (endMarker != std::string::npos) {
                std::string yaml = s.substr(lineEnd + 1, endMarker - (lineEnd + 1));
                for (const auto& line : splitLines(yaml)) {
                    std::string t = trim(line);
                    if (startsWith(t, "title:")) {
                        res.title = unquote(trim(t.substr(6)));
                    } else if (startsWith(t, "displayMode:")) {
                        res.displayMode = unquote(trim(t.substr(12)));
                    }
                }
                size_t afterClose = s.find('\n', endMarker + 1);
                s = (afterClose != std::string::npos) ? s.substr(afterClose + 1) : "";
            }
        }
    }

    // 2. Directives %%{...}%% and %% comments
    std::string cleaned;
    cleaned.reserve(s.size());
    auto lines = splitLines(s);
    for (size_t li = 0; li < lines.size(); li++) {
        std::string line = lines[li];
        std::string t = trim(line);
        if (startsWith(t, "%%{")) {
            size_t close = t.find("}%%");
            if (close != std::string::npos) {
                std::string json = t.substr(3, close - 3);
                // Extract simple key:value pairs like "curve": "linear" or "nodeSpacing": 50
                for (const char* key : {"curve", "nodeSpacing", "rankSpacing", "showData", "mirrorActors", "showSequenceNumbers"}) {
                    std::string pat = std::string("\"") + key + "\"";
                    size_t kp = json.find(pat);
                    if (kp == std::string::npos) kp = json.find(key);
                    if (kp != std::string::npos) {
                        size_t colon = json.find(':', kp);
                        if (colon != std::string::npos) {
                            size_t valStart = colon + 1;
                            while (valStart < json.size() && (json[valStart] == ' ' || json[valStart] == '\t')) valStart++;
                            size_t valEnd = valStart;
                            while (valEnd < json.size() && json[valEnd] != ',' && json[valEnd] != '}' && json[valEnd] != '\n') valEnd++;
                            res.config[key] = unquote(trim(json.substr(valStart, valEnd - valStart)));
                        }
                    }
                }
                continue;
            }
        }
        if (startsWith(t, "%%")) {
            continue;
        }
        cleaned += line;
        if (li + 1 < lines.size()) cleaned += '\n';
    }
    res.code = cleaned;
    return res;
}

// ============================================================================
// Diagram Type Detector (src/detect.ts  all 37 detectors in exact order)
// ============================================================================
struct Detected {
    std::string type;
    std::string mermaidType;
};

inline Detected detectDiagram(const std::string& text) {
    size_t i = 0;
    while (i < text.size() && (unsigned char)text[i] <= ' ') i++;
    std::string rest = text.substr(i);
    // Extract first token (letters, digits, hyphen, underscore)
    size_t tokEnd = 0;
    while (tokEnd < rest.size() && (std::isalnum((unsigned char)rest[tokEnd]) || rest[tokEnd] == '-' || rest[tokEnd] == '_')) {
        tokEnd++;
    }
    std::string firstTok = rest.substr(0, tokEnd);
    std::string firstLower = toLower(firstTok);
    char afterTok = (tokEnd < rest.size()) ? rest[tokEnd] : '\0';
    bool isBoundary = (afterTok == '\0' || (unsigned char)afterTok <= ' ' || afterTok == ';' || afterTok == ':');

    if (isBoundary) {
        if (firstTok == "flowchart-elk") return {"flowchart", "flowchart-elk"};
        if (firstTok == "flowchart") return {"flowchart", "flowchart"};
        if (firstTok == "graph") return {"flowchart", "graph"};
        if (firstTok == "sequenceDiagram") return {"sequence", "sequence"};
        if (firstTok == "classDiagram-v2") return {"class", "classDiagram"};
        if (firstTok == "classDiagram") return {"class", "class"};
        if (firstTok == "stateDiagram-v2") return {"state", "stateDiagram"};
        if (firstTok == "stateDiagram") return {"state", "state"};
        if (firstTok == "erDiagram") return {"er", "er"};
        if (firstTok == "gantt") return {"gantt", "gantt"};
        if (firstTok == "pie") return {"pie", "pie"};
        if (firstTok == "xychart-beta" || firstTok == "xychart") return {"xychart", "xychart"};
        if (firstTok == "mindmap") return {"mindmap", "mindmap"};
        if (firstTok == "gitGraph") return {"gitGraph", "gitGraph"};
        if (firstLower == "timeline") return {"timeline", "timeline"};
        if (firstLower == "journey") return {"journey", "journey"};
        if (firstTok == "quadrantChart") return {"quadrantChart", "quadrantChart"};
        if (firstTok == "sankey-beta" || firstTok == "sankey") return {"sankey", "sankey"};
        if (firstTok == "C4Context" || firstTok == "C4Container" || firstTok == "C4Component" ||
            firstTok == "C4Dynamic" || firstTok == "C4Deployment") return {"c4", "c4"};
        if (firstTok == "block-beta" || firstTok == "block") return {"block", "block"};
        if (firstTok == "packet-beta" || firstTok == "packet") return {"packet", "packet"};
        if (firstTok == "architecture-beta" || firstTok == "architecture") return {"architecture", "architecture"};
        if (firstTok == "kanban") return {"kanban", "kanban"};
        if (firstTok == "radar-beta" || firstTok == "radar") return {"radar", "radar"};
        if (firstTok == "treemap-beta" || firstTok == "treemap") return {"treemap", "treemap"};
        if (firstLower == "treeview-beta" || firstLower == "treeview") return {"treeView", "treeView"};
        if (firstTok == "venn-beta" || firstTok == "venn") return {"venn", "venn"};
        if (firstTok == "requirementDiagram") return {"requirement", "requirement"};
        if (firstTok == "ishikawa-beta" || firstTok == "ishikawa") return {"ishikawa", "ishikawa"};
        if (firstTok == "wardley-beta" || firstTok == "wardley") return {"wardley", "wardley-beta"};
        if (firstTok == "cynefin-beta" || firstTok == "cynefin") return {"cynefin", "cynefin"};
        if (firstTok == "railroad-ebnf-beta" || firstTok == "railroad-abnf-beta" ||
            firstTok == "railroad-peg-beta" || firstTok == "railroad-beta" || firstTok == "railroad") {
            return {"railroad", "railroad"};
        }
        if (firstTok == "eventmodeling") return {"eventmodeling", "eventmodeling"};
        if (firstLower == "swimlane-beta" || firstLower == "swimlane") return {"swimlane", "swimlane"};
        if (firstLower == "usecase-beta" || firstLower == "usecase" || firstTok == "useCaseDiagram") return {"usecase", "usecase"};
        if (firstLower == "agentflow-beta" || firstLower == "agentflow") return {"agentflow", "agentflow"};
        if (firstLower == "info") return {"info", "info"};
    }
    return {"", ""};
}

// ============================================================================
// Text Metrics & Label Layout (src/text/metrics.ts, measurer.ts, label.ts)
// ============================================================================
enum TextStyle : int {
    StyleNone = 0,
    StyleBold = 1,
    StyleItalic = 2,
    StyleMono = 4,
    StyleStrike = 8,
    StyleSub = 16,
    StyleSup = 32,
    StyleSmall = 64,
    StyleUnderline = 128,
};

inline const std::array<uint16_t, 95>& unpackMetrics(bool bold) {
    static auto unpack = [](const char* packed) {
        std::array<uint16_t, 95> table{};
        for (size_t i = 0; i < 95; i++) {
            auto d = [](char c) -> uint16_t {
                if (c >= '0' && c <= '9') return uint16_t(c - '0');
                if (c >= 'a' && c <= 'z') return uint16_t(c - 'a' + 10);
                return 0;
            };
            table[i] = uint16_t(d(packed[i * 2]) * 36 + d(packed[i * 2 + 1]));
        }
        return table;
    };
    static const auto regularTable = unpack(
        "7q7q9vfgfgopij5b9999atg87q997q7qfgfgfgfgfgfgfgfgfgfg7q7qg8g8g8fgs7ijijk2k2ijgzlmk27qdwijfgn5k2lmijlmk2ijgzk2ijq8ijijgz7q7q7qd1fg99fgfgdwfgfg7qfgfg6666dw66n5fgfgfgfg99dw7qfgdwk2dwdwdw9a789ag8");
    static const auto boldTable = unpack(
        "7q99d6fgfgopk26m9999atg87q997q7qfgfgfgfgfgfgfgfgfgfg9999g8g8g8gzr3k2k2k2k2ijgzlmk27qfgk2gzn5k2lmijlmk2ijgzk2ijq8ijijgz997q99g8fg99fggzfggzfg99gzgz7q7qfg7qopgzgzgzgzatfg99gzfglmfgfgdwat7satg8");
    return bold ? boldTable : regularTable;
}

inline bool isWideCodepoint(uint32_t cp) {
    return (cp >= 0x1100 && cp <= 0x115F) ||
           (cp >= 0x2E80 && cp <= 0xA4CF && cp != 0x303F) ||
           (cp >= 0xAC00 && cp <= 0xD7A3) ||
           (cp >= 0xF900 && cp <= 0xFAFF) ||
           (cp >= 0xFE10 && cp <= 0xFE6F) ||
           (cp >= 0xFF00 && cp <= 0xFF60) ||
           (cp >= 0xFFE0 && cp <= 0xFFE6) ||
           (cp >= 0x1F000 && cp <= 0x1FAFF) ||
           (cp >= 0x20000 && cp <= 0x3FFFD);
}

inline bool isZeroWidthCodepoint(uint32_t cp) {
    return cp < 32 || (cp >= 0x7F && cp < 0xA0) ||
           cp == 0x00AD ||
           (cp >= 0x0300 && cp <= 0x036F) ||
           (cp >= 0x200B && cp <= 0x200F) ||
           (cp >= 0x202A && cp <= 0x202E) ||
           (cp >= 0x2060 && cp <= 0x2064) ||
           (cp >= 0xFE00 && cp <= 0xFE0F) ||
           cp == 0xFEFF;
}

inline double measureTextWidth(const std::string& text, double fontSize, int style = 0) {
    if (text.empty()) return 0.0;
    const auto& table = unpackMetrics((style & StyleBold) != 0);
    bool mono = (style & StyleMono) != 0;
    double sum = 0.0;
    for (size_t i = 0; i < text.size(); ) {
        uint32_t cp = nextUtf8(text, i);
        if (isZeroWidthCodepoint(cp)) continue;
        if (mono) {
            sum += isWideCodepoint(cp) ? 1200.0 : 600.0;
        } else if (cp >= 32 && cp <= 126) {
            sum += table[cp - 32];
        } else if (isWideCodepoint(cp)) {
            sum += 1000.0;
        } else {
            sum += 556.0;
        }
    }
    double factor = ((style & (StyleSub | StyleSup)) != 0) ? 0.75 :
                    ((style & StyleSmall) != 0) ? 0.8 : 1.0;
    return (sum * fontSize * factor) / 1000.0;
}

// ============================================================================
// Zero-Dependency Native C++ LaTeX-to-MathML Typesetter & Box Sizer
// ============================================================================
struct MNode {
    std::string mml;
    double w = 0.0; // width in em
    double h = 1.0; // height in em
    bool isLargeOp = false;
};

inline std::string doubleStruckUtf8(char c) {
    switch (c) {
        case 'C': return "\xE2\x84\x82";
        case 'H': return "\xE2\x84\x8D";
        case 'N': return "\xE2\x84\x95";
        case 'P': return "\xE2\x84\x99";
        case 'Q': return "\xE2\x84\x9A";
        case 'R': return "\xE2\x84\x9D";
        case 'Z': return "\xE2\x84\xA4";
        default: break;
    }
    if (c >= 'A' && c <= 'Z') {
        uint32_t cp = 0x1D538u + uint32_t(c - 'A');
        char out[5] = {
            char(0xF0 | ((cp >> 18) & 0x07)),
            char(0x80 | ((cp >> 12) & 0x3F)),
            char(0x80 | ((cp >> 6) & 0x3F)),
            char(0x80 | (cp & 0x3F)),
            0
        };
        return std::string(out, 4);
    }
    if (c >= 'a' && c <= 'z') {
        uint32_t cp = 0x1D552u + uint32_t(c - 'a');
        char out[5] = {
            char(0xF0 | ((cp >> 18) & 0x07)),
            char(0x80 | ((cp >> 12) & 0x3F)),
            char(0x80 | ((cp >> 6) & 0x3F)),
            char(0x80 | (cp & 0x3F)),
            0
        };
        return std::string(out, 4);
    }
    return std::string(1, c);
}

struct LatexParser {
    std::string s;
    size_t pos = 0;
    bool displayMode = true;
    int recDepth = 0;

    LatexParser(const std::string& src, bool disp, int d = 0) : s(src), pos(0), displayMode(disp), recDepth(d) {}

    void skipSpaces() {
        while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\r' || s[pos] == '\n')) pos++;
    }

    std::string readCommand() {
        if (pos >= s.size() || s[pos] != '\\') return "";
        pos++;
        if (pos >= s.size()) return "";
        if (!std::isalpha(static_cast<unsigned char>(s[pos]))) {
            return std::string(1, s[pos++]);
        }
        size_t st = pos;
        while (pos < s.size() && std::isalpha(static_cast<unsigned char>(s[pos]))) pos++;
        return s.substr(st, pos - st);
    }

    std::string readRawGroup() {
        skipSpaces();
        if (pos < s.size() && s[pos] == '{') {
            pos++;
            int depth = 1;
            size_t st = pos;
            while (pos < s.size() && depth > 0) {
                if (s[pos] == '\\' && pos + 1 < s.size()) { pos += 2; continue; }
                if (s[pos] == '{') depth++;
                else if (s[pos] == '}') {
                    depth--;
                    if (depth == 0) {
                        std::string res = s.substr(st, pos - st);
                        pos++;
                        return res;
                    }
                }
                pos++;
            }
            return s.substr(st);
        }
        if (pos < s.size()) return std::string(1, s[pos++]);
        return "";
    }

    std::string readBracketOpt() {
        skipSpaces();
        if (pos >= s.size() || s[pos] != '[') return "";
        pos++;
        int depth = 1;
        size_t st = pos;
        while (pos < s.size() && depth > 0) {
            if (s[pos] == '\\' && pos + 1 < s.size()) { pos += 2; continue; }
            if (s[pos] == '[') depth++;
            else if (s[pos] == ']') {
                depth--;
                if (depth == 0) {
                    std::string res = s.substr(st, pos - st);
                    pos++;
                    return res;
                }
            }
            pos++;
        }
        return s.substr(st);
    }

    std::string readDelimToken() {
        skipSpaces();
        if (pos >= s.size()) return "";
        if (s[pos] == '\\') {
            std::string cmd = readCommand();
            if (cmd == "{" || cmd == "lbrace") return "{";
            if (cmd == "}" || cmd == "rbrace") return "}";
            if (cmd == "|" || cmd == "Vert" || cmd == "parallel") return "\xE2\x80\x96";
            if (cmd == "vert") return "|";
            if (cmd == "langle") return "\xE2\x9F\xA8";
            if (cmd == "rangle") return "\xE2\x9F\xA9";
            if (cmd == "lfloor") return "\xE2\x8C\x8A";
            if (cmd == "rfloor") return "\xE2\x8C\x8B";
            if (cmd == "lceil") return "\xE2\x8C\x88";
            if (cmd == "rceil") return "\xE2\x8C\x89";
            if (cmd == "backslash") return "\\";
            return cmd;
        }
        char c = s[pos++];
        if (c == '.') return "";
        return std::string(1, c);
    }

    MNode parseEnvironment(const std::string& env) {
        std::string endTag = "\\end{" + env + "}";
        size_t endPos = s.find(endTag, pos);
        std::string body;
        if (endPos != std::string::npos) {
            body = s.substr(pos, endPos - pos);
            pos = endPos + endTag.size();
        } else {
            body = s.substr(pos);
            pos = s.size();
        }
        // Optional column alignment spec for array: {ccc}
        if (env == "array" || env == "matrix*" || env == "pmatrix*" || env == "bmatrix*") {
            size_t b0 = 0;
            while (b0 < body.size() && (body[b0] == ' ' || body[b0] == '\t')) b0++;
            if (b0 < body.size() && body[b0] == '{') {
                size_t b1 = body.find('}', b0 + 1);
                if (b1 != std::string::npos) body = body.substr(b1 + 1);
            }
        }
        // Split body into rows by `\\` (outside nested braces/environments)
        std::vector<std::string> rows;
        {
            int depth = 0;
            size_t st = 0;
            for (size_t i = 0; i < body.size(); i++) {
                if (body[i] == '{') depth++;
                else if (body[i] == '}') depth = std::max(0, depth - 1);
                else if (depth == 0 && body[i] == '\\' && i + 1 < body.size() && body[i + 1] == '\\') {
                    rows.push_back(body.substr(st, i - st));
                    i++;
                    st = i + 1;
                }
            }
            std::string tail = trim(body.substr(st));
            if (!tail.empty() || rows.empty()) rows.push_back(tail);
        }
        std::vector<std::vector<MNode>> grid;
        size_t maxCols = 0;
        for (const auto& rStr : rows) {
            std::vector<MNode> cols;
            int depth = 0;
            size_t st = 0;
            for (size_t i = 0; i < rStr.size(); i++) {
                if (rStr[i] == '\\' && i + 1 < rStr.size()) { i++; continue; }
                if (rStr[i] == '{') depth++;
                else if (rStr[i] == '}') depth = std::max(0, depth - 1);
                else if (depth == 0 && rStr[i] == '&') {
                    LatexParser sub(rStr.substr(st, i - st), false, recDepth + 1);
                    cols.push_back(sub.parseExpr());
                    st = i + 1;
                }
            }
            LatexParser sub(rStr.substr(st), false, recDepth + 1);
            cols.push_back(sub.parseExpr());
            maxCols = std::max(maxCols, cols.size());
            grid.push_back(std::move(cols));
        }
        std::vector<double> colW(maxCols, 0.0);
        double totalH = 0.0;
        std::string tableMml = "<mtable>";
        for (size_t r = 0; r < grid.size(); r++) {
            tableMml += "<mtr>";
            double rowH = 1.0;
            for (size_t c = 0; c < grid[r].size(); c++) {
                const auto& cell = grid[r][c];
                std::string alignAttr = (env == "cases" && c == 0) ? " columnalign=\"left\"" : "";
                tableMml += "<mtd" + alignAttr + ">" + cell.mml + "</mtd>";
                colW[c] = std::max(colW[c], cell.w);
                rowH = std::max(rowH, cell.h);
            }
            tableMml += "</mtr>";
            totalH += rowH + (r + 1 < grid.size() ? 0.35 : 0.0);
        }
        tableMml += "</mtable>";
        double totalW = 0.0;
        for (size_t c = 0; c < maxCols; c++) totalW += colW[c] + (c + 1 < maxCols ? 0.75 : 0.0);

        std::string lDelim, rDelim;
        if (env == "pmatrix" || env == "pmatrix*") { lDelim = "("; rDelim = ")"; }
        else if (env == "bmatrix" || env == "bmatrix*") { lDelim = "["; rDelim = "]"; }
        else if (env == "Bmatrix" || env == "Bmatrix*") { lDelim = "{"; rDelim = "}"; }
        else if (env == "vmatrix" || env == "vmatrix*") { lDelim = "|"; rDelim = "|"; }
        else if (env == "Vmatrix" || env == "Vmatrix*") { lDelim = "\xE2\x80\x96"; rDelim = "\xE2\x80\x96"; }
        else if (env == "cases") { lDelim = "{"; rDelim = ""; }

        MNode out;
        out.h = std::max(1.2, totalH + 0.2);
        out.w = totalW + (!lDelim.empty() ? 0.55 : 0.0) + (!rDelim.empty() ? 0.55 : 0.0);
        if (!lDelim.empty() || !rDelim.empty()) {
            out.mml = "<mrow>";
            if (!lDelim.empty()) out.mml += "<mo stretchy=\"true\">" + esc(lDelim) + "</mo>";
            out.mml += tableMml;
            if (!rDelim.empty()) out.mml += "<mo stretchy=\"true\">" + esc(rDelim) + "</mo>";
            out.mml += "</mrow>";
        } else {
            out.mml = tableMml;
        }
        return out;
    }

    MNode parseAtom() {
        skipSpaces();
        if (pos >= s.size() || recDepth > 64) {
            if (pos < s.size()) pos = s.size();
            return {"", 0.0, 1.0, false};
        }
        struct DepthGuard { int& d; DepthGuard(int& r) : d(r) { ++d; } ~DepthGuard() { --d; } } dg(recDepth);

        char c = s[pos];
        if (c == '{') {
            pos++;
            MNode inner = parseExpr('}');
            if (pos < s.size() && s[pos] == '}') pos++;
            return {"<mrow>" + inner.mml + "</mrow>", inner.w, inner.h, inner.isLargeOp};
        }
        if (c == '\\') {
            std::string cmd = readCommand();
            if (cmd.empty()) return {"", 0.0, 1.0, false};

            // Spacing
            if (cmd == "," || cmd == "thinspace") return {"<mspace width=\"0.17em\"/>", 0.17, 1.0, false};
            if (cmd == ":" || cmd == ">" || cmd == "medspace") return {"<mspace width=\"0.22em\"/>", 0.22, 1.0, false};
            if (cmd == ";" || cmd == "thickspace") return {"<mspace width=\"0.28em\"/>", 0.28, 1.0, false};
            if (cmd == "!") return {"", 0.0, 1.0, false};
            if (cmd == " ") return {"<mspace width=\"0.25em\"/>", 0.25, 1.0, false};
            if (cmd == "quad") return {"<mspace width=\"1em\"/>", 1.0, 1.0, false};
            if (cmd == "qquad") return {"<mspace width=\"2em\"/>", 2.0, 1.0, false};
            if (cmd == "displaystyle" || cmd == "textstyle" || cmd == "scriptstyle" || cmd == "limits" || cmd == "nolimits") {
                return {"", 0.0, 1.0, false};
            }

            // Environments
            if (cmd == "begin") {
                std::string env = trim(readRawGroup());
                return parseEnvironment(env);
            }

            // \left ... \right
            if (cmd == "left") {
                std::string ld = readDelimToken();
                MNode inner = parseExpr(0, true);
                std::string rd;
                skipSpaces();
                if (s.compare(pos, 6, "\\right") == 0) {
                    pos += 6;
                    rd = readDelimToken();
                }
                std::string m = "<mrow>";
                double dw = 0.0;
                if (!ld.empty()) { m += "<mo stretchy=\"true\">" + esc(ld) + "</mo>"; dw += 0.48; }
                m += inner.mml;
                if (!rd.empty()) { m += "<mo stretchy=\"true\">" + esc(rd) + "</mo>"; dw += 0.48; }
                m += "</mrow>";
                return {m, inner.w + dw, std::max(1.15, inner.h), false};
            }
            if (cmd == "big" || cmd == "Big" || cmd == "bigg" || cmd == "Bigg" ||
                cmd == "bigl" || cmd == "bigr" || cmd == "Bigl" || cmd == "Bigr" ||
                cmd == "biggl" || cmd == "biggr" || cmd == "Biggl" || cmd == "Biggr") {
                std::string d = readDelimToken();
                return {"<mo stretchy=\"true\">" + esc(d) + "</mo>", 0.5, 1.3, false};
            }

            // Fractions & Binomials
            if (cmd == "frac" || cmd == "dfrac" || cmd == "tfrac" || cmd == "cfrac") {
                MNode numNode = parseAtom();
                MNode denNode = parseAtom();
                double w = std::max(numNode.w, denNode.w) * 0.94 + 0.45;
                double h = (numNode.h + denNode.h) * 0.85 + 0.50;
                return {"<mfrac><mrow>" + numNode.mml + "</mrow><mrow>" + denNode.mml + "</mrow></mfrac>", w, h, false};
            }
            if (cmd == "binom" || cmd == "dbinom" || cmd == "tbinom") {
                MNode nNode = parseAtom();
                MNode kNode = parseAtom();
                double w = std::max(nNode.w, kNode.w) * 0.94 + 1.05;
                double h = (nNode.h + kNode.h) * 0.85 + 0.45;
                return {"<mrow><mo stretchy=\"true\">(</mo><mfrac linethickness=\"0\"><mrow>" + nNode.mml +
                        "</mrow><mrow>" + kNode.mml + "</mrow></mfrac><mo stretchy=\"true\">)</mo></mrow>", w, h, false};
            }

            // Roots
            if (cmd == "sqrt") {
                std::string opt = readBracketOpt();
                MNode arg = parseAtom();
                if (!opt.empty()) {
                    LatexParser sub(opt, false, recDepth + 1);
                    MNode deg = sub.parseExpr();
                    double w = arg.w + std::max(0.85, deg.w * 0.65 + 0.45);
                    double h = arg.h + 0.32;
                    return {"<mroot><mrow>" + arg.mml + "</mrow><mrow>" + deg.mml + "</mrow></mroot>", w, h, false};
                }
                return {"<msqrt><mrow>" + arg.mml + "</mrow></msqrt>", arg.w + 0.82, arg.h + 0.28, false};
            }

            // Overset / Underset / Stackrel
            if (cmd == "overset" || cmd == "stackrel") {
                MNode topNode = parseAtom();
                MNode baseNode = parseAtom();
                return {"<mover><mrow>" + baseNode.mml + "</mrow><mrow>" + topNode.mml + "</mrow></mover>",
                        std::max(baseNode.w, topNode.w * 0.75) + 0.1, baseNode.h + topNode.h * 0.65, false};
            }
            if (cmd == "underset") {
                MNode botNode = parseAtom();
                MNode baseNode = parseAtom();
                return {"<munder><mrow>" + baseNode.mml + "</mrow><mrow>" + botNode.mml + "</mrow></munder>",
                        std::max(baseNode.w, botNode.w * 0.75) + 0.1, baseNode.h + botNode.h * 0.65, false};
            }

            // Accents & Bars
            if (cmd == "hat" || cmd == "widehat" || cmd == "tilde" || cmd == "widetilde" ||
                cmd == "bar" || cmd == "overline" || cmd == "vec" || cmd == "overrightarrow" ||
                cmd == "overleftarrow" || cmd == "dot" || cmd == "ddot" || cmd == "overbrace") {
                MNode arg = parseAtom();
                std::string acc = "^";
                if (cmd == "tilde" || cmd == "widetilde") acc = "~";
                else if (cmd == "bar" || cmd == "overline") acc = "\xC2\xAF";
                else if (cmd == "vec" || cmd == "overrightarrow") acc = "\xE2\x86\x92";
                else if (cmd == "overleftarrow") acc = "\xE2\x86\x90";
                else if (cmd == "dot") acc = "\xCB\x99";
                else if (cmd == "ddot") acc = "\xC2\xA8";
                else if (cmd == "overbrace") acc = "\xE2\x8F\x9E";
                return {"<mover accent=\"true\"><mrow>" + arg.mml + "</mrow><mo>" + acc + "</mo></mover>",
                        arg.w, arg.h + 0.22, false};
            }
            if (cmd == "underline" || cmd == "underbrace") {
                MNode arg = parseAtom();
                std::string acc = (cmd == "underbrace") ? "\xE2\x8F\x9F" : "_";
                return {"<munder accentunder=\"true\"><mrow>" + arg.mml + "</mrow><mo>" + acc + "</mo></munder>",
                        arg.w, arg.h + 0.22, false};
            }

            // Text & Font Commands
            if (cmd == "text" || cmd == "textrm" || cmd == "textbf" || cmd == "textit" || cmd == "mbox" || cmd == "operatorname") {
                std::string txt = readRawGroup();
                double tw = double(txt.size()) * 0.56;
                if (cmd == "operatorname") {
                    return {"<mi mathvariant=\"normal\">" + esc(txt) + "</mi>", tw + 0.15, 1.0, false};
                }
                return {"<mtext>" + esc(txt) + "</mtext>", tw, 1.0, false};
            }
            if (cmd == "mathbb") {
                std::string txt = readRawGroup();
                std::string outStr;
                for (char ch : txt) outStr += doubleStruckUtf8(ch);
                return {"<mi>" + esc(outStr) + "</mi>", double(txt.size()) * 0.72, 1.0, false};
            }
            if (cmd == "mathrm" || cmd == "mathbf" || cmd == "mathit" || cmd == "mathcal" ||
                cmd == "mathscr" || cmd == "mathfrak" || cmd == "mathsf" || cmd == "mathtt" ||
                cmd == "boldsymbol" || cmd == "bm") {
                std::string mv = "normal";
                if (cmd == "mathbf" || cmd == "boldsymbol" || cmd == "bm") mv = "bold";
                else if (cmd == "mathit") mv = "italic";
                else if (cmd == "mathcal" || cmd == "mathscr") mv = "script";
                else if (cmd == "mathfrak") mv = "fraktur";
                else if (cmd == "mathsf") mv = "sans-serif";
                else if (cmd == "mathtt") mv = "monospace";
                MNode arg = parseAtom();
                return {"<mstyle mathvariant=\"" + mv + "\">" + arg.mml + "</mstyle>", arg.w * 1.05, arg.h, arg.isLargeOp};
            }

            // Escaped literal delimiters
            if (cmd == "{" || cmd == "lbrace") return {"<mo stretchy=\"false\">{</mo>", 0.45, 1.0, false};
            if (cmd == "}" || cmd == "rbrace") return {"<mo stretchy=\"false\">}</mo>", 0.45, 1.0, false};
            if (cmd == "%") return {"<mo>%</mo>", 0.75, 1.0, false};
            if (cmd == "$") return {"<mtext>$</mtext>", 0.55, 1.0, false};
            if (cmd == "&") return {"<mtext>&amp;</mtext>", 0.70, 1.0, false};
            if (cmd == "#") return {"<mtext>#</mtext>", 0.65, 1.0, false};
            if (cmd == "_") return {"<mtext>_</mtext>", 0.55, 1.0, false};

            // Large Operators
            static const std::unordered_map<std::string, std::string> largeOps = {
                {"sum", "\xE2\x88\x91"}, {"prod", "\xE2\x88\x8F"}, {"coprod", "\xE2\x88\x90"},
                {"int", "\xE2\x88\xAB"}, {"iint", "\xE2\x88\xAC"}, {"iiint", "\xE2\x88\xAD"}, {"oint", "\xE2\x88\xAE"},
                {"bigcup", "\xE2\x8B\x83"}, {"bigcap", "\xE2\x8B\x82"}, {"bigoplus", "\xE2\xA8\x81"},
                {"bigotimes", "\xE2\xA8\x82"}, {"bigsqcup", "\xE2\xA8\x86"}, {"bigvee", "\xE2\x8B\x81"}, {"bigwedge", "\xE2\x8B\x80"}
            };
            auto loIt = largeOps.find(cmd);
            if (loIt != largeOps.end()) {
                return {"<mo>" + loIt->second + "</mo>", 1.20, 1.55, true};
            }

            // Standard Math Functions
            static const std::unordered_set<std::string> mathFuncs = {
                "sin", "cos", "tan", "cot", "sec", "csc", "arcsin", "arccos", "arctan",
                "sinh", "cosh", "tanh", "coth", "log", "ln", "lg", "exp",
                "lim", "limsup", "liminf", "max", "min", "sup", "inf",
                "arg", "deg", "det", "dim", "gcd", "hom", "ker", "Pr", "mod", "bmod"
            };
            if (mathFuncs.count(cmd)) {
                bool under = (cmd == "lim" || cmd == "limsup" || cmd == "liminf" ||
                              cmd == "max" || cmd == "min" || cmd == "sup" || cmd == "inf" ||
                              cmd == "det" || cmd == "Pr" || cmd == "gcd");
                return {"<mi mathvariant=\"normal\">" + cmd + "</mi>", double(cmd.size()) * 0.56 + 0.18, 1.0, under};
            }

            // Lowercase Greek & Symbols (<mi>)
            static const std::unordered_map<std::string, std::string> greekLower = {
                {"alpha", "\xCE\xB1"}, {"beta", "\xCE\xB2"}, {"gamma", "\xCE\xB3"}, {"delta", "\xCE\xB4"},
                {"epsilon", "\xCF\xB5"}, {"varepsilon", "\xCE\xB5"}, {"zeta", "\xCE\xB6"}, {"eta", "\xCE\xB7"},
                {"theta", "\xCE\xB8"}, {"vartheta", "\xCF\x91"}, {"iota", "\xCE\xB9"}, {"kappa", "\xCE\xBA"},
                {"lambda", "\xCE\xBB"}, {"mu", "\xCE\xBC"}, {"nu", "\xCE\xBD"}, {"xi", "\xCE\xBE"},
                {"pi", "\xCF\x80"}, {"varpi", "\xCF\x96"}, {"rho", "\xCF\x81"}, {"varrho", "\xCF\xB1"},
                {"sigma", "\xCF\x83"}, {"varsigma", "\xCF\x82"}, {"tau", "\xCF\x84"}, {"upsilon", "\xCF\x85"},
                {"phi", "\xCF\x95"}, {"varphi", "\xCF\x86"}, {"chi", "\xCF\x87"}, {"psi", "\xCF\x88"},
                {"omega", "\xCF\x89"}, {"infty", "\xE2\x88\x9E"}, {"partial", "\xE2\x88\x82"}, {"nabla", "\xE2\x88\x87"},
                {"hbar", "\xE2\x84\x8F"}, {"ell", "\xE2\x84\x93"}, {"Re", "\xE2\x84\x9C"}, {"Im", "\xE2\x84\x91"},
                {"wp", "\xE2\x84\x98"}, {"aleph", "\xE2\x84\xB5"}, {"emptyset", "\xE2\x88\x85"}, {"varnothing", "\xE2\x88\x85"},
                {"forall", "\xE2\x88\x80"}, {"exists", "\xE2\x88\x83"}, {"nexists", "\xE2\x88\x84"},
                {"neg", "\xC2\xAC"}, {"lnot", "\xC2\xAC"}, {"top", "\xE2\x8A\xA4"}, {"bot", "\xE2\x8A\xA5"},
                {"triangle", "\xE2\x96\xB3"}, {"angle", "\xE2\x88\xA0"}, {"prime", "\xE2\x80\xB2"},
                {"cdots", "\xE2\x8B\xAF"}, {"ldots", "\xE2\x80\xA6"}, {"dots", "\xE2\x80\xA6"},
                {"vdots", "\xE2\x8B\xAE"}, {"ddots", "\xE2\x8B\xB1"}
            };
            auto glIt = greekLower.find(cmd);
            if (glIt != greekLower.end()) {
                double w = (cmd == "cdots" || cmd == "ldots" || cmd == "dots") ? 1.15 : 0.68;
                return {"<mi>" + glIt->second + "</mi>", w, 1.0, false};
            }

            // Uppercase Greek (<mi mathvariant="normal">)
            static const std::unordered_map<std::string, std::string> greekUpper = {
                {"Gamma", "\xCE\x93"}, {"Delta", "\xCE\x94"}, {"Theta", "\xCE\x98"}, {"Lambda", "\xCE\x9B"},
                {"Xi", "\xCE\x9E"}, {"Pi", "\xCE\xA0"}, {"Sigma", "\xCE\xA3"}, {"Upsilon", "\xCE\xA5"},
                {"Phi", "\xCE\xA6"}, {"Psi", "\xCE\xA8"}, {"Omega", "\xCE\xA9"}
            };
            auto guIt = greekUpper.find(cmd);
            if (guIt != greekUpper.end()) {
                return {"<mi mathvariant=\"normal\">" + guIt->second + "</mi>", 0.78, 1.0, false};
            }

            // Operators & Relations (<mo>)
            static const std::unordered_map<std::string, std::string> mathOps = {
                {"pm", "\xC2\xB1"}, {"mp", "\xE2\x88\x93"}, {"times", "\xC3\x97"}, {"div", "\xC3\xB7"},
                {"cdot", "\xE2\x8B\x85"}, {"ast", "\xE2\x88\x97"}, {"star", "\xE2\x8B\x86"}, {"circ", "\xE2\x88\x98"},
                {"bullet", "\xE2\x88\x99"}, {"oplus", "\xE2\x8A\x95"}, {"ominus", "\xE2\x8A\x96"},
                {"otimes", "\xE2\x8A\x97"}, {"oslash", "\xE2\x8A\x98"}, {"odot", "\xE2\x8A\x99"},
                {"cap", "\xE2\x88\xA9"}, {"cup", "\xE2\x88\xAA"}, {"setminus", "\xE2\x88\x96"},
                {"wedge", "\xE2\x88\xA7"}, {"land", "\xE2\x88\xA7"}, {"vee", "\xE2\x88\xA8"}, {"lor", "\xE2\x88\xA8"},
                {"neq", "\xE2\x89\xA0"}, {"ne", "\xE2\x89\xA0"}, {"leq", "\xE2\x89\xA4"}, {"le", "\xE2\x89\xA4"},
                {"geq", "\xE2\x89\xA5"}, {"ge", "\xE2\x89\xA5"}, {"ll", "\xE2\x89\xAA"}, {"gg", "\xE2\x89\xAB"},
                {"approx", "\xE2\x89\x88"}, {"equiv", "\xE2\x89\xA1"}, {"sim", "\xE2\x88\xBC"}, {"simeq", "\xE2\x89\x83"},
                {"cong", "\xE2\x89\x85"}, {"propto", "\xE2\x88\x9D"}, {"doteq", "\xE2\x89\x90"},
                {"in", "\xE2\x88\x88"}, {"notin", "\xE2\x88\x89"}, {"ni", "\xE2\x88\x8B"},
                {"subset", "\xE2\x8A\x82"}, {"supset", "\xE2\x8A\x83"}, {"subseteq", "\xE2\x8A\x86"}, {"supseteq", "\xE2\x8A\x87"},
                {"vdash", "\xE2\x8A\xA2"}, {"dashv", "\xE2\x8A\xA3"}, {"models", "\xE2\x8A\xA8"},
                {"perp", "\xE2\x8A\xA5"}, {"parallel", "\xE2\x88\xA5"}, {"mid", "\xE2\x88\xA3"}, {"nmid", "\xE2\x88\xA4"},
                {"to", "\xE2\x86\x92"}, {"rightarrow", "\xE2\x86\x92"}, {"leftarrow", "\xE2\x86\x90"}, {"gets", "\xE2\x86\x90"},
                {"leftrightarrow", "\xE2\x86\x94"}, {"Rightarrow", "\xE2\x87\x92"}, {"Leftarrow", "\xE2\x87\x90"},
                {"Leftrightarrow", "\xE2\x87\x94"}, {"iff", "\xE2\x9F\xBA"}, {"implies", "\xE2\x9F\xB9"},
                {"impliedby", "\xE2\x9F\xB8"}, {"mapsto", "\xE2\x86\xA6"}, {"longmapsto", "\xE2\x9F\xBC"},
                {"longrightarrow", "\xE2\x9F\xB6"}, {"longleftarrow", "\xE2\x9F\xB5"},
                {"uparrow", "\xE2\x86\x91"}, {"downarrow", "\xE2\x86\x93"}, {"updownarrow", "\xE2\x86\x95"},
                {"Uparrow", "\xE2\x87\x91"}, {"Downarrow", "\xE2\x87\x93"},
                {"nearrow", "\xE2\x86\x97"}, {"searrow", "\xE2\x86\x98"}, {"swarrow", "\xE2\x86\x99"}, {"nwarrow", "\xE2\x86\x96"},
                {"langle", "\xE2\x9F\xA8"}, {"rangle", "\xE2\x9F\xA9"},
                {"lfloor", "\xE2\x8C\x8A"}, {"rfloor", "\xE2\x8C\x8B"},
                {"lceil", "\xE2\x8C\x88"}, {"rceil", "\xE2\x8C\x89"},
                {"|", "\xE2\x80\x96"}, {"Vert", "\xE2\x80\x96"}, {"vert", "|"}, {"backslash", "\\"}
            };
            auto opIt = mathOps.find(cmd);
            if (opIt != mathOps.end()) {
                return {"<mo>" + opIt->second + "</mo>", 1.05, 1.0, false};
            }

            // Fallback unknown command -> identifier
            return {"<mi>" + esc(cmd) + "</mi>", double(cmd.size()) * 0.58, 1.0, false};
        }

        // Numbers
        if (std::isdigit(static_cast<unsigned char>(c))) {
            size_t st = pos;
            while (pos < s.size() && (std::isdigit(static_cast<unsigned char>(s[pos])) ||
                   (s[pos] == '.' && pos + 1 < s.size() && std::isdigit(static_cast<unsigned char>(s[pos + 1]))))) {
                pos++;
            }
            std::string numStr = s.substr(st, pos - st);
            return {"<mn>" + numStr + "</mn>", double(numStr.size()) * 0.60, 1.0, false};
        }

        // ASCII letters
        if (std::isalpha(static_cast<unsigned char>(c))) {
            pos++;
            double w = std::isupper(static_cast<unsigned char>(c)) ? 0.74 : 0.60;
            return {"<mi>" + std::string(1, c) + "</mi>", w, 1.0, false};
        }

        // Multi-byte UTF-8 character (e.g. Japanese/Greek/Unicode directly in math)
        if (static_cast<unsigned char>(c) >= 0x80) {
            size_t st = pos;
            uint32_t cp = nextUtf8(s, pos);
            std::string u = s.substr(st, pos - st);
            double w = isWideCodepoint(cp) ? 1.05 : 0.68;
            return {"<mi>" + esc(u) + "</mi>", w, 1.0, false};
        }

        // Operators and punctuation
        pos++;
        if (c == '+' || c == '-' || c == '=' || c == '<' || c == '>' || c == '*' || c == '/') {
            return {"<mo>" + esc(std::string(1, c)) + "</mo>", 1.02, 1.0, false};
        }
        if (c == '(' || c == ')' || c == '[' || c == ']' || c == '|') {
            return {"<mo stretchy=\"false\">" + esc(std::string(1, c)) + "</mo>", 0.42, 1.0, false};
        }
        if (c == ',' || c == ';' || c == ':' || c == '!' || c == '?') {
            return {"<mo>" + esc(std::string(1, c)) + "</mo>", 0.40, 1.0, false};
        }
        return {"<mo>" + esc(std::string(1, c)) + "</mo>", 0.55, 1.0, false};
    }

    MNode parseFactor() {
        MNode base = parseAtom();
        if (base.mml.empty()) return base;

        bool hasSub = false, hasSup = false;
        MNode subNode, supNode;

        while (true) {
            skipSpaces();
            if (pos >= s.size()) break;
            if (s[pos] == '_' && !hasSub) {
                pos++;
                subNode = parseAtom();
                hasSub = true;
            } else if (s[pos] == '^' && !hasSup) {
                pos++;
                supNode = parseAtom();
                hasSup = true;
            } else if (s[pos] == '\'' && !hasSup) {
                std::string primes;
                int pCount = 0;
                while (pos < s.size() && s[pos] == '\'') {
                    primes += "\xE2\x80\xB2";
                    pCount++;
                    pos++;
                }
                supNode = {"<mo>" + primes + "</mo>", double(pCount) * 0.35, 0.8, false};
                hasSup = true;
            } else {
                break;
            }
        }

        if (!hasSub && !hasSup) return base;

        if (base.isLargeOp && displayMode) {
            double w = std::max({base.w, hasSub ? subNode.w * 0.78 : 0.0, hasSup ? supNode.w * 0.78 : 0.0}) + 0.20;
            double h = base.h + (hasSub ? subNode.h * 0.65 : 0.0) + (hasSup ? supNode.h * 0.65 : 0.0) + 0.15;
            if (hasSub && hasSup) {
                return {"<munderover><mrow>" + base.mml + "</mrow><mrow>" + subNode.mml + "</mrow><mrow>" + supNode.mml + "</mrow></munderover>", w, h, false};
            }
            if (hasSub) {
                return {"<munder><mrow>" + base.mml + "</mrow><mrow>" + subNode.mml + "</mrow></munder>", w, h, false};
            }
            return {"<mover><mrow>" + base.mml + "</mrow><mrow>" + supNode.mml + "</mrow></mover>", w, h, false};
        }

        double scriptW = std::max(hasSub ? subNode.w : 0.0, hasSup ? supNode.w : 0.0) * 0.75 + 0.10;
        double w = base.w + scriptW;
        double h = base.h + (hasSup ? std::max(0.28, (supNode.h - 0.5) * 0.65) : 0.0) +
                            (hasSub ? std::max(0.20, (subNode.h - 0.5) * 0.55) : 0.0);
        if (hasSub && hasSup) {
            return {"<msubsup><mrow>" + base.mml + "</mrow><mrow>" + subNode.mml + "</mrow><mrow>" + supNode.mml + "</mrow></msubsup>", w, h, false};
        }
        if (hasSub) {
            return {"<msub><mrow>" + base.mml + "</mrow><mrow>" + subNode.mml + "</mrow></msub>", w, h, false};
        }
        return {"<msup><mrow>" + base.mml + "</mrow><mrow>" + supNode.mml + "</mrow></msup>", w, h, false};
    }

    MNode parseExpr(char stopChar = 0, bool stopOnRight = false) {
        MNode out;
        out.h = 1.0;
        while (true) {
            skipSpaces();
            if (pos >= s.size()) break;
            if (stopChar != 0 && s[pos] == stopChar) break;
            if (stopOnRight && s.compare(pos, 6, "\\right") == 0 &&
                (pos + 6 >= s.size() || !std::isalpha(static_cast<unsigned char>(s[pos + 6])))) {
                break;
            }
            size_t before = pos;
            MNode f = parseFactor();
            if (pos == before) { pos++; continue; }
            out.mml += f.mml;
            out.w += f.w;
            out.h = std::max(out.h, f.h);
        }
        return out;
    }
};

inline MathBox typesetLatexMath(const std::string& tex, double fontSize = 16.0, bool displayMode = true) {
    MathBox box;
    std::string clean = trim(tex);
    if (clean.empty()) return box;
    LatexParser parser(clean, displayMode);
    MNode root = parser.parseExpr();
    std::string dispAttr = displayMode ? " display=\"block\"" : "";
    box.html = "<math xmlns=\"http://www.w3.org/1998/Math/MathML\"" + dispAttr + "><mrow>" + root.mml + "</mrow></math>";
    box.width = std::ceil(std::max(fontSize * 1.1, root.w * fontSize + 14.0));
    box.height = std::ceil(std::max(fontSize * 1.35, root.h * fontSize + 6.0));
    return box;
}

inline MathResolver& activeMathResolver() {
    static MathResolver r = nullptr;
    return r;
}

struct Span {
    std::string text;
    int style = 0;
    double width = 0.0;
    double mathHeight = 0.0;
    std::string mathSvg;
    std::string mathHtml;
};

struct Label {
    std::vector<std::vector<Span>> lines;
    std::vector<double> widths;
    std::vector<double> heights;
    double width = 0.0;
    double height = 0.0;
    double size = 16.0;
    double lineHeight = 24.0;
};

inline bool looksLikeInlineMath(const std::string& inner) {
    if (inner.empty() || inner.front() == ' ' || inner.back() == ' ') return false;
    bool allDigits = true;
    for (char c : inner) {
        if (!std::isdigit(static_cast<unsigned char>(c)) && c != '.' && c != ',') {
            allDigits = false;
            break;
        }
    }
    if (allDigits) return false;
    for (char c : inner) {
        if (c == '\\' || c == '^' || c == '_' || c == '{' || c == '}' || c == '=') return true;
    }
    if (inner.size() <= 16 && std::isalpha(static_cast<unsigned char>(inner.front()))) return true;
    return false;
}

inline std::vector<Span> parseInlineSpans(const std::string& rawLine, bool markdown, double fontSize, int baseStyle) {
    std::vector<Span> out;
    std::string cur;
    int curStyle = baseStyle;
    auto flush = [&]() {
        if (!cur.empty()) {
            std::string dec = decodeEntities(cur);
            out.push_back({dec, curStyle, measureTextWidth(dec, fontSize, curStyle), 0.0, "", ""});
            cur.clear();
        }
    };
    auto pushMathSpan = [&](const std::string& tex, bool displayMode) {
        std::string cleanTex = trim(decodeEntities(tex));
        if (cleanTex.empty()) return false;
        MathBox box;
        auto& customMath = activeMathResolver();
        if (customMath) {
            box = customMath(cleanTex, fontSize);
        } else {
            box = typesetLatexMath(cleanTex, fontSize, displayMode);
        }
        if (box.width > 0.0 && (!box.html.empty() || !box.svg.empty())) {
            flush();
            Span sp;
            sp.text = cleanTex;
            sp.style = curStyle;
            sp.width = std::ceil(box.width);
            sp.mathHeight = std::ceil(box.height);
            sp.mathSvg = box.svg;
            sp.mathHtml = box.html;
            out.push_back(std::move(sp));
            return true;
        }
        return false;
    };
    for (size_t i = 0; i < rawLine.size(); ) {
        // 1. Display / standard Pele math: $$...$$
        if (!(curStyle & StyleMono) && i + 3 < rawLine.size() && rawLine[i] == '$' && rawLine[i + 1] == '$') {
            size_t closePos = rawLine.find("$$", i + 2);
            if (closePos != std::string::npos && closePos > i + 2) {
                std::string tex = rawLine.substr(i + 2, closePos - (i + 2));
                if (pushMathSpan(tex, true)) {
                    i = closePos + 2;
                    continue;
                }
            }
        }
        // 2. Inline single-$ math: $...$ (when content is a formula, not currency like $100)
        if (!(curStyle & StyleMono) && rawLine[i] == '$' && (i == 0 || rawLine[i - 1] != '\\') &&
            (i + 1 < rawLine.size() && rawLine[i + 1] != '$')) {
            size_t closePos = i + 1;
            while (closePos < rawLine.size()) {
                if (rawLine[closePos] == '$' && rawLine[closePos - 1] != '\\') break;
                closePos++;
            }
            if (closePos < rawLine.size() && closePos > i + 1) {
                std::string tex = rawLine.substr(i + 1, closePos - (i + 1));
                if (looksLikeInlineMath(tex) && pushMathSpan(tex, true)) {
                    i = closePos + 1;
                    continue;
                }
            }
        }
        // HTML inline tags: <b>, </b>, <strong>, </strong>, <i>, </i>, <em>, </em>, <code>, </code>, <u>, </u>, <s>, </s>, <sub>, </sub>, <sup>, </sup>, <small>, </small>
        if (rawLine[i] == '<') {
            size_t gt = rawLine.find('>', i + 1);
            if (gt != std::string::npos && gt - i <= 12) {
                std::string tag = toLower(trim(rawLine.substr(i + 1, gt - i - 1)));
                bool close = (!tag.empty() && tag[0] == '/');
                std::string name = close ? trim(tag.substr(1)) : tag;
                int bit = 0;
                if (name == "b" || name == "strong") bit = StyleBold;
                else if (name == "i" || name == "em") bit = StyleItalic;
                else if (name == "code") bit = StyleMono;
                else if (name == "u" || name == "ins") bit = StyleUnderline;
                else if (name == "s" || name == "del") bit = StyleStrike;
                else if (name == "sub") bit = StyleSub;
                else if (name == "sup") bit = StyleSup;
                else if (name == "small") bit = StyleSmall;
                if (bit != 0) {
                    flush();
                    if (close) curStyle &= ~bit;
                    else curStyle |= bit;
                    i = gt + 1;
                    continue;
                }
            }
        }
        if (markdown) {
            if (i + 1 < rawLine.size() && rawLine[i] == '*' && rawLine[i + 1] == '*') {
                flush();
                curStyle ^= StyleBold;
                i += 2;
                continue;
            }
            if (i + 1 < rawLine.size() && rawLine[i] == '~' && rawLine[i + 1] == '~') {
                flush();
                curStyle ^= StyleStrike;
                i += 2;
                continue;
            }
            if (rawLine[i] == '*') {
                flush();
                curStyle ^= StyleItalic;
                i += 1;
                continue;
            }
            if (rawLine[i] == '`') {
                flush();
                curStyle ^= StyleMono;
                i += 1;
                continue;
            }
        }
        cur += rawLine[i++];
    }
    flush();
    return out;
}

inline Label layoutLabel(const std::string& raw, bool markdown = false, double fontSize = 16.0, double maxWidth = 4000.0, int baseStyle = 0) {
    Label lbl;
    lbl.size = fontSize;
    lbl.lineHeight = std::round(fontSize * 1.5);
    if (raw.empty()) return lbl;

    // Normalize <br>, <br/>, <br /> and literal \n into '\n' (preserving LaTeX commands like \neq, \nabla, \nu, and \\ inside $$...$$)
    std::string norm;
    norm.reserve(raw.size());
    bool inDoubleDollar = false;
    for (size_t i = 0; i < raw.size(); ) {
        if (i + 1 < raw.size() && raw[i] == '$' && raw[i + 1] == '$') {
            inDoubleDollar = !inDoubleDollar;
            norm += "$$";
            i += 2;
            continue;
        }
        if (raw[i] == '<') {
            size_t gt = raw.find('>', i + 1);
            if (gt != std::string::npos && gt - i <= 10) {
                std::string tag = toLower(trim(raw.substr(i + 1, gt - i - 1)));
                if (tag == "br" || tag == "br/" || tag == "br /") {
                    norm += '\n';
                    i = gt + 1;
                    continue;
                }
            }
        }
        if (!inDoubleDollar && raw[i] == '\\' && i + 1 < raw.size() && raw[i + 1] == 'n' &&
            (i + 2 >= raw.size() || !std::isalpha(static_cast<unsigned char>(raw[i + 2])))) {
            norm += '\n';
            i += 2;
            continue;
        }
        norm += raw[i++];
    }

    auto rawLines = splitLines(norm);
    for (const auto& rl : rawLines) {
        std::string t = trim(rl);
        if (t.empty() && rawLines.size() == 1) return lbl;
        auto spans = parseInlineSpans(t, markdown, fontSize, baseStyle);
        double totalW = 0.0;
        for (const auto& sp : spans) totalW += sp.width;

        if (maxWidth <= 0.0 || totalW <= maxWidth || spans.empty()) {
            lbl.lines.push_back(spans);
            lbl.widths.push_back(totalW);
            lbl.width = std::max(lbl.width, totalW);
        } else {
            // Word wrap across spans (treating math spans as atomic boxes)
            std::vector<Span> curLine;
            double curW = 0.0;
            for (const auto& sp : spans) {
                if (!sp.mathHtml.empty() || !sp.mathSvg.empty()) {
                    if (curW + sp.width > maxWidth && !curLine.empty()) {
                        auto& lastSp = curLine.back();
                        if (lastSp.mathHtml.empty() && lastSp.mathSvg.empty() && !lastSp.text.empty() && lastSp.text.back() == ' ') {
                            while (!lastSp.text.empty() && lastSp.text.back() == ' ') lastSp.text.pop_back();
                            double nw = measureTextWidth(lastSp.text, fontSize, lastSp.style);
                            curW += (nw - lastSp.width);
                            lastSp.width = nw;
                        }
                        lbl.lines.push_back(curLine);
                        lbl.widths.push_back(curW);
                        lbl.width = std::max(lbl.width, curW);
                        curLine.clear();
                        curW = 0.0;
                    }
                    curLine.push_back(sp);
                    curW += sp.width;
                    continue;
                }
                auto words = splitChar(sp.text, ' ');
                std::string accum;
                for (size_t wi = 0; wi < words.size(); wi++) {
                    std::string candidate = accum.empty() ? words[wi] : (accum + " " + words[wi]);
                    double cw = measureTextWidth(candidate, fontSize, sp.style);
                    if (curW + cw > maxWidth && (!curLine.empty() || !accum.empty())) {
                        if (!accum.empty()) {
                            double aw = measureTextWidth(accum, fontSize, sp.style);
                            curLine.push_back({accum, sp.style, aw, 0.0, "", ""});
                            curW += aw;
                            accum.clear();
                        }
                        lbl.lines.push_back(curLine);
                        lbl.widths.push_back(curW);
                        lbl.width = std::max(lbl.width, curW);
                        curLine.clear();
                        curW = 0.0;
                        accum = words[wi];
                    } else {
                        accum = candidate;
                    }
                }
                if (!accum.empty()) {
                    double aw = measureTextWidth(accum, fontSize, sp.style);
                    curLine.push_back({accum, sp.style, aw, 0.0, "", ""});
                    curW += aw;
                }
            }
            if (!curLine.empty()) {
                lbl.lines.push_back(curLine);
                lbl.widths.push_back(curW);
                lbl.width = std::max(lbl.width, curW);
            }
        }
    }
    lbl.height = 0.0;
    lbl.heights.reserve(lbl.lines.size());
    for (const auto& line : lbl.lines) {
        double lh = lbl.lineHeight;
        for (const auto& sp : line) {
            if (sp.mathHeight > 0.0) lh = std::max(lh, std::ceil(sp.mathHeight + lbl.size / 4.0));
        }
        lbl.heights.push_back(lh);
        lbl.height += lh;
    }
    return lbl;
}

// ============================================================================
// SVG Theme, Root Document & Builder (src/svg/theme.ts, root.ts, builder.ts)
// ============================================================================
static const char* const ROUNDED = " style=\"rx:var(--_r)\"";
static const char* const FONT_MONO = "var(--_fm)";
static const char* const CRITICAL = "var(--pele-critical,#e45756)";

inline std::string seriesColor(int index) {
    static const char* const SERIES[8] = {
        "var(--pele-series-1,#4c78a8)",
        "var(--pele-series-2,#f58518)",
        "var(--pele-series-3,#54a24b)",
        "var(--pele-series-4,#e45756)",
        "var(--pele-series-5,#72b7b2)",
        "var(--pele-series-6,#eeca3b)",
        "var(--pele-series-7,#b279a2)",
        "var(--pele-series-8,#9d755d)",
    };
    int k = ((index % 8) + 8) % 8;
    return SERIES[k];
}

using TextBlock = Label;

inline double measureTextWidth(const std::string& text, double fontSize, bool bold, bool mono) {
    int st = (bold ? StyleBold : 0) | (mono ? StyleMono : 0);
    return measureTextWidth(text, fontSize, st);
}

inline Label layoutLabel(const std::string& raw, double fontSize, bool bold, double maxWidth = 4000.0) {
    return layoutLabel(raw, true, fontSize, maxWidth, bold ? StyleBold : 0);
}

struct ResolvedStyle {
    std::string shape;
    std::string line;
    std::string text;
    std::string fill;
    std::string stroke;
    std::string color;
    std::string textColor = "var(--_fg)";
    std::string extra;
    bool bold = false;
    bool italic = false;
};

inline std::string sanitizeCssValue(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    for (unsigned char c : raw) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == '#' || c == '(' || c == ')' || c == ',' || c == '.' || c == '%' ||
            c == '-' || c == '_' || c == ' ') {
            out.push_back(static_cast<char>(c));
        }
    }
    std::string low = toLower(out);
    if (low.find("url(") != std::string::npos || low.find("expression") != std::string::npos ||
        low.find("javascript") != std::string::npos || low.find("import") != std::string::npos) {
        return "";
    }
    return trim(out);
}

inline ResolvedStyle resolveStyle(const std::vector<std::string>& decls) {
    ResolvedStyle rs;
    std::string shapeCss;
    std::string lineCss;
    std::string textCss;
    for (const auto& rawDecl : decls) {
        for (const auto& part : splitChar(rawDecl, ',')) {
            for (const auto& sub : splitChar(part, ';')) {
                size_t colon = sub.find(':');
                if (colon == std::string::npos) continue;
                std::string k = toLower(trim(sub.substr(0, colon)));
                std::string v = sanitizeCssValue(trim(sub.substr(colon + 1)));
                if (k.empty() || v.empty()) continue;
                if (k == "fill") { rs.fill = esc(v); shapeCss += "fill:" + esc(v) + ";"; }
                else if (k == "stroke") { rs.stroke = esc(v); shapeCss += "stroke:" + esc(v) + ";"; lineCss += "stroke:" + esc(v) + ";"; }
                else if (k == "stroke-width") { shapeCss += "stroke-width:" + esc(v) + ";"; lineCss += "stroke-width:" + esc(v) + ";"; }
                else if (k == "stroke-dasharray") { shapeCss += "stroke-dasharray:" + esc(v) + ";"; lineCss += "stroke-dasharray:" + esc(v) + ";"; }
                else if (k == "color") { rs.color = esc(v); rs.textColor = esc(v); textCss += "fill:" + esc(v) + ";color:" + esc(v) + ";"; }
                else if (k == "font-weight" && (v == "bold" || v == "700" || v == "600")) { rs.bold = true; }
                else if (k == "font-style" && v == "italic") { rs.italic = true; }
            }
        }
    }
    if (!shapeCss.empty()) rs.shape = " style=\"" + shapeCss + "\"";
    if (!lineCss.empty()) rs.line = " style=\"" + lineCss + "\"";
    if (!textCss.empty()) rs.text = " style=\"" + textCss + "\"";
    return rs;
}

inline ResolvedStyle resolveStyle(const std::map<std::string, std::string>& styleMap) {
    std::vector<std::string> decls;
    for (const auto& kv : styleMap) decls.push_back(kv.first + ":" + kv.second);
    return resolveStyle(decls);
}

inline std::string rounded(const std::string& shapeAttr = "") {
    if (shapeAttr.empty()) return ROUNDED;
    if (shapeAttr.find("style=\"") != std::string::npos) {
        std::string out = shapeAttr;
        size_t pos = out.find("style=\"");
        out.insert(pos + 7, "rx:var(--_r);");
        return out;
    }
    return shapeAttr + ROUNDED;
}

inline std::string spanStyleAttr(int style) {
    std::string s;
    if (style & StyleBold) s += " font-weight=\"var(--_w)\"";
    if (style & StyleItalic) s += " font-style=\"italic\"";
    if (style & StyleMono) s += " font-family=\"var(--_fm)\"";
    if ((style & StyleStrike) && (style & StyleUnderline)) s += " text-decoration=\"underline line-through\"";
    else if (style & StyleStrike) s += " text-decoration=\"line-through\"";
    else if (style & StyleUnderline) s += " text-decoration=\"underline\"";
    if (style & StyleSub) s += " font-size=\"75%\" baseline-shift=\"sub\"";
    else if (style & StyleSup) s += " font-size=\"75%\" baseline-shift=\"super\"";
    else if (style & StyleSmall) s += " font-size=\"80%\"";
    return s;
}

inline std::string labelSvg(const Label& label, double cx, double cy, const std::string& attrs = "") {
    if (label.lines.empty() || label.height <= 0.0) return "";
    double shift = label.size * 0.35;
    bool hasMath = false;
    for (const auto& line : label.lines) {
        for (const auto& sp : line) {
            if (!sp.mathHtml.empty() || !sp.mathSvg.empty()) {
                hasMath = true;
                break;
            }
        }
        if (hasMath) break;
    }
    if (!hasMath) {
        if (label.lines.size() == 1 && label.lines[0].size() == 1) {
            const auto& sp = label.lines[0][0];
            return "<text" + attrs + spanStyleAttr(sp.style) +
                   " x=\"" + num(cx) + "\" y=\"" + num(cy + shift) + "\" text-anchor=\"middle\">" +
                   esc(sp.text) + "</text>";
        }
        double top = cy - label.height / 2.0;
        std::string body;
        for (size_t i = 0; i < label.lines.size(); i++) {
            const auto& line = label.lines[i];
            if (line.empty()) continue;
            double y = top + (double(i) + 0.5) * label.lineHeight + shift;
            if (line.size() == 1) {
                body += "<tspan x=\"" + num(cx) + "\" y=\"" + num(y) + "\"" + spanStyleAttr(line[0].style) + ">" +
                        esc(line[0].text) + "</tspan>";
            } else {
                bool first = true;
                for (const auto& sp : line) {
                    if (sp.text.empty()) continue;
                    body += "<tspan" + (first ? (" x=\"" + num(cx) + "\" y=\"" + num(y) + "\"") : std::string("")) +
                            spanStyleAttr(sp.style) + ">" + esc(sp.text) + "</tspan>";
                    first = false;
                }
            }
        }
        if (body.empty()) return "";
        return "<text" + attrs + " text-anchor=\"middle\">" + body + "</text>";
    }

    // Mixed or pure math rendering (matches src/svg/builder.ts)
    std::string mathAttrs;
    {
        std::string fillVal;
        size_t fPos = attrs.find(" fill=\"");
        if (fPos != std::string::npos) {
            size_t fEnd = attrs.find('"', fPos + 7);
            if (fEnd != std::string::npos) fillVal = trim(attrs.substr(fPos + 7, fEnd - (fPos + 7)));
        }
        size_t sPos = attrs.find("fill:");
        if (sPos != std::string::npos) {
            size_t sEnd = attrs.find_first_of(";\"", sPos + 5);
            if (sEnd != std::string::npos) fillVal = trim(attrs.substr(sPos + 5, sEnd - (sPos + 5)));
        }
        if (!fillVal.empty() && fillVal != "none") {
            mathAttrs += " color=\"" + fillVal + "\"";
        }
        size_t oPos = attrs.find("opacity:");
        if (oPos != std::string::npos) {
            size_t oEnd = attrs.find_first_of(";\"", oPos + 8);
            if (oEnd != std::string::npos) mathAttrs += " opacity=\"" + trim(attrs.substr(oPos + 8, oEnd - (oPos + 8))) + "\"";
        }
    }
    double top = cy - label.height / 2.0;
    double lineY = top;
    std::string textOut;
    std::string mathOut;
    for (size_t i = 0; i < label.lines.size(); i++) {
        const auto& line = label.lines[i];
        double lh = (i < label.heights.size()) ? label.heights[i] : label.lineHeight;
        double midY = lineY + lh / 2.0;
        double y = midY + shift;
        lineY += lh;
        if (line.empty()) continue;

        bool lineHasMath = false;
        for (const auto& sp : line) {
            if (!sp.mathHtml.empty() || !sp.mathSvg.empty()) { lineHasMath = true; break; }
        }
        if (!lineHasMath) {
            if (line.size() == 1) {
                textOut += "<tspan x=\"" + num(cx) + "\" y=\"" + num(y) + "\"" + spanStyleAttr(line[0].style) + ">" +
                           esc(line[0].text) + "</tspan>";
            } else {
                bool first = true;
                for (const auto& sp : line) {
                    if (sp.text.empty()) continue;
                    textOut += "<tspan" + (first ? (" x=\"" + num(cx) + "\" y=\"" + num(y) + "\"") : std::string("")) +
                               spanStyleAttr(sp.style) + ">" + esc(sp.text) + "</tspan>";
                    first = false;
                }
            }
            continue;
        }

        double lineW = (i < label.widths.size()) ? label.widths[i] : label.width;
        double x = cx - lineW / 2.0;
        for (const auto& sp : line) {
            if (!sp.mathSvg.empty()) {
                double mh = (sp.mathHeight > 0.0) ? sp.mathHeight : label.lineHeight;
                double mx = x;
                double my = midY - mh / 2.0;
                mathOut += "<svg class=\"pele-math\" x=\"" + num(mx) + "\" y=\"" + num(my) +
                           "\" width=\"" + num(sp.width) + "\" height=\"" + num(mh) +
                           "\" overflow=\"visible\"" + mathAttrs + ">" + sp.mathSvg + "</svg>";
            } else if (!sp.mathHtml.empty()) {
                double mh = (sp.mathHeight > 0.0) ? sp.mathHeight : label.lineHeight;
                double mx = x;
                double my = midY - mh / 2.0;
                mathOut += "<foreignObject class=\"pele-math\" x=\"" + num(mx) + "\" y=\"" + num(my) +
                           "\" width=\"" + num(sp.width) + "\" height=\"" + num(mh) +
                           "\" overflow=\"visible\"" + mathAttrs + "><div xmlns=\"http://www.w3.org/1999/xhtml\" style=\"display:flex;align-items:center;justify-content:center;width:100%;height:100%;font-size:" +
                           num(label.size) + "px;line-height:normal;white-space:nowrap\">" +
                           sp.mathHtml + "</div></foreignObject>";
            } else if (!sp.text.empty()) {
                textOut += "<tspan x=\"" + num(x) + "\" y=\"" + num(y) + "\" text-anchor=\"start\"" +
                           spanStyleAttr(sp.style) + ">" + esc(sp.text) + "</tspan>";
            }
            x += sp.width;
        }
    }
    std::string out;
    if (!textOut.empty()) {
        out += "<text" + attrs + " text-anchor=\"middle\" xml:space=\"preserve\">" + textOut + "</text>";
    }
    out += mathOut;
    return out;
}

inline std::string edgeLabelSvg(const std::string& id, const Label& label, double cx, double cy, const std::string& textAttrs = "") {
    if (label.width <= 0.0 || label.height <= 0.0) return "";
    double bw = label.width + 8.0;
    double bh = label.height;
    return "<g class=\"pele-edge-label\" data-id=\"" + escText(id) + "\">" +
           "<rect x=\"" + num(cx - bw / 2.0) + "\" y=\"" + num(cy - bh / 2.0) +
           "\" width=\"" + num(bw) + "\" height=\"" + num(bh) + "\" rx=\"3\" fill=\"var(--_bg)\"/>" +
           labelSvg(label, cx, cy, textAttrs) + "</g>";
}

inline std::string labelSvg(double cx, double cy, const Label& label, double fontSize, const std::string& anchor = "middle", const std::string& fill = "") {
    std::string attrs;
    if (std::abs(fontSize - 16.0) > 0.1) attrs += " font-size=\"" + num(fontSize) + "\"";
    if (!fill.empty()) attrs += " fill=\"" + esc(fill) + "\"";
    bool hasMath = false;
    for (const auto& line : label.lines) {
        for (const auto& sp : line) {
            if (!sp.mathHtml.empty() || !sp.mathSvg.empty()) { hasMath = true; break; }
        }
        if (hasMath) break;
    }
    double adjCx = cx;
    if (hasMath) {
        if (anchor == "start") adjCx = cx + label.width / 2.0;
        else if (anchor == "end") adjCx = cx - label.width / 2.0;
    }
    std::string res = labelSvg(label, adjCx, cy, attrs);
    if (!hasMath && anchor != "middle" && !anchor.empty()) {
        size_t pos = res.find("text-anchor=\"middle\"");
        if (pos != std::string::npos) res.replace(pos, 20, "text-anchor=\"" + anchor + "\"");
    }
    return res;
}

inline std::string edgeLabelSvg(double cx, double cy, const Label& label, double fontSize) {
    std::string attrs;
    if (std::abs(fontSize - 16.0) > 0.1) attrs += " font-size=\"" + num(fontSize) + "\"";
    return edgeLabelSvg("", label, cx, cy, attrs);
}

inline std::string svgDocument(const std::string& type, double width, double height, double fontSize, const std::string& content) {
    int w = std::max(1, (int)std::ceil(width));
    int h = std::max(1, (int)std::ceil(height));
    int fs = std::max(1, (int)std::round(fontSize));
    std::string extras;
    if (content.find("var(--_r)") != std::string::npos) extras += ";--_r:var(--pele-radius,4px)";
    if (content.find("var(--_tw)") != std::string::npos) extras += ";--_tw:var(--pele-title-weight,bold)";
    if (content.find("var(--_hw)") != std::string::npos) extras += ";--_hw:var(--pele-heading-weight,bold)";
    if (content.find("var(--_w)") != std::string::npos) extras += ";--_w:var(--pele-bold-weight,bold)";
    if (content.find("var(--_fm)") != std::string::npos) extras += ";--_fm:var(--pele-font-mono,monospace)";

    return "<svg xmlns=\"http://www.w3.org/2000/svg\" class=\"pele pele-" + type +
           "\" viewBox=\"0 0 " + std::to_string(w) + " " + std::to_string(h) +
           "\" width=\"" + std::to_string(w) + "\" height=\"" + std::to_string(h) +
           "\" style=\"max-width:100%;height:auto;--_bg:var(--pele-bg,#fff);--_fg:var(--pele-fg,#1f1f1f);"
           "--_m:var(--pele-muted,#6e6e6e);--_l:var(--pele-line,#1f1f1f);--_s:var(--pele-surface,#f3f3f3);"
           "--_a:var(--pele-surface-alt,#e6e6e6);--_b:var(--pele-border,#8a8a8a);--_c:var(--pele-accent,#5b7bd5);"
           "color:var(--_fg)" + extras + ";font:" + std::to_string(fs) +
           "px sans-serif;letter-spacing:normal;word-spacing:normal;text-transform:none;direction:ltr;"
           "font-family:var(--pele-font,sans-serif)\" fill=\"var(--_fg)\" role=\"graphics-document document\" "
           "aria-roledescription=\"" + type + "\">" + content + "</svg>";
}

// ============================================================================
// Node Shapes & Sizing (src/svg/shapes.ts & src/diagrams/flowchart/shapes.ts)
// ============================================================================
struct ShapeSize {
    double w = 0.0;
    double h = 0.0;
    double dy = 0.0;
};

inline std::string canonicalShape(const std::string& raw) {
    std::string s = toLower(trim(raw));
    if (s.empty() || s == "rect" || s == "proc" || s == "process" || s == "rectangle" || s == "squarerect" || s == "square") return "rect";
    if (s == "rounded" || s == "event" || s == "roundedrect" || s == "round") return "rounded";
    if (s == "stadium" || s == "terminal" || s == "pill") return "stadium";
    if (s == "fr-rect" || s == "subprocess" || s == "subproc" || s == "framed-rectangle" || s == "subroutine") return "fr-rect";
    if (s == "cyl" || s == "db" || s == "database" || s == "cylinder") return "cyl";
    if (s == "circle" || s == "circ") return "circle";
    if (s == "dbl-circ" || s == "double-circle" || s == "doublecircle") return "dbl-circ";
    if (s == "sm-circ" || s == "start" || s == "small-circle" || s == "statestart") return "sm-circ";
    if (s == "fr-circ" || s == "stop" || s == "framed-circle" || s == "stateend") return "fr-circ";
    if (s == "f-circ" || s == "junction" || s == "filled-circle") return "f-circ";
    if (s == "diam" || s == "decision" || s == "diamond" || s == "question") return "diam";
    if (s == "hex" || s == "hexagon" || s == "prepare") return "hex";
    if (s == "lean-r" || s == "lean-right" || s == "in-out" || s == "lean_right") return "lean-r";
    if (s == "lean-l" || s == "lean-left" || s == "out-in" || s == "lean_left") return "lean-l";
    if (s == "trap-b" || s == "priority" || s == "trapezoid-bottom" || s == "trapezoid") return "trap-b";
    if (s == "trap-t" || s == "manual" || s == "trapezoid-top" || s == "inv-trapezoid" || s == "inv_trapezoid") return "trap-t";
    if (s == "odd" || s == "rect_left_inv_arrow" || s == "asymmetric") return "odd";
    if (s == "cloud") return "cloud";
    if (s == "bang") return "bang";
    if (s == "doc" || s == "document") return "doc";
    if (s == "docs" || s == "documents" || s == "st-doc" || s == "stacked-document") return "docs";
    if (s == "delay" || s == "half-rounded-rectangle") return "delay";
    if (s == "h-cyl" || s == "das" || s == "horizontal-cylinder") return "h-cyl";
    if (s == "tri" || s == "extract" || s == "triangle") return "tri";
    if (s == "flip-tri" || s == "manual-file" || s == "flipped-triangle") return "flip-tri";
    if (s == "hourglass" || s == "collate") return "hourglass";
    if (s == "fork" || s == "join" || s == "forkjoin") return "fork";
    if (s == "note" || s == "notch-rect" || s == "card" || s == "notched-rectangle") return "notch-rect";
    if (s == "text") return "text";
    if (s == "ellipse") return "ellipse";
    if (s == "person") return "person";
    if (s == "input" || s == "output" || s == "human" || s == "user") return "stadium";
    if (s == "task" || s == "agent") return "rounded";
    if (s == "tool" || s == "action") return "hex";
    if (s == "decision" || s == "router" || s == "guard" || s == "condition") return "diam";
    if (s == "llm" || s == "model") return "fr-rect";
    if (s == "memory" || s == "store") return "cyl";
    return "rect";
}

inline ShapeSize shapeSize(const std::string& shape, double lw, double lh) {
    static constexpr double PAD_X = 18.0;
    static constexpr double PAD_Y = 12.0;
    double w = std::max(lw + 2.0 * PAD_X, 2.0 * PAD_X + 12.0);
    double h = std::max(lh + 2.0 * PAD_Y, 36.0);
    if (shape == "sm-circ" || shape == "f-circ") return {14.0, 14.0, 0.0};
    if (shape == "fr-circ") return {20.0, 20.0, 0.0};
    if (shape == "fork") return {72.0, 8.0, 0.0};
    if (shape == "circle") {
        double d = std::max(std::hypot(lw, lh) + 20.0, 44.0);
        return {d, d, 0.0};
    }
    if (shape == "dbl-circ") {
        double d = std::max(std::hypot(lw, lh) + 28.0, 52.0);
        return {d, d, 0.0};
    }
    if (shape == "ellipse") {
        return { std::max(lw * 1.35 + 24.0, 56.0), std::max(lh * 1.35 + 18.0, 40.0), 0.0 };
    }
    if (shape == "diam") {
        double dw = std::max((lw + 20.0) * 1.6, 64.0);
        double dh = std::max((lh + 16.0) * 1.6, 48.0);
        return {dw, dh, 0.0};
    }
    if (shape == "hex" || shape == "lean-r" || shape == "lean-l" || shape == "trap-b" || shape == "trap-t" || shape == "odd") {
        return {w + 20.0, h, 0.0};
    }
    if (shape == "stadium") {
        return {w + h * 0.35, h, 0.0};
    }
    if (shape == "fr-rect") {
        return {w + 16.0, h, 0.0};
    }
    if (shape == "cyl") {
        double ry = std::min(10.0, std::max(6.0, w / 12.0));
        return {w, h + 2.0 * ry, ry * 0.5};
    }
    if (shape == "h-cyl") {
        return {w + 20.0, h, 0.0};
    }
    if (shape == "doc" || shape == "docs") {
        return {w, h + 10.0, -3.0};
    }
    if (shape == "tri") {
        return {std::max(lw * 1.8 + 28.0, 64.0), std::max(lh * 1.8 + 24.0, 52.0), lh * 0.2};
    }
    if (shape == "flip-tri") {
        return {std::max(lw * 1.8 + 28.0, 64.0), std::max(lh * 1.8 + 24.0, 52.0), -lh * 0.2};
    }
    if (shape == "cloud") {
        return {w + 24.0, h + 16.0, 0.0};
    }
    if (shape == "person") {
        return {w, h + 28.0, 14.0};
    }
    return {w, h, 0.0};
}

inline std::string drawShape(const std::string& shape, double w, double h, const std::string& shapeAttrs, const std::string& lineAttrs = " stroke=\"var(--_b)\"") {
    double hw = w / 2.0;
    double hh = h / 2.0;
    if (shape == "text") return "";
    if (shape == "rounded") {
        return "<rect x=\"" + num(-hw) + "\" y=\"" + num(-hh) + "\" width=\"" + num(w) + "\" height=\"" + num(h) + "\" rx=\"8\"" + shapeAttrs + "/>";
    }
    if (shape == "stadium") {
        return "<rect x=\"" + num(-hw) + "\" y=\"" + num(-hh) + "\" width=\"" + num(w) + "\" height=\"" + num(h) + "\" rx=\"" + num(hh) + "\"" + shapeAttrs + "/>";
    }
    if (shape == "circle" || shape == "sm-circ") {
        return "<circle r=\"" + num(hw) + "\"" + shapeAttrs + "/>";
    }
    if (shape == "f-circ") {
        return "<circle r=\"" + num(hw) + "\" fill=\"var(--_l)\" stroke=\"var(--_l)\"/>";
    }
    if (shape == "fr-circ") {
        return "<circle r=\"" + num(hw) + "\" fill=\"var(--_bg)\"" + lineAttrs + "/><circle r=\"" + num(hw - 4.0) + "\" fill=\"var(--_l)\"/>";
    }
    if (shape == "dbl-circ") {
        return "<circle r=\"" + num(hw) + "\"" + shapeAttrs + "/><circle r=\"" + num(std::max(2.0, hw - 5.0)) + "\" fill=\"none\"" + lineAttrs + "/>";
    }
    if (shape == "ellipse") {
        return "<ellipse rx=\"" + num(hw) + "\" ry=\"" + num(hh) + "\"" + shapeAttrs + "/>";
    }
    if (shape == "diam") {
        return "<polygon points=\"0," + num(-hh) + " " + num(hw) + ",0 0," + num(hh) + " " + num(-hw) + ",0\" stroke-linejoin=\"round\"" + shapeAttrs + "/>";
    }
    if (shape == "hex") {
        double k = std::min(14.0, hw * 0.4);
        return "<polygon points=\"" + num(-hw + k) + "," + num(-hh) + " " + num(hw - k) + "," + num(-hh) + " " +
               num(hw) + ",0 " + num(hw - k) + "," + num(hh) + " " + num(-hw + k) + "," + num(hh) + " " +
               num(-hw) + ",0\" stroke-linejoin=\"round\"" + shapeAttrs + "/>";
    }
    if (shape == "lean-r") {
        double k = std::min(14.0, hw * 0.35);
        return "<polygon points=\"" + num(-hw + k) + "," + num(-hh) + " " + num(hw) + "," + num(-hh) + " " +
               num(hw - k) + "," + num(hh) + " " + num(-hw) + "," + num(hh) + "\" stroke-linejoin=\"round\"" + shapeAttrs + "/>";
    }
    if (shape == "lean-l") {
        double k = std::min(14.0, hw * 0.35);
        return "<polygon points=\"" + num(-hw) + "," + num(-hh) + " " + num(hw - k) + "," + num(-hh) + " " +
               num(hw) + "," + num(hh) + " " + num(-hw + k) + "," + num(hh) + "\" stroke-linejoin=\"round\"" + shapeAttrs + "/>";
    }
    if (shape == "trap-b") {
        double k = std::min(14.0, hw * 0.35);
        return "<polygon points=\"" + num(-hw + k) + "," + num(-hh) + " " + num(hw - k) + "," + num(-hh) + " " +
               num(hw) + "," + num(hh) + " " + num(-hw) + "," + num(hh) + "\" stroke-linejoin=\"round\"" + shapeAttrs + "/>";
    }
    if (shape == "trap-t") {
        double k = std::min(14.0, hw * 0.35);
        return "<polygon points=\"" + num(-hw) + "," + num(-hh) + " " + num(hw) + "," + num(-hh) + " " +
               num(hw - k) + "," + num(hh) + " " + num(-hw + k) + "," + num(hh) + "\" stroke-linejoin=\"round\"" + shapeAttrs + "/>";
    }
    if (shape == "odd") {
        double k = std::min(14.0, hh);
        return "<polygon points=\"" + num(-hw) + "," + num(-hh) + " " + num(hw) + "," + num(-hh) + " " +
               num(hw) + "," + num(hh) + " " + num(-hw) + "," + num(hh) + " " + num(-hw + k) + ",0\" stroke-linejoin=\"round\"" + shapeAttrs + "/>";
    }
    if (shape == "fr-rect") {
        return "<rect x=\"" + num(-hw) + "\" y=\"" + num(-hh) + "\" width=\"" + num(w) + "\" height=\"" + num(h) + "\"" + rounded(shapeAttrs) + "/>" +
               "<path d=\"M" + num(-hw + 8.0) + "," + num(-hh) + "V" + num(hh) + "M" + num(hw - 8.0) + "," + num(-hh) + "V" + num(hh) + "\" fill=\"none\"" + lineAttrs + "/>";
    }
    if (shape == "cyl") {
        double ry = std::min(10.0, std::max(6.0, w / 12.0));
        double ty = -hh + ry;
        double by = hh - ry;
        std::string l = num(-hw), r = num(hw), rxS = num(hw), ryS = num(ry);
        return "<path d=\"M" + l + "," + num(ty) + "V" + num(by) + "A" + rxS + "," + ryS + " 0 0 0 " + r + "," + num(by) +
               "V" + num(ty) + "A" + rxS + "," + ryS + " 0 0 0 " + l + "," + num(ty) + "Z\"" + shapeAttrs + "/>" +
               "<path d=\"M" + l + "," + num(ty) + "A" + rxS + "," + ryS + " 0 0 0 " + r + "," + num(ty) + "\" fill=\"none\"" + lineAttrs + "/>";
    }
    if (shape == "doc" || shape == "docs") {
        double wave = 6.0;
        double by = hh - wave;
        return "<path d=\"M" + num(-hw) + "," + num(-hh) + "H" + num(hw) + "V" + num(by) +
               "Q" + num(hw * 0.5) + "," + num(by - wave) + " 0," + num(by) +
               "T" + num(-hw) + "," + num(by) + "Z\"" + shapeAttrs + "/>";
    }
    if (shape == "delay") {
        return "<path d=\"M" + num(-hw) + "," + num(-hh) + "H" + num(hw - hh) + "A" + num(hh) + "," + num(hh) +
               " 0 0 1 " + num(hw - hh) + "," + num(hh) + "H" + num(-hw) + "Z\"" + shapeAttrs + "/>";
    }
    if (shape == "tri") {
        return "<polygon points=\"0," + num(-hh) + " " + num(hw) + "," + num(hh) + " " + num(-hw) + "," + num(hh) + "\" stroke-linejoin=\"round\"" + shapeAttrs + "/>";
    }
    if (shape == "flip-tri") {
        return "<polygon points=\"" + num(-hw) + "," + num(-hh) + " " + num(hw) + "," + num(-hh) + " 0," + num(hh) + "\" stroke-linejoin=\"round\"" + shapeAttrs + "/>";
    }
    if (shape == "fork") {
        return "<rect x=\"" + num(-hw) + "\" y=\"" + num(-hh) + "\" width=\"" + num(w) + "\" height=\"" + num(h) + "\" rx=\"2\" fill=\"var(--_l)\"/>";
    }
    if (shape == "person") {
        double bodyTop = -hh + 28.0;
        return "<circle cx=\"0\" cy=\"" + num(-hh + 13.0) + "\" r=\"13\"" + shapeAttrs + "/>" +
               "<rect x=\"" + num(-hw) + "\" y=\"" + num(bodyTop) + "\" width=\"" + num(w) + "\" height=\"" + num(hh - bodyTop) + "\" rx=\"10\"" + shapeAttrs + "/>";
    }
    return "<rect x=\"" + num(-hw) + "\" y=\"" + num(-hh) + "\" width=\"" + num(w) + "\" height=\"" + num(h) + "\"" + rounded(shapeAttrs) + "/>";
}

inline std::string drawShape(const std::string& shape, double cx, double cy, double w, double h, const ResolvedStyle& rs) {
    std::string fill = rs.fill.empty() ? "var(--_s)" : esc(rs.fill);
    std::string stroke = rs.stroke.empty() ? "var(--_b)" : esc(rs.stroke);
    std::string shapeAttrs = " fill=\"" + fill + "\" stroke=\"" + stroke + "\" stroke-width=\"var(--_w)\"" + rs.extra + rs.shape;
    std::string lineAttrs = " stroke=\"" + stroke + "\" stroke-width=\"var(--_w)\"";
    return "<g transform=\"translate(" + num(cx) + "," + num(cy) + ")\">" + drawShape(shape, w, h, shapeAttrs, lineAttrs) + "</g>";
}

// ============================================================================
// Edge Markers & Path Routing (src/svg/edges.ts)
// ============================================================================
static constexpr double ARROW_LEN = 8.0;

inline double markerTrim(const std::string& type) {
    if (type == "arrow_point") return ARROW_LEN - 1.0;
    if (type == "arrow_circle" || type == "arrow_cross") return 4.0;
    if (type == "extension") return 12.0;
    if (type == "composition" || type == "aggregation") return 16.0;
    return 0.0;
}

inline std::string marker(const std::string& type, double x, double y, double dx, double dy, const std::string& color = "var(--_l)") {
    if (type.empty() || type == "none" || type == "arrow_open") return "";
    double nx = -dy;
    double ny = dx;
    if (type == "arrow_point") {
        double bx = x - dx * ARROW_LEN;
        double by = y - dy * ARROW_LEN;
        double half = 3.36;
        return "<path class=\"pele-marker\" d=\"M" + num(x) + "," + num(y) +
               "L" + num(bx + nx * half) + "," + num(by + ny * half) +
               "L" + num(bx - nx * half) + "," + num(by - ny * half) +
               "Z\" fill=\"" + color + "\" stroke=\"none\"/>";
    }
    if (type == "arrow_circle") {
        double r = 4.0;
        return "<circle class=\"pele-marker\" cx=\"" + num(x - dx * r) + "\" cy=\"" + num(y - dy * r) +
               "\" r=\"" + num(r) + "\" fill=\"var(--_bg)\" stroke=\"" + color + "\"/>";
    }
    if (type == "arrow_cross") {
        double cx = x - dx * 4.0;
        double cy = y - dy * 4.0;
        double d = 3.5;
        return "<path class=\"pele-marker\" d=\"M" + num(cx - (dx + nx) * d) + "," + num(cy - (dy + ny) * d) +
               "L" + num(cx + (dx + nx) * d) + "," + num(cy + (dy + ny) * d) +
               "M" + num(cx - (dx - nx) * d) + "," + num(cy - (dy - ny) * d) +
               "L" + num(cx + (dx - nx) * d) + "," + num(cy + (dy - ny) * d) + "\"/>";
    }
    if (type == "extension") {
        double bx = x - dx * 12.0;
        double by = y - dy * 12.0;
        return "<path class=\"pele-marker\" d=\"M" + num(x) + "," + num(y) +
               "L" + num(bx + nx * 6.0) + "," + num(by + ny * 6.0) +
               "L" + num(bx - nx * 6.0) + "," + num(by - ny * 6.0) +
               "Z\" fill=\"var(--_bg)\" stroke=\"" + color + "\" stroke-linejoin=\"round\"/>";
    }
    if (type == "composition" || type == "aggregation") {
        double mx = x - dx * 8.0;
        double my = y - dy * 8.0;
        double bx = x - dx * 16.0;
        double by = y - dy * 16.0;
        return "<path class=\"pele-marker\" d=\"M" + num(x) + "," + num(y) +
               "L" + num(mx + nx * 5.0) + "," + num(my + ny * 5.0) +
               "L" + num(bx) + "," + num(by) +
               "L" + num(mx - nx * 5.0) + "," + num(my - ny * 5.0) +
               "Z\" stroke-linejoin=\"round\"" + (type == "composition" ? (" fill=\"" + color + "\"") : " fill=\"var(--_bg)\"") + "/>";
    }
    return "";
}

struct EdgePath {
    std::string d;
    double sx = 0, sy = 0, sdx = 0, sdy = 0;
    double ex = 0, ey = 0, edx = 0, edy = 0;
};

inline double easeCurve(double across, double along) {
    double denom = std::abs(across) > 1e-6 ? std::abs(across) : 1.0;
    return std::max(0.25, std::min(0.5, (4.0 * std::abs(along)) / denom));
}

// Builds the path for a route of (x, y) points, trimming both ends to leave room for markers,
// adding straight LEAD=6 segments before markers, and drawing flow-aligned cubic S-curves (C).
inline EdgePath routePoints(
    const std::vector<std::pair<double, double>>& pts,
    const std::string& curve,
    double startTrim,
    double endTrim,
    int defaultAxis = -1
) {
    EdgePath ep;
    if (pts.size() < 2) return ep;

    const bool linear = (curve == "linear");
    const bool stepped = startsWith(curve, "step");
    const double LEAD = 6.0;
    const double CORNER = 12.0;
    const double HANDLE = 0.62;

    int axis = defaultAxis;
    if (axis < 0) {
        double totDx = std::abs(pts.back().first - pts.front().first);
        double totDy = std::abs(pts.back().second - pts.front().second);
        axis = (totDy >= totDx) ? 0 : 1;
    }

    auto nearPt = [](double ax, double ay, double bx, double by) {
        return std::abs(ax - bx) < 0.01 && std::abs(ay - by) < 0.01;
    };

    std::vector<double> xs, ys;
    std::vector<int> axes;
    for (size_t i = 0; i < pts.size(); i++) {
        double x = pts[i].first;
        double y = pts[i].second;
        if (stepped && !xs.empty() && std::abs(x - xs.back()) >= 0.01 && std::abs(y - ys.back()) >= 0.01) {
            double px = xs.back(), py = ys.back();
            double my = (curve == "stepBefore") ? py : (curve == "stepAfter") ? y : (py + y) / 2.0;
            double mx = (curve == "stepBefore") ? px : (curve == "stepAfter") ? x : (px + x) / 2.0;
            double turns[4] = {
                axis == 0 ? px : mx,
                axis == 0 ? my : py,
                axis == 0 ? x  : mx,
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
    auto unit = [&](int i, int j, int ax) -> std::pair<double, double> {
        if (!straight(std::max(i, j))) {
            if (ax == 0) return {0.0, (ys[j] - ys[i] >= 0.0) ? 1.0 : -1.0};
            return {(xs[j] - xs[i] >= 0.0) ? 1.0 : -1.0, 0.0};
        }
        double dx = xs[j] - xs[i];
        double dy = ys[j] - ys[i];
        double len = std::hypot(dx, dy);
        if (len < 1e-6) return {1.0, 0.0};
        return {dx / len, dy / len};
    };

    auto [sdx, sdy] = unit(0, 1, axes[1]);
    auto [edx, edy] = unit(last - 1, last, axes[last]);
    ep.sx = xs[0];
    ep.sy = ys[0];
    ep.ex = xs[last];
    ep.ey = ys[last];
    ep.sdx = -sdx;
    ep.sdy = -sdy;
    ep.edx = edx;
    ep.edy = edy;

    xs[0] += sdx * startTrim;
    ys[0] += sdy * startTrim;
    xs[last] -= edx * endTrim;
    ys[last] -= edy * endTrim;

    auto segLen = [&](int i) {
        return std::abs(xs[i] - xs[i - 1]) + std::abs(ys[i] - ys[i - 1]);
    };
    auto turns = [&](int i) {
        return !linear && i > 0 && i < last && aligned(i) && aligned(i + 1) &&
               segLen(i) >= 0.01 && segLen(i + 1) >= 0.01 &&
               ((std::abs(xs[i] - xs[i - 1]) < 0.01) != (std::abs(xs[i + 1] - xs[i]) < 0.01));
    };
    auto alongRun = [&](int i, double ux, double uy) {
        return std::abs(xs[i] - xs[i - 1]) * std::abs(ux) + std::abs(ys[i] - ys[i - 1]) * std::abs(uy);
    };
    auto leadDist = [&](double reach, double tr, double run) {
        double room = std::min(std::max(0.0, reach - LEAD - tr), run / 4.0);
        return room + std::min(reach - tr - room, (run - room) / 4.0);
    };

    double startReach = (startTrim > 0.0) ? (startTrim + LEAD) : 0.0;
    double endReach = (endTrim > 0.0) ? (endTrim + LEAD) : 0.0;
    double arrives = (endReach > endTrim && (!straight(last) || turns(last - 1)))
                     ? leadDist(endReach, endTrim, alongRun(last, edx, edy))
                     : 0.0;
    double leaves = (startReach > startTrim && (!straight(1) || turns(1)))
                    ? leadDist(startReach, startTrim, alongRun(1, sdx, sdy))
                    : 0.0;

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

    ep.d = "M" + num(xs[0]) + "," + num(ys[0]);
    double atX = xs[0], atY = ys[0];
    auto lineTo = [&](double x, double y) {
        if (nearPt(x, y, atX, atY)) return;
        ep.d += "L" + num(x) + "," + num(y);
        atX = x;
        atY = y;
    };

    for (int i = 1; i <= last; i++) {
        double x = xs[i], y = ys[i];
        if (!straight(i)) {
            bool down = (axes[i] == 0);
            double al = down ? (y - atY) : (x - atX);
            double ac = down ? (x - atX) : (y - atY);
            double k = std::abs(al) * easeCurve(ac, al);
            double signAl = (al >= 0.0) ? 1.0 : -1.0;
            double ux = down ? 0.0 : signAl * k;
            double uy = down ? signAl * k : 0.0;
            ep.d += "C" + num(atX + ux) + "," + num(atY + uy) + " " +
                    num(x - ux) + "," + num(y - uy) + " " +
                    num(x) + "," + num(y);
            atX = x;
            atY = y;
            continue;
        }
        if (!turns(i)) {
            if (i < last && aligned(i) && aligned(i + 1) &&
                ((x - xs[i - 1]) * (xs[i + 1] - x) + (y - ys[i - 1]) * (ys[i + 1] - y) > 0.0)) {
                continue;
            }
            lineTo(x, y);
            continue;
        }
        double before = turns(i - 1) ? segLen(i) / 2.0 : segLen(i);
        double after = turns(i + 1) ? segLen(i + 1) / 2.0 : segLen(i + 1);
        if (stepped) before = after = std::min({CORNER, before, after});
        auto [ux, uy] = unit(i - 1, i, axes[i]);
        auto [vx, vy] = unit(i, i + 1, axes[i + 1]);
        lineTo(x - ux * before, y - uy * before);
        atX = x + vx * after;
        atY = y + vy * after;
        if (stepped) {
            ep.d += "Q" + num(x) + "," + num(y) + " " + num(atX) + "," + num(atY);
        } else {
            ep.d += "C" + num(x - ux * before * (1.0 - HANDLE)) + "," + num(y - uy * before * (1.0 - HANDLE)) + " " +
                    num(x + vx * after * (1.0 - HANDLE)) + "," + num(y + vy * after * (1.0 - HANDLE)) + " " +
                    num(atX) + "," + num(atY);
        }
    }
    return ep;
}

inline std::string marker(const std::string& type, double x, double y, double angle) {
    return marker(type, x, y, std::cos(angle), std::sin(angle));
}

inline std::string routePoints(const std::vector<std::pair<double, double>>& pts, const std::string& curve) {
    return routePoints(pts, curve, 0.0, 0.0).d;
}

// ============================================================================
// Sugiyama Layered & Compound Graph Layout (src/layout/layered.ts & compound.ts)
// ============================================================================
struct LayoutNode {
    std::string id;
    std::string label;
    std::string shape = "rect";
    std::map<std::string, std::string> style;
    int parentSubgraph = -1;
    double w = 60.0;
    double h = 36.0;
    double x = 0.0;
    double y = 0.0;
    int parent = -1; // index of parent group node, or -1
    bool isGroup = false;
    double padX = 20.0;
    double padTop = 36.0;
    double padBottom = 20.0;
    double minW = 80.0;
};

struct LayoutEdge {
    int src = 0;
    int dst = 0;
    int minlen = 1;
    std::string label;
    std::string stroke = "solid";
    std::string arrowStart = "none";
    std::string arrowEnd = "arrow_point";
    double labelW = 0.0;
    double labelH = 0.0;
    std::vector<std::pair<double, double>> route;
    double labelX = 0.0;
    double labelY = 0.0;
};

struct LayoutSubgraph {
    std::string id;
    std::string title;
    std::string direction;
    int parentSubgraph = -1;
    std::vector<int> nodeIndices;
    std::vector<int> childSubgraphs;
    double x = 0.0;
    double y = 0.0;
    double w = 0.0;
    double h = 0.0;
};

struct LayoutConfig {
    std::string dir = "TB"; // TB, TD, BT, LR, RL
    double nodeSep = 40.0;
    double rankSep = 48.0;
    double edgeSep = 16.0;
};

struct LayoutResult {
    double width = 0.0;
    double height = 0.0;
};

inline void clipToBoxBorder(double cx, double cy, double w, double h, double tx, double ty, double& ox, double& oy) {
    double dx = tx - cx;
    double dy = ty - cy;
    double hw = std::max(1.0, w / 2.0);
    double hh = std::max(1.0, h / 2.0);
    if (std::abs(dx) < 1e-6 && std::abs(dy) < 1e-6) {
        ox = cx;
        oy = cy + hh;
        return;
    }
    double sx = (std::abs(dx) > 1e-6) ? (hw / std::abs(dx)) : 1e9;
    double sy = (std::abs(dy) > 1e-6) ? (hh / std::abs(dy)) : 1e9;
    double s = std::min(sx, sy);
    ox = cx + dx * s;
    oy = cy + dy * s;
}

inline LayoutResult layoutGraph(std::vector<LayoutNode>& nodes, std::vector<LayoutEdge>& edges, const LayoutConfig& cfg) {
    LayoutResult res;
    int N = (int)nodes.size();
    if (N == 0) return res;

    bool horiz = (cfg.dir == "LR" || cfg.dir == "RL");
    bool rev = (cfg.dir == "BT" || cfg.dir == "RL");

    // In canonical Sugiyama space, flow runs top-to-bottom (+Y).
    // If horizontal, swap w and h of leaf nodes during layout, then swap back.
    std::vector<double> lw(N), lh(N);
    for (int i = 0; i < N; i++) {
        lw[i] = horiz ? nodes[i].h : nodes[i].w;
        lh[i] = horiz ? nodes[i].w : nodes[i].h;
    }

    // Identify leaf (non-empty-group) nodes vs group containers
    std::vector<bool> hasChildren(N, false);
    for (int i = 0; i < N; i++) {
        if (nodes[i].parent >= 0 && nodes[i].parent < N) {
            hasChildren[nodes[i].parent] = true;
        }
    }

    // Map any edge endpoint on a group to its first leaf descendant so ranks stay consistent
    auto repLeaf = [&](int u) {
        int cur = u;
        for (int hop = 0; hop < N && cur >= 0 && hasChildren[cur]; hop++) {
            int found = -1;
            for (int j = 0; j < N; j++) {
                if (nodes[j].parent == cur) { found = j; break; }
            }
            if (found < 0) break;
            cur = found;
        }
        return cur;
    };

    // 1. Cycle removal via DFS & Rank Assignment (Longest Path)
    std::vector<std::vector<std::pair<int, int>>> adj(N);
    std::vector<int> indeg(N, 0);
    std::vector<int> vis(N, 0); // 0=unvisited, 1=active, 2=done
    struct RawE { int u, v, minlen, origIdx; bool reversed; };
    std::vector<RawE> dagEdges;
    for (size_t ei = 0; ei < edges.size(); ei++) {
        int u = repLeaf(edges[ei].src);
        int v = repLeaf(edges[ei].dst);
        if (u < 0 || v < 0 || u >= N || v >= N || u == v) continue;
        int ml = std::max(1, edges[ei].minlen);
        dagEdges.push_back({u, v, ml, (int)ei, false});
    }

    // DFS to detect back-edges and reverse them
    std::vector<std::vector<int>> tmpAdj(N);
    for (size_t i = 0; i < dagEdges.size(); i++) tmpAdj[dagEdges[i].u].push_back((int)i);
    std::function<void(int)> dfs = [&](int u) {
        vis[u] = 1;
        for (int ei : tmpAdj[u]) {
            int v = dagEdges[ei].v;
            if (vis[v] == 1) {
                std::swap(dagEdges[ei].u, dagEdges[ei].v);
                dagEdges[ei].reversed = true;
            } else if (vis[v] == 0) {
                dfs(v);
            }
        }
        vis[u] = 2;
    };
    for (int i = 0; i < N; i++) {
        if (!hasChildren[i] && vis[i] == 0) dfs(i);
    }

    std::vector<int> rank(N, 0);
    for (int iter = 0; iter < N; iter++) {
        bool changed = false;
        for (const auto& de : dagEdges) {
            if (rank[de.v] < rank[de.u] + de.minlen) {
                rank[de.v] = rank[de.u] + de.minlen;
                changed = true;
            }
        }
        if (!changed) break;
    }

    int maxRank = 0;
    for (int i = 0; i < N; i++) {
        if (!hasChildren[i]) maxRank = std::max(maxRank, rank[i]);
    }

    // 2. Group nodes by rank
    std::vector<std::vector<int>> layers(maxRank + 1);
    for (int i = 0; i < N; i++) {
        if (!hasChildren[i]) layers[rank[i]].push_back(i);
    }

    // 3. Barycenter Crossing Minimization (8 sweeps down/up)
    std::vector<int> posInLayer(N, 0);
    auto updatePos = [&]() {
        for (auto& layer : layers) {
            for (size_t k = 0; k < layer.size(); k++) posInLayer[layer[k]] = (int)k;
        }
    };
    updatePos();

    for (int pass = 0; pass < 8; pass++) {
        // Downward sweep
        for (int r = 1; r <= maxRank; r++) {
            std::vector<std::pair<double, int>> bary;
            for (size_t idx = 0; idx < layers[r].size(); idx++) {
                int u = layers[r][idx];
                double sum = 0.0;
                int cnt = 0;
                for (const auto& de : dagEdges) {
                    if (de.v == u && rank[de.u] < r) { sum += posInLayer[de.u]; cnt++; }
                }
                // Group siblings together by parent
                double parentBias = (nodes[u].parent >= 0) ? (nodes[u].parent * 0.01) : 0.0;
                double b = (cnt > 0 ? (sum / cnt) : double(idx)) + parentBias;
                bary.push_back({b, u});
            }
            std::stable_sort(bary.begin(), bary.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
            for (size_t idx = 0; idx < bary.size(); idx++) layers[r][idx] = bary[idx].second;
            updatePos();
        }
        // Upward sweep
        for (int r = maxRank - 1; r >= 0; r--) {
            std::vector<std::pair<double, int>> bary;
            for (size_t idx = 0; idx < layers[r].size(); idx++) {
                int u = layers[r][idx];
                double sum = 0.0;
                int cnt = 0;
                for (const auto& de : dagEdges) {
                    if (de.u == u && rank[de.v] > r) { sum += posInLayer[de.v]; cnt++; }
                }
                double parentBias = (nodes[u].parent >= 0) ? (nodes[u].parent * 0.01) : 0.0;
                double b = (cnt > 0 ? (sum / cnt) : double(idx)) + parentBias;
                bary.push_back({b, u});
            }
            std::stable_sort(bary.begin(), bary.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
            for (size_t idx = 0; idx < bary.size(); idx++) layers[r][idx] = bary[idx].second;
            updatePos();
        }
    }

    // 4. Coordinate Assignment in canonical (X across layer, Y along ranks)
    std::vector<double> layerH(maxRank + 1, 0.0);
    std::vector<double> rankGap(maxRank + 1, cfg.rankSep);
    for (int r = 0; r <= maxRank; r++) {
        for (int u : layers[r]) layerH[r] = std::max(layerH[r], lh[u]);
    }
    // Increase rankGap if edges between r and r+1 have labels or cross group boundaries
    for (const auto& de : dagEdges) {
        const auto& e = edges[de.origIdx];
        int r = std::min(rank[de.u], rank[de.v]);
        double extra = horiz ? e.labelW : e.labelH;
        if (nodes[de.u].parent != nodes[de.v].parent) extra += 28.0;
        rankGap[r] = std::max(rankGap[r], cfg.rankSep + extra);
    }

    std::vector<double> layerY(maxRank + 1, 0.0);
    double curY = 0.0;
    for (int r = 0; r <= maxRank; r++) {
        int rr = rev ? (maxRank - r) : r;
        layerY[rr] = curY + layerH[rr] / 2.0;
        curY += layerH[rr] + rankGap[rr];
    }

    std::vector<double> cx(N, 0.0), cy(N, 0.0);
    double maxLayerW = 0.0;
    for (int r = 0; r <= maxRank; r++) {
        double totalW = 0.0;
        for (size_t i = 0; i < layers[r].size(); i++) {
            int u = layers[r][i];
            if (i > 0) {
                int prev = layers[r][i - 1];
                double sep = cfg.nodeSep + (nodes[u].parent != nodes[prev].parent ? 28.0 : 0.0);
                totalW += sep;
            }
            totalW += lw[u];
        }
        maxLayerW = std::max(maxLayerW, totalW);
        double x = -totalW / 2.0;
        for (size_t i = 0; i < layers[r].size(); i++) {
            int u = layers[r][i];
            if (i > 0) {
                int prev = layers[r][i - 1];
                x += cfg.nodeSep + (nodes[u].parent != nodes[prev].parent ? 28.0 : 0.0);
            }
            cx[u] = x + lw[u] / 2.0;
            cy[u] = layerY[r];
            x += lw[u];
        }
    }

    // Refine X positions toward connected neighbors (2 passes without creating overlaps)
    for (int iter = 0; iter < 4; iter++) {
        for (int r = 0; r <= maxRank; r++) {
            for (size_t i = 0; i < layers[r].size(); i++) {
                int u = layers[r][i];
                double sum = 0.0;
                int cnt = 0;
                for (const auto& de : dagEdges) {
                    if (de.u == u) { sum += cx[de.v]; cnt++; }
                    else if (de.v == u) { sum += cx[de.u]; cnt++; }
                }
                if (cnt > 0) {
                    double target = sum / cnt;
                    double lo = (i > 0) ? (cx[layers[r][i - 1]] + lw[layers[r][i - 1]] / 2.0 + cfg.nodeSep + (nodes[u].parent != nodes[layers[r][i - 1]].parent ? 28.0 : 0.0) + lw[u] / 2.0) : -1e9;
                    double hi = (i + 1 < layers[r].size()) ? (cx[layers[r][i + 1]] - lw[layers[r][i + 1]] / 2.0 - cfg.nodeSep - (nodes[u].parent != nodes[layers[r][i + 1]].parent ? 28.0 : 0.0) - lw[u] / 2.0) : 1e9;
                    if (lo <= hi) cx[u] = std::max(lo, std::min(hi, target));
                }
            }
        }
    }

    // Transform back from canonical to screen coordinates
    for (int i = 0; i < N; i++) {
        if (!hasChildren[i]) {
            if (horiz) {
                nodes[i].x = cy[i];
                nodes[i].y = cx[i];
            } else {
                nodes[i].x = cx[i];
                nodes[i].y = cy[i];
            }
        }
    }

    // 5. Compute bounding boxes of group containers bottom-up
    std::vector<int> depth(N, 0);
    for (int i = 0; i < N; i++) {
        int d = 0, p = nodes[i].parent;
        while (p >= 0 && p < N && d < N) { d++; p = nodes[p].parent; }
        depth[i] = d;
    }
    std::vector<int> groupOrder;
    for (int i = 0; i < N; i++) if (nodes[i].isGroup && hasChildren[i]) groupOrder.push_back(i);
    std::sort(groupOrder.begin(), groupOrder.end(), [&](int a, int b) { return depth[a] > depth[b]; });

    for (int g : groupOrder) {
        double gx0 = 1e9, gy0 = 1e9, gx1 = -1e9, gy1 = -1e9;
        for (int i = 0; i < N; i++) {
            if (nodes[i].parent == g) {
                gx0 = std::min(gx0, nodes[i].x - nodes[i].w / 2.0);
                gy0 = std::min(gy0, nodes[i].y - nodes[i].h / 2.0);
                gx1 = std::max(gx1, nodes[i].x + nodes[i].w / 2.0);
                gy1 = std::max(gy1, nodes[i].y + nodes[i].h / 2.0);
            }
        }
        if (gx0 <= gx1) {
            gx0 -= nodes[g].padX;
            gx1 += nodes[g].padX;
            gy0 -= nodes[g].padTop;
            gy1 += nodes[g].padBottom;
            double gw = std::max(gx1 - gx0, nodes[g].minW);
            double gh = gy1 - gy0;
            nodes[g].x = (gx0 + gx1) / 2.0;
            nodes[g].y = (gy0 + gy1) / 2.0;
            nodes[g].w = gw;
            nodes[g].h = gh;
        }
    }

    // Normalize coordinates so minX = 0, minY = 0
    double minX = 1e9, minY = 1e9, maxX = -1e9, maxY = -1e9;
    for (int i = 0; i < N; i++) {
        if (nodes[i].isGroup && !hasChildren[i] && nodes[i].w <= 0) continue;
        minX = std::min(minX, nodes[i].x - nodes[i].w / 2.0);
        minY = std::min(minY, nodes[i].y - nodes[i].h / 2.0);
        maxX = std::max(maxX, nodes[i].x + nodes[i].w / 2.0);
        maxY = std::max(maxY, nodes[i].y + nodes[i].h / 2.0);
    }
    if (minX > maxX) { minX = minY = maxX = maxY = 0.0; }
    constexpr double LAYOUT_PAD = 8.0;
    for (int i = 0; i < N; i++) {
        nodes[i].x = nodes[i].x - minX + LAYOUT_PAD;
        nodes[i].y = nodes[i].y - minY + LAYOUT_PAD;
    }
    res.width = (maxX - minX) + 2.0 * LAYOUT_PAD;
    res.height = (maxY - minY) + 2.0 * LAYOUT_PAD;

    // 6. Route edges with port spreading (`sideEnds` + `spread` from src/layout/layered.ts)
    struct PortEnd {
        size_t edgeIdx;
        bool isSrc;
        bool leaves;
        int side; // 0=top, 1=right, 2=bottom, 3=left
        double toward;
    };
    std::map<std::pair<int, int>, std::vector<PortEnd>> sidePorts;
    for (size_t ei = 0; ei < edges.size(); ei++) {
        const auto& e = edges[ei];
        if (e.src < 0 || e.src >= N || e.dst < 0 || e.dst >= N || e.src == e.dst) continue;
        const auto& s = nodes[e.src];
        const auto& t = nodes[e.dst];
        int sSide = 2, tSide = 0;
        if (horiz) {
            if (std::abs(t.x - s.x) > 1.0) {
                sSide = (t.x > s.x) ? 1 : 3;
                tSide = (t.x > s.x) ? 3 : 1;
            } else {
                sSide = (t.y > s.y) ? 2 : 0;
                tSide = (t.y > s.y) ? 0 : 2;
            }
        } else {
            if (std::abs(t.y - s.y) > 1.0) {
                sSide = (t.y > s.y) ? 2 : 0;
                tSide = (t.y > s.y) ? 0 : 2;
            } else {
                sSide = (t.x > s.x) ? 1 : 3;
                tSide = (t.x > s.x) ? 3 : 1;
            }
        }
        double sToward = (sSide == 0 || sSide == 2) ? t.x : t.y;
        double tToward = (tSide == 0 || tSide == 2) ? s.x : s.y;
        bool sLeaves = (e.arrowStart == "none" || e.arrowStart.empty());
        bool tLeaves = (e.arrowEnd == "none" || e.arrowEnd.empty());
        sidePorts[{e.src, sSide}].push_back({ei, true, sLeaves, sSide, sToward});
        sidePorts[{e.dst, tSide}].push_back({ei, false, tLeaves, tSide, tToward});
    }

    constexpr double PORT_PITCH = 16.0;
    constexpr double MIN_PITCH = 8.0;
    std::vector<std::pair<double, double>> srcPt(edges.size()), dstPt(edges.size());
    std::vector<int> srcSide(edges.size(), 2), dstSide(edges.size(), 0);
    for (auto& kv : sidePorts) {
        int u = kv.first.first;
        int side = kv.first.second;
        auto& list = kv.second;
        std::stable_sort(list.begin(), list.end(), [](const PortEnd& a, const PortEnd& b) {
            return a.toward < b.toward;
        });
        const auto& nd = nodes[u];
        bool verticalSide = (side == 1 || side == 3);
        bool zeroSpanShape = (nd.shape == "diam" || nd.shape == "tri" || nd.shape == "flip-tri" ||
                              nd.shape == "sm-circ" || nd.shape == "f-circ" || nd.shape == "fr-circ" ||
                              nd.shape == "person" || nd.shape == "text" || nd.shape == "circle" ||
                              nd.shape == "dbl-circ" || nd.shape == "ellipse");
        double span = zeroSpanShape ? 0.0 : std::max(0.0, (verticalSide ? nd.h : nd.w) - 24.0);
        std::vector<double> room(list.size(), 0.0);
        auto fill = [&](int mode) -> double {
            double tot = 0.0;
            if (!room.empty()) room[0] = 0.0;
            for (size_t k = 1; k < list.size(); k++) {
                bool same = (list[k].leaves == list[k - 1].leaves);
                bool bothLeaves = same && list[k].leaves;
                double r = 1.0;
                if (mode == 1) r = bothLeaves ? 0.5 : 1.0;
                else if (mode == 2) r = same ? 0.0 : 1.0;
                room[k] = r;
                tot += r;
            }
            return tot;
        };
        double total = (list.size() >= 2) ? fill(0) : 0.0;
        if (total > 0.0 && span / total < MIN_PITCH) total = fill(1);
        if (total > 0.0 && span / total < MIN_PITCH) total = fill(2);
        double pitch = (total > 0.0) ? std::min(PORT_PITCH, span / total) : 0.0;
        double at = -total * pitch / 2.0;
        for (size_t k = 0; k < list.size(); k++) {
            if (k > 0) at += room[k] * pitch;
            double offset = (list.size() > 1) ? at : 0.0;
            double px = nd.x + (side == 1 ? nd.w / 2.0 : side == 3 ? -nd.w / 2.0 : offset);
            double py = nd.y + (side == 2 ? nd.h / 2.0 : side == 0 ? -nd.h / 2.0 : offset);
            if (list[k].isSrc) {
                srcPt[list[k].edgeIdx] = {px, py};
                srcSide[list[k].edgeIdx] = side;
            } else {
                dstPt[list[k].edgeIdx] = {px, py};
                dstSide[list[k].edgeIdx] = side;
            }
        }
    }

    // Group coincident twin edges that share BOTH start and end points (layered.ts `twins`)
    struct CoincidentKey {
        long long sx, sy, ex, ey;
        bool operator<(const CoincidentKey& o) const {
            return std::tie(sx, sy, ex, ey) < std::tie(o.sx, o.sy, o.ex, o.ey);
        }
    };
    std::map<CoincidentKey, int> twinCount, twinSeen;
    for (size_t ei = 0; ei < edges.size(); ei++) {
        const auto& e = edges[ei];
        if (e.src < 0 || e.src >= N || e.dst < 0 || e.dst >= N || e.src == e.dst) continue;
        auto [sx, sy] = srcPt[ei];
        auto [ex, ey] = dstPt[ei];
        CoincidentKey ck{std::llround(sx), std::llround(sy), std::llround(ex), std::llround(ey)};
        twinCount[ck]++;
    }

    std::map<int, int> selfLoopSeen;
    double extraMinX = 0.0, extraMinY = 0.0, extraMaxX = res.width, extraMaxY = res.height;
    std::vector<double> edgeMidCross(edges.size(), 0.0);
    std::vector<double> edgeMidAlong(edges.size(), 0.0);
    std::vector<bool> edgeHasBend(edges.size(), false);
    int skipEdgeCounter = 0;

    for (size_t ei = 0; ei < edges.size(); ei++) {
        auto& e = edges[ei];
        if (e.src < 0 || e.src >= N || e.dst < 0 || e.dst >= N || e.src == e.dst) continue;
        auto [sx, sy] = srcPt[ei];
        auto [ex, ey] = dstPt[ei];
        CoincidentKey ck{std::llround(sx), std::llround(sy), std::llround(ex), std::llround(ey)};
        int total = twinCount[ck];
        int idx = twinSeen[ck]++;
        double twinShift = (total > 1) ? ((idx - (total - 1) / 2.0) * 16.0) : 0.0;

        int ss = srcSide[ei];
        size_t srcDeg = sidePorts[{e.src, srcSide[ei]}].size();
        size_t dstDeg = sidePorts[{e.dst, dstSide[ei]}].size();

        if (ss == 0 || ss == 2) {
            double my = (sy + ey) / 2.0;
            double mx = (sx + ex) / 2.0 + twinShift;
            if (e.labelW > 0.0 && std::abs(twinShift) < 0.1) {
                if (dstDeg < srcDeg) mx = ex;
                else if (srcDeg < dstDeg) mx = sx;
            }
            // Check if multi-rank skip edge crosses an intermediate node
            int rMin = std::min(rank[e.src], rank[e.dst]);
            int rMax = std::max(rank[e.src], rank[e.dst]);
            if (rMax - rMin > 1) {
                for (int r = rMin + 1; r < rMax; r++) {
                    for (int midNode : layers[r]) {
                        double lineX = sx + (ex - sx) * ((nodes[midNode].y - sy) / ((ey - sy) != 0.0 ? (ey - sy) : 1.0));
                        if (std::abs(lineX - nodes[midNode].x) < (nodes[midNode].w / 2.0 + 12.0)) {
                            double sideSign = (skipEdgeCounter % 2 == 0) ? 1.0 : -1.0;
                            int sideSlot = skipEdgeCounter / 2;
                            skipEdgeCounter++;
                            mx = nodes[midNode].x + sideSign * (nodes[midNode].w / 2.0 + 20.0 + sideSlot * 16.0);
                            edgeHasBend[ei] = true;
                            break;
                        }
                    }
                }
            }
            if (std::abs(twinShift) >= 0.1 || (e.labelW > 0.0 && std::abs(mx - (sx + ex) / 2.0) > 0.5)) {
                edgeHasBend[ei] = true;
            }
            edgeMidCross[ei] = mx;
            edgeMidAlong[ei] = my;
        } else {
            double mx = (sx + ex) / 2.0;
            double my = (sy + ey) / 2.0 + twinShift;
            if (e.labelH > 0.0 && std::abs(twinShift) < 0.1) {
                if (dstDeg < srcDeg) my = ey;
                else if (srcDeg < dstDeg) my = sy;
            }
            int rMin = std::min(rank[e.src], rank[e.dst]);
            int rMax = std::max(rank[e.src], rank[e.dst]);
            if (rMax - rMin > 1) {
                for (int r = rMin + 1; r < rMax; r++) {
                    for (int midNode : layers[r]) {
                        double lineY = sy + (ey - sy) * ((nodes[midNode].x - sx) / ((ex - sx) != 0.0 ? (ex - sx) : 1.0));
                        if (std::abs(lineY - nodes[midNode].y) < (nodes[midNode].h / 2.0 + 12.0)) {
                            double sideSign = (skipEdgeCounter % 2 == 0) ? 1.0 : -1.0;
                            int sideSlot = skipEdgeCounter / 2;
                            skipEdgeCounter++;
                            my = nodes[midNode].y + sideSign * (nodes[midNode].h / 2.0 + 20.0 + sideSlot * 16.0);
                            edgeHasBend[ei] = true;
                            break;
                        }
                    }
                }
            }
            if (std::abs(twinShift) >= 0.1 || (e.labelH > 0.0 && std::abs(my - (sy + ey) / 2.0) > 0.5)) {
                edgeHasBend[ei] = true;
            }
            edgeMidCross[ei] = my;
            edgeMidAlong[ei] = mx;
        }
    }

    // Spread edge labels in the same rank gap so parallel/adjacent edge labels never overlap
    std::map<long long, std::vector<size_t>> labelBands;
    for (size_t ei = 0; ei < edges.size(); ei++) {
        const auto& e = edges[ei];
        if (e.src < 0 || e.src >= N || e.dst < 0 || e.dst >= N || e.src == e.dst) continue;
        if (e.labelW > 0.0 && e.labelH > 0.0) {
            long long bandKey = std::llround(edgeMidAlong[ei] / 12.0);
            labelBands[bandKey].push_back(ei);
        }
    }
    for (auto& kv : labelBands) {
        auto& idxs = kv.second;
        if (idxs.size() < 2) continue;
        std::stable_sort(idxs.begin(), idxs.end(), [&](size_t a, size_t b) {
            return edgeMidCross[a] < edgeMidCross[b];
        });
        bool anyOverlap = false;
        for (size_t k = 1; k < idxs.size(); k++) {
            size_t a = idxs[k - 1], b = idxs[k];
            bool vert = (srcSide[a] == 0 || srcSide[a] == 2);
            double minGap = (vert ? (edges[a].labelW + edges[b].labelW) : (edges[a].labelH + edges[b].labelH)) / 2.0 + 16.0;
            if (edgeMidCross[b] - edgeMidCross[a] < minGap) anyOverlap = true;
        }
        if (anyOverlap) {
            double centerSum = 0.0;
            for (size_t idx : idxs) centerSum += edgeMidCross[idx];
            double center = centerSum / double(idxs.size());
            std::vector<double> gaps(idxs.size(), 0.0);
            double totalSpan = 0.0;
            for (size_t k = 1; k < idxs.size(); k++) {
                size_t a = idxs[k - 1], b = idxs[k];
                bool vert = (srcSide[a] == 0 || srcSide[a] == 2);
                double g = std::max(edgeMidCross[b] - edgeMidCross[a],
                                    (vert ? (edges[a].labelW + edges[b].labelW) : (edges[a].labelH + edges[b].labelH)) / 2.0 + 16.0);
                gaps[k] = g;
                totalSpan += g;
            }
            double cur = center - totalSpan / 2.0;
            for (size_t k = 0; k < idxs.size(); k++) {
                if (k > 0) cur += gaps[k];
                edgeMidCross[idxs[k]] = cur;
                edgeHasBend[idxs[k]] = true;
            }
        }
    }

    for (size_t ei = 0; ei < edges.size(); ei++) {
        auto& e = edges[ei];
        if (e.src < 0 || e.src >= N || e.dst < 0 || e.dst >= N) continue;
        const auto& s = nodes[e.src];
        if (e.src == e.dst) {
            int k = selfLoopSeen[e.src]++;
            double reach = 26.0 + k * 10.0;
            double sx = s.x + s.w / 2.0;
            double sy = s.y - 6.0;
            double ex = s.x + s.w / 2.0;
            double ey = s.y + 6.0;
            e.route = {{sx, sy}, {sx + reach, sy - 10.0}, {ex + reach, ey + 10.0}, {ex, ey}};
            e.labelX = sx + reach + 6.0 + e.labelW / 2.0;
            e.labelY = s.y;
            extraMaxX = std::max(extraMaxX, e.labelX + e.labelW / 2.0);
        } else {
            auto [sx, sy] = srcPt[ei];
            auto [ex, ey] = dstPt[ei];
            int ss = srcSide[ei];
            if (ss == 0 || ss == 2) {
                double mx = edgeMidCross[ei];
                double my = edgeMidAlong[ei];
                if (!edgeHasBend[ei]) {
                    e.route = {{sx, sy}, {ex, ey}};
                } else {
                    e.route = {{sx, sy}, {mx, my}, {ex, ey}};
                    double hw = std::max(8.0, e.labelW / 2.0 + 4.0);
                    extraMinX = std::min(extraMinX, mx - hw);
                    extraMaxX = std::max(extraMaxX, mx + hw);
                }
                e.labelX = mx;
                e.labelY = my;
            } else {
                double mx = edgeMidAlong[ei];
                double my = edgeMidCross[ei];
                if (!edgeHasBend[ei]) {
                    e.route = {{sx, sy}, {ex, ey}};
                } else {
                    e.route = {{sx, sy}, {mx, my}, {ex, ey}};
                    double hh = std::max(8.0, e.labelH / 2.0 + 4.0);
                    extraMinY = std::min(extraMinY, my - hh);
                    extraMaxY = std::max(extraMaxY, my + hh);
                }
                e.labelX = mx;
                e.labelY = my;
            }
        }
    }
    if (extraMinX < 0.0 || extraMinY < 0.0) {
        double shiftX = (extraMinX < 0.0) ? -extraMinX : 0.0;
        double shiftY = (extraMinY < 0.0) ? -extraMinY : 0.0;
        for (int i = 0; i < N; i++) {
            nodes[i].x += shiftX;
            nodes[i].y += shiftY;
        }
        for (auto& e : edges) {
            for (auto& pt : e.route) {
                pt.first += shiftX;
                pt.second += shiftY;
            }
            e.labelX += shiftX;
            e.labelY += shiftY;
        }
        extraMaxX += shiftX;
        extraMaxY += shiftY;
    }
    res.width = std::max(res.width, extraMaxX);
    res.height = std::max(res.height, extraMaxY);
    return res;
}

inline LayoutResult layoutGraph(
    std::vector<LayoutNode>& nodes,
    std::vector<LayoutEdge>& edges,
    std::vector<LayoutSubgraph>& subgraphs,
    const std::string& direction,
    double nodeSep = 40.0,
    double rankSep = 48.0
) {
    int nCount = static_cast<int>(nodes.size());
    int sgCount = static_cast<int>(subgraphs.size());
    std::vector<LayoutNode> allNodes = nodes;
    allNodes.resize(nCount + sgCount);
    for (int i = 0; i < nCount; i++) {
        allNodes[i].parent = (nodes[i].parentSubgraph >= 0) ? (nCount + nodes[i].parentSubgraph) : -1;
    }
    for (int s = 0; s < sgCount; s++) {
        auto& gn = allNodes[nCount + s];
        gn.id = subgraphs[s].id;
        gn.isGroup = true;
        gn.parent = (subgraphs[s].parentSubgraph >= 0) ? (nCount + subgraphs[s].parentSubgraph) : -1;
        gn.minW = std::max(80.0, measureTextWidth(subgraphs[s].title, 15.0, true, false) + 32.0);
    }
    LayoutConfig cfg;
    cfg.dir = direction;
    cfg.nodeSep = nodeSep;
    cfg.rankSep = rankSep;
    LayoutResult res = layoutGraph(allNodes, edges, cfg);
    for (int i = 0; i < nCount; i++) {
        nodes[i].x = allNodes[i].x;
        nodes[i].y = allNodes[i].y;
    }
    for (int s = 0; s < sgCount; s++) {
        const auto& gn = allNodes[nCount + s];
        subgraphs[s].w = gn.w;
        subgraphs[s].h = gn.h;
        subgraphs[s].x = gn.x - gn.w / 2.0;
        subgraphs[s].y = gn.y - gn.h / 2.0;
    }
    return res;
}

// ---------------------------------------------------------------------------
// 7. Flowchart / Swimlane / Agentflow Parser & Renderer
// ---------------------------------------------------------------------------

struct FlowEdgeSpec {
    std::string stroke = "solid";
    std::string arrowStart = "none";
    std::string arrowEnd = "arrow_point";
    std::string label;
};

inline bool matchEdgeOperator(const std::string& s, size_t pos, size_t& endPos, FlowEdgeSpec& spec) {
    // Wait: check if inside quotes or brackets before calling matchEdgeOperator
    static const struct OpRule {
        const char* pat;
        const char* stroke;
        const char* aStart;
        const char* aEnd;
    } ops[] = {
        {"<==>", "thick", "arrow_point", "arrow_point"},
        {"<-.->", "dotted", "arrow_point", "arrow_point"},
        {"<-->", "solid", "arrow_point", "arrow_point"},
        {"o--o", "solid", "arrow_circle", "arrow_circle"},
        {"x--x", "solid", "arrow_cross", "arrow_cross"},
        {"-.->", "dotted", "none", "arrow_point"},
        {"-.-o", "dotted", "none", "arrow_circle"},
        {"-.-x", "dotted", "none", "arrow_cross"},
        {"-.-", "dotted", "none", "none"},
        {"===>", "thick", "none", "arrow_point"},
        {"==>", "thick", "none", "arrow_point"},
        {"===", "thick", "none", "none"},
        {"~~~", "invisible", "none", "none"},
        {"--->", "solid", "none", "arrow_point"},
        {"-->", "solid", "none", "arrow_point"},
        {"--o", "solid", "none", "arrow_circle"},
        {"--x", "solid", "none", "arrow_cross"},
        {"<--", "solid", "arrow_point", "none"},
        {"---", "solid", "none", "none"},
    };
    // Check inline label syntax: `-- text -->`, `-. text .->`, `== text ==>`
    if (s.compare(pos, 2, "--") == 0 && pos + 2 < s.size() && s[pos + 2] != '-' && s[pos + 2] != '>' && s[pos + 2] != 'o' && s[pos + 2] != 'x') {
        for (const char* closePat : {"-->", "--o", "--x", "---"}) {
            size_t cp = s.find(closePat, pos + 2);
            if (cp != std::string::npos) {
                std::string mid = trim(s.substr(pos + 2, cp - (pos + 2)));
                if (!mid.empty() && mid.find('[') == std::string::npos && mid.find('(') == std::string::npos) {
                    spec.stroke = "solid";
                    spec.arrowStart = "none";
                    spec.arrowEnd = ( std::string(closePat) == "-->" ? "arrow_point" : std::string(closePat) == "--o" ? "arrow_circle" : std::string(closePat) == "--x" ? "arrow_cross" : "none" );
                    spec.label = unquote(mid);
                    endPos = cp + 3;
                    return true;
                }
            }
        }
    }
    if (s.compare(pos, 2, "-.") == 0 && pos + 2 < s.size() && s[pos + 2] != '-') {
        size_t cp = s.find(".->", pos + 2);
        if (cp != std::string::npos) {
            std::string mid = trim(s.substr(pos + 2, cp - (pos + 2)));
            if (!mid.empty()) {
                spec.stroke = "dotted";
                spec.arrowStart = "none";
                spec.arrowEnd = "arrow_point";
                spec.label = unquote(mid);
                endPos = cp + 3;
                return true;
            }
        }
    }
    if (s.compare(pos, 2, "==") == 0 && pos + 2 < s.size() && s[pos + 2] != '=' && s[pos + 2] != '>') {
        size_t cp = s.find("==>", pos + 2);
        if (cp != std::string::npos) {
            std::string mid = trim(s.substr(pos + 2, cp - (pos + 2)));
            if (!mid.empty()) {
                spec.stroke = "thick";
                spec.arrowStart = "none";
                spec.arrowEnd = "arrow_point";
                spec.label = unquote(mid);
                endPos = cp + 3;
                return true;
            }
        }
    }
    auto findCloseBar = [&](size_t startBar) -> size_t {
        bool inMath = false;
        for (size_t k = startBar + 1; k < s.size(); k++) {
            if (k + 1 < s.size() && s[k] == '$' && s[k + 1] == '$') {
                inMath = !inMath;
                k++;
                continue;
            }
            if (!inMath && s[k] == '|') return k;
        }
        return s.find('|', startBar + 1);
    };
    for (const auto& op : ops) {
        size_t len = std::strlen(op.pat);
        if (s.compare(pos, len, op.pat) == 0) {
            // Consume extra dashes/equals
            size_t ep = pos + len;
            spec.stroke = op.stroke;
            spec.arrowStart = op.aStart;
            spec.arrowEnd = op.aEnd;
            spec.label.clear();
            // Check for `|label|` right after operator
            size_t ws = ep;
            while (ws < s.size() && (s[ws] == ' ' || s[ws] == '\t')) ws++;
            if (ws < s.size() && s[ws] == '|') {
                size_t closeBar = findCloseBar(ws);
                if (closeBar != std::string::npos) {
                    spec.label = unquote(trim(s.substr(ws + 1, closeBar - (ws + 1))));
                    ep = closeBar + 1;
                }
            }
            endPos = ep;
            return true;
        }
    }
    // Variable-length arrows: `---->`, `----`, `====>`, `====`, `-.->`, `-..->`, `~~~~`
    if (s.compare(pos, 2, "--") == 0 || s.compare(pos, 2, "==") == 0 || s.compare(pos, 3, "~~~") == 0) {
        char ch = s[pos];
        size_t ep = pos;
        while (ep < s.size() && s[ep] == ch) ep++;
        if (ep - pos >= 2) {
            spec.stroke = (ch == '=') ? "thick" : (ch == '~') ? "invisible" : "solid";
            spec.arrowStart = "none";
            spec.arrowEnd = "none";
            spec.label.clear();
            if (ep < s.size() && s[ep] == '>') { spec.arrowEnd = "arrow_point"; ep++; }
            else if (ep < s.size() && s[ep] == 'o') { spec.arrowEnd = "arrow_circle"; ep++; }
            else if (ep < s.size() && s[ep] == 'x') { spec.arrowEnd = "arrow_cross"; ep++; }
            else if (ep - pos < 3) return false;
            size_t ws = ep;
            while (ws < s.size() && (s[ws] == ' ' || s[ws] == '\t')) ws++;
            if (ws < s.size() && s[ws] == '|') {
                size_t closeBar = findCloseBar(ws);
                if (closeBar != std::string::npos) {
                    spec.label = unquote(trim(s.substr(ws + 1, closeBar - (ws + 1))));
                    ep = closeBar + 1;
                }
            }
            endPos = ep;
            return true;
        }
    }
    if (s.compare(pos, 2, "-.") == 0) {
        size_t ep = pos + 1;
        while (ep < s.size() && s[ep] == '.') ep++;
        if (ep < s.size() && s[ep] == '-') {
            ep++;
            spec.stroke = "dotted";
            spec.arrowStart = "none";
            spec.arrowEnd = "none";
            spec.label.clear();
            if (ep < s.size() && s[ep] == '>') { spec.arrowEnd = "arrow_point"; ep++; }
            size_t ws = ep;
            while (ws < s.size() && (s[ws] == ' ' || s[ws] == '\t')) ws++;
            if (ws < s.size() && s[ws] == '|') {
                size_t closeBar = findCloseBar(ws);
                if (closeBar != std::string::npos) {
                    spec.label = unquote(trim(s.substr(ws + 1, closeBar - (ws + 1))));
                    ep = closeBar + 1;
                }
            }
            endPos = ep;
            return true;
        }
    }
    return false;
}

inline void parseFlowNodeToken(
    const std::string& rawTok,
    std::string& outId,
    std::string& outLabel,
    std::string& outShape,
    std::string& outClass,
    bool& hasExplicitDef
) {
    hasExplicitDef = false;
    std::string t = trim(rawTok);
    outClass.clear();
    // Strip `:::className` at end
    size_t tripleColon = t.rfind(":::");
    if (tripleColon != std::string::npos) {
        outClass = trim(t.substr(tripleColon + 3));
        t = trim(t.substr(0, tripleColon));
    }
    // Check `id@{ ... }` or `id[label]@{ ... }` syntax
    size_t atBrace = t.find("@{");
    if (atBrace != std::string::npos && t.back() == '}') {
        std::string head = trim(t.substr(0, atBrace));
        std::string body = t.substr(atBrace + 2, t.size() - atBrace - 3);
        std::string dummyCls;
        bool headHasDef = false;
        parseFlowNodeToken(head, outId, outLabel, outShape, dummyCls, headHasDef);
        hasExplicitDef = true;
        for (const auto& kv : splitChar(body, ',')) {
            size_t col = kv.find(':');
            if (col != std::string::npos) {
                std::string k = toLower(trim(kv.substr(0, col)));
                std::string v = unquote(trim(kv.substr(col + 1)));
                if (k == "shape") outShape = canonicalShape(v);
                else if (k == "label") outLabel = v;
            }
        }
        return;
    }

    // Find first bracket/paren/brace/angle outside quotes
    size_t openPos = std::string::npos;
    for (size_t i = 0; i < t.size(); i++) {
        char c = t[i];
        if (c == '[' || c == '(' || c == '{' || c == '>') {
            openPos = i;
            break;
        }
    }
    if (openPos == std::string::npos || openPos == 0) {
        outId = unquote(t);
        outLabel = outId;
        outShape = "rect";
        return;
    }
    outId = trim(t.substr(0, openPos));
    std::string rest = trim(t.substr(openPos));
    hasExplicitDef = true;

    struct Delim {
        const char* open;
        const char* close;
        const char* shape;
    };
    static const Delim delims[] = {
        {"(((", ")))", "dbl-circ"},
        {"([", "])", "stadium"},
        {"[[", "]]", "fr-rect"},
        {"[(", ")]", "cyl"},
        {"((", "))", "circle"},
        {"{{", "}}", "hex"},
        {"[/", "/]", "lean-r"},
        {"[\\", "\\]", "lean-l"},
        {"[/", "\\]", "trap-b"},
        {"[\\", "/]", "trap-t"},
        {">", "]", "odd"},
        {"[", "]", "rect"},
        {"(", ")", "rounded"},
        {"{", "}", "diam"},
    };
    for (const auto& d : delims) {
        size_t oLen = std::strlen(d.open);
        size_t cLen = std::strlen(d.close);
        if (rest.size() >= oLen + cLen &&
            rest.compare(0, oLen, d.open) == 0 &&
            rest.compare(rest.size() - cLen, cLen, d.close) == 0) {
            outShape = d.shape;
            outLabel = unquote(trim(rest.substr(oLen, rest.size() - oLen - cLen)));
            return;
        }
    }
    outShape = "rect";
    outLabel = unquote(rest);
}

inline RenderResult renderFlowchart(const std::string& code, const std::string& diagramType, const RenderOptions& opts) {
    bool isSwimlane = (diagramType == "swimlane");
    bool isAgentflow = (diagramType == "agentflow");
    double fontSize = opts.fontSize;
    double edgeFontSize = std::round(fontSize * 0.85);

    std::string direction = "TD";
    std::vector<LayoutNode> nodes;
    std::map<std::string, int> nodeMap;
    std::vector<LayoutEdge> edges;
    std::vector<LayoutSubgraph> subgraphs;
    std::map<std::string, int> sgMap;
    std::vector<int> sgStack;
    std::map<std::string, std::map<std::string, std::string>> classDefs;
    std::map<std::string, std::vector<std::string>> nodeClasses;
    std::map<std::string, int> agentKindSlot;

    auto ensureNode = [&](const std::string& id) -> int {
        auto it = nodeMap.find(id);
        if (it != nodeMap.end()) return it->second;
        int idx = static_cast<int>(nodes.size());
        LayoutNode n;
        n.id = id;
        n.label = id;
        n.shape = isAgentflow ? "rounded" : "rect";
        if (!sgStack.empty()) {
            n.parentSubgraph = sgStack.back();
            subgraphs[sgStack.back()].nodeIndices.push_back(idx);
        }
        nodes.push_back(n);
        nodeMap[id] = idx;
        return idx;
    };

    auto rawLines = splitLines(code);
    std::vector<std::string> joinedLines;
    {
        std::string accum;
        int bDepth = 0;
        char qChar = 0;
        bool inMath = false;
        for (const auto& rl : rawLines) {
            std::string t = trim(rl);
            if (t.empty() && accum.empty()) continue;
            if (!accum.empty()) accum += "\n" + t;
            else accum = t;
            for (size_t ci = 0; ci < t.size(); ci++) {
                char c = t[ci];
                if (qChar) { if (c == qChar) qChar = 0; }
                else if (ci + 1 < t.size() && c == '$' && t[ci + 1] == '$') { inMath = !inMath; ci++; }
                else if (!inMath) {
                    if (c == '"' || c == '\'') qChar = c;
                    else if (c == '[' || c == '(' || c == '{') bDepth++;
                    else if (c == ']' || c == ')' || c == '}') bDepth = std::max(0, bDepth - 1);
                }
            }
            if (qChar == 0 && !inMath && bDepth == 0) {
                joinedLines.push_back(accum);
                accum.clear();
            }
        }
        if (!accum.empty()) joinedLines.push_back(accum);
    }
    std::vector<std::string> statements;
    for (const auto& t : joinedLines) {
        if (t.empty()) continue;
        // Split by `;` outside quotes/brackets/math
        int depth = 0;
        char q = 0;
        bool inMath = false;
        size_t start = 0;
        for (size_t i = 0; i < t.size(); i++) {
            char c = t[i];
            if (q) {
                if (c == q) q = 0;
            } else if (i + 1 < t.size() && c == '$' && t[i + 1] == '$') {
                inMath = !inMath;
                i++;
            } else if (!inMath) {
                if (c == '"' || c == '\'') q = c;
                else if (c == '[' || c == '(' || c == '{') depth++;
                else if (c == ']' || c == ')' || c == '}') depth = std::max(0, depth - 1);
                else if (c == ';' && depth == 0) {
                    std::string sub = trim(t.substr(start, i - start));
                    if (!sub.empty()) statements.push_back(sub);
                    start = i + 1;
                }
            }
        }
        std::string tail = trim(t.substr(start));
        if (!tail.empty()) statements.push_back(tail);
    }

    std::map<std::string, bool> edgeIds;
    bool headerSeen = false;
    for (const auto& rawStmt : statements) {
        std::string stmt = rawStmt;
        // Strip edge IDs like `e1@-->` or `e1@==>`
        for (size_t atP = stmt.find('@'); atP != std::string::npos; atP = stmt.find('@', atP + 1)) {
            if (atP + 1 < stmt.size() && (stmt[atP + 1] == '-' || stmt[atP + 1] == '=' || stmt[atP + 1] == '<' || stmt[atP + 1] == '~')) {
                size_t idSt = atP;
                while (idSt > 0 && (std::isalnum(static_cast<unsigned char>(stmt[idSt - 1])) || stmt[idSt - 1] == '_' || stmt[idSt - 1] == '-')) idSt--;
                if (idSt < atP && (idSt == 0 || stmt[idSt - 1] == ' ' || stmt[idSt - 1] == '\t')) {
                    edgeIds[stmt.substr(idSt, atP - idSt)] = true;
                    stmt.erase(idSt, atP - idSt + 1);
                    atP = idSt;
                }
            }
        }
        std::string low = toLower(stmt);
        if (low.rfind("acctitle", 0) == 0 || low.rfind("accdescr", 0) == 0) continue;
        size_t atBraceStmt = stmt.find("@{");
        if (atBraceStmt != std::string::npos) {
            std::string maybeEdgeId = trim(stmt.substr(0, atBraceStmt));
            if (edgeIds.count(maybeEdgeId)) continue;
        }
        if (!headerSeen) {
            headerSeen = true;
            std::istringstream iss(stmt);
            std::string kw, dir;
            iss >> kw >> dir;
            std::string lkw = toLower(kw);
            if (lkw == "flowchart" || lkw == "graph" || lkw == "flowchart-elk" || lkw == "swimlane" || lkw == "swimlane-beta" || lkw == "agentflow" || lkw == "agentflow-beta") {
                std::string udir = toLower(dir);
                if (udir == "lr" || udir == "rl" || udir == "bt" || udir == "tb" || udir == "td") {
                    for (char& c : udir) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                    direction = udir;
                }
                continue;
            }
        }
        if (low.rfind("direction ", 0) == 0) {
            std::string dir = trim(stmt.substr(10));
            for (char& c : dir) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            if (!sgStack.empty()) subgraphs[sgStack.back()].direction = dir;
            else direction = dir;
            continue;
        }
        if (low == "end") {
            if (!sgStack.empty()) sgStack.pop_back();
            continue;
        }
        if (low.rfind("subgraph ", 0) == 0 || low.rfind("lane ", 0) == 0 || (isAgentflow && low.rfind("flow ", 0) == 0)) {
            size_t pLen = (low.rfind("lane ", 0) == 0 || low.rfind("flow ", 0) == 0) ? 5 : 9;
            std::string rest = trim(stmt.substr(pLen));
            std::string sgId = rest;
            std::string sgTitle = rest;
            size_t ob = rest.find('[');
            if (ob != std::string::npos && rest.back() == ']') {
                sgId = trim(rest.substr(0, ob));
                sgTitle = unquote(trim(rest.substr(ob + 1, rest.size() - ob - 2)));
            } else {
                sgTitle = unquote(rest);
                sgId = sgTitle;
            }
            int sgIdx = static_cast<int>(subgraphs.size());
            LayoutSubgraph sg;
            sg.id = sgId;
            sg.title = sgTitle;
            if (!sgStack.empty()) {
                sg.parentSubgraph = sgStack.back();
                subgraphs[sgStack.back()].childSubgraphs.push_back(sgIdx);
            }
            subgraphs.push_back(sg);
            sgMap[sgId] = sgIdx;
            sgStack.push_back(sgIdx);
            continue;
        }
        if (low.rfind("classdef ", 0) == 0) {
            std::string rest = trim(stmt.substr(9));
            size_t sp = rest.find_first_of(" \t");
            if (sp != std::string::npos) {
                std::string cnames = trim(rest.substr(0, sp));
                std::string props = trim(rest.substr(sp + 1));
                std::map<std::string, std::string> pmap;
                for (const auto& kv : splitChar(props, ',')) {
                    size_t col = kv.find(':');
                    if (col != std::string::npos) {
                        pmap[trim(kv.substr(0, col))] = trim(kv.substr(col + 1));
                    }
                }
                for (const auto& cn : splitChar(cnames, ',')) {
                    classDefs[trim(cn)] = pmap;
                }
            }
            continue;
        }
        if (low.rfind("class ", 0) == 0) {
            std::string rest = trim(stmt.substr(6));
            size_t sp = rest.find_last_of(" \t");
            if (sp != std::string::npos) {
                std::string nlist = trim(rest.substr(0, sp));
                std::string cname = trim(rest.substr(sp + 1));
                for (const auto& nid : splitChar(nlist, ',')) {
                    nodeClasses[trim(nid)].push_back(cname);
                }
            }
            continue;
        }
        if (low.rfind("style ", 0) == 0) {
            std::string rest = trim(stmt.substr(6));
            size_t sp = rest.find_first_of(" \t");
            if (sp != std::string::npos) {
                std::string nlist = trim(rest.substr(0, sp));
                std::string props = trim(rest.substr(sp + 1));
                for (const auto& nid : splitChar(nlist, ',')) {
                    int idx = ensureNode(trim(nid));
                    for (const auto& kv : splitChar(props, ',')) {
                        size_t col = kv.find(':');
                        if (col != std::string::npos) {
                            nodes[idx].style[trim(kv.substr(0, col))] = trim(kv.substr(col + 1));
                        }
                    }
                }
            }
            continue;
        }
        if (low.rfind("linkstyle ", 0) == 0 || low.rfind("click ", 0) == 0) continue;

        // Agentflow node declarations: `agent id "Label"`, `tool id "Label"`, `llm id "Label"`, `human id "Label"`, `router id "Label"`, `memory id "Label"`
        if (isAgentflow) {
            std::istringstream iss(stmt);
            std::string kw;
            iss >> kw;
            std::string lkw = toLower(kw);
            int slot = -1;
            std::string afShape = "rounded";
            if (lkw == "agent") { slot = 0; afShape = "rounded"; }
            else if (lkw == "llm" || lkw == "model") { slot = 1; afShape = "fr-rect"; }
            else if (lkw == "tool" || lkw == "action") { slot = 2; afShape = "hex"; }
            else if (lkw == "memory" || lkw == "store" || lkw == "db") { slot = 3; afShape = "cyl"; }
            else if (lkw == "human" || lkw == "user") { slot = 4; afShape = "stadium"; }
            else if (lkw == "router" || lkw == "guard" || lkw == "condition") { slot = 5; afShape = "diam"; }
            if (slot >= 0) {
                std::string rest = trim(stmt.substr(kw.size()));
                // Only treat as declaration if no edge operator is present outside quotes
                if (rest.find("-->") == std::string::npos && rest.find("-.->") == std::string::npos) {
                    std::string nid, nlbl, nsh, ncls;
                    bool hasDef = false;
                    size_t sp = rest.find_first_of(" \t[({");
                    if (sp != std::string::npos && (rest[sp] == ' ' || rest[sp] == '\t')) {
                        nid = trim(rest.substr(0, sp));
                        nlbl = unquote(trim(rest.substr(sp + 1)));
                        nsh = afShape;
                    } else {
                        parseFlowNodeToken(rest, nid, nlbl, nsh, ncls, hasDef);
                        if (!hasDef) nsh = afShape;
                    }
                    int idx = ensureNode(nid);
                    nodes[idx].label = nlbl;
                    nodes[idx].shape = nsh;
                    agentKindSlot[nid] = slot;
                    continue;
                }
            }
        }

        // Tokenize chain of node groups separated by edge operators outside brackets/quotes/math
        std::vector<std::string> segments;
        std::vector<FlowEdgeSpec> edgeSpecs;
        int depth = 0;
        char q = 0;
        bool inMath = false;
        size_t segStart = 0;
        for (size_t i = 0; i < stmt.size(); ) {
            char c = stmt[i];
            if (q) {
                if (c == q) q = 0;
                i++;
                continue;
            }
            if (i + 1 < stmt.size() && c == '$' && stmt[i + 1] == '$') {
                inMath = !inMath;
                i += 2;
                continue;
            }
            if (inMath) {
                i++;
                continue;
            }
            if (c == '"' || c == '\'') { q = c; i++; continue; }
            if (c == '[' || c == '(' || c == '{') { depth++; i++; continue; }
            if (c == ']' || c == ')' || c == '}') { depth = std::max(0, depth - 1); i++; continue; }
            if (depth == 0) {
                size_t opEnd = i;
                FlowEdgeSpec spec;
                if (matchEdgeOperator(stmt, i, opEnd, spec)) {
                    segments.push_back(trim(stmt.substr(segStart, i - segStart)));
                    edgeSpecs.push_back(spec);
                    segStart = opEnd;
                    i = opEnd;
                    continue;
                }
            }
            i++;
        }
        segments.push_back(trim(stmt.substr(segStart)));
        if (q != 0 || depth != 0) return RenderResult{};
        for (const auto& sg : segments) {
            if (sg.empty()) return RenderResult{};
        }

        bool badToken = false;
        auto resolveSegmentGroup = [&](const std::string& seg) -> std::vector<int> {
            std::vector<int> group;
            // Split by `&` outside brackets/quotes/math
            int d = 0;
            char qc = 0;
            bool inM = false;
            size_t st = 0;
            std::vector<std::string> parts;
            for (size_t i = 0; i < seg.size(); i++) {
                char c = seg[i];
                if (qc) { if (c == qc) qc = 0; }
                else if (i + 1 < seg.size() && c == '$' && seg[i + 1] == '$') { inM = !inM; i++; }
                else if (!inM) {
                    if (c == '"' || c == '\'') qc = c;
                    else if (c == '[' || c == '(' || c == '{') d++;
                    else if (c == ']' || c == ')' || c == '}') d = std::max(0, d - 1);
                    else if (c == '&' && d == 0) {
                        parts.push_back(trim(seg.substr(st, i - st)));
                        st = i + 1;
                    }
                }
            }
            parts.push_back(trim(seg.substr(st)));
            for (const auto& pt : parts) {
                if (pt.empty()) { badToken = true; continue; }
                std::string nid, nlbl, nsh, ncls;
                bool hasDef = false;
                parseFlowNodeToken(pt, nid, nlbl, nsh, ncls, hasDef);
                if (nid.empty()) { badToken = true; continue; }
                if (!hasDef && (nid.find_first_of(" \t|<>=") != std::string::npos || nid.back() == '-')) {
                    badToken = true;
                    continue;
                }
                // Check if nid refers to a subgraph
                int idx = ensureNode(nid);
                if (hasDef) {
                    nodes[idx].label = nlbl;
                    nodes[idx].shape = nsh;
                }
                if (!ncls.empty()) nodeClasses[nid].push_back(ncls);
                group.push_back(idx);
            }
            return group;
        };

        if (segments.size() == 1) {
            resolveSegmentGroup(segments[0]);
            if (badToken) return RenderResult{};
        } else {
            std::vector<int> prevGroup = resolveSegmentGroup(segments[0]);
            if (badToken) return RenderResult{};
            for (size_t k = 0; k < edgeSpecs.size(); k++) {
                std::vector<int> nextGroup = resolveSegmentGroup(segments[k + 1]);
                if (badToken) return RenderResult{};
                const auto& sp = edgeSpecs[k];
                for (int u : prevGroup) {
                    for (int v : nextGroup) {
                        LayoutEdge le;
                        le.src = u;
                        le.dst = v;
                        le.label = sp.label;
                        le.stroke = sp.stroke;
                        le.arrowStart = sp.arrowStart;
                        le.arrowEnd = sp.arrowEnd;
                        if (!le.label.empty()) {
                            TextBlock tb = layoutLabel(le.label, edgeFontSize, false);
                            le.labelW = tb.width + 10.0;
                            le.labelH = tb.height + 6.0;
                        }
                        edges.push_back(le);
                    }
                }
                prevGroup = nextGroup;
            }
        }
    }

    if (nodes.empty() && subgraphs.empty()) return RenderResult{};

    // Apply classDefs to nodes and compute sizes
    std::map<int, TextBlock> nodeLabels;
    for (size_t i = 0; i < nodes.size(); i++) {
        auto& n = nodes[i];
        auto cit = nodeClasses.find(n.id);
        if (cit != nodeClasses.end()) {
            std::map<std::string, std::string> merged;
            for (const auto& cn : cit->second) {
                auto cdit = classDefs.find(cn);
                if (cdit != classDefs.end()) {
                    for (const auto& kv : cdit->second) merged[kv.first] = kv.second;
                }
            }
            for (const auto& kv : n.style) merged[kv.first] = kv.second;
            n.style = merged;
        }
        TextBlock tb = layoutLabel(n.label, fontSize, false, 220.0);
        nodeLabels[static_cast<int>(i)] = tb;
        ShapeSize sz = shapeSize(n.shape, tb.width, tb.height);
        n.w = sz.w;
        n.h = sz.h;
    }

    LayoutResult lres = layoutGraph(nodes, edges, subgraphs, direction);

    // For swimlane, equalize top-level lane heights/widths so lanes look like crisp adjacent swimlanes
    if (isSwimlane && !subgraphs.empty()) {
        double minY = 1e9, maxY = 0.0;
        for (const auto& sg : subgraphs) {
            if (sg.parentSubgraph < 0 && sg.w > 0 && sg.h > 0) {
                minY = std::min(minY, sg.y);
                maxY = std::max(maxY, sg.y + sg.h);
            }
        }
        if (maxY > minY) {
            for (auto& sg : subgraphs) {
                if (sg.parentSubgraph < 0 && sg.w > 0 && sg.h > 0) {
                    sg.y = minY;
                    sg.h = maxY - minY;
                }
            }
        }
    }

    std::string body;
    // 1. Subgraphs
    for (const auto& sg : subgraphs) {
        if (sg.w <= 0 || sg.h <= 0) continue;
        std::string cls = isSwimlane ? "pele-cluster pele-lane" : "pele-cluster";
        body += "<g class=\"" + cls + "\" data-id=\"" + esc(sg.id) + "\">"
                "<rect x=\"" + num(sg.x) + "\" y=\"" + num(sg.y) + "\" width=\"" + num(sg.w) + "\" height=\"" + num(sg.h) +
                "\" rx=\"var(--_r)\" fill=\"var(--_a)\" fill-opacity=\"0.55\" stroke=\"var(--_b)\" stroke-width=\"var(--_w)\"" +
                (isSwimlane ? "" : " stroke-dasharray=\"5 3\"") + "/>";
        if (isSwimlane) {
            body += "<line x1=\"" + num(sg.x) + "\" y1=\"" + num(sg.y + 30.0) + "\" x2=\"" + num(sg.x + sg.w) + "\" y2=\"" + num(sg.y + 30.0) +
                    "\" stroke=\"var(--_b)\" stroke-width=\"var(--_w)\"/>";
        }
        if (!sg.title.empty()) {
            TextBlock stb = layoutLabel(sg.title, fontSize * 0.92, true);
            body += labelSvg(sg.x + sg.w / 2.0, sg.y + 16.0, stb, fontSize * 0.92, "middle", "var(--_m)");
        }
        body += "</g>";
    }

    // 2. Edges
    int flowAxis = (direction == "LR" || direction == "RL") ? 1 : 0;
    for (const auto& e : edges) {
        if (e.stroke == "invisible" || e.route.size() < 2) continue;
        double startTrim = markerTrim(e.arrowStart);
        double endTrim = markerTrim(e.arrowEnd);
        std::string startMark, endMark, d;
        if (e.src == e.dst && e.route.size() == 4) {
            auto pts = e.route;
            if (e.arrowStart != "none") {
                double ang = std::atan2(pts[0].second - pts[1].second, pts[0].first - pts[1].first);
                startMark = marker(e.arrowStart, pts[0].first, pts[0].second, ang);
                pts[0].first -= std::cos(ang) * startTrim;
                pts[0].second -= std::sin(ang) * startTrim;
            }
            if (e.arrowEnd != "none") {
                size_t L = pts.size() - 1;
                double ang = std::atan2(pts[L].second - pts[L - 1].second, pts[L].first - pts[L - 1].first);
                endMark = marker(e.arrowEnd, pts[L].first, pts[L].second, ang);
                pts[L].first -= std::cos(ang) * endTrim;
                pts[L].second -= std::sin(ang) * endTrim;
            }
            d = "M" + num(pts[0].first) + "," + num(pts[0].second) +
                "C" + num(pts[1].first) + "," + num(pts[1].second) + " " +
                num(pts[2].first) + "," + num(pts[2].second) + " " +
                num(pts[3].first) + "," + num(pts[3].second);
        } else {
            EdgePath ep = routePoints(e.route, opts.curve, startTrim, endTrim, flowAxis);
            if (e.arrowStart != "none") {
                startMark = marker(e.arrowStart, ep.sx, ep.sy, ep.sdx, ep.sdy);
            }
            if (e.arrowEnd != "none") {
                endMark = marker(e.arrowEnd, ep.ex, ep.ey, ep.edx, ep.edy);
            }
            d = ep.d;
        }
        std::string dash = (e.stroke == "dotted") ? " stroke-dasharray=\"3 4\"" : "";
        std::string sw = (e.stroke == "thick") ? " stroke-width=\"2.5\"" : "";
        body += "<g class=\"pele-edge\">"
                "<path d=\"" + d + "\" fill=\"none\" stroke=\"var(--_l)\" stroke-linecap=\"round\"" + sw + dash + "/>" +
                startMark + endMark;
        if (!e.label.empty()) {
            TextBlock tb = layoutLabel(e.label, edgeFontSize, false);
            body += edgeLabelSvg(e.labelX, e.labelY, tb, edgeFontSize);
        }
        body += "</g>";
    }

    // 3. Nodes
    for (size_t i = 0; i < nodes.size(); i++) {
        const auto& n = nodes[i];
        ResolvedStyle rs = resolveStyle(n.style);
        if (isAgentflow && n.style.empty()) {
            int slot = agentKindSlot.count(n.id) ? agentKindSlot[n.id] : static_cast<int>(i % 6);
            std::string c = seriesColor(slot);
            rs.fill = c;
            rs.stroke = c;
            rs.extra = " fill-opacity=\"0.16\"";
        }
        body += "<g class=\"pele-node\" data-id=\"" + esc(n.id) + "\">" +
                drawShape(n.shape, n.x, n.y, n.w, n.h, rs) +
                labelSvg(n.x, n.y, nodeLabels[static_cast<int>(i)], fontSize, "middle", rs.textColor) +
                "</g>";
    }

    std::string outType = (diagramType == "graph" || diagramType == "flowchart-elk") ? "flowchart" : diagramType;
    RenderResult r;
    r.type = outType;
    r.width = std::ceil(lres.width);
    r.height = std::ceil(lres.height);
    r.svg = svgDocument(outType, r.width, r.height, fontSize, body);
    return r;
}

// ---------------------------------------------------------------------------
// 8. Sequence Diagram Parser & Renderer
// ---------------------------------------------------------------------------

inline std::string drawSeqActorBox(
    const std::string& kind,
    double cx, double topY, double w, double h,
    const TextBlock& label, double fontSize,
    const std::string& fill, const std::string& stroke
) {
    if (kind == "actor") {
        double iconTop = topY + 6.0;
        double headR = 8.0;
        double headCy = iconTop + headR;
        double bodyTop = headCy + headR;
        double bodyBot = bodyTop + 14.0;
        double armY = bodyTop + 5.0;
        double legBot = bodyBot + 11.0;
        double labelCy = legBot + 6.0 + label.height / 2.0;
        return "<circle cx=\"" + num(cx) + "\" cy=\"" + num(headCy) + "\" r=\"" + num(headR) +
               "\" fill=\"" + fill + "\" stroke=\"" + stroke + "\" stroke-width=\"var(--_w)\"/>"
               "<path d=\"M" + num(cx) + " " + num(bodyTop) + "V" + num(bodyBot) +
               "M" + num(cx - 12) + " " + num(armY) + "H" + num(cx + 12) +
               "M" + num(cx) + " " + num(bodyBot) + "L" + num(cx - 10) + " " + num(legBot) +
               "M" + num(cx) + " " + num(bodyBot) + "L" + num(cx + 10) + " " + num(legBot) +
               "\" fill=\"none\" stroke=\"" + stroke + "\" stroke-width=\"var(--_w)\" stroke-linecap=\"round\"/>" +
               labelSvg(cx, labelCy, label, fontSize);
    }
    if (kind == "database") {
        double x = cx - w / 2.0;
        double ry = 6.0;
        double b = topY + h;
        return "<path d=\"M" + num(x) + " " + num(topY + ry) +
               "A" + num(w / 2.0) + " " + num(ry) + " 0 0 1 " + num(x + w) + " " + num(topY + ry) +
               "V" + num(b - ry) +
               "A" + num(w / 2.0) + " " + num(ry) + " 0 0 1 " + num(x) + " " + num(b - ry) +
               "Z\" fill=\"" + fill + "\" stroke=\"" + stroke + "\" stroke-width=\"var(--_w)\"/>"
               "<ellipse cx=\"" + num(cx) + "\" cy=\"" + num(topY + ry) + "\" rx=\"" + num(w / 2.0) + "\" ry=\"" + num(ry) +
               "\" fill=\"" + fill + "\" stroke=\"" + stroke + "\" stroke-width=\"var(--_w)\"/>" +
               labelSvg(cx, topY + ry + (h - ry) / 2.0, label, fontSize);
    }
    if (kind == "collections") {
        double x = cx - w / 2.0;
        double off = 4.0;
        return "<rect x=\"" + num(x + off) + "\" y=\"" + num(topY - off) + "\" width=\"" + num(w) + "\" height=\"" + num(h) +
               "\" rx=\"var(--_r)\" fill=\"" + fill + "\" stroke=\"" + stroke + "\" stroke-width=\"var(--_w)\"/>"
               "<rect x=\"" + num(x) + "\" y=\"" + num(topY) + "\" width=\"" + num(w) + "\" height=\"" + num(h) +
               "\" rx=\"var(--_r)\" fill=\"" + fill + "\" stroke=\"" + stroke + "\" stroke-width=\"var(--_w)\"/>" +
               labelSvg(cx, topY + h / 2.0, label, fontSize);
    }
    double x = cx - w / 2.0;
    return "<rect x=\"" + num(x) + "\" y=\"" + num(topY) + "\" width=\"" + num(w) + "\" height=\"" + num(h) +
           "\" rx=\"var(--_r)\" fill=\"" + fill + "\" stroke=\"" + stroke + "\" stroke-width=\"var(--_w)\"/>" +
           labelSvg(cx, topY + h / 2.0, label, fontSize);
}

inline std::string drawSeqArrow(const std::string& arrowType, double x, double y, double angle) {
    bool isCross = (arrowType.find('x') != std::string::npos);
    bool isAsync = (arrowType.find(')') != std::string::npos);
    bool isPoint = (arrowType.find(">>") != std::string::npos);
    if (!isCross && !isAsync && !isPoint) return "";
    double c = std::cos(angle), s = std::sin(angle);
    auto rot = [&](double px, double py) {
        return num(x + px * c - py * s) + " " + num(y + px * s + py * c);
    };
    if (isCross) {
        return "<path d=\"M" + rot(-9, -4.5) + "L" + rot(-1, 4.5) + "M" + rot(-9, 4.5) + "L" + rot(-1, -4.5) +
               "\" fill=\"none\" stroke=\"var(--_l)\" stroke-width=\"var(--_hw)\" stroke-linecap=\"round\"/>";
    }
    if (isAsync) {
        return "<path d=\"M" + rot(-9, -4.5) + "L" + rot(0, 0) + "L" + rot(-9, 4.5) +
               "\" fill=\"none\" stroke=\"var(--_l)\" stroke-width=\"var(--_hw)\" stroke-linecap=\"round\" stroke-linejoin=\"round\"/>";
    }
    return "<polygon points=\"" + rot(0, 0) + " " + rot(-9, -4.2) + " " + rot(-9, 4.2) + "\" fill=\"var(--_l)\"/>";
}

inline RenderResult renderSequence(const std::string& code, const RenderOptions& opts) {
    double fontSize = opts.fontSize;
    double msgFont = std::round(fontSize * 0.9);
    double smallFont = std::round(fontSize * 0.8);

    struct SeqParticipant {
        std::string id;
        std::string label;
        std::string kind = "participant";
        int boxIdx = -1;
        TextBlock tb;
        double w = 96.0;
        double h = 40.0;
        double cx = 0.0;
    };
    struct SeqBox {
        std::string label;
        std::string color;
        std::vector<int> pIndices;
    };
    struct SeqEvent {
        std::string type; // "msg", "note", "frame_start", "frame_div", "frame_end"
        int from = -1;
        int to = -1;
        std::string arrow;
        std::string text;
        std::string notePos; // "left of", "right of", "over"
        std::string frameKind;
        bool activateTarget = false;
        bool deactivateSource = false;
    };
    struct SeqActivation {
        int pIdx;
        double startY;
        double endY;
        int depth;
    };

    std::vector<SeqParticipant> parts;
    std::map<std::string, int> partMap;
    std::vector<SeqBox> boxes;
    int currentBox = -1;
    bool autonumber = false;
    std::vector<SeqEvent> events;

    auto ensurePart = [&](const std::string& id, const std::string& label = "", const std::string& kind = "participant") -> int {
        auto it = partMap.find(id);
        if (it != partMap.end()) {
            if (!label.empty() && parts[it->second].label == parts[it->second].id) parts[it->second].label = label;
            if (kind != "participant") parts[it->second].kind = kind;
            return it->second;
        }
        int idx = static_cast<int>(parts.size());
        SeqParticipant p;
        p.id = id;
        p.label = label.empty() ? id : label;
        p.kind = kind;
        p.boxIdx = currentBox;
        if (currentBox >= 0) boxes[currentBox].pIndices.push_back(idx);
        parts.push_back(p);
        partMap[id] = idx;
        return idx;
    };

    auto lines = splitLines(code);
    bool headerDone = false;
    for (const auto& rawLine : lines) {
        std::string line = trim(rawLine);
        if (line.empty()) continue;
        std::string low = toLower(line);
        if (!headerDone && low.rfind("sequencediagram", 0) == 0) {
            headerDone = true;
            continue;
        }
        headerDone = true;
        if (low.rfind("autonumber", 0) == 0) { autonumber = true; continue; }
        if (low.rfind("box", 0) == 0 && (line.size() == 3 || line[3] == ' ' || line[3] == '\t')) {
            std::string rest = trim(line.substr(3));
            SeqBox bx;
            bx.label = unquote(rest);
            currentBox = static_cast<int>(boxes.size());
            boxes.push_back(bx);
            continue;
        }
        if (low == "end") {
            if (currentBox >= 0) { currentBox = -1; continue; }
            SeqEvent ev;
            ev.type = "frame_end";
            events.push_back(ev);
            continue;
        }
        // Check participant declarations
        static const char* pKinds[] = {"participant", "actor", "boundary", "control", "entity", "database", "collections", "queue"};
        bool matchedKind = false;
        for (const char* pk : pKinds) {
            size_t klen = std::strlen(pk);
            if (low.rfind(pk, 0) == 0 && line.size() > klen && (line[klen] == ' ' || line[klen] == '\t')) {
                std::string rest = trim(line.substr(klen));
                std::string pid = rest, plbl = rest;
                size_t asPos = toLower(rest).find(" as ");
                if (asPos != std::string::npos) {
                    pid = unquote(trim(rest.substr(0, asPos)));
                    plbl = unquote(trim(rest.substr(asPos + 4)));
                } else {
                    pid = unquote(rest);
                    plbl = pid;
                }
                ensurePart(pid, plbl, pk);
                matchedKind = true;
                break;
            }
        }
        if (matchedKind) continue;

        if (low.rfind("activate ", 0) == 0 || low.rfind("deactivate ", 0) == 0) continue;

        // Note
        if (low.rfind("note ", 0) == 0) {
            size_t colon = line.find(':');
            if (colon != std::string::npos) {
                std::string head = trim(line.substr(5, colon - 5));
                std::string text = trim(line.substr(colon + 1));
                std::string lhead = toLower(head);
                SeqEvent ev;
                ev.type = "note";
                ev.text = text;
                if (lhead.rfind("left of ", 0) == 0) {
                    ev.notePos = "left of";
                    ev.from = ensurePart(unquote(trim(head.substr(8))));
                    ev.to = ev.from;
                } else if (lhead.rfind("right of ", 0) == 0) {
                    ev.notePos = "right of";
                    ev.from = ensurePart(unquote(trim(head.substr(9))));
                    ev.to = ev.from;
                } else if (lhead.rfind("over ", 0) == 0) {
                    ev.notePos = "over";
                    std::string who = trim(head.substr(5));
                    auto sp = splitChar(who, ',');
                    ev.from = ensurePart(unquote(trim(sp[0])));
                    ev.to = (sp.size() > 1) ? ensurePart(unquote(trim(sp[1]))) : ev.from;
                }
                if (ev.from >= 0) events.push_back(ev);
                continue;
            }
        }

        // Frames: loop, alt, opt, par, critical, break, rect
        static const char* fStarts[] = {"loop", "alt", "opt", "par", "critical", "break", "rect"};
        bool matchedFrame = false;
        for (const char* fk : fStarts) {
            size_t flen = std::strlen(fk);
            if (low == fk || (low.rfind(fk, 0) == 0 && (line[flen] == ' ' || line[flen] == '\t'))) {
                SeqEvent ev;
                ev.type = "frame_start";
                ev.frameKind = fk;
                ev.text = (line.size() > flen) ? trim(line.substr(flen)) : "";
                events.push_back(ev);
                matchedFrame = true;
                break;
            }
        }
        if (matchedFrame) continue;

        static const char* fDivs[] = {"else", "and", "option"};
        for (const char* fd : fDivs) {
            size_t dlen = std::strlen(fd);
            if (low == fd || (low.rfind(fd, 0) == 0 && (line[dlen] == ' ' || line[dlen] == '\t'))) {
                SeqEvent ev;
                ev.type = "frame_div";
                ev.frameKind = fd;
                ev.text = (line.size() > dlen) ? trim(line.substr(dlen)) : "";
                events.push_back(ev);
                matchedFrame = true;
                break;
            }
        }
        if (matchedFrame) continue;

        // Message: `A ->>+ B : text`
        static const char* seqArrows[] = {"<<->>", "<<-->>", "-->>", "->>", "-->", "->", "--x", "-x", "--)", "-)"};
        for (const char* ar : seqArrows) {
            size_t apos = line.find(ar);
            if (apos != std::string::npos) {
                std::string lhs = unquote(trim(line.substr(0, apos)));
                size_t aend = apos + std::strlen(ar);
                bool act = false, deact = false;
                if (aend < line.size() && line[aend] == '+') { act = true; aend++; }
                else if (aend < line.size() && line[aend] == '-') { deact = true; aend++; }
                std::string rest = trim(line.substr(aend));
                std::string rhs = rest, msgText;
                size_t col = rest.find(':');
                if (col != std::string::npos) {
                    rhs = unquote(trim(rest.substr(0, col)));
                    msgText = trim(rest.substr(col + 1));
                } else {
                    rhs = unquote(rest);
                }
                if (!lhs.empty() && !rhs.empty()) {
                    SeqEvent ev;
                    ev.type = "msg";
                    ev.from = ensurePart(lhs);
                    ev.to = ensurePart(rhs);
                    ev.arrow = ar;
                    ev.text = msgText;
                    ev.activateTarget = act;
                    ev.deactivateSource = deact;
                    events.push_back(ev);
                }
                break;
            }
        }
    }

    if (parts.empty()) ensurePart("A");

    // Measure participants
    double maxActorH = 40.0;
    for (auto& p : parts) {
        p.tb = layoutLabel(p.label, fontSize, false);
        p.w = std::max(96.0, p.tb.width + 28.0);
        p.h = (p.kind == "actor") ? (48.0 + p.tb.height + 6.0) : std::max(40.0, p.tb.height + 20.0);
        maxActorH = std::max(maxActorH, p.h);
    }

    // Compute horizontal spacing between participants
    int P = static_cast<int>(parts.size());
    std::vector<double> gaps(std::max(1, P - 1), 130.0);
    for (int i = 0; i < P - 1; i++) {
        gaps[i] = std::max(130.0, (parts[i].w + parts[i + 1].w) / 2.0 + 36.0);
    }
    for (const auto& ev : events) {
        if (ev.type == "msg" && ev.from != ev.to) {
            TextBlock tb = layoutLabel(ev.text, msgFont, false);
            double needed = tb.width + 48.0;
            int lo = std::min(ev.from, ev.to);
            int hi = std::max(ev.from, ev.to);
            double cur = 0.0;
            for (int k = lo; k < hi; k++) cur += gaps[k];
            if (needed > cur) {
                double add = (needed - cur) / (hi - lo);
                for (int k = lo; k < hi; k++) gaps[k] += add;
            }
        }
    }

    double pad = 24.0;
    double leftMargin = pad;
    // Check if there is a "left of" note on participant 0
    for (const auto& ev : events) {
        if (ev.type == "note" && ev.notePos == "left of" && ev.from == 0) {
            TextBlock ntb = layoutLabel(ev.text, msgFont, false, 180.0);
            leftMargin = std::max(leftMargin, ntb.width + 36.0);
        }
    }
    parts[0].cx = leftMargin + parts[0].w / 2.0;
    for (int i = 1; i < P; i++) {
        parts[i].cx = parts[i - 1].cx + gaps[i - 1];
    }

    double topActorY = pad + (boxes.empty() ? 0.0 : 24.0);
    double curY = topActorY + maxActorH + 20.0;

    struct RenderedMsg {
        int from, to;
        std::string arrow;
        TextBlock tb;
        double y;
        int seqNum = 0;
    };
    struct RenderedNote {
        double x, y, w, h;
        TextBlock tb;
    };
    struct RenderedFrame {
        std::string kind;
        std::string label;
        double startY = 0.0;
        double endY = 0.0;
        int minP = 0, maxP = 0;
        std::vector< std::pair<double, std::string> > dividers;
    };

    std::vector<RenderedMsg> rMsgs;
    std::vector<RenderedNote> rNotes;
    std::vector<RenderedFrame> rFrames;
    std::vector<int> frameStack;
    int msgCounter = 0;

    for (const auto& ev : events) {
        if (ev.type == "msg") {
            msgCounter++;
            TextBlock tb = layoutLabel(ev.text, msgFont, false);
            for (int fi : frameStack) {
                rFrames[fi].minP = std::min({rFrames[fi].minP, ev.from, ev.to});
                rFrames[fi].maxP = std::max({rFrames[fi].maxP, ev.from, ev.to});
            }
            if (ev.from == ev.to) {
                double rowH = std::max(44.0, tb.height + 28.0);
                curY += 16.0;
                rMsgs.push_back({ev.from, ev.to, ev.arrow, tb, curY, autonumber ? msgCounter : 0});
                curY += rowH;
            } else {
                double labelH = ev.text.empty() ? 0.0 : (tb.height + 6.0);
                curY += std::max(28.0, labelH + 14.0);
                rMsgs.push_back({ev.from, ev.to, ev.arrow, tb, curY, autonumber ? msgCounter : 0});
                curY += 10.0;
            }
        } else if (ev.type == "note") {
            TextBlock tb = layoutLabel(ev.text, msgFont, false, 200.0);
            double nw = std::max(90.0, tb.width + 24.0);
            double nh = std::max(32.0, tb.height + 16.0);
            curY += 10.0;
            double nx = parts[ev.from].cx - nw / 2.0;
            if (ev.notePos == "left of") nx = parts[ev.from].cx - parts[ev.from].w / 2.0 - nw - 12.0;
            else if (ev.notePos == "right of") nx = parts[ev.from].cx + parts[ev.from].w / 2.0 + 12.0;
            else if (ev.from != ev.to) {
                double lx = std::min(parts[ev.from].cx, parts[ev.to].cx);
                double rx = std::max(parts[ev.from].cx, parts[ev.to].cx);
                nw = std::max(nw, (rx - lx) + 60.0);
                nx = (lx + rx) / 2.0 - nw / 2.0;
            }
            rNotes.push_back({nx, curY, nw, nh, tb});
            curY += nh + 8.0;
        } else if (ev.type == "frame_start") {
            curY += 12.0;
            int fi = static_cast<int>(rFrames.size());
            RenderedFrame rf;
            rf.kind = ev.frameKind;
            rf.label = ev.text;
            rf.startY = curY;
            rf.minP = P - 1;
            rf.maxP = 0;
            rFrames.push_back(rf);
            frameStack.push_back(fi);
            curY += 26.0;
        } else if (ev.type == "frame_div") {
            curY += 10.0;
            if (!frameStack.empty()) {
                rFrames[frameStack.back()].dividers.push_back({curY, ev.text});
            }
            curY += 22.0;
        } else if (ev.type == "frame_end") {
            curY += 12.0;
            if (!frameStack.empty()) {
                int fi = frameStack.back();
                frameStack.pop_back();
                rFrames[fi].endY = curY;
            }
            curY += 8.0;
        }
    }
    while (!frameStack.empty()) {
        int fi = frameStack.back();
        frameStack.pop_back();
        rFrames[fi].endY = curY;
    }

    double bottomActorY = curY + 16.0;
    double totalHeight = bottomActorY + maxActorH + pad;
    double maxX = parts.back().cx + parts.back().w / 2.0 + pad;
    for (const auto& rn : rNotes) maxX = std::max(maxX, rn.x + rn.w + pad);
    for (const auto& rm : rMsgs) {
        if (rm.from == rm.to) maxX = std::max(maxX, parts[rm.from].cx + 50.0 + rm.tb.width + pad);
    }

    std::string body;

    // 1. Participant boxes
    for (const auto& bx : boxes) {
        if (bx.pIndices.empty()) continue;
        double x0 = 1e9, x1 = 0.0;
        for (int pi : bx.pIndices) {
            x0 = std::min(x0, parts[pi].cx - parts[pi].w / 2.0 - 12.0);
            x1 = std::max(x1, parts[pi].cx + parts[pi].w / 2.0 + 12.0);
        }
        double by = pad - 6.0;
        double bh = totalHeight - pad * 2.0 + 12.0;
        body += "<g class=\"pele-box\"><rect x=\"" + num(x0) + "\" y=\"" + num(by) + "\" width=\"" + num(x1 - x0) + "\" height=\"" + num(bh) +
                "\" rx=\"var(--_r)\" fill=\"var(--_a)\" fill-opacity=\"0.4\" stroke=\"var(--_b)\" stroke-width=\"var(--_w)\"/>";
        if (!bx.label.empty()) {
            TextBlock btb = layoutLabel(bx.label, smallFont, true);
            body += labelSvg((x0 + x1) / 2.0, by + 14.0, btb, smallFont, "middle", "var(--_m)");
        }
        body += "</g>";
    }

    // 2. Lifelines
    for (const auto& p : parts) {
        body += "<line class=\"pele-lifeline\" x1=\"" + num(p.cx) + "\" y1=\"" + num(topActorY + p.h) +
                "\" x2=\"" + num(p.cx) + "\" y2=\"" + num(bottomActorY) +
                "\" stroke=\"var(--_b)\" stroke-width=\"var(--_w)\" stroke-dasharray=\"5 4\"/>";
    }

    // 3. Frames
    for (const auto& rf : rFrames) {
        int lo = (rf.minP <= rf.maxP) ? rf.minP : 0;
        int hi = (rf.minP <= rf.maxP) ? rf.maxP : (P - 1);
        double fx = parts[lo].cx - parts[lo].w / 2.0 - 16.0;
        double fw = (parts[hi].cx + parts[hi].w / 2.0 + 16.0) - fx;
        double fh = std::max(36.0, rf.endY - rf.startY);
        body += "<g class=\"pele-frame\">";
        if (rf.kind == "rect") {
            body += "<rect x=\"" + num(fx) + "\" y=\"" + num(rf.startY) + "\" width=\"" + num(fw) + "\" height=\"" + num(fh) +
                    "\" rx=\"var(--_r)\" fill=\"var(--_a)\" fill-opacity=\"0.45\" stroke=\"var(--_b)\" stroke-width=\"var(--_w)\"/>";
        } else {
            std::string tag = rf.kind;
            TextBlock ttb = layoutLabel(tag, smallFont, true);
            double tabW = std::max(46.0, ttb.width + 20.0);
            double tabH = 20.0;
            body += "<rect x=\"" + num(fx) + "\" y=\"" + num(rf.startY) + "\" width=\"" + num(fw) + "\" height=\"" + num(fh) +
                    "\" fill=\"none\" stroke=\"var(--_b)\" stroke-width=\"var(--_w)\"/>"
                    "<polygon points=\"" + num(fx) + "," + num(rf.startY) + " " + num(fx + tabW) + "," + num(rf.startY) + " " +
                    num(fx + tabW) + "," + num(rf.startY + tabH - 6) + " " + num(fx + tabW - 6) + "," + num(rf.startY + tabH) + " " +
                    num(fx) + "," + num(rf.startY + tabH) + "\" fill=\"var(--_a)\" stroke=\"var(--_b)\" stroke-width=\"var(--_w)\"/>" +
                    labelSvg(fx + (tabW - 3.0) / 2.0, rf.startY + tabH / 2.0, ttb, smallFont);
            if (!rf.label.empty()) {
                TextBlock ltb = layoutLabel("[" + rf.label + "]", smallFont, false);
                body += labelSvg(fx + tabW + 10.0 + ltb.width / 2.0, rf.startY + tabH / 2.0, ltb, smallFont, "middle", "var(--_m)");
            }
            for (const auto& dv : rf.dividers) {
                body += "<line x1=\"" + num(fx) + "\" y1=\"" + num(dv.first) + "\" x2=\"" + num(fx + fw) + "\" y2=\"" + num(dv.first) +
                        "\" stroke=\"var(--_b)\" stroke-width=\"var(--_w)\" stroke-dasharray=\"5 3\"/>";
                if (!dv.second.empty()) {
                    TextBlock dtb = layoutLabel("[" + dv.second + "]", smallFont, false);
                    body += labelSvg(fx + 14.0 + dtb.width / 2.0, dv.first + 12.0, dtb, smallFont, "middle", "var(--_m)");
                }
            }
        }
        body += "</g>";
    }

    // 4. Notes
    for (const auto& rn : rNotes) {
        double fold = 10.0;
        double x = rn.x, y = rn.y, w = rn.w, h = rn.h;
        body += "<g class=\"pele-note\">"
                "<polygon points=\"" + num(x) + "," + num(y) + " " + num(x + w - fold) + "," + num(y) + " " +
                num(x + w) + "," + num(y + fold) + " " + num(x + w) + "," + num(y + h) + " " + num(x) + "," + num(y + h) +
                "\" fill=\"var(--_a)\" stroke=\"var(--_b)\" stroke-width=\"var(--_w)\"/>"
                "<polyline points=\"" + num(x + w - fold) + "," + num(y) + " " + num(x + w - fold) + "," + num(y + fold) + " " +
                num(x + w) + "," + num(y + fold) + "\" fill=\"none\" stroke=\"var(--_b)\" stroke-width=\"var(--_w)\"/>" +
                labelSvg(x + w / 2.0, y + h / 2.0, rn.tb, msgFont) +
                "</g>";
    }

    // 5. Messages
    for (const auto& rm : rMsgs) {
        bool dashed = (rm.arrow.find("--") != std::string::npos);
        bool bidir = (rm.arrow.rfind("<<", 0) == 0);
        std::string dashAttr = dashed ? " stroke-dasharray=\"5 4\"" : "";
        body += "<g class=\"pele-message\">";
        double sx = parts[rm.from].cx;
        double ex = parts[rm.to].cx;
        if (rm.from == rm.to) {
            double loopW = 36.0, loopH = 24.0;
            body += "<path d=\"M" + num(sx) + " " + num(rm.y) + "H" + num(sx + loopW) + "V" + num(rm.y + loopH) + "H" + num(sx + 2.0) +
                    "\" fill=\"none\" stroke=\"var(--_l)\" stroke-width=\"var(--_w)\"" + dashAttr + "/>" +
                    drawSeqArrow(rm.arrow, sx, rm.y + loopH, kPi);
            if (!rm.tb.lines.empty() && !rm.tb.lines[0].empty()) {
                body += labelSvg(sx + loopW + 10.0, rm.y + loopH / 2.0, rm.tb, msgFont, "start");
            }
        } else {
            double dir = (ex >= sx) ? 1.0 : -1.0;
            double angle = (dir > 0) ? 0.0 : kPi;
            double trimEnd = (rm.arrow.find(">>") != std::string::npos) ? 7.0 : 0.0;
            double trimStart = bidir ? 7.0 : 0.0;
            body += "<line x1=\"" + num(sx + dir * trimStart) + "\" y1=\"" + num(rm.y) +
                    "\" x2=\"" + num(ex - dir * trimEnd) + "\" y2=\"" + num(rm.y) +
                    "\" stroke=\"var(--_l)\" stroke-width=\"var(--_w)\"" + dashAttr + "/>" +
                    drawSeqArrow(rm.arrow, ex, rm.y, angle);
            if (bidir) {
                body += drawSeqArrow("->>", sx, rm.y, angle + kPi);
            }
            if (!rm.tb.lines.empty() && !rm.tb.lines[0].empty()) {
                double midX = (sx + ex) / 2.0;
                double labelCy = rm.y - 8.0 - rm.tb.height / 2.0;
                body += labelSvg(midX, labelCy, rm.tb, msgFont);
            }
        }
        if (rm.seqNum > 0) {
            body += "<circle cx=\"" + num(sx) + "\" cy=\"" + num(rm.y) + "\" r=\"9\" fill=\"var(--_l)\"/>"
                    "<text x=\"" + num(sx) + "\" y=\"" + num(rm.y) + "\" text-anchor=\"middle\" dominant-baseline=\"central\" "
                    "font-size=\"" + num(smallFont) + "\" font-weight=\"600\" fill=\"var(--_bg)\">" + std::to_string(rm.seqNum) + "</text>";
        }
        body += "</g>";
    }

    // 6. Top & Bottom Actors
    for (const auto& p : parts) {
        body += "<g class=\"pele-actor\" data-id=\"" + esc(p.id) + "\">" +
                drawSeqActorBox(p.kind, p.cx, topActorY, p.w, p.h, p.tb, fontSize, "var(--_s)", "var(--_b)") +
                drawSeqActorBox(p.kind, p.cx, bottomActorY, p.w, p.h, p.tb, fontSize, "var(--_s)", "var(--_b)") +
                "</g>";
    }

    RenderResult r;
    r.type = "sequence";
    r.width = std::ceil(maxX);
    r.height = std::ceil(totalHeight);
    r.svg = svgDocument("sequence", r.width, r.height, fontSize, body);
    return r;
}

// ---------------------------------------------------------------------------
// 9. Class Diagram Parser & Renderer
// ---------------------------------------------------------------------------

inline std::string classMarker(const std::string& kind, double x, double y, double angle) {
    if (kind == "none" || kind.empty()) return "";
    double c = std::cos(angle), s = std::sin(angle);
    auto rot = [&](double px, double py) {
        return num(x + px * c - py * s) + "," + num(y + px * s + py * c);
    };
    if (kind == "extension") {
        return "<polygon points=\"" + rot(0, 0) + " " + rot(-12, -6) + " " + rot(-12, 6) +
               "\" fill=\"var(--_bg)\" stroke=\"var(--_l)\" stroke-width=\"var(--_hw)\" stroke-linejoin=\"round\"/>";
    }
    if (kind == "composition" || kind == "aggregation") {
        std::string fill = (kind == "composition") ? "var(--_l)" : "var(--_bg)";
        return "<polygon points=\"" + rot(0, 0) + " " + rot(-7, -4.5) + " " + rot(-14, 0) + " " + rot(-7, 4.5) +
               "\" fill=\"" + fill + "\" stroke=\"var(--_l)\" stroke-width=\"var(--_hw)\" stroke-linejoin=\"round\"/>";
    }
    if (kind == "dependency") {
        return "<polyline points=\"" + rot(-9, -4.5) + " " + rot(0, 0) + " " + rot(-9, 4.5) +
               "\" fill=\"none\" stroke=\"var(--_l)\" stroke-width=\"var(--_hw)\" stroke-linecap=\"round\" stroke-linejoin=\"round\"/>";
    }
    return "";
}

inline RenderResult renderClass(const std::string& code, const RenderOptions& opts) {
    double fontSize = opts.fontSize;
    double memberFont = std::round(fontSize * 0.88);
    double smallFont = std::round(fontSize * 0.8);

    struct ClassMember {
        std::string text;
        bool isMethod = false;
        bool isStatic = false;
        bool isAbstract = false;
    };
    struct ClassInfo {
        std::string id;
        std::string label;
        std::string annotation;
        std::vector<ClassMember> attrs;
        std::vector<ClassMember> methods;
    };

    std::string direction = "TD";
    std::vector<ClassInfo> classes;
    std::map<std::string, int> classMap;
    std::vector<LayoutEdge> edges;
    std::vector<std::pair<std::string, std::string>> edgeCards;
    std::vector<std::string> classNotes;

    auto ensureClass = [&](const std::string& rawId) -> int {
        std::string id = trim(rawId);
        std::string explicitLabel;
        size_t ob = id.find('[');
        if (ob != std::string::npos && id.back() == ']') {
            explicitLabel = unquote(trim(id.substr(ob + 1, id.size() - ob - 2)));
            id = trim(id.substr(0, ob));
        }
        std::string generic;
        size_t tilde = id.find('~');
        if (tilde != std::string::npos) {
            size_t tilde2 = id.find('~', tilde + 1);
            if (tilde2 != std::string::npos) {
                generic = id.substr(tilde + 1, tilde2 - tilde - 1);
                id = id.substr(0, tilde);
            }
        }
        id = unquote(trim(id));
        auto it = classMap.find(id);
        if (it != classMap.end()) {
            if (!explicitLabel.empty()) classes[it->second].label = explicitLabel;
            else if (!generic.empty()) classes[it->second].label = id + "<" + generic + ">";
            return it->second;
        }
        int idx = static_cast<int>(classes.size());
        ClassInfo ci;
        ci.id = id;
        ci.label = !explicitLabel.empty() ? explicitLabel : (generic.empty() ? id : (id + "<" + generic + ">"));
        classes.push_back(ci);
        classMap[id] = idx;
        return idx;
    };

    auto addMember = [&](int cidx, const std::string& rawMem) {
        std::string m = trim(rawMem);
        if (m.empty()) return;
        if (m.rfind("<<", 0) == 0 && m.size() > 4 && m.substr(m.size() - 2) == ">>") {
            classes[cidx].annotation = trim(m.substr(2, m.size() - 4));
            return;
        }
        ClassMember cm;
        if (!m.empty() && m.back() == '$' && (m.size() < 2 || m[m.size() - 2] != '$')) { cm.isStatic = true; m.pop_back(); m = trim(m); }
        if (!m.empty() && m.back() == '*') { cm.isAbstract = true; m.pop_back(); m = trim(m); }
        cm.isMethod = (m.find('(') != std::string::npos && m.find(')') != std::string::npos);
        if (cm.isMethod) {
            size_t rp = m.rfind(')');
            if (rp != std::string::npos && rp + 1 < m.size()) {
                std::string ret = trim(m.substr(rp + 1));
                if (!ret.empty() && ret.front() == '*') { cm.isAbstract = true; ret = trim(ret.substr(1)); }
                if (!ret.empty() && ret.front() == '$') { cm.isStatic = true; ret = trim(ret.substr(1)); }
                if (!ret.empty() && ret.front() != ':') {
                    m = trim(m.substr(0, rp + 1)) + " : " + ret;
                } else if (ret.empty()) {
                    m = trim(m.substr(0, rp + 1));
                }
            }
            cm.text = m;
            classes[cidx].methods.push_back(cm);
        } else {
            cm.text = m;
            classes[cidx].attrs.push_back(cm);
        }
    };

    auto lines = splitLines(code);
    int activeClass = -1;
    for (const auto& rawLine : lines) {
        std::string line = trim(rawLine);
        if (line.empty()) continue;
        std::string low = toLower(line);
        if (low.rfind("classdiagram", 0) == 0) continue;
        if (low.rfind("direction ", 0) == 0) {
            direction = trim(line.substr(10));
            for (char& c : direction) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            continue;
        }
        if (activeClass >= 0) {
            if (line == "}") { activeClass = -1; continue; }
            addMember(activeClass, line);
            continue;
        }
        if (line == "}") continue;
        if (low.rfind("namespace ", 0) == 0) continue;
        if (low.rfind("note ", 0) == 0) {
            size_t q1 = line.find('"');
            if (q1 != std::string::npos && line.back() == '"' && line.size() > q1 + 1) {
                classNotes.push_back(line.substr(q1 + 1, line.size() - q1 - 2));
            }
            continue;
        }

        if (low.rfind("class ", 0) == 0) {
            std::string rest = trim(line.substr(6));
            bool openBrace = (!rest.empty() && rest.back() == '{');
            if (openBrace) rest = trim(rest.substr(0, rest.size() - 1));
            int cidx = ensureClass(rest);
            if (openBrace) activeClass = cidx;
            continue;
        }

        // Check relationship operators
        struct RelRule {
            const char* pat;
            const char* stroke;
            const char* aStart;
            const char* aEnd;
        };
        static const RelRule rels[] = {
            {"<|--", "solid", "extension", "none"},
            {"--|>", "solid", "none", "extension"},
            {"<|..", "dotted", "extension", "none"},
            {"..|>", "dotted", "none", "extension"},
            {"*--", "solid", "composition", "none"},
            {"--*", "solid", "none", "composition"},
            {"o--", "solid", "aggregation", "none"},
            {"--o", "solid", "none", "aggregation"},
            {"<-->", "solid", "dependency", "dependency"},
            {"<--", "solid", "dependency", "none"},
            {"-->", "solid", "none", "dependency"},
            {"<..", "dotted", "dependency", "none"},
            {"..>", "dotted", "none", "dependency"},
            {"--", "solid", "none", "none"},
            {"..", "dotted", "none", "none"},
        };
        bool matchedRel = false;
        for (const auto& rl : rels) {
            size_t pos = line.find(rl.pat);
            if (pos != std::string::npos) {
                std::string lhs = trim(line.substr(0, pos));
                std::string rhsFull = trim(line.substr(pos + std::strlen(rl.pat)));
                std::string label;
                size_t col = rhsFull.find(':');
                if (col != std::string::npos) {
                    label = trim(rhsFull.substr(col + 1));
                    rhsFull = trim(rhsFull.substr(0, col));
                }
                // Strip optional cardinality quotes `"1"` on lhs/rhs
                std::string srcCard, dstCard;
                auto stripCard = [](std::string& s, std::string& card, bool atEnd) {
                    if (atEnd && !s.empty() && s.back() == '"') {
                        size_t q1 = s.rfind('"', s.size() - 2);
                        if (q1 != std::string::npos) {
                            card = s.substr(q1 + 1, s.size() - q1 - 2);
                            s = trim(s.substr(0, q1));
                        }
                    } else if (!atEnd && !s.empty() && s.front() == '"') {
                        size_t q2 = s.find('"', 1);
                        if (q2 != std::string::npos) {
                            card = s.substr(1, q2 - 1);
                            s = trim(s.substr(q2 + 1));
                        }
                    }
                };
                stripCard(lhs, srcCard, true);
                stripCard(rhsFull, dstCard, false);
                if (!lhs.empty() && !rhsFull.empty()) {
                    LayoutEdge le;
                    le.src = ensureClass(lhs);
                    le.dst = ensureClass(rhsFull);
                    le.stroke = rl.stroke;
                    le.arrowStart = rl.aStart;
                    le.arrowEnd = rl.aEnd;
                    le.label = label;
                    if (!label.empty()) {
                        TextBlock tb = layoutLabel(label, smallFont, false);
                        le.labelW = tb.width + 10.0;
                        le.labelH = tb.height + 6.0;
                    }
                    edges.push_back(le);
                    edgeCards.push_back({srcCard, dstCard});
                    matchedRel = true;
                }
                break;
            }
        }
        if (matchedRel) continue;

        // Check `ClassName : member`
        size_t col = line.find(':');
        if (col != std::string::npos) {
            std::string cname = trim(line.substr(0, col));
            std::string mem = trim(line.substr(col + 1));
            int cidx = ensureClass(cname);
            addMember(cidx, mem);
        }
    }

    // Build LayoutNodes for classes
    double padX = 12.0, padY = 8.0, memLineH = std::round(memberFont * 1.45);
    std::vector<LayoutNode> lnodes(classes.size());
    std::vector<double> headerHs(classes.size());
    for (size_t i = 0; i < classes.size(); i++) {
        const auto& ci = classes[i];
        TextBlock titleTb = layoutLabel(ci.label, fontSize, true);
        double maxW = titleTb.width;
        double hH = padY * 2.0 + titleTb.height;
        if (!ci.annotation.empty()) {
            TextBlock atb = layoutLabel("\xC2\xAB" + ci.annotation + "\xC2\xBB", smallFont, false);
            maxW = std::max(maxW, atb.width);
            hH += atb.height + 2.0;
        }
        for (const auto& a : ci.attrs) {
            maxW = std::max(maxW, measureTextWidth(a.text, memberFont, false, true));
        }
        for (const auto& m : ci.methods) {
            maxW = std::max(maxW, measureTextWidth(m.text, memberFont, false, true));
        }
        double totalW = std::max(120.0, maxW + padX * 2.0);
        double attrH = ci.attrs.empty() ? 0.0 : (ci.attrs.size() * memLineH + padY * 2.0);
        double methH = ci.methods.empty() ? 0.0 : (ci.methods.size() * memLineH + padY * 2.0);
        double totalH = hH + attrH + methH;
        headerHs[i] = hH;
        lnodes[i].id = ci.id;
        lnodes[i].label = ci.label;
        lnodes[i].w = totalW;
        lnodes[i].h = totalH;
    }

    std::vector<LayoutSubgraph> emptySgs;
    LayoutResult lres = layoutGraph(lnodes, edges, emptySgs, direction);

    std::string body;
    // 1. Edges
    int clsFlowAxis = (direction == "LR" || direction == "RL") ? 1 : 0;
    for (size_t ei = 0; ei < edges.size(); ei++) {
        const auto& e = edges[ei];
        if (e.route.size() < 2) continue;
        double sTr = (e.arrowStart == "extension" ? 12.0 : (e.arrowStart == "composition" || e.arrowStart == "aggregation") ? 14.0 : 0.0);
        double eTr = (e.arrowEnd == "extension" ? 12.0 : (e.arrowEnd == "composition" || e.arrowEnd == "aggregation") ? 14.0 : 0.0);
        EdgePath ep = routePoints(e.route, opts.curve, sTr, eTr, clsFlowAxis);
        std::string sMark, eMark;
        if (e.arrowStart != "none") {
            sMark = classMarker(e.arrowStart, ep.sx, ep.sy, std::atan2(ep.sdy, ep.sdx));
        }
        if (e.arrowEnd != "none") {
            eMark = classMarker(e.arrowEnd, ep.ex, ep.ey, std::atan2(ep.edy, ep.edx));
        }
        std::string dash = (e.stroke == "dotted") ? " stroke-dasharray=\"5 4\"" : "";
        body += "<g class=\"pele-edge\">"
                "<path d=\"" + ep.d + "\" fill=\"none\" stroke=\"var(--_l)\"" + dash + "/>" +
                sMark + eMark;
        if (!e.label.empty()) {
            TextBlock tb = layoutLabel(e.label, smallFont, false);
            body += edgeLabelSvg(e.labelX, e.labelY, tb, smallFont);
        }
        if (ei < edgeCards.size()) {
            if (!edgeCards[ei].first.empty()) {
                TextBlock ctb = layoutLabel(edgeCards[ei].first, smallFont, false);
                body += labelSvg(ep.sx - ep.sdx * 16.0 + 12.0, ep.sy - ep.sdy * 16.0, ctb, smallFont);
            }
            if (!edgeCards[ei].second.empty()) {
                TextBlock ctb = layoutLabel(edgeCards[ei].second, smallFont, false);
                body += labelSvg(ep.ex - ep.edx * 16.0 - 12.0, ep.ey - ep.edy * 16.0, ctb, smallFont);
            }
        }
        body += "</g>";
    }

    // 2. Classes
    for (size_t i = 0; i < classes.size(); i++) {
        const auto& ci = classes[i];
        const auto& n = lnodes[i];
        double x = n.x - n.w / 2.0;
        double y = n.y - n.h / 2.0;
        double hH = headerHs[i];
        body += "<g class=\"pele-class\" data-id=\"" + esc(ci.id) + "\">"
                "<rect x=\"" + num(x) + "\" y=\"" + num(y) + "\" width=\"" + num(n.w) + "\" height=\"" + num(n.h) +
                "\" rx=\"var(--_r)\" fill=\"var(--_s)\" stroke=\"var(--_b)\" stroke-width=\"var(--_w)\"/>";
        double curY = y + padY;
        if (!ci.annotation.empty()) {
            TextBlock atb = layoutLabel("\xC2\xAB" + ci.annotation + "\xC2\xBB", smallFont, false);
            body += "<g class=\"pele-annotation\">" + labelSvg(n.x, curY + atb.height / 2.0, atb, smallFont, "middle", "var(--_m)") + "</g>";
            curY += atb.height + 2.0;
        }
        TextBlock ttb = layoutLabel(ci.label, fontSize, true);
        body += labelSvg(n.x, curY + ttb.height / 2.0, ttb, fontSize);
        curY = y + hH;

        auto renderSection = [&](const std::vector<ClassMember>& mems) {
            if (mems.empty()) return;
            body += "<line class=\"pele-divider\" x1=\"" + num(x) + "\" y1=\"" + num(curY) + "\" x2=\"" + num(x + n.w) + "\" y2=\"" + num(curY) +
                    "\" stroke=\"var(--_b)\" stroke-width=\"var(--_w)\"/>"
                    "<g class=\"pele-members\" font-family=\"var(--_fm)\" font-size=\"" + num(memberFont) + "\">";
            curY += padY;
            for (const auto& m : mems) {
                double my = curY + memLineH / 2.0;
                std::string st;
                if (m.isAbstract) st += " font-style=\"italic\"";
                if (m.isStatic) st += " text-decoration=\"underline\"";
                body += "<text x=\"" + num(x + padX) + "\" y=\"" + num(my) + "\" dominant-baseline=\"central\"" + st + ">" + escText(m.text) + "</text>";
                curY += memLineH;
            }
            curY += padY;
            body += "</g>";
        };
        renderSection(ci.attrs);
        renderSection(ci.methods);
        body += "</g>";
    }

    double noteY = lres.height + 12.0;
    for (const auto& nt : classNotes) {
        TextBlock ntb = layoutLabel(nt, smallFont, false);
        double nw = std::max(80.0, ntb.width + 20.0), nh = std::max(28.0, ntb.height + 14.0);
        body += "<g class=\"pele-note\">"
                "<rect x=\"24\" y=\"" + num(noteY) + "\" width=\"" + num(nw) + "\" height=\"" + num(nh) +
                "\" rx=\"4\" fill=\"var(--_a)\" stroke=\"var(--_b)\" stroke-width=\"var(--_w)\"/>" +
                labelSvg(24.0 + nw / 2.0, noteY + nh / 2.0, ntb, smallFont) + "</g>";
        noteY += nh + 8.0;
        lres.width = std::max(lres.width, 48.0 + nw);
        lres.height = noteY + 12.0;
    }

    RenderResult r;
    r.type = "class";
    r.width = std::ceil(lres.width);
    r.height = std::ceil(lres.height);
    r.svg = svgDocument("class", r.width, r.height, fontSize, body);
    return r;
}

// ---------------------------------------------------------------------------
// 10. State Diagram Parser & Renderer
// ---------------------------------------------------------------------------

inline RenderResult renderState(const std::string& code, const RenderOptions& opts) {
    double fontSize = opts.fontSize;
    double edgeFont = std::round(fontSize * 0.85);
    double descFont = std::round(fontSize * 0.85);

    struct StateInfo {
        std::string id;
        std::string label;
        std::string kind = "normal"; // "normal", "start", "end", "choice", "fork", "join"
        std::vector<std::string> descriptions;
    };

    std::string direction = "TD";
    std::vector<StateInfo> states;
    std::map<std::string, int> stateMap;
    std::vector<LayoutEdge> edges;
    std::vector<LayoutSubgraph> subgraphs;
    std::vector<int> sgStack;
    std::vector<std::string> stateNotes;
    int starCount = 0;

    auto ensureState = [&](const std::string& id, const std::string& kind = "normal") -> int {
        auto it = stateMap.find(id);
        if (it != stateMap.end()) {
            if (kind != "normal") states[it->second].kind = kind;
            return it->second;
        }
        int idx = static_cast<int>(states.size());
        StateInfo si;
        si.id = id;
        si.label = id;
        si.kind = kind;
        states.push_back(si);
        stateMap[id] = idx;
        return idx;
    };

    auto lines = splitLines(code);
    for (const auto& rawLine : lines) {
        std::string line = trim(rawLine);
        if (line.empty()) continue;
        std::string low = toLower(line);
        if (low.rfind("statediagram", 0) == 0) continue;
        if (low.rfind("direction ", 0) == 0) {
            direction = trim(line.substr(10));
            for (char& c : direction) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            continue;
        }
        if (line == "}") {
            if (!sgStack.empty()) sgStack.pop_back();
            continue;
        }
        if (low.rfind("note ", 0) == 0) {
            size_t col = line.find(':');
            if (col != std::string::npos) {
                stateNotes.push_back(trim(line.substr(col + 1)));
            }
            continue;
        }

        if (low.rfind("state ", 0) == 0) {
            std::string rest = trim(line.substr(6));
            bool openBrace = (!rest.empty() && rest.back() == '{');
            if (openBrace) rest = trim(rest.substr(0, rest.size() - 1));
            std::string stKind = "normal";
            for (const char* stTag : {"<<choice>>", "<<fork>>", "<<join>>"}) {
                size_t tp = toLower(rest).find(stTag);
                if (tp != std::string::npos) {
                    stKind = std::string(stTag).substr(2, std::strlen(stTag) - 4);
                    rest = trim(rest.substr(0, tp));
                }
            }
            std::string sid = rest, slbl = rest;
            size_t asPos = toLower(rest).find(" as ");
            if (asPos != std::string::npos) {
                slbl = unquote(trim(rest.substr(0, asPos)));
                sid = unquote(trim(rest.substr(asPos + 4)));
            } else {
                sid = unquote(rest);
                slbl = sid;
            }
            if (openBrace) {
                int sgIdx = static_cast<int>(subgraphs.size());
                LayoutSubgraph sg;
                sg.id = sid;
                sg.title = slbl;
                if (!sgStack.empty()) {
                    sg.parentSubgraph = sgStack.back();
                    subgraphs[sgStack.back()].childSubgraphs.push_back(sgIdx);
                }
                subgraphs.push_back(sg);
                sgStack.push_back(sgIdx);
            } else {
                int idx = ensureState(sid, stKind);
                states[idx].label = slbl;
            }
            continue;
        }

        // Transition: `A --> B : label`
        size_t arrowPos = line.find("-->");
        if (arrowPos != std::string::npos) {
            std::string lhs = trim(line.substr(0, arrowPos));
            std::string rhsFull = trim(line.substr(arrowPos + 3));
            std::string label;
            size_t col = rhsFull.find(':');
            if (col != std::string::npos) {
                label = trim(rhsFull.substr(col + 1));
                rhsFull = trim(rhsFull.substr(0, col));
            }
            int u = -1, v = -1;
            if (lhs == "[*]") {
                starCount++;
                u = ensureState("__start_" + std::to_string(starCount), "start");
            } else {
                u = ensureState(unquote(lhs));
            }
            if (rhsFull == "[*]") {
                starCount++;
                v = ensureState("__end_" + std::to_string(starCount), "end");
            } else {
                v = ensureState(unquote(rhsFull));
            }
            LayoutEdge le;
            le.src = u;
            le.dst = v;
            le.label = label;
            if (!label.empty()) {
                TextBlock tb = layoutLabel(label, edgeFont, false);
                le.labelW = tb.width + 10.0;
                le.labelH = tb.height + 6.0;
            }
            edges.push_back(le);
            continue;
        }

        // State description: `StateId : description`
        size_t col = line.find(':');
        if (col != std::string::npos) {
            std::string sid = unquote(trim(line.substr(0, col)));
            std::string desc = trim(line.substr(col + 1));
            int idx = ensureState(sid);
            if (!desc.empty()) states[idx].descriptions.push_back(desc);
            continue;
        }
        ensureState(unquote(line));
    }

    std::vector<LayoutNode> lnodes(states.size());
    for (size_t i = 0; i < states.size(); i++) {
        const auto& si = states[i];
        lnodes[i].id = si.id;
        lnodes[i].label = si.label;
        if (si.kind == "start") { lnodes[i].w = 20.0; lnodes[i].h = 20.0; }
        else if (si.kind == "end") { lnodes[i].w = 24.0; lnodes[i].h = 24.0; }
        else if (si.kind == "choice") { lnodes[i].w = 28.0; lnodes[i].h = 28.0; }
        else if (si.kind == "fork" || si.kind == "join") { lnodes[i].w = 72.0; lnodes[i].h = 8.0; }
        else {
            TextBlock tb = layoutLabel(si.label, fontSize, !si.descriptions.empty(), 200.0);
            double maxW = tb.width;
            double descH = 0.0;
            for (const auto& d : si.descriptions) {
                TextBlock dtb = layoutLabel(d, descFont, false, 200.0);
                maxW = std::max(maxW, dtb.width);
                descH += dtb.height + 4.0;
            }
            lnodes[i].w = std::max(72.0, maxW + 28.0);
            lnodes[i].h = std::max(36.0, tb.height + 16.0 + (descH > 0 ? descH + 12.0 : 0.0));
        }
    }

    LayoutResult lres = layoutGraph(lnodes, edges, subgraphs, direction);

    std::string body;
    // Composite subgraphs
    for (const auto& sg : subgraphs) {
        if (sg.w <= 0 || sg.h <= 0) continue;
        body += "<g class=\"pele-state-composite\" data-id=\"" + esc(sg.id) + "\">"
                "<rect x=\"" + num(sg.x) + "\" y=\"" + num(sg.y) + "\" width=\"" + num(sg.w) + "\" height=\"" + num(sg.h) +
                "\" rx=\"8\" fill=\"var(--_a)\" fill-opacity=\"0.55\" stroke=\"var(--_b)\" stroke-width=\"var(--_w)\"/>"
                "<line x1=\"" + num(sg.x) + "\" y1=\"" + num(sg.y + 28.0) + "\" x2=\"" + num(sg.x + sg.w) + "\" y2=\"" + num(sg.y + 28.0) +
                "\" stroke=\"var(--_b)\" stroke-width=\"var(--_w)\"/>";
        TextBlock stb = layoutLabel(sg.title, fontSize * 0.92, true);
        body += labelSvg(sg.x + sg.w / 2.0, sg.y + 15.0, stb, fontSize * 0.92) + "</g>";
    }

    // Edges
    int stFlowAxis = (direction == "LR" || direction == "RL") ? 1 : 0;
    for (const auto& e : edges) {
        if (e.route.size() < 2) continue;
        EdgePath ep = routePoints(e.route, opts.curve, 0.0, 7.0, stFlowAxis);
        std::string eMark = marker("arrow_point", ep.ex, ep.ey, ep.edx, ep.edy);
        body += "<g class=\"pele-edge\">"
                "<path d=\"" + ep.d + "\" fill=\"none\" stroke=\"var(--_l)\" stroke-linecap=\"round\"/>" +
                eMark;
        if (!e.label.empty()) {
            TextBlock tb = layoutLabel(e.label, edgeFont, false);
            body += edgeLabelSvg(e.labelX, e.labelY, tb, edgeFont);
        }
        body += "</g>";
    }

    // States
    for (size_t i = 0; i < states.size(); i++) {
        const auto& si = states[i];
        const auto& n = lnodes[i];
        if (si.kind == "start") {
            body += "<circle class=\"pele-state-start\" cx=\"" + num(n.x) + "\" cy=\"" + num(n.y) + "\" r=\"9\" fill=\"var(--_l)\"/>";
        } else if (si.kind == "end") {
            body += "<g class=\"pele-state-end\">"
                    "<circle cx=\"" + num(n.x) + "\" cy=\"" + num(n.y) + "\" r=\"11\" fill=\"var(--_bg)\" stroke=\"var(--_l)\" stroke-width=\"var(--_hw)\"/>"
                    "<circle cx=\"" + num(n.x) + "\" cy=\"" + num(n.y) + "\" r=\"6\" fill=\"var(--_l)\"/></g>";
        } else if (si.kind == "choice") {
            double hw = n.w / 2.0, hh = n.h / 2.0;
            body += "<polygon class=\"pele-state-choice\" points=\"" + num(n.x) + "," + num(n.y - hh) + " " + num(n.x + hw) + "," + num(n.y) + " " +
                    num(n.x) + "," + num(n.y + hh) + " " + num(n.x - hw) + "," + num(n.y) + "\" fill=\"var(--_s)\" stroke=\"var(--_b)\" stroke-width=\"var(--_w)\"/>";
        } else if (si.kind == "fork" || si.kind == "join") {
            body += "<rect class=\"pele-state-" + si.kind + "\" x=\"" + num(n.x - n.w / 2.0) + "\" y=\"" + num(n.y - n.h / 2.0) +
                    "\" width=\"" + num(n.w) + "\" height=\"" + num(n.h) + "\" rx=\"3\" fill=\"var(--_l)\"/>";
        } else {
            double x = n.x - n.w / 2.0, y = n.y - n.h / 2.0;
            body += "<g class=\"pele-state\" data-id=\"" + esc(si.id) + "\">"
                    "<rect x=\"" + num(x) + "\" y=\"" + num(y) + "\" width=\"" + num(n.w) + "\" height=\"" + num(n.h) +
                    "\" rx=\"8\" fill=\"var(--_s)\" stroke=\"var(--_b)\" stroke-width=\"var(--_w)\"/>";
            TextBlock tb = layoutLabel(si.label, fontSize, !si.descriptions.empty(), 200.0);
            if (si.descriptions.empty()) {
                body += labelSvg(n.x, n.y, tb, fontSize);
            } else {
                double hH = tb.height + 14.0;
                body += labelSvg(n.x, y + hH / 2.0, tb, fontSize) +
                        "<line x1=\"" + num(x) + "\" y1=\"" + num(y + hH) + "\" x2=\"" + num(x + n.w) + "\" y2=\"" + num(y + hH) +
                        "\" stroke=\"var(--_b)\" stroke-width=\"var(--_w)\"/>";
                double cy = y + hH + 8.0;
                for (const auto& d : si.descriptions) {
                    TextBlock dtb = layoutLabel(d, descFont, false, 200.0);
                    body += labelSvg(x + 10.0, cy + dtb.height / 2.0, dtb, descFont, "start", "var(--_m)");
                    cy += dtb.height + 4.0;
                }
            }
            body += "</g>";
        }
    }

    double noteY = lres.height + 12.0;
    for (const auto& nt : stateNotes) {
        TextBlock ntb = layoutLabel(nt, descFont, false);
        double nw = std::max(80.0, ntb.width + 20.0), nh = std::max(28.0, ntb.height + 14.0);
        body += "<g class=\"pele-note\">"
                "<rect x=\"24\" y=\"" + num(noteY) + "\" width=\"" + num(nw) + "\" height=\"" + num(nh) +
                "\" rx=\"4\" fill=\"var(--_a)\" stroke=\"var(--_b)\" stroke-width=\"var(--_w)\"/>" +
                labelSvg(24.0 + nw / 2.0, noteY + nh / 2.0, ntb, descFont) + "</g>";
        noteY += nh + 8.0;
        lres.width = std::max(lres.width, 48.0 + nw);
        lres.height = noteY + 12.0;
    }

    RenderResult r;
    r.type = "state";
    r.width = std::ceil(lres.width);
    r.height = std::ceil(lres.height);
    r.svg = svgDocument("state", r.width, r.height, fontSize, body);
    return r;
}

// ---------------------------------------------------------------------------
// 11. ER Diagram Parser & Renderer
// ---------------------------------------------------------------------------

inline std::string erMarker(const std::string& card, double x, double y, double angle) {
    double c = std::cos(angle), s = std::sin(angle);
    auto pt = [&](double px, double py) -> std::pair<double, double> {
        return {x + px * c - py * s, y + px * s + py * c};
    };
    auto ln = [&](double x1, double y1, double x2, double y2) {
        auto [ax, ay] = pt(x1, y1);
        auto [bx, by] = pt(x2, y2);
        return "<line x1=\"" + num(ax) + "\" y1=\"" + num(ay) + "\" x2=\"" + num(bx) + "\" y2=\"" + num(by) + "\"/>";
    };
    auto circ = [&](double cx, double cy, double r) {
        auto [ax, ay] = pt(cx, cy);
        return "<circle cx=\"" + num(ax) + "\" cy=\"" + num(ay) + "\" r=\"" + num(r) + "\" fill=\"var(--_bg)\"/>";
    };
    auto crow = [&]() {
        return ln(-12, 0, 0, -6) + ln(-12, 0, 0, 0) + ln(-12, 0, 0, 6);
    };
    std::string inner;
    if (card == "only_one") inner = ln(-5, -6, -5, 6) + ln(-10, -6, -10, 6);
    else if (card == "zero_or_one") inner = ln(-5, -6, -5, 6) + circ(-13, 0, 4.5);
    else if (card == "one_or_more") inner = crow() + ln(-14, -6, -14, 6);
    else if (card == "zero_or_more") inner = crow() + circ(-17, 0, 4.5);
    return "<g stroke=\"var(--_l)\" stroke-width=\"var(--_hw)\" stroke-linecap=\"round\" fill=\"none\">" + inner + "</g>";
}

inline RenderResult renderEr(const std::string& code, const RenderOptions& opts) {
    double fontSize = opts.fontSize;
    double attrFont = std::round(fontSize * 0.86);
    double smallFont = std::round(fontSize * 0.78);

    struct ErAttr {
        std::string type;
        std::string name;
        std::string key;
        std::string comment;
    };
    struct ErEntity {
        std::string id;
        std::string label;
        std::vector<ErAttr> attrs;
        double colTypeW = 0.0;
        double colNameW = 0.0;
        double colKeyW = 0.0;
        double colCommentW = 0.0;
    };

    std::string direction = "TD";
    std::vector<ErEntity> entities;
    std::map<std::string, int> entMap;
    std::vector<LayoutEdge> edges;
    std::vector<std::pair<std::string, std::string>> edgeCards;

    auto ensureEnt = [&](const std::string& rawId) -> int {
        std::string id = trim(rawId);
        std::string explicitLabel;
        size_t ob = id.find('[');
        if (ob != std::string::npos && id.back() == ']') {
            explicitLabel = unquote(trim(id.substr(ob + 1, id.size() - ob - 2)));
            id = trim(id.substr(0, ob));
        }
        id = unquote(trim(id));
        auto it = entMap.find(id);
        if (it != entMap.end()) {
            if (!explicitLabel.empty()) entities[it->second].label = explicitLabel;
            return it->second;
        }
        int idx = static_cast<int>(entities.size());
        ErEntity e;
        e.id = id;
        e.label = explicitLabel.empty() ? id : explicitLabel;
        entities.push_back(e);
        entMap[id] = idx;
        return idx;
    };

    auto parseLeftCard = [](const std::string& s) -> std::string {
        if (s == "||") return "only_one";
        if (s == "|o" || s == "o|") return "zero_or_one";
        if (s == "}|" || s == "|{") return "one_or_more";
        if (s == "}o" || s == "o{") return "zero_or_more";
        return "only_one";
    };

    auto lines = splitLines(code);
    int activeEnt = -1;
    for (const auto& rawLine : lines) {
        std::string line = trim(rawLine);
        if (line.empty()) continue;
        std::string low = toLower(line);
        if (low.rfind("erdiagram", 0) == 0 || low.rfind("subgraph ", 0) == 0 || low == "end") continue;
        if (low.rfind("direction ", 0) == 0) {
            direction = trim(line.substr(10));
            for (char& c : direction) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            continue;
        }
        if (activeEnt >= 0) {
            if (line == "}") { activeEnt = -1; continue; }
            std::string attrLine = line;
            std::string commentStr;
            size_t q1 = attrLine.find('"');
            if (q1 != std::string::npos && attrLine.back() == '"' && attrLine.size() > q1 + 1) {
                commentStr = attrLine.substr(q1 + 1, attrLine.size() - q1 - 2);
                attrLine = trim(attrLine.substr(0, q1));
            }
            std::istringstream iss(attrLine);
            ErAttr a;
            a.comment = commentStr;
            iss >> a.type >> a.name;
            std::string tok;
            if (iss >> tok) {
                if (tok == "PK" || tok == "FK" || tok == "UK" || tok.find("PK") != std::string::npos || tok.find("FK") != std::string::npos) {
                    a.key = tok;
                }
            }
            if (!a.type.empty() && !a.name.empty()) entities[activeEnt].attrs.push_back(a);
            continue;
        }
        if (line.back() == '{') {
            std::string ename = trim(line.substr(0, line.size() - 1));
            activeEnt = ensureEnt(ename);
            continue;
        }

        // Check ER relationship: `A ||--o{ B : "places"`
        static const char* lCards[] = {"||", "|o", "o|", "}|", "|{", "}o", "o{"};
        bool matchedRel = false;
        for (const char* lc : lCards) {
            for (const char* mid : {"--", ".."}) {
                for (const char* rc : lCards) {
                    std::string pat = std::string(lc) + mid + rc;
                    size_t pos = line.find(pat);
                    if (pos != std::string::npos) {
                        std::string lhs = trim(line.substr(0, pos));
                        std::string rhsFull = trim(line.substr(pos + pat.size()));
                        std::string label;
                        size_t col = rhsFull.find(':');
                        if (col != std::string::npos) {
                            label = unquote(trim(rhsFull.substr(col + 1)));
                            rhsFull = trim(rhsFull.substr(0, col));
                        }
                        if (!lhs.empty() && !rhsFull.empty()) {
                            LayoutEdge le;
                            le.src = ensureEnt(lhs);
                            le.dst = ensureEnt(rhsFull);
                            le.stroke = (std::string(mid) == "..") ? "dotted" : "solid";
                            le.label = label;
                            if (!label.empty()) {
                                TextBlock tb = layoutLabel(label, smallFont, false);
                                le.labelW = tb.width + 10.0;
                                le.labelH = tb.height + 6.0;
                            }
                            edges.push_back(le);
                            edgeCards.push_back({parseLeftCard(lc), parseLeftCard(rc)});
                            matchedRel = true;
                        }
                        break;
                    }
                }
                if (matchedRel) break;
            }
            if (matchedRel) break;
        }
        if (matchedRel) continue;

        // Check word-based ER relationship (`CAR 1 to zero or more NAMED-DRIVER : allows`)
        size_t colPos = line.find(':');
        if (colPos != std::string::npos && low.find(" to ") < colPos) {
            std::string head = trim(line.substr(0, colPos));
            std::string label = unquote(trim(line.substr(colPos + 1)));
            size_t sp1 = head.find(' ');
            size_t sp2 = head.rfind(' ');
            if (sp1 != std::string::npos && sp2 != std::string::npos && sp2 > sp1) {
                std::string lhs = trim(head.substr(0, sp1));
                std::string rhs = trim(head.substr(sp2 + 1));
                LayoutEdge le;
                le.src = ensureEnt(lhs);
                le.dst = ensureEnt(rhs);
                le.stroke = (low.find("optionally") != std::string::npos) ? "dotted" : "solid";
                le.label = label;
                if (!label.empty()) {
                    TextBlock tb = layoutLabel(label, smallFont, false);
                    le.labelW = tb.width + 10.0;
                    le.labelH = tb.height + 6.0;
                }
                edges.push_back(le);
                edgeCards.push_back({"only_one", "zero_or_more"});
                continue;
            }
        }

        // Standalone entity declaration (`CUSTOMER` or `"This  Unicode"`)
        ensureEnt(line);
    }

    double padX = 12.0, colGap = 12.0, baseHeaderH = std::round(fontSize * 1.4) + 16.0, rowH = std::round(attrFont * 1.45) + 4.0;
    std::vector<LayoutNode> lnodes(entities.size());
    std::vector<double> entHeaderHs(entities.size(), baseHeaderH);
    for (size_t i = 0; i < entities.size(); i++) {
        auto& e = entities[i];
        TextBlock ttb = layoutLabel(e.label, fontSize, true);
        entHeaderHs[i] = std::max(baseHeaderH, ttb.height + 16.0);
        double attrsH = 0.0;
        for (const auto& a : e.attrs) {
            e.colTypeW = std::max(e.colTypeW, measureTextWidth(a.type, attrFont, false, true));
            e.colNameW = std::max(e.colNameW, measureTextWidth(a.name, attrFont, false, false));
            if (!a.key.empty()) e.colKeyW = std::max(e.colKeyW, measureTextWidth(a.key, smallFont, true, false));
            if (!a.comment.empty()) {
                TextBlock ctb = layoutLabel(a.comment, smallFont, false);
                e.colCommentW = std::max(e.colCommentW, ctb.width);
                attrsH += std::max(rowH, ctb.height + 6.0);
            } else {
                attrsH += rowH;
            }
        }
        double attrW = e.attrs.empty() ? 0.0 : (padX * 2.0 + e.colTypeW + colGap + e.colNameW +
                       (e.colKeyW > 0 ? colGap + e.colKeyW : 0.0) +
                       (e.colCommentW > 0 ? colGap + e.colCommentW : 0.0));
        lnodes[i].id = e.id;
        lnodes[i].label = e.label;
        lnodes[i].w = std::max({120.0, ttb.width + padX * 2.0, attrW});
        lnodes[i].h = entHeaderHs[i] + attrsH;
    }

    std::vector<LayoutSubgraph> emptySgs;
    LayoutResult lres = layoutGraph(lnodes, edges, emptySgs, direction);

    std::string body;
    for (size_t ei = 0; ei < edges.size(); ei++) {
        const auto& e = edges[ei];
        if (e.route.size() < 2) continue;
        const auto& pts = e.route;
        size_t L = pts.size() - 1;
        double sAng = std::atan2(pts[0].second - pts[1].second, pts[0].first - pts[1].first);
        double eAng = std::atan2(pts[L].second - pts[L - 1].second, pts[L].first - pts[L - 1].first);
        std::string dash = (e.stroke == "dotted") ? " stroke-dasharray=\"5 4\"" : "";
        body += "<g class=\"pele-edge\">"
                "<path d=\"" + routePoints(pts, opts.curve) + "\" fill=\"none\" stroke=\"var(--_l)\" stroke-width=\"var(--_w)\"" + dash + "/>" +
                erMarker(edgeCards[ei].first, pts[0].first, pts[0].second, sAng) +
                erMarker(edgeCards[ei].second, pts[L].first, pts[L].second, eAng);
        if (!e.label.empty()) {
            TextBlock tb = layoutLabel(e.label, smallFont, false);
            body += edgeLabelSvg(e.labelX, e.labelY, tb, smallFont);
        }
        body += "</g>";
    }

    for (size_t i = 0; i < entities.size(); i++) {
        const auto& e = entities[i];
        const auto& n = lnodes[i];
        double headerH = entHeaderHs[i];
        double x = n.x - n.w / 2.0, y = n.y - n.h / 2.0;
        body += "<g class=\"pele-entity\" data-id=\"" + esc(e.id) + "\">"
                "<rect x=\"" + num(x) + "\" y=\"" + num(y) + "\" width=\"" + num(n.w) + "\" height=\"" + num(n.h) +
                "\" rx=\"var(--_r)\" fill=\"var(--_s)\" stroke=\"none\"/>";
        if (!e.attrs.empty()) {
            body += "<g class=\"pele-er-stripes\">";
            double ry = y + headerH;
            for (size_t ai = 0; ai < e.attrs.size(); ai++) {
                double thisRowH = rowH;
                if (!e.attrs[ai].comment.empty()) {
                    TextBlock ctb = layoutLabel(e.attrs[ai].comment, smallFont, false);
                    thisRowH = std::max(rowH, ctb.height + 6.0);
                }
                if (ai % 2 == 1) {
                    body += "<rect x=\"" + num(x) + "\" y=\"" + num(ry) + "\" width=\"" + num(n.w) + "\" height=\"" + num(thisRowH) +
                            "\" fill=\"var(--_a)\" fill-opacity=\"0.6\"/>";
                }
                ry += thisRowH;
            }
            body += "</g>";
        }
        body += "<rect x=\"" + num(x) + "\" y=\"" + num(y) + "\" width=\"" + num(n.w) + "\" height=\"" + num(n.h) +
                "\" rx=\"var(--_r)\" fill=\"none\" stroke=\"var(--_b)\" stroke-width=\"var(--_w)\"/>";
        TextBlock ttb = layoutLabel(e.label, fontSize, true);
        body += labelSvg(n.x, y + headerH / 2.0, ttb, fontSize);
        if (!e.attrs.empty()) {
            body += "<line x1=\"" + num(x) + "\" y1=\"" + num(y + headerH) + "\" x2=\"" + num(x + n.w) + "\" y2=\"" + num(y + headerH) +
                    "\" stroke=\"var(--_b)\" stroke-width=\"var(--_w)\"/>"
                    "<g class=\"pele-er-attributes\" font-size=\"" + num(attrFont) + "\">";
            double typeX = x + padX;
            double nameX = typeX + e.colTypeW + colGap;
            double keyX = nameX + e.colNameW + colGap;
            double commentX = keyX + (e.colKeyW > 0 ? e.colKeyW + colGap : 0.0);
            double ry = y + headerH;
            for (size_t ai = 0; ai < e.attrs.size(); ai++) {
                TextBlock ctb;
                double thisRowH = rowH;
                if (!e.attrs[ai].comment.empty()) {
                    ctb = layoutLabel(e.attrs[ai].comment, smallFont, false);
                    thisRowH = std::max(rowH, ctb.height + 6.0);
                }
                double cy = ry + thisRowH / 2.0;
                body += "<text x=\"" + num(typeX) + "\" y=\"" + num(cy) + "\" dominant-baseline=\"central\" font-family=\"var(--_fm)\" fill=\"var(--_m)\">" + escText(e.attrs[ai].type) + "</text>"
                        "<text x=\"" + num(nameX) + "\" y=\"" + num(cy) + "\" dominant-baseline=\"central\">" + escText(e.attrs[ai].name) + "</text>";
                if (!e.attrs[ai].key.empty()) {
                    body += "<text x=\"" + num(keyX) + "\" y=\"" + num(cy) + "\" dominant-baseline=\"central\" font-size=\"" + num(smallFont) + "\" font-weight=\"600\" fill=\"var(--_c)\">" + escText(e.attrs[ai].key) + "</text>";
                }
                if (!e.attrs[ai].comment.empty()) {
                    body += labelSvg(commentX, cy, ctb, smallFont, "start", "var(--_m)");
                }
                ry += thisRowH;
            }
            body += "</g>";
        }
        body += "</g>";
    }

    RenderResult r;
    r.type = "er";
    r.width = std::ceil(lres.width);
    r.height = std::ceil(lres.height);
    r.svg = svgDocument("er", r.width, r.height, fontSize, body);
    return r;
}

// ---------------------------------------------------------------------------
// 12. C4, Block, Architecture, Requirement, UseCase Renderers
// ---------------------------------------------------------------------------

inline std::vector<std::string> parseCallArgs(const std::string& argStr) {
    std::vector<std::string> res;
    std::string cur;
    char q = 0;
    for (size_t i = 0; i < argStr.size(); i++) {
        char c = argStr[i];
        if (q) {
            if (c == q) q = 0;
            else cur.push_back(c);
        } else {
            if (c == '"' || c == '\'') q = c;
            else if (c == ',') {
                res.push_back(trim(cur));
                cur.clear();
            } else {
                cur.push_back(c);
            }
        }
    }
    if (!cur.empty() || !argStr.empty()) res.push_back(trim(cur));
    return res;
}

inline RenderResult renderC4(const std::string& code, const RenderOptions& opts) {
    double fontSize = opts.fontSize;
    double smallFont = std::round(fontSize * 0.8);

    struct C4Elem {
        std::string id;
        std::string label;
        std::string kind;
        std::string tech;
        std::string desc;
        bool external = false;
    };
    std::vector<C4Elem> elems;
    std::map<std::string, int> elemMap;
    std::vector<LayoutEdge> edges;
    std::vector<LayoutSubgraph> subgraphs;
    std::vector<int> sgStack;
    std::string title;

    for (const auto& rawLine : splitLines(code)) {
        std::string line = trim(rawLine);
        if (line.empty()) continue;
        std::string low = toLower(line);
        if (low.rfind("c4", 0) == 0) continue;
        if (low.rfind("title ", 0) == 0) { title = unquote(trim(line.substr(6))); continue; }
        if (line == "}") { if (!sgStack.empty()) sgStack.pop_back(); continue; }
        bool openBrace = (!line.empty() && line.back() == '{');
        if (openBrace) line = trim(line.substr(0, line.size() - 1));
        size_t lp = line.find('('), rp = line.rfind(')');
        if (lp == std::string::npos || rp == std::string::npos || rp < lp) continue;
        std::string fn = trim(line.substr(0, lp));
        std::string lfn = toLower(fn);
        auto args = parseCallArgs(line.substr(lp + 1, rp - lp - 1));
        if (openBrace || lfn.find("boundary") != std::string::npos) {
            int sgIdx = static_cast<int>(subgraphs.size());
            LayoutSubgraph sg;
            sg.id = args.size() > 0 ? args[0] : "b";
            sg.title = args.size() > 1 ? args[1] : sg.id;
            if (!sgStack.empty()) {
                sg.parentSubgraph = sgStack.back();
                subgraphs[sgStack.back()].childSubgraphs.push_back(sgIdx);
            }
            subgraphs.push_back(sg);
            sgStack.push_back(sgIdx);
        } else if (lfn.rfind("rel", 0) == 0 || lfn.rfind("birel", 0) == 0) {
            if (args.size() >= 3 && elemMap.count(args[0]) && elemMap.count(args[1])) {
                LayoutEdge le;
                le.src = elemMap[args[0]];
                le.dst = elemMap[args[1]];
                le.label = args[2];
                if (args.size() >= 4 && !args[3].empty()) le.label += "\n[" + args[3] + "]";
                TextBlock tb = layoutLabel(le.label, smallFont, false, 80.0);
                le.labelW = tb.width + 10.0;
                le.labelH = tb.height + 6.0;
                edges.push_back(le);
            }
        } else if (!args.empty()) {
            C4Elem el;
            el.id = args[0];
            el.label = args.size() > 1 ? args[1] : args[0];
            el.kind = fn;
            el.external = (lfn.find("_ext") != std::string::npos);
            if (lfn.rfind("person", 0) == 0 || lfn.rfind("system", 0) == 0) {
                el.desc = args.size() > 2 ? args[2] : "";
            } else {
                el.tech = args.size() > 2 ? args[2] : "";
                el.desc = args.size() > 3 ? args[3] : "";
            }
            int idx = static_cast<int>(elems.size());
            elems.push_back(el);
            elemMap[el.id] = idx;
            if (!sgStack.empty()) subgraphs[sgStack.back()].nodeIndices.push_back(idx);
        }
    }

    std::vector<LayoutNode> lnodes(elems.size());
    for (size_t i = 0; i < elems.size(); i++) {
        lnodes[i].id = elems[i].id;
        lnodes[i].w = 180.0;
        lnodes[i].h = elems[i].desc.empty() ? 76.0 : 102.0;
        if (!sgStack.empty()) lnodes[i].parentSubgraph = sgStack.back();
    }
    for (size_t si = 0; si < subgraphs.size(); si++) {
        for (int ni : subgraphs[si].nodeIndices) lnodes[ni].parentSubgraph = static_cast<int>(si);
    }

    LayoutResult lres = layoutGraph(lnodes, edges, subgraphs, "TD", 44.0, 56.0);
    double yOff = title.empty() ? 0.0 : 30.0;
    std::string body;
    if (!title.empty()) {
        TextBlock ttb = layoutLabel(title, fontSize * 1.15, true);
        body += labelSvg(lres.width / 2.0, 20.0, ttb, fontSize * 1.15);
    }
    for (const auto& sg : subgraphs) {
        if (sg.w <= 0 || sg.h <= 0) continue;
        body += "<g class=\"pele-c4-boundary\"><rect x=\"" + num(sg.x) + "\" y=\"" + num(sg.y + yOff) +
                "\" width=\"" + num(sg.w) + "\" height=\"" + num(sg.h) +
                "\" rx=\"8\" fill=\"none\" stroke=\"var(--_b)\" stroke-width=\"var(--_w)\" stroke-dasharray=\"6 4\"/>" +
                labelSvg(sg.x + 14.0, sg.y + yOff + 16.0, layoutLabel(sg.title, smallFont, true), smallFont, "start", "var(--_m)") + "</g>";
    }
    for (const auto& e : edges) {
        if (e.route.size() < 2) continue;
        auto pts = e.route;
        for (auto& p : pts) p.second += yOff;
        size_t L = pts.size() - 1;
        double ang = std::atan2(pts[L].second - pts[L - 1].second, pts[L].first - pts[L - 1].first);
        std::string eMark = marker("arrow_point", pts[L].first, pts[L].second, ang);
        pts[L].first -= std::cos(ang) * 7.0;
        pts[L].second -= std::sin(ang) * 7.0;
        body += "<g class=\"pele-edge\"><path d=\"" + routePoints(pts, opts.curve) +
                "\" fill=\"none\" stroke=\"var(--_l)\" stroke-width=\"var(--_w)\" stroke-dasharray=\"4 3\"/>" + eMark;
        if (!e.label.empty()) body += edgeLabelSvg(e.labelX, e.labelY + yOff, layoutLabel(e.label, smallFont, false, 80.0), smallFont);
        body += "</g>";
    }
    for (size_t i = 0; i < elems.size(); i++) {
        const auto& el = elems[i];
        const auto& n = lnodes[i];
        double x = n.x - n.w / 2.0, y = n.y - n.h / 2.0 + yOff;
        std::string fill = el.external ? "var(--_a)" : "var(--_c)";
        std::string op = el.external ? "0.7" : "0.18";
        body += "<g class=\"pele-c4-element\" data-id=\"" + esc(el.id) + "\">"
                "<rect x=\"" + num(x) + "\" y=\"" + num(y) + "\" width=\"" + num(n.w) + "\" height=\"" + num(n.h) +
                "\" rx=\"8\" fill=\"" + fill + "\" fill-opacity=\"" + op + "\" stroke=\"var(--_b)\" stroke-width=\"var(--_w)\"/>";
        std::string lkind = toLower(el.kind);
        std::string stereo = lkind;
        if (lkind.find("person") != std::string::npos) stereo = el.external ? "external person" : "person";
        else if (lkind.find("system") != std::string::npos) stereo = el.external ? "external system" : "system";
        else if (lkind.find("container") != std::string::npos) stereo = el.external ? "external container" : "container";
        else if (lkind.find("component") != std::string::npos) stereo = el.external ? "external component" : "component";
        double cy = y + 15.0;
        body += labelSvg(n.x, cy, layoutLabel("\xC2\xAB" + stereo + "\xC2\xBB", smallFont * 0.9, false), smallFont * 0.9, "middle", "var(--_m)");
        cy += 18.0;
        body += labelSvg(n.x, cy, layoutLabel(el.label, fontSize, true), fontSize);
        if (!el.tech.empty()) {
            cy += 16.0;
            body += labelSvg(n.x, cy, layoutLabel("[" + el.tech + "]", smallFont * 0.9, false), smallFont * 0.9, "middle", "var(--_m)");
        }
        if (!el.desc.empty()) {
            cy += 20.0;
            body += labelSvg(n.x, cy, layoutLabel(el.desc, smallFont, false, n.w - 20.0), smallFont, "middle", "var(--_m)");
        }
        body += "</g>";
    }

    RenderResult r;
    r.type = "c4";
    r.width = std::ceil(lres.width);
    r.height = std::ceil(lres.height + yOff);
    r.svg = svgDocument("c4", r.width, r.height, fontSize, body);
    return r;
}

inline RenderResult renderBlock(const std::string& code, const RenderOptions& opts) {
    double fontSize = opts.fontSize;
    int columns = 0;
    struct BItem {
        std::string id;
        std::string label;
        std::string shape = "rect";
        int span = 1;
        bool isSpace = false;
    };
    struct BLink {
        std::string src;
        std::string dst;
        std::string label;
    };
    std::vector<BItem> items;
    std::map<std::string, int> itemMap;
    std::vector<BLink> links;

    for (const auto& rawLine : splitLines(code)) {
        std::string line = trim(rawLine);
        if (line.empty()) continue;
        std::string low = toLower(line);
        if (low.rfind("block", 0) == 0 && (line.size() == 5 || line.rfind("block-beta", 0) == 0)) continue;
        if (low == "end") continue;
        if (low.rfind("columns ", 0) == 0) {
            try { columns = std::stoi(trim(line.substr(8))); } catch (...) {}
            continue;
        }
        if (line.find("-->") != std::string::npos) {
            size_t p = line.find("-->");
            std::string lhs = trim(line.substr(0, p));
            std::string rhs = trim(line.substr(p + 3));
            std::string lbl;
            size_t dpos = lhs.find("--");
            if (dpos != std::string::npos) {
                lbl = unquote(trim(lhs.substr(dpos + 2)));
                lhs = trim(lhs.substr(0, dpos));
            }
            links.push_back({lhs, rhs, lbl});
            continue;
        }
        // Tokenize space-separated blocks on this line
        size_t i = 0;
        while (i < line.size()) {
            while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) i++;
            if (i >= line.size()) break;
            size_t st = i;
            int d = 0;
            char q = 0;
            while (i < line.size()) {
                char c = line[i];
                if (q) { if (c == q) q = 0; }
                else if (c == '"' || c == '\'') q = c;
                else if (c == '[' || c == '(' || c == '{') d++;
                else if (c == ']' || c == ')' || c == '}') d = std::max(0, d - 1);
                else if ((c == ' ' || c == '\t') && d == 0) break;
                i++;
            }
            std::string tok = trim(line.substr(st, i - st));
            if (tok.empty()) continue;
            int span = 1;
            size_t col = tok.rfind(':');
            if (col != std::string::npos && col + 1 < tok.size() && std::isdigit(static_cast<unsigned char>(tok[col + 1]))) {
                try { span = std::max(1, std::stoi(tok.substr(col + 1))); } catch (...) {}
                tok = tok.substr(0, col);
            }
            if (tok == "space") {
                BItem bi; bi.isSpace = true; bi.span = span;
                items.push_back(bi);
            } else {
                std::string nid, nlbl, nsh, ncls;
                bool hasDef = false;
                parseFlowNodeToken(tok, nid, nlbl, nsh, ncls, hasDef);
                int idx = static_cast<int>(items.size());
                BItem bi; bi.id = nid; bi.label = nlbl; bi.shape = nsh; bi.span = span;
                items.push_back(bi);
                itemMap[nid] = idx;
            }
        }
    }

    int cols = columns > 0 ? columns : std::max(1, static_cast<int>(items.size()));
    double cellW = 130.0, cellH = 54.0, gap = 16.0, pad = 24.0;
    std::vector<std::pair<double, double>> centers(items.size());
    std::string body;
    int col = 0, row = 0;
    for (size_t i = 0; i < items.size(); i++) {
        int sp = std::min(cols, items[i].span);
        if (col + sp > cols) { row++; col = 0; }
        double x = pad + col * (cellW + gap);
        double y = pad + row * (cellH + gap);
        double w = sp * cellW + (sp - 1) * gap;
        double cx = x + w / 2.0, cy = y + cellH / 2.0;
        centers[i] = {cx, cy};
        if (!items[i].isSpace) {
            ResolvedStyle rs;
            body += "<g class=\"pele-block\" data-id=\"" + esc(items[i].id) + "\">" +
                    drawShape(items[i].shape, cx, cy, w, cellH, rs) +
                    labelSvg(cx, cy, layoutLabel(items[i].label, fontSize, false), fontSize) + "</g>";
        }
        col += sp;
        if (col >= cols) { row++; col = 0; }
    }
    int totalRows = row + (col > 0 ? 1 : 0);
    double smallFont = std::round(fontSize * 0.85);
    for (const auto& lk : links) {
        if (itemMap.count(lk.src) && itemMap.count(lk.dst)) {
            auto [x1, y1] = centers[itemMap[lk.src]];
            auto [x2, y2] = centers[itemMap[lk.dst]];
            double ang = std::atan2(y2 - y1, x2 - x1);
            body += "<g class=\"pele-edge\"><line x1=\"" + num(x1) + "\" y1=\"" + num(y1) + "\" x2=\"" + num(x2) + "\" y2=\"" + num(y2) +
                    "\" stroke=\"var(--_l)\" stroke-width=\"var(--_w)\"/>" + marker("arrow_point", x2, y2, ang);
            if (!lk.label.empty()) {
                body += edgeLabelSvg((x1 + x2) / 2.0, (y1 + y2) / 2.0, layoutLabel(lk.label, smallFont, false), smallFont);
            }
            body += "</g>";
        }
    }

    RenderResult r;
    r.type = "block";
    r.width = std::ceil(pad * 2.0 + cols * cellW + (cols - 1) * gap);
    r.height = std::ceil(pad * 2.0 + std::max(1, totalRows) * cellH + std::max(0, totalRows - 1) * gap);
    r.svg = svgDocument("block", r.width, r.height, fontSize, body);
    return r;
}

inline std::string drawArchIcon(const std::string& icon, double cx, double cy) {
    std::string ic = toLower(icon);
    std::string inner;
    if (ic.find("database") != std::string::npos || ic.find("disk") != std::string::npos) {
        inner = "<ellipse cx=\"0\" cy=\"-7\" rx=\"10\" ry=\"4\"/><path d=\"M-10 -7V7A10 4 0 0 0 10 7V-7\"/><path d=\"M-10 0A10 4 0 0 0 10 0\"/>";
    } else if (ic.find("cloud") != std::string::npos) {
        inner = "<path d=\"M-8 6H8A6 6 0 0 0 9 -5A8 8 0 0 0 -6 -6A6 6 0 0 0 -8 6Z\"/>";
    } else if (ic.find("internet") != std::string::npos || ic.find("globe") != std::string::npos) {
        inner = "<circle cx=\"0\" cy=\"0\" r=\"10\"/><ellipse cx=\"0\" cy=\"0\" rx=\"5\" ry=\"10\"/><line x1=\"-10\" y1=\"0\" x2=\"10\" y2=\"0\"/>";
    } else {
        inner = "<rect x=\"-10\" y=\"-8\" width=\"20\" height=\"7\" rx=\"1.5\"/><rect x=\"-10\" y=\"1\" width=\"20\" height=\"7\" rx=\"1.5\"/>"
                "<circle cx=\"-6\" cy=\"-4.5\" r=\"1\" fill=\"var(--_c)\"/><circle cx=\"-6\" cy=\"4.5\" r=\"1\" fill=\"var(--_c)\"/>";
    }
    return "<g transform=\"translate(" + num(cx) + "," + num(cy) + ")\" fill=\"none\" stroke=\"var(--_c)\" stroke-width=\"var(--_hw)\" stroke-linecap=\"round\" stroke-linejoin=\"round\">" + inner + "</g>";
}

inline RenderResult renderArchitecture(const std::string& code, const RenderOptions& opts) {
    double fontSize = opts.fontSize;
    double smallFont = std::round(fontSize * 0.85);
    struct ArchNode { std::string id, icon, label, group; bool isJunction = false; };
    std::vector<ArchNode> anodes;
    std::map<std::string, int> nodeMap;
    std::vector<LayoutSubgraph> subgraphs;
    std::map<std::string, int> sgMap;
    std::vector<LayoutEdge> edges;

    for (const auto& rawLine : splitLines(code)) {
        std::string line = trim(rawLine);
        if (line.empty()) continue;
        std::string low = toLower(line);
        if (low.rfind("architecture", 0) == 0) continue;
        if (low.rfind("group ", 0) == 0) {
            std::string rest = trim(line.substr(6));
            size_t ob = rest.find('['), cb = rest.find(']');
            std::string gid = rest.substr(0, rest.find_first_of("([ "));
            std::string gtitle = (ob != std::string::npos && cb != std::string::npos) ? rest.substr(ob + 1, cb - ob - 1) : gid;
            int sgIdx = static_cast<int>(subgraphs.size());
            LayoutSubgraph sg; sg.id = gid; sg.title = gtitle;
            subgraphs.push_back(sg);
            sgMap[gid] = sgIdx;
            continue;
        }
        if (low.rfind("service ", 0) == 0 || low.rfind("junction ", 0) == 0) {
            bool isJ = (low.rfind("junction ", 0) == 0);
            std::string rest = trim(line.substr(isJ ? 9 : 8));
            ArchNode an;
            an.isJunction = isJ;
            size_t lp = rest.find('('), rp = rest.find(')');
            size_t ob = rest.find('['), cb = rest.find(']');
            an.id = trim(rest.substr(0, std::min({lp, ob, rest.find(' ')})));
            if (lp != std::string::npos && rp != std::string::npos) an.icon = rest.substr(lp + 1, rp - lp - 1);
            if (ob != std::string::npos && cb != std::string::npos) an.label = rest.substr(ob + 1, cb - ob - 1);
            else an.label = an.id;
            size_t inPos = toLower(rest).find(" in ");
            if (inPos != std::string::npos) an.group = trim(rest.substr(inPos + 4));
            int idx = static_cast<int>(anodes.size());
            anodes.push_back(an);
            nodeMap[an.id] = idx;
            if (!an.group.empty() && sgMap.count(an.group)) subgraphs[sgMap[an.group]].nodeIndices.push_back(idx);
            continue;
        }
        size_t dashPos = line.find("--");
        if (dashPos != std::string::npos) {
            std::string lhs = trim(line.substr(0, dashPos));
            std::string rhs = trim(line.substr(dashPos + 2));
            if (!rhs.empty() && rhs.front() == '>') rhs = trim(rhs.substr(1));
            auto cleanId = [](const std::string& s) {
                size_t col = s.find(':');
                std::string id = (col != std::string::npos) ? s.substr(0, col) : s;
                if (!id.empty() && (id.back() == '<' || id.back() == '>')) id.pop_back();
                if (!id.empty() && (id.front() == '<' || id.front() == '>')) id = id.substr(1);
                return trim(id);
            };
            std::string u = cleanId(lhs), v = cleanId(rhs);
            if (nodeMap.count(u) && nodeMap.count(v)) {
                LayoutEdge le;
                le.src = nodeMap[u];
                le.dst = nodeMap[v];
                le.arrowStart = (lhs.find('<') != std::string::npos) ? "arrow_point" : "none";
                le.arrowEnd = (line.find('>') != std::string::npos) ? "arrow_point" : "none";
                edges.push_back(le);
            }
        }
    }

    std::vector<LayoutNode> lnodes(anodes.size());
    for (size_t i = 0; i < anodes.size(); i++) {
        lnodes[i].id = anodes[i].id;
        lnodes[i].w = anodes[i].isJunction ? 14.0 : 96.0;
        lnodes[i].h = anodes[i].isJunction ? 14.0 : 72.0;
        if (!anodes[i].group.empty() && sgMap.count(anodes[i].group)) lnodes[i].parentSubgraph = sgMap[anodes[i].group];
    }

    LayoutResult lres = layoutGraph(lnodes, edges, subgraphs, "LR", 44.0, 64.0);
    std::string body;
    for (const auto& sg : subgraphs) {
        if (sg.w <= 0 || sg.h <= 0) continue;
        body += "<g class=\"pele-arch-group\"><rect x=\"" + num(sg.x) + "\" y=\"" + num(sg.y) + "\" width=\"" + num(sg.w) + "\" height=\"" + num(sg.h) +
                "\" rx=\"8\" fill=\"var(--_a)\" fill-opacity=\"0.35\" stroke=\"var(--_b)\" stroke-width=\"var(--_w)\" stroke-dasharray=\"5 3\"/>" +
                labelSvg(sg.x + 12.0, sg.y + 14.0, layoutLabel(sg.title, smallFont, true), smallFont, "start", "var(--_m)") + "</g>";
    }
    for (const auto& e : edges) {
        if (e.route.size() < 2) continue;
        double eTr = (e.arrowEnd != "none") ? 7.0 : 0.0;
        EdgePath ep = routePoints(e.route, opts.curve, 0.0, eTr, 1);
        std::string eMark;
        if (e.arrowEnd != "none") {
            eMark = marker("arrow_point", ep.ex, ep.ey, ep.edx, ep.edy);
        }
        body += "<g class=\"pele-edge\"><path d=\"" + ep.d + "\" fill=\"none\" stroke=\"var(--_l)\"/>" + eMark + "</g>";
    }
    for (size_t i = 0; i < anodes.size(); i++) {
        const auto& an = anodes[i];
        const auto& n = lnodes[i];
        if (an.isJunction) {
            body += "<circle class=\"pele-arch-junction\" cx=\"" + num(n.x) + "\" cy=\"" + num(n.y) + "\" r=\"5\" fill=\"var(--_l)\"/>";
        } else {
            body += "<g class=\"pele-arch-service\" data-id=\"" + esc(an.id) + "\">"
                    "<rect x=\"" + num(n.x - 24) + "\" y=\"" + num(n.y - 28) + "\" width=\"48\" height=\"42\" rx=\"8\" fill=\"var(--_s)\" stroke=\"var(--_b)\" stroke-width=\"var(--_w)\"/>" +
                    drawArchIcon(an.icon, n.x, n.y - 7.0) +
                    labelSvg(n.x, n.y + 26.0, layoutLabel(an.label, smallFont, false), smallFont) + "</g>";
        }
    }

    RenderResult r;
    r.type = "architecture";
    r.width = std::ceil(lres.width);
    r.height = std::ceil(lres.height);
    r.svg = svgDocument("architecture", r.width, r.height, fontSize, body);
    return r;
}

inline RenderResult renderRequirement(const std::string& code, const RenderOptions& opts) {
    double fontSize = opts.fontSize;
    double smallFont = std::round(fontSize * 0.82);
    struct ReqBox {
        std::string name;
        std::string stereo;
        std::vector<std::pair<std::string, std::string>> props;
    };
    std::vector<ReqBox> boxes;
    std::map<std::string, int> boxMap;
    std::vector<LayoutEdge> edges;

    int active = -1;
    for (const auto& rawLine : splitLines(code)) {
        std::string line = trim(rawLine);
        if (line.empty()) continue;
        std::string low = toLower(line);
        if (low.rfind("requirementdiagram", 0) == 0) continue;
        if (active >= 0) {
            if (line == "}") { active = -1; continue; }
            size_t col = line.find(':');
            if (col != std::string::npos) {
                boxes[active].props.push_back({trim(line.substr(0, col)), unquote(trim(line.substr(col + 1)))});
            }
            continue;
        }
        if (line.back() == '{') {
            std::string head = trim(line.substr(0, line.size() - 1));
            size_t sp = head.find_first_of(" \t");
            ReqBox rb;
            rb.stereo = (sp != std::string::npos) ? trim(head.substr(0, sp)) : "Requirement";
            rb.name = (sp != std::string::npos) ? unquote(trim(head.substr(sp + 1))) : head;
            active = static_cast<int>(boxes.size());
            boxMap[rb.name] = active;
            boxes.push_back(rb);
            continue;
        }
        // Relationship: `A - satisfies -> B` or `A <- copies - B`
        size_t dashPos = line.find(" - ");
        if (dashPos != std::string::npos) {
            size_t arrowPos = line.find("->", dashPos);
            if (arrowPos != std::string::npos) {
                std::string u = unquote(trim(line.substr(0, dashPos)));
                std::string rel = trim(line.substr(dashPos + 3, arrowPos - (dashPos + 3)));
                std::string v = unquote(trim(line.substr(arrowPos + 2)));
                if (boxMap.count(u) && boxMap.count(v)) {
                    LayoutEdge le; le.src = boxMap[u]; le.dst = boxMap[v];
                    le.label = "\xC2\xAB" + rel + "\xC2\xBB";
                    le.stroke = "dotted";
                    TextBlock tb = layoutLabel(le.label, smallFont, false);
                    le.labelW = tb.width + 10.0; le.labelH = tb.height + 6.0;
                    edges.push_back(le);
                }
            }
        }
    }

    std::vector<LayoutNode> lnodes(boxes.size());
    std::vector<double> reqHeaderHs(boxes.size(), 44.0);
    for (size_t i = 0; i < boxes.size(); i++) {
        TextBlock ntb = layoutLabel(boxes[i].name, fontSize, true);
        double hH = std::max(44.0, 24.0 + ntb.height + 8.0);
        reqHeaderHs[i] = hH;
        double propsH = 0.0;
        double maxW = std::max(180.0, ntb.width + 24.0);
        for (const auto& pr : boxes[i].props) {
            TextBlock vtb = layoutLabel(pr.second, smallFont, false);
            propsH += std::max(18.0, vtb.height + 4.0);
            maxW = std::max(maxW, 94.0 + vtb.width + 12.0);
        }
        lnodes[i].id = boxes[i].name;
        lnodes[i].w = maxW;
        lnodes[i].h = hH + 4.0 + propsH + 10.0;
    }
    std::vector<LayoutSubgraph> emptySgs;
    LayoutResult lres = layoutGraph(lnodes, edges, emptySgs, "TD", 44.0, 56.0);
    std::string body;
    for (const auto& e : edges) {
        if (e.route.size() < 2) continue;
        EdgePath ep = routePoints(e.route, opts.curve, 0.0, 7.0, 0);
        std::string eMark = marker("arrow_point", ep.ex, ep.ey, ep.edx, ep.edy);
        body += "<g class=\"pele-edge\"><path d=\"" + ep.d + "\" fill=\"none\" stroke=\"var(--_l)\" stroke-dasharray=\"5 3\"/>" + eMark +
                edgeLabelSvg(e.labelX, e.labelY, layoutLabel(e.label, smallFont, false), smallFont) + "</g>";
    }
    for (size_t i = 0; i < boxes.size(); i++) {
        const auto& bx = boxes[i];
        const auto& n = lnodes[i];
        double hH = reqHeaderHs[i];
        double x = n.x - n.w / 2.0, y = n.y - n.h / 2.0;
        std::string capStereo = bx.stereo;
        if (!capStereo.empty()) capStereo[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(capStereo[0])));
        TextBlock ntb = layoutLabel(bx.name, fontSize, true);
        body += "<g class=\"pele-requirement\" data-id=\"" + esc(bx.name) + "\">"
                "<rect x=\"" + num(x) + "\" y=\"" + num(y) + "\" width=\"" + num(n.w) + "\" height=\"" + num(n.h) +
                "\" rx=\"var(--_r)\" fill=\"var(--_s)\" stroke=\"var(--_b)\" stroke-width=\"var(--_w)\"/>" +
                labelSvg(n.x, y + 14.0, layoutLabel("\xC2\xAB" + capStereo + "\xC2\xBB", smallFont, false), smallFont, "middle", "var(--_m)") +
                labelSvg(n.x, y + 22.0 + (hH - 22.0) / 2.0, ntb, fontSize) +
                "<line x1=\"" + num(x) + "\" y1=\"" + num(y + hH) + "\" x2=\"" + num(x + n.w) + "\" y2=\"" + num(y + hH) + "\" stroke=\"var(--_b)\" stroke-width=\"var(--_w)\"/>";
        double ry = y + hH + 6.0;
        for (const auto& pr : bx.props) {
            std::string pk = toLower(pr.first);
            std::string kDisp = (pk == "id") ? "ID" : (pk == "text") ? "Text" : (pk == "risk") ? "Risk" : (pk == "verifymethod") ? "Verification" : (pk == "type") ? "Type" : pr.first;
            std::string vDisp = pr.second;
            if ((pk == "risk" || pk == "verifymethod") && !vDisp.empty()) {
                vDisp[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(vDisp[0])));
            }
            TextBlock vtb = layoutLabel(vDisp, smallFont, false);
            double rowH = std::max(18.0, vtb.height + 4.0);
            double cy = ry + rowH / 2.0;
            body += "<text x=\"" + num(x + 10.0) + "\" y=\"" + num(cy) + "\" dominant-baseline=\"central\" font-size=\"" + num(smallFont) + "\" fill=\"var(--_m)\">" + escText(kDisp) + "</text>" +
                    labelSvg(x + 84.0, cy, vtb, smallFont, "start");
            ry += rowH;
        }
        body += "</g>";
    }

    RenderResult r;
    r.type = "requirement";
    r.width = std::ceil(lres.width);
    r.height = std::ceil(lres.height);
    r.svg = svgDocument("requirement", r.width, r.height, fontSize, body);
    return r;
}

inline RenderResult renderUsecase(const std::string& code, const RenderOptions& opts) {
    double fontSize = opts.fontSize;
    double smallFont = std::round(fontSize * 0.82);
    struct UcNode { std::string id, label; bool isActor = false; int boundary = -1; };
    std::vector<UcNode> unodes;
    std::map<std::string, int> umap;
    std::vector<LayoutSubgraph> subgraphs;
    std::vector<int> sgStack;
    std::vector<LayoutEdge> edges;

    auto ensureUc = [&](const std::string& id, const std::string& lbl, bool isActor) -> int {
        auto it = umap.find(id);
        if (it != umap.end()) return it->second;
        int idx = static_cast<int>(unodes.size());
        UcNode u; u.id = id; u.label = lbl.empty() ? id : lbl; u.isActor = isActor;
        if (!sgStack.empty() && !isActor) {
            u.boundary = sgStack.back();
            subgraphs[sgStack.back()].nodeIndices.push_back(idx);
        }
        unodes.push_back(u);
        umap[id] = idx;
        return idx;
    };

    for (const auto& rawLine : splitLines(code)) {
        std::string line = trim(rawLine);
        if (line.empty()) continue;
        std::string low = toLower(line);
        if ((low.rfind("usecase", 0) == 0 && line.find(' ') == std::string::npos) || low.rfind("direction ", 0) == 0) continue;
        if (line == "}" || low == "end") { if (!sgStack.empty()) sgStack.pop_back(); continue; }
        if (low.rfind("boundary ", 0) == 0 || low.rfind("systemboundary ", 0) == 0 || low.rfind("system ", 0) == 0 || low.rfind("rectangle ", 0) == 0 || low.rfind("package ", 0) == 0) {
            std::string rest = trim(line.substr(line.find(' ') + 1));
            if (!rest.empty() && rest.back() == '{') rest = trim(rest.substr(0, rest.size() - 1));
            size_t asPos = toLower(rest).find(" as ");
            if (asPos != std::string::npos) rest = trim(rest.substr(0, asPos));
            std::string bid = unquote(rest);
            int sgIdx = static_cast<int>(subgraphs.size());
            LayoutSubgraph sg; sg.id = bid; sg.title = bid;
            subgraphs.push_back(sg);
            sgStack.push_back(sgIdx);
            continue;
        }
        if (low.rfind("actor ", 0) == 0) {
            std::string rest = trim(line.substr(6));
            size_t lp = rest.find('('), rp = rest.rfind(')');
            if (lp != std::string::npos && rp != std::string::npos && rp > lp) {
                std::string id = unquote(trim(rest.substr(0, lp)));
                std::string lbl = unquote(trim(rest.substr(lp + 1, rp - lp - 1)));
                ensureUc(id.empty() ? lbl : id, lbl, true);
            } else {
                size_t asPos = toLower(rest).find(" as ");
                std::string lbl = (asPos != std::string::npos) ? unquote(trim(rest.substr(0, asPos))) : unquote(rest);
                std::string id = (asPos != std::string::npos) ? unquote(trim(rest.substr(asPos + 4))) : lbl;
                ensureUc(id, lbl, true);
            }
            continue;
        }
        if (low.rfind("usecase ", 0) == 0 || (line.find('(') != std::string::npos && line.back() == ')' && line.find("--") == std::string::npos && line.find("..") == std::string::npos && line.find("->") == std::string::npos)) {
            std::string rest = (low.rfind("usecase ", 0) == 0) ? trim(line.substr(8)) : line;
            size_t lp = rest.find('('), rp = rest.rfind(')');
            if (lp != std::string::npos && lp > 0 && rp == rest.size() - 1 && toLower(rest).find(" as ") == std::string::npos) {
                std::string id = unquote(trim(rest.substr(0, lp)));
                std::string lbl = unquote(trim(rest.substr(lp + 1, rp - lp - 1)));
                ensureUc(id, lbl, false);
                continue;
            }
            size_t asPos = toLower(rest).find(" as ");
            std::string lbl = (asPos != std::string::npos) ? trim(rest.substr(0, asPos)) : rest;
            if (!lbl.empty() && lbl.front() == '(' && lbl.back() == ')') lbl = lbl.substr(1, lbl.size() - 2);
            lbl = unquote(lbl);
            std::string id = (asPos != std::string::npos) ? unquote(trim(rest.substr(asPos + 4))) : lbl;
            ensureUc(id, lbl, false);
            continue;
        }
        // Edges: `A --> (Login) : <<include>>`
        for (const char* op : {".>", "..>", "<..", "-->", "--", "->"}) {
            size_t pos = line.find(op);
            if (pos != std::string::npos) {
                std::string lhs = trim(line.substr(0, pos));
                std::string rhsFull = trim(line.substr(pos + std::strlen(op)));
                std::string label;
                size_t col = rhsFull.find(':');
                if (col != std::string::npos) {
                    label = trim(rhsFull.substr(col + 1));
                    rhsFull = trim(rhsFull.substr(0, col));
                }
                auto resolveRef = [&](std::string s) -> int {
                    bool isUc = (s.front() == '(' && s.back() == ')');
                    bool isAct = (s.front() == ':' && s.back() == ':');
                    if (isUc || isAct) s = s.substr(1, s.size() - 2);
                    s = unquote(trim(s));
                    if (umap.count(s)) return umap[s];
                    return ensureUc(s, s, !isUc && isAct);
                };
                LayoutEdge le;
                le.src = resolveRef(lhs);
                le.dst = resolveRef(rhsFull);
                le.stroke = (std::string(op).find('.') != std::string::npos) ? "dotted" : "solid";
                le.arrowEnd = (std::string(op).find('>') != std::string::npos) ? "arrow_point" : "none";
                le.label = label;
                if (!label.empty()) {
                    TextBlock tb = layoutLabel(label, smallFont, false);
                    le.labelW = tb.width + 10.0; le.labelH = tb.height + 6.0;
                }
                edges.push_back(le);
                break;
            }
        }
    }

    std::vector<LayoutNode> lnodes(unodes.size());
    for (size_t i = 0; i < unodes.size(); i++) {
        TextBlock tb = layoutLabel(unodes[i].label, fontSize, false);
        lnodes[i].id = unodes[i].id;
        lnodes[i].parentSubgraph = unodes[i].boundary;
        if (unodes[i].isActor) {
            lnodes[i].w = std::max(54.0, tb.width + 12.0);
            lnodes[i].h = 56.0 + tb.height;
        } else {
            lnodes[i].w = std::max(110.0, tb.width * 1.35 + 28.0);
            lnodes[i].h = std::max(44.0, tb.height * 1.4 + 20.0);
        }
    }

    LayoutResult lres = layoutGraph(lnodes, edges, subgraphs, "LR", 36.0, 64.0);
    std::string body;
    for (const auto& sg : subgraphs) {
        if (sg.w <= 0 || sg.h <= 0) continue;
        body += "<g class=\"pele-usecase-boundary\"><rect x=\"" + num(sg.x) + "\" y=\"" + num(sg.y) + "\" width=\"" + num(sg.w) + "\" height=\"" + num(sg.h) +
                "\" rx=\"8\" fill=\"var(--_a)\" fill-opacity=\"0.35\" stroke=\"var(--_b)\" stroke-width=\"var(--_w)\"/>" +
                labelSvg(sg.x + sg.w / 2.0, sg.y + 16.0, layoutLabel(sg.title, fontSize * 0.92, true), fontSize * 0.92) + "</g>";
    }
    for (const auto& e : edges) {
        if (e.route.size() < 2) continue;
        double eTr = (e.arrowEnd != "none") ? 7.0 : 0.0;
        EdgePath ep = routePoints(e.route, opts.curve, 0.0, eTr, 1);
        std::string eMark;
        if (e.arrowEnd != "none") {
            eMark = marker("arrow_point", ep.ex, ep.ey, ep.edx, ep.edy);
        }
        std::string dash = (e.stroke == "dotted") ? " stroke-dasharray=\"5 3\"" : "";
        body += "<g class=\"pele-edge\"><path d=\"" + ep.d + "\" fill=\"none\" stroke=\"var(--_l)\"" + dash + "/>" + eMark;
        if (!e.label.empty()) body += edgeLabelSvg(e.labelX, e.labelY, layoutLabel(e.label, smallFont, false), smallFont);
        body += "</g>";
    }
    for (size_t i = 0; i < unodes.size(); i++) {
        const auto& u = unodes[i];
        const auto& n = lnodes[i];
        TextBlock tb = layoutLabel(u.label, fontSize, false);
        if (u.isActor) {
            body += "<g class=\"pele-usecase-actor\" data-id=\"" + esc(u.id) + "\">" +
                    drawSeqActorBox("actor", n.x, n.y - n.h / 2.0, n.w, n.h, tb, fontSize, "var(--_s)", "var(--_l)") + "</g>";
        } else {
            body += "<g class=\"pele-usecase\" data-id=\"" + esc(u.id) + "\">"
                    "<ellipse cx=\"" + num(n.x) + "\" cy=\"" + num(n.y) + "\" rx=\"" + num(n.w / 2.0) + "\" ry=\"" + num(n.h / 2.0) +
                    "\" fill=\"var(--_s)\" stroke=\"var(--_b)\" stroke-width=\"var(--_w)\"/>" +
                    labelSvg(n.x, n.y, tb, fontSize) + "</g>";
        }
    }

    RenderResult r;
    r.type = "usecase";
    r.width = std::ceil(lres.width);
    r.height = std::ceil(lres.height);
    r.svg = svgDocument("usecase", r.width, r.height, fontSize, body);
    return r;
}

// ---------------------------------------------------------------------------
// 13. Pie Chart Parser & Renderer (Matches src/diagrams/pie/render.ts)
// ---------------------------------------------------------------------------

inline RenderResult renderPie(const std::string& code, const RenderOptions& opts) {
    double fontSize = opts.fontSize;
    double titleFont = std::round(fontSize * 1.15);
    double legendFont = std::round(fontSize * 0.9);
    double pctFont = std::round(fontSize * 0.82);

    bool showData = false;
    std::string title;
    struct PieSlice { std::string label; double value = 0.0; };
    std::vector<PieSlice> slices;

    for (const auto& rawLine : splitLines(code)) {
        std::string line = trim(rawLine);
        if (line.empty()) continue;
        std::string low = toLower(line);
        if (low.rfind("pie", 0) == 0) {
            if (low.find("showdata") != std::string::npos) showData = true;
            size_t tpos = low.find("title ");
            if (tpos != std::string::npos) title = unquote(trim(line.substr(tpos + 6)));
            continue;
        }
        if (low == "showdata") { showData = true; continue; }
        if (low.rfind("title ", 0) == 0) { title = unquote(trim(line.substr(6))); continue; }
        size_t col = line.rfind(':');
        if (col != std::string::npos) {
            std::string lbl = unquote(trim(line.substr(0, col)));
            double val = 0.0;
            try { val = std::stod(trim(line.substr(col + 1))); } catch (...) {}
            if (val > 0.0) slices.push_back({lbl, val});
        }
    }

    double pad = 24.0, radius = 92.0;
    double total = 0.0;
    for (const auto& s : slices) total += s.value;
    if (total <= 0.0) total = 1.0;

    TextBlock titleTb = title.empty() ? TextBlock{} : layoutLabel(title, titleFont, true);
    double titleH = title.empty() ? 0.0 : (titleTb.height + 16.0);

    std::vector<std::string> legendLabels;
    double maxLegendW = 0.0;
    for (const auto& s : slices) {
        std::string text = showData ? (s.label + " [" + num(s.value) + "]") : s.label;
        legendLabels.push_back(text);
        maxLegendW = std::max(maxLegendW, measureTextWidth(text, legendFont, false, false));
    }

    double legendRowH = std::round(legendFont * 1.6);
    double legendW = slices.empty() ? 0.0 : (20.0 + maxLegendW);
    double legendH = slices.size() * legendRowH;
    double chartDia = radius * 2.0;
    double contentH = std::max(chartDia, legendH);
    double cx = pad + radius;
    double cy = pad + titleH + contentH / 2.0;
    double legendX = cx + radius + 32.0;
    double legendStartY = cy - legendH / 2.0;

    std::string body;
    if (!title.empty()) {
        double totalW = pad * 2.0 + chartDia + (slices.empty() ? 0.0 : 32.0 + legendW);
        body += labelSvg(totalW / 2.0, pad + titleTb.height / 2.0, titleTb, titleFont);
    }

    double angle = -kPi / 2.0;
    for (size_t i = 0; i < slices.size(); i++) {
        double frac = slices[i].value / total;
        double sweep = frac * kPi * 2.0;
        double endAngle = angle + sweep;
        std::string col = seriesColor(static_cast<int>(i));
        if (slices.size() == 1 || sweep >= kPi * 2.0 - 1e-4) {
            body += "<circle class=\"pele-slice\" cx=\"" + num(cx) + "\" cy=\"" + num(cy) + "\" r=\"" + num(radius) +
                    "\" fill=\"" + col + "\" stroke=\"var(--_bg)\" stroke-width=\"1.5\"/>";
        } else {
            double x1 = cx + std::cos(angle) * radius, y1 = cy + std::sin(angle) * radius;
            double x2 = cx + std::cos(endAngle) * radius, y2 = cy + std::sin(endAngle) * radius;
            int largeArc = (sweep > kPi) ? 1 : 0;
            body += "<path class=\"pele-slice\" d=\"M" + num(cx) + " " + num(cy) + "L" + num(x1) + " " + num(y1) +
                    "A" + num(radius) + " " + num(radius) + " 0 " + std::to_string(largeArc) + " 1 " + num(x2) + " " + num(y2) +
                    "Z\" fill=\"" + col + "\" stroke=\"var(--_bg)\" stroke-width=\"1.5\"/>";
        }
        int pct = static_cast<int>(std::round(frac * 100.0));
        if (pct >= 5) {
            double mid = angle + sweep / 2.0;
            double lr = (slices.size() == 1) ? 0.0 : radius * 0.64;
            double lx = cx + std::cos(mid) * lr, ly = cy + std::sin(mid) * lr;
            body += "<text x=\"" + num(lx) + "\" y=\"" + num(ly) + "\" text-anchor=\"middle\" dominant-baseline=\"central\" "
                    "font-size=\"" + num(pctFont) + "\" font-weight=\"600\" fill=\"#fff\">" + std::to_string(pct) + "%</text>";
        }
        angle = endAngle;
    }

    body += "<circle cx=\"" + num(cx) + "\" cy=\"" + num(cy) + "\" r=\"" + num(radius) +
            "\" fill=\"none\" stroke=\"var(--_b)\" stroke-width=\"var(--_w)\"/>";

    for (size_t i = 0; i < slices.size(); i++) {
        double ly = legendStartY + i * legendRowH + legendRowH / 2.0;
        std::string col = seriesColor(static_cast<int>(i));
        body += "<g class=\"pele-legend-item\">"
                "<rect x=\"" + num(legendX) + "\" y=\"" + num(ly - 6.0) + "\" width=\"12\" height=\"12\" rx=\"3\" fill=\"" + col + "\"/>"
                "<text x=\"" + num(legendX + 18.0) + "\" y=\"" + num(ly) + "\" dominant-baseline=\"central\" font-size=\"" + num(legendFont) + "\">" +
                escText(legendLabels[i]) + "</text></g>";
    }

    RenderResult r;
    r.type = "pie";
    r.width = std::ceil(pad * 2.0 + chartDia + (slices.empty() ? 0.0 : 32.0 + legendW));
    r.height = std::ceil(pad * 2.0 + titleH + contentH);
    r.svg = svgDocument("pie", r.width, r.height, fontSize, body);
    return r;
}

// ---------------------------------------------------------------------------
// 14. XYChart Parser & Renderer (Matches src/diagrams/xychart/render.ts)
// ---------------------------------------------------------------------------

inline RenderResult renderXyChart(const std::string& code, const RenderOptions& opts) {
    double fontSize = opts.fontSize;
    double titleFont = std::round(fontSize * 1.15);
    double axisFont = std::round(fontSize * 0.82);

    bool horizontal = false;
    std::string title, xAxisTitle, yAxisTitle;
    std::vector<std::string> categories;
    bool hasYRange = false;
    double yMinSpec = 0.0, yMaxSpec = 100.0;
    struct XySeries { std::string kind; std::vector<double> values; };
    std::vector<XySeries> series;

    auto parseBracketList = [](const std::string& s) -> std::vector<std::string> {
        size_t ob = s.find('['), cb = s.rfind(']');
        if (ob == std::string::npos || cb == std::string::npos || cb <= ob) return {};
        return parseCallArgs(s.substr(ob + 1, cb - ob - 1));
    };

    for (const auto& rawLine : splitLines(code)) {
        std::string line = trim(rawLine);
        if (line.empty()) continue;
        std::string low = toLower(line);
        if (low.rfind("xychart", 0) == 0) {
            if (low.find("horizontal") != std::string::npos) horizontal = true;
            continue;
        }
        if (low.rfind("title ", 0) == 0) { title = unquote(trim(line.substr(6))); continue; }
        if (low.rfind("x-axis", 0) == 0) {
            auto items = parseBracketList(line);
            if (!items.empty()) categories = items;
            size_t ob = line.find('[');
            std::string head = trim(line.substr(6, ob == std::string::npos ? std::string::npos : ob - 6));
            if (!head.empty() && head.find("-->") == std::string::npos) xAxisTitle = unquote(head);
            continue;
        }
        if (low.rfind("y-axis", 0) == 0) {
            std::string rest = trim(line.substr(6));
            size_t arrow = rest.find("-->");
            if (arrow != std::string::npos) {
                std::string left = trim(rest.substr(0, arrow));
                std::string right = trim(rest.substr(arrow + 3));
                size_t sp = left.find_last_of(" \t\"");
                std::string minStr = (sp != std::string::npos) ? trim(left.substr(sp + 1)) : left;
                if (sp != std::string::npos) yAxisTitle = unquote(trim(left.substr(0, sp + 1)));
                try {
                    yMinSpec = std::stod(minStr);
                    yMaxSpec = std::stod(right);
                    hasYRange = true;
                } catch (...) {}
            } else {
                yAxisTitle = unquote(rest);
            }
            continue;
        }
        if (low.rfind("bar", 0) == 0 || low.rfind("line", 0) == 0) {
            std::string kind = (low.rfind("bar", 0) == 0) ? "bar" : "line";
            auto items = parseBracketList(line);
            XySeries s; s.kind = kind;
            for (const auto& it : items) {
                try { s.values.push_back(std::stod(it)); } catch (...) {}
            }
            if (!s.values.empty()) series.push_back(s);
        }
    }

    size_t maxCount = categories.size();
    double dataMin = 0.0, dataMax = 10.0;
    bool firstVal = true;
    for (const auto& s : series) {
        maxCount = std::max(maxCount, s.values.size());
        for (double v : s.values) {
            if (firstVal) { dataMin = std::min(0.0, v); dataMax = v; firstVal = false; }
            else { dataMin = std::min(dataMin, v); dataMax = std::max(dataMax, v); }
        }
    }
    if (maxCount == 0) maxCount = 1;
    if (categories.empty()) {
        for (size_t i = 0; i < maxCount; i++) categories.push_back(std::to_string(i + 1));
    }

    double yMin = hasYRange ? yMinSpec : std::min(0.0, dataMin);
    double yMax = hasYRange ? yMaxSpec : dataMax;
    if (yMax <= yMin) yMax = yMin + 10.0;
    double step = std::pow(10.0, std::floor(std::log10((yMax - yMin) / 5.0)));
    double norm = ((yMax - yMin) / 5.0) / step;
    double niceStep = (norm <= 1.0 ? 1.0 : norm <= 2.0 ? 2.0 : norm <= 5.0 ? 5.0 : 10.0) * step;
    yMin = std::floor(yMin / niceStep) * niceStep;
    yMax = std::ceil(yMax / niceStep) * niceStep;

    double pad = 24.0;
    double titleH = title.empty() ? 0.0 : 32.0;
    double plotW = std::max(320.0, maxCount * 58.0);
    double plotH = 220.0;
    double leftAxisW = 54.0 + (yAxisTitle.empty() ? 0.0 : 22.0);
    double bottomAxisH = 34.0 + (xAxisTitle.empty() ? 0.0 : 22.0);
    double plotX = pad + leftAxisW;
    double plotY = pad + titleH;
    double totalW = plotX + plotW + pad;
    double totalH = plotY + plotH + bottomAxisH + pad;

    std::string body;
    if (!title.empty()) {
        body += labelSvg(totalW / 2.0, pad + 10.0, layoutLabel(title, titleFont, true), titleFont);
    }

    // Grid & ticks
    body += "<g class=\"pele-grid\">";
    for (double tv = yMin; tv <= yMax + niceStep * 0.01; tv += niceStep) {
        double frac = (tv - yMin) / (yMax - yMin);
        if (!horizontal) {
            double y = plotY + plotH - frac * plotH;
            body += "<line x1=\"" + num(plotX) + "\" y1=\"" + num(y) + "\" x2=\"" + num(plotX + plotW) + "\" y2=\"" + num(y) +
                    "\" stroke=\"var(--_b)\" stroke-width=\"1\" stroke-opacity=\"0.35\"/>"
                    "<text x=\"" + num(plotX - 8.0) + "\" y=\"" + num(y) + "\" text-anchor=\"end\" dominant-baseline=\"central\" "
                    "font-size=\"" + num(axisFont) + "\" fill=\"var(--_m)\">" + num(tv) + "</text>";
        } else {
            double x = plotX + frac * plotW;
            body += "<line x1=\"" + num(x) + "\" y1=\"" + num(plotY) + "\" x2=\"" + num(x) + "\" y2=\"" + num(plotY + plotH) +
                    "\" stroke=\"var(--_b)\" stroke-width=\"1\" stroke-opacity=\"0.35\"/>"
                    "<text x=\"" + num(x) + "\" y=\"" + num(plotY + plotH + 16.0) + "\" text-anchor=\"middle\" dominant-baseline=\"central\" "
                    "font-size=\"" + num(axisFont) + "\" fill=\"var(--_m)\">" + num(tv) + "</text>";
        }
    }
    body += "</g>";

    // Category labels
    double bandSpan = (!horizontal ? plotW : plotH) / maxCount;
    for (size_t i = 0; i < categories.size(); i++) {
        if (!horizontal) {
            double cx = plotX + (i + 0.5) * bandSpan;
            body += "<text x=\"" + num(cx) + "\" y=\"" + num(plotY + plotH + 16.0) + "\" text-anchor=\"middle\" dominant-baseline=\"central\" "
                    "font-size=\"" + num(axisFont) + "\" fill=\"var(--_m)\">" + escText(categories[i]) + "</text>";
        } else {
            double cy = plotY + (i + 0.5) * bandSpan;
            body += "<text x=\"" + num(plotX - 8.0) + "\" y=\"" + num(cy) + "\" text-anchor=\"end\" dominant-baseline=\"central\" "
                    "font-size=\"" + num(axisFont) + "\" fill=\"var(--_m)\">" + escText(categories[i]) + "</text>";
        }
    }

    // Bars
    std::vector<size_t> barIndices;
    for (size_t si = 0; si < series.size(); si++) if (series[si].kind == "bar") barIndices.push_back(si);
    if (!barIndices.empty()) {
        double groupSpan = bandSpan * 0.68;
        double barW = groupSpan / barIndices.size();
        double zeroFrac = std::clamp((0.0 - yMin) / (yMax - yMin), 0.0, 1.0);
        for (size_t bi = 0; bi < barIndices.size(); bi++) {
            size_t si = barIndices[bi];
            std::string col = seriesColor(static_cast<int>(si));
            body += "<g class=\"pele-bars\" fill=\"" + col + "\">";
            for (size_t i = 0; i < series[si].values.size(); i++) {
                double v = series[si].values[i];
                double vFrac = std::clamp((v - yMin) / (yMax - yMin), 0.0, 1.0);
                if (!horizontal) {
                    double bx = plotX + (i + 0.5) * bandSpan - groupSpan / 2.0 + bi * barW;
                    double y0 = plotY + plotH - zeroFrac * plotH;
                    double y1 = plotY + plotH - vFrac * plotH;
                    body += "<rect x=\"" + num(bx) + "\" y=\"" + num(std::min(y0, y1)) + "\" width=\"" + num( std::max(2.0, barW - 2.0) ) +
                            "\" height=\"" + num(std::abs(y1 - y0)) + "\" rx=\"2\"/>";
                } else {
                    double by = plotY + (i + 0.5) * bandSpan - groupSpan / 2.0 + bi * barW;
                    double x0 = plotX + zeroFrac * plotW;
                    double x1 = plotX + vFrac * plotW;
                    body += "<rect x=\"" + num(std::min(x0, x1)) + "\" y=\"" + num(by) + "\" width=\"" + num(std::abs(x1 - x0)) +
                            "\" height=\"" + num( std::max(2.0, barW - 2.0) ) + "\" rx=\"2\"/>";
                }
            }
            body += "</g>";
        }
    }

    // Lines
    for (size_t si = 0; si < series.size(); si++) {
        if (series[si].kind != "line" || series[si].values.empty()) continue;
        std::string col = seriesColor(static_cast<int>(si));
        std::vector<std::pair<double, double>> pts;
        for (size_t i = 0; i < series[si].values.size(); i++) {
            double vFrac = std::clamp((series[si].values[i] - yMin) / (yMax - yMin), 0.0, 1.0);
            if (!horizontal) pts.push_back({plotX + (i + 0.5) * bandSpan, plotY + plotH - vFrac * plotH});
            else pts.push_back({plotX + vFrac * plotW, plotY + (i + 0.5) * bandSpan});
        }
        body += "<g class=\"pele-line\">"
                "<path d=\"" + routePoints(pts, opts.curve) + "\" fill=\"none\" stroke=\"" + col + "\" stroke-width=\"2.2\" stroke-linecap=\"round\"/>";
        for (const auto& p : pts) {
            body += "<circle cx=\"" + num(p.first) + "\" cy=\"" + num(p.second) + "\" r=\"3.5\" fill=\"" + col + "\" stroke=\"var(--_bg)\" stroke-width=\"1.5\"/>";
        }
        body += "</g>";
    }

    // Axes & axis titles
    body += "<path class=\"pele-axis\" d=\"M" + num(plotX) + " " + num(plotY) + "V" + num(plotY + plotH) + "H" + num(plotX + plotW) +
            "\" fill=\"none\" stroke=\"var(--_b)\" stroke-width=\"var(--_w)\"/>";
    if (!yAxisTitle.empty()) {
        double yx = pad + 6.0, yy = plotY + plotH / 2.0;
        body += "<text x=\"" + num(yx) + "\" y=\"" + num(yy) + "\" text-anchor=\"middle\" dominant-baseline=\"central\" font-size=\"" + num(axisFont) + "\" fill=\"var(--_m)\" transform=\"rotate(-90 " + num(yx) + " " + num(yy) + ")\">" + escText(yAxisTitle) + "</text>";
    }
    if (!xAxisTitle.empty()) {
        body += "<text x=\"" + num(plotX + plotW / 2.0) + "\" y=\"" + num(plotY + plotH + 36.0) + "\" text-anchor=\"middle\" dominant-baseline=\"central\" font-size=\"" + num(axisFont) + "\" fill=\"var(--_m)\">" + escText(xAxisTitle) + "</text>";
    }

    RenderResult r;
    r.type = "xychart";
    r.width = std::ceil(totalW);
    r.height = std::ceil(totalH);
    r.svg = svgDocument("xychart", r.width, r.height, fontSize, body);
    return r;
}

// ---------------------------------------------------------------------------
// 15. Gantt Chart Parser & Renderer (Matches src/diagrams/gantt/render.ts)
// ---------------------------------------------------------------------------

inline RenderResult renderGantt(const std::string& code, const RenderOptions& opts) {
    double fontSize = opts.fontSize;
    double titleFont = std::round(fontSize * 1.15);
    double smallFont = std::round(fontSize * 0.8);

    std::string title, currentSection;
    struct GanttTask {
        std::string id;
        std::string label;
        std::string section;
        double startDay = 0.0;
        double endDay = 1.0;
        bool done = false;
        bool active = false;
        bool crit = false;
        bool milestone = false;
    };
    std::vector<GanttTask> tasks;
    std::map<std::string, int> taskMap;

    auto parseDayNumber = [](const std::string& s, double& outDay) -> bool {
        // Parse YYYY-MM-DD into days since 2024-01-01
        int y = 0, m = 0, d = 0;
        if (sscanf_s(s.c_str(), "%d-%d-%d", &y, &m, &d) == 3) {
            static const int mdays[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
            int mIdx = std::clamp(m - 1, 0, 11);
            outDay = (y - 2024) * 365 + mdays[mIdx] + d;
            return true;
        }
        return false;
    };
    auto parseDurationDays = [](const std::string& s, double& outDur) -> bool {
        if (s.empty()) return false;
        double v = 0.0;
        char unit = 'd';
        if (sscanf_s(s.c_str(), "%lf%c", &v, &unit, 1) >= 1) {
            if (unit == 'w' || unit == 'W') v *= 7.0;
            else if (unit == 'h' || unit == 'H') v /= 24.0;
            outDur = std::max(0.5, v);
            return true;
        }
        return false;
    };

    double cursorDay = 0.0;
    bool hasBaseDate = false;
    int baseY = 2024, baseM = 1, baseD = 1;

    for (const auto& rawLine : splitLines(code)) {
        std::string line = trim(rawLine);
        if (line.empty()) continue;
        std::string low = toLower(line);
        if (low.rfind("gantt", 0) == 0 || low.rfind("dateformat", 0) == 0 || low.rfind("axisformat", 0) == 0 ||
            low.rfind("tickinterval", 0) == 0 || low.rfind("excludes", 0) == 0 || low.rfind("todaymarker", 0) == 0) continue;
        if (low.rfind("title ", 0) == 0) { title = unquote(trim(line.substr(6))); continue; }
        if (low.rfind("section ", 0) == 0) { currentSection = trim(line.substr(8)); continue; }
        size_t col = line.find(':');
        if (col != std::string::npos) {
            GanttTask gt;
            gt.label = trim(line.substr(0, col));
            gt.section = currentSection;
            auto parts = splitChar(line.substr(col + 1), ',');
            std::vector<std::string> rem;
            for (const auto& rawP : parts) {
                std::string p = trim(rawP);
                std::string lp = toLower(p);
                if (lp == "done") gt.done = true;
                else if (lp == "active") gt.active = true;
                else if (lp == "crit") gt.crit = true;
                else if (lp == "milestone") gt.milestone = true;
                else if (!p.empty()) rem.push_back(p);
            }
            double startD = cursorDay;
            double durD = gt.milestone ? 0.2 : 3.0;
            bool startSet = false;
            for (size_t i = 0; i < rem.size(); i++) {
                std::string p = rem[i];
                std::string lp = toLower(p);
                double dayVal = 0.0, durVal = 0.0;
                if (lp.rfind("after ", 0) == 0) {
                    std::string dep = trim(p.substr(6));
                    if (taskMap.count(dep)) startD = tasks[taskMap[dep]].endDay;
                    startSet = true;
                } else if (parseDayNumber(p, dayVal)) {
                    if (!hasBaseDate) {
                        sscanf_s(p.c_str(), "%d-%d-%d", &baseY, &baseM, &baseD);
                        hasBaseDate = true;
                    }
                    if (!startSet) { startD = dayVal; startSet = true; }
                    else durD = std::max(0.5, dayVal - startD);
                } else if (parseDurationDays(p, durVal)) {
                    durD = durVal;
                } else if (gt.id.empty() && i == 0) {
                    gt.id = p;
                }
            }
            gt.startDay = startD;
            gt.endDay = startD + (gt.milestone ? 0.1 : durD);
            cursorDay = gt.endDay;
            int idx = static_cast<int>(tasks.size());
            if (!gt.id.empty()) taskMap[gt.id] = idx;
            tasks.push_back(gt);
        }
    }

    if (tasks.empty()) tasks.push_back({"t1", "Task", "", 0.0, 5.0});
    double minDay = tasks[0].startDay, maxDay = tasks[0].endDay;
    double maxLabelW = 80.0, maxSecW = 0.0;
    for (const auto& t : tasks) {
        minDay = std::min(minDay, t.startDay);
        maxDay = std::max(maxDay, t.endDay);
        maxLabelW = std::max(maxLabelW, measureTextWidth(t.label, smallFont, false, false));
        if (!t.section.empty()) maxSecW = std::max(maxSecW, measureTextWidth(t.section, smallFont, true, false));
    }
    double spanDays = std::max(1.0, maxDay - minDay);
    double pad = 24.0, rowH = 30.0, plotW = 480.0;
    double secColW = maxSecW > 0 ? (maxSecW + 20.0) : 0.0;
    double lblColW = maxLabelW + 20.0;
    double plotX = pad + secColW + lblColW;
    double titleH = title.empty() ? 0.0 : 32.0;
    double plotY = pad + titleH + 22.0;
    double plotH = tasks.size() * rowH;

    std::string body;
    if (!title.empty()) {
        body += labelSvg((plotX + plotW + pad) / 2.0, pad + 10.0, layoutLabel(title, titleFont, true), titleFont);
    }

    // Section bands
    size_t si = 0, secIdx = 0;
    while (si < tasks.size()) {
        size_t sj = si + 1;
        while (sj < tasks.size() && tasks[sj].section == tasks[si].section) sj++;
        double sy = plotY + si * rowH;
        double sh = (sj - si) * rowH;
        if (secIdx % 2 == 1) {
            body += "<rect class=\"pele-section\" x=\"" + num(pad) + "\" y=\"" + num(sy) +
                    "\" width=\"" + num(secColW + lblColW + plotW) + "\" height=\"" + num(sh) +
                    "\" fill=\"var(--_a)\" fill-opacity=\"0.4\"/>";
        }
        if (!tasks[si].section.empty()) {
            body += labelSvg(pad + 8.0, sy + sh / 2.0, layoutLabel(tasks[si].section, smallFont, true), smallFont, "start", "var(--_m)");
        }
        si = sj;
        secIdx++;
    }

    // Vertical grid lines
    int ticks = (hasBaseDate && spanDays <= 16.0) ? std::max(1, static_cast<int>(std::round(spanDays / 2.0))) : 6;
    body += "<g class=\"pele-grid\">";
    for (int k = 0; k <= ticks; k++) {
        double frac = static_cast<double>(k) / ticks;
        double x = plotX + frac * plotW;
        int dayOffset = static_cast<int>(std::round(minDay + frac * spanDays));
        std::string tickLbl;
        if (hasBaseDate) {
            static const int mlen[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
            int y = 2024;
            int remD = dayOffset;
            while (remD <= 0) { y--; remD += 365; }
            while (remD > 365) { y++; remD -= 365; }
            int m = 1;
            for (int mi = 0; mi < 12; mi++) {
                if (remD <= mlen[mi]) { m = mi + 1; break; }
                remD -= mlen[mi];
                m = mi + 2;
            }
            char buf[32];
            snprintf(buf, sizeof(buf), "%04d-%02d-%02d", y, std::clamp(m, 1, 12), std::clamp(remD, 1, 31));
            tickLbl = buf;
        } else {
            tickLbl = "Day " + std::to_string(static_cast<int>(std::round(frac * spanDays)) + 1);
        }
        body += "<line x1=\"" + num(x) + "\" y1=\"" + num(plotY - 4.0) + "\" x2=\"" + num(x) + "\" y2=\"" + num(plotY + plotH) +
                "\" stroke=\"var(--_b)\" stroke-width=\"1\" stroke-opacity=\"0.35\"/>"
                "<text x=\"" + num(x) + "\" y=\"" + num(plotY - 10.0) + "\" text-anchor=\"middle\" font-size=\"" + num(smallFont) + "\" fill=\"var(--_m)\">" +
                escText(tickLbl) + "</text>";
    }
    body += "</g>";

    // Tasks
    for (size_t i = 0; i < tasks.size(); i++) {
        const auto& t = tasks[i];
        double ry = plotY + i * rowH;
        double cy = ry + rowH / 2.0;
        body += "<text x=\"" + num(plotX - 10.0) + "\" y=\"" + num(cy) + "\" text-anchor=\"end\" dominant-baseline=\"central\" font-size=\"" + num(smallFont) + "\">" +
                escText(t.label) + "</text>";
        double x1 = plotX + ((t.startDay - minDay) / spanDays) * plotW;
        double x2 = plotX + ((t.endDay - minDay) / spanDays) * plotW;
        std::string fill = t.crit ? "var(--pele-series-5,#d96b6b)" : t.done ? "var(--_m)" : "var(--_c)";
        std::string op = t.done ? "0.45" : t.active ? "1" : "0.82";
        if (t.milestone) {
            double r = 7.0;
            body += "<polygon class=\"pele-task pele-milestone\" points=\"" + num(x1) + "," + num(cy - r) + " " + num(x1 + r) + "," + num(cy) + " " +
                    num(x1) + "," + num(cy + r) + " " + num(x1 - r) + "," + num(cy) + "\" fill=\"" + fill + "\"/>";
        } else {
            double bw = std::max(8.0, x2 - x1);
            double bh = rowH - 10.0;
            body += "<rect class=\"pele-task\" x=\"" + num(x1) + "\" y=\"" + num(cy - bh / 2.0) + "\" width=\"" + num(bw) + "\" height=\"" + num(bh) +
                    "\" rx=\"4\" fill=\"" + fill + "\" fill-opacity=\"" + op + "\"/>";
        }
    }

    RenderResult r;
    r.type = "gantt";
    r.width = std::ceil(plotX + plotW + pad);
    r.height = std::ceil(plotY + plotH + pad);
    r.svg = svgDocument("gantt", r.width, r.height, fontSize, body);
    return r;
}

// ---------------------------------------------------------------------------
// 16. Mindmap Parser & Renderer (Matches src/diagrams/mindmap/render.ts)
// ---------------------------------------------------------------------------

inline RenderResult renderMindmap(const std::string& code, const RenderOptions& opts) {
    double fontSize = opts.fontSize;
    struct MmNode {
        std::string label;
        std::string shape = "rounded";
        int indent = 0;
        int parent = -1;
        int branchIdx = -1;
        std::vector<int> children;
        TextBlock tb;
        double w = 0.0, h = 0.0, subH = 0.0, x = 0.0, y = 0.0;
    };
    std::vector<MmNode> nodes;

    auto parseMmToken = [](const std::string& raw, std::string& lbl, std::string& shp) {
        std::string s = trim(raw);
        size_t openPos = s.find_first_of("([{}");
        if (openPos != std::string::npos && openPos > 0) s = trim(s.substr(openPos));
        if (s.size() >= 4 && s.rfind("((", 0) == 0 && s.substr(s.size() - 2) == "))") { shp = "circle"; lbl = s.substr(2, s.size() - 4); }
        else if (s.size() >= 4 && s.rfind("{{", 0) == 0 && s.substr(s.size() - 2) == "}}") { shp = "hex"; lbl = s.substr(2, s.size() - 4); }
        else if (s.size() >= 2 && s.front() == '[' && s.back() == ']') { shp = "rect"; lbl = s.substr(1, s.size() - 2); }
        else if (s.size() >= 2 && s.front() == '(' && s.back() == ')') { shp = "rounded"; lbl = s.substr(1, s.size() - 2); }
        else { shp = "rounded"; lbl = s; }
        lbl = unquote(trim(lbl));
    };

    std::vector<int> stack;
    for (const auto& rawLine : splitLines(code)) {
        std::string t = trim(rawLine);
        if (t.empty() || toLower(t).rfind("mindmap", 0) == 0 || toLower(t).rfind("::icon", 0) == 0) continue;
        int ind = 0;
        for (char c : rawLine) {
            if (c == ' ') ind++;
            else if (c == '\t') ind += 4;
            else break;
        }
        MmNode n;
        n.indent = ind;
        parseMmToken(t, n.label, n.shape);
        while (!stack.empty() && nodes[stack.back()].indent >= ind) stack.pop_back();
        int idx = static_cast<int>(nodes.size());
        if (!stack.empty()) {
            n.parent = stack.back();
            nodes[n.parent].children.push_back(idx);
            n.branchIdx = (nodes[n.parent].parent < 0) ? static_cast<int>(nodes[n.parent].children.size() - 1) : nodes[n.parent].branchIdx;
        }
        double f = (n.parent < 0) ? std::round(fontSize * 1.1) : fontSize;
        n.tb = layoutLabel(n.label, f, n.parent < 0, 180.0);
        n.w = n.tb.width + (n.parent < 0 ? 32.0 : 24.0);
        n.h = n.tb.height + (n.parent < 0 ? 20.0 : 14.0);
        nodes.push_back(n);
        stack.push_back(idx);
    }

    if (nodes.empty()) {
        MmNode r; r.label = "root"; r.tb = layoutLabel("root", fontSize, true); r.w = 80.0; r.h = 36.0;
        nodes.push_back(r);
    }

    double vGap = 14.0, hGap = 54.0;
    std::function<double(int)> calcSubH = [&](int u) -> double {
        if (nodes[u].children.empty()) return (nodes[u].subH = nodes[u].h);
        double sum = 0.0;
        for (size_t i = 0; i < nodes[u].children.size(); i++) {
            if (i > 0) sum += vGap;
            sum += calcSubH(nodes[u].children[i]);
        }
        return (nodes[u].subH = std::max(nodes[u].h, sum));
    };
    calcSubH(0);

    // Split root's children between right and left sides
    std::vector<int> rightKids, leftKids;
    double rH = 0.0, lH = 0.0;
    for (int c : nodes[0].children) {
        if (rH <= lH) { rightKids.push_back(c); rH += nodes[c].subH + vGap; }
        else { leftKids.push_back(c); lH += nodes[c].subH + vGap; }
    }

    nodes[0].x = 0.0;
    nodes[0].y = 0.0;

    std::function<void(int, double, int)> placeSubtree = [&](int u, double cy, int dir) {
        nodes[u].y = cy;
        int p = nodes[u].parent;
        nodes[u].x = nodes[p].x + dir * (nodes[p].w / 2.0 + hGap + nodes[u].w / 2.0);
        double totalKidsH = 0.0;
        for (size_t i = 0; i < nodes[u].children.size(); i++) {
            if (i > 0) totalKidsH += vGap;
            totalKidsH += nodes[nodes[u].children[i]].subH;
        }
        double curY = cy - totalKidsH / 2.0;
        for (int c : nodes[u].children) {
            placeSubtree(c, curY + nodes[c].subH / 2.0, dir);
            curY += nodes[c].subH + vGap;
        }
    };

    auto placeSide = [&](const std::vector<int>& kids, int dir) {
        double tot = 0.0;
        for (size_t i = 0; i < kids.size(); i++) {
            if (i > 0) tot += vGap;
            tot += nodes[kids[i]].subH;
        }
        double curY = -tot / 2.0;
        for (int c : kids) {
            placeSubtree(c, curY + nodes[c].subH / 2.0, dir);
            curY += nodes[c].subH + vGap;
        }
    };
    placeSide(rightKids, 1);
    placeSide(leftKids, -1);

    double minX = 1e9, minY = 1e9, maxX = -1e9, maxY = -1e9;
    for (const auto& n : nodes) {
        minX = std::min(minX, n.x - n.w / 2.0);
        maxX = std::max(maxX, n.x + n.w / 2.0);
        minY = std::min(minY, n.y - n.h / 2.0);
        maxY = std::max(maxY, n.y + n.h / 2.0);
    }
    double pad = 24.0;
    double offX = pad - minX, offY = pad - minY;
    for (auto& n : nodes) { n.x += offX; n.y += offY; }

    std::string body;
    for (size_t i = 1; i < nodes.size(); i++) {
        const auto& n = nodes[i];
        const auto& p = nodes[n.parent];
        int dir = (n.x >= p.x) ? 1 : -1;
        double sx = p.x + dir * (p.w / 2.0);
        double sy = p.y;
        double ex = n.x - dir * (n.w / 2.0);
        double ey = n.y;
        double mx = (sx + ex) / 2.0;
        std::string col = seriesColor(std::max(0, n.branchIdx));
        body += "<path class=\"pele-edge\" d=\"M" + num(sx) + " " + num(sy) + "C" + num(mx) + " " + num(sy) + " " +
                num(mx) + " " + num(ey) + " " + num(ex) + " " + num(ey) +
                "\" fill=\"none\" stroke=\"" + col + "\" stroke-width=\"2\" stroke-linecap=\"round\"/>";
    }
    for (size_t i = 0; i < nodes.size(); i++) {
        const auto& n = nodes[i];
        ResolvedStyle rs;
        if (i == 0) {
            rs.fill = "var(--_c)";
            rs.stroke = "var(--_c)";
            rs.extra = " fill-opacity=\"0.2\"";
        } else {
            std::string col = seriesColor(std::max(0, n.branchIdx));
            rs.fill = col;
            rs.stroke = col;
            rs.extra = " fill-opacity=\"0.14\"";
        }
        double f = (i == 0) ? std::round(fontSize * 1.1) : fontSize;
        body += "<g class=\"pele-mindmap-node\">" + drawShape(n.shape, n.x, n.y, n.w, n.h, rs) +
                labelSvg(n.x, n.y, n.tb, f) + "</g>";
    }

    RenderResult r;
    r.type = "mindmap";
    r.width = std::ceil(maxX - minX + pad * 2.0);
    r.height = std::ceil(maxY - minY + pad * 2.0);
    r.svg = svgDocument("mindmap", r.width, r.height, fontSize, body);
    return r;
}

// ---------------------------------------------------------------------------
// 17. GitGraph Parser & Renderer (Matches src/diagrams/git/render.ts)
// ---------------------------------------------------------------------------

inline RenderResult renderGitGraph(const std::string& code, const RenderOptions& opts) {
    double fontSize = opts.fontSize;
    double smallFont = std::round(fontSize * 0.78);

    struct GitCommit {
        std::string id;
        std::string tag;
        std::string type = "NORMAL"; // NORMAL, HIGHLIGHT, REVERSE, MERGE
        int branchIdx = 0;
        std::vector<int> parents;
    };
    std::vector<std::string> branches = {"main"};
    std::map<std::string, int> branchMap = {{"main", 0}};
    std::map<int, int> branchHead;
    std::vector<GitCommit> commits;
    int curBranch = 0;
    int autoId = 1;

    auto ensureBranch = [&](const std::string& bname) -> int {
        auto it = branchMap.find(bname);
        if (it != branchMap.end()) return it->second;
        int idx = static_cast<int>(branches.size());
        branches.push_back(bname);
        branchMap[bname] = idx;
        if (branchHead.count(curBranch)) branchHead[idx] = branchHead[curBranch];
        return idx;
    };

    for (const auto& rawLine : splitLines(code)) {
        std::string line = trim(rawLine);
        if (line.empty()) continue;
        std::string low = toLower(line);
        if (low.rfind("gitgraph", 0) == 0) continue;
        if (low.rfind("branch ", 0) == 0) {
            std::string b = unquote(trim(line.substr(7)));
            size_t sp = b.find(' ');
            if (sp != std::string::npos) b = b.substr(0, sp);
            curBranch = ensureBranch(b);
            continue;
        }
        if (low.rfind("checkout ", 0) == 0 || low.rfind("switch ", 0) == 0) {
            size_t pLen = (low.rfind("switch ", 0) == 0) ? 7 : 9;
            curBranch = ensureBranch(unquote(trim(line.substr(pLen))));
            continue;
        }
        if (low.rfind("commit", 0) == 0 || low.rfind("merge ", 0) == 0 || low.rfind("cherry-pick", 0) == 0) {
            bool isMerge = (low.rfind("merge ", 0) == 0);
            GitCommit c;
            c.branchIdx = curBranch;
            if (branchHead.count(curBranch)) c.parents.push_back(branchHead[curBranch]);
            if (isMerge) {
                c.type = "MERGE";
                std::string rest = trim(line.substr(6));
                std::string targetB = unquote(trim(rest.substr(0, rest.find(' '))));
                if (branchMap.count(targetB) && branchHead.count(branchMap[targetB])) {
                    c.parents.push_back(branchHead[branchMap[targetB]]);
                }
            }
            // Extract id: "..." tag: "..." type: HIGHLIGHT/REVERSE
            size_t idPos = low.find("id:");
            if (idPos != std::string::npos) {
                size_t q1 = line.find('"', idPos), q2 = (q1 != std::string::npos) ? line.find('"', q1 + 1) : std::string::npos;
                if (q1 != std::string::npos && q2 != std::string::npos) c.id = line.substr(q1 + 1, q2 - q1 - 1);
            }
            size_t tagPos = low.find("tag:");
            if (tagPos != std::string::npos) {
                size_t q1 = line.find('"', tagPos), q2 = (q1 != std::string::npos) ? line.find('"', q1 + 1) : std::string::npos;
                if (q1 != std::string::npos && q2 != std::string::npos) c.tag = line.substr(q1 + 1, q2 - q1 - 1);
            }
            if (low.find("highlight") != std::string::npos) c.type = "HIGHLIGHT";
            else if (low.find("reverse") != std::string::npos) c.type = "REVERSE";
            if (c.id.empty()) c.id = "c" + std::to_string(autoId++);
            int cidx = static_cast<int>(commits.size());
            commits.push_back(c);
            branchHead[curBranch] = cidx;
        }
    }

    double pad = 24.0, laneH = 48.0, colStep = 58.0;
    double maxBranchW = 50.0;
    for (const auto& b : branches) maxBranchW = std::max(maxBranchW, measureTextWidth(b, smallFont, true, false) + 20.0);
    double startX = pad + maxBranchW + 24.0;
    double topPad = pad + 26.0;

    std::string body;
    for (size_t bi = 0; bi < branches.size(); bi++) {
        double y = topPad + bi * laneH;
        std::string col = seriesColor(static_cast<int>(bi));
        double endX = startX + std::max<size_t>(1, commits.size()) * colStep;
        body += "<g class=\"pele-branch\">"
                "<rect x=\"" + num(pad) + "\" y=\"" + num(y - 11.0) + "\" width=\"" + num(maxBranchW) + "\" height=\"22\" rx=\"4\" fill=\"" + col + "\" fill-opacity=\"0.18\" stroke=\"" + col + "\" stroke-width=\"1.2\"/>"
                "<text x=\"" + num(pad + maxBranchW / 2.0) + "\" y=\"" + num(y) + "\" text-anchor=\"middle\" dominant-baseline=\"central\" font-size=\"" + num(smallFont) + "\" font-weight=\"600\">" + escText(branches[bi]) + "</text>"
                "<line x1=\"" + num(pad + maxBranchW + 6.0) + "\" y1=\"" + num(y) + "\" x2=\"" + num(endX) + "\" y2=\"" + num(y) + "\" stroke=\"var(--_b)\" stroke-width=\"1\" stroke-dasharray=\"4 4\" stroke-opacity=\"0.5\"/></g>";
    }

    for (size_t ci = 0; ci < commits.size(); ci++) {
        double x2 = startX + ci * colStep;
        double y2 = topPad + commits[ci].branchIdx * laneH;
        for (int pIdx : commits[ci].parents) {
            double x1 = startX + pIdx * colStep;
            double y1 = topPad + commits[pIdx].branchIdx * laneH;
            int colorSlot = (commits[ci].branchIdx != commits[pIdx].branchIdx) ? std::max(commits[ci].branchIdx, commits[pIdx].branchIdx) : commits[ci].branchIdx;
            std::string col = seriesColor(colorSlot);
            if (std::abs(y1 - y2) < 1.0) {
                body += "<line class=\"pele-edge\" x1=\"" + num(x1) + "\" y1=\"" + num(y1) + "\" x2=\"" + num(x2) + "\" y2=\"" + num(y2) +
                        "\" stroke=\"" + col + "\" stroke-width=\"2.5\"/>";
            } else {
                double mx = (x1 + x2) / 2.0;
                body += "<path class=\"pele-edge\" d=\"M" + num(x1) + " " + num(y1) + "C" + num(mx) + " " + num(y1) + " " + num(mx) + " " + num(y2) + " " + num(x2) + " " + num(y2) +
                        "\" fill=\"none\" stroke=\"" + col + "\" stroke-width=\"2.5\"/>";
            }
        }
    }

    for (size_t ci = 0; ci < commits.size(); ci++) {
        const auto& c = commits[ci];
        double x = startX + ci * colStep;
        double y = topPad + c.branchIdx * laneH;
        std::string col = seriesColor(c.branchIdx);
        body += "<g class=\"pele-commit\">";
        if (c.type == "HIGHLIGHT") {
            body += "<rect x=\"" + num(x - 7.0) + "\" y=\"" + num(y - 7.0) + "\" width=\"14\" height=\"14\" rx=\"2\" fill=\"" + col + "\" stroke=\"var(--_bg)\" stroke-width=\"1.5\"/>";
        } else if (c.type == "MERGE") {
            body += "<circle cx=\"" + num(x) + "\" cy=\"" + num(y) + "\" r=\"7.5\" fill=\"var(--_bg)\" stroke=\"" + col + "\" stroke-width=\"2.2\"/>"
                    "<circle cx=\"" + num(x) + "\" cy=\"" + num(y) + "\" r=\"3.5\" fill=\"" + col + "\"/>";
        } else {
            body += "<circle cx=\"" + num(x) + "\" cy=\"" + num(y) + "\" r=\"6.5\" fill=\"" + col + "\" stroke=\"var(--_bg)\" stroke-width=\"1.5\"/>";
        }
        if (!c.tag.empty()) {
            double tw = measureTextWidth(c.tag, smallFont, false, false) + 12.0;
            body += "<g class=\"pele-tag\"><rect x=\"" + num(x - tw / 2.0) + "\" y=\"" + num(y - 26.0) + "\" width=\"" + num(tw) + "\" height=\"16\" rx=\"3\" fill=\"var(--_a)\" stroke=\"var(--_b)\" stroke-width=\"1\"/>"
                    "<text x=\"" + num(x) + "\" y=\"" + num(y - 18.0) + "\" text-anchor=\"middle\" dominant-baseline=\"central\" font-size=\"" + num(smallFont) + "\">" + escText(c.tag) + "</text></g>";
        }
        body += "<text x=\"" + num(x) + "\" y=\"" + num(y + 16.0) + "\" text-anchor=\"middle\" font-size=\"" + num(smallFont) + "\" fill=\"var(--_m)\">" + escText(c.id) + "</text></g>";
    }

    RenderResult r;
    r.type = "gitGraph";
    r.width = std::ceil(startX + std::max<size_t>(1, commits.size()) * colStep + pad);
    r.height = std::ceil(topPad + branches.size() * laneH + pad);
    r.svg = svgDocument("gitGraph", r.width, r.height, fontSize, body);
    return r;
}

// ---------------------------------------------------------------------------
// 18. Timeline & User Journey Renderers
// ---------------------------------------------------------------------------

inline RenderResult renderTimeline(const std::string& code, const RenderOptions& opts) {
    double fontSize = opts.fontSize;
    double titleFont = std::round(fontSize * 1.15);
    double smallFont = std::round(fontSize * 0.85);

    std::string title;
    struct TlPeriod { std::string period; std::vector<std::string> events; };
    std::vector<TlPeriod> periods;

    for (const auto& rawLine : splitLines(code)) {
        std::string line = trim(rawLine);
        if (line.empty()) continue;
        std::string low = toLower(line);
        if (low.rfind("timeline", 0) == 0 || low.rfind("section ", 0) == 0) continue;
        if (low.rfind("title ", 0) == 0) { title = unquote(trim(line.substr(6))); continue; }
        size_t col = line.find(':');
        if (col != std::string::npos) {
            std::string left = trim(line.substr(0, col));
            std::string right = trim(line.substr(col + 1));
            if (!left.empty()) {
                TlPeriod p; p.period = left;
                for (const auto& ev : splitChar(right, ':')) {
                    std::string te = trim(ev);
                    if (!te.empty()) p.events.push_back(te);
                }
                periods.push_back(p);
            } else if (!periods.empty()) {
                for (const auto& ev : splitChar(right, ':')) {
                    std::string te = trim(ev);
                    if (!te.empty()) periods.back().events.push_back(te);
                }
            }
        }
    }
    if (periods.empty()) periods.push_back({"2024", {"Event"}});

    double pad = 24.0, colW = 156.0, gap = 20.0;
    double titleH = title.empty() ? 0.0 : 32.0;
    double axisY = pad + titleH + 42.0;
    size_t maxEv = 1;
    for (const auto& p : periods) {
        maxEv = std::max(maxEv, p.events.size());
        for (const auto& ev : p.events) {
            colW = std::max(colW, std::min(220.0, measureTextWidth(ev, smallFont, false, false) + 28.0));
        }
    }
    double totalW = pad * 2.0 + periods.size() * colW + (periods.size() - 1) * gap;
    double totalH = axisY + 24.0 + maxEv * 42.0 + pad;

    std::string body;
    if (!title.empty()) body += labelSvg(totalW / 2.0, pad + 10.0, layoutLabel(title, titleFont, true), titleFont);
    body += "<line x1=\"" + num(pad) + "\" y1=\"" + num(axisY) + "\" x2=\"" + num(totalW - pad) + "\" y2=\"" + num(axisY) +
            "\" stroke=\"var(--_b)\" stroke-width=\"2\"/>";
    for (size_t i = 0; i < periods.size(); i++) {
        double x = pad + i * (colW + gap), cx = x + colW / 2.0;
        std::string col = seriesColor(static_cast<int>(i));
        body += "<g class=\"pele-timeline-period\">"
                "<rect x=\"" + num(x) + "\" y=\"" + num(axisY - 34.0) + "\" width=\"" + num(colW) + "\" height=\"26\" rx=\"6\" fill=\"" + col + "\" fill-opacity=\"0.18\" stroke=\"" + col + "\" stroke-width=\"1.2\"/>" +
                labelSvg(cx, axisY - 21.0, layoutLabel(periods[i].period, fontSize, true), fontSize) +
                "<circle cx=\"" + num(cx) + "\" cy=\"" + num(axisY) + "\" r=\"5\" fill=\"" + col + "\"/>";
        for (size_t ei = 0; ei < periods[i].events.size(); ei++) {
            double ey = axisY + 18.0 + ei * 42.0;
            body += "<line x1=\"" + num(cx) + "\" y1=\"" + num(ey - 10.0) + "\" x2=\"" + num(cx) + "\" y2=\"" + num(ey) + "\" stroke=\"" + col + "\" stroke-width=\"1.2\" stroke-dasharray=\"3 2\"/>"
                    "<rect x=\"" + num(x + 6.0) + "\" y=\"" + num(ey) + "\" width=\"" + num(colW - 12.0) + "\" height=\"32\" rx=\"5\" fill=\"var(--_s)\" stroke=\"var(--_b)\" stroke-width=\"var(--_w)\"/>" +
                    labelSvg(cx, ey + 16.0, layoutLabel(periods[i].events[ei], smallFont, false, colW - 20.0), smallFont);
        }
        body += "</g>";
    }

    RenderResult r;
    r.type = "timeline";
    r.width = std::ceil(totalW);
    r.height = std::ceil(totalH);
    r.svg = svgDocument("timeline", r.width, r.height, fontSize, body);
    return r;
}

inline RenderResult renderJourney(const std::string& code, const RenderOptions& opts) {
    double fontSize = opts.fontSize;
    double titleFont = std::round(fontSize * 1.15);
    double smallFont = std::round(fontSize * 0.85);

    std::string title, curSec;
    struct JTask { std::string label, section; int score = 3; std::vector<std::string> actors; };
    std::vector<JTask> tasks;
    std::vector<std::string> allActors;
    std::map<std::string, int> actorMap;

    for (const auto& rawLine : splitLines(code)) {
        std::string line = trim(rawLine);
        if (line.empty()) continue;
        std::string low = toLower(line);
        if (low.rfind("journey", 0) == 0) continue;
        if (low.rfind("title ", 0) == 0) { title = unquote(trim(line.substr(6))); continue; }
        if (low.rfind("section ", 0) == 0) { curSec = trim(line.substr(8)); continue; }
        size_t col = line.find(':');
        if (col != std::string::npos) {
            JTask jt; jt.label = trim(line.substr(0, col)); jt.section = curSec;
            auto parts = splitChar(line.substr(col + 1), ':');
            if (!parts.empty()) {
                try { jt.score = std::clamp(std::stoi(trim(parts[0])), 1, 5); } catch (...) {}
            }
            if (parts.size() >= 2) {
                for (const auto& rawA : splitChar(parts[1], ',')) {
                    std::string act = trim(rawA);
                    if (!act.empty()) {
                        jt.actors.push_back(act);
                        if (!actorMap.count(act)) {
                            actorMap[act] = static_cast<int>(allActors.size());
                            allActors.push_back(act);
                        }
                    }
                }
            }
            tasks.push_back(jt);
        }
    }
    if (tasks.empty()) tasks.push_back({"Step", "", 5, {}});

    double pad = 24.0, cardW = 140.0, gap = 16.0;
    double titleH = title.empty() ? 0.0 : 32.0;
    double legendH = allActors.empty() ? 0.0 : 26.0;
    double secY = pad + titleH + legendH;
    double cardY = secY + 34.0;
    double totalW = pad * 2.0 + tasks.size() * cardW + (tasks.size() - 1) * gap;
    double totalH = cardY + 92.0 + pad;

    std::string body;
    if (!title.empty()) body += labelSvg(totalW / 2.0, pad + 10.0, layoutLabel(title, titleFont, true), titleFont);
    if (!allActors.empty()) {
        double lx = pad;
        double ly = pad + titleH + 8.0;
        for (size_t ai = 0; ai < allActors.size(); ai++) {
            std::string col = seriesColor(static_cast<int>(ai));
            body += "<circle cx=\"" + num(lx + 6.0) + "\" cy=\"" + num(ly) + "\" r=\"5\" fill=\"" + col + "\"/>"
                    "<text x=\"" + num(lx + 15.0) + "\" y=\"" + num(ly) + "\" dominant-baseline=\"central\" font-size=\"" + num(smallFont) + "\">" + escText(allActors[ai]) + "</text>";
            lx += measureTextWidth(allActors[ai], smallFont, false, false) + 32.0;
        }
    }

    size_t i = 0, sIdx = 0;
    while (i < tasks.size()) {
        size_t j = i + 1;
        while (j < tasks.size() && tasks[j].section == tasks[i].section) j++;
        double sx = pad + i * (cardW + gap);
        double sw = (j - i) * cardW + (j - i - 1) * gap;
        std::string col = seriesColor(static_cast<int>(sIdx));
        if (!tasks[i].section.empty()) {
            body += "<rect x=\"" + num(sx) + "\" y=\"" + num(secY) + "\" width=\"" + num(sw) + "\" height=\"24\" rx=\"5\" fill=\"" + col + "\" fill-opacity=\"0.2\" stroke=\"" + col + "\" stroke-width=\"1.2\"/>" +
                    labelSvg(sx + sw / 2.0, secY + 12.0, layoutLabel(tasks[i].section, smallFont, true), smallFont);
        }
        for (size_t k = i; k < j; k++) {
            double cx = pad + k * (cardW + gap);
            double midX = cx + cardW / 2.0;
            body += "<g class=\"pele-journey-task\">"
                    "<rect x=\"" + num(cx) + "\" y=\"" + num(cardY) + "\" width=\"" + num(cardW) + "\" height=\"84\" rx=\"6\" fill=\"var(--_s)\" stroke=\"var(--_b)\" stroke-width=\"var(--_w)\"/>" +
                    labelSvg(midX, cardY + 26.0, layoutLabel(tasks[k].label, smallFont, false, cardW - 16.0), smallFont);
            double circleY = cardY + 64.0;
            std::string scol = (tasks[k].score >= 4) ? "var(--pele-series-2,#4ea87a)" : (tasks[k].score <= 2) ? "var(--pele-series-5,#d96b6b)" : "var(--_c)";
            body += "<circle cx=\"" + num(midX) + "\" cy=\"" + num(circleY) + "\" r=\"12\" fill=\"" + scol + "\" fill-opacity=\"0.2\" stroke=\"" + scol + "\" stroke-width=\"1.5\"/>"
                    "<text x=\"" + num(midX) + "\" y=\"" + num(circleY) + "\" text-anchor=\"middle\" dominant-baseline=\"central\" font-size=\"" + num(smallFont) + "\" font-weight=\"600\">" +
                    std::to_string(tasks[k].score) + "</text></g>";
        }
        i = j;
        sIdx++;
    }
    body += "<g class=\"pele-scale\">";
    for (int sc = 5; sc >= 1; sc--) {
        double sy = cardY + (5 - sc) * 18.0 + 8.0;
        body += "<text x=\"" + num(pad - 8.0) + "\" y=\"" + num(sy) + "\" text-anchor=\"end\" dominant-baseline=\"central\" font-size=\"" + num(smallFont * 0.85) + "\" fill=\"var(--_m)\">" + std::to_string(sc) + "</text>";
    }
    body += "</g>";

    RenderResult r;
    r.type = "journey";
    r.width = std::ceil(totalW);
    r.height = std::ceil(totalH);
    r.svg = svgDocument("journey", r.width, r.height, fontSize, body);
    return r;
}

// ---------------------------------------------------------------------------
// 19. Quadrant, Sankey, Packet, Kanban, Radar, Treemap, TreeView, Venn,
//     Ishikawa, Wardley, Cynefin, Railroad, EventModeling, Info Renderers
// ---------------------------------------------------------------------------

inline RenderResult renderQuadrant(const std::string& code, const RenderOptions& opts) {
    double fontSize = opts.fontSize;
    double smallFont = std::round(fontSize * 0.84);
    std::string title, xLeft, xRight, yBottom, yTop, q1, q2, q3, q4;
    struct QPt { std::string label; double x = 0.5, y = 0.5; };
    std::vector<QPt> pts;

    for (const auto& rawLine : splitLines(code)) {
        std::string line = trim(rawLine);
        if (line.empty()) continue;
        std::string low = toLower(line);
        if (low.rfind("quadrantchart", 0) == 0) continue;
        if (low.rfind("title ", 0) == 0) { title = unquote(trim(line.substr(6))); continue; }
        if (low.rfind("x-axis ", 0) == 0) {
            std::string rest = trim(line.substr(7));
            size_t ar = rest.find("-->");
            if (ar != std::string::npos) { xLeft = unquote(trim(rest.substr(0, ar))); xRight = unquote(trim(rest.substr(ar + 3))); }
            else xLeft = unquote(rest);
            continue;
        }
        if (low.rfind("y-axis ", 0) == 0) {
            std::string rest = trim(line.substr(7));
            size_t ar = rest.find("-->");
            if (ar != std::string::npos) { yBottom = unquote(trim(rest.substr(0, ar))); yTop = unquote(trim(rest.substr(ar + 3))); }
            else yBottom = unquote(rest);
            continue;
        }
        if (low.rfind("quadrant-1 ", 0) == 0) { q1 = unquote(trim(line.substr(11))); continue; }
        if (low.rfind("quadrant-2 ", 0) == 0) { q2 = unquote(trim(line.substr(11))); continue; }
        if (low.rfind("quadrant-3 ", 0) == 0) { q3 = unquote(trim(line.substr(11))); continue; }
        if (low.rfind("quadrant-4 ", 0) == 0) { q4 = unquote(trim(line.substr(11))); continue; }
        size_t col = line.find(':');
        if (col != std::string::npos) {
            std::string lbl = unquote(trim(line.substr(0, col)));
            size_t ob = line.find('[', col), cb = line.find(']', col);
            if (ob != std::string::npos && cb != std::string::npos) {
                auto xy = splitChar(line.substr(ob + 1, cb - ob - 1), ',');
                if (xy.size() >= 2) {
                    try { pts.push_back({lbl, std::stod(trim(xy[0])), std::stod(trim(xy[1]))}); } catch (...) {}
                }
            }
        }
    }

    double pad = 32.0, titleH = title.empty() ? 0.0 : 28.0, size = 300.0;
    double gx = pad + 24.0, gy = pad + titleH;
    double hw = size / 2.0;
    std::string body;
    if (!title.empty()) body += labelSvg(gx + hw, pad + 8.0, layoutLabel(title, fontSize * 1.15, true), fontSize * 1.15);
    body += "<rect x=\"" + num(gx) + "\" y=\"" + num(gy) + "\" width=\"" + num(hw) + "\" height=\"" + num(hw) + "\" fill=\"var(--_s)\"/>"
            "<rect x=\"" + num(gx + hw) + "\" y=\"" + num(gy) + "\" width=\"" + num(hw) + "\" height=\"" + num(hw) + "\" fill=\"var(--_a)\" fill-opacity=\"0.6\"/>"
            "<rect x=\"" + num(gx) + "\" y=\"" + num(gy + hw) + "\" width=\"" + num(hw) + "\" height=\"" + num(hw) + "\" fill=\"var(--_a)\" fill-opacity=\"0.6\"/>"
            "<rect x=\"" + num(gx + hw) + "\" y=\"" + num(gy + hw) + "\" width=\"" + num(hw) + "\" height=\"" + num(hw) + "\" fill=\"var(--_s)\"/>"
            "<rect x=\"" + num(gx) + "\" y=\"" + num(gy) + "\" width=\"" + num(size) + "\" height=\"" + num(size) + "\" fill=\"none\" stroke=\"var(--_b)\" stroke-width=\"var(--_w)\"/>"
            "<line x1=\"" + num(gx + hw) + "\" y1=\"" + num(gy) + "\" x2=\"" + num(gx + hw) + "\" y2=\"" + num(gy + size) + "\" stroke=\"var(--_b)\" stroke-width=\"1\"/>"
            "<line x1=\"" + num(gx) + "\" y1=\"" + num(gy + hw) + "\" x2=\"" + num(gx + size) + "\" y2=\"" + num(gy + hw) + "\" stroke=\"var(--_b)\" stroke-width=\"1\"/>";
    if (!q2.empty()) body += labelSvg(gx + hw / 2.0, gy + 18.0, layoutLabel(q2, smallFont, true), smallFont, "middle", "var(--_m)");
    if (!q1.empty()) body += labelSvg(gx + hw * 1.5, gy + 18.0, layoutLabel(q1, smallFont, true), smallFont, "middle", "var(--_m)");
    if (!q3.empty()) body += labelSvg(gx + hw / 2.0, gy + hw + 18.0, layoutLabel(q3, smallFont, true), smallFont, "middle", "var(--_m)");
    if (!q4.empty()) body += labelSvg(gx + hw * 1.5, gy + hw + 18.0, layoutLabel(q4, smallFont, true), smallFont, "middle", "var(--_m)");

    for (size_t i = 0; i < pts.size(); i++) {
        double px = gx + std::clamp(pts[i].x, 0.0, 1.0) * size;
        double py = gy + (1.0 - std::clamp(pts[i].y, 0.0, 1.0)) * size;
        std::string col = seriesColor(static_cast<int>(i));
        body += "<g class=\"pele-quadrant-point\">"
                "<circle cx=\"" + num(px) + "\" cy=\"" + num(py) + "\" r=\"5.5\" fill=\"" + col + "\" stroke=\"var(--_bg)\" stroke-width=\"1.5\"/>"
                "<text x=\"" + num(px) + "\" y=\"" + num(py + 14.0) + "\" text-anchor=\"middle\" font-size=\"" + num(smallFont) + "\">" + escText(pts[i].label) + "</text></g>";
    }
    if (!xLeft.empty()) body += labelSvg(gx + hw / 2.0, gy + size + 18.0, layoutLabel(xLeft, smallFont, false), smallFont, "middle", "var(--_m)");
    if (!xRight.empty()) body += labelSvg(gx + hw * 1.5, gy + size + 18.0, layoutLabel(xRight, smallFont, false), smallFont, "middle", "var(--_m)");
    if (!yBottom.empty()) {
        double yx = gx - 14.0, yy = gy + hw * 1.5;
        body += "<text x=\"" + num(yx) + "\" y=\"" + num(yy) + "\" text-anchor=\"middle\" dominant-baseline=\"central\" font-size=\"" + num(smallFont) + "\" fill=\"var(--_m)\" transform=\"rotate(-90 " + num(yx) + " " + num(yy) + ")\">" + escText(yBottom) + "</text>";
    }
    if (!yTop.empty()) {
        double yx = gx - 14.0, yy = gy + hw * 0.5;
        body += "<text x=\"" + num(yx) + "\" y=\"" + num(yy) + "\" text-anchor=\"middle\" dominant-baseline=\"central\" font-size=\"" + num(smallFont) + "\" fill=\"var(--_m)\" transform=\"rotate(-90 " + num(yx) + " " + num(yy) + ")\">" + escText(yTop) + "</text>";
    }

    RenderResult r;
    r.type = "quadrantChart";
    r.width = std::ceil(gx + size + pad);
    r.height = std::ceil(gy + size + 36.0 + pad);
    r.svg = svgDocument("quadrantChart", r.width, r.height, fontSize, body);
    return r;
}

inline RenderResult renderSankey(const std::string& code, const RenderOptions& opts) {
    double fontSize = opts.fontSize;
    double smallFont = std::round(fontSize * 0.85);
    struct SkLink { int u, v; double val; };
    std::vector<std::string> names;
    std::map<std::string, int> nmap;
    std::vector<SkLink> links;
    auto ensureN = [&](const std::string& s) -> int {
        if (nmap.count(s)) return nmap[s];
        int id = static_cast<int>(names.size());
        names.push_back(s); nmap[s] = id; return id;
    };
    for (const auto& rawLine : splitLines(code)) {
        std::string line = trim(rawLine);
        if (line.empty() || toLower(line).rfind("sankey", 0) == 0) continue;
        auto cols = parseCallArgs(line);
        if (cols.size() >= 3) {
            try {
                double v = std::stod(cols[2]);
                if (v > 0) links.push_back({ensureN(cols[0]), ensureN(cols[1]), v});
            } catch (...) {}
        }
    }
    if (names.empty()) { ensureN("A"); ensureN("B"); links.push_back({0, 1, 10.0}); }
    int N = static_cast<int>(names.size());
    std::vector<int> layer(N, 0);
    for (int it = 0; it < N; it++) {
        for (const auto& lk : links) if ( lk.u != lk.v ) layer[lk.v] = std::max(layer[lk.v], layer[lk.u] + 1);
    }
    int maxL = 0;
    std::vector<double> inSum(N, 0.0), outSum(N, 0.0);
    for (const auto& lk : links) { outSum[lk.u] += lk.val; inSum[lk.v] += lk.val; }
    for (int l : layer) maxL = std::max(maxL, l);

    std::vector<std::vector<int>> cols(maxL + 1);
    for (int i = 0; i < N; i++) cols[layer[i]].push_back(i);

    double pad = 24.0, plotW = std::max(360.0, maxL * 180.0), plotH = 220.0, nodeW = 14.0;
    double maxColTotal = 1.0;
    for (const auto& c : cols) {
        double tot = 0.0;
        for (int u : c) tot += std::max(inSum[u], outSum[u]);
        maxColTotal = std::max(maxColTotal, tot);
    }
    double scale = (plotH - 40.0) / maxColTotal;
    std::vector<double> nx(N), ny(N), nh(N), inOff(N, 0.0), outOff(N, 0.0);
    for (int l = 0; l <= maxL; l++) {
        double x = pad + (maxL == 0 ? 0.0 : (static_cast<double>(l) / maxL) * (plotW - nodeW));
        double curY = pad;
        for (int u : cols[l]) {
            nx[u] = x;
            ny[u] = curY;
            nh[u] = std::max(10.0, std::max(inSum[u], outSum[u]) * scale);
            curY += nh[u] + 16.0;
        }
    }

    std::string body;
    for (const auto& lk : links) {
        double w = std::max(2.0, lk.val * scale);
        double x1 = nx[lk.u] + nodeW, y1 = ny[lk.u] + outOff[lk.u] + w / 2.0;
        double x2 = nx[lk.v], y2 = ny[lk.v] + inOff[lk.v] + w / 2.0;
        outOff[lk.u] += w; inOff[lk.v] += w;
        double mx = (x1 + x2) / 2.0;
        std::string col = seriesColor(lk.u);
        body += "<path class=\"pele-sankey-link\" d=\"M" + num(x1) + " " + num(y1) + "C" + num(mx) + " " + num(y1) + " " +
                num(mx) + " " + num(y2) + " " + num(x2) + " " + num(y2) + "\" fill=\"none\" stroke=\"" + col + "\" stroke-width=\"" + num(w) + "\" stroke-opacity=\"0.38\"/>";
    }
    double maxBottom = plotH + pad;
    for (int i = 0; i < N; i++) {
        std::string col = seriesColor(i);
        maxBottom = std::max(maxBottom, ny[i] + nh[i] + pad);
        bool rightSide = (layer[i] == maxL && maxL > 0);
        double lx = rightSide ? (nx[i] - 6.0) : (nx[i] + nodeW + 6.0);
        std::string anc = rightSide ? "end" : "start";
        double nodeTotal = std::max(inSum[i], outSum[i]);
        body += "<g class=\"pele-sankey-node\"><rect x=\"" + num(nx[i]) + "\" y=\"" + num(ny[i]) + "\" width=\"" + num(nodeW) + "\" height=\"" + num(nh[i]) + "\" rx=\"2\" fill=\"" + col + "\"/>"
                "<text x=\"" + num(lx) + "\" y=\"" + num(ny[i] + nh[i] / 2.0 - 6.0) + "\" text-anchor=\"" + anc + "\" dominant-baseline=\"central\" font-size=\"" + num(smallFont) + "\">" + escText(names[i]) + "</text>"
                "<text x=\"" + num(lx) + "\" y=\"" + num(ny[i] + nh[i] / 2.0 + 8.0) + "\" text-anchor=\"" + anc + "\" dominant-baseline=\"central\" font-size=\"" + num(smallFont * 0.85) + "\" fill=\"var(--_m)\">" + num(nodeTotal) + "</text></g>";
    }

    RenderResult r;
    r.type = "sankey";
    r.width = std::ceil(plotW + pad * 2.0 + 80.0);
    r.height = std::ceil(maxBottom);
    r.svg = svgDocument("sankey", r.width, r.height, fontSize, body);
    return r;
}

inline RenderResult renderPacket(const std::string& code, const RenderOptions& opts) {
    double fontSize = opts.fontSize;
    double smallFont = std::round(fontSize * 0.76);
    std::string title;
    struct PField { int startBit = 0, endBit = 0; std::string label; };
    std::vector<PField> fields;
    int nextBit = 0;

    for (const auto& rawLine : splitLines(code)) {
        std::string line = trim(rawLine);
        if (line.empty() || toLower(line).rfind("packet", 0) == 0) continue;
        if (toLower(line).rfind("title ", 0) == 0) { title = unquote(trim(line.substr(6))); continue; }
        size_t col = line.find(':');
        if (col != std::string::npos) {
            std::string spec = trim(line.substr(0, col));
            std::string lbl = unquote(trim(line.substr(col + 1)));
            int s = nextBit, e = nextBit;
            if (!spec.empty() && spec.front() == '+') {
                int len = std::max(1, std::stoi(spec.substr(1)));
                s = nextBit; e = nextBit + len - 1;
            } else if (spec.find('-') != std::string::npos) {
                size_t d = spec.find('-');
                s = std::stoi(spec.substr(0, d));
                e = std::stoi(spec.substr(d + 1));
            } else {
                s = e = std::stoi(spec);
            }
            nextBit = e + 1;
            fields.push_back({s, e, lbl});
        }
    }
    if (fields.empty()) fields.push_back({0, 31, "Payload"});

    double pad = 24.0, bitW = 20.0, rowH = 44.0;
    double titleH = title.empty() ? 0.0 : 28.0;
    double topY = pad + titleH + 18.0;
    int maxRow = 0;
    std::string body;
    for (const auto& f : fields) {
        int cur = f.startBit;
        while (cur <= f.endBit) {
            int row = cur / 32;
            maxRow = std::max(maxRow, row);
            int colStart = cur % 32;
            int rowEndBit = (row + 1) * 32 - 1;
            int segEnd = std::min(f.endBit, rowEndBit);
            int span = segEnd - cur + 1;
            double x = pad + colStart * bitW;
            double y = topY + row * rowH;
            double w = span * bitW;
            body += "<g class=\"pele-packet-field\"><rect x=\"" + num(x) + "\" y=\"" + num(y) + "\" width=\"" + num(w) + "\" height=\"" + num(rowH) +
                    "\" fill=\"var(--_s)\" stroke=\"var(--_b)\" stroke-width=\"var(--_w)\"/>"
                    "<text class=\"pele-packet-bit\" x=\"" + num(x + 4.0) + "\" y=\"" + num(y + 10.0) + "\" text-anchor=\"start\" font-size=\"" + num(smallFont) + "\" font-family=\"var(--_fm)\" fill=\"var(--_m)\">" + std::to_string(cur) + "</text>";
            if (segEnd > cur) {
                body += "<text class=\"pele-packet-bit\" x=\"" + num(x + w - 4.0) + "\" y=\"" + num(y + 10.0) + "\" text-anchor=\"end\" font-size=\"" + num(smallFont) + "\" font-family=\"var(--_fm)\" fill=\"var(--_m)\">" + std::to_string(segEnd) + "</text>";
            }
            body += labelSvg(x + w / 2.0, y + rowH / 2.0 + 4.0, layoutLabel(f.label, fontSize * 0.9, false), fontSize * 0.9) + "</g>";
            cur = segEnd + 1;
        }
    }
    double totalW = pad * 2.0 + 32.0 * bitW;
    if (!title.empty()) body += labelSvg(totalW / 2.0, pad + 8.0, layoutLabel(title, fontSize * 1.15, true), fontSize * 1.15);

    RenderResult r;
    r.type = "packet";
    r.width = std::ceil(totalW);
    r.height = std::ceil(topY + (maxRow + 1) * rowH + pad);
    r.svg = svgDocument("packet", r.width, r.height, fontSize, body);
    return r;
}

inline RenderResult renderKanban(const std::string& code, const RenderOptions& opts) {
    double fontSize = opts.fontSize;
    double smallFont = std::round(fontSize * 0.85);
    struct KCol { std::string title; std::vector<std::string> cards; };
    std::vector<KCol> cols;
    int baseIndent = -1;

    for (const auto& rawLine : splitLines(code)) {
        std::string t = trim(rawLine);
        if (t.empty() || toLower(t).rfind("kanban", 0) == 0) continue;
        int ind = 0;
        for (char c : rawLine) { if (c == ' ') ind++; else if (c == '\t') ind += 4; else break; }
        size_t atPos = t.find("@{");
        if (atPos != std::string::npos) t = trim(t.substr(0, atPos));
        size_t ob = t.find('['), cb = t.rfind(']');
        std::string lbl = (ob != std::string::npos && cb != std::string::npos && cb > ob) ? t.substr(ob + 1, cb - ob - 1) : t;
        lbl = unquote(trim(lbl));
        if (baseIndent < 0 || ind <= baseIndent) {
            baseIndent = ind;
            cols.push_back({lbl, {}});
        } else if (!cols.empty()) {
            cols.back().cards.push_back(lbl);
        }
    }
    if (cols.empty()) cols.push_back({"Todo", {"Task"}});

    double pad = 24.0, colW = 196.0, gap = 16.0, cardH = 44.0;
    size_t maxCards = 1;
    for (const auto& c : cols) maxCards = std::max(maxCards, c.cards.size());
    double colH = 48.0 + maxCards * (cardH + 10.0) + 12.0;

    std::string body;
    for (size_t i = 0; i < cols.size(); i++) {
        double x = pad + i * (colW + gap);
        std::string col = seriesColor(static_cast<int>(i));
        body += "<g class=\"pele-kanban-column\">"
                "<rect x=\"" + num(x) + "\" y=\"" + num(pad) + "\" width=\"" + num(colW) + "\" height=\"" + num(colH) +
                "\" rx=\"8\" fill=\"var(--_a)\" fill-opacity=\"0.55\" stroke=\"var(--_b)\" stroke-width=\"var(--_w)\"/>"
                "<circle cx=\"" + num(x + 18.0) + "\" cy=\"" + num(pad + 22.0) + "\" r=\"5\" fill=\"" + col + "\"/>" +
                labelSvg(x + 30.0, pad + 22.0, layoutLabel(cols[i].title, fontSize, true), fontSize, "start") +
                "<text class=\"pele-count\" x=\"" + num(x + colW - 14.0) + "\" y=\"" + num(pad + 22.0) + "\" text-anchor=\"end\" dominant-baseline=\"central\" font-size=\"" + num(smallFont) + "\" fill=\"var(--_m)\">" + std::to_string(cols[i].cards.size()) + "</text>";
        for (size_t ci = 0; ci < cols[i].cards.size(); ci++) {
            double cy = pad + 44.0 + ci * (cardH + 10.0);
            body += "<g class=\"pele-kanban-card\"><rect x=\"" + num(x + 10.0) + "\" y=\"" + num(cy) + "\" width=\"" + num(colW - 20.0) + "\" height=\"" + num(cardH) +
                    "\" rx=\"6\" fill=\"var(--_s)\" stroke=\"var(--_b)\" stroke-width=\"var(--_w)\"/>" +
                    labelSvg(x + colW / 2.0, cy + cardH / 2.0, layoutLabel(cols[i].cards[ci], smallFont, false, colW - 34.0), smallFont) + "</g>";
        }
        body += "</g>";
    }

    RenderResult r;
    r.type = "kanban";
    r.width = std::ceil(pad * 2.0 + cols.size() * colW + (cols.size() - 1) * gap);
    r.height = std::ceil(pad * 2.0 + colH);
    r.svg = svgDocument("kanban", r.width, r.height, fontSize, body);
    return r;
}

inline RenderResult renderRadar(const std::string& code, const RenderOptions& opts) {
    double fontSize = opts.fontSize;
    double smallFont = std::round(fontSize * 0.85);
    std::string title;
    std::vector<std::string> axes;
    struct RCurve { std::string label; std::vector<double> vals; };
    std::vector<RCurve> curves;
    double maxVal = 100.0;
    bool hasMax = false;

    for (const auto& rawLine : splitLines(code)) {
        std::string line = trim(rawLine);
        if (line.empty() || toLower(line).rfind("radar", 0) == 0) continue;
        std::string low = toLower(line);
        if (low.rfind("title ", 0) == 0) { title = unquote(trim(line.substr(6))); continue; }
        if (low.rfind("max ", 0) == 0) { try { maxVal = std::stod(trim(line.substr(4))); hasMax = true; } catch (...) {} continue; }
        if (low.rfind("axis ", 0) == 0) {
            for (const auto& a : splitChar(line.substr(5), ',')) {
                std::string s = trim(a);
                size_t ob = s.find('['), cb = s.find(']');
                if (ob != std::string::npos && cb != std::string::npos) s = s.substr(ob + 1, cb - ob - 1);
                axes.push_back(unquote(trim(s)));
            }
            continue;
        }
        if (low.rfind("curve ", 0) == 0) {
            size_t ob = line.find('{'), cb = line.find('}');
            std::string head = trim(line.substr(6, ob == std::string::npos ? std::string::npos : ob - 6));
            size_t lb = head.find('['), rb = head.find(']');
            std::string clbl = (lb != std::string::npos && rb != std::string::npos) ? head.substr(lb + 1, rb - lb - 1) : head;
            RCurve rc; rc.label = unquote(trim(clbl));
            if (ob != std::string::npos && cb != std::string::npos) {
                for (const auto& v : splitChar(line.substr(ob + 1, cb - ob - 1), ',')) {
                    size_t col = v.find(':');
                    std::string vs = (col != std::string::npos) ? v.substr(col + 1) : v;
                    try { rc.vals.push_back(std::stod(trim(vs))); } catch (...) {}
                }
            }
            curves.push_back(rc);
        }
    }
    if (axes.size() < 3) axes = {"A", "B", "C", "D", "E"};
    if (!hasMax) {
        maxVal = 1.0;
        for (const auto& c : curves) for (double v : c.vals) maxVal = std::max(maxVal, v);
    }

    double pad = 42.0, radius = 110.0, titleH = title.empty() ? 0.0 : 28.0;
    double cx = pad + radius + 44.0, cy = pad + titleH + radius + 16.0;
    int N = static_cast<int>(axes.size());
    std::string body;
    if (!title.empty()) body += labelSvg(cx, pad + 8.0, layoutLabel(title, fontSize * 1.15, true), fontSize * 1.15);

    for (int t = 1; t <= 5; t++) {
        double r = radius * (static_cast<double>(t) / 5.0);
        std::string pts;
        for (int i = 0; i < N; i++) {
            double ang = -kPi / 2.0 + (2.0 * kPi * i) / N;
            pts += (i ? " " : "") + num(cx + std::cos(ang) * r) + "," + num(cy + std::sin(ang) * r);
        }
        body += "<polygon points=\"" + pts + "\" fill=\"none\" stroke=\"var(--_b)\" stroke-width=\"1\" stroke-opacity=\"0.4\"/>";
    }
    for (int i = 0; i < N; i++) {
        double ang = -kPi / 2.0 + (2.0 * kPi * i) / N;
        double ex = cx + std::cos(ang) * radius, ey = cy + std::sin(ang) * radius;
        double lx = cx + std::cos(ang) * (radius + 22.0), ly = cy + std::sin(ang) * (radius + 18.0);
        body += "<line x1=\"" + num(cx) + "\" y1=\"" + num(cy) + "\" x2=\"" + num(ex) + "\" y2=\"" + num(ey) + "\" stroke=\"var(--_b)\" stroke-width=\"1\" stroke-opacity=\"0.5\"/>" +
                labelSvg(lx, ly, layoutLabel(axes[i], smallFont, false), smallFont);
    }
    for (size_t ci = 0; ci < curves.size(); ci++) {
        std::string col = seriesColor(static_cast<int>(ci));
        std::string pts, dots;
        for (int i = 0; i < N; i++) {
            double v = (i < static_cast<int>(curves[ci].vals.size())) ? curves[ci].vals[i] : 0.0;
            double r = radius * std::clamp(v / maxVal, 0.0, 1.0);
            double ang = -kPi / 2.0 + (2.0 * kPi * i) / N;
            double px = cx + std::cos(ang) * r, py = cy + std::sin(ang) * r;
            pts += (i ? " " : "") + num(px) + "," + num(py);
            dots += "<circle cx=\"" + num(px) + "\" cy=\"" + num(py) + "\" r=\"3.5\" fill=\"" + col + "\"/>";
        }
        body += "<g class=\"pele-radar-curve\"><polygon points=\"" + pts + "\" fill=\"" + col + "\" fill-opacity=\"0.2\" stroke=\"" + col + "\" stroke-width=\"2\"/>" + dots + "</g>";
    }
    double legendH = 0.0;
    if (!curves.empty()) {
        legendH = 26.0;
        double lx = pad;
        double ly = cy + radius + 34.0;
        for (size_t ci = 0; ci < curves.size(); ci++) {
            if (curves[ci].label.empty()) continue;
            std::string col = seriesColor(static_cast<int>(ci));
            body += "<rect x=\"" + num(lx) + "\" y=\"" + num(ly - 5.0) + "\" width=\"10\" height=\"10\" rx=\"2\" fill=\"" + col + "\"/>"
                    "<text x=\"" + num(lx + 15.0) + "\" y=\"" + num(ly) + "\" dominant-baseline=\"central\" font-size=\"" + num(smallFont) + "\">" + escText(curves[ci].label) + "</text>";
            lx += measureTextWidth(curves[ci].label, smallFont, false, false) + 30.0;
        }
    }

    RenderResult r;
    r.type = "radar";
    r.width = std::ceil(cx * 2.0);
    r.height = std::ceil(cy + radius + 44.0 + legendH);
    r.svg = svgDocument("radar", r.width, r.height, fontSize, body);
    return r;
}

inline RenderResult renderTreemap(const std::string& code, const RenderOptions& opts) {
    double fontSize = opts.fontSize;
    double smallFont = std::round(fontSize * 0.85);
    struct TmLeaf { std::string label, group; double val = 10.0; int gIdx = 0; };
    std::vector<TmLeaf> leaves;
    std::string curGroup;
    int gCounter = -1;

    for (const auto& rawLine : splitLines(code)) {
        std::string t = trim(rawLine);
        if (t.empty() || toLower(t).rfind("treemap", 0) == 0) continue;
        size_t col = t.rfind(':');
        if (col != std::string::npos) {
            std::string lbl = unquote(trim(t.substr(0, col)));
            double v = 10.0;
            try { v = std::max(1.0, std::stod(trim(t.substr(col + 1)))); } catch (...) {}
            leaves.push_back({lbl, curGroup, v, std::max(0, gCounter)});
        } else {
            curGroup = unquote(t);
            gCounter++;
        }
    }
    if (leaves.empty()) leaves.push_back({"Item", "", 100.0, 0});

    double pad = 24.0, W = 480.0, H = 280.0;
    std::string body;
    std::function<void(size_t, size_t, double, double, double, double, bool)> sliceLayout =
        [&](size_t l, size_t r, double x, double y, double w, double h, bool horiz) {
            if (l >= r) return;
            if (l + 1 == r) {
                const auto& lf = leaves[l];
                std::string col = seriesColor(lf.gIdx);
                body += "<g class=\"pele-treemap-leaf\"><rect x=\"" + num(x + 2.0) + "\" y=\"" + num(y + 2.0) +
                        "\" width=\"" + num(std::max(4.0, w - 4.0)) + "\" height=\"" + num(std::max(4.0, h - 4.0)) +
                        "\" rx=\"4\" fill=\"" + col + "\" fill-opacity=\"0.22\" stroke=\"" + col + "\" stroke-width=\"1.2\"/>";
                if (w > 48.0 && h > 28.0) {
                    body += labelSvg(x + w / 2.0, y + h / 2.0 - 7.0, layoutLabel(lf.label, smallFont, true), smallFont) +
                            labelSvg(x + w / 2.0, y + h / 2.0 + 9.0, layoutLabel(num(lf.val), smallFont * 0.9, false), smallFont * 0.9, "middle", "var(--_m)");
                }
                body += "</g>";
                return;
            }
            double tot = 0.0;
            for (size_t i = l; i < r; i++) tot += leaves[i].val;
            double acc = 0.0;
            size_t mid = l + 1;
            for (size_t i = l; i < r - 1; i++) {
                acc += leaves[i].val;
                mid = i + 1;
                if (acc >= tot * 0.5) break;
            }
            double frac = (tot > 0) ? (acc / tot) : 0.5;
            if (horiz) {
                double w1 = w * frac;
                sliceLayout(l, mid, x, y, w1, h, !horiz);
                sliceLayout(mid, r, x + w1, y, w - w1, h, !horiz);
            } else {
                double h1 = h * frac;
                sliceLayout(l, mid, x, y, w, h1, !horiz);
                sliceLayout(mid, r, x, y + h1, w, h - h1, !horiz);
            }
        };
    sliceLayout(0, leaves.size(), pad, pad, W, H, true);
    std::map<std::string, double> groupSums;
    std::vector<std::string> groupOrder;
    for (const auto& lf : leaves) {
        if (!lf.group.empty()) {
            if (!groupSums.count(lf.group)) groupOrder.push_back(lf.group);
            groupSums[lf.group] += lf.val;
        }
    }
    for (size_t gi = 0; gi < groupOrder.size(); gi++) {
        const std::string& gname = groupOrder[gi];
        double gx = pad + gi * 140.0;
        body += "<g class=\"pele-treemap-group\">"
                "<text x=\"" + num(gx + 6.0) + "\" y=\"" + num(pad - 8.0) + "\" font-size=\"" + num(smallFont) + "\" font-weight=\"600\">" + escText(gname) + "</text>"
                "<text x=\"" + num(gx + 6.0 + measureTextWidth(gname, smallFont, true, false) + 6.0) + "\" y=\"" + num(pad - 8.0) + "\" font-size=\"" + num(smallFont * 0.9) + "\" fill=\"var(--_m)\">" + num(groupSums[gname]) + "</text></g>";
    }

    RenderResult r;
    r.type = "treemap";
    r.width = std::ceil(W + pad * 2.0);
    r.height = std::ceil(H + pad * 2.0);
    r.svg = svgDocument("treemap", r.width, r.height, fontSize, body);
    return r;
}

inline RenderResult renderTreeView(const std::string& code, const RenderOptions& opts) {
    double fontSize = opts.fontSize;
    struct TvItem { std::string name; int depth = 0; bool isFolder = false; };
    std::vector<TvItem> items;
    std::vector<int> indents;

    for (const auto& rawLine : splitLines(code)) {
        std::string t = trim(rawLine);
        if (t.empty() || toLower(t).rfind("treeview", 0) == 0) continue;
        int ind = 0;
        size_t pos = 0;
        while (pos < rawLine.size()) {
            unsigned char c = static_cast<unsigned char>(rawLine[pos]);
            if (c == ' ') { ind++; pos++; }
            else if (c == '\t') { ind += 4; pos++; }
            else if (c == '|' || c == '+' || c == '`' || c == '-') { ind++; pos++; }
            else if (pos + 2 < rawLine.size() && c == 0xE2 && static_cast<unsigned char>(rawLine[pos + 1]) == 0x94) {
                // UTF-8 box drawing character (, , )
                ind += 2;
                pos += 3;
            } else break;
        }
        std::string cleanName = unquote(trim(rawLine.substr(pos)));
        if (cleanName.empty()) continue;
        while (!indents.empty() && indents.back() >= ind) indents.pop_back();
        int depth = static_cast<int>(indents.size());
        indents.push_back(ind);
        items.push_back({cleanName, depth, false});
    }
    for (size_t i = 0; i + 1 < items.size(); i++) {
        if (items[i + 1].depth > items[i].depth) items[i].isFolder = true;
    }
    for (auto& it : items) {
        if (!it.name.empty() && it.name.back() == '/') {
            it.isFolder = true;
            it.name.pop_back();
        }
    }

    double pad = 20.0, rowH = 26.0, indentW = 20.0, maxW = 180.0;
    std::string body;
    for (size_t i = 0; i < items.size(); i++) {
        double x = pad + items[i].depth * indentW;
        double cy = pad + i * rowH + rowH / 2.0;
        maxW = std::max(maxW, x + 22.0 + measureTextWidth(items[i].name, fontSize, items[i].isFolder, false) + pad);
        if (items[i].depth > 0) {
            double gx = x - indentW / 2.0;
            body += "<path d=\"M" + num(gx) + " " + num(cy - rowH / 2.0) + "V" + num(cy) + "H" + num(x - 2.0) + "\" fill=\"none\" stroke=\"var(--_b)\" stroke-width=\"1\"/>";
        }
        if (items[i].isFolder) {
            body += "<path d=\"M" + num(x) + " " + num(cy - 5) + "H" + num(x + 5) + "L" + num(x + 7) + " " + num(cy - 3) + "H" + num(x + 14) + "V" + num(cy + 6) + "H" + num(x) + "Z\" fill=\"var(--_c)\" fill-opacity=\"0.28\" stroke=\"var(--_c)\" stroke-width=\"1.2\"/>";
        } else {
            body += "<rect x=\"" + num(x + 1) + "\" y=\"" + num(cy - 6) + "\" width=\"11\" height=\"13\" rx=\"1.5\" fill=\"var(--_s)\" stroke=\"var(--_b)\" stroke-width=\"1.2\"/>";
        }
        body += "<text x=\"" + num(x + 20.0) + "\" y=\"" + num(cy) + "\" dominant-baseline=\"central\" font-size=\"" + num(fontSize) + "\"" +
                (items[i].isFolder ? " font-weight=\"600\"" : "") + ">" + escText(items[i].name) + "</text>";
    }

    RenderResult r;
    r.type = "treeView";
    r.width = std::ceil(maxW);
    r.height = std::ceil(pad * 2.0 + std::max<size_t>(1, items.size()) * rowH);
    r.svg = svgDocument("treeView", r.width, r.height, fontSize, body);
    return r;
}

inline RenderResult renderVenn(const std::string& code, const RenderOptions& opts) {
    double fontSize = opts.fontSize;
    std::string title;
    struct VSet { std::string id, label; };
    std::vector<VSet> sets;
    std::vector<std::pair<std::vector<std::string>, std::string>> unions;

    for (const auto& rawLine : splitLines(code)) {
        std::string line = trim(rawLine);
        if (line.empty() || toLower(line).rfind("venn", 0) == 0) continue;
        std::string low = toLower(line);
        if (low.rfind("title ", 0) == 0) { title = unquote(trim(line.substr(6))); continue; }
        if (low.rfind("set ", 0) == 0) {
            std::string rest = trim(line.substr(4));
            size_t ob = rest.find('['), cb = rest.find(']');
            std::string id = trim(rest.substr(0, ob == std::string::npos ? rest.find(':') : ob));
            std::string lbl = (ob != std::string::npos && cb != std::string::npos) ? rest.substr(ob + 1, cb - ob - 1) : id;
            sets.push_back({unquote(id), unquote(lbl)});
        } else if (low.rfind("union ", 0) == 0) {
            std::string rest = trim(line.substr(6));
            size_t ob = rest.find('['), cb = rest.find(']');
            std::string idPart = trim(rest.substr(0, ob));
            std::string lbl = (ob != std::string::npos && cb != std::string::npos) ? rest.substr(ob + 1, cb - ob - 1) : "";
            std::vector<std::string> ids;
            for (const auto& s : splitChar(idPart, ',')) ids.push_back(trim(s));
            unions.push_back({ids, unquote(lbl)});
        }
    }
    if (sets.empty()) sets = {{"A", "A"}, {"B", "B"}};

    double pad = 28.0, R = 88.0, titleH = title.empty() ? 0.0 : 28.0;
    double cx = pad + R + 60.0, cy = pad + titleH + R + (sets.size() >= 3 ? 28.0 : 8.0);
    std::map<std::string, std::pair<double, double>> pos;
    std::string body;
    if (!title.empty()) body += labelSvg(cx, pad + 8.0, layoutLabel(title, fontSize * 1.15, true), fontSize * 1.15);

    for (size_t i = 0; i < sets.size(); i++) {
        double x = cx, y = cy, lx = cx, ly = cy;
        if (sets.size() == 2) {
            x = cx + (i == 0 ? -48.0 : 48.0);
            lx = x + (i == 0 ? -32.0 : 32.0);
        } else if (sets.size() >= 3) {
            double ang = -kPi / 2.0 + (2.0 * kPi * i) / sets.size();
            x = cx + std::cos(ang) * 48.0;
            y = cy + std::sin(ang) * 48.0;
            lx = cx + std::cos(ang) * 86.0;
            ly = cy + std::sin(ang) * 86.0;
        }
        pos[sets[i].id] = {x, y};
        std::string col = seriesColor(static_cast<int>(i));
        body += "<g class=\"pele-venn-set\"><circle cx=\"" + num(x) + "\" cy=\"" + num(y) + "\" r=\"" + num(R) +
                "\" fill=\"" + col + "\" fill-opacity=\"0.22\" stroke=\"" + col + "\" stroke-width=\"2\"/>" +
                labelSvg(lx, ly, layoutLabel(sets[i].label, fontSize, true), fontSize) + "</g>";
    }
    for (const auto& u : unions) {
        if (u.second.empty() || u.first.empty()) continue;
        double ux = 0.0, uy = 0.0;
        int cnt = 0;
        for (const auto& id : u.first) if (pos.count(id)) { ux += pos[id].first; uy += pos[id].second; cnt++; }
        if (cnt > 0) body += labelSvg(ux / cnt, uy / cnt, layoutLabel(u.second, fontSize * 0.9, false), fontSize * 0.9);
    }

    RenderResult r;
    r.type = "venn";
    r.width = std::ceil(cx * 2.0);
    r.height = std::ceil(cy + R + 54.0);
    r.svg = svgDocument("venn", r.width, r.height, fontSize, body);
    return r;
}

inline RenderResult renderIshikawa(const std::string& code, const RenderOptions& opts) {
    double fontSize = opts.fontSize;
    double smallFont = std::round(fontSize * 0.85);
    std::string effect = "Effect";
    struct IshCat { std::string name; std::vector<std::string> causes; };
    std::vector<IshCat> cats;
    int rootInd = -1, catInd = -1;

    for (const auto& rawLine : splitLines(code)) {
        std::string t = trim(rawLine);
        if (t.empty() || toLower(t).rfind("ishikawa", 0) == 0) continue;
        int ind = 0;
        for (char c : rawLine) { if (c == ' ') ind++; else if (c == '\t') ind += 4; else break; }
        std::string lbl = unquote(t);
        if (rootInd < 0) { rootInd = ind; effect = lbl; }
        else if (catInd < 0 || ind <= catInd) { catInd = ind; cats.push_back({lbl, {}}); }
        else if (!cats.empty()) cats.back().causes.push_back(lbl);
    }
    if (cats.empty()) cats = {{"People", {"Training"}}, {"Process", {"Steps"}}};

    size_t pairs = (cats.size() + 1) / 2;
    double pad = 28.0, pairStep = 148.0, spineLen = std::max(280.0, pairs * pairStep + 80.0);
    double cy = 170.0, startX = pad + 40.0, endX = startX + spineLen;

    std::string body;
    body += "<line class=\"pele-ishikawa-spine\" x1=\"" + num(startX) + "\" y1=\"" + num(cy) + "\" x2=\"" + num(endX) + "\" y2=\"" + num(cy) +
            "\" stroke=\"var(--_l)\" stroke-width=\"2.5\"/>" + marker("arrow_point", endX, cy, 0.0);
    TextBlock etb = layoutLabel(effect, fontSize, true);
    double ew = etb.width + 28.0, eh = etb.height + 20.0;
    body += "<rect x=\"" + num(endX + 4.0) + "\" y=\"" + num(cy - eh / 2.0) + "\" width=\"" + num(ew) + "\" height=\"" + num(eh) +
            "\" rx=\"6\" fill=\"var(--_c)\" fill-opacity=\"0.18\" stroke=\"var(--_c)\" stroke-width=\"1.8\"/>" +
            labelSvg(endX + 4.0 + ew / 2.0, cy, etb, fontSize);

    for (size_t i = 0; i < cats.size(); i++) {
        bool top = (i % 2 == 0);
        double dir = top ? -1.0 : 1.0;
        double joinX = endX - 60.0 - (i / 2) * pairStep;
        double tipX = joinX - 56.0, tipY = cy + dir * 115.0;
        std::string col = seriesColor(static_cast<int>(i));
        body += "<g class=\"pele-ishikawa-rib\"><line x1=\"" + num(tipX) + "\" y1=\"" + num(tipY) + "\" x2=\"" + num(joinX) + "\" y2=\"" + num(cy) +
                "\" stroke=\"" + col + "\" stroke-width=\"2\"/>";
        TextBlock ctb = layoutLabel(cats[i].name, smallFont, true);
        double cw = ctb.width + 18.0, ch = 24.0;
        body += "<rect x=\"" + num(tipX - cw / 2.0) + "\" y=\"" + num(tipY + (top ? -ch - 4.0 : 4.0)) + "\" width=\"" + num(cw) + "\" height=\"" + num(ch) +
                "\" rx=\"4\" fill=\"" + col + "\" fill-opacity=\"0.16\" stroke=\"" + col + "\" stroke-width=\"1.2\"/>" +
                labelSvg(tipX, tipY + (top ? -ch / 2.0 - 4.0 : ch / 2.0 + 4.0), ctb, smallFont);
        for (size_t ci = 0; ci < cats[i].causes.size(); ci++) {
            double frac = static_cast<double>(ci + 1) / (cats[i].causes.size() + 1);
            double bx = tipX + (joinX - tipX) * frac;
            double by = tipY + (cy - tipY) * frac;
            body += "<line x1=\"" + num(bx - 48.0) + "\" y1=\"" + num(by) + "\" x2=\"" + num(bx) + "\" y2=\"" + num(by) + "\" stroke=\"var(--_l)\" stroke-width=\"1.2\"/>"
                    "<text x=\"" + num(bx - 52.0) + "\" y=\"" + num(by - 3.0) + "\" text-anchor=\"end\" font-size=\"" + num(smallFont) + "\">" + escText(cats[i].causes[ci]) + "</text>";
        }
        body += "</g>";
    }

    RenderResult r;
    r.type = "ishikawa";
    r.width = std::ceil(endX + ew + pad + 12.0);
    r.height = std::ceil(cy * 2.0);
    r.svg = svgDocument("ishikawa", r.width, r.height, fontSize, body);
    return r;
}

inline RenderResult renderWardley(const std::string& code, const RenderOptions& opts) {
    double fontSize = opts.fontSize;
    double smallFont = std::round(fontSize * 0.84);
    std::string title;
    struct WComp { std::string name; double vis = 0.5, evo = 0.5; };
    std::vector<WComp> comps;
    std::map<std::string, int> cmap;
    std::vector<std::pair<int, int>> links;

    for (const auto& rawLine : splitLines(code)) {
        std::string line = trim(rawLine);
        if (line.empty() || toLower(line).rfind("wardley", 0) == 0) continue;
        std::string low = toLower(line);
        if (low.rfind("title ", 0) == 0) { title = unquote(trim(line.substr(6))); continue; }
        if (low.rfind("component ", 0) == 0 || low.rfind("anchor ", 0) == 0) {
            size_t ob = line.find('['), cb = line.find(']');
            std::string name = unquote(trim(line.substr(line.find(' ') + 1, ob - line.find(' ') - 1)));
            if (ob != std::string::npos && cb != std::string::npos) {
                auto xy = splitChar(line.substr(ob + 1, cb - ob - 1), ',');
                if (xy.size() >= 2) {
                    try {
                        int idx = static_cast<int>(comps.size());
                        comps.push_back({name, std::stod(trim(xy[0])), std::stod(trim(xy[1]))});
                        cmap[name] = idx;
                    } catch (...) {}
                }
            }
            continue;
        }
        size_t ar = line.find("->");
        if (ar != std::string::npos) {
            std::string u = unquote(trim(line.substr(0, ar)));
            std::string v = unquote(trim(line.substr(ar + 2)));
            if (cmap.count(u) && cmap.count(v)) links.push_back({cmap[u], cmap[v]});
        }
    }

    double pad = 32.0, plotW = 460.0, plotH = 260.0, titleH = title.empty() ? 0.0 : 28.0;
    double gx = pad + 26.0, gy = pad + titleH;
    std::string body;
    if (!title.empty()) body += labelSvg(gx + plotW / 2.0, pad + 8.0, layoutLabel(title, fontSize * 1.15, true), fontSize * 1.15);

    static const char* stages[] = {"Genesis", "Custom Built", "Product", "Commodity"};
    for (int i = 0; i < 4; i++) {
        double sx = gx + i * (plotW / 4.0);
        if (i > 0) body += "<line x1=\"" + num(sx) + "\" y1=\"" + num(gy) + "\" x2=\"" + num(sx) + "\" y2=\"" + num(gy + plotH) + "\" stroke=\"var(--_b)\" stroke-width=\"1\" stroke-dasharray=\"4 4\"/>";
        body += labelSvg(sx + plotW / 8.0, gy + plotH + 16.0, layoutLabel(stages[i], smallFont, false), smallFont, "middle", "var(--_m)");
    }
    body += "<path d=\"M" + num(gx) + " " + num(gy) + "V" + num(gy + plotH) + "H" + num(gx + plotW) + "\" fill=\"none\" stroke=\"var(--_b)\" stroke-width=\"var(--_w)\"/>"
            "<text x=\"" + num(gx + plotW) + "\" y=\"" + num(gy + plotH + 32.0) + "\" text-anchor=\"end\" font-size=\"" + num(smallFont) + "\" font-weight=\"600\" fill=\"var(--_m)\">Evolution</text>"
            "<text x=\"" + num(pad - 6.0) + "\" y=\"" + num(gy + plotH / 2.0) + "\" text-anchor=\"middle\" font-size=\"" + num(smallFont) + "\" font-weight=\"600\" fill=\"var(--_m)\" transform=\"rotate(-90 " + num(pad - 6.0) + " " + num(gy + plotH / 2.0) + ")\">Visibility</text>";

    for (const auto& lk : links) {
        double x1 = gx + comps[lk.first].evo * plotW, y1 = gy + (1.0 - comps[lk.first].vis) * plotH;
        double x2 = gx + comps[lk.second].evo * plotW, y2 = gy + (1.0 - comps[lk.second].vis) * plotH;
        body += "<line class=\"pele-wardley-link\" x1=\"" + num(x1) + "\" y1=\"" + num(y1) + "\" x2=\"" + num(x2) + "\" y2=\"" + num(y2) + "\" stroke=\"var(--_l)\" stroke-width=\"1.4\"/>";
    }
    for (const auto& c : comps) {
        double x = gx + c.evo * plotW, y = gy + (1.0 - c.vis) * plotH;
        body += "<g class=\"pele-wardley-component\"><circle cx=\"" + num(x) + "\" cy=\"" + num(y) + "\" r=\"5.5\" fill=\"var(--_bg)\" stroke=\"var(--_c)\" stroke-width=\"2\"/>"
                "<text x=\"" + num(x + 9.0) + "\" y=\"" + num(y) + "\" dominant-baseline=\"central\" font-size=\"" + num(smallFont) + "\">" + escText(c.name) + "</text></g>";
    }

    RenderResult r;
    r.type = "wardley";
    r.width = std::ceil(gx + plotW + 64.0);
    r.height = std::ceil(gy + plotH + 38.0 + pad);
    r.svg = svgDocument("wardley", r.width, r.height, fontSize, body);
    return r;
}

inline RenderResult renderCynefin(const std::string& code, const RenderOptions& opts) {
    double fontSize = opts.fontSize;
    double smallFont = std::round(fontSize * 0.85);
    std::string title;
    std::map<std::string, std::vector<std::string>> domItems;
    std::string curDom = "complex";

    for (const auto& rawLine : splitLines(code)) {
        std::string t = trim(rawLine);
        if (t.empty() || toLower(t).rfind("cynefin", 0) == 0) continue;
        std::string low = toLower(t);
        if (low.rfind("title ", 0) == 0) { title = unquote(trim(t.substr(6))); continue; }
        if (low == "complex" || low == "complicated" || low == "chaotic" || low == "clear" || low == "simple" || low == "obvious" || low == "disorder" || low == "confused" || low == "confusion") {
            curDom = (low == "simple" || low == "obvious") ? "clear" : (low == "disorder" || low == "confusion") ? "confused" : low;
            continue;
        }
        if (t.front() == '-' || t.front() == '*') t = trim(t.substr(1));
        domItems[curDom].push_back(unquote(t));
    }

    double pad = 24.0, qW = 210.0, qH = 140.0, gap = 10.0, titleH = title.empty() ? 0.0 : 28.0;
    double topY = pad + titleH;
    struct QSpec { const char* key; const char* name; const char* sub1; const char* sub2; int col, row, colorIdx; };
    static const QSpec specs[] = {
        {"complex", "Complex", "Probe \xE2\x86\x92 Sense \xE2\x86\x92 Respond", "Emergent Practices", 0, 0, 0},
        {"complicated", "Complicated", "Sense \xE2\x86\x92 Analyse \xE2\x86\x92 Respond", "Good Practices", 1, 0, 1},
        {"chaotic", "Chaotic", "Act \xE2\x86\x92 Sense \xE2\x86\x92 Respond", "Novel Practices", 0, 1, 5},
        {"clear", "Clear", "Sense \xE2\x86\x92 Categorise \xE2\x86\x92 Respond", "Best Practices", 1, 1, 2},
    };
    std::string body;
    double totalW = pad * 2.0 + qW * 2.0 + gap;
    if (!title.empty()) body += labelSvg(totalW / 2.0, pad + 8.0, layoutLabel(title, fontSize * 1.15, true), fontSize * 1.15);

    for (const auto& sp : specs) {
        double x = pad + sp.col * (qW + gap);
        double y = topY + sp.row * (qH + gap);
        std::string col = seriesColor(sp.colorIdx);
        body += "<g class=\"pele-cynefin-domain\">"
                "<rect x=\"" + num(x) + "\" y=\"" + num(y) + "\" width=\"" + num(qW) + "\" height=\"" + num(qH) +
                "\" rx=\"8\" fill=\"" + col + "\" fill-opacity=\"0.14\" stroke=\"" + col + "\" stroke-width=\"1.5\"/>" +
                labelSvg(x + 14.0, y + 16.0, layoutLabel(sp.name, fontSize, true), fontSize, "start") +
                "<text class=\"pele-cynefin-subtitle\" x=\"" + num(x + 14.0) + "\" y=\"" + num(y + 31.0) + "\" font-size=\"" + num(smallFont * 0.78) + "\" fill=\"var(--_m)\">" + escText(sp.sub1) + "</text>"
                "<text class=\"pele-cynefin-subtitle\" x=\"" + num(x + 14.0) + "\" y=\"" + num(y + 43.0) + "\" font-size=\"" + num(smallFont * 0.78) + "\" fill=\"var(--_m)\">" + escText(sp.sub2) + "</text>";
        const auto& list = domItems[sp.key];
        for (size_t i = 0; i < list.size(); i++) {
            double iy = y + 54.0 + i * 26.0;
            body += "<rect x=\"" + num(x + 12.0) + "\" y=\"" + num(iy) + "\" width=\"" + num(qW - 24.0) + "\" height=\"22\" rx=\"4\" fill=\"var(--_s)\" stroke=\"var(--_b)\" stroke-width=\"1\"/>" +
                    labelSvg(x + qW / 2.0, iy + 11.0, layoutLabel(list[i], smallFont, false), smallFont);
        }
        body += "</g>";
    }
    double midX = pad + qW + gap / 2.0, midY = topY + qH + gap / 2.0;
    body += "<g class=\"pele-cynefin-center\">"
            "<ellipse cx=\"" + num(midX) + "\" cy=\"" + num(midY) + "\" rx=\"38\" ry=\"18\" fill=\"var(--_bg)\" stroke=\"var(--_b)\" stroke-width=\"1.2\"/>"
            "<text x=\"" + num(midX) + "\" y=\"" + num(midY - 4.0) + "\" text-anchor=\"middle\" dominant-baseline=\"central\" font-size=\"" + num(smallFont * 0.75) + "\" font-weight=\"600\">Confusion</text>"
            "<text x=\"" + num(midX) + "\" y=\"" + num(midY + 6.0) + "\" text-anchor=\"middle\" dominant-baseline=\"central\" font-size=\"" + num(smallFont * 0.68) + "\" fill=\"var(--_m)\">Disorder</text></g>";

    RenderResult r;
    r.type = "cynefin";
    r.width = std::ceil(totalW);
    r.height = std::ceil(topY + qH * 2.0 + gap + pad);
    r.svg = svgDocument("cynefin", r.width, r.height, fontSize, body);
    return r;
}

inline RenderResult renderRailroad(const std::string& code, const RenderOptions& opts) {
    double fontSize = opts.fontSize;
    struct RRule { std::string name; std::vector<std::string> tokens; };
    std::vector<RRule> rules;

    for (const auto& rawLine : splitLines(code)) {
        std::string line = trim(rawLine);
        if (line.empty() || toLower(line).rfind("railroad", 0) == 0 || toLower(line).rfind("title ", 0) == 0) continue;
        size_t pos = line.find("::=");
        size_t opLen = 3;
        if (pos == std::string::npos) { pos = line.find('='); opLen = 1; }
        if (pos == std::string::npos) { pos = line.find(':'); opLen = 1; }
        if (pos != std::string::npos) {
            RRule rr;
            rr.name = trim(line.substr(0, pos));
            std::string rhs = trim(line.substr(pos + opLen));
            if (!rhs.empty() && rhs.back() == ';') rhs = trim(rhs.substr(0, rhs.size() - 1));
            size_t i = 0;
            while (i < rhs.size()) {
                while (i < rhs.size() && (rhs[i] == ' ' || rhs[i] == '\t' || rhs[i] == '|')) i++;
                if (i >= rhs.size()) break;
                if (rhs[i] == '"' || rhs[i] == '\'') {
                    char q = rhs[i++];
                    size_t st = i;
                    while (i < rhs.size() && rhs[i] != q) i++;
                    rr.tokens.push_back("\"" + rhs.substr(st, i - st) + "\"");
                    if (i < rhs.size()) i++;
                } else {
                    size_t st = i;
                    while (i < rhs.size() && rhs[i] != ' ' && rhs[i] != '\t' && rhs[i] != '|') i++;
                    std::string tok = rhs.substr(st, i - st);
                    if (!tok.empty()) rr.tokens.push_back(tok);
                }
            }
            rules.push_back(rr);
        }
    }
    if (rules.empty()) rules.push_back({"expr", {"\"id\""}});

    double pad = 24.0, curY = pad, maxW = 240.0;
    std::string body;
    for (const auto& rr : rules) {
        body += labelSvg(pad, curY + 10.0, layoutLabel(rr.name, fontSize, true), fontSize, "start");
        curY += 32.0;
        double x = pad + 12.0, cy = curY + 18.0;
        body += "<circle cx=\"" + num(x) + "\" cy=\"" + num(cy) + "\" r=\"4\" fill=\"var(--_l)\"/>"
                "<circle cx=\"" + num(x + 8.0) + "\" cy=\"" + num(cy) + "\" r=\"4\" fill=\"none\" stroke=\"var(--_l)\" stroke-width=\"1.5\"/>";
        x += 12.0;
        for (const auto& tok : rr.tokens) {
            bool isTerm = (!tok.empty() && tok.front() == '"');
            std::string lbl = unquote(tok);
            double tw = measureTextWidth(lbl, fontSize, false, isTerm) + 24.0;
            double th = 28.0;
            body += "<line x1=\"" + num(x) + "\" y1=\"" + num(cy) + "\" x2=\"" + num(x + 20.0) + "\" y2=\"" + num(cy) + "\" stroke=\"var(--_l)\" stroke-width=\"var(--_w)\"/>";
            x += 20.0;
            double rx = isTerm ? 14.0 : 3.0;
            std::string fill = isTerm ? "var(--_c)" : "var(--_s)";
            std::string op = isTerm ? " fill-opacity=\"0.16\"" : "";
            body += "<g class=\"pele-railroad-node\"><rect x=\"" + num(x) + "\" y=\"" + num(cy - th / 2.0) + "\" width=\"" + num(tw) + "\" height=\"" + num(th) +
                    "\" rx=\"" + num(rx) + "\" fill=\"" + fill + "\"" + op + " stroke=\"var(--_b)\" stroke-width=\"var(--_w)\"/>" +
                    labelSvg(x + tw / 2.0, cy, layoutLabel(lbl, fontSize, false), fontSize) + "</g>";
            x += tw;
        }
        body += "<line x1=\"" + num(x) + "\" y1=\"" + num(cy) + "\" x2=\"" + num(x + 20.0) + "\" y2=\"" + num(cy) + "\" stroke=\"var(--_l)\" stroke-width=\"var(--_w)\"/>";
        x += 20.0;
        body += "<circle cx=\"" + num(x) + "\" cy=\"" + num(cy) + "\" r=\"4\" fill=\"none\" stroke=\"var(--_l)\" stroke-width=\"1.5\"/>"
                "<circle cx=\"" + num(x + 8.0) + "\" cy=\"" + num(cy) + "\" r=\"4\" fill=\"var(--_l)\"/>";
        maxW = std::max(maxW, x + 16.0 + pad);
        curY += 52.0;
    }

    RenderResult r;
    r.type = "railroad";
    r.width = std::ceil(maxW);
    r.height = std::ceil(curY);
    r.svg = svgDocument("railroad", r.width, r.height, fontSize, body);
    return r;
}

inline RenderResult renderEventModeling(const std::string& code, const RenderOptions& opts) {
    double fontSize = opts.fontSize;
    double smallFont = std::round(fontSize * 0.85);
    std::string title;
    struct EmEntity { std::string id, label, kind; int row = 1; };
    std::vector<EmEntity> ents;
    std::map<std::string, int> emap;
    std::vector<std::pair<int, int>> flows;

    for (const auto& rawLine : splitLines(code)) {
        std::string line = trim(rawLine);
        if (line.empty() || toLower(line).rfind("eventmodeling", 0) == 0 || toLower(line).rfind("event-modeling", 0) == 0) continue;
        std::string low = toLower(line);
        if (low.rfind("title ", 0) == 0) { title = unquote(trim(line.substr(6))); continue; }
        size_t ar = line.find("->>");
        size_t arLen = 3;
        if (ar == std::string::npos) { ar = line.find("->"); arLen = 2; }
        if (ar != std::string::npos && (low.rfind("tf ", 0) != 0 && low.rfind("ui ", 0) != 0 && low.rfind("cmd ", 0) != 0 && low.rfind("command ", 0) != 0 && low.rfind("evt ", 0) != 0 && low.rfind("event ", 0) != 0 && low.rfind("view ", 0) != 0 && low.rfind("rmo ", 0) != 0 && low.rfind("pcr ", 0) != 0)) {
            std::string u = trim(line.substr(0, ar));
            std::string v = trim(line.substr(ar + arLen));
            size_t col = v.find(':');
            if (col != std::string::npos) v = trim(v.substr(0, col));
            if (emap.count(u) && emap.count(v)) flows.push_back({emap[u], emap[v]});
            continue;
        }
        std::istringstream iss(line);
        std::string kw, id;
        iss >> kw;
        if (toLower(kw) == "tf" || toLower(kw) == "timeframe" || toLower(kw) == "rf" || toLower(kw) == "resetframe") {
            std::string tfNum;
            iss >> tfNum >> kw;
        }
        iss >> id;
        std::string lkw = toLower(kw);
        int row = -1;
        if (lkw == "ui" || lkw == "screen" || lkw == "automation" || lkw == "processor" || lkw == "pcr") row = 0;
        else if (lkw == "command" || lkw == "cmd" || lkw == "view" || lkw == "readmodel" || lkw == "rm" || lkw == "rmo") row = 1;
        else if (lkw == "event" || lkw == "evt") row = 2;
        if (row >= 0 && !id.empty()) {
            std::string rest;
            std::getline(iss, rest);
            for (const char* cutTok : {"{", "[[", "->>", "->"}) {
                size_t cp = rest.find(cutTok);
                if (cp != std::string::npos) rest = rest.substr(0, cp);
            }
            std::string lbl = unquote(trim(rest));
            if (lbl.empty()) lbl = id;
            int idx = static_cast<int>(ents.size());
            ents.push_back({id, lbl, lkw, row});
            emap[id] = idx;
        }
    }
    if (ents.empty()) ents.push_back({"e1", "Event", "evt", 2});
    if (flows.empty() && ents.size() > 1) {
        for (size_t i = 0; i + 1 < ents.size(); i++) flows.push_back({static_cast<int>(i), static_cast<int>(i + 1)});
    }

    double pad = 24.0, laneLblW = 128.0, colStep = 148.0, laneH = 82.0, cardW = 124.0, cardH = 44.0;
    double titleH = title.empty() ? 0.0 : 28.0;
    double topY = pad + titleH;
    double totalW = pad * 2.0 + laneLblW + ents.size() * colStep + 16.0;
    static const char* laneNames[] = {"UI/Automation", "Command/Read Model", "Events"};

    std::string body;
    if (!title.empty()) body += labelSvg(totalW / 2.0, pad + 8.0, layoutLabel(title, fontSize * 1.15, true), fontSize * 1.15);
    for (int r = 0; r < 3; r++) {
        double ly = topY + r * laneH;
        body += "<rect x=\"" + num(pad) + "\" y=\"" + num(ly) + "\" width=\"" + num(totalW - pad * 2.0) + "\" height=\"" + num(laneH - 6.0) +
                "\" rx=\"6\" fill=\"var(--_a)\" fill-opacity=\"0.38\" stroke=\"var(--_b)\" stroke-width=\"1\"/>" +
                labelSvg(pad + 12.0, ly + (laneH - 6.0) / 2.0, layoutLabel(laneNames[r], smallFont, true), smallFont, "start", "var(--_m)");
    }

    std::vector<std::pair<double, double>> centers(ents.size());
    for (size_t i = 0; i < ents.size(); i++) {
        double cx = pad + laneLblW + (i + 0.5) * colStep;
        double cy = topY + ents[i].row * laneH + (laneH - 6.0) / 2.0;
        centers[i] = {cx, cy};
    }
    for (const auto& fl : flows) {
        auto [x1, y1] = centers[fl.first];
        auto [x2, y2] = centers[fl.second];
        double sy = y1 + (y2 > y1 ? cardH / 2.0 : -cardH / 2.0);
        double ey = y2 + (y2 > y1 ? -cardH / 2.0 : cardH / 2.0);
        double ang = std::atan2(ey - sy, x2 - x1);
        body += "<g class=\"pele-edge\"><line x1=\"" + num(x1) + "\" y1=\"" + num(sy) + "\" x2=\"" + num(x2 - std::cos(ang) * 7.0) + "\" y2=\"" + num(ey - std::sin(ang) * 7.0) +
                "\" stroke=\"var(--_l)\" stroke-width=\"var(--_w)\"/>" + marker("arrow_point", x2, ey, ang) + "</g>";
    }
    for (size_t i = 0; i < ents.size(); i++) {
        auto [cx, cy] = centers[i];
        std::string k = ents[i].kind;
        std::string col = (k == "evt" || k == "event") ? seriesColor(1) : (k == "cmd" || k == "command") ? seriesColor(0) : (k == "view" || k == "readmodel" || k == "rm") ? seriesColor(2) : "var(--_b)";
        body += "<g class=\"pele-em-card\"><rect x=\"" + num(cx - cardW / 2.0) + "\" y=\"" + num(cy - cardH / 2.0) + "\" width=\"" + num(cardW) + "\" height=\"" + num(cardH) +
                "\" rx=\"6\" fill=\"" + col + "\" fill-opacity=\"0.2\" stroke=\"" + col + "\" stroke-width=\"1.4\"/>" +
                labelSvg(cx, cy, layoutLabel(ents[i].label, smallFont, false, cardW - 14.0), smallFont) + "</g>";
    }

    RenderResult r;
    r.type = "eventmodeling";
    r.width = std::ceil(totalW);
    r.height = std::ceil(topY + 3.0 * laneH + pad);
    r.svg = svgDocument("eventmodeling", r.width, r.height, fontSize, body);
    return r;
}

inline RenderResult renderInfo(const RenderOptions& opts) {
    double fontSize = opts.fontSize;
    TextBlock tb = layoutLabel("Pele v1.12.0", fontSize, false);
    double w = std::ceil(tb.width + 48.0), h = std::ceil(tb.height + 32.0);
    std::string body = "<rect x=\"8\" y=\"8\" width=\"" + num(w - 16.0) + "\" height=\"" + num(h - 16.0) +
                       "\" rx=\"var(--_r)\" fill=\"var(--_s)\" stroke=\"var(--_b)\" stroke-width=\"var(--_w)\"/>" +
                       labelSvg(w / 2.0, h / 2.0, tb, fontSize);
    RenderResult r;
    r.type = "info";
    r.width = w;
    r.height = h;
    r.svg = svgDocument("info", w, h, fontSize, body);
    return r;
}

} // namespace detail

// ---------------------------------------------------------------------------
// 20. Public Entry Point: Pele::render & Pele::latexToMathML
// ---------------------------------------------------------------------------

inline MathBox typesetMath(const std::string& tex, double fontSize = 16.0, bool displayMode = true) {
    return detail::typesetLatexMath(tex, fontSize, displayMode);
}

inline std::string latexToMathML(const std::string& tex, bool displayMode = true) {
    return detail::typesetLatexMath(tex, 16.0, displayMode).html;
}

inline RenderResult render(const std::string& input, const RenderOptions& options = RenderOptions()) {
    using namespace detail;
    activeMathResolver() = options.math;
    Preprocessed prep = preprocess(input);
    Detected det = detectDiagram(prep.code);
    std::string dtype = det.type;
    RenderResult res;
    if (dtype == "flowchart" || dtype == "graph" || dtype == "flowchart-elk" || dtype == "swimlane" || dtype == "agentflow") {
        res = renderFlowchart(prep.code, dtype, options);
    } else if (dtype == "sequence") res = renderSequence(prep.code, options);
    else if (dtype == "class") res = renderClass(prep.code, options);
    else if (dtype == "state") res = renderState(prep.code, options);
    else if (dtype == "er") res = renderEr(prep.code, options);
    else if (dtype == "pie") res = renderPie(prep.code, options);
    else if (dtype == "xychart") res = renderXyChart(prep.code, options);
    else if (dtype == "gantt") res = renderGantt(prep.code, options);
    else if (dtype == "mindmap") res = renderMindmap(prep.code, options);
    else if (dtype == "gitGraph") res = renderGitGraph(prep.code, options);
    else if (dtype == "timeline") res = renderTimeline(prep.code, options);
    else if (dtype == "journey") res = renderJourney(prep.code, options);
    else if (dtype == "quadrantChart") res = renderQuadrant(prep.code, options);
    else if (dtype == "sankey") res = renderSankey(prep.code, options);
    else if (dtype == "c4") res = renderC4(prep.code, options);
    else if (dtype == "block") res = renderBlock(prep.code, options);
    else if (dtype == "packet") res = renderPacket(prep.code, options);
    else if (dtype == "architecture") res = renderArchitecture(prep.code, options);
    else if (dtype == "kanban") res = renderKanban(prep.code, options);
    else if (dtype == "radar") res = renderRadar(prep.code, options);
    else if (dtype == "treemap") res = renderTreemap(prep.code, options);
    else if (dtype == "treeView") res = renderTreeView(prep.code, options);
    else if (dtype == "venn") res = renderVenn(prep.code, options);
    else if (dtype == "requirement") res = renderRequirement(prep.code, options);
    else if (dtype == "ishikawa") res = renderIshikawa(prep.code, options);
    else if (dtype == "wardley") res = renderWardley(prep.code, options);
    else if (dtype == "cynefin") res = renderCynefin(prep.code, options);
    else if (dtype == "railroad") res = renderRailroad(prep.code, options);
    else if (dtype == "eventmodeling") res = renderEventModeling(prep.code, options);
    else if (dtype == "usecase") res = renderUsecase(prep.code, options);
    else if (dtype == "info") res = renderInfo(options);
    res.ok = !res.svg.empty();
    res.mermaidType = det.mermaidType;
    return res;
}

} // namespace Pele

