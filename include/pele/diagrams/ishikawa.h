#pragma once
#include "../foundation.h"

namespace Pele {
namespace IshikawaDiagram {

struct IshikawaNode {
    std::string text;
    std::vector<std::shared_ptr<IshikawaNode>> children;
};

struct IshikawaModel : public DiagramMeta {
    std::shared_ptr<IshikawaNode> root;
};

class IshikawaDb : public IshikawaModel {
public:
    struct StackEntry {
        int level = 0;
        std::shared_ptr<IshikawaNode> node;
    };
    std::vector<StackEntry> stack;
    std::optional<int> baseLevel;

    void addNode(int rawLevel, const std::string& text) {
        auto node = std::make_shared<IshikawaNode>();
        node->text = text;
        if (!root) {
            root = node;
            stack = {{0, node}};
            return;
        }
        if (!baseLevel.has_value()) baseLevel = rawLevel;
        int level = std::max(rawLevel - *baseLevel + 1, 1);
        while (stack.size() > 1 && stack.back().level >= level) stack.pop_back();
        stack.back().node->children.push_back(node);
        stack.push_back({level, node});
    }
};

namespace T {
    enum Type {
        END = 0,
        EOF_TOK = 1,
        SPACELINE = 2,
        NL = 3,
        ISHIKAWA = 4,
        SPACELIST = 5,
        TEXT = 6
    };
}

inline const char* ishikawaTokenName(int t) noexcept {
    static const char* NAMES[] = {"$end", "EOF", "SPACELINE", "NL", "ISHIKAWA", "SPACELIST", "TEXT"};
    return (t >= 0 && t <= 6) ? NAMES[t] : "$end";
}

struct IshikawaTokens {
    std::vector<int> types;
    std::vector<int> starts;
    std::vector<int> ends;
};

inline int matchIshikawaKeyword(const std::string& src, int at) noexcept {
    static const char* word = "ishikawa-beta";
    int k = 0;
    while (k < 13) {
        int c = charCodeAt(src, at + k);
        int w = static_cast<unsigned char>(word[k]);
        if (c != w && (c + 32 != w || c < 65 || c > 90)) break;
        k++;
    }
    if (k == 13 && !isWord(charCodeAt(src, at + 13))) return 13;
    return (k >= 8 && !isWord(charCodeAt(src, at + 8))) ? 8 : 0;
}

inline IshikawaTokens tokenizeIshikawa(const std::string& src) {
    std::vector<int> types;
    std::vector<int> starts;
    std::vector<int> ends;
    int n = static_cast<int>(src.size());
    int i = 0;
    while (i < n) {
        int j = i;
        int lastBreak = -1;
        while (j < n) {
            size_t next = static_cast<size_t>(j);
            int cp = charCodeAtUtf8(src, static_cast<size_t>(j), &next);
            if (!isSpace(cp)) break;
            if (cp == 10) lastBreak = j;
            j = static_cast<int>(next);
        }
        int type = 0;
        int end = 0;
        if (charCodeAt(src, j) == 37 && charCodeAt(src, j + 1) == 37) {
            type = T::SPACELINE;
            end = j + 2;
            while (end < n) {
                size_t next = static_cast<size_t>(end);
                int cp = charCodeAtUtf8(src, static_cast<size_t>(end), &next);
                if (cp == 10 || cp == 13 || cp == 0x2028 || cp == 0x2029) break;
                end = static_cast<int>(next);
            }
        } else if (j == i) {
            int length = matchIshikawaKeyword(src, i);
            if (length > 0) {
                type = T::ISHIKAWA;
                end = i + length;
            } else {
                type = T::TEXT;
                size_t nl = src.find('\n', static_cast<size_t>(i));
                end = (nl == std::string::npos) ? n : static_cast<int>(nl);
            }
        } else if (lastBreak > i) {
            type = T::SPACELINE;
            end = lastBreak + 1;
        } else if (lastBreak == i) {
            type = T::NL;
            end = i + 1;
        } else {
            type = T::SPACELIST;
            end = j;
        }
        types.push_back(type);
        starts.push_back(i);
        ends.push_back(end);
        i = end;
    }
    types.push_back(T::EOF_TOK);
    types.push_back(T::END);
    starts.push_back(n);
    starts.push_back(n);
    ends.push_back(n);
    ends.push_back(n);
    return {std::move(types), std::move(starts), std::move(ends)};
}

inline void parseIshikawa(const std::string& src, IshikawaDb& db) {
    static const std::string STOP = "'SPACELINE', 'NL', 'EOF'";
    auto toks = tokenizeIshikawa(src);
    const auto& types = toks.types;
    const auto& starts = toks.starts;
    const auto& ends = toks.ends;
    size_t i = 0;

    auto fail = [&](const std::string& expected) {
        throw syntaxError("ishikawa",  src,  static_cast<size_t>(starts[i]), Pele::cat({"Expecting ", expected, ", got '", ishikawaTokenName(types[i]), "'"}));
    };

    if (types[i] == T::SPACELINE) {
        i++;
        while (types[i] == T::SPACELINE || types[i] == T::NL) i++;
        if (types[i] != T::ISHIKAWA) fail("'SPACELINE', 'NL', 'ISHIKAWA'");
    } else if (types[i] != T::ISHIKAWA) {
        fail("'SPACELINE', 'ISHIKAWA'");
    }
    i++;
    if (types[i] == T::NL) i++;

    for (;;) {
        int type = types[i];
        if (type == T::TEXT || (type == T::SPACELIST && types[i + 1] == T::TEXT)) {
            size_t text = (type == T::TEXT) ? i : (i + 1);
            i = text + 1;
            if (types[i] != T::SPACELINE && types[i] != T::NL && types[i] != T::EOF_TOK) fail(STOP);
            int indentLen = 0;
            if (type != T::TEXT) {
                indentLen = static_cast<int>(toUtf16(src.substr(starts[text - 1], ends[text - 1] - starts[text - 1])).size());
            }
            db.addNode(indentLen, trimStr(src.substr(starts[text], ends[text] - starts[text])));
        } else if (type == T::SPACELINE || type == T::SPACELIST) {
            i++;
            if (types[i] != T::SPACELINE && types[i] != T::NL && types[i] != T::EOF_TOK) {
                fail(type == T::SPACELIST ? Pele::cat({STOP, ", 'TEXT'"}) : STOP);
            }
        } else {
            fail("'SPACELINE', 'SPACELIST', 'TEXT'");
        }
        i++;
        while (types[i] == T::NL || types[i] == T::EOF_TOK) i++;
        if (types[i] == T::END) return;
    }
}

constexpr double SLANT = 0.5;
inline const double LENGTH = std::hypot(SLANT, 1.0);
constexpr double FIRST_ROW = 14.0;
constexpr double ROW_GAP = 6.0;
constexpr double BONE_END = 8.0;
constexpr double TWIG_STEP = 22.0;
constexpr double STUB = 18.0;
constexpr double TEXT_GAP = 5.0;
constexpr double PAIR_GAP = 26.0;
constexpr double HEAD_GAP = 34.0;
constexpr double TAIL = 28.0;
constexpr double BOX_PAD_X = 10.0;
constexpr double BOX_PAD_Y = 5.0;

struct Bone {
    Label label;
    int depth = 0;
    int parent = -1;
    int slot = 0;
    int kids = 0;
    double rise = 0.0;
    double at = 0.0;
    double textEnd = 0.0;
};

struct Category {
    std::shared_ptr<IshikawaNode> node;
    Label label;
    double boxW = 0.0;
    double boxH = 0.0;
    std::vector<Bone> bones;
    double rise = 0.0;
    double reach = 0.0;
};

struct Frame {
    std::shared_ptr<IshikawaNode> node;
    int depth = 0;
    int parent = -1;
    int slot = 0;
    int bone = -1;
};

inline Category arrangeCategory(const std::shared_ptr<IshikawaNode>& node, bool up, const TextMeasurer& measurer, double size, double small) noexcept {
    Label label = layoutLabel(node->text, false, measurer, size, 180.0);
    double lineHeight = std::round(small * 1.5);
    std::vector<Bone> bones;
    std::vector<int> rows;
    std::vector<Frame> stack;

    auto push = [&](const std::vector<std::shared_ptr<IshikawaNode>>& children, int depth, int parent) {
        int n = static_cast<int>(children.size());
        for (int slot = n - 1; slot >= 0; slot--) {
            stack.push_back({children[up ? (n - 1 - slot) : slot], depth, parent, slot, -1});
        }
    };
    push(node->children, 2, -1);
    while (!stack.empty()) {
        Frame frame = stack.back();
        stack.pop_back();
        if (frame.bone != -1) {
            rows.push_back(frame.bone);
            continue;
        }
        int index = static_cast<int>(bones.size());
        bones.push_back({
            layoutLabel(frame.node->text, false, measurer, small, 200.0),
            frame.depth,
            frame.parent,
            frame.slot,
            static_cast<int>(frame.node->children.size()),
            0.0,
            0.0,
            0.0
        });
        if (frame.depth % 2 == 0) {
            rows.push_back(index);
        } else {
            frame.bone = index;
            stack.push_back(frame);
        }
        push(frame.node->children, frame.depth + 1, index);
    }

    double rise = FIRST_ROW;
    for (int index : rows) {
        double height = std::max(bones[index].label.height, lineHeight) + ROW_GAP;
        bones[index].rise = rise + height / 2.0;
        rise += height;
    }
    double boxW = label.width + 2.0 * BOX_PAD_X;
    double reach = boxW / 2.0;
    for (auto& bone : bones) {
        const Bone* parent = bone.parent == -1 ? nullptr : &bones[bone.parent];
        if (bone.depth % 2 == 0) {
            bone.at = parent ? parent->at : 0.0;
            bone.textEnd = bone.at - (bone.kids > 0 ? static_cast<double>(bone.kids + 1) * TWIG_STEP : STUB) - TEXT_GAP;
        } else {
            bone.at = parent->at - static_cast<double>(parent->kids - bone.slot) * TWIG_STEP;
            bone.textEnd = bone.at - TEXT_GAP;
        }
        reach = std::max(reach, bone.label.width - bone.textEnd);
    }
    return {node, std::move(label), boxW, label.height + 2.0 * BOX_PAD_Y, std::move(bones), rise + BONE_END, reach};
}

inline double ishikawaTitleRoom(const RenderOptions& options) noexcept {
    double available = options.maxWidth;
    return available > 0.0 ? std::max(80.0, available - 2.0 * options.padding) : 4000.0;
}

inline Rendered verticalCauses(const IshikawaModel& model, const RenderOptions& options) noexcept {
    double size = options.fontSize;
    double small = std::round(size * 0.875);
    const auto& measurer = getMeasurer(options);
    double pad = options.padding;
    double maxW = options.maxWidth > 0.0 ? options.maxWidth : 320.0;
    double room = std::max(140.0, maxW - 2.0 * pad);
    Label title = layoutLabel(model.title, false, measurer, size, ishikawaTitleRoom(options), Style::Bold);

    struct OrderEntry {
        std::shared_ptr<IshikawaNode> node;
        int depth = 0;
        int parent = -1;
        double x = 0.0;
        double y = 0.0;
        double w = 0.0;
        double h = 0.0;
        Label label;
    };
    std::vector<OrderEntry> order;
    struct PendingEntry {
        std::shared_ptr<IshikawaNode> node;
        int depth = 0;
        int parent = -1;
    };
    std::vector<PendingEntry> pending;
    if (model.root) pending.push_back({model.root, 0, -1});
    int maxDepth = 0;
    while (!pending.empty()) {
        PendingEntry entry = pending.back();
        pending.pop_back();
        int index = static_cast<int>(order.size());
        order.push_back({entry.node, entry.depth, entry.parent, 0.0, 0.0, 0.0, 0.0, {}});
        maxDepth = std::max(maxDepth, entry.depth);
        for (int i = static_cast<int>(entry.node->children.size()) - 1; i >= 0; i--) {
            pending.push_back({entry.node->children[i], entry.depth + 1, index});
        }
    }
    double indent = std::min(20.0, std::max(2.0, (room - 120.0) / std::max(1, maxDepth)));
    double y = pad + (title.height > 0.0 ? title.height + 12.0 : 0.0);
    double width = title.width;
    std::string nodes;
    std::string edges;
    for (auto& entry : order) {
        entry.x = pad + entry.depth * indent;
        double labelRoom = std::max(60.0, room - entry.depth * indent - 20.0);
        entry.label = breakLabel(
            layoutLabel(entry.node->text, false, measurer, entry.depth < 2 ? size : small, labelRoom, entry.depth < 2 ? Style::Bold : 0),
            measurer,
            labelRoom
        );
        entry.w = entry.label.width + 20.0;
        entry.h = entry.label.height + 12.0;
        entry.y = y;
        y += entry.h + 12.0;
        width = std::max(width, entry.x - pad + entry.w);
        bool framed = entry.depth < 2;
        std::string cls = entry.depth == 0 ? "pele-ishikawa-head" : entry.depth == 1 ? "pele-ishikawa-category" : "pele-ishikawa-cause";
        Pele::appendCat(nodes, {"<g class=\"pele-node ", cls, "\" data-id=\"", escText(entry.node->text), "\">", (framed ? Pele::cat({"<rect x=\"", num(entry.x), "\" y=\"", num(entry.y), "\" width=\"", num(entry.w), "\" height=\"", num(entry.h), "\" rx=\"", (entry.depth == 0 ? "12" : "4"), "\" fill=\"var(--_s)\" stroke=\"var(--_b)\"/>"}) : ""), labelSvg(entry.label, entry.x + entry.w / 2.0, entry.y + entry.h / 2.0, std::string(" class=\"pele-label\"") + (entry.depth < 2 ? " font-weight=\"var(--_hw)\"" : "") + (entry.depth > 1 ? Pele::cat({" font-size=\"", num(small), "\""}) : "")), "</g>"});
        if (entry.parent >= 0) {
            const auto& p = order[entry.parent];
            double gx = p.x + indent / 2.0;
            Pele::appendCat(edges, {"<path class=\"pele-edge\" d=\"M", num(gx), ",", num(p.y + p.h), "V", num(entry.y + entry.h / 2.0), "H", num(entry.x), "\" fill=\"none\" stroke=\"var(--_b)\"/>"});
        }
    }
    double totalWidth = std::ceil(width + 2.0 * pad);
    double height = std::ceil(std::max(pad + title.height, y - 12.0) + pad);
    std::string content = labelSvg(title, totalWidth / 2.0, pad + title.height / 2.0, " class=\"pele-title\" font-weight=\"var(--_tw)\"") + edges + nodes;
    return {svgDocument("ishikawa", totalWidth, height, size, options, DiagramMeta{}, content), totalWidth, height, {}};
}

inline Rendered drawIshikawa(const IshikawaModel& model, const Config& /*config*/, const RenderOptions& options, bool down) noexcept {
    if (down) return verticalCauses(model, options);
    double size = options.fontSize;
    double small = std::round(size * 0.875);
    const auto& measurer = getMeasurer(options);
    double pad = options.padding;
    const auto& root = model.root;
    Label title = layoutLabel(model.title, false, measurer, size, 4000.0, Style::Bold);
    double titleHeight = title.height > 0.0 ? title.height + 12.0 : 0.0;

    std::string body;
    double minX = 0.0;
    double maxX = 0.0;
    double minY = 0.0;
    double maxY = 0.0;
    if (root) {
        Label head = layoutLabel(root->text, false, measurer, size, 200.0, Style::Bold);
        double headH = std::max(head.height + 24.0, 48.0);
        double headW = head.width + 24.0 + headH / 2.0;
        std::vector<Category> categories;
        categories.reserve(root->children.size());
        for (size_t i = 0; i < root->children.size(); i++) {
            categories.push_back(arrangeCategory(root->children[i], i % 2 == 0, measurer, size, small));
        }
        for (auto& cat : categories) cat.rise = std::max(cat.rise, headH / 2.0 + 14.0);

        std::vector<double> xs;
        double x = -HEAD_GAP;
        for (size_t i = 0; i < categories.size(); i += 2) {
            if (i > 0) {
                double room = 0.0;
                for (size_t side = 0; side < 2; side++) {
                    const Category* before = (i - 2 + side < categories.size()) ? &categories[i - 2 + side] : nullptr;
                    const Category* next = (i + side < categories.size()) ? &categories[i + side] : nullptr;
                    if (before) room = std::max(room, before->reach + (next ? next->boxW / 2.0 : 0.0));
                }
                x -= room + PAIR_GAP;
            }
            xs.push_back(x);
            xs.push_back(x);
        }
        double tail = !categories.empty() ? x - TAIL : -HEAD_GAP;

        maxX = headW;
        minX = tail;
        minY = -headH / 2.0;
        maxY = headH / 2.0;
        std::string groups;
        for (size_t i = 0; i < categories.size(); i++) {
            const auto& category = categories[i];
            double sign = (i % 2 == 0) ? -1.0 : 1.0;
            double ax = xs[i];
            auto px = [&](double at, double rise) { return ax + at - rise * SLANT; };
            double endX = px(0.0, category.rise);
            double endY = sign * category.rise;
            double trim = ARROW - 1.0;
            std::string d = Pele::cat({"M", num(ax - (SLANT / LENGTH) * trim), ",", num((sign * trim) / LENGTH), "L", num(endX), ",", num(endY)});
            std::string texts;
            for (const auto& bone : category.bones) {
                double y = sign * bone.rise;
                if (bone.depth % 2 == 0) {
                    Pele::appendCat(d, {"M", num(px(bone.at, bone.rise)), ",", num(y), "H", num(px(bone.textEnd + TEXT_GAP, bone.rise))});
                } else {
                    double from = category.bones[bone.parent].rise;
                    Pele::appendCat(d, {"M", num(px(bone.at, from)), ",", num(sign * from), "L", num(px(bone.at, bone.rise)), ",", num(y)});
                }
                double right = px(bone.textEnd, bone.rise);
                double labelX = std::floor((right - bone.label.width / 2.0) * 100.0 + 0.5) / 100.0;
                double labelY = std::floor(y * 100.0 + 0.5) / 100.0;
                texts += labelSvg(bone.label, labelX, labelY, bone.depth > 2 ? " fill=\"var(--_m)\"" : "");
                minX = std::min(minX, right - bone.label.width);
            }
            double boxY = endY + sign * (category.boxH / 2.0);
            minX = std::min(minX, endX - category.boxW / 2.0);
            maxX = std::max(maxX, endX + category.boxW / 2.0);
            minY = std::min(minY, boxY - category.boxH / 2.0);
            maxY = std::max(maxY, boxY + category.boxH / 2.0);
            Pele::appendCat(groups, {"<g class=\"pele-ishikawa-category\" data-id=\"", escText(category.node->text), "\">", "<path class=\"pele-ishikawa-bones\" d=\"", d, "\" fill=\"none\" stroke=\"var(--_l)\"/>", marker("arrow_point", ax, 0.0, SLANT / LENGTH, -sign / LENGTH, "var(--_l)"), "<rect x=\"", num(endX - category.boxW / 2.0), "\" y=\"", num(boxY - category.boxH / 2.0), "\" width=\"", num(category.boxW), "\" height=\"", num(category.boxH), "\" fill=\"var(--_s)\" stroke=\"var(--_b)\"", ROUNDED, "/>", labelSvg(category.label, endX, boxY, " class=\"pele-label\""), (!texts.empty() ? Pele::cat({"<g class=\"pele-ishikawa-causes\" font-size=\"", num(small), "\">", texts, "</g>"}) : ""), "</g>"});
        }

        double r = headH / 2.0;
        body = Pele::cat({"<path class=\"pele-ishikawa-spine\" d=\"M", num(tail), ",0H", num(-(ARROW - 1.0)), "\" fill=\"none\" stroke=\"var(--_l)\"/>", marker("arrow_point", 0.0, 0.0, 1.0, 0.0, "var(--_l)"), groups, "<g class=\"pele-ishikawa-head\" data-id=\"", escText(root->text), "\">", "<path d=\"M0,", num(-r), "H", num(headW - r), "A", num(r), ",", num(r), " 0 0 1 ", num(headW - r), ",", num(r), "H0Z\" fill=\"var(--_s)\" stroke=\"var(--_b)\" stroke-linejoin=\"round\"/>", labelSvg(head, 12.0 + head.width / 2.0, 0.0, " class=\"pele-label\" font-weight=\"var(--_hw)\""), "</g>"});
    }

    double inner = std::max(maxX - minX, title.width);
    double width = std::ceil(inner + 2.0 * pad);
    double height = std::ceil(maxY - minY + titleHeight + 2.0 * pad);
    double ox = pad + (inner - (maxX - minX)) / 2.0 - minX;
    double oy = pad + titleHeight - minY;
    std::string svg = svgDocument(
        "ishikawa",
        width,
        height,
        size,
        options,
        DiagramMeta{},
        labelSvg(title, width / 2.0, pad + title.height / 2.0, " class=\"pele-title\" font-weight=\"var(--_tw)\"") +
            (!body.empty() ? Pele::cat({"<g transform=\"translate(", num(ox), ",", num(oy), ")\">", body, "</g>"}) : "")
    );
    return {svg, width, height, {}};
}

inline Rendered renderIshikawaModel(const IshikawaModel& model, const Config& config, const RenderOptions& options) noexcept {
    Rendered across = drawIshikawa(model, config, options, false);
    double available = options.maxWidth;
    if (!(available > 0.0) || across.width <= available || available >= 640.0) return across;
    Rendered down = drawIshikawa(model, config, options, true);
    return down.width < across.width ? down : across;
}

inline Rendered render(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options) {
    IshikawaDb db;
    db.title = title;
    parseIshikawa(source, db);
    return renderIshikawaModel(db, config, options);
}

} // namespace IshikawaDiagram

inline Rendered renderIshikawa(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options) {
    return IshikawaDiagram::render(source, config, title, options);
}

inline Rendered renderIshikawa(const std::string& source, const RenderOptions& options = {}) {
    auto pre = preprocess(source);
    std::string parsed = Pele::cat({encodeEntities(pre.text), "\n"});
    RenderOptions opts = options;
    if (!opts.hasResponsive) {
        const auto* section = pre.config.get("ishikawa");
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
        auto res = IshikawaDiagram::render(parsed, pre.config, pre.title, opts);
        for (auto& link : res.links) link.id = decodeEntities(link.id);
        return res;
    });
}

} // namespace Pele
