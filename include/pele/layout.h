#pragma once
#include "svg.h"
#include <numeric>

namespace Pele {

// ============================================================================
// Port of src/layout/layered.ts
// ============================================================================

enum class LKind : uint8_t {
  Node = 0,
  Dummy = 1,
  Label = 2,
  StartPort = 3,
  EndPort = 4,
};

enum class LEnd : uint8_t {
  None = 0,
  Leaves = 1,
  Arrives = 2,
};

struct LNode {
  double w = 0;
  double h = 0;
  double span = 0;
  LKind kind = LKind::Node;
  double key = std::numeric_limits<double>::quiet_NaN();
  int pin = 0;
  double hint = std::numeric_limits<double>::quiet_NaN();
  double x = 0;
  double y = 0;
  int rank = 0;
};

struct LEdge {
  int tail = 0;
  int head = 0;
  int minlen = 1;
  double weight = 1;
  double tailDx = 0;
  double headDx = 0;
  bool tailFixed = false;
  bool headFixed = false;
  double labelW = 0;
  double labelH = 0;
  std::vector<double> points;
  double labelX = 0;
  double labelY = 0;
  bool reversed = false;
};

struct LayeredGroups {
  std::vector<int> of;
  std::vector<int> parent;
};

struct LayeredOptions {
  double nodeSep = 36;
  double rankSep = 48;
  double edgeSep = 16;
  double portSep = 16;
  double headRoom = 0;
  bool tracks = false;
  bool apart = false;
  bool straight = false;
  bool orderOnly = false;
  std::optional<LayeredGroups> groups;
};

struct LayeredResult {
  double width = 0;
  double height = 0;
  int crossings = 0;
  int declared = 0;
};

namespace detail_layered {

constexpr int MAX_SWEEPS = 24;
constexpr int MAX_DUMMIES = 20000;
constexpr double MIN_SLOPE = 0.25;
constexpr double MAX_GAP = 1.6;
constexpr double TRACK_MARGIN = 14;
constexpr double TRACK_PITCH = 10;
constexpr double TRACK_SPREAD = 16;
constexpr double TRACK_CLEAR = 6;
constexpr double LEVEL_MARGIN = 18;
constexpr double LEVEL_PITCH = 10;
constexpr double LEVEL_WIDE = 2;
constexpr double SPREAD_PAD = 10;
constexpr double MAX_END_STEP = 28;
constexpr double SETTLE_PASSES = 6;
constexpr double SNAP_CLOSE = 1.5;

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
  bool hasGRP = false;
  std::vector<int> GRP;
  std::vector<int> groupParent;
};

inline double halfRoom(const Graph& g, const LayeredOptions& opt, int v) noexcept {
  return g.W[v] / 2.0 + (g.KIND[v] == static_cast<uint8_t>(LKind::Node) ? opt.nodeSep : opt.edgeSep) / 2.0;
}

inline void sortByBary(std::vector<int>& arr, int len, const std::vector<double>& bary, const std::vector<int>& pos, bool biasRight, std::vector<int>& tmp) noexcept {
  if (len <= 16) {
    for (int i = 1; i < len; i++) {
      int v = arr[i];
      double bv = bary[v];
      int pv = pos[v];
      int j = i - 1;
      while (j >= 0) {
        int u = arr[j];
        double d = bary[u] - bv;
        if (d > 0 || (d == 0 && (biasRight ? pos[u] < pv : pos[u] > pv))) {
          arr[j + 1] = u;
          j--;
        } else {
          break;
        }
      }
      arr[j + 1] = v;
    }
    return;
  }
  for (int width = 1; width < len; width <<= 1) {
    for (int lo = 0; lo < len; lo += width << 1) {
      int mid = std::min(lo + width, len);
      int hi = std::min(lo + (width << 1), len);
      int i = lo, j = mid, k = lo;
      while (i < mid && j < hi) {
        int a = arr[i], b = arr[j];
        double d = bary[a] - bary[b];
        if (d < 0 || (d == 0 && (biasRight ? pos[a] > pos[b] : pos[a] < pos[b]))) {
          tmp[k++] = a;
          i++;
        } else {
          tmp[k++] = b;
          j++;
        }
      }
      while (i < mid) tmp[k++] = arr[i++];
      while (j < hi) tmp[k++] = arr[j++];
    }
    for (int i = 0; i < len; i++) arr[i] = tmp[i];
  }
}

} // namespace detail_layered

inline void orient(std::vector<LNode>& nodes, std::vector<LEdge>& edges) noexcept {
  int n = static_cast<int>(nodes.size());
  int m = static_cast<int>(edges.size());
  std::vector<int> head(n, -1);
  std::vector<int> next(m, 0);
  for (int ei = m - 1; ei >= 0; ei--) {
    int u = edges[ei].tail;
    next[ei] = head[u];
    head[u] = ei;
  }
  std::vector<uint8_t> state(n, 0);
  std::vector<int> stackNode;
  std::vector<int> stackEdge;
  for (int s = 0; s < n; s++) {
    if (state[s] != 0) continue;
    state[s] = 1;
    stackNode.push_back(s);
    stackEdge.push_back(head[s]);
    while (!stackNode.empty()) {
      int top = static_cast<int>(stackNode.size()) - 1;
      int ei = stackEdge[top];
      if (ei == -1) {
        state[stackNode[top]] = 2;
        stackNode.pop_back();
        stackEdge.pop_back();
        continue;
      }
      stackEdge[top] = next[ei];
      int v = edges[ei].head;
      if (state[v] == 1) {
        int t = edges[ei].tail;
        edges[ei].tail = edges[ei].head;
        edges[ei].head = t;
        edges[ei].reversed = true;
      } else if (state[v] == 0) {
        state[v] = 1;
        stackNode.push_back(v);
        stackEdge.push_back(head[v]);
      }
    }
  }
}

inline void rank(std::vector<LNode>& nodes, std::vector<LEdge>& edges, int step = 1) noexcept {
  int n = static_cast<int>(nodes.size());
  int m = static_cast<int>(edges.size());
  if (n == 0) return;

  std::vector<int> inDeg(n, 0);
  std::vector<int> outHead(n, -1);
  std::vector<int> outNext(m, 0);
  std::vector<int> inHead(n, -1);
  std::vector<int> inNext(m, 0);
  for (int ei = 0; ei < m; ei++) {
    int u = edges[ei].tail;
    int v = edges[ei].head;
    inDeg[v]++;
    outNext[ei] = outHead[u];
    outHead[u] = ei;
    inNext[ei] = inHead[v];
    inHead[v] = ei;
  }

  std::vector<int> topo(n, 0);
  int qTail = 0;
  for (int i = 0; i < n; i++) {
    nodes[i].rank = 0;
    if (inDeg[i] == 0) topo[qTail++] = i;
  }
  for (int qHead = 0; qHead < qTail; qHead++) {
    int u = topo[qHead];
    int ru = nodes[u].rank;
    for (int ei = outHead[u]; ei != -1; ei = outNext[ei]) {
      int v = edges[ei].head;
      int need = ru + edges[ei].minlen * step;
      if (need > nodes[v].rank) nodes[v].rank = need;
      if (--inDeg[v] == 0) topo[qTail++] = v;
    }
  }
  if (qTail < n) {
    std::vector<uint8_t> seen(n, 0);
    for (int i = 0; i < qTail; i++) seen[topo[i]] = 1;
    for (int i = 0; i < n; i++) if (!seen[i]) topo[qTail++] = i;
  }

  for (int pass = 0; pass < 4; pass++) {
    bool moved = false;
    for (int k = n - 1; k >= 0; k--) {
      int u = topo[k];
      if (nodes[u].kind == LKind::StartPort || outHead[u] == -1) continue;
      int pull = std::numeric_limits<int>::max();
      for (int ei = outHead[u]; ei != -1; ei = outNext[ei]) {
        int r = nodes[edges[ei].head].rank - edges[ei].minlen * step;
        if (r < pull) pull = r;
      }
      if (inHead[u] == -1) {
        if (pull > nodes[u].rank) {
          nodes[u].rank = pull;
          moved = true;
        }
        continue;
      }
      int push = 0;
      for (int ei = inHead[u]; ei != -1; ei = inNext[ei]) {
        int r = nodes[edges[ei].tail].rank + edges[ei].minlen * step;
        if (r > push) push = r;
      }
      if (pull <= push) continue;
      double wIn = 0, wOut = 0;
      for (int ei = inHead[u]; ei != -1; ei = inNext[ei]) wIn += edges[ei].weight;
      for (int ei = outHead[u]; ei != -1; ei = outNext[ei]) wOut += edges[ei].weight;
      int target = wOut > wIn ? pull : push;
      if (target != nodes[u].rank) {
        nodes[u].rank = target;
        moved = true;
      }
    }
    for (int k = 0; k < n; k++) {
      int u = topo[k];
      if (inHead[u] == -1 || outHead[u] == -1) continue;
      int push = 0;
      for (int ei = inHead[u]; ei != -1; ei = inNext[ei]) {
        int r = nodes[edges[ei].tail].rank + edges[ei].minlen * step;
        if (r > push) push = r;
      }
      int pull = std::numeric_limits<int>::max();
      for (int ei = outHead[u]; ei != -1; ei = outNext[ei]) {
        int r = nodes[edges[ei].head].rank - edges[ei].minlen * step;
        if (r < pull) pull = r;
      }
      double wIn = 0, wOut = 0;
      for (int ei = inHead[u]; ei != -1; ei = inNext[ei]) wIn += edges[ei].weight;
      for (int ei = outHead[u]; ei != -1; ei = outNext[ei]) wOut += edges[ei].weight;
      int target = wIn >= wOut ? push : pull;
      if (target >= push && target <= pull && target != nodes[u].rank) {
        nodes[u].rank = target;
        moved = true;
      }
    }
    if (!moved) break;
  }

  int minRank = std::numeric_limits<int>::max();
  for (int i = 0; i < n; i++) if (nodes[i].rank < minRank) minRank = nodes[i].rank;
  if (minRank != 0) for (int i = 0; i < n; i++) nodes[i].rank -= minRank;

  int maxRank = 0;
  for (int i = 0; i < n; i++) if (nodes[i].rank > maxRank) maxRank = nodes[i].rank;
  for (int i = 0; i < n; i++) {
    if (nodes[i].kind == LKind::StartPort) nodes[i].rank = 0;
    else if (nodes[i].kind == LKind::EndPort) nodes[i].rank = maxRank;
  }
}

