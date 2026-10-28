#pragma once
#include "../common_parse.h"
#include "../layout.h"
#include "../svg.h"
#include "../text.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Pele {
namespace requirement_detail {

// ============================================================================
// Port of src/diagrams/requirement/types.ts
// ============================================================================

struct Requirement {
  std::string name;
  std::string type;
  std::string requirementId;
  std::string text;
  std::string risk;
  std::string verifyMethod;
  std::vector<std::string> cssStyles;
  std::vector<std::string> classes;
};

struct RequirementElement {
  std::string name;
  std::string type;
  std::string docRef;
  std::vector<std::string> cssStyles;
  std::vector<std::string> classes;
};

struct Relation {
  std::string type;
  std::string src;
  std::string dst;
};

struct RequirementClass {
  std::string id;
  std::vector<std::string> styles;
  std::vector<std::string> textStyles;
};

// ============================================================================
// Port of src/diagrams/requirement/tokens.ts
// ============================================================================

enum T : int {
  END = 0,
  ERROR = 1,
  title = 2,
  acc_title = 3,
  acc_title_value = 4,
  acc_descr = 5,
  acc_descr_value = 6,
  acc_descr_multiline_value = 7,
  direction_tb = 8,
  direction_bt = 9,
  direction_rl = 10,
  direction_lr = 11,
  NEWLINE = 12,
  TOK_EOF = 13,
  RD = 14,
  STRUCT_START = 15,
  STRUCT_STOP = 16,
  STYLE_SEPARATOR = 17,
  COLONSEP = 18,
  ID = 19,
  TEXT = 20,
  RISK = 21,
  VERIFYMTHD = 22,
  REQUIREMENT = 23,
  FUNCTIONAL_REQUIREMENT = 24,
  INTERFACE_REQUIREMENT = 25,
  PERFORMANCE_REQUIREMENT = 26,
  PHYSICAL_REQUIREMENT = 27,
  DESIGN_CONSTRAINT = 28,
  LOW_RISK = 29,
  MED_RISK = 30,
  HIGH_RISK = 31,
  VERIFY_ANALYSIS = 32,
  VERIFY_DEMONSTRATION = 33,
  VERIFY_INSPECTION = 34,
  VERIFY_TEST = 35,
  ELEMENT = 36,
  CONTAINS = 37,
  COPIES = 38,
  DERIVES = 39,
  SATISFIES = 40,
  VERIFIES = 41,
  REFINES = 42,
  TRACES = 43,
  TYPE = 44,
  DOCREF = 45,
  STYLE = 46,
  CLASSDEF = 47,
  CLASS = 48,
  ALPHA = 49,
  COLON = 50,
  SEMICOLON = 51,
  PERCENT = 52,
  MINUS = 53,
  BRKT = 54,
  END_ARROW_L = 55,
  END_ARROW_R = 56,
  LINE = 57,
  qString = 58,
  unqString = 59,
  COMMA = 60,
};

inline const std::vector<std::string>& tokenNames() {
  static const std::vector<std::string> names = {
    "END",
    "ERROR",
    "title",
    "acc_title",
    "acc_title_value",
    "acc_descr",
    "acc_descr_value",
    "acc_descr_multiline_value",
    "direction_tb",
    "direction_bt",
    "direction_rl",
    "direction_lr",
    "NEWLINE",
    "EOF",
    "RD",
    "STRUCT_START",
    "STRUCT_STOP",
    "STYLE_SEPARATOR",
    "COLONSEP",
    "ID",
    "TEXT",
    "RISK",
    "VERIFYMTHD",
    "REQUIREMENT",
    "FUNCTIONAL_REQUIREMENT",
    "INTERFACE_REQUIREMENT",
    "PERFORMANCE_REQUIREMENT",
    "PHYSICAL_REQUIREMENT",
    "DESIGN_CONSTRAINT",
    "LOW_RISK",
    "MED_RISK",
    "HIGH_RISK",
    "VERIFY_ANALYSIS",
    "VERIFY_DEMONSTRATION",
    "VERIFY_INSPECTION",
    "VERIFY_TEST",
    "ELEMENT",
    "CONTAINS",
    "COPIES",
    "DERIVES",
    "SATISFIES",
    "VERIFIES",
    "REFINES",
    "TRACES",
    "TYPE",
    "DOCREF",
    "STYLE",
    "CLASSDEF",
    "CLASS",
    "ALPHA",
    "COLON",
    "SEMICOLON",
    "PERCENT",
    "MINUS",
    "BRKT",
    "END_ARROW_L",
    "END_ARROW_R",
    "LINE",
    "qString",
    "unqString",
    "COMMA",
  };
  return names;
}

inline const std::unordered_map<std::string, int>& keywordsMap() {
  static const std::unordered_map<std::string, int> kw = {
    {"requirementdiagram", T::RD},
    {"id", T::ID},
    {"text", T::TEXT},
    {"risk", T::RISK},
    {"verifymethod", T::VERIFYMTHD},
    {"requirement", T::REQUIREMENT},
    {"functionalrequirement", T::FUNCTIONAL_REQUIREMENT},
    {"interfacerequirement", T::INTERFACE_REQUIREMENT},
    {"performancerequirement", T::PERFORMANCE_REQUIREMENT},
    {"physicalrequirement", T::PHYSICAL_REQUIREMENT},
    {"designconstraint", T::DESIGN_CONSTRAINT},
    {"low", T::LOW_RISK},
    {"medium", T::MED_RISK},
    {"high", T::HIGH_RISK},
    {"analysis", T::VERIFY_ANALYSIS},
    {"demonstration", T::VERIFY_DEMONSTRATION},
    {"inspection", T::VERIFY_INSPECTION},
    {"test", T::VERIFY_TEST},
    {"element", T::ELEMENT},
    {"contains", T::CONTAINS},
    {"copies", T::COPIES},
    {"derives", T::DERIVES},
    {"satisfies", T::SATISFIES},
    {"verifies", T::VERIFIES},
    {"refines", T::REFINES},
    {"traces", T::TRACES},
    {"type", T::TYPE},
    {"docref", T::DOCREF},
    {"style", T::STYLE},
    {"classdef", T::CLASSDEF},
    {"class", T::CLASS},
  };
  return kw;
}

constexpr size_t LONGEST_KEYWORD = 22;

// ============================================================================
// Port of src/diagrams/requirement/lexer.ts
// ============================================================================

struct Tokens {
  std::vector<int> types;
  std::vector<std::string> texts;
  std::vector<size_t> starts;
};

enum LexState : int {
  S_INITIAL = 0,
  S_string = 1,
  S_style = 2,
  S_acc_title = 3,
  S_acc_descr = 4,
  S_acc_descr_multiline = 5,
};

inline bool isReqStopChar(unsigned char c) {
  return c == ':' || c == ',' || c == '\r' || c == '\n' || c == '{' || c == '<' || c == '>' || c == '-' || c == '=';
}

inline bool isJsSpaceAt(const std::string& src, size_t i, size_t& len) {
  unsigned char c = static_cast<unsigned char>(src[i]);
  if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f') {
    len = 1;
    return true;
  }
  if (c == 0xC2 && i + 1 < src.size() && static_cast<unsigned char>(src[i + 1]) == 0xA0) {
    len = 2;
    return true;
  }
  if (c == 0xE2 && i + 2 < src.size() && static_cast<unsigned char>(src[i + 1]) == 0x80) {
    unsigned char c2 = static_cast<unsigned char>(src[i + 2]);
    if (c2 == 0xA8 || c2 == 0xA9) {
      len = 3;
      return true;
    }
  }
  len = 1;
  return false;
}

inline bool isLineEndAt(const std::string& src, size_t i) {
  unsigned char c = static_cast<unsigned char>(src[i]);
  if (c == '\n' || c == '\r') return true;
  if (c == 0xE2 && i + 2 < src.size() && static_cast<unsigned char>(src[i + 1]) == 0x80) {
    unsigned char c2 = static_cast<unsigned char>(src[i + 2]);
    if (c2 == 0xA8 || c2 == 0xA9) return true;
  }
  return false;
}

inline bool matchPrefixCI(const std::string& src, size_t pos, const char* pat, size_t patLen) {
  if (pos + patLen > src.size()) return false;
  for (size_t i = 0; i < patLen; i++) {
    unsigned char a = static_cast<unsigned char>(src[pos + i]);
    unsigned char b = static_cast<unsigned char>(pat[i]);
    if ((a | 32) != (b | 32)) return false;
  }
  return true;
}

inline int matchAccDirective(const std::string& src, size_t p, const char* kw, size_t kwLen, char delim) {
  if (!matchPrefixCI(src, p, kw, kwLen)) return -1;
  size_t q = p + kwLen;
  size_t len = 1;
  while (q < src.size() && isJsSpaceAt(src, q, len)) q += len;
  if (q >= src.size() || src[q] != delim) return -1;
  q++;
  while (q < src.size() && isJsSpaceAt(src, q, len)) q += len;
  return static_cast<int>(q);
}

