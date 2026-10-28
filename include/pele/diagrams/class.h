#pragma once
#include "../foundation.h"
#include "../util.h"
#include "../text.h"
#include "../svg.h"
#include "../common_parse.h"
#include "state.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Pele::ClassDiagram {

using StateDiagram::CEdge;
using StateDiagram::CNode;
using StateDiagram::CompoundResult;
using StateDiagram::cnode;
using StateDiagram::compoundLayout;
using StateDiagram::direction;
using StateDiagram::runsAcross;
using StateDiagram::turnToFit;

inline std::string num(double value) noexcept {
    if (!std::isfinite(value)) return "0";
    long long n = static_cast<long long>(std::floor(value * 100.0 + 0.5));
    if (n == 0) return "0";
    bool neg = n < 0;
    long long m = neg ? -n : n;
    long long q = m / 100;
    long long r = m % 100;
    std::string s = (neg ? "-" : "") + std::to_string(q);
    if (r == 0) return s;
    if (r % 10 == 0) return s + "." + std::to_string(r / 10);
    if (r < 10) return s + ".0" + std::to_string(r);
    return s + "." + std::to_string(r);
}

inline CEdge cedge(int src, int dst, int minlen = 1, double labelW = 0.0, double labelH = 0.0) noexcept {
    CEdge e;
    e.src = src;
    e.dst = dst;
    e.minlen = minlen;
    e.labelW = labelW;
    e.labelH = labelH;
    return e;
}

inline void shiftLayout(std::vector<CNode>& nodes, std::vector<CEdge>& edges, double dx, double dy) noexcept {
    for (auto& c : nodes) {
        c.x += dx;
        c.y += dy;
    }
    for (auto& e : edges) {
        for (size_t i = 0; i + 1 < e.route.size(); i += 3) {
            e.route[i] += dx;
            e.route[i + 1] += dy;
        }
        e.labelX += dx;
        e.labelY += dy;
    }
}

// ============================================================================
// src/diagrams/common/generics.ts
// ============================================================================

namespace detail_generics {

inline int tildes(const std::string& text) noexcept {
    int count = 0;
    for (size_t i = text.find('~'); i != std::string::npos; i = text.find('~', i + 1)) count++;
    return count;
}

inline std::string pairTildes(const std::string& text) noexcept {
    int count = tildes(text);
    if (count <= 1) return text;
    bool lead = (count % 2 != 0) && !text.empty() && text[0] == '~';
    std::string chars = text;
    std::vector<size_t> at;
    for (size_t i = lead ? 1 : 0; i < chars.size(); i++) {
        if (chars[i] == '~') at.push_back(i);
    }
    for (int lo = 0, hi = static_cast<int>(at.size()) - 1; lo < hi; lo++, hi--) {
        chars[at[lo]] = '<';
        chars[at[hi]] = '>';
    }
    return chars;
}

} // namespace detail_generics

inline std::string parseGenericTypes(const std::string& input) {
    if (input.find('~') == std::string::npos) return input;
    // input.split(/(,)/)
    std::vector<std::string> sets;
    size_t start = 0;
    for (size_t p = input.find(','); p != std::string::npos; p = input.find(',', start)) {
        sets.push_back(input.substr(start, p - start));
        sets.push_back(",");
        start = p + 1;
    }
    sets.push_back(input.substr(start));

    std::vector<std::string> output;
    for (size_t i = 0; i < sets.size(); i++) {
        std::string set = sets[i];
        if (set == "," && i > 0 && i + 1 < sets.size() &&
            detail_generics::tildes(sets[i - 1]) == 1 &&
            detail_generics::tildes(sets[i + 1]) == 1) {
            set = Pele::cat({sets[i - 1], ",", sets[i + 1]});
            i++;
            output.pop_back();
        }
        output.push_back(detail_generics::pairTildes(set));
    }
    std::string res;
    for (const auto& s : output) res += s;
    return res;
}

// ============================================================================
// src/layout/components.ts
// ============================================================================

inline bool wrapComponents(
    std::vector<CNode>& nodes,
    std::vector<CEdge>& edges,
    double width,
    double gap,
    CompoundResult& outResult
) {
    int n = static_cast<int>(nodes.size());
    std::vector<int> parent(n);
    for (int i = 0; i < n; i++) parent[i] = i;
    auto root = [&](int i) -> int {
        while (parent[i] != i) {
            parent[i] = parent[parent[i]];
            i = parent[i];
        }
        return i;
    };
    auto join = [&](int a, int b) {
        parent[root(a)] = root(b);
    };
    for (int i = 0; i < n; i++) {
        if (nodes[i].parent >= 0) join(i, nodes[i].parent);
    }
    for (const auto& e : edges) join(e.src, e.dst);

    struct Part {
        std::vector<int> nodeIndices;
        std::vector<int> edgeIndices;
        double left = std::numeric_limits<double>::infinity();
        double top = std::numeric_limits<double>::infinity();
        double right = -std::numeric_limits<double>::infinity();
        double bottom = -std::numeric_limits<double>::infinity();
    };
    std::vector<int> partOrder;
    std::unordered_map<int, Part> parts;
    auto include = [](Part& p, double x, double y, double w = 0.0, double h = 0.0) {
        p.left = std::min(p.left, x - w / 2.0);
        p.top = std::min(p.top, y - h / 2.0);
        p.right = std::max(p.right, x + w / 2.0);
        p.bottom = std::max(p.bottom, y + h / 2.0);
    };
    for (int i = 0; i < n; i++) {
        int id = root(i);
        auto it = parts.find(id);
        if (it == parts.end()) {
            partOrder.push_back(id);
            it = parts.emplace(id, Part{}).first;
        }
        const auto& c = nodes[i];
        it->second.nodeIndices.push_back(i);
        include(it->second, c.x, c.y, c.w, c.h);
    }
    if (partOrder.size() < 2) return false;
    for (int ei = 0; ei < static_cast<int>(edges.size()); ei++) {
        const auto& e = edges[ei];
        auto& part = parts[root(e.src)];
        part.edgeIndices.push_back(ei);
        for (size_t i = 0; i + 1 < e.route.size(); i += 3) {
            include(part, e.route[i], e.route[i + 1]);
        }
        if (e.labelW > 0.0) include(part, e.labelX, e.labelY, e.labelW, e.labelH);
    }

    double x = 0.0, y = 0.0, rowHeight = 0.0, right = 0.0;
    double from = std::numeric_limits<double>::infinity();
    double to = -std::numeric_limits<double>::infinity();
    std::vector<double> shifts;
    for (int id : partOrder) {
        const auto& part = parts[id];
        double w = part.right - part.left;
        double h = part.bottom - part.top;
        if (x > 0.0 && x + w > width) {
            y += rowHeight + gap;
            x = 0.0;
            rowHeight = 0.0;
        }
        shifts.push_back(x - part.left);
        shifts.push_back(y - part.top);
        from = std::min(from, part.left);
        to = std::max(to, part.right);
        right = std::max(right, x + w);
        rowHeight = std::max(rowHeight, h);
        x += w + gap;
    }
    if (right >= to - from) return false;
    size_t k = 0;
    for (int id : partOrder) {
        const auto& part = parts[id];
        double dx = shifts[k++];
        double dy = shifts[k++];
        for (int ni : part.nodeIndices) {
            nodes[ni].x += dx;
            nodes[ni].y += dy;
        }
        for (int ei : part.edgeIndices) {
            auto& e = edges[ei];
            for (size_t i = 0; i + 1 < e.route.size(); i += 3) {
                e.route[i] += dx;
                e.route[i + 1] += dy;
            }
            if (e.labelW > 0.0) {
                e.labelX += dx;
                e.labelY += dy;
            }
        }
    }
    outResult = {right, y + rowHeight};
    return true;
}

// ============================================================================
// src/svg/cluster.ts & endSide & tighten
// ============================================================================

using Crossings = std::unordered_map<int, std::vector<double>>;

inline void markCrossings(Crossings& crossings, const std::vector<double>& route) noexcept {
    for (size_t k = 0; k + 1 < route.size(); k += 3) {
        int key = static_cast<int>(std::llround(route[k + 1]));
        crossings[key].push_back(route[k]);
    }
}

inline double clusterTitleX(const CNode& c, double width, const Crossings& crossings, double pad) noexcept {
    if (width == 0.0) return c.x;
    double top = c.y - c.h / 2.0;
    double left = c.x - c.w / 2.0;
    double half = width / 2.0;
    double candidates[3] = {left + pad - 6.0 + half, c.x, left + c.w - pad + 6.0 - half};
    auto it = crossings.find(static_cast<int>(std::llround(top)));
    if (it == crossings.end()) return candidates[0];
    const auto& xs = it->second;
    double best = candidates[0];
    double bestGap = -1.0;
    for (double x : candidates) {
        double gap = std::numeric_limits<double>::infinity();
        for (double cx : xs) {
            if (cx > left && cx < left + c.w) gap = std::min(gap, std::abs(cx - x) - half);
        }
        if (gap >= 6.0) return x;
        if (gap > bestGap + 0.5) {
            bestGap = gap;
            best = x;
        }
    }
    return best;
}

inline bool titleCrossed(const CNode& c, double width, double x, const Crossings& crossings) noexcept {
    if (width <= 0.0) return false;
    auto it = crossings.find(static_cast<int>(std::llround(c.y - c.h / 2.0)));
    if (it == crossings.end()) return false;
    double left = c.x - c.w / 2.0;
    for (double cx : it->second) {
        if (cx > left && cx < left + c.w && std::abs(cx - x) < width / 2.0 + 4.0) return true;
    }
    return false;
}

template <typename NodeVec>
inline std::string struckTitle(
    const Label& label,
    double x,
    double y,
    const std::string& attrs,
    const std::string& id,
    const NodeVec& nodes,
    size_t index,
    const IconResolver& icons = nullptr
) {
    int depth = 1;
    for (int p = nodes[index].parent; p >= 0 && depth < 8; p = nodes[p].parent) depth++;
    std::string fill = Pele::cat({"color-mix(in srgb,var(--_a) ", num(100.0 * (1.0 - std::pow(0.5, depth))), "%,var(--_bg))"});
    return Pele::cat({"<g class=\"pele-cluster-title\" data-id=\"", id, "\">", labelSvg(label,  x,  y, Pele::cat({" fill=\"none\" stroke=\"", fill, "\" stroke-width=\"5\" stroke-linejoin=\"round\""}),  icons), labelSvg(label, x, y, attrs, icons), "</g>"});
}

inline int endSide(const std::vector<double>& route, size_t at, const CNode& c) noexcept {
    double dx = route[at] - c.x;
    double dy = route[at + 1] - c.y;
    if (std::abs(std::abs(dy) - c.h / 2.0) < 0.5) return dy < 0.0 ? 0 : 2;
    if (std::abs(std::abs(dx) - c.w / 2.0) < 0.5) return dx < 0.0 ? 3 : 1;
    return -1;
}

// ============================================================================
// src/diagrams/class/members.ts
// ============================================================================

inline bool isVisibilityChar(char ch) noexcept {
    return ch == '#' || ch == '+' || ch == '~' || ch == '-';
}

struct ClassMember {
    std::string id;
    std::string memberType; // "method" or "attribute"
    std::string visibility; // "#", "+", "~", "-", or ""
    std::string classifier; // "$", "*", or ""
    std::string parameters;
    std::string returnType;

    ClassMember() = default;
    ClassMember(const std::string& input, const std::string& mType) : memberType(mType) {
        if (mType != "method" || !parseMethod(input)) {
            parseAttribute(input);
        }
        bool leadSpace = !id.empty() && id[0] == ' ';
        std::string trimmed = trimStr(id);
        id = leadSpace ? Pele::cat({" ", trimmed}) : trimmed;
    }

    bool parseMethod(const std::string& input) {
        size_t n = input.size();
        for (size_t start = 0; start < n;) {
            size_t end = start;
            while (end < n) {
                size_t next = end;
                int cp = charCodeAtUtf8(input, end, &next);
                if (isLineEnd(cp)) break;
                end = next;
            }
            int close = static_cast<int>(end) - 1;
            while (close > static_cast<int>(start) && static_cast<unsigned char>(input[close]) != 41) close--;
            int open = close - 1;
            while (open > static_cast<int>(start) && static_cast<unsigned char>(input[open]) != 40) open--;
            if (open > static_cast<int>(start)) {
                char first = input[start];
                bool visible = isVisibilityChar(first) && open > static_cast<int>(start) + 1;
                if (visible) visibility = std::string(1, first);
                size_t idStart = visible ? start + 1 : start;
                id = input.substr(idStart, static_cast<size_t>(open) - idStart);
                parameters = trimStr(input.substr(static_cast<size_t>(open) + 1, static_cast<size_t>(close - open - 1)));
                size_t rest = static_cast<size_t>(close) + 1;
                std::string clf;
                if (rest < n) {
                    size_t nextPos = rest;
                    int nextCp = charCodeAtUtf8(input, rest, &nextPos);
                    std::string nextStr = input.substr(rest, nextPos - rest);
                    if (nextStr == "$" || nextStr == "*" || isSpace(nextCp)) {
                        clf = trimStr(nextStr);
                        rest = nextPos;
                    }
                }
                size_t stop = rest;
                while (stop < n) {
                    size_t next = stop;
                    int cp = charCodeAtUtf8(input, stop, &next);
                    if (isLineEnd(cp)) break;
                    stop = next;
                }
                returnType = trimStr(input.substr(rest, stop - rest));
                if (clf.empty() && !returnType.empty()) {
                    char last = returnType.back();
                    if (last == '$' || last == '*') {
                        clf = std::string(1, last);
                        returnType.pop_back();
                    }
                }
                classifier = clf;
                return true;
            }
            if (end < n) {
                size_t next = end;
                charCodeAtUtf8(input, end, &next);
                start = next;
            } else {
                start = n;
            }
        }
        memberType = "attribute";
        return false;
    }

