#pragma once
#include "../layout.h"
#include "../common_parse.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Pele {

namespace gantt_detail {

// ============================================================================
// Proleptic Gregorian Date in Local Time (`src/diagrams/gantt/dates.ts`)
// ============================================================================

inline int localTzOffsetMinutes() noexcept {
  static const int offset = []() {
    std::time_t sample = 1704067200; // 2024-01-01 00:00:00 UTC
    std::tm utcTm{};
    std::tm locTm{};
#if defined(_WIN32)
    gmtime_s(&utcTm, &sample);
    localtime_s(&locTm, &sample);
#else
    gmtime_r(&sample, &utcTm);
    localtime_r(&sample, &locTm);
#endif
    auto toDays = [](int y, int m, int d) -> long long {
      y -= m <= 2;
      const long long era = (y >= 0 ? y : y - 399) / 400;
      const unsigned yoe = static_cast<unsigned>(y - era * 400);
      const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
      const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
      return era * 146097 + static_cast<long long>(doe) - 719468;
    };
    long long uDays = toDays(utcTm.tm_year + 1900, utcTm.tm_mon + 1, utcTm.tm_mday);
    long long lDays = toDays(locTm.tm_year + 1900, locTm.tm_mon + 1, locTm.tm_mday);
    long long uMin = uDays * 1440 + utcTm.tm_hour * 60 + utcTm.tm_min;
    long long lMin = lDays * 1440 + locTm.tm_hour * 60 + locTm.tm_min;
    // JS Date.getTimezoneOffset() returns (UTC - Local) in minutes
    return static_cast<int>(uMin - lMin);
  }();
  return offset;
}

inline long long daysFromCivil(long long y, long long m, long long d) noexcept {
  y -= m <= 2;
  const long long era = (y >= 0 ? y : y - 399) / 400;
  const long long yoe = y - era * 400;
  const long long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const long long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

struct CivilDate {
  int year = 1970;
  int month0 = 0; // 0..11
  int day = 1;    // 1..31
  int weekday = 4; // 0=Sun..6=Sat
  int hours = 0;
  int minutes = 0;
  int seconds = 0;
  int ms = 0;
};

inline CivilDate decomposeMs(double utcMs, int tzOffsetMin) noexcept {
  double localMs = std::floor(utcMs - static_cast<double>(tzOffsetMin) * 60000.0);
  double daysD = std::floor(localMs / 86400000.0);
  long long z = static_cast<long long>(daysD);
  long long remMs = static_cast<long long>(localMs - daysD * 86400000.0);
  if (remMs < 0) {
    remMs += 86400000LL;
    z -= 1;
  }
  int wd = static_cast<int>(((z + 4) % 7 + 7) % 7);
  z += 719468;
  const long long era = (z >= 0 ? z : z - 146096) / 146097;
  const long long doe = z - era * 146097;
  const long long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  long long y = yoe + era * 400;
  const long long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const long long mp = (5 * doy + 2) / 153;
  const long long d = doy - (153 * mp + 2) / 5 + 1;
  const long long m = mp + (mp < 10 ? 3 : -9);
  y += (m <= 2);

  CivilDate c;
  c.year = static_cast<int>(y);
  c.month0 = static_cast<int>(m - 1);
  c.day = static_cast<int>(d);
  c.weekday = wd;
  c.hours = static_cast<int>(remMs / 3600000LL);
  remMs %= 3600000LL;
  c.minutes = static_cast<int>(remMs / 60000LL);
  remMs %= 60000LL;
  c.seconds = static_cast<int>(remMs / 1000LL);
  c.ms = static_cast<int>(remMs % 1000LL);
  return c;
}

inline double composeMs(long long year, long long month0, long long day, long long hours, long long minutes, long long seconds, double ms, int tzOffsetMin) noexcept {
  long long ym = year + (month0 >= 0 ? month0 / 12 : (month0 - 11) / 12);
  long long mn = ((month0 % 12) + 12) % 12;
  long long epochDays = daysFromCivil(ym, mn + 1, 1) + (day - 1);
  double localMs = (static_cast<double>(epochDays) * 86400.0 +
                    static_cast<double>(hours) * 3600.0 +
                    static_cast<double>(minutes) * 60.0 +
                    static_cast<double>(seconds)) * 1000.0 + std::trunc(ms);
  return localMs + static_cast<double>(tzOffsetMin) * 60000.0;
}

struct GanttDate {
  double utcMs = std::numeric_limits<double>::quiet_NaN();

  GanttDate() = default;
  explicit GanttDate(double ms) : utcMs(std::isfinite(ms) ? std::trunc(ms) : std::numeric_limits<double>::quiet_NaN()) {}

  static GanttDate invalid() { return GanttDate(); }

  static GanttDate fromLocal(int year, int month0, int day = 1, int hours = 0, int minutes = 0, int seconds = 0, double ms = 0.0, bool mapTwoDigitYear = true) {
    int y = (mapTwoDigitYear && year >= 0 && year <= 99) ? (1900 + year) : year;
    return GanttDate(composeMs(y, month0, day, hours, minutes, seconds, ms, localTzOffsetMinutes()));
  }

  static GanttDate fromUTC(int year, int month0, int day = 1, int hours = 0, int minutes = 0, int seconds = 0, double ms = 0.0, bool mapTwoDigitYear = true) {
    int y = (mapTwoDigitYear && year >= 0 && year <= 99) ? (1900 + year) : year;
    return GanttDate(composeMs(y, month0, day, hours, minutes, seconds, ms, 0));
  }

  bool isValid() const { return std::isfinite(utcMs) && std::abs(utcMs) <= 8.64e15; }
  double getTime() const { return isValid() ? utcMs : std::numeric_limits<double>::quiet_NaN(); }

  CivilDate local() const { return decomposeMs(utcMs, localTzOffsetMinutes()); }
  CivilDate utc() const { return decomposeMs(utcMs, 0); }

  int getFullYear() const { return local().year; }
  int getMonth() const { return local().month0; }
  int getDate() const { return local().day; }
  int getDay() const { return local().weekday; }
  int getHours() const { return local().hours; }
  int getMinutes() const { return local().minutes; }
  int getSeconds() const { return local().seconds; }
  int getMilliseconds() const { return local().ms; }
  int getUTCSeconds() const { return utc().seconds; }
  int getTimezoneOffset() const { return localTzOffsetMinutes(); }

  void setTime(double ms) {
    utcMs = (std::isfinite(ms) && std::abs(ms) <= 8.64e15) ? std::trunc(ms) : std::numeric_limits<double>::quiet_NaN();
  }

  void setHours(int h, int m, int s, int ms) {
    if (!isValid()) return;
    CivilDate c = local();
    setTime(composeMs(c.year, c.month0, c.day, h, m, s, ms, localTzOffsetMinutes()));
  }

  void setDate(int d) {
    if (!isValid()) return;
    CivilDate c = local();
    setTime(composeMs(c.year, c.month0, d, c.hours, c.minutes, c.seconds, c.ms, localTzOffsetMinutes()));
  }

  void setMonth(int m0) {
    if (!isValid()) return;
    CivilDate c = local();
    setTime(composeMs(c.year, m0, c.day, c.hours, c.minutes, c.seconds, c.ms, localTzOffsetMinutes()));
  }

  void setMonth(int m0, int d) {
    if (!isValid()) return;
    CivilDate c = local();
    setTime(composeMs(c.year, m0, d, c.hours, c.minutes, c.seconds, c.ms, localTzOffsetMinutes()));
  }

  void setFullYear(int y) {
    if (!isValid()) return;
    CivilDate c = local();
    setTime(composeMs(y, c.month0, c.day, c.hours, c.minutes, c.seconds, c.ms, localTzOffsetMinutes()));
  }
};

inline const std::vector<std::string>& MONTHS() noexcept {
  static const std::vector<std::string> m = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
  };
  return m;
}

inline const std::vector<std::string>& WEEKDAYS() noexcept {
  static const std::vector<std::string> w = {
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
  };
  return w;
}

constexpr double DAY_MS = 86400000.0;
constexpr double WEEK_MS = 7.0 * DAY_MS;

inline bool isValid(const GanttDate& date) noexcept {
  return date.isValid();
}

inline std::string padZero(int value, size_t length) noexcept {
  std::string s = std::to_string(value);
  return s.size() >= length ? s : std::string(length - s.size(), '0') + s;
}

inline std::string ordinal(int n) noexcept {
  static const char* s[4] = {"th", "st", "nd", "rd"};
  int v = n % 100;
  int idx1 = (v - 20) % 10;
  const char* suf = (idx1 >= 0 && idx1 < 4 && s[idx1]) ? s[idx1] : ((v >= 0 && v < 4 && s[v]) ? s[v] : s[0]);
  return std::to_string(n) + suf;
}

inline int utcOffset(const GanttDate& date) noexcept {
  int offset = -static_cast<int>(std::round(static_cast<double>(date.getTimezoneOffset()) / 15.0)) * 15;
  return offset == 0 ? 0 : offset;
}

inline std::string zone(const GanttDate& date) noexcept {
  int offset = utcOffset(date);
  int minutes = std::abs(offset);
  return std::string(offset >= 0 ? "+" : "-") + padZero(minutes / 60, 2) + ":" + padZero(minutes % 60, 2);
}

inline int isoWeekday(const GanttDate& date) noexcept {
  int d = date.getDay();
  return d == 0 ? 7 : d;
}

inline GanttDate addDays(const GanttDate& date, int days) noexcept {
  GanttDate next = date;
  next.setDate(next.getDate() + days);
  return next;
}

inline int daysInMonth(const GanttDate& date) noexcept {
  GanttDate last = GanttDate::fromLocal(date.getFullYear(), date.getMonth() + 1, 0, 0, 0, 0, 0, false);
  last.setHours(23, 59, 59, 999);
  return last.getDate();
}

template <typename ApplyFn>
inline GanttDate setCalendar(const GanttDate& date, ApplyFn apply) noexcept {
  GanttDate next = date;
  next.setDate(1);
  apply(next);
  next.setDate(std::min(date.getDate(), daysInMonth(next)));
  return next;
}

inline GanttDate add(const GanttDate& date, double amount, const std::string& unit) noexcept {
  if (unit == "M") {
    return setCalendar(date, [&](GanttDate& d) { d.setMonth(date.getMonth() + static_cast<int>(amount)); });
  }
  if (unit == "y") {
    return setCalendar(date, [&](GanttDate& d) { d.setFullYear(date.getFullYear() + static_cast<int>(amount)); });
  }
  if (unit == "d") {
    return addDays(date, static_cast<int>(std::round(amount)));
  }
  if (unit == "w") {
    return addDays(date, static_cast<int>(std::round(7.0 * amount)));
  }
  if (unit == "h") {
    return GanttDate(date.getTime() + amount * 3600000.0);
  }
  if (unit == "m") {
    return GanttDate(date.getTime() + amount * 60000.0);
  }
  if (unit == "s") {
    return GanttDate(date.getTime() + amount * 1000.0);
  }
  return GanttDate(date.getTime() + amount);
}

inline GanttDate weekThursday(const GanttDate& date) noexcept {
  return addDays(date, 4 - isoWeekday(date));
}

inline int isoWeek(const GanttDate& date) noexcept {
  GanttDate thursday = weekThursday(date);
  GanttDate jan1 = GanttDate::fromLocal(thursday.getFullYear(), 0, 1, 0, 0, 0, 0, false);
  int weekday = isoWeekday(jan1);
  GanttDate first = addDays(jan1, 4 - weekday + (weekday > 4 ? 7 : 0));
  double zoneDelta = static_cast<double>(utcOffset(first) - utcOffset(thursday)) * 60000.0;
  double weeks = (thursday.getTime() - first.getTime() - zoneDelta) / WEEK_MS;
  int w = static_cast<int>(weeks < 0.0 ? std::ceil(weeks) : std::floor(weeks));
  return w + 1;
}

inline std::string isoDate(const GanttDate& date) noexcept {
  int year = date.getFullYear();
  int month = date.getMonth() + 1;
  int day = date.getDate();
  std::string yStr = (year >= 1000 && year <= 9999) ? std::to_string(year) : padZero(year, 4);
  return yStr + (month < 10 ? "-0" : "-") + std::to_string(month) + (day < 10 ? "-0" : "-") + std::to_string(day);
}

inline std::optional<std::string> format(const GanttDate& date, const std::string& tmpl) noexcept {
  if (!isValid(date)) return "Invalid Date";
  std::string str = tmpl.empty() ? "YYYY-MM-DDTHH:mm:ssZ" : tmpl;
  if (str == "YYYY-MM-DD") return isoDate(date);

  static const std::vector<std::string> advTokens = {
    "GGGG", "gggg", "zzz", "WW", "ww", "wo", "Do", "kk", "Q", "W", "w", "z", "X", "x", "k", "S"
  };
  bool unsupported = false;
  std::string advanced;
  for (size_t i = 0; i < str.size(); ) {
    if (str[i] == '[') {
      size_t close = str.find(']', i + 1);
      if (close != std::string::npos && close > i + 1) {
        advanced += str.substr(i, close - i + 1);
        i = close + 1;
        continue;
      }
    }
    std::string matched;
    for (const auto& tok : advTokens) {
      if (str.compare(i, tok.size(), tok) == 0) {
        matched = tok;
        break;
      }
    }
    if (!matched.empty()) {
      i += matched.size();
      if (matched == "Q") advanced += std::to_string((date.getMonth() / 3) + 1);
      else if (matched == "Do") advanced += "[" + ordinal(date.getDate()) + "]";
      else if (matched == "GGGG") advanced += std::to_string(weekThursday(date).getFullYear());
      else if (matched == "W") advanced += std::to_string(isoWeek(date));
      else if (matched == "WW") advanced += padZero(isoWeek(date), 2);
      else if (matched == "k") advanced += std::to_string(date.getHours() ? date.getHours() : 24);
      else if (matched == "kk") advanced += padZero(date.getHours() ? date.getHours() : 24, 2);
      else if (matched == "X") advanced += std::to_string(static_cast<long long>(std::floor(date.getTime() / 1000.0)));
      else if (matched == "x") advanced += std::to_string(static_cast<long long>(date.getTime()));
      else if (matched == "S") advanced += "S";
      else {
        unsupported = true;
        break;
      }
    } else {
      advanced.push_back(str[i++]);
    }
  }
  if (unsupported) return std::nullopt;

  static const std::vector<std::string> fmtTokens = {
    "YYYY", "MMMM", "dddd", "MMM", "ddd", "SSS",
    "YY", "MM", "DD", "dd", "HH", "hh", "mm", "ss", "ZZ",
    "M", "D", "d", "H", "h", "a", "A", "m", "s", "Z"
  };
  int hours = date.getHours();
  std::string out;
  for (size_t i = 0; i < advanced.size(); ) {
    if (advanced[i] == '[') {
      size_t close = advanced.find(']', i + 1);
      if (close != std::string::npos && close > i + 1) {
        out += advanced.substr(i + 1, close - i - 1);
        i = close + 1;
        continue;
      }
    }
    std::string matched;
    for (const auto& tok : fmtTokens) {
      if (advanced.compare(i, tok.size(), tok) == 0) {
        matched = tok;
        break;
      }
    }
    if (!matched.empty()) {
      i += matched.size();
      if (matched == "YY") {
        std::string ys = std::to_string(date.getFullYear());
        out += ys.size() >= 2 ? ys.substr(ys.size() - 2) : ys;
      } else if (matched == "YYYY") out += padZero(date.getFullYear(), 4);
      else if (matched == "M") out += std::to_string(date.getMonth() + 1);
      else if (matched == "MM") out += padZero(date.getMonth() + 1, 2);
      else if (matched == "MMM") out += MONTHS()[date.getMonth()].substr(0, 3);
      else if (matched == "MMMM") out += MONTHS()[date.getMonth()];
      else if (matched == "D") out += std::to_string(date.getDate());
      else if (matched == "DD") out += padZero(date.getDate(), 2);
      else if (matched == "d") out += std::to_string(date.getDay());
      else if (matched == "dd") out += WEEKDAYS()[date.getDay()].substr(0, 2);
      else if (matched == "ddd") out += WEEKDAYS()[date.getDay()].substr(0, 3);
      else if (matched == "dddd") out += WEEKDAYS()[date.getDay()];
      else if (matched == "H") out += std::to_string(hours);
      else if (matched == "HH") out += padZero(hours, 2);
      else if (matched == "h") out += std::to_string((hours % 12) ? (hours % 12) : 12);
      else if (matched == "hh") out += padZero((hours % 12) ? (hours % 12) : 12, 2);
      else if (matched == "a") out += (hours < 12 ? "am" : "pm");
      else if (matched == "A") out += (hours < 12 ? "AM" : "PM");
      else if (matched == "m") out += std::to_string(date.getMinutes());
      else if (matched == "mm") out += padZero(date.getMinutes(), 2);
      else if (matched == "s") out += std::to_string(date.getSeconds());
      else if (matched == "ss") out += padZero(date.getSeconds(), 2);
      else if (matched == "SSS") out += padZero(date.getMilliseconds(), 3);
      else if (matched == "Z") out += zone(date);
      else if (matched == "ZZ") out += replaceAllStr(zone(date), ":", "");
    } else {
      out.push_back(advanced[i++]);
    }
  }
  return out;
}

struct Fields {
  std::optional<int> year;
  std::optional<int> month;
  std::optional<int> day;
  bool dayGiven = false;
  std::optional<int> hours;
  std::optional<int> minutes;
  std::optional<int> seconds;
  std::optional<int> milliseconds;
  std::optional<int> week;
  std::optional<int> offset;
  std::optional<bool> afternoon;
};

enum class MatchKind {
  Literal,
  Digit1,
  Digit2,
  Digit3,
  Digit4,
  Digit1To2,
  Signed,
  Offset,
  Word
};

struct ParseStep {
  MatchKind matchKind = MatchKind::Literal;
  std::string token; // literal text or format token name
};

inline bool isSepChar(char c) noexcept {
  return c == '-' || c == '_' || c == ':' || c == '/' || c == '.' || c == ',' || c == '(' || c == ')' ||
         isSpace(static_cast<unsigned char>(c));
}

inline bool isWordNonSepNonDigit(char c) noexcept {
  return !(c >= '0' && c <= '9') && c != '-' && c != '_' && c != ':' && c != '/' && c != ',' && c != '(' && c != ')' &&
         !isSpace(static_cast<unsigned char>(c));
}

inline std::optional<std::string> findMatchInSlice(const std::string& slice, MatchKind kind) noexcept {
  size_t n = slice.size();
  auto isDig = [](char c) { return c >= '0' && c <= '9'; };
  switch (kind) {
    case MatchKind::Digit1:
      for (size_t i = 0; i < n; i++) if (isDig(slice[i])) return slice.substr(i, 1);
      return std::nullopt;
    case MatchKind::Digit2:
      for (size_t i = 0; i + 1 < n; i++) if (isDig(slice[i]) && isDig(slice[i + 1])) return slice.substr(i, 2);
      return std::nullopt;
    case MatchKind::Digit3:
      for (size_t i = 0; i + 2 < n; i++) if (isDig(slice[i]) && isDig(slice[i + 1]) && isDig(slice[i + 2])) return slice.substr(i, 3);
      return std::nullopt;
    case MatchKind::Digit4:
      for (size_t i = 0; i + 3 < n; i++) {
        if (isDig(slice[i]) && isDig(slice[i + 1]) && isDig(slice[i + 2]) && isDig(slice[i + 3])) return slice.substr(i, 4);
      }
      return std::nullopt;
    case MatchKind::Digit1To2:
      for (size_t i = 0; i < n; i++) {
        if (isDig(slice[i])) {
          size_t len = (i + 1 < n && isDig(slice[i + 1])) ? 2 : 1;
          return slice.substr(i, len);
        }
      }
      return std::nullopt;
    case MatchKind::Signed:
      for (size_t i = 0; i < n; i++) {
        size_t j = i;
        if ((slice[j] == '+' || slice[j] == '-') && j + 1 < n && isDig(slice[j + 1])) j++;
        if (j < n && isDig(slice[j])) {
          while (j < n && isDig(slice[j])) j++;
          return slice.substr(i, j - i);
        }
      }
      return std::nullopt;
    case MatchKind::Offset:
      for (size_t i = 0; i < n; i++) {
        if (slice[i] == 'Z') return std::string("Z");
        if ((slice[i] == '+' || slice[i] == '-') && i + 2 < n && isDig(slice[i + 1]) && isDig(slice[i + 2])) {
          size_t j = i + 3;
          if (j < n && slice[j] == ':') {
            if (j + 2 < n && isDig(slice[j + 1]) && isDig(slice[j + 2])) j += 3;
            else j += 1;
          } else if (j + 1 < n && isDig(slice[j]) && isDig(slice[j + 1])) {
            j += 2;
          }
          return slice.substr(i, j - i);
        }
      }
      return std::nullopt;
    case MatchKind::Word:
      for (size_t i = 0; i < n; i++) {
        size_t j = i;
        while (j < n && isDig(slice[j])) j++;
        size_t wStart = j;
        while (j < n && isWordNonSepNonDigit(slice[j])) j++;
        if (j > wStart) return slice.substr(i, j - i);
      }
      return std::nullopt;
    default:
      return std::nullopt;
  }
}

inline bool applyStepReader(Fields& f, const std::string& token, const std::string& input) {
  if (token == "A") { f.afternoon = (input == "PM"); return true; }
  if (token == "a") { f.afternoon = (input == "pm"); return true; }
  if (token == "Q") { f.month = (std::stoi(input) - 1) * 3 + 1; return true; }
  if (token == "S") { f.milliseconds = std::stoi(input) * 100; return true; }
  if (token == "SS") { f.milliseconds = std::stoi(input) * 10; return true; }
  if (token == "SSS") { f.milliseconds = std::stoi(input); return true; }
  if (token == "s" || token == "ss") { f.seconds = std::stoi(input); return true; }
  if (token == "m" || token == "mm") { f.minutes = std::stoi(input); return true; }
  if (token == "H" || token == "h" || token == "HH" || token == "hh") { f.hours = std::stoi(input); return true; }
  if (token == "D" || token == "DD") { f.day = std::stoi(input); return true; }
  if (token == "Do") {
    std::string digits;
    for (char c : input) {
      if (c >= '0' && c <= '9') digits.push_back(c);
      else if (!digits.empty()) break;
    }
    if (digits.empty()) return false;
    f.day = std::stoi(digits);
    f.dayGiven = true;
    for (int i = 1; i <= 31; i++) {
      if (ordinal(i) == input) f.day = i;
    }
    return true;
  }
  if (token == "w" || token == "ww") { f.week = std::stoi(input); return true; }
  if (token == "M" || token == "MM") { f.month = std::stoi(input); return true; }
  if (token == "MMM") {
    for (int i = 0; i < 12; i++) {
      if (MONTHS()[i].substr(0, 3) == input) {
        int idx = i + 1;
        f.month = (idx % 12) ? (idx % 12) : idx;
        return true;
      }
    }
    return false;
  }
  if (token == "MMMM") {
    for (int i = 0; i < 12; i++) {
      if (MONTHS()[i] == input) {
        int idx = i + 1;
        f.month = (idx % 12) ? (idx % 12) : idx;
        return true;
      }
    }
    return false;
  }
  if (token == "Y" || token == "YYYY") { f.year = std::stoi(input); return true; }
  if (token == "YY") {
    int v = std::stoi(input);
    f.year = v + (v > 68 ? 1900 : 2000);
    return true;
  }
  if (token == "Z" || token == "ZZ") {
    if (input == "Z") { f.offset = 0; return true; }
    char sign = input[0];
    int hh = std::stoi(input.substr(1, 2));
    int mm = 0;
    if (input.size() >= 5) {
      size_t mPos = (input[3] == ':') ? 4 : 3;
      if (mPos + 2 <= input.size()) mm = std::stoi(input.substr(mPos, 2));
    }
    int mins = hh * 60 + mm;
    f.offset = (mins == 0) ? 0 : (sign == '+' ? -mins : mins);
    return true;
  }
  return false;
}

inline std::optional<MatchKind> readerKindForToken(const std::string& tok) noexcept {
  if (tok == "A" || tok == "a" || tok == "Do" || tok == "MMM" || tok == "MMMM") return MatchKind::Word;
  if (tok == "Q" || tok == "S") return MatchKind::Digit1;
  if (tok == "SS" || tok == "DD" || tok == "ww" || tok == "MM" || tok == "YY") return MatchKind::Digit2;
  if (tok == "SSS") return MatchKind::Digit3;
  if (tok == "YYYY") return MatchKind::Digit4;
  if (tok == "s" || tok == "ss" || tok == "m" || tok == "mm" || tok == "H" || tok == "h" ||
      tok == "HH" || tok == "hh" || tok == "D" || tok == "w" || tok == "M") return MatchKind::Digit1To2;
  if (tok == "Y") return MatchKind::Signed;
  if (tok == "Z" || tok == "ZZ") return MatchKind::Offset;
  return std::nullopt;
}

inline std::optional<std::vector<ParseStep>> compileFormat(const std::string& tmpl) noexcept {
  // Check for 'L' or 'l' outside [...]
  for (size_t i = 0; i < tmpl.size(); ) {
    if (tmpl[i] == '[') {
      size_t close = tmpl.find(']', i + 1);
      if (close != std::string::npos && close > i + 1) {
        i = close + 1;
        continue;
      }
    }
    if (tmpl[i] == 'L' || tmpl[i] == 'l') return std::nullopt;
    i++;
  }

  static const std::vector<std::string> tokList = {
    "YYYY", "MMMM", "MMM", "SSS", "Do", "YY", "ww", "MM", "DD", "hh", "HH", "mm", "ss", "SS", "ZZ",
    "A", "a", "Q", "Y", "w", "M", "D", "h", "H", "m", "s", "S", "z", "Z"
  };

  std::vector<ParseStep> steps;
  for (size_t i = 0; i < tmpl.size(); ) {
    if (tmpl[i] == '[') {
      size_t nextOpen = tmpl.find('[', i + 1);
      size_t close = tmpl.find(']', i + 1);
      if (close != std::string::npos && (nextOpen == std::string::npos || close < nextOpen)) {
        steps.push_back({MatchKind::Literal, tmpl.substr(i + 1, close - i - 1)});
        i = close + 1;
        continue;
      }
    }
    if (isSepChar(tmpl[i])) {
      size_t j = i + 1;
      while (j < tmpl.size() && isSepChar(tmpl[j])) j++;
      steps.push_back({MatchKind::Literal, tmpl.substr(i, j - i)});
      i = j;
      continue;
    }
    std::string matched;
    for (const auto& t : tokList) {
      if (tmpl.compare(i, t.size(), t) == 0) {
        matched = t;
        break;
      }
    }
    if (!matched.empty()) {
      auto mk = readerKindForToken(matched);
      if (mk.has_value()) steps.push_back({*mk, matched});
      else steps.push_back({MatchKind::Literal, matched});
      i += matched.size();
    } else {
      i++;
    }
  }
  if (steps.empty()) return std::nullopt;
  return steps;
}

inline bool isAllDigitsOrSign(const std::string& s, bool allowDot = true) noexcept {
  std::string t = trimStr(s);
  if (t.empty()) return false;
  size_t i = (t[0] == '+' || t[0] == '-') ? 1 : 0;
  if (i >= t.size()) return false;
  bool dot = false;
  for (; i < t.size(); i++) {
    if (t[i] == '.' && allowDot && !dot) dot = true;
    else if (t[i] < '0' || t[i] > '9') return false;
  }
  return true;
}

inline std::optional<GanttDate> loose(const std::string& input, const std::string& tmpl, const GanttDate& now) {
  if (tmpl == "x" || tmpl == "X") {
    if (!isAllDigitsOrSign(input)) return std::nullopt;
    double v = std::stod(input);
    return GanttDate((tmpl == "X" ? 1000.0 : 1.0) * v);
  }
  auto steps = compileFormat(tmpl);
  if (!steps.has_value()) return std::nullopt;
  Fields f;
  std::string rest = input;
  size_t start = 0;
  for (const auto& step : *steps) {
    if (step.matchKind == MatchKind::Literal) {
      start += step.token.size();
      continue;
    }
    if (start > rest.size()) return std::nullopt;
    auto m = findMatchInSlice(rest.substr(start), step.matchKind);
    if (!m.has_value() || !applyStepReader(f, step.token, *m)) return std::nullopt;
    size_t pos = rest.find(*m);
    if (pos != std::string::npos) rest.erase(pos, m->size());
  }
  if (f.afternoon.has_value() && f.hours.has_value()) {
    if (*f.afternoon) {
      if (*f.hours < 12) *f.hours += 12;
    } else if (*f.hours == 12) {
      *f.hours = 0;
    }
  }
  int day = (f.dayGiven || (f.day.has_value() && *f.day != 0))
    ? f.day.value_or(0)
    : ((!f.year.has_value() || *f.year == 0) && (!f.month.has_value() || *f.month == 0) ? now.getDate() : 1);
  int year = (f.year.has_value() && *f.year != 0) ? *f.year : now.getFullYear();
  int month = 0;
  if (!((f.year.has_value() && *f.year != 0) && (!f.month.has_value() || *f.month == 0))) {
    month = (f.month.has_value() && *f.month > 0) ? (*f.month - 1) : now.getMonth();
  }
  int hours = f.hours.value_or(0);
  int minutes = f.minutes.value_or(0);
  int seconds = f.seconds.value_or(0);
  int ms = f.milliseconds.value_or(0);
  if (f.offset.has_value()) {
    return GanttDate::fromUTC(year, month, day, hours, minutes, seconds, ms + *f.offset * 60000.0);
  }
  if (f.week.has_value() && *f.week != 0) return std::nullopt;
  return GanttDate::fromLocal(year, month, day, hours, minutes, seconds, ms);
}

inline std::optional<GanttDate> parseStrict(const std::string& input, const std::string& tmpl, const GanttDate& now) {
  auto date = loose(input, tmpl, now);
  if (!date.has_value() || !isValid(*date)) return std::nullopt;
  auto fmt = format(*date, tmpl);
  if (!fmt.has_value() || *fmt != input) return std::nullopt;
  return date;
}

// Fallback JS `new Date(str)` parser for ISO / slash dates when dateFormat does not match
inline GanttDate parseJsFallbackDate(const std::string& raw) {
  std::string s = trimStr(raw);
  // Match YYYY-MM-DD (ISO date only -> UTC!) or YYYY-MM-DD[T ]HH:mm(:ss(.sss))?(Z|[+-]HH:mm)?
  auto isDig = [](char c) { return c >= '0' && c <= '9'; };
  if (s.size() == 10 && isDig(s[0]) && isDig(s[1]) && isDig(s[2]) && isDig(s[3]) &&
      s[4] == '-' && isDig(s[5]) && isDig(s[6]) && s[7] == '-' && isDig(s[8]) && isDig(s[9])) {
    int y = std::stoi(s.substr(0, 4));
    int m = std::stoi(s.substr(5, 2));
    int d = std::stoi(s.substr(8, 2));
    if (m >= 1 && m <= 12 && d >= 1 && d <= 31) {
      return GanttDate::fromUTC(y, m - 1, d, 0, 0, 0, 0, false);
    }
  }
  // Try parseStrict with common formats
  GanttDate dummy(0.0);
  for (const char* fmt : {"YYYY-MM-DDTHH:mm:ss.SSSZ", "YYYY-MM-DDTHH:mm:ssZ", "YYYY-MM-DDTHH:mm:ss", "YYYY-MM-DD HH:mm:ss", "YYYY/MM/DD", "MM/DD/YYYY"}) {
    auto p = loose(s, fmt, dummy);
    if (p.has_value() && p->isValid()) return *p;
  }
  return GanttDate::invalid();
}

// ============================================================================
// Axis (`src/diagrams/gantt/axis.ts`)
// ============================================================================

constexpr double SECOND = 1000.0;
constexpr double MINUTE = 60.0 * SECOND;
constexpr double HOUR = 60.0 * MINUTE;
constexpr double DAY = 24.0 * HOUR;
constexpr double WEEK = 7.0 * DAY;
constexpr double MONTH = 30.0 * DAY;
constexpr double YEAR = 365.0 * DAY;

struct Calendar {
  std::function<void(GanttDate&)> floorFn;
  std::function<void(GanttDate&, int)> offsetFn;
  std::function<int(const GanttDate&)> fieldFn;
  double cycle = std::numeric_limits<double>::infinity();
  double ms = 1.0;
};

inline void startOfDay(GanttDate& d) noexcept {
  d.setHours(0, 0, 0, 0);
}

inline Calendar weekCalendar(int weekStart) noexcept {
  Calendar c;
  c.floorFn = [weekStart](GanttDate& d) {
    d.setDate(d.getDate() - ((d.getDay() + 7 - weekStart) % 7));
    startOfDay(d);
  };
  c.offsetFn = [](GanttDate& d, int step) {
    d.setDate(d.getDate() + step * 7);
  };
  c.cycle = std::numeric_limits<double>::infinity();
  c.ms = WEEK;
  return c;
}

inline Calendar baseCalendar(const std::string& unit, int weekStart = 0) noexcept {
  if (unit == "week") return weekCalendar(weekStart);
  Calendar c;
  if (unit == "millisecond") {
    c.floorFn = [](GanttDate&) {};
    c.offsetFn = [](GanttDate& d, int step) { d.setTime(d.getTime() + step); };
    c.cycle = std::numeric_limits<double>::infinity();
    c.ms = 1.0;
  } else if (unit == "second") {
    c.floorFn = [](GanttDate& d) { d.setTime(d.getTime() - d.getMilliseconds()); };
    c.offsetFn = [](GanttDate& d, int step) { d.setTime(d.getTime() + step * SECOND); };
    c.fieldFn = [](const GanttDate& d) { return d.getUTCSeconds(); };
    c.cycle = 60.0;
    c.ms = SECOND;
  } else if (unit == "minute") {
    c.floorFn = [](GanttDate& d) { d.setTime(d.getTime() - d.getMilliseconds() - d.getSeconds() * SECOND); };
    c.offsetFn = [](GanttDate& d, int step) { d.setTime(d.getTime() + step * MINUTE); };
    c.fieldFn = [](const GanttDate& d) { return d.getMinutes(); };
    c.cycle = 60.0;
    c.ms = MINUTE;
  } else if (unit == "hour") {
    c.floorFn = [](GanttDate& d) { d.setTime(d.getTime() - d.getMilliseconds() - d.getSeconds() * SECOND - d.getMinutes() * MINUTE); };
    c.offsetFn = [](GanttDate& d, int step) { d.setTime(d.getTime() + step * HOUR); };
    c.fieldFn = [](const GanttDate& d) { return d.getHours(); };
    c.cycle = 24.0;
    c.ms = HOUR;
  } else if (unit == "day") {
    c.floorFn = startOfDay;
    c.offsetFn = [](GanttDate& d, int step) { d.setDate(d.getDate() + step); };
    c.fieldFn = [](const GanttDate& d) { return d.getDate() - 1; };
    c.cycle = 31.0;
    c.ms = DAY;
  } else if (unit == "month") {
    c.floorFn = [](GanttDate& d) { d.setDate(1); startOfDay(d); };
    c.offsetFn = [](GanttDate& d, int step) { d.setMonth(d.getMonth() + step); };
    c.fieldFn = [](const GanttDate& d) { return d.getMonth(); };
    c.cycle = 12.0;
    c.ms = MONTH;
  } else { // year
    c.floorFn = [](GanttDate& d) { d.setMonth(0, 1); startOfDay(d); };
    c.offsetFn = [](GanttDate& d, int step) { d.setFullYear(d.getFullYear() + step); };
    c.cycle = std::numeric_limits<double>::infinity();
    c.ms = YEAR;
  }
  return c;
}

inline int spanUnits(const GanttDate& start, const GanttDate& end, double unit) noexcept {
  return static_cast<int>(std::floor((end.getTime() - start.getTime() - (end.getTimezoneOffset() - start.getTimezoneOffset()) * MINUTE) / unit));
}

inline int posMod(int a, int b) noexcept {
  return ((a % b) + b) % b;
}

inline Calendar every(const std::string& unit, int step, int weekStart) noexcept {
  Calendar base = baseCalendar(unit, weekStart);
  if (step <= 1) return base;
  if (unit == "millisecond") {
    Calendar c = base;
    c.floorFn = [step](GanttDate& d) { d.setTime(std::floor(d.getTime() / step) * step); };
    c.offsetFn = [step](GanttDate& d, int n) { d.setTime(d.getTime() + static_cast<double>(n) * step); };
    return c;
  }
  if (unit == "year") {
    Calendar c = base;
    c.floorFn = [base, step](GanttDate& d) {
      int y = static_cast<int>(std::floor(static_cast<double>(d.getFullYear()) / step)) * step;
      d.setFullYear(y);
      base.floorFn(d);
    };
    c.offsetFn = [step](GanttDate& d, int n) { d.setFullYear(d.getFullYear() + n * step); };
    return c;
  }
  if (unit == "week") {
    GanttDate epoch(0.0);
    base.floorFn(epoch);
    Calendar c = base;
    c.floorFn = [base, epoch, step](GanttDate& d) {
      base.floorFn(d);
      base.offsetFn(d, -posMod(spanUnits(epoch, d, WEEK), step));
    };
    c.offsetFn = [base, step](GanttDate& d, int n) { base.offsetFn(d, n * step); };
    return c;
  }
  auto field = base.fieldFn;
  auto test = [field, step](const GanttDate& d) -> bool { return field(d) % step == 0; };
  Calendar c = base;
  c.floorFn = [base, test](GanttDate& d) {
    for (base.floorFn(d); std::isfinite(d.getTime()) && !test(d); base.floorFn(d)) {
      d.setTime(d.getTime() - 1.0);
    }
  };
  c.offsetFn = [base, test](GanttDate& d, int n) {
    for (int i = 0; i < n && std::isfinite(d.getTime()); i++) {
      do base.offsetFn(d, 1);
      while (std::isfinite(d.getTime()) && !test(d));
    }
  };
  return c;
}

inline std::optional<std::vector<double>> rangeTicks(const Calendar& calendar, double start, double stop, size_t limit) noexcept {
  GanttDate d(start - 1.0);
  calendar.floorFn(d);
  calendar.offsetFn(d, 1);
  calendar.floorFn(d);
  std::vector<double> out;
  double previous = -std::numeric_limits<double>::infinity();
  while (d.getTime() <= stop && d.getTime() > previous) {
    if (out.size() >= limit) return std::nullopt;
    previous = d.getTime();
    out.push_back(previous);
    calendar.offsetFn(d, 1);
    calendar.floorFn(d);
  }
  return out;
}

inline int tickStep(double start, double stop, int count) noexcept {
  double step = (stop - start) / std::max(0, count);
  double power = std::floor(std::log10(step));
  double error = step / std::pow(10.0, power);
  double factor = error >= std::sqrt(50.0) ? 10.0 : error >= std::sqrt(10.0) ? 5.0 : error >= std::sqrt(2.0) ? 2.0 : 1.0;
  return static_cast<int>(std::pow(10.0, power) * factor);
}

inline std::vector<double> autoTicks(double start, double stop, int count) noexcept {
  struct AutoEntry { const char* unit; int step; double ms; };
  static const AutoEntry AUTO[] = {
    {"second", 1, SECOND},
    {"second", 5, 5 * SECOND},
    {"second", 15, 15 * SECOND},
    {"second", 30, 30 * SECOND},
    {"minute", 1, MINUTE},
    {"minute", 5, 5 * MINUTE},
    {"minute", 15, 15 * MINUTE},
    {"minute", 30, 30 * MINUTE},
    {"hour", 1, HOUR},
    {"hour", 3, 3 * HOUR},
    {"hour", 6, 6 * HOUR},
    {"hour", 12, 12 * HOUR},
    {"day", 1, DAY},
    {"day", 2, 2 * DAY},
    {"week", 1, WEEK},
    {"month", 1, MONTH},
    {"month", 3, 3 * MONTH},
    {"year", 1, YEAR},
  };
  constexpr size_t AUTO_LEN = sizeof(AUTO) / sizeof(AUTO[0]);
  double target = (stop - start) / count;
  size_t i = 0;
  while (i < AUTO_LEN && AUTO[i].ms <= target) i++;
  Calendar calendar;
  if (i == AUTO_LEN) {
    calendar = every("year", std::max(1, tickStep(start / YEAR, stop / YEAR, count)), 0);
  } else if (i == 0) {
    calendar = every("millisecond", std::max(tickStep(start, stop, count), 1), 0);
  } else {
    size_t pick = (target / AUTO[i - 1].ms < AUTO[i].ms / target) ? (i - 1) : i;
    calendar = every(AUTO[pick].unit, AUTO[pick].step, 0);
  }
  auto r = rangeTicks(calendar, start, stop, static_cast<size_t>(4 * count + 8));
  return r.value_or(std::vector<double>{});
}

inline std::optional<std::vector<double>> intervalTicks(
  const std::string& interval,
  const std::string& weekday,
  double start,
  double stop,
  size_t limit
) {
  if (interval.empty() || interval[0] < '1' || interval[0] > '9') return std::nullopt;
  size_t p = 0;
  while (p < interval.size() && interval[p] >= '0' && interval[p] <= '9') p++;
  std::string unit = interval.substr(p);
  if (unit != "millisecond" && unit != "second" && unit != "minute" && unit != "hour" &&
      unit != "day" && unit != "week" && unit != "month") {
    return std::nullopt;
  }
  int step = std::stoi(interval.substr(0, p));
  static const char* WEEK_STARTS[] = {"sunday", "monday", "tuesday", "wednesday", "thursday", "friday", "saturday"};
  int wkIdx = 0;
  for (int i = 0; i < 7; i++) if (weekday == WEEK_STARTS[i]) { wkIdx = i; break; }
  Calendar calendar = every(unit, step, wkIdx);
  if ((stop - start) / (std::min(static_cast<double>(step), calendar.cycle) * calendar.ms) > static_cast<double>(limit)) {
    return std::nullopt;
  }
  return rangeTicks(calendar, start, stop, limit);
}

inline std::string d3Pad(long long value, const std::string& fill, size_t width) noexcept {
  std::string sign = value < 0 ? "-" : "";
  std::string digits = std::to_string(value < 0 ? -value : value);
  if (digits.size() < width) {
    std::string prefix;
    for (size_t i = 0; i < width - digits.size(); i++) prefix += fill;
    return sign + prefix + digits;
  }
  return sign + digits;
}

inline GanttDate floored(const GanttDate& date, const Calendar& calendar) noexcept {
  GanttDate d(date.getTime());
  calendar.floorFn(d);
  return d;
}

inline int weekOfYear(const GanttDate& date, int weekStart) noexcept {
  Calendar week = weekCalendar(weekStart);
  GanttDate yearStart = floored(date, baseCalendar("year"));
  return spanUnits(floored(GanttDate(yearStart.getTime() - 1.0), week), floored(date, week), WEEK);
}

inline GanttDate isoThursday(const GanttDate& date) noexcept {
  Calendar thursday = weekCalendar(4);
  int day = date.getDay();
  GanttDate d = floored((day >= 4 || day == 0) ? date : GanttDate(date.getTime() - 1.0), thursday);
  if (!(day >= 4 || day == 0)) {
    thursday.offsetFn(d, 1);
    thursday.floorFn(d);
  }
  return d;
}

inline std::string timeFormat(const std::string& specifier, const GanttDate& date);

inline std::string directive(char c, const GanttDate& d, const std::string& p) noexcept {
  switch (c) {
    case 'a': return WEEKDAYS()[d.getDay()].substr(0, 3);
    case 'A': return WEEKDAYS()[d.getDay()];
    case 'b': return MONTHS()[d.getMonth()].substr(0, 3);
    case 'B': return MONTHS()[d.getMonth()];
    case 'c': return timeFormat("%x, %X", d);
    case 'd':
    case 'e': return d3Pad(d.getDate(), p, 2);
    case 'f': return d3Pad(d.getMilliseconds(), p, 3) + "000";
    case 'g': return d3Pad(isoThursday(d).getFullYear() % 100, p, 2);
    case 'G': return d3Pad(isoThursday(d).getFullYear() % 10000, p, 4);
    case 'H': return d3Pad(d.getHours(), p, 2);
    case 'I': return d3Pad((d.getHours() % 12) ? (d.getHours() % 12) : 12, p, 2);
    case 'j': return d3Pad(1 + spanUnits(floored(d, baseCalendar("year")), floored(d, baseCalendar("day")), DAY), p, 3);
    case 'L': return d3Pad(d.getMilliseconds(), p, 3);
    case 'm': return d3Pad(d.getMonth() + 1, p, 2);
    case 'M': return d3Pad(d.getMinutes(), p, 2);
    case 'p': return d.getHours() >= 12 ? "PM" : "AM";
    case 'q': return std::to_string(1 + d.getMonth() / 3);
    case 'Q': return std::to_string(static_cast<long long>(d.getTime()));
    case 's': return std::to_string(static_cast<long long>(std::floor(d.getTime() / 1000.0)));
    case 'S': return d3Pad(d.getSeconds(), p, 2);
    case 'u': return std::to_string(d.getDay() ? d.getDay() : 7);
    case 'U': return d3Pad(weekOfYear(d, 0), p, 2);
    case 'V': {
      GanttDate thursday = isoThursday(d);
      GanttDate yearStart = floored(thursday, baseCalendar("year"));
      GanttDate first = floored(yearStart, weekCalendar(4));
      return d3Pad(spanUnits(first, thursday, WEEK) + (yearStart.getDay() == 4 ? 1 : 0), p, 2);
    }
    case 'w': return std::to_string(d.getDay());
    case 'W': return d3Pad(weekOfYear(d, 1), p, 2);
    case 'x': return timeFormat("%-m/%-d/%Y", d);
    case 'X': return timeFormat("%-I:%M:%S %p", d);
    case 'y': return d3Pad(d.getFullYear() % 100, p, 2);
    case 'Y': return d3Pad(d.getFullYear() % 10000, p, 4);
    case 'Z': {
      int z = d.getTimezoneOffset();
      int absZ = std::abs(z);
      return std::string(z > 0 ? "-" : "+") + d3Pad(absZ / 60, "0", 2) + d3Pad(absZ % 60, "0", 2);
    }
    case '%': return "%";
    default: return std::string(1, c);
  }
}

inline std::string timeFormat(const std::string& specifier, const GanttDate& date) {
  std::string out;
  size_t from = 0;
  size_t n = specifier.size();
  for (size_t i = 0; i < n; i++) {
    if (specifier[i] != '%') continue;
    out += specifier.substr(from, i - from);
    char c = (++i < n) ? specifier[i] : '\0';
    std::string fill;
    bool hasFill = false;
    if (c == '-') { fill = ""; hasFill = true; }
    else if (c == '_') { fill = " "; hasFill = true; }
    else if (c == '0') { fill = "0"; hasFill = true; }
    if (hasFill) c = (++i < n) ? specifier[i] : '\0';
    else fill = (c == 'e') ? " " : "0";
    if (c != '\0') out += directive(c, date, fill);
    from = i + 1;
  }
  if (from < n) out += specifier.substr(from);
  return out;
}

// ============================================================================
// Lexer & Parser (`src/diagrams/gantt/tokens.ts`, `lexer.ts`, `parser.ts`)
// ============================================================================

enum T {
  END = 0,
  ERROR,
  open_directive,
  acc_title,
  acc_title_value,
  acc_descr,
  acc_descr_value,
  acc_descr_multiline_value,
  NL,
  href,
  callbackname,
  callbackargs,
  click,
  gantt,
  dateFormat,
  inclusiveEndDates,
  topAxis,
  axisFormat,
  tickInterval,
  includes,
  excludes,
  todayMarker,
  weekday_monday,
  weekday_tuesday,
  weekday_wednesday,
  weekday_thursday,
  weekday_friday,
  weekday_saturday,
  weekday_sunday,
  weekend_friday,
  weekend_saturday,
  date,
  title,
  accDescription,
  section,
  taskTxt,
  taskData,
  COLON,
  EOF_TOK
};

inline const std::vector<std::string>& TOKEN_NAMES() noexcept {
  static const std::vector<std::string> names = {
    "$end", "INVALID", "open_directive", "acc_title", "acc_title_value",
    "acc_descr", "acc_descr_value", "acc_descr_multiline_value", "NL",
    "href", "callbackname", "callbackargs", "click", "gantt", "dateFormat",
    "inclusiveEndDates", "topAxis", "axisFormat", "tickInterval", "includes",
    "excludes", "todayMarker", "weekday_monday", "weekday_tuesday",
    "weekday_wednesday", "weekday_thursday", "weekday_friday",
    "weekday_saturday", "weekday_sunday", "weekend_friday", "weekend_saturday",
    "date", "title", "accDescription", "section", "taskTxt", "taskData", ":", "EOF"
  };
  return names;
}

inline bool ciAt(const std::string& src, int p, const std::string& word) noexcept {
  if (p + static_cast<int>(word.size()) > static_cast<int>(src.size())) return false;
  for (size_t i = 0; i < word.size(); i++) {
    char a = src[p + i];
    char b = word[i];
    if (a >= 'A' && a <= 'Z') a = static_cast<char>(a + 32);
    if (b >= 'A' && b <= 'Z') b = static_cast<char>(b + 32);
    if (a != b) return false;
  }
  return true;
}

inline bool isWordChar(char c) noexcept {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
}

inline bool wordBoundaryAt(const std::string& src, int p) noexcept {
  return p >= static_cast<int>(src.size()) || !isWordChar(src[p]);
}

struct GanttTokens {
  std::vector<int> types;
  std::vector<std::string> texts;
  std::vector<int> starts;
};

inline GanttTokens tokenizeGantt(const std::string& src) {
  enum State {
    S_INITIAL = 0,
    S_acc_title,
    S_acc_descr,
    S_acc_descr_multiline,
    S_href,
    S_callbackname,
    S_callbackargs,
    S_click
  };
  int n = static_cast<int>(src.size());
  GanttTokens out;
  int state = S_INITIAL;
  int p = 0;

  auto emit = [&](int type, int s, int e) {
    out.types.push_back(type);
    out.texts.push_back(src.substr(s, e - s));
    out.starts.push_back(s);
    p = e;
  };
  auto until = [&](char ch, int from) -> int {
    size_t e = src.find(ch, from);
    return e == std::string::npos ? n : static_cast<int>(e);
  };
  auto skipSpaces = [&](int i) -> int {
    while (i < n && isSpace(charCodeAt(src, i))) i++;
    return i;
  };
  auto matchStmt = [&](const std::string& kw, bool stopHash, bool stopSemi) -> int {
    int kLen = static_cast<int>(kw.size());
    if (!ciAt(src, p, kw)) return -1;
    int i = p + kLen;
    if (i >= n || !isSpace(charCodeAt(src, i))) return -1;
    i++;
    int startBody = i;
    while (i < n && src[i] != '\n' && (!stopHash || src[i] != '#') && (!stopSemi || src[i] != ';')) i++;
    return i > startBody ? i : -1;
  };

  auto keyword = [&](int c) -> bool {
    int lc = c | 32;
    int e = -1;
    switch (lc) {
      case 'a': {
        if (ciAt(src, p, "acctitle")) {
          int i = skipSpaces(p + 8);
          if (i < n && src[i] == ':') {
            e = skipSpaces(i + 1);
            state = S_acc_title;
            emit(T::acc_title, p, e);
            return true;
          }
        }
        if (ciAt(src, p, "accdescr")) {
          int i = skipSpaces(p + 8);
          if (i < n && src[i] == ':') {
            e = skipSpaces(i + 1);
            state = S_acc_descr;
            emit(T::acc_descr, p, e);
            return true;
          }
          if (i < n && src[i] == '{') {
            p = skipSpaces(i + 1);
            state = S_acc_descr_multiline;
            return true;
          }
        }
        if ((e = matchStmt("axisformat", true, true)) >= 0) { emit(T::axisFormat, p, e); return true; }
        if ((e = matchStmt("accdescription", true, true)) >= 0) { emit(T::accDescription, p, e); return true; }
        break;
      }
      case 'h': {
        if (ciAt(src, p, "href") && p + 4 < n && isSpace(charCodeAt(src, p + 4))) {
          int i = skipSpaces(p + 4);
          if (i < n && src[i] == '"') {
            state = S_href;
            p = i + 1;
            return true;
          }
        }
        break;
      }
      case 'c': {
        if (ciAt(src, p, "call") && p + 4 < n && isSpace(charCodeAt(src, p + 4))) {
          state = S_callbackname;
          p = skipSpaces(p + 4);
          return true;
        }
        if (ciAt(src, p, "click") && p + 5 < n && isSpace(charCodeAt(src, p + 5))) {
          state = S_click;
          p = skipSpaces(p + 5);
          return true;
        }
        break;
      }
      case 't': {
        if (ciAt(src, p, "title")) {
          int i = p + 5;
          while (i < n && isSpace(charCodeAt(src, i)) && src[i] != '\r' && src[i] != '\n') i++;
          if (i >= n || src[i] == '\r' || src[i] == '\n') {
            p = i;
            return true;
          }
        }
        if (ciAt(src, p, "topaxis") && wordBoundaryAt(src, p + 7)) {
          emit(T::topAxis, p, p + 7);
          return true;
        }
        if ((e = matchStmt("tickinterval", true, true)) >= 0) { emit(T::tickInterval, p, e); return true; }
        if ((e = matchStmt("todaymarker", false, true)) >= 0) { emit(T::todayMarker, p, e); return true; }
        if (ciAt(src, p, "title") && p + 5 < n && isSpace(charCodeAt(src, p + 5)) && src[p + 5] != '\r' && src[p + 5] != '\n') {
          int i = p + 6;
          int startBody = i;
          while (i < n && src[i] != '\n') i++;
          if (i > startBody) { emit(T::title, p, i); return true; }
        }
        break;
      }
      case 'w': {
        if (ciAt(src, p, "weekday") && p + 7 < n && isSpace(charCodeAt(src, p + 7))) {
          int i = skipSpaces(p + 7);
          static const char* days[] = {"monday", "tuesday", "wednesday", "thursday", "friday", "saturday", "sunday"};
          for (int d = 0; d < 7; d++) {
            int dLen = static_cast<int>(std::strlen(days[d]));
            if (ciAt(src, i, days[d]) && wordBoundaryAt(src, i + dLen)) {
              emit(T::weekday_monday + d, p, i + dLen);
              return true;
            }
          }
        }
        if (ciAt(src, p, "weekend") && p + 7 < n && isSpace(charCodeAt(src, p + 7))) {
          int i = skipSpaces(p + 7);
          if (ciAt(src, i, "friday") && wordBoundaryAt(src, i + 6)) {
            emit(T::weekend_friday, p, i + 6);
            return true;
          }
          if (ciAt(src, i, "saturday") && wordBoundaryAt(src, i + 8)) {
            emit(T::weekend_saturday, p, i + 8);
            return true;
          }
        }
        break;
      }
      case 'g': {
        if (ciAt(src, p, "gantt") && wordBoundaryAt(src, p + 5)) {
          emit(T::gantt, p, p + 5);
          return true;
        }
        break;
      }
      case 'd': {
        if ((e = matchStmt("dateformat", true, true)) >= 0) { emit(T::dateFormat, p, e); return true; }
        break;
      }
      case 'i': {
        if (ciAt(src, p, "inclusiveenddates") && wordBoundaryAt(src, p + 17)) {
          emit(T::inclusiveEndDates, p, p + 17);
          return true;
        }
        if ((e = matchStmt("includes", true, true)) >= 0) { emit(T::includes, p, e); return true; }
        break;
      }
      case 'e': {
        if ((e = matchStmt("excludes", true, true)) >= 0) { emit(T::excludes, p, e); return true; }
        break;
      }
      case 's': {
        if ((e = matchStmt("section", false, false)) >= 0) { emit(T::section, p, e); return true; }
        break;
      }
    }
    return false;
  };

  auto isDateAt = [&](int at) -> bool {
    if (at + 10 > n) return false;
    auto isD = [](char ch) { return ch >= '0' && ch <= '9'; };
    return isD(src[at]) && isD(src[at + 1]) && isD(src[at + 2]) && isD(src[at + 3]) &&
           src[at + 4] == '-' && isD(src[at + 5]) && isD(src[at + 6]) &&
           src[at + 7] == '-' && isD(src[at + 8]) && isD(src[at + 9]) &&
           wordBoundaryAt(src, at + 10);
  };

  while (true) {
    if (p >= n) {
      int type = T::EOF_TOK;
      switch (state) {
        case S_acc_title: type = T::acc_title_value; break;
        case S_acc_descr: type = T::acc_descr_value; break;
        case S_acc_descr_multiline: type = T::acc_descr_multiline_value; break;
        case S_href: type = T::href; break;
        case S_callbackname: type = T::callbackname; break;
        case S_callbackargs: type = T::callbackargs; break;
        case S_click: type = T::click; break;
      }
      emit(type, n, n);
      break;
    }

    int c = charCodeAt(src, p);
    switch (state) {
      case S_INITIAL: {
        int d = charCodeAt(src, p + 1);
        if (c == 37) {
          if (d != 37) {
            int e = until('\n', p + 1);
            while (e < n && src[e] == '\n') e++;
            p = e;
          } else if (charCodeAt(src, p + 2) == 123) {
            emit(T::open_directive, p, p + 3);
            emit(T::ERROR, p, p);
            return out;
          } else {
            p = until('\n', p + 2);
          }
        } else if (c == 10) {
          int e = p + 1;
          while (e < n && src[e] == '\n') e++;
          emit(T::NL, p, e);
        } else if (isSpace(c)) {
          p++;
          while (p < n && isSpace(charCodeAt(src, p))) p++;
        } else if (keyword(c)) {
          break;
        } else if (c >= '0' && c <= '9' && isDateAt(p)) {
          emit(T::date, p, p + 10);
        } else if (c != ':') {
          int e = p + 1;
          while (e < n && src[e] != ':' && src[e] != '\n') e++;
          emit(T::taskTxt, p, e);
        } else {
          int e = p + 1;
          while (e < n && src[e] != '#' && src[e] != '\n' && src[e] != ';') e++;
          emit(e > p + 1 ? T::taskData : T::COLON, p, e);
        }
        break;
      }
      case S_acc_title:
      case S_acc_descr: {
        int type = (state == S_acc_title) ? T::acc_title_value : T::acc_descr_value;
        state = S_INITIAL;
        emit(type, p, until('\n', p));
        break;
      }
      case S_acc_descr_multiline:
        if (c == '}') {
          state = S_INITIAL;
          p++;
        } else {
          emit(T::acc_descr_multiline_value, p, until('}', p));
        }
        break;
      case S_href:
        if (c == '"') {
          state = S_INITIAL;
          p++;
        } else {
          emit(T::href, p, until('"', p));
        }
        break;
      case S_callbackname:
        if (c == '(') {
          int i = skipSpaces(p + 1);
          if (i < n && src[i] == ')') {
            state = S_INITIAL;
            p = i + 1;
          } else {
            state = S_callbackargs;
            p++;
          }
        } else {
          emit(T::callbackname, p, until('(', p));
        }
        break;
      case S_callbackargs:
        if (c == ')') {
          state = S_INITIAL;
          p++;
        } else {
          emit(T::callbackargs, p, until(')', p));
        }
        break;
      case S_click:
        if (isSpace(c)) {
          state = S_INITIAL;
          p++;
        } else {
          int e = p + 1;
          while (e < n && !isSpace(charCodeAt(src, e))) e++;
          emit(T::click, p, e);
        }
        break;
    }
  }

  out.types.push_back(T::END);
  out.texts.push_back("");
  out.starts.push_back(n);
  return out;
}

// ============================================================================
// GanttDb (`src/diagrams/gantt/db.ts`)
// ============================================================================

struct GanttTask {
  std::string id;
  std::string task;
  std::string section;
  std::optional<GanttDate> startTime;
  std::optional<GanttDate> endTime;
  std::optional<GanttDate> renderEndTime;
  bool manualEndTime = false;
  bool processed = false;
  bool active = false;
  bool done = false;
  bool crit = false;
  bool milestone = false;
  bool vert = false;
  int order = -1;
  std::vector<std::string> classes;
  std::optional<std::string> prevTaskId;
  struct Raw {
    std::string data;
    std::optional<std::string> start;
    std::string end;
  } raw;
};

inline double ganttClock(std::optional<double> nowOpt = std::nullopt) noexcept {
  if (nowOpt.has_value() && std::isfinite(*nowOpt)) return *nowOpt;
  auto tp = std::chrono::system_clock::now();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
  return static_cast<double>(ms);
}

inline std::pair<double, std::string> parseDuration(const std::string& str) {
  std::string s = trimStr(str);
  if (s.empty()) return {std::numeric_limits<double>::quiet_NaN(), "ms"};
  std::string unit;
  size_t numEnd = s.size();
  if (s.size() >= 2 && s.substr(s.size() - 2) == "ms") {
    unit = "ms";
    numEnd = s.size() - 2;
  } else {
    char last = s.back();
    if (last == 'M' || last == 'd' || last == 'h' || last == 'm' || last == 's' || last == 'w' || last == 'y') {
      unit = std::string(1, last);
      numEnd = s.size() - 1;
    } else {
      return {std::numeric_limits<double>::quiet_NaN(), "ms"};
    }
  }
  std::string numPart = s.substr(0, numEnd);
  if (numPart.empty()) return {std::numeric_limits<double>::quiet_NaN(), "ms"};
  bool dot = false;
  for (size_t i = 0; i < numPart.size(); i++) {
    char c = numPart[i];
    if (c == '.' && !dot && i > 0 && i + 1 < numPart.size()) dot = true;
    else if (c < '0' || c > '9') return {std::numeric_limits<double>::quiet_NaN(), "ms"};
  }
  return {std::stod(numPart), unit};
}

inline std::vector<std::string> mergeTokens(const std::vector<std::string>& existing, const std::string& txt) noexcept {
  std::vector<std::string> out;
  Pele::FlatSet<std::string> seen;
  for (const auto& e : existing) {
    if (seen.insert(e).second) out.push_back(e);
  }
  std::string low = toLowerAscii(txt);
  size_t i = 0;
  while (i < low.size()) {
    while (i < low.size() && (low[i] == ',' || isSpace(charCodeAt(low, static_cast<int>(i))))) i++;
    size_t j = i;
    while (j < low.size() && low[j] != ',' && !isSpace(charCodeAt(low, static_cast<int>(j)))) j++;
    if (j > i) {
      std::string tok = low.substr(i, j - i);
      if (seen.insert(tok).second) out.push_back(tok);
    }
    i = j;
  }
  return out;
}

class GanttDb : public DiagramMeta {
public:
  std::string dateFormat;
  std::string axisFormat;
  std::optional<std::string> tickInterval;
  std::string todayMarker;
  std::vector<std::string> includes;
  std::vector<std::string> excludes;
  bool inclusiveEndDates = false;
  bool topAxis = false;
  std::string displayMode;
  std::optional<std::string> weekday;
  std::string weekend = "saturday";
  std::vector<std::string> sections;
  Pele::FlatMap<std::string, std::string> links;
  std::vector<GanttTask> tasks;

private:
  Pele::FlatMap<std::string, size_t> byId;
  std::string currentSection;
  int taskCount = 0;
  int lastOrder = 0;
  std::optional<std::string> lastTaskId;
  double now = 0.0;
  double work = 0.0;
  Pele::FlatSet<std::string> includeSet;
  Pele::FlatSet<std::string> excludeSet;
  std::vector<bool> excludedDays;
  bool datedTokens = false;

