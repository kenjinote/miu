#pragma once
#include "../foundation.h"
#include <array>
#include <deque>

namespace Pele {
namespace C4Diagram {

// ============================================================================
// src/diagrams/c4/types.ts
// ============================================================================

struct C4Named {
    std::string key;
    std::string value;
};

struct C4Attr {
    bool isNamed = false;
    std::string str;
    C4Named named;

    static C4Attr makeStr(const std::string& s) {
        C4Attr a;
        a.isNamed = false;
        a.str = s;
        return a;
    }
    static C4Attr makeNamed(const std::string& k, const std::string& v) {
        C4Attr a;
        a.isNamed = true;
        a.named = {k, v};
        return a;
    }
};

struct C4Boundary;

struct C4Element {
    std::string alias;
    std::string label;
    std::optional<C4Named> aliasAttr;
    std::optional<C4Named> labelAttr;
    C4Boundary* parent = nullptr;
    std::optional<std::string> type;
    std::optional<std::string> descr;
    std::optional<std::string> techn;
    std::optional<std::string> sprite;
    std::optional<std::string> tags;
    std::optional<std::string> link;
    std::optional<std::string> bgColor;
    std::optional<std::string> fontColor;
    std::optional<std::string> borderColor;
    std::optional<std::string> shadowing;
    std::optional<std::string> shape;
    std::optional<std::string> legendText;
    std::optional<std::string> legendSprite;
    std::map<std::string, std::string> extra;
    virtual ~C4Element() = default;
};

struct C4Shape : C4Element {
    std::string kind;
};

struct C4Boundary : C4Element {
    std::optional<std::string> nodeType;
};

struct C4Rel {
    std::string type;
    std::string from;
    std::string to;
    std::string label;
    std::optional<C4Named> fromAttr;
    std::optional<C4Named> toAttr;
    std::optional<C4Named> labelAttr;
    std::optional<std::string> techn;
    std::optional<std::string> descr;
    std::optional<std::string> sprite;
    std::optional<std::string> tags;
    std::optional<std::string> link;
    std::optional<std::string> textColor;
    std::optional<std::string> lineColor;
    std::optional<int> offsetX;
    std::optional<int> offsetY;
    std::map<std::string, std::string> extra;
};

// ============================================================================
// src/diagrams/c4/db.ts
// ============================================================================

inline bool jsParseInt(const std::string& raw, int& out) {
    std::string s = trimStartStr(raw);
    if (s.empty()) return false;
    size_t i = 0;
    int sign = 1;
    if (s[i] == '+' || s[i] == '-') {
        if (s[i] == '-') sign = -1;
        i++;
    }
    if (i >= s.size() || !isDigit(static_cast<unsigned char>(s[i]))) return false;
    long long val = 0;
    while (i < s.size() && isDigit(static_cast<unsigned char>(s[i]))) {
        val = val * 10 + (s[i] - '0');
        if (val > 100000000LL) val = 100000000LL;
        i++;
    }
    out = static_cast<int>(val * sign);
    return true;
}

inline void setElementField(C4Element& el, const std::string& field, const std::string& val) {
    if (field == "label") el.label = val;
    else if (field == "type") el.type = val;
    else if (field == "descr") el.descr = val;
    else if (field == "techn") el.techn = val;
    else if (field == "sprite") el.sprite = val;
    else if (field == "tags") el.tags = val;
    else if (field == "link") el.link = val;
    else if (field == "bgColor") el.bgColor = val;
    else if (field == "fontColor") el.fontColor = val;
    else if (field == "borderColor") el.borderColor = val;
    else if (field == "shadowing") el.shadowing = val;
    else if (field == "shape") el.shape = val;
    else if (field == "legendText") el.legendText = val;
    else if (field == "legendSprite") el.legendSprite = val;
    else el.extra[field] = val;
}

inline void setRelField(C4Rel& rel, const std::string& field, const std::string& val) {
    if (field == "label") rel.label = val;
    else if (field == "descr") rel.descr = val;
    else if (field == "techn") rel.techn = val;
    else if (field == "sprite") rel.sprite = val;
    else if (field == "tags") rel.tags = val;
    else if (field == "link") rel.link = val;
    else if (field == "textColor") rel.textColor = val;
    else if (field == "lineColor") rel.lineColor = val;
    else rel.extra[field] = val;
}

inline void applyNamedEl(C4Element& el, const C4Named& attr) {
    setElementField(el, attr.key, attr.value);
}

inline void applyNamedRel(C4Rel& rel, const C4Named& attr) {
    if (attr.key == "offsetX" || attr.key == "offsetY") {
        int v = 0;
        if (jsParseInt(attr.value, v)) {
            if (attr.key == "offsetX") rel.offsetX = v;
            else rel.offsetY = v;
        } else {
            if (attr.key == "offsetX") rel.offsetX = 0;
            else rel.offsetY = 0;
        }
    } else {
        setRelField(rel, attr.key, attr.value);
    }
}

inline void putEl(C4Element& el, const std::string& field, const C4Attr* attr) {
    if (!attr) return;
    if (!attr->isNamed) setElementField(el, field, attr->str);
    else applyNamedEl(el, attr->named);
}

inline void putRel(C4Rel& rel, const std::string& field, const C4Attr* attr) {
    if (!attr) return;
    if (!attr->isNamed) setRelField(rel, field, attr->str);
    else applyNamedRel(rel, attr->named);
}

inline void textEl(C4Element& el, const std::string& field, const C4Attr* attr) {
    if (!attr) {
        if (field == "descr" && !el.descr.has_value()) el.descr = "";
        if (field == "techn" && !el.techn.has_value()) el.techn = "";
    } else {
        putEl(el, field, attr);
    }
}

inline void textRel(C4Rel& rel, const std::string& field, const C4Attr* attr) {
    if (!attr) {
        if (field == "descr" && !rel.descr.has_value()) rel.descr = "";
        if (field == "techn" && !rel.techn.has_value()) rel.techn = "";
    } else {
        putRel(rel, field, attr);
    }
}

inline void setLabelEl(C4Element& el, const C4Attr* attr) {
    el.labelAttr = std::nullopt;
    if (attr && attr->isNamed) {
        el.label = "";
        el.labelAttr = attr->named;
        applyNamedEl(el, attr->named);
    } else {
        el.label = attr ? attr->str : "";
    }
}

inline void setLabelRel(C4Rel& rel, const C4Attr* attr) {
    rel.labelAttr = std::nullopt;
    if (attr && attr->isNamed) {
        rel.label = "";
        rel.labelAttr = attr->named;
        applyNamedRel(rel, attr->named);
    } else {
        rel.label = attr ? attr->str : "";
    }
}

class C4Db : public DiagramMeta {
public:
    std::string c4Type;
    std::deque<C4Shape> shapes;
    std::deque<C4Boundary> boundaries;
    std::deque<C4Rel> rels;
    int shapeInRow = 4;
    int boundaryInRow = 2;

    std::unordered_map<std::string, C4Shape*> shapeIndex;
    std::unordered_map<std::string, C4Boundary*> boundaryIndex;
    std::unordered_map<std::string, std::unordered_map<std::string, C4Rel*>> relIndex;
    C4Boundary* current = nullptr;
    std::vector<C4Boundary*> stack;

    C4Db() {
        C4Boundary root;
        root.alias = "global";
        root.label = "global";
        root.type = "global";
        root.parent = nullptr;
        boundaries.push_back(std::move(root));
        current = &boundaries[0];
    }

    static const C4Attr* at(const std::vector<C4Attr>& attrs, size_t idx) {
        return idx < attrs.size() ? &attrs[idx] : nullptr;
    }

    void setC4Type(const std::string& t) {
        c4Type = t;
    }

    void setTitle(const std::string& t) {
        title = t;
    }

    void setAccDescription(const std::string& descr) {
        std::string out;
        size_t i = 0;
        while (i < descr.size()) {
            if (descr[i] == '\n') {
                out.push_back('\n');
                i++;
                while (i < descr.size()) {
                    size_t next = i;
                    int cp = charCodeAtUtf8(descr, i, &next);
                    if (!isSpace(cp)) break;
                    i = next;
                }
            } else {
                out.push_back(descr[i++]);
            }
        }
        accDescr = out;
    }

    template <typename E>
    E& getOrCreateElement(std::deque<E>& list, std::unordered_map<std::string, E*>& index, const C4Attr* alias, const C4Attr* titleAttr) {
        E* el = (alias && !alias->isNamed) ? (index.count(alias->str) ? index[alias->str] : nullptr) : nullptr;
        if (!el) {
            list.emplace_back();
            el = &list.back();
            if (alias && !alias->isNamed) {
                el->alias = alias->str;
                index[alias->str] = el;
            } else if (alias && alias->isNamed) {
                el->aliasAttr = alias->named;
                applyNamedEl(*el, alias->named);
            }
        }
        setLabelEl(*el, titleAttr);
        return *el;
    }

    void finishShape(C4Shape& shape, const std::string& kind, const std::vector<C4Attr>& attrs, size_t idx) {
        putEl(shape, "sprite", at(attrs, idx));
        putEl(shape, "tags", at(attrs, idx + 1));
        putEl(shape, "link", at(attrs, idx + 2));
        shape.kind = kind;
        shape.parent = current;
    }

    void addPersonOrSystem(const std::string& kind, const std::vector<C4Attr>& attrs) {
        auto& shape = getOrCreateElement(shapes, shapeIndex, at(attrs, 0), at(attrs, 1));
        textEl(shape, "descr", at(attrs, 2));
        finishShape(shape, kind, attrs, 3);
    }

    void addContainer(const std::string& kind, const std::vector<C4Attr>& attrs) {
        auto& shape = getOrCreateElement(shapes, shapeIndex, at(attrs, 0), at(attrs, 1));
        textEl(shape, "techn", at(attrs, 2));
        textEl(shape, "descr", at(attrs, 3));
        finishShape(shape, kind, attrs, 4);
    }

    void addComponent(const std::string& kind, const std::vector<C4Attr>& attrs) {
        addContainer(kind, attrs);
    }

    C4Boundary& makeBoundary(const std::vector<C4Attr>& attrs, const std::string& fallback) {
        auto& b = getOrCreateElement(boundaries, boundaryIndex, at(attrs, 0), at(attrs, 1));
        if (!at(attrs, 2)) b.type = fallback;
        else putEl(b, "type", at(attrs, 2));
        return b;
    }

    void openBoundary(C4Boundary& b) {
        b.parent = current;
        stack.push_back(current);
        current = &b;
    }

    void addPersonOrSystemBoundary(const std::vector<C4Attr>& attrs, const std::string& fallback = "system") {
        auto& b = makeBoundary(attrs, fallback);
        putEl(b, "tags", at(attrs, 3));
        putEl(b, "link", at(attrs, 4));
        openBoundary(b);
    }

    void addContainerBoundary(const std::vector<C4Attr>& attrs) {
        addPersonOrSystemBoundary(attrs, "container");
    }

    void addDeploymentNode(const std::string& nodeType, const std::vector<C4Attr>& attrs) {
        auto& b = makeBoundary(attrs, "node");
        textEl(b, "descr", at(attrs, 3));
        putEl(b, "sprite", at(attrs, 4));
        putEl(b, "tags", at(attrs, 5));
        putEl(b, "link", at(attrs, 6));
        b.nodeType = nodeType;
        openBoundary(b);
    }

    void popBoundaryParseStack() {
        if (!stack.empty()) {
            current = stack.back();
            stack.pop_back();
        } else {
            current = &boundaries[0];
        }
    }

