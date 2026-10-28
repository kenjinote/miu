#pragma once
#include "../foundation.h"

namespace Pele {
namespace TreemapDiagram {

// ============================================================================
// src/diagrams/treemap/parser.ts
// ============================================================================

namespace T {
enum Kind {
    accDescr = 0,
    accTitle = 1,
    title = 2,
    keyword = 3,
    classDef = 4,
    styleSeparator = 5,
    separator = 6,
    comma = 7,
    indentation = 8,
    whitespace = 9,
    comment = 10,
    newline = 11,
    id = 12,
    number = 13,
    string = 14
};
} // namespace T

inline TokenType makeTreemapTitle() noexcept {
    TokenType t = common_tokens::makeTitle();
    t.match = [](const std::string& src, int at, int) -> int {
        int p = common_tokens::matchTabsSpaces(src, at);
        if (src.compare(p, 5, "title") != 0) return -1;
        p += 5;
        if (p < static_cast<int>(src.size()) && (src[p] == '\t' || src[p] == ' ')) {
            return common_tokens::matchRestOfLineBeforeComment(src, p + 1);
        }
        if (p < static_cast<int>(src.size()) && isWord(charCodeAt(src, p))) return -1;
        return p;
    };
    return t;
}

inline const std::vector<TokenType>& treemapTokens() noexcept {
    static const std::vector<TokenType> types = []() {
        TokenType kw;
        kw.name = "TREEMAP_KEYWORD";
        kw.isRegex = true;
        kw.first = "t";
        kw.match = [](const std::string& src, int at, int) -> int {
            if (startsWith(src, "treemap-beta", static_cast<size_t>(at))) return at + 12;
            if (startsWith(src, "treemap", static_cast<size_t>(at))) return at + 7;
            return -1;
        };

        TokenType cdef;
        cdef.name = "CLASS_DEF";
        cdef.isRegex = true;
        cdef.first = "c";
        cdef.match = [](const std::string& src, int at, int) -> int {
            if (!startsWith(src, "classDef", static_cast<size_t>(at))) return -1;
            int n = static_cast<int>(src.size());
            int p = at + 8;
            while (p < n && isSpace(charCodeAt(src, p))) p++;
            if (p == at + 8 || p >= n) return -1;
            int c0 = charCodeAt(src, p);
            if (!isLetter(c0) && c0 != '_') return -1;
            int idStart = p;
            p++;
            while (p < n && isWord(charCodeAt(src, p))) p++;
            if (p - idStart < 2) return -1;
            int afterId = p;
            while (p < n && isSpace(charCodeAt(src, p))) p++;
            if (p > afterId) {
                while (p < n && src[p] != ';' && src[p] != '\r' && src[p] != '\n') p++;
            }
            if (p < n && src[p] == ';') p++;
            return p;
        };

        auto mark = [](const std::string& name, const std::string& lit) {
            TokenType t;
            t.name = name;
            t.literal = lit;
            t.isRegex = false;
            return t;
        };

        TokenType indentTok;
        indentTok.name = "INDENTATION";
        indentTok.isRegex = true;
        indentTok.first = " \t";
        indentTok.match = [](const std::string& src, int at, int) -> int {
            int p = common_tokens::matchTabsSpaces(src, at);
            return p > at ? p : -1;
        };

        TokenType wsTok = common_tokens::makeWhitespace();
        wsTok.name = "WS";

        TokenType commentTok;
        commentTok.name = "ML_COMMENT";
        commentTok.isRegex = true;
        commentTok.hidden = true;
        commentTok.first = "%";
        commentTok.match = [](const std::string& src, int at, int) -> int {
            if (src.compare(at, 2, "%%") != 0) return -1;
            int p = at + 2;
            int n = static_cast<int>(src.size());
            while (p < n && src[p] != '\n') p++;
            return p;
        };

        TokenType nlTok = common_tokens::makeNewline();
        nlTok.name = "NL";
        nlTok.hidden = true;

        TokenType idTok;
        idTok.name = "ID2";
        idTok.isRegex = true;
        idTok.match = [](const std::string& src, int at, int) -> int {
            int n = static_cast<int>(src.size());
            if (at >= n) return -1;
            int c = charCodeAt(src, at);
            if (!isLetter(c) && c != '_') return -1;
            int p = at + 1;
            while (p < n && isWord(charCodeAt(src, p))) p++;
            return p;
        };

        TokenType numTok;
        numTok.name = "NUMBER2";
        numTok.isRegex = true;
        numTok.first = "0123456789_.,";
        numTok.match = [](const std::string& src, int at, int) -> int {
            int n = static_cast<int>(src.size());
            int p = at;
            while (p < n) {
                char c = src[p];
                if ((c >= '0' && c <= '9') || c == '_' || c == '.' || c == ',') p++;
                else break;
            }
            return p > at ? p : -1;
        };

        TokenType strTok;
        strTok.name = "STRING2";
        strTok.isRegex = true;
        strTok.first = "\"'";
        strTok.match = [](const std::string& src, int at, int) -> int {
            int n = static_cast<int>(src.size());
            if (at >= n) return -1;
            char q = src[at];
            if (q != '"' && q != '\'') return -1;
            size_t end = src.find(q, static_cast<size_t>(at + 1));
            return end == std::string::npos ? -1 : static_cast<int>(end + 1);
        };

        return std::vector<TokenType>{
            common_tokens::makeAccDescr(),
            common_tokens::makeAccTitle(),
            makeTreemapTitle(),
            kw,
            cdef,
            mark("STYLE_SEPARATOR", ":::"),
            mark("SEPARATOR", ":"),
            mark("COMMA", ","),
            indentTok,
            wsTok,
            commentTok,
            nlTok,
            idTok,
            numTok,
            strTok
        };
    }();
    return types;
}

struct TreemapRowEntry {
    bool isClassDef = false;
    std::optional<int> indent;
    // For ClassDefStatement:
    std::string className;
    std::string styleText;
    // For TreemapRow:
    bool isLeaf = false;
    std::string name;
    double value = 0.0;
    std::optional<std::string> classSelector;
};

struct TreemapAst {
    std::optional<std::string> title;
    std::optional<std::string> accTitle;
    std::optional<std::string> accDescr;
    std::vector<TreemapRowEntry> rows;
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

inline TreemapAst parseTreemap(const std::string& src) {
    const auto& types = treemapTokens();
    Tokens toks = tokenize(src, types, "treemap");
    TokenReader r(toks, types, "treemap");
    const auto& kinds = toks.kinds;
    TreemapAst ast;

    auto classSelector = [&]() -> std::optional<std::string> {
        if (r.accept(T::styleSeparator)) return r.expect(T::id);
        return std::nullopt;
    };

    r.expect(T::keyword);
    while (r.kind() != -1) {
        int kind = r.kind();
        if (kind == T::accDescr) {
            ast.accDescr = fixAccDescrValue(r.take());
            continue;
        }
        if (kind == T::accTitle) {
            ast.accTitle = fixAccTitleValue(r.take());
            continue;
        }
        if (kind == T::title) {
            ast.title = fixTitleValue(r.take());
            continue;
        }
        if (kind != T::indentation && kind != T::string && kind != T::classDef) {
            r.fail("a node, a class definition, or a title");
        }

        std::optional<int> indent;
        if (kind == T::indentation) {
            indent = static_cast<int>(r.take().size());
        }
        if (r.kind() == T::classDef) {
            std::string raw = r.take();
            size_t p = 8;
            while (p < raw.size() && isSpace(static_cast<unsigned char>(raw[p]))) p++;
            size_t idStart = p;
            while (p < raw.size() && isWord(static_cast<unsigned char>(raw[p]))) p++;
            std::string className = raw.substr(idStart, p - idStart);
            std::string styleText;
            size_t afterId = p;
            while (p < raw.size() && isSpace(static_cast<unsigned char>(raw[p]))) p++;
            if (p > afterId && p < raw.size()) {
                size_t end = p;
                while (end < raw.size() && raw[end] != ';' && raw[end] != '\n' && raw[end] != '\r') end++;
                styleText = raw.substr(p, end - p);
            }
            TreemapRowEntry row;
            row.isClassDef = true;
            row.indent = indent;
            row.className = className;
            row.styleText = styleText;
            ast.rows.push_back(std::move(row));
            continue;
        }

        std::string strTok = r.expect(T::string);
        std::string name = strTok.size() >= 2 ? strTok.substr(1, strTok.size() - 2) : "";
        int after = (kinds[r.i] == T::indentation) ? (r.i + 1) : r.i;
        TreemapRowEntry row;
        row.indent = indent;
        row.name = name;
        if (kinds[after] == T::separator || kinds[after] == T::comma) {
            r.i = after + 1;
            r.accept(T::indentation);
            std::string numStr = replaceAllStr(r.expect(T::number), ",", "");
            row.isLeaf = true;
            row.value = std::strtod(numStr.c_str(), nullptr);
        } else {
            row.isLeaf = false;
        }
        row.classSelector = classSelector();
        ast.rows.push_back(std::move(row));
    }
    return ast;
}

// ============================================================================
// src/diagrams/treemap/model.ts
// ============================================================================

struct TreemapNode {
    std::string name;
    bool isSection = false;
    std::vector<TreemapNode> children;
    std::optional<double> value;
    std::string classSelector;
    std::vector<std::string> cssCompiledStyles;
};

struct TreemapItem {
    int level = 0;
    std::string name;
    bool isLeaf = false;
    std::optional<double> value;
    std::string classSelector;
    std::vector<std::string> cssCompiledStyles;
};

struct TreemapModel : public DiagramMeta {
    std::vector<TreemapNode> roots;
    std::unordered_map<std::string, std::vector<std::string>> classes;
};

inline std::vector<TreemapNode> buildHierarchy(const std::vector<TreemapItem>& items) noexcept {
    // Use shared_ptr tree first so pushing onto children vectors never invalidates pointers in `stack`
    struct TempNode {
        TreemapNode data;
        std::vector<std::shared_ptr<TempNode>> children;
    };
    std::vector<std::shared_ptr<TempNode>> tempRoots;
    std::vector<std::pair<std::shared_ptr<TempNode>, int>> stack;

    for (const auto& item : items) {
        auto tn = std::make_shared<TempNode>();
        tn->data.name = item.name;
        tn->data.isSection = !item.isLeaf;
        tn->data.classSelector = item.classSelector;
        tn->data.cssCompiledStyles = item.cssCompiledStyles;
        if (item.isLeaf && item.value.has_value()) tn->data.value = item.value;

        while (!stack.empty() && stack.back().second >= item.level) stack.pop_back();
        if (stack.empty()) tempRoots.push_back(tn);
        else stack.back().first->children.push_back(tn);
        if (!item.isLeaf) stack.push_back({tn, item.level});
    }

    auto convert = [](auto& self, const std::shared_ptr<TempNode>& tn) -> TreemapNode {
        TreemapNode out = tn->data;
        for (const auto& ch : tn->children) {
            out.children.push_back(self(self, ch));
        }
        return out;
    };
    std::vector<TreemapNode> roots;
    for (const auto& r : tempRoots) roots.push_back(convert(convert, r));
    return roots;
}

inline TreemapModel buildTreemap(const TreemapAst& ast, const std::string& fallbackTitle) noexcept {
    TreemapModel model;
    for (const auto& row : ast.rows) {
        if (!row.isClassDef) continue;
        auto& styles = model.classes[row.className];
        std::string text = replaceAllStr(row.styleText, "\\,", "\x01\x02\x03");
        text = replaceAllStr(text, ",", ";");
        text = replaceAllStr(text, "\x01\x02\x03", ",");
        for (const auto& s : splitStr(text, ';')) styles.push_back(s);
    }

    std::vector<TreemapItem> items;
    for (const auto& row : ast.rows) {
        if (row.isClassDef) continue;
        TreemapItem item;
        item.level = row.indent.value_or(0);
        item.name = row.name;
        item.isLeaf = row.isLeaf;
        if (row.isLeaf) item.value = row.value;
        if (row.classSelector.has_value()) {
            item.classSelector = *row.classSelector;
            auto it = model.classes.find(*row.classSelector);
            if (it != model.classes.end() && !it->second.empty()) {
                item.cssCompiledStyles = it->second;
            }
        }
        items.push_back(std::move(item));
    }

    model.title = (ast.title.has_value() && !ast.title->empty()) ? *ast.title : fallbackTitle;
    if (ast.accTitle.has_value() && !ast.accTitle->empty()) {
        model.accTitle = trimStartStr(*ast.accTitle);
    }
    if (ast.accDescr.has_value() && !ast.accDescr->empty()) {
        model.accDescr = *ast.accDescr;
    }
    model.roots = buildHierarchy(items);
    return model;
}

// ============================================================================
// src/diagrams/treemap/format.ts
// ============================================================================

inline std::string groupDigits(const std::string& digits) noexcept {
    std::string out;
    for (int i = static_cast<int>(digits.size()); i > 0; i -= 3) {
        int start = std::max(0, i - 3);
        out = digits.substr(static_cast<size_t>(start), static_cast<size_t>(i - start)) + (out.empty() ? "" : Pele::cat({",", out}));
    }
    return out;
}

inline std::function<std::string(double)> treemapFormatter(const std::string& rawSpec) {
    // RE_SPEC = /^(?:.?[<>=^])?([-+( ])?(\$)?0?\d*(,)?(?:\.(\d+))?(~)?([a-zA-Z%])?$/
    std::string spec = (rawSpec == "$0,0") ? "$," : rawSpec;
    struct ParsedSpec {
        bool valid = false;
        bool plus = false;
        std::string dollar;
        bool comma = false;
        std::optional<int> precision;
        bool tilde = false;
        char type = 0;
    };
    auto parseOne = [](const std::string& s) -> ParsedSpec {
        ParsedSpec res;
        size_t i = 0;
        size_t n = s.size();
        auto isAlign = [](char c) { return c == '<' || c == '>' || c == '=' || c == '^'; };
        if (i < n && isAlign(s[i])) i++;
        else if (i + 1 < n && isAlign(s[i + 1])) i += 2;
        if (i < n && (s[i] == '-' || s[i] == '+' || s[i] == '(' || s[i] == ' ')) {
            res.plus = (s[i] == '+');
            i++;
        }
        if (i < n && s[i] == '$') {
            res.dollar = "$";
            i++;
        }
        if (i < n && s[i] == '0') i++;
        while (i < n && isDigit(static_cast<unsigned char>(s[i]))) i++;
        if (i < n && s[i] == ',') {
            res.comma = true;
            i++;
        }
        if (i < n && s[i] == '.') {
            i++;
            size_t dStart = i;
            while (i < n && isDigit(static_cast<unsigned char>(s[i]))) i++;
            if (i == dStart) return res;
            int p = 0;
            try { p = std::stoi(s.substr(dStart, i - dStart)); } catch (...) { p = 20; }
            res.precision = std::min(p, 20);
        }
        if (i < n && s[i] == '~') {
            res.tilde = true;
            i++;
        }
        if (i < n && (isLetter(static_cast<unsigned char>(s[i])) || s[i] == '%')) {
            res.type = s[i];
            i++;
        }
        if (i == n) res.valid = true;
        return res;
    };

    ParsedSpec m = parseOne(spec);
    if (!m.valid) m = parseOne(",");

    return [m](double value) -> std::string {
        double absVal = std::abs(value);
        std::string body;
        std::string suffix;
        auto toFixedStr = [](double v, int prec) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(prec) << v;
            return oss.str();
        };
        if (m.type == 'f') {
            body = toFixedStr(absVal, m.precision.value_or(6));
        } else if (m.type == '%') {
            body = toFixedStr(absVal * 100.0, m.precision.value_or(6));
            suffix = "%";
        } else if (m.type == 'd') {
            body = toFixedStr(std::round(absVal), 0);
        } else if (m.type == 'e') {
            std::ostringstream oss;
            oss << std::scientific << std::setprecision(m.precision.value_or(6)) << absVal;
            body = oss.str();
            size_t ePos = body.find('e');
            if (ePos != std::string::npos && ePos + 2 < body.size() && (body[ePos + 1] == '+' || body[ePos + 1] == '-')) {
                size_t z = ePos + 2;
                while (z + 1 < body.size() && body[z] == '0') z++;
                body = body.substr(0, ePos + 2) + body.substr(z);
            }
        } else {
            int prec = std::max(1, m.precision.value_or(12));
            std::ostringstream oss;
            oss << std::setprecision(prec) << absVal;
            double rounded = std::stod(oss.str());
            std::ostringstream oss2;
            oss2 << std::setprecision(15) << rounded;
            body = oss2.str();
        }

        if (m.tilde && body.find('.') != std::string::npos && body.find('e') == std::string::npos) {
            while (!body.empty() && body.back() == '0') body.pop_back();
            if (!body.empty() && body.back() == '.') body.pop_back();
        }
        if (m.comma && body.find('e') == std::string::npos) {
            size_t dot = body.find('.');
            body = (dot == std::string::npos) ? groupDigits(body) : (groupDigits(body.substr(0, dot)) + body.substr(dot));
        }
        return std::string(value < 0 ? "-" : m.plus ? "+" : "") + m.dollar + body + suffix;
    };
}

// ============================================================================
// src/diagrams/treemap/layout.ts
// ============================================================================

struct Box {
    const TreemapNode* node = nullptr;
    std::shared_ptr<Box> parent;
    int series = 0;
    double value = 0.0;
    std::vector<std::shared_ptr<Box>> children;
    bool headed = false;
    double header = 0.0;
    double x = 0.0;
    double y = 0.0;
    double w = 0.0;
    double h = 0.0;
};

struct Insets {
    double gap = 0.0;
    double header = 0.0;
    double shortHeader = 0.0;
    double side = 0.0;
};

inline std::shared_ptr<Box> makeBox(const TreemapNode& node, const std::shared_ptr<Box>& parent, int series) noexcept {
    auto b = std::make_shared<Box>();
    b->node = &node;
    b->parent = parent;
    b->series = series;
    b->value = (!node.isSection && node.value.has_value() && std::isfinite(*node.value) && *node.value > 0.0) ? *node.value : 0.0;
    return b;
}

inline double clampVal(double v, double low, double high) noexcept {
    return v >= low ? std::min(v, high) : low;
}

inline void squarify(std::vector<std::shared_ptr<Box>>& boxes, double total, double x, double y, double w, double h, double gap) noexcept {
    double rest = total;
    size_t from = 0;
    while (from < boxes.size()) {
        double scale = (w * h) / rest;
        double side = std::min(w, h);
        double sum = boxes[from]->value * scale;
        double largest = sum;
        double worst = std::max((side * side * largest) / (sum * sum), (sum * sum) / (side * side * largest));
        size_t to = from + 1;
        while (to < boxes.size()) {
            double smallest = boxes[to]->value * scale;
            double next = sum + smallest;
            double ratio = std::max((side * side * largest) / (next * next), (next * next) / (side * side * smallest));
            if (ratio > worst) break;
            worst = ratio;
            sum = next;
            to++;
        }

        double thickness = sum / side;
        double offset = 0.0;
        for (size_t i = from; i < to; i++) {
            auto& b = boxes[i];
            double length = (i == to - 1) ? (side - offset) : ((b->value * scale) / thickness);
            double bx = w >= h ? x : x + offset;
            double by = w >= h ? y + offset : y;
            double bw = w >= h ? thickness : length;
            double bh = w >= h ? length : thickness;
            double gx = std::round(bx + bw - gap / 2.0) > std::round(bx + gap / 2.0) ? gap / 2.0 : 0.0;
            double gy = std::round(by + bh - gap / 2.0) > std::round(by + gap / 2.0) ? gap / 2.0 : 0.0;
            b->x = std::round(bx + gx);
            b->y = std::round(by + gy);
            b->w = std::round(bx + bw - gx) - b->x;
            b->h = std::round(by + bh - gy) - b->y;
            offset += length;
            rest -= b->value;
        }
        if (w >= h) {
            x += thickness;
            w -= thickness;
        } else {
            y += thickness;
            h -= thickness;
        }
        from = to;
    }
}

inline std::vector<std::shared_ptr<Box>> layoutTreemap(const std::vector<TreemapNode>& roots, double width, double height, const Insets& insets) noexcept {
    std::vector<std::shared_ptr<Box>> top;
    for (size_t i = 0; i < roots.size(); i++) {
        top.push_back(makeBox(roots[i], nullptr, static_cast<int>(i)));
    }
    std::vector<std::shared_ptr<Box>> all = top;
    for (size_t i = 0; i < all.size(); i++) {
        auto parent = all[i];
        if (parent->node->isSection) {
            for (const auto& chNode : parent->node->children) {
                auto child = makeBox(chNode, parent, parent->series);
                parent->children.push_back(child);
                all.push_back(child);
            }
        }
    }
    for (int i = static_cast<int>(all.size()) - 1; i >= 0; i--) {
        auto& b = all[static_cast<size_t>(i)];
        if (b->parent) b->parent->value += b->value;
    }

    double gap = insets.gap;
    double header = insets.header;
    double shortH = insets.shortHeader;
    double side = insets.side;
    std::vector<std::shared_ptr<Box>> placed;

    auto place = [&](const std::vector<std::shared_ptr<Box>>& children, double x, double y, double w, double h) {
        std::vector<std::shared_ptr<Box>> shown;
        for (const auto& b : children) {
            if (b->value > 0.0) shown.push_back(b);
        }
        std::stable_sort(shown.begin(), shown.end(), [](const std::shared_ptr<Box>& a, const std::shared_ptr<Box>& b) {
            return a->value > b->value;
        });
        double total = 0.0;
        for (const auto& b : shown) total += b->value;
        if (shown.empty() || !std::isfinite(total)) return;
        w = std::max(0.0, w);
        h = std::max(0.0, h);
        if (w > 0.0 && h > 0.0) squarify(shown, total, x - gap / 2.0, y - gap / 2.0, w + gap, h + gap, gap);
        for (const auto& b : shown) {
            double x0 = clampVal(b->x, x, x + w);
            double y0 = clampVal(b->y, y, y + h);
            b->w = clampVal(b->x + b->w, x0, x + w) - x0;
            b->h = clampVal(b->y + b->h, y0, y + h) - y0;
            b->x = x0;
            b->y = y0;
            placed.push_back(b);
        }
    };

    place(top, 0.0, 0.0, width, height);
    for (size_t i = 0; i < placed.size(); i++) {
        auto b = placed[i];
        if (b->children.empty()) continue;
        double topSpace = (b->h - header - side >= 12.0) ? header : shortH;
        b->headed = (b->w > std::max(24.0, 2.0 * side)) && (b->h - topSpace - side >= 8.0);
        b->header = b->headed ? topSpace : 0.0;
        if (b->headed) place(b->children, b->x + side, b->y + topSpace, b->w - 2.0 * side, b->h - topSpace - side);
        else place(b->children, b->x, b->y, b->w, b->h);
    }
    return placed;
}

// ============================================================================
// src/diagrams/treemap/render.ts
// ============================================================================

constexpr double HEADER = 28.0;
constexpr double SHORT_HEADER = 18.0;
constexpr double SIDE = 6.0;
constexpr double LINE = 1.25;

struct FittedText {
    std::string text;
    double size = 0.0;
    double width = 0.0;
};

inline std::optional<FittedText> fitText(
    const std::string& text,
    double maxW,
    double size,
    double minSize,
    const TextMeasurer& measurer,
    int style = 0
) {
    if (text.empty() || maxW <= 0.0) return std::nullopt;
    double width = measurer.width(text, size, style);
    if (width <= maxW) return FittedText{text, size, width};

    double fitted = std::max(minSize, std::floor((size * maxW) / width));
    while (fitted >= minSize) {
        width = measurer.width(text, fitted, style);
        if (width <= maxW) return FittedText{text, fitted, width};
        fitted -= 1.0;
    }

    std::vector<std::string> chars;
    size_t idx = 0;
    while (idx < text.size()) {
        size_t s = idx;
        nextCodepoint(text, idx);
        chars.push_back(text.substr(s, idx - s));
    }
    auto joinPrefix = [&](int len) {
        std::string res;
        for (int k = 0; k < len; k++) res += chars[static_cast<size_t>(k)];
        return res;
    };
    const std::string ELLIPSIS = "\xE2\x80\xA6";
    int low = 0;
    int high = static_cast<int>(chars.size()) - 1;
    while (low < high) {
        int mid = (low + high + 1) >> 1;
        if (measurer.width(trimEndStr(joinPrefix(mid)) + ELLIPSIS, minSize, style) <= maxW) low = mid;
        else high = mid - 1;
    }
    if (low == 0) return std::nullopt;
    std::string cut = trimEndStr(joinPrefix(low)) + ELLIPSIS;
    width = measurer.width(cut, minSize, style);
    if (width <= maxW) return FittedText{cut, minSize, width};
    return std::nullopt;
}

inline std::string titlePart(const std::string& raw) noexcept {
    return trimStr(esc(trimStr(decodeEntities(raw))));
}

inline int leafCount(const std::vector<TreemapNode>& roots) noexcept {
    int count = 0;
    std::vector<const TreemapNode*> stack;
    for (const auto& r : roots) stack.push_back(&r);
    while (!stack.empty()) {
        const TreemapNode* node = stack.back();
        stack.pop_back();
        if (node->isSection) {
            for (const auto& ch : node->children) stack.push_back(&ch);
        } else if (node->value.has_value() && std::isfinite(*node->value) && *node->value > 0.0) {
            count++;
        }
    }
    return count;
}

inline std::string corners(const std::string& attrs, const Box& b) noexcept {
    double cap = std::min(b.w, b.h) / 4.0;
    if (cap < 4.0) {
        return replaceAllStr(rounded(attrs),  "rx:var(--_r)", Pele::cat({"rx:min(var(--_r),", num(cap), "px)"}));
    }
    return rounded(attrs);
}

inline double treemapOption(const Config& config, const std::string& key, double fallback, double minVal, double maxVal) noexcept {
    const auto* t = config.get("treemap");
    if (!t || !t->isMap()) return fallback;
    const auto* v = t->get(key);
    if (!v || !v->isNumber()) return fallback;
    return v->nVal >= minVal ? std::min(v->nVal, maxVal) : fallback;
}

inline double capWidth(const RenderOptions& options, double natural, double least, double around = 0.0) noexcept {
    double available = options.maxWidth;
    if (!(available > 0.0)) return natural;
    return std::max(std::min(least, natural), std::min(natural, available - around));
}

inline double treemapTitleRoom(const RenderOptions& options) noexcept {
    double available = options.maxWidth;
    double pad = options.hasPadding ? options.padding : 8.0;
    return available > 0.0 ? std::max(80.0, available - 2.0 * pad) : 4000.0;
}

inline Rendered renderTreemapModel(const TreemapModel& model, const Config& config, const RenderOptions& options) noexcept {
    double size = options.hasFontSize ? options.fontSize : 16.0;
    double small = std::round(size * 0.875);
    double tiny = std::round(size * 0.75);
    double least = std::round(size * 0.5625);
    const TextMeasurer& measurer = getMeasurer(options);
    const Config* treemap = config.get("treemap");
    double pad = options.hasPadding ? options.padding : treemapOption(config, "diagramPadding", 8.0, 0.0, 2000.0);

    double width = std::round(capWidth(options, treemapOption(config, "nodeWidth", 64.0, 1.0, 400.0) * 10.0, 200.0, 2.0 * pad));
    double height = std::round(treemapOption(config, "nodeHeight", 40.0, 1.0, 400.0) * 10.0);
    bool showValues = !(treemap && treemap->isMap() && treemap->get("showValues") && treemap->get("showValues")->isBool() && !treemap->get("showValues")->bVal);
    std::string valFmtSpec = ",";
    if (treemap && treemap->isMap()) {
        if (const auto* vf = treemap->get("valueFormat"); vf && vf->isString()) {
            valFmtSpec = vf->sVal;
        }
    }
    auto format = treemapFormatter(valFmtSpec);
    double crowd = std::min(1.0, std::max(0.0, std::log10((width * height) / std::max(1, leafCount(model.roots)) / 800.0)));
    auto boxes = layoutTreemap(model.roots, width, height, {
        treemapOption(config, "padding", std::round(2.0 + 2.0 * crowd), 0.0, 200.0),
        HEADER,
        SHORT_HEADER,
        std::round(3.0 + 3.0 * crowd)
    });

    Label title = layoutLabel(model.title, false, measurer, size, treemapTitleRoom(options), Style::Bold);
    double titleHeight = title.height > 0.0 ? title.height + 8.0 : 0.0;
    double left = pad + std::max(0.0, (title.width - width) / 2.0);
    double top = pad + titleHeight;

    std::string sections;
    std::string leaves;
    for (const auto& bPtr : boxes) {
        const Box& b = *bPtr;
        const TreemapNode& node = *b.node;
        ResolvedStyle style = resolveStyle(node.cssCompiledStyles);
        std::string color = seriesColor(b.series);
        double x = left + b.x;
        double y = top + b.y;
        std::string name = trimStr(decodeEntities(node.name));
        std::string value = showValues ? format(b.value) : "";
        std::string group = Pele::cat({"data-id=\"", escText(node.name), "\">"});

        std::string own = titlePart(node.name);
        std::vector<std::string> path;
        if (!own.empty()) path.push_back(own);
        for (auto p = b.parent; p != nullptr && !p->headed; p = p->parent) {
            std::string section = titlePart(p->node->name);
            if (!section.empty()) path.insert(path.begin(), section);
        }
        std::string full;
        for (size_t k = 0; k < path.size(); k++) {
            if (k > 0) full += " \xE2\x80\xBA ";
            full += path[k];
        }
        auto tooltip = [&](const std::optional<std::string>& drawn) -> std::string {
            if (full.empty() || (drawn.has_value() && *drawn == name && full == own)) return "";
            return Pele::cat({"<title>", full, "</title>"});
        };
        std::string rect = Pele::cat({"<rect x=\"", num(x), "\" y=\"", num(y), "\" width=\"", num(b.w), "\" height=\"", num(b.h), "\" fill=\"", color, "\""});
        std::optional<double> fixed = style.hasFontSize ? std::optional<double>(style.fontSize) : std::nullopt;
        int textStyle = (style.bold ? Style::Bold : 0) | (style.italic ? Style::Italic : 0);
        std::string inkAttr = inkFill(style);

        if (node.isSection) {
            std::string text;
            std::optional<std::string> drawn;
            if (b.headed) {
                bool isShort = b.header < HEADER;
                double labelSize = fixed.value_or(isShort ? tiny : small);
                double valueSize = fixed.value_or(tiny);
                double room = b.w - 2.0 * SIDE - 4.0;
                double valueWidth = !value.empty() ? measurer.width(value, valueSize, textStyle) : 0.0;
                bool withValue = !isShort && valueWidth > 0.0 && (room - valueWidth - 12.0 >= 3.0 * labelSize);
                std::optional<FittedText> label;
                if (labelSize * LINE < b.header) {
                    label = fitText(name, withValue ? (room - valueWidth - 12.0) : room, labelSize, labelSize, measurer, textStyle | Style::Bold);
                }
                double baseline = y + b.header / 2.0 + 1.0;
                if (label.has_value()) {
                    drawn = label->text;
                    Pele::appendCat(text, {"<text x=\"", num(x + SIDE + 2.0), "\" y=\"", num(baseline + labelSize * 0.35), "\" font-weight=\"var(--_hw)\"", (isShort ? Pele::cat({" font-size=\"", num(labelSize), "\""}) : ""), inkAttr, style.text, ">", esc(label->text), "</text>"});
                }
                if (withValue && valueSize * LINE <= HEADER) {
                    Pele::appendCat(text, {"<text class=\"pele-value\" x=\"", num(x + b.w - SIDE - 2.0), "\" y=\"", num(baseline + valueSize * 0.35), "\" font-size=\"", num(tiny), "\" text-anchor=\"end\"", (!inkAttr.empty() ? inkAttr : " fill=\"var(--_m)\""), style.text, ">", esc(value), "</text>"});
                }
            }
            std::string tint = setsFill(style.shape) ? "" : " fill-opacity=\"0.12\"";
            std::string shape = b.headed
                ? Pele::cat({rect, corners(Pele::cat({tint, " stroke=\"", color, "\"", style.shape}),  b), "/>"})
                : Pele::cat({"<rect x=\"", num(x), "\" y=\"", num(y), "\" width=\"", num(b.w), "\" height=\"", num(b.h), "\" fill=\"none\"/>"});
            Pele::appendCat(sections, {"<g class=\"pele-node pele-section", classNames(node.classSelector), "\" ", group, tooltip(drawn), shape, text, "</g>"});
            continue;
        }

        std::string text;
        double room = b.w - 8.0;
        double smallest = fixed.value_or(least);
        double tallest = !fixed.has_value()
            ? std::min(size, std::floor((b.h - 4.0) / LINE))
            : ((*fixed * LINE <= b.h - 4.0) ? *fixed : 0.0);
        std::optional<FittedText> label;
        if (tallest >= smallest) {
            label = fitText(name, room, tallest, smallest, measurer, textStyle);
        }
        std::optional<std::string> drawnLeaf;
        if (label.has_value()) {
            drawnLeaf = label->text;
            std::string cx = num(x + b.w / 2.0);
            double cy = y + b.h / 2.0;
            double valueSize = fixed.value_or(std::max(least, std::round(label->size * 0.8)));
            double lines = (label->size + valueSize) * LINE;
            if (!value.empty() && lines <= b.h - 4.0 && measurer.width(value, valueSize, textStyle) <= room) {
                double first = cy - lines / 2.0 + (label->size * LINE) / 2.0;
                double second = cy + lines / 2.0 - (valueSize * LINE) / 2.0;
                text = Pele::cat({"<text x=\"", cx, "\" y=\"", num(first + label->size * 0.35), "\" font-size=\"", num(label->size), "\"", inkAttr, style.text, ">", esc(label->text), "</text>", "<text class=\"pele-value\" x=\"", cx, "\" y=\"", num(second + valueSize * 0.35), "\" font-size=\"", num(valueSize), "\"", inkAttr, style.text, ">", esc(value), "</text>"});
            } else {
                text = Pele::cat({"<text x=\"", cx, "\" y=\"", num(cy + label->size * 0.35), "\" font-size=\"", num(label->size), "\"", inkAttr, style.text, ">", esc(label->text), "</text>"});
            }
        }
        Pele::appendCat(leaves, {"<g class=\"pele-node pele-leaf", classNames(node.classSelector), "\" ", group, tooltip(drawnLeaf), rect, corners(style.shape, b), "/>", text, "</g>"});
    }

    double totalWidth = std::ceil(std::max(width, title.width) + 2.0 * pad);
    double totalHeight = std::ceil(height + titleHeight + 2.0 * pad);
    std::string content =
        labelSvg(title, totalWidth / 2.0, pad + title.height / 2.0, " class=\"pele-title\" font-weight=\"var(--_tw)\"") +
        (!sections.empty() ? Pele::cat({"<g class=\"pele-sections\" font-size=\"", num(small), "\">", sections, "</g>"}) : "") +
        (!leaves.empty() ? Pele::cat({"<g class=\"pele-leaves\" text-anchor=\"middle\" fill=\"var(--_bg)\">", leaves, "</g>"}) : "");

    return {
        svgDocument("treemap", totalWidth, totalHeight, size, options, model, content),
        totalWidth,
        totalHeight,
        {}
    };
}

inline Rendered render(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options = {}) {
    TreemapModel model = buildTreemap(parseTreemap(source), title);
    return renderTreemapModel(model, config, options);
}

} // namespace TreemapDiagram

inline Rendered renderTreemap(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options = {}) {
    return TreemapDiagram::render(source, config, title, options);
}

} // namespace Pele
