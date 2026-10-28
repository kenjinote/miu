#pragma once

#include "../foundation.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Pele {
namespace QuadrantDiagram {

struct QuadrantText {
  std::string text;
  std::string type = "text"; // "text" | "markdown"
};

struct PointStyle {
  std::optional<double> radius;
  std::optional<std::string> color;
  std::optional<std::string> strokeColor;
  std::optional<std::string> strokeWidth;
};

struct QuadrantPoint {
  double x = 0.0;
  double y = 0.0;
  QuadrantText text;
  std::string className;
  PointStyle style;
};

struct QuadrantModel : DiagramMeta {
  std::optional<QuadrantText> quadrants[4];
  std::optional<QuadrantText> xAxisLeft;
  std::optional<QuadrantText> xAxisRight;
  std::optional<QuadrantText> yAxisBottom;
  std::optional<QuadrantText> yAxisTop;
  std::vector<QuadrantPoint> points;
  std::unordered_map<std::string, PointStyle> classes;
};

enum Tok {
  T_END = 0,
  T_ERROR = 1,
  T_NEWLINE = 2,
  T_title = 3,
  T_title_value = 4,
  T_acc_title = 5,
  T_acc_title_value = 6,
  T_acc_descr = 7,
  T_acc_descr_value = 8,
  T_acc_descr_multiline_value = 9,
  T_X_AXIS = 10,
  T_Y_AXIS = 11,
  T_DELIMITER = 12,
  T_QUADRANT_1 = 13,
  T_QUADRANT_2 = 14,
  T_QUADRANT_3 = 15,
  T_QUADRANT_4 = 16,
  T_CLASSDEF = 17,
  T_MD_STR = 18,
  T_STR = 19,
  T_class_name = 20,
  T_point_start = 21,
  T_point_x = 22,
  T_point_y = 23,
  T_QUADRANT = 24,
  T_ALPHA = 25,
  T_UNICODE_TEXT = 26,
  T_COLON = 27,
  T_PLUS = 28,
  T_COMMA = 29,
  T_EQUALS = 30,
  T_MULT = 31,
  T_BRKT = 32,
  T_UNDERSCORE = 33,
  T_DOT = 34,
  T_AMP = 35,
  T_MINUS = 36,
  T_NUM = 37,
  T_SPACE = 38,
  T_SEMI = 39,
  T_PUNCTUATION = 40,
  T_EOF = 41
};

struct Tokens {
  std::vector<int> types;
  std::vector<std::string> texts;
};

inline bool wordAtU16(const std::vector<uint16_t>& src, int p, const std::string& word) noexcept {
  int n = static_cast<int>(src.size());
  int wl = static_cast<int>(word.size());
  if (p < 0 || p + wl > n) return false;
  for (int i = 0; i < wl; ++i) {
    uint16_t c1 = src[p + i];
    uint16_t c2 = static_cast<uint16_t>(word[i]);
    if (c1 >= 'A' && c1 <= 'Z') c1 = static_cast<uint16_t>(c1 + 32);
    if (c2 >= 'A' && c2 <= 'Z') c2 = static_cast<uint16_t>(c2 + 32);
    if (c1 != c2) return false;
  }
  return true;
}

inline int singleTok(uint16_t c) noexcept {
  if (c >= 128) return 0;
  switch (c) {
    case ':': return T_COLON;
    case '+': return T_PLUS;
    case ',': return T_COMMA;
    case '=': return T_EQUALS;
    case '*': return T_MULT;
    case '#': return T_BRKT;
    case '_': return T_UNDERSCORE;
    case '.': return T_DOT;
    case '&': return T_AMP;
    case '-': return T_MINUS;
    case ' ':
    case '\t':
    case '\v':
    case '\f': return T_SPACE;
    case ';': return T_SEMI;
    case '!':
    case '$':
    case '%':
    case '\'':
    case '`':
    case '?':
    case '\\':
    case '/': return T_PUNCTUATION;
    default: return 0;
  }
}

inline Tokens tokenizeQuadrant(const std::string& rawSrc) {
  std::vector<uint16_t> src = toUtf16(rawSrc);
  int n = static_cast<int>(src.size());
  Tokens out;
  std::vector<int> stack;
  int state = 0;
  static const int AT_END[6] = {T_EOF, T_title_value, T_acc_title_value, T_acc_descr_value, T_acc_descr_multiline_value, T_STR};

  int p = 0;
  int spaceFrom = 0, spaceTo = 0;
  int dashFrom = 0, dashTo = 0;
  int wsFrom = 0, wsTo = 0;
  int gapFrom = 0, gapTo = 0;

  auto charAt = [&](int idx) -> uint32_t {
    return (idx >= 0 && idx < n) ? src[idx] : 0;
  };
  auto emit = [&](int type, int s, int e) {
    out.types.push_back(type);
    out.texts.push_back(fromUtf16(src.data() + s, static_cast<size_t>(e - s)));
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
  auto lineEnd = [&](int q) -> int {
    for (int i = q; i < n; ++i) {
      if (src[i] == 10) return i;
    }
    return n;
  };
  auto spaces = [&](int q) -> int {
    while (q < n && src[q] == 32) q++;
    return q;
  };
  auto run = [&](int q) -> int {
    while (q < n && isSpace(src[q])) q++;
    return q;
  };
  auto coordinate = [&](int q) -> int {
    uint32_t c = charAt(q);
    if (c == 49) return q + 1;
    if (c != 48) return -1;
    int e = q + 1;
    if (e < n && !isLineEnd(charAt(e)) && isDigit(charAt(e + 1))) {
      e += 2;
      while (e < n && isDigit(charAt(e))) e++;
    }
    return e;
  };

  while (p < n) {
    uint32_t c = charAt(p);
    uint32_t next = charAt(p + 1);

    if (state == 1 || state == 2 || state == 3) {
      emit(AT_END[state], p, lineEnd(p));
      pop();
      continue;
    }
    if (state == 4) {
      if (c == 125) {
        p++;
        pop();
      } else {
        int e = p;
        while (e < n && src[e] != 125) e++;
        emit(T_acc_descr_multiline_value, p, e);
      }
      continue;
    }
    if (state == 5) {
      if (c == 34) {
        p++;
        pop();
      } else {
        int e = p;
        while (e < n && src[e] != 34) e++;
        emit(T_STR, p, e);
      }
      continue;
    }
    if (state == 6) {
      if (c == 96 && next == 34) {
        p += 2;
        pop();
        continue;
      }
      if (c != 96 && c != 34) {
        int e = p + 1;
        while (e < n && src[e] != 96 && src[e] != 34) e++;
        emit(T_MD_STR, p, e);
        continue;
      }
      break;
    }
    if (state == 7) {
      if (isWord(c)) {
        int e = p + 1;
        while (e < n && isWord(charAt(e))) e++;
        emit(T_class_name, p, e);
        pop();
        continue;
      }
      break;
    }
    if (state == 8) {
      int e = coordinate(p);
      if (e != -1) {
        emit(T_point_x, p, e);
        push(9);
        continue;
      }
      int k = run(p);
      if (charAt(k) == 93) {
        p = spaces(k + 1);
        pop();
        continue;
      }
      break;
    }
    if (state == 9) {
      int k = run(p);
      if (charAt(k) == 44) {
        p = run(k + 1);
        state = 10;
        continue;
      }
      break;
    }
    if (state == 10) {
      int e = coordinate(p);
      if (e != -1) {
        emit(T_point_y, p, e);
        pop();
        continue;
      }
      break;
    }

    if (c == 37 && next == 37) {
      p = lineEnd(p + 2);
      continue;
    }
    if (c == 10 || c == 13) {
      int e = p + 1;
      while (e < n && (src[e] == 10 || src[e] == 13)) e++;
      emit(T_NEWLINE, p, e);
      continue;
    }

    uint32_t lower = c | 32;
    if (lower == 116 && wordAtU16(src, p, "title") && !isWord(charAt(p + 5))) {
      emit(T_title, p, p + 5);
      push(1);
      continue;
    }
    if (lower == 97 && wordAtU16(src, p, "acc")) {
      bool descr = wordAtU16(src, p + 3, "descr");
      if (descr || wordAtU16(src, p + 3, "title")) {
        int k = run(p + 8);
        uint32_t d = charAt(k);
        if (d == 58) {
          emit(descr ? T_acc_descr : T_acc_title, p, run(k + 1));
          push(descr ? 3 : 2);
          continue;
        }
        if (d == 123 && descr) {
          p = run(k + 1);
          push(4);
          continue;
        }
      }
    }

    int j = p;
    if (c == 32) {
      if (p < spaceFrom || p >= spaceTo) {
        spaceFrom = p;
        spaceTo = spaces(p);
      }
      j = spaceTo;
    }
    uint32_t d = charAt(j);
    uint32_t dl = d | 32;
    int type = 0;
    int end = 0;
    if (dl == 120 && wordAtU16(src, j, "x-axis")) {
      type = T_X_AXIS;
      end = j + 6;
    } else if (dl == 121 && wordAtU16(src, j, "y-axis")) {
      type = T_Y_AXIS;
      end = j + 6;
    } else if (d == 45) {
      if (j < dashFrom || j >= dashTo) {
        dashFrom = j;
        dashTo = j;
        while (dashTo < n && src[dashTo] == 45) dashTo++;
      }
      if (dashTo - j >= 2 && charAt(dashTo) == 62) {
        type = T_DELIMITER;
        end = dashTo + 1;
      }
    } else if (dl == 113 && wordAtU16(src, j, "quadrant")) {
      uint32_t q = charAt(j + 9);
      if (charAt(j + 8) == 45 && q >= 49 && q <= 52) {
        type = T_QUADRANT_1 + static_cast<int>(q - 49);
        end = j + 10;
      } else if (wordAtU16(src, j + 8, "chart")) {
        type = T_QUADRANT;
        end = j + 13;
      }
    }
    if (type != 0) {
      emit(type, p, spaces(end));
      continue;
    }

    if (lower == 99 && wordAtU16(src, p, "classdef") && !isWord(charAt(p + 8))) {
      emit(T_CLASSDEF, p, p + 8);
      continue;
    }
    if (c == 34) {
      if (next == 96) {
        p += 2;
        push(6);
      } else {
        p++;
        push(5);
      }
      continue;
    }
    if (c == 58 && next == 58 && charAt(p + 2) == 58) {
      p += 3;
      push(7);
      continue;
    }
    if (c == 58 || isSpace(c)) {
      int k = p;
      if (c != 58) {
        if (p < wsFrom || p >= wsTo) {
          wsFrom = p;
          wsTo = run(p);
        }
        k = wsTo;
      }
      if (charAt(k) == 58) {
        if (k + 1 < gapFrom || k + 1 >= gapTo) {
          gapFrom = k + 1;
          gapTo = run(k + 1);
        }
        if (charAt(gapTo) == 91) {
          emit(T_point_start, p, run(gapTo + 1));
          push(8);
          continue;
        }
      }
    }

    if (isLetter(c)) {
      int e = p + 1;
      while (e < n && isLetter(charAt(e))) e++;
      emit(T_ALPHA, p, e);
      continue;
    }
    if (c >= 128) {
      int e = p + 1;
      while (e < n && charAt(e) >= 128) e++;
      emit(T_UNICODE_TEXT, p, e);
      continue;
    }
    if (isDigit(c)) {
      int e = p + 1;
      while (e < n && isDigit(charAt(e))) e++;
      emit(T_NUM, p, e);
      continue;
    }
    int st = singleTok(static_cast<uint16_t>(c));
    if (st != 0) {
      emit(st, p, p + 1);
      continue;
    }
    break;
  }

  if (p < n) {
    out.types.push_back(T_ERROR);
    out.texts.push_back("");
    return out;
  }
  if (state < 6) {
    out.types.push_back(AT_END[state]);
    out.texts.push_back("");
  }
  out.types.push_back(T_END);
  out.texts.push_back("");
  return out;
}

inline std::pair<std::string, std::string> keyValue(const std::string& style) noexcept {
  std::string s = trimStr(style);
  size_t colon = s.find(':');
  if (colon == std::string::npos) return {s, "undefined"};
  std::string rest = trimStartStr(s.substr(colon + 1));
  size_t next = rest.find(':');
  std::string k = trimEndStr(s.substr(0, colon));
  std::string v = (next == std::string::npos) ? rest : trimEndStr(rest.substr(0, next));
  return {k, v};
}

inline PointStyle parseStyles(const std::vector<std::string>& styles) {
  PointStyle out;
  for (const auto& style : styles) {
    auto kv = keyValue(style);
    const std::string& key = kv.first;
    const std::string& val = kv.second;
    if (key == "radius") {
      out.radius = static_cast<double>(std::atoi(val.c_str()));
    } else if (key == "color") {
      out.color = val;
    } else if (key == "stroke-color") {
      out.strokeColor = val;
    } else if (key == "stroke-width") {
      out.strokeWidth = val;
    }
  }
  return out;
}

inline QuadrantText trimmedText(const QuadrantText& label) noexcept {
  return {trimStr(label.text), label.type};
}

inline bool isWordTok(int t) noexcept {
  return t == T_PUNCTUATION || t == T_AMP || t == T_NUM || t == T_ALPHA ||
         t == T_COMMA || t == T_PLUS || t == T_EQUALS || t == T_MULT ||
         t == T_DOT || t == T_BRKT || t == T_UNDERSCORE || t == T_UNICODE_TEXT;
}

inline bool isIdTok(int t) noexcept {
  return t == T_ALPHA || t == T_NUM || t == T_MINUS || t == T_COMMA ||
         t == T_COLON || t == T_AMP || t == T_BRKT || t == T_MULT || t == T_UNICODE_TEXT;
}

inline bool isStyleTok(int t) noexcept {
  return t == T_ALPHA || t == T_NUM || t == T_COLON || t == T_SPACE || t == T_BRKT || t == T_MINUS;
}

inline bool isEolTok(int t) noexcept {
  return t == T_NEWLINE || t == T_SEMI || t == T_EOF;
}

inline bool isTextTok(int t) noexcept {
  return t == T_STR || t == T_MD_STR || isWordTok(t);
}

inline void parseQuadrant(const std::string& src, QuadrantModel& db) {
  Tokens toks = tokenizeQuadrant(src);
  size_t i = 0;
  auto curType = [&]() -> int {
    return i < toks.types.size() ? toks.types[i] : T_END;
  };
  auto expect = [&](int t) -> std::string {
    if (curType() == t) return toks.texts[i++];
    return "";
  };
  auto readText = [&]() -> QuadrantText {
    int first = curType();
    if (!isTextTok(first)) return {"", "text"};
    std::string s = toks.texts[i++];
    while (isWordTok(curType()) || curType() == T_SPACE || curType() == T_MINUS) {
      s += toks.texts[i++];
    }
    return {s, first == T_MD_STR ? "markdown" : "text"};
  };
  auto readStyles = [&]() -> std::vector<std::string> {
    std::vector<std::string> out;
    for (;;) {
      if (!isStyleTok(curType())) return out;
      std::string s = toks.texts[i++];
      while (isStyleTok(curType())) s += toks.texts[i++];
      out.push_back(trimStr(s));
      if (curType() != T_COMMA) return out;
      i++;
    }
  };

  while (isEolTok(curType()) || curType() == T_SPACE) i++;
  expect(T_QUADRANT);

  while (curType() != T_END) {
    while (curType() == T_SPACE) i++;
    int t = curType();
    if (t == T_CLASSDEF) {
      i++;
      expect(T_SPACE);
      std::string id;
      while (isIdTok(curType())) id += toks.texts[i++];
      expect(T_SPACE);
      db.classes[id] = parseStyles(readStyles());
    } else if (t == T_X_AXIS || t == T_Y_AXIS) {
      i++;
      QuadrantText from = readText();
      std::optional<QuadrantText> to;
      if (curType() == T_DELIMITER) {
        i++;
        if (isTextTok(curType())) to = readText();
        else from.text += " \xE2\x9F\xB6 ";
      }
      if (t == T_X_AXIS) {
        db.xAxisLeft = trimmedText(from);
        if (to.has_value()) db.xAxisRight = trimmedText(*to);
      } else {
        db.yAxisBottom = trimmedText(from);
        if (to.has_value()) db.yAxisTop = trimmedText(*to);
      }
    } else if (t >= T_QUADRANT_1 && t <= T_QUADRANT_4) {
      i++;
      db.quadrants[t - T_QUADRANT_1] = trimmedText(readText());
    } else if (t == T_title) {
      i++;
      db.title = trimStr(expect(T_title_value));
    } else if (t == T_acc_title) {
      i++;
      db.accTitle = trimStartStr(trimStr(expect(T_acc_title_value)));
    } else if (t == T_acc_descr) {
      i++;
      std::string d = trimStr(expect(T_acc_descr_value));
      db.accDescr = d;
    } else if (t == T_acc_descr_multiline_value) {
      std::string raw = trimStr(toks.texts[i++]);
      std::string res;
      for (size_t k = 0; k < raw.size(); ++k) {
        res.push_back(raw[k]);
        if (raw[k] == '\n') {
          while (k + 1 < raw.size() && (raw[k + 1] == ' ' || raw[k + 1] == '\t' || raw[k + 1] == '\r')) k++;
        }
      }
      db.accDescr = res;
    } else if (isTextTok(t)) {
      QuadrantText label = readText();
      std::string className = (curType() == T_class_name) ? toks.texts[i++] : "";
      expect(T_point_start);
      std::string xStr = expect(T_point_x);
      std::string yStr = expect(T_point_y);
      std::vector<std::string> st = isStyleTok(curType()) ? readStyles() : std::vector<std::string>{};
      db.points.push_back({
        std::strtod(xStr.c_str(), nullptr),
        std::strtod(yStr.c_str(), nullptr),
        trimmedText(label),
        className,
        parseStyles(st)
      });
    }
    if (isEolTok(curType())) {
      i++;
    } else {
      break;
    }
  }
}

inline const Config* setting(const Config& config, const std::string& key) noexcept {
  if (!config.isMap()) return nullptr;
  const Config* qc = config.get("quadrantChart");
  if (!qc || !qc->isMap()) return nullptr;
  return qc->get(key);
}

inline double lengthOpt(const Config& config, const std::string& key, double fallback, double maxVal) noexcept {
  const Config* v = setting(config, key);
  if (v && v->isNumber() && v->nVal >= 0.0) return std::min(v->nVal, maxVal);
  return fallback;
}

inline std::string hexColor(const std::string& color) noexcept {
  return (!color.empty() && color[0] == '#') ? color : Pele::cat({"#", color});
}

inline double capWidth(const RenderOptions& options, double natural, double least) noexcept {
  double max = options.maxWidth;
  return (max > 0.0 && std::isfinite(max)) ? std::min(natural, std::max(least, max)) : natural;
}

inline Rendered renderQuadrantModel(const QuadrantModel& model, const Config& config, const RenderOptions& options) noexcept {
  constexpr double GAP = 4.0;
  constexpr size_t CAREFUL = 300;

  double size = options.hasFontSize ? options.fontSize : 16.0;
  double axisSize = std::round(size * 0.875);
  double pointSize = std::round(size * 0.75);
  const TextMeasurer& measurer = getMeasurer(options);
  double natural = std::round(std::max(100.0, lengthOpt(config, "chartWidth", 500.0, 4000.0)));
  double width = std::round(capWidth(options, natural, 280.0));
  double height = std::round((std::max(100.0, lengthOpt(config, "chartHeight", 500.0, 4000.0)) * width) / natural);
  double outer = lengthOpt(config, "quadrantPadding", options.hasPadding ? options.padding : 8.0, 200.0);
  double xPad = lengthOpt(config, "xAxisLabelPadding", 5.0, 200.0);
  double yPad = lengthOpt(config, "yAxisLabelPadding", 5.0, 200.0);
  bool hasPoints = !model.points.empty();

  auto makeLabel = [&](const std::optional<QuadrantText>& text, double fontSize, double max) -> Label {
    if (!text.has_value()) return layoutLabel("", false, measurer, fontSize, std::max(max, 40.0));
    return layoutLabel(text->text, text->type == "markdown", measurer, fontSize, std::max(max, 40.0));
  };

  Label title = layoutLabel(model.title, false, measurer, size, std::max(width - 2.0 * outer, 40.0), Style::Bold);
  double titlePad = lengthOpt(config, "titlePadding", 10.0, 200.0);
  double titleSpace = title.height > 0.0 ? title.height + titlePad : 0.0;

  bool hasX = (model.xAxisLeft.has_value() && !model.xAxisLeft->text.empty()) ||
              (model.xAxisRight.has_value() && !model.xAxisRight->text.empty());
  bool hasY = (model.yAxisBottom.has_value() && !model.yAxisBottom->text.empty()) ||
              (model.yAxisTop.has_value() && !model.yAxisTop->text.empty());
  const Config* xPos = setting(config, "xAxisPosition");
  const Config* yPos = setting(config, "yAxisPosition");
  bool xBelow = hasPoints || (xPos && xPos->isString() && xPos->sVal == "bottom");
  bool yRight = yPos && yPos->isString() && yPos->sVal == "right";

  double side = width - 2.0 * outer;
  Label xLeft = makeLabel(model.xAxisLeft, axisSize, model.xAxisRight.has_value() ? (side / 2.0 - 16.0) : side);
  Label xRight = makeLabel(model.xAxisRight, axisSize, side / 2.0 - 16.0);
  double xHeight = std::max(xLeft.height, xRight.height);
  double xSpace = hasX ? (xHeight + xPad) : 0.0;
  double tall = height - 2.0 * outer - titleSpace - xSpace;
  Label yBottom = makeLabel(model.yAxisBottom, axisSize, model.yAxisTop.has_value() ? (tall / 2.0 - 16.0) : tall);
  Label yTop = makeLabel(model.yAxisTop, axisSize, tall / 2.0 - 16.0);
  double yHeight = std::max(yBottom.height, yTop.height);
  double ySpace = hasY ? (yHeight + yPad) : 0.0;

  double left = outer + (yRight ? 0.0 : ySpace);
  double top = outer + titleSpace + (xBelow ? 0.0 : xSpace);
  double w = std::max(0.0, side - ySpace);
  double h = std::max(0.0, tall);
  double tileW = std::max(0.0, (w - GAP) / 2.0);
  double tileH = std::max(0.0, (h - GAP) / 2.0);

  std::vector<double> taken = {
    left + tileW, top, left + w - tileW, top + h,
    left, top + tileH, left + w, top + h - tileH
  };
  auto isClear = [&](size_t from, double x0, double y0, double x1, double y1) -> bool {
    for (size_t k = from; k < taken.size(); k += 4) {
      if (taken[k] >= taken[k + 2] || taken[k + 1] >= taken[k + 3]) continue;
      if (x0 < taken[k + 2] && x1 > taken[k] && y0 < taken[k + 3] && y1 > taken[k + 1]) return false;
    }
    return true;
  };

  double textTop = lengthOpt(config, "quadrantTextTopPadding", 5.0, 200.0);
  std::string quadrantsOut;
  for (int k = 0; k < 4; ++k) {
    double x = (k == 0 || k == 3) ? (left + w - tileW) : left;
    double y = (k < 2) ? top : (top + h - tileH);
    Label text = makeLabel(model.quadrants[k], size, tileW - 16.0);
    double cx = x + tileW / 2.0;
    double cy = hasPoints ? (y + textTop + text.height / 2.0) : (y + tileH / 2.0);
    taken.push_back(cx - text.width / 2.0);
    taken.push_back(cy - text.height / 2.0);
    taken.push_back(cx + text.width / 2.0);
    taken.push_back(cy + text.height / 2.0);
    quadrantsOut +=
      "<g class=\"pele-quadrant\" data-id=\"" + std::to_string(k + 1) + "\">" +
      "<rect x=\"" + num(x) + "\" y=\"" + num(y) + "\" width=\"" + num(tileW) + "\" height=\"" + num(tileH) + "\"" + ROUNDED + "/>" +
      labelSvg(text, cx, cy, " class=\"pele-label\" fill=\"var(--_fg)\" stroke=\"none\"") +
      "</g>";
  }

  std::string axesOut;
  if (hasX) {
    double y = xBelow ? (top + h + xPad + xHeight / 2.0) : (top - xPad - xHeight / 2.0);
    axesOut +=
      labelSvg(xLeft, xRight.height > 0.0 ? (left + w / 4.0) : (left + xLeft.width / 2.0), y, "") +
      labelSvg(xRight, left + (w * 3.0) / 4.0, y, "");
  }
  if (hasY) {
    double x = yRight ? (left + w + yPad + yHeight / 2.0) : (left - yPad - yHeight / 2.0);
    auto turned = [&](const Label& text, double y) -> std::string {
      return text.height > 0.0 ? Pele::cat({"<g transform=\"translate(", num(x), ",", num(y), ") rotate(-90)\">", labelSvg(text, 0.0, 0.0, ""), "</g>"}) : "";
    };
    axesOut +=
      turned(yBottom, yTop.height > 0.0 ? (top + (h * 3.0) / 4.0) : (top + h - yBottom.width / 2.0)) +
      turned(yTop, top + h / 4.0);
  }

  double radius = lengthOpt(config, "pointRadius", 5.0, 200.0);
  double textPad = lengthOpt(config, "pointTextPadding", 5.0, 200.0);
  std::string fill = seriesColor(0);

  struct Dot {
    double cx = 0.0;
    double cy = 0.0;
    double reach = 0.0;
    Label text;
  };
  std::vector<Dot> dots;
  std::vector<std::string> circles;

  for (const auto& point : model.points) {
    if (!(point.x >= 0.0 && point.x <= 1.0 && point.y >= 0.0 && point.y <= 1.0)) continue;
    const PointStyle* shared = nullptr;
    auto cit = model.classes.find(point.className);
    if (cit != model.classes.end()) shared = &cit->second;

    double rVal = radius;
    if (point.style.radius.has_value()) rVal = *point.style.radius;
    else if (shared && shared->radius.has_value()) rVal = *shared->radius;
    double r = std::min(rVal, 2000.0);

    std::optional<std::string> color = point.style.color.has_value() ? point.style.color : (shared ? shared->color : std::nullopt);
    std::optional<std::string> strokeColor = point.style.strokeColor.has_value() ? point.style.strokeColor : (shared ? shared->strokeColor : std::nullopt);
    std::optional<std::string> strokeWidth = point.style.strokeWidth.has_value() ? point.style.strokeWidth : (shared ? shared->strokeWidth : std::nullopt);

    std::vector<std::string> styles;
    if (color.has_value()) styles.push_back(Pele::cat({"fill:", hexColor(*color)}));
    if (strokeWidth.has_value()) {
      if (strokeColor.has_value()) styles.push_back(Pele::cat({"stroke:", hexColor(*strokeColor)}));
      styles.push_back("stroke-width:" + *strokeWidth);
    }
    double cx = left + point.x * w;
    double cy = top + (1.0 - point.y) * h;
    double swReach = 0.0;
    if (strokeWidth.has_value()) {
      swReach = std::min(static_cast<double>(std::atoi(strokeWidth->c_str())), 400.0) / 2.0;
    }
    dots.push_back({
      cx,
      cy,
      r + swReach,
      makeLabel(point.text, pointSize, 200.0)
    });
    circles.push_back(Pele::cat({"<circle class=\"pele-point", classNames(point.className), "\" data-id=\"", escText(point.text.text), "\" cx=\"", num(cx), "\" cy=\"", num(cy), "\" r=\"", num(r), "\"", ((strokeWidth.has_value() && !strokeColor.has_value()) ? Pele::cat({" stroke=\"", fill, "\""}) : ""), resolveStyle(styles).shape, "/>"}));
  }

  bool careful = dots.size() <= CAREFUL;
  if (careful) {
    for (const auto& d : dots) {
      taken.push_back(d.cx - d.reach);
      taken.push_back(d.cy - d.reach);
      taken.push_back(d.cx + d.reach);
      taken.push_back(d.cy + d.reach);
    }
  }

  std::string labelsOut;
  for (const auto& d : dots) {
    const auto& text = d.text;
    double halfW = text.width / 2.0;
    double halfH = text.size * 0.6 + (text.height - text.lineHeight) / 2.0;
    double away = d.reach + textPad;
    double inset = std::min(away, std::max(0.0, (tileW - text.width) / 2.0));
    size_t qIdx = (d.cx < left + w / 2.0) ? ((d.cy < top + h / 2.0) ? 1 : 2) : ((d.cy < top + h / 2.0) ? 0 : 3);
    size_t heading = 8 + 4 * qIdx;
    double below = std::max(d.cy + away, taken[heading + 3] + textPad) + halfH;
    double spots[10] = {
      d.cx, d.cy + away + halfH,
      d.cx, d.cy - away - halfH,
      d.cx + away + halfW, d.cy,
      d.cx - away - halfW, d.cy,
      d.cx, below
    };
    double tx = d.cx;
    double ty = spots[1];
    bool found = false;
    for (int k = 0; k < 20; k += 2) {
      int spot = k % 10;
      double margin = (spot == 8) ? inset : 0.0;
      double x = ((spot < 4 || spot == 8) && text.width < w)
        ? std::min(std::max(spots[spot], left + halfW + margin), left + w - halfW - margin)
        : spots[spot];
      double y = spots[spot + 1];
      if (x - halfW < left || x + halfW > left + w || y - halfH < top || y + halfH > top + h) continue;
      bool free = !careful || isClear(k < 10 ? 0 : 8, x - halfW, y - halfH, x + halfW, y + halfH);
      if (!found || free) {
        tx = x;
        ty = y;
        found = true;
      }
      if (free) break;
    }
    if (careful) {
      taken.push_back(tx - halfW);
      taken.push_back(ty - halfH);
      taken.push_back(tx + halfW);
      taken.push_back(ty + halfH);
    }
    labelsOut += labelSvg(text, tx, ty, " class=\"pele-label\"");
  }

  std::string circlesJoined;
  for (auto it = circles.rbegin(); it != circles.rend(); ++it) {
    circlesJoined += *it;
  }

  std::string content = Pele::cat({labelSvg(title, width / 2.0, titlePad / 2.0 + title.height / 2.0, " class=\"pele-title\" font-weight=\"var(--_tw)\""), "<g class=\"pele-quadrants\" fill=\"var(--_s)\" stroke=\"var(--_b)\">", quadrantsOut, "</g>", (!axesOut.empty() ? Pele::cat({"<g class=\"pele-axis-labels\" font-size=\"", num(axisSize), "\" fill=\"var(--_m)\">", axesOut, "</g>"}) : ""), (!dots.empty()
      ? Pele::cat({"<g class=\"pele-points\" fill=\"", fill, "\">", circlesJoined, "</g>", "<g class=\"pele-point-labels\" font-size=\"", num(pointSize), "\">", labelsOut, "</g>"})
      : "")});

  return {
    svgDocument("quadrantChart", width, height, size, options, model, content),
    width,
    height,
    {}
  };
}

inline Rendered render(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options = {}) {
  QuadrantModel db;
  if (!title.empty()) db.title = title;
  parseQuadrant(source, db);
  return renderQuadrantModel(db, config, options);
}

} // namespace QuadrantDiagram

inline Rendered renderQuadrant(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options = {}) {
  return QuadrantDiagram::render(source, config, title, options);
}

} // namespace Pele
