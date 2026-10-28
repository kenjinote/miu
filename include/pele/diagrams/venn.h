#pragma once
#include "../foundation.h"

namespace Pele {
namespace VennDiagram {

// ============================================================================
// src/diagrams/venn/types.ts & db.ts
// ============================================================================

struct VennSubset {
    std::vector<std::string> sets;
    double size = 10.0;
    std::string label;
    bool hasLabel = false;
};

struct VennText {
    std::vector<std::string> sets;
    std::string id;
    std::string label;
    bool hasLabel = false;
};

struct VennStyle {
    std::vector<std::string> targets;
    std::vector<std::pair<std::string, std::string>> styles;
};

inline std::string normalizeText(const std::string& text) noexcept {
    std::string trimmed = trimStr(text);
    if (trimmed.size() >= 2 && trimmed.front() == '"' && trimmed.back() == '"') {
        return trimmed.substr(1, trimmed.size() - 2);
    }
    return trimmed;
}

inline std::vector<std::string> normalizeIds(const std::vector<std::string>& ids) noexcept {
    std::vector<std::string> out;
    out.reserve(ids.size());
    for (const auto& id : ids) out.push_back(normalizeText(id));
    std::sort(out.begin(), out.end());
    return out;
}

class VennDb : public DiagramMeta {
public:
    std::vector<VennSubset> subsets;
    std::vector<VennText> textNodes;
    std::vector<VennStyle> styles;
    Pele::FlatSet<std::string> known;
    std::vector<std::string> current;
    bool hasCurrent = false;
    bool indent = false;

    void setDiagramTitle(const std::string& t) {
        title = t;
    }

    void addSubsetData(const std::vector<std::string>& ids, const std::string& label, bool hasLabel, double size, bool hasSize) {
        auto sets = normalizeIds(ids);
        current = sets;
        hasCurrent = true;
        if (sets.size() == 1) known.insert(sets[0]);
        double n = static_cast<double>(ids.size());
        double resolvedSize = hasSize ? size : (10.0 / (n * n));
        subsets.push_back({sets, resolvedSize, hasLabel ? normalizeText(label) : "", hasLabel});
    }

    void addTextData(const std::vector<std::string>& ids, const std::string& id, const std::string& label, bool hasLabel) {
        textNodes.push_back({normalizeIds(ids), normalizeText(id), hasLabel ? normalizeText(label) : "", hasLabel});
    }

    void addStyleData(const std::vector<std::string>& ids, const std::vector<std::pair<std::string, std::string>>& data) {
        std::vector<std::pair<std::string, std::string>> mapVec;
        for (const auto& [key, value] : data) {
            std::string normVal = !value.empty() ? normalizeText(value) : value;
            bool found = false;
            for (auto& kv : mapVec) {
                if (kv.first == key) {
                    kv.second = normVal;
                    found = true;
                    break;
                }
            }
            if (!found) mapVec.push_back({key, normVal});
        }
        styles.push_back({normalizeIds(ids), std::move(mapVec)});
    }

    void validateUnionIdentifiers(const std::vector<std::string>& ids) {
        std::vector<std::string> unknown;
        for (const auto& raw : ids) {
            std::string id = normalizeText(raw);
            if (!known.count(id)) unknown.push_back(id);
        }
        if (!unknown.empty()) {
            std::string joined;
            for (size_t i = 0; i < unknown.size(); i++) {
                if (i > 0) joined += ", ";
                joined += unknown[i];
            }
            throw PeleError(Pele::cat({"unknown set identifier: ", joined}),  "semantic",  "venn");
        }
    }

    bool getIndentMode() const { return indent; }
    void setIndentMode(bool enabled) { indent = enabled; }
};

// ============================================================================
// src/diagrams/venn/lexer.ts & parser.ts
// ============================================================================

namespace T {
    enum Type {
        END,
        ERROR,
        EOF_TOK,
        NEWLINE,
        VENN,
        TITLE,
        SET,
        UNION,
        TEXT,
        INDENT_TEXT,
        STYLE,
        BRACKET_LABEL,
        NUMERIC,
        HEXCOLOR,
        RGBCOLOR,
        RGBACOLOR,
        IDENTIFIER,
        STRING,
        COMMA,
        COLON
    };
}

inline const char* tokenName(int t) noexcept {
    static const char* NAMES[] = {
        "$end", "INVALID", "EOF", "NEWLINE", "VENN", "TITLE", "SET", "UNION",
        "TEXT", "INDENT_TEXT", "STYLE", "BRACKET_LABEL", "NUMERIC", "HEXCOLOR",
        "RGBCOLOR", "RGBACOLOR", "IDENTIFIER", "STRING", "COMMA", "COLON"
    };
    return (t >= 0 && t <= T::COLON) ? NAMES[t] : "INVALID";
}

class VennLexer {
public:
    std::string text;
    size_t start = 0;

    VennLexer(const std::string& src_, VennDb& host_) : src(src_), host(host_) {}

