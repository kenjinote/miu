#pragma once
#include "../foundation.h"

namespace Pele {
namespace PieDiagram {

struct PieSectionAst {
    std::string label;
    double value = 0.0;
};

struct PieAst {
    bool showData = false;
    std::string title;
    std::string accTitle;
    std::string accDescr;
    std::vector<PieSectionAst> sections;
};

struct PieModel : public DiagramMeta {
    bool showData = false;
    std::vector<std::pair<std::string, double>> sections;
};

namespace T {
    enum Type {
        showData = 0,
        pie = 1,
        colon = 2,
        number = 3,
        accDescr = 4,
        accTitle = 5,
        title = 6,
        string = 7,
        newline = 8,
        whitespace = 9,
        yaml = 10,
        directive = 11,
        singleLineComment = 12
    };
}

inline int scanDigits(const std::string& src, int from) {
    int i = from;
    for (int c = charCodeAt(src, i); c >= 48 && c <= 57; c = charCodeAt(src, ++i)) {}
    return i;
}

inline TokenScanner numberScanner() {
    int from = 0;
    int end = 0;
    int fraction = -1;
    return [from, end, fraction](const std::string& src, int p, int) mutable -> int {
        int at = charCodeAt(src, p) == 45 ? p + 1 : p;
        if (at < from || at >= end) {
            from = at;
            end = scanDigits(src, at);
            fraction = -1;
            if (end > at && charCodeAt(src, end) == 46) {
                int stop = scanDigits(src, end + 1);
                int length = stop - end - 1;
                if (length > 0 && charCodeAt(src, stop) != 46) fraction = stop;
                else if (length > 1) fraction = stop - 1;
            }
        }
        if (end == at) return -1;
        if (fraction >= 0) return fraction;
        if (charCodeAt(src, at) == 48) return charCodeAt(src, at + 1) == 46 ? -1 : at + 1;
        if (charCodeAt(src, end) != 46) return end;
        return end - 1 > at ? end - 1 : -1;
    };
}

inline const std::vector<TokenType>& pieTokens() {
    static const std::vector<TokenType> tokens = []() {
        TokenType colonTok;
        colonTok.name = ":";
        colonTok.literal = ":";

        TokenType numTok;
        numTok.name = "NUMBER_PIE";
        numTok.isRegex = true;
        numTok.scanner = numberScanner;
        numTok.first = "-0123456789";

        return std::vector<TokenType>{
            common_tokens::keyword("showData"),
            common_tokens::keyword("pie"),
            colonTok,
            numTok,
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
    return tokens;
}

inline PieAst parsePie(const std::string& src) {
    auto safeSingle = [](const std::string& text) {
        std::string s = trimStr(text);
        std::string out;
        int spaces = 0;
        for (size_t idx = 0; idx < s.size(); idx++) {
            char c = s[idx];
            if (c == '\t' || c == ' ') {
                spaces++;
            } else {
                if (spaces == 1) out.push_back(s[idx - 1]);
                else if (spaces >= 2) out.push_back(' ');
                spaces = 0;
                out.push_back(c);
            }
        }
        return out;
    };
    auto safeTitleValue = [&](const std::string& text) -> std::string {
        size_t pos = text.find("title");
        if (pos == std::string::npos) return "";
        return safeSingle(text.substr(pos + 5));
    };
    auto safeAccTitleValue = [&](const std::string& text) -> std::string {
        size_t pos = text.find(':');
        if (pos == std::string::npos) return "";
        return safeSingle(text.substr(pos + 1));
    };
    auto safeAccDescrValue = [&](const std::string& text) -> std::string {
        size_t brace = text.find('{');
        size_t colon = text.find(':');
        if (colon != std::string::npos && (brace == std::string::npos || colon < brace)) {
            return safeSingle(text.substr(colon + 1));
        }
        if (brace != std::string::npos) {
            size_t close = text.find('}', brace + 1);
            std::string body = text.substr(brace + 1, close == std::string::npos ? std::string::npos : close - brace - 1);
            return cleanAccDescr(body);
        }
        return "";
    };

    const auto& tokTypes = pieTokens();
    Tokens toks = tokenize(src, tokTypes, "pie");
    TokenReader r(toks, tokTypes, "pie");
    PieAst ast;

    auto endOfLine = [&]() {
        if (r.kind() == -1) return;
        if (r.kind() != T::newline) r.fail("a line break or the end of input");
        while (r.kind() == T::newline) r.i++;
    };

    while (r.kind() == T::newline) r.i++;
    r.expect(T::pie);
    ast.showData = r.accept(T::showData);
    while (r.kind() != -1) {
        switch (r.kind()) {
            case T::newline:
                r.i++;
                break;
            case T::accDescr:
                ast.accDescr = safeAccDescrValue(r.take());
                endOfLine();
                break;
            case T::accTitle:
                ast.accTitle = safeAccTitleValue(r.take());
                endOfLine();
                break;
            case T::title:
                ast.title = safeTitleValue(r.take());
                endOfLine();
                break;
            case T::string: {
                std::string label = common_tokens::stringValue(r.take());
                r.expect(T::colon);
                double value = std::stod(r.expect(T::number));
                ast.sections.push_back({label, value});
                endOfLine();
                break;
            }
            default:
                r.fail("a section, a title, or a line break");
        }
    }
    return ast;
}

inline std::string jsNumStr(double v) {
    if (std::isnan(v)) return "NaN";
    if (std::isinf(v)) return v < 0 ? "-Infinity" : "Infinity";
    if (v == 0.0) return "0";
    std::ostringstream oss;
    oss << std::setprecision(15) << v;
    return oss.str();
}

inline std::string stripNewlineIndent(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    size_t i = 0;
    while (i < s.size()) {
        if (s[i] == '\n') {
            out.push_back('\n');
            i++;
            while (i < s.size()) {
                size_t next = i;
                int cp = charCodeAtUtf8(s, i, &next);
                if (!isSpace(cp)) break;
                i = next;
            }
        } else {
            out.push_back(s[i++]);
        }
    }
    return out;
}

inline PieModel buildPie(const PieAst& ast, const std::string& title) {
    PieModel model;
    std::unordered_set<std::string> seen;
    for (const auto& sec : ast.sections) {
        if (sec.value < 0.0) {
            throw PeleError(
                "\"" + sec.label + "\" has invalid value: " + jsNumStr(sec.value) + ". Negative values are not allowed in pie charts. All slice values must be >= 0.",
                "semantic",
                "pie"
            );
        }
        if (!seen.count(sec.label)) {
            seen.insert(sec.label);
            model.sections.push_back({sec.label, sec.value});
        }
    }
    model.title = !ast.title.empty() ? ast.title : title;
    model.accTitle = !ast.accTitle.empty() ? trimStartStr(ast.accTitle) : "";
    model.accDescr = !ast.accDescr.empty() ? stripNewlineIndent(ast.accDescr) : "";
    model.showData = ast.showData;
    return model;
}

constexpr double PIE_RADIUS = 120.0;
constexpr double SWATCH = 12.0;
constexpr double ROW = 26.0;
constexpr double GAP = 32.0;
constexpr double OUTSIDE = 28.0;
constexpr double PI = 3.14159265358979323846;

inline const YamlValue* pieOption(const Config& config, const std::string& key) {
    const auto* pie = config.get("pie");
    if (!pie || !pie->isMap()) return nullptr;
    return pie->get(key);
}

inline std::string piePoint(double angle, double radius) {
    return num(std::sin(angle) * radius) + "," + num(-std::cos(angle) * radius);
}

inline std::string pieSlice(double start, double end, double outer, double inner) {
    if (end - start >= PI * 2.0 - 1e-9) {
        auto ring = [](double r, int sweep) {
            return "M0," + num(-r) + "A" + num(r) + "," + num(r) + " 0 1 " + std::to_string(sweep) + " 0," + num(r) + "A" + num(r) + "," + num(r) + " 0 1 " + std::to_string(sweep) + " 0," + num(-r) + "Z";
        };
        return ring(outer, 1) + (inner > 0.0 ? ring(inner, 0) : "");
    }
    int large = (end - start > PI) ? 1 : 0;
    if (inner <= 0.0) {
        return "M0,0L" + piePoint(start, outer) + "A" + num(outer) + "," + num(outer) + " 0 " + std::to_string(large) + " 1 " + piePoint(end, outer) + "Z";
    }
    return "M" + piePoint(start, outer) + "A" + num(outer) + "," + num(outer) + " 0 " + std::to_string(large) + " 1 " + piePoint(end, outer) +
           "L" + piePoint(end, inner) + "A" + num(inner) + "," + num(inner) + " 0 " + std::to_string(large) + " 0 " + piePoint(start, inner) + "Z";
}

inline double pieTitleRoom(const RenderOptions& options) {
    double available = options.maxWidth;
    return available > 0.0 ? std::max(80.0, available - 2.0 * options.padding) : 4000.0;
}

inline Rendered drawPie(const PieModel& model, const Config& config, const RenderOptions& options, double radius, bool tight) {
    double size = options.fontSize;
    double small = std::round(size * 0.875);
    const auto& measurer = getMeasurer(options);
    double pad = options.padding;

    const auto* posOpt = pieOption(config, "textPosition");
    double textPosition = (posOpt && posOpt->isNumber() && posOpt->nVal >= 0.0 && posOpt->nVal <= 1.0) ? posOpt->nVal : 0.75;
    const auto* holeOpt = pieOption(config, "donutHole");
    double inner = (holeOpt && holeOpt->isNumber() && holeOpt->nVal > 0.0 && holeOpt->nVal <= 0.9) ? holeOpt->nVal * radius : 0.0;
    const auto* legOpt = pieOption(config, "legendPosition");
    std::string placed = (legOpt && !legOpt->isNull()) ? legOpt->asString() : "right";
    std::string legendPosition = (tight && (placed == "left" || placed == "right")) ? "bottom" : placed;

    const auto& entries = model.sections;
    double sum = 0.0;
    for (const auto& [_, value] : entries) sum += value;

    std::vector<Label> legend;
    legend.reserve(entries.size());
    for (const auto& [label, value] : entries) {
        std::string txt = model.showData ? (label + " [" + jsNumStr(value) + "]") : label;
        legend.push_back(layoutLabel(txt, false, measurer, small, 4000.0));
    }
    double legendWidth = 0.0;
    for (const auto& label : legend) legendWidth = std::max(legendWidth, label.width);
    legendWidth += !legend.empty() ? SWATCH + 8.0 : 0.0;
    double legendHeight = static_cast<double>(legend.size()) * ROW;

    Label title = layoutLabel(model.title, false, measurer, size, pieTitleRoom(options), Style::Bold);
    double titleHeight = title.height > 0.0 ? title.height + 16.0 : 0.0;

    auto toFixed0 = [](double v) {
        return std::to_string(static_cast<long long>(std::llround(v)));
    };

    std::vector<double> shares(entries.size(), 0.0);
    std::vector<bool> narrow(entries.size(), false);
    bool anyNarrow = false;
    for (size_t i = 0; i < entries.size(); i++) {
        double value = entries[i].second;
        double share = sum > 0.0 ? value / sum : 0.0;
        shares[i] = (share * 100.0 >= 1.0 && toFixed0(share * 100.0) != "0") ? share : 0.0;
        if (shares[i] > 0.0 && shares[i] < 0.5) {
            double chord = 2.0 * radius * textPosition * std::sin(shares[i] * PI);
            if (chord < measurer.width(toFixed0(shares[i] * 100.0) + "%", small, 0) + 6.0) {
                narrow[i] = true;
                anyNarrow = true;
            }
        }
    }
    double margin = anyNarrow ? OUTSIDE : 0.0;

    std::string side = (legendPosition == "left" || legendPosition == "right" || legendPosition == "center") ? legendPosition : "";
    bool stacked = (legendPosition == "top" || legendPosition == "bottom");
    double diameter = (radius + margin) * 2.0;
    double width = diameter;
    double height = diameter;
    if (side == "left" || side == "right") {
        width += legendWidth > 0.0 ? GAP + legendWidth : 0.0;
        height = std::max(height, legendHeight);
    } else if (stacked) {
        width = std::max(width, legendWidth);
        height += legendHeight > 0.0 ? 20.0 + legendHeight : 0.0;
    }
    width = std::max(width, title.width);

    double cx = pad + (side == "left" ? legendWidth + GAP + diameter / 2.0 : side == "right" ? diameter / 2.0 : width / 2.0);
    double bodyTop = pad + titleHeight;
    double cy = bodyTop + (legendPosition == "top" ? legendHeight + 20.0 + diameter / 2.0 : stacked ? diameter / 2.0 : height / 2.0);

    std::string slices;
    std::string labels;
    std::string outside;
    double angle = 0.0;
    double rim = radius + OUTSIDE / 2.0;
    struct PlacedBox {
        double x;
        double y;
        double width;
    };
    std::vector<PlacedBox> placedOutside;
    auto clear = [&](double x, double y, double w) {
        for (const auto& other : placedOutside) {
            if (!(std::abs(x - other.x) >= (w + other.width) / 2.0 + 2.0 || std::abs(y - other.y) >= small + 2.0)) {
                return false;
            }
        }
        return true;
    };

    for (size_t index = 0; index < entries.size(); index++) {
        const auto& label = entries[index].first;
        double share = shares[index];
        if (share == 0.0) continue;
        double end = angle + share * PI * 2.0;
        slices += "<path class=\"pele-slice\" data-id=\"" + escText(label) + "\" d=\"" + pieSlice(angle, end, radius, inner) + "\" fill=\"" + seriesColor(static_cast<int>(index)) + "\"/>";
        double mid = (angle + end) / 2.0;
        std::string text = toFixed0(share * 100.0) + "%";
        if (narrow[index]) {
            double w = measurer.width(text, small, 0);
            double at = mid;
            for (int step = 0; step < 360 && !clear(std::sin(at) * rim, -std::cos(at) * rim, w); step++) {
                at += 2.0 / rim;
            }
            double x = std::sin(at) * rim;
            double y = -std::cos(at) * rim;
            placedOutside.push_back({x, y, w});
            outside += "<text x=\"" + num(x) + "\" y=\"" + num(y + small * 0.35) + "\">" + text + "</text>";
        } else {
            double r = radius * textPosition;
            labels += "<text x=\"" + num(std::sin(mid) * r) + "\" y=\"" + num(-std::cos(mid) * r + small * 0.35) + "\">" + text + "</text>";
        }
        angle = end;
    }
    if (slices.empty()) {
        slices = "<circle class=\"pele-slice\" r=\"" + jsNumStr(radius) + "\" fill=\"none\" stroke=\"var(--_b)\"/>";
    }

    std::string legendOut;
    double lx = 0.0;
    double ly = 0.0;
    if (side == "right") {
        lx = cx + diameter / 2.0 + GAP;
        ly = bodyTop + (height - legendHeight) / 2.0;
    } else if (side == "left") {
        lx = pad;
        ly = bodyTop + (height - legendHeight) / 2.0;
    } else if (side == "center") {
        lx = cx - legendWidth / 2.0;
        ly = cy - legendHeight / 2.0;
    } else {
        lx = pad + (width - legendWidth) / 2.0;
        ly = (legendPosition == "top") ? bodyTop : bodyTop + diameter + 20.0;
    }
    for (size_t index = 0; index < legend.size(); index++) {
        const auto& label = legend[index];
        double y = ly + static_cast<double>(index) * ROW + ROW / 2.0;
        legendOut +=
            "<g class=\"pele-legend-item\" data-id=\"" + escText(entries[index].first) + "\">" +
            "<rect x=\"" + num(lx) + "\" y=\"" + num(y - SWATCH / 2.0) + "\" width=\"" + num(SWATCH) + "\" height=\"" + num(SWATCH) + "\" rx=\"2\" fill=\"" + seriesColor(static_cast<int>(index)) + "\"/>" +
            labelSvg(label, lx + SWATCH + 8.0 + label.width / 2.0, y, "") +
            "</g>";
    }

    double totalWidth = std::ceil(width + 2.0 * pad);
    double totalHeight = std::ceil(height + titleHeight + 2.0 * pad);
    std::string content =
        labelSvg(title, totalWidth / 2.0, pad + title.height / 2.0, " class=\"pele-title\" font-weight=\"var(--_tw)\"") +
        "<g class=\"pele-slices\" transform=\"translate(" + num(cx) + "," + num(cy) + ")\" stroke=\"var(--_bg)\" stroke-width=\"1\" stroke-linejoin=\"round\">" + slices + "</g>" +
        (!labels.empty()
            ? ("<g class=\"pele-slice-labels\" transform=\"translate(" + num(cx) + "," + num(cy) + ")\" font-size=\"" + num(small) + "\" text-anchor=\"middle\" fill=\"var(--_bg)\">" + labels + "</g>")
            : "") +
        (!outside.empty()
            ? ("<g class=\"pele-slice-labels\" transform=\"translate(" + num(cx) + "," + num(cy) + ")\" font-size=\"" + num(small) + "\" text-anchor=\"middle\">" + outside + "</g>")
            : "") +
        (!legendOut.empty() ? ("<g class=\"pele-legend\" font-size=\"" + num(small) + "\">" + legendOut + "</g>") : "");

    return {
        svgDocument("pie", totalWidth, totalHeight, size, options, model, content),
        totalWidth,
        totalHeight,
        {}
    };
}

inline Rendered renderPieModel(const PieModel& model, const Config& config, const RenderOptions& options) {
    double natural = PIE_RADIUS * 2.0;
    double least = 140.0;
    Rendered result = drawPie(model, config, options, natural / 2.0, false);
    double available = options.maxWidth;
    if (!(available > 0.0) || result.width <= available) return result;
    double floor = std::min(least, natural);
    double plot = natural;
    result = drawPie(model, config, options, plot / 2.0, true);
    for (int round = 0; round < 3 && result.width > available && plot > floor; round++) {
        plot = std::max(floor, plot - (result.width - available));
        result = drawPie(model, config, options, plot / 2.0, true);
    }
    return result;
}

inline Rendered render(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options) {
    PieModel model = buildPie(parsePie(source), title);
    return renderPieModel(model, config, options);
}

} // namespace PieDiagram

inline Rendered renderPie(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options) {
    return PieDiagram::render(source, config, title, options);
}

inline Rendered renderPie(const std::string& source, const RenderOptions& options = {}) {
    auto pre = preprocess(source);
    std::string parsed = encodeEntities(pre.text) + "\n";
    RenderOptions opts = options;
    if (!opts.hasResponsive) {
        const auto* section = pre.config.get("pie");
        bool fixed = (section && section->isMap() && section->get("useMaxWidth") && section->get("useMaxWidth")->isBool() && !section->get("useMaxWidth")->bVal);
        opts.responsive = !fixed;
        opts.hasResponsive = true;
    }
    IconResolver wrappedIcons;
    if (opts.icons) {
        auto orig = opts.icons;
        wrappedIcons = [orig](const std::string& name) { return orig(decodeEntities(name)); };
        opts.icons = wrappedIcons;
    }
    return withIcons(opts.icons, [&]() {
        auto res = PieDiagram::render(parsed, pre.config, pre.title, opts);
        for (auto& link : res.links) link.id = decodeEntities(link.id);
        return res;
    });
}

} // namespace Pele