    void addRel(const std::string& type, const std::vector<C4Attr>& attrs) {
        const C4Attr* from = at(attrs, 0);
        const C4Attr* to = at(attrs, 1);
        const C4Attr* lbl = at(attrs, 2);
        if (!from || !to || !lbl) return;
        bool known = !from->isNamed && !to->isNamed;
        C4Rel* rel = nullptr;
        if (known) {
            auto it1 = relIndex.find(from->str);
            if (it1 != relIndex.end()) {
                auto it2 = it1->second.find(to->str);
                if (it2 != it1->second.end()) rel = it2->second;
            }
        }
        if (!rel) {
            rels.emplace_back();
            rel = &rels.back();
            rel->type = type;
            if (known) {
                relIndex[from->str][to->str] = rel;
            }
            if (!from->isNamed) rel->from = from->str;
            else rel->fromAttr = from->named;
            if (!to->isNamed) rel->to = to->str;
            else rel->toAttr = to->named;
        }
        rel->type = type;
        setLabelRel(*rel, lbl);
        textRel(*rel, "techn", at(attrs, 3));
        textRel(*rel, "descr", at(attrs, 4));
        putRel(*rel, "sprite", at(attrs, 5));
        putRel(*rel, "tags", at(attrs, 6));
        putRel(*rel, "link", at(attrs, 7));
    }

    C4Element* getC4Shape(const std::string& alias) {
        auto itS = shapeIndex.find(alias);
        if (itS != shapeIndex.end()) return itS->second;
        auto itB = boundaryIndex.find(alias);
        if (itB != boundaryIndex.end()) return itB->second;
        return nullptr;
    }

    const C4Element* getC4Shape(const std::string& alias) const {
        auto itS = shapeIndex.find(alias);
        if (itS != shapeIndex.end()) return itS->second;
        auto itB = boundaryIndex.find(alias);
        if (itB != boundaryIndex.end()) return itB->second;
        return nullptr;
    }

    void updateElStyle(const std::vector<C4Attr>& attrs) {
        const C4Attr* a0 = at(attrs, 0);
        C4Element* el = (a0 && !a0->isNamed) ? getC4Shape(a0->str) : nullptr;
        if (!el) return;
        putEl(*el, "bgColor", at(attrs, 1));
        putEl(*el, "fontColor", at(attrs, 2));
        putEl(*el, "borderColor", at(attrs, 3));
        putEl(*el, "shadowing", at(attrs, 4));
        putEl(*el, "shape", at(attrs, 5));
        putEl(*el, "sprite", at(attrs, 6));
        putEl(*el, "techn", at(attrs, 7));
        putEl(*el, "legendText", at(attrs, 8));
        putEl(*el, "legendSprite", at(attrs, 9));
    }

    void updateRelStyle(const std::vector<C4Attr>& attrs) {
        const C4Attr* from = at(attrs, 0);
        const C4Attr* to = at(attrs, 1);
        if (!from || !to || from->isNamed || to->isNamed) return;
        auto it1 = relIndex.find(from->str);
        if (it1 == relIndex.end()) return;
        auto it2 = it1->second.find(to->str);
        if (it2 == it1->second.end()) return;
        C4Rel& rel = *it2->second;
        putRel(rel, "textColor", at(attrs, 2));
        putRel(rel, "lineColor", at(attrs, 3));
        if (const C4Attr* x = at(attrs, 4)) {
            if (!x->isNamed) {
                int v = 0;
                rel.offsetX = jsParseInt(x->str, v) ? v : 0;
            } else {
                applyNamedRel(rel, x->named);
            }
        }
        if (const C4Attr* y = at(attrs, 5)) {
            if (!y->isNamed) {
                int v = 0;
                rel.offsetY = jsParseInt(y->str, v) ? v : 0;
            } else {
                applyNamedRel(rel, y->named);
            }
        }
    }

