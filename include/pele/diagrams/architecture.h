#pragma once
#include "../layout.h"
#include "../common_parse.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Pele {

namespace architecture_detail {

// ============================================================================
// Safe single-space normalizer for title/accTitle/accDescr
// ============================================================================

inline std::string archSingle(const std::string& text) noexcept {
  std::string s = trimStr(text);
  std::string out;
  int spaces = 0;
  for (size_t i = 0; i < s.size(); i++) {
    char c = s[i];
    if (c == '\t' || c == ' ') {
      spaces++;
    } else {
      if (spaces == 1) out.push_back(s[i - 1]);
      else if (spaces >= 2) out.push_back(' ');
      spaces = 0;
      out.push_back(c);
    }
  }
  return out;
}

inline std::string archTitleValue(const std::string& text) noexcept {
  size_t pos = text.find("title");
  if (pos == std::string::npos) return "";
  return archSingle(text.substr(pos + 5));
}

inline std::string archAccTitleValue(const std::string& text) noexcept {
  size_t pos = text.find(':');
  if (pos == std::string::npos) return "";
  return archSingle(text.substr(pos + 1));
}

inline std::string archAccDescrValue(const std::string& text) noexcept {
  size_t brace = text.find('{');
  size_t colon = text.find(':');
  if (colon != std::string::npos && (brace == std::string::npos || colon < brace)) {
    return archSingle(text.substr(colon + 1));
  }
  if (brace != std::string::npos) {
    size_t close = text.find('}', brace + 1);
    std::string body = text.substr(brace + 1, close == std::string::npos ? std::string::npos : close - brace - 1);
    return cleanAccDescr(body);
  }
  return "";
}

// ============================================================================
// Parser (`src/diagrams/architecture/parser.ts`)
// ============================================================================

enum T {
  header = 0,
  junction,
  service,
  column,
  group,
  align,
  row,
  dashes,
  inTok,
  colon,
  dash,
  direction,
  arrowGroup,
  into,
  accDescr,
  accTitle,
  title,
  string,
  id,
  newline,
  whitespace,
  yaml,
  directive,
  comment,
  icon,
  label
};

inline bool isWordChar(char c) noexcept {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
}

inline TokenType archWord(const std::string& text) noexcept {
  TokenType t;
  t.name = text;
  t.literal = text;
  t.isRegex = false;
  t.longer = {T::id};
  return t;
}

inline TokenType archLiteral(const std::string& text, std::vector<int> longer = {}) noexcept {
  TokenType t;
  t.name = text;
  t.literal = text;
  t.isRegex = false;
  t.longer = std::move(longer);
  return t;
}

inline const std::vector<TokenType>& getTokens() noexcept {
  static const std::vector<TokenType> tokens = []() {
    std::vector<TokenType> v;
    v.push_back(archWord("architecture-beta")); // 0
    v.push_back(archWord("junction"));          // 1
    v.push_back(archWord("service"));           // 2
    v.push_back(archWord("column"));            // 3
    v.push_back(archWord("group"));             // 4
    v.push_back(archWord("align"));             // 5
    v.push_back(archWord("row"));               // 6
    v.push_back(archLiteral("--", {T::yaml}));  // 7
    v.push_back(archWord("in"));                // 8
    v.push_back(archLiteral(":"));              // 9
    v.push_back(archLiteral("-", {T::yaml}));   // 10

    TokenType dir;
    dir.name = "ARROW_DIRECTION";
    dir.isRegex = true;
    dir.first = "LRTB";
    dir.match = [](const std::string& src, int at, int) -> int {
      if (at >= static_cast<int>(src.size())) return -1;
      char c = src[at];
      return (c == 'L' || c == 'R' || c == 'T' || c == 'B') ? (at + 1) : -1;
    };
    v.push_back(std::move(dir));                // 11

    TokenType ag;
    ag.name = "ARROW_GROUP";
    ag.literal = "{group}";
    ag.isRegex = false;
    v.push_back(std::move(ag));                 // 12

    TokenType intoTok;
    intoTok.name = "ARROW_INTO";
    intoTok.isRegex = true;
    intoTok.first = "<>";
    intoTok.match = [](const std::string& src, int at, int) -> int {
      if (at >= static_cast<int>(src.size())) return -1;
      char c = src[at];
      return (c == '<' || c == '>') ? (at + 1) : -1;
    };
    v.push_back(std::move(intoTok));            // 13

    v.push_back(common_tokens::makeAccDescr()); // 14
    v.push_back(common_tokens::makeAccTitle()); // 15

    TokenType titleTok;
    titleTok.name = "TITLE";
    titleTok.isRegex = true;
    titleTok.first = "\t t";
    titleTok.match = [](const std::string& src, int at, int) -> int {
      int p = common_tokens::matchTabsSpaces(src, at);
      if (src.compare(p, 5, "title") != 0) return -1;
      p += 5;
      int n = static_cast<int>(src.size());
      if (p < n && (src[p] == '\t' || src[p] == ' ')) {
        return common_tokens::matchRestOfLineBeforeComment(src, p + 1);
      }
      // (?! [-\w]*\w )
      int q = p;
      while (q < n && src[q] == '-') q++;
      if (q < n && isWordChar(src[q])) return -1;
      return p;
    };
    v.push_back(std::move(titleTok));           // 16

    v.push_back(common_tokens::makeString());   // 17

    TokenType idTok;
    idTok.name = "ID";
    idTok.isRegex = true;
    idTok.first = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_";
    idTok.match = [](const std::string& src, int at, int) -> int {
      int n = static_cast<int>(src.size());
      if (at >= n || !isWordChar(src[at])) return -1;
      int p = at + 1;
      int lastWord = p;
      while (p < n && (isWordChar(src[p]) || src[p] == '-')) {
        if (isWordChar(src[p])) lastWord = p + 1;
        p++;
      }
      return lastWord;
    };
    v.push_back(std::move(idTok));              // 18

    v.push_back(common_tokens::makeNewline());           // 19
    v.push_back(common_tokens::makeWhitespace());        // 20
    v.push_back(common_tokens::makeYaml());              // 21
    v.push_back(common_tokens::makeDirective());         // 22
    v.push_back(common_tokens::makeSingleLineComment()); // 23

    TokenType iconTok;
    iconTok.name = "ARCH_ICON";
    iconTok.isRegex = true;
    iconTok.first = "(";
    iconTok.match = [](const std::string& src, int at, int) -> int {
      int n = static_cast<int>(src.size());
      if (at >= n || src[at] != '(') return -1;
      int p = at + 1;
      while (p < n && (isWordChar(src[p]) || src[p] == '-' || src[p] == ':')) p++;
      if (p > at + 1 && p < n && src[p] == ')') return p + 1;
      return -1;
    };
    v.push_back(std::move(iconTok));            // 24

    TokenType lblTok;
    lblTok.name = "ARCH_TITLE";
    lblTok.isRegex = true;
    lblTok.first = "[";
    lblTok.match = [](const std::string& src, int at, int) -> int {
      int n = static_cast<int>(src.size());
      if (at + 1 >= n || src[at] != '[') return -1;
      char q = src[at + 1];
      if (q == '"' || q == '\'') {
        int p = at + 2;
        while (p < n) {
          if (src[p] == '\\') {
            p += 2;
            if (p > n) break;
          } else if (src[p] == q) {
            if (p + 1 < n && src[p + 1] == ']') return p + 2;
            break;
          } else {
            p++;
          }
        }
      }
      int p = at + 1;
      while (p < n && src[p] != '[' && src[p] != ']' && src[p] != '\r' && src[p] != '\n') p++;
      if (p > at + 1 && p < n && src[p] == ']') return p + 1;
      return -1;
    };
    v.push_back(std::move(lblTok));             // 25

    return v;
  }();
  return tokens;
}

struct GroupAst {
  std::string id;
  std::optional<std::string> icon;
  std::optional<std::string> title;
  std::optional<std::string> in;
};

struct ServiceAst {
  std::string id;
  std::optional<std::string> icon;
  std::optional<std::string> iconText;
  std::optional<std::string> title;
  std::optional<std::string> in;
};

struct JunctionAst {
  std::string id;
  std::optional<std::string> in;
};

struct EdgeAst {
  std::string lhsId;
  bool lhsGroup = false;
  char lhsDir = 'R';
  bool lhsInto = false;
  std::optional<std::string> title;
  bool rhsInto = false;
  char rhsDir = 'L';
  std::string rhsId;
  bool rhsGroup = false;
};

struct AlignmentAst {
  std::string direction; // "row" | "column"
  std::vector<std::string> members;
};

struct ArchitectureAst {
  std::optional<std::string> title;
  std::optional<std::string> accTitle;
  std::optional<std::string> accDescr;
  std::vector<GroupAst> groups;
  std::vector<ServiceAst> services;
  std::vector<JunctionAst> junctions;
  std::vector<EdgeAst> edges;
  std::vector<AlignmentAst> alignments;
};

inline std::string labelValue(const std::string& text) noexcept {
  std::string result = trimStr(text.substr(1, text.size() - 2));
  if (result.size() >= 2 && ((result.front() == '"' && result.back() == '"') || (result.front() == '\'' && result.back() == '\''))) {
    result = result.substr(1, result.size() - 2);
    result = replaceAllStr(result, "\\\"", "\"");
    result = replaceAllStr(result, "\\'", "'");
  }
  return trimStr(result);
}

inline ArchitectureAst parseArchitecture(const std::string& src) {
  const auto& tokTypes = getTokens();
  Tokens toks = tokenize(src, tokTypes, "architecture");
  TokenReader r(toks, tokTypes, "architecture");
  ArchitectureAst ast;

  auto is = [&](int kind) -> bool { return r.kind() == kind; };
  auto endOfLine = [&]() {
    if (r.kind() == -1) return;
    if (r.kind() != T::newline) r.fail("a line break or the end of input");
    while (r.kind() == T::newline) r.i++;
  };

  while (r.kind() == T::newline) r.i++;
  r.expect(T::header);
  while (r.kind() != -1) {
    switch (r.kind()) {
      case T::newline:
        r.i++;
        continue;
      case T::accDescr:
        ast.accDescr = archAccDescrValue(r.take());
        break;
      case T::accTitle:
        ast.accTitle = archAccTitleValue(r.take());
        break;
      case T::title:
        ast.title = archTitleValue(r.take());
        break;
      case T::group: {
        r.i++;
        GroupAst group;
        group.id = r.expect(T::id);
        if (is(T::icon)) {
          std::string ic = r.take();
          group.icon = ic.substr(1, ic.size() - 2);
        }
        if (is(T::label)) group.title = labelValue(r.take());
        if (r.accept(T::inTok)) group.in = r.expect(T::id);
        ast.groups.push_back(std::move(group));
        break;
      }
      case T::service: {
        r.i++;
        ServiceAst service;
        service.id = r.expect(T::id);
        if (is(T::string)) {
          service.iconText = common_tokens::stringValue(r.take());
        } else if (is(T::icon)) {
          std::string ic = r.take();
          service.icon = ic.substr(1, ic.size() - 2);
        }
        if (is(T::label)) service.title = labelValue(r.take());
        if (r.accept(T::inTok)) service.in = r.expect(T::id);
        ast.services.push_back(std::move(service));
        break;
      }
      case T::junction: {
        r.i++;
        JunctionAst junction;
        junction.id = r.expect(T::id);
        if (r.accept(T::inTok)) junction.in = r.expect(T::id);
        ast.junctions.push_back(std::move(junction));
        break;
      }
      case T::id: {
        EdgeAst edge;
        edge.lhsId = r.take();
        edge.lhsGroup = r.accept(T::arrowGroup);
        r.expect(T::colon);
        edge.lhsDir = r.expect(T::direction)[0];
        edge.lhsInto = r.accept(T::into);
        if (!r.accept(T::dashes)) {
          if (!is(T::dash)) r.fail("'--' or '-'");
          r.i++;
          edge.title = labelValue(r.expect(T::label));
          r.expect(T::dash);
        }
        edge.rhsInto = r.accept(T::into);
        edge.rhsDir = r.expect(T::direction)[0];
        r.expect(T::colon);
        edge.rhsId = r.expect(T::id);
        edge.rhsGroup = r.accept(T::arrowGroup);
        ast.edges.push_back(std::move(edge));
        break;
      }
      case T::align: {
        r.i++;
        if (!is(T::row) && !is(T::column)) r.fail("'row' or 'column'");
        std::string direction = r.take();
        std::vector<std::string> members = {r.expect(T::id), r.expect(T::id)};
        while (is(T::id)) members.push_back(r.take());
        ast.alignments.push_back({std::move(direction), std::move(members)});
        break;
      }
      default:
        r.fail("a group, a service, a junction, an edge, an alignment, a title, or a line break");
    }
    endOfLine();
  }
  return ast;
}

// ============================================================================
// DB (`src/diagrams/architecture/db.ts`)
// ============================================================================

struct ArchNode {
  std::string id;
  std::string type; // "service" | "junction"
  std::optional<std::string> icon;
  std::optional<std::string> iconText;
  std::optional<std::string> title;
  std::optional<std::string> in;
};

using ArchGroup = GroupAst;
using ArchEdge = EdgeAst;
using ArchLayoutHint = AlignmentAst;

[[noreturn]] inline void archFail(const std::string& message) {
  throw PeleError(message, "semantic", "architecture");
}

class ArchitectureDb : public DiagramMeta {
public:
  std::vector<ArchNode> nodeList;
  Pele::FlatMap<std::string, size_t> nodeMap;
  std::vector<ArchGroup> groupList;
  Pele::FlatMap<std::string, size_t> groupMap;
  std::vector<ArchEdge> edges;
  std::vector<ArchLayoutHint> layoutHints;

private:
  Pele::FlatMap<std::string, std::string> registered; // id -> "node" | "group"

