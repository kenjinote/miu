#pragma once
#include "../foundation.h"

namespace Pele {
namespace CynefinDiagram {

// ============================================================================
// src/diagrams/cynefin/parser.ts
// ============================================================================

namespace T {
enum Kind {
    keywordColon = 0,
    keyword = 1,
    arrow = 2,
    colon = 3,
    domain = 4,
    accDescr = 5,
    accTitle = 6,
    title = 7,
    string = 8,
    newline = 9,
    whitespace = 10,
    yaml = 11,
    directive = 12,
    singleLineComment = 13
};
} // namespace T

inline const std::vector<TokenType>& cynefinTokens() {
    static const std::vector<TokenType> types = []() {
        auto mark = [](const std::string& text) {
            TokenType t;
            t.name = text;
            t.literal = text;
            t.isRegex = false;
            return t;
        };

        TokenType dom;
        dom.name = "DOMAIN_NAME";
        dom.isRegex = true;
        dom.first = "c";
        dom.match = [](const std::string& src, int at, int) -> int {
            static const char* DOMAINS[] = {"complicated", "confusion", "complex", "chaotic", "clear"};
            // Note: in JS regex `/complex|complicated|clear|chaotic|confusion/y`, alternation tries in order!
            static const char* ORDER[] = {"complex", "complicated", "clear", "chaotic", "confusion"};
            for (const char* w : ORDER) {
                size_t len = std::strlen(w);
                if (startsWith(src, w, static_cast<size_t>(at))) {
                    return at + static_cast<int>(len);
                }
            }
            return -1;
        };

        return std::vector<TokenType>{
            mark("cynefin-beta:"),
            common_tokens::keyword("cynefin-beta"),
            mark("-->"),
            mark(":"),
            dom,
            common_tokens::makeAccDescr(),
            common_tokens::makeAccTitle(),
            common_tokens::makeTitle(),
            common_tokens::makeString(),
            common_tokens::makeNewline(),
            common_tokens::makeWhitespace(),
            common_tokens::makeYaml(),
            common_tokens::makeDirective(),
            common_tokens::makeSingleLineComment()
        };
    }();
    return types;
}

struct DomainBlockAst {
    std::string domain;
    std::vector<std::string> items;
};

struct TransitionAst {
    std::string from;
    std::string to;
    std::optional<std::string> label;
};

struct CynefinAst {
    std::optional<std::string> title;
    std::optional<std::string> accTitle;
    std::optional<std::string> accDescr;
    std::vector<DomainBlockAst> domains;
    std::vector<TransitionAst> transitions;
};

inline std::string fixSingle(const std::string& text) noexcept {
    std::string s = trimStr(text);
    std::string out;
    int spaces = 0;
    char firstSpace = ' ';
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '\t' || c == ' ') {
            if (spaces == 0) firstSpace = c;
            spaces++;
        } else {
            if (spaces == 1) out.push_back(firstSpace);
            else if (spaces >= 2) out.push_back(' ');
            spaces = 0;
            out.push_back(c);
        }
    }
    return out;
}

inline std::string fixTitleValue(const std::string& text) noexcept {
    size_t pos = text.find("title");
    if (pos == std::string::npos) return "";
    return fixSingle(text.substr(pos + 5));
}

inline std::string fixAccTitleValue(const std::string& text) noexcept {
    size_t pos = text.find(':');
    if (pos == std::string::npos) return "";
    return fixSingle(text.substr(pos + 1));
}

inline std::string fixAccDescrValue(const std::string& text) noexcept {
    size_t brace = text.find('{');
    size_t colon = text.find(':');
    if (colon != std::string::npos && (brace == std::string::npos || colon < brace)) {
        return fixSingle(text.substr(colon + 1));
    }
    if (brace != std::string::npos) {
        size_t close = text.find('}', brace + 1);
        std::string body = text.substr(brace + 1, close == std::string::npos ? std::string::npos : close - brace - 1);
        return cleanAccDescr(body);
    }
    return "";
}

