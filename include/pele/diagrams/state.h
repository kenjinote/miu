#pragma once
#include "../svg.h"
#include "../layout.h"
#include "../common_parse.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Pele {
namespace StateDiagram {

// ============================================================================
// src/layout/layered.ts (exact 1:1 port)
// ============================================================================

namespace detail_layered_exact {

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
constexpr double IN_LINE_L = 0.5;
constexpr double SNAP = 12.0;
constexpr double LEAN_CLEAR = 40.0;
constexpr int MAX_WEIGHED = 400;
constexpr int SAMPLES = 16;

enum class End : uint8_t {
    Held = 0,
    Leaves = 1,
    Arrives = 2,
};

inline bool isDummy(uint8_t kind) {
    return kind == static_cast<uint8_t>(LKind::Dummy) || kind == static_cast<uint8_t>(LKind::Label);
}

inline void sortRange(std::vector<int>& list, int from, int to, const std::vector<int>& key) {
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

inline void sortByBary(
    std::vector<int>& list,
    int count,
    const std::vector<double>& bary,
    const std::vector<int>& pos,
    bool right,
    std::vector<int>& spare
) {
    auto before = [&](int a, int b) -> bool {
        return bary[a] < bary[b] || (bary[a] == bary[b] && (right ? pos[a] > pos[b] : pos[a] < pos[b]));
    };
    constexpr int RUN = 8;
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
            while (i < mid && j < end) {
                (*to)[k++] = before((*from)[j], (*from)[i]) ? (*from)[j++] : (*from)[i++];
            }
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
    std::vector<double>& W;
    std::vector<uint8_t>& KIND;
    std::vector<int>& RANK;
    std::vector<int>& EF;
    std::vector<int>& ET;
    std::vector<double>& EFD;
    std::vector<double>& ETD;
    std::vector<int>& succStart;
    std::vector<int>& succE;
    std::vector<int>& predStart;
    std::vector<int>& predE;
    std::vector<int>& layerStart;
    std::vector<int>& order;
    std::vector<int>& pos;
    bool offsets = false;
    std::vector<uint8_t>& fixed;
    bool hasGRP = false;
    std::vector<int>& GRP;
    std::vector<int>& groupParent;
};

inline double halfRoom(const Graph& g, const LayeredOptions& opt, int v) {
    return g.W[v] / 2.0 + (g.KIND[v] == static_cast<uint8_t>(LKind::Node) ? opt.nodeSep : opt.edgeSep) / 2.0;
}

inline void orient(std::vector<LNode>& nodes, std::vector<LEdge>& edges) {
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
        for (int i = 0; i < m; i++) {
            if (!edges[i].reversed) out[fill[edges[i].tail]++] = i;
        }
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

inline void rank(std::vector<LNode>& nodes, const std::vector<LEdge>& edges, int step = 1) {
    int n = static_cast<int>(nodes.size());
    int m = static_cast<int>(edges.size());
    std::vector<int> from(m, 0);
    std::vector<int> to(m, 0);
    std::vector<int> len(m, 0);
    std::vector<int> outStart(n + 1, 0);
    std::vector<int> inStart(n + 1, 0);
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
    std::vector<int> outE(m, 0);
    std::vector<int> inE(m, 0);
    {
        std::vector<int> of(outStart.begin(), outStart.begin() + n);
        std::vector<int> inf(inStart.begin(), inStart.begin() + n);
        for (int i = 0; i < m; i++) {
            outE[of[from[i]]++] = i;
            inE[inf[to[i]]++] = i;
        }
    }

    std::vector<int> r(n, 0);
    std::vector<int> indeg(n, 0);
    for (int i = 0; i < n; i++) indeg[i] = inStart[i + 1] - inStart[i];
    std::vector<int> topo(n, 0);
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
            int lo = std::numeric_limits<int>::min();
            int hi = std::numeric_limits<int>::max();
            for (int k = inStart[v]; k < inStart[v + 1]; k++) {
                int ei = inE[k];
                lo = std::max(lo, r[from[ei]] + len[ei]);
            }
            for (int k = outStart[v]; k < outStart[v + 1]; k++) {
                int ei = outE[k];
                hi = std::min(hi, r[to[ei]] - len[ei]);
            }
            int next = r[v];
            if (outs > ins && hi != std::numeric_limits<int>::max()) next = hi;
            else if (ins > outs && lo != std::numeric_limits<int>::min()) next = lo;
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

inline double sideEnds(
    const Graph& g,
    int v,
    int side,
    double span,
    const std::vector<uint8_t>& kinds,
    std::vector<int>& ids,
    std::vector<double>& room
) {
    const auto& start = (side == 0) ? g.predStart : g.succStart;
    const auto& list = (side == 0) ? g.predE : g.succE;
    const auto& other = (side == 0) ? g.EF : g.ET;
    const auto& pos = g.pos;
    for (int k = start[v]; k < start[v + 1]; k++) {
        if (kinds[list[k]] != static_cast<uint8_t>(End::Held)) ids.push_back(list[k]);
    }
    if (ids.size() < 2) return 0.0;
    std::sort(ids.begin(), ids.end(), [&](int a, int b) {
        if (pos[other[a]] != pos[other[b]]) return pos[other[a]] < pos[other[b]];
        return a < b;
    });
    room.resize(ids.size(), 0.0);
    auto fill = [&](auto share) -> double {
        double total = 0.0;
        room[0] = 0.0;
        for (size_t k = 1; k < ids.size(); k++) {
            bool same = (kinds[ids[k]] == kinds[ids[k - 1]]);
            room[k] = share(same && kinds[ids[k]] == static_cast<uint8_t>(End::Leaves), same);
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

inline void spread(Graph& g, const std::vector<LNode>& nodes, const std::vector<uint8_t>& EFK, const std::vector<uint8_t>& ETK) {
    const auto& real = g.real;
    const auto& W = g.W;
    auto& EFD = g.EFD;
    auto& ETD = g.ETD;
    for (int v = 0; v < real; v++) {
        double span = std::min(nodes[v].span, W[v]);
        if (span <= 0.0 || nodes[v].kind != LKind::Node) continue;
        for (int side = 0; side < 2; side++) {
            std::vector<int> ids;
            std::vector<double> room;
            double total = sideEnds(g, v, side, span, side == 0 ? ETK : EFK, ids, room);
            if (total == 0.0) continue;
            auto& dx = (side == 0) ? ETD : EFD;
            double at = -total / 2.0;
            for (size_t k = 0; k < ids.size(); k++) {
                at += room[k];
                dx[ids[k]] = at;
            }
        }
    }
}

inline int follow(const Graph& g, const std::vector<uint8_t>& empty, int e, int side) {
    if (empty.empty()) return e;
    const auto& KIND = g.KIND;
    const auto& RANK = g.RANK;
    const auto& EF = g.EF;
    const auto& ET = g.ET;
    const auto& succStart = g.succStart;
    const auto& succE = g.succE;
    const auto& predStart = g.predStart;
    const auto& predE = g.predE;
    for (int hops = 0; hops < 100000; hops++) {
        int u = (side == 0) ? EF[e] : ET[e];
        if (KIND[u] != static_cast<uint8_t>(LKind::Dummy) || !empty[RANK[u]]) break;
        e = (side == 0) ? predE[predStart[u]] : succE[succStart[u]];
    }
    return e;
}

inline std::array<std::vector<int>, 2> freeEnds(const Graph& g, const std::vector<uint8_t>& EFK, const std::vector<uint8_t>& ETK) {
    std::array<std::vector<int>, 2> free{std::vector<int>(g.n, 0), std::vector<int>(g.n, 0)};
    for (size_t e = 0; e < g.EF.size(); e++) {
        if (ETK[e] != static_cast<uint8_t>(End::Held)) free[0][g.ET[e]]++;
        if (EFK[e] != static_cast<uint8_t>(End::Held)) free[1][g.EF[e]]++;
    }
    return free;
}

inline double ease(double across, double along) {
    double a = std::abs(across);
    return std::max(0.25, std::min(0.5, (4.0 * std::abs(along)) / (a != 0.0 ? a : 1.0)));
}

inline void alignLabels(
    Graph& g,
    const std::vector<LNode>& nodes,
    std::vector<double>& xs,
    const std::array<std::vector<int>, 2>& free,
    const LayeredOptions& opt,
    const std::vector<uint8_t>& empty
) {
    double sep = opt.edgeSep;
    const auto& n = g.n;
    const auto& real = g.real;
    const auto& maxRank = g.maxRank;
    const auto& KIND = g.KIND;
    const auto& RANK = g.RANK;
    const auto& W = g.W;
    const auto& EF = g.EF;
    const auto& ET = g.ET;
    const auto& EFD = g.EFD;
    const auto& ETD = g.ETD;
    const auto& succStart = g.succStart;
    const auto& succE = g.succE;
    const auto& predStart = g.predStart;
    const auto& predE = g.predE;
    const auto& layerStart = g.layerStart;
    const auto& order = g.order;
    const auto& pos = g.pos;

    auto placed = [&](int v) -> bool {
        return KIND[v] == static_cast<uint8_t>(LKind::Label) ||
               (KIND[v] == static_cast<uint8_t>(LKind::Dummy) && !(!empty.empty() && empty[RANK[v]] == 1));
    };
    auto end = [&](int e, int side) -> std::pair<double, int> {
        int u = (side == 0) ? EF[e] : ET[e];
        double x = (side == 0) ? (xs[u] + EFD[e]) : (xs[u] + ETD[e]);
        return {x, (u < real && nodes[u].kind == LKind::Node) ? free[1 - side][u] : 0};
    };
    auto room = [&](int v) -> std::pair<double, double> {
        int s = layerStart[RANK[v]];
        int i = s + pos[v];
        auto clear = [&](int other) -> double {
            return W[other] / 2.0 + W[v] / 2.0 +
                   (KIND[other] == static_cast<uint8_t>(LKind::Node) ? (opt.nodeSep / 2.0 + sep / 2.0) : (KIND[other] == KIND[v] ? sep : sep / 2.0));
        };
        double lo = (i > s) ? (xs[order[i - 1]] + clear(order[i - 1])) : -std::numeric_limits<double>::infinity();
        double hi = (i + 1 < layerStart[RANK[v] + 1]) ? (xs[order[i + 1]] - clear(order[i + 1])) : std::numeric_limits<double>::infinity();
        return {lo, hi};
    };
    auto choose = [](double x, std::pair<double, int> up, std::pair<double, int> down, double lo, double hi) -> double {
        bool upFirst = (up.second != down.second) ? (up.second < down.second) : (std::abs(up.first - x) <= std::abs(down.first - x));
        double c0 = upFirst ? up.first : down.first;
        double c1 = upFirst ? down.first : up.first;
        if (c0 >= lo && c0 <= hi) return c0;
        if (c1 >= lo && c1 <= hi) return c1;
        return std::max(lo, std::min(hi, c0));
    };

    std::vector<uint8_t> done(n, 0);
    std::vector<int> chain;
    for (int r = 1; r < maxRank; r++) {
        for (int i = layerStart[r]; i < layerStart[r + 1]; i++) {
            if (!placed(order[i]) || done[order[i]]) continue;
            int top = order[i];
            while (isDummy(KIND[EF[predE[predStart[top]]]])) top = EF[predE[predStart[top]]];
            int bottom = top;
            double lo = -std::numeric_limits<double>::infinity();
            double hi = std::numeric_limits<double>::infinity();
            chain.clear();
            for (int d = top; isDummy(KIND[d]); d = ET[succE[succStart[d]]]) {
                bottom = d;
                if (!placed(d)) continue;
                done[d] = 1;
                chain.push_back(d);
                auto [least, most] = room(d);
                if (least > lo) lo = least;
                if (most < hi) hi = most;
            }
            if (lo <= hi) {
                double x = choose(xs[chain[0]], end(predE[predStart[top]], 0), end(succE[succStart[bottom]], 1), lo, hi);
                for (int d : chain) xs[d] = x;
                continue;
            }
            for (int d : chain) {
                auto [least, most] = room(d);
                if (least > most) continue;
                xs[d] = choose(xs[d], end(follow(g, empty, predE[predStart[d]], 0), 0), end(follow(g, empty, succE[succStart[d]], 1), 1), least, most);
            }
        }
    }
}

inline void settle(
    Graph& g,
    const std::vector<LNode>& nodes,
    std::vector<double>& xs,
    const std::vector<uint8_t>& EFK,
    const std::vector<uint8_t>& ETK,
    const std::array<std::vector<int>, 2>& free,
    const std::vector<uint8_t>& empty
) {
    const auto& real = g.real;
    const auto& W = g.W;
    const auto& EF = g.EF;
    const auto& ET = g.ET;
    auto& EFD = g.EFD;
    auto& ETD = g.ETD;
    const auto& succStart = g.succStart;
    const auto& succE = g.succE;
    const auto& predStart = g.predStart;
    const auto& predE = g.predE;
    const auto& pos = g.pos;

    auto wanted = [&](int e, int side, int v, double span, bool again) -> double {
        if (free[side][v] < 2) return 0.0;
        e = follow(g, empty, e, side);
        int u = (side == 0) ? EF[e] : ET[e];
        double point = (side == 0) ? (xs[u] + EFD[e]) : (xs[u] + ETD[e]);
        if (again && std::abs(point - xs[v]) <= span / 2.0 + IN_LINE_L) {
            return std::max(-span / 2.0, std::min(span / 2.0, point - xs[v]));
        }
        double middle = point;
        double width = 0.0;
        if (u < real && nodes[u].kind == LKind::Node && ((side == 0) ? EFK : ETK)[e] != static_cast<uint8_t>(End::Held) && free[1 - side][u] >= 2) {
            width = std::max(0.0, std::min(nodes[u].span, W[u]));
            if (width > 0.0) middle = xs[u];
        }
        double lo = std::max(xs[v] - span / 2.0, middle - width / 2.0);
        double hi = std::min(xs[v] + span / 2.0, middle + width / 2.0);
        if (lo > hi + IN_LINE_L) {
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
    for (int v = 0; v < real; v++) {
        double span = std::min(nodes[v].span, W[v]);
        if (span <= 0.0 || nodes[v].kind != LKind::Node) continue;
        for (int side = 0; side < 2; side++) {
            const auto& other = (side == 0) ? EF : ET;
            const auto& start = (side == 0) ? predStart : succStart;
            const auto& list = (side == 0) ? predE : succE;
            std::vector<int> ids;
            std::vector<double> room;
            sideEnds(g, v, side, span, side == 0 ? ETK : EFK, ids, room);
            if (ids.empty()) continue;
            if (ids.size() == 1) room = {0.0};
            std::vector<int> all;
            for (int k = start[v]; k < start[v + 1]; k++) all.push_back(list[k]);
            std::sort(all.begin(), all.end(), [&](int a, int b) {
                if (pos[other[a]] != pos[other[b]]) return pos[other[a]] < pos[other[b]];
                return a < b;
            });
            sides.push_back({v, side, span, std::move(ids), std::move(room), std::move(all)});
        }
    }

    for (int pass = 0, moved = 1; moved && pass < 6; pass++) {
        moved = 0;
        for (const auto& sinfo : sides) {
            int v = sinfo.v;
            int side = sinfo.side;
            double span = sinfo.span;
            const auto& ids = sinfo.ids;
            const auto& room = sinfo.room;
            const auto& all = sinfo.all;
            const auto& kinds = (side == 0) ? ETK : EFK;
            auto& dx = (side == 0) ? ETD : EFD;
            size_t k = 0;
            double lo = -span / 2.0;
            for (size_t i = 0; i <= all.size(); i++) {
                bool held = (i < all.size() && kinds[all[i]] == static_cast<uint8_t>(End::Held));
                if (i < all.size() && !held) continue;
                size_t size = 0;
                for (int j = static_cast<int>(i) - 1; j >= 0 && kinds[all[j]] != static_cast<uint8_t>(End::Held); j--) size++;
                double hi = held ? (dx[all[i]] - PORT_PITCH) : (span / 2.0);
                if (size > 0) {
                    std::vector<double> sum;
                    std::vector<int> count;
                    double before = 0.0;
                    for (size_t j = 0; j < size; j++) {
                        if (j > 0) before += room[k + j];
                        sum.push_back(wanted(ids[k + j], side, v, span, pass > 0) - before);
                        count.push_back(1);
                        bool together = (j > 0 && room[k + j] == 0.0);
                        while (sum.size() > 1 && (together || sum[sum.size() - 2] / count[count.size() - 2] > sum.back() / count.back())) {
                            together = false;
                            double top = sum.back();
                            sum.pop_back();
                            int pooled = count.back();
                            count.pop_back();
                            sum.back() += top;
                            count.back() += pooled;
                        }
                    }
                    double total = before;
                    size_t j = 0;
                    before = 0.0;
                    for (size_t b = 0; b < sum.size(); b++) {
                        double level = (hi - total < lo) ? ((lo + hi - total) / 2.0) : std::max(lo, std::min(hi - total, sum[b] / count[b]));
                        for (int c = 0; c < count[b]; c++, j++) {
                            if (j > 0) before += room[k + j];
                            if (std::abs(dx[ids[k + j]] - level - before) > 0.01) moved = 1;
                            dx[ids[k + j]] = level + before;
                        }
                    }
                    k += size;
                }
                if (held) lo = dx[all[i]] + PORT_PITCH;
            }
        }
    }
}

inline void snap(Graph& g, std::vector<double>& xs, const std::vector<uint8_t>& empty, const LayeredOptions& opt) {
    const auto& n = g.n;
    const auto& KIND = g.KIND;
    const auto& RANK = g.RANK;
    const auto& EF = g.EF;
    const auto& ET = g.ET;
    const auto& EFD = g.EFD;
    const auto& ETD = g.ETD;
    const auto& layerStart = g.layerStart;
    const auto& order = g.order;
    const auto& pos = g.pos;

    auto passed = [&](int u) -> bool {
        return !empty.empty() && KIND[u] == static_cast<uint8_t>(LKind::Dummy) && empty[RANK[u]] == 1;
    };
    std::vector<int> a;
    std::vector<int> b;
    std::vector<double> da;
    std::vector<double> db;
    for (int e = 0; e < static_cast<int>(EF.size()); e++) {
        if (passed(EF[e])) continue;
        int f = follow(g, empty, e, 1);
        a.push_back(EF[e]);
        b.push_back(ET[f]);
        da.push_back(EFD[e]);
        db.push_back(ETD[f]);
    }
    auto off = [&](int k) -> double {
        return xs[b[k]] + db[k] - xs[a[k]] - da[k];
    };
    std::vector<int> block(n, 0);
    std::vector<std::vector<int>> members(n);
    for (int i = 0; i < n; i++) {
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
    std::vector<int> nearList;
    for (int k = 0; k < static_cast<int>(a.size()); k++) {
        double d = std::abs(off(k));
        if (d < 0.01) join(a[k], b[k]);
        else if (d <= SNAP) nearList.push_back(k);
    }
    if (nearList.empty()) return;
    std::sort(nearList.begin(), nearList.end(), [&](int x, int y) {
        double dx = std::abs(off(x));
        double dy = std::abs(off(y));
        if (dx != dy) return dx < dy;
        return x < y;
    });
    auto gap = [&](int v) -> double { return halfRoom(g, opt, v); };
    auto isFree = [&](int id, double shift) -> bool {
        for (int v : members[id]) {
            int s = layerStart[RANK[v]];
            int end = layerStart[RANK[v] + 1];
            int i = s + pos[v];
            if (i > s && block[order[i - 1]] != id && xs[v] + shift - xs[order[i - 1]] < gap(v) + gap(order[i - 1]) - 0.01) return false;
            if (i + 1 < end && block[order[i + 1]] != id && xs[order[i + 1]] - xs[v] - shift < gap(v) + gap(order[i + 1]) - 0.01) return false;
        }
        return true;
    };
    for (int k : nearList) {
        double d = off(k);
        if (std::abs(d) < 0.01) {
            join(a[k], b[k]);
            continue;
        }
        if (std::abs(d) > SNAP || block[a[k]] == block[b[k]]) continue;
        int upper = block[a[k]];
        int lower = block[b[k]];
        int first = (members[upper].size() <= members[lower].size()) ? upper : lower;
        for (int id : {first, first == upper ? lower : upper}) {
            double shift = (id == upper) ? d : -d;
            if (!isFree(id, shift)) continue;
            for (int v : members[id]) xs[v] += shift;
            join(a[k], b[k]);
            break;
        }
    }
}

inline std::vector<uint8_t> emptyRanks(const Graph& g) {
    const auto& maxRank = g.maxRank;
    const auto& KIND = g.KIND;
    const auto& layerStart = g.layerStart;
    const auto& order = g.order;
    std::vector<uint8_t> empty(maxRank + 1, 0);
    bool any = false;
    for (int r = 1; r < maxRank; r++) {
        bool only = (layerStart[r + 1] > layerStart[r]);
        for (int i = layerStart[r]; only && i < layerStart[r + 1]; i++) {
            if (KIND[order[i]] != static_cast<uint8_t>(LKind::Dummy)) only = false;
        }
        if (only) {
            empty[r] = 1;
            any = true;
        }
    }
    return any ? empty : std::vector<uint8_t>{};
}

inline void assignTracks(
    Pieces& pieces,
    std::vector<int>& trackCount,
    bool curved,
    const std::vector<double>& room,
    std::vector<double>& tall
) {
    const auto& from = pieces.from;
    const auto& to = pieces.to;
    const auto& rank = pieces.rank;
    auto& track = pieces.track;
    auto& levels = pieces.levels;
    const auto& wide = pieces.wide;
    int total = static_cast<int>(from.size());
    std::vector<int> rankOrder;
    std::unordered_map<int, std::vector<int>> byRank;
    for (int k = 0; k < total; k++) {
        track.push_back(-1);
        levels.push_back(0);
        if (std::abs(from[k] - to[k]) < IN_LINE_L) continue;
        auto it = byRank.find(rank[k]);
        if (it == byRank.end()) {
            rankOrder.push_back(rank[k]);
            byRank[rank[k]] = {k};
        } else {
            it->second.push_back(k);
        }
    }
    if (byRank.empty()) return;

    std::vector<double> LO(total, 0.0);
    std::vector<double> HI(total, 0.0);
    for (int k = 0; k < total; k++) {
        LO[k] = std::min(from[k], to[k]);
        HI[k] = std::max(from[k], to[k]);
    }
    auto within = [&](double x, int k) -> int {
        return (x >= LO[k] - IN_LINE_L && x <= HI[k] + IN_LINE_L) ? 1 : 0;
    };
    auto mates = [&](int a, int b) -> bool {
        return std::abs(from[a] - from[b]) < IN_LINE_L || std::abs(to[a] - to[b]) < IN_LINE_L;
    };
    auto overlap = [&](int a, int b) -> bool {
        return LO[a] < HI[b] - IN_LINE_L && LO[b] < HI[a] - IN_LINE_L;
    };
    auto clash = [&](int a, int b) -> bool {
        return overlap(a, b) && !mates(a, b);
    };
    auto meet = [&](int a, int b) -> bool {
        return overlap(a, b) || (within(to[a], b) + within(from[b], a) + within(to[b], a) + within(from[a], b)) > 0;
    };
    auto crossings = [&](int a, int b) -> int {
        return within(to[a], b) + within(from[b], a);
    };
    auto nearPairs = [&](const std::vector<int>& list) -> std::vector<int> {
        std::vector<int> byLo(list.size(), 0);
        for (size_t i = 0; i < list.size(); i++) byLo[i] = static_cast<int>(i);
        std::stable_sort(byLo.begin(), byLo.end(), [&](int a, int b) {
            return LO[list[a]] < LO[list[b]];
        });
        std::vector<int> pairs;
        for (size_t x = 0; x < byLo.size(); x++) {
            int i = byLo[x];
            double reach = HI[list[i]] + 4.0 * IN_LINE_L;
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
        std::vector<int> group(count, 0);
        for (int i = 0; i < count; i++) group[i] = i;
        auto find = [&](int i) -> int {
            while (group[i] != i) i = group[i] = group[group[i]];
            return i;
        };
        bool weighed = (count <= MAX_WEIGHED);
        std::vector<int> pairs = weighed ? nearPairs(list) : std::vector<int>{};
        std::vector<std::vector<int>> clashes(weighed ? count : 0);
        for (size_t p = 0; p < pairs.size(); p += 2) {
            int i = pairs[p];
            int j = pairs[p + 1];
            if (!clash(list[i], list[j])) continue;
            clashes[i].push_back(j);
            clashes[j].push_back(i);
        }
        std::vector<int> sequence(count, 0);
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
                int first = (ab < ba) ? i : j;
                int second = (ab < ba) ? j : i;
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
        std::unordered_set<int> taken;
        std::vector<std::vector<int>> onTrack;
        int rows = 0;
        for (int at : sequence) {
            int k = list[at];
            int t = 0;
            if (at < static_cast<int>(above.size())) {
                for (int i : above[at]) {
                    if (track[list[i]] >= t) t = track[list[i]] + 1;
                }
            }
            if (weighed) {
                taken.clear();
                for (int i : clashes[at]) taken.insert(track[list[i]]);
                while (taken.count(t)) t++;
            } else {
                for (;;) {
                    bool conflict = false;
                    if (t < static_cast<int>(onTrack.size())) {
                        for (int other : onTrack[t]) {
                            if (clash(k, other)) {
                                conflict = true;
                                break;
                            }
                        }
                    }
                    if (!conflict) break;
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

    std::array<double, 2 * SAMPLES + 2> lineA{};
    std::array<double, 2 * SAMPLES + 2> lineB{};
    auto sample = [&](int k, double h, std::array<double, 2 * SAMPLES + 2>& out) {
        double c = ease(to[k] - from[k], h);
        for (int i = 0; i <= SAMPLES; i++) {
            double t = static_cast<double>(i) / SAMPLES;
            double u = 1.0 - t;
            out[2 * i] = from[k] + (to[k] - from[k]) * t * t * (3.0 - 2.0 * t);
            out[2 * i + 1] = h * (3.0 * c * t * u * u + 3.0 * (1.0 - c) * t * t * u + t * t * t);
        }
    };
    auto reaches = [](const std::array<double, 2 * SAMPLES + 2>& points, const std::array<double, 2 * SAMPLES + 2>& line, double limit) -> bool {
        double bound = limit * limit * (1.0 + 1e-9);
        for (int i = 0; i <= SAMPLES; i++) {
            double px = points[2 * i];
            double py = points[2 * i + 1];
            for (int j = 0; j < SAMPLES; j++) {
                double dx = line[2 * j + 2] - line[2 * j];
                double dy = line[2 * j + 3] - line[2 * j + 1];
                double denom = dx * dx + dy * dy;
                double t = std::max(0.0, std::min(1.0, ((px - line[2 * j]) * dx + (py - line[2 * j + 1]) * dy) / (denom != 0.0 ? denom : 1.0)));
                double ex = px - line[2 * j] - t * dx;
                double ey = py - line[2 * j + 1] - t * dy;
                if (ex * ex + ey * ey < bound && std::hypot(ex, ey) < limit) return true;
            }
        }
        return false;
    };
    auto nearCurve = [&](int a, int b, double height, double limit) -> bool {
        double h = std::max(height / 2.0, height - 2.0 * LEVEL_LEAD);
        sample(a, h, lineA);
        sample(b, h, lineB);
        return reaches(lineA, lineB, limit) || reaches(lineB, lineA, limit);
    };

    for (int r : rankOrder) {
        auto& all = byRank[r];
        if (!curved) {
            trackCount[r] = lay(all);
            continue;
        }
        if (static_cast<int>(all.size()) > MAX_WEIGHED) continue;
        std::vector<int> far;
        for (int k : all) if (wide[k] == 1) far.push_back(k);
        lay(far);
        std::unordered_set<int> levelSet;
        for (int k : far) if (levels[k] > 1) levelSet.insert(k);
        std::vector<int> pairs = nearPairs(all);
        for (size_t p = 0; p < pairs.size(); p += 2) {
            int a = all[pairs[p]];
            int b = all[pairs[p + 1]];
            if ((levelSet.count(a) && levelSet.count(b)) || !clash(a, b) || (from[a] < to[a]) != (from[b] < to[b])) continue;
            if (nearCurve(a, b, room[r], (wide[a] == 1 || wide[b] == 1) ? LEVEL_CLEAR : LEVEL_TOUCH)) {
                levelSet.insert(a);
                levelSet.insert(b);
            }
        }
        for (int k : far) {
            track[k] = -1;
            levels[k] = 0;
        }
        if (levelSet.empty()) continue;
        int crossing = 0;
        int alongside = 0;
        std::unordered_set<int> beside;
        for (size_t p = 0; p < pairs.size(); p += 2) {
            int a = all[pairs[p]];
            int b = all[pairs[p + 1]];
            if (levelSet.count(a) && levelSet.count(b)) {
                if (!overlap(a, b)) continue;
                if ((from[a] < to[a]) != (from[b] < to[b])) crossing++;
                else if (!mates(a, b)) alongside++;
            } else if ((levelSet.count(a) || levelSet.count(b)) && meet(a, b)) {
                beside.insert(a);
                beside.insert(b);
            }
        }
        if (crossing > alongside) {
            for (int k : all) {
                if ((HI[k] - LO[k]) * CROSSED_SLOPE > tall[r]) tall[r] = (HI[k] - LO[k]) * CROSSED_SLOPE;
            }
            continue;
        }
        std::vector<int> members;
        for (int k : all) {
            if (levelSet.count(k) || (wide[k] != 1 && beside.count(k))) members.push_back(k);
        }
        lay(members);
        int most = 0;
        for (int k : members) if (levels[k] > most) most = levels[k];
        trackCount[r] = most;
    }
}

inline void straighten(
    const Graph& g,
    std::vector<double>& xs,
    const std::vector<uint8_t>& empty,
    const std::vector<int>& firstDummy,
    const std::vector<int>& span,
    double sep,
    std::vector<uint8_t>& onLine
) {
    const auto& maxRank = g.maxRank;
    const auto& EF = g.EF;
    const auto& ET = g.ET;
    const auto& EFD = g.EFD;
    const auto& ETD = g.ETD;
    const auto& succStart = g.succStart;
    const auto& succE = g.succE;
    const auto& predStart = g.predStart;
    const auto& predE = g.predE;
    const auto& layerStart = g.layerStart;
    const auto& order = g.order;

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
            int above = predE[predStart[d]];
            int below = succE[succStart[last]];
            double x0 = xs[EF[above]] + EFD[above];
            double x1 = xs[ET[below]] + ETD[below];
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
    for (int r = 1; r < maxRank; r++) {
        if (!empty[r]) continue;
        int s = layerStart[r];
        int end = layerStart[r + 1];
        std::vector<int> row(order.begin() + s, order.begin() + end);
        std::sort(row.begin(), row.end(), [&](int a, int b) {
            if (from[a] != from[b]) return from[a] < from[b];
            if (to[a] != to[b]) return to[a] < to[b];
            return a < b;
        });
        for (size_t i = 0; i < row.size();) {
            twins.clear();
            size_t j = i;
            for (; j < row.size() && std::abs(from[row[j]] - from[row[i]]) < 1.0 && std::abs(to[row[j]] - to[row[i]]) < 1.0; j++) {
                twins.push_back(row[j]);
            }
            std::sort(twins.begin(), twins.end(), [&](int a, int b) {
                return g.pos[a] < g.pos[b];
            });
            for (size_t k = 0; k < twins.size(); k++) {
                int d = twins[k];
                xs[d] = want[d] + (static_cast<double>(k) - (static_cast<double>(twins.size()) - 1.0) / 2.0) * sep;
                if (twins.size() == 1 || sep == 0.0) onLine[d] = 1;
            }
            i = j;
        }
    }
}

inline void pastLabels(
    const Graph& g,
    std::vector<double>& xs,
    const std::vector<int>& firstDummy,
    const std::vector<int>& span,
    double clear,
    std::vector<uint8_t>& onLine
) {
    const auto& maxRank = g.maxRank;
    const auto& W = g.W;
    const auto& KIND = g.KIND;
    const auto& RANK = g.RANK;
    const auto& EF = g.EF;
    const auto& ET = g.ET;
    const auto& EFD = g.EFD;
    const auto& ETD = g.ETD;
    const auto& succStart = g.succStart;
    const auto& succE = g.succE;
    const auto& predStart = g.predStart;
    const auto& predE = g.predE;
    const auto& layerStart = g.layerStart;
    const auto& order = g.order;
    std::vector<uint8_t> labelled(maxRank + 1, 0);
    bool any = false;
    for (int r = 1; r < maxRank; r++) {
        bool labels = false;
        bool others = false;
        for (int i = layerStart[r]; i < layerStart[r + 1]; i++) {
            uint8_t kind = KIND[order[i]];
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
            if (KIND[d] != static_cast<uint8_t>(LKind::Dummy) || !labelled[RANK[d]]) continue;
            int first = d;
            int last = d;
            while (first > d0 && onLine[first - 1]) first--;
            while (last + 1 < end && onLine[last + 1]) last++;
            int above = predE[predStart[first]];
            int below = succE[succStart[last]];
            double from = xs[EF[above]] + EFD[above];
            double to = xs[ET[below]] + ETD[below];
            double lo = std::min(from, to) - clear;
            double hi = std::max(from, to) + clear;
            bool isFree = true;
            int r = RANK[d];
            for (int i = layerStart[r]; isFree && i < layerStart[r + 1]; i++) {
                int label = order[i];
                if (KIND[label] == static_cast<uint8_t>(LKind::Label) && xs[label] + W[label] / 2.0 > lo && xs[label] - W[label] / 2.0 < hi) {
                    isFree = false;
                }
            }
            if (!isFree) continue;
            onLine[d] = 1;
            int steps = last - first + 2;
            for (int k = first; k <= last; k++) {
                xs[k] = from + ((to - from) * (k - first + 1)) / steps;
            }
        }
    }
}

inline std::pair<int, int> reduceCrossings(Graph& g, const std::vector<LNode>& nodes) {
    const auto& n = g.n;
    const auto& real = g.real;
    const auto& maxRank = g.maxRank;
    const auto& W = g.W;
    const auto& EF = g.EF;
    const auto& ET = g.ET;
    const auto& EFD = g.EFD;
    const auto& ETD = g.ETD;
    const auto& succStart = g.succStart;
    const auto& succE = g.succE;
    const auto& predStart = g.predStart;
    const auto& predE = g.predE;
    const auto& layerStart = g.layerStart;
    auto& order = g.order;
    auto& pos = g.pos;
    const auto& offsets = g.offsets;
    const auto& fixed = g.fixed;
    const auto& hasGRP = g.hasGRP;
    const auto& GRP = g.GRP;
    const auto& groupParent = g.groupParent;

    int widest = 0;
    for (int r = 0; r <= maxRank; r++) widest = std::max(widest, layerStart[r + 1] - layerStart[r]);
    if (widest < 2) return {0, 0};

    bool hasPlace = offsets;
    std::vector<int> place(hasPlace ? EF.size() : 0, 0);
    std::vector<int> placeIn(hasPlace ? EF.size() : 0, 0);
    std::vector<int> places(hasPlace ? n : 0, 0);
    std::vector<int> leaving = hasPlace ? succE : std::vector<int>{};
    std::vector<uint8_t> mixed(hasPlace ? n : 0, 0);
    if (hasPlace) {
        std::vector<int> arriving = predE;
        auto byOffset = [](std::vector<int>& list, int from, int to, const std::vector<double>& dx) -> bool {
            for (int k = from + 1; k < to; k++) {
                if (dx[list[k]] == dx[list[from]]) continue;
                std::stable_sort(list.begin() + from, list.begin() + to, [&](int a, int b) {
                    return dx[a] < dx[b];
                });
                return true;
            }
            return false;
        };
        for (int v = 0; v < n; v++) {
            places[v] = 1;
            if (byOffset(arriving, predStart[v], predStart[v + 1], ETD)) {
                for (int k = predStart[v] + 1; k < predStart[v + 1]; k++) {
                    if (ETD[arriving[k]] != ETD[arriving[k - 1]]) places[v]++;
                    placeIn[arriving[k]] = places[v] - 1;
                }
            }
            if (byOffset(leaving, succStart[v], succStart[v + 1], EFD)) mixed[v] = 1;
        }
    }
    int size = 1;
    int limit = hasPlace ? (widest + static_cast<int>(EF.size())) : widest;
    while (size < limit) size <<= 1;
    std::vector<int> tree(2 * size, 0);
    std::vector<int> targets(EF.size(), 0);

    auto count = [&]() -> int {
        int total = 0;
        for (int r = 0; r < maxRank; r++) {
            int southLen = layerStart[r + 2] - layerStart[r + 1];
            if (hasPlace) {
                int next = 0;
                for (int i = layerStart[r + 1]; i < layerStart[r + 2]; i++) {
                    int v = order[i];
                    for (int k = predStart[v]; k < predStart[v + 1]; k++) place[predE[k]] = next + placeIn[predE[k]];
                    next += places[v];
                }
                southLen = next;
            }
            if (southLen < 2) continue;
            int t = 0;
            for (int i = layerStart[r]; i < layerStart[r + 1]; i++) {
                int v = order[i];
                int from = succStart[v];
                int to = succStart[v + 1];
                int t0 = t;
                for (int k = from; k < to; k++) targets[t++] = hasPlace ? place[leaving[k]] : pos[ET[succE[k]]];
                int d = t - t0;
                if (d < 2) continue;
                if (hasPlace && mixed[v]) {
                    for (int a = t0 + 1; a < t; a++) {
                        int item = targets[a];
                        double dx = EFD[leaving[from + a - t0]];
                        int b = a - 1;
                        while (b >= t0 && EFD[leaving[from + b - t0]] == dx && targets[b] > item) {
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
    std::vector<double> bary(n, 0.0);
    std::vector<int> moving(widest, 0);
    std::vector<int> spare(widest, 0);

    int groups = hasGRP ? static_cast<int>(groupParent.size()) : 0;
    std::vector<int> depth(groups, 0);
    for (int gr = 0; gr < groups; gr++) for (int p = gr; p >= 0; p = groupParent[p]) depth[gr]++;
    std::vector<double> groupSum(groups, 0.0);
    std::vector<int> groupCount(groups, 0);
    std::vector<int> groupFirst(groups, 0);
    auto arrange = [&](int r, const std::vector<double>& key) {
        int s = layerStart[r];
        int e = layerStart[r + 1];
        for (int i = s; i < e; i++) {
            for (int gr = GRP[order[i]]; gr >= 0; gr = groupParent[gr]) {
                groupSum[gr] = 0.0;
                groupCount[gr] = 0;
            }
        }
        for (int i = s; i < e; i++) {
            int v = order[i];
            for (int gr = GRP[v]; gr >= 0; gr = groupParent[gr]) {
                if (groupCount[gr] == 0) groupFirst[gr] = i;
                groupSum[gr] += key[v];
                groupCount[gr]++;
            }
        }
        auto value = [&](int item) -> double { return item >= n ? (groupSum[item - n] / groupCount[item - n]) : key[item]; };
        auto first = [&](int item) -> int { return item >= n ? groupFirst[item - n] : (s + pos[item]); };
        std::vector<int> row(order.begin() + s, order.begin() + e);
        std::stable_sort(row.begin(), row.end(), [&](int a, int b) {
            int ia = a, ib = b;
            int ca = GRP[a], cb = GRP[b];
            int da = ca < 0 ? 0 : depth[ca];
            int db = cb < 0 ? 0 : depth[cb];
            for (; da > db; da--) {
                ia = n + ca;
                ca = groupParent[ca];
            }
            for (; db > da; db--) {
                ib = n + cb;
                cb = groupParent[cb];
            }
            while (ca != cb) {
                ia = n + ca;
                ca = groupParent[ca];
                ib = n + cb;
                cb = groupParent[cb];
            }
            double va = value(ia), vb = value(ib);
            if (va != vb) return va < vb;
            return first(ia) < first(ib);
        });
        for (int i = s; i < e; i++) {
            order[i] = row[i - s];
            pos[order[i]] = i - s;
        }
    };
    if (hasGRP) {
        std::vector<double> at(n, 0.0);
        for (int r = 0; r <= maxRank; r++) {
            for (int i = layerStart[r]; i < layerStart[r + 1]; i++) at[order[i]] = i - layerStart[r];
            arrange(r, at);
        }
    }
    int best = hasGRP ? count() : declared;
    if (best == 0) return {0, declared};
    std::vector<int> bestOrder = order;
    int start = std::min(best, declared);

    auto sweep = [&](bool down, bool biasRight) {
        const auto& st = down ? predStart : succStart;
        const auto& list = down ? predE : succE;
        const auto& other = down ? EF : ET;
        const auto& otherDx = down ? EFD : ETD;
        for (int step = 1; step <= maxRank; step++) {
            int r = down ? step : maxRank - step;
            if (fixed[r]) continue;
            int s = layerStart[r];
            int e = layerStart[r + 1];
            int movable = 0;
            for (int i = s; i < e; i++) {
                int v = order[i];
                int pin = v < real ? nodes[v].pin : 0;
                if (pin != 0) {
                    bary[v] = pin * 1e9;
                    moving[movable++] = v;
                    continue;
                }
                int from = st[v];
                int to = st[v + 1];
                if (from == to) {
                    if (hasGRP) {
                        int across = down ? (layerStart[r] - layerStart[r - 1]) : (layerStart[r + 2] - layerStart[r + 1]);
                        bary[v] = (static_cast<double>(i - s) * across) / (e - s);
                        continue;
                    }
                    bary[v] = -1.0;
                    continue;
                }
                double sum = 0.0;
                for (int k = from; k < to; k++) {
                    int u = other[list[k]];
                    sum += pos[u];
                    if (offsets && W[u] > 0.0) sum += otherDx[list[k]] / W[u];
                }
                bary[v] = sum / (to - from);
                moving[movable++] = v;
            }
            if (hasGRP) {
                arrange(r, bary);
                continue;
            }
            if (movable < 2) continue;
            sortByBary(moving, movable, bary, pos, biasRight, spare);
            int k = 0;
            for (int i = s; i < e; i++) if (bary[order[i]] != -1.0) order[i] = moving[k++];
            for (int i = s; i < e; i++) pos[order[i]] = i - s;
        }
    };

    auto outOfOrder = [&]() -> int {
        int total = 0;
        for (int r = 0; r <= maxRank; r++) {
            for (int i = layerStart[r]; i < layerStart[r + 1]; i++) {
                if (order[i] >= real) continue;
                for (int j = i + 1; j < layerStart[r + 1]; j++) if (order[j] < order[i]) total++;
            }
        }
        return total;
    };
    auto settlePositions = [&]() {
        for (int r = 0; r <= maxRank; r++) for (int i = layerStart[r]; i < layerStart[r + 1]; i++) pos[order[i]] = i - layerStart[r];
    };

    auto pinOf = [&](int v) -> int { return v < real ? nodes[v].pin : 0; };
    auto atPos = [&](int e, bool up) -> double {
        int u = up ? EF[e] : ET[e];
        return (offsets && W[u] > 0.0) ? (pos[u] + (up ? EFD[e] : ETD[e]) / W[u]) : pos[u];
    };
    auto between = [&](int a, int b) -> int {
        int c = 0;
        for (int k = predStart[a]; k < predStart[a + 1]; k++) {
            double x = atPos(predE[k], true);
            for (int j = predStart[b]; j < predStart[b + 1]; j++) if (x > atPos(predE[j], true)) c++;
        }
        for (int k = succStart[a]; k < succStart[a + 1]; k++) {
            double x = atPos(succE[k], false);
            for (int j = succStart[b]; j < succStart[b + 1]; j++) if (x > atPos(succE[j], false)) c++;
        }
        return c;
    };
    auto transpose = [&]() {
        for (int pass = 0; pass < 8; pass++) {
            bool improved = false;
            for (int r = 0; r <= maxRank; r++) {
                if (fixed[r]) continue;
                int s = layerStart[r];
                for (int i = s; i + 1 < layerStart[r + 1]; i++) {
                    int a = order[i];
                    int b = order[i + 1];
                    if (pinOf(a) != pinOf(b) || (hasGRP && GRP[a] != GRP[b])) continue;
                    if (between(b, a) >= between(a, b)) continue;
                    order[i] = b;
                    order[i + 1] = a;
                    pos[b] = i - s;
                    pos[a] = i + 1 - s;
                    improved = true;
                }
            }
            if (!improved) break;
        }
    };

    std::vector<int> initial = order;
    bool small = (n <= 600);
    std::vector<int> chosen = bestOrder;
    double chosenOut = std::numeric_limits<double>::infinity();
    for (int attempt = 0; attempt < (small ? 2 : 1); attempt++) {
        if (attempt > 0) {
            order = initial;
            settlePositions();
        }
        int found = start;
        std::vector<int> foundOrder = initial;
        for (int i = 0, stale = 0; stale < 4 && i < MAX_SWEEPS; i++, stale++) {
            sweep((i + attempt) % 2 == 0, i % 4 >= 2);
            int c = count();
            if (c < found) {
                found = c;
                foundOrder = order;
                stale = 0;
                if (c == 0) break;
            }
        }
        order = foundOrder;
        if (small && found > 0) {
            settlePositions();
            transpose();
            int c = count();
            if (c < found) {
                found = c;
                foundOrder = order;
            }
        }
        double out = small ? outOfOrder() : 0;
        if (attempt == 0 || found < best || (found == best && out < chosenOut)) {
            best = found;
            chosen = foundOrder;
            chosenOut = out;
        }
    }
    order = chosen;
    settlePositions();
    bestOrder = chosen;
    if (hasGRP) best = count();

    if (!small) return {best, declared};
    for (int i = 0; i < real; i++) if (nodes[i].pin != 0) return {best, declared};
    if (chosenOut == 0 || fixed[0] || fixed[maxRank]) return {best, declared};
    for (int r = 0; r <= maxRank; r++) std::reverse(order.begin() + layerStart[r], order.begin() + layerStart[r + 1]);
    settlePositions();
    if (count() > best || outOfOrder() >= chosenOut) order = bestOrder;
    return {best, declared};
}

inline std::vector<double> position(Graph& g, const LayeredOptions& opt) {
    const auto& n = g.n;
    const auto& maxRank = g.maxRank;
    const auto& W = g.W;
    const auto& KIND = g.KIND;
    const auto& EF = g.EF;
    const auto& ET = g.ET;
    const auto& EFD = g.EFD;
    const auto& ETD = g.ETD;
    const auto& succStart = g.succStart;
    auto& succE = g.succE;
    const auto& predStart = g.predStart;
    auto& predE = g.predE;
    const auto& layerStart = g.layerStart;
    const auto& order = g.order;
    const auto& pos = g.pos;
    int segments = static_cast<int>(EF.size());
    std::vector<double> gap(n, 0.0);
    for (int i = 0; i < n; i++) gap[i] = halfRoom(g, opt, i);

    std::vector<int> key(segments, 0);
    for (int e = 0; e < segments; e++) key[e] = pos[ET[e]];
    for (int v = 0; v < n; v++) if (succStart[v + 1] - succStart[v] > 1) sortRange(succE, succStart[v], succStart[v + 1], key);
    for (int e = 0; e < segments; e++) key[e] = pos[EF[e]];
    for (int v = 0; v < n; v++) if (predStart[v + 1] - predStart[v] > 1) sortRange(predE, predStart[v], predStart[v + 1], key);

    std::vector<uint8_t> conflict(segments, 0);
    for (int r = 1; r <= maxRank; r++) {
        int prevLen = layerStart[r] - layerStart[r - 1];
        int s = layerStart[r];
        int end = layerStart[r + 1];
        int k0 = 0;
        int scan = s;
        for (int i = s; i < end; i++) {
            int v = order[i];
            int inner = -1;
            if (isDummy(KIND[v])) {
                int u = EF[predE[predStart[v]]];
                if (isDummy(KIND[u])) inner = u;
            }
            int k1 = inner >= 0 ? pos[inner] : prevLen;
            if (inner >= 0 || i == end - 1) {
                for (int j = scan; j <= i; j++) {
                    int w = order[j];
                    bool wDummy = isDummy(KIND[w]);
                    for (int k = predStart[w]; k < predStart[w + 1]; k++) {
                        int e = predE[k];
                        int up = pos[EF[e]];
                        if ((up < k0 || k1 < up) && !(wDummy && isDummy(KIND[EF[e]]))) conflict[e] = 1;
                    }
                }
                scan = i + 1;
                k0 = k1;
            }
        }
    }

    std::vector<std::vector<double>> results;
    std::vector<int> root(n, 0);
    std::vector<int> align(n, 0);
    std::vector<double> shift(n, 0.0);
    std::vector<int> eFrom(n, 0);
    std::vector<int> eTo(n, 0);
    std::vector<double> eW(n, 0.0);
    std::vector<int> indeg(n, 0);
    std::vector<int> outStart(n + 1, 0);
    std::vector<int> outE(n, 0);
    std::vector<int> fill(n, 0);
    std::vector<int> topo(n, 0);
    std::vector<double> x(n, 0.0);

    for (int variant = 0; variant < 4; variant++) {
        bool up = (variant < 2);
        bool right = ((variant & 1) == 1);
        const auto& start = up ? predStart : succStart;
        const auto& list = up ? predE : succE;
        const auto& other = up ? EF : ET;
        const auto& ownDx = up ? ETD : EFD;
        const auto& otherDx = up ? EFD : ETD;
        for (int i = 0; i < n; i++) {
            root[i] = i;
            align[i] = i;
            shift[i] = 0.0;
            indeg[i] = 0;
            outStart[i + 1] = 0;
            x[i] = 0.0;
        }

        for (int step = 0; step <= maxRank; step++) {
            int r = up ? step : maxRank - step;
            int s = layerStart[r];
            int end = layerStart[r + 1];
            int adjacent = up ? (r - 1) : (r + 1);
            int adjacentLast = (adjacent >= 0 && adjacent <= maxRank) ? (layerStart[adjacent + 1] - layerStart[adjacent] - 1) : 0;
            int prevIdx = -1;
            for (int i = right ? (end - 1) : s; right ? (i >= s) : (i < end); right ? i-- : i++) {
                int v = order[i];
                int from = start[v];
                int d = start[v + 1] - from;
                if (d == 0) continue;
                int lo = from + ((d - 1) >> 1);
                int hi = from + (d >> 1);
                for (int pass = 0; pass < 2; pass++) {
                    if (pass == 1 && lo == hi) break;
                    int e = list[((pass == 0) != right) ? lo : hi];
                    int w = other[e];
                    int idx = right ? (adjacentLast - pos[w]) : pos[w];
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
        for (int r = 0; r <= maxRank; r++) {
            int s = layerStart[r];
            int end = layerStart[r + 1];
            for (int i = s + 1; i < end; i++) {
                int a = right ? order[i] : order[i - 1];
                int b = right ? order[i - 1] : order[i];
                double sep = gap[a] + gap[b];
                eFrom[blockEdges] = root[a];
                eTo[blockEdges] = root[b];
                eW[blockEdges] = right ? (sep - shift[a] + shift[b]) : (sep + shift[a] - shift[b]);
                indeg[root[b]]++;
                outStart[root[a] + 1]++;
                blockEdges++;
            }
        }
        for (int i = 0; i < n; i++) outStart[i + 1] += outStart[i];
        for (int i = 0; i < n; i++) fill[i] = outStart[i];
        for (int e = 0; e < blockEdges; e++) outE[fill[eFrom[e]]++] = e;

        int count = 0;
        for (int i = 0; i < n; i++) if (root[i] == i && indeg[i] == 0) topo[count++] = i;
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
            if (minV != std::numeric_limits<double>::infinity() && minV > x[v]) x[v] = minV;
        }
        std::vector<double> xs(n, 0.0);
        for (int i = 0; i < n; i++) xs[i] = (right ? -x[root[i]] : x[root[i]]) + shift[i];
        results.push_back(std::move(xs));
    }

    int narrowest = 0;
    double bestWidth = std::numeric_limits<double>::infinity();
    double mins[4] = {0.0, 0.0, 0.0, 0.0};
    double maxs[4] = {0.0, 0.0, 0.0, 0.0};
    for (int k = 0; k < 4; k++) {
        double lo = std::numeric_limits<double>::infinity();
        double hi = -std::numeric_limits<double>::infinity();
        const auto& xs = results[k];
        for (int i = 0; i < n; i++) {
            double half = W[i] / 2.0;
            if (xs[i] - half < lo) lo = xs[i] - half;
            if (xs[i] + half > hi) hi = xs[i] + half;
        }
        mins[k] = lo;
        maxs[k] = hi;
        if (hi - lo < bestWidth) {
            bestWidth = hi - lo;
            narrowest = k;
        }
    }
    for (int k = 0; k < 4; k++) {
        double delta = ((k & 1) == 0) ? (mins[narrowest] - mins[k]) : (maxs[narrowest] - maxs[k]);
        if (delta != 0.0) {
            auto& xs = results[k];
            for (int i = 0; i < n; i++) xs[i] += delta;
        }
    }

    std::vector<double> out(n, 0.0);
    for (int i = 0; i < n; i++) {
        double a = results[0][i];
        double b = results[1][i];
        double c = results[2][i];
        double d = results[3][i];
        if (a > b) std::swap(a, b);
        if (c > d) std::swap(c, d);
        if (a > c) std::swap(a, c);
        if (b > d) std::swap(b, d);
        if (b > c) std::swap(b, c);
        out[i] = (b + c) / 2.0;
    }
    return out;
}

inline LayeredResult layoutLayered(std::vector<LNode>& nodes, std::vector<LEdge>& edges, const LayeredOptions& opt = {}) {
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
        std::unordered_set<int64_t> seen;
        for (const auto& e : edges) {
            int a = std::min(e.tail, e.head);
            int b = std::max(e.tail, e.head);
            int64_t key = static_cast<int64_t>(a) * real + b;
            if (e.labelW > 0.0 || seen.count(key)) {
                step = 2;
                break;
            }
            seen.insert(key);
        }
    }

    detail_layered_exact::orient(nodes, edges);
    detail_layered_exact::rank(nodes, edges, step);

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
    std::vector<double> W(n, 0.0);
    std::vector<double> H(n, 0.0);
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
    std::vector<int> EF(segments, 0);
    std::vector<int> ET(segments, 0);
    std::vector<double> EFD(segments, 0.0);
    std::vector<double> ETD(segments, 0.0);
    std::vector<int> firstDummy(m, -1);
    std::vector<uint8_t> heads(maxRank + 1, 0);
    std::vector<int> labelNode(m, -1);
    bool spans = false;
    for (int i = 0; i < real; i++) if (nodes[i].span > 0.0 && nodes[i].kind == LKind::Node) spans = true;
    std::vector<uint8_t> EFK(spans ? segments : 0, 0);
    std::vector<uint8_t> ETK(spans ? segments : 0, 0);
    std::vector<int> firstSeg(m, -1);
    std::vector<int> lastSeg(m, -1);
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
            if (spans && !(e.reversed ? e.headFixed : e.tailFixed)) {
                EFK[ce] = static_cast<uint8_t>(e.reversed ? End::Arrives : End::Leaves);
            }
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
                EFD[ce] = (prev == a) ? aDx : 0.0;
                ce++;
                prev = d;
            }
            EF[ce] = prev;
            ET[ce] = b;
            EFD[ce] = (prev == a) ? aDx : 0.0;
            ETD[ce] = bDx;
            if (spans && !(e.reversed ? e.tailFixed : e.headFixed)) {
                ETK[ce] = static_cast<uint8_t>(e.reversed ? End::Leaves : End::Arrives);
            }
            lastSeg[ei] = ce;
            ce++;
        }
    }

    bool hasGRP = opt.groups.has_value();
    std::vector<int> GRP;
    std::vector<int> groupParent;
    if (hasGRP) {
        const auto& of = opt.groups->of;
        const auto& parent = opt.groups->parent;
        groupParent = parent;
        GRP.assign(n, 0);
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

    std::vector<int> succStart(n + 1, 0);
    std::vector<int> predStart(n + 1, 0);
    for (int e = 0; e < segments; e++) {
        succStart[EF[e] + 1]++;
        predStart[ET[e] + 1]++;
    }
    for (int i = 0; i < n; i++) {
        succStart[i + 1] += succStart[i];
        predStart[i + 1] += predStart[i];
    }
    std::vector<int> succE(segments, 0);
    std::vector<int> predE(segments, 0);
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
    std::vector<int> order(n, 0);
    std::vector<int> pos(n, 0);

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

    auto sortRank = [&](int r, auto keyFn) -> bool {
        int s = layerStart[r];
        int e = layerStart[r + 1];
        if (e - s < 2) return false;
        for (int i = s; i < e; i++) if (std::isnan(keyFn(order[i]))) return false;
        std::vector<int> layer(order.begin() + s, order.begin() + e);
        std::unordered_map<int, int> index;
        for (int i = 0; i < static_cast<int>(layer.size()); i++) index[layer[i]] = i;
        std::stable_sort(layer.begin(), layer.end(), [&](int a, int b) {
            double ka = keyFn(a), kb = keyFn(b);
            if (ka != kb) return ka < kb;
            return index[a] < index[b];
        });
        for (int i = s; i < e; i++) order[i] = layer[i - s];
        return true;
    };
    for (int r = 0; r <= maxRank; r++) sortRank(r, [&](int v) { return KEY[v]; });

    bool pinned = false;
    for (int i = 0; i < real; i++) if (nodes[i].pin != 0) pinned = true;
    if (pinned) {
        for (int r = 0; r <= maxRank; r++) {
            int s = layerStart[r], e = layerStart[r + 1];
            std::vector<int> layer(order.begin() + s, order.begin() + e);
            std::unordered_map<int, int> index;
            for (int i = 0; i < static_cast<int>(layer.size()); i++) index[layer[i]] = i;
            auto pinOf = [&](int v) -> int { return v < real ? nodes[v].pin : 0; };
            std::stable_sort(layer.begin(), layer.end(), [&](int a, int b) {
                int pa = pinOf(a), pb = pinOf(b);
                if (pa != pb) return pa < pb;
                return index[a] < index[b];
            });
            for (int i = s; i < e; i++) order[i] = layer[i - s];
        }
    }
    std::vector<uint8_t> fixed(maxRank + 1, 0);
    auto hintFn = [&](int v) -> double {
        return (v < real && (nodes[v].kind == LKind::StartPort || nodes[v].kind == LKind::EndPort)) ? nodes[v].hint : std::numeric_limits<double>::quiet_NaN();
    };
    for (int r : {0, maxRank}) if (sortRank(r, hintFn)) fixed[r] = 1;
    auto setPositions = [&]() {
        for (int r = 0; r <= maxRank; r++) {
            int s = layerStart[r];
            for (int i = s, e = layerStart[r + 1]; i < e; i++) pos[order[i]] = i - s;
        }
    };
    setPositions();

    Graph g{n, real, maxRank, W, KIND, RANK, EF, ET, EFD, ETD, succStart, succE, predStart, predE, layerStart, order, pos, offsets, fixed, hasGRP, GRP, groupParent};

    int crossings = 0;
    int declared = 0;
    if (segments > 0 && maxRank > 0) {
        auto res = reduceCrossings(g, nodes);
        crossings = res.first;
        declared = res.second;
        setPositions();
    }
    if (opt.orderOnly) {
        auto at = position(g, opt);
        for (int i = 0; i < real; i++) nodes[i].x = at[i];
        for (int ei = 0; ei < m; ei++) {
            auto& e = edges[ei];
            int a = e.reversed ? e.head : e.tail;
            int b = e.reversed ? e.tail : e.head;
            std::vector<double> pts = {static_cast<double>(RANK[a]), at[a]};
            if (!direct[ei] && firstDummy[ei] >= 0) {
                for (int d = firstDummy[ei], end = d + span[ei] - 1; d < end; d++) {
                    pts.push_back(static_cast<double>(RANK[d]));
                    pts.push_back(at[d]);
                }
            }
            pts.push_back(static_cast<double>(RANK[b]));
            pts.push_back(at[b]);
            e.points = std::move(pts);
        }
        return {0.0, 0.0, crossings, declared};
    }

    if (spans) spread(g, nodes, EFK, ETK);

    auto xs = position(g, opt);

    auto empty = emptyRanks(g);
    if (spans) {
        auto free = freeEnds(g, EFK, ETK);
        alignLabels(g, nodes, xs, free, opt, empty);
        settle(g, nodes, xs, EFK, ETK, free, empty);
        snap(g, xs, empty, opt);
        for (int ei = 0; ei < m; ei++) {
            if (direct[ei]) continue;
            auto& e = edges[ei];
            double aDx = EFD[firstSeg[ei]];
            double bDx = ETD[lastSeg[ei]];
            e.tailDx = e.reversed ? bDx : aDx;
            e.headDx = e.reversed ? aDx : bDx;
        }
    }

    std::vector<uint8_t> onLine(n, 0);
    if (!empty.empty() && !opt.tracks) straighten(g, xs, empty, firstDummy, span, opt.apart ? 0.0 : opt.edgeSep, onLine);
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
                bool bare = !empty.empty() && empty[RANK[d]] == 1;
                if (!opt.tracks && bare != (onLine[d] == 1)) twin = true;
            }
            if (twin) continue;
            double x = xs[EF[firstSeg[ei]]] + EFD[firstSeg[ei]];
            int r = RANK[EF[firstSeg[ei]]];
            for (int d = d0, end = d0 < 0 ? d0 : d0 + span[ei] - 1; d < end; d++) {
                if (!empty.empty() && empty[RANK[d]]) continue;
                pieces.from.push_back(x);
                pieces.to.push_back(xs[d]);
                pieces.rank.push_back(r);
                pieces.lower.push_back(d);
                x = xs[d];
                r = RANK[d];
            }
            pieces.from.push_back(x);
            pieces.to.push_back(xs[ET[lastSeg[ei]]] + ETD[lastSeg[ei]]);
            pieces.rank.push_back(r);
            pieces.lower.push_back(-1);
        }
        pieceStart[m] = static_cast<int>(pieces.from.size());
    }

    auto portOnly = [&](int r, LKind kind) -> bool {
        bool found = false;
        for (int i = layerStart[r]; i < layerStart[r + 1]; i++) {
            uint8_t k = KIND[order[i]];
            if (k == static_cast<uint8_t>(kind)) found = true;
            else if (k != static_cast<uint8_t>(LKind::Dummy)) return false;
        }
        return found;
    };
    int startRanks = portOnly(0, LKind::StartPort) ? step : 0;
    int endRanks = portOnly(maxRank, LKind::EndPort) ? step : 0;
    std::vector<double> run(maxRank + 1, 0.0);
    for (int e = 0; e < segments; e++) {
        double across = std::abs(xs[EF[e]] + EFD[e] - xs[ET[e]] - ETD[e]);
        if (across > run[RANK[EF[e]]]) run[RANK[EF[e]]] = across;
    }
    std::vector<double> bandTop(maxRank + 1, 0.0);
    std::vector<double> bandBottom(maxRank + 1, 0.0);
    auto usual = [&](int r) -> double { return (r < startRanks || r >= maxRank - endRanks ? opt.portSep : opt.rankSep) / step; };
    std::vector<double> gapUnder(maxRank + 1, 0.0);
    for (int r = 0; r < maxRank; r++) {
        gapUnder[r] = hasPieces ? usual(r) : std::max(usual(r), std::min((MAX_GAP * opt.rankSep) / step, run[r] * MIN_SLOPE));
    }
    std::vector<uint8_t> between(maxRank + 1, 1);
    for (int i = 0; i < real; i++) between[RANK[i]] = 0;
    for (int r = 1; r < maxRank; r++) {
        if (opt.tracks || !between[r] || (heads[r] != 1 && heads[r] != 2)) continue;
        double shift = std::min(opt.headRoom / 2.0, gapUnder[heads[r] == 1 ? (r - 1) : r] / 2.0);
        gapUnder[r - 1] += (heads[r] == 1) ? -shift : shift;
        gapUnder[r] += (heads[r] == 1) ? shift : -shift;
    }
    auto inset = [&](int d) -> double {
        return (KIND[d] == static_cast<uint8_t>(LKind::Dummy) && between[RANK[d]] && !opt.tracks) ? ((bandBottom[RANK[d]] - bandTop[RANK[d]]) / 4.0) : 0.0;
    };
    double margin = opt.tracks ? TRACK_MARGIN : LEVEL_MARGIN;
    double leastPitch = opt.tracks ? TRACK_PITCH : LEVEL_PITCH;
    double mostPitch = opt.tracks ? TRACK_SPREAD : LEVEL_PITCH;
    std::vector<int> below(maxRank + 1, 0);
    if (hasPieces) {
        std::vector<double> room(maxRank + 1, 0.0);
        for (int r = 0; r < maxRank; r++) {
            if (!empty.empty() && empty[r]) continue;
            int next = r + 1;
            room[r] = gapUnder[r];
            while (next < maxRank && !empty.empty() && empty[next]) room[r] += gapUnder[next++];
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
        for (int i = layerStart[r]; i < layerStart[r + 1]; i++) if (H[order[i]] > h) h = H[order[i]];
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
        double half = W[i] / 2.0;
        if (xs[i] - half < minX) minX = xs[i] - half;
        if (xs[i] + half > maxX) maxX = xs[i] + half;
    }
    std::vector<uint8_t> arrives(maxRank + 1, 0);
    std::vector<uint8_t> leaves(maxRank + 1, 0);
    for (int i = 0; i < real; i++) {
        if (KIND[i] != static_cast<uint8_t>(LKind::Node) || !spans) continue;
        if (predStart[i + 1] > predStart[i]) arrives[RANK[i]] = 1;
        if (succStart[i + 1] > succStart[i]) leaves[RANK[i]] = 1;
    }
    for (int i = 0; i < real; i++) {
        auto& node = nodes[i];
        int r = node.rank;
        node.x = xs[i] - minX;
        if (KIND[i] == static_cast<uint8_t>(LKind::Node) && leaves[r] && !arrives[r]) node.y = bandBottom[r] - node.h / 2.0;
        else if (KIND[i] == static_cast<uint8_t>(LKind::Node) && arrives[r] && !leaves[r]) node.y = bandTop[r] + node.h / 2.0;
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
                pts.push_back(bandTop[RANK[d]] + inset(d));
                if (bandBottom[RANK[d]] > bandTop[RANK[d]]) {
                    pts.push_back(xs[d] - minX);
                    pts.push_back(bandBottom[RANK[d]] - inset(d));
                }
            }
        } else if (d0 >= 0) {
            for (int d = d0, end = d0 + span[ei] - 1; d < end; d++) {
                if (onLine[d]) continue;
                double x = xs[d] - minX;
                double top = bandTop[RANK[d]];
                double bottom = bandBottom[RANK[d]];
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
            e.labelY = (bandTop[RANK[ln]] + bandBottom[RANK[ln]]) / 2.0;
        } else if (e.labelW > 0.0) {
            e.labelX = (e.points[0] + e.points[e.points.size() - 2]) / 2.0;
            e.labelY = (e.points[1] + e.points[e.points.size() - 1]) / 2.0;
        }
    }

    return {maxX - minX, height, crossings, declared};
}

} // namespace detail_layered_exact

using detail_layered_exact::orient;
using detail_layered_exact::rank;
using detail_layered_exact::layoutLayered;


// ============================================================================
// src/layout/compound.ts (exact 1:1 port)
// ============================================================================

inline std::string direction(const std::string& dir) {
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
    double seq = 0.0;
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
    Left = 3,
};

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
    std::vector<std::shared_ptr<PortEdge>> ports;
    std::unordered_map<int, std::shared_ptr<PortEdge>> portOf;
    std::unordered_map<int, std::vector<double>> routes;
    double x = 0.0;
    double y = 0.0;
};

constexpr int MAX_PORTS = 40000;
constexpr int MAX_ORDERED_PORTS = 400;
constexpr int MAX_ORDERED_EDGES = 500;
constexpr double FLAT_SLACK = 1.5;
constexpr double FLAT_SPARE = 4.0;

struct Snapshot {
    std::vector<double> nodes;
    std::vector<std::vector<double>> routes;
    std::vector<double> labels;
    std::vector<double> levels;
    CompoundResult total;
};

inline Snapshot snapshot(
    const std::vector<CNode>& nodes,
    const std::vector<CEdge>& edges,
    const std::vector<std::shared_ptr<Level>>& levels,
    CompoundResult total
) {
    Snapshot s;
    s.nodes.reserve(nodes.size() * 4);
    for (const auto& c : nodes) {
        s.nodes.push_back(c.x);
        s.nodes.push_back(c.y);
        s.nodes.push_back(c.w);
        s.nodes.push_back(c.h);
    }
    s.labels.reserve(edges.size() * 2);
    s.routes.reserve(edges.size());
    for (const auto& e : edges) {
        s.labels.push_back(e.labelX);
        s.labels.push_back(e.labelY);
        s.routes.push_back(e.route);
    }
    for (const auto& level : levels) {
        s.levels.push_back(level->x);
        s.levels.push_back(level->y);
        for (const auto& port : level->ports) {
            s.levels.push_back(port->px);
            s.levels.push_back(port->py);
        }
    }
    s.total = total;
    return s;
}

inline CompoundResult restore(
    const Snapshot& s,
    std::vector<CNode>& nodes,
    std::vector<CEdge>& edges,
    std::vector<std::shared_ptr<Level>>& levels
) {
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
        level->x = s.levels[k++];
        level->y = s.levels[k++];
        for (auto& port : level->ports) {
            port->px = s.levels[k++];
            port->py = s.levels[k++];
        }
    }
    return s.total;
}

constexpr double FAN = 24.0;
constexpr double IN_LINE = 0.5;
constexpr int CURVE_PIECES = 3;
constexpr int CORNER_PIECES = 2;
constexpr int STEP_PIECES = 3;
constexpr double HANDLE_C = 0.62;
constexpr int MAX_INSERT = 128;

enum class RouteStyle : int {
    Curved = 0,
    Stepped = 1,
    Linear = 2,
};

inline RouteStyle styleOf(const LayeredOptions& opt) {
    return opt.tracks ? RouteStyle::Stepped : opt.straight ? RouteStyle::Linear : RouteStyle::Curved;
}

inline int routeCrossings(const std::vector<CNode>& nodes, const std::vector<CEdge>& edges, RouteStyle style = RouteStyle::Curved) {
    int size = 0;
    for (const auto& e : edges) {
        if (e.src != e.dst && e.route.size() > 3) {
            size += (static_cast<int>(e.route.size()) / 3 - 1) * STEP_PIECES;
        }
    }
    std::vector<double> ax(size, 0.0);
    std::vector<double> ay(size, 0.0);
    std::vector<double> bx(size, 0.0);
    std::vector<double> by(size, 0.0);
    std::vector<double> low(size, 0.0);
    std::vector<double> high(size, 0.0);
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
        bool on = (x0 == toX && y0 == toY && ((y1 == y0 && fromY == y0) || (x1 == x0 && fromX == x0)));
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

    for (int ei = 0; ei < static_cast<int>(edges.size()); ei++) {
        const auto& e = edges[ei];
        if (e.src == e.dst) continue;
        const auto& r = e.route;
        int rlen = static_cast<int>(r.size());
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
                (std::abs(r[k] - r[k - 3]) < 0.01) != (std::abs(r[k + 3] - r[k]) < 0.01);
        };
        if (rlen < 3) continue;
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
                    add(ei, part, x0, y0, x1, y1);
                    continue;
                }
                double before = turns(k - 3) ? (length(k) / 2.0) : length(k);
                double after = turns(k + 3) ? (length(k + 3) / 2.0) : length(k + 3);
                double ux = (x1 - r[k - 3]) / length(k);
                double uy = (y1 - r[k - 2]) / length(k);
                double vx = (r[k + 3] - x1) / length(k + 3);
                double vy = (r[k + 4] - y1) / length(k + 3);
                double sx = x1 - ux * before;
                double sy = y1 - uy * before;
                add(ei, part, x0, y0, sx, sy);
                double c1x = x1 - ux * before * (1.0 - HANDLE_C);
                double c1y = y1 - uy * before * (1.0 - HANDLE_C);
                double c2x = x1 + vx * after * (1.0 - HANDLE_C);
                double c2y = y1 + vy * after * (1.0 - HANDLE_C);
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
                    add(ei, turn, px, py, x, y);
                    px = x;
                    py = y;
                }
                continue;
            }
            if (style == RouteStyle::Stepped) {
                bool down = (r[k + 2] == 0.0);
                double mx = down ? x0 : (x0 + x1) / 2.0;
                double my = down ? (y0 + y1) / 2.0 : y0;
                add(ei, part, x0, y0, mx, my);
                add(ei, part, mx, my, down ? x1 : mx, down ? my : y1);
                add(ei, part, down ? x1 : mx, down ? my : y1, x1, y1);
                continue;
            }
            bool down = (r[k + 2] == 0.0);
            double c = down ? easeCurve(x1 - x0, y1 - y0) : easeCurve(y1 - y0, x1 - x0);
            double px = x0;
            double py = y0;
            for (int i = 1; i <= CURVE_PIECES; i++) {
                double t = static_cast<double>(i) / CURVE_PIECES;
                double u = 1.0 - t;
                double across = t * t * (3.0 - 2.0 * t);
                double along = 3.0 * c * t * u * u + 3.0 * (1.0 - c) * t * t * u + t * t * t;
                double x = x0 + (x1 - x0) * (down ? across : along);
                double y = y0 + (y1 - y0) * (down ? along : across);
                add(ei, part, px, py, x, y);
                px = x;
                py = y;
            }
        }
    }

    int bands = bottom > top ? std::max(1, static_cast<int>(std::llround(std::sqrt(static_cast<double>(count))))) : 1;
    double scale = bottom > top ? (static_cast<double>(bands) / (bottom - top)) : 0.0;
    auto bandOf = [&](double y) -> int {
        return std::max(0, std::min(bands - 1, static_cast<int>(std::floor((y - top) * scale))));
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
        for (int b = first[p], last = bandOf(high[p]); b <= last; b++) {
            inBand[fill[b]++] = p;
        }
    }

    std::vector<double> runs(edges.size() * 8, 0.0);
    auto inLine = [](const std::vector<double>& r, int a, int b, int c) -> bool {
        return (std::abs(r[a] - r[b]) < IN_LINE && std::abs(r[a] - r[c]) < IN_LINE) ||
               (std::abs(r[a + 1] - r[b + 1]) < IN_LINE && std::abs(r[a + 1] - r[c + 1]) < IN_LINE);
    };
    for (int ei = 0; ei < static_cast<int>(edges.size()); ei++) {
        const auto& r = edges[ei].route;
        int rlen = static_cast<int>(r.size());
        if (rlen < 6) continue;
        int k = 3;
        while (k + 3 < rlen && inLine(r, 0, k, k + 3)) k += 3;
        int last = rlen - 3;
        int j = last - 3;
        while (j >= 3 && inLine(r, last, j, j - 3)) j -= 3;
        int o = ei * 8;
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
            bool here = (first[p] == band);
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

inline CNode cnode(double w, double h, int parent = -1) {
    CNode c;
    c.w = w;
    c.h = h;
    c.parent = parent;
    return c;
}

inline Side startSide(const std::string& dir) {
    return dir == "TB" ? Side::Top : dir == "BT" ? Side::Bottom : dir == "LR" ? Side::Left : Side::Right;
}

inline Side opposite(Side side) {
    return static_cast<Side>((static_cast<int>(side) + 2) & 3);
}

inline int endSide(const std::vector<double>& route, int at, const CNode& c) {
    double dx = route[at] - c.x;
    double dy = route[at + 1] - c.y;
    if (std::abs(std::abs(dy) - c.h / 2.0) < 0.5) return dy < 0.0 ? 0 : 2;
    if (std::abs(std::abs(dx) - c.w / 2.0) < 0.5) return dx < 0.0 ? 3 : 1;
    return -1;
}

inline LNode lnode(double w, double h, LKind kind = LKind::Node) {
    LNode n;
    n.w = w;
    n.h = h;
    n.kind = kind;
    return n;
}

inline LEdge ledge(int tail, int head, int minlen = 1, double labelW = 0.0, double labelH = 0.0) {
    LEdge e;
    e.tail = tail;
    e.head = head;
    e.minlen = minlen;
    e.labelW = labelW;
    e.labelH = labelH;
    return e;
}

inline CompoundResult compoundLayout(
    std::vector<CNode>& nodes,
    std::vector<CEdge>& edges,
    const std::string& rootDir,
    const LayeredOptions& opt
) {
    int n = static_cast<int>(nodes.size());
    std::unordered_map<int, std::shared_ptr<Level>> levelOf;
    std::vector<std::shared_ptr<Level>> order;
    auto makeLevel = [&](int id, const std::string& dir) -> std::shared_ptr<Level> {
        auto level = std::make_shared<Level>();
        level->id = id;
        level->dir = dir;
        levelOf[id] = level;
        return level;
    };
    auto root = makeLevel(-1, rootDir);

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
    root->items = std::move(rootItems);
    order.push_back(root);
    std::vector<int> depth(n, 0);
    std::vector<uint8_t> placed(n, 0);
    auto descend = [&](size_t from) {
        for (size_t q = from; q < order.size(); q++) {
            auto level = order[q];
            for (int item : level->items) {
                placed[item] = 1;
                depth[item] = level->id < 0 ? 0 : depth[level->id] + 1;
                if (nodes[item].isGroup) {
                    auto child = makeLevel(item, !nodes[item].dir.empty() ? nodes[item].dir : level->dir);
                    child->items = children[item];
                    order.push_back(child);
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
        root->items.push_back(i);
        placed[i] = 1;
        depth[i] = 0;
        if (nodes[i].isGroup) {
            size_t from = order.size();
            auto child = makeLevel(i, !nodes[i].dir.empty() ? nodes[i].dir : rootDir);
            child->items = children[i];
            order.push_back(child);
            descend(from);
        }
    }

    for (int q = static_cast<int>(order.size()) - 1; q > 0; q--) {
        auto& level = order[q];
        double seq = std::numeric_limits<double>::infinity();
        for (int item : level->items) if (nodes[item].seq < seq) seq = nodes[item].seq;
        if (std::isfinite(seq)) nodes[level->id].seq = seq;
    }
    for (auto& level : order) {
        std::sort(level->items.begin(), level->items.end(), [&](int a, int b) {
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
        levelOf.at(nodes[a].parent)->edges.push_back({ei, a, b});
        for (int pass = 0; pass < 2; pass++) {
            const auto& path = pass == 0 ? aPath : bPath;
            bool out = (pass == 0);
            for (int inner : path) {
                auto& level = levelOf.at(nodes[inner].parent);
                auto port = std::make_shared<PortEdge>();
                port->e = ei;
                port->inner = inner;
                port->out = out;
                port->side = Side::Top;
                port->hint = std::numeric_limits<double>::quiet_NaN();
                level->ports.push_back(port);
                level->portOf[ei * 2 + (out ? 0 : 1)] = port;
            }
        }
    }

    std::vector<int> local(n, 0);
    auto endpointBelow = [](int item, const CEdge& e, bool out) -> bool {
        return (out ? e.src : e.dst) != item;
    };

    bool ordered = ports > 1 && ports <= MAX_ORDERED_PORTS && static_cast<int>(edges.size()) <= MAX_ORDERED_EDGES;

    std::vector<double> key(n, std::numeric_limits<double>::quiet_NaN());
    bool keyed = false;
    int flatCrossings = 0;
    std::function<double(const Level&, const PortEdge&)> farHint;
    bool flat = order.size() > 1 && ordered;
    for (const auto& level : order) if (level->dir != rootDir) flat = false;
    if (flat) {
        bool vertical = (rootDir == "TB" || rootDir == "BT");
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
        auto flatLedges = std::make_shared<std::vector<LEdge>>();
        auto ledgeOf = std::make_shared<std::vector<int>>(edges.size(), -1);
        for (int ei = 0; ei < static_cast<int>(edges.size()); ei++) {
            const auto& e = edges[ei];
            if (e.src == e.dst || leaf[e.src] < 0 || leaf[e.dst] < 0) continue;
            (*ledgeOf)[ei] = static_cast<int>(flatLedges->size());
            flatLedges->push_back(ledge(leaf[e.src], leaf[e.dst], e.minlen, vertical ? e.labelW : e.labelH, vertical ? e.labelH : e.labelW));
        }
        LayeredOptions fopt = opt;
        fopt.groups = LayeredGroups{of, parent};
        fopt.orderOnly = true;
        auto flatLayout = detail_layered_exact::layoutLayered(lnodes, *flatLedges, fopt);
        flatCrossings = std::min(flatLayout.crossings, flatLayout.declared);
        std::vector<double> sum(n, 0.0);
        std::vector<int> count(n, 0);
        auto last = std::make_shared<std::vector<int>>(n, -1);
        for (int i = 0; i < n; i++) {
            if (leaf[i] < 0) continue;
            key[i] = lnodes[leaf[i]].x;
            for (int p = nodes[i].parent; p >= 0; p = nodes[p].parent) {
                sum[p] += key[i];
                count[p]++;
                (*last)[p] = std::max((*last)[p], lnodes[leaf[i]].rank);
            }
        }
        for (int i = 0; i < n; i++) if (count[i] > 0) key[i] = sum[i] / count[i];
        farHint = [&, flatLedges, ledgeOf, last](const Level& level, const PortEdge& port) -> double {
            int le = (*ledgeOf)[port.e];
            if (le < 0 || level.id < 0 || !port.out || port.side != opposite(startSide(level.dir))) {
                return std::numeric_limits<double>::quiet_NaN();
            }
            const auto& points = (*flatLedges)[le].points;
            double inside = key[edges[port.e].src];
            double outside = 0.0;
            int lastRank = (*last)[level.id];
            for (size_t k = 0; k + 1 < points.size(); k += 2) {
                if (points[k] == lastRank) inside = points[k + 1];
                if (points[k] == lastRank + 1) outside = points[k + 1];
            }
            return inside + outside * 1e-6;
        };
    }

    struct BuiltLevel {
        std::vector<LNode> lnodes;
        std::vector<LEdge> ledges;
        std::vector<int> dummies;
    };
    auto build = [&](const Level& level, bool sized) -> BuiltLevel {
        bool vertical = (level.dir == "TB" || level.dir == "BT");
        Side start = startSide(level.dir);
        BuiltLevel out;
        for (size_t i = 0; i < level.items.size(); i++) {
            int item = level.items[i];
            local[item] = static_cast<int>(i);
            const auto& node = nodes[item];
            LNode ln = sized ? lnode(vertical ? node.w : node.h, vertical ? node.h : node.w) : lnode(0.0, 0.0);
            ln.span = node.isGroup ? std::min(node.span, ln.w - 2.0 * node.padX) : node.span;
            ln.key = keyed ? key[item] : std::numeric_limits<double>::quiet_NaN();
            out.lnodes.push_back(ln);
        }
        auto held = [&](int item, int ei, bool isOut) -> bool {
            return nodes[item].isGroup && endpointBelow(item, edges[ei], isOut);
        };
        auto offset = [&](int item, int ei, bool isOut) -> double {
            if (!sized || !nodes[item].isGroup || !endpointBelow(item, edges[ei], isOut)) return 0.0;
            const auto& port = levelOf.at(item)->portOf.at(ei * 2 + (isOut ? 0 : 1));
            return vertical ? (port->px - nodes[item].w / 2.0) : (port->py - nodes[item].h / 2.0);
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
            out.ledges.push_back(edge);
        }
        out.dummies.assign(level.ports.size(), -1);
        for (size_t i = 0; i < level.ports.size(); i++) {
            const auto& port = *level.ports[i];
            if (port.side == start || port.side == opposite(start)) {
                int d = static_cast<int>(out.lnodes.size());
                LNode ln = lnode(0.0, 0.0, port.side == start ? LKind::StartPort : LKind::EndPort);
                ln.key = keyed ? key[port.out ? edges[port.e].dst : edges[port.e].src] : std::numeric_limits<double>::quiet_NaN();
                ln.hint = (std::isnan(port.hint) && keyed && farHint) ? farHint(level, port) : port.hint;
                out.lnodes.push_back(ln);
                out.dummies[i] = d;
                LEdge edge = port.out ? ledge(local[port.inner], d) : ledge(d, local[port.inner]);
                if (port.out) {
                    edge.tailDx = offset(port.inner, port.e, true);
                    edge.tailFixed = held(port.inner, port.e, true);
                } else {
                    edge.headDx = offset(port.inner, port.e, false);
                    edge.headFixed = held(port.inner, port.e, false);
                }
                out.ledges.push_back(edge);
            } else {
                out.lnodes[local[port.inner]].pin = (port.side == Side::Top || port.side == Side::Left) ? -1 : 1;
            }
        }
        return out;
    };

    struct LaidEntry {
        std::vector<double> given;
        std::vector<LNode> lnodes;
        std::vector<LEdge> ledges;
        LayeredResult size;
    };
    std::unordered_map<int, std::vector<LaidEntry>> laid;
    auto sameVal = [](double a, double b) -> bool {
        if (std::isnan(a) && std::isnan(b)) return true;
        return a == b;
    };
    auto layOut = [&](const Level& level, std::vector<LNode>& lnodes, std::vector<LEdge>& ledges) -> LaidEntry {
        if (!ordered) {
            auto sz = detail_layered_exact::layoutLayered(lnodes, ledges, opt);
            return {{}, lnodes, ledges, sz};
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
            bool same = (p.given.size() == given.size());
            for (size_t i = 0; same && i < given.size(); i++) same = sameVal(p.given[i], given[i]);
            if (same) return p;
        }
        auto sz = detail_layered_exact::layoutLayered(lnodes, ledges, opt);
        LaidEntry done{std::move(given), lnodes, ledges, sz};
        past.push_back(done);
        return done;
    };

    CompoundResult total{0.0, 0.0};
    auto place = [&]() {
        for (auto& level : order) {
            for (auto& port : level->ports) {
                if (nodes[port->inner].isGroup && endpointBelow(port->inner, edges[port->e], port->out)) {
                    levelOf.at(port->inner)->portOf.at(port->e * 2 + (port->out ? 0 : 1))->side = port->side;
                }
            }
            if (level->edges.empty()) continue;
            bool needed = false;
            for (const auto& le : level->edges) {
                const auto& e = edges[le.e];
                if ((nodes[le.a].isGroup && e.src != le.a) || (nodes[le.b].isGroup && e.dst != le.b)) needed = true;
            }
            if (!needed) continue;
            auto built = build(*level, false);
            detail_layered_exact::orient(built.lnodes, built.ledges);
            detail_layered_exact::rank(built.lnodes, built.ledges, 1);
            Side start = startSide(level->dir);
            for (size_t i = 0; i < level->edges.size(); i++) {
                const auto& le = level->edges[i];
                const auto& e = edges[le.e];
                bool aAbove = !built.ledges[i].reversed;
                if (nodes[le.a].isGroup && e.src != le.a) {
                    levelOf.at(le.a)->portOf.at(le.e * 2)->side = aAbove ? opposite(start) : start;
                }
                if (nodes[le.b].isGroup && e.dst != le.b) {
                    levelOf.at(le.b)->portOf.at(le.e * 2 + 1)->side = aAbove ? start : opposite(start);
                }
            }
        }

        for (int q = static_cast<int>(order.size()) - 1; q >= 0; q--) {
            auto& level = order[q];
            CNode* group = level->id >= 0 ? &nodes[level->id] : nullptr;
            bool vertical = (level->dir == "TB" || level->dir == "BT");
            bool flip = (level->dir == "BT" || level->dir == "RL");
            auto built = build(*level, true);
            auto laidRes = layOut(*level, built.lnodes, built.ledges);
            const auto& lnodes = laidRes.lnodes;
            const auto& ledges = laidRes.ledges;
            const auto& size = laidRes.size;
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
                out.reserve(points.size() / 2 * 3);
                for (size_t i = 0; i + 1 < points.size(); i += 2) {
                    out.push_back(tx(points[i], points[i + 1]));
                    out.push_back(ty(points[i], points[i + 1]));
                    out.push_back(axis);
                }
                return out;
            };

            for (size_t i = 0; i < level->items.size(); i++) {
                int item = level->items[i];
                nodes[item].x = tx(lnodes[i].x, lnodes[i].y);
                nodes[item].y = ty(lnodes[i].x, lnodes[i].y);
            }
            for (size_t i = 0; i < level->edges.size(); i++) {
                const auto& le = level->edges[i];
                const auto& edge = ledges[i];
                level->routes[le.e] = convert(edge.points);
                auto& e = edges[le.e];
                if (e.labelW > 0.0) {
                    e.labelX = tx(edge.labelX, edge.labelY);
                    e.labelY = ty(edge.labelX, edge.labelY);
                }
            }
            size_t k = level->edges.size();
            for (size_t i = 0; i < level->ports.size(); i++) {
                auto& port = *level->ports[i];
                const auto& item = nodes[port.inner];
                if (dummies[i] >= 0) {
                    auto route = convert(ledges[k++].points);
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
                    const auto& inner = levelOf.at(port.inner)->portOf.at(port.e * 2 + (port.out ? 0 : 1));
                    ix += inner->px - item.w / 2.0;
                    iy += inner->py - item.h / 2.0;
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

        for (auto& level : order) {
            double ox = level->x;
            double oy = level->y;
            for (int item : level->items) {
                auto& node = nodes[item];
                node.x += ox;
                node.y += oy;
                if (node.isGroup) {
                    auto& child = levelOf.at(item);
                    child->x = node.x - node.w / 2.0;
                    child->y = node.y - node.h / 2.0;
                }
            }
        }

        auto append = [](std::vector<double>& route, const std::vector<double>& part, double ox, double oy) {
            for (size_t i = 0; i + 2 < part.size(); i += 3) {
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
            for (const auto& le : level->edges) {
                auto& e = edges[le.e];
                std::vector<double> route;
                std::vector<std::shared_ptr<Level>> outward;
                for (int item = e.src; nodes[item].parent != level->id; item = nodes[item].parent) {
                    outward.push_back(levelOf.at(nodes[item].parent));
                }
                for (const auto& lv : outward) {
                    append(route, lv->portOf.at(le.e * 2)->route, lv->x, lv->y);
                }
                append(route, level->routes.at(le.e), level->x, level->y);
                std::vector<std::shared_ptr<Level>> inward;
                for (int item = e.dst; nodes[item].parent != level->id; item = nodes[item].parent) {
                    inward.push_back(levelOf.at(nodes[item].parent));
                }
                for (int i = static_cast<int>(inward.size()) - 1; i >= 0; i--) {
                    append(route, inward[i]->portOf.at(le.e * 2 + 1)->route, inward[i]->x, inward[i]->y);
                }
                e.route = std::move(route);
                if (e.labelW > 0.0) {
                    e.labelX += level->x;
                    e.labelY += level->y;
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
            auto first = snapshot(nodes, edges, order, total);
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
            if (level->id < 0) continue;
            const auto& box = nodes[level->id];
            bool vertical = (level->dir == "TB" || level->dir == "BT");
            Side end = opposite(startSide(level->dir));
            std::vector<std::pair<double, double>> seen;
            for (auto& port : level->ports) {
                if (port->side != end) continue;
                const auto& route = edges[port->e].route;
                double bx = level->x + port->px;
                double by = level->y + port->py;
                int at = -1;
                for (int k = 0; k < static_cast<int>(route.size()) && (at < 0 || port->out); k += 3) {
                    if (std::abs(route[k] - bx) < 0.01 && std::abs(route[k + 1] - by) < 0.01) at = k;
                }
                if (at < 0) continue;
                int a = vertical ? 0 : 1;
                int next = port->out ? at + 3 : at - 3;
                while (next >= 0 && next < static_cast<int>(route.size()) && std::abs(route[next + a] - route[at + a]) < IN_LINE) {
                    next += port->out ? 3 : -3;
                }
                if (next < 0 || next >= static_cast<int>(route.size())) continue;
                port->hint = route[next + a] - (vertical ? (box.x - box.w / 2.0) : (box.y - box.h / 2.0));
                seen.push_back({vertical ? port->px : port->py, port->hint});
            }
            std::stable_sort(seen.begin(), seen.end(), [](const auto& p, const auto& q) {
                return p.first < q.first;
            });
            for (size_t i = 1; i < seen.size(); i++) {
                if (seen[i - 1].second > seen[i].second) again = true;
            }
        }
        if (again) {
            int before = crossings >= 0 ? crossings : routeCrossings(nodes, edges, style);
            if (before > 0) {
                auto first = snapshot(nodes, edges, order, total);
                place();
                if (routeCrossings(nodes, edges, style) >= before) {
                    total = restore(first, nodes, edges, order);
                }
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
// src/diagrams/common/fit-width.ts (runsAcross, turnToFit)
// ============================================================================

inline bool runsAcross(const std::string& dir) {
    return dir == "LR" || dir == "RL";
}

template <typename DrawFn>
inline Rendered turnToFit(const RenderOptions& options, DrawFn&& draw) {
    constexpr double BREAKPOINT = 640.0;
    Rendered across = draw(false);
    double available = options.maxWidth;
    if (!(available > 0.0) || across.width <= available) {
        return across;
    }
    if (available >= BREAKPOINT) return across;
    Rendered down = draw(true);
    return down.width < across.width ? down : across;
}

// ============================================================================
// src/diagrams/state/types.ts
// ============================================================================

struct StateNote {
    std::string position;
    std::string text;
};

struct Stmt;
using StmtPtr = std::shared_ptr<Stmt>;

struct StateStmt {
    std::string id;
    std::string type; // empty = undefined
    bool hasDescription = false;
    bool isDescArray = false;
    std::string descStr;
    std::vector<std::string> descArr;
    std::vector<std::string> descriptions;
    bool hasDoc = false;
    std::vector<StmtPtr> doc;
    bool hasNote = false;
    StateNote note;
    int start = -1; // -1 = undefined, 1 = true, 0 = false
    bool hasClasses = false;
    std::vector<std::string> classes;
    std::vector<std::string> styles;
    std::vector<std::string> textStyles;
};
using StateStmtPtr = std::shared_ptr<StateStmt>;

struct Stmt {
    // kind: "string", "state", "relation", "classDef", "style", "applyClass", "dir", "click", "root"
    std::string kind;
    std::string strVal;
    StateStmtPtr state;
    StateStmtPtr state1;
    StateStmtPtr state2;
    std::string description;
    bool hasDescription = false;
    std::string id;
    StateStmtPtr clickState;
    std::string classes;
    std::string styleClass;
    std::string value;
    std::string url;
    std::string tooltip;
};

inline StmtPtr makeStringStmt(const std::string& s) {
    auto p = std::make_shared<Stmt>();
    p->kind = "string";
    p->strVal = s;
    return p;
}

inline StmtPtr makeStateStmtWrap(StateStmtPtr st) {
    auto p = std::make_shared<Stmt>();
    p->kind = "state";
    p->state = std::move(st);
    return p;
}

struct StateRelation {
    std::string id1;
    std::string id2;
    std::string relationTitle;
};

struct StateClassDef {
    std::string id;
    std::vector<std::string> styles;
    std::vector<std::string> textStyles;
};

struct StateLink {
    std::string url;
    std::string tooltip;
};

// ============================================================================
// src/diagrams/state/graph.ts
// ============================================================================

struct StateNode {
    std::string id;
    std::string shape; // empty = undefined
    std::string label;
    bool hasLabel = false;
    std::vector<std::string> description;
    std::string cssClasses;
    std::vector<std::string> cssCompiledStyles;
    std::vector<std::string> cssStyles;
    std::string type; // empty = undefined
    bool isGroup = false;
    std::string parentId;
    bool hasParentId = false;
    std::string dir; // empty = undefined
    std::string position;
    std::string noteFor;
    bool hasNoteFor = false;
    int colorIndex = -1;
    bool hasColorIndex = false;
};

struct StateEdge {
    std::string id;
    std::string start;
    std::string end;
    std::string label;
    std::string classes;
    std::string pattern; // "" or "dashed"
};

struct StateGraph {
    std::vector<StateNode> nodes;
    std::vector<StateEdge> edges;
};

namespace detail_graph {

struct Draft {
    std::string shape;
    bool isDescArray = false;
    std::string descStr;
    std::vector<std::string> descArr;
    std::string cssClasses;
    std::vector<std::string> cssStyles;
    std::string type;
    std::string dir;
    int colorIndex = -1;
    bool hasColorIndex = false;

    size_t descLength() const {
        return isDescArray ? descArr.size() : descStr.size();
    }
};

struct Frame {
    StateStmtPtr item;
    std::vector<StmtPtr> doc;
    size_t at = 0;
    bool alt = true;
};

inline std::string docDirection(const std::vector<StmtPtr>& doc) {
    std::string dir = "TB";
    for (const auto& item : doc) {
        if (item && item->kind == "dir") dir = item->value;
    }
    return dir;
}

} // namespace detail_graph

inline StateGraph buildStateGraph(
    const std::vector<StmtPtr>& rootDoc,
    const std::unordered_map<std::string, StateStmtPtr>& states,
    const std::unordered_map<std::string, StateClassDef>& classes
) {
    using namespace detail_graph;
    std::vector<StateNode> nodes;
    std::vector<StateEdge> edges;
    std::unordered_map<std::string, size_t> byId;
    std::unordered_map<std::string, Draft> drafts;
    std::unordered_map<std::string, std::pair<bool, int>> slots;
    int counter = 0;
    int nextSlot = 0;

    auto insert = [&](StateNode node) {
        if (node.id.empty() || node.id == "</join></fork>" || node.id == "</choice>") return;
        if (!node.cssClasses.empty()) {
            bool hasCompiled = false;
            std::vector<std::string> compiled;
            for (const auto& name : splitStr(node.cssClasses, ' ')) {
                auto it = classes.find(name);
                if (it == classes.end()) continue;
                if (!hasCompiled) {
                    compiled = node.cssCompiledStyles;
                    hasCompiled = true;
                }
                for (const auto& style : it->second.styles) compiled.push_back(style);
            }
            if (hasCompiled) node.cssCompiledStyles = std::move(compiled);
        }
        auto it = byId.find(node.id);
        if (it != byId.end()) {
            auto& existing = nodes[it->second];
            bool keepParent = node.hasParentId || existing.hasParentId;
            std::string parentId = node.hasParentId ? node.parentId : existing.parentId;
            existing = std::move(node);
            if (keepParent) {
                existing.parentId = parentId;
                existing.hasParentId = true;
            }
        } else {
            byId[node.id] = nodes.size();
            nodes.push_back(std::move(node));
        }
    };

    auto visit = [&](const StateStmtPtr& parent, const StateStmtPtr& item, bool alt) {
        const std::string& id = item->id;
        if (id == "root") return;
        auto itKnown = states.find(id);
        std::string classStr;
        std::vector<std::string> styles;
        if (itKnown != states.end() && itKnown->second) {
            const auto& known = *itKnown->second;
            for (size_t k = 0; k < known.classes.size(); k++) {
                if (k > 0) classStr += " ";
                classStr += known.classes[k];
            }
            styles = known.styles;
        }

        auto itDraft = drafts.find(id);
        if (itDraft == drafts.end()) {
            std::string shape = "rect";
            if (item->start == 1) shape = "stateStart";
            else if (item->start == 0) shape = "stateEnd";
            if (item->type != "default") shape = item->type; // note: if item->type is empty (e.g. note), shape becomes ""
            Draft d;
            d.shape = shape;
            d.isDescArray = false;
            d.descStr = id;
            d.cssClasses = classStr + " statediagram-state";
            d.cssStyles = styles;
            itDraft = drafts.emplace(id, std::move(d)).first;
        }
        Draft& draft = itDraft->second;

        bool itemDescTruthy = item->hasDescription && (item->isDescArray || !item->descStr.empty());
        if (itemDescTruthy) {
            if (draft.isDescArray) {
                draft.shape = "rectWithTitle";
                if (item->isDescArray) {
                    for (const auto& s : item->descArr) draft.descArr.push_back(s);
                } else {
                    draft.descArr.push_back(item->descStr);
                }
            } else if (!draft.descStr.empty()) {
                draft.shape = "rectWithTitle";
                std::vector<std::string> arr;
                if (draft.descStr != id) arr.push_back(draft.descStr);
                if (item->isDescArray) {
                    for (const auto& s : item->descArr) arr.push_back(s);
                } else {
                    arr.push_back(item->descStr);
                }
                draft.isDescArray = true;
                draft.descArr = std::move(arr);
                draft.descStr.clear();
            } else {
                draft.shape = "rect";
                if (item->isDescArray) {
                    draft.isDescArray = true;
                    draft.descArr = item->descArr;
                } else {
                    draft.isDescArray = false;
                    draft.descStr = item->descStr;
                }
            }
        }
        if (draft.descLength() == 1 && draft.shape == "rectWithTitle") {
            draft.shape = (draft.type == "group") ? "roundedWithTitle" : "rect";
        }

        if (draft.type.empty() && item->hasDoc) {
            draft.type = "group";
            draft.dir = docDirection(item->doc);
            draft.shape = (item->type == "divider") ? "divider" : "roundedWithTitle";
            if (draft.shape == "divider" && parent && !parent->id.empty() && slots.count(parent->id)) {
                auto sl = slots[parent->id];
                draft.hasColorIndex = sl.first;
                draft.colorIndex = sl.second;
            } else {
                int slot = nextSlot++;
                if (!trimStr(classStr).empty() || !styles.empty()) {
                    draft.hasColorIndex = false;
                    draft.colorIndex = -1;
                } else {
                    draft.hasColorIndex = true;
                    draft.colorIndex = slot;
                }
            }
            slots[id] = {draft.hasColorIndex, draft.colorIndex};
            draft.cssClasses = draft.cssClasses + " statediagram-cluster " + (alt ? "statediagram-cluster-alt" : "");
        }

        StateNode node;
        node.id = id;
        node.shape = draft.shape;
        if (draft.isDescArray) {
            if (!draft.descArr.empty()) {
                node.label = draft.descArr[0];
                node.hasLabel = true;
                for (size_t k = 1; k < draft.descArr.size(); k++) node.description.push_back(draft.descArr[k]);
            }
        } else {
            node.label = draft.descStr;
            node.hasLabel = true;
        }
        node.cssClasses = draft.cssClasses;
        node.cssStyles = draft.cssStyles;
        node.type = draft.type;
        node.isGroup = (draft.type == "group");
        node.dir = draft.dir;
        node.colorIndex = draft.colorIndex;
        node.hasColorIndex = draft.hasColorIndex;
        if (node.shape == "divider") {
            node.label = "";
            node.hasLabel = true;
        }
        if (parent && parent->id != "root") {
            node.parentId = parent->id;
            node.hasParentId = true;
        }

        if (!item->hasNote) {
            insert(std::move(node));
            return;
        }
        const auto& note = item->note;
        auto aside = [&](const std::string& asideId, const std::string& shape, const std::string& cssClasses, bool isGroup) -> StateNode {
            StateNode n;
            n.id = asideId;
            n.shape = shape;
            n.label = note.text;
            n.hasLabel = true;
            n.cssClasses = cssClasses;
            n.type = isGroup ? "group" : "node";
            n.isGroup = isGroup;
            n.position = note.position;
            return n;
        };
        StateNode noteNode = aside(id + "----note-" + std::to_string(counter++), "note", "statediagram-note", false);
        noteNode.parentId = id + "----parent";
        noteNode.hasParentId = true;
        noteNode.noteFor = id;
        noteNode.hasNoteFor = true;
        insert(aside(noteNode.parentId, "noteGroup", draft.cssClasses, true));
        std::string noteNodeId = noteNode.id;
        insert(std::move(noteNode));
        insert(std::move(node));
        bool left = (note.position == "left of");
        std::string from = left ? noteNodeId : id;
        std::string to = left ? id : noteNodeId;
        edges.push_back({from + "-" + to, from, to, "", "transition note-edge", "dashed"});
    };

    std::vector<Frame> stack;
    stack.push_back({nullptr, rootDoc, 0, true});
    while (!stack.empty()) {
        auto& frame = stack.back();
        if (frame.at >= frame.doc.size()) {
            stack.pop_back();
            continue;
        }
        auto item = frame.doc[frame.at++];
        if (!item || item->kind == "string") continue;
        if (item->kind == "state") {
            visit(frame.item, item->state, frame.alt);
            if (item->state->hasDoc) {
                bool nextAlt = !frame.alt;
                stack.push_back({item->state, item->state->doc, 0, nextAlt});
            }
        } else if (item->kind == "relation") {
            visit(frame.item, item->state1, frame.alt);
            visit(frame.item, item->state2, frame.alt);
            edges.push_back({"edge" + std::to_string(counter++), item->state1->id, item->state2->id, item->hasDescription ? item->description : "", "transition", ""});
        }
    }

    for (const auto& node : nodes) {
        if (node.isGroup && !node.description.empty()) {
            throw PeleError(
                "Group nodes can only have label. Remove the additional description for node [" + node.id + "]",
                "semantic",
                "state"
            );
        }
    }
    return {std::move(nodes), std::move(edges)};
}

// ============================================================================
// src/diagrams/state/db.ts
// ============================================================================

class StateDb : public DiagramMeta {
public:
    std::vector<StmtPtr> rootDoc;
    std::unordered_map<std::string, StateStmtPtr> states;
    std::vector<StateRelation> relations;
    std::unordered_map<std::string, StateClassDef> classes;
    std::unordered_map<std::string, StateLink> links;
    StateGraph graph;

private:
    int dividerCount = 0;

public:
    std::string direction() const {
        std::string dir = "TB";
        for (const auto& item : rootDoc) {
            if (item && item->kind == "dir") dir = item->value;
        }
        return dir;
    }

    void setRootDoc(std::vector<StmtPtr> doc) {
        rootDoc = std::move(doc);
        translate();
        extract(rootDoc);
    }

    void extract(const std::vector<StmtPtr>& doc) {
        states.clear();
        relations.clear();
        classes.clear();
        for (const auto& item : doc) {
            if (!item || item->kind == "string") continue;
            if (item->kind == "state") {
                const auto& st = *item->state;
                addState(trimStr(st.id), st.type.empty() ? "default" : st.type, st.hasDoc, st.doc, st.hasDescription, st.isDescArray, st.descStr, st.descArr, st.hasNote, st.note, false, {});
            } else if (item->kind == "relation") {
                addRelation(item->state1, item->state2, item->hasDescription ? item->description : "");
            } else if (item->kind == "classDef") {
                addStyleClass(trimStr(item->id), item->classes);
            } else if (item->kind == "style") {
                std::vector<std::string> styles;
                for (const auto& s : splitStr(item->styleClass, ',')) {
                    styles.push_back(trimStr(replaceAllStr(s, ";", "")));
                }
                for (auto& state : named(trimStr(item->id))) {
                    state->styles = styles;
                }
            } else if (item->kind == "applyClass") {
                setCssClass(trimStr(item->id), item->styleClass);
            } else if (item->kind == "click") {
                std::string cid = item->clickState ? item->clickState->id : item->id;
                addLink(cid, item->url, item->tooltip);
            }
        }
        graph = buildStateGraph(doc, states, classes);
    }

    void translate() {
        struct PendingItem {
            std::string parentId;
            StmtPtr stmt;
            StateStmtPtr stateOnly;
            bool isRoot = false;
            bool first = true;
        };
        std::vector<PendingItem> pending;
        pending.push_back({"root", nullptr, nullptr, true, true});
        while (!pending.empty()) {
            auto cur = pending.back();
            pending.pop_back();
            if (!cur.isRoot && !cur.stmt && !cur.stateOnly) continue;
            if (cur.stmt && cur.stmt->kind == "string") continue;
            if (cur.stmt && cur.stmt->kind == "relation") {
                pending.push_back({cur.parentId, nullptr, cur.stmt->state2, false, false});
                pending.push_back({cur.parentId, nullptr, cur.stmt->state1, false, true});
                continue;
            }
            StateStmtPtr st = cur.stateOnly ? cur.stateOnly : (cur.stmt && cur.stmt->kind == "state" ? cur.stmt->state : nullptr);
            if (st) {
                if (st->id == "[*]") {
                    st->id = cur.parentId + (cur.first ? "_start" : "_end");
                    st->start = cur.first ? 1 : 0;
                } else {
                    st->id = trimStr(st->id);
                }
            } else if (!cur.isRoot) {
                continue;
            }

            std::vector<StmtPtr>* docPtr = cur.isRoot ? &rootDoc : (st && st->hasDoc ? &st->doc : nullptr);
            if (!docPtr) continue;
            std::string nodeId = cur.isRoot ? "root" : st->id;

            std::vector<StmtPtr> regions;
            std::vector<StmtPtr> current;
            for (const auto& item : *docPtr) {
                if (item && item->kind == "state" && item->state->type == "divider" && !item->state->hasDoc) {
                    auto copy = std::make_shared<StateStmt>(*item->state);
                    copy->hasDoc = true;
                    copy->doc = std::move(current);
                    current.clear();
                    regions.push_back(makeStateStmtWrap(copy));
                } else {
                    current.push_back(item);
                }
            }
            if (!regions.empty()) {
                if (!current.empty()) {
                    auto div = std::make_shared<StateStmt>();
                    div->id = getDividerId();
                    div->type = "divider";
                    div->hasDoc = true;
                    div->doc = std::move(current);
                    regions.push_back(makeStateStmtWrap(div));
                }
                *docPtr = std::move(regions);
            }
            for (int i = static_cast<int>(docPtr->size()) - 1; i >= 0; i--) {
                pending.push_back({nodeId, (*docPtr)[i], nullptr, false, true});
            }
        }
    }

    void addState(
        const std::string& id,
        const std::string& type = "default",
        bool hasDoc = false,
        const std::vector<StmtPtr>& doc = {},
        bool hasDescr = false,
        bool isDescArray = false,
        const std::string& descStr = "",
        const std::vector<std::string>& descArr = {},
        bool hasNote = false,
        const StateNote& note = {},
        bool hasClasses = false,
        const std::vector<std::string>& cls = {}
    ) {
        std::string trimmedId = trimStr(id);
        auto it = states.find(trimmedId);
        StateStmtPtr state;
        if (it == states.end()) {
            state = std::make_shared<StateStmt>();
            state->id = trimmedId;
            state->type = type;
            state->hasDoc = hasDoc;
            state->doc = doc;
            state->hasNote = hasNote;
            state->note = note;
            states[trimmedId] = state;
        } else {
            state = it->second;
            if (!state->hasDoc && hasDoc) {
                state->hasDoc = true;
                state->doc = doc;
            }
            if (state->type.empty()) state->type = type;
        }
        if (hasDescr && (isDescArray || !descStr.empty())) {
            if (isDescArray) {
                for (const auto& t : descArr) addDescription(trimmedId, trimStr(t));
            } else {
                addDescription(trimmedId, trimStr(descStr));
            }
        }
        if (hasNote) {
            state->hasNote = true;
            state->note = note;
        }
        if (hasClasses) {
            for (const auto& name : cls) setCssClass(trimmedId, trimStr(name));
        }
    }

    void addLink(const std::string& stateId, const std::string& url, const std::string& tooltip) {
        links[stateId] = {url, tooltip};
    }

    void addRelation(const StateStmtPtr& item1, const StateStmtPtr& item2, const std::string& title = "") {
        for (const auto& item : {item1, item2}) {
            addState(
                item->id,
                item->type.empty() ? "default" : item->type,
                item->hasDoc,
                item->doc,
                item->hasDescription,
                item->isDescArray,
                item->descStr,
                item->descArr,
                item->hasNote,
                item->note,
                item->hasClasses,
                item->classes
            );
        }
        relations.push_back({trimStr(item1->id), trimStr(item2->id), title});
    }

    void addDescription(const std::string& id, const std::string& descr) {
        auto it = states.find(id);
        if (it == states.end() || !it->second) return;
        std::string d = (!descr.empty() && descr[0] == ':') ? trimStr(descr.substr(1)) : descr;
        it->second->descriptions.push_back(d);
    }

    std::string getDividerId() {
        return "divider-id-" + std::to_string(++dividerCount);
    }

    void addStyleClass(const std::string& id, const std::string& styleAttributes = "") {
        auto it = classes.find(id);
        if (it == classes.end()) {
            it = classes.emplace(id, StateClassDef{id, {}, {}}).first;
        }
        if (styleAttributes.empty()) return;
        auto replaceFirst = [](std::string s, const std::string& from, const std::string& to) -> std::string {
            size_t pos = s.find(from);
            if (pos != std::string::npos) s.replace(pos, from.size(), to);
            return s;
        };
        for (const auto& attrib : splitStr(styleAttributes, ',')) {
            std::string fixed = trimStr(replaceFirst(attrib, ";", ""));
            if (attrib.find("color") != std::string::npos) {
                it->second.textStyles.push_back(replaceFirst(replaceFirst(fixed, "fill", "bgFill"), "color", "fill"));
            }
            it->second.styles.push_back(fixed);
        }
    }

    void setCssClass(const std::string& itemIds, const std::string& cssClassName) {
        for (auto& state : named(itemIds)) {
            state->classes.push_back(cssClassName);
        }
    }

    void setDirection(const std::string& dir) {
        for (auto& item : rootDoc) {
            if (item && item->kind == "dir") {
                item->value = dir;
                return;
            }
        }
        auto d = std::make_shared<Stmt>();
        d->kind = "dir";
        d->value = dir;
        rootDoc.insert(rootDoc.begin(), d);
    }

    std::string trimColon(const std::string& text) const {
        return (!text.empty() && text[0] == ':') ? trimStr(text.substr(1)) : trimStr(text);
    }

    void setAccTitle(const std::string& text) {
        size_t i = 0;
        while (i < text.size() && isSpace(charCodeAt(text, static_cast<int>(i)))) i++;
        accTitle = text.substr(i);
    }

    void setAccDescription(const std::string& text) {
        std::string out;
        out.reserve(text.size());
        for (size_t i = 0; i < text.size();) {
            if (text[i] == '\n') {
                out.push_back('\n');
                i++;
                while (i < text.size() && isSpace(charCodeAt(text, static_cast<int>(i)))) i++;
            } else {
                out.push_back(text[i++]);
            }
        }
        accDescr = std::move(out);
    }

private:
    std::vector<StateStmtPtr> named(const std::string& ids) {
        std::vector<StateStmtPtr> out;
        for (const auto& id : splitStr(ids, ',')) {
            auto it = states.find(id);
            if (it == states.end()) {
                addState(id);
                it = states.find(trimStr(id));
            }
            if (it != states.end() && it->second) out.push_back(it->second);
        }
        return out;
    }
};

// ============================================================================
// src/diagrams/state/tokens.ts & src/diagrams/state/lexer.ts
// ============================================================================

namespace detail_parse {

enum T : int {
    END = 0,
    ERROR = 1,
    NL = 2,
    SD = 3,
    DESCR = 4,
    ARROW = 5,
    HIDE_EMPTY = 6,
    scale = 7,
    WIDTH = 8,
    COMPOSIT_STATE = 9,
    STRUCT_START = 10,
    STRUCT_STOP = 11,
    STATE_DESCR = 12,
    AS = 13,
    ID = 14,
    FORK = 15,
    JOIN = 16,
    CHOICE = 17,
    CONCURRENT = 18,
    note = 19,
    NOTE_TEXT = 20,
    acc_title = 21,
    acc_title_value = 22,
    acc_descr = 23,
    acc_descr_value = 24,
    acc_descr_multiline_value = 25,
    CLICK = 26,
    STRING = 27,
    HREF = 28,
    classDef = 29,
    CLASSDEF_ID = 30,
    DEFAULT_CLASSDEF_ID = 31,
    CLASSDEF_STYLEOPTS = 32,
    DEFAULT = 33,
    style = 34,
    STYLE_IDS = 35,
    STYLEDEF_STYLEOPTS = 36,
    class_tok = 37,
    CLASSENTITY_IDS = 38,
    STYLECLASS = 39,
    direction_tb = 40,
    direction_bt = 41,
    direction_rl = 42,
    direction_lr = 43,
    EDGE_STATE = 44,
    STYLE_SEPARATOR = 45,
    left_of = 46,
    right_of = 47,
    INVALID = 48,
};

inline const char* const TOKEN_NAMES[] = {
    "$end",
    "INVALID",
    "NL",
    "SD",
    "DESCR",
    "-->",
    "HIDE_EMPTY",
    "scale",
    "WIDTH",
    "COMPOSIT_STATE",
    "STRUCT_START",
    "STRUCT_STOP",
    "STATE_DESCR",
    "AS",
    "ID",
    "FORK",
    "JOIN",
    "CHOICE",
    "CONCURRENT",
    "note",
    "NOTE_TEXT",
    "acc_title",
    "acc_title_value",
    "acc_descr",
    "acc_descr_value",
    "acc_descr_multiline_value",
    "CLICK",
    "STRING",
    "HREF",
    "classDef",
    "CLASSDEF_ID",
    "DEFAULT_CLASSDEF_ID",
    "CLASSDEF_STYLEOPTS",
    "DEFAULT",
    "style",
    "STYLE_IDS",
    "STYLEDEF_STYLEOPTS",
    "class",
    "CLASSENTITY_IDS",
    "STYLECLASS",
    "direction_tb",
    "direction_bt",
    "direction_rl",
    "direction_lr",
    "EDGE_STATE",
    "STYLE_SEPARATOR",
    "left_of",
    "right_of",
    "INVALID",
};

struct StateTokens {
    std::vector<int> types;
    std::vector<std::string> texts;
    std::vector<int> starts;
    std::string error;
    bool hasError = false;
};

enum class S : int {
    INITIAL,
    struct_st,
    STATE,
    STATE_STRING,
    STATE_ID,
    SCALE,
    acc_title,
    acc_descr,
    acc_descr_multiline,
    CLASSDEF,
    CLASSDEFID,
    CLASS,
    CLASS_STYLE,
    STYLE,
    STYLEDEF_STYLES,
    NOTE,
    NOTE_ID,
    NOTE_TEXT,
    FLOATING_NOTE,
    FLOATING_NOTE_ID,
};

inline StateTokens tokenizeState(const std::string& src) {
    int n = static_cast<int>(src.size());
    std::vector<int> types;
    std::vector<std::string> texts;
    std::vector<int> starts;
    std::vector<S> stack;
    S state = S::INITIAL;
    int p = 0;
    std::string error;
    bool hasError = false;

    auto ch = [&](int q) -> int {
        return (q >= 0 && q < n) ? static_cast<unsigned char>(src[q]) : -1;
    };
    auto word = [&](const char* w, int q) -> bool {
        for (int i = 0; w[i] != '\0'; i++) {
            int a = static_cast<unsigned char>(w[i]);
            int c = ch(q + i);
            if (c != a && !(a >= 97 && a <= 122 && c == a - 32)) return false;
        }
        return true;
    };

    auto makeFinder = [n](auto matchFn) {
        int from = 0;
        int next = -2;
        return [n, matchFn, from, next](int q) mutable -> int {
            if (next == -2 || q < from || (next != -1 && q > next)) {
                next = -1;
                for (int k = q; k < n; k++) {
                    if (matchFn(k)) {
                        next = k;
                        break;
                    }
                }
                from = q;
            }
            return next;
        };
    };

    auto nextDirection = makeFinder([&](int k) -> bool {
        return (ch(k) | 32) == 'd' && word("direction", k);
    });
    auto nextStereotype = makeFinder([&](int k) -> bool {
        int c0 = ch(k);
        int c1 = ch(k + 1);
        if (!((c0 == '<' && c1 == '<') || (c0 == '[' && c1 == '['))) return false;
        const char* close = (c0 == '<') ? ">>" : "]]";
        if (word("fork", k + 2) && ch(k + 6) == close[0] && ch(k + 7) == close[1]) return true;
        if (word("join", k + 2) && ch(k + 6) == close[0] && ch(k + 7) == close[1]) return true;
        if (word("choice", k + 2) && ch(k + 8) == close[0] && ch(k + 9) == close[1]) return true;
        return false;
    });
    auto nextLineEnd = makeFinder([&](int k) -> bool {
        int c0 = ch(k);
        if (c0 == '\n' || c0 == '\r') return true;
        // \u2028 (E2 80 A8) or \u2029 (E2 80 A9) in UTF-8
        if (c0 == 0xE2 && ch(k + 1) == 0x80 && (ch(k + 2) == 0xA8 || ch(k + 2) == 0xA9)) return true;
        return false;
    });
    auto nextPercent = makeFinder([&](int k) -> bool {
        return ch(k) == '%' && ch(k + 1) == '%';
    });
    auto nextBrace = makeFinder([&](int k) -> bool {
        return ch(k) == '{';
    });

    int dirFrom = -1;
    int dirTo = -1;
    int dirAt[4] = {-1, -1, -1, -1};
    int dirEnd[4] = {0, 0, 0, 0};

    int stereoFrom = -1;
    int stereoTo = -1;
    int stereoAt[6] = {-1, -1, -1, -1, -1, -1};

    auto emit = [&](int type, int s, int e) {
        types.push_back(type);
        texts.push_back(src.substr(s, e - s));
        starts.push_back(s);
        p = e;
    };
    auto emitText = [&](int type, const std::string& text, int s, int e) {
        types.push_back(type);
        texts.push_back(text);
        starts.push_back(s);
        p = e;
    };
    auto push = [&](S s) {
        stack.push_back(state);
        state = s;
    };
    auto pop = [&]() {
        if (!stack.empty()) {
            state = stack.back();
            stack.pop_back();
        }
    };
    auto wsEnd = [&](int q) -> int {
        int j = q;
        while (j < n && isSpace(charCodeAt(src, j))) j++;
        return j;
    };
    auto wordEnd = [&](int q) -> int {
        int j = q;
        while (j < n && isWord(charCodeAt(src, j))) j++;
        return j;
    };
    auto eol = [&](int q) -> int {
        size_t k = src.find('\n', q);
        return k == std::string::npos ? n : static_cast<int>(k);
    };
    auto lineEnd = [&](int q) -> int {
        int k = nextLineEnd(q);
        return k == -1 ? n : k;
    };

    auto matchDirection = [&](int q) -> int {
        if (q < dirFrom || q > dirTo) {
            int first = nextDirection(q);
            int end = first == -1 ? n : lineEnd(q);
            dirFrom = q;
            dirTo = end;
            dirAt[0] = dirAt[1] = dirAt[2] = dirAt[3] = -1;
            for (int d = first; d != -1 && d < end; d = nextDirection(d + 1)) {
                int w = wsEnd(d + 9);
                if (w == d + 9) continue;
                int kind = word("tb", w) ? 0 : word("bt", w) ? 1 : word("rl", w) ? 2 : word("lr", w) ? 3 : -1;
                if (kind == -1) continue;
                dirAt[kind] = d;
                dirEnd[kind] = w + 2;
            }
        }
        for (int kind = 0; kind < 4; kind++) if (dirAt[kind] >= q) return kind;
        return -1;
    };

    auto matchStereotype = [&](int q) -> int {
        if (q < stereoFrom || q > stereoTo) {
            int first = nextStereotype(q);
            int end = first == -1 ? n : lineEnd(q);
            stereoFrom = q;
            stereoTo = end;
            for (int k = 0; k < 6; k++) stereoAt[k] = -1;
            for (int d = first; d != -1 && d < end; d = nextStereotype(d + 1)) {
                int letter = ch(d + 2) | 32;
                stereoAt[(ch(d) == 60 ? 0 : 3) + (letter == 102 ? 0 : letter == 106 ? 1 : 2)] = d;
            }
        }
        for (int kind = 0; kind < 6; kind++) if (stereoAt[kind] >= q) return kind;
        return -1;
    };

    auto emitId = [&](int s, int e) -> bool {
        int at = nextPercent(s);
        if (at == -1 || at + 2 > e) {
            emit(T::ID, s, e);
            return true;
        }
        if (at == s) {
            p = e;
            return false;
        }
        emit(T::ID, s, at);
        return true;
    };

    auto finish = [&](int type) -> StateTokens {
        types.push_back(type);
        texts.push_back("");
        starts.push_back(p);
        return {std::move(types), std::move(texts), std::move(starts), error, hasError};
    };

    for (;;) {
        if (p >= n) {
            switch (state) {
                case S::INITIAL: emit(T::NL, n, n); break;
                case S::acc_title: emit(T::acc_title_value, n, n); break;
                case S::acc_descr: emit(T::acc_descr_value, n, n); break;
                case S::acc_descr_multiline: emit(T::acc_descr_multiline_value, n, n); break;
                case S::CLASSDEFID: emit(T::CLASSDEF_STYLEOPTS, n, n); break;
                case S::CLASS_STYLE: emit(T::STYLECLASS, n, n); break;
                case S::STYLEDEF_STYLES: emit(T::STYLEDEF_STYLEOPTS, n, n); break;
                case S::STATE_STRING: emit(T::STATE_DESCR, n, n); break;
                case S::FLOATING_NOTE: emit(T::NOTE_TEXT, n, n); break;
                case S::STATE_ID:
                case S::FLOATING_NOTE_ID:
                    emit(T::ID, n, n);
                    break;
                default: break;
            }
            return finish(T::END);
        }

        int c = charCodeAt(src, p);
        switch (state) {
            case S::INITIAL:
            case S::struct_st: {
                bool root = (state == S::INITIAL);
                int lc = c | 32;
                if (root) {
                    if (lc == 99 && word("click", p) && !isWord(charCodeAt(src, p + 5))) {
                        emit(T::CLICK, p, p + 5);
                        continue;
                    }
                    if (lc == 104 && word("href", p) && !isWord(charCodeAt(src, p + 4))) {
                        emit(T::HREF, p, p + 4);
                        continue;
                    }
                    if (c == 34) {
                        size_t close = src.find('"', p + 1);
                        if (close != std::string::npos) {
                            emit(T::STRING, p, static_cast<int>(close + 1));
                            continue;
                        }
                    }
                    if (lc == 100 && word("default", p) && !isWord(charCodeAt(src, p + 7))) {
                        emit(T::DEFAULT, p, p + 7);
                        continue;
                    }
                    int kind = matchDirection(p);
                    if (kind != -1) {
                        emit(T::direction_tb + kind, p, eol(dirEnd[kind]));
                        continue;
                    }
                    if (c == 10) {
                        int j = p + 1;
                        while (ch(j) == 10) j++;
                        emit(T::NL, p, j);
                        continue;
                    }
                    if (isSpace(c)) {
                        p = wsEnd(p);
                        continue;
                    }
                } else if (c != 10 && isSpace(c)) {
                    int j = p + 1;
                    while (j < n && ch(j) != 10 && isSpace(charCodeAt(src, j))) j++;
                    p = j;
                    continue;
                }
                if (c == 35 || (c == 37 && ch(p + 1) == 37 && ch(p + 2) != 123)) {
                    p = eol(p);
                    continue;
                }
                if (lc == 115) {
                    if (root && word("scale", p)) {
                        int e = wsEnd(p + 5);
                        if (e > p + 5) {
                            emit(T::scale, p, e);
                            push(S::SCALE);
                            continue;
                        }
                    }
                    if (word("style", p)) {
                        int e = wsEnd(p + 5);
                        if (e > p + 5) {
                            emit(T::style, p, e);
                            push(S::STYLE);
                            continue;
                        }
                    }
                    if (word("state", p)) {
                        int e = wsEnd(p + 5);
                        if (e > p + 5) {
                            p = e;
                            push(S::STATE);
                            continue;
                        }
                        if (root && word("diagram", p + 5)) {
                            int from = word("-v2", p + 12) ? p + 15 : p + 12;
                            e = wsEnd(from);
                            if (e > from) {
                                emit(T::SD, p, e);
                                continue;
                            }
                        }
                    }
                } else if (lc == 99) {
                    if (word("class", p)) {
                        bool def = word("def", p + 5);
                        int from = def ? p + 8 : p + 5;
                        int e = wsEnd(from);
                        if (e > from) {
                            emit(def ? T::classDef : T::class_tok, p, e);
                            push(def ? S::CLASSDEF : S::CLASS);
                            continue;
                        }
                    }
                } else if (lc == 97 && root) {
                    if (word("acctitle", p) || word("accdescr", p)) {
                        bool title = (ch(p + 3) | 32) == 116;
                        int q = wsEnd(p + 8);
                        int d = ch(q);
                        if (d == 58) {
                            emit(title ? T::acc_title : T::acc_descr, p, wsEnd(q + 1));
                            push(title ? S::acc_title : S::acc_descr);
                            continue;
                        }
                        if (d == 123 && !title) {
                            p = wsEnd(q + 1);
                            push(S::acc_descr_multiline);
                            continue;
                        }
                    }
                }
                if (root) {
                    if (c == 123) {
                        emit(T::STRUCT_START, p, p + 1);
                        push(S::struct_st);
                        continue;
                    }
                } else {
                    int kind = matchDirection(p);
                    if (kind != -1) {
                        emit(T::direction_tb + kind, p, eol(dirEnd[kind]));
                        continue;
                    }
                    if (c == 125) {
                        emit(T::STRUCT_STOP, p, p + 1);
                        pop();
                        continue;
                    }
                    if (c == 10) {
                        p++;
                        continue;
                    }
                }
                if (lc == 110 && word("note", p)) {
                    int e = wsEnd(p + 4);
                    if (e > p + 4) {
                        emit(T::note, p, e);
                        push(S::NOTE);
                        continue;
                    }
                }
                if (root && lc == 104 && word("hide empty description", p) && !isWord(charCodeAt(src, p + 22))) {
                    emit(T::HIDE_EMPTY, p, p + 22);
                    continue;
                }
                if (c == 91 && ch(p + 1) == 42 && ch(p + 2) == 93) {
                    emit(T::EDGE_STATE, p, p + 3);
                    continue;
                }
                int j = p;
                while (j < n) {
                    int d = charCodeAt(src, j);
                    if (d == 58 || d == 45 || d == 123 || isSpace(d)) break;
                    j++;
                }
                if (j > p) {
                    emitId(p, j);
                    continue;
                }
                if (c == 58) {
                    j = p + 1;
                    while (j < n) {
                        int d = ch(j);
                        if (d == 10 || d == 59) break;
                        if (d == 58) {
                            int after = ch(j + 1);
                            if (j + 1 >= n || after == 58 || after == 10 || after == 59) break;
                            j++;
                        }
                        j++;
                    }
                    if (j > p + 1) {
                        emitText(T::DESCR, trimStr(src.substr(p, j - p)), p, j);
                        continue;
                    }
                    if (ch(p + 1) == 58 && ch(p + 2) == 58) {
                        emit(T::STYLE_SEPARATOR, p, p + 3);
                        continue;
                    }
                } else if (c == 45 && ch(p + 1) == 45) {
                    if (ch(p + 2) == 62) {
                        emit(T::ARROW, p, p + 3);
                        continue;
                    }
                    if (!root) {
                        emit(T::CONCURRENT, p, p + 2);
                        continue;
                    }
                }
                if (!root) return finish(T::ERROR);
                emit(T::INVALID, p, p + 1);
                continue;
            }

            case S::STATE: {
                if (c != 10 && isSpace(c)) {
                    int j = p + 1;
                    while (j < n && ch(j) != 10 && isSpace(charCodeAt(src, j))) j++;
                    p = j;
                    continue;
                }
                if (c == 35 || (c == 37 && ch(p + 1) == 37 && ch(p + 2) != 123)) {
                    p = eol(p);
                    continue;
                }
                int kind = matchStereotype(p);
                if (kind != -1) {
                    int at = stereoAt[kind];
                    bool choice = (kind == 2 || kind == 5);
                    emitText(choice ? T::CHOICE : (kind % 3 == 0 ? T::FORK : T::JOIN), trimStr(src.substr(p, at - p)), p, at + (choice ? 10 : 8));
                    pop();
                    continue;
                }
                if (c == 34) {
                    p++;
                    push(S::STATE_STRING);
                    continue;
                }
                int q = (c == 10) ? wsEnd(p) : p;
                if (word("as", q)) {
                    int e = wsEnd(q + 2);
                    if (e > q + 2) {
                        emit(T::AS, p, e);
                        push(S::STATE_ID);
                        continue;
                    }
                }
                if (c == 10) {
                    p++;
                    pop();
                    continue;
                }
                if (c == 123) {
                    emit(T::STRUCT_START, p, p + 1);
                    pop();
                    push(S::struct_st);
                    continue;
                }
                if (isWord(c)) {
                    int first = wordEnd(p);
                    int second = wsEnd(first);
                    if (second > first && isWord(charCodeAt(src, second))) {
                        int brace = nextBrace(second + 1);
                        if (brace != -1 && brace < lineEnd(second)) {
                            error = "Error: State name must be a single word. Found: \"" + trimStr(src.substr(p, brace + 1 - p)) + "\"";
                            hasError = true;
                            return finish(T::ERROR);
                        }
                    }
                }
                int j = p + 1;
                while (j < n) {
                    int d = charCodeAt(src, j);
                    if (d == 123 || isSpace(d)) break;
                    j++;
                }
                emit(T::COMPOSIT_STATE, p, j);
                continue;
            }

            case S::STATE_STRING: {
                if (c == 34) {
                    p++;
                    pop();
                    continue;
                }
                size_t close = src.find('"', p);
                emit(T::STATE_DESCR, p, close == std::string::npos ? n : static_cast<int>(close));
                continue;
            }

            case S::STATE_ID: {
                int j = p;
                while (j < n) {
                    int d = ch(j);
                    if (d == 10 || d == 123) break;
                    j++;
                }
                if (emitId(p, j)) pop();
                continue;
            }

            case S::SCALE: {
                if (c >= 48 && c <= 57) {
                    int j = p + 1;
                    while (j < n && ch(j) >= 48 && ch(j) <= 57) j++;
                    emit(T::WIDTH, p, j);
                    continue;
                }
                int q = wsEnd(p);
                if (q > p && word("width", q) && !isWord(charCodeAt(src, q + 5))) {
                    p = q + 5;
                    pop();
                    continue;
                }
                return finish(T::ERROR);
            }

            case S::acc_title:
                emit(T::acc_title_value, p, eol(p));
                pop();
                continue;

            case S::acc_descr:
                emit(T::acc_descr_value, p, eol(p));
                pop();
                continue;

            case S::acc_descr_multiline: {
                if (c == 125) {
                    p++;
                    pop();
                    continue;
                }
                size_t close = src.find('}', p);
                emit(T::acc_descr_multiline_value, p, close == std::string::npos ? n : static_cast<int>(close));
                continue;
            }

            case S::CLASSDEF: {
                bool isDefault = word("default", p) && wsEnd(p + 7) > p + 7;
                int j = isDefault ? p + 7 : wordEnd(p);
                int e = wsEnd(j);
                if (j == p || e == j) return finish(T::ERROR);
                emit(isDefault ? T::DEFAULT_CLASSDEF_ID : T::CLASSDEF_ID, p, e);
                pop();
                push(S::CLASSDEFID);
                continue;
            }

            case S::CLASSDEFID:
                emit(T::CLASSDEF_STYLEOPTS, p, eol(p));
                pop();
                continue;

            case S::CLASS: {
                int j = wordEnd(p);
                if (j == p) return finish(T::ERROR);
                while (ch(j) == 44) {
                    int k = wsEnd(j + 1);
                    int e = wordEnd(k);
                    if (e == k) break;
                    j = e;
                }
                emit(T::CLASSENTITY_IDS, p, j);
                pop();
                push(S::CLASS_STYLE);
                continue;
            }

            case S::CLASS_STYLE:
                emit(T::STYLECLASS, p, eol(p));
                pop();
                continue;

            case S::STYLE: {
                int j = p;
                while (j < n && (isWord(charCodeAt(src, j)) || ch(j) == 44)) j++;
                int e = wsEnd(j);
                if (j == p || e == j) return finish(T::ERROR);
                emit(T::STYLE_IDS, p, e);
                pop();
                push(S::STYLEDEF_STYLES);
                continue;
            }

            case S::STYLEDEF_STYLES:
                emit(T::STYLEDEF_STYLEOPTS, p, eol(p));
                pop();
                continue;

            case S::NOTE: {
                if (word("left of", p) && !isWord(charCodeAt(src, p + 7))) {
                    emit(T::left_of, p, p + 7);
                    pop();
                    push(S::NOTE_ID);
                    continue;
                }
                if (word("right of", p) && !isWord(charCodeAt(src, p + 8))) {
                    emit(T::right_of, p, p + 8);
                    pop();
                    push(S::NOTE_ID);
                    continue;
                }
                if (c == 34) {
                    p++;
                    pop();
                    push(S::FLOATING_NOTE);
                    continue;
                }
                return finish(T::ERROR);
            }

            case S::NOTE_ID: {
                int q = wsEnd(p);
                int j = q;
                while (j < n) {
                    int d = charCodeAt(src, j);
                    if (d == 58 || d == 45 || isSpace(d)) break;
                    j++;
                }
                if (j == q) return finish(T::ERROR);
                if (emitId(p, j)) {
                    pop();
                    push(S::NOTE_TEXT);
                }
                continue;
            }

            case S::NOTE_TEXT: {
                int q = wsEnd(p);
                if (ch(q) == 58) {
                    int j = q + 1;
                    while (j < n) {
                        int d = ch(j);
                        if (d == 58 || d == 10 || d == 59) break;
                        j++;
                    }
                    if (j > q + 1) {
                        emitText(T::NOTE_TEXT, trimStr(src.substr(p + 2, j - (p + 2))), p, j);
                        pop();
                        continue;
                    }
                }
                size_t kPos = src.find('\n', p);
                int k = kPos == std::string::npos ? -1 : static_cast<int>(kPos);
                int e = -1;
                while (k != -1) {
                    e = wsEnd(k);
                    if (word("end note", e) && !isWord(charCodeAt(src, e + 8))) break;
                    size_t nextNl = src.find('\n', e);
                    k = nextNl == std::string::npos ? -1 : static_cast<int>(nextNl);
                }
                if (k == -1) return finish(T::ERROR);
                emitText(T::NOTE_TEXT, trimStr(src.substr(p, e - p)), p, e + 8);
                pop();
                continue;
            }

            case S::FLOATING_NOTE: {
                int q = wsEnd(p);
                if (word("as", q)) {
                    emit(T::AS, p, wsEnd(q + 2));
                    pop();
                    push(S::FLOATING_NOTE_ID);
                    continue;
                }
                if (c == 34) {
                    p++;
                    continue;
                }
                size_t close = src.find('"', p);
                emit(T::NOTE_TEXT, p, close == std::string::npos ? n : static_cast<int>(close));
                continue;
            }

            case S::FLOATING_NOTE_ID:
                if (emitId(p, eol(p))) pop();
                continue;
        }
    }
}

// ============================================================================
// src/diagrams/state/parser.ts
// ============================================================================

inline void parseState(const std::string& src, StateDb& db) {
    static const char* DIRECTIONS[] = {"TB", "BT", "RL", "LR"};
    static const std::string STATEMENT =
        "'NL', 'ID', 'HIDE_EMPTY', 'scale', 'COMPOSIT_STATE', 'STATE_DESCR', 'FORK', 'JOIN', 'CHOICE', 'CONCURRENT', 'note', "
        "'acc_title', 'acc_descr', 'acc_descr_multiline_value', 'CLICK', 'classDef', 'style', 'class', 'direction_tb', "
        "'direction_bt', 'direction_rl', 'direction_lr', 'EDGE_STATE'";

    auto tok = tokenizeState(src);
    const auto& types = tok.types;
    const auto& texts = tok.texts;
    const auto& starts = tok.starts;
    size_t i = 0;

    auto fail = [&](const std::string& expected) -> void {
        int t = types[i];
        if (t == T::ERROR) {
            if (tok.hasError) {
                size_t offset = static_cast<size_t>(starts[i]);
                size_t end = trimEndStr(src).size();
                if (end > 0 && offset >= end) offset = end;
                int line = 1;
                size_t lineStart = 0;
                for (size_t p = src.find('\n'); p != std::string::npos && p < offset; p = src.find('\n', p + 1)) {
                    line++;
                    lineStart = p + 1;
                }
                int column = static_cast<int>(offset >= lineStart ? offset - lineStart + 1 : 1);
                throw PeleError(tok.error, "syntax", "state", line, column);
            }
            throw syntaxError("state", src, starts[i], "", true);
        }
        throw syntaxError("state", src, starts[i], "Expecting " + expected + ", got '" + std::string(TOKEN_NAMES[t]) + "'");
    };
    auto expect = [&](int type) -> std::string {
        if (types[i] != type) fail("'" + std::string(TOKEN_NAMES[type]) + "'");
        return texts[i++];
    };
    auto idStatement = [&]() -> StateStmtPtr {
        int t = types[i];
        if (t != T::ID && t != T::EDGE_STATE) fail("'ID', 'EDGE_STATE'");
        std::string id = trimStr(texts[i++]);
        auto st = std::make_shared<StateStmt>();
        st->id = id;
        st->type = "default";
        st->hasDescription = true;
        st->isDescArray = false;
        st->descStr = "";
        if (types[i] != T::STYLE_SEPARATOR) return st;
        i++;
        st->hasClasses = true;
        st->classes = {trimStr(expect(T::ID))};
        return st;
    };

    while (types[i] == T::NL) i++;
    if (types[i] != T::SD) fail("'NL', 'SD'");
    i++;

    std::vector<StmtPtr> doc;
    std::vector<std::vector<StmtPtr>> outer;
    std::vector<StateStmtPtr> openStates;
    auto add = [&](StmtPtr value) {
        if (value && value->kind == "string" && value->strVal == "nl") return;
        doc.push_back(std::move(value));
    };
    auto open = [&](const std::string& id, const std::string& description) {
        i++;
        auto st = std::make_shared<StateStmt>();
        st->id = id;
        st->type = "default";
        st->hasDescription = true;
        st->isDescArray = false;
        st->descStr = description;
        st->hasDoc = true;
        doc.push_back(makeStateStmtWrap(st));
        outer.push_back(std::move(doc));
        openStates.push_back(st);
        doc.clear();
    };

    for (;;) {
        int t = types[i];
        switch (t) {
            case T::NL:
                i++;
                break;
            case T::ID:
            case T::EDGE_STATE: {
                auto first = idStatement();
                if (types[i] == T::DESCR) {
                    first->hasDescription = true;
                    first->isDescArray = false;
                    first->descStr = db.trimColon(texts[i++]);
                    add(makeStateStmtWrap(first));
                } else if (types[i] == T::ARROW) {
                    i++;
                    auto second = idStatement();
                    auto rel = std::make_shared<Stmt>();
                    rel->kind = "relation";
                    rel->state1 = first;
                    rel->state2 = second;
                    if (types[i] == T::DESCR) {
                        rel->hasDescription = true;
                        rel->description = db.trimColon(texts[i++]);
                    }
                    add(rel);
                } else {
                    add(makeStateStmtWrap(first));
                }
                break;
            }
            case T::HIDE_EMPTY:
                add(makeStringStmt(texts[i++]));
                break;
            case T::scale:
                i++;
                expect(T::WIDTH);
                add(makeStringStmt(texts[i - 2]));
                break;
            case T::COMPOSIT_STATE:
                if (types[i + 1] == T::STRUCT_START) open(texts[i++], "");
                else add(makeStringStmt(texts[i++]));
                break;
            case T::STATE_DESCR: {
                std::string description = texts[i++];
                expect(T::AS);
                std::string id = expect(T::ID);
                if (types[i] == T::STRUCT_START) {
                    open(id, description);
                } else if (id.find(':') != std::string::npos) {
                    auto parts = splitStr(id, ':');
                    auto st = std::make_shared<StateStmt>();
                    st->id = parts[0];
                    st->type = "default";
                    st->hasDescription = true;
                    st->isDescArray = true;
                    st->descArr = {trimStr(description), parts.size() > 1 ? parts[1] : ""};
                    add(makeStateStmtWrap(st));
                } else {
                    auto st = std::make_shared<StateStmt>();
                    st->id = id;
                    st->type = "default";
                    st->hasDescription = true;
                    st->isDescArray = false;
                    st->descStr = trimStr(description);
                    add(makeStateStmtWrap(st));
                }
                break;
            }
            case T::FORK: {
                auto st = std::make_shared<StateStmt>();
                st->id = texts[i++];
                st->type = "fork";
                add(makeStateStmtWrap(st));
                break;
            }
            case T::JOIN: {
                auto st = std::make_shared<StateStmt>();
                st->id = texts[i++];
                st->type = "join";
                add(makeStateStmtWrap(st));
                break;
            }
            case T::CHOICE: {
                auto st = std::make_shared<StateStmt>();
                st->id = texts[i++];
                st->type = "choice";
                add(makeStateStmtWrap(st));
                break;
            }
            case T::CONCURRENT: {
                i++;
                auto st = std::make_shared<StateStmt>();
                st->id = db.getDividerId();
                st->type = "divider";
                add(makeStateStmtWrap(st));
                break;
            }
            case T::note: {
                std::string keyword = texts[i++];
                if (types[i] == T::left_of || types[i] == T::right_of) {
                    std::string position = trimStr(texts[i++]);
                    std::string id = trimStr(expect(T::ID));
                    auto st = std::make_shared<StateStmt>();
                    st->id = id;
                    st->hasNote = true;
                    st->note = {position, trimStr(expect(T::NOTE_TEXT))};
                    add(makeStateStmtWrap(st));
                } else {
                    if (types[i] != T::NOTE_TEXT) fail("'NOTE_TEXT', 'left_of', 'right_of'");
                    i++;
                    expect(T::AS);
                    expect(T::ID);
                    add(makeStringStmt(keyword));
                }
                break;
            }
            case T::direction_tb:
            case T::direction_bt:
            case T::direction_rl:
            case T::direction_lr: {
                std::string value = DIRECTIONS[t - T::direction_tb];
                i++;
                db.setDirection(value);
                auto s = std::make_shared<Stmt>();
                s->kind = "dir";
                s->value = value;
                add(s);
                break;
            }
            case T::acc_title: {
                i++;
                std::string value = trimStr(expect(T::acc_title_value));
                db.setAccTitle(value);
                add(makeStringStmt(value));
                break;
            }
            case T::acc_descr: {
                i++;
                std::string value = trimStr(expect(T::acc_descr_value));
                db.setAccDescription(value);
                add(makeStringStmt(value));
                break;
            }
            case T::acc_descr_multiline_value: {
                std::string value = trimStr(texts[i++]);
                db.setAccDescription(value);
                add(makeStringStmt(value));
                break;
            }
            case T::CLICK: {
                i++;
                auto id = idStatement();
                std::string url;
                std::string tooltip;
                if (types[i] == T::HREF) {
                    i++;
                    url = expect(T::STRING);
                } else {
                    if (types[i] != T::STRING) fail("'STRING', 'HREF'");
                    url = texts[i++];
                    tooltip = expect(T::STRING);
                }
                expect(T::NL);
                auto s = std::make_shared<Stmt>();
                s->kind = "click";
                s->clickState = id;
                s->url = url;
                s->tooltip = tooltip;
                add(s);
                break;
            }
            case T::classDef: {
                i++;
                std::string id = trimStr(expect(T::CLASSDEF_ID));
                auto s = std::make_shared<Stmt>();
                s->kind = "classDef";
                s->id = id;
                s->classes = trimStr(expect(T::CLASSDEF_STYLEOPTS));
                add(s);
                break;
            }
            case T::style: {
                i++;
                std::string id = trimStr(expect(T::STYLE_IDS));
                auto s = std::make_shared<Stmt>();
                s->kind = "style";
                s->id = id;
                s->styleClass = trimStr(expect(T::STYLEDEF_STYLEOPTS));
                add(s);
                break;
            }
            case T::class_tok: {
                i++;
                std::string id = trimStr(expect(T::CLASSENTITY_IDS));
                auto s = std::make_shared<Stmt>();
                s->kind = "applyClass";
                s->id = id;
                s->styleClass = trimStr(expect(T::STYLECLASS));
                add(s);
                break;
            }
            case T::STRUCT_STOP:
                if (outer.empty()) fail("'$end', " + STATEMENT);
                i++;
                openStates.back()->doc = std::move(doc);
                openStates.pop_back();
                doc = std::move(outer.back());
                outer.pop_back();
                break;
            case T::END:
                if (!outer.empty()) fail("'STRUCT_STOP', " + STATEMENT);
                db.setRootDoc(std::move(doc));
                return;
            default:
                fail(!outer.empty() ? ("'STRUCT_STOP', " + STATEMENT) : ("'$end', " + STATEMENT));
        }
    }
}

} // namespace detail_parse

// ============================================================================
// src/diagrams/state/render.ts
// ============================================================================

namespace detail_render {

constexpr double NODE_SEP = 40.0;
constexpr double EDGE_SEP = 16.0;
constexpr double RANK_SEP = 44.0;
constexpr double GROUP_PAD = 16.0;
constexpr double REGION_PAD = 10.0;
constexpr double PAD_X = 16.0;
constexpr double PAD_Y = 10.0;
constexpr double LOOP = 26.0;
constexpr double FOLD = 8.0;
constexpr double NOTE_WRAP = 320.0;
constexpr double CORNER = 10.0;
constexpr double BAR = 8.0;

inline const std::string SURFACE = " fill=\"var(--_s)\" stroke=\"var(--_b)\"";
inline const std::string RULE = " fill=\"none\" stroke=\"var(--_b)\"";

enum class K : int {
    State = 0,
    Described = 1,
    Start = 2,
    End = 3,
    Choice = 4,
    Fork = 5,
    Join = 6,
    Note = 7,
    Composite = 8,
    Region = 9,
    Hidden = 10,
};

inline K kindOfShape(const std::string& shape) {
    if (shape == "rectWithTitle") return K::Described;
    if (shape == "stateStart") return K::Start;
    if (shape == "stateEnd") return K::End;
    if (shape == "choice") return K::Choice;
    if (shape == "fork") return K::Fork;
    if (shape == "join") return K::Join;
    if (shape == "note") return K::Note;
    if (shape == "divider") return K::Hidden;
    return K::State;
}

inline const char* const CLASSES[] = {
    "pele-node pele-state",
    "pele-node pele-state",
    "pele-node pele-state-start",
    "pele-node pele-state-end",
    "pele-node pele-state-choice",
    "pele-node pele-state-fork",
    "pele-node pele-state-join",
    "pele-note",
    "pele-cluster pele-state-composite",
};

struct View {
    const StateNode* node = nullptr;
    K kind = K::State;
    ResolvedStyle style;
    bool hasStyle = false;
    Label label;
    std::vector<Label> lines;
    double w = 0.0;
    double h = 0.0;
    double titleH = 0.0;
};

inline bool isVertical(const std::string& dir) {
    return dir == "TB" || dir == "BT";
}

inline double numberOption(const Config& config, const std::string& key, double fallback) {
    const YamlValue* st = config.get("state");
    if (!st || !st->isMap()) return fallback;
    const YamlValue* v = st->get(key);
    if (v && v->isNumber() && v->nVal > 0.0) {
        return std::min(v->nVal, 2000.0);
    }
    return fallback;
}

inline std::string unquote(const std::string& text) {
    size_t from = 0;
    size_t to = text.size();
    while (from < to && text[from] == '"') from++;
    while (to > from && text[to - 1] == '"') to--;
    return text.substr(from, to - from);
}

inline double clampVal(double value, double minV, double maxV) {
    return value < minV ? minV : value > maxV ? maxV : value;
}

inline std::string rectSvg(double x, double y, double w, double h, const std::string& rest) {
    return "<rect x=\"" + num(x) + "\" y=\"" + num(y) + "\" width=\"" + num(w) + "\" height=\"" + num(h) + "\"" + rest + "/>";
}

inline CNode makeGroup(const std::string& dir, double padX, double padTop, double padBottom) {
    CNode c = cnode(0.0, 0.0);
    c.isGroup = true;
    c.dir = dir;
    c.padX = padX;
    c.padTop = padTop;
    c.padBottom = padBottom;
    return c;
}

inline std::string stripMermaidClasses(const std::string& s) {
    std::string out;
    bool first = true;
    size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && s[i] == ' ') i++;
        if (i >= s.size()) break;
        size_t j = i;
        while (j < s.size() && s[j] != ' ') j++;
        std::string tok = s.substr(i, j - i);
        i = j;
        if (tok == "statediagram-state" || tok == "statediagram-cluster-alt" || tok == "statediagram-cluster" || tok == "statediagram-note") {
            continue;
        }
        if (!first) out += " ";
        out += tok;
        first = false;
    }
    return out;
}

inline double titleX(const CNode& c, double width, const std::unordered_map<int, std::vector<double>>& crossings) {
    auto it = crossings.find(static_cast<int>(std::llround(c.y - c.h / 2.0)));
    if (it == crossings.end() || width == 0.0) return c.x;
    const auto& xs = it->second;
    double left = c.x - c.w / 2.0;
    double half = width / 2.0;
    double best = c.x;
    double bestGap = -1.0;
    double candidates[3] = {c.x, left + GROUP_PAD + half, left + c.w - GROUP_PAD - half};
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

inline Rendered draw(const StateDb& db, const Config& config, const RenderOptions& options, bool turned) {
    const auto& graph = db.graph;
    std::string curve;
    if (const YamlValue* fc = config.get("flowchart")) {
        if (fc->isMap()) {
            if (const YamlValue* cv = fc->get("curve")) {
                if (cv->isString()) curve = cv->sVal;
            }
        }
    }
    double size = options.fontSize > 0.0 ? options.fontSize : 16.0;
    double small = std::round(size * 0.875);
    const TextMeasurer& measurer = getMeasurer(options);
    double wrapWidth = numberOption(config, "wrappingWidth", 200.0);
    double pad = options.hasPadding ? options.padding : 8.0;
    const auto& icons = options.icons;
    std::string rootDir = turned ? "TB" : direction(db.direction());
    Label noLabel = layoutLabel("", false, measurer, size, 0.0);
    auto text = [&](const std::string& raw, bool hasRaw, double fontSize, double wrap, int style = 0) -> Label {
        if (!hasRaw) return layoutLabel("", true, measurer, fontSize, wrap, style);
        return layoutLabel(raw, true, measurer, fontSize, wrap, style);
    };

    std::unordered_map<std::string, int> index;
    std::vector<View> views;
    std::vector<CNode> cnodes;
    auto add = [&](const StateNode* node, K kind, const CNode* c) -> int {
        View v;
        v.node = node;
        v.kind = kind;
        v.label = noLabel;
        views.push_back(std::move(v));
        if (c) cnodes.push_back(*c);
        return static_cast<int>(views.size()) - 1;
    };
    for (const auto& node : graph.nodes) {
        if (node.shape == "noteGroup") continue;
        K kind = node.isGroup ? (node.shape == "divider" ? K::Region : K::Composite) : kindOfShape(node.shape);
        index[node.id] = add(&node, kind, nullptr);
    }
    int states = static_cast<int>(views.size());

    std::vector<int> logical;
    logical.reserve(states);
    for (int i = 0; i < states; i++) {
        int parent = -1;
        if (views[i].node->hasParentId) {
            auto it = index.find(views[i].node->parentId);
            if (it != index.end()) parent = it->second;
        }
        logical.push_back(parent != -1 && parent != i && views[parent].node->isGroup ? parent : -1);
    }
    {
        std::vector<std::vector<int>> kids(states);
        std::vector<int> queue;
        std::vector<uint8_t> placed(states, 0);
        for (int i = 0; i < states; i++) {
            if (logical[i] == -1) {
                placed[i] = 1;
                queue.push_back(i);
            } else {
                kids[logical[i]].push_back(i);
            }
        }
        size_t done = 0;
        auto settle = [&]() {
            for (; done < queue.size(); done++) {
                for (int kid : kids[queue[done]]) {
                    if (placed[kid]) continue;
                    placed[kid] = 1;
                    queue.push_back(kid);
                }
            }
        };
        settle();
        std::vector<int> seen(states, -1);
        for (int i = 0; i < states; i++) {
            if (placed[i]) continue;
            int ring = i;
            while (seen[ring] != i) {
                seen[ring] = i;
                ring = logical[ring];
            }
            logical[ring] = -1;
            placed[ring] = 1;
            queue.push_back(ring);
            settle();
        }
    }
    std::vector<int> owners(states, -1);
    for (int i = 0; i < states; i++) {
        if (!views[i].node->hasNoteFor) continue;
        auto it = index.find(views[i].node->noteFor);
        if (it == index.end()) continue;
        int owner = it->second;
        owners[i] = owner;
        logical[i] = logical[owner];
    }

    auto innerDir = [&](int composite) -> std::string {
        return direction(views[composite].node->dir);
    };
    auto across = [&](int i) -> bool {
        return isVertical(logical[i] == -1 ? rootDir : innerDir(logical[i]));
    };

    std::vector<int> degree(states, 0);
    std::vector<uint8_t> noted(states, 0);
    std::vector<double> appear(states, std::numeric_limits<double>::infinity());
    double appeared = 0.0;
    std::vector<int> counts(2 * states, 0);
    for (const auto& edge : graph.edges) {
        auto itSrc = index.find(edge.start);
        auto itDst = index.find(edge.end);
        if (itSrc == index.end() || itDst == index.end()) continue;
        int src = itSrc->second;
        int dst = itDst->second;
        if (edge.pattern == "dashed") {
            noted[views[src].kind == K::Note ? dst : src] = 1;
            continue;
        }
        degree[src] = std::max(degree[src], ++counts[2 * src]);
        degree[dst] = std::max(degree[dst], ++counts[2 * dst + 1]);
        if (!std::isfinite(appear[src])) appear[src] = appeared++;
        if (!std::isfinite(appear[dst])) appear[dst] = appeared++;
    }

    for (int i = 0; i < states; i++) {
        auto& view = views[i];
        const auto& node = *view.node;
        std::vector<std::string> styleDecls = node.cssCompiledStyles;
        if (!node.cssStyles.empty()) {
            styleDecls.insert(styleDecls.end(), node.cssStyles.begin(), node.cssStyles.end());
        }
        ResolvedStyle style = resolveStyle(styleDecls);
        int textStyle = (style.bold ? Style::Bold : 0) | (style.italic ? Style::Italic : 0);
        double fontSize = style.hasFontSize ? style.fontSize : size;
        view.style = style;
        view.hasStyle = true;
        bool hasC = false;
        CNode c;
        switch (view.kind) {
            case K::Composite:
                view.label = text(node.label, node.hasLabel, fontSize, 4000.0, textStyle);
                view.titleH = view.label.height > 0.0 ? view.label.height + 12.0 : 0.0;
                c = makeGroup(innerDir(i), GROUP_PAD, view.titleH + GROUP_PAD, GROUP_PAD);
                c.minW = view.label.width + 2.0 * GROUP_PAD;
                hasC = true;
                break;
            case K::Region:
                c = makeGroup(innerDir(i), REGION_PAD, REGION_PAD, REGION_PAD);
                hasC = true;
                break;
            case K::Start:
                view.w = view.h = 14.0;
                break;
            case K::End:
                view.w = view.h = 18.0;
                break;
            case K::Choice:
                view.w = view.h = 30.0;
                break;
            case K::Fork:
            case K::Join: {
                double length = clampVal(40.0 * degree[i] + 16.0, 72.0, 320.0);
                view.w = across(i) ? length : BAR;
                view.h = across(i) ? BAR : length;
                break;
            }
            case K::Hidden:
                break;
            case K::Note:
                view.label = text(node.label, node.hasLabel, small, std::max(wrapWidth, NOTE_WRAP));
                view.w = view.label.width + 24.0;
                view.h = view.label.height + 16.0;
                break;
            default: {
                view.label = text(node.label, node.hasLabel, fontSize, wrapWidth, textStyle);
                double width = view.label.width;
                double height = view.label.height + 2.0 * PAD_Y;
                if (view.kind == K::Described) {
                    height += 14.0;
                    for (const auto& line : node.description) {
                        Label lbl = text(line.empty() ? " " : line, true, small, wrapWidth, textStyle);
                        view.lines.push_back(lbl);
                        if (lbl.width > width) width = lbl.width;
                        height += lbl.height;
                    }
                }
                view.w = std::max(width + 2.0 * PAD_X, 56.0);
                view.h = height;
                break;
            }
        }
        if (!hasC) c = cnode(view.w, view.h);
        if (view.kind == K::State || view.kind == K::Described) {
            c.span = (across(i) ? view.w : view.h) - 24.0;
        }
        cnodes.push_back(c);
    }

    std::vector<CEdge> cedges;
    std::vector<Label> edgeLabels;
    std::vector<const StateEdge*> drawn;
    std::vector<int> loopCount(states, 0);
    std::vector<double> loopWidth(states, 0.0);
    std::vector<double> loopHeight(states, 0.0);
    std::unordered_map<int, int> anchors;
    auto encloses = [&](int composite, int item) -> bool {
        if (!cnodes[composite].isGroup) return false;
        for (int p = logical[item]; p != -1; p = logical[p]) {
            if (p == composite) return true;
        }
        return false;
    };
    auto anchor = [&](int composite, bool first) -> int {
        anchors[static_cast<int>(cedges.size())] = composite;
        appear.push_back(first ? -1.0 : std::numeric_limits<double>::infinity());
        CNode ac = cnode(0.0, 0.0, composite);
        return add(nullptr, K::Hidden, &ac);
    };
    for (const auto& edge : graph.edges) {
        auto itSrc = index.find(edge.start);
        auto itDst = index.find(edge.end);
        if (itSrc == index.end() || itDst == index.end()) continue;
        int src = itSrc->second;
        int dst = itDst->second;
        Label label = text(edge.label, true, small, wrapWidth);
        double labelW = label.width > 0.0 ? label.width + 12.0 : 0.0;
        if (src == dst) {
            loopCount[src]++;
            loopWidth[src] = std::max(loopWidth[src], labelW);
            loopHeight[src] += label.height;
        } else if (encloses(src, dst)) {
            src = anchor(src, true);
        } else if (encloses(dst, src)) {
            dst = anchor(dst, false);
        }
        drawn.push_back(&edge);
        edgeLabels.push_back(std::move(label));
        cedges.push_back({src, dst, 1, labelW, edgeLabels.back().height > 0.0 ? edgeLabels.back().height + 4.0 : 0.0, {}, 0.0, 0.0});
    }

    for (int i = 0; i < states; i++) {
        if (owners[i] != -1) continue;
        auto& item = cnodes[i];
        double room = 0.0;
        if (loopCount[i] > 0) {
            double reach = LOOP + (loopCount[i] - 1) * 8.0;
            room = across(i) ? (reach + loopWidth[i]) : (reach * 0.75 + 4.0 + loopHeight[i]);
            if (!item.isGroup) {
                if (across(i)) {
                    item.w += 2.0 * room;
                    item.h = std::max(item.h, loopHeight[i]);
                } else {
                    item.h += 2.0 * room;
                    item.w = std::max(item.w, loopWidth[i]);
                }
                room = 0.0;
            }
        }
        if (!noted[i] && room == 0.0) {
            item.parent = logical[i];
            continue;
        }
        CNode c = across(i) ? makeGroup("LR", room, 0.0, 0.0) : makeGroup("TB", 0.0, room, room);
        c.parent = logical[i];
        appear.push_back(appear[i]);
        int grpIdx = add(nullptr, K::Hidden, &c);
        cnodes[i].parent = grpIdx;
    }
    for (int i = 0; i < states; i++) {
        if (owners[i] != -1) cnodes[i].parent = cnodes[owners[i]].parent;
    }

    {
        std::vector<std::vector<int>> members(cnodes.size());
        std::vector<int> top;
        for (int i = 0; i < static_cast<int>(cnodes.size()); i++) {
            int p = cnodes[i].parent;
            if (p >= 0) members[p].push_back(i);
            else top.push_back(i);
        }
        auto initial = [&](int i) -> int { return views[i].kind == K::Start ? 0 : 1; };
        auto byFlow = [&](int a, int b) -> bool {
            int ia = initial(a);
            int ib = initial(b);
            if (ia != ib) return ia < ib;
            if (appear[a] != appear[b]) return appear[a] < appear[b];
            return a < b;
        };
        double seq = 0.0;
        std::sort(top.begin(), top.end(), byFlow);
        std::reverse(top.begin(), top.end());
        std::vector<int> pending = std::move(top);
        while (!pending.empty()) {
            int i = pending.back();
            pending.pop_back();
            cnodes[i].seq = seq++;
            auto& inside = members[i];
            std::sort(inside.begin(), inside.end(), byFlow);
            for (int k = static_cast<int>(inside.size()) - 1; k >= 0; k--) {
                pending.push_back(inside[k]);
            }
        }
    }

    LayeredOptions lopt;
    lopt.nodeSep = numberOption(config, "nodeSpacing", NODE_SEP);
    lopt.edgeSep = EDGE_SEP;
    lopt.rankSep = numberOption(config, "rankSpacing", RANK_SEP);
    lopt.portSep = 20.0;
    lopt.tracks = (curve == "step" || curve == "stepBefore" || curve == "stepAfter");
    lopt.straight = (curve == "linear");
    lopt.headRoom = markerTrim("arrow_point");
    auto layout = compoundLayout(cnodes, cedges, rootDir, lopt);

    double width = std::ceil(layout.width + 2.0 * pad);
    double height = std::ceil(layout.height + 2.0 * pad);
    std::vector<LinkInfo> links;

    std::unordered_map<int, std::vector<double>> crossings;
    for (const auto& e : cedges) {
        const auto& route = e.route;
        for (int k = 3; k < static_cast<int>(route.size()) - 3; k += 3) {
            int key = static_cast<int>(std::llround(route[k + 1]));
            crossings[key].push_back(route[k]);
        }
    }

    std::vector<std::vector<int>> children(cnodes.size());
    std::vector<int> order;
    for (int i = 0; i < static_cast<int>(cnodes.size()); i++) {
        int p = cnodes[i].parent;
        if (p >= 0) children[p].push_back(i);
        else order.push_back(i);
    }
    std::vector<int> roots = order;
    for (size_t q = 0; q < order.size(); q++) {
        for (int child : children[order[q]]) order.push_back(child);
    }

    std::string clusters;
    std::vector<Fill> fills;
    std::string dividers;
    std::string nodesOut;
    std::string notesOut;

    auto divide = [&](const std::vector<int>& items, double left, double top, double right, double bottom) {
        std::vector<int> regions;
        for (int i : items) if (views[i].kind == K::Region) regions.push_back(i);
        if (regions.size() < 2) return;
        auto lo = [&](int i, bool xAxis) -> double {
            return xAxis ? (cnodes[i].x - cnodes[i].w / 2.0) : (cnodes[i].y - cnodes[i].h / 2.0);
        };
        auto hi = [&](int i, bool xAxis) -> double {
            return xAxis ? (cnodes[i].x + cnodes[i].w / 2.0) : (cnodes[i].y + cnodes[i].h / 2.0);
        };
        for (bool xAxis : {true, false}) {
            std::vector<int> sorted = regions;
            std::sort(sorted.begin(), sorted.end(), [&](int a, int b) {
                double da = lo(a, xAxis);
                double db = lo(b, xAxis);
                if (da != db) return da < db;
                return a < b;
            });
            bool apart = true;
            for (size_t k = 1; k < sorted.size(); k++) {
                if (hi(sorted[k - 1], xAxis) > lo(sorted[k], xAxis) + 0.5) apart = false;
            }
            if (!apart) continue;
            for (size_t k = 1; k < sorted.size(); k++) {
                std::string at = num((hi(sorted[k - 1], xAxis) + lo(sorted[k], xAxis)) / 2.0 + pad);
                dividers += "<path class=\"pele-divider\" d=\"M" + (xAxis ? (at + "," + num(top) + "V" + num(bottom)) : (num(left) + "," + at + "H" + num(right))) + "\"/>";
            }
            return;
        }
        for (int i : regions) {
            const auto& c = cnodes[i];
            dividers += rectSvg(c.x - c.w / 2.0 + pad, c.y - c.h / 2.0 + pad, c.w, c.h, " class=\"pele-divider\" rx=\"" + num(CORNER) + "\"");
        }
    };
    divide(roots, pad / 2.0, pad / 2.0, width - pad / 2.0, height - pad / 2.0);

    for (int i : order) {
        const auto& view = views[i];
        const auto* node = view.node;
        if (!node || !view.hasStyle || static_cast<int>(view.kind) >= static_cast<int>(K::Region)) continue;
        const auto& style = view.style;
        const auto& c = cnodes[i];
        double x = c.x + pad;
        double y = c.y + pad;
        double w = view.w;
        double h = view.h;
        std::string ink = inkFill(style);
        std::string labelAttrs = " class=\"pele-label\"" + ink + style.text;
        std::string body;

        switch (view.kind) {
            case K::Composite: {
                double left = x - c.w / 2.0;
                double top = y - c.h / 2.0;
                if (paints(style.shape)) {
                    fills.push_back({{left, top, left + c.w, top + c.h}, style.shape});
                }
                body = rectSvg(left, top, c.w, c.h, " rx=\"" + num(CORNER) + "\" fill=\"var(--_a)\"" + (setsFill(style.shape) ? "" : " fill-opacity=\"0.5\"") + " stroke=\"var(--_b)\"" + style.shape);
                if (view.titleH > 0.0) {
                    body += "<path d=\"M" + num(left) + "," + num(top + view.titleH) + "H" + num(left + c.w) + "\"" + RULE + style.line + "/>" +
                            labelSvg(view.label, titleX(c, view.label.width, crossings) + pad, top + view.titleH / 2.0, " class=\"pele-cluster-label\"" + ink + style.text, icons);
                }
                divide(children[i], left, top + view.titleH, left + c.w, top + c.h);
                break;
            }
            case K::Start:
                body = "<circle r=\"7\" fill=\"var(--_l)\"" + style.shape + "/>";
                break;
            case K::End:
                body = "<circle r=\"8.5\" fill=\"var(--_bg)\" stroke=\"var(--_l)\"" + style.line + "/><circle r=\"5\" fill=\"var(--_l)\"" + style.shape + "/>";
                break;
            case K::Choice:
                body = "<polygon points=\"0," + num(-h / 2.0) + " " + num(w / 2.0) + ",0 0," + num(h / 2.0) + " " + num(-w / 2.0) + ",0\"" + SURFACE + style.shape + "/>";
                break;
            case K::Fork:
            case K::Join:
                body = rectSvg(-w / 2.0, -h / 2.0, w, h, " rx=\"2\" fill=\"var(--_l)\"" + style.shape);
                break;
            case K::Note: {
                std::string r = num(w / 2.0);
                std::string b = num(h / 2.0);
                std::string fold = num(w / 2.0 - FOLD);
                std::string corner = num(FOLD - h / 2.0);
                body = "<path d=\"M-" + r + ",-" + b + "H" + fold + "L" + r + "," + corner + "V" + b + "H-" + r + "Z\" fill=\"var(--_bg)\" stroke=\"var(--_b)\"/>" +
                       "<path d=\"M" + fold + ",-" + b + "V" + corner + "H" + r + "\"" + RULE + "/>" +
                       labelSvg(view.label, 0.0, 0.0, " class=\"pele-label\" font-size=\"" + num(small) + "\" fill=\"var(--_m)\"", icons);
                break;
            }
            default: {
                body = rectSvg(-w / 2.0, -h / 2.0, w, h, " rx=\"" + num(std::min(CORNER, h / 2.0)) + "\"" + SURFACE + style.shape);
                if (view.kind == K::State) {
                    body += labelSvg(view.label, 0.0, 0.0, labelAttrs, icons);
                    break;
                }
                double at = PAD_Y - h / 2.0 + view.label.height;
                body += "<path d=\"M" + num(-w / 2.0) + "," + num(at + 6.0) + "H" + num(w / 2.0) + "\"" + RULE + style.line + "/>" +
                        labelSvg(view.label, 0.0, at - view.label.height / 2.0, labelAttrs, icons);
                at += 14.0;
                for (const auto& line : view.lines) {
                    body += labelSvg(line, 0.0, at + line.height / 2.0, " class=\"pele-state-description\" font-size=\"" + num(small) + "\"" + ink + style.text, icons);
                    at += line.height;
                }
                break;
            }
        }

        auto itLink = db.links.find(node->id);
        if (itLink != db.links.end()) {
            std::string tooltip = decodeEntities(unquote(itLink->second.tooltip));
            if (!tooltip.empty()) body = "<title>" + esc(tooltip) + "</title>" + body;
            body = linked(body, unquote(itLink->second.url), node->id, links, options, " target=\"_blank\"");
        }
        std::string open = "<g class=\"" + std::string(CLASSES[static_cast<int>(view.kind)]) + classNames(stripMermaidClasses(node->cssClasses)) + "\" data-id=\"" + escText(node->id) + "\"";
        if (view.kind == K::Composite) clusters += open + ">" + body + "</g>";
        else if (view.kind == K::Note) notesOut += open + " transform=\"translate(" + num(x) + "," + num(y) + ")\">" + body + "</g>";
        else nodesOut += open + " transform=\"translate(" + num(x) + "," + num(y) + ")\">" + body + "</g>";
    }

    auto attach = [&](std::vector<double>& route, int at, int i) {
        const auto& c = cnodes[i];
        if (c.isGroup) return;
        const auto& view = views[i];
        bool bar = (view.kind == K::Fork || view.kind == K::Join);
        int other = (at == 0) ? 3 : (at - 3);
        for (int axis : {1, 0}) {
            double extent = axis ? c.h : c.w;
            double drawnExtent = axis ? view.h : view.w;
            double breadth = axis ? view.w : view.h;
            double center = (axis ? c.y : c.x) + pad;
            double cross = (axis ? c.x : c.y) + pad;
            double delta = route[at + axis] - center;
            if (std::abs(std::abs(delta) - extent / 2.0) >= 0.5) continue;
            double sgn = delta > 0.0 ? 1.0 : delta < 0.0 ? -1.0 : 0.0;
            route[at + axis] -= (sgn * (extent - drawnExtent)) / 2.0;
            if (bar && breadth > drawnExtent) {
                route[at + 1 - axis] = clampVal(route[other + 1 - axis], cross - breadth / 2.0 + 6.0, cross + breadth / 2.0 - 6.0);
            }
            return;
        }
    };

    SideReach reach;
    for (size_t i = 0; i < cedges.size(); i++) {
        const auto& ce = cedges[i];
        if (ce.src == ce.dst || ce.route.size() < 6 || drawn[i]->pattern == "dashed") continue;
        reach.add(ce.dst, endSide(ce.route, static_cast<int>(ce.route.size()) - 3, cnodes[ce.dst]), "arrow_point");
    }

    std::string casings;
    std::string edgesOut;
    std::string labelsOut;
    std::vector<int> loopSeen(states, 0);
    std::vector<double> loopText(states, 0.0);
    for (size_t i = 0; i < cedges.size(); i++) {
        auto& ce = cedges[i];
        const auto& edge = *drawn[i];
        const auto& label = edgeLabels[i];
        bool note = (edge.pattern == "dashed");
        std::string endType = note ? "none" : "arrow_point";
        EdgePath path;
        if (ce.src == ce.dst) {
            const auto& a = cnodes[ce.src];
            const auto& view = views[ce.src];
            int k = loopSeen[ce.src]++;
            bool side = across(ce.src);
            double w = a.isGroup ? a.w : view.w;
            double h = a.isGroup ? a.h : view.h;
            double out = (side ? w : h) / 2.0;
            double spread = std::max(std::min((side ? h : w) / 2.0 - 4.0, 8.0 + k * 6.0), 2.0);
            double rch = LOOP + k * 8.0;
            double hyp = std::hypot(rch, spread);
            double trim = markerTrim(endType) / (hyp == 0.0 ? 1.0 : hyp);
            auto px = [&](double dx, double dy) -> double { return a.x + pad + (side ? (out + dx) : -dy); };
            auto py = [&](double dx, double dy) -> double { return a.y + pad + (side ? dy : (out + dx)); };
            auto at = [&](double dx, double dy) -> std::string { return num(px(dx, dy)) + "," + num(py(dx, dy)); };
            path.d = "M" + at(0.0, -spread) + "C" + at(rch, -2.0 * spread) + " " + at(rch, 2.0 * spread) + " " + at(rch * trim, spread + spread * trim);
            path.ex = px(0.0, spread);
            path.ey = py(0.0, spread);
            path.edx = px(-rch, -spread) - px(0.0, 0.0);
            path.edy = py(-rch, -spread) - py(0.0, 0.0);
            double len = std::hypot(path.edx, path.edy);
            if (len == 0.0) len = 1.0;
            path.edx /= len;
            path.edy /= len;
            double furthest = LOOP + (loopCount[ce.src] - 1) * 8.0;
            double below = loopText[ce.src] + label.height / 2.0;
            loopText[ce.src] += label.height;
            ce.labelX = side ? (a.x + out + furthest + 4.0 + ce.labelW / 2.0) : a.x;
            ce.labelY = side ? (a.y - loopHeight[ce.src] / 2.0 + below) : (a.y + out + furthest * 0.75 + 4.0 + below);
        } else {
            std::vector<double> route = ce.route;
            double leaves = reach.of(ce.src, endSide(route, 0, cnodes[ce.src]));
            double arrives = reach.of(ce.dst, endSide(route, static_cast<int>(route.size()) - 3, cnodes[ce.dst]));
            for (size_t k = 0; k < route.size(); k += 3) {
                route[k] += pad;
                route[k + 1] += pad;
            }
            attach(route, 0, ce.src);
            attach(route, static_cast<int>(route.size()) - 3, ce.dst);
            auto itAnc = anchors.find(static_cast<int>(i));
            if (itAnc != anchors.end() && route.size() >= 6) {
                int composite = itAnc->second;
                const auto& g = cnodes[composite];
                bool inward = (views[ce.src].kind == K::Hidden);
                size_t at = inward ? 0 : (route.size() - 3);
                bool nearSide = ((g.dir == "TB" || g.dir == "LR") == inward);
                std::vector<double> point = isVertical(g.dir)
                    ? std::vector<double>{route[at], g.y + pad + (nearSide ? (views[composite].titleH - g.h / 2.0) : (g.h / 2.0)), 0.0}
                    : std::vector<double>{g.x + pad + (nearSide ? (-g.w / 2.0) : (g.w / 2.0)), route[at + 1], 1.0};
                if (inward) route.insert(route.begin(), point.begin(), point.end());
                else route.insert(route.end(), point.begin(), point.end());
            }
            path = routePath(route, curve, 0.0, markerTrim(endType), leaves, arrives);
        }
        auto over = overFills(path.d, {path.ex, path.ey}, fills, "", path.hasTokens ? &path.tokens : nullptr);
        if (over.onFill && over.value.empty()) {
            casings += edgeCasing(path, "none", endType);
        }
        std::string kind = note
            ? ("pele-note-edge\" stroke=\"" + (!over.value.empty() ? over.value : "var(--_b)") + "\" stroke-dasharray=\"3 4")
            : (!over.value.empty() ? ("pele-transition\" stroke=\"" + over.value) : "pele-transition");
        edgesOut +=
            "<g class=\"pele-edge " + kind + "\" data-id=\"" + escText(edge.id) + "\">" +
            "<path d=\"" + path.d + "\"/>" +
            marker(endType, path.ex, path.ey, path.edx, path.edy, !over.value.empty() ? over.value : "var(--_l)") +
            "</g>";
        if (label.width > 0.0) {
            double x = ce.labelX + pad;
            double y = ce.labelY + pad;
            labelsOut += edgeLabelSvg(edge.id, label, x, y, "", icons);
        }
    }

    std::string innerContent =
        (!clusters.empty() ? ("<g class=\"pele-clusters\">" + clusters + "</g>") : "") +
        (!dividers.empty() ? ("<g class=\"pele-dividers\" stroke-dasharray=\"4 4\"" + RULE + ">" + dividers + "</g>") : "") +
        casingsSvg(casings) +
        (!edgesOut.empty() ? ("<g class=\"pele-edges\" fill=\"none\" stroke=\"var(--_l)\" stroke-linecap=\"round\">" + edgesOut + "</g>") : "") +
        (!labelsOut.empty() ? ("<g class=\"pele-edge-labels\" font-size=\"" + num(small) + "\">" + labelsOut + "</g>") : "") +
        "<g class=\"pele-nodes\">" + nodesOut + notesOut + "</g>";

    auto titled = withTitle(db.title, measurer, size, pad, width, height, innerContent);
    std::string svg = svgDocument("state", titled.width, titled.height, size, options, db, titled.content);
    return {svg, titled.width, titled.height, links};
}

} // namespace detail_render

inline Rendered renderStateDiagram(const StateDb& db, const Config& config = {}, const RenderOptions& options = {}) {
    if (!runsAcross(db.direction())) return detail_render::draw(db, config, options, false);
    return turnToFit(options, [&](bool down) { return detail_render::draw(db, config, options, down); });
}

inline Rendered render(const std::string& text, const Config& config = {}, const RenderOptions& options = {}, const std::string& title = "") {
    StateDb db;
    db.title = title;
    detail_parse::parseState(text, db);
    return renderStateDiagram(db, config, options);
}

inline Rendered render(const std::string& text, const Config& config, const std::string& title, const RenderOptions& options = {}) {
    return render(text, config, options, title);
}

} // namespace StateDiagram

inline Rendered renderState(const std::string& text, const Config& config = {}, const RenderOptions& options = {}, const std::string& title = "") {
    return StateDiagram::render(text, config, options, title);
}

inline Rendered renderState(const std::string& text, const Config& config, const std::string& title, const RenderOptions& options = {}) {
    return StateDiagram::render(text, config, options, title);
}

} // namespace Pele
