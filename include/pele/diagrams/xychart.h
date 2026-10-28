#pragma once
#include "../foundation.h"

namespace Pele {
namespace XyChartDiagram {

// ============================================================================
// Helpers
// ============================================================================

inline std::string jsNumberToString(double v) {
    if (std::isnan(v)) return "NaN";
    if (!std::isfinite(v)) return v > 0 ? "Infinity" : "-Infinity";
    if (v == 0.0) v = 0.0;
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.15g", v);
    std::string s(buf);
    // Normalize e+06 -> e+6 if any
    size_t ePos = s.find('e');
    if (ePos != std::string::npos && ePos + 2 < s.size() && (s[ePos + 1] == '+' || s[ePos + 1] == '-')) {
        size_t z = ePos + 2;
        while (z + 1 < s.size() && s[z] == '0') z++;
        if (z > ePos + 2) s.erase(ePos + 2, z - (ePos + 2));
    }
    return s;
}

inline double jsRound(double x) {
    return std::floor(x + 0.5);
}

inline bool wordAtAscii(const std::string& src, size_t at, const std::string& word) {
    if (at + word.size() > src.size()) return false;
    for (size_t k = 0; k < word.size(); k++) {
        int c = static_cast<unsigned char>(src[at + k]);
        int w = static_cast<unsigned char>(word[k]);
        if (c != w && !(w >= 97 && w <= 122 && c == w - 32)) return false;
    }
    return true;
}

inline double capWidth(const RenderOptions& options, double natural, double least, double around = 0.0) {
    double available = options.maxWidth;
    if (!(available > 0.0)) return natural;
    return std::max(std::min(least, natural), std::min(natural, available - around));
}

// ============================================================================
// src/diagrams/xychart/ticks.ts
// ============================================================================

inline double tickStep(double span, double count) {
    double raw = span / std::max(count, 1.0);
    double power = std::floor(std::log10(raw));
    double unit = raw / std::pow(10.0, power);
    double factor = unit >= 7.07 ? 10.0 : unit >= 3.16 ? 5.0 : unit >= 1.41 ? 2.0 : 1.0;
    return power < 0.0 ? (-std::pow(10.0, -power) / factor) : (factor * std::pow(10.0, power));
}

inline double tickIndex(double step, double value) {
    return step > 0.0 ? (value / step) : (value * -step);
}

inline double tickValue(double step, double index) {
    double v = step > 0.0 ? (index * step) : (index / -step);
    return v == 0.0 ? 0.0 : v;
}

inline std::vector<double> ticksBetween(double lo, double hi, double step) {
    double first = jsRound(tickIndex(step, lo));
    if (tickValue(step, first) < lo) first += 1.0;
    double last = jsRound(tickIndex(step, hi));
    if (tickValue(step, last) > hi) last -= 1.0;
    std::vector<double> out;
    for (double i = first; i <= last && out.size() < 100; i += 1.0) {
        out.push_back(tickValue(step, i));
    }
    return out;
}

// ============================================================================
// src/diagrams/xychart/db.ts
// ============================================================================

struct XyText {
    std::string text;
    std::string type = "text";
};

struct DataPoint {
    double value = 0.0;
    std::string label;
};

struct AxisData {
    std::string type = "band"; // "band" or "linear"
    std::string title;
    std::vector<std::string> categories;
    double min = std::numeric_limits<double>::infinity();
    double max = -std::numeric_limits<double>::infinity();
};

struct PlotEntry {
    std::string x;
    double y = 0.0;
    bool hasY = false;
};

struct Plot {
    std::string type; // "line" or "bar"
    std::string title;
    std::vector<PlotEntry> data;
    std::vector<std::string> pointLabels;
    bool hasPointLabels = false;
};

class XyChartDb : public DiagramMeta {
public:
    std::string orientation; // "" or "vertical" or "horizontal"
    AxisData xAxis{"band", "", {}, 0.0, 0.0};
    AxisData yAxis{"linear", "", {}, std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()};
    bool hasSetXAxis = false;
    bool hasSetYAxis = false;
    std::vector<Plot> plots;

    void setOrientation(const std::string& orient) {
        orientation = (orient == "horizontal") ? "horizontal" : "vertical";
    }

    void setXAxisTitle(const XyText& t) {
        xAxis.title = trimStr(t.text);
    }

    void setXAxisRangeData(double minVal, double maxVal) {
        xAxis.type = "linear";
        xAxis.categories.clear();
        xAxis.min = minVal;
        xAxis.max = maxVal;
        hasSetXAxis = true;
    }

    void setXAxisBand(const std::vector<XyText>& cats) {
        xAxis.type = "band";
        xAxis.categories.clear();
        for (const auto& c : cats) xAxis.categories.push_back(trimStr(c.text));
        hasSetXAxis = true;
    }

    void setYAxisTitle(const XyText& t) {
        yAxis.title = trimStr(t.text);
    }

    void setYAxisRangeData(double minVal, double maxVal) {
        yAxis.type = "linear";
        yAxis.min = minVal;
        yAxis.max = maxVal;
        hasSetYAxis = true;
    }

    std::vector<PlotEntry> plotData(std::vector<double> values) {
        if (values.empty()) return {};
        if (!hasSetXAxis) setXAxisRangeData(1.0, static_cast<double>(values.size()));
        if (xAxis.type == "band" && values.size() > xAxis.categories.size()) {
            values.resize(xAxis.categories.size());
        }
        if (!hasSetYAxis) {
            double minV = yAxis.min;
            double maxV = yAxis.max;
            for (double v : values) {
                if (v < minV) minV = v;
                if (v > maxV) maxV = v;
            }
            yAxis.min = minV;
            yAxis.max = maxV;
        }
        std::vector<PlotEntry> out;
        if (xAxis.type == "band") {
            for (size_t i = 0; i < xAxis.categories.size(); i++) {
                if (i < values.size()) out.push_back({xAxis.categories[i], values[i], true});
                else out.push_back({xAxis.categories[i], 0.0, false});
            }
            return out;
        }
        if (values.size() == 1) {
            return {{jsNumberToString(xAxis.min), values[0], true}};
        }
        double step = (xAxis.max - xAxis.min) / static_cast<double>(values.size() - 1);
        for (size_t i = 0; i < values.size(); i++) {
            out.push_back({jsNumberToString(xAxis.min + static_cast<double>(i) * step), values[i], true});
        }
        return out;
    }

