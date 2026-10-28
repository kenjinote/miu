#pragma once
#include "../foundation.h"
#include "flowchart.h"

namespace Pele {
namespace Agentflow {

inline std::string afNum(double v) {
    return num(Flowchart::jsRound(v * 100.0) / 100.0);
}

// ============================================================================
// Port of src/diagrams/agentflow/colorSlots.ts
// ============================================================================

inline const std::unordered_map<std::string, int>& kindSlotMap() {
    static const std::unordered_map<std::string, int> m = {
        {"tool", 0},
        {"task", 1},
        {"decision", 2},
        {"input", 3},
        {"refdoc", 4},
        {"connector", 5},
        {"action", 6}
    };
    return m;
}

constexpr int KIND_COUNT = 7;

inline int containerSlotCount(int paletteLength) {
    return std::max(1, paletteLength - KIND_COUNT);
}

inline int containerSlot(int n, int paletteLength) {
    int slot = KIND_COUNT + (n % containerSlotCount(paletteLength));
    return paletteLength > 0 ? (slot % paletteLength) : slot;
}

// ============================================================================
// Port of src/diagrams/agentflow/shapes.ts
// ============================================================================

inline const std::unordered_map<std::string, std::string>& shapeAliasesMap() {
    static const std::unordered_map<std::string, std::string> m = {
        {"task", "roundedRect"},
        {"tool", "subroutine"},
        {"input", "lean-right"},
        {"decision", "diamond"},
        {"refdoc", "lin-doc"},
        {"action", "hexagon"},
        {"round", "rect"}
    };
    return m;
}

inline const std::unordered_set<std::string>& removedShapesSet() {
    static const std::unordered_set<std::string> s = {
        "doc",
        "stadium",
        "terminal",
        "circle",
        "trapezoid",
        "inv_trapezoid",
        "inv-trapezoid",
        "doublecircle",
        "double-circle",
        "typeDeclaration",
        "procs",
        "lean_left",
        "lean-left",
        "in-out",
        "cylinder",
        "ellipse",
        "odd",
        "tag-rect",
        "tagged-rectangle",
        "delay",
        "half-rounded-rectangle",
        "lin-rect",
        "lined-rectangle",
        "win-pane",
        "window-pane",
        "curv-trap",
        "curved-trapezoid"
    };
    return s;
}

inline const std::unordered_set<std::string>& allowedShapesSet() {
    static const std::unordered_set<std::string> s = {
        "roundedRect",
        "subroutine",
        "subprocess",
        "subproc",
        "framed-rectangle",
        "lean-right",
        "diamond",
        "lin-doc",
        "lined-document",
        "hexagon",
        "hex",
        "connector",
        "collapsedGroup"
    };
    return s;
}

inline const std::string DEFAULT_SHAPE = "roundedRect";

inline std::string resolveShapeAlias(const std::string& shape) {
    if (shape.empty()) return shape;
    const auto& m = shapeAliasesMap();
    auto it = m.find(shape);
    return it != m.end() ? it->second : shape;
}

inline bool isValidShape(const std::string& name) {
    return Flowchart::isValidShape(name) || name == "constructor";
}

// ============================================================================
// Port of src/diagrams/agentflow/types.ts & db.ts
// ============================================================================

using FlowText = Flowchart::FlowText;
using DocItem = Flowchart::DocItem;

struct Loc {
    int first_line = 0;
    int first_column = 0;
    int last_line = 0;
    int last_column = 0;
};

struct AgentNode {
    std::string id;
    std::string text;
    bool hasText = false;
    std::string labelType = "text";
    std::string type;
    bool hasType = false;
    std::vector<std::string> styles;
    std::vector<std::string> classes;
    std::string dir;
    std::map<std::string, std::string> props;
    bool hasProps = false;
    std::string link;
    std::string linkTarget;
    YamlValue metadata = YamlValue::makeNull();
    bool hasMetadata = false;
    bool isConnector = false;
};

struct AgentEdge {
    std::string start;
    std::string end;
    std::string type;
    bool hasType = false;
    std::string text;
    std::string labelType = "text";
    std::string stroke;
    std::string edgeSemantic;
    int length = 1;
    bool hasLength = false;
    std::vector<std::string> style;
    bool hasStyle = false;
    std::vector<std::string> classes;
    std::string id;
    bool isUserDefinedId = false;
    std::string interpolate;
    YamlValue animate = YamlValue::makeNull();
    bool hasAnimate = false;
    YamlValue animation = YamlValue::makeNull();
    bool hasAnimation = false;
    YamlValue metadata = YamlValue::makeNull();
    bool hasMetadata = false;
};

struct AgentClassDef {
    std::string id;
    std::vector<std::string> styles;
    std::vector<std::string> textStyles;
};

struct AgentSubgraph {
    std::string id;
    std::vector<std::string> nodes;
    std::string title;
    std::vector<std::string> classes;
    std::string dir;
    std::string labelType = "markdown";
    std::string type = "flow";
    YamlValue metadata = YamlValue::makeNull();
    bool hasMetadata = false;
};

struct AgentLinkInfo {
    std::string type;
    std::string stroke;
    int length = 1;
    bool hasLength = false;
    std::string edgeSemantic;
    FlowText text;
    bool hasText = false;
    std::string id;
    bool hasId = false;
};

struct ElementPosition {
    int startLine = 0;
    int startColumn = 0;
    int endLine = 0;
    int endColumn = 0;
    int startIndex = 0;
    int endIndex = 0;
};

struct ElementMapping {
    std::string id;
    std::string type; // 'vertex' | 'edge' | 'subgraph' | 'connector' | 'attachment'
    ElementPosition position;
    bool isBare = false;
};

struct Diagnostic {
    std::string id;
    std::string severity; // 'warning' | 'error'
    std::string message;
    std::string nodeId;
    std::string edgeId;
    ElementPosition position;
    bool hasPosition = false;
};

struct AgentflowDbOptions {
    int maxEdges = 5000;
    bool hasMaxEdges = false;
    bool inheritDir = false;
    std::string direction;
    int lineOffset = 0;
    std::string source;
    bool hasSource = false;
};

inline bool isToolDefinition(const AgentNode& vertex) {
    static const std::unordered_set<std::string> TOOL_SHAPES = {
        "subroutine", "subprocess", "subproc", "framed-rectangle", "tool"
    };
    return vertex.hasType && TOOL_SHAPES.count(vertex.type) > 0;
}

inline std::string agentLabelType(const std::string& type) {
    return (type == "markdown" || type == "string" || type == "text") ? type : "markdown";
}

inline std::string agentUnquote(const std::string& text) {
    return (text.size() >= 2 && text.front() == '"' && text.back() == '"')
        ? text.substr(1, text.size() - 2)
        : text;
}

inline YamlValue stripPrototypeKeys(const YamlValue& root) {
    if (root.isArray()) {
        YamlValue out = YamlValue::makeArray();
        for (const auto& entry : root.aVal) {
            out.aVal.push_back(stripPrototypeKeys(entry));
        }
        return out;
    }
    if (root.isMap()) {
        YamlValue out = YamlValue::makeMap();
        for (const auto& [k, v] : root.mVal) {
            if (k == "__proto__" || k == "constructor" || k == "prototype") continue;
            out.mVal[k] = stripPrototypeKeys(v);
        }
        return out;
    }
    return root;
}

inline void mergeMetadata(YamlValue& into, bool& hasInto, const YamlValue& doc) {
    if (!hasInto || !into.isMap()) {
        into = YamlValue::makeMap();
        hasInto = true;
    }
    if (doc.isMap()) {
        for (const auto& [k, v] : doc.mVal) {
            into.mVal[k] = v;
        }
    } else if (doc.isArray()) {
        for (size_t i = 0; i < doc.aVal.size(); i++) {
            into.mVal[std::to_string(i)] = doc.aVal[i];
        }
    }
}

inline bool isBlockScalarHeaderLine(const std::string& code) {
    // /:\s*[>|][\d+-]*\s*$/
    size_t colon = code.rfind(':');
    if (colon == std::string::npos) return false;
    size_t p = colon + 1;
    while (p < code.size() && isSpace(static_cast<unsigned char>(code[p]))) p++;
    if (p >= code.size() || (code[p] != '>' && code[p] != '|')) return false;
    p++;
    while (p < code.size() && (isDigit(static_cast<unsigned char>(code[p])) || code[p] == '+' || code[p] == '-')) p++;
    while (p < code.size() && isSpace(static_cast<unsigned char>(code[p]))) p++;
    return p == code.size();
}

inline std::string stripTrailingComma(const std::string& code) {
    // code.replace(/,([\t ]*)$/, '$1')
    size_t p = code.size();
    while (p > 0 && (code[p - 1] == ' ' || code[p - 1] == '\t')) p--;
    if (p > 0 && code[p - 1] == ',') {
        return code.substr(0, p - 1) + code.substr(p);
    }
    return code;
}

inline std::string stripLineTrailingCommas(const std::string& body) {
    bool inSingle = false;
    bool inDouble = false;
    int flowDepth = 0;
    int blockScalarIndent = -1;
    auto lines = splitStr(body, '\n');
    std::string result;
    for (size_t li = 0; li < lines.size(); li++) {
        if (li > 0) result.push_back('\n');
        const std::string& line = lines[li];
        if (blockScalarIndent >= 0) {
            int indent = static_cast<int>(line.size() - trimStartStr(line).size());
            if (trimStr(line).empty() || indent > blockScalarIndent) {
                result += line;
                continue;
            }
            blockScalarIndent = -1;
        }
        int commentStart = -1;
        for (size_t i = 0; i < line.size(); i++) {
            char ch = line[i];
            if (inDouble) {
                if (ch == '\\') i++;
                else if (ch == '"') inDouble = false;
            } else if (inSingle) {
                if (ch == '\'') inSingle = false;
            } else if (ch == '"') {
                inDouble = true;
            } else if (ch == '\'') {
                inSingle = true;
            } else if (ch == '#' && (i == 0 || line[i - 1] == ' ' || line[i - 1] == '\t')) {
                commentStart = static_cast<int>(i);
                break;
            } else if (ch == '[' || ch == '{') {
                flowDepth++;
            } else if (ch == ']' || ch == '}') {
                flowDepth = std::max(0, flowDepth - 1);
            }
        }
        if (inSingle || inDouble || flowDepth > 0) {
            result += line;
            continue;
        }
        std::string code = commentStart >= 0 ? line.substr(0, static_cast<size_t>(commentStart)) : line;
        std::string comment = commentStart >= 0 ? line.substr(static_cast<size_t>(commentStart)) : "";
        if (isBlockScalarHeaderLine(code)) {
            blockScalarIndent = static_cast<int>(line.size() - trimStartStr(line).size());
            result += line;
            continue;
        }
        result += stripTrailingComma(code) + comment;
    }
    return result;
}

inline std::string edgeSemanticOf(const std::string& type, const std::string& stroke) {
    if (type == "arrow_point" && stroke == "normal") return "sequence";
    if (type == "arrow_cross" && stroke == "normal") return "failure";
    if (type == "arrow_open" && stroke == "dotted") return "reference";
    return "";
}

inline AgentLinkInfo destructEndLink(const std::string& raw) {
    std::string str = trimStr(raw);
    std::string line = str.empty() ? "" : str.substr(0, str.size() - 1);
    std::string type = "arrow_open";
    if (!str.empty()) {
        switch (str.back()) {
            case 'x':
                type = "arrow_cross";
                break;
            case '>':
                type = "arrow_point";
                break;
            case '-':
            case '.':
                line = str;
                break;
        }
    }
    std::string stroke = "normal";
    int length = static_cast<int>(line.size()) - 1;
    int dots = 0;
    for (char c : line) if (c == '.') dots++;
    if (dots > 0) {
        stroke = "dotted";
        length = dots;
    }
    AgentLinkInfo info;
    info.type = type;
    info.stroke = stroke;
    info.length = length;
    info.hasLength = true;
    return info;
}

class AgentflowDb : public DiagramMeta {
public:
    std::string direction;
    std::vector<std::string> nodeOrder;
    std::unordered_map<std::string, AgentNode> nodes;
    std::vector<AgentEdge> edges;
    std::vector<AgentSubgraph> subgraphs;
    std::vector<std::string> connectorOrder;
    std::unordered_map<std::string, AgentNode> connectors;
    std::unordered_map<std::string, AgentClassDef> classes;
    std::unordered_map<std::string, std::string> tooltips;
    std::vector<std::string> defaultEdgeStyle;
    bool hasDefaultEdgeStyle = false;
    std::string defaultInterpolate;
    std::vector<ElementMapping> mappings;
    std::vector<Diagnostic> diagnostics;
    int lineOffset = 0;

    std::unordered_map<std::string, size_t> subgraphLookup;
    std::unordered_set<std::string> globalNodes;
    std::unordered_set<std::string> members;
    std::unordered_map<std::string, size_t> edgeById;
    std::unordered_map<std::string, int> pairCount;
    int subCount = 0;
    std::unordered_map<std::string, size_t> firstMapping;
    std::unordered_map<std::string, std::vector<Diagnostic>> reported;
    AgentflowDbOptions options;

    explicit AgentflowDb(AgentflowDbOptions opts = {})
        : lineOffset(opts.lineOffset), options(std::move(opts)) {}

    std::pair<int, int> errorAt(const Loc* loc) const {
        if (loc == nullptr) return {0, 0};
        if (!options.hasSource) {
            return {loc->first_line + lineOffset, loc->first_column + 1};
        }
        const std::string& src = options.source;
        size_t start = 0;
        for (int line = 1; line < loc->first_line; line++) {
            size_t nl = src.find('\n', start);
            start = (nl == std::string::npos) ? src.size() : nl + 1;
        }
        size_t offset = start + static_cast<size_t>(std::max(0, loc->first_column));
        int line = 1;
        size_t lineStart = 0;
        for (size_t i = src.find('\n'); i != std::string::npos && i < offset; i = src.find('\n', i + 1)) {
            line++;
            lineStart = i + 1;
        }
        int column = static_cast<int>(offset >= lineStart ? offset - lineStart + 1 : 1);
        return {line, column};
    }

    PeleError shapeError(const std::string& message, const Loc* loc) const {
        auto [line, col] = errorAt(loc);
        return PeleError(message, "semantic", "agentflow", line, col);
    }

    YamlValue metadata(const std::string& id, const std::string& source, const Loc* loc) {
        bool multiline = source.find('\n') != std::string::npos;
        std::string stripped = multiline ? stripLineTrailingCommas(source) : source;
        YamlValue parsed = parseYaml(multiline ? (stripped + "\n") : ("{\n" + source + "\n}"));
        if (parsed.isNull()) parsed = YamlValue::makeMap();
        return stripPrototypeKeys(parsed);
    }

