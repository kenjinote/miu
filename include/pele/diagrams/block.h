#pragma once
#include "../foundation.h"

namespace Pele {
namespace BlockDiagram {

// ============================================================================
// src/diagrams/block/types.ts
// ============================================================================

struct Block {
    std::string id;
    bool hasId = false;
    std::string type;
    std::string label;
    bool hasLabel = false;
    std::vector<Block> children;
    int columns = -1;
    bool hasColumns = false;
    int widthInColumns = 1;
    bool hasWidthInColumns = false;
    int width = 1;
    bool hasWidth = false;
    std::vector<std::string> directions;
    std::vector<std::string> classes;
    std::vector<std::string> styles;
    int colorIndex = 0;
    std::string start;
    std::string end;
    std::string arrowTypeStart;
    std::string arrowTypeEnd;
    std::string thickness;
    std::string pattern;
    std::string css;
    std::string styleClass;
    std::string stylesStr;
};

struct Statement {
    bool isList = false;
    Block single;
    std::vector<Block> list;
};

struct BlockClassDef {
    std::string id;
    std::vector<std::string> styles;
    std::vector<std::string> textStyles;
};

struct BlockModel : public DiagramMeta {
    Block root;
    std::vector<Block> blocks;
    std::vector<Block> edges;
    std::unordered_map<std::string, BlockClassDef> classes;
    std::unordered_map<std::string, Block*> byId;
};

// ============================================================================
// src/diagrams/block/db.ts
// ============================================================================

inline std::string typeStr2Type(const std::string& typeStr) noexcept {
    if (typeStr == "[]") return "square";
    if (typeStr == "()") return "round";
    if (typeStr == "(())") return "circle";
    if (typeStr == ">]") return "rect_left_inv_arrow";
    if (typeStr == "{}") return "diamond";
    if (typeStr == "{{}}") return "hexagon";
    if (typeStr == "([])") return "stadium";
    if (typeStr == "[[]]") return "subroutine";
    if (typeStr == "[()]") return "cylinder";
    if (typeStr == "((()))") return "doublecircle";
    if (typeStr == "[//]") return "lean_right";
    if (typeStr == "[\\\\]") return "lean_left";
    if (typeStr == "[/\\]") return "trapezoid";
    if (typeStr == "[\\/]") return "inv_trapezoid";
    if (typeStr == "<[]>") return "block_arrow";
    return "na";
}

inline std::string edgeStrToEdgeData(const std::string& typeStr) noexcept {
    std::string t = trimStr(typeStr);
    if (t.empty()) return "";
    switch (t.back()) {
        case 'x': return "arrow_cross";
        case 'o': return "arrow_circle";
        case '>': return "arrow_point";
        default: return "";
    }
}

inline std::string edgeStrToEdgeStartData(const std::string& typeStr) noexcept {
    std::string t = trimStr(typeStr);
    if (t.empty()) return "arrow_open";
    switch (t.front()) {
        case 'x': return "arrow_cross";
        case 'o': return "arrow_circle";
        case '<': return "arrow_point";
        default: return "arrow_open";
    }
}

inline std::string edgeStrToThickness(const std::string& typeStr) noexcept {
    return typeStr.find("==") != std::string::npos ? "thick" : "normal";
}

inline std::string edgeStrToPattern(const std::string& typeStr) noexcept {
    return typeStr.find(".-") != std::string::npos ? "dotted" : "solid";
}

class BlockDb : public BlockModel {
public:
    int ids = 0;
    int colors = 0;
    Pele::FlatSet<std::string> pending;
    Pele::FlatMap<std::string, int> edgeCount;
    std::unordered_map<std::string, Block> namedStore;

    BlockDb() {
        root.id = "root";
        root.hasId = true;
        root.type = "composite";
        root.columns = -1;
        root.hasColumns = true;
    }

    std::string generateId() {
        return "id-" + std::to_string(++ids);
    }

    void addStyleClass(const std::string& id, const std::string& styleAttributes = "") {
        auto& found = classes[id];
        found.id = id;
        for (const auto& attribute : splitStr(styleAttributes, ',')) {
            size_t semi = attribute.find(';');
            std::string fixed = trimStr(semi == std::string::npos ? attribute : (attribute.substr(0, semi) + attribute.substr(semi + 1)));
            if (attribute.find("color") != std::string::npos) {
                found.textStyles.push_back(replaceAllStr(replaceAllStr(fixed, "fill", "bgFill"), "color", "fill"));
            }
            found.styles.push_back(fixed);
        }
    }

    Block& named(const std::string& id) {
        auto it = namedStore.find(id);
        if (it == namedStore.end()) {
            Block b;
            b.id = id;
            b.hasId = true;
            b.type = "na";
            namedStore[id] = b;
            pending.insert(id);
        }
        return namedStore[id];
    }

    void addStyle2Node(const std::string& id, const std::string& styles = "") {
        named(id).styles = splitStr(styles, ',');
    }

    void setCssClass(const std::string& itemIds, const std::string& cssClassName) {
        for (const auto& rawId : splitStr(itemIds, ',')) {
            named(trimStr(rawId)).classes.push_back(cssClassName);
        }
    }

    static std::vector<Block> flattenStatements(const std::vector<Statement>& stmts) {
        std::vector<Block> out;
        for (const auto& s : stmts) {
            if (s.isList) {
                for (const auto& b : s.list) out.push_back(b);
            } else {
                out.push_back(s.single);
            }
        }
        return out;
    }

    void buildBlockList(std::vector<Block>& list, Block& parent) {
        std::vector<Block> children;
        for (size_t at = 0; at < list.size(); at++) {
            Block block = list[at];
            if (block.type == "classDef") {
                addStyleClass(block.id, block.css);
                continue;
            }
            if (block.type == "applyClass") {
                setCssClass(block.id, block.styleClass);
                continue;
            }
            if (block.type == "applyStyles") {
                if (!block.stylesStr.empty()) addStyle2Node(block.id, block.stylesStr);
                continue;
            }
            if (block.type == "column-setting") {
                parent.columns = block.hasColumns ? block.columns : -1;
                parent.hasColumns = true;
                continue;
            }
            if (block.type == "edge") {
                int count = ++edgeCount[block.id];
                block.id = std::to_string(count) + "-" + block.id;
                edges.push_back(block);
                continue;
            }

            if (!block.hasId || block.id.empty()) {
                block.id = generateId();
                block.hasId = true;
            }
            if (!block.hasLabel || block.label.empty()) {
                block.label = (block.type == "composite") ? "" : block.id;
                block.hasLabel = true;
            }

            auto knownIt = namedStore.find(block.id);
            bool fresh = (knownIt == namedStore.end()) || (pending.erase(block.id) > 0);
            if (!fresh) {
                if (block.type != "na") knownIt->second.type = block.type;
                if (block.label != block.id) {
                    knownIt->second.label = block.label;
                    knownIt->second.hasLabel = true;
                }
                // Also update existing child in `children` if in same container, or recursively in tree
                auto updateExisting = [&](auto& self, std::vector<Block>& vec) -> void {
                    for (auto& existing : vec) {
                        if (existing.id == block.id) {
                            if (block.type != "na") existing.type = block.type;
                            if (block.label != block.id) {
                                existing.label = block.label;
                                existing.hasLabel = true;
                            }
                            if (!block.directions.empty()) existing.directions = block.directions;
                        }
                        self(self, existing.children);
                    }
                };
                updateExisting(updateExisting, children);
                updateExisting(updateExisting, root.children);
            } else {
                if (knownIt != namedStore.end()) {
                    block.classes = knownIt->second.classes;
                    block.styles = knownIt->second.styles;
                }
                if (block.type == "composite") block.colorIndex = colors++;
                namedStore[block.id] = block;
            }

            std::vector<Block> inner = std::move(block.children);
            block.children.clear();
            if (block.type == "space") {
                int w = block.hasWidth ? block.width : 1;
                for (int j = 0; j < w; j++) {
                    Block copy = block;
                    copy.id = block.id + "-" + std::to_string(j);
                    copy.children.clear();
                    namedStore[copy.id] = copy;
                    children.push_back(std::move(copy));
                }
            } else if (fresh) {
                if (!inner.empty()) {
                    buildBlockList(inner, block);
                }
                children.push_back(std::move(block));
            }
        }
        // Apply any late style/class declarations to blocks in `children`
        auto applyPendingStyles = [&](auto& self, std::vector<Block>& vec) -> void {
            for (auto& b : vec) {
                auto it = namedStore.find(b.id);
                if (it != namedStore.end()) {
                    if (!it->second.classes.empty()) b.classes = it->second.classes;
                    if (!it->second.styles.empty()) b.styles = it->second.styles;
                }
                self(self, b.children);
            }
        };
        applyPendingStyles(applyPendingStyles, children);
        parent.children = std::move(children);
    }