    void addPlot(const std::string& ptype, const XyText& ptitle, const std::vector<DataPoint>& data) {
        std::vector<double> vals;
        vals.reserve(data.size());
        for (const auto& d : data) vals.push_back(d.value);
        Plot plot;
        plot.type = ptype;
        plot.title = trimStr(ptitle.text);
        plot.data = plotData(std::move(vals));
        if (ptype == "line") {
            bool anyLabel = false;
            for (const auto& d : data) {
                if (!trimStr(d.label).empty()) { anyLabel = true; break; }
            }
            if (anyLabel) {
                plot.hasPointLabels = true;
                for (const auto& d : data) plot.pointLabels.push_back(trimStr(d.label));
            }
        }
        plots.push_back(std::move(plot));
    }

    void setLineData(const XyText& ptitle, const std::vector<DataPoint>& data) {
        addPlot("line", ptitle, data);
    }

    void setBarData(const XyText& ptitle, const std::vector<DataPoint>& data) {
        addPlot("bar", ptitle, data);
    }

    void setDiagramTitle(const std::string& text) {
        title = text;
    }

    void setAccTitle(const std::string& text) {
        accTitle = trimStartStr(text);
    }

    void setAccDescription(const std::string& text) {
        std::string out;
        size_t i = 0;
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
        accDescr = out;
    }
};

// ============================================================================
// src/diagrams/xychart/tokens.ts & lexer.ts & parser.ts
// ============================================================================

namespace T {
    enum Type {
        END,
        ERROR,
        NEWLINE,
        title,
        acc_title,
        acc_title_value,
        acc_descr,
        acc_descr_value,
        acc_descr_multiline_value,
        XYCHART,
        CHART_ORIENTATION,
        X_AXIS,
        Y_AXIS,
        ARROW_DELIMITER,
        LINE,
        BAR,
        NUMBER_WITH_DECIMAL,
        SQUARE_BRACES_START,
        SQUARE_BRACES_END,
        STR,
        ALPHA,
        COLON,
        PLUS,
        COMMA,
        EQUALS,
        MULT,
        BRKT,
        UNDERSCORE,
        DOT,
        AMP,
        MINUS,
        NUM,
        SEMI,
        EOF_TOK
    };
}

inline const char* tokenName(int t) {
    static const char* NAMES[] = {
        "$end", "INVALID", "NEWLINE", "title", "acc_title", "acc_title_value",
        "acc_descr", "acc_descr_value", "acc_descr_multiline_value", "XYCHART",
        "CHART_ORIENTATION", "X_AXIS", "Y_AXIS", "ARROW_DELIMITER", "LINE", "BAR",
        "NUMBER_WITH_DECIMAL", "SQUARE_BRACES_START", "SQUARE_BRACES_END", "STR",
        "ALPHA", "COLON", "PLUS", "COMMA", "EQUALS", "MULT", "BRKT", "UNDERSCORE",
        "DOT", "AMP", "MINUS", "NUM", "SEMI", "EOF"
    };
    return (t >= 0 && t <= T::EOF_TOK) ? NAMES[t] : "INVALID";
}

struct XyTokens {
    std::vector<int> types;
    std::vector<std::string> texts;
    std::vector<size_t> starts;
};

inline XyTokens tokenize(const std::string& src) {
    enum State {
        INITIAL,
        axis_data,
        axis_band_data,
        data,
        data_inner,
        S_string,
        acc_title,
        acc_descr,
        acc_descr_multiline
    };
    static const int AT_END[] = {
        T::EOF_TOK, T::EOF_TOK, T::EOF_TOK, T::EOF_TOK, T::EOF_TOK,
        T::STR, T::acc_title_value, T::acc_descr_value, T::acc_descr_multiline_value
    };
    static const std::string BROKEN_RULE =
        "`)" + std::string(36, ' ') + "{ this.pushstate(md_string); }\n<md_string>(?:(?!`\").)+" + std::string(18, ' ') + "{ return md_str; }\n<md_string>(?:`";

    static const std::vector<int> SINGLE = []() {
        std::vector<int> tbl(128, 0);
        tbl[':'] = T::COLON;
        tbl['+'] = T::PLUS;
        tbl[','] = T::COMMA;
        tbl['='] = T::EQUALS;
        tbl['*'] = T::MULT;
        tbl['#'] = T::BRKT;
        tbl['_'] = T::UNDERSCORE;
        tbl['.'] = T::DOT;
        tbl['&'] = T::AMP;
        tbl['-'] = T::MINUS;
        tbl[';'] = T::SEMI;
        return tbl;
    }();

    size_t n = src.size();
    XyTokens out;
    std::vector<int> stack;
    int state = INITIAL;
    size_t p = 0;

    auto emit = [&](int type, size_t s, size_t e) {
        out.types.push_back(type);
        out.texts.push_back(src.substr(s, e - s));
        out.starts.push_back(s);
        p = e;
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
    auto lineEnd = [&](size_t q) -> size_t {
        size_t e = src.find('\n', q);
        return e == std::string::npos ? n : e;
    };
    auto charAt = [&](size_t idx) -> int {
        if (idx >= n) return -1;
        size_t dummy = idx;
        return charCodeAtUtf8(src, idx, &dummy);
    };
    auto byteAt = [&](size_t idx) -> int {
        return idx < n ? static_cast<unsigned char>(src[idx]) : -1;
    };
    auto run = [&](size_t q) -> size_t {
        while (q < n) {
            size_t next = q;
            int cp = charCodeAtUtf8(src, q, &next);
            if (!isSpace(cp)) break;
            q = next;
        }
        return q;
    };
    auto digits = [&](size_t q) -> size_t {
        while (q < n && isDigit(byteAt(q))) q++;
        return q;
    };
    auto keyword = [&](const std::string& word) -> bool {
        return wordAtAscii(src, p, word) && !isWord(byteAt(p + word.size()));
    };

    while (p < n) {
        int c = charAt(p);

        if (state == S_string) {
            if (c == 34) {
                p++;
                pop();
            } else {
                size_t e = src.find('"', p);
                emit(T::STR, p, e == std::string::npos ? n : e);
            }
            continue;
        }
        if (state == acc_title || state == acc_descr) {
            emit(AT_END[state], p, lineEnd(p));
            pop();
            continue;
        }
        if (state == acc_descr_multiline) {
            if (c == 125) {
                p++;
                pop();
            } else {
                size_t e = src.find('}', p);
                emit(T::acc_descr_multiline_value, p, e == std::string::npos ? n : e);
            }
            continue;
        }

        int next = byteAt(p + 1);
        if (c == 37 && next == 37) {
            p = lineEnd(p + 2);
            continue;
        }
        if (c == 10 || c == 13) {
            if ((state == axis_data || state == data) && (c == 10 || next == 10)) {
                emit(T::NEWLINE, p, c == 10 ? p + 1 : p + 2);
                pop();
                continue;
            }
            size_t e = p + 1;
            while (byteAt(e) == 10 || byteAt(e) == 13) e++;
            emit(T::NEWLINE, p, e);
            continue;
        }

        bool matchedSwitch = false;
        switch (c | 32) {
            case 116:
                if (keyword("title")) {
                    emit(T::title, p, p + 5);
                    matchedSwitch = true;
                }
                break;
            case 97:
                if (wordAtAscii(src, p, "acc")) {
                    bool descr = wordAtAscii(src, p + 3, "descr");
                    if (descr || wordAtAscii(src, p + 3, "title")) {
                        size_t k = run(p + 8);
                        int d = byteAt(k);
                        if (d == 58) {
                            emit(descr ? T::acc_descr : T::acc_title, p, run(k + 1));
                            push(descr ? acc_descr : acc_title);
                            matchedSwitch = true;
                        } else if (d == 123 && descr) {
                            p = run(k + 1);
                            push(acc_descr_multiline);
                            matchedSwitch = true;
                        }
                    }
                }
                break;
            case 120:
                if (keyword("xychart-beta")) {
                    emit(T::XYCHART, p, p + 12);
                    matchedSwitch = true;
                } else if (keyword("xychart")) {
                    emit(T::XYCHART, p, p + 7);
                    matchedSwitch = true;
                } else if (keyword("x-axis")) {
                    emit(T::X_AXIS, p, p + 6);
                    push(axis_data);
                    matchedSwitch = true;
                }
                break;
            case 121:
                if (keyword("y-axis")) {
                    emit(T::Y_AXIS, p, p + 6);
                    push(axis_data);
                    matchedSwitch = true;
                }
                break;
            case 118:
                if (wordAtAscii(src, p, "vertical")) {
                    emit(T::CHART_ORIENTATION, p, p + 8);
                    matchedSwitch = true;
                }
                break;
            case 104:
                if (wordAtAscii(src, p, "horizontal")) {
                    emit(T::CHART_ORIENTATION, p, p + 10);
                    matchedSwitch = true;
                }
                break;
            case 108:
                if (keyword("line")) {
                    emit(T::LINE, p, p + 4);
                    push(data);
                    matchedSwitch = true;
                }
                break;
            case 98:
                if (keyword("bar")) {
                    emit(T::BAR, p, p + 3);
                    push(data);
                    matchedSwitch = true;
                }
                break;
        }
        if (matchedSwitch) continue;

        if (c == 91) {
            if (state == axis_data) push(axis_band_data);
            else if (state == data) push(data_inner);
            emit(T::SQUARE_BRACES_START, p, p + 1);
            continue;
        }
        if (c == 93) {
            if (state == data_inner || state == axis_band_data) pop();
            emit(T::SQUARE_BRACES_END, p, p + 1);
            continue;
        }
        if (state == axis_data && c == 45 && next == 45 && byteAt(p + 2) == 62) {
            emit(T::ARROW_DELIMITER, p, p + 3);
            continue;
        }
        if (state == axis_data || state == data_inner) {
            size_t q = (c == 43 || c == 45) ? (p + 1) : p;
            int d = byteAt(q);
            int e = -1;
            if (isDigit(d)) {
                size_t ed = digits(q + 1);
                if (byteAt(ed) == 46 && isDigit(byteAt(ed + 1))) ed = digits(ed + 2);
                e = static_cast<int>(ed);
            } else if (d == 46 && isDigit(byteAt(q + 1))) {
                e = static_cast<int>(digits(q + 2));
            }
            if (e != -1) {
                emit(T::NUMBER_WITH_DECIMAL, p, static_cast<size_t>(e));
                continue;
            }
        }
        if (c == 96 && wordAtAscii(src, p, BROKEN_RULE)) {
            p += BROKEN_RULE.size();
            pop();
            continue;
        }
        if (c == 34) {
            p++;
            push(S_string);
            continue;
        }
        if (isLetter(c)) {
            size_t e = p + 1;
            while (isLetter(byteAt(e))) e++;
            emit(T::ALPHA, p, e);
            continue;
        }
        if (isDigit(c)) {
            emit(T::NUM, p, digits(p + 1));
            continue;
        }
        if (isSpace(c)) {
            size_t nextP = p;
            charCodeAtUtf8(src, p, &nextP);
            p = run(nextP);
            continue;
        }
        if (c >= 0 && c < 128 && SINGLE[c] != 0) {
            emit(SINGLE[c], p, p + 1);
            continue;
        }
        break;
    }

    if (p < n) {
        out.types.push_back(T::ERROR);
        out.texts.push_back("");
        out.starts.push_back(p);
        return out;
    }
    out.types.push_back(AT_END[state]);
    out.texts.push_back("");
    out.starts.push_back(n);
    out.types.push_back(T::END);
    out.texts.push_back("");
    out.starts.push_back(n);
    return out;
}

inline void parseXyChart(const std::string& src, XyChartDb& db) {
    auto tok = tokenize(src);
    const auto& types = tok.types;
    const auto& texts = tok.texts;
    const auto& starts = tok.starts;
    size_t i = 0;

    auto isWordTok = [](int t) {
        return t == T::AMP || t == T::NUM || t == T::ALPHA || t == T::PLUS || t == T::EQUALS ||
               t == T::MULT || t == T::DOT || t == T::BRKT || t == T::MINUS || t == T::UNDERSCORE;
    };
    auto isEolTok = [](int t) {
        return t == T::NEWLINE || t == T::SEMI || t == T::EOF_TOK;
    };
    auto isText = [&](int t) {
        return t == T::STR || isWordTok(t);
    };

    auto fail = [&](const std::string& expected) {
        int t = types[i];
        if (t == T::ERROR) throw syntaxError("xychart", src, starts[i], "", true);
        throw syntaxError("xychart", src, starts[i], "Expecting " + expected + ", got '" + tokenName(t) + "'");
    };

    auto expect = [&](int type) -> std::string {
        if (types[i] != type) fail(std::string("'") + tokenName(type) + "'");
        return texts[i++];
    };

    auto text = [&]() -> XyText {
        if (!isText(types[i])) fail("'STR', 'ALPHA', 'NUM'");
        std::string s = texts[i++];
        if (types[i - 1] != T::STR) {
            while (isWordTok(types[i])) s += texts[i++];
        }
        return {s, "text"};
    };

    auto range = [&]() -> std::pair<double, double> {
        double minV = std::stod(expect(T::NUMBER_WITH_DECIMAL));
        expect(T::ARROW_DELIMITER);
        double maxV = std::stod(expect(T::NUMBER_WITH_DECIMAL));
        return {minV, maxV};
    };

    auto points = [&]() -> std::vector<DataPoint> {
        std::vector<DataPoint> out;
        expect(T::SQUARE_BRACES_START);
        for (;;) {
            double value = std::stod(expect(T::NUMBER_WITH_DECIMAL));
            std::string label = (types[i] == T::STR) ? texts[i++] : "";
            out.push_back({value, label});
            if (types[i] != T::COMMA) break;
            i++;
        }
        expect(T::SQUARE_BRACES_END);
        return out;
    };

    for (;;) {
        if (isEolTok(types[i])) i++;
        else if (types[i] != T::XYCHART) break;
        else if (types[++i] == T::CHART_ORIENTATION) db.setOrientation(texts[i++]);
    }

    while (types[i] != T::END) {
        int t = types[i++];
        switch (t) {
            case T::title:
                db.setDiagramTitle(trimStr(text().text));
                break;
            case T::X_AXIS:
            case T::Y_AXIS: {
                bool hasTitle = isText(types[i]);
                XyText axTitle = hasTitle ? text() : XyText{"", "text"};
                if (t == T::X_AXIS && types[i] == T::SQUARE_BRACES_START) {
                    std::vector<XyText> categories;
                    do {
                        i++;
                        categories.push_back(text());
                    } while (types[i] == T::COMMA);
                    expect(T::SQUARE_BRACES_END);
                    db.setXAxisBand(categories);
                } else if (types[i] == T::NUMBER_WITH_DECIMAL || !hasTitle) {
                    auto [minV, maxV] = range();
                    if (t == T::X_AXIS) db.setXAxisRangeData(minV, maxV);
                    else db.setYAxisRangeData(minV, maxV);
                }
                if (t == T::X_AXIS) db.setXAxisTitle(axTitle);
                else db.setYAxisTitle(axTitle);
                break;
            }
            case T::LINE:
            case T::BAR: {
                XyText ptitle = (types[i] == T::SQUARE_BRACES_START) ? XyText{"", "text"} : text();
                auto data = points();
                if (t == T::LINE) db.setLineData(ptitle, data);
                else db.setBarData(ptitle, data);
                break;
            }
            case T::acc_title:
                db.setAccTitle(trimStr(expect(T::acc_title_value)));
                break;
            case T::acc_descr:
                db.setAccDescription(trimStr(expect(T::acc_descr_value)));
                break;
            case T::acc_descr_multiline_value:
                db.setAccDescription(trimStr(texts[i - 1]));
                break;
            default:
                i--;
                fail("'title', 'X_AXIS', 'Y_AXIS', 'LINE', 'BAR', 'acc_title', 'acc_descr', 'acc_descr_multiline_value'");
        }
        while (isEolTok(types[i])) i++;
    }
}

// ============================================================================
// src/diagrams/xychart/render.ts
// ============================================================================

struct Mark {
    double at = 0.0;
    Label label;
};

inline Config record(const YamlValue* value) {
    return (value && value->isMap()) ? *value : YamlValue::makeMap();
}

inline bool on(const Config& config, const std::string& key, bool fallback = true) {
    const auto* v = config.get(key);
    return (v && v->isBool()) ? v->bVal : fallback;
}

inline double dimension(const YamlValue* value, double fallback) {
    return (value && value->isNumber() && value->nVal > 0.0)
        ? std::round(std::min(std::max(value->nVal, 100.0), 4000.0))
        : fallback;
}

inline std::optional<double> finiteVal(double value, bool hasValue = true) {
    constexpr double LIMIT = 1e300;
    if (!hasValue || !std::isfinite(value)) return std::nullopt;
    return std::min(std::max(value, -LIMIT), LIMIT);
}

inline double widestMark(const std::vector<Mark>& marks) {
    double w = 0.0;
    for (const auto& m : marks) w = std::max(w, m.label.width);
    return w;
}

inline std::string aligned(const Label& label, double x, double y, const std::string& anchor) {
    if (label.lines.size() == 1 && label.lines[0].size() == 1 && !label.lines[0][0].hasIcon && label.lines[0][0].style == 0) {
        return "<text x=\"" + num(x) + "\" y=\"" + num(y + label.size * 0.35) + "\" text-anchor=\"" + anchor + "\">" + esc(label.lines[0][0].text) + "</text>";
    }
    double shift = (anchor == "start") ? (label.width / 2.0) : (anchor == "end") ? (-label.width / 2.0) : 0.0;
    return labelSvg(label, x + shift, y, "");
}

inline Rendered renderXyChartModel(const XyChartDb& model, const Config& config, const RenderOptions& options) {
    constexpr double TICK = 4.0;
    constexpr double SWATCH = 12.0;
    constexpr double ROW = 26.0;
    constexpr double LEGEND_GAP = 24.0;

    if (model.plots.empty()) {
        throw PeleError("No Plot to render, please provide a plot with some data", "semantic", "xychart");
    }
    Config chart = record(config.get("xyChart"));
    double size = options.fontSize;
    double small = std::round(size * 0.875);
    double tiny = std::round(size * 0.75);
    const TextMeasurer& measurer = getMeasurer(options);
    double pad = options.padding;
    double natural = dimension(chart.get("width"), 700.0);
    double width = capWidth(options, natural, 240.0);
    double height = std::round(dimension(chart.get("height"), 500.0) * std::max(0.6, width / natural));
    std::string orientStr = !model.orientation.empty()
        ? model.orientation
        : (chart.get("chartOrientation") && chart.get("chartOrientation")->isString() ? chart.get("chartOrientation")->sVal : "");
    bool horizontal = (orientStr == "horizontal");
    bool showValues = on(chart, "showDataLabel", false);
    const auto& x = model.xAxis;
    const auto& plots = model.plots;
    auto textFn = [&](const std::string& raw, double fontSize, double max) -> Label {
        return layoutLabel(raw, false, measurer, fontSize, max);
    };
    double lineHeight = textFn("0", small, 4000.0).height;
    double tinyHeight = textFn("0", tiny, 4000.0).height;

    Label title = layoutLabel(on(chart, "showTitle") ? model.title : "", false, measurer, size, width - 2.0 * pad, Style::Bold);
    double titleSpace = title.height > 0.0 ? (title.height + 12.0) : 0.0;

    struct LegendItem {
        size_t index;
        Label label;
    };
    std::vector<LegendItem> legend;
    double legendWidth = 0.0;
    if (on(chart, "showLegend")) {
        for (size_t idx = 0; idx < plots.size(); idx++) {
            if (plots[idx].title.empty()) continue;
            Label lbl = textFn(plots[idx].title, small, width * 0.3);
            legendWidth = std::max(legendWidth, lbl.width + SWATCH + 8.0);
            legend.push_back({idx, std::move(lbl)});
        }
    }
    double legendSpace = !legend.empty() ? (legendWidth + LEGEND_GAP) : 0.0;

    int bars = 0;
    for (const auto& p : plots) if (p.type == "bar") bars++;
    double lo = finiteVal(model.yAxis.min).value_or(std::numeric_limits<double>::infinity());
    double hi = finiteVal(model.yAxis.max).value_or(-std::numeric_limits<double>::infinity());
    bool exact = model.hasSetYAxis && std::isfinite(lo) && std::isfinite(hi) && lo != hi;
    if (!exact) {
        lo = std::numeric_limits<double>::infinity();
        hi = -std::numeric_limits<double>::infinity();
        for (const auto& plot : plots) {
            for (const auto& entry : plot.data) {
                auto val = finiteVal(entry.y, entry.hasY);
                if (!val.has_value()) continue;
                if (*val < lo) lo = *val;
                if (*val > hi) hi = *val;
            }
        }
        if (lo > hi) lo = hi = 0.0;
        if (bars > 0) {
            lo = std::min(lo, 0.0);
            hi = std::max(hi, 0.0);
        }
        if (lo == hi) {
            if (lo == 0.0) hi = 1.0;
            else {
                lo -= std::abs(lo) / 2.0;
                hi += std::abs(hi) / 2.0;
            }
        }
    }

    auto valueMarks = [&](double length, double spacing) -> std::vector<Mark> {
        double step = tickStep(std::abs(hi - lo), std::max(2.0, std::min(10.0, std::floor(length / spacing))));
        if (!exact) {
            lo = tickValue(step, std::floor(tickIndex(step, lo) + 1e-9));
            hi = tickValue(step, std::ceil(tickIndex(step, hi) - 1e-9));
        }
        std::vector<Mark> out;
        for (double val : ticksBetween(std::min(lo, hi), std::max(lo, hi), step)) {
            out.push_back({(val - lo) / (hi - lo), textFn(jsNumberToString(val), small, 4000.0)});
        }
        return out;
    };

    size_t count = 1;
    if (x.type == "band") count = std::max<size_t>(x.categories.size(), 1);
    else for (const auto& plot : plots) count = std::max(count, plot.data.size());
    double slot = 1.0 / static_cast<double>(count);
    double inset = (x.type == "band" || bars > 0) ? (slot / 2.0) : 0.0;
    auto x0 = finiteVal(x.min, x.type == "linear");
    auto x1 = finiteVal(x.max, x.type == "linear");
    bool ranged = x0.has_value() && x1.has_value() && (*x0 != *x1);
    auto place = [&](size_t i, size_t n) -> double {
        if (x.type == "band") return (static_cast<double>(i) + 0.5) * slot;
        double frac = (n > 1) ? (static_cast<double>(i) / static_cast<double>(n - 1)) : (ranged ? 0.0 : 0.5);
        return inset + frac * (1.0 - 2.0 * inset);
    };

    Config categoryConf = record(chart.get("xAxis"));
    Config valueConf = record(chart.get("yAxis"));
    const Config& leftConf = horizontal ? categoryConf : valueConf;
    const Config& bottomConf = horizontal ? valueConf : categoryConf;
    Label leftTitle = textFn(on(leftConf, "showTitle") ? (horizontal ? x.title : model.yAxis.title) : "", small, height * 0.8);
    Label bottomTitle = textFn(on(bottomConf, "showTitle") ? (horizontal ? model.yAxis.title : x.title) : "", small, width * 0.8);
    double leftTick = on(leftConf, "showTick") ? TICK : 0.0;
    double bottomTick = on(bottomConf, "showTick") ? TICK : 0.0;
    bool leftLabels = on(leftConf, "showLabel");
    bool bottomLabels = on(bottomConf, "showLabel");

    double wrapAt = horizontal ? (width * 0.3) : std::max(48.0, (width - 2.0 * pad - 64.0 - legendSpace) * slot - 8.0);
    std::vector<Label> categories;
    if (x.type == "band" && on(categoryConf, "showLabel")) {
        for (const auto& cat : x.categories) categories.push_back(textFn(cat, small, wrapAt));
    }
    double categoryWidth = 0.0;
    double categoryHeight = 0.0;
    for (const auto& lbl : categories) {
        categoryWidth = std::max(categoryWidth, lbl.width);
        categoryHeight = std::max(categoryHeight, lbl.height);
    }

    auto categoryMarks = [&](double length) -> std::vector<Mark> {
        std::vector<Mark> marks;
        if (x.type == "band") {
            double need = categories.empty() ? 8.0 : (horizontal ? categoryHeight : (categoryWidth + 12.0));
            size_t every = static_cast<size_t>(std::max(1.0, std::ceil(need / (length * slot))));
            for (size_t i = 0; i < x.categories.size(); i += every) {
                Label lbl = (i < categories.size()) ? categories[i] : textFn("", small, 0.0);
                marks.push_back({(static_cast<double>(i) + 0.5) * slot, std::move(lbl)});
            }
        } else if (ranged) {
            double step = tickStep(std::abs(*x1 - *x0), std::max(2.0, std::min(10.0, std::floor(length / (horizontal ? 32.0 : 80.0)))));
            for (double val : ticksBetween(std::min(*x0, *x1), std::max(*x0, *x1), step)) {
                marks.push_back({inset + ((val - *x0) / (*x1 - *x0)) * (1.0 - 2.0 * inset), textFn(jsNumberToString(val), small, 4000.0)});
            }
        } else if (x0.has_value()) {
            marks.push_back({0.5, textFn(jsNumberToString(*x0), small, 4000.0)});
        }
        return marks;
    };

    double pointLabelWidth = 0.0;
    double valueWidth = 0.0;
    for (const auto& plot : plots) {
        for (const auto& lbl : plot.pointLabels) {
            pointLabelWidth = std::max(pointLabelWidth, measurer.width(lbl, tiny, 0));
        }
        if (showValues && horizontal && plot.type == "bar") {
            for (const auto& entry : plot.data) {
                auto fv = finiteVal(entry.y, entry.hasY);
                std::string s = fv.has_value() ? jsNumberToString(*fv) : "";
                valueWidth = std::max(valueWidth, measurer.width(s, tiny, 0));
            }
        }
    }

    double bottomLabelHeight = !bottomLabels ? 0.0 : ((horizontal || x.type != "band") ? lineHeight : categoryHeight);
    double bottomSpace = bottomTick + (bottomLabelHeight > 0.0 ? (bottomLabelHeight + 4.0) : 0.0) + (bottomTitle.height > 0.0 ? (bottomTitle.height + 4.0) : 0.0);
    double headroom = horizontal ? 0.0 : ((pointLabelWidth > 0.0 || showValues) ? (tinyHeight + 4.0) : (leftLabels ? (lineHeight / 2.0) : 0.0));
    double top = pad + titleSpace + headroom;
    double plotHeight = std::max(1.0, height - pad - bottomSpace - top);
    double bottom = top + plotHeight;

    std::vector<Mark> leftMarks = horizontal ? categoryMarks(plotHeight) : valueMarks(plotHeight, 56.0);
    double leftLabelWidth = leftLabels ? widestMark(leftMarks) : 0.0;
    double left = pad + (leftTitle.height > 0.0 ? (leftTitle.height + 6.0) : 0.0) + (leftLabelWidth > 0.0 ? (leftLabelWidth + 6.0) : 0.0) + leftTick;
    double beside = std::max(pointLabelWidth > 0.0 ? (pointLabelWidth + 12.0) : 0.0, valueWidth > 0.0 ? (valueWidth + 8.0) : 0.0);
    double reach = horizontal ? std::min(width * 0.25, beside) : 0.0;
    double plotWidth = std::max(1.0, width - pad - left - legendSpace - reach);
    std::vector<Mark> bottomMarks = horizontal ? valueMarks(plotWidth, 80.0) : categoryMarks(plotWidth);
    if (bottomLabels && legendSpace + reach == 0.0) {
        double over = 0.0;
        for (const auto& mark : bottomMarks) {
            over = std::max(over, mark.at * plotWidth + mark.label.width / 2.0 - plotWidth);
        }
        plotWidth = std::max(1.0, plotWidth - over);
    }
    double right = left + plotWidth;

    auto px = [&](double c, double v) -> double { return horizontal ? (left + v * plotWidth) : (left + c * plotWidth); };
    auto py = [&](double c, double v) -> double { return horizontal ? (top + c * plotHeight) : (bottom - v * plotHeight); };
    auto scaleFn = [&](double value) -> double { return (value - lo) / (hi - lo); };
    auto clampFn = [](double v) -> double { return std::min(std::max(v, 0.0), 1.0); };
    auto rule = [&](double v) -> std::string {
        return horizontal
            ? ("M" + num(px(0.0, v)) + "," + num(top) + "V" + num(bottom))
            : ("M" + num(left) + "," + num(py(0.0, v)) + "H" + num(right));
    };

    std::string grid;
    for (const auto& mark : (horizontal ? bottomMarks : leftMarks)) grid += rule(mark.at);

    std::string axis;
    std::string labels;
    if (on(leftConf, "showAxisLine")) axis += "M" + num(left) + "," + num(top) + "V" + num(bottom);
    if (on(bottomConf, "showAxisLine")) axis += "M" + num(left) + "," + num(bottom) + "H" + num(right);
    for (const auto& mark : leftMarks) {
        double y = horizontal ? (top + mark.at * plotHeight) : (bottom - mark.at * plotHeight);
        if (leftTick > 0.0) axis += "M" + num(left) + "," + num(y) + "h" + num(-leftTick);
        if (leftLabels) labels += aligned(mark.label, left - leftTick - 6.0, y, "end");
    }
    for (const auto& mark : bottomMarks) {
        double at = left + mark.at * plotWidth;
        if (bottomTick > 0.0) axis += "M" + num(at) + "," + num(bottom) + "v" + num(bottomTick);
        if (bottomLabels) labels += aligned(mark.label, at, bottom + bottomTick + 4.0 + mark.label.height / 2.0, "middle");
    }

    double span = (horizontal ? plotHeight : plotWidth) * slot;
    double group = span * 0.7;
    double thickness = group / std::max(bars, 1);
    double gap = bars > 1 ? std::min(2.0, thickness * 0.2) : 0.0;
    double base = clampFn(scaleFn(0.0));
    if (bars > 0 && base > 0.0 && base < 1.0) axis += rule(base);

    std::string series;
    std::string lines;
    std::string outside;
    std::string pointLabels;
    int bar = 0;
    for (size_t idx = 0; idx < plots.size(); idx++) {
        const auto& plot = plots[idx];
        std::string color = seriesColor(static_cast<int>(idx));
        std::string id = !plot.title.empty() ? (" data-id=\"" + escText(plot.title) + "\"") : "";
        size_t n = (x.type == "band") ? std::min(plot.data.size(), count) : plot.data.size();
        if (plot.type == "bar") {
            double offset = (bar++ - (bars - 1) / 2.0) * thickness - (thickness - gap) / 2.0;
            std::string rects;
            for (size_t i = 0; i < n; i++) {
                auto val = finiteVal(plot.data[i].y, plot.data[i].hasY);
                if (!val.has_value()) continue;
                double c = place(i, n);
                double v = clampFn(scaleFn(*val));
                double from = std::min(base, v);
                double to = std::max(base, v);
                double bx = horizontal ? px(c, from) : (px(c, 0.0) + offset);
                double by = horizontal ? (py(c, 0.0) + offset) : py(c, to);
                double bw = horizontal ? ((to - from) * plotWidth) : (thickness - gap);
                double bh = horizontal ? (thickness - gap) : ((to - from) * plotHeight);
                rects += "<rect x=\"" + num(bx) + "\" y=\"" + num(by) + "\" width=\"" + num(bw) + "\" height=\"" + num(bh) + "\"/>";
                if (!showValues) continue;
                std::string lblStr = jsNumberToString(*val);
                double w = measurer.width(lblStr, tiny, 0);
                double outDir = v >= base ? 1.0 : -1.0;
                double tx = bx + bw / 2.0;
                double ty = by + bh / 2.0;
                std::string anchor = "middle";
                if (horizontal) {
                    if (bh < tiny) continue;
                    tx = (outDir > 0.0 ? (bx + bw) : bx) + outDir * 6.0;
                    anchor = outDir > 0.0 ? "start" : "end";
                } else {
                    if (w + 2.0 > span / bars) continue;
                    ty = (outDir > 0.0 ? by : (by + bh)) - outDir * (tinyHeight / 2.0 + 2.0);
                }
                outside += "<text x=\"" + num(tx) + "\" y=\"" + num(ty + tiny * 0.35) + "\" text-anchor=\"" + anchor + "\">" + lblStr + "</text>";
            }
            series += "<g class=\"pele-series pele-bars\"" + id + " fill=\"" + color + "\">" + rects + "</g>";
            continue;
        }

        std::string d;
        std::string dots;
        bool pen = false;
        double pc = 0.0;
        double pv = std::numeric_limits<double>::quiet_NaN();
        for (size_t i = 0; i < n; i++) {
            auto val = finiteVal(plot.data[i].y, plot.data[i].hasY);
            if (!val.has_value()) {
                pv = std::numeric_limits<double>::quiet_NaN();
                continue;
            }
            double c = place(i, n);
            double v = scaleFn(*val);
            bool visible = (v >= 0.0 && v <= 1.0);
            if (std::isnan(pv)) {
                if (visible) d += "M" + num(px(c, v)) + "," + num(py(c, v));
                pen = visible;
            } else {
                double dv = v - pv;
                double t0 = 0.0;
                double t1 = 1.0;
                if (dv != 0.0) {
                    double a = -pv / dv;
                    double b = (1.0 - pv) / dv;
                    t0 = std::max(t0, std::min(a, b));
                    t1 = std::min(t1, std::max(a, b));
                } else if (!visible) {
                    t0 = 2.0;
                }
                if (t0 <= t1) {
                    double c0 = pc + (c - pc) * t0;
                    double v0 = pv + dv * t0;
                    double c1 = pc + (c - pc) * t1;
                    double v1 = pv + dv * t1;
                    if (!pen || t0 > 0.0) d += "M" + num(px(c0, v0)) + "," + num(py(c0, v0));
                    d += "L" + num(px(c1, v1)) + "," + num(py(c1, v1));
                }
                pen = visible;
            }
            pc = c;
            pv = v;
            if (!visible) continue;
            double cx = px(c, v);
            double cy = py(c, v);
            if (span >= 12.0) dots += "<circle cx=\"" + num(cx) + "\" cy=\"" + num(cy) + "\" r=\"3\"/>";
            std::string ptRaw = (plot.hasPointLabels && i < plot.pointLabels.size()) ? plot.pointLabels[i] : "";
            Label lbl = textFn(ptRaw, tiny, 4000.0);
            if (lbl.height > 0.0) {
                double half = lbl.width / 2.0;
                pointLabels += horizontal
                    ? aligned(lbl, cx + 10.0, cy, "start")
                    : aligned(lbl, std::max(std::min(cx, right - half), left + half), cy - 6.0 - lbl.height / 2.0, "middle");
            }
        }
        lines += "<g class=\"pele-series pele-line\"" + id + " fill=\"" + color + "\">" +
            (!d.empty() ? ("<path d=\"" + d + "\" fill=\"none\" stroke=\"" + color + "\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"/>") : "") +
            dots + "</g>";
    }

    std::string titles;
    if (leftTitle.height > 0.0) {
        titles += "<g transform=\"translate(" + num(pad + leftTitle.height / 2.0) + "," + num(top + plotHeight / 2.0) + ") rotate(-90)\">" + labelSvg(leftTitle, 0.0, 0.0, "") + "</g>";
    }
    titles += labelSvg(bottomTitle, left + plotWidth / 2.0, height - pad - bottomTitle.height / 2.0, "");

    std::string legendOut;
    double legendHeight = 0.0;
    for (const auto& item : legend) legendHeight += std::max(ROW, item.label.height + 6.0);
    double ly = top + std::max(0.0, (plotHeight - legendHeight) / 2.0);
    double lx = right + reach + LEGEND_GAP;
    for (const auto& item : legend) {
        double row = std::max(ROW, item.label.height + 6.0);
        if (ly + row > height) break;
        double y = ly + row / 2.0;
        std::string color = seriesColor(static_cast<int>(item.index));
        legendOut += "<g class=\"pele-legend-item\" data-id=\"" + escText(plots[item.index].title) + "\">" +
            (plots[item.index].type == "bar"
                ? ("<rect x=\"" + num(lx) + "\" y=\"" + num(y - SWATCH / 2.0) + "\" width=\"" + num(SWATCH) + "\" height=\"" + num(SWATCH) + "\" rx=\"2\" fill=\"" + color + "\"/>")
                : ("<path d=\"M" + num(lx) + "," + num(y) + "h" + num(SWATCH) + "\" stroke=\"" + color + "\" stroke-width=\"2\" stroke-linecap=\"round\"/>")) +
            labelSvg(item.label, lx + SWATCH + 8.0 + item.label.width / 2.0, y, "") +
            "</g>";
        ly += row;
    }

    std::string content =
        labelSvg(title, width / 2.0, pad + title.height / 2.0, " class=\"pele-title\" font-weight=\"var(--_tw)\"") +
        (!grid.empty() ? ("<path class=\"pele-grid\" d=\"" + grid + "\" stroke=\"var(--_a)\"/>") : "") +
        "<g class=\"pele-plot\">" + series + lines + "</g>" +
        (!axis.empty() ? ("<path class=\"pele-axis\" d=\"" + axis + "\" fill=\"none\" stroke=\"var(--_b)\"/>") : "") +
        (!labels.empty() ? ("<g class=\"pele-axis-labels\" font-size=\"" + num(small) + "\" fill=\"var(--_m)\">" + labels + "</g>") : "") +
        (!titles.empty() ? ("<g class=\"pele-axis-titles\" font-size=\"" + num(small) + "\">" + titles + "</g>") : "") +
        (!outside.empty() ? ("<g class=\"pele-data-labels\" font-size=\"" + num(tiny) + "\">" + outside + "</g>") : "") +
        (!pointLabels.empty() ? ("<g class=\"pele-point-labels\" font-size=\"" + num(tiny) + "\">" + pointLabels + "</g>") : "") +
        (!legendOut.empty() ? ("<g class=\"pele-legend\" font-size=\"" + num(small) + "\">" + legendOut + "</g>") : "");

    return {
        svgDocument("xychart", width, height, size, options, model, content),
        width,
        height,
        {}
    };
}

inline Rendered render(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options) {
    XyChartDb db;
    db.title = title;
    parseXyChart(source, db);
    return renderXyChartModel(db, config, options);
}

} // namespace XyChartDiagram

inline Rendered renderXYChart(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options) {
    return XyChartDiagram::render(source, config, title, options);
}

inline Rendered renderXYChart(const std::string& raw, RenderOptions options = {}) {
    auto pre = preprocess(raw);
    std::string parsed = encodeEntities(pre.text) + "\n";
    const auto* section = pre.config.get("xyChart");
    bool fixed = (section && section->isMap() && section->get("useMaxWidth") && section->get("useMaxWidth")->isBool() && !section->get("useMaxWidth")->bVal);
    if (!options.hasResponsive) {
        options.responsive = !fixed;
        options.hasResponsive = true;
    }
    return XyChartDiagram::render(parsed, pre.config, pre.title, options);
}

} // namespace Pele
