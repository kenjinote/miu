#pragma once
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <cmath>
#include <algorithm>
#include <functional>
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <regex>
#include <optional>

namespace Pele {

struct Size {
    double width = 0;
    double height = 0;
};

inline int charCodeAt(const std::string& s, int pos) {
    if (pos < 0 || static_cast<size_t>(pos) >= s.size()) return -1;
    return static_cast<unsigned char>(s[pos]);
}

// ============================================================================
// UTF-8 / UTF-16 String Utilities
// ============================================================================

inline uint32_t nextCodepoint(const std::string& s, size_t& i) {
    if (i >= s.size()) return 0;
    unsigned char c = static_cast<unsigned char>(s[i++]);
    if (c < 0x80) return c;
    if ((c >> 5) == 0x6 && i < s.size()) {
        uint32_t cp = ((c & 0x1F) << 6) | (static_cast<unsigned char>(s[i++]) & 0x3F);
        return cp;
    }
    if ((c >> 4) == 0xE && i + 1 < s.size()) {
        uint32_t cp = ((c & 0x0F) << 12) | ((static_cast<unsigned char>(s[i++]) & 0x3F) << 6);
        cp |= (static_cast<unsigned char>(s[i++]) & 0x3F);
        return cp;
    }
    if ((c >> 3) == 0x1E && i + 2 < s.size()) {
        uint32_t cp = ((c & 0x07) << 18) | ((static_cast<unsigned char>(s[i++]) & 0x3F) << 12);
        cp |= ((static_cast<unsigned char>(s[i++]) & 0x3F) << 6);
        cp |= (static_cast<unsigned char>(s[i++]) & 0x3F);
        return cp;
    }
    return c;
}

inline void appendCodepoint(std::string& out, uint32_t cp) {
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0x10FFFF) {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

inline std::vector<uint16_t> toUtf16(const std::string& s) {
    std::vector<uint16_t> out;
    out.reserve(s.size());
    size_t i = 0;
    while (i < s.size()) {
        uint32_t cp = nextCodepoint(s, i);
        if (cp <= 0xFFFF) {
            out.push_back(static_cast<uint16_t>(cp));
        } else if (cp <= 0x10FFFF) {
            cp -= 0x10000;
            out.push_back(static_cast<uint16_t>(0xD800 | (cp >> 10)));
            out.push_back(static_cast<uint16_t>(0xDC00 | (cp & 0x3FF)));
        }
    }
    return out;
}

inline std::string fromUtf16(const uint16_t* data, size_t len) {
    std::string out;
    out.reserve(len);
    size_t i = 0;
    while (i < len) {
        uint32_t c = data[i++];
        if (c >= 0xD800 && c <= 0xDBFF && i < len && data[i] >= 0xDC00 && data[i] <= 0xDFFF) {
            uint32_t low = data[i++];
            uint32_t cp = 0x10000 + ((c - 0xD800) << 10) + (low - 0xDC00);
            appendCodepoint(out, cp);
        } else {
            appendCodepoint(out, c);
        }
    }
    return out;
}

// ============================================================================
// src/util/chars.ts
// ============================================================================

inline bool isSpace(int c) {
    if (c < 0) return false;
    if (c < 128) return c == 32 || (c >= 9 && c <= 13);
    return (
        c == 0xa0 ||
        c == 0x1680 ||
        (c >= 0x2000 && c <= 0x200a) ||
        c == 0x2028 ||
        c == 0x2029 ||
        c == 0x202f ||
        c == 0x205f ||
        c == 0x3000 ||
        c == 0xfeff
    );
}

inline bool isBlank(int c) {
    return c == 32 || c == 9;
}

inline bool isLineEnd(int c) {
    return c == 10 || c == 13 || c == 0x2028 || c == 0x2029;
}

inline bool isDigit(int c) {
    return c >= 48 && c <= 57;
}

inline bool isLetter(int c) {
    return (c >= 65 && c <= 90) || (c >= 97 && c <= 122);
}

inline bool isHex(int c) {
    return (c >= 48 && c <= 57) || (c >= 65 && c <= 70) || (c >= 97 && c <= 102);
}

inline bool isWord(int c) {
    return (c >= 48 && c <= 57) || (c >= 65 && c <= 90) || (c >= 97 && c <= 122) || c == 95;
}

inline int charCodeAtUtf8(const std::string& s, size_t pos, size_t* nextPos = nullptr) {
    if (pos >= s.size()) {
        if (nextPos) *nextPos = pos;
        return -1;
    }
    size_t i = pos;
    uint32_t cp = nextCodepoint(s, i);
    if (nextPos) *nextPos = i;
    return static_cast<int>(cp);
}

inline std::string trimStartStr(const std::string& s) {
    size_t i = 0;
    while (i < s.size()) {
        size_t next = i;
        int cp = charCodeAtUtf8(s, i, &next);
        if (!isSpace(cp)) break;
        i = next;
    }
    return s.substr(i);
}

inline std::string trimEndStr(const std::string& s) {
    size_t end = s.size();
    while (end > 0) {
        unsigned char b = static_cast<unsigned char>(s[end - 1]);
        if (b < 0x80) {
            if (!isSpace(b)) break;
            end--;
        } else {
            size_t start = end - 1;
            while (start > 0 && (static_cast<unsigned char>(s[start]) & 0xC0) == 0x80) start--;
            size_t dummy = start;
            int cp = charCodeAtUtf8(s, start, &dummy);
            if (!isSpace(cp)) break;
            end = start;
        }
    }
    return s.substr(0, end);
}

inline std::string trimStr(const std::string& s) {
    return trimEndStr(trimStartStr(s));
}

inline std::string toLowerAscii(const std::string& s) {
    std::string out = s;
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
    }
    return out;
}

inline std::vector<std::string> splitStr(const std::string& s, char delim) {
    std::vector<std::string> out;
    size_t start = 0;
    while (true) {
        size_t pos = s.find(delim, start);
        if (pos == std::string::npos) {
            out.push_back(s.substr(start));
            break;
        }
        out.push_back(s.substr(start, pos - start));
        start = pos + 1;
    }
    return out;
}

inline bool startsWith(const std::string& s, const std::string& prefix, size_t pos = 0) {
    if (pos + prefix.size() > s.size()) return false;
    return std::memcmp(s.data() + pos, prefix.data(), prefix.size()) == 0;
}

inline bool endsWith(const std::string& s, const std::string& suffix) {
    if (suffix.size() > s.size()) return false;
    return std::memcmp(s.data() + s.size() - suffix.size(), suffix.data(), suffix.size()) == 0;
}

inline std::string replaceAllStr(std::string s, const std::string& from, const std::string& to) {
    if (from.empty()) return s;
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
    return s;
}

// ============================================================================
// src/util/unicode.ts
// ============================================================================

inline const std::vector<uint16_t>& unicodeLetterTable() {
    static const std::vector<uint16_t> table = []() {
        const char* PACKED =
            "4q 0 b 0 5 0 6 m 2 u 2 cp 5 b f 4 8 0 2 0 3m 4 2 1 3 3 9 0 2 2 2 0 2 j 2 2a 2 3u 9 4d a 11 3 0 8 12 21 q 6 2 1a 16 10 1 2 2q 2 0 g 1 8 1 b 2 3 0 h 0 2 t u 2g c 0 p w a 1 5 0 6 l 5 0 a 0 4 0 o o 20 0 2 a 2g 1h 4 0 j 0 8 9 g 6 2 6 6 7 3 1 3 l 2 6 2 0 4 3 4 0 h 0 e 1 2 2 f 1 k 5 5 1 3 l 2 6 2 1 2 1 2 1 w 3 2 0 k 2 h 8 2 2 2 l 2 6 2 1 2 4 4 0 j 0 g 1 10 7 3 1 3 l 2 6 2 1 2 4 4 0 v 1 2 2 g 0 i 0 2 5 4 2 2 3 4 1 2 0 2 1 4 1 4 2 4 b n 0 1h 7 2 2 2 m 2 9 2 4 4 0 r 1 7 1 10 7 2 2 2 m 2 9 2 4 4 0 x 0 2 1 g 1 j 7 2 2 2 14 3 0 h 0 i 1 p 5 6 h 4 n 2 8 2 0 3 6 1n 1b 2 1 d 6 1n 1 2 0 3 1 2 0 3 0 7 3 2 6 2 2 2 0 2 0 3 1 2 3 2 1 a 0 3 4 2 0 m 3 x 0 1s 7 2 z s 4 38 16 l 0 h 5 5 3 4 0 4 1 8 2 5 c d 0 i 11 2 0 6 0 3 16 2 98 2 3 3 6 2 0 2 3 3 14 2 3 3 w 2 3 3 6 2 0 2 3 3 e 2 1k 2 3 3 1u 12 f h 2c d h7 3 g 2 p 6 22 m c 2 3 f h f h f c 2 2 g 1f 10 0 5 0 1w 2f 9 14 2 0 6 1x b s 1g t 3 4 c 17 m 6 1l m a 1g 2b 0 2m 1a i 6 1k t e 1 b 17 r z 16 2 b z 30 3 2 3 4 1 a 5b 1t 7p 3 5 3 11 3 5 3 7 2 0 2 0 2 0 2 u 3 1g 2 6 2 0 4 2 2 6 4 3 3 5 5 c 6 2 2 6 39 0 e 0 h c 2u 0 5 0 3 9 2 0 4 4 7 0 2 0 2 0 2 3 2 a 3 3 6 4 5 0 1h 1 22k 1a 2 1a 2 3o 7 3 4 1 d 11 2 0 6 0 3 1j 8 0 h m a 6 2 6 2 6 2 6 2 6 2 6 2 6 2 6 29 0 d2 1 17 4 6 1 5 2d 7 2 2 2h 2 3 6 14 4 2l i q 1i f e9 52t 23 g5o 1g wc 1w 19 3 7g 4 f b 1 l 1a h o 9 1x 1e 8 3 2u 3 3 2 3 d a 26 9 2 2 2 3 2 m u 1f f 1d 1r 5 4 0 f r b m q s 8 1a t 0 1d 14 o 2 2 7 l m 4 0 6 1b 2 0 4 1 3 4 3 0 2 0 p 2 3 a 8 2 d 5 3 5 3 5 a 6 2 6 42 y u 8mb d m 5 1c 6it a5 3 2x 13 6 d 4 6 0 2 9 2 c 2 4 2 0 2 1 2 1 2 2z y a2 j 1r 3 1h 15 b 39 4 2 3q 11 p 7 p c 2g 4 5 3 5 3 5 3 2";
        auto parts = splitStr(PACKED, ' ');
        std::vector<uint16_t> out(parts.size());
        uint32_t prev = 0;
        for (size_t i = 0; i + 1 < parts.size(); i += 2) {
            uint32_t start = prev + static_cast<uint32_t>(std::stoul(parts[i], nullptr, 36));
            prev = start + static_cast<uint32_t>(std::stoul(parts[i + 1], nullptr, 36));
            out[i] = static_cast<uint16_t>(start);
            out[i + 1] = static_cast<uint16_t>(prev);
        }
        return out;
    }();
    return table;
}

inline bool isUnicodeLetter(int code) {
    if (code < 0xaa || code > 0xffdc) return false;
    const auto& t = unicodeLetterTable();
    int lo = 0;
    int hi = static_cast<int>(t.size() >> 1) - 1;
    while (lo <= hi) {
        int mid = (lo + hi) >> 1;
        if (code < t[mid * 2]) hi = mid - 1;
        else if (code > t[mid * 2 + 1]) lo = mid + 1;
        else return true;
    }
    return false;
}

// ============================================================================
// src/text/entities.ts
// ============================================================================

inline const std::unordered_map<std::string, std::string>& namedEntitiesMap() {
    static const std::unordered_map<std::string, std::string> m = []() {
        const std::string NAMED =
            "amp:38,lt:60,gt:62,quot:34,apos:39,nbsp:160,shy:173,iexcl:161,cent:162,pound:163,curren:164,yen:165,sect:167,"
            "copy:169,laquo:171,not:172,reg:174,deg:176,plusmn:177,sup2:178,sup3:179,micro:181,para:182,middot:183,"
            "sup1:185,raquo:187,frac14:188,frac12:189,frac34:190,iquest:191,times:215,divide:247,ndash:8211,mdash:8212,"
            "lsquo:8216,rsquo:8217,sbquo:8218,ldquo:8220,rdquo:8221,bdquo:8222,dagger:8224,Dagger:8225,bull:8226,"
            "hellip:8230,permil:8240,prime:8242,Prime:8243,lsaquo:8249,rsaquo:8250,euro:8364,trade:8482,larr:8592,"
            "uarr:8593,rarr:8594,darr:8595,harr:8596,lArr:8656,uArr:8657,rArr:8658,dArr:8659,hArr:8660,forall:8704,"
            "part:8706,exist:8707,empty:8709,nabla:8711,isin:8712,notin:8713,ni:8715,prod:8719,sum:8721,minus:8722,"
            "radic:8730,infin:8734,and:8743,or:8744,cap:8745,cup:8746,int:8747,there4:8756,sim:8764,cong:8773,"
            "asymp:8776,ne:8800,equiv:8801,le:8804,ge:8805,sub:8834,sup:8835,sube:8838,supe:8839,oplus:8853,otimes:8855,"
            "perp:8869,sdot:8901,loz:9674,spades:9824,clubs:9827,hearts:9829,diams:9830,check:10003,cross:10007,"
            "AMP:38,LT:60,GT:62,QUOT:34,lsqb:91,rsqb:93,lcub:123,rcub:125,verbar:124,VerticalLine:124,midast:42,"
            "DiacriticalGrave:96,Hat:94,UnderBar:95,"
            "equals:61,colon:58,semi:59,num:35,percnt:37,lpar:40,rpar:41,lbrack:91,rbrack:93,lbrace:123,rbrace:125,"
            "vert:124,sol:47,bsol:92,quest:63,excl:33,ast:42,plus:43,comma:44,period:46,dollar:36,commat:64,grave:96,"
            "hat:94,lowbar:95,tilde:126,Alpha:913,Beta:914,Gamma:915,Delta:916,Epsilon:917,Zeta:918,Eta:919,Theta:920,"
            "Iota:921,Kappa:922,Lambda:923,Mu:924,Nu:925,Xi:926,Omicron:927,Pi:928,Rho:929,Sigma:931,Tau:932,"
            "Upsilon:933,Phi:934,Chi:935,Psi:936,Omega:937,alpha:945,beta:946,gamma:947,delta:948,epsilon:949,zeta:950,"
            "eta:951,theta:952,iota:953,kappa:954,lambda:955,mu:956,nu:957,xi:958,omicron:959,pi:960,rho:961,sigmaf:962,"
            "sigma:963,tau:964,upsilon:965,phi:966,chi:967,psi:968,omega:969";
        std::unordered_map<std::string, std::string> res;
        for (const auto& pair : splitStr(NAMED, ',')) {
            size_t at = pair.find(':');
            if (at != std::string::npos) {
                uint32_t cp = static_cast<uint32_t>(std::stoul(pair.substr(at + 1)));
                std::string s;
                appendCodepoint(s, cp);
                res[pair.substr(0, at)] = s;
            }
        }
        return res;
    }();
    return m;
}

// In UTF-8: 'ﬂ' = "\xEF\xAC\x82", '°' = "\xC2\xB0", '¶' = "\xC2\xB6", 'ß' = "\xC3\x9F"
inline const std::string PLACEHOLDER_OPEN = "\xEF\xAC\x82\xC2\xB0";
inline const std::string PLACEHOLDER_CLOSE = "\xC2\xB6\xC3\x9F";

inline std::string restorePlaceholders(const std::string& text) {
    std::string out;
    size_t last = 0;
    size_t from = text.find(PLACEHOLDER_OPEN);
    while (from != std::string::npos) {
        size_t stop = text.find(PLACEHOLDER_CLOSE, from + PLACEHOLDER_OPEN.size());
        if (stop == std::string::npos) break;
        bool numeric = startsWith(text, "\xC2\xB0", from + PLACEHOLDER_OPEN.size());
        size_t innerStart = from + PLACEHOLDER_OPEN.size() + (numeric ? 2 : 0);
        out += text.substr(last, from - last) + (numeric ? "&#" : "&") + text.substr(innerStart, stop - innerStart) + ";";
        last = stop + PLACEHOLDER_CLOSE.size();
        from = text.find(PLACEHOLDER_OPEN, last);
    }
    return last == 0 ? text : out + text.substr(last);
}

inline std::string writtenCodes(const std::string& text) {
    if (text.find(PLACEHOLDER_CLOSE) == std::string::npos) return text;
    std::string out;
    size_t last = 0;
    size_t from = text.find(PLACEHOLDER_OPEN);
    while (from != std::string::npos) {
        size_t stop = text.find(PLACEHOLDER_CLOSE, from + PLACEHOLDER_OPEN.size());
        if (stop == std::string::npos) break;
        bool numeric = startsWith(text, "\xC2\xB0", from + PLACEHOLDER_OPEN.size());
        size_t innerStart = from + PLACEHOLDER_OPEN.size() + (numeric ? 2 : 0);
        out += text.substr(last, from - last) + "#" + text.substr(innerStart, stop - innerStart) + ";";
        last = stop + PLACEHOLDER_CLOSE.size();
        from = text.find(PLACEHOLDER_OPEN, last);
    }
    return last == 0 ? text : out + text.substr(last);
}

inline std::string decodeEntities(std::string text) {
    if (text.find(PLACEHOLDER_CLOSE) != std::string::npos) {
        text = restorePlaceholders(text);
    }
    if (text.find('&') == std::string::npos) return text;
    std::string out;
    out.reserve(text.size());
    const auto& named = namedEntitiesMap();
    size_t i = 0;
    while (i < text.size()) {
        if (text[i] == '&') {
            size_t p = i + 1;
            if (p < text.size() && text[p] == '#') {
                p++;
                if (p < text.size() && (text[p] == 'x' || text[p] == 'X')) {
                    p++;
                    size_t start = p;
                    while (p < text.size() && isHex(static_cast<unsigned char>(text[p]))) p++;
                    if (p > start && p < text.size() && text[p] == ';') {
                        std::string m = text.substr(i, p - i + 1);
                        try {
                            uint64_t n = std::stoull(text.substr(start, p - start), nullptr, 16);
                            if (n > 0 && n <= 0x10ffff && (n < 0xd800 || n > 0xdfff)) appendCodepoint(out, static_cast<uint32_t>(n));
                            else out += m;
                        } catch (...) { out += m; }
                        i = p + 1;
                        continue;
                    }
                } else {
                    size_t start = p;
                    while (p < text.size() && isDigit(static_cast<unsigned char>(text[p]))) p++;
                    if (p > start && p < text.size() && text[p] == ';') {
                        std::string m = text.substr(i, p - i + 1);
                        try {
                            uint64_t n = std::stoull(text.substr(start, p - start), nullptr, 10);
                            if (n > 0 && n <= 0x10ffff && (n < 0xd800 || n > 0xdfff)) appendCodepoint(out, static_cast<uint32_t>(n));
                            else out += m;
                        } catch (...) { out += m; }
                        i = p + 1;
                        continue;
                    }
                }
            } else {
                size_t start = p;
                while (p < text.size() && isWord(static_cast<unsigned char>(text[p]))) p++;
                if (p > start && p < text.size() && text[p] == ';') {
                    std::string name = text.substr(start, p - start);
                    auto it = named.find(name);
                    if (it != named.end()) out += it->second;
                    else out += text.substr(i, p - i + 1);
                    i = p + 1;
                    continue;
                }
            }
        }
        out.push_back(text[i++]);
    }
    return out;
}

// ============================================================================
// src/types.ts & src/errors.ts
// ============================================================================

struct LinkInfo {
    std::string id;
    std::string href;
    bool internal = false;
};

struct MathMarkup {
    double width = 0.0;
    double height = 0.0;
    std::string svg;
    std::string html;
    bool hasSvg = false;
};

struct TextMeasurer {
    virtual ~TextMeasurer() = default;
    virtual double width(const std::string& text, double fontSize, int style = 0) const = 0;
};

using IconResolver = std::function<std::string(const std::string&)>;
using MathMarkupResolver = std::function<bool(const std::string& tex, double fontSize, MathMarkup& out)>;

struct RenderOptions {
    double fontSize = 16.0;
    bool hasFontSize = false;
    std::string fontFamily;
    std::string fontFamilyMono;
    double padding = 8.0;
    bool hasPadding = false;
    double maxWidth = 0.0;
    bool hasMaxWidth = false;
    bool responsive = true;
    bool hasResponsive = false;
    bool links = true;
    std::vector<std::string> linkSchemes;
    std::string linkRel = "noopener";
    bool images = true;
    std::vector<std::string> imageSchemes;
    const TextMeasurer* measurer = nullptr;
    IconResolver icons;
    MathMarkupResolver math;
};

struct Rendered {
    std::string svg;
    double width = 0.0;
    double height = 0.0;
    std::vector<LinkInfo> links;
};

struct DiagramMeta {
    std::string title;
    std::string accTitle;
    std::string accDescr;
};

class PeleError : public std::runtime_error {
public:
    std::string code;
    std::string type;
    std::string mermaidType;
    int line = 0;
    int column = 0;
    std::string snippet;

