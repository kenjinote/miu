#pragma once
#include "../foundation.h"
#include "../util.h"
#include "../text.h"
#include "../svg.h"
#include "../common_parse.h"
#include "state.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Pele::ErDiagram {

using StateDiagram::CEdge;
using StateDiagram::CNode;
using StateDiagram::CompoundResult;
using StateDiagram::cnode;
using StateDiagram::compoundLayout;
using StateDiagram::direction;
using StateDiagram::runsAcross;
using StateDiagram::turnToFit;

inline std::string num(double value) noexcept {
    if (!std::isfinite(value)) return "0";
    long long n = static_cast<long long>(std::floor(value * 100.0 + 0.5));
    if (n == 0) return "0";
    bool neg = n < 0;
    long long m = neg ? -n : n;
    long long q = m / 100;
    long long r = m % 100;
    std::string s = (neg ? "-" : "") + std::to_string(q);
    if (r == 0) return s;
    if (r % 10 == 0) return s + "." + std::to_string(r / 10);
    if (r < 10) return s + ".0" + std::to_string(r);
    return s + "." + std::to_string(r);
}

inline CEdge cedge(int src, int dst, int minlen = 1, double labelW = 0.0, double labelH = 0.0) noexcept {
    CEdge e;
    e.src = src;
    e.dst = dst;
    e.minlen = minlen;
    e.labelW = labelW;
    e.labelH = labelH;
    return e;
}

inline void shiftLayout(std::vector<CNode>& nodes, std::vector<CEdge>& edges, double dx, double dy) noexcept {
    for (auto& c : nodes) {
        c.x += dx;
        c.y += dy;
    }
    for (auto& e : edges) {
        for (size_t i = 0; i + 1 < e.route.size(); i += 3) {
            e.route[i] += dx;
            e.route[i + 1] += dy;
        }
        e.labelX += dx;
        e.labelY += dy;
    }
}

using Crossings = std::unordered_map<int, std::vector<double>>;

inline void markCrossings(Crossings& crossings, const std::vector<double>& route) noexcept {
    for (size_t k = 0; k + 1 < route.size(); k += 3) {
        int key = static_cast<int>(std::llround(route[k + 1]));
        crossings[key].push_back(route[k]);
    }
}

inline double clusterTitleX(const CNode& c, double width, const Crossings& crossings, double pad) noexcept {
    if (width == 0.0) return c.x;
    double top = c.y - c.h / 2.0;
    double left = c.x - c.w / 2.0;
    double half = width / 2.0;
    double candidates[3] = {left + pad - 6.0 + half, c.x, left + c.w - pad + 6.0 - half};
    auto it = crossings.find(static_cast<int>(std::llround(top)));
    if (it == crossings.end()) return candidates[0];
    const auto& xs = it->second;
    double best = candidates[0];
    double bestGap = -1.0;
    for (double x : candidates) {
        double gap = std::numeric_limits<double>::infinity();
        for (double cx : xs) {
            if (cx > left && cx < left + c.w) gap = std::min(gap, std::abs(cx - x) - half);
        }
        if (gap >= 6.0) return x;
        if (gap > bestGap + 0.5) {
            bestGap = gap;
            best = x;
        }
    }
    return best;
}

inline bool internalLink(const std::string& name, const RenderOptions& options) noexcept {
    std::string url = sanitizeUrl(name);
    if (url == "about:blank" || linkUrl(url, options).empty()) return false;
    std::string seen;
    size_t i = 0;
    while (i < name.size()) {
        size_t s = i;
        uint32_t cp = nextCodepoint(name, i);
        if (isInvalidXmlCodepoint(cp) || cp == '\t' || cp == '\n' || cp == '\r') continue;
        seen.append(name, s, i - s);
    }
    size_t start = 0;
    while (start < seen.size() && static_cast<unsigned char>(seen[start]) <= 0x20) start++;
    seen = seen.substr(start);
    return !linkUrl(seen, options).empty();
}

// ============================================================================
// src/diagrams/er/types.ts & src/diagrams/er/db.ts
// ============================================================================

struct ErAttribute {
    std::string type;
    std::string name;
    std::vector<std::string> keys;
    std::string comment;
};

struct ErEntity {
    std::string id;
    std::string label;
    std::string alias;
    std::vector<ErAttribute> attributes;
    std::string cssClasses = "default";
    std::vector<std::string> cssStyles;
};
using ErEntityPtr = std::shared_ptr<ErEntity>;

struct RelSpec {
    std::string cardA; // "ZERO_OR_ONE", "ZERO_OR_MORE", "ONE_OR_MORE", "ONLY_ONE", "MD_PARENT"
    std::string relType; // "NON_IDENTIFYING", "IDENTIFYING"
    std::string cardB;
};

struct ErRelationship {
    std::string entityA;
    std::string roleA;
    std::string entityB;
    RelSpec relSpec;
};

struct ErClassDef {
    std::string id;
    std::vector<std::string> styles;
    std::vector<std::string> textStyles;
};

struct ErSubgraph {
    std::string id;
    std::vector<std::string> nodes;
    std::string title;
    std::vector<std::string> classes;
    std::vector<std::string> cssStyles;
    std::string dir;
    bool hasDir = false;
    std::string labelType = "markdown";
};
using ErSubgraphPtr = std::shared_ptr<ErSubgraph>;

struct DocItem {
    bool isDir = false;
    std::string value;
};

class ErDb : public DiagramMeta {
public:
    std::string direction = "TB";
    std::vector<std::pair<std::string, ErEntityPtr>> entitiesList;
    std::unordered_map<std::string, ErEntityPtr> entities;
    std::vector<ErRelationship> relationships;
    std::unordered_map<std::string, ErClassDef> classes;
    std::vector<ErSubgraphPtr> subgraphs;

private:
    std::unordered_map<std::string, ErSubgraphPtr> subgraphLookup;
    Pele::FlatSet<std::string> claimed;

public:
    ErEntityPtr addEntity(const std::string& name, const std::string& alias = "") {
        auto it = entities.find(name);
        if (it == entities.end()) {
            auto entity = std::make_shared<ErEntity>();
            entity->id = "entity-" + name + "-" + std::to_string(entitiesList.size());
            entity->label = name;
            entity->alias = alias;
            entity->cssClasses = "default";
            entities[name] = entity;
            entitiesList.push_back({name, entity});
            return entity;
        }
        if (it->second->alias.empty() && !alias.empty()) {
            it->second->alias = alias;
        }
        return it->second;
    }

    void addAttributes(const std::string& name, const std::vector<ErAttribute>& attributes) {
        auto entity = addEntity(name);
        for (int i = static_cast<int>(attributes.size()) - 1; i >= 0; i--) {
            entity->attributes.push_back(attributes[i]);
        }
    }

    void addRelationship(const std::string& entA, const std::string& roleA, const std::string& entB, const RelSpec& relSpec) {
        std::string entityA = subgraphLookup.count(entA) ? entA : addEntity(entA)->id;
        std::string entityB = subgraphLookup.count(entB) ? entB : addEntity(entB)->id;
        relationships.push_back({entityA, roleA, entityB, relSpec});
    }

    void setDirection(const std::string& dir) {
        direction = dir;
    }

    void addCssStyles(const std::vector<std::string>& ids, const std::vector<std::string>& styles) {
        for (const auto& id : ids) {
            auto itEnt = entities.find(id);
            if (itEnt != entities.end()) {
                for (const auto& style : styles) itEnt->second->cssStyles.push_back(style);
            }
            auto itSg = subgraphLookup.find(id);
            if (itSg != subgraphLookup.end()) {
                for (const auto& style : styles) itSg->second->cssStyles.push_back(style);
            }
        }
    }

    void addClass(const std::vector<std::string>& ids, const std::vector<std::string>& styles) {
        for (const auto& id : ids) {
            auto& def = classes[id];
            def.id = id;
            for (const auto& style : styles) {
                if (style.find("color") != std::string::npos) {
                    std::string r = style;
                    size_t pos = r.find("fill");
                    if (pos != std::string::npos) r.replace(pos, 4, "bgFill");
                    def.textStyles.push_back(r);
                }
                def.styles.push_back(style);
            }
        }
    }

    void setClass(const std::vector<std::string>& ids, const std::vector<std::string>& classNames) {
        for (const auto& id : ids) {
            auto itEnt = entities.find(id);
            if (itEnt != entities.end()) {
                for (const auto& name : classNames) itEnt->second->cssClasses += " " + name;
            }
            auto itSg = subgraphLookup.find(id);
            if (itSg != subgraphLookup.end()) {
                for (const auto& name : classNames) itSg->second->classes.push_back(name);
            }
        }
    }

    std::string addSubGraph(const std::string& rawId, const std::vector<DocItem>& list, const std::string& rawTitle, const std::string& rawLabelType = "") {
        std::string id = trimStr(rawId);
        Pele::FlatSet<std::string> seen;
        std::vector<std::string> nodes;
        std::string dir;
        bool hasDir = false;
        for (const auto& item : list) {
            if (item.isDir) {
                dir = item.value;
                hasDir = true;
                continue;
            }
            std::string trimmed = trimStr(item.value);
            if (trimmed.empty() || seen.count(trimmed)) continue;
            seen.insert(trimmed);
            if (!claimed.count(item.value)) nodes.push_back(item.value);
        }
        for (const auto& node : nodes) claimed.insert(node);
        auto subgraph = std::make_shared<ErSubgraph>();
        subgraph->id = id;
        subgraph->nodes = std::move(nodes);
        subgraph->title = trimStr(rawTitle);
        subgraph->dir = dir;
        subgraph->hasDir = hasDir;
        subgraph->labelType = (rawLabelType == "markdown" || rawLabelType == "string" || rawLabelType == "text") ? rawLabelType : "markdown";
        subgraphs.push_back(subgraph);
        subgraphLookup[id] = subgraph;
        return id;
    }

    void setAccTitle(const std::string& text) {
        accTitle = trimStartStr(text);
    }

