#pragma once
#include "flowchart.h"

namespace Pele {
namespace Usecase {

namespace detail {

using Span = std::pair<int, int>;

enum Arrow {
    SOLID_ARROW = 0,
    BACK_ARROW = 1,
    LINE_SOLID = 2,
    CIRCLE_ARROW = 3,
    CROSS_ARROW = 4,
    CIRCLE_ARROW_REVERSED = 5,
    CROSS_ARROW_REVERSED = 6
};

struct JsonVal {
    enum Type { Null, Bool, Number, String, Array, Object } type = Null;
    std::string scalar;
    std::vector<JsonVal> arr;
    std::unordered_map<std::string, JsonVal> obj;

    bool isPrimitiveOrNull() const {
        return type != Array && type != Object;
    }
};

struct OrderedJson {
    JsonVal value;
    std::unordered_map<std::string, std::vector<std::string>> propertyOrder;
};

struct Actor {
    std::string id;
    std::string label;
    std::string labelType = "text";
    std::string type = "normal";
    std::string icon;
    bool business = false;
    std::string stereotype;
    std::string parentId;
    std::vector<std::string> classes;
    std::vector<std::string> styles;
};

struct UseCase {
    std::string id;
    std::string label;
    std::string labelType = "text";
    std::string shape = "ellipse";
    bool business = false;
    std::string stereotype;
    std::string parentId;
    std::vector<std::string> classes;
    std::vector<std::string> styles;
};

struct SystemBoundary {
    std::string id;
    std::string label;
    std::string labelType = "text";
    std::string type = "rect";
    std::vector<std::string> members;
    std::vector<std::string> classes;
    std::vector<std::string> styles;
};

struct Relationship {
    std::string id;
    bool explicitId = false;
    std::string source;
    std::string target;
    std::string type = "association";
    int arrowType = SOLID_ARROW;
    std::string label;
    bool hasLabel = false;
    std::string labelType;
    bool hasLabelType = false;
    int minlen = 1;
    std::vector<std::string> classes;
    std::vector<std::string> styles;
    bool animate = false;
    std::string animation;
};

struct UsecaseNote {
    std::string id;
    std::string target;
    std::string label;
    std::string labelType = "text";
};

struct UsecaseJsonNode {
    std::string id;
    JsonVal value;
    std::unordered_map<std::string, std::vector<std::string>> propertyOrder;
    std::vector<std::string> classes;
    std::vector<std::string> styles;
};

struct ClassDef {
    std::string id;
    std::vector<std::string> styles;
};

struct UsecaseModel {
    std::string type = "usecase";
    std::vector<std::string> actorOrder;
    std::unordered_map<std::string, Actor> actors;
    std::vector<std::string> useCaseOrder;
    std::unordered_map<std::string, UseCase> useCases;
    std::vector<std::string> boundaryOrder;
    std::unordered_map<std::string, SystemBoundary> systemBoundaries;
    std::vector<Relationship> relationships;
    std::vector<std::string> noteOrder;
    std::unordered_map<std::string, UsecaseNote> notes;
    std::vector<std::string> jsonOrder;
    std::unordered_map<std::string, UsecaseJsonNode> jsonNodes;
    std::vector<std::string> classDefOrder;
    std::unordered_map<std::string, ClassDef> classDefs;
    std::string direction = "LR";
    std::string title;
    std::string accTitle;
    std::string accDescr;
    std::string accDescription;
};

// ============================================================================
// src/diagrams/usecase/tokens.ts & lexer.ts
// ============================================================================

enum T {
    T_EOF = 0,
    USECASE,
    ACTOR,
    SYSTEM_BOUNDARY,
    END,
    DIRECTION,
    TD,
    TB,
    BT,
    LR,
    RL,
    NOTE,
    FOR,
    JSON,
    CLASS_DEF,
    CLASS,
    STYLE,
    INCLUDE,
    EXTEND,
    TRUE_TOK,
    FALSE_TOK,
    IDENTIFIER,
    NUMBER,
    AT,
    COMMA,
    COLON,
    HASH_COLOR,
    CSS_IDENTIFIER,
    CSS_ESCAPED_COMMA,
    DASH,
    DOT,
    PERCENT,
    CSS_PUNCTUATION,
    LABEL_PUNCTUATION,
    LABEL_SYMBOL,
    PLAIN_STRING,
    MARKDOWN_STRING,
    UNCLOSED_MARKDOWN_STRING,
    COMMENT,
    NEWLINE,
    ACC_DESCR_BLOCK,
    ACC_TITLE_LINE,
    ACC_DESCR_LINE,
    JSON_DECLARATION_START,
    JSON_OBJECT_LITERAL,
    UNCLOSED_JSON_OBJECT_LITERAL,
    GENERALIZATION,
    DEPENDENCY_ARROW,
    STEREOTYPE_START,
    STEREOTYPE_END,
    STEREOTYPE_TEXT,
    UNCLOSED_STEREOTYPE_TEXT,
    CLASS_SEPARATOR,
    FORWARD_SOLID,
    BACKWARD_SOLID,
    FORWARD_CIRCLE,
    BACKWARD_CIRCLE,
    FORWARD_CROSS,
    BACKWARD_CROSS,
    MARKERLESS_SOLID,
    METADATA_START,
    LBRACE,
    RBRACE,
    LBRACKET,
    RBRACKET,
    LPAREN,
    RPAREN
};

inline const std::vector<std::string>& tokenNames() noexcept {
    static const std::vector<std::string> names = splitStr(
        "EOF USECASE ACTOR SYSTEM_BOUNDARY END DIRECTION TD TB BT LR RL NOTE FOR JSON CLASS_DEF CLASS STYLE INCLUDE EXTEND TRUE FALSE IDENTIFIER NUMBER AT COMMA COLON HASH_COLOR CSS_IDENTIFIER CSS_ESCAPED_COMMA DASH DOT PERCENT CSS_PUNCTUATION LABEL_PUNCTUATION LABEL_SYMBOL PLAIN_STRING MARKDOWN_STRING UNCLOSED_MARKDOWN_STRING COMMENT NEWLINE ACC_DESCR_BLOCK ACC_TITLE_LINE ACC_DESCR_LINE JSON_DECLARATION_START JSON_OBJECT_LITERAL UNCLOSED_JSON_OBJECT_LITERAL GENERALIZATION DEPENDENCY_ARROW STEREOTYPE_START STEREOTYPE_END STEREOTYPE_TEXT UNCLOSED_STEREOTYPE_TEXT CLASS_SEPARATOR FORWARD_SOLID BACKWARD_SOLID FORWARD_CIRCLE BACKWARD_CIRCLE FORWARD_CROSS BACKWARD_CROSS MARKERLESS_SOLID METADATA_START LBRACE RBRACE LBRACKET RBRACKET LPAREN RPAREN",
        ' '
    );
    return names;
}

struct Tokens {
    std::vector<int> types;
    std::vector<int> starts;
    std::vector<int> ends;
};

inline const std::array<uint8_t, 128>& lexClassTable() {
    static const std::array<uint8_t, 128> tbl = []() {
        std::array<uint8_t, 128> a{};
        for (int c = 48; c <= 57; c++) a[c] = 1;
        for (int c = 65; c <= 90; c++) a[c] = a[c + 32] = 1;
        a[95] = 1;
        const char* punct = "!#$&*+/=?^|~";
        for (const char* p = punct; *p; ++p) a[static_cast<unsigned char>(*p)] = 2;
        return a;
    }();
    return tbl;
}

inline bool isLexWord(int c) {
    return c >= 0 && c < 128 && (lexClassTable()[c] & 1) != 0;
}

inline bool startsWithNoCase(const std::string& src, int at, const std::string& word) noexcept {
    if (at < 0 || static_cast<size_t>(at) + word.size() > src.size()) return false;
    for (size_t k = 0; k < word.size(); k++) {
        if ((static_cast<unsigned char>(src[at + k]) | 32) != static_cast<unsigned char>(word[k])) return false;
    }
    return true;
}

inline int keywordAt(const std::string& src, int at, int c) noexcept {
    size_t pos = static_cast<size_t>(at);
    switch (c) {
        case 97: return startsWith(src, "actor", pos) ? ACTOR : 0;
        case 115: return startsWith(src, "systemBoundary", pos) ? SYSTEM_BOUNDARY : startsWith(src, "style", pos) ? STYLE : 0;
        case 101: return startsWith(src, "end", pos) ? END : startsWithNoCase(src, at, "extend") ? EXTEND : 0;
        case 69: return startsWithNoCase(src, at, "extend") ? EXTEND : 0;
        case 100: return startsWith(src, "direction", pos) ? DIRECTION : 0;
        case 84: return startsWith(src, "TD", pos) ? TD : startsWith(src, "TB", pos) ? TB : 0;
        case 66: return startsWith(src, "BT", pos) ? BT : 0;
        case 76: return startsWith(src, "LR", pos) ? LR : 0;
        case 82: return startsWith(src, "RL", pos) ? RL : 0;
        case 110: return startsWith(src, "note", pos) ? NOTE : 0;
        case 102: return startsWith(src, "for", pos) ? FOR : startsWith(src, "false", pos) ? FALSE_TOK : 0;
        case 106: return startsWith(src, "json", pos) ? JSON : 0;
        case 99: return startsWith(src, "classDef", pos) ? CLASS_DEF : startsWith(src, "class", pos) ? CLASS : 0;
        case 105:
        case 73: return startsWithNoCase(src, at, "include") ? INCLUDE : 0;
        case 116: return startsWith(src, "true", pos) ? TRUE_TOK : 0;
        case 117: return startsWith(src, "usecase-beta", pos) ? USECASE : 0;
        default: return 0;
    }
}

inline std::pair<int, int> lineColumn(const std::string& src, int offset) noexcept {
    int line = 1;
    int lineStart = 0;
    int limit = std::min(offset, static_cast<int>(src.size()));
    for (int i = 0; i < limit; i++) {
        int c = charCodeAt(src, i);
        if (c != 10 && c != 13) continue;
        if (c == 13 && i + 1 < limit && charCodeAt(src, i + 1) == 10) i++;
        line++;
        lineStart = i + 1;
    }
    return {line, limit - lineStart + 1};
}

inline int stringEnd(const std::string& src, int at, int quote) noexcept {
    int n = static_cast<int>(src.size());
    for (int k = at + 1; k < n; k++) {
        int c = charCodeAt(src, k);
        if (c == quote) return k + 1;
        if (c == 10 || c == 13) return -1;
    }
    return -1;
}

inline bool startsToken(const std::string& src, int at) noexcept {
    int c = charCodeAt(src, at);
    if (c != 34 && c != 39) return true;
    return (c == 34 && charCodeAt(src, at + 1) == 96) || stringEnd(src, at, c) != -1;
}

inline Tokens tokenize(const std::string& src) {
    static const int KEYWORD_LENGTH[] = {0, 12, 5, 14, 3, 9, 2, 2, 2, 2, 2, 4, 3, 4, 8, 5, 5, 7, 6, 4, 5};
    const auto& CLASS_TBL = lexClassTable();
    std::vector<int> types;
    std::vector<int> starts;
    std::vector<int> ends;
    int n = static_cast<int>(src.size());
    int i = 0;
    bool lineStart = true;
    bool noBrace = false;

    while (i < n) {
        int c = charCodeAt(src, i);
        if (c == 32 || c == 9) {
            i++;
            continue;
        }
        int type = 0;
        int end = i + 1;
        bool fresh = false;

        if (c >= 0 && c < 128 && (CLASS_TBL[c] & 1) != 0) {
            type = 0;
            if (c == 97 && lineStart && startsWith(src, "acc", static_cast<size_t>(i))) {
                bool descr = startsWith(src, "Descr", static_cast<size_t>(i + 3));
                if (descr || startsWith(src, "Title", static_cast<size_t>(i + 3))) {
                    int j = i + 8;
                    while (isBlank(charCodeAt(src, j))) j++;
                    int next = charCodeAt(src, j);
                    if (next == 58) {
                        type = descr ? ACC_DESCR_LINE : ACC_TITLE_LINE;
                        end = j + 1;
                        while (end < n && charCodeAt(src, end) != 10 && charCodeAt(src, end) != 13) end++;
                    } else if (next == 123 && descr && !noBrace) {
                        size_t close = src.find('}', static_cast<size_t>(j + 1));
                        if (close == std::string::npos) noBrace = true;
                        else {
                            type = ACC_DESCR_BLOCK;
                            end = static_cast<int>(close) + 1;
                        }
                    }
                }
            } else if (c == 106 && startsWith(src, "json", static_cast<size_t>(i)) && isBlank(charCodeAt(src, i + 4))) {
                int j = i + 5;
                while (isBlank(charCodeAt(src, j))) j++;
                int id = j;
                while (isLexWord(charCodeAt(src, j))) j++;
                if (j > id) {
                    while (isBlank(charCodeAt(src, j))) j++;
                    if (charCodeAt(src, j) == 64) {
                        j++;
                        while (isBlank(charCodeAt(src, j))) j++;
                        if (charCodeAt(src, j) == 123) {
                            types.push_back(JSON_DECLARATION_START);
                            starts.push_back(i);
                            ends.push_back(j);
                            i = j;
                            int depth = 0;
                            bool quoted = false;
                            bool escaped = false;
                            int k = j;
                            for (; k < n; k++) {
                                int ch = charCodeAt(src, k);
                                if (quoted) {
                                    if (escaped) escaped = false;
                                    else if (ch == 92) escaped = true;
                                    else if (ch == 34) quoted = false;
                                } else if (ch == 34) quoted = true;
                                else if (ch == 123) depth++;
                                else if (ch == 125 && --depth == 0) break;
                            }
                            types.push_back(k < n ? JSON_OBJECT_LITERAL : UNCLOSED_JSON_OBJECT_LITERAL);
                            starts.push_back(j);
                            i = k < n ? k + 1 : n;
                            ends.push_back(i);
                            lineStart = false;
                            continue;
                        }
                    }
                }
            }
            if (type == 0) {
                while (end < n && isLexWord(charCodeAt(src, end))) end++;
                int keyword = keywordAt(src, i, c);
                if (keyword != 0) {
                    if (keyword == USECASE || end - i == KEYWORD_LENGTH[keyword]) {
                        type = keyword;
                        end = i + KEYWORD_LENGTH[keyword];
                    } else type = IDENTIFIER;
                } else if ((c == 111 || c == 120) && charCodeAt(src, i + 1) == 45 && charCodeAt(src, i + 2) == 45) {
                    type = c == 111 ? BACKWARD_CIRCLE : BACKWARD_CROSS;
                    end = i + 3;
                } else if (isDigit(c)) {
                    int digits = i + 1;
                    while (isDigit(charCodeAt(src, digits))) digits++;
                    if (charCodeAt(src, digits) == 46 && isDigit(charCodeAt(src, digits + 1))) {
                        type = NUMBER;
                        end = digits + 2;
                        while (isDigit(charCodeAt(src, end))) end++;
                        while (isLetter(charCodeAt(src, end))) end++;
                    } else type = IDENTIFIER;
                } else {
                    type = IDENTIFIER;
                    while (charCodeAt(src, end) == 45 && isLexWord(charCodeAt(src, end + 1))) {
                        type = CSS_IDENTIFIER;
                        end += 2;
                        while (isLexWord(charCodeAt(src, end))) end++;
                    }
                }
            }
        } else if (c == 10) {
            type = NEWLINE;
            fresh = true;
        } else if (c == 13) {
            type = NEWLINE;
            fresh = true;
            if (charCodeAt(src, end) == 10) end++;
        } else if (c == 34 || c == 39) {
            if (c == 34 && charCodeAt(src, i + 1) == 96) {
                size_t close = src.find("`\"", static_cast<size_t>(i + 2));
                type = close == std::string::npos ? UNCLOSED_MARKDOWN_STRING : MARKDOWN_STRING;
                end = close == std::string::npos ? n : static_cast<int>(close) + 2;
            } else {
                type = PLAIN_STRING;
                end = stringEnd(src, i, c);
                if (end == -1) {
                    int resume = i + 1;
                    while (resume < n && !startsToken(src, resume)) resume++;
                    auto [ln, col] = lineColumn(src, i);
                    throw PeleError("Error lexing usecase diagram", "syntax", "usecase", ln, col);
                }
            }
        } else if (c == 37) {
            if (lineStart && charCodeAt(src, end) == 37) {
                type = COMMENT;
                while (end < n && charCodeAt(src, end) != 10 && charCodeAt(src, end) != 13) end++;
            } else type = PERCENT;
        } else if (c == 45) {
            if (charCodeAt(src, end) != 45) type = DASH;
            else {
                while (charCodeAt(src, end) == 45) end++;
                int next = charCodeAt(src, end);
                bool two = (end - i == 2);
                if (two && next == 124 && charCodeAt(src, end + 1) == 62) {
                    type = GENERALIZATION;
                    end += 2;
                } else if (next == 62) {
                    type = FORWARD_SOLID;
                    end++;
                } else if (two && (next == 111 || next == 120)) {
                    type = next == 111 ? FORWARD_CIRCLE : FORWARD_CROSS;
                    end++;
                } else type = MARKERLESS_SOLID;
            }
        } else if (c == 46) {
            int next = charCodeAt(src, end);
            if (next == 46 && charCodeAt(src, end + 1) == 62) {
                type = DEPENDENCY_ARROW;
                end += 2;
            } else if (isDigit(next)) {
                type = NUMBER;
                while (isDigit(charCodeAt(src, end))) end++;
                while (isLetter(charCodeAt(src, end))) end++;
            } else type = DOT;
        } else if (c == 60) {
            if (charCodeAt(src, end) == 60) {
                types.push_back(STEREOTYPE_START);
                starts.push_back(i);
                ends.push_back(i + 2);
                lineStart = false;
                i += 2;
                if (i >= n) break;
                if (charCodeAt(src, i) == 62 && charCodeAt(src, i + 1) == 62) {
                    type = STEREOTYPE_END;
                    end = i + 2;
                } else {
                    int close = i;
                    bool found = false;
                    for (; close < n; close++) {
                        int ch = charCodeAt(src, close);
                        if (ch == 10 || ch == 13) break;
                        if (ch == 62 && charCodeAt(src, close + 1) == 62) {
                            found = true;
                            break;
                        }
                    }
                    if (found && !trimStr(src.substr(static_cast<size_t>(i), static_cast<size_t>(close - i))).empty()) {
                        types.push_back(STEREOTYPE_TEXT);
                        starts.push_back(i);
                        ends.push_back(close);
                        type = STEREOTYPE_END;
                        i = close;
                        end = close + 2;
                    } else {
                        while (close < n && charCodeAt(src, close) != 10 && charCodeAt(src, close) != 13) close++;
                        if (close == i) {
                            auto [ln, col] = lineColumn(src, i);
                            throw PeleError("Error lexing usecase diagram", "syntax", "usecase", ln, col);
                        }
                        type = UNCLOSED_STEREOTYPE_TEXT;
                        end = close;
                    }
                }
            } else if (charCodeAt(src, end) == 45 && charCodeAt(src, end + 1) == 45) {
                type = BACKWARD_SOLID;
                end += 2;
                while (charCodeAt(src, end) == 45) end++;
            } else type = LABEL_PUNCTUATION;
        } else if (c == 58) {
            if (charCodeAt(src, end) == 58 && charCodeAt(src, end + 1) == 58) {
                type = CLASS_SEPARATOR;
                end += 2;
            } else type = COLON;
        } else if (c == 64) {
            if (charCodeAt(src, end) == 123) {
                type = METADATA_START;
                end++;
            } else type = AT;
        } else if (c == 123) type = LBRACE;
        else if (c == 125) type = RBRACE;
        else if (c == 91) type = LBRACKET;
        else if (c == 93) type = RBRACKET;
        else if (c == 40) type = LPAREN;
        else if (c == 41) type = RPAREN;
        else if (c == 44) type = COMMA;
        else if (c == 35) {
            while (isHex(charCodeAt(src, end))) end++;
            type = end > i + 1 ? HASH_COLOR : CSS_PUNCTUATION;
        } else if (c == 92) {
            if (charCodeAt(src, end) == 44) {
                type = CSS_ESCAPED_COMMA;
                end++;
            } else type = LABEL_PUNCTUATION;
        } else if (c < 127 && c > 32) {
            type = (CLASS_TBL[c] & 2) != 0 ? CSS_PUNCTUATION : LABEL_PUNCTUATION;
        } else {
            type = LABEL_SYMBOL;
            while (end < n) {
                int ch = charCodeAt(src, end);
                if ((ch >= 32 && ch < 127) || ch == 9 || ch == 10 || ch == 13) break;
                end++;
            }
        }

        types.push_back(type);
        starts.push_back(i);
        ends.push_back(end);
        i = end;
        lineStart = fresh;
    }

    for (int k = 0; k < 3; k++) {
        types.push_back(T_EOF);
        starts.push_back(n);
        ends.push_back(n);
    }
    return {std::move(types), std::move(starts), std::move(ends)};
}

// ============================================================================
// src/diagrams/usecase/json.ts
// ============================================================================

inline std::string formatJsonNumber(const std::string& raw) {
    try {
        double d = std::stod(raw);
        if (d == 0.0) return "0";
        if (std::floor(d) == d && std::abs(d) < 9007199254740991.0) {
            return std::to_string(static_cast<long long>(d));
        }
        std::ostringstream oss;
        oss << std::setprecision(15) << d;
        return oss.str();
    } catch (...) {
        return raw;
    }
}

inline OrderedJson parseOrderedJson(const std::string& text) {
    OrderedJson res;
    std::vector<std::string> created;
    int n = static_cast<int>(text.size());
    int at = 0;

    auto skip = [&]() {
        while (at < n) {
            int c = charCodeAt(text, at);
            if (c != 32 && c != 9 && c != 10 && c != 13) return;
            at++;
        }
    };

    auto readString = [&]() -> std::string {
        at++;
        std::string value;
        while (at < n) {
            int start = at;
            unsigned char ch = static_cast<unsigned char>(text[at++]);
            if (ch == '"') return value;
            if (ch < 0x20) throw PeleError("Invalid JSON token", "syntax", "usecase");
            if (ch != '\\') {
                value.push_back(static_cast<char>(ch));
                continue;
            }
            if (at >= n) throw PeleError("Invalid JSON token", "syntax", "usecase");
            char escape = text[at++];
            if (escape == '"' || escape == '\\' || escape == '/') value.push_back(escape);
            else if (escape == 'b') value.push_back('\b');
            else if (escape == 'f') value.push_back('\f');
            else if (escape == 'n') value.push_back('\n');
            else if (escape == 'r') value.push_back('\r');
            else if (escape == 't') value.push_back('\t');
            else if (escape == 'u') {
                if (at + 4 > n) throw PeleError("Invalid JSON token", "syntax", "usecase");
                for (int k = 0; k < 4; k++) if (!isHex(charCodeAt(text, at + k))) throw PeleError("Invalid JSON token", "syntax", "usecase");
                uint16_t u1 = static_cast<uint16_t>(std::stoul(text.substr(static_cast<size_t>(at), 4), nullptr, 16));
                at += 4;
                if (u1 >= 0xD800 && u1 <= 0xDBFF && at + 6 <= n && text[at] == '\\' && text[at + 1] == 'u') {
                    bool ok2 = true;
                    for (int k = 0; k < 4; k++) if (!isHex(charCodeAt(text, at + 2 + k))) ok2 = false;
                    if (ok2) {
                        uint16_t u2 = static_cast<uint16_t>(std::stoul(text.substr(static_cast<size_t>(at + 2), 4), nullptr, 16));
                        if (u2 >= 0xDC00 && u2 <= 0xDFFF) {
                            at += 6;
                            uint16_t pair[2] = {u1, u2};
                            value += fromUtf16(pair, 2);
                            continue;
                        }
                    }
                }
                appendCodepoint(value, u1);
            } else {
                (void)start;
                throw PeleError("Invalid JSON token", "syntax", "usecase");
            }
        }
        throw PeleError("Invalid JSON token", "syntax", "usecase");
    };

    auto literal = [&](const std::string& word) {
        for (size_t k = 0; k < word.size(); k++) {
            if (at + static_cast<int>(k) >= n || text[at + k] != word[k]) throw PeleError("Invalid JSON token", "syntax", "usecase");
        }
        at += static_cast<int>(word.size());
    };

    auto readNumber = [&]() -> std::string {
        int start = at;
        if (charCodeAt(text, at) == 45) at++;
        if (charCodeAt(text, at) == 48) at++;
        else if (isDigit(charCodeAt(text, at))) {
            while (isDigit(charCodeAt(text, at))) at++;
        } else throw PeleError("Invalid JSON token", "syntax", "usecase");
        if (charCodeAt(text, at) == 46) {
            at++;
            if (!isDigit(charCodeAt(text, at))) throw PeleError("Invalid JSON token", "syntax", "usecase");
            while (isDigit(charCodeAt(text, at))) at++;
        }
        if ((charCodeAt(text, at) | 32) == 101) {
            at++;
            if (charCodeAt(text, at) == 43 || charCodeAt(text, at) == 45) at++;
            if (!isDigit(charCodeAt(text, at))) throw PeleError("Invalid JSON token", "syntax", "usecase");
            while (isDigit(charCodeAt(text, at))) at++;
        }
        return formatJsonNumber(text.substr(static_cast<size_t>(start), static_cast<size_t>(at - start)));
    };

    auto parseVal = [&](auto& self, const std::string& pointer) -> JsonVal {
        skip();
        int c = charCodeAt(text, at);
        if (c == 123) {
            at++;
            JsonVal v;
            v.type = JsonVal::Object;
            std::vector<std::string> keys;
            std::unordered_map<std::string, std::pair<size_t, size_t>> ranges;
            created.push_back(pointer);
            skip();
            if (charCodeAt(text, at) == 125) {
                at++;
                res.propertyOrder[pointer] = std::move(keys);
                return v;
            }
            for (;;) {
                skip();
                if (charCodeAt(text, at) != 34) throw PeleError("Invalid JSON token", "syntax", "usecase");
                std::string key = readString();
                auto rIt = ranges.find(key);
                if (rIt == ranges.end()) keys.push_back(key);
                else {
                    for (size_t k = rIt->second.first; k < rIt->second.second; k++) {
                        res.propertyOrder.erase(created[k]);
                    }
                }
                size_t from = created.size();
                skip();
                if (charCodeAt(text, at) != 58) throw PeleError("Invalid JSON token", "syntax", "usecase");
                at++;
                std::string childPtr = Pele::cat({pointer, "/", replaceAllStr(replaceAllStr(key, "~", "~0"), "/", "~1")});
                v.obj[key] = self(self, childPtr);
                ranges[key] = {from, created.size()};
                skip();
                int next = charCodeAt(text, at);
                if (next == 125) {
                    at++;
                    break;
                }
                if (next != 44) throw PeleError("Invalid JSON token", "syntax", "usecase");
                at++;
            }
            res.propertyOrder[pointer] = std::move(keys);
            return v;
        }
        if (c == 91) {
            at++;
            JsonVal v;
            v.type = JsonVal::Array;
            skip();
            if (charCodeAt(text, at) == 93) {
                at++;
                return v;
            }
            int idx = 0;
            for (;;) {
                v.arr.push_back(self(self, pointer + "/" + std::to_string(idx++)));
                skip();
                int next = charCodeAt(text, at);
                if (next == 93) {
                    at++;
                    break;
                }
                if (next != 44) throw PeleError("Invalid JSON token", "syntax", "usecase");
                at++;
            }
            return v;
        }
        if (c == 34) {
            JsonVal v;
            v.type = JsonVal::String;
            v.scalar = readString();
            return v;
        }
        if (c == 116) {
            literal("true");
            JsonVal v;
            v.type = JsonVal::Bool;
            v.scalar = "true";
            return v;
        }
        if (c == 102) {
            literal("false");
            JsonVal v;
            v.type = JsonVal::Bool;
            v.scalar = "false";
            return v;
        }
        if (c == 110) {
            literal("null");
            JsonVal v;
            v.type = JsonVal::Null;
            v.scalar = "null";
            return v;
        }
        if (c == 45 || isDigit(c)) {
            JsonVal v;
            v.type = JsonVal::Number;
            v.scalar = readNumber();
            return v;
        }
        throw PeleError("Invalid JSON token", "syntax", "usecase");
    };

    res.value = parseVal(parseVal, "");
    skip();
    if (at != n || res.value.type != JsonVal::Object) {
        throw PeleError("Invalid JSON", "syntax", "usecase");
    }
    return res;
}
// ============================================================================
// src/diagrams/usecase/db.ts
// ============================================================================

struct DraftLabel {
    std::string text;
    std::string type = "text";
    Span span{0, 0};
};

struct DraftProperty {
    std::string key;
    bool isBool = false;
    bool boolVal = false;
    std::string strVal;
    Span span{0, 0};
    Span keySpan{0, 0};
    Span valueSpan{0, 0};
};

struct DraftBoundary {
    std::string id;
    DraftLabel label;
    Span span{0, 0};
    bool generated = false;
    std::vector<std::string> classes;
};

struct DraftElement {
    std::string id;
    std::string kind; // "actor" | "usecase"
    DraftLabel label;
    Span span{0, 0};
    bool generated = false;
    bool hasParent = false;
    DraftBoundary parent;
    std::string shape;
    std::vector<DraftProperty> metadata;
    bool hasMetadata = false;
    std::string stereotype;
    std::vector<std::string> classes;
};

struct DraftJson {
    std::string id;
    JsonVal value;
    std::unordered_map<std::string, std::vector<std::string>> propertyOrder;
    Span span{0, 0};
    std::vector<std::string> classes;
};

struct DraftEndpoint {
    std::string id;
    DraftLabel label;
    Span span{0, 0};
    bool generated = false;
    bool declares = false;
    bool classed = false;
};

struct DraftRelationship {
    DraftEndpoint source;
    DraftEndpoint target;
    Span span{0, 0};
    std::string id;
    bool hasId = false;
    Span idSpan{0, 0};
    bool hasIdSpan = false;
    std::string type = "association";
    int arrowType = SOLID_ARROW;
    DraftLabel label;
    bool hasLabel = false;
    int minlen = 1;
};

struct DraftNote {
    std::string target;
    Span span{0, 0};
    DraftLabel label;
};

struct DraftMetadata {
    std::string target;
    Span span{0, 0};
    std::vector<DraftProperty> properties;
};

struct DraftClassAssignTarget {
    std::string id;
    Span span{0, 0};
};

struct DraftClassAssign {
    std::vector<DraftClassAssignTarget> targets;
    std::vector<std::string> classes;
};

struct DraftStyleAssign {
    std::string target;
    Span span{0, 0};
    std::vector<std::string> styles;
};

struct DraftClassDef {
    std::vector<std::string> ids;
    std::vector<std::string> styles;
};

struct Drafts {
    std::vector<DraftElement> elements;
    std::vector<DraftBoundary> boundaries;
    std::vector<DraftJson> jsons;
    std::vector<DraftRelationship> relationships;
    std::vector<DraftNote> notes;
    std::vector<DraftMetadata> metadata;
    std::vector<DraftClassAssign> classes;
    std::vector<DraftStyleAssign> styles;
    std::vector<DraftClassDef> classDefs;
    std::string direction;
    std::string accTitle;
    std::string accDescription;
};

inline void pushUnique(std::vector<std::string>& target, const std::vector<std::string>& values) noexcept {
    if (values.empty()) return;
    for (const auto& v : values) {
        if (std::find(target.begin(), target.end(), v) == target.end()) {
            target.push_back(v);
        }
    }
}

struct Origin {
    std::string kind;
    Span span{0, 0};
    bool generated = false;
    std::string label;
};

struct ElementState {
    std::string id;
    std::string kind; // "actor" | "usecase"
    DraftLabel label;
    Span span{0, 0};
    bool generated = false;
    bool hasParent = false;
    DraftBoundary parent;
    std::string shape;
    std::string stereotype;
    Span stereotypeSpan{0, 0};
    bool hasStereotypeSpan = false;
    std::vector<std::string> classes;
    std::string actorType;
    std::string icon;
    bool hasBusiness = false;
    bool business = false;
};

struct BoundaryState {
    std::string id;
    DraftLabel label;
    Span span{0, 0};
    bool generated = false;
    std::vector<std::string> classes;
    std::string type;
    std::vector<std::string> members;
};

inline UsecaseModel resolveUsecase(const std::string& src, const Drafts& drafts, const std::string& title) {
    UsecaseModel model;
    std::unordered_map<std::string, Origin> symbols;
    std::vector<std::string> elementOrder;
    std::unordered_map<std::string, ElementState> elements;
    std::vector<std::string> boundaryStateOrder;
    std::unordered_map<std::string, BoundaryState> boundaries;
    Pele::FlatMap<std::string, size_t> edgeIdxMap;
    Pele::FlatMap<std::string, int> first;

    auto fail = [&](const std::string& msg, Span sp) {
        auto [ln, col] = lineColumn(src, sp.first);
        throw PeleError(msg, "semantic", "usecase", ln, col);
    };

    auto mention = [&](const std::string& id, int offset) {
        auto it = first.find(id);
        if (it == first.end() || offset < it->second) first[id] = offset;
    };

    auto registerSym = [&](const std::string& id, const std::string& labelText, Span sp, bool gen, const std::string& kind) {
        symbols[id] = {kind, sp, gen, labelText};
    };

    auto claim = [&](const std::string& id, Span sp, bool gen, const std::string& kind) {
        auto it = symbols.find(id);
        if (it == symbols.end()) return;
        if (it->second.kind != kind) {
            fail("ID '" + id + "' is declared as both " + it->second.kind + " and " + kind, sp);
        }
        if (it->second.generated || gen) {
            fail(Pele::cat({"Generated ID '", id, "' collides with another declaration"}),  sp);
        }
    };

    auto uniqueSym = [&](const std::string& id, const std::string& kind, Span sp) {
        auto it = symbols.find(id);
        if (it != symbols.end()) {
            fail(Pele::cat({"ID '", id, "' is declared more than once"}),  sp);
        }
        symbols[id] = {kind, sp, false, ""};
    };

    auto actorProperty = [&](ElementState& state, const DraftProperty& prop, bool replace) {
        if (prop.key == "type" && !prop.isBool && (prop.strVal == "normal" || prop.strVal == "hollow" || prop.strVal == "awesome")) {
            if (!replace && !state.actorType.empty() && state.actorType != prop.strVal) {
                fail(Pele::cat({"Actor '", state.id, "' has conflicting type metadata"}),  prop.keySpan);
            }
            state.actorType = prop.strVal;
        } else if (prop.key == "icon" && !prop.isBool) {
            if (!replace && !state.icon.empty() && state.icon != prop.strVal) {
                fail(Pele::cat({"Actor '", state.id, "' has conflicting icon metadata"}),  prop.keySpan);
            }
            state.icon = prop.strVal;
        } else if (prop.key == "business" && prop.isBool) {
            if (!replace && state.hasBusiness && state.business != prop.boolVal) {
                fail(Pele::cat({"Actor '", state.id, "' has conflicting business metadata"}),  prop.keySpan);
            }
            state.hasBusiness = true;
            state.business = prop.boolVal;
        } else {
            fail(Pele::cat({"Metadata property '", prop.key, "' is invalid for actor '", state.id, "'"}),  prop.keySpan);
        }
    };

    auto elementMetadata = [&](ElementState& state, const std::vector<DraftProperty>& props, bool replace) {
        for (const auto& prop : props) {
            if (state.kind == "actor") actorProperty(state, prop, replace);
            else if (prop.key == "business" && prop.isBool) {
                if (!replace && state.hasBusiness && state.business != prop.boolVal) {
                    fail(Pele::cat({"Use case '", state.id, "' has conflicting business metadata"}),  prop.keySpan);
                }
                state.hasBusiness = true;
                state.business = prop.boolVal;
            } else {
                fail(Pele::cat({"Metadata property '", prop.key, "' is invalid for usecase '", state.id, "'"}),  prop.keySpan);
            }
        }
    };

    auto publish = [&](const ElementState& state) {
        if (state.kind == "actor") {
            if (model.useCases.erase(state.id)) {
                model.useCaseOrder.erase(std::remove(model.useCaseOrder.begin(), model.useCaseOrder.end(), state.id), model.useCaseOrder.end());
            }
            std::vector<std::string> existingStyles;
            auto it = model.actors.find(state.id);
            if (it != model.actors.end()) existingStyles = it->second.styles;
            else model.actorOrder.push_back(state.id);

            Actor actor;
            actor.id = state.id;
            actor.label = state.label.text;
            actor.labelType = state.label.type;
            actor.business = state.hasBusiness ? state.business : false;
            actor.classes = state.classes;
            actor.type = !state.icon.empty() ? "icon" : (!state.actorType.empty() ? state.actorType : "normal");
            actor.styles = std::move(existingStyles);
            actor.icon = state.icon;
            actor.stereotype = state.stereotype;
            if (state.hasParent && !state.parent.id.empty()) actor.parentId = state.parent.id;
            model.actors[state.id] = std::move(actor);
        } else {
            if (model.actors.erase(state.id)) {
                model.actorOrder.erase(std::remove(model.actorOrder.begin(), model.actorOrder.end(), state.id), model.actorOrder.end());
            }
            std::vector<std::string> existingStyles;
            auto it = model.useCases.find(state.id);
            if (it != model.useCases.end()) existingStyles = it->second.styles;
            else model.useCaseOrder.push_back(state.id);

            UseCase uc;
            uc.id = state.id;
            uc.label = state.label.text;
            uc.labelType = state.label.type;
            uc.business = state.hasBusiness ? state.business : false;
            uc.classes = state.classes;
            uc.shape = !state.shape.empty() ? state.shape : "ellipse";
            uc.styles = std::move(existingStyles);
            uc.stereotype = state.stereotype;
            if (state.hasParent && !state.parent.id.empty()) uc.parentId = state.parent.id;
            model.useCases[state.id] = std::move(uc);
        }
    };

    auto declareElement = [&](const DraftElement& draft) {
        claim(draft.id, draft.span, draft.generated, draft.kind);
        bool hasParent = draft.hasParent && !draft.parent.id.empty();
        auto it = elements.find(draft.id);
        if (it == elements.end()) {
            ElementState state;
            state.kind = draft.kind;
            state.id = draft.id;
            state.label = draft.label;
            state.span = draft.span;
            state.generated = draft.generated;
            state.classes = draft.classes;
            state.hasParent = hasParent;
            if (hasParent) state.parent = draft.parent;
            state.shape = draft.shape;
            state.stereotype = draft.stereotype;
            if (!draft.stereotype.empty()) {
                state.stereotypeSpan = draft.span;
                state.hasStereotypeSpan = true;
            }
            if (draft.hasMetadata) elementMetadata(state, draft.metadata, false);
            elementOrder.push_back(draft.id);
            elements[draft.id] = std::move(state);
            registerSym(draft.id, draft.label.text, draft.span, draft.generated, draft.kind);
            return;
        }
        ElementState& existing = it->second;
        if (existing.label.text != draft.label.text || existing.label.type != draft.label.type) {
            fail(Pele::cat({"ID '", draft.id, "' has conflicting labels"}),  draft.span);
        }
        if (!draft.shape.empty() && !existing.shape.empty() && draft.shape != existing.shape) {
            fail(Pele::cat({"Use case '", draft.id, "' has conflicting shapes"}),  draft.span);
        }
        if (hasParent && existing.hasParent && !existing.parent.id.empty() && draft.parent.id != existing.parent.id) {
            fail(Pele::cat({"Element '", draft.id, "' belongs to more than one system boundary"}),  draft.parent.span);
        }
        if (!draft.stereotype.empty() && !existing.stereotype.empty() && draft.stereotype != existing.stereotype) {
            fail(Pele::cat({"Element '", draft.id, "' has conflicting stereotypes"}),  draft.span);
        }
        if (existing.shape.empty()) existing.shape = draft.shape;
        if (!existing.hasParent && draft.hasParent) {
            existing.hasParent = true;
            existing.parent = draft.parent;
        }
        if (existing.stereotype.empty()) existing.stereotype = draft.stereotype;
        if (!existing.hasStereotypeSpan && !draft.stereotype.empty()) {
            existing.stereotypeSpan = draft.span;
            existing.hasStereotypeSpan = true;
        }
        pushUnique(existing.classes, draft.classes);
        if (draft.hasMetadata) elementMetadata(existing, draft.metadata, false);
    };

    auto declareBoundary = [&](const DraftBoundary& draft) {
        claim(draft.id, draft.span, draft.generated, "boundary");
        auto it = boundaries.find(draft.id);
        if (it != boundaries.end()) {
            if (it->second.label.text != draft.label.text || it->second.label.type != draft.label.type) {
                fail(Pele::cat({"Boundary '", draft.id, "' has conflicting titles"}),  draft.span);
            }
            pushUnique(it->second.classes, draft.classes);
            return;
        }
        BoundaryState bs;
        bs.id = draft.id;
        bs.label = draft.label;
        bs.span = draft.span;
        bs.generated = draft.generated;
        bs.classes = draft.classes;
        boundaryStateOrder.push_back(draft.id);
        boundaries[draft.id] = std::move(bs);
        registerSym(draft.id, draft.label.text, draft.span, draft.generated, "boundary");
    };

    auto endpoint = [&](const DraftEndpoint& end) -> std::string {
        if (end.classed && !end.declares) {
            fail(Pele::cat({"Relationship endpoint '", end.id, "' uses ::: without declaring the node"}),  end.span);
        }
        auto it = symbols.find(end.id);
        if (it != symbols.end()) return it->second.kind;
        ElementState st;
        st.id = end.id;
        st.label = end.label;
        st.span = end.span;
        st.generated = end.generated;
        st.kind = "usecase";
        st.shape = "ellipse";
        elementOrder.push_back(end.id);
        elements[end.id] = std::move(st);
        registerSym(end.id, end.label.text, end.span, end.generated, "usecase");
        mention(end.id, end.span.first);
        return "usecase";
    };

    struct Decl {
        int offset;
        std::function<void()> run;
    };
    std::vector<Decl> declarations;
    for (const auto& r : drafts.relationships) {
        mention(r.source.id, r.source.span.first);
        mention(r.target.id, r.target.span.first);
    }
    for (const auto& d : drafts.elements) {
        mention(d.id, d.span.first);
        declarations.push_back({d.span.first, [&d, &declareElement]() { declareElement(d); }});
    }
    for (const auto& d : drafts.boundaries) {
        mention(d.id, d.span.first);
        declarations.push_back({d.span.first, [&d, &declareBoundary]() { declareBoundary(d); }});
    }
    for (const auto& d : drafts.jsons) {
        mention(d.id, d.span.first);
        declarations.push_back({d.span.first, [&d, &uniqueSym]() { uniqueSym(d.id, "json", d.span); }});
    }
    for (const auto& r : drafts.relationships) {
        if (r.hasId && r.hasIdSpan) {
            declarations.push_back({r.idSpan.first, [&r, &uniqueSym]() { uniqueSym(r.id, "edge", r.idSpan); }});
        }
    }
    std::stable_sort(declarations.begin(), declarations.end(), [](const Decl& a, const Decl& b) {
        return a.offset < b.offset;
    });
    for (const auto& decl : declarations) decl.run();

    auto sortIds = [&](std::vector<std::string> ids) {
        std::stable_sort(ids.begin(), ids.end(), [&](const std::string& a, const std::string& b) {
            int fa = first.count(a) ? first[a] : 0;
            int fb = first.count(b) ? first[b] : 0;
            return fa < fb;
        });
        return ids;
    };

    for (const auto& id : sortIds(elementOrder)) publish(elements[id]);
    for (const auto& id : sortIds(boundaryStateOrder)) {
        const auto& state = boundaries[id];
        SystemBoundary sb;
        sb.id = state.id;
        sb.label = state.label.text;
        sb.labelType = state.label.type;
        sb.type = "rect";
        model.boundaryOrder.push_back(state.id);
        model.systemBoundaries[state.id] = std::move(sb);
    }
    std::vector<const DraftJson*> sortedJsons;
    for (const auto& j : drafts.jsons) sortedJsons.push_back(&j);
    std::stable_sort(sortedJsons.begin(), sortedJsons.end(), [&](const DraftJson* a, const DraftJson* b) {
        int fa = first.count(a->id) ? first[a->id] : 0;
        int fb = first.count(b->id) ? first[b->id] : 0;
        return fa < fb;
    });
    for (const auto* j : sortedJsons) {
        UsecaseJsonNode jn;
        jn.id = j->id;
        jn.value = j->value;
        jn.propertyOrder = j->propertyOrder;
        jn.classes = j->classes;
        model.jsonOrder.push_back(j->id);
        model.jsonNodes[j->id] = std::move(jn);
    }

    int anonymous = 0;
    for (const auto& draft : drafts.relationships) {
        std::string sourceKind = endpoint(draft.source);
        std::string targetKind = endpoint(draft.target);
        auto related = [](const std::string& k) { return k == "actor" || k == "usecase" || k == "json"; };
        if (!related(sourceKind)) fail(Pele::cat({"Relationship source '", draft.source.id, "' cannot be ", sourceKind}),  draft.source.span);
        if (!related(targetKind)) fail(Pele::cat({"Relationship target '", draft.target.id, "' cannot be ", targetKind}),  draft.target.span);
        if ((draft.type == "include" || draft.type == "extend") && (sourceKind != "usecase" || targetKind != "usecase")) {
            fail(Pele::cat({draft.type, " relationship requires use-case endpoints"}),  draft.span);
        }
        if (draft.type == "generalization" && (sourceKind == "json" || sourceKind != targetKind)) {
            fail("Generalization requires actor-to-actor or use-case-to-use-case endpoints", draft.span);
        }
        if (draft.type == "association" && (sourceKind == "json" || targetKind == "json") && draft.arrowType > 2) {
            fail("JSON relationship permits only point, reversed-point, or markerless solid association", draft.span);
        }
        std::string id = draft.hasId ? draft.id : ("edge-" + std::to_string(anonymous++));
        Relationship rel;
        rel.id = id;
        rel.explicitId = draft.hasId;
        rel.source = draft.source.id;
        rel.target = draft.target.id;
        rel.type = draft.type;
        rel.arrowType = draft.arrowType;
        rel.minlen = draft.minlen;
        rel.animate = false;
        if (draft.hasLabel) {
            rel.label = draft.label.text;
            rel.hasLabel = true;
            rel.labelType = draft.label.type;
            rel.hasLabelType = true;
        }
        edgeIdxMap[id] = model.relationships.size();
        model.relationships.push_back(std::move(rel));
    }

    auto declaredActors = model.actors;
    auto declaredUseCases = model.useCases;
    model.actors.clear();
    model.actorOrder.clear();
    model.useCases.clear();
    model.useCaseOrder.clear();
    for (const auto& id : sortIds(elementOrder)) {
        auto aIt = declaredActors.find(id);
        auto uIt = declaredUseCases.find(id);
        if (aIt != declaredActors.end()) {
            model.actorOrder.push_back(id);
            model.actors[id] = aIt->second;
        } else if (uIt != declaredUseCases.end()) {
            model.useCaseOrder.push_back(id);
            model.useCases[id] = uIt->second;
        } else {
            publish(elements[id]);
        }
    }

    for (const auto& m : drafts.metadata) {
        auto oIt = symbols.find(m.target);
        if (oIt == symbols.end()) {
            fail(Pele::cat({"Metadata target '", m.target, "' is unresolved"}),  m.span);
        }
        std::string kind = oIt->second.kind;
        if (kind == "actor" || kind == "usecase") {
            elementMetadata(elements[m.target], m.properties, true);
        } else {
            for (const auto& prop : m.properties) {
                if (kind == "boundary" && boundaries.count(m.target) && prop.key == "type" && !prop.isBool && (prop.strVal == "rect" || prop.strVal == "package")) {
                    boundaries[m.target].type = prop.strVal;
                } else if (kind == "edge" && edgeIdxMap.count(m.target) && prop.key == "animate" && prop.isBool) {
                    model.relationships[edgeIdxMap[m.target]].animate = prop.boolVal;
                } else if (kind == "edge" && edgeIdxMap.count(m.target) && prop.key == "animation" && !prop.isBool && (prop.strVal == "fast" || prop.strVal == "slow")) {
                    model.relationships[edgeIdxMap[m.target]].animation = prop.strVal;
                    model.relationships[edgeIdxMap[m.target]].animate = true;
                } else {
                    fail(Pele::cat({"Metadata property '", prop.key, "' is invalid"}),  prop.keySpan);
                }
            }
        }
    }
    for (auto& rel : model.relationships) {
        if (!rel.animation.empty()) rel.animate = true;
    }

    for (const auto& id : elementOrder) {
        const auto& state = elements[id];
        if (state.kind == "actor") {
            if (!state.icon.empty() && !state.actorType.empty() && state.actorType != "normal") {
                fail(Pele::cat({"Actor '", state.id, "' cannot combine icon with type '", state.actorType, "'"}),  state.span);
            }
            if (state.hasBusiness && state.business && (!state.icon.empty() || state.actorType == "awesome")) {
                fail(Pele::cat({"Business actor '", state.id, "' must use normal or hollow geometry"}),  state.span);
            }
        } else if (state.shape == "rect" && state.hasBusiness && state.business) {
            fail(Pele::cat({"Rectangular use case '", state.id, "' cannot be a business use case"}),  state.span);
        }
        publish(state);
    }

    std::vector<const DraftElement*> sortedElements;
    for (const auto& d : drafts.elements) sortedElements.push_back(&d);
    std::stable_sort(sortedElements.begin(), sortedElements.end(), [](const DraftElement* a, const DraftElement* b) {
        return a->span.first < b->span.first;
    });
    for (const auto* d : sortedElements) {
        if (!d->hasParent || d->parent.id.empty()) continue;
        auto& members = boundaries[d->parent.id].members;
        if (std::find(members.begin(), members.end(), d->id) == members.end()) {
            members.push_back(d->id);
        }
    }
    for (const auto& id : boundaryStateOrder) {
        const auto& state = boundaries[id];
        auto& boundary = model.systemBoundaries[state.id];
        boundary.type = !state.type.empty() ? state.type : "rect";
        boundary.members = state.members;
        boundary.classes = state.classes;
    }

    int notesCount = 0;
    for (const auto& draft : drafts.notes) {
        auto oIt = symbols.find(draft.target);
        if (oIt == symbols.end()) fail(Pele::cat({"Note target '", draft.target, "' is unresolved"}),  draft.span);
        if (oIt->second.kind != "actor" && oIt->second.kind != "usecase") {
            fail(Pele::cat({"Note target '", draft.target, "' must be an actor or use case"}),  draft.span);
        }
        std::string id = "note-" + std::to_string(notesCount++);
        model.noteOrder.push_back(id);
        model.notes[id] = {id, draft.target, draft.label.text, draft.label.type};
    }

    for (const auto& def : drafts.classDefs) {
        for (const auto& id : def.ids) {
            if (!model.classDefs.count(id)) model.classDefOrder.push_back(id);
            model.classDefs[id] = {id, def.styles};
        }
    }

    auto getStylable = [&](const std::string& id, Span sp) -> std::pair<std::vector<std::string>*, std::vector<std::string>*> {
        auto oIt = symbols.find(id);
        if (oIt == symbols.end()) fail(Pele::cat({"Class/style target '", id, "' is unresolved or anonymous"}),  sp);
        const std::string& kind = oIt->second.kind;
        if (kind == "actor" && model.actors.count(id)) return {&model.actors[id].classes, &model.actors[id].styles};
        if (kind == "usecase" && model.useCases.count(id)) return {&model.useCases[id].classes, &model.useCases[id].styles};
        if (kind == "boundary" && model.systemBoundaries.count(id)) return {&model.systemBoundaries[id].classes, &model.systemBoundaries[id].styles};
        if (kind == "json" && model.jsonNodes.count(id)) return {&model.jsonNodes[id].classes, &model.jsonNodes[id].styles};
        if (kind == "edge" && edgeIdxMap.count(id)) {
            auto& r = model.relationships[edgeIdxMap[id]];
            return {&r.classes, &r.styles};
        }
        fail(Pele::cat({"Class/style target '", id, "' is unresolved or anonymous"}),  sp);
        return {nullptr, nullptr};
    };

    for (const auto& assign : drafts.classes) {
        for (const auto& target : assign.targets) {
            auto [clsPtr, styPtr] = getStylable(target.id, target.span);
            if (clsPtr) pushUnique(*clsPtr, assign.classes);
        }
    }
    for (const auto& assign : drafts.styles) {
        auto [clsPtr, styPtr] = getStylable(assign.target, assign.span);
        if (styPtr) {
            for (const auto& s : assign.styles) styPtr->push_back(s);
        }
    }

    if (!drafts.direction.empty()) model.direction = drafts.direction;
    model.title = title;
    model.accTitle = drafts.accTitle;
    model.accDescription = drafts.accDescription;
    // drafts.accDescription.replace(/\n\s+/g, '\n')
    std::string accD;
    for (size_t i = 0; i < drafts.accDescription.size(); ) {
        if (drafts.accDescription[i] == '\n') {
            accD.push_back('\n');
            i++;
            while (i < drafts.accDescription.size()) {
                size_t next = i;
                int cp = charCodeAtUtf8(drafts.accDescription, i, &next);
                if (!isSpace(cp)) break;
                i = next;
            }
        } else {
            accD.push_back(drafts.accDescription[i++]);
        }
    }
    model.accDescr = std::move(accD);
    return model;
}
// ============================================================================
// src/diagrams/usecase/parser.ts
// ============================================================================

constexpr uint8_t P_LINE = 1;
constexpr uint8_t P_RELATION = 2;
constexpr uint8_t P_STYLE_PART = 4;
constexpr uint8_t P_FOLLOWS_NAME = 8;
constexpr uint8_t P_BOUNDARY_LINE = 16;
constexpr uint8_t P_CLOSES_LABEL = 32;
constexpr uint8_t P_METADATA_FIRST = 64;

inline const std::array<uint8_t, RPAREN + 1>& parserFlags() {
    static const std::array<uint8_t, RPAREN + 1> flags = []() {
        std::array<uint8_t, RPAREN + 1> f{};
        const auto& names = tokenNames();
        auto flag = [&](const std::string& list, uint8_t bits) {
            for (const auto& name : splitStr(list, '|')) {
                if (name == "WORD") {
                    for (int t = USECASE; t <= IDENTIFIER; t++) f[t] |= bits;
                } else {
                    auto it = std::find(names.begin(), names.end(), name);
                    if (it != names.end()) {
                        f[static_cast<size_t>(it - names.begin())] |= bits;
                    }
                }
            }
        };
        const std::string ARROWS = "DEPENDENCY_ARROW|GENERALIZATION|FORWARD_SOLID|BACKWARD_SOLID|MARKERLESS_SOLID|FORWARD_CIRCLE|BACKWARD_CIRCLE|FORWARD_CROSS|BACKWARD_CROSS";
        const std::string NAMES = "IDENTIFIER|PLAIN_STRING|MARKDOWN_STRING";
        const std::string STATEMENTS = "ACC_TITLE_LINE|ACC_DESCR_LINE|ACC_DESCR_BLOCK|DIRECTION|ACTOR|SYSTEM_BOUNDARY|NOTE|JSON_DECLARATION_START|CLASS_DEF|CLASS|STYLE";
        const std::string STYLE_PARTS = "WORD|PLAIN_STRING|NUMBER|HASH_COLOR|CSS_IDENTIFIER|CSS_ESCAPED_COMMA|DASH|DOT|PERCENT|CSS_PUNCTUATION|COLON|LPAREN|RPAREN|LBRACKET|RBRACKET|LBRACE|RBRACE|AT|MARKERLESS_SOLID";

        flag(Pele::cat({"NEWLINE|COMMENT|", STATEMENTS, "|", NAMES}),  P_LINE);
        flag(Pele::cat({"IDENTIFIER|", ARROWS}),  P_RELATION);
        flag(STYLE_PARTS, P_STYLE_PART);
        flag(Pele::cat({"STEREOTYPE_START|CLASS_SEPARATOR|IDENTIFIER|", ARROWS, "|NEWLINE|EOF"}),  P_FOLLOWS_NAME);
        flag(Pele::cat({"NEWLINE|COMMENT|ACTOR|", NAMES}),  P_BOUNDARY_LINE);
        flag("FORWARD_SOLID|MARKERLESS_SOLID|FORWARD_CIRCLE|FORWARD_CROSS", P_CLOSES_LABEL);
        flag("NEWLINE|IDENTIFIER|PLAIN_STRING|RBRACE", P_METADATA_FIRST);
        return f;
    }();
    return flags;
}

inline std::string generatedId(const std::string& label) noexcept {
    auto u16 = toUtf16(label);
    std::string out;
    out.reserve(u16.size());
    for (uint16_t u : u16) {
        if ((u >= '0' && u <= '9') || (u >= 'A' && u <= 'Z') || (u >= 'a' && u <= 'z') || u == '_') {
            out.push_back(static_cast<char>(u));
        } else {
            out.push_back('_');
        }
    }
    return out;
}

struct ParseItem {
    std::string id;
    DraftLabel label;
    Span span{0, 0};
    bool generated = false;
    std::string shape;
    std::vector<DraftProperty> metadata;
    bool hasMetadata = false;
    std::string stereotype;
    Span stereotypeSpan{0, 0};
    bool hasStereotypeSpan = false;
    std::vector<std::string> classes;
    std::vector<Span> classSpans;
    bool explicitDecl = false;
    Span nodeSpan{0, 0};
};

struct ParseTail {
    std::string explicitId;
    bool hasExplicitId = false;
    Span explicitIdSpan{0, 0};
    bool hasExplicitIdSpan = false;
    std::string type = "association";
    int arrowType = SOLID_ARROW;
    DraftLabel label;
    bool hasLabel = false;
    int minlen = 1;
    ParseItem target;
    Span span{0, 0};
};

inline UsecaseModel parseUsecase(const std::string& source, const std::string& title = "") {
    const auto& FLAGS = parserFlags();
    static const Pele::FlatSet<std::string> FORBIDDEN = {"allowmixing", "newpage", "package", "rectangle", "skinparam"};

    Tokens toks = tokenize(source);
    const auto& types = toks.types;
    const auto& starts = toks.starts;
    const auto& ends = toks.ends;
    int p = 0;
    int lastEnd = 0;
    Drafts drafts;
    bool hasParent = false;
    DraftBoundary parent;
    std::vector<std::pair<size_t, int>> pendingJson;

    auto image = [&](int k) -> std::string {
        return source.substr(static_cast<size_t>(starts[k]), static_cast<size_t>(ends[k] - starts[k]));
    };

    auto tokSpan = [&](int k) -> Span {
        return {starts[k], ends[k]};
    };

    auto fail = [&](const std::string& detail) {
        auto [ln, col] = lineColumn(source, starts[p]);
        throw PeleError(Pele::cat({"Error parsing usecase diagram: ", detail}),  "syntax",  "usecase",  ln,  col);
    };

    auto expect = [&](int type) -> int {
        if (types[p] != type) fail("Unexpected token");
        return p++;
    };

    auto isWordTok = [](int type) -> bool {
        return type >= USECASE && type <= IDENTIFIER;
    };

    auto isLabelTextTok = [](int type) -> bool {
        return type >= USECASE && type <= LABEL_SYMBOL;
    };

    auto lineEnd = [&]() {
        if (p > 0) lastEnd = ends[p - 1];
        if (types[p] == NEWLINE) p++;
        else if (types[p] != T_EOF) fail("Expected newline or EOF");
    };

    auto content = [&](int k) -> Span {
        int trim = types[k] == MARKDOWN_STRING ? 2 : types[k] == PLAIN_STRING ? 1 : 0;
        return {starts[k] + trim, ends[k] - trim};
    };

    auto tokenLabel = [&](int k) -> DraftLabel {
        Span at = content(k);
        return {source.substr(static_cast<size_t>(at.first), static_cast<size_t>(at.second - at.first)), types[k] == MARKDOWN_STRING ? "markdown" : "text", at};
    };

    auto parseLabel = [&]() -> DraftLabel {
        int type = types[p];
        if (type == PLAIN_STRING || type == MARKDOWN_STRING) return tokenLabel(p++);
        if (!isLabelTextTok(type)) fail("Expected label");
        int first = p;
        while (isLabelTextTok(types[p])) p++;
        Span at = {starts[first], ends[p - 1]};
        return {source.substr(static_cast<size_t>(at.first), static_cast<size_t>(at.second - at.first)), "text", at};
    };

    auto isKeyTok = [](int type) -> bool {
        return type == IDENTIFIER || type == PLAIN_STRING;
    };

    auto parseProperty = [&]() -> DraftProperty {
        int key = p;
        if (!isKeyTok(types[p])) fail("Expected property key");
        p++;
        expect(COLON);
        int value = p;
        int type = types[p];
        if (!isKeyTok(type) && type != TRUE_TOK && type != FALSE_TOK) fail("Expected property value");
        p++;
        Span keySpan = content(key);
        Span valueSpan = content(value);
        DraftProperty prop;
        prop.key = source.substr(static_cast<size_t>(keySpan.first), static_cast<size_t>(keySpan.second - keySpan.first));
        if (type == TRUE_TOK || type == FALSE_TOK) {
            prop.isBool = true;
            prop.boolVal = (type == TRUE_TOK);
        } else {
            prop.isBool = false;
            prop.strVal = source.substr(static_cast<size_t>(valueSpan.first), static_cast<size_t>(valueSpan.second - valueSpan.first));
        }
        prop.span = {starts[key], ends[value]};
        prop.keySpan = keySpan;
        prop.valueSpan = valueSpan;
        return prop;
    };

    auto separatorAhead = [&]() -> bool {
        int a = types[p];
        int b = types[p + 1];
        int c = types[p + 2];
        if (a == COMMA) return isKeyTok(b) || (b == NEWLINE && (c == NEWLINE || isKeyTok(c)));
        if (a != NEWLINE) return false;
        return b == COMMA || isKeyTok(b) || (b == NEWLINE && (c == NEWLINE || c == COMMA || isKeyTok(c)));
    };

    auto parseMetadata = [&]() -> std::vector<DraftProperty> {
        expect(METADATA_START);
        std::vector<DraftProperty> props;
        while (types[p] == NEWLINE) p++;
        if (isKeyTok(types[p])) {
            props.push_back(parseProperty());
            while (separatorAhead()) {
                if (types[p] == COMMA) p++;
                else {
                    while (types[p] == NEWLINE) p++;
                    if (types[p] == COMMA) p++;
                }
                while (types[p] == NEWLINE) p++;
                props.push_back(parseProperty());
            }
            if (types[p] == COMMA) p++;
            while (types[p] == NEWLINE) p++;
        }
        expect(RBRACE);
        return props;
    };

    auto classSuffix = [&](std::vector<std::string>& classes, std::vector<Span>& spans) {
        do {
            p++;
            int id = expect(IDENTIFIER);
            classes.push_back(image(id));
            spans.push_back(tokSpan(id));
        } while (types[p] == COMMA);
    };

    auto parseName = [&](bool brackets) -> ParseItem {
        int k = p;
        int type = types[p];
        ParseItem item;
        item.span = content(k);
        item.generated = (type != IDENTIFIER);
        if (type == IDENTIFIER) {
            p++;
            item.id = image(k);
            int open = types[p];
            if (open == LPAREN || (brackets && open == LBRACKET)) {
                p++;
                item.label = parseLabel();
                expect(open == LPAREN ? RPAREN : RBRACKET);
                item.shape = (open == LPAREN) ? "ellipse" : "rect";
            } else {
                item.label = tokenLabel(k);
            }
        } else if (type == PLAIN_STRING || type == MARKDOWN_STRING) {
            p++;
            item.label = tokenLabel(k);
            item.id = generatedId(item.label.text);
        } else {
            fail("Expected name");
        }
        return item;
    };

    auto parseElement = [&](bool actor) -> ParseItem {
        int first = p;
        ParseItem item = parseName(!actor);
        if (actor) item.shape.clear();
        if (types[p] == METADATA_START) {
            item.metadata = parseMetadata();
            item.hasMetadata = true;
        }
        if (types[p] == STEREOTYPE_START) {
            p++;
            int text = expect(STEREOTYPE_TEXT);
            expect(STEREOTYPE_END);
            item.stereotype = trimStr(image(text));
            item.stereotypeSpan = tokSpan(text);
            item.hasStereotypeSpan = true;
        }
        if (types[p] == CLASS_SEPARATOR) classSuffix(item.classes, item.classSpans);
        item.explicitDecl = !item.shape.empty() || item.hasMetadata || !item.stereotype.empty();
        item.nodeSpan = {starts[first], ends[p - 1]};
        return item;
    };

    auto toDraft = [&](const ParseItem& item, const std::string& kind) -> DraftElement {
        DraftElement d;
        d.id = item.id;
        d.kind = kind;
        d.label = item.label;
        d.span = item.span;
        d.generated = item.generated;
        d.hasParent = hasParent;
        if (hasParent) d.parent = parent;
        d.shape = item.shape;
        d.metadata = item.metadata;
        d.hasMetadata = item.hasMetadata;
        d.stereotype = item.stereotype;
        d.classes = item.classes;
        return d;
    };

    auto toEndpoint = [&](const ParseItem& item, bool declares) -> DraftEndpoint {
        return {item.id, item.label, item.span, item.generated, declares, !item.classes.empty()};
    };

    auto labeledRight = [&](uint8_t closers) -> bool {
        if (types[p] < USECASE || types[p] > MARKDOWN_STRING) return false;
        for (int k = p + 1; ; k++) {
            int type = types[k];
            if (type == MARKERLESS_SOLID || (FLAGS[type] & closers) != 0) return true;
            if (type < USECASE || type > MARKDOWN_STRING) return false;
        }
    };

    auto minimumLength = [&](int k) -> int {
        int dashes = ends[k] - starts[k] - (types[k] == MARKERLESS_SOLID ? 0 : 1);
        return std::max(1, dashes - 1);
    };

    auto parseRelationTail = [&]() -> ParseTail {
        int first = p;
        ParseTail tail;
        if (types[p] == IDENTIFIER && types[p + 1] == AT) {
            tail.explicitId = image(p);
            tail.hasExplicitId = true;
            tail.explicitIdSpan = tokSpan(p);
            tail.hasExplicitIdSpan = true;
            p += 2;
        }
        int k = p;
        int type = types[p++];
        if (type == DEPENDENCY_ARROW) {
            expect(COLON);
            if (types[p] != INCLUDE && types[p] != EXTEND) fail("Expected include or extend");
            tail.type = (types[p] == INCLUDE) ? "include" : "extend";
            tail.label = {tail.type, "text", tokSpan(p)};
            tail.hasLabel = true;
            p++;
        } else if (type == GENERALIZATION) {
            tail.type = "generalization";
        } else if (type == FORWARD_SOLID) {
            tail.minlen = minimumLength(k);
        } else if (type == BACKWARD_SOLID) {
            tail.arrowType = BACK_ARROW;
            if (ends[k] - starts[k] == 3 && labeledRight(0)) {
                tail.label = parseLabel();
                tail.hasLabel = true;
                tail.minlen = minimumLength(expect(MARKERLESS_SOLID));
            } else {
                tail.minlen = minimumLength(k);
            }
        } else if (type == MARKERLESS_SOLID) {
            tail.arrowType = LINE_SOLID;
            if (ends[k] - starts[k] == 2 && labeledRight(P_CLOSES_LABEL)) {
                tail.label = parseLabel();
                tail.hasLabel = true;
                int close = types[p];
                if (close == FORWARD_SOLID) tail.arrowType = SOLID_ARROW;
                else if (close == FORWARD_CIRCLE) tail.arrowType = CIRCLE_ARROW;
                else if (close == FORWARD_CROSS) tail.arrowType = CROSS_ARROW;
                else if (close != MARKERLESS_SOLID) fail("Expected closing arrow");
                if (tail.arrowType == SOLID_ARROW || tail.arrowType == LINE_SOLID) tail.minlen = minimumLength(p);
                p++;
            } else {
                tail.minlen = minimumLength(k);
            }
        } else if (type == FORWARD_CIRCLE) {
            tail.arrowType = CIRCLE_ARROW;
        } else if (type == FORWARD_CROSS) {
            tail.arrowType = CROSS_ARROW;
        } else if (type == BACKWARD_CIRCLE || type == BACKWARD_CROSS) {
            tail.arrowType = (type == BACKWARD_CIRCLE) ? CIRCLE_ARROW_REVERSED : CROSS_ARROW_REVERSED;
            if (labeledRight(0)) {
                tail.label = parseLabel();
                tail.hasLabel = true;
                expect(MARKERLESS_SOLID);
            }
        } else {
            p--;
            fail("Expected relationship arrow");
        }
        tail.target = parseElement(false);
        tail.span = {starts[first], ends[p - 1]};
        return tail;
    };

    auto addRelationship = [&](const ParseItem& srcItem, bool declaration, const ParseTail& tail) {
        const ParseItem& target = tail.target;
        if (target.explicitDecl) drafts.elements.push_back(toDraft(target, "usecase"));
        DraftRelationship rel;
        rel.source = toEndpoint(srcItem, declaration);
        rel.target = toEndpoint(target, target.explicitDecl);
        rel.span = tail.span;
        rel.id = tail.explicitId;
        rel.hasId = tail.hasExplicitId;
        rel.idSpan = tail.explicitIdSpan;
        rel.hasIdSpan = tail.hasExplicitIdSpan;
        rel.type = tail.type;
        rel.arrowType = tail.arrowType;
        rel.label = tail.label;
        rel.hasLabel = tail.hasLabel;
        rel.minlen = tail.minlen;
        drafts.relationships.push_back(std::move(rel));
    };

    auto parseActors = [&](bool top) {
        p++;
        std::vector<ParseItem> items = {parseElement(true)};
        bool hasTail = false;
        ParseTail tail;
        if (types[p] == COMMA) {
            do {
                p++;
                items.push_back(parseElement(true));
            } while (types[p] == COMMA);
        } else if (top && (FLAGS[types[p]] & P_RELATION) != 0) {
            tail = parseRelationTail();
            hasTail = true;
        }
        lineEnd();
        for (const auto& item : items) drafts.elements.push_back(toDraft(item, "actor"));
        if (hasTail) addRelationship(items[0], true, tail);
    };

    auto parseEntity = [&](bool top) {
        ParseItem srcItem = parseElement(false);
        bool hasTail = false;
        ParseTail tail;
        if (top && (FLAGS[types[p]] & P_RELATION) != 0) {
            tail = parseRelationTail();
            hasTail = true;
        }
        lineEnd();
        if (!hasTail) {
            srcItem.explicitDecl = true;
            if (srcItem.shape.empty()) srcItem.shape = "ellipse";
        }
        if (srcItem.explicitDecl) drafts.elements.push_back(toDraft(srcItem, "usecase"));
        if (hasTail) addRelationship(srcItem, srcItem.explicitDecl, tail);
    };

    auto parseStyle = [&]() -> std::string {
        int first = p;
        int type = types[p];
        if (isWordTok(type) || type == CSS_IDENTIFIER) p++;
        else if (type == MARKERLESS_SOLID) {
            p++;
            if (!isWordTok(types[p])) fail("Expected WORD");
            p++;
        } else fail("Expected style property");
        expect(COLON);
        if ((FLAGS[types[p]] & P_STYLE_PART) == 0) fail("Expected style value");
        while ((FLAGS[types[p]] & P_STYLE_PART) != 0) p++;
        return replaceAllStr(source.substr(static_cast<size_t>(starts[first]), static_cast<size_t>(ends[p - 1] - starts[first])), "\\,", ",");
    };

    auto parseStyles = [&]() -> std::vector<std::string> {
        std::vector<std::string> out = {parseStyle()};
        while (types[p] == COMMA) {
            p++;
            out.push_back(parseStyle());
        }
        return out;
    };

    auto parseIdentifiers = [&]() -> std::vector<int> {
        std::vector<int> out = {expect(IDENTIFIER)};
        while (types[p] == COMMA) {
            p++;
            out.push_back(expect(IDENTIFIER));
        }
        return out;
    };

    std::function<void(bool)> parseLine;

    auto parseBoundary = [&]() {
        p++;
        ParseItem named = parseName(true);
        DraftBoundary group;
        group.id = named.id;
        group.label = named.label;
        group.span = named.span;
        group.generated = named.generated;
        group.classes = named.classes;
        bool hasInline = false;
        std::vector<DraftProperty> inlineMeta;
        if (types[p] == METADATA_START) {
            inlineMeta = parseMetadata();
            hasInline = true;
        }
        if (types[p] == CLASS_SEPARATOR) classSuffix(group.classes, named.classSpans);
        lineEnd();
        drafts.boundaries.push_back(group);
        hasParent = true;
        parent = group;
        while ((FLAGS[types[p]] & P_BOUNDARY_LINE) != 0) parseLine(false);
        hasParent = false;
        expect(END);
        lineEnd();
        if (hasInline) {
            drafts.metadata.push_back({group.id, group.span, std::move(inlineMeta)});
        }
    };

    auto parseTopStatement = [&]() {
        int k = p;
        int type = types[p];
        if (type == ACC_TITLE_LINE || type == ACC_DESCR_LINE || type == ACC_DESCR_BLOCK) {
            p++;
            std::string text = image(k);
            if (type == ACC_DESCR_BLOCK) {
                size_t lb = text.find('{');
                size_t rb = text.rfind('}');
                if (lb != std::string::npos && rb != std::string::npos && rb > lb) {
                    drafts.accDescription = trimStr(text.substr(lb + 1, rb - lb - 1));
                }
            } else if (type == ACC_DESCR_LINE) {
                size_t col = text.find(':');
                if (col != std::string::npos) drafts.accDescription = trimStr(text.substr(col + 1));
            } else {
                size_t col = text.find(':');
                if (col != std::string::npos) drafts.accTitle = trimStr(text.substr(col + 1));
            }
        } else if (type == DIRECTION) {
            p++;
            int val = types[p];
            if (val < TD || val > RL) fail("Expected direction");
            drafts.direction = (val == TD) ? "TB" : image(p);
            p++;
        } else if (type == ACTOR) {
            parseActors(true);
            return;
        } else if (type == SYSTEM_BOUNDARY) {
            parseBoundary();
            return;
        } else if (type == NOTE) {
            p++;
            expect(FOR);
            int target = expect(IDENTIFIER);
            DraftLabel text = parseLabel();
            drafts.notes.push_back({image(target), tokSpan(target), text});
        } else if (type == JSON_DECLARATION_START) {
            p++;
            int literal = expect(JSON_OBJECT_LITERAL);
            std::vector<std::string> classes;
            std::vector<Span> classSpans;
            if (types[p] == CLASS_SEPARATOR) classSuffix(classes, classSpans);
            int from = starts[k] + 4;
            while (charCodeAt(source, from) == 32 || charCodeAt(source, from) == 9) from++;
            int to = from;
            while (isLexWord(charCodeAt(source, to))) to++;
            DraftJson json;
            json.id = source.substr(static_cast<size_t>(from), static_cast<size_t>(to - from));
            json.span = {from, to};
            json.classes = std::move(classes);
            pendingJson.push_back({drafts.jsons.size(), literal});
            drafts.jsons.push_back(std::move(json));
        } else if (type == CLASS_DEF) {
            p++;
            auto ids = parseIdentifiers();
            std::vector<std::string> idStrs;
            for (int id : ids) idStrs.push_back(image(id));
            drafts.classDefs.push_back({std::move(idStrs), parseStyles()});
        } else if (type == CLASS) {
            p++;
            auto targets = parseIdentifiers();
            auto classes = parseIdentifiers();
            std::vector<DraftClassAssignTarget> tList;
            for (int id : targets) tList.push_back({image(id), tokSpan(id)});
            std::vector<std::string> cList;
            for (int id : classes) cList.push_back(image(id));
            drafts.classes.push_back({std::move(tList), std::move(cList)});
        } else if (type == STYLE) {
            p++;
            int target = expect(IDENTIFIER);
            drafts.styles.push_back({image(target), tokSpan(target), parseStyles()});
        } else {
            int next = types[p + 1];
            if (next == METADATA_START) {
                if ((FLAGS[types[p + 2]] & P_METADATA_FIRST) == 0) fail("Unexpected metadata");
                p++;
                Span at = content(k);
                std::string targetId = (type != IDENTIFIER)
                    ? generatedId(source.substr(static_cast<size_t>(at.first), static_cast<size_t>(at.second - at.first)))
                    : image(k);
                auto assigned = parseMetadata();
                drafts.metadata.push_back({targetId, at, std::move(assigned)});
            } else {
                bool follows = (FLAGS[next] & P_FOLLOWS_NAME) != 0 || (type == IDENTIFIER && (next == LPAREN || next == LBRACKET));
                if (!follows || (type == IDENTIFIER && FORBIDDEN.count(toLowerAscii(image(k))))) {
                    fail("Unexpected statement");
                }
                parseEntity(true);
                return;
            }
        }
        lineEnd();
    };

    parseLine = [&](bool top) {
        int type = types[p];
        if (type == NEWLINE) {
            p++;
            return;
        }
        if (type == COMMENT) {
            p++;
            lineEnd();
            return;
        }
        if (top) parseTopStatement();
        else if (type == ACTOR) parseActors(false);
        else parseEntity(false);
    };

    expect(USECASE);
    lineEnd();
    while ((FLAGS[types[p]] & P_LINE) != 0) parseLine(true);
    if (types[p] != T_EOF) fail("Redundant input");
    for (const auto& [jsonIdx, literal] : pendingJson) {
        OrderedJson parsed = parseOrderedJson(image(literal));
        drafts.jsons[jsonIdx].value = std::move(parsed.value);
        drafts.jsons[jsonIdx].propertyOrder = std::move(parsed.propertyOrder);
    }
    return resolveUsecase(source, drafts, title);
}

// ============================================================================
// src/diagrams/usecase/graph.ts
// ============================================================================

struct JsonRow {
    std::string key;
    std::string accessibleKey;
    std::string value;
};

struct UsecaseGraphNode {
    std::string id;
    std::string label;
    std::string labelType = "text";
    std::string shape;
    bool isGroup = false;
    double padding = 10.0;
    std::string cssClasses;
    std::vector<std::string> cssStyles;
    std::vector<std::string> cssCompiledStyles;
    std::string actorType;
    bool business = false;
    std::string icon;
    std::string stereotype;
    std::string parentId;
    std::string noteTarget;
    std::vector<JsonRow> jsonRows;
    bool hasJsonRows = false;
    std::string boundaryType;
};

struct UsecaseGraphEdge {
    std::string id;
    std::string start;
    std::string end;
    std::string relationshipType;
    std::string pattern = "solid";
    std::string arrowTypeStart = "none";
    std::string arrowTypeEnd = "none";
    std::string label;
    bool hasLabel = false;
    std::string labelType = "text";
    std::string classes;
    std::vector<std::string> style;
    std::vector<std::string> cssCompiledStyles;
    bool animate = false;
    std::string animation;
    int minlen = 1;
    bool explicitId = false;
    bool internal = false;
};

struct UsecaseGraph {
    std::vector<UsecaseGraphNode> nodes;
    std::vector<UsecaseGraphEdge> edges;
};

inline std::vector<JsonRow> flattenJsonRows(const UsecaseJsonNode& json) noexcept {
    std::vector<JsonRow> rows;
    struct StackItem {
        const JsonVal* val;
        std::string path;
        std::string pointer;
    };
    std::vector<StackItem> stack = {{&json.value, "", ""}};
    std::vector<StackItem> children;
    while (!stack.empty()) {
        StackItem top = std::move(stack.back());
        stack.pop_back();
        children.clear();
        const JsonVal* current = top.val;
        const std::string& path = top.path;
        const std::string& pointer = top.pointer;
        if (!current) {
            rows.push_back({path, path, "undefined"});
            continue;
        }
        if (current->type == JsonVal::Array) {
            if (current->arr.empty()) {
                rows.push_back({path, path, "[]"});
            } else {
                bool allPrim = true;
                for (const auto& item : current->arr) {
                    if (!item.isPrimitiveOrNull()) { allPrim = false; break; }
                }
                if (allPrim) {
                    for (size_t i = 0; i < current->arr.size(); i++) {
                        rows.push_back({i == 0 ? path : "", path, current->arr[i].scalar});
                    }
                } else {
                    for (size_t i = 0; i < current->arr.size(); i++) {
                        children.push_back({&current->arr[i], path + "[" + std::to_string(i) + "]", pointer + "/" + std::to_string(i)});
                    }
                }
            }
        } else if (current->type == JsonVal::Object) {
            std::vector<std::string> keys;
            auto oIt = json.propertyOrder.find(pointer);
            if (oIt != json.propertyOrder.end()) keys = oIt->second;
            else {
                for (const auto& [k, _] : current->obj) keys.push_back(k);
            }
            if (keys.empty()) {
                rows.push_back({path, path, "{}"});
            }
            for (const auto& key : keys) {
                auto vIt = current->obj.find(key);
                const JsonVal* childVal = (vIt != current->obj.end()) ? &vIt->second : nullptr;
                std::string childPath = path.empty() ? key : Pele::cat({path, ".", key});
                std::string childPtr = Pele::cat({pointer, "/", replaceAllStr(replaceAllStr(key, "~", "~0"), "/", "~1")});
                children.push_back({childVal, std::move(childPath), std::move(childPtr)});
            }
        } else {
            rows.push_back({path, path, current->scalar});
        }
        for (int i = static_cast<int>(children.size()) - 1; i >= 0; i--) {
            stack.push_back(std::move(children[i]));
        }
    }
    return rows;
}

inline std::string joinClassList(const std::vector<std::string>& parts) noexcept {
    std::string out;
    for (const auto& p : parts) {
        if (p.empty()) continue;
        if (!out.empty()) out.push_back(' ');
        out += p;
    }
    return out;
}

inline std::pair<std::string, std::string> relationshipMarkers(const Relationship& rel) noexcept {
    static const std::pair<const char*, const char*> MARKERS[] = {
        {"none", "arrow_point"},
        {"arrow_point", "none"},
        {"none", "none"},
        {"none", "arrow_circle"},
        {"none", "arrow_cross"},
        {"arrow_circle", "none"},
        {"arrow_cross", "none"}
    };
    if (rel.type == "generalization") return {"none", "extension"};
    if (rel.type != "association") return {MARKERS[SOLID_ARROW].first, MARKERS[SOLID_ARROW].second};
    if (rel.arrowType >= 0 && rel.arrowType <= 6) return {MARKERS[rel.arrowType].first, MARKERS[rel.arrowType].second};
    return {MARKERS[LINE_SOLID].first, MARKERS[LINE_SOLID].second};
}

inline UsecaseGraph buildUsecaseGraph(const UsecaseModel& model) noexcept {
    UsecaseGraph g;

    auto compiled = [&](const std::vector<std::string>& classNames) -> std::vector<std::string> {
        std::vector<std::string> order;
        Pele::FlatMap<std::string, std::string> out;
        std::vector<std::string> all = {"default"};
        all.insert(all.end(), classNames.begin(), classNames.end());
        for (const auto& name : all) {
            auto it = model.classDefs.find(name);
            if (it == model.classDefs.end()) continue;
            for (const auto& raw : it->second.styles) {
                std::string style = trimStr(raw);
                size_t colon = style.find(':');
                std::string prop = trimStr(colon == std::string::npos ? style : style.substr(0, colon));
                if (!prop.empty()) {
                    if (!out.count(prop)) order.push_back(prop);
                    out[prop] = style;
                }
            }
        }
        std::vector<std::string> res;
        for (const auto& k : order) res.push_back(out[k]);
        return res;
    };

    for (const auto& id : model.actorOrder) {
        const auto& actor = model.actors.at(id);
        UsecaseGraphNode n;
        n.id = actor.id;
        n.label = actor.label;
        n.labelType = actor.labelType;
        n.shape = actor.type == "hollow" ? "usecaseActorHollow"
                : actor.type == "awesome" ? "usecaseActorAwesome"
                : actor.type == "icon" ? "usecaseActorIcon"
                : "usecaseActor";
        n.isGroup = false;
        n.padding = 10.0;
        std::vector<std::string> cls = {"default",  "usecase-actor", Pele::cat({"usecase-actor-", actor.type})};
        if (actor.business) cls.push_back("usecase-business");
        cls.insert(cls.end(), actor.classes.begin(), actor.classes.end());
        n.cssClasses = joinClassList(cls);
        n.cssStyles = actor.styles;
        n.cssCompiledStyles = compiled(actor.classes);
        n.actorType = actor.type;
        n.business = actor.business;
        n.icon = actor.icon;
        n.stereotype = actor.stereotype;
        n.parentId = actor.parentId;
        g.nodes.push_back(std::move(n));
    }

    for (const auto& id : model.useCaseOrder) {
        const auto& uc = model.useCases.at(id);
        bool ellipse = (uc.shape == "ellipse");
        UsecaseGraphNode n;
        n.id = uc.id;
        n.label = uc.label;
        n.labelType = uc.labelType;
        n.shape = !ellipse ? "rect" : (uc.business ? "usecaseBusiness" : "usecaseEllipse");
        n.isGroup = false;
        n.padding = ellipse ? 20.0 : 10.0;
        std::vector<std::string> cls = {"default",  "usecase-element", Pele::cat({"usecase-", uc.shape})};
        if (uc.business) cls.push_back("usecase-business");
        cls.insert(cls.end(), uc.classes.begin(), uc.classes.end());
        n.cssClasses = joinClassList(cls);
        n.cssStyles = uc.styles;
        n.cssCompiledStyles = compiled(uc.classes);
        n.business = uc.business;
        n.stereotype = uc.stereotype;
        n.parentId = uc.parentId;
        g.nodes.push_back(std::move(n));
    }

    for (const auto& id : model.noteOrder) {
        const auto& note = model.notes.at(id);
        UsecaseGraphNode n;
        n.id = note.id;
        n.label = note.label;
        n.labelType = note.labelType;
        n.shape = "note";
        n.isGroup = false;
        n.padding = 10.0;
        n.cssClasses = "default usecase-note";
        n.cssCompiledStyles = compiled({});
        n.noteTarget = note.target;
        g.nodes.push_back(std::move(n));
    }

    for (const auto& id : model.jsonOrder) {
        const auto& json = model.jsonNodes.at(id);
        UsecaseGraphNode n;
        n.id = json.id;
        n.label = json.id;
        n.labelType = "text";
        n.shape = "usecaseJsonTable";
        n.isGroup = false;
        n.padding = 10.0;
        std::vector<std::string> cls = {"default", "usecase-json-table"};
        cls.insert(cls.end(), json.classes.begin(), json.classes.end());
        n.cssClasses = joinClassList(cls);
        n.cssStyles = json.styles;
        n.cssCompiledStyles = compiled(json.classes);
        n.jsonRows = flattenJsonRows(json);
        n.hasJsonRows = true;
        g.nodes.push_back(std::move(n));
    }

    for (const auto& id : model.boundaryOrder) {
        const auto& boundary = model.systemBoundaries.at(id);
        UsecaseGraphNode n;
        n.id = boundary.id;
        n.label = boundary.label;
        n.labelType = boundary.labelType;
        n.shape = "usecaseSystemBoundary";
        n.isGroup = true;
        n.padding = 20.0;
        std::vector<std::string> cls = {"default",  "system-boundary", Pele::cat({"system-boundary-", boundary.type})};
        cls.insert(cls.end(), boundary.classes.begin(), boundary.classes.end());
        n.cssClasses = joinClassList(cls);
        n.cssStyles = boundary.styles;
        n.cssCompiledStyles = compiled(boundary.classes);
        n.boundaryType = boundary.type;
        g.nodes.push_back(std::move(n));
    }

    for (const auto& rel : model.relationships) {
        bool semantic = (rel.type == "include" || rel.type == "extend");
        auto [startM, endM] = relationshipMarkers(rel);
        bool animated = rel.animate || !rel.animation.empty();
        UsecaseGraphEdge e;
        e.id = rel.id;
        e.start = rel.source;
        e.end = rel.target;
        e.relationshipType = rel.type;
        e.pattern = semantic ? "dotted" : "solid";
        e.arrowTypeStart = startM;
        e.arrowTypeEnd = endM;
        std::vector<std::string> cls = {"default",  "relationship", Pele::cat({"relationship-", rel.type})};
        cls.insert(cls.end(), rel.classes.begin(), rel.classes.end());
        if (animated) cls.push_back(Pele::cat({"edge-animation-", (!rel.animation.empty() ? rel.animation : "fast")}));
        e.classes = joinClassList(cls);
        e.style = rel.styles;
        e.cssCompiledStyles = compiled(rel.classes);
        e.animate = rel.animate;
        e.animation = rel.animation;
        e.minlen = rel.minlen;
        e.explicitId = rel.explicitId;
        e.internal = false;
        if (semantic) {
            e.label = rel.type;
            e.hasLabel = true;
            e.labelType = "text";
        } else if (rel.type == "association") {
            if (rel.hasLabel && !rel.label.empty()) {
                e.label = rel.label;
                e.hasLabel = true;
            }
            if (rel.hasLabelType && !rel.labelType.empty()) {
                e.labelType = rel.labelType;
            }
        }
        g.edges.push_back(std::move(e));
    }

    for (const auto& id : model.noteOrder) {
        const auto& note = model.notes.at(id);
        UsecaseGraphEdge e;
        e.id = Pele::cat({note.id, "-edge"});
        e.start = note.id;
        e.end = note.target;
        e.relationshipType = "note";
        e.pattern = "dotted";
        e.arrowTypeStart = "none";
        e.arrowTypeEnd = "none";
        e.classes = "default relationship relationship-note";
        e.cssCompiledStyles = compiled({});
        e.animate = false;
        e.minlen = 1;
        e.explicitId = false;
        e.internal = true;
        g.edges.push_back(std::move(e));
    }

    return g;
}

// ============================================================================
// src/diagrams/usecase/render.ts
// ============================================================================

constexpr double UC_NODE_SEP = 32.0;
constexpr double UC_EDGE_SEP = 16.0;
constexpr double UC_RANK_SEP = 72.0;
constexpr double UC_GROUP_PAD = 20.0;
constexpr double UC_LOOP = 26.0;
constexpr double UC_FIGURE_W = 40.0;
constexpr double UC_FIGURE_H = 60.0;
constexpr double UC_FIGURE_GAP = 6.0;
constexpr double UC_TAB_PAD = 6.0;
constexpr double UC_CELL_PAD = 10.0;
constexpr double UC_ROW_PAD = 8.0;
constexpr double UC_FOLD = 9.0;
constexpr double UC_EXTENSION = 12.0;
constexpr double UC_PORT_GAP = 14.0;
inline const std::string UC_SHAPE_ATTRS = " fill=\"var(--_s)\" stroke=\"var(--_b)\"";

enum class ViewKind {
    Actor,
    Ellipse,
    Box,
    Json,
    Boundary
};

struct View {
    UsecaseGraphNode node;
    ViewKind kind = ViewKind::Box;
    ResolvedStyle style;
    Label label;
    Label stereotype;
    double w = 0.0;
    double h = 0.0;
    std::vector<std::pair<Label, Label>> rows;
    double keyW = 0.0;
    double headH = 0.0;
    int loops = 0;
};

inline double ucNumberOption(const Config& config, const std::string& key, double fallback) noexcept {
    if (const auto* uc = config.get("usecase"); uc && uc->isMap()) {
        if (const auto* v = uc->get(key); v && v->isNumber() && v->nVal > 0) {
            return std::min(v->nVal, 2000.0);
        }
    }
    return fallback;
}

inline std::string ucLiteral(const std::string& text) noexcept {
    std::string out;
    out.reserve(text.size() + 8);
    for (unsigned char c : text) {
        if (c == '&' || c == '<' || c == '>' || c == '\\') {
            out += "&#" + std::to_string(static_cast<int>(c)) + ";";
        } else {
            out.push_back(static_cast<char>(c));
        }
    }
    return out;
}

inline Label ucText(const std::string& raw, bool markdown, const TextMeasurer& measurer, double size, double wrap, int base = 0) noexcept {
    return layoutLabel((!raw.empty() && !markdown) ? ucLiteral(raw) : raw, markdown, measurer, size, wrap, base);
}

inline std::pair<double, double> ucAnchor(const View& view, const Flowchart::CNode& c, int side, double along) noexcept {
    double sign = (side == 1 || side == 2) ? 1.0 : -1.0;
    bool upright = (side == 1 || side == 3);
    double reach = (upright ? view.w : view.h) / 2.0;
    double middle = 0.0;
    if (view.kind == ViewKind::Actor) {
        if (upright) {
            reach = UC_FIGURE_W / 2.0 + 3.0;
            middle = (UC_FIGURE_H - view.h) / 2.0;
        } else {
            reach += 2.0;
        }
    } else if (view.kind == ViewKind::Ellipse) {
        double span = (upright ? view.h : view.w) / 2.0;
        double ratio = span == 0.0 ? 0.0 : (along / span);
        reach *= std::sqrt(std::max(1.0 - ratio * ratio, 0.0));
    }
    return upright ? std::make_pair(c.x + sign * reach, c.y + middle + along)
                   : std::make_pair(c.x + along, c.y + sign * reach);
}

inline double ucSideLength(const View& view, int side) noexcept {
    bool upright = (side == 1 || side == 3);
    if (view.kind == ViewKind::Actor) return upright ? UC_FIGURE_H * 0.5 : UC_FIGURE_W * 0.6;
    double length = upright ? view.h : view.w;
    return view.kind == ViewKind::Ellipse ? length * 0.6 : length - 16.0;
}

struct UcEnd {
    std::vector<double>* route = nullptr;
    size_t at = 0;
    double toward = 0.0;
};

inline void ucAttachEnds(std::vector<Flowchart::CEdge>& cedges, const std::vector<Flowchart::CNode>& cnodes, const std::vector<View>& views) noexcept {
    std::vector<int> sideKeys;
    std::unordered_map<int, std::vector<UcEnd>> sides;
    auto add = [&](int index, std::vector<double>& route, size_t at, size_t next) {
        const auto& c = cnodes[index];
        if (c.isGroup) return;
        double dx = route[at] - c.x;
        double dy = route[at + 1] - c.y;
        int side = -1;
        if (std::abs(std::abs(dy) - c.h / 2.0) < 0.5) side = dy < 0 ? 0 : 2;
        else if (std::abs(std::abs(dx) - c.w / 2.0) < 0.5) side = dx < 0 ? 3 : 1;
        else return;
        int key = index * 4 + side;
        UcEnd end{&route, at, route[next + ((side & 1) ? 1 : 0)]};
        if (!sides.count(key)) sideKeys.push_back(key);
        sides[key].push_back(end);
    };
    for (auto& e : cedges) {
        if (e.src == e.dst || e.route.size() < 6) continue;
        size_t last = e.route.size() - 3;
        add(e.src, e.route, 0, 3);
        add(e.dst, e.route, last, last - 3);
    }
    for (int key : sideKeys) {
        auto& ends = sides[key];
        int index = key >> 2;
        int side = key & 3;
        const auto& view = views[index];
        double step = ends.size() > 1 ? std::min(UC_PORT_GAP, ucSideLength(view, side) / static_cast<double>(ends.size() - 1)) : 0.0;
        std::stable_sort(ends.begin(), ends.end(), [](const UcEnd& a, const UcEnd& b) {
            return a.toward < b.toward;
        });
        for (size_t i = 0; i < ends.size(); i++) {
            auto [x, y] = ucAnchor(view, cnodes[index], side, (static_cast<double>(i) - static_cast<double>(ends.size() - 1) / 2.0) * step);
            (*ends[i].route)[ends[i].at] = x;
            (*ends[i].route)[ends[i].at + 1] = y;
        }
    }
}

inline std::string ucNum(double value) noexcept {
    return num(Flowchart::jsRound(value * 100.0) / 100.0);
}

inline std::string ucExtension(double x, double y, double dx, double dy, const std::string& color) noexcept {
    double bx = x - dx * UC_EXTENSION;
    double by = y - dy * UC_EXTENSION;
    double half = UC_EXTENSION * 0.5;
    return Pele::cat({"<path class=\"pele-marker\" d=\"M", ucNum(x), ",", ucNum(y), "L", ucNum(bx - dy * half), ",", ucNum(by + dx * half), "L", ucNum(bx + dy * half), ",", ucNum(by - dx * half), "Z\" fill=\"var(--_bg)\" stroke=\"", color, "\" stroke-linejoin=\"round\"/>"});
}

inline double ucTrim(const std::string& type) noexcept {
    return type == "extension" ? UC_EXTENSION : markerTrim(type);
}

inline std::string ucMark(const std::string& type, double x, double y, double dx, double dy, const std::string& color, const std::string& stroke = "none") noexcept {
    return type == "extension" ? ucExtension(x, y, dx, dy, color) : marker(type, x, y, dx, dy, color, stroke);
}

inline std::string ucFigure(const View& view, const IconResolver& icons) noexcept {
    const auto& node = view.node;
    const auto& shape = view.style.shape;
    std::string line = Pele::cat({" fill=\"none\" stroke=\"var(--_b)\"", view.style.line});
    double head = -21.0;
    double radius = 9.0;
    std::string out;
    if (node.actorType == "hollow") {
        radius = 8.0;
        out = Pele::cat({"<circle cy=\"-21\" r=\"8\" fill=\"var(--_bg)\" stroke=\"var(--_b)\"", shape, "/>"
              "<path d=\"M-20,-9H20V0H5.5L20,15.5L12,25.5L0,12L-12,25.5L-20,15.5L-5.5,0H-20Z\" fill=\"var(--_bg)\" stroke=\"var(--_b)\" stroke-linejoin=\"round\"", shape, "/>"});
    } else if (node.actorType == "awesome") {
        out = Pele::cat({"<path d=\"M0,-30A11,11 0 1 1 0,-8A11,11 0 1 1 0,-30ZM-20,25C-20,9 -12,-3 0,-3C12,-3 20,9 20,25C20,28 18,30 15,30H-15C-18,30 -20,28 -20,25Z\" fill=\"var(--_b)\"", shape, "/>"});
    } else if (node.actorType == "icon") {
        head = -14.0;
        radius = 12.0;
        out = Pele::cat({"<rect x=\"-22\" y=\"-26\" width=\"44\" height=\"44\"", rounded(UC_SHAPE_ATTRS + shape), "/>"
              "<svg class=\"pele-icon\" data-icon=\"", escText(node.icon), "\" x=\"-14\" y=\"-18\" width=\"28\" height=\"28\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\">", (icons ? icons(node.icon) : ""), "</svg>"});
    } else {
        out = Pele::cat({"<circle cy=\"-21\" r=\"9\"", UC_SHAPE_ATTRS, shape, "/><path d=\"M0,-12V10M-14,-3H14M0,10L-12,30M0,10L12,30\"", line, "/>"});
    }
    if (node.business) {
        Pele::appendCat(out, {"<path class=\"pele-business\" d=\"M", num(0.12 * radius), ",", num(head + 0.99 * radius), "L", num(0.92 * radius), ",", num(head - 0.39 * radius), "\"", line, "/>"});
    }
    return out;
}

inline double ucTitleX(const Flowchart::CNode& c, double width, const Flowchart::Crossings& crossings) noexcept {
    auto it = crossings.find(static_cast<int>(Flowchart::jsRound(c.y - c.h / 2.0)));
    if (width == 0.0 || it == crossings.end()) return c.x;
    double left = c.x - c.w / 2.0 + 6.0;
    double right = c.x + c.w / 2.0 - 6.0;
    std::vector<double> cuts;
    for (double x : it->second) {
        if (x > left && x < right) cuts.push_back(x);
    }
    std::sort(cuts.begin(), cuts.end());
    cuts.push_back(right);
    double need = width / 2.0 + 6.0;
    double best = c.x;
    double bestOff = std::numeric_limits<double>::infinity();
    double widest = 0.0;
    double from = left;
    for (double cut : cuts) {
        if (cut - from >= 2.0 * need) {
            double x = std::min(std::max(c.x, from + need), cut - need);
            if (std::abs(x - c.x) < bestOff) {
                bestOff = std::abs(x - c.x);
                best = x;
            }
        } else if (std::isinf(bestOff) && cut - from > widest) {
            widest = cut - from;
            best = (from + cut) / 2.0;
        }
        from = cut;
    }
    return best;
}

inline std::string ucLeftLabel(const Label& label, double x, double cy, const std::string& attrs, const IconResolver& icons) noexcept {
    if (label.lines.size() < 2) return labelSvg(label, x + label.width / 2.0, cy, attrs, icons);
    std::string out;
    for (size_t i = 0; i < label.lines.size(); i++) {
        out += labelSvg(labelLine(label, i), x + label.widths[i] / 2.0, cy - label.height / 2.0 + lineMiddle(label, i), attrs, icons);
    }
    return out;
}

inline Rendered drawUsecase(const UsecaseModel& model, const Config& config, const RenderOptions& options, bool turned) noexcept {
    UsecaseGraph graph = buildUsecaseGraph(model);
    double size = options.fontSize > 0 ? options.fontSize : 16.0;
    double small = std::round(size * 0.875);
    double tiny = std::round(size * 0.8125);
    const TextMeasurer& measurer = getMeasurer(options);
    double pad = options.hasPadding ? options.padding : 8.0;
    const auto& icons = options.icons;
    std::string rootDir = turned ? "TB" : Flowchart::direction(model.direction);
    bool sideways = (rootDir == "LR" || rootDir == "RL");

    Pele::FlatMap<std::string, int> index;
    std::vector<View> views;
    std::vector<Flowchart::CNode> cnodes;
    for (size_t i = 0; i < graph.nodes.size(); i++) {
        index[graph.nodes[i].id] = static_cast<int>(i);
    }

    for (const auto& node : graph.nodes) {
        std::vector<std::string> styleDecls = node.cssCompiledStyles;
        if (!node.cssStyles.empty()) {
            styleDecls.insert(styleDecls.end(), node.cssStyles.begin(), node.cssStyles.end());
        }
        ResolvedStyle style = resolveStyle(styleDecls);
        int base = (style.bold ? Style::Bold : 0) | (style.italic ? Style::Italic : 0);
        double fontSize = style.hasFontSize ? style.fontSize : size;
        bool markdown = (node.labelType == "markdown");
        Label stereotype = ucText(!node.stereotype.empty() ? Pele::cat({"\xC2\xAB", node.stereotype, "\xC2\xBB"}) : "", false, measurer, tiny, 200.0, base);
        View view;
        view.node = node;
        view.kind = ViewKind::Box;
        view.style = style;
        view.label = stereotype;
        view.stereotype = stereotype;
        Flowchart::CNode c;
        if (node.isGroup) {
            view.kind = ViewKind::Boundary;
            view.label = ucText(node.label, markdown, measurer, fontSize, 4000.0, base);
            double titleH = view.label.height;
            c = Flowchart::cnode(0.0, 0.0);
            c.isGroup = true;
            c.padX = UC_GROUP_PAD;
            c.padBottom = UC_GROUP_PAD;
            c.padTop = (node.boundaryType == "package") ? (titleH + 2.0 * UC_TAB_PAD + 16.0) : (titleH > 0 ? titleH + 16.0 : UC_GROUP_PAD);
            c.minW = view.label.width + 2.0 * UC_GROUP_PAD;
        } else {
            if (node.hasJsonRows) {
                view.kind = ViewKind::Json;
                view.label = ucText(node.label, false, measurer, fontSize, 4000.0, base | Style::Bold);
                double valueW = 0.0;
                for (const auto& row : node.jsonRows) {
                    Label k = ucText(row.key, false, measurer, small, 4000.0, base);
                    Label v = ucText(row.value, false, measurer, small, 260.0, base);
                    view.keyW = std::max(view.keyW, k.width);
                    valueW = std::max(valueW, v.width);
                    view.h += std::max({k.height, v.height, std::round(small * 1.5)}) + UC_ROW_PAD;
                    view.rows.push_back({std::move(k), std::move(v)});
                }
                if (view.keyW > 0.0) view.keyW += 2.0 * UC_CELL_PAD;
                view.headH = std::max(view.label.height, std::round(fontSize * 1.5)) + UC_ROW_PAD;
                view.w = std::max({view.label.width + 2.0 * UC_CELL_PAD, !view.rows.empty() ? (view.keyW + valueW + 2.0 * UC_CELL_PAD) : 0.0, 72.0});
                view.h += view.headH;
            } else if (!node.actorType.empty()) {
                view.kind = ViewKind::Actor;
                view.label = ucText(node.label, markdown, measurer, fontSize, 140.0, base);
                view.w = std::max({UC_FIGURE_W, view.label.width, stereotype.width});
                view.h = UC_FIGURE_H + UC_FIGURE_GAP + stereotype.height + view.label.height;
            } else {
                view.label = ucText(node.label, markdown, measurer, node.shape == "note" ? small : fontSize, 180.0, base);
                double tw = std::max(view.label.width, stereotype.width);
                double th = view.label.height + stereotype.height;
                if (node.shape == "usecaseEllipse" || node.shape == "usecaseBusiness") {
                    view.kind = ViewKind::Ellipse;
                    view.w = std::max(tw + 2.0 * node.padding + (node.business ? 16.0 : 4.0), 88.0);
                    view.h = std::max(th + 28.0, 48.0);
                } else if (node.shape == "note") {
                    view.w = tw + 2.0 * node.padding + 6.0;
                    view.h = th + 18.0;
                } else {
                    view.w = std::max(tw + 2.0 * node.padding + 12.0, 80.0);
                    view.h = std::max(th + 24.0, 44.0);
                }
            }
            c = Flowchart::cnode(view.w, view.h);
        }
        c.seq = static_cast<int>(views.size());
        views.push_back(std::move(view));
        cnodes.push_back(std::move(c));
    }

    for (size_t i = 0; i < graph.nodes.size(); i++) {
        if (!graph.nodes[i].parentId.empty() && index.count(graph.nodes[i].parentId)) {
            cnodes[i].parent = index[graph.nodes[i].parentId];
        } else {
            cnodes[i].parent = -1;
        }
    }

    std::vector<Flowchart::CEdge> cedges;
    std::vector<Label> edgeLabels;
    std::vector<UsecaseGraphEdge> drawn;
    for (const auto& edge : graph.edges) {
        auto sIt = index.find(edge.start);
        auto dIt = index.find(edge.end);
        if (sIt == index.end() || dIt == index.end()) continue;
        int src = sIt->second;
        int dst = dIt->second;
        bool semantic = (edge.relationshipType == "include" || edge.relationshipType == "extend");
        std::string rawLbl = semantic ? Pele::cat({"\xC2\xAB", edge.label, "\xC2\xBB"}) : (edge.hasLabel ? edge.label : "");
        Label label = ucText(rawLbl, edge.labelType == "markdown", measurer, small, 200.0);
        if (src == dst) {
            views[src].loops++;
            if (sideways) cnodes[src].h += 2.0 * (UC_LOOP + (label.height > 0 ? label.height + 8.0 : 0.0));
            else cnodes[src].w += 2.0 * (UC_LOOP + (label.width > 0 ? label.width + 12.0 : 0.0));
        }
        drawn.push_back(edge);
        edgeLabels.push_back(label);
        Flowchart::CEdge ce;
        ce.src = src;
        ce.dst = dst;
        ce.minlen = edge.minlen;
        ce.labelW = label.width > 0 ? label.width + 12.0 : 0.0;
        ce.labelH = label.height > 0 ? label.height + 4.0 : 0.0;
        cedges.push_back(std::move(ce));
    }

    Flowchart::LayeredOptions cOpts;
    cOpts.nodeSep = ucNumberOption(config, "nodeSpacing", UC_NODE_SEP);
    cOpts.edgeSep = UC_EDGE_SEP;
    cOpts.rankSep = ucNumberOption(config, "rankSpacing", UC_RANK_SEP);
    cOpts.portSep = 20.0;
    auto layout = Flowchart::compoundLayout(cnodes, cedges, rootDir, cOpts);

    Label title = titleLabel(model.title, measurer, size, 4000.0);
    double titleHeight = title.height > 0 ? title.height + 12.0 : 0.0;
    double inner = std::max(layout.width, title.width);
    double ox = pad + (inner - layout.width) / 2.0;
    double oy = pad + titleHeight;
    Flowchart::shiftLayout(cnodes, cedges, ox, oy);
    double width = std::ceil(inner + 2.0 * pad);
    double height = std::ceil(layout.height + titleHeight + 2.0 * pad);

    Flowchart::Crossings crossings;
    for (const auto& e : cedges) Flowchart::markCrossings(crossings, e.route);
    ucAttachEnds(cedges, cnodes, views);

    std::string clusters;
    std::vector<Fill> fills;
    std::string nodesOut;
    for (size_t i = 0; i < views.size(); i++) {
        const auto& view = views[i];
        const auto& c = cnodes[i];
        const auto& node = view.node;
        std::string strippedClasses = node.cssClasses;
        if (startsWith(strippedClasses, "default ")) strippedClasses = strippedClasses.substr(8);
        else if (strippedClasses == "default") strippedClasses.clear();
        std::string classes = classNames(strippedClasses);
        std::string id = escText(node.id);
        const std::string& textAttrs = view.style.text;
        const std::string& shape = view.style.shape;
        std::string color = inkFill(view.style);
        std::string muted = !color.empty() ? color : " fill=\"var(--_m)\"";
        double w = view.w;
        double h = view.h;

        if (view.kind == ViewKind::Boundary) {
            double x = c.x - c.w / 2.0;
            double y = c.y - c.h / 2.0;
            if (paints(shape)) fills.push_back({{x, y, x + c.w, y + c.h}, shape});
            std::string box = " fill=\"var(--_a)\"" + std::string(setsFill(shape) ? "" : " fill-opacity=\"0.5\"") + " stroke=\"var(--_b)\"" + shape;
            std::string titleAttrs = Pele::cat({" class=\"pele-cluster-label\"", muted, textAttrs});
            std::string body;
            if (node.boundaryType == "package") {
                double tabW = std::min(view.label.width + 2.0 * UC_TAB_PAD + 8.0, c.w);
                double tabH = view.label.height + 2.0 * UC_TAB_PAD;
                body = Pele::cat({"<rect class=\"pele-boundary-tab\" x=\"", num(x), "\" y=\"", num(y), "\" width=\"", num(tabW), "\" height=\"", num(tabH), "\"", box, "/>"
                       "<rect x=\"", num(x), "\" y=\"", num(y + tabH), "\" width=\"", num(c.w), "\" height=\"", num(c.h - tabH), "\"", box, "/>", labelSvg(view.label, x + tabW / 2.0, y + tabH / 2.0, titleAttrs, icons)});
            } else {
                body = Pele::cat({"<rect x=\"", num(x), "\" y=\"", num(y), "\" width=\"", num(c.w), "\" height=\"", num(c.h), "\"", rounded(box), "/>", labelSvg(view.label, ucTitleX(c, view.label.width, crossings), y + 8.0 + view.label.height / 2.0, titleAttrs, icons)});
            }
            Pele::appendCat(clusters, {"<g class=\"pele-cluster pele-boundary", classes, "\" data-id=\"", id, "\">", body, "</g>"});
            continue;
        }

        std::string body;
        std::string kindStr;
        if (view.kind == ViewKind::Actor) {
            kindStr = "pele-actor";
            double top = -h / 2.0;
            double stereotypeY = top + UC_FIGURE_H + UC_FIGURE_GAP + view.stereotype.height / 2.0;
            body = Pele::cat({"<g class=\"pele-actor-figure\" transform=\"translate(0,", num(top + UC_FIGURE_H / 2.0), ")\">", ucFigure(view, icons), "</g>", labelSvg(view.stereotype,  0.0,  stereotypeY, Pele::cat({" class=\"pele-stereotype\" font-size=\"", num(tiny), "\" fill=\"var(--_m)\""}),  icons), labelSvg(view.label,  0.0,  h / 2.0 - view.label.height / 2.0, Pele::cat({" class=\"pele-label\"", textAttrs}),  icons)});
        } else if (view.kind == ViewKind::Json) {
            kindStr = "pele-json";
            double x0 = Flowchart::jsRound((-w / 2.0) * 100.0) / 100.0;
            double y = -h / 2.0 + view.headH;
            std::string cells;
            std::string lines = Pele::cat({"M", ucNum(x0), ",", ucNum(y), "H", ucNum(-w / 2.0 + w)});
            if (view.keyW > 0.0) Pele::appendCat(lines, {"M", ucNum(-w / 2.0 + view.keyW), ",", ucNum(y), "V", ucNum(h / 2.0)});
            for (const auto& [key, val] : view.rows) {
                double rowH = std::max({key.height, val.height, std::round(small * 1.5)}) + UC_ROW_PAD;
                cells += labelSvg(key,  -w / 2.0 + UC_CELL_PAD + key.width / 2.0,  y + rowH / 2.0, Pele::cat({" class=\"pele-json-key\"", muted}),  icons) +
                         ucLeftLabel(val,  -w / 2.0 + view.keyW + UC_CELL_PAD,  y + rowH / 2.0, Pele::cat({" class=\"pele-json-value\"", color, textAttrs}),  icons);
                y += rowH;
            }
            body = Pele::cat({"<rect x=\"", ucNum(x0), "\" y=\"", ucNum(-h / 2.0), "\" width=\"", ucNum(w), "\" height=\"", ucNum(h), "\"", rounded(UC_SHAPE_ATTRS + shape), "/>"
                   "<path class=\"pele-json-lines\" d=\"", lines, "\" fill=\"none\" stroke=\"var(--_b)\"", view.style.line, "/>", labelSvg(view.label,  0.0,  -h / 2.0 + view.headH / 2.0, Pele::cat({" class=\"pele-label\" font-weight=\"var(--_hw)\"", color, textAttrs}),  icons), "<g font-size=\"", ucNum(small), "\">", cells, "</g>"});
        } else {
            double labelY = view.stereotype.height / 2.0;
            std::string outline;
            if (view.kind == ViewKind::Ellipse) {
                kindStr = "pele-usecase";
                outline = Pele::cat({"<ellipse rx=\"", ucNum(w / 2.0), "\" ry=\"", ucNum(h / 2.0), "\"", UC_SHAPE_ATTRS, shape, "/>"});
                if (node.business) {
                    double from = w / 2.0 - 15.0;
                    double to = w / 2.0 - 5.0;
                    auto arc = [&](double at) {
                        double r = at / (w / 2.0);
                        return (h / 2.0) * std::sqrt(std::max(0.0, 1.0 - r * r));
                    };
                    Pele::appendCat(outline, {"<path class=\"pele-business\" d=\"M", ucNum(from), ",", ucNum(arc(from)), "L", ucNum(to), ",", ucNum(-arc(to)), "\" fill=\"none\" stroke=\"var(--_b)\"", view.style.line, "/>"});
                }
            } else if (node.shape == "note") {
                kindStr = "pele-note";
                double r = w / 2.0;
                double b = h / 2.0;
                outline = Pele::cat({"<path d=\"M", ucNum(-r), ",", ucNum(-b), "H", ucNum(r - UC_FOLD), "L", ucNum(r), ",", ucNum(UC_FOLD - b), "V", ucNum(b), "H", ucNum(-r), "Z\" fill=\"var(--_a)\" stroke=\"var(--_b)\" stroke-linejoin=\"round\"", shape, "/>"
                          "<path d=\"M", ucNum(r - UC_FOLD), ",", ucNum(-b), "V", ucNum(UC_FOLD - b), "H", ucNum(r), "\" fill=\"none\" stroke=\"var(--_b)\" stroke-linejoin=\"round\"", view.style.line, "/>"});
            } else {
                kindStr = "pele-usecase";
                outline = Pele::cat({"<rect x=\"", ucNum(-w / 2.0), "\" y=\"", ucNum(-h / 2.0), "\" width=\"", ucNum(w), "\" height=\"", ucNum(h), "\"", rounded(UC_SHAPE_ATTRS + shape), "/>"});
            }
            body = outline +
                   labelSvg(view.stereotype,  0.0,  -view.label.height / 2.0, Pele::cat({" class=\"pele-stereotype\" font-size=\"", num(tiny), "\"", muted}),  icons) +
                   labelSvg(view.label, 0.0, labelY, " class=\"pele-label\"" + std::string(node.shape == "note" ? Pele::cat({" font-size=\"", num(small), "\""}) : "") + color + textAttrs, icons);
        }
        Pele::appendCat(nodesOut, {"<g class=\"pele-node ", kindStr, classes, "\" data-id=\"", id, "\" transform=\"translate(", num(c.x), ",", num(c.y), ")\">", body, "</g>"});
    }

    std::string casings;
    std::string edgesOut;
    std::string labelsOut;
    Pele::FlatMap<int, int> loopSeen;
    for (size_t i = 0; i < cedges.size(); i++) {
        auto& ce = cedges[i];
        const auto& edge = drawn[i];
        const auto& label = edgeLabels[i];
        bool hasStyle = !edge.style.empty() || !edge.cssCompiledStyles.empty();
        ResolvedStyle style;
        if (hasStyle) {
            std::vector<std::string> combined = edge.cssCompiledStyles;
            combined.insert(combined.end(), edge.style.begin(), edge.style.end());
            style = resolveStyle(combined);
        }
        const std::string& startType = edge.arrowTypeStart;
        const std::string& endType = edge.arrowTypeEnd;
        EdgePath path;
        if (ce.src == ce.dst) {
            const auto& c = cnodes[ce.src];
            const auto& view = views[ce.src];
            int k = loopSeen[ce.src]++;
            int side = sideways ? 2 : 1;
            double spread = std::min(ucSideLength(view, side) / 2.0, 8.0 + k * 6.0);
            double reach = UC_LOOP + k * 8.0;
            auto [ax, ay] = ucAnchor(view, c, side, -spread);
            auto [bx, by] = ucAnchor(view, c, side, spread);
            double len = std::hypot(reach, spread);
            if (len == 0.0) len = 1.0;
            double ux = sideways ? (spread / len) : (reach / len);
            double uy = sideways ? (reach / len) : (spread / len);
            double st = ucTrim(startType);
            double et = ucTrim(endType);
            auto outPt = [&](double x, double y, double sign, double far) {
                return sideways
                    ? Pele::cat({num(x + sign * spread * (far != 0.0 ? 1.0 : 0.0)), ",", num(y + far)})
                    : Pele::cat({num(x + far), ",", num(y + sign * spread * (far != 0.0 ? 1.0 : 0.0))});
            };
            double sx = sideways ? (ax - ux * st) : (ax + ux * st);
            double sy = sideways ? (ay + uy * st) : (ay - uy * st);
            double ex = bx + ux * et;
            double ey = by + uy * et;
            path.d = Pele::cat({"M", num(sx), ",", num(sy), "C", outPt(ax, ay, -1.0, reach), " ", outPt(bx, by, 1.0, reach), " ", num(ex), ",", num(ey)});
            path.sx = ax;
            path.sy = ay;
            path.sdx = sideways ? ux : -ux;
            path.sdy = sideways ? -uy : uy;
            path.ex = bx;
            path.ey = by;
            path.edx = -ux;
            path.edy = -uy;
            if (sideways) {
                ce.labelX = c.x;
                ce.labelY = std::max(ay, by) + reach * 0.75 + 6.0 + label.height / 2.0;
            } else {
                ce.labelX = std::max(ax, bx) + reach * 0.75 + 8.0 + label.width / 2.0;
                ce.labelY = c.y;
            }
        } else {
            if (ce.route.size() < 6) continue;
            path = routePath(ce.route, "", ucTrim(startType), ucTrim(endType));
        }

        FillLookup under = fillUnder(path.d, {path.sx, path.sy, path.ex, path.ey}, fills, path.hasTokens ? &path.tokens : nullptr);
        bool hasOver = (under.state != FillLookup::None);
        std::string over = (under.state == FillLookup::Found) ? edgeInk(under.fill, hasStyle ? style.line : "") : "";
        std::string color = !over.empty() ? over : ((hasStyle && style.hasStroke) ? esc(style.stroke) : "var(--_l)");
        if (hasOver && over.empty()) {
            std::string bg = "var(--_bg)";
            casings += casing(path.d, ucMark(startType, path.sx, path.sy, path.sdx, path.sdy, bg, bg) + ucMark(endType, path.ex, path.ey, path.edx, path.edy, bg, bg), hasStyle ? style.line : "");
        }
        std::string dashes = edge.internal ? "2 3" : (edge.pattern == "dotted" ? "5 4" : (edge.animate ? "9 5" : ""));
        std::string marks = ucMark(startType, path.sx, path.sy, path.sdx, path.sdy, color) + ucMark(endType, path.ex, path.ey, path.edx, path.edy, color);

        // Strip /^default relationship relationship-\S+\s?/
        std::string edgeCls = edge.classes;
        std::string prefix = Pele::cat({"default relationship relationship-", edge.relationshipType});
        if (startsWith(edgeCls, Pele::cat({prefix, " "}))) edgeCls = edgeCls.substr(prefix.size() + 1);
        else if (edgeCls == prefix) edgeCls.clear();

        Pele::appendCat(edgesOut, {"<g class=\"pele-edge pele-", edge.relationshipType, classNames(edgeCls), "\" data-id=\"", escText(edge.id), "\"", (!over.empty() ? Pele::cat({" stroke=\"", over, "\""}) : ""), (hasStyle ? style.line : ""), ">", "<path d=\"", path.d, "\"", (!dashes.empty() ? Pele::cat({" stroke-dasharray=\"", dashes, "\""}) : ""), "/>", ((under.state == FillLookup::Found && !over.empty()) ? hollowOn(marks, under.fill) : marks), "</g>"});
        if (label.width > 0.0) {
            bool semantic = (edge.relationshipType == "include" || edge.relationshipType == "extend");
            labelsOut += edgeLabelSvg(edge.id, label, ce.labelX, ce.labelY, std::string(semantic ? " fill=\"var(--_m)\"" : "") + (hasStyle ? style.text : ""), icons);
        }
    }

    DiagramMeta meta{model.title, model.accTitle, model.accDescr};
    std::string svg = svgDocument(
        "usecase", 
        width, 
        height, 
        size, 
        options, 
        meta, Pele::cat({labelSvg(title, width / 2.0, pad + title.height / 2.0, " class=\"pele-title\" font-weight=\"var(--_tw)\""), (!clusters.empty() ? Pele::cat({"<g class=\"pele-clusters\">", clusters, "</g>"}) : ""), casingsSvg(casings), (!edgesOut.empty() ? Pele::cat({"<g class=\"pele-edges\" fill=\"none\" stroke=\"var(--_l)\" stroke-linecap=\"round\">", edgesOut, "</g>"}) : ""), (!labelsOut.empty() ? Pele::cat({"<g class=\"pele-edge-labels\" font-size=\"", num(small), "\">", labelsOut, "</g>"}) : ""), "<g class=\"pele-nodes\">", nodesOut, "</g>"}));

    return {svg, width, height, {}};
}

inline Rendered renderUsecaseModel(const UsecaseModel& model, const Config& config, const RenderOptions& options) noexcept {
    if (!Flowchart::runsAcross(model.direction)) return drawUsecase(model, config, options, false);
    return Flowchart::turnToFit(options, [&](bool down) { return drawUsecase(model, config, options, down); });
}

} // namespace detail

inline Rendered render(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options = {}) {
    auto model = detail::parseUsecase(source, title);

    RenderOptions eff = options;
    if (!eff.hasResponsive) {
        const YamlValue* sec = config.isMap() ? config.get("usecase") : nullptr;
        bool fixed = (sec && sec->isMap() && sec->get("useMaxWidth") && sec->get("useMaxWidth")->isBool() && !sec->get("useMaxWidth")->bVal);
        eff.responsive = !fixed;
        eff.hasResponsive = true;
    }
    IconResolver wrappedIcons;
    if (eff.icons) {
        auto orig = eff.icons;
        wrappedIcons = [orig](const std::string& name) { return orig(decodeEntities(name)); };
        eff.icons = wrappedIcons;
    }
    return withMath(eff.math, [&]() {
        return withIcons(eff.icons, [&]() {
            return detail::renderUsecaseModel(model, config, eff);
        });
    });
}

inline Rendered render(const std::string& source, const Config& config, const RenderOptions& options, const std::string& title = "") {
    return render(source, config, title, options);
}

inline Rendered render(const std::string& source, const RenderOptions& options = {}) {
    auto pre = preprocess(source);
    std::string parsed = Pele::cat({encodeEntities(pre.text), "\n"});
    return render(parsed, pre.config, pre.title, options);
}

} // namespace Usecase

inline Rendered renderUsecase(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options = {}) {
    return Usecase::render(source, config, title, options);
}

inline Rendered renderUsecase(const std::string& source, const Config& config, const RenderOptions& options, const std::string& title = "") {
    return Usecase::render(source, config, title, options);
}

inline Rendered renderUsecase(const std::string& source, const RenderOptions& options = {}) {
    return Usecase::render(source, options);
}

} // namespace Pele