namespace detail_layered {

inline std::vector<uint8_t> emptyRanks(const Graph& g) noexcept {
  std::vector<uint8_t> out;
  for (int r = 1; r < g.maxRank; r++) {
    bool allDummy = true;
    for (int i = g.layerStart[r]; i < g.layerStart[r + 1]; i++) {
      if (g.KIND[g.order[i]] != static_cast<uint8_t>(LKind::Dummy)) {
        allDummy = false;
        break;
      }
    }
    if (allDummy) {
      if (out.empty()) out.assign(g.maxRank + 1, 0);
      out[r] = 1;
    }
  }
  return out;
}

inline int freeEnds(const Graph& g, const std::vector<uint8_t>& EFK, const std::vector<uint8_t>& ETK) noexcept {
  int total = 0;
  for (int i = 0; i < g.real; i++) {
    if (g.KIND[i] != static_cast<uint8_t>(LKind::Node)) continue;
    for (int k = g.succStart[i]; k < g.succStart[i + 1]; k++) if (EFK[g.succE[k]] == static_cast<uint8_t>(LEnd::Leaves)) total++;
    for (int k = g.predStart[i]; k < g.predStart[i + 1]; k++) if (ETK[g.predE[k]] == static_cast<uint8_t>(LEnd::Arrives)) total++;
  }
  return total;
}

inline void alignLabels(
  Graph& g,
  const std::vector<LNode>& nodes,
  std::vector<double>& xs,
  int free,
  const LayeredOptions& opt,
  const std::vector<uint8_t>& empty
) {
  if (free > 120) return;
  auto& real = g.real;
  auto& maxRank = g.maxRank;
  auto& KIND = g.KIND;
  auto& RANK = g.RANK;
  auto& EF = g.EF;
  auto& ET = g.ET;
  auto& EFD = g.EFD;
  auto& ETD = g.ETD;
  auto& succStart = g.succStart;
  auto& succE = g.succE;
  auto& predStart = g.predStart;
  auto& predE = g.predE;
  auto& layerStart = g.layerStart;
  auto& order = g.order;
  auto& pos = g.pos;

  int labels = 0;
  for (int d = real; d < g.n; d++) if (KIND[d] == static_cast<uint8_t>(LKind::Label)) labels++;
  if (labels == 0) return;

  std::vector<double> margin(g.n, 0);
  for (int i = 0; i < g.n; i++) margin[i] = halfRoom(g, opt, i);

  auto trace = [&](int start, int side) -> int {
    int v = start;
    while (v >= real) {
      int seg = side < 0 ? predE[predStart[v]] : succE[succStart[v]];
      int next = side < 0 ? EF[seg] : ET[seg];
      if (!empty.empty() && empty[RANK[v]] && next < real) v = next;
      else break;
    }
    return v;
  };
  auto endSpan = [&](int v) -> double { return v < real && KIND[v] == static_cast<uint8_t>(LKind::Node) ? nodes[v].span / 2.0 : 0.0; };
  auto setTrace = [&](int d, int side, double x) {
    int v = d;
    while (true) {
      int seg = side < 0 ? predE[predStart[v]] : succE[succStart[v]];
      int next = side < 0 ? EF[seg] : ET[seg];
      if (!empty.empty() && empty[RANK[v]] && next < real) {
        xs[v] = x;
        v = next;
      } else {
        if (side < 0) EFD[seg] = x - xs[next];
        else ETD[seg] = x - xs[next];
        return;
      }
    }
  };

  for (int pass = 0; pass < 4; pass++) {
    bool moved = false;
    for (int r = 1; r < maxRank; r++) {
      int s = layerStart[r];
      int e = layerStart[r + 1];
      for (int i = s; i < e; i++) {
        int d = order[i];
        if (KIND[d] != static_cast<uint8_t>(LKind::Label)) continue;
        int u = trace(EF[predE[predStart[d]]], -1);
        int w = trace(ET[succE[succStart[d]]], 1);
        double uSpan = endSpan(u);
        double wSpan = endSpan(w);
        if (uSpan == 0 && wSpan == 0) continue;
        double lo = std::max(xs[u] - uSpan, xs[w] - wSpan);
        double hi = std::min(xs[u] + uSpan, xs[w] + wSpan);
        if (lo > hi) continue;
        if (i > s) lo = std::max(lo, xs[order[i - 1]] + margin[order[i - 1]] + margin[d]);
        if (i + 1 < e) hi = std::min(hi, xs[order[i + 1]] - margin[order[i + 1]] - margin[d]);
        if (lo > hi) continue;

        auto bound = [&](int v, int start, int dir, int side) -> void {
          int r2 = RANK[v];
          if (r2 == r) return;
          int s2 = layerStart[r2];
          int e2 = layerStart[r2 + 1];
          int p = pos[v];
          for (int k = start; k < (side < 0 ? predStart[v + 1] : succStart[v + 1]); k++) {
            int seg = side < 0 ? predE[k] : succE[k];
            int far = trace(side < 0 ? EF[seg] : ET[seg], side);
            if (far == d) continue;
            double reach = xs[far] + endSpan(far) * dir;
            if (dir < 0) lo = std::max(lo, reach);
            else hi = std::min(hi, reach);
          }
          int adj = p + dir;
          if (adj >= 0 && adj < e2 - s2) {
            int nb = order[s2 + adj];
            for (int k = side < 0 ? predStart[nb] : succStart[nb]; k < (side < 0 ? predStart[nb + 1] : succStart[nb + 1]); k++) {
              int seg = side < 0 ? predE[k] : succE[k];
              int far = trace(side < 0 ? EF[seg] : ET[seg], side);
              if ((pos[far] - pos[d]) * dir >= 0) continue;
              double nbX = xs[nb] + (side < 0 ? ETD[seg] : EFD[seg]);
              if (dir < 0) lo = std::max(lo, nbX);
              else hi = std::min(hi, nbX);
            }
          }
        };
        if (uSpan > 0) {
          bound(u, predStart[u], -1, 1);
          bound(u, succStart[u], 1, 1);
        }
        if (wSpan > 0) {
          bound(w, predStart[w], -1, -1);
          bound(w, succStart[w], 1, -1);
        }
        if (lo > hi) continue;
        double want = (xs[u] + xs[w]) / 2.0;
        double x = std::max(lo, std::min(hi, want));
        if (std::abs(x - xs[d]) > 0.5) moved = true;
        xs[d] = x;
        setTrace(d, -1, x);
        setTrace(d, 1, x);
      }
    }
    if (!moved) break;
  }
}

inline void spread(Graph& g, const std::vector<LNode>& nodes, const std::vector<uint8_t>& EFK, const std::vector<uint8_t>& ETK) noexcept {
  auto& real = g.real;
  auto& RANK = g.RANK;
  auto& EF = g.EF;
  auto& ET = g.ET;
  auto& EFD = g.EFD;
  auto& ETD = g.ETD;
  auto& succStart = g.succStart;
  auto& succE = g.succE;
  auto& predStart = g.predStart;
  auto& predE = g.predE;
  auto& pos = g.pos;

  struct Entry {
    int seg;
    bool from;
    int across;
    int farRank;
  };
  std::vector<Entry> entries;

  for (int i = 0; i < real; i++) {
    double span = nodes[i].span;
    if (span <= 0 || nodes[i].kind != LKind::Node) continue;
    for (int side : {-1, 1}) {
      uint8_t want = side < 0 ? static_cast<uint8_t>(LEnd::Arrives) : static_cast<uint8_t>(LEnd::Leaves);
      entries.clear();
      for (int k = succStart[i]; k < succStart[i + 1]; k++) {
        int seg = succE[k];
        if (EFK[seg] != want) continue;
        int v = ET[seg];
        while (v >= real) v = ET[succE[succStart[v]]];
        entries.push_back({seg, true, pos[ET[seg]], RANK[v]});
      }
      for (int k = predStart[i]; k < predStart[i + 1]; k++) {
        int seg = predE[k];
        if (ETK[seg] != want) continue;
        int v = EF[seg];
        while (v >= real) v = EF[predE[predStart[v]]];
        entries.push_back({seg, false, pos[EF[seg]], RANK[v]});
      }
      int deg = static_cast<int>(entries.size());
      if (deg <= 1) continue;
      std::stable_sort(entries.begin(), entries.end(), [&](const Entry& a, const Entry& b) {
        if (a.across != b.across) return a.across < b.across;
        int fr = side > 0 ? a.farRank - b.farRank : b.farRank - a.farRank;
        if (fr != 0) return fr < 0;
        return a.seg < b.seg;
      });
      double step = std::min((span - 2.0 * SPREAD_PAD) / (deg - 1), MAX_END_STEP);
      if (step <= 0) continue;
      double start = -((deg - 1) * step) / 2.0;
      for (int k = 0; k < deg; k++) {
        double dx = start + k * step;
        if (entries[k].from) EFD[entries[k].seg] = dx;
        else ETD[entries[k].seg] = dx;
      }
      g.offsets = true;
    }
  }
}

inline void settle(
  Graph& g,
  const std::vector<LNode>& nodes,
  std::vector<double>& xs,
  const std::vector<uint8_t>& EFK,
  const std::vector<uint8_t>& ETK,
  int free,
  const std::vector<uint8_t>& empty
) {
  auto& real = g.real;
  auto& RANK = g.RANK;
  auto& EF = g.EF;
  auto& ET = g.ET;
  auto& EFD = g.EFD;
  auto& ETD = g.ETD;
  auto& succStart = g.succStart;
  auto& succE = g.succE;
  auto& predStart = g.predStart;
  auto& predE = g.predE;
  if (free == 0) return;

  std::vector<int> seg(free, 0);
  std::vector<uint8_t> isFrom(free, 0);
  std::vector<int> other(free, 0);
  std::vector<double> dx(free, 0);
  std::vector<int> sideStart;
  std::vector<double> sideHalf;
  std::vector<int> sideNode;
  int p = 0;
  for (int i = 0; i < real; i++) {
    double span = nodes[i].span;
    if (span <= 0 || nodes[i].kind != LKind::Node) continue;
    for (uint8_t want : {static_cast<uint8_t>(LEnd::Arrives), static_cast<uint8_t>(LEnd::Leaves)}) {
      int s0 = p;
      for (int k = succStart[i]; k < succStart[i + 1]; k++) {
        int e = succE[k];
        if (EFK[e] != want) continue;
        seg[p] = e;
        isFrom[p] = 1;
        dx[p] = EFD[e];
        p++;
      }
      for (int k = predStart[i]; k < predStart[i + 1]; k++) {
        int e = predE[k];
        if (ETK[e] != want) continue;
        seg[p] = e;
        isFrom[p] = 0;
        dx[p] = ETD[e];
        p++;
      }
      if (p == s0) continue;
      for (int a = s0 + 1; a < p; a++) {
        int ts = seg[a];
        uint8_t tf = isFrom[a];
        double td = dx[a];
        double tx = tf ? xs[ET[ts]] + ETD[ts] : xs[EF[ts]] + EFD[ts];
        int b = a - 1;
        while (b >= s0) {
          double bx = isFrom[b] ? xs[ET[seg[b]]] + ETD[seg[b]] : xs[EF[seg[b]]] + EFD[seg[b]];
          if (td < dx[b] || (td == dx[b] && (tx < bx || (tx == bx && ts < seg[b])))) {
            seg[b + 1] = seg[b];
            isFrom[b + 1] = isFrom[b];
            dx[b + 1] = dx[b];
            b--;
          } else break;
        }
        seg[b + 1] = ts;
        isFrom[b + 1] = tf;
        dx[b + 1] = td;
      }
      sideStart.push_back(s0);
      sideHalf.push_back(span / 2.0);
      sideNode.push_back(i);
    }
  }
  sideStart.push_back(p);

  std::vector<int> atFrom(EF.size(), -1);
  std::vector<int> atTo(EF.size(), -1);
  for (int k = 0; k < p; k++) {
    if (isFrom[k]) atFrom[seg[k]] = k;
    else atTo[seg[k]] = k;
  }
  for (int k = 0; k < p; k++) {
    int e = seg[k];
    int v = isFrom[k] ? ET[e] : EF[e];
    while (!empty.empty() && v >= real && empty[RANK[v]]) {
      e = isFrom[k] ? succE[succStart[v]] : predE[predStart[v]];
      v = isFrom[k] ? ET[e] : EF[e];
    }
    int o = isFrom[k] ? atTo[e] : atFrom[e];
    other[k] = o >= 0 ? o : -(v + 1);
  }

  std::vector<double> target(p, 0);
  std::vector<double> bL(p, 0);
  std::vector<double> bR(p, 0);
  std::vector<int> bN(p, 0);
  int sides = static_cast<int>(sideHalf.size());
  for (int pass = 0; pass < SETTLE_PASSES; pass++) {
    for (int s = 0; s < sides; s++) {
      int s0 = sideStart[s];
      int s1 = sideStart[s + 1];
      int count = s1 - s0;
      double half = sideHalf[s];
      double nx = xs[sideNode[s]];
      for (int k = s0; k < s1; k++) {
        int o = other[k];
        if (o >= 0) {
          target[k] = xs[isFrom[o] ? EF[seg[o]] : ET[seg[o]]] + dx[o] - nx;
        } else {
          int v = -o - 1;
          double odx = v < real ? (isFrom[k] ? ETD[seg[k]] : EFD[seg[k]]) : 0.0;
          target[k] = xs[v] + odx - nx;
        }
      }
      if (count == 1) {
        dx[s0] = std::max(-half, std::min(half, target[s0]));
        continue;
      }
      double step = std::min((2.0 * half - 2.0 * SPREAD_PAD) / (count - 1), MAX_END_STEP);
      if (step <= 0) continue;
      double lo = std::max(-half + SPREAD_PAD, -half);
      double hi = std::min(half - SPREAD_PAD, half);
      int top = 0;
      for (int k = 0; k < count; k++) {
        double shift = k * step;
        double v = target[s0 + k] - shift;
        bL[top] = lo - shift;
        bR[top] = hi - shift;
        target[top] = v;
        bN[top] = 1;
        while (top > 0 && target[top - 1] / bN[top - 1] > target[top] / bN[top]) {
          target[top - 1] += target[top];
          bN[top - 1] += bN[top];
          if (bL[top] > bL[top - 1]) bL[top - 1] = bL[top];
          if (bR[top] < bR[top - 1]) bR[top - 1] = bR[top];
          top--;
        }
        top++;
      }
      int idx = s0;
      for (int b = 0; b < top; b++) {
        double base = std::max(bL[b], std::min(bR[b], target[b] / bN[b]));
        for (int c = 0; c < bN[b]; c++) {
          dx[idx] = base + (idx - s0) * step;
          idx++;
        }
      }
    }
  }
  for (int k = 0; k < p; k++) {
    int e = seg[k];
    if (isFrom[k]) {
      EFD[e] = dx[k];
      int v = ET[e];
      if (!empty.empty() && v >= real && empty[RANK[v]]) xs[v] = xs[EF[e]] + dx[k];
    } else {
      ETD[e] = dx[k];
      int v = EF[e];
      if (!empty.empty() && v >= real && empty[RANK[v]]) xs[v] = xs[ET[e]] + dx[k];
    }
  }
}

inline void snap(Graph& g, std::vector<double>& xs, const std::vector<uint8_t>& empty, const LayeredOptions& opt) noexcept {
  auto& n = g.n;
  auto& real = g.real;
  auto& maxRank = g.maxRank;
  auto& KIND = g.KIND;
  auto& RANK = g.RANK;
  auto& EF = g.EF;
  auto& ET = g.ET;
  auto& EFD = g.EFD;
  auto& ETD = g.ETD;
  auto& succStart = g.succStart;
  auto& succE = g.succE;
  auto& predStart = g.predStart;
  auto& predE = g.predE;
  auto& layerStart = g.layerStart;
  auto& order = g.order;
  auto& pos = g.pos;
  int segments = static_cast<int>(EF.size());
  for (int e = 0; e < segments; e++) {
    int u = EF[e];
    int v = ET[e];
    int bend = -1;
    while (!empty.empty() && v >= real && empty[RANK[v]]) {
      bend = v;
      e = succE[succStart[v]];
      v = ET[e];
    }
    double ax = xs[u] + EFD[u == EF[e] ? e : succE[succStart[u]]];
    double bx = xs[v] + ETD[e];
    double diff = std::abs(ax - bx);
    if (diff == 0 || diff > SNAP_CLOSE) continue;
    if (v >= real && KIND[v] == static_cast<uint8_t>(LKind::Dummy) && bend < 0) {
      int r = RANK[v];
      int p = pos[v];
      int s = layerStart[r];
      double room = halfRoom(g, opt, v);
      if (p > 0 && ax - room < xs[order[s + p - 1]] + halfRoom(g, opt, order[s + p - 1])) continue;
      if (p + 1 < layerStart[r + 1] - s && ax + room > xs[order[s + p + 1]] - halfRoom(g, opt, order[s + p + 1])) continue;
      xs[v] = ax;
    } else if (u >= real && KIND[u] == static_cast<uint8_t>(LKind::Dummy) && bend < 0) {
      int r = RANK[u];
      int p = pos[u];
      int s = layerStart[r];
      double room = halfRoom(g, opt, u);
      if (p > 0 && bx - room < xs[order[s + p - 1]] + halfRoom(g, opt, order[s + p - 1])) continue;
      if (p + 1 < layerStart[r + 1] - s && bx + room > xs[order[s + p + 1]] - halfRoom(g, opt, order[s + p + 1])) continue;
      xs[u] = bx;
    } else if (bend < 0) {
      ETD[e] = ax - xs[v];
    }
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
  auto& maxRank = g.maxRank;
  auto& RANK = g.RANK;
  auto& EF = g.EF;
  auto& ET = g.ET;
  auto& EFD = g.EFD;
  auto& ETD = g.ETD;
  auto& succStart = g.succStart;
  auto& succE = g.succE;
  auto& predStart = g.predStart;
  auto& predE = g.predE;
  auto& layerStart = g.layerStart;
  auto& order = g.order;
  std::vector<double> target(g.n, std::numeric_limits<double>::quiet_NaN());
  int m = static_cast<int>(firstDummy.size());
  for (int ei = 0; ei < m; ei++) {
    int d0 = firstDummy[ei];
    if (d0 < 0) continue;
    int end = d0 + span[ei] - 1;
    int k = d0;
    while (k < end) {
      if (!empty[RANK[k]]) {
        k++;
        continue;
      }
      int k1 = k;
      while (k1 + 1 < end && empty[RANK[k1 + 1]]) k1++;
      int upSeg = predE[predStart[k]];
      int downSeg = succE[succStart[k1]];
      int u = EF[upSeg];
      int v = ET[downSeg];
      double ux = xs[u] + EFD[upSeg];
      double vx = xs[v] + ETD[downSeg];
      for (int d = k; d <= k1; d++) {
        target[d] = ux + ((vx - ux) * (RANK[d] - RANK[u])) / (RANK[v] - RANK[u]);
      }
      k = k1 + 1;
    }
  }
  for (int r = 1; r < maxRank; r++) {
    if (!empty[r]) continue;
    int s = layerStart[r];
    int e = layerStart[r + 1];
    for (int pass = 0; pass < 4; pass++) {
      for (int i = s; i < e; i++) {
        int d = order[i];
        if (std::isnan(target[d])) continue;
        double lo = i > s ? xs[order[i - 1]] + sep : -std::numeric_limits<double>::infinity();
        double hi = i + 1 < e ? xs[order[i + 1]] - sep : std::numeric_limits<double>::infinity();
        if (lo <= hi) xs[d] = std::max(lo, std::min(hi, target[d]));
      }
      for (int i = e - 1; i >= s; i--) {
        int d = order[i];
        if (std::isnan(target[d])) continue;
        double lo = i > s ? xs[order[i - 1]] + sep : -std::numeric_limits<double>::infinity();
        double hi = i + 1 < e ? xs[order[i + 1]] - sep : std::numeric_limits<double>::infinity();
        if (lo <= hi) xs[d] = std::max(lo, std::min(hi, target[d]));
      }
    }
    for (int i = s; i < e; i++) {
      int d = order[i];
      if (std::abs(xs[d] - target[d]) < 0.5) onLine[d] = 1;
    }
  }
}

inline void pastLabels(
  const Graph& g,
  std::vector<double>& xs,
  const std::vector<int>& firstDummy,
  const std::vector<int>& span,
  double sep,
  std::vector<uint8_t>& onLine
) {
  auto& real = g.real;
  auto& maxRank = g.maxRank;
  auto& W = g.W;
  auto& KIND = g.KIND;
  auto& RANK = g.RANK;
  auto& EF = g.EF;
  auto& ET = g.ET;
  auto& EFD = g.EFD;
  auto& ETD = g.ETD;
  auto& succStart = g.succStart;
  auto& succE = g.succE;
  auto& predStart = g.predStart;
  auto& predE = g.predE;
  auto& layerStart = g.layerStart;
  auto& order = g.order;
  std::vector<uint8_t> labelRank(maxRank + 1, 0);
  for (int r = 1; r < maxRank; r++) {
    bool hasLabel = false;
    bool hasNode = false;
    for (int i = layerStart[r]; i < layerStart[r + 1]; i++) {
      int v = order[i];
      if (v < real) hasNode = true;
      else if (KIND[v] == static_cast<uint8_t>(LKind::Label)) hasLabel = true;
    }
    if (hasLabel && !hasNode) labelRank[r] = 1;
  }
  std::vector<double> target(g.n, std::numeric_limits<double>::quiet_NaN());
  int m = static_cast<int>(firstDummy.size());
  for (int ei = 0; ei < m; ei++) {
    int d0 = firstDummy[ei];
    if (d0 < 0) continue;
    int end = d0 + span[ei] - 1;
    for (int d = d0; d < end; d++) {
      int r = RANK[d];
      if (!labelRank[r] || KIND[d] != static_cast<uint8_t>(LKind::Dummy)) continue;
      int upSeg = predE[predStart[d]];
      int downSeg = succE[succStart[d]];
      int u = EF[upSeg];
      int v = ET[downSeg];
      if (u >= real && onLine[u]) {
        upSeg = predE[predStart[u]];
        u = EF[upSeg];
      }
      if (v >= real && onLine[v]) {
        downSeg = succE[succStart[v]];
        v = ET[downSeg];
      }
      double ux = xs[u] + EFD[upSeg];
      double vx = xs[v] + ETD[downSeg];
      target[d] = ux + ((vx - ux) * (r - RANK[u])) / (RANK[v] - RANK[u]);
    }
  }
  for (int r = 1; r < maxRank; r++) {
    if (!labelRank[r]) continue;
    int s = layerStart[r];
    int e = layerStart[r + 1];
    for (int pass = 0; pass < 4; pass++) {
      for (int i = s; i < e; i++) {
        int d = order[i];
        if (std::isnan(target[d])) continue;
        double lo = i > s ? xs[order[i - 1]] + W[order[i - 1]] / 2.0 + sep : -std::numeric_limits<double>::infinity();
        double hi = i + 1 < e ? xs[order[i + 1]] - W[order[i + 1]] / 2.0 - sep : std::numeric_limits<double>::infinity();
        if (lo <= hi) xs[d] = std::max(lo, std::min(hi, target[d]));
      }
      for (int i = e - 1; i >= s; i--) {
        int d = order[i];
        if (std::isnan(target[d])) continue;
        double lo = i > s ? xs[order[i - 1]] + W[order[i - 1]] / 2.0 + sep : -std::numeric_limits<double>::infinity();
        double hi = i + 1 < e ? xs[order[i + 1]] - W[order[i + 1]] / 2.0 - sep : std::numeric_limits<double>::infinity();
        if (lo <= hi) xs[d] = std::max(lo, std::min(hi, target[d]));
      }
    }
    for (int i = s; i < e; i++) {
      int d = order[i];
      if (std::isnan(target[d])) continue;
      if (std::abs(xs[d] - target[d]) < 0.5) {
        onLine[d] = 1;
        continue;
      }
      int upSeg = predE[predStart[d]];
      int downSeg = succE[succStart[d]];
      double ux = xs[EF[upSeg]] + EFD[upSeg];
      double vx = xs[ET[downSeg]] + ETD[downSeg];
      if ((xs[d] - ux) * (vx - xs[d]) < 0) xs[d] = std::abs(xs[d] - ux) <= std::abs(xs[d] - vx) ? ux : vx;
    }
  }
}

inline void assignTracks(
  Pieces& pieces,
  std::vector<int>& trackCount,
  bool levelsOnly,
  const std::vector<double>& room,
  std::vector<double>& tall
) {
  int total = static_cast<int>(pieces.from.size());
  pieces.track.assign(total, -1);
  if (levelsOnly) pieces.levels.assign(total, 0);
  if (total == 0) return;

  int maxRank = static_cast<int>(trackCount.size()) - 1;
  std::vector<int> start(maxRank + 2, 0);
  for (int i = 0; i < total; i++) start[pieces.rank[i] + 1]++;
  for (int r = 0; r <= maxRank; r++) start[r + 1] += start[r];
  std::vector<int> byRank(total, 0);
  {
    std::vector<int> fill(start.begin(), start.begin() + maxRank + 1);
    for (int i = 0; i < total; i++) byRank[fill[pieces.rank[i]]++] = i;
  }

  std::vector<double> left(total, 0);
  std::vector<double> right(total, 0);
  std::vector<int> turn(total, 0);
  for (int i = 0; i < total; i++) {
    double a = pieces.from[i];
    double b = pieces.to[i];
    double d = b - a;
    if (std::abs(d) > 0.5) {
      turn[i] = d > 0 ? 1 : -1;
      left[i] = std::min(a, b);
      right[i] = std::max(a, b);
    } else if (levelsOnly) {
      left[i] = a;
      right[i] = a;
    }
  }

  if (!levelsOnly) {
    for (int r = 0; r <= maxRank; r++) {
      std::vector<int> active;
      for (int k = start[r]; k < start[r + 1]; k++) {
        int i = byRank[k];
        if (turn[i] != 0) active.push_back(i);
      }
      if (active.empty()) continue;
      if (active.size() > 1) {
        std::stable_sort(active.begin(), active.end(), [&](int a, int b) {
          return pieces.to[a] < pieces.to[b];
        });
      }
      int n = static_cast<int>(active.size());
      std::vector<int> inDeg(n, 0);
      std::vector<std::vector<int>> adj(n);
      for (int a = 0; a < n; a++) {
        int i = active[a];
        for (int b = a + 1; b < n; b++) {
          int j = active[b];
          if (right[i] < left[j] - TRACK_CLEAR || right[j] < left[i] - TRACK_CLEAR) continue;
          int prod = turn[i] * turn[j];
          int first = -1, second = -1;
          if (prod > 0) {
            double df = (pieces.from[j] - pieces.from[i]) * turn[i];
            double dt = (pieces.to[j] - pieces.to[i]) * turn[i];
            if (df >= 0 && dt >= 0 && df + dt > 0) {
              first = turn[i] > 0 ? b : a;
              second = turn[i] > 0 ? a : b;
            } else if (df <= 0 && dt <= 0 && df + dt < 0) {
              first = turn[i] > 0 ? a : b;
              second = turn[i] > 0 ? b : a;
            }
          } else {
            double sepF = (pieces.from[j] - pieces.from[i]) * turn[i];
            double sepT = (pieces.to[i] - pieces.to[j]) * turn[i];
            if (sepF >= 0 && sepT < 0) {
              first = a;
              second = b;
            } else if (sepT >= 0 && sepF < 0) {
              first = b;
              second = a;
            }
          }
          if (first >= 0) {
            adj[first].push_back(second);
            inDeg[second]++;
          }
        }
      }
      std::vector<int> order(n, 0);
      std::vector<uint8_t> done(n, 0);
      for (int step = 0; step < n; step++) {
        int pick = -1;
        for (int a = 0; a < n; a++) {
          if (!done[a] && inDeg[a] == 0) {
            pick = a;
            break;
          }
        }
        if (pick < 0) {
          for (int a = 0; a < n; a++) {
            if (!done[a] && (pick < 0 || inDeg[a] < inDeg[pick])) pick = a;
          }
        }
        done[pick] = 1;
        order[step] = pick;
        for (int u : adj[pick]) inDeg[u]--;
      }
      std::vector<std::vector<int>> tracks;
      for (int a : order) {
        int i = active[a];
        int minTrack = 0;
        for (int b = 0; b < n; b++) {
          for (int u : adj[b]) {
            if (u == a && pieces.track[active[b]] >= minTrack) minTrack = pieces.track[active[b]] + 1;
          }
        }
        int chosen = -1;
        for (int t = minTrack; t < static_cast<int>(tracks.size()); t++) {
          bool fits = true;
          for (int j : tracks[t]) {
            if (left[i] <= right[j] + TRACK_CLEAR && right[i] >= left[j] - TRACK_CLEAR) {
              fits = false;
              break;
            }
          }
          if (fits) {
            chosen = t;
            break;
          }
        }
        if (chosen < 0) {
          chosen = static_cast<int>(tracks.size());
          tracks.push_back({});
        }
        tracks[chosen].push_back(i);
        pieces.track[i] = chosen;
      }
      trackCount[r] = static_cast<int>(tracks.size());
    }
    return;
  }

  for (int r = 0; r <= maxRank; r++) {
    int s = start[r];
    int e = start[r + 1];
    if (e - s < 2) continue;
    std::vector<int> order(byRank.begin() + s, byRank.begin() + e);
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
      if (left[a] != left[b]) return left[a] < left[b];
      return right[a] < right[b];
    });
    int p = 0;
    while (p < static_cast<int>(order.size())) {
      int q = p + 1;
      double reach = right[order[p]];
      int wideCount = pieces.wide[order[p]];
      bool sameEnds = turn[order[p]] != 0;
      while (q < static_cast<int>(order.size()) && left[order[q]] < reach - TRACK_CLEAR) {
        int j = order[q++];
        if (right[j] > reach) reach = right[j];
        wideCount += pieces.wide[j];
        if (turn[j] == 0 || left[j] != left[order[p]] || right[j] != right[order[p]]) sameEnds = false;
      }
      int count = q - p;
      if (wideCount > 0 && count > 1 && !sameEnds) {
        std::vector<int> group(order.begin() + p, order.begin() + q);
        if (group.size() > 2) {
          std::stable_sort(group.begin(), group.end(), [&](int a, int b) {
            if (turn[a] != turn[b]) return turn[a] < turn[b];
            return turn[a] < 0 ? right[a] < right[b] : left[b] < left[a];
          });
        }
        int k = 0;
        for (int i : group) {
          pieces.track[i] = k++;
          pieces.levels[i] = count;
        }
        if (count > trackCount[r]) trackCount[r] = count;
      } else {
        for (int a = p; a < q; a++) {
          int i = order[a];
          double span = (right[i] - left[i]) * MIN_SLOPE;
          if (span > tall[r]) tall[r] = span;
          if (!pieces.wide[i]) continue;
          double slope = room[r] / (right[i] - left[i]);
          double dy = 0.75 * room[r];
          for (int b = p; b < q; b++) {
            int j = order[b];
            if (j == i || pieces.wide[j]) continue;
            if (turn[j] == 0) {
              if (left[j] > left[i] + TRACK_CLEAR && left[j] < right[i] - TRACK_CLEAR) {
                double hit = slope * (right[i] - left[j]);
                if (std::abs(hit - room[r] / 2.0) < dy) {
                  pieces.track[i] = 0;
                  pieces.levels[i] = 1;
                  break;
                }
              }
            } else if (turn[i] * turn[j] > 0) {
              double df = (pieces.from[j] - pieces.from[i]) * turn[i];
              double dt = (pieces.to[j] - pieces.to[i]) * turn[i];
              if (df * dt < -TRACK_CLEAR * TRACK_CLEAR) {
                pieces.track[i] = 0;
                pieces.levels[i] = 1;
                break;
              }
            }
          }
        }
      }
      p = q;
    }
  }
}

