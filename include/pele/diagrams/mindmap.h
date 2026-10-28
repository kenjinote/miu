#pragma once
#include "../foundation.h"

namespace Pele {
namespace MindmapDiagram {

struct MindmapNode {
    int id = 0;
    std::string nodeId;
    int level = 0;
    std::string descr;
    int type = 0;
    std::vector<int> children;
    bool isRoot = false;
    int section = 0;
    bool hasSection = false;
    std::string className;
    std::string icon;
};

struct MindmapModel : public DiagramMeta {
    std::vector<MindmapNode> nodes;
};

constexpr int SECTIONS = 11;

class MindmapDb : public MindmapModel, public outline::OutlineDb {
public:
    int baseLevel = 0;
    std::vector<int> open;

    void addNode(int level, const std::string& id, const std::string& descr, int type, const std::optional<std::string>& /*shapeData*/ = std::nullopt) override {
        bool isRoot = nodes.empty();
        if (isRoot) baseLevel = level;
        level -= baseLevel;

        int depth = static_cast<int>(open.size());
        while (depth > 0 && nodes[open[depth - 1]].level >= level) depth--;
        int parentIdx = depth > 0 ? open[depth - 1] : -1;
        if (parentIdx < 0 && !isRoot) {
            throw PeleError(
                "There can be only one root. No parent could be found for (\"" + descr + "\")",
                "semantic",
                "mindmap"
            );
        }
        int nodeIdNum = static_cast<int>(nodes.size());
        MindmapNode node;
        node.id = nodeIdNum;
        node.nodeId = id;
        node.level = level;
        node.descr = descr;
        node.type = type;
        node.isRoot = isRoot;
        if (parentIdx >= 0) {
            auto& parent = nodes[parentIdx];
            node.section = parent.isRoot ? (static_cast<int>(parent.children.size()) % SECTIONS) : parent.section;
            node.hasSection = parent.isRoot || parent.hasSection;
            parent.children.push_back(nodeIdNum);
        }
        open.resize(depth);
        open.push_back(nodeIdNum);
        nodes.push_back(std::move(node));
    }