  static constexpr double WORK_LIMIT = 4e8;
  static constexpr double DAY_STEP = 400.0;
  static constexpr double DATED_STEP = 200.0;
  static constexpr double FORMAT_STEP = 400.0;

public:
  void setDateFormat(const std::string& txt) { dateFormat = txt; }
  void enableInclusiveEndDates() { inclusiveEndDates = true; }
  void TopAxis() { topAxis = true; }
  void setAxisFormat(const std::string& txt) { axisFormat = txt; }
  void setTickInterval(const std::string& txt) { tickInterval = txt; }
  void setTodayMarker(const std::string& txt) { todayMarker = txt; }
  void setIncludes(const std::string& txt) { includes = mergeTokens(includes, txt); }
  void setExcludes(const std::string& txt) { excludes = mergeTokens(excludes, txt); }
  void setWeekday(const std::string& txt) { weekday = txt; }
  void setWeekend(const std::string& startDay) { weekend = startDay; }
  void setDiagramTitle(const std::string& txt) { title = txt; }
  void setAccTitle(const std::string& txt) { accTitle = trimStartStr(txt); }
  void setAccDescription(const std::string& txt) { accDescr = cleanAccDescr(txt); }
  void addSection(const std::string& txt) {
    currentSection = txt;
    sections.push_back(txt);
  }