    int next() {
        size_t n = src.size();
        for (;;) {
            size_t i = at;
            start = i;
            text.clear();
            if (i >= n) {
                if (ended) return T::END;
                ended = true;
                return T::EOF_TOK;
            }
            int c = charCodeAt(src, static_cast<int>(i));
            if (c == 37 && charCodeAt(src, static_cast<int>(i + 1)) == 37 && charCodeAt(src, static_cast<int>(i + 2)) != 123) {
                at = lineEnd(i + 2);
                continue;
            }
            if (lineStart && word(i, "title") && afterLineFeed(i) && trimStr(src.substr(i + 5, lineEnd(i + 5) - (i + 5))).empty()) {
                size_t j = lineEnd(i + 5);
                while (charCodeAt(src, static_cast<int>(j)) == 10 || charCodeAt(src, static_cast<int>(j)) == 13) j++;
                at = j;
                continue;
            }
            if (lineStart) {
                if (c == 32 || c == 9) {
                    size_t j = i + 1;
                    while (charCodeAt(src, static_cast<int>(j)) == 32 || charCodeAt(src, static_cast<int>(j)) == 9) j++;
                    at = j;
                    if (keyword(j, "text") && host.getIndentMode()) {
                        at = j + 4;
                        lineStart = false;
                        text = src.substr(i, j - i);
                        return T::INDENT_TEXT;
                    }
                    continue;
                }
                if (c != 10 && c != 13) {
                    host.setIndentMode(false);
                    lineStart = false;
                }
            }
            if (c == 10 || c == 13) {
                i++;
                while (charCodeAt(src, static_cast<int>(i)) == 10 || charCodeAt(src, static_cast<int>(i)) == 13) i++;
                lineStart = true;
                return token(T::NEWLINE, i);
            }
            if (c == 37 && charCodeAt(src, static_cast<int>(i + 1)) == 37) {
                at = lineEnd(i);
                continue;
            }
            if (c == 32 || c == 9) {
                i++;
                while (charCodeAt(src, static_cast<int>(i)) == 32 || charCodeAt(src, static_cast<int>(i)) == 9) i++;
                at = i;
                continue;
            }

            if (((c | 32) >= 97 && (c | 32) <= 122) || c == 95) {
                int sep = charCodeAt(src, static_cast<int>(i + 5));
                if (word(i, "title") && isSpace(sep) && sep != 10 && sep != 13) {
                    size_t end = i + 6;
                    while (end < n) {
                        int ch = charCodeAt(src, static_cast<int>(end));
                        if (ch == 35 || ch == 10 || ch == 59) break;
                        end++;
                    }
                    if (end > i + 6) return token(T::TITLE, end);
                }
                static const std::pair<const char*, int> KEYWORDS[] = {
                    {"venn-beta", T::VENN},
                    {"set", T::SET},
                    {"union", T::UNION},
                    {"text", T::TEXT},
                    {"style", T::STYLE}
                };
                for (const auto& [kw, kwType] : KEYWORDS) {
                    size_t kwLen = std::strlen(kw);
                    if (keyword(i, kw) && charCodeAt(src, static_cast<int>(i + kwLen)) != 45) {
                        return token(kwType, i + kwLen);
                    }
                }
                if (word(i, "rgb")) {
                    bool alpha = (charCodeAt(src, static_cast<int>(i + 3)) | 32) == 97;
                    size_t open = i + (alpha ? 4 : 3);
                    int end = (charCodeAt(src, static_cast<int>(open)) == 40) ? color(open, alpha ? 4 : 3) : -1;
                    if (end != -1) return token(alpha ? T::RGBACOLOR : T::RGBCOLOR, static_cast<size_t>(end));
                }
                i++;
                while (isWord(charCodeAt(src, static_cast<int>(i))) || charCodeAt(src, static_cast<int>(i)) == 45) i++;
                return token(T::IDENTIFIER, i);
            }
            if (c == 91) {
                if (charCodeAt(src, static_cast<int>(i + 1)) == 34) {
                    size_t quote = src.find('"', i + 2);
                    if (quote == std::string::npos || charCodeAt(src, static_cast<int>(quote + 1)) != 93) return T::ERROR;
                    at = quote + 2;
                    text = src.substr(i + 2, quote - (i + 2));
                    return T::BRACKET_LABEL;
                }
                size_t end = i + 1;
                while (end < n && charCodeAt(src, static_cast<int>(end)) != 93 && charCodeAt(src, static_cast<int>(end)) != 34) end++;
                if (end == i + 1 || charCodeAt(src, static_cast<int>(end)) != 93) return T::ERROR;
                at = end + 1;
                text = trimStr(src.substr(i + 1, end - (i + 1)));
                return T::BRACKET_LABEL;
            }
            if (isDigit(c) || c == 46 || c == 43 || c == 45) {
                if (c == 43 || c == 45) i++;
                size_t from = i;
                while (isDigit(charCodeAt(src, static_cast<int>(i)))) i++;
                if (i > from) {
                    if (charCodeAt(src, static_cast<int>(i)) == 46 && isDigit(charCodeAt(src, static_cast<int>(i + 1)))) {
                        i += 2;
                        while (isDigit(charCodeAt(src, static_cast<int>(i)))) i++;
                    }
                } else if (charCodeAt(src, static_cast<int>(i)) == 46 && isDigit(charCodeAt(src, static_cast<int>(i + 1)))) {
                    i += 2;
                    while (isDigit(charCodeAt(src, static_cast<int>(i)))) i++;
                } else {
                    return T::ERROR;
                }
                return token(T::NUMERIC, i);
            }
            if (c == 35) {
                size_t end = i + 1;
                while (end < i + 9 && isHex(charCodeAt(src, static_cast<int>(end)))) end++;
                return end >= i + 4 ? token(T::HEXCOLOR, end) : T::ERROR;
            }
            if (c == 34) {
                size_t quote = src.find('"', i + 1);
                return quote == std::string::npos ? T::ERROR : token(T::STRING, quote + 1);
            }
            if (c == 44) return token(T::COMMA, i + 1);
            if (c == 58) return token(T::COLON, i + 1);
            return T::ERROR;
        }
    }

private:
    const std::string& src;
    VennDb& host;
    size_t at = 0;
    bool lineStart = false;
    bool ended = false;

    bool word(size_t pos, const char* w) const {
        for (size_t k = 0; w[k] != '\0'; k++) {
            int c = charCodeAt(src, static_cast<int>(pos + k));
            int wc = static_cast<unsigned char>(w[k]);
            if (c != wc && (c + 32 != wc || c < 65 || c > 90)) return false;
        }
        return true;
    }

