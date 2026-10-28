#pragma once
#include "../foundation.h"

namespace Pele {
namespace KanbanDiagram {

struct KanbanNode {
    std::string id;
    int level = 0;
    std::string label;
    std::string icon;
    std::string assigned;
    std::string ticket;
    std::string priority;
    std::string cssClasses;
};

struct KanbanSection : public KanbanNode {
    std::vector<KanbanNode> items;
};

struct KanbanModel : public DiagramMeta {
    std::vector<KanbanSection> sections;
};

inline bool yamlTruthy(const YamlValue* v) {
    if (!v || v->isNull()) return false;
    if (v->isBool()) return v->bVal;
    if (v->isNumber()) return v->nVal != 0.0 && !std::isnan(v->nVal);
    if (v->isString()) return !v->sVal.empty();
    return true;
}

class KanbanDb : public KanbanModel, public outline::OutlineDb {
public:
    bool hasLast = false;
    bool lastIsSection = false;
    int sectionLevel = 0;
    bool hasMisplaced = false;
    std::string misplacedLabel;

    void addNode(int level, const std::string& id, const std::string& descr, int /*type*/, const std::optional<std::string>& shapeData = std::nullopt) override {
        KanbanNode node;
        node.id = id;
        node.level = level;
        node.label = descr;

        if (shapeData.has_value()) {
            std::string yaml = (shapeData->find('\n') != std::string::npos) ? (*shapeData + "\n") : ("{\n" + *shapeData + "\n}");
            YamlValue doc;
            try {
                doc = parseYaml(yaml);
            } catch (const std::exception& err) {
                throw PeleError("Metadata of \"" + node.id + "\" is not valid YAML. " + err.what(), "syntax", "kanban");
            }
            if (doc.isMap()) {
                const auto* sh = doc.get("shape");
                std::string shape = yamlTruthy(sh) ? sh->asString() : "";
                if (shape != toLowerAscii(shape) || shape.find('_') != std::string::npos) {
                    throw PeleError("No such shape: " + shape + ". Shape names should be lowercase.", "semantic", "kanban");
                }
                if (const auto* v = doc.get("label"); yamlTruthy(v)) node.label = v->asString();
                if (const auto* v = doc.get("icon"); yamlTruthy(v)) node.icon = v->asString();
                if (const auto* v = doc.get("assigned"); yamlTruthy(v)) node.assigned = v->asString();
                if (const auto* v = doc.get("ticket"); yamlTruthy(v)) node.ticket = v->asString();
                if (const auto* v = doc.get("priority"); yamlTruthy(v)) node.priority = v->asString();
            }
        }

        if (hasMisplaced) {
            throw PeleError("Items without section detected, found section (\"" + misplacedLabel + "\")", "semantic", "kanban");
        }
        if (sections.empty()) sectionLevel = level;
        hasLast = true;
        if (level == sectionLevel) {
            KanbanSection sec;
            static_cast<KanbanNode&>(sec) = std::move(node);
            sections.push_back(std::move(sec));
            lastIsSection = true;
        } else {
            if (level < sectionLevel) {
                hasMisplaced = true;
                misplacedLabel = node.label;
            }
            sections.back().items.push_back(std::move(node));
            lastIsSection = false;
        }
    }