    void updateLayoutConfig(const std::vector<C4Attr>& attrs) {
        auto count = [](const C4Attr* attr) -> int {
            if (!attr) return -1;
            int v = 0;
            const std::string& s = attr->isNamed ? attr->named.value : attr->str;
            return jsParseInt(s, v) ? v : -1;
        };
        int sh = count(at(attrs, 0));
        int bd = count(at(attrs, 1));
        if (sh >= 1) shapeInRow = sh;
        if (bd >= 1) boundaryInRow = bd;
    }
};

// ============================================================================
// src/diagrams/c4/tokens.ts & lexer.ts & parser.ts
// ============================================================================

namespace T {
    enum Type {
        END,
        ERROR,
        direction_tb,
        direction_bt,
        direction_rl,
        direction_lr,
        title,
        accDescription,
        acc_title,
        acc_title_value,
        acc_descr,
        acc_descr_value,
        acc_descr_multiline_value,
        NEWLINE,
        C4_CONTEXT,
        C4_CONTAINER,
        C4_COMPONENT,
        C4_DYNAMIC,
        C4_DEPLOYMENT,
        PERSON,
        PERSON_EXT,
        SYSTEM,
        SYSTEM_DB,
        SYSTEM_QUEUE,
        SYSTEM_EXT,
        SYSTEM_EXT_DB,
        SYSTEM_EXT_QUEUE,
        CONTAINER,
        CONTAINER_DB,
        CONTAINER_QUEUE,
        CONTAINER_EXT,
        CONTAINER_EXT_DB,
        CONTAINER_EXT_QUEUE,
        COMPONENT,
        COMPONENT_DB,
        COMPONENT_QUEUE,
        COMPONENT_EXT,
        COMPONENT_EXT_DB,
        COMPONENT_EXT_QUEUE,
        ENTERPRISE_BOUNDARY,
        SYSTEM_BOUNDARY,
        BOUNDARY,
        CONTAINER_BOUNDARY,
        NODE,
        NODE_L,
        NODE_R,
        REL,
        BIREL,
        REL_U,
        REL_D,
        REL_L,
        REL_R,
        REL_B,
        REL_INDEX,
        UPDATE_EL_STYLE,
        UPDATE_REL_STYLE,
        UPDATE_LAYOUT_CONFIG,
        EOF_IN_STRUCT,
        ATTRIBUTE_EMPTY,
        STR,
        STR_KEY,
        STR_VALUE,
        LBRACE,
        RBRACE,
        EOF_TOK,
    };
}

inline const std::vector<std::string>& tokenNames() {
    static const std::vector<std::string> names = {
        "$end", "INVALID", "direction_tb", "direction_bt", "direction_rl", "direction_lr",
        "title", "accDescription", "acc_title", "acc_title_value", "acc_descr", "acc_descr_value",
        "acc_descr_multiline_value", "NEWLINE", "C4_CONTEXT", "C4_CONTAINER", "C4_COMPONENT",
        "C4_DYNAMIC", "C4_DEPLOYMENT", "PERSON", "PERSON_EXT", "SYSTEM", "SYSTEM_DB",
        "SYSTEM_QUEUE", "SYSTEM_EXT", "SYSTEM_EXT_DB", "SYSTEM_EXT_QUEUE", "CONTAINER",
        "CONTAINER_DB", "CONTAINER_QUEUE", "CONTAINER_EXT", "CONTAINER_EXT_DB",
        "CONTAINER_EXT_QUEUE", "COMPONENT", "COMPONENT_DB", "COMPONENT_QUEUE",
        "COMPONENT_EXT", "COMPONENT_EXT_DB", "COMPONENT_EXT_QUEUE", "ENTERPRISE_BOUNDARY",
        "SYSTEM_BOUNDARY", "BOUNDARY", "CONTAINER_BOUNDARY", "NODE", "NODE_L", "NODE_R",
        "REL", "BIREL", "REL_U", "REL_D", "REL_L", "REL_R", "REL_B", "REL_INDEX",
        "UPDATE_EL_STYLE", "UPDATE_REL_STYLE", "UPDATE_LAYOUT_CONFIG", "EOF_IN_STRUCT",
        "ATTRIBUTE_EMPTY", "STR", "STR_KEY", "STR_VALUE", "LBRACE", "RBRACE", "EOF"
    };
    return names;
}

inline const std::unordered_map<std::string, int>& c4Keywords() {
    static const std::unordered_map<std::string, int> m = {
        {"C4Context", T::C4_CONTEXT},
        {"C4Container", T::C4_CONTAINER},
        {"C4Component", T::C4_COMPONENT},
        {"C4Dynamic", T::C4_DYNAMIC},
        {"C4Deployment", T::C4_DEPLOYMENT},
        {"Person", T::PERSON},
        {"Person_Ext", T::PERSON_EXT},
        {"System", T::SYSTEM},
        {"SystemDb", T::SYSTEM_DB},
        {"SystemQueue", T::SYSTEM_QUEUE},
        {"System_Ext", T::SYSTEM_EXT},
        {"SystemDb_Ext", T::SYSTEM_EXT_DB},
        {"SystemQueue_Ext", T::SYSTEM_EXT_QUEUE},
        {"Container", T::CONTAINER},
        {"ContainerDb", T::CONTAINER_DB},
        {"ContainerQueue", T::CONTAINER_QUEUE},
        {"Container_Ext", T::CONTAINER_EXT},
        {"ContainerDb_Ext", T::CONTAINER_EXT_DB},
        {"ContainerQueue_Ext", T::CONTAINER_EXT_QUEUE},
        {"Component", T::COMPONENT},
        {"ComponentDb", T::COMPONENT_DB},
        {"ComponentQueue", T::COMPONENT_QUEUE},
        {"Component_Ext", T::COMPONENT_EXT},
        {"ComponentDb_Ext", T::COMPONENT_EXT_DB},
        {"ComponentQueue_Ext", T::COMPONENT_EXT_QUEUE},
        {"Enterprise_Boundary", T::ENTERPRISE_BOUNDARY},
        {"System_Boundary", T::SYSTEM_BOUNDARY},
        {"Boundary", T::BOUNDARY},
        {"Container_Boundary", T::CONTAINER_BOUNDARY},
        {"Deployment_Node", T::NODE},
        {"Node", T::NODE},
        {"Node_L", T::NODE_L},
        {"Node_R", T::NODE_R},
        {"Rel", T::REL},
        {"BiRel", T::BIREL},
        {"Rel_Up", T::REL_U},
        {"Rel_U", T::REL_U},
        {"Rel_Down", T::REL_D},
        {"Rel_D", T::REL_D},
        {"Rel_Left", T::REL_L},
        {"Rel_L", T::REL_L},
        {"Rel_Right", T::REL_R},
        {"Rel_R", T::REL_R},
        {"Rel_Back", T::REL_B},
        {"RelIndex", T::REL_INDEX},
        {"UpdateElementStyle", T::UPDATE_EL_STYLE},
        {"UpdateRelStyle", T::UPDATE_REL_STYLE},
        {"UpdateLayoutConfig", T::UPDATE_LAYOUT_CONFIG},
    };
    return m;
}

struct C4Tokens {
    std::vector<int> types;
    std::vector<size_t> starts;
    std::vector<size_t> ends;
};

inline C4Tokens tokenizeC4(const std::string& src) {
    enum S {
        INITIAL,
        struct_s,
        attribute,
        string_s,
        string_kv,
        string_kv_key,
        string_kv_value,
        acc,
        acc_descr_multiline
    };
    size_t n = src.size();
    C4Tokens out;
    int state = INITIAL;
    int accValue = T::acc_title_value;
    size_t p = 0;
    bool done = false;

    auto charAt = [&](size_t idx) -> int {
        if (idx >= n) return -1;
        size_t dummy = idx;
        return charCodeAtUtf8(src, idx, &dummy);
    };
    auto byteAt = [&](size_t idx) -> int {
        if (idx >= n) return -1;
        return static_cast<unsigned char>(src[idx]);
    };
    auto emit = [&](int type, size_t s, size_t e) {
        out.types.push_back(type);
        out.starts.push_back(s);
        out.ends.push_back(e);
        p = e;
    };
    auto skipWs = [&](size_t q) -> size_t {
        while (q < n) {
            size_t next = q;
            int cp = charCodeAtUtf8(src, q, &next);
            if (!isSpace(cp)) break;
            q = next;
        }
        return q;
    };
    auto upTo = [&](char ch, size_t from) -> size_t {
        size_t q = src.find(ch, from);
        return q == std::string::npos ? n : q;
    };
    auto statement = [&](const std::string& word, int type) -> bool {
        size_t q = p + word.size();
        if (!startsWith(src, word, p) || !isSpace(charAt(q))) return false;
        size_t next = q;
        charCodeAtUtf8(src, q, &next);
        size_t from = next;
        q = from;
        while (q < n) {
            int c = byteAt(q);
            if (c == 35 || c == 10 || c == 59) break;
            q++;
        }
        if (q == from) return false;
        emit(type, p, q);
        return true;
    };

    const auto& kw = c4Keywords();
    static const char* DIRS[4] = {"TB", "BT", "RL", "LR"};

    while (true) {
        if (done) {
            emit(T::END, n, n);
            break;
        }
        if (p >= n) done = true;
        int c = charAt(p);

        if (state == INITIAL) {
            if (p >= n) {
                emit(T::EOF_TOK, n, n);
                continue;
            }
            // Check RE_DIRECTION on current line
            size_t lEnd = p;
            while (lEnd < n) {
                size_t next = lEnd;
                int cp = charCodeAtUtf8(src, lEnd, &next);
                if (isLineEnd(cp)) break;
                lEnd = next;
            }
            size_t dirPos = src.find("direction", p);
            if (dirPos != std::string::npos && dirPos < lEnd) {
                bool matchedDir = false;
                for (int dIdx = 0; dIdx < 4 && !matchedDir; dIdx++) {
                    // Check if any `direction\s+<DIR>` exists in [p, lEnd)
                    size_t searchFrom = dirPos;
                    while (searchFrom < lEnd) {
                        size_t found = src.find("direction", searchFrom);
                        if (found == std::string::npos || found >= lEnd) break;
                        size_t afterDir = found + 9;
                        size_t afterWs = afterDir;
                        while (afterWs < lEnd) {
                            size_t next = afterWs;
                            int cp = charCodeAtUtf8(src, afterWs, &next);
                            if (!isSpace(cp)) break;
                            afterWs = next;
                        }
                        if (afterWs > afterDir && afterWs + 2 <= lEnd && src.compare(afterWs, 2, DIRS[dIdx]) == 0) {
                            size_t endMatch = upTo('\n', afterWs + 2);
                            emit(T::direction_tb + dIdx, p, endMatch);
                            matchedDir = true;
                            break;
                        }
                        searchFrom = found + 1;
                    }
                }
                if (matchedDir) continue;
            }
            if (c == 116) { // 't'
                // RE_BARE_TITLE = /title[^\S\r\n]*(?=[\r\n]|$)\s*/y
                if (startsWith(src, "title", p)) {
                    size_t q = p + 5;
                    while (q < n) {
                        size_t next = q;
                        int cp = charCodeAtUtf8(src, q, &next);
                        if (!isSpace(cp) || cp == 10 || cp == 13) break;
                        q = next;
                    }
                    if (q >= n || byteAt(q) == 10 || byteAt(q) == 13) {
                        p = skipWs(q);
                        continue;
                    }
                }
                if (statement("title", T::title)) continue;
            }
            if (c == 97) { // 'a'
                if (statement("accDescription", T::accDescription)) continue;
                bool isTitle = startsWith(src, "accTitle", p);
                if (isTitle || startsWith(src, "accDescr", p)) {
                    size_t q = skipWs(p + 8);
                    int d = byteAt(q);
                    if (d == 58) {
                        emit(isTitle ? T::acc_title : T::acc_descr, p, skipWs(q + 1));
                        accValue = isTitle ? T::acc_title_value : T::acc_descr_value;
                        state = acc;
                        continue;
                    }
                    if (d == 123 && !isTitle) {
                        p = skipWs(q + 1);
                        state = acc_descr_multiline;
                        continue;
                    }
                }
            }
            if (c == 37 && byteAt(p + 1) == 37) {
                size_t q = upTo('\n', p + 2);
                while (q < n && (byteAt(q) == 10 || byteAt(q) == 13)) q++;
                p = q;
                continue;
            }
            if (isSpace(c)) {
                size_t q = p;
                int lastNl = -1;
                while (q < n) {
                    size_t next = q;
                    int d = charCodeAtUtf8(src, q, &next);
                    if (!isSpace(d)) break;
                    if (d == 10) lastNl = static_cast<int>(q);
                    q = next;
                }
                if (lastNl != -1) emit(T::NEWLINE, p, static_cast<size_t>(lastNl + 1));
                else p = q;
                continue;
            }
            if (c == 123) {
                emit(T::LBRACE, p, p + 1);
                continue;
            }
            if (c == 125) {
                emit(T::RBRACE, p, p + 1);
                continue;
            }
            if (isWord(c)) {
                size_t q = p + 1;
                while (q < n && isWord(byteAt(q))) q++;
                if (q - p <= 19) {
                    auto it = kw.find(src.substr(p, q - p));
                    if (it != kw.end()) {
                        if (it->second > T::C4_DEPLOYMENT) state = struct_s;
                        emit(it->second, p, q);
                        continue;
                    }
                }
            }
            emit(T::ERROR, p, p);
            break;
        }

        if (state == struct_s) {
            if (p >= n) {
                emit(T::EOF_IN_STRUCT, n, n);
            } else if (c == 40) {
                size_t q = p + 1;
                while (byteAt(q) == 32) q++;
                if (byteAt(q) == 44) emit(T::ATTRIBUTE_EMPTY, p, q + 1);
                else p++;
                state = attribute;
            } else if (c == 41) {
                p++;
                state = INITIAL;
            } else {
                emit(T::ERROR, p, p);
                break;
            }
            continue;
        }

        if (state == attribute) {
            if (p >= n) continue;
            if (c == 41) {
                p++;
                state = INITIAL;
            } else if (c == 44) {
                if (byteAt(p + 1) == 44) emit(T::ATTRIBUTE_EMPTY, p, p + 2);
                else p++;
            } else {
                size_t q = p;
                while (byteAt(q) == 32) q++;
                int d = byteAt(q);
                if (d == 34) {
                    if (byteAt(q + 1) == 34) {
                        emit(T::ATTRIBUTE_EMPTY, p, q + 2);
                    } else {
                        p = q + 1;
                        state = string_s;
                    }
                } else if (d == 36) {
                    p = q + 1;
                    state = string_kv;
                } else {
                    emit(T::STR, p, upTo(',', p));
                }
            }
            continue;
        }

        if (state == string_s) {
            if (c == 34) {
                p++;
                state = attribute;
            } else {
                emit(T::STR, p, upTo('"', p));
            }
            continue;
        }

        if (state == string_kv) {
            emit(T::STR_KEY, p, upTo('=', p));
            state = string_kv_key;
            continue;
        }

        if (state == string_kv_key) {
            if (p >= n) continue;
            size_t q = p + 1;
            if (c == 61) while (byteAt(q) == 32) q++;
            if (c != 61 || byteAt(q) != 34) {
                emit(T::ERROR, p, p);
                break;
            }
            p = q + 1;
            state = string_kv_value;
            continue;
        }

        if (state == string_kv_value) {
            if (p >= n) continue;
            if (c == 34) {
                p++;
                state = attribute;
            } else {
                emit(T::STR_VALUE, p, upTo('"', p));
            }
            continue;
        }

        if (state == acc) {
            emit(accValue, p, upTo('\n', p));
            state = INITIAL;
            continue;
        }

        if (c == 125) {
            p++;
            state = INITIAL;
        } else {
            emit(T::acc_descr_multiline_value, p, upTo('}', p));
        }
    }
    return out;
}

inline void parseC4(const std::string& src, C4Db& db) {
    static const char* KINDS[] = {
        "person", "external_person", "system", "system_db", "system_queue",
        "external_system", "external_system_db", "external_system_queue",
        "container", "container_db", "container_queue", "external_container",
        "external_container_db", "external_container_queue", "component",
        "component_db", "component_queue", "external_component",
        "external_component_db", "external_component_queue"
    };
    static const char* REL_TYPES[] = {"rel", "birel", "rel_u", "rel_d", "rel_l", "rel_r", "rel_b", "rel"};
    static const char* NODE_TYPES[] = {"node", "nodeL", "nodeR"};
    static const std::string HEADERS = "'direction_tb', 'direction_bt', 'direction_rl', 'direction_lr', 'C4_CONTEXT', 'C4_CONTAINER', 'C4_COMPONENT', 'C4_DYNAMIC', 'C4_DEPLOYMENT'";
    static const std::string ATTRIBUTES = "'STR', 'STR_KEY', 'ATTRIBUTE', 'ATTRIBUTE_EMPTY'";

    enum PState {
        Item,
        Diagram,
        ItemOrEnd,
        AfterOther,
        AfterDiagram
    };

    auto isOther = [](int t) {
        return t == T::title || t == T::accDescription || t == T::acc_title || t == T::acc_descr || t == T::acc_descr_multiline_value;
    };
    auto isDiagram = [](int t) {
        return t >= T::PERSON && t <= T::UPDATE_LAYOUT_CONFIG;
    };

    C4Tokens tok = tokenizeC4(src);
    const auto& types = tok.types;
    const auto& starts = tok.starts;
    const auto& ends = tok.ends;
    const auto& names = tokenNames();
    size_t i = 0;

    auto text = [&](size_t at) -> std::string {
        return src.substr(starts[at], ends[at] - starts[at]);
    };
    auto fail = [&](const std::string& expected) {
        int t = types[i];
        if (t == T::ERROR) throw syntaxError("c4", src, starts[i], "", true);
        throw syntaxError("c4", src, starts[i], "Expecting " + expected + ", got '" + names[t] + "'");
    };
    auto expect = [&](int type) {
        if (types[i] != type) fail("'" + names[type] + "'");
        i++;
    };

    auto attributes = [&]() -> std::vector<C4Attr> {
        std::vector<C4Attr> attrs;
        while (true) {
            int t = types[i];
            if (t == T::STR) {
                attrs.push_back(C4Attr::makeStr(trimStr(text(i++))));
            } else if (t == T::ATTRIBUTE_EMPTY) {
                attrs.push_back(C4Attr::makeStr(""));
                i++;
            } else if (t == T::STR_KEY) {
                i++;
                if (types[i] != T::STR_VALUE) fail("'STR_VALUE'");
                attrs.push_back(C4Attr::makeNamed(trimStr(text(i - 1)), trimStr(text(i))));
                i++;
            } else {
                break;
            }
        }
        if (attrs.empty()) fail(ATTRIBUTES);
        return attrs;
    };

    auto other = [&](int t) {
        if (t == T::title) {
            std::string s = text(i++);
            db.setTitle(s.size() >= 6 ? s.substr(6) : "");
        } else if (t == T::accDescription) {
            std::string s = text(i++);
            db.setAccDescription(s.size() >= 15 ? s.substr(15) : "");
        } else if (t == T::acc_descr_multiline_value) {
            db.setAccDescription(trimStr(text(i++)));
        } else {
            i++;
            if (t == T::acc_title) {
                if (types[i] != T::acc_title_value) fail("'acc_title_value'");
                db.setTitle(trimStr(text(i++)));
            } else {
                if (types[i] != T::acc_descr_value) fail("'acc_descr_value'");
                db.setAccDescription(trimStr(text(i++)));
            }
        }
    };

    auto diagram = [&](int t) -> bool {
        i++;
        auto attrs = attributes();
        if (t <= T::SYSTEM_EXT_QUEUE) {
            db.addPersonOrSystem(KINDS[t - T::PERSON], attrs);
        } else if (t <= T::CONTAINER_EXT_QUEUE) {
            db.addContainer(KINDS[t - T::PERSON], attrs);
        } else if (t <= T::COMPONENT_EXT_QUEUE) {
            db.addComponent(KINDS[t - T::PERSON], attrs);
        } else if (t <= T::NODE_R) {
            auto insertAt2 = [&](const std::string& val) {
                auto pos = attrs.begin() + std::min<size_t>(2, attrs.size());
                attrs.insert(pos, C4Attr::makeStr(val));
            };
            if (t == T::ENTERPRISE_BOUNDARY) insertAt2("ENTERPRISE");
            else if (t == T::SYSTEM_BOUNDARY) insertAt2("SYSTEM");
            else if (t == T::CONTAINER_BOUNDARY) insertAt2("CONTAINER");
            if (t >= T::NODE) db.addDeploymentNode(NODE_TYPES[t - T::NODE], attrs);
            else if (t == T::CONTAINER_BOUNDARY) db.addContainerBoundary(attrs);
            else db.addPersonOrSystemBoundary(attrs);
            if (types[i] == T::LBRACE) {
                i++;
                expect(T::NEWLINE);
            } else if (types[i] == T::NEWLINE) {
                i++;
                expect(T::LBRACE);
                if (types[i] == T::NEWLINE) i++;
            } else {
                fail("'NEWLINE', 'LBRACE'");
            }
            return true;
        } else if (t <= T::REL_INDEX) {
            if (t == T::REL_INDEX && !attrs.empty()) attrs.erase(attrs.begin());
            db.addRel(REL_TYPES[t - T::REL], attrs);
        } else if (t == T::UPDATE_EL_STYLE) {
            db.updateElStyle(attrs);
        } else if (t == T::UPDATE_REL_STYLE) {
            db.updateRelStyle(attrs);
        } else {
            db.updateLayoutConfig(attrs);
        }
        return false;
    };

    int header = types[i];
    if (header < T::C4_CONTEXT || header > T::C4_DEPLOYMENT) fail(HEADERS);
    i++;
    expect(T::NEWLINE);

    int depth = 0;
    int state = Item;
    while (true) {
        int t = types[i];
        if (state == AfterOther || state == AfterDiagram) {
            if (t == T::NEWLINE) {
                i++;
                state = ItemOrEnd;
                continue;
            }
            if (state == AfterOther && isDiagram(t)) {
                state = Diagram;
                continue;
            }
        } else if (isDiagram(t)) {
            if (diagram(t)) {
                depth++;
                state = Diagram;
            } else {
                state = AfterDiagram;
            }
            continue;
        } else if (isOther(t) && state != Diagram) {
            other(t);
            state = AfterOther;
            continue;
        } else if (state != ItemOrEnd) {
            fail("'title', 'accDescription', 'acc_title', 'acc_descr', 'acc_descr_multiline_value', 'PERSON', 'REL'");
        }

        if (depth > 0) {
            if (t != T::RBRACE) fail("'NEWLINE', 'RBRACE'");
            i++;
            db.popBoundaryParseStack();
            depth--;
            state = AfterDiagram;
        } else {
            if (t != T::EOF_TOK) fail("'NEWLINE', 'EOF'");
            i++;
            expect(T::END);
            db.setC4Type(text(0));
            return;
        }
    }
}

// ============================================================================
// src/diagrams/c4/route.ts
// ============================================================================

struct Box {
    double x = 0;
    double y = 0;
    double w = 0;
    double h = 0;
    std::string form = "rect";
    virtual ~Box() = default;
};

struct Route {
    std::string d;
    double sx = 0;
    double sy = 0;
    double sdx = 0;
    double sdy = 0;
    double ex = 0;
    double ey = 0;
    double edx = 0;
    double edy = 0;
    double x0 = 0;
    double y0 = 0;
    double x1 = 0;
    double y1 = 0;
    double nx = 0;
    double ny = 0;
    double lean = 0;
    double room = 0;
    std::vector<double> points;
};

constexpr double LOOP = 28.0;
constexpr double LANE = 20.0;
constexpr double LANE_STEP = 7.0;
constexpr double CORNER = 8.0;
constexpr double LEAN = 30.0;
constexpr double NARROW = 88.0;
constexpr double WIDE = 160.0;
constexpr double LABEL_PAD = 8.0;
constexpr double HEAD = 26.0;
constexpr double NEAR = 6.0;
constexpr double FRAME_NEAR = 3.0;
constexpr double PORT_STEP = 12.0;

inline double cylRy(double w) {
    return std::min(12.0, std::max(6.0, w / 14.0));
}

inline double queueRx(double h) {
    return std::min(12.0, h / 4.0);
}

inline double mathSign(double v) {
    return v > 0.0 ? 1.0 : v < 0.0 ? -1.0 : 0.0;
}

inline double hitRect(double x0, double y0, double x1, double y1, double px, double py, double dx, double dy) {
    double enter = 0.0;
    double leave = 1.0;
    constexpr double INF = std::numeric_limits<double>::infinity();
    if (dx == 0.0) {
        if (px < x0 || px > x1) return INF;
    } else {
        double a = (x0 - px) / dx;
        double b = (x1 - px) / dx;
        enter = std::max(enter, std::min(a, b));
        leave = std::min(leave, std::max(a, b));
    }
    if (dy == 0.0) {
        if (py < y0 || py > y1) return INF;
    } else {
        double a = (y0 - py) / dy;
        double b = (y1 - py) / dy;
        enter = std::max(enter, std::min(a, b));
        leave = std::min(leave, std::max(a, b));
    }
    return enter <= leave ? enter : INF;
}

inline double hitEllipse(double cx, double cy, double rx, double ry, double px, double py, double dx, double dy) {
    constexpr double INF = std::numeric_limits<double>::infinity();
    double ux = dx / rx;
    double uy = dy / ry;
    double vx = (px - cx) / rx;
    double vy = (py - cy) / ry;
    double a = ux * ux + uy * uy;
    double b = 2.0 * (ux * vx + uy * vy);
    double c = vx * vx + vy * vy - 1.0;
    double disc = b * b - 4.0 * a * c;
    if (a == 0.0 || disc < 0.0) return INF;
    double t = (-b - std::sqrt(disc)) / (2.0 * a);
    return (t >= 0.0 && t <= 1.0) ? t : (c <= 0.0 ? 0.0 : INF);
}

inline double hit(const Box& box, double px, double py, double qx, double qy) {
    double x = box.x, y = box.y, w = box.w, h = box.h;
    double dx = qx - px;
    double dy = qy - py;
    if (box.form == "person") {
        return std::min(hitRect(x, y + HEAD, x + w, y + h, px, py, dx, dy), hitEllipse(x + w / 2.0, y + 14.0, 13.0, 13.0, px, py, dx, dy));
    }
    if (box.form == "cyl") {
        double ry = cylRy(w);
        return std::min({
            hitRect(x, y + ry, x + w, y + h - ry, px, py, dx, dy),
            hitEllipse(x + w / 2.0, y + ry, w / 2.0, ry, px, py, dx, dy),
            hitEllipse(x + w / 2.0, y + h - ry, w / 2.0, ry, px, py, dx, dy)
        });
    }
    if (box.form == "h-cyl") {
        double rx = queueRx(h);
        return std::min({
            hitRect(x + rx, y, x + w - rx, y + h, px, py, dx, dy),
            hitEllipse(x + rx, y + h / 2.0, rx, h / 2.0, px, py, dx, dy),
            hitEllipse(x + w - rx, y + h / 2.0, rx, h / 2.0, px, py, dx, dy)
        });
    }
    return hitRect(x, y, x + w, y + h, px, py, dx, dy);
}

inline bool crosses(const std::vector<double>& points, double x0, double y0, double x1, double y1) {
    for (size_t i = 2; i + 1 < points.size(); i += 2) {
        double px = points[i - 2];
        double py = points[i - 1];
        if (hitRect(x0, y0, x1, y1, px, py, points[i] - px, points[i + 1] - py) <= 1.0) return true;
    }
    return false;
}

inline bool contains(const Box& outer, const Box& inner) {
    return outer.x <= inner.x &&
           outer.y <= inner.y &&
           outer.x + outer.w >= inner.x + inner.w &&
           outer.y + outer.h >= inner.y + inner.h;
}

inline std::array<double, 4> connect(const Box& a, const Box& b, double shift) {
    double ax = a.x + a.w / 2.0;
    double ay = a.y + a.h / 2.0;
    double bx = b.x + b.w / 2.0;
    double by = b.y + b.h / 2.0;
    bool down = contains(a, b);
    if (down || contains(b, a)) {
        const Box& outer = down ? a : b;
        const Box& inner = down ? b : a;
        double ix = inner.x + inner.w / 2.0;
        double iy = inner.y + inner.h / 2.0;
        double left = inner.x - outer.x;
        double right = outer.x + outer.w - inner.x - inner.w;
        double top = inner.y - outer.y;
        double bottom = outer.y + outer.h - inner.y - inner.h;
        double least = std::min({left, right, top, bottom});
        double ox = (least == left) ? outer.x : (least == right) ? (outer.x + outer.w) : ix;
        double oy = (least == left || least == right) ? iy : (least == bottom) ? (outer.y + outer.h) : outer.y;
        double t = std::min(hit(inner, ox, oy, ix, iy), 1.0);
        double mx = ox + (ix - ox) * t;
        double my = oy + (iy - oy) * t;
        if (down) return {ox, oy, mx, my};
        return {mx, my, ox, oy};
    }

    double px = ax, py = ay, qx = bx, qy = by;
    double x0 = std::max(a.x, b.x);
    double x1 = std::min(a.x + a.w, b.x + b.w);
    double y0 = std::max(a.y, b.y);
    double y1 = std::min(a.y + a.h, b.y + b.h);
    if (x0 < x1) px = qx = (x0 + x1) / 2.0;
    else if (y0 < y1) py = qy = (y0 + y1) / 2.0;
    double len = std::hypot(qx - px, qy - py);
    if (len == 0.0) len = 1.0;
    double nx = (-(qy - py) / len) * shift;
    double ny = ((qx - px) / len) * shift;
    double ta = hit(a, qx + nx, qy + ny, px + nx, py + ny);
    double tb = hit(b, px + nx, py + ny, qx + nx, qy + ny);
    if (ta <= 1.0 && tb <= 1.0) {
        px += nx;
        py += ny;
        qx += nx;
        qy += ny;
    } else {
        px = ax;
        py = ay;
        qx = bx;
        qy = by;
        ta = std::min(hit(a, qx, qy, px, py), 1.0);
        tb = std::min(hit(b, px, py, qx, qy), 1.0);
    }
    return {qx + (px - qx) * ta, qy + (py - qy) * ta, px + (qx - px) * tb, py + (qy - py) * tb};
}

class Lanes {
public:
    std::vector<double> upright;
    std::vector<double> level;
    std::vector<double> frames[2];
    std::unordered_map<const Box*, std::vector<double>> taken;
    int squeezed = 0;

