#pragma once

#include "../foundation.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Pele {
namespace SankeyDiagram {

struct SankeyNode {
  std::string id;
  int index = 0;
};

struct SankeyLink {
  int source = 0;
  int target = 0;
  double value = 0.0;
};

struct SankeyModel : DiagramMeta {
  std::vector<SankeyNode> nodes;
  std::vector<SankeyLink> links;
  std::unordered_map<std::string, int> lookup;

  int findOrCreateNode(const std::string& id) {
    auto it = lookup.find(id);
    if (it != lookup.end()) return it->second;
    int idx = static_cast<int>(nodes.size());
    nodes.push_back({id, idx});
    lookup[id] = idx;
    return idx;
  }

  void addLink(int source, int target, double value) {
    links.push_back({source, target, value});
  }
};

enum class TokType {
  END = 0,
  STUCK = 1,
  SANKEY = 2,
  NEWLINE = 3,
  COMMA = 4,
  DQUOTE = 5,
  NON_ESCAPED_TEXT = 6,
  ESCAPED_TEXT = 7,
  EOF_TOK = 8
};

struct Tokens {
  std::vector<TokType> types;
  std::vector<std::string> texts;
  std::vector<int> starts;
};

inline bool isTextData(uint32_t c) {
  return (c >= 32 && c <= 126 && c != 34 && c != 44) ||
         (c >= 0xa0 && c <= 0xd7ff) ||
         (c >= 0xe000 && c <= 0xfffd) ||
         (c >= 0x10000 && c <= 0x10ffff);
}

inline bool isBlankU16(uint16_t c) {
  if (c == 10 || c == 13) return false;
  if (c < 128) return c == 32 || c == 9 || c == 11 || c == 12;
  return c == 0xa0 || c == 0x1680 || (c >= 0x2000 && c <= 0x200a) ||
         c == 0x2028 || c == 0x2029 || c == 0x202f || c == 0x205f ||
         c == 0x3000 || c == 0xfeff;
}

inline bool isJsTrimU16(uint16_t c) {
  return c == 10 || c == 13 || isBlankU16(c);
}

inline std::vector<uint16_t> trimU16(const std::vector<uint16_t>& s) {
  size_t a = 0;
  size_t b = s.size();
  while (a < b && isJsTrimU16(s[a])) a++;
  while (b > a && isJsTrimU16(s[b - 1])) b--;
  return std::vector<uint16_t>(s.begin() + static_cast<ptrdiff_t>(a), s.begin() + static_cast<ptrdiff_t>(b));
}

inline std::string trimJsStr(const std::string& s) {
  auto trimmed = trimU16(toUtf16(s));
  return fromUtf16(trimmed.data(), trimmed.size());
}

inline std::vector<uint16_t> prepareTextForParsingU16(const std::string& text) {
  std::vector<uint16_t> u = toUtf16(text);
  size_t end = u.size();
  while (end > 0 && isBlankU16(u[end - 1])) end--;
  std::vector<uint16_t> rows;
  size_t kept = 0;
  bool quoted = false;
  for (size_t i = 0; i < end;) {
    uint16_t c = u[i];
    if (quoted) {
      if (c == 34) {
        if (i + 1 < u.size() && u[i + 1] == 34) i++;
        else quoted = false;
      }
      i++;
    } else if (c == 34) {
      quoted = true;
      i++;
    } else if (isBlankU16(c)) {
      size_t j = i + 1;
      while (j < u.size() && isBlankU16(u[j])) j++;
      uint16_t p = (i > 0) ? u[i - 1] : 0;
      uint16_t q = (j < u.size()) ? u[j] : 0;
      if (i == 0 || q == 10 || q == 13 || ((p == 10 || p == 13) && q != 44)) {
        rows.insert(rows.end(), u.begin() + static_cast<ptrdiff_t>(kept), u.begin() + static_cast<ptrdiff_t>(i));
        kept = j;
      }
      i = j;
    } else {
      i++;
    }
  }
  rows.insert(rows.end(), u.begin() + static_cast<ptrdiff_t>(kept), u.begin() + static_cast<ptrdiff_t>(end));

  std::vector<uint16_t> collapsed;
  collapsed.reserve(rows.size());
  for (size_t i = 0; i < rows.size();) {
    if (rows[i] == 10 || rows[i] == 13) {
      collapsed.push_back(10);
      while (i < rows.size() && (rows[i] == 10 || rows[i] == 13)) i++;
    } else {
      collapsed.push_back(rows[i]);
      i++;
    }
  }

  return trimU16(collapsed);
}

inline uint32_t codePointAtU16(const std::vector<uint16_t>& src, size_t q) {
  uint16_t c1 = src[q];
  if (c1 >= 0xD800 && c1 <= 0xDBFF && q + 1 < src.size()) {
    uint16_t c2 = src[q + 1];
    if (c2 >= 0xDC00 && c2 <= 0xDFFF) {
      return 0x10000u + ((static_cast<uint32_t>(c1 - 0xD800) << 10) | static_cast<uint32_t>(c2 - 0xDC00));
    }
  }
  return c1;
}

inline Tokens tokenize(const std::vector<uint16_t>& src) {
  int n = static_cast<int>(src.size());
  Tokens out;
  std::vector<int> stack;
  int state = 0; // 0: INITIAL, 1: csv, 2: escaped_text
  int p = 0;

  auto emit = [&](TokType type, int s, int e) {
    out.types.push_back(type);
    out.texts.push_back(fromUtf16(src.data() + s, static_cast<size_t>(e - s)));
    out.starts.push_back(s);
    p = e;
  };

  auto keyword = [&](const std::string& word) -> bool {
    int wl = static_cast<int>(word.size());
    if (p + wl > n) return false;
    for (int i = 0; i < wl; ++i) {
      uint16_t ch = src[p + i];
      if (ch >= 'A' && ch <= 'Z') ch = static_cast<uint16_t>(ch + 32);
      if (ch != static_cast<uint16_t>(word[i])) return false;
    }
    uint32_t nextCh = (p + wl < n) ? src[p + wl] : 0;
    return !isWord(nextCh);
  };

  while (p < n) {
    uint16_t c = src[p];
    int start = p;
    int q = p;
    if (state == 2) {
      uint16_t nextC = (p + 1 < n) ? src[p + 1] : 0;
      if (c == 34 && nextC != 34) {
        if (!stack.empty()) {
          state = stack.back();
          stack.pop_back();
        }
        emit(TokType::DQUOTE, p, p + 1);
        continue;
      }
      while (q < n) {
        uint32_t d = codePointAtU16(src, static_cast<size_t>(q));
        uint16_t nextQ = (q + 1 < n) ? src[q + 1] : 0;
        if (isTextData(d) || d == 44 || d == 13 || d == 10) {
          q += (d > 0xffff) ? 2 : 1;
        } else if (d == 34 && nextQ == 34) {
          q += 2;
        } else {
          break;
        }
      }
      emit(TokType::ESCAPED_TEXT, p, q);
    } else {
      if (state == 0 && (c | 32) == 115) {
        int length = keyword("sankey-beta") ? 11 : (keyword("sankey") ? 6 : 0);
        if (length > 0) {
          stack.push_back(state);
          state = 1;
          emit(TokType::SANKEY, p, p + length);
          continue;
        }
      }
      uint16_t nextC = (p + 1 < n) ? src[p + 1] : 0;
      if (c == 10 || (c == 13 && nextC == 10)) {
        emit(TokType::NEWLINE, p, c == 10 ? p + 1 : p + 2);
        continue;
      }
      if (c == 44) {
        emit(TokType::COMMA, p, p + 1);
        continue;
      }
      if (c == 34) {
        stack.push_back(state);
        state = 2;
        emit(TokType::DQUOTE, p, p + 1);
        continue;
      }
      while (q < n) {
        uint32_t d = codePointAtU16(src, static_cast<size_t>(q));
        if (!isTextData(d)) break;
        q += (d > 0xffff) ? 2 : 1;
      }
      emit(TokType::NON_ESCAPED_TEXT, p, q);
    }
    if (q == start) {
      emit(TokType::STUCK, p, p);
      return out;
    }
  }

  emit(state == 2 ? TokType::ESCAPED_TEXT : TokType::EOF_TOK, n, n);
  emit(TokType::END, n, n);
  return out;
}

inline double jsParseFloat(const std::string& raw) {
  std::string s = trimJsStr(raw);
  if (s.empty()) return std::numeric_limits<double>::quiet_NaN();
  if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
    return 0.0;
  }
  const char* start = s.c_str();
  char* endptr = nullptr;
  double val = std::strtod(start, &endptr);
  if (endptr == start) return std::numeric_limits<double>::quiet_NaN();
  return val;
}