    PeleError(const std::string& msg, const std::string& code_ = "syntax", const std::string& type_ = "", int line_ = 0, int col_ = 0, const std::string& snip_ = "")
        : std::runtime_error(msg), code(code_), type(type_), line(line_), column(col_), snippet(snip_) {}
};

inline PeleError syntaxError(const std::string& type, const std::string& src, size_t offset, const std::string& detail, bool lexical = false) {
    size_t end = trimEndStr(src).size();
    if (end > 0 && offset >= end) offset = end;
    int line = 1;
    size_t lineStart = 0;
    for (size_t i = src.find('\n'); i != std::string::npos && i < offset; i = src.find('\n', i + 1)) {
        line++;
        lineStart = i + 1;
    }
    size_t lineEnd = src.find('\n', offset);
    if (lineEnd == std::string::npos) lineEnd = src.size();
    int column = static_cast<int>(offset >= lineStart ? offset - lineStart + 1 : 1);
    std::string text = src.substr(lineStart, lineEnd - lineStart);
    std::string snippet = text + "\n" + std::string(std::max(0, column - 1), '-') + "^";
    std::string head = lexical
        ? ("Lexical error on line " + std::to_string(line) + ". Unrecognized text.")
        : ("Parse error on line " + std::to_string(line) + ":");
    std::string message = lexical ? (head + "\n" + snippet) : (head + "\n" + snippet + "\n" + detail);
    return PeleError(message, "syntax", type, line, column, snippet);
}

// ============================================================================
// src/util/yaml.ts & src/preprocess.ts & src/detect.ts
// ============================================================================

struct YamlValue {
    enum Type { Null, Bool, Number, String, Array, Map } type = Null;
    bool bVal = false;
    double nVal = 0.0;
    std::string sVal;
    std::vector<YamlValue> aVal;
    std::map<std::string, YamlValue> mVal;

