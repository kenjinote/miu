#pragma once
#include <array>
#include "../foundation.h"

namespace Pele {
namespace Flowchart {

inline double jsRound(double x) noexcept {
    return std::floor(x + 0.5);
}

inline bool objectIs(double a, double b) noexcept {
    if (std::isnan(a) && std::isnan(b)) return true;
    return a == b && std::signbit(a) == std::signbit(b);
}

// ============================================================================
// src/layout/layered.ts
// ============================================================================

enum class LKind : int {
    Node = 0,
    Dummy = 1,
    Label = 2,
    StartPort = 3,
    EndPort = 4
};

struct LNode {
    double w = 0.0;
    double h = 0.0;
    LKind kind = LKind::Node;
    int pin = 0;
    double hint = std::numeric_limits<double>::quiet_NaN();
    double key = std::numeric_limits<double>::quiet_NaN();
    double span = 0.0;
    double x = 0.0;
    double y = 0.0;
    int rank = 0;
};

struct LEdge {
    int tail = 0;
    int head = 0;
    int minlen = 1;
    double labelW = 0.0;
    double labelH = 0.0;
    double tailDx = 0.0;
    double headDx = 0.0;
    bool tailFixed = false;
    bool headFixed = false;
    bool reversed = false;
    std::vector<double> points;
    double labelX = 0.0;
    double labelY = 0.0;
};

struct LayeredOptions {
    double nodeSep = 40.0;
    double edgeSep = 16.0;
    double rankSep = 48.0;
    double portSep = 20.0;
    bool tracks = false;
    bool straight = false;
    bool apart = false;
    double headRoom = 0.0;
    bool hasGroups = false;
    std::vector<int> groupsOf;
    std::vector<int> groupsParent;
    bool orderOnly = false;
};

struct LayeredResult {
    double width = 0.0;
    double height = 0.0;
    int crossings = 0;
    int declared = 0;
};

inline LNode lnode(double w, double h, LKind kind = LKind::Node) noexcept {
    LNode n;
    n.w = w;
    n.h = h;
    n.kind = kind;
    return n;
}

inline LEdge ledge(int tail, int head, int minlen = 1, double labelW = 0.0, double labelH = 0.0) noexcept {
    LEdge e;
    e.tail = tail;
    e.head = head;
    e.minlen = minlen;
    e.labelW = labelW;
    e.labelH = labelH;
    return e;
}

namespace detail {

constexpr int MAX_SWEEPS = 24;
constexpr int MAX_DUMMIES = 20000;
constexpr double PORT_PITCH = 16.0;
constexpr double MIN_PITCH = 8.0;
constexpr double MIN_SLOPE = 0.125;
constexpr double MAX_GAP = 3.0;
constexpr double TRACK_MARGIN = 18.0;
constexpr double TRACK_PITCH = 10.0;
constexpr double TRACK_SPREAD = 14.0;
constexpr double LEVEL_WIDE = 2.0;
constexpr double LEVEL_MARGIN = 28.0;
constexpr double LEVEL_PITCH = 12.0;
constexpr double LEVEL_LEAD = 12.0;
constexpr double LEVEL_CLEAR = 8.0;
constexpr double LEVEL_TOUCH = 4.0;
constexpr double CROSSED_SLOPE = 0.5;
constexpr double IN_LINE = 0.5;
constexpr double SNAP = 12.0;
constexpr double LEAN_CLEAR = 40.0;
constexpr int MAX_WEIGHED = 400;
constexpr int SAMPLES = 16;

enum EndKind : uint8_t {
    EndHeld = 0,
    EndLeaves = 1,
    EndArrives = 2
};

inline bool isDummy(uint8_t kind) noexcept {
    return kind == static_cast<uint8_t>(LKind::Dummy) || kind == static_cast<uint8_t>(LKind::Label);
}

inline void sortRange(std::vector<int>& list, int from, int to, const std::vector<int>& key) noexcept {
    for (int i = from + 1; i < to; i++) {
        int item = list[i];
        int k = key[item];
        int j = i - 1;
        while (j >= from && (key[list[j]] > k || (key[list[j]] == k && list[j] > item))) {
            list[j + 1] = list[j];
            j--;
        }
        list[j + 1] = item;
    }
}

inline void sortByBary(std::vector<int>& list, int count, const std::vector<double>& bary, const std::vector<int>& pos, bool right, std::vector<int>& spare) noexcept {
    auto before = [&](int a, int b) -> bool {
        return bary[a] < bary[b] || (bary[a] == bary[b] && (right ? pos[a] > pos[b] : pos[a] < pos[b]));
    };
    const int RUN = 8;
    for (int s = 0; s < count; s += RUN) {
        for (int i = s + 1, end = std::min(count, s + RUN); i < end; i++) {
            int item = list[i];
            int j = i - 1;
            while (j >= s && before(item, list[j])) {
                list[j + 1] = list[j];
                j--;
            }
            list[j + 1] = item;
        }
    }
    std::vector<int>* from = &list;
    std::vector<int>* to = &spare;
    for (int width = RUN; width < count; width *= 2) {
        for (int s = 0; s < count; s += 2 * width) {
            int mid = std::min(count, s + width);
            int end = std::min(count, s + 2 * width);
            int i = s, j = mid, k = s;
            while (i < mid && j < end) (*to)[k++] = before((*from)[j], (*from)[i]) ? (*from)[j++] : (*from)[i++];
            while (i < mid) (*to)[k++] = (*from)[i++];
            while (j < end) (*to)[k++] = (*from)[j++];
        }
        std::swap(from, to);
    }
    if (from != &list) {
        for (int i = 0; i < count; i++) list[i] = (*from)[i];
    }
}

struct Pieces {
    std::vector<double> from;
    std::vector<double> to;
    std::vector<int> rank;
    std::vector<int> lower;
    std::vector<int> track;
    std::vector<int> levels;
    std::vector<int> wide;
};

struct Graph {
    int n = 0;
    int real = 0;
    int maxRank = 0;
    std::vector<double> W;
    std::vector<uint8_t> KIND;
    std::vector<int> RANK;
    std::vector<int> EF;
    std::vector<int> ET;
    std::vector<double> EFD;
    std::vector<double> ETD;
    std::vector<int> succStart;
    std::vector<int> succE;
    std::vector<int> predStart;
    std::vector<int> predE;
    std::vector<int> layerStart;
    std::vector<int> order;
    std::vector<int> pos;
    bool offsets = false;
    std::vector<uint8_t> fixed;
    bool hasGrp = false;
    std::vector<int> GRP;
    std::vector<int> groupParent;
};

inline double halfRoom(const Graph& g, const LayeredOptions& opt, int v) noexcept {
    return g.W[v] / 2.0 + (g.KIND[v] == static_cast<uint8_t>(LKind::Node) ? opt.nodeSep : opt.edgeSep) / 2.0;
}

} // namespace detail

inline double ease(double across, double along) noexcept {
    double den = std::abs(across);
    if (den == 0.0) den = 1.0;
    return std::max(0.25, std::min(0.5, (4.0 * std::abs(along)) / den));
}

inline void orient(std::vector<LNode>& nodes, std::vector<LEdge>& edges) noexcept {
    int n = static_cast<int>(nodes.size());
    int m = static_cast<int>(edges.size());
    std::vector<int> start(n + 1, 0);
    for (int i = 0; i < m; i++) {
        auto& e = edges[i];
        e.reversed = (nodes[e.head].kind == LKind::StartPort || nodes[e.tail].kind == LKind::EndPort);
        if (!e.reversed) start[e.tail + 1]++;
    }
    for (int i = 0; i < n; i++) start[i + 1] += start[i];
    std::vector<int> out(start[n], 0);
    {
        std::vector<int> fill(start.begin(), start.begin() + n);
        for (int i = 0; i < m; i++) if (!edges[i].reversed) out[fill[edges[i].tail]++] = i;
    }
    std::vector<uint8_t> state(n, 0);
    std::vector<int> cursor(start.begin(), start.begin() + n);
    std::vector<int> stack;
    for (int root = 0; root < n; root++) {
        if (state[root] != 0) continue;
        stack.push_back(root);
        state[root] = 1;
        while (!stack.empty()) {
            int v = stack.back();
            if (cursor[v] < start[v + 1]) {
                auto& e = edges[out[cursor[v]++]];
                int w = e.head;
                if (state[w] == 1) e.reversed = true;
                else if (state[w] == 0) {
                    state[w] = 1;
                    stack.push_back(w);
                }
            } else {
                state[v] = 2;
                stack.pop_back();
            }
        }
    }
}

inline void rank(std::vector<LNode>& nodes, std::vector<LEdge>& edges, int step) noexcept {
    int n = static_cast<int>(nodes.size());
    int m = static_cast<int>(edges.size());
    std::vector<int> from(m), to(m), len(m);
    std::vector<int> outStart(n + 1, 0), inStart(n + 1, 0);
    for (int i = 0; i < m; i++) {
        const auto& e = edges[i];
        int a = e.reversed ? e.head : e.tail;
        int b = e.reversed ? e.tail : e.head;
        from[i] = a;
        to[i] = b;
        len[i] = std::max(1, e.minlen) * step;
        outStart[a + 1]++;
        inStart[b + 1]++;
    }
    for (int i = 0; i < n; i++) {
        outStart[i + 1] += outStart[i];
        inStart[i + 1] += inStart[i];
    }
    std::vector<int> outE(m), inE(m);
    {
        std::vector<int> of(outStart.begin(), outStart.begin() + n);
        std::vector<int> inf(inStart.begin(), inStart.begin() + n);
        for (int i = 0; i < m; i++) {
            outE[of[from[i]]++] = i;
            inE[inf[to[i]]++] = i;
        }
    }
    std::vector<int> r(n, 0), indeg(n, 0), topo(n, 0);
    for (int i = 0; i < n; i++) indeg[i] = inStart[i + 1] - inStart[i];
    int count = 0;
    for (int i = 0; i < n; i++) if (indeg[i] == 0) topo[count++] = i;
    for (int q = 0; q < count; q++) {
        int v = topo[q];
        for (int k = outStart[v]; k < outStart[v + 1]; k++) {
            int ei = outE[k];
            int w = to[ei];
            if (r[v] + len[ei] > r[w]) r[w] = r[v] + len[ei];
            if (--indeg[w] == 0) topo[count++] = w;
        }
    }
    for (int pass = 0; pass < 8; pass++) {
        bool changed = false;
        for (int q = count - 1; q >= 0; q--) {
            int v = topo[q];
            int ins = inStart[v + 1] - inStart[v];
            int outs = outStart[v + 1] - outStart[v];
            if (ins == 0 && outs == 0) continue;
            int lo = std::numeric_limits<int>::min() / 2;
            int hi = std::numeric_limits<int>::max() / 2;
            for (int k = inStart[v]; k < inStart[v + 1]; k++) {
                int ei = inE[k];
                lo = std::max(lo, r[from[ei]] + len[ei]);
            }
            for (int k = outStart[v]; k < outStart[v + 1]; k++) {
                int ei = outE[k];
                hi = std::min(hi, r[to[ei]] - len[ei]);
            }
            int next = r[v];
            if (outs > ins && hi != std::numeric_limits<int>::max() / 2) next = hi;
            else if (ins > outs && lo != std::numeric_limits<int>::min() / 2) next = lo;
            if (next != r[v]) {
                r[v] = next;
                changed = true;
            }
        }
        if (!changed) break;
    }
    int minR = std::numeric_limits<int>::max();
    int maxR = std::numeric_limits<int>::min();
    bool startPorts = false;
    for (int i = 0; i < n; i++) {
        LKind kind = nodes[i].kind;
        if (kind == LKind::StartPort) startPorts = true;
        if (kind == LKind::StartPort || kind == LKind::EndPort) continue;
        if (r[i] < minR) minR = r[i];
        if (r[i] > maxR) maxR = r[i];
    }
    if (minR == std::numeric_limits<int>::max()) {
        minR = 0;
        maxR = 0;
    }
    for (int i = 0; i < n; i++) {
        if (nodes[i].kind == LKind::StartPort) r[i] = minR - step;
        else if (nodes[i].kind == LKind::EndPort) r[i] = maxR + step;
    }
    if (startPorts) minR -= step;
    for (int i = 0; i < n; i++) nodes[i].rank = r[i] - minR;
}

namespace detail {

inline double sideEnds(const Graph& g, int v, int side, double span, const std::vector<uint8_t>& kinds, std::vector<int>& ids, std::vector<double>& room) noexcept {
    const auto& start = side == 0 ? g.predStart : g.succStart;
    const auto& list = side == 0 ? g.predE : g.succE;
    const auto& other = side == 0 ? g.EF : g.ET;
    const auto& pos = g.pos;
    for (int k = start[v]; k < start[v + 1]; k++) if (kinds[list[k]] != EndHeld) ids.push_back(list[k]);
    if (ids.size() < 2) return 0.0;
    std::sort(ids.begin(), ids.end(), [&](int a, int b) {
        if (pos[other[a]] != pos[other[b]]) return pos[other[a]] < pos[other[b]];
        return a < b;
    });
    room.resize(ids.size(), 0.0);
    auto fill = [&](auto&& share) -> double {
        double total = 0.0;
        room[0] = 0.0;
        for (size_t k = 1; k < ids.size(); k++) {
            bool same = kinds[ids[k]] == kinds[ids[k - 1]];
            room[k] = share(same && kinds[ids[k]] == EndLeaves, same);
            total += room[k];
        }
        return total;
    };
    double total = fill([](bool, bool) { return 1.0; });
    if (span / total < MIN_PITCH) total = fill([](bool leaves, bool) { return leaves ? 0.5 : 1.0; });
    if (span / total < MIN_PITCH) total = fill([](bool, bool same) { return same ? 0.0 : 1.0; });
    if (total == 0.0) return 0.0;
    double pitch = std::min(PORT_PITCH, span / total);
    for (size_t k = 1; k < ids.size(); k++) room[k] *= pitch;
    return total * pitch;
}

inline void spread(Graph& g, const std::vector<LNode>& nodes, const std::vector<uint8_t>& EFK, const std::vector<uint8_t>& ETK) noexcept {
    for (int v = 0; v < g.real; v++) {
        double span = std::min(nodes[v].span, g.W[v]);
        if (span <= 0.0 || nodes[v].kind != LKind::Node) continue;
        for (int side = 0; side < 2; side++) {
            std::vector<int> ids;
            std::vector<double> room;
            double total = sideEnds(g, v, side, span, side == 0 ? ETK : EFK, ids, room);
            if (total == 0.0) continue;
            auto& dx = side == 0 ? g.ETD : g.EFD;
            double at = -total / 2.0;
            for (size_t k = 0; k < ids.size(); k++) {
                at += room[k];
                dx[ids[k]] = at;
            }
        }
    }
}

inline int follow(const Graph& g, const std::vector<uint8_t>* empty, int e, int side) noexcept {
    if (!empty) return e;
    for (int hops = 0; hops < 100000; hops++) {
        int u = side == 0 ? g.EF[e] : g.ET[e];
        if (g.KIND[u] != static_cast<uint8_t>(LKind::Dummy) || !(*empty)[g.RANK[u]]) break;
        e = side == 0 ? g.predE[g.predStart[u]] : g.succE[g.succStart[u]];
    }
    return e;
}

inline std::pair<std::vector<int>, std::vector<int>> freeEnds(const Graph& g, const std::vector<uint8_t>& EFK, const std::vector<uint8_t>& ETK) noexcept {
    std::pair<std::vector<int>, std::vector<int>> free{std::vector<int>(g.n, 0), std::vector<int>(g.n, 0)};
    for (size_t e = 0; e < g.EF.size(); e++) {
        if (ETK[e] != EndHeld) free.first[g.ET[e]]++;
        if (EFK[e] != EndHeld) free.second[g.EF[e]]++;
    }
    return free;
}

inline void alignLabels(const Graph& g, const std::vector<LNode>& nodes, std::vector<double>& xs, const std::pair<std::vector<int>, std::vector<int>>& free, const LayeredOptions& opt, const std::vector<uint8_t>* empty) noexcept {
    double sep = opt.edgeSep;
    auto placed = [&](int v) -> bool {
        return g.KIND[v] == static_cast<uint8_t>(LKind::Label) || (g.KIND[v] == static_cast<uint8_t>(LKind::Dummy) && !(empty && (*empty)[g.RANK[v]] == 1));
    };
    auto end = [&](int e, int side) -> std::pair<double, int> {
        int u = side == 0 ? g.EF[e] : g.ET[e];
        double x = side == 0 ? xs[u] + g.EFD[e] : xs[u] + g.ETD[e];
        int f = (u < g.real && nodes[u].kind == LKind::Node) ? (1 - side == 0 ? free.first[u] : free.second[u]) : 0;
        return {x, f};
    };
    auto room = [&](int v) -> std::pair<double, double> {
        int s = g.layerStart[g.RANK[v]];
        int i = s + g.pos[v];
        auto clear = [&](int other) -> double {
            return g.W[other] / 2.0 + g.W[v] / 2.0 + (g.KIND[other] == static_cast<uint8_t>(LKind::Node) ? opt.nodeSep / 2.0 + sep / 2.0 : g.KIND[other] == g.KIND[v] ? sep : sep / 2.0);
        };
        double lo = i > s ? xs[g.order[i - 1]] + clear(g.order[i - 1]) : -std::numeric_limits<double>::infinity();
        double hi = i + 1 < g.layerStart[g.RANK[v] + 1] ? xs[g.order[i + 1]] - clear(g.order[i + 1]) : std::numeric_limits<double>::infinity();
        return {lo, hi};
    };
    auto choose = [&](double x, std::pair<double, int> up, std::pair<double, int> down, double lo, double hi) -> double {
        bool upFirst = up.second != down.second ? up.second < down.second : std::abs(up.first - x) <= std::abs(down.first - x);
        double c0 = upFirst ? up.first : down.first;
        double c1 = upFirst ? down.first : up.first;
        if (c0 >= lo && c0 <= hi) return c0;
        if (c1 >= lo && c1 <= hi) return c1;
        return std::max(lo, std::min(hi, c0));
    };
    std::vector<uint8_t> done(g.n, 0);
    std::vector<int> chain;
    for (int r = 1; r < g.maxRank; r++) {
        for (int i = g.layerStart[r]; i < g.layerStart[r + 1]; i++) {
            if (!placed(g.order[i]) || done[g.order[i]]) continue;
            int top = g.order[i];
            while (isDummy(g.KIND[g.EF[g.predE[g.predStart[top]]]])) top = g.EF[g.predE[g.predStart[top]]];
            int bottom = top;
            double lo = -std::numeric_limits<double>::infinity();
            double hi = std::numeric_limits<double>::infinity();
            chain.clear();
            for (int d = top; isDummy(g.KIND[d]); d = g.ET[g.succE[g.succStart[d]]]) {
                bottom = d;
                if (!placed(d)) continue;
                done[d] = 1;
                chain.push_back(d);
                auto [least, most] = room(d);
                if (least > lo) lo = least;
                if (most < hi) hi = most;
            }
            if (lo <= hi) {
                double x = choose(xs[chain[0]], end(g.predE[g.predStart[top]], 0), end(g.succE[g.succStart[bottom]], 1), lo, hi);
                for (int d : chain) xs[d] = x;
                continue;
            }
            for (int d : chain) {
                auto [least, most] = room(d);
                if (least > most) continue;
                xs[d] = choose(xs[d], end(follow(g, empty, g.predE[g.predStart[d]], 0), 0), end(follow(g, empty, g.succE[g.succStart[d]], 1), 1), least, most);
            }
        }
    }
}

inline void settle(Graph& g, const std::vector<LNode>& nodes, const std::vector<double>& xs, const std::vector<uint8_t>& EFK, const std::vector<uint8_t>& ETK, const std::pair<std::vector<int>, std::vector<int>>& free, const std::vector<uint8_t>* empty) noexcept {
    auto wanted = [&](int e, int side, int v, double span, bool again) -> double {
        int fv = side == 0 ? free.first[v] : free.second[v];
        if (fv < 2) return 0.0;
        e = follow(g, empty, e, side);
        int u = side == 0 ? g.EF[e] : g.ET[e];
        double point = side == 0 ? xs[u] + g.EFD[e] : xs[u] + g.ETD[e];
        if (again && std::abs(point - xs[v]) <= span / 2.0 + IN_LINE) return std::max(-span / 2.0, std::min(span / 2.0, point - xs[v]));
        double middle = point;
        double width = 0.0;
        int fu = (1 - side) == 0 ? free.first[u] : free.second[u];
        if (u < g.real && nodes[u].kind == LKind::Node && (side == 0 ? EFK : ETK)[e] != EndHeld && fu >= 2) {
            width = std::max(0.0, std::min(nodes[u].span, g.W[u]));
            if (width > 0.0) middle = xs[u];
        }
        double lo = std::max(xs[v] - span / 2.0, middle - width / 2.0);
        double hi = std::min(xs[v] + span / 2.0, middle + width / 2.0);
        if (lo > hi + IN_LINE) {
            double lean = std::max(0.0, span / 2.0 - LEAN_CLEAR);
            return std::max(-lean, std::min(lean, point - xs[v]));
        }
        double place = (xs[v] * width + middle * span) / (width + span);
        return std::max(lo, std::min(hi, place)) - xs[v];
    };
    struct SideInfo {
        int v;
        int side;
        double span;
        std::vector<int> ids;
        std::vector<double> room;
        std::vector<int> all;
    };
    std::vector<SideInfo> sides;
    for (int v = 0; v < g.real; v++) {
        double span = std::min(nodes[v].span, g.W[v]);
        if (span <= 0.0 || nodes[v].kind != LKind::Node) continue;
        for (int side = 0; side < 2; side++) {
            const auto& other = side == 0 ? g.EF : g.ET;
            const auto& start = side == 0 ? g.predStart : g.succStart;
            const auto& list = side == 0 ? g.predE : g.succE;
            std::vector<int> ids;
            std::vector<double> room;
            sideEnds(g, v, side, span, side == 0 ? ETK : EFK, ids, room);
            if (ids.empty()) continue;
            if (ids.size() == 1) room.assign(1, 0.0);
            std::vector<int> all;
            for (int k = start[v]; k < start[v + 1]; k++) all.push_back(list[k]);
            std::sort(all.begin(), all.end(), [&](int a, int b) {
                if (g.pos[other[a]] != g.pos[other[b]]) return g.pos[other[a]] < g.pos[other[b]];
                return a < b;
            });
            sides.push_back({v, side, span, std::move(ids), std::move(room), std::move(all)});
        }
    }
    bool moved = true;
    for (int pass = 0; moved && pass < 6; pass++) {
        moved = false;
        for (const auto& s : sides) {
            const auto& kinds = s.side == 0 ? ETK : EFK;
            auto& dx = s.side == 0 ? g.ETD : g.EFD;
            size_t k = 0;
            double lo = -s.span / 2.0;
            for (size_t i = 0; i <= s.all.size(); i++) {
                bool held = i < s.all.size() && kinds[s.all[i]] == EndHeld;
                if (i < s.all.size() && !held) continue;
                size_t size = 0;
                for (int j = static_cast<int>(i) - 1; j >= 0 && kinds[s.all[j]] != EndHeld; j--) size++;
                double hi = held ? dx[s.all[i]] - PORT_PITCH : s.span / 2.0;
                if (size > 0) {
                    std::vector<double> sum;
                    std::vector<int> count;
                    double before = 0.0;
                    for (size_t j = 0; j < size; j++) {
                        if (j > 0) before += s.room[k + j];
                        sum.push_back(wanted(s.ids[k + j], s.side, s.v, s.span, pass > 0) - before);
                        count.push_back(1);
                        bool together = j > 0 && s.room[k + j] == 0.0;
                        while (sum.size() > 1 && (together || sum[sum.size() - 2] / count[count.size() - 2] > sum.back() / count.back())) {
                            together = false;
                            double top = sum.back();
                            int pooled = count.back();
                            sum.pop_back();
                            count.pop_back();
                            sum.back() += top;
                            count.back() += pooled;
                        }
                    }
                    double total = before;
                    size_t j = 0;
                    before = 0.0;
                    for (size_t b = 0; b < sum.size(); b++) {
                        double level = hi - total < lo ? (lo + hi - total) / 2.0 : std::max(lo, std::min(hi - total, sum[b] / count[b]));
                        for (int c = 0; c < count[b]; c++, j++) {
                            if (j > 0) before += s.room[k + j];
                            if (std::abs(dx[s.ids[k + j]] - level - before) > 0.01) moved = true;
                            dx[s.ids[k + j]] = level + before;
                        }
                    }
                    k += size;
                }
                if (held) lo = dx[s.all[i]] + PORT_PITCH;
            }
        }
    }
}

inline void snap(const Graph& g, std::vector<double>& xs, const std::vector<uint8_t>* empty, const LayeredOptions& opt) noexcept {
    auto passed = [&](int u) -> bool {
        return empty && g.KIND[u] == static_cast<uint8_t>(LKind::Dummy) && (*empty)[g.RANK[u]] == 1;
    };
    std::vector<int> a, b;
    std::vector<double> da, db;
    for (int e = 0; e < static_cast<int>(g.EF.size()); e++) {
        if (passed(g.EF[e])) continue;
        int f = follow(g, empty, e, 1);
        a.push_back(g.EF[e]);
        b.push_back(g.ET[f]);
        da.push_back(g.EFD[e]);
        db.push_back(g.ETD[f]);
    }
    auto off = [&](int k) -> double {
        return xs[b[k]] + db[k] - xs[a[k]] - da[k];
    };
    std::vector<int> block(g.n);
    std::vector<std::vector<int>> members(g.n);
    for (int i = 0; i < g.n; i++) {
        block[i] = i;
        members[i] = {i};
    }
    auto join = [&](int x, int y) {
        int keep = block[x];
        int drop = block[y];
        if (keep == drop) return;
        if (members[keep].size() < members[drop].size()) std::swap(keep, drop);
        for (int v : members[drop]) {
            block[v] = keep;
            members[keep].push_back(v);
        }
        members[drop].clear();
    };
    std::vector<int> near;
    for (int k = 0; k < static_cast<int>(a.size()); k++) {
        double d = std::abs(off(k));
        if (d < 0.01) join(a[k], b[k]);
        else if (d <= SNAP) near.push_back(k);
    }
    if (near.empty()) return;
    std::sort(near.begin(), near.end(), [&](int x, int y) {
        double dx = std::abs(off(x));
        double dy = std::abs(off(y));
        if (dx != dy) return dx < dy;
        return x < y;
    });
    auto gap = [&](int v) -> double { return halfRoom(g, opt, v); };
    auto freeBlock = [&](int id, double shift) -> bool {
        for (int v : members[id]) {
            int s = g.layerStart[g.RANK[v]];
            int end = g.layerStart[g.RANK[v] + 1];
            int i = s + g.pos[v];
            if (i > s && block[g.order[i - 1]] != id && xs[v] + shift - xs[g.order[i - 1]] < gap(v) + gap(g.order[i - 1]) - 0.01) return false;
            if (i + 1 < end && block[g.order[i + 1]] != id && xs[g.order[i + 1]] - xs[v] - shift < gap(v) + gap(g.order[i + 1]) - 0.01) return false;
        }
        return true;
    };
    for (int k : near) {
        double d = off(k);
        if (std::abs(d) < 0.01) {
            join(a[k], b[k]);
            continue;
        }
        if (std::abs(d) > SNAP || block[a[k]] == block[b[k]]) continue;
        int upper = block[a[k]];
        int lower = block[b[k]];
        int first = members[upper].size() <= members[lower].size() ? upper : lower;
        int candidates[2] = {first, first == upper ? lower : upper};
        for (int id : candidates) {
            double shift = id == upper ? d : -d;
            if (!freeBlock(id, shift)) continue;
            for (int v : members[id]) xs[v] += shift;
            join(a[k], b[k]);
            break;
        }
    }
}

inline bool emptyRanks(const Graph& g, std::vector<uint8_t>& empty) noexcept {
    empty.assign(g.maxRank + 1, 0);
    bool any = false;
    for (int r = 1; r < g.maxRank; r++) {
        bool only = g.layerStart[r + 1] > g.layerStart[r];
        for (int i = g.layerStart[r]; only && i < g.layerStart[r + 1]; i++) {
            if (g.KIND[g.order[i]] != static_cast<uint8_t>(LKind::Dummy)) only = false;
        }
        if (only) {
            empty[r] = 1;
            any = true;
        }
    }
    return any;
}

inline void assignTracks(Pieces& pieces, std::vector<int>& trackCount, bool curved, const std::vector<double>& room, std::vector<double>& tall) noexcept {
    auto& from = pieces.from;
    auto& to = pieces.to;
    auto& rank = pieces.rank;
    auto& track = pieces.track;
    auto& levels = pieces.levels;
    auto& wide = pieces.wide;
    int total = static_cast<int>(from.size());
    std::vector<int> rankKeys;
    std::unordered_map<int, std::vector<int>> byRank;
    for (int k = 0; k < total; k++) {
        track.push_back(-1);
        levels.push_back(0);
        if (std::abs(from[k] - to[k]) < IN_LINE) continue;
        auto it = byRank.find(rank[k]);
        if (it != byRank.end()) it->second.push_back(k);
        else {
            rankKeys.push_back(rank[k]);
            byRank[rank[k]] = {k};
        }
    }
    if (byRank.empty()) return;
    std::vector<double> LO(total), HI(total);
    for (int k = 0; k < total; k++) {
        LO[k] = std::min(from[k], to[k]);
        HI[k] = std::max(from[k], to[k]);
    }
    auto within = [&](double x, int k) -> int {
        return (x >= LO[k] - IN_LINE && x <= HI[k] + IN_LINE) ? 1 : 0;
    };
    auto mates = [&](int a, int b) -> bool {
        return std::abs(from[a] - from[b]) < IN_LINE || std::abs(to[a] - to[b]) < IN_LINE;
    };
    auto overlap = [&](int a, int b) -> bool {
        return LO[a] < HI[b] - IN_LINE && LO[b] < HI[a] - IN_LINE;
    };
    auto clash = [&](int a, int b) -> bool {
        return overlap(a, b) && !mates(a, b);
    };
    auto meet = [&](int a, int b) -> bool {
        return overlap(a, b) || within(to[a], b) + within(from[b], a) + within(to[b], a) + within(from[a], b) > 0;
    };
    auto crossings = [&](int a, int b) -> int {
        return within(to[a], b) + within(from[b], a);
    };
    auto nearPairs = [&](const std::vector<int>& list) -> std::vector<int> {
        std::vector<int> byLo(list.size());
        for (size_t i = 0; i < list.size(); i++) byLo[i] = static_cast<int>(i);
        std::stable_sort(byLo.begin(), byLo.end(), [&](int a, int b) {
            return LO[list[a]] < LO[list[b]];
        });
        std::vector<int> pairs;
        for (size_t x = 0; x < byLo.size(); x++) {
            int i = byLo[x];
            double reach = HI[list[i]] + 4.0 * IN_LINE;
            for (size_t y = x + 1; y < byLo.size() && LO[list[byLo[y]]] <= reach; y++) {
                int j = byLo[y];
                pairs.push_back(std::min(i, j));
                pairs.push_back(std::max(i, j));
            }
        }
        return pairs;
    };
    auto lay = [&](std::vector<int>& list) -> int {
        std::sort(list.begin(), list.end(), [&](int a, int b) {
            double da = HI[a] - LO[a];
            double db = HI[b] - LO[b];
            if (db != da) return db > da;
            if (LO[a] != LO[b]) return LO[a] < LO[b];
            return a < b;
        });
        int count = static_cast<int>(list.size());
        std::vector<std::vector<int>> above;
        std::vector<int> group(count);
        for (int i = 0; i < count; i++) group[i] = i;
        auto find = [&](int i) -> int {
            while (group[i] != i) {
                group[i] = group[group[i]];
                i = group[i];
            }
            return i;
        };
        bool weighed = count <= MAX_WEIGHED;
        std::vector<int> pairs = weighed ? nearPairs(list) : std::vector<int>{};
        std::vector<std::vector<int>> clashes(weighed ? count : 0);
        for (size_t p = 0; p < pairs.size(); p += 2) {
            int i = pairs[p];
            int j = pairs[p + 1];
            if (!clash(list[i], list[j])) continue;
            clashes[i].push_back(j);
            clashes[j].push_back(i);
        }
        std::vector<int> sequence(count);
        for (int i = 0; i < count; i++) sequence[i] = i;
        if (weighed) {
            std::vector<int> waiting(count, 0);
            std::vector<std::vector<int>> after(count);
            above.resize(count);
            for (size_t p = 0; p < pairs.size(); p += 2) {
                int i = pairs[p];
                int j = pairs[p + 1];
                int a = list[i];
                int b = list[j];
                if (!meet(a, b)) continue;
                group[find(i)] = find(j);
                int ab = crossings(a, b);
                int ba = crossings(b, a);
                if (ab == ba) continue;
                int first = ab < ba ? i : j;
                int second = ab < ba ? j : i;
                above[second].push_back(first);
                after[first].push_back(second);
                waiting[second]++;
            }
            std::vector<uint8_t> placed(count, 0);
            sequence.clear();
            for (int done = 0; done < count; done++) {
                int pick = -1;
                for (int i = 0; i < count; i++) {
                    if (placed[i]) continue;
                    if (pick < 0 || waiting[i] < waiting[pick]) pick = i;
                    if (waiting[i] == 0) break;
                }
                placed[pick] = 1;
                sequence.push_back(pick);
                for (int j : after[pick]) waiting[j]--;
            }
        }
        Pele::FlatSet<int> taken;
        std::vector<std::vector<int>> onTrack;
        int rows = 0;
        for (int at : sequence) {
            int k = list[at];
            int t = 0;
            if (at < static_cast<int>(above.size())) {
                for (int i : above[at]) if (track[list[i]] >= t) t = track[list[i]] + 1;
            }
            if (weighed) {
                taken.clear();
                for (int i : clashes[at]) taken.insert(track[list[i]]);
                while (taken.count(t)) t++;
            } else {
                while (t < static_cast<int>(onTrack.size())) {
                    bool anyClash = false;
                    for (int other : onTrack[t]) if (clash(k, other)) { anyClash = true; break; }
                    if (!anyClash) break;
                    t++;
                }
                if (t >= static_cast<int>(onTrack.size())) onTrack.resize(t + 1);
                onTrack[t].push_back(k);
            }
            track[k] = t;
            if (t >= rows) rows = t + 1;
        }
        std::vector<int> first(count, rows);
        std::vector<int> last(count, -1);
        for (int i = 0; i < count; i++) {
            int root = find(i);
            first[root] = std::min(first[root], track[list[i]]);
            last[root] = std::max(last[root], track[list[i]]);
        }
        for (int i = 0; i < count; i++) {
            int root = find(i);
            levels[list[i]] = last[root] - first[root] + 1;
            if (curved) track[list[i]] -= first[root];
        }
        return rows;
    };

    double lineA[2 * SAMPLES + 2];
    double lineB[2 * SAMPLES + 2];
    auto sample = [&](int k, double h, double* out) {
        double c = ease(to[k] - from[k], h);
        for (int i = 0; i <= SAMPLES; i++) {
            double t = static_cast<double>(i) / SAMPLES;
            double u = 1.0 - t;
            out[2 * i] = from[k] + (to[k] - from[k]) * t * t * (3.0 - 2.0 * t);
            out[2 * i + 1] = h * (3.0 * c * t * u * u + 3.0 * (1.0 - c) * t * t * u + t * t * t);
        }
    };
    auto reaches = [&](const double* points, const double* line, double limit) -> bool {
        double bound = limit * limit * (1.0 + 1e-9);
        for (int i = 0; i <= SAMPLES; i++) {
            double px = points[2 * i];
            double py = points[2 * i + 1];
            for (int j = 0; j < SAMPLES; j++) {
                double dx = line[2 * j + 2] - line[2 * j];
                double dy = line[2 * j + 3] - line[2 * j + 1];
                double den = dx * dx + dy * dy;
                if (den == 0.0) den = 1.0;
                double t = std::max(0.0, std::min(1.0, ((px - line[2 * j]) * dx + (py - line[2 * j + 1]) * dy) / den));
                double ex = px - line[2 * j] - t * dx;
                double ey = py - line[2 * j + 1] - t * dy;
                if (ex * ex + ey * ey < bound && std::hypot(ex, ey) < limit) return true;
            }
        }
        return false;
    };
    auto near = [&](int a, int b, double height, double limit) -> bool {
        double h = std::max(height / 2.0, height - 2.0 * LEVEL_LEAD);
        sample(a, h, lineA);
        sample(b, h, lineB);
        return reaches(lineA, lineB, limit) || reaches(lineB, lineA, limit);
    };

    for (int r : rankKeys) {
        auto& all = byRank[r];
        if (!curved) {
            trackCount[r] = lay(all);
            continue;
        }
        if (static_cast<int>(all.size()) > MAX_WEIGHED) continue;
        std::vector<int> far;
        for (int k : all) if (wide[k] == 1) far.push_back(k);
        lay(far);
        Pele::FlatSet<int> level;
        for (int k : far) if (levels[k] > 1) level.insert(k);
        std::vector<int> pairs = nearPairs(all);
        for (size_t p = 0; p < pairs.size(); p += 2) {
            int a = all[pairs[p]];
            int b = all[pairs[p + 1]];
            if ((level.count(a) && level.count(b)) || !clash(a, b) || (from[a] < to[a]) != (from[b] < to[b])) continue;
            if (near(a, b, room[r], wide[a] == 1 || wide[b] == 1 ? LEVEL_CLEAR : LEVEL_TOUCH)) {
                level.insert(a);
                level.insert(b);
            }
        }
        for (int k : far) {
            track[k] = -1;
            levels[k] = 0;
        }
        if (level.empty()) continue;
        int crossing = 0;
        int alongside = 0;
        Pele::FlatSet<int> beside;
        for (size_t p = 0; p < pairs.size(); p += 2) {
            int a = all[pairs[p]];
            int b = all[pairs[p + 1]];
            if (level.count(a) && level.count(b)) {
                if (!overlap(a, b)) continue;
                if ((from[a] < to[a]) != (from[b] < to[b])) crossing++;
                else if (!mates(a, b)) alongside++;
            } else if ((level.count(a) || level.count(b)) && meet(a, b)) {
                beside.insert(a);
                beside.insert(b);
            }
        }
        if (crossing > alongside) {
            for (int k : all) if ((HI[k] - LO[k]) * CROSSED_SLOPE > tall[r]) tall[r] = (HI[k] - LO[k]) * CROSSED_SLOPE;
            continue;
        }
        std::vector<int> members;
        for (int k : all) if (level.count(k) || (wide[k] != 1 && beside.count(k))) members.push_back(k);
        lay(members);
        int most = 0;
        for (int k : members) if (levels[k] > most) most = levels[k];
        trackCount[r] = most;
    }
}

inline void straighten(const Graph& g, std::vector<double>& xs, const std::vector<uint8_t>& empty, const std::vector<int>& firstDummy, const std::vector<int>& span, double sep, std::vector<uint8_t>& onLine) noexcept {
    std::vector<double> want(xs.size(), 0.0);
    std::vector<double> from(xs.size(), 0.0);
    std::vector<double> to(xs.size(), 0.0);
    for (size_t ei = 0; ei < firstDummy.size(); ei++) {
        int d0 = firstDummy[ei];
        if (d0 < 0) continue;
        int end = d0 + span[ei] - 1;
        int base = g.RANK[d0];
        for (int d = d0; d < end; d++) {
            if (!empty[base + d - d0]) continue;
            int last = d;
            while (last + 1 < end && empty[base + last + 1 - d0]) last++;
            int above = g.predE[g.predStart[d]];
            int below = g.succE[g.succStart[last]];
            double x0 = xs[g.EF[above]] + g.EFD[above];
            double x1 = xs[g.ET[below]] + g.ETD[below];
            int steps = last - d + 2;
            for (int k = d; k <= last; k++) {
                want[k] = x0 + ((x1 - x0) * (k - d + 1)) / steps;
                from[k] = x0;
                to[k] = x1;
            }
            d = last;
        }
    }
    std::vector<int> twins;
    for (int r = 1; r < g.maxRank; r++) {
        if (!empty[r]) continue;
        int s = g.layerStart[r];
        int end = g.layerStart[r + 1];
        std::vector<int> row(g.order.begin() + s, g.order.begin() + end);
        std::sort(row.begin(), row.end(), [&](int a, int b) {
            if (from[a] != from[b]) return from[a] < from[b];
            if (to[a] != to[b]) return to[a] < to[b];
            return a < b;
        });
        for (size_t i = 0; i < row.size(); ) {
            twins.clear();
            size_t j = i;
            for (; j < row.size() && std::abs(from[row[j]] - from[row[i]]) < 1.0 && std::abs(to[row[j]] - to[row[i]]) < 1.0; j++) twins.push_back(row[j]);
            std::sort(twins.begin(), twins.end(), [&](int a, int b) {
                return g.pos[a] < g.pos[b];
            });
            for (size_t k = 0; k < twins.size(); k++) {
                int d = twins[k];
                xs[d] = want[d] + (static_cast<double>(k) - static_cast<double>(twins.size() - 1) / 2.0) * sep;
                if (twins.size() == 1 || sep == 0.0) onLine[d] = 1;
            }
            i = j;
        }
    }
}

inline void pastLabels(const Graph& g, std::vector<double>& xs, const std::vector<int>& firstDummy, const std::vector<int>& span, double clear, std::vector<uint8_t>& onLine) noexcept {
    std::vector<uint8_t> labelled(g.maxRank + 1, 0);
    bool any = false;
    for (int r = 1; r < g.maxRank; r++) {
        bool labels = false;
        bool others = false;
        for (int i = g.layerStart[r]; i < g.layerStart[r + 1]; i++) {
            uint8_t kind = g.KIND[g.order[i]];
            if (kind == static_cast<uint8_t>(LKind::Label)) labels = true;
            else if (kind != static_cast<uint8_t>(LKind::Dummy)) others = true;
        }
        if (labels && !others) {
            labelled[r] = 1;
            any = true;
        }
    }
    if (!any) return;
    for (size_t ei = 0; ei < firstDummy.size(); ei++) {
        int d0 = firstDummy[ei];
        if (d0 < 0) continue;
        for (int d = d0, end = d0 + span[ei] - 1; d < end; d++) {
            if (g.KIND[d] != static_cast<uint8_t>(LKind::Dummy) || !labelled[g.RANK[d]]) continue;
            int first = d;
            int last = d;
            while (first > d0 && onLine[first - 1]) first--;
            while (last + 1 < end && onLine[last + 1]) last++;
            int above = g.predE[g.predStart[first]];
            int below = g.succE[g.succStart[last]];
            double from = xs[g.EF[above]] + g.EFD[above];
            double to = xs[g.ET[below]] + g.ETD[below];
            double lo = std::min(from, to) - clear;
            double hi = std::max(from, to) + clear;
            bool free = true;
            int r = g.RANK[d];
            for (int i = g.layerStart[r]; free && i < g.layerStart[r + 1]; i++) {
                int label = g.order[i];
                if (g.KIND[label] == static_cast<uint8_t>(LKind::Label) && xs[label] + g.W[label] / 2.0 > lo && xs[label] - g.W[label] / 2.0 < hi) free = false;
            }
            if (!free) continue;
            onLine[d] = 1;
            int steps = last - first + 2;
            for (int k = first; k <= last; k++) xs[k] = from + ((to - from) * (k - first + 1)) / steps;
        }
    }
}

inline std::pair<int, int> reduceCrossings(Graph& g, const std::vector<LNode>& nodes) noexcept {
    int widest = 0;
    for (int r = 0; r <= g.maxRank; r++) widest = std::max(widest, g.layerStart[r + 1] - g.layerStart[r]);
    if (widest < 2) return {0, 0};
    bool hasPlace = g.offsets;
    std::vector<int> place(hasPlace ? g.EF.size() : 0, 0);
    std::vector<int> placeIn(hasPlace ? g.EF.size() : 0, 0);
    std::vector<int> places(hasPlace ? g.n : 0, 0);
    std::vector<int> leaving = hasPlace ? g.succE : std::vector<int>{};
    std::vector<uint8_t> mixed(hasPlace ? g.n : 0, 0);
    if (hasPlace) {
        std::vector<int> arriving = g.predE;
        auto byOffset = [&](std::vector<int>& list, int from, int to, const std::vector<double>& dx) -> bool {
            for (int k = from + 1; k < to; k++) {
                if (dx[list[k]] == dx[list[from]]) continue;
                std::stable_sort(list.begin() + from, list.begin() + to, [&](int a, int b) {
                    return dx[a] < dx[b];
                });
                return true;
            }
            return false;
        };
        for (int v = 0; v < g.n; v++) {
            places[v] = 1;
            if (byOffset(arriving, g.predStart[v], g.predStart[v + 1], g.ETD)) {
                for (int k = g.predStart[v] + 1; k < g.predStart[v + 1]; k++) {
                    if (g.ETD[arriving[k]] != g.ETD[arriving[k - 1]]) places[v]++;
                    placeIn[arriving[k]] = places[v] - 1;
                }
            }
            if (byOffset(leaving, g.succStart[v], g.succStart[v + 1], g.EFD)) mixed[v] = 1;
        }
    }
    int size = 1;
    int limit = hasPlace ? (widest + static_cast<int>(g.EF.size())) : widest;
    while (size < limit) size <<= 1;
    std::vector<int> tree(2 * size, 0);
    std::vector<int> targets(g.EF.size(), 0);

    auto count = [&]() -> int {
        int total = 0;
        for (int r = 0; r < g.maxRank; r++) {
            int southLen = g.layerStart[r + 2] - g.layerStart[r + 1];
            if (hasPlace) {
                int next = 0;
                for (int i = g.layerStart[r + 1]; i < g.layerStart[r + 2]; i++) {
                    int v = g.order[i];
                    for (int k = g.predStart[v]; k < g.predStart[v + 1]; k++) place[g.predE[k]] = next + placeIn[g.predE[k]];
                    next += places[v];
                }
                southLen = next;
            }
            if (southLen < 2) continue;
            int t = 0;
            for (int i = g.layerStart[r]; i < g.layerStart[r + 1]; i++) {
                int v = g.order[i];
                int from = g.succStart[v];
                int to = g.succStart[v + 1];
                int t0 = t;
                for (int k = from; k < to; k++) targets[t++] = hasPlace ? place[leaving[k]] : g.pos[g.ET[g.succE[k]]];
                int d = t - t0;
                if (d < 2) continue;
                if (hasPlace && mixed[v]) {
                    for (int a = t0 + 1; a < t; a++) {
                        int item = targets[a];
                        double dx = g.EFD[leaving[from + a - t0]];
                        int b = a - 1;
                        while (b >= t0 && g.EFD[leaving[from + b - t0]] == dx && targets[b] > item) {
                            targets[b + 1] = targets[b];
                            b--;
                        }
                        targets[b + 1] = item;
                    }
                } else if (d <= 12) {
                    for (int a = t0 + 1; a < t; a++) {
                        int item = targets[a];
                        int b = a - 1;
                        while (b >= t0 && targets[b] > item) {
                            targets[b + 1] = targets[b];
                            b--;
                        }
                        targets[b + 1] = item;
                    }
                } else {
                    std::sort(targets.begin() + t0, targets.begin() + t);
                }
            }
            if (t < 2) continue;
            int leaves = 1;
            while (leaves < southLen) leaves <<= 1;
            std::fill(tree.begin(), tree.begin() + 2 * leaves, 0);
            for (int k = 0; k < t; k++) {
                int i = targets[k] + leaves;
                tree[i]++;
                while (i > 1) {
                    if ((i & 1) == 0) total += tree[i + 1];
                    i >>= 1;
                    tree[i]++;
                }
            }
        }
        return total;
    };

    int declared = count();
    std::vector<double> bary(g.n, 0.0);
    std::vector<int> moving(widest, 0);
    std::vector<int> spare(widest, 0);

    int groups = g.hasGrp ? static_cast<int>(g.groupParent.size()) : 0;
    std::vector<int> depth(groups, 0);
    for (int gr = 0; gr < groups; gr++) for (int p = gr; p >= 0; p = g.groupParent[p]) depth[gr]++;
    std::vector<double> groupSum(groups, 0.0);
    std::vector<int> groupCount(groups, 0);
    std::vector<int> groupFirst(groups, 0);
    auto arrange = [&](int r, const std::vector<double>& key) {
        int s = g.layerStart[r];
        int e = g.layerStart[r + 1];
        for (int i = s; i < e; i++) {
            for (int gr = g.GRP[g.order[i]]; gr >= 0; gr = g.groupParent[gr]) {
                groupSum[gr] = 0.0;
                groupCount[gr] = 0;
            }
        }
        for (int i = s; i < e; i++) {
            int v = g.order[i];
            for (int gr = g.GRP[v]; gr >= 0; gr = g.groupParent[gr]) {
                if (groupCount[gr] == 0) groupFirst[gr] = i;
                groupSum[gr] += key[v];
                groupCount[gr]++;
            }
        }
        auto value = [&](int item) -> double {
            return item >= g.n ? groupSum[item - g.n] / groupCount[item - g.n] : key[item];
        };
        auto first = [&](int item) -> int {
            return item >= g.n ? groupFirst[item - g.n] : s + g.pos[item];
        };
        std::vector<int> row(g.order.begin() + s, g.order.begin() + e);
        std::stable_sort(row.begin(), row.end(), [&](int a, int b) {
            int ia = a, ib = b;
            int ca = g.GRP[a], cb = g.GRP[b];
            int da = ca < 0 ? 0 : depth[ca];
            int db = cb < 0 ? 0 : depth[cb];
            for (; da > db; da--) {
                ia = g.n + ca;
                ca = g.groupParent[ca];
            }
            for (; db > da; db--) {
                ib = g.n + cb;
                cb = g.groupParent[cb];
            }
            while (ca != cb) {
                ia = g.n + ca;
                ca = g.groupParent[ca];
                ib = g.n + cb;
                cb = g.groupParent[cb];
            }
            double va = value(ia), vb = value(ib);
            if (va != vb) return va < vb;
            return first(ia) < first(ib);
        });
        for (size_t i = 0; i < row.size(); i++) g.order[s + i] = row[i];
        for (int i = s; i < e; i++) g.pos[g.order[i]] = i - s;
    };
    if (g.hasGrp) {
        std::vector<double> at(g.n, 0.0);
        for (int r = 0; r <= g.maxRank; r++) {
            for (int i = g.layerStart[r]; i < g.layerStart[r + 1]; i++) at[g.order[i]] = i - g.layerStart[r];
            arrange(r, at);
        }
    }
    int best = g.hasGrp ? count() : declared;
    if (best == 0) return {0, declared};
    std::vector<int> bestOrder = g.order;
    int start = std::min(best, declared);

    auto sweep = [&](bool down, bool biasRight) {
        const auto& st = down ? g.predStart : g.succStart;
        const auto& list = down ? g.predE : g.succE;
        const auto& other = down ? g.EF : g.ET;
        const auto& otherDx = down ? g.EFD : g.ETD;
        for (int step = 1; step <= g.maxRank; step++) {
            int r = down ? step : g.maxRank - step;
            if (g.fixed[r]) continue;
            int s = g.layerStart[r];
            int e = g.layerStart[r + 1];
            int movable = 0;
            for (int i = s; i < e; i++) {
                int v = g.order[i];
                int pin = v < g.real ? nodes[v].pin : 0;
                if (pin != 0) {
                    bary[v] = pin * 1e9;
                    moving[movable++] = v;
                    continue;
                }
                int from = st[v];
                int to = st[v + 1];
                if (from == to) {
                    if (g.hasGrp) {
                        int across = down ? g.layerStart[r] - g.layerStart[r - 1] : g.layerStart[r + 2] - g.layerStart[r + 1];
                        bary[v] = (static_cast<double>(i - s) * across) / (e - s);
                        continue;
                    }
                    bary[v] = -1.0;
                    continue;
                }
                double sum = 0.0;
                for (int k = from; k < to; k++) {
                    int u = other[list[k]];
                    sum += g.pos[u];
                    if (g.offsets && g.W[u] > 0.0) sum += otherDx[list[k]] / g.W[u];
                }
                bary[v] = sum / (to - from);
                moving[movable++] = v;
            }
            if (g.hasGrp) {
                arrange(r, bary);
                continue;
            }
            if (movable < 2) continue;
            sortByBary(moving, movable, bary, g.pos, biasRight, spare);
            int k = 0;
            for (int i = s; i < e; i++) if (bary[g.order[i]] != -1.0) g.order[i] = moving[k++];
            for (int i = s; i < e; i++) g.pos[g.order[i]] = i - s;
        }
    };

    auto outOfOrder = [&]() -> int {
        int total = 0;
        for (int r = 0; r <= g.maxRank; r++) {
            for (int i = g.layerStart[r]; i < g.layerStart[r + 1]; i++) {
                if (g.order[i] >= g.real) continue;
                for (int j = i + 1; j < g.layerStart[r + 1]; j++) if (g.order[j] < g.order[i]) total++;
            }
        }
        return total;
    };
    auto settlePositions = [&]() {
        for (int r = 0; r <= g.maxRank; r++) for (int i = g.layerStart[r]; i < g.layerStart[r + 1]; i++) g.pos[g.order[i]] = i - g.layerStart[r];
    };

    auto pinOf = [&](int v) -> int { return v < g.real ? nodes[v].pin : 0; };
    auto at = [&](int e, bool up) -> double {
        int u = up ? g.EF[e] : g.ET[e];
        return g.offsets && g.W[u] > 0.0 ? g.pos[u] + (up ? g.EFD[e] : g.ETD[e]) / g.W[u] : g.pos[u];
    };
    auto between = [&](int a, int b) -> int {
        int c = 0;
        for (int k = g.predStart[a]; k < g.predStart[a + 1]; k++) {
            double x = at(g.predE[k], true);
            for (int j = g.predStart[b]; j < g.predStart[b + 1]; j++) if (x > at(g.predE[j], true)) c++;
        }
        for (int k = g.succStart[a]; k < g.succStart[a + 1]; k++) {
            double x = at(g.succE[k], false);
            for (int j = g.succStart[b]; j < g.succStart[b + 1]; j++) if (x > at(g.succE[j], false)) c++;
        }
        return c;
    };
    auto transpose = [&]() {
        for (int pass = 0; pass < 8; pass++) {
            bool improved = false;
            for (int r = 0; r <= g.maxRank; r++) {
                if (g.fixed[r]) continue;
                int s = g.layerStart[r];
                for (int i = s; i + 1 < g.layerStart[r + 1]; i++) {
                    int a = g.order[i];
                    int b = g.order[i + 1];
                    if (pinOf(a) != pinOf(b) || (g.hasGrp && g.GRP[a] != g.GRP[b])) continue;
                    if (between(b, a) >= between(a, b)) continue;
                    g.order[i] = b;
                    g.order[i + 1] = a;
                    g.pos[b] = i - s;
                    g.pos[a] = i + 1 - s;
                    improved = true;
                }
            }
            if (!improved) break;
        }
    };

    std::vector<int> initial = g.order;
    bool small = g.n <= 600;
    std::vector<int> chosen = bestOrder;
    int chosenOut = std::numeric_limits<int>::max();
    for (int attempt = 0; attempt < (small ? 2 : 1); attempt++) {
        if (attempt > 0) {
            g.order = initial;
            settlePositions();
        }
        int found = start;
        std::vector<int> foundOrder = initial;
        for (int i = 0, stale = 0; stale < 4 && i < MAX_SWEEPS; i++, stale++) {
            sweep((i + attempt) % 2 == 0, i % 4 >= 2);
            int c = count();
            if (c < found) {
                found = c;
                foundOrder = g.order;
                stale = 0;
                if (c == 0) break;
            }
        }
        g.order = foundOrder;
        if (small && found > 0) {
            settlePositions();
            transpose();
            int c = count();
            if (c < found) {
                found = c;
                foundOrder = g.order;
            }
        }
        int out = small ? outOfOrder() : 0;
        if (attempt == 0 || found < best || (found == best && out < chosenOut)) {
            best = found;
            chosen = foundOrder;
            chosenOut = out;
        }
    }
    g.order = chosen;
    settlePositions();
    bestOrder = chosen;
    if (g.hasGrp) best = count();

    if (!small) return {best, declared};
    for (int i = 0; i < g.real; i++) if (nodes[i].pin != 0) return {best, declared};
    if (chosenOut == 0 || g.fixed[0] || g.fixed[g.maxRank]) return {best, declared};
    for (int r = 0; r <= g.maxRank; r++) std::reverse(g.order.begin() + g.layerStart[r], g.order.begin() + g.layerStart[r + 1]);
    settlePositions();
    if (count() > best || outOfOrder() >= chosenOut) g.order = bestOrder;
    return {best, declared};
}

inline std::vector<double> position(Graph& g, const LayeredOptions& opt) noexcept {
    int segments = static_cast<int>(g.EF.size());
    std::vector<double> gap(g.n, 0.0);
    for (int i = 0; i < g.n; i++) gap[i] = halfRoom(g, opt, i);

    std::vector<int> key(segments, 0);
    for (int e = 0; e < segments; e++) key[e] = g.pos[g.ET[e]];
    for (int v = 0; v < g.n; v++) if (g.succStart[v + 1] - g.succStart[v] > 1) sortRange(g.succE, g.succStart[v], g.succStart[v + 1], key);
    for (int e = 0; e < segments; e++) key[e] = g.pos[g.EF[e]];
    for (int v = 0; v < g.n; v++) if (g.predStart[v + 1] - g.predStart[v] > 1) sortRange(g.predE, g.predStart[v], g.predStart[v + 1], key);

    std::vector<uint8_t> conflict(segments, 0);
    for (int r = 1; r <= g.maxRank; r++) {
        int prevLen = g.layerStart[r] - g.layerStart[r - 1];
        int s = g.layerStart[r];
        int end = g.layerStart[r + 1];
        int k0 = 0;
        int scan = s;
        for (int i = s; i < end; i++) {
            int v = g.order[i];
            int inner = -1;
            if (isDummy(g.KIND[v])) {
                int u = g.EF[g.predE[g.predStart[v]]];
                if (isDummy(g.KIND[u])) inner = u;
            }
            int k1 = inner >= 0 ? g.pos[inner] : prevLen;
            if (inner >= 0 || i == end - 1) {
                for (int j = scan; j <= i; j++) {
                    int w = g.order[j];
                    bool wDummy = isDummy(g.KIND[w]);
                    for (int k = g.predStart[w]; k < g.predStart[w + 1]; k++) {
                        int e = g.predE[k];
                        int up = g.pos[g.EF[e]];
                        if ((up < k0 || k1 < up) && !(wDummy && isDummy(g.KIND[g.EF[e]]))) conflict[e] = 1;
                    }
                }
                scan = i + 1;
                k0 = k1;
            }
        }
    }

    std::vector<std::vector<double>> results;
    std::vector<int> root(g.n), align(g.n), eFrom(g.n), eTo(g.n), indeg(g.n), outStart(g.n + 1), outE(g.n), fill(g.n), topo(g.n);
    std::vector<double> shift(g.n), eW(g.n), x(g.n);

    for (int variant = 0; variant < 4; variant++) {
        bool up = variant < 2;
        bool right = (variant & 1) == 1;
        const auto& start = up ? g.predStart : g.succStart;
        const auto& list = up ? g.predE : g.succE;
        const auto& other = up ? g.EF : g.ET;
        const auto& ownDx = up ? g.ETD : g.EFD;
        const auto& otherDx = up ? g.EFD : g.ETD;
        for (int i = 0; i < g.n; i++) {
            root[i] = i;
            align[i] = i;
            shift[i] = 0.0;
            indeg[i] = 0;
            outStart[i + 1] = 0;
            x[i] = 0.0;
        }
        outStart[0] = 0;

        for (int step = 0; step <= g.maxRank; step++) {
            int r = up ? step : g.maxRank - step;
            int s = g.layerStart[r];
            int end = g.layerStart[r + 1];
            int adjacent = up ? r - 1 : r + 1;
            int adjacentLast = (adjacent >= 0 && adjacent <= g.maxRank) ? (g.layerStart[adjacent + 1] - g.layerStart[adjacent] - 1) : 0;
            int prevIdx = -1;
            for (int i = right ? end - 1 : s; right ? i >= s : i < end; right ? i-- : i++) {
                int v = g.order[i];
                int from = start[v];
                int d = start[v + 1] - from;
                if (d == 0) continue;
                int lo = from + ((d - 1) >> 1);
                int hi = from + (d >> 1);
                for (int pass = 0; pass < 2; pass++) {
                    if (pass == 1 && lo == hi) break;
                    int e = list[(pass == 0) != right ? lo : hi];
                    int w = other[e];
                    int idx = right ? adjacentLast - g.pos[w] : g.pos[w];
                    if (align[v] == v && prevIdx < idx && conflict[e] == 0) {
                        align[w] = v;
                        align[v] = root[v] = root[w];
                        shift[v] = shift[w] + otherDx[e] - ownDx[e];
                        prevIdx = idx;
                    }
                }
            }
        }

        int blockEdges = 0;
        for (int r = 0; r <= g.maxRank; r++) {
            int s = g.layerStart[r];
            int end = g.layerStart[r + 1];
            for (int i = s + 1; i < end; i++) {
                int a = right ? g.order[i] : g.order[i - 1];
                int b = right ? g.order[i - 1] : g.order[i];
                double sep = gap[a] + gap[b];
                eFrom[blockEdges] = root[a];
                eTo[blockEdges] = root[b];
                eW[blockEdges] = right ? sep - shift[a] + shift[b] : sep + shift[a] - shift[b];
                indeg[root[b]]++;
                outStart[root[a] + 1]++;
                blockEdges++;
            }
        }
        for (int i = 0; i < g.n; i++) outStart[i + 1] += outStart[i];
        for (int i = 0; i < g.n; i++) fill[i] = outStart[i];
        for (int e = 0; e < blockEdges; e++) outE[fill[eFrom[e]]++] = e;

        int count = 0;
        for (int i = 0; i < g.n; i++) if (root[i] == i && indeg[i] == 0) topo[count++] = i;
        for (int q = 0; q < count; q++) {
            int v = topo[q];
            for (int k = outStart[v]; k < outStart[v + 1]; k++) {
                int e = outE[k];
                int w = eTo[e];
                if (x[v] + eW[e] > x[w]) x[w] = x[v] + eW[e];
                if (--indeg[w] == 0) topo[count++] = w;
            }
        }
        for (int q = count - 1; q >= 0; q--) {
            int v = topo[q];
            double minV = std::numeric_limits<double>::infinity();
            for (int k = outStart[v]; k < outStart[v + 1]; k++) {
                int e = outE[k];
                double cand = x[eTo[e]] - eW[e];
                if (cand < minV) minV = cand;
            }
            if (std::isfinite(minV) && minV > x[v]) x[v] = minV;
        }
        std::vector<double> xs(g.n, 0.0);
        for (int i = 0; i < g.n; i++) xs[i] = (right ? -x[root[i]] : x[root[i]]) + shift[i];
        results.push_back(std::move(xs));
    }

    int narrowest = 0;
    double bestWidth = std::numeric_limits<double>::infinity();
    std::vector<double> mins, maxs;
    for (int k = 0; k < 4; k++) {
        double lo = std::numeric_limits<double>::infinity();
        double hi = -std::numeric_limits<double>::infinity();
        const auto& xs = results[k];
        for (int i = 0; i < g.n; i++) {
            double half = g.W[i] / 2.0;
            if (xs[i] - half < lo) lo = xs[i] - half;
            if (xs[i] + half > hi) hi = xs[i] + half;
        }
        mins.push_back(lo);
        maxs.push_back(hi);
        if (hi - lo < bestWidth) {
            bestWidth = hi - lo;
            narrowest = k;
        }
    }
    for (int k = 0; k < 4; k++) {
        double delta = (k & 1) == 0 ? mins[narrowest] - mins[k] : maxs[narrowest] - maxs[k];
        if (delta != 0.0) {
            auto& xs = results[k];
            for (int i = 0; i < g.n; i++) xs[i] += delta;
        }
    }
    std::vector<double> out(g.n, 0.0);
    for (int i = 0; i < g.n; i++) {
        double a = results[0][i], b = results[1][i], c = results[2][i], d = results[3][i];
        if (a > b) std::swap(a, b);
        if (c > d) std::swap(c, d);
        if (a > c) std::swap(a, c);
        if (b > d) std::swap(b, d);
        if (b > c) std::swap(b, c);
        out[i] = (b + c) / 2.0;
    }
    return out;
}

} // namespace detail

inline LayeredResult layered(std::vector<LNode>& nodes, std::vector<LEdge>& edges, const LayeredOptions& opt) noexcept {
    using namespace detail;
    int real = static_cast<int>(nodes.size());
    if (real == 0) return {0.0, 0.0, 0, 0};
    int m = static_cast<int>(edges.size());
    if (real == 1 && m == 0 && nodes[0].kind == LKind::Node) {
        auto& node = nodes[0];
        node.rank = 0;
        node.x = node.w / 2.0;
        node.y = node.h / 2.0;
        return {node.w, node.h, 0, 0};
    }

    int step = 1;
    if (m > 0) {
        Pele::FlatSet<long long> seen;
        for (const auto& e : edges) {
            long long a = std::min(e.tail, e.head);
            long long b = std::max(e.tail, e.head);
            long long key = a * real + b;
            if (e.labelW > 0.0 || seen.count(key)) {
                step = 2;
                break;
            }
            seen.insert(key);
        }
    }

    orient(nodes, edges);
    rank(nodes, edges, step);

    std::vector<int> span(m, 0);
    int dummies = 0;
    for (int ei = 0; ei < m; ei++) {
        const auto& e = edges[ei];
        span[ei] = std::abs(nodes[e.head].rank - nodes[e.tail].rank);
        if (span[ei] > 1) dummies += span[ei] - 1;
    }
    std::vector<uint8_t> direct(m, 0);
    if (dummies > MAX_DUMMIES) {
        std::vector<int> longest;
        for (int ei = 0; ei < m; ei++) if (span[ei] > 1) longest.push_back(ei);
        std::sort(longest.begin(), longest.end(), [&](int a, int b) {
            if (span[b] != span[a]) return span[b] < span[a];
            return a < b;
        });
        for (int ei : longest) {
            if (dummies <= MAX_DUMMIES) break;
            direct[ei] = 1;
            dummies -= span[ei] - 1;
        }
    }

    int n = real + dummies;
    std::vector<double> W(n, 0.0), H(n, 0.0);
    std::vector<uint8_t> KIND(n, 0);
    std::vector<int> RANK(n, 0);
    int maxRank = 0;
    for (int i = 0; i < real; i++) {
        const auto& node = nodes[i];
        W[i] = node.w;
        H[i] = node.h;
        KIND[i] = static_cast<uint8_t>(node.kind);
        RANK[i] = node.rank;
        if (node.rank > maxRank) maxRank = node.rank;
    }

    int segments = 0;
    for (int ei = 0; ei < m; ei++) if (!direct[ei]) segments += span[ei];
    std::vector<int> EF(segments, 0), ET(segments, 0);
    std::vector<double> EFD(segments, 0.0), ETD(segments, 0.0);
    std::vector<int> firstDummy(m, -1);
    std::vector<uint8_t> heads(maxRank + 1, 0);
    std::vector<int> labelNode(m, -1);
    bool spans = false;
    for (int i = 0; i < real; i++) if (nodes[i].span > 0.0 && nodes[i].kind == LKind::Node) spans = true;
    std::vector<uint8_t> EFK(spans ? segments : 0, 0);
    std::vector<uint8_t> ETK(spans ? segments : 0, 0);
    std::vector<int> firstSeg(m, -1), lastSeg(m, -1);
    std::vector<double> KEY(n, std::numeric_limits<double>::quiet_NaN());
    for (int i = 0; i < real; i++) KEY[i] = nodes[i].key;
    bool offsets = false;
    {
        int next = real;
        int ce = 0;
        for (int ei = 0; ei < m; ei++) {
            if (direct[ei]) continue;
            const auto& e = edges[ei];
            int a = e.reversed ? e.head : e.tail;
            int b = e.reversed ? e.tail : e.head;
            double aDx = e.reversed ? e.headDx : e.tailDx;
            double bDx = e.reversed ? e.tailDx : e.headDx;
            if (aDx != 0.0 || bDx != 0.0) offsets = true;
            int ra = RANK[a];
            int rb = RANK[b];
            int labelRank = -1;
            if (e.labelW > 0.0) {
                labelRank = ra + ((rb - ra) >> 1);
                if (step == 2 && rb - ra >= 4) {
                    if ((labelRank & 1) == 1) labelRank += e.reversed ? 1 : -1;
                } else if (step == 2 && (labelRank & 1) == 0) {
                    labelRank += e.reversed ? 1 : -1;
                }
            }
            int prev = a;
            if (rb - ra > 1) firstDummy[ei] = next;
            firstSeg[ei] = ce;
            if (spans && !(e.reversed ? e.headFixed : e.tailFixed)) EFK[ce] = e.reversed ? EndArrives : EndLeaves;
            for (int r = ra + 1; r < rb; r++) {
                int d = next++;
                RANK[d] = r;
                KEY[d] = KEY[a] + ((KEY[b] - KEY[a]) * (r - ra)) / (rb - ra);
                if (r == labelRank) {
                    W[d] = e.labelW;
                    H[d] = e.labelH;
                    KIND[d] = static_cast<uint8_t>(LKind::Label);
                    labelNode[ei] = d;
                    heads[r] |= e.reversed ? 2 : 1;
                } else {
                    KIND[d] = static_cast<uint8_t>(LKind::Dummy);
                }
                EF[ce] = prev;
                ET[ce] = d;
                EFD[ce] = prev == a ? aDx : 0.0;
                ce++;
                prev = d;
            }
            EF[ce] = prev;
            ET[ce] = b;
            EFD[ce] = prev == a ? aDx : 0.0;
            ETD[ce] = bDx;
            if (spans && !(e.reversed ? e.tailFixed : e.headFixed)) ETK[ce] = e.reversed ? EndLeaves : EndArrives;
            lastSeg[ei] = ce;
            ce++;
        }
    }

    std::vector<int> GRP;
    if (opt.hasGroups) {
        const auto& of = opt.groupsOf;
        const auto& parent = opt.groupsParent;
        GRP.assign(n, -1);
        for (int i = 0; i < real; i++) GRP[i] = of[i];
        auto depth = [&](int gr) -> int {
            int d = 0;
            for (; gr >= 0; gr = parent[gr]) d++;
            return d;
        };
        auto lca = [&](int x, int y) -> int {
            int dx = depth(x);
            int dy = depth(y);
            for (; dx > dy; dx--) x = parent[x];
            for (; dy > dx; dy--) y = parent[y];
            while (x != y) {
                x = parent[x];
                y = parent[y];
            }
            return x;
        };
        for (int ei = 0; ei < m; ei++) {
            if (firstDummy[ei] < 0) continue;
            const auto& e = edges[ei];
            int gr = lca(of[e.tail], of[e.head]);
            for (int d = firstDummy[ei], end = d + span[ei] - 1; d < end; d++) GRP[d] = gr;
        }
    }

    std::vector<int> succStart(n + 1, 0), predStart(n + 1, 0);
    for (int e = 0; e < segments; e++) {
        succStart[EF[e] + 1]++;
        predStart[ET[e] + 1]++;
    }
    for (int i = 0; i < n; i++) {
        succStart[i + 1] += succStart[i];
        predStart[i + 1] += predStart[i];
    }
    std::vector<int> succE(segments, 0), predE(segments, 0);
    {
        std::vector<int> sf(succStart.begin(), succStart.begin() + n);
        std::vector<int> pf(predStart.begin(), predStart.begin() + n);
        for (int e = 0; e < segments; e++) {
            succE[sf[EF[e]]++] = e;
            predE[pf[ET[e]]++] = e;
        }
    }

    std::vector<int> layerStart(maxRank + 2, 0);
    for (int i = 0; i < n; i++) layerStart[RANK[i] + 1]++;
    for (int r = 0; r <= maxRank; r++) layerStart[r + 1] += layerStart[r];
    std::vector<int> order(n, 0), pos(n, 0);

    {
        std::vector<int> byRank(n, 0);
        std::vector<int> fill(layerStart.begin(), layerStart.begin() + maxRank + 1);
        for (int i = 0; i < n; i++) byRank[fill[RANK[i]]++] = i;
        for (int r = 0; r <= maxRank; r++) fill[r] = layerStart[r];
        std::vector<uint8_t> visited(n, 0);
        std::vector<int> stack;
        for (int k = 0; k < n; k++) {
            if (visited[byRank[k]]) continue;
            stack.push_back(byRank[k]);
            while (!stack.empty()) {
                int v = stack.back();
                stack.pop_back();
                if (visited[v]) continue;
                visited[v] = 1;
                order[fill[RANK[v]]++] = v;
                for (int j = succStart[v + 1] - 1; j >= succStart[v]; j--) {
                    int w = ET[succE[j]];
                    if (!visited[w]) stack.push_back(w);
                }
            }
        }
    }

    auto sortRank = [&](int r, auto&& keyFn) -> bool {
        int s = layerStart[r];
        int e = layerStart[r + 1];
        if (e - s < 2) return false;
        for (int i = s; i < e; i++) if (std::isnan(keyFn(order[i]))) return false;
        std::vector<int> layer(order.begin() + s, order.begin() + e);
        Pele::FlatMap<int, int> index;
        for (size_t i = 0; i < layer.size(); i++) index[layer[i]] = static_cast<int>(i);
        std::sort(layer.begin(), layer.end(), [&](int a, int b) {
            double ka = keyFn(a), kb = keyFn(b);
            if (ka != kb) return ka < kb;
            return index[a] < index[b];
        });
        for (size_t i = 0; i < layer.size(); i++) order[s + i] = layer[i];
        return true;
    };
    for (int r = 0; r <= maxRank; r++) sortRank(r, [&](int v) { return KEY[v]; });

    bool pinned = false;
    for (int i = 0; i < real; i++) if (nodes[i].pin != 0) pinned = true;
    if (pinned) {
        for (int r = 0; r <= maxRank; r++) {
            int s = layerStart[r], e = layerStart[r + 1];
            std::vector<int> layer(order.begin() + s, order.begin() + e);
            Pele::FlatMap<int, int> index;
            for (size_t i = 0; i < layer.size(); i++) index[layer[i]] = static_cast<int>(i);
            auto pinOf = [&](int v) -> int { return v < real ? nodes[v].pin : 0; };
            std::sort(layer.begin(), layer.end(), [&](int a, int b) {
                if (pinOf(a) != pinOf(b)) return pinOf(a) < pinOf(b);
                return index[a] < index[b];
            });
            for (size_t i = 0; i < layer.size(); i++) order[s + i] = layer[i];
        }
    }
    std::vector<uint8_t> fixed(maxRank + 1, 0);
    auto hintFn = [&](int v) -> double {
        return (v < real && (nodes[v].kind == LKind::StartPort || nodes[v].kind == LKind::EndPort)) ? nodes[v].hint : std::numeric_limits<double>::quiet_NaN();
    };
    for (int r : {0, maxRank}) if (sortRank(r, hintFn)) fixed[r] = 1;
    for (int r = 0; r <= maxRank; r++) {
        int s = layerStart[r];
        for (int i = s, e = layerStart[r + 1]; i < e; i++) pos[order[i]] = i - s;
    }

    Graph g{n, real, maxRank, std::move(W), std::move(KIND), std::move(RANK), std::move(EF), std::move(ET), std::move(EFD), std::move(ETD), std::move(succStart), std::move(succE), std::move(predStart), std::move(predE), std::move(layerStart), std::move(order), std::move(pos), offsets, std::move(fixed), opt.hasGroups, std::move(GRP), opt.groupsParent};

    int crossings = 0;
    int declared = 0;
    if (segments > 0 && maxRank > 0) {
        std::tie(crossings, declared) = reduceCrossings(g, nodes);
        for (int r = 0; r <= maxRank; r++) {
            int s = g.layerStart[r];
            for (int i = s, e = g.layerStart[r + 1]; i < e; i++) g.pos[g.order[i]] = i - s;
        }
    }
    if (opt.orderOnly) {
        std::vector<double> at = position(g, opt);
        for (int i = 0; i < real; i++) nodes[i].x = at[i];
        for (int ei = 0; ei < m; ei++) {
            auto& e = edges[ei];
            int a = e.reversed ? e.head : e.tail;
            int b = e.reversed ? e.tail : e.head;
            std::vector<double> pts = {static_cast<double>(g.RANK[a]), at[a]};
            if (!direct[ei] && firstDummy[ei] >= 0) {
                for (int d = firstDummy[ei], end = d + span[ei] - 1; d < end; d++) {
                    pts.push_back(static_cast<double>(g.RANK[d]));
                    pts.push_back(at[d]);
                }
            }
            pts.push_back(static_cast<double>(g.RANK[b]));
            pts.push_back(at[b]);
            e.points = std::move(pts);
        }
        return {0.0, 0.0, crossings, declared};
    }

    if (spans) spread(g, nodes, EFK, ETK);

    std::vector<double> xs = position(g, opt);

    std::vector<uint8_t> emptyVec;
    bool hasEmpty = emptyRanks(g, emptyVec);
    const std::vector<uint8_t>* empty = hasEmpty ? &emptyVec : nullptr;
    if (spans) {
        auto free = freeEnds(g, EFK, ETK);
        alignLabels(g, nodes, xs, free, opt, empty);
        settle(g, nodes, xs, EFK, ETK, free, empty);
        snap(g, xs, empty, opt);
        for (int ei = 0; ei < m; ei++) {
            if (direct[ei]) continue;
            auto& e = edges[ei];
            double aDx = g.EFD[firstSeg[ei]];
            double bDx = g.ETD[lastSeg[ei]];
            e.tailDx = e.reversed ? bDx : aDx;
            e.headDx = e.reversed ? aDx : bDx;
        }
    }

    std::vector<uint8_t> onLine(n, 0);
    if (empty && !opt.tracks) straighten(g, xs, *empty, firstDummy, span, opt.apart ? 0.0 : opt.edgeSep, onLine);
    if (!opt.tracks) pastLabels(g, xs, firstDummy, span, opt.edgeSep, onLine);

    std::vector<int> pieceStart(m + 1, 0);
    bool hasPieces = opt.tracks || (spans && !opt.straight);
    Pieces pieces;
    std::vector<int> trackCount(maxRank + 1, 0);
    if (hasPieces) {
        for (int ei = 0; ei < m; ei++) {
            pieceStart[ei] = static_cast<int>(pieces.from.size());
            if (direct[ei]) continue;
            int d0 = firstDummy[ei];
            bool twin = false;
            for (int d = d0, end = d0 < 0 ? d0 : d0 + span[ei] - 1; d < end; d++) {
                bool bare = empty != nullptr && (*empty)[g.RANK[d]] == 1;
                if (!opt.tracks && bare != (onLine[d] == 1)) twin = true;
            }
            if (twin) continue;
            double x = xs[g.EF[firstSeg[ei]]] + g.EFD[firstSeg[ei]];
            int r = g.RANK[g.EF[firstSeg[ei]]];
            for (int d = d0, end = d0 < 0 ? d0 : d0 + span[ei] - 1; d < end; d++) {
                if (empty && (*empty)[g.RANK[d]]) continue;
                pieces.from.push_back(x);
                pieces.to.push_back(xs[d]);
                pieces.rank.push_back(r);
                pieces.lower.push_back(d);
                x = xs[d];
                r = g.RANK[d];
            }
            pieces.from.push_back(x);
            pieces.to.push_back(xs[g.ET[lastSeg[ei]]] + g.ETD[lastSeg[ei]]);
            pieces.rank.push_back(r);
            pieces.lower.push_back(-1);
        }
        pieceStart[m] = static_cast<int>(pieces.from.size());
    }

    auto portOnly = [&](int r, LKind kind) -> bool {
        bool found = false;
        for (int i = g.layerStart[r]; i < g.layerStart[r + 1]; i++) {
            uint8_t k = g.KIND[g.order[i]];
            if (k == static_cast<uint8_t>(kind)) found = true;
            else if (k != static_cast<uint8_t>(LKind::Dummy)) return false;
        }
        return found;
    };
    int startRanks = portOnly(0, LKind::StartPort) ? step : 0;
    int endRanks = portOnly(maxRank, LKind::EndPort) ? step : 0;
    std::vector<double> run(maxRank + 1, 0.0);
    for (int e = 0; e < segments; e++) {
        double across = std::abs(xs[g.EF[e]] + g.EFD[e] - xs[g.ET[e]] - g.ETD[e]);
        if (across > run[g.RANK[g.EF[e]]]) run[g.RANK[g.EF[e]]] = across;
    }
    std::vector<double> bandTop(maxRank + 1, 0.0), bandBottom(maxRank + 1, 0.0);
    auto usual = [&](int r) -> double {
        return (r < startRanks || r >= maxRank - endRanks ? opt.portSep : opt.rankSep) / step;
    };
    std::vector<double> gapUnder(maxRank + 1, 0.0);
    for (int r = 0; r < maxRank; r++) {
        gapUnder[r] = hasPieces ? usual(r) : std::max(usual(r), std::min((MAX_GAP * opt.rankSep) / step, run[r] * MIN_SLOPE));
    }
    std::vector<uint8_t> between(maxRank + 1, 1);
    for (int i = 0; i < real; i++) between[g.RANK[i]] = 0;
    for (int r = 1; r < maxRank; r++) {
        if (opt.tracks || !between[r] || (heads[r] != 1 && heads[r] != 2)) continue;
        double shift = std::min(opt.headRoom / 2.0, gapUnder[heads[r] == 1 ? r - 1 : r] / 2.0);
        gapUnder[r - 1] += heads[r] == 1 ? -shift : shift;
        gapUnder[r] += heads[r] == 1 ? shift : -shift;
    }
    auto inset = [&](int d) -> double {
        return (g.KIND[d] == static_cast<uint8_t>(LKind::Dummy) && between[g.RANK[d]] && !opt.tracks) ? (bandBottom[g.RANK[d]] - bandTop[g.RANK[d]]) / 4.0 : 0.0;
    };
    double margin = opt.tracks ? TRACK_MARGIN : LEVEL_MARGIN;
    double leastPitch = opt.tracks ? TRACK_PITCH : LEVEL_PITCH;
    double mostPitch = opt.tracks ? TRACK_SPREAD : LEVEL_PITCH;
    std::vector<int> below(maxRank + 1, 0);
    if (hasPieces) {
        std::vector<double> room(maxRank + 1, 0.0);
        for (int r = 0; r < maxRank; r++) {
            if (empty && (*empty)[r]) continue;
            int next = r + 1;
            room[r] = gapUnder[r];
            while (next < maxRank && empty && (*empty)[next]) room[r] += gapUnder[next++];
            below[r] = next;
        }
        for (size_t k = 0; k < pieces.from.size(); k++) {
            pieces.wide.push_back(std::abs(pieces.from[k] - pieces.to[k]) > LEVEL_WIDE * room[pieces.rank[k]] ? 1 : 0);
        }
        std::vector<double> tall(maxRank + 1, 0.0);
        assignTracks(pieces, trackCount, !opt.tracks, room, tall);
        for (int r = 0; r < maxRank; r++) {
            if (trackCount[r] > 0) gapUnder[r] += std::max(0.0, 2.0 * margin + (trackCount[r] - 1) * leastPitch - room[r]);
            else if (tall[r] > 0.0) gapUnder[r] += std::max(0.0, std::min(MAX_GAP * opt.rankSep, tall[r]) - room[r]);
        }
    }
    double y = 0.0;
    for (int r = 0; r <= maxRank; r++) {
        double h = 0.0;
        for (int i = g.layerStart[r]; i < g.layerStart[r + 1]; i++) if (H[g.order[i]] > h) h = H[g.order[i]];
        bandTop[r] = y;
        bandBottom[r] = y + h;
        y += h + gapUnder[r];
    }
    double height = y;
    auto trackY = [&](int piece, int onTrack) -> double {
        int r = pieces.rank[piece];
        double top = bandBottom[r];
        double bottom = bandTop[below[r]];
        int count = opt.tracks ? trackCount[r] : pieces.levels[piece];
        double pitch = count > 1 ? std::min(mostPitch, (bottom - top - 2.0 * margin) / (count - 1)) : 0.0;
        return (top + bottom) / 2.0 + (onTrack - (count - 1) / 2.0) * pitch;
    };

    double minX = std::numeric_limits<double>::infinity();
    double maxX = -std::numeric_limits<double>::infinity();
    for (int i = 0; i < n; i++) {
        double half = g.W[i] / 2.0;
        if (xs[i] - half < minX) minX = xs[i] - half;
        if (xs[i] + half > maxX) maxX = xs[i] + half;
    }
    std::vector<uint8_t> arrives(maxRank + 1, 0), leaves(maxRank + 1, 0);
    for (int i = 0; i < real; i++) {
        if (g.KIND[i] != static_cast<uint8_t>(LKind::Node) || !spans) continue;
        if (g.predStart[i + 1] > g.predStart[i]) arrives[g.RANK[i]] = 1;
        if (g.succStart[i + 1] > g.succStart[i]) leaves[g.RANK[i]] = 1;
    }
    for (int i = 0; i < real; i++) {
        auto& node = nodes[i];
        int r = node.rank;
        node.x = xs[i] - minX;
        if (g.KIND[i] == static_cast<uint8_t>(LKind::Node) && leaves[r] && !arrives[r]) node.y = bandBottom[r] - node.h / 2.0;
        else if (g.KIND[i] == static_cast<uint8_t>(LKind::Node) && arrives[r] && !leaves[r]) node.y = bandTop[r] + node.h / 2.0;
        else node.y = (bandTop[r] + bandBottom[r]) / 2.0;
    }

    for (int ei = 0; ei < m; ei++) {
        auto& e = edges[ei];
        const auto& first = nodes[e.reversed ? e.head : e.tail];
        const auto& last = nodes[e.reversed ? e.tail : e.head];
        std::vector<double> pts = {first.x + (e.reversed ? e.headDx : e.tailDx), first.y + first.h / 2.0};
        int d0 = firstDummy[ei];
        if (hasPieces && pieceStart[ei + 1] > pieceStart[ei]) {
            for (int k = pieceStart[ei]; k < pieceStart[ei + 1]; k++) {
                if (pieces.track[k] >= 0 && (opt.tracks || pieces.wide[k] == 1)) {
                    double across = trackY(k, pieces.track[k]);
                    pts.push_back(pieces.from[k] - minX);
                    pts.push_back(across);
                    pts.push_back(pieces.to[k] - minX);
                    pts.push_back(across);
                } else if (pieces.track[k] > 0) {
                    pts.push_back(pieces.from[k] - minX);
                    pts.push_back(trackY(k, pieces.track[k] - 1));
                }
                int d = pieces.lower[k];
                if (d < 0) continue;
                pts.push_back(xs[d] - minX);
                pts.push_back(bandTop[g.RANK[d]] + inset(d));
                if (bandBottom[g.RANK[d]] > bandTop[g.RANK[d]]) {
                    pts.push_back(xs[d] - minX);
                    pts.push_back(bandBottom[g.RANK[d]] - inset(d));
                }
            }
        } else if (d0 >= 0) {
            for (int d = d0, end = d0 + span[ei] - 1; d < end; d++) {
                if (onLine[d]) continue;
                double x = xs[d] - minX;
                double top = bandTop[g.RANK[d]];
                double bottom = bandBottom[g.RANK[d]];
                pts.push_back(x);
                pts.push_back(top + inset(d));
                if (bottom > top) {
                    pts.push_back(x);
                    pts.push_back(bottom - inset(d));
                }
            }
        }
        pts.push_back(last.x + (e.reversed ? e.tailDx : e.headDx));
        pts.push_back(last.y - last.h / 2.0);
        if (e.reversed) {
            for (int i = 0, j = static_cast<int>(pts.size()) - 2; i < j; i += 2, j -= 2) {
                std::swap(pts[i], pts[j]);
                std::swap(pts[i + 1], pts[j + 1]);
            }
        }
        e.points = std::move(pts);
        int ln = labelNode[ei];
        if (ln >= 0) {
            e.labelX = xs[ln] - minX;
            e.labelY = (bandTop[g.RANK[ln]] + bandBottom[g.RANK[ln]]) / 2.0;
        } else if (e.labelW > 0.0) {
            e.labelX = (e.points[0] + e.points[e.points.size() - 2]) / 2.0;
            e.labelY = (e.points[1] + e.points[e.points.size() - 1]) / 2.0;
        }
    }

    return {maxX - minX, height, crossings, declared};
}

// ============================================================================
// Port of src/layout/compound.ts
// ============================================================================

inline std::string direction(const std::string& dir) noexcept {
    return (dir == "BT" || dir == "LR" || dir == "RL") ? dir : "TB";
}

struct CNode {
    double w = 0.0;
    double h = 0.0;
    int parent = -1;
    bool isGroup = false;
    std::string dir; // empty = undefined
    double padX = 0.0;
    double padTop = 0.0;
    double padBottom = 0.0;
    double minW = 0.0;
    int seq = 0;
    double span = 0.0;
    double x = 0.0;
    double y = 0.0;
};

struct CEdge {
    int src = 0;
    int dst = 0;
    int minlen = 1;
    double labelW = 0.0;
    double labelH = 0.0;
    std::vector<double> route;
    double labelX = 0.0;
    double labelY = 0.0;
};

struct CompoundResult {
    double width = 0.0;
    double height = 0.0;
};

enum class Side : int {
    Top = 0,
    Right = 1,
    Bottom = 2,
    Left = 3
};

enum class RouteStyle : int {
    Curved = 0,
    Stepped = 1,
    Linear = 2
};

inline RouteStyle styleOf(const LayeredOptions& opt) noexcept {
    return opt.tracks ? RouteStyle::Stepped : opt.straight ? RouteStyle::Linear : RouteStyle::Curved;
}

namespace detail {

struct PortEdge {
    int e = 0;
    int inner = 0;
    bool out = false;
    Side side = Side::Top;
    double hint = std::numeric_limits<double>::quiet_NaN();
    double px = 0.0;
    double py = 0.0;
    std::vector<double> route;
};

struct LevelEdge {
    int e = 0;
    int a = 0;
    int b = 0;
};

struct Level {
    int id = -1;
    std::string dir = "TB";
    std::vector<int> items;
    std::vector<LevelEdge> edges;
    std::vector<PortEdge> ports;
    Pele::FlatMap<int, int> portIdxOf; // key: ei * 2 + (out ? 0 : 1) -> index in ports
    std::unordered_map<int, std::vector<double>> routes;
    double x = 0.0;
    inline PortEdge& getPort(int key) { return ports[portIdxOf.at(key)]; }
    double y = 0.0;
};

constexpr int MAX_PORTS = 40000;
constexpr int MAX_ORDERED_PORTS = 400;
constexpr int MAX_ORDERED_EDGES = 500;
constexpr double FLAT_SLACK = 1.5;
constexpr int FLAT_SPARE = 4;
constexpr double FAN = 24.0;
constexpr int CURVE_PIECES = 3;
constexpr int CORNER_PIECES = 2;
constexpr int STEP_PIECES = 3;
constexpr double HANDLE = 0.62;
constexpr int MAX_INSERT = 128;

struct Snapshot {
    std::vector<double> nodes;
    std::vector<std::vector<double>> routes;
    std::vector<double> labels;
    std::vector<double> levels;
    CompoundResult total;
};

inline Snapshot snapshot(const std::vector<CNode>& nodes, const std::vector<CEdge>& edges, const std::vector<Level>& levels, CompoundResult total) noexcept {
    Snapshot s;
    s.nodes.reserve(nodes.size() * 4);
    for (const auto& c : nodes) {
        s.nodes.push_back(c.x);
        s.nodes.push_back(c.y);
        s.nodes.push_back(c.w);
        s.nodes.push_back(c.h);
    }
    s.routes.reserve(edges.size());
    s.labels.reserve(edges.size() * 2);
    for (const auto& e : edges) {
        s.routes.push_back(e.route);
        s.labels.push_back(e.labelX);
        s.labels.push_back(e.labelY);
    }
    for (const auto& level : levels) {
        s.levels.push_back(level.x);
        s.levels.push_back(level.y);
        for (const auto& port : level.ports) {
            s.levels.push_back(port.px);
            s.levels.push_back(port.py);
        }
    }
    s.total = total;
    return s;
}

inline CompoundResult restore(const Snapshot& s, std::vector<CNode>& nodes, std::vector<CEdge>& edges, std::vector<Level>& levels) noexcept {
    for (size_t i = 0; i < nodes.size(); i++) {
        nodes[i].x = s.nodes[4 * i];
        nodes[i].y = s.nodes[4 * i + 1];
        nodes[i].w = s.nodes[4 * i + 2];
        nodes[i].h = s.nodes[4 * i + 3];
    }
    for (size_t i = 0; i < edges.size(); i++) {
        edges[i].route = s.routes[i];
        edges[i].labelX = s.labels[2 * i];
        edges[i].labelY = s.labels[2 * i + 1];
    }
    size_t k = 0;
    for (auto& level : levels) {
        level.x = s.levels[k++];
        level.y = s.levels[k++];
        for (auto& port : level.ports) {
            port.px = s.levels[k++];
            port.py = s.levels[k++];
        }
    }
    return s.total;
}

inline Side startSide(const std::string& dir) noexcept {
    return dir == "TB" ? Side::Top : dir == "BT" ? Side::Bottom : dir == "LR" ? Side::Left : Side::Right;
}

inline Side opposite(Side side) noexcept {
    return static_cast<Side>((static_cast<int>(side) + 2) & 3);
}

} // namespace detail

inline int routeCrossings(const std::vector<CNode>& nodes, const std::vector<CEdge>& edges, RouteStyle style = RouteStyle::Curved) noexcept {
    using namespace detail;
    size_t size = 0;
    for (const auto& e : edges) {
        if (e.src != e.dst && e.route.size() > 3) size += (e.route.size() / 3 - 1) * STEP_PIECES;
    }
    std::vector<double> ax(size, 0.0), ay(size, 0.0), bx(size, 0.0), by(size, 0.0), low(size, 0.0), high(size, 0.0);
    std::vector<int> owner(size, 0);
    std::vector<uint8_t> ends(size, 0);
    int count = 0;
    double top = std::numeric_limits<double>::infinity();
    double bottom = -std::numeric_limits<double>::infinity();
    double fromX = std::numeric_limits<double>::quiet_NaN();
    double fromY = std::numeric_limits<double>::quiet_NaN();
    double toX = std::numeric_limits<double>::quiet_NaN();
    double toY = std::numeric_limits<double>::quiet_NaN();

    auto add = [&](int ei, uint8_t part, double x0, double y0, double x1, double y1) {
        int p = count - 1;
        bool on = x0 == toX && y0 == toY && ((y1 == y0 && fromY == y0) || (x1 == x0 && fromX == x0));
        if (on && p >= 0 && owner[p] == ei && ends[p] == part && (x0 - fromX) * (x1 - x0) + (y0 - fromY) * (y1 - y0) > 0.0) {
            count--;
            x0 = fromX;
            y0 = fromY;
        }
        fromX = x0;
        fromY = y0;
        toX = x1;
        toY = y1;
        ends[count] = part;
        ax[count] = std::min(x0, x1);
        bx[count] = std::max(x0, x1);
        ay[count] = x0 <= x1 ? y0 : y1;
        by[count] = x0 <= x1 ? y1 : y0;
        low[count] = std::min(y0, y1);
        high[count] = std::max(y0, y1);
        if (low[count] < top) top = low[count];
        if (high[count] > bottom) bottom = high[count];
        owner[count++] = ei;
    };

    for (size_t ei = 0; ei < edges.size(); ei++) {
        const auto& e = edges[ei];
        if (e.src == e.dst) continue;
        const auto& r = e.route;
        int rlen = static_cast<int>(r.size());
        if (rlen < 3) continue;
        auto aligned = [&](int k) -> bool {
            return std::abs(r[k] - r[k - 3]) < 0.01 || std::abs(r[k + 1] - r[k - 2]) < 0.01;
        };
        auto length = [&](int k) -> double {
            return std::abs(r[k] - r[k - 3]) + std::abs(r[k + 1] - r[k - 2]);
        };
        auto turns = [&](int k) -> bool {
            return style == RouteStyle::Curved &&
                   k > 0 &&
                   k < rlen - 3 &&
                   aligned(k) &&
                   aligned(k + 3) &&
                   length(k) >= 0.01 &&
                   length(k + 3) >= 0.01 &&
                   ((std::abs(r[k] - r[k - 3]) < 0.01) != (std::abs(r[k + 3] - r[k]) < 0.01));
        };
        double atX = r[0];
        double atY = r[1];
        for (int k = 3; k < rlen; k += 3) {
            uint8_t part = static_cast<uint8_t>((k == 3 ? 1 : 0) | (k == rlen - 3 ? 2 : 0));
            double x0 = atX;
            double y0 = atY;
            double x1 = r[k];
            double y1 = r[k + 1];
            atX = x1;
            atY = y1;
            if (aligned(k) || style == RouteStyle::Linear) {
                if (!turns(k)) {
                    add(static_cast<int>(ei), part, x0, y0, x1, y1);
                    continue;
                }
                double before = turns(k - 3) ? length(k) / 2.0 : length(k);
                double after = turns(k + 3) ? length(k + 3) / 2.0 : length(k + 3);
                double ux = (x1 - r[k - 3]) / length(k);
                double uy = (y1 - r[k - 2]) / length(k);
                double vx = (r[k + 3] - x1) / length(k + 3);
                double vy = (r[k + 4] - y1) / length(k + 3);
                double sx = x1 - ux * before;
                double sy = y1 - uy * before;
                add(static_cast<int>(ei), part, x0, y0, sx, sy);
                double c1x = x1 - ux * before * (1.0 - HANDLE);
                double c1y = y1 - uy * before * (1.0 - HANDLE);
                double c2x = x1 + vx * after * (1.0 - HANDLE);
                double c2y = y1 + vy * after * (1.0 - HANDLE);
                atX = x1 + vx * after;
                atY = y1 + vy * after;
                uint8_t turn = static_cast<uint8_t>(part | (k + 3 == rlen - 3 ? 2 : 0));
                double px = sx;
                double py = sy;
                for (int i = 1; i <= CORNER_PIECES; i++) {
                    double t = static_cast<double>(i) / CORNER_PIECES;
                    double u = 1.0 - t;
                    double x = u * u * u * sx + 3.0 * u * u * t * c1x + 3.0 * u * t * t * c2x + t * t * t * atX;
                    double y = u * u * u * sy + 3.0 * u * u * t * c1y + 3.0 * u * t * t * c2y + t * t * t * atY;
                    add(static_cast<int>(ei), turn, px, py, x, y);
                    px = x;
                    py = y;
                }
                continue;
            }
            if (style == RouteStyle::Stepped) {
                bool down = r[k + 2] == 0.0;
                double mx = down ? x0 : (x0 + x1) / 2.0;
                double my = down ? (y0 + y1) / 2.0 : y0;
                add(static_cast<int>(ei), part, x0, y0, mx, my);
                add(static_cast<int>(ei), part, mx, my, down ? x1 : mx, down ? my : y1);
                add(static_cast<int>(ei), part, down ? x1 : mx, down ? my : y1, x1, y1);
                continue;
            }
            bool down = r[k + 2] == 0.0;
            double c = down ? ease(x1 - x0, y1 - y0) : ease(y1 - y0, x1 - x0);
            double px = x0;
            double py = y0;
            for (int i = 1; i <= CURVE_PIECES; i++) {
                double t = static_cast<double>(i) / CURVE_PIECES;
                double u = 1.0 - t;
                double across = t * t * (3.0 - 2.0 * t);
                double along = 3.0 * c * t * u * u + 3.0 * (1.0 - c) * t * t * u + t * t * t;
                double x = x0 + (x1 - x0) * (down ? across : along);
                double y = y0 + (y1 - y0) * (down ? along : across);
                add(static_cast<int>(ei), part, px, py, x, y);
                px = x;
                py = y;
            }
        }
    }

    int bands = bottom > top ? std::max(1, static_cast<int>(std::round(std::sqrt(static_cast<double>(count))))) : 1;
    double scale = bottom > top ? static_cast<double>(bands) / (bottom - top) : 0.0;
    auto bandOf = [&](double y) -> int {
        return std::min(bands - 1, std::max(0, static_cast<int>(std::floor((y - top) * scale))));
    };
    std::vector<int> first(count, 0);
    std::vector<int> bandStart(bands + 1, 0);
    for (int p = 0; p < count; p++) {
        first[p] = bandOf(low[p]);
        for (int b = first[p], last = bandOf(high[p]); b <= last; b++) bandStart[b + 1]++;
    }
    for (int b = 0; b < bands; b++) bandStart[b + 1] += bandStart[b];
    std::vector<int> fill(bandStart.begin(), bandStart.begin() + bands);
    std::vector<int> inBand(bandStart[bands], 0);
    for (int p = 0; p < count; p++) {
        for (int b = first[p], last = bandOf(high[p]); b <= last; b++) inBand[fill[b]++] = p;
    }

    std::vector<double> runs(edges.size() * 8, 0.0);
    auto inLine = [](const std::vector<double>& r, int a, int b, int c) -> bool {
        return (std::abs(r[a] - r[b]) < IN_LINE && std::abs(r[a] - r[c]) < IN_LINE) ||
               (std::abs(r[a + 1] - r[b + 1]) < IN_LINE && std::abs(r[a + 1] - r[c + 1]) < IN_LINE);
    };
    for (size_t ei = 0; ei < edges.size(); ei++) {
        const auto& r = edges[ei].route;
        int rlen = static_cast<int>(r.size());
        if (rlen < 6) continue;
        int k = 3;
        while (k + 3 < rlen && inLine(r, 0, k, k + 3)) k += 3;
        int last = rlen - 3;
        int j = last - 3;
        while (j >= 3 && inLine(r, last, j, j - 3)) j -= 3;
        size_t o = ei * 8;
        runs[o] = r[0];
        runs[o + 1] = r[1];
        runs[o + 2] = r[k];
        runs[o + 3] = r[k + 1];
        runs[o + 4] = r[j];
        runs[o + 5] = r[j + 1];
        runs[o + 6] = r[last];
        runs[o + 7] = r[last + 1];
    }
    auto onRun = [&](int ei, int end, double x, double y) -> bool {
        int o = ei * 8 + (end == 1 ? 0 : 4);
        double x0 = runs[o];
        double y0 = runs[o + 1];
        double dx = runs[o + 2] - x0;
        double dy = runs[o + 3] - y0;
        double length = dx * dx + dy * dy;
        double t = length > 0.0 ? std::max(0.0, std::min(1.0, ((x - x0) * dx + (y - y0) * dy) / length)) : 0.0;
        return std::hypot(x - x0 - t * dx, y - y0 - t * dy) < IN_LINE;
    };

    int total = 0;
    for (int band = 0; band < bands; band++) {
        int bandEnd = bandStart[band + 1];
        if (bandEnd - bandStart[band] > MAX_INSERT) {
            std::sort(inBand.begin() + bandStart[band], inBand.begin() + bandEnd, [&](int p, int q) {
                if (ax[p] != ax[q]) return ax[p] < ax[q];
                return p < q;
            });
        } else {
            for (int i = bandStart[band] + 1; i < bandEnd; i++) {
                int p = inBand[i];
                int j = i;
                for (; j > bandStart[band] && ax[inBand[j - 1]] > ax[p]; j--) inBand[j] = inBand[j - 1];
                inBand[j] = p;
            }
        }
        for (int s = bandStart[band]; s < bandEnd; s++) {
            int p = inBand[s];
            double dx = bx[p] - ax[p];
            double dy = by[p] - ay[p];
            bool here = first[p] == band;
            for (int t = s + 1; t < bandEnd; t++) {
                int q = inBand[t];
                if (ax[q] > bx[p]) break;
                if (high[q] < low[p] || low[q] > high[p] || owner[p] == owner[q] || (!here && first[q] != band)) continue;
                double d1 = dx * (ay[q] - ay[p]) - dy * (ax[q] - ax[p]);
                double d2 = dx * (by[q] - ay[p]) - dy * (bx[q] - ax[p]);
                if ((d1 > 0.0) == (d2 > 0.0)) continue;
                double ex = bx[q] - ax[q];
                double ey = by[q] - ay[q];
                double d3 = ex * (ay[p] - ay[q]) - ey * (ax[p] - ax[q]);
                double d4 = ex * (by[p] - ay[q]) - ey * (bx[p] - ax[q]);
                if ((d3 > 0.0) == (d4 > 0.0)) continue;
                const auto& ep = edges[owner[p]];
                const auto& eq = edges[owner[q]];
                double at = d3 / (d3 - d4);
                double x = ax[p] + at * dx;
                double y = ay[p] + at * dy;
                auto fan = [&](int v, int pe, int qe) -> bool {
                    return ((ends[p] & pe) != 0 && (ends[q] & qe) != 0) ||
                           (std::abs(x - nodes[v].x) < nodes[v].w / 2.0 + FAN && std::abs(y - nodes[v].y) < nodes[v].h / 2.0 + FAN) ||
                           (onRun(owner[p], pe, x, y) && onRun(owner[q], qe, x, y));
                };
                if (ep.src == eq.src && fan(ep.src, 1, 1)) continue;
                if (ep.src == eq.dst && fan(ep.src, 1, 2)) continue;
                if (ep.dst == eq.src && fan(ep.dst, 2, 1)) continue;
                if (ep.dst == eq.dst && fan(ep.dst, 2, 2)) continue;
                total++;
            }
        }
    }
    return total;
}

inline void inheritCrossedDirections(std::vector<CNode>& nodes, const std::vector<std::pair<int, int>>& ends) noexcept {
    auto ancestors = [&](int i) -> Pele::FlatSet<int> {
        Pele::FlatSet<int> set;
        for (int p = nodes[i].parent; p >= 0 && !set.count(p); p = nodes[p].parent) set.insert(p);
        return set;
    };
    for (const auto& [src, dst] : ends) {
        if (src < 0 || dst < 0 || src == dst) continue;
        auto a = ancestors(src);
        auto b = ancestors(dst);
        for (int g : a) if (!b.count(g)) nodes[g].dir.clear();
        for (int g : b) if (!a.count(g)) nodes[g].dir.clear();
    }
}

inline void shiftLayout(std::vector<CNode>& nodes, std::vector<CEdge>& edges, double ox, double oy) noexcept {
    for (auto& c : nodes) {
        c.x += ox;
        c.y += oy;
    }
    for (auto& e : edges) {
        for (size_t k = 0; k < e.route.size(); k += 3) {
            e.route[k] += ox;
            e.route[k + 1] += oy;
        }
        e.labelX += ox;
        e.labelY += oy;
    }
}

inline CNode cnode(double w, double h, int parent = -1) noexcept {
    CNode c;
    c.w = w;
    c.h = h;
    c.parent = parent;
    return c;
}

inline CompoundResult compoundLayout(std::vector<CNode>& nodes, std::vector<CEdge>& edges, const std::string& rootDir, const LayeredOptions& opt) noexcept {
    using namespace detail;
    int n = static_cast<int>(nodes.size());
    std::vector<Level> order;
    Pele::FlatMap<int, int> levelIdxOf; // level.id -> index in order
    auto makeLevel = [&](int id, const std::string& dir) -> int {
        int idx = static_cast<int>(order.size());
        Level lv;
        lv.id = id;
        lv.dir = dir;
        order.push_back(std::move(lv));
        levelIdxOf[id] = idx;
        return idx;
    };
    makeLevel(-1, rootDir);

    std::vector<std::vector<int>> children(n);
    std::vector<int> rootItems;
    for (int i = 0; i < n; i++) {
        int p = nodes[i].parent;
        if (p >= 0 && p < n && p != i && nodes[p].isGroup) children[p].push_back(i);
        else {
            nodes[i].parent = -1;
            rootItems.push_back(i);
        }
    }
    order[0].items = std::move(rootItems);
    std::vector<int> depth(n, 0);
    std::vector<uint8_t> placed(n, 0);
    auto descend = [&](size_t from) {
        for (size_t q = from; q < order.size(); q++) {
            int lvId = order[q].id;
            std::string lvDir = order[q].dir;
            std::vector<int> items = order[q].items;
            for (int item : items) {
                placed[item] = 1;
                depth[item] = lvId < 0 ? 0 : depth[lvId] + 1;
                if (nodes[item].isGroup) {
                    int childIdx = makeLevel(item, !nodes[item].dir.empty() ? nodes[item].dir : lvDir);
                    order[childIdx].items = children[item];
                }
            }
        }
    };
    descend(0);
    for (int i = 0; i < n; i++) {
        if (placed[i]) continue;
        int former = nodes[i].parent;
        if (former >= 0 && former < n) {
            auto& ch = children[former];
            ch.erase(std::remove(ch.begin(), ch.end(), i), ch.end());
        }
        nodes[i].parent = -1;
        order[0].items.push_back(i);
        placed[i] = 1;
        depth[i] = 0;
        if (nodes[i].isGroup) {
            size_t from = order.size();
            int childIdx = makeLevel(i, !nodes[i].dir.empty() ? nodes[i].dir : rootDir);
            order[childIdx].items = children[i];
            descend(from);
        }
    }

    for (int q = static_cast<int>(order.size()) - 1; q > 0; q--) {
        const auto& level = order[q];
        int seq = std::numeric_limits<int>::max();
        for (int item : level.items) if (nodes[item].seq < seq) seq = nodes[item].seq;
        if (seq != std::numeric_limits<int>::max()) nodes[level.id].seq = seq;
    }
    for (auto& level : order) {
        std::sort(level.items.begin(), level.items.end(), [&](int a, int b) {
            if (nodes[a].seq != nodes[b].seq) return nodes[a].seq < nodes[b].seq;
            return a < b;
        });
    }

    int ports = 0;
    for (auto& e : edges) {
        int a = e.src;
        int b = e.dst;
        int crossed = 0;
        for (; depth[a] > depth[b]; crossed++) a = nodes[a].parent;
        for (; depth[b] > depth[a]; crossed++) b = nodes[b].parent;
        for (; a != b && nodes[a].parent != nodes[b].parent; crossed += 2) {
            a = nodes[a].parent;
            b = nodes[b].parent;
        }
        if (a == b) continue;
        if (ports + crossed > MAX_PORTS) {
            e.src = a;
            e.dst = b;
        } else {
            ports += crossed;
        }
    }

    std::vector<int> loops;
    std::vector<int> stray;
    for (int ei = 0; ei < static_cast<int>(edges.size()); ei++) {
        auto& e = edges[ei];
        e.route.clear();
        if (e.src == e.dst) {
            loops.push_back(ei);
            continue;
        }
        int a = e.src;
        int b = e.dst;
        std::vector<int> aPath;
        std::vector<int> bPath;
        while (depth[a] > depth[b]) {
            aPath.push_back(a);
            a = nodes[a].parent;
        }
        while (depth[b] > depth[a]) {
            bPath.push_back(b);
            b = nodes[b].parent;
        }
        if (a == b) {
            stray.push_back(ei);
            continue;
        }
        while (nodes[a].parent != nodes[b].parent) {
            aPath.push_back(a);
            bPath.push_back(b);
            a = nodes[a].parent;
            b = nodes[b].parent;
        }
        order[levelIdxOf.at(nodes[a].parent)].edges.push_back({ei, a, b});
        for (int pass = 0; pass < 2; pass++) {
            const auto& path = pass == 0 ? aPath : bPath;
            bool out = pass == 0;
            for (int inner : path) {
                auto& level = order[levelIdxOf.at(nodes[inner].parent)];
                int pIdx = static_cast<int>(level.ports.size());
                PortEdge port;
                port.e = ei;
                port.inner = inner;
                port.out = out;
                port.side = Side::Top;
                port.hint = std::numeric_limits<double>::quiet_NaN();
                level.ports.push_back(std::move(port));
                level.portIdxOf[ei * 2 + (out ? 0 : 1)] = pIdx;
            }
        }
    }

    std::vector<int> local(n, 0);
    auto endpointBelow = [&](int item, const CEdge& e, bool out) -> bool {
        return (out ? e.src : e.dst) != item;
    };

    bool ordered = ports > 1 && ports <= MAX_ORDERED_PORTS && static_cast<int>(edges.size()) <= MAX_ORDERED_EDGES;

    std::vector<double> key(n, std::numeric_limits<double>::quiet_NaN());
    bool keyed = false;
    int flatCrossings = 0;
    bool hasFarHint = false;
    std::vector<int> flatLedgeOf;
    std::vector<LEdge> flatLedges;
    std::vector<int> flatLast;
    bool flat = order.size() > 1 && ordered;
    for (const auto& level : order) if (level.dir != rootDir) flat = false;
    if (flat) {
        bool vertical = rootDir == "TB" || rootDir == "BT";
        std::vector<int> leaf(n, -1);
        std::vector<int> group(n, -1);
        std::vector<LNode> lnodes;
        std::vector<int> of;
        std::vector<int> parent;
        for (int i = 0; i < n; i++) {
            if (!nodes[i].isGroup) continue;
            group[i] = static_cast<int>(parent.size());
            parent.push_back(-1);
        }
        for (int i = 0; i < n; i++) {
            int p = nodes[i].parent;
            if (nodes[i].isGroup) parent[group[i]] = p >= 0 ? group[p] : -1;
            else {
                leaf[i] = static_cast<int>(lnodes.size());
                lnodes.push_back(lnode(vertical ? nodes[i].w : nodes[i].h, vertical ? nodes[i].h : nodes[i].w));
                of.push_back(p >= 0 ? group[p] : -1);
            }
        }
        flatLedgeOf.assign(edges.size(), -1);
        for (size_t ei = 0; ei < edges.size(); ei++) {
            const auto& e = edges[ei];
            if (e.src == e.dst || leaf[e.src] < 0 || leaf[e.dst] < 0) continue;
            flatLedgeOf[ei] = static_cast<int>(flatLedges.size());
            flatLedges.push_back(ledge(leaf[e.src], leaf[e.dst], e.minlen, vertical ? e.labelW : e.labelH, vertical ? e.labelH : e.labelW));
        }
        LayeredOptions flatOpt = opt;
        flatOpt.hasGroups = true;
        flatOpt.groupsOf = std::move(of);
        flatOpt.groupsParent = std::move(parent);
        flatOpt.orderOnly = true;
        LayeredResult flatLayout = layered(lnodes, flatLedges, flatOpt);
        flatCrossings = std::min(flatLayout.crossings, flatLayout.declared);
        std::vector<double> sum(n, 0.0);
        std::vector<int> count(n, 0);
        flatLast.assign(n, -1);
        for (int i = 0; i < n; i++) {
            if (leaf[i] < 0) continue;
            key[i] = lnodes[leaf[i]].x;
            for (int p = nodes[i].parent; p >= 0; p = nodes[p].parent) {
                sum[p] += key[i];
                count[p]++;
                flatLast[p] = std::max(flatLast[p], lnodes[leaf[i]].rank);
            }
        }
        for (int i = 0; i < n; i++) if (count[i] > 0) key[i] = sum[i] / count[i];
        hasFarHint = true;
    }

    auto farHint = [&](const Level& level, const PortEdge& port) -> double {
        int le = flatLedgeOf[port.e];
        if (le < 0 || level.id < 0 || !port.out || port.side != opposite(startSide(level.dir))) return std::numeric_limits<double>::quiet_NaN();
        const auto& points = flatLedges[le].points;
        double inside = key[edges[port.e].src];
        double outside = 0.0;
        for (size_t k = 0; k < points.size(); k += 2) {
            if (points[k] == flatLast[level.id]) inside = points[k + 1];
            if (points[k] == flatLast[level.id] + 1) outside = points[k + 1];
        }
        return inside + outside * 1e-6;
    };

    struct Built {
        std::vector<LNode> lnodes;
        std::vector<LEdge> ledges;
        std::vector<int> dummies;
    };

    auto build = [&](const Level& level, bool sized) -> Built {
        bool vertical = level.dir == "TB" || level.dir == "BT";
        Side start = startSide(level.dir);
        std::vector<LNode> lnodes;
        std::vector<LEdge> ledges;
        lnodes.reserve(level.items.size() + level.ports.size());
        for (size_t i = 0; i < level.items.size(); i++) {
            int item = level.items[i];
            local[item] = static_cast<int>(i);
            const auto& node = nodes[item];
            LNode ln = sized ? lnode(vertical ? node.w : node.h, vertical ? node.h : node.w) : lnode(0.0, 0.0);
            ln.span = node.isGroup ? std::min(node.span, ln.w - 2.0 * node.padX) : node.span;
            ln.key = keyed ? key[item] : std::numeric_limits<double>::quiet_NaN();
            lnodes.push_back(ln);
        }
        auto held = [&](int item, int ei, bool out) -> bool {
            return nodes[item].isGroup && endpointBelow(item, edges[ei], out);
        };
        auto offset = [&](int item, int ei, bool out) -> double {
            if (!sized || !nodes[item].isGroup || !endpointBelow(item, edges[ei], out)) return 0.0;
            const auto& childLv = order[levelIdxOf.at(item)];
            const auto& port = childLv.ports[childLv.portIdxOf.at(ei * 2 + (out ? 0 : 1))];
            return vertical ? port.px - nodes[item].w / 2.0 : port.py - nodes[item].h / 2.0;
        };
        for (const auto& le : level.edges) {
            const auto& e = edges[le.e];
            LEdge edge = ledge(local[le.a], local[le.b], e.minlen);
            if (sized) {
                edge.labelW = vertical ? e.labelW : e.labelH;
                edge.labelH = vertical ? e.labelH : e.labelW;
                edge.tailDx = offset(le.a, le.e, true);
                edge.headDx = offset(le.b, le.e, false);
                edge.tailFixed = held(le.a, le.e, true);
                edge.headFixed = held(le.b, le.e, false);
            }
            ledges.push_back(std::move(edge));
        }
        std::vector<int> dummies(level.ports.size(), -1);
        for (size_t i = 0; i < level.ports.size(); i++) {
            const auto& port = level.ports[i];
            if (port.side == start || port.side == opposite(start)) {
                int d = static_cast<int>(lnodes.size());
                LNode ln = lnode(0.0, 0.0, port.side == start ? LKind::StartPort : LKind::EndPort);
                ln.key = keyed ? key[port.out ? edges[port.e].dst : edges[port.e].src] : std::numeric_limits<double>::quiet_NaN();
                ln.hint = (std::isnan(port.hint) && keyed && hasFarHint) ? farHint(level, port) : port.hint;
                lnodes.push_back(ln);
                dummies[i] = d;
                LEdge edge = port.out ? ledge(local[port.inner], d) : ledge(d, local[port.inner]);
                if (port.out) {
                    edge.tailDx = offset(port.inner, port.e, true);
                    edge.tailFixed = held(port.inner, port.e, true);
                } else {
                    edge.headDx = offset(port.inner, port.e, false);
                    edge.headFixed = held(port.inner, port.e, false);
                }
                ledges.push_back(std::move(edge));
            } else {
                lnodes[local[port.inner]].pin = (port.side == Side::Top || port.side == Side::Left) ? -1 : 1;
            }
        }
        return {std::move(lnodes), std::move(ledges), std::move(dummies)};
    };

    struct LaidEntry {
        std::vector<double> given;
        std::vector<LNode> lnodes;
        std::vector<LEdge> ledges;
        LayeredResult size;
    };
    std::unordered_map<int, std::vector<LaidEntry>> laid;
    auto layOut = [&](const Level& level, std::vector<LNode> lnodes, std::vector<LEdge> ledges) -> LaidEntry {
        if (!ordered) {
            LayeredResult sz = layered(lnodes, ledges, opt);
            return {{}, std::move(lnodes), std::move(ledges), sz};
        }
        std::vector<double> given;
        given.reserve(lnodes.size() * 7 + ledges.size() * 9);
        for (const auto& ln : lnodes) {
            given.push_back(ln.w);
            given.push_back(ln.h);
            given.push_back(static_cast<double>(ln.kind));
            given.push_back(static_cast<double>(ln.pin));
            given.push_back(ln.hint);
            given.push_back(ln.key);
            given.push_back(ln.span);
        }
        for (const auto& le : ledges) {
            given.push_back(static_cast<double>(le.tail));
            given.push_back(static_cast<double>(le.head));
            given.push_back(static_cast<double>(le.minlen));
            given.push_back(le.labelW);
            given.push_back(le.labelH);
            given.push_back(le.tailDx);
            given.push_back(le.headDx);
            given.push_back(le.tailFixed ? 1.0 : 0.0);
            given.push_back(le.headFixed ? 1.0 : 0.0);
        }
        auto& past = laid[level.id];
        for (const auto& p : past) {
            bool same = p.given.size() == given.size();
            for (size_t i = 0; same && i < given.size(); i++) same = objectIs(p.given[i], given[i]);
            if (same) return p;
        }
        LayeredResult sz = layered(lnodes, ledges, opt);
        LaidEntry done{std::move(given), std::move(lnodes), std::move(ledges), sz};
        past.push_back(done);
        return done;
    };

    CompoundResult total{0.0, 0.0};
    auto place = [&]() {
        for (auto& level : order) {
            for (const auto& port : level.ports) {
                if (nodes[port.inner].isGroup && endpointBelow(port.inner, edges[port.e], port.out)) {
                    order[levelIdxOf.at(port.inner)].getPort(port.e * 2 + (port.out ? 0 : 1)).side = port.side;
                }
            }
            if (level.edges.empty()) continue;
            bool needed = false;
            for (const auto& le : level.edges) {
                const auto& e = edges[le.e];
                if ((nodes[le.a].isGroup && e.src != le.a) || (nodes[le.b].isGroup && e.dst != le.b)) needed = true;
            }
            if (!needed) continue;
            auto built = build(level, false);
            orient(built.lnodes, built.ledges);
            rank(built.lnodes, built.ledges, 1);
            Side start = startSide(level.dir);
            for (size_t i = 0; i < level.edges.size(); i++) {
                const auto& le = level.edges[i];
                const auto& e = edges[le.e];
                bool aAbove = !built.ledges[i].reversed;
                if (nodes[le.a].isGroup && e.src != le.a) {
                    order[levelIdxOf.at(le.a)].getPort(le.e * 2).side = aAbove ? opposite(start) : start;
                }
                if (nodes[le.b].isGroup && e.dst != le.b) {
                    order[levelIdxOf.at(le.b)].getPort(le.e * 2 + 1).side = aAbove ? start : opposite(start);
                }
            }
        }

        for (int q = static_cast<int>(order.size()) - 1; q >= 0; q--) {
            auto& level = order[q];
            CNode* group = level.id >= 0 ? &nodes[level.id] : nullptr;
            bool vertical = level.dir == "TB" || level.dir == "BT";
            bool flip = level.dir == "BT" || level.dir == "RL";
            auto built = build(level, true);
            auto res = layOut(level, std::move(built.lnodes), std::move(built.ledges));
            const auto& lnodes = res.lnodes;
            const auto& ledges = res.ledges;
            const auto& size = res.size;
            const auto& dummies = built.dummies;
            double contentW = vertical ? size.width : size.height;
            double contentH = vertical ? size.height : size.width;
            double padX = group ? group->padX : 0.0;
            double padTop = group ? group->padTop : 0.0;
            double boxW = std::max(contentW + 2.0 * padX, group ? group->minW : 0.0);
            double boxH = contentH + padTop + (group ? group->padBottom : 0.0);
            double offX = (boxW - contentW) / 2.0;
            double offY = padTop;
            auto tx = [&](double lx, double ly) -> double {
                return offX + (vertical ? lx : (flip ? size.height - ly : ly));
            };
            auto ty = [&](double lx, double ly) -> double {
                return offY + (vertical ? (flip ? size.height - ly : ly) : lx);
            };
            double axis = vertical ? 0.0 : 1.0;
            auto convert = [&](const std::vector<double>& points) -> std::vector<double> {
                std::vector<double> out;
                out.reserve((points.size() / 2) * 3);
                for (size_t i = 0; i < points.size(); i += 2) {
                    out.push_back(tx(points[i], points[i + 1]));
                    out.push_back(ty(points[i], points[i + 1]));
                    out.push_back(axis);
                }
                return out;
            };

            for (size_t i = 0; i < level.items.size(); i++) {
                int item = level.items[i];
                nodes[item].x = tx(lnodes[i].x, lnodes[i].y);
                nodes[item].y = ty(lnodes[i].x, lnodes[i].y);
            }
            for (size_t i = 0; i < level.edges.size(); i++) {
                const auto& le = level.edges[i];
                const auto& edge = ledges[i];
                level.routes[le.e] = convert(edge.points);
                auto& e = edges[le.e];
                if (e.labelW > 0.0) {
                    e.labelX = tx(edge.labelX, edge.labelY);
                    e.labelY = ty(edge.labelX, edge.labelY);
                }
            }
            size_t k = level.edges.size();
            for (size_t i = 0; i < level.ports.size(); i++) {
                auto& port = level.ports[i];
                const auto& item = nodes[port.inner];
                if (dummies[i] >= 0) {
                    std::vector<double> route = convert(ledges[k++].points);
                    size_t at = port.out ? route.size() - 3 : 0;
                    double bx = route[at];
                    double by = route[at + 1];
                    if (port.side == Side::Top) by = 0.0;
                    else if (port.side == Side::Bottom) by = boxH;
                    else if (port.side == Side::Left) bx = 0.0;
                    else bx = boxW;
                    if (port.out) {
                        route.push_back(bx);
                        route.push_back(by);
                        route.push_back(axis);
                    } else {
                        route.insert(route.begin(), {bx, by, axis});
                    }
                    port.px = bx;
                    port.py = by;
                    port.route = std::move(route);
                    continue;
                }
                double ix = item.x;
                double iy = item.y;
                if (item.isGroup && endpointBelow(port.inner, edges[port.e], port.out)) {
                    const auto& inner = order[levelIdxOf.at(port.inner)].getPort(port.e * 2 + (port.out ? 0 : 1));
                    ix += inner.px - item.w / 2.0;
                    iy += inner.py - item.h / 2.0;
                } else if (port.side == Side::Top) iy -= item.h / 2.0;
                else if (port.side == Side::Bottom) iy += item.h / 2.0;
                else if (port.side == Side::Left) ix -= item.w / 2.0;
                else ix += item.w / 2.0;
                double bx = ix;
                double by = iy;
                if (port.side == Side::Top) by = 0.0;
                else if (port.side == Side::Bottom) by = boxH;
                else if (port.side == Side::Left) bx = 0.0;
                else bx = boxW;
                double a = (port.side == Side::Top || port.side == Side::Bottom) ? 0.0 : 1.0;
                port.px = bx;
                port.py = by;
                if (port.out) port.route = {ix, iy, a, bx, by, a};
                else port.route = {bx, by, a, ix, iy, a};
            }

            if (group) {
                group->w = boxW;
                group->h = boxH;
            } else {
                total = {boxW, boxH};
            }
        }

        for (size_t q = 0; q < order.size(); q++) {
            double ox = order[q].x;
            double oy = order[q].y;
            for (int item : order[q].items) {
                auto& node = nodes[item];
                node.x += ox;
                node.y += oy;
                if (node.isGroup) {
                    auto& child = order[levelIdxOf.at(item)];
                    child.x = node.x - node.w / 2.0;
                    child.y = node.y - node.h / 2.0;
                }
            }
        }

        auto append = [](std::vector<double>& route, const std::vector<double>& part, double ox, double oy) {
            for (size_t i = 0; i < part.size(); i += 3) {
                double x = part[i] + ox;
                double y = part[i + 1] + oy;
                size_t len = route.size();
                if (len >= 3 && std::abs(route[len - 3] - x) < 0.01 && std::abs(route[len - 2] - y) < 0.01) continue;
                route.push_back(x);
                route.push_back(y);
                route.push_back(part[i + 2]);
            }
        };

        for (const auto& level : order) {
            for (const auto& le : level.edges) {
                auto& e = edges[le.e];
                std::vector<double> route;
                std::vector<int> outward;
                for (int item = e.src; nodes[item].parent != level.id; item = nodes[item].parent) {
                    outward.push_back(levelIdxOf.at(nodes[item].parent));
                }
                for (int lvIdx : outward) {
                    const auto& lv = order[lvIdx];
                    append(route, lv.ports[lv.portIdxOf.at(le.e * 2)].route, lv.x, lv.y);
                }
                append(route, level.routes.at(le.e), level.x, level.y);
                std::vector<int> inward;
                for (int item = e.dst; nodes[item].parent != level.id; item = nodes[item].parent) {
                    inward.push_back(levelIdxOf.at(nodes[item].parent));
                }
                for (int i = static_cast<int>(inward.size()) - 1; i >= 0; i--) {
                    const auto& lv = order[inward[i]];
                    append(route, lv.ports[lv.portIdxOf.at(le.e * 2 + 1)].route, lv.x, lv.y);
                }
                e.route = std::move(route);
                if (e.labelW > 0.0) {
                    e.labelX += level.x;
                    e.labelY += level.y;
                }
            }
        }
    };

    RouteStyle style = styleOf(opt);
    keyed = flat;
    place();
    int crossings = -1;
    if (flat) {
        crossings = routeCrossings(nodes, edges, style);
        if (crossings > flatCrossings * FLAT_SLACK + FLAT_SPARE) {
            Snapshot first = snapshot(nodes, edges, order, total);
            keyed = false;
            place();
            int own = routeCrossings(nodes, edges, style);
            if (own >= crossings) {
                keyed = true;
                total = restore(first, nodes, edges, order);
            } else {
                crossings = own;
            }
        }
    }

    if (keyed == flat && ordered) {
        bool again = false;
        for (auto& level : order) {
            if (level.id < 0) continue;
            const auto& box = nodes[level.id];
            bool vertical = level.dir == "TB" || level.dir == "BT";
            Side end = opposite(startSide(level.dir));
            std::vector<std::pair<double, double>> seen;
            for (auto& port : level.ports) {
                if (port.side != end) continue;
                const auto& route = edges[port.e].route;
                double bx = level.x + port.px;
                double by = level.y + port.py;
                int at = -1;
                int rlen = static_cast<int>(route.size());
                for (int k = 0; k < rlen && (at < 0 || port.out); k += 3) {
                    if (std::abs(route[k] - bx) < 0.01 && std::abs(route[k + 1] - by) < 0.01) at = k;
                }
                if (at < 0) continue;
                int a = vertical ? 0 : 1;
                int next = port.out ? at + 3 : at - 3;
                while (next >= 0 && next < rlen && std::abs(route[next + a] - route[at + a]) < IN_LINE) next += port.out ? 3 : -3;
                if (next < 0 || next >= rlen) continue;
                port.hint = route[next + a] - (vertical ? box.x - box.w / 2.0 : box.y - box.h / 2.0);
                seen.push_back({vertical ? port.px : port.py, port.hint});
            }
            std::stable_sort(seen.begin(), seen.end(), [](const auto& p, const auto& q) {
                return p.first < q.first;
            });
            for (size_t i = 1; i < seen.size(); i++) if (seen[i - 1].second > seen[i].second) again = true;
        }
        if (again) {
            int before = crossings >= 0 ? crossings : routeCrossings(nodes, edges, style);
            if (before > 0) {
                Snapshot first = snapshot(nodes, edges, order, total);
                place();
                if (routeCrossings(nodes, edges, style) >= before) total = restore(first, nodes, edges, order);
            }
        }
    }

    for (int ei : stray) {
        auto& e = edges[ei];
        const auto& a = nodes[e.src];
        const auto& b = nodes[e.dst];
        e.route = {a.x, a.y, 0.0, b.x, b.y, 0.0};
        e.labelX = (a.x + b.x) / 2.0;
        e.labelY = (a.y + b.y) / 2.0;
    }
    for (int ei : loops) {
        auto& e = edges[ei];
        const auto& node = nodes[e.src];
        e.labelX = node.x + node.w / 2.0 + 28.0 + e.labelW / 2.0;
        e.labelY = node.y;
    }

    return total;
}

// ============================================================================
// Port of SVG, Cluster, Edge, URL, and Fit-Width helpers for Flowchart
// ============================================================================

inline int endSide(const std::vector<double>& route, size_t at, const CNode& c) noexcept {
    double dx = route[at] - c.x;
    double dy = route[at + 1] - c.y;
    if (std::abs(std::abs(dy) - c.h / 2.0) < 0.5) return dy < 0.0 ? 0 : 2;
    if (std::abs(std::abs(dx) - c.w / 2.0) < 0.5) return dx < 0.0 ? 3 : 1;
    return -1;
}

template <typename ViewT>
inline void insetRoute(std::vector<double>& route, size_t at, const ViewT& view, const CNode& c) noexcept {
    if (c.isGroup) return;
    int side = endSide(route, at, c);
    if (side == 0 || side == 2) {
        double amount = shapeInset(view.shape, view.w, view.h, side, route[at] - c.x) + (c.h - view.h) / 2.0;
        route[at + 1] += side == 0 ? amount : -amount;
    } else if (side >= 0) {
        double amount = shapeInset(view.shape, view.w, view.h, side, route[at + 1] - c.y) + (c.w - view.w) / 2.0;
        route[at] += side == 3 ? amount : -amount;
    }
}

using Crossings = std::unordered_map<int, std::vector<double>>;

inline void markCrossings(Crossings& crossings, const std::vector<double>& route) noexcept {
    for (size_t k = 0; k < route.size(); k += 3) {
        int key = static_cast<int>(std::round(route[k + 1]));
        crossings[key].push_back(route[k]);
    }
}

inline double clusterTitleX(const CNode& c, double width, const Crossings& crossings, double pad) noexcept {
    if (width == 0.0) return c.x;
    double top = c.y - c.h / 2.0;
    double left = c.x - c.w / 2.0;
    double half = width / 2.0;
    double candidates[3] = {left + pad - 6.0 + half, c.x, left + c.w - pad + 6.0 - half};
    auto it = crossings.find(static_cast<int>(std::round(top)));
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
    auto it = crossings.find(static_cast<int>(std::round(c.y - c.h / 2.0)));
    if (it == crossings.end()) return false;
    double left = c.x - c.w / 2.0;
    for (double cx : it->second) {
        if (cx > left && cx < left + c.w && std::abs(cx - x) < width / 2.0 + 4.0) return true;
    }
    return false;
}

inline std::string struckTitle(
    const Label& label,
    double x,
    double y,
    const std::string& attrs,
    const std::string& id,
    const std::vector<CNode>& nodes,
    size_t index,
    const IconResolver& icons = nullptr
) {
    int depth = 1;
    for (int p = nodes[index].parent; p >= 0 && depth < 8; p = nodes[p].parent) depth++;
    double pct = 100.0 * (1.0 - std::pow(0.5, depth));
    std::string fill = Pele::cat({"color-mix(in srgb,var(--_a) ", num(pct), "%,var(--_bg))"});
    return Pele::cat({"<g class=\"pele-cluster-title\" data-id=\"", id, "\">", labelSvg(label,  x,  y, Pele::cat({" fill=\"none\" stroke=\"", fill, "\" stroke-width=\"5\" stroke-linejoin=\"round\""}),  icons), labelSvg(label, x, y, attrs, icons), "</g>"});
}

inline EdgePath loopPath(
    const CNode& c,
    double w,
    double h,
    int k,
    bool sideways,
    double base,
    double trim,
    CEdge& e
) {
    double half = (sideways ? w : h) / 2.0;
    double spread = std::min(half - 4.0, 8.0 + k * 6.0);
    double reach = base + k * 8.0;
    double len = std::hypot(reach, spread);
    if (len == 0.0) len = 1.0;
    auto at = [&](double out, double along) -> std::string {
        return sideways
            ? Pele::cat({num(c.x + along), ",", num(c.y + h / 2.0 + out)})
            : Pele::cat({num(c.x + w / 2.0 + out), ",", num(c.y + along)});
    };
    double tx = (reach / len) * trim;
    double ty = (spread / len) * trim;
    if (sideways) {
        e.labelX = c.x;
        e.labelY = c.y + h / 2.0 + reach + 4.0 + e.labelH / 2.0;
    } else {
        e.labelX = c.x + w / 2.0 + reach + 4.0 + e.labelW / 2.0;
        e.labelY = c.y;
    }
    EdgePath ep;
    ep.d = Pele::cat({"M", at(0.0, -spread), "C", at(reach, -spread * 2.0), " ", at(reach, spread * 2.0), " ", at(tx, spread + ty)});
    ep.sx = sideways ? c.x - spread : c.x + w / 2.0;
    ep.sy = sideways ? c.y + h / 2.0 : c.y - spread;
    ep.sdx = sideways ? 0.0 : -1.0;
    ep.sdy = sideways ? -1.0 : 0.0;
    ep.ex = sideways ? c.x + spread : c.x + w / 2.0;
    ep.ey = sideways ? c.y + h / 2.0 : c.y + spread;
    ep.edx = sideways ? -spread / len : -reach / len;
    ep.edy = sideways ? -reach / len : -spread / len;
    return ep;
}

inline double titleRoomExact(const RenderOptions& options) noexcept {
    double available = options.maxWidth;
    double pad = options.hasPadding ? options.padding : 8.0;
    return (options.hasMaxWidth && available > 0.0) ? std::max(80.0, available - 2.0 * pad) : 4000.0;
}

inline bool runsAcross(const std::string& dir) noexcept {
    return dir == "LR" || dir == "RL";
}

template <typename DrawFn>
inline Rendered turnToFit(const RenderOptions& options, DrawFn&& draw) noexcept {
    Rendered across = draw(false);
    double available = options.maxWidth;
    if (!options.hasMaxWidth || !(available > 0.0) || across.width <= available) return across;
    if (available >= 640.0) return across;
    Rendered down = draw(true);
    return down.width < across.width ? down : across;
}

template <typename DrawFn>
inline Rendered tightenExact(const RenderOptions& options, DrawFn&& draw) noexcept {
    Rendered loose = draw(false);
    double available = options.maxWidth;
    if (!options.hasMaxWidth || !(available > 0.0) || loose.width <= available) return loose;
    Rendered tight = draw(true);
    return tight.width < loose.width ? tight : loose;
}

inline bool internalLink(const std::string& name, const RenderOptions& options) noexcept {
    std::string url = sanitizeUrl(name);
    if (url == "about:blank" || linkUrl(url, options).empty()) return false;
    std::string seen;
    size_t i = 0;
    while (i < name.size()) {
        size_t s = i;
        uint32_t cp = nextCodepoint(name, i);
        if (isInvalidXmlCodepoint(cp) || cp == '\t' || cp == '\n' || cp == '\r') continue;
        seen.append(name, s, i - s);
    }
    size_t lead = 0;
    while (lead < seen.size() && static_cast<unsigned char>(seen[lead]) <= 0x20) lead++;
    seen = seen.substr(lead);
    return !linkUrl(seen, options).empty();
}

// ============================================================================
// Port of src/diagrams/flowchart/shapes.ts
// ============================================================================

inline const Pele::FlatMap<std::string, std::string>& canonicalShapeMap() noexcept {
    static const Pele::FlatMap<std::string, std::string> m = []() {
        static const std::vector<std::vector<std::string>> SHAPES = {
            {"rect", "proc", "process", "rectangle", "squareRect"},
            {"rounded", "event", "roundedRect"},
            {"stadium", "terminal", "pill"},
            {"fr-rect", "subprocess", "subproc", "framed-rectangle", "subroutine"},
            {"cyl", "db", "database", "cylinder"},
            {"datastore", "data-store"},
            {"folder", "directory"},
            {"bucket"},
            {"console"},
            {"browser"},
            {"person"},
            {"circle", "circ"},
            {"bang"},
            {"cloud"},
            {"diam", "decision", "diamond", "question"},
            {"hex", "hexagon", "prepare"},
            {"lean-r", "lean-right", "in-out", "lean_right"},
            {"lean-l", "lean-left", "out-in", "lean_left"},
            {"trap-b", "priority", "trapezoid-bottom", "trapezoid"},
            {"trap-t", "manual", "trapezoid-top", "inv-trapezoid", "inv_trapezoid"},
            {"dbl-circ", "double-circle", "doublecircle"},
            {"text"},
            {"notch-rect", "card", "notched-rectangle"},
            {"lin-rect", "lined-rectangle", "lined-process", "lin-proc", "shaded-process"},
            {"sm-circ", "start", "small-circle", "stateStart"},
            {"fr-circ", "stop", "framed-circle", "stateEnd"},
            {"fork", "join", "forkJoin"},
            {"hourglass", "collate"},
            {"brace", "comment", "brace-l"},
            {"brace-r"},
            {"braces"},
            {"bolt", "com-link", "lightning-bolt"},
            {"doc", "document"},
            {"delay", "half-rounded-rectangle"},
            {"h-cyl", "das", "horizontal-cylinder"},
            {"lin-cyl", "disk", "lined-cylinder"},
            {"curv-trap", "curved-trapezoid", "display"},
            {"div-rect", "div-proc", "divided-rectangle", "divided-process"},
            {"tri", "extract", "triangle"},
            {"win-pane", "internal-storage", "window-pane"},
            {"f-circ", "junction", "filled-circle"},
            {"notch-pent", "loop-limit", "notched-pentagon"},
            {"flip-tri", "manual-file", "flipped-triangle"},
            {"sl-rect", "manual-input", "sloped-rectangle"},
            {"docs", "documents", "st-doc", "stacked-document"},
            {"st-rect", "procs", "processes", "stacked-rectangle"},
            {"bow-rect", "stored-data", "bow-tie-rectangle"},
            {"cross-circ", "summary", "crossed-circle"},
            {"tag-doc", "tagged-document"},
            {"tag-rect", "tagged-rectangle", "tag-proc", "tagged-process"},
            {"flag", "paper-tape"},
            {"odd", "rect_left_inv_arrow"},
            {"lin-doc", "lined-document"}
        };
        static const std::vector<std::string> INTERNAL = {
            "state", "choice", "note", "composite", "rectWithTitle", "labelRect",
            "block_arrow", "collapsedGroup", "iconSquare", "iconCircle", "icon",
            "iconRounded", "imageSquare", "anchor", "kanbanItem", "mindmapCircle",
            "defaultMindmapNode", "classBox", "erBox", "requirementBox",
            "usecaseActor", "usecaseActorHollow", "usecaseActorAwesome",
            "usecaseActorIcon", "usecaseBusiness", "usecaseEllipse", "usecaseJsonTable"
        };
        Pele::FlatMap<std::string, std::string> res;
        for (const auto& names : SHAPES) {
            for (const auto& name : names) res[name] = names[0];
        }
        for (const auto& name : INTERNAL) res[name] = name;
        return res;
    }();
    return m;
}

inline bool isValidShape(const std::string& name) noexcept {
    return canonicalShapeMap().count(name) > 0;
}

inline std::string canonicalShape(const std::string& name) noexcept {
    if (name.empty()) return "rect";
    if (name == "square") return "rect";
    if (name == "round") return "rounded";
    if (name == "ellipse") return "ellipse";
    if (name == "hexagon") return "hex";
    if (name == "diamond") return "diam";
    const auto& m = canonicalShapeMap();
    auto it = m.find(name);
    return it != m.end() ? it->second : "rect";
}

// ============================================================================
// Port of src/diagrams/flowchart/types.ts & db.ts
// ============================================================================

struct FlowText {
    std::string text;
    std::string type = "text"; // "text" | "string" | "markdown"
};

struct FlowNode {
    std::string id;
    std::string text;
    bool hasText = false;
    std::string labelType = "text";
    std::string type;
    bool hasType = false;
    std::vector<std::string> styles;
    std::vector<std::string> classes;
    std::string dir;
    std::map<std::string, std::string> props;
    bool hasProps = false;
    std::string link;
    std::string linkTarget;
    std::string icon;
    std::string form;
    std::string pos;
    std::string img;
    std::string constraint;
    double assetWidth = 0.0;
    bool hasAssetWidth = false;
    double assetHeight = 0.0;
    bool hasAssetHeight = false;
};

struct FlowEdge {
    std::string start;
    std::string end;
    std::string type;
    bool hasType = false;
    std::string text;
    std::string labelType = "text";
    std::string stroke;
    int length = 1;
    bool hasLength = false;
    std::vector<std::string> style;
    bool hasStyle = false;
    std::vector<std::string> classes;
    std::string id;
    bool isUserDefinedId = false;
    std::string interpolate;
    bool animate = false;
    bool hasAnimate = false;
    std::string animation;
};

struct FlowClassDef {
    std::string id;
    std::vector<std::string> styles;
    std::vector<std::string> textStyles;
};

struct FlowSubgraph {
    std::string id;
    std::vector<std::string> nodes;
    std::string title;
    std::vector<std::string> classes;
    std::string dir;
    std::string labelType = "markdown";
    YamlValue metadata = YamlValue::makeNull();
};

struct FlowLinkInfo {
    std::string type;
    std::string stroke;
    int length = 1;
    bool hasLength = false;
    FlowText text;
    bool hasText = false;
    std::string id;
    bool hasId = false;
};

struct DocItem {
    enum Kind { Str, Arr, Dir } kind = Str;
    std::string strVal;
    std::vector<std::string> arrVal;
    std::string dirVal;