inline void parseSankey(const std::vector<uint16_t>& src, SankeyModel& db) {
  Tokens toks = tokenize(src);
  size_t i = 0;
  auto expect = [&](TokType t) {
    if (i < toks.types.size() && toks.types[i] == t) i++;
  };
  auto field = [&]() -> std::string {
    if (i < toks.types.size() && toks.types[i] == TokType::NON_ESCAPED_TEXT) {
      return toks.texts[i++];
    }
    if (i < toks.types.size() && toks.types[i] == TokType::DQUOTE) i++;
    std::string text;
    if (i < toks.types.size() && toks.types[i] == TokType::ESCAPED_TEXT) {
      text = toks.texts[i++];
    }
    expect(TokType::DQUOTE);
    return text;
  };
  auto name = [&]() -> std::string {
    std::string text = trimJsStr(field());
    std::string out;
    out.reserve(text.size());
    for (size_t k = 0; k < text.size(); ++k) {
      if (text[k] == '"' && k + 1 < text.size() && text[k + 1] == '"') {
        out.push_back('"');
        ++k;
      } else {
        out.push_back(text[k]);
      }
    }
    return out;
  };

  expect(TokType::SANKEY);
  expect(TokType::NEWLINE);
  while (i < toks.types.size()) {
    std::string from = name();
    expect(TokType::COMMA);
    std::string to = name();
    expect(TokType::COMMA);
    double value = jsParseFloat(field());
    if (!std::isfinite(value)) break;
    int source = db.findOrCreateNode(from);
    int target = db.findOrCreateNode(to);
    db.addLink(source, target, value);
    if (i >= toks.types.size() || toks.types[i] != TokType::NEWLINE) break;
    i++;
  }
}