    YamlValue() = default;
    static YamlValue makeNull() { return YamlValue(); }
    static YamlValue makeBool(bool b) { YamlValue v; v.type = Bool; v.bVal = b; return v; }
    static YamlValue makeNumber(double n) { YamlValue v; v.type = Number; v.nVal = n; return v; }
    static YamlValue makeString(const std::string& s) { YamlValue v; v.type = String; v.sVal = s; return v; }
    static YamlValue makeArray() { YamlValue v; v.type = Array; return v; }
    static YamlValue makeMap() { YamlValue v; v.type = Map; return v; }

    bool isNull() const { return type == Null; }
    bool isBool() const { return type == Bool; }
    bool isNumber() const { return type == Number; }
    bool isString() const { return type == String; }
    bool isArray() const { return type == Array; }
    bool isMap() const { return type == Map; }

    std::string asString() const {
        if (type == String) return sVal;
        if (type == Bool) return bVal ? "true" : "false";
        if (type == Number) {
            std::ostringstream oss;
            oss << nVal;
            return oss.str();
        }
        return "";
    }

    const YamlValue* get(const std::string& k) const {
        if (type != Map) return nullptr;
        auto it = mVal.find(k);
        return it == mVal.end() ? nullptr : &it->second;
    }

    YamlValue& operator[](const std::string& k) {
        if (type != Map) { type = Map; mVal.clear(); }
        return mVal[k];
    }
};

using Config = YamlValue;

inline YamlValue yamlScalar(const std::string& raw) {
    std::string s = trimStr(raw);
    if (s.empty() || s == "~" || s == "null" || s == "Null" || s == "NULL") return YamlValue::makeNull();
    if (s == "true" || s == "True" || s == "TRUE") return YamlValue::makeBool(true);
    if (s == "false" || s == "False" || s == "FALSE") return YamlValue::makeBool(false);
    char c = s[0];
    if (c == '-' || c == '+' || c == '.' || (c >= '0' && c <= '9')) {
        try {
            size_t idx = 0;
            double d = std::stod(s, &idx);
            if (idx == s.size()) return YamlValue::makeNumber(d);
        } catch (...) {}
    }
    return YamlValue::makeString(s);
}

inline std::pair<YamlValue, size_t> parseYamlFlow(const std::string& s, size_t pos, const std::string& stops, int depth = 0) {
    if (depth > 64) return {YamlValue::makeNull(), s.size()};
    auto skip = [&](size_t p) {
        while (p < s.size()) {
            char c = s[p];
            if (c == ' ' || c == '\n' || c == '\t' || c == '\r') p++;
            else if (c == '#' && (p == 0 || isSpace(static_cast<unsigned char>(s[p - 1])))) {
                size_t nl = s.find('\n', p);
                p = nl == std::string::npos ? s.size() : nl;
            } else break;
        }
        return p;
    };
    pos = skip(pos);
    if (pos >= s.size()) return {YamlValue::makeNull(), pos};
    char ch = s[pos];
    if (ch == '{') {
        YamlValue out = YamlValue::makeMap();
        pos++;
        while (pos < s.size()) {
            pos = skip(pos);
            if (pos >= s.size() || s[pos] == '}') return {out, pos + (pos < s.size() ? 1 : 0)};
            auto [key, nextPos] = parseYamlFlow(s, pos, ":,}", depth + 1);
            pos = skip(nextPos);
            YamlValue val = YamlValue::makeNull();
            if (pos < s.size() && s[pos] == ':') {
                auto [v, vp] = parseYamlFlow(s, pos + 1, ",}", depth + 1);
                val = v;
                pos = vp;
            }
            out.mVal[key.asString()] = val;
            pos = skip(pos);
            if (pos < s.size() && s[pos] == ',') pos++;
        }
        return {out, pos};
    }
    if (ch == '[') {
        YamlValue out = YamlValue::makeArray();
        pos++;
        while (pos < s.size()) {
            pos = skip(pos);
            if (pos >= s.size() || s[pos] == ']') return {out, pos + (pos < s.size() ? 1 : 0)};
            auto [val, vp] = parseYamlFlow(s, pos, ",]", depth + 1);
            out.aVal.push_back(val);
            pos = skip(vp);
            if (pos < s.size() && s[pos] == ',') pos++;
        }
        return {out, pos};
    }
    if (ch == '"' || ch == '\'') {
        size_t end = pos + 1;
        std::string unq;
        while (end < s.size()) {
            if (ch == '"' && s[end] == '\\' && end + 1 < s.size()) {
                char esc = s[end + 1];
                if (esc == 'n') unq.push_back('\n');
                else if (esc == 'r') unq.push_back('\r');
                else if (esc == 't') unq.push_back('\t');
                else unq.push_back(esc);
                end += 2;
            } else if (s[end] == ch) {
                if (ch == '\'' && end + 1 < s.size() && s[end + 1] == '\'') {
                    unq.push_back('\'');
                    end += 2;
                } else break;
            } else {
                unq.push_back(s[end++]);
            }
        }
        return {YamlValue::makeString(unq), end < s.size() ? end + 1 : end};
    }
    size_t end = pos;
    while (end < s.size()) {
        char c = s[end];
        if (stops.find(c) != std::string::npos && (c != ':' || end + 1 >= s.size() || s[end + 1] == ' ' || s[end + 1] == '\t' || s[end + 1] == '\n' || s[end + 1] == ',' || s[end + 1] == '}' || s[end + 1] == ']')) break;
        if (c == '#' && end > pos && isSpace(static_cast<unsigned char>(s[end - 1]))) break;
        end++;
    }
    return {yamlScalar(s.substr(pos, end - pos)), end};
}

inline YamlValue parseYaml(const std::string& src) {
    std::string trimmed = trimStr(src);
    if (trimmed.empty()) return YamlValue::makeNull();
    if (trimmed[0] == '{' || trimmed[0] == '[') {
        return parseYamlFlow(trimmed, 0, "").first;
    }
    auto lines = splitStr(src, '\n');
    size_t i = 0;
    auto parseBlock = [&](auto& self, int minIndent, int depth) -> YamlValue {
        if (depth > 64) return YamlValue::makeNull();
        while (i < lines.size()) {
            std::string t = trimStr(lines[i]);
            if (!t.empty() && t[0] != '#' && t != "---") break;
            i++;
        }
        if (i >= lines.size()) return YamlValue::makeNull();
        auto getIndent = [&](size_t idx) {
            int k = 0;
            while (k < static_cast<int>(lines[idx].size()) && lines[idx][k] == ' ') k++;
            return k;
        };
        int indent = getIndent(i);
        if (indent < minIndent) return YamlValue::makeNull();
        std::string line = lines[i].substr(indent);
        if (line == "-" || startsWith(line, "- ")) {
            YamlValue out = YamlValue::makeArray();
            while (i < lines.size()) {
                while (i < lines.size() && (trimStr(lines[i]).empty() || trimStr(lines[i])[0] == '#')) i++;
                if (i >= lines.size() || getIndent(i) != indent) break;
                std::string cur = lines[i].substr(indent);
                if (cur != "-" && !startsWith(cur, "- ")) break;
                std::string rest = trimStr(cur.substr(1));
                i++;
                if (rest.empty()) out.aVal.push_back(self(self, indent + 1, depth + 1));
                else out.aVal.push_back(parseYamlFlow(rest, 0, "").first);
            }
            return out;
        }
        YamlValue out = YamlValue::makeMap();
        while (i < lines.size()) {
            while (i < lines.size() && (trimStr(lines[i]).empty() || trimStr(lines[i])[0] == '#')) i++;
            if (i >= lines.size()) break;
            int k = getIndent(i);
            if (k < indent) break;
            if (k > indent) { i++; continue; }
            std::string cur = lines[i].substr(indent);
            size_t colon = cur.find(':');
            if (colon == std::string::npos) { i++; continue; }
            std::string key = trimStr(cur.substr(0, colon));
            if (key.size() >= 2 && ((key.front() == '"' && key.back() == '"') || (key.front() == '\'' && key.back() == '\''))) {
                key = key.substr(1, key.size() - 2);
            }
            std::string rest = trimStr(cur.substr(colon + 1));
            if (!rest.empty() && rest[0] == '#') rest.clear();
            i++;
            if (rest.empty()) {
                out.mVal[key] = self(self, indent + 1, depth + 1);
            } else {
                out.mVal[key] = parseYamlFlow(rest, 0, "").first;
            }
        }
        return out;
    };
    return parseBlock(parseBlock, 0, 0);
}

inline void mergeConfig(Config& target, const Config& source) {
    if (!source.isMap()) return;
    if (!target.isMap()) target = YamlValue::makeMap();
    for (const auto& [k, v] : source.mVal) {
        if (k == "__proto__" || k == "constructor" || k == "prototype") continue;
        if (v.isMap() && target.mVal[k].isMap()) {
            mergeConfig(target.mVal[k], v);
        } else {
            target.mVal[k] = v;
        }
    }
}

struct Preprocessed {
    std::string text;
    std::string title;
    bool hasTitle = false;
    Config config = YamlValue::makeMap();
    std::string withComments;
    int lineOffset = 0;
};

inline std::string normalizeLineBreaks(const std::string& text) {
    if (text.find('\r') == std::string::npos) return text;
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); i++) {
        if (text[i] == '\r') {
            if (i + 1 < text.size() && text[i + 1] == '\n') i++;
            out.push_back('\n');
        } else {
            out.push_back(text[i]);
        }
    }
    return out;
}

