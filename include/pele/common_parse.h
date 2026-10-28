#pragma once
#include "util.h"

namespace Pele {

// ============================================================================
// Port of src/diagrams/common/tokens.ts
// ============================================================================

using TokenScanner = std::function<int(const std::string& src, int at, int previous)>;

struct TokenType {
  std::string name;
  // Either literal string (`isRegex == false`) or custom matcher/regex
  std::string literal;
  bool isRegex = false;
  TokenScanner match;
  std::function<TokenScanner()> scanner;
  TokenScanner opener;
  bool hidden = false;
  std::optional<std::string> first;
  std::vector<int> longer;
};

struct Tokens {
  std::vector<int> kinds;
  std::vector<int> starts;
  std::vector<int> ends;
  std::string src;
};

inline Tokens tokenize(const std::string& src, const std::vector<TokenType>& types, const std::string& diagram) {
  std::vector<int> kinds;
  std::vector<int> starts;
  std::vector<int> ends;
  int tCount = static_cast<int>(types.size());
  std::vector<uint8_t> dead(tCount, 0);
  std::vector<int> every(tCount);
  for (int k = 0; k < tCount; k++) every[k] = k;

  std::vector<std::vector<int>> starting(128);
  for (int c = 0; c < 128; c++) {
    char ch = static_cast<char>(c);
    for (int k : every) {
      const auto& t = types[k];
      if (!t.isRegex && !t.match && !t.scanner) {
        if (!t.literal.empty() && t.literal[0] == ch) starting[c].push_back(k);
      } else {
        if (!t.first.has_value() || t.first->find(ch) != std::string::npos) starting[c].push_back(k);
      }
    }
  }

  std::vector<TokenScanner> scanners(tCount);
  int n = static_cast<int>(src.size());

  auto matchAt = [&](int k, int p) -> int {
    if (dead[k]) return -1;
    const auto& type = types[k];
    int prev = kinds.empty() ? -1 : kinds.back();
    if (type.match) return type.match(src, p, prev);
    if (type.scanner) {
      if (!scanners[k]) scanners[k] = type.scanner();
      return scanners[k](src, p, prev);
    }
    if (!type.isRegex) {
      const auto& pat = type.literal;
      if (p + static_cast<int>(pat.size()) <= n && src.compare(p, pat.size(), pat) == 0) {
        return p + static_cast<int>(pat.size());
      }
      return -1;
    }
    return -1;
  };

  int p = 0;
  while (p < n) {
    unsigned char c = static_cast<unsigned char>(src[p]);
    const auto& list = c < 128 ? starting[c] : every;
    bool matched = false;
    for (int k : list) {
      int end = matchAt(k, p);
      if (end <= p) {
        if (types[k].opener && types[k].opener(src, p, -1) > p) dead[k] = 1;
        continue;
      }
      int kind = k;
      for (int alt : types[k].longer) {
        int altEnd = matchAt(alt, p);
        if (altEnd > end) {
          kind = alt;
          end = altEnd;
          break;
        }
      }
      if (!types[kind].hidden) {
        kinds.push_back(kind);
        starts.push_back(p);
        ends.push_back(end);
      }
      p = end;
      matched = true;
      break;
    }
    if (!matched) {
      throw syntaxError(diagram, src, p, "", true);
    }
  }
  kinds.push_back(-1);
  starts.push_back(n);
  ends.push_back(n);
  return {kinds, starts, ends, src};
}

namespace common_tokens {

inline int matchTabsSpaces(const std::string& s, int p) {
  int n = static_cast<int>(s.size());
  while (p < n && (s[p] == '\t' || s[p] == ' ')) p++;
  return p;
}

inline int matchRestOfLineBeforeComment(const std::string& s, int p) {
  int n = static_cast<int>(s.size());
  while (p < n && s[p] != '\n' && s[p] != '\r') {
    if (p + 1 < n && s[p] == '%' && s[p + 1] == '%') break;
    p++;
  }
  return p;
}

inline TokenType makeAccDescr() {
  TokenType t;
  t.name = "ACC_DESCR";
  t.isRegex = true;
  t.first = "\t a";
  t.match = [](const std::string& src, int at, int) -> int {
    int p = matchTabsSpaces(src, at);
    if (src.compare(p, 8, "accDescr") != 0) return -1;
    p += 8;
    int q = matchTabsSpaces(src, p);
    if (q < static_cast<int>(src.size()) && src[q] == ':') {
      return matchRestOfLineBeforeComment(src, q + 1);
    }
    q = p;
    while (q < static_cast<int>(src.size()) && isSpace(charCodeAt(src, q))) q++;
    if (q < static_cast<int>(src.size()) && src[q] == '{') {
      size_t close = src.find('}', q + 1);
      if (close != std::string::npos) return static_cast<int>(close + 1);
    }
    return -1;
  };
  return t;
}

inline TokenType makeAccTitle() {
  TokenType t;
  t.name = "ACC_TITLE";
  t.isRegex = true;
  t.first = "\t a";
  t.match = [](const std::string& src, int at, int) -> int {
    int p = matchTabsSpaces(src, at);
    if (src.compare(p, 8, "accTitle") != 0) return -1;
    p = matchTabsSpaces(src, p + 8);
    if (p < static_cast<int>(src.size()) && src[p] == ':') {
      return matchRestOfLineBeforeComment(src, p + 1);
    }
    return -1;
  };
  return t;
}

inline TokenType makeTitle() {
  TokenType t;
  t.name = "TITLE";
  t.isRegex = true;
  t.first = "\t t";
  t.match = [](const std::string& src, int at, int) -> int {
    int p = matchTabsSpaces(src, at);
    if (src.compare(p, 5, "title") != 0) return -1;
    p += 5;
    if (p < static_cast<int>(src.size()) && (src[p] == '\t' || src[p] == ' ')) {
      return matchRestOfLineBeforeComment(src, p + 1);
    }
    return p;
  };
  return t;
}

inline TokenType makeString() {
  TokenType t;
  t.name = "STRING";
  t.isRegex = true;
  t.first = "\"'";
  t.match = [](const std::string& src, int at, int) -> int {
    if (at >= static_cast<int>(src.size())) return -1;
    char q = src[at];
    if (q != '"' && q != '\'') return -1;
    int p = at + 1;
    int n = static_cast<int>(src.size());
    while (p < n) {
      if (src[p] == '\\') {
        p += 2;
        if (p > n) return -1;
      } else if (src[p] == q) {
        return p + 1;
      } else {
        p++;
      }
    }
    return -1;
  };
  return t;
}

inline TokenType makeNewline() {
  TokenType t;
  t.name = "NEWLINE";
  t.isRegex = true;
  t.first = "\r\n";
  t.match = [](const std::string& src, int at, int) -> int {
    if (at < static_cast<int>(src.size()) && src[at] == '\r') {
      if (at + 1 < static_cast<int>(src.size()) && src[at + 1] == '\n') return at + 2;
      return -1;
    }
    if (at < static_cast<int>(src.size()) && src[at] == '\n') return at + 1;
    return -1;
  };
  return t;
}

inline TokenType makeWhitespace() {
  TokenType t;
  t.name = "WHITESPACE";
  t.isRegex = true;
  t.hidden = true;
  t.first = "\t ";
  t.match = [](const std::string& src, int at, int) -> int {
    int p = matchTabsSpaces(src, at);
    return p > at ? p : -1;
  };
  return t;
}

inline TokenType makeYaml() {
  TokenType t;
  t.name = "YAML";
  t.isRegex = true;
  t.hidden = true;
  t.first = "-";
  auto matchOpener = [](const std::string& src, int at, int) -> int {
    if (src.compare(at, 3, "---") != 0) return -1;
    int p = matchTabsSpaces(src, at + 3);
    if (p < static_cast<int>(src.size()) && src[p] == '\r') p++;
    if (p < static_cast<int>(src.size()) && src[p] == '\n') return p + 1;
    return -1;
  };
  t.opener = matchOpener;
  t.match = [matchOpener](const std::string& src, int at, int) -> int {
    int p = matchOpener(src, at, -1);
    if (p < 0) return -1;
    int n = static_cast<int>(src.size());
    while (p < n) {
      if (src.compare(p, 3, "---") == 0) {
        int q = p + 3;
        if (q < n && src[q] == '\r') q++;
        if (q < n && src[q] == '\n') return q + 1;
        if (q >= n || isSpace(charCodeAt(src, q))) return q;
      }
      size_t nl = src.find('\n', p);
      if (nl == std::string::npos) return -1;
      p = static_cast<int>(nl + 1);
    }
    return -1;
  };
  return t;
}

inline TokenType makeDirective() {
  TokenType t;
  t.name = "DIRECTIVE";
  t.isRegex = true;
  t.hidden = true;
  t.first = "\t %";
  auto matchOpener = [](const std::string& src, int at, int) -> int {
    int p = matchTabsSpaces(src, at);
    if (src.compare(p, 3, "%%{") == 0) return p + 3;
    return -1;
  };
  t.opener = matchOpener;
  t.match = [matchOpener](const std::string& src, int at, int) -> int {
    int p = matchOpener(src, at, -1);
    if (p < 0) return -1;
    size_t close = src.find("}%%", p);
    if (close == std::string::npos) return -1;
    int q = static_cast<int>(close + 3);
    int n = static_cast<int>(src.size());
    if (q < n && src[q] == '\r') q++;
    if (q < n && src[q] == '\n') return q + 1;
    if (q >= n || isSpace(charCodeAt(src, q))) return q;
    return -1;
  };
  return t;
}

inline TokenType makeSingleLineComment() {
  TokenType t;
  t.name = "SINGLE_LINE_COMMENT";
  t.isRegex = true;
  t.hidden = true;
  t.first = "\t %";
  t.match = [](const std::string& src, int at, int) -> int {
    int p = matchTabsSpaces(src, at);
    if (src.compare(p, 2, "%%") != 0) return -1;
    p += 2;
    while (p < static_cast<int>(src.size()) && src[p] != '\n' && src[p] != '\r') p++;
    return p;
  };
  return t;
}

inline TokenType keyword(const std::string& word) {
  TokenType t;
  t.name = word;
  t.isRegex = true;
  t.first = word.substr(0, 1);
  t.match = [word](const std::string& src, int at, int) -> int {
    if (src.compare(at, word.size(), word) != 0) return -1;
    int end = at + static_cast<int>(word.size());
    if (end >= static_cast<int>(src.size()) || isSpace(charCodeAt(src, end)) || src.compare(end, 2, "%%") == 0) {
      return end;
    }
    return -1;
  };
  return t;
}

inline std::string single(const std::string& text) {
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

inline std::string titleValue(const std::string& text) {
  size_t pos = text.find("title");
  if (pos == std::string::npos) return "";
  return single(text.substr(pos + 5));
}

inline std::string accTitleValue(const std::string& text) {
  size_t pos = text.find(':');
  if (pos == std::string::npos) return "";
  return single(text.substr(pos + 1));
}

inline std::string accDescrValue(const std::string& text) {
  size_t brace = text.find('{');
  size_t colon = text.find(':');
  if (colon != std::string::npos && (brace == std::string::npos || colon < brace)) {
    return single(text.substr(colon + 1));
  }
  if (brace != std::string::npos) {
    size_t close = text.find('}', brace + 1);
    std::string body = text.substr(brace + 1, close == std::string::npos ? std::string::npos : close - brace - 1);
    return cleanAccDescr(body);
  }
  return "";
}

inline std::string stringValue(const std::string& text) {
  if (text.size() < 2) return "";
  if (text.find('\\') == std::string::npos) return text.substr(1, text.size() - 2);
  std::string out;
  for (size_t i = 1; i + 1 < text.size(); i++) {
    char c = text[i];
    if (c == '\\' && i + 2 < text.size()) {
      char next = text[++i];
      switch (next) {
        case 'b': out.push_back('\b'); break;
        case 'f': out.push_back('\f'); break;
        case 'n': out.push_back('\n'); break;
        case 'r': out.push_back('\r'); break;
        case 't': out.push_back('\t'); break;
        case 'v': out.push_back('\v'); break;
        case '0': out.push_back('\0'); break;
        default: out.push_back(next); break;
      }
    } else {
      out.push_back(c);
    }
  }
  return out;
}

} // namespace common_tokens

class TokenReader {
public:
  int i = 0;
  const Tokens& tokens;
  const std::vector<TokenType>& types;
  std::string diagram;