    void setHierarchy(const std::vector<Statement>& statements) {
        auto flat = flattenStatements(statements);
        buildBlockList(flat, root);
        blocks = root.children;
    }
};

// ============================================================================
// src/diagrams/block/tokens.ts & src/diagrams/block/lexer.ts
// ============================================================================

namespace T {
    enum Type {
        END, ERROR, KEY, ID_BLOCK, COLUMNS, MD_STR, STR, SPACE_BLOCK, DEFAULT, LINKSTYLE,
        INTERPOLATE, CLASSDEF, DEFAULT_CLASSDEF_ID, CLASSDEF_ID, CLASSDEF_STYLEOPTS,
        CLASS, CLASSENTITY_IDS, STYLECLASS, STYLE, STYLE_ENTITY_IDS, STYLE_DEFINITION_DATA,
        acc_title, acc_title_value, acc_descr, acc_descr_value, acc_descr_multiline_value,
        end, NODE_DSTART, NODE_DEND, BLOCK_ARROW_START, BLOCK_ARROW_END, DIR, NODE_ID,
        LINK, START_LINK, LINK_LABEL, SIZE, EOF_TOK
    };
}

struct Tokens {
    std::vector<int> types;
    std::vector<std::string> texts;
    std::vector<size_t> starts;
};

inline bool isBlockIdStop(int ch) noexcept {
    if (ch < 0) return true;
    if (ch < 128) {
        switch (ch) {
            case 9: case 10: case 11: case 12: case 13: case 32:
            case 40: case 41: case 45: case 58: case 60: case 61: case 62:
            case 91: case 123: case 125:
                return true;
            default:
                return false;
        }
    }
    return isSpace(ch);
}

inline Tokens tokenize(const std::string& src) {
    enum State {
        INITIAL, S_string, S_md_string, NODE, BLOCK_ARROW, ARROW_DIR, LLABEL,
        CLASSDEF, CLASSDEFID, CLASS, CLASS_STYLE, STYLE_STMNT, STYLE_DEFINITION,
        acc_title, acc_descr, acc_descr_multiline
    };
    static const std::vector<std::string> DIRECTIONS = {"right", "left", "x", "y", "up", "down"};

    size_t n = src.size();
    Tokens out;
    std::vector<int> stack;
    int state = INITIAL;
    size_t p = 0;

    auto charAt = [&](size_t idx) -> int {
        if (idx >= n) return -1;
        size_t dummy = idx;
        return charCodeAtUtf8(src, idx, &dummy);
    };
    auto byteAt = [&](size_t idx) -> int {
        if (idx >= n) return -1;
        return static_cast<unsigned char>(src[idx]);
    };
    auto put = [&](int type, const std::string& text, size_t end) -> bool {
        out.types.push_back(type);
        out.texts.push_back(text);
        out.starts.push_back(p);
        p = end;
        return true;
    };
    auto emit = [&](int type, size_t end) -> bool {
        return put(type, src.substr(p, end - p), end);
    };
    auto push = [&](int s) {
        stack.push_back(state);
        state = s;
    };
    auto pop = [&]() {
        if (!stack.empty()) {
            state = stack.back();
            stack.pop_back();
        }
    };
    auto ws = [&](size_t q) -> size_t {
        while (q < n) {
            size_t next = q;
            int cp = charCodeAtUtf8(src, q, &next);
            if (!isSpace(cp)) break;
            q = next;
        }
        return q;
    };
    auto digits = [&](size_t q) -> size_t {
        while (q < n && isDigit(byteAt(q))) q++;
        return q;
    };
    auto word = [&](size_t q) -> size_t {
        while (q < n && isWord(byteAt(q))) q++;
        return q;
    };
    auto keyword = [&](const std::string& text) -> bool {
        return startsWith(src, text, p) && !isWord(byteAt(p + text.size()));
    };
    auto quote = [&]() -> bool {
        if (byteAt(p + 1) == 96) {
            push(S_md_string);
            p += 2;
        } else {
            push(S_string);
            p++;
        }
        return true;
    };
    auto dotted = [&](size_t q) -> int {
        if (byteAt(q) != 46) return -1;
        do q++; while (byteAt(q) == 46);
        if (byteAt(q) != 45) return -1;
        int c = byteAt(q + 1);
        return static_cast<int>((c == 120 || c == 111 || c == 62) ? (q + 2) : (q + 1));
    };
    auto linkEnd = [&](size_t q) -> int {
        int c = byteAt(q);
        if (c == 120 || c == 111 || c == 60) q++;
        int line = byteAt(q);
        if (line == 46) return dotted(q);
        if (line != 45 && line != 61) return -1;
        size_t k = q + 1;
        while (byteAt(k) == line) k++;
        if (k - q == 1) return line == 45 ? dotted(k) : -1;
        int head = byteAt(k);
        if (head == 120 || head == 111 || head == 62) return static_cast<int>(k + 1);
        return (k - q >= 3) ? static_cast<int>(k) : -1;
    };
    auto link = [&]() -> bool {
        int e = linkEnd(p);
        if (e >= 0) return emit(T::LINK, ws(static_cast<size_t>(e)));
        size_t q = (byteAt(p) == 60) ? (p + 1) : p;
        int a = byteAt(q);
        int b = byteAt(q + 1);
        if ((a == 45 && (b == 45 || b == 46)) || (a == 61 && b == 61)) {
            push(LLABEL);
            return emit(T::START_LINK, ws(q + 2));
        }
        return false;
    };
    auto lineState = [&](int type) -> bool {
        size_t e = src.find('\n', p);
        if (e == std::string::npos) e = n;
        pop();
        return emit(type, e);
    };
    auto idList = [&](int type, int next) -> bool {
        size_t k = word(p);
        if (k == p) return false;
        while (byteAt(k) == 44) {
            size_t from = ws(k + 1);
            size_t to = word(from);
            if (to == from) break;
            k = to;
        }
        state = next;
        return emit(type, k);
    };

    auto initial = [&](int c) -> bool {
        if (isSpace(c)) {
            p = ws(p);
            return true;
        }
        size_t q = 0;
        switch (c) {
            case 98: // 'b'
                if (startsWith(src, "block", p)) {
                    if (keyword("block-beta")) return emit(T::KEY, p + 10);
                    if (byteAt(p + 5) == 58) return emit(T::ID_BLOCK, p + 6);
                    if (!isWord(byteAt(p + 5))) return emit(T::KEY, p + 5);
                }
                break;
            case 99: // 'c'
                if (startsWith(src, "columns", p)) {
                    q = ws(p + 7);
                    if (q > p + 7) {
                        if (startsWith(src, "auto", q) && !isWord(byteAt(q + 4))) return put(T::COLUMNS, "-1", q + 4);
                        size_t e = digits(q);
                        if (e > q) return put(T::COLUMNS, src.substr(q, e - q), e);
                    }
                } else if (startsWith(src, "class", p)) {
                    bool def = startsWith(src, "Def", p + 5);
                    size_t from = def ? (p + 8) : (p + 5);
                    q = ws(from);
                    if (q > from) {
                        push(def ? CLASSDEF : CLASS);
                        return emit(def ? T::CLASSDEF : T::CLASS, q);
                    }
                }
                break;
            case 34: // '"'
                return quote();
            case 115: // 's'
                if (startsWith(src, "space", p)) {
                    if (byteAt(p + 5) == 58) {
                        size_t e = digits(p + 6);
                        if (e > p + 6) return put(T::SPACE_BLOCK, src.substr(p + 6, e - (p + 6)), e);
                    }
                    if (!isWord(byteAt(p + 5))) return put(T::SPACE_BLOCK, "1", p + 5);
                } else if (startsWith(src, "style", p)) {
                    q = ws(p + 5);
                    if (q > p + 5) {
                        push(STYLE_STMNT);
                        return emit(T::STYLE, q);
                    }
                }
                break;
            case 100: // 'd'
                if (keyword("default")) return emit(T::DEFAULT, p + 7);
                break;
            case 108: // 'l'
                if (keyword("linkStyle")) return emit(T::LINKSTYLE, p + 9);
                break;
            case 105: // 'i'
                if (keyword("interpolate")) return emit(T::INTERPOLATE, p + 11);
                break;
            case 97: // 'a'
                if (startsWith(src, "accTitle", p)) {
                    q = ws(p + 8);
                    if (byteAt(q) == 58) {
                        push(acc_title);
                        return emit(T::acc_title, ws(q + 1));
                    }
                } else if (startsWith(src, "accDescr", p)) {
                    q = ws(p + 8);
                    int d = byteAt(q);
                    if (d == 58) {
                        push(acc_descr);
                        return emit(T::acc_descr, ws(q + 1));
                    }
                    if (d == 123) {
                        push(acc_descr_multiline);
                        p = ws(q + 1);
                        return true;
                    }
                }
                break;
            case 101: // 'e'
                if (keyword("end")) return emit(T::end, ws(p + 3));
                break;
            case 45: // '-'
                if (byteAt(p + 1) == 41) {
                    push(NODE);
                    return emit(T::NODE_DSTART, p + 2);
                }
                return link();
            case 61: // '='
                return link();
            case 60: // '<'
                if (byteAt(p + 1) == 91) {
                    push(BLOCK_ARROW);
                    return emit(T::BLOCK_ARROW_START, p + 2);
                }
                return link();
            case 40: { // '('
                int d = byteAt(p + 1);
                size_t len = (d == 45 || d == 91) ? 2 : (d == 40 ? (byteAt(p + 2) == 40 ? 3 : 2) : 1);
                push(NODE);
                return emit(T::NODE_DSTART, p + len);
            }
            case 41: // ')'
            case 123: // '{'
                push(NODE);
                return emit(T::NODE_DSTART, byteAt(p + 1) == c ? p + 2 : p + 1);
            case 62: // '>'
                push(NODE);
                return emit(T::NODE_DSTART, p + 1);
            case 91: { // '['
                int d = byteAt(p + 1);
                push(NODE);
                return emit(T::NODE_DSTART, (d == 91 || d == 124 || d == 40 || d == 92 || d == 47) ? p + 2 : p + 1);
            }
            case 58: { // ':'
                size_t e = digits(p + 1);
                if (e > p + 1) return put(T::SIZE, src.substr(p + 1, e - (p + 1)), e);
                return false;
            }
        }
        q = p;
        while (q < n) {
            size_t next = q;
            int ch = charCodeAtUtf8(src, q, &next);
            if (isBlockIdStop(ch)) break;
            q = next;
        }
        return q > p && emit(T::NODE_ID, q);
    };

    auto nodeEnd = [&](int c) -> bool {
        int d = byteAt(p + 1);
        size_t len = 0;
        switch (c) {
            case 34: return quote();
            case 40: len = (d == 45) ? 2 : (d == 40 ? (byteAt(p + 2) == 40 ? 3 : 2) : 1); break;
            case 41: len = (d == 41) ? (byteAt(p + 2) == 41 ? 3 : 2) : (d == 93 ? 2 : 1); break;
            case 125: len = (d == 125) ? 2 : 1; break;
            case 45: len = (d == 41) ? 2 : 0; break;
            case 93: len = (d == 93 || d == 41 || d == 62) ? 2 : 1; break;
            case 92:
            case 47: len = (d == 93) ? 2 : 0; break;
        }
        if (len == 0) return false;
        pop();
        return emit(T::NODE_DEND, p + len);
    };

    auto arrowDir = [&](int c) -> bool {
        if (c == 41) {
            pop();
            pop();
            return put(T::BLOCK_ARROW_END, "]>", ws(p + 1));
        }
        size_t from = ws(c == 44 ? p + 1 : p);
        for (const auto& dir : DIRECTIONS) {
            if (startsWith(src, dir, from)) {
                size_t e = ws(from + dir.size());
                size_t s = (c == 44) ? from : p;
                return put(T::DIR, src.substr(s, e - s), e);
            }
        }
        return false;
    };

    while (true) {
        if (p >= n) {
            int type = -1;
            switch (state) {
                case INITIAL: type = T::EOF_TOK; break;
                case S_string: type = T::STR; break;
                case CLASSDEFID: type = T::CLASSDEF_STYLEOPTS; break;
                case CLASS_STYLE: type = T::STYLECLASS; break;
                case STYLE_DEFINITION: type = T::STYLE_DEFINITION_DATA; break;
                case acc_title: type = T::acc_title_value; break;
                case acc_descr: type = T::acc_descr_value; break;
                case acc_descr_multiline: type = T::acc_descr_multiline_value; break;
            }
            p = n;
            if (type != -1) put(type, "", n);
            break;
        }

        int c = charAt(p);
        bool ok = true;
        switch (state) {
            case INITIAL: ok = initial(c); break;
            case S_string:
                if (c == 34) {
                    pop();
                    p++;
                } else {
                    size_t e = src.find('"', p);
                    emit(T::STR, e == std::string::npos ? n : e);
                }
                break;
            case S_md_string:
                if (c == 96) {
                    ok = (byteAt(p + 1) == 34);
                    if (ok) { pop(); p += 2; }
                } else if (c == 34) {
                    ok = false;
                } else {
                    size_t q = p + 1;
                    while (q < n) {
                        int ch = byteAt(q);
                        if (ch == 96 || ch == 34) break;
                        q++;
                    }
                    emit(T::MD_STR, q);
                }
                break;
            case NODE: ok = nodeEnd(c); break;
            case BLOCK_ARROW:
                if (c == 34) {
                    quote();
                } else if (c == 93 && byteAt(p + 1) == 62) {
                    size_t q = ws(p + 2);
                    ok = (byteAt(q) == 40);
                    if (ok) {
                        push(ARROW_DIR);
                        p = q + 1;
                    }
                } else {
                    ok = false;
                }
                break;
            case ARROW_DIR: ok = arrowDir(c); break;
            case LLABEL:
                if (c == 34) {
                    if (byteAt(p + 1) == 96) {
                        quote();
                    } else {
                        emit(T::LINK_LABEL, p + 1);
                        push(S_string);
                    }
                } else {
                    int e = linkEnd(ws(p));
                    ok = (e >= 0);
                    if (ok) {
                        pop();
                        emit(T::LINK, ws(static_cast<size_t>(e)));
                    }
                }
                break;
            case CLASSDEF: {
                int type = T::DEFAULT_CLASSDEF_ID;
                size_t k = startsWith(src, "DEFAULT", p) ? (p + 7) : p;
                size_t q = ws(k);
                if (k == p || q == k) {
                    type = T::CLASSDEF_ID;
                    k = word(p);
                    q = ws(k);
                }
                ok = (k > p && q > k);
                if (ok) {
                    state = CLASSDEFID;
                    emit(type, q);
                }
                break;
            }
            case CLASSDEFID: lineState(T::CLASSDEF_STYLEOPTS); break;
            case CLASS: ok = idList(T::CLASSENTITY_IDS, CLASS_STYLE); break;
            case CLASS_STYLE: lineState(T::STYLECLASS); break;
            case STYLE_STMNT: ok = idList(T::STYLE_ENTITY_IDS, STYLE_DEFINITION); break;
            case STYLE_DEFINITION: lineState(T::STYLE_DEFINITION_DATA); break;
            case acc_title: lineState(T::acc_title_value); break;
            case acc_descr: lineState(T::acc_descr_value); break;
            case acc_descr_multiline:
                if (c == 125) {
                    pop();
                    p++;
                } else {
                    size_t e = src.find('}', p);
                    emit(T::acc_descr_multiline_value, e == std::string::npos ? n : e);
                }
                break;
        }
        if (!ok) {
            out.types.push_back(T::ERROR);
            out.texts.push_back("");
            out.starts.push_back(p);
            return out;
        }
    }
    out.types.push_back(T::END);
    out.texts.push_back("");
    out.starts.push_back(n);
    return out;
}

// ============================================================================
// src/diagrams/block/parser.ts
// ============================================================================

inline std::vector<Statement> closeDocument(std::vector<Statement> list) noexcept {
    if (list.empty() || !list.back().isList) return list;
    auto last = std::move(list.back().list);
    list.pop_back();
    for (auto& b : last) {
        Statement s;
        s.isList = false;
        s.single = std::move(b);
        list.push_back(std::move(s));
    }
    return list;
}

inline void parseBlock(const std::string& src, BlockDb& db) {
    auto tok = tokenize(src);
    const auto& types = tok.types;
    const auto& texts = tok.texts;
    size_t i = 0;

    auto expect = [&](int type) -> std::string {
        if (i >= types.size() || types[i] != type) return "";
        return texts[i++];
    };

    struct NodeInfo {
        std::string id;
        std::string label;
        bool hasLabel = false;
        std::string typeStr;
        std::vector<std::string> directions;
    };

    auto parseNode = [&]() -> NodeInfo {
        std::string id = expect(T::NODE_ID);
        if (i < types.size() && types[i] == T::NODE_DSTART) {
            std::string open = texts[i++];
            std::string label = expect(T::STR);
            std::string close = expect(T::NODE_DEND);
            return {id, label, true, open + close, {}};
        }
        if (i < types.size() && types[i] == T::BLOCK_ARROW_START) {
            std::string open = texts[i++];
            std::string label = expect(T::STR);
            std::vector<std::string> directions = {expect(T::DIR)};
            while (i < types.size() && types[i] == T::DIR) directions.push_back(texts[i++]);
            std::string close = expect(T::BLOCK_ARROW_END);
            return {id, label, true, open + close, directions};
        }
        return {id, "", false, "", {}};
    };

    auto nodeStatement = [&]() -> Statement {
        NodeInfo first = parseNode();
        int size = (i < types.size() && types[i] == T::SIZE) ? std::stoi(texts[i++]) : 1;
        Block from;
        from.id = first.id;
        from.hasId = true;
        from.label = first.label;
        from.hasLabel = first.hasLabel;
        from.type = typeStr2Type(first.typeStr);
        from.directions = first.directions;
        from.widthInColumns = size;
        from.hasWidthInColumns = true;

        std::vector<Block> chain;
        bool hasChain = false;
        while (i < types.size() && (types[i] == T::LINK || types[i] == T::START_LINK)) {
            std::string label;
            if (types[i++] == T::START_LINK) {
                expect(T::LINK_LABEL);
                label = expect(T::STR);
                expect(T::LINK);
            }
            std::string link = texts[i - 1];
            NodeInfo to = parseNode();
            std::string arrowTypeEnd = edgeStrToEdgeData(link);
            std::string arrowTypeStart = edgeStrToEdgeStartData(link);
            std::string thickness = edgeStrToThickness(link);
            std::string pattern = edgeStrToPattern(link);
            if (!hasChain) {
                hasChain = true;
                Block firstBlk;
                firstBlk.id = from.id;
                firstBlk.hasId = true;
                firstBlk.label = from.label;
                firstBlk.hasLabel = from.hasLabel;
                firstBlk.type = from.type;
                firstBlk.directions = from.directions;
                chain.push_back(std::move(firstBlk));
            }
            from = Block{};
            from.id = to.id;
            from.hasId = true;
            from.label = to.label;
            from.hasLabel = to.hasLabel;
            from.type = typeStr2Type(to.typeStr);
            from.directions = to.directions;

            Block edgeBlk;
            edgeBlk.id = Pele::cat({chain.back().id, "-", to.id});
            edgeBlk.hasId = true;
            edgeBlk.start = chain.back().id;
            edgeBlk.end = to.id;
            edgeBlk.label = label;
            edgeBlk.hasLabel = true;
            edgeBlk.type = "edge";
            edgeBlk.thickness = thickness;
            edgeBlk.pattern = pattern;
            edgeBlk.directions = to.directions;
            edgeBlk.arrowTypeEnd = arrowTypeEnd;
            edgeBlk.arrowTypeStart = arrowTypeStart;

            chain.push_back(std::move(edgeBlk));
            chain.push_back(from);
        }
        Statement stmt;
        if (hasChain) {
            stmt.isList = true;
            stmt.list = std::move(chain);
        } else {
            stmt.isList = false;
            stmt.single = std::move(from);
        }
        return stmt;
    };

    expect(T::KEY);
    struct OpenFrame {
        std::vector<Statement> list;
        Statement header;
        bool hasHeader = false;
    };
    std::vector<OpenFrame> open;
    std::vector<Statement> list;

    while (i < types.size()) {
        int t = types[i];
        if (t == T::NODE_ID) {
            list.push_back(nodeStatement());
        } else if (t == T::COLUMNS) {
            Block b;
            b.type = "column-setting";
            b.columns = std::stoi(texts[i++]);
            b.hasColumns = true;
            list.push_back({false, std::move(b), {}});
        } else if (t == T::SPACE_BLOCK) {
            int w = std::stoi(texts[i++]);
            Block b;
            b.id = db.generateId();
            b.hasId = true;
            b.type = "space";
            b.label = "";
            b.hasLabel = true;
            b.width = w;
            b.hasWidth = true;
            list.push_back({false, std::move(b), {}});
        } else if (t == T::KEY) {
            i++;
            open.push_back({std::move(list), {}, false});
            list.clear();
        } else if (t == T::ID_BLOCK) {
            i++;
            Statement hdr = nodeStatement();
            open.push_back({std::move(list), std::move(hdr), true});
            list.clear();
        } else if (t == T::end) {
            if (open.empty()) break;
            auto frame = std::move(open.back());
            open.pop_back();
            i++;
            std::string id = db.generateId();
            auto closed = closeDocument(std::move(list));
            std::vector<Block> children = BlockDb::flattenStatements(closed);
            list = std::move(frame.list);
            Block comp;
            if (!frame.hasHeader) {
                comp.id = id;
                comp.hasId = true;
                comp.type = "composite";
                comp.label = "";
                comp.hasLabel = true;
                comp.children = std::move(children);
            } else {
                comp = frame.header.single;
                comp.type = "composite";
                comp.children = std::move(children);
            }
            list.push_back({false, std::move(comp), {}});
        } else if (t == T::CLASSDEF) {
            i++;
            Block b;
            b.type = "classDef";
            b.id = trimStr(expect(T::CLASSDEF_ID));
            b.hasId = true;
            b.css = trimStr(expect(T::CLASSDEF_STYLEOPTS));
            list.push_back({false, std::move(b), {}});
        } else if (t == T::CLASS) {
            i++;
            Block b;
            b.type = "applyClass";
            b.id = trimStr(expect(T::CLASSENTITY_IDS));
            b.hasId = true;
            b.styleClass = trimStr(expect(T::STYLECLASS));
            list.push_back({false, std::move(b), {}});
        } else if (t == T::STYLE) {
            i++;
            Block b;
            b.type = "applyStyles";
            b.id = trimStr(expect(T::STYLE_ENTITY_IDS));
            b.hasId = true;
            b.stylesStr = trimStr(expect(T::STYLE_DEFINITION_DATA));
            list.push_back({false, std::move(b), {}});
        } else if (t == T::EOF_TOK || t == T::END) {
            db.setHierarchy(closeDocument(std::move(list)));
            return;
        } else {
            i++;
        }
    }
    if (!list.empty()) {
        db.setHierarchy(closeDocument(std::move(list)));
    }
}

// ============================================================================
// src/diagrams/block/arrows.ts
// ============================================================================

constexpr int ARR_RIGHT = 1;
constexpr int ARR_LEFT = 2;
constexpr int ARR_UP = 4;
constexpr int ARR_DOWN = 8;
constexpr double ARR_OVERHANG = 7.0;
constexpr double ARR_MAX_DEPTH = 20.0;
constexpr double ARR_NUB = 12.0;

struct ArrowSize {
    double w = 0.0;
    double h = 0.0;
    double dx = 0.0;
    double dy = 0.0;
};

inline int arrowMask(const std::vector<std::string>& directions) noexcept {
    int mask = 0;
    for (const auto& rawDir : directions) {
        std::string d = trimStr(rawDir);
        if (d == "right") mask |= ARR_RIGHT;
        else if (d == "left") mask |= ARR_LEFT;
        else if (d == "up") mask |= ARR_UP;
        else if (d == "down") mask |= ARR_DOWN;
        else if (d == "x") mask |= ARR_RIGHT | ARR_LEFT;
        else if (d == "y") mask |= ARR_UP | ARR_DOWN;
    }
    return mask;
}

inline char arrowAxis(int mask) noexcept {
    if (mask != 0 && (mask & (ARR_UP | ARR_DOWN)) == 0) return 'x';
    if (mask != 0 && (mask & (ARR_RIGHT | ARR_LEFT)) == 0) return 'y';
    return '\0';
}

inline ArrowSize arrowSize(int mask, double tw, double th) noexcept {
    char axis = arrowAxis(mask);
    auto bit = [&](int flag) -> int { return (mask & flag) ? 1 : 0; };
    int across = bit(ARR_LEFT) - bit(ARR_RIGHT);
    int along = bit(ARR_UP) - bit(ARR_DOWN);
    if (axis == 'x') {
        double h = std::max(th + 8.0, 14.0) + 2.0 * ARR_OVERHANG;
        double depth = std::min(h / 2.0, ARR_MAX_DEPTH);
        return {std::max(tw + 12.0, 16.0) + depth * (mask == (ARR_RIGHT | ARR_LEFT) ? 2.0 : 1.0), h, (across * depth) / 2.0, 0.0};
    }
    if (axis == 'y') {
        double w = std::max(tw + 16.0, 14.0) + 2.0 * ARR_OVERHANG;
        double depth = std::min(w / 2.0, ARR_MAX_DEPTH);
        return {w, std::max(th + 8.0, 16.0) + depth * (mask == (ARR_UP | ARR_DOWN) ? 2.0 : 1.0), 0.0, (along * depth) / 2.0};
    }
    return {
        std::max(tw + 16.0, 24.0) + ARR_NUB * (bit(ARR_LEFT) + bit(ARR_RIGHT)),
        std::max(th + 8.0, 24.0) + ARR_NUB * (bit(ARR_UP) + bit(ARR_DOWN)),
        (across * ARR_NUB) / 2.0,
        (along * ARR_NUB) / 2.0
    };
}

inline std::string drawArrow(int mask, double w, double h, const std::string& attrs) noexcept {
    char axis = arrowAxis(mask);
    double x = w / 2.0;
    double y = h / 2.0;
    std::vector<double> p;
    if (axis == 'x') {
        double depth = std::min(y, ARR_MAX_DEPTH);
        double s = y - ARR_OVERHANG;
        double x0 = (mask & ARR_LEFT) ? (-x + depth) : -x;
        double x1 = (mask & ARR_RIGHT) ? (x - depth) : x;
        p = {x0, -s, x1, -s};
        if (mask & ARR_RIGHT) p.insert(p.end(), {x1, -y, x, 0.0, x1, y});
        p.insert(p.end(), {x1, s, x0, s});
        if (mask & ARR_LEFT) p.insert(p.end(), {x0, y, -x, 0.0, x0, -y});
    } else if (axis == 'y') {
        double depth = std::min(x, ARR_MAX_DEPTH);
        double s = x - ARR_OVERHANG;
        double y0 = (mask & ARR_UP) ? (-y + depth) : -y;
        double y1 = (mask & ARR_DOWN) ? (y - depth) : y;
        p = {s, y0, s, y1};
        if (mask & ARR_DOWN) p.insert(p.end(), {x, y1, 0.0, y, -x, y1});
        p.insert(p.end(), {-s, y1, -s, y0});
        if (mask & ARR_UP) p.insert(p.end(), {-x, y0, 0.0, -y, x, y0});
    } else {
        double x0 = (mask & ARR_LEFT) ? (-x + ARR_NUB) : -x;
        double x1 = (mask & ARR_RIGHT) ? (x - ARR_NUB) : x;
        double y0 = (mask & ARR_UP) ? (-y + ARR_NUB) : -y;
        double y1 = (mask & ARR_DOWN) ? (y - ARR_NUB) : y;
        double cx = (x0 + x1) / 2.0;
        double cy = (y0 + y1) / 2.0;
        double kx = std::min(10.0, (x1 - x0) / 2.0);
        double ky = std::min(10.0, (y1 - y0) / 2.0);
        p = {x0, y0};
        if (mask & ARR_UP) p.insert(p.end(), {cx - kx, y0, cx, -y, cx + kx, y0});
        p.insert(p.end(), {x1, y0});
        if (mask & ARR_RIGHT) p.insert(p.end(), {x1, cy - ky, x, cy, x1, cy + ky});
        p.insert(p.end(), {x1, y1});
        if (mask & ARR_DOWN) p.insert(p.end(), {cx + kx, y1, cx, y, cx - kx, y1});
        p.insert(p.end(), {x0, y1});
        if (mask & ARR_LEFT) p.insert(p.end(), {x0, cy + ky, -x, cy, x0, cy - ky});
    }
    std::string points;
    for (size_t i = 0; i + 1 < p.size(); i += 2) {
        if (i > 0) points.push_back(' ');
        Pele::appendCat(points, {num(p[i]), ",", num(p[i + 1])});
    }
    return Pele::cat({"<polygon points=\"", points, "\" stroke-linejoin=\"round\"", attrs, "/>"});
}

// ============================================================================
// src/diagrams/block/layout.ts & src/diagrams/block/render.ts
// ============================================================================

enum class ViewKind { Leaf, Arrow, Composite, Space };

struct View {
    std::vector<View*> children;
    int columns = -1;
    int span = 1;
    bool space = false;
    double head = 0.0;
    double nw = 0.0;
    double nh = 0.0;
    int col = 0;
    int row = 0;
    int cols = 0;
    std::vector<double> heights;
    std::vector<double> rowGaps;
    std::vector<double> colGaps;
    double x = 0.0;
    double y = 0.0;
    double w = 0.0;
    double h = 0.0;