    void addVertex(
        const std::string& id,
        const FlowText* textObj = nullptr,
        const std::string* type = nullptr,
        const std::vector<std::string>* style = nullptr,
        const std::vector<std::string>* cls = nullptr,
        const std::string* dir = nullptr,
        const std::map<std::string, std::string>* props = nullptr,
        const std::string* metadataStr = nullptr,
        const Loc* metadataLoc = nullptr
    ) {
        if (id.empty() || trimStr(id).empty()) return;

        YamlValue doc = YamlValue::makeNull();
        bool hasDoc = false;
        bool hasFields = false;
        std::string authoredShape;
        bool hasAuthoredShape = false;
        if (metadataStr != nullptr) {
            doc = metadata(id, *metadataStr, metadataLoc);
            hasDoc = true;
            if (doc.isMap()) {
                hasFields = true;
                auto shIt = doc.mVal.find("shape");
                if (shIt != doc.mVal.end() && shIt->second.isString()) {
                    authoredShape = shIt->second.sVal;
                    hasAuthoredShape = true;
                    shIt->second.sVal = resolveShapeAlias(authoredShape);
                }
            }
        }

        auto sgIt = subgraphLookup.find(id);
        if (sgIt != subgraphLookup.end() && hasDoc) {
            auto& sg = subgraphs[sgIt->second];
            mergeMetadata(sg.metadata, sg.hasMetadata, doc);
            return;
        }

        auto connIt = connectors.find(id);
        if (connIt != connectors.end() && hasDoc) {
            mergeMetadata(connIt->second.metadata, connIt->second.hasMetadata, doc);
            auto nIt = nodes.find(id);
            if (nIt != nodes.end()) {
                mergeMetadata(nIt->second.metadata, nIt->second.hasMetadata, doc);
            }
            return;
        }

        if (id == "connectors") return;

        auto edgeIt = edgeById.find(id);
        if (edgeIt != edgeById.end()) {
            auto& edge = edges[edgeIt->second];
            if (hasFields) {
                if (const auto* v = doc.get("animate"); v != nullptr) {
                    edge.animate = *v;
                    edge.hasAnimate = true;
                }
                if (const auto* v = doc.get("animation"); v != nullptr) {
                    edge.animation = *v;
                    edge.hasAnimation = true;
                }
                if (const auto* v = doc.get("curve"); v != nullptr && !v->isNull()) {
                    edge.interpolate = v->asString();
                }
            }
            if (hasDoc) {
                mergeMetadata(edge.metadata, edge.hasMetadata, doc);
            }
            return;
        }

        auto vIt = nodes.find(id);
        if (vIt == nodes.end()) {
            AgentNode an;
            an.id = id;
            an.labelType = "text";
            nodeOrder.push_back(id);
            vIt = nodes.emplace(id, std::move(an)).first;
        }
        AgentNode& vertex = vIt->second;

        if (textObj != nullptr) {
            vertex.labelType = textObj->type;
            vertex.text = agentUnquote(trimStr(textObj->text));
            vertex.hasText = true;
        } else if (!vertex.hasText) {
            vertex.text = id;
            vertex.hasText = true;
        }
        if (type != nullptr) {
            vertex.type = *type;
            vertex.hasType = true;
        }
        if (style != nullptr) {
            for (const auto& s : *style) vertex.styles.push_back(s);
        }
        if (cls != nullptr) {
            for (const auto& c : *cls) vertex.classes.push_back(c);
        }
        if (dir != nullptr) {
            vertex.dir = *dir;
        }
        if (!vertex.hasProps) {
            vertex.hasProps = true;
            if (props != nullptr) vertex.props = *props;
        } else if (props != nullptr) {
            for (const auto& [k, v] : *props) vertex.props[k] = v;
        }

        if (!hasDoc) return;
        mergeMetadata(vertex.metadata, vertex.hasMetadata, doc);
        if (!hasFields) return;

        if (const auto* shVal = doc.get("shape"); shVal != nullptr && !shVal->isNull()) {
            bool truthy = shVal->isString() ? !shVal->sVal.empty() : (shVal->isBool() ? shVal->bVal : (shVal->isNumber() ? shVal->nVal != 0.0 : true));
            if (truthy) {
                if (!hasAuthoredShape && !shVal->isString()) {
                    throw shapeError("No such shape: " + shVal->asString() + ".", metadataLoc);
                }
                std::string authored = hasAuthoredShape ? authoredShape : shVal->sVal;
                if (authored != toLowerAscii(authored) || authored.find('_') != std::string::npos) {
                    throw shapeError("No such shape: " + authored + ". Shape names should be lowercase.", metadataLoc);
                }
                std::string shape = shVal->sVal;
                if (!isValidShape(shape)) {
                    throw shapeError("No such shape: " + shape + ".", metadataLoc);
                }
                vertex.type = shape;
                vertex.hasType = true;
            }
        }
        if (const auto* lblVal = doc.get("label"); lblVal != nullptr && !lblVal->isNull()) {
            std::string s = lblVal->asString();
            bool truthy = !s.empty() && (!lblVal->isBool() || lblVal->bVal) && (!lblVal->isNumber() || lblVal->nVal != 0.0);
            if (truthy) {
                vertex.text = s;
                vertex.hasText = true;
                const auto* lt = doc.get("labelType");
                vertex.labelType = agentLabelType(lt ? lt->asString() : "");
            }
        }
    }

    void addSingleLink(const std::string& start, const std::string& end, const AgentLinkInfo* type, const std::string& id = "") {
        AgentEdge edge;
        edge.start = start;
        edge.end = end;
        edge.labelType = "text";
        edge.isUserDefinedId = false;
        edge.interpolate = defaultInterpolate;
        if (type != nullptr && type->hasText) {
            edge.text = agentUnquote(trimStr(type->text.text));
            edge.labelType = agentLabelType(type->text.type);
        }
        if (type != nullptr) {
            edge.type = type->type;
            edge.hasType = true;
            edge.stroke = type->stroke;
            if (type->hasLength) {
                edge.length = type->length > 10 ? 10 : type->length;
                edge.hasLength = true;
            }
            if (!type->edgeSemantic.empty()) edge.edgeSemantic = type->edgeSemantic;
        }
        std::string pair = start + std::string(1, '\0') + end;
        int existing = pairCount[pair];
        if (!id.empty() && !edgeById.count(id)) {
            edge.id = id;
            edge.isUserDefinedId = true;
        } else {
            edge.id = "L_" + start + "_" + end + "_" + std::to_string(existing == 0 ? 0 : existing + 1);
        }

        if (options.hasMaxEdges && static_cast<int>(edges.size()) >= options.maxEdges) {
            throw PeleError("Edge limit exceeded. The limit is " + std::to_string(options.maxEdges) + ".", "limit", "agentflow");
        }
        size_t idx = edges.size();
        std::string eid = edge.id;
        edges.push_back(std::move(edge));
        pairCount[pair] = existing + 1;
        if (!edgeById.count(eid)) edgeById[eid] = idx;
    }

    void addLink(const std::vector<std::string>& starts, const std::vector<std::string>& ends, const AgentLinkInfo* linkData) {
        std::string id;
        if (linkData != nullptr && linkData->hasId) {
            id = replaceAllStr(linkData->id, "@", "");
        }
        const std::string& lastStart = starts.back();
        const std::string& firstEnd = ends.front();
        for (const auto& start : starts) {
            for (const auto& end : ends) {
                addSingleLink(start, end, linkData, (start == lastStart && end == firstEnd) ? id : "");
            }
        }
    }

    void updateLinkInterpolate(const std::vector<std::string>& positions, const std::string& interpolate) {
        for (const auto& pos : positions) {
            if (pos == "default") defaultInterpolate = interpolate;
            else edgeAt(pos).interpolate = interpolate;
        }
    }

    void updateLink(const std::vector<std::string>& positions, std::vector<std::string> style) {
        bool hasFill = false;
        for (const auto& s : style) if (startsWith(s, "fill")) hasFill = true;
        if (!style.empty() && !hasFill) style.push_back("fill:none");
        for (const auto& pos : positions) {
            if (pos == "default") {
                defaultEdgeStyle = style;
                hasDefaultEdgeStyle = true;
                continue;
            }
            auto& edge = edgeAt(pos);
            edge.style = style;
            edge.hasStyle = true;
        }
    }

    AgentEdge& edgeAt(const std::string& pos) {
        long long idx = -1;
        try { idx = std::stoll(pos); } catch (...) {}
        if (idx < 0 || idx >= static_cast<long long>(edges.size())) {
            throw PeleError(
                "The index " + pos + " for linkStyle is out of bounds. Valid indices for linkStyle are between 0 and " +
                std::to_string(static_cast<long long>(edges.size()) - 1) +
                ". (Help: Ensure that the index is within the range of existing edges.)",
                "semantic",
                "agentflow"
            );
        }
        return edges[static_cast<size_t>(idx)];
    }

    void addClass(const std::string& ids, const std::vector<std::string>& styles) {
        std::string joined;
        for (size_t i = 0; i < styles.size(); i++) {
            if (i > 0) joined.push_back(',');
            joined += styles[i];
        }
        joined = replaceAllStr(joined, "\\,", "\x01");
        joined = replaceAllStr(joined, ",", ";");
        joined = replaceAllStr(joined, "\x01", ",");
        auto styleList = splitStr(joined, ';');
        for (const auto& id : splitStr(ids, ',')) {
            auto& classNode = classes[id];
            classNode.id = id;
            for (const auto& s : styleList) {
                if (s.find("color") != std::string::npos) {
                    classNode.textStyles.push_back(replaceAllStr(s, "fill", "bgFill"));
                }
                classNode.styles.push_back(s);
            }
        }
    }

    void setDirection(const std::string& dir) {
        std::string d = trimStr(dir);
        if (d.find('<') != std::string::npos) d = "RL";
        if (d.find('^') != std::string::npos) d = "BT";
        if (d.find('>') != std::string::npos) d = "LR";
        if (d.find('v') != std::string::npos) d = "TB";
        if (d == "TD") d = "TB";
        direction = d;
    }

    void setClass(const std::string& ids, const std::string& className) {
        for (const auto& id : splitStr(ids, ',')) {
            auto nIt = nodes.find(id);
            if (nIt != nodes.end()) nIt->second.classes.push_back(className);
            auto eIt = edgeById.find(id);
            if (eIt != edgeById.end()) edges[eIt->second].classes.push_back(className);
            auto sIt = subgraphLookup.find(id);
            if (sIt != subgraphLookup.end()) subgraphs[sIt->second].classes.push_back(className);
        }
    }

    void setTooltip(const std::string& ids, const std::string* tooltip) {
        if (tooltip == nullptr) return;
        for (const auto& id : splitStr(ids, ',')) tooltips[id] = *tooltip;
    }

    void setLink(const std::string& ids, const std::string& link, const std::string& target = "") {
        std::string url = trimStr(link);
        for (const auto& id : splitStr(ids, ',')) {
            auto nIt = nodes.find(id);
            if (nIt != nodes.end()) {
                nIt->second.link = !url.empty() ? Flowchart::flowSafeUrl(url) : "";
                nIt->second.linkTarget = target;
            }
        }
        setClass(ids, "clickable");
    }

    void setClickEvent(const std::string& ids, const std::string& = "", const std::string& = "") {
        setClass(ids, "clickable");
    }

    void setAccTitle(const std::string& text) {
        accTitle = trimStartStr(text);
    }

    void setAccDescription(const std::string& text) {
        std::string out;
        size_t i = 0;
        while (i < text.size()) {
            out.push_back(text[i]);
            if (text[i] == '\n') {
                i++;
                while (i < text.size() && isSpace(static_cast<unsigned char>(text[i])) && text[i] != '\n') i++;
            } else {
                i++;
            }
        }
        accDescr = out;
    }

    std::string addSubGraph(const FlowText* idText, const std::vector<DocItem>& list, const FlowText* titleText) {
        std::string id = idText ? trimStr(idText->text) : "";
        std::string titleStr = titleText ? titleText->text : "";
        if (idText != nullptr && idText == titleText) {
            bool hasSpace = false;
            size_t p = 0;
            while (p < titleStr.size()) {
                int cp = charCodeAtUtf8(titleStr, p, &p);
                if (isSpace(cp)) { hasSpace = true; break; }
            }
            if (hasSpace) id.clear();
        }

        std::unordered_set<std::string> seen;
        std::vector<std::string> nodeList;
        std::string dir;
        for (const auto& entry : list) {
            if (entry.kind == DocItem::Str) {
                if (!trimStr(entry.strVal).empty() && !seen.count(entry.strVal)) {
                    seen.insert(entry.strVal);
                    nodeList.push_back(entry.strVal);
                }
            } else if (entry.kind == DocItem::Arr) {
                for (const auto& item : entry.arrVal) {
                    if (!trimStr(item).empty() && !seen.count(item)) {
                        seen.insert(item);
                        nodeList.push_back(item);
                    }
                }
            } else {
                dir = entry.dirVal;
            }
        }
        if (dir.empty() && options.inheritDir) {
            dir = !direction.empty() ? direction : options.direction;
        }
        if (id.empty()) id = "subGraph" + std::to_string(subCount);
        subCount++;

        std::vector<std::string> filtered;
        for (const auto& nodeId : nodeList) {
            if (nodeId != id && !globalNodes.count(nodeId) && !members.count(nodeId)) {
                filtered.push_back(nodeId);
            }
        }
        for (const auto& nodeId : filtered) members.insert(nodeId);

        auto existingIt = subgraphLookup.find(id);
        if (existingIt != subgraphLookup.end()) {
            auto& existing = subgraphs[existingIt->second];
            for (const auto& nodeId : filtered) existing.nodes.push_back(nodeId);
            if (!trimStr(titleStr).empty()) existing.title = trimStr(titleStr);
            if (!dir.empty()) existing.dir = dir;
            return id;
        }

        AgentSubgraph sg;
        sg.id = id;
        sg.nodes = std::move(filtered);
        sg.title = trimStr(titleStr);
        sg.dir = dir;
        sg.labelType = agentLabelType(titleText ? titleText->type : "");
        sg.type = "flow";
        size_t idx = subgraphs.size();
        subgraphs.push_back(std::move(sg));
        subgraphLookup[id] = idx;
        return id;
    }

    void addGlobal(const std::vector<DocItem>& list) {
        bool release = false;
        auto add = [&](const std::string& item) {
            std::string id = trimStr(item);
            if (id.empty()) return;
            globalNodes.insert(id);
            if (members.erase(id) > 0) release = true;
        };
        for (const auto& entry : list) {
            if (entry.kind == DocItem::Str) add(entry.strVal);
            else if (entry.kind == DocItem::Arr) {
                for (const auto& item : entry.arrVal) add(item);
            }
        }
        if (!release) return;
        for (auto& sg : subgraphs) {
            std::vector<std::string> kept;
            for (const auto& nodeId : sg.nodes) {
                if (!globalNodes.count(nodeId)) kept.push_back(nodeId);
            }
            sg.nodes = std::move(kept);
        }
    }

    std::string addConnector(const FlowText* textObj, const FlowText* titleObj = nullptr) {
        std::string id = textObj ? trimStr(textObj->text) : "";
        if (id.empty()) return "";
        std::string titleStr = titleObj ? trimStr(titleObj->text) : "";
        auto it = connectors.find(id);
        if (it == connectors.end()) {
            AgentNode created;
            created.id = id;
            created.labelType = "text";
            created.isConnector = true;
            created.text = !titleStr.empty() ? titleStr : id;
            created.hasText = true;
            connectorOrder.push_back(id);
            connectors[id] = created;
            if (!nodes.count(id)) nodeOrder.push_back(id);
            nodes[id] = std::move(created);
        } else if (!titleStr.empty()) {
            it->second.text = titleStr;
            it->second.hasText = true;
            auto nIt = nodes.find(id);
            if (nIt != nodes.end()) {
                nIt->second.text = titleStr;
                nIt->second.hasText = true;
            }
        }
        return id;
    }

    AgentLinkInfo destructLink(const std::string& end, const std::string& start = "") const {
        AgentLinkInfo info = destructEndLink(end);
        if (!start.empty()) {
            std::string stroke = (start.find('.') != std::string::npos) ? "dotted" : "normal";
            if (stroke != info.stroke) {
                AgentLinkInfo inv;
                inv.type = "INVALID";
                inv.stroke = "INVALID";
                return inv;
            }
        }
        info.edgeSemantic = edgeSemanticOf(info.type, info.stroke);
        return info;
    }

    const AgentSubgraph* subgraph(const std::string& id) const {
        auto it = subgraphLookup.find(id);
        return it != subgraphLookup.end() ? &subgraphs[it->second] : nullptr;
    }

    std::string vertexKind(const AgentNode& vertex) const {
        if (isToolDefinition(vertex)) return "tool";
        std::string shape = resolveShapeAlias(vertex.type);
        if (shape == "hexagon" || shape == "hex") return "action";
        if (shape == "lean-right" || shape == "lean_right") return "input";
        if (shape == "lin-doc" || shape == "lined-document") return "refdoc";
        if (shape == "diamond") return "decision";
        return "task";
    }

    ElementPosition position(const Loc* loc) const {
        int startLine = loc ? loc->first_line : 0;
        int startColumn = loc ? loc->first_column : 0;
        ElementPosition p;
        p.startLine = startLine + lineOffset;
        p.startColumn = startColumn;
        p.endLine = (loc ? loc->last_line : startLine) + lineOffset;
        p.endColumn = loc ? loc->last_column : startColumn;
        p.startIndex = 0;
        p.endIndex = 0;
        return p;
    }

    size_t pushMapping(const std::string& id, const std::string& type, const ElementPosition& pos, bool isBare = false) {
        size_t idx = mappings.size();
        mappings.push_back({id, type, pos, isBare});
        if (!firstMapping.count(id)) firstMapping[id] = idx;
        return idx;
    }

    void addVertexMapping(const std::string& id, bool hasShape, const Loc* loc) {
        if (id.empty()) return;
        pushMapping(id, "vertex", position(loc), !hasShape);
    }

    void extendVertexMapping(const std::string& id, const Loc* loc) {
        if (id.empty() || loc == nullptr) return;
        ElementPosition end = position(loc);
        for (int i = static_cast<int>(mappings.size()) - 1; i >= 0; i--) {
            auto& m = mappings[static_cast<size_t>(i)];
            if (m.type != "vertex" || m.id != id) continue;
            if (m.isBare) {
                m.isBare = false;
                m.type = "attachment";
            }
            auto& p = m.position;
            if (end.endLine > p.endLine || (end.endLine == p.endLine && end.endColumn > p.endColumn)) {
                p.endLine = end.endLine;
                p.endColumn = end.endColumn;
            }
            return;
        }
        pushMapping(id, "attachment", end);
    }