    void parseAttribute(const std::string& input) {
        if (!input.empty() && isVisibilityChar(input.front())) {
            visibility = std::string(1, input.front());
        }
        if (!input.empty() && (input.back() == '$' || input.back() == '*')) {
            classifier = std::string(1, input.back());
        }
        size_t s = visibility.empty() ? 0 : 1;
        size_t e = classifier.empty() ? input.size() : (input.size() > 0 ? input.size() - 1 : 0);
        id = (e > s) ? input.substr(s, e - s) : "";
    }

    std::pair<std::string, std::string> getDisplayDetails() const {
        std::string displayText = visibility + parseGenericTypes(id);
        if (memberType == "method") {
            Pele::appendCat(displayText, {"(", parseGenericTypes(trimStr(parameters)), ")"});
            if (!returnType.empty()) Pele::appendCat(displayText, {" : ", parseGenericTypes(returnType)});
        }
        return {trimStr(displayText), parseClassifier()};
    }

    std::string parseClassifier() const {
        return classifier == "*" ? "font-style:italic;" : classifier == "$" ? "text-decoration:underline;" : "";
    }
};

// ============================================================================
// src/diagrams/class/types.ts & src/diagrams/class/db.ts
// ============================================================================

struct RelationSpec {
    int type1 = -1; // -1 = 'none', 0..4 = RELATION_TYPE
    int type2 = -1; // -1 = 'none', 0..4 = RELATION_TYPE
    int lineType = 0; // 0 = LINE, 1 = DOTTED_LINE
};

struct ClassNode {
    std::string id;
    std::string type;
    std::string label;
    std::string cssClasses = "default";
    std::vector<ClassMember> methods;
    std::vector<ClassMember> members;
    std::vector<std::string> annotations;
    std::vector<std::string> styles;
    std::string parent;
    bool hasParent = false;
    std::string link;
    bool hasLink = false;
    std::string linkTarget;
    bool hasLinkTarget = false;
    bool haveCallback = false;
    std::string tooltip;
    bool hasTooltip = false;
};
using ClassNodePtr = std::shared_ptr<ClassNode>;

struct ClassRelation {
    std::string id1;
    std::string id2;
    std::string relationTitle1;
    std::string relationTitle2;
    std::string title;
    bool hasTitle = false;
    RelationSpec relation;
};

struct ClassNote {
    std::string id;
    std::string className;
    bool hasClass = false;
    std::string text;
    int index = 0;
    std::string parent;
    bool hasParent = false;
};
using ClassNotePtr = std::shared_ptr<ClassNote>;

struct ClassInterface {
    std::string id;
    std::string label;
    std::string classId;
};

struct NamespaceNode {
    std::string id;
    std::string label;
    std::unordered_map<std::string, ClassNodePtr> classes;
    std::unordered_map<std::string, ClassNotePtr> notes;
    std::unordered_map<std::string, std::shared_ptr<NamespaceNode>> children;
    std::string parent;
    bool hasParent = false;
    bool explicitNs = false;
};
using NamespaceNodePtr = std::shared_ptr<NamespaceNode>;

struct StyleClass {
    std::string id;
    std::vector<std::string> styles;
    std::vector<std::string> textStyles;
};

namespace detail_db {

constexpr int LINE = 0;
constexpr int DOTTED_LINE = 1;

constexpr int AGGREGATION = 0;
constexpr int EXTENSION = 1;
constexpr int COMPOSITION = 2;
constexpr int DEPENDENCY = 3;
constexpr int LOLLIPOP = 4;

inline std::pair<std::string, std::string> splitName(const std::string& id) noexcept {
    size_t at = id.find('~');
    if (at == std::string::npos || at == 0) return {id, ""};
    size_t end = id.find('~', at + 1);
    return {
        id.substr(0, at),
        id.substr(at + 1, end == std::string::npos ? std::string::npos : (end - at - 1))
    };
}

inline std::string lookupId(const std::string& id) noexcept {
    int c = charCodeAt(id, 0);
    std::string s = (c >= 48 && c <= 57) ? Pele::cat({"classId-", id}) : id;
    return splitName(s).first;
}

} // namespace detail_db

class ClassDb : public DiagramMeta {
public:
    std::string direction = "TB";
    std::vector<ClassNodePtr> classesList;
    std::unordered_map<std::string, ClassNodePtr> classes;
    std::vector<ClassRelation> relations;
    std::vector<ClassNotePtr> notesList;
    std::unordered_map<std::string, ClassNotePtr> notes;
    std::vector<NamespaceNodePtr> namespacesList;
    std::unordered_map<std::string, NamespaceNodePtr> namespaces;
    std::vector<ClassInterface> interfaces;
    std::unordered_map<std::string, StyleClass> styleClasses;

private:
    std::vector<std::string> namespaceStack;
    std::unordered_map<std::string, std::vector<ClassNodePtr>> withCssClass;

    void tag(const ClassNodePtr& node, const std::string& cssClass) {
        auto& vec = withCssClass[cssClass];
        for (const auto& existing : vec) {
            if (existing == node) return;
        }
        vec.push_back(node);
    }

    ClassNodePtr ensure(const std::string& id) {
        addClass(id);
        return classes.at(detail_db::splitName(id).first);
    }

    std::string addInterface(const std::string& label, const std::string& classId) {
        std::string id = "interface" + std::to_string(interfaces.size());
        interfaces.push_back({id, label, classId});
        return id;
    }

public:
    void addClass(const std::string& id) {
        auto [className, type] = detail_db::splitName(id);
        if (classes.count(className)) return;
        auto node = std::make_shared<ClassNode>();
        node->id = className;
        node->type = type;
        node->label = className;
        node->cssClasses = "default";
        classes[className] = node;
        classesList.push_back(node);
        tag(node, "default");
    }

    void setClassLabel(const std::string& id, const std::string& label) {
        ensure(id)->label = label;
    }

    void addRelation(ClassRelation relation) {
        int type1 = relation.relation.type1;
        int type2 = relation.relation.type2;
        if (type1 == detail_db::LOLLIPOP && type2 == -1) {
            addClass(relation.id2);
            relation.id1 = addInterface(relation.id1, relation.id2);
        } else if (type2 == detail_db::LOLLIPOP && type1 == -1) {
            addClass(relation.id1);
            relation.id2 = addInterface(relation.id2, relation.id1);
        } else {
            addClass(relation.id1);
            addClass(relation.id2);
        }
        relation.id1 = detail_db::splitName(relation.id1).first;
        relation.id2 = detail_db::splitName(relation.id2).first;
        relation.relationTitle1 = trimStr(relation.relationTitle1);
        relation.relationTitle2 = trimStr(relation.relationTitle2);
        relations.push_back(std::move(relation));
    }

    void addAnnotation(const std::string& className, const std::string& annotation) {
        ensure(className)->annotations.push_back(annotation);
    }

    void addMember(const std::string& className, const std::string& member) {
        auto node = ensure(className);
        std::string text = trimStr(member);
        if (startsWith(text, "<<") && endsWith(text, ">>") && text.size() >= 4) {
            node->annotations.push_back(text.substr(2, text.size() - 4));
        } else if (text.find(')') != std::string::npos && text.find(')') > 0) {
            ClassMember parsed(text, "method");
            if (parsed.memberType == "method") node->methods.push_back(std::move(parsed));
            else node->members.push_back(std::move(parsed));
        } else if (!text.empty()) {
            node->members.push_back(ClassMember(text, "attribute"));
        }
    }

    void addMembers(const std::string& className, std::vector<std::string> members) {
        std::reverse(members.begin(), members.end());
        for (const auto& member : members) addMember(className, member);
    }

    std::string addNote(const std::string& text, const std::string& className = "", bool hasClass = false) {
        int index = static_cast<int>(notesList.size());
        std::string id = "note" + std::to_string(index);
        auto note = std::make_shared<ClassNote>();
        note->id = id;
        note->className = className;
        note->hasClass = hasClass;
        note->text = text;
        note->index = index;
        notes[id] = note;
        notesList.push_back(note);
        return id;
    }

    std::string cleanupLabel(const std::string& label) const {
        return trimStr(startsWith(label, ":") ? label.substr(1) : label);
    }

    void setCssClass(const std::string& ids, const std::string& className) {
        for (const auto& id : splitStr(ids, ',')) {
            auto it = classes.find(detail_db::lookupId(id));
            if (it == classes.end()) continue;
            Pele::appendCat(it->second->cssClasses, {" ", className});
            for (const auto& word : splitStr(className, ' ')) tag(it->second, word);
        }
    }

    void defineClass(const std::vector<std::string>& ids, const std::vector<std::string>& style) {
        for (const auto& id : ids) {
            auto& styleClass = styleClasses[id];
            styleClass.id = id;
            for (const auto& s : style) {
                if (s.find("color") != std::string::npos) {
                    std::string r = s;
                    size_t pos = r.find("fill");
                    if (pos != std::string::npos) r.replace(pos, 4, "bgFill");
                    styleClass.textStyles.push_back(r);
                }
                styleClass.styles.push_back(s);
            }
            auto itTagged = withCssClass.find(id);
            if (itTagged == withCssClass.end()) continue;
            std::vector<std::string> parts;
            for (const auto& s : style) {
                for (const auto& p : splitStr(s, ',')) parts.push_back(p);
            }
            for (const auto& node : itTagged->second) {
                for (const auto& part : parts) node->styles.push_back(part);
            }
        }
    }

    void setTooltip(const std::string& ids, const std::string& tooltip) {
        for (const auto& id : splitStr(ids, ',')) {
            auto it = classes.find(detail_db::splitName(id).first);
            if (it != classes.end()) {
                it->second->tooltip = tooltip;
                it->second->hasTooltip = true;
            }
        }
    }

    void setLink(const std::string& ids, const std::string& link, const std::string& target = "", bool hasTarget = false) {
        std::string url = trimStr(decodeEntities(link));
        for (const auto& id : splitStr(ids, ',')) {
            auto it = classes.find(detail_db::lookupId(id));
            if (it != classes.end()) {
                if (!url.empty()) {
                    std::string s = sanitizeUrl(url);
                    std::string low = toLowerAscii(s);
                    if ((startsWith(low, "http://") || startsWith(low, "https://")) && s.find(':', 6) == std::string::npos) {
                        size_t hostStart = startsWith(low, "https://") ? 8 : 7;
                        size_t pathPos = s.find_first_of("/?#", hostStart);
                        if (pathPos == std::string::npos) {
                            s = Pele::cat({toLowerAscii(s), "/"});
                        } else {
                            std::string prefix = toLowerAscii(s.substr(0, pathPos));
                            std::string rest = s.substr(pathPos);
                            if (rest[0] != '/') rest = "/" + rest;
                            s = prefix + rest;
                        }
                    }
                    it->second->link = s;
                    it->second->hasLink = true;
                } else {
                    it->second->link.clear();
                    it->second->hasLink = false;
                }
                it->second->linkTarget = hasTarget ? target : "_blank";
                it->second->hasLinkTarget = true;
            }
        }
        setCssClass(ids, "clickable");
    }

    void setClickEvent(const std::string& ids, const std::string& = "", const std::string& = "") {
        for (const auto& id : splitStr(ids, ',')) {
            auto it = classes.find(detail_db::splitName(id).first);
            if (it != classes.end()) it->second->haveCallback = true;
        }
        setCssClass(ids, "clickable");
    }

    void setDirection(const std::string& dir) {
        direction = dir;
    }

    void setAccTitle(const std::string& text) {
        accTitle = trimStartStr(text);
    }

    void setAccDescription(const std::string& text) {
        std::string out;
        out.reserve(text.size());
        for (size_t i = 0; i < text.size();) {
            if (text[i] == '\n') {
                out.push_back('\n');
                i++;
                while (i < text.size()) {
                    size_t next = i;
                    int cp = charCodeAtUtf8(text, i, &next);
                    if (!isSpace(cp)) break;
                    i = next;
                }
            } else {
                out.push_back(text[i++]);
            }
        }
        accDescr = std::move(out);
    }