  void addTask(const std::string& descr, const std::string& dataStr) {
    std::string rawData = (!dataStr.empty() && dataStr[0] == ':') ? dataStr.substr(1) : dataStr;
    std::vector<std::string> data;
    size_t pos = 0;
    for (;;) {
      size_t comma = rawData.find(',', pos);
      if (comma == std::string::npos) {
        data.push_back(rawData.substr(pos));
        break;
      }
      data.push_back(rawData.substr(pos, comma - pos));
      pos = comma + 1;
    }

    GanttTask task;
    task.task = descr;
    task.section = currentSection;
    task.prevTaskId = lastTaskId;
    task.raw.data = dataStr;

    while (!data.empty()) {
      std::string t = trimStr(data.front());
      if (t == "active") task.active = true;
      else if (t == "done") task.done = true;
      else if (t == "crit") task.crit = true;
      else if (t == "milestone") task.milestone = true;
      else if (t == "vert") task.vert = true;
      else break;
      data.erase(data.begin());
    }
    if (data.empty()) {
      throw PeleError(Pele::cat({"Task \"", trimStr(descr), "\" needs a duration or an end date."}),  "semantic",  "gantt");
    }
    if (data.size() > 3) {
      throw PeleError(Pele::cat({"Task \"", trimStr(descr), "\" has too many fields. A task takes an id, a start, and an end or duration."}), 
        "semantic", 
        "gantt"
      );
    }
    for (auto& d : data) d = trimStr(d);
    if (data.size() == 3) {
      task.id = data[0];
    } else {
      taskCount++;
      task.id = "task" + std::to_string(taskCount);
    }
    if (data.size() > 1) task.raw.start = data[data.size() - 2];
    task.raw.end = data.back();

    if (!task.vert) task.order = lastOrder++;
    size_t idx = tasks.size();
    lastTaskId = task.id;
    byId[task.id] = idx;
    tasks.push_back(std::move(task));
  }