    static DocItem makeStr(std::string s) {
        DocItem d;
        d.kind = Str;
        d.strVal = std::move(s);
        return d;
    }
    static DocItem makeArr(std::vector<std::string> a) {
        DocItem d;
        d.kind = Arr;
        d.arrVal = std::move(a);
        return d;
    }
    static DocItem makeDir(std::string dir) {
        DocItem d;
        d.kind = Dir;
        d.dirVal = std::move(dir);
        return d;
    }
};

struct FlowDbOptions {
    int maxEdges = 5000;
    bool hasMaxEdges = false;
    bool inheritDir = false;
    std::string direction;
};

inline std::string flowLabelType(const std::string& type) noexcept {
    return (type == "markdown" || type == "string" || type == "text") ? type : "markdown";
}

inline std::string flowUnquote(const std::string& text) noexcept {
    return (text.size() >= 2 && text.front() == '"' && text.back() == '"')
        ? text.substr(1, text.size() - 2)
        : text;
}

inline int flowCountChar(char ch, const std::string& str) noexcept {
    int count = 0;
    for (char c : str) if (c == ch) count++;
    return count;
}

inline FlowLinkInfo destructStartLink(const std::string& raw) noexcept {
    std::string str = trimStr(raw);
    std::string type = "arrow_open";
    if (!str.empty()) {
        switch (str[0]) {
            case '<':
                type = "arrow_point";
                str = str.substr(1);
                break;
            case 'x':
                type = "arrow_cross";
                str = str.substr(1);
                break;
            case 'o':
                type = "arrow_circle";
                str = str.substr(1);
                break;
        }
    }
    std::string stroke = "normal";
    if (str.find('=') != std::string::npos) stroke = "thick";
    if (str.find('.') != std::string::npos) stroke = "dotted";
    FlowLinkInfo info;
    info.type = type;
    info.stroke = stroke;
    return info;
}

inline FlowLinkInfo destructEndLink(const std::string& raw) noexcept {
    std::string str = trimStr(raw);
    std::string line = str.empty() ? "" : str.substr(0, str.size() - 1);
    std::string type = "arrow_open";
    if (!str.empty()) {
        switch (str.back()) {
            case 'x':
                type = "arrow_cross";
                if (str.front() == 'x') {
                    type = Pele::cat({"double_", type});
                    if (!line.empty()) line = line.substr(1);
                }
                break;
            case '>':
                type = "arrow_point";
                if (str.front() == '<') {
                    type = Pele::cat({"double_", type});
                    if (!line.empty()) line = line.substr(1);
                }
                break;
            case 'o':
                type = "arrow_circle";
                if (str.front() == 'o') {
                    type = Pele::cat({"double_", type});
                    if (!line.empty()) line = line.substr(1);
                }
                break;
        }
    }
    std::string stroke = "normal";
    int length = static_cast<int>(line.size()) - 1;
    if (!line.empty() && line.front() == '=') stroke = "thick";
    if (!line.empty() && line.front() == '~') stroke = "invisible";
    int dots = flowCountChar('.', line);
    if (dots > 0) {
        stroke = "dotted";
        length = dots;
    }
    FlowLinkInfo info;
    info.type = type;
    info.stroke = stroke;
    info.length = length;
    info.hasLength = true;
    return info;
}

inline std::string flowSafeUrl(const std::string& rawUrl) noexcept {
    std::string u = sanitizeUrl(rawUrl);
    if (u == "about:blank") return u;
    std::string normalized = replaceAllStr(u, "\\", "/");
    size_t nl = normalized.find('\n');
    std::string firstLine = nl == std::string::npos ? normalized : normalized.substr(0, nl);
    size_t lastColon = firstLine.rfind(':');
    if (lastColon == std::string::npos) return u;
    std::string protocol = trimStr(toLowerAscii(firstLine.substr(0, lastColon + 1)));
    if (protocol == "http:" || protocol == "https:") {
        size_t pos = lastColon + 1;
        while (pos < normalized.size() && normalized[pos] == '/') pos++;
        size_t hostStart = pos;
        while (pos < normalized.size() && normalized[pos] != '/' && normalized[pos] != '?' && normalized[pos] != '#') pos++;
        if (pos == hostStart) return "about:blank";
        std::string host = toLowerAscii(normalized.substr(hostStart, pos - hostStart));
        std::string rest = normalized.substr(pos);
        if (rest.empty() || rest[0] != '/') rest = "/" + rest;
        return protocol + "//" + host + rest;
    }
    return normalized;
}

class FlowDb : public DiagramMeta {
public:
    std::string direction;
    std::vector<std::string> nodeOrder;
    std::unordered_map<std::string, FlowNode> nodes;
    std::vector<FlowEdge> edges;
    std::vector<FlowSubgraph> subgraphs;
    std::unordered_map<std::string, FlowClassDef> classes;
    Pele::FlatMap<std::string, std::string> tooltips;
    std::vector<std::string> defaultEdgeStyle;
    bool hasDefaultEdgeStyle = false;
    std::string defaultInterpolate;