    std::string addNamespace(const std::string& id, const std::string& label = "", bool hasLabel = false) {
        std::string prefix = namespaceStack.empty() ? "" : namespaceStack.back();
        std::string qualified = !prefix.empty() ? Pele::cat({prefix, ".", id}) : id;
        namespaceStack.push_back(qualified);

        auto itExisting = namespaces.find(qualified);
        if (itExisting != namespaces.end()) {
            itExisting->second->explicitNs = true;
            if (hasLabel && !label.empty()) itExisting->second->label = label;
            return qualified;
        }

        std::string parentId = prefix;
        bool hasParentId = !prefix.empty();
        size_t from = !prefix.empty() ? prefix.size() + 1 : 0;
        for (;;) {
            size_t dot = qualified.find('.', from);
            bool leaf = (dot == std::string::npos);
            std::string current = leaf ? qualified : qualified.substr(0, dot);
            NamespaceNodePtr node;
            auto it = namespaces.find(current);
            if (it == namespaces.end()) {
                std::string part = qualified.substr(from, (leaf ? qualified.size() : dot) - from);
                node = std::make_shared<NamespaceNode>();
                node->id = current;
                node->label = (leaf && hasLabel && !label.empty()) ? label : part;
                if (hasParentId) {
                    node->parent = parentId;
                    node->hasParent = true;
                }
                node->explicitNs = leaf;
                namespaces[current] = node;
                namespacesList.push_back(node);
            } else {
                node = it->second;
                if (leaf) node->explicitNs = true;
            }
            if (hasParentId) {
                auto itParent = namespaces.find(parentId);
                if (itParent != namespaces.end()) {
                    if (!itParent->second->children.count(current)) {
                        itParent->second->children[current] = node;
                    }
                    if (!node->hasParent) {
                        node->parent = parentId;
                        node->hasParent = true;
                    }
                }
            }
            if (leaf) return qualified;
            parentId = current;
            hasParentId = true;
            from = dot + 1;
        }
    }

    void popNamespace() {
        if (!namespaceStack.empty()) namespaceStack.pop_back();
    }

    void addClassesToNamespace(
        const std::string& id,
        const std::vector<std::string>& classNames,
        const std::vector<std::string>& noteNames
    ) {
        auto itNs = namespaces.find(id);
        if (itNs == namespaces.end()) return;
        auto& ns = itNs->second;
        for (const auto& name : classNames) {
            std::string className = detail_db::splitName(name).first;
            auto it = classes.find(className);
            if (it == classes.end()) continue;
            it->second->parent = id;
            it->second->hasParent = true;
            ns->classes[className] = it->second;
        }
        for (const auto& name : noteNames) {
            auto it = notes.find(name);
            if (it == notes.end()) continue;
            it->second->parent = id;
            it->second->hasParent = true;
            ns->notes[name] = it->second;
        }
    }

    void setCssStyle(const std::string& id, const std::vector<std::string>& styles) {
        auto it = classes.find(id);
        if (it == classes.end()) return;
        for (const auto& s : styles) {
            for (const auto& part : splitStr(s, ',')) it->second->styles.push_back(part);
        }
    }
};

// ============================================================================
// src/diagrams/class/graph.ts & src/diagrams/class/markers.ts
// ============================================================================

struct GraphNode {
    std::string id;
    std::string label;
    bool isGroup = false;
    std::string shape; // "rect", "classBox", "note", "interface"
    std::string parentId;
    bool hasParentId = false;
    int parent = -1;
    ClassNodePtr classNode;
    int colorIndex = -1;
    bool hasColorIndex = false;
};

struct GraphEdge {
    std::string id;
    std::string start;
    std::string end;
    int source = -1;
    int target = -1;
    std::string label;
    bool hasLabel = false;
    std::string arrowTypeStart; // "aggregation", "extension", "composition", "dependency", "lollipop", "none"
    std::string arrowTypeEnd;
    std::string startLabelRight;
    std::string endLabelLeft;
    std::string pattern; // "solid", "dashed", "dotted"
    std::string classes;
};

struct ClassGraph {
    std::vector<GraphNode> nodes;
    std::vector<GraphEdge> edges;
};

inline std::string relationEndMarker(int type) noexcept {
    static const char* MARKERS[5] = {"aggregation", "extension", "composition", "dependency", "lollipop"};
    if (type >= 0 && type < 5) return MARKERS[type];
    return "none";
}

inline ClassGraph buildClassGraph(const ClassDb& model, bool hierarchical = true) noexcept {
    std::vector<GraphNode> nodes;
    std::vector<GraphEdge> edges;
    Pele::FlatMap<std::string, int> namespaceAt;
    Pele::FlatMap<std::string, int> classAt;
    Pele::FlatMap<std::string, int> interfaceAt;

    std::unordered_map<std::string, std::pair<bool, std::string>> declared;
    auto declaredAncestor = [&](const std::string& id, bool hasId) -> std::pair<bool, std::string> {
        if (hierarchical || !hasId) return {hasId, id};
        std::vector<std::string> chain;
        std::string current = id;
        bool hasCurrent = true;
        std::pair<bool, std::string> found{false, ""};
        while (hasCurrent && !current.empty()) {
            auto itDec = declared.find(current);
            if (itDec != declared.end()) {
                found = itDec->second;
                break;
            }
            auto itNs = model.namespaces.find(current);
            if (itNs == model.namespaces.end()) break;
            if (itNs->second->explicitNs) {
                found = {true, current};
                break;
            }
            chain.push_back(current);
            hasCurrent = itNs->second->hasParent;
            current = itNs->second->parent;
        }
        for (const auto& visited : chain) declared[visited] = found;
        return found;
    };
    auto group = [&](const std::string& id, bool hasId) -> int {
        if (!hasId) return -1;
        auto it = namespaceAt.find(id);
        return it == namespaceAt.end() ? -1 : it->second;
    };

    for (const auto& ns : model.namespacesList) {
        if (!hierarchical && !ns->explicitNs) continue;
        namespaceAt[ns->id] = static_cast<int>(nodes.size());
        GraphNode gn;
        gn.id = ns->id;
        gn.label = hierarchical ? ns->label : ns->id;
        gn.isGroup = true;
        gn.shape = "rect";
        if (hierarchical && ns->hasParent) {
            gn.parentId = ns->parent;
            gn.hasParentId = true;
        }
        gn.parent = -1;
        nodes.push_back(std::move(gn));
    }
    for (auto& node : nodes) node.parent = group(node.parentId, node.hasParentId);

    int colorIndex = 0;
    for (const auto& classNode : model.classesList) {
        auto [hasParentId, parentId] = declaredAncestor(classNode->parent, classNode->hasParent);
        classAt[classNode->id] = static_cast<int>(nodes.size());
        GraphNode gn;
        gn.id = classNode->id;
        gn.label = classNode->label;
        gn.isGroup = false;
        gn.shape = "classBox";
        gn.parentId = parentId;
        gn.hasParentId = hasParentId;
        gn.parent = group(parentId, hasParentId);
        gn.classNode = classNode;
        gn.colorIndex = colorIndex++;
        gn.hasColorIndex = true;
        nodes.push_back(std::move(gn));
    }

    for (const auto& note : model.notesList) {
        auto [hasParentId, parentId] = declaredAncestor(note->parent, note->hasParent);
        int target = -1;
        if (note->hasClass) {
            auto it = classAt.find(note->className);
            if (it != classAt.end()) target = it->second;
        }
        if (target >= 0) {
            GraphEdge ge;
            ge.id = "edgeNote" + std::to_string(note->index);
            ge.start = note->id;
            ge.end = nodes[target].id;
            ge.source = static_cast<int>(nodes.size());
            ge.target = target;
            ge.hasLabel = false;
            ge.arrowTypeStart = "none";
            ge.arrowTypeEnd = "none";
            ge.pattern = "dotted";
            ge.classes = "relation";
            edges.push_back(std::move(ge));
        }
        GraphNode gn;
        gn.id = note->id;
        gn.label = note->text;
        gn.isGroup = false;
        gn.shape = "note";
        gn.parentId = parentId;
        gn.hasParentId = hasParentId;
        gn.parent = group(parentId, hasParentId);
        nodes.push_back(std::move(gn));
    }

    for (const auto& item : model.interfaces) {
        auto itOwner = classAt.find(item.classId);
        interfaceAt[item.id] = static_cast<int>(nodes.size());
        GraphNode gn;
        gn.id = item.id;
        gn.label = item.label;
        gn.isGroup = false;
        gn.shape = "interface";
        gn.hasParentId = false;
        gn.parent = (itOwner == classAt.end()) ? -1 : nodes[itOwner->second].parent;
        nodes.push_back(std::move(gn));
    }

    int count = 0;
    for (const auto& relation : model.relations) {
        count++;
        int type1 = relation.relation.type1;
        int type2 = relation.relation.type2;
        const auto& startAt = (type1 == detail_db::LOLLIPOP && type2 == -1) ? interfaceAt : classAt;
        const auto& endAt = (&startAt == &classAt && type2 == detail_db::LOLLIPOP && type1 == -1) ? interfaceAt : classAt;
        auto itS = startAt.find(relation.id1);
        auto itT = endAt.find(relation.id2);
        GraphEdge ge;
        ge.id = "id_" + relation.id1 + "_" + relation.id2 + "_" + std::to_string(count);
        ge.start = relation.id1;
        ge.end = relation.id2;
        ge.source = (itS == startAt.end()) ? -1 : itS->second;
        ge.target = (itT == endAt.end()) ? -1 : itT->second;
        ge.label = relation.title;
        ge.hasLabel = relation.hasTitle;
        ge.arrowTypeStart = relationEndMarker(type1);
        ge.arrowTypeEnd = relationEndMarker(type2);
        ge.startLabelRight = (relation.relationTitle1 == "none") ? "" : relation.relationTitle1;
        ge.endLabelLeft = (relation.relationTitle2 == "none") ? "" : relation.relationTitle2;
        ge.pattern = (relation.relation.lineType == 1) ? "dashed" : "solid";
        ge.classes = "relation";
        edges.push_back(std::move(ge));
    }

    return {std::move(nodes), std::move(edges)};
}

inline double classMarkerTrim(const std::string& type) noexcept {
    if (type == "extension") return 12.0;
    if (type == "composition" || type == "aggregation") return 16.0;
    if (type == "lollipop") return 10.0;
    if (type == "dependency") return markerTrim("arrow_point");
    return 0.0;
}

inline std::string classMarker(
    const std::string& type,
    double x,
    double y,
    double dx,
    double dy,
    const std::string& color
) {
    constexpr double TRIANGLE = 12.0;
    constexpr double DIAMOND = 16.0;
    constexpr double HALF = 6.0;
    constexpr double RING = 5.0;
    double nx = -dy;
    double ny = dx;
    if (type == "extension") {
        double bx = x - dx * TRIANGLE;
        double by = y - dy * TRIANGLE;
        return Pele::cat({"<path class=\"pele-marker\" d=\"M", num(x), ",", num(y), "L", num(bx + nx * HALF), ",", num(by + ny * HALF), "L", num(bx - nx * HALF), ",", num(by - ny * HALF), "Z\" stroke-linejoin=\"round\"/>"});
    }
    if (type == "composition" || type == "aggregation") {
        double mx = x - (dx * DIAMOND) / 2.0;
        double my = y - (dy * DIAMOND) / 2.0;
        double half = HALF - 1.0;
        return Pele::cat({"<path class=\"pele-marker\" d=\"M", num(x), ",", num(y), "L", num(mx + nx * half), ",", num(my + ny * half), "L", num(x - dx * DIAMOND), ",", num(y - dy * DIAMOND), "L", num(mx - nx * half), ",", num(my - ny * half), "Z\" stroke-linejoin=\"round\"", (type == "composition" ? Pele::cat({" fill=\"", color, "\""}) : ""), "/>"});
    }
    if (type == "lollipop") {
        return Pele::cat({"<circle class=\"pele-marker\" cx=\"", num(x - dx * RING), "\" cy=\"", num(y - dy * RING), "\" r=\"5\"/>"});
    }
    if (type == "dependency") {
        return marker("arrow_point", x, y, dx, dy, color);
    }
    return "";
}

// ============================================================================
// src/diagrams/class/tokens.ts, lexer.ts & parser.ts
// ============================================================================