inline std::string cleanupComments(const std::string& text) {
    if (text.find("%%") == std::string::npos) return trimStartStr(text);
    std::string out;
    size_t i = 0;
    while (i < text.size()) {
        size_t nl = text.find('\n', i);
        size_t lineEnd = (nl == std::string::npos) ? text.size() : nl + 1;
        std::string line = text.substr(i, lineEnd - i);
        std::string trimmed = trimStartStr(line);
        if (startsWith(trimmed, "%%") && !startsWith(trimmed, "%%{") && trimmed.size() > 2 && trimmed != "%%\n") {
            // drop comment line
        } else {
            out += line;
        }
        i = lineEnd;
    }
    return trimStartStr(out);
}

inline Preprocessed preprocess(const std::string& source) {
    Preprocessed res;
    std::string text = normalizeLineBreaks(source);

    // Replace double-quoted attributes inside HTML tags with single quotes: <tag attr="val"> -> <tag attr='val'>
    size_t lastClose = text.find('<') != std::string::npos ? text.rfind('>') : std::string::npos;
    if (lastClose != std::string::npos && lastClose > 0) {
        std::string head = text.substr(0, lastClose + 1);
        std::string replaced;
        replaced.reserve(head.size());
        size_t p = 0;
        while (p < head.size()) {
            if (head[p] == '<' && p + 1 < head.size() && isWord(static_cast<unsigned char>(head[p + 1]))) {
                size_t gt = head.find('>', p + 1);
                if (gt != std::string::npos) {
                    std::string tagContent = head.substr(p, gt - p + 1);
                    // replace ="([^"]*)" with ='$1'
                    std::string mod;
                    size_t k = 0;
                    while (k < tagContent.size()) {
                        if (k + 1 < tagContent.size() && tagContent[k] == '=' && tagContent[k + 1] == '"') {
                            size_t qEnd = tagContent.find('"', k + 2);
                            if (qEnd != std::string::npos) {
                                mod += "='" + tagContent.substr(k + 2, qEnd - (k + 2)) + "'";
                                k = qEnd + 1;
                                continue;
                            }
                        }
                        mod.push_back(tagContent[k++]);
                    }
                    replaced += mod;
                    p = gt + 1;
                    continue;
                }
            }
            replaced.push_back(head[p++]);
        }
        text = replaced + text.substr(lastClose + 1);
    }

    // Front matter: --- ... ---
    if (text.find("---") != std::string::npos) {
        size_t cursor = 0;
        while (cursor < text.size() && (text[cursor] == ' ' || text[cursor] == '\t')) cursor++;
        std::string indent = text.substr(0, cursor);
        if (startsWith(text, "---", cursor)) {
            size_t nl = text.find('\n', cursor + 3);
            if (nl != std::string::npos) {
                bool validOpen = true;
                for (size_t k = cursor + 3; k < nl; k++) {
                    if (text[k] != ' ' && text[k] != '\t') { validOpen = false; break; }
                }
                if (validOpen) {
                    std::string needle = "\n" + indent + "---";
                    size_t closePos = text.find(needle, nl);
                    if (closePos != std::string::npos) {
                        size_t afterFence = closePos + needle.size();
                        size_t closeEnd = text.find('\n', afterFence);
                        if (closeEnd == std::string::npos) closeEnd = text.size();
                        bool validClose = true;
                        for (size_t k = afterFence; k < closeEnd; k++) {
                            if (text[k] != ' ' && text[k] != '\t') { validClose = false; break; }
                        }
                        if (validClose) {
                            std::string body = text.substr(nl + 1, closePos - (nl + 1));
                            YamlValue parsed = parseYaml(body);
                            if (parsed.isMap()) {
                                if (auto* t = parsed.get("title"); t && !t->isNull()) {
                                    res.title = t->asString();
                                    res.hasTitle = true;
                                }
                                if (auto* c = parsed.get("config"); c && c->isMap()) {
                                    mergeConfig(res.config, *c);
                                }
                                if (auto* dm = parsed.get("displayMode"); dm && !dm->isNull()) {
                                    res.config["gantt"]["displayMode"] = YamlValue::makeString(dm->asString());
                                }
                            }
                            size_t frontLen = closeEnd < text.size() ? closeEnd + 1 : closeEnd;
                            for (size_t k = 0; k < frontLen; k++) if (text[k] == '\n') res.lineOffset++;
                            text = text.substr(frontLen);
                        }
                    }
                }
            }
        }
    }

    // Directives: %%{...}%%
    while (true) {
        size_t dStart = text.find("%%{");
        if (dStart == std::string::npos) break;
        size_t dEnd = text.find("}%%", dStart + 3);
        size_t removeEnd = (dEnd == std::string::npos) ? text.size() : dEnd + 3;
        std::string inner = trimStr(text.substr(dStart + 3, (dEnd == std::string::npos ? text.size() : dEnd) - (dStart + 3)));
        for (char& c : inner) if (c == '\'') c = '"';
        if (startsWith(toLowerAscii(inner), "wrap")) {
            res.config["wrap"] = YamlValue::makeBool(true);
        } else {
            size_t colon = inner.find(':');
            if (colon != std::string::npos) {
                std::string dtype = toLowerAscii(trimStr(inner.substr(0, colon)));
                if (dtype == "init" || dtype == "initialize") {
                    std::string jsonBody = trimStr(inner.substr(colon + 1));
                    YamlValue parsed = parseYaml(jsonBody);
                    if (parsed.isMap()) mergeConfig(res.config, parsed);
                }
            }
        }
        text.erase(dStart, removeEnd - dStart);
    }

    res.withComments = text;
    res.text = cleanupComments(text);
    return res;
}