  void registerId(const std::string& kind, const std::string& id, const std::optional<std::string>& parent) {
    auto it = registered.find(id);
    if (it != registered.end()) {
      archFail("The " + kind + " id [" + id + "] is already in use by another " + it->second);
    }
    if (parent.has_value()) {
      if (id == *parent) archFail(Pele::cat({"The ", kind, " [", id, "] cannot be placed within itself"}));
      auto pit = registered.find(*parent);
      if (pit == registered.end()) {
        archFail(Pele::cat({"The ", kind, " [", id, "]'s parent does not exist. Please make sure the parent is created before this ", kind}));
      }
      if (pit->second == "node") archFail(Pele::cat({"The ", kind, " [", id, "]'s parent is not a group"}));
    }
    registered[id] = (kind == "group") ? "group" : "node";
  }

public:
  void addService(const ServiceAst& s) {
    registerId("service", s.id, s.in);
    nodeMap[s.id] = nodeList.size();
    nodeList.push_back({s.id, "service", s.icon, s.iconText, s.title, s.in});
  }

  void addJunction(const JunctionAst& j) {
    registerId("junction", j.id, j.in);
    nodeMap[j.id] = nodeList.size();
    nodeList.push_back({j.id, "junction", std::nullopt, std::nullopt, std::nullopt, j.in});
  }

  void addGroup(const GroupAst& g) {
    registerId("group", g.id, g.in);
    groupMap[g.id] = groupList.size();
    groupList.push_back(g);
  }

  void addEdge(const EdgeAst& e) {
    struct EndInfo { const char* side; const std::string& id; bool crosses; };
    EndInfo ends[2] = {{"left", e.lhsId, e.lhsGroup}, {"right", e.rhsId, e.rhsGroup}};
    for (const auto& end : ends) {
      if (registered.find(end.id) == registered.end()) {
        archFail("The " + std::string(end.side) + "-hand id [" + end.id + "] does not yet exist. Please create the service/group before declaring an edge to it.");
      }
    }
    for (const auto& end : ends) {
      if (nodeMap.find(end.id) == nodeMap.end()) {
        archFail("The " + std::string(end.side) + "-hand id [" + end.id + "] is a group. Add {group} to a service inside it instead.");
      }
    }
    const auto& lhsParent = nodeList[nodeMap[e.lhsId]].in;
    const auto& rhsParent = nodeList[nodeMap[e.rhsId]].in;
    if (lhsParent.has_value() && rhsParent.has_value() && *lhsParent == *rhsParent) {
      for (const auto& end : ends) {
        if (end.crosses) {
          archFail("The " + std::string(end.side) + "-hand id [" + end.id + "] is modified to traverse the group boundary, but the edge does not pass through two groups.");
        }
      }
    }
    edges.push_back(e);
  }