  TokenReader(const Tokens& tokens_, const std::vector<TokenType>& types_, std::string diagram_)
    : tokens(tokens_), types(types_), diagram(std::move(diagram_)) {}

  int kind() const { return tokens.kinds[i]; }

  std::string text(int index = -1) const {
    int idx = index < 0 ? i : index;
    return tokens.src.substr(tokens.starts[idx], tokens.ends[idx] - tokens.starts[idx]);
  }

  std::string take() { return text(i++); }

  bool accept(int k) {
    if (tokens.kinds[i] != k) return false;
    i++;
    return true;
  }

  std::string expect(int k) {
    if (tokens.kinds[i] != k) fail("token of type '" + types[k].name + "'");
    return take();
  }

  [[noreturn]] void fail(const std::string& expected) const {
    std::string found = kind() == -1 ? "end of input" : ("`" + text() + "`");
    int at = tokens.starts[i];
    if (kind() == -1) {
      int k = i;
      while (k > 0 && trimStr(tokens.src.substr(tokens.starts[k - 1], tokens.ends[k - 1] - tokens.starts[k - 1])).empty()) k--;
      if (k > 0) at = tokens.ends[k - 1];
    }
    throw syntaxError(diagram, tokens.src, at, "Expecting " + expected + " but found " + found + ".");
  }
};

// ============================================================================
// Port of src/diagrams/common/scan.ts
// ============================================================================

struct ScanTokens {
  std::vector<int> types;
  std::vector<std::string> texts;
  std::vector<int> starts;
};

inline int lineEnd(const std::string& src, int from) {
  size_t at = src.find('\n', from);
  return at == std::string::npos ? static_cast<int>(src.size()) : static_cast<int>(at);
}

inline bool keywordAt(const std::string& src, int p, const std::string& word) {
  for (size_t i = 0; i < word.size(); i++) {
    if ((charCodeAt(src, p + static_cast<int>(i)) | 32) != static_cast<int>(word[i])) return false;
  }
  return true;
}

inline int skipTrivia(const std::string& src, int from) {
  int n = static_cast<int>(src.size());
  int p = from;
  while (p < n) {
    int c = charCodeAt(src, p);
    if (c == 37 && charCodeAt(src, p + 1) != 123) {
      p = lineEnd(src, p);
    } else if (c == 10) {
      return p;
    } else if (isSpace(c)) {
      do p++;
      while (p < n && isSpace(charCodeAt(src, p)));
    } else if (c == 35) {
      p = lineEnd(src, p);
    } else {
      return p;
    }
  }
  return p;
}

inline int accessibility(const std::string& src, int p, ScanTokens& out, int first) {
  bool descr = keywordAt(src, p, "accdescr");
  if (!descr && !keywordAt(src, p, "acctitle")) return p;
  int n = static_cast<int>(src.size());
  int q = p + 8;
  while (isSpace(charCodeAt(src, q))) q++;
  int c = charCodeAt(src, q);
  auto emit = [&](int type, int s, int e) {
    out.types.push_back(type);
    out.texts.push_back(src.substr(s, e - s));
    out.starts.push_back(s);
  };
  if (c == 58) {
    q++;
    while (isSpace(charCodeAt(src, q))) q++;
    int end = lineEnd(src, q);
    emit(descr ? first + 2 : first, p, q);
    emit(descr ? first + 3 : first + 1, q, end);
    return end;
  }
  if (!descr || c != 123) return p;
  q++;
  while (isSpace(charCodeAt(src, q))) q++;
  while (charCodeAt(src, q) != 125) {
    size_t found = src.find('}', q);
    int end = found == std::string::npos ? n : static_cast<int>(found);
    emit(first + 4, q, end);
    if (q == n) return n;
    q = end;
  }
  return q + 1;
}

// ============================================================================
// Port of src/diagrams/common/outline.ts
// ============================================================================

namespace outline {

enum T {
  END = 0,
  ERROR = 1,
  SPACELINE = 2,
  NL = 3,
  EOF_TOK = 4,
  KEYWORD = 5,
  SPACELIST = 6,
  ICON = 7,
  CLASS = 8,
  NODE_DSTART = 9,
  NODE_ID = 10,
  NODE_DESCR = 11,
  NODE_DEND = 12,
  SHAPE_DATA = 13,
};

inline std::string tokenName(int type, bool kanban) {
  static const char* NAMES[] = {
    "1", "INVALID", "SPACELINE", "NL", "EOF", "", "SPACELIST",
    "ICON", "CLASS", "NODE_DSTART", "NODE_ID", "NODE_DESCR", "NODE_DEND", "SHAPE_DATA"
  };
  return type == T::KEYWORD ? (kanban ? "KANBAN" : "MINDMAP") : NAMES[type];
}

namespace NODE_TYPE {
  constexpr int DEFAULT = 0;
  constexpr int NO_BORDER = 0;
  constexpr int ROUNDED_RECT = 1;
  constexpr int RECT = 2;
  constexpr int CIRCLE = 3;
  constexpr int CLOUD = 4;
  constexpr int BANG = 5;
  constexpr int HEXAGON = 6;
}

inline int getType(const std::string& start, const std::string& end) {
  if (start == "[") return NODE_TYPE::RECT;
  if (start == "(") return end == ")" ? NODE_TYPE::ROUNDED_RECT : NODE_TYPE::CLOUD;
  if (start == "((") return NODE_TYPE::CIRCLE;
  if (start == ")") return NODE_TYPE::CLOUD;
  if (start == "))") return NODE_TYPE::BANG;
  if (start == "{{") return NODE_TYPE::HEXAGON;
  return NODE_TYPE::DEFAULT;
}

struct OutlineDecoration {
  std::optional<std::string> icon;
  std::optional<std::string> className;
};

struct OutlineDb {
  virtual ~OutlineDb() = default;
  virtual int getType(const std::string& start, const std::string& end) { return outline::getType(start, end); }
  virtual void addNode(int level, const std::string& id, const std::string& descr, int type, const std::optional<std::string>& shapeData = std::nullopt) = 0;
  virtual void decorateNode(const OutlineDecoration& decoration) = 0;
};

inline ScanTokens tokenizeOutline(const std::string& src, bool kanban) {
  enum State { INITIAL, CLASS, ICON, NODE, NSTR, NSTR2, DATA, DATA_STR };
  int n = static_cast<int>(src.size());
  ScanTokens out;
  std::string keyword = kanban ? "kanban" : "mindmap";
  State state = INITIAL;
  int p = 0;
  int nextBreak = -1;

  auto put = [&](int type, const std::string& text, int end) {
    out.types.push_back(type);
    out.texts.push_back(text);
    out.starts.push_back(p);
    p = end;
  };
  auto emit = [&](int type, int end, State next) {
    put(type, src.substr(p, end - p), end);
    state = next;
  };
  auto upTo = [&](char ch) -> int {
    size_t e = src.find(ch, p);
    return e == std::string::npos ? n : static_cast<int>(e);
  };
  auto matchLine = [&](int from) -> int {
    int q = from;
    while (q < n && src[q] != '\n' && src[q] != '\r') q++;
    return q > from ? q : -1;
  };
  auto matchSpace = [&](int from) -> int {
    int q = from;
    while (q < n && isSpace(charCodeAt(src, q))) q++;
    return q > from ? q : -1;
  };
  auto matchId = [&](int from) -> int {
    int q = from;
    while (q < n) {
      char ch = src[q];
      if (ch == '(' || ch == '[' || ch == '\n' || ch == ')' || ch == '{' || ch == '}' || (kanban && ch == '@')) break;
      q++;
    }
    return q > from ? q : -1;
  };
  auto matchDescr = [&](int from) -> int {
    int q = from;
    while (q < n) {
      char ch = src[q];
      if (ch == ')' || ch == ']' || ch == '(' || ch == '}') break;
      q++;
    }
    return q > from ? q : -1;
  };
  auto matchMd = [&](int from) -> int {
    int q = from;
    while (q < n && src[q] != '`' && src[q] != '"') q++;
    return q > from ? q : -1;
  };
  auto matchData = [&](int from) -> int {
    int q = from;
    while (q < n && src[q] != '}' && src[q] != '^' && src[q] != '"') q++;
    return q > from ? q : -1;
  };

  while (p < n) {
    int c = charCodeAt(src, p);
    int d = charCodeAt(src, p + 1);
    int e = -1;
    if (state == INITIAL) {
      int w = (c <= 32 || c >= 160) ? std::max(matchSpace(p), p) : p;
      if (kanban && c == 64 && d == 123) {
        put(T::SHAPE_DATA, "", p + 2);
        state = DATA;
      } else if (charCodeAt(src, w) == 37 && charCodeAt(src, w + 1) == 37) {
        emit(T::SPACELINE, std::max(matchLine(w), w), INITIAL);
      } else if (w > p) {
        if (nextBreak < p) nextBreak = upTo('\n');
        if (nextBreak >= w) emit(T::SPACELIST, w, INITIAL);
        else {
          size_t lastNl = src.rfind('\n', w - 1);
          if (lastNl != std::string::npos && static_cast<int>(lastNl) > p) emit(T::SPACELINE, static_cast<int>(lastNl) + 1, INITIAL);
          else emit(T::NL, p + 1, INITIAL);
        }
      } else if (keywordAt(src, p, keyword) && !isWord(charCodeAt(src, p + static_cast<int>(keyword.size())))) {
        emit(T::KEYWORD, p + static_cast<int>(keyword.size()), INITIAL);
      } else if (c == 58 && d == 58 && charCodeAt(src, p + 2) == 58) {
        p += 3;
        state = CLASS;
      } else if (c == 58 && d == 58 && keywordAt(src, p + 2, "icon") && charCodeAt(src, p + 6) == 40) {
        p += 7;
        state = ICON;
      } else if ((c == 45 && d == 41) || (c == 40 && (d == 45 || d == 40)) || ((c == 41 || c == 123) && d == c)) {
        emit(T::NODE_DSTART, p + 2, NODE);
      } else if (c == 41 || c == 40 || c == 91) {
        emit(T::NODE_DSTART, p + 1, NODE);
      } else if ((e = matchId(p)) != -1) {
        emit(T::NODE_ID, e, INITIAL);
      } else {
        break;
      }
    } else if (state == NODE) {
      if (c == 34) {
        state = d == 96 ? NSTR2 : NSTR;
        p += d == 96 ? 2 : 1;
      } else if (c == 41 || c == 40) {
        emit(T::NODE_DEND, (d == c || (c == 40 && d == 45)) ? p + 2 : p + 1, INITIAL);
      } else if (c == 93) {
        emit(T::NODE_DEND, p + 1, INITIAL);
      } else if ((c == 125 && d == 125) || (c == 45 && d == 41)) {
        emit(T::NODE_DEND, p + 2, INITIAL);
      } else {
        int endD = c == 125 ? matchLine(p) : matchDescr(p);
        if (endD == -1) break;
        emit(T::NODE_DESCR, endD, NODE);
      }
    } else if (state == NSTR) {
      if (c == 34) {
        p++;
        state = NODE;
      } else {
        emit(T::NODE_DESCR, upTo('"'), NSTR);
      }
    } else if (state == NSTR2) {
      if (c == 96 && d == 34) {
        p += 2;
        state = NODE;
      } else if ((e = matchMd(p)) != -1) {
        emit(T::NODE_DESCR, e, NSTR2);
      } else {
        break;
      }
    } else if (state == CLASS) {
      if ((e = matchLine(p)) != -1) {
        emit(T::CLASS, e, INITIAL);
      } else if (c == 10) {
        p++;
        state = INITIAL;
      } else {
        break;
      }
    } else if (state == ICON) {
      if (c == 41) {
        p++;
        state = INITIAL;
      } else {
        emit(T::ICON, upTo(')'), ICON);
      }
    } else if (state == DATA) {
      if (c == 34) {
        emit(T::SHAPE_DATA, p + 1, DATA_STR);
      } else if (c == 125) {
        p++;
        state = INITIAL;
      } else if ((e = matchData(p)) != -1) {
        emit(T::SHAPE_DATA, e, DATA);
      } else {
        break;
      }
    } else if (c == 34) {
      emit(T::SHAPE_DATA, p + 1, DATA);
    } else {
      e = upTo('"');
      std::string raw = src.substr(p, e - p);
      std::string replaced;
      for (size_t k = 0; k < raw.size();) {
        if (raw[k] == '\n') {
          replaced += "<br/>";
          k++;
          while (k < raw.size() && isSpace(static_cast<unsigned char>(raw[k]))) k++;
        } else {
          replaced.push_back(raw[k++]);
        }
      }
      put(T::SHAPE_DATA, replaced, e);
    }
  }

  if (p < n) {
    put(T::ERROR, "", p);
  } else {
    if (state == INITIAL) put(T::EOF_TOK, "", n);
    put(T::END, "", n);
  }
  return out;
}

inline void parseOutline(const std::string& src, OutlineDb& db, bool kanban) {
  std::string diagram = kanban ? "kanban" : "mindmap";
  auto toks = tokenizeOutline(src, kanban);
  const auto& types = toks.types;
  const auto& texts = toks.texts;
  const auto& starts = toks.starts;
  std::string data = kanban ? ", 'SHAPE_DATA'" : "";
  const std::string STOP = "'SPACELINE', 'NL', 'EOF'";
  int i = 0;

  auto isStop = [](int type) {
    return type == T::SPACELINE || type == T::NL || type == T::EOF_TOK;
  };
  auto isStart = [](int type) {
    return type == T::SPACELINE || type == T::SPACELIST || type == T::ICON ||
           type == T::CLASS || type == T::NODE_DSTART || type == T::NODE_ID;
  };
  auto fail = [&](const std::string& expected) -> void {
    int t = types[i];
    if (t == T::ERROR) throw syntaxError(diagram, src, starts[i], "", true);
    throw syntaxError(diagram, src, starts[i], "Expecting " + expected + ", got '" + tokenName(t, kanban) + "'");
  };

  std::string keyword = "'" + tokenName(T::KEYWORD, kanban) + "'";
  if (types[i] == T::SPACELINE) {
    i++;
    while (types[i] == T::SPACELINE || types[i] == T::NL) i++;
    if (types[i] != T::KEYWORD) fail("'SPACELINE', 'NL', " + keyword);
  } else if (types[i] != T::KEYWORD) {
    fail("'SPACELINE', " + keyword);
  }
  i++;
  std::string first = "'SPACELINE', 'NL', 'SPACELIST', 'ICON', 'CLASS', 'NODE_DSTART', 'NODE_ID'";
  if (types[i] == T::NL) {
    i++;
    first = "'SPACELINE', 'SPACELIST', 'ICON', 'CLASS', 'NODE_DSTART', 'NODE_ID'";
  }

  for (;;) {
    int t = types[i];
    int level = 0;
    bool indented = t == T::SPACELIST;
    if (indented) {
      level = static_cast<int>(texts[i++].size());
      t = types[i];
    }
    if (t == T::NODE_ID || t == T::NODE_DSTART) {
      std::optional<std::string> id;
      std::string descr;
      int type = 0;
      if (t == T::NODE_ID) id = texts[i++];
      if (types[i] == T::NODE_DSTART) {
        std::string start = texts[i++];
        if (types[i] != T::NODE_DESCR) fail("'NODE_DESCR'");
        descr = texts[i++];
        if (types[i] != T::NODE_DEND) fail("'NODE_DEND'");
        std::string end = texts[i++];
        if (!isStop(types[i]) && types[i] != T::SHAPE_DATA) fail(STOP + data);
        type = db.getType(start, end);
      } else {
        if (!isStop(types[i]) && types[i] != T::SHAPE_DATA) fail(STOP + ", 'NODE_DSTART'" + data);
        descr = *id;
      }
      if (types[i] == T::SHAPE_DATA) {
        std::string shapeData = texts[i++];
        while (types[i] == T::SHAPE_DATA) shapeData += texts[i++];
        if (!isStop(types[i])) fail(STOP + data);
        db.addNode(level, id.value_or(descr), descr, type, shapeData);
      } else {
        db.addNode(level, id.value_or(descr), descr, type);
      }
    } else if (t == T::ICON || t == T::CLASS) {
      std::string text = texts[i++];
      if (!isStop(types[i])) fail(STOP);
      OutlineDecoration dec;
      if (t == T::ICON) dec.icon = text;
      else dec.className = text;
      db.decorateNode(dec);
    } else if (indented) {
      if (!isStop(t)) fail(STOP + ", 'ICON', 'CLASS', 'NODE_DSTART', 'NODE_ID'");
    } else if (t == T::SPACELINE) {
      if (!isStop(types[++i])) fail(STOP);
    } else {
      fail(first);
    }

    i++;
    while (types[i] == T::NL || types[i] == T::EOF_TOK) i++;
    if (types[i] == T::END) return;
    if (!isStart(types[i])) fail(STOP + ", 'SPACELIST', 'ICON', 'CLASS', 'NODE_DSTART', 'NODE_ID'");
  }
}

} // namespace outline

} // namespace Pele