    Pele::FlatMap<std::string, size_t> subgraphLookup;
    Pele::FlatSet<std::string> members;
    Pele::FlatMap<std::string, size_t> edgeById;
    Pele::FlatMap<std::string, int> pairCount;
    int subCount = 0;
    FlowDbOptions options;

    explicit FlowDb(FlowDbOptions opts = {}) : options(std::move(opts)) {}

    void addVertex(
        const std::string& id,
        const FlowText* textObj = nullptr,
        const std::string& type = "",
        const std::vector<std::string>* style = nullptr,
        const std::vector<std::string>* cls = nullptr,
        const std::string& dir = "",
        const std::map<std::string, std::string>* props = nullptr,
        const std::string* metadata = nullptr
    ) {
        if (id.empty() || trimStr(id).empty()) return;

        YamlValue doc = YamlValue::makeNull();
        bool hasDoc = false;
        if (metadata != nullptr) {
            std::string yaml = (metadata->find('\n') != std::string::npos)
                ? (*metadata + "\n")
                : ("{\n" + *metadata + "\n}");
            YamlValue parsed = parseYaml(yaml);
            if (parsed.isMap()) {
                doc = std::move(parsed);
                hasDoc = true;
            }
        }

        auto sgIt = subgraphLookup.find(id);
        if (sgIt != subgraphLookup.end() && hasDoc) {
            auto& sg = subgraphs[sgIt->second];
            if (!sg.metadata.isMap()) sg.metadata = YamlValue::makeMap();
            for (const auto& [k, v] : doc.mVal) sg.metadata.mVal[k] = v;
            return;
        }

        auto edgeIt = edgeById.find(id);
        if (edgeIt != edgeById.end()) {
            auto& edge = edges[edgeIt->second];
            if (hasDoc) {
                if (const auto* v = doc.get("animate"); v && !v->isNull()) {
                    edge.animate = v->isBool() ? v->bVal : !v->asString().empty();
                    edge.hasAnimate = true;
                }
                if (const auto* v = doc.get("animation"); v && !v->isNull()) {
                    edge.animation = v->asString();
                }
                if (const auto* v = doc.get("curve"); v && !v->isNull()) {
                    edge.interpolate = v->asString();
                }
            }
            return;
        }

        auto vIt = nodes.find(id);
        if (vIt == nodes.end()) {
            FlowNode fn;
            fn.id = id;
            fn.labelType = "text";
            nodeOrder.push_back(id);
            vIt = nodes.emplace(id, std::move(fn)).first;
        }
        FlowNode& vertex = vIt->second;

        if (textObj != nullptr) {
            vertex.labelType = textObj->type;
            vertex.text = flowUnquote(trimStr(textObj->text));
            vertex.hasText = true;
        } else if (!vertex.hasText) {
            vertex.text = id;
            vertex.hasText = true;
        }
        if (!type.empty()) {
            vertex.type = type;
            vertex.hasType = true;
        }
        if (style != nullptr) {
            for (const auto& s : *style) vertex.styles.push_back(s);
        }
        if (cls != nullptr) {
            for (const auto& c : *cls) vertex.classes.push_back(c);
        }
        if (!dir.empty()) vertex.dir = dir;
        if (!vertex.hasProps) {
            vertex.hasProps = true;
            if (props != nullptr) vertex.props = *props;
        } else if (props != nullptr) {
            for (const auto& [k, v] : *props) vertex.props[k] = v;
        }

        if (!hasDoc) return;

        if (const auto* v = doc.get("shape"); v && !v->isNull()) {
            std::string shape = v->asString();
            if (!shape.empty()) {
                if (shape != toLowerAscii(shape) || shape.find('_') != std::string::npos) {
                    throw PeleError(Pele::cat({"No such shape: ", shape, ". Shape names should be lowercase."}),  "semantic",  "flowchart");
                }
                if (!isValidShape(shape)) {
                    throw PeleError(Pele::cat({"No such shape: ", shape, "."}),  "semantic",  "flowchart");
                }
                vertex.type = shape;
                vertex.hasType = true;
            }
        }
        std::string labelStr;
        if (const auto* v = doc.get("label"); v && !v->isNull()) {
            if (v->isString()) labelStr = v->sVal;
            std::string s = v->asString();
            bool truthy = !s.empty() && (!v->isBool() || v->bVal) && (!v->isNumber() || v->nVal != 0.0);
            if (truthy) {
                vertex.text = s;
                vertex.hasText = true;
                const auto* lt = doc.get("labelType");
                vertex.labelType = flowLabelType(lt ? lt->asString() : "");
            }
        }
        if (const auto* v = doc.get("icon"); v && !v->isNull() && !v->asString().empty()) {
            vertex.icon = v->asString();
            if (trimStr(labelStr).empty() && vertex.text == id) vertex.text = "";
        }
        if (const auto* v = doc.get("form"); v && !v->isNull() && !v->asString().empty()) vertex.form = v->asString();
        if (const auto* v = doc.get("pos"); v && !v->isNull() && !v->asString().empty()) vertex.pos = v->asString();
        if (const auto* v = doc.get("img"); v && !v->isNull() && !v->asString().empty()) {
            vertex.img = v->asString();
            if (trimStr(labelStr).empty() && vertex.text == id) vertex.text = "";
        }
        if (const auto* v = doc.get("constraint"); v && !v->isNull() && !v->asString().empty()) vertex.constraint = v->asString();
        if (const auto* v = doc.get("w"); v && !v->isNull()) {
            double w = v->isNumber() ? v->nVal : 0.0;
            if (!v->isNumber()) { try { w = std::stod(v->asString()); } catch (...) {} }
            if (w != 0.0) { vertex.assetWidth = w; vertex.hasAssetWidth = true; }
        }
        if (const auto* v = doc.get("h"); v && !v->isNull()) {
            double h = v->isNumber() ? v->nVal : 0.0;
            if (!v->isNumber()) { try { h = std::stod(v->asString()); } catch (...) {} }
            if (h != 0.0) { vertex.assetHeight = h; vertex.hasAssetHeight = true; }
        }
    }