namespace detail_parse {

enum T : int {
    END = 0,
    ERROR = 1,
    CLASS_DIAGRAM = 2,
    NEWLINE = 3,
    EOF_TOK = 4,
    SQS = 5,
    STR = 6,
    SQE = 7,
    DOT = 8,
    GENERICTYPE = 9,
    LABEL = 10,
    acc_title = 11,
    acc_title_value = 12,
    acc_descr = 13,
    acc_descr_value = 14,
    acc_descr_multiline_value = 15,
    STRUCT_START = 16,
    STRUCT_STOP = 17,
    NAMESPACE = 18,
    STYLE_SEPARATOR = 19,
    ANNOTATION_START = 20,
    ANNOTATION_END = 21,
    CLASS = 22,
    MEMBER = 23,
    NOTE_FOR = 24,
    NOTE = 25,
    CLASSDEF = 26,
    ALPHA = 27,
    COMMA = 28,
    direction_tb = 29,
    direction_bt = 30,
    direction_rl = 31,
    direction_lr = 32,
    AGGREGATION = 33,
    EXTENSION = 34,
    COMPOSITION = 35,
    DEPENDENCY = 36,
    LOLLIPOP = 37,
    LINE = 38,
    DOTTED_LINE = 39,
    CALLBACK = 40,
    LINK = 41,
    LINK_TARGET = 42,
    CLICK = 43,
    CALLBACK_NAME = 44,
    CALLBACK_ARGS = 45,
    HREF = 46,
    STYLE = 47,
    CSSCLASS = 48,
    COLON = 49,
    BRKT = 50,
    PCT = 51,
    MINUS = 52,
    UNICODE_TEXT = 53,
    BQUOTE_STR = 54,
    EDGE_STATE = 55,
    EOF_IN_STRUCT = 56,
    OPEN_IN_STRUCT = 57,
    PLUS = 58,
    EQUALS = 59,
    PUNCTUATION = 60
};

inline const char* const TOKEN_NAMES[] = {
    "$end",
    "INVALID",
    "CLASS_DIAGRAM",
    "NEWLINE",
    "EOF",
    "SQS",
    "STR",
    "SQE",
    "DOT",
    "GENERICTYPE",
    "LABEL",
    "acc_title",
    "acc_title_value",
    "acc_descr",
    "acc_descr_value",
    "acc_descr_multiline_value",
    "STRUCT_START",
    "STRUCT_STOP",
    "NAMESPACE",
    "STYLE_SEPARATOR",
    "ANNOTATION_START",
    "ANNOTATION_END",
    "CLASS",
    "MEMBER",
    "NOTE_FOR",
    "NOTE",
    "CLASSDEF",
    "ALPHA",
    "COMMA",
    "direction_tb",
    "direction_bt",
    "direction_rl",
    "direction_lr",
    "AGGREGATION",
    "EXTENSION",
    "COMPOSITION",
    "DEPENDENCY",
    "LOLLIPOP",
    "LINE",
    "DOTTED_LINE",
    "CALLBACK",
    "LINK",
    "LINK_TARGET",
    "CLICK",
    "CALLBACK_NAME",
    "CALLBACK_ARGS",
    "HREF",
    "STYLE",
    "CSSCLASS",
    "COLON",
    "BRKT",
    "PCT",
    "MINUS",
    "UNICODE_TEXT",
    "BQUOTE_STR",
    "EDGE_STATE",
    "EOF_IN_STRUCT",
    "OPEN_IN_STRUCT",
    "PLUS",
    "EQUALS",
    "PUNCTUATION"
};

struct Tokens {
    std::vector<int> types;
    std::vector<std::string> texts;
    std::vector<size_t> starts;
};

enum S : int {
    S_INITIAL = 0,
    S_string = 1,
    S_bqstring = 2,
    S_generic = 3,
    S_callbackName = 4,
    S_callbackArgs = 5,
    S_accTitle = 6,
    S_accDescr = 7,
    S_accDescrMultiline = 8,
    S_cls = 9,
    S_classBody = 10,
    S_namespace = 11,
    S_namespaceBody = 12
};

inline Tokens tokenize(const std::string& src) {
    static const int AT_END[13] = {
        T::EOF_TOK,
        T::STR,
        T::EOF_TOK,
        T::GENERICTYPE,
        T::CALLBACK_NAME,
        T::CALLBACK_ARGS,
        T::acc_title_value,
        T::acc_descr_value,
        T::acc_descr_multiline_value,
        T::EOF_TOK,
        T::EOF_IN_STRUCT,
        T::EOF_TOK,
        T::EOF_IN_STRUCT
    };
    const size_t n = src.size();
    std::vector<int> types;
    std::vector<std::string> texts;
    std::vector<size_t> starts;
    std::vector<int> stack;
    int state = S_INITIAL;
    size_t p = 0;

    auto emit = [&](int type, size_t s, size_t e) -> bool {
        types.push_back(type);
        texts.push_back(src.substr(s, e - s));
        starts.push_back(s);
        p = e;
        return true;
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
    auto kw = [&](const std::string& word) -> bool {
        return startsWith(src, word, p) && !isWord(charCodeAt(src, static_cast<int>(p + word.size())));
    };
    auto skipWs = [&](size_t q) -> size_t {
        while (q < n) {
            size_t next = q;
            int cp = charCodeAtUtf8(src, q, &next);
            if (!isSpace(cp)) break;
            q = next;
        }
        return q;
    };
    auto until = [&](char ch) -> size_t {
        size_t e = src.find(ch, p);
        return e == std::string::npos ? n : e;
    };
    auto space = [&](bool leaves) {
        size_t q = p;
        int k = -1;
        while (q < n) {
            size_t next = q;
            int cp = charCodeAtUtf8(src, q, &next);
            if (!isSpace(cp)) break;
            if (cp == 10) k = static_cast<int>(q);
            q = next;
        }
        if (k < 0) {
            p = q;
            return;
        }
        if (leaves) pop();
        emit(T::NEWLINE, p, static_cast<size_t>(k) + 1);
    };
    auto keyword = [&](int c) -> bool {
        switch (c) {
            case 99:
                if (kw("cssClass")) return emit(T::CSSCLASS, p, p + 8);
                if (kw("callback")) return emit(T::CALLBACK, p, p + 8);
                if (kw("click")) return emit(T::CLICK, p, p + 5);
                break;
            case 108:
                if (kw("link")) return emit(T::LINK, p, p + 4);
                break;
            case 110:
                if (kw("note for")) return emit(T::NOTE_FOR, p, p + 8);
                if (kw("note")) return emit(T::NOTE, p, p + 4);
                break;
            case 60:
                if (charCodeAt(src, static_cast<int>(p + 1)) == 60) return emit(T::ANNOTATION_START, p, p + 2);
                break;
            case 62:
                if (charCodeAt(src, static_cast<int>(p + 1)) == 62) return emit(T::ANNOTATION_END, p, p + 2);
                break;
            case 104:
                if (kw("href")) return emit(T::HREF, p, p + 4);
                break;
        }
        return false;
    };
    auto common = [&](int c, size_t nextCpPos) -> bool {
        if (keyword(c)) return true;
        int d = charCodeAt(src, static_cast<int>(p + 1));
        switch (c) {
            case 34:
                push(S_string);
                p++;
                return true;
            case 126:
                push(S_generic);
                p++;
                return true;
            case 96:
                push(S_bqstring);
                p++;
                return true;
            case 95:
                if (kw("_self")) return emit(T::LINK_TARGET, p, p + 5);
                if (kw("_blank")) return emit(T::LINK_TARGET, p, p + 6);
                if (kw("_parent")) return emit(T::LINK_TARGET, p, p + 7);
                if (kw("_top")) return emit(T::LINK_TARGET, p, p + 4);
                break;
            case 60:
                return d == 124 ? emit(T::EXTENSION, p, p + 2) : emit(T::DEPENDENCY, p, p + 1);
            case 124:
                return d == 62 && emit(T::EXTENSION, p, p + 2);
            case 62:
                return emit(T::DEPENDENCY, p, p + 1);
            case 42:
                return emit(T::COMPOSITION, p, p + 1);
            case 111:
                if (!isWord(d)) return emit(T::AGGREGATION, p, p + 1);
                break;
            case 40:
                return d == 41 && emit(T::LOLLIPOP, p, p + 2);
            case 45:
                return d == 45 ? emit(T::LINE, p, p + 2) : emit(T::MINUS, p, p + 1);
            case 46:
                return d == 46 ? emit(T::DOTTED_LINE, p, p + 2) : emit(T::DOT, p, p + 1);
            case 58: {
                size_t q = p + 1;
                while (q < n) {
                    int ch = charCodeAt(src, static_cast<int>(q));
                    if (ch == 58 || ch == 10 || ch == 59) break;
                    q++;
                }
                if (q > p + 1) return emit(T::LABEL, p, q);
                return d == 58 && charCodeAt(src, static_cast<int>(p + 2)) == 58 && emit(T::STYLE_SEPARATOR, p, p + 3);
            }
            case 43:
                return emit(T::PLUS, p, p + 1);
            case 37:
                return emit(T::PCT, p, p + 1);
            case 61:
                return emit(T::EQUALS, p, p + 1);
            case 91:
                return emit(T::SQS, p, p + 1);
            case 93:
                return emit(T::SQE, p, p + 1);
            case 33:
            case 35:
            case 36:
            case 38:
            case 39:
            case 44:
            case 63:
            case 92:
            case 47:
                return emit(T::PUNCTUATION, p, p + 1);
        }
        if (isWord(c)) {
            size_t q = p + 1;
            while (q < n && isWord(charCodeAt(src, static_cast<int>(q)))) q++;
            return emit(T::ALPHA, p, q);
        }
        return isUnicodeLetter(c) && emit(T::UNICODE_TEXT, p, nextCpPos);
    };
    auto directionMatch = [&]() -> bool {
        // Find end of current line (first \n, \r, \u2028, \u2029)
        size_t lineEnd = p;
        while (lineEnd < n) {
            size_t next = lineEnd;
            int cp = charCodeAtUtf8(src, lineEnd, &next);
            if (isLineEnd(cp)) break;
            lineEnd = next;
        }
        size_t dirPos = src.find("direction", p);
        if (dirPos == std::string::npos || dirPos >= lineEnd) return false;
        static const char* DIRS[4] = {"TB", "BT", "RL", "LR"};
        for (int i = 0; i < 4; i++) {
            for (size_t pos = dirPos; pos < lineEnd; pos = src.find("direction", pos + 1)) {
                if (pos == std::string::npos || pos >= lineEnd) break;
                size_t wsStart = pos + 9;
                size_t wsEnd = wsStart;
                while (wsEnd < n) {
                    size_t next = wsEnd;
                    int cp = charCodeAtUtf8(src, wsEnd, &next);
                    if (!isSpace(cp)) break;
                    wsEnd = next;
                }
                if (wsEnd > wsStart && startsWith(src, DIRS[i], wsEnd)) {
                    size_t endMatch = wsEnd + 2;
                    while (endMatch < n && src[endMatch] != '\n') endMatch++;
                    return emit(T::direction_tb + i, p, endMatch);
                }
            }
        }
        return false;
    };
    auto initial = [&](int c, size_t nextCpPos) -> bool {
        if (directionMatch()) return true;
        if (isSpace(c)) {
            space(false);
            return true;
        }
        switch (c) {
            case 37:
                if (charCodeAt(src, static_cast<int>(p + 1)) == 37) {
                    size_t e = until('\n');
                    while (e < n && (src[e] == 10 || src[e] == 13)) e++;
                    p = e;
                    return true;
                }
                break;
            case 97: {
                bool title = startsWith(src, "accTitle", p);
                if (!title && !startsWith(src, "accDescr", p)) break;
                size_t q = skipWs(p + 8);
                int d = charCodeAt(src, static_cast<int>(q));
                if (d == 58) {
                    emit(title ? T::acc_title : T::acc_descr, p, skipWs(q + 1));
                    push(title ? S_accTitle : S_accDescr);
                    return true;
                }
                if (d == 123 && !title) {
                    p = skipWs(q + 1);
                    push(S_accDescrMultiline);
                    return true;
                }
                break;
            }
            case 99:
                if (kw("classDiagram-v2")) return emit(T::CLASS_DIAGRAM, p, p + 15);
                if (kw("classDiagram")) return emit(T::CLASS_DIAGRAM, p, p + 12);
                if (startsWith(src, "call", p) && p + 4 < n) {
                    size_t dummy = p + 4;
                    if (isSpace(charCodeAtUtf8(src, p + 4, &dummy))) {
                        p = skipWs(p + 4);
                        push(S_callbackName);
                        return true;
                    }
                }
                if (kw("classDef")) return emit(T::CLASSDEF, p, p + 8);
                if (kw("class")) {
                    push(S_cls);
                    return emit(T::CLASS, p, p + 5);
                }
                break;
            case 91:
                if (startsWith(src, "*]", p + 1)) return emit(T::EDGE_STATE, p, p + 3);
                break;
            case 115:
                if (kw("style")) return emit(T::STYLE, p, p + 5);
                break;
            case 110:
                if (kw("namespace")) {
                    push(S_namespace);
                    return emit(T::NAMESPACE, p, p + 9);
                }
                break;
            case 44:
                return emit(T::COMMA, p, p + 1);
            case 35:
                return emit(T::BRKT, p, p + 1);
        }
        return common(c, nextCpPos) || (c == 58 && emit(T::COLON, p, p + 1));
    };

    for (;;) {
        if (p >= n) {
            types.push_back(AT_END[state]);
            texts.push_back("");
            starts.push_back(n);
            break;
        }
        size_t nextCpPos = p;
        int c = charCodeAtUtf8(src, p, &nextCpPos);
        bool ok = true;
        switch (state) {
            case S_INITIAL:
                ok = initial(c, nextCpPos);
                break;
            case S_cls:
                if (isSpace(c)) {
                    space(true);
                } else if (c == 125) {
                    pop();
                    pop();
                    emit(T::STRUCT_STOP, p, p + 1);
                } else if (c == 123) {
                    push(S_classBody);
                    emit(T::STRUCT_START, p, p + 1);
                } else {
                    ok = common(c, nextCpPos);
                }
                break;
            case S_classBody:
                if (c == 34) {
                    push(S_string);
                    p++;
                } else if (c == 125) {
                    pop();
                    emit(T::STRUCT_STOP, p, p + 1);
                } else if (c == 91 && startsWith(src, "*]", p + 1)) {
                    emit(T::EDGE_STATE, p, p + 3);
                } else if (c == 123) {
                    emit(T::OPEN_IN_STRUCT, p, p + 1);
                } else if (c == 10) {
                    p++;
                } else {
                    size_t q = p + 1;
                    while (q < n) {
                        unsigned char ch = static_cast<unsigned char>(src[q]);
                        if (ch == 123 || ch == 125 || ch == 10) break;
                        q++;
                    }
                    emit(T::MEMBER, p, q);
                }
                break;
            case S_namespace:
                if (isSpace(c)) {
                    space(true);
                } else if (c == 123) {
                    push(S_namespaceBody);
                    emit(T::STRUCT_START, p, p + 1);
                } else if (c == 125) {
                    pop();
                } else if (c == 110 && kw("namespace")) {
                    push(S_namespace);
                    emit(T::NAMESPACE, p, p + 9);
                } else {
                    ok = common(c, nextCpPos);
                }
                break;
            case S_namespaceBody:
                if (isSpace(c)) {
                    space(false);
                } else if (c == 125) {
                    pop();
                    emit(T::STRUCT_STOP, p, p + 1);
                } else if (c == 110 && kw("namespace")) {
                    push(S_namespace);
                    emit(T::NAMESPACE, p, p + 9);
                } else if (c == 91 && startsWith(src, "*]", p + 1)) {
                    emit(T::EDGE_STATE, p, p + 3);
                } else if (c == 99 && kw("class")) {
                    push(S_cls);
                    emit(T::CLASS, p, p + 5);
                } else {
                    ok = common(c, nextCpPos);
                }
                break;
            case S_string:
                if (c == 34) {
                    pop();
                    p++;
                } else {
                    emit(T::STR, p, until('"'));
                }
                break;
            case S_generic:
                if (c == 34) {
                    push(S_string);
                    p++;
                } else if (keyword(c)) {
                    break;
                } else if (c == 126) {
                    pop();
                    p++;
                } else {
                    emit(T::GENERICTYPE, p, until('~'));
                }
                break;
            case S_bqstring:
                if (c == 34) {
                    push(S_string);
                    p++;
                } else if (keyword(c)) {
                    break;
                } else if (c == 126) {
                    push(S_generic);
                    p++;
                } else if (c == 96) {
                    pop();
                    p++;
                } else {
                    emit(T::BQUOTE_STR, p, until('`'));
                }
                break;
            case S_callbackName:
                if (c == 40) {
                    size_t q = skipWs(p + 1);
                    pop();
                    if (charCodeAt(src, static_cast<int>(q)) == 41) {
                        p = q + 1;
                    } else {
                        push(S_callbackArgs);
                        p++;
                    }
                } else {
                    emit(T::CALLBACK_NAME, p, until('('));
                }
                break;
            case S_callbackArgs:
                if (c == 41) {
                    pop();
                    p++;
                } else {
                    emit(T::CALLBACK_ARGS, p, until(')'));
                }
                break;
            case S_accTitle:
            case S_accDescr:
                emit(state == S_accTitle ? T::acc_title_value : T::acc_descr_value, p, until('\n'));
                pop();
                break;
            case S_accDescrMultiline:
                if (c == 125) {
                    pop();
                    p++;
                } else {
                    emit(T::acc_descr_multiline_value, p, until('}'));
                }
                break;
        }
        if (!ok) {
            types.push_back(T::ERROR);
            texts.push_back("");
            starts.push_back(p);
            return {std::move(types), std::move(texts), std::move(starts)};
        }
    }
    types.push_back(T::END);
    texts.push_back("");
    starts.push_back(n);
    return {std::move(types), std::move(texts), std::move(starts)};
}

inline void parseClassDiagram(const std::string& src, ClassDb& db) {
    constexpr int COUNT = T::PUNCTUATION + 1;
    uint8_t ALPHANUM[COUNT] = {};
    ALPHANUM[T::UNICODE_TEXT] = 1;
    ALPHANUM[T::ALPHA] = 1;
    ALPHANUM[T::MINUS] = 1;

    uint8_t STYLE_PART[COUNT] = {};
    STYLE_PART[T::ALPHA] = 1;
    STYLE_PART[T::COLON] = 1;
    STYLE_PART[T::BRKT] = 1;
    STYLE_PART[T::STYLE] = 1;
    STYLE_PART[T::PCT] = 1;
    STYLE_PART[T::LABEL] = 1;

    uint8_t END_TYPE[COUNT] = {};
    END_TYPE[T::AGGREGATION] = 1;
    END_TYPE[T::EXTENSION] = 2;
    END_TYPE[T::COMPOSITION] = 3;
    END_TYPE[T::DEPENDENCY] = 4;
    END_TYPE[T::LOLLIPOP] = 5;

    const std::string NAME_TOKENS = "'UNICODE_TEXT', 'NUM', 'ALPHA', 'MINUS', 'BQUOTE_STR'";

    auto [types, texts, starts] = tokenize(src);
    size_t i = 0;

    auto fail = [&](const std::string& expected) {
        int t = types[i];
        if (t == T::ERROR) throw syntaxError("class", src, starts[i], "", true);
        throw syntaxError("class", src, starts[i], "Expecting " + expected + ", got '" + std::string(TOKEN_NAMES[t]) + "'");
    };
    auto expect = [&](int type) -> std::string {
        if (types[i] != type) fail("'" + std::string(TOKEN_NAMES[type]) + "'");
        return texts[i++];
    };
    auto alphaNum = [&]() -> std::string {
        if (ALPHANUM[types[i]] == 0) fail("'UNICODE_TEXT', 'NUM', 'ALPHA', 'MINUS'");
        return texts[i++];
    };
    auto name = [&](bool generic) -> std::string {
        std::string out;
        for (;;) {
            if (types[i] == T::BQUOTE_STR) {
                out += texts[i++];
                break;
            }
            if (ALPHANUM[types[i]] == 0) fail(NAME_TOKENS);
            out += texts[i++];
            int t = types[i];
            if (t == T::DOT) {
                out.push_back('.');
                i++;
            } else if (ALPHANUM[t] == 0 && t != T::BQUOTE_STR) {
                break;
            }
        }
        if (generic && types[i] == T::GENERICTYPE) Pele::appendCat(out, {"~", texts[i++], "~"});
        return out;
    };
    auto classLabel = [&]() -> std::string {
        i++;
        std::string label = expect(T::STR);
        expect(T::SQE);
        return label;
    };
    auto members = [&]() -> std::vector<std::string> {
        if (types[i] != T::MEMBER) fail("'STRUCT_STOP', 'MEMBER'");
        std::vector<std::string> list;
        while (types[i] == T::MEMBER) list.push_back(texts[i++]);
        expect(T::STRUCT_STOP);
        std::reverse(list.begin(), list.end());
        return list;
    };
    auto classStatement = [&]() -> std::string {
        i++;
        std::string id = name(true);
        db.addClass(id);
        if (types[i] == T::SQS) db.setClassLabel(id, classLabel());
        switch (types[i]) {
            case T::STYLE_SEPARATOR: {
                i++;
                std::string css = alphaNum();
                db.setCssClass(id, css);
                if (types[i] == T::STRUCT_START) {
                    i++;
                    db.addMembers(id, members());
                }
                break;
            }
            case T::STRUCT_START:
                i++;
                if (types[i] == T::STRUCT_STOP) i++;
                else db.addMembers(id, members());
                break;
            case T::ANNOTATION_START: {
                i++;
                std::string annotation = alphaNum();
                expect(T::ANNOTATION_END);
                db.addAnnotation(id, annotation);
                if (types[i] == T::STRUCT_START) {
                    i++;
                    if (types[i] == T::STRUCT_STOP) i++;
                    else db.addMembers(id, members());
                }
                break;
            }
        }
        return id;
    };
    auto noteStatement = [&]() -> std::string {
        if (types[i++] == T::NOTE) return db.addNote(expect(T::STR));
        std::string id = name(true);
        return db.addNote(expect(T::STR), id, true);
    };
    struct Frame {
        std::string id;
        std::vector<std::string> classes;
        std::vector<std::string> notes;
    };
    auto namespaceStatement = [&]() {
        std::vector<Frame> open;
        for (;;) {
            i++;
            std::string ns = name(false);
            std::string id = (types[i] == T::SQS) ? db.addNamespace(ns, classLabel(), true) : db.addNamespace(ns);
            expect(T::STRUCT_START);
            if (types[i] == T::NEWLINE) i++;
            open.push_back({id, {}, {}});
            bool item = true;
            while (item) {
                int t = types[i];
                if (t == T::NAMESPACE) break;
                if (t == T::CLASS) open.back().classes.push_back(classStatement());
                else if (t == T::NOTE || t == T::NOTE_FOR) open.back().notes.push_back(noteStatement());
                else fail("'NAMESPACE', 'CLASS', 'NOTE_FOR', 'NOTE'");
                for (;;) {
                    if (types[i] == T::NEWLINE) {
                        int next = types[i + 1];
                        if (next == T::NAMESPACE || next == T::CLASS || next == T::NOTE || next == T::NOTE_FOR) {
                            i++;
                            break;
                        }
                        i++;
                    }
                    expect(T::STRUCT_STOP);
                    Frame done = std::move(open.back());
                    open.pop_back();
                    db.addClassesToNamespace(done.id, done.classes, done.notes);
                    db.popNamespace();
                    if (open.empty()) {
                        item = false;
                        break;
                    }
                }
            }
            if (open.empty()) return;
        }
    };
    auto relationEnd = [&]() -> int {
        int type = END_TYPE[types[i]];
        if (type == 0) return -1;
        i++;
        return type - 1;
    };
    auto classOrRelation = [&]() {
        std::string id1 = name(true);
        int t = types[i];
        if (t == T::LABEL) {
            db.addMember(id1, db.cleanupLabel(texts[i++]));
            return;
        }
        std::string title1 = "none";
        if (t == T::STR) {
            title1 = texts[i++];
            t = types[i];
        } else if (END_TYPE[t] == 0 && t != T::LINE && t != T::DOTTED_LINE) {
            return;
        }
        int type1 = relationEnd();
        t = types[i];
        if (t != T::LINE && t != T::DOTTED_LINE) {
            fail("'AGGREGATION', 'EXTENSION', 'COMPOSITION', 'DEPENDENCY', 'LOLLIPOP', 'LINE', 'DOTTED_LINE'");
        }
        i++;
        int type2 = relationEnd();
        std::string title2 = (types[i] == T::STR) ? texts[i++] : "none";
        std::string id2 = name(true);
        ClassRelation relation;
        relation.id1 = id1;
        relation.id2 = id2;
        relation.relation = {type1, type2, t == T::LINE ? 0 : 1};
        relation.relationTitle1 = title1;
        relation.relationTitle2 = title2;
        if (types[i] == T::LABEL) {
            relation.title = db.cleanupLabel(texts[i++]);
            relation.hasTitle = true;
        }
        db.addRelation(std::move(relation));
    };
    auto styles = [&]() -> std::vector<std::string> {
        std::vector<std::string> out;
        for (;;) {
            if (STYLE_PART[types[i]] == 0) fail("'ALPHA', 'NUM', 'COLON', 'UNIT', 'SPACE', 'BRKT', 'STYLE', 'PCT', 'LABEL'");
            std::string s = texts[i++];
            while (STYLE_PART[types[i]] == 1) s += texts[i++];
            out.push_back(s);
            if (types[i] != T::COMMA) return out;
            i++;
        }
    };
    auto tooltip = [&](const std::string& id) {
        if (types[i] == T::STR) db.setTooltip(id, texts[i++]);
    };
    auto linkStatement = [&](const std::string& id) {
        std::string href = expect(T::STR);
        bool hasTip = (types[i] == T::STR);
        std::string tip = hasTip ? texts[i++] : "";
        if (types[i] == T::LINK_TARGET) db.setLink(id, href, texts[i++], true);
        else db.setLink(id, href);
        if (hasTip) db.setTooltip(id, tip);
    };
    auto statement = [&]() {
        int t = types[i];
        switch (t) {
            case T::UNICODE_TEXT:
            case T::ALPHA:
            case T::MINUS:
            case T::BQUOTE_STR:
                classOrRelation();
                return;
            case T::NAMESPACE:
                namespaceStatement();
                return;
            case T::CLASS:
                classStatement();
                return;
            case T::MEMBER:
                i++;
                return;
            case T::ANNOTATION_START: {
                i++;
                std::string annotation = alphaNum();
                expect(T::ANNOTATION_END);
                db.addAnnotation(name(true), annotation);
                return;
            }
            case T::CALLBACK: {
                i++;
                std::string id = name(true);
                db.setClickEvent(id, expect(T::STR));
                tooltip(id);
                return;
            }
            case T::LINK:
                i++;
                linkStatement(name(true));
                return;
            case T::CLICK: {
                i++;
                std::string id = name(true);
                if (types[i] == T::HREF) {
                    i++;
                    linkStatement(id);
                    return;
                }
                std::string fn = expect(T::CALLBACK_NAME);
                if (types[i] == T::CALLBACK_ARGS) db.setClickEvent(id, fn, texts[i++]);
                else db.setClickEvent(id, fn);
                tooltip(id);
                return;
            }
            case T::STYLE: {
                i++;
                std::string id = expect(T::ALPHA);
                db.setCssStyle(id, styles());
                return;
            }
            case T::CSSCLASS: {
                i++;
                std::string ids = expect(T::STR);
                db.setCssClass(ids, expect(T::ALPHA));
                return;
            }
            case T::NOTE:
            case T::NOTE_FOR:
                noteStatement();
                return;
            case T::CLASSDEF: {
                i++;
                std::vector<std::string> ids = {expect(T::ALPHA)};
                while (types[i] == T::COMMA) {
                    i++;
                    ids.push_back(expect(T::ALPHA));
                }
                db.defineClass(ids, styles());
                return;
            }
            case T::direction_tb:
            case T::direction_bt:
            case T::direction_rl:
            case T::direction_lr:
                i++;
                db.setDirection(t == T::direction_tb ? "TB" : t == T::direction_bt ? "BT" : t == T::direction_rl ? "RL" : "LR");
                return;
            case T::acc_title:
                i++;
                db.setAccTitle(trimStr(expect(T::acc_title_value)));
                return;
            case T::acc_descr:
                i++;
                db.setAccDescription(trimStr(expect(T::acc_descr_value)));
                return;
            case T::acc_descr_multiline_value:
                db.setAccDescription(trimStr(texts[i++]));
                return;
        }
        fail(
            "'acc_title', 'acc_descr', 'acc_descr_multiline_value', 'NAMESPACE', 'ANNOTATION_START', 'CLASS', 'MEMBER', 'NOTE_FOR', 'NOTE', 'CLASSDEF', 'direction_tb', 'CALLBACK', 'LINK', 'CLICK', 'STYLE', 'CSSCLASS', 'ALPHA', 'MINUS', 'BQUOTE_STR'"
        );
    };

    bool header = (types[i] == T::CLASS_DIAGRAM);
    if (header) {
        i++;
        expect(T::NEWLINE);
    }
    for (;;) {
        statement();
        if (types[i] != T::NEWLINE) break;
        i++;
        if (types[i] == T::EOF_TOK || types[i] == T::END) break;
    }
    if (header) expect(T::EOF_TOK);
    expect(T::END);
}

} // namespace detail_parse

// ============================================================================
// src/diagrams/class/render.ts
// ============================================================================

namespace detail_render {

constexpr double NODE_SEP = 40.0;
constexpr double EDGE_SEP = 16.0;
constexpr double RANK_SEP = 56.0;
constexpr double GROUP_PAD = 20.0;
constexpr double LOOP = 26.0;
constexpr double PAD_X = 12.0;
constexpr double TITLE_PAD = 8.0;
constexpr double SECTION_PAD = 6.0;
constexpr double EMPTY_SECTION = 10.0;
constexpr double MIN_WIDTH = 64.0;
constexpr double FOLD = 8.0;

inline double numberOption(const Config& config, const std::string& key, double fallback) noexcept {
    if (!config.isMap()) return fallback;
    auto it = config.mVal.find(key);
    if (it != config.mVal.end() && it->second.isNumber() && it->second.nVal > 0.0) {
        return std::min(it->second.nVal, 2000.0);
    }
    return fallback;
}

template <typename Opts, typename = void>
struct HasMonospaceVec : std::false_type {};
template <typename Opts>
struct HasMonospaceVec<Opts, std::void_t<decltype(std::declval<const Opts&>().monospace)>> : std::true_type {};

template <typename Opts>
inline bool hasMonospaceOption(const Opts& opts, const std::string& item) noexcept {
    if constexpr (HasMonospaceVec<Opts>::value) {
        for (const auto& m : opts.monospace) {
            if (m == item) return true;
        }
    }
    return false;
}

struct Line {
    std::string text;
    double width = 0.0;
    std::string classifier;
};

struct AnnotationItem {
    bool isLabel = false;
    std::string text;
    Label label;
};

struct View {
    GraphNode node;
    ResolvedStyle style;
    Label label;
    double w = 0.0;
    double h = 0.0;
    int loops = 0;
    std::vector<AnnotationItem> annotations;
    std::vector<Line> attributes;
    std::vector<Line> methods;
    double titleH = 0.0;
    double attributesH = 0.0;
    double fontSize = 16.0;
};

struct End {
    size_t edgeIdx = 0;
    size_t at = 0;
    double toward = 0.0;
    int order = 0;
    double room = 0.0;
    int side = -1;
    bool after = false;
};

inline Line memberLine(const ClassMember& member, const TextMeasurer& measurer, double size, bool isMono) noexcept {
    std::string text = trimStr(decodeEntities(member.getDisplayDetails().first));
    int style = (member.classifier == "*" ? Style::Italic : 0) | (isMono ? Style::Mono : 0);
    return {text, measurer.width(text, size, style), member.classifier};
}

inline Rendered draw(const ClassDb& db, const Config& config, const RenderOptions& options, bool turned, bool tight) {
    Config conf = Config::makeMap();
    if (config.isMap()) {
        auto it = config.mVal.find("class");
        if (it != config.mVal.end() && it->second.isMap()) conf = it->second;
    }
    bool hierarchical = true;
    if (conf.isMap()) {
        auto it = conf.mVal.find("hierarchicalNamespaces");
        if (it != conf.mVal.end() && it->second.isBool() && !it->second.bVal) hierarchical = false;
    }
    bool hideEmpty = false;
    if (conf.isMap()) {
        auto it = conf.mVal.find("hideEmptyMembersBox");
        if (it != conf.mVal.end() && it->second.isBool() && it->second.bVal) hideEmpty = true;
    }
    ClassGraph graph = buildClassGraph(db, hierarchical);
    bool monospaced = hasMonospaceOption(options, "members");
    auto mono = [&](const ResolvedStyle& style) -> bool {
        return monospaced && style.text.find("\"font-family:") == std::string::npos && style.text.find(";font-family:") == std::string::npos;
    };
    double size = options.fontSize > 0.0 ? options.fontSize : 16.0;
    double small = std::round(size * 0.875);
    double tiny = std::round(size * 0.8);
    double tinyHeight = std::round(tiny * 1.4);
    const TextMeasurer& measurer = getMeasurer(options);
    double pad = options.hasPadding ? options.padding : 8.0;

    auto mathText = [&](const std::string& raw, double fontSize, Label& outLabel) -> bool {
        if (raw.find("$$") == std::string::npos) return false;
        outLabel = layoutLabel(raw, false, measurer, fontSize, 4000.0);
        for (const auto& line : outLabel.lines) {
            if (hasMath(line)) return true;
        }
        return false;
    };
    auto cardWidth = [&](const std::string& raw) -> double {
        if (raw.empty()) return 0.0;
        Label lbl;
        if (mathText(raw, tiny, lbl)) return lbl.width;
        return measurer.width(decodeEntities(raw), tiny, 0);
    };
    std::string rootDir = turned ? "TB" : direction(db.direction);

    Pele::FlatMap<std::string, std::string> effect;
    std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> fromDefs;
    std::vector<std::string> keptOrder;
    Pele::FlatMap<std::string, std::string> kept;
    auto keep = [&](const std::string& decl) {
        std::string prop;
        auto itE = effect.find(decl);
        if (itE == effect.end()) {
            ResolvedStyle one = resolveStyle({decl});
            if (!one.shape.empty() || !one.text.empty()) {
                size_t col = decl.find(':');
                prop = toLowerAscii(trimStr(col == std::string::npos ? "" : decl.substr(0, col)));
            }
            effect[decl] = prop;
        } else {
            prop = itE->second;
        }
        if (!prop.empty()) {
            if (!kept.count(prop)) keptOrder.push_back(prop);
            kept[prop] = decl;
        }
    };
    auto classStyle = [&](const ClassNode& cls) -> ResolvedStyle {
        kept.clear();
        keptOrder.clear();
        auto itB = fromDefs.find(cls.cssClasses);
        if (itB == fromDefs.end()) {
            for (const auto& name : splitStr(cls.cssClasses, ' ')) {
                auto itDef = db.styleClasses.find(name);
                if (itDef != db.styleClasses.end()) {
                    for (const auto& s : itDef->second.styles) {
                        for (const auto& part : splitStr(s, ',')) keep(part);
                    }
                }
            }
            std::vector<std::pair<std::string, std::string>> base;
            for (const auto& p : keptOrder) base.push_back({p, kept[p]});
            fromDefs[cls.cssClasses] = std::move(base);
        } else {
            for (const auto& [prop, decl] : itB->second) {
                if (!kept.count(prop)) keptOrder.push_back(prop);
                kept[prop] = decl;
            }
        }
        for (const auto& s : cls.styles) keep(s);
        if (kept.empty()) return resolveStyle({});
        std::vector<std::string> vals;
        for (const auto& p : keptOrder) vals.push_back(kept[p]);
        return resolveStyle(vals);
    };

    std::vector<int> children(graph.nodes.size(), 0);
    for (const auto& node : graph.nodes) {
        if (node.parent >= 0) children[node.parent]++;
    }

    std::vector<uint8_t> crossed(graph.nodes.size(), 0);
    for (const auto& edge : graph.edges) {
        if (edge.source < 0 || edge.target < 0) continue;
        int a = graph.nodes[edge.source].parent;
        int b = graph.nodes[edge.target].parent;
        if (a == b) continue;
        for (int g = a; g >= 0 && crossed[g] == 0; g = graph.nodes[g].parent) crossed[g] = 1;
        for (int g = b; g >= 0 && crossed[g] == 0; g = graph.nodes[g].parent) crossed[g] = 1;
    }

    auto makeBaseView = [&](const GraphNode& node, const Label& label, double w, double h, double fontSize) -> View {
        View v;
        v.node = node;
        v.style = resolveStyle({});
        v.label = label;
        v.w = w;
        v.h = h;
        v.fontSize = fontSize;
        return v;
    };

    auto makeClassView = [&](const GraphNode& node, const ClassNode& cls) -> View {
        ResolvedStyle style = classStyle(cls);
        double fontSize = style.hasFontSize ? style.fontSize : size;
        double memberSize = std::round(fontSize * 0.875);
        double lineHeight = std::round(memberSize * 1.5);
        std::string generic = !cls.type.empty()
            ? Pele::cat({"&lt;", replaceAllStr(replaceAllStr(parseGenericTypes(cls.type), "<", "&lt;"), ">", "&gt;"), "&gt;"})
            : "";
        Label label = layoutLabel(cls.label + generic, false, measurer, fontSize, 4000.0, Style::Bold | (style.italic ? Style::Italic : 0));
        std::vector<AnnotationItem> annotations;
        for (const auto& a : cls.annotations) {
            std::string raw = "\xC2\xAB" + trimStr(a) + "\xC2\xBB"; // «...»
            Label lbl;
            if (mathText(raw, tiny, lbl)) {
                annotations.push_back({true, "", std::move(lbl)});
            } else {
                annotations.push_back({false, Pele::cat({"\xC2\xAB", trimStr(decodeEntities(a)), "\xC2\xBB"}),  {}});
            }
        }
        std::vector<Line> attributes;
        for (const auto& m : cls.members) attributes.push_back(memberLine(m, measurer, memberSize, mono(style)));
        std::vector<Line> methods;
        for (const auto& m : cls.methods) methods.push_back(memberLine(m, measurer, memberSize, mono(style)));
        double inner = label.width;
        double annotationsH = 0.0;
        for (const auto& a : annotations) {
            inner = std::max(inner, !a.isLabel ? measurer.width(a.text, tiny, 0) : a.label.width);
            annotationsH += !a.isLabel ? tinyHeight : std::max(tinyHeight, a.label.height);
        }
        for (const auto& line : attributes) inner = std::max(inner, line.width);
        for (const auto& line : methods) inner = std::max(inner, line.width);
        bool sections = !(hideEmpty && attributes.empty() && methods.empty());
        auto section = [&](size_t count) -> double {
            return !sections ? 0.0 : (count > 0 ? (2.0 * SECTION_PAD + static_cast<double>(count) * lineHeight) : EMPTY_SECTION);
        };
        double titleH = 2.0 * TITLE_PAD + annotationsH + std::max(label.height, std::round(fontSize * 1.5));
        double attributesH = section(attributes.size());
        View v;
        v.node = node;
        v.style = std::move(style);
        v.label = std::move(label);
        v.w = std::max(inner + 2.0 * PAD_X, MIN_WIDTH);
        v.h = titleH + attributesH + section(methods.size());
        v.annotations = std::move(annotations);
        v.attributes = std::move(attributes);
        v.methods = std::move(methods);
        v.titleH = titleH;
        v.attributesH = attributesH;
        v.fontSize = fontSize;
        return v;
    };

    std::vector<View> views;
    std::vector<CNode> cnodes;
    for (size_t i = 0; i < graph.nodes.size(); i++) {
        const auto& node = graph.nodes[i];
        View view;
        CNode c;
        if (node.isGroup) {
            Label label = layoutLabel(node.label, false, measurer, size, 4000.0);
            view = makeBaseView(node, label, 0.0, 0.0, size);
            if (children[i] > 0) {
                c = cnode(0.0, 0.0);
                c.isGroup = true;
                c.padX = GROUP_PAD;
                c.padTop = label.height > 0.0 ? label.height + 16.0 : GROUP_PAD;
                c.padBottom = GROUP_PAD;
                c.minW = label.width + 2.0 * GROUP_PAD - 12.0 + (crossed[i] == 1 && !tight ? std::min(label.width + 24.0, 240.0) : 0.0);
            } else {
                view.w = std::max(label.width + 2.0 * GROUP_PAD, 80.0);
                view.h = label.height + 2.0 * GROUP_PAD;
                c = cnode(view.w, view.h);
            }
        } else if (node.classNode) {
            view = makeClassView(node, *node.classNode);
            c = cnode(view.w, view.h);
        } else if (node.shape == "note") {
            Label label = layoutLabel(node.label, true, measurer, small, 260.0);
            view = makeBaseView(node, label, std::max(label.width + 2.0 * PAD_X, 40.0), label.height + 2.0 * TITLE_PAD, small);
            c = cnode(view.w, view.h);
        } else {
            Label label = layoutLabel(node.label, false, measurer, small, 200.0);
            view = makeBaseView(node, label, std::max(label.width, 12.0) + 4.0, std::max(label.height, 12.0), small);
            c = cnode(view.w, view.h);
        }
        c.parent = node.parent;
        c.seq = static_cast<double>(i);
        views.push_back(std::move(view));
        cnodes.push_back(std::move(c));
    }

    std::vector<CEdge> cedges;
    std::vector<GraphEdge> drawn;
    std::vector<Label> edgeLabels;
    std::vector<double> startRoom;
    std::vector<double> endRoom;
    std::vector<double> crowd(views.size() * 4, 0.0);
    auto meet = [&](int item, int slot, double room) {
        crowd[item * 4 + slot] += 1.0;
        if (room > crowd[item * 4 + slot + 1]) crowd[item * 4 + slot + 1] = room;
    };
    for (const auto& edge : graph.edges) {
        if (edge.source < 0 || edge.target < 0) continue;
        Label label = layoutLabel(edge.hasLabel ? edge.label : "", true, measurer, small, 200.0);
        double startW = cardWidth(edge.startLabelRight);
        double endW = cardWidth(edge.endLabelLeft);
        if (edge.source == edge.target) {
            views[edge.source].loops++;
            cnodes[edge.source].w += 2.0 * (LOOP + (label.width > 0.0 ? label.width + 12.0 : 0.0));
        } else {
            meet(edge.source, 0, startW);
            meet(edge.target, 2, endW);
        }
        startRoom.push_back(startW);
        endRoom.push_back(endW);
        drawn.push_back(edge);
        edgeLabels.push_back(label);
        cedges.push_back(cedge(
            edge.source,
            edge.target,
            1,
            label.width > 0.0 ? label.width + 12.0 : 0.0,
            label.height > 0.0 ? label.height + 4.0 : 0.0
        ));
    }

    if (rootDir == "TB" || rootDir == "BT") {
        for (size_t i = 0; i < views.size(); i++) {
            if (!views[i].node.classNode) continue;
            double need = 0.0;
            for (int slot = 0; slot < 4; slot += 2) {
                double count = crowd[i * 4 + slot];
                double room = crowd[i * 4 + slot + 1];
                if (count > 1.0) need = std::max(need, (count - 1.0) * (room > 0.0 ? room + 24.0 : 14.0) + 24.0);
            }
            need = std::min(need, 480.0);
            if (need > views[i].w) {
                cnodes[i].w += need - views[i].w;
                views[i].w = need;
            }
        }
    }

    bool lettered = false;
    for (size_t i = 0; i < drawn.size() && !lettered; i++) {
        lettered = startRoom[i] > 0.0 || endRoom[i] > 0.0;
    }
    double tallCards = 0.0;
    if (rootDir == "TB" || rootDir == "BT") {
        auto cardHeight = [&](const std::string& raw) -> double {
            if (raw.empty()) return 0.0;
            Label lbl;
            double mh = mathText(raw, tiny, lbl) ? lbl.height : 0.0;
            return std::max(tinyHeight, mh);
        };
        for (const auto& edge : drawn) {
            tallCards = std::max(tallCards, cardHeight(edge.startLabelRight) + cardHeight(edge.endLabelLeft) - 2.0 * tinyHeight);
        }
    }
    LayeredOptions lopt;
    lopt.nodeSep = numberOption(conf, "nodeSpacing", NODE_SEP);
    lopt.edgeSep = EDGE_SEP;
    lopt.rankSep = numberOption(conf, "rankSpacing", lettered ? RANK_SEP + 16.0 : RANK_SEP) + tallCards;
    lopt.portSep = 20.0;
    lopt.apart = true;
    CompoundResult layout = compoundLayout(cnodes, cedges, rootDir, lopt);

    std::vector<int> sideKeys;
    std::unordered_map<int, std::vector<End>> sides;
    struct CardItem {
        std::string text;
        bool hasMath = false;
        Label math;
        double x = 0.0;
        double y = 0.0;
        double w = 0.0;
        double h = 0.0;
        int parent = -1;
    };
    std::vector<CardItem> cards;

    auto attach = [&](size_t edgeIdx, size_t at, int item, double room, int order, End& outEnd) -> bool {
        auto& route = cedges[edgeIdx].route;
        const auto& c = cnodes[item];
        int side = endSide(route, at, c);
        if (side < 0) return false;
        double slack = (c.w - views[item].w) / 2.0;
        if (side == 1) route[at] -= slack;
        else if (side == 3) route[at] += slack;
        int across = side & 1;
        double toward = route[(at == 0 ? 3 : at - 3) + across];
        double bend = toward - route[at + across];
        outEnd = {edgeIdx, at, toward, order, room, side, across ? (bend > 0.0) : (bend >= 0.0)};
        int key = item * 4 + side;
        if (!sides.count(key)) sideKeys.push_back(key);
        sides[key].push_back(outEnd);
        return true;
    };

    std::vector<bool> hasStart(cedges.size(), false);
    std::vector<End> starts(cedges.size());
    std::vector<bool> hasEnd(cedges.size(), false);
    std::vector<End> ends(cedges.size());
    for (size_t i = 0; i < cedges.size(); i++) {
        const auto& ce = cedges[i];
        bool routed = (ce.src != ce.dst && ce.route.size() >= 6);
        if (routed) {
            hasStart[i] = attach(i, 0, ce.src, startRoom[i], static_cast<int>(i), starts[i]);
            hasEnd[i] = attach(i, ce.route.size() - 3, ce.dst, endRoom[i], static_cast<int>(i), ends[i]);
        }
    }

    auto onBorder = [&](int item, double x, double y) -> bool {
        for (int g = cnodes[item].parent, hops = 0; g >= 0 && hops < 10000; g = cnodes[g].parent, hops++) {
            const auto& c = cnodes[g];
            double dx = std::abs(x - c.x);
            double dy = std::abs(y - c.y);
            if ((std::abs(dy - c.h / 2.0) < 0.5 && dx <= c.w / 2.0 + 0.5) ||
                (std::abs(dx - c.w / 2.0) < 0.5 && dy <= c.h / 2.0 + 0.5)) {
                return true;
            }
        }
        return false;
    };

    std::vector<double> gaps;
    for (int key : sideKeys) {
        auto& list = sides[key];
        if (list.size() < 2) continue;
        int item = key >> 2;
        int across = key & 1;
        double length = across ? views[item].h : views[item].w;
        std::sort(list.begin(), list.end(), [](const End& a, const End& b) {
            if (a.toward != b.toward) return a.toward < b.toward;
            return a.order < b.order;
        });
        if (gaps.size() < list.size()) gaps.resize(list.size(), 0.0);
        double total = 0.0;
        for (size_t j = 0; j + 1 < list.size(); j++) {
            const auto& a = list[j];
            const auto& b = list[j + 1];
            double gap = across ? 20.0 : 24.0;
            if (a.room > 0.0 && a.after) gap += across ? (tinyHeight + 6.0) : a.room;
            if (b.room > 0.0 && !b.after) gap += across ? (tinyHeight + 6.0) : b.room;
            gaps[j] = gap;
            total += gap;
        }
        double scale = std::min(1.0, (length - 24.0) / total);
        if (scale <= 0.0) continue;
        double at = (across ? cnodes[item].y : cnodes[item].x) - (total * scale) / 2.0;
        for (size_t j = 0; j < list.size(); j++) {
            auto& route = cedges[list[j].edgeIdx].route;
            int end = static_cast<int>(list[j].at);
            int step = (end == 0) ? 3 : -3;
            int first = end + step;
            int border = -1;
            for (int k = first; k >= 0 && k < static_cast<int>(route.size()) && std::abs(route[k + across] - route[first + across]) < 0.5; k += step) {
                if (onBorder(item, route[k], route[k + 1])) {
                    border = k;
                    break;
                }
            }
            if (border >= 0 && std::abs(route[first + across] - at) <= 24.0) {
                for (int k = first; k != border + step; k += step) route[k + across] = at;
            }
            route[end + across] = at;
            if (j + 1 < list.size()) at += gaps[j] * scale;
        }
    }

    auto straightenEnd = [&](std::vector<double>& route, bool hasEndVal, const End& end, double room, bool first) {
        if (!hasEndVal || room <= 0.0) return;
        int across = end.side & 1;
        size_t at = first ? 0 : (route.size() - 3);
        size_t along = at + 1 - across;
        double reach = std::abs(route[(first ? 3 : at - 3) + 1 - across] - route[along]);
        double length = std::min(across ? (room + 10.0) : (tinyHeight + 6.0), reach * 0.35);
        if (length < 4.0) return;
        double pt[3] = {route[at], route[at + 1], route[at + 2]};
        pt[1 - across] += (end.side == 0 || end.side == 3 ? -1.0 : 1.0) * length;
        size_t insertPos = first ? 3 : at;
        route.insert(route.begin() + insertPos, {pt[0], pt[1], pt[2]});
    };
    for (size_t i = 0; i < cedges.size(); i++) {
        straightenEnd(cedges[i].route, hasStart[i], starts[i], startRoom[i], true);
        straightenEnd(cedges[i].route, hasEnd[i], ends[i], endRoom[i], false);
    }

    double minX = 0.0;
    double minY = 0.0;
    double maxX = layout.width;
    double maxY = layout.height;
    auto card = [&](int parent, const std::string& raw, double width, double x, double y, int side, bool after) {
        if (raw.empty()) return;
        Label math;
        bool isMath = mathText(raw, tiny, math);
        double h = isMath ? std::max(tinyHeight, math.height) : tinyHeight;
        double cx = x;
        double cy = y;
        if (side & 1) {
            cx += (side == 3 ? -1.0 : 1.0) * (5.0 + width / 2.0);
            cy += (after ? 1.0 : -1.0) * (7.0 + h / 2.0);
        } else {
            cx += (after ? 1.0 : -1.0) * (9.0 + width / 2.0);
            cy += (side == 0 ? -1.0 : 1.0) * (3.0 + h / 2.0);
        }
        cards.push_back({decodeEntities(raw), isMath, std::move(math), cx, cy, width, h, parent});
        minX = std::min(minX, cx - width / 2.0);
        maxX = std::max(maxX, cx + width / 2.0);
        minY = std::min(minY, cy - h / 2.0);
        maxY = std::max(maxY, cy + h / 2.0);
    };

    Pele::FlatMap<int, int> loopSeen;
    std::vector<int> loopAt(cedges.size(), 0);
    for (size_t i = 0; i < cedges.size(); i++) {
        auto& ce = cedges[i];
        const auto& edge = drawn[i];
        const auto& route = ce.route;
        if (ce.src == ce.dst) {
            int k = loopSeen[ce.src]++;
            loopAt[i] = k;
            const auto& c = cnodes[ce.src];
            const auto& view = views[ce.src];
            double x = c.x + view.w / 2.0;
            double spread = std::min(view.h / 2.0 - 4.0, 8.0 + k * 6.0);
            card(ce.src, edge.startLabelRight, startRoom[i], x, c.y - spread, 1, false);
            card(ce.dst, edge.endLabelLeft, endRoom[i], x, c.y + spread, 1, true);
            ce.labelX = c.x + view.w / 2.0 + LOOP + k * 8.0 + 4.0 + ce.labelW / 2.0;
            ce.labelY = c.y;
        } else if (route.size() >= 6) {
            size_t last = route.size() - 3;
            card(ce.src, edge.startLabelRight, startRoom[i], route[0], route[1], hasStart[i] ? starts[i].side : 2, hasStart[i] ? starts[i].after : true);
            card(ce.dst, edge.endLabelLeft, endRoom[i], route[last], route[last + 1], hasEnd[i] ? ends[i].side : 0, hasEnd[i] ? ends[i].after : true);
        }
    }

    double available = options.maxWidth;
    if (available > 0.0 && maxX - minX + 2.0 * pad > available) {
        std::vector<CNode> combined = cnodes;
        for (const auto& item : cards) {
            CNode cn = cnode(item.w, item.h, item.parent);
            cn.x = item.x;
            cn.y = item.y;
            combined.push_back(cn);
        }
        CompoundResult wrapped;
        if (wrapComponents(combined, cedges, std::max(1.0, available - 2.0 * pad), numberOption(conf, "nodeSpacing", NODE_SEP), wrapped)) {
            for (size_t i = 0; i < cnodes.size(); i++) cnodes[i] = combined[i];
            for (size_t i = 0; i < cards.size(); i++) {
                cards[i].x = combined[cnodes.size() + i].x;
                cards[i].y = combined[cnodes.size() + i].y;
            }
            minX = minY = 0.0;
            maxX = wrapped.width;
            maxY = wrapped.height;
        }
    }

    double ox = pad - minX;
    double oy = pad - minY;
    double width = std::ceil(maxX - minX + 2.0 * pad);
    double height = std::ceil(maxY - minY + 2.0 * pad);
    std::vector<LinkInfo> links;

    Crossings crossings;
    if (!graph.nodes.empty() && graph.nodes[0].isGroup) {
        for (const auto& e : cedges) markCrossings(crossings, e.route);
    }

    auto isTargetAllowed = [](const std::string& t) {
        return t == "_self" || t == "_blank" || t == "_parent" || t == "_top";
    };

    auto classSvg = [&](const View& view, const ClassNode& cls, const std::string& id, const std::string& at) -> std::string {
        double w = view.w;
        double h = view.h;
        const auto& style = view.style;
        double left = -w / 2.0;
        double top = -h / 2.0;
        double memberSize = std::round(view.fontSize * 0.875);
        double lineHeight = std::round(memberSize * 1.5);
        std::string body = Pele::cat({"<rect x=\"", num(left), "\" y=\"", num(top), "\" width=\"", num(w), "\" height=\"", num(h), "\" fill=\"var(--_s)\" stroke=\"var(--_b)\"", rounded(style.shape), "/>"});
        std::string ink = inkFill(style);
        double first = top + view.titleH;
        double second = first + view.attributesH;
        if (h > view.titleH) {
            std::string stroke = style.hasStroke ? Pele::cat({" style=\"stroke:", esc(style.stroke), "\""}) : "";
            Pele::appendCat(body, {"<path class=\"pele-divider\" d=\"M", num(left), ",", num(first), "H", num(-left), "M", num(left), ",", num(second), "H", num(-left), "\" fill=\"none\" stroke=\"var(--_b)\"", stroke, "/>"});
        }
        double y = top + TITLE_PAD;
        for (const auto& annotation : view.annotations) {
            if (!annotation.isLabel) {
                Pele::appendCat(body, {"<text class=\"pele-annotation\" x=\"0\" y=\"", num(y + tinyHeight / 2.0 + tiny * 0.35), "\" text-anchor=\"middle\" font-size=\"", num(tiny), "\"", (!ink.empty() ? ink : " fill=\"var(--_m)\""), ">", esc(annotation.text), "</text>"});
                y += tinyHeight;
            } else {
                double ah = std::max(tinyHeight, annotation.label.height);
                body += labelSvg(annotation.label,  0.0,  y + ah / 2.0, Pele::cat({" class=\"pele-annotation\" font-size=\"", num(tiny), "\"", (!ink.empty() ? ink : " fill=\"var(--_m)\"")}),  options.icons);
                y += ah;
            }
        }
        std::string titleSize = (view.fontSize == size) ? "" : Pele::cat({" font-size=\"", num(view.fontSize), "\""});
        body += labelSvg(view.label,  0.0,  (y + first - TITLE_PAD) / 2.0, Pele::cat({" class=\"pele-label\"", titleSize, " font-weight=\"var(--_hw)\"", ink, style.text}),  options.icons);
        auto renderLines = [&](const std::vector<Line>& list, const std::string& kind, double from) -> std::string {
            std::string out;
            double baseline = from + SECTION_PAD + lineHeight / 2.0 + memberSize * 0.35;
            for (const auto& line : list) {
                std::string mark;
                if (line.classifier == "$") {
                    mark = Pele::cat({" pele-static\"", (!style.text.empty() ? Pele::cat({style.text.substr(0, style.text.size() - 1), "text-decoration:underline\""}) : " style=\"text-decoration:underline\"")});
                } else if (line.classifier == "*") {
                    mark = Pele::cat({" pele-abstract\" font-style=\"italic\"", style.text});
                } else {
                    mark = Pele::cat({"\"", style.text});
                }
                Pele::appendCat(out, {"<text class=\"pele-member pele-", kind, mark, " x=\"", num(left + PAD_X), "\" y=\"", num(baseline), "\">", esc(line.text), "</text>"});
                baseline += lineHeight;
            }
            return out;
        };
        if (!view.attributes.empty() || !view.methods.empty()) {
            Pele::appendCat(body, {"<g class=\"pele-members\" font-size=\"", num(memberSize), "\"", ink, (mono(style) ? Pele::cat({" font-family=\"", FONT_MONO, "\""}) : ""), ">", renderLines(view.attributes, "attribute", first), renderLines(view.methods, "method", second), "</g>"});
        }
        if (cls.hasTooltip && !cls.tooltip.empty()) body = "<title>" + escText(cls.tooltip) + "</title>" + body;
        if (cls.hasLink && !cls.link.empty()) {
            std::string targetAttr = (cls.hasLinkTarget && isTargetAllowed(cls.linkTarget)) ? Pele::cat({" target=\"", cls.linkTarget, "\""}) : "";
            body = linked(body, cls.link, cls.id, links, options, targetAttr);
        }
        std::string rawCls = cls.cssClasses;
        if (startsWith(rawCls, "default ")) rawCls = rawCls.substr(8);
        else if (rawCls == "default") rawCls = "";
        return Pele::cat({"<g class=\"pele-node pele-class", classNames(rawCls), "\" data-id=\"", id, "\"", at, ">", body, "</g>"});
    };

    std::string clusters;
    std::string struckTitles;
    std::string nodesOut;
    for (size_t i = 0; i < views.size(); i++) {
        const auto& view = views[i];
        const auto& c = cnodes[i];
        const auto& node = view.node;
        double x = c.x + ox;
        double y = c.y + oy;
        std::string id = escText(node.id);
        double w = c.isGroup ? c.w : view.w;
        double h = c.isGroup ? c.h : view.h;
        if (node.isGroup) {
            double titleX = c.isGroup ? clusterTitleX(c, view.label.width, crossings, GROUP_PAD) : c.x;
            double titleY = y - h / 2.0 + 8.0 + view.label.height / 2.0;
            std::string titleAttrs = " class=\"pele-cluster-label\" fill=\"var(--_m)\"";
            bool struck = c.isGroup && titleCrossed(c, view.label.width, titleX, crossings);
            if (struck) struckTitles += struckTitle(view.label, titleX + ox, titleY, titleAttrs, id, graph.nodes, i);
            Pele::appendCat(clusters, {"<g class=\"pele-cluster pele-namespace\" data-id=\"", id, "\">"
                "<rect x=\"", num(x - w / 2.0), "\" y=\"", num(y - h / 2.0), "\" width=\"", num(w), "\" height=\"", num(h), "\" fill=\"var(--_a)\" fill-opacity=\"0.5\" stroke=\"var(--_b)\"", ROUNDED, "/>", (struck ? "" : labelSvg(view.label, titleX + ox, titleY, titleAttrs)), "</g>"});
            continue;
        }
        std::string at = Pele::cat({" transform=\"translate(", num(x), ",", num(y), ")\""});
        if (node.classNode) {
            nodesOut += classSvg(view, *node.classNode, id, at);
        } else if (node.shape == "note") {
            double r = w / 2.0;
            double t = -h / 2.0;
            Pele::appendCat(nodesOut, {"<g class=\"pele-node pele-note\" data-id=\"", id, "\"", at, ">"
                "<path d=\"M", num(-r), ",", num(t), "H", num(r - FOLD), "L", num(r), ",", num(t + FOLD), "V", num(-t), "H", num(-r), "Z\" fill=\"var(--_a)\" stroke=\"var(--_b)\" stroke-linejoin=\"round\"/>"
                "<path d=\"M", num(r - FOLD), ",", num(t), "V", num(t + FOLD), "H", num(r), "\" fill=\"none\" stroke=\"var(--_b)\" stroke-linejoin=\"round\"/>", labelSvg(view.label,  0.0,  0.0, Pele::cat({" class=\"pele-label\" font-size=\"", num(small), "\""}),  options.icons), "</g>"});
        } else {
            Pele::appendCat(nodesOut, {"<g class=\"pele-node pele-interface\" data-id=\"", id, "\"", at, ">", labelSvg(view.label,  0.0,  0.0, Pele::cat({" class=\"pele-label\" font-size=\"", num(small), "\""}),  options.icons), "</g>"});
        }
    }

    const std::string LINE_COLOR = "var(--_l)";
    std::string edgesOut;
    std::string labelsOut;
    for (size_t i = 0; i < cedges.size(); i++) {
        auto& ce = cedges[i];
        const auto& edge = drawn[i];
        const auto& label = edgeLabels[i];
        const std::string& startType = edge.arrowTypeStart;
        const std::string& endType = edge.arrowTypeEnd;
        double startTrim = classMarkerTrim(startType);
        double endTrim = classMarkerTrim(endType);
        EdgePath path;
        if (ce.src == ce.dst) {
            const auto& c = cnodes[ce.src];
            const auto& view = views[ce.src];
            int k = loopAt[i];
            double x = c.x + ox + view.w / 2.0;
            double y = c.y + oy;
            double spread = std::min(view.h / 2.0 - 4.0, 8.0 + k * 6.0);
            double reach = LOOP + k * 8.0;
            path.d = Pele::cat({"M", num(x + startTrim), ",", num(y - spread), "C", num(x + reach + startTrim), ",", num(y - spread), " ", num(x + reach + endTrim), ",", num(y + spread), " ", num(x + endTrim), ",", num(y + spread)});
            path.sx = x;
            path.sy = y - spread;
            path.sdx = -1.0;
            path.sdy = 0.0;
            path.ex = x;
            path.ey = y + spread;
            path.edx = -1.0;
            path.edy = 0.0;
        } else {
            auto& route = ce.route;
            for (size_t k = 0; k + 1 < route.size(); k += 3) {
                route[k] += ox;
                route[k + 1] += oy;
            }
            path = routePath(route, "", startTrim, endTrim);
        }
        std::string attrs = (edge.pattern == "dashed") ? " stroke-dasharray=\"6 4\"" : (edge.pattern == "dotted") ? " stroke-dasharray=\"2 4\"" : "";
        edgesOut += "<g class=\"pele-edge" + std::string(edge.pattern == "dotted" ? " pele-note-edge" : " pele-relation") + "\" data-id=\"" + escText(edge.id) + "\">"
            "<path d=\"" + path.d + "\"" + attrs + "/>" +
            classMarker(startType, path.sx, path.sy, path.sdx, path.sdy, LINE_COLOR) +
            classMarker(endType, path.ex, path.ey, path.edx, path.edy, LINE_COLOR) +
            "</g>";
        if (label.width > 0.0) {
            double lx = ce.labelX + ox;
            double ly = ce.labelY + oy;
            labelsOut += edgeLabelSvg(edge.id, label, lx, ly, "", options.icons);
        }
    }

    std::string cardsOut;
    for (const auto& item : cards) {
        if (item.hasMath) {
            cardsOut += labelSvg(item.math, item.x + ox, item.y + oy, " class=\"pele-cardinality\"", options.icons);
        } else {
            Pele::appendCat(cardsOut, {"<text class=\"pele-cardinality\" x=\"", num(item.x + ox), "\" y=\"", num(item.y + oy + tiny * 0.35), "\" text-anchor=\"middle\">", esc(item.text), "</text>"});
        }
    }

    Titled titled = withTitle(
        db.title, 
        measurer, 
        size, 
        pad, 
        width, 
        height, Pele::cat({(!clusters.empty() ? Pele::cat({"<g class=\"pele-clusters\">", clusters, "</g>"}) : ""), (!edgesOut.empty() ? Pele::cat({"<g class=\"pele-edges\" fill=\"none\" stroke=\"", LINE_COLOR, "\" stroke-linecap=\"round\">", edgesOut, "</g>"}) : ""), (!struckTitles.empty() ? Pele::cat({"<g class=\"pele-cluster-titles\">", struckTitles, "</g>"}) : ""), (!labelsOut.empty() ? Pele::cat({"<g class=\"pele-edge-labels\" font-size=\"", num(small), "\">", labelsOut, "</g>"}) : ""), (!cardsOut.empty() ? Pele::cat({"<g class=\"pele-cardinalities\" font-size=\"", num(tiny), "\" fill=\"var(--_m)\">", cardsOut, "</g>"}) : ""), "<g class=\"pele-nodes\">", nodesOut, "</g>"}));
    std::string svg = svgDocument("class", titled.width, titled.height, size, options, db, titled.content);
    return {std::move(svg), titled.width, titled.height, std::move(links)};
}

} // namespace detail_render

// ============================================================================
// Public entry point
// ============================================================================

inline Rendered render(
    const std::string& source,
    const Config& config = {},
    const RenderOptions& options = {},
    const std::string& title = ""
) {
    ClassDb db;
    if (!title.empty()) db.title = title;
    detail_parse::parseClassDiagram(source, db);
    auto fitted = [&](bool down) -> Rendered {
        return tighten(options, [&](bool tight) {
            return detail_render::draw(db, config, options, down, tight);
        });
    };
    return runsAcross(db.direction) ? turnToFit(options, fitted) : fitted(false);
}

inline Rendered render(
    const std::string& source,
    const Config& config,
    const std::string& title,
    const RenderOptions& options = {}
) {
    return render(source, config, options, title);
}

} // namespace Pele::ClassDiagram

namespace Pele {

inline Rendered renderClass(
    const std::string& text,
    const Config& config = {},
    const RenderOptions& options = {},
    const std::string& title = ""
) {
    return ClassDiagram::render(text, config, options, title);
}

inline Rendered renderClass(
    const std::string& text,
    const Config& config,
    const std::string& title,
    const RenderOptions& options = {}
) {
    return ClassDiagram::render(text, config, options, title);
}

} // namespace Pele