    void decorateNode(const outline::OutlineDecoration& decoration) override {
        if (!hasLast || sections.empty()) return;
        KanbanNode& node = lastIsSection ? static_cast<KanbanNode&>(sections.back()) : sections.back().items.back();
        if (decoration.icon.has_value() && !decoration.icon->empty()) node.icon = *decoration.icon;
        if (decoration.className.has_value() && !decoration.className->empty()) node.cssClasses = *decoration.className;
    }
};

inline void parseKanban(const std::string& src, outline::OutlineDb& db) {
    outline::parseOutline(src, db, true);
}

constexpr double COLUMN_GAP = 12.0;
constexpr double COLUMN_PAD = 8.0;
constexpr double CARD_PAD = 10.0;
constexpr double CARD_GAP = 8.0;
constexpr double META_GAP = 4.0;
constexpr double ICON_GAP = 6.0;

inline std::string priorityStroke(const std::string& p) {
    if (p == "very high") return "stroke=\"" + CRITICAL + "\"";
    if (p == "high") return "stroke=\"" + CRITICAL + "\" opacity=\"0.5\"";
    if (p == "low") return "stroke=\"var(--_m)\" opacity=\"0.5\"";
    if (p == "very low") return "stroke=\"var(--_m)\"";
    return "";
}

inline Label fitKanbanLabel(const std::string& raw, bool markdown, const TextMeasurer& measurer, double size, double max, int base = 0) {
    Label label = layoutLabel(raw, markdown, measurer, size, max, base);
    if (label.width <= max) return label;
    std::vector<std::vector<Span>> lines;
    std::vector<double> widths;
    double widest = 0.0;
    std::vector<Span> line;
    double width = 0.0;
    auto commit = [&]() {
        lines.push_back(std::move(line));
        widths.push_back(width);
        widest = std::max(widest, width);
        line.clear();
        width = 0.0;
    };
    for (size_t i = 0; i < label.lines.size(); i++) {
        if (label.widths[i] <= max) {
            line = label.lines[i];
            width = label.widths[i];
            commit();
            continue;
        }
        for (const auto& span : label.lines[i]) {
            if (span.hasIcon || span.hasMath) {
                if (width > 0.0 && width + span.width > max) commit();
                line.push_back(span);
                width += span.width;
                continue;
            }
            std::string text;
            double from = width;
            size_t idx = 0;
            while (idx < span.text.size()) {
                size_t s = idx;
                nextCodepoint(span.text, idx);
                std::string ch = span.text.substr(s, idx - s);
                double w = measurer.width(ch, size, span.style);
                if (width > 0.0 && width + w > max) {
                    if (!text.empty()) {
                        Span sp;
                        sp.text = text;
                        sp.style = span.style;
                        sp.width = width - from;
                        line.push_back(std::move(sp));
                    }
                    commit();
                    text.clear();
                    from = 0.0;
                }
                text += ch;
                width += w;
            }
            if (!text.empty()) {
                Span sp;
                sp.text = text;
                sp.style = span.style;
                sp.width = width - from;
                line.push_back(std::move(sp));
            }
        }
        commit();
    }
    Label fitted = label;
    fitted.lines = std::move(lines);
    fitted.widths = std::move(widths);
    fitted.width = widest;
    fitted.height = static_cast<double>(fitted.lines.size()) * label.lineHeight;
    return label.hasHeights ? withHeights(std::move(fitted)) : fitted;
}

inline std::string textBlock(const Label& label, double x, double y, const std::string& attrs, const IconResolver& icons, bool end = false) {
    std::string out;
    for (size_t i = 0; i < label.lines.size(); i++) {
        const auto& line = label.lines[i];
        double cy = y + lineMiddle(label, i);
        if (line.size() == 1 && !line[0].hasIcon && !line[0].hasMath && (line[0].style & (Style::Sub | Style::Sup | Style::Underline)) == 0) {
            out += "<text" + textStyle(attrs, line[0].style) + " x=\"" + num(x) + "\" y=\"" + num(cy + label.size * 0.35) + "\"" + (end ? " text-anchor=\"end\"" : "") + ">" + esc(line[0].text) + "</text>";
        } else {
            double width = label.widths[i];
            out += labelSvg(labelLine(label, i), x + (end ? -width : width) / 2.0, cy, attrs, icons);
        }
    }
    return out;
}

inline double kanbanTitleRoom(const RenderOptions& options) {
    double available = options.maxWidth;
    return available > 0.0 ? std::max(80.0, available - 2.0 * options.padding) : 4000.0;
}

struct KanbanCard {
    const KanbanNode* item = nullptr;
    Label label;
    Label ticket;
    Label assigned;
    double meta = 0.0;
    double height = 0.0;
};

inline Rendered renderKanbanModel(const KanbanModel& model, const Config& config, const RenderOptions& options) {
    double size = options.fontSize;
    double small = std::round(size * 0.875);
    double line = std::round(size * 1.5);
    double smallLine = std::round(small * 1.5);
    const auto& measurer = getMeasurer(options);
    double pad = options.padding;
    const auto& icons = options.icons;
    const auto* conf = config.get("kanban");
    double glyph = small * 1.25;

    auto hasIcon = [](const std::string& name) {
        return !name.empty() && !iconMarkup(name).empty();
    };

    double mathWidth = 0.0;
    auto holds = [&](const std::string& raw, double fontSize, double reserved) {
        if (raw.empty() || raw.find("$$") == std::string::npos) return;
        for (const auto& spans : layoutLabel(raw, true, measurer, fontSize, std::numeric_limits<double>::infinity()).lines) {
            for (const auto& span : spans) {
                if (span.hasMath) mathWidth = std::max(mathWidth, span.math.width + reserved);
            }
        }
    };
    for (const auto& section : model.sections) {
        double cntW = measurer.width(std::to_string(section.items.size()), small, 0);
        holds(section.label, size, 2.0 * COLUMN_PAD + 8.0 + cntW + 12.0 + (hasIcon(section.icon) ? glyph + ICON_GAP : 0.0));
        for (const auto& item : section.items) {
            double reserved = 2.0 * (COLUMN_PAD + CARD_PAD);
            holds(item.label, small, reserved + (hasIcon(item.icon) ? glyph + ICON_GAP : 0.0));
            holds(item.ticket, small, reserved);
            holds(item.assigned, small, reserved);
        }
    }

    double cfgSecW = 200.0;
    if (conf && conf->isMap()) {
        if (const auto* sw = conf->get("sectionWidth"); sw && sw->isNumber() && sw->nVal > 0.0) {
            cfgSecW = std::min(std::max(sw->nVal, 80.0), 2000.0);
        }
    }
    double ownWidth = std::max(cfgSecW, std::ceil(mathWidth));
    double room = options.maxWidth > 0.0 ? (options.maxWidth - 2.0 * pad) : std::numeric_limits<double>::infinity();
    int count = static_cast<int>(model.sections.size());
    bool fits = static_cast<double>(count) * (ownWidth + COLUMN_GAP) - COLUMN_GAP <= room;
    int perRow = fits ? std::max(count, 1) : std::max(1, static_cast<int>(std::floor((room + COLUMN_GAP) / (ownWidth + COLUMN_GAP))));
    double columnWidth = fits
        ? ownWidth
        : std::max({std::min(ownWidth, room), std::min(std::floor((room + COLUMN_GAP) / perRow) - COLUMN_GAP, ownWidth * 2.0), std::ceil(mathWidth)});
    std::string baseUrl;
    if (conf && conf->isMap()) {
        if (const auto* tbu = conf->get("ticketBaseUrl"); tbu && tbu->isString()) baseUrl = tbu->sVal;
    }
    double cardWidth = columnWidth - 2.0 * COLUMN_PAD;
    double textWidth = cardWidth - 2.0 * CARD_PAD;
    std::vector<LinkInfo> links;

    const auto& sections = model.sections;
    std::vector<Label> titles;
    std::vector<double> countWidths;
    std::vector<std::vector<KanbanCard>> cards;
    double headHeight = line;
    std::vector<double> bodyHeights;

    for (const auto& section : sections) {
        double countWidth = measurer.width(std::to_string(section.items.size()), small, 0);
        double reserved = countWidth + 12.0 + (hasIcon(section.icon) ? glyph + ICON_GAP : 0.0);
        Label t = fitKanbanLabel(section.label, true, measurer, size, cardWidth - 8.0 - reserved, Style::Bold);
        headHeight = std::max(headHeight, t.height);
        countWidths.push_back(countWidth);
        titles.push_back(std::move(t));

        std::vector<KanbanCard> list;
        double height = 0.0;
        for (const auto& item : section.items) {
            Label label = fitKanbanLabel(item.label, true, measurer, small, textWidth - (hasIcon(item.icon) ? glyph + ICON_GAP : 0.0));
            Label ticket = fitKanbanLabel(item.ticket, false, measurer, small, textWidth);
            Label assigned = fitKanbanLabel(item.assigned, false, measurer, small, textWidth);
            double meta = (ticket.width + assigned.width + 8.0 > textWidth)
                ? (ticket.height + assigned.height)
                : std::max(ticket.height, assigned.height);
            double cardHeight = 2.0 * CARD_PAD + std::max(label.height, smallLine) + (meta > 0.0 ? META_GAP + meta : 0.0);
            list.push_back({&item, std::move(label), std::move(ticket), std::move(assigned), meta, cardHeight});
            height += cardHeight + CARD_GAP;
        }
        cards.push_back(std::move(list));
        bodyHeights.push_back(height);
    }
    headHeight += 2.0 * COLUMN_PAD;

    std::vector<double> rowHeights;
    std::vector<double> rowTops;
    double curY = 0.0;
    for (size_t first = 0; first < sections.size(); first += static_cast<size_t>(perRow)) {
        double tallest = CARD_GAP;
        for (size_t idx = first; idx < std::min(first + static_cast<size_t>(perRow), sections.size()); idx++) {
            tallest = std::max(tallest, bodyHeights[idx]);
        }
        rowTops.push_back(curY);
        rowHeights.push_back(headHeight + tallest);
        curY += headHeight + tallest + COLUMN_GAP;
    }
    double boardHeight = !rowHeights.empty() ? (rowTops.back() + rowHeights.back()) : 0.0;

    Label title = titleLabel(model.title, measurer, size, kanbanTitleRoom(options));
    double titleHeight = title.height > 0.0 ? title.height + 16.0 : 0.0;
    double boardWidth = std::max(static_cast<double>(std::min(static_cast<int>(sections.size()), perRow)) * (columnWidth + COLUMN_GAP) - COLUMN_GAP, 0.0);
    double width = std::max(boardWidth, title.width);
    double left = pad + (width - boardWidth) / 2.0;
    double edge = columnWidth - COLUMN_PAD - 4.0;

    std::string out;
    for (size_t index = 0; index < sections.size(); index++) {
        const auto& section = sections[index];
        size_t row = index / static_cast<size_t>(perRow);
        std::string body =
            "<rect width=\"" + num(columnWidth) + "\" height=\"" + num(rowHeights[row]) + "\" fill=\"var(--_s)\"" + ROUNDED + "/>" +
            textBlock(titles[index], COLUMN_PAD + 4.0, COLUMN_PAD, " class=\"pele-cluster-label\" font-weight=\"var(--_hw)\"", icons) +
            "<text class=\"pele-count\" x=\"" + num(edge) + "\" y=\"" + num(COLUMN_PAD + line / 2.0 + small * 0.35) + "\" text-anchor=\"end\" font-size=\"" + num(small) + "\" fill=\"var(--_m)\">" + std::to_string(section.items.size()) + "</text>";
        if (hasIcon(section.icon)) {
            body += iconSvg(section.icon, edge - countWidths[index] - ICON_GAP - glyph, COLUMN_PAD + (line - glyph) / 2.0, glyph, icons);
        }

        double y = headHeight;
        for (const auto& card : cards[index]) {
            const auto& item = *card.item;
            std::string priority = toLowerAscii(trimStr(item.priority));
            std::string color = priorityStroke(priority);
            std::string inner = "<rect width=\"" + num(cardWidth) + "\" height=\"" + num(card.height) + "\" fill=\"var(--_bg)\" stroke=\"var(--_b)\"" + ROUNDED + "/>";
            if (!color.empty()) {
                inner += "<path class=\"pele-priority\" d=\"M2.5,5V" + num(card.height - 5.0) + "\" fill=\"none\" " + color + " stroke-width=\"3\" stroke-linecap=\"round\"/>";
            }
            inner += textBlock(card.label, CARD_PAD, CARD_PAD, " class=\"pele-label\"", icons);
            if (hasIcon(item.icon)) {
                inner += iconSvg(item.icon, cardWidth - CARD_PAD - glyph, CARD_PAD + (smallLine - glyph) / 2.0, glyph, icons);
            }
            double bottom = card.height - CARD_PAD;
            if (!item.ticket.empty()) {
                double top = bottom - card.meta;
                std::string href;
                if (!baseUrl.empty()) {
                    std::string replacedUrl = baseUrl;
                    size_t tPos = replacedUrl.find("#TICKET#");
                    if (tPos != std::string::npos) {
                        replacedUrl.replace(tPos, 8, item.ticket);
                    }
                    href = linkUrl(sanitizeUrl(decodeEntities(replacedUrl)), options);
                }
                if (href.empty()) {
                    inner += textBlock(card.ticket, CARD_PAD, top, " class=\"pele-ticket\" fill=\"var(--_m)\"", icons);
                } else {
                    inner +=
                        "<a href=\"" + esc(href) + "\" target=\"_blank\"" + relAttr(options) + ">" +
                        textBlock(card.ticket, CARD_PAD, top, " class=\"pele-ticket\" fill=\"var(--_c)\"", icons) +
                        "</a>";
                    links.push_back({item.id, href, false});
                }
            }
            if (!item.assigned.empty()) {
                inner += textBlock(
                    card.assigned,
                    cardWidth - CARD_PAD,
                    bottom - card.assigned.height,
                    " class=\"pele-assigned\" fill=\"var(--_m)\"",
                    icons,
                    true
                );
            }
            std::string priClass = color.empty() ? "" : (" pele-priority-" + replaceAllStr(priority, " ", "-"));
            body +=
                "<g class=\"pele-node pele-card" + priClass + classNames(item.cssClasses) + "\" data-id=\"" + escText(item.id) +
                "\" transform=\"translate(" + num(COLUMN_PAD) + "," + num(y) + ")\" font-size=\"" + num(small) + "\">" + inner + "</g>";
            y += card.height + CARD_GAP;
        }

        out +=
            "<g class=\"pele-cluster pele-column" + classNames(section.cssClasses) + "\" data-id=\"" + escText(section.id) +
            "\" transform=\"translate(" + num(left + static_cast<double>(index % static_cast<size_t>(perRow)) * (columnWidth + COLUMN_GAP)) + "," +
            num(pad + titleHeight + rowTops[row]) + ")\">" + body + "</g>";
    }

    double totalWidth = std::ceil(width + 2.0 * pad);
    double totalHeight = std::ceil(boardHeight + titleHeight + 2.0 * pad);
    std::string content = labelSvg(title, totalWidth / 2.0, pad + title.height / 2.0, " class=\"pele-title\" font-weight=\"var(--_tw)\"") + out;

    return {
        svgDocument("kanban", totalWidth, totalHeight, size, options, DiagramMeta{}, content),
        totalWidth,
        totalHeight,
        std::move(links)
    };
}

inline Rendered render(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options) {
    KanbanDb db;
    db.title = title;
    parseKanban(source, db);
    return renderKanbanModel(db, config, options);
}

} // namespace KanbanDiagram

inline Rendered renderKanban(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options) {
    return KanbanDiagram::render(source, config, title, options);
}

inline Rendered renderKanban(const std::string& source, const RenderOptions& options = {}) {
    auto pre = preprocess(source);
    std::string parsed = encodeEntities(pre.text) + "\n";
    RenderOptions opts = options;
    if (!opts.hasResponsive) {
        const auto* section = pre.config.get("kanban");
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
            auto res = KanbanDiagram::render(parsed, pre.config, pre.title, opts);
            for (auto& link : res.links) link.id = decodeEntities(link.id);
            return res;
        });
    });
}

} // namespace Pele