    void setAccDescription(const std::string& text) {
        std::string out;
        out.reserve(text.size());
        for (size_t i = 0; i < text.size();) {
            if (text[i] == '\n') {
                out.push_back('\n');
                i++;
                while (i < text.size()) {
                    size_t next = i;
                    int cp = charCodeAtUtf8(text, i, &next);
                    if (!isSpace(cp)) break;
                    i = next;
                }
            } else {
                out.push_back(text[i++]);
            }
        }
        accDescr = std::move(out);
    }
};

// ============================================================================
// src/diagrams/er/graph.ts & src/diagrams/er/markers.ts
// ============================================================================

struct ErGraphNode {
    std::string id;
    std::string label;
    std::string alias;
    std::vector<ErAttribute> attributes;
    bool isGroup = false;
    std::string parentId;
    bool hasParentId = false;
    std::vector<std::string> cssStyles;
    std::vector<std::string> cssCompiledStyles;
    std::string cssClasses;
    std::string dir;
    bool hasDir = false;
    std::string labelType = "markdown";
};

struct ErGraphEdge {
    std::string id;
    std::string start;
    std::string end;
    std::string label;
    std::string arrowTypeStart;
    std::string arrowTypeEnd;
    std::string pattern; // "solid" or "dashed"
};

struct ErGraph {
    std::vector<ErGraphNode> nodes;
    std::vector<ErGraphEdge> edges;
};

inline ErGraph buildErGraph(const ErDb& model) noexcept {
    std::vector<ErGraphNode> nodes;
    std::vector<ErGraphEdge> edges;
    const auto& subgraphs = model.subgraphs;

    auto compiled = [&](const std::vector<std::string>& classNames) -> std::vector<std::string> {
        std::vector<std::string> out;
        for (const auto& name : classNames) {
            auto it = model.classes.find(name);
            if (it == model.classes.end()) continue;
            for (const auto& style : it->second.styles) out.push_back(trimStr(style));
            for (const auto& style : it->second.textStyles) out.push_back(trimStr(style));
        }
        return out;
    };

    Pele::FlatMap<std::string, std::string> parentOf;
    Pele::FlatSet<std::string> subgraphIds;
    for (int i = static_cast<int>(subgraphs.size()) - 1; i >= 0; i--) {
        const auto& subgraph = *subgraphs[i];
        subgraphIds.insert(subgraph.id);
        for (const auto& id : subgraph.nodes) parentOf[id] = subgraph.id;
    }

    for (int i = static_cast<int>(subgraphs.size()) - 1; i >= 0; i--) {
        const auto& subgraph = *subgraphs[i];
        ErGraphNode gn;
        gn.id = subgraph.id;
        gn.label = subgraph.title;
        gn.isGroup = true;
        auto itP = parentOf.find(subgraph.id);
        if (itP != parentOf.end()) {
            gn.parentId = itP->second;
            gn.hasParentId = true;
        }
        gn.cssStyles = subgraph.cssStyles;
        gn.cssCompiledStyles = compiled(subgraph.classes);
        for (size_t k = 0; k < subgraph.classes.size(); k++) {
            if (k > 0) gn.cssClasses += " ";
            gn.cssClasses += subgraph.classes[k];
        }
        gn.dir = subgraph.dir;
        gn.hasDir = subgraph.hasDir;
        gn.labelType = subgraph.labelType;
        nodes.push_back(std::move(gn));
    }

    Pele::FlatMap<std::string, std::string> moved;
    for (const auto& [name, entityPtr] : model.entitiesList) {
        const auto& entity = *entityPtr;
        if (subgraphIds.count(name)) {
            moved[entity.id] = name;
            continue;
        }
        ErGraphNode gn;
        gn.id = entity.id;
        gn.label = entity.label;
        gn.alias = entity.alias;
        gn.attributes = entity.attributes;
        gn.isGroup = false;
        auto itP = parentOf.find(name);
        if (itP != parentOf.end()) {
            gn.parentId = itP->second;
            gn.hasParentId = true;
        }
        gn.cssStyles = entity.cssStyles;
        gn.cssCompiledStyles = compiled(splitStr(entity.cssClasses, ' '));
        gn.cssClasses = entity.cssClasses;
        gn.labelType = "markdown";
        nodes.push_back(std::move(gn));
    }

    for (size_t index = 0; index < model.relationships.size(); index++) {
        const auto& relationship = model.relationships[index];
        auto itA = moved.find(relationship.entityA);
        auto itB = moved.find(relationship.entityB);
        ErGraphEdge ge;
        ge.id = "id_" + relationship.entityA + "_" + relationship.entityB + "_" + std::to_string(index);
        ge.start = (itA != moved.end()) ? itA->second : relationship.entityA;
        ge.end = (itB != moved.end()) ? itB->second : relationship.entityB;
        ge.label = relationship.roleA;
        ge.arrowTypeStart = toLowerAscii(relationship.relSpec.cardB);
        ge.arrowTypeEnd = toLowerAscii(relationship.relSpec.cardA);
        ge.pattern = (relationship.relSpec.relType == "IDENTIFYING") ? "solid" : "dashed";
        edges.push_back(std::move(ge));
    }

    return {std::move(nodes), std::move(edges)};
}

constexpr double MARKER_LENGTH = 18.0;

inline std::string erMarker(const std::string& type, double x, double y, double dx, double dy) noexcept {
    constexpr double BAR = 5.0;
    constexpr double FOOT = 6.0;
    constexpr double FOOT_LENGTH = 10.0;
    constexpr double RING = 3.5;

    auto at = [&](double back, double side) -> std::string {
        return Pele::cat({num(x - dx * back - dy * side), ",", num(y - dy * back + dx * side)});
    };
    auto bar = [&](double back) -> std::string {
        return Pele::cat({"M", at(back, -BAR), "L", at(back, BAR)});
    };
    std::string foot = Pele::cat({"M", at(0.0, -FOOT), "L", at(FOOT_LENGTH, 0.0), "L", at(0.0, FOOT)});
    auto ring = [&](double back) -> std::string {
        return Pele::cat({"<circle class=\"pele-marker\" cx=\"", num(x - dx * back), "\" cy=\"", num(y - dy * back), "\" r=\"3.5\" fill=\"var(--_bg)\"/>"});
    };
    auto path = [](const std::string& d) -> std::string {
        return Pele::cat({"<path class=\"pele-marker\" d=\"", d, "\"/>"});
    };

    if (type == "only_one") return path(bar(6.0) + bar(10.0));
    if (type == "zero_or_one") return path(bar(6.0)) + ring(13.5);
    if (type == "one_or_more") return path(foot + bar(FOOT_LENGTH + 3.5));
    if (type == "zero_or_more") return path(foot) + ring(FOOT_LENGTH + RING + 0.5);
    if (type == "md_parent") return Pele::cat({"<path class=\"pele-marker\" d=\"M", at(0.0, 0.0), "L", at(6.0, 4.0), "L", at(12.0, 0.0), "L", at(6.0, -4.0), "Z\" fill=\"var(--_bg)\"/>"});
    return "";
}

// ============================================================================
// src/diagrams/er/tokens.ts, lexer.ts & parser.ts
// ============================================================================

