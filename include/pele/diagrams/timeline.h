#pragma once
#include "../foundation.h"

namespace Pele {
namespace TimelineDiagram {

struct TimelinePeriod {
    std::string text;
    std::vector<std::string> events;
    std::string section;
    int sectionIndex = -1;
};

struct TimelineModel : public DiagramMeta {
    std::string direction = "LR";
    std::vector<std::string> sections;
    std::vector<TimelinePeriod> periods;
};

inline std::string stripNewlineIndent(const std::string& s) noexcept {
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

class TimelineDb : public TimelineModel {
public:
    std::string currentSection;

    TimelineDb& getCommonDb() { return *this; }

    void setDiagramTitle(const std::string& text) { title = text; }

    void setAccTitle(const std::string& text) { accTitle = trimStartStr(text); }

    void setAccDescription(const std::string& text) { accDescr = stripNewlineIndent(text); }

    void setDirection(const std::string& dir) { direction = dir; }

    void addSection(const std::string& text) {
        currentSection = text;
        sections.push_back(text);
    }

    void addTask(const std::string& period, int /*length*/ = 0, const std::string& event = "") {
        TimelinePeriod p;
        p.text = period;
        if (!event.empty()) p.events.push_back(event);
        p.section = currentSection;
        p.sectionIndex = static_cast<int>(sections.size()) - 1;
        periods.push_back(std::move(p));
    }

    void addEvent(const std::string& event) {
        if (periods.empty()) {
            throw PeleError("An event needs a time period before it.", "semantic", "timeline");
        }
        periods.back().events.push_back(event);
    }
};

namespace T {
    enum Type {
        END = 0,
        EOF_TOK = 1,
        INVALID = 2,
        NEWLINE = 3,
        timeline = 4,
        timeline_lr = 5,
        timeline_td = 6,
        title = 7,
        acc_title = 8,
        acc_title_value = 9,
        acc_descr = 10,
        acc_descr_value = 11,
        acc_descr_multiline_value = 12,
        section = 13,
        period = 14,
        event = 15
    };
}

inline const char* timelineTokenName(int t) noexcept {
    static const char* NAMES[] = {
        "$end", "EOF", "INVALID", "NEWLINE", "timeline", "timeline_lr", "timeline_td",
        "title", "acc_title", "acc_title_value", "acc_descr", "acc_descr_value",
        "acc_descr_multiline_value", "section", "period", "event"
    };
    return (t >= 0 && t <= 15) ? NAMES[t] : "$end";
}

inline int skipSpaceUtf8(const std::string& src, int from) noexcept {
    size_t p = static_cast<size_t>(from);
    while (p < src.size()) {
        size_t next = p;
        int cp = charCodeAtUtf8(src, p, &next);
        if (!isSpace(cp)) break;
        p = next;
    }
    return static_cast<int>(p);
}

inline int skipTriviaUtf8(const std::string& src, int from) noexcept {
    int n = static_cast<int>(src.size());
    int p = from;
    while (p < n) {
        size_t next = static_cast<size_t>(p);
        int c = charCodeAtUtf8(src, static_cast<size_t>(p), &next);
        if (c == 37 && charCodeAt(src, p + 1) != 123) {
            p = lineEnd(src, p);
        } else if (c == 10) {
            return p;
        } else if (isSpace(c)) {
            p = static_cast<int>(next);
            while (p < n) {
                size_t n2 = static_cast<size_t>(p);
                int c2 = charCodeAtUtf8(src, static_cast<size_t>(p), &n2);
                if (c2 == 10 || !isSpace(c2)) break;
                p = static_cast<int>(n2);
            }
        } else if (c == 35) {
            p = lineEnd(src, p);
        } else {
            return p;
        }
    }
    return p;
}

inline int scanAccessibility(const std::string& src, int p, ScanTokens& out, int first) noexcept {
    bool descr = keywordAt(src, p, "accdescr");
    if (!descr && !keywordAt(src, p, "acctitle")) return p;
    int n = static_cast<int>(src.size());
    int q = skipSpaceUtf8(src, p + 8);
    int c = charCodeAt(src, q);
    auto emit = [&](int type, int s, int e) {
        out.types.push_back(type);
        out.texts.push_back(src.substr(s, e - s));
        out.starts.push_back(s);
    };
    if (c == 58) {
        q = skipSpaceUtf8(src, q + 1);
        int end = lineEnd(src, q);
        emit(descr ? first + 2 : first, p, q);
        emit(descr ? first + 3 : first + 1, q, end);
        return end;
    }
    if (!descr || c != 123) return p;
    q = skipSpaceUtf8(src, q + 1);
    while (charCodeAt(src, q) != 125) {
        size_t found = src.find('}', static_cast<size_t>(q));
        int end = found == std::string::npos ? n : static_cast<int>(found);
        emit(first + 4, q, end);
        if (q == n) return n;
        q = end;
    }
    return q + 1;
}

inline ScanTokens tokenizeTimeline(const std::string& src) {
    int n = static_cast<int>(src.size());
    ScanTokens out;
    int p = 0;

    auto emit = [&](int type, int e) {
        out.types.push_back(type);
        out.texts.push_back(src.substr(p, e - p));
        out.starts.push_back(p);
        p = e;
    };
    auto isSpaceAt = [&](int pos) {
        if (pos < 0 || pos >= n) return false;
        size_t dummy = static_cast<size_t>(pos);
        return isSpace(charCodeAtUtf8(src, static_cast<size_t>(pos), &dummy));
    };

    for (;;) {
        p = skipTriviaUtf8(src, p);
        if (p >= n) break;
        int c = charCodeAt(src, p);
        int e = p + 1;
        if (c == 10) {
            while (charCodeAt(src, e) == 10) e++;
            emit(T::NEWLINE, e);
            continue;
        }
        if (keywordAt(src, p, "timeline")) {
            e = p + 8;
            while (charCodeAt(src, e) == 32 || charCodeAt(src, e) == 9) e++;
            bool lr = keywordAt(src, e, "lr");
            if (e > p + 8 && (lr || keywordAt(src, e, "td")) && !isWord(charCodeAt(src, e + 2))) {
                emit(lr ? T::timeline_lr : T::timeline_td, e + 2);
                continue;
            }
            if (!isWord(charCodeAt(src, p + 8))) {
                emit(T::timeline, p + 8);
                continue;
            }
        }
        if (keywordAt(src, p, "title")) {
            e = p + 5;
            while (e < n && charCodeAt(src, e) != 10 && isSpaceAt(e)) {
                size_t next = static_cast<size_t>(e);
                charCodeAtUtf8(src, static_cast<size_t>(e), &next);
                e = static_cast<int>(next);
            }
            if (e == n || charCodeAt(src, e) == 10) {
                p = e;
                continue;
            }
            size_t afterSep = static_cast<size_t>(p + 5);
            int sep = charCodeAtUtf8(src, static_cast<size_t>(p + 5), &afterSep);
            if (sep != 13 && isSpace(sep)) {
                emit(T::title, lineEnd(src, static_cast<int>(afterSep)));
                continue;
            }
        }
        e = scanAccessibility(src, p, out, T::acc_title);
        if (e != p) {
            p = e;
            continue;
        }
        if (keywordAt(src, p, "section") && isSpaceAt(p + 7)) {
            size_t afterSec = static_cast<size_t>(p + 7);
            charCodeAtUtf8(src, static_cast<size_t>(p + 7), &afterSec);
            e = static_cast<int>(afterSec);
            int startSec = e;
            while (e < n && charCodeAt(src, e) != 58 && charCodeAt(src, e) != 10) e++;
            if (e > startSec) {
                emit(T::section, e);
                continue;
            }
        }
        if (c != 58) {
            e = p + 1;
            for (int k = charCodeAt(src, e); e < n && k != 35 && k != 58 && k != 10; k = charCodeAt(src, ++e)) {}
            emit(T::period, e);
            continue;
        }
        if (isSpaceAt(p + 1)) {
            size_t afterColonSpace = static_cast<size_t>(p + 1);
            charCodeAtUtf8(src, static_cast<size_t>(p + 1), &afterColonSpace);
            e = static_cast<int>(afterColonSpace);
            int startEv = e;
            for (; e < n; e++) {
                int k = charCodeAt(src, e);
                if (k == 10 || (k == 58 && isSpaceAt(e + 1))) break;
            }
            if (e > startEv) {
                emit(T::event, e);
                continue;
            }
        }
        emit(T::INVALID, p + 1);
    }

    if (out.starts.empty() || out.starts.back() < n) emit(T::EOF_TOK, n);
    emit(T::END, n);
    return out;
}

inline void parseTimeline(const std::string& src, TimelineDb& db) {
    static const std::string STATEMENT =
        "'EOF', 'SPACE', 'NEWLINE', 'title', 'acc_title', 'acc_descr', 'acc_descr_multiline_value', 'section', 'period', 'event'";
    auto toks = tokenizeTimeline(src);
    const auto& types = toks.types;
    const auto& texts = toks.texts;
    const auto& starts = toks.starts;

    auto fail = [&](size_t i, const std::string& expected) {
        std::string got = (types[i] == T::END) ? "1" : timelineTokenName(types[i]);
        throw syntaxError("timeline",  src,  static_cast<size_t>(starts[i]), Pele::cat({"Expecting ", expected, ", got '", got, "'"}));
    };

    int head = types[0];
    if (head != T::timeline && head != T::timeline_lr && head != T::timeline_td) {
        fail(0, "'timeline', 'timeline_lr', 'timeline_td'");
    }
    if (head != T::timeline) db.setDirection(head == T::timeline_lr ? "LR" : "TD");

    for (size_t i = 1; i < types.size(); i++) {
        const std::string& text = texts[i];
        switch (types[i]) {
            case T::NEWLINE:
                break;
            case T::title: {
                size_t p5 = 5;
                if (p5 < text.size()) nextCodepoint(text, p5);
                db.getCommonDb().setDiagramTitle(text.substr(p5));
                break;
            }
            case T::acc_title:
                db.getCommonDb().setAccTitle(trimStr(texts[++i]));
                break;
            case T::acc_descr:
                db.getCommonDb().setAccDescription(trimStr(texts[++i]));
                break;
            case T::acc_descr_multiline_value:
                db.getCommonDb().setAccDescription(trimStr(text));
                break;
            case T::section: {
                size_t p7 = 7;
                if (p7 < text.size()) nextCodepoint(text, p7);
                db.addSection(text.substr(p7));
                break;
            }
            case T::period:
                db.addTask(text, 0, "");
                break;
            case T::event: {
                size_t p1 = 1;
                if (p1 < text.size()) nextCodepoint(text, p1);
                db.addEvent(text.substr(p1));
                break;
            }
            case T::EOF_TOK:
                return;
            default:
                fail(i, STATEMENT);
        }
    }
}

// ============================================================================
// Port of src/diagrams/common/fit.ts (plainLabel, fitLabel)
// ============================================================================

inline size_t matchBrTag(const std::string& s, size_t pos) noexcept {
    if (pos + 3 >= s.size() || s[pos] != '<') return 0;
    if ((s[pos + 1] != 'b' && s[pos + 1] != 'B') || (s[pos + 2] != 'r' && s[pos + 2] != 'R')) return 0;
    size_t p = pos + 3;
    while (p < s.size()) {
        size_t next = p;
        int cp = charCodeAtUtf8(s, p, &next);
        if (!isSpace(cp)) break;
        p = next;
    }
    if (p < s.size() && s[p] == '/') p++;
    if (p < s.size() && s[p] == '>') return p + 1 - pos;
    return 0;
}

inline Label plainLabel(const std::string& raw, const TextMeasurer& measurer, double size, double max, int base = 0) noexcept {
    if (raw.empty()) return Label{};
    std::string text;
    if (raw.find('<') != std::string::npos) {
        text.reserve(raw.size() + 8);
        for (size_t i = 0; i < raw.size(); i++) {
            if (raw[i] == '<' && matchBrTag(raw, i) == 0) {
                text += "&lt;";
            } else {
                text.push_back(raw[i]);
            }
        }
    } else {
        text = raw;
    }
    if (text.find(":fa-") != std::string::npos) {
        text = replaceAllStr(text, ":fa-", "&colon;fa-");
    }
    return layoutLabel(text, false, measurer, size, max, base);
}

inline Label fitPlainLabel(const std::string& raw, const TextMeasurer& measurer, double size, double max, int base = 0) noexcept {
    Label label = plainLabel(raw, measurer, size, max, base);
    if (raw.empty() || label.width <= max) return label;
    std::string out;
    double width = 0.0;
    size_t i = 0;
    while (i < raw.size()) {
        std::string unit;
        bool isBr = false;
        bool isEntity = false;
        size_t brLen = matchBrTag(raw, i);
        if (brLen > 0) {
            unit = raw.substr(i, brLen);
            isBr = true;
            i += brLen;
        } else if (startsWith(raw, PLACEHOLDER_OPEN, i)) {
            size_t p = i + PLACEHOLDER_OPEN.size();
            if (startsWith(raw, "\xC2\xB0", p)) p += 2;
            size_t wStart = p;
            while (p < raw.size() && isWord(static_cast<unsigned char>(raw[p]))) p++;
            if (p > wStart && startsWith(raw, PLACEHOLDER_CLOSE, p)) {
                size_t end = p + PLACEHOLDER_CLOSE.size();
                unit = raw.substr(i, end - i);
                isEntity = true;
                i = end;
            }
        }
        if (unit.empty() && raw[i] == '&') {
            size_t p = i + 1;
            if (p < raw.size() && raw[p] == '#') p++;
            size_t wStart = p;
            while (p < raw.size() && isWord(static_cast<unsigned char>(raw[p]))) p++;
            if (p > wStart && p < raw.size() && raw[p] == ';') {
                unit = raw.substr(i, p + 1 - i);
                isEntity = true;
                i = p + 1;
            }
        }
        if (unit.empty()) {
            size_t s = i;
            nextCodepoint(raw, i);
            unit = raw.substr(s, i - s);
        }

        if (trimStr(unit).empty() || isBr) {
            width = 0.0;
        } else {
            double w = measurer.width(isEntity ? "x" : unit, size, base);
            if (width > 0.0 && width + w > max) {
                out.push_back('\n');
                width = 0.0;
            }
            width += w;
        }
        out += unit;
    }
    return plainLabel(out, measurer, size, max, base);
}

constexpr double PAD_X = 12.0;
constexpr double PAD_Y = 8.0;
constexpr double MIN_TEXT = 152.0;
constexpr double MAX_TEXT = 216.0;
constexpr double EVENT_TEXT_TD = 264.0;
constexpr double GAP = 20.0;
constexpr double AXIS = 20.0;
constexpr double SPINE = 14.0;
constexpr double EVENT_GAP = 8.0;
constexpr double SECTION_GAP = 8.0;
constexpr double TAIL = 12.0;

struct TimelineGroup {
    int section = -1;
    std::vector<int> periods;
};

inline std::string timelineBox(double x, double y, double w, double h, const std::string& attrs) noexcept {
    return Pele::cat({"<rect x=\"", num(x), "\" y=\"", num(y), "\" width=\"", num(w), "\" height=\"", num(h), "\"", rounded(attrs), "/>"});
}

inline Rendered drawTimeline(const TimelineModel& model, const Config& config, const RenderOptions& options, bool vertical) noexcept {
    static const std::string ON_COLOR = " fill=\"var(--_bg)\"";
    static const std::string LINE = " fill=\"none\" stroke=\"var(--_b)\"";

    double size = options.fontSize;
    double small = std::round(size * 0.875);
    const auto& measurer = getMeasurer(options);
    double pad = options.padding;
    bool multicolor = true;
    if (const auto* tlCfg = config.get("timeline"); tlCfg && tlCfg->isMap()) {
        if (const auto* dm = tlCfg->get("disableMulticolor"); dm && dm->isBool() && dm->bVal) {
            multicolor = false;
        }
    }
    const auto& periods = model.periods;
    bool hasSections = !model.sections.empty();

    std::vector<TimelineGroup> groups;
    for (size_t s = 0; s < model.sections.size(); s++) {
        groups.push_back({static_cast<int>(s), {}});
    }
    TimelineGroup leading{-1, {}};
    for (size_t i = 0; i < periods.size(); i++) {
        int sIdx = periods[i].sectionIndex;
        if (sIdx >= 0 && static_cast<size_t>(sIdx) < groups.size()) {
            groups[sIdx].periods.push_back(static_cast<int>(i));
        } else {
            leading.periods.push_back(static_cast<int>(i));
        }
    }
    if (!leading.periods.empty()) {
        groups.insert(groups.begin(), std::move(leading));
    }

    double room = (vertical && options.maxWidth > 0.0) ? (options.maxWidth - 2.0 * pad) : std::numeric_limits<double>::infinity();
    double taken = AXIS + SPINE * 2.0 + 4.0 * PAD_X;
    double periodText = std::min(MIN_TEXT, std::max(80.0, std::round((room - taken) * 0.38)));
    double eventText = std::min(EVENT_TEXT_TD, std::max(110.0, room - taken - periodText));
    double textW = periodText;
    auto eventTextW = [&]() { return vertical ? eventText : textW; };

    std::vector<Label> periodLabels(periods.size());
    std::vector<std::vector<Label>> eventLabels(periods.size());
    for (size_t i = 0; i < periods.size(); i++) {
        periodLabels[i] = plainLabel(periods[i].text, measurer, size, textW);
        eventLabels[i].reserve(periods[i].events.size());
        for (const auto& e : periods[i].events) {
            eventLabels[i].push_back(plainLabel(e, measurer, small, eventTextW()));
        }
    }

    double widest = 0.0;
    for (const auto& l : periodLabels) widest = std::max(widest, l.width);
    if (!vertical) {
        for (const auto& ls : eventLabels) for (const auto& l : ls) widest = std::max(widest, l.width);
    }
    bool overflow = widest > textW;
    if (vertical) {
        for (const auto& ls : eventLabels) for (const auto& l : ls) if (l.width > eventText) overflow = true;
    }
    if (overflow) {
        textW = std::min(std::max(std::ceil(widest), periodText), std::isinf(room) ? MAX_TEXT : periodText);
        for (size_t i = 0; i < periods.size(); i++) {
            periodLabels[i] = fitPlainLabel(periods[i].text, measurer, size, textW);
            for (size_t j = 0; j < periods[i].events.size(); j++) {
                eventLabels[i][j] = fitPlainLabel(periods[i].events[j], measurer, small, eventTextW());
            }
        }
    }

    double lineHeight = std::round(size * 1.5);
    double colW = textW + 2.0 * PAD_X;
    double eventW = eventTextW() + 2.0 * PAD_X;
    auto chipHeight = [&](const Label& label) { return std::max(label.height, lineHeight) + 2.0 * PAD_Y; };
    auto eventHeight = [&](const Label& label) { return std::max(label.height, std::round(small * 1.5)) + 2.0 * PAD_Y; };
    auto colorOf = [&](const TimelineGroup& group, int period) -> std::string {
        return group.section >= 0 ? seriesColor(group.section) : (hasSections ? "var(--_m)" : seriesColor(multicolor ? period : 0));
    };

    bool hasEvents = false;
    for (const auto& ls : eventLabels) if (!ls.empty()) hasEvents = true;
    int slots = 0;
    for (const auto& g : groups) slots += std::max(static_cast<int>(g.periods.size()), 1);

    double pitch = colW + GAP;
    bool drawn = !periods.empty();
    double bodyW = vertical
        ? (colW + (hasEvents ? (AXIS + SPINE * 2.0 + eventW) : (drawn ? (AXIS + TAIL) : 0.0)))
        : (slots > 0 ? (static_cast<double>(slots) * pitch - GAP + (drawn ? (TAIL + ARROW) : 0.0)) : 0.0);
    Label title = fitPlainLabel(model.title, measurer, size, std::max(bodyW, 320.0), Style::Bold);
    double titleH = title.height > 0.0 ? title.height + 16.0 : 0.0;
    double width = std::max(bodyW, title.width);
    double left = pad + (width - bodyW) / 2.0;
    double top = pad + titleH;

    auto sectionLabel = [&](const TimelineGroup& group, double w) {
        return fitPlainLabel(model.sections[group.section], measurer, size, w - 2.0 * PAD_X, Style::Bold);
    };
    auto header = [&](const TimelineGroup& group, const Label& label, double x, double y, double w, double h) {
        return timelineBox(x,  y,  w,  h, Pele::cat({" fill=\"", seriesColor(group.section), "\""})) +
               labelSvg(label,  x + w / 2.0,  y + h / 2.0, Pele::cat({" font-weight=\"var(--_hw)\"", ON_COLOR}));
    };
    auto periodSvg = [&](int i, const std::string& color, double x, double y, double h) {
        return timelineBox(x,  y,  colW,  h, Pele::cat({" fill=\"", color, "\""})) +
               labelSvg(periodLabels[i], x + colW / 2.0, y + h / 2.0, ON_COLOR);
    };
    auto eventSvg = [&](const Label& label, const std::string& color, double x, double y, double h) {
        return Pele::cat({"<g class=\"pele-node pele-event\">", timelineBox(x,  y,  eventW,  h, Pele::cat({" fill=\"", color, "\" fill-opacity=\"0.16\""})), labelSvg(label,  x + eventW / 2.0,  y + h / 2.0, Pele::cat({" font-size=\"", num(small), "\""})), "</g>"});
    };
    auto dotSvg = [](double x, double y, const std::string& color) {
        return Pele::cat({"<circle class=\"pele-dot\" cx=\"", num(x), "\" cy=\"", num(y), "\" r=\"4\" fill=\"", color, "\" stroke=\"var(--_bg)\" stroke-width=\"2\"/>"});
    };
    auto openGroup = [&](const TimelineGroup& group) -> std::string {
        return group.section >= 0 ? Pele::cat({"<g class=\"pele-cluster pele-section\" data-id=\"", escText(trimStr(model.sections[group.section])), "\">"}) : "";
    };
    auto openPeriod = [&](int i) -> std::string {
        return Pele::cat({"<g class=\"pele-node pele-period\" data-id=\"", escText(trimStr(periods[i].text)), "\">"});
    };

    std::string body;
    double height = 0.0;

    if (vertical) {
        double axisX = left + colW + AXIS;
        double spineX = axisX + SPINE;
        double eventsX = spineX + SPINE;
        double y = top;
        for (const auto& group : groups) {
            body += openGroup(group);
            if (group.section >= 0) {
                Label label = sectionLabel(group, bodyW);
                double h = chipHeight(label);
                body += header(group, label, left, y, bodyW, h);
                y += h + SECTION_GAP + 4.0;
            }
            for (int i : group.periods) {
                std::string color = colorOf(group, i);
                double h = chipHeight(periodLabels[i]);
                double first = !eventLabels[i].empty() ? eventHeight(eventLabels[i][0]) : h;
                double cy = y + std::max(h, first) / 2.0;
                std::string d = Pele::cat({"M", num(left + colW), ",", num(cy), "H", num(axisX)});
                std::string cards;
                double ey = cy - first / 2.0;
                double last = cy;
                for (const auto& label : eventLabels[i]) {
                    double eh = eventHeight(label);
                    last = ey + eh / 2.0;
                    Pele::appendCat(d, {"M", num(spineX), ",", num(last), "H", num(eventsX)});
                    cards += eventSvg(label, color, eventsX, ey, eh);
                    ey += eh + EVENT_GAP;
                }
                if (!cards.empty()) Pele::appendCat(d, {"M", num(axisX), ",", num(cy), "H", num(spineX), "V", num(last)});
                Pele::appendCat(body, {openPeriod(i), periodSvg(i, color, left, cy - h / 2.0, h), "<path class=\"pele-edge\" d=\"", d, "\"", LINE, "/>", dotSvg(axisX, cy, color), cards, "</g>"});
                y = std::max(cy + h / 2.0, ey - EVENT_GAP) + GAP;
            }
            if (group.periods.empty()) y += GAP - SECTION_GAP - 4.0;
            if (group.section >= 0) body += "</g>";
        }
        if (!groups.empty()) y -= GAP;
        if (drawn) {
            double tip = y + TAIL + ARROW;
            body = Pele::cat({"<path class=\"pele-axis\" d=\"M", num(axisX), ",", num(top), "V", num(tip - ARROW + 1.0), "\" fill=\"none\" stroke=\"var(--_l)\"/>", marker("arrow_point", axisX, tip, 0.0, 1.0, "var(--_l)"), body});
            y = tip;
        }
        height = y + pad;
    } else {
        double headerH = 0.0;
        std::vector<std::optional<Label>> sectionLabels(groups.size());
        for (size_t g = 0; g < groups.size(); g++) {
            if (groups[g].section < 0) continue;
            Label label = sectionLabel(groups[g], static_cast<double>(std::max(static_cast<int>(groups[g].periods.size()), 1)) * pitch - GAP);
            headerH = std::max(headerH, chipHeight(label));
            sectionLabels[g] = std::move(label);
        }
        double chipH = 0.0;
        for (const auto& label : periodLabels) chipH = std::max(chipH, chipHeight(label));
        double periodsY = top + (hasSections ? headerH + SECTION_GAP : 0.0);
        double axisY = periodsY + chipH + (chipH > 0.0 ? AXIS : 0.0);
        double eventsY = axisY + AXIS;
        double bottom = drawn ? (axisY + TAIL) : (top + headerH);
        int slot = 0;
        for (size_t g = 0; g < groups.size(); g++) {
            const auto& group = groups[g];
            int span = std::max(static_cast<int>(group.periods.size()), 1);
            double gx = left + static_cast<double>(slot) * pitch;
            body += openGroup(group);
            if (sectionLabels[g].has_value()) {
                body += header(group, *sectionLabels[g], gx, top, static_cast<double>(span) * pitch - GAP, headerH);
            }
            for (size_t k = 0; k < group.periods.size(); k++) {
                int i = group.periods[k];
                std::string color = colorOf(group, i);
                double x = gx + static_cast<double>(k) * pitch;
                double cx = x + colW / 2.0;
                std::string d = Pele::cat({"M", num(cx), ",", num(periodsY + chipH), "V", num(!eventLabels[i].empty() ? eventsY : axisY)});
                std::string cards;
                double ey = eventsY;
                for (const auto& eventLabel : eventLabels[i]) {
                    double eh = eventHeight(eventLabel);
                    if (ey > eventsY) Pele::appendCat(d, {"M", num(cx), ",", num(ey - EVENT_GAP), "V", num(ey)});
                    cards += eventSvg(eventLabel, color, x, ey, eh);
                    ey += eh + EVENT_GAP;
                }
                bottom = std::max(bottom, ey - EVENT_GAP);
                Pele::appendCat(body, {openPeriod(i), periodSvg(i, color, x, periodsY, chipH), "<path class=\"pele-edge\" d=\"", d, "\"", LINE, "/>", dotSvg(cx, axisY, color), cards, "</g>"});
            }
            if (group.section >= 0) body += "</g>";
            slot += span;
        }
        if (drawn) {
            double tip = left + bodyW;
            body = Pele::cat({"<path class=\"pele-axis\" d=\"M", num(left), ",", num(axisY), "H", num(tip - ARROW + 1.0), "\" fill=\"none\" stroke=\"var(--_l)\"/>", marker("arrow_point", tip, axisY, 1.0, 0.0, "var(--_l)"), body});
        }
        height = bottom + pad;
    }

    double totalWidth = std::ceil(width + 2.0 * pad);
    double totalHeight = std::ceil(std::max(height, pad * 2.0));
    std::string content =
        labelSvg(title, totalWidth / 2.0, pad + title.height / 2.0, " class=\"pele-title\" font-weight=\"var(--_tw)\"") + body;
    return {
        svgDocument("timeline", totalWidth, totalHeight, size, options, model, content),
        totalWidth,
        totalHeight,
        {}
    };
}

inline Rendered renderTimelineModel(const TimelineModel& model, const Config& config, const RenderOptions& options) noexcept {
    if (model.direction == "TD") return drawTimeline(model, config, options, true);
    Rendered across = drawTimeline(model, config, options, false);
    double available = options.maxWidth;
    if (!(available > 0.0) || across.width <= available || available >= 640.0) return across;
    Rendered down = drawTimeline(model, config, options, true);
    return down.width < across.width ? down : across;
}

inline Rendered render(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options) {
    TimelineDb db;
    db.title = title;
    parseTimeline(source, db);
    return renderTimelineModel(db, config, options);
}

} // namespace TimelineDiagram

inline Rendered renderTimeline(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options) {
    return TimelineDiagram::render(source, config, title, options);
}

inline Rendered renderTimeline(const std::string& source, const RenderOptions& options = {}) {
    auto pre = preprocess(source);
    std::string parsed = Pele::cat({encodeEntities(pre.text), "\n"});
    RenderOptions opts = options;
    if (!opts.hasResponsive) {
        const auto* section = pre.config.get("timeline");
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
        auto res = TimelineDiagram::render(parsed, pre.config, pre.title, opts);
        for (auto& link : res.links) link.id = decodeEntities(link.id);
        return res;
    });
}

} // namespace Pele