  void setLink(const std::string& ids, const std::string& link) {
    std::string href = sanitizeUrl(link);
    size_t pos = 0;
    for (;;) {
      size_t comma = ids.find(',', pos);
      std::string id = comma == std::string::npos ? ids.substr(pos) : ids.substr(pos, comma - pos);
      if (byId.find(id) != byId.end()) links[id] = href;
      if (comma == std::string::npos) break;
      pos = comma + 1;
    }
    setClass(ids, "clickable");
  }

  void setClickEvent(const std::string& ids, const std::string&, const std::optional<std::string>&) {
    setClass(ids, "clickable");
  }

  void setClass(const std::string& ids, const std::string& className) {
    size_t pos = 0;
    for (;;) {
      size_t comma = ids.find(',', pos);
      std::string id = comma == std::string::npos ? ids.substr(pos) : ids.substr(pos, comma - pos);
      auto it = byId.find(id);
      if (it != byId.end()) tasks[it->second].classes.push_back(className);
      if (comma == std::string::npos) break;
      pos = comma + 1;
    }
  }

  static bool isNumericFormat(const std::string& s) {
    static const std::string allowed = "YMDHhmsSZXx-/.:, T";
    for (char c : s) {
      if (allowed.find(c) == std::string::npos) return false;
    }
    return true;
  }