    void addSingleLink(const std::string& start, const std::string& end, const FlowLinkInfo* type, const std::string& id = "") {
        FlowEdge edge;
        edge.start = start;
        edge.end = end;
        edge.labelType = "text";
        edge.isUserDefinedId = false;
        edge.interpolate = defaultInterpolate;
        if (type != nullptr && type->hasText) {
            edge.text = flowUnquote(trimStr(type->text.text));
            edge.labelType = flowLabelType(type->text.type);
        }
        if (type != nullptr) {
            edge.type = type->type;
            edge.hasType = true;
            edge.stroke = type->stroke;
            if (type->hasLength) {
                edge.length = type->length > 10 ? 10 : type->length;
                edge.hasLength = true;
            }
        }
        std::string pair = start + std::string(1, '\0') + end;
        int count = pairCount[pair];
        if (!id.empty() && !edgeById.count(id)) {
            edge.id = id;
            edge.isUserDefinedId = true;
        } else {
            edge.id = "L_" + start + "_" + end + "_" + std::to_string(count == 0 ? 0 : count + 1);
        }
        if (options.hasMaxEdges && static_cast<int>(edges.size()) >= options.maxEdges) {
            throw PeleError("Edge limit exceeded. The limit is " + std::to_string(options.maxEdges) + ".", "limit", "flowchart");
        }
        size_t idx = edges.size();
        std::string eid = edge.id;
        edges.push_back(std::move(edge));
        pairCount[pair] = count + 1;
        if (!edgeById.count(eid)) edgeById[eid] = idx;
    }