    const Block* block = nullptr;
    ViewKind kind = ViewKind::Leaf;
    int index = 0;
    std::string shape;
    Label label;
    ResolvedStyle style;
    int mask = 0;
    double dx = 0.0;
    double dy = 0.0;
    double bw = 0.0;
    double bh = 0.0;
};

struct Box {
    double x = 0.0;
    double y = 0.0;
    double bw = 0.0;
    double bh = 0.0;
    const View* viewPtr = nullptr;
};

struct Route {
    const Block* edge = nullptr;
    std::vector<double> points;
    int loop = 0;
    Label label;
    double lx = 0.0;
    double ly = 0.0;
};

inline double totalGaps(const std::vector<double>& gaps) noexcept {
    double sum = 0.0;
    for (double g : gaps) sum += g;
    return sum;
}

inline void measureCell(View& box, double gap, double inset) noexcept {
    int columns = box.columns;
    std::vector<double> heights;
    int position = 0;
    int cols = 0;
    double cw = 0.0;
    double tallest = 0.0;
    for (View* child : box.children) {
        int px = (columns < 0) ? position : (position % columns);
        int py = (columns < 0) ? 0 : (position / columns);
        int span = child->span >= 1 ? std::min(child->span, 1000) : 1;
        if (columns > 0 && span > columns - px) span = columns - px;
        child->span = span;
        child->col = px;
        child->row = py;
        position += span;
        if (px + span > cols) cols = px + span;
        if (py == static_cast<int>(heights.size())) heights.push_back(0.0);
        if (child->children.empty()) heights[py] = -1.0;
        else if (heights[py] >= 0.0 && child->nh > heights[py]) heights[py] = child->nh;
        if (child->space) continue;
        double width = (child->nw - gap * (span - 1)) / span;
        if (width > cw) cw = width;
        if (child->nh > tallest) tallest = child->nh;
    }
    double height = (static_cast<double>(heights.size()) - 1.0) * gap + totalGaps(box.rowGaps);
    for (size_t row = 0; row < heights.size(); row++) {
        if (heights[row] < 0.0) heights[row] = tallest;
        height += heights[row];
    }
    box.cols = cols;
    box.heights = std::move(heights);
    box.nw = std::max(box.nw, cols * cw + (cols - 1) * gap + totalGaps(box.colGaps) + 2.0 * inset);
    box.nh = height + 2.0 * inset + box.head;
}

inline void arrangeCell(View& box, double gap, double inset) noexcept {
    int cols = box.cols;
    const auto& heights = box.heights;
    const auto& rowGaps = box.rowGaps;
    const auto& colGaps = box.colGaps;
    int rows = static_cast<int>(heights.size());
    double cw = (box.w - 2.0 * inset - (cols - 1) * gap - totalGaps(colGaps)) / (cols > 0 ? cols : 1);
    double room = box.h - 2.0 * inset - box.head - (rows - 1) * gap - totalGaps(rowGaps);
    std::vector<double> offsets;
    if (!colGaps.empty()) {
        double sum = 0.0;
        for (int col = 0; col < cols; col++) {
            offsets.push_back(sum);
            sum += (col < static_cast<int>(colGaps.size())) ? colGaps[col] : 0.0;
        }
    }
    double needed = 0.0;
    for (double h : heights) needed += h;
    double scale = needed > 0.0 ? (room / needed) : 0.0;
    double share = needed > 0.0 ? 0.0 : (rows > 0 ? room / rows : 0.0);
    double left = box.x - box.w / 2.0 + inset;
    double top = box.y - box.h / 2.0 + inset + box.head;
    int row = 0;
    for (View* child : box.children) {
        for (; row < child->row; row++) {
            double rg = (row < static_cast<int>(rowGaps.size())) ? rowGaps[row] : 0.0;
            top += heights[row] * scale + share + gap + rg;
        }
        double before = (child->col < static_cast<int>(offsets.size())) ? offsets[child->col] : 0.0;
        int endCol = child->col + child->span - 1;
        double after = (endCol >= 0 && endCol < static_cast<int>(offsets.size())) ? offsets[endCol] : 0.0;
        child->w = cw * child->span + gap * (child->span - 1) + after - before;
        child->h = heights[row] * scale + share;
        child->x = left + child->col * (cw + gap) + before + child->w / 2.0;
        child->y = top + child->h / 2.0;
    }
}

inline std::pair<double, double> layoutCells(View& root, double gap) noexcept {
    std::vector<View*> order = {&root};
    for (size_t i = 0; i < order.size(); i++) {
        for (View* child : order[i]->children) order.push_back(child);
    }
    for (int i = static_cast<int>(order.size()) - 1; i >= 0; i--) {
        View* box = order[i];
        if (!box->children.empty()) measureCell(*box, gap, i == 0 ? 0.0 : gap);
    }
    root.w = root.nw;
    root.h = root.nh;
    root.x = root.w / 2.0;
    root.y = root.h / 2.0;
    for (size_t i = 0; i < order.size(); i++) {
        View* box = order[i];
        if (!box->children.empty()) arrangeCell(*box, gap, i == 0 ? 0.0 : gap);
    }
    return {root.w, root.h};
}

inline bool isRoundView(const View& v) noexcept {
    return v.kind == ViewKind::Leaf && (v.shape == "circle" || v.shape == "dbl-circ");
}

inline double blockEdgeY(const View& v, double x, double sign) noexcept {
    double hw = v.bw / 2.0;
    double hh = v.bh / 2.0;
    double dx = std::abs(x - v.x);
    double reach = hh;
    if (isRoundView(v)) reach = std::sqrt(std::max(hw * hw - dx * dx, 0.0));
    else if (v.kind == ViewKind::Leaf && v.shape == "diam") reach = hh * std::max(1.0 - dx / hw, 0.0);
    else if (v.kind == ViewKind::Leaf) reach = hh - shapeInset(v.shape, v.bw, v.bh, sign > 0 ? 2 : 0);
    else if (v.kind == ViewKind::Arrow && arrowAxis(v.mask) == 'x') reach = hh - ARR_OVERHANG;
    return v.y + sign * reach;
}

inline double blockEdgeX(const View& v, double y, double sign) noexcept {
    double hw = v.bw / 2.0;
    double hh = v.bh / 2.0;
    double dy = std::abs(y - v.y);
    double reach = hw;
    if (isRoundView(v)) reach = std::sqrt(std::max(hh * hh - dy * dy, 0.0));
    else if (v.kind == ViewKind::Leaf && v.shape == "diam") reach = hw * std::max(1.0 - dy / hh, 0.0);
    else if (v.kind == ViewKind::Leaf) reach = hw - shapeInset(v.shape, v.bw, v.bh, sign > 0 ? 1 : 3);
    else if (v.kind == ViewKind::Arrow && arrowAxis(v.mask) == 'y') reach = hw - ARR_OVERHANG;
    return v.x + sign * reach;
}

inline void blockBorder(const View& v, double tx, double ty, std::vector<double>& out) noexcept {
    double dx = tx - v.x;
    double dy = ty - v.y;
    double hw = v.bw / 2.0;
    double hh = v.bh / 2.0;
    double k = 0.0;
    if (isRoundView(v)) {
        double h = std::hypot(dx, dy);
        k = hw / (h == 0.0 ? 1.0 : h);
    } else if (v.kind == ViewKind::Leaf && v.shape == "diam") {
        double den = std::abs(dx) / hw + std::abs(dy) / hh;
        k = 1.0 / (den == 0.0 ? 1.0 : den);
    } else {
        double kx = (dx == 0.0) ? std::numeric_limits<double>::infinity() : (hw / std::abs(dx));
        double ky = (dy == 0.0) ? std::numeric_limits<double>::infinity() : (hh / std::abs(dy));
        k = std::min(kx, ky);
    }
    if (!std::isfinite(k)) k = 0.0;
    out.push_back(v.x + dx * k);
    out.push_back(v.y + dy * k);
}

inline bool crossesBox(double x1, double y1, double x2, double y2, const View& v) noexcept {
    double l = v.x - v.bw / 2.0 + 1.0;
    double r = v.x + v.bw / 2.0 - 1.0;
    double t = v.y - v.bh / 2.0 + 1.0;
    double b = v.y + v.bh / 2.0 - 1.0;
    if ((x1 < l && x2 < l) || (x1 > r && x2 > r) || (y1 < t && y2 < t) || (y1 > b && y2 > b)) return false;
    if (x1 == x2 || y1 == y2) return true;
    double dx = x2 - x1;
    double dy = y2 - y1;
    auto side = [&](double x, double y) -> int {
        double val = dx * (y - y1) - dy * (x - x1);
        return (val > 0.0) ? 1 : (val < 0.0) ? -1 : 0;
    };
    int first = side(l, t);
    return first == 0 || side(r, t) != first || side(r, b) != first || side(l, b) != first;
}

inline int countHits(const std::vector<double>& points, const View* s, const View* t, const std::vector<View*>& obstacles) noexcept {
    int count = 0;
    for (const View* v : obstacles) {
        if (v == s || v == t) continue;
        for (size_t i = 0; i + 3 < points.size(); i += 2) {
            if (crossesBox(points[i], points[i + 1], points[i + 2], points[i + 3], *v)) {
                count++;
                break;
            }
        }
    }
    return count;
}

inline std::vector<double> routeBlockEdge(const View& s, const View& t, double shift, const std::vector<View*>* obstacles) noexcept {
    double sl = s.x - s.bw / 2.0;
    double sr = s.x + s.bw / 2.0;
    double st = s.y - s.bh / 2.0;
    double sb = s.y + s.bh / 2.0;
    double tl = t.x - t.bw / 2.0;
    double tr = t.x + t.bw / 2.0;
    double tt = t.y - t.bh / 2.0;
    double tb = t.y + t.bh / 2.0;
    double x1 = std::max(sl, tl);
    double x2 = std::min(sr, tr);
    double y1 = std::max(st, tt);
    double y2 = std::min(sb, tb);
    bool overX = (x2 - x1 > 6.0);
    bool overY = (y2 - y1 > 6.0);

    if (overX && overY) {
        const View& inner = (s.bw * s.bh <= t.bw * t.bh) ? s : t;
        const View& outer = (&inner == &s) ? t : s;
        double up = inner.y - inner.bh / 2.0 - (outer.y - outer.bh / 2.0);
        double down = outer.y + outer.bh / 2.0 - (inner.y + inner.bh / 2.0);
        double left = inner.x - inner.bw / 2.0 - (outer.x - outer.bw / 2.0);
        double right = outer.x + outer.bw / 2.0 - (inner.x + inner.bw / 2.0);
        double least = std::min({up, down, left, right});
        std::vector<double> points;
        if (least == down) points = {inner.x, blockEdgeY(inner, inner.x, 1.0), inner.x, outer.y + outer.bh / 2.0};
        else if (least == up) points = {inner.x, blockEdgeY(inner, inner.x, -1.0), inner.x, outer.y - outer.bh / 2.0};
        else if (least == right) points = {blockEdgeX(inner, inner.y, 1.0), inner.y, outer.x + outer.bw / 2.0, inner.y};
        else points = {blockEdgeX(inner, inner.y, -1.0), inner.y, outer.x - outer.bw / 2.0, inner.y};
        if (&inner == &s) return points;
        return {points[2], points[3], points[0], points[1]};
    }
    if (overX) {
        double margin = std::min(6.0, (x2 - x1) / 2.0);
        double x = std::min(std::max((x1 + x2) / 2.0 + shift, x1 + margin), x2 - margin);
        double down = (t.y > s.y) ? 1.0 : -1.0;
        return {x, blockEdgeY(s, x, down), x, blockEdgeY(t, x, -down)};
    }
    if (overY) {
        double margin = std::min(6.0, (y2 - y1) / 2.0);
        double y = std::min(std::max((y1 + y2) / 2.0 + shift, y1 + margin), y2 - margin);
        double right = (t.x > s.x) ? 1.0 : -1.0;
        return {blockEdgeX(s, y, right), y, blockEdgeX(t, y, -right), y};
    }

    double right = (t.x > s.x) ? 1.0 : -1.0;
    double down = (t.y > s.y) ? 1.0 : -1.0;
    std::vector<double> across = {blockEdgeX(s, s.y, right), s.y, t.x, s.y, t.x, blockEdgeY(t, t.x, -down)};
    std::vector<double> along = {s.x, blockEdgeY(s, s.x, down), s.x, t.y, blockEdgeX(t, t.y, -right), t.y};
    bool wide = (std::abs(t.x - s.x) >= std::abs(t.y - s.y));
    std::vector<double> best = wide ? across : along;
    if (!obstacles) return best;
    int least = countHits(best, &s, &t, *obstacles);
    if (least == 0) return best;
    const auto& other = wide ? along : across;
    int count = countHits(other, &s, &t, *obstacles);
    if (count < least) {
        best = other;
        least = count;
    }
    if (least == 0) return best;
    std::vector<double> direct;
    blockBorder(s, t.x, t.y, direct);
    blockBorder(t, s.x, s.y, direct);
    return countHits(direct, &s, &t, *obstacles) < least ? direct : best;
}

inline EdgePath traceBlockEdge(const std::vector<double>& p, double startTrim, double endTrim) noexcept {
    size_t n = p.size();
    double sx = p[0];
    double sy = p[1];
    double ex = p[n - 2];
    double ey = p[n - 1];
    double ax = p[2] - sx;
    double ay = p[3] - sy;
    double first = std::hypot(ax, ay);
    ax /= (first == 0.0 ? 1.0 : first);
    ay /= (first == 0.0 ? 1.0 : first);
    double bx = ex - p[n - 4];
    double by = ey - p[n - 3];
    double last = std::hypot(bx, by);
    bx /= (last == 0.0 ? 1.0 : last);
    by /= (last == 0.0 ? 1.0 : last);
    if (n == 4 && startTrim + endTrim > first && (startTrim + endTrim) > 0.0) {
        double scale = first / (startTrim + endTrim);
        startTrim *= scale;
        endTrim *= scale;
    }
    startTrim = std::min(startTrim, first);
    endTrim = std::min(endTrim, last);
    std::string d = Pele::cat({"M", num(sx + ax * startTrim), ",", num(sy + ay * startTrim)});
    if (n == 6) {
        double r = std::max(0.0, std::min({8.0, first - startTrim, last - endTrim}));
        Pele::appendCat(d, {"L", num(p[2] - ax * r), ",", num(p[3] - ay * r), "Q", num(p[2]), ",", num(p[3]), " ", num(p[2] + bx * r), ",", num(p[3] + by * r)});
    }
    Pele::appendCat(d, {"L", num(ex - bx * endTrim), ",", num(ey - by * endTrim)});
    return {d, {}, false, sx, sy, -ax, -ay, ex, ey, bx, by};
}

struct LoopArcInfo {
    double px, py, r, a1, a2;
};

inline LoopArcInfo blockLoopArc(const View& v, int k) noexcept {
    constexpr double PI = 3.14159265358979323846;
    constexpr double SQRT1_2 = 0.70710678118654752440;
    double hw = v.bw / 2.0;
    double hh = v.bh / 2.0;
    double r = std::max(1.0, std::min({10.0 + 5.0 * (k - 1), hw, hh}));
    if (isRoundView(v)) {
        double px = v.x + hw * SQRT1_2;
        double py = v.y - hw * SQRT1_2;
        double half = 2.0 * std::asin(std::min(1.0, r / (2.0 * hw)));
        auto angle = [&](double at) {
            return std::atan2(v.y - hw * std::sin(at) - py, v.x + hw * std::cos(at) - px);
        };
        return {px, py, r, angle(PI / 4.0 + half), angle(PI / 4.0 - half)};
    }
    if (v.kind == ViewKind::Leaf && v.shape == "diam") {
        double slope = std::atan2(hh, hw);
        return {v.x + hw / 2.0, v.y - hh / 2.0, r, slope + PI, slope};
    }
    return {v.x + hw, v.y - hh, r, PI, PI / 2.0};
}

inline EdgePath blockLoopPath(const View& v, int k, double startTrim, double endTrim) noexcept {
    constexpr double PI = 3.14159265358979323846;
    auto arc = blockLoopArc(v, k);
    double from = arc.a1 + startTrim / arc.r;
    double to = arc.a2 - endTrim / arc.r;
    double sweep = std::fmod(to - from, 2.0 * PI);
    if (sweep <= 0.0) sweep += 2.0 * PI;
    double x1 = arc.px + arc.r * std::cos(from);
    double y1 = arc.py + arc.r * std::sin(from);
    double x2 = arc.px + arc.r * std::cos(to);
    double y2 = arc.py + arc.r * std::sin(to);
    double sx = arc.px + arc.r * std::cos(arc.a1);
    double sy = arc.py + arc.r * std::sin(arc.a1);
    double ex = arc.px + arc.r * std::cos(arc.a2);
    double ey = arc.py + arc.r * std::sin(arc.a2);
    double sl = std::hypot(sx - x1, sy - y1);
    double el = std::hypot(ex - x2, ey - y2);
    std::string d = Pele::cat({"M", num(x1), ",", num(y1), "A", num(arc.r), ",", num(arc.r), " 0 ", (sweep > PI ? "1" : "0"), " 1 ", num(x2), ",", num(y2)});
    return {
        d, {}, false,
        sx, sy,
        sl > 0.0 ? (sx - x1) / sl : std::sin(arc.a1),
        sl > 0.0 ? (sy - y1) / sl : -std::cos(arc.a1),
        ex, ey,
        el > 0.0 ? (ex - x2) / el : -std::sin(arc.a2),
        el > 0.0 ? (ey - y2) / el : std::cos(arc.a2)
    };
}

inline bool coversBox(double x, double y, double w, double h, const View* s, const View* t, const std::vector<Box>* obstacles) noexcept {
    if (!obstacles) return false;
    for (const auto& v : *obstacles) {
        if (v.viewPtr != s && v.viewPtr != t && std::abs(x - v.x) < (w + v.bw) / 2.0 && std::abs(y - v.y) < (h + v.bh) / 2.0) return true;
    }
    return false;
}

inline double labelSpot(const std::vector<double>& p, size_t at, double w, double h, const View* s, const View* t, const std::vector<View*>* obstacles) noexcept {
    if (!obstacles) return 0.5;
    static const double SPOTS[5] = {0.5, 0.3, 0.7, 0.15, 0.85};
    for (double spot : SPOTS) {
        double x = p[at] + (p[at + 2] - p[at]) * spot;
        double y = p[at + 1] + (p[at + 3] - p[at + 1]) * spot;
        bool hit = false;
        for (const View* v : *obstacles) {
            if (v != s && v != t && std::abs(x - v->x) < (w + v->bw) / 2.0 && std::abs(y - v->y) < (h + v->bh) / 2.0) {
                hit = true;
                break;
            }
        }
        if (!hit) return spot;
    }
    return 0.5;
}

inline double stackLabels(const std::vector<size_t>& stack, std::vector<Route>& routes, const std::vector<std::vector<double>>& legs, const View* s, const View* t, const std::vector<Box>* obstacles) noexcept {
    constexpr double LABEL_GAP = 4.0;
    bool vertical = (legs[stack[0]][0] == legs[stack[0]][2]);
    double lo = -std::numeric_limits<double>::infinity();
    double hi = std::numeric_limits<double>::infinity();
    double need = -LABEL_GAP;
    bool crowded = false;
    for (size_t a = 0; a < stack.size(); a++) {
        const auto& leg = legs[stack[a]];
        if (vertical ? (leg[0] != leg[2]) : (leg[1] != leg[3])) return 0.0;
        lo = std::max(lo, vertical ? std::min(leg[1], leg[3]) : std::min(leg[0], leg[2]));
        hi = std::min(hi, vertical ? std::max(leg[1], leg[3]) : std::max(leg[0], leg[2]));
        const auto& p = routes[stack[a]];
        need += (vertical ? p.label.height : (p.label.width + 8.0)) + LABEL_GAP;
        for (size_t b = 0; b < a; b++) {
            const auto& q = routes[stack[b]];
            if (std::abs(p.lx - q.lx) < (p.label.width + q.label.width) / 2.0 + 8.0 && std::abs(p.ly - q.ly) < (p.label.height + q.label.height) / 2.0) crowded = true;
        }
    }
    if (!crowded) return 0.0;
    if (need > hi - lo - 2.0 * ARROW) return need - (hi - lo - 2.0 * ARROW);
    std::vector<double> along;
    double next = (lo + hi - need) / 2.0;
    for (size_t i : stack) {
        const auto& rt = routes[i];
        double size = vertical ? rt.label.height : (rt.label.width + 8.0);
        double at = next + size / 2.0;
        if (coversBox(vertical ? rt.lx : at, vertical ? at : rt.ly, rt.label.width + 8.0, rt.label.height, s, t, obstacles)) return 0.0;
        along.push_back(at);
        next += size + LABEL_GAP;
    }
    for (size_t k = 0; k < stack.size(); k++) {
        if (vertical) routes[stack[k]].ly = along[k];
        else routes[stack[k]].lx = along[k];
    }
    return 0.0;
}

inline bool widenGap(const std::vector<View*>& parents, View* s, View* t, bool vertical, double shortAmt, std::unordered_map<std::vector<double>*, std::vector<double>>& base) noexcept {
    std::unordered_set<View*> chain;
    for (View* v = s; v != nullptr; v = parents[v->index]) chain.insert(v);
    View* tb = nullptr;
    View* box = t;
    while (box != nullptr && !chain.count(box)) {
        tb = box;
        box = parents[box->index];
    }
    if (box == nullptr || tb == nullptr || box == s) return false;
    View* sb = s;
    while (parents[sb->index] != box) sb = parents[sb->index];
    int at = 0;
    if (vertical) {
        if (sb->row == tb->row) return false;
        at = std::max(sb->row, tb->row) - 1;
    } else {
        View* l = (sb->col < tb->col) ? sb : tb;
        View* r = (sb->col < tb->col) ? tb : sb;
        if (r->col < l->col + l->span) return false;
        at = r->col - 1;
    }
    auto& gaps = vertical ? box->rowGaps : box->colGaps;
    if (!base.count(&gaps)) base[&gaps] = gaps;
    if (static_cast<int>(gaps.size()) <= at) gaps.resize(at + 1, 0.0);
    const auto& orig = base[&gaps];
    double baseVal = (at < static_cast<int>(orig.size())) ? orig[at] : 0.0;
    gaps[at] = std::max(gaps[at], baseVal + shortAmt);
    return true;
}

inline Rendered drawBlock(const BlockModel& model, const Config& config, const RenderOptions& options, bool tight) noexcept {
    double size = options.fontSize;
    double edgeSize = std::round(size * 0.875);
    const TextMeasurer& measurer = getMeasurer(options);
    double pad = options.padding;
    const auto& icons = options.icons;
    double configuredGap = 12.0;
    if (const auto* blkCfg = config.get("block"); blkCfg && blkCfg->isMap()) {
        if (const auto* p = blkCfg->get("padding"); p && p->isNumber() && p->nVal >= 0.0) {
            configuredGap = std::min(p->nVal, 200.0);
        }
    }
    double gap = std::min(configuredGap, tight ? 6.0 : std::numeric_limits<double>::infinity());
    double wrap = tight ? 80.0 : 200.0;

    static const Pele::FlatMap<std::string, std::string> SHAPES = {
        {"square", "rect"}, {"round", "rounded"}, {"circle", "circle"},
        {"doublecircle", "dbl-circ"}, {"diamond", "diam"}, {"hexagon", "hex"},
        {"stadium", "stadium"}, {"subroutine", "fr-rect"}, {"cylinder", "cyl"},
        {"rect_left_inv_arrow", "odd"}, {"lean_right", "lean-r"}, {"lean_left", "lean-l"},
        {"trapezoid", "trap-b"}, {"inv_trapezoid", "trap-t"}
    };
    static const Pele::FlatSet<std::string> FIXED = {"circle", "dbl-circ", "diam"};
    static const Pele::FlatSet<std::string> WIDE = {"stadium", "hex", "odd", "lean-r", "lean-l", "trap-b", "trap-t"};

    auto styleOf = [&](const Block& block) -> ResolvedStyle {
        std::vector<std::string> declarations;
        auto add = [&](const std::vector<std::string>& styles) {
            for (const auto& st : styles) {
                for (const auto& part : splitStr(st, ';')) declarations.push_back(part);
            }
        };
        std::vector<std::string> clsNames = !block.classes.empty() ? block.classes : std::vector<std::string>{"default"};
        for (const auto& name : clsNames) {
            auto it = model.classes.find(name);
            if (it != model.classes.end()) add(it->second.styles);
        }
        if (!block.styles.empty()) add(block.styles);
        return resolveStyle(declarations);
    };

    auto makeView = [&](const Block& block, int index) -> std::unique_ptr<View> {
        auto v = std::make_unique<View>();
        v->block = &block;
        v->index = index;
        const std::string& type = block.type;
        v->style = (type == "space") ? ResolvedStyle{} : styleOf(block);
        int txtStyle = (v->style.bold ? Style::Bold : 0) | (v->style.italic ? Style::Italic : 0);
        double fontSize = v->style.hasFontSize ? v->style.fontSize : size;
        if (type == "space") {
            v->kind = ViewKind::Space;
            v->label = layoutLabel("", false, measurer, fontSize, wrap);
        } else if (type == "composite") {
            v->kind = ViewKind::Composite;
            v->label = layoutLabel(block.label, false, measurer, fontSize, tight ? titleRoom(options) : 4000.0, txtStyle);
            v->head = v->label.height > 0.0 ? (v->label.height + 4.0) : 0.0;
            v->nw = v->label.width + 2.0 * gap;
            v->nh = v->head + 2.0 * gap;
        } else if (type == "block_arrow") {
            v->kind = ViewKind::Arrow;
            v->label = layoutLabel(block.label, false, measurer, fontSize, wrap, txtStyle);
            bool blank = true;
            for (const auto& line : v->label.lines) {
                for (const auto& part : line) {
                    if (part.hasIcon || !trimStr(part.text).empty()) blank = false;
                }
            }
            v->mask = arrowMask(block.directions);
            ArrowSize s = blank ? arrowSize(v->mask, 0.0, 0.0) : arrowSize(v->mask, v->label.width, v->label.height);
            v->nw = s.w;
            v->nh = s.h;
            v->dx = s.dx;
            v->dy = s.dy;
        } else {
            v->kind = ViewKind::Leaf;
            auto it = SHAPES.find(type);
            v->shape = (it != SHAPES.end()) ? it->second : "rect";
            v->label = layoutLabel(block.label, false, measurer, fontSize, wrap, txtStyle);
            ShapeSize s = shapeSize(v->shape, v->label.width, v->label.height);
            v->nw = s.w;
            v->nh = (v->shape == "cyl") ? std::max(s.h, v->label.height + 40.0) : s.h;
            v->dy = s.dy;
        }
        v->columns = (v->kind == ViewKind::Composite) ? (block.hasColumns ? block.columns : -1) : -1;
        v->span = (v->kind == ViewKind::Space) ? 1 : (block.hasWidthInColumns ? block.widthInColumns : 1);
        v->space = (v->kind == ViewKind::Space);
        return v;
    };

    std::vector<std::unique_ptr<View>> ownedViews;
    ownedViews.push_back(makeView(model.root, 0));
    View* root = ownedViews[0].get();
    root->nw = 0.0;
    root->nh = 0.0;
    std::vector<View*> order = {root};
    std::vector<View*> parents = {nullptr};
    std::unordered_map<std::string, View*> byId;
    for (size_t i = 0; i < order.size(); i++) {
        View* parent = order[i];
        if (parent->kind != ViewKind::Composite) continue;
        for (const auto& child : parent->block->children) {
            auto uv = makeView(child, static_cast<int>(order.size()));
            View* v = uv.get();
            ownedViews.push_back(std::move(uv));
            parent->children.push_back(v);
            parents.push_back(parent);
            order.push_back(v);
            if (v->kind != ViewKind::Space) byId[child.id] = v;
        }
    }

    std::vector<View*> obstacles;
    std::vector<Box> titles;
    std::vector<double> widths;
    for (View* v : order) widths.push_back(v->nw);

    auto place = [&]() -> std::pair<double, double> {
        for (View* v : order) v->nw = widths[v->index];
        auto content = layoutCells(*root, gap);
        obstacles.clear();
        titles.clear();
        for (View* v : order) {
            if (v->kind == ViewKind::Leaf && FIXED.count(v->shape)) {
                v->bw = v->nw;
                v->bh = v->nh;
            } else if (v->kind == ViewKind::Arrow) {
                v->bw = v->span > 1 ? v->w : v->nw;
                v->bh = v->nh;
            } else {
                v->bw = v->w;
                v->bh = (v->kind == ViewKind::Leaf && WIDE.count(v->shape)) ? v->nh : v->h;
            }
            if (v->kind == ViewKind::Leaf && v->shape == "cyl") {
                v->dy = std::min(12.0, std::max(6.0, v->bw / 14.0)) / 2.0;
            }
            if (v->kind == ViewKind::Leaf || v->kind == ViewKind::Arrow) obstacles.push_back(v);
            else if (v->head > 0.0) titles.push_back({v->x, v->y - v->h / 2.0 + (v->head + gap) / 2.0 + 2.0, v->label.width, v->label.height, nullptr});
        }
        return content;
    };
    auto content = place();

    Pele::FlatMap<size_t, int> pairs;
    Pele::FlatSet<size_t> labelled;
    std::vector<std::pair<View*, View*>> ends;
    std::vector<const Block*> drawn;
    for (const auto& edge : model.edges) {
        auto sIt = byId.find(edge.start);
        auto tIt = byId.find(edge.end);
        if (sIt == byId.end() || tIt == byId.end()) continue;
        View* s = sIt->second;
        View* t = tIt->second;
        size_t key = static_cast<size_t>(std::min(s->index, t->index)) * order.size() + static_cast<size_t>(std::max(s->index, t->index));
        pairs[key]++;
        if (!edge.label.empty()) labelled.insert(key);
        ends.push_back({s, t});
        drawn.push_back(&edge);
    }

    const std::vector<View*>* check = (drawn.size() * obstacles.size() <= 2000000) ? &obstacles : nullptr;
    std::vector<Route> routes;
    std::unordered_map<size_t, std::vector<size_t>> stacks;
    std::vector<std::vector<double>> legs(drawn.size());

    auto routeAll = [&]() {
        Pele::FlatMap<size_t, int> seen;
        routes.clear();
        stacks.clear();
        for (size_t i = 0; i < drawn.size(); i++) {
            auto [s, t] = ends[i];
            size_t key = static_cast<size_t>(std::min(s->index, t->index)) * order.size() + static_cast<size_t>(std::max(s->index, t->index));
            int total = pairs[key];
            int k = seen[key]++;
            Label label = layoutLabel(drawn[i]->label, false, measurer, edgeSize, 200.0);
            std::vector<double> points;
            double lx = 0.0, ly = 0.0;
            if (s == t) {
                auto arc = blockLoopArc(*s, k + 1);
                lx = arc.px + arc.r + 6.0 + label.width / 2.0;
                ly = arc.py - arc.r / 2.0;
            } else {
                double step = labelled.count(key) ? (std::round(edgeSize * 1.5) + 2.0) : 14.0;
                points = routeBlockEdge(*s, *t, (k - (total - 1) / 2.0) * step, check);
                size_t n = points.size();
                double firstLeg = std::abs(points[2] - points[0]) + std::abs(points[3] - points[1]);
                double lastLeg = std::abs(points[n - 2] - points[n - 4]) + std::abs(points[n - 1] - points[n - 3]);
                size_t at = (n == 6 && lastLeg > firstLeg) ? 2 : 0;
                double spot = label.width > 0.0 ? labelSpot(points, at, label.width + 8.0, label.height, s, t, check) : 0.5;
                lx = points[at] + (points[at + 2] - points[at]) * spot;
                ly = points[at + 1] + (points[at + 3] - points[at + 1]) * spot;
                if (label.width > 0.0 && total > 1) {
                    legs[i] = {points[at], points[at + 1], points[at + 2], points[at + 3]};
                    stacks[key].push_back(i);
                }
            }
            routes.push_back({drawn[i], std::move(points), s == t ? (k + 1) : 0, std::move(label), lx, ly});
        }
    };

    auto settle = [&](bool grow) -> bool {
        routeAll();
        std::vector<Box> blocked;
        if (check) {
            for (const View* v : *check) blocked.push_back({v->x, v->y, v->bw, v->bh, v});
            for (const auto& tb : titles) blocked.push_back(tb);
        }
        std::unordered_map<std::vector<double>*, std::vector<double>> base;
        bool grown = false;
        for (const auto& [key, stack] : stacks) {
            if (stack.size() < 2) continue;
            auto [s, t] = ends[stack[0]];
            double shortAmt = stackLabels(stack, routes, legs, s, t, check ? &blocked : nullptr);
            bool vertical = (legs[stack[0]][0] == legs[stack[0]][2]);
            if (grow && shortAmt > 0.0 && (vertical || !tight) && widenGap(parents, s, t, vertical, std::ceil(shortAmt), base)) grown = true;
        }
        return grown;
    };

    auto overlap = [&]() -> double {
        double area = 0.0;
        for (size_t i = 0; i < routes.size(); i++) {
            const auto& p = routes[i];
            for (size_t j = i + 1; j < routes.size(); j++) {
                const auto& q = routes[j];
                double pw = (p.label.width + 8.0) / 2.0;
                double qw = (q.label.width + 8.0) / 2.0;
                double w = std::min(p.lx + pw, q.lx + qw) - std::max(p.lx - pw, q.lx - qw);
                double h = std::min(p.ly + p.label.height / 2.0, q.ly + q.label.height / 2.0) - std::max(p.ly - p.label.height / 2.0, q.ly - q.label.height / 2.0);
                if (p.label.width > 0.0 && q.label.width > 0.0 && w > 0.0 && h > 0.0) area += w * h;
            }
        }
        return area;
    };

    double minX = 0.0, minY = 0.0, maxX = 0.0, maxY = 0.0;
    auto growBounds = [&](double x, double y, double w, double h) {
        if (x - w / 2.0 < minX) minX = x - w / 2.0;
        if (x + w / 2.0 > maxX) maxX = x + w / 2.0;
        if (y - h / 2.0 < minY) minY = y - h / 2.0;
        if (y + h / 2.0 > maxY) maxY = y + h / 2.0;
    };
    auto bound = [&]() -> double {
        minX = 0.0;
        minY = 0.0;
        maxX = content.first;
        maxY = content.second;
        for (size_t i = 0; i < routes.size(); i++) {
            const auto& rt = routes[i];
            if (rt.loop > 0) {
                auto arc = blockLoopArc(*ends[i].first, rt.loop);
                growBounds(arc.px, arc.py, 2.0 * arc.r + 2.0, 2.0 * arc.r + 2.0);
            }
            if (rt.label.width > 0.0) growBounds(rt.lx, rt.ly, rt.label.width + 8.0, rt.label.height);
        }
        return maxX - minX;
    };

    if (settle(drawn.size() * drawn.size() <= 2000000)) {
        double before = overlap();
        double narrow = bound();
        bool grown = true;
        for (int round = 0; round < 3 && grown; round++) {
            content = place();
            grown = settle(round < 2);
        }
        double wide = bound();
        double room = (options.maxWidth > 0.0) ? options.maxWidth : std::numeric_limits<double>::infinity();
        if (overlap() > before - 1.0 || (wide > narrow && std::ceil(wide + 2.0 * pad) > room)) {
            for (View* v : order) {
                v->rowGaps.clear();
                v->colGaps.clear();
            }
            content = place();
            settle(false);
        }
    }

    bound();

    Label title = titleLabel(model.title, measurer, size, tight ? titleRoom(options) : 4000.0);
    double titleHeight = title.height > 0.0 ? (title.height + 12.0) : 0.0;
    double inner = std::max(maxX - minX, title.width);
    double ox = pad - minX + (inner - (maxX - minX)) / 2.0;
    double oy = pad + titleHeight - minY;
    for (View* v : order) {
        v->x += ox;
        v->y += oy;
    }
    double width = std::ceil(inner + 2.0 * pad);
    double height = std::ceil(maxY - minY + titleHeight + 2.0 * pad);

    std::string clusters;
    std::string nodes;
    std::vector<Fill> fills;
    const std::string SHAPE_ATTRS = " fill=\"var(--_s)\" stroke=\"var(--_b)\"";
    const std::string LINE_ATTRS = " stroke=\"var(--_b)\"";

    for (size_t i = 1; i < order.size(); i++) {
        const View& v = *order[i];
        if (v.kind == ViewKind::Space) continue;
        std::string joinedCls;
        for (size_t c = 0; c < v.block->classes.size(); c++) {
            if (c > 0) joinedCls.push_back(' ');
            joinedCls += v.block->classes[c];
        }
        std::string classes = classNames(joinedCls);
        std::string id = escText(v.block->id);
        if (v.kind == ViewKind::Composite) {
            double top = v.y - v.h / 2.0;
            if (paints(v.style.shape)) {
                fills.push_back({{v.x - v.w / 2.0, top, v.x + v.w / 2.0, top + v.h}, v.style.shape});
            }
            Pele::appendCat(clusters, {"<g class=\"pele-cluster", classes, "\" data-id=\"", id, "\">"
                "<rect x=\"", num(v.x - v.w / 2.0), "\" y=\"", num(top), "\" width=\"", num(v.w), "\" height=\"", num(v.h), "\" fill=\"var(--_a)\"", (setsFill(v.style.shape) ? "" : " fill-opacity=\"0.5\""), " stroke=\"var(--_b)\"", rounded(v.style.shape), "/>", labelSvg(v.label,  v.x,  top + (v.head + gap) / 2.0 + 2.0, Pele::cat({" class=\"pele-cluster-label\"", inkFill(v.style, "var(--_m)"), v.style.text}),  icons), "</g>"});
            continue;
        }
        bool isArr = (v.kind == ViewKind::Arrow);
        Pele::appendCat(nodes, {"<g class=\"pele-node pele-shape-", (isArr ? std::string("block-arrow") : v.shape), classes, "\" data-id=\"", id, "\" transform=\"translate(", num(v.x), ",", num(v.y), ")\">", (isArr
                ? drawArrow(v.mask, v.bw, v.bh, SHAPE_ATTRS + v.style.shape)
                : drawShape(v.shape, v.bw, v.bh, SHAPE_ATTRS + v.style.shape, LINE_ATTRS + v.style.line)), labelSvg(v.label,  v.dx,  v.dy, Pele::cat({" class=\"pele-label\"", ((isArr || shapeFilled(v.shape)) ? inkFill(v.style) : std::string("")), v.style.text}),  icons), "</g>"});
    }

    std::string casings;
    std::string edges;
    std::string labels;
    for (size_t i = 0; i < routes.size(); i++) {
        auto& rt = routes[i];
        const Block& edge = *rt.edge;
        const std::string& startType = edge.arrowTypeStart;
        const std::string& endType = edge.arrowTypeEnd;
        EdgePath path;
        if (rt.loop > 0) {
            path = blockLoopPath(*ends[i].first, rt.loop, markerTrim(startType), markerTrim(endType));
        } else {
            for (size_t k = 0; k + 1 < rt.points.size(); k += 2) {
                rt.points[k] += ox;
                rt.points[k + 1] += oy;
            }
            path = traceBlockEdge(rt.points, markerTrim(startType), markerTrim(endType));
        }
        auto over = overFills(path.d, {path.sx, path.sy, path.ex, path.ey}, fills);
        if (over.onFill && over.value.empty()) {
            casings += edgeCasing(path, startType, endType, "", edge.thickness == "thick");
        }
        std::string id = escText(edge.id);
        std::string strokeCol = !over.value.empty() ? over.value : "var(--_l)";
        Pele::appendCat(edges, {"<g class=\"pele-edge\" data-id=\"", id, "\"", (!over.value.empty() ? Pele::cat({" stroke=\"", over.value, "\""}) : ""), ">"
            "<path d=\"", path.d, "\"", (edge.thickness == "thick" ? " stroke-width=\"2.5\"" : ""), (edge.pattern == "dotted" ? " stroke-dasharray=\"3 4\"" : ""), "/>", marker(startType, path.sx, path.sy, path.sdx, path.sdy, strokeCol), marker(endType, path.ex, path.ey, path.edx, path.edy, strokeCol), "</g>"});
        if (rt.label.width > 0.0) {
            double x = rt.lx + ox;
            double y = rt.ly + oy;
            labels += edgeLabelSvg(edge.id, rt.label, x, y, "", icons);
        }
    }

    std::string svg = svgDocument(
        "block", 
        width, 
        height, 
        size, 
        options, 
        model, Pele::cat({labelSvg(title, width / 2.0, pad + title.height / 2.0, " class=\"pele-title\" font-weight=\"var(--_tw)\""), (!clusters.empty() ? Pele::cat({"<g class=\"pele-clusters\">", clusters, "</g>"}) : ""), casingsSvg(casings), (!edges.empty() ? Pele::cat({"<g class=\"pele-edges\" fill=\"none\" stroke=\"var(--_l)\" stroke-linecap=\"round\">", edges, "</g>"}) : ""), "<g class=\"pele-nodes\">", nodes, "</g>", (!labels.empty() ? Pele::cat({"<g class=\"pele-edge-labels\" font-size=\"", num(edgeSize), "\">", labels, "</g>"}) : "")}));

    return {svg, width, height, {}};
}

inline Rendered render(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options) {
    BlockDb db;
    db.title = title;
    parseBlock(source, db);
    return tighten(options, [&](bool tight) { return drawBlock(db, config, options, tight); });
}

} // namespace BlockDiagram
} // namespace Pele
