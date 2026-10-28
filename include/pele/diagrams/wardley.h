#pragma once
#include "../foundation.h"

namespace Pele {
namespace WardleyDiagram {

// ============================================================================
// src/diagrams/wardley/builder.ts
// ============================================================================

struct WardleyNode {
    std::string id;
    std::string label;
    double x = 0.0;
    bool hasX = false;
    double y = 0.0;
    bool hasY = false;
    std::string className;
    double labelOffsetX = 0.0;
    bool hasLabelOffsetX = false;
    double labelOffsetY = 0.0;
    bool hasLabelOffsetY = false;
    bool inPipeline = false;
    bool isPipelineParent = false;
    bool inertia = false;
    std::string sourceStrategy; // "", "build", "buy", "outsource", "market"
};

struct WardleyLink {
    std::string source;
    std::string target;
    bool dashed = false;
    std::string label;
    bool hasLabel = false;
    std::string flow; // "", "forward", "backward", "bidirectional"
};

struct WardleyTrend {
    std::string nodeId;
    double targetX = 0.0;
    double targetY = 0.0;
};

struct WardleyPipeline {
    std::string nodeId;
    std::vector<std::string> componentIds;
};

struct WardleyCoord {
    double x = 0.0;
    double y = 0.0;
};

struct WardleyAnnotation {
    int number = 0;
    std::vector<WardleyCoord> coordinates;
    std::string text;
    bool hasText = false;
};

struct WardleyNote {
    std::string text;
    double x = 0.0;
    double y = 0.0;
};

struct WardleyForce {
    std::string name;
    double x = 0.0;
    double y = 0.0;
};

struct WardleyAxesConfig {
    std::string xLabel;
    bool hasXLabel = false;
    std::string yLabel;
    bool hasYLabel = false;
    std::vector<std::string> stages;
    std::vector<double> stageBoundaries;
    bool hasStageBoundaries = false;
};

struct WardleyModel : public DiagramMeta {
    std::vector<WardleyNode> nodes;
    std::vector<WardleyLink> links;
    std::vector<WardleyTrend> trends;
    std::vector<WardleyPipeline> pipelines;
    std::vector<WardleyAnnotation> annotations;
    std::vector<WardleyNote> notes;
    std::vector<WardleyForce> accelerators;
    std::vector<WardleyForce> deaccelerators;
    WardleyCoord annotationsBox;
    bool hasAnnotationsBox = false;
    WardleyAxesConfig axes;
    double sizeWidth = 0.0;
    double sizeHeight = 0.0;
    bool hasSize = false;
};

class WardleyBuilder {
public:
    std::vector<std::string> nodeOrder;
    std::unordered_map<std::string, WardleyNode> nodes;
    std::unordered_map<std::string, std::string> labels;
    bool hasLabelsCache = false;
    std::vector<WardleyLink> links;
    std::vector<std::string> trendOrder;
    std::unordered_map<std::string, WardleyTrend> trends;
    std::vector<std::string> pipelineOrder;
    std::unordered_map<std::string, WardleyPipeline> pipelines;
    std::vector<WardleyAnnotation> annotations;
    std::vector<WardleyNote> notes;
    std::vector<WardleyForce> accelerators;
    std::vector<WardleyForce> deaccelerators;
    WardleyCoord annotationsBox;
    bool hasAnnotationsBox = false;
    WardleyAxesConfig axes;
    double sizeWidth = 0.0;
    double sizeHeight = 0.0;
    bool hasSize = false;

    void addNode(const WardleyNode& node) {
        auto it = nodes.find(node.id);
        if (it == nodes.end()) {
            nodeOrder.push_back(node.id);
            nodes[node.id] = node;
        } else {
            WardleyNode& existing = it->second;
            existing.label = node.label;
            if (node.hasX) { existing.x = node.x; existing.hasX = true; }
            if (node.hasY) { existing.y = node.y; existing.hasY = true; }
            if (!node.className.empty()) existing.className = node.className;
            if (node.hasLabelOffsetX) { existing.labelOffsetX = node.labelOffsetX; existing.hasLabelOffsetX = true; }
            if (node.hasLabelOffsetY) { existing.labelOffsetY = node.labelOffsetY; existing.hasLabelOffsetY = true; }
            if (node.inPipeline) existing.inPipeline = true;
            if (node.isPipelineParent) existing.isPipelineParent = true;
            existing.inertia = node.inertia;
            existing.sourceStrategy = node.sourceStrategy;
        }
        hasLabelsCache = false;
    }

    void addLink(const WardleyLink& link) {
        links.push_back(link);
    }

    void addTrend(const WardleyTrend& trend) {
        if (!trends.count(trend.nodeId)) trendOrder.push_back(trend.nodeId);
        trends[trend.nodeId] = trend;
    }

    void startPipeline(const std::string& nodeId) {
        if (!pipelines.count(nodeId)) pipelineOrder.push_back(nodeId);
        pipelines[nodeId] = {nodeId, {}};
        auto it = nodes.find(nodeId);
        if (it != nodes.end()) it->second.isPipelineParent = true;
    }

    void addPipelineComponent(const std::string& pipelineNodeId, const std::string& componentId) {
        auto pIt = pipelines.find(pipelineNodeId);
        if (pIt != pipelines.end()) pIt->second.componentIds.push_back(componentId);
        auto nIt = nodes.find(componentId);
        if (nIt != nodes.end()) nIt->second.inPipeline = true;
    }

    void addAnnotation(const WardleyAnnotation& annotation) {
        annotations.push_back(annotation);
    }

    void addNote(const WardleyNote& note) {
        notes.push_back(note);
    }

    void addAccelerator(const WardleyForce& accelerator) {
        accelerators.push_back(accelerator);
    }

    void addDeaccelerator(const WardleyForce& deaccelerator) {
        deaccelerators.push_back(deaccelerator);
    }

    void setAnnotationsBox(double x, double y) {
        annotationsBox = {x, y};
        hasAnnotationsBox = true;
    }

    void setAxes(const std::vector<std::string>& stages, const std::vector<double>& stageBoundaries) {
        axes.stages = stages;
        axes.stageBoundaries = stageBoundaries;
        axes.hasStageBoundaries = true;
    }

    void setSize(double width, double height) {
        sizeWidth = width;
        sizeHeight = height;
        hasSize = true;
    }

    const WardleyNode* getNode(const std::string& id) const {
        auto it = nodes.find(id);
        return it != nodes.end() ? &it->second : nullptr;
    }

    std::string resolveNodeId(const std::string& name) {
        if (nodes.count(name)) return name;
        if (!hasLabelsCache) {
            labels.clear();
            for (const auto& id : nodeOrder) {
                const auto& node = nodes[id];
                if (!labels.count(node.label)) labels[node.label] = id;
            }
            hasLabelsCache = true;
        }
        auto it = labels.find(name);
        return it != labels.end() ? it->second : name;
    }