  void addLayoutHint(const ArchLayoutHint& hint) {
    if (hint.members.size() < 2) {
      archFail("An align directive requires at least two members; got " + std::to_string(hint.members.size()));
    }
    Pele::FlatSet<std::string> seen;
    for (const auto& id : hint.members) {
      auto it = registered.find(id);
      if (it == registered.end() || it->second != "node") {
        archFail(Pele::cat({"align ", hint.direction, " references [", id, "], which is not a service or junction"}));
      }
      if (!seen.insert(id).second) {
        archFail(Pele::cat({"align ", hint.direction, " lists [", id, "] more than once"}));
      }
    }
    layoutHints.push_back(hint);
  }
};

inline void populate(const ArchitectureAst& ast, ArchitectureDb& db) noexcept {
  if (ast.accDescr.has_value() && !ast.accDescr->empty()) db.accDescr = cleanAccDescr(*ast.accDescr);
  if (ast.accTitle.has_value() && !ast.accTitle->empty()) db.accTitle = trimStartStr(*ast.accTitle);
  if (ast.title.has_value() && !ast.title->empty()) db.title = *ast.title;
  for (const auto& g : ast.groups) db.addGroup(g);
  for (const auto& s : ast.services) db.addService(s);
  for (const auto& j : ast.junctions) db.addJunction(j);
  for (const auto& e : ast.edges) db.addEdge(e);
  for (const auto& a : ast.alignments) db.addLayoutHint(a);
}

// ============================================================================
// Built-in Icons (`src/diagrams/architecture/icons.ts`)
// ============================================================================

inline std::string builtinIcon(const std::string& name) noexcept {
  std::string d;
  if (name == "database") d = "M4 6a8 3 0 1 0 16 0a8 3 0 1 0-16 0M4 6v12a8 3 0 0 0 16 0V6M4 12a8 3 0 0 0 16 0";
  else if (name == "server") d = "M3 4h18v7H3zM3 13h18v7H3zM7 7.5h.01M7 16.5h.01M11 7.5h6M11 16.5h6";
  else if (name == "disk") d = "M5 3h14v18H5zM12 6.5a4 4 0 1 0 0 8a4 4 0 1 0 0-8M12 10.5h.01M8 18h3";
  else if (name == "internet") d = "M3 12a9 9 0 1 0 18 0a9 9 0 1 0-18 0M3 12h18M12 3a13 13 0 0 0 0 18M12 3a13 13 0 0 1 0 18";
  else if (name == "cloud") d = "M7 18a4 4 0 0 1-.5-7.97A6 6 0 0 1 18 9.5a4.25 4.25 0 0 1-.5 8.5z";
  return d.empty() ? "" : Pele::cat({"<path d=\"", d, "\" stroke=\"var(--_fg)\" stroke-width=\"1.5\"/>"});
}

// ============================================================================
// Grid Layout (`src/diagrams/architecture/layout.ts`)
// ============================================================================

struct Grid {
  std::vector<int> col;
  std::vector<int> row;
  std::vector<int> groupCol;
  std::vector<int> groupRow;
  std::vector<int> groupCols;
  std::vector<int> groupRows;
  std::vector<int> groupParent;
  std::vector<uint8_t> groupEmpty;
  std::vector<int> nodeParent;
};

constexpr int DX[4] = {-1, 1, 0, 0};
constexpr int DY[4] = {0, 0, -1, 1};

inline int sideIndex(char side) noexcept {
  return side == 'L' ? 0 : side == 'R' ? 1 : side == 'T' ? 2 : 3;
}

inline Grid placeOnGrid(
  const std::vector<ArchNode>& nodes,
  const std::vector<ArchGroup>& groups,
  const std::vector<ArchEdge>& edges,
  const std::vector<ArchLayoutHint>& hints
) {
  constexpr int SMALL = 64;
  constexpr int64_t OFFSET = 1LL << 25;
  constexpr int64_t SPAN = 1LL << 26;

  int N = static_cast<int>(nodes.size());
  int G = static_cast<int>(groups.size());
  int root = G;
  Pele::FlatMap<std::string, int> nodeIndex;
  Pele::FlatMap<std::string, int> groupIndex;
  std::vector<int> parent(N + G, root);
  std::vector<int> depth(G + 1, 0);
  std::vector<std::vector<int>> kids(G + 1);

  for (int g = 0; g < G; g++) {
    groupIndex[groups[g].id] = g;
    if (groups[g].in.has_value()) {
      auto it = groupIndex.find(*groups[g].in);
      if (it != groupIndex.end() && it->second != g) {
        parent[N + g] = it->second;
        depth[g] = depth[it->second] + 1;
      } else {
        depth[g] = 1;
      }
    } else {
      depth[g] = 1;
    }
  }
  for (int i = 0; i < N; i++) {
    nodeIndex[nodes[i].id] = i;
    if (nodes[i].in.has_value()) {
      auto it = groupIndex.find(*nodes[i].in);
      if (it != groupIndex.end()) parent[i] = it->second;
    }
    kids[parent[i]].push_back(i);
  }
  for (int g = 0; g < G; g++) {
    kids[parent[N + g]].push_back(N + g);
  }

  std::vector<int> linkA, linkB, sideA, sideB, itemA, itemB;
  std::vector<std::vector<int>> hintLinks(N + G);
  std::vector<std::vector<int>> edgeLinks(N + G);

  auto addLink = [&](int a, int b, int sa, int sb, std::vector<std::vector<int>>& lists) {
    if (a == b) return;
    int ia = a, ib = b;
    int ca = parent[a], cb = parent[b];
    while (ca != cb) {
      if (depth[ca] >= depth[cb]) {
        ia = N + ca;
        ca = parent[ia];
      } else {
        ib = N + cb;
        cb = parent[ib];
      }
    }
    int k = static_cast<int>(linkA.size());
    linkA.push_back(a);
    linkB.push_back(b);
    sideA.push_back(sa);
    sideB.push_back(sb);
    itemA.push_back(ia);
    itemB.push_back(ib);
    lists[ia].push_back(k);
    lists[ib].push_back(k);
  };

  for (const auto& hint : hints) {
    bool isRow = (hint.direction == "row");
    for (size_t i = 1; i < hint.members.size(); i++) {
      auto itA = nodeIndex.find(hint.members[i - 1]);
      auto itB = nodeIndex.find(hint.members[i]);
      if (itA != nodeIndex.end() && itB != nodeIndex.end()) {
        addLink(itA->second, itB->second, isRow ? 1 : 3, isRow ? 0 : 2, hintLinks);
      }
    }
  }
  for (bool likeSides : {false, true}) {
    for (const auto& edge : edges) {
      if ((edge.lhsDir == edge.rhsDir) != likeSides) continue;
      auto itA = nodeIndex.find(edge.lhsId);
      auto itB = nodeIndex.find(edge.rhsId);
      if (itA != nodeIndex.end() && itB != nodeIndex.end()) {
        addLink(itA->second, itB->second, sideIndex(edge.lhsDir), sideIndex(edge.rhsDir), edgeLinks);
      }
    }
  }

  std::vector<int> x(N + G, 0);
  std::vector<int> y(N + G, 0);
  std::vector<int> w(N + G, 1);
  std::vector<int> h(N + G, 1);
  std::vector<uint8_t> placed(N + G, 0);

  std::vector<int> order;
  Pele::FlatMap<int64_t, int> cells;
  std::vector<int> large;
  Pele::FlatMap<std::string, int> reach;
  size_t first = 0;
  int ax = 0, ay = 0;

  auto anchor = [&](int node, int item) {
    ax = 0;
    ay = 0;
    for (int cur = node; cur != item; cur = N + parent[cur]) {
      ax += x[cur];
      ay += y[cur];
    }
  };

  auto put = [&](int item, int px, int py) {
    x[item] = px;
    y[item] = py;
    placed[item] = 1;
    order.push_back(item);
    if (w[item] * h[item] > SMALL) {
      large.push_back(item);
      return;
    }
    for (int cy = py; cy < py + h[item]; cy++) {
      for (int cx = px; cx < px + w[item]; cx++) {
        cells[(static_cast<int64_t>(cx) + OFFSET) * SPAN + static_cast<int64_t>(cy) + OFFSET] = item;
      }
    }
  };

  auto overlaps = [&](int item, int rx, int ry, int rw, int rh) -> bool {
    return x[item] < rx + rw && rx < x[item] + w[item] && y[item] < ry + rh && ry < y[item] + h[item];
  };

  auto collide = [&](int rx, int ry, int rw, int rh) -> int {
    if (rw * rh > SMALL) {
      for (size_t i = first; i < order.size(); i++) {
        if (overlaps(order[i], rx, ry, rw, rh)) return order[i];
      }
      return -1;
    }
    for (int cy = ry; cy < ry + rh; cy++) {
      for (int cx = rx; cx < rx + rw; cx++) {
        auto it = cells.find((static_cast<int64_t>(cx) + OFFSET) * SPAN + static_cast<int64_t>(cy) + OFFSET);
        if (it != cells.end()) return it->second;
      }
    }
    for (int item : large) {
      if (overlaps(item, rx, ry, rw, rh)) return item;
    }
    return -1;
  };

  auto slide = [&](int rx, int ry, int rw, int rh, int sx, int sy) -> int {
    std::string key = std::to_string(rx) + "," + std::to_string(ry) + "," + std::to_string(rw) + "," +
                      std::to_string(rh) + "," + std::to_string(sx) + "," + std::to_string(sy);
    int k = 0;
    auto it = reach.find(key);
    if (it != reach.end()) k = it->second;
    for (;;) {
      int hit = collide(rx + k * sx, ry + k * sy, rw, rh);
      if (hit < 0) break;
      k = sx > 0 ? (x[hit] + w[hit] - rx)
        : sx < 0 ? (rx + rw - x[hit])
        : sy > 0 ? (y[hit] + h[hit] - ry)
                 : (ry + rh - y[hit]);
    }
    reach[key] = k;
    return k;
  };

  auto follow = [&](int from, int k) {
    bool forward = (itemA[k] == from);
    int to = forward ? itemB[k] : itemA[k];
    if (placed[to]) return;
    int sp = forward ? sideA[k] : sideB[k];
    int sq = forward ? sideB[k] : sideA[k];
    anchor(forward ? linkA[k] : linkB[k], from);
    int pax = ax, pay = ay;
    anchor(forward ? linkB[k] : linkA[k], to);
    int dx = DX[sp];
    int dy = DY[sp];
    int shiftX = 0, shiftY = 0;
    bool facing = (DX[sp] + DX[sq] == 0 && DY[sp] + DY[sq] == 0);
    if (sp == sq) {
      dx = (DX[sp] == 0) ? 1 : 0;
      dy = 1 - dx;
    } else if (!facing) {
      shiftX = -DX[sq];
      shiftY = -DY[sq];
    }
    int bx = 0, by = 0;
    if (dx != 0) {
      bx = dx > 0 ? (x[from] + w[from]) : (x[from] - w[to]);
      by = y[from] + pay + shiftY - ay;
    } else {
      by = dy > 0 ? (y[from] + h[from]) : (y[from] - h[to]);
      bx = x[from] + pax + shiftX - ax;
    }
    if (facing) {
      const auto& list = edgeLinks[to];
      if (list.size() > 1) {
        std::vector<int> wanted;
        for (int j : list) {
          bool toIsB = (itemB[j] == to);
          int other = toIsB ? itemA[j] : itemB[j];
          if (!placed[other] || (toIsB ? sideA[j] : sideB[j]) != sp || (toIsB ? sideB[j] : sideA[j]) != sq) continue;
          anchor(toIsB ? linkA[j] : linkB[j], other);
          int at = (dx != 0) ? (y[other] + ay) : (x[other] + ax);
          anchor(toIsB ? linkB[j] : linkA[j], to);
          wanted.push_back(at - (dx != 0 ? ay : ax));
        }
        if (wanted.size() > 1) {
          std::sort(wanted.begin(), wanted.end());
          if (dx != 0) by = wanted[(wanted.size() - 1) >> 1];
          else bx = wanted[(wanted.size() - 1) >> 1];
        }
      }
      int px = (dx == 0) ? 1 : 0;
      int py = 1 - px;
      int forth = slide(bx, by, w[to], h[to], px, py);
      int back = (forth == 0) ? 0 : slide(bx, by, w[to], h[to], -px, -py);
      int near = (forth <= back) ? forth : -back;
      bx += px * near;
      by += py * near;
    } else {
      int far = slide(bx, by, w[to], h[to], dx, dy);
      bx += dx * far;
      by += dy * far;
    }
    put(to, bx, by);
  };

  auto layout = [&](int c) {
    const auto& items = kids[c];
    if (items.empty()) return;
    order.clear();
    std::vector<size_t> starts;
    for (int start : items) {
      if (placed[start]) continue;
      first = order.size();
      starts.push_back(first);
      cells.clear();
      large.clear();
      reach.clear();
      put(start, 0, 0);
      size_t hintAt = first;
      size_t edgeAt = first;
      size_t linkAt = 0;
      for (;;) {
        if (hintAt < order.size()) {
          int from = order[hintAt++];
          for (int k : hintLinks[from]) follow(from, k);
          continue;
        }
        if (edgeAt >= order.size()) break;
        const auto& list = edgeLinks[order[edgeAt]];
        if (linkAt >= list.size()) {
          edgeAt++;
          linkAt = 0;
          continue;
        }
        follow(order[edgeAt], list[linkAt++]);
      }
    }
    starts.push_back(order.size());

    size_t count = starts.size() - 1;
    std::vector<int> minX(count), minY(count), partW(count), partH(count);
    int area = 0;
    int widest = 0;
    for (size_t p = 0; p < count; p++) {
      int x0 = std::numeric_limits<int>::max();
      int y0 = std::numeric_limits<int>::max();
      int x1 = std::numeric_limits<int>::min();
      int y1 = std::numeric_limits<int>::min();
      for (size_t i = starts[p]; i < starts[p + 1]; i++) {
        int item = order[i];
        x0 = std::min(x0, x[item]);
        y0 = std::min(y0, y[item]);
        x1 = std::max(x1, x[item] + w[item]);
        y1 = std::max(y1, y[item] + h[item]);
      }
      minX[p] = x0;
      minY[p] = y0;
      partW[p] = x1 - x0;
      partH[p] = y1 - y0;
      area += partW[p] * partH[p];
      widest = std::max(widest, partW[p]);
    }
    int limit = std::max(widest, static_cast<int>(std::ceil(std::sqrt(static_cast<double>(area) * 2.0))));
    int cx = 0, cy = 0, shelf = 0, width = 0;
    for (size_t p = 0; p < count; p++) {
      if (cx > 0 && cx + partW[p] > limit) {
        cy += shelf;
        cx = 0;
        shelf = 0;
      }
      for (size_t i = starts[p]; i < starts[p + 1]; i++) {
        x[order[i]] += cx - minX[p];
        y[order[i]] += cy - minY[p];
      }
      cx += partW[p];
      shelf = std::max(shelf, partH[p]);
      width = std::max(width, cx);
    }
    if (c < G) {
      w[N + c] = width;
      h[N + c] = cy + shelf;
    }
  };

  for (int c = G - 1; c >= 0; c--) layout(c);
  layout(root);

  Grid grid;
  grid.groupCol.assign(G, 0);
  grid.groupRow.assign(G, 0);
  grid.groupParent.assign(G, -1);
  grid.groupEmpty.assign(G, 0);
  for (int g = 0; g < G; g++) {
    int p = parent[N + g];
    grid.groupParent[g] = (p == root) ? -1 : p;
    grid.groupCol[g] = x[N + g] + (p == root ? 0 : grid.groupCol[p]);
    grid.groupRow[g] = y[N + g] + (p == root ? 0 : grid.groupRow[p]);
    if (kids[g].empty()) grid.groupEmpty[g] = 1;
  }
  grid.col.assign(N, 0);
  grid.row.assign(N, 0);
  grid.nodeParent.assign(N, -1);
  for (int i = 0; i < N; i++) {
    int p = parent[i];
    grid.nodeParent[i] = (p == root) ? -1 : p;
    grid.col[i] = x[i] + (p == root ? 0 : grid.groupCol[p]);
    grid.row[i] = y[i] + (p == root ? 0 : grid.groupRow[p]);
  }
  grid.groupCols.assign(w.begin() + N, w.end());
  grid.groupRows.assign(h.begin() + N, h.end());
  return grid;
}

// ============================================================================
// Renderer (`src/diagrams/architecture/render.ts`)
// ============================================================================

constexpr double STUB = 12.0;
constexpr double CORNER = 6.0;
constexpr double LABEL_GAP = 4.0;
constexpr double DOT = 3.0;

inline double setting(const Config& config, const std::string& key, double fallback, double minV, double maxV) noexcept {
  const auto* sec = config.get("architecture");
  if (sec && sec->isMap()) {
    const auto* v = sec->get(key);
    if (v && v->isNumber() && std::isfinite(v->nVal)) {
      return std::min(maxV, std::max(minV, v->nVal));
    }
  }
  return fallback;
}

inline Pele::FlatMap<int, int> rankValues(std::vector<int> values) noexcept {
  std::sort(values.begin(), values.end());
  Pele::FlatMap<int, int> out;
  for (int v : values) {
    if (out.find(v) == out.end()) {
      int idx = static_cast<int>(out.size());
      out[v] = idx;
    }
  }
  return out;
}

inline std::string iconSlot(const std::string& name, double x, double y, double side, const IconResolver& icons) noexcept {
  std::string body = icons ? icons(name) : "";
  if (body.empty() && !icons) body = builtinIcon(name);
  return Pele::cat({"<svg class=\"pele-icon\" data-icon=\"", escText(name), "\" x=\"", num(x), "\" y=\"", num(y), "\" width=\"", num(side), "\" height=\"", num(side), "\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\">", body, "</svg>"});
}

inline Label fitLabel(const Label& label, double maxWidth, size_t maxLines, const TextMeasurer& measurer) noexcept {
  bool cut = label.lines.size() > maxLines;
  if (!cut && label.width <= maxWidth) return label;
  const std::string ellipsis = "\xE2\x80\xA6"; // …
  double dots = measurer.width(ellipsis, label.size, 0);
  std::vector<std::vector<Span>> lines;
  std::vector<double> widths;
  double widest = 0.0;
  size_t limit = std::min(maxLines, label.lines.size());
  for (size_t i = 0; i < limit; i++) {
    std::vector<Span> line = label.lines[i];
    double width = label.widths[i];
    if (width > maxWidth || (cut && i == maxLines - 1)) {
      for (;;) {
        if (line.empty()) {
          Span s;
          s.text = ellipsis;
          s.style = 0;
          s.width = dots;
          line.push_back(std::move(s));
          width = dots;
          break;
        }
        Span& span = line.back();
        double rest = width - span.width;
        if (!span.hasIcon && rest + dots < maxWidth) {
          // Collect UTF-8 codepoint boundaries
          std::vector<size_t> bounds = {0};
          for (size_t k = 0; k < span.text.size(); ) {
            unsigned char c = static_cast<unsigned char>(span.text[k]);
            size_t adv = (c < 0x80) ? 1 : ((c & 0xE0) == 0xC0) ? 2 : ((c & 0xF0) == 0xE0) ? 3 : 4;
            k = std::min(span.text.size(), k + adv);
            bounds.push_back(k);
          }
          size_t lo = 0;
          size_t hi = bounds.size() - 1;
          while (lo < hi) {
            size_t mid = (lo + hi + 1) >> 1;
            if (rest + measurer.width(span.text.substr(0, bounds[mid]), label.size, span.style) + dots <= maxWidth) lo = mid;
            else hi = mid - 1;
          }
          span.text = trimEndStr(span.text.substr(0, bounds[lo])) + ellipsis;
          span.width = measurer.width(span.text, label.size, span.style);
          width = rest + span.width;
          break;
        }
        line.pop_back();
        width = rest;
      }
    }
    lines.push_back(std::move(line));
    widths.push_back(width);
    widest = std::max(widest, width);
  }
  Label out = label;
  out.lines = std::move(lines);
  out.widths = std::move(widths);
  out.width = widest;
  out.height = static_cast<double>(out.lines.size()) * label.lineHeight;
  return out;
}

struct Port {
  double x = 0;
  double y = 0;
  double dx = 0;
  double dy = 0;
  double x0 = 0;
  double y0 = 0;
  double x1 = 0;
  double y1 = 0;
  double gap = 0;
  bool wire = false;
};

inline std::vector<double> routeEdge(const Port& s, const Port& e) noexcept {
  double sx = s.x, sy = s.y, sdx = s.dx, sdy = s.dy;
  double ex = e.x, ey = e.y, edx = e.dx, edy = e.dy;
  double s1x = sx + sdx * STUB;
  double s1y = sy + sdy * STUB;
  double e1x = ex + edx * STUB;
  double e1y = ey + edy * STUB;
  if (sdx * edx + sdy * edy == 0.0) {
    double cx = sdx != 0.0 ? ex : sx;
    double cy = sdx != 0.0 ? sy : ey;
    if ((cx - sx) * sdx + (cy - sy) * sdy > 0.0 && (cx - ex) * edx + (cy - ey) * edy > 0.0) {
      return {sx, sy, cx, cy, ex, ey};
    }
    return sdx != 0.0
      ? std::vector<double>{sx, sy, s1x, s1y, s1x, e1y, e1x, e1y, ex, ey}
      : std::vector<double>{sx, sy, s1x, s1y, e1x, s1y, e1x, e1y, ex, ey};
  }
  bool sideways = sdx != 0.0;
  double offset = sideways ? (ey - sy) : (ex - sx);
  if (sdx == -edx && sdy == -edy) {
    double ahead = (ex - sx) * sdx + (ey - sy) * sdy;
    if (ahead > 0.0) {
      if (std::abs(offset) < 0.5) return {sx, sy, ex, ey};
      if (sideways) {
        double lo = sdx > 0.0 ? s.x1 : e.x1;
        double hi = sdx > 0.0 ? e.x0 : s.x0;
        double mx = lo < hi ? (lo + hi) / 2.0 : (sx + ex) / 2.0;
        return {sx, sy, mx, sy, mx, ey, ex, ey};
      }
      double lo = sdy > 0.0 ? s.y1 : e.y1;
      double hi = sdy > 0.0 ? e.y0 : s.y0;
      double my = lo < hi ? (lo + hi) / 2.0 : (sy + ey) / 2.0;
      return {sx, sy, sx, my, ex, my, ex, ey};
    }
  } else if (std::abs(offset) >= 1.0) {
    if (sideways) {
      double x = sdx > 0.0 ? std::max(sx, ex) + STUB : std::min(sx, ex) - STUB;
      return {sx, sy, x, sy, x, ey, ex, ey};
    }
    double y = sdy > 0.0 ? std::max(sy, ey) + STUB : std::min(sy, ey) - STUB;
    return {sx, sy, sx, y, ex, y, ex, ey};
  }
  if (sideways) {
    double lane = s.y1 + STUB <= e.y0 ? (s.y1 + e.y0) / 2.0
                : e.y1 + STUB <= s.y0 ? (e.y1 + s.y0) / 2.0
                : std::max(s.y1, e.y1) + STUB;
    return {sx, sy, s1x, sy, s1x, lane, e1x, lane, e1x, ey, ex, ey};
  }
  double lane = s.x1 + STUB <= e.x0 ? (s.x1 + e.x0) / 2.0
              : e.x1 + STUB <= s.x0 ? (e.x1 + s.x0) / 2.0
              : std::max(s.x1, e.x1) + STUB;
  return {sx, sy, sx, s1y, lane, s1y, lane, e1y, ex, e1y, ex, ey};
}

inline std::vector<double> simplifyPoints(const std::vector<double>& p) noexcept {
  std::vector<double> out = {p[0], p[1]};
  for (size_t i = 2; i < p.size(); i += 2) {
    double x = p[i];
    double y = p[i + 1];
    size_t n = out.size();
    bool sameX = std::abs(x - out[n - 2]) < 0.01;
    bool sameY = std::abs(y - out[n - 1]) < 0.01;
    if (sameX && sameY) continue;
    if (n >= 4 && ((sameX && std::abs(out[n - 4] - out[n - 2]) < 0.01) || (sameY && std::abs(out[n - 3] - out[n - 1]) < 0.01))) {
      out[n - 2] = x;
      out[n - 1] = y;
    } else {
      out.push_back(x);
      out.push_back(y);
    }
  }
  return out;
}

inline std::string pathData(const std::vector<double>& p, double startTrim, double endTrim) noexcept {
  size_t n = p.size() / 2;
  std::vector<double> xs(n), ys(n);
  for (size_t i = 0; i < n; i++) {
    xs[i] = p[i * 2];
    ys[i] = p[i * 2 + 1];
  }
  auto trim = [&](size_t at, size_t toward, double amount) {
    double len = std::hypot(xs[toward] - xs[at], ys[toward] - ys[at]);
    double t = std::min(amount, std::max(0.0, len - 1.0));
    if (len > 0.0 && t > 0.0) {
      xs[at] += ((xs[toward] - xs[at]) / len) * t;
      ys[at] += ((ys[toward] - ys[at]) / len) * t;
    }
  };
  trim(0, 1, startTrim);
  trim(n - 1, n - 2, endTrim);
  std::string d = Pele::cat({"M", num(xs[0]), ",", num(ys[0])});
  for (size_t i = 1; i + 1 < n; i++) {
    double before = std::hypot(xs[i] - xs[i - 1], ys[i] - ys[i - 1]);
    double after = std::hypot(xs[i + 1] - xs[i], ys[i + 1] - ys[i]);
    double r = std::min({CORNER, before / 2.0, after / 2.0});
    if (r < 0.5) {
      Pele::appendCat(d, {"L", num(xs[i]), ",", num(ys[i])});
      continue;
    }
    double inX = xs[i] - ((xs[i] - xs[i - 1]) / before) * r;
    double inY = ys[i] - ((ys[i] - ys[i - 1]) / before) * r;
    double outX = xs[i] + ((xs[i + 1] - xs[i]) / after) * r;
    double outY = ys[i] + ((ys[i + 1] - ys[i]) / after) * r;
    Pele::appendCat(d, {"L", num(inX), ",", num(inY), "Q", num(xs[i]), ",", num(ys[i]), " ", num(outX), ",", num(outY)});
  }
  return Pele::cat({d, "L", num(xs[n - 1]), ",", num(ys[n - 1])});
}

inline Rendered draw(const ArchitectureDb& model, const Config& config, const RenderOptions& options, bool tight) noexcept {
  double iconSize = setting(config, "iconSize", 48.0, 8.0, 512.0);
  double size = setting(config, "fontSize", options.fontSize, 4.0, 128.0);
  double groupPad = std::min(setting(config, "padding", 20.0, 0.0, 256.0), tight ? 12.0 : std::numeric_limits<double>::infinity());
  double small = std::round(size * 0.875);
  const TextMeasurer& measurer = getMeasurer(options);
  double pad = options.padding;
  const auto& icons = options.icons;
  double half = iconSize / 2.0;

  const auto& nodes = model.nodeList;
  const auto& groups = model.groupList;
  int N = static_cast<int>(nodes.size());
  int G = static_cast<int>(groups.size());
  Grid grid = placeOnGrid(nodes, groups, model.edges, model.layoutHints);
  Pele::FlatMap<std::string, int> index;
  for (int i = 0; i < N; i++) index[nodes[i].id] = i;

  std::vector<Label> labels(N);
  std::vector<double> boxW(N, 0.0), boxH(N, 0.0);
  double wrap = tight ? std::max(iconSize, 64.0) : std::max(iconSize * 2.25, 96.0);
  for (int i = 0; i < N; i++) {
    const auto& node = nodes[i];
    std::string ttl = (node.type == "service" && node.title.has_value()) ? *node.title : "";
    Label label = layoutLabel(ttl, true, measurer, size, wrap);
    boxW[i] = std::max(iconSize, 2.0 * std::ceil(label.width / 2.0));
    boxH[i] = iconSize + (label.height > 0.0 ? LABEL_GAP + label.height : 0.0);
    labels[i] = std::move(label);
  }

  std::vector<int> colValues, rowValues;
  for (int i = 0; i < N; i++) {
    colValues.push_back(grid.col[i]);
    rowValues.push_back(grid.row[i]);
  }
  for (int g = 0; g < G; g++) {
    if (!grid.groupEmpty[g]) continue;
    colValues.push_back(grid.groupCol[g]);
    rowValues.push_back(grid.groupRow[g]);
  }
  auto colOf = rankValues(std::move(colValues));
  auto rowOf = rankValues(std::move(rowValues));
  int C = static_cast<int>(colOf.size());
  int R = static_cast<int>(rowOf.size());
  std::vector<double> colW(C, 0.0), rowH(R, 0.0);
  double gap = tight ? std::max(24.0, 2.0 * STUB) : std::max(40.0, std::round(iconSize * 0.75));
  std::vector<double> gapX(C, gap), gapY(R, gap);
  std::vector<int> nodeCol(N, 0), nodeRow(N, 0);
  for (int i = 0; i < N; i++) {
    int c = (nodeCol[i] = colOf[grid.col[i]]);
    int r = (nodeRow[i] = rowOf[grid.row[i]]);
    colW[c] = std::max(colW[c], boxW[i]);
    rowH[r] = std::max(rowH[r], boxH[i]);
  }
  for (int g = 0; g < G; g++) {
    if (!grid.groupEmpty[g]) continue;
    int c = colOf[grid.groupCol[g]];
    int r = rowOf[grid.groupRow[g]];
    colW[c] = std::max(colW[c], iconSize);
    rowH[r] = std::max(rowH[r], half);
  }

  int E = static_cast<int>(model.edges.size());
  std::vector<Label> edgeLabels(E);
  std::vector<int> ends(E * 2, -1);
  for (int k = 0; k < E; k++) {
    const auto& edge = model.edges[k];
    auto itA = index.find(edge.lhsId);
    auto itB = index.find(edge.rhsId);
    int a = (ends[k * 2] = (itA != index.end() ? itA->second : -1));
    int b = (ends[k * 2 + 1] = (itB != index.end() ? itB->second : -1));
    Label label = layoutLabel(edge.title.value_or(""), true, measurer, small, 160.0);
    if (label.width > 0.0 && a >= 0 && b >= 0) {
      if (nodeCol[a] != nodeCol[b]) {
        int c = std::min(nodeCol[a], nodeCol[b]);
        gapX[c] = std::max(gapX[c], std::ceil(label.width) + 28.0);
      } else {
        colW[nodeCol[a]] = std::max(colW[nodeCol[a]], 2.0 * std::ceil(label.width / 2.0) + 8.0);
        if (nodeRow[a] != nodeRow[b]) {
          int r = std::min(nodeRow[a], nodeRow[b]);
          gapY[r] = std::max(gapY[r], std::ceil(label.height) + 24.0);
        }
      }
    }
    edgeLabels[k] = std::move(label);
  }

  std::vector<Label> groupLabels(G);
  std::vector<int> c0(G, 0), c1(G, 0), r0(G, 0), r1(G, 0);
  std::vector<double> insL(G, 0.0), insR(G, 0.0), insT(G, 0.0), insB(G, 0.0);
  std::vector<double> headW(G, 0.0), headH(G, 0.0);
  std::vector<double> colL(C, 0.0), colR(C, 0.0), rowT(R, 0.0), rowB(R, 0.0);
  double badge = std::round(size * 1.25);
  for (int g = 0; g < G; g++) {
    c0[g] = colOf[grid.groupCol[g]];
    c1[g] = colOf[grid.groupCol[g] + grid.groupCols[g] - 1];
    r0[g] = rowOf[grid.groupRow[g]];
    r1[g] = rowOf[grid.groupRow[g] + grid.groupRows[g] - 1];
    Label label = layoutLabel(groups[g].title.value_or(""), true, measurer, size, tight ? std::max(80.0, titleRoom(options) / 2.0) : 320.0);
    double iconW = groups[g].icon.has_value() ? badge : 0.0;
    headW[g] = label.width + iconW + (label.width > 0.0 && iconW > 0.0 ? 6.0 : 0.0);
    headH[g] = std::max(label.height, iconW);
    groupLabels[g] = std::move(label);
  }
  for (int g = G - 1; g >= 0; g--) {
    double l = groupPad + insL[g];
    double r = groupPad + insR[g];
    double t = (headH[g] > 0.0 ? std::max(groupPad, headH[g] + 12.0) : groupPad) + insT[g];
    double b = groupPad + insB[g];
    if (headW[g] > 0.0) {
      double inner = l + r;
      for (int c = c0[g]; c <= c1[g]; c++) {
        inner += colW[c] + (c < c1[g] ? colR[c] + gapX[c] + colL[c + 1] : 0.0);
      }
      if (headW[g] + 20.0 > inner) r += std::ceil(headW[g] + 20.0 - inner);
    }
    insL[g] = l;
    insR[g] = r;
    insT[g] = t;
    insB[g] = b;
    colL[c0[g]] = std::max(colL[c0[g]], l);
    colR[c1[g]] = std::max(colR[c1[g]], r);
    rowT[r0[g]] = std::max(rowT[r0[g]], t);
    rowB[r1[g]] = std::max(rowB[r1[g]], b);
    int p = grid.groupParent[g];
    if (p < 0) continue;
    if (c0[p] == c0[g]) insL[p] = std::max(insL[p], l);
    if (c1[p] == c1[g]) insR[p] = std::max(insR[p], r);
    if (r0[p] == r0[g]) insT[p] = std::max(insT[p], t);
    if (r1[p] == r1[g]) insB[p] = std::max(insB[p], b);
  }

  std::vector<double> X(C, 0.0), Y(R, 0.0);
  double innerW = 0.0;
  for (int c = 0; c < C; c++) {
    X[c] = innerW + colL[c];
    innerW = X[c] + colW[c] + colR[c] + (c < C - 1 ? gapX[c] : 0.0);
  }
  double innerH = 0.0;
  for (int r = 0; r < R; r++) {
    Y[r] = innerH + rowT[r];
    innerH = Y[r] + rowH[r] + rowB[r] + (r < R - 1 ? gapY[r] : 0.0);
  }

  double minX = 0.0, minY = 0.0, maxX = innerW, maxY = innerH;
  auto left = [&](int g) -> double { return X[c0[g]] - insL[g]; };
  auto right = [&](int g) -> double { return X[c1[g]] + colW[c1[g]] + insR[g]; };
  auto top = [&](int g) -> double { return Y[r0[g]] - insT[g]; };
  auto bottom = [&](int g) -> double { return Y[r1[g]] + rowH[r1[g]] + insB[g]; };
  auto centerX = [&](int i) -> double { return X[nodeCol[i]] + colW[nodeCol[i]] / 2.0; };

  auto port = [&](int i, int side, bool viaGroup) -> Port {
    double dx = DX[side];
    double dy = DY[side];
    double cx = centerX(i);
    double y = Y[nodeRow[i]];
    Port p{cx, y + half, dx, dy, cx - boxW[i] / 2.0, y, cx + boxW[i] / 2.0, y + boxH[i], 0.0, false};
    if (nodes[i].type == "junction") {
      p.x0 = p.x1 = cx;
      p.y0 = p.y1 = p.y;
      p.gap = DOT;
    } else if (dx != 0.0) {
      p.x = cx + dx * half;
    } else {
      p.y = dy < 0.0 ? y : (y + boxH[i] + (labels[i].height > 0.0 ? 2.0 : 0.0));
    }
    int g = viaGroup ? grid.nodeParent[i] : -1;
    if (g >= 0) {
      p.x0 = left(g);
      p.x1 = right(g);
      p.y0 = top(g);
      p.y1 = bottom(g);
      p.gap = 0.0;
      p.wire = (nodes[i].type == "junction");
      if (dx != 0.0) p.x = dx < 0.0 ? p.x0 : p.x1;
      else p.y = dy < 0.0 ? p.y0 : p.y1;
    }
    return p;
  };

  std::string edgesOut;
  std::string labelsOut;
  std::vector<double> runs;
  double trimLength = markerTrim("arrow_point");
  for (int k = 0; k < E; k++) {
    const auto& edge = model.edges[k];
    int a = ends[k * 2];
    int b = ends[k * 2 + 1];
    if (a < 0 || b < 0) continue;
    Port s = port(a, sideIndex(edge.lhsDir), edge.lhsGroup);
    Port e = port(b, sideIndex(edge.rhsDir), edge.rhsGroup);
    if (s.x == e.x && s.y == e.y && s.dx == e.dx) {
      s.x -= 8.0 * std::abs(s.dy);
      s.y -= 8.0 * std::abs(s.dx);
      e.x += 8.0 * std::abs(s.dy);
      e.y += 8.0 * std::abs(s.dx);
    }
    std::vector<double> routed = routeEdge(s, e);
    if (s.wire) {
      routed.insert(routed.begin(), {centerX(a), Y[nodeRow[a]] + half});
    }
    if (e.wire) {
      routed.push_back(centerX(b));
      routed.push_back(Y[nodeRow[b]] + half);
    }
    std::vector<double> points = simplifyPoints(routed);
    if (points.size() < 4) continue;
    std::string id = escText(Pele::cat({edge.lhsId, "-", edge.rhsId}));
    Pele::appendCat(edgesOut, {"<g class=\"pele-edge\" data-id=\"", id, "\">", "<path d=\"", pathData(points, edge.lhsInto && !s.wire ? trimLength + s.gap : 0.0, edge.rhsInto && !e.wire ? trimLength + e.gap : 0.0), "\"/>", (edge.lhsInto ? marker("arrow_point", s.x + s.dx * s.gap, s.y + s.dy * s.gap, -s.dx, -s.dy, "var(--_l)") : ""), (edge.rhsInto ? marker("arrow_point", e.x + e.dx * e.gap, e.y + e.dy * e.gap, -e.dx, -e.dy, "var(--_l)") : ""), "</g>"});

    int best = -1;
    double bestLength = -1.0;
    int flat = -1;
    double flatLength = -1.0;
    const Label& label = edgeLabels[k];
    for (size_t i = 0; i < points.size(); i += 2) {
      minX = std::min(minX, points[i] - 4.0);
      maxX = std::max(maxX, points[i] + 4.0);
      minY = std::min(minY, points[i + 1] - 4.0);
      maxY = std::max(maxY, points[i + 1] + 4.0);
    }
    for (size_t i = 0; i + 3 < points.size(); i += 2) {
      bool horizontal = std::abs(points[i + 1] - points[i + 3]) < 0.01;
      double length = std::abs(points[i] - points[i + 2]) + std::abs(points[i + 1] - points[i + 3]);
      if (!horizontal) {
        runs.push_back(points[i]);
        runs.push_back(std::min(points[i + 1], points[i + 3]));
        runs.push_back(std::max(points[i + 1], points[i + 3]));
      }
      if (length > bestLength) {
        best = static_cast<int>(i);
        bestLength = length;
      }
      if (horizontal && length > flatLength) {
        flat = static_cast<int>(i);
        flatLength = length;
      }
    }
    if (label.width > 0.0) {
      int at = (flatLength >= label.width + 8.0) ? flat : best;
      double lx = (points[at] + points[at + 2]) / 2.0;
      double ly = (points[at + 1] + points[at + 3]) / 2.0;
      double lw = label.width + 8.0;
      bool upright = std::abs(points[at] - points[at + 2]) < 0.01;
      for (const Port& p : {s, e}) {
        if (lx + lw / 2.0 <= p.x0 || lx - lw / 2.0 >= p.x1 || ly + label.height / 2.0 <= p.y0 || ly - label.height / 2.0 >= p.y1) continue;
        if (upright) lx = lx >= (p.x0 + p.x1) / 2.0 ? (p.x1 + lw / 2.0 + 4.0) : (p.x0 - lw / 2.0 - 4.0);
        else ly = ly >= (p.y0 + p.y1) / 2.0 ? (p.y1 + label.height / 2.0 + 4.0) : (p.y0 - label.height / 2.0 - 4.0);
      }
      minX = std::min(minX, lx - lw / 2.0);
      maxX = std::max(maxX, lx + lw / 2.0);
      minY = std::min(minY, ly - label.height / 2.0);
      maxY = std::max(maxY, ly + label.height / 2.0);
      labelsOut += edgeLabelSvg(Pele::cat({edge.lhsId, "-", edge.rhsId}),  label,  lx,  ly,  "",  icons);
    }
  }

  std::string clusters;
  for (int g = 0; g < G; g++) {
    const auto& group = groups[g];
    double gx = left(g);
    double gy = top(g);
    double gw = right(g) - gx;
    const Label& label = groupLabels[g];
    std::string head;
    if (headW[g] > 0.0) {
      double band = gy + headH[g] + 12.0;
      std::vector<double> crossings;
      for (size_t i = 0; i < runs.size(); i += 3) {
        if (runs[i + 1] < band && runs[i + 2] > gy && runs[i] > gx && runs[i] < gx + gw) crossings.push_back(runs[i]);
      }
      std::sort(crossings.begin(), crossings.end());
      crossings.push_back(gx + gw - 4.0);
      double start = gx + 10.0;
      for (double crossing : crossings) {
        if (crossing - 6.0 - start >= headW[g]) break;
        start = std::max(start, crossing + 6.0);
      }
      if (start + headW[g] > gx + gw - 4.0) start = gx + 10.0;
      double cy = gy + 6.0 + headH[g] / 2.0;
      double textX = start;
      if (group.icon.has_value()) {
        head += iconSlot(*group.icon, start, cy - badge / 2.0, badge, icons);
        textX += badge + 6.0;
      }
      head += labelSvg(label, textX + label.width / 2.0, cy, " class=\"pele-cluster-label\" fill=\"var(--_m)\"", icons);
    }
    clusters +=
      "<g class=\"pele-cluster\" data-id=\"" + escText(group.id) + "\">" +
      "<rect x=\"" + num(gx) + "\" y=\"" + num(gy) + "\" width=\"" + num(gw) + "\" height=\"" + num(bottom(g) - gy) + "\" fill=\"var(--_a)\" fill-opacity=\"0.5\" stroke=\"var(--_b)\"" + std::string(ROUNDED) + "/>" +
      head +
      "</g>";
  }

  std::string nodesOut;
  double glyph = iconSize * 0.5;
  double textSize = std::max(6.0, std::round(size * 0.75));
  for (int i = 0; i < N; i++) {
    const auto& node = nodes[i];
    std::string at = Pele::cat({"data-id=\"", escText(node.id), "\" transform=\"translate(", num(centerX(i)), ",", num(Y[nodeRow[i]]), ")\""});
    if (node.type == "junction") {
      Pele::appendCat(nodesOut, {"<g class=\"pele-node pele-junction\" ", at, "><circle cy=\"", num(half), "\" r=\"", num(DOT), "\" fill=\"var(--_l)\"/></g>"});
      continue;
    }
    std::string inner;
    if (node.icon.has_value()) {
      inner = iconSlot(*node.icon, -glyph / 2.0, (iconSize - glyph) / 2.0, glyph, icons);
    } else if (node.iconText.has_value()) {
      Label text = layoutLabel(*node.iconText, false, measurer, textSize, iconSize - 8.0);
      size_t lines = static_cast<size_t>(std::max(1.0, std::floor((iconSize - 4.0) / (text.lineHeight > 0.0 ? text.lineHeight : 1.0))));
      inner = labelSvg(fitLabel(text, iconSize - 8.0, lines, measurer),  0.0,  half, Pele::cat({" class=\"pele-icon-text\" font-size=\"", num(textSize), "\""}),  icons);
    }
    nodesOut +=
      "<g class=\"pele-node pele-service\" " + at + ">" +
      "<rect x=\"" + num(-half) + "\" width=\"" + num(iconSize) + "\" height=\"" + num(iconSize) + "\" fill=\"var(--_s)\" stroke=\"var(--_b)\"" + std::string(ROUNDED) + "/>" +
      inner +
      labelSvg(labels[i], 0.0, iconSize + LABEL_GAP + labels[i].height / 2.0, " class=\"pele-label\"", icons) +
      "</g>";
  }

  Label title = layoutLabel(model.title, false, measurer, size, tight ? titleRoom(options) : 4000.0, Style::Bold);
  double titleHeight = title.height > 0.0 ? title.height + 12.0 : 0.0;
  double width = std::ceil(std::max(maxX - minX, title.width) + 2.0 * pad);
  double height = std::ceil(maxY - minY + titleHeight + 2.0 * pad);
  double ox = std::round((width - (maxX - minX)) / 2.0 - minX);
  double oy = std::round(pad + titleHeight - minY);
  std::string body =
    (!clusters.empty() ? Pele::cat({"<g class=\"pele-clusters\">", clusters, "</g>"}) : "") +
    (!edgesOut.empty() ? Pele::cat({"<g class=\"pele-edges\" fill=\"none\" stroke=\"var(--_l)\" stroke-linecap=\"round\" stroke-linejoin=\"round\">", edgesOut, "</g>"}) : "") +
    (!labelsOut.empty() ? Pele::cat({"<g class=\"pele-edge-labels\" font-size=\"", num(small), "\">", labelsOut, "</g>"}) : "") +
    (!nodesOut.empty() ? Pele::cat({"<g class=\"pele-nodes\">", nodesOut, "</g>"}) : "");

  std::string svg = svgDocument(
    "architecture",
    width,
    height,
    size,
    options,
    model,
    labelSvg(title, width / 2.0, pad + title.height / 2.0, " class=\"pele-title\" font-weight=\"var(--_tw)\"") +
      (!body.empty() ? Pele::cat({"<g transform=\"translate(", num(ox), ",", num(oy), ")\">", body, "</g>"}) : "")
  );
  return {svg, width, height, {}};
}

inline Rendered renderArchitectureModel(const ArchitectureDb& model, const Config& config, const RenderOptions& options) noexcept {
  return tighten(options, [&](bool tight) {
    return draw(model, config, options, tight);
  });
}

} // namespace architecture_detail

class ArchitectureDiagram {
public:
  static Rendered render(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options) {
    architecture_detail::ArchitectureDb db;
    if (!title.empty()) db.title = title;
    architecture_detail::populate(architecture_detail::parseArchitecture(source), db);
    return architecture_detail::renderArchitectureModel(db, config, options);
  }
};

inline Rendered renderArchitecture(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options) {
  return ArchitectureDiagram::render(source, config, title, options);
}

inline Rendered renderArchitecture(const std::string& raw, RenderOptions options = {}) {
  auto pre = preprocess(raw);
  std::string parsed = Pele::cat({encodeEntities(pre.text), "\n"});
  const auto* section = pre.config.get("architecture");
  bool fixed = (section && section->isMap() && section->get("useMaxWidth") && section->get("useMaxWidth")->isBool() && !section->get("useMaxWidth")->bVal);
  if (!options.hasResponsive) {
    options.responsive = !fixed;
    options.hasResponsive = true;
  }
  return ArchitectureDiagram::render(parsed, pre.config, pre.title, options);
}

} // namespace Pele