    void addEdgeMapping(const std::vector<std::string>& toNodes, const Loc* loc) {
        std::string joined;
        for (const auto& n : toNodes) {
            if (n.empty()) continue;
            if (!joined.empty()) joined.push_back('>');
            joined += n;
        }
        pushMapping(!joined.empty() ? joined : "edge", "edge", position(loc));
    }

    void blockMapping(const std::string& id, const std::string& type, const Loc* startLoc, const Loc* endLoc) {
        if (id.empty()) return;
        ElementPosition start = position(startLoc);
        ElementPosition end = endLoc ? position(endLoc) : start;
        pushMapping(id, type, {start.startLine, start.startColumn, end.endLine, end.endColumn, 0, 0});
    }

    void addSubgraphMapping(const FlowText* id, const Loc* startLoc, const Loc* endLoc) {
        blockMapping(id ? id->text : "", "subgraph", startLoc, endLoc);
    }

    void addConnectorMapping(const FlowText* textObj, const Loc* startLoc, const Loc* endLoc) {
        blockMapping(textObj ? trimStr(textObj->text) : "", "connector", startLoc, endLoc);
    }

    void emit(const std::string& id, const std::string& severity, const std::string& message, const std::string& nodeId = "", const std::string& edgeId = "") {
        std::string anchor = !nodeId.empty() ? nodeId : edgeId;
        Diagnostic diag;
        diag.id = id;
        diag.severity = severity;
        diag.message = message;
        if (!nodeId.empty()) diag.nodeId = nodeId;
        else if (!edgeId.empty()) diag.edgeId = edgeId;
        if (!anchor.empty()) {
            auto fmIt = firstMapping.find(anchor);
            if (fmIt != firstMapping.end()) {
                diag.position = mappings[fmIt->second].position;
                diag.hasPosition = true;
            }
        }
        auto& same = reported[anchor];
        for (const auto& d : same) {
            if (d.id == id && d.severity == severity && d.message == message && d.nodeId == diag.nodeId && d.edgeId == diag.edgeId) {
                return;
            }
        }
        same.push_back(diag);
        diagnostics.push_back(std::move(diag));
    }

    void emitWarning(const std::string& id, const std::string& message, const std::string& nodeId = "", const std::string& edgeId = "") {
        emit(id, "warning", message, nodeId, edgeId);
    }