inline std::string dropColorSemicolons(const std::string& text, const std::string& keyword) {
    size_t from = text.find(keyword);
    if (from == std::string::npos) return text;
    std::string out;
    size_t last = 0;
    while (from != std::string::npos) {
        size_t lineEnd = from;
        while (lineEnd < text.size() && text[lineEnd] != '\n' && text[lineEnd] != '\r') lineEnd++;
        bool afterColon = false;
        size_t hash = std::string::npos;
        for (size_t j = from + keyword.size(); j < lineEnd; j++) {
            unsigned char c = static_cast<unsigned char>(text[j]);
            if (c == ':') afterColon = true;
            else if (c == '#') {
                if (afterColon) { hash = j; break; }
            } else if (isSpace(c)) {
                afterColon = false;
            }
        }
        if (hash != std::string::npos && lineEnd > 0) {
            size_t semi = text.rfind(';', lineEnd - 1);
            if (semi != std::string::npos && semi > hash) {
                out += text.substr(last, semi - last);
                last = semi + 1;
            }
        }
        from = lineEnd >= text.size() ? std::string::npos : text.find(keyword, lineEnd + 1);
    }
    return last == 0 ? text : out + text.substr(last);
}

inline std::string encodeEntities(std::string text) {
    if (text.find('#') == std::string::npos) return text;
    text = dropColorSemicolons(dropColorSemicolons(text, "style"), "classDef");
    std::string out;
    out.reserve(text.size());
    size_t i = 0;
    while (i < text.size()) {
        if (text[i] == '#') {
            size_t p = i + 1;
            while (p < text.size() && isWord(static_cast<unsigned char>(text[p]))) p++;
            if (p > i + 1 && p < text.size() && text[p] == ';') {
                std::string inner = text.substr(i + 1, p - (i + 1));
                bool isInt = true;
                for (size_t k = 0; k < inner.size(); k++) {
                    if (k == 0 && inner[k] == '+' && inner.size() > 1) continue;
                    if (!isDigit(static_cast<unsigned char>(inner[k]))) { isInt = false; break; }
                }
                out += (isInt ? "\xEF\xAC\x82\xC2\xB0\xC2\xB0" : "\xEF\xAC\x82\xC2\xB0") + inner + "\xC2\xB6\xC3\x9F";
                i = p + 1;
                continue;
            }
        }
        out.push_back(text[i++]);
    }
    return out;
}