inline Tokens tokenize(const std::string& src) {
  size_t n = src.size();
  std::vector<int> types;
  std::vector<std::string> texts;
  std::vector<size_t> starts;
  std::vector<int> stack;
  int state = S_INITIAL;
  size_t p = 0;

  std::vector<size_t> dirStarts[4];
  std::vector<size_t> dirEnds[4];
  size_t dirSeen[4] = {0, 0, 0, 0};
  bool anyDir = false;
  int64_t lineEnd = -1;

  for (size_t i = 0; i + 12 <= n; i++) {
    if (matchPrefixCI(src, i, "direction", 9)) {
      size_t q = i + 9;
      size_t chLen = 1;
      if (q < n && isJsSpaceAt(src, q, chLen)) {
        while (q < n && isJsSpaceAt(src, q, chLen)) q += chLen;
        if (q + 2 <= n) {
          unsigned char d0 = static_cast<unsigned char>(src[q]) | 32;
          unsigned char d1 = static_cast<unsigned char>(src[q + 1]) | 32;
          int kind = -1;
          if (d0 == 't' && d1 == 'b') kind = 0;
          else if (d0 == 'b' && d1 == 't') kind = 1;
          else if (d0 == 'r' && d1 == 'l') kind = 2;
          else if (d0 == 'l' && d1 == 'r') kind = 3;
          if (kind >= 0) {
            dirStarts[kind].push_back(i);
            dirEnds[kind].push_back(q + 2);
            anyDir = true;
            i = q + 1;
          }
        }
      }
    }
  }

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
  auto lineBreak = [&](size_t q) -> size_t {
    size_t e = src.find('\n', q);
    return e == std::string::npos ? n : e;
  };

  auto direction = [&]() -> bool {
    if (lineEnd < static_cast<int64_t>(p)) {
      size_t le = p;
      while (le < n && !isLineEndAt(src, le)) le++;
      lineEnd = static_cast<int64_t>(le);
      for (int k = 0; k < 4; k++) {
        const auto& list = dirStarts[k];
        size_t j = dirSeen[k];
        while (j < list.size() && static_cast<int64_t>(list[j]) <= lineEnd) j++;
        dirSeen[k] = j;
      }
    }
    for (int k = 0; k < 4; k++) {
      int64_t j = static_cast<int64_t>(dirSeen[k]) - 1;
      if (j >= 0 && dirStarts[k][static_cast<size_t>(j)] >= p) {
        emit(T::direction_tb + k, p, lineBreak(dirEnds[k][static_cast<size_t>(j)]));
        return true;
      }
    }
    return false;
  };

  const auto& kwMap = keywordsMap();

  auto charAt = [&](size_t idx) -> int {
    return idx < n ? static_cast<unsigned char>(src[idx]) : -1;
  };

  auto initial = [&](int c) -> bool {
    int lower = c | 32;
    if (lower == 116 && p + 5 < n && matchPrefixCI(src, p + 1, "itle", 4) && isSpace(charAt(p + 5))) {
      size_t q = p + 6;
      while (q < n) {
        int d = charAt(q);
        if (d == 35 || d == 10 || d == 59) break;
        q++;
      }
      if (q > p + 6) {
        emit(T::title, p, q);
        return true;
      }
    }
    if (lower == 97 && (charAt(p + 1) | 32) == 99 && (charAt(p + 2) | 32) == 99) {
      int e = -1;
      if ((e = matchAccDirective(src, p, "accTitle", 8, ':')) >= 0) {
        push(S_acc_title);
        emit(T::acc_title, p, static_cast<size_t>(e));
        return true;
      }
      if ((e = matchAccDirective(src, p, "accDescr", 8, ':')) >= 0) {
        push(S_acc_descr);
        emit(T::acc_descr, p, static_cast<size_t>(e));
        return true;
      }
      if ((e = matchAccDirective(src, p, "accDescr", 8, '{')) >= 0) {
        push(S_acc_descr_multiline);
        p = static_cast<size_t>(e);
        return true;
      }
    }
    if (anyDir && !isLineEndAt(src, p) && direction()) return true;

    if (c == 10 || (c == 13 && charAt(p + 1) == 10)) {
      size_t q = p;
      while (q < n) {
        int d = charAt(q);
        if (d == 10) q++;
        else if (d == 13 && charAt(q + 1) == 10) q += 2;
        else break;
      }
      emit(T::NEWLINE, p, q);
      return true;
    }
    if (isSpace(c)) {
      p++;
      while (p < n && isSpace(charAt(p))) p++;
      return true;
    }
    if (c == 35 || c == 37) {
      p = lineBreak(p);
      return true;
    }

    if (isWord(c)) {
      size_t q = p + 1;
      while (q < n && isWord(charAt(q))) q++;
      if (q - p <= LONGEST_KEYWORD) {
        auto it = kwMap.find(toLowerAscii(src.substr(p, q - p)));
        if (it != kwMap.end()) {
          int keyword = it->second;
          if (keyword == T::STYLE || keyword == T::CLASSDEF || keyword == T::CLASS) push(S_style);
          emit(keyword, p, q);
          return true;
        }
      }
      while (q < n) {
        unsigned char d = static_cast<unsigned char>(src[q]);
        if (d < 128 && isReqStopChar(d)) break;
        q++;
      }
      types.push_back(T::unqString);
      texts.push_back(trimStr(src.substr(p, q - p)));
      starts.push_back(p);
      p = q;
      return true;
    }

    switch (c) {
      case 123:
        emit(T::STRUCT_START, p, p + 1);
        return true;
      case 125:
        emit(T::STRUCT_STOP, p, p + 1);
        return true;
      case 58:
        if (charAt(p + 1) == 58 && charAt(p + 2) == 58) emit(T::STYLE_SEPARATOR, p, p + 3);
        else emit(T::COLONSEP, p, p + 1);
        return true;
      case 60:
        if (charAt(p + 1) != 45) return false;
        emit(T::END_ARROW_L, p, p + 2);
        return true;
      case 45:
        if (charAt(p + 1) == 62) emit(T::END_ARROW_R, p, p + 2);
        else emit(T::LINE, p, p + 1);
        return true;
      case 34:
        push(S_string);
        p++;
        return true;
      case 44:
        emit(T::COMMA, p, p + 1);
        return true;
    }
    return false;
  };

  auto style = [&](int c) -> bool {
    if (isWord(c)) {
      size_t q = p + 1;
      while (q < n && isWord(charAt(q))) q++;
      emit(T::ALPHA, p, q);
      return true;
    }
    switch (c) {
      case 58:
        emit(T::COLON, p, p + 1);
        return true;
      case 59:
        emit(T::SEMICOLON, p, p + 1);
        return true;
      case 37:
        emit(T::PERCENT, p, p + 1);
        return true;
      case 45:
        emit(T::MINUS, p, p + 1);
        return true;
      case 35:
        emit(T::BRKT, p, p + 1);
        return true;
      case 32:
        p++;
        return true;
      case 34:
        push(S_string);
        p++;
        return true;
      case 10:
        pop();
        p++;
        return true;
      case 44:
        emit(T::COMMA, p, p + 1);
        return true;
    }
    return false;
  };

  while (p < n) {
    int c = charAt(p);
    bool ok = true;
    switch (state) {
      case S_INITIAL:
        ok = initial(c);
        break;
      case S_style:
        ok = style(c);
        break;
      case S_string:
        if (c == 34) {
          pop();
          p++;
        } else {
          size_t e = src.find('"', p);
          emit(T::qString, p, e == std::string::npos ? n : e);
        }
        break;
      case S_acc_title:
        pop();
        emit(T::acc_title_value, p, lineBreak(p));
        break;
      case S_acc_descr:
        pop();
        emit(T::acc_descr_value, p, lineBreak(p));
        break;
      case S_acc_descr_multiline:
        if (c == 125) {
          pop();
          p++;
        } else {
          size_t e = src.find('}', p);
          emit(T::acc_descr_multiline_value, p, e == std::string::npos ? n : e);
        }
        break;
    }
    if (!ok) {
      types.push_back(T::ERROR);
      texts.push_back("");
      starts.push_back(p);
      return Tokens{std::move(types), std::move(texts), std::move(starts)};
    }
  }

  int last = state == S_INITIAL
    ? T::TOK_EOF
    : state == S_string
      ? T::qString
      : state == S_acc_title
        ? T::acc_title_value
        : state == S_acc_descr
          ? T::acc_descr_value
          : state == S_acc_descr_multiline
            ? T::acc_descr_multiline_value
            : -1;
  if (last != -1) emit(last, n, n);
  emit(T::END, n, n);
  return Tokens{std::move(types), std::move(texts), std::move(starts)};
}

// ============================================================================
// Port of src/diagrams/requirement/db.ts
// ============================================================================

constexpr size_t MAX_STYLE_COPIES = 1000000;