    void decorateNode(const outline::OutlineDecoration& decoration) override {
        if (nodes.empty()) return;
        auto& node = nodes.back();
        if (decoration.icon.has_value() && !decoration.icon->empty()) node.icon = *decoration.icon;
        if (decoration.className.has_value() && !decoration.className->empty()) node.className = *decoration.className;
    }
};

inline void parseMindmap(const std::string& src, outline::OutlineDb& db) {
    outline::parseOutline(src, db, false);
}

constexpr double GAP_X = 36.0;
constexpr double GAP_Y = 8.0;
constexpr double GROUP_GAP = 18.0;
constexpr double BRANCH_GAP_X = 52.0;
constexpr double BRANCH_GAP_Y = 28.0;
constexpr double TEXT_PAD = 6.0;
constexpr double PILL_PAD = 12.0;
constexpr double ICON_GAP = 6.0;
constexpr double INDENT = 28.0;
constexpr double STEM = 12.0;
constexpr double BEND = 8.0;
constexpr double ROW_GAP = 10.0;

inline std::string leftLabelMindmap(const Label& label, double left, double cy, const IconResolver& icons) {
    if (label.lines.size() < 2) {
        return labelSvg(label, left + label.width / 2.0, cy, " class=\"pele-label\"", icons);
    }
    std::string out;
    double top = cy - label.height / 2.0;
    for (size_t k = 0; k < label.lines.size(); k++) {
        out += labelSvg(labelLine(label, k), left + label.widths[k] / 2.0, top + lineMiddle(label, k), " class=\"pele-label\"", icons);
    }
    return out;
}

inline double mindmapTitleRoom(const RenderOptions& options) {
    double available = options.maxWidth;
    return available > 0.0 ? std::max(80.0, available - 2.0 * options.padding) : 4000.0;
}

inline Rendered drawMindmap(const MindmapModel& model, const Config& config, const RenderOptions& options, bool down) {
    static const char* SHAPES[7] = {"", "rounded", "rect", "circle", "cloud", "bang", "hex"};
    static const std::string ROOT_SHAPE = "stadium";

    double size = options.fontSize;
    const auto& measurer = getMeasurer(options);
    double pad = options.padding;
    const auto& icons = options.icons;
    double wrap = 200.0;
    if (const auto* mmCfg = config.get("mindmap"); mmCfg && mmCfg->isMap()) {
        if (const auto* mnw = mmCfg->get("maxNodeWidth"); mnw && mnw->isNumber() && mnw->nVal > 0.0) {
            wrap = std::min(mnw->nVal, 2000.0);
        }
    }
    double glyph = size * 1.25;

    const auto& nodes = model.nodes;
    int n = static_cast<int>(nodes.size());
    std::vector<int> parent(n, 0);
    std::vector<int> depth(n, 0);
    for (int i = 0; i < n; i++) {
        for (int childId : nodes[i].children) {
            parent[childId] = i;
            depth[childId] = depth[i] + 1;
        }
    }

    double room = (down && options.maxWidth > 0.0) ? (options.maxWidth - 2.0 * pad) : std::numeric_limits<double>::infinity();
    std::vector<Label> labels(n);
    std::vector<std::string> shapes(n);
    std::vector<double> w(n, 0.0);
    std::vector<double> h(n, 0.0);
    std::vector<double> dy(n, 0.0);
    std::vector<double> anchor(n, 0.0);
    std::vector<uint8_t> drawn(n, 0);

    for (int i = 0; i < n; i++) {
        const auto& node = nodes[i];
        drawn[i] = (!node.icon.empty() && !iconMarkup(node.icon).empty()) ? 1 : 0;
        double fit = std::min(wrap, std::max(72.0, room - depth[i] * INDENT - 2.0 * TEXT_PAD - (drawn[i] ? glyph + ICON_GAP : 0.0)));
        const Label& label = (labels[i] = layoutLabel(node.descr, true, measurer, size, fit));
        double tw = label.width + (drawn[i] ? glyph + (label.width > 0.0 ? ICON_GAP : 0.0) : 0.0);
        double th = std::max(label.height, std::round(size * 1.5));
        std::string shape = (node.type >= 0 && node.type < 7 && SHAPES[node.type][0] != '\0')
            ? std::string(SHAPES[node.type])
            : (i == 0 ? ROOT_SHAPE : "");
        if (down && i == 0 && shape != "rect" && shape != "rounded") shape = ROOT_SHAPE;
        shapes[i] = shape;
        if (shape.empty()) {
            bool pill = !node.children.empty();
            w[i] = tw + 2.0 * (pill ? PILL_PAD : TEXT_PAD);
            h[i] = th + (pill ? 6.0 : 0.0);
            anchor[i] = h[i] / 2.0;
        } else {
            ShapeSize s = shapeSize(shape, tw, th);
            w[i] = s.w;
            h[i] = s.h;
            dy[i] = s.dy;
            anchor[i] = s.h / 2.0;
        }
    }

    std::vector<double> extent(n, 0.0);
    std::vector<double> rel(n, 0.0);
    std::vector<double> own(n, 0.0);
    std::vector<double> kids(n, 0.0);
    double stacked = 0.0;

    auto stack = [&](const std::vector<int>& list, int from, int to, double gap) -> double {
        double yVal = 0.0;
        double last = 0.0;
        for (int k = from; k < to; k++) {
            int c = list[k];
            rel[c] = yVal;
            bool grouped = !nodes[c].children.empty() || (k + 1 < to && !nodes[list[k + 1]].children.empty());
            last = grouped ? std::max(gap, GROUP_GAP) : gap;
            yVal += extent[c] + last;
        }
        stacked = yVal - last;
        int a = list[from];
        int b = list[to - 1];
        return (rel[a] + own[a] + anchor[a] + rel[b] + own[b] + anchor[b]) / 2.0;
    };

    for (int i = n - 1; i > 0; i--) {
        const auto& children = nodes[i].children;
        if (children.empty()) {
            extent[i] = h[i];
            continue;
        }
        double top = stack(children, 0, static_cast<int>(children.size()), GAP_Y) - anchor[i];
        double low = std::min(0.0, top);
        extent[i] = std::max(stacked, top + h[i]) - low;
        own[i] = top - low;
        kids[i] = -low;
    }

    std::vector<double> x(n, 0.0);
    std::vector<double> y(n, 0.0);
    std::vector<double> band(n, 0.0);
    std::vector<int> side(n, 0);
    std::vector<double> trunk(n, 0.0);

    if (down) {
        double row = 0.0;
        for (int i = 0; i < n; i++) {
            if (i > 0 && parent[i] == 0) row += ROW_GAP;
            x[i] = depth[i] * INDENT + w[i] / 2.0;
            y[i] = row;
            row += h[i] + ROW_GAP;
        }
    } else if (n > 0) {
        const auto& branches = nodes[0].children;
        int count = static_cast<int>(branches.size());
        double total = 0.0;
        for (int bId : branches) total += extent[bId];
        int split = count;
        double best = std::numeric_limits<double>::infinity();
        double right = 0.0;
        for (int k = 1; k < count; k++) {
            right += extent[branches[k - 1]];
            double diff = std::abs(2.0 * right - total + (2 * k - count) * BRANCH_GAP_Y);
            if (diff < best) {
                best = diff;
                split = k;
            }
        }
        for (int from = 0, to = split, dir = 1; from < count; from = to, to = count, dir = -1) {
            double middle = stack(branches, from, to, BRANCH_GAP_Y);
            for (int k = from; k < to; k++) {
                int c = branches[k];
                band[c] = rel[c] - middle;
                side[c] = dir;
            }
        }
        y[0] = -h[0] / 2.0;
        for (int i = 0; i < n; i++) {
            if (i > 0) y[i] = band[i] + own[i];
            for (int c : nodes[i].children) {
                if (i > 0) {
                    side[c] = side[i];
                    band[c] = band[i] + kids[i] + rel[c];
                }
                x[c] = x[i] + side[c] * (w[i] / 2.0 + (i > 0 ? GAP_X : BRANCH_GAP_X) + w[c] / 2.0);
            }
        }
    }

    double minX = 0.0;
    double maxX = 0.0;
    double minY = 0.0;
    double maxY = 0.0;
    std::string out;
    std::string edges;
    std::string body;
    std::string color;
    auto flush = [&](int section) {
        if (body.empty()) return;
        out +=
            "<g class=\"pele-branch pele-section-" + std::to_string(section) + "\">" +
            "<g class=\"pele-edges\" fill=\"none\" stroke=\"" + color + "\" stroke-width=\"1.5\">" + edges + "</g>" + body + "</g>";
        edges.clear();
        body.clear();
    };

    for (int k = 1; k <= n; k++) {
        int i = k % n;
        const auto& node = nodes[i];
        const auto& shape = shapes[i];
        const auto& label = labels[i];
        double cx = x[i];
        double top = y[i];
        minX = std::min(minX, cx - w[i] / 2.0);
        maxX = std::max(maxX, cx + w[i] / 2.0);
        minY = std::min(minY, top);
        maxY = std::max(maxY, top + h[i]);

        if (i == 0 || parent[i] == 0) flush(nodes[k - 1].hasSection ? nodes[k - 1].section : 0);
        if (i > 0 && down) {
            if (parent[i] == 0) color = seriesColor(node.hasSection ? node.section : 0);
            int p = parent[i];
            double stem = (p == 0) ? std::min(std::max(STEM, h[0] / 2.0), INDENT - BEND) : STEM;
            double x1 = x[p] - w[p] / 2.0 + std::min(stem, w[p] / 2.0);
            double y1 = trunk[p] != 0.0 ? trunk[p] : (y[p] + h[p]);
            double left = cx - w[i] / 2.0;
            double x2 = left + (!shape.empty() ? shapeInset(shape, w[i], h[i], 3) : 0.0);
            double y2 = top + anchor[i];
            double bend = std::max(0.0, std::min({BEND, x2 - x1, y2 - y1}));
            trunk[p] = y2 - bend;
            edges += "<path class=\"pele-edge\" d=\"M" + num(x1) + "," + num(y1) + "V" + num(y2 - bend) + "Q" + num(x1) + "," + num(y2) + " " + num(x1 + bend) + "," + num(y2) + "H" + num(x2) + "\"/>";
        } else if (i > 0) {
            if (parent[i] == 0) color = seriesColor(node.hasSection ? node.section : 0);
            int p = parent[i];
            int dir = side[i];
            int from = dir > 0 ? 1 : 3;
            double x1 = x[p] + dir * (w[p] / 2.0 - (!shapes[p].empty() ? shapeInset(shapes[p], w[p], h[p], from) : 0.0));
            double y1 = y[p] + anchor[p];
            double near = cx - (dir * w[i]) / 2.0;
            double x2 = near + (!shape.empty() ? dir * shapeInset(shape, w[i], h[i], 4 - from) : 0.0);
            double y2 = top + anchor[i];
            std::string mid = num((x1 + x2) / 2.0);
            edges += "<path class=\"pele-edge\" d=\"M" + num(x1) + "," + num(y1) + "C" + mid + "," + num(y1) + " " + mid + "," + num(y2) + " " + num(x2) + "," + num(y2) + "\"/>";
        }

        bool pill = shape.empty() && !node.children.empty();
        std::string tint = " fill=\"" + color + "\" fill-opacity=\"0.22\" stroke=\"" + color + "\"";
        std::string inner;
        if (!shape.empty()) {
            inner = i > 0
                ? drawShape(shape, w[i], h[i], tint, " stroke=\"" + color + "\"")
                : drawShape(shape, w[i], h[i], " fill=\"var(--_s)\" stroke=\"var(--_b)\"", " stroke=\"var(--_b)\"");
        } else if (pill) {
            inner = "<rect x=\"" + num(-w[i] / 2.0) + "\" y=\"" + num(-h[i] / 2.0) + "\" width=\"" + num(w[i]) + "\" height=\"" + num(h[i]) + "\" rx=\"" + num(h[i] / 2.0) + "\"" + tint + "/>";
        }
        double shift = 0.0;
        if (!node.icon.empty() && drawn[i]) {
            shift = (glyph + (label.width > 0.0 ? ICON_GAP : 0.0)) / 2.0;
            inner += iconSvg(node.icon, -shift - label.width / 2.0, dy[i] - glyph / 2.0, glyph, icons);
        }
        std::string shapeClass = !shape.empty() ? shape : (pill ? "branch" : "text");
        std::string group =
            "<g class=\"pele-node pele-shape-" + shapeClass + (i > 0 ? "" : " pele-root") + classNames(node.className) + "\" data-id=\"" + escText(node.nodeId) +
            "\" transform=\"translate(" + num(cx) + "," + num(top + h[i] / 2.0) + ")\">" +
            inner +
            (down && shape.empty() ? leftLabelMindmap(label, shift - label.width / 2.0, dy[i], icons) : labelSvg(label, shift, dy[i], " class=\"pele-label\"", icons)) +
            "</g>";
        if (i > 0) body += group;
        else out += group;
    }

    Label title = titleLabel(model.title, measurer, size, mindmapTitleRoom(options));
    double titleHeight = title.height > 0.0 ? title.height + 16.0 : 0.0;
    double width = std::max(maxX - minX, title.width);
    double totalWidth = std::ceil(width + 2.0 * pad);
    double totalHeight = std::ceil(maxY - minY + titleHeight + 2.0 * pad);
    std::string content =
        labelSvg(title, totalWidth / 2.0, pad + title.height / 2.0, " class=\"pele-title\" font-weight=\"var(--_tw)\"") +
        "<g transform=\"translate(" + num(pad - minX + (width - maxX + minX) / 2.0) + "," + num(pad + titleHeight - minY) + ")\">" + out + "</g>";

    return {
        svgDocument("mindmap", totalWidth, totalHeight, size, options, DiagramMeta{}, content),
        totalWidth,
        totalHeight,
        {}
    };
}

inline Rendered renderMindmapModel(const MindmapModel& model, const Config& config, const RenderOptions& options) {
    Rendered across = drawMindmap(model, config, options, false);
    double available = options.maxWidth;
    if (!(available > 0.0) || across.width <= available || available >= 640.0) return across;
    Rendered down = drawMindmap(model, config, options, true);
    return down.width < across.width ? down : across;
}

inline Rendered render(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options) {
    MindmapDb db;
    db.title = title;
    parseMindmap(source, db);
    return renderMindmapModel(db, config, options);
}

} // namespace MindmapDiagram

inline Rendered renderMindmap(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options) {
    return MindmapDiagram::render(source, config, title, options);
}

inline Rendered renderMindmap(const std::string& source, const RenderOptions& options = {}) {
    auto pre = preprocess(source);
    std::string parsed = encodeEntities(pre.text) + "\n";
    RenderOptions opts = options;
    if (!opts.hasResponsive) {
        const auto* section = pre.config.get("mindmap");
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
    return withMath(opts.math, [&]() {
        return withIcons(opts.icons, [&]() {
            auto res = MindmapDiagram::render(parsed, pre.config, pre.title, opts);
            for (auto& link : res.links) link.id = decodeEntities(link.id);
            return res;
        });
    });
}

} // namespace Pele