inline CynefinAst parseCynefin(const std::string& src) {
    const auto& types = cynefinTokens();
    Tokens toks = tokenize(src, types, "cynefin");
    TokenReader r(toks, types, "cynefin");
    CynefinAst ast;

    auto skipLines = [&]() {
        while (r.kind() == T::newline) r.i++;
    };
    auto endOfLine = [&]() {
        if (r.kind() == -1) return;
        if (r.kind() != T::newline) r.fail("a line break or the end of input");
        skipLines();
    };

    skipLines();
    if (r.kind() != T::keyword && r.kind() != T::keywordColon) r.fail("'cynefin-beta'");
    r.i++;
    while (r.kind() != -1) {
        switch (r.kind()) {
            case T::newline:
                r.i++;
                break;
            case T::accDescr:
                ast.accDescr = fixAccDescrValue(r.take());
                endOfLine();
                break;
            case T::accTitle:
                ast.accTitle = fixAccTitleValue(r.take());
                endOfLine();
                break;
            case T::title:
                ast.title = fixTitleValue(r.take());
                endOfLine();
                break;
            case T::domain: {
                std::string name = r.take();
                if (r.accept(T::arrow)) {
                    TransitionAst tr;
                    tr.from = name;
                    tr.to = r.expect(T::domain);
                    if (r.accept(T::colon)) {
                        tr.label = common_tokens::stringValue(r.expect(T::string));
                    }
                    endOfLine();
                    ast.transitions.push_back(std::move(tr));
                    break;
                }
                DomainBlockAst block;
                block.domain = name;
                skipLines();
                while (r.kind() == T::string) {
                    block.items.push_back(common_tokens::stringValue(r.take()));
                    skipLines();
                }
                ast.domains.push_back(std::move(block));
                break;
            }
            default:
                r.fail("a domain, a transition, a title, or a line break");
        }
    }
    return ast;
}

// ============================================================================
// src/diagrams/cynefin/model.ts
// ============================================================================

struct CynefinDomain {
    std::string name;
    std::vector<std::string> items;
};

struct CynefinTransition {
    std::string from;
    std::string to;
    std::string label;
    bool hasLabel = false;
};

struct CynefinModel : public DiagramMeta {
    std::unordered_map<std::string, CynefinDomain> domains;
    std::vector<CynefinTransition> transitions;
};

inline CynefinModel buildCynefin(const CynefinAst& ast, const std::string& fallbackTitle) noexcept {
    CynefinModel model;
    for (const auto& block : ast.domains) {
        model.domains[block.domain] = {block.domain, block.items};
    }
    for (const auto& t : ast.transitions) {
        if (t.from == t.to) continue;
        CynefinTransition ct;
        ct.from = t.from;
        ct.to = t.to;
        if (t.label.has_value() && !t.label->empty()) {
            ct.label = *t.label;
            ct.hasLabel = true;
        }
        model.transitions.push_back(std::move(ct));
    }
    model.title = (ast.title.has_value() && !ast.title->empty()) ? *ast.title : fallbackTitle;
    if (ast.accTitle.has_value() && !ast.accTitle->empty()) {
        model.accTitle = trimStartStr(*ast.accTitle);
    }
    if (ast.accDescr.has_value() && !ast.accDescr->empty()) {
        model.accDescr = *ast.accDescr;
    }
    return model;
}

// ============================================================================
// src/diagrams/cynefin/boundaries.ts
// ============================================================================

inline double seededRandom(double seedVal) noexcept {
    uint32_t seed = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int64_t>(seedVal)));
    uint32_t t = seed + 0x6d2b79f5u;
    t = (t ^ (t >> 15)) * (t | 1u);
    t ^= t + (t ^ (t >> 7)) * (t | 61u);
    return static_cast<double>(t ^ (t >> 14)) / 4294967296.0;
}

inline int32_t hashString(const std::string& str) noexcept {
    auto u16 = toUtf16(str);
    uint32_t hash = 0;
    for (uint16_t c : u16) {
        hash = (hash << 5) - hash + static_cast<uint32_t>(c);
    }
    return static_cast<int32_t>(hash);
}

inline double resolveSeed(double configured, const std::string& id) noexcept {
    if (std::isfinite(configured) && configured != 0.0) return configured;
    return static_cast<double>(hashString(id));
}

struct WaveOptions {
    int segments = 7;
    bool vertical = false;
    int pointStep = 0;
    int controlStep = 0;
    int controlShift = 0;
    bool pinStart = false;
    bool pinEnd = false;
};