namespace detail_parse {

enum T : int {
    END = 0,
    ERROR = 1,
    ER_DIAGRAM = 2,
    EOF_TOK = 3,
    NEWLINE = 4,
    COLON = 5,
    STYLE_SEPARATOR = 6,
    BLOCK_START = 7,
    BLOCK_STOP = 8,
    SQS = 9,
    SQE = 10,
    acc_title = 11,
    acc_title_value = 12,
    acc_descr = 13,
    acc_descr_value = 14,
    acc_descr_multiline_value = 15,
    END_KW = 16,
    SUBGRAPH = 17,
    direction_tb = 18,
    direction_bt = 19,
    direction_rl = 20,
    direction_lr = 21,
    CLASSDEF = 22,
    UNICODE_TEXT = 23,
    STYLE_TEXT = 24,
    COMMA = 25,
    CLASS = 26,
    STYLE = 27,
    SEMI = 28,
    NUM = 29,
    BRKT = 30,
    ENTITY_NAME = 31,
    DECIMAL_NUM = 32,
    ENTITY_ONE = 33,
    ATTRIBUTE_WORD = 34,
    ATTRIBUTE_KEY = 35,
    COMMENT = 36,
    ZERO_OR_ONE = 37,
    ZERO_OR_MORE = 38,
    ONE_OR_MORE = 39,
    ONLY_ONE = 40,
    MD_PARENT = 41,
    NON_IDENTIFYING = 42,
    IDENTIFYING = 43,
    WORD = 44,
    CHAR = 45
};

inline const char* const TOKEN_NAMES[] = {
    "$end",
    "INVALID",
    "ER_DIAGRAM",
    "EOF",
    "NEWLINE",
    "COLON",
    "STYLE_SEPARATOR",
    "BLOCK_START",
    "BLOCK_STOP",
    "SQS",
    "SQE",
    "acc_title",
    "acc_title_value",
    "acc_descr",
    "acc_descr_value",
    "acc_descr_multiline_value",
    "END",
    "SUBGRAPH",
    "direction_tb",
    "direction_bt",
    "direction_rl",
    "direction_lr",
    "CLASSDEF",
    "UNICODE_TEXT",
    "STYLE_TEXT",
    "COMMA",
    "CLASS",
    "STYLE",
    "SEMI",
    "NUM",
    "BRKT",
    "ENTITY_NAME",
    "DECIMAL_NUM",
    "ENTITY_ONE",
    "ATTRIBUTE_WORD",
    "ATTRIBUTE_KEY",
    "COMMENT",
    "ZERO_OR_ONE",
    "ZERO_OR_MORE",
    "ONE_OR_MORE",
    "ONLY_ONE",
    "MD_PARENT",
    "NON_IDENTIFYING",
    "IDENTIFYING",
    "WORD",
    "CHAR"
};

struct Tokens {
    std::vector<int> types;
    std::vector<std::string> texts;
    std::vector<size_t> starts;
};

enum S : int {
    S_INITIAL = 0,
    S_block = 1,
    S_block_bq = 2,
    S_acc_title = 3,
    S_acc_descr = 4,
    S_acc_descr_multiline = 5,
    S_style = 6
};

inline Tokens tokenize(const std::string& src) {
    constexpr uint8_t TEXT_M = 1;
    constexpr uint8_t STYLE_TEXT_M = 2;
    constexpr uint8_t ATTR_START_M = 4;
    constexpr uint8_t ATTR_REST_M = 8;

    static const auto CLASS_TAB = [&]() {
        std::array<uint8_t, 128> tab{};
        for (int c = 0; c < 128; c++) {
            bool word = (c >= 48 && c <= 57) || (c >= 65 && c <= 90) || (c >= 97 && c <= 122) || c == 95;
            bool letter = word && (c < 48 || c > 57);
            if (word) tab[c] |= TEXT_M | STYLE_TEXT_M | ATTR_REST_M;
            if (letter) tab[c] |= ATTR_START_M;
        }
        tab['-'] |= TEXT_M | STYLE_TEXT_M | ATTR_REST_M;
        tab['*'] |= TEXT_M | STYLE_TEXT_M | ATTR_REST_M | ATTR_START_M;
        tab['.'] |= TEXT_M | ATTR_REST_M;
        for (char ch : {'[', ']', '(', ')', ','}) tab[static_cast<unsigned char>(ch)] |= ATTR_REST_M;
        return tab;
    }();

    auto isAttr = [&](int c, uint8_t kind) -> bool {
        if (c < 0) return false;
        return c < 128 ? (CLASS_TAB[c] & kind) != 0 : (c >= 0xc0 || c == 0xb5);
    };

    const size_t n = src.size();
    std::vector<int> types;
    std::vector<std::string> texts;
    std::vector<size_t> starts;
    int state = S_INITIAL;
    size_t p = 0;

    auto emit = [&](int type, size_t s, size_t e) {
        types.push_back(type);
        texts.push_back(src.substr(s, e - s));
        starts.push_back(s);
        p = e;
    };
    auto kwAt = [&](size_t pos, const std::string& word) -> bool {
        if (pos + word.size() > n) return false;
        for (size_t i = 0; i < word.size(); i++) {
            int k = static_cast<unsigned char>(word[i]);
            int c = static_cast<unsigned char>(src[pos + i]);
            if (c != k && (k < 97 || k > 122 || (c | 32) != k)) return false;
        }
        return true;
    };
    auto kw = [&](const std::string& word) -> bool {
        return kwAt(p, word);
    };
    auto kwb = [&](const std::string& word) -> bool {
        return kw(word) && !isWord(charCodeAt(src, static_cast<int>(p + word.size())));
    };
    auto lower = [&](size_t q) -> int {
        int c = charCodeAt(src, static_cast<int>(q));
        return (c >= 65 && c <= 90) ? (c + 32) : c;
    };
    auto nextLineEnd = [&](size_t q) -> size_t {
        while (q < n) {
            size_t next = q;
            int cp = charCodeAtUtf8(src, q, &next);
            if (isLineEnd(cp)) return q;
            q = next;
        }
        return n;
    };
    auto digits = [&](size_t q) -> size_t {
        while (q < n && isDigit(charCodeAt(src, static_cast<int>(q)))) q++;
        return q;
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
    auto directionMatch = [&]() -> bool {
        size_t lineEnd = nextLineEnd(p);
        static const char* DIRS[4] = {"tb", "bt", "rl", "lr"};
        for (int i = 0; i < 4; i++) {
            for (size_t pos = p; pos + 9 <= lineEnd; pos++) {
                if (!kwAt(pos, "direction")) continue;
                size_t wsStart = pos + 9;
                size_t wsEnd = skipWs(wsStart);
                if (wsEnd > wsStart && kwAt(wsEnd, DIRS[i])) {
                    size_t endMatch = wsEnd + 2;
                    while (endMatch < n && src[endMatch] != '\n') endMatch++;
                    emit(T::direction_tb + i, p, endMatch);
                    return true;
                }
            }
        }
        return false;
    };
    auto quoted = [&]() {
        size_t e = src.find('"', p + 1);
        if (e == std::string::npos) {
            emit(T::CHAR, p, p + 1);
            return;
        }
        bool name = (e > p + 1);
        for (size_t q = p + 1; name && q < e; q++) {
            unsigned char c = static_cast<unsigned char>(src[q]);
            if (c == 37 || c == 92 || c == 13 || c == 10 || c == 11 || c == 8) name = false;
        }
        emit(name ? T::ENTITY_NAME : T::WORD, p, e + 1);
    };
    auto number = [&](int c) {
        int d = charCodeAt(src, static_cast<int>(p + 1));
        if (d == 43 && (c == 49 || c == 48)) {
            emit(c == 49 ? T::ONE_OR_MORE : T::ZERO_OR_MORE, p, p + 2);
            return;
        }
        size_t q = digits(p + 1);
        if (charCodeAt(src, static_cast<int>(q)) == 46 && isDigit(charCodeAt(src, static_cast<int>(q + 1)))) {
            emit(T::DECIMAL_NUM, p, digits(q + 2));
            return;
        }
        if (c == 49) {
            size_t nextD = p + 1;
            int cpD = (p + 1 < n) ? charCodeAtUtf8(src, p + 1, &nextD) : -1;
            if (isSpace(cpD)) {
                size_t r = skipWs(nextD);
                int f = charCodeAt(src, static_cast<int>(r));
                if (isWord(f) || f == 34 || f == 39) {
                    emit(T::ONLY_ONE, p, p + 1);
                    return;
                }
            } else if (d == 45 || d == 46) {
                int f = charCodeAt(src, static_cast<int>(p + 2));
                if (f == 45 || f == 46) {
                    emit(T::ONLY_ONE, p, p + 1);
                    return;
                }
            }
            if (!isWord(d)) {
                emit(T::ENTITY_ONE, p, p + 1);
                return;
            }
        }
        emit(T::NUM, p, q);
    };
    auto keyword = [&](int lc) -> bool {
        int d = charCodeAt(src, static_cast<int>(p + 1));
        switch (lc) {
            case 101:
                if (kwb("erdiagram")) emit(T::ER_DIAGRAM, p, p + 9);
                else if (kwb("end")) {
                    size_t e = skipWs(p + 3);
                    emit(T::END_KW, p, e);
                } else return false;
                return true;
            case 115:
                if (kwb("style")) {
                    state = S_style;
                    emit(T::STYLE, p, p + 5);
                } else if (kwb("subgraph")) emit(T::SUBGRAPH, p, p + 8);
                else return false;
                return true;
            case 99:
                if (kwb("classdef")) {
                    state = S_style;
                    emit(T::CLASSDEF, p, p + 8);
                } else if (kwb("class")) emit(T::CLASS, p, p + 5);
                else return false;
                return true;
            case 111:
                if (kwb("one or zero")) emit(T::ZERO_OR_ONE, p, p + 11);
                else if (kwb("one or more") || kwb("one or many")) emit(T::ONE_OR_MORE, p, p + 11);
                else if (kwb("one")) emit(T::ONLY_ONE, p, p + 3);
                else if (kwb("only one")) emit(T::ONLY_ONE, p, p + 8);
                else if (d == 124) emit(T::ZERO_OR_ONE, p, p + 2);
                else if (d == 123) emit(T::ZERO_OR_MORE, p, p + 2);
                else if (kwb("optionally to")) emit(T::NON_IDENTIFYING, p, p + 13);
                else return false;
                return true;
            case 122:
                if (kwb("zero or one")) emit(T::ZERO_OR_ONE, p, p + 11);
                else if (kwb("zero or more") || kwb("zero or many")) emit(T::ZERO_OR_MORE, p, p + 12);
                else return false;
                return true;
            case 109:
                if (kw("many(0)")) emit(T::ZERO_OR_MORE, p, p + 7);
                else if (kw("many(1)")) emit(T::ONE_OR_MORE, p, p + 7);
                else if (kwb("many")) emit(T::ZERO_OR_MORE, p, p + 4);
                else return false;
                return true;
            case 116:
                if (!kwb("to")) return false;
                emit(T::IDENTIFYING, p, p + 2);
                return true;
            case 117:
                if (d != 46 && d != 45 && d != 124) return false;
                emit(T::MD_PARENT, p, p + 1);
                return true;
            case 124:
                if (lower(p + 1) == 111 && !isWord(charCodeAt(src, static_cast<int>(p + 2)))) emit(T::ZERO_OR_ONE, p, p + 2);
                else if (d == 124) emit(T::ONLY_ONE, p, p + 2);
                else if (d == 123) emit(T::ONE_OR_MORE, p, p + 2);
                else return false;
                return true;
            case 125:
                if (lower(p + 1) == 111 && !isWord(charCodeAt(src, static_cast<int>(p + 2)))) emit(T::ZERO_OR_MORE, p, p + 2);
                else if (d == 124) emit(T::ONE_OR_MORE, p, p + 2);
                else return false;
                return true;
            case 46:
                if (d != 46 && d != 45) return false;
                emit(T::NON_IDENTIFYING, p, p + 2);
                return true;
            case 45:
                if (d == 45) emit(T::IDENTIFYING, p, p + 2);
                else if (d == 46) emit(T::NON_IDENTIFYING, p, p + 2);
                else return false;
                return true;
        }
        return false;
    };
    auto initial = [&](int c, size_t nextCpPos) {
        if ((c == 97 || c == 65) && kw("acc")) {
            bool isTitle = kw("acctitle");
            bool isDescr = !isTitle && kw("accdescr");
            if (isTitle || isDescr) {
                size_t q = skipWs(p + 8);
                int d = charCodeAt(src, static_cast<int>(q));
                if (d == 58) {
                    state = isTitle ? S_acc_title : S_acc_descr;
                    emit(isTitle ? T::acc_title : T::acc_descr, p, skipWs(q + 1));
                    return;
                }
                if (d == 123 && isDescr) {
                    state = S_acc_descr_multiline;
                    p = skipWs(q + 1);
                    return;
                }
            }
        }
        if (directionMatch()) return;

        switch (c) {
            case 32:
            case 9:
            case 13: {
                size_t e = p + 1;
                while (e < n) {
                    unsigned char d = static_cast<unsigned char>(src[e]);
                    if (d != 32 && d != 9 && d != 13) break;
                    e++;
                }
                p = e;
                return;
            }
            case 10: {
                size_t e = p + 1;
                while (e < n && src[e] == 10) e++;
                emit(T::NEWLINE, p, e);
                return;
            }
            case 34:
                quoted();
                return;
            case 123:
                state = S_block;
                emit(T::BLOCK_START, p, p + 1);
                return;
            case 35:
                emit(T::BRKT, p, p + 1);
                return;
            case 44:
                emit(T::COMMA, p, p + 1);
                return;
            case 58:
                if (charCodeAt(src, static_cast<int>(p + 1)) == 58 && charCodeAt(src, static_cast<int>(p + 2)) == 58) {
                    emit(T::STYLE_SEPARATOR, p, p + 3);
                } else {
                    emit(T::COLON, p, p + 1);
                }
                return;
            case 91:
                emit(T::SQS, p, p + 1);
                return;
            case 93:
                emit(T::SQE, p, p + 1);
                return;
        }
        if (isDigit(c)) {
            number(c);
            return;
        }
        if (keyword((c >= 65 && c <= 90) ? (c + 32) : c)) return;
        if (c >= 128 || (c >= 0 && (CLASS_TAB[c] & TEXT_M) != 0)) {
            size_t e = nextCpPos;
            while (e < n) {
                size_t next = e;
                int d = charCodeAtUtf8(src, e, &next);
                if (d >= 128 || (d >= 0 && (CLASS_TAB[d] & TEXT_M) != 0)) e = next;
                else break;
            }
            emit(T::UNICODE_TEXT, p, e);
            return;
        }
        emit(T::CHAR, p, nextCpPos);
    };
    auto genericMatch = [&]() -> bool {
        size_t tildeNext = src.find('~', p);
        if (tildeNext == std::string::npos) return false;
        size_t wsNext = p;
        while (wsNext < n) {
            size_t next = wsNext;
            int cp = charCodeAtUtf8(src, wsNext, &next);
            if (isSpace(cp)) break;
            wsNext = next;
        }
        if (tildeNext >= wsNext) return false;
        size_t tildeLine = nextLineEnd(p);
        if (tildeLine == 0) return false;
        size_t tildeLast = src.rfind('~', tildeLine - 1);
        if (tildeLast == std::string::npos || tildeLast <= tildeNext) return false;
        size_t e = tildeLast + 1;
        while (e < n) {
            size_t next = e;
            int cp = charCodeAtUtf8(src, e, &next);
            if (isSpace(cp)) break;
            e = next;
        }
        emit(T::ATTRIBUTE_WORD, p, e);
        return true;
    };
    auto block = [&](int c, size_t nextCpPos) {
        if (isSpace(c)) {
            p = skipWs(nextCpPos);
            return;
        }
        int lc = c | 32;
        if ((lc == 112 || lc == 102 || lc == 117) &&
            (charCodeAt(src, static_cast<int>(p + 1)) | 32) == 107 &&
            !isWord(charCodeAt(src, static_cast<int>(p + 2)))) {
            emit(T::ATTRIBUTE_KEY, p, p + 2);
            return;
        }
        if (genericMatch()) return;
        if (isAttr(c, ATTR_START_M)) {
            size_t e = nextCpPos;
            while (e < n) {
                size_t next = e;
                int d = charCodeAtUtf8(src, e, &next);
                if (isAttr(d, ATTR_REST_M)) e = next;
                else break;
            }
            emit(T::ATTRIBUTE_WORD, p, e);
            return;
        }
        if (c == 96) {
            state = S_block_bq;
            p++;
            return;
        }
        if (c == 34) {
            size_t e = src.find('"', p + 1);
            if (e != std::string::npos) {
                emit(T::COMMENT, p, e + 1);
                return;
            }
        } else if (c == 125) {
            state = S_INITIAL;
            emit(T::BLOCK_STOP, p, p + 1);
            return;
        }
        emit(T::CHAR, p, nextCpPos);
    };

    while (p < n) {
        size_t nextCpPos = p;
        int c = charCodeAtUtf8(src, p, &nextCpPos);
        switch (state) {
            case S_INITIAL:
                initial(c, nextCpPos);
                break;
            case S_block:
                block(c, nextCpPos);
                break;
            case S_block_bq:
                if (c == 96) {
                    state = S_block;
                    p++;
                } else {
                    size_t e = src.find('`', p);
                    emit(T::ATTRIBUTE_WORD, p, e == std::string::npos ? n : e);
                }
                break;
            case S_acc_title:
            case S_acc_descr: {
                size_t e = src.find('\n', p);
                emit(state == S_acc_title ? T::acc_title_value : T::acc_descr_value, p, e == std::string::npos ? n : e);
                state = S_INITIAL;
                break;
            }
            case S_acc_descr_multiline:
                if (c == 125) {
                    state = S_INITIAL;
                    p++;
                } else {
                    size_t e = src.find('}', p);
                    emit(T::acc_descr_multiline_value, p, e == std::string::npos ? n : e);
                }
                break;
            case S_style:
                if (c == 10) {
                    size_t e = p + 1;
                    while (e < n && src[e] == 10) e++;
                    state = S_INITIAL;
                    emit(T::NEWLINE, p, e);
                } else if (isSpace(c)) {
                    p = skipWs(nextCpPos);
                } else if (c == 58) emit(T::COLON, p, p + 1);
                else if (c == 44) emit(T::COMMA, p, p + 1);
                else if (c == 35) emit(T::BRKT, p, p + 1);
                else if (c == 59) emit(T::SEMI, p, p + 1);
                else if (c >= 128 || (c >= 0 && (CLASS_TAB[c] & STYLE_TEXT_M) != 0)) {
                    size_t e = nextCpPos;
                    while (e < n) {
                        size_t next = e;
                        int d = charCodeAtUtf8(src, e, &next);
                        if (d >= 128 || (d >= 0 && (CLASS_TAB[d] & STYLE_TEXT_M) != 0)) e = next;
                        else break;
                    }
                    emit(T::STYLE_TEXT, p, e);
                } else {
                    types.push_back(T::ERROR);
                    texts.push_back("");
                    starts.push_back(p);
                    return {std::move(types), std::move(texts), std::move(starts)};
                }
                break;
        }
    }

    int last = (state == S_INITIAL) ? T::EOF_TOK
        : (state == S_acc_title) ? T::acc_title_value
        : (state == S_acc_descr) ? T::acc_descr_value
        : (state == S_acc_descr_multiline) ? T::acc_descr_multiline_value
        : -1;
    if (last != -1) {
        types.push_back(last);
        texts.push_back("");
        starts.push_back(n);
    }
    types.push_back(T::END);
    texts.push_back("");
    starts.push_back(n);
    return {std::move(types), std::move(texts), std::move(starts)};
}

inline void parseEr(const std::string& src, ErDb& db) {
    static const std::unordered_map<int, std::string> CARDINALITY = {
        {T::ZERO_OR_ONE, "ZERO_OR_ONE"},
        {T::ZERO_OR_MORE, "ZERO_OR_MORE"},
        {T::ONE_OR_MORE, "ONE_OR_MORE"},
        {T::ONLY_ONE, "ONLY_ONE"},
        {T::MD_PARENT, "MD_PARENT"}
    };
    static const char* const DIRECTIONS[4] = {"TB", "BT", "RL", "LR"};
    const std::string NAME = "'UNICODE_TEXT', 'NUM', 'ENTITY_NAME', 'DECIMAL_NUM', 'ENTITY_ONE'";
    const std::string CARDS = "'ZERO_OR_ONE', 'ZERO_OR_MORE', 'ONE_OR_MORE', 'ONLY_ONE', 'MD_PARENT'";
    const std::string LINE = Pele::cat({"'EOF', 'NEWLINE', 'title', 'acc_title', 'acc_descr', 'acc_descr_multiline_value', 'SUBGRAPH', 'direction_tb', "
        "'direction_bt', 'direction_rl', 'direction_lr', 'CLASSDEF', 'CLASS', 'STYLE', ", NAME});

    auto [types, texts, starts] = tokenize(src);
    size_t i = 0;

    auto fail = [&](const std::string& expected) {
        int t = types[i];
        if (t == T::ERROR) throw syntaxError("er", src, starts[i], "", true);
        std::string got = (t == T::CHAR) ? texts[i] : std::string(TOKEN_NAMES[t]);
        throw syntaxError("er",  src,  starts[i], Pele::cat({"Expecting ", expected, ", got '", got, "'"}));
    };
    auto expect = [&](int type) -> std::string {
        if (types[i] != type) fail("'" + std::string(TOKEN_NAMES[type]) + "'");
        return texts[i++];
    };
    auto separator = [&]() {
        int t = types[i];
        if (t != T::NEWLINE && t != T::SEMI && t != T::EOF_TOK) fail("'EOF', 'NEWLINE', 'SEMI'");
        i++;
    };
    auto isName = [](int t) -> bool {
        return t == T::UNICODE_TEXT || t == T::ENTITY_NAME || t == T::NUM || t == T::DECIMAL_NUM || t == T::ENTITY_ONE;
    };
    auto entityName = [&]() -> std::string {
        int t = types[i];
        if (!isName(t)) fail(NAME);
        std::string text = texts[i++];
        return (t == T::ENTITY_NAME && text.size() >= 2) ? text.substr(1, text.size() - 2) : text;
    };
    auto idList = [&]() -> std::vector<std::string> {
        std::vector<std::string> ids;
        for (;;) {
            int t = types[i];
            if (t != T::UNICODE_TEXT && t != T::STYLE_TEXT) fail("'UNICODE_TEXT', 'STYLE_TEXT'");
            ids.push_back(texts[i++]);
            if (types[i] != T::COMMA) return ids;
            i++;
        }
    };
    auto isStyleComponent = [](int t) -> bool {
        return t == T::STYLE_TEXT || t == T::NUM || t == T::COLON || t == T::BRKT;
    };
    auto stylesOpt = [&]() -> std::vector<std::string> {
        std::vector<std::string> styles;
        for (;;) {
            if (!isStyleComponent(types[i])) fail("'STYLE_TEXT', 'COLON', 'NUM', 'BRKT'");
            std::string style = texts[i++];
            while (isStyleComponent(types[i])) style += texts[i++];
            styles.push_back(style);
            if (types[i] != T::COMMA) return styles;
            i++;
        }
    };
    auto relSpec = [&]() -> RelSpec {
        std::string cardB = CARDINALITY.at(types[i++]);
        int r = types[i];
        if (r != T::IDENTIFYING && r != T::NON_IDENTIFYING) fail("'NON_IDENTIFYING', 'IDENTIFYING'");
        std::string relType = (r == T::IDENTIFYING) ? "IDENTIFYING" : "NON_IDENTIFYING";
        i++;
        auto itA = CARDINALITY.find(types[i]);
        if (itA == CARDINALITY.end()) fail(CARDS);
        i++;
        return {itA->second, relType, cardB};
    };
    auto role = [&]() -> std::string {
        int t = types[i];
        if (t != T::WORD && t != T::ENTITY_NAME && t != T::UNICODE_TEXT) fail("'UNICODE_TEXT', 'ENTITY_NAME', 'WORD'");
        std::string text = texts[i++];
        return (t == T::UNICODE_TEXT) ? text : (text.size() >= 2 ? text.substr(1, text.size() - 2) : "");
    };
    auto isChar = [&](const std::string& ch) -> bool {
        return types[i] == T::CHAR && texts[i] == ch;
    };
    auto attributes = [&]() -> std::vector<ErAttribute> {
        std::vector<ErAttribute> list;
        do {
            std::string type = expect(T::ATTRIBUTE_WORD);
            if (isChar("?")) type += texts[i++];
            if (types[i] != T::ATTRIBUTE_WORD) fail(endsWith(type, "?") ? "'ATTRIBUTE_WORD'" : "'ATTRIBUTE_WORD', '?'");
            ErAttribute attribute;
            attribute.type = type;
            attribute.name = texts[i++];
            if (types[i] == T::ATTRIBUTE_KEY) {
                attribute.keys.push_back(texts[i++]);
                while (isChar(",")) {
                    i++;
                    attribute.keys.push_back(expect(T::ATTRIBUTE_KEY));
                }
            }
            if (types[i] == T::COMMENT) {
                std::string c = texts[i++];
                attribute.comment = c.size() >= 2 ? c.substr(1, c.size() - 2) : "";
            }
            list.push_back(std::move(attribute));
        } while (types[i] != T::BLOCK_STOP);
        std::reverse(list.begin(), list.end());
        return list;
    };
    auto block = [&](std::vector<ErAttribute>& outAttrs) -> bool {
        if (types[i] != T::BLOCK_START) return false;
        i++;
        if (types[i] == T::BLOCK_STOP) {
            i++;
            return false;
        }
        outAttrs = attributes();
        i++;
        return true;
    };
    auto entityStatement = [&]() -> std::vector<std::string> {
        std::string a = entityName();
        int t = types[i];
        if (t == T::SQS) {
            i++;
            std::string alias = entityName();
            expect(T::SQE);
            bool hasClasses = false;
            std::vector<std::string> classes;
            if (types[i] == T::STYLE_SEPARATOR) {
                i++;
                classes = idList();
                hasClasses = true;
            }
            std::vector<ErAttribute> attrs;
            bool hasAttrs = block(attrs);
            db.addEntity(a, alias);
            if (hasAttrs) db.addAttributes(a, attrs);
            if (hasClasses) db.setClass({a}, classes);
            return {a};
        }
        bool hasClasses = false;
        std::vector<std::string> classes;
        if (t == T::STYLE_SEPARATOR) {
            i++;
            classes = idList();
            hasClasses = true;
            t = types[i];
        }
        if (CARDINALITY.count(t)) {
            RelSpec spec = relSpec();
            std::string b = entityName();
            bool hasClassesB = false;
            std::vector<std::string> classesB;
            if (types[i] == T::STYLE_SEPARATOR) {
                i++;
                classesB = idList();
                hasClassesB = true;
            }
            expect(T::COLON);
            std::string label = role();
            db.addEntity(a);
            db.addEntity(b);
            db.addRelationship(a, label, b, spec);
            if (hasClasses) db.setClass({a}, classes);
            if (hasClassesB) db.setClass({b}, classesB);
            return {a, b};
        }
        std::vector<ErAttribute> attrs;
        bool hasAttrs = block(attrs);
        db.addEntity(a);
        if (hasAttrs) db.addAttributes(a, attrs);
        if (hasClasses) db.setClass({a}, classes);
        return {a};
    };

    expect(T::ER_DIAGRAM);
    struct Frame {
        std::string id;
        std::string title;
        std::vector<DocItem> list;
    };
    std::vector<Frame> frames = {{"", "", {}}};
    bool nested = false;

    for (;;) {
        int t = types[i];
        switch (t) {
            case T::NEWLINE:
                i++;
                break;
            case T::EOF_TOK:
                i++;
                if (nested) fail(Pele::cat({"'END', ", LINE}));
                return;
            case T::END_KW: {
                if (!nested) fail(LINE);
                i++;
                std::string id = db.addSubGraph(frames.back().id, frames.back().list, frames.back().title);
                frames.pop_back();
                nested = (frames.size() > 1);
                if (nested) frames.back().list.push_back({false, id});
                break;
            }
            case T::SUBGRAPH: {
                i++;
                std::string id = entityName();
                std::string title = id;
                if (types[i] == T::SQS) {
                    i++;
                    title = entityName();
                    while (isName(types[i])) title += " " + entityName();
                    expect(T::SQE);
                }
                separator();
                frames.push_back({id, title, {}});
                nested = true;
                break;
            }
            case T::acc_title: {
                i++;
                std::string value = trimStr(expect(T::acc_title_value));
                db.setAccTitle(value);
                if (nested) frames.back().list.push_back({false, value});
                break;
            }
            case T::acc_descr: {
                i++;
                std::string value = trimStr(expect(T::acc_descr_value));
                db.setAccDescription(value);
                if (nested) frames.back().list.push_back({false, value});
                break;
            }
            case T::acc_descr_multiline_value: {
                std::string value = trimStr(texts[i++]);
                db.setAccDescription(value);
                if (nested) frames.back().list.push_back({false, value});
                break;
            }
            case T::direction_tb:
            case T::direction_bt:
            case T::direction_rl:
            case T::direction_lr: {
                i++;
                std::string value = DIRECTIONS[t - T::direction_tb];
                if (nested) frames.back().list.push_back({true, value});
                else db.setDirection(value);
                break;
            }
            case T::CLASSDEF: {
                std::string keyword = texts[i++];
                auto ids = idList();
                auto styles = stylesOpt();
                separator();
                db.addClass(ids, styles);
                if (nested) frames.back().list.push_back({false, keyword});
                break;
            }
            case T::CLASS: {
                std::string keyword = texts[i++];
                auto ids = idList();
                db.setClass(ids, idList());
                if (nested) frames.back().list.push_back({false, keyword});
                break;
            }
            case T::STYLE: {
                std::string keyword = texts[i++];
                auto ids = idList();
                auto styles = stylesOpt();
                separator();
                db.addCssStyles(ids, styles);
                if (nested) frames.back().list.push_back({false, keyword});
                break;
            }
            default: {
                if (!isName(t)) fail(nested ? Pele::cat({"'END', ", LINE}) : LINE);
                auto names = entityStatement();
                if (nested) {
                    for (const auto& name : names) frames.back().list.push_back({false, name});
                }
            }
        }
    }
}

} // namespace detail_parse

// ============================================================================
// src/diagrams/er/render.ts
// ============================================================================

namespace detail_render {

constexpr double NODE_SEP = 48.0;
constexpr double EDGE_SEP = 20.0;
constexpr double RANK_SEP = 64.0;
constexpr double GROUP_PAD = 20.0;
constexpr double LOOP = 28.0;
constexpr double LOOP_BAND = 38.0;
constexpr double MIN_WIDTH = 96.0;
constexpr double BOX_PAD_X = 18.0;
constexpr double BOX_PAD_Y = 12.0;
constexpr double HEAD_PAD_Y = 9.0;
constexpr double CELL_PAD_X = 12.0;
constexpr double CELL_GAP = 16.0;
constexpr double ROW_PAD = 9.0;
constexpr double COMMENT_WRAP = 260.0;
constexpr double NO_WRAP = 1e6;
constexpr double PORT_GAP = 18.0;
constexpr double PORT_MARGIN = 14.0;

template <typename Opts, typename = void>
struct HasMonospaceVec : std::false_type {};
template <typename Opts>
struct HasMonospaceVec<Opts, std::void_t<decltype(std::declval<const Opts&>().monospace)>> : std::true_type {};

template <typename Opts>
inline bool hasMonospaceOption(const Opts& opts, const std::string& item) noexcept {
    if constexpr (HasMonospaceVec<Opts>::value) {
        for (const auto& m : opts.monospace) {
            if (m == item) return true;
        }
    }
    return false;
}

inline double numberOption(const Config& config, const std::string& key, double fallback) noexcept {
    if (!config.isMap()) return fallback;
    auto itEr = config.mVal.find("er");
    if (itEr == config.mVal.end() || !itEr->second.isMap()) return fallback;
    auto it = itEr->second.mVal.find(key);
    if (it != itEr->second.mVal.end() && it->second.isNumber() && it->second.nVal > 0.0) {
        return std::min(it->second.nVal, 2000.0);
    }
    return fallback;
}

struct Row {
    Label cells[4];
    double h = 0.0;
};

struct NodeView {
    ErGraphNode node;
    ResolvedStyle style;
    Label label;
    double w = 0.0;
    double h = 0.0;
    double headH = 0.0;
    std::vector<Row> rows;
    std::vector<double> cols;
    double rowSize = 14.0;
    double keySize = 12.0;
    int mono = 0;
    bool below = false;
};

struct Loop {
    Label label;
    double band = 0.0;
    double at = 0.0;
};

inline std::string textLeft(const Label& label, double x, double y, const std::string& attrs, const IconResolver& icons, int mono = 0) noexcept {
    const auto& lines = label.lines;
    if (lines.empty()) return "";
    if (lines.size() == 1 && lines[0].size() == 1) {
        const auto& only = lines[0][0];
        if (!only.hasIcon && !only.hasMath && only.style == mono && !(mono != 0 && only.text.find("  ") != std::string::npos)) {
            return Pele::cat({"<text", textStyle(attrs, mono), " x=\"", num(x), "\" y=\"", num(y), "\">", esc(only.text), "</text>"});
        }
    }
    int shared = sharedStyle(label);
    double shift = label.size * 0.35;
    double top = y - shift - label.lineHeight / 2.0;
    std::string body;
    std::string extra;
    for (size_t i = 0; i < lines.size(); i++) {
        const auto& line = lines[i];
        double middle = top + lineMiddle(label, i);
        if (hasMath(line)) {
            extra += labelSvg(labelLine(label, i), x + label.widths[i] / 2.0, middle, attrs, icons);
            continue;
        }
        y = middle + shift;
        bool hasIcon = false;
        for (const auto& span : line) {
            if (span.hasIcon) { hasIcon = true; break; }
        }
        double sx = x;
        bool placed = false;
        for (const auto& span : line) {
            if (span.hasIcon) {
                double side = label.size * 1.1;
                Pele::appendCat(extra, {"<svg class=\"pele-icon\" data-icon=\"", escText(span.icon), "\" x=\"", num(sx), "\" y=\"", num(y - label.size * 0.35 - side / 2.0), "\" width=\"", num(side), "\" height=\"", num(side), "\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\">", (icons ? icons(span.icon) : ""), "</svg>"});
            } else if (!span.text.empty()) {
                std::string pos = (hasIcon || !placed) ? Pele::cat({" x=\"", num(sx), "\" y=\"", num(y), "\""}) : "";
                Pele::appendCat(body, {"<tspan", pos, spanStyle(span.style & ~shared), ">", esc(span.text), "</tspan>"});
                placed = true;
            }
            sx += span.width;
        }
    }
    return Pele::cat({"<text", textStyle(attrs, shared), " xml:space=\"preserve\">", body, "</text>", extra});
}

inline std::string cellText(const std::string& raw) {
    std::string converted = parseGenericTypes(raw);
    return (converted == raw) ? raw : replaceAllStr(replaceAllStr(converted, "<", "&lt;"), ">", "&gt;");
}

inline std::vector<double> straighten(const std::vector<double>& route) noexcept {
    std::vector<double> out;
    for (size_t i = 0; i < 3 && i < route.size(); i++) out.push_back(route[i]);
    for (size_t k = 3; k + 2 < route.size(); k += 3) {
        size_t prev = out.size() - 3;
        auto flat = [&](size_t axis) -> bool {
            return std::abs(out[prev + axis] - route[k + axis]) < 0.01 &&
                   std::abs(route[k + axis] - route[k + 3 + axis]) < 0.01;
        };
        if (k + 5 < route.size() && (flat(0) || flat(1))) continue;
        out.push_back(route[k]);
        out.push_back(route[k + 1]);
        out.push_back(route[k + 2]);
    }
    return out;
}

inline void spreadEnds(std::vector<CEdge>& cedges, const std::vector<CNode>& cnodes, const std::vector<NodeView>& views) noexcept {
    struct End {
        size_t edgeIdx = 0;
        size_t at = 0;
        double toward = 0.0;
    };
    std::vector<int> sideKeys;
    std::unordered_map<int, std::vector<End>> sides;
    auto add = [&](int node, size_t edgeIdx, size_t at, size_t next) {
        const auto& route = cedges[edgeIdx].route;
        const auto& c = cnodes[node];
        double dx = route[at] - c.x;
        double dy = route[at + 1] - c.y;
        int side = -1;
        if (std::abs(std::abs(dy) - c.h / 2.0) < 0.5) side = dy < 0.0 ? 0 : 2;
        else if (std::abs(std::abs(dx) - c.w / 2.0) < 0.5) side = dx < 0.0 ? 3 : 1;
        else return;
        int key = node * 4 + side;
        if (!sides.count(key)) sideKeys.push_back(key);
        sides[key].push_back({edgeIdx, at, route[next + (side & 1 ? 1 : 0)]});
    };
    for (size_t ei = 0; ei < cedges.size(); ei++) {
        const auto& e = cedges[ei];
        if (e.src == e.dst || e.route.size() < 6) continue;
        size_t last = e.route.size() - 3;
        add(e.src, ei, 0, 3);
        add(e.dst, ei, last, last - 3);
    }
    for (int key : sideKeys) {
        auto& ends = sides[key];
        if (ends.size() < 2) continue;
        int node = key >> 2;
        bool vertical = (key & 1) == 1;
        double length = vertical
            ? (cnodes[node].isGroup ? cnodes[node].h : views[node].h)
            : (cnodes[node].isGroup ? cnodes[node].w : views[node].w);
        double step = std::max(std::min(PORT_GAP, (length - 2.0 * PORT_MARGIN) / static_cast<double>(ends.size() - 1)), 0.0);
        double center = vertical ? cnodes[node].y : cnodes[node].x;
        std::stable_sort(ends.begin(), ends.end(), [](const End& a, const End& b) {
            return a.toward < b.toward;
        });
        for (size_t i = 0; i < ends.size(); i++) {
            cedges[ends[i].edgeIdx].route[ends[i].at + (vertical ? 1 : 0)] =
                center + (static_cast<double>(i) - static_cast<double>(ends.size() - 1) / 2.0) * step;
        }
    }
}

inline void inset(std::vector<double>& route, size_t at, const NodeView& view, const CNode& c) noexcept {
    if (c.isGroup) return;
    double dx = route[at] - c.x;
    double dy = route[at + 1] - c.y;
    auto sign = [](double v) -> double { return v > 0.0 ? 1.0 : (v < 0.0 ? -1.0 : 0.0); };
    if (c.w != view.w && std::abs(std::abs(dx) - c.w / 2.0) < 0.5) {
        route[at] -= (sign(dx) * (c.w - view.w)) / 2.0;
    }
    if (c.h != view.h && std::abs(std::abs(dy) - c.h / 2.0) < 0.5) {
        route[at + 1] -= (sign(dy) * (c.h - view.h)) / 2.0;
    }
}

inline Rendered draw(const ErDb& db, const Config& config, const RenderOptions& options, bool turned) noexcept {
    ErGraph graph = buildErGraph(db);
    bool monospaced = hasMonospaceOption(options, "attributes");
    double size = options.fontSize > 0.0 ? options.fontSize : 16.0;
    double smallSize = std::round(size * 0.875);
    const TextMeasurer& measurer = getMeasurer(options);
    double pad = options.hasPadding ? options.padding : 8.0;
    const auto& icons = options.icons;

    Pele::FlatMap<std::string, int> index;
    std::vector<NodeView> views;
    std::vector<CNode> cnodes;
    for (size_t i = 0; i < graph.nodes.size(); i++) {
        index[graph.nodes[i].id] = static_cast<int>(i);
    }

    std::vector<CEdge> cedges;
    std::vector<Label> edgeLabels;
    std::vector<ErGraphEdge> drawn;
    std::vector<int> loopKeys;
    std::unordered_map<int, std::vector<std::shared_ptr<Loop>>> loops;
    std::vector<std::shared_ptr<Loop>> loopOf;
    for (const auto& edge : graph.edges) {
        auto itS = index.find(edge.start);
        auto itD = index.find(edge.end);
        if (itS == index.end() || itD == index.end()) continue;
        int src = itS->second;
        int dst = itD->second;
        Label label = layoutLabel(edge.label, true, measurer, smallSize, 200.0);
        std::shared_ptr<Loop> loop;
        if (src == dst) {
            loop = std::make_shared<Loop>();
            loop->label = label;
            if (!loops.count(src)) loopKeys.push_back(src);
            loops[src].push_back(loop);
        }
        loopOf.push_back(loop);
        drawn.push_back(edge);
        edgeLabels.push_back(label);
        cedges.push_back(cedge(
            src,
            dst,
            1,
            label.width > 0.0 ? label.width + 12.0 : 0.0,
            label.height > 0.0 ? label.height + 4.0 : 0.0
        ));
    }

    auto horizontal = [&](int i) -> bool {
        int at = i;
        for (size_t steps = 0; steps < graph.nodes.size(); steps++) {
            if (!graph.nodes[at].hasParentId) break;
            auto itUp = index.find(graph.nodes[at].parentId);
            if (itUp == index.end()) break;
            int up = itUp->second;
            if (graph.nodes[up].hasDir && !graph.nodes[up].dir.empty()) {
                return graph.nodes[up].dir == "LR" || graph.nodes[up].dir == "RL";
            }
            at = up;
        }
        return db.direction == "LR" || db.direction == "RL";
    };

    for (size_t i = 0; i < graph.nodes.size(); i++) {
        const auto& node = graph.nodes[i];
        std::vector<std::string> styleDecls = node.cssCompiledStyles;
        if (!node.cssStyles.empty()) {
            styleDecls.insert(styleDecls.end(), node.cssStyles.begin(), node.cssStyles.end());
        }
        ResolvedStyle style = resolveStyle(styleDecls);
        int base = (style.bold ? Style::Bold : 0) | (style.italic ? Style::Italic : 0);
        int mono = (monospaced && style.text.find("\"font-family:") == std::string::npos && style.text.find(";font-family:") == std::string::npos) ? Style::Mono : 0;
        double nameSize = style.hasFontSize ? style.fontSize : size;
        double rowSize = style.hasFontSize ? style.fontSize : smallSize;
        double keySize = style.hasFontSize ? style.fontSize : std::round(size * 0.75);
        Label label = node.isGroup
            ? layoutLabel(node.label, true, measurer, nameSize, 4000.0, base)
            : layoutLabel(!node.alias.empty() ? node.alias : node.label, true, measurer, nameSize, 240.0, base);
        auto itOwn = loops.find(static_cast<int>(i));
        bool hasOwn = (itOwn != loops.end() && !itOwn->second.empty());
        bool below = hasOwn && !node.isGroup && horizontal(static_cast<int>(i));
        NodeView view;
        view.node = node;
        view.style = std::move(style);
        view.label = std::move(label);
        view.rowSize = rowSize;
        view.keySize = keySize;
        view.mono = mono;
        view.below = below;
        double reach = 0.0;
        double bands = 0.0;
        if (hasOwn) {
            for (auto& loop : itOwn->second) {
                loop->band = std::max(LOOP_BAND, below ? (loop->label.width + 12.0) : (loop->label.height + 6.0));
                bands += loop->band;
                reach = std::max(reach, below ? loop->label.height : loop->label.width);
            }
        }
        double minW = std::max(MIN_WIDTH, below ? (bands + 8.0) : 0.0);
        CNode c;
        if (node.isGroup) {
            c = cnode(0.0, 0.0);
            c.isGroup = true;
            if (node.hasDir && !node.dir.empty()) c.dir = direction(node.dir);
            c.padX = GROUP_PAD;
            c.padTop = view.label.height > 0.0 ? view.label.height + 16.0 : GROUP_PAD;
            c.padBottom = GROUP_PAD;
            c.minW = std::max(view.label.width + 2.0 * GROUP_PAD - 12.0, MIN_WIDTH);
        } else {
            double nameH = std::max(view.label.height, std::round(nameSize * 1.5));
            if (node.attributes.empty()) {
                view.w = std::max(view.label.width + 2.0 * BOX_PAD_X, minW);
                view.h = std::max(nameH + 2.0 * BOX_PAD_Y, below ? 0.0 : (bands + 8.0));
            } else {
                double rowLine = std::round(rowSize * 1.5);
                double widths[4] = {0.0, 0.0, 0.0, 0.0};
                for (const auto& attribute : node.attributes) {
                    std::string joinedKeys;
                    for (size_t k = 0; k < attribute.keys.size(); k++) {
                        if (k > 0) joinedKeys += ", ";
                        joinedKeys += attribute.keys[k];
                    }
                    Row row;
                    row.cells[0] = layoutLabel(cellText(attribute.type), true, measurer, rowSize, NO_WRAP, base | mono);
                    row.cells[1] = layoutLabel(cellText(attribute.name), true, measurer, rowSize, NO_WRAP, base | mono);
                    row.cells[2] = layoutLabel(joinedKeys, false, measurer, keySize, NO_WRAP, base | mono);
                    row.cells[3] = layoutLabel(cellText(attribute.comment), true, measurer, rowSize, COMMENT_WRAP, base);
                    double h = rowLine;
                    for (int k = 0; k < 4; k++) {
                        if (row.cells[k].width > widths[k]) widths[k] = row.cells[k].width;
                        if (row.cells[k].height > h) h = row.cells[k].height;
                    }
                    row.h = h + ROW_PAD;
                    view.h += row.h;
                    view.rows.push_back(std::move(row));
                }
                int columns = 0;
                double inner = 0.0;
                for (double w : widths) {
                    if (w == 0.0) continue;
                    inner += w + (columns > 0 ? CELL_GAP : 0.0);
                    columns++;
                }
                view.w = std::max({inner + 2.0 * CELL_PAD_X, view.label.width + 2.0 * BOX_PAD_X, minW});
                double spare = columns > 0 ? (view.w - inner - 2.0 * CELL_PAD_X) / static_cast<double>(columns) : 0.0;
                double x = CELL_PAD_X;
                for (double w : widths) {
                    view.cols.push_back(x);
                    if (w > 0.0) x += w + spare + CELL_GAP;
                }
                view.headH = nameH + 2.0 * HEAD_PAD_Y;
                view.h += view.headH;
            }
            double room = hasOwn ? 2.0 * (MARKER_LENGTH + LOOP * 0.75 + 10.0 + reach) : 0.0;
            c = below ? cnode(view.w, view.h + room) : cnode(view.w + room, view.h);
        }
        c.seq = static_cast<double>(views.size());
        views.push_back(std::move(view));
        cnodes.push_back(std::move(c));
    }
    for (size_t i = 0; i < graph.nodes.size(); i++) {
        if (graph.nodes[i].hasParentId) {
            auto it = index.find(graph.nodes[i].parentId);
            cnodes[i].parent = (it != index.end()) ? it->second : -1;
        } else {
            cnodes[i].parent = -1;
        }
    }

    LayeredOptions lopt;
    lopt.nodeSep = numberOption(config, "nodeSpacing", NODE_SEP);
    lopt.edgeSep = EDGE_SEP;
    lopt.rankSep = numberOption(config, "rankSpacing", RANK_SEP);
    lopt.portSep = 24.0;
    CompoundResult layout = compoundLayout(cnodes, cedges, turned ? "TB" : direction(db.direction), lopt);

    spreadEnds(cedges, cnodes, views);

    double layoutWidth = layout.width;
    for (int i : loopKeys) {
        const auto& c = cnodes[i];
        if (!c.isGroup) continue;
        double reach = 0.0;
        for (const auto& loop : loops[i]) reach = std::max(reach, loop->label.width);
        layoutWidth = std::max(layoutWidth, c.x + c.w / 2.0 + MARKER_LENGTH + LOOP * 0.75 + 10.0 + reach);
    }

    Label title = titleLabel(db.title, measurer, size, 4000.0);
    double titleHeight = title.height > 0.0 ? title.height + 12.0 : 0.0;
    double inner = std::max(layoutWidth, title.width);
    double ox = pad + (inner - layoutWidth) / 2.0;
    double oy = pad + titleHeight;
    shiftLayout(cnodes, cedges, ox, oy);
    double width = std::ceil(inner + 2.0 * pad);
    double height = std::ceil(layout.height + titleHeight + 2.0 * pad);
    std::vector<LinkInfo> links;

    Crossings crossings;
    bool anyGroup = false;
    for (const auto& c : cnodes) {
        if (c.isGroup) { anyGroup = true; break; }
    }
    if (anyGroup) {
        for (const auto& e : cedges) markCrossings(crossings, e.route);
    }

    const std::string BOX_ATTRS = " fill=\"var(--_s)\" stroke=\"var(--_b)\"";
    std::string clusters;
    std::vector<Fill> fills;
    std::string nodesOut;
    for (size_t i = 0; i < views.size(); i++) {
        auto& view = views[i];
        const auto& c = cnodes[i];
        const auto& node = view.node;
        std::string rawCls = node.cssClasses;
        if (startsWith(rawCls, "default ")) rawCls = rawCls.substr(8);
        else if (rawCls == "default") rawCls = "";
        std::string classes = classNames(rawCls);
        if (c.isGroup) {
            view.w = c.w;
            view.h = c.h;
            double x = c.x - c.w / 2.0;
            double y = c.y - c.h / 2.0;
            double titleX = clusterTitleX(c, view.label.width, crossings, GROUP_PAD);
            if (paints(view.style.shape)) {
                fills.push_back({{x, y, x + c.w, y + c.h}, view.style.shape});
            }
            Pele::appendCat(clusters, {"<g class=\"pele-cluster", classes, "\" data-id=\"", escText(node.id), "\">"
                "<rect x=\"", num(x), "\" y=\"", num(y), "\" width=\"", num(c.w), "\" height=\"", num(c.h), "\" fill=\"var(--_a)\"", (setsFill(view.style.shape) ? "" : " fill-opacity=\"0.5\""), " stroke=\"var(--_b)\"", rounded(view.style.shape), "/>", labelSvg(view.label,  titleX,  y + 8.0 + view.label.height / 2.0, Pele::cat({" class=\"pele-cluster-label\"", inkFill(view.style, "var(--_m)"), view.style.text}),  icons), "</g>"});
            continue;
        }

        double w = view.w;
        double h = view.h;
        double x0 = -w / 2.0;
        double y0 = -h / 2.0;
        const std::string& text = view.style.text;
        std::string color = inkFill(view.style);
        std::string muted = !color.empty() ? color : " fill=\"var(--_m)\"";
        std::string shape = Pele::cat({"x=\"", num(x0), "\" y=\"", num(y0), "\" width=\"", num(w), "\" height=\"", num(h), "\""});
        std::string name = labelSvg(view.label,  0.0,  !view.rows.empty() ? (y0 + view.headH / 2.0) : 0.0, Pele::cat({" class=\"pele-label\"", color, text}),  icons);
        std::string display = decodeEntities(!node.alias.empty() ? node.alias : node.label);
        if ((Pele::cat({classes, " "})).find(" internal-link ") != std::string::npos && !name.empty() && options.links) {
            bool safe = internalLink(display, options);
            std::string target = esc(display);
            name = Pele::cat({"<a class=\"internal-link\"", (safe ? Pele::cat({" href=\"", target, "\"", relAttr(options)}) : ""), " data-href=\"", target, "\">", name, "</a>"});
            links.push_back({node.label, display, true});
        }

        std::string body;
        if (view.rows.empty()) {
            body = Pele::cat({"<rect ", shape, rounded(BOX_ATTRS + view.style.shape), "/>", name});
        } else {
            double rowLine = std::round(view.rowSize * 1.5);
            std::string lineColor = view.style.hasStroke ? esc(view.style.stroke) : "var(--_b)";
            std::string stripes;
            std::string cells;
            double y = y0 + view.headH;
            for (size_t r = 0; r < view.rows.size(); r++) {
                const auto& row = view.rows[r];
                if (r % 2 == 0) {
                    std::string span = Pele::cat({"x=\"", num(x0), "\" width=\"", num(w), "\""});
                    if (r == view.rows.size() - 1) {
                        Pele::appendCat(stripes, {"<rect ", span, " y=\"", num(y), "\" height=\"", num(row.h), "\"", ROUNDED, "/>"
                            "<rect ", span, " y=\"", num(y), "\" height=\"", num(row.h / 2.0), "\"/>"});
                    } else {
                        Pele::appendCat(stripes, {"<rect ", span, " y=\"", num(y), "\" height=\"", num(row.h), "\"/>"});
                    }
                }
                double baseline = y + ROW_PAD / 2.0 + rowLine / 2.0 + view.rowSize * 0.35;
                cells +=
                    textLeft(row.cells[0],  x0 + view.cols[0],  baseline, Pele::cat({" class=\"pele-er-type\"", muted, text}),  icons,  view.mono) +
                    textLeft(row.cells[1],  x0 + view.cols[1],  baseline, Pele::cat({" class=\"pele-er-name\"", color, text}),  icons,  view.mono) +
                    textLeft(row.cells[2],  x0 + view.cols[2],  baseline, Pele::cat({" class=\"pele-er-keys\" font-size=\"", num(view.keySize), "\"", muted, text}),  icons,  view.mono) +
                    textLeft(row.cells[3],  x0 + view.cols[3],  baseline, Pele::cat({" class=\"pele-er-comment\"", muted, text}),  icons);
                y += row.h;
            }
            std::string dividerY = num(y0 + view.headH);
            std::string tone = ink(view.style.shape);
            std::string shade = (tone == ON_LIGHT) ? ON_DARK : (tone == ON_DARK) ? ON_LIGHT : "var(--_bg)";
            body = Pele::cat({"<rect ", shape, rounded(BOX_ATTRS + view.style.shape), "/>", "<g class=\"pele-er-stripes\" fill=\"", shade, "\"", (view.style.shape.find("fill:") != std::string::npos ? " opacity=\"0.6\"" : ""), ">", stripes, "</g>", "<path class=\"pele-er-divider\" d=\"M", num(x0), ",", dividerY, "H", num(x0 + w), "\" stroke=\"", lineColor, "\"/>", "<rect ", shape, " fill=\"none\" stroke=\"var(--_b)\"", rounded(view.style.line), "/>", name, "<g class=\"pele-er-attributes\" font-size=\"", num(view.rowSize), "\">", cells, "</g>"});
        }
        Pele::appendCat(nodesOut, {"<g class=\"pele-node pele-entity", classes, "\" data-id=\"", escText(node.label), "\" transform=\"translate(", num(c.x), ",", num(c.y), ")\">", body, "</g>"});
    }

    for (int i : loopKeys) {
        const auto& view = views[i];
        auto& own = loops[i];
        double total = 0.0;
        for (const auto& loop : own) total += loop->band;
        double scale = std::min(1.0, std::max((view.below ? view.w : view.h) - 8.0, 0.0) / total);
        double at = (-total * scale) / 2.0;
        for (auto& loop : own) {
            loop->band *= scale;
            loop->at = at + loop->band / 2.0;
            at += loop->band;
        }
    }

    std::string edgesOut;
    std::string casings;
    std::string labelsOut;
    for (size_t i = 0; i < cedges.size(); i++) {
        auto& ce = cedges[i];
        const auto& edge = drawn[i];
        const auto& label = edgeLabels[i];
        std::string d;
        std::string start;
        std::string end;
        const auto& loop = loopOf[i];
        if (loop) {
            const auto& c = cnodes[ce.src];
            const auto& view = views[ce.src];
            double spread = std::max(std::min(loop->band / 2.0 - 6.0, 13.0), 1.0);
            double far = MARKER_LENGTH + LOOP * 0.75 + 6.0;
            if (view.below) {
                double x = c.x + loop->at;
                double y = c.y + view.h / 2.0;
                std::string turn = num(y + MARKER_LENGTH);
                std::string out = num(y + MARKER_LENGTH + LOOP);
                d = Pele::cat({"M", num(x - spread), ",", num(y), "V", turn, "C", num(x - spread), ",", out, " ", num(x + spread), ",", out, " ", num(x + spread), ",", turn, "V", num(y)});
                start = erMarker(edge.arrowTypeStart, x - spread, y, 0.0, -1.0);
                end = erMarker(edge.arrowTypeEnd, x + spread, y, 0.0, -1.0);
                ce.labelX = x;
                ce.labelY = y + far + label.height / 2.0;
            } else {
                double x = c.x + view.w / 2.0;
                double y = c.y + loop->at;
                std::string turn = num(x + MARKER_LENGTH);
                std::string out = num(x + MARKER_LENGTH + LOOP);
                d = Pele::cat({"M", num(x), ",", num(y - spread), "H", turn, "C", out, ",", num(y - spread), " ", out, ",", num(y + spread), " ", turn, ",", num(y + spread), "H", num(x)});
                start = erMarker(edge.arrowTypeStart, x, y - spread, -1.0, 0.0);
                end = erMarker(edge.arrowTypeEnd, x, y + spread, -1.0, 0.0);
                ce.labelX = x + far + label.width / 2.0;
                ce.labelY = y;
            }
        } else {
            auto route = straighten(ce.route);
            if (route.size() < 6) continue;
            inset(route, 0, views[ce.src], cnodes[ce.src]);
            inset(route, route.size() - 3, views[ce.dst], cnodes[ce.dst]);
            double dx = std::abs(route[route.size() - 3] - route[0]);
            double dy = std::abs(route[route.size() - 2] - route[1]);
            bool direct = (route.size() == 6) && (dx < 0.01 || dy < 0.01 || std::hypot(dx, dy) <= 3.0 * MARKER_LENGTH);
            double stub = direct ? 0.0 : MARKER_LENGTH;
            EdgePath path = routePath(route, "", stub, stub);
            d = stub > 0.0 ? Pele::cat({"M", num(path.sx), ",", num(path.sy), "L", path.d.substr(1), "L", num(path.ex), ",", num(path.ey)}) : path.d;
            start = erMarker(edge.arrowTypeStart, path.sx, path.sy, path.sdx, path.sdy);
            end = erMarker(edge.arrowTypeEnd, path.ex, path.ey, path.edx, path.edy);
        }
        auto underRes = fillUnder(d, {}, fills);
        bool hasOver = (underRes.state != FillLookup::None);
        std::string over = (underRes.state == FillLookup::Found) ? edgeInk(underRes.fill) : "";
        if (hasOver && over.empty()) casings += casing(d, start + end);
        if (underRes.state == FillLookup::Found && !over.empty()) {
            start = hollowOn(start, underRes.fill);
            end = hollowOn(end, underRes.fill);
        }
        bool dashed = (edge.pattern == "dashed");
        edgesOut += "<g class=\"pele-edge pele-er-" + std::string(dashed ? "non-identifying" : "identifying") + "\" data-id=\"" + escText(edge.id) + "\"" + (!over.empty() ? Pele::cat({" stroke=\"", over, "\""}) : "") + ">"
            "<path d=\"" + d + "\"" + (dashed ? " stroke-dasharray=\"4 3\"" : "") + "/>" +
            start + end + "</g>";
        if (label.width > 0.0) {
            labelsOut += edgeLabelSvg(edge.id, label, ce.labelX, ce.labelY, "", icons);
        }
    }

    std::string svg = svgDocument(
        "er", 
        width, 
        height, 
        size, 
        options, 
        db, Pele::cat({labelSvg(title, width / 2.0, pad + title.height / 2.0, " class=\"pele-title\" font-weight=\"var(--_tw)\""), (!clusters.empty() ? Pele::cat({"<g class=\"pele-clusters\">", clusters, "</g>"}) : ""), casingsSvg(casings), (!edgesOut.empty() ? Pele::cat({"<g class=\"pele-edges\" fill=\"none\" stroke=\"var(--_l)\" stroke-linecap=\"round\">", edgesOut, "</g>"}) : ""), (!labelsOut.empty() ? Pele::cat({"<g class=\"pele-edge-labels\" font-size=\"", num(smallSize), "\">", labelsOut, "</g>"}) : ""), "<g class=\"pele-nodes\">", nodesOut, "</g>"}));

    return {std::move(svg), width, height, std::move(links)};
}

} // namespace detail_render

// ============================================================================
// Public entry point
// ============================================================================

inline Rendered render(
    const std::string& source,
    const Config& config = {},
    const RenderOptions& options = {},
    const std::string& title = ""
) {
    ErDb db;
    if (!title.empty()) db.title = title;
    detail_parse::parseEr(source, db);
    if (!runsAcross(db.direction)) return detail_render::draw(db, config, options, false);
    return turnToFit(options, [&](bool down) { return detail_render::draw(db, config, options, down); });
}

inline Rendered render(
    const std::string& source,
    const Config& config,
    const std::string& title,
    const RenderOptions& options = {}
) {
    return render(source, config, options, title);
}

} // namespace Pele::ErDiagram

namespace Pele {

inline Rendered renderEr(
    const std::string& text,
    const Config& config = {},
    const RenderOptions& options = {},
    const std::string& title = ""
) {
    return ErDiagram::render(text, config, options, title);
}

inline Rendered renderEr(
    const std::string& text,
    const Config& config,
    const std::string& title,
    const RenderOptions& options = {}
) {
    return ErDiagram::render(text, config, options, title);
}

} // namespace Pele