    void addLink(const std::vector<std::string>& starts, const std::vector<std::string>& ends, const FlowLinkInfo* linkData) {
        std::string id;
        if (linkData != nullptr && linkData->hasId) {
            id = replaceAllStr(linkData->id, "@", "");
        }
        const std::string& lastStart = starts.back();
        const std::string& firstEnd = ends.front();
        for (const auto& start : starts) {
            for (const auto& end : ends) {
                addSingleLink(start, end, linkData, (start == lastStart && end == firstEnd) ? id : "");
            }
        }
    }

    void updateLinkInterpolate(const std::vector<std::string>& positions, const std::string& interpolate) {
        for (const auto& pos : positions) {
            if (pos == "default") defaultInterpolate = interpolate;
            else edgeAt(pos).interpolate = interpolate;
        }
    }

    void updateLink(const std::vector<std::string>& positions, std::vector<std::string> style) {
        bool hasFill = false;
        for (const auto& s : style) if (startsWith(s, "fill")) hasFill = true;
        if (!style.empty() && !hasFill) style.push_back("fill:none");
        for (const auto& pos : positions) {
            if (pos == "default") {
                defaultEdgeStyle = style;
                hasDefaultEdgeStyle = true;
                continue;
            }
            auto& edge = edgeAt(pos);
            edge.style = style;
            edge.hasStyle = true;
        }
    }

    FlowEdge& edgeAt(const std::string& pos) {
        long long idx = -1;
        try { idx = std::stoll(pos); } catch (...) {}
        if (idx < 0 || idx >= static_cast<long long>(edges.size())) {
            throw PeleError(
                "The index " + pos + " for linkStyle is out of bounds. Valid indices for linkStyle are between 0 and " +
                std::to_string(static_cast<long long>(edges.size()) - 1) +
                ". (Help: Ensure that the index is within the range of existing edges.)",
                "semantic",
                "flowchart"
            );
        }
        return edges[static_cast<size_t>(idx)];
    }

    void addClass(const std::string& ids, const std::vector<std::string>& styles) {
        std::string joined;
        for (size_t i = 0; i < styles.size(); i++) {
            if (i > 0) joined.push_back(',');
            joined += styles[i];
        }
        joined = replaceAllStr(joined, "\\,", "\x01");
        joined = replaceAllStr(joined, ",", ";");
        joined = replaceAllStr(joined, "\x01", ",");
        auto styleList = splitStr(joined, ';');
        for (const auto& id : splitStr(ids, ',')) {
            auto& classNode = classes[id];
            classNode.id = id;
            for (const auto& s : styleList) {
                if (s.find("color") != std::string::npos) {
                    classNode.textStyles.push_back(replaceAllStr(s, "fill", "bgFill"));
                }
                classNode.styles.push_back(s);
            }
        }
    }

    void setDirection(const std::string& dir) {
        std::string d = trimStr(dir);
        if (d.find('<') != std::string::npos) d = "RL";
        if (d.find('^') != std::string::npos) d = "BT";
        if (d.find('>') != std::string::npos) d = "LR";
        if (d.find('v') != std::string::npos) d = "TB";
        if (d == "TD") d = "TB";
        direction = d;
    }

    void setClass(const std::string& ids, const std::string& className) {
        for (const auto& id : splitStr(ids, ',')) {
            auto nIt = nodes.find(id);
            if (nIt != nodes.end()) nIt->second.classes.push_back(className);
            auto eIt = edgeById.find(id);
            if (eIt != edgeById.end()) edges[eIt->second].classes.push_back(className);
            auto sIt = subgraphLookup.find(id);
            if (sIt != subgraphLookup.end()) subgraphs[sIt->second].classes.push_back(className);
        }
    }

    void setTooltip(const std::string& ids, const std::string& tooltip) {
        for (const auto& id : splitStr(ids, ',')) tooltips[id] = tooltip;
    }

    void setLink(const std::string& ids, const std::string& link, const std::string& target = "") {
        std::string url = trimStr(link);
        for (const auto& id : splitStr(ids, ',')) {
            auto nIt = nodes.find(id);
            if (nIt != nodes.end()) {
                nIt->second.link = !url.empty() ? flowSafeUrl(url) : "";
                nIt->second.linkTarget = target;
            }
        }
        setClass(ids, "clickable");
    }

    void setClickEvent(const std::string& ids, const std::string& = "", const std::string& = "") {
        setClass(ids, "clickable");
    }

    void setAccTitle(const std::string& text) {
        accTitle = trimStartStr(text);
    }

    void setAccDescription(const std::string& text) {
        std::string out;
        size_t i = 0;
        while (i < text.size()) {
            out.push_back(text[i]);
            if (text[i] == '\n') {
                i++;
                while (i < text.size() && isSpace(static_cast<unsigned char>(text[i])) && text[i] != '\n') i++;
            } else {
                i++;
            }
        }
        accDescr = out;
    }