    bool keyword(size_t pos, const char* w) const {
        size_t len = std::strlen(w);
        return word(pos, w) && !isWord(charCodeAt(src, static_cast<int>(pos + len)));
    }

    int color(size_t pos, int parts) const {
        size_t i = pos + 1;
        for (int part = 0; part < parts; part++) {
            while (isSpace(charCodeAt(src, static_cast<int>(i)))) i++;
            size_t from = i;
            while (isDigit(charCodeAt(src, static_cast<int>(i))) || charCodeAt(src, static_cast<int>(i)) == 46) i++;
            if (i == from) return -1;
            while (isSpace(charCodeAt(src, static_cast<int>(i)))) i++;
            if (charCodeAt(src, static_cast<int>(i)) != (part == parts - 1 ? 41 : 44)) return -1;
            i++;
        }
        return static_cast<int>(i);
    }

    bool afterLineFeed(size_t pos) const {
        int k = static_cast<int>(pos) - 1;
        while (charCodeAt(src, k) == 32 || charCodeAt(src, k) == 9) k--;
        return charCodeAt(src, k) == 10;
    }

    size_t lineEnd(size_t from) const {
        size_t end = src.find('\n', from);
        return end == std::string::npos ? src.size() : end;
    }

    int token(int type, size_t end) {
        text = src.substr(at, end - at);
        at = end;
        return type;
    }
};

inline void parseVenn(const std::string& src, VennDb& db) {
    const std::string STATEMENT = "'EOF', 'NEWLINE', 'TITLE', 'SET', 'UNION', 'TEXT', 'INDENT_TEXT', 'STYLE'";
    const std::string VALUE = "'NUMERIC', 'IDENTIFIER', 'STRING', 'HEXCOLOR', 'RGBCOLOR', 'RGBACOLOR'";

    VennLexer lexer(src, db);
    int type = -1;

    auto peek = [&]() -> int {
        return type == -1 ? (type = lexer.next()) : type;
    };

    auto fail = [&](const std::string& expected) {
        if (peek() == T::ERROR) throw syntaxError("venn", src, lexer.start, "", true);
        throw syntaxError("venn",  src,  lexer.start, Pele::cat({"Expecting ", expected, ", got '", tokenName(type), "'"}));
    };

    auto take = [&]() -> std::string {
        type = -1;
        return lexer.text;
    };

    auto expect = [&](int wanted) -> std::string {
        if (peek() != wanted) fail(std::string("'") + tokenName(wanted) + "'");
        return take();
    };

    auto identifier = [&]() -> std::string {
        if (peek() != T::IDENTIFIER && type != T::STRING) fail("'IDENTIFIER', 'STRING'");
        return take();
    };

    auto identifiers = [&]() -> std::vector<std::string> {
        std::vector<std::string> list = {identifier()};
        while (peek() == T::COMMA) {
            take();
            list.push_back(identifier());
        }
        return list;
    };

    auto subset = [&](const std::vector<std::string>& ids, bool isUnion) {
        bool hasLabel = (peek() == T::BRACKET_LABEL);
        std::string label = hasLabel ? take() : "";
        bool hasSize = false;
        double size = 0.0;
        if (peek() == T::COLON) {
            take();
            size = std::stod(expect(T::NUMERIC));
            hasSize = true;
        }
        if (isUnion) {
            if (ids.size() < 2) throw PeleError("union requires multiple identifiers", "semantic", "venn");
            db.validateUnionIdentifiers(ids);
        }
        db.addSubsetData(ids, label, hasLabel, size, hasSize);
        db.setIndentMode(true);
    };

    auto textStmt = [&](auto&& getSets) {
        int kind = peek();
        if (kind != T::IDENTIFIER && kind != T::STRING && kind != T::NUMERIC) fail("'NUMERIC', 'IDENTIFIER', 'STRING'");
        std::string id = take();
        bool hasLabel = (kind != T::NUMERIC && peek() == T::BRACKET_LABEL);
        std::string label = hasLabel ? take() : "";
        db.addTextData(getSets(), id, label, hasLabel);
    };

    auto styleStmt = [&]() {
        auto ids = identifiers();
        std::vector<std::pair<std::string, std::string>> styles;
        for (;;) {
            std::string key = expect(T::IDENTIFIER);
            expect(T::COLON);
            std::string value;
            if (peek() == T::STRING) {
                value = take();
            } else {
                std::vector<std::string> parts;
                while (peek() == T::IDENTIFIER || (type >= T::NUMERIC && type <= T::RGBACOLOR)) {
                    parts.push_back(take());
                }
                if (parts.empty()) fail(VALUE);
                for (size_t i = 0; i < parts.size(); i++) {
                    if (i > 0) value.push_back(' ');
                    value += parts[i];
                }
            }
            styles.push_back({key, value});
            if (peek() != T::COMMA) break;
            take();
        }
        db.addStyleData(ids, styles);
    };

    while (peek() == T::NEWLINE) take();
    expect(T::VENN);
    for (;;) {
        int statement = peek();
        if (statement == T::EOF_TOK) return;
        if (statement < T::NEWLINE || statement > T::STYLE || statement == T::VENN) fail(STATEMENT);
        take();
        if (statement == T::TITLE) db.setDiagramTitle(lexer.text.substr(6));
        else if (statement == T::SET) subset({identifier()}, false);
        else if (statement == T::UNION) subset(identifiers(), true);
        else if (statement == T::TEXT) {
            auto ids = identifiers();
            textStmt([&]() { return ids; });
        } else if (statement == T::INDENT_TEXT) {
            textStmt([&]() {
                if (!db.hasCurrent) throw PeleError("text requires set", "semantic", "venn");
                return db.current;
            });
        } else if (statement == T::STYLE) styleStmt();
    }
}

// ============================================================================
// src/diagrams/venn/layout.ts
// ============================================================================

struct Circle {
    std::string id;
    double x = 0.0;
    double y = 0.0;
    double r = 0.0;
};

struct Spot {
    double x = 0.0;
    double y = 0.0;
    double margin = 0.0;
};

constexpr double VENN_PI = 3.14159265358979323846;
constexpr double VENN_TAU = VENN_PI * 2.0;

inline std::string jsonStrArray(const std::vector<std::string>& arr) noexcept {
    std::string out = "[";
    for (size_t i = 0; i < arr.size(); i++) {
        if (i > 0) out.push_back(',');
        out.push_back('"');
        for (char c : arr[i]) {
            if (c == '"' || c == '\\') out.push_back('\\');
            out.push_back(c);
        }
        out.push_back('"');
    }
    out.push_back(']');
    return out;
}

inline std::string keyOf(const std::vector<std::string>& ids) noexcept {
    return ids.size() == 1 ? ids[0] : jsonStrArray(ids);
}

inline std::vector<VennSubset> ensurePairwiseSubsets(const std::vector<VennSubset>& subsets) noexcept {
    Pele::FlatSet<std::string> present;
    Pele::FlatMap<std::string, double> sizes;
    for (const auto& subset : subsets) {
        auto sorted = subset.sets;
        std::sort(sorted.begin(), sorted.end());
        present.insert(jsonStrArray(sorted));
        if (subset.sets.size() == 1) sizes[subset.sets[0]] = subset.size;
    }
    std::vector<VennSubset> added;
    for (const auto& subset : subsets) {
        if (subset.sets.size() < 3) continue;
        auto members = subset.sets;
        std::sort(members.begin(), members.end());
        for (size_t i = 0; i + 1 < members.size(); i++) {
            for (size_t j = i + 1; j < members.size(); j++) {
                std::vector<std::string> pair = {members[i], members[j]};
                std::string key = jsonStrArray(pair);
                if (present.count(key)) continue;
                present.insert(key);
                auto itA = sizes.find(pair[0]);
                auto itB = sizes.find(pair[1]);
                double sz = (itA != sizes.end() && itB != sizes.end()) ? (std::min(itA->second, itB->second) / 4.0) : 2.5;
                added.push_back({pair, sz, "", true});
            }
        }
    }
    if (added.empty()) return subsets;
    std::vector<VennSubset> res = subsets;
    res.insert(res.end(), added.begin(), added.end());
    return res;
}

inline double lensArea(double r1, double r2, double d) noexcept {
    if (d >= r1 + r2) return 0.0;
    double small = std::min(r1, r2);
    if (d <= std::abs(r1 - r2)) return VENN_PI * small * small;
    double a1 = r1 * r1 * std::acos((d * d + r1 * r1 - r2 * r2) / (2.0 * d * r1));
    double a2 = r2 * r2 * std::acos((d * d + r2 * r2 - r1 * r1) / (2.0 * d * r2));
    double a3 = 0.5 * std::sqrt(std::max((-d + r1 + r2) * (d + r1 - r2) * (d - r1 + r2) * (d + r1 + r2), 0.0));
    return a1 + a2 - a3;
}

inline double distanceForOverlap(double r1, double r2, double area) noexcept {
    double low = std::abs(r1 - r2);
    double high = r1 + r2;
    if (!(area > 0.0)) return high;
    double minR = std::min(r1, r2);
    if (area >= VENN_PI * minR * minR) return low;
    for (int i = 0; i < 48; i++) {
        double middle = (low + high) / 2.0;
        if (lensArea(r1, r2, middle) > area) low = middle;
        else high = middle;
    }
    return (low + high) / 2.0;
}

inline double circleMargin(const std::vector<Circle>& circles, const std::vector<int>& inside, const std::vector<int>& outside, double x, double y, double floor) noexcept {
    double best = std::numeric_limits<double>::infinity();
    for (int i : inside) {
        const auto& c = circles[i];
        double dx = x - c.x;
        double dy = y - c.y;
        double room = c.r - std::sqrt(dx * dx + dy * dy);
        if (room < best) {
            if (room < floor) return room;
            best = room;
        }
    }
    for (int i : outside) {
        const auto& c = circles[i];
        double dx = x - c.x;
        double dy = y - c.y;
        double room = std::sqrt(dx * dx + dy * dy) - c.r;
        if (room < best) {
            if (room < floor) return room;
            best = room;
        }
    }
    return best;
}

inline Spot findSpot(const std::vector<Circle>& circles, const std::vector<int>& inside, const std::vector<int>& outside) noexcept {
    double cx = 0.0;
    double cy = 0.0;
    const Circle* smallest = &circles[inside[0]];
    for (int i : inside) {
        const auto& c = circles[i];
        cx += c.x / static_cast<double>(inside.size());
        cy += c.y / static_cast<double>(inside.size());
        if (c.r < smallest->r) smallest = &c;
    }
    double r = smallest->r;
    double x = cx;
    double y = cy;
    double best = circleMargin(circles, inside, outside, x, y, -std::numeric_limits<double>::infinity());
    double off = 0.0;
    auto consider = [&](double px, double py) -> bool {
        double m = circleMargin(circles, inside, outside, px, py, best - r * 1e-9);
        if (!(m > best - r * 1e-9)) return false;
        double distance = std::hypot(px - cx, py - cy);
        if (m > best + r * 1e-9 || distance < off - r * 1e-9) {
            best = m;
            off = distance;
            x = px;
            y = py;
            return true;
        }
        return false;
    };
    constexpr int GRID = 10;
    for (int gy = -GRID; gy <= GRID; gy++) {
        for (int gx = -GRID; gx <= GRID; gx++) {
            if (gx * gx + gy * gy <= GRID * GRID) {
                consider(smallest->x + (gx * r) / GRID, smallest->y + (gy * r) / GRID);
            }
        }
    }
    double step = r / GRID;
    for (int round = 0; round < 40 && step > r * 1e-6; round++) {
        bool moved = false;
        for (int k = 0; k < 8; k++) {
            double angle = (k * VENN_TAU) / 8.0;
            if (consider(x + std::cos(angle) * step, y + std::sin(angle) * step)) moved = true;
        }
        if (!moved) step /= 2.0;
    }
    return {x, y, best};
}

inline std::vector<Circle> layoutCircles(const std::vector<VennSubset>& subsets) noexcept {
    std::vector<std::string> sizeOrder;
    Pele::FlatMap<std::string, double> sizes;
    auto setSize = [&](const std::string& id, double val, bool onlyIfNew) {
        auto it = sizes.find(id);
        if (it == sizes.end()) {
            sizeOrder.push_back(id);
            sizes[id] = val;
        } else if (!onlyIfNew) {
            it->second = val;
        }
    };
    for (const auto& subset : subsets) {
        for (const auto& id : subset.sets) setSize(id, 10.0, true);
    }
    for (const auto& subset : subsets) {
        if (subset.sets.size() == 1) setSize(subset.sets[0], subset.size, false);
    }

    Pele::FlatMap<std::string, int> index;
    std::vector<Circle> circles;
    double largest = 0.0;
    for (const auto& id : sizeOrder) {
        double sz = sizes[id];
        if (std::isfinite(sz) && sz > largest) largest = sz;
    }
    if (largest == 0.0) largest = 10.0;
    double widest = std::sqrt(largest / VENN_PI);
    for (const auto& id : sizeOrder) {
        double sz = sizes[id];
        index[id] = static_cast<int>(circles.size());
        double r = (std::isfinite(sz) && sz > 0.0) ? std::sqrt(sz / VENN_PI) : 0.0;
        circles.push_back({id, 0.0, 0.0, std::max(r, widest * 0.3)});
    }
    int n = static_cast<int>(circles.size());
    if (n < 2) return circles;

    Pele::FlatMap<int, double> overlaps;
    for (const auto& subset : ensurePairwiseSubsets(subsets)) {
        if (subset.sets.size() != 2) continue;
        int a = index[subset.sets[0]];
        int b = index[subset.sets[1]];
        if (a != b) overlaps[std::min(a, b) * n + std::max(a, b)] = subset.size;
    }
    Pele::FlatMap<int, double> wanted;
    for (const auto& [key, area] : overlaps) {
        wanted[key] = distanceForOverlap(circles[key / n].r, circles[key % n].r, area);
    }
    double gap = widest * 0.15;
    auto distance = [&](int a, int b) -> double {
        auto it = wanted.find(a * n + b);
        return it != wanted.end() ? it->second : (circles[a].r + circles[b].r + gap);
    };

    if (n == 2) {
        circles[1].x = distance(0, 1);
        return circles;
    }

    if (n == 3) {
        double ab = distance(0, 1);
        double ac = distance(0, 2);
        double bc = distance(1, 2);
        bool triple = false;
        for (const auto& subset : subsets) {
            Pele::FlatSet<std::string> uniq(subset.sets.begin(), subset.sets.end());
            if (uniq.size() == 3) { triple = true; break; }
        }
        for (int attempt = 0; attempt < 40; attempt++) {
            double x = ab > 0.0 ? (ab * ab + ac * ac - bc * bc) / (2.0 * ab) : 0.0;
            circles[1].x = ab;
            circles[2].x = std::max(-ac, std::min(ac, x));
            circles[2].y = std::sqrt(std::max(ac * ac - x * x, 0.0));
            if (!triple || findSpot(circles, {0, 1, 2}, {}).margin > widest * 0.12) break;
            ab *= 0.94;
            ac *= 0.94;
            bc *= 0.94;
        }
        return circles;
    }

    double total = 0.0;
    for (const auto& c : circles) total += 2.0 * c.r + gap;
    int rounds = std::min(300, static_cast<int>(std::floor(3e6 / (static_cast<double>(n) * n))));
    if (n <= 64) {
        double ring = total / VENN_TAU;
        for (int i = 0; i < n; i++) {
            double angle = (i * VENN_TAU) / n - VENN_PI / 2.0;
            circles[i].x = std::cos(angle) * ring;
            circles[i].y = std::sin(angle) * ring;
        }
    } else {
        int columns = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(n))));
        double cell = 2.0 * widest + gap;
        for (int i = 0; i < n; i++) {
            circles[i].x = (i % columns) * cell;
            circles[i].y = (i / columns) * cell;
        }
    }
    std::vector<double> dx(n, 0.0);
    std::vector<double> dy(n, 0.0);
    std::vector<int> pulls(n, 0);
    for (int round = 0; round < rounds; round++) {
        std::fill(dx.begin(), dx.end(), 0.0);
        std::fill(dy.begin(), dy.end(), 0.0);
        std::fill(pulls.begin(), pulls.end(), 0);
        for (int a = 0; a < n; a++) {
            for (int b = a + 1; b < n; b++) {
                auto it = wanted.find(a * n + b);
                bool hasOverlap = (it != wanted.end());
                double apart = circles[a].r + circles[b].r + gap;
                double ux = circles[b].x - circles[a].x;
                double uy = circles[b].y - circles[a].y;
                double length = std::hypot(ux, uy);
                if (!hasOverlap && length >= apart) continue;
                if (length < 1e-9) {
                    ux = std::cos(static_cast<double>(a + b));
                    uy = std::sin(static_cast<double>(a + b));
                    length = 1.0;
                }
                double targetDist = hasOverlap ? it->second : apart;
                double move = (length - targetDist) / 2.0 / length;
                dx[a] += ux * move;
                dy[a] += uy * move;
                dx[b] -= ux * move;
                dy[b] -= uy * move;
                pulls[a]++;
                pulls[b]++;
            }
        }
        for (int i = 0; i < n; i++) {
            if (pulls[i] == 0) continue;
            circles[i].x += dx[i] / pulls[i];
            circles[i].y += dy[i] / pulls[i];
        }
    }
    return circles;
}

