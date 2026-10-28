#pragma once
#include "../layout.h"
#include "../common_parse.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Pele {

namespace railroad_detail {

inline std::string rrSingle(const std::string& text) {
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

inline std::string rrTitleValue(const std::string& text) {
  size_t pos = text.find("title");
  if (pos == std::string::npos) return "";
  return rrSingle(text.substr(pos + 5));
}

inline std::string rrAccTitleValue(const std::string& text) {
  size_t pos = text.find(':');
  if (pos == std::string::npos) return "";
  return rrSingle(text.substr(pos + 1));
}

inline std::string rrAccDescrValue(const std::string& text) {
  size_t brace = text.find('{');
  size_t colon = text.find(':');
  if (colon != std::string::npos && (brace == std::string::npos || colon < brace)) {
    return rrSingle(text.substr(colon + 1));
  }
  if (brace != std::string::npos) {
    size_t close = text.find('}', brace + 1);
    std::string body = text.substr(brace + 1, close == std::string::npos ? std::string::npos : close - brace - 1);
    return cleanAccDescr(body);
  }
  return "";
}

inline bool isWordDash(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
}

inline bool isAlnumDash(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
}

inline bool isAlpha(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

inline bool isHexDigit(char c) {
  return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

inline TokenType rrLiteral(const std::string& name, std::vector<int> longer = {}) {
  TokenType t;
  t.name = name;
  t.literal = name;
  t.isRegex = false;
  t.longer = std::move(longer);
  return t;
}

inline TokenType rrOpening(const std::string& word, int nameTokIdx) {
  TokenType t = common_tokens::keyword(word);
  t.longer = {nameTokIdx};
  return t;
}

inline TokenType rrIdentifier(const std::string& name) {
  TokenType t;
  t.name = name;
  t.isRegex = true;
  t.first = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_";
  t.match = [](const std::string& src, int at, int) -> int {
    int n = static_cast<int>(src.size());
    if (at >= n) return -1;
    char c = src[at];
    if (!isAlpha(c) && c != '_') return -1;
    int p = at + 1;
    while (p < n && isWordDash(src[p])) p++;
    return p;
  };
  return t;
}

inline TokenType rrQuoted(const std::string& name) {
  TokenType t = common_tokens::makeString();
  t.name = name;
  return t;
}

inline TokenType rrTitle(bool allowUnderscore) {
  TokenType t;
  t.name = "TITLE";
  t.isRegex = true;
  t.first = "t";
  t.match = [allowUnderscore](const std::string& src, int at, int) -> int {
    if (src.compare(at, 5, "title") != 0) return -1;
    int p = at + 5;
    int n = static_cast<int>(src.size());
    if (p < n && (src[p] == '\t' || src[p] == ' ')) {
      return common_tokens::matchRestOfLineBeforeComment(src, p + 1);
    }
    if (p < n && (allowUnderscore ? isWordDash(src[p]) : isAlnumDash(src[p]))) {
      return -1;
    }
    return p;
  };
  return t;
}

inline TokenType rrAccTitle() {
  TokenType t;
  t.name = "ACC_TITLE";
  t.isRegex = true;
  t.first = "a";
  t.match = [](const std::string& src, int at, int) -> int {
    if (src.compare(at, 8, "accTitle") != 0) return -1;
    int p = common_tokens::matchTabsSpaces(src, at + 8);
    if (p < static_cast<int>(src.size()) && src[p] == ':') {
      return common_tokens::matchRestOfLineBeforeComment(src, p + 1);
    }
    return -1;
  };
  return t;
}

inline TokenType rrAccDescr() {
  TokenType t;
  t.name = "ACC_DESCR";
  t.isRegex = true;
  t.first = "a";
  t.scanner = []() -> TokenScanner {
    int unclosed = std::numeric_limits<int>::max();
    return [unclosed](const std::string& src, int at, int) mutable -> int {
      if (src.compare(at, 8, "accDescr") != 0) return -1;
      int n = static_cast<int>(src.size());
      int i = at + 8;
      while (i < n && (src[i] == ' ' || src[i] == '\t')) i++;
      if (i < n && src[i] == ':') {
        for (i++; i < n; i++) {
          char c = src[i];
          if (c == '\n' || c == '\r' || (c == '%' && i + 1 < n && src[i + 1] == '%')) break;
        }
        return i;
      }
      while (i < n && isSpace(charCodeAt(src, i))) i++;
      if (i >= n || src[i] != '{' || i >= unclosed) return -1;
      size_t close = src.find('}', i + 1);
      if (close == std::string::npos) {
        unclosed = i;
        return -1;
      }
      return static_cast<int>(close + 1);
    };
  };
  return t;
}

inline std::vector<TokenType> rrHiddenTokens(const std::string& prefix) {
  TokenType ws;
  ws.name = prefix + "_WHITESPACE";
  ws.isRegex = true;
  ws.hidden = true;
  ws.first = "\t \r\n";
  ws.match = [](const std::string& src, int at, int) -> int {
    int n = static_cast<int>(src.size());
    int p = at;
    while (p < n && (src[p] == '\t' || src[p] == ' ' || src[p] == '\r' || src[p] == '\n')) p++;
    return p > at ? p : -1;
  };

  TokenType yaml = common_tokens::makeYaml();
  yaml.name = prefix + "_YAML";
  yaml.first = "-";

  TokenType dir = common_tokens::makeDirective();
  dir.name = prefix + "_DIRECTIVE";
  dir.first = "\t %";

  TokenType slc = common_tokens::makeSingleLineComment();
  slc.name = prefix + "_SINGLE_LINE_COMMENT";
  slc.first = "\t %";

  return {ws, yaml, dir, slc};
}

inline TokenType rrBlockComment(const std::string& name) {
  TokenType t;
  t.name = name;
  t.isRegex = true;
  t.hidden = true;
  t.first = "/";
  t.opener = [](const std::string& src, int at, int) -> int {
    return src.compare(at, 2, "/*") == 0 ? at + 2 : -1;
  };
  t.match = [](const std::string& src, int at, int) -> int {
    if (src.compare(at, 2, "/*") != 0) return -1;
    size_t close = src.find("*/", at + 2);
    return close == std::string::npos ? -1 : static_cast<int>(close + 2);
  };
  return t;
}

inline std::string rrUnescape(const std::string& text) {
  if (text.size() < 2) return "";
  if (text.find('\\') == std::string::npos) return text.substr(1, text.size() - 2);
  size_t end = text.size() - 1;
  std::string out;
  for (size_t i = 1; i < end; i++) {
    char c = text[i];
    if (c == '\\' && i + 1 < end) {
      c = text[++i];
      out.push_back(c == 'n' ? '\n' : c == 'r' ? '\r' : c == 't' ? '\t' : c);
    } else {
      out.push_back(c);
    }
  }
  return out;
}

struct RailroadNode {
  std::string type; // "terminal", "nonterminal", "special", "sequence", "choice", "optional", "repetition"
  std::string value;
  std::string name;
  std::string text;
  double min = 0;
  double max = std::numeric_limits<double>::infinity();
  std::vector<RailroadNode> children;
};

struct RailroadRule {
  std::string name;
  RailroadNode definition;
};

struct RailroadModel : DiagramMeta {
  std::string notation = "railroad";
  std::vector<RailroadRule> rules;
};

inline RailroadNode makeTerminal(const std::string& value) {
  RailroadNode n;
  n.type = "terminal";
  n.value = value;
  return n;
}

inline RailroadNode makeNonterminal(const std::string& name) {
  RailroadNode n;
  n.type = "nonterminal";
  n.name = name;
  return n;
}

inline RailroadNode makeSpecial(const std::string& text) {
  RailroadNode n;
  n.type = "special";
  n.text = text;
  return n;
}

inline RailroadNode makeOptional(RailroadNode element) {
  RailroadNode n;
  n.type = "optional";
  n.children.push_back(std::move(element));
  return n;
}

inline RailroadNode makeRepetition(RailroadNode element, double min, double max = std::numeric_limits<double>::infinity()) {
  RailroadNode n;
  n.type = "repetition";
  n.min = min;
  n.max = max;
  n.children.push_back(std::move(element));
  return n;
}

inline RailroadNode makeSequence(std::vector<RailroadNode> elements) {
  if (elements.size() == 1) return std::move(elements[0]);
  RailroadNode n;
  n.type = "sequence";
  n.children = std::move(elements);
  return n;
}

inline RailroadNode makeChoice(std::vector<RailroadNode> alternatives) {
  if (alternatives.size() == 1) return std::move(alternatives[0]);
  RailroadNode n;
  n.type = "choice";
  n.children = std::move(alternatives);
  return n;
}

struct ParsedGrammar {
  std::optional<std::string> title;
  std::optional<std::string> accTitle;
  std::optional<std::string> accDescr;
  std::vector<RailroadRule> rules;
};

template <typename UnquoteFn, typename RuleFn>
inline ParsedGrammar parseGrammar(
  const std::string& src,
  const std::vector<TokenType>& types,
  int meta,
  UnquoteFn unquote,
  RuleFn ruleFn
) {
  Tokens toks = tokenize(src, types, "railroad");
  TokenReader r(toks, types, "railroad");
  ParsedGrammar ast;
  r.expect(0);
  for (;;) {
    int k = r.kind();
    if (k == meta) {
      std::string title = rrTitleValue(r.take());
      if (title.size() >= 2 && (title.front() == '"' || title.front() == '\'') && title.back() == title.front()) {
        ast.title = unquote(title);
      } else {
        ast.title = title;
      }
    } else if (k == meta + 1) {
      ast.accTitle = rrAccTitleValue(r.take());
    } else if (k == meta + 2) {
      ast.accDescr = rrAccDescrValue(r.take());
    } else {
      break;
    }
  }
  while (r.kind() != -1) {
    ast.rules.push_back(ruleFn(r));
  }
  return ast;
}

// ----------------------------------------------------------------------------
// 1. IR (`railroad-beta`)
// ----------------------------------------------------------------------------

namespace ir {

enum T {
  keyword = 0,
  nonterminal,
  zeroOrMore,
  oneOrMore,
  sequence,
  optional,
  terminal,
  special,
  choice,
  equals,
  semicolon,
  open,
  comma,
  close,
  title,
  accTitle,
  accDescr,
  id,
  string
};

inline const std::vector<TokenType>& getTokens() {
  static const std::vector<TokenType> tokens = []() {
    std::vector<TokenType> v;
    v.push_back(rrOpening("railroad-beta", T::id));
    v.push_back(rrLiteral("nonterminal", {T::id}));
    v.push_back(rrLiteral("zeroOrMore", {T::id}));
    v.push_back(rrLiteral("oneOrMore", {T::id}));
    v.push_back(rrLiteral("sequence", {T::id}));
    v.push_back(rrLiteral("optional", {T::id}));
    v.push_back(rrLiteral("terminal", {T::id}));
    v.push_back(rrLiteral("special", {T::id}));
    v.push_back(rrLiteral("choice", {T::id}));
    v.push_back(rrLiteral("="));
    v.push_back(rrLiteral(";"));
    v.push_back(rrLiteral("("));
    v.push_back(rrLiteral(","));
    v.push_back(rrLiteral(")"));
    v.push_back(rrTitle(true));
    v.push_back(rrAccTitle());
    v.push_back(rrAccDescr());
    v.push_back(rrIdentifier("RR_ID"));
    v.push_back(rrQuoted("RR_STRING"));
    for (auto& ht : rrHiddenTokens("RR")) v.push_back(std::move(ht));
    v.push_back(rrBlockComment("RR_BLOCK_COMMENT"));
    return v;
  }();
  return tokens;
}

inline std::string argument(TokenReader& r) {
  r.i++;
  r.expect(T::open);
  std::string value = rrUnescape(r.expect(T::string));
  r.expect(T::close);
  return value;
}

struct OpenFrame {
  int kind;
  std::vector<RailroadNode> items;
};

inline RailroadNode expression(TokenReader& r) {
  std::vector<OpenFrame> openStack;
  for (;;) {
    RailroadNode value;
    int k = r.kind();
    switch (k) {
      case T::terminal:
        value = makeTerminal(argument(r));
        break;
      case T::nonterminal:
        value = makeNonterminal(argument(r));
        break;
      case T::special:
        value = makeSpecial(argument(r));
        break;
      case T::sequence:
      case T::choice:
      case T::optional:
      case T::oneOrMore:
      case T::zeroOrMore:
        r.i++;
        r.expect(T::open);
        openStack.push_back({k, {}});
        continue;
      default:
        r.fail("an expression");
    }
    for (;;) {
      if (openStack.empty()) return value;
      OpenFrame& parent = openStack.back();
      RailroadNode nextValue;
      if (parent.kind == T::optional) {
        nextValue = makeOptional(std::move(value));
      } else if (parent.kind == T::oneOrMore) {
        nextValue = makeRepetition(std::move(value), 1);
      } else if (parent.kind == T::zeroOrMore) {
        nextValue = makeRepetition(std::move(value), 0);
      } else {
        parent.items.push_back(std::move(value));
        if (r.accept(T::comma)) break;
        if (parent.kind == T::sequence) {
          nextValue = makeSequence(std::move(parent.items));
        } else {
          nextValue = makeChoice(std::move(parent.items));
        }
      }
      r.expect(T::close);
      openStack.pop_back();
      value = std::move(nextValue);
    }
  }
}

inline ParsedGrammar parse(const std::string& src) {
  return parseGrammar(src, getTokens(), T::title, rrUnescape, [](TokenReader& r) -> RailroadRule {
    std::string name = r.expect(T::id);
    r.expect(T::equals);
    RailroadNode def = expression(r);
    r.expect(T::semicolon);
    return {std::move(name), std::move(def)};
  });
}

} // namespace ir

// ----------------------------------------------------------------------------
// 2. EBNF (`railroad-ebnf-beta`)
// ----------------------------------------------------------------------------

namespace ebnf {

enum T {
  keyword = 0,
  defines,
  equals,
  semicolon,
  bar,
  comma,
  group,
  groupEnd,
  option,
  optionEnd,
  repeat,
  repeatEnd,
  question,
  star,
  plus,
  minus,
  title,
  accTitle,
  accDescr,
  id,
  string,
  special
};

inline int specialSequence(const std::string& src, int at, int) {
  int n = static_cast<int>(src.size());
  if (at >= n || src[at] != '?') return -1;
  bool solid = false;
  for (int i = at + 1; i < n; i++) {
    int c = charCodeAt(src, i);
    if (c == 63) return solid ? i + 1 : -1;
    if (c == 59) return -1;
    if (!isSpace(c)) solid = true;
  }
  return -1;
}

inline const std::vector<TokenType>& getTokens() {
  static const std::vector<TokenType> tokens = []() {
    std::vector<TokenType> v;
    // Indices:
    // 19 = id, 21 = special, 23 = EBNF_YAML, 27 = EBNF_ISO_COMMENT
    v.push_back(rrOpening("railroad-ebnf-beta", T::id)); // 0
    v.push_back(rrLiteral("::="));                       // 1
    v.push_back(rrLiteral("="));                         // 2
    v.push_back(rrLiteral(";"));                         // 3
    v.push_back(rrLiteral("|"));                         // 4
    v.push_back(rrLiteral(","));                         // 5
    v.push_back(rrLiteral("(", {27}));                   // 6
    v.push_back(rrLiteral(")"));                         // 7
    v.push_back(rrLiteral("["));                         // 8
    v.push_back(rrLiteral("]"));                         // 9
    v.push_back(rrLiteral("{"));                         // 10
    v.push_back(rrLiteral("}"));                         // 11
    v.push_back(rrLiteral("?", {T::special}));           // 12
    v.push_back(rrLiteral("*"));                         // 13
    v.push_back(rrLiteral("+"));                         // 14
    v.push_back(rrLiteral("-", {23}));                   // 15
    v.push_back(rrTitle(true));                          // 16
    v.push_back(rrAccTitle());                           // 17
    v.push_back(rrAccDescr());                           // 18
    v.push_back(rrIdentifier("EBNF_ID"));                // 19
    v.push_back(rrQuoted("EBNF_STRING"));                // 20

    TokenType spec;
    spec.name = "EBNF_SPECIAL_SEQUENCE";
    spec.isRegex = true;
    spec.first = "?";
    spec.match = specialSequence;
    v.push_back(std::move(spec));                        // 21

    for (auto& ht : rrHiddenTokens("EBNF")) v.push_back(std::move(ht)); // 22..25
    v.push_back(rrBlockComment("EBNF_BLOCK_COMMENT"));   // 26

    TokenType iso;
    iso.name = "EBNF_ISO_COMMENT";
    iso.isRegex = true;
    iso.hidden = true;
    iso.first = "(";
    iso.opener = [](const std::string& src, int at, int) -> int {
      return src.compare(at, 2, "(*") == 0 ? at + 2 : -1;
    };
    iso.match = [](const std::string& src, int at, int) -> int {
      if (src.compare(at, 2, "(*") != 0) return -1;
      size_t close = src.find("*)", at + 2);
      return close == std::string::npos ? -1 : static_cast<int>(close + 2);
    };
    v.push_back(std::move(iso));                         // 27
    return v;
  }();
  return tokens;
}

struct Frame {
  std::vector<std::vector<RailroadNode>> alternatives;
  int bracketKind = -1;
  int close = -2;
  bool except = false;
};

inline Frame makeFrame(int bracketKind, int close, bool except) {
  Frame f;
  f.alternatives.push_back({});
  f.bracketKind = bracketKind;
  f.close = close;
  f.except = except;
  return f;
}

inline RailroadNode foldChoice(std::vector<std::vector<RailroadNode>>& alternatives) {
  std::vector<RailroadNode> seqs;
  seqs.reserve(alternatives.size());
  for (auto& seq : alternatives) {
    seqs.push_back(makeSequence(std::move(seq)));
  }
  return makeChoice(std::move(seqs));
}

inline RailroadNode definition(TokenReader& r) {
  std::vector<Frame> stack;
  Frame top = makeFrame(-1, -2, false);
  bool except = false;
  for (;;) {
    RailroadNode primary;
    int kind = r.kind();
    switch (kind) {
      case T::string:
        primary = makeTerminal(rrUnescape(r.take()));
        break;
      case T::id:
        primary = makeNonterminal(r.take());
        break;
      case T::special: {
        std::string raw = r.take();
        primary = makeSpecial(trimStr(raw.substr(1, raw.size() - 2)));
        break;
      }
      case T::group:
      case T::option:
      case T::repeat: {
        r.i++;
        stack.push_back(std::move(top));
        top = makeFrame(kind, kind + 1, except);
        except = false;
        continue;
      }
      default:
        r.fail("a terminal, a name, or an opening bracket");
    }
    for (;;) {
      if (except) {
        RailroadNode& prev = top.alternatives.back().back();
        RailroadNode seqNode;
        seqNode.type = "sequence";
        seqNode.children.push_back(std::move(prev));
        seqNode.children.push_back(makeTerminal("-"));
        seqNode.children.push_back(std::move(primary));
        prev = std::move(seqNode);
        except = false;
      } else {
        top.alternatives.back().push_back(std::move(primary));
      }
      RailroadNode& term = top.alternatives.back().back();
      for (;;) {
        int k = r.kind();
        if (k == T::question) {
          r.take();
          term = makeOptional(std::move(term));
        } else if (k == T::star) {
          r.take();
          term = makeRepetition(std::move(term), 0);
        } else if (k == T::plus) {
          r.take();
          term = makeRepetition(std::move(term), 1);
        } else {
          break;
        }
      }
      int k = r.kind();
      if (k == top.close) {
        r.i++;
        RailroadNode inner = foldChoice(top.alternatives);
        if (top.bracketKind == T::group) primary = std::move(inner);
        else if (top.bracketKind == T::option) primary = makeOptional(std::move(inner));
        else primary = makeRepetition(std::move(inner), 0);
        except = top.except;
        top = std::move(stack.back());
        stack.pop_back();
        continue;
      }
      if (k == T::minus) {
        r.i++;
        except = true;
      } else if (k == T::comma) {
        r.i++;
      } else if (k == T::bar) {
        r.i++;
        top.alternatives.push_back({});
      } else if (k != T::string && k != T::id && k != T::special && k != T::group && k != T::option && k != T::repeat) {
        if (stack.empty()) return foldChoice(top.alternatives);
        r.fail("token of type '" + getTokens()[top.close].name + "'");
      }
      break;
    }
  }
}

inline ParsedGrammar parse(const std::string& src) {
  return parseGrammar(src, getTokens(), T::title, rrUnescape, [](TokenReader& r) -> RailroadRule {
    std::string name = r.expect(T::id);
    if (!r.accept(T::equals)) r.expect(T::defines);
    RailroadNode def = definition(r);
    r.expect(T::semicolon);
    return {std::move(name), std::move(def)};
  });
}

} // namespace ebnf

// ----------------------------------------------------------------------------
// 3. ABNF (`railroad-abnf-beta`)
// ----------------------------------------------------------------------------

namespace abnf {

enum T {
  keyword = 0,
  equals,
  semicolon,
  slash,
  group,
  groupEnd,
  option,
  optionEnd,
  title,
  accTitle,
  accDescr,
  name,
  string,
  number,
  repeat,
  exact
};

inline const std::vector<TokenType>& getTokens() {
  static const std::vector<TokenType> tokens = []() {
    std::vector<TokenType> v;
    // Indices: 11 = name, 20 = ABNF_COMMENT
    v.push_back(rrOpening("railroad-abnf-beta", T::name)); // 0
    v.push_back(rrLiteral("="));                           // 1
    v.push_back(rrLiteral(";", {20}));                     // 2
    v.push_back(rrLiteral("/"));                           // 3
    v.push_back(rrLiteral("("));                           // 4
    v.push_back(rrLiteral(")"));                           // 5
    v.push_back(rrLiteral("["));                           // 6
    v.push_back(rrLiteral("]"));                           // 7
    v.push_back(rrTitle(false));                           // 8
    v.push_back(rrAccTitle());                             // 9
    v.push_back(rrAccDescr());                             // 10

    TokenType ruleName;
    ruleName.name = "ABNF_RULENAME";
    ruleName.isRegex = true;
    ruleName.first = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    ruleName.match = [](const std::string& src, int at, int) -> int {
      int n = static_cast<int>(src.size());
      if (at >= n || !isAlpha(src[at])) return -1;
      int p = at + 1;
      while (p < n && isAlnumDash(src[p])) p++;
      return p;
    };
    v.push_back(std::move(ruleName));                      // 11

    TokenType abnfStr;
    abnfStr.name = "ABNF_STRING";
    abnfStr.isRegex = true;
    abnfStr.first = "\"";
    abnfStr.match = [](const std::string& src, int at, int) -> int {
      int n = static_cast<int>(src.size());
      if (at >= n || src[at] != '"') return -1;
      size_t close = src.find('"', at + 1);
      return close == std::string::npos ? -1 : static_cast<int>(close + 1);
    };
    v.push_back(std::move(abnfStr));                       // 12

    TokenType numVal;
    numVal.name = "ABNF_NUMVAL";
    numVal.isRegex = true;
    numVal.first = "%";
    numVal.match = [](const std::string& src, int at, int) -> int {
      int n = static_cast<int>(src.size());
      if (at + 2 >= n || src[at] != '%') return -1;
      char base = src[at + 1];
      if (base != 'x' && base != 'X' && base != 'd' && base != 'D' && base != 'b' && base != 'B') return -1;
      int p = at + 2;
      if (!isHexDigit(src[p])) return -1;
      while (p < n && isHexDigit(src[p])) p++;
      while (p + 1 < n && (src[p] == '-' || src[p] == '.') && isHexDigit(src[p + 1])) {
        p++;
        while (p < n && isHexDigit(src[p])) p++;
      }
      return p;
    };
    v.push_back(std::move(numVal));                        // 13

    TokenType rep;
    rep.name = "ABNF_REPEAT";
    rep.isRegex = true;
    rep.first = "0123456789*";
    rep.match = [](const std::string& src, int at, int) -> int {
      int n = static_cast<int>(src.size());
      int p = at;
      while (p < n && src[p] >= '0' && src[p] <= '9') p++;
      if (p >= n || src[p] != '*') return -1;
      p++;
      while (p < n && src[p] >= '0' && src[p] <= '9') p++;
      return p;
    };
    v.push_back(std::move(rep));                           // 14

    TokenType exactRep;
    exactRep.name = "ABNF_EXACT_REPEAT";
    exactRep.isRegex = true;
    exactRep.first = "0123456789";
    exactRep.match = [](const std::string& src, int at, int) -> int {
      int n = static_cast<int>(src.size());
      int p = at;
      while (p < n && src[p] >= '0' && src[p] <= '9') p++;
      return p > at ? p : -1;
    };
    v.push_back(std::move(exactRep));                      // 15

    for (auto& ht : rrHiddenTokens("ABNF")) v.push_back(std::move(ht)); // 16..19

    TokenType comment;
    comment.name = "ABNF_COMMENT";
    comment.isRegex = true;
    comment.hidden = true;
    comment.first = ";";
    comment.match = [](const std::string& src, int at, int) -> int {
      int n = static_cast<int>(src.size());
      if (at >= n || src[at] != ';') return -1;
      int p = at + 1;
      while (p < n && src[p] != '\n' && src[p] != '\r') p++;
      return p;
    };
    v.push_back(std::move(comment));                       // 20
    return v;
  }();
  return tokens;
}

struct Frame {
  std::vector<std::vector<RailroadNode>> alternatives;
  int bracketKind = -1;
  int close = -2;
  std::optional<std::string> repeat;
};

inline Frame makeFrame(int bracketKind, int close, std::optional<std::string> repeat) {
  Frame f;
  f.alternatives.push_back({});
  f.bracketKind = bracketKind;
  f.close = close;
  f.repeat = std::move(repeat);
  return f;
}

inline RailroadNode applyRepeat(RailroadNode primary, const std::optional<std::string>& repeat) {
  if (!repeat.has_value() || repeat->empty()) return primary;
  const std::string& r = *repeat;
  size_t star = r.find('*');
  bool exact = (star == std::string::npos);
  double min = (star != std::string::npos && star > 0) || exact ? std::stod(r) : 0.0;
  double max = exact ? min : (star < r.size() - 1 ? std::stod(r.substr(star + 1)) : std::numeric_limits<double>::infinity());
  return (min == 0.0 && max == 1.0) ? makeOptional(std::move(primary)) : makeRepetition(std::move(primary), min, max);
}

inline RailroadNode alternation(TokenReader& r) {
  std::vector<Frame> stack;
  Frame top = makeFrame(-1, -2, std::nullopt);
  for (;;) {
    std::optional<std::string> repeat;
    if (r.kind() == T::repeat || r.kind() == T::exact) {
      repeat = r.take();
    }
    RailroadNode primary;
    int kind = r.kind();
    switch (kind) {
      case T::string: {
        std::string s = r.take();
        primary = makeTerminal(s.substr(1, s.size() - 2));
        break;
      }
      case T::number:
        primary = makeTerminal(r.take());
        break;
      case T::name:
        primary = makeNonterminal(r.take());
        break;
      case T::group:
      case T::option: {
        r.i++;
        stack.push_back(std::move(top));
        top = makeFrame(kind, kind + 1, std::move(repeat));
        continue;
      }
      default:
        r.fail("a string, a number, a name, or an opening bracket");
    }
    for (;;) {
      top.alternatives.back().push_back(applyRepeat(std::move(primary), repeat));
      int k = r.kind();
      if (k == top.close) {
        r.i++;
        RailroadNode inner = ebnf::foldChoice(top.alternatives);
        primary = (top.bracketKind == T::group) ? std::move(inner) : makeOptional(std::move(inner));
        repeat = top.repeat;
        top = std::move(stack.back());
        stack.pop_back();
        continue;
      }
      if (k == T::slash) {
        r.i++;
        top.alternatives.push_back({});
      } else if (k != T::string && k != T::number && k != T::name && k != T::group && k != T::option && k != T::repeat && k != T::exact) {
        if (stack.empty()) return ebnf::foldChoice(top.alternatives);
        r.fail("token of type '" + getTokens()[top.close].name + "'");
      }
      break;
    }
  }
}

inline ParsedGrammar parse(const std::string& src) {
  return parseGrammar(
    src,
    getTokens(),
    T::title,
    [](const std::string& text) { return text.substr(1, text.size() - 2); },
    [](TokenReader& r) -> RailroadRule {
      std::string name = r.expect(T::name);
      r.expect(T::equals);
      RailroadNode def = alternation(r);
      r.expect(T::semicolon);
      return {std::move(name), std::move(def)};
    }
  );
}

} // namespace abnf

// ----------------------------------------------------------------------------
// 4. PEG (`railroad-peg-beta`)
// ----------------------------------------------------------------------------

namespace peg {

enum T {
  keyword = 0,
  arrow,
  semicolon,
  slash,
  andTok,
  notTok,
  question,
  star,
  plus,
  group,
  groupEnd,
  dot,
  title,
  accTitle,
  accDescr,
  id,
  string
};

inline const std::vector<TokenType>& getTokens() {
  static const std::vector<TokenType> tokens = []() {
    std::vector<TokenType> v;
    v.push_back(rrOpening("railroad-peg-beta", T::id)); // 0
    v.push_back(rrLiteral("<-"));                       // 1
    v.push_back(rrLiteral(";"));                        // 2
    v.push_back(rrLiteral("/"));                        // 3
    v.push_back(rrLiteral("&"));                        // 4
    v.push_back(rrLiteral("!"));                        // 5
    v.push_back(rrLiteral("?"));                        // 6
    v.push_back(rrLiteral("*"));                        // 7
    v.push_back(rrLiteral("+"));                        // 8
    v.push_back(rrLiteral("("));                        // 9
    v.push_back(rrLiteral(")"));                        // 10
    v.push_back(rrLiteral("."));                        // 11
    v.push_back(rrTitle(true));                         // 12
    v.push_back(rrAccTitle());                          // 13
    v.push_back(rrAccDescr());                          // 14
    v.push_back(rrIdentifier("PEG_ID"));                // 15
    v.push_back(rrQuoted("PEG_STRING"));                // 16
    for (auto& ht : rrHiddenTokens("PEG")) v.push_back(std::move(ht)); // 17..20

    TokenType comment;
    comment.name = "PEG_LINE_COMMENT";
    comment.isRegex = true;
    comment.hidden = true;
    comment.first = "#";
    comment.match = [](const std::string& src, int at, int) -> int {
      int n = static_cast<int>(src.size());
      if (at >= n || src[at] != '#') return -1;
      int p = at + 1;
      while (p < n && src[p] != '\n' && src[p] != '\r') p++;
      return p;
    };
    v.push_back(std::move(comment));                    // 21
    return v;
  }();
  return tokens;
}

struct Frame {
  std::vector<std::vector<RailroadNode>> alternatives;
  std::optional<std::string> op;
};

inline Frame makeFrame(std::optional<std::string> op) {
  Frame f;
  f.alternatives.push_back({});
  f.op = std::move(op);
  return f;
}

inline std::string pegLabel(const RailroadNode& node) {
  if (node.type == "terminal") return "\"" + node.value + "\"";
  if (node.type == "nonterminal") return node.name;
  if (node.type == "special") return node.text;
  return "(...)";
}

inline RailroadNode definition(TokenReader& r) {
  std::vector<Frame> stack;
  Frame top = makeFrame(std::nullopt);
  for (;;) {
    std::optional<std::string> op;
    if (r.kind() == T::andTok || r.kind() == T::notTok) {
      op = r.take();
    }
    RailroadNode primary;
    int kind = r.kind();
    switch (kind) {
      case T::string:
        primary = makeTerminal(rrUnescape(r.take()));
        break;
      case T::id:
        primary = makeNonterminal(r.take());
        break;
      case T::dot:
        primary = makeSpecial(r.take());
        break;
      case T::group: {
        r.i++;
        stack.push_back(std::move(top));
        top = makeFrame(std::move(op));
        continue;
      }
      default:
        r.fail("a literal, a name, a dot, or an opening parenthesis");
    }
    for (;;) {
      RailroadNode suffixNode = std::move(primary);
      if (r.kind() == T::question || r.kind() == T::star || r.kind() == T::plus) {
        std::string sop = r.take();
        suffixNode = (sop == "?") ? makeOptional(std::move(suffixNode)) : makeRepetition(std::move(suffixNode), sop == "+" ? 1 : 0);
      }
      RailroadNode prefixNode = op.has_value() ? makeSpecial(*op + pegLabel(suffixNode)) : std::move(suffixNode);
      top.alternatives.back().push_back(std::move(prefixNode));
      int k = r.kind();
      if (k == T::groupEnd && !stack.empty()) {
        r.i++;
        primary = ebnf::foldChoice(top.alternatives);
        op = top.op;
        top = std::move(stack.back());
        stack.pop_back();
        continue;
      }
      if (k == T::slash) {
        r.i++;
        top.alternatives.push_back({});
      } else if (k != T::andTok && k != T::notTok && k != T::string && k != T::id && k != T::dot && k != T::group) {
        if (stack.empty()) return ebnf::foldChoice(top.alternatives);
        r.fail("token of type ')'");
      }
      break;
    }
  }
}

inline ParsedGrammar parse(const std::string& src) {
  return parseGrammar(src, getTokens(), T::title, rrUnescape, [](TokenReader& r) -> RailroadRule {
    std::string name = r.expect(T::id);
    r.expect(T::arrow);
    RailroadNode def = definition(r);
    r.expect(T::semicolon);
    return {std::move(name), std::move(def)};
  });
}

} // namespace peg

// ----------------------------------------------------------------------------
// DB & Notation Detection
// ----------------------------------------------------------------------------

inline std::optional<std::string> notationOf(const std::string& source) {
  size_t p = 0;
  while (p < source.size() && isSpace(charCodeAt(source, static_cast<int>(p)))) p++;
  std::string rem = toLowerAscii(source.substr(p, 24));
  if (startsWith(rem, "railroad-ebnf-beta")) return "ebnf";
  if (startsWith(rem, "railroad-abnf-beta")) return "abnf";
  if (startsWith(rem, "railroad-peg-beta")) return "peg";
  if (startsWith(rem, "railroad-beta")) return "railroad";
  return std::nullopt;
}

inline RailroadModel parseRailroad(const std::string& source, const std::string& title) {
  auto notation = notationOf(source);
  if (!notation.has_value()) {
    throw PeleError("No railroad diagram keyword found.", "syntax", "railroad");
  }
  RailroadModel db;
  if (!title.empty()) db.title = title;
  db.notation = *notation;
  ParsedGrammar ast;
  if (*notation == "ebnf") ast = ebnf::parse(source);
  else if (*notation == "abnf") ast = abnf::parse(source);
  else if (*notation == "peg") ast = peg::parse(source);
  else ast = ir::parse(source);

  if (ast.accDescr.has_value() && !ast.accDescr->empty()) {
    db.accDescr = cleanAccDescr(*ast.accDescr);
  }
  if (ast.accTitle.has_value() && !ast.accTitle->empty()) {
    db.accTitle = trimStartStr(*ast.accTitle);
  }
  if (ast.title.has_value() && !ast.title->empty()) {
    db.title = *ast.title;
  }
  db.rules = std::move(ast.rules);
  return db;
}

// ----------------------------------------------------------------------------
// Renderer (`src/diagrams/railroad/render.ts`)
// ----------------------------------------------------------------------------

enum class Kind {
  Terminal,
  NonTerminal,
  Special,
  Sequence,
  Choice,
  Bypass,
  Loop,
  Folded
};

struct Box {
  Kind kind = Kind::Terminal;
  double width = 0;
  double up = 0;
  double down = 0;
  std::vector<Box> kids;
  std::string text;
  std::string id;
  double reach = 0;
};

struct Metrics {
  const TextMeasurer* measurer = nullptr;
  double size = 14;
  double padding = 12;
  double height = 30;
  double gap = 16;
  double sep = 10;
  double arc = 10;
};

constexpr double LEAD = 12;
constexpr double HEAD_GAP = 6;
constexpr double RULE_GAP = 18;
constexpr double COUNT_SIZE = 12;
constexpr double ARROW = 7;

inline double option(const Config& config, const std::string& key, double fallback, double maxVal) {
  const auto* rr = config.get("railroad");
  if (!rr || !rr->isMap()) return fallback;
  const auto* v = rr->get(key);
  if (v && v->isNumber() && v->nVal >= 0.0) {
    return std::min(v->nVal, maxVal);
  }
  return fallback;
}

inline std::string shown(const std::string& text) {
  std::string s = text.find(PLACEHOLDER_CLOSE) != std::string::npos
    ? decodeEntities(replaceAllStr(text, "&", "&amp;"))
    : text;
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    if (c == '\n') out += "\\n";
    else if (c == '\r') out += "\\r";
    else if (c == '\t') out += "\\t";
    else out.push_back(c);
  }
  return out;
}

inline double even(double value) {
  return std::ceil(value / 2.0) * 2.0;
}

inline Box leaf(Kind kind, const std::string& text, const std::string& id, int style, const Metrics& m) {
  double half = m.height / 2.0;
  double width = std::max(even(m.measurer->width(text, m.size, style) + 2.0 * m.padding), m.height);
  return {kind, width, half, half, {}, text, id, 0};
}

inline std::string countStr(double min, double max) {
  if (!std::isfinite(max) && min <= 1.0) return "";
  auto fmt = [](double value) -> std::string {
    return std::isfinite(value) ? num(value) : "\xE2\x88\x9E"; // ∞
  };
  if (min == max) return fmt(min) + "\xC3\x97"; // ×
  if (!std::isfinite(max)) return "\xE2\x89\xA5" + fmt(min) + "\xC3\x97"; // ≥...×
  return min == 0.0
    ? "\xE2\x89\xA4" + fmt(max) + "\xC3\x97" // ≤...×
    : fmt(min) + "\xE2\x80\x93" + fmt(max) + "\xC3\x97"; // –...×
}

inline double below(double offset, const Box& above, const Box& branch, const Metrics& m) {
  return std::max(offset + above.down + m.sep + branch.up, 2.0 * m.arc);
}

inline Box bypass(Box kid, const Metrics& m) {
  double reach = std::max(kid.up + m.sep, 2.0 * m.arc);
  double width = kid.width + 4.0 * m.arc;
  double down = kid.down;
  return {Kind::Bypass, width, reach, down, {std::move(kid)}, "", "", reach};
}

inline bool isAllSpaces(const std::string& s) {
  if (s.empty()) return false;
  for (char c : s) {
    if (c != ' ') return false;
  }
  return true;
}

inline Box measure(const RailroadNode& root, const Metrics& m, double room = std::numeric_limits<double>::infinity()) {
  std::unordered_map<const RailroadNode*, double> budgets;
  if (std::isfinite(room)) {
    std::vector<std::pair<const RailroadNode*, double>> pending = {{&root, room}};
    for (size_t i = 0; i < pending.size(); i++) {
      const RailroadNode* node = pending[i].first;
      double width = pending[i].second;
      budgets[node] = width;
      double around = (node->type == "choice" || node->type == "optional") ? 4.0 * m.arc
                    : (node->type == "repetition") ? (node->min == 0.0 ? 6.0 : 2.0) * m.arc
                    : (node->type == "sequence") ? 6.0 * m.arc
                    : 0.0;
      for (const auto& child : node->children) {
        pending.push_back({&child, std::max(40.0, width - around)});
      }
    }
  }

  // Bottom-up fold over RailroadNode tree
  std::vector<const RailroadNode*> nodes = {&root};
  std::vector<std::vector<Box>> done = {{}};
  for (;;) {
    size_t top = nodes.size() - 1;
    const RailroadNode* node = nodes[top];
    auto& results = done[top];
    if (results.size() < node->children.size()) {
      const RailroadNode* child = &node->children[results.size()];
      nodes.push_back(child);
      done.push_back({});
    } else {
      auto it = budgets.find(node);
      double nodeRoom = (it != budgets.end()) ? it->second : std::numeric_limits<double>::infinity();
      Box value;
      if (node->type == "terminal") {
        std::string text = shown(node->value);
        if (isAllSpaces(text)) {
          std::string vis;
          for (size_t k = 0; k < text.size(); k++) vis += "\xE2\x90\xA3"; // ␣
          text = std::move(vis);
        }
        value = leaf(Kind::Terminal, text, "", Style::Mono, m);
      } else if (node->type == "nonterminal") {
        value = leaf(Kind::NonTerminal, shown(node->name), node->name, 0, m);
      } else if (node->type == "special") {
        value = leaf(Kind::Special, "? " + shown(node->text) + " ?", "", Style::Italic, m);
      } else if (node->type == "sequence") {
        double width = std::max(0, static_cast<int>(results.size()) - 1) * m.gap;
        double up = 0;
        double down = 0;
        for (const auto& kid : results) {
          width += kid.width;
          up = std::max(up, kid.up);
          down = std::max(down, kid.down);
        }
        if (width <= nodeRoom || results.size() < 2) {
          value = {Kind::Sequence, width, up, down, std::move(results), "", "", 0};
        } else {
          std::vector<Box> rows;
          std::vector<Box> row;
          double rowW = 0;
          auto flush = [&]() {
            double rUp = 0, rDown = 0;
            for (const auto& kid : row) {
              rUp = std::max(rUp, kid.up);
              rDown = std::max(rDown, kid.down);
            }
            rows.push_back({Kind::Sequence, rowW, rUp, rDown, std::move(row), "", "", 0});
            row.clear();
            rowW = 0;
          };
          for (auto& kid : results) {
            if (!row.empty() && rowW + m.gap + kid.width > std::max(40.0, nodeRoom - 6.0 * m.arc)) flush();
            rowW += (!row.empty() ? m.gap : 0.0) + kid.width;
            row.push_back(std::move(kid));
          }
          flush();
          if (rows.size() < 2) {
            // Re-collect kids from rows[0]
            value = {Kind::Sequence, width, up, down, std::move(rows[0].kids), "", "", 0};
          } else {
            double foldedWidth = 0, offset = 0;
            for (size_t i = 0; i < rows.size(); i++) {
              foldedWidth = std::max(foldedWidth, rows[i].width);
              if (i > 0) offset += rows[i - 1].down + std::max(m.sep, 2.0 * m.arc) + rows[i].up;
            }
            double fUp = rows[0].up;
            double fDown = offset + rows.back().down;
            value = {Kind::Folded, foldedWidth + 6.0 * m.arc, fUp, fDown, std::move(rows), "", "", 0};
          }
        }
      } else if (node->type == "choice") {
        double inner = 0;
        double offset = 0;
        for (size_t i = 0; i < results.size(); i++) {
          inner = std::max(inner, results[i].width);
          if (i > 0) offset = below(offset, results[i - 1], results[i], m);
        }
        double cUp = !results.empty() ? results[0].up : 0.0;
        double cDown = !results.empty() ? offset + results.back().down : 0.0;
        value = {Kind::Choice, inner + 4.0 * m.arc, cUp, cDown, std::move(results), "", "", 0};
      } else if (node->type == "optional") {
        value = bypass(std::move(results[0]), m);
      } else if (node->type == "repetition") {
        double min = node->min;
        double max = node->max;
        Box kid = std::move(results[0]);
        if (min == 1.0 && max == 1.0) {
          value = std::move(kid);
        } else {
          std::string text = countStr(min, max);
          double reach = std::max(kid.down + m.sep, 2.0 * m.arc);
          double label = text.empty() ? 0.0 : even(m.measurer->width(text, COUNT_SIZE, 0));
          double lWidth = std::max(kid.width, label) + 2.0 * m.arc;
          double lUp = kid.up;
          double lDown = reach + (text.empty() ? ARROW / 2.0 + 1.0 : COUNT_SIZE + 6.0);
          Box loop = {Kind::Loop, lWidth, lUp, lDown, {std::move(kid)}, text, "", reach};
          value = (min == 0.0) ? bypass(std::move(loop), m) : std::move(loop);
        }
      }
      if (top == 0) return value;
      nodes.pop_back();
      done.pop_back();
      done[top - 1].push_back(std::move(value));
    }
  }
}

template <typename T>
inline void reverseFrom(std::vector<T>& list, size_t from) {
  for (size_t i = from, j = list.size(); i < j && j > 0; i++) {
    j--;
    if (i >= j) break;
    std::swap(list[i], list[j]);
  }
}

inline Rendered draw(const RailroadModel& model, const Config& config, const RenderOptions& options, bool tight) {
  double base = options.fontSize;
  double fontSize = option(config, "fontSize", 0.0, 200.0);
  double size = fontSize > 0.0 ? fontSize : std::round(base * 0.875);
  double headSize = fontSize > 0.0 ? fontSize : base;
  double padding = option(config, "padding", -1.0, 200.0);
  double lineHeight = std::round(size * 1.5);
  const TextMeasurer& measurer = getMeasurer(options);
  Metrics m;
  m.measurer = &measurer;
  m.size = size;
  m.padding = padding < 0.0 ? 12.0 : padding;
  m.height = even(lineHeight + (padding < 0.0 ? 9.0 : 2.0 * padding));
  m.gap = std::min(option(config, "horizontalSeparation", 16.0, 500.0), tight ? 12.0 : std::numeric_limits<double>::infinity());
  m.sep = option(config, "verticalSeparation", 10.0, 500.0);
  m.arc = std::min(option(config, "arcRadius", 10.0, 200.0), tight ? 6.0 : std::numeric_limits<double>::infinity());

  const auto* rrCfg = config.get("railroad");
  bool showMarkers = true;
  if (rrCfg && rrCfg->isMap()) {
    const auto* sm = rrCfg->get("showMarkers");
    if (sm && sm->isBool() && !sm->bVal) showMarkers = false;
  }
  double radius = !showMarkers ? 0.0 : option(config, "markerRadius", 4.0, 50.0);
  double strokeWidth = option(config, "strokeWidth", 0.0, 20.0);
  std::string stroke = strokeWidth > 0.0 ? " stroke-width=\"" + num(strokeWidth) + "\"" : "";
  double pad = options.padding;
  double arc = m.arc;

  std::string A = num(arc);
  auto turn = [&](int sweep, const std::string& dx, const std::string& dy) -> std::string {
    return "a" + A + "," + A + " 0 0 " + std::to_string(sweep) + " " + dx + A + "," + dy + A;
  };
  std::string eastToSouth = turn(1, "", "");
  std::string southToEast = turn(0, "", "");
  std::string eastToNorth = turn(0, "", "-");
  std::string northToEast = turn(1, "", "-");
  std::string loopDown = turn(0, "-", "");
  std::string loopUp = turn(0, "-", "-");

  Label title = layoutLabel(model.title, false, measurer, base, tight ? titleRoom(options) : 4000.0, Style::Bold);
  double titleHeight = title.height > 0.0 ? title.height + 16.0 : 0.0;
  double headHeight = std::round(headSize * 1.5);
  double textShift = size * 0.35;
  double boxTop = m.height / 2.0;

  std::string track;
  std::string arrows;
  std::string nodes;

  auto line = [&](double x1, double y, double x2) {
    if (x2 > x1) track += "M" + num(x1) + "," + num(y) + "H" + num(x2);
  };

  auto place = [&](const Box& root, double left, double baseline) {
    std::vector<const Box*> boxes = {&root};
    std::vector<double> at = {left, baseline};
    while (!boxes.empty()) {
      const Box* box = boxes.back();
      boxes.pop_back();
      double y = at.back();
      at.pop_back();
      double x = at.back();
      at.pop_back();
      const auto& kids = box->kids;
      double right = x + box->width;
      switch (box->kind) {
        case Kind::Sequence: {
          double kx = right;
          for (int i = static_cast<int>(kids.size()) - 1; i >= 0; i--) {
            kx -= kids[i].width;
            boxes.push_back(&kids[i]);
            at.push_back(kx);
            at.push_back(y);
            if (i > 0) line(kx - m.gap, y, kx);
            kx -= m.gap;
          }
          break;
        }
        case Kind::Folded: {
          double start = x + 2.0 * arc;
          size_t from = boxes.size();
          double offset = 0.0;
          line(x, y, start);
          for (size_t i = 0; i < kids.size(); i++) {
            const Box& kid = kids[i];
            double ky = y + offset;
            boxes.push_back(&kid);
            at.push_back(ky);
            at.push_back(start);
            if (i + 1 < kids.size()) {
              const Box& next = kids[i + 1];
              double nextOffset = offset + kid.down + std::max(m.sep, 2.0 * arc) + next.up;
              double mid = y + offset + kid.down + std::max(m.sep, 2.0 * arc) / 2.0;
              track += "M" + num(start + kid.width) + "," + num(ky) + "H" + num(right - 2.0 * arc)
                     + "V" + num(mid) + "H" + num(x + arc) + "V" + num(y + nextOffset) + "H" + num(start);
              arrows += "M" + num(start - ARROW) + "," + num(y + nextOffset - ARROW / 2.0)
                      + "l" + num(ARROW) + "," + num(ARROW / 2.0) + "l" + num(-ARROW) + "," + num(ARROW / 2.0) + "z";
              offset = nextOffset;
            } else {
              track += "M" + num(start + kid.width) + "," + num(ky) + "H" + num(right - arc) + "V" + num(y) + "H" + num(right);
            }
          }
          reverseFrom(boxes, from);
          reverseFrom(at, 2 * from);
          break;
        }
        case Kind::Choice: {
          double inner = box->width - 4.0 * arc;
          double railIn = x + arc;
          double railOut = right - arc;
          size_t from = boxes.size();
          double offset = 0.0;
          for (size_t i = 0; i < kids.size(); i++) {
            const Box& kid = kids[i];
            if (i > 0) offset = below(offset, kids[i - 1], kid, m);
            double ky = y + offset;
            double kx = x + 2.0 * arc + (inner - kid.width) / 2.0;
            bool last = (i == kids.size() - 1);
            if (i == 0) {
              line(x, y, kx);
              line(kx + kid.width, y, right);
            } else {
              track +=
                (last ? "M" + num(x) + "," + num(y) + eastToSouth + "V" + num(ky - arc) : "M" + num(railIn) + "," + num(ky - arc)) +
                southToEast +
                (kx > railIn + arc ? "H" + num(kx) : "") +
                "M" + num(kx + kid.width) + "," + num(ky) +
                (kx > railIn + arc ? "H" + num(railOut - arc) : "") +
                eastToNorth +
                (last ? "V" + num(y + arc) + northToEast : "");
            }
            boxes.push_back(&kid);
            at.push_back(ky);
            at.push_back(kx);
          }
          reverseFrom(boxes, from);
          reverseFrom(at, 2 * from);
          break;
        }
        case Kind::Bypass: {
          const Box& kid = kids[0];
          double kx = x + 2.0 * arc;
          line(x, y, kx);
          line(kx + kid.width, y, right);
          track +=
            "M" + num(x) + "," + num(y) + eastToNorth + "V" + num(y - box->reach + arc) + northToEast + "H" + num(right - 2.0 * arc) +
            eastToSouth + "V" + num(y - arc) + southToEast;
          boxes.push_back(&kid);
          at.push_back(kx);
          at.push_back(y);
          break;
        }
        case Kind::Loop: {
          const Box& kid = kids[0];
          double kx = x + (box->width - kid.width) / 2.0;
          double loopY = y + box->reach;
          double middle = x + box->width / 2.0;
          line(x, y, kx);
          line(kx + kid.width, y, right);
          track +=
            "M" + num(x + arc) + "," + num(y) + loopDown + "V" + num(loopY - arc) + southToEast + "H" + num(right - arc) +
            eastToNorth + "V" + num(y + arc) + loopUp;
          arrows += "M" + num(middle - ARROW / 2.0) + "," + num(loopY) + "l" + num(ARROW) + "," + num(-ARROW / 2.0) + "v" + num(ARROW) + "z";
          if (!box->text.empty()) {
            nodes += "<text class=\"pele-edge-label pele-count\" x=\"" + num(middle) + "\" y=\"" + num(loopY + COUNT_SIZE + 3.0) + "\" font-size=\"" + num(COUNT_SIZE) + "\" fill=\"var(--_m)\">" + esc(box->text) + "</text>";
          }
          boxes.push_back(&kid);
          at.push_back(kx);
          at.push_back(y);
          break;
        }
        default: {
          const std::string& text = box->text;
          bool hasSpace = (!text.empty() && (text.front() == ' ' || text.back() == ' ')) || text.find("  ") != std::string::npos;
          std::string space = hasSpace ? " xml:space=\"preserve\"" : "";
          std::string shape = "<rect x=\"" + num(x) + "\" y=\"" + num(y - boxTop) + "\" width=\"" + num(box->width) + "\" height=\"" + num(m.height) + "\"";
          std::string label = "<text x=\"" + num(x + box->width / 2.0) + "\" y=\"" + num(y + textShift) + "\"" + space;
          if (box->kind == Kind::Terminal) {
            nodes += "<g class=\"pele-node pele-terminal\">" + shape + " rx=\"" + num(boxTop) + "\" fill=\"var(--_s)\" stroke=\"var(--_b)\"" + stroke + "/>" + label + " font-family=\"" + std::string(FONT_MONO) + "\">" + esc(text) + "</text></g>";
          } else if (box->kind == Kind::NonTerminal) {
            nodes += "<g class=\"pele-node pele-nonterminal\" data-id=\"" + escText(box->id) + "\">" + shape + " fill=\"var(--_bg)\" stroke=\"var(--_b)\"" + stroke + std::string(ROUNDED) + "/>" + label + ">" + esc(text) + "</text></g>";
          } else {
            std::string body = text.size() >= 4 ? text.substr(2, text.size() - 4) : "";
            nodes +=
              "<g class=\"pele-node pele-special\">" + shape + " fill=\"var(--_bg)\" stroke=\"var(--_b)\" stroke-dasharray=\"4 3\"" + stroke + std::string(ROUNDED) + "/>" +
              label + " font-style=\"italic\"><tspan fill=\"var(--_m)\">? </tspan>" + esc(body) + "<tspan fill=\"var(--_m)\"> ?</tspan></text></g>";
          }
        }
      }
    }
  };

  std::string rules;
  double width = title.width;
  double top = pad + titleHeight;
  double bottom = pad + title.height;
  for (const auto& rule : model.rules) {
    double maxW = options.maxWidth > 0.0 ? options.maxWidth : 320.0;
    double room = tight ? std::max(80.0, maxW - 2.0 * pad - 4.0 * radius - 2.0 * LEAD - 1.0) : std::numeric_limits<double>::infinity();
    Box box = measure(rule.definition, m, room);
    std::string name = shown(rule.name);
    double baseline = top + headHeight + HEAD_GAP + box.up;
    double start = pad + 2.0 * radius;
    double left = start + LEAD;
    double end = left + box.width + LEAD;
    track.clear();
    arrows.clear();
    nodes.clear();
    line(start, baseline, left);
    line(left + box.width, baseline, end);
    place(box, left, baseline);
    std::string markers = radius > 0.0
      ? "<circle class=\"pele-marker pele-start\" cx=\"" + num(pad + radius) + "\" cy=\"" + num(baseline) + "\" r=\"" + num(radius) + "\" fill=\"var(--_l)\"/>"
        "<circle class=\"pele-marker pele-end\" cx=\"" + num(end + radius) + "\" cy=\"" + num(baseline) + "\" r=\"" + num(radius) + "\" fill=\"var(--_l)\"/>"
      : "";
    rules +=
      "<g class=\"pele-rule\" data-id=\"" + escText(rule.name) + "\">"
      "<text class=\"pele-rule-name\" x=\"" + num(pad) + "\" y=\"" + num(top + headHeight / 2.0 + headSize * 0.35) + "\" font-size=\"" + num(headSize) + "\" font-weight=\"var(--_hw)\" text-anchor=\"start\">" + esc(name) + "</text>"
      "<path class=\"pele-edge pele-track\" d=\"" + track + "\" fill=\"none\" stroke=\"var(--_l)\"" + stroke + "/>" +
      (!arrows.empty() ? "<path class=\"pele-marker\" d=\"" + arrows + "\" fill=\"var(--_l)\"/>" : "") +
      markers +
      nodes +
      "</g>";
    width = std::max(width, std::max(end + 2.0 * radius - pad, measurer.width(name, headSize, Style::Bold)));
    bottom = baseline + box.down;
    top = bottom + m.sep + RULE_GAP;
  }

  double totalWidth = std::ceil(width + 2.0 * pad);
  double totalHeight = std::ceil(bottom + pad);
  std::string content =
    labelSvg(title, totalWidth / 2.0, pad + title.height / 2.0, " class=\"pele-title\" font-weight=\"var(--_tw)\"") +
    (!rules.empty() ? "<g class=\"pele-rules\" transform=\"translate(0.5,0.5)\" font-size=\"" + num(size) + "\" text-anchor=\"middle\">" + rules + "</g>" : "");

  return {
    svgDocument("railroad", totalWidth, totalHeight, base, options, model, content),
    totalWidth,
    totalHeight,
    {}
  };
}

inline Rendered renderRailroadModel(const RailroadModel& model, const Config& config, const RenderOptions& options) {
  return tighten(options, [&](bool tight) {
    return draw(model, config, options, tight);
  });
}

} // namespace railroad_detail

class RailroadDiagram {
public:
  static Rendered render(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options) {
    auto model = railroad_detail::parseRailroad(source, title);
    return railroad_detail::renderRailroadModel(model, config, options);
  }
};

inline Rendered renderRailroad(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options) {
  return RailroadDiagram::render(source, config, title, options);
}

inline Rendered renderRailroad(const std::string& raw, RenderOptions options = {}) {
  auto pre = preprocess(raw);
  std::string parsed = encodeEntities(pre.text) + "\n";
  const auto* section = pre.config.get("railroad");
  bool fixed = (section && section->isMap() && section->get("useMaxWidth") && section->get("useMaxWidth")->isBool() && !section->get("useMaxWidth")->bVal);
  if (!options.hasResponsive) {
    options.responsive = !fixed;
    options.hasResponsive = true;
  }
  return RailroadDiagram::render(parsed, pre.config, pre.title, options);
}

} // namespace Pele