    bool free(bool isUpright, double at, double a0, double a1) const {
        double lo = std::min(a0, a1);
        double hi = std::max(a0, a1);
        auto near = [&](const std::vector<double>& list, double gap) {
            for (size_t i = 0; i + 2 < list.size(); i += 3) {
                if (std::abs(list[i] - at) < gap && list[i + 1] < hi && list[i + 2] > lo) return true;
            }
            return false;
        };
        return !near(isUpright ? upright : level, NEAR) && !near(frames[isUpright ? 0 : 1], FRAME_NEAR);
    }

    int crossings(const std::vector<double>& points) const {
        int count = 0;
        for (size_t i = 2; i + 1 < points.size(); i += 2) {
            bool isUpright = (points[i] == points[i - 2]);
            double at = isUpright ? points[i] : points[i + 1];
            double lo = std::min(isUpright ? points[i - 1] : points[i - 2], isUpright ? points[i + 1] : points[i]);
            double hi = std::max(isUpright ? points[i - 1] : points[i - 2], isUpright ? points[i + 1] : points[i]);
            const auto& list = isUpright ? level : upright;
            for (size_t j = 0; j + 2 < list.size(); j += 3) {
                if (list[j] > lo && list[j] < hi && list[j + 1] < at && list[j + 2] > at) count++;
            }
        }
        return count;
    }