inline std::string regionPath(const std::vector<Circle>& circles) noexcept {
    auto whole = [](const Circle& c) -> std::string {
        return Pele::cat({"M", num(c.x - c.r), ",", num(c.y), "A", num(c.r), ",", num(c.r), " 0 1 1 ", num(c.x + c.r), ",", num(c.y), "A", num(c.r), ",", num(c.r), " 0 1 1 ", num(c.x - c.r), ",", num(c.y), "Z"});
    };
    if (circles.size() == 1) return whole(circles[0]);
    auto within = [&](double x, double y, size_t skip, size_t other) -> bool {
        for (size_t k = 0; k < circles.size(); k++) {
            if (k != skip && k != other && std::hypot(x - circles[k].x, y - circles[k].y) > circles[k].r * (1.0 + 1e-9)) return false;
        }
        return true;
    };
    struct Corner {
        double x;
        double y;
        size_t a;
        size_t b;
    };
    std::vector<Corner> corners;
    for (size_t i = 0; i < circles.size(); i++) {
        for (size_t j = i + 1; j < circles.size(); j++) {
            const auto& a = circles[i];
            const auto& b = circles[j];
            double d = std::hypot(b.x - a.x, b.y - a.y);
            if (d >= a.r + b.r || d <= std::abs(a.r - b.r) || d == 0.0) continue;
            double along = (a.r * a.r - b.r * b.r + d * d) / (2.0 * d);
            double up = std::sqrt(std::max(a.r * a.r - along * along, 0.0));
            double mx = a.x + ((b.x - a.x) * along) / d;
            double my = a.y + ((b.y - a.y) * along) / d;
            for (int sign : {1, -1}) {
                double x = mx + (sign * up * (b.y - a.y)) / d;
                double y = my - (sign * up * (b.x - a.x)) / d;
                if (within(x, y, i, j)) corners.push_back({x, y, i, j});
            }
        }
    }
    if (corners.size() < 2) {
        size_t smallestIdx = 0;
        for (size_t k = 1; k < circles.size(); k++) {
            if (circles[k].r < circles[smallestIdx].r) smallestIdx = k;
        }
        const auto& smallest = circles[smallestIdx];
        for (size_t k = 0; k < circles.size(); k++) {
            if (k != smallestIdx && std::hypot(circles[k].x - smallest.x, circles[k].y - smallest.y) + smallest.r > circles[k].r * (1.0 + 1e-9)) {
                return "";
            }
        }
        return whole(smallest);
    }
    double cx = 0.0;
    double cy = 0.0;
    for (const auto& p : corners) {
        cx += p.x / static_cast<double>(corners.size());
        cy += p.y / static_cast<double>(corners.size());
    }
    std::sort(corners.begin(), corners.end(), [&](const Corner& p, const Corner& q) {
        return std::atan2(p.y - cy, p.x - cx) < std::atan2(q.y - cy, q.x - cx);
    });
    std::string d;
    for (size_t i = 0; i < corners.size(); i++) {
        const auto& from = corners[i];
        const auto& to = corners[(i + 1) % corners.size()];
        if (i == 0) d = "M" + num(from.x) + "," + num(from.y);
        const Circle* arc = nullptr;
        int large = 0;
        double narrowest = std::numeric_limits<double>::infinity();
        for (size_t cIdx : {from.a, from.b}) {
            if (cIdx != to.a && cIdx != to.b) continue;
            const auto& c = circles[cIdx];
            double start = std::atan2(from.y - c.y, from.x - c.x);
            double sweep = std::atan2(to.y - c.y, to.x - c.x) - start;
            if (sweep <= 0.0) sweep += VENN_TAU;
            double mx = c.x + c.r * std::cos(start + sweep / 2.0);
            double my = c.y + c.r * std::sin(start + sweep / 2.0);
            double width = std::hypot(mx - (from.x + to.x) / 2.0, my - (from.y + to.y) / 2.0);
            if (width < narrowest) {
                narrowest = width;
                arc = &c;
                large = sweep > VENN_PI ? 1 : 0;
            }
        }
        if (!arc) Pele::appendCat(d, {"L", num(to.x), ",", num(to.y)});
        else d += "A" + num(arc->r) + "," + num(arc->r) + " 0 " + std::to_string(large) + " 1 " + num(to.x) + "," + num(to.y);
    }
    return Pele::cat({d, "Z"});
}