  std::vector<GanttTask>& getTasks(std::optional<double> nowOpt = std::nullopt) {
    now = ganttClock(nowOpt);
    work = 0.0;
    includeSet.clear();
    for (const auto& s : includes) includeSet.insert(s);
    excludeSet.clear();
    for (const auto& s : excludes) excludeSet.insert(s);

    int weekendStart = (weekend == "friday") ? 5 : (weekend == "saturday") ? 6 : -99;
    bool weekends = excludeSet.find("weekends") != excludeSet.end();
    excludedDays.assign(7, false);
    for (int day = 0; day < 7; day++) {
      int iso = day == 0 ? 7 : day;
      std::string nameLow = toLowerAscii(WEEKDAYS()[day]);
      excludedDays[day] = (weekends && (iso == weekendStart || iso == weekendStart + 1)) ||
                          (excludeSet.find(nameLow) != excludeSet.end());
    }

    datedTokens = !includes.empty() || !isNumericFormat(trimStr(dateFormat));
    static const Pele::FlatSet<std::string> dayNames = {
      "sunday", "monday", "tuesday", "wednesday", "thursday", "friday", "saturday"
    };
    for (const auto& token : excludes) {
      if (token != "weekends" && dayNames.find(token) == dayNames.end()) datedTokens = true;
    }

    for (auto& task : tasks) {
      task.startTime.reset();
      task.endTime.reset();
      task.renderEndTime.reset();
      task.processed = false;
      task.manualEndTime = false;
    }

    bool allProcessed = compileTasks();
    for (int i = 0; !allProcessed && i < 10; i++) allProcessed = compileTasks();
    return tasks;
  }