struct LayoutOptions {
  double width = 600.0;
  double height = 400.0;
  double nodeWidth = 10.0;
  double nodePadding = 12.0;
  double labelHeight = 16.0;
  std::string alignment = "justify";
};

struct PlacedNode {
  int layer = 0;
  double value = 0.0;
  double x = 0.0;
  double y = 0.0;
  double height = 0.0;
};

struct PlacedLink {
  int source = 0;
  int target = 0;
  double value = 0.0;
  double thickness = 0.0;
  double sourceY = 0.0;
  double targetY = 0.0;
};

struct SankeyLayout {
  std::vector<PlacedNode> nodes;
  std::vector<PlacedLink> links;
};

template <typename Compare>
inline void sortSmallOrStd(std::vector<int>& list, Compare compare) {
  if (list.size() > 8) {
    std::stable_sort(list.begin(), list.end(), [&](int a, int b) {
      return compare(a, b) < 0;
    });
    return;
  }
  for (size_t i = 1; i < list.size(); ++i) {
    int item = list[i];
    int j = static_cast<int>(i) - 1;
    while (j >= 0 && compare(list[j], item) > 0) {
      list[j + 1] = list[j];
      j--;
    }
    list[j + 1] = item;
  }
}

inline SankeyLayout layoutSankey(const SankeyModel& model, const LayoutOptions& options) {
  constexpr int PASSES = 6;
  int n = static_cast<int>(model.nodes.size());
  std::vector<PlacedLink> links;
  links.reserve(model.links.size());
  for (const auto& link : model.links) {
    links.push_back({
      link.source,
      link.target,
      (std::isfinite(link.value) && link.value > 0.0) ? link.value : 0.0,
      0.0,
      0.0,
      0.0
    });
  }
  std::vector<std::vector<int>> outgoing(n);
  std::vector<std::vector<int>> incoming(n);
  for (int k = 0; k < static_cast<int>(links.size()); ++k) {
    outgoing[links[k].source].push_back(k);
    incoming[links[k].target].push_back(k);
  }

  std::vector<int> depth(n, 0);
  std::vector<int> pending(n, 0);
  std::vector<int> order;
  order.reserve(n);
  for (int i = 0; i < n; ++i) {
    pending[i] = static_cast<int>(incoming[i].size());
    if (pending[i] == 0) order.push_back(i);
  }
  for (size_t q = 0; q < order.size(); ++q) {
    int i = order[q];
    for (int k : outgoing[i]) {
      int t = links[k].target;
      if (depth[t] < depth[i] + 1) depth[t] = depth[i] + 1;
      if (--pending[t] == 0) order.push_back(t);
    }
  }

  int layers = 1;
  for (int i = 0; i < n; ++i) {
    if (depth[i] + 1 > layers) layers = depth[i] + 1;
  }
  std::vector<int> reach(n, 0);
  for (int q = static_cast<int>(order.size()) - 1; q >= 0; --q) {
    int i = order[q];
    for (int k : outgoing[i]) {
      if (reach[i] < reach[links[k].target] + 1) reach[i] = reach[links[k].target] + 1;
    }
  }

  double largest = 0.0;
  for (const auto& link : links) {
    if (link.value > largest) largest = link.value;
  }
  double unit = largest > 0.0 ? 1.0 / largest : 0.0;

  std::vector<PlacedNode> nodes;
  nodes.reserve(n);
  std::vector<double> flow(n, 0.0);
  std::vector<std::vector<int>> columns(layers);
  double step = layers > 1 ? (options.width - options.nodeWidth) / (layers - 1) : 0.0;
  for (int i = 0; i < n; ++i) {
    int layer = 0;
    if (options.alignment == "left") {
      layer = depth[i];
    } else if (options.alignment == "right") {
      layer = layers - 1 - reach[i];
    } else if (options.alignment == "center") {
      layer = depth[i];
      if (incoming[i].empty() && !outgoing[i].empty()) {
        layer = layers;
        for (int k : outgoing[i]) {
          if (depth[links[k].target] - 1 < layer) layer = depth[links[k].target] - 1;
        }
      }
    } else {
      layer = !outgoing[i].empty() ? depth[i] : layers - 1;
    }
    double into = 0.0;
    double out = 0.0;
    double value = 0.0;
    double valueOut = 0.0;
    for (int k : incoming[i]) {
      into += links[k].value * unit;
      value += links[k].value;
    }
    for (int k : outgoing[i]) {
      out += links[k].value * unit;
      valueOut += links[k].value;
    }
    flow[i] = std::max(into, out);
    nodes.push_back({layer, std::max(value, valueOut), layer * step, 0.0, 0.0});
    columns[layer].push_back(i);
  }

  size_t tallest = 1;
  for (const auto& column : columns) {
    if (column.size() > tallest) tallest = column.size();
  }
  double gap = tallest > 1 ? std::min(options.nodePadding, options.height / static_cast<double>(tallest - 1)) : options.nodePadding;
  double scale = std::numeric_limits<double>::infinity();
  for (const auto& column : columns) {
    double sum = 0.0;
    for (int i : column) sum += flow[i];
    if (sum > 0.0) {
      scale = std::min(scale, (options.height - static_cast<double>(column.size() - 1) * gap) / sum);
    }
  }
  if (!std::isfinite(scale) || scale < 0.0) scale = 0.0;

  double claim = std::max(0.0, options.labelHeight - gap);
  auto fits = [&](double s) -> bool {
    for (const auto& column : columns) {
      double extent = static_cast<double>(column.size() - 1) * gap;
      for (int i : column) extent += std::max(flow[i] * s, claim);
      if (extent > options.height + 1e-6) return false;
    }
    return true;
  };
  if (claim > 0.0 && !fits(scale)) {
    if (fits(0.0)) {
      double low = 0.0;
      for (int k = 0; k < 40; ++k) {
        double mid = (low + scale) / 2.0;
        if (fits(mid)) low = mid;
        else scale = mid;
      }
      scale = low;
    } else {
      claim = 0.0;
    }
  }

  std::vector<double> margin(n, 0.0);
  for (auto& link : links) link.thickness = link.value * unit * scale;
  for (const auto& column : columns) {
    double y = 0.0;
    for (int i : column) {
      nodes[i].height = flow[i] * scale;
      margin[i] = std::max(0.0, claim - nodes[i].height) / 2.0;
      nodes[i].y = y + margin[i];
      y += nodes[i].height + 2.0 * margin[i] + gap;
    }
    double spare = (options.height - y + gap) / static_cast<double>(column.size() + 1);
    for (size_t k = 0; k < column.size(); ++k) {
      nodes[column[k]].y += spare * static_cast<double>(k + 1);
    }
  }

  auto center = [&](int i) -> double {
    return nodes[i].y + nodes[i].height / 2.0;
  };

  std::vector<double> sourceOffset(links.size(), 0.0);
  std::vector<double> targetOffset(links.size(), 0.0);
  auto byTarget = [&](int a, int b) -> double {
    double diff = center(links[a].target) - center(links[b].target);
    return diff != 0.0 ? diff : static_cast<double>(a - b);
  };
  auto bySource = [&](int a, int b) -> double {
    double diff = center(links[a].source) - center(links[b].source);
    return diff != 0.0 ? diff : static_cast<double>(a - b);
  };
  auto byCenter = [&](int a, int b) -> double {
    double diff = center(a) - center(b);
    return diff != 0.0 ? diff : static_cast<double>(a - b);
  };

  auto stackOut = [&](int i) {
    double y = 0.0;
    sortSmallOrStd(outgoing[i], byTarget);
    for (int k : outgoing[i]) {
      sourceOffset[k] = y + links[k].thickness / 2.0;
      y += links[k].thickness;
    }
  };
  auto stackIn = [&](int i) {
    double y = 0.0;
    sortSmallOrStd(incoming[i], bySource);
    for (int k : incoming[i]) {
      targetOffset[k] = y + links[k].thickness / 2.0;
      y += links[k].thickness;
    }
  };
  auto stackAll = [&]() {
    for (int i = 0; i < n; ++i) {
      stackOut(i);
      stackIn(i);
    }
  };

  auto settle = [&](std::vector<int>& column, double pull, bool forward) {
    for (int i : column) {
      const auto& list = forward ? incoming[i] : outgoing[i];
      if (list.empty()) continue;
      if (forward) stackIn(i);
      else stackOut(i);
      double weighted = 0.0;
      double weight = 0.0;
      double plain = 0.0;
      for (int k : list) {
        double level = forward
          ? nodes[links[k].source].y + sourceOffset[k] - targetOffset[k]
          : nodes[links[k].target].y + targetOffset[k] - sourceOffset[k];
        weighted += level * links[k].thickness;
        weight += links[k].thickness;
        plain += center(forward ? links[k].source : links[k].target) - nodes[i].height / 2.0;
      }
      double wanted = weight > 0.0 ? weighted / weight : plain / static_cast<double>(list.size());
      nodes[i].y += (wanted - nodes[i].y) * pull;
    }
    sortSmallOrStd(column, byCenter);
    double y = 0.0;
    for (int i : column) {
      if (nodes[i].y - margin[i] < y) nodes[i].y = y + margin[i];
      y = nodes[i].y + nodes[i].height + margin[i] + gap;
    }
    y = options.height;
    for (int k = static_cast<int>(column.size()) - 1; k >= 0; --k) {
      int i = column[k];
      if (nodes[i].y + nodes[i].height + margin[i] > y) nodes[i].y = y - nodes[i].height - margin[i];
      y = nodes[i].y - margin[i] - gap;
    }
  };

  stackAll();
  for (int pass = 0; pass < PASSES; ++pass) {
    double pull = std::pow(0.99, pass);
    for (int l = layers - 2; l >= 0; --l) settle(columns[l], pull, false);
    stackAll();
    for (int l = 1; l < layers; ++l) settle(columns[l], pull, true);
    stackAll();
  }
  for (size_t k = 0; k < links.size(); ++k) {
    links[k].sourceY = nodes[links[k].source].y + sourceOffset[k];
    links[k].targetY = nodes[links[k].target].y + targetOffset[k];
  }

  return {nodes, links};
}