inline std::pair<int, int> reduceCrossings(Graph& g, const std::vector<LNode>& nodes) noexcept {
  auto& n = g.n;
  auto& real = g.real;
  auto& maxRank = g.maxRank;
  auto& W = g.W;
  auto& EF = g.EF;
  auto& ET = g.ET;
  auto& EFD = g.EFD;
  auto& ETD = g.ETD;
  auto& succStart = g.succStart;
  auto& succE = g.succE;
  auto& predStart = g.predStart;
  auto& predE = g.predE;
  auto& layerStart = g.layerStart;
  auto& order = g.order;
  auto& pos = g.pos;
  auto& offsets = g.offsets;
  auto& fixed = g.fixed;
  bool hasGRP = g.hasGRP;
  auto& GRP = g.GRP;
  auto& groupParent = g.groupParent;

  int widest = 0;
  int maxLayerEdges = 0;
  for (int r = 0; r <= maxRank; r++) {
    int w = layerStart[r + 1] - layerStart[r];
    if (w > widest) widest = w;
    int e = 0;
    for (int i = layerStart[r]; i < layerStart[r + 1]; i++) {
      int v = order[i];
      e += succStart[v + 1] - succStart[v];
    }
    if (e > maxLayerEdges) maxLayerEdges = e;
  }

  std::vector<int> leaving;
  std::vector<int> arriving;
  std::vector<int> place;
  std::vector<uint8_t> mixed;
  if (offsets) {
    leaving.assign(succE.begin(), succE.end());
    arriving.assign(predE.begin(), predE.end());
    place.assign(EF.size(), 0);
    mixed.assign(n, 0);
    for (int v = 0; v < n; v++) {
      int s0 = succStart[v];
      int s1 = succStart[v + 1];
      if (s1 - s0 > 1) {
        std::sort(leaving.begin() + s0, leaving.begin() + s1, [&](int a, int b) {
          return EFD[a] < EFD[b];
        });
        for (int k = s0 + 1; k < s1; k++) {
          if (EFD[leaving[k]] != EFD[leaving[k - 1]]) {
            mixed[v] = 1;
            break;
          }
        }
      }
      int p0 = predStart[v];
      int p1 = predStart[v + 1];
      if (p1 - p0 > 1) {
        std::sort(arriving.begin() + p0, arriving.begin() + p1, [&](int a, int b) {
          return ETD[a] < ETD[b];
        });
      }
    }
  }
  int maxSouth = offsets ? maxLayerEdges : widest;
  int treeSize = 1;
  while (treeSize < maxSouth) treeSize <<= 1;
  std::vector<int> tree(2 * treeSize, 0);
  std::vector<int> targets(maxLayerEdges, 0);

  auto count = [&]() -> int {
    int total = 0;
    for (int r = 0; r < maxRank; r++) {
      int southLen = layerStart[r + 2] - layerStart[r + 1];
      if (offsets) {
        int p = 0;
        for (int j = layerStart[r + 1]; j < layerStart[r + 2]; j++) {
          int w = order[j];
          int from = predStart[w];
          int to = predStart[w + 1];
          if (from == to) continue;
          double prev = ETD[arriving[from]];
          for (int k = from; k < to; k++) {
            int e = arriving[k];
            if (ETD[e] != prev) {
              p++;
              prev = ETD[e];
            }
            place[e] = p;
          }
          p++;
        }
        southLen = p;
      }
      int t = 0;
      for (int i = layerStart[r]; i < layerStart[r + 1]; i++) {
        int v = order[i];
        int from = succStart[v];
        int to = succStart[v + 1];
        int t0 = t;
        for (int k = from; k < to; k++) targets[t++] = offsets ? place[leaving[k]] : pos[ET[succE[k]]];
        int d = t - t0;
        if (d < 2) continue;
        if (offsets && mixed[v]) {
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
  std::vector<double> bary(n, 0);
  std::vector<int> moving(widest, 0);
  std::vector<int> spare(widest, 0);

  int groups = hasGRP ? static_cast<int>(groupParent.size()) : 0;
  std::vector<int> depth(groups, 0);
  for (int gr = 0; gr < groups; gr++) for (int p = gr; p >= 0; p = groupParent[p]) depth[gr]++;
  std::vector<double> groupSum(groups, 0);
  std::vector<int> groupCount(groups, 0);
  std::vector<int> groupFirst(groups, 0);
  auto arrange = [&](int r, const std::vector<double>& key) {
    int s = layerStart[r];
    int e = layerStart[r + 1];
    for (int i = s; i < e; i++) {
      for (int gr = GRP[order[i]]; gr >= 0; gr = groupParent[gr]) {
        groupSum[gr] = 0;
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
    auto value = [&](int item) -> double { return item >= n ? groupSum[item - n] / groupCount[item - n] : key[item]; };
    auto first = [&](int item) -> int { return item >= n ? groupFirst[item - n] : s + pos[item]; };
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
    std::vector<double> at(n, 0);
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
            int across = down ? layerStart[r] - layerStart[r - 1] : layerStart[r + 2] - layerStart[r + 1];
            bary[v] = (static_cast<double>(i - s) * across) / (e - s);
            continue;
          }
          bary[v] = -1;
          continue;
        }
        double sum = 0;
        for (int k = from; k < to; k++) {
          int u = other[list[k]];
          sum += pos[u];
          if (offsets && W[u] > 0) sum += otherDx[list[k]] / W[u];
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
      for (int i = s; i < e; i++) if (bary[order[i]] != -1) order[i] = moving[k++];
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
    return offsets && W[u] > 0 ? pos[u] + (up ? EFD[e] : ETD[e]) / W[u] : pos[u];
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
  bool small = n <= 600;
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

inline std::vector<double> position(const Graph& g, const LayeredOptions& opt) noexcept {
  auto& n = g.n;
  auto& maxRank = g.maxRank;
  auto& W = g.W;
  auto& KIND = g.KIND;
  auto& EF = g.EF;
  auto& ET = g.ET;
  auto& EFD = g.EFD;
  auto& ETD = g.ETD;
  auto& succStart = g.succStart;
  auto& succE = g.succE;
  auto& predStart = g.predStart;
  auto& predE = g.predE;
  auto& layerStart = g.layerStart;
  auto& order = g.order;
  auto& pos = g.pos;
  int segments = static_cast<int>(EF.size());
  std::vector<double> gap(n, 0);
  for (int i = 0; i < n; i++) gap[i] = halfRoom(g, opt, i);

  std::vector<uint8_t> conflict(segments, 0);
  for (int r = 1; r < maxRank; r++) {
    int k0 = 0;
    int s1 = layerStart[r + 1];
    int e1 = layerStart[r + 2];
    int topLen = layerStart[r + 1] - layerStart[r];
    for (int l1 = 0; l1 < e1 - s1; l1++) {
      int v = order[s1 + l1];
      int k1 = -1;
      if (KIND[v] == static_cast<uint8_t>(LKind::Dummy) && predStart[v + 1] - predStart[v] == 1) {
        int u = EF[predE[predStart[v]]];
        if (KIND[u] == static_cast<uint8_t>(LKind::Dummy)) k1 = pos[u];
      }
      if (k1 >= 0 || l1 == e1 - s1 - 1) {
        int lim = k1 >= 0 ? k1 : topLen - 1;
        for (int p = 0; p <= l1; p++) {
          int w = order[s1 + p];
          for (int k = predStart[w]; k < predStart[w + 1]; k++) {
            int seg = predE[k];
            int uPos = pos[EF[seg]];
            if (uPos < k0 || uPos > lim) conflict[seg] = 1;
          }
        }
        if (k1 >= 0) k0 = k1;
      }
    }
  }

  std::vector<int> root(n, 0);
  std::vector<int> align(n, 0);
  std::vector<int> sink(n, 0);
  std::vector<double> shift(n, 0);
  std::vector<double> x(n, 0);
  std::vector<double> dxToRoot(n, 0);
  std::vector<int> neigh(n, 0);

  auto alignPass = [&](bool down, bool left) -> std::vector<double> {
    for (int i = 0; i < n; i++) {
      root[i] = i;
      align[i] = i;
      sink[i] = i;
      shift[i] = std::numeric_limits<double>::infinity();
      x[i] = std::numeric_limits<double>::quiet_NaN();
      dxToRoot[i] = 0;
    }

    for (int step = 1; step <= maxRank; step++) {
      int r = down ? step : maxRank - step;
      int s = layerStart[r];
      int e = layerStart[r + 1];
      int rlim = left ? -1 : std::numeric_limits<int>::max();
      for (int idx = 0; idx < e - s; idx++) {
        int i = left ? s + idx : e - 1 - idx;
        int v = order[i];
        int deg = 0;
        if (down) {
          for (int k = predStart[v]; k < predStart[v + 1]; k++) {
            int seg = predE[k];
            if (!conflict[seg]) neigh[deg++] = seg;
          }
        } else {
          for (int k = succStart[v]; k < succStart[v + 1]; k++) {
            int seg = succE[k];
            if (!conflict[seg]) neigh[deg++] = seg;
          }
        }
        if (deg == 0) continue;
        for (int a = 1; a < deg; a++) {
          int seg = neigh[a];
          int key = pos[down ? EF[seg] : ET[seg]];
          int b = a - 1;
          while (b >= 0 && pos[down ? EF[neigh[b]] : ET[neigh[b]]] > key) {
            neigh[b + 1] = neigh[b];
            b--;
          }
          neigh[b + 1] = seg;
        }
        int mid1 = (deg - 1) >> 1;
        int mid2 = deg >> 1;
        for (int m = (left ? mid1 : mid2); (left ? m <= mid2 : m >= mid1); m += (left ? 1 : -1)) {
          if (align[v] != v) continue;
          int seg = neigh[m];
          int u = down ? EF[seg] : ET[seg];
          int up = pos[u];
          if (left ? up > rlim : up < rlim) {
            align[u] = v;
            root[v] = root[u];
            dxToRoot[v] = dxToRoot[u] + (down ? EFD[seg] - ETD[seg] : ETD[seg] - EFD[seg]);
            align[v] = root[v];
            rlim = up;
          }
        }
      }
    }

    std::function<void(int)> placeBlock = [&](int v) {
      if (!std::isnan(x[v])) return;
      x[v] = 0;
      int w = v;
      do {
        int r = g.RANK[w];
        int s = layerStart[r];
        int e = layerStart[r + 1];
        int p = pos[w];
        if (left ? p > 0 : p < e - s - 1) {
          int pred = order[s + (left ? p - 1 : p + 1)];
          int u = root[pred];
          placeBlock(u);
          if (sink[v] == v) sink[v] = sink[u];
          double sep = gap[w] + gap[pred];
          double need = left ? x[u] + dxToRoot[pred] + sep - dxToRoot[w] : x[u] + dxToRoot[pred] - sep - dxToRoot[w];
          if (sink[v] != sink[u]) {
            double sVal = left ? x[v] - need : need - x[v];
            if (sVal < shift[sink[u]]) shift[sink[u]] = sVal;
          } else {
            if (left ? need > x[v] : need < x[v]) x[v] = need;
          }
        }
        w = align[w];
      } while (w != v);
    };

    for (int i = 0; i < n; i++) if (root[i] == i) placeBlock(i);
    for (int i = 0; i < n; i++) {
      int rv = root[i];
      double sVal = shift[sink[rv]];
      x[i] = x[rv] + dxToRoot[i] + (std::isfinite(sVal) ? (left ? sVal : -sVal) : 0.0);
    }
    return x;
  };

  auto xUL = alignPass(true, true);
  auto xUR = alignPass(true, false);
  auto xDL = alignPass(false, true);
  auto xDR = alignPass(false, false);
  std::vector<double>* layouts[4] = {&xUL, &xUR, &xDL, &xDR};

  double minSpan = std::numeric_limits<double>::infinity();
  int narrowest = 0;
  double mins[4] = {0, 0, 0, 0};
  double maxs[4] = {0, 0, 0, 0};
  for (int k = 0; k < 4; k++) {
    auto& arr = *layouts[k];
    double lo = std::numeric_limits<double>::infinity();
    double hi = -std::numeric_limits<double>::infinity();
    for (int i = 0; i < n; i++) {
      double hw = W[i] / 2.0;
      if (arr[i] - hw < lo) lo = arr[i] - hw;
      if (arr[i] + hw > hi) hi = arr[i] + hw;
    }
    mins[k] = lo;
    maxs[k] = hi;
    if (hi - lo < minSpan) {
      minSpan = hi - lo;
      narrowest = k;
    }
  }
  for (int k = 0; k < 4; k++) {
    if (k == narrowest) continue;
    double d = k % 2 == 0 ? mins[narrowest] - mins[k] : maxs[narrowest] - maxs[k];
    auto& arr = *layouts[k];
    for (int i = 0; i < n; i++) arr[i] += d;
  }

  std::vector<double> out(n, 0);
  for (int i = 0; i < n; i++) {
    double a = xUL[i], b = xUR[i], c = xDL[i], d = xDR[i];
    if (a > b) std::swap(a, b);
    if (c > d) std::swap(c, d);
    if (a > c) std::swap(a, c);
    if (b > d) std::swap(b, d);
    out[i] = (b + c) / 2.0;
  }
  return out;
}

} // namespace detail_layered

inline LayeredResult layoutLayered(std::vector<LNode>& nodes, std::vector<LEdge>& edges, const LayeredOptions& opt = {}) noexcept {
  using namespace detail_layered;
  int real = static_cast<int>(nodes.size());
  int m = static_cast<int>(edges.size());
  if (real == 0) return {0, 0, 0, 0};

  int step = 1;
  if (!opt.tracks) {
    for (int ei = 0; ei < m; ei++) {
      if (edges[ei].labelW > 0) {
        step = 2;
        break;
      }
    }
  }
  if (step == 1 && !opt.tracks) {
    Pele::FlatSet<int64_t> seen;
    for (int ei = 0; ei < m; ei++) {
      const auto& e = edges[ei];
      int a = e.tail < e.head ? e.tail : e.head;
      int b = e.tail < e.head ? e.head : e.tail;
      int64_t key = static_cast<int64_t>(a) * real + b;
      if (seen.count(key)) {
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
  std::vector<double> W(n, 0);
  std::vector<double> H(n, 0);
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
  std::vector<double> EFD(segments, 0);
  std::vector<double> ETD(segments, 0);
  std::vector<int> firstDummy(m, -1);
  std::vector<uint8_t> heads(maxRank + 1, 0);
  std::vector<int> labelNode(m, -1);
  bool spans = false;
  for (int i = 0; i < real; i++) if (nodes[i].span > 0 && nodes[i].kind == LKind::Node) spans = true;
  std::vector<uint8_t> EFK;
  std::vector<uint8_t> ETK;
  if (spans) {
    EFK.assign(segments, 0);
    ETK.assign(segments, 0);
  }
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
      if (aDx != 0 || bDx != 0) offsets = true;
      int ra = RANK[a];
      int rb = RANK[b];
      int labelRank = -1;
      if (e.labelW > 0) {
        labelRank = ra + ((rb - ra) >> 1);
        if (step == 2 && rb - ra >= 4) {
          if ((labelRank & 1) == 1) labelRank += e.reversed ? 1 : -1;
        } else if (step == 2 && (labelRank & 1) == 0) labelRank += e.reversed ? 1 : -1;
      }
      int prev = a;
      if (rb - ra > 1) firstDummy[ei] = next;
      firstSeg[ei] = ce;
      if (spans && !(e.reversed ? e.headFixed : e.tailFixed)) EFK[ce] = static_cast<uint8_t>(e.reversed ? LEnd::Arrives : LEnd::Leaves);
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
        EFD[ce] = prev == a ? aDx : 0;
        ce++;
        prev = d;
      }
      EF[ce] = prev;
      ET[ce] = b;
      EFD[ce] = prev == a ? aDx : 0;
      ETD[ce] = bDx;
      if (spans && !(e.reversed ? e.tailFixed : e.headFixed)) ETK[ce] = static_cast<uint8_t>(e.reversed ? LEnd::Leaves : LEnd::Arrives);
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
    Pele::FlatMap<int, int> index;
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
      Pele::FlatMap<int, int> index;
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
      std::vector<double> pts = {static_cast<double>(g.RANK[a]), at[a]};
      if (!direct[ei] && firstDummy[ei] >= 0) {
        for (int d = firstDummy[ei], end = d + span[ei] - 1; d < end; d++) {
          pts.push_back(g.RANK[d]);
          pts.push_back(at[d]);
        }
      }
      pts.push_back(g.RANK[b]);
      pts.push_back(at[b]);
      e.points = std::move(pts);
    }
    return {0, 0, crossings, declared};
  }

  if (spans) spread(g, nodes, EFK, ETK);

  auto xs = position(g, opt);

  auto empty = emptyRanks(g);
  if (spans) {
    int free = freeEnds(g, EFK, ETK);
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
        bool bare = !empty.empty() && empty[g.RANK[d]] == 1;
        if (!opt.tracks && bare != (onLine[d] == 1)) twin = true;
      }
      if (twin) continue;
      double x = xs[g.EF[firstSeg[ei]]] + g.EFD[firstSeg[ei]];
      int r = g.RANK[g.EF[firstSeg[ei]]];
      for (int d = d0, end = d0 < 0 ? d0 : d0 + span[ei] - 1; d < end; d++) {
        if (!empty.empty() && empty[g.RANK[d]]) continue;
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
  std::vector<double> run(maxRank + 1, 0);
  for (int e = 0; e < segments; e++) {
    double across = std::abs(xs[g.EF[e]] + g.EFD[e] - xs[g.ET[e]] - g.ETD[e]);
    if (across > run[g.RANK[g.EF[e]]]) run[g.RANK[g.EF[e]]] = across;
  }
  std::vector<double> bandTop(maxRank + 1, 0);
  std::vector<double> bandBottom(maxRank + 1, 0);
  auto usual = [&](int r) -> double { return (r < startRanks || r >= maxRank - endRanks ? opt.portSep : opt.rankSep) / step; };
  std::vector<double> gapUnder(maxRank + 1, 0);
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
    std::vector<double> room(maxRank + 1, 0);
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
    std::vector<double> tall(maxRank + 1, 0);
    assignTracks(pieces, trackCount, !opt.tracks, room, tall);
    for (int r = 0; r < maxRank; r++) {
      if (trackCount[r] > 0) gapUnder[r] += std::max(0.0, 2.0 * margin + (trackCount[r] - 1) * leastPitch - room[r]);
      else if (tall[r] > 0) gapUnder[r] += std::max(0.0, std::min(MAX_GAP * opt.rankSep, tall[r]) - room[r]);
    }
  }
  double y = 0;
  for (int r = 0; r <= maxRank; r++) {
    double h = 0;
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
  std::vector<uint8_t> arrives(maxRank + 1, 0);
  std::vector<uint8_t> leaves(maxRank + 1, 0);
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
    } else if (e.labelW > 0) {
      e.labelX = (e.points[0] + e.points[e.points.size() - 2]) / 2.0;
      e.labelY = (e.points[1] + e.points[e.points.size() - 1]) / 2.0;
    }
  }

  return {maxX - minX, height, crossings, declared};
}

// ============================================================================
// Port of src/layout/compound.ts
// ============================================================================

struct CNode {
  double w = 0;
  double h = 0;
  double span = 0;
  double sidewaysSpan = 0;
  int parent = -1;
  bool isGroup = false;
  std::optional<std::string> dir;
  double padTop = 0;
  double padRight = 0;
  double padBottom = 0;
  double padLeft = 0;
  int pin = 0;
  double x = 0;
  double y = 0;
};

struct CEdge {
  int src = 0;
  int dst = 0;
  int minlen = 1;
  double weight = 1;
  double srcDx = 0;
  double srcDy = 0;
  double dstDx = 0;
  double dstDy = 0;
  bool srcFixed = false;
  bool dstFixed = false;
  double labelW = 0;
  double labelH = 0;
  std::vector<double> route;
  double labelX = 0;
  double labelY = 0;
};

struct CompoundOptions {
  std::string dir = "TB";
  double nodeSep = 36;
  double rankSep = 48;
  double edgeSep = 16;
  double portSep = 16;
  double headRoom = 0;
  bool tracks = false;
  bool apart = false;
  bool straight = false;
};

namespace detail_compound {

constexpr double SIDE_PORT_PAD = 18;
constexpr double TURN_DIST = 14;
constexpr double LOOP_W = 28;
constexpr double LOOP_H = 22;
constexpr double FLAT_SLACK = 1.25;
constexpr double FLAT_SPARE = 2;
constexpr double IN_LINE = 1;
constexpr int MAX_CROSSING_EDGES = 600;

enum class Side : int { Top = 0, Right = 1, Bottom = 2, Left = 3 };

inline Side startSide(const std::string& dir) noexcept {
  if (dir == "BT") return Side::Bottom;
  if (dir == "LR") return Side::Left;
  if (dir == "RL") return Side::Right;
  return Side::Top;
}

inline Side opposite(Side s) noexcept {
  return static_cast<Side>((static_cast<int>(s) + 2) & 3);
}

inline LayeredOptions styleOf(const CompoundOptions& opt) noexcept {
  LayeredOptions lo;
  lo.nodeSep = opt.nodeSep;
  lo.rankSep = opt.rankSep;
  lo.edgeSep = opt.edgeSep;
  lo.portSep = opt.portSep;
  lo.headRoom = opt.headRoom;
  lo.tracks = opt.tracks;
  lo.apart = opt.apart;
  lo.straight = opt.straight;
  return lo;
}

struct Port {
  int id = 0;
  int e = 0;
  bool out = false;
  int inner = 0;
  int outer = 0;
  double weight = 0;
  Side side = Side::Top;
  double hint = std::numeric_limits<double>::quiet_NaN();
  double px = 0;
  double py = 0;
  std::vector<double> route;
};

struct LevelEdge {
  int e = 0;
  int from = 0;
  int to = 0;
  int fromPort = -1;
  int toPort = -1;
  int minlen = 1;
  double weight = 1;
  double labelW = 0;
  double labelH = 0;
};

struct Level {
  int id = -1;
  std::string dir = "TB";
  std::vector<int> children;
  std::vector<Port> ports;
  Pele::FlatMap<int, int> portOf; // key -> index in ports
  std::vector<LevelEdge> edges;
  std::unordered_map<int, std::vector<double>> routes;
  double innerW = 0;
  double innerH = 0;
  double shiftX = 0;
  double shiftY = 0;
  double x = 0;
  double y = 0;
};

inline void toLocal(const std::string& dir, std::vector<LNode>& lnodes, std::vector<LEdge>& ledges, double width, double height) noexcept {
  if (dir == "TB") {
    for (auto& e : ledges) {
      std::vector<double> out;
      out.reserve(e.points.size() / 2 * 3);
      for (size_t i = 0; i < e.points.size(); i += 2) {
        out.push_back(e.points[i]);
        out.push_back(e.points[i + 1]);
        out.push_back(0);
      }
      e.points = std::move(out);
    }
    return;
  }
  if (dir == "BT") {
    for (auto& n : lnodes) n.y = height - n.y;
    for (auto& e : ledges) {
      std::vector<double> out;
      out.reserve(e.points.size() / 2 * 3);
      for (size_t i = 0; i < e.points.size(); i += 2) {
        out.push_back(e.points[i]);
        out.push_back(height - e.points[i + 1]);
        out.push_back(0);
      }
      e.points = std::move(out);
      e.labelY = height - e.labelY;
    }
    return;
  }
  if (dir == "LR") {
    for (auto& n : lnodes) {
      double lx = n.x;
      n.x = n.y;
      n.y = lx;
    }
    for (auto& e : ledges) {
      std::vector<double> out;
      out.reserve(e.points.size() / 2 * 3);
      for (size_t i = 0; i < e.points.size(); i += 2) {
        out.push_back(e.points[i + 1]);
        out.push_back(e.points[i]);
        out.push_back(1);
      }
      e.points = std::move(out);
      double lx = e.labelX;
      e.labelX = e.labelY;
      e.labelY = lx;
    }
    return;
  }
  // RL
  for (auto& n : lnodes) {
    double lx = n.x;
    n.x = height - n.y;
    n.y = lx;
  }
  for (auto& e : ledges) {
    std::vector<double> out;
    out.reserve(e.points.size() / 2 * 3);
    for (size_t i = 0; i < e.points.size(); i += 2) {
      out.push_back(height - e.points[i + 1]);
      out.push_back(e.points[i]);
      out.push_back(1);
    }
    e.points = std::move(out);
    double lx = e.labelX;
    e.labelX = height - e.labelY;
    e.labelY = lx;
  }
}

inline void appendRoute(std::vector<double>& dst, const std::vector<double>& pts, double ox, double oy) noexcept {
  int n = static_cast<int>(pts.size());
  if (n == 0) return;
  int start = 0;
  int dlen = static_cast<int>(dst.size());
  if (dlen >= 3 && std::abs(dst[dlen - 3] - (pts[0] + ox)) < 0.01 && std::abs(dst[dlen - 2] - (pts[1] + oy)) < 0.01) {
    start = 3;
  }
  for (int i = start; i < n; i += 3) {
    dst.push_back(pts[i] + ox);
    dst.push_back(pts[i + 1] + oy);
    dst.push_back(pts[i + 2]);
  }
}

inline int routeCrossings(const std::vector<CNode>& nodes, const std::vector<CEdge>& edges, const LayeredOptions& style) noexcept {
  struct Seg {
    int e;
    double ax, ay, bx, by;
    double x0, x1, y0, y1;
  };
  std::vector<Seg> segs;
  for (int ei = 0; ei < static_cast<int>(edges.size()); ei++) {
    const auto& r = edges[ei].route;
    for (int k = 0; k + 3 < static_cast<int>(r.size()); k += 3) {
      double ax = r[k], ay = r[k + 1], bx = r[k + 3], by = r[k + 4];
      if (ax == bx && ay == by) continue;
      segs.push_back({ei, ax, ay, bx, by, std::min(ax, bx), std::max(ax, bx), std::min(ay, by), std::max(ay, by)});
    }
  }
  double pad = std::min(style.nodeSep, style.edgeSep) / 2.0;
  int total = 0;
  for (int i = 0; i < static_cast<int>(segs.size()); i++) {
    const auto& s = segs[i];
    const auto& e = edges[s.e];
    for (int j = i + 1; j < static_cast<int>(segs.size()); j++) {
      const auto& t = segs[j];
      if (s.e == t.e) continue;
      if (s.x1 <= t.x0 || t.x1 <= s.x0 || s.y1 <= t.y0 || t.y1 <= s.y0) continue;
      double d1 = (s.bx - s.ax) * (t.ay - s.ay) - (s.by - s.ay) * (t.ax - s.ax);
      double d2 = (s.bx - s.ax) * (t.by - s.ay) - (s.by - s.ay) * (t.bx - s.ax);
      if ((d1 >= 0 && d2 >= 0) || (d1 <= 0 && d2 <= 0)) continue;
      double d3 = (t.bx - t.ax) * (s.ay - t.ay) - (t.by - t.ay) * (s.ax - t.ax);
      double d4 = (t.bx - t.ax) * (s.by - t.ay) - (t.by - t.ay) * (s.bx - t.ax);
      if ((d3 >= 0 && d4 >= 0) || (d3 <= 0 && d4 <= 0)) continue;
      total++;
    }
    for (int ni = 0; ni < static_cast<int>(nodes.size()); ni++) {
      const auto& n = nodes[ni];
      if (n.isGroup || ni == e.src || ni == e.dst) continue;
      double x0 = n.x - n.w / 2.0 - pad;
      double x1 = n.x + n.w / 2.0 + pad;
      double y0 = n.y - n.h / 2.0 - pad;
      double y1 = n.y + n.h / 2.0 + pad;
      if (s.x1 <= x0 || x1 <= s.x0 || s.y1 <= y0 || y1 <= s.y0) continue;
      double dx = s.bx - s.ax;
      double dy = s.by - s.ay;
      double c1 = dx * (y0 - s.ay) - dy * (x0 - s.ax);
      double c2 = dx * (y0 - s.ay) - dy * (x1 - s.ax);
      double c3 = dx * (y1 - s.ay) - dy * (x0 - s.ax);
      double c4 = dx * (y1 - s.ay) - dy * (x1 - s.ax);
      if ((c1 > 0 && c2 > 0 && c3 > 0 && c4 > 0) || (c1 < 0 && c2 < 0 && c3 < 0 && c4 < 0)) continue;
      total++;
    }
  }
  return total;
}

struct Snapshot {
  std::vector<double> nodePos;
  std::vector<double> edgeHead;
  std::vector<std::vector<double>> edgeRoute;
  std::vector<double> levelBox;
  std::vector<double> portHint;
  Size total;
};

inline Snapshot snapshot(const std::vector<CNode>& nodes, const std::vector<CEdge>& edges, const std::vector<Level>& levels, Size total) noexcept {
  Snapshot s;
  s.nodePos.resize(nodes.size() * 4);
  for (size_t i = 0; i < nodes.size(); i++) {
    s.nodePos[i * 4] = nodes[i].x;
    s.nodePos[i * 4 + 1] = nodes[i].y;
    s.nodePos[i * 4 + 2] = nodes[i].w;
    s.nodePos[i * 4 + 3] = nodes[i].h;
  }
  s.edgeHead.resize(edges.size() * 6);
  s.edgeRoute.resize(edges.size());
  for (size_t i = 0; i < edges.size(); i++) {
    const auto& e = edges[i];
    s.edgeHead[i * 6] = e.srcDx;
    s.edgeHead[i * 6 + 1] = e.srcDy;
    s.edgeHead[i * 6 + 2] = e.dstDx;
    s.edgeHead[i * 6 + 3] = e.dstDy;
    s.edgeHead[i * 6 + 4] = e.labelX;
    s.edgeHead[i * 6 + 5] = e.labelY;
    s.edgeRoute[i] = e.route;
  }
  s.levelBox.resize(levels.size() * 2);
  for (size_t i = 0; i < levels.size(); i++) {
    s.levelBox[i * 2] = levels[i].x;
    s.levelBox[i * 2 + 1] = levels[i].y;
    for (const auto& port : levels[i].ports) s.portHint.push_back(port.hint);
  }
  s.total = total;
  return s;
}

inline Size restore(const Snapshot& s, std::vector<CNode>& nodes, std::vector<CEdge>& edges, std::vector<Level>& levels) noexcept {
  for (size_t i = 0; i < nodes.size(); i++) {
    nodes[i].x = s.nodePos[i * 4];
    nodes[i].y = s.nodePos[i * 4 + 1];
    nodes[i].w = s.nodePos[i * 4 + 2];
    nodes[i].h = s.nodePos[i * 4 + 3];
  }
  for (size_t i = 0; i < edges.size(); i++) {
    auto& e = edges[i];
    e.srcDx = s.edgeHead[i * 6];
    e.srcDy = s.edgeHead[i * 6 + 1];
    e.dstDx = s.edgeHead[i * 6]; // wait: let's check index i*6+2!
    e.dstDx = s.edgeHead[i * 6 + 2];
    e.dstDy = s.edgeHead[i * 6 + 3];
    e.labelX = s.edgeHead[i * 6 + 4];
    e.labelY = s.edgeHead[i * 6 + 5];
    e.route = s.edgeRoute[i];
  }
  size_t p = 0;
  for (size_t i = 0; i < levels.size(); i++) {
    levels[i].x = s.levelBox[i * 2];
    levels[i].y = s.levelBox[i * 2 + 1];
    for (auto& port : levels[i].ports) port.hint = s.portHint[p++];
  }
  return s.total;
}

inline void extendPort(Port& port, const std::vector<double>& pts, bool atStart, double boxW, double boxH) noexcept;

inline void layoutSingleLevel(Level& level, std::vector<CNode>& nodes, std::vector<CEdge>& edges, const LayeredOptions& style, bool keyed, const std::vector<double>& keys) noexcept {
  const auto& dir = level.dir;
  bool sideways = dir == "LR" || dir == "RL";
  bool rev = dir == "BT" || dir == "RL";
  double revSign = rev ? -1.0 : 1.0;
  Side sSide = startSide(dir);
  Side eSide = opposite(sSide);

  int cCount = static_cast<int>(level.children.size());
  Pele::FlatMap<int, int> childIndex;
  for (int i = 0; i < cCount; i++) childIndex[level.children[i]] = i;

  std::vector<LNode> lnodes;
  for (int i = 0; i < cCount; i++) {
    const auto& c = nodes[level.children[i]];
    LNode ln;
    ln.w = sideways ? c.h : c.w;
    ln.h = sideways ? c.w : c.h;
    ln.span = sideways ? c.sidewaysSpan : c.span;
    ln.kind = LKind::Node;
    ln.key = keyed ? keys[level.children[i]] : std::numeric_limits<double>::quiet_NaN();
    ln.pin = c.pin;
    lnodes.push_back(ln);
  }

  for (auto& port : level.ports) {
    port.side = port.out ? eSide : sSide;
  }

  if (level.id >= 0 && !level.ports.empty()) {
    std::vector<LNode> rnodes;
    for (int i = 0; i < cCount; i++) {
      LNode rn;
      rn.w = 1;
      rn.h = 1;
      rn.kind = LKind::Node;
      rnodes.push_back(rn);
    }
    std::vector<LEdge> redges;
    for (const auto& le : level.edges) {
      if (le.fromPort >= 0 || le.toPort >= 0) continue;
      LEdge re;
      re.tail = childIndex[le.from];
      re.head = childIndex[le.to];
      re.minlen = le.minlen;
      re.weight = le.weight;
      redges.push_back(re);
    }
    orient(rnodes, redges);
    rank(rnodes, redges);
    int maxR = 0;
    for (int i = 0; i < cCount; i++) if (rnodes[i].rank > maxR) maxR = rnodes[i].rank;
    for (auto& port : level.ports) {
      int r = rnodes[childIndex[port.inner]].rank;
      if (port.out && r == 0 && maxR > 0) port.side = sSide;
      else if (!port.out && r == maxR && maxR > 0) port.side = eSide;
    }
  }

  for (int p = 0; p < static_cast<int>(level.ports.size()); p++) {
    const auto& port = level.ports[p];
    LNode ln;
    ln.w = 0;
    ln.h = 0;
    ln.kind = port.side == sSide ? LKind::StartPort : LKind::EndPort;
    ln.key = keyed ? (std::isnan(port.hint) ? keys[port.outer] : std::numeric_limits<double>::quiet_NaN()) : std::numeric_limits<double>::quiet_NaN();
    ln.hint = port.hint;
    lnodes.push_back(ln);
  }

  std::vector<LEdge> ledges;
  for (const auto& le : level.edges) {
    const auto& orig = edges[le.e];
    int tail = le.fromPort >= 0 ? cCount + le.fromPort : childIndex[le.from];
    int head = le.toPort >= 0 ? cCount + le.toPort : childIndex[le.to];
    double tailDx = le.fromPort >= 0 ? 0.0 : (sideways ? orig.srcDy : orig.srcDx);
    double headDx = le.toPort >= 0 ? 0.0 : (sideways ? orig.dstDy : orig.dstDx);
    bool tailFixed = le.fromPort < 0 && orig.srcFixed;
    bool headFixed = le.toPort < 0 && orig.dstFixed;
    if (lnodes[tail].kind == LKind::EndPort || lnodes[head].kind == LKind::StartPort) {
      LEdge e;
      e.tail = head;
      e.head = tail;
      e.minlen = le.minlen;
      e.weight = le.weight;
      e.tailDx = headDx;
      e.headDx = tailDx;
      e.tailFixed = headFixed;
      e.headFixed = tailFixed;
      e.labelW = sideways ? le.labelH : le.labelW;
      e.labelH = sideways ? le.labelW : le.labelH;
      ledges.push_back(e);
    } else {
      LEdge e;
      e.tail = tail;
      e.head = head;
      e.minlen = le.minlen;
      e.weight = le.weight;
      e.tailDx = tailDx;
      e.headDx = headDx;
      e.tailFixed = tailFixed;
      e.headFixed = headFixed;
      e.labelW = sideways ? le.labelH : le.labelW;
      e.labelH = sideways ? le.labelW : le.labelH;
      ledges.push_back(e);
    }
  }

  auto res = layoutLayered(lnodes, ledges, style);
  double width = res.width;
  double height = res.height;
  for (size_t k = 0; k < level.edges.size(); k++) {
    const auto& le = level.edges[k];
    const auto& l = ledges[k];
    auto& orig = edges[le.e];
    bool flipped = lnodes[le.fromPort >= 0 ? cCount + le.fromPort : childIndex[le.from]].kind == LKind::EndPort ||
                   lnodes[le.toPort >= 0 ? cCount + le.toPort : childIndex[le.to]].kind == LKind::StartPort;
    double srcAcross = flipped ? l.headDx : l.tailDx;
    double dstAcross = flipped ? l.tailDx : l.headDx;
    double srcAlong = (flipped != l.reversed ? -1.0 : 1.0) * revSign;
    double dstAlong = -srcAlong;
    if (le.fromPort < 0 && !orig.srcFixed) {
      orig.srcDx = sideways ? srcAlong : srcAcross;
      orig.srcDy = sideways ? srcAcross : srcAlong;
    }
    if (le.toPort < 0 && !orig.dstFixed) {
      orig.dstDx = sideways ? dstAlong : dstAcross;
      orig.dstDy = sideways ? dstAcross : dstAlong;
    }
  }
  toLocal(dir, lnodes, ledges, width, height);

  double innerW = sideways ? height : width;
  double innerH = sideways ? width : height;
  double padTop = 0, padRight = 0, padBottom = 0, padLeft = 0;
  if (level.id >= 0) {
    const auto& g = nodes[level.id];
    padTop = g.padTop;
    padRight = g.padRight;
    padBottom = g.padBottom;
    padLeft = g.padLeft;
  }
  double shiftX = padLeft;
  double shiftY = padTop;
  double boxW = innerW + padLeft + padRight;
  double boxH = innerH + padTop + padBottom;
  if (level.id >= 0) {
    auto& g = nodes[level.id];
    if (g.w > boxW) {
      shiftX += (g.w - boxW) / 2.0;
      boxW = g.w;
    }
    if (g.h > boxH) {
      shiftY += (g.h - boxH) / 2.0;
      boxH = g.h;
    }
    g.w = boxW;
    g.h = boxH;
  }
  level.innerW = boxW;
  level.innerH = boxH;
  level.shiftX = shiftX;
  level.shiftY = shiftY;

  for (int i = 0; i < cCount; i++) {
    int cid = level.children[i];
    nodes[cid].x = lnodes[i].x + shiftX;
    nodes[cid].y = lnodes[i].y + shiftY;
  }

  for (size_t k = 0; k < level.edges.size(); k++) {
    const auto& le = level.edges[k];
    const auto& l = ledges[k];
    std::vector<double> pts = l.points;
    for (size_t i = 0; i < pts.size(); i += 3) {
      pts[i] += shiftX;
      pts[i + 1] += shiftY;
    }
    int fromNode = le.fromPort >= 0 ? cCount + le.fromPort : childIndex[le.from];
    int toNode = le.toPort >= 0 ? cCount + le.toPort : childIndex[le.to];
    if (lnodes[fromNode].kind == LKind::EndPort || lnodes[toNode].kind == LKind::StartPort) {
      for (int i = 0, j = static_cast<int>(pts.size()) - 3; i < j; i += 3, j -= 3) {
        std::swap(pts[i], pts[j]);
        std::swap(pts[i + 1], pts[j + 1]);
        std::swap(pts[i + 2], pts[j + 2]);
      }
    }
    if (le.labelW > 0) {
      edges[le.e].labelX = l.labelX + shiftX;
      edges[le.e].labelY = l.labelY + shiftY;
    }
    if (le.fromPort >= 0) {
      auto& port = level.ports[le.fromPort];
      extendPort(port, pts, true, boxW, boxH);
    } else if (le.toPort >= 0) {
      auto& port = level.ports[le.toPort];
      extendPort(port, pts, false, boxW, boxH);
    } else {
      level.routes[le.e] = std::move(pts);
    }
  }
}

inline void extendPort(Port& port, const std::vector<double>& pts, bool atStart, double boxW, double boxH) noexcept {
  int end = atStart ? 0 : static_cast<int>(pts.size()) - 3;
  double x = pts[end];
  double y = pts[end + 1];
  double axis = pts[end + 2];
  double bx = x;
  double by = y;
  if (port.side == Side::Top) by = 0;
  else if (port.side == Side::Bottom) by = boxH;
  else if (port.side == Side::Left) bx = 0;
  else bx = boxW;
  port.px = bx;
  port.py = by;

  std::vector<double> out;
  if (atStart) {
    out.push_back(bx);
    out.push_back(by);
    out.push_back(axis);
    for (double v : pts) out.push_back(v);
  } else {
    out = pts;
    out.push_back(bx);
    out.push_back(by);
    out.push_back(axis);
  }
  port.route = std::move(out);
}

inline void shiftPortRoute(std::vector<double>& route, int end, int adj, bool vertical, double np) noexcept {
  if (vertical) {
    double oldX = route[end];
    if (std::abs(route[adj] - oldX) < 0.01) route[adj] = np;
    route[end] = np;
  } else {
    double oldY = route[end + 1];
    if (std::abs(route[adj + 1] - oldY) < 0.01) route[adj + 1] = np;
    route[end + 1] = np;
  }
}

inline void adjustBorderPorts(Level& childLv, const CNode& childNode, Level& parentLv, std::vector<CEdge>& edges) noexcept {
  if (childLv.ports.empty()) return;
  double gx = childNode.x - childNode.w / 2.0;
  double gy = childNode.y - childNode.h / 2.0;

  for (int sideInt = 0; sideInt < 4; sideInt++) {
    Side side = static_cast<Side>(sideInt);
    std::vector<int> onSide;
    for (int pi = 0; pi < static_cast<int>(childLv.ports.size()); pi++) {
      if (childLv.ports[pi].side == side) onSide.push_back(pi);
    }
    if (onSide.empty()) continue;
    bool vertical = side == Side::Top || side == Side::Bottom;
    double span = vertical ? childNode.w : childNode.h;
    double origin = vertical ? gx : gy;

    struct Desired {
      int portIdx;
      double cur;
      double want;
      bool hasPRoute;
      int eKey;
      bool atStart;
    };
    std::vector<Desired> desired;
    for (int pi : onSide) {
      auto& port = childLv.ports[pi];
      double cur = vertical ? port.px : port.py;
      for (const auto& le : parentLv.edges) {
        if (le.e != port.e) continue;
        bool atStart = le.from == childLv.id;
        if (!atStart && le.to != childLv.id) continue;
        if (atStart != port.out) continue;
        bool hasPRoute = false;
        int eKey = le.e;
        const std::vector<double>* pRoutePtr = nullptr;
        auto itR = parentLv.routes.find(le.e);
        if (itR != parentLv.routes.end()) {
          hasPRoute = true;
          pRoutePtr = &itR->second;
        } else if (le.fromPort >= 0) {
          pRoutePtr = &parentLv.ports[le.fromPort].route;
          eKey = -1 - le.fromPort;
          hasPRoute = true;
        } else if (le.toPort >= 0) {
          pRoutePtr = &parentLv.ports[le.toPort].route;
          eKey = -1 - le.toPort;
          hasPRoute = true;
        }
        if (!hasPRoute || !pRoutePtr || pRoutePtr->size() < 6) continue;
        const auto& pRoute = *pRoutePtr;
        int nearIdx = atStart ? 3 : static_cast<int>(pRoute.size()) - 6;
        double outer = (vertical ? pRoute[nearIdx] : pRoute[nearIdx + 1]) - origin;
        int cNear = port.out ? static_cast<int>(port.route.size()) - 6 : 3;
        double inner = vertical ? port.route[cNear] : port.route[cNear + 1];
        double want = (outer + inner) / 2.0;
        desired.push_back({pi, cur, want, true, eKey, atStart});
      }
    }
    if (desired.empty()) continue;

    std::stable_sort(desired.begin(), desired.end(), [](const Desired& a, const Desired& b) {
      return a.cur < b.cur;
    });
    double lo = std::min(SIDE_PORT_PAD, span / 2.0);
    double hi = std::max(span - SIDE_PORT_PAD, span / 2.0);
    int k = static_cast<int>(desired.size());
    double minGap = k > 1 ? std::min(16.0, (hi - lo) / (k - 1)) : 0.0;

    std::vector<double> pos(k, 0);
    for (int i = 0; i < k; i++) pos[i] = std::max(lo + i * minGap, std::min(hi - (k - 1 - i) * minGap, desired[i].want));
    for (int i = 1; i < k; i++) if (pos[i] < pos[i - 1] + minGap) pos[i] = pos[i - 1] + minGap;
    for (int i = k - 2; i >= 0; i--) if (pos[i] > pos[i + 1] - minGap) pos[i] = pos[i + 1] - minGap;

    for (int i = 0; i < k; i++) {
      auto& d = desired[i];
      auto& port = childLv.ports[d.portIdx];
      double np = pos[i];
      if (vertical) port.px = np;
      else port.py = np;

      std::vector<double>& pRoute = d.eKey >= 0 ? parentLv.routes[d.eKey] : parentLv.ports[-1 - d.eKey].route;
      int cEnd = port.out ? static_cast<int>(port.route.size()) - 3 : 0;
      int cAdj = port.out ? static_cast<int>(port.route.size()) - 6 : 3;
      shiftPortRoute(port.route, cEnd, cAdj, vertical, np);

      int pEnd = d.atStart ? 0 : static_cast<int>(pRoute.size()) - 3;
      int pAdj = d.atStart ? 3 : static_cast<int>(pRoute.size()) - 6;
      double pNew = origin + np;
      if (pRoute.size() == 6 && edges[port.e].labelW > 0) {
        int other = d.atStart ? 3 : 0;
        double otherP = vertical ? pRoute[other] : pRoute[other + 1];
        if (std::abs(otherP - pNew) < 1.0) shiftPortRoute(pRoute, other, pEnd, vertical, pNew);
      }
      shiftPortRoute(pRoute, pEnd, pAdj, vertical, pNew);
    }
  }
}

inline void rerouteSideEntries(const Level& childLv, const CNode& childNode, Level& parentLv, const std::vector<CNode>& nodes) noexcept {
  double gx0 = childNode.x - childNode.w / 2.0;
  double gx1 = childNode.x + childNode.w / 2.0;
  double gy0 = childNode.y - childNode.h / 2.0;
  double gy1 = childNode.y + childNode.h / 2.0;
  Side sSide = startSide(childLv.dir);
  Side eSide = opposite(sSide);

  for (const auto& le : parentLv.edges) {
    auto itR = parentLv.routes.find(le.e);
    if (itR == parentLv.routes.end()) continue;
    auto& pRoute = itR->second;
    bool atStart = le.from == childLv.id;
    bool atEnd = le.to == childLv.id;
    if (!atStart && !atEnd) continue;
    int portKey = le.e * 2 + (atStart ? 0 : 1);
    auto itP = childLv.portOf.find(portKey);
    if (itP == childLv.portOf.end()) continue;
    const auto& port = childLv.ports[itP->second];
    if (le.from >= 0 && le.to >= 0) {
      int otherIdx = atStart ? le.to : le.from;
      const auto& other = nodes[otherIdx];
      Side expected = atStart ? eSide : sSide;
      if (port.side == expected) {
        bool behind = (expected == Side::Bottom && other.y < gy0) ||
                      (expected == Side::Top && other.y > gy1) ||
                      (expected == Side::Right && other.x < gx0) ||
                      (expected == Side::Left && other.x > gx1);
        if (behind) {
          int end = atStart ? 0 : static_cast<int>(pRoute.size()) - 3;
          double px = gx0 + port.px;
          double py = gy0 + port.py;
          double axis = pRoute[end + 2];
          double tx = px, ty = py;
          if (expected == Side::Bottom) ty = gy1 + TURN_DIST;
          else if (expected == Side::Top) ty = gy0 - TURN_DIST;
          else if (expected == Side::Right) tx = gx1 + TURN_DIST;
          else tx = gx0 - TURN_DIST;
          if (atStart) {
            pRoute[0] = px; pRoute[1] = py;
            pRoute.insert(pRoute.begin() + 3, {tx, ty, axis});
          } else {
            pRoute[end] = tx; pRoute[end + 1] = ty;
            pRoute.push_back(px); pRoute.push_back(py); pRoute.push_back(axis);
          }
        }
      }
    }
    int pEnd = atStart ? 0 : static_cast<int>(pRoute.size()) - 3;
    pRoute[pEnd] = gx0 + port.px;
    pRoute[pEnd + 1] = gy0 + port.py;
  }
}

inline std::vector<double> flatOrder(const std::vector<CNode>& nodes, const std::vector<CEdge>& edges, const std::vector<int>& loops, const LayeredOptions& style) noexcept {
  int n = static_cast<int>(nodes.size());
  std::vector<int> leafIdx(n, -1);
  std::vector<int> groupIdx(n, -1);
  std::vector<LNode> fnodes;
  std::vector<int> groupParent;
  for (int i = 0; i < n; i++) {
    if (nodes[i].isGroup) {
      groupIdx[i] = static_cast<int>(groupParent.size());
      groupParent.push_back(-1);
    } else {
      leafIdx[i] = static_cast<int>(fnodes.size());
      LNode ln;
      ln.w = nodes[i].w;
      ln.h = nodes[i].h;
      ln.kind = LKind::Node;
      ln.pin = nodes[i].pin;
      fnodes.push_back(ln);
    }
  }
  if (fnodes.size() < 2) return {};
  std::vector<int> groupOf(fnodes.size(), -1);
  for (int i = 0; i < n; i++) {
    int p = nodes[i].parent;
    int g = p >= 0 ? groupIdx[p] : -1;
    if (nodes[i].isGroup) groupParent[groupIdx[i]] = g;
    else groupOf[leafIdx[i]] = g;
  }
  Pele::FlatSet<int> loopSet(loops.begin(), loops.end());
  std::vector<LEdge> fedges;
  for (int ei = 0; ei < static_cast<int>(edges.size()); ei++) {
    if (loopSet.count(ei)) continue;
    const auto& e = edges[ei];
    int tail = leafIdx[e.src];
    int head = leafIdx[e.dst];
    if (tail < 0 || head < 0) continue;
    LEdge le;
    le.tail = tail;
    le.head = head;
    le.minlen = e.minlen;
    le.weight = e.weight;
    fedges.push_back(le);
  }
  LayeredOptions fopt = style;
  fopt.orderOnly = true;
  if (!groupParent.empty()) {
    fopt.groups = LayeredGroups{groupOf, groupParent};
  }
  auto res = layoutLayered(fnodes, fedges, fopt);
  std::vector<double> key(n + 2, std::numeric_limits<double>::quiet_NaN());
  for (int i = 0; i < n; i++) {
    if (!nodes[i].isGroup) key[i] = fnodes[leafIdx[i]].x;
  }
  std::function<double(int)> fill = [&](int id) -> double {
    if (!std::isnan(key[id])) return key[id];
    double sum = 0;
    int count = 0;
    for (int i = 0; i < n; i++) {
      if (nodes[i].parent == id) {
        double k = fill(i);
        if (!std::isnan(k)) {
          sum += k;
          count++;
        }
      }
    }
    key[id] = count > 0 ? sum / count : std::numeric_limits<double>::quiet_NaN();
    return key[id];
  };
  for (int i = 0; i < n; i++) if (nodes[i].isGroup) fill(i);
  key[n] = res.crossings;
  key[n + 1] = res.declared;
  return key;
}

} // namespace detail_compound

inline Size layoutCompound(std::vector<CNode>& nodes, std::vector<CEdge>& edges, const CompoundOptions& opt = {}) noexcept {
  using namespace detail_compound;
  int n = static_cast<int>(nodes.size());
  if (n == 0) return {0, 0};

  auto depth = [&](int i) -> int {
    int d = 0;
    for (int p = nodes[i].parent; p >= 0; p = nodes[p].parent) d++;
    return d;
  };

  auto lca = [&](int a, int b) -> int {
    int da = depth(a);
    int db = depth(b);
    int x = a, y = b;
    while (da > db) { x = nodes[x].parent; da--; }
    while (db > da) { y = nodes[y].parent; db--; }
    while (x != y) { x = nodes[x].parent; y = nodes[y].parent; }
    return x;
  };

  auto childTowards = [&](int anc, int desc) -> int {
    int p = desc;
    while (nodes[p].parent != anc) p = nodes[p].parent;
    return p;
  };

  auto isAncestor = [&](int a, int b) -> bool {
    for (int p = nodes[b].parent; p >= 0; p = nodes[p].parent) {
      if (p == a) return true;
    }
    return false;
  };

  Pele::FlatMap<int, int> levelIdxOf;
  std::vector<Level> levels;
  auto ensureLevel = [&](int id) -> int {
    auto it = levelIdxOf.find(id);
    if (it != levelIdxOf.end()) return it->second;
    std::string dir = opt.dir;
    for (int p = id; p >= 0; p = nodes[p].parent) {
      if (nodes[p].dir.has_value() && !nodes[p].dir->empty()) {
        dir = *nodes[p].dir;
        break;
      }
    }
    int idx = static_cast<int>(levels.size());
    Level lv;
    lv.id = id;
    lv.dir = dir;
    levels.push_back(std::move(lv));
    levelIdxOf[id] = idx;
    return idx;
  };
  ensureLevel(-1);
  for (int i = 0; i < n; i++) {
    if (nodes[i].isGroup) ensureLevel(i);
    levels[ensureLevel(nodes[i].parent)].children.push_back(i);
  }

  std::vector<int> loops;
  std::vector<int> stray;
  for (int ei = 0; ei < static_cast<int>(edges.size()); ei++) {
    const auto& e = edges[ei];
    if (e.src == e.dst) {
      loops.push_back(ei);
      continue;
    }
    if (isAncestor(e.src, e.dst) || isAncestor(e.dst, e.src)) {
      stray.push_back(ei);
      continue;
    }
    int top = lca(e.src, e.dst);
    int topSrc = childTowards(top, e.src);
    int topDst = childTowards(top, e.dst);

    for (int item = e.src; nodes[item].parent != top; item = nodes[item].parent) {
      int g = nodes[item].parent;
      auto& lv = levels[ensureLevel(g)];
      int pIdx = static_cast<int>(lv.ports.size());
      Port port;
      port.id = pIdx;
      port.e = ei;
      port.out = true;
      port.inner = item;
      port.outer = topDst;
      port.weight = e.weight;
      lv.ports.push_back(port);
      lv.portOf[ei * 2] = pIdx;
      LevelEdge le;
      le.e = ei;
      le.from = item;
      le.to = -1;
      le.fromPort = -1;
      le.toPort = pIdx;
      le.minlen = 1;
      le.weight = e.weight;
      lv.edges.push_back(le);
    }

    for (int item = e.dst; nodes[item].parent != top; item = nodes[item].parent) {
      int g = nodes[item].parent;
      auto& lv = levels[ensureLevel(g)];
      int pIdx = static_cast<int>(lv.ports.size());
      Port port;
      port.id = pIdx;
      port.e = ei;
      port.out = false;
      port.inner = item;
      port.outer = topSrc;
      port.weight = e.weight;
      lv.ports.push_back(port);
      lv.portOf[ei * 2 + 1] = pIdx;
      LevelEdge le;
      le.e = ei;
      le.from = -1;
      le.to = item;
      le.fromPort = pIdx;
      le.toPort = -1;
      le.minlen = 1;
      le.weight = e.weight;
      lv.edges.push_back(le);
    }

    auto& topLv = levels[ensureLevel(top)];
    LevelEdge le;
    le.e = ei;
    le.from = topSrc;
    le.to = topDst;
    le.fromPort = -1;
    le.toPort = -1;
    le.minlen = e.minlen;
    le.weight = e.weight;
    le.labelW = e.labelW;
    le.labelH = e.labelH;
    topLv.edges.push_back(le);
  }

  for (int ei : loops) {
    auto& e = edges[ei];
    auto& node = nodes[e.src];
    double w = LOOP_W + (e.labelW > 0 ? e.labelW + 8.0 : 0.0);
    node.w += w;
    node.h = std::max(node.h, LOOP_H * 2.0);
  }

  bool hasGroups = levels.size() > 1;
  bool canFlat = hasGroups && edges.size() <= MAX_CROSSING_EDGES;
  const auto& rootDir = opt.dir;
  for (const auto& lv : levels) {
    if (lv.dir != rootDir) {
      canFlat = false;
      break;
    }
  }
  std::vector<double> flat = canFlat ? flatOrder(nodes, edges, loops, styleOf(opt)) : std::vector<double>{};
  double flatCrossings = !flat.empty() ? flat[n] : 0;
  bool ordered = !flat.empty() ? flat[n + 1] > 0 : true;

  std::vector<int> orderIdx;
  std::function<void(int)> visit = [&](int id) {
    int lIdx = levelIdxOf[id];
    for (int c : levels[lIdx].children) {
      if (nodes[c].isGroup) visit(c);
    }
    orderIdx.push_back(lIdx);
  };
  visit(-1);

  std::vector<CNode> baseSize(n);
  for (int i = 0; i < n; i++) baseSize[i] = nodes[i];
  bool hasKeyed = !flat.empty();
  Size total{0, 0};

  auto place = [&]() {
    for (int i = 0; i < n; i++) {
      if (nodes[i].isGroup) {
        nodes[i].w = baseSize[i].w;
        nodes[i].h = baseSize[i].h;
      }
    }
    auto style = styleOf(opt);
    for (int lIdx : orderIdx) {
      layoutSingleLevel(levels[lIdx], nodes, edges, style, hasKeyed, flat);
    }
    for (int lIdx : orderIdx) {
      auto& level = levels[lIdx];
      for (int cid : level.children) {
        if (!nodes[cid].isGroup) continue;
        auto& childLv = levels[levelIdxOf[cid]];
        adjustBorderPorts(childLv, nodes[cid], level, edges);
        rerouteSideEntries(childLv, nodes[cid], level, nodes);
      }
    }

    auto& root = levels[levelIdxOf[-1]];
    root.x = 0;
    root.y = 0;
    total = {root.innerW, root.innerH};
    for (int i = static_cast<int>(orderIdx.size()) - 1; i >= 0; i--) {
      auto& level = levels[orderIdx[i]];
      for (int cid : level.children) {
        auto& c = nodes[cid];
        c.x += level.x;
        c.y += level.y;
        if (c.isGroup) {
          auto& cLv = levels[levelIdxOf[cid]];
          cLv.x = c.x - c.w / 2.0;
          cLv.y = c.y - c.h / 2.0;
        }
      }
    }

    for (int lIdx : orderIdx) {
      auto& level = levels[lIdx];
      for (const auto& le : level.edges) {
        if (le.fromPort >= 0 || le.toPort >= 0) continue;
        auto& e = edges[le.e];
        std::vector<double> route;
        std::vector<int> outward;
        for (int item = e.src; nodes[item].parent != level.id; item = nodes[item].parent) {
          outward.push_back(levelIdxOf[nodes[item].parent]);
        }
        for (int oIdx : outward) {
          const auto& lv = levels[oIdx];
          appendRoute(route, lv.ports[lv.portOf.at(le.e * 2)].route, lv.x, lv.y);
        }
        appendRoute(route, level.routes.at(le.e), level.x, level.y);
        std::vector<int> inward;
        for (int item = e.dst; nodes[item].parent != level.id; item = nodes[item].parent) {
          inward.push_back(levelIdxOf[nodes[item].parent]);
        }
        for (int k = static_cast<int>(inward.size()) - 1; k >= 0; k--) {
          const auto& lv = levels[inward[k]];
          appendRoute(route, lv.ports[lv.portOf.at(le.e * 2 + 1)].route, lv.x, lv.y);
        }
        e.route = std::move(route);
        if (e.labelW > 0) {
          e.labelX += level.x;
          e.labelY += level.y;
        }
      }
    }
  };

  auto style = styleOf(opt);
  place();
  int crossings = -1;
  if (!flat.empty()) {
    crossings = routeCrossings(nodes, edges, style);
    if (crossings > flatCrossings * FLAT_SLACK + FLAT_SPARE) {
      auto first = snapshot(nodes, edges, levels, total);
      hasKeyed = false;
      place();
      int own = routeCrossings(nodes, edges, style);
      if (own >= crossings) {
        hasKeyed = true;
        total = restore(first, nodes, edges, levels);
      } else {
        crossings = own;
      }
    }
  }

  if (hasKeyed == (!flat.empty()) && ordered) {
    bool again = false;
    for (int lIdx : orderIdx) {
      auto& level = levels[lIdx];
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
        for (int k = 0; k < static_cast<int>(route.size()) && (at < 0 || port.out); k += 3) {
          if (std::abs(route[k] - bx) < 0.01 && std::abs(route[k + 1] - by) < 0.01) at = k;
        }
        if (at < 0) continue;
        int a = vertical ? 0 : 1;
        int next = port.out ? at + 3 : at - 3;
        while (next >= 0 && next < static_cast<int>(route.size()) && std::abs(route[next + a] - route[at + a]) < IN_LINE) {
          next += port.out ? 3 : -3;
        }
        if (next < 0 || next >= static_cast<int>(route.size())) continue;
        port.hint = route[next + a] - (vertical ? box.x - box.w / 2.0 : box.y - box.h / 2.0);
        seen.push_back({vertical ? port.px : port.py, port.hint});
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
        auto first = snapshot(nodes, edges, levels, total);
        place();
        if (routeCrossings(nodes, edges, style) >= before) {
          total = restore(first, nodes, edges, levels);
        }
      }
    }
  }

  for (int ei : stray) {
    auto& e = edges[ei];
    const auto& a = nodes[e.src];
    const auto& b = nodes[e.dst];
    e.route = {a.x, a.y, 0, b.x, b.y, 0};
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
// Port of src/layout/components.ts
// ============================================================================

inline std::vector<std::vector<int>> components(int count, const std::vector<std::pair<int, int>>& links) noexcept {
  std::vector<int> parent(count);
  for (int i = 0; i < count; i++) parent[i] = i;
  auto find = [&](int x) {
    while (parent[x] != x) {
      parent[x] = parent[parent[x]];
      x = parent[x];
    }
    return x;
  };
  for (const auto& [a, b] : links) {
    int ra = find(a);
    int rb = find(b);
    if (ra != rb) {
      if (ra < rb) parent[rb] = ra;
      else parent[ra] = rb;
    }
  }
  Pele::FlatMap<int, int> groupIdx;
  std::vector<std::vector<int>> groups;
  for (int i = 0; i < count; i++) {
    int r = find(i);
    auto it = groupIdx.find(r);
    if (it != groupIdx.end()) groups[it->second].push_back(i);
    else {
      groupIdx[r] = static_cast<int>(groups.size());
      groups.push_back({i});
    }
  }
  return groups;
}

struct ComponentItem {
  double width = 0;
  double height = 0;
  double x = 0;
  double y = 0;
};

inline Size packComponents(std::vector<ComponentItem>& items, double gap = 48, double targetAspect = 1.4) noexcept {
  if (items.empty()) return {0, 0};
  if (items.size() == 1) {
    items[0].x = 0;
    items[0].y = 0;
    return {items[0].width, items[0].height};
  }
  double totalArea = 0;
  double widest = 0;
  for (const auto& it : items) {
    totalArea += (it.width + gap) * (it.height + gap);
    if (it.width > widest) widest = it.width;
  }
  double targetW = std::max(widest, std::sqrt(totalArea * targetAspect));
  double x = 0, y = 0, rowH = 0, maxW = 0;
  for (auto& it : items) {
    if (x > 0 && x + it.width > targetW) {
      x = 0;
      y += rowH + gap;
      rowH = 0;
    }
    it.x = x;
    it.y = y;
    x += it.width + gap;
    if (x - gap > maxW) maxW = x - gap;
    if (it.height > rowH) rowH = it.height;
  }
  return {maxW, y + rowH};
}

// ============================================================================
// Port of src/layout/lanes.ts
// ============================================================================

constexpr double LANE_GUTTER = 28;
constexpr double LANE_GAP = 14;
constexpr double LANE_MARGIN = 16;

struct Lane {
  double x = 0;
  double y = 0;
  double w = 0;
  double h = 0;
};

struct LanedGraph {
  std::vector<Lane> lanes;
  double width = 0;
  double height = 0;
};

inline LanedGraph layoutLanes(
  std::vector<CNode>& nodes,
  std::vector<CEdge>& edges,
  const std::vector<int>& laneOf,
  int laneCount,
  const CompoundOptions& opt
) {
  using namespace detail_compound;
  bool sideways = opt.dir == "LR" || opt.dir == "RL";
  bool rev = opt.dir == "BT" || opt.dir == "RL";
  int n = static_cast<int>(nodes.size());
  int m = static_cast<int>(edges.size());

  std::vector<int> loops;
  for (int ei = 0; ei < m; ei++) if (edges[ei].src == edges[ei].dst) loops.push_back(ei);
  for (int ei : loops) {
    const auto& e = edges[ei];
    auto& node = nodes[e.src];
    node.w += LOOP_W + (e.labelW > 0 ? e.labelW + 8.0 : 0.0);
    node.h = std::max(node.h, LOOP_H * 2.0);
  }

  Pele::FlatSet<int> loopSet(loops.begin(), loops.end());
  struct LaneGroupInfo {
    int id;
    int lane;
    std::vector<int> members;
  };
  std::vector<LaneGroupInfo> groups;
  Pele::FlatMap<int, int> groupSlot;
  for (int i = 0; i < n; i++) {
    if (!nodes[i].isGroup || laneOf[i] < 0) continue;
    groupSlot[i] = static_cast<int>(groups.size());
    groups.push_back({i, laneOf[i], {}});
  }
  std::vector<int> groupOf(n, -1);
  for (int i = 0; i < n; i++) {
    if (nodes[i].isGroup) continue;
    for (int p = nodes[i].parent; p >= 0; p = nodes[p].parent) {
      auto it = groupSlot.find(p);
      if (it != groupSlot.end()) {
        groupOf[i] = it->second;
        break;
      }
    }
    for (int p = nodes[i].parent; p >= 0; p = nodes[p].parent) {
      auto it = groupSlot.find(p);
      if (it != groupSlot.end()) groups[it->second].members.push_back(i);
    }
  }
  std::vector<int> groupParent(groups.size(), -1);
  for (size_t g = 0; g < groups.size(); g++) {
    for (int p = nodes[groups[g].id].parent; p >= 0; p = nodes[p].parent) {
      auto it = groupSlot.find(p);
      if (it != groupSlot.end()) {
        groupParent[g] = it->second;
        break;
      }
    }
  }

  std::vector<int> leafOf(n, -1);
  std::vector<int> nodeOf;
  std::vector<LNode> allNodes;
  std::vector<int> leafGroup;
  for (int i = 0; i < n; i++) {
    if (nodes[i].isGroup) continue;
    leafOf[i] = static_cast<int>(allNodes.size());
    nodeOf.push_back(i);
    LNode ln;
    ln.w = sideways ? nodes[i].h : nodes[i].w;
    ln.h = sideways ? nodes[i].w : nodes[i].h;
    ln.span = sideways ? nodes[i].sidewaysSpan : nodes[i].span;
    ln.kind = LKind::Node;
    allNodes.push_back(ln);
    leafGroup.push_back(groupOf[i]);
  }
  std::vector<int> edgeIndex;
  std::vector<LEdge> allEdges;
  for (int ei = 0; ei < m; ei++) {
    if (loopSet.count(ei)) continue;
    const auto& e = edges[ei];
    int tail = leafOf[e.src];
    int head = leafOf[e.dst];
    if (tail < 0 || head < 0) continue;
    edgeIndex.push_back(ei);
    LEdge le;
    le.tail = tail;
    le.head = head;
    le.minlen = e.minlen;
    le.weight = e.weight;
    le.labelW = sideways ? e.labelH : e.labelW;
    le.labelH = sideways ? e.labelW : e.labelH;
    allEdges.push_back(le);
  }
  orient(allNodes, allEdges);
  rank(allNodes, allEdges, 1);

  if (!groups.empty()) {
    int leaves = static_cast<int>(allNodes.size());
    std::vector<std::vector<int>> members(groups.size());
    for (int li = 0; li < leaves; li++) {
      for (int g = leafGroup[li]; g >= 0; g = groupParent[g]) members[g].push_back(li);
    }
    for (int pass = 0; pass < 4; pass++) {
      bool changed = false;
      for (size_t g = 0; g < groups.size(); g++) {
        const auto& mem = members[g];
        if (mem.empty()) continue;
        std::vector<uint8_t> inGroup(leaves, 0);
        for (int li : mem) inGroup[li] = 1;
        int lo = std::numeric_limits<int>::max();
        int hi = -std::numeric_limits<int>::max();
        for (int li : mem) {
          int r = allNodes[li].rank;
          if (r < lo) lo = r;
          if (r > hi) hi = r;
        }
        std::vector<int> outside;
        for (int li = 0; li < leaves; li++) {
          if (!inGroup[li] && allNodes[li].rank >= lo && allNodes[li].rank <= hi) outside.push_back(li);
        }
        if (outside.empty()) continue;
        std::vector<uint8_t> reachFromGroup(leaves, 0);
        for (int li : mem) reachFromGroup[li] = 1;
        for (bool more = true; more;) {
          more = false;
          for (const auto& e : allEdges) {
            if (reachFromGroup[e.tail] && !reachFromGroup[e.head]) {
              reachFromGroup[e.head] = 1;
              more = true;
            }
          }
        }
        std::vector<uint8_t> canReachGroup(leaves, 0);
        for (int li : mem) canReachGroup[li] = 1;
        for (bool more = true; more;) {
          more = false;
          for (const auto& e : allEdges) {
            if (canReachGroup[e.head] && !canReachGroup[e.tail]) {
              canReachGroup[e.tail] = 1;
              more = true;
            }
          }
        }
        bool conflict = false;
        for (int li : outside) {
          if (reachFromGroup[li] && canReachGroup[li]) {
            conflict = true;
            break;
          }
        }
        if (conflict) continue;
        for (int li : outside) {
          LEdge le;
          le.tail = reachFromGroup[li] ? mem[0] : li;
          le.head = reachFromGroup[li] ? li : mem[0];
          le.minlen = 1;
          le.weight = 0;
          allEdges.push_back(le);
        }
        for (int li : mem) {
          for (int mj : mem) {
            if (li == mj) continue;
            if (!reachFromGroup[li] || !canReachGroup[mj]) continue;
            LEdge le;
            le.tail = li;
            le.head = mj;
            le.minlen = 0;
            le.weight = 4;
            allEdges.push_back(le);
          }
        }
        changed = true;
      }
      if (!changed) break;
      rank(allNodes, allEdges, 1);
    }
    allEdges.resize(edgeIndex.size());
  }

  std::vector<int> baseRank(allNodes.size(), 0);
  int maxBaseRank = 0;
  for (size_t i = 0; i < allNodes.size(); i++) {
    baseRank[i] = allNodes[i].rank;
    if (baseRank[i] > maxBaseRank) maxBaseRank = baseRank[i];
  }

  std::vector<double> padBefore(maxBaseRank + 1, 0);
  std::vector<double> padAfter(maxBaseRank + 1, 0);
  for (const auto& g : groups) {
    if (g.members.empty()) continue;
    int lo = std::numeric_limits<int>::max();
    int hi = -std::numeric_limits<int>::max();
    for (int idx : g.members) {
      int r = baseRank[leafOf[idx]];
      if (r < lo) lo = r;
      if (r > hi) hi = r;
    }
    const auto& c = nodes[g.id];
    double startPad = sideways ? (rev ? c.padRight : c.padLeft) : (rev ? c.padBottom : c.padTop);
    double endPad = sideways ? (rev ? c.padLeft : c.padRight) : (rev ? c.padTop : c.padBottom);
    padBefore[lo] = std::max(padBefore[lo], startPad);
    padAfter[hi] = std::max(padAfter[hi], endPad);
  }
  std::vector<double> extraGap(maxBaseRank, 0);
  for (int r = 0; r < maxBaseRank; r++) extraGap[r] = padAfter[r] + padBefore[r + 1];

  int step = 1;
  for (const auto& e : allEdges) {
    if (e.labelW > 0) {
      step = 2;
      break;
    }
  }
  if (step == 1) {
    Pele::FlatSet<int64_t> seen;
    int leaves = static_cast<int>(allNodes.size());
    for (const auto& e : allEdges) {
      int a = std::min(e.tail, e.head);
      int b = std::max(e.tail, e.head);
      int64_t key = static_cast<int64_t>(a) * leaves + b;
      if (seen.count(key)) {
        step = 2;
        break;
      }
      seen.insert(key);
    }
  }

  std::vector<double> across(n, 0);
  std::vector<Lane> lanes(laneCount);
  double cursor = 0;
  for (int lane = 0; lane < laneCount; lane++) {
    std::vector<int> members;
    for (size_t li = 0; li < allNodes.size(); li++) {
      if (laneOf[nodeOf[li]] == lane) members.push_back(static_cast<int>(li));
    }
    std::vector<int> localOf(allNodes.size(), -1);
    std::vector<LNode> lnodes;
    for (int k = 0; k < static_cast<int>(members.size()); k++) {
      localOf[members[k]] = k;
      const auto& a = allNodes[members[k]];
      LNode ln;
      ln.w = a.w;
      ln.h = a.h;
      ln.span = a.span;
      ln.kind = LKind::Node;
      lnodes.push_back(ln);
    }
    std::vector<LEdge> ledges;
    for (const auto& e : allEdges) {
      int lt = localOf[e.tail];
      int lh = localOf[e.head];
      if (lt < 0 || lh < 0) continue;
      LEdge le;
      le.tail = lt;
      le.head = lh;
      le.minlen = std::max(1, baseRank[e.head] - baseRank[e.tail]);
      le.weight = e.weight;
      le.labelW = e.labelW;
      le.labelH = e.labelH;
      ledges.push_back(le);
    }
    std::vector<int> slotOf(groups.size(), -1);
    std::vector<int> lgroupParent;
    for (size_t g = 0; g < groups.size(); g++) {
      if (groups[g].lane != lane) continue;
      slotOf[g] = static_cast<int>(lgroupParent.size());
      lgroupParent.push_back(-1);
    }
    for (size_t g = 0; g < groups.size(); g++) {
      if (slotOf[g] >= 0 && groupParent[g] >= 0) lgroupParent[slotOf[g]] = slotOf[groupParent[g]];
    }
    std::vector<int> lgroupOf;
    if (!lgroupParent.empty()) {
      for (int mIdx : members) {
        int g = leafGroup[mIdx];
        lgroupOf.push_back(g >= 0 ? slotOf[g] : -1);
      }
    }
    LayeredOptions lopt;
    lopt.nodeSep = opt.nodeSep;
    lopt.rankSep = opt.rankSep;
    lopt.edgeSep = opt.edgeSep;
    lopt.portSep = opt.portSep;
    lopt.headRoom = opt.headRoom;
    if (!lgroupOf.empty()) lopt.groups = LayeredGroups{lgroupOf, lgroupParent};
    auto res = layoutLayered(lnodes, ledges, lopt);
    double contentSpan = res.width;
    for (size_t k = 0; k < members.size(); k++) {
      across[nodeOf[members[k]]] = lnodes[k].x;
    }

    double minEdge = 0;
    double maxEdge = contentSpan;
    for (const auto& g : groups) {
      if (g.lane != lane || g.members.empty()) continue;
      double lo = std::numeric_limits<double>::infinity();
      double hi = -std::numeric_limits<double>::infinity();
      for (int idx : g.members) {
        double w = sideways ? nodes[idx].h : nodes[idx].w;
        lo = std::min(lo, across[idx] - w / 2.0);
        hi = std::max(hi, across[idx] + w / 2.0);
      }
      const auto& c = nodes[g.id];
      double padLo = sideways ? c.padTop : c.padLeft;
      double padHi = sideways ? c.padBottom : c.padRight;
      lo -= padLo;
      hi += padHi;
      double minSpan = sideways ? c.h : c.w;
      if (hi - lo < minSpan) {
        double extra = (minSpan - (hi - lo)) / 2.0;
        lo -= extra;
        hi += extra;
      }
      minEdge = std::min(minEdge, lo);
      maxEdge = std::max(maxEdge, hi);
    }

    double inner = std::max(contentSpan, maxEdge - minEdge);
    double span = std::max(140.0, inner + 2.0 * LANE_MARGIN);
    double offset = cursor + (span - (maxEdge - minEdge)) / 2.0 - minEdge;
    for (int mIdx : members) {
      int idx = nodeOf[mIdx];
      across[idx] += offset;
    }
    lanes[lane] = sideways ? Lane{0, cursor, 0, span} : Lane{cursor, 0, span, 0};
    cursor += span;
  }

  for (size_t li = 0; li < allNodes.size(); li++) {
    allNodes[li].pin = laneOf[nodeOf[li]] + 1;
    allNodes[li].key = across[nodeOf[li]];
  }
  LayeredOptions fullOpt;
  fullOpt.nodeSep = opt.nodeSep;
  fullOpt.rankSep = opt.rankSep;
  fullOpt.edgeSep = opt.edgeSep;
  fullOpt.portSep = opt.portSep;
  fullOpt.headRoom = opt.headRoom;
  if (!groups.empty()) fullOpt.groups = LayeredGroups{leafGroup, groupParent};
  auto full = layoutLayered(allNodes, allEdges, fullOpt);

  double globalMin = std::numeric_limits<double>::infinity();
  double globalMax = -std::numeric_limits<double>::infinity();
  for (size_t li = 0; li < allNodes.size(); li++) {
    int idx = nodeOf[li];
    double x = across[idx];
    double w = allNodes[li].w;
    if (x - w / 2.0 < globalMin) globalMin = x - w / 2.0;
    if (x + w / 2.0 > globalMax) globalMax = x + w / 2.0;
  }
  std::vector<double> laneShift(laneCount, 0);
  if (allNodes.size() >= 2 && full.width > 1 && globalMax - globalMin > 1) {
    double scale = (globalMax - globalMin) / full.width;
    for (int lane = 0; lane < laneCount; lane++) {
      double sum = 0;
      int count = 0;
      for (size_t li = 0; li < allNodes.size(); li++) {
        int idx = nodeOf[li];
        if (laneOf[idx] != lane) continue;
        sum += across[idx] - (globalMin + allNodes[li].x * scale);
        count++;
      }
      if (count > 0) laneShift[lane] = sum / count;
    }
  }

  std::vector<double> rankPos(maxBaseRank + 1, 0);
  for (size_t li = 0; li < allNodes.size(); li++) {
    rankPos[baseRank[li]] = allNodes[li].y;
  }
  for (int r = 1; r <= maxBaseRank; r++) {
    if (rankPos[r] == 0) rankPos[r] = rankPos[r - 1] + opt.rankSep;
  }
  std::vector<double> rankMid(maxBaseRank, 0);
  for (int r = 0; r < maxBaseRank; r++) rankMid[r] = (rankPos[r] + rankPos[r + 1]) / 2.0;
  std::vector<double> rankShift(maxBaseRank + 1, 0);
  double accShift = padBefore[0];
  rankShift[0] = accShift;
  for (int r = 0; r < maxBaseRank; r++) {
    accShift += extraGap[r];
    rankShift[r + 1] = accShift;
  }
  double totalShift = accShift + padAfter[maxBaseRank];
  auto shiftFlow = [&](double y) -> double {
    double sd = padBefore[0];
    for (int r = 0; r < maxBaseRank; r++) {
      if (y > rankMid[r]) sd += extraGap[r];
      else break;
    }
    return y + sd;
  };

  for (size_t li = 0; li < allNodes.size(); li++) {
    allNodes[li].x = across[nodeOf[li]];
    allNodes[li].y += rankShift[baseRank[li]];
  }
  for (size_t k = 0; k < allEdges.size(); k++) {
    auto& le = allEdges[k];
    int tailNode = nodeOf[le.tail];
    int headNode = nodeOf[le.head];
    int tailLane = laneOf[tailNode];
    int headLane = laneOf[headNode];
    double sVal = tailLane == headLane ? laneShift[tailLane] : (laneShift[tailLane] + laneShift[headLane]) / 2.0;
    auto& pts = le.points;
    int last = static_cast<int>(pts.size()) - 2;
    int firstNode = le.reversed ? le.head : le.tail;
    int lastNode = le.reversed ? le.tail : le.head;
    pts[0] = allNodes[firstNode].x + (le.reversed ? le.headDx : le.tailDx);
    pts[1] = allNodes[firstNode].y + allNodes[firstNode].h / 2.0;
    pts[last] = allNodes[lastNode].x + (le.reversed ? le.tailDx : le.headDx);
    pts[last + 1] = allNodes[lastNode].y - allNodes[lastNode].h / 2.0;
    for (int i = 2; i < last; i += 2) {
      pts[i] += sVal;
      pts[i + 1] = shiftFlow(pts[i + 1]);
    }
    if (le.labelW > 0) {
      if (pts.size() == 4) {
        le.labelX = (pts[0] + pts[2]) / 2.0;
        le.labelY = (pts[1] + pts[3]) / 2.0;
      } else {
        le.labelX += sVal;
        le.labelY = shiftFlow(le.labelY);
      }
    }
  }

  double flowSpan = full.height + totalShift + 2.0 * LANE_MARGIN;
  toLocal(opt.dir, allNodes, allEdges, cursor, flowSpan - 2.0 * LANE_MARGIN);

  double shiftX = sideways ? LANE_GUTTER + LANE_MARGIN : 0.0;
  double shiftY = sideways ? 0.0 : LANE_GUTTER + LANE_MARGIN;
  for (size_t li = 0; li < allNodes.size(); li++) {
    int idx = nodeOf[li];
    nodes[idx].x = allNodes[li].x + shiftX;
    nodes[idx].y = allNodes[li].y + shiftY;
  }

  for (const auto& g : groups) {
    auto& c = nodes[g.id];
    if (g.members.empty()) continue;
    double x0 = std::numeric_limits<double>::infinity();
    double y0 = std::numeric_limits<double>::infinity();
    double x1 = -std::numeric_limits<double>::infinity();
    double y1 = -std::numeric_limits<double>::infinity();
    for (int idx : g.members) {
      const auto& mNode = nodes[idx];
      x0 = std::min(x0, mNode.x - mNode.w / 2.0);
      y0 = std::min(y0, mNode.y - mNode.h / 2.0);
      x1 = std::max(x1, mNode.x + mNode.w / 2.0);
      y1 = std::max(y1, mNode.y + mNode.h / 2.0);
    }
    x0 -= c.padLeft;
    y0 -= c.padTop;
    x1 += c.padRight;
    y1 += c.padBottom;
    double w = std::max(c.w, x1 - x0);
    double h = std::max(c.h, y1 - y0);
    c.x = (x0 + x1) / 2.0;
    c.y = (y0 + y1) / 2.0;
    c.w = w;
    c.h = h;
  }

  double revSign = rev ? -1.0 : 1.0;
  for (size_t k = 0; k < allEdges.size(); k++) {
    const auto& le = allEdges[k];
    auto& orig = edges[edgeIndex[k]];
    double srcAcross = le.tailDx;
    double dstAcross = le.headDx;
    double srcAlong = (le.reversed ? -1.0 : 1.0) * revSign;
    double dstAlong = -srcAlong;
    if (!orig.srcFixed) {
      orig.srcDx = sideways ? srcAlong : srcAcross;
      orig.srcDy = sideways ? srcAcross : srcAlong;
    }
    if (!orig.dstFixed) {
      orig.dstDx = sideways ? dstAlong : dstAcross;
      orig.dstDy = sideways ? dstAcross : dstAlong;
    }
    std::vector<double> pts = le.points;
    for (size_t i = 0; i < pts.size(); i += 3) {
      pts[i] += shiftX;
      pts[i + 1] += shiftY;
    }
    if (le.reversed) {
      for (int i = 0, j = static_cast<int>(pts.size()) - 3; i < j; i += 3, j -= 3) {
        std::swap(pts[i], pts[j]);
        std::swap(pts[i + 1], pts[j + 1]);
        std::swap(pts[i + 2], pts[j + 2]);
      }
    }
    orig.route = std::move(pts);
    if (orig.labelW > 0) {
      orig.labelX = le.labelX + shiftX;
      orig.labelY = le.labelY + shiftY;
    }
  }
  for (int ei : loops) {
    auto& e = edges[ei];
    const auto& node = nodes[e.src];
    e.labelX = node.x + node.w / 2.0 + 28.0 + e.labelW / 2.0;
    e.labelY = node.y;
  }

  double fullLen = LANE_GUTTER + flowSpan;
  for (auto& lane : lanes) {
    if (sideways) lane.w = fullLen;
    else lane.h = fullLen;
  }

  return {
    lanes,
    sideways ? fullLen : cursor,
    sideways ? cursor : fullLen,
  };
}

// ============================================================================
// Port of endSide / insetRoute from src/svg/shapes.ts and src/svg/cluster.ts
// ============================================================================

inline int endSide(const std::vector<double>& route, int at, const CNode& c) noexcept {
  double dx = route[at] - c.x;
  double dy = route[at + 1] - c.y;
  if (std::abs(std::abs(dy) - c.h / 2.0) < 0.5) return dy < 0 ? 0 : 2;
  if (std::abs(std::abs(dx) - c.w / 2.0) < 0.5) return dx < 0 ? 3 : 1;
  return -1;
}

struct ShapeViewRef {
  std::string shape;
  double w = 0;
  double h = 0;
};

inline void insetRoute(std::vector<double>& route, int at, const ShapeViewRef& view, const CNode& c) noexcept {
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
    int key = static_cast<int>(std::llround(route[k + 1]));
    crossings[key].push_back(route[k]);
  }
}

inline double clusterTitleX(const CNode& c, double width, const Crossings& crossings, double pad) noexcept {
  if (width == 0) return c.x;
  double top = c.y - c.h / 2.0;
  double left = c.x - c.w / 2.0;
  double half = width / 2.0;
  double candidates[3] = {left + pad - 6.0 + half, c.x, left + c.w - pad + 6.0 - half};
  auto it = crossings.find(static_cast<int>(std::llround(top)));
  if (it == crossings.end()) return candidates[0];
  const auto& xs = it->second;
  double best = candidates[0];
  double bestGap = -1;
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
  if (width <= 0) return false;
  auto it = crossings.find(static_cast<int>(std::llround(c.y - c.h / 2.0)));
  if (it == crossings.end()) return false;
  double left = c.x - c.w / 2.0;
  for (double cx : it->second) {
    if (cx > left && cx < left + c.w && std::abs(cx - x) < width / 2.0 + 4.0) return true;
  }
  return false;
}

template <typename NodeWithParent>
inline std::string struckTitle(
  const Label& label,
  double x,
  double y,
  const std::string& attrs,
  const std::string& id,
  const std::vector<NodeWithParent>& nodes,
  int index,
  const IconResolver& icons = nullptr
) {
  int depth = 1;
  for (int p = nodes[index].parent; p >= 0 && depth < 8; p = nodes[p].parent) depth++;
  std::string fill = Pele::cat({"color-mix(in srgb,var(--_a) ", num(100.0 * (1.0 - std::pow(0.5, depth))), "%,var(--_bg))"});
  return Pele::cat({"<g class=\"pele-cluster-title\" data-id=\"", id, "\">", labelSvg(label,  x,  y, Pele::cat({" fill=\"none\" stroke=\"", fill, "\" stroke-width=\"5\" stroke-linejoin=\"round\""}),  icons), labelSvg(label, x, y, attrs, icons), "</g>"});
}

} // namespace Pele