  bool isInvalidDate(const GanttDate& date) {
    bool wk = excludedDays[date.getDay()];
    if (!datedTokens) return wk;
    std::string tmpl = trimStr(dateFormat);
    std::string dateOnly = isoDate(date);
    std::optional<std::string> formatted = dateOnly;
    spend(DATED_STEP);
    if (tmpl != "YYYY-MM-DD") {
      spend(FORMAT_STEP + static_cast<double>(tmpl.size()) * 8.0);
      formatted = format(date, tmpl);
    }
    auto listed = [&](const Pele::FlatSet<std::string>& set) -> bool {
      return set.find(dateOnly) != set.end() || (formatted.has_value() && set.find(*formatted) != set.end());
    };
    if (listed(includeSet)) return false;
    return wk || listed(excludeSet);
  }

private:
  void spend(double units) {
    work += units;
    if (work > WORK_LIMIT) {
      throw PeleError("The gantt chart takes too much work to schedule.", "limit", "gantt");
    }
  }

  std::optional<GanttDate> parse(const std::string& str, const std::string& tmpl) {
    spend(static_cast<double>((tmpl.size() + 1) * (str.size() + 1)));
    return parseStrict(str, tmpl, GanttDate(now));
  }

  GanttDate today() const {
    GanttDate t(now);
    t.setHours(0, 0, 0, 0);
    return t;
  }

  bool compileTasks() {
    bool allProcessed = true;
    for (auto& task : tasks) {
      compileTask(task);
      allProcessed = allProcessed && task.processed;
    }
    return allProcessed;
  }

  void compileTask(GanttTask& task) {
    if (!task.raw.start.has_value()) {
      if (task.prevTaskId.has_value()) {
        auto it = byId.find(*task.prevTaskId);
        task.startTime = (it != byId.end()) ? tasks[it->second].endTime : std::optional<GanttDate>(today());
      } else {
        task.startTime = today();
      }
    } else {
      auto st = getStartDate(*task.raw.start);
      if (st.has_value()) task.startTime = *st;
    }
    if (!task.startTime.has_value()) return;
    task.endTime = getEndDate(*task.startTime, task.raw.end);
    if (!task.endTime.has_value()) return;
    task.processed = true;
    task.manualEndTime = parse(task.raw.end, "YYYY-MM-DD").has_value();
    checkTaskDates(task);
  }

  std::vector<const GanttTask*> references(const std::string& list) const {
    std::vector<const GanttTask*> found;
    size_t i = 0;
    while (i < list.size()) {
      while (i < list.size() && list[i] == ' ') i++;
      size_t j = i;
      while (j < list.size() && list[j] != ' ') j++;
      if (j > i) {
        std::string id = list.substr(i, j - i);
        auto it = byId.find(id);
        if (it != byId.end()) found.push_back(&tasks[it->second]);
      }
      i = j;
    }
    return found;
  }

  static bool isAfterOrUntil(const std::string& str, const std::string& kw, std::string& restOut) {
    if (str.compare(0, kw.size(), kw) != 0) return false;
    size_t i = kw.size();
    if (i >= str.size() || !isSpace(charCodeAt(str, static_cast<int>(i)))) return false;
    while (i < str.size() && isSpace(charCodeAt(str, static_cast<int>(i)))) i++;
    size_t j = i;
    while (j < str.size() && (isWordChar(str[j]) || str[j] == '-' || str[j] == ' ')) j++;
    if (j == i) return false;
    restOut = str.substr(i, j - i);
    return true;
  }

  std::optional<GanttDate> getStartDate(const std::string& raw) {
    std::string str = trimStr(raw);
    std::string tmpl = trimStr(dateFormat);

    if ((tmpl == "x" || tmpl == "X") && !str.empty()) {
      bool allDig = true;
      for (char c : str) if (c < '0' || c > '9') { allDig = false; break; }
      if (allDig) return checked(GanttDate( std::stod(str) ), str);
    }

    std::string refList;
    if (isAfterOrUntil(str, "after", refList)) {
      auto refs = references(refList);
      const GanttTask* latest = nullptr;
      for (const auto* t : refs) {
        if (!t->endTime.has_value()) return std::nullopt;
        if (!latest || t->endTime->getTime() > latest->endTime->getTime()) latest = t;
      }
      return latest ? latest->endTime : std::optional<GanttDate>(today());
    }

    auto parsed = parse(str, tmpl);
    if (parsed.has_value()) return *parsed;

    GanttDate fallback = parseJsFallbackDate(str);
    if (!isValid(fallback) || fallback.getFullYear() < -10000 || fallback.getFullYear() > 10000) {
      throw PeleError(Pele::cat({"Invalid date:", str}),  "semantic",  "gantt");
    }
    return fallback;
  }

  GanttDate checked(const GanttDate& date, const std::string& str) {
    if (!isValid(date)) throw PeleError(Pele::cat({"Invalid date:", str}),  "semantic",  "gantt");
    return date;
  }

  std::optional<GanttDate> getEndDate(const GanttDate& startTime, const std::string& raw) {
    std::string str = trimStr(raw);
    std::string refList;
    if (isAfterOrUntil(str, "until", refList)) {
      auto refs = references(refList);
      const GanttTask* earliest = nullptr;
      for (const auto* t : refs) {
        if (!t->startTime.has_value()) return std::nullopt;
        if (!earliest || t->startTime->getTime() < earliest->startTime->getTime()) earliest = t;
      }
      return earliest ? earliest->startTime : std::optional<GanttDate>(today());
    }

    std::string tmpl = trimStr(dateFormat);
    auto parsed = parse(str, tmpl);
    if (parsed.has_value()) return inclusiveEndDates ? addDays(*parsed, 1) : *parsed;

    auto [val, unit] = parseDuration(str);
    if (std::isnan(val)) return startTime;
    GanttDate end = add(startTime, val, unit);
    return isValid(end) ? end : startTime;
  }