inline std::string u32ToBase36(uint32_t v) {
  if (v == 0) return "0";
  static const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
  std::string out;
  while (v > 0) {
    out.push_back(digits[v % 36]);
    v /= 36;
  }
  std::reverse(out.begin(), out.end());
  return out;
}

inline std::string contentId(const std::string& text) {
  std::vector<uint16_t> u = toUtf16(text);
  uint32_t a = 0x811c9dc5u;
  int32_t b = 5381;
  for (size_t i = 0; i < u.size(); ++i) {
    uint32_t c = u[i];
    a = static_cast<uint32_t>(static_cast<int32_t>(a ^ c) * static_cast<int32_t>(0x01000193u));
    b = static_cast<int32_t>(static_cast<uint32_t>(b * 33) + c);
  }
  return u32ToBase36(a) + u32ToBase36(static_cast<uint32_t>(b));
}

inline double numberOption(const Config& conf, const std::string& key, double fallback, double minVal, double maxVal) {
  if (!conf.isMap()) return fallback;
  const Config* v = conf.get(key);
  if (v && v->isNumber() && v->nVal >= minVal) {
    return std::min(v->nVal, maxVal);
  }
  return fallback;
}

inline std::string textOption(const Config& conf, const std::string& key) {
  if (!conf.isMap()) return "";
  const Config* v = conf.get(key);
  if (!v) return "";
  if (v->isString()) return v->sVal;
  if (v->isNumber() && std::isfinite(v->nVal)) return num(v->nVal);
  return "";
}