// ============================================================================
// src/detect.ts
// ============================================================================

struct Detected {
    std::string type;
    std::string mermaidType;
};

inline bool detectDiagram(const std::string& cleaned, Detected& out) {
    std::string s = trimStartStr(cleaned);
    auto matchPrefix = [&](const std::string& p, bool caseInsensitive = false, bool wordBoundary = false, bool spaceOrEnd = false, bool spaceColonOrEnd = false) {
        if (s.size() < p.size()) return false;
        for (size_t i = 0; i < p.size(); i++) {
            char a = s[i];
            char b = p[i];
            if (caseInsensitive) {
                if (a >= 'A' && a <= 'Z') a = static_cast<char>(a + 32);
                if (b >= 'A' && b <= 'Z') b = static_cast<char>(b + 32);
            }
            if (a != b) return false;
        }
        if (wordBoundary) {
            if (s.size() > p.size() && isWord(static_cast<unsigned char>(s[p.size()]))) return false;
        }
        if (spaceOrEnd) {
            if (s.size() > p.size() && !isSpace(static_cast<unsigned char>(s[p.size()]))) return false;
        }
        if (spaceColonOrEnd) {
            if (s.size() > p.size() && !isSpace(static_cast<unsigned char>(s[p.size()])) && s[p.size()] != ':') return false;
        }
        return true;
    };

    if (matchPrefix("flowchart-elk")) { out = {"flowchart", "flowchart-elk"}; return true; }
    if (matchPrefix("mindmap")) { out = {"mindmap", "mindmap"}; return true; }
    if (matchPrefix("architecture")) { out = {"architecture", "architecture"}; return true; }
    if (matchPrefix("agentflow-beta", false, true)) { out = {"agentflow", "agentflow"}; return true; }
    if (matchPrefix("C4Context") || matchPrefix("C4Container") || matchPrefix("C4Component") || matchPrefix("C4Dynamic") || matchPrefix("C4Deployment")) { out = {"c4", "c4"}; return true; }
    if (matchPrefix("kanban")) { out = {"kanban", "kanban"}; return true; }
    if (matchPrefix("classDiagram")) { out = {"class", "classDiagram"}; return true; }
    if (matchPrefix("erDiagram")) { out = {"er", "er"}; return true; }
    if (matchPrefix("gantt")) { out = {"gantt", "gantt"}; return true; }
    if (matchPrefix("info")) { out = {"info", "info"}; return true; }
    if (matchPrefix("pie")) { out = {"pie", "pie"}; return true; }
    if (matchPrefix("requirement")) { out = {"requirement", "requirement"}; return true; }
    if (matchPrefix("sequenceDiagram")) { out = {"sequence", "sequence"}; return true; }
    if (matchPrefix("swimlane-beta", false, true)) { out = {"swimlane", "swimlane"}; return true; }
    if (matchPrefix("graph") || matchPrefix("flowchart")) { out = {"flowchart", "flowchart-v2"}; return true; }
    if (matchPrefix("timeline")) { out = {"timeline", "timeline"}; return true; }
    if (matchPrefix("gitGraph")) { out = {"gitGraph", "gitGraph"}; return true; }
    if (matchPrefix("stateDiagram")) { out = {"state", "stateDiagram"}; return true; }
    if (matchPrefix("journey")) { out = {"journey", "journey"}; return true; }
    if (matchPrefix("quadrantChart")) { out = {"quadrantChart", "quadrantChart"}; return true; }
    if (matchPrefix("sankey")) { out = {"sankey", "sankey"}; return true; }
    if (matchPrefix("packet")) { out = {"packet", "packet"}; return true; }
    if (matchPrefix("xychart")) { out = {"xychart", "xychart"}; return true; }
    if (matchPrefix("block")) { out = {"block", "block"}; return true; }
    if (matchPrefix("eventmodeling")) { out = {"eventmodeling", "eventmodeling"}; return true; }
    if (matchPrefix("treeView-beta")) { out = {"treeView", "treeView"}; return true; }
    if (matchPrefix("radar-beta")) { out = {"radar", "radar"}; return true; }
    if (matchPrefix("ishikawa-beta", true, true) || matchPrefix("ishikawa", true, true)) { out = {"ishikawa", "ishikawa"}; return true; }
    if (matchPrefix("treemap")) { out = {"treemap", "treemap"}; return true; }
    if (matchPrefix("railroad-ebnf-beta", true)) { out = {"railroad", "railroadEbnf"}; return true; }
    if (matchPrefix("railroad-abnf-beta", true)) { out = {"railroad", "railroadAbnf"}; return true; }
    if (matchPrefix("railroad-peg-beta", true)) { out = {"railroad", "railroadPeg"}; return true; }
    if (matchPrefix("railroad-beta", true)) { out = {"railroad", "railroad"}; return true; }
    if (matchPrefix("venn-beta")) { out = {"venn", "venn"}; return true; }
    if (matchPrefix("wardley-beta", true)) { out = {"wardley", "wardley"}; return true; }
    if (matchPrefix("cynefin-beta", false, false, false, true)) { out = {"cynefin", "cynefin"}; return true; }
    if (matchPrefix("usecase-beta", false, false, true)) { out = {"usecase", "usecase"}; return true; }
    return false;
}

