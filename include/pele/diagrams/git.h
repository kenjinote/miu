#pragma once

#include "../foundation.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Pele {
namespace GitDiagram {

namespace CommitType {
constexpr int NORMAL = 0;
constexpr int REVERSE = 1;
constexpr int HIGHLIGHT = 2;
constexpr int MERGE = 3;
constexpr int CHERRY_PICK = 4;
} // namespace CommitType

struct GitCommit {
  std::string id;
  std::string message;
  int seq = 0;
  int type = CommitType::NORMAL;
  std::vector<std::string> tags;
  std::vector<std::string> parents;
  std::string branch;
  std::optional<int> customType;
  bool customId = false;
};

struct GitModel : DiagramMeta {
  std::string direction = "LR";
  std::vector<std::string> commitOrder;
  std::unordered_map<std::string, GitCommit> commits;
  std::unordered_map<std::string, std::optional<std::string>> branches;
  std::vector<std::string> lanes;
  std::string currentBranch;
};

enum class TokKind {
  cherryPick = 0,
  gitGraphColon = 1,
  highlight = 2,
  gitGraph = 3,
  checkout = 4,
  reverse = 5,
  parent = 6,
  commit = 7,
  normal = 8,
  branch = 9,
  order = 10,
  switchTok = 11,
  type = 12,
  merge = 13,
  msg = 14,
  tag = 15,
  id = 16,
  lr = 17,
  tb = 18,
  bt = 19,
  colon = 20,
  accDescr = 21,
  accTitle = 22,
  title = 23,
  intTok = 24,
  stringTok = 25,
  newline = 26,
  reference = 31
};

struct TokStream {
  std::vector<int> kinds;
  std::vector<std::string> texts;
};

inline bool isAsciiWordChar(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

inline int matchReference(const std::string& src, int p) {
  int n = static_cast<int>(src.size());
  if (p >= n || !isAsciiWordChar(src[p])) return -1;
  int e = p + 1;
  while (e < n && (isAsciiWordChar(src[e]) || src[e] == '-' || src[e] == '.' || src[e] == '/')) {
    e++;
  }
  while (e > p + 1 && !(isAsciiWordChar(src[e - 1]) || src[e - 1] == '-')) {
    e--;
  }
  return e;
}

inline bool matchRefSuffix(const std::string& src, int p) {
  int n = static_cast<int>(src.size());
  int e = p;
  while (e < n && (isAsciiWordChar(src[e]) || src[e] == '-' || src[e] == '.' || src[e] == '/')) {
    e++;
  }
  while (e > p && !(isAsciiWordChar(src[e - 1]) || src[e - 1] == '-')) {
    e--;
  }
  return e > p;
}

inline bool startsWithExact(const std::string& src, int p, const std::string& w) {
  if (p + static_cast<int>(w.size()) > static_cast<int>(src.size())) return false;
  return src.compare(static_cast<size_t>(p), w.size(), w) == 0;
}

inline bool startsWithCaseless(const std::string& src, int p, const std::string& w) {
  if (p + static_cast<int>(w.size()) > static_cast<int>(src.size())) return false;
  for (size_t i = 0; i < w.size(); ++i) {
    char c1 = src[static_cast<size_t>(p) + i];
    char c2 = w[i];
    if (c1 >= 'A' && c1 <= 'Z') c1 = static_cast<char>(c1 + 32);
    if (c2 >= 'A' && c2 <= 'Z') c2 = static_cast<char>(c2 + 32);
    if (c1 != c2) return false;
  }
  return true;
}

inline int matchWordOrRef(const std::string& src, int p, const std::string& w, bool caseless, int wordKind, int& outKind) {
  bool ok = caseless ? startsWithCaseless(src, p, w) : startsWithExact(src, p, w);
  if (!ok) return -1;
  int wEnd = p + static_cast<int>(w.size());
  if (caseless && wEnd < static_cast<int>(src.size()) && isAsciiWordChar(src[wEnd])) {
    outKind = static_cast<int>(TokKind::reference);
    return matchReference(src, p);
  }
  int rEnd = matchReference(src, p);
  if (rEnd > wEnd) {
    outKind = static_cast<int>(TokKind::reference);
    return rEnd;
  }
  outKind = wordKind;
  return wEnd;
}

inline TokStream tokenizeGit(const std::string& src) {
  TokStream out;
  int n = static_cast<int>(src.size());
  int p = 0;
  auto emit = [&](int k, int s, int e) {
    out.kinds.push_back(k);
    out.texts.push_back(src.substr(static_cast<size_t>(s), static_cast<size_t>(e - s)));
    p = e;
  };

  while (p < n) {
    int k = -1;
    int e = -1;

    if ((e = matchWordOrRef(src, p, "cherry-pick", false, static_cast<int>(TokKind::cherryPick), k)) != -1) { emit(k, p, e); continue; }
    if (startsWithExact(src, p, "gitGraph:")) { emit(static_cast<int>(TokKind::gitGraphColon), p, p + 9); continue; }
    if ((e = matchWordOrRef(src, p, "HIGHLIGHT", false, static_cast<int>(TokKind::highlight), k)) != -1) { emit(k, p, e); continue; }
    if ((e = matchWordOrRef(src, p, "gitGraph", true, static_cast<int>(TokKind::gitGraph), k)) != -1) { emit(k, p, e); continue; }
    if ((e = matchWordOrRef(src, p, "checkout", false, static_cast<int>(TokKind::checkout), k)) != -1) { emit(k, p, e); continue; }
    if ((e = matchWordOrRef(src, p, "REVERSE", false, static_cast<int>(TokKind::reverse), k)) != -1) { emit(k, p, e); continue; }
    if (startsWithExact(src, p, "parent:")) { emit(static_cast<int>(TokKind::parent), p, p + 7); continue; }
    if ((e = matchWordOrRef(src, p, "commit", false, static_cast<int>(TokKind::commit), k)) != -1) { emit(k, p, e); continue; }
    if ((e = matchWordOrRef(src, p, "NORMAL", false, static_cast<int>(TokKind::normal), k)) != -1) { emit(k, p, e); continue; }
    if ((e = matchWordOrRef(src, p, "branch", false, static_cast<int>(TokKind::branch), k)) != -1) { emit(k, p, e); continue; }
    if (startsWithExact(src, p, "order:")) { emit(static_cast<int>(TokKind::order), p, p + 6); continue; }
    if ((e = matchWordOrRef(src, p, "switch", false, static_cast<int>(TokKind::switchTok), k)) != -1) { emit(k, p, e); continue; }
    if (startsWithExact(src, p, "type:")) { emit(static_cast<int>(TokKind::type), p, p + 5); continue; }
    if ((e = matchWordOrRef(src, p, "merge", false, static_cast<int>(TokKind::merge), k)) != -1) { emit(k, p, e); continue; }
    if (startsWithExact(src, p, "msg:")) { emit(static_cast<int>(TokKind::msg), p, p + 4); continue; }
    if (startsWithExact(src, p, "tag:")) { emit(static_cast<int>(TokKind::tag), p, p + 4); continue; }
    if (startsWithExact(src, p, "id:")) { emit(static_cast<int>(TokKind::id), p, p + 3); continue; }
    if ((e = matchWordOrRef(src, p, "LR", false, static_cast<int>(TokKind::lr), k)) != -1) { emit(k, p, e); continue; }
    if ((e = matchWordOrRef(src, p, "TB", false, static_cast<int>(TokKind::tb), k)) != -1) { emit(k, p, e); continue; }
    if ((e = matchWordOrRef(src, p, "BT", false, static_cast<int>(TokKind::bt), k)) != -1) { emit(k, p, e); continue; }
    if (src[p] == ':') { emit(static_cast<int>(TokKind::colon), p, p + 1); continue; }

    {
      int q = p;
      while (q < n && (src[q] == ' ' || src[q] == '\t')) q++;
      if (startsWithExact(src, q, "accDescr")) {
        int r = q + 8;
        while (r < n && (src[r] == ' ' || src[r] == '\t' || src[r] == '\n' || src[r] == '\r')) r++;
        if (r < n && src[r] == ':') {
          r++;
          while (r < n && (src[r] == ' ' || src[r] == '\t')) r++;
          int le = r;
          while (le < n && src[le] != '\r' && src[le] != '\n') le++;
          if (le > r) { emit(static_cast<int>(TokKind::accDescr), p, le); continue; }
        } else if (r < n && src[r] == '{') {
          r++;
          int close = r;
          while (close < n && src[close] != '}') close++;
          if (close < n && src[close] == '}') {
            emit(static_cast<int>(TokKind::accDescr), p, close + 1);
            continue;
          }
        }
      }
      if (startsWithExact(src, q, "accTitle")) {
        int r = q + 8;
        while (r < n && (src[r] == ' ' || src[r] == '\t' || src[r] == '\n' || src[r] == '\r')) r++;
        if (r < n && src[r] == ':') {
          r++;
          while (r < n && (src[r] == ' ' || src[r] == '\t')) r++;
          int le = r;
          while (le < n && src[le] != '\r' && src[le] != '\n') le++;
          if (le > r) { emit(static_cast<int>(TokKind::accTitle), p, le); continue; }
        }
      }
    }

    if (startsWithCaseless(src, p, "title")) {
      int q = p + 5;
      if (!matchRefSuffix(src, q)) {
        int r = q;
        while (r < n && (src[r] == ' ' || src[r] == '\t')) r++;
        int le = r;
        while (le < n && src[le] != '\r' && src[le] != '\n') le++;
        if (le > r) {
          emit(static_cast<int>(TokKind::title), p, le);
          continue;
        }
      }
      if (q == n || src[q] == ' ' || src[q] == '\t' || src[q] == '\r' || src[q] == '\n') {
        emit(static_cast<int>(TokKind::title), p, q);
        continue;
      }
    }

    if (src[p] == '0') {
      emit(static_cast<int>(TokKind::intTok), p, p + 1);
      continue;
    }
    if (src[p] >= '1' && src[p] <= '9') {
      int q = p + 1;
      while (q < n && src[q] >= '0' && src[q] <= '9') q++;
      if (q < n && src[q] == '.') q--;
      if (q > p) {
        emit(static_cast<int>(TokKind::intTok), p, q);
        continue;
      }
    }

    if (src[p] == '"' || src[p] == '\'') {
      char quote = src[p];
      int q = p + 1;
      while (q < n && src[q] != quote) {
        if (src[q] == '\\' && q + 1 < n) q += 2;
        else q++;
      }
      if (q < n && src[q] == quote) {
        emit(static_cast<int>(TokKind::stringTok), p, q + 1);
        continue;
      }
    }

    if (src[p] == '\r' || src[p] == '\n') {
      int q = p + 1;
      while (q < n && (src[q] == '\r' || src[q] == '\n')) q++;
      emit(static_cast<int>(TokKind::newline), p, q);
      continue;
    }

    if (src[p] == ' ' || src[p] == '\t') {
      while (p < n && (src[p] == ' ' || src[p] == '\t')) p++;
      continue;
    }

    if ((src[p] == '%' && p + 1 < n && src[p + 1] == '%') || src[p] == '#') {
      while (p < n && src[p] != '\r' && src[p] != '\n') p++;
      continue;
    }

    e = matchReference(src, p);
    if (e != -1) {
      emit(static_cast<int>(TokKind::reference), p, e);
      continue;
    }

    p++;
  }
  return out;
}

inline std::string unescapeGitString(const std::string& tok) {
  if (tok.size() < 2) return "";
  std::string out;
  out.reserve(tok.size() - 2);
  for (size_t i = 1; i + 1 < tok.size(); ++i) {
    if (tok[i] == '\\' && i + 2 < tok.size()) {
      ++i;
      char c = tok[i];
      switch (c) {
        case 'b': out.push_back('\b'); break;
        case 'f': out.push_back('\f'); break;
        case 'n': out.push_back('\n'); break;
        case 'r': out.push_back('\r'); break;
        case 't': out.push_back('\t'); break;
        case 'v': out.push_back('\v'); break;
        default: out.push_back(c); break;
      }
    } else {
      out.push_back(tok[i]);
    }
  }
  return out;
}

inline std::string extractTitleVal(const std::string& tok) {
  if (tok.size() <= 5) return "";
  size_t i = 5;
  while (i < tok.size() && (tok[i] == ' ' || tok[i] == '\t')) i++;
  return tok.substr(i);
}

inline std::string extractAccTitleVal(const std::string& tok) {
  size_t colon = tok.find(':');
  if (colon == std::string::npos) return "";
  size_t i = colon + 1;
  while (i < tok.size() && (tok[i] == ' ' || tok[i] == '\t' || tok[i] == '\r' || tok[i] == '\n')) i++;
  return tok.substr(i);
}

inline std::string extractAccDescrVal(const std::string& tok) {
  size_t brace = tok.find('{');
  if (brace != std::string::npos && !tok.empty() && tok.back() == '}') {
    std::string inner = tok.substr(brace + 1, tok.size() - brace - 2);
    size_t a = 0;
    size_t b = inner.size();
    while (a < b && (inner[a] == ' ' || inner[a] == '\t' || inner[a] == '\r' || inner[a] == '\n')) a++;
    while (b > a && (inner[b - 1] == ' ' || inner[b - 1] == '\t' || inner[b - 1] == '\r' || inner[b - 1] == '\n')) b--;
    std::string trimmed = inner.substr(a, b - a);
    std::string res;
    for (size_t i = 0; i < trimmed.size(); ++i) {
      res.push_back(trimmed[i]);
      if (trimmed[i] == '\n') {
        while (i + 1 < trimmed.size() && (trimmed[i + 1] == ' ' || trimmed[i + 1] == '\t' || trimmed[i + 1] == '\r')) {
          i++;
        }
      }
    }
    return res;
  }
  size_t colon = tok.find(':');
  if (colon == std::string::npos) return "";
  size_t i = colon + 1;
  while (i < tok.size() && (tok[i] == ' ' || tok[i] == '\t')) i++;
  return tok.substr(i);
}

struct StatementAst {
  std::string kind; // Commit, Branch, Merge, Checkout, CherryPicking
  std::optional<std::string> id;
  std::optional<std::string> message;
  std::vector<std::string> tags;
  std::optional<std::string> type;
  std::string name;
  std::optional<int> order;
  std::string branch;
  std::optional<std::string> parent;
};

struct GitGraphAst {
  std::optional<std::string> dir;
  std::optional<std::string> title;
  std::optional<std::string> accTitle;
  std::optional<std::string> accDescr;
  std::vector<StatementAst> statements;
};

inline GitGraphAst parseGit(const std::string& src) {
  TokStream ts = tokenizeGit(src);
  size_t i = 0;
  auto kind = [&]() -> int {
    return i < ts.kinds.size() ? ts.kinds[i] : -1;
  };
  auto accept = [&](TokKind k) -> bool {
    if (kind() == static_cast<int>(k)) {
      i++;
      return true;
    }
    return false;
  };
  auto take = [&]() -> std::string {
    if (i < ts.texts.size()) return ts.texts[i++];
    return "";
  };
  auto endOfLine = [&]() {
    while (kind() == static_cast<int>(TokKind::newline)) i++;
  };
  auto parseName = [&]() -> std::string {
    if (kind() == static_cast<int>(TokKind::reference)) return take();
    if (kind() == static_cast<int>(TokKind::stringTok)) return unescapeGitString(take());
    return "";
  };
  auto parseString = [&]() -> std::string {
    if (kind() == static_cast<int>(TokKind::stringTok)) return unescapeGitString(take());
    return "";
  };
  auto parseCommitType = [&]() -> std::string {
    if (kind() == static_cast<int>(TokKind::normal) ||
        kind() == static_cast<int>(TokKind::reverse) ||
        kind() == static_cast<int>(TokKind::highlight)) {
      return take();
    }
    return "NORMAL";
  };

  GitGraphAst ast;
  while (kind() == static_cast<int>(TokKind::newline)) i++;
  if (!accept(TokKind::gitGraphColon)) {
    accept(TokKind::gitGraph);
    int next = (i + 1 < ts.kinds.size()) ? ts.kinds[i + 1] : -1;
    if ((kind() == static_cast<int>(TokKind::lr) ||
         kind() == static_cast<int>(TokKind::tb) ||
         kind() == static_cast<int>(TokKind::bt)) &&
        next == static_cast<int>(TokKind::colon)) {
      ast.dir = take();
      i++;
    } else {
      accept(TokKind::colon);
    }
  }

  while (kind() != -1) {
    int k = kind();
    if (k == static_cast<int>(TokKind::newline)) {
      i++;
    } else if (k == static_cast<int>(TokKind::accDescr)) {
      ast.accDescr = extractAccDescrVal(take());
      endOfLine();
    } else if (k == static_cast<int>(TokKind::accTitle)) {
      ast.accTitle = extractAccTitleVal(take());
      endOfLine();
    } else if (k == static_cast<int>(TokKind::title)) {
      ast.title = extractTitleVal(take());
      endOfLine();
    } else if (k == static_cast<int>(TokKind::commit)) {
      i++;
      StatementAst st;
      st.kind = "Commit";
      for (;;) {
        if (accept(TokKind::id)) st.id = parseString();
        else if (accept(TokKind::tag)) st.tags.push_back(parseString());
        else if (accept(TokKind::type)) st.type = parseCommitType();
        else if (accept(TokKind::msg) || kind() == static_cast<int>(TokKind::stringTok)) st.message = parseString();
        else break;
      }
      endOfLine();
      ast.statements.push_back(std::move(st));
    } else if (k == static_cast<int>(TokKind::branch)) {
      i++;
      StatementAst st;
      st.kind = "Branch";
      st.name = parseName();
      if (accept(TokKind::order)) {
        if (kind() == static_cast<int>(TokKind::intTok)) {
          st.order = std::stoi(take());
        }
      }
      endOfLine();
      ast.statements.push_back(std::move(st));
    } else if (k == static_cast<int>(TokKind::merge)) {
      i++;
      StatementAst st;
      st.kind = "Merge";
      st.branch = parseName();
      for (;;) {
        if (accept(TokKind::id)) st.id = parseString();
        else if (accept(TokKind::tag)) st.tags.push_back(parseString());
        else if (accept(TokKind::type)) st.type = parseCommitType();
        else break;
      }
      endOfLine();
      ast.statements.push_back(std::move(st));
    } else if (k == static_cast<int>(TokKind::checkout) || k == static_cast<int>(TokKind::switchTok)) {
      i++;
      StatementAst st;
      st.kind = "Checkout";
      st.branch = parseName();
      endOfLine();
      ast.statements.push_back(std::move(st));
    } else if (k == static_cast<int>(TokKind::cherryPick)) {
      i++;
      StatementAst st;
      st.kind = "CherryPicking";
      for (;;) {
        if (accept(TokKind::id)) st.id = parseString();
        else if (accept(TokKind::tag)) st.tags.push_back(parseString());
        else if (accept(TokKind::parent)) st.parent = parseString();
        else break;
      }
      endOfLine();
      ast.statements.push_back(std::move(st));
    } else {
      i++;
    }
  }
  return ast;
}

inline int typeCode(const std::string& name) {
  if (name == "REVERSE") return CommitType::REVERSE;
  if (name == "HIGHLIGHT") return CommitType::HIGHLIGHT;
  return CommitType::NORMAL;
}

inline GitModel buildGit(const GitGraphAst& ast, const Config& config, const std::string& title) {
  const Config emptyObj = Config::makeMap();
  const Config* gc = config.isMap() ? config.get("gitGraph") : nullptr;
  const Config& git = (gc && gc->isMap()) ? *gc : emptyObj;
  const Config* mbn = git.isMap() ? git.get("mainBranchName") : nullptr;
  std::string mainName = (mbn && mbn->isString()) ? mbn->sVal : "main";
  const Config* mbo = git.isMap() ? git.get("mainBranchOrder") : nullptr;
  double mainOrder = (mbo && mbo->isNumber()) ? mbo->nVal : 0.0;

  GitModel model;
  model.branches[mainName] = std::nullopt;
  std::vector<std::pair<std::string, double>> orders;
  orders.push_back({mainName, mainOrder});
  std::string headId;
  bool hasHead = false;
  std::string current = mainName;
  std::string direction = ast.dir.value_or("LR");
  int seq = 0;

  auto nextId = [&]() -> std::string {
    for (uint32_t salt = static_cast<uint32_t>(seq + 1); ; salt += 0x9e3779b1u) {
      uint32_t h = salt * 0x9e3779b1u;
      char buf[16];
      std::snprintf(buf, sizeof(buf), "%08x", h);
      std::string hex7(buf, 7);
      std::string id = std::to_string(seq) + "-" + hex7;
      if (model.commits.find(id) == model.commits.end()) return id;
    }
  };

  auto add = [&](const GitCommit& c) {
    headId = c.id;
    hasHead = true;
    if (model.commits.find(c.id) == model.commits.end()) {
      model.commitOrder.push_back(c.id);
    }
    model.commits[c.id] = c;
    model.branches[current] = c.id;
  };

  auto checkout = [&](const std::string& br) {
    auto it = model.branches.find(br);
    if (it == model.branches.end()) return;
    current = br;
    if (it->second.has_value() && model.commits.find(it->second.value()) != model.commits.end()) {
      headId = it->second.value();
      hasHead = true;
    } else {
      headId.clear();
      hasHead = false;
    }
  };

  for (const auto& st : ast.statements) {
    if (st.kind == "Commit") {
      GitCommit c;
      c.id = (st.id.has_value() && !st.id->empty()) ? *st.id : nextId();
      c.message = st.message.value_or("");
      c.seq = seq++;
      c.type = st.type.has_value() ? typeCode(*st.type) : CommitType::NORMAL;
      c.tags = st.tags;
      if (hasHead) c.parents.push_back(headId);
      c.branch = current;
      add(c);
    } else if (st.kind == "Branch") {
      if (model.branches.find(st.name) != model.branches.end()) continue;
      model.branches[st.name] = hasHead ? std::optional<std::string>(headId) : std::nullopt;
      orders.push_back({st.name, static_cast<double>(st.order.value_or(0))});
      checkout(st.name);
    } else if (st.kind == "Merge") {
      const std::string& other = st.branch;
      auto curIt = model.branches.find(current);
      auto othIt = model.branches.find(other);
      if (curIt == model.branches.end() || othIt == model.branches.end()) continue;
      if (!curIt->second.has_value() || !othIt->second.has_value()) continue;
      std::string otherHead = othIt->second.value();
      std::string customIdStr = st.id.value_or("");
      GitCommit c;
      c.id = !customIdStr.empty() ? customIdStr : nextId();
      c.message = "merged branch " + other + " into " + current;
      c.seq = seq++;
      if (hasHead) {
        c.parents.push_back(headId);
        c.parents.push_back(otherHead);
      }
      c.branch = current;
      c.type = CommitType::MERGE;
      if (st.type.has_value()) c.customType = typeCode(*st.type);
      c.customId = !customIdStr.empty();
      c.tags = st.tags;
      add(c);
    } else if (st.kind == "Checkout") {
      checkout(st.branch);
    } else if (st.kind == "CherryPicking") {
      if (!st.id.has_value()) continue;
      auto srcIt = model.commits.find(*st.id);
      if (srcIt == model.commits.end()) continue;
      const GitCommit& source = srcIt->second;
      GitCommit c;
      c.id = nextId();
      c.message = "cherry-picked " + source.message + " into " + current;
      c.seq = seq++;
      if (hasHead) {
        c.parents.push_back(headId);
        c.parents.push_back(source.id);
      }
      c.branch = current;
      c.type = CommitType::CHERRY_PICK;
      if (!st.tags.empty()) {
        for (const auto& tg : st.tags) {
          if (!tg.empty()) c.tags.push_back(tg);
        }
      } else {
        std::string autoTag = "cherry-pick:" + source.id;
        if (source.type == CommitType::MERGE) {
          autoTag += "|parent:" + st.parent.value_or("");
        }
        c.tags.push_back(autoTag);
      }
      add(c);
    }
  }

  std::stable_sort(orders.begin(), orders.end(), [](const auto& a, const auto& b) {
    return a.second < b.second;
  });
  for (const auto& p : orders) {
    model.lanes.push_back(p.first);
  }
  model.direction = direction;
  model.currentBranch = current;
  model.title = (ast.title.has_value() && !ast.title->empty()) ? *ast.title : title;
  if (ast.accTitle.has_value()) model.accTitle = *ast.accTitle;
  if (ast.accDescr.has_value()) model.accDescr = *ast.accDescr;
  return model;
}

inline bool isBrTagAt(const std::string& s, size_t pos) {
  size_t i = pos + 1;
  if (i + 1 >= s.size()) return false;
  char c1 = s[i];
  char c2 = s[i + 1];
  if ((c1 != 'b' && c1 != 'B') || (c2 != 'r' && c2 != 'R')) return false;
  i += 2;
  while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')) i++;
  if (i < s.size() && s[i] == '/') i++;
  return i < s.size() && s[i] == '>';
}

inline Label plainLabel(const std::string& raw, const TextMeasurer& measurer, double size, double max, int base = 0) {
  std::string text;
  text.reserve(raw.size());
  for (size_t i = 0; i < raw.size(); ++i) {
    if (raw[i] == '<' && !isBrTagAt(raw, i)) {
      text += "&lt;";
    } else if (raw[i] == ':' && i + 4 <= raw.size() && raw.compare(i, 4, ":fa-") == 0) {
      text += "&colon;fa-";
      i += 3;
    } else {
      text.push_back(raw[i]);
    }
  }
  return layoutLabel(text, false, measurer, size, max, base);
}

inline bool flagOpt(const Config& config, const std::string& key, bool fallback) {
  if (!config.isMap()) return fallback;
  const Config* gc = config.get("gitGraph");
  if (!gc || !gc->isMap()) return fallback;
  const Config* v = gc->get(key);
  return (v && v->isBool()) ? v->bVal : fallback;
}

inline double tagWidth(const Label& tag) {
  constexpr double NOTCH = 7.0;
  return NOTCH + 4.0 + tag.width + 6.0;
}

inline std::string symbolSvg(int type, double x, double y, const std::string& color) {
  constexpr double DOT = 7.0;
  std::string at = "cx=\"" + num(x) + "\" cy=\"" + num(y) + "\"";
  if (type == CommitType::HIGHLIGHT) {
    return "<rect x=\"" + num(x - DOT - 1.0) + "\" y=\"" + num(y - DOT - 1.0) + "\" width=\"" + num(2.0 * DOT + 2.0) + "\" height=\"" + num(2.0 * DOT + 2.0) + "\" rx=\"2\" fill=\"" + color + "\"/>";
  }
  if (type == CommitType::MERGE) {
    return "<circle " + at + " r=\"" + num(DOT - 1.0) + "\" fill=\"var(--_bg)\" stroke=\"" + color + "\" stroke-width=\"2\"/><circle " + at + " r=\"2.5\" fill=\"" + color + "\"/>";
  }
  std::string dot = "<circle " + at + " r=\"" + num(DOT) + "\" fill=\"" + color + "\"/>";
  if (type == CommitType::REVERSE) {
    return dot + "<path d=\"M" + num(x - 3.0) + "," + num(y - 3.0) + "l6,6M" + num(x - 3.0) + "," + num(y + 3.0) + "l6,-6\" fill=\"none\" stroke=\"var(--_bg)\" stroke-width=\"1.5\" stroke-linecap=\"round\"/>";
  }
  if (type == CommitType::CHERRY_PICK) {
    return dot +
      "<path d=\"M" + num(x - 2.6) + "," + num(y + 1.0) + "L" + num(x + 0.8) + "," + num(y - 4.2) + "L" + num(x + 2.6) + "," + num(y + 1.0) + "\" fill=\"none\" stroke=\"var(--_bg)\" stroke-linejoin=\"round\"/>" +
      "<circle cx=\"" + num(x - 2.6) + "\" cy=\"" + num(y + 2.2) + "\" r=\"1.8\" fill=\"var(--_bg)\"/>" +
      "<circle cx=\"" + num(x + 2.6) + "\" cy=\"" + num(y + 2.2) + "\" r=\"1.8\" fill=\"var(--_bg)\"/>";
  }
  return dot;
}

template <typename DrawFn>
inline Rendered turnToFit(const RenderOptions& options, DrawFn&& drawFn) {
  Rendered across = drawFn(false);
  double max = options.maxWidth;
  if (!(max > 0.0) || !std::isfinite(max) || across.width <= max) return across;
  Rendered down = drawFn(true);
  return down.width < across.width ? down : across;
}

inline Rendered drawGit(const GitModel& model, const Config& config, const RenderOptions& options, const std::string& direction) {
  constexpr double DOT = 7.0;
  constexpr double STEP = 44.0;
  constexpr double LANE = 44.0;
  constexpr double CURVE = 36.0;
  constexpr double GAP = 8.0;
  constexpr double LABEL_GAP = 20.0;
  constexpr double TAIL = 20.0;
  constexpr double NOTCH = 7.0;
  constexpr double TAG_GAP = 6.0;
  const double HALF = std::sqrt(0.5);
  static const char* const CLASSES[] = {"normal", "reverse", "highlight", "merge", "cherry-pick"};

  double size = options.hasFontSize ? options.fontSize : 16.0;
  double small = std::round(size * 0.75);
  const TextMeasurer& measurer = getMeasurer(options);
  double pad = options.hasPadding ? options.padding : 8.0;

  bool showBranches = flagOpt(config, "showBranches", true);
  bool showCommitLabel = flagOpt(config, "showCommitLabel", true);
  bool rotate = flagOpt(config, "rotateCommitLabel", true);
  bool parallel = flagOpt(config, "parallelCommits", false);
  bool vertical = direction != "LR";
  bool flip = direction == "BT";

  std::unordered_map<std::string, int> laneOf;
  for (size_t i = 0; i < model.lanes.size(); ++i) {
    laneOf[model.lanes[i]] = static_cast<int>(i);
  }
  int laneCount = static_cast<int>(model.lanes.size());
  double medium = std::round(size * 0.875);
  std::vector<Label> names;
  names.reserve(model.lanes.size());
  for (const auto& name : model.lanes) {
    names.push_back(plainLabel(showBranches ? name : "", measurer, medium, 4000.0));
  }

  struct View {
    GitCommit commit;
    int index = 0;
    int lane = 0;
    double m = 0.0;
    std::optional<Label> label;
    std::vector<Label> tags;
  };

  std::vector<GitCommit> sortedCommits;
  sortedCommits.reserve(model.commitOrder.size());
  for (const auto& cid : model.commitOrder) {
    auto it = model.commits.find(cid);
    if (it != model.commits.end()) sortedCommits.push_back(it->second);
  }
  std::stable_sort(sortedCommits.begin(), sortedCommits.end(), [](const GitCommit& a, const GitCommit& b) {
    return a.seq < b.seq;
  });

  std::vector<View> views;
  views.reserve(sortedCommits.size());
  std::unordered_map<std::string, int> byId;
  for (const auto& commit : sortedCommits) {
    bool labelled = showCommitLabel &&
                    commit.type != CommitType::CHERRY_PICK &&
                    (commit.type != CommitType::MERGE || commit.customId);
    std::vector<Label> tags;
    for (const auto& tg : commit.tags) {
      Label lb = plainLabel(tg, measurer, small, 4000.0);
      if (!lb.lines.empty()) tags.push_back(std::move(lb));
    }
    int idx = static_cast<int>(views.size());
    int ln = 0;
    auto lit = laneOf.find(commit.branch);
    if (lit != laneOf.end()) ln = lit->second;
    std::optional<Label> lbl = labelled ? std::optional<Label>(plainLabel(commit.id, measurer, small, 4000.0)) : std::nullopt;
    views.push_back({commit, idx, ln, 0.0, std::move(lbl), std::move(tags)});
    byId[commit.id] = idx;
  }

  std::vector<double> half(views.size(), 0.0);
  std::vector<double> tagHalf(views.size(), 0.0);
  std::vector<double> lo(laneCount, DOT);
  std::vector<double> hi(laneCount, DOT);

  for (size_t k = 0; k < views.size(); ++k) {
    const auto& view = views[k];
    double tagsMain = 0.0;
    double tagsCross = 0.0;
    for (const auto& tag : view.tags) {
      double w = tagWidth(tag);
      if (vertical) {
        tagsMain = std::max(tagsMain, tag.height);
        tagsCross += w + 4.0;
      } else {
        tagsMain = std::max(tagsMain, w);
        tagsCross += tag.height + 3.0;
      }
    }
    double labelMain = 0.0;
    double labelCross = 0.0;
    if (view.label.has_value() && !view.label->lines.empty()) {
      if (rotate) {
        labelCross = 3.0 + HALF * (view.label->width + view.label->height);
      } else {
        labelMain = vertical ? view.label->height : view.label->width;
        labelCross = 6.0 + (vertical ? view.label->width : view.label->height);
      }
    }
    half[k] = std::max({DOT, tagsMain / 2.0, labelMain / 2.0});
    tagHalf[k] = tagsMain / 2.0;
    double tagSide = DOT + (!view.tags.empty() ? TAG_GAP + tagsCross : 0.0);
    double labelSide = DOT + labelCross;
    int lane = view.lane;
    if (lane >= 0 && lane < laneCount) {
      lo[lane] = std::max(lo[lane], vertical ? labelSide : tagSide);
      hi[lane] = std::max(hi[lane], vertical ? tagSide : labelSide);
    }
  }

  std::vector<double> lead(views.size(), 0.0);
  if (!vertical) {
    for (size_t k = 0; k < views.size(); ++k) {
      if (tagHalf[k] == 0.0) continue;
      for (const auto& pid : views[k].commit.parents) {
        auto pit = byId.find(pid);
        if (pit != byId.end() && views[pit->second].lane < views[k].lane) {
          lead[k] = tagHalf[k] + 4.0;
        }
      }
    }
  }

  auto space = [&](size_t p, size_t k) -> double {
    return std::max({STEP, half[p] + half[k] + GAP, lead[k] > 0.0 ? half[p] + CURVE + lead[k] : 0.0});
  };

  for (size_t k = 0; k < views.size(); ++k) {
    if (!parallel) {
      views[k].m = (k == 0) ? 0.0 : (views[k - 1].m + space(k - 1, k));
      continue;
    }
    double m = 0.0;
    for (const auto& pid : views[k].commit.parents) {
      auto pit = byId.find(pid);
      if (pit == byId.end() || static_cast<size_t>(pit->second) >= k) continue;
      size_t pIdx = static_cast<size_t>(pit->second);
      m = std::max(m, views[pIdx].m + space(pIdx, k));
    }
    views[k].m = m;
  }

  double end = 0.0;
  for (const auto& view : views) end = std::max(end, view.m);

  std::vector<double> cross(laneCount, 0.0);
  for (int i = 0; i < laneCount; ++i) {
    const auto& name = names[i];
    double reach = name.lines.empty() ? 0.0 : (vertical ? name.width / 2.0 + 8.0 : name.height / 2.0 + 2.0);
    lo[i] = std::max(lo[i], reach);
    hi[i] = std::max(hi[i], reach);
    if (i > 0) cross[i] = cross[i - 1] + std::max(LANE, hi[i - 1] + lo[i] + GAP);
  }

  auto px = [&](double m, double c) -> double { return vertical ? c : m; };
  auto py = [&](double m, double c) -> double { return vertical ? (flip ? -m : m) : c; };
  auto point = [&](double m, double c) -> std::string { return num(px(m, c)) + "," + num(py(m, c)); };

  double minX = std::numeric_limits<double>::infinity();
  double minY = std::numeric_limits<double>::infinity();
  double maxX = -std::numeric_limits<double>::infinity();
  double maxY = -std::numeric_limits<double>::infinity();
  auto grow = [&](double x0, double y0, double x1, double y1) {
    if (x0 < minX) minX = x0;
    if (y0 < minY) minY = y0;
    if (x1 > maxX) maxX = x1;
    if (y1 > maxY) maxY = y1;
  };

  std::string branches;
  if (showBranches) {
    for (int i = 0; i < laneCount; ++i) {
      const auto& name = names[i];
      std::string color = seriesColor(i);
      double c = cross[i];
      double w = name.width + 16.0;
      double h = name.height + 4.0;
      double from = name.lines.empty() ? -LABEL_GAP : (-LABEL_GAP - (vertical ? h : w));
      double cx = px(from + (vertical ? h : w) / 2.0, c);
      double cy = py(from + (vertical ? h : w) / 2.0, c);
      grow(px(-LABEL_GAP, c), py(-LABEL_GAP, c), px(-LABEL_GAP, c), py(-LABEL_GAP, c));
      grow(px(end + TAIL, c), py(end + TAIL, c), px(end + TAIL, c), py(end + TAIL, c));
      branches +=
        "<g class=\"pele-branch\" data-id=\"" + escText(model.lanes[i]) + "\">" +
        "<path class=\"pele-lane\" d=\"M" + point(-LABEL_GAP, c) + "L" + point(end + TAIL, c) + "\" stroke=\"" + color + "\" stroke-dasharray=\"2 4\"/>";
      if (!name.lines.empty()) {
        grow(cx - w / 2.0, cy - h / 2.0, cx + w / 2.0, cy + h / 2.0);
        branches +=
          "<rect x=\"" + num(cx - w / 2.0) + "\" y=\"" + num(cy - h / 2.0) + "\" width=\"" + num(w) + "\" height=\"" + num(h) + "\" fill=\"" + color + "\"" + ROUNDED + "/>" +
          labelSvg(name, cx, cy, " class=\"pele-label\" fill=\"var(--_bg)\"");
      }
      branches += "</g>";
    }
  }

  std::vector<std::vector<double>> taken(laneCount);
  for (const auto& view : views) {
    if (view.lane >= 0 && view.lane < laneCount) {
      taken[view.lane].push_back(view.m);
    }
  }
  if (parallel) {
    for (auto& list : taken) std::sort(list.begin(), list.end());
  }
  auto busy = [&](int lane, double from, double to) -> bool {
    const auto& list = taken[lane];
    size_t low = 0;
    size_t high = list.size();
    while (low < high) {
      size_t mid = (low + high) >> 1;
      if (list[mid] <= from) low = mid + 1;
      else high = mid;
    }
    return low < list.size() && list[low] < to;
  };

  std::string curves;
  std::string lines;
  for (const auto& view : views) {
    const auto& commit = view.commit;
    double c2 = cross[view.lane];
    double m2 = view.m;
    for (size_t j = 0; j < commit.parents.size(); ++j) {
      auto pit = byId.find(commit.parents[j]);
      if (pit == byId.end() || pit->second == view.index) continue;
      const auto& parent = views[pit->second];
      double c1 = cross[parent.lane];
      double m1 = parent.m;
      if (parent.lane == view.lane) {
        lines += "<path class=\"pele-edge\" d=\"M" + point(m1, c1) + "L" + point(m2, c2) + "\" stroke=\"" + seriesColor(view.lane) + "\"/>";
        continue;
      }
      bool merged = j > 0 && commit.type == CommitType::MERGE;
      bool picked = j > 0 && !merged;
      std::string d = "M" + point(m1, c1);
      if (m2 <= m1) {
        double mid = (m1 + m2) / 2.0;
        d += "C" + point(mid, c1) + " " + point(mid, c2) + " " + point(m2, c2);
      } else if (!merged && !busy(view.lane, m1, m2)) {
        double s = std::min(CURVE, m2 - m1);
        d += "C" + point(m1 + s / 2.0, c1) + " " + point(m1 + s / 2.0, c2) + " " + point(m1 + s, c2) + "L" + point(m2, c2);
      } else {
        double to = (parent.lane < view.lane && m2 - lead[view.index] > m1) ? (m2 - lead[view.index]) : m2;
        double s = std::min(CURVE, to - m1);
        double along = c1;
        if (!merged || busy(parent.lane, m1, m2)) {
          along = c1 > c2 ? (c2 + hi[view.lane] + GAP / 2.0) : (c2 - lo[view.lane] - GAP / 2.0);
          s = std::min(CURVE, (to - m1) / 2.0);
          d += "C" + point(m1 + s / 2.0, c1) + " " + point(m1 + s / 2.0, along) + " " + point(m1 + s, along);
        }
        d += "L" + point(to - s, along) + "C" + point(to - s / 2.0, along) + " " + point(to - s / 2.0, c2) + " " + point(to, c2);
        if (to < m2) d += "L" + point(m2, c2);
      }
      curves +=
        "<path class=\"pele-edge" + std::string(merged ? " pele-edge-merge" : (picked ? " pele-edge-cherry-pick" : "")) + "\" d=\"" + d + "\" " +
        "stroke=\"" + seriesColor(merged ? parent.lane : view.lane) + "\"" + (picked ? " stroke-dasharray=\"4 3\"" : "") + "/>";
    }
  }

  std::string commitsOut;
  for (const auto& view : views) {
    const auto& commit = view.commit;
    double x = px(view.m, cross[view.lane]);
    double y = py(view.m, cross[view.lane]);
    int type = commit.customType.value_or(commit.type);
    grow(x - DOT - 1.0, y - DOT - 1.0, x + DOT + 1.0, y + DOT + 1.0);
    bool written = commit.type < CommitType::MERGE && !commit.message.empty();
    std::string body = written ? ("<title>" + escText(commit.message) + "</title>") : "";
    body += symbolSvg(type, x, y, seriesColor(view.lane));

    if (view.label.has_value() && !view.label->lines.empty()) {
      const auto& label = *view.label;
      double w = label.width;
      double h = label.height;
      auto back = [&](double cx, double cy) -> std::string {
        return "<rect x=\"" + num(cx - w / 2.0 - 2.0) + "\" y=\"" + num(cy - h / 2.0 + 2.0) + "\" width=\"" + num(w + 4.0) + "\" height=\"" + num(h - 4.0) + "\" rx=\"3\" fill=\"var(--_bg)\"/>";
      };
      if (rotate) {
        double k = (HALF * h) / 2.0;
        double ax = vertical ? (x - DOT - 3.0 - k) : (x + 3.0);
        double ay = vertical ? y : (y + DOT + 3.0 + k);
        double reach = HALF * w + k;
        grow(ax - reach, flip ? (ay - reach) : (ay - k), ax + k, flip ? (ay + k) : (ay + reach));
        body +=
          "<g class=\"pele-commit-label\" transform=\"rotate(" + std::string(flip ? "45" : "-45") + " " + num(ax) + " " + num(ay) + ")\">" +
          back(ax - w / 2.0, ay) +
          labelSvg(label, ax - w / 2.0, ay, "") +
          "</g>";
      } else {
        double cx = vertical ? (x - DOT - 6.0 - w / 2.0) : x;
        double cy = vertical ? y : (y + DOT + 6.0 + h / 2.0);
        grow(cx - w / 2.0, cy - h / 2.0, cx + w / 2.0, cy + h / 2.0);
        body += "<g class=\"pele-commit-label\">" + back(cx, cy) + labelSvg(label, cx, cy, "") + "</g>";
      }
    }

    double offset = DOT + TAG_GAP;
    int tStart = vertical ? 0 : static_cast<int>(view.tags.size()) - 1;
    int tStep = vertical ? 1 : -1;
    for (int t = tStart; t >= 0 && t < static_cast<int>(view.tags.size()); t += tStep) {
      const auto& tag = view.tags[t];
      double w = tagWidth(tag);
      double h = tag.height;
      double left = vertical ? (x + offset) : (x - w / 2.0);
      double cy = vertical ? y : (y - offset - h / 2.0);
      offset += vertical ? (w + 4.0) : (h + 3.0);
      grow(left, cy - h / 2.0, left + w, cy + h / 2.0);
      body +=
        "<g class=\"pele-tag\">" +
        std::string("<path d=\"M") + num(left) + "," + num(cy) + "L" + num(left + NOTCH) + "," + num(cy - h / 2.0) + "H" + num(left + w) + "V" + num(cy + h / 2.0) + "H" + num(left + NOTCH) + "Z\" fill=\"var(--_s)\" stroke=\"var(--_b)\" stroke-linejoin=\"round\"/>" +
        "<circle cx=\"" + num(left + NOTCH) + "\" cy=\"" + num(cy) + "\" r=\"1.5\" fill=\"var(--_b)\"/>" +
        labelSvg(tag, left + NOTCH + 4.0 + tag.width / 2.0, cy, " fill=\"var(--_fg)\"") +
        "</g>";
    }

    std::string mergeCls = (commit.type == CommitType::MERGE && type != CommitType::MERGE) ? " pele-commit-merge" : "";
    const char* clsName = (type >= 0 && type < 5) ? CLASSES[type] : CLASSES[0];
    commitsOut += "<g class=\"pele-commit pele-commit-" + std::string(clsName) + mergeCls + "\" data-id=\"" + escText(commit.id) + "\">" + body + "</g>";
  }

  if (minX > maxX) grow(0.0, 0.0, 0.0, 0.0);
  Label title = layoutLabel(model.title, false, measurer, size, 4000.0, Style::Bold);
  double titleHeight = title.height > 0.0 ? title.height + 16.0 : 0.0;
  double inner = std::max(maxX - minX, title.width);
  double width = std::ceil(inner + 2.0 * pad);
  double height = std::ceil(maxY - minY + titleHeight + 2.0 * pad);
  double dx = pad - minX + (inner - (maxX - minX)) / 2.0;
  double dy = pad + titleHeight - minY;

  std::string content =
    labelSvg(title, width / 2.0, pad + title.height / 2.0, " class=\"pele-title\" font-weight=\"var(--_tw)\"") +
    "<g transform=\"translate(" + num(dx) + "," + num(dy) + ")\">" +
    (!branches.empty() ? ("<g class=\"pele-branches\" fill=\"none\" font-size=\"" + num(medium) + "\">" + branches + "</g>") : "") +
    ((!curves.empty() || !lines.empty()) ? ("<g class=\"pele-edges\" fill=\"none\" stroke-width=\"2\" stroke-linecap=\"round\">" + curves + lines + "</g>") : "") +
    (!commitsOut.empty() ? ("<g class=\"pele-commits\" font-size=\"" + num(small) + "\" fill=\"var(--_m)\">" + commitsOut + "</g>") : "") +
    "</g>";

  return {
    svgDocument("gitGraph", width, height, size, options, model, content),
    width,
    height,
    {}
  };
}

inline Rendered renderGitModel(const GitModel& model, const Config& config, const RenderOptions& options) {
  if (model.direction != "LR") return drawGit(model, config, options, model.direction);
  return turnToFit(options, [&](bool down) {
    return drawGit(model, config, options, down ? "TB" : "LR");
  });
}

inline Rendered render(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options = {}) {
  GitModel model = buildGit(parseGit(source), config, title);
  return renderGitModel(model, config, options);
}

} // namespace GitDiagram

inline Rendered renderGit(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options = {}) {
  return GitDiagram::render(source, config, title, options);
}

} // namespace Pele