inline std::vector<double> wave(double from, double to, double center, double seed, double amplitude, const WaveOptions& o) noexcept {
    double step = (to - from) / static_cast<double>(o.segments);
    std::vector<double> along;
    std::vector<double> across;
    for (int i = 0; i <= o.segments; i++) {
        bool pinned = (i == 0 && o.pinStart) || (i == o.segments && o.pinEnd);
        along.push_back(from + static_cast<double>(i) * step);
        across.push_back(pinned ? center : (center + seededRandom(seed + static_cast<double>(i * o.pointStep)) * amplitude * 2.0 - amplitude));
    }
    std::vector<double> out = o.vertical ? std::vector<double>{across[0], along[0]} : std::vector<double>{along[0], across[0]};
    for (int i = 0; i < o.segments; i++) {
        double mid = (along[i] + along[i + 1]) / 2.0;
        double offset = amplitude * 1.5 * (i % 2 == 0 ? 1.0 : -1.0) * seededRandom(seed + static_cast<double>(i * o.controlStep + o.controlShift));
        if (o.vertical) {
            out.push_back(across[i] + offset);
            out.push_back(mid);
            out.push_back(across[i + 1] - offset);
            out.push_back(mid);
            out.push_back(across[i + 1]);
            out.push_back(along[i + 1]);
        } else {
            out.push_back(mid);
            out.push_back(across[i] + offset);
            out.push_back(mid);
            out.push_back(across[i + 1] - offset);
            out.push_back(along[i + 1]);
            out.push_back(across[i + 1]);
        }
    }
    return out;
}

inline std::vector<double> cliff(double cx, double top, double bottom, double amplitude) noexcept {
    double h = bottom - top;
    return {
        cx, top,
        cx + amplitude, top + h * 0.2, cx - amplitude * 1.5, top + h * 0.55, cx + amplitude * 0.5, top + h * 0.75,
        cx - amplitude, top + h * 0.85, cx + amplitude * 0.3, top + h * 0.95, cx, bottom
    };
}

inline std::string forwardCurve(const std::vector<double>& curve) noexcept {
    std::string d;
    for (size_t i = 2; i + 5 < curve.size(); i += 6) {
        Pele::appendCat(d, {"C", num(curve[i]), ",", num(curve[i + 1]), " ", num(curve[i + 2]), ",", num(curve[i + 3]), " ", num(curve[i + 4]), ",", num(curve[i + 5])});
    }
    return d;
}

inline std::string backwardCurve(const std::vector<double>& curve) noexcept {
    std::string d;
    for (int i = static_cast<int>(curve.size()) - 6; i >= 2; i -= 6) {
        Pele::appendCat(d, {"C", num(curve[i + 2]), ",", num(curve[i + 3]), " ", num(curve[i]), ",", num(curve[i + 1]), " ", num(curve[i - 2]), ",", num(curve[i - 1])});
    }
    return d;
}

inline std::string curvePath(const std::vector<double>& curve) noexcept {
    return Pele::cat({"M", num(curve[0]), ",", num(curve[1]), forwardCurve(curve)});
}

inline std::string ellipsePath(double cx, double cy, double rx, double ry) noexcept {
    std::string r = Pele::cat({num(rx), ",", num(ry)});
    return Pele::cat({"M", num(cx - rx), ",", num(cy), "A", r, " 0 1,1 ", num(cx + rx), ",", num(cy), "A", r, " 0 1,1 ", num(cx - rx), ",", num(cy), "Z"});
}

// ============================================================================
// src/diagrams/cynefin/render.ts
// ============================================================================

constexpr int CONFUSION_COLOR = 4;
constexpr size_t MAX_CONFUSION_ITEMS = 3;
constexpr double ITEM_MAX_WIDTH = 200.0;
constexpr double ITEM_PAD_X = 10.0;
constexpr double ITEM_PAD_Y = 4.0;
constexpr double ITEM_GAP = 6.0;
constexpr double HEAD_GAP = 10.0;
constexpr double MARGIN = 20.0;
constexpr double CLEARANCE = 10.0;

struct Badge {
    std::string text;
    Label label;
    double w = 0.0;
    double h = 0.0;
    bool overflow = false;
};

struct Block {
    std::string name;
    Label title;
    std::vector<Label> subtitles;
    std::vector<Badge> badges;
    double w = 0.0;
    double h = 0.0;
    double cx = 0.0;
    double cy = 0.0;
};

inline double cynefinSetting(const Config& config, const std::string& key, double fallback, double minVal, double maxVal) noexcept {
    const auto* c = config.get("cynefin");
    if (!c || !c->isMap()) return fallback;
    const auto* v = c->get(key);
    if (!v || !v->isNumber()) return fallback;
    return (v->nVal >= minVal && v->nVal <= maxVal) ? v->nVal : fallback;
}