// ============================================================================
// src/diagrams/common/caseless.ts & accDescr.ts & generics.ts & fit*.ts
// ============================================================================

inline std::string foldCase(const std::string& s) {
    return toLowerAscii(s);
}

inline std::string cleanAccDescr(const std::string& text) {
    auto lines = splitStr(text, '\n');
    std::string out;
    for (const auto& rawLine : lines) {
        std::string t = trimStr(rawLine);
        if (t.empty()) continue;
        std::string collapsed;
        bool inBlank = false;
        for (char c : t) {
            if (c == ' ' || c == '\t') {
                if (!inBlank) {
                    collapsed.push_back(' ');
                    inBlank = true;
                }
            } else {
                collapsed.push_back(c);
                inBlank = false;
            }
        }
        if (!out.empty()) out.push_back('\n');
        out += collapsed;
    }
    return out;
}

inline int countTildes(const std::string& text) {
    int count = 0;
    for (char c : text) if (c == '~') count++;
    return count;
}

inline std::string pairTildes(const std::string& text) {
    int count = countTildes(text);
    if (count <= 1) return text;
    bool lead = (count % 2 != 0) && !text.empty() && text[0] == '~';
    std::string chars = text;
    std::vector<size_t> at;
    for (size_t i = lead ? 1 : 0; i < chars.size(); i++) {
        if (chars[i] == '~') at.push_back(i);
    }
    for (size_t lo = 0, hi = at.size() - 1; lo < hi; lo++, hi--) {
        chars[at[lo]] = '<';
        chars[at[hi]] = '>';
    }
    return chars;
}