  void checkTaskDates(GanttTask& task) {
    if (excludes.empty() || task.manualEndTime) return;
    GanttDate cursor = addDays(*task.startTime, 1);
    GanttDate endTime = *task.endTime;
    double maxEndTime = addDays(endTime, 10000).getTime();
    bool invalid = false;
    std::optional<double> renderEndMs;
    while (cursor.getTime() <= endTime.getTime()) {
      spend(DAY_STEP);
      if (!invalid) renderEndMs = endTime.getTime();
      invalid = isInvalidDate(cursor);
      if (invalid) {
        endTime.setDate(endTime.getDate() + 1);
        if (endTime.getTime() > maxEndTime) {
          throw PeleError(
            "Failed to find a valid date that was not excluded by `excludes` after 10,000 iterations.",
            "semantic",
            "gantt"
          );
        }
      }
      cursor.setDate(cursor.getDate() + 1);
    }
    task.endTime = endTime;
    if (renderEndMs.has_value()) task.renderEndTime = GanttDate(*renderEndMs);
    else task.renderEndTime.reset();
  }
};

// ============================================================================
// Parser (`src/diagrams/gantt/parser.ts`)
// ============================================================================

inline void parseGantt(const std::string& src, GanttDb& db) {
  static const char* WEEKDAYS_LOWER[] = {
    "monday", "tuesday", "wednesday", "thursday", "friday", "saturday", "sunday"
  };
  static const std::string LINE =
    "'EOF', 'SPACE', 'NL', 'weekday_monday', 'weekday_tuesday', 'weekday_wednesday', 'weekday_thursday', "
    "'weekday_friday', 'weekday_saturday', 'weekday_sunday', 'weekend_friday', 'weekend_saturday', 'dateFormat', "
    "'inclusiveEndDates', 'topAxis', 'axisFormat', 'tickInterval', 'excludes', 'includes', 'todayMarker', 'title', "
    "'acc_title', 'acc_descr', 'acc_descr_multiline_value', 'section', 'taskTxt', 'click'";

  static const std::vector<uint8_t> STARTS_LINE = []() {
    std::vector<uint8_t> s(T::EOF_TOK + 1, 0);
    for (int t = T::weekday_monday; t <= T::weekend_saturday; t++) s[t] = 1;
    for (int t : {
      T::EOF_TOK, T::NL, T::dateFormat, T::inclusiveEndDates, T::topAxis,
      T::axisFormat, T::tickInterval, T::excludes, T::includes, T::todayMarker,
      T::title, T::acc_title, T::acc_descr, T::acc_descr_multiline_value,
      T::section, T::taskTxt, T::click
    }) {
      s[t] = 1;
    }
    return s;
  }();

  GanttTokens toks = tokenizeGantt(src);
  size_t i = 0;

  auto fail = [&](const std::string& expected) {
    int t = toks.types[i];
    std::string got = (t == T::END) ? "1" : TOKEN_NAMES()[t];
    throw syntaxError("gantt",  src,  toks.starts[i], Pele::cat({"Expecting ", expected, ", got '", got, "'"}));
  };

  auto end = [&](const std::string& also = "") {
    int t = toks.types[i];
    if (t < 0 || t >= static_cast<int>(STARTS_LINE.size()) || STARTS_LINE[t] != 1) {
      fail(LINE + also);
    }
  };

  if (toks.types[i] != T::gantt) fail("'gantt'");
  i++;

  while (true) {
    int t = toks.types[i];
    const std::string& text = toks.texts[i];
    if (t < 0 || t >= static_cast<int>(STARTS_LINE.size()) || STARTS_LINE[t] != 1) fail(LINE);
    i++;
    switch (t) {
      case T::EOF_TOK:
        if (toks.types[i] == T::END) return;
        end();
        break;
      case T::NL:
        break;
      case T::dateFormat:
        end();
        db.setDateFormat(text.substr(11));
        break;
      case T::inclusiveEndDates:
        end();
        db.enableInclusiveEndDates();
        break;
      case T::topAxis:
        end();
        db.TopAxis();
        break;
      case T::axisFormat:
        end();
        db.setAxisFormat(text.substr(11));
        break;
      case T::tickInterval:
        end();
        db.setTickInterval(text.substr(13));
        break;
      case T::excludes:
        end();
        db.setExcludes(text.substr(9));
        break;
      case T::includes:
        end();
        db.setIncludes(text.substr(9));
        break;
      case T::todayMarker:
        end();
        db.setTodayMarker(text.substr(12));
        break;
      case T::weekend_friday:
        end();
        db.setWeekend("friday");
        break;
      case T::weekend_saturday:
        end();
        db.setWeekend("saturday");
        break;
      case T::title:
        end();
        db.setDiagramTitle(text.substr(6));
        break;
      case T::acc_title: {
        if (toks.types[i] != T::acc_title_value) fail("'acc_title_value'");
        std::string val = toks.texts[i++];
        end();
        db.setAccTitle(trimStr(val));
        break;
      }
      case T::acc_descr: {
        if (toks.types[i] != T::acc_descr_value) fail("'acc_descr_value'");
        std::string val = toks.texts[i++];
        end();
        db.setAccDescription(trimStr(val));
        break;
      }
      case T::acc_descr_multiline_value:
        end();
        db.setAccDescription(trimStr(text));
        break;
      case T::section:
        end();
        db.addSection(text.substr(8));
        break;
      case T::taskTxt: {
        if (toks.types[i] != T::taskData) fail("'taskData'");
        std::string data = toks.texts[i++];
        end();
        db.addTask(text, data);
        break;
      }
      case T::click: {
        std::optional<std::string> name;
        std::optional<std::string> args;
        std::optional<std::string> href;
        if (toks.types[i] == T::callbackname) {
          name = toks.texts[i++];
          if (toks.types[i] == T::callbackargs) args = toks.texts[i++];
          if (toks.types[i] == T::href) {
            href = toks.texts[i++];
            end();
          } else {
            end(!args.has_value() ? ", 'callbackargs', 'href'" : ", 'href'");
          }
        } else if (toks.types[i] == T::href) {
          href = toks.texts[i++];
          if (toks.types[i] == T::callbackname) {
            name = toks.texts[i++];
            if (toks.types[i] == T::callbackargs) {
              args = toks.texts[i++];
              end();
            } else {
              end(", 'callbackargs'");
            }
          } else {
            end(", 'callbackname'");
          }
        } else {
          fail("'callbackname', 'href'");
        }
        if (name.has_value()) db.setClickEvent(text, *name, args);
        if (href.has_value()) db.setLink(text, *href);
        break;
      }
      default:
        end();
        db.setWeekday(WEEKDAYS_LOWER[t - T::weekday_monday]);
        break;
    }
  }
}

// ============================================================================
// Renderer (`src/diagrams/gantt/render.ts`)
// ============================================================================

constexpr double WIDTH = 800.0;
constexpr double MIN_PLOT = 240.0;
constexpr double LABEL_GAP = 8.0;

inline double ganttCapWidth(const RenderOptions& options, double natural, double floor) noexcept {
  double maxWidth = options.maxWidth;
  return maxWidth > 0.0 ? std::max(std::min(floor, natural), std::min(natural, maxWidth)) : natural;
}

inline std::optional<double> setting(const Config& config, const std::string& key, double minV, double maxV) noexcept {
  const auto* sec = config.get("gantt");
  if (sec && sec->isMap()) {
    const auto* v = sec->get(key);
    if (v && v->isNumber() && std::isfinite(v->nVal)) {
      return std::min(maxV, std::max(minV, v->nVal));
    }
  }
  return std::nullopt;
}

inline std::optional<std::string> textSetting(const Config& config, const std::string& key) noexcept {
  const auto* sec = config.get("gantt");
  if (sec && sec->isMap()) {
    const auto* v = sec->get(key);
    if (v && v->isString()) return v->sVal;
  }
  return std::nullopt;
}

inline bool flagSetting(const Config& config, const std::string& key) noexcept {
  const auto* sec = config.get("gantt");
  if (sec && sec->isMap()) {
    const auto* v = sec->get(key);
    return v && v->isBool() && v->bVal;
  }
  return false;
}

inline std::string replaceBreakTags(const std::string& raw) noexcept {
  std::string out;
  for (size_t i = 0; i < raw.size(); ) {
    if (raw[i] == '<' && i + 3 < raw.size() &&
        (raw[i + 1] == 'b' || raw[i + 1] == 'B') &&
        (raw[i + 2] == 'r' || raw[i + 2] == 'R')) {
      size_t j = i + 3;
      while (j < raw.size() && isSpace(charCodeAt(raw, static_cast<int>(j)))) j++;
      if (j < raw.size() && raw[j] == '/') j++;
      if (j < raw.size() && raw[j] == '>') {
        out.push_back(' ');
        i = j + 1;
        continue;
      }
    }
    out.push_back(raw[i++]);
  }
  return out;
}

inline Label oneLine(const std::string& raw, const TextMeasurer& measurer, double size) noexcept {
  Label label = layoutLabel(replaceBreakTags(raw), false, measurer, size, 4000.0);
  if (label.lines.size() <= 1) return label;
  Label single = label;
  single.lines = {label.lines[0]};
  single.widths = {label.widths[0]};
  single.width = label.widths[0];
  single.height = label.lineHeight;
  return single;
}

inline bool scheduled(const GanttTask& task) noexcept {
  return task.startTime.has_value() && task.endTime.has_value() &&
         std::isfinite(task.startTime->getTime()) && std::isfinite(task.endTime->getTime());
}

inline int packRows(const std::vector<const GanttTask*>& tasks, int offset, std::unordered_map<const GanttTask*, int>& rowOf) noexcept {
  std::vector<const GanttTask*> sorted;
  for (const auto* t : tasks) if (scheduled(*t)) sorted.push_back(t);
  std::stable_sort(sorted.begin(), sorted.end(), [](const GanttTask* a, const GanttTask* b) {
    double da = a->startTime->getTime();
    double db = b->startTime->getTime();
    if (da != db) return da < db;
    return a->order < b->order;
  });
  std::vector<double> freeEnds;
  for (const auto* task : sorted) {
    double start = task->startTime->getTime();
    size_t row = 0;
    while (row < freeEnds.size() && start < freeEnds[row]) row++;
    if (row == freeEnds.size()) freeEnds.push_back(task->endTime->getTime());
    else freeEnds[row] = task->endTime->getTime();
    rowOf[task] = offset + static_cast<int>(row);
  }
  int rows = static_cast<int>(freeEnds.size());
  for (const auto* task : tasks) {
    if (rowOf.find(task) == rowOf.end()) rowOf[task] = offset + rows++;
  }
  return std::max(rows, 1);
}

struct Bar {
  const GanttTask* task = nullptr;
  Label label;
  int row = 0;
  double x0 = std::numeric_limits<double>::quiet_NaN();
  double x1 = std::numeric_limits<double>::quiet_NaN();
  double labelX = 0.0;
  bool inside = false;
};

inline Rendered renderGanttModel(GanttDb& model, const Config& config, const RenderOptions& options) noexcept {
  double size = options.fontSize;
  const TextMeasurer& measurer = getMeasurer(options);
  double pad = options.padding;
  double now = ganttClock();

  double labelSize = setting(config, "fontSize", 4.0, 72.0).value_or(std::round(size * 0.8125));
  double sectionSize = setting(config, "sectionFontSize", 4.0, 72.0).value_or(labelSize);
  double tickSize = std::round(size * 0.75);
  double barHeight = setting(config, "barHeight", 4.0, 400.0).value_or(std::round(labelSize * 1.85));
  double barGap = setting(config, "barGap", 0.0, 400.0).value_or(std::round(barHeight / 4.0));
  double rowHeight = barHeight + barGap;
  int sectionStyles = static_cast<int>(std::round(setting(config, "numberSectionStyles", 1.0, 1000.0).value_or(4.0)));
  bool compact = (model.displayMode == "compact");
  bool topAxis = model.topAxis || flagSetting(config, "topAxis");

  auto& tasks = model.getTasks(now);
  std::vector<const GanttTask*> rowTasks;
  std::vector<const GanttTask*> verts;
  for (const auto& t : tasks) {
    if (!t.vert) rowTasks.push_back(&t);
    else if (scheduled(t)) verts.push_back(&t);
  }

  Pele::FlatMap<std::string, int> categories;
  std::vector<std::string> categoryOrder;
  for (const auto* task : rowTasks) {
    if (categories.find(task->section) == categories.end()) {
      int idx = static_cast<int>(categories.size());
      categories[task->section] = idx;
      categoryOrder.push_back(task->section);
    }
  }

  std::unordered_map<const GanttTask*, int> rowOf;
  std::vector<std::string> rowSection;
  if (compact) {
    std::unordered_map<std::string, std::vector<const GanttTask*>> groups;
    std::vector<std::string> groupOrder;
    for (const auto* task : rowTasks) {
      if (groups.find(task->section) == groups.end()) groupOrder.push_back(task->section);
      groups[task->section].push_back(task);
    }
    for (const auto& sec : groupOrder) {
      int rows = packRows(groups[sec], static_cast<int>(rowSection.size()), rowOf);
      for (int i = 0; i < rows; i++) rowSection.push_back(sec);
    }
  } else {
    for (const auto* task : rowTasks) {
      rowOf[task] = static_cast<int>(rowSection.size());
      rowSection.push_back(task->section);
    }
  }
  int rowCount = static_cast<int>(rowSection.size());

  double start = std::numeric_limits<double>::infinity();
  double stop = -std::numeric_limits<double>::infinity();
  for (const auto& task : tasks) {
    if (!scheduled(task)) continue;
    start = std::min(start, task.startTime->getTime());
    stop = std::max(stop, task.endTime->getTime());
  }
  bool hasPlot = start <= stop;

  Label title = layoutLabel(model.title, false, measurer, size, 4000.0, Style::Bold);
  double titleHeight = title.height > 0.0 ? title.height + 16.0 : 0.0;

  if (!hasPlot) {
    double width = std::ceil(std::max(title.width, 1.0) + 2.0 * pad);
    double height = std::ceil(std::max(title.height, 1.0) + 2.0 * pad);
    std::string content = labelSvg(title, width / 2.0, pad + title.height / 2.0, " class=\"pele-title\" font-weight=\"var(--_tw)\"");
    return {svgDocument("gantt", width, height, size, options, model, content), width, height, {}};
  }

  std::unordered_map<std::string, Label> sectionLabels;
  double sectionWidth = 0.0;
  for (const auto& section : categoryOrder) {
    Label label = layoutLabel(section, false, measurer, sectionSize, 180.0);
    sectionWidth = std::max(sectionWidth, label.width);
    sectionLabels[section] = std::move(label);
  }

  std::string axisFormat = !model.axisFormat.empty()
    ? model.axisFormat
    : (model.dateFormat == "D" ? "%d" : textSetting(config, "axisFormat").value_or("%Y-%m-%d"));
  auto tickLabel = [&](double t) -> std::string { return timeFormat(axisFormat, GanttDate(t)); };
  double sampleWidth = std::max(measurer.width(tickLabel(start), tickSize, 0), measurer.width(tickLabel(stop), tickSize, 0));

  double left = pad + setting(config, "leftPadding", 0.0, 2000.0).value_or(std::max(sectionWidth > 0.0 ? sectionWidth + 24.0 : 0.0, sampleWidth / 2.0));
  double rightPadding = setting(config, "rightPadding", 0.0, 2000.0).value_or(std::max(16.0, sampleWidth / 2.0));
  double target = ganttCapWidth(options, setting(config, "useWidth", 100.0, 20000.0).value_or(WIDTH), 280.0);
  double plotWidth = std::max(MIN_PLOT, target - left - rightPadding - pad);
  double right = left + plotWidth;
  double scale = stop > start ? plotWidth / (stop - start) : 0.0;
  auto x = [&](double t) -> double { return scale == 0.0 ? left + plotWidth / 2.0 : left + (t - start) * scale; };

  double tickHeight = std::round(tickSize * 1.75);
  auto topPadding = setting(config, "topPadding", 0.0, 2000.0);
  auto gridStart = setting(config, "gridLineStartPadding", 0.0, 2000.0);
  double headHeight = pad + titleHeight + (topAxis ? tickHeight : 0.0);
  double plotTop = std::max(headHeight, topPadding.value_or(0.0));
  double gridLead = (topPadding.has_value() && gridStart.has_value())
    ? std::max(0.0, std::min(*topPadding - *gridStart, plotTop - headHeight))
    : 0.0;
  double plotHeight = std::max(rowCount, 1) * rowHeight;
  double plotBottom = plotTop + plotHeight;

  std::vector<Bar> bars;
  double extent = right + rightPadding;
  for (const auto* task : rowTasks) {
    Label label = oneLine(task->task, measurer, labelSize);
    int row = rowOf[task];
    if (!scheduled(*task)) {
      double labelX = left + LABEL_GAP + label.width / 2.0;
      extent = std::max(extent, labelX + label.width / 2.0);
      bars.push_back({task, std::move(label), row, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN(), labelX, false});
      continue;
    }
    double x0 = x(task->startTime->getTime());
    double endMs = task->renderEndTime.has_value() ? task->renderEndTime->getTime() : task->endTime->getTime();
    double x1 = std::max(x(endMs), x0 + (task->milestone ? 0.0 : 2.0));
    if (task->milestone) {
      double mid = x0 + (x(task->endTime->getTime()) - x0) / 2.0;
      x0 = mid - barHeight / 2.0;
      x1 = mid + barHeight / 2.0;
    }
    bool inside = !task->milestone && label.width + 2.0 * LABEL_GAP <= x1 - x0;
    double labelX = (x0 + x1) / 2.0;
    if (!inside) {
      bool fitsRight = x1 + LABEL_GAP + label.width <= right + rightPadding;
      bool fitsLeft = x0 - LABEL_GAP - label.width >= left;
      labelX = (fitsRight || !fitsLeft) ? (x1 + LABEL_GAP + label.width / 2.0) : (x0 - LABEL_GAP - label.width / 2.0);
      extent = std::max(extent, labelX + label.width / 2.0);
    }
    bars.push_back({task, std::move(label), row, x0, x1, labelX, inside});
  }

  std::string interval = model.tickInterval.value_or(textSetting(config, "tickInterval").value_or(""));
  std::string weekday = model.weekday.value_or(textSetting(config, "weekday").value_or("sunday"));
  int count = std::max(2, std::min(10, static_cast<int>(std::floor(plotWidth / (sampleWidth + 24.0)))));
  auto ivTicks = intervalTicks(interval, weekday, start, stop, static_cast<size_t>(std::ceil(plotWidth / 3.0)));
  std::vector<double> ticks = ivTicks.has_value() ? *ivTicks : autoTicks(start, stop, count);

  std::string grid;
  std::string axisBottom;
  std::string axisTop;
  double lastRight = -std::numeric_limits<double>::infinity();
  std::string lastLabel;
  double bottomY = plotBottom + tickHeight / 2.0 + 2.0;
  double topY = plotTop - gridLead - tickHeight / 2.0 - 2.0;
  for (double t : ticks) {
    double tx = x(t);
    Pele::appendCat(grid, {"M", num(tx), ",", num(plotTop - gridLead), "V", num(plotBottom)});
    std::string label = tickLabel(t);
    double w = measurer.width(label, tickSize, 0);
    if (tx - w / 2.0 < lastRight + 8.0 || label == lastLabel) continue;
    lastRight = tx + w / 2.0;
    lastLabel = label;
    extent = std::max(extent, lastRight);
    std::string body = Pele::cat({"text-anchor=\"middle\">", esc(label), "</text>"});
    Pele::appendCat(axisBottom, {"<text x=\"", num(tx), "\" y=\"", num(bottomY + tickSize * 0.35), "\" ", body});
    if (topAxis) Pele::appendCat(axisTop, {"<text x=\"", num(tx), "\" y=\"", num(topY + tickSize * 0.35), "\" ", body});
  }

  std::string vertLines;
  std::string vertLabels;
  std::vector<double> vertEnds;
  double vertTop = plotBottom + tickHeight + 4.0;
  double vertLine = std::round(labelSize * 1.5);
  std::vector<const GanttTask*> sortedVerts = verts;
  std::stable_sort(sortedVerts.begin(), sortedVerts.end(), [](const GanttTask* a, const GanttTask* b) {
    return a->startTime->getTime() < b->startTime->getTime();
  });
  for (const auto* task : sortedVerts) {
    double vx = x(task->startTime->getTime());
    Label label = oneLine(task->task, measurer, labelSize);
    double cx = std::max(pad + label.width / 2.0, vx);
    size_t line = 0;
    while (line < vertEnds.size() && cx - label.width / 2.0 < vertEnds[line] + 8.0) line++;
    if (line == vertEnds.size()) vertEnds.push_back(cx + label.width / 2.0);
    else vertEnds[line] = cx + label.width / 2.0;
    extent = std::max(extent, vertEnds[line]);
    Pele::appendCat(vertLines, {"<path class=\"pele-vert\" data-id=\"", escText(task->id), "\" d=\"M", num(vx), ",", num(plotTop - gridLead), "V", num(plotBottom), "\"/>"});
    vertLabels += labelSvg(label,  cx,  vertTop + static_cast<double>(line) * vertLine + vertLine / 2.0, Pele::cat({" class=\"pele-vert-label\" data-id=\"", escText(task->id), "\""}));
  }

  double width = std::ceil(std::max(extent, pad + title.width) + pad);
  double height = std::ceil(plotBottom + tickHeight + (!vertEnds.empty() ? 4.0 + static_cast<double>(vertEnds.size()) * vertLine : 0.0) + pad);

  std::string bands;
  std::string sectionOut;
  for (int row = 0; row < rowCount; ) {
    const std::string& section = rowSection[row];
    int end = row + 1;
    while (end < rowCount && rowSection[end] == section) end++;
    int index = categories[section] % sectionStyles;
    double y = plotTop + row * rowHeight;
    double h = (end - row) * rowHeight;
    if (categories.size() > 1 || !section.empty()) {
      bands += "<rect class=\"pele-section pele-section-" + std::to_string(index) + "\" x=\"0\" y=\"" + num(y) + "\" width=\"" + num(width) + "\" height=\"" + num(h) + "\" fill=\"" +
               std::string(index % 2 == 0 ? "var(--_s)" : "none") + "\"/>";
    }
    const Label& label = sectionLabels[section];
    sectionOut += labelSvg(label,  pad + 8.0 + label.width / 2.0,  y + h / 2.0, Pele::cat({" class=\"pele-section-label\" data-id=\"", escText(section), "\""}),  options.icons);
    row = end;
  }

  std::string excluded;
  if ((!model.excludes.empty() || !model.includes.empty()) && add(GanttDate(start), 6.0, "y").getTime() > stop) {
    std::optional<GanttDate> from;
    std::optional<GanttDate> to;
    auto flush = [&]() {
      if (!from.has_value() || !to.has_value()) return;
      GanttDate dayStart = *from;
      dayStart.setHours(0, 0, 0, 0);
      GanttDate dayEnd = *to;
      dayEnd.setHours(23, 59, 59, 999);
      double ex0 = std::max(left, x(dayStart.getTime()));
      double ex1 = std::min(right, x(dayEnd.getTime()));
      if (ex1 > ex0) {
        Pele::appendCat(excluded, {"<rect x=\"", num(ex0), "\" y=\"", num(plotTop), "\" width=\"", num(ex1 - ex0), "\" height=\"", num(plotHeight), "\"/>"});
      }
      from.reset();
      to.reset();
    };
    for (GanttDate day(start); day.getTime() <= stop; day = addDays(day, 1)) {
      if (model.isInvalidDate(day)) {
        if (!from.has_value()) from = day;
        to = day;
      } else {
        flush();
      }
    }
    flush();
  }

  std::vector<LinkInfo> links;
  std::string tasksOut;
  for (const auto& bar : bars) {
    const GanttTask& task = *bar.task;
    double cy = plotTop + bar.row * rowHeight + rowHeight / 2.0;
    std::string state = task.active ? "active" : task.done ? "done" : "";
    std::string classes = "pele-task";
    if (!state.empty()) Pele::appendCat(classes, {" pele-", state});
    if (task.crit) classes += " pele-crit";
    if (task.milestone) classes += " pele-milestone";
    if (std::find(task.classes.begin(), task.classes.end(), "clickable") != task.classes.end()) {
      classes += " pele-clickable";
    }

    std::string shape;
    std::string textFill;
    if (std::isnan(bar.x0)) {
      classes += " pele-unscheduled";
      textFill = " fill=\"var(--_m)\"";
    } else {
      std::string color = task.crit ? CRITICAL : "var(--_c)";
      std::string paint = (state == "done")
        ? "fill=\"var(--_a)\""
        : (state == "active")
          ? Pele::cat({"fill=\"", color, "\" fill-opacity=\"0.25\""})
          : Pele::cat({"fill=\"", color, "\""});
      std::string stroke = "stroke=\"" + std::string(state == "done" && !task.crit ? "var(--_b)" : color) + "\"";
      if (task.milestone) {
        double cx = (bar.x0 + bar.x1) / 2.0;
        double r = barHeight / 2.0;
        shape = Pele::cat({"<path d=\"M", num(cx), ",", num(cy - r), "L", num(cx + r), ",", num(cy), "L", num(cx), ",", num(cy + r), "L", num(cx - r), ",", num(cy), "Z\" ", paint, " ", stroke, " stroke-linejoin=\"round\"/>"});
      } else {
        shape = "<rect x=\"" + num(bar.x0) + "\" y=\"" + num(cy - barHeight / 2.0) + "\" width=\"" + num(bar.x1 - bar.x0) + "\" height=\"" + num(barHeight) + "\" " + paint + " " + stroke + std::string(ROUNDED) + "/>";
      }
      if (bar.inside) textFill = state.empty() ? " fill=\"var(--_bg)\"" : "";
      else if (state == "done") textFill = " fill=\"var(--_m)\"";
    }

    std::string body = shape + labelSvg(bar.label,  bar.labelX,  cy, Pele::cat({" class=\"pele-label\"", textFill}));
    auto linkIt = model.links.find(task.id);
    if (linkIt != model.links.end()) {
      body = linked(body, linkIt->second, task.id, links, options);
    }
    Pele::appendCat(tasksOut, {"<g class=\"", classes, "\" data-id=\"", escText(task.id), "\">", body, "</g>"});
  }

  std::string today;
  if (model.todayMarker != "off" && now >= start && now <= stop && stop > start) {
    double tx = x(now);
    double top = plotTop - gridLead;
    std::string style;
    if (!model.todayMarker.empty()) {
      std::vector<std::string> parts;
      size_t pos = 0;
      for (;;) {
        size_t comma = model.todayMarker.find(',', pos);
        if (comma == std::string::npos) { parts.push_back(model.todayMarker.substr(pos)); break; }
        parts.push_back(model.todayMarker.substr(pos, comma - pos));
        pos = comma + 1;
      }
      style = resolveStyle(parts).line;
    }
    today = Pele::cat({"<g class=\"pele-today\"><path d=\"M", num(tx), ",", num(top), "V", num(plotBottom), "\" fill=\"none\" stroke=\"var(--_c)\" stroke-width=\"2\"", style, "/>", (!style.empty() ? "" : "<path d=\"M" + num(tx - 4.0) + "," + num(top - 5.0) + "H" + num(tx + 4.0) + "L" + num(tx) + "," + num(top + 1.0) + "Z\" fill=\"var(--_c)\"/>"), "</g>"});
  }

  std::string axisLine = Pele::cat({"M", num(left), ",", num(plotBottom), "H", num(right), (topAxis ? Pele::cat({"M", num(left), ",", num(plotTop - gridLead), "H", num(right)}) : "")});
  std::string content = Pele::cat({labelSvg(title, width / 2.0, pad + title.height / 2.0, " class=\"pele-title\" font-weight=\"var(--_tw)\""), (!bands.empty() ? Pele::cat({"<g class=\"pele-sections\">", bands, "</g>"}) : ""), (!excluded.empty() ? Pele::cat({"<g class=\"pele-excluded\" fill=\"var(--_a)\">", excluded, "</g>"}) : ""), "<g class=\"pele-grid\" fill=\"none\"><path d=\"", grid, "\" stroke=\"var(--_b)\" stroke-dasharray=\"1 3\"/><path d=\"", axisLine, "\" stroke=\"var(--_b)\"/></g>", (!vertLines.empty() ? Pele::cat({"<g class=\"pele-verts\" fill=\"none\" stroke=\"var(--_l)\" stroke-width=\"1.5\" stroke-dasharray=\"4 3\">", vertLines, "</g>"}) : ""), today, "<g class=\"pele-tasks\" font-size=\"", num(labelSize), "\">", tasksOut, "</g>", "<g class=\"pele-section-labels\" font-size=\"", num(sectionSize), "\">", sectionOut, "</g>", "<g class=\"pele-axis\" font-size=\"", num(tickSize), "\" fill=\"var(--_m)\">", axisBottom, axisTop, "</g>", (!vertLabels.empty() ? Pele::cat({"<g class=\"pele-vert-labels\" font-size=\"", num(labelSize), "\">", vertLabels, "</g>"}) : "")});

  return {svgDocument("gantt", width, height, size, options, model, content), width, height, links};
}

} // namespace gantt_detail

class GanttDiagram {
public:
  static Rendered render(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options) {
    gantt_detail::GanttDb db;
    if (!title.empty()) db.title = title;
    const auto* sec = config.get("gantt");
    if (sec && sec->isMap()) {
      const auto* dm = sec->get("displayMode");
      if (dm && dm->isString()) db.displayMode = dm->sVal;
    }
    gantt_detail::parseGantt(source, db);
    return gantt_detail::renderGanttModel(db, config, options);
  }
};

inline Rendered renderGantt(const std::string& source, const Config& config, const std::string& title, const RenderOptions& options) {
  return GanttDiagram::render(source, config, title, options);
}

inline Rendered renderGantt(const std::string& raw, RenderOptions options = {}) {
  auto pre = preprocess(raw);
  std::string parsed = Pele::cat({encodeEntities(pre.text), "\n"});
  const auto* section = pre.config.get("gantt");
  bool fixed = (section && section->isMap() && section->get("useMaxWidth") && section->get("useMaxWidth")->isBool() && !section->get("useMaxWidth")->bVal);
  if (!options.hasResponsive) {
    options.responsive = !fixed;
    options.hasResponsive = true;
  }
  return GanttDiagram::render(parsed, pre.config, pre.title, options);
}

} // namespace Pele