inline Rendered renderCynefinModel(const CynefinModel& model, const Config& config, const RenderOptions& options) noexcept {
    double size = options.hasFontSize ? options.fontSize : 16.0;
    double small = std::round(size * 0.8125);
    double itemSize = std::round(size * 0.875);
    const TextMeasurer& measurer = getMeasurer(options);
    double pad = options.hasPadding ? options.padding : 8.0;
    const auto& icons = options.icons;
    const auto* cynCfg = config.get("cynefin");
    bool showDescriptions = !(cynCfg && cynCfg->isMap() && cynCfg->get("showDomainDescriptions") && cynCfg->get("showDomainDescriptions")->isBool() && !cynCfg->get("showDomainDescriptions")->bVal);
    double amplitude = cynefinSetting(config, "boundaryAmplitude", 8.0, 0.0, 50.0);
    double seed = resolveSeed(cynefinSetting(config, "seed", 0.0, -1e15, 1e15), "pele");
    double itemLine = std::round(itemSize * 1.5);

    struct QuadSpec {
        const char* name;
        int col;
        int row;
        int color;
    };
    static const QuadSpec QUADRANTS[4] = {
        {"complex", 0, 0, 0},
        {"complicated", 1, 0, 1},
        {"chaotic", 0, 1, 3},
        {"clear", 1, 1, 2}
    };

    auto getMeta = [](const std::string& name) -> std::tuple<std::string, std::string, std::string> {
        if (name == "complex") return {"Complex", "Probe \xE2\x86\x92 Sense \xE2\x86\x92 Respond", "Emergent Practices"};
        if (name == "complicated") return {"Complicated", "Sense \xE2\x86\x92 Analyse \xE2\x86\x92 Respond", "Good Practices"};
        if (name == "clear") return {"Clear", "Sense \xE2\x86\x92 Categorise \xE2\x86\x92 Respond", "Best Practices"};
        if (name == "chaotic") return {"Chaotic", "Act \xE2\x86\x92 Sense \xE2\x86\x92 Respond", "Novel Practices"};
        return {"Confusion", "", "Disorder"};
    };

    auto makeBadge = [&](const std::string& text, bool overflow) -> Badge {
        Label label = layoutLabel(text, false, measurer, itemSize, ITEM_MAX_WIDTH);
        return {text, label, label.width + 2.0 * ITEM_PAD_X, std::max(label.height, itemLine) + 2.0 * ITEM_PAD_Y, overflow};
    };

    auto makeBlock = [&](const std::string& name) -> Block {
        auto [m0, m1, m2] = getMeta(name);
        Label title = layoutLabel(m0, false, measurer, size, 4000.0, Style::Bold);
        std::vector<Label> subtitles;
        if (showDescriptions) {
            if (!m1.empty()) subtitles.push_back(layoutLabel(m1, false, measurer, small, 4000.0, Style::Italic));
            if (!m2.empty()) subtitles.push_back(layoutLabel(m2, false, measurer, small, 4000.0, Style::Italic));
        }
        std::vector<std::string> items;
        auto it = model.domains.find(name);
        if (it != model.domains.end()) items = it->second.items;
        size_t shown = (name == "confusion" && items.size() > MAX_CONFUSION_ITEMS) ? MAX_CONFUSION_ITEMS : items.size();
        std::vector<Badge> badges;
        for (size_t i = 0; i < shown; i++) badges.push_back(makeBadge(items[i], false));
        if (shown < items.size()) badges.push_back(makeBadge("+" + std::to_string(items.size() - shown) + " more", true));

        double w = title.width;
        double h = title.height;
        for (const auto& s : subtitles) {
            w = std::max(w, s.width);
            h += s.height;
        }
        if (!badges.empty()) h += HEAD_GAP - ITEM_GAP;
        for (const auto& b : badges) {
            w = std::max(w, b.w);
            h += ITEM_GAP + b.h;
        }
        return {name, std::move(title), std::move(subtitles), std::move(badges), w, h, 0.0, 0.0};
    };

    std::vector<Block> quadrants;
    for (const auto& q : QUADRANTS) quadrants.push_back(makeBlock(q.name));
    Block confusion = makeBlock("confusion");
    double rx = std::max(88.0, confusion.w * 0.71 + 8.0);
    double ry = std::max(64.0, confusion.h * 0.71 + 6.0);

    double quadW = cynefinSetting(config, "width", 640.0, 100.0, 10000.0) / 2.0;
    double quadH = cynefinSetting(config, "height", 440.0, 100.0, 10000.0) / 2.0;
    for (const auto& b : quadrants) {
        quadW = std::max(quadW, b.w + 2.0 * MARGIN);
        quadH = std::max(quadH, b.h + 2.0 * MARGIN);
    }
    for (const auto& b : quadrants) {
        double dx = (quadW - b.w) / 2.0;
        double dy = (quadH - b.h) / 2.0;
        double nx = dx / (rx + CLEARANCE);
        double ny = dy / (ry + CLEARANCE);
        if (nx * nx + ny * ny >= 1.0) continue;
        double wider = b.w + 2.0 * (rx + CLEARANCE);
        double taller = b.h + 2.0 * (ry + CLEARANCE);
        if (wider * quadH <= quadW * taller) quadW = std::max(quadW, wider);
        else quadH = std::max(quadH, taller);
    }

    Label title = layoutLabel(model.title, false, measurer, size, 4000.0, Style::Bold);
    double titleHeight = title.height > 0.0 ? title.height + 12.0 : 0.0;
    double width = 2.0 * quadW;
    double height = 2.0 * quadH;
    double x0 = pad;
    double y0 = pad + titleHeight;
    double cx = x0 + quadW;
    double cy = y0 + quadH;
    double right = x0 + width;
    double bottom = y0 + height;

    auto fold = wave(y0, cy, cx, seed, amplitude, {4, true, 17, 31, 7, false, true});
    auto west = wave(x0, cx, cy, seed + 100.0, amplitude, {4, false, 23, 37, 11, false, true});
    auto east = wave(cx, right, cy, seed + 200.0, amplitude, {4, false, 23, 37, 11, true, false});
    auto drop = cliff(cx, cy, bottom, width * 0.03);
    auto atPt = [](const std::vector<double>& c, bool end) -> std::string {
        return end
            ? Pele::cat({num(c[c.size() - 2]), ",", num(c[c.size() - 1])})
            : Pele::cat({num(c[0]), ",", num(c[1])});
    };

    Pele::FlatMap<std::string, std::string> regions = {
        {"complex", Pele::cat({"M", num(x0), ",", num(y0), "L", atPt(fold, false), forwardCurve(fold), backwardCurve(west), "Z"})},
        {"complicated", Pele::cat({"M", atPt(fold, false), "L", num(right), ",", num(y0), "L", atPt(east, true), backwardCurve(east), backwardCurve(fold), "Z"})},
        {"chaotic", Pele::cat({"M", atPt(west, false), forwardCurve(west), forwardCurve(drop), "L", num(x0), ",", num(bottom), "Z"})},
        {"clear", Pele::cat({"M", atPt(east, false), forwardCurve(east), "L", num(right), ",", num(bottom), "L", atPt(drop, true), backwardCurve(drop), "Z"})}
    };

    std::string fills;
    for (const auto& q : QUADRANTS) {
        fills += "<path class=\"pele-cynefin-region\" data-id=\"" + std::string(q.name) + "\" d=\"" + regions[q.name] + "\" fill=\"" + seriesColor(q.color) + "\" fill-opacity=\"0.14\"/>";
    }
    std::string boundaries = Pele::cat({"<g class=\"pele-cynefin-boundaries\" fill=\"none\" stroke=\"var(--_b)\" stroke-dasharray=\"6 3\">"
        "<path d=\"", curvePath(fold), "\"/><path d=\"", curvePath(west), "\"/><path d=\"", curvePath(east), "\"/>"
        "<path class=\"pele-cynefin-cliff\" d=\"", curvePath(drop), "\" stroke=\"var(--_l)\" stroke-width=\"2\" stroke-dasharray=\"none\" stroke-linecap=\"round\"/></g>"});
    std::string oval = ellipsePath(cx, cy, rx, ry);
    std::string ellipse = Pele::cat({"<path d=\"", oval, "\" fill=\"var(--_bg)\"/>"
        "<path class=\"pele-cynefin-region\" data-id=\"confusion\" d=\"", oval, "\" fill=\"", seriesColor(CONFUSION_COLOR), "\" fill-opacity=\"0.18\" stroke=\"var(--_b)\" stroke-dasharray=\"4 2\"/>"});

    auto drawBlock = [&](const Block& b) -> std::string {
        double y = b.cy - b.h / 2.0;
        std::string out = labelSvg(b.title, b.cx, y + b.title.height / 2.0, " class=\"pele-label\" font-weight=\"var(--_hw)\"");
        y += b.title.height;
        for (const auto& s : b.subtitles) {
            out += labelSvg(s,  b.cx,  y + s.height / 2.0, Pele::cat({" class=\"pele-cynefin-subtitle\" font-size=\"", num(small), "\" font-style=\"italic\" fill=\"var(--_m)\""}));
            y += s.height;
        }
        y += HEAD_GAP - ITEM_GAP;
        for (const auto& item : b.badges) {
            y += ITEM_GAP;
            out +=
                "<g class=\"pele-node" + std::string(item.overflow ? " pele-cynefin-overflow" : "") + "\"" + (item.overflow ? "" : Pele::cat({" data-id=\"", escText(item.text), "\""})) + ">"
                "<rect x=\"" + num(b.cx - item.w / 2.0) + "\" y=\"" + num(y) + "\" width=\"" + num(item.w) + "\" height=\"" + num(item.h) + "\" fill=\"var(--_bg)\" stroke=\"var(--_b)\"" + (item.overflow ? " stroke-dasharray=\"3 2\"" : "") + ROUNDED + "/>" +
                labelSvg(item.label,  b.cx,  y + item.h / 2.0, Pele::cat({" class=\"pele-label\" font-size=\"", num(itemSize), "\"", (item.overflow ? " fill=\"var(--_m)\"" : "")}),  icons) +
                "</g>";
            y += item.h;
        }
        return Pele::cat({"<g class=\"pele-cynefin-domain\" data-id=\"", b.name, "\">", out, "</g>"});
    };

    std::unordered_map<std::string, Block*> blocks;
    std::string domains;
    for (size_t idx = 0; idx < 4; idx++) {
        auto& b = quadrants[idx];
        b.cx = x0 + quadW * (static_cast<double>(QUADRANTS[idx].col) + 0.5);
        b.cy = y0 + quadH * (static_cast<double>(QUADRANTS[idx].row) + 0.5);
        blocks[QUADRANTS[idx].name] = &b;
        domains += drawBlock(b);
    }
    confusion.cx = cx;
    confusion.cy = cy;
    blocks["confusion"] = &confusion;
    domains += drawBlock(confusion);

    auto inside = [&](const Block* b, double x, double y) -> bool {
        if (b == &confusion) {
            double ux = (x - cx) / (rx + 4.0);
            double uy = (y - cy) / (ry + 4.0);
            return ux * ux + uy * uy <= 1.0;
        }
        return std::abs(x - b->cx) <= b->w / 2.0 + 8.0 && std::abs(y - b->cy) <= b->h / 2.0 + 8.0;
    };

    std::string edges;
    Pele::FlatMap<std::string, int> seen;
    for (const auto& transition : model.transitions) {
        auto fromIt = blocks.find(transition.from);
        auto toIt = blocks.find(transition.to);
        if (fromIt == blocks.end() || toIt == blocks.end() || fromIt->second == toIt->second) continue;
        const Block* from = fromIt->second;
        const Block* to = toIt->second;
        std::string key = transition.from < transition.to ? Pele::cat({transition.from, " ", transition.to}) : Pele::cat({transition.to, " ", transition.from});
        int repeat = seen[key]++;

        double dx = to->cx - from->cx;
        double dy = to->cy - from->cy;
        double len = std::hypot(dx, dy);
        if (len == 0.0) len = 1.0;
        double mx = (from->cx + to->cx) / 2.0;
        double my = (from->cy + to->cy) / 2.0;
        double outward = (-dy * (mx - cx) + dx * (my - cy) >= 0.0) ? 1.0 : -1.0;
        double nx = (-dy / len) * outward;
        double ny = (dx / len) * outward;
        bool diagonal = (from != &confusion && to != &confusion && std::abs(dx) > 1.0 && std::abs(dy) > 1.0);
        double bow = (diagonal ? 2.0 * (std::hypot(rx * nx, ry * ny) + 16.0) : len * 0.15) + static_cast<double>(repeat) * 56.0;
        double px = mx + nx * bow;
        double py = my + ny * bow;

        auto point = [&](double t) -> std::pair<double, double> {
            double u = 1.0 - t;
            return {
                u * u * from->cx + 2.0 * u * t * px + t * t * to->cx,
                u * u * from->cy + 2.0 * u * t * py + t * t * to->cy
            };
        };
        auto crossing = [&](const Block* b, double lo, double hi, bool leaving) -> double {
            for (int i = 0; i < 16; i++) {
                double mid = (lo + hi) / 2.0;
                auto [ptX, ptY] = point(mid);
                if (inside(b, ptX, ptY) == leaving) lo = mid;
                else hi = mid;
            }
            return (lo + hi) / 2.0;
        };
        constexpr int STEPS = 48;
        int stepIdx = 1;
        while (stepIdx <= STEPS) {
            auto [ptX, ptY] = point(static_cast<double>(stepIdx) / STEPS);
            if (!inside(from, ptX, ptY)) break;
            stepIdx++;
        }
        if (stepIdx > STEPS) continue;
        double t0 = crossing(from, static_cast<double>(stepIdx - 1) / STEPS, static_cast<double>(stepIdx) / STEPS, true);
        while (stepIdx <= STEPS) {
            auto [ptX, ptY] = point(static_cast<double>(stepIdx) / STEPS);
            if (inside(to, ptX, ptY)) break;
            stepIdx++;
        }
        double t1 = stepIdx > STEPS ? 1.0 : crossing(to, static_cast<double>(stepIdx - 1) / STEPS, static_cast<double>(stepIdx) / STEPS, false);

        auto [sx, sy] = point(t0);
        auto [ex, ey] = point(t1);
        auto blossom = [&](double a, double b, double c) -> double {
            return (1.0 - t0) * (1.0 - t1) * a + ((1.0 - t0) * t1 + t0 * (1.0 - t1)) * b + t0 * t1 * c;
        };
        double qx = blossom(from->cx, px, to->cx);
        double qy = blossom(from->cy, py, to->cy);
        double el = std::hypot(ex - qx, ey - qy);
        if (el == 0.0) el = 1.0;
        double ux = (ex - qx) / el;
        double uy = (ey - qy) / el;
        std::string head = marker("arrow_point", ex, ey, ux, uy, "var(--_l)");
        ex -= ux * markerTrim("arrow_point");
        ey -= uy * markerTrim("arrow_point");

        std::string text;
        if (transition.hasLabel) {
            Label label = layoutLabel(transition.label, false, measurer, small, ITEM_MAX_WIDTH);
            auto [lx, ly] = point((t0 + t1) / 2.0);
            double away = (std::abs(nx) * label.width) / 2.0 + (std::abs(ny) * label.height) / 2.0 + 6.0;
            double half = label.width / 2.0 + 4.0;
            double tx = std::min(std::max(lx + nx * away, x0 + half), std::max(right - half, x0 + half));
            text = labelSvg(label,  tx,  ly + ny * away, Pele::cat({" class=\"pele-edge-label\" font-size=\"", num(small), "\""}),  icons);
        }
        Pele::appendCat(edges, {"<g class=\"pele-edge\" data-id=\"", transition.from, "-", transition.to, "\">"
            "<path d=\"M", num(sx), ",", num(sy), "Q", num(qx), ",", num(qy), " ", num(ex), ",", num(ey), "\" fill=\"none\" stroke=\"var(--_l)\"/>", head, text, "</g>"});
    }

    double totalWidth = std::ceil(std::max(width, title.width) + 2.0 * pad);
    double totalHeight = std::ceil(height + titleHeight + 2.0 * pad);
    std::string content = Pele::cat({labelSvg(title, x0 + width / 2.0, pad + title.height / 2.0, " class=\"pele-title\" font-weight=\"var(--_tw)\""), "<g class=\"pele-cynefin-regions\">", fills, "</g>", boundaries, ellipse, domains, (!edges.empty() ? Pele::cat({"<g class=\"pele-edges\">", edges, "</g>"}) : "")});

    return {
        svgDocument("cynefin", totalWidth, totalHeight, size, options, model, content),
        totalWidth,
        totalHeight,
        {}
    };
}

inline Rendered render(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options = {}) {
    CynefinModel model = buildCynefin(parseCynefin(source), title);
    return renderCynefinModel(model, config, options);
}

} // namespace CynefinDiagram

inline Rendered renderCynefin(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options = {}) {
    return CynefinDiagram::render(source, config, title, options);
}

} // namespace Pele