inline double rounded(double value) {
  return std::abs(value) < 1e15 ? std::floor(value * 100.0 + 0.5) / 100.0 : value;
}

inline std::string fillStyle(const Config& color) {
  if (!color.isString()) return "";
  return resolveStyle({"fill:" + color.sVal}).shape;
}

inline double sankeyTitleRoom(const RenderOptions& options) {
  double pad = options.hasPadding ? options.padding : 8.0;
  double maxW = options.maxWidth;
  return maxW > 0.0 ? std::max(80.0, maxW - 2.0 * pad) : 4000.0;
}

template <typename DrawFn>
inline Rendered fitWidth(const RenderOptions& options, double natural, double least, DrawFn&& draw) {
  Rendered result = draw(natural);
  double available = options.maxWidth;
  if (!(available > 0.0) || result.width <= available) return result;
  double floor = std::min(least, natural);
  double plot = natural;
  for (int round = 0; round < 3 && result.width > available && plot > floor; round++) {
    plot = std::max(floor, plot - (result.width - available));
    result = draw(plot);
  }
  return result;
}

inline bool isWordOrDashId(const std::string& s) {
  if (s.empty()) return false;
  for (unsigned char c : s) {
    bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
    if (!ok) return false;
  }
  return true;
}

inline Rendered draw(const SankeyModel& model, const Config& conf, const RenderOptions& options, double chartWidth, double natural) {
  constexpr double LABEL_GAP = 6.0;
  constexpr double VALUE_GAP = 15.0;
  constexpr double LABEL_ROOM = 1.8;
  constexpr size_t CAREFUL = 300;

  double size = options.hasFontSize ? options.fontSize : 16.0;
  double least = std::round(size * 0.625);
  double small = std::max(least, std::round(size * 0.875 * std::min(1.0, chartWidth / natural)));
  const TextMeasurer& measurer = getMeasurer(options);
  double pad = options.hasPadding ? options.padding : 8.0;

  double chartHeight = numberOption(conf, "height", 400.0, 1.0, 10000.0);
  double nodeWidth = std::min(numberOption(conf, "nodeWidth", 10.0, 1.0, 200.0), chartWidth);
  const Config* sv = conf.isMap() ? conf.get("showValues") : nullptr;
  bool showValues = !(sv && sv->isBool() && !sv->bVal);
  std::string prefix = decodeEntities(textOption(conf, "prefix"));
  std::string suffix = decodeEntities(textOption(conf, "suffix"));
  const Config* ls = conf.isMap() ? conf.get("labelStyle") : nullptr;
  bool outlined = ls && ls->isString() && ls->sVal == "outlined";
  std::string alignment = "justify";
  const Config* na = conf.isMap() ? conf.get("nodeAlignment") : nullptr;
  if (na && na->isString()) {
    if (na->sVal == "left" || na->sVal == "right" || na->sVal == "center") alignment = na->sVal;
  }
  const Config* lc = conf.isMap() ? conf.get("linkColor") : nullptr;
  std::string linkColor = (lc && lc->isString()) ? lc->sVal : "gradient";

  auto lay = [&](double labelSize) -> SankeyLayout {
    return layoutSankey(model, {
      chartWidth,
      chartHeight,
      nodeWidth,
      numberOption(conf, "nodePadding", 12.0, 0.0, 200.0) + (showValues ? VALUE_GAP : 0.0),
      labelSize * 1.15,
      alignment
    });
  };
  SankeyLayout layout = lay(small);
  std::unordered_map<int, int> rows;
  int fullest = 1;
  for (const auto& node : layout.nodes) {
    int count = ++rows[node.layer];
    if (count > fullest) fullest = count;
  }
  double crowded = std::max(least, std::round(chartHeight / static_cast<double>(fullest) / LABEL_ROOM));
  if (crowded < small) {
    small = crowded;
    layout = lay(small);
  }
  const auto& nodes = layout.nodes;
  const auto& links = layout.links;
  double lineHeight = small * 1.5;

  std::unordered_map<std::string, std::string> custom;
  const Config* nc = conf.isMap() ? conf.get("nodeColors") : nullptr;
  if (nc && nc->isMap()) {
    for (const auto& kv : nc->mVal) {
      std::string st = fillStyle(kv.second);
      if (!st.empty()) custom[kv.first] = st;
    }
  }
  auto nodeStyle = [&](int i) -> std::string {
    auto it = custom.find(model.nodes[i].id);
    return it != custom.end() ? it->second : "";
  };
  auto nodeColor = [&](int i) -> std::string {
    std::string st = nodeStyle(i);
    const std::string pfx = " style=\"fill:";
    const std::string sfx = ";\"";
    if (st.size() >= pfx.size() + sfx.size() &&
        st.compare(0, pfx.size(), pfx) == 0 &&
        st.compare(st.size() - sfx.size(), sfx.size(), sfx) == 0 &&
        st.find('\n') == std::string::npos) {
      return st.substr(pfx.size(), st.size() - pfx.size() - sfx.size());
    }
    return seriesColor(i);
  };

  int central = 0;
  double most = 0.0;
  for (const auto& node : nodes) {
    if (node.value > most) {
      most = node.value;
      central = node.layer;
    }
  }
  double minX = 0.0;
  double maxX = chartWidth;
  double minY = 0.0;
  double maxY = chartHeight;

  struct SankeyLabelInfo {
    std::string value;
    double width = 0.0;
    bool after = false;
    double left = 0.0;
    double y = 0.0;
  };
  std::vector<SankeyLabelInfo> labels;
  labels.reserve(nodes.size());
  for (size_t i = 0; i < nodes.size(); ++i) {
    const auto& node = nodes[i];
    std::string name = decodeEntities(model.nodes[i].id);
    std::string value = showValues ? (prefix + num(rounded(node.value)) + suffix) : "";
    double width = measurer.width(!value.empty() ? (name + " " + value) : name, small, 0);
    bool after = outlined ? (node.layer >= central) : (node.x < chartWidth / 2.0);
    double x = after ? (node.x + nodeWidth + LABEL_GAP) : (node.x - LABEL_GAP);
    double y = node.y + node.height / 2.0;
    minX = std::min(minX, after ? x : x - width);
    maxX = std::max(maxX, after ? x + width : x);
    minY = std::min(minY, y - small * 0.75);
    maxY = std::max(maxY, y + small * 0.75);
    labels.push_back({value, width, after, after ? x : x - width, y});
  }

  std::unordered_map<int, std::vector<size_t>> bands;
  if (nodes.size() <= CAREFUL) {
    for (size_t idx = 0; idx < labels.size(); ++idx) {
      int band = static_cast<int>(std::floor(labels[idx].y / lineHeight));
      for (int key : {band - 2, band - 1, band, band + 1, band + 2}) {
        bands[key].push_back(idx);
      }
    }
  }

  std::string labelsOut;
  std::string nodesOut;
  for (size_t i = 0; i < nodes.size(); ++i) {
    const auto& node = nodes[i];
    const std::string& id = model.nodes[i].id;
    std::string name = decodeEntities(id);
    double height = std::max(node.height, 1.0);
    nodesOut += "<rect class=\"pele-sankey-node\" data-id=\"" + escText(id) + "\" x=\"" + num(node.x) + "\" y=\"" + num(node.y) + "\" width=\"" + num(nodeWidth) + "\" height=\"" + num(height) + "\" rx=\"" + num(std::min(2.0, height / 2.0)) + "\" fill=\"" + seriesColor(static_cast<int>(i)) + "\"" + nodeStyle(static_cast<int>(i)) + "/>";

    auto& label = labels[i];
    auto overlapsLabel = [&](double left, double y) -> bool {
      int b = static_cast<int>(std::floor(y / lineHeight));
      auto it = bands.find(b);
      if (it == bands.end()) return false;
      for (size_t otherIdx : it->second) {
        if (otherIdx == i) continue;
        const auto& other = labels[otherIdx];
        if (std::abs(other.y - y) < lineHeight &&
            left < other.left + other.width + LABEL_GAP &&
            left + label.width > other.left - LABEL_GAP) {
          return true;
        }
      }
      return false;
    };

    if (nodes.size() <= CAREFUL && overlapsLabel(label.left, label.y)) {
      bool placed = false;
      for (double dy : {0.0, -lineHeight, lineHeight}) {
        if (placed) break;
        for (bool after : {label.after, !label.after}) {
          double left = after ? (node.x + nodeWidth + LABEL_GAP) : (node.x - LABEL_GAP - label.width);
          double y = label.y + dy;
          if (overlapsLabel(left, y) || left < minX || left + label.width > maxX || y - small * 0.75 < minY || y + small * 0.75 > maxY) continue;
          bool barOverlap = false;
          for (size_t j = 0; j < nodes.size(); ++j) {
            if (j == i) continue;
            const auto& bar = nodes[j];
            if (left < bar.x + nodeWidth + LABEL_GAP &&
                left + label.width > bar.x - LABEL_GAP &&
                y - small * 0.75 < bar.y + bar.height &&
                y + small * 0.75 > bar.y) {
              barOverlap = true;
              break;
            }
          }
          if (barOverlap) continue;
          double own = std::max({0.0, node.y - y, y - node.y - node.height});
          bool colCloser = false;
          for (size_t j = 0; j < nodes.size(); ++j) {
            if (j == i) continue;
            const auto& bar = nodes[j];
            if (bar.x == node.x && std::max({0.0, bar.y - y, y - bar.y - bar.height}) <= own) {
              colCloser = true;
              break;
            }
          }
          if (colCloser) continue;
          label.left = left;
          label.y = y;
          label.after = after;
          placed = true;
          break;
        }
      }
    }

    double x = label.after ? (node.x + nodeWidth + LABEL_GAP) : (node.x - LABEL_GAP);
    std::string position = " x=\"" + num(x) + "\" y=\"" + num(label.y + small * 0.35) + "\" text-anchor=\"" + (label.after ? "start" : "end") + "\"";
    std::string text = esc(name) + (!label.value.empty() ? (" <tspan class=\"pele-sankey-value\" fill=\"var(--_m)\">" + esc(label.value) + "</tspan>") : "");
    labelsOut += "<g class=\"pele-label\" data-id=\"" + escText(id) + "\">" +
      (outlined
        ? ("<text" + position + " fill=\"none\" stroke=\"var(--_bg)\" stroke-width=\"3\" stroke-linejoin=\"round\">" + esc(name) + (!label.value.empty() ? (" " + esc(label.value)) : "") + "</text>")
        : "") +
      "<text" + position + ">" + text + "</text></g>";
  }

  std::string fixed = (linkColor == "gradient" || linkColor == "source" || linkColor == "target") ? "" : fillStyle(Config::makeString(linkColor));
  std::string idPrefix = "";
  bool gradient = (linkColor == "gradient") && isWordOrDashId(idPrefix);
  std::vector<std::string> gradientOrder;
  std::unordered_map<std::string, std::string> gradients;
  std::string linksOut;
  for (const auto& link : links) {
    int end = (linkColor == "target") ? link.target : link.source;
    double half = std::max(link.thickness, 1.0) / 2.0;
    double x0 = nodes[link.source].x + nodeWidth;
    double x1 = nodes[link.target].x;
    std::string endStyle = !fixed.empty() ? fixed : nodeStyle(end);
    std::string paint = "fill=\"" + seriesColor(end) + "\"" + endStyle;
    if (gradient) {
      std::string stops = "x1=\"" + num(x0) + "\" x2=\"" + num(x1) + "\"><stop offset=\"0\" stop-color=\"" + nodeColor(link.source) + "\"/><stop offset=\"1\" stop-color=\"" + nodeColor(link.target) + "\"/>";
      std::string id = idPrefix + "-sankey-" + contentId(stops);
      if (gradients.find(id) == gradients.end()) {
        gradientOrder.push_back(id);
      }
      gradients[id] = "<linearGradient id=\"" + id + "\" gradientUnits=\"userSpaceOnUse\" " + stops + "</linearGradient>";
      paint = "fill=\"url(#" + id + ") " + nodeColor(link.source) + "\"";
    }
    std::string mid = num((x0 + x1) / 2.0);
    std::string from = escText(model.nodes[link.source].id);
    std::string to = escText(model.nodes[link.target].id);
    linksOut +=
      "<path class=\"pele-sankey-link\" data-id=\"" + from + "-&gt;" + to + "\" d=\"M" + num(x0) + "," + num(link.sourceY - half) +
      "C" + mid + "," + num(link.sourceY - half) + " " + mid + "," + num(link.targetY - half) + " " + num(x1) + "," + num(link.targetY - half) +
      "L" + num(x1) + "," + num(link.targetY + half) +
      "C" + mid + "," + num(link.targetY + half) + " " + mid + "," + num(link.sourceY + half) + " " + num(x0) + "," + num(link.sourceY + half) +
      "Z\" " + paint + ">" +
      "<title>" + from + " \xE2\x86\x92 " + to + ": " + esc(prefix) + num(rounded(link.value)) + esc(suffix) + "</title></path>";
  }

  Label title = layoutLabel(model.title, false, measurer, size, sankeyTitleRoom(options), Style::Bold);
  double titleHeight = title.height > 0.0 ? title.height + 12.0 : 0.0;
  double inner = std::max(maxX - minX, title.width);
  double ox = pad - minX + (inner - (maxX - minX)) / 2.0;
  double oy = pad + titleHeight - minY;
  double width = std::ceil(inner + 2.0 * pad);
  double height = std::ceil(maxY - minY + titleHeight + 2.0 * pad);

  std::string defsStr;
  if (!gradientOrder.empty()) {
    defsStr = "<defs>";
    for (const auto& gid : gradientOrder) {
      defsStr += gradients[gid];
    }
    defsStr += "</defs>";
  }

  std::string body =
    labelSvg(title, width / 2.0, pad + title.height / 2.0, " class=\"pele-title\" font-weight=\"var(--_tw)\"") +
    "<g transform=\"translate(" + num(ox) + "," + num(oy) + ")\">" +
    defsStr +
    "<g class=\"pele-sankey-links\" fill-opacity=\"0.4\">" + linksOut + "</g>" +
    "<g class=\"pele-sankey-nodes\">" + nodesOut + "</g>" +
    "<g class=\"pele-labels\" font-size=\"" + num(small) + "\">" + labelsOut + "</g>" +
    "</g>";

  std::string svg = svgDocument("sankey", width, height, size, options, model, body);
  return {svg, width, height, {}};
}

inline Rendered renderSankeyModel(const SankeyModel& model, const Config& config, const RenderOptions& options) {
  const Config emptyObj = Config::makeMap();
  const Config* sc = config.isMap() ? config.get("sankey") : nullptr;
  const Config& conf = (sc && sc->isMap()) ? *sc : emptyObj;
  double natural = numberOption(conf, "width", 600.0, 1.0, 10000.0);
  return fitWidth(options, natural, 160.0, [&](double chartWidth) {
    return draw(model, conf, options, chartWidth, natural);
  });
}

inline Rendered render(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options = {}) {
  SankeyModel db;
  if (!title.empty()) db.title = title;
  parseSankey(prepareTextForParsingU16(source), db);
  return renderSankeyModel(db, config, options);
}

} // namespace SankeyDiagram

inline Rendered renderSankey(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options = {}) {
  return SankeyDiagram::render(source, config, title, options);
}

} // namespace Pele