    void emitError(const std::string& id, const std::string& message, const std::string& nodeId = "", const std::string& edgeId = "") {
        emit(id, "error", message, nodeId, edgeId);
    }
};

// ============================================================================
// Port of src/diagrams/agentflow/tokens.ts
// ============================================================================

namespace Tok {
enum T : int {
    END = 0,
    ERROR,
    acc_title,
    acc_title_value,
    acc_descr,
    acc_descr_value,
    acc_descr_multiline_value,
    SHAPE_DATA,
    CALLBACKNAME,
    CALLBACKARGS,
    MD_STR,
    STR,
    STYLE,
    DEFAULT,
    LINKSTYLE,
    INTERPOLATE,
    CLASSDEF,
    CLASS,
    HREF,
    CLICK,
    GRAPH,
    flow,
    connector,
    global,
    end,
    LINK_TARGET,
    NODIR,
    DIR,
    direction_tb,
    direction_bt,
    direction_rl,
    direction_lr,
    direction_td,
    LINK_ID,
    NUM,
    BRKT,
    STYLE_SEPARATOR,
    COLON,
    AMP,
    SEMI,
    COMMA,
    MULT,
    LINK,
    START_LINK,
    EDGE_TEXT,
    ELLIPSE_END,
    TEXT,
    ELLIPSE_START,
    STADIUMEND,
    STADIUMSTART,
    SUBROUTINEEND,
    SUBROUTINESTART,
    VERTEX_WITH_PROPS_START,
    TAGEND,
    CYLINDEREND,
    CYLINDERSTART,
    DOUBLECIRCLEEND,
    DOUBLECIRCLESTART,
    TRAPEND,
    INVTRAPEND,
    TRAPSTART,
    INVTRAPSTART,
    TAGSTART,
    UP,
    SEP,
    DOWN,
    COMMENT,
    NODE_STRING,
    MINUS,
    UNICODE_TEXT,
    PIPE,
    PE,
    PS,
    SQE,
    SQS,
    DIAMOND_STOP,
    DIAMOND_START,
    NEWLINE,
    SPACE,
    EOF_TOK
};

inline const char* tokenName(int t) {
    static const char* NAMES[] = {
        "$end",
        "INVALID",
        "acc_title",
        "acc_title_value",
        "acc_descr",
        "acc_descr_value",
        "acc_descr_multiline_value",
        "SHAPE_DATA",
        "CALLBACKNAME",
        "CALLBACKARGS",
        "MD_STR",
        "STR",
        "STYLE",
        "DEFAULT",
        "LINKSTYLE",
        "INTERPOLATE",
        "CLASSDEF",
        "CLASS",
        "HREF",
        "CLICK",
        "GRAPH",
        "flow",
        "connector",
        "global",
        "end",
        "LINK_TARGET",
        "NODIR",
        "DIR",
        "direction_tb",
        "direction_bt",
        "direction_rl",
        "direction_lr",
        "direction_td",
        "LINK_ID",
        "NUM",
        "BRKT",
        "STYLE_SEPARATOR",
        "COLON",
        "AMP",
        "SEMI",
        "COMMA",
        "MULT",
        "LINK",
        "START_LINK",
        "EDGE_TEXT",
        "-)",
        "TEXT",
        "(-",
        "STADIUMEND",
        "STADIUMSTART",
        "SUBROUTINEEND",
        "SUBROUTINESTART",
        "VERTEX_WITH_PROPS_START",
        "TAGEND",
        "CYLINDEREND",
        "CYLINDERSTART",
        "DOUBLECIRCLEEND",
        "DOUBLECIRCLESTART",
        "TRAPEND",
        "INVTRAPEND",
        "TRAPSTART",
        "INVTRAPSTART",
        "TAGSTART",
        "UP",
        "SEP",
        "DOWN",
        "COMMENT",
        "NODE_STRING",
        "MINUS",
        "UNICODE_TEXT",
        "PIPE",
        "PE",
        "PS",
        "SQE",
        "SQS",
        "DIAMOND_STOP",
        "DIAMOND_START",
        "NEWLINE",
        "SPACE",
        "EOF"
    };
    return (t >= 0 && t <= EOF_TOK) ? NAMES[t] : "INVALID";
}
} // namespace Tok
// ============================================================================
// Port of src/diagrams/agentflow/lexer.ts
// ============================================================================

struct AgentTokens {
    std::vector<int> types;
    std::vector<std::string> texts;
    std::vector<size_t> starts;
    std::vector<int> locs; // 4 ints per token: first_line, first_col, last_line, last_col
};

namespace detail {

enum LexState : int {
    S_INITIAL = 0,
    S_string,
    S_md_string,
    S_acc_title,
    S_acc_descr,
    S_acc_descr_multiline,
    S_dir,
    S_text,
    S_ellipseText,
    S_trapText,
    S_edgeText,
    S_click,
    S_callbackname,
    S_callbackargs,
    S_shapeData,
    S_shapeDataStr
};

} // namespace detail

inline AgentTokens tokenize(const std::string& src) {
    using namespace Tok;
    using namespace detail;
    const auto& nodeChar = Flowchart::detail::nodeCharTable();
    size_t n = src.size();
    std::vector<int> types;
    std::vector<std::string> texts;
    std::vector<size_t> starts;
    std::vector<int> locs;
    std::vector<int> stack;
    int state = S_INITIAL;
    size_t p = 0;
    int line = 1;
    int col = 0;
    bool firstGraph = true;
    size_t dirNext = src.find("direction");
    size_t atNext = src.find('@');
    size_t lineEnd = 0;
    bool hasLineEnd = false;
    size_t dirFailed = 0;
    size_t runEnd = 0;
    long long runAt = -1;
    long long wsFrom = -1;
    size_t wsTo = 0;
    long long blankFrom = -1;
    size_t blankTo = 0;
    long long noLink = -1;
    long long noArrow = -1;

    auto charAt = [&](size_t idx) -> int {
        return idx < n ? static_cast<unsigned char>(src[idx]) : -1;
    };

    auto advance = [&](size_t e) {
        long long brk = -1;
        for (size_t k = p; k < e; k++) {
            unsigned char ch = static_cast<unsigned char>(src[k]);
            if (ch == 10) {
                line++;
                brk = static_cast<long long>(k);
            } else if (ch == 13) {
                if (k + 1 < e && static_cast<unsigned char>(src[k + 1]) == 10) k++;
                line++;
                brk = static_cast<long long>(k);
            }
        }
        if (brk < 0) {
            col += static_cast<int>(e - p);
        } else {
            size_t k = static_cast<size_t>(brk + 1);
            while (k < e) {
                size_t np = k;
                int cp = Flowchart::detail::codepointAt(src, k, np);
                if (cp == 0x2028 || cp == 0x2029) break;
                k = np;
            }
            col = static_cast<int>(k - static_cast<size_t>(brk) - 1);
        }
        p = e;
    };

    auto emit = [&](int type, size_t e) {
        types.push_back(type);
        texts.push_back(src.substr(p, e - p));
        starts.push_back(p);
        locs.push_back(line);
        locs.push_back(col);
        advance(e);
        locs.push_back(line);
        locs.push_back(col);
    };

    auto emitFlat = [&](int type, size_t e) {
        types.push_back(type);
        texts.push_back(src.substr(p, e - p));
        starts.push_back(p);
        locs.push_back(line);
        locs.push_back(col);
        col += static_cast<int>(e - p);
        p = e;
        locs.push_back(line);
        locs.push_back(col);
    };

    auto emitChars = [&](int type, size_t e) {
        types.push_back(type);
        texts.push_back(src.substr(p, e - p));
        starts.push_back(p);
        locs.push_back(line);
        locs.push_back(col);
        for (size_t k = p; k < e; k++) {
            unsigned char ch = static_cast<unsigned char>(src[k]);
            if (ch == 10 || ch == 13) {
                line++;
                col = 0;
            } else {
                col++;
            }
        }
        p = e;
        locs.push_back(line);
        locs.push_back(col);
    };

    auto emitText = [&](int type, std::string text, size_t e) {
        types.push_back(type);
        texts.push_back(std::move(text));
        starts.push_back(p);
        locs.push_back(line);
        locs.push_back(col);
        advance(e);
        locs.push_back(line);
        locs.push_back(col);
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

    auto kw = [&](const char* word, size_t len) -> bool {
        if (!startsWith(src, word, p)) return false;
        int after = charAt(p + len);
        return after < 0 || !isWord(after);
    };

    auto wsEnd = [&](size_t q) -> size_t {
        if (wsFrom >= 0 && q >= static_cast<size_t>(wsFrom) && q <= wsTo) return wsTo;
        size_t j = Flowchart::detail::skipSpaces(src, q);
        wsFrom = static_cast<long long>(q);
        wsTo = j;
        return j;
    };

    auto blankEnd = [&](size_t q) -> size_t {
        if (blankFrom >= 0 && q >= static_cast<size_t>(blankFrom) && q <= blankTo) return blankTo;
        size_t j = q;
        while (j < n) {
            unsigned char ch = static_cast<unsigned char>(src[j]);
            if (ch != 32 && ch != 9) break;
            j++;
        }
        blankFrom = static_cast<long long>(q);
        blankTo = j;
        return j;
    };

    auto arrowEnd = [&](size_t q) -> int {
        size_t j = wsEnd(q);
        if (static_cast<long long>(j) == noArrow || charAt(j) != 45 || charAt(j + 1) != 45) return -1;
        size_t k = j + 2;
        while (charAt(k) == 45) k++;
        int t = charAt(k);
        if (t == 120 || t == 62) return static_cast<int>(wsEnd(k + 1));
        noArrow = static_cast<long long>(j);
        return -1;
    };

    auto link = [&]() -> bool {
        size_t j = wsEnd(p);
        if (static_cast<long long>(j) == noLink || charAt(j) != 45) return false;
        int d = charAt(j + 1);
        size_t k = j + 2;
        if (d == 45) {
            while (charAt(k) == 45) k++;
            int t = charAt(k);
            if (t == 120 || t == 62) {
                emit(LINK, wsEnd(k + 1));
            } else {
                push(S_edgeText);
                emit(START_LINK, wsEnd(j + 2));
            }
            return true;
        }
        if (d == 46) {
            while (charAt(k) == 46) k++;
            if (charAt(k) == 45) {
                emit(LINK, wsEnd(k + 1));
                return true;
            }
        }
        noLink = static_cast<long long>(j);
        return false;
    };

    auto commentEnd = [&](size_t q) -> int {
        if (charAt(q) != 37 || charAt(q + 1) != 37 || charAt(q + 2) == 123) return -1;
        size_t e = src.find('\n', q + 2);
        return static_cast<int>(e == std::string::npos ? n : e);
    };

    auto quote = [&]() {
        if (charAt(p + 1) == 96) {
            push(S_md_string);
            advance(p + 2);
        } else {
            push(S_string);
            advance(p + 1);
        }
    };

    auto opener = [&](int c) -> bool {
        int d = charAt(p + 1);
        if (c == 40) {
            if (d == 45) {
                push(S_ellipseText);
                emitFlat(ELLIPSE_START, p + 2);
            } else if (d == 91) {
                push(S_text);
                emitFlat(STADIUMSTART, p + 2);
            } else if (d == 40 && charAt(p + 2) == 40) {
                push(S_text);
                emitFlat(DOUBLECIRCLESTART, p + 3);
            } else {
                push(S_text);
                emitFlat(PS, p + 1);
            }
            return true;
        }
        if (c == 91) {
            if (d == 91) {
                push(S_text);
                emitFlat(SUBROUTINESTART, p + 2);
            } else if (d == 40) {
                push(S_text);
                emitFlat(CYLINDERSTART, p + 2);
            } else if (d == 47) {
                push(S_trapText);
                emitFlat(TRAPSTART, p + 2);
            } else if (d == 92) {
                push(S_trapText);
                emitFlat(INVTRAPSTART, p + 2);
            } else {
                push(S_text);
                emitFlat(SQS, p + 1);
            }
            return true;
        }
        if (c == 124) {
            push(S_text);
            emitFlat(PIPE, p + 1);
            return true;
        }
        if (c == 123) {
            push(S_text);
            emitFlat(DIAMOND_START, p + 1);
            return true;
        }
        return false;
    };

    auto nodeString = [&]() -> bool {
        size_t q = p;
        while (q < n) {
            int ch = static_cast<unsigned char>(src[q]);
            if (ch < 128 && nodeChar[ch] == 1) {
                q++;
            } else if (ch == 45 && q + 1 < n) {
                int d = static_cast<unsigned char>(src[q + 1]);
                if (d == 62 || d == 45 || d == 46) break;
                q++;
            } else {
                break;
            }
        }
        if (q == p) return false;
        emitFlat(NODE_STRING, q);
        return true;
    };

    auto blanks = [&](size_t from) -> size_t {
        size_t e = from;
        while (e < n) {
            int ch = static_cast<unsigned char>(src[e]);
            if (ch != 32 && ch != 9) break;
            e++;
        }
        return e;
    };

    auto initial = [&](int c) -> bool {
        int e = -1;
        switch (c) {
            case 97:
                if (startsWith(src, "acc", p)) {
                    if ((e = Flowchart::detail::matchAccHeader(src, p, "accTitle", ':')) >= 0) {
                        push(S_acc_title);
                        emit(acc_title, static_cast<size_t>(e));
                        return true;
                    }
                    if ((e = Flowchart::detail::matchAccHeader(src, p, "accDescr", ':')) >= 0) {
                        push(S_acc_descr);
                        emit(acc_descr, static_cast<size_t>(e));
                        return true;
                    }
                    if ((e = Flowchart::detail::matchAccHeader(src, p, "accDescr", '{')) >= 0) {
                        push(S_acc_descr_multiline);
                        advance(static_cast<size_t>(e));
                        return true;
                    }
                }
                if (kw("agentflow-beta", 14)) {
                    if (firstGraph) {
                        firstGraph = false;
                        push(S_dir);
                    }
                    emitFlat(GRAPH, p + 14);
                    return true;
                }
                break;
            case 64:
                if (charAt(p + 1) == 123) {
                    push(S_shapeData);
                    emitText(SHAPE_DATA, "", p + 2);
                    return true;
                }
                break;
            case 99:
                if ((e = Flowchart::detail::matchWordSpaces(src, p, "call")) >= 0) {
                    push(S_callbackname);
                    advance(static_cast<size_t>(e));
                    return true;
                }
                if (kw("classDef", 8)) {
                    emitFlat(CLASSDEF, p + 8);
                    return true;
                }
                if (kw("class", 5)) {
                    emitFlat(CLASS, p + 5);
                    return true;
                }
                if ((e = Flowchart::detail::matchWordSpaces(src, p, "click")) >= 0) {
                    push(S_click);
                    advance(static_cast<size_t>(e));
                    return true;
                }
                if (kw("connector", 9)) {
                    emitFlat(connector, p + 9);
                    return true;
                }
                break;
            case 34:
                quote();
                return true;
            case 115:
                if (kw("style", 5)) {
                    emitFlat(STYLE, p + 5);
                    return true;
                }
                break;
            case 100:
                if (kw("default", 7)) {
                    emitFlat(DEFAULT, p + 7);
                    return true;
                }
                break;
            case 108:
                if (kw("linkStyle", 9)) {
                    emitFlat(LINKSTYLE, p + 9);
                    return true;
                }
                break;
            case 105:
                if (kw("interpolate", 11)) {
                    emitFlat(INTERPOLATE, p + 11);
                    return true;
                }
                break;
            case 104:
                if (startsWith(src, "href", p) && Flowchart::detail::isSpaceAt(src, p + 4)) {
                    size_t np = p + 4;
                    Flowchart::detail::codepointAt(src, p + 4, np);
                    emit(HREF, np);
                    return true;
                }
                break;
            case 102:
                if (kw("flow", 4)) {
                    emitFlat(flow, p + 4);
                    return true;
                }
                break;
            case 103:
                if (kw("global", 6)) {
                    emitFlat(global, blanks(p + 6));
                    return true;
                }
                break;
            case 101:
                if (kw("end", 3)) {
                    emitFlat(end, blanks(p + 3));
                    return true;
                }
                break;
            case 95:
                if (kw("_self", 5)) {
                    emitFlat(LINK_TARGET, p + 5);
                    return true;
                }
                if (kw("_blank", 6)) {
                    emitFlat(LINK_TARGET, p + 6);
                    return true;
                }
                if (kw("_parent", 7)) {
                    emitFlat(LINK_TARGET, p + 7);
                    return true;
                }
                if (kw("_top", 4)) {
                    emitFlat(LINK_TARGET, p + 4);
                    return true;
                }
                break;
        }

        if (dirNext != std::string::npos && p >= dirFailed) {
            if (dirNext < p) dirNext = src.find("direction", p);
            if (dirNext != std::string::npos) {
                if (!hasLineEnd || lineEnd < p) {
                    size_t le = p;
                    while (le < n) {
                        size_t np = le;
                        int cp = Flowchart::detail::codepointAt(src, le, np);
                        if (isLineEnd(cp)) break;
                        le = np;
                    }
                    lineEnd = le;
                    hasLineEnd = true;
                }
                if (dirNext < lineEnd) {
                    static const char* DIRS[5] = {"TB", "BT", "RL", "LR", "TD"};
                    for (int i = 0; i < 5; i++) {
                        if ((e = Flowchart::detail::matchDirectionStmt(src, p, lineEnd, DIRS[i])) >= 0) {
                            emit(direction_tb + i, static_cast<size_t>(e));
                            return true;
                        }
                    }
                    dirFailed = lineEnd;
                }
            }
        }

        if (atNext != std::string::npos && c != 34 && !Flowchart::detail::isSpaceAt(src, p)) {
            if (p >= runEnd) {
                if (atNext < p) atNext = src.find('@', p);
                size_t q = p;
                while (q < n) {
                    size_t np = q;
                    int cp = Flowchart::detail::codepointAt(src, q, np);
                    if (cp == 34 || isSpace(cp)) break;
                    q = np;
                }
                runEnd = q;
                runAt = -1;
                if (atNext != std::string::npos && atNext < q) {
                    size_t startK = (q < n ? q : n - 1);
                    if (startK > 0) {
                        for (size_t k = startK - 1; ; k--) {
                            if (charAt(k) == 64) {
                                int after = charAt(k + 1);
                                if (after != 123 && after != 34) {
                                    runAt = static_cast<long long>(k);
                                    break;
                                }
                            }
                            if (k <= atNext) break;
                        }
                    }
                }
            }
            if (runAt > static_cast<long long>(p)) {
                emitFlat(LINK_ID, static_cast<size_t>(runAt) + 1);
                return true;
            }
        }

        if (c >= 48 && c <= 57) {
            size_t q = p + 1;
            while (q < n && isDigit(static_cast<unsigned char>(src[q]))) q++;
            emitFlat(NUM, q);
            return true;
        }

        switch (c) {
            case 35:
                emitFlat(BRKT, p + 1);
                return true;
            case 58:
                if (charAt(p + 1) == 58 && charAt(p + 2) == 58) emitFlat(STYLE_SEPARATOR, p + 3);
                else emitFlat(COLON, p + 1);
                return true;
            case 38:
                emitFlat(AMP, p + 1);
                return true;
            case 59:
                emitFlat(SEMI, p + 1);
                return true;
            case 44:
                emitFlat(COMMA, p + 1);
                return true;
            case 42:
                emitFlat(MULT, p + 1);
                return true;
            case 60:
                emitFlat(TAGSTART, p + 1);
                return true;
            case 45:
                if (link() || nodeString()) return true;
                emitFlat(MINUS, p + 1);
                return true;
            case 40:
                return opener(c);
            case 91:
                if (charAt(p + 1) == 124) {
                    emitFlat(VERTEX_WITH_PROPS_START, p + 2);
                    return true;
                }
                return opener(c);
            case 124:
            case 123:
                return opener(c);
            case 62:
                push(S_text);
                emitFlat(TAGEND, p + 1);
                return true;
            case 94:
                emitFlat(UP, p + 1);
                return true;
            case 92:
                if (charAt(p + 1) == 124) {
                    emitFlat(SEP, p + 2);
                    return true;
                }
                return nodeString();
            case 118:
                if (p + 1 >= n || !isWord(static_cast<unsigned char>(src[p + 1]))) {
                    emitFlat(DOWN, p + 1);
                    return true;
                }
                return nodeString();
            case 37:
                if ((e = commentEnd(p)) >= 0) {
                    emit(COMMENT, static_cast<size_t>(e));
                    return true;
                }
                return nodeString();
        }

        size_t nextPos = p;
        int cp = Flowchart::detail::codepointAt(src, p, nextPos);
        if (isSpace(cp)) {
            if (link()) return true;
            if ((c == 32 || c == 9) && (e = commentEnd(blankEnd(p))) >= 0) {
                emit(COMMENT, static_cast<size_t>(e));
                return true;
            }
            size_t nlEnd = p;
            while (nlEnd < n) {
                int ch = charAt(nlEnd);
                if (ch == 10) nlEnd++;
                else if (ch == 13 && charAt(nlEnd + 1) == 10) nlEnd += 2;
                else break;
            }
            if (nlEnd > p) emit(NEWLINE, nlEnd);
            else emit(SPACE, nextPos);
            return true;
        }

        if (c < 128) return nodeString();
        if (isUnicodeLetter(cp)) {
            types.push_back(UNICODE_TEXT);
            texts.push_back(src.substr(p, nextPos - p));
            starts.push_back(p);
            locs.push_back(line);
            locs.push_back(col);
            col += 1;
            p = nextPos;
            locs.push_back(line);
            locs.push_back(col);
            return true;
        }
        return false;
    };

    while (true) {
        if (p >= n) {
            int type = -1;
            switch (state) {
                case S_INITIAL:
                    type = EOF_TOK;
                    break;
                case S_acc_title:
                    type = acc_title_value;
                    break;
                case S_acc_descr:
                    type = acc_descr_value;
                    break;
                case S_acc_descr_multiline:
                    type = acc_descr_multiline_value;
                    break;
                case S_click:
                    type = CLICK;
                    break;
                case S_callbackname:
                    type = CALLBACKNAME;
                    break;
                case S_callbackargs:
                    type = CALLBACKARGS;
                    break;
            }
            if (type != -1) emitFlat(type, n);
            break;
        }

        int c = static_cast<unsigned char>(src[p]);
        bool ok = true;

        switch (state) {
            case S_INITIAL:
                ok = initial(c);
                break;

            case S_string:
                if (c == 34) {
                    if (charAt(p + 1) == 96) {
                        push(S_md_string);
                        advance(p + 2);
                    } else {
                        pop();
                        advance(p + 1);
                    }
                } else {
                    size_t e = src.find('"', p);
                    emit(STR, e == std::string::npos ? n : e);
                }
                break;

            case S_md_string:
                if (c == 96) {
                    if (charAt(p + 1) == 34) {
                        pop();
                        advance(p + 2);
                    } else {
                        ok = false;
                    }
                } else if (c == 34) {
                    quote();
                } else {
                    size_t q = p + 1;
                    while (q < n) {
                        unsigned char ch = static_cast<unsigned char>(src[q]);
                        if (ch == 96 || ch == 34) break;
                        q++;
                    }
                    emit(MD_STR, q);
                }
                break;

            case S_acc_title:
            case S_acc_descr: {
                size_t e = src.find('\n', p);
                if (e == std::string::npos) e = n;
                int type = (state == S_acc_title) ? acc_title_value : acc_descr_value;
                pop();
                emit(type, e);
                break;
            }

            case S_acc_descr_multiline:
                if (c == 125) {
                    pop();
                    advance(p + 1);
                } else {
                    size_t e = src.find('}', p);
                    if (e == std::string::npos) e = n;
                    emit(acc_descr_multiline_value, e);
                }
                break;

            case S_dir: {
                int e = -1;
                if (c == 34) {
                    quote();
                } else if ((e = Flowchart::detail::matchNoDir(src, p)) >= 0) {
                    pop();
                    emit(NODIR, static_cast<size_t>(e));
                } else if ((e = Flowchart::detail::matchDirWord(src, p)) >= 0 ||
                           (e = Flowchart::detail::matchDirSym(src, p)) >= 0 ||
                           (e = Flowchart::detail::matchDirV(src, p)) >= 0) {
                    pop();
                    emit(DIR, static_cast<size_t>(e));
                } else {
                    ok = opener(c);
                }
                break;
            }

            case S_text: {
                int d = charAt(p + 1);
                switch (c) {
                    case 34:
                        quote();
                        break;
                    case 40:
                    case 91:
                    case 123:
                        opener(c);
                        break;
                    case 93:
                        pop();
                        if (d == 41) emitFlat(STADIUMEND, p + 2);
                        else if (d == 93) emitFlat(SUBROUTINEEND, p + 2);
                        else emitFlat(SQE, p + 1);
                        break;
                    case 41:
                        pop();
                        if (d == 93) emitFlat(CYLINDEREND, p + 2);
                        else if (d == 41 && charAt(p + 2) == 41) emitFlat(DOUBLECIRCLEEND, p + 3);
                        else emitFlat(PE, p + 1);
                        break;
                    case 124:
                        pop();
                        emitFlat(PIPE, p + 1);
                        break;
                    case 125:
                        pop();
                        emitFlat(DIAMOND_STOP, p + 1);
                        break;
                    case 37: {
                        int e = commentEnd(p);
                        if (e >= 0) advance(static_cast<size_t>(e));
                        else ok = false;
                        break;
                    }
                    default: {
                        size_t q = p + 1;
                        while (q < n) {
                            unsigned char ch = static_cast<unsigned char>(src[q]);
                            if (ch == 91 || ch == 93 || ch == 40 || ch == 41 ||
                                ch == 123 || ch == 125 || ch == 124 || ch == 34 || ch == 37) {
                                break;
                            }
                            q++;
                        }
                        emit(TEXT, q);
                        break;
                    }
                }
                break;
            }

            case S_ellipseText: {
                int d = charAt(p + 1);
                int e = -1;
                if (c == 34) {
                    quote();
                } else if (c == 37 && (e = commentEnd(p)) >= 0) {
                    advance(static_cast<size_t>(e));
                } else if ((c == 45 || c == 47 || c == 41) && d == 41) {
                    pop();
                    emitFlat(ELLIPSE_END, p + 2);
                } else if (c == 40 || c == 91 || c == 123) {
                    opener(c);
                } else if (c == 41 || c == 93 || c == 125) {
                    ok = false;
                } else {
                    size_t q = p + 1;
                    while (q < n) {
                        unsigned char ch = static_cast<unsigned char>(src[q]);
                        if (ch == 34 || ch == 40 || ch == 41 || ch == 91 || ch == 93 || ch == 123 || ch == 125) break;
                        if ((ch == 45 || ch == 47) && charAt(q + 1) == 41) break;
                        if (ch == 37 && charAt(q + 1) == 37 && charAt(q + 2) != 123) break;
                        q++;
                    }
                    emitChars(TEXT, q);
                }
                break;
            }

            case S_trapText: {
                int d = charAt(p + 1);
                if (c == 34) {
                    quote();
                    break;
                }
                switch (c) {
                    case 40:
                        if (d == 45 || d == 91 || (d == 40 && charAt(p + 2) == 40)) {
                            opener(c);
                        } else if (d == 93) {
                            pop();
                            emitFlat(TRAPEND, p + 2);
                        } else {
                            opener(c);
                        }
                        break;
                    case 91:
                    case 123:
                        opener(c);
                        break;
                    case 92:
                        if (d == 93) {
                            pop();
                            emitFlat(TRAPEND, p + 2);
                        } else {
                            emitFlat(TEXT, p + 1);
                        }
                        break;
                    case 47:
                        if (d == 93) {
                            pop();
                            emitFlat(INVTRAPEND, p + 2);
                        } else {
                            emitFlat(TEXT, p + 1);
                        }
                        break;
                    case 93:
                    case 41:
                        if (d == 93) {
                            pop();
                            emitFlat(TRAPEND, p + 2);
                        } else {
                            ok = false;
                        }
                        break;
                    case 125:
                        ok = false;
                        break;
                    case 37: {
                        int e = commentEnd(p);
                        if (e >= 0) advance(static_cast<size_t>(e));
                        else ok = false;
                        break;
                    }
                    default: {
                        if ((c == 63 || c == 61) && d == 93) {
                            pop();
                            emitFlat(TRAPEND, p + 2);
                            break;
                        }
                        size_t q = p + 1;
                        while (q < n) {
                            unsigned char ch = static_cast<unsigned char>(src[q]);
                            if (ch == 92 || ch == 91 || ch == 93 || ch == 40 ||
                                ch == 41 || ch == 123 || ch == 125 || ch == 47 || ch == 37) {
                                break;
                            }
                            q++;
                        }
                        emit(TEXT, q);
                        break;
                    }
                }
                break;
            }

            case S_edgeText: {
                if (c == 34) {
                    quote();
                    break;
                }
                int e = arrowEnd(p);
                if (e >= 0) {
                    pop();
                    emit(LINK, static_cast<size_t>(e));
                    break;
                }
                if (c == 45 && charAt(p + 1) == 45) {
                    ok = false;
                    break;
                }
                size_t q = p + 1;
                while (q < n) {
                    int ch = static_cast<unsigned char>(src[q]);
                    if (ch == 34) break;
                    if (ch == 45) {
                        if (charAt(q + 1) == 45) break;
                        q++;
                    } else {
                        size_t np = q;
                        int cp = Flowchart::detail::codepointAt(src, q, np);
                        if (isSpace(cp) && arrowEnd(q) >= 0) break;
                        q = np;
                    }
                }
                emitChars(EDGE_TEXT, q);
                break;
            }

            case S_click:
                if (c == 34) {
                    quote();
                } else if (Flowchart::detail::isSpaceAt(src, p)) {
                    size_t np = p;
                    Flowchart::detail::codepointAt(src, p, np);
                    pop();
                    advance(np);
                } else {
                    size_t q = p + 1;
                    while (q < n && !Flowchart::detail::isSpaceAt(src, q)) q++;
                    emitFlat(CLICK, q);
                }
                break;

            case S_callbackname:
                if (c == 40) {
                    int e = Flowchart::detail::matchEmptyCall(src, p);
                    pop();
                    if (e >= 0) {
                        advance(static_cast<size_t>(e));
                    } else {
                        push(S_callbackargs);
                        advance(p + 1);
                    }
                } else {
                    size_t e = src.find('(', p);
                    if (e == std::string::npos) e = n;
                    emit(CALLBACKNAME, e);
                }
                break;

            case S_callbackargs:
                if (c == 41) {
                    pop();
                    advance(p + 1);
                } else {
                    size_t e = src.find(')', p);
                    if (e == std::string::npos) e = n;
                    emit(CALLBACKARGS, e);
                }
                break;

            case S_shapeData:
                if (c == 34) {
                    push(S_shapeDataStr);
                    emitFlat(SHAPE_DATA, p + 1);
                } else if (c == 125) {
                    pop();
                    advance(p + 1);
                } else if (c == 94) {
                    ok = false;
                } else {
                    size_t q = p + 1;
                    while (q < n) {
                        unsigned char ch = static_cast<unsigned char>(src[q]);
                        if (ch == 125 || ch == 94 || ch == 34) break;
                        q++;
                    }
                    emit(SHAPE_DATA, q);
                }
                break;

            case S_shapeDataStr:
                if (c == 34) {
                    pop();
                    emitFlat(SHAPE_DATA, p + 1);
                } else {
                    size_t e = src.find('"', p);
                    if (e == std::string::npos) e = n;
                    emitText(SHAPE_DATA, Flowchart::detail::replaceNewlineIndentsWithBr(src.substr(p, e - p)), e);
                }
                break;
        }

        if (!ok) {
            types.push_back(ERROR);
            texts.push_back("");
            starts.push_back(p);
            locs.push_back(line);
            locs.push_back(col);
            locs.push_back(line);
            locs.push_back(col);
            return {std::move(types), std::move(texts), std::move(starts), std::move(locs)};
        }
    }

    types.push_back(END);
    texts.push_back("");
    starts.push_back(n);
    locs.push_back(line);
    locs.push_back(col);
    locs.push_back(line);
    locs.push_back(col);
    return {std::move(types), std::move(texts), std::move(starts), std::move(locs)};
}

// ============================================================================
// Port of src/diagrams/agentflow/parser.ts
// ============================================================================

namespace detail {

struct AgentFrame {
    bool global = false;
    size_t at = 0;
    FlowText id;
    bool hasId = false;
    FlowText title;
    bool hasTitle = false;
    std::string data;
    bool hasData = false;
    size_t dataFrom = 0;
    size_t dataTo = 0;
    std::vector<DocItem> list;
    bool collect = false;
};

inline std::pair<std::string, std::vector<std::pair<size_t, size_t>>> stripBraceTrail(const std::string& source) {
    // Replaces /}[^\S\n]*\n/g with "}\n"
    std::string out;
    out.reserve(source.size());
    std::vector<std::pair<size_t, size_t>> edits;
    size_t i = 0;
    size_t n = source.size();
    while (i < n) {
        if (source[i] == '}') {
            size_t j = i + 1;
            while (j < n) {
                size_t np = j;
                int cp = Flowchart::detail::codepointAt(source, j, np);
                if (!isSpace(cp) || cp == '\n') break;
                j = np;
            }
            if (j < n && source[j] == '\n') {
                out.push_back('}');
                if (j > i + 1) edits.push_back({out.size(), j - (i + 1)});
                out.push_back('\n');
                i = j + 1;
                continue;
            }
        }
        out.push_back(source[i++]);
    }
    return {std::move(out), std::move(edits)};
}

} // namespace detail

inline void parseAgentflow(const std::string& source, AgentflowDb& db, bool raw = false) {
    using namespace Tok;
    std::string src = source;
    std::vector<std::pair<size_t, size_t>> braceEdits;
    if (!raw && source.find('}') != std::string::npos) {
        auto [s, r] = detail::stripBraceTrail(source);
        src = std::move(s);
        braceEdits = std::move(r);
    }

    auto mapOffset = [&](size_t at) -> size_t {
        if (braceEdits.empty()) return at;
        size_t orig = at;
        for (const auto& [editPos, removed] : braceEdits) {
            if (at >= editPos) orig += removed;
            else break;
        }
        return orig;
    };

    static const auto ID_TBL = []() {
        std::array<uint8_t, EOF_TOK + 1> a{};
        for (int t : {NUM, NODE_STRING, DOWN, MINUS, DEFAULT, COMMA, COLON, AMP, BRKT, MULT, UNICODE_TEXT}) a[t] = 1;
        return a;
    }();
    static const auto ALPHANUM_TBL = []() {
        std::array<uint8_t, EOF_TOK + 1> a{};
        for (int t : {NUM, UNICODE_TEXT, NODE_STRING, DIR, DOWN, MINUS, COMMA, COLON, AMP, BRKT, MULT}) a[t] = 1;
        return a;
    }();
    static const auto NO_TAGS_TBL = []() {
        std::array<uint8_t, EOF_TOK + 1> a{};
        for (int t : {
            NUM, NODE_STRING, SPACE, MINUS, AMP, UNICODE_TEXT, COLON, MULT, BRKT,
            STYLE, LINKSTYLE, CLASSDEF, CLASS, CLICK, GRAPH, DIR, flow, connector,
            global, end, DOWN, UP, START_LINK
        }) a[t] = 1;
        return a;
    }();
    static const auto STYLE_PART_TBL = []() {
        std::array<uint8_t, EOF_TOK + 1> a{};
        for (int t : {NUM, NODE_STRING, COLON, SPACE, BRKT, STYLE}) a[t] = 1;
        return a;
    }();
    static const auto SEPARATOR_TBL = []() {
        std::array<uint8_t, EOF_TOK + 1> a{};
        for (int t : {NEWLINE, SEMI, EOF_TOK, COMMENT}) a[t] = 1;
        return a;
    }();
    static const auto TRAILING_TBL = []() {
        std::array<uint8_t, EOF_TOK + 1> a{};
        for (int t : {COMMENT, NEWLINE, SPACE, EOF_TOK}) a[t] = 1;
        return a;
    }();

    static const std::string TEXT_TOKENS = "'UNICODE_TEXT', 'TEXT', 'TAGSTART'";
    static const std::string SEPARATORS = "'COMMENT', 'SEMI', 'NEWLINE', 'EOF'";
    static const std::string STATEMENT_START =
        "'COMMENT', 'SEMI', 'NEWLINE', 'SPACE', 'EOF', 'flow', 'global', 'connector', 'acc_title', 'acc_descr', 'acc_descr_multiline_value', 'STYLE', 'LINKSTYLE', 'CLASSDEF', 'CLASS', 'CLICK', 'NODE_STRING'";
    static const char* DIRECTIONS[5] = {"TB", "BT", "RL", "LR", "TD"};

    AgentTokens toks = tokenize(src);
    const auto& types = toks.types;
    const auto& texts = toks.texts;
    const auto& starts = toks.starts;
    const auto& locs = toks.locs;
    size_t i = 0;
    std::string pendingData;
    bool hasPendingData = false;
    size_t dataFrom = 0;
    size_t dataTo = 0;

    auto fail = [&](const std::string& expected) -> void {
        int t = types[i];
        size_t from = starts[i];
        if (t == EOF_TOK || t == END) {
            size_t k = i;
            while (k > 0 && TRAILING_TBL[types[k - 1]]) k--;
            from = trimEndStr(src.substr(0, starts[k])).size();
        }
        size_t at = (src == source) ? from : mapOffset(from);
        if (t == ERROR) throw syntaxError("agentflow", source, at, "", true);
        throw syntaxError("agentflow", source, at, "Expecting " + expected + ", got '" + std::string(tokenName(t)) + "'");
    };

    auto span = [&](size_t a, size_t b) -> Loc {
        return {
            locs[4 * a],
            locs[4 * a + 1],
            locs[4 * b + 2],
            locs[4 * b + 3]
        };
    };

    auto expect = [&](int type, const std::string& also = "") -> std::string {
        if (types[i] != type) {
            fail("'" + std::string(tokenName(type)) + "'" + (!also.empty() ? ", " + also : ""));
        }
        return texts[i++];
    };

    auto separator = [&]() {
        if (SEPARATOR_TBL[types[i]] == 0) fail(SEPARATORS);
        i++;
    };

    auto idString = [&]() -> std::string {
        if (ID_TBL[types[i]] == 0) fail("'NODE_STRING'");
        std::string s = texts[i++];
        while (ID_TBL[types[i]] == 1) s += texts[i++];
        return s;
    };

    auto alphaNum = [&]() -> std::string {
        if (ALPHANUM_TBL[types[i]] == 0) fail("'NODE_STRING'");
        std::string s = texts[i++];
        while (ALPHANUM_TBL[types[i]] == 1) s += texts[i++];
        return s;
    };

    auto text = [&]() -> FlowText {
        int t = types[i];
        std::string type;
        if (t == STR) type = "string";
        else if (t == MD_STR) type = "markdown";
        else if (t == TEXT || t == TAGSTART || t == TAGEND || t == UNICODE_TEXT) type = "text";
        else fail(TEXT_TOKENS + ", 'TAGEND', 'STR', 'MD_STR'");
        std::string s = texts[i++];
        while (true) {
            int n = types[i];
            if (n != TEXT && n != TAGSTART && n != TAGEND && n != UNICODE_TEXT) break;
            s += texts[i++];
        }
        return {s, type};
    };

    auto closed = [&](int type) {
        expect(type, TEXT_TOKENS);
    };

    auto edgeText = [&]() -> FlowText {
        int t = types[i];
        std::string type;
        if (t == STR) type = "string";
        else if (t == MD_STR) type = "markdown";
        else if (t == EDGE_TEXT || t == UNICODE_TEXT) type = "text";
        else fail("'EDGE_TEXT', 'STR', 'MD_STR'");
        std::string s = texts[i++];
        while (types[i] == EDGE_TEXT || types[i] == UNICODE_TEXT) s += texts[i++];
        return {s, type};
    };

    auto textNoTags = [&]() -> FlowText {
        int t = types[i];
        std::string type;
        if (t == STR) type = "text";
        else if (t == MD_STR) type = "markdown";
        else if (NO_TAGS_TBL[t] == 1) type = "text";
        else fail("'NODE_STRING', 'STR', 'MD_STR'");
        std::string s = texts[i++];
        while (NO_TAGS_TBL[types[i]] == 1) s += texts[i++];
        return {s, type};
    };

    auto shapeData = [&]() -> std::string {
        dataFrom = i;
        std::string s = texts[i++];
        while (types[i] == SHAPE_DATA) s += texts[i++];
        dataTo = i - 1;
        return s;
    };

    auto attach = [&](const std::string& id, const std::string& data, size_t from, size_t to) {
        Loc loc = span(from, to);
        db.addVertex(id, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &data, &loc);
        db.extendVertexMapping(id, &loc);
    };

    auto attachData = [&](const std::string& id, const detail::AgentFrame& frame) {
        if (!frame.hasData) return;
        Loc loc = span(frame.dataFrom, frame.dataTo);
        db.addVertex(id, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &frame.data, &loc);
    };

    auto vertex = [&]() -> std::string {
        size_t from = i;
        std::string id = idString();
        FlowText label;
        bool hasLabel = false;
        std::string type;
        bool hasType = false;
        std::map<std::string, std::string> props;
        bool hasProps = false;
        switch (types[i]) {
            case SQS:
                i++;
                label = text();
                hasLabel = true;
                closed(SQE);
                type = "square";
                hasType = true;
                break;
            case DOUBLECIRCLESTART:
                i++;
                label = text();
                hasLabel = true;
                closed(DOUBLECIRCLEEND);
                type = "doublecircle";
                hasType = true;
                break;
            case PS:
                i++;
                if (types[i] == PS) {
                    i++;
                    label = text();
                    hasLabel = true;
                    closed(PE);
                    expect(PE);
                    type = "circle";
                    hasType = true;
                } else {
                    label = text();
                    hasLabel = true;
                    closed(PE);
                    type = "round";
                    hasType = true;
                }
                break;
            case ELLIPSE_START:
                i++;
                label = text();
                hasLabel = true;
                closed(ELLIPSE_END);
                type = "ellipse";
                hasType = true;
                break;
            case STADIUMSTART:
                i++;
                label = text();
                hasLabel = true;
                closed(STADIUMEND);
                type = "stadium";
                hasType = true;
                break;
            case SUBROUTINESTART:
                i++;
                label = text();
                hasLabel = true;
                closed(SUBROUTINEEND);
                type = "subroutine";
                hasType = true;
                break;
            case VERTEX_WITH_PROPS_START: {
                i++;
                std::string field = expect(NODE_STRING);
                expect(COLON);
                std::string value = expect(NODE_STRING);
                expect(PIPE);
                label = text();
                hasLabel = true;
                closed(SQE);
                type = "rect";
                hasType = true;
                props[field] = value;
                hasProps = true;
                break;
            }
            case CYLINDERSTART:
                i++;
                label = text();
                hasLabel = true;
                closed(CYLINDEREND);
                type = "cylinder";
                hasType = true;
                break;
            case DIAMOND_START:
                i++;
                if (types[i] == DIAMOND_START) {
                    i++;
                    label = text();
                    hasLabel = true;
                    closed(DIAMOND_STOP);
                    expect(DIAMOND_STOP);
                    type = "hexagon";
                    hasType = true;
                } else {
                    label = text();
                    hasLabel = true;
                    closed(DIAMOND_STOP);
                    type = "diamond";
                    hasType = true;
                }
                break;
            case TAGEND:
                i++;
                label = text();
                hasLabel = true;
                closed(SQE);
                type = "odd";
                hasType = true;
                break;
            case TRAPSTART:
                i++;
                label = text();
                hasLabel = true;
                if (types[i] == INVTRAPEND) {
                    i++;
                    type = "lean_right";
                    hasType = true;
                } else {
                    closed(TRAPEND);
                    type = "trapezoid";
                    hasType = true;
                }
                break;
            case INVTRAPSTART:
                i++;
                label = text();
                hasLabel = true;
                if (types[i] == TRAPEND) {
                    i++;
                    type = "lean_left";
                    hasType = true;
                } else {
                    closed(INVTRAPEND);
                    type = "inv_trapezoid";
                    hasType = true;
                }
                break;
        }
        if (hasProps) {
            db.addVertex(id, hasLabel ? &label : nullptr, hasType ? &type : nullptr, nullptr, nullptr, nullptr, &props);
        } else if (hasType) {
            db.addVertex(id, hasLabel ? &label : nullptr, &type);
        } else {
            db.addVertex(id);
        }
        Loc loc = span(from, i - 1);
        db.addVertexMapping(id, hasType, &loc);
        return id;
    };

    auto styledVertex = [&]() -> std::string {
        std::string id = vertex();
        if (types[i] == STYLE_SEPARATOR) {
            i++;
            db.setClass(id, idString());
        }
        return id;
    };

    auto node = [&]() -> std::vector<std::string> {
        std::vector<std::string> ids = {styledVertex()};
        hasPendingData = false;
        pendingData.clear();
        while (true) {
            bool hasData = (types[i] == SHAPE_DATA);
            std::string data = hasData ? shapeData() : "";
            size_t from = dataFrom;
            size_t to = dataTo;
            if (types[i] != SPACE) {
                if (hasData) {
                    pendingData = data;
                    hasPendingData = true;
                }
                return ids;
            }
            do i++;
            while (types[i] == SPACE);
            if (types[i] != AMP) {
                if (hasData) fail("'AMP'");
                return ids;
            }
            i++;
            if (types[i] != SPACE) fail("'SPACE'");
            do i++;
            while (types[i] == SPACE);
            std::string next = styledVertex();
            if (hasData) attach(ids.back(), data, from, to);
            ids.push_back(next);
        }
    };

    auto link = [&]() -> AgentLinkInfo {
        std::string id;
        bool hasId = false;
        if (types[i] == LINK_ID) {
            id = texts[i++];
            hasId = true;
        }
        if (types[i] == LINK) {
            AgentLinkInfo info = db.destructLink(texts[i++]);
            if (hasId) {
                info.id = id;
                info.hasId = true;
            }
            if (types[i] == PIPE) {
                i++;
                info.text = text();
                info.hasText = true;
                closed(PIPE);
                if (types[i] == SPACE) i++;
            }
            return info;
        }
        if (types[i] == START_LINK) {
            std::string start = texts[i++];
            FlowText label = edgeText();
            std::string end = expect(LINK, "'EDGE_TEXT'");
            AgentLinkInfo info = db.destructLink(end, start);
            info.text = label;
            info.hasText = true;
            if (hasId) {
                info.id = id;
                info.hasId = true;
            }
            return info;
        }
        fail("'LINK', 'START_LINK'");
        return {};
    };

    auto vertexStatement = [&]() -> std::vector<std::string> {
        size_t from = i;
        std::vector<std::string> stmt = node();
        if (hasPendingData) attach(stmt.back(), pendingData, dataFrom, dataTo);
        std::vector<std::string> nodes = stmt;
        while (true) {
            int t = types[i];
            if (t != LINK && t != START_LINK && t != LINK_ID) return nodes;
            AgentLinkInfo info = link();
            std::vector<std::string> next = node();
            if (hasPendingData) attach(next.back(), pendingData, dataFrom, dataTo);
            db.addLink(stmt, next, &info);
            Loc loc = span(from, i - 1);
            db.addEdgeMapping(next, &loc);
            std::vector<std::string> combined = next;
            combined.insert(combined.end(), nodes.begin(), nodes.end());
            nodes = std::move(combined);
            stmt = std::move(next);
        }
    };

    auto style = [&]() -> std::string {
        if (STYLE_PART_TBL[types[i]] == 0) fail("'NODE_STRING', 'NUM', 'COLON', 'BRKT'");
        std::string s = texts[i++];
        while (STYLE_PART_TBL[types[i]] == 1) s += texts[i++];
        return s;
    };

    auto stylesOpt = [&]() -> std::vector<std::string> {
        std::vector<std::string> out = {style()};
        while (types[i] == COMMA) {
            i++;
            out.push_back(style());
        }
        return out;
    };

    auto linkStyleStatement = [&]() {
        i++;
        expect(SPACE);
        std::vector<std::string> positions;
        if (types[i] == DEFAULT) {
            positions.push_back(texts[i++]);
        } else {
            positions.push_back(expect(NUM, "'DEFAULT'"));
            while (types[i] == COMMA) {
                i++;
                positions.push_back(expect(NUM));
            }
        }
        expect(SPACE);
        if (types[i] != INTERPOLATE) {
            db.updateLink(positions, stylesOpt());
            return;
        }
        i++;
        expect(SPACE);
        std::string interp = alphaNum();
        if (types[i] == SPACE) {
            i++;
            auto styles = stylesOpt();
            db.updateLinkInterpolate(positions, interp);
            db.updateLink(positions, styles);
        } else {
            db.updateLinkInterpolate(positions, interp);
        }
    };

    auto linkTail = [&](const std::string& id, const std::string& url) {
        if (types[i] != SPACE) {
            db.setLink(id, url);
            return;
        }
        i++;
        if (types[i] == LINK_TARGET) {
            db.setLink(id, url, texts[i++]);
            return;
        }
        std::string tooltip = expect(STR, "'LINK_TARGET'");
        if (types[i] == SPACE) {
            i++;
            db.setLink(id, url, expect(LINK_TARGET));
        } else {
            db.setLink(id, url);
        }
        db.setTooltip(id, &tooltip);
    };

    auto clickStatement = [&]() {
        std::string id = texts[i++];
        switch (types[i]) {
            case CALLBACKNAME: {
                std::string name = texts[i++];
                bool hasArgs = (types[i] == CALLBACKARGS);
                std::string args = hasArgs ? texts[i++] : "";
                std::string tooltip;
                bool hasTooltip = false;
                if (types[i] == SPACE) {
                    i++;
                    tooltip = expect(STR);
                    hasTooltip = true;
                }
                if (hasArgs) db.setClickEvent(id, name, args);
                else db.setClickEvent(id, name);
                if (hasTooltip) db.setTooltip(id, &tooltip);
                return;
            }
            case HREF:
                i++;
                linkTail(id, expect(STR));
                return;
            case STR:
                linkTail(id, texts[i++]);
                return;
            default: {
                std::string name = alphaNum();
                if (types[i] == SPACE) {
                    i++;
                    std::string tooltip = expect(STR);
                    db.setClickEvent(id, name);
                    db.setTooltip(id, &tooltip);
                } else {
                    db.setClickEvent(id, name);
                }
                return;
            }
        }
    };

    auto header = [&](size_t at) -> detail::AgentFrame {
        expect(SPACE, SEPARATORS);
        FlowText id = textNoTags();
        FlowText title{"", "text"};
        if (types[i] == SQS) {
            i++;
            title = text();
            closed(SQE);
        }
        bool hasData = (types[i] == SHAPE_DATA);
        std::string data = hasData ? shapeData() : "";
        if (SEPARATOR_TBL[types[i]] == 0) fail(SEPARATORS + ", 'SHAPE_DATA'");
        detail::AgentFrame frame;
        frame.global = false;
        frame.at = at;
        frame.id = id;
        frame.hasId = true;
        frame.title = title;
        frame.hasTitle = true;
        frame.data = data;
        frame.hasData = hasData;
        frame.dataFrom = dataFrom;
        frame.dataTo = dataTo;
        return frame;
    };

    auto statement = [&]() -> DocItem {
        int t = types[i];
        switch (t) {
            case STYLE: {
                i++;
                expect(SPACE);
                std::string id = idString();
                expect(SPACE);
                auto st = stylesOpt();
                db.addVertex(id, nullptr, nullptr, &st);
                separator();
                return DocItem::makeArr({});
            }
            case LINKSTYLE:
                linkStyleStatement();
                separator();
                return DocItem::makeArr({});
            case CLASSDEF: {
                i++;
                expect(SPACE);
                std::string id = idString();
                expect(SPACE);
                db.addClass(id, stylesOpt());
                separator();
                return DocItem::makeArr({});
            }
            case CLASS: {
                i++;
                expect(SPACE);
                std::string ids = idString();
                expect(SPACE);
                db.setClass(ids, idString());
                separator();
                return DocItem::makeArr({});
            }
            case CLICK:
                clickStatement();
                separator();
                return DocItem::makeArr({});
            case connector: {
                detail::AgentFrame frame = header(i++);
                size_t sep = i++;
                std::string id = db.addConnector(frame.hasId ? &frame.id : nullptr, frame.hasTitle ? &frame.title : nullptr);
                attachData(id, frame);
                Loc startLoc = span(frame.at, frame.at);
                Loc endLoc = span(sep, sep);
                db.addConnectorMapping(frame.hasId ? &frame.id : nullptr, &startLoc, &endLoc);
                return DocItem::makeStr(id);
            }
            case direction_tb:
            case direction_bt:
            case direction_rl:
            case direction_lr:
            case direction_td:
                i++;
                return DocItem::makeDir(DIRECTIONS[t - direction_tb]);
            case acc_title: {
                i++;
                std::string val = trimStr(expect(acc_title_value));
                db.setAccTitle(val);
                return DocItem::makeStr(val);
            }
            case acc_descr: {
                i++;
                std::string val = trimStr(expect(acc_descr_value));
                db.setAccDescription(val);
                return DocItem::makeStr(val);
            }
            case acc_descr_multiline_value: {
                std::string val = trimStr(texts[i++]);
                db.setAccDescription(val);
                return DocItem::makeStr(val);
            }
            default: {
                if (ID_TBL[t] == 0) fail(STATEMENT_START);
                auto nodes = vertexStatement();
                separator();
                return DocItem::makeArr(std::move(nodes));
            }
        }
    };

    auto document = [&]() {
        std::vector<detail::AgentFrame> open;
        std::vector<DocItem> list;
        bool collect = false;
        while (true) {
            int t = types[i];
            if (t == END) break;
            if (t == end) {
                if (open.empty()) break;
                detail::AgentFrame frame = std::move(open.back());
                open.pop_back();
                size_t at = i++;
                if (frame.global) {
                    db.addGlobal(list);
                    list = std::move(frame.list);
                    collect = frame.collect;
                    continue;
                }
                std::string id = db.addSubGraph(frame.hasId ? &frame.id : nullptr, list, frame.hasTitle ? &frame.title : nullptr);
                attachData(id, frame);
                Loc startLoc = span(frame.at, frame.at);
                Loc endLoc = span(at, at);
                db.addSubgraphMapping(frame.hasId ? &frame.id : nullptr, &startLoc, &endLoc);
                list = std::move(frame.list);
                collect = frame.collect;
                if (collect) list.push_back(DocItem::makeStr(id));
            } else if (t == SEMI || t == NEWLINE || t == SPACE || t == EOF_TOK) {
                if (collect) list.push_back(DocItem::makeStr(texts[i]));
                i++;
            } else if (t == COMMENT) {
                i++;
            } else if (t == flow || t == global) {
                size_t at = i++;
                detail::AgentFrame frame;
                if (t == global || SEPARATOR_TBL[types[i]] == 1) {
                    frame.global = (t == global);
                    frame.at = at;
                    frame.list = std::move(list);
                    frame.collect = collect;
                    separator();
                } else {
                    frame = header(at);
                    frame.list = std::move(list);
                    frame.collect = collect;
                    i++;
                }
                open.push_back(std::move(frame));
                list.clear();
                collect = true;
            } else {
                DocItem item = statement();
                if (collect && (item.kind != DocItem::Arr || !item.arrVal.empty())) {
                    list.push_back(std::move(item));
                }
            }
        }
        if (!open.empty()) fail("'end'");
    };

    while (types[i] == SPACE || types[i] == NEWLINE || types[i] == COMMENT) i++;
    expect(GRAPH, "'SPACE', 'NEWLINE', 'COMMENT'");
    if (types[i] == NODIR) {
        i++;
        db.setDirection("TB");
    } else {
        std::string dir = expect(DIR, "'NODIR'");
        int t = types[i];
        if (t == SPACE) {
            do i++;
            while (types[i] == SPACE);
            t = types[i];
            if (t != NEWLINE) fail("'NEWLINE', 'SPACE'");
        } else if (t != SEMI && t != NEWLINE && t != COMMENT) {
            fail("'COMMENT', 'SEMI', 'NEWLINE', 'SPACE'");
        }
        i++;
        db.setDirection(dir);
    }
    document();
    if (types[i] != END) fail(STATEMENT_START);
}

// ============================================================================
// Port of src/diagrams/agentflow/graph.ts
// ============================================================================

struct GraphNode {
    std::string id;
    std::string label;
    bool hasLabel = false;
    std::string labelType = "text";
    std::string shape;
    bool isGroup = false;
    std::string parentId;
    bool hasParentId = false;
    std::vector<std::string> cssStyles;
    std::vector<std::string> cssCompiledStyles;
    std::string cssClasses;
    std::string dir;
    std::string link;
    std::string linkTarget;
    std::string tooltip;
    int colorIndex = 0;
    bool hasColorIndex = false;
    std::string kind;
    bool hasKind = false;
    YamlValue metadata = YamlValue::makeNull();
    bool hasMetadata = false;
};

struct GraphEdge {
    std::string id;
    bool isUserDefinedId = false;
    std::string start;
    std::string end;
    std::string type;
    std::string label;
    std::string labelType = "text";
    std::string thickness;
    std::string pattern;
    int minlen = 1;
    bool hasMinlen = false;
    std::string arrowTypeStart;
    std::string arrowTypeEnd;
    std::vector<std::string> cssCompiledStyles;
    std::vector<std::string> style;
    std::string classes;
    YamlValue animate = YamlValue::makeNull();
    bool hasAnimate = false;
    YamlValue animation = YamlValue::makeNull();
    bool hasAnimation = false;
    std::string curve;
    YamlValue metadata = YamlValue::makeNull();
    bool hasMetadata = false;
};

struct AgentGraph {
    std::vector<GraphNode> nodes;
    std::vector<GraphEdge> edges;
};

inline void normaliseNodeShapes(std::vector<GraphNode>& nodes, AgentflowDb& sink) {
    const auto& removed = removedShapesSet();
    const auto& allowed = allowedShapesSet();
    for (auto& node : nodes) {
        if (node.isGroup) continue;
        if (node.shape.empty() || node.shape == "squareRect" || node.shape == "rect") {
            node.shape = DEFAULT_SHAPE;
        }
        if (removed.count(node.shape)) {
            sink.emitError("SHAPE_REMOVED", "shape \"" + node.shape + "\" was removed in v0.8.1, using \"" + DEFAULT_SHAPE + "\"", node.id);
            node.shape = DEFAULT_SHAPE;
        } else if (!allowed.count(node.shape)) {
            sink.emitWarning("SHAPE_UNSUPPORTED", "shape \"" + node.shape + "\" is not supported, using \"" + DEFAULT_SHAPE + "\"", node.id);
            node.shape = DEFAULT_SHAPE;
        }
    }
}

template <typename KindOfFn>
inline void assignColorSlots(
    std::vector<GraphNode>& nodes,
    KindOfFn&& kindOf,
    const std::unordered_map<std::string, int>& containerOrder,
    int paletteLength
) {
    const auto& ks = kindSlotMap();
    int fallback = static_cast<int>(containerOrder.size());
    for (auto& node : nodes) {
        if (node.isGroup || node.shape == "collapsedGroup") {
            auto it = containerOrder.find(node.id);
            int ord = (it != containerOrder.end()) ? it->second : fallback++;
            node.colorIndex = containerSlot(ord, paletteLength);
            node.hasColorIndex = true;
            continue;
        }
        std::string kind = kindOf(node.id);
        if (!kind.empty() && ks.count(kind)) {
            node.kind = kind;
            node.hasKind = true;
        }
    }
}

inline AgentGraph buildAgentGraph(AgentflowDb& db, int paletteLength, const std::string& defaultCurve = "") {
    std::vector<GraphNode> nodes;
    std::vector<GraphEdge> edges;
    const auto& subgraphs = db.subgraphs;

    auto compiled = [&](const std::vector<std::string>& classNamesList) -> std::vector<std::string> {
        std::vector<std::string> out;
        for (const auto& name : classNamesList) {
            auto it = db.classes.find(name);
            if (it == db.classes.end()) continue;
            const auto& def = it->second;
            if (!def.styles.empty()) {
                for (const auto& s : def.styles) out.push_back(trimStr(s));
                for (auto& s : out) s = trimStr(s);
            }
            if (!def.textStyles.empty()) {
                for (const auto& s : def.textStyles) out.push_back(trimStr(s));
                for (auto& s : out) s = trimStr(s);
            }
        }
        return out;
    };

    auto joinSpace = [](const std::vector<std::string>& v) -> std::string {
        std::string res;
        for (size_t i = 0; i < v.size(); i++) {
            if (i > 0) res.push_back(' ');
            res += v[i];
        }
        return res;
    };

    std::vector<std::string> hiddenList;
    std::unordered_set<std::string> hidden;
    std::unordered_map<std::string, std::string> owner;
    std::unordered_set<std::string> walked;
    for (const auto& sg : subgraphs) {
        bool isCollapsed = sg.hasMetadata && sg.metadata.isMap() &&
                           sg.metadata.get("view") != nullptr &&
                           sg.metadata.get("view")->isString() &&
                           sg.metadata.get("view")->sVal == "collapsed";
        if (!isCollapsed || hidden.count(sg.id)) continue;
        std::vector<std::string> pending = {sg.id};
        while (!pending.empty()) {
            std::string cur = pending.back();
            pending.pop_back();
            const AgentSubgraph* sub = db.subgraph(cur);
            if (sub == nullptr) continue;
            for (const auto& childId : sub->nodes) {
                if (childId == sg.id || hidden.count(childId)) continue;
                hidden.insert(childId);
                hiddenList.push_back(childId);
                owner[childId] = sg.id;
                if (!walked.count(childId)) pending.push_back(childId);
            }
        }
        walked.insert(sg.id);
    }

    std::unordered_map<std::string, std::string> collapsedInto;
    for (const auto& id : hiddenList) {
        std::vector<std::string> path = {id};
        std::string top = owner[id];
        while (true) {
            auto kIt = collapsedInto.find(top);
            bool hasKnown = (kIt != collapsedInto.end());
            auto oIt = owner.find(top);
            std::string up = hasKnown ? kIt->second : (oIt != owner.end() ? oIt->second : "");
            if (up.empty()) break;
            if (!hasKnown) path.push_back(top);
            top = up;
            if (hasKnown) break;
        }
        for (const auto& p : path) collapsedInto[p] = top;
    }

    std::unordered_map<std::string, size_t> posMap;
    for (size_t i = 0; i < subgraphs.size(); i++) posMap[subgraphs[i].id] = i;

    std::unordered_map<std::string, std::string> parentOf;
    auto closesLoop = [&](const std::string& childId, const std::string& parentId, size_t at) -> bool {
        auto cIt = posMap.find(childId);
        if (cIt == posMap.end() || cIt->second < at) return false;
        std::string current = parentId;
        for (size_t hops = 0; !current.empty() && hops <= subgraphs.size(); hops++) {
            if (current == childId) return true;
            auto pIt = parentOf.find(current);
            current = (pIt != parentOf.end()) ? pIt->second : "";
        }
        return false;
    };

    for (int i = static_cast<int>(subgraphs.size()) - 1; i >= 0; i--) {
        const auto& sg = subgraphs[static_cast<size_t>(i)];
        if (hidden.count(sg.id)) continue;
        for (const auto& id : sg.nodes) {
            if (closesLoop(id, sg.id, static_cast<size_t>(i))) {
                db.emitWarning(
                    "CONTAINMENT_VIOLATION",
                    "Container \"" + sg.id + "\" cannot contain \"" + id + "\" because \"" + id + "\" already contains it. The nesting that would close the loop is dropped.",
                    id
                );
                continue;
            }
            parentOf[id] = sg.id;
        }
    }

    std::unordered_map<std::string, size_t> byId;
    for (int i = static_cast<int>(subgraphs.size()) - 1; i >= 0; i--) {
        const auto& sg = subgraphs[static_cast<size_t>(i)];
        if (hidden.count(sg.id)) continue;
        bool collapsed = sg.hasMetadata && sg.metadata.isMap() &&
                         sg.metadata.get("view") != nullptr &&
                         sg.metadata.get("view")->isString() &&
                         sg.metadata.get("view")->sVal == "collapsed";
        GraphNode node;
        node.id = sg.id;
        node.label = sg.title;
        node.hasLabel = true;
        node.labelType = sg.labelType;
        auto pIt = parentOf.find(sg.id);
        if (pIt != parentOf.end()) {
            node.parentId = pIt->second;
            node.hasParentId = true;
        }
        node.cssCompiledStyles = compiled(sg.classes);
        node.cssClasses = joinSpace(sg.classes);
        node.shape = collapsed ? "collapsedGroup" : "flowGroup";
        node.dir = sg.dir;
        node.isGroup = !collapsed;
        if (collapsed) {
            node.metadata = sg.hasMetadata && sg.metadata.isMap() ? sg.metadata : YamlValue::makeMap();
            node.metadata.mVal["containerType"] = YamlValue::makeString(sg.type);
            node.hasMetadata = true;
        } else if (sg.hasMetadata) {
            node.metadata = sg.metadata;
            node.hasMetadata = true;
        }
        size_t idx = nodes.size();
        nodes.push_back(std::move(node));
        byId[sg.id] = idx;
    }

    for (const auto& nodeId : db.nodeOrder) {
        if (hidden.count(nodeId)) continue;
        const auto& vertex = db.nodes.at(nodeId);
        auto exIt = byId.find(vertex.id);
        if (exIt != byId.end()) {
            auto& existing = nodes[exIt->second];
            existing.cssStyles = vertex.styles;
            existing.cssCompiledStyles = compiled(vertex.classes);
            existing.cssClasses = joinSpace(vertex.classes);
            continue;
        }
        std::string type = resolveShapeAlias(vertex.type);
        GraphNode node;
        node.id = vertex.id;
        node.label = vertex.text;
        node.hasLabel = vertex.hasText;
        node.labelType = vertex.labelType;
        auto pIt = parentOf.find(vertex.id);
        if (pIt != parentOf.end()) {
            node.parentId = pIt->second;
            node.hasParentId = true;
        }
        node.cssStyles = vertex.styles;
        std::vector<std::string> fullClasses = {"default", "node"};
        fullClasses.insert(fullClasses.end(), vertex.classes.begin(), vertex.classes.end());
        node.cssCompiledStyles = compiled(fullClasses);
        node.cssClasses = "default " + joinSpace(vertex.classes);
        node.dir = vertex.dir;
        node.link = vertex.link;
        node.linkTarget = vertex.linkTarget;
        auto ttIt = db.tooltips.find(vertex.id);
        if (ttIt != db.tooltips.end()) node.tooltip = ttIt->second;
        if (vertex.hasMetadata) {
            node.metadata = vertex.metadata;
            node.hasMetadata = true;
        }
        node.isGroup = false;
        node.shape = (type == "square" || type.empty()) ? "squareRect" : type;
        size_t idx = nodes.size();
        nodes.push_back(std::move(node));
        byId[vertex.id] = idx;
    }

    for (size_t index = 0; index < db.edges.size(); index++) {
        const auto& raw = db.edges[index];
        auto sIt = collapsedInto.find(raw.start);
        std::string start = (sIt != collapsedInto.end()) ? sIt->second : raw.start;
        auto eIt = collapsedInto.find(raw.end);
        std::string end = (eIt != collapsedInto.end()) ? eIt->second : raw.end;
        if (start == end && raw.start != raw.end) continue;
        std::vector<std::string> styles;
        if (db.hasDefaultEdgeStyle) styles = db.defaultEdgeStyle;
        if (raw.hasStyle) styles.insert(styles.end(), raw.style.begin(), raw.style.end());
        std::string arrow = (raw.type == "arrow_open")
            ? "none"
            : ((raw.type == "arrow_cross" || raw.type == "arrow_point") ? raw.type : "arrow_point");
        GraphEdge ge;
        ge.id = !raw.id.empty() ? raw.id : ("L_" + start + "_" + end + "_" + std::to_string(index));
        ge.isUserDefinedId = raw.isUserDefinedId;
        ge.start = start;
        ge.end = end;
        ge.type = raw.hasType ? raw.type : "normal";
        ge.label = raw.text;
        ge.labelType = raw.labelType;
        ge.thickness = raw.stroke;
        ge.pattern = raw.stroke;
        ge.minlen = raw.length;
        ge.hasMinlen = raw.hasLength;
        ge.classes = "edge-thickness-normal edge-pattern-solid flowchart-link";
        ge.arrowTypeStart = "none";
        ge.arrowTypeEnd = arrow;
        ge.cssCompiledStyles = compiled(raw.classes);
        ge.style = std::move(styles);
        if (raw.hasAnimate) { ge.animate = raw.animate; ge.hasAnimate = true; }
        if (raw.hasAnimation) { ge.animation = raw.animation; ge.hasAnimation = true; }
        ge.curve = !raw.interpolate.empty() ? raw.interpolate : (!db.defaultInterpolate.empty() ? db.defaultInterpolate : defaultCurve);
        if (raw.hasMetadata) { ge.metadata = raw.metadata; ge.hasMetadata = true; }
        edges.push_back(std::move(ge));
    }

    normaliseNodeShapes(nodes, db);

    std::unordered_map<std::string, std::vector<std::string>> children;
    for (const auto& sg : subgraphs) {
        auto pIt = parentOf.find(sg.id);
        if (pIt == parentOf.end()) continue;
        children[pIt->second].push_back(sg.id);
    }
    std::unordered_map<std::string, int> order;
    for (const auto& sg : subgraphs) {
        if (parentOf.count(sg.id)) continue;
        std::vector<std::string> pending = {sg.id};
        while (!pending.empty()) {
            std::string id = pending.back();
            pending.pop_back();
            if (order.count(id)) continue;
            int nextOrd = static_cast<int>(order.size());
            order[id] = nextOrd;
            auto kIt = children.find(id);
            if (kIt != children.end()) {
                const auto& kids = kIt->second;
                for (int i = static_cast<int>(kids.size()) - 1; i >= 0; i--) {
                    pending.push_back(kids[static_cast<size_t>(i)]);
                }
            }
        }
    }

    assignColorSlots(
        nodes,
        [&](const std::string& id) -> std::string {
            if (db.connectors.count(id)) return "connector";
            auto nIt = db.nodes.find(id);
            return nIt != db.nodes.end() ? db.vertexKind(nIt->second) : "";
        },
        order,
        paletteLength
    );

    return {std::move(nodes), std::move(edges)};
}

// ============================================================================
// Port of src/diagrams/agentflow/render.ts
// ============================================================================

constexpr int PALETTE = 8;
constexpr double NODE_SEP = 40.0;
constexpr double EDGE_SEP = 16.0;
constexpr double RANK_SEP = 48.0;
constexpr double GROUP_PAD = 20.0;
constexpr double LOOP = 26.0;
constexpr double DOTS = 14.0;

inline int flowEndSide(const std::string& dir) {
    if (dir == "RL") return 3;
    if (dir == "BT") return 0;
    if (dir == "LR") return 1;
    return 2; // TB
}

inline double numberOption(const Config& config, const std::string& key, double fallback) {
    if (!config.isMap()) return fallback;
    const auto* af = config.get("agentflow");
    if (!af || !af->isMap()) return fallback;
    const auto* v = af->get(key);
    if (v && v->isNumber() && v->nVal > 0.0) return std::min(v->nVal, 2000.0);
    return fallback;
}

inline std::string tint(int slot, const std::string& wash, const ResolvedStyle& style) {
    std::string color = seriesColor(slot);
    return " fill=\"" + color + "\"" + (setsFill(style.shape) ? "" : (" fill-opacity=\"" + wash + "\"")) + " stroke=\"" + color + "\"";
}

inline std::string dotPath(double x, double y) {
    return "M" + afNum(x - 1.75) + "," + afNum(y) + "a1.75,1.75 0 1 0 3.5,0a1.75,1.75 0 1 0 -3.5,0Z";
}

inline std::string drawAgentShape(const std::string& shape, double w, double h, const std::string& a, const std::string& line) {
    auto afRect = [&](double rw, double rh, const std::string& attrs, const std::string& extra = "") {
        return "<rect x=\"" + afNum(-rw / 2.0) + "\" y=\"" + afNum(-rh / 2.0) + "\" width=\"" + afNum(rw) + "\" height=\"" + afNum(rh) + "\"" + extra + attrs + "/>";
    };
    auto afPoly = [&](const std::vector<double>& pts, const std::string& attrs) {
        std::string s = "<polygon points=\"";
        for (size_t i = 0; i < pts.size(); i += 2) {
            if (i > 0) s.push_back(' ');
            s += afNum(pts[i]) + "," + afNum(pts[i + 1]);
        }
        return s + "\"" + attrs + "/>";
    };
    auto afWave = [&](double ww, double wh, const std::string& attrs) {
        double x = ww / 2.0;
        double y = wh / 2.0 - 10.0;
        double q = ww / 4.0;
        return "<path d=\"M" + afNum(-x) + "," + afNum(-wh / 2.0) + "H" + afNum(x) + "V" + afNum(y) + "q" + afNum(-q) + ",-12 " + afNum(-2.0 * q) + ",0t" + afNum(-2.0 * q) + ",0Z\"" + attrs + "/>";
    };
    if (shape == "rounded") return afRect(w, h, a, " rx=\"" + afNum(std::min(12.0, h / 2.0)) + "\"");
    if (shape == "fr-rect") {
        double x = w / 2.0 - 8.0;
        return afRect(w, h, rounded(a)) + "<path d=\"M" + afNum(-x) + "," + afNum(-h / 2.0) + "V" + afNum(h / 2.0) + "M" + afNum(x) + "," + afNum(-h / 2.0) + "V" + afNum(h / 2.0) + "\" fill=\"none\"" + line + "/>";
    }
    if (shape == "diam") return afPoly({0.0, -h / 2.0, w / 2.0, 0.0, 0.0, h / 2.0, -w / 2.0, 0.0}, a);
    if (shape == "hex") {
        double m = h / 4.0;
        return afPoly({-w / 2.0 + m, -h / 2.0, w / 2.0 - m, -h / 2.0, w / 2.0, 0.0, w / 2.0 - m, h / 2.0, -w / 2.0 + m, h / 2.0, -w / 2.0, 0.0}, a);
    }
    if (shape == "lean-r") {
        double s = h / 4.0;
        return afPoly({-w / 2.0 + s, -h / 2.0, w / 2.0, -h / 2.0, w / 2.0 - s, h / 2.0, -w / 2.0, h / 2.0}, a);
    }
    if (shape == "lin-doc") {
        return afWave(w, h, a) + "<path d=\"M" + afNum(-w / 2.0 + 8.0) + "," + afNum(-h / 2.0) + "V" + afNum(h / 2.0 - 6.0) + "\" fill=\"none\"" + line + "/>";
    }
    return drawShape(shape, w, h, a, line);
}

struct NodeView {
    GraphNode node;
    ResolvedStyle style;
    Label label;
    std::string shape;
    double w = 0.0;
    double h = 0.0;
    double dy = 0.0;
    int loops = 0;
};

inline Rendered drawAgentflow(
    AgentflowDb& db,
    const AgentGraph& graph,
    const Config& config,
    const RenderOptions& options,
    bool turned
) {
    double size = options.hasFontSize ? options.fontSize : 16.0;
    double edgeSize = Flowchart::jsRound(size * 0.875);
    const auto& measurer = getMeasurer(options);
    double wrapWidth = numberOption(config, "wrappingWidth", 120.0);
    double minWidth = numberOption(config, "minNodeWidth", 120.0);
    double pad = options.hasPadding ? options.padding : 8.0;
    const auto& icons = options.icons;

    std::unordered_map<std::string, int> index;
    std::vector<NodeView> views;
    std::vector<Flowchart::CNode> cnodes;
    views.reserve(graph.nodes.size());
    cnodes.reserve(graph.nodes.size());
    for (size_t i = 0; i < graph.nodes.size(); i++) {
        index[graph.nodes[i].id] = static_cast<int>(i);
    }

    for (const auto& node : graph.nodes) {
        std::vector<std::string> styles = node.cssCompiledStyles;
        if (!node.cssStyles.empty()) {
            styles.insert(styles.end(), node.cssStyles.begin(), node.cssStyles.end());
        }
        ResolvedStyle style = resolveStyle(styles);
        int textStyle = (style.bold ? Style::Bold : 0) | (style.italic ? Style::Italic : 0);
        bool markdown = (node.labelType == "markdown");
        double fontSize = style.hasFontSize ? style.fontSize : size;
        NodeView view;
        view.node = node;
        view.style = style;
        Flowchart::CNode c;
        if (node.isGroup) {
            view.label = layoutLabel(node.label, markdown, measurer, fontSize, 4000.0, textStyle);
            view.shape = "group";
            c = Flowchart::cnode(0.0, 0.0);
            c.isGroup = true;
            if (!node.dir.empty()) {
                c.dir = Flowchart::direction(node.dir);
            }
            c.padX = GROUP_PAD;
            c.padTop = view.label.height > 0.0 ? (view.label.height + 16.0) : GROUP_PAD;
            c.padBottom = GROUP_PAD;
            c.minW = view.label.width + 2.0 * GROUP_PAD - 12.0;
        } else if (node.shape == "collapsedGroup") {
            view.label = layoutLabel(node.label, markdown, measurer, fontSize, wrapWidth, textStyle);
            double w = std::max(view.label.width, minWidth) + 2.0 * SHAPE_PAD_X;
            double h = view.label.height + 2.0 * SHAPE_PAD_Y + DOTS;
            view.shape = "collapsed";
            view.w = w;
            view.h = h;
            view.dy = -DOTS / 2.0;
            c = Flowchart::cnode(w, h);
        } else {
            std::string shape = Flowchart::canonicalShape(node.shape);
            view.label = layoutLabel(node.label, markdown, measurer, fontSize, wrapWidth, textStyle);
            auto s = shapeSize(shape, shape == "diam" ? view.label.width : std::max(view.label.width, minWidth), view.label.height);
            view.shape = shape;
            view.w = s.w;
            view.h = s.h;
            view.dy = s.dy;
            c = Flowchart::cnode(s.w, s.h);
        }
        c.seq = static_cast<int>(views.size());
        views.push_back(std::move(view));
        cnodes.push_back(std::move(c));
    }

    for (size_t i = 0; i < graph.nodes.size(); i++) {
        const auto& node = graph.nodes[i];
        if (node.hasParentId) {
            auto pIt = index.find(node.parentId);
            cnodes[i].parent = (pIt != index.end()) ? pIt->second : -1;
        } else {
            cnodes[i].parent = -1;
        }
    }

    std::vector<std::pair<int, int>> edgePairs;
    edgePairs.reserve(graph.edges.size());
    for (const auto& edge : graph.edges) {
        auto sIt = index.find(edge.start);
        auto eIt = index.find(edge.end);
        edgePairs.push_back({sIt != index.end() ? sIt->second : -1, eIt != index.end() ? eIt->second : -1});
    }
    Flowchart::inheritCrossedDirections(cnodes, edgePairs);

    std::string rootDir = turned ? "TB" : Flowchart::direction(db.direction);
    auto flowOf = [&](int i) -> std::string {
        std::string dir;
        for (int p = cnodes[static_cast<size_t>(i)].parent, hops = 0; p >= 0 && dir.empty() && hops < 10000; p = cnodes[static_cast<size_t>(p)].parent, hops++) {
            if (!cnodes[static_cast<size_t>(p)].dir.empty()) dir = cnodes[static_cast<size_t>(p)].dir;
        }
        return !dir.empty() ? dir : rootDir;
    };
    auto flowsSideways = [&](int i) -> bool {
        std::string dir = flowOf(i);
        return dir == "LR" || dir == "RL";
    };

    std::vector<int> depth(cnodes.size(), -1);
    for (size_t i = 0; i < cnodes.size(); i++) {
        std::vector<size_t> chain;
        int j = static_cast<int>(i);
        while (j >= 0 && depth[static_cast<size_t>(j)] < 0 && chain.size() <= cnodes.size()) {
            chain.push_back(static_cast<size_t>(j));
            j = cnodes[static_cast<size_t>(j)].parent;
        }
        int d = j >= 0 ? (depth[static_cast<size_t>(j)] + 1) : 0;
        for (int k = static_cast<int>(chain.size()) - 1; k >= 0; k--) {
            depth[chain[static_cast<size_t>(k)]] = d++;
        }
    }
    auto contains = [&](int group, int i) -> bool {
        int j = i;
        for (int d = depth[static_cast<size_t>(i)]; d > depth[static_cast<size_t>(group)]; d--) {
            j = cnodes[static_cast<size_t>(j)].parent;
        }
        return j == group && i != group;
    };

    struct HeldEdge {
        GraphEdge edge;
        Label label;
        int src = 0;
        int dst = 0;
    };

    std::vector<Flowchart::CEdge> cedges;
    std::vector<Label> edgeLabels;
    std::vector<GraphEdge> drawn;
    std::vector<HeldEdge> held;

    for (const auto& edge : graph.edges) {
        auto sIt = index.find(edge.start);
        auto dIt = index.find(edge.end);
        if (sIt == index.end() || dIt == index.end()) continue;
        int src = sIt->second;
        int dst = dIt->second;
        Label label = layoutLabel(edge.label, edge.labelType == "markdown", measurer, edgeSize, wrapWidth);
        if (contains(src, dst) || contains(dst, src)) {
            held.push_back({edge, std::move(label), src, dst});
            continue;
        }
        if (src == dst) {
            views[static_cast<size_t>(src)].loops++;
            if (flowsSideways(src)) {
                cnodes[static_cast<size_t>(src)].h += 2.0 * (LOOP + (label.height > 0.0 ? (label.height + 8.0) : 0.0));
            } else {
                cnodes[static_cast<size_t>(src)].w += 2.0 * (LOOP + (label.width > 0.0 ? (label.width + 12.0) : 0.0));
            }
        }
        drawn.push_back(edge);
        Flowchart::CEdge ce;
        ce.src = src;
        ce.dst = dst;
        ce.minlen = edge.hasMinlen ? edge.minlen : 1;
        ce.labelW = label.width > 0.0 ? (label.width + 12.0) : 0.0;
        ce.labelH = label.height > 0.0 ? (label.height + 4.0) : 0.0;
        cedges.push_back(std::move(ce));
        edgeLabels.push_back(std::move(label));
    }

    Flowchart::LayeredOptions layoutOpts;
    layoutOpts.nodeSep = numberOption(config, "nodeSpacing", NODE_SEP);
    layoutOpts.edgeSep = EDGE_SEP;
    layoutOpts.rankSep = numberOption(config, "rankSpacing", RANK_SEP);
    layoutOpts.portSep = 20.0;
    auto layout = Flowchart::compoundLayout(cnodes, cedges, rootDir, layoutOpts);

    Label title = layoutLabel(db.title, false, measurer, size, 4000.0, Style::Bold);
    double titleHeight = title.height > 0.0 ? (title.height + 12.0) : 0.0;
    double inner = std::max(layout.width, title.width);
    double ox = pad + (inner - layout.width) / 2.0;
    double oy = pad + titleHeight;
    Flowchart::shiftLayout(cnodes, cedges, ox, oy);
    double width = std::ceil(inner + 2.0 * pad);
    double height = std::ceil(layout.height + titleHeight + 2.0 * pad);
    std::vector<LinkInfo> links;

    std::vector<EdgePath> heldPaths;
    std::vector<double> heldLabels;
    std::vector<std::vector<double>> heldRoutes;
    bool careful = (static_cast<double>(held.size()) * static_cast<double>(cnodes.size()) < 2e6);
    for (const auto& hItem : held) {
        const auto& edge = hItem.edge;
        int src = hItem.src;
        int dst = hItem.dst;
        bool out = contains(dst, src);
        int outer = out ? dst : src;
        int i = out ? src : dst;
        const auto& g = cnodes[static_cast<size_t>(outer)];
        const auto& n = cnodes[static_cast<size_t>(i)];
        int end = flowEndSide(!g.dir.empty() ? g.dir : flowOf(outer));
        int first = out ? end : ((end + 2) & 3);
        auto toward = [&](int side) -> std::vector<double> {
            bool upright = ((side & 1) == 0);
            double sign = (side == 0 || side == 3) ? -1.0 : 1.0;
            double nx = upright ? n.x : (n.x + (sign * n.w) / 2.0);
            double ny = upright ? (n.y + (sign * n.h) / 2.0) : n.y;
            double bx = upright ? n.x : (g.x + (sign * g.w) / 2.0);
            double by = upright ? (g.y + (sign * g.h) / 2.0) : n.y;
            double axis = upright ? 0.0 : 1.0;
            return out ? std::vector<double>{nx, ny, axis, bx, by, axis}
                       : std::vector<double>{bx, by, axis, nx, ny, axis};
        };
        auto blocked = [&](const std::vector<double>& r) -> bool {
            for (size_t k = 0; k < cnodes.size(); k++) {
                const auto& c = cnodes[k];
                if (static_cast<int>(k) != i &&
                    !c.isGroup &&
                    c.x + c.w / 2.0 > std::min(r[0], r[3]) - 1.0 &&
                    c.x - c.w / 2.0 < std::max(r[0], r[3]) + 1.0 &&
                    c.y + c.h / 2.0 > std::min(r[1], r[4]) - 1.0 &&
                    c.y - c.h / 2.0 < std::max(r[1], r[4]) + 1.0) {
                    return true;
                }
            }
            return false;
        };
        std::vector<double> route = toward(first);
        if (careful && blocked(route)) {
            for (int side : {(first + 1) & 3, (first + 3) & 3, (first + 2) & 3}) {
                std::vector<double> other = toward(side);
                if (!blocked(other)) {
                    route = std::move(other);
                    break;
                }
            }
        }
        heldRoutes.push_back(route);
        heldLabels.push_back((route[0] + route[3]) / 2.0);
        heldLabels.push_back((route[1] + route[4]) / 2.0);
        std::vector<double> drawnRoute = route;
        const auto& vi = views[static_cast<size_t>(i)];
        Flowchart::insetRoute(drawnRoute, out ? 0 : 3, vi, n);
        heldPaths.push_back(routePath(drawnRoute, "linear", 0.0, markerTrim(edge.arrowTypeEnd)));
    }

    Flowchart::Crossings crossings;
    bool anyGroup = false;
    for (const auto& c : cnodes) if (c.isGroup) anyGroup = true;
    if (anyGroup) {
        for (const auto& e : cedges) Flowchart::markCrossings(crossings, e.route);
        for (const auto& route : heldRoutes) Flowchart::markCrossings(crossings, route);
    }

    const auto& ksMap = kindSlotMap();
    std::string clusters;
    std::string nodesOut;
    std::vector<Fill> fills;
    for (size_t i = 0; i < views.size(); i++) {
        const auto& view = views[i];
        const auto& c = cnodes[i];
        const auto& node = view.node;
        double x = c.x;
        double y = c.y;
        std::string rawClasses = node.cssClasses;
        if (startsWith(rawClasses, "default ")) rawClasses = rawClasses.substr(8);
        else if (rawClasses == "default") rawClasses.clear();
        std::string classes = classNames(rawClasses);
        std::string id = escText(node.id);
        if (c.isGroup) {
            if (paints(view.style.shape)) {
                fills.push_back({{x - c.w / 2.0, y - c.h / 2.0, x + c.w / 2.0, y + c.h / 2.0}, view.style.shape});
            }
            double titleX = Flowchart::clusterTitleX(c, view.label.width, crossings, GROUP_PAD);
            int slot = node.hasColorIndex ? node.colorIndex : KIND_COUNT;
            clusters +=
                "<g class=\"pele-cluster pele-flow" + classes + "\" data-id=\"" + id + "\">" +
                "<rect x=\"" + afNum(x - c.w / 2.0) + "\" y=\"" + afNum(y - c.h / 2.0) + "\" width=\"" + afNum(c.w) + "\" height=\"" + afNum(c.h) + "\"" +
                rounded(tint(slot, "0.06", view.style) + view.style.shape) + "/>" +
                labelSvg(view.label, titleX, y - c.h / 2.0 + 8.0 + view.label.height / 2.0, " class=\"pele-cluster-label\"" + inkFill(view.style, "var(--_m)") + view.style.text, icons) +
                "</g>";
            continue;
        }

        double w = view.w;
        double h = view.h;
        std::string kindAttr;
        std::string shapeSvg;
        if (view.shape == "collapsed") {
            int slot = node.hasColorIndex ? node.colorIndex : KIND_COUNT;
            std::string color = seriesColor(slot);
            double lineY = h / 2.0 - DOTS - 2.0;
            double dotsY = lineY + (DOTS + 2.0) / 2.0;
            shapeSvg =
                "<rect x=\"" + afNum(-w / 2.0) + "\" y=\"" + afNum(-h / 2.0) + "\" width=\"" + afNum(w) + "\" height=\"" + afNum(h) + "\"" +
                rounded(tint(slot, "0.06", view.style) + view.style.shape) + "/>" +
                "<path d=\"M" + afNum(-w / 2.0 + 10.0) + "," + afNum(lineY) + "H" + afNum(w / 2.0 - 10.0) + "\" fill=\"none\" stroke=\"" + color + "\" stroke-dasharray=\"3 3\"" + view.style.line + "/>" +
                "<path class=\"pele-collapsed-mark\" d=\"" + dotPath(-9.0, dotsY) + dotPath(0.0, dotsY) + dotPath(9.0, dotsY) + "\" fill=\"" + color + "\" stroke=\"none\"/>";
        } else {
            std::string kName = node.hasKind ? node.kind : "task";
            auto kIt = ksMap.find(kName);
            int slot = (kIt != ksMap.end()) ? kIt->second : 1;
            kindAttr = " pele-kind-" + kName;
            shapeSvg = drawAgentShape(
                view.shape,
                w,
                h,
                tint(slot, "0.14", view.style) + view.style.shape,
                " stroke=\"" + seriesColor(slot) + "\"" + view.style.line
            );
        }

        std::string ink = (view.shape == "collapsed" || shapeFilled(view.shape)) ? inkFill(view.style) : "";
        std::string text = labelSvg(view.label, 0.0, view.dy, " class=\"pele-label\"" + ink + view.style.text, icons);
        bool isInternal = ((classes + " ").find(" internal-link ") != std::string::npos);
        if (isInternal && !node.label.empty() && options.links) {
            std::string name = decodeEntities(node.label);
            bool safe = Flowchart::internalLink(name, options);
            std::string target = esc(name);
            text = "<a class=\"internal-link\"" + (safe ? (" href=\"" + target + "\"" + relAttr(options)) : "") + " data-href=\"" + target + "\">" + text + "</a>";
            links.push_back({node.id, name, true});
        }
        std::string body = shapeSvg + text;
        if (!node.tooltip.empty()) body = "<title>" + escText(node.tooltip) + "</title>" + body;
        if (!node.link.empty()) {
            body = linked(body, node.link, node.id, links, options, !node.linkTarget.empty() ? (" target=\"" + esc(node.linkTarget) + "\"") : "");
        }
        nodesOut += "<g class=\"pele-node pele-shape-" + view.shape + kindAttr + classes + "\" data-id=\"" + id + "\" transform=\"translate(" + afNum(x) + "," + afNum(y) + ")\">" + body + "</g>";
    }

    std::string casings;
    std::string edgesOut;
    std::string labelsOut;
    auto put = [&](const GraphEdge& edge, const Label& label, const EdgePath& path, double x, double y) {
        bool hasStyle = !edge.style.empty() || !edge.cssCompiledStyles.empty();
        ResolvedStyle style;
        if (hasStyle) {
            std::vector<std::string> combined = edge.cssCompiledStyles;
            combined.insert(combined.end(), edge.style.begin(), edge.style.end());
            style = resolveStyle(combined);
        }
        const std::string& endType = edge.arrowTypeEnd;
        OverFillResult over = overFills(path.d, {path.ex, path.ey}, fills, hasStyle ? style.line : "", path.hasTokens ? &path.tokens : nullptr);
        std::string color = !over.value.empty() ? over.value : ((hasStyle && style.hasStroke) ? esc(style.stroke) : "var(--_l)");
        std::string kind = (edge.thickness == "dotted") ? "reference" : ((endType == "arrow_cross") ? "failure" : "sequence");
        std::string rawClasses = edge.classes;
        rawClasses = replaceAllStr(rawClasses, "edge-thickness-normal", "");
        rawClasses = replaceAllStr(rawClasses, "edge-pattern-solid", "");
        rawClasses = replaceAllStr(rawClasses, "flowchart-link", "");
        std::string classes = classNames(rawClasses);
        if (over.onFill && over.value.empty()) {
            casings += edgeCasing(path, "none", endType, hasStyle ? style.line : "");
        }
        edgesOut +=
            "<g class=\"pele-edge pele-edge-" + kind + classes + "\" data-id=\"" + escText(edge.id) + "\"" +
            (!over.value.empty() ? (" stroke=\"" + over.value + "\"") : "") +
            (hasStyle ? style.line : "") + ">" +
            "<path d=\"" + path.d + "\"" + (edge.thickness == "dotted" ? " stroke-dasharray=\"3 4\"" : "") + "/>" +
            marker(endType, path.ex, path.ey, path.edx, path.edy, color) +
            "</g>";
        if (label.width > 0.0) {
            labelsOut += edgeLabelSvg(edge.id, label, x, y, hasStyle ? style.text : "", icons);
        }
    };

    std::unordered_map<int, int> loopSeen;
    for (size_t i = 0; i < cedges.size(); i++) {
        auto& ce = cedges[i];
        const auto& edge = drawn[i];
        const auto& label = edgeLabels[i];
        const std::string& endType = edge.arrowTypeEnd;
        EdgePath path;
        if (ce.src == ce.dst) {
            const auto& view = views[static_cast<size_t>(ce.src)];
            int k = loopSeen[ce.src]++;
            path = Flowchart::loopPath(cnodes[static_cast<size_t>(ce.src)], view.w, view.h, k, flowsSideways(ce.src), LOOP, markerTrim(endType), ce);
        } else {
            std::vector<double> route = ce.route;
            const auto& vSrc = views[static_cast<size_t>(ce.src)];
            const auto& vDst = views[static_cast<size_t>(ce.dst)];
            Flowchart::insetRoute(route, 0, vSrc, cnodes[static_cast<size_t>(ce.src)]);
            Flowchart::insetRoute(route, route.size() - 3, vDst, cnodes[static_cast<size_t>(ce.dst)]);
            path = routePath(route, edge.curve, 0.0, markerTrim(endType));
        }
        put(edge, label, path, ce.labelX, ce.labelY);
    }
    for (size_t i = 0; i < held.size(); i++) {
        put(held[i].edge, held[i].label, heldPaths[i], heldLabels[2 * i], heldLabels[2 * i + 1]);
    }

    std::string svg = svgDocument(
        "agentflow",
        width,
        height,
        size,
        options,
        db,
        labelSvg(title, width / 2.0, pad + title.height / 2.0, " class=\"pele-title\" font-weight=\"var(--_tw)\"") +
            (!clusters.empty() ? ("<g class=\"pele-clusters\">" + clusters + "</g>") : "") +
            casingsSvg(casings) +
            (!edgesOut.empty() ? ("<g class=\"pele-edges\" fill=\"none\" stroke=\"var(--_l)\" stroke-linecap=\"round\">" + edgesOut + "</g>") : "") +
            (!labelsOut.empty() ? ("<g class=\"pele-edge-labels\" font-size=\"" + afNum(edgeSize) + "\">" + labelsOut + "</g>") : "") +
            "<g class=\"pele-nodes\">" + nodesOut + "</g>"
    );

    return {std::move(svg), width, height, std::move(links)};
}

inline Rendered renderAgentflowDiagram(AgentflowDb& db, const Config& config, const RenderOptions& options) {
    std::string curve;
    if (config.isMap()) {
        if (const auto* fc = config.get("flowchart"); fc && fc->isMap()) {
            if (const auto* c = fc->get("curve"); c && c->isString()) curve = c->sVal;
        }
    }
    AgentGraph graph = buildAgentGraph(db, PALETTE, curve);
    if (!Flowchart::runsAcross(db.direction)) return drawAgentflow(db, graph, config, options, false);
    return Flowchart::turnToFit(options, [&](bool down) {
        return drawAgentflow(db, graph, config, options, down);
    });
}

} // namespace Agentflow

inline Rendered renderAgentflow(const std::string& source, const RenderOptions& options = {}) {
    Preprocessed pre = preprocess(source);
    std::string body = encodeEntities(pre.withComments) + "\n";
    Agentflow::AgentflowDbOptions dbOpts;
    dbOpts.maxEdges = 5000;
    dbOpts.hasMaxEdges = true;
    dbOpts.lineOffset = pre.lineOffset;
    dbOpts.source = body;
    dbOpts.hasSource = true;
    if (pre.config.isMap()) {
        if (const auto* fc = pre.config.get("flowchart"); fc && fc->isMap()) {
            if (const auto* inh = fc->get("inheritDir"); inh && inh->isBool() && inh->bVal) {
                dbOpts.inheritDir = true;
            }
        }
        if (const auto* d = pre.config.get("direction"); d && d->isString()) {
            dbOpts.direction = d->sVal;
        }
    }
    Agentflow::AgentflowDb db(std::move(dbOpts));
    if (pre.hasTitle && !pre.title.empty()) db.title = pre.title;
    Agentflow::parseAgentflow(body, db);
    return Agentflow::renderAgentflowDiagram(db, pre.config, options);
}

} // namespace Pele
