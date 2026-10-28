#pragma once
#include "../foundation.h"

namespace Pele {
namespace JourneyDiagram {

struct JourneyTask {
    std::string task;
    double score = std::numeric_limits<double>::quiet_NaN();
    std::vector<std::string> people;
    std::string section;
};

struct JourneyModel : public DiagramMeta {
    std::vector<std::string> sections;
    std::vector<JourneyTask> tasks;
};

inline std::vector<std::string> journeyActors(const std::vector<JourneyTask>& tasks) {
    std::unordered_set<std::string> seen;
    std::vector<std::string> names;
    for (const auto& t : tasks) {
        for (const auto& name : t.people) {
            if (!seen.count(name)) {
                seen.insert(name);
                names.push_back(name);
            }
        }
    }
    std::sort(names.begin(), names.end(), [](const std::string& a, const std::string& b) {
        return toUtf16(a) < toUtf16(b);
    });
    return names;
}

inline double jsNumber(const std::string& raw) {
    std::string s = trimStr(raw);
    if (s.empty()) return 0.0;
    if (s == "Infinity" || s == "+Infinity") return std::numeric_limits<double>::infinity();
    if (s == "-Infinity") return -std::numeric_limits<double>::infinity();
    try {
        size_t idx = 0;
        double v = std::stod(s, &idx);
        if (idx == s.size()) return v;
    } catch (...) {}
    return std::numeric_limits<double>::quiet_NaN();
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

class JourneyDb : public JourneyModel {
public:
    std::string currentSection;

    void setDiagramTitle(const std::string& text) { title = text; }

    void setAccTitle(const std::string& text) { accTitle = trimStartStr(text); }

    void setAccDescription(const std::string& text) { accDescr = stripNewlineIndent(text); }

    void addSection(const std::string& text) {
        currentSection = text;
        sections.push_back(text);
    }

    void addTask(const std::string& task, const std::string& data) {
        auto pieces = splitStr(data.substr(1), ':');
        std::vector<std::string> people;
        if (pieces.size() > 1) {
            for (const auto& name : splitStr(pieces[1], ',')) {
                people.push_back(trimStr(name));
            }
        }
        tasks.push_back({task, jsNumber(pieces[0]), std::move(people), currentSection});
    }

    std::vector<std::string> getActors() const {
        return journeyActors(tasks);
    }
};

namespace T {
    enum Type {
        END = 0,
        EOF_TOK = 1,
        INVALID = 2,
        NEWLINE = 3,
        journey = 4,
        title = 5,
        acc_title = 6,
        acc_title_value = 7,
        acc_descr = 8,
        acc_descr_value = 9,
        acc_descr_multiline_value = 10,
        section = 11,
        taskName = 12,
        taskData = 13,
        colon = 14
    };
}

inline const char* journeyTokenName(int t) {
    static const char* NAMES[] = {
        "$end", "EOF", "INVALID", "NEWLINE", "journey", "title",
        "acc_title", "acc_title_value", "acc_descr", "acc_descr_value",
        "acc_descr_multiline_value", "section", "taskName", "taskData", ":"
    };
    return (t >= 0 && t <= 14) ? NAMES[t] : "$end";
}

inline int textEnd(const std::string& src, int from, bool colon) {
    int n = static_cast<int>(src.size());
    int e = from;
    for (; e < n; e++) {
        int c = charCodeAt(src, e);
        if (c == 35 || c == 10 || c == 59 || (colon && c == 58)) break;
    }
    return e;
}

inline int skipSpaceUtf8(const std::string& src, int from) {
    size_t p = static_cast<size_t>(from);
    while (p < src.size()) {
        size_t next = p;
        int cp = charCodeAtUtf8(src, p, &next);
        if (!isSpace(cp)) break;
        p = next;
    }
    return static_cast<int>(p);
}

inline int skipTriviaUtf8(const std::string& src, int from) {
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

inline int scanAccessibility(const std::string& src, int p, ScanTokens& out, int first) {
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

inline ScanTokens tokenizeJourney(const std::string& src) {
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
        if (keywordAt(src, p, "journey") && !isWord(charCodeAt(src, p + 7))) {
            emit(T::journey, p + 7);
            continue;
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
                int startTitle = static_cast<int>(afterSep);
                e = textEnd(src, startTitle, false);
                if (e > startTitle) {
                    emit(T::title, e);
                    continue;
                }
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
            int startSec = static_cast<int>(afterSec);
            e = textEnd(src, startSec, true);
            if (e > startSec) {
                emit(T::section, e);
                continue;
            }
        }
        if (c == 58) {
            e = textEnd(src, p + 1, false);
            emit(e > p + 1 ? T::taskData : T::colon, e > p + 1 ? e : p + 1);
        } else if (c == 59) {
            emit(T::INVALID, p + 1);
        } else {
            emit(T::taskName, textEnd(src, p + 1, true));
        }
    }

    if (out.starts.empty() || out.starts.back() < n) emit(T::EOF_TOK, n);
    emit(T::END, n);
    return out;
}

inline void parseJourney(const std::string& src, JourneyDb& db) {
    static const std::string STATEMENT =
        "'EOF', 'SPACE', 'NEWLINE', 'title', 'acc_title', 'acc_descr', 'acc_descr_multiline_value', 'section', 'taskName'";
    auto toks = tokenizeJourney(src);
    const auto& types = toks.types;
    const auto& texts = toks.texts;
    const auto& starts = toks.starts;

    auto fail = [&](size_t i, const std::string& expected) {
        std::string got = (types[i] == T::END) ? "1" : journeyTokenName(types[i]);
        throw syntaxError("journey", src, static_cast<size_t>(starts[i]), "Expecting " + expected + ", got '" + got + "'");
    };

    if (types[0] != T::journey) fail(0, "'journey'");

    for (size_t i = 1; i < types.size(); i++) {
        const std::string& text = texts[i];
        switch (types[i]) {
            case T::NEWLINE:
                break;
            case T::title: {
                size_t p5 = 5;
                if (p5 < text.size()) nextCodepoint(text, p5);
                db.setDiagramTitle(text.substr(p5));
                break;
            }
            case T::acc_title:
                db.setAccTitle(trimStr(texts[++i]));
                break;
            case T::acc_descr:
                db.setAccDescription(trimStr(texts[++i]));
                break;
            case T::acc_descr_multiline_value:
                db.setAccDescription(trimStr(text));
                break;
            case T::section: {
                size_t p7 = 7;
                if (p7 < text.size()) nextCodepoint(text, p7);
                db.addSection(text.substr(p7));
                break;
            }
            case T::taskName:
                if (types[++i] != T::taskData) fail(i, "'taskData'");
                db.addTask(text, texts[i]);
                break;
            case T::EOF_TOK:
                return;
            default:
                fail(i, STATEMENT);
        }
    }
}

inline size_t matchBrTag(const std::string& s, size_t pos) {
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

inline Label plainLabel(const std::string& raw, const TextMeasurer& measurer, double size, double max, int base = 0) {
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

inline Label fitPlainLabel(const std::string& raw, const TextMeasurer& measurer, double size, double max, int base = 0) {
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

constexpr double PAD_X = 10.0;
constexpr double PAD_Y = 8.0;
constexpr double MIN_TEXT = 124.0;
constexpr double MAX_TEXT = 200.0;
constexpr double GAP = 12.0;
constexpr double BAND_PAD = 6.0;
constexpr double GUTTER = 24.0;
constexpr double LEVEL = 26.0;
constexpr double FACE = 11.0;
constexpr double DOT = 5.0;
constexpr double DOT_STEP = 13.0;
constexpr double LEGEND_GAP = 20.0;

inline std::string faceSvg(double x, double y, const std::string& mood) {
    static const std::string EYES = "<circle cx=\"-3.6\" cy=\"-2.8\" r=\"1.3\"/><circle cx=\"3.6\" cy=\"-2.8\" r=\"1.3\"/>";
    std::string mouth = (mood == "happy") ? "M-5,2A6,6 0 0 0 5,2" : (mood == "sad") ? "M-5,6.5A6,6 0 0 1 5,6.5" : "M-4.5,4.5H4.5";
    return "<g class=\"pele-face pele-face-" + mood + "\" transform=\"translate(" + num(x) + "," + num(y) + ")\">"
           "<circle r=\"11\" fill=\"var(--_bg)\" stroke=\"var(--_l)\"/>"
           "<g fill=\"var(--_l)\">" + EYES + "</g>"
           "<path d=\"" + mouth + "\" fill=\"none\" stroke=\"var(--_l)\" stroke-linecap=\"round\"/>"
           "</g>";
}

inline Rendered verticalJourney(const JourneyModel& model, const RenderOptions& options) {
    double size = options.fontSize;
    double small = std::round(size * 0.875);
    const auto& measurer = getMeasurer(options);
    double pad = options.padding;
    double maxW = options.maxWidth > 0.0 ? options.maxWidth : 320.0;
    double room = std::max(140.0, maxW - 2.0 * pad);
    Label title = fitPlainLabel(model.title, measurer, size, room, Style::Bold);

    std::vector<std::string> names;
    for (const auto& name : journeyActors(model.tasks)) {
        if (!name.empty()) names.push_back(name);
    }
    std::unordered_map<std::string, int> actors;
    for (size_t i = 0; i < names.size(); i++) actors[names[i]] = static_cast<int>(i);

    double y = pad + (title.height > 0.0 ? title.height + 16.0 : 0.0);
    std::optional<std::string> section;
    std::string body;
    double width = title.width;
    for (size_t index = 0; index < model.tasks.size(); index++) {
        const auto& task = model.tasks[index];
        if (!section.has_value() || task.section != *section) {
            section = task.section;
            Label head = fitPlainLabel(*section, measurer, size, room - 16.0, Style::Bold);
            if (head.height > 0.0) {
                body += labelSvg(head, pad + 8.0 + head.width / 2.0, y + head.height / 2.0, " class=\"pele-cluster-label\" font-weight=\"var(--_hw)\"");
                width = std::max(width, head.width + 16.0);
                y += head.height + 8.0;
            }
        }
        Label label = fitPlainLabel(task.task, measurer, small, room - 72.0);
        struct PersonEntry {
            std::string name;
            Label label;
        };
        std::vector<PersonEntry> people;
        for (const auto& name : task.people) {
            if (!name.empty()) people.push_back({name, fitPlainLabel(name, measurer, small, room - 44.0)});
        }
        double textH = std::max(label.height, 42.0);
        double h = 2.0 * PAD_Y + textH;
        for (const auto& person : people) h += std::max(16.0, person.label.height) + 4.0;
        double w = std::max(room, label.width + 72.0);
        body += "<g class=\"pele-node pele-journey-step\" data-id=\"" + std::to_string(index) + "\"><rect x=\"" + num(pad) + "\" y=\"" + num(y) + "\" width=\"" + num(w) + "\" height=\"" + num(h) + "\" fill=\"var(--_s)\" stroke=\"var(--_b)\"" + ROUNDED + "/>";
        body += labelSvg(label, pad + PAD_X + label.width / 2.0, y + PAD_Y + label.height / 2.0, " class=\"pele-label\"");
        double cx = pad + w - 28.0;
        body += faceSvg(cx, y + PAD_Y + FACE, task.score > 3.0 ? "happy" : task.score < 3.0 ? "sad" : "neutral");
        if (!std::isnan(task.score)) {
            body += "<text class=\"pele-journey-score\" x=\"" + num(cx) + "\" y=\"" + num(y + PAD_Y + 2.0 * FACE + small) + "\" font-size=\"" + num(small) + "\" text-anchor=\"middle\">" + num(std::min(5.0, std::max(1.0, task.score))) + "</text>";
        }
        double py = y + PAD_Y + textH + 4.0;
        for (const auto& person : people) {
            double ph = std::max(16.0, person.label.height);
            body += "<circle class=\"pele-actor\" cx=\"" + num(pad + 16.0) + "\" cy=\"" + num(py + ph / 2.0) + "\" r=\"5\" fill=\"" + seriesColor(actors[person.name]) + "\"/>" +
                    labelSvg(person.label, pad + 28.0 + person.label.width / 2.0, py + ph / 2.0, " class=\"pele-journey-actor\" font-size=\"" + num(small) + "\"");
            py += ph + 4.0;
        }
        body += "</g>";
        y += h;
        if (index + 1 < model.tasks.size()) {
            body += "<path class=\"pele-edge\" d=\"M" + num(pad + w / 2.0) + "," + num(y) + "v16\" fill=\"none\" stroke=\"var(--_l)\"/>";
        }
        y += 16.0;
        width = std::max(width, w);
    }
    double totalWidth = std::ceil(width + 2.0 * pad);
    double height = std::ceil(std::max(pad + title.height, y - (!model.tasks.empty() ? 16.0 : 0.0)) + pad);
    std::string content = labelSvg(title, totalWidth / 2.0, pad + title.height / 2.0, " class=\"pele-title\" font-weight=\"var(--_tw)\"") + body;
    return {svgDocument("journey", totalWidth, height, size, options, model, content), totalWidth, height, {}};
}

inline Rendered drawJourney(const JourneyModel& model, const Config& /*config*/, const RenderOptions& options, bool down) {
    if (down) return verticalJourney(model, options);
    double size = options.fontSize;
    double small = std::round(size * 0.875);
    double tiny = std::round(size * 0.75);
    const auto& measurer = getMeasurer(options);
    double pad = options.padding;
    const auto& tasks = model.tasks;
    size_t n = tasks.size();

    std::vector<std::string> names;
    for (const auto& name : journeyActors(tasks)) {
        if (!name.empty()) names.push_back(name);
    }
    std::unordered_map<std::string, int> actor;
    for (size_t i = 0; i < names.size(); i++) actor[names[i]] = static_cast<int>(i);

    double textW = MIN_TEXT;
    std::vector<Label> labels(n);
    double widest = 0.0;
    for (size_t i = 0; i < n; i++) {
        labels[i] = plainLabel(tasks[i].task, measurer, small, textW);
        widest = std::max(widest, labels[i].width);
    }
    if (widest > textW) {
        textW = std::min(std::ceil(widest), MAX_TEXT);
        for (size_t i = 0; i < n; i++) {
            labels[i] = fitPlainLabel(tasks[i].task, measurer, small, textW);
        }
    }

    double cardW = textW + 2.0 * PAD_X;
    double pitch = cardW + GAP;
    double plotW = n > 0 ? static_cast<double>(n) * pitch - GAP : 0.0;

    double lineH = std::round(small * 1.5);
    double limit = std::max(plotW, 240.0);
    struct LegendEntry {
        std::string name;
        Label label;
        double x = 0.0;
        double y = 0.0;
        double h = 0.0;
    };
    std::vector<LegendEntry> legend;
    double legendW = 0.0;
    double legendH = 0.0;
    double lx = 0.0;
    for (const auto& name : names) {
        Label label = fitPlainLabel(name, measurer, small, limit - 2.0 * DOT - 6.0);
        double w = 2.0 * DOT + 6.0 + label.width;
        if (lx > 0.0 && lx + w > limit) {
            lx = 0.0;
            legendH += 4.0;
        }
        double y = lx > 0.0 ? legend.back().y : legendH;
        double h = std::max(label.height, lineH);
        legend.push_back({name, std::move(label), lx, y, h});
        lx += w;
        legendW = std::max(legendW, lx);
        legendH = std::max(legendH, y + h);
        lx += LEGEND_GAP;
    }

    double bodyW = n > 0 ? GUTTER + plotW : 0.0;
    Label title = fitPlainLabel(model.title, measurer, size, std::max(bodyW, 320.0), Style::Bold);
    double titleH = title.height > 0.0 ? title.height + 16.0 : 0.0;
    double width = std::max({bodyW, title.width, n > 0 ? GUTTER + legendW : 0.0});
    double left = pad + (width - bodyW) / 2.0 + GUTTER;
    double top = pad + titleH;

    struct SectionRun {
        size_t start = 0;
        size_t count = 0;
        std::optional<Label> label;
    };
    std::vector<SectionRun> runs;
    double bandH = 0.0;
    for (size_t i = 0; i < n;) {
        size_t end = i + 1;
        while (end < n && tasks[end].section == tasks[i].section) end++;
        std::optional<Label> label;
        if (!tasks[i].section.empty()) {
            Label l = fitPlainLabel(tasks[i].section, measurer, small, static_cast<double>(end - i) * pitch - GAP - 2.0 * PAD_X, Style::Bold);
            bandH = std::max(bandH, std::max(l.height, lineH) + 2.0 * BAND_PAD);
            label = std::move(l);
        }
        runs.push_back({i, end - i, std::move(label)});
        i = end;
    }

    double textH = lineH;
    for (const auto& l : labels) textH = std::max(textH, l.height);
    double dotsH = !names.empty() ? 2.0 * DOT + 6.0 : 0.0;
    double cardH = textH + dotsH + 2.0 * PAD_Y;
    double cardsY = top + (bandH > 0.0 ? bandH + BAND_PAD : 0.0);
    double chartTop = cardsY + cardH + FACE + 16.0;
    double chartBottom = chartTop + 4.0 * LEVEL;
    double right = left + plotW;

    std::string body;
    double bottom = top;
    if (n > 0) {
        std::string grid;
        std::string scale;
        for (int score = 5; score >= 1; score--) {
            double y = chartTop + static_cast<double>(5 - score) * LEVEL;
            grid += "M" + num(left - 6.0) + "," + num(y) + "H" + num(right);
            scale += "<text x=\"" + num(left - 12.0) + "\" y=\"" + num(y + tiny * 0.35) + "\">" + std::to_string(score) + "</text>";
        }
        body +=
            "<path class=\"pele-grid\" d=\"" + grid + "\" fill=\"none\" stroke=\"var(--_a)\"/>" +
            "<g class=\"pele-scale\" font-size=\"" + num(tiny) + "\" text-anchor=\"end\" fill=\"var(--_m)\">" + scale + "</g>";

        std::string line;
        std::string stems;
        std::string faces;
        std::vector<std::string> cards;
        cards.reserve(n);
        for (size_t i = 0; i < n; i++) {
            const auto& task = tasks[i];
            double x = left + static_cast<double>(i) * pitch;
            double cx = x + cardW / 2.0;
            double score = task.score;
            double level = std::isnan(score) ? 3.0 : std::min(5.0, std::max(1.0, score));
            double y = chartTop + (5.0 - level) * LEVEL;
            line += (i == 0 ? "M" : "L") + num(cx) + "," + num(y);
            stems += "M" + num(cx) + "," + num(cardsY + cardH) + "V" + num(y - FACE);
            faces += faceSvg(cx, y, score > 3.0 ? "happy" : score < 3.0 ? "sad" : "neutral");

            std::string dots;
            std::vector<std::string> people;
            for (const auto& name : task.people) {
                if (!name.empty()) people.push_back(name);
            }
            double step = std::min(DOT_STEP, (cardW - 2.0 * PAD_X - 2.0 * DOT) / std::max(static_cast<int>(people.size()) - 1, 1));
            for (size_t k = 0; k < people.size(); k++) {
                const auto& name = people[k];
                auto it = actor.find(name);
                int cIdx = (it != actor.end()) ? it->second : 0;
                dots +=
                    "<circle class=\"pele-actor\" cx=\"" + num(cx + (static_cast<double>(k) - static_cast<double>(people.size() - 1) / 2.0) * step) +
                    "\" cy=\"" + num(cardsY + cardH - PAD_Y - DOT) + "\" r=\"5\" fill=\"" + seriesColor(cIdx) + "\" stroke=\"var(--_s)\">" +
                    "<title>" + escText(name) + "</title></circle>";
            }
            cards.push_back(
                "<g class=\"pele-node pele-task\" data-id=\"" + escText(trimStr(task.task)) + "\">" +
                "<rect x=\"" + num(x) + "\" y=\"" + num(cardsY) + "\" width=\"" + num(cardW) + "\" height=\"" + num(cardH) + "\" fill=\"var(--_s)\" stroke=\"var(--_b)\"" + ROUNDED + "/>" +
                labelSvg(labels[i], cx, cardsY + PAD_Y + textH / 2.0, " font-size=\"" + num(small) + "\"") +
                dots +
                "</g>"
            );
        }
        body +=
            "<path class=\"pele-stems\" d=\"" + stems + "\" fill=\"none\" stroke=\"var(--_b)\" stroke-dasharray=\"2 3\"/>" +
            (n > 1 ? ("<path class=\"pele-edge\" d=\"" + line + "\" fill=\"none\" stroke=\"var(--_l)\" stroke-linejoin=\"round\"/>") : "") +
            faces;

        for (const auto& run : runs) {
            std::string members;
            for (size_t k = run.start; k < run.start + run.count; k++) members += cards[k];
            if (!run.label.has_value()) {
                body += members;
                continue;
            }
            double x = left + static_cast<double>(run.start) * pitch;
            double w = static_cast<double>(run.count) * pitch - GAP;
            body +=
                "<g class=\"pele-cluster pele-section\" data-id=\"" + escText(trimStr(tasks[run.start].section)) + "\">" +
                "<rect x=\"" + num(x) + "\" y=\"" + num(top) + "\" width=\"" + num(w) + "\" height=\"" + num(bandH) + "\" fill=\"var(--_a)\"" + ROUNDED + "/>" +
                labelSvg(*run.label, x + w / 2.0, top + bandH / 2.0, " font-size=\"" + num(small) + "\" font-weight=\"var(--_hw)\"") +
                members +
                "</g>";
        }
        bottom = chartBottom + FACE;

        if (!legend.empty()) {
            double x0 = left + (plotW - legendW) / 2.0;
            double y0 = bottom + 16.0;
            std::string items;
            for (const auto& item : legend) {
                double cy = y0 + item.y + item.h / 2.0;
                auto it = actor.find(item.name);
                int cIdx = (it != actor.end()) ? it->second : 0;
                items +=
                    "<g class=\"pele-legend-item\" data-id=\"" + escText(item.name) + "\">" +
                    "<circle cx=\"" + num(x0 + item.x + DOT) + "\" cy=\"" + num(cy) + "\" r=\"5\" fill=\"" + seriesColor(cIdx) + "\"/>" +
                    labelSvg(item.label, x0 + item.x + 2.0 * DOT + 6.0 + item.label.width / 2.0, cy, "") +
                    "</g>";
            }
            body += "<g class=\"pele-legend\" font-size=\"" + num(small) + "\">" + items + "</g>";
            bottom = y0 + legendH;
        }
    }

    double totalWidth = std::ceil(width + 2.0 * pad);
    double totalHeight = std::ceil(std::max(bottom, pad) + pad);
    std::string content =
        labelSvg(title, pad + width / 2.0, pad + title.height / 2.0, " class=\"pele-title\" font-weight=\"var(--_tw)\"") + body;
    return {
        svgDocument("journey", totalWidth, totalHeight, size, options, model, content),
        totalWidth,
        totalHeight,
        {}
    };
}

inline Rendered renderJourneyModel(const JourneyModel& model, const Config& config, const RenderOptions& options) {
    Rendered across = drawJourney(model, config, options, false);
    double available = options.maxWidth;
    if (!(available > 0.0) || across.width <= available || available >= 640.0) return across;
    Rendered down = drawJourney(model, config, options, true);
    return down.width < across.width ? down : across;
}

inline Rendered render(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options) {
    JourneyDb db;
    db.title = title;
    parseJourney(source, db);
    return renderJourneyModel(db, config, options);
}

} // namespace JourneyDiagram

inline Rendered renderJourney(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options) {
    return JourneyDiagram::render(source, config, title, options);
}

inline Rendered renderJourney(const std::string& source, const RenderOptions& options = {}) {
    auto pre = preprocess(source);
    std::string parsed = encodeEntities(pre.text) + "\n";
    RenderOptions opts = options;
    if (!opts.hasResponsive) {
        const auto* section = pre.config.get("journey");
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
        auto res = JourneyDiagram::render(parsed, pre.config, pre.title, opts);
        for (auto& link : res.links) link.id = decodeEntities(link.id);
        return res;
    });
}

} // namespace Pele