    void frame(const Box& box) {
        frames[0].insert(frames[0].end(), {box.x, box.y, box.y + box.h, box.x + box.w, box.y, box.y + box.h});
        frames[1].insert(frames[1].end(), {box.y, box.x, box.x + box.w, box.y + box.h, box.x, box.x + box.w});
    }

    void take(const std::vector<double>& points) {
        for (size_t i = 2; i + 1 < points.size(); i += 2) {
            if (points[i] == points[i - 2]) {
                upright.insert(upright.end(), {points[i], std::min(points[i - 1], points[i + 1]), std::max(points[i - 1], points[i + 1])});
            } else if (points[i + 1] == points[i - 1]) {
                level.insert(level.end(), {points[i + 1], std::min(points[i - 2], points[i]), std::max(points[i - 2], points[i])});
            }
        }
    }

    std::vector<double> ports(const Box* box, int side, double want, double lo, double hi) const {
        auto it = taken.find(box);
        const std::vector<double>* used = (it != taken.end()) ? &it->second : nullptr;
        std::vector<double> out;
        for (int k = 0; k < 9 && out.size() < 4; k++) {
            double at = want + ((k & 1) ? 1.0 : -1.0) * std::ceil(k / 2.0) * PORT_STEP;
            if (at < lo || at > hi) continue;
            bool clear = true;
            if (used) {
                for (size_t i = 0; i + 1 < used->size(); i += 2) {
                    if ((*used)[i] == side && std::abs((*used)[i + 1] - at) < PORT_STEP) clear = false;
                }
            }
            if (clear) out.push_back(at);
        }
        if (out.empty()) out.push_back(want);
        return out;
    }

