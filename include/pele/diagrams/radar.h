#pragma once
#include "../foundation.h"

namespace Pele {
namespace RadarDiagram {

// ============================================================================
// src/diagrams/radar/parser.ts
// ============================================================================

namespace T {
enum Kind {
    radarColon = 0,
    radar = 1,
    showLegend = 2,
    graticule = 3,
    curve = 4,
    ticks = 5,
    axis = 6,
    max = 7,
    min = 8,
    colon = 9,
    comma = 10,
    open = 11,
    close = 12,
    openBrace = 13,
    closeBrace = 14,
    graticuleValue = 15,
    boolean = 16,
    accDescr = 17,
    accTitle = 18,
    title = 19,
    number = 20,
    string = 21,
    id = 22,
    newline = 23,
    whitespace = 24,
    yaml = 25,
    directive = 26,
    singleLineComment = 27
};
} // namespace T

inline int matchDigits(const std::string& src, int at) noexcept {
    int end = at;
    int n = static_cast<int>(src.size());
    while (end < n && src[end] >= '0' && src[end] <= '9') end++;
    return end;
}

inline int matchRadarNumber(const std::string& src, int at, int) noexcept {
    int whole = matchDigits(src, at);
    if (whole == at) return -1;
    bool dot = (charCodeAt(src, whole) == 46);
    if (dot) {
        int fraction = matchDigits(src, whole + 1);
        int length = fraction - whole - 1;
        if (length > 0 && charCodeAt(src, fraction) != 46) return fraction;
        if (length > 1) return fraction - 1;
    }
    if (charCodeAt(src, at) == 48) return at + 1;
    if (!dot) return whole;
    return whole - 1 > at ? whole - 1 : -1;
}

inline bool hasNameRest(const std::string& src, int at) noexcept {
    int n = static_cast<int>(src.size());
    int p = at;
    while (p < n) {
        int c = charCodeAt(src, p);
        if (isWord(c)) return true;
        if (c == '-') p++;
        else break;
    }
    return false;
}

inline int matchRadarId(const std::string& src, int at, int) noexcept {
    int n = static_cast<int>(src.size());
    if (at >= n || !isWord(charCodeAt(src, at))) return -1;
    int p = at + 1;
    int lastWord = p;
    while (p < n) {
        int c = charCodeAt(src, p);
        if (isWord(c)) {
            p++;
            lastWord = p;
        } else if (c == '-') {
            p++;
        } else {
            break;
        }
    }
    return lastWord;
}

inline TokenScanner makeOptionValueScanner(const std::vector<std::string>& words, int optionKind) noexcept {
    return [words, optionKind](const std::string& src, int at, int previous) -> int {
        int end = -1;
        for (const auto& w : words) {
            if (startsWith(src, w, static_cast<size_t>(at))) {
                end = at + static_cast<int>(w.size());
                break;
            }
        }
        if (end < 0) return -1;
        if (previous == optionKind) return end;
        return hasNameRest(src, end) ? -1 : end;
    };
}

inline TokenType makeRadarTitle() noexcept {
    TokenType t = common_tokens::makeTitle();
    t.match = [](const std::string& src, int at, int) -> int {
        int p = common_tokens::matchTabsSpaces(src, at);
        if (src.compare(p, 5, "title") != 0) return -1;
        p += 5;
        if (p < static_cast<int>(src.size()) && (src[p] == '\t' || src[p] == ' ')) {
            return common_tokens::matchRestOfLineBeforeComment(src, p + 1);
        }
        if (hasNameRest(src, p)) return -1;
        return p;
    };
    return t;
}

inline const std::vector<TokenType>& radarTokens() noexcept {
    static const std::vector<TokenType> types = []() {
        auto mark = [](const std::string& text) {
            TokenType t;
            t.name = text;
            t.literal = text;
            t.isRegex = false;
            return t;
        };
        auto word = [](const std::string& text) {
            TokenType t;
            t.name = text;
            t.literal = text;
            t.isRegex = false;
            t.longer = {T::id};
            return t;
        };

        TokenType radarKw = common_tokens::keyword("radar-beta");
        radarKw.longer = {T::id};

        TokenType gratVal;
        gratVal.name = "GRATICULE";
        gratVal.isRegex = true;
        gratVal.first = "cp";
        gratVal.match = makeOptionValueScanner({"circle", "polygon"}, T::graticule);

        TokenType boolVal;
        boolVal.name = "BOOLEAN";
        boolVal.isRegex = true;
        boolVal.first = "tf";
        boolVal.match = makeOptionValueScanner({"true", "false"}, T::showLegend);

        TokenType numTok;
        numTok.name = "NUMBER";
        numTok.isRegex = true;
        numTok.first = "0123456789";
        numTok.match = matchRadarNumber;

        TokenType idTok;
        idTok.name = "ID";
        idTok.isRegex = true;
        idTok.match = matchRadarId;

        return std::vector<TokenType>{
            mark("radar-beta:"),
            radarKw,
            word("showLegend"),
            word("graticule"),
            word("curve"),
            word("ticks"),
            word("axis"),
            word("max"),
            word("min"),
            mark(":"),
            mark(","),
            mark("["),
            mark("]"),
            mark("{"),
            mark("}"),
            gratVal,
            boolVal,
            common_tokens::makeAccDescr(),
            common_tokens::makeAccTitle(),
            makeRadarTitle(),
            numTok,
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

struct AxisAst {
    std::string name;
    std::optional<std::string> label;
};

struct EntryAst {
    std::optional<std::string> axisRef;
    double value = 0.0;
};

struct CurveAst {
    std::string name;
    std::optional<std::string> label;
    std::vector<EntryAst> entries;
};

struct OptionAst {
    std::string name;
    bool bVal = false;
    double nVal = 0.0;
    std::string sVal;
};

struct RadarAst {
    std::optional<std::string> title;
    std::optional<std::string> accTitle;
    std::optional<std::string> accDescr;
    std::vector<AxisAst> axes;
    std::vector<CurveAst> curves;
    std::vector<OptionAst> options;
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

inline RadarAst parseRadar(const std::string& src) {
    const auto& types = radarTokens();
    Tokens toks = tokenize(src, types, "radar");
    TokenReader r(toks, types, "radar");
    RadarAst ast;

    auto newlines = [&]() {
        while (r.kind() == T::newline) r.i++;
    };

    auto endOfLine = [&]() {
        if (r.kind() == -1) return;
        if (r.kind() != T::newline) r.fail("a line break or the end of input");
        newlines();
    };

    auto label = [&]() -> std::optional<std::string> {
        if (!r.accept(T::open)) return std::nullopt;
        std::string text = common_tokens::stringValue(r.expect(T::string));
        r.expect(T::close);
        return text;
    };

    auto entry = [&](bool detailed) -> EntryAst {
        if (!detailed) {
            return {std::nullopt, std::stod(r.expect(T::number))};
        }
        std::string axisRef = r.expect(T::id);
        r.accept(T::colon);
        return {axisRef, std::stod(r.expect(T::number))};
    };

    auto curve = [&]() -> CurveAst {
        std::string name = r.expect(T::id);
        auto text = label();
        CurveAst node{name, text, {}};
        r.expect(T::openBrace);
        newlines();
        if (r.kind() != T::number && r.kind() != T::id) r.fail("a number or an axis name");
        bool detailed = (r.kind() == T::id);
        node.entries.push_back(entry(detailed));
        while (r.accept(T::comma)) {
            newlines();
            node.entries.push_back(entry(detailed));
        }
        newlines();
        r.expect(T::closeBrace);
        return node;
    };

    auto axis = [&]() -> AxisAst {
        std::string name = r.expect(T::id);
        auto text = label();
        return {name, text};
    };

    auto option = [&]() -> OptionAst {
        int kind = r.kind();
        std::string name = r.take();
        OptionAst opt;
        opt.name = name;
        if (kind == T::showLegend) opt.bVal = (r.expect(T::boolean) == "true");
        else if (kind == T::graticule) opt.sVal = r.expect(T::graticuleValue);
        else opt.nVal = std::stod(r.expect(T::number));
        return opt;
    };

    auto isOption = [&]() -> bool {
        int k = r.kind();
        return k == T::showLegend || k == T::ticks || k == T::max || k == T::min || k == T::graticule;
    };

    newlines();
    if (!r.accept(T::radarColon)) {
        r.expect(T::radar);
        r.accept(T::colon);
    }
    newlines();
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
            case T::axis:
                r.i++;
                do ast.axes.push_back(axis());
                while (r.accept(T::comma));
                break;
            case T::curve:
                r.i++;
                do ast.curves.push_back(curve());
                while (r.accept(T::comma));
                break;
            default:
                if (!isOption()) r.fail("an axis, a curve, an option, a title, or a line break");
                ast.options.push_back(option());
                while (r.accept(T::comma)) {
                    if (!isOption()) r.fail("an option");
                    ast.options.push_back(option());
                }
        }
    }
    return ast;
}

// ============================================================================
// src/diagrams/radar/model.ts
// ============================================================================

struct RadarAxis {
    std::string name;
    std::string label;
};

struct RadarCurve {
    std::string name;
    std::string label;
    std::vector<double> entries;
};

struct RadarOptions {
    bool showLegend = true;
    int ticks = 5;
    std::optional<double> max;
    double min = 0.0;
    std::string graticule = "circle";
};

struct RadarModel : public DiagramMeta {
    std::vector<RadarAxis> axes;
    std::vector<RadarCurve> curves;
    RadarOptions options;
};

constexpr int MAX_TICKS = 32;

inline PeleError invalidRadar(const std::string& message) noexcept {
    return PeleError(message, "semantic", "radar");
}

inline RadarModel buildRadar(const RadarAst& ast, const std::string& fallbackTitle) {
    RadarModel model;
    for (const auto& a : ast.axes) {
        model.axes.push_back({a.name, a.label.value_or(a.name)});
    }

    for (const auto& c : ast.curves) {
        std::vector<double> entries;
        if (!c.entries[0].axisRef.has_value()) {
            for (const auto& e : c.entries) entries.push_back(e.value);
        } else {
            if (model.axes.empty()) throw invalidRadar("Axes must be populated before curves for reference entries");
            Pele::FlatMap<std::string, double> byAxis;
            for (const auto& e : c.entries) {
                if (e.axisRef.has_value() && !byAxis.count(*e.axisRef)) {
                    byAxis[*e.axisRef] = e.value;
                }
            }
            for (const auto& a : model.axes) {
                auto it = byAxis.find(a.name);
                if (it == byAxis.end()) throw invalidRadar(Pele::cat({"Missing entry for axis ", a.label}));
                entries.push_back(it->second);
            }
        }
        model.curves.push_back({c.name, c.label.value_or(c.name), std::move(entries)});
    }

    for (const auto& opt : ast.options) {
        if (opt.name == "showLegend") model.options.showLegend = opt.bVal;
        else if (opt.name == "graticule") model.options.graticule = opt.sVal;
        else if (opt.name == "ticks") model.options.ticks = static_cast<int>(opt.nVal);
        else if (opt.name == "max") model.options.max = opt.nVal;
        else if (opt.name == "min") model.options.min = opt.nVal;
    }
    if (model.options.ticks > MAX_TICKS) model.options.ticks = MAX_TICKS;

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
// src/diagrams/radar/render.ts
// ============================================================================

constexpr double SWATCH = 12.0;
constexpr double ROW = 26.0;
constexpr double GAP = 24.0;

struct Point {
    double x = 0.0;
    double y = 0.0;
};

inline double radarOption(const Config& config, const std::string& key, double fallback, double minVal, double maxVal) noexcept {
    const auto* r = config.get("radar");
    if (!r || !r->isMap()) return fallback;
    const auto* v = r->get(key);
    if (!v || !v->isNumber()) return fallback;
    return v->nVal >= minVal ? std::min(v->nVal, maxVal) : fallback;
}

inline double relativeRadius(double value, double minVal, double maxVal, double radius) noexcept {
    double clipped = std::min(std::max(value, minVal), maxVal);
    double scaled = radius * (clipped - minVal);
    return std::isfinite(scaled) ? scaled / (maxVal - minVal) : radius * ((clipped - minVal) / (maxVal - minVal));
}

inline std::string closedRoundCurve(const std::vector<Point>& points, double tension) noexcept {
    size_t n = points.size();
    std::string d = Pele::cat({"M", num(points[0].x), ",", num(points[0].y)});
    for (size_t i = 0; i < n; i++) {
        const auto& p0 = points[(i + n - 1) % n];
        const auto& p1 = points[i];
        const auto& p2 = points[(i + 1) % n];
        const auto& p3 = points[(i + 2) % n];
        Pele::appendCat(d, {" C", num(p1.x + (p2.x - p0.x) * tension), ",", num(p1.y + (p2.y - p0.y) * tension), " ", num(p2.x - (p3.x - p1.x) * tension), ",", num(p2.y - (p3.y - p1.y) * tension), " ", num(p2.x), ",", num(p2.y)});
    }
    return Pele::cat({d, " Z"});
}

inline std::string polygonPoints(const std::vector<Point>& points) noexcept {
    std::string out;
    for (const auto& p : points) {
        if (!out.empty()) out.push_back(' ');
        Pele::appendCat(out, {num(p.x), ",", num(p.y)});
    }
    return out;
}

inline double radarTitleRoom(const RenderOptions& options) noexcept {
    double available = options.maxWidth;
    double pad = options.hasPadding ? options.padding : 8.0;
    return available > 0 ? std::max(80.0, available - 2.0 * pad) : 4000.0;
}

template <typename DrawFn>
inline Rendered fitWidth(const RenderOptions& options, double natural, double least, DrawFn&& draw) noexcept {
    Rendered result = draw(natural, false);
    double available = options.maxWidth;
    if (!(available > 0) || result.width <= available) return result;
    double floor = std::min(least, natural);
    double plot = natural;
    result = draw(plot, true);
    for (int round = 0; round < 3 && result.width > available && plot > floor; round++) {
        plot = std::max(floor, plot - (result.width - available));
        result = draw(plot, true);
    }
    return result;
}

inline Rendered drawRadar(const RadarModel& model, const Config& config, const RenderOptions& options, double width, double height, bool tight) noexcept {
    double size = options.hasFontSize ? options.fontSize : 16.0;
    double small = std::round(size * 0.875);
    const TextMeasurer& measurer = getMeasurer(options);
    double pad = options.hasPadding ? options.padding : 8.0;

    double axisScale = radarOption(config, "axisScaleFactor", 1.0, 0.0, 10.0);
    double labelFactor = radarOption(config, "axisLabelFactor", 1.05, 0.0, 10.0);
    double tension = radarOption(config, "curveTension", 0.17, 0.0, 1.0);
    double radius = std::min(width, height) / 2.0;

    const auto& axes = model.axes;
    const auto& curves = model.curves;
    int ticks = model.options.ticks;
    const std::string& graticule = model.options.graticule;
    double minVal = model.options.min;
    size_t count = axes.size();
    constexpr double PI = 3.14159265358979323846;

    std::vector<Point> unit(count);
    for (size_t i = 0; i < count; i++) {
        double angle = (2.0 * PI * static_cast<double>(i)) / static_cast<double>(count) - PI / 2.0;
        unit[i] = {std::cos(angle), std::sin(angle)};
    }

    double maxVal = model.options.max.value_or(-std::numeric_limits<double>::infinity());
    if (!model.options.max.has_value()) {
        for (const auto& curve : curves) {
            for (double v : curve.entries) {
                if (v > maxVal) maxVal = v;
            }
        }
    }

    double x0 = -width / 2.0 - radarOption(config, "marginLeft", 16.0, 0.0, 2000.0);
    double x1 = width / 2.0 + radarOption(config, "marginRight", 16.0, 0.0, 2000.0);
    double y0 = -height / 2.0 - radarOption(config, "marginTop", 16.0, 0.0, 2000.0);
    double y1 = height / 2.0 + radarOption(config, "marginBottom", 16.0, 0.0, 2000.0);

    std::string grid;
    for (int i = 0; i < ticks; i++) {
        double r = std::min(radius, (radius * static_cast<double>(i + 1)) / static_cast<double>(ticks));
        if (graticule == "circle") {
            Pele::appendCat(grid, {"<circle r=\"", num(r), "\"/>"});
        } else {
            std::vector<Point> pts;
            pts.reserve(unit.size());
            for (const auto& u : unit) pts.push_back({u.x * r, u.y * r});
            Pele::appendCat(grid, {"<polygon points=\"", polygonPoints(pts), "\"/>"});
        }
    }

    std::string spokes;
    std::string labels;
    for (size_t i = 0; i < unit.size(); i++) {
        const auto& u = unit[i];
        double reach = radius * axisScale;
        Pele::appendCat(spokes, {"M0,0L", num(u.x * reach), ",", num(u.y * reach)});
        Label label = layoutLabel(axes[i].label, false, measurer, small, 4000.0);
        double at = radius * labelFactor + 6.0;
        double cx = u.x * at + (u.x > 0.01 ? label.width / 2.0 : u.x < -0.01 ? -label.width / 2.0 : 0.0);
        double cy = u.y * at + (u.y > 0.01 ? label.height / 2.0 : u.y < -0.01 ? -label.height / 2.0 : 0.0);
        Pele::appendCat(labels, {"<g class=\"pele-axis-label\" data-id=\"", escText(axes[i].name), "\">", labelSvg(label, cx, cy, ""), "</g>"});
        x0 = std::min(x0, cx - label.width / 2.0);
        x1 = std::max(x1, cx + label.width / 2.0);
        y0 = std::min(y0, cy - label.height / 2.0);
        y1 = std::max(y1, cy + label.height / 2.0);
    }

    std::string shapes;
    for (size_t index = 0; index < curves.size(); index++) {
        const auto& curve = curves[index];
        if (curve.entries.size() != count) continue;
        std::vector<Point> points(count);
        for (size_t i = 0; i < count; i++) {
            double r = relativeRadius(curve.entries[i], minVal, maxVal, radius);
            double safe = std::isfinite(r) ? r : 0.0;
            points[i] = {unit[i].x * safe, unit[i].y * safe};
        }
        std::string color = seriesColor(static_cast<int>(index));
        std::string attrs = Pele::cat({" class=\"pele-curve\" data-id=\"", escText(curve.name), "\" fill=\"", color, "\" stroke=\"", color, "\""});
        shapes += (graticule == "circle")
            ? Pele::cat({"<path", attrs, " d=\"", closedRoundCurve(points, tension), "\"/>"})
            : Pele::cat({"<polygon", attrs, " points=\"", polygonPoints(points), "\"/>"});
    }

    std::vector<Label> legend;
    if (model.options.showLegend) {
        for (const auto& curve : curves) {
            legend.push_back(layoutLabel(curve.label, false, measurer, small, 4000.0));
        }
    }
    double legendWidth = 0.0;
    for (const auto& label : legend) legendWidth = std::max(legendWidth, label.width);
    double legendHeight = static_cast<double>(legend.size()) * ROW;
    double legendX = tight ? -(SWATCH + 8.0 + legendWidth) / 2.0 : x1 + GAP;
    double legendTop = tight ? y1 + GAP / 2.0 : -legendHeight / 2.0;
    if (!legend.empty() && tight) {
        x0 = std::min(x0, legendX);
        x1 = std::max(x1, -legendX);
        y1 = legendTop + legendHeight;
    } else if (!legend.empty()) {
        x1 = legendX + SWATCH + 8.0 + legendWidth;
        y0 = std::min(y0, -legendHeight / 2.0);
        y1 = std::max(y1, legendHeight / 2.0);
    }

    std::string legendOut;
    for (size_t index = 0; index < legend.size(); index++) {
        const auto& label = legend[index];
        double y = legendTop + static_cast<double>(index) * ROW + ROW / 2.0;
        std::string color = seriesColor(static_cast<int>(index));
        Pele::appendCat(legendOut, {"<g class=\"pele-legend-item\" data-id=\"", escText(curves[index].name), "\">"
            "<rect x=\"", num(legendX), "\" y=\"", num(y - SWATCH / 2.0), "\" width=\"", num(SWATCH), "\" height=\"", num(SWATCH), "\" rx=\"2\" fill=\"", color, "\" fill-opacity=\"0.4\" stroke=\"", color, "\"/>", labelSvg(label, legendX + SWATCH + 8.0 + label.width / 2.0, y, ""), "</g>"});
    }

    Label title = layoutLabel(model.title, false, measurer, size, radarTitleRoom(options), Style::Bold);
    double titleHeight = title.height > 0.0 ? title.height + 8.0 : 0.0;
    double bodyWidth = std::max(x1 - x0, title.width);
    double totalWidth = std::ceil(bodyWidth + 2.0 * pad);
    double totalHeight = std::ceil(y1 - y0 + titleHeight + 2.0 * pad);
    std::string origin = Pele::cat({"translate(", num(pad + (bodyWidth - (x1 - x0)) / 2.0 - x0), ",", num(pad + titleHeight - y0), ")"});

    std::string content = Pele::cat({labelSvg(title, totalWidth / 2.0, pad + title.height / 2.0, " class=\"pele-title\" font-weight=\"var(--_tw)\""), "<g transform=\"", origin, "\">", (!grid.empty() ? Pele::cat({"<g class=\"pele-graticule\" fill=\"none\" stroke=\"var(--_a)\">", grid, "</g>"}) : ""), (!spokes.empty() ? Pele::cat({"<path class=\"pele-axes\" d=\"", spokes, "\" fill=\"none\" stroke=\"var(--_b)\"/>"}) : ""), (!shapes.empty() ? Pele::cat({"<g class=\"pele-curves\" fill-opacity=\"0.2\" stroke-width=\"1.5\" stroke-linejoin=\"round\">", shapes, "</g>"}) : ""), (!labels.empty() ? Pele::cat({"<g class=\"pele-axis-labels\" font-size=\"", num(small), "\">", labels, "</g>"}) : ""), (!legendOut.empty() ? Pele::cat({"<g class=\"pele-legend\" font-size=\"", num(small), "\">", legendOut, "</g>"}) : ""), "</g>"});

    return {
        svgDocument("radar", totalWidth, totalHeight, size, options, model, content),
        totalWidth,
        totalHeight,
        {}
    };
}

inline Rendered renderRadarModel(const RadarModel& model, const Config& config, const RenderOptions& options) noexcept {
    double natural = radarOption(config, "width", 320.0, 1.0, 4000.0);
    return fitWidth(options, natural, 160.0, [&](double width, bool tight) {
        return drawRadar(model, config, options, width, (radarOption(config, "height", 320.0, 1.0, 4000.0) * width) / natural, tight);
    });
}

inline Rendered render(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options = {}) {
    RadarModel model = buildRadar(parseRadar(source), title);
    return renderRadarModel(model, config, options);
}

} // namespace RadarDiagram

inline Rendered renderRadar(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options = {}) {
    return RadarDiagram::render(source, config, title, options);
}

} // namespace Pele
