#pragma once
#include "flowchart.h"

namespace Pele {
namespace Swimlane {

using Flowchart::CNode;
using Flowchart::CEdge;
using Flowchart::CompoundResult;
using Flowchart::LNode;
using Flowchart::LEdge;
using Flowchart::LayeredOptions;
using Flowchart::lnode;
using Flowchart::ledge;
using Flowchart::orient;
using Flowchart::rank;
using Flowchart::jsRound;
using Flowchart::FlowDb;
using Flowchart::FlowDbOptions;
using Flowchart::FlowGraph;
using Flowchart::FlowVariant;
using Flowchart::GraphNode;
using Flowchart::parseFlowchart;
using Flowchart::renderFlowchartModel;

// ============================================================================
// Port of src/layout/lanes.ts
// ============================================================================

namespace detail {

constexpr double LANE_HEAD = 34.0;
constexpr double LANE_PAD = 24.0;
constexpr int SWEEPS = 8;
constexpr int MAX_DUMMIES = 20000;
constexpr int MAX_HOPS = 10000;
constexpr int ROUTE_BUDGET = 2000000;
constexpr double NEAR_DIST = 4.0;
constexpr int ROUNDS = 3;
constexpr int RELAY = 8;
constexpr double TURN = 20.0;
constexpr double MARGIN = 20.0;
constexpr double LEG = 14.0;
constexpr double CLOSE = 3.0;

inline std::pair<double, double> border(const CNode& from, const CNode& to) noexcept {
    double dx = to.x - from.x;
    double dy = to.y - from.y;
    double hw = std::max(from.w / 2.0, 0.5);
    double hh = std::max(from.h / 2.0, 0.5);
    if (std::abs(dx) < hw && std::abs(dy) < hh) {
        double toSide = hw - std::abs(dx);
        double toEnd = hh - std::abs(dy);
        if (toSide < toEnd) {
            return {from.x + (dx < 0.0 ? -hw : hw), to.y};
        }
        return {to.x, from.y + (dy < 0.0 ? -hh : hh)};
    }
    double sx = dx != 0.0 ? hw / std::abs(dx) : std::numeric_limits<double>::infinity();
    double sy = dy != 0.0 ? hh / std::abs(dy) : std::numeric_limits<double>::infinity();
    double scale = std::min(sx, sy);
    return {from.x + dx * scale, from.y + dy * scale};
}

inline double jsSign(double x) noexcept {
    if (x > 0.0) return 1.0;
    if (x < 0.0) return -1.0;
    return 0.0;
}

} // namespace detail

inline CompoundResult laneLayout(
    std::vector<CNode>& nodes,
    std::vector<CEdge>& edges,
    const std::string& dir,
    const LayeredOptions& opt,
    double lanePad = detail::LANE_PAD
) {
    using namespace detail;
    int n = static_cast<int>(nodes.size());
    if (n == 0) return {0.0, 0.0};
    bool sideways = (dir == "LR" || dir == "RL");
    bool flipped = (dir == "BT" || dir == "RL");

    std::vector<int> lanes;
    for (int i = n - 1; i >= 0; i--) {
        if (nodes[i].parent < 0 && nodes[i].isGroup) lanes.push_back(i);
    }
    int laneCount = std::max(1, static_cast<int>(lanes.size()));
    std::vector<int> laneSlot(n, -1);
    for (size_t k = 0; k < lanes.size(); k++) laneSlot[lanes[k]] = static_cast<int>(k);
    std::vector<uint8_t> hasChild(n, 0);
    for (int i = 0; i < n; i++) {
        if (nodes[i].parent >= 0) hasChild[nodes[i].parent] = 1;
    }

    std::vector<double> lanePads(laneCount, lanePad);
    double endPad = lanePad;
    auto laneOf = [&](int i) -> int {
        int t = i;
        double room = lanePad;
        for (int hops = 0; nodes[t].parent >= 0 && hops < MAX_HOPS; hops++) {
            t = nodes[t].parent;
            if (laneSlot[t] < 0) room += std::max({nodes[t].padX, nodes[t].padTop, nodes[t].padBottom});
        }
        int lane = std::max(0, laneSlot[t]);
        if (room > lanePads[lane]) lanePads[lane] = room;
        if (room > endPad) endPad = room;
        return lane;
    };

    std::vector<int> itemOf(n, -1);
    std::vector<int> real;
    for (int i = 0; i < n; i++) {
        if (laneSlot[i] >= 0) continue;
        if (!nodes[i].isGroup || !hasChild[i]) {
            itemOf[i] = static_cast<int>(real.size());
            real.push_back(i);
        }
    }
    auto along = [&](const CNode& c) -> double { return sideways ? c.w : c.h; };
    auto cross = [&](const CNode& c) -> double { return sideways ? c.h : c.w; };

    std::vector<LNode> lnodes;
    lnodes.reserve(real.size());
    for (int i : real) lnodes.push_back(lnode(cross(nodes[i]), along(nodes[i])));
    std::vector<LEdge> ledges;
    std::vector<int> edgeOf;
    int step = 1;
    {
        Pele::FlatSet<int64_t> seen;
        int64_t rlen = static_cast<int64_t>(real.size());
        for (size_t ei = 0; ei < edges.size(); ei++) {
            const auto& e = edges[ei];
            int a = itemOf[e.src];
            int b = itemOf[e.dst];
            if (a < 0 || b < 0 || a == b) continue;
            int64_t key = static_cast<int64_t>(std::min(a, b)) * rlen + static_cast<int64_t>(std::max(a, b));
            if (e.labelW > 0.0 || seen.count(key)) step = 2;
            seen.insert(key);
            ledges.push_back(ledge(a, b, e.minlen, sideways ? e.labelH : e.labelW, sideways ? e.labelW : e.labelH));
            edgeOf.push_back(static_cast<int>(ei));
        }
    }
    orient(lnodes, ledges);
    rank(lnodes, ledges, step);

    int m = static_cast<int>(ledges.size());
    std::vector<int> itemLane(real.size(), 0);
    for (size_t k = 0; k < real.size(); k++) itemLane[k] = laneOf(real[k]);

    struct PlaceResult {
        double width = 0.0;
        double height = 0.0;
        std::vector<uint8_t> routed;
        std::vector<uint8_t> margins;
        int64_t work = 0;
    };

    auto place = [&](const std::vector<uint8_t>& plain, const std::vector<uint8_t>& margins) -> PlaceResult {
        std::vector<double> pad(laneCount, 0.0);
        for (int lane = 0; lane < laneCount; lane++) {
            pad[lane] = margins[lane] ? std::max(lanePads[lane], MARGIN) : lanePads[lane];
        }
        std::vector<uint8_t> used(laneCount, 0);
        int dummies = 0;
        std::vector<int> span(m, 0);
        for (int k = 0; k < m; k++) {
            span[k] = std::abs(lnodes[ledges[k].head].rank - lnodes[ledges[k].tail].rank);
            if (!plain[k]) dummies += span[k] - 1;
        }
        bool direct = dummies > MAX_DUMMIES;
        int total = static_cast<int>(real.size()) + (direct ? 0 : dummies);
        std::vector<int> RANK(total, 0);
        std::vector<int> LANE(total, 0);
        std::vector<double> SIZE(total, 0.0);
        std::vector<double> DEPTH(total, 0.0);
        std::vector<uint8_t> DUMMY(total, 0);
        int maxRank = 0;
        for (size_t k = 0; k < real.size(); k++) {
            RANK[k] = lnodes[k].rank;
            LANE[k] = itemLane[k];
            SIZE[k] = lnodes[k].w;
            DEPTH[k] = lnodes[k].h;
            if (RANK[k] > maxRank) maxRank = RANK[k];
        }
        std::vector<int> firstDummy(m, -1);
        std::vector<int> labelItem(m, -1);
        std::vector<int> links;
        {
            int next = static_cast<int>(real.size());
            for (int k = 0; k < m; k++) {
                const auto& e = ledges[k];
                int a = e.reversed ? e.head : e.tail;
                int b = e.reversed ? e.tail : e.head;
                if (direct || span[k] < 2 || plain[k]) {
                    links.push_back(a);
                    links.push_back(b);
                    continue;
                }
                int ra = RANK[a];
                int rb = RANK[b];
                int labelRank = -1;
                if (e.labelW > 0.0) {
                    labelRank = ra + ((rb - ra) >> 1);
                    if (step == 2 && (labelRank & 1) == 0) labelRank--;
                }
                firstDummy[k] = next;
                int prev = a;
                for (int r = ra + 1; r < rb; r++) {
                    int d = next++;
                    RANK[d] = r;
                    LANE[d] = LANE[a] + static_cast<int>(jsRound((static_cast<double>(LANE[b] - LANE[a]) * (r - ra)) / (rb - ra)));
                    DUMMY[d] = 1;
                    if (r == labelRank) {
                        SIZE[d] = e.labelW;
                        DEPTH[d] = e.labelH;
                        labelItem[k] = d;
                    }
                    links.push_back(prev);
                    links.push_back(d);
                    prev = d;
                }
                links.push_back(prev);
                links.push_back(b);
            }
        }

        std::vector<int> upStart(total + 1, 0);
        std::vector<int> downStart(total + 1, 0);
        for (size_t k = 0; k < links.size(); k += 2) {
            downStart[links[k] + 1]++;
            upStart[links[k + 1] + 1]++;
        }
        for (int i = 0; i < total; i++) {
            upStart[i + 1] += upStart[i];
            downStart[i + 1] += downStart[i];
        }
        std::vector<int> up(links.size() / 2, 0);
        std::vector<int> down(links.size() / 2, 0);
        {
            std::vector<int> uf(upStart.begin(), upStart.begin() + total);
            std::vector<int> df(downStart.begin(), downStart.begin() + total);
            for (size_t k = 0; k < links.size(); k += 2) {
                down[df[links[k]]++] = links[k + 1];
                up[uf[links[k + 1]]++] = links[k];
            }
        }

        std::vector<std::vector<int>> rows(maxRank + 1);
        for (int i = 0; i < total; i++) rows[RANK[i]].push_back(i);
        std::vector<double> pos(total, 0.0);
        std::vector<double> key(total, 0.0);
        auto settle = [&](std::vector<int>& row) {
            std::sort(row.begin(), row.end(), [&](int a, int b) {
                if (LANE[a] != LANE[b]) return LANE[a] < LANE[b];
                if (key[a] != key[b]) return key[a] < key[b];
                return a < b;
            });
            for (size_t k = 0; k < row.size(); k++) pos[row[k]] = static_cast<double>(k);
        };
        for (auto& row : rows) {
            for (size_t k = 0; k < row.size(); k++) key[row[k]] = static_cast<double>(k);
            settle(row);
        }
        for (int sweep = 0; sweep < SWEEPS && maxRank > 0; sweep++) {
            bool downward = (sweep & 1) == 0;
            for (int s = 1; s <= maxRank; s++) {
                auto& row = rows[downward ? s : maxRank - s];
                const auto& start = downward ? upStart : downStart;
                const auto& list = downward ? up : down;
                for (int i : row) {
                    double sum = 0.0;
                    int count = start[i + 1] - start[i];
                    for (int k = start[i]; k < start[i + 1]; k++) sum += pos[list[k]];
                    key[i] = count > 0 ? sum / count : pos[i];
                }
                settle(row);
            }
        }

        auto gap = [&](int a, int b) -> double { return (DUMMY[a] || DUMMY[b]) ? opt.edgeSep : opt.nodeSep; };
        auto eachRun = [&](const std::vector<int>& row, auto&& visit) {
            size_t from = 0;
            while (from < row.size()) {
                size_t to = from;
                while (to + 1 < row.size() && LANE[row[to + 1]] == LANE[row[from]]) to++;
                visit(row, from, to);
                from = to + 1;
            }
        };
        auto runSize = [&](const std::vector<int>& row, size_t from, size_t to) -> double {
            double size = SIZE[row[from]];
            for (size_t k = from; k < to; k++) size += gap(row[k], row[k + 1]) + SIZE[row[k + 1]];
            return size;
        };

        std::vector<double> breadth(laneCount, 0.0);
        double head = !lanes.empty() ? LANE_HEAD : 0.0;
        for (size_t k = 0; k < lanes.size(); k++) {
            const auto& lane = nodes[lanes[k]];
            breadth[k] = std::max(0.0, lane.minW - 2.0 * pad[k]);
            if (lane.padTop > head) head = lane.padTop;
        }
        for (const auto& row : rows) {
            eachRun(row, [&](const std::vector<int>& r, size_t from, size_t to) {
                double size = runSize(r, from, to);
                if (size > breadth[LANE[r[from]]]) breadth[LANE[r[from]]] = size;
            });
        }
        std::vector<double> laneStart(laneCount + 1, 0.0);
        for (int k = 0; k < laneCount; k++) laneStart[k + 1] = laneStart[k] + breadth[k] + 2.0 * pad[k];

        std::vector<double> C(total, 0.0);
        std::vector<double> want(total, 0.0);
        auto center = [&](const std::vector<int>& row, size_t from, size_t to) {
            int lane = LANE[row[from]];
            double at = (laneStart[lane] + pad[lane] + (laneStart[lane + 1] - pad[lane]) - runSize(row, from, to)) / 2.0;
            for (size_t k = from; k <= to; k++) {
                C[row[k]] = at + SIZE[row[k]] / 2.0;
                if (k < to) at += SIZE[row[k]] + gap(row[k], row[k + 1]);
            }
        };
        auto align = [&](const std::vector<int>& row, size_t from, size_t to) {
            int lane = LANE[row[from]];
            double flr = laneStart[lane] + pad[lane];
            for (size_t k = from; k <= to; k++) {
                int i = row[k];
                C[i] = std::max(want[i], flr + SIZE[i] / 2.0);
                if (k < to) flr = C[i] + SIZE[i] / 2.0 + gap(i, row[k + 1]);
            }
            double cl = laneStart[lane + 1] - pad[lane];
            for (size_t k = to + 1; k-- > from;) {
                int i = row[k];
                C[i] = std::min(C[i], cl - SIZE[i] / 2.0);
                if (k > from) cl = C[i] - SIZE[i] / 2.0 - gap(row[k - 1], i);
            }
        };
        for (const auto& row : rows) eachRun(row, center);
        for (int pass = 0; pass < 4 && maxRank > 0; pass++) {
            bool downward = (pass & 1) == 0;
            for (int s = 1; s <= maxRank; s++) {
                const auto& row = rows[downward ? s : maxRank - s];
                const auto& start = downward ? upStart : downStart;
                const auto& list = downward ? up : down;
                for (int i : row) {
                    double sum = 0.0;
                    int count = 0;
                    for (int k = start[i]; k < start[i + 1]; k++) {
                        if (LANE[list[k]] != LANE[i]) continue;
                        sum += C[list[k]];
                        count++;
                    }
                    want[i] = count > 0 ? sum / count : C[i];
                }
                eachRun(row, align);
            }
        }

        std::vector<double> bandStart(maxRank + 1, 0.0);
        std::vector<double> bandEnd(maxRank + 1, 0.0);
        std::vector<double> sep(maxRank + 1, opt.rankSep / step);
        bool anyPlain = false;
        for (uint8_t on : plain) if (on == 1) anyPlain = true;
        if (anyPlain) {
            double full = opt.rankSep / step;
            std::fill(sep.begin(), sep.end(), full / CLOSE);
            for (int k = 0; k < m; k++) {
                const auto& e = ledges[k];
                int a = e.reversed ? e.head : e.tail;
                int b = e.reversed ? e.tail : e.head;
                if (plain[k]) {
                    if (e.labelH <= 0.0 || span[k] > step) continue;
                    double room = std::min(full, (e.labelH + 2.0 * TURN - std::min(DEPTH[a], DEPTH[b]) / 2.0) / span[k]);
                    for (int r = RANK[a]; r < RANK[b]; r++) if (room > sep[r]) sep[r] = room;
                    continue;
                }
                int d0 = firstDummy[k];
                if (d0 < 0 || span[k] <= step * e.minlen) {
                    for (int r = RANK[a]; r < RANK[b]; r++) sep[r] = full;
                    continue;
                }
                for (int d = d0 - 1; d < d0 + span[k] - 1; d++) {
                    int u = d < d0 ? a : d;
                    int v = d + 1 < d0 + span[k] - 1 ? d + 1 : b;
                    if (LANE[u] != LANE[v] || std::abs(C[u] - C[v]) >= 0.5) sep[RANK[u]] = full;
                }
            }
        }

        double at = head + endPad;
        for (int r = 0; r <= maxRank; r++) {
            double depth = 0.0;
            for (int i : rows[r]) if (DEPTH[i] > depth) depth = DEPTH[i];
            bandStart[r] = at;
            bandEnd[r] = at + depth;
            at += depth + (r == maxRank ? 0.0 : sep[r]);
        }
        double length = at + endPad;
        double breadthTotal = laneStart[laneCount];
        auto flip = [&](double a) -> double { return flipped ? head + length - a : a; };
        auto px = [&](double c, double a) -> double { return sideways ? flip(a) : c; };
        auto py = [&](double c, double a) -> double { return sideways ? c : flip(a); };
        double axis = sideways ? 1.0 : 0.0;

        for (size_t k = 0; k < real.size(); k++) {
            auto& node = nodes[real[k]];
            double a = (bandStart[RANK[k]] + bandEnd[RANK[k]]) / 2.0;
            node.x = px(C[k], a);
            node.y = py(C[k], a);
        }
        for (size_t k = 0; k < lanes.size(); k++) {
            auto& lane = nodes[lanes[k]];
            double c = (laneStart[k] + laneStart[k + 1]) / 2.0;
            double size = laneStart[k + 1] - laneStart[k];
            lane.x = sideways ? length / 2.0 : c;
            lane.y = sideways ? c : length / 2.0;
            lane.w = sideways ? length : size;
            lane.h = sideways ? size : length;
            lane.padTop = head;
        }

        std::vector<int> depthOf(n, 0);
        std::vector<int> inner;
        for (int i = 0; i < n; i++) {
            if (!nodes[i].isGroup || laneSlot[i] >= 0 || itemOf[i] >= 0) continue;
            int d = 0;
            for (int p = nodes[i].parent; p >= 0 && d < MAX_HOPS; p = nodes[p].parent) d++;
            depthOf[i] = d;
            inner.push_back(i);
        }
        std::sort(inner.begin(), inner.end(), [&](int a, int b) {
            if (depthOf[b] != depthOf[a]) return depthOf[b] < depthOf[a];
            return a < b;
        });
        std::vector<double> box(n * 4);
        for (int i = 0; i < n; i++) {
            box[i * 4] = std::numeric_limits<double>::infinity();
            box[i * 4 + 1] = std::numeric_limits<double>::infinity();
            box[i * 4 + 2] = -std::numeric_limits<double>::infinity();
            box[i * 4 + 3] = -std::numeric_limits<double>::infinity();
        }
        auto grow = [&](int g, const CNode& c) {
            if (c.x - c.w / 2.0 < box[g * 4]) box[g * 4] = c.x - c.w / 2.0;
            if (c.y - c.h / 2.0 < box[g * 4 + 1]) box[g * 4 + 1] = c.y - c.h / 2.0;
            if (c.x + c.w / 2.0 > box[g * 4 + 2]) box[g * 4 + 2] = c.x + c.w / 2.0;
            if (c.y + c.h / 2.0 > box[g * 4 + 3]) box[g * 4 + 3] = c.y + c.h / 2.0;
        };
        for (int i : real) {
            if (nodes[i].parent >= 0 && laneSlot[nodes[i].parent] < 0) grow(nodes[i].parent, nodes[i]);
        }
        for (int g : inner) {
            auto& c = nodes[g];
            if (std::isinf(box[g * 4])) continue;
            double w = std::max(box[g * 4 + 2] - box[g * 4] + 2.0 * c.padX, c.minW);
            c.x = (box[g * 4] + box[g * 4 + 2]) / 2.0;
            c.w = w;
            c.y = (box[g * 4 + 1] - c.padTop + box[g * 4 + 3] + c.padBottom) / 2.0;
            c.h = box[g * 4 + 3] - box[g * 4 + 1] + c.padTop + c.padBottom;
            if (c.parent >= 0 && laneSlot[c.parent] < 0) grow(c.parent, c);
        }

        for (auto& e : edges) e.route.clear();
        for (int k = 0; k < m; k++) {
            const auto& e = ledges[k];
            auto& out = edges[edgeOf[k]];
            int a = e.reversed ? e.head : e.tail;
            int b = e.reversed ? e.tail : e.head;
            std::vector<double> pts = {C[a], bandEnd[RANK[a]] - (bandEnd[RANK[a]] - bandStart[RANK[a]] - DEPTH[a]) / 2.0};
            int d0 = firstDummy[k];
            if (d0 >= 0) {
                for (int d = d0; d < d0 + span[k] - 1; d++) {
                    pts.push_back(C[d]);
                    pts.push_back(bandStart[RANK[d]]);
                    if (bandEnd[RANK[d]] > bandStart[RANK[d]]) {
                        pts.push_back(C[d]);
                        pts.push_back(bandEnd[RANK[d]]);
                    }
                }
            }
            pts.push_back(C[b]);
            pts.push_back(bandStart[RANK[b]] + (bandEnd[RANK[b]] - bandStart[RANK[b]] - DEPTH[b]) / 2.0);
            std::vector<double> route;
            route.reserve((pts.size() / 2) * 3);
            for (size_t p = 0; p < pts.size(); p += 2) {
                route.push_back(px(pts[p], pts[p + 1]));
                route.push_back(py(pts[p], pts[p + 1]));
                route.push_back(axis);
            }
            if (e.reversed) {
                for (int i = 0, j = static_cast<int>(route.size()) - 3; i < j; i += 3, j -= 3) {
                    std::swap(route[i], route[j]);
                    std::swap(route[i + 1], route[j + 1]);
                }
            }
            out.route = std::move(route);
            int label = labelItem[k];
            if (label >= 0) {
                double mid = (bandStart[RANK[label]] + bandEnd[RANK[label]]) / 2.0;
                out.labelX = px(C[label], mid);
                out.labelY = py(C[label], mid);
            } else {
                out.labelX = (out.route[0] + out.route[out.route.size() - 3]) / 2.0;
                out.labelY = (out.route[1] + out.route[out.route.size() - 2]) / 2.0;
            }
        }

        for (auto& e : edges) {
            if (!e.route.empty() || e.src == e.dst) continue;
            const auto& a = nodes[e.src];
            const auto& b = nodes[e.dst];
            auto [ax, ay] = border(a, b);
            auto [bx, by] = border(b, a);
            double flat = std::abs(bx - ax) > std::abs(by - ay) ? 1.0 : 0.0;
            e.route = {ax, ay, flat, bx, by, flat};
            e.labelX = (ax + bx) / 2.0;
            e.labelY = (ay + by) / 2.0;
        }

        std::vector<uint8_t> routed(m, 0);
        int64_t routePoints = 0;
        for (const auto& edge : edges) routePoints += static_cast<int64_t>(edge.route.size() / 3);
        int64_t work = static_cast<int64_t>(m) * (static_cast<int64_t>(real.size()) + static_cast<int64_t>(edges.size()) + routePoints);
        if (work <= ROUTE_BUDGET) {
            auto amid = [&](int i) -> double { return (bandStart[RANK[i]] + bandEnd[RANK[i]]) / 2.0; };
            std::vector<std::pair<double, double>> labels(edges.size());
            for (size_t i = 0; i < edges.size(); i++) {
                labels[i] = {sideways ? edges[i].labelY : edges[i].labelX, flip(sideways ? edges[i].labelX : edges[i].labelY)};
            }
            auto overlaps = [](double c0, double a0, double c1, double a1, double lc, double la, double cw, double ah) -> bool {
                return c1 > lc - cw / 2.0 && c0 < lc + cw / 2.0 && a1 > la - ah / 2.0 && a0 < la + ah / 2.0;
            };
            auto clear = [&](double c0, double a0, double c1, double a1, int src, int dst, int ei) -> bool {
                for (int i = 0; i < static_cast<int>(real.size()); i++) {
                    if (i != src && i != dst && overlaps(c0, a0, c1, a1, C[i], amid(i), SIZE[i] + 8.0, DEPTH[i] + 8.0)) return false;
                }
                for (int i = 0; i < static_cast<int>(edges.size()); i++) {
                    const auto& e = edges[i];
                    if (i != ei && e.labelW > 0.0 && e.src != e.dst && overlaps(c0, a0, c1, a1, labels[i].first, labels[i].second, sideways ? e.labelH : e.labelW, sideways ? e.labelW : e.labelH)) return false;
                }
                return true;
            };
            auto crossings = [&](double x0, double y0, double x1, double y1, int ei) -> int {
                int count = 0;
                for (int i = 0; i < static_cast<int>(edges.size()); i++) {
                    if (i == ei) continue;
                    const auto& route = edges[i].route;
                    for (size_t j = 3; j < route.size(); j += 3) {
                        if (std::max(route[j - 3], route[j]) > x0 && std::min(route[j - 3], route[j]) < x1 &&
                            std::max(route[j - 2], route[j + 1]) > y0 && std::min(route[j - 2], route[j + 1]) < y1) {
                            count++;
                            break;
                        }
                    }
                }
                return count;
            };
            auto meets = [&](double x, double y, int ei) -> int {
                int count = 0;
                for (int i = 0; i < static_cast<int>(edges.size()); i++) {
                    const auto& route = edges[i].route;
                    if (i == ei || route.empty()) continue;
                    size_t last = route.size() - 3;
                    if ((std::abs(route[0] - x) < 1.0 && std::abs(route[1] - y) < 1.0) ||
                        (std::abs(route[last] - x) < 1.0 && std::abs(route[last + 1] - y) < 1.0)) {
                        count++;
                    }
                }
                return count;
            };
            auto retraces = [&](double x0, double y0, double x1, double y1, int ei) -> bool {
                bool upright = std::abs(x1 - x0) < 0.5;
                double lo = upright ? std::min(y0, y1) : std::min(x0, x1);
                double hi = upright ? std::max(y0, y1) : std::max(x0, x1);
                double line = upright ? x0 : y0;
                for (int i = 0; i < static_cast<int>(edges.size()); i++) {
                    const auto& other = edges[i];
                    if (i == ei || other.src == edges[ei].src || other.dst == edges[ei].dst) continue;
                    const auto& route = other.route;
                    for (size_t j = 3; j < route.size(); j += 3) {
                        double u0 = route[j - 3 + (upright ? 0 : 1)];
                        double u1 = route[j + (upright ? 0 : 1)];
                        if (std::abs(u0 - line) >= NEAR_DIST || std::abs(u1 - line) >= NEAR_DIST) continue;
                        double v0 = route[j - 3 + (upright ? 1 : 0)];
                        double v1 = route[j + (upright ? 1 : 0)];
                        if (std::min(hi, std::max(v0, v1)) - std::max(lo, std::min(v0, v1)) > 1.0) return true;
                    }
                }
                return false;
            };

            for (int k = 0; k < m; k++) {
                const auto& e = ledges[k];
                int ei = edgeOf[k];
                auto& out = edges[ei];
                int a = e.tail;
                int b = e.head;
                if (LANE[a] == LANE[b] || std::abs(C[b] - C[a]) < 1.0) continue;
                double ac = C[a], bc = C[b], aa = amid(a), ba = amid(b);
                double dc = jsSign(bc - ac), da = jsSign(ba - aa);

                auto attempt = [&](const std::vector<double>& pts, double margin) -> bool {
                    for (size_t p = 2; p < pts.size(); p += 2) {
                        if (!clear(std::min(pts[p - 2], pts[p]) - margin, std::min(pts[p - 1], pts[p + 1]) - margin,
                                   std::max(pts[p - 2], pts[p]) + margin, std::max(pts[p - 1], pts[p + 1]) + margin, a, b, ei)) return false;
                        if (retraces(px(pts[p - 2], pts[p - 1]), py(pts[p - 2], pts[p - 1]), px(pts[p], pts[p + 1]), py(pts[p], pts[p + 1]), ei)) return false;
                    }
                    double lc = 0.0, la = 0.0, longest = -1.0;
                    if (out.labelW > 0.0) {
                        double cw = e.labelW, ah = e.labelH;
                        for (size_t p = 2; p < pts.size(); p += 2) {
                            double cc = (pts[p - 2] + pts[p]) / 2.0;
                            double ca = (pts[p - 1] + pts[p + 1]) / 2.0;
                            double across = std::abs(pts[p] - pts[p - 2]);
                            double along = std::abs(pts[p + 1] - pts[p - 1]);
                            double run = across + along;
                            if (run <= longest || (across > 0.0 ? across < cw + 8.0 : along < ah + 8.0)) continue;
                            if (cc - cw / 2.0 < 0.0 || cc + cw / 2.0 > breadthTotal || ca - ah / 2.0 < 0.0 || ca + ah / 2.0 > length) continue;
                            if (!clear(cc - cw / 2.0, ca - ah / 2.0, cc + cw / 2.0, ca + ah / 2.0, -1, -1, ei)) continue;
                            double lx = px(cc, ca), ly = py(cc, ca);
                            if (crossings(lx - out.labelW / 2.0, ly - out.labelH / 2.0, lx + out.labelW / 2.0, ly + out.labelH / 2.0, ei) > 0) continue;
                            lc = cc;
                            la = ca;
                            longest = run;
                        }
                        if (longest < 0.0) return false;
                    }
                    std::vector<double> way = {pts[0], pts[1]};
                    for (size_t p = 2; p < pts.size(); p += 2) {
                        double dx = pts[p] - pts[p - 2];
                        double dy = pts[p + 1] - pts[p - 1];
                        double run = std::abs(dx) + std::abs(dy);
                        double first = p > 2 ? TURN : 0.0;
                        double last = p + 2 < pts.size() ? TURN : 0.0;
                        if (run > first + last + 1.0) {
                            if (first > 0.0) {
                                way.push_back(pts[p - 2] + (dx / run) * first);
                                way.push_back(pts[p - 1] + (dy / run) * first);
                            }
                            if (last > 0.0) {
                                way.push_back(pts[p] - (dx / run) * last);
                                way.push_back(pts[p + 1] - (dy / run) * last);
                            }
                        }
                        way.push_back(pts[p]);
                        way.push_back(pts[p + 1]);
                    }
                    out.route.clear();
                    for (size_t p = 0; p < way.size(); p += 2) {
                        bool crosswise = p == 0 ? way[0] != way[2] : way[p] != way[p - 2];
                        out.route.push_back(px(way[p], way[p + 1]));
                        out.route.push_back(py(way[p], way[p + 1]));
                        out.route.push_back(crosswise ? 1.0 - axis : axis);
                    }
                    if (out.labelW > 0.0) {
                        out.labelX = px(lc, la);
                        out.labelY = py(lc, la);
                        labels[ei] = {lc, la};
                    }
                    routed[k] = 1;
                    return true;
                };

                if (attempt({ac + dc * SIZE[a] / 2.0, aa, bc, aa, bc, ba - da * DEPTH[b] / 2.0}, 2.0)) continue;
                if (attempt({ac, aa + da * DEPTH[a] / 2.0, ac, ba, bc - dc * SIZE[b] / 2.0, ba}, 2.0)) continue;
                if (work > ROUTE_BUDGET / RELAY) continue;
                struct AroundWay {
                    std::vector<double> pts;
                    double cost = 0.0;
                    int lane = 0;
                };
                std::vector<AroundWay> around;
                std::array<int, 2> lanePair = LANE[a] < LANE[b] ? std::array<int, 2>{LANE[b], LANE[a]} : std::array<int, 2>{LANE[a], LANE[b]};
                for (int lane : lanePair) {
                    double inset = std::max(pad[lane] - LEG, std::min(pad[lane] / 2.0, MARGIN - LEG));
                    for (double ch : {laneStart[lane] + inset, laneStart[lane + 1] - inset}) {
                        if (std::abs(ch - ac) < SIZE[a] / 2.0 + 4.0 || std::abs(ch - bc) < SIZE[b] / 2.0 + 4.0) continue;
                        std::vector<double> pts = {
                            ac + jsSign(ch - ac) * SIZE[a] / 2.0, aa,
                            ch, aa,
                            ch, ba,
                            bc + jsSign(ch - bc) * SIZE[b] / 2.0, ba
                        };
                        double cost = static_cast<double>(meets(px(pts[0], pts[1]), py(pts[0], pts[1]), ei) + meets(px(pts[6], pts[7]), py(pts[6], pts[7]), ei)) * 1e6;
                        for (size_t p = 2; p < pts.size(); p += 2) {
                            double x0 = px(pts[p - 2], pts[p - 1]);
                            double y0 = py(pts[p - 2], pts[p - 1]);
                            double x1 = px(pts[p], pts[p + 1]);
                            double y1 = py(pts[p], pts[p + 1]);
                            cost += static_cast<double>(crossings(std::min(x0, x1), std::min(y0, y1), std::max(x0, x1), std::max(y0, y1), ei)) * 1e6 + std::abs(x1 - x0) + std::abs(y1 - y0);
                        }
                        around.push_back({std::move(pts), cost, lane});
                    }
                }
                std::stable_sort(around.begin(), around.end(), [](const AroundWay& p, const AroundWay& q) {
                    return p.cost < q.cost;
                });
                for (const auto& way : around) {
                    if (!attempt(way.pts, 1.0)) continue;
                    used[way.lane] = 1;
                    break;
                }
            }
        }

        return {sideways ? length : breadthTotal, sideways ? breadthTotal : length, std::move(routed), std::move(used), work};
    };

    std::vector<uint8_t> plain(m, 0);
    std::vector<uint8_t> margins(laneCount, 0);
    PlaceResult placed = place(plain, margins);
    for (int round = 0; round < ROUNDS && placed.work <= ROUTE_BUDGET / RELAY; round++) {
        std::vector<uint8_t> next(m, 0);
        if (round == 0) {
            next = placed.routed;
        } else {
            for (int k = 0; k < m; k++) next[k] = plain[k] & placed.routed[k];
        }
        if (next == plain && placed.margins == margins) break;
        plain = std::move(next);
        margins = placed.margins;
        placed = place(plain, margins);
    }
    bool lost = false;
    for (int k = 0; k < m; k++) {
        if (plain[k] && !placed.routed[k]) {
            lost = true;
            break;
        }
    }
    if (lost) {
        placed = place(std::vector<uint8_t>(m, 0), std::vector<uint8_t>(laneCount, 0));
    }
    return {placed.width, placed.height};
}

// ============================================================================
// Port of src/diagrams/swimlane/index.ts
// ============================================================================

inline void prepareSwimlane(FlowGraph& graph) noexcept {
    const std::string DEFAULT_LANE = "__swimlane_default__";
    bool loose = false;
    for (auto& node : graph.nodes) {
        if (node.isGroup) node.dir.clear();
        else if (!node.hasParentId) loose = true;
    }
    if (!loose) return;
    for (auto& node : graph.nodes) {
        if (!node.isGroup && !node.hasParentId) {
            node.parentId = DEFAULT_LANE;
            node.hasParentId = true;
        }
    }
    GraphNode def;
    def.id = DEFAULT_LANE;
    def.hasLabel = false;
    def.labelType = "text";
    def.shape = "rect";
    def.isGroup = true;
    def.hasParentId = false;
    def.cssClasses = "";
    graph.nodes.push_back(std::move(def));
}

inline std::string drawLane(
    const CNode& c,
    const Label& label,
    const ResolvedStyle& style,
    const std::string& classes,
    const std::string& id,
    const std::string& dir,
    const IconResolver& icons
) {
    double left = c.x - c.w / 2.0;
    double top = c.y - c.h / 2.0;
    bool sideways = (dir == "LR" || dir == "RL");
    double cx = sideways ? left + c.padTop / 2.0 : c.x;
    double cy = sideways ? c.y : top + c.padTop / 2.0;
    std::string text = labelSvg(label,  cx,  cy, Pele::cat({" class=\"pele-cluster-label\"", inkFill(style, "var(--_m)"), style.text}),  icons);
    return Pele::cat({"<g class=\"pele-cluster pele-lane", classes, "\" data-id=\"", id, "\">", "<rect x=\"", num(left), "\" y=\"", num(top), "\" width=\"", num(c.w), "\" height=\"", num(c.h), "\" fill=\"none\" stroke=\"var(--_b)\"/>", "<rect x=\"", num(left), "\" y=\"", num(top), "\" width=\"", num(sideways ? c.padTop : c.w), "\" height=\"", num(sideways ? c.h : c.padTop), "\" fill=\"var(--_a)\" stroke=\"var(--_b)\"", style.shape, "/>", (sideways && !text.empty() ? Pele::cat({"<g transform=\"rotate(-90 ", num(cx), " ", num(cy), ")\">", text, "</g>"}) : text), "</g>"});
}

inline std::array<double, 4> laneBand(const CNode& c, const std::string& dir) noexcept {
    double left = c.x - c.w / 2.0;
    double top = c.y - c.h / 2.0;
    if (dir == "LR" || dir == "RL") {
        return {left, top, left + c.padTop, top + c.h};
    }
    return {left, top, left + c.w, top + c.padTop};
}

inline const FlowVariant& swimlaneVariant() noexcept {
    static const FlowVariant v{
        "swimlane",
        prepareSwimlane,
        [](std::vector<CNode>& nodes, std::vector<CEdge>& edges, const std::string& dir, const LayeredOptions& options, bool tight) {
            return laneLayout(nodes, edges, dir, options, tight ? 12.0 : detail::LANE_PAD);
        },
        drawLane,
        laneBand
    };
    return v;
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
        const YamlValue* sec = config.isMap() ? config.get("swimlane") : nullptr;
        bool fixed = (sec && sec->isMap() && sec->get("useMaxWidth") && sec->get("useMaxWidth")->isBool() && !sec->get("useMaxWidth")->bVal);
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
            return renderFlowchartModel(db, config, eff, &swimlaneVariant());
        });
    });
    for (auto& link : res.links) link.id = decodeEntities(link.id);
    return res;
}

inline Rendered render(const std::string& source, const Config& config, const RenderOptions& options, const std::string& title = "") {
    return render(source, config, title, options);
}

} // namespace Swimlane

inline Rendered renderSwimlane(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options = {}) {
    return Swimlane::render(source, config, title, options);
}

inline Rendered renderSwimlane(const std::string& source, const Config& config, const RenderOptions& options, const std::string& title = "") {
    return Swimlane::render(source, config, title, options);
}

inline Rendered renderSwimlane(const std::string& source, const RenderOptions& options = {}) {
    auto pre = preprocess(source);
    std::string parsed = Pele::cat({encodeEntities(pre.text), "\n"});
    return Swimlane::render(parsed, pre.config, pre.title, options);
}

} // namespace Pele