inline std::string parseGenericTypes(const std::string& input) {
    if (input.find('~') == std::string::npos) return input;
    std::vector<std::string> sets;
    size_t start = 0;
    for (size_t i = 0; i < input.size(); i++) {
        if (input[i] == ',') {
            sets.push_back(input.substr(start, i - start));
            sets.push_back(",");
            start = i + 1;
        }
    }
    sets.push_back(input.substr(start));
    std::vector<std::string> output;
    for (size_t i = 0; i < sets.size(); i++) {
        std::string set = sets[i];
        if (set == "," && i > 0 && i + 1 < sets.size() && countTildes(sets[i - 1]) == 1 && countTildes(sets[i + 1]) == 1) {
            set = sets[i - 1] + "," + sets[i + 1];
            i++;
            output.pop_back();
        }
        output.push_back(pairTildes(set));
    }
    std::string res;
    for (const auto& s : output) res += s;
    return res;
}

inline double fitScale(double contentWidth, double pad, const RenderOptions& options) {
    double maxWidth = options.maxWidth;
    if (!(maxWidth > 0)) return 1.0;
    double room = maxWidth - 2.0 * pad;
    if (!(contentWidth > 0 && room > 0 && room < contentWidth)) return 1.0;
    return std::max(0.25, room / contentWidth);
}

inline double fitFontSize(double scale, double base, double floor = 11.0) {
    return scale < 1.0 ? std::max(floor, std::round(base * std::pow(scale, 0.7) * 2.0) / 2.0) : base;
}

inline RenderOptions fitTextOptions(double contentWidth, double pad, const RenderOptions& options, double floor = 11.0) {
    double scale = fitScale(contentWidth, pad, options);
    double base = options.fontSize;
    double fontSize = fitFontSize(scale, base, floor);
    if (fontSize == base) return options;
    RenderOptions next = options;
    next.fontSize = fontSize;
    return next;
}

inline double titleRoom(const RenderOptions& options) {
    double maxWidth = options.maxWidth;
    double pad = options.padding;
    return maxWidth > 0 ? std::max(120.0, maxWidth - 2.0 * pad) : 4000.0;
}

template <typename DrawFn>
inline Rendered tighten(const RenderOptions& options, DrawFn&& draw) {
    Rendered wide = draw(false);
    double maxWidth = options.maxWidth;
    if (!(maxWidth > 0 && wide.width > maxWidth)) return wide;
    Rendered tight = draw(true);
    return tight.width < wide.width ? tight : wide;
}

} // namespace Pele