class RequirementDb : public DiagramMeta {
public:
  std::string direction = "TB";
  std::vector<std::string> requirementOrder;
  std::unordered_map<std::string, Requirement> requirements;
  std::vector<std::string> elementOrder;
  std::unordered_map<std::string, RequirementElement> elements;
  std::vector<Relation> relations;
  std::vector<std::string> classOrder;
  std::unordered_map<std::string, RequirementClass> classes;

private:
  std::string newId_;
  std::string newText_;
  std::string newRisk_;
  std::string newVerifyMethod_;
  std::string newElementType_;
  std::string newDocRef_;
  size_t styleCopies_ = 0;

  void copy(std::vector<std::string>& target, const std::vector<std::string>& styles) {
    styleCopies_ += styles.size();
    if (styleCopies_ > MAX_STYLE_COPIES) {
      throw PeleError("Requirement diagram applies too many style declarations.", "limit", "requirement");
    }
    for (const auto& s : styles) target.push_back(s);
  }

  bool findNode(const std::string& id, std::vector<std::string>*& cssStyles, std::vector<std::string>*& clsList) {
    auto rIt = requirements.find(id);
    if (rIt != requirements.end()) {
      cssStyles = &rIt->second.cssStyles;
      clsList = &rIt->second.classes;
      return true;
    }
    auto eIt = elements.find(id);
    if (eIt != elements.end()) {
      cssStyles = &eIt->second.cssStyles;
      clsList = &eIt->second.classes;
      return true;
    }
    return false;
  }

public:
  void setDirection(const std::string& dir) { direction = dir; }

  Requirement& addRequirement(const std::string& name, const std::string& type) {
    auto it = requirements.find(name);
    if (it == requirements.end()) {
      requirementOrder.push_back(name);
      Requirement req;
      req.name = name;
      req.type = type;
      req.requirementId = newId_;
      req.text = newText_;
      req.risk = newRisk_;
      req.verifyMethod = newVerifyMethod_;
      req.classes = {"default"};
      it = requirements.emplace(name, std::move(req)).first;
    }
    newId_.clear();
    newText_.clear();
    newRisk_.clear();
    newVerifyMethod_.clear();
    return it->second;
  }

  void setNewReqId(const std::string& id) { newId_ = id; }
  void setNewReqText(const std::string& text) { newText_ = text; }
  void setNewReqRisk(const std::string& risk) { newRisk_ = risk; }
  void setNewReqVerifyMethod(const std::string& verifyMethod) { newVerifyMethod_ = verifyMethod; }

  RequirementElement& addElement(const std::string& name) {
    auto it = elements.find(name);
    if (it == elements.end()) {
      elementOrder.push_back(name);
      RequirementElement el;
      el.name = name;
      el.type = newElementType_;
      el.docRef = newDocRef_;
      el.classes = {"default"};
      it = elements.emplace(name, std::move(el)).first;
    }
    newElementType_.clear();
    newDocRef_.clear();
    return it->second;
  }

  void setNewElementType(const std::string& type) { newElementType_ = type; }
  void setNewElementDocRef(const std::string& docRef) { newDocRef_ = docRef; }

  void addRelationship(const std::string& type, const std::string& src, const std::string& dst) {
    relations.push_back(Relation{type, src, dst});
  }

  void setCssStyle(const std::vector<std::string>& ids, const std::vector<std::string>& styles) {
    for (const auto& id : ids) {
      std::vector<std::string>* cssStyles = nullptr;
      std::vector<std::string>* clsList = nullptr;
      if (!findNode(id, cssStyles, clsList)) return;
      for (const auto& s : styles) {
        if (s.find(',') != std::string::npos) {
          copy(*cssStyles, splitStr(s, ','));
        } else {
          copy(*cssStyles, {s});
        }
      }
    }
  }

  void setClass(const std::vector<std::string>& ids, const std::vector<std::string>& classNames) {
    for (const auto& id : ids) {
      std::vector<std::string>* cssStyles = nullptr;
      std::vector<std::string>* clsList = nullptr;
      if (!findNode(id, cssStyles, clsList)) continue;
      for (const auto& name : classNames) {
        clsList->push_back(name);
        auto cIt = classes.find(name);
        if (cIt != classes.end()) {
          copy(*cssStyles, cIt->second.styles);
        }
      }
    }
  }

  void defineClass(const std::vector<std::string>& ids, const std::vector<std::string>& style) {
    for (const auto& id : ids) {
      auto it = classes.find(id);
      if (it == classes.end()) {
        classOrder.push_back(id);
        RequirementClass sc;
        sc.id = id;
        it = classes.emplace(id, std::move(sc)).first;
      }
      auto& styleClass = it->second;
      for (const auto& s : style) {
        if (s.find("color") != std::string::npos) {
          std::string replaced = s;
          size_t pos = replaced.find("fill");
          if (pos != std::string::npos) replaced.replace(pos, 4, "bgFill");
          styleClass.textStyles.push_back(replaced);
        }
        styleClass.styles.push_back(s);
      }
      std::vector<std::string> split;
      for (const auto& s : style) {
        auto parts = splitStr(s, ',');
        for (const auto& pt : parts) split.push_back(pt);
      }
      for (const auto& rName : requirementOrder) {
        auto& req = requirements[rName];
        if (std::find(req.classes.begin(), req.classes.end(), id) != req.classes.end()) {
          copy(req.cssStyles, split);
        }
      }
      for (const auto& eName : elementOrder) {
        auto& el = elements[eName];
        if (std::find(el.classes.begin(), el.classes.end(), id) != el.classes.end()) {
          copy(el.cssStyles, split);
        }
      }
    }
  }

  void setAccTitle(const std::string& text) {
    size_t i = 0;
    size_t len = 1;
    while (i < text.size() && isJsSpaceAt(text, i, len)) i += len;
    accTitle = text.substr(i);
  }

  void setAccDescription(const std::string& text) {
    std::string out;
    size_t i = 0;
    while (i < text.size()) {
      if (text[i] == '\n') {
        out.push_back('\n');
        i++;
        size_t len = 1;
        while (i < text.size() && isJsSpaceAt(text, i, len)) i += len;
      } else {
        out.push_back(text[i]);
        i++;
      }
    }
    accDescr = out;
  }
};

// ============================================================================
// Port of src/diagrams/requirement/parser.ts
// ============================================================================

inline const std::vector<std::string>& requirementValues() {
  static const std::vector<std::string> vals = {
    "Requirement",
    "Functional Requirement",
    "Interface Requirement",
    "Performance Requirement",
    "Physical Requirement",
    "Design Constraint",
    "Low",
    "Medium",
    "High",
    "Analysis",
    "Demonstration",
    "Inspection",
    "Test",
    "",
    "contains",
    "copies",
    "derives",
    "satisfies",
    "verifies",
    "refines",
    "traces",
  };
  return vals;
}

inline bool isStylePart(int t) {
  return t == T::ALPHA || t == T::COLON || t == T::BRKT || t == T::MINUS || t == T::SEMICOLON;
}