    void claim(const Box* box, int side, double at) {
        auto& u = taken[box];
        u.push_back(static_cast<double>(side));
        u.push_back(at);
    }
};

inline std::string routePathStr(const std::vector<double>& points, double startTrim, double endTrim) {
    size_t n = points.size();
    double ux = mathSign(points[2] - points[0]);
    double uy = mathSign(points[3] - points[1]);
    std::string d = "M" + num(points[0] + ux * startTrim) + "," + num(points[1] + uy * startTrim);
    for (size_t i = 2; i < n - 2; i += 2) {
        double x = points[i];
        double y = points[i + 1];
        double ix = mathSign(x - points[i - 2]);
        double iy = mathSign(y - points[i - 1]);
        double ox = mathSign(points[i + 2] - x);
        double oy = mathSign(points[i + 3] - y);
        double r = std::min({
            CORNER,
            std::abs(x - points[i - 2] + y - points[i - 1]) / 2.0,
            std::abs(points[i + 2] - x + points[i + 3] - y) / 2.0
        });
        d += (ix != 0.0 ? ("H" + num(x - ix * r)) : ("V" + num(y - iy * r))) +
             "Q" + num(x) + "," + num(y) + " " + num(x + ox * r) + "," + num(y + oy * r);
    }
    double lx = mathSign(points[n - 2] - points[n - 4]);
    double ly = mathSign(points[n - 1] - points[n - 3]);
    return d + (lx != 0.0 ? ("H" + num(points[n - 2] - lx * endTrim)) : ("V" + num(points[n - 1] - ly * endTrim)));
}

inline bool clearOf(const std::vector<double>& points, const std::vector<const Box*>& others, const Box* a, const Box* b) {
    for (const Box* other : others) {
        if (other != a && other != b && crosses(points, other->x, other->y, other->x + other->w, other->y + other->h)) return false;
    }
    return true;
}

inline Route bent(const std::vector<double>& points, size_t along, double startTrim, double endTrim, double nx, double ny, double lean, double room) {
    size_t n = points.size();
    Route r;
    r.d = routePathStr(points, startTrim, endTrim);
    r.sx = points[0];
    r.sy = points[1];
    r.sdx = -mathSign(points[2] - points[0]);
    r.sdy = -mathSign(points[3] - points[1]);
    r.ex = points[n - 2];
    r.ey = points[n - 1];
    r.edx = mathSign(points[n - 2] - points[n - 4]);
    r.edy = mathSign(points[n - 1] - points[n - 3]);
    r.x0 = points[along];
    r.y0 = points[along + 1];
    r.x1 = points[along + 2];
    r.y1 = points[along + 3];
    r.nx = nx;
    r.ny = ny;
    r.lean = lean;
    r.room = room;
    r.points = points;
    return r;
}

inline std::vector<double> acrossLanes(double lo, double hi) {
    std::vector<double> out;
    double mid = (lo + hi) / 2.0;
    double step = std::max(LANE_STEP, (hi - lo) / 4.0);
    for (int k = 0; k < 9; k++) {
        double at = mid + ((k & 1) ? 1.0 : -1.0) * std::ceil(k / 2.0) * step;
        if (at >= lo + NEAR && at <= hi - NEAR) out.push_back(at);
    }
    return out;
}

inline std::optional<Route> elbow(const Box* a, const Box* b, const std::vector<const Box*>& others, Lanes& lanes, double startTrim, double endTrim) {
    double ax = a->x + a->w / 2.0;
    double ay = a->y + a->h / 2.0;
    double bx = b->x + b->w / 2.0;
    double by = b->y + b->h / 2.0;
    struct Best {
        std::vector<double> points;
        double cost = 0;
        bool upright = false;
        int side0 = 0;
        int side1 = 0;
        bool valid = false;
    } best;

    auto consider = [&](const std::vector<double>& points, bool upright, int s0, int s1) -> bool {
        if (!clearOf(points, others, a, b)) return false;
        for (size_t i = 2; i + 1 < points.size(); i += 2) {
            bool up = (points[i] == points[i - 2]);
            if (!lanes.free(up, up ? points[i] : points[i + 1], up ? points[i - 1] : points[i - 2], up ? points[i + 1] : points[i])) return false;
        }
        double cost = lanes.crossings(points) * 1e6;
        for (size_t i = 2; i + 1 < points.size(); i += 2) {
            cost += std::abs(points[i] - points[i - 2]) + std::abs(points[i + 1] - points[i - 1]);
        }
        if (!best.valid || cost < best.cost) {
            best = {points, cost, upright, s0, s1, true};
            return true;
        }
        return false;
    };

    if (a->x + a->w <= b->x || b->x + b->w <= a->x) {
        bool right = bx > ax;
        double down = mathSign(by - ay);
        int s0 = right ? 1 : 0;
        int s1 = right ? 0 : 1;
        auto la = acrossLanes(right ? a->x + a->w : b->x + b->w, right ? b->x : a->x);
        bool doneSearch = false;
        for (double y0 : lanes.ports(a, s0, ay + (down * a->h) / 4.0, a->y + 8.0, a->y + a->h - 8.0)) {
            if (doneSearch) break;
            for (double y1 : lanes.ports(b, s1, by - (down * b->h) / 4.0, b->y + 8.0, b->y + b->h - 8.0)) {
                if (doneSearch) break;
                for (double lane : la) {
                    double x0 = lane + (ax - lane) * std::min(hit(*a, lane, y0, ax, y0), 1.0);
                    double x1 = lane + (bx - lane) * std::min(hit(*b, lane, y1, bx, y1), 1.0);
                    if (consider({x0, y0, lane, y0, lane, y1, x1, y1}, true, s0, s1)) {
                        doneSearch = true;
                        break;
                    }
                }
            }
        }
    }
    if (a->y + a->h <= b->y || b->y + b->h <= a->y) {
        bool below = by > ay;
        double sideways = mathSign(bx - ax);
        int s0 = below ? 3 : 2;
        int s1 = below ? 2 : 3;
        auto la = acrossLanes(below ? a->y + a->h : b->y + b->h, below ? b->y : a->y);
        bool doneSearch = false;
        for (double x0 : lanes.ports(a, s0, ax + (sideways * a->w) / 4.0, a->x + 8.0, a->x + a->w - 8.0)) {
            if (doneSearch) break;
            for (double x1 : lanes.ports(b, s1, bx - (sideways * b->w) / 4.0, b->x + 8.0, b->x + b->w - 8.0)) {
                if (doneSearch) break;
                for (double lane : la) {
                    double y0 = lane + (ay - lane) * std::min(hit(*a, x0, lane, x0, ay), 1.0);
                    double y1 = lane + (by - lane) * std::min(hit(*b, x1, lane, x1, by), 1.0);
                    if (consider({x0, y0, x0, lane, x1, lane, x1, y1}, false, s0, s1)) {
                        doneSearch = true;
                        break;
                    }
                }
            }
        }
    }
    if (!best.valid) return std::nullopt;
    lanes.take(best.points);
    lanes.claim(a, best.side0, best.upright ? best.points[1] : best.points[0]);
    lanes.claim(b, best.side1, best.upright ? best.points[7] : best.points[6]);
    return bent(best.points, 2, startTrim, endTrim, 0, 0, 0, best.upright ? NARROW : WIDE);
}

inline Route routeRel(
    const Box* a,
    const Box* b,
    double shift,
    const std::vector<const Box*>* others,
    Lanes& lanes,
    double startTrim,
    double endTrim
) {
    if (a == b) {
        double spread = std::min(a->h / 2.0 - 4.0, 10.0);
        double len = std::hypot(LOOP, spread);
        double x = a->x + a->w;
        double cy = a->y + a->h / 2.0;
        double ux = LOOP / len;
        double uy = spread / len;
        Route r;
        r.d = "M" + num(x + ux * startTrim) + "," + num(cy - spread - uy * startTrim) +
              "C" + num(x + LOOP) + "," + num(cy - spread * 2.0) + " " +
              num(x + LOOP) + "," + num(cy + spread * 2.0) + " " +
              num(x + ux * endTrim) + "," + num(cy + spread + uy * endTrim);
        r.sx = x;
        r.sy = cy - spread;
        r.sdx = -ux;
        r.sdy = uy;
        r.ex = x;
        r.ey = cy + spread;
        r.edx = -ux;
        r.edy = -uy;
        r.x0 = x + LOOP + 4.0;
        r.y0 = cy;
        r.x1 = x + LOOP + 4.0;
        r.y1 = cy;
        r.nx = 1.0;
        r.ny = 0.0;
        r.lean = 0.0;
        r.room = 64.0;
        return r;
    }

    auto [sx, sy, ex, ey] = connect(*a, *b, shift);
    bool upright = (sx == ex);
    bool blocked = false;
    if (others && !contains(*a, *b) && !contains(*b, *a)) {
        for (const Box* other : *others) {
            if (other != a && other != b && hitRect(other->x, other->y, other->x + other->w, other->y + other->h, sx, sy, ex - sx, ey - sy) <= 1.0) {
                blocked = true;
            }
        }
    }
    if (others && blocked && !upright && sy != ey) {
        auto around = elbow(a, b, *others, lanes, startTrim, endTrim);
        if (around.has_value()) return *around;
        lanes.squeezed++;
    }
    if (others && blocked && (upright || sy == ey)) {
        auto laneOn = [&](bool forward) -> double {
            double lane = forward ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity();
            auto widen = [&](const Box* box) {
                double edge = upright ? box->x : box->y;
                lane = forward ? std::max(lane, edge + (upright ? box->w : box->h)) : std::min(lane, edge);
            };
            widen(a);
            widen(b);
            for (const Box* other : *others) {
                if (other != a && other != b && hitRect(other->x, other->y, other->x + other->w, other->y + other->h, sx, sy, ex - sx, ey - sy) <= 1.0) {
                    widen(other);
                }
            }
            return lane;
        };
        double lo = upright ? std::min(a->y, b->y) : std::min(a->x, b->x);
        double hi = upright ? std::max(a->y + a->h, b->y + b->h) : std::max(a->x + a->w, b->x + b->w);
        auto outside = [&](bool forward, double lane) -> bool {
            for (const Box* other : *others) {
                if (other == a || other == b) continue;
                bool near = upright ? (other->y < hi && other->y + other->h > lo) : (other->x < hi && other->x + other->w > lo);
                double start = upright ? other->x : other->y;
                double end = start + (upright ? other->w : other->h);
                if (near && (forward ? end > lane : start < lane)) return false;
            }
            return true;
        };
        double ax = a->x + a->w / 2.0;
        double ay = a->y + a->h / 2.0;
        double bx = b->x + b->w / 2.0;
        double by = b->y + b->h / 2.0;
        double towards = upright ? mathSign(by - ay) : mathSign(bx - ax);

        struct AlongWay {
            std::vector<double> points;
            int side = 0;
            double p0 = 0;
            double p1 = 0;
            int step = 0;
        };
        auto along = [&](bool forward) -> AlongWay {
            double lane = laneOn(forward);
            double sign = forward ? 1.0 : -1.0;
            int side = upright ? (forward ? 1 : 0) : (forward ? 3 : 2);
            double p0 = upright
                ? lanes.ports(a, side, ay + (towards * a->h) / 4.0, a->y + 8.0, a->y + a->h - 8.0)[0]
                : lanes.ports(a, side, ax + (towards * a->w) / 4.0, a->x + 8.0, a->x + a->w - 8.0)[0];
            double p1 = upright
                ? lanes.ports(b, side, by - (towards * b->h) / 4.0, b->y + 8.0, b->y + b->h - 8.0)[0]
                : lanes.ports(b, side, bx - (towards * b->w) / 4.0, b->x + 8.0, b->x + b->w - 8.0)[0];
            auto pointsAt = [&](double at) -> std::vector<double> {
                if (upright) {
                    double x0 = at + (ax - at) * std::min(hit(*a, at, p0, ax, p0), 1.0);
                    double x1 = at + (bx - at) * std::min(hit(*b, at, p1, bx, p1), 1.0);
                    return {x0, p0, at, p0, at, p1, x1, p1};
                }
                double y0 = at + (ay - at) * std::min(hit(*a, p0, at, p0, ay), 1.0);
                double y1 = at + (by - at) * std::min(hit(*b, p1, at, p1, by), 1.0);
                return {p0, y0, p0, at, p1, at, p1, y1};
            };
            std::vector<double> pts = pointsAt(lane + sign * LANE);
            for (int k = 0; k <= 4; k++) {
                double at = lane + sign * (LANE + k * LANE_STEP);
                std::vector<double> tried = pointsAt(at);
                if (!clearOf(tried, *others, a, b)) continue;
                pts = tried;
                if (lanes.free(upright, at, p0, p1)) return {pts, side, p0, p1, k};
            }
            return {pts, side, p0, p1, 5};
        };

        bool natural = upright ? (ey > sy) : (ex < sx);
        bool out = outside(natural, laneOn(natural));
        bool back = outside(!natural, laneOn(!natural));
        AlongWay first = along(natural);
        std::optional<AlongWay> otherWay = (out == back) ? std::make_optional(along(!natural)) : std::nullopt;
        auto cost = [&](const AlongWay& way) -> int {
            return lanes.crossings(way.points) * 8 + way.step;
        };
        bool forward = (back && !out) ? !natural : (otherWay.has_value() && cost(*otherWay) < cost(first)) ? !natural : natural;
        AlongWay chosen = (forward == natural) ? first : (otherWay.has_value() ? *otherWay : along(!natural));
        double sign = forward ? 1.0 : -1.0;
        if (chosen.step > 4) lanes.squeezed++;
        lanes.take(chosen.points);
        lanes.claim(a, chosen.side, chosen.p0);
        lanes.claim(b, chosen.side, chosen.p1);
        return bent(chosen.points, 2, startTrim, endTrim, upright ? sign : 0.0, 0.0, upright ? LEAN : 0.0, upright ? NARROW : WIDE);
    }

    double len = std::hypot(ex - sx, ey - sy);
    if (len == 0.0) len = 1.0;
    double dx = (ex - sx) / len;
    double dy = (ey - sy) / len;
    if (upright || sy == ey) lanes.take({sx, sy, ex, ey});
    Route r;
    r.d = "M" + num(sx + dx * startTrim) + "," + num(sy + dy * startTrim) +
          "L" + num(ex - dx * endTrim) + "," + num(ey - dy * endTrim);
    r.sx = sx;
    r.sy = sy;
    r.sdx = -dx;
    r.sdy = -dy;
    r.ex = ex;
    r.ey = ey;
    r.edx = dx;
    r.edy = dy;
    r.x0 = sx;
    r.y0 = sy;
    r.x1 = ex;
    r.y1 = ey;
    r.nx = shift != 0.0 ? -dy : 0.0;
    r.ny = shift != 0.0 ? dx : 0.0;
    r.lean = 0.0;
    r.room = std::abs(dx) > std::abs(dy) ? NARROW : WIDE;
    r.points = {sx, sy, ex, ey};
    return r;
}

// ============================================================================
// src/diagrams/c4/layout.ts
// ============================================================================

constexpr double SHAPE_W = 216.0;
constexpr double SHAPE_MIN_H = 64.0;
constexpr double PAD_X = 14.0;
constexpr double PAD_Y = 12.0;
constexpr double GAP_X = NARROW + LABEL_PAD + 3.0 * ARROW;
constexpr double GAP_Y = 88.0;
constexpr double GROUP_PAD = LANE + 8.0;
constexpr double GROUP_GAP = 48.0;
constexpr double HEAD_LEFT = 16.0;
constexpr double HEAD_TOP = 10.0;

inline const std::unordered_map<std::string, std::string>& c4Forms() {
    static const std::unordered_map<std::string, std::string> m = {
        {"person", "person"},
        {"box", "rect"},
        {"rounded", "rect"},
        {"component", "rect"},
        {"cylinder", "cyl"},
        {"database", "cyl"},
        {"db", "cyl"},
        {"queue", "h-cyl"},
        {"pipe", "h-cyl"}
    };
    return m;
}

struct Node : Box {
    const C4Shape* shape = nullptr;
    std::vector<Label> labels;
    double textH = 0;
};

struct Group : Box {
    const C4Boundary* boundary = nullptr;
    std::vector<Group*> kids;
    std::deque<Node> nodes;
    std::vector<Label> labels;
    double head = 0;
    double headW = 0;
    double headX = HEAD_LEFT;
};

struct Layout {
    std::deque<Group> groupStore;
    std::vector<Group*> order;
    std::unordered_map<const C4Element*, const Box*> boxes;
};

inline std::string formOf(const C4Shape& shape) {
    const auto& forms = c4Forms();
    if (shape.shape.has_value()) {
        auto it = forms.find(toLowerAscii(*shape.shape));
        if (it != forms.end()) return it->second;
    }
    if (shape.sprite.has_value()) {
        auto it = forms.find(toLowerAscii(*shape.sprite));
        if (it != forms.end()) return it->second;
    }
    if (shape.tags.has_value()) {
        for (const auto& tag : splitStr(*shape.tags, ',')) {
            auto it = forms.find(toLowerAscii(trimStr(tag)));
            if (it != forms.end()) return it->second;
        }
    }
    const std::string& kind = shape.kind;
    if (endsWith(kind, "person")) return "person";
    if (endsWith(kind, "_db")) return "cyl";
    if (endsWith(kind, "_queue")) return "h-cyl";
    return "rect";
}

inline Node measureShape(const C4Shape& shape, const TextMeasurer& measurer, double size, double small, double width = SHAPE_W) {
    std::string form = formOf(shape);
    double capW = (form == "h-cyl") ? 36.0 : 0.0;
    double capH = (form == "person") ? HEAD : (form == "cyl") ? 24.0 : 0.0;
    double inner = width - 2.0 * PAD_X - capW;
    std::string kindText = "\xC2\xAB" + replaceAllStr(shape.kind, "_", " ") + "\xC2\xBB";
    std::string technText = (shape.techn.has_value() && !shape.techn->empty()) ? ("[" + *shape.techn + "]") : "";
    std::string descrText = shape.descr.value_or("");
    std::vector<Label> labels = {
        layoutLabel(kindText, false, measurer, small - 1.0, inner),
        layoutLabel(shape.label, false, measurer, size, inner, Style::Bold),
        layoutLabel(technText, false, measurer, small, inner),
        layoutLabel(descrText, false, measurer, small, inner)
    };
    double tw = 0;
    double textH = labels[3].height > 0 ? 4.0 : 0.0;
    for (const auto& lbl : labels) {
        tw = std::max(tw, lbl.width);
        textH += lbl.height;
    }
    Node n;
    n.shape = &shape;
    n.form = form;
    n.labels = std::move(labels);
    n.textH = textH;
    n.x = 0;
    n.y = 0;
    n.w = std::max(width, tw + 2.0 * PAD_X + capW);
    n.h = std::max(SHAPE_MIN_H, textH + 2.0 * PAD_Y + capH);
    return n;
}

template <typename PtrVec>
inline std::pair<double, double> placeRows(PtrVec& boxes, int perRow, double gapX, double gapY, bool stretch) {
    double width = 0;
    double y = 0;
    int total = static_cast<int>(boxes.size());
    for (int from = 0; from < total; from += perRow) {
        int to = std::min(total, from + perRow);
        double x = 0;
        double rowH = 0;
        for (int i = from; i < to; i++) rowH = std::max(rowH, boxes[i]->h);
        for (int i = from; i < to; i++) {
            Box* box = boxes[i];
            box->x = x;
            box->y = y;
            if (stretch) box->h = rowH;
            x += box->w + gapX;
        }
        width = std::max(width, x - gapX);
        y += rowH + gapY;
    }
    return {width, total > 0 ? y - gapY : 0.0};
}

inline Layout layoutC4(const C4Db& db, const TextMeasurer& measurer, double size, double small, std::optional<double> room, double gap) {
    Layout lay;
    double groupPad = !room.has_value() ? GROUP_PAD : 12.0;
    std::unordered_map<const C4Boundary*, Group*> groups;
    for (const auto& boundary : db.boundaries) {
        double labelRoom = !room.has_value() ? 4000.0 : std::max(80.0, *room - 2.0 * HEAD_LEFT - 2.0 * GROUP_PAD * 2.0);
        std::vector<Label> labels;
        if (boundary.parent) {
            std::string typeText = (boundary.type.has_value() && !boundary.type->empty()) ? ("[" + *boundary.type + "]") : "";
            std::string descrText = boundary.descr.value_or("");
            labels = {
                layoutLabel(boundary.label, false, measurer, size, labelRoom, Style::Bold),
                layoutLabel(typeText, false, measurer, small, labelRoom),
                layoutLabel(descrText, false, measurer, small, labelRoom)
            };
        }
        double head = 0;
        double headW = 0;
        for (const auto& lbl : labels) {
            head += lbl.height;
            headW = std::max(headW, lbl.width);
        }
        Group g;
        g.boundary = &boundary;
        g.labels = std::move(labels);
        g.head = head > 0 ? head + HEAD_TOP + 6.0 + LANE : groupPad;
        g.headW = headW;
        g.headX = HEAD_LEFT;
        g.form = "rect";
        lay.groupStore.push_back(std::move(g));
        groups[&boundary] = &lay.groupStore.back();
    }
    Group* root = groups[&db.boundaries[0]];
    for (auto& g : lay.groupStore) {
        if (g.boundary->parent) {
            auto it = groups.find(g.boundary->parent);
            if (it != groups.end()) it->second->kids.push_back(&g);
        }
    }
    lay.order.push_back(root);
    std::unordered_map<const C4Boundary*, int> depths;
    depths[root->boundary] = 0;
    for (size_t i = 0; i < lay.order.size(); i++) {
        for (Group* kid : lay.order[i]->kids) {
            depths[kid->boundary] = depths[lay.order[i]->boundary] + 1;
            lay.order.push_back(kid);
        }
    }
    for (const auto& shape : db.shapes) {
        if (shape.parent) {
            auto dIt = depths.find(shape.parent);
            int depth = (dIt != depths.end()) ? dIt->second : 0;
            double nodeWidth = !room.has_value() ? SHAPE_W : std::max(100.0, std::min(SHAPE_W, *room - 2.0 * depth * groupPad - 32.0));
            auto gIt = groups.find(shape.parent);
            if (gIt != groups.end()) {
                gIt->second->nodes.push_back(measureShape(shape, measurer, size, small, nodeWidth));
            }
        }
    }

    int perRow = std::max(1, db.shapeInRow);
    int groupsPerRow = std::max(1, db.boundaryInRow);
    std::unordered_map<const Group*, double> kidsTop;
    for (int i = static_cast<int>(lay.order.size()) - 1; i >= 0; i--) {
        Group* group = lay.order[i];
        double gapX = !room.has_value() ? GAP_X : gap;
        auto countCols = [&](const auto& ptrs, int limit, double gVal) -> int {
            if (!room.has_value()) return limit;
            double widest = 1.0;
            for (const auto* box : ptrs) widest = std::max(widest, box->w);
            return std::max(1, std::min(limit, static_cast<int>(std::floor((*room + gVal - 2.0 * groupPad) / (widest + gVal)))));
        };
        std::vector<Box*> nodePtrs;
        for (auto& n : group->nodes) nodePtrs.push_back(&n);
        std::vector<Box*> kidPtrs;
        for (Group* k : group->kids) kidPtrs.push_back(k);

        auto [gridW, gridH] = placeRows(nodePtrs, countCols(nodePtrs, perRow, gapX), gapX, GAP_Y, true);
        auto [kidsW, kidsH] = placeRows(kidPtrs, countCols(kidPtrs, groupsPerRow, GROUP_GAP), GROUP_GAP, GROUP_GAP, false);
        double between = (gridH > 0 && kidsH > 0) ? GROUP_GAP : 0.0;
        double frame = (group == root) ? 0.0 : groupPad;
        kidsTop[group] = gridH + between;
        group->w = std::max({gridW, kidsW, group->headW + 2.0 * HEAD_LEFT - 2.0 * frame}) + 2.0 * frame;
        group->h = (frame != 0.0 ? group->head : 0.0) + gridH + between + kidsH + frame;
    }

    for (Group* group : lay.order) {
        double left = group->x + (group == root ? 0.0 : groupPad);
        double top = group->y + (group == root ? 0.0 : group->head);
        for (auto& node : group->nodes) {
            node.x += left;
            node.y += top;
            lay.boxes[node.shape] = &node;
        }
        for (Group* kid : group->kids) {
            kid->x += left;
            kid->y += top + kidsTop[group];
        }
        if (group != root) lay.boxes[group->boundary] = group;
    }
    return lay;
}

// ============================================================================
// src/diagrams/c4/render.ts
// ============================================================================

constexpr double PAIR_SHIFT = 7.0;
constexpr double NARROW_GAP = 36.0;
constexpr double WIDEN = 24.0;
constexpr double MAX_OFFSET = 2000.0;
constexpr double EDGE = 8.0;
constexpr double CARE_BUDGET = 2000000.0;

inline ResolvedStyle userStyle(const std::optional<std::string>& fill, const std::optional<std::string>& stroke, const std::optional<std::string>& color) {
    std::vector<std::string> declarations;
    if (fill.has_value() && !fill->empty()) declarations.push_back("fill:" + *fill);
    if (stroke.has_value() && !stroke->empty()) declarations.push_back("stroke:" + *stroke);
    if (color.has_value() && !color->empty()) declarations.push_back("color:" + *color);
    return resolveStyle(declarations);
}

inline std::string linkC4(const std::string& body, const std::optional<std::string>& url, const std::string& id, std::vector<LinkInfo>& links, const RenderOptions& options) {
    if (!url.has_value() || url->empty()) return body;
    std::string u = *url;
    if (u.find("\xC2\xB6\xC3\x9F") != std::string::npos) {
        u = replaceAllStr(u, "\xEF\xAC\x82\xC2\xB0\xC2\xB0", "#");
        u = replaceAllStr(u, "\xEF\xAC\x82\xC2\xB0", "#");
        u = replaceAllStr(u, "\xC2\xB6\xC3\x9F", ";");
    }
    return linked(body, sanitizeUrl(u), id, links, options);
}

inline std::pair<Rendered, int> drawC4(const C4Db& db, const Config&, const RenderOptions& options, bool tight, double gap = NARROW_GAP) {
    double size = options.fontSize;
    double small = std::round(size * 0.8125);
    const TextMeasurer& measurer = getMeasurer(options);
    double pad = options.padding;
    std::vector<LinkInfo> links;
    std::optional<double> room = tight ? std::make_optional(std::max(80.0, (options.maxWidth > 0 ? options.maxWidth : 320.0) - 2.0 * pad)) : std::nullopt;
    Layout lay = layoutC4(db, measurer, size, small, room, gap);
    Group* root = lay.order[0];

    double minX = 0;
    double minY = 0;
    double maxX = root->w;
    double maxY = root->h;
    auto grow = [&](double x0, double y0, double x1, double y1) {
        minX = std::min(minX, x0);
        minY = std::min(minY, y0);
        maxX = std::max(maxX, x1);
        maxY = std::max(maxY, y1);
    };

    std::vector<const Box*> ends;
    std::unordered_map<const Box*, std::unordered_set<const Box*>> pairs;
    for (const auto& rel : db.rels) {
        const C4Element* from = rel.fromAttr.has_value() ? nullptr : db.getC4Shape(rel.from);
        const C4Element* to = rel.toAttr.has_value() ? nullptr : db.getC4Shape(rel.to);
        const Box* a = (from && lay.boxes.count(from)) ? lay.boxes.at(from) : nullptr;
        const Box* b = (to && lay.boxes.count(to)) ? lay.boxes.at(to) : nullptr;
        if (!a || !b) {
            throw PeleError("C4 rel \"" + rel.from + "\" -> \"" + rel.to + "\" references an unknown shape or boundary", "semantic", "c4");
        }
        ends.push_back(a);
        ends.push_back(b);
        pairs[a].insert(b);
    }

    std::vector<const Box*> nodes;
    std::vector<double> taken;
    for (Group* group : lay.order) {
        for (const auto& node : group->nodes) {
            nodes.push_back(&node);
            taken.insert(taken.end(), {node.x + EDGE, node.y + EDGE, node.x + node.w - EDGE, node.y + node.h - EDGE});
        }
    }
    bool careful = static_cast<double>(db.rels.size()) * static_cast<double>(nodes.size() + lay.order.size() + 3 * db.rels.size()) <= CARE_BUDGET;
    auto covered = [&](double x0, double y0, double x1, double y1) -> double {
        double area = 0;
        for (size_t i = 0; i + 3 < taken.size(); i += 4) {
            double w = std::min(x1, taken[i + 2]) - std::max(x0, taken[i]);
            if (w <= 0) continue;
            double h = std::min(y1, taken[i + 3]) - std::max(y0, taken[i + 1]);
            if (h > 0) area += w * h;
        }
        return area;
    };
    auto clampOffset = [](const std::optional<int>& val) -> double {
        if (!val.has_value() || *val == 0) return 0.0;
        return std::max(-MAX_OFFSET, std::min(MAX_OFFSET, static_cast<double>(*val)));
    };

    std::string casings;
    std::string edgesOut;
    double trim = markerTrim("arrow_point");
    std::vector<Fill> fills;
    for (Group* group : lay.order) {
        if (group == root) continue;
        std::string shape = userStyle(group->boundary->bgColor, std::nullopt, std::nullopt).shape;
        if (paints(shape)) {
            fills.push_back({{group->x, group->y, group->x + group->w, group->y + group->h}, shape});
        }
    }
    Lanes lanes;
    for (Group* group : lay.order) if (group != root) lanes.frame(*group);

    std::vector<Route> routes;
    routes.reserve(db.rels.size());
    for (size_t index = 0; index < db.rels.size(); index++) {
        const auto& rel = db.rels[index];
        const Box* a = ends[index * 2];
        const Box* b = ends[index * 2 + 1];
        ResolvedStyle style = userStyle(std::nullopt, rel.lineColor, std::nullopt);
        bool startArrow = (rel.type == "birel" || rel.type == "rel_b");
        bool endArrow = (rel.type != "rel_b");
        bool reverse = (a != b && pairs.count(b) && pairs[b].count(a));
        Route r = routeRel(a, b, reverse ? PAIR_SHIFT : 0.0, careful ? &nodes : nullptr, lanes, startArrow ? trim : 0.0, endArrow ? trim : 0.0);
        grow(std::min(r.x0, r.x1), std::min(r.y0, r.y1), std::max(r.x0, r.x1), std::max(r.y0, r.y1));
        if (startArrow && careful) taken.insert(taken.end(), {r.sx - ARROW, r.sy - ARROW, r.sx + ARROW, r.sy + ARROW});
        if (endArrow && careful) taken.insert(taken.end(), {r.ex - ARROW, r.ey - ARROW, r.ex + ARROW, r.ey + ARROW});
        auto over = overFills(r.d, {r.sx, r.sy, r.ex, r.ey}, fills, style.line);
        std::string color = !over.value.empty() ? over.value : (style.hasStroke ? esc(style.stroke) : "var(--_l)");
        if (over.onFill && over.value.empty()) {
            EdgePath ep;
            ep.d = r.d;
            ep.sx = r.sx; ep.sy = r.sy; ep.sdx = r.sdx; ep.sdy = r.sdy;
            ep.ex = r.ex; ep.ey = r.ey; ep.edx = r.edx; ep.edy = r.edy;
            casings += edgeCasing(ep, startArrow ? "arrow_point" : "none", endArrow ? "arrow_point" : "none", style.line);
        }
        edgesOut +=
            "<g class=\"pele-edge\" data-id=\"" + escText(rel.from + "-" + rel.to) + "\"" +
            (!over.value.empty() ? (" stroke=\"" + over.value + "\"") : "") +
            style.line + "><path d=\"" + r.d + "\"/>" +
            (startArrow ? marker("arrow_point", r.sx, r.sy, r.sdx, r.sdy, color) : "") +
            (endArrow ? marker("arrow_point", r.ex, r.ey, r.edx, r.edy, color) : "") +
            "</g>";
        routes.push_back(std::move(r));
    }

    for (Group* group : lay.order) {
        if (!careful || group->headW == 0) continue;
        double top = group->y + HEAD_TOP;
        double bottom = group->y + group->head - LANE;
        int fewest = std::numeric_limits<int>::max();
        double candidates[3] = {HEAD_LEFT, (group->w - group->headW) / 2.0, group->w - HEAD_LEFT - group->headW};
        for (double x : candidates) {
            int count = 0;
            for (const auto& r : routes) {
                if (crosses(r.points, group->x + x - 4.0, top, group->x + x + group->headW + 4.0, bottom)) count++;
            }
            if (count < fewest) {
                fewest = count;
                group->headX = x;
            }
            if (count == 0) break;
        }
        taken.insert(taken.end(), {group->x + group->headX - 4.0, top, group->x + group->headX + group->headW + 4.0, bottom});
    }

    static const double SLIDE[7] = {0.5, 0.4, 0.6, 0.3, 0.7, 0.2, 0.8};
    std::string labelsOut;
    for (size_t index = 0; index < db.rels.size(); index++) {
        const auto& rel = db.rels[index];
        const Route& r = routes[index];
        std::string id = rel.from + "-" + rel.to;
        ResolvedStyle style = userStyle(std::nullopt, std::nullopt, rel.textColor);
        std::string text = (db.c4Type == "C4Dynamic" ? (std::to_string(index + 1) + ": ") : "") + rel.label;
        Label label = layoutLabel(text, false, measurer, small, r.room);
        std::string technStr = (rel.techn.has_value() && !rel.techn->empty()) ? ("[" + *rel.techn + "]") : "";
        Label techn = layoutLabel(technStr, false, measurer, small, r.room, Style::Italic);
        double lw = std::max(label.width, techn.width) + LABEL_PAD;
        double lh = label.height + techn.height;
        if (lh == 0) continue;

        double beside = std::abs(r.nx) * (lw / 2.0) + std::abs(r.ny) * (lh / 2.0) + 2.0;
        double aside = r.lean != 0.0 ? std::max(0.0, std::min(r.lean, beside - 14.0)) : beside;
        double dx = r.nx * aside + (tight ? 0.0 : clampOffset(rel.offsetX));
        double dy = r.ny * aside + (tight ? 0.0 : clampOffset(rel.offsetY));
        double at = 0.5;
        if (careful) {
            double least = std::numeric_limits<double>::infinity();
            bool moved = (dx != r.nx * aside || dy != r.ny * aside);
            for (int pass = 0; pass < (moved ? 2 : 1) && least > 0; pass++) {
                double px = pass ? r.nx * aside : dx;
                double py = pass ? r.ny * aside : dy;
                for (double t : SLIDE) {
                    double cx = r.x0 + (r.x1 - r.x0) * t + px;
                    double cy = r.y0 + (r.y1 - r.y0) * t + py;
                    double area = covered(cx - lw / 2.0, cy - lh / 2.0, cx + lw / 2.0, cy + lh / 2.0);
                    if (area < least) {
                        least = area;
                        at = t;
                        dx = px;
                        dy = py;
                    }
                    if (area == 0) break;
                }
            }
        }
        double cx = r.x0 + (r.x1 - r.x0) * at + dx;
        double top = r.y0 + (r.y1 - r.y0) * at + dy - lh / 2.0;
        if (careful) taken.insert(taken.end(), {cx - lw / 2.0, top, cx + lw / 2.0, top + lh});
        grow(cx - lw / 2.0, top, cx + lw / 2.0, top + lh);
        labelsOut +=
            "<g class=\"pele-edge-label\" data-id=\"" + escText(id) + "\">" +
            (rel.descr.has_value() && !rel.descr->empty() ? ("<title>" + escText(*rel.descr) + "</title>") : "") +
            "<rect x=\"" + num(cx - lw / 2.0) + "\" y=\"" + num(top) + "\" width=\"" + num(lw) + "\" height=\"" + num(lh) + "\" rx=\"3\" fill=\"var(--_bg)\"/>" +
            linkC4(
                labelSvg(label, cx, top + label.height / 2.0, style.text, options.icons) +
                labelSvg(techn, cx, top + label.height + techn.height / 2.0, " class=\"pele-c4-techn\" fill=\"var(--_m)\"" + style.text, options.icons),
                rel.link,
                id,
                links,
                options
            ) +
            "</g>";
    }

    std::string clusters;
    std::string nodesOut;
    for (Group* group : lay.order) {
        if (group != root) {
            const C4Boundary& boundary = *group->boundary;
            ResolvedStyle style = userStyle(boundary.bgColor, boundary.borderColor, boundary.fontColor);
            double x = group->x + group->headX;
            double y = group->y + HEAD_TOP;
            std::string color = inkFill(style) + style.text;
            std::string muted = inkFill(style, "var(--_m)") + style.text;
            std::string text = leftLabelSvg(group->labels[0], x, y, " class=\"pele-cluster-label\" font-weight=\"var(--_hw)\"" + color);
            y += group->labels[0].height;
            text += leftLabelSvg(group->labels[1], x, y, " class=\"pele-c4-type\" font-size=\"" + num(small) + "\"" + muted);
            y += group->labels[1].height;
            text += leftLabelSvg(group->labels[2], x, y, " class=\"pele-c4-descr\" font-size=\"" + num(small) + "\"" + muted);
            clusters +=
                "<g class=\"pele-cluster pele-c4-" + std::string(boundary.nodeType.has_value() ? "node" : "boundary") + "\" data-id=\"" + escText(boundary.alias) + "\">" +
                "<rect x=\"" + num(group->x) + "\" y=\"" + num(group->y) + "\" width=\"" + num(group->w) + "\" height=\"" + num(group->h) + "\" fill=\"none\" stroke=\"var(--_b)\"" +
                (boundary.nodeType.has_value() ? "" : " stroke-dasharray=\"6 4\"") +
                rounded(style.shape) + "/>" +
                linkC4(text, boundary.link, boundary.alias, links, options) +
                "</g>";
        }
        for (const auto& node : group->nodes) {
            const C4Shape& shape = *node.shape;
            bool external = startsWith(shape.kind, "external");
            ResolvedStyle style = userStyle(shape.bgColor, shape.borderColor, shape.fontColor);
            std::string stroke = std::string(" stroke=\"var(--_b)\"") + (external ? " stroke-dasharray=\"4 3\"" : "");
            double w = node.w, h = node.h;
            const std::string& form = node.form;
            const auto& labels = node.labels;
            double dx = (form == "h-cyl") ? (-queueRx(h) / 2.0) : 0.0;
            double dy = (form == "person") ? (HEAD / 2.0) : (form == "cyl") ? (cylRy(w) / 2.0) : 0.0;
            std::string color = inkFill(style) + style.text;
            std::string muted = inkFill(style, "var(--_m)") + style.text;
            double y = dy - node.textH / 2.0;
            std::string text = labelSvg(labels[0], dx, y + labels[0].height / 2.0, " class=\"pele-c4-type\" font-size=\"" + num(small - 1.0) + "\"" + muted, options.icons);
            y += labels[0].height;
            text += labelSvg(labels[1], dx, y + labels[1].height / 2.0, " class=\"pele-label\" font-weight=\"var(--_hw)\"" + color, options.icons);
            y += labels[1].height;
            text += labelSvg(labels[2], dx, y + labels[2].height / 2.0, " class=\"pele-c4-techn\" font-size=\"" + num(small) + "\"" + muted, options.icons);
            y += labels[2].height + 4.0;
            text += labelSvg(labels[3], dx, y + labels[3].height / 2.0, " class=\"pele-c4-descr\" font-size=\"" + num(small) + "\"" + color, options.icons);
            nodesOut +=
                "<g class=\"pele-node pele-shape-" + form + " pele-c4-" + shape.kind + (external ? " pele-c4-external" : "") + "\" data-id=\"" + escText(shape.alias) +
                "\" transform=\"translate(" + num(node.x + w / 2.0) + "," + num(node.y + h / 2.0) + ")\">" +
                linkC4(
                    drawShape(form, w, h, std::string(" fill=\"var(") + (external ? "--_bg" : "--_s") + ")\"" + stroke + style.shape, stroke + style.line) + text,
                    shape.link,
                    shape.alias,
                    links,
                    options
                ) +
                "</g>";
        }
    }

    Label title = layoutLabel(db.title, false, measurer, size, tight ? titleRoom(options) : 4000.0, Style::Bold);
    double titleHeight = title.height > 0 ? title.height + 12.0 : 0.0;
    double inner = std::max(maxX - minX, title.width);
    double width = std::ceil(inner + 2.0 * pad);
    double height = std::ceil(maxY - minY + titleHeight + 2.0 * pad);
    double ox = pad - minX + (inner - (maxX - minX)) / 2.0;
    double oy = pad + titleHeight - minY;

    std::string svg = svgDocument(
        "c4",
        width,
        height,
        size,
        options,
        db,
        labelSvg(title, width / 2.0, pad + title.height / 2.0, " class=\"pele-title\" font-weight=\"var(--_tw)\"", options.icons) +
        "<g transform=\"translate(" + num(ox) + "," + num(oy) + ")\">" +
        (!clusters.empty() ? ("<g class=\"pele-clusters\">" + clusters + "</g>") : "") +
        casingsSvg(casings) +
        (!edgesOut.empty() ? ("<g class=\"pele-edges\" fill=\"none\" stroke=\"var(--_l)\" stroke-linecap=\"round\">" + edgesOut + "</g>") : "") +
        (!nodesOut.empty() ? ("<g class=\"pele-nodes\">" + nodesOut + "</g>") : "") +
        (!labelsOut.empty() ? ("<g class=\"pele-edge-labels\" font-size=\"" + num(small) + "\">" + labelsOut + "</g>") : "") +
        "</g>"
    );
    return {{svg, width, height, std::move(links)}, lanes.squeezed};
}

inline Rendered render(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options, int = 0, int = 5000) {
    C4Db db;
    if (!title.empty()) db.title = title;
    parseC4(source, db);
    return tighten(options, [&](bool tight) {
        if (!tight) return drawC4(db, config, options, false).first;
        auto [best, squeezed] = drawC4(db, config, options, true, NARROW_GAP);
        for (double gap = NARROW_GAP + WIDEN; squeezed > 0 && gap <= NARROW_GAP + 3.0 * WIDEN; gap += WIDEN) {
            auto [wider, left] = drawC4(db, config, options, true, gap);
            if (left < squeezed) {
                best = std::move(wider);
                squeezed = left;
            }
        }
        return best;
    });
}

} // namespace C4Diagram

inline Rendered renderC4(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options, int lineOffset = 0, int maxEdges = 5000) {
    return C4Diagram::render(source, config, title, options, lineOffset, maxEdges);
}

} // namespace Pele