    std::string addSubGraph(const FlowText* idText, const std::vector<DocItem>& list, const FlowText* titleText) {
        std::string id = idText ? trimStr(idText->text) : "";
        std::string titleStr = titleText ? titleText->text : "";
        if (idText != nullptr && idText == titleText) {
            bool hasSpace = false;
            size_t p = 0;
            while (p < titleStr.size()) {
                int cp = charCodeAtUtf8(titleStr, p, &p);
                if (isSpace(cp)) { hasSpace = true; break; }
            }
            if (hasSpace) id.clear();
        }

        Pele::FlatSet<std::string> seen;
        std::vector<std::string> nodeList;
        std::string dir;
        for (const auto& entry : list) {
            if (entry.kind == DocItem::Str) {
                if (!trimStr(entry.strVal).empty() && !seen.count(entry.strVal)) {
                    seen.insert(entry.strVal);
                    nodeList.push_back(entry.strVal);
                }
            } else if (entry.kind == DocItem::Arr) {
                for (const auto& item : entry.arrVal) {
                    if (!trimStr(item).empty() && !seen.count(item)) {
                        seen.insert(item);
                        nodeList.push_back(item);
                    }
                }
            } else {
                dir = entry.dirVal;
            }
        }
        if (dir.empty() && options.inheritDir) {
            dir = !direction.empty() ? direction : options.direction;
        }
        if (id.empty()) id = "subGraph" + std::to_string(subCount);
        subCount++;

        std::vector<std::string> filtered;
        for (const auto& nodeId : nodeList) {
            if (!members.count(nodeId) && nodeId != id) {
                filtered.push_back(nodeId);
            }
        }
        for (const auto& nodeId : filtered) members.insert(nodeId);

        auto existingIt = subgraphLookup.find(id);
        if (existingIt != subgraphLookup.end()) {
            auto& existing = subgraphs[existingIt->second];
            for (const auto& nodeId : filtered) existing.nodes.push_back(nodeId);
        } else {
            FlowSubgraph sg;
            sg.id = id;
            sg.nodes = std::move(filtered);
            sg.title = trimStr(titleStr);
            sg.dir = dir;
            sg.labelType = flowLabelType(titleText ? titleText->type : "");
            size_t idx = subgraphs.size();
            subgraphs.push_back(std::move(sg));
            subgraphLookup[id] = idx;
        }
        return id;
    }

    FlowLinkInfo destructLink(const std::string& end, const std::string& start = "") const {
        FlowLinkInfo info = destructEndLink(end);
        if (start.empty()) return info;
        FlowLinkInfo startInfo = destructStartLink(start);
        if (startInfo.stroke != info.stroke) {
            FlowLinkInfo inv;
            inv.type = "INVALID";
            inv.stroke = "INVALID";
            return inv;
        }
        if (startInfo.type == "arrow_open") {
            startInfo.type = info.type;
        } else {
            if (startInfo.type != info.type) {
                FlowLinkInfo inv;
                inv.type = "INVALID";
                inv.stroke = "INVALID";
                return inv;
            }
            startInfo.type = Pele::cat({"double_", startInfo.type});
        }
        if (startInfo.type == "double_arrow") startInfo.type = "double_arrow_point";
        startInfo.length = info.length;
        startInfo.hasLength = info.hasLength;
        return startInfo;
    }