// ============================================================================
// src/diagrams/venn/render.ts
// ============================================================================

inline Rendered renderVennModel(const VennDb& model, const Config& /*config*/, const RenderOptions& options) noexcept {
    constexpr double VENN_RADIUS = 100.0;
    constexpr double MAX_GROWTH = 1.6;
    const ResolvedStyle NO_STYLE = resolveStyle({});

    double size = options.fontSize;
    double small = std::round(size * 0.875);
    double tiny = std::round(size * 0.8125);
    const TextMeasurer& measurer = getMeasurer(options);
    double pad = options.padding;

    auto circles = layoutCircles(model.subsets);
    Pele::FlatMap<std::string, int> index;
    for (size_t i = 0; i < circles.size(); i++) index[circles[i].id] = static_cast<int>(i);

    std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> styles;
    for (const auto& st : model.styles) {
        std::string key = keyOf(st.targets);
        auto& merged = styles[key];
        for (const auto& [prop, val] : st.styles) {
            bool found = false;
            for (auto& kv : merged) {
                if (kv.first == prop) {
                    kv.second = val;
                    found = true;
                    break;
                }
            }
            if (!found) merged.push_back({prop, val});
        }
    }
    auto styleOf = [&](const std::string& key) -> ResolvedStyle {
        auto it = styles.find(key);
        if (it == styles.end()) return NO_STYLE;
        std::vector<std::string> list;
        for (const auto& [prop, val] : it->second) list.push_back(Pele::cat({prop, ":", val}));
        return resolveStyle(list);
    };

    std::vector<std::vector<int>> neighbors(circles.size());
    for (size_t a = 0; a < circles.size(); a++) {
        for (size_t b = a + 1; b < circles.size(); b++) {
            if (std::hypot(circles[a].x - circles[b].x, circles[a].y - circles[b].y) < circles[a].r + circles[b].r) {
                neighbors[a].push_back(static_cast<int>(b));
                neighbors[b].push_back(static_cast<int>(a));
            }
        }
    }

    struct RegionLine {
        std::string id;
        bool hasId = false;
        std::string raw;
        double size = 0.0;
        int base = 0;
        Label label;
    };

    struct Region {
        std::vector<std::string> sets;
        std::vector<int> inside;
        Spot spot;
        std::vector<RegionLine> lines;
        ResolvedStyle style;
        bool across = false;
    };

    std::vector<std::string> statedOrder;
    std::unordered_map<std::string, VennSubset> stated;
    for (const auto& subset : model.subsets) {
        std::string key = keyOf(subset.sets);
        if (!stated.count(key)) statedOrder.push_back(key);
        stated[key] = subset;
    }

    std::vector<std::string> regionOrder;
    std::unordered_map<std::string, Region> regions;
    for (const auto& key : statedOrder) {
        const auto& subset = stated[key];
        std::vector<std::string> sets;
        Pele::FlatSet<std::string> seenSets;
        for (const auto& s : subset.sets) {
            if (seenSets.insert(s).second) sets.push_back(s);
        }
        std::vector<int> inside;
        Pele::FlatSet<int> within;
        for (const auto& id : sets) {
            int idx = index[id];
            inside.push_back(idx);
            within.insert(idx);
        }
        int smallest = inside[0];
        for (int i : inside) if (circles[i].r < circles[smallest].r) smallest = i;
        ResolvedStyle style = styleOf(key);
        bool hasText = subset.hasLabel || subset.sets.size() == 1;
        std::string text = subset.hasLabel ? subset.label : (subset.sets.size() == 1 ? subset.sets[0] : "");
        int base = (style.bold ? Style::Bold : 0) | (style.italic ? Style::Italic : 0);
        std::vector<int> others;
        for (int nb : neighbors[smallest]) {
            if (!within.count(nb)) others.push_back(nb);
        }
        Spot spot = findSpot(circles, inside, others);
        bool across = !others.empty() && spot.margin < circles[smallest].r * 0.2;
        if (across) spot = findSpot(circles, inside, {});
        Region region{sets, inside, spot, {}, style, across};
        if (hasText && !text.empty()) {
            double fontSize = sets.size() == 1 ? size : small;
            region.lines.push_back({"", false, text, fontSize, base, layoutLabel(text, false, measurer, fontSize, 4000.0, base)});
        }
        regionOrder.push_back(key);
        regions[key] = std::move(region);
    }
    for (const auto& node : model.textNodes) {
        std::string raw = node.hasLabel ? node.label : node.id;
        auto it = regions.find(keyOf(node.sets));
        if (it != regions.end()) {
            it->second.lines.push_back({node.id, true, raw, tiny, 0, layoutLabel(raw, false, measurer, tiny, 4000.0)});
        }
    }

    double largest = 0.0;
    for (const auto& c : circles) if (c.r > largest) largest = c.r;
    double scale = largest > 0.0 ? (VENN_RADIUS / largest) : 1.0;
    double growth = 1.0;
    for (const auto& key : regionOrder) {
        const auto& region = regions[key];
        if (region.spot.margin <= 0.0) continue;
        double w = 0.0;
        double h = 0.0;
        for (const auto& line : region.lines) {
            w = std::max(w, line.label.width);
            h += line.label.height;
        }
        double need = std::hypot(w, h) / 2.0 + 4.0;
        growth = std::max(growth, std::min(need / (region.spot.margin * scale), MAX_GROWTH));
    }
    scale *= growth;

    for (const auto& key : regionOrder) {
        auto& region = regions[key];
        double room = std::max(region.spot.margin * scale * 1.8, 72.0);
        for (auto& line : region.lines) {
            if (line.label.width > room) {
                line.label = layoutLabel(line.raw, false, measurer, line.size, room, line.base);
            }
        }
    }

    double minX = std::numeric_limits<double>::infinity();
    double minY = std::numeric_limits<double>::infinity();
    double maxX = -std::numeric_limits<double>::infinity();
    double maxY = -std::numeric_limits<double>::infinity();
    for (const auto& c : circles) {
        minX = std::min(minX, (c.x - c.r) * scale);
        minY = std::min(minY, (c.y - c.r) * scale);
        maxX = std::max(maxX, (c.x + c.r) * scale);
        maxY = std::max(maxY, (c.y + c.r) * scale);
    }
    for (const auto& key : regionOrder) {
        const auto& region = regions[key];
        double wide = 0.0;
        double tall = 0.0;
        for (const auto& line : region.lines) {
            wide = std::max(wide, line.label.width);
            tall += line.label.height;
        }
        minX = std::min(minX, region.spot.x * scale - wide / 2.0);
        maxX = std::max(maxX, region.spot.x * scale + wide / 2.0);
        minY = std::min(minY, region.spot.y * scale - tall / 2.0);
        maxY = std::max(maxY, region.spot.y * scale + tall / 2.0);
    }
    if (circles.empty()) minX = minY = maxX = maxY = 0.0;

    Label title = layoutLabel(!model.title.empty() ? normalizeText(model.title) : "", false, measurer, size, 4000.0, Style::Bold);
    double titleHeight = title.height > 0.0 ? (title.height + 12.0) : 0.0;
    double inner = std::max(maxX - minX, title.width);
    double ox = pad + (inner - (maxX - minX)) / 2.0 - minX;
    double oy = pad + titleHeight - minY;
    double width = std::ceil(inner + 2.0 * pad);
    double height = std::ceil(maxY - minY + titleHeight + 2.0 * pad);
    std::vector<Circle> placed;
    placed.reserve(circles.size());
    for (const auto& c : circles) {
        placed.push_back({c.id, c.x * scale + ox, c.y * scale + oy, c.r * scale});
    }

    std::string sets;
    for (size_t i = 0; i < placed.size(); i++) {
        const auto& c = placed[i];
        std::string color = seriesColor(static_cast<int>(i));
        auto it = regions.find(c.id);
        const ResolvedStyle& style = (it != regions.end()) ? it->second.style : NO_STYLE;
        Pele::appendCat(sets, {"<circle class=\"pele-venn-set\" data-id=\"", escText(c.id), "\" cx=\"", num(c.x), "\" cy=\"", num(c.y), "\" r=\"", num(c.r), "\" fill=\"", color, "\" fill-opacity=\"0.2\" stroke=\"", color, "\"", style.shape, "/>"});
    }

    std::vector<const Region*> painted;
    for (const auto& key : regionOrder) {
        const auto& r = regions[key];
        if (r.sets.size() > 1 && !r.style.shape.empty()) painted.push_back(&r);
    }
    auto under = [&](const Region& region) -> std::string {
        if (region.across) return "";
        bool hasFillOpacity = (region.style.shape.find("\"fill-opacity:") != std::string::npos || region.style.shape.find(";fill-opacity:") != std::string::npos);
        std::string fill = (region.sets.size() == 1 && hasFillOpacity) ? region.style.shape : "";
        for (const Region* other : painted) {
            bool allIn = true;
            for (const auto& id : other->sets) {
                if (std::find(region.sets.begin(), region.sets.end(), id) == region.sets.end()) {
                    allIn = false;
                    break;
                }
            }
            if (allIn) fill = other->style.shape;
        }
        return fill;
    };

    std::string overlaps;
    std::string labels;
    for (const auto& key : regionOrder) {
        const auto& region = regions[key];
        std::string joinedSets;
        for (size_t i = 0; i < region.sets.size(); i++) {
            if (i > 0) joinedSets.push_back('|');
            joinedSets += region.sets[i];
        }
        std::string id = escText(joinedSets);
        if (region.sets.size() > 1 && !region.style.shape.empty()) {
            std::vector<Circle> insideCircles;
            for (int idx : region.inside) insideCircles.push_back(placed[idx]);
            std::string d = regionPath(insideCircles);
            if (!d.empty()) {
                Pele::appendCat(overlaps, {"<path class=\"pele-venn-overlap\" data-id=\"", id, "\" d=\"", d, "\" fill=\"none\"", region.style.shape, "/>"});
            }
        }
        double total = 0.0;
        for (const auto& line : region.lines) total += line.label.height;
        if (total == 0.0) continue;
        double x = region.spot.x * scale + ox;
        double y = region.spot.y * scale + oy - total / 2.0;
        std::string fill = under(region);
        std::string content;
        for (const auto& line : region.lines) {
            std::string own = !line.hasId ? region.style.text : styleOf(line.id).text;
            std::string color = ink(fill, own);
            std::string attrs = !line.hasId
                ? Pele::cat({" class=\"pele-label\"", (line.size == size ? std::string("") : Pele::cat({" font-size=\"", num(line.size), "\""})), (!color.empty() ? Pele::cat({" fill=\"", color, "\""}) : std::string("")), own})
                : Pele::cat({" class=\"pele-venn-text\" data-id=\"", escText(line.id), "\" font-size=\"", num(tiny), "\" fill=\"", (!color.empty() ? color : std::string("var(--_m)")), "\"", own});
            content += labelSvg(line.label, x, y + line.label.height / 2.0, attrs);
            y += line.label.height;
        }
        Pele::appendCat(labels, {"<g class=\"pele-venn-region\" data-id=\"", id, "\">", content, "</g>"});
    }

    std::string svg = svgDocument(
        "venn", 
        width, 
        height, 
        size, 
        options, 
        DiagramMeta{}, Pele::cat({labelSvg(title, width / 2.0, pad + title.height / 2.0, " class=\"pele-title\" font-weight=\"var(--_tw)\""), "<g class=\"pele-venn-sets\">", sets, "</g>", (!overlaps.empty() ? Pele::cat({"<g class=\"pele-venn-overlaps\">", overlaps, "</g>"}) : ""), (!labels.empty() ? Pele::cat({"<g class=\"pele-venn-labels\">", labels, "</g>"}) : "")}));
    return {svg, width, height, {}};
}

inline Rendered render(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options) {
    VennDb db;
    if (!title.empty()) db.title = title;
    parseVenn(source, db);
    return renderVennModel(db, config, options);
}

} // namespace VennDiagram

inline Rendered renderVenn(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options) {
    return VennDiagram::render(source, config, title, options);
}

inline Rendered renderVenn(const std::string& raw, RenderOptions options = {}) {
    auto pre = preprocess(raw);
    std::string parsed = Pele::cat({encodeEntities(pre.text), "\n"});
    const auto* section = pre.config.get("venn");
    bool fixed = (section && section->isMap() && section->get("useMaxWidth") && section->get("useMaxWidth")->isBool() && !section->get("useMaxWidth")->bVal);
    if (!options.hasResponsive) {
        options.responsive = !fixed;
        options.hasResponsive = true;
    }
    return VennDiagram::render(parsed, pre.config, pre.title, options);
}

} // namespace Pele