inline void parseRequirement(const std::string& src, RequirementDb& db) {
  Tokens toks = tokenize(src);
  const auto& types = toks.types;
  const auto& texts = toks.texts;
  const auto& starts = toks.starts;
  size_t i = 0;
  std::vector<int> fields;
  std::vector<std::string> values;
  const auto& names = tokenNames();
  const auto& vals = requirementValues();

  auto fail = [&](int from, int to = -1, std::vector<int> others = {}) -> void {
    if (to == -1) to = from;
    int t = types[i];
    if (t == T::ERROR) throw syntaxError("requirement", src, static_cast<int>(starts[i]), "", true);
    std::vector<int> all;
    for (int k = from; k <= to; k++) all.push_back(k);
    for (int o : others) all.push_back(o);
    std::string expected;
    for (size_t idx = 0; idx < all.size(); idx++) {
      if (idx > 0) expected += ", ";
      expected += "'" + names[static_cast<size_t>(all[idx])] + "'";
    }
    throw syntaxError("requirement", src, static_cast<int>(starts[i]), "Expecting " + expected + ", got '" + names[static_cast<size_t>(t)] + "'");
  };

  auto expect = [&](int type) -> std::string {
    if (types[i] != type) fail(type);
    return texts[i++];
  };

  auto keyword = [&](int from, int to) -> std::string {
    int t = types[i];
    if (t < from || t > to) fail(from, to);
    i++;
    return vals[static_cast<size_t>(t - T::REQUIREMENT)];
  };

  auto string = [&]() -> std::string {
    int t = types[i];
    if (t != T::unqString && t != T::qString) fail(T::qString, T::unqString);
    return texts[i++];
  };

  auto directive = [&]() -> bool {
    int t = types[i];
    if (t == T::acc_title) {
      i++;
      db.setAccTitle(trimStr(expect(T::acc_title_value)));
    } else if (t == T::acc_descr) {
      i++;
      db.setAccDescription(trimStr(expect(T::acc_descr_value)));
    } else if (t == T::acc_descr_multiline_value) {
      db.setAccDescription(trimStr(texts[i++]));
    } else {
      return false;
    }
    return true;
  };

  auto idList = [&]() -> std::vector<std::string> {
    std::vector<std::string> list;
    while (true) {
      int t = types[i];
      if (t != T::ALPHA && t != T::unqString && t != T::qString) fail(T::qString, T::unqString, {T::ALPHA});
      list.push_back(texts[i++]);
      if (types[i] != T::COMMA) return list;
      i++;
    }
  };

  auto stylesOpt = [&]() -> std::vector<std::string> {
    std::vector<std::string> styles;
    while (true) {
      if (!isStylePart(types[i])) fail(T::ALPHA, T::SEMICOLON, {T::MINUS, T::BRKT});
      std::string style = texts[i++];
      while (isStylePart(types[i])) style += texts[i++];
      styles.push_back(style);
      if (types[i] != T::COMMA) return styles;
      i++;
    }
  };

  auto classList = [&]() -> std::optional<std::vector<std::string>> {
    std::optional<std::vector<std::string>> classes;
    if (types[i] == T::STYLE_SEPARATOR) {
      i++;
      classes = idList();
      expect(T::STRUCT_START);
    } else if (types[i] == T::STRUCT_START) {
      i++;
    } else {
      fail(T::STRUCT_START, T::STRUCT_START, {T::STYLE_SEPARATOR});
    }
    expect(T::NEWLINE);
    return classes;
  };

  auto body = [&](bool requirement) {
    fields.clear();
    values.clear();
    while (true) {
      int t = types[i];
      if (t == T::STRUCT_STOP) {
        i++;
        break;
      }
      if (t == T::NEWLINE) {
        i++;
        continue;
      }
      if (requirement ? (t < T::ID || t > T::VERIFYMTHD) : (t != T::TYPE && t != T::DOCREF)) {
        if (requirement) fail(T::ID, T::VERIFYMTHD, {T::NEWLINE, T::STRUCT_STOP});
        fail(T::TYPE, T::DOCREF, {T::NEWLINE, T::STRUCT_STOP});
      }
      i++;
      expect(T::COLONSEP);
      values.push_back(
        t == T::RISK ? keyword(T::LOW_RISK, T::HIGH_RISK)
                     : t == T::VERIFYMTHD ? keyword(T::VERIFY_ANALYSIS, T::VERIFY_TEST)
                                          : string()
      );
      fields.push_back(t);
      expect(T::NEWLINE);
    }
    for (int k = static_cast<int>(fields.size()) - 1; k >= 0; k--) {
      const std::string& value = values[static_cast<size_t>(k)];
      switch (fields[static_cast<size_t>(k)]) {
        case T::ID:
          db.setNewReqId(value);
          break;
        case T::TEXT:
          db.setNewReqText(value);
          break;
        case T::RISK:
          db.setNewReqRisk(value);
          break;
        case T::VERIFYMTHD:
          db.setNewReqVerifyMethod(value);
          break;
        case T::TYPE:
          db.setNewElementType(value);
          break;
        default:
          db.setNewElementDocRef(value);
          break;
      }
    }
  };

  while (directive()) {
    if (types[i] == T::NEWLINE) i++;
  }
  if (types[i] != T::RD) fail(T::RD, T::RD, {T::acc_title, T::acc_descr, T::acc_descr_multiline_value});
  i++;
  expect(T::NEWLINE);

  while (true) {
    int t = types[i];
    if (t == T::TOK_EOF) return;
    if (t == T::NEWLINE) {
      i++;
    } else if (directive()) {
      continue;
    } else if (t >= T::direction_tb && t <= T::direction_lr) {
      i++;
      db.setDirection(t == T::direction_tb ? "TB" : t == T::direction_bt ? "BT" : t == T::direction_rl ? "RL" : "LR");
    } else if (t >= T::REQUIREMENT && t <= T::DESIGN_CONSTRAINT) {
      std::string type = keyword(T::REQUIREMENT, T::DESIGN_CONSTRAINT);
      std::string name = string();
      auto classes = classList();
      body(true);
      db.addRequirement(name, type);
      if (classes.has_value()) db.setClass({name}, *classes);
    } else if (t == T::ELEMENT) {
      i++;
      std::string name = string();
      auto classes = classList();
      body(false);
      db.addElement(name);
      if (classes.has_value()) db.setClass({name}, *classes);
    } else if (t == T::STYLE) {
      i++;
      auto ids = idList();
      db.setCssStyle(ids, stylesOpt());
    } else if (t == T::CLASSDEF) {
      i++;
      auto ids = idList();
      db.defineClass(ids, stylesOpt());
    } else if (t == T::CLASS) {
      i++;
      auto ids = idList();
      db.setClass(ids, idList());
    } else if (t == T::unqString || t == T::qString) {
      std::string id = texts[i++];
      int next = types[i];
      if (next == T::STYLE_SEPARATOR) {
        i++;
        db.setClass({id}, idList());
      } else if (next == T::END_ARROW_L || next == T::LINE) {
        i++;
        std::string relationship = keyword(T::CONTAINS, T::TRACES);
        expect(next == T::LINE ? T::END_ARROW_R : T::LINE);
        std::string other = string();
        if (next == T::LINE) db.addRelationship(relationship, id, other);
        else db.addRelationship(relationship, other, id);
      } else {
        fail(T::STYLE_SEPARATOR, T::STYLE_SEPARATOR, {T::END_ARROW_L, T::LINE});
      }
    } else {
      fail(
        T::NEWLINE,
        T::TOK_EOF,
        {
          T::acc_title,
          T::acc_descr,
          T::acc_descr_multiline_value,
          T::direction_tb,
          T::direction_bt,
          T::direction_rl,
          T::direction_lr,
          T::REQUIREMENT,
          T::FUNCTIONAL_REQUIREMENT,
          T::INTERFACE_REQUIREMENT,
          T::PERFORMANCE_REQUIREMENT,
          T::PHYSICAL_REQUIREMENT,
          T::DESIGN_CONSTRAINT,
          T::ELEMENT,
          T::CLASSDEF,
          T::CLASS,
          T::STYLE,
          T::unqString,
          T::qString,
        }
      );
    }
  }
}

// ============================================================================
// Port of src/diagrams/requirement/graph.ts
// ============================================================================

struct GraphNode {
  std::string id;
  const Requirement* requirement = nullptr;
  const RequirementElement* element = nullptr;
  std::vector<std::string> cssStyles;
  std::string cssClasses;
};

struct GraphEdge {
  std::string id;
  std::optional<std::string> start;
  std::optional<std::string> end;
  std::string type;
  bool contains = false;
};

struct RequirementGraph {
  std::vector<GraphNode> nodes;
  std::vector<GraphEdge> edges;
  std::string direction;
};

inline std::string reqJoinStr(const std::vector<std::string>& items, const std::string& sep) {
  std::string out;
  for (size_t i = 0; i < items.size(); i++) {
    if (i > 0) out += sep;
    out += items[i];
  }
  return out;
}

inline RequirementGraph buildRequirementGraph(const RequirementDb& model) {
  std::vector<GraphNode> nodes;
  for (const auto& name : model.requirementOrder) {
    const auto& req = model.requirements.at(name);
    nodes.push_back(GraphNode{
      req.name,
      &req,
      nullptr,
      req.cssStyles,
      reqJoinStr(req.classes, " "),
    });
  }
  for (const auto& name : model.elementOrder) {
    const auto& el = model.elements.at(name);
    nodes.push_back(GraphNode{
      el.name,
      nullptr,
      &el,
      el.cssStyles,
      reqJoinStr(el.classes, " "),
    });
  }
  auto has = [&](const std::string& id) -> std::optional<std::string> {
    if (model.requirements.count(id) || model.elements.count(id)) return id;
    return std::nullopt;
  };
  std::vector<GraphEdge> edges;
  edges.reserve(model.relations.size());
  for (size_t idx = 0; idx < model.relations.size(); idx++) {
    const auto& relation = model.relations[idx];
    edges.push_back(GraphEdge{
      relation.src + "-" + relation.dst + "-" + std::to_string(idx),
      has(relation.src),
      has(relation.dst),
      relation.type,
      relation.type == "contains",
    });
  }
  return RequirementGraph{std::move(nodes), std::move(edges), model.direction};
}

// ============================================================================
// Port of src/diagrams/requirement/render.ts
// ============================================================================

constexpr double REQ_NODE_SEP = 40.0;
constexpr double REQ_EDGE_SEP = 16.0;
constexpr double REQ_RANK_SEP = 56.0;
constexpr double REQ_PAD_X = 14.0;
constexpr double REQ_PAD_Y = 10.0;
constexpr double REQ_ROW_GAP = 2.0;
constexpr double REQ_COLUMN_GAP = 12.0;
constexpr double REQ_MIN_WIDTH = 120.0;
constexpr double REQ_NAME_WRAP = 280.0;
constexpr double REQ_VALUE_WRAP = 240.0;
constexpr double REQ_LOOP = 26.0;
constexpr double REQ_CONTAINS_RADIUS = 6.0;
constexpr double REQ_ARROW_LENGTH = 9.0;
constexpr double REQ_ARROW_HALF = 4.5;
constexpr double REQ_END_STEP = 28.0;
constexpr double REQ_END_MARGIN = 14.0;