    const FlowSubgraph* subgraph(const std::string& id) const {
        auto it = subgraphLookup.find(id);
        return it == subgraphLookup.end() ? nullptr : &subgraphs[it->second];
    }
};

// ============================================================================
// Port of src/diagrams/flowchart/tokens.ts & lexer.ts & parser.ts
// ============================================================================

namespace Tok {
enum T : int {
    END = 0,
    ERROR,
    acc_title,
    acc_title_value,
    acc_descr,
    acc_descr_value,
    acc_descr_multiline_value,
    SHAPE_DATA,
    CALLBACKNAME,
    CALLBACKARGS,
    MD_STR,
    STR,
    STYLE,
    DEFAULT,
    LINKSTYLE,
    INTERPOLATE,
    CLASSDEF,
    CLASS,
    HREF,
    CLICK,
    GRAPH,
    subgraph,
    end,
    LINK_TARGET,
    NODIR,
    DIR,
    direction_tb,
    direction_bt,
    direction_rl,
    direction_lr,
    direction_td,
    LINK_ID,
    NUM,
    BRKT,
    STYLE_SEPARATOR,
    COLON,
    AMP,
    SEMI,
    COMMA,
    MULT,
    LINK,
    START_LINK,
    EDGE_TEXT,
    ELLIPSE_END,
    TEXT,
    ELLIPSE_START,
    STADIUMEND,
    STADIUMSTART,
    SUBROUTINEEND,
    SUBROUTINESTART,
    VERTEX_WITH_PROPS_START,
    TAGEND,
    CYLINDEREND,
    CYLINDERSTART,
    DOUBLECIRCLEEND,
    DOUBLECIRCLESTART,
    TRAPEND,
    INVTRAPEND,
    TRAPSTART,
    INVTRAPSTART,
    TAGSTART,
    UP,
    SEP,
    DOWN,
    NODE_STRING,
    MINUS,
    UNICODE_TEXT,
    PIPE,
    PE,
    PS,
    SQE,
    SQS,
    DIAMOND_STOP,
    DIAMOND_START,
    NEWLINE,
    SPACE,
    EOF_TOK
};

inline const char* tokenName(int t) noexcept {
    static const char* NAMES[] = {
        "$end", "INVALID", "acc_title", "acc_title_value", "acc_descr", "acc_descr_value",
        "acc_descr_multiline_value", "SHAPE_DATA", "CALLBACKNAME", "CALLBACKARGS", "MD_STR", "STR",
        "STYLE", "DEFAULT", "LINKSTYLE", "INTERPOLATE", "CLASSDEF", "CLASS", "HREF", "CLICK",
        "GRAPH", "subgraph", "end", "LINK_TARGET", "NODIR", "DIR", "direction_tb", "direction_bt",
        "direction_rl", "direction_lr", "direction_td", "LINK_ID", "NUM", "BRKT", "STYLE_SEPARATOR",
        "COLON", "AMP", "SEMI", "COMMA", "MULT", "LINK", "START_LINK", "EDGE_TEXT", "-)", "TEXT",
        "(-", "STADIUMEND", "STADIUMSTART", "SUBROUTINEEND", "SUBROUTINESTART",
        "VERTEX_WITH_PROPS_START", "TAGEND", "CYLINDEREND", "CYLINDERSTART", "DOUBLECIRCLEEND",
        "DOUBLECIRCLESTART", "TRAPEND", "INVTRAPEND", "TRAPSTART", "INVTRAPSTART", "TAGSTART",
        "UP", "SEP", "DOWN", "NODE_STRING", "MINUS", "UNICODE_TEXT", "PIPE", "PE", "PS", "SQE",
        "SQS", "DIAMOND_STOP", "DIAMOND_START", "NEWLINE", "SPACE", "EOF"
    };
    return (t >= 0 && t <= EOF_TOK) ? NAMES[t] : "INVALID";
}
} // namespace Tok

struct FlowTokens {
    std::vector<int> types;
    std::vector<std::string> texts;
    std::vector<size_t> starts;
};

namespace detail {

inline const std::array<uint8_t, 128>& nodeCharTable() noexcept {
    static const std::array<uint8_t, 128> tbl = []() {
        std::array<uint8_t, 128> a{};
        const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!\"#$%&'*+.`?\\_/";
        for (const char* p = chars; *p; ++p) a[static_cast<unsigned char>(*p)] = 1;
        return a;
    }();
    return tbl;
}

inline int codepointAt(const std::string& src, size_t pos, size_t& nextPos) noexcept {
    if (pos >= src.size()) { nextPos = pos; return -1; }
    unsigned char c = static_cast<unsigned char>(src[pos]);
    if (c < 0x80) { nextPos = pos + 1; return c; }
    return charCodeAtUtf8(src, pos, &nextPos);
}

inline bool isSpaceAt(const std::string& src, size_t pos, size_t* nextPos = nullptr) noexcept {
    size_t np = pos;
    int cp = codepointAt(src, pos, np);
    if (nextPos) *nextPos = np;
    return isSpace(cp);
}

inline size_t skipSpaces(const std::string& src, size_t pos) noexcept {
    while (pos < src.size()) {
        size_t np = pos;
        int cp = codepointAt(src, pos, np);
        if (!isSpace(cp)) break;
        pos = np;
    }
    return pos;
}

// Sticky matchers corresponding to RE_* in lexer.ts
inline int matchAccHeader(const std::string& src, size_t q, const char* word, char sep) noexcept {
    size_t len = std::strlen(word);
    if (!startsWith(src, word, q)) return -1;
    size_t p = skipSpaces(src, q + len);
    if (p >= src.size() || src[p] != sep) return -1;
    return static_cast<int>(skipSpaces(src, p + 1));
}

inline int matchWordSpaces(const std::string& src, size_t q, const char* word) noexcept {
    size_t len = std::strlen(word);
    if (!startsWith(src, word, q)) return -1;
    size_t p = skipSpaces(src, q + len);
    return p > q + len ? static_cast<int>(p) : -1;
}

inline int matchNoDir(const std::string& src, size_t q) noexcept {
    size_t p = q;
    int lastNl = -1;
    while (p < src.size()) {
        size_t np = p;
        int cp = codepointAt(src, p, np);
        if (!isSpace(cp)) break;
        if (cp == '\n') lastNl = static_cast<int>(np);
        p = np;
    }
    return lastNl;
}

inline int matchDirWord(const std::string& src, size_t q) noexcept {
    size_t p = skipSpaces(src, q);
    if (p + 2 > src.size()) return -1;
    char a = src[p], b = src[p + 1];
    bool ok = (a == 'L' && b == 'R') || (a == 'R' && b == 'L') ||
              (a == 'T' && b == 'B') || (a == 'B' && b == 'T') ||
              (a == 'T' && b == 'D') || (a == 'B' && b == 'R');
    if (!ok) return -1;
    if (p + 2 < src.size() && isWord(static_cast<unsigned char>(src[p + 2]))) return -1;
    return static_cast<int>(p + 2);
}

inline int matchDirSym(const std::string& src, size_t q) noexcept {
    size_t p = skipSpaces(src, q);
    if (p < src.size() && (src[p] == '<' || src[p] == '>' || src[p] == '^')) return static_cast<int>(p + 1);
    return -1;
}

inline int matchDirV(const std::string& src, size_t q) noexcept {
    size_t p = skipSpaces(src, q);
    if (p < src.size() && src[p] == 'v' && (p + 1 >= src.size() || !isWord(static_cast<unsigned char>(src[p + 1])))) {
        return static_cast<int>(p + 1);
    }
    return -1;
}

inline int matchDirectionStmt(const std::string& src, size_t q, size_t lineEnd, const char* dirCode) noexcept {
    // Matches /.*direction\s+(TB|BT|RL|LR|TD)[^\n]*/y starting at q
    size_t pos = q;
    while (pos < lineEnd) {
        size_t found = src.find("direction", pos);
        if (found == std::string::npos || found >= lineEnd) break;
        size_t afterWord = found + 9;
        size_t sp = afterWord;
        while (sp < lineEnd && isSpaceAt(src, sp)) {
            size_t np = sp;
            codepointAt(src, sp, np);
            sp = np;
        }
        if (sp > afterWord && sp + 2 <= lineEnd && src[sp] == dirCode[0] && src[sp + 1] == dirCode[1]) {
            size_t nl = src.find('\n', sp + 2);
            return static_cast<int>(nl == std::string::npos ? src.size() : nl);
        }
        pos = found + 1;
    }
    return -1;
}

// RE_LINK = /\s*[xo<]?--+[-xo>]\s*/y
inline int matchLink(const std::string& src, size_t q) noexcept {
    size_t p = skipSpaces(src, q);
    if (p < src.size() && (src[p] == 'x' || src[p] == 'o' || src[p] == '<')) p++;
    size_t dashStart = p;
    while (p < src.size() && src[p] == '-') p++;
    size_t dashes = p - dashStart;
    if (dashes < 2) return -1;
    if (p < src.size() && (src[p] == 'x' || src[p] == 'o' || src[p] == '>')) p++;
    else if (dashes == 2) return -1;
    return static_cast<int>(skipSpaces(src, p));
}

// RE_START_LINK = /\s*[xo<]?--\s*/y
inline int matchStartLink(const std::string& src, size_t q) noexcept {
    size_t p = skipSpaces(src, q);
    if (p < src.size() && (src[p] == 'x' || src[p] == 'o' || src[p] == '<')) p++;
    if (p + 2 > src.size() || src[p] != '-' || src[p + 1] != '-') return -1;
    return static_cast<int>(skipSpaces(src, p + 2));
}

// RE_THICK_LINK = /\s*[xo<]?==+[=xo>]\s*/y
inline int matchThickLink(const std::string& src, size_t q) noexcept {
    size_t p = skipSpaces(src, q);
    if (p < src.size() && (src[p] == 'x' || src[p] == 'o' || src[p] == '<')) p++;
    size_t eqStart = p;
    while (p < src.size() && src[p] == '=') p++;
    size_t eqs = p - eqStart;
    if (eqs < 2) return -1;
    if (p < src.size() && (src[p] == 'x' || src[p] == 'o' || src[p] == '>')) p++;
    else if (eqs == 2) return -1;
    return static_cast<int>(skipSpaces(src, p));
}

// RE_THICK_START = /\s*[xo<]?==\s*/y
inline int matchThickStart(const std::string& src, size_t q) noexcept {
    size_t p = skipSpaces(src, q);
    if (p < src.size() && (src[p] == 'x' || src[p] == 'o' || src[p] == '<')) p++;
    if (p + 2 > src.size() || src[p] != '=' || src[p + 1] != '=') return -1;
    return static_cast<int>(skipSpaces(src, p + 2));
}

// RE_DOTTED_LINK = /\s*[xo<]?-?\.+-[xo>]?\s*/y
inline int matchDottedLink(const std::string& src, size_t q) noexcept {
    size_t p = skipSpaces(src, q);
    if (p < src.size() && (src[p] == 'x' || src[p] == 'o' || src[p] == '<')) p++;
    if (p < src.size() && src[p] == '-') p++;
    size_t dotStart = p;
    while (p < src.size() && src[p] == '.') p++;
    if (p == dotStart) return -1;
    if (p >= src.size() || src[p] != '-') return -1;
    p++;
    if (p < src.size() && (src[p] == 'x' || src[p] == 'o' || src[p] == '>')) p++;
    return static_cast<int>(skipSpaces(src, p));
}

// RE_DOTTED_START = /\s*[xo<]?-\.\s*/y
inline int matchDottedStart(const std::string& src, size_t q) noexcept {
    size_t p = skipSpaces(src, q);
    if (p < src.size() && (src[p] == 'x' || src[p] == 'o' || src[p] == '<')) p++;
    if (p + 2 > src.size() || src[p] != '-' || src[p + 1] != '.') return -1;
    return static_cast<int>(skipSpaces(src, p + 2));
}

// RE_INVIS_LINK = /\s*~~~+\s*/y
inline int matchInvisLink(const std::string& src, size_t q) noexcept {
    size_t p = skipSpaces(src, q);
    size_t tildeStart = p;
    while (p < src.size() && src[p] == '~') p++;
    if (p - tildeStart < 3) return -1;
    return static_cast<int>(skipSpaces(src, p));
}

// RE_NEWLINE = /(?:\r?\n)+/y
inline int matchNewline(const std::string& src, size_t q) noexcept {
    size_t p = q;
    while (p < src.size()) {
        if (src[p] == '\n') p++;
        else if (src[p] == '\r' && p + 1 < src.size() && src[p + 1] == '\n') p += 2;
        else break;
    }
    return p > q ? static_cast<int>(p) : -1;
}

// RE_SPACE = /[^\S\n\r]+/y
inline int matchNonNlSpace(const std::string& src, size_t q) noexcept {
    size_t p = q;
    while (p < src.size()) {
        size_t np = p;
        int cp = codepointAt(src, p, np);
        if (!isSpace(cp) || cp == '\n' || cp == '\r') break;
        p = np;
    }
    return p > q ? static_cast<int>(p) : -1;
}

// RE_EMPTY_CALL = /\(\s*\)/y
inline int matchEmptyCall(const std::string& src, size_t q) noexcept {
    if (q >= src.size() || src[q] != '(') return -1;
    size_t p = skipSpaces(src, q + 1);
    if (p < src.size() && src[p] == ')') return static_cast<int>(p + 1);
    return -1;
}

inline std::string replaceNewlineIndentsWithBr(const std::string& s) noexcept {
    std::string out;
    size_t i = 0;
    while (i < s.size()) {
        if (s[i] == '\n') {
            out += "<br/>";
            i++;
            while (i < s.size() && isSpaceAt(s, i)) {
                // Note: RE_BR = /\n\s*/g matches \n and any following \s*
                size_t np = i;
                codepointAt(s, i, np);
                i = np;
            }
        } else {
            out.push_back(s[i++]);
        }
    }
    return out;
}

} // namespace detail

inline FlowTokens tokenizeFlowchart(const std::string& src) {
    using namespace detail;
    using namespace Tok;
    enum LexState : int {
        INITIAL = 0,
        string,
        md_string,
        acc_title_st,
        acc_descr_st,
        acc_descr_multiline_st,
        dir_st,
        text_st,
        ellipseText_st,
        trapText_st,
        edgeText_st,
        thickEdgeText_st,
        dottedEdgeText_st,
        click_st,
        callbackname_st,
        callbackargs_st,
        shapeData_st,
        shapeDataStr_st
    };

    const auto& NODE_CHAR = nodeCharTable();
    size_t n = src.size();
    std::vector<int> types;
    std::vector<std::string> texts;
    std::vector<size_t> starts;
    std::vector<int> stack;
    int state = INITIAL;
    size_t p = 0;
    bool firstGraph = true;
    size_t dirNext = src.find("direction");
    size_t atNext = src.find('@');
    size_t lineEnd = 0;
    bool hasLineEnd = false;
    size_t dirFailed = 0;
    size_t runEnd = 0;
    int runAt = -1;
    size_t wsFrom = std::string::npos;
    size_t wsTo = std::string::npos;

    auto emit = [&](int type, size_t s, size_t e) {
        types.push_back(type);
        texts.push_back(src.substr(s, e - s));
        starts.push_back(s);
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
    auto kw = [&](const char* word, size_t wlen) -> bool {
        if (p + wlen > n) return false;
        if (std::memcmp(src.data() + p, word, wlen) != 0) return false;
        return p + wlen == n || !isWord(static_cast<unsigned char>(src[p + wlen]));
    };
    auto chAt = [&](size_t idx) -> int {
        return idx < n ? static_cast<unsigned char>(src[idx]) : -1;
    };

    auto linkChar = [&](size_t q) -> int {
        size_t j = q;
        if (wsFrom != std::string::npos && q >= wsFrom && q <= wsTo) {
            j = wsTo;
        } else if (isSpaceAt(src, q)) {
            j = skipSpaces(src, q);
            wsFrom = q;
            wsTo = j;
        }
        int d = chAt(j);
        return (d == 'x' || d == 'o' || d == '<') ? chAt(j + 1) : d;
    };
    auto invisAhead = [&](size_t q) -> bool {
        size_t j = q;
        if (wsFrom != std::string::npos && q >= wsFrom && q <= wsTo) {
            j = wsTo;
        } else if (isSpaceAt(src, q)) {
            j = skipSpaces(src, q);
            wsFrom = q;
            wsTo = j;
        }
        return chAt(j) == '~';
    };
    auto quote = [&]() {
        if (chAt(p + 1) == '`') {
            push(md_string);
            p += 2;
        } else {
            push(string);
            p++;
        }
    };
    auto invisLink = [&]() -> bool {
        if (!invisAhead(p)) return false;
        int e = matchInvisLink(src, p);
        if (e < 0) return false;
        emit(LINK, p, static_cast<size_t>(e));
        return true;
    };
    auto opener = [&](int c) -> bool {
        int d = chAt(p + 1);
        if (c == '(') {
            if (d == '-') {
                push(ellipseText_st);
                emit(ELLIPSE_START, p, p + 2);
            } else if (d == '[') {
                push(text_st);
                emit(STADIUMSTART, p, p + 2);
            } else if (d == '(' && chAt(p + 2) == '(') {
                push(text_st);
                emit(DOUBLECIRCLESTART, p, p + 3);
            } else {
                push(text_st);
                emit(PS, p, p + 1);
            }
            return true;
        }
        if (c == '[') {
            if (d == '[') {
                push(text_st);
                emit(SUBROUTINESTART, p, p + 2);
            } else if (d == '(') {
                push(text_st);
                emit(CYLINDERSTART, p, p + 2);
            } else if (d == '/') {
                push(trapText_st);
                emit(TRAPSTART, p, p + 2);
            } else if (d == '\\') {
                push(trapText_st);
                emit(INVTRAPSTART, p, p + 2);
            } else {
                push(text_st);
                emit(SQS, p, p + 1);
            }
            return true;
        }
        if (c == '|') {
            push(text_st);
            emit(PIPE, p, p + 1);
            return true;
        }
        if (c == '{') {
            push(text_st);
            emit(DIAMOND_START, p, p + 1);
            return true;
        }
        return false;
    };
    auto graph = [&](size_t len) {
        if (firstGraph) {
            firstGraph = false;
            push(dir_st);
        }
        emit(GRAPH, p, p + len);
    };
    auto nodeString = [&]() -> bool {
        size_t q = p;
        while (q < n) {
            unsigned char ch = static_cast<unsigned char>(src[q]);
            if (ch < 128 && NODE_CHAR[ch] == 1) {
                q++;
            } else if (ch == '-' && q + 1 < n) {
                int d = chAt(q + 1);
                if (d == '>' || d == '-' || d == '.') break;
                q++;
            } else {
                break;
            }
        }
        if (q == p) return false;
        emit(NODE_STRING, p, q);
        return true;
    };
    auto link = [&](int c) -> bool {
        int d = linkChar(p);
        int e = -1;
        if (d == '-') {
            if ((e = matchLink(src, p)) >= 0) {
                emit(LINK, p, static_cast<size_t>(e));
                return true;
            }
            if ((e = matchStartLink(src, p)) >= 0) {
                push(edgeText_st);
                emit(START_LINK, p, static_cast<size_t>(e));
                return true;
            }
            if ((e = matchDottedLink(src, p)) >= 0) {
                emit(LINK, p, static_cast<size_t>(e));
                return true;
            }
            if ((e = matchDottedStart(src, p)) >= 0) {
                push(dottedEdgeText_st);
                emit(START_LINK, p, static_cast<size_t>(e));
                return true;
            }
        } else if (d == '=') {
            if ((e = matchThickLink(src, p)) >= 0) {
                emit(LINK, p, static_cast<size_t>(e));
                return true;
            }
            if ((e = matchThickStart(src, p)) >= 0) {
                push(thickEdgeText_st);
                emit(START_LINK, p, static_cast<size_t>(e));
                return true;
            }
        } else if (d == '.') {
            if ((e = matchDottedLink(src, p)) >= 0) {
                emit(LINK, p, static_cast<size_t>(e));
                return true;
            }
        }
        return c != '-' && c != '=' && c != '.' && invisLink();
    };

    auto initial = [&](int c, size_t nextP) -> bool {
        int e = -1;
        switch (c) {
            case 'a':
                if (startsWith(src, "acc", p)) {
                    if ((e = matchAccHeader(src, p, "accTitle", ':')) >= 0) {
                        push(acc_title_st);
                        emit(acc_title, p, static_cast<size_t>(e));
                        return true;
                    }
                    if ((e = matchAccHeader(src, p, "accDescr", ':')) >= 0) {
                        push(acc_descr_st);
                        emit(acc_descr, p, static_cast<size_t>(e));
                        return true;
                    }
                    if ((e = matchAccHeader(src, p, "accDescr", '{')) >= 0) {
                        push(acc_descr_multiline_st);
                        p = static_cast<size_t>(e);
                        return true;
                    }
                }
                break;
            case '@':
                if (chAt(p + 1) == '{') {
                    push(shapeData_st);
                    types.push_back(SHAPE_DATA);
                    texts.push_back("");
                    starts.push_back(p);
                    p += 2;
                    return true;
                }
                break;
            case 'c':
                if ((e = matchWordSpaces(src, p, "call")) >= 0) {
                    push(callbackname_st);
                    p = static_cast<size_t>(e);
                    return true;
                }
                if (kw("classDef", 8)) {
                    emit(CLASSDEF, p, p + 8);
                    return true;
                }
                if (kw("class", 5)) {
                    emit(CLASS, p, p + 5);
                    return true;
                }
                if ((e = matchWordSpaces(src, p, "click")) >= 0) {
                    push(click_st);
                    p = static_cast<size_t>(e);
                    return true;
                }
                break;
            case '"':
                quote();
                return true;
            case 's':
                if (kw("style", 5)) {
                    emit(STYLE, p, p + 5);
                    return true;
                }
                if (kw("swimlane-beta", 13)) {
                    graph(13);
                    return true;
                }
                if (kw("subgraph", 8)) {
                    emit(subgraph, p, p + 8);
                    return true;
                }
                break;
            case 'd':
                if (kw("default", 7)) {
                    emit(DEFAULT, p, p + 7);
                    return true;
                }
                break;
            case 'l':
                if (kw("linkStyle", 9)) {
                    emit(LINKSTYLE, p, p + 9);
                    return true;
                }
                break;
            case 'i':
                if (kw("interpolate", 11)) {
                    emit(INTERPOLATE, p, p + 11);
                    return true;
                }
                break;
            case 'h':
                if (startsWith(src, "href", p) && isSpaceAt(src, p + 4)) {
                    emit(HREF, p, p + 5);
                    return true;
                }
                break;
            case 'f':
                if (kw("flowchart-elk", 13)) {
                    graph(13);
                    return true;
                }
                if (kw("flowchart", 9)) {
                    graph(9);
                    return true;
                }
                break;
            case 'g':
                if (kw("graph", 5)) {
                    graph(5);
                    return true;
                }
                break;
            case 'e':
                if (kw("end", 3)) {
                    size_t endPos = skipSpaces(src, p + 3);
                    emit(end, p, endPos);
                    return true;
                }
                break;
            case '_':
                if (kw("_self", 5)) { emit(LINK_TARGET, p, p + 5); return true; }
                if (kw("_blank", 6)) { emit(LINK_TARGET, p, p + 6); return true; }
                if (kw("_parent", 7)) { emit(LINK_TARGET, p, p + 7); return true; }
                if (kw("_top", 4)) { emit(LINK_TARGET, p, p + 4); return true; }
                break;
        }

        if (dirNext != std::string::npos && p >= dirFailed) {
            if (dirNext < p) dirNext = src.find("direction", p);
            if (dirNext != std::string::npos) {
                if (!hasLineEnd || lineEnd < p) {
                    hasLineEnd = true;
                    lineEnd = p;
                    while (lineEnd < n) {
                        size_t np = lineEnd;
                        int cp = codepointAt(src, lineEnd, np);
                        if (isLineEnd(cp)) break;
                        lineEnd = np;
                    }
                }
                if (dirNext < lineEnd) {
                    static const char* DIRS[5] = {"TB", "BT", "RL", "LR", "TD"};
                    for (int i = 0; i < 5; i++) {
                        if ((e = matchDirectionStmt(src, p, lineEnd, DIRS[i])) >= 0) {
                            emit(direction_tb + i, p, static_cast<size_t>(e));
                            return true;
                        }
                    }
                    dirFailed = lineEnd;
                }
            }
        }

        if (atNext != std::string::npos && c != '"' && !isSpace(c)) {
            if (p >= runEnd) {
                if (atNext < p) atNext = src.find('@', p);
                size_t q = p;
                while (q < n) {
                    size_t nq = q;
                    int cp = codepointAt(src, q, nq);
                    if (cp == '"' || isSpace(cp)) break;
                    q = nq;
                }
                runEnd = q;
                runAt = -1;
                if (atNext != std::string::npos && atNext < q) {
                    size_t limit = std::min(q, n > 0 ? n - 1 : 0);
                    for (int k = static_cast<int>(limit) - 1; k >= static_cast<int>(atNext); k--) {
                        if (src[k] != '@') continue;
                        int after = chAt(k + 1);
                        if (after != '{' && after != '"') {
                            runAt = k;
                            break;
                        }
                    }
                }
            }
            if (runAt > static_cast<int>(p)) {
                emit(LINK_ID, p, static_cast<size_t>(runAt + 1));
                return true;
            }
        }

        if (c >= '0' && c <= '9') {
            size_t q = p + 1;
            while (q < n && src[q] >= '0' && src[q] <= '9') q++;
            emit(NUM, p, q);
            return true;
        }

        switch (c) {
            case '#': emit(BRKT, p, p + 1); return true;
            case ':':
                if (chAt(p + 1) == ':' && chAt(p + 2) == ':') emit(STYLE_SEPARATOR, p, p + 3);
                else emit(COLON, p, p + 1);
                return true;
            case '&': emit(AMP, p, p + 1); return true;
            case ';': emit(SEMI, p, p + 1); return true;
            case ',': emit(COMMA, p, p + 1); return true;
            case '*': emit(MULT, p, p + 1); return true;
            case 'x':
            case 'o':
                return link(c) || nodeString();
            case '<':
                if (link(c)) return true;
                emit(TAGSTART, p, p + 1);
                return true;
            case '-':
                if (link(c) || nodeString()) return true;
                emit(MINUS, p, p + 1);
                return true;
            case '=':
                return link(c);
            case '.':
                return link(c) || nodeString();
            case '~':
                return invisLink();
            case '(':
                return opener(c);
            case '[':
                if (chAt(p + 1) == '|') {
                    emit(VERTEX_WITH_PROPS_START, p, p + 2);
                    return true;
                }
                return opener(c);
            case '|':
            case '{':
                return opener(c);
            case '>':
                push(text_st);
                emit(TAGEND, p, p + 1);
                return true;
            case '^':
                emit(UP, p, p + 1);
                return true;
            case '\\':
                if (chAt(p + 1) == '|') {
                    emit(SEP, p, p + 2);
                    return true;
                }
                return nodeString();
            case 'v':
                if (!isWord(chAt(p + 1))) {
                    emit(DOWN, p, p + 1);
                    return true;
                }
                return nodeString();
        }

        if (isSpace(c)) {
            if (link(c)) return true;
            if ((e = matchNewline(src, p)) >= 0) {
                emit(NEWLINE, p, static_cast<size_t>(e));
                return true;
            }
            if ((e = matchNonNlSpace(src, p)) >= 0) {
                emit(SPACE, p, static_cast<size_t>(e));
                return true;
            }
            return false;
        }

        if (c < 128) return nodeString();
        if (isUnicodeLetter(c)) {
            emit(UNICODE_TEXT, p, nextP);
            return true;
        }
        return false;
    };

    auto edgeTextFn = [&](int c, auto&& matchRe, int a, int b) -> bool {
        if (c == '"') {
            quote();
            return true;
        }
        int d = linkChar(p);
        if (d == a || d == b) {
            int e = matchRe(src, p);
            if (e >= 0) {
                pop();
                emit(LINK, p, static_cast<size_t>(e));
                return true;
            }
        }
        if (c == a && (a != '-' || chAt(p + 1) == '-')) return false;
        size_t q = p;
        codepointAt(src, q, q);
        while (q < n) {
            size_t nq = q;
            int ch = codepointAt(src, q, nq);
            if (ch == '"') break;
            if (ch == a) {
                if (a != '-' || chAt(q + 1) == '-') break;
            } else if (ch == 'x' || ch == 'o' || ch == '<' || ch == b || isSpace(ch)) {
                int dd = linkChar(q);
                if ((dd == a || dd == b) && matchRe(src, q) >= 0) break;
            }
            q = nq;
        }
        emit(EDGE_TEXT, p, q);
        return true;
    };

    while (true) {
        if (p >= n) {
            int type = -1;
            switch (state) {
                case INITIAL: type = EOF_TOK; break;
                case acc_title_st: type = acc_title_value; break;
                case acc_descr_st: type = acc_descr_value; break;
                case acc_descr_multiline_st: type = acc_descr_multiline_value; break;
                case click_st: type = CLICK; break;
                case callbackname_st: type = CALLBACKNAME; break;
                case callbackargs_st: type = CALLBACKARGS; break;
            }
            if (type != -1) emit(type, n, n);
            break;
        }

        size_t nextP = p;
        int c = codepointAt(src, p, nextP);
        bool ok = true;

        switch (state) {
            case INITIAL:
                ok = initial(c, nextP);
                break;

            case string:
                if (c == '"') {
                    if (chAt(p + 1) == '`') {
                        push(md_string);
                        p += 2;
                    } else {
                        pop();
                        p++;
                    }
                } else {
                    size_t e = src.find('"', p);
                    emit(STR, p, e == std::string::npos ? n : e);
                }
                break;

            case md_string:
                if (c == '`') {
                    if (chAt(p + 1) == '"') {
                        pop();
                        p += 2;
                    } else {
                        ok = false;
                    }
                } else if (c == '"') {
                    quote();
                } else {
                    size_t q = nextP;
                    while (q < n) {
                        char ch = src[q];
                        if (ch == '`' || ch == '"') break;
                        q++;
                    }
                    emit(MD_STR, p, q);
                }
                break;

            case acc_title_st:
            case acc_descr_st: {
                size_t e = src.find('\n', p);
                if (e == std::string::npos) e = n;
                int type = state == acc_title_st ? acc_title_value : acc_descr_value;
                pop();
                emit(type, p, e);
                break;
            }

            case acc_descr_multiline_st:
                if (c == '}') {
                    pop();
                    p++;
                } else {
                    size_t e = src.find('}', p);
                    if (e == std::string::npos) e = n;
                    emit(acc_descr_multiline_value, p, e);
                }
                break;

            case dir_st: {
                int e = -1;
                if (c == '"') {
                    quote();
                } else if ((e = matchNoDir(src, p)) >= 0) {
                    pop();
                    emit(NODIR, p, static_cast<size_t>(e));
                } else if ((e = matchDirWord(src, p)) >= 0 || (e = matchDirSym(src, p)) >= 0 || (e = matchDirV(src, p)) >= 0) {
                    pop();
                    emit(DIR, p, static_cast<size_t>(e));
                } else {
                    ok = invisLink() || opener(c);
                }
                break;
            }

            case text_st: {
                int d = chAt(p + 1);
                switch (c) {
                    case '"': quote(); break;
                    case '(':
                    case '[':
                    case '{':
                        opener(c);
                        break;
                    case ']':
                        pop();
                        if (d == ')') emit(STADIUMEND, p, p + 2);
                        else if (d == ']') emit(SUBROUTINEEND, p, p + 2);
                        else emit(SQE, p, p + 1);
                        break;
                    case ')':
                        pop();
                        if (d == ']') emit(CYLINDEREND, p, p + 2);
                        else if (d == ')' && chAt(p + 2) == ')') emit(DOUBLECIRCLEEND, p, p + 3);
                        else emit(PE, p, p + 1);
                        break;
                    case '|':
                        pop();
                        emit(PIPE, p, p + 1);
                        break;
                    case '}':
                        pop();
                        emit(DIAMOND_STOP, p, p + 1);
                        break;
                    default: {
                        if ((c == '~' || isSpace(c)) && invisLink()) break;
                        size_t q = nextP;
                        while (q < n) {
                            char ch = src[q];
                            if (ch == '[' || ch == ']' || ch == '(' || ch == ')' || ch == '{' || ch == '}' || ch == '|' || ch == '"') break;
                            q++;
                        }
                        emit(TEXT, p, q);
                    }
                }
                break;
            }

            case ellipseText_st: {
                int d = chAt(p + 1);
                if (c == '"') {
                    quote();
                } else if ((c == '~' || isSpace(c)) && invisLink()) {
                    break;
                } else if ((c == '-' || c == '/' || c == ')') && d == ')') {
                    pop();
                    emit(ELLIPSE_END, p, p + 2);
                } else if (c == '(' || c == '[' || c == '{') {
                    opener(c);
                } else if (c == ')' || c == ']' || c == '}') {
                    ok = false;
                } else {
                    size_t q = nextP;
                    while (q < n) {
                        size_t nq = q;
                        int ch = codepointAt(src, q, nq);
                        if (ch == '"' || ch == '(' || ch == ')' || ch == '[' || ch == ']' || ch == '{' || ch == '}') break;
                        if ((ch == '-' || ch == '/') && chAt(q + 1) == ')') break;
                        if ((ch == '~' || isSpace(ch)) && invisAhead(q) && matchInvisLink(src, q) >= 0) break;
                        q = nq;
                    }
                    emit(TEXT, p, q);
                }
                break;
            }

            case trapText_st: {
                int d = chAt(p + 1);
                if (c == '"') {
                    quote();
                    break;
                }
                if ((c == '~' || isSpace(c)) && invisLink()) break;
                switch (c) {
                    case '(':
                        if (d == '-' || d == '[' || (d == '(' && chAt(p + 2) == '(')) {
                            opener(c);
                        } else if (d == ']') {
                            pop();
                            emit(TRAPEND, p, p + 2);
                        } else {
                            opener(c);
                        }
                        break;
                    case '[':
                    case '{':
                        opener(c);
                        break;
                    case '\\':
                        if (d == ']') {
                            pop();
                            emit(TRAPEND, p, p + 2);
                        } else {
                            emit(TEXT, p, p + 1);
                        }
                        break;
                    case '/':
                        if (d == ']') {
                            pop();
                            emit(INVTRAPEND, p, p + 2);
                        } else {
                            emit(TEXT, p, p + 1);
                        }
                        break;
                    case ']':
                    case ')':
                        if (d == ']') {
                            pop();
                            emit(TRAPEND, p, p + 2);
                        } else {
                            ok = false;
                        }
                        break;
                    case '}':
                        ok = false;
                        break;
                    default: {
                        if ((c == '?' || c == '=') && d == ']') {
                            pop();
                            emit(TRAPEND, p, p + 2);
                            break;
                        }
                        size_t q = nextP;
                        while (q < n) {
                            char ch = src[q];
                            if (ch == '\\' || ch == '[' || ch == ']' || ch == '(' || ch == ')' || ch == '{' || ch == '}' || ch == '/') break;
                            q++;
                        }
                        emit(TEXT, p, q);
                    }
                }
                break;
            }

            case edgeText_st:
                ok = edgeTextFn(c, matchLink, '-', -1);
                break;
            case thickEdgeText_st:
                ok = edgeTextFn(c, matchThickLink, '=', -1);
                break;
            case dottedEdgeText_st:
                ok = edgeTextFn(c, matchDottedLink, '.', '-');
                break;

            case click_st:
                if (c == '"') {
                    quote();
                } else if (isSpace(c)) {
                    pop();
                    p = nextP;
                } else {
                    size_t q = nextP;
                    while (q < n) {
                        size_t nq = q;
                        if (isSpaceAt(src, q, &nq)) break;
                        q = nq;
                    }
                    emit(CLICK, p, q);
                }
                break;

            case callbackname_st:
                if (c == '(') {
                    int e = matchEmptyCall(src, p);
                    pop();
                    if (e >= 0) {
                        p = static_cast<size_t>(e);
                    } else {
                        push(callbackargs_st);
                        p++;
                    }
                } else {
                    size_t e = src.find('(', p);
                    if (e == std::string::npos) e = n;
                    emit(CALLBACKNAME, p, e);
                }
                break;

            case callbackargs_st:
                if (c == ')') {
                    pop();
                    p++;
                } else {
                    size_t e = src.find(')', p);
                    if (e == std::string::npos) e = n;
                    emit(CALLBACKARGS, p, e);
                }
                break;

            case shapeData_st:
                if (c == '"') {
                    push(shapeDataStr_st);
                    emit(SHAPE_DATA, p, p + 1);
                } else if (c == '}') {
                    pop();
                    p++;
                } else if (c == '^') {
                    ok = false;
                } else {
                    size_t q = nextP;
                    while (q < n) {
                        char ch = src[q];
                        if (ch == '}' || ch == '^' || ch == '"') break;
                        q++;
                    }
                    emit(SHAPE_DATA, p, q);
                }
                break;

            case shapeDataStr_st:
                if (c == '"') {
                    pop();
                    emit(SHAPE_DATA, p, p + 1);
                } else {
                    size_t e = src.find('"', p);
                    if (e == std::string::npos) e = n;
                    types.push_back(SHAPE_DATA);
                    texts.push_back(replaceNewlineIndentsWithBr(src.substr(p, e - p)));
                    starts.push_back(p);
                    p = e;
                }
                break;
        }

        if (!ok) {
            types.push_back(ERROR);
            texts.push_back("");
            starts.push_back(p);
            return {std::move(types), std::move(texts), std::move(starts)};
        }
    }

    types.push_back(END);
    texts.push_back("");
    starts.push_back(n);
    return {std::move(types), std::move(texts), std::move(starts)};
}

inline void parseFlowchart(const std::string& source, FlowDb& db) {
    using namespace Tok;
    using namespace detail;

    // Mermaid trims whitespace between a closing brace and the end of its line before parsing: /}\s*\n/g -> '}\n'
    std::string src;
    std::vector<std::pair<size_t, size_t>> braceEdits; // (srcPos, removedBytes)
    if (source.find('}') != std::string::npos) {
        src.reserve(source.size());
        size_t i = 0;
        while (i < source.size()) {
            if (source[i] == '}') {
                size_t p = i + 1;
                while (p < source.size() && isSpaceAt(source, p) && source[p] != '\n') {
                    size_t np = p;
                    codepointAt(source, p, np);
                    p = np;
                }
                if (p < source.size() && source[p] == '\n') {
                    src.push_back('}');
                    if (p > i + 1) braceEdits.push_back({src.size(), p - (i + 1)});
                    src.push_back('\n');
                    i = p + 1;
                    continue;
                }
            }
            src.push_back(source[i++]);
        }
    } else {
        src = source;
    }

    auto mapOffset = [&](size_t at) -> size_t {
        if (braceEdits.empty()) return at;
        size_t orig = at;
        for (const auto& [editPos, removed] : braceEdits) {
            if (at >= editPos) orig += removed;
            else break;
        }
        return orig;
    };

    static const std::array<uint8_t, EOF_TOK + 1> ID_TOK = []() {
        std::array<uint8_t, EOF_TOK + 1> a{};
        for (int t : {NUM, NODE_STRING, DOWN, MINUS, DEFAULT, COMMA, COLON, AMP, BRKT, MULT, UNICODE_TEXT}) a[t] = 1;
        return a;
    }();
    static const std::array<uint8_t, EOF_TOK + 1> ALPHANUM_TOK = []() {
        std::array<uint8_t, EOF_TOK + 1> a{};
        for (int t : {NUM, UNICODE_TEXT, NODE_STRING, DIR, DOWN, MINUS, COMMA, COLON, AMP, BRKT, MULT}) a[t] = 1;
        return a;
    }();
    static const std::array<uint8_t, EOF_TOK + 1> NO_TAGS_TOK = []() {
        std::array<uint8_t, EOF_TOK + 1> a{};
        for (int t : {NUM, NODE_STRING, SPACE, MINUS, AMP, UNICODE_TEXT, COLON, MULT, BRKT, STYLE, LINKSTYLE, CLASSDEF, CLASS, CLICK, GRAPH, DIR, subgraph, end, DOWN, UP, START_LINK}) a[t] = 1;
        return a;
    }();
    static const std::array<uint8_t, EOF_TOK + 1> STYLE_PART_TOK = []() {
        std::array<uint8_t, EOF_TOK + 1> a{};
        for (int t : {NUM, NODE_STRING, COLON, SPACE, BRKT, STYLE}) a[t] = 1;
        return a;
    }();

    static const char* TEXT_TOKENS = "'UNICODE_TEXT', 'TEXT', 'TAGSTART'";
    static const char* STATEMENT_START =
        "'SEMI', 'NEWLINE', 'SPACE', 'EOF', 'subgraph', 'acc_title', 'acc_descr', 'acc_descr_multiline_value', 'STYLE', 'LINKSTYLE', 'CLASSDEF', 'CLASS', 'CLICK', 'NODE_STRING'";
    static const char* DIRECTIONS[5] = {"TB", "BT", "RL", "LR", "TD"};

    FlowTokens tok = tokenizeFlowchart(src);
    const auto& types = tok.types;
    const auto& texts = tok.texts;
    const auto& starts = tok.starts;
    size_t i = 0;
    std::string pendingData;
    bool hasPendingData = false;

    auto fail = [&](const std::string& expected) -> void {
        int t = types[i];
        size_t at = mapOffset(starts[i]);
        if (t == ERROR) throw syntaxError("flowchart", source, at, "", true);
        throw syntaxError("flowchart", source, at, "Expecting " + expected + ", got '" + std::string(tokenName(t)) + "'");
    };

    auto expect = [&](int type, const std::string& also = "") -> std::string {
        if (types[i] != type) {
            fail("'" + std::string(tokenName(type)) + "'" + (!also.empty() ? Pele::cat({", ", also}) : ""));
        }
        return texts[i++];
    };

    auto separator = [&]() {
        int t = types[i];
        if (t != NEWLINE && t != SEMI && t != EOF_TOK) fail("'SEMI', 'NEWLINE', 'EOF'");
        i++;
    };

    auto idString = [&]() -> std::string {
        if (ID_TOK[types[i]] == 0) fail("'NODE_STRING'");
        std::string s = texts[i++];
        while (ID_TOK[types[i]] == 1) s += texts[i++];
        return s;
    };

    auto alphaNum = [&]() -> std::string {
        if (ALPHANUM_TOK[types[i]] == 0) fail("'NODE_STRING'");
        std::string s = texts[i++];
        while (ALPHANUM_TOK[types[i]] == 1) s += texts[i++];
        return s;
    };

    auto text = [&]() -> FlowText {
        int t = types[i];
        std::string type;
        if (t == STR) type = "string";
        else if (t == MD_STR) type = "markdown";
        else if (t == TEXT || t == TAGSTART || t == TAGEND || t == UNICODE_TEXT) type = "text";
        else fail(std::string(TEXT_TOKENS) + ", 'TAGEND', 'STR', 'MD_STR'");
        std::string s = texts[i++];
        while (true) {
            int n = types[i];
            if (n != TEXT && n != TAGSTART && n != TAGEND && n != UNICODE_TEXT) break;
            s += texts[i++];
        }
        return {std::move(s), std::move(type)};
    };

    auto closed = [&](int type) {
        expect(type, TEXT_TOKENS);
    };

    auto edgeText = [&]() -> FlowText {
        int t = types[i];
        std::string type;
        if (t == STR) type = "string";
        else if (t == MD_STR) type = "markdown";
        else if (t == EDGE_TEXT || t == UNICODE_TEXT) type = "text";
        else fail("'EDGE_TEXT', 'STR', 'MD_STR'");
        std::string s = texts[i++];
        while (types[i] == EDGE_TEXT || types[i] == UNICODE_TEXT) s += texts[i++];
        return {std::move(s), std::move(type)};
    };

    auto textNoTags = [&]() -> FlowText {
        int t = types[i];
        std::string type;
        if (t == STR) type = "text";
        else if (t == MD_STR) type = "markdown";
        else if (NO_TAGS_TOK[t] == 1) type = "text";
        else fail("'NODE_STRING', 'STR', 'MD_STR'");
        std::string s = texts[i++];
        while (NO_TAGS_TOK[types[i]] == 1) s += texts[i++];
        return {std::move(s), std::move(type)};
    };

    auto shapeData = [&]() -> std::string {
        std::string s = texts[i++];
        while (types[i] == SHAPE_DATA) s += texts[i++];
        return s;
    };

    auto vertex = [&]() -> std::string {
        std::string id = idString();
        FlowText label;
        switch (types[i]) {
            case SQS:
                i++;
                label = text();
                closed(SQE);
                db.addVertex(id, &label, "square");
                break;
            case DOUBLECIRCLESTART:
                i++;
                label = text();
                closed(DOUBLECIRCLEEND);
                db.addVertex(id, &label, "doublecircle");
                break;
            case PS:
                i++;
                if (types[i] == PS) {
                    i++;
                    label = text();
                    closed(PE);
                    expect(PE);
                    db.addVertex(id, &label, "circle");
                } else {
                    label = text();
                    closed(PE);
                    db.addVertex(id, &label, "round");
                }
                break;
            case ELLIPSE_START:
                i++;
                label = text();
                closed(ELLIPSE_END);
                db.addVertex(id, &label, "ellipse");
                break;
            case STADIUMSTART:
                i++;
                label = text();
                closed(STADIUMEND);
                db.addVertex(id, &label, "stadium");
                break;
            case SUBROUTINESTART:
                i++;
                label = text();
                closed(SUBROUTINEEND);
                db.addVertex(id, &label, "subroutine");
                break;
            case VERTEX_WITH_PROPS_START: {
                i++;
                std::string field = expect(NODE_STRING);
                expect(COLON);
                std::string value = expect(NODE_STRING);
                expect(PIPE);
                label = text();
                closed(SQE);
                std::map<std::string, std::string> props{{field, value}};
                db.addVertex(id, &label, "rect", nullptr, nullptr, "", &props);
                break;
            }
            case CYLINDERSTART:
                i++;
                label = text();
                closed(CYLINDEREND);
                db.addVertex(id, &label, "cylinder");
                break;
            case DIAMOND_START:
                i++;
                if (types[i] == DIAMOND_START) {
                    i++;
                    label = text();
                    closed(DIAMOND_STOP);
                    expect(DIAMOND_STOP);
                    db.addVertex(id, &label, "hexagon");
                } else {
                    label = text();
                    closed(DIAMOND_STOP);
                    db.addVertex(id, &label, "diamond");
                }
                break;
            case TAGEND:
                i++;
                label = text();
                closed(SQE);
                db.addVertex(id, &label, "odd");
                break;
            case TRAPSTART:
                i++;
                label = text();
                if (types[i] == INVTRAPEND) {
                    i++;
                    db.addVertex(id, &label, "lean_right");
                } else {
                    closed(TRAPEND);
                    db.addVertex(id, &label, "trapezoid");
                }
                break;
            case INVTRAPSTART:
                i++;
                label = text();
                if (types[i] == TRAPEND) {
                    i++;
                    db.addVertex(id, &label, "lean_left");
                } else {
                    closed(INVTRAPEND);
                    db.addVertex(id, &label, "inv_trapezoid");
                }
                break;
            default:
                db.addVertex(id);
                break;
        }
        return id;
    };

    auto styledVertex = [&]() -> std::string {
        std::string id = vertex();
        if (types[i] == STYLE_SEPARATOR) {
            i++;
            db.setClass(id, idString());
        }
        return id;
    };

    auto node = [&]() -> std::vector<std::string> {
        std::vector<std::string> ids = {styledVertex()};
        pendingData.clear();
        hasPendingData = false;
        while (true) {
            bool hasData = (types[i] == SHAPE_DATA);
            std::string data = hasData ? shapeData() : "";
            if (types[i] != SPACE) {
                pendingData = std::move(data);
                hasPendingData = hasData;
                return ids;
            }
            do i++;
            while (types[i] == SPACE);
            if (types[i] != AMP) {
                if (hasData) fail("'AMP'");
                return ids;
            }
            i++;
            if (types[i] != SPACE) fail("'SPACE'");
            do i++;
            while (types[i] == SPACE);
            std::string next = styledVertex();
            if (hasData) {
                db.addVertex(ids.back(), nullptr, "", nullptr, nullptr, "", nullptr, &data);
            }
            ids.push_back(std::move(next));
        }
    };

    auto link = [&]() -> FlowLinkInfo {
        bool hasId = false;
        std::string id;
        if (types[i] == LINK_ID) {
            id = texts[i++];
            hasId = true;
        }
        if (types[i] == LINK) {
            FlowLinkInfo info = db.destructLink(texts[i++]);
            if (hasId) {
                info.id = id;
                info.hasId = true;
            }
            if (types[i] == PIPE) {
                i++;
                info.text = text();
                info.hasText = true;
                closed(PIPE);
                if (types[i] == SPACE) i++;
            }
            return info;
        }
        if (types[i] == START_LINK) {
            std::string start = texts[i++];
            FlowText label = edgeText();
            std::string endStr = expect(LINK, "'EDGE_TEXT'");
            FlowLinkInfo info = db.destructLink(endStr, start);
            info.text = std::move(label);
            info.hasText = true;
            if (hasId) {
                info.id = id;
                info.hasId = true;
            }
            return info;
        }
        fail("'LINK', 'START_LINK'");
        return {};
    };

    auto vertexStatement = [&](bool collect) -> std::vector<std::string> {
        std::vector<std::string> stmt = node();
        if (hasPendingData) {
            db.addVertex(stmt.back(), nullptr, "", nullptr, nullptr, "", nullptr, &pendingData);
        }
        std::vector<std::string> nodesList = stmt;
        while (true) {
            int t = types[i];
            if (t != LINK && t != START_LINK && t != LINK_ID) return nodesList;
            FlowLinkInfo info = link();
            std::vector<std::string> next = node();
            if (hasPendingData) {
                db.addVertex(next.back(), nullptr, "", nullptr, nullptr, "", nullptr, &pendingData);
            }
            db.addLink(stmt, next, &info);
            if (collect) {
                std::vector<std::string> combined = next;
                combined.insert(combined.end(), nodesList.begin(), nodesList.end());
                nodesList = std::move(combined);
            }
            stmt = std::move(next);
        }
    };

    auto styleFn = [&]() -> std::string {
        if (STYLE_PART_TOK[types[i]] == 0) fail("'NODE_STRING', 'NUM', 'COLON', 'BRKT'");
        std::string s = texts[i++];
        while (STYLE_PART_TOK[types[i]] == 1) s += texts[i++];
        return s;
    };

    auto stylesOpt = [&]() -> std::vector<std::string> {
        std::vector<std::string> out = {styleFn()};
        while (types[i] == COMMA) {
            i++;
            out.push_back(styleFn());
        }
        return out;
    };

    auto linkStyleStatement = [&]() {
        i++;
        expect(SPACE);
        std::vector<std::string> positions;
        if (types[i] == DEFAULT) {
            positions.push_back(texts[i++]);
        } else {
            positions.push_back(expect(NUM, "'DEFAULT'"));
            while (types[i] == COMMA) {
                i++;
                positions.push_back(expect(NUM));
            }
        }
        expect(SPACE);
        if (types[i] != INTERPOLATE) {
            db.updateLink(positions, stylesOpt());
            return;
        }
        i++;
        expect(SPACE);
        std::string interpolate = alphaNum();
        if (types[i] == SPACE) {
            i++;
            auto styles = stylesOpt();
            db.updateLinkInterpolate(positions, interpolate);
            db.updateLink(positions, std::move(styles));
        } else {
            db.updateLinkInterpolate(positions, interpolate);
        }
    };

    auto linkTail = [&](const std::string& id, const std::string& url) {
        if (types[i] != SPACE) {
            db.setLink(id, url);
            return;
        }
        i++;
        if (types[i] == LINK_TARGET) {
            db.setLink(id, url, texts[i++]);
            return;
        }
        std::string tooltip = expect(STR, "'LINK_TARGET'");
        if (types[i] == SPACE) {
            i++;
            db.setLink(id, url, expect(LINK_TARGET));
        } else {
            db.setLink(id, url);
        }
        db.setTooltip(id, tooltip);
    };

    auto clickStatement = [&]() {
        std::string id = texts[i++];
        switch (types[i]) {
            case CALLBACKNAME: {
                std::string name = texts[i++];
                bool hasArgs = (types[i] == CALLBACKARGS);
                std::string args = hasArgs ? texts[i++] : "";
                bool hasTooltip = false;
                std::string tooltip;
                if (types[i] == SPACE) {
                    i++;
                    tooltip = expect(STR);
                    hasTooltip = true;
                }
                if (hasArgs) db.setClickEvent(id, name, args);
                else db.setClickEvent(id, name);
                if (hasTooltip) db.setTooltip(id, tooltip);
                return;
            }
            case HREF:
                i++;
                linkTail(id, expect(STR));
                return;
            case STR:
                linkTail(id, texts[i++]);
                return;
            default: {
                std::string name = alphaNum();
                if (types[i] == SPACE) {
                    i++;
                    std::string tooltip = expect(STR);
                    db.setClickEvent(id, name);
                    db.setTooltip(id, tooltip);
                } else {
                    db.setClickEvent(id, name);
                }
            }
        }
    };

    struct Frame {
        FlowText id;
        bool hasId = false;
        FlowText title;
        bool hasTitle = false;
        bool sameIdTitle = false;
        std::vector<DocItem> list;
        bool collect = false;
    };

    auto subgraphHeader = [&]() -> Frame {
        i++;
        int t = types[i];
        if (t == NEWLINE || t == SEMI || t == EOF_TOK) {
            i++;
            return Frame{};
        }
        expect(SPACE, "'SEMI', 'NEWLINE', 'EOF'");
        FlowText id = textNoTags();
        FlowText title = id;
        bool same = true;
        if (types[i] == SQS) {
            i++;
            title = text();
            same = false;
            closed(SQE);
        }
        t = types[i];
        if (t != NEWLINE && t != SEMI && t != EOF_TOK) fail("'SEMI', 'NEWLINE', 'EOF', 'SQS'");
        i++;
        Frame f;
        f.id = std::move(id);
        f.hasId = true;
        f.title = std::move(title);
        f.hasTitle = true;
        f.sameIdTitle = same;
        return f;
    };

    auto statement = [&](bool collect) -> DocItem {
        int t = types[i];
        switch (t) {
            case STYLE: {
                i++;
                expect(SPACE);
                std::string id = idString();
                expect(SPACE);
                auto st = stylesOpt();
                db.addVertex(id, nullptr, "", &st);
                separator();
                return DocItem::makeArr({});
            }
            case LINKSTYLE:
                linkStyleStatement();
                separator();
                return DocItem::makeArr({});
            case CLASSDEF: {
                i++;
                expect(SPACE);
                std::string id = idString();
                expect(SPACE);
                db.addClass(id, stylesOpt());
                separator();
                return DocItem::makeArr({});
            }
            case CLASS: {
                i++;
                expect(SPACE);
                std::string ids = idString();
                expect(SPACE);
                db.setClass(ids, idString());
                separator();
                return DocItem::makeArr({});
            }
            case CLICK:
                clickStatement();
                separator();
                return DocItem::makeArr({});
            case direction_tb:
            case direction_bt:
            case direction_rl:
            case direction_lr:
            case direction_td:
                i++;
                return DocItem::makeDir(DIRECTIONS[t - direction_tb]);
            case acc_title: {
                i++;
                std::string val = trimStr(expect(acc_title_value));
                db.setAccTitle(val);
                return DocItem::makeStr(val);
            }
            case acc_descr: {
                i++;
                std::string val = trimStr(expect(acc_descr_value));
                db.setAccDescription(val);
                return DocItem::makeStr(val);
            }
            case acc_descr_multiline_value: {
                std::string val = trimStr(texts[i++]);
                db.setAccDescription(val);
                return DocItem::makeStr(val);
            }
            default: {
                if (ID_TOK[t] == 0) fail(STATEMENT_START);
                auto nodes = vertexStatement(collect);
                separator();
                return DocItem::makeArr(std::move(nodes));
            }
        }
    };

    auto document = [&]() {
        std::vector<Frame> open;
        std::vector<DocItem> list;
        bool collect = false;
        while (true) {
            int t = types[i];
            if (t == END) break;
            if (t == end) {
                if (open.empty()) break;
                Frame frame = std::move(open.back());
                open.pop_back();
                i++;
                const FlowText* idPtr = frame.hasId ? &frame.id : nullptr;
                const FlowText* titlePtr = frame.sameIdTitle ? idPtr : (frame.hasTitle ? &frame.title : nullptr);
                std::string id = db.addSubGraph(idPtr, list, titlePtr);
                list = std::move(frame.list);
                collect = frame.collect;
                if (collect) list.push_back(DocItem::makeStr(id));
            } else if (t == SEMI || t == NEWLINE || t == SPACE || t == EOF_TOK) {
                if (collect) list.push_back(DocItem::makeStr(texts[i]));
                i++;
            } else if (t == subgraph) {
                Frame frame = subgraphHeader();
                frame.list = std::move(list);
                frame.collect = collect;
                open.push_back(std::move(frame));
                list.clear();
                collect = true;
            } else {
                DocItem item = statement(collect);
                if (collect && (item.kind != DocItem::Arr || !item.arrVal.empty())) {
                    list.push_back(std::move(item));
                }
            }
        }
        if (!open.empty()) fail("'end'");
    };

    while (types[i] == SPACE || types[i] == NEWLINE) i++;
    expect(GRAPH, "'SPACE', 'NEWLINE'");
    if (types[i] == NODIR) {
        i++;
        db.setDirection("TB");
    } else {
        std::string dir = expect(DIR, "'NODIR'");
        int t = types[i];
        if (t == SPACE) {
            do i++;
            while (types[i] == SPACE);
            t = types[i];
            if (t != NEWLINE) fail("'NEWLINE', 'SPACE'");
        } else if (t != SEMI && t != NEWLINE) {
            fail("'SEMI', 'NEWLINE', 'SPACE'");
        }
        i++;
        db.setDirection(dir);
    }
    document();
    if (types[i] != END) fail(STATEMENT_START);
}

// ============================================================================
// Port of src/diagrams/flowchart/graph.ts & render.ts
// ============================================================================

struct GraphNode {
    std::string id;
    std::string label;
    bool hasLabel = false;
    std::string labelType = "text";
    std::string shape = "rect";
    bool isGroup = false;
    std::string parentId;
    bool hasParentId = false;
    std::vector<std::string> cssStyles;
    std::vector<std::string> cssCompiledStyles;
    std::string cssClasses;
    std::string dir;
    std::string link;
    std::string linkTarget;
    std::string tooltip;
    std::string icon;
    std::string pos;
    std::string img;
    double assetWidth = 0.0;
    bool hasAssetWidth = false;
    double assetHeight = 0.0;
    bool hasAssetHeight = false;
    std::string constraint;
    int colorIndex = -1;
    YamlValue metadata = YamlValue::makeNull();
};

struct GraphEdge {
    std::string id;
    bool isUserDefinedId = false;
    std::string start;
    std::string end;
    std::string type = "normal";
    std::string label;
    std::string labelType = "text";
    std::string thickness;
    std::string pattern;
    int minlen = 1;
    bool hasMinlen = false;
    std::string arrowTypeStart = "none";
    std::string arrowTypeEnd = "arrow_point";
    std::vector<std::string> cssCompiledStyles;
    std::vector<std::string> style;
    std::string classes;
    bool animate = false;
    bool hasAnimate = false;
    std::string animation;
    std::string curve;
};

struct FlowGraph {
    std::vector<GraphNode> nodes;
    std::vector<GraphEdge> edges;
};

inline std::string flowShapeOf(const FlowNode& vertex) noexcept {
    if (!vertex.img.empty()) return "imageSquare";
    if (!vertex.icon.empty()) {
        if (vertex.form == "circle") return "iconCircle";
        if (vertex.form == "square") return "iconSquare";
        if (vertex.form == "rounded") return "iconRounded";
        return "icon";
    }
    if (!vertex.hasType || vertex.type.empty() || vertex.type == "square") return "squareRect";
    if (vertex.type == "round") return "roundedRect";
    return vertex.type;
}

inline std::pair<std::string, std::string> flowArrows(const std::string& type) noexcept {
    if (type == "arrow_point" || type == "arrow_circle" || type == "arrow_cross") {
        return {"none", type};
    }
    if (type == "double_arrow_point" || type == "double_arrow_circle" || type == "double_arrow_cross") {
        std::string single = type.substr(7); // remove "double_"
        return {single, single};
    }
    return {"none", "arrow_point"};
}

inline FlowGraph buildFlowGraph(const FlowDb& model, const std::string& defaultCurve = "") noexcept {
    std::vector<GraphNode> nodes;
    std::vector<GraphEdge> edges;
    const auto& subgraphs = model.subgraphs;

    auto compiled = [&](const std::vector<std::string>& classNamesList) -> std::vector<std::string> {
        std::vector<std::string> out;
        for (const auto& name : classNamesList) {
            auto it = model.classes.find(name);
            if (it == model.classes.end()) continue;
            for (const auto& s : it->second.styles) out.push_back(trimStr(s));
            for (const auto& s : it->second.textStyles) out.push_back(trimStr(s));
        }
        return out;
    };

    Pele::FlatMap<std::string, std::string> subgraphParent;
    for (const auto& sg : subgraphs) {
        for (const auto& childId : sg.nodes) {
            if (model.subgraph(childId) != nullptr) subgraphParent[childId] = sg.id;
        }
    }

    Pele::FlatMap<std::string, int> declarationIndex;
    std::unordered_map<std::string, std::vector<std::string>> childrenOf;
    for (const auto& sg : subgraphs) {
        auto it = subgraphParent.find(sg.id);
        if (it == subgraphParent.end()) continue;
        childrenOf[it->second].push_back(sg.id);
    }
    int nextIndex = 0;
    for (const auto& sg : subgraphs) {
        if (subgraphParent.count(sg.id)) continue;
        std::vector<std::string> pending = {sg.id};
        while (!pending.empty()) {
            std::string id = std::move(pending.back());
            pending.pop_back();
            if (declarationIndex.count(id)) continue;
            declarationIndex[id] = nextIndex++;
            auto it = childrenOf.find(id);
            if (it != childrenOf.end()) {
                const auto& kids = it->second;
                for (int k = static_cast<int>(kids.size()) - 1; k >= 0; k--) pending.push_back(kids[k]);
            }
        }
    }

    auto isCollapsed = [&](const std::string& id) -> bool {
        const auto* sg = model.subgraph(id);
        if (!sg || !sg->metadata.isMap()) return false;
        const auto* v = sg->metadata.get("view");
        return v && v->asString() == "collapsed";
    };
    bool anyCollapsed = false;
    for (const auto& sg : subgraphs) {
        if (sg.metadata.isMap()) {
            const auto* v = sg.metadata.get("view");
            if (v && v->asString() == "collapsed") anyCollapsed = true;
        }
    }

    Pele::FlatMap<std::string, std::string> outermost; // empty string = undefined
    auto outermostCollapsed = [&](const std::string& id) -> std::string {
        std::vector<std::string> chain;
        Pele::FlatSet<std::string> seen;
        std::string current = id;
        while (!current.empty() && !outermost.count(current) && !seen.count(current)) {
            seen.insert(current);
            chain.push_back(current);
            auto it = subgraphParent.find(current);
            current = (it != subgraphParent.end()) ? it->second : "";
        }
        std::string result = !current.empty() ? outermost[current] : "";
        for (int k = static_cast<int>(chain.size()) - 1; k >= 0; k--) {
            if (result.empty() && isCollapsed(chain[k])) result = chain[k];
            outermost[chain[k]] = result;
        }
        return result;
    };

    Pele::FlatSet<std::string> hidden;
    Pele::FlatMap<std::string, std::string> collapsedInto;
    if (anyCollapsed) {
        for (const auto& sg : subgraphs) {
            std::string ancestor = outermostCollapsed(sg.id);
            if (ancestor.empty()) continue;
            if (sg.id != ancestor) {
                hidden.insert(sg.id);
                collapsedInto[sg.id] = ancestor;
            }
            for (const auto& childId : sg.nodes) {
                if (childId == ancestor) continue;
                hidden.insert(childId);
                collapsedInto[childId] = ancestor;
            }
        }
    }

    Pele::FlatMap<std::string, std::string> parentOf;
    Pele::FlatSet<std::string> groups;
    for (int i = static_cast<int>(subgraphs.size()) - 1; i >= 0; i--) {
        const auto& sg = subgraphs[i];
        if (hidden.count(sg.id)) continue;
        if (!sg.nodes.empty()) groups.insert(sg.id);
        for (const auto& id : sg.nodes) parentOf[id] = sg.id;
    }

    auto joinClasses = [](const std::vector<std::string>& cls) -> std::string {
        std::string out;
        for (size_t i = 0; i < cls.size(); i++) {
            if (i > 0) out.push_back(' ');
            out += cls[i];
        }
        return out;
    };

    Pele::FlatMap<std::string, size_t> byId;
    for (int i = static_cast<int>(subgraphs.size()) - 1; i >= 0; i--) {
        const auto& sg = subgraphs[i];
        if (hidden.count(sg.id)) continue;
        bool collapsed = isCollapsed(sg.id);
        GraphNode node;
        node.id = sg.id;
        node.label = sg.title;
        node.hasLabel = true;
        node.labelType = sg.labelType;
        auto pIt = parentOf.find(sg.id);
        if (pIt != parentOf.end()) {
            node.parentId = pIt->second;
            node.hasParentId = true;
        }
        node.cssCompiledStyles = compiled(sg.classes);
        node.cssClasses = joinClasses(sg.classes);
        node.shape = collapsed ? "collapsedGroup" : "rect";
        node.dir = sg.dir;
        node.isGroup = !collapsed;
        auto dIt = declarationIndex.find(sg.id);
        if (dIt != declarationIndex.end()) node.colorIndex = dIt->second;
        if (!collapsed) node.metadata = sg.metadata;
        byId[sg.id] = nodes.size();
        nodes.push_back(std::move(node));
    }

    for (const auto& nodeId : model.nodeOrder) {
        if (hidden.count(nodeId)) continue;
        const auto& vertex = model.nodes.at(nodeId);
        auto exIt = byId.find(vertex.id);
        if (exIt != byId.end()) {
            auto& existing = nodes[exIt->second];
            existing.cssStyles = vertex.styles;
            existing.cssCompiledStyles = compiled(vertex.classes);
            existing.cssClasses = joinClasses(vertex.classes);
            continue;
        }
        bool isGroup = groups.count(vertex.id) > 0;
        GraphNode node;
        node.id = vertex.id;
        node.label = vertex.text;
        node.hasLabel = vertex.hasText;
        node.labelType = vertex.labelType;
        auto pIt = parentOf.find(vertex.id);
        if (pIt != parentOf.end()) {
            node.parentId = pIt->second;
            node.hasParentId = true;
        }
        node.cssStyles = vertex.styles;
        std::vector<std::string> allCls = {"default", "node"};
        allCls.insert(allCls.end(), vertex.classes.begin(), vertex.classes.end());
        node.cssCompiledStyles = compiled(allCls);
        node.cssClasses = Pele::cat({"default ", joinClasses(vertex.classes)});
        node.dir = vertex.dir;
        node.link = vertex.link;
        node.linkTarget = vertex.linkTarget;
        auto tIt = model.tooltips.find(vertex.id);
        if (tIt != model.tooltips.end()) node.tooltip = tIt->second;
        node.icon = vertex.icon;
        node.pos = vertex.pos;
        node.img = vertex.img;
        node.assetWidth = vertex.assetWidth;
        node.hasAssetWidth = vertex.hasAssetWidth;
        node.assetHeight = vertex.assetHeight;
        node.hasAssetHeight = vertex.hasAssetHeight;
        node.constraint = vertex.constraint;
        node.isGroup = isGroup;
        node.shape = isGroup ? "rect" : flowShapeOf(vertex);
        nodes.push_back(std::move(node));
    }

    for (size_t index = 0; index < model.edges.size(); index++) {
        const auto& raw = model.edges[index];
        auto sIt = collapsedInto.find(raw.start);
        std::string start = sIt != collapsedInto.end() ? sIt->second : raw.start;
        auto eIt = collapsedInto.find(raw.end);
        std::string end = eIt != collapsedInto.end() ? eIt->second : raw.end;
        if (start == end && (sIt != collapsedInto.end() || eIt != collapsedInto.end())) continue;

        auto [arrowStart, arrowEnd] = flowArrows(raw.type);
        std::vector<std::string> styles;
        if (model.hasDefaultEdgeStyle) styles = model.defaultEdgeStyle;
        if (raw.hasStyle) styles.insert(styles.end(), raw.style.begin(), raw.style.end());
        bool plain = raw.stroke == "invisible" || raw.type == "arrow_open";
        GraphEdge ge;
        ge.id = !raw.id.empty() ? raw.id : ("L_" + start + "_" + end + "_" + std::to_string(index));
        ge.isUserDefinedId = raw.isUserDefinedId;
        ge.start = start;
        ge.end = end;
        ge.type = raw.hasType ? raw.type : "normal";
        ge.label = raw.text;
        ge.labelType = raw.labelType;
        ge.thickness = raw.stroke;
        ge.pattern = raw.stroke;
        ge.minlen = raw.length;
        ge.hasMinlen = raw.hasLength;
        ge.classes = raw.stroke == "invisible" ? "" : "edge-thickness-normal edge-pattern-solid flowchart-link";
        ge.arrowTypeStart = plain ? "none" : arrowStart;
        ge.arrowTypeEnd = plain ? "none" : arrowEnd;
        ge.cssCompiledStyles = compiled(raw.classes);
        ge.style = std::move(styles);
        ge.animate = raw.animate;
        ge.hasAnimate = raw.hasAnimate;
        ge.animation = raw.animation;
        ge.curve = !raw.interpolate.empty() ? raw.interpolate : (!model.defaultInterpolate.empty() ? model.defaultInterpolate : defaultCurve);
        edges.push_back(std::move(ge));
    }

    return {std::move(nodes), std::move(edges)};
}

struct FlowVariant {
    std::string type;
    std::function<void(FlowGraph&)> prepare;
    std::function<CompoundResult(std::vector<CNode>&, std::vector<CEdge>&, const std::string&, const LayeredOptions&, bool)> layout;
    std::function<std::string(const CNode&, const Label&, const ResolvedStyle&, const std::string&, const std::string&, const std::string&, const IconResolver&)> drawGroup;
    std::function<std::array<double, 4>(const CNode&, const std::string&)> filledBox;
};

struct NodeView {
    GraphNode node;
    ResolvedStyle style;
    Label label;
    std::string shape;
    double w = 0.0;
    double h = 0.0;
    double dy = 0.0;
    int loops = 0;
};

inline double flowNumberOption(const Config& section, const std::string& key, double fallback) noexcept {
    if (!section.isMap()) return fallback;
    const auto* v = section.get(key);
    if (v && v->isNumber() && v->nVal > 0.0) return std::min(v->nVal, 2000.0);
    return fallback;
}

inline double flowAssetSize(bool hasValue, double value, double fallback) noexcept {
    return (hasValue && value > 0.0 && value <= 4000.0) ? value : fallback;
}

inline Rendered drawFlowchart(
    const FlowDb& db,
    const Config& config,
    const RenderOptions& options,
    const FlowVariant* variant,
    bool turned,
    bool tight
) {
    constexpr double NODE_SEP = 40.0;
    constexpr double EDGE_SEP = 16.0;
    constexpr double RANK_SEP = 48.0;
    constexpr double GROUP_PAD = 20.0;
    constexpr double LOOP = 26.0;
    const std::string SHAPE_ATTRS = " fill=\"var(--_s)\" stroke=\"var(--_b)\"";
    const std::string LINE_ATTRS = " stroke=\"var(--_b)\"";

    double groupPad = tight ? 12.0 : GROUP_PAD;
    const YamlValue* own = (variant && config.isMap()) ? config.get(variant->type) : nullptr;
    const YamlValue* flowCfg = (own && own->isMap()) ? own : (config.isMap() ? config.get("flowchart") : nullptr);
    YamlValue emptyMap = YamlValue::makeMap();
    const Config& flow = (flowCfg && flowCfg->isMap()) ? *flowCfg : emptyMap;

    std::string defaultCurve;
    if (const auto* cv = flow.get("curve"); cv && cv->isString()) defaultCurve = cv->sVal;
    FlowGraph graph = buildFlowGraph(db, defaultCurve);
    if (variant && variant->prepare) variant->prepare(graph);

    double size = options.hasFontSize ? options.fontSize : 16.0;
    double edgeSize = std::round(size * 0.875);
    const TextMeasurer& measurer = getMeasurer(options);
    double wrapWidth = std::min(flowNumberOption(flow, "wrappingWidth", 200.0), (variant && tight) ? 100.0 : std::numeric_limits<double>::infinity());
    double pad = options.hasPadding ? options.padding : 8.0;
    const IconResolver& icons = options.icons;

    Pele::FlatMap<std::string, int> index;
    std::vector<NodeView> views;
    std::vector<CNode> cnodes;
    views.reserve(graph.nodes.size());
    cnodes.reserve(graph.nodes.size());
    for (size_t i = 0; i < graph.nodes.size(); i++) {
        index[graph.nodes[i].id] = static_cast<int>(i);
    }

    for (const auto& node : graph.nodes) {
        std::vector<std::string> allStyles = node.cssCompiledStyles;
        if (!node.cssStyles.empty()) {
            allStyles.insert(allStyles.end(), node.cssStyles.begin(), node.cssStyles.end());
        }
        ResolvedStyle style = resolveStyle(allStyles);
        int textStyleFlags = (style.bold ? Style::Bold : 0) | (style.italic ? Style::Italic : 0);
        bool markdown = node.labelType == "markdown";
        double fontSize = style.hasFontSize ? style.fontSize : size;
        NodeView view;
        CNode c;
        if (node.isGroup) {
            double maxRoom = (variant && tight) ? std::max(80.0, titleRoomExact(options) / 2.0) : 4000.0;
            Label label = layoutLabel(node.hasLabel ? node.label : "", markdown, measurer, fontSize, maxRoom, textStyleFlags);
            view = {node, style, std::move(label), "group", 0.0, 0.0, 0.0, 0};
            c = cnode(0.0, 0.0);
            c.isGroup = true;
            c.dir = !node.dir.empty() ? direction(node.dir) : "";
            c.padX = groupPad;
            c.padTop = view.label.height > 0.0 ? view.label.height + 16.0 : GROUP_PAD;
            c.padBottom = groupPad;
            c.minW = view.label.width + 2.0 * groupPad - 12.0;
        } else {
            std::string shape = canonicalShape(node.shape);
            std::string textStr = (!node.img.empty() || !node.icon.empty() || shapeHasLabel(shape)) ? (node.hasLabel ? node.label : "") : "";
            Label label = layoutLabel(textStr, markdown, measurer, fontSize, wrapWidth, textStyleFlags);
            double w = 0.0, h = 0.0, dy = 0.0;
            if (!node.img.empty() || !node.icon.empty()) {
                double box = !node.img.empty() ? 80.0 : 48.0;
                double aw = flowAssetSize(node.hasAssetWidth, node.assetWidth, box);
                double ah = flowAssetSize(node.hasAssetHeight, node.assetHeight, box);
                w = std::max(aw, label.width + 8.0);
                h = ah + (label.height > 0.0 ? label.height + 6.0 : 0.0);
                dy = node.pos == "t" ? -ah / 2.0 : ah / 2.0;
                if (label.height == 0.0) dy = 0.0;
            } else {
                ShapeSize s = shapeSize(shape, label.width, label.height);
                w = s.w;
                h = s.h;
                dy = s.dy;
            }
            view = {node, style, std::move(label), shape, w, h, dy, 0};
            c = cnode(w, h);
        }
        c.seq = static_cast<int>(views.size());
        views.push_back(std::move(view));
        cnodes.push_back(std::move(c));
    }

    for (size_t i = 0; i < graph.nodes.size(); i++) {
        if (graph.nodes[i].hasParentId) {
            auto it = index.find(graph.nodes[i].parentId);
            cnodes[i].parent = (it != index.end()) ? it->second : -1;
        } else {
            cnodes[i].parent = -1;
        }
    }

    std::vector<std::pair<int, int>> ends;
    ends.reserve(graph.edges.size());
    for (const auto& edge : graph.edges) {
        auto sIt = index.find(edge.start);
        auto eIt = index.find(edge.end);
        ends.push_back({sIt != index.end() ? sIt->second : -1, eIt != index.end() ? eIt->second : -1});
    }
    inheritCrossedDirections(cnodes, ends);

    std::string rootDir = turned ? "TB" : direction(db.direction);
    auto flowsSideways = [&](size_t i) -> bool {
        std::string dir;
        for (int p = cnodes[i].parent, hops = 0; p >= 0 && dir.empty() && hops < 10000; p = cnodes[p].parent, hops++) {
            dir = cnodes[p].dir;
        }
        if (dir.empty()) dir = rootDir;
        return dir == "LR" || dir == "RL";
    };

    for (size_t i = 0; i < views.size(); i++) {
        const auto& view = views[i];
        if (view.node.isGroup) cnodes[i].span = std::numeric_limits<double>::infinity();
        else if (view.node.img.empty() && view.node.icon.empty()) {
            cnodes[i].span = shapeSpan(view.shape, view.w, view.h, flowsSideways(i));
        }
    }

    std::vector<CEdge> cedges;
    std::vector<Label> edgeLabels;
    std::vector<GraphEdge> drawn;
    for (const auto& edge : graph.edges) {
        auto sIt = index.find(edge.start);
        auto dIt = index.find(edge.end);
        if (sIt == index.end() || dIt == index.end()) continue;
        int src = sIt->second;
        int dst = dIt->second;
        Label label = layoutLabel(edge.label, edge.labelType == "markdown", measurer, edgeSize, wrapWidth);
        if (src == dst) {
            views[src].loops++;
            if (flowsSideways(src)) cnodes[src].h += 2.0 * (LOOP + (label.height > 0.0 ? label.height + 8.0 : 0.0));
            else cnodes[src].w += 2.0 * (LOOP + (label.width > 0.0 ? label.width + 12.0 : 0.0));
        }
        drawn.push_back(edge);
        CEdge ce;
        ce.src = src;
        ce.dst = dst;
        ce.minlen = edge.hasMinlen ? edge.minlen : 1;
        ce.labelW = label.width > 0.0 ? label.width + 12.0 : 0.0;
        ce.labelH = label.height > 0.0 ? label.height + 4.0 : 0.0;
        cedges.push_back(std::move(ce));
        edgeLabels.push_back(std::move(label));
    }

    LayeredOptions layoutOptions;
    layoutOptions.nodeSep = flowNumberOption(flow, "nodeSpacing", NODE_SEP) * (tight ? 0.5 : 1.0);
    layoutOptions.edgeSep = EDGE_SEP;
    layoutOptions.rankSep = flowNumberOption(flow, "rankSpacing", RANK_SEP) * (tight ? 0.75 : 1.0);
    layoutOptions.portSep = 20.0;
    layoutOptions.tracks = !defaultCurve.empty() && startsWith(defaultCurve, "step");
    layoutOptions.straight = defaultCurve == "linear";
    layoutOptions.headRoom = markerTrim("arrow_point");

    CompoundResult layout = (variant && variant->layout)
        ? variant->layout(cnodes, cedges, rootDir, layoutOptions, tight)
        : compoundLayout(cnodes, cedges, rootDir, layoutOptions);

    Label title = titleLabel(db.title, measurer, size, tight ? titleRoomExact(options) : 4000.0);
    double titleHeight = title.height > 0.0 ? title.height + 12.0 : 0.0;
    double innerWidth = std::max(layout.width, title.width);
    double ox = pad + (innerWidth - layout.width) / 2.0;
    double oy = pad + titleHeight;
    shiftLayout(cnodes, cedges, ox, oy);
    double width = std::ceil(innerWidth + 2.0 * pad);
    double height = std::ceil(layout.height + titleHeight + 2.0 * pad);
    std::vector<LinkInfo> links;

    Crossings crossings;
    bool anyGroup = false;
    for (const auto& c : cnodes) if (c.isGroup) anyGroup = true;
    if (anyGroup) {
        for (const auto& e : cedges) markCrossings(crossings, e.route);
    }

    std::string clusters;
    std::string struckTitles;
    std::string nodesOut;
    std::vector<Fill> fills;
    for (size_t i = 0; i < views.size(); i++) {
        const auto& view = views[i];
        const auto& c = cnodes[i];
        const auto& node = view.node;
        double x = c.x;
        double y = c.y;
        std::string rawCls = node.cssClasses;
        if (startsWith(rawCls, "default ")) rawCls = rawCls.substr(8);
        else if (rawCls == "default") rawCls.clear();
        std::string classes = classNames(rawCls);
        std::string id = escText(node.id);
        if (c.isGroup && variant && variant->drawGroup && c.parent < 0) {
            if (paints(view.style.shape) && variant->filledBox) {
                auto b = variant->filledBox(c, rootDir);
                fills.push_back(Fill{{b[0], b[1], b[2], b[3]}, view.style.shape});
            }
            clusters += variant->drawGroup(c, view.label, view.style, classes, id, rootDir, icons);
            continue;
        }
        if (c.isGroup) {
            if (paints(view.style.shape)) {
                fills.push_back(Fill{{x - c.w / 2.0, y - c.h / 2.0, x + c.w / 2.0, y + c.h / 2.0}, view.style.shape});
            }
            double titleX = clusterTitleX(c, view.label.width, crossings, groupPad);
            double titleY = y - c.h / 2.0 + 8.0 + view.label.height / 2.0;
            bool struck = titleCrossed(c, view.label.width, titleX, crossings);
            std::string titleAttrs = Pele::cat({" class=\"pele-cluster-label\"", (struck ? std::string(" fill=\"var(--_m)\"") : inkFill(view.style, "var(--_m)")), view.style.text});
            if (struck) struckTitles += struckTitle(view.label, titleX, titleY, titleAttrs, id, cnodes, i, icons);
            Pele::appendCat(clusters, {"<g class=\"pele-cluster", classes, "\" data-id=\"", id, "\">"
                "<rect x=\"", num(x - c.w / 2.0), "\" y=\"", num(y - c.h / 2.0), "\" width=\"", num(c.w), "\" height=\"", num(c.h), "\" fill=\"var(--_a)\"", (setsFill(view.style.shape) ? "" : " fill-opacity=\"0.5\""), " stroke=\"var(--_b)\"", rounded(view.style.shape), "/>", (struck ? "" : labelSvg(view.label, titleX, titleY, titleAttrs, icons)), "</g>"});
            continue;
        }

        double w = view.w;
        double h = view.h;
        std::string inner;
        std::string inkStr;
        if (!node.img.empty()) {
            double aw = flowAssetSize(node.hasAssetWidth, node.assetWidth, 80.0);
            double ah = flowAssetSize(node.hasAssetHeight, node.assetHeight, 80.0);
            double top = view.label.height > 0.0 ? (node.pos == "t" ? h / 2.0 - ah : -h / 2.0) : -ah / 2.0;
            std::string src = imageUrl(sanitizeUrl(node.img), options);
            inner = src.empty()
                ? ""
                : Pele::cat({"<image href=\"", esc(src), "\" x=\"", num(-aw / 2.0), "\" y=\"", num(top), "\" width=\"", num(aw), "\" height=\"", num(ah), "\" preserveAspectRatio=\"", (node.constraint == "on" ? "xMidYMid meet" : "none"), "\"/>"});
        } else if (!node.icon.empty()) {
            double box = flowAssetSize(node.hasAssetHeight, node.assetHeight, 48.0);
            double top = view.label.height > 0.0 ? (node.pos == "t" ? h / 2.0 - box : -h / 2.0) : -box / 2.0;
            std::string frame;
            if (node.shape == "icon") {
                frame = "";
            } else if (node.shape == "iconCircle") {
                frame = Pele::cat({"<circle cy=\"", num(top + box / 2.0), "\" r=\"", num(box / 2.0), "\"", SHAPE_ATTRS, view.style.shape, "/>"});
            } else {
                frame = Pele::cat({"<rect x=\"", num(-box / 2.0), "\" y=\"", num(top), "\" width=\"", num(box), "\" height=\"", num(box), "\" rx=\"", (node.shape == "iconRounded" ? "8" : "0"), "\"", SHAPE_ATTRS, view.style.shape, "/>"});
            }
            double glyph = box * 0.6;
            inner = Pele::cat({frame, "<svg class=\"pele-icon\" data-icon=\"", escText(node.icon), "\" x=\"", num(-glyph / 2.0), "\" y=\"", num(top + (box - glyph) / 2.0), "\" width=\"", num(glyph), "\" height=\"", num(glyph), "\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\">", (icons ? icons(node.icon) : ""), "</svg>"});
        } else if (view.shape == "diam") {
            auto jn = [](double v) { return num(jsRound(v * 100.0) / 100.0); };
            inner = Pele::cat({"<polygon points=\"0,", jn(-h / 2.0), " ", jn(w / 2.0), ",0 0,", jn(h / 2.0), " ", jn(-w / 2.0), ",0\"", SHAPE_ATTRS, view.style.shape, "/>"});
            if (shapeFilled(view.shape)) inkStr = inkFill(view.style);
        } else {
            inner = drawShape(view.shape, w, h, SHAPE_ATTRS + view.style.shape, LINE_ATTRS + view.style.line);
            if (shapeFilled(view.shape)) inkStr = inkFill(view.style);
        }

        std::string text = labelSvg(view.label,  0.0,  view.dy, Pele::cat({" class=\"pele-label\"", inkStr, view.style.text}),  icons);
        bool isInternal = Pele::cat({classes, " "}).find(" internal-link ") != std::string::npos;
        if (isInternal && node.hasLabel && !node.label.empty() && options.links) {
            std::string name = decodeEntities(node.label);
            bool safe = internalLink(name, options);
            std::string target = esc(name);
            text = Pele::cat({"<a class=\"internal-link\"", (safe ? Pele::cat({" href=\"", target, "\"", relAttr(options)}) : ""), " data-href=\"", target, "\">", text, "</a>"});
            links.push_back({node.id, name, true});
        }
        std::string body = inner + text;
        if (!node.tooltip.empty()) body = "<title>" + escText(node.tooltip) + "</title>" + body;
        if (!node.link.empty()) {
            body = linked(body, node.link, node.id, links, options, !node.linkTarget.empty() ? Pele::cat({" target=\"", esc(node.linkTarget), "\""}) : "");
        }
        Pele::appendCat(nodesOut, {"<g class=\"pele-node pele-shape-", view.shape, classes, "\" data-id=\"", id, "\" transform=\"translate(", num(x), ",", num(y), ")\">", body, "</g>"});
    }

    SideReach reach;
    for (size_t i = 0; i < cedges.size(); i++) {
        const auto& ce = cedges[i];
        if (ce.src == ce.dst || drawn[i].thickness == "invisible" || ce.route.size() < 6) continue;
        reach.add(ce.src, endSide(ce.route, 0, cnodes[ce.src]), drawn[i].arrowTypeStart);
        reach.add(ce.dst, endSide(ce.route, ce.route.size() - 3, cnodes[ce.dst]), drawn[i].arrowTypeEnd);
    }

    std::string casings;
    std::string edgesOut;
    std::string labelsOut;
    Pele::FlatMap<int, int> loopSeen;
    for (size_t i = 0; i < cedges.size(); i++) {
        auto& ce = cedges[i];
        const auto& edge = drawn[i];
        const auto& label = edgeLabels[i];
        bool hasStyle = !edge.style.empty() || !edge.cssCompiledStyles.empty();
        ResolvedStyle style;
        if (hasStyle) {
            std::vector<std::string> combined = edge.cssCompiledStyles;
            combined.insert(combined.end(), edge.style.begin(), edge.style.end());
            style = resolveStyle(combined);
        }
        if (edge.thickness != "invisible") {
            const std::string& startType = edge.arrowTypeStart;
            const std::string& endType = edge.arrowTypeEnd;
            EdgePath path;
            if (ce.src == ce.dst) {
                const auto& view = views[ce.src];
                int k = loopSeen[ce.src]++;
                path = loopPath(cnodes[ce.src], view.w, view.h, k, flowsSideways(ce.src), LOOP, markerTrim(endType), ce);
            } else {
                std::vector<double> route = ce.route;
                double leaves = reach.of(ce.src, endSide(route, 0, cnodes[ce.src]));
                double arrives = reach.of(ce.dst, endSide(route, route.size() - 3, cnodes[ce.dst]));
                insetRoute(route, 0, views[ce.src], cnodes[ce.src]);
                insetRoute(route, route.size() - 3, views[ce.dst], cnodes[ce.dst]);
                path = routePath(route, edge.curve, markerTrim(startType), markerTrim(endType), leaves, arrives);
            }
            OverFillResult over = overFills(path.d, {path.sx, path.sy, path.ex, path.ey}, fills, hasStyle ? style.line : "", path.hasTokens ? &path.tokens : nullptr);
            std::string color = !over.value.empty()
                ? over.value
                : ((hasStyle && style.hasStroke && edge.thickness != "invisible") ? esc(style.stroke) : "var(--_l)");
            std::string attrs;
            if (edge.thickness == "thick") attrs += " stroke-width=\"2.5\"";
            else if (edge.thickness == "dotted") attrs += " stroke-dasharray=\"3 4\"";
            std::string rawEdgeCls = edge.classes;
            rawEdgeCls = replaceAllStr(rawEdgeCls, "edge-thickness-normal", "");
            rawEdgeCls = replaceAllStr(rawEdgeCls, "edge-pattern-solid", "");
            rawEdgeCls = replaceAllStr(rawEdgeCls, "flowchart-link", "");
            std::string classes = classNames(rawEdgeCls);
            if (over.onFill && over.value.empty()) {
                casings += edgeCasing(path, startType, endType, hasStyle ? style.line : "", edge.thickness == "thick");
            }
            Pele::appendCat(edgesOut, {"<g class=\"pele-edge", classes, "\" data-id=\"", escText(edge.id), "\"", (!over.value.empty() ? Pele::cat({" stroke=\"", over.value, "\""}) : ""), (hasStyle ? style.line : ""), ">"
                "<path d=\"", path.d, "\"", attrs, "/>", marker(startType, path.sx, path.sy, path.sdx, path.sdy, color), marker(endType, path.ex, path.ey, path.edx, path.edy, color), "</g>"});
        }
        if (label.width > 0.0) {
            labelsOut += edgeLabelSvg(edge.id, label, ce.labelX, ce.labelY, hasStyle ? style.text : "", icons);
        }
    }

    std::string svg = svgDocument(
        variant ? variant->type : "flowchart", 
        width, 
        height, 
        size, 
        options, 
        db, Pele::cat({labelSvg(title, width / 2.0, pad + title.height / 2.0, " class=\"pele-title\" font-weight=\"var(--_tw)\""), (!clusters.empty() ? Pele::cat({"<g class=\"pele-clusters\">", clusters, "</g>"}) : ""), casingsSvg(casings), (!edgesOut.empty() ? Pele::cat({"<g class=\"pele-edges\" fill=\"none\" stroke=\"var(--_l)\" stroke-linecap=\"round\">", edgesOut, "</g>"}) : ""), (!struckTitles.empty() ? Pele::cat({"<g class=\"pele-cluster-titles\">", struckTitles, "</g>"}) : ""), (!labelsOut.empty() ? Pele::cat({"<g class=\"pele-edge-labels\" font-size=\"", num(edgeSize), "\">", labelsOut, "</g>"}) : ""), "<g class=\"pele-nodes\">", nodesOut, "</g>"}));

    return {std::move(svg), width, height, std::move(links)};
}

inline Rendered renderFlowchartModel(const FlowDb& db, const Config& config, const RenderOptions& options, const FlowVariant* variant = nullptr) noexcept {
    auto fitted = [&](bool down) -> Rendered {
        return tightenExact(options, [&](bool tight) {
            return drawFlowchart(db, config, options, variant, down, tight);
        });
    };
    return runsAcross(db.direction) ? turnToFit(options, fitted) : fitted(false);
}

inline Rendered render(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options = {}) {
    const YamlValue* flowCfg = config.isMap() ? config.get("flowchart") : nullptr;
    FlowDbOptions dbOpts;
    if (flowCfg && flowCfg->isMap()) {
        if (const auto* inh = flowCfg->get("inheritDir"); inh && inh->isBool() && inh->bVal) {
            dbOpts.inheritDir = true;
        }
    }
    dbOpts.maxEdges = 5000;
    dbOpts.hasMaxEdges = true;
    FlowDb db(dbOpts);
    if (!title.empty()) db.title = title;
    parseFlowchart(source, db);

    RenderOptions eff = options;
    if (!eff.hasResponsive) {
        bool fixed = (flowCfg && flowCfg->isMap() && flowCfg->get("useMaxWidth") && flowCfg->get("useMaxWidth")->isBool() && !flowCfg->get("useMaxWidth")->bVal);
        eff.responsive = !fixed;
        eff.hasResponsive = true;
    }
    IconResolver wrappedIcons;
    if (eff.icons) {
        auto orig = eff.icons;
        wrappedIcons = [orig](const std::string& name) { return orig(decodeEntities(name)); };
        eff.icons = wrappedIcons;
    }
    Rendered res = withMath(eff.math, [&]() {
        return withIcons(eff.icons, [&]() {
            return renderFlowchartModel(db, config, eff);
        });
    });
    for (auto& link : res.links) link.id = decodeEntities(link.id);
    return res;
}

inline Rendered render(const std::string& source, const Config& config, const RenderOptions& options, const std::string& title = "") {
    return render(source, config, title, options);
}

} // namespace Flowchart

inline Rendered renderFlowchart(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options = {}) {
    return Flowchart::render(source, config, title, options);
}

inline Rendered renderFlowchart(const std::string& source, const Config& config, const RenderOptions& options, const std::string& title = "") {
    return Flowchart::render(source, config, title, options);
}

inline Rendered renderFlowchart(const std::string& source, const RenderOptions& options = {}) {
    auto pre = preprocess(source);
    std::string parsed = Pele::cat({encodeEntities(pre.text), "\n"});
    return Flowchart::render(parsed, pre.config, pre.title, options);
}

} // namespace Pele