    void buildInto(WardleyModel& out) const {
        for (const auto& id : nodeOrder) {
            const auto& node = nodes.at(id);
            if (!node.hasX || !node.hasY) {
                throw PeleError("Node \"" + node.label + "\" is missing coordinates", "semantic", "wardley");
            }
            out.nodes.push_back(node);
        }
        out.links = links;
        for (const auto& id : trendOrder) out.trends.push_back(trends.at(id));
        for (const auto& id : pipelineOrder) out.pipelines.push_back(pipelines.at(id));
        out.annotations = annotations;
        out.notes = notes;
        out.accelerators = accelerators;
        out.deaccelerators = deaccelerators;
        out.annotationsBox = annotationsBox;
        out.hasAnnotationsBox = hasAnnotationsBox;
        out.axes = axes;
        out.sizeWidth = sizeWidth;
        out.sizeHeight = sizeHeight;
        out.hasSize = hasSize;
    }
};

// ============================================================================
// src/diagrams/wardley/parser.ts
// ============================================================================

namespace T {
    enum Type {
        lbracket,
        comma,
        rbracket,
        at,
        slash,
        lparen,
        rparen,
        minus,
        lbrace,
        rbrace,
        number,
        arrow,
        linkPort,
        linkArrow,
        linkLabel,
        strategy,
        wardley,
        size,
        evolution,
        anchor,
        component,
        label,
        inertia,
        evolve,
        pipeline,
        note,
        annotations,
        annotation,
        accelerator,
        deaccelerator,
        name,
        ws,
        accDescr,
        accTitle,
        title,
        intTok,
        string,
        id,
        newline,
        whitespace,
        yaml,
        directive,
        singleLineComment
    };
}

inline int scanDigits(const std::string& src, int from) {
    int i = from;
    while (i < static_cast<int>(src.size()) && isDigit(charCodeAt(src, i))) i++;
    return i;
}

inline bool isWardleyNameCont(const std::string& src, int p) {
    int c = charCodeAt(src, p);
    if (isWord(c) || c == '(' || c == ')' || c == '&') return true;
    if (c == '-' && charCodeAt(src, p + 1) != '>') return true;
    return false;
}

inline const std::vector<TokenType>& wardleyTokens() {
    static const std::vector<TokenType> types = []() {
        auto lit = [](const std::string& name, const std::string& pat) {
            TokenType t;
            t.name = name;
            t.literal = pat;
            return t;
        };
        auto word = [](const std::string& name, const std::string& text) {
            TokenType t;
            t.name = name;
            t.literal = text;
            return t;
        };

        TokenType minusTok = lit("-", "-");
        minusTok.longer = {T::arrow, T::linkArrow, T::yaml};

        TokenType numTok;
        numTok.name = "WARDLEY_NUMBER";
        numTok.isRegex = true;
        numTok.first = "0123456789";
        numTok.scanner = []() -> TokenScanner {
            int from = 0;
            int end = 0;
            return [from, end](const std::string& src, int p, int) mutable -> int {
                if (p < from || p >= end) {
                    from = p;
                    end = scanDigits(src, p);
                }
                if (end == p || charCodeAt(src, end) != 46) return -1;
                int stop = scanDigits(src, end + 1);
                return stop > end + 1 ? stop : -1;
            };
        };

        TokenType arrowTok = lit("ARROW", "->");

        TokenType linkPortTok;
        linkPortTok.name = "LINK_PORT";
        linkPortTok.isRegex = true;
        linkPortTok.first = "+";
        linkPortTok.match = [](const std::string& src, int at, int) -> int {
            if (src.compare(at, 3, "+<>") == 0) return at + 3;
            if (src.compare(at, 2, "+>") == 0 || src.compare(at, 2, "+<") == 0) return at + 2;
            return -1;
        };

        TokenType linkArrowTok;
        linkArrowTok.name = "LINK_ARROW";
        linkArrowTok.isRegex = true;
        linkArrowTok.first = "->+";
        linkArrowTok.match = [](const std::string& src, int at, int) -> int {
            if (src.compare(at, 3, "-->") == 0) return at + 3;
            if (src.compare(at, 4, "-.->") == 0) return at + 4;
            if (charCodeAt(src, at) == '>') return at + 1;
            if (src.compare(at, 2, "+'") == 0) {
                size_t close = src.find('\'', at + 2);
                if (close != std::string::npos) {
                    if (src.compare(close + 1, 2, "<>") == 0) return static_cast<int>(close + 3);
                    int next = charCodeAt(src, static_cast<int>(close + 1));
                    if (next == '<' || next == '>') return static_cast<int>(close + 2);
                }
            }
            return -1;
        };

        TokenType linkLabelTok;
        linkLabelTok.name = "LINK_LABEL";
        linkLabelTok.isRegex = true;
        linkLabelTok.first = ";";
        linkLabelTok.match = [](const std::string& src, int at, int) -> int {
            if (charCodeAt(src, at) != ';') return -1;
            int p = at + 1;
            int n = static_cast<int>(src.size());
            while (p < n && src[p] != '\n' && src[p] != '\r') p++;
            return p > at + 1 ? p : -1;
        };

        TokenType strategyTok;
        strategyTok.name = "STRATEGY";
        strategyTok.isRegex = true;
        strategyTok.first = "bom";
        strategyTok.match = [](const std::string& src, int at, int) -> int {
            if (src.compare(at, 5, "build") == 0) return at + 5;
            if (src.compare(at, 3, "buy") == 0) return at + 3;
            if (src.compare(at, 9, "outsource") == 0) return at + 9;
            if (src.compare(at, 6, "market") == 0) return at + 6;
            return -1;
        };

        TokenType nameWithSpacesTok;
        nameWithSpacesTok.name = "NAME_WITH_SPACES";
        nameWithSpacesTok.isRegex = true;
        nameWithSpacesTok.first = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
        nameWithSpacesTok.match = [](const std::string& src, int at, int) -> int {
            if (!isLetter(charCodeAt(src, at))) return -1;
            if (src.compare(at, 5, "title") == 0 && isSpace(charCodeAt(src, at + 5))) return -1;
            if (src.compare(at, 8, "accTitle") == 0 || src.compare(at, 8, "accDescr") == 0) return -1;
            int n = static_cast<int>(src.size());
            int p = at + 1;
            while (p < n && isWardleyNameCont(src, p)) p++;
            for (;;) {
                int saved = p;
                while (p < n && (src[p] == ' ' || src[p] == '\t')) p++;
                if (p == saved) break;
                int c = charCodeAt(src, p);
                if (!isLetter(c) && c != '(') {
                    p = saved;
                    break;
                }
                p++;
                while (p < n && isWardleyNameCont(src, p)) p++;
            }
            return p;
        };

        TokenType wsTok = common_tokens::makeWhitespace();
        wsTok.name = "WS";

        TokenType accDescrTok = common_tokens::makeAccDescr();
        accDescrTok.scanner = []() -> TokenScanner {
            bool unclosed = false;
            return [unclosed](const std::string& src, int at, int) mutable -> int {
                int p = common_tokens::matchTabsSpaces(src, at);
                if (src.compare(p, 8, "accDescr") != 0) return -1;
                p += 8;
                int q = common_tokens::matchTabsSpaces(src, p);
                if (q < static_cast<int>(src.size()) && src[q] == ':') {
                    return common_tokens::matchRestOfLineBeforeComment(src, q + 1);
                }
                q = p;
                while (q < static_cast<int>(src.size()) && isSpace(charCodeAt(src, q))) q++;
                if (q < static_cast<int>(src.size()) && src[q] == '{') {
                    if (unclosed) return -1;
                    size_t close = src.find('}', q + 1);
                    if (close == std::string::npos) {
                        unclosed = true;
                        return -1;
                    }
                    return static_cast<int>(close + 1);
                }
                return -1;
            };
        };

        TokenType intTok;
        intTok.name = "INT";
        intTok.isRegex = true;
        intTok.first = "0123456789";
        intTok.match = [](const std::string& src, int at, int) -> int {
            int c = charCodeAt(src, at);
            if (c == '0') return at + 1;
            if (c >= '1' && c <= '9') {
                int p = scanDigits(src, at + 1);
                return charCodeAt(src, p) != '.' ? p : -1;
            }
            return -1;
        };

        TokenType idTok;
        idTok.name = "ID";
        idTok.isRegex = true;
        idTok.first = "_0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
        idTok.match = [](const std::string& src, int at, int) -> int {
            if (!isWord(charCodeAt(src, at))) return -1;
            int p = at + 1;
            int n = static_cast<int>(src.size());
            while (p < n && (isWord(charCodeAt(src, p)) || src[p] == '-')) p++;
            while (p > at + 1 && src[p - 1] == '-') p--;
            return p;
        };

        return std::vector<TokenType>{
            lit("[", "["),
            lit(",", ","),
            lit("]", "]"),
            lit("@", "@"),
            lit("/", "/"),
            lit("(", "("),
            lit(")", ")"),
            minusTok,
            lit("{", "{"),
            lit("}", "}"),
            numTok,
            arrowTok,
            linkPortTok,
            linkArrowTok,
            linkLabelTok,
            strategyTok,
            word("KW_WARDLEY", "wardley-beta"),
            word("KW_SIZE", "size"),
            word("KW_EVOLUTION", "evolution"),
            word("KW_ANCHOR", "anchor"),
            word("KW_COMPONENT", "component"),
            word("KW_LABEL", "label"),
            word("KW_INERTIA", "inertia"),
            word("KW_EVOLVE", "evolve"),
            word("KW_PIPELINE", "pipeline"),
            word("KW_NOTE", "note"),
            word("KW_ANNOTATIONS", "annotations"),
            word("KW_ANNOTATION", "annotation"),
            word("KW_ACCELERATOR", "accelerator"),
            word("KW_DEACCELERATOR", "deaccelerator"),
            nameWithSpacesTok,
            wsTok,
            accDescrTok,
            common_tokens::makeAccTitle(),
            common_tokens::makeTitle(),
            intTok,
            common_tokens::makeString(),
            idTok,
            common_tokens::makeNewline(),
            common_tokens::makeWhitespace(),
            common_tokens::makeYaml(),
            common_tokens::makeDirective(),
            common_tokens::makeSingleLineComment()
        };
    }();
    return types;
}

struct LabelAst {
    bool negX = false;
    int offsetX = 0;
    bool negY = false;
    int offsetY = 0;
    bool present = false;
};

struct StageAst {
    std::string name;
    double boundary = 0.0;
    bool hasBoundary = false;
    std::string secondName;
    bool hasSecondName = false;
};

struct ComponentAst {
    std::string name;
    double visibility = 0.0;
    double evolution = 0.0;
    LabelAst label;
    std::string strategy;
    bool hasStrategy = false;
    bool inertia = false;
};

struct PipelineComponentAst {
    std::string name;
    double evolution = 0.0;
    LabelAst label;
};

struct LinkAst {
    std::string from;
    std::string fromPort;
    std::string arrow;
    std::string to;
    std::string toPort;
    std::string linkLabel;
    bool hasLinkLabel = false;
};

struct AnchorAst {
    std::string name;
    double visibility = 0.0;
    double evolution = 0.0;
};

struct EvolveAst {
    std::string component;
    double target = 0.0;
};

struct PipelineAst {
    std::string parent;
    std::vector<PipelineComponentAst> components;
};

struct NoteAst {
    std::string text;
    double visibility = 0.0;
    double evolution = 0.0;
};

struct AnnotationsAst {
    double x = 0.0;
    double y = 0.0;
};

struct AnnotationAst {
    int number = 0;
    double x = 0.0;
    double y = 0.0;
    std::string text;
};

struct ForceAst {
    std::string name;
    double x = 0.0;
    double y = 0.0;
};

struct WardleyAst {
    std::string title;
    bool hasTitle = false;
    std::string accTitle;
    bool hasAccTitle = false;
    std::string accDescr;
    bool hasAccDescr = false;
    int sizeWidth = 0;
    int sizeHeight = 0;
    bool hasSize = false;
    std::vector<StageAst> evolutionStages;
    bool hasEvolution = false;
    std::vector<AnchorAst> anchors;
    std::vector<ComponentAst> components;
    std::vector<LinkAst> links;
    std::vector<EvolveAst> evolves;
    std::vector<PipelineAst> pipelines;
    std::vector<NoteAst> notes;
    std::vector<AnnotationsAst> annotations;
    std::vector<AnnotationAst> annotation;
    std::vector<ForceAst> accelerators;
    std::vector<ForceAst> deaccelerators;
};

inline std::string wardleySingle(const std::string& text) {
    std::string s = trimStr(text);
    std::string out;
    size_t i = 0;
    while (i < s.size()) {
        if (s[i] == '\t' || s[i] == ' ') {
            size_t j = i + 1;
            while (j < s.size() && (s[j] == '\t' || s[j] == ' ')) j++;
            if (j - i >= 2) out.push_back(' ');
            else out.push_back(s[i]);
            i = j;
        } else {
            out.push_back(s[i++]);
        }
    }
    return out;
}

inline std::string wardleyTitleValue(const std::string& text) {
    size_t pos = text.find("title");
    if (pos == std::string::npos) return "";
    return wardleySingle(text.substr(pos + 5));
}

inline std::string wardleyAccTitleValue(const std::string& text) {
    size_t pos = text.find(':');
    if (pos == std::string::npos) return "";
    return wardleySingle(text.substr(pos + 1));
}

inline std::string wardleyAccDescrValue(const std::string& text) {
    size_t brace = text.find('{');
    size_t colon = text.find(':');
    if (colon != std::string::npos && (brace == std::string::npos || colon < brace)) {
        return wardleySingle(text.substr(colon + 1));
    }
    if (brace != std::string::npos) {
        size_t close = text.find('}', brace + 1);
        std::string body = text.substr(brace + 1, close == std::string::npos ? std::string::npos : close - brace - 1);
        return cleanAccDescr(body);
    }
    return "";
}

inline WardleyAst parseWardley(const std::string& src) {
    const auto& tokTypes = wardleyTokens();
    Tokens toks = tokenize(src, tokTypes, "wardley");
    TokenReader r(toks, tokTypes, "wardley");
    WardleyAst ast;

    auto at = [&](int k) -> bool { return r.tokens.kinds[r.i] == k; };
    auto isName = [&]() -> bool { return at(T::string) || at(T::id) || at(T::name); };
    auto name = [&]() -> std::string {
        if (at(T::string)) return common_tokens::stringValue(r.take());
        if (at(T::id) || at(T::name)) return r.take();
        r.fail("a name");
    };
    auto number = [&]() -> double { return std::stod(r.expect(T::number)); };
    auto intVal = [&]() -> int { return std::stoi(r.expect(T::intTok)); };
    auto coordinate = [&]() -> double {
        return at(T::number) ? std::stod(r.take()) : static_cast<double>(std::stoi(r.expect(T::intTok)));
    };
    auto endOfLine = [&]() {
        if (at(-1)) return;
        if (!at(T::newline)) r.fail("a line break or the end of input");
        while (at(T::newline)) r.i++;
    };
    auto pair = [&](auto&& readFn) -> std::pair<decltype(readFn()), decltype(readFn())> {
        r.expect(T::lbracket);
        auto first = readFn();
        r.expect(T::comma);
        auto second = readFn();
        r.expect(T::rbracket);
        return {first, second};
    };
    auto label = [&]() -> LabelAst {
        r.expect(T::label);
        r.expect(T::lbracket);
        bool negX = r.accept(T::minus);
        int offsetX = intVal();
        r.expect(T::comma);
        bool negY = r.accept(T::minus);
        int offsetY = intVal();
        r.expect(T::rbracket);
        return {negX, offsetX, negY, offsetY, true};
    };
    auto stage = [&]() -> StageAst {
        StageAst out;
        out.name = name();
        if (r.accept(T::at)) {
            out.boundary = number();
            out.hasBoundary = true;
        }
        if (r.accept(T::slash)) {
            out.secondName = name();
            out.hasSecondName = true;
        }
        return out;
    };

    while (at(T::newline)) r.i++;
    r.expect(T::wardley);
    while (!at(-1)) {
        switch (r.kind()) {
            case T::newline:
                r.i++;
                break;
            case T::accDescr:
                ast.accDescr = wardleyAccDescrValue(r.take());
                ast.hasAccDescr = true;
                endOfLine();
                break;
            case T::accTitle:
                ast.accTitle = wardleyAccTitleValue(r.take());
                ast.hasAccTitle = true;
                endOfLine();
                break;
            case T::title:
                ast.title = wardleyTitleValue(r.take());
                ast.hasTitle = true;
                endOfLine();
                break;
            case T::size: {
                r.i++;
                auto [w, h] = pair(intVal);
                endOfLine();
                ast.sizeWidth = w;
                ast.sizeHeight = h;
                ast.hasSize = true;
                break;
            }
            case T::evolution: {
                r.i++;
                std::vector<StageAst> stages = {stage()};
                r.expect(T::arrow);
                stages.push_back(stage());
                while (r.accept(T::arrow)) stages.push_back(stage());
                endOfLine();
                ast.evolutionStages = std::move(stages);
                ast.hasEvolution = true;
                break;
            }
            case T::anchor: {
                r.i++;
                std::string anchorName = name();
                auto [vis, evo] = pair(number);
                endOfLine();
                ast.anchors.push_back({anchorName, vis, evo});
                break;
            }
            case T::component: {
                r.i++;
                ComponentAst comp;
                comp.name = name();
                auto [vis, evo] = pair(number);
                comp.visibility = vis;
                comp.evolution = evo;
                if (at(T::label)) comp.label = label();
                if (at(T::lparen) && r.tokens.kinds[r.i + 1] == T::strategy) {
                    r.i++;
                    comp.strategy = r.take();
                    comp.hasStrategy = true;
                    r.expect(T::rparen);
                }
                if (r.accept(T::inertia)) {
                    comp.inertia = true;
                } else if (r.accept(T::lparen)) {
                    r.expect(T::inertia);
                    r.expect(T::rparen);
                    comp.inertia = true;
                }
                endOfLine();
                ast.components.push_back(std::move(comp));
                break;
            }
            case T::evolve: {
                r.i++;
                std::string comp = name();
                double target = number();
                endOfLine();
                ast.evolves.push_back({comp, target});
                break;
            }
            case T::pipeline: {
                r.i++;
                std::string parent = name();
                r.expect(T::lbrace);
                r.expect(T::newline);
                while (at(T::newline)) r.i++;
                std::vector<PipelineComponentAst> comps;
                do {
                    r.expect(T::component);
                    PipelineComponentAst pc;
                    pc.name = name();
                    r.expect(T::lbracket);
                    pc.evolution = number();
                    r.expect(T::rbracket);
                    if (at(T::label)) pc.label = label();
                    endOfLine();
                    comps.push_back(std::move(pc));
                } while (at(T::component));
                r.expect(T::rbrace);
                endOfLine();
                ast.pipelines.push_back({parent, std::move(comps)});
                break;
            }
            case T::note: {
                r.i++;
                std::string text = common_tokens::stringValue(r.expect(T::string));
                auto [vis, evo] = pair(number);
                endOfLine();
                ast.notes.push_back({text, vis, evo});
                break;
            }
            case T::annotations: {
                r.i++;
                auto [x, y] = pair(coordinate);
                endOfLine();
                ast.annotations.push_back({x, y});
                break;
            }
            case T::annotation: {
                r.i++;
                int n = intVal();
                r.expect(T::comma);
                auto [x, y] = pair(coordinate);
                std::string text = common_tokens::stringValue(r.expect(T::string));
                endOfLine();
                ast.annotation.push_back({n, x, y, text});
                break;
            }
            case T::accelerator:
            case T::deaccelerator: {
                bool slow = (r.kind() == T::deaccelerator);
                r.i++;
                std::string force = name();
                auto [x, y] = pair(number);
                endOfLine();
                if (slow) ast.deaccelerators.push_back({force, x, y});
                else ast.accelerators.push_back({force, x, y});
                break;
            }
            default: {
                if (!isName()) r.fail("a statement");
                LinkAst link;
                link.from = name();
                if (at(T::linkPort)) link.fromPort = r.take();
                if (at(T::linkArrow) || at(T::arrow)) link.arrow = r.take();
                link.to = name();
                if (at(T::linkPort)) link.toPort = r.take();
                if (at(T::linkLabel)) {
                    link.linkLabel = trimStr(r.take().substr(1));
                    link.hasLinkLabel = true;
                }
                endOfLine();
                ast.links.push_back(std::move(link));
                break;
            }
        }
    }
    return ast;
}

// ============================================================================
// src/diagrams/wardley/model.ts
// ============================================================================

inline double toPercent(double value, const std::string& context) {
    double normalized = value <= 1.0 ? (value * 100.0) : value;
    if (normalized < 0.0 || normalized > 100.0) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.15g", value);
        throw PeleError(context + " must be between 0-1 (decimal) or 0-100 (percentage). Received: " + buf, "semantic", "wardley");
    }
    return normalized;
}

inline WardleyCoord toCoordinates(double visibility, double evolution, const std::string& context) {
    return {toPercent(evolution, context + " evolution"), toPercent(visibility, context + " visibility")};
}

inline std::string flowFromPort(const std::string& port) {
    if (port == "+<>") return "bidirectional";
    if (port == "+<") return "backward";
    if (port == "+>") return "forward";
    return "";
}

struct ArrowFlowInfo {
    std::string flow;
    std::string label;
    bool hasLabel = false;
};

inline ArrowFlowInfo flowFromArrow(const std::string& arrow) {
    if (arrow.empty() || arrow[0] != '+') return {};
    ArrowFlowInfo res;
    size_t end = arrow.find('\'', 2);
    if (arrow.size() > 1 && arrow[1] == '\'' && end != std::string::npos) {
        res.label = arrow.substr(2, end - 2);
        res.hasLabel = true;
    }
    if (arrow.find("<>") != std::string::npos) res.flow = "bidirectional";
    else if (arrow.find('<') != std::string::npos) res.flow = "backward";
    else if (arrow.find('>') != std::string::npos) res.flow = "forward";
    return res;
}

inline void populate(const WardleyAst& ast, WardleyBuilder& db) {
    if (ast.hasSize) db.setSize(ast.sizeWidth, ast.sizeHeight);

    if (ast.hasEvolution) {
        std::vector<std::string> stages;
        std::vector<double> boundaries;
        for (const auto& st : ast.evolutionStages) {
            stages.push_back(st.hasSecondName ? (trimStr(st.name) + " / " + trimStr(st.secondName)) : trimStr(st.name));
            if (st.hasBoundary) boundaries.push_back(st.boundary);
        }
        db.setAxes(stages, boundaries);
    }

    for (const auto& anchor : ast.anchors) {
        auto at = toCoordinates(anchor.visibility, anchor.evolution, "Anchor \"" + anchor.name + "\"");
        WardleyNode node;
        node.id = anchor.name;
        node.label = anchor.name;
        node.x = at.x;
        node.hasX = true;
        node.y = at.y;
        node.hasY = true;
        node.className = "anchor";
        db.addNode(node);
    }

    for (const auto& comp : ast.components) {
        auto at = toCoordinates(comp.visibility, comp.evolution, "Component \"" + comp.name + "\"");
        WardleyNode node;
        node.id = comp.name;
        node.label = comp.name;
        node.x = at.x;
        node.hasX = true;
        node.y = at.y;
        node.hasY = true;
        node.className = "component";
        if (comp.label.present) {
            node.labelOffsetX = (comp.label.negX ? -1.0 : 1.0) * comp.label.offsetX;
            node.hasLabelOffsetX = true;
            node.labelOffsetY = (comp.label.negY ? -1.0 : 1.0) * comp.label.offsetY;
            node.hasLabelOffsetY = true;
        }
        node.inertia = comp.inertia;
        if (comp.hasStrategy) node.sourceStrategy = comp.strategy;
        db.addNode(node);
    }

    for (const auto& note : ast.notes) {
        auto at = toCoordinates(note.visibility, note.evolution, "Note \"" + note.text + "\"");
        db.addNote({note.text, at.x, at.y});
    }

    for (const auto& pipeline : ast.pipelines) {
        const WardleyNode* parent = db.getNode(pipeline.parent);
        if (!parent || !parent->hasY) {
            throw PeleError("Pipeline \"" + pipeline.parent + "\" must reference an existing component with coordinates.", "semantic", "wardley");
        }
        double y = parent->y;
        db.startPipeline(pipeline.parent);
        for (const auto& comp : pipeline.components) {
            std::string id = pipeline.parent + "_" + comp.name;
            double x = toPercent(comp.evolution, "Pipeline component \"" + comp.name + "\" evolution");
            WardleyNode node;
            node.id = id;
            node.label = comp.name;
            node.x = x;
            node.hasX = true;
            node.y = y;
            node.hasY = true;
            node.className = "pipeline-component";
            if (comp.label.present) {
                node.labelOffsetX = (comp.label.negX ? -1.0 : 1.0) * comp.label.offsetX;
                node.hasLabelOffsetX = true;
                node.labelOffsetY = (comp.label.negY ? -1.0 : 1.0) * comp.label.offsetY;
                node.hasLabelOffsetY = true;
            }
            db.addNode(node);
            db.addPipelineComponent(pipeline.parent, id);
        }
    }

    for (const auto& link : ast.links) {
        bool dashed = !link.arrow.empty() && (link.arrow.find("-.->") != std::string::npos || link.arrow.find(".-.") != std::string::npos);
        auto fromArrow = flowFromArrow(link.arrow);
        std::string flow = flowFromPort(link.fromPort);
        if (flow.empty()) flow = flowFromPort(link.toPort);
        if (flow.empty()) flow = fromArrow.flow;
        WardleyLink wl;
        wl.source = db.resolveNodeId(link.from);
        wl.target = db.resolveNodeId(link.to);
        wl.dashed = dashed;
        if (fromArrow.hasLabel) {
            wl.label = fromArrow.label;
            wl.hasLabel = true;
        } else if (link.hasLinkLabel) {
            wl.label = link.linkLabel;
            wl.hasLabel = true;
        }
        wl.flow = flow;
        db.addLink(wl);
    }

    for (const auto& evolve : ast.evolves) {
        const WardleyNode* node = db.getNode(evolve.component);
        if (node && node->hasY) {
            db.addTrend({evolve.component, toPercent(evolve.target, "Evolve target for \"" + evolve.component + "\""), node->y});
        }
    }

    if (!ast.annotations.empty()) {
        auto at = toCoordinates(ast.annotations[0].x, ast.annotations[0].y, "Annotations box");
        db.setAnnotationsBox(at.x, at.y);
    }

    for (const auto& ann : ast.annotation) {
        auto at = toCoordinates(ann.x, ann.y, "Annotation " + std::to_string(ann.number));
        db.addAnnotation({ann.number, {{at.x, at.y}}, ann.text, !ann.text.empty()});
    }

    for (const auto& acc : ast.accelerators) {
        auto at = toCoordinates(acc.x, acc.y, "Accelerator \"" + acc.name + "\"");
        db.addAccelerator({acc.name, at.x, at.y});
    }

    for (const auto& deacc : ast.deaccelerators) {
        auto at = toCoordinates(deacc.x, deacc.y, "Deaccelerator \"" + deacc.name + "\"");
        db.addDeaccelerator({deacc.name, at.x, at.y});
    }
}

inline WardleyModel buildWardley(const WardleyAst& ast, const std::string& title) {
    WardleyBuilder db;
    populate(ast, db);
    WardleyModel model;
    model.title = !ast.title.empty() ? ast.title : title;
    if (ast.hasAccTitle) model.accTitle = trimStartStr(ast.accTitle);
    if (ast.hasAccDescr) {
        std::string out;
        size_t i = 0;
        const std::string& text = ast.accDescr;
        while (i < text.size()) {
            if (text[i] == '\n') {
                size_t j = i + 1;
                size_t afterSpace = j;
                while (afterSpace < text.size()) {
                    size_t next = afterSpace;
                    int cp = charCodeAtUtf8(text, afterSpace, &next);
                    if (!isSpace(cp)) break;
                    afterSpace = next;
                }
                if (afterSpace > j) {
                    out.push_back('\n');
                    i = afterSpace;
                    continue;
                }
            }
            out.push_back(text[i++]);
        }
        model.accDescr = out;
    }
    db.buildInto(model);
    return model;
}

// ============================================================================
// src/diagrams/wardley/render.ts
// ============================================================================

inline double wardleySetting(const Config& config, const std::string& key, double fallback, double minV, double maxV) {
    const auto* sec = config.get("wardley-beta");
    if (sec && sec->isMap()) {
        const auto* v = sec->get(key);
        if (v && v->isNumber() && v->nVal >= minV && v->nVal <= maxV) return v->nVal;
    }
    return fallback;
}

inline double wardleyClamp(double value, double minV, double maxV) {
    return value < minV ? minV : value > maxV ? maxV : value;
}

inline std::string startLabel(const Label& label, double x, double y, const std::string& attrs) {
    int shared = sharedStyle(label);
    std::string body;
    double baseline = y;
    for (const auto& line : label.lines) {
        bool first = true;
        for (const auto& span : line) {
            if (span.hasIcon) continue;
            std::string style = spanStyle(span.style & ~shared);
            body += "<tspan" + (first ? (" x=\"" + num(x) + "\" y=\"" + num(baseline) + "\"") : std::string("")) + style + ">" + esc(span.text) + "</tspan>";
            first = false;
        }
        baseline += label.lineHeight;
    }
    return body.empty() ? "" : ("<text" + textStyle(attrs, shared) + " xml:space=\"preserve\">" + body + "</text>");
}

inline double wardleyDimension(const Config& config, double given, bool hasGiven, const std::string& key, double fallback) {
    constexpr double CANVAS_MARGIN = 48.0;
    double value = (hasGiven && std::isfinite(given)) ? given : wardleySetting(config, key, fallback, 1.0, 1e6);
    return wardleyClamp(value, 200.0, 10000.0) - 2.0 * CANVAS_MARGIN;
}

template <typename DrawFn>
inline Rendered fitWidthWardley(const RenderOptions& options, double natural, double least, DrawFn&& draw) {
    Rendered result = draw(natural, false);
    double available = options.maxWidth;
    if (!(available > 0.0) || result.width <= available) return result;
    double floor = std::min(least, natural);
    double plot = natural;
    result = draw(plot, true);
    for (int round = 0; round < 3 && result.width > available && plot > floor; round++) {
        plot = std::max(floor, plot - (result.width - available));
        result = draw(plot, true);
    }
    return result;
}

inline Rendered drawWardley(const WardleyModel& model, const Config& config, const RenderOptions& options, double plotW, double naturalW, bool tight) {
    static const std::vector<std::string> DEFAULT_STAGES = {"Genesis", "Custom Built", "Product", "Commodity"};
    constexpr double R = 6.0;
    constexpr double SQUARE = 10.0;
    constexpr double LABEL_OFFSET = 8.0;
    constexpr double PIPELINE_PAD = 15.0;
    constexpr double FORCE_WIDTH = 60.0;
    constexpr double FORCE_HEIGHT = 30.0;
    constexpr double FORCE_HEAD = 20.0;
    const std::string ACCENT = "var(--_c)";
    const std::string LINE = "var(--_l)";

    double size = options.fontSize;
    double small = std::round(size * 0.8125);
    double tiny = std::round(size * 0.6875);
    const TextMeasurer& measurer = getMeasurer(options);
    double pad = options.padding;

    double plotH = wardleyDimension(config, model.sizeHeight, model.hasSize, "height", 600.0) * std::max(0.75, plotW / naturalW);
    const auto* sec = config.get("wardley-beta");
    bool showGrid = (sec && sec->isMap() && sec->get("showGrid") && sec->get("showGrid")->isBool() && sec->get("showGrid")->bVal);

    double minX = 0.0;
    double minY = 0.0;
    double maxX = plotW;
    double maxY = plotH;
    auto grow = [&](double x0, double y0, double x1, double y1) {
        if (x0 < minX) minX = x0;
        if (y0 < minY) minY = y0;
        if (x1 > maxX) maxX = x1;
        if (y1 > maxY) maxY = y1;
    };
    auto centered = [&](const Label& label, double cx, double cy, const std::string& attrs) -> std::string {
        grow(cx - label.width / 2.0, cy - label.height / 2.0, cx + label.width / 2.0, cy + label.height / 2.0);
        return labelSvg(label, cx, cy, attrs, options.icons);
    };
    auto starting = [&](const Label& label, double x, double y, const std::string& attrs) -> std::string {
        grow(x, y - label.size, x + label.width, y - label.size + label.height + 4.0);
        return startLabel(label, x, y, attrs);
    };
    double defaultMaxW = tight ? std::max(64.0, std::min(120.0, plotW / 2.0)) : 4000.0;
    auto textFn = [&](const std::string& raw, double fontSize, double maxWidth = -1.0, int style = 0) -> Label {
        return layoutLabel(raw, false, measurer, fontSize, maxWidth < 0.0 ? defaultMaxW : maxWidth, style);
    };
    auto projectX = [&](double value) -> double { return (value / 100.0) * plotW; };
    auto projectY = [&](double value) -> double { return plotH - (value / 100.0) * plotH; };

    const auto& stages = !model.axes.stages.empty() ? model.axes.stages : DEFAULT_STAGES;
    const auto& boundaries = model.axes.stageBoundaries;
    bool custom = model.axes.hasStageBoundaries && boundaries.size() == stages.size();
    std::string frame;
    std::string stageLabels;
    double stageBottom = plotH;
    double previous = 0.0;
    for (size_t idx = 0; idx < stages.size(); idx++) {
        double start = custom ? previous : (static_cast<double>(idx) / static_cast<double>(stages.size()));
        double end = custom ? wardleyClamp(boundaries[idx], 0.0, 1.0) : (static_cast<double>(idx + 1) / static_cast<double>(stages.size()));
        previous = end;
        if (idx > 0) {
            frame += "<path class=\"pele-wardley-stage\" d=\"M" + num(start * plotW) + ",0V" + num(plotH) + "\" stroke=\"var(--_b)\" stroke-dasharray=\"4 4\"/>";
        }
        Label label = textFn(stages[idx], small, std::max(48.0, std::abs(end - start) * plotW - 12.0));
        stageLabels += centered(label, ((start + end) / 2.0) * plotW, plotH + 8.0 + label.height / 2.0, " class=\"pele-wardley-stage-label\" font-size=\"" + num(small) + "\" fill=\"var(--_m)\"");
        stageBottom = std::max(stageBottom, plotH + 8.0 + label.height);
    }
    if (showGrid) {
        for (int i = 1; i < 4; i++) {
            frame += "<path class=\"pele-wardley-grid\" d=\"M" + num((plotW * i) / 4.0) + ",0V" + num(plotH) + "M0," + num((plotH * i) / 4.0) + "H" + num(plotW) + "\" stroke=\"var(--_b)\" stroke-dasharray=\"2 6\"/>";
        }
    }
    frame += "<path class=\"pele-wardley-axis\" d=\"M0,0V" + num(plotH) + "H" + num(plotW) + "\" stroke=\"" + LINE + "\"/>";
    Label xTitle = textFn(model.axes.hasXLabel ? model.axes.xLabel : "Evolution", small);
    Label yTitle = textFn(model.axes.hasYLabel ? model.axes.yLabel : "Visibility", small);
    std::string axisAttrs = " class=\"pele-wardley-axis-label\" font-size=\"" + num(small) + "\" font-weight=\"var(--_hw)\"";
    std::string axisLabels =
        centered(xTitle, plotW / 2.0, stageBottom + 6.0 + xTitle.height / 2.0, axisAttrs) +
        labelSvg(yTitle, -16.0, plotH / 2.0, axisAttrs + " transform=\"rotate(-90 -16 " + num(plotH / 2.0) + ")\"");
    grow(-16.0 - yTitle.height / 2.0, 0.0, 0.0, 0.0);
    double legendTop = stageBottom + 6.0 + xTitle.height + 10.0;

    struct PlacedNode {
        double x = 0.0;
        double y = 0.0;
        const WardleyNode* node = nullptr;
    };
    std::vector<std::string> posOrder;
    std::unordered_map<std::string, PlacedNode> positions;
    for (const auto& node : model.nodes) {
        if (!positions.count(node.id)) posOrder.push_back(node.id);
        positions[node.id] = {projectX(node.x), projectY(node.y), &node};
    }

    std::string pipelines;
    std::unordered_map<std::string, std::unordered_set<std::string>> members;
    for (const auto& pipeline : model.pipelines) {
        members[pipeline.nodeId] = std::unordered_set<std::string>(pipeline.componentIds.begin(), pipeline.componentIds.end());
        std::vector<PlacedNode*> parts;
        for (const auto& id : pipeline.componentIds) {
            auto it = positions.find(id);
            if (it != positions.end()) parts.push_back(&it->second);
        }
        if (parts.empty()) continue;
        std::stable_sort(parts.begin(), parts.end(), [](const PlacedNode* a, const PlacedNode* b) { return a->x < b->x; });
        double left = parts.front()->x;
        double right = parts.back()->x;
        double y = parts.front()->y;
        double top = y - 2.0 * R;
        auto pIt = positions.find(pipeline.nodeId);
        if (pIt != positions.end()) {
            pIt->second.x = (left + right) / 2.0;
            pIt->second.y = top - SQUARE / 6.0;
        }
        grow(left - PIPELINE_PAD, top, right + PIPELINE_PAD, top + 4.0 * R);
        pipelines +=
            "<g class=\"pele-wardley-pipeline\" data-id=\"" + escText(pipeline.nodeId) + "\">" +
            "<rect x=\"" + num(left - PIPELINE_PAD) + "\" y=\"" + num(top) + "\" width=\"" + num(right - left + 2.0 * PIPELINE_PAD) + "\" height=\"" + num(4.0 * R) + "\" fill=\"var(--_s)\" stroke=\"var(--_b)\"" + ROUNDED + "/>" +
            (parts.size() > 1 ? ("<path d=\"M" + num(left) + "," + num(y) + "H" + num(right) + "\" stroke=\"" + LINE + "\" stroke-dasharray=\"4 4\"/>") : "") +
            "</g>";
    }

    std::unordered_map<const WardleyNode*, Label> anchorLabels;
    auto reach = [&](const PlacedNode& at, double ux, double uy) -> double {
        const WardleyNode& node = *at.node;
        if (node.className == "anchor") {
            auto it = anchorLabels.find(&node);
            if (it == anchorLabels.end()) {
                Label lbl = textFn(node.label, small, tight ? std::max(64.0, plotW / 2.0) : 4000.0, Style::Bold);
                it = anchorLabels.emplace(&node, std::move(lbl)).first;
            }
            return 1.0 / std::hypot(ux / (it->second.width / 2.0 + 5.0), uy / (small * 0.8));
        }
        if (node.isPipelineParent) return SQUARE / 1.41421356237309504880;
        return !node.sourceStrategy.empty() ? (2.0 * R) : R;
    };

    std::unordered_map<const PlacedNode*, int> taken;
    auto occupy = [&](const PlacedNode* at, double ux, double uy) {
        if (std::abs(ux) < 0.25 || std::abs(uy) < 0.25) return;
        taken[at] |= (ux > 0.0 ? (uy < 0.0 ? 1 : 2) : (uy < 0.0 ? 4 : 8));
    };

    std::string links;
    for (const auto& link : model.links) {
        auto fIt = positions.find(link.source);
        auto tIt = positions.find(link.target);
        if (fIt == positions.end() || tIt == positions.end() || &fIt->second == &tIt->second) continue;
        auto mIt = members.find(link.target);
        if (mIt != members.end() && mIt->second.count(link.source)) continue;
        const PlacedNode& from = fIt->second;
        const PlacedNode& to = tIt->second;
        double dx = to.x - from.x;
        double dy = to.y - from.y;
        double distance = std::hypot(dx, dy);
        if (distance < 0.01) continue;
        double ux = dx / distance;
        double uy = dy / distance;
        double start = reach(from, ux, uy);
        double end = reach(to, ux, uy);
        if (start + end >= distance) continue;
        occupy(&from, ux, uy);
        occupy(&to, -ux, -uy);
        double x1 = from.x + ux * start;
        double y1 = from.y + uy * start;
        double x2 = to.x - ux * end;
        double y2 = to.y - uy * end;
        std::string heads;
        if (link.flow == "forward" || link.flow == "bidirectional") {
            heads += marker("arrow_point", x2, y2, ux, uy, LINE);
            x2 -= ux * markerTrim("arrow_point");
            y2 -= uy * markerTrim("arrow_point");
        }
        if (link.flow == "backward" || link.flow == "bidirectional") {
            heads += marker("arrow_point", x1, y1, -ux, -uy, LINE);
            x1 += ux * markerTrim("arrow_point");
            y1 += uy * markerTrim("arrow_point");
        }
        std::string caption;
        if (link.hasLabel && !link.label.empty()) {
            Label label = textFn(link.label, tiny + 1.0, 200.0);
            double side = ux >= 0.0 ? 1.0 : -1.0;
            double px = uy * side;
            double py = -ux * side;
            double away = 5.0 + (std::abs(px) * label.width) / 2.0 + (std::abs(py) * label.height) / 2.0;
            caption = centered(label, (from.x + to.x) / 2.0 + px * away, (from.y + to.y) / 2.0 + py * away, " class=\"pele-edge-label\" font-size=\"" + num(tiny + 1.0) + "\" fill=\"var(--_m)\"");
        }
        links +=
            "<g class=\"pele-edge" + std::string(link.dashed ? " pele-wardley-dashed" : "") + "\" data-id=\"" + escText(link.source) + "-" + escText(link.target) + "\">" +
            "<path d=\"M" + num(x1) + "," + num(y1) + "L" + num(x2) + "," + num(y2) + "\" stroke=\"" + LINE + "\"" + (link.dashed ? " stroke-dasharray=\"6 6\"" : "") + "/>" +
            heads + caption + "</g>";
    }

    std::string trends;
    for (const auto& trend : model.trends) {
        auto oIt = positions.find(trend.nodeId);
        if (oIt == positions.end()) continue;
        const PlacedNode& origin = oIt->second;
        double tx = projectX(trend.targetX);
        double ty = projectY(trend.targetY);
        double dx = tx - origin.x;
        double dy = ty - origin.y;
        double distance = std::hypot(dx, dy);
        std::string arrow;
        double start = distance > 0.0 ? reach(origin, dx / distance, dy / distance) : 0.0;
        if (distance > start + R + 4.0) {
            double ux = dx / distance;
            double uy = dy / distance;
            double ex = tx - ux * (R + 2.0);
            double ey = ty - uy * (R + 2.0);
            arrow =
                "<path d=\"M" + num(origin.x + ux * start) + "," + num(origin.y + uy * start) + "L" + num(ex - ux * markerTrim("arrow_point")) + "," + num(ey - uy * markerTrim("arrow_point")) + "\" stroke=\"" + ACCENT + "\" stroke-dasharray=\"4 4\"/>" +
                marker("arrow_point", ex, ey, ux, uy, ACCENT);
        }
        grow(tx - R, ty - R, tx + R, ty + R);
        trends +=
            "<g class=\"pele-wardley-trend\" data-id=\"" + escText(trend.nodeId) + "\">" + arrow +
            "<circle cx=\"" + num(tx) + "\" cy=\"" + num(ty) + "\" r=\"" + num(R) + "\" fill=\"var(--_bg)\" stroke=\"" + ACCENT + "\"/></g>";
    }

    std::string nodes;
    double tri = 1.2 * R;
    double dot = 0.7 * R;
    constexpr double WARDLEY_PI = 3.14159265358979323846;
    for (const auto& idKey : posOrder) {
        const PlacedNode& at = positions[idKey];
        const WardleyNode& node = *at.node;
        double x = at.x;
        double y = at.y;
        bool anchor = (node.className == "anchor");
        const std::string& strategy = node.sourceStrategy;
        std::string shape;
        if (!strategy.empty()) {
            std::string fill = (strategy == "outsource") ? "var(--_m)" : (strategy == "buy") ? "var(--_a)" : (strategy == "build") ? "var(--_s)" : "var(--_bg)";
            shape += "<circle class=\"pele-wardley-" + strategy + "\" cx=\"" + num(x) + "\" cy=\"" + num(y) + "\" r=\"" + num(2.0 * R) + "\" fill=\"" + fill + "\" stroke=\"" + LINE + "\"/>";
        }
        if (strategy == "market") {
            double bx = tri * std::cos(WARDLEY_PI / 6.0);
            double by = y + tri * std::sin(WARDLEY_PI / 6.0);
            shape += "<path d=\"M" + num(x) + "," + num(y - tri) + "L" + num(x - bx) + "," + num(by) + "H" + num(x + bx) + "Z\" fill=\"none\" stroke=\"" + LINE + "\"/>";
            double pts[3][2] = {{x, y - tri}, {x - bx, by}, {x + bx, by}};
            for (const auto& pt : pts) {
                shape += "<circle cx=\"" + num(pt[0]) + "\" cy=\"" + num(pt[1]) + "\" r=\"" + num(dot) + "\" fill=\"var(--_bg)\" stroke=\"" + LINE + "\"/>";
            }
        } else if (node.isPipelineParent) {
            shape += "<rect x=\"" + num(x - SQUARE / 2.0) + "\" y=\"" + num(y - SQUARE / 2.0) + "\" width=\"" + num(SQUARE) + "\" height=\"" + num(SQUARE) + "\" fill=\"var(--_bg)\" stroke=\"" + LINE + "\"/>";
        } else if (!anchor) {
            shape += "<circle cx=\"" + num(x) + "\" cy=\"" + num(y) + "\" r=\"" + num(R) + "\" fill=\"var(--_bg)\" stroke=\"" + LINE + "\"/>";
        }
        if (node.inertia) {
            double offset = (node.isPipelineParent ? (SQUARE / 2.0) : R) + 10.0 + (!strategy.empty() ? (R + 4.0) : 0.0);
            shape += "<rect class=\"pele-wardley-inertia\" x=\"" + num(x + offset) + "\" y=\"" + num(y - R) + "\" width=\"4\" height=\"" + num(2.0 * R) + "\" fill=\"" + LINE + "\"/>";
        }
        bool hasOx = node.hasLabelOffsetX;
        bool hasOy = node.hasLabelOffsetY;
        double ox = hasOx ? wardleyClamp(node.labelOffsetX, -5000.0, 5000.0) : 0.0;
        double oy = hasOy ? wardleyClamp(node.labelOffsetY, -5000.0, 5000.0) : 0.0;
        std::string caption;
        if (anchor) {
            auto aIt = anchorLabels.find(&node);
            Label lbl = (aIt != anchorLabels.end()) ? aIt->second : textFn(node.label, small, tight ? std::max(64.0, plotW / 2.0) : 4000.0, Style::Bold);
            caption = centered(lbl, x + (hasOx ? ox : 0.0), y + (hasOy ? oy : -3.0), " class=\"pele-label\" font-size=\"" + num(small) + "\" font-weight=\"var(--_hw)\"");
        } else {
            Label lbl = textFn(node.label, small);
            std::string attrs = " class=\"pele-label\" font-size=\"" + num(small) + "\"";
            double lift = LABEL_OFFSET + (!strategy.empty() ? 10.0 : 0.0);
            if (hasOx || hasOy) {
                caption = starting(lbl, x + (hasOx ? ox : lift), y + (hasOy ? oy : -lift), attrs);
            } else if (node.inPipeline) {
                caption = centered(lbl, x, y + 2.0 * R + 4.0 + lbl.height / 2.0, attrs);
            } else {
                int busy = taken.count(&at) ? taken[&at] : 0;
                int corner = (busy & 1) == 0 ? 1 : (busy & 2) == 0 ? 2 : (busy & 4) == 0 ? 4 : (busy & 8) == 0 ? 8 : 1;
                bool right = (corner == 1 || corner == 2);
                bool up = (corner == 1 || corner == 4);
                caption = starting(lbl, right ? (x + lift) : (x - lift - lbl.width), up ? (y - lift) : (y + lift + small * 0.75), attrs);
            }
        }
        grow(x - 2.0 * R, y - 2.0 * R, x + 2.0 * R + 20.0, y + 2.0 * R);
        std::string kindClass = anchor ? "anchor" : node.isPipelineParent ? "pipeline-parent" : node.inPipeline ? "pipeline-component" : "component";
        nodes += "<g class=\"pele-node pele-wardley-" + kindClass + "\" data-id=\"" + escText(node.id) + "\">" + shape + caption + "</g>";
    }

    std::string annotations;
    for (const auto& ann : model.annotations) {
        Label label = textFn(std::to_string(ann.number), tiny, 4000.0, Style::Bold);
        std::string marks;
        for (const auto& coord : ann.coordinates) {
            double x = projectX(coord.x);
            double y = projectY(coord.y);
            double r = std::max(10.0, label.width / 2.0 + 4.0);
            grow(x - r, y - r, x + r, y + r);
            marks += "<circle cx=\"" + num(x) + "\" cy=\"" + num(y) + "\" r=\"" + num(r) + "\" fill=\"var(--_bg)\" stroke=\"" + LINE + "\"/>" +
                     labelSvg(label, x, y, " font-size=\"" + num(tiny) + "\" font-weight=\"var(--_hw)\"");
        }
        annotations += "<g class=\"pele-wardley-annotation\" data-id=\"" + escText(std::to_string(ann.number)) + "\">" + marks + "</g>";
    }
    std::vector<const WardleyAnnotation*> listed;
    for (const auto& ann : model.annotations) {
        if (ann.hasText && !ann.text.empty()) listed.push_back(&ann);
    }
    std::stable_sort(listed.begin(), listed.end(), [](const WardleyAnnotation* a, const WardleyAnnotation* b) {
        return a->number < b->number;
    });
    if (!listed.empty()) {
        std::vector<Label> lines;
        double w = 0.0;
        double h = 0.0;
        for (const auto* a : listed) {
            Label line = textFn(std::to_string(a->number) + ". " + a->text, small, tight ? std::max(80.0, plotW - 20.0) : 320.0);
            w = std::max(w, line.width);
            h += line.height;
            lines.push_back(std::move(line));
        }
        w += 20.0;
        h += 14.0;
        double bx = model.hasAnnotationsBox ? wardleyClamp(projectX(model.annotationsBox.x), 0.0, std::max(0.0, plotW - w)) : 0.0;
        double by = model.hasAnnotationsBox ? wardleyClamp(projectY(model.annotationsBox.y), 0.0, std::max(0.0, plotH - h)) : legendTop;
        grow(bx, by, bx + w, by + h);
        std::string rows;
        double y = by + 7.0;
        for (const auto& line : lines) {
            rows += startLabel(line, bx + 10.0, y + line.lineHeight / 2.0 + small * 0.35, " font-size=\"" + num(small) + "\"");
            y += line.height;
        }
        annotations += "<g class=\"pele-wardley-annotations\"><rect x=\"" + num(bx) + "\" y=\"" + num(by) + "\" width=\"" + num(w) + "\" height=\"" + num(h) + "\" fill=\"var(--_bg)\" stroke=\"var(--_b)\"" + ROUNDED + "/>" + rows + "</g>";
    }

    std::string notes;
    for (const auto& note : model.notes) {
        notes += starting(textFn(note.text, small, std::min(360.0, plotW * 0.6), Style::Bold), projectX(note.x), projectY(note.y), " class=\"pele-wardley-note\" font-size=\"" + num(small) + "\" font-weight=\"var(--_hw)\"");
    }

    std::string forces;
    auto force = [&](const std::string& name, double px, double py, bool forward) {
        double x = projectX(px);
        double y = projectY(py);
        double half = FORCE_HEIGHT / 2.0;
        double tip = forward ? (x + FORCE_WIDTH) : x;
        double tail = forward ? x : (x + FORCE_WIDTH);
        double neck = forward ? (x + FORCE_WIDTH - FORCE_HEAD) : (x + FORCE_HEAD);
        std::string d = "M" + num(tail) + "," + num(y - half) + "H" + num(neck) + "V" + num(y - half - 8.0) + "L" + num(tip) + "," + num(y) + "L" + num(neck) + "," + num(y + half + 8.0) + "V" + num(y + half) + "H" + num(tail) + "Z";
        grow(x, y - half - 8.0, x + FORCE_WIDTH, y + half + 8.0);
        forces +=
            "<g class=\"pele-wardley-" + std::string(forward ? "accelerator" : "deaccelerator") + "\" data-id=\"" + escText(name) + "\">" +
            "<path d=\"" + d + "\" fill=\"var(--_s)\" stroke=\"" + LINE + "\"/>" +
            centered(textFn(name, tiny + 1.0, 160.0, Style::Bold), x + FORCE_WIDTH / 2.0, y + half + 8.0 + 12.0, " font-size=\"" + num(tiny + 1.0) + "\" font-weight=\"var(--_hw)\"") +
            "</g>";
    };
    for (const auto& a : model.accelerators) force(a.name, a.x, a.y, true);
    for (const auto& d : model.deaccelerators) force(d.name, d.x, d.y, false);

    Label title = layoutLabel(model.title, false, measurer, size, tight ? std::max(80.0, plotW) : 4000.0, Style::Bold);
    double titleHeight = title.height > 0.0 ? (title.height + 12.0) : 0.0;
    double left = std::min(minX, plotW / 2.0 - title.width / 2.0);
    double right = std::max(maxX, plotW / 2.0 + title.width / 2.0);
    double tx = pad - left;
    double ty = pad + titleHeight - minY;
    double totalWidth = std::ceil(right - left + 2.0 * pad);
    double totalHeight = std::ceil(maxY - minY + titleHeight + 2.0 * pad);
    std::string content =
        labelSvg(title, tx + plotW / 2.0, pad + title.height / 2.0, " class=\"pele-title\" font-weight=\"var(--_tw)\"") +
        "<g transform=\"translate(" + num(tx) + "," + num(ty) + ")\">" +
        "<g class=\"pele-wardley-frame\" fill=\"none\">" + frame + "</g>" +
        stageLabels +
        axisLabels +
        pipelines +
        (!links.empty() ? ("<g class=\"pele-edges\" fill=\"none\">" + links + "</g>") : "") +
        (!trends.empty() ? ("<g class=\"pele-wardley-trends\" fill=\"none\">" + trends + "</g>") : "") +
        nodes +
        annotations +
        notes +
        forces +
        "</g>";

    return {
        svgDocument("wardley", totalWidth, totalHeight, size, options, model, content),
        totalWidth,
        totalHeight,
        {}
    };
}

inline Rendered render(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options) {
    WardleyModel model = buildWardley(parseWardley(source), title);
    double natural = wardleyDimension(config, model.sizeWidth, model.hasSize, "width", 900.0);
    return fitWidthWardley(options, natural, 160.0, [&](double plotW, bool tight) {
        return drawWardley(model, config, options, plotW, natural, tight);
    });
}

} // namespace WardleyDiagram

inline Rendered renderWardley(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options) {
    return WardleyDiagram::render(source, config, title, options);
}

inline Rendered renderWardley(const std::string& raw, RenderOptions options = {}) {
    auto pre = preprocess(raw);
    std::string parsed = encodeEntities(pre.text) + "\n";
    const auto* section = pre.config.get("wardley-beta");
    bool fixed = (section && section->isMap() && section->get("useMaxWidth") && section->get("useMaxWidth")->isBool() && !section->get("useMaxWidth")->bVal);
    if (!options.hasResponsive) {
        options.responsive = !fixed;
        options.hasResponsive = true;
    }
    return WardleyDiagram::render(parsed, pre.config, pre.title, options);
}

} // namespace Pele