inline std::string num(double value) {
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

struct Row {
  Label key;
  Label value;
  double height = 0.0;
};

struct NodeView {
  GraphNode node;
  ResolvedStyle style;
  Label kind;
  Label name;
  std::vector<Row> rows;
  double keyWidth = 0.0;
  double headHeight = 0.0;
  double w = 0.0;
  double h = 0.0;
};

inline Label restyle(Label label, int bits, const TextMeasurer& measurer) {
  if (bits == 0 || label.lines.empty()) return label;
  double width = 0.0;
  std::vector<double> widths;
  widths.reserve(label.lines.size());
  for (auto& line : label.lines) {
    double total = 0.0;
    for (auto& span : line) {
      int st = span.style | bits;
      double w = (span.hasIcon || span.hasMath) ? span.width : measurer.width(span.text, label.size, st);
      total += w;
      span.style = st;
      span.width = w;
    }
    widths.push_back(total);
    if (total > width) width = total;
  }
  label.widths = std::move(widths);
  label.width = width;
  return label;
}

inline std::string startText(const Label& label, double x, double top, const std::string& attrs) {
  int shared = sharedStyle(label);
  std::string body;
  std::string math;
  for (size_t i = 0; i < label.lines.size(); i++) {
    const auto& line = label.lines[i];
    double middle = top + lineMiddle(label, i);
    if (hasMath(line)) {
      math += labelSvg(labelLine(label, i), x + label.widths[i] / 2.0, middle, attrs);
      continue;
    }
    double y = middle + label.size * 0.35;
    bool first = true;
    for (const auto& span : line) {
      if (span.hasIcon || span.text.empty()) continue;
      body += "<tspan" + (first ? (" x=\"" + num(x) + "\" y=\"" + num(y) + "\"") : "") +
              spanStyle(span.style & ~shared) + ">" + esc(span.text) + "</tspan>";
      first = false;
    }
  }
  return (body.empty() ? "" : ("<text" + textStyle(attrs, shared) + " text-anchor=\"start\" xml:space=\"preserve\">" + body + "</text>")) + math;
}

inline std::string containsMarker(double x, double y, double dx, double dy) {
  double r = REQ_CONTAINS_RADIUS;
  double cx = x - dx * r;
  double cy = y - dy * r;
  return "<circle class=\"pele-marker\" cx=\"" + num(cx) + "\" cy=\"" + num(cy) + "\" r=\"" + num(r) + "\"/>" +
         "<path class=\"pele-marker\" d=\"M" + num(cx - dx * r) + "," + num(cy - dy * r) + "L" + num(cx + dx * r) + "," + num(cy + dy * r) +
         "M" + num(cx + dy * r) + "," + num(cy - dx * r) + "L" + num(cx - dy * r) + "," + num(cy + dx * r) + "\"/>";
}

inline std::string arrowMarker(double x, double y, double dx, double dy) {
  double bx = x - dx * REQ_ARROW_LENGTH;
  double by = y - dy * REQ_ARROW_LENGTH;
  return "<path class=\"pele-marker\" d=\"M" + num(bx - dy * REQ_ARROW_HALF) + "," + num(by + dx * REQ_ARROW_HALF) +
         "L" + num(x) + "," + num(y) + "L" + num(bx + dy * REQ_ARROW_HALF) + "," + num(by - dx * REQ_ARROW_HALF) +
         "\" stroke-linejoin=\"round\"/>";
}

inline void spreadEnds(std::vector<CNode>& cnodes, std::vector<CEdge>& cedges, const std::vector<NodeView>& views) {
  struct SideEnd {
    std::vector<double>* route;
    size_t at;
    double toward;
  };
  std::vector<int> sideOrder;
  std::unordered_map<int, std::vector<SideEnd>> sides;
  for (auto& edge : cedges) {
    auto& route = edge.route;
    if (edge.src == edge.dst || route.size() < 6) continue;
    size_t ends[2] = {0, route.size() - 3};
    for (size_t at : ends) {
      int node = (at == 0) ? edge.src : edge.dst;
      const CNode& c = cnodes[static_cast<size_t>(node)];
      double dx = route[at] - c.x;
      double dy = route[at + 1] - c.y;
      size_t next = (at == 0) ? 3 : (at - 3);
      int side = -1;
      if (std::abs(std::abs(dy) - c.h / 2.0) < 0.5) side = dy < 0 ? 0 : 2;
      else if (std::abs(std::abs(dx) - c.w / 2.0) < 0.5) side = dx < 0 ? 3 : 1;
      else continue;
      int key = node * 4 + side;
      SideEnd end{&route, at, route[(side % 2 == 0) ? next : (next + 1)]};
      auto it = sides.find(key);
      if (it != sides.end()) {
        it->second.push_back(end);
      } else {
        sideOrder.push_back(key);
        sides.emplace(key, std::vector<SideEnd>{end});
      }
    }
  }
  for (int key : sideOrder) {
    auto& list = sides[key];
    if (list.size() < 2) continue;
    size_t node = static_cast<size_t>(key >> 2);
    bool vertical = (key % 2) == 1;
    double span = (vertical ? views[node].h : views[node].w) - 2.0 * REQ_END_MARGIN;
    double step = std::max(0.0, std::min(REQ_END_STEP, span / static_cast<double>(list.size() - 1)));
    std::stable_sort(list.begin(), list.end(), [](const SideEnd& a, const SideEnd& b) {
      return a.toward < b.toward;
    });
    for (size_t j = 0; j < list.size(); j++) {
      double offset = (static_cast<double>(j) - static_cast<double>(list.size() - 1) / 2.0) * step;
      if (vertical) (*list[j].route)[list[j].at + 1] = cnodes[node].y + offset;
      else (*list[j].route)[list[j].at] = cnodes[node].x + offset;
    }
  }
}

inline std::string reqDirection(const std::string& dir) {
  return (dir == "BT" || dir == "LR" || dir == "RL") ? dir : "TB";
}

// ============================================================================
// Exact port of src/layout/layered.ts for requirement's compoundLayout
// ============================================================================

namespace req_layered {

constexpr int MAX_SWEEPS = 24;
constexpr int MAX_DUMMIES = 20000;
constexpr double MIN_SLOPE = 0.125;
constexpr double MAX_GAP = 3.0;

inline bool isDummyKind(uint8_t kind) {
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
    std::vector<int>& spare) {
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
        if (state[w] == 1) {
          e.reversed = true;
        } else if (state[w] == 0) {
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

inline void rank(std::vector<LNode>& nodes, std::vector<LEdge>& edges, int step) {
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
  std::vector<uint8_t> fixed;
};

inline double halfRoom(const Graph& g, const LayeredOptions& opt, int v) {
  return g.W[v] / 2.0 + (g.KIND[v] == static_cast<uint8_t>(LKind::Node) ? opt.nodeSep : opt.edgeSep) / 2.0;
}

inline void reduceCrossings(Graph& g, const std::vector<LNode>& nodes) {
  auto& n = g.n;
  auto& real = g.real;
  auto& maxRank = g.maxRank;
  auto& EF = g.EF;
  auto& ET = g.ET;
  auto& succStart = g.succStart;
  auto& succE = g.succE;
  auto& predStart = g.predStart;
  auto& predE = g.predE;
  auto& layerStart = g.layerStart;
  auto& order = g.order;
  auto& pos = g.pos;
  auto& fixed = g.fixed;

  int widest = 0;
  for (int r = 0; r <= maxRank; r++) widest = std::max(widest, layerStart[r + 1] - layerStart[r]);
  if (widest < 2) return;

  int size = 1;
  while (size < widest) size <<= 1;
  std::vector<int> tree(2 * size, 0);
  std::vector<int> targets(EF.size(), 0);

  auto count = [&]() -> int {
    int total = 0;
    for (int r = 0; r < maxRank; r++) {
      int southLen = layerStart[r + 2] - layerStart[r + 1];
      if (southLen < 2) continue;
      int t = 0;
      for (int i = layerStart[r]; i < layerStart[r + 1]; i++) {
        int v = order[i];
        int from = succStart[v];
        int to = succStart[v + 1];
        int t0 = t;
        for (int k = from; k < to; k++) targets[t++] = pos[ET[succE[k]]];
        int d = t - t0;
        if (d < 2) continue;
        if (d <= 12) {
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
  int best = declared;
  if (best == 0) return;
  std::vector<int> bestOrder = order;
  int start = best;

  std::vector<double> bary(n, 0.0);
  std::vector<int> moving(widest, 0);
  std::vector<int> spare(widest, 0);

  auto sweep = [&](bool down, bool biasRight) {
    const auto& st = down ? predStart : succStart;
    const auto& list = down ? predE : succE;
    const auto& other = down ? EF : ET;
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
          bary[v] = -1.0;
          continue;
        }
        double sum = 0.0;
        for (int k = from; k < to; k++) {
          int u = other[list[k]];
          sum += pos[u];
        }
        bary[v] = sum / static_cast<double>(to - from);
        moving[movable++] = v;
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
    for (int r = 0; r <= maxRank; r++) {
      for (int i = layerStart[r]; i < layerStart[r + 1]; i++) pos[order[i]] = i - layerStart[r];
    }
  };

  auto pinOf = [&](int v) -> int { return v < real ? nodes[v].pin : 0; };
  auto at = [&](int e, bool up) -> double {
    int u = up ? EF[e] : ET[e];
    return static_cast<double>(pos[u]);
  };
  auto between = [&](int a, int b) -> int {
    int c = 0;
    for (int k = predStart[a]; k < predStart[a + 1]; k++) {
      double x = at(predE[k], true);
      for (int j = predStart[b]; j < predStart[b + 1]; j++) if (x > at(predE[j], true)) c++;
    }
    for (int k = succStart[a]; k < succStart[a + 1]; k++) {
      double x = at(succE[k], false);
      for (int j = succStart[b]; j < succStart[b + 1]; j++) if (x > at(succE[j], false)) c++;
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
          if (pinOf(a) != pinOf(b)) continue;
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
  int chosenOut = std::numeric_limits<int>::max();
  for (int attempt = 0; attempt < (small ? 2 : 1); attempt++) {
    if (attempt > 0) {
      order = initial;
      settlePositions();
    }
    int found = start;
    std::vector<int> foundOrder = initial;
    for (int i = 0, stale = 0; stale < 4 && i < MAX_SWEEPS; i++, stale++) {
      sweep((i + attempt) % 2 == 0, (i % 4) >= 2);
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
    int out = small ? outOfOrder() : 0;
    if (attempt == 0 || found < best || (found == best && out < chosenOut)) {
      best = found;
      chosen = foundOrder;
      chosenOut = out;
    }
  }
  order = chosen;
  settlePositions();
  bestOrder = chosen;

  if (!small) return;
  for (int i = 0; i < real; i++) if (nodes[i].pin != 0) return;
  if (chosenOut == 0 || fixed[0] || fixed[maxRank]) return;
  for (int r = 0; r <= maxRank; r++) {
    std::reverse(order.begin() + layerStart[r], order.begin() + layerStart[r + 1]);
  }
  settlePositions();
  if (count() > best || outOfOrder() >= chosenOut) {
    order = bestOrder;
    settlePositions();
  }
}

inline std::vector<double> position(Graph& g, const LayeredOptions& opt) {
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
      if (isDummyKind(KIND[v])) {
        int u = EF[predE[predStart[v]]];
        if (isDummyKind(KIND[u])) inner = u;
      }
      int k1 = inner >= 0 ? pos[inner] : prevLen;
      if (inner >= 0 || i == end - 1) {
        for (int j = scan; j <= i; j++) {
          int w = order[j];
          bool wDummy = isDummyKind(KIND[w]);
          for (int k = predStart[w]; k < predStart[w + 1]; k++) {
            int e = predE[k];
            int up = pos[EF[e]];
            if ((up < k0 || k1 < up) && !(wDummy && isDummyKind(KIND[EF[e]]))) conflict[e] = 1;
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
    bool up = variant < 2;
    bool right = (variant & 1) == 1;
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
      int adjacent = up ? r - 1 : r + 1;
      int adjacentLast = (adjacent >= 0 && adjacent <= maxRank) ? (layerStart[adjacent + 1] - layerStart[adjacent] - 1) : 0;
      int prevIdx = -1;
      for (int i = right ? end - 1 : s; right ? i >= s : i < end; right ? i-- : i++) {
        int v = order[i];
        int from = start[v];
        int d = start[v + 1] - from;
        if (d == 0) continue;
        int lo = from + ((d - 1) >> 1);
        int hi = from + (d >> 1);
        for (int pass = 0; pass < 2; pass++) {
          if (pass == 1 && lo == hi) break;
          int e = list[(pass == 0) != right ? lo : hi];
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
  double mins[4], maxs[4];
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
    double delta = (k & 1) == 0 ? (mins[narrowest] - mins[k]) : (maxs[narrowest] - maxs[k]);
    if (delta != 0.0) {
      auto& xs = results[k];
      for (int i = 0; i < n; i++) xs[i] += delta;
    }
  }

  std::vector<double> out(n, 0.0);
  for (int i = 0; i < n; i++) {
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

inline std::vector<uint8_t> emptyRanks(const Graph& g) {
  std::vector<uint8_t> empty(g.maxRank + 1, 0);
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
  return any ? empty : std::vector<uint8_t>{};
}

inline void straighten(
    const Graph& g,
    std::vector<double>& xs,
    const std::vector<uint8_t>& empty,
    const std::vector<int>& firstDummy,
    const std::vector<int>& span,
    double sep,
    std::vector<uint8_t>& onLine) {
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
      double steps = static_cast<double>(last - d + 2);
      for (int k = d; k <= last; k++) {
        want[k] = x0 + ((x1 - x0) * static_cast<double>(k - d + 1)) / steps;
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
      std::sort(twins.begin(), twins.end(), [&](int a, int b) { return g.pos[a] < g.pos[b]; });
      for (size_t k = 0; k < twins.size(); k++) {
        int d = twins[k];
        xs[d] = want[d] + (static_cast<double>(k) - static_cast<double>(twins.size() - 1) / 2.0) * sep;
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
    std::vector<uint8_t>& onLine) {
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
      bool free = true;
      int r = RANK[d];
      for (int i = layerStart[r]; free && i < layerStart[r + 1]; i++) {
        int label = order[i];
        if (KIND[label] == static_cast<uint8_t>(LKind::Label) &&
            xs[label] + W[label] / 2.0 > lo &&
            xs[label] - W[label] / 2.0 < hi) {
          free = false;
        }
      }
      if (!free) continue;
      onLine[d] = 1;
      double steps = static_cast<double>(last - first + 2);
      for (int k = first; k <= last; k++) {
        xs[k] = from + ((to - from) * static_cast<double>(k - first + 1)) / steps;
      }
    }
  }
}

inline LayeredResult layered(std::vector<LNode>& nodes, std::vector<LEdge>& edges, const LayeredOptions& opt) {
  int real = static_cast<int>(nodes.size());
  if (real == 0) return {0, 0, 0, 0};
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
      if (e.labelW > 0 || seen.count(key)) {
        step = 2;
        break;
      }
      seen.insert(key);
    }
  }

  req_layered::orient(nodes, edges);
  req_layered::rank(nodes, edges, step);

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
      return span[b] != span[a] ? span[b] < span[a] : a < b;
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
  std::vector<int> firstSeg(m, -1);
  std::vector<int> lastSeg(m, -1);
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
      int ra = RANK[a];
      int rb = RANK[b];
      int labelRank = -1;
      if (e.labelW > 0) {
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
      for (int r = ra + 1; r < rb; r++) {
        int d = next++;
        RANK[d] = r;
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
      lastSeg[ei] = ce;
      ce++;
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

  std::vector<uint8_t> fixed(maxRank + 1, 0);
  auto setPositions = [&]() {
    for (int r = 0; r <= maxRank; r++) {
      int s = layerStart[r];
      for (int i = s, e = layerStart[r + 1]; i < e; i++) pos[order[i]] = i - s;
    }
  };
  setPositions();

  Graph g{n, real, maxRank, W, KIND, RANK, EF, ET, EFD, ETD, succStart, succE, predStart, predE, layerStart, order, pos, fixed};

  if (segments > 0 && maxRank > 0) {
    reduceCrossings(g, nodes);
    setPositions();
  }

  std::vector<double> xs = position(g, opt);
  std::vector<uint8_t> empty = emptyRanks(g);

  std::vector<uint8_t> onLine(n, 0);
  if (!empty.empty() && !opt.tracks) straighten(g, xs, empty, firstDummy, span, opt.apart ? 0.0 : opt.edgeSep, onLine);
  if (!opt.tracks) pastLabels(g, xs, firstDummy, span, opt.edgeSep, onLine);

  std::vector<double> run(maxRank + 1, 0.0);
  for (int e = 0; e < segments; e++) {
    double across = std::abs(xs[g.EF[e]] + g.EFD[e] - xs[g.ET[e]] - g.ETD[e]);
    if (across > run[g.RANK[g.EF[e]]]) run[g.RANK[g.EF[e]]] = across;
  }
  std::vector<double> bandTop(maxRank + 1, 0.0);
  std::vector<double> bandBottom(maxRank + 1, 0.0);
  double usual = opt.rankSep / static_cast<double>(step);
  std::vector<double> gapUnder(maxRank + 1, 0.0);
  for (int r = 0; r < maxRank; r++) {
    gapUnder[r] = std::max(usual, std::min((MAX_GAP * opt.rankSep) / static_cast<double>(step), run[r] * MIN_SLOPE));
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
    return (g.KIND[d] == static_cast<uint8_t>(LKind::Dummy) && between[g.RANK[d]] && !opt.tracks)
      ? (bandBottom[g.RANK[d]] - bandTop[g.RANK[d]]) / 4.0
      : 0.0;
  };

  double y = 0.0;
  for (int r = 0; r <= maxRank; r++) {
    double h = 0.0;
    for (int i = g.layerStart[r]; i < g.layerStart[r + 1]; i++) {
      if (H[g.order[i]] > h) h = H[g.order[i]];
    }
    bandTop[r] = y;
    bandBottom[r] = y + h;
    y += h + gapUnder[r];
  }
  double height = y;

  double minX = std::numeric_limits<double>::infinity();
  double maxX = -std::numeric_limits<double>::infinity();
  for (int i = 0; i < n; i++) {
    double half = g.W[i] / 2.0;
    if (xs[i] - half < minX) minX = xs[i] - half;
    if (xs[i] + half > maxX) maxX = xs[i] + half;
  }
  for (int i = 0; i < real; i++) {
    auto& node = nodes[i];
    int r = node.rank;
    node.x = xs[i] - minX;
    node.y = (bandTop[r] + bandBottom[r]) / 2.0;
  }

  for (int ei = 0; ei < m; ei++) {
    auto& e = edges[ei];
    const auto& first = nodes[e.reversed ? e.head : e.tail];
    const auto& last = nodes[e.reversed ? e.tail : e.head];
    std::vector<double> pts = {first.x + (e.reversed ? e.headDx : e.tailDx), first.y + first.h / 2.0};
    int d0 = firstDummy[ei];
    if (d0 >= 0) {
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

  return {maxX - minX, height, 0, 0};
}

} // namespace req_layered

inline Size reqCompoundLayout(
    std::vector<CNode>& nodes,
    std::vector<CEdge>& edges,
    const std::string& rootDir,
    const LayeredOptions& opt) {
  bool vertical = (rootDir == "TB" || rootDir == "BT");
  bool flip = (rootDir == "BT" || rootDir == "RL");
  size_t n = nodes.size();

  std::vector<LNode> lnodes;
  lnodes.reserve(n);
  for (size_t i = 0; i < n; i++) {
    LNode ln;
    ln.w = vertical ? nodes[i].w : nodes[i].h;
    ln.h = vertical ? nodes[i].h : nodes[i].w;
    ln.span = nodes[i].span;
    ln.kind = LKind::Node;
    lnodes.push_back(ln);
  }

  std::vector<size_t> loops;
  std::vector<size_t> edgeIndices;
  std::vector<LEdge> ledges;
  for (size_t ei = 0; ei < edges.size(); ei++) {
    auto& e = edges[ei];
    e.route.clear();
    if (e.src == e.dst) {
      loops.push_back(ei);
      continue;
    }
    edgeIndices.push_back(ei);
    LEdge le;
    le.tail = e.src;
    le.head = e.dst;
    le.minlen = e.minlen;
    le.labelW = vertical ? e.labelW : e.labelH;
    le.labelH = vertical ? e.labelH : e.labelW;
    ledges.push_back(le);
  }

  LayeredResult size = req_layered::layered(lnodes, ledges, opt);
  double contentW = vertical ? size.width : size.height;
  double contentH = vertical ? size.height : size.width;
  double axis = vertical ? 0.0 : 1.0;

  auto tx = [&](double lx, double ly) -> double {
    return vertical ? lx : (flip ? size.height - ly : ly);
  };
  auto ty = [&](double lx, double ly) -> double {
    return vertical ? (flip ? size.height - ly : ly) : lx;
  };

  for (size_t i = 0; i < n; i++) {
    nodes[i].x = tx(lnodes[i].x, lnodes[i].y);
    nodes[i].y = ty(lnodes[i].x, lnodes[i].y);
  }
  for (size_t k = 0; k < edgeIndices.size(); k++) {
    size_t ei = edgeIndices[k];
    const auto& le = ledges[k];
    auto& e = edges[ei];
    std::vector<double> out;
    out.reserve(le.points.size() / 2 * 3);
    for (size_t p = 0; p + 1 < le.points.size(); p += 2) {
      out.push_back(tx(le.points[p], le.points[p + 1]));
      out.push_back(ty(le.points[p], le.points[p + 1]));
      out.push_back(axis);
    }
    e.route = std::move(out);
    if (e.labelW > 0) {
      e.labelX = tx(le.labelX, le.labelY);
      e.labelY = ty(le.labelX, le.labelY);
    }
  }
  for (size_t ei : loops) {
    auto& e = edges[ei];
    const auto& node = nodes[static_cast<size_t>(e.src)];
    e.labelX = node.x + node.w / 2.0 + 28.0 + e.labelW / 2.0;
    e.labelY = node.y;
  }

  return Size{contentW, contentH};
}

inline std::string stripDefaultClass(const std::string& cssClasses) {
  if (startsWith(cssClasses, "default ")) return cssClasses.substr(8);
  if (cssClasses == "default") return "";
  return cssClasses;
}

inline Rendered draw(const RequirementDb& model, const Config& /*_config*/, const RenderOptions& options, bool turned) {
  RequirementGraph graph = buildRequirementGraph(model);
  double size = options.fontSize;
  double small = std::round(size * 0.875);
  const TextMeasurer& measurer = getMeasurer(options);
  double pad = options.padding;

  std::unordered_map<std::string, int> index;
  std::vector<NodeView> views;
  std::vector<CNode> cnodes;
  for (const auto& node : graph.nodes) {
    ResolvedStyle style = resolveStyle(node.cssStyles);
    int base = (style.bold ? Style::Bold : 0) | (style.italic ? Style::Italic : 0);
    double nameSize = style.hasFontSize ? style.fontSize : size;
    double bodySize = style.hasFontSize ? style.fontSize : small;
    const Requirement* requirement = node.requirement;
    const RequirementElement* element = node.element;
    std::string kindStr = "\xC2\xAB" + (requirement ? requirement->type : std::string("Element")) + "\xC2\xBB";
    Label kind = restyle(layoutLabel(kindStr, false, measurer, bodySize, 4000.0), base, measurer);
    Label name = restyle(layoutLabel(node.id, true, measurer, nameSize, REQ_NAME_WRAP), base | Style::Bold, measurer);

    std::vector<std::pair<std::string, std::string>> fields;
    if (requirement) {
      fields = {
        {"ID", requirement->requirementId},
        {"Text", requirement->text},
        {"Risk", requirement->risk},
        {"Verification", requirement->verifyMethod},
      };
    } else if (element) {
      fields = {
        {"Type", element->type},
        {"Doc Ref", element->docRef},
      };
    }

    std::vector<Row> rows;
    double keyWidth = 0.0;
    double valueWidth = 0.0;
    double bodyHeight = 0.0;
    for (const auto& kv : fields) {
      if (kv.second.empty()) continue;
      Label key = restyle(layoutLabel(kv.first, false, measurer, bodySize, 4000.0), base, measurer);
      Label value = restyle(layoutLabel(kv.second, true, measurer, bodySize, REQ_VALUE_WRAP), base, measurer);
      double height = std::max(key.height, value.height);
      keyWidth = std::max(keyWidth, key.width);
      valueWidth = std::max(valueWidth, value.width);
      bodyHeight += height + REQ_ROW_GAP;
      rows.push_back(Row{std::move(key), std::move(value), height});
    }
    if (!rows.empty()) bodyHeight += 2.0 * REQ_PAD_Y - REQ_ROW_GAP;
    double headHeight = 2.0 * REQ_PAD_Y + kind.height + name.height;
    double w = std::max({
      REQ_MIN_WIDTH,
      std::max(kind.width, name.width) + 2.0 * REQ_PAD_X,
      !rows.empty() ? (keyWidth + REQ_COLUMN_GAP + valueWidth + 2.0 * REQ_PAD_X) : 0.0,
    });
    if (index.find(node.id) == index.end()) {
      index[node.id] = static_cast<int>(views.size());
    }
    CNode c;
    c.w = w;
    c.h = headHeight + bodyHeight;
    views.push_back(NodeView{
      node,
      std::move(style),
      std::move(kind),
      std::move(name),
      std::move(rows),
      keyWidth,
      headHeight,
      w,
      headHeight + bodyHeight,
    });
    cnodes.push_back(c);
  }

  std::string rootDir = turned ? "TB" : reqDirection(graph.direction);
  bool sideways = (rootDir == "LR" || rootDir == "RL");
  std::vector<CEdge> cedges;
  std::vector<Label> edgeLabels;
  std::vector<GraphEdge> drawn;
  for (const auto& edge : graph.edges) {
    if (!edge.start.has_value() || !edge.end.has_value()) continue;
    auto sIt = index.find(*edge.start);
    auto dIt = index.find(*edge.end);
    if (sIt == index.end() || dIt == index.end()) continue;
    int src = sIt->second;
    int dst = dIt->second;
    Label label = layoutLabel("\xC2\xAB" + edge.type + "\xC2\xBB", false, measurer, small, 4000.0);
    if (src == dst) {
      if (sideways) cnodes[static_cast<size_t>(src)].h += 2.0 * (REQ_LOOP + label.height + 8.0);
      else cnodes[static_cast<size_t>(src)].w += 2.0 * (REQ_LOOP + label.width + 12.0);
    }
    drawn.push_back(edge);
    CEdge ce;
    ce.src = src;
    ce.dst = dst;
    ce.minlen = 1;
    ce.labelW = label.width + 12.0;
    ce.labelH = label.height + 4.0;
    cedges.push_back(std::move(ce));
    edgeLabels.push_back(std::move(label));
  }

  LayeredOptions lopt;
  lopt.nodeSep = REQ_NODE_SEP;
  lopt.edgeSep = REQ_EDGE_SEP;
  lopt.rankSep = REQ_RANK_SEP;
  lopt.portSep = 20.0;
  Size layout = reqCompoundLayout(cnodes, cedges, rootDir, lopt);

  spreadEnds(cnodes, cedges, views);

  Label title = titleLabel(model.title, measurer, size, 4000.0);
  double titleHeight = title.height > 0.0 ? (title.height + 12.0) : 0.0;
  double inner = std::max(layout.width, title.width);
  double ox = pad + (inner - layout.width) / 2.0;
  double oy = pad + titleHeight;
  double width = std::ceil(inner + 2.0 * pad);
  double height = std::ceil(layout.height + titleHeight + 2.0 * pad);

  std::string nodesOut;
  for (size_t i = 0; i < views.size(); i++) {
    const auto& view = views[i];
    double w = view.w;
    double h = view.h;
    const auto& style = view.style;
    std::string color = inkFill(style);
    std::string muted = !color.empty() ? color : " fill=\"var(--_m)\"";
    double left = -w / 2.0;
    double top = -h / 2.0;
    std::string body =
      "<rect x=\"" + num(left) + "\" y=\"" + num(top) + "\" width=\"" + num(w) + "\" height=\"" + num(h) +
      "\" fill=\"var(--_s)\" stroke=\"var(--_b)\"" + rounded(style.shape) + "/>" +
      labelSvg(
        view.kind,
        0.0,
        top + REQ_PAD_Y + view.kind.height / 2.0,
        " class=\"pele-stereotype\" font-size=\"" + num(view.kind.size) + "\"" + muted + style.text
      ) +
      labelSvg(
        view.name,
        0.0,
        top + REQ_PAD_Y + view.kind.height + view.name.height / 2.0,
        " class=\"pele-label\" font-size=\"" + num(view.name.size) + "\" font-weight=\"var(--_hw)\"" + color + style.text
      );
    if (!view.rows.empty()) {
      double divider = top + view.headHeight;
      body += "<path class=\"pele-divider\" d=\"M" + num(left) + "," + num(divider) + "H" + num(-left) +
              "\" stroke=\"var(--_b)\"" + style.line + "/>";
      double y = divider + REQ_PAD_Y;
      std::string rowsStr;
      for (const auto& row : view.rows) {
        rowsStr +=
          startText(row.key, left + REQ_PAD_X, y, " class=\"pele-field\"" + muted + style.text) +
          startText(row.value, left + REQ_PAD_X + view.keyWidth + REQ_COLUMN_GAP, y, " class=\"pele-value\"" + color + style.text);
        y += row.height + REQ_ROW_GAP;
      }
      body += "<g class=\"pele-fields\" font-size=\"" + num(view.rows[0].key.size) + "\">" + rowsStr + "</g>";
    }
    nodesOut += "<g class=\"pele-node " + std::string(view.node.requirement ? "pele-requirement" : "pele-element") +
                classNames(stripDefaultClass(view.node.cssClasses)) +
                "\" data-id=\"" + escText(view.node.id) + "\" transform=\"translate(" +
                num(cnodes[i].x + ox) + "," + num(cnodes[i].y + oy) + ")\">" + body + "</g>";
  }

  std::string edgesOut;
  std::string labelsOut;
  std::unordered_map<int, int> loopSeen;
  for (size_t i = 0; i < cedges.size(); i++) {
    const auto& ce = cedges[i];
    const auto& edge = drawn[i];
    const auto& label = edgeLabels[i];
    double startTrim = edge.contains ? 2.0 * REQ_CONTAINS_RADIUS : 0.0;
    EdgePath path;
    double labelX = ce.labelX + ox;
    double labelY = ce.labelY + oy;
    if (ce.src == ce.dst) {
      const auto& view = views[static_cast<size_t>(ce.src)];
      double cx = cnodes[static_cast<size_t>(ce.src)].x + ox;
      double cy = cnodes[static_cast<size_t>(ce.src)].y + oy;
      int k = loopSeen[ce.src]++;
      double half = (sideways ? view.w : view.h) / 2.0;
      double spread = std::min(half - 4.0, 12.0 + k * 6.0);
      double reach = REQ_LOOP + k * 8.0;
      auto at = [&](double outDist, double along) -> std::string {
        return sideways
          ? (num(cx + along) + "," + num(cy + view.h / 2.0 + outDist))
          : (num(cx + view.w / 2.0 + outDist) + "," + num(cy + along));
      };
      double pull = reach / 0.75;
      path.d = "M" + at(startTrim, -spread) + "C" + at(pull, -spread) + " " + at(pull, spread) + " " + at(0.0, spread);
      path.sx = sideways ? (cx - spread) : (cx + view.w / 2.0);
      path.sy = sideways ? (cy + view.h / 2.0) : (cy - spread);
      path.sdx = sideways ? 0.0 : -1.0;
      path.sdy = sideways ? -1.0 : 0.0;
      path.ex = sideways ? (cx + spread) : (cx + view.w / 2.0);
      path.ey = sideways ? (cy + view.h / 2.0) : (cy + spread);
      path.edx = sideways ? 0.0 : -1.0;
      path.edy = sideways ? -1.0 : 0.0;
      if (sideways) {
        labelX = cx;
        labelY = cy + view.h / 2.0 + reach + 4.0 + ce.labelH / 2.0;
      } else {
        labelX = cx + view.w / 2.0 + reach + 4.0 + ce.labelW / 2.0;
        labelY = cy;
      }
    } else {
      std::vector<double> route = ce.route;
      for (size_t k = 0; k < route.size(); k += 3) {
        route[k] += ox;
        route[k + 1] += oy;
      }
      path = routePath(route, "", startTrim, 0.0);
    }
    edgesOut +=
      "<g class=\"pele-edge pele-relationship-" + std::string(edge.contains ? "contains" : "arrow") +
      "\" data-id=\"" + escText(edge.id) + "\">" +
      "<path d=\"" + path.d + "\"" + (edge.contains ? "" : " stroke-dasharray=\"5 4\"") + "/>" +
      (edge.contains
        ? containsMarker(path.sx, path.sy, path.sdx, path.sdy)
        : arrowMarker(path.ex, path.ey, path.edx, path.edy)) +
      "</g>";
    labelsOut += edgeLabelSvg(edge.id, label, labelX, labelY, "");
  }

  std::string svg = svgDocument(
    "requirement",
    width,
    height,
    size,
    options,
    model,
    labelSvg(title, width / 2.0, pad + title.height / 2.0, " class=\"pele-title\" font-weight=\"var(--_tw)\"") +
      (!edgesOut.empty() ? ("<g class=\"pele-edges\" fill=\"none\" stroke=\"var(--_l)\" stroke-linecap=\"round\">" + edgesOut + "</g>") : "") +
      (!labelsOut.empty() ? ("<g class=\"pele-edge-labels\" font-size=\"" + num(small) + "\" fill=\"var(--_m)\">" + labelsOut + "</g>") : "") +
      "<g class=\"pele-nodes\">" + nodesOut + "</g>"
  );

  return Rendered{svg, width, height, {}};
}

inline bool reqRunsAcross(const std::string& direction) {
  return direction == "LR" || direction == "RL";
}

template <typename DrawFn>
inline Rendered reqTurnToFit(const RenderOptions& options, DrawFn&& drawFn) {
  Rendered across = drawFn(false);
  double available = options.maxWidth;
  if (!options.hasMaxWidth || !(available > 0.0) || across.width <= available) return across;
  if (available >= 640.0) return across;
  Rendered down = drawFn(true);
  return down.width < across.width ? down : across;
}

inline Rendered renderRequirementModel(const RequirementDb& model, const Config& config, const RenderOptions& options) {
  if (!reqRunsAcross(model.direction)) return draw(model, config, options, false);
  return reqTurnToFit(options, [&](bool down) {
    return draw(model, config, options, down);
  });
}

} // namespace requirement_detail

class RequirementDiagram {
public:
  static Rendered render(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options) {
    requirement_detail::RequirementDb db;
    if (!title.empty()) db.title = title;
    requirement_detail::parseRequirement(source, db);
    return requirement_detail::renderRequirementModel(db, config, options);
  }
};

inline Rendered renderRequirement(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options) {
  return RequirementDiagram::render(source, config, title, options);
}

inline Rendered renderRequirement(const std::string& raw, RenderOptions options = {}) {
  auto pre = preprocess(raw);
  std::string parsed = encodeEntities(pre.text) + "\n";
  return RequirementDiagram::render(parsed, pre.config, pre.title, options);
}

} // namespace Pele
