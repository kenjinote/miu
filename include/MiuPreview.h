#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <chrono>
#include <random>
#include <algorithm>
#include "Pele.h"
#include "pele/math_engine.h"
#include "MiuEmmet.h"

namespace MiuPreview {

inline std::string convertMarkdownToHtml(const std::string& md, int depth = 0);
inline std::string formatMarkdownInline(const std::string& s, int depth = 0);

inline std::string escapeHtmlBasic(const std::string& s) {
    std::string out; out.reserve(s.size() + 64);
    for (char c : s) {
        if (c == '&') out += "&amp;";
        else if (c == '<') out += "&lt;";
        else if (c == '>') out += "&gt;";
        else if (c == '"') out += "&quot;";
        else out += c;
    }
    return out;
}

    inline bool mdIsAsciiPunct(char c)  noexcept{
        return (c >= 0x21 && c <= 0x2F) || (c >= 0x3A && c <= 0x40) ||
               (c >= 0x5B && c <= 0x60) || (c >= 0x7B && c <= 0x7E);
    }
    inline bool mdIsSpace(char c)  noexcept{
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }
    inline bool mdIsAlnum(char c)  noexcept{
        unsigned char u = (unsigned char)c;
        return (u >= '0' && u <= '9') || (u >= 'A' && u <= 'Z') || (u >= 'a' && u <= 'z');
    }
    inline std::string mdUnescapeString(const std::string& s)  noexcept{
        std::string r; r.reserve(s.size());
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '\\' && i + 1 < s.size() && mdIsAsciiPunct(s[i + 1])) {
                r += s[++i];
            } else {
                r += s[i];
            }
        }
        return r;
    }
    inline size_t mdFindMatchingBracket(const std::string& s, size_t openPos)  noexcept{
        size_t n = s.size();
        int depth = 1;
        size_t i = openPos + 1;
        while (i < n) {
            if (s[i] == '\\' && i + 1 < n) { i += 2; continue; }
            if (s[i] == '`') {
                size_t ticks = 0;
                while (i + ticks < n && s[i + ticks] == '`') ticks++;
                size_t j = i + ticks;
                while (j < n) {
                    if (s[j] == '`') {
                        size_t cticks = 0;
                        while (j + cticks < n && s[j + cticks] == '`') cticks++;
                        if (cticks == ticks) { j += cticks; break; }
                        j += cticks;
                    } else j++;
                }
                i = j;
                continue;
            }
            if (s[i] == '[') depth++;
            else if (s[i] == ']') {
                depth--;
                if (depth == 0) return i;
            }
            i++;
        }
        return std::string::npos;
    }
    inline bool mdParseLinkDestAndTitle(const std::string& s, size_t openParen, size_t& closeParenOut, std::string& urlOut, std::string& titleOut)  noexcept{
        size_t n = s.size();
        if (openParen >= n || s[openParen] != '(') return false;
        size_t i = openParen + 1;
        while (i < n && mdIsSpace(s[i])) i++;
        if (i >= n) return false;
        if (s[i] == ')') {
            urlOut.clear(); titleOut.clear(); closeParenOut = i; return true;
        }
        std::string rawUrl;
        if (s[i] == '<') {
            i++;
            while (i < n && s[i] != '>' && s[i] != '\n' && s[i] != '<') {
                if (s[i] == '\\' && i + 1 < n) { rawUrl += s[i]; rawUrl += s[i + 1]; i += 2; continue; }
                rawUrl += s[i++];
            }
            if (i >= n || s[i] != '>') return false;
            i++;
        } else {
            int pDepth = 0;
            while (i < n && !mdIsSpace(s[i])) {
                if (s[i] == '\\' && i + 1 < n) { rawUrl += s[i]; rawUrl += s[i + 1]; i += 2; continue; }
                if (s[i] == '(') { pDepth++; rawUrl += s[i++]; }
                else if (s[i] == ')') {
                    if (pDepth == 0) break;
                    pDepth--; rawUrl += s[i++];
                } else {
                    rawUrl += s[i++];
                }
            }
            if (pDepth != 0) return false;
        }
        bool hadSpace = false;
        while (i < n && mdIsSpace(s[i])) { hadSpace = true; i++; }
        std::string rawTitle;
        if (hadSpace && i < n && (s[i] == '"' || s[i] == '\'' || s[i] == '(')) {
            char closeQ = (s[i] == '(') ? ')' : s[i];
            i++;
            while (i < n && s[i] != closeQ) {
                if (s[i] == '\\' && i + 1 < n) { rawTitle += s[i]; rawTitle += s[i + 1]; i += 2; continue; }
                rawTitle += s[i++];
            }
            if (i >= n || s[i] != closeQ) return false;
            i++;
            while (i < n && mdIsSpace(s[i])) i++;
        }
        if (i >= n || s[i] != ')') return false;
        closeParenOut = i;
        urlOut = mdUnescapeString(rawUrl);
        titleOut = mdUnescapeString(rawTitle);
        return true;
    }
    inline bool mdIsSafeUrl(const std::string& rawUrl, bool isImage)  noexcept{
        size_t a = 0, b = rawUrl.size();
        while (a < b && (unsigned char)rawUrl[a] <= 0x20) a++;
        while (b > a && (unsigned char)rawUrl[b - 1] <= 0x20) b--;
        if (a >= b) return true;
        std::string norm;
        norm.reserve(b - a);
        for (size_t i = a; i < b; ++i) {
            unsigned char c = (unsigned char)rawUrl[i];
            if (c < 0x20 || c == 0x7F) return false;
            if (c == ' ') continue;
            if (c == '&') {
                if (i + 1 < b && rawUrl[i + 1] == '#') {
                    size_t k = i + 2;
                    uint32_t cp = 0;
                    bool hasDigits = false;
                    if (k < b && (rawUrl[k] == 'x' || rawUrl[k] == 'X')) {
                        k++;
                        while (k < b && (k - i) <= 12) {
                            char hc = rawUrl[k];
                            if (hc >= '0' && hc <= '9') { cp = (cp << 4) | (hc - '0'); hasDigits = true; k++; }
                            else if (hc >= 'a' && hc <= 'f') { cp = (cp << 4) | (hc - 'a' + 10); hasDigits = true; k++; }
                            else if (hc >= 'A' && hc <= 'F') { cp = (cp << 4) | (hc - 'A' + 10); hasDigits = true; k++; }
                            else break;
                        }
                    } else {
                        while (k < b && (k - i) <= 12 && rawUrl[k] >= '0' && rawUrl[k] <= '9') {
                            cp = cp * 10 + (rawUrl[k] - '0');
                            hasDigits = true;
                            k++;
                        }
                    }
                    if (hasDigits) {
                        if (k < b && rawUrl[k] == ';') i = k;
                        else i = k - 1;
                        if (cp <= 0x20 || cp == 0x7F) continue;
                        if (cp < 0x80) {
                            char dc = (char)cp;
                            if (dc >= 'A' && dc <= 'Z') dc = (char)(dc + 32);
                            norm += dc;
                        } else {
                            norm += '?';
                        }
                        continue;
                    }
                } else {
                    size_t k = i + 1;
                    while (k < b && (k - i) <= 12 && mdIsAlnum(rawUrl[k])) k++;
                    std::string ent = Emmet::toLowerStr(rawUrl.substr(i + 1, k - (i + 1)));
                    bool hasSemi = (k < b && rawUrl[k] == ';');
                    if (ent == "colon") {
                        norm += ':';
                        i = hasSemi ? k : (k - 1);
                        continue;
                    } else if (ent == "tab" || ent == "newline") {
                        i = hasSemi ? k : (k - 1);
                        continue;
                    } else if (ent == "amp" && hasSemi) {
                        norm += '&';
                        i = k;
                        continue;
                    }
                }
            }
            if (c >= 'A' && c <= 'Z') norm += (char)(c + 32);
            else norm += (char)c;
        }
        if (norm.empty() || norm[0] == '#') return true;
        size_t colon = std::string::npos;
        for (size_t i = 0; i < norm.size(); ++i) {
            char c = norm[i];
            if (c == '/' || c == '?' || c == '#') break;
            if (c == ':') { colon = i; break; }
        }
        if (colon == std::string::npos) return true;
        if (colon == 0) return false;
        for (size_t i = 0; i < colon; ++i) {
            char c = norm[i];
            if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.')) return false;
        }
        std::string scheme = norm.substr(0, colon + 1);
        if (scheme == "http:" || scheme == "https:") {
            return norm.compare(colon, 3, "://") == 0;
        }
        if (!isImage && scheme == "mailto:") return true;
        if (isImage && scheme == "data:") {
            static const char* const safeDataImg[] = {
                "data:image/png;", "data:image/jpeg;", "data:image/jpg;",
                "data:image/gif;", "data:image/webp;", "data:image/bmp;",
                "data:image/x-icon;", "data:image/avif;"
            };
            for (const char* pfx : safeDataImg) {
                size_t plen = strlen(pfx);
                if (norm.compare(0, plen, pfx) == 0) return true;
            }
            return false;
        }
        return false;
    }
    inline bool mdIsSafeHtmlTag(const std::string& tagContent)  noexcept{
        if (tagContent.empty()) return false;
        if (tagContent.compare(0, 3, "!--") == 0 && tagContent.size() >= 5 && tagContent.compare(tagContent.size() - 2, 2, "--") == 0) return true;
        size_t n = tagContent.size(), p = 0;
        bool isClosing = false;
        if (tagContent[p] == '/') { isClosing = true; p++; }
        size_t nameStart = p;
        while (p < n && mdIsAlnum(tagContent[p])) p++;
        if (p == nameStart) return false;
        std::string name = Emmet::toLowerStr(tagContent.substr(nameStart, p - nameStart));
        static const char* const safeTags[] = {
            "br", "hr", "kbd", "sub", "sup", "mark", "u", "small", "del", "ins", "s",
            "b", "i", "em", "strong", "code", "pre", "abbr", "dfn", "var", "samp", "cite", "q",
            "ruby", "rt", "rp", "details", "summary", "span", "div", "p", "img", "a",
            "ul", "ol", "li", "dl", "dt", "dd", "table", "thead", "tbody", "tr", "th", "td",
            "blockquote", "h1", "h2", "h3", "h4", "h5", "h6"
        };
        bool ok = false;
        for (const char* st : safeTags) { if (name == st) { ok = true; break; } }
        if (!ok) return false;
        if (isClosing) {
            while (p < n && mdIsSpace(tagContent[p])) p++;
            return p == n;
        }
        if (p < n && !mdIsSpace(tagContent[p]) && tagContent[p] != '/') return false;
        static const char* const safeCommonAttrs[] = {
            "id", "class", "title", "lang", "dir", "align", "valign", "width", "height",
            "clear", "open", "start", "type", "reversed", "value", "colspan", "rowspan",
            "headers", "scope", "aria-label", "aria-hidden", "role"
        };
        while (p < n) {
            while (p < n && mdIsSpace(tagContent[p])) p++;
            if (p >= n) break;
            if (tagContent[p] == '/') {
                p++;
                while (p < n && mdIsSpace(tagContent[p])) p++;
                return p == n;
            }
            size_t attrStart = p;
            while (p < n && (mdIsAlnum(tagContent[p]) || tagContent[p] == '-' || tagContent[p] == '_')) p++;
            if (p == attrStart) return false;
            std::string attrName = Emmet::toLowerStr(tagContent.substr(attrStart, p - attrStart));
            bool attrAllowed = false;
            for (const char* sa : safeCommonAttrs) {
                if (attrName == sa) { attrAllowed = true; break; }
            }
            if (!attrAllowed) {
                if (name == "a" && (attrName == "href" || attrName == "target" || attrName == "rel" || attrName == "name")) attrAllowed = true;
                else if (name == "img" && (attrName == "src" || attrName == "alt" || attrName == "loading" || attrName == "decoding")) attrAllowed = true;
            }
            if (!attrAllowed) return false;
            while (p < n && mdIsSpace(tagContent[p])) p++;
            if (p < n && tagContent[p] == '=') {
                p++;
                while (p < n && mdIsSpace(tagContent[p])) p++;
                if (p >= n) return false;
                std::string attrVal;
                if (tagContent[p] == '"' || tagContent[p] == '\'') {
                    char q = tagContent[p++];
                    size_t vs = p;
                    while (p < n && tagContent[p] != q) p++;
                    if (p >= n) return false;
                    attrVal = tagContent.substr(vs, p - vs);
                    p++;
                } else {
                    size_t vs = p;
                    while (p < n && !mdIsSpace(tagContent[p]) && tagContent[p] != '/' && tagContent[p] != '"' && tagContent[p] != '\'' && tagContent[p] != '<' && tagContent[p] != '=' && tagContent[p] != '`') p++;
                    if (p == vs) return false;
                    attrVal = tagContent.substr(vs, p - vs);
                }
                if (attrName == "href" && !mdIsSafeUrl(attrVal, false)) return false;
                if (attrName == "src" && !mdIsSafeUrl(attrVal, true)) return false;
            } else {
                if (attrName == "href" || attrName == "src") return false;
            }
        }
        return true;
    }
    inline size_t mdFindClosingEmphasis(const std::string& s, size_t start, char ch, size_t dlen)  noexcept{
        size_t n = s.size();
        size_t i = start;
        int innerSingle = 0;
        int innerDouble = 0;
        while (i < n) {
            if (s[i] == '\\' && i + 1 < n) { i += 2; continue; }
            if (s[i] == '`') {
                size_t ticks = 0;
                while (i + ticks < n && s[i + ticks] == '`') ticks++;
                size_t j = i + ticks;
                while (j < n) {
                    if (s[j] == '`') {
                        size_t cticks = 0;
                        while (j + cticks < n && s[j + cticks] == '`') cticks++;
                        if (cticks == ticks) { j += cticks; break; }
                        j += cticks;
                    } else j++;
                }
                i = j;
                continue;
            }
            if (s[i] == '[') {
                size_t rb = mdFindMatchingBracket(s, i);
                if (rb != std::string::npos && rb + 1 < n && s[rb + 1] == '(') {
                    size_t cp = 0; std::string u, t;
                    if (mdParseLinkDestAndTitle(s, rb + 1, cp, u, t)) { i = cp + 1; continue; }
                }
            }
            if (s[i] == '<') {
                size_t gt = s.find('>', i + 1);
                if (gt != std::string::npos && gt - i <= 256) {
                    std::string inner = s.substr(i + 1, gt - i - 1);
                    if (inner.find("http://") == 0 || inner.find("https://") == 0 || mdIsSafeHtmlTag(inner)) {
                        i = gt + 1; continue;
                    }
                }
            }
            if (s[i] == ch) {
                size_t run = 0;
                while (i + run < n && s[i + run] == ch) run++;
                bool prevSpace = (i == 0) || mdIsSpace(s[i - 1]);
                bool nextSpace = (i + run >= n) || mdIsSpace(s[i + run]);
                bool nextAlnum = (i + run < n) && mdIsAlnum(s[i + run]);
                bool prevAlnum = (i > 0) && mdIsAlnum(s[i - 1]);
                bool canClose = !prevSpace && (ch != '_' || !nextAlnum);
                bool canOpen  = !nextSpace && (ch != '_' || !prevAlnum);
                if (canClose) {
                    if (run == dlen) {
                        if (dlen == 1 && innerSingle > 0) { innerSingle--; i += run; continue; }
                        if (dlen == 2 && innerDouble > 0) { innerDouble--; i += run; continue; }
                        return i;
                    }
                    if (run == 3 && dlen == 2) {
                        if (innerSingle > 0) return i + 1;
                        return i;
                    }
                    if (run == 3 && dlen == 1) {
                        if (innerDouble > 0) return i + 2;
                        return i;
                    }
                    if (run > dlen && ch != '~') {
                        return i;
                    }
                }
                if (canOpen && !canClose) {
                    if (run == 1) innerSingle++;
                    else if (run == 2) innerDouble++;
                }
                i += run;
                continue;
            }
            i++;
        }
        return std::string::npos;
    }

    inline uint64_t fnv1a64(const char* data, size_t len, uint64_t seed = 14695981039346656037ULL)  noexcept{
        uint64_t h = seed;
        for (size_t i = 0; i < len; ++i) {
            h ^= (unsigned char)data[i];
            h *= 1099511628211ULL;
        }
        return h;
    }
    struct MathCacheSlot {
        uint64_t hash = 0;
        bool displayMode = false;
        std::string tex;
        std::string mml;
    };
    inline const std::string& cachedLatexToMathML(const std::string& tex, bool displayMode)  noexcept{
        static MathCacheSlot s_cache[512];
        uint64_t h = fnv1a64(tex.data(), tex.size(), displayMode ? 0x9E3779B97F4A7C15ULL : 14695981039346656037ULL);
        if (h == 0) h = 1;
        MathCacheSlot& slot = s_cache[h & 511];
        if (slot.hash == h && slot.displayMode == displayMode && slot.tex == tex) {
            return slot.mml;
        }
        slot.hash = h;
        slot.displayMode = displayMode;
        slot.tex = tex;
        slot.mml = Pele::latexToMathML(tex, displayMode);
        return slot.mml;
    }
    struct MermaidCacheSlot {
        uint64_t hash = 0;
        std::string code;
        std::string svg;
    };
    inline const std::string& cachedMermaidRenderSvg(const std::string& code)  noexcept{
        static MermaidCacheSlot s_cache[128];
        uint64_t h = fnv1a64(code.data(), code.size());
        if (h == 0) h = 1;
        MermaidCacheSlot& slot = s_cache[h & 127];
        if (slot.hash == h && slot.code == code) {
            return slot.svg;
        }
        slot.hash = h;
        slot.code = code;
        slot.svg = Pele::render(code).svg;
        return slot.svg;
    }

inline std::string formatMarkdownInline(const std::string& s, int depth) {
    if (depth > 32) return escapeHtmlBasic(s);
    std::string r; r.reserve(s.size() + 64);
    size_t i = 0, n = s.size();
    while (i < n) {
        if (s[i] == '\\' && i + 1 < n) {
            char nx = s[i + 1];
            if (nx == '\n') { r += "<br>\n"; i += 2; continue; }
            if (mdIsAsciiPunct(nx)) {
                if (nx == '&') r += "&amp;";
                else if (nx == '<') r += "&lt;";
                else if (nx == '>') r += "&gt;";
                else if (nx == '"') r += "&quot;";
                else r += nx;
                i += 2;
                continue;
            }
        }
        if (s[i] == '`') {
            size_t ticks = 0;
            while (i + ticks < n && s[i + ticks] == '`') ticks++;
            size_t j = i + ticks;
            size_t found = std::string::npos;
            while (j < n) {
                if (s[j] == '`') {
                    size_t cticks = 0;
                    while (j + cticks < n && s[j + cticks] == '`') cticks++;
                    if (cticks == ticks) { found = j; break; }
                    j += cticks;
                } else {
                    j++;
                }
            }
            if (found != std::string::npos) {
                std::string code = s.substr(i + ticks, found - (i + ticks));
                for (char& c : code) { if (c == '\n' || c == '\r') c = ' '; }
                if (code.size() >= 2 && code.front() == ' ' && code.back() == ' ') {
                    bool allSpaces = true;
                    for (char c : code) { if (c != ' ') { allSpaces = false; break; } }
                    if (!allSpaces) code = code.substr(1, code.size() - 2);
                }
                r += "<code>"; r += escapeHtmlBasic(code); r += "</code>";
                i = found + ticks;
                continue;
            } else {
                r.append(ticks, '`');
                i += ticks;
                continue;
            }
        }
        if (i + 3 < n && s[i] == '$' && s[i + 1] == '$') {
            size_t closeMath = s.find("$$", i + 2);
            if (closeMath != std::string::npos && closeMath > i + 2) {
                std::string tex = s.substr(i + 2, closeMath - (i + 2));
                const std::string& mml = cachedLatexToMathML(tex, true);
                if (!mml.empty()) {
                    r += "<span class=\"math-inline\">"; r += mml; r += "</span>";
                    i = closeMath + 2;
                    continue;
                }
            }
        }
        if (s[i] == '$' && (i == 0 || s[i - 1] != '\\') && (i + 1 < n && s[i + 1] != '$')) {
            size_t closeMath = i + 1;
            while (closeMath < n) {
                if (s[closeMath] == '$' && s[closeMath - 1] != '\\') break;
                if (s[closeMath] == '\n') { closeMath = n; break; }
                closeMath++;
            }
            if (closeMath < n && closeMath > i + 1) {
                std::string tex = s.substr(i + 1, closeMath - (i + 1));
                if (Pele::detail::looksLikeInlineMath(tex)) {
                    const std::string& mml = cachedLatexToMathML(tex, false);
                    if (!mml.empty()) {
                        r += "<span class=\"math-inline\">"; r += mml; r += "</span>";
                        i = closeMath + 1;
                        continue;
                    }
                }
            }
        }
        if (s[i] == '!' && i + 1 < n && s[i + 1] == '[') {
            size_t rb = mdFindMatchingBracket(s, i + 1);
            if (rb != std::string::npos && rb + 1 < n && s[rb + 1] == '(') {
                size_t cp = 0; std::string url, title;
                if (mdParseLinkDestAndTitle(s, rb + 1, cp, url, title)) {
                    if (!mdIsSafeUrl(url, true)) url.clear();
                    std::string alt = mdUnescapeString(s.substr(i + 2, rb - (i + 2)));
                    r += "<img src=\""; r += escapeHtmlBasic(url);
                    r += "\" alt=\""; r += escapeHtmlBasic(alt); r += "\"";
                    if (!title.empty()) { r += " title=\""; r += escapeHtmlBasic(title); r += "\""; }
                    r += " style=\"max-width:100%\">";
                    i = cp + 1;
                    continue;
                }
            }
        }
        if (s[i] == '[') {
            size_t rb = mdFindMatchingBracket(s, i);
            if (rb != std::string::npos && rb + 1 < n && s[rb + 1] == '(') {
                size_t cp = 0; std::string url, title;
                if (mdParseLinkDestAndTitle(s, rb + 1, cp, url, title)) {
                    if (!mdIsSafeUrl(url, false)) url = "#";
                    r += "<a href=\""; r += escapeHtmlBasic(url); r += "\"";
                    if (!title.empty()) { r += " title=\""; r += escapeHtmlBasic(title); r += "\""; }
                    r += ">"; r += formatMarkdownInline(s.substr(i + 1, rb - (i + 1)), depth + 1); r += "</a>";
                    i = cp + 1;
                    continue;
                }
            }
        }
        if (s[i] == '<') {
            if (s.compare(i, 4, "<!--") == 0) {
                size_t ce = s.find("-->", i + 4);
                if (ce != std::string::npos) { i = ce + 3; continue; }
            }
            size_t gt = s.find('>', i + 1);
            if (gt != std::string::npos && gt > i + 1 && gt - i <= 512 && s.find('\n', i + 1) > gt) {
                std::string inner = s.substr(i + 1, gt - i - 1);
                if ((inner.compare(0, 7, "http://") == 0 || inner.compare(0, 8, "https://") == 0 || inner.compare(0, 6, "ftp://") == 0) && inner.find(' ') == std::string::npos) {
                    r += "<a href=\""; r += escapeHtmlBasic(inner); r += "\">"; r += escapeHtmlBasic(inner); r += "</a>";
                    i = gt + 1;
                    continue;
                }
                if (inner.compare(0, 7, "mailto:") == 0 && inner.find(' ') == std::string::npos) {
                    std::string addr = inner.substr(7);
                    r += "<a href=\"mailto:"; r += escapeHtmlBasic(addr); r += "\">"; r += escapeHtmlBasic(addr); r += "</a>";
                    i = gt + 1;
                    continue;
                }
                size_t atPos = inner.find('@');
                if (atPos != std::string::npos && atPos > 0 && atPos + 1 < inner.size() && inner.find(' ') == std::string::npos && inner.find('/') == std::string::npos && inner.find('.', atPos) != std::string::npos) {
                    r += "<a href=\"mailto:"; r += escapeHtmlBasic(inner); r += "\">"; r += escapeHtmlBasic(inner); r += "</a>";
                    i = gt + 1;
                    continue;
                }
                if (mdIsSafeHtmlTag(inner)) {
                    r += '<'; r += inner; r += '>';
                    i = gt + 1;
                    continue;
                }
            }
        }
        if ((s.compare(i, 8, "https://") == 0 || s.compare(i, 7, "http://") == 0) &&
            (i == 0 || mdIsSpace(s[i - 1]) || s[i - 1] == '(' || s[i - 1] == '[' || s[i - 1] == '*' || s[i - 1] == '_' || s[i - 1] == '~')) {
            size_t uEnd = i;
            while (uEnd < n && !mdIsSpace(s[uEnd]) && s[uEnd] != '<' && s[uEnd] != '>' && s[uEnd] != '"') uEnd++;
            while (uEnd > i) {
                char last = s[uEnd - 1];
                if (last == '.' || last == ',' || last == ';' || last == ':' || last == '!' || last == '?' || last == '\'') {
                    uEnd--;
                } else if (last == ')') {
                    int pCount = 0;
                    for (size_t k = i; k < uEnd; ++k) {
                        if (s[k] == '(') pCount++;
                        else if (s[k] == ')') pCount--;
                    }
                    if (pCount < 0) uEnd--;
                    else break;
                } else {
                    break;
                }
            }
            if (uEnd > i + 8) {
                std::string url = s.substr(i, uEnd - i);
                r += "<a href=\""; r += escapeHtmlBasic(url); r += "\">"; r += escapeHtmlBasic(url); r += "</a>";
                i = uEnd;
                continue;
            }
        }
        if (s[i] == '*' || s[i] == '_' || s[i] == '~') {
            char ch = s[i];
            size_t run = 0;
            while (i + run < n && s[i + run] == ch) run++;
            bool prevAlnum = (i > 0) && mdIsAlnum(s[i - 1]);
            bool nextSpace = (i + run >= n) || mdIsSpace(s[i + run]);
            bool canOpen = !nextSpace && (ch != '_' || !prevAlnum);
            if (canOpen) {
                if (ch == '~') {
                    if (run == 2) {
                        size_t j = mdFindClosingEmphasis(s, i + 2, '~', 2);
                        if (j != std::string::npos) {
                            r += "<del>"; r += formatMarkdownInline(s.substr(i + 2, j - (i + 2)), depth + 1); r += "</del>";
                            i = j + 2;
                            continue;
                        }
                    }
                } else {
                    if (run >= 3) {
                        size_t j3 = mdFindClosingEmphasis(s, i + 3, ch, 3);
                        if (j3 != std::string::npos) {
                            r += "<strong><em>"; r += formatMarkdownInline(s.substr(i + 3, j3 - (i + 3)), depth + 1); r += "</em></strong>";
                            i = j3 + 3;
                            continue;
                        }
                    }
                    if (run >= 2) {
                        size_t j2 = mdFindClosingEmphasis(s, i + 2, ch, 2);
                        if (j2 != std::string::npos) {
                            r += "<strong>"; r += formatMarkdownInline(s.substr(i + 2, j2 - (i + 2)), depth + 1); r += "</strong>";
                            i = j2 + 2;
                            continue;
                        }
                    }
                    if (run >= 1) {
                        size_t j1 = mdFindClosingEmphasis(s, i + 1, ch, 1);
                        if (j1 != std::string::npos) {
                            r += "<em>"; r += formatMarkdownInline(s.substr(i + 1, j1 - (i + 1)), depth + 1); r += "</em>";
                            i = j1 + 1;
                            continue;
                        }
                    }
                }
            }
            r.append(run, ch);
            i += run;
            continue;
        }
        if (s[i] == '&') {
            size_t semi = s.find(';', i + 1);
            if (semi != std::string::npos && semi > i + 2 && semi - i <= 32) {
                bool validEnt = true;
                if (s[i + 1] == '#') {
                    size_t k = i + 2;
                    if (k < semi && (s[k] == 'x' || s[k] == 'X')) {
                        k++;
                        if (k == semi) validEnt = false;
                        for (; k < semi; ++k) {
                            char c = s[k];
                            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) { validEnt = false; break; }
                        }
                    } else {
                        if (k == semi) validEnt = false;
                        for (; k < semi; ++k) {
                            if (s[k] < '0' || s[k] > '9') { validEnt = false; break; }
                        }
                    }
                } else {
                    for (size_t k = i + 1; k < semi; ++k) {
                        if (!mdIsAlnum(s[k])) { validEnt = false; break; }
                    }
                }
                if (validEnt) {
                    r.append(s, i, semi - i + 1);
                    i = semi + 1;
                    continue;
                }
            }
            r += "&amp;";
            i++;
            continue;
        }
        if (s[i] == '<') r += "&lt;";
        else if (s[i] == '>') r += "&gt;";
        else r += s[i];
        i++;
    }
    return r;
}
inline std::string convertMarkdownToHtml(const std::string& md, int depth) {
    if (depth > 16) return "<p>" + escapeHtmlBasic(md) + "</p>\n";
    auto trimStr = [](const std::string& str) -> std::string {
        size_t a = 0, b = str.size();
        while (a < b && (str[a] == ' ' || str[a] == '\t')) a++;
        while (b > a && (str[b - 1] == ' ' || str[b - 1] == '\t')) b--;
        return str.substr(a, b - a);
    };
    auto getIndent = [](const std::string& line, size_t& firstNonSpace) -> int {
        int cols = 0;
        size_t i = 0;
        while (i < line.size()) {
            if (line[i] == ' ') { cols++; i++; }
            else if (line[i] == '\t') { cols += 4 - (cols % 4); i++; }
            else break;
        }
        firstNonSpace = i;
        return cols;
    };
    auto splitCells = [&](const std::string& row) -> std::vector<std::string> {
        std::string t = trimStr(row);
        if (!t.empty() && t.front() == '|') t.erase(0, 1);
        if (!t.empty() && t.back() == '|') {
            size_t bs = 0;
            for (size_t k = t.size() - 1; k > 0 && t[k - 1] == '\\'; --k) bs++;
            if ((bs % 2) == 0) t.pop_back();
        }
        std::vector<std::string> cells;
        size_t start = 0, i = 0, n = t.size();
        while (i < n) {
            if (t[i] == '\\' && i + 1 < n) { i += 2; continue; }
            if (t[i] == '`') {
                size_t ticks = 0;
                while (i + ticks < n && t[i + ticks] == '`') ticks++;
                size_t j = i + ticks;
                while (j < n) {
                    if (t[j] == '`') {
                        size_t cticks = 0;
                        while (j + cticks < n && t[j + cticks] == '`') cticks++;
                        if (cticks == ticks) { j += cticks; break; }
                        j += cticks;
                    } else j++;
                }
                i = j;
                continue;
            }
            if (t[i] == '|') {
                std::string c = trimStr(t.substr(start, i - start));
                std::string u; u.reserve(c.size());
                for (size_t k = 0; k < c.size(); ++k) {
                    if (c[k] == '\\' && k + 1 < c.size() && c[k + 1] == '|') { u += '|'; k++; }
                    else u += c[k];
                }
                cells.push_back(std::move(u));
                start = i + 1;
            }
            i++;
        }
        {
            std::string c = trimStr(t.substr(start));
            std::string u; u.reserve(c.size());
            for (size_t k = 0; k < c.size(); ++k) {
                if (c[k] == '\\' && k + 1 < c.size() && c[k + 1] == '|') { u += '|'; k++; }
                else u += c[k];
            }
            cells.push_back(std::move(u));
        }
        return cells;
    };
    auto isTableSep = [&](const std::string& s) -> bool {
        if (s.find('|') == std::string::npos || s.find('-') == std::string::npos) return false;
        auto cells = splitCells(s);
        if (cells.empty()) return false;
        for (const auto& c : cells) {
            if (c.empty()) return false;
            size_t a = 0, b = c.size();
            if (c[a] == ':') a++;
            if (b > a && c[b - 1] == ':') b--;
            if (b <= a) return false;
            for (size_t k = a; k < b; ++k) {
                if (c[k] != '-') return false;
            }
        }
        return true;
    };
    auto isFenceStart = [&](const std::string& line, char& fChar, size_t& fLen, std::string& info) -> bool {
        size_t p = 0;
        int ind = getIndent(line, p);
        if (ind > 3 || p >= line.size()) return false;
        char c = line[p];
        if (c != '`' && c != '~') return false;
        size_t len = 0;
        while (p + len < line.size() && line[p + len] == c) len++;
        if (len < 3) return false;
        std::string rest = trimStr(line.substr(p + len));
        if (c == '`' && rest.find('`') != std::string::npos) return false;
        fChar = c; fLen = len; info = rest;
        return true;
    };
    auto isFenceEnd = [&](const std::string& line, char fChar, size_t fLen) -> bool {
        size_t p = 0;
        int ind = getIndent(line, p);
        if (ind > 3 || p >= line.size() || line[p] != fChar) return false;
        size_t len = 0;
        while (p + len < line.size() && line[p + len] == fChar) len++;
        if (len < fLen) return false;
        for (size_t k = p + len; k < line.size(); ++k) {
            if (line[k] != ' ' && line[k] != '\t') return false;
        }
        return true;
    };
    auto isThematicBreak = [&](const std::string& line) -> bool {
        size_t p = 0;
        int ind = getIndent(line, p);
        if (ind > 3 || p >= line.size()) return false;
        char c = line[p];
        if (c != '-' && c != '*' && c != '_') return false;
        size_t count = 0;
        for (size_t k = p; k < line.size(); ++k) {
            if (line[k] == c) count++;
            else if (line[k] != ' ' && line[k] != '\t') return false;
        }
        return count >= 3;
    };
    auto isSetextUnderline = [&](const std::string& line, int& levelOut) -> bool {
        size_t p = 0;
        int ind = getIndent(line, p);
        if (ind > 3 || p >= line.size()) return false;
        char c = line[p];
        if (c != '=' && c != '-') return false;
        size_t k = p;
        while (k < line.size() && line[k] == c) k++;
        while (k < line.size() && (line[k] == ' ' || line[k] == '\t')) k++;
        if (k != line.size()) return false;
        levelOut = (c == '=') ? 1 : 2;
        return true;
    };
    auto isAtxHeading = [&](const std::string& line, int& levelOut, std::string& textOut) -> bool {
        size_t p = 0;
        int ind = getIndent(line, p);
        if (ind > 3 || p >= line.size() || line[p] != '#') return false;
        size_t h = 0;
        while (p + h < line.size() && h < 7 && line[p + h] == '#') h++;
        if (h == 0 || h > 6) return false;
        if (p + h < line.size() && line[p + h] != ' ' && line[p + h] != '\t') return false;
        levelOut = (int)h;
        std::string rest = trimStr(line.substr(p + h));
        if (!rest.empty() && rest.back() == '#') {
            size_t e = rest.size();
            while (e > 0 && rest[e - 1] == '#') e--;
            if (e == 0) rest.clear();
            else if (rest[e - 1] == ' ' || rest[e - 1] == '\t') {
                while (e > 0 && (rest[e - 1] == ' ' || rest[e - 1] == '\t')) e--;
                rest.erase(e);
            }
        }
        textOut = rest;
        return true;
    };
    auto isBlockquoteLine = [&](const std::string& line, std::string& innerOut) -> bool {
        size_t p = 0;
        int ind = getIndent(line, p);
        if (ind > 3 || p >= line.size() || line[p] != '>') return false;
        p++;
        if (p < line.size() && (line[p] == ' ' || line[p] == '\t')) p++;
        innerOut = line.substr(p);
        return true;
    };
    auto isListItem = [&](const std::string& line, int& indOut, bool& isOlOut, int& startNumOut, std::string& contentOut) -> bool {
        size_t p = 0;
        int ind = getIndent(line, p);
        if (p >= line.size()) return false;
        char c = line[p];
        if ((c == '-' || c == '*' || c == '+') && (p + 1 == line.size() || line[p + 1] == ' ' || line[p + 1] == '\t')) {
            if ((c == '-' || c == '*') && isThematicBreak(line)) return false;
            indOut = ind;
            isOlOut = false;
            startNumOut = 1;
            size_t cs = p + 1;
            if (cs < line.size() && (line[cs] == ' ' || line[cs] == '\t')) cs++;
            contentOut = line.substr(cs);
            return true;
        }
        size_t d = p;
        while (d < line.size() && (d - p) < 9 && line[d] >= '0' && line[d] <= '9') d++;
        if (d > p && d < line.size() && (line[d] == '.' || line[d] == ')') && (d + 1 == line.size() || line[d + 1] == ' ' || line[d + 1] == '\t')) {
            indOut = ind;
            isOlOut = true;
            startNumOut = atoi(line.substr(p, d - p).c_str());
            size_t cs = d + 1;
            if (cs < line.size() && (line[cs] == ' ' || line[cs] == '\t')) cs++;
            contentOut = line.substr(cs);
            return true;
        }
        return false;
    };

    std::vector<std::string> lines;
    lines.reserve(md.size() / 36 + 16);
    {
        size_t pos = 0, len = md.size();
        while (pos < len) {
            size_t eol = md.find('\n', pos);
            if (eol == std::string::npos) eol = len;
            std::string l = md.substr(pos, eol - pos);
            if (!l.empty() && l.back() == '\r') l.pop_back();
            lines.push_back(std::move(l));
            pos = eol + 1;
        }
    }

    std::string out; out.reserve(md.size() * 2 + 256);
    struct ListLevel { bool isOl; int indent; bool liOpen; };
    std::vector<ListLevel> listStack;
    bool inTable = false;
    std::vector<const char*> colAligns;
    std::vector<std::string> paraLines;

    auto formatParaLines = [&](const std::vector<std::string>& plines) -> std::string {
        std::string res;
        for (size_t k = 0; k < plines.size(); ++k) {
            std::string s = plines[k];
            size_t lead = 0;
            while (lead < s.size() && (s[lead] == ' ' || s[lead] == '\t')) lead++;
            s.erase(0, lead);
            bool hardBreak = false;
            if (k + 1 < plines.size()) {
                size_t trailSp = 0;
                while (trailSp < s.size() && s[s.size() - 1 - trailSp] == ' ') trailSp++;
                if (trailSp >= 2) {
                    s.erase(s.size() - trailSp);
                    hardBreak = true;
                } else if (!s.empty() && s.back() == '\\') {
                    size_t bs = 0;
                    for (size_t m = s.size(); m > 0 && s[m - 1] == '\\'; --m) bs++;
                    if ((bs % 2) == 1) {
                        s.pop_back();
                        hardBreak = true;
                    }
                }
            } else {
                while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
            }
            res += formatMarkdownInline(s);
            if (k + 1 < plines.size()) {
                res += hardBreak ? "<br>\n" : "\n";
            }
        }
        return res;
    };
    auto flushPara = [&]() {
        if (!paraLines.empty()) {
            out += "<p>"; out += formatParaLines(paraLines); out += "</p>\n";
            paraLines.clear();
        }
    };
    auto closeLists = [&]() {
        while (!listStack.empty()) {
            if (listStack.back().liOpen) out += "</li>";
            out += listStack.back().isOl ? "</ol>\n" : "</ul>\n";
            listStack.pop_back();
        }
    };
    auto closeTable = [&]() {
        if (inTable) { out += "</tbody></table>\n"; inTable = false; colAligns.clear(); }
    };

    size_t nLines = lines.size();
    for (size_t idx = 0; idx < nLines; ++idx) {
        const std::string& line = lines[idx];
        size_t firstNonSp = 0;
        int lineIndent = getIndent(line, firstNonSp);
        bool isBlank = (firstNonSp >= line.size());

        // 1. Fenced code block
        char fChar = 0; size_t fLen = 0; std::string fInfo;
        if (isFenceStart(line, fChar, fLen, fInfo)) {
            flushPara(); closeLists(); closeTable();
            std::string rawLang;
            if (!fInfo.empty()) {
                size_t sp = 0;
                while (sp < fInfo.size() && !mdIsSpace(fInfo[sp])) sp++;
                rawLang = fInfo.substr(0, sp);
            }
            std::string lowerLang = Emmet::toLowerStr(rawLang);
            if (lowerLang == "mermaid" || lowerLang == "pele" || lowerLang == "mmd") {
                std::string code;
                bool firstLine = true;
                idx++;
                while (idx < nLines && !isFenceEnd(lines[idx], fChar, fLen)) {
                    if (!firstLine) code += "\n";
                    firstLine = false;
                    code += lines[idx];
                    idx++;
                }
                const std::string& svg = cachedMermaidRenderSvg(code);
                if (!svg.empty()) {
                    out += "<div class=\"pele-diagram\">";
                    out += svg;
                    out += "</div>\n";
                } else {
                    out += "<div class=\"pele-diagram\" data-empty=\"1\"></div>\n";
                }
                continue;
            }
            if (lowerLang == "math" || lowerLang == "latex" || lowerLang == "katex") {
                std::string code;
                bool firstLine = true;
                idx++;
                while (idx < nLines && !isFenceEnd(lines[idx], fChar, fLen)) {
                    if (!firstLine) code += "\n";
                    firstLine = false;
                    code += lines[idx];
                    idx++;
                }
                const std::string& mml = cachedLatexToMathML(code, true);
                if (!mml.empty()) {
                    out += "<div class=\"math-block\" style=\"margin:1em 0;text-align:center;\">";
                    out += mml;
                    out += "</div>\n";
                }
                continue;
            }
            if (!rawLang.empty()) {
                std::string lang = escapeHtmlBasic(rawLang);
                out += "<pre><code class=\"language-"; out += lang; out += "\">";
            } else {
                out += "<pre><code>";
            }
            idx++;
            while (idx < nLines && !isFenceEnd(lines[idx], fChar, fLen)) {
                out += escapeHtmlBasic(lines[idx]);
                out += "\n";
                idx++;
            }
            out += "</code></pre>\n";
            continue;
        }

        // 1b. Display math block ($$ ... $$)
        if (!isBlank && lineIndent <= 3 && line.compare(firstNonSp, 2, "$$") == 0) {
            std::string trimmedMath = trimStr(line);
            if (trimmedMath == "$$") {
                flushPara(); closeLists(); closeTable();
                std::string code;
                bool firstLine = true;
                idx++;
                while (idx < nLines && trimStr(lines[idx]) != "$$") {
                    if (!firstLine) code += "\n";
                    firstLine = false;
                    code += lines[idx];
                    idx++;
                }
                const std::string& mml = cachedLatexToMathML(code, true);
                if (!mml.empty()) {
                    out += "<div class=\"math-block\" style=\"margin:1em 0;text-align:center;\">";
                    out += mml;
                    out += "</div>\n";
                }
                continue;
            } else if (trimmedMath.size() > 4 && trimmedMath.compare(trimmedMath.size() - 2, 2, "$$") == 0 &&
                       trimmedMath.find("$$", 2) == trimmedMath.size() - 2) {
                flushPara(); closeLists(); closeTable();
                std::string code = trimmedMath.substr(2, trimmedMath.size() - 4);
                const std::string& mml = cachedLatexToMathML(code, true);
                if (!mml.empty()) {
                    out += "<div class=\"math-block\" style=\"margin:1em 0;text-align:center;\">";
                    out += mml;
                    out += "</div>\n";
                }
                continue;
            }
        }

        // 2. Active table continuation
        if (inTable) {
            if (!isBlank && line.find('|') != std::string::npos) {
                auto cells = splitCells(line);
                size_t numCols = colAligns.size();
                out += "<tr>";
                for (size_t c = 0; c < numCols; ++c) {
                    out += "<td";
                    out += colAligns[c];
                    out += ">";
                    if (c < cells.size()) out += formatMarkdownInline(cells[c]);
                    out += "</td>";
                }
                out += "</tr>";
                continue;
            }
            closeTable();
        }

        // 3. Blank line
        if (isBlank) {
            flushPara();
            // Keep list open across a single blank line if the next non-blank line is a list item or indented continuation
            if (!listStack.empty() && idx + 1 < nLines) {
                int nxtInd = 0, nxtStart = 1; bool nxtOl = false; std::string nxtContent;
                size_t nxtFirst = 0;
                int nxtRawInd = getIndent(lines[idx + 1], nxtFirst);
                if (isListItem(lines[idx + 1], nxtInd, nxtOl, nxtStart, nxtContent) ||
                    (nxtFirst < lines[idx + 1].size() && nxtRawInd >= 2)) {
                    continue;
                }
            }
            closeLists();
            continue;
        }

        // 4. Setext heading underline (when paragraph lines are pending)
        int setextLevel = 0;
        if (!paraLines.empty() && isSetextUnderline(line, setextLevel)) {
            char tag[8]; std::snprintf(tag, sizeof(tag), "h%d", setextLevel);
            out += "<"; out += tag; out += ">";
            out += formatParaLines(paraLines);
            out += "</"; out += tag; out += ">\n";
            paraLines.clear();
            continue;
        }

        // 5. ATX heading (# .. ######)
        int atxLevel = 0; std::string atxText;
        if (isAtxHeading(line, atxLevel, atxText)) {
            flushPara(); closeLists();
            char tag[8]; std::snprintf(tag, sizeof(tag), "h%d", atxLevel);
            out += "<"; out += tag; out += ">";
            out += formatMarkdownInline(atxText);
            out += "</"; out += tag; out += ">\n";
            continue;
        }

        // 6. Thematic break (---, ***, ___)
        if (isThematicBreak(line)) {
            flushPara(); closeLists();
            out += "<hr>\n";
            continue;
        }

        // 7. Blockquote & GFM Alerts (> ...)
        std::string bqInner;
        if (isBlockquoteLine(line, bqInner)) {
            flushPara(); closeLists();
            std::vector<std::string> bqLines;
            bqLines.push_back(bqInner);
            while (idx + 1 < nLines) {
                std::string nextInner;
                if (isBlockquoteLine(lines[idx + 1], nextInner)) {
                    bqLines.push_back(nextInner);
                    idx++;
                } else {
                    break;
                }
            }
            std::string firstTrim = trimStr(bqLines[0]);
            std::string alertType, alertTitle;
            if (firstTrim.size() >= 4 && firstTrim.compare(0, 2, "[!") == 0 && firstTrim.back() == ']') {
                std::string kind = Emmet::toLowerStr(firstTrim.substr(2, firstTrim.size() - 3));
                if (kind == "note") { alertType = "note"; alertTitle = "Note"; }
                else if (kind == "tip") { alertType = "tip"; alertTitle = "Tip"; }
                else if (kind == "important") { alertType = "important"; alertTitle = "Important"; }
                else if (kind == "warning") { alertType = "warning"; alertTitle = "Warning"; }
                else if (kind == "caution") { alertType = "caution"; alertTitle = "Caution"; }
            }
            if (!alertType.empty()) {
                std::string joined;
                for (size_t k = 1; k < bqLines.size(); ++k) {
                    joined += bqLines[k];
                    if (k + 1 < bqLines.size()) joined += "\n";
                }
                out += "<div class=\"markdown-alert markdown-alert-"; out += alertType; out += "\"><p class=\"markdown-alert-title\">";
                out += alertTitle; out += "</p>";
                out += convertMarkdownToHtml(joined, depth + 1);
                out += "</div>\n";
            } else {
                std::string joined;
                for (size_t k = 0; k < bqLines.size(); ++k) {
                    joined += bqLines[k];
                    if (k + 1 < bqLines.size()) joined += "\n";
                }
                out += "<blockquote>\n";
                out += convertMarkdownToHtml(joined, depth + 1);
                out += "</blockquote>\n";
            }
            continue;
        }

        // 8. GFM Table start (Header row | Separator row)
        if (line.find('|') != std::string::npos && idx + 1 < nLines && isTableSep(lines[idx + 1])) {
            flushPara(); closeLists();
            auto hdrs = splitCells(line);
            auto seps = splitCells(lines[idx + 1]);
            if (hdrs.size() == seps.size()) {
                idx++;
                colAligns.clear();
                for (const auto& sp : seps) {
                    bool l = !sp.empty() && sp.front() == ':';
                    bool r = !sp.empty() && sp.back() == ':';
                    if (l && r) colAligns.push_back(" style=\"text-align:center\"");
                    else if (r) colAligns.push_back(" style=\"text-align:right\"");
                    else if (l) colAligns.push_back(" style=\"text-align:left\"");
                    else colAligns.push_back("");
                }
                out += "<table><thead><tr>";
                for (size_t c = 0; c < hdrs.size(); ++c) {
                    out += "<th"; out += colAligns[c]; out += ">";
                    out += formatMarkdownInline(hdrs[c]);
                    out += "</th>";
                }
                out += "</tr></thead><tbody>";
                inTable = true;
                continue;
            }
        }

        // 9. Lists (Unordered, Ordered, Nested, Task Lists)
        int liIndent = 0, liStartNum = 1; bool liIsOl = false; std::string liContent;
        if (isListItem(line, liIndent, liIsOl, liStartNum, liContent)) {
            flushPara();
            if (listStack.empty() || liIndent >= listStack.back().indent + 2) {
                if (liIsOl) {
                    if (liStartNum != 1) {
                        char buf[32]; std::snprintf(buf, sizeof(buf), "<ol start=\"%d\">", liStartNum);
                        out += buf;
                    } else {
                        out += "<ol>";
                    }
                } else {
                    out += "<ul>";
                }
                listStack.push_back({ liIsOl, liIndent, false });
            } else {
                while (listStack.size() > 1 && liIndent < listStack.back().indent) {
                    if (listStack.back().liOpen) out += "</li>";
                    out += listStack.back().isOl ? "</ol>" : "</ul>";
                    listStack.pop_back();
                }
                if (listStack.back().isOl != liIsOl) {
                    if (listStack.back().liOpen) out += "</li>";
                    out += listStack.back().isOl ? "</ol>" : "</ul>";
                    listStack.pop_back();
                    if (liIsOl) {
                        if (liStartNum != 1) {
                            char buf[32]; std::snprintf(buf, sizeof(buf), "<ol start=\"%d\">", liStartNum);
                            out += buf;
                        } else {
                            out += "<ol>";
                        }
                    } else {
                        out += "<ul>";
                    }
                    listStack.push_back({ liIsOl, liIndent, false });
                } else if (listStack.back().liOpen) {
                    out += "</li>";
                    listStack.back().liOpen = false;
                }
            }
            bool isTask = false, isChecked = false;
            if (liContent.compare(0, 4, "[ ] ") == 0 || liContent == "[ ]") {
                isTask = true; isChecked = false;
                liContent.erase(0, liContent.size() >= 4 ? 4 : 3);
            } else if (liContent.compare(0, 4, "[x] ") == 0 || liContent.compare(0, 4, "[X] ") == 0 || liContent == "[x]" || liContent == "[X]") {
                isTask = true; isChecked = true;
                liContent.erase(0, liContent.size() >= 4 ? 4 : 3);
            }
            if (isTask) {
                out += "<li class=\"task-list-item\"><input type=\"checkbox\" ";
                if (isChecked) out += "checked ";
                if (depth == 0) {
                    out += "data-line=\"" + std::to_string(idx) + "\"> ";
                } else {
                    out += "disabled> ";
                }
            } else {
                out += "<li>";
            }
            out += formatMarkdownInline(liContent);
            listStack.back().liOpen = true;
            continue;
        }

        // Indented continuation line inside an open list item
        if (!listStack.empty() && lineIndent >= 2) {
            out += " ";
            out += formatMarkdownInline(trimStr(line));
            continue;
        }

        closeLists();

        // 10. Indented code block (4+ spaces when not inside a paragraph or list)
        if (paraLines.empty() && lineIndent >= 4) {
            out += "<pre><code>";
            while (idx < nLines) {
                size_t fn = 0;
                int ind = getIndent(lines[idx], fn);
                if (fn >= lines[idx].size()) {
                    out += "\n";
                    idx++;
                    continue;
                }
                if (ind < 4) { idx--; break; }
                size_t cut = 0; int c = 0;
                while (cut < lines[idx].size() && c < 4) {
                    if (lines[idx][cut] == ' ') { c++; cut++; }
                    else if (lines[idx][cut] == '\t') { c += 4 - (c % 4); cut++; }
                    else break;
                }
                out += escapeHtmlBasic(lines[idx].substr(cut));
                out += "\n";
                idx++;
            }
            out += "</code></pre>\n";
            continue;
        }

        // 11. Accumulate paragraph line
        paraLines.push_back(line);
    }
    flushPara();
    closeLists();
    closeTable();
    return out;
}

inline bool looksLikeMermaid(const std::string& s)  noexcept{
    size_t n = s.size(), i = 0;
    if (n >= 3 && (unsigned char)s[0] == 0xEF && (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF) i = 3;
    while (i < n && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')) i++;
    if (i + 3 <= n && s[i] == '-' && s[i + 1] == '-' && s[i + 2] == '-') {
        size_t p = i + 3;
        while (p < n && (s[p] == ' ' || s[p] == '\t')) p++;
        if (p < n && (s[p] == '\r' || s[p] == '\n')) {
            while (p < n && s[p] != '\n') p++;
            if (p < n) p++;
            while (p < n) {
                size_t ls = p;
                while (p < n && s[p] != '\n') p++;
                size_t le = p;
                if (p < n) p++;
                while (ls < le && (s[ls] == ' ' || s[ls] == '\t')) ls++;
                while (le > ls && (s[le - 1] == ' ' || s[le - 1] == '\t' || s[le - 1] == '\r')) le--;
                if (le - ls == 3 && s[ls] == '-' && s[ls + 1] == '-' && s[ls + 2] == '-') { i = p; break; }
            }
        }
    }
    while (i < n) {
        while (i < n && (s[i] == ' ' || s[i] == '\t')) i++;
        if (i >= n) return false;
        if (s[i] == '\r' || s[i] == '\n') { i++; continue; }
        if (i + 1 < n && s[i] == '%' && s[i + 1] == '%') {
            while (i < n && s[i] != '\n') i++;
            continue;
        }
        size_t ts = i;
        while (i < n && (mdIsAlnum(s[i]) || s[i] == '-' || s[i] == '_')) i++;
        if (i == ts) return false;
        std::string tok = Emmet::toLowerStr(s.substr(ts, i - ts));
        size_t rs = i;
        while (rs < n && (s[rs] == ' ' || s[rs] == '\t')) rs++;
        size_t re = rs;
        while (re < n && s[re] != '\r' && s[re] != '\n') re++;
        while (re > rs && (s[re - 1] == ' ' || s[re - 1] == '\t')) re--;
        std::string rest = Emmet::toLowerStr(s.substr(rs, re - rs));
        static const char* const kUnambiguous[] = {
            "flowchart", "flowchart-elk", "sequencediagram", "classdiagram", "statediagram", "statediagram-v2",
            "erdiagram", "gitgraph", "xychart", "xychart-beta", "quadrantchart", "requirementdiagram",
            "c4context", "c4container", "c4component", "c4dynamic", "c4deployment", "eventmodeling",
            "sankey-beta", "block-beta", "packet-beta", "architecture-beta", "radar-beta", "treemap-beta",
            "venn-beta", "ishikawa-beta", "wardley-beta", "cynefin-beta", "railroad-beta", "railroad-ebnf-beta",
            "railroad-abnf-beta", "railroad-peg-beta", "swimlane-beta", "usecase-beta", "agentflow-beta", "treeview-beta",
            "gantt", "mindmap", "sankey", "kanban", "treemap", "ishikawa", "wardley", "cynefin", "swimlane", "agentflow", "treeview"
        };
        for (const char* kw : kUnambiguous) {
            if (tok == kw) return true;
        }
        if (tok == "graph") {
            if (rest.size() >= 2) {
                std::string d = rest.substr(0, 2);
                if ((d == "td" || d == "tb" || d == "bt" || d == "rl" || d == "lr") &&
                    (rest.size() == 2 || rest[2] == ' ' || rest[2] == '\t' || rest[2] == ';')) return true;
            }
            if (s.find("-->") != std::string::npos || s.find("---") != std::string::npos) return true;
        }
        if (tok == "pie") {
            if (rest.empty() || rest.rfind("title", 0) == 0 || rest.rfind("showdata", 0) == 0) return true;
        }
        if (tok == "timeline" || tok == "journey") {
            if (rest.empty() || rest.rfind("title", 0) == 0) return true;
        }
        if (tok == "info" && rest.empty()) return true;
        static const char* const kSingleLineKw[] = {
            "block", "packet", "architecture", "radar", "venn", "railroad", "usecase", "requirement"
        };
        if (rest.empty() && re < n) {
            for (const char* kw : kSingleLineKw) {
                if (tok == kw) return true;
            }
        }
        return false;
    }
    return false;
}

    inline const char* getMdThemeStyle() noexcept {
    static const char* const kMdThemeStyle =
        "<style>"
        ":root,html[data-theme=\"light\"]{"
        "color-scheme:light;--md-fg:#222;--md-hr:#d8dee4;--md-code-bg:#f3f4f6;--md-pre-bd:#d0d7de;--md-kbd-bg:#f6f8fa;--md-bq-bd:#d0d7de;--md-tbl-bd:#d0d7de;--md-tr-even:#f8fafc;"
        "--pele-bg:#ffffff;--pele-fg:#1f2328;--pele-muted:#57606a;--pele-line:#424a53;--pele-surface:#f6f8fa;--pele-surface-alt:#eaeef2;--pele-border:#8c959f;--pele-accent:#0969da;--pele-critical:#cf222e;--pele-error:#cf222e;--pele-radius:6px;"
        "--pele-series-1:#0969da;--pele-series-2:#bc4c00;--pele-series-3:#1a7f37;--pele-series-4:#cf222e;--pele-series-5:#1b7c83;--pele-series-6:#9a6700;--pele-series-7:#8250df;--pele-series-8:#57606a;"
        "--al-note-bd:#0969da;--al-note-bg:rgba(9,105,218,.08);--al-note-fg:#0969da;"
        "--al-tip-bd:#1a7f37;--al-tip-bg:rgba(26,127,55,.08);--al-tip-fg:#1a7f37;"
        "--al-imp-bd:#8250df;--al-imp-bg:rgba(130,80,223,.08);--al-imp-fg:#8250df;"
        "--al-warn-bd:#9a6700;--al-warn-bg:rgba(154,103,0,.08);--al-warn-fg:#9a6700;"
        "--al-caut-bd:#cf222e;--al-caut-bg:rgba(207,34,46,.08);--al-caut-fg:#cf222e"
        "}"
        "html[data-theme=\"dark\"]{"
        "color-scheme:dark;--md-fg:#e0e0e0;--md-hr:#383838;--md-code-bg:#2d2d2d;--md-pre-bd:#333;--md-kbd-bg:#2d2d2d;--md-bq-bd:#555;--md-tbl-bd:#444;--md-tr-even:#252526;"
        "--pele-bg:#000000;--pele-fg:#e6edf3;--pele-muted:#9198a1;--pele-line:#9da7b3;--pele-surface:#252526;--pele-surface-alt:#2d2d30;--pele-border:#484f58;--pele-accent:#58a6ff;--pele-critical:#ff7b72;--pele-error:#ff7b72;--pele-radius:6px;"
        "--pele-series-1:#58a6ff;--pele-series-2:#f0883e;--pele-series-3:#3fb950;--pele-series-4:#ff7b72;--pele-series-5:#56d4dd;--pele-series-6:#d29922;--pele-series-7:#bc8cff;--pele-series-8:#a5d6ff;"
        "--al-note-bd:#2f81f7;--al-note-bg:rgba(47,129,247,.1);--al-note-fg:#58a6ff;"
        "--al-tip-bd:#3fb950;--al-tip-bg:rgba(63,185,80,.1);--al-tip-fg:#3fb950;"
        "--al-imp-bd:#a371f7;--al-imp-bg:rgba(163,113,247,.1);--al-imp-fg:#bc8cff;"
        "--al-warn-bd:#d29922;--al-warn-bg:rgba(210,153,34,.1);--al-warn-fg:#e3b341;"
        "--al-caut-bd:#f85149;--al-caut-bg:rgba(248,81,73,.1);--al-caut-fg:#ff7b72"
        "}"
        "html{background:var(--pele-bg);color:var(--md-fg);min-height:100%}"
        "html::-webkit-scrollbar,body::-webkit-scrollbar{display:none}html,body{scrollbar-width:none}"
        "body{font-family:system-ui,-apple-system,'Segoe UI',sans-serif;background:var(--pele-bg);color:var(--md-fg);margin:16px 20px;min-height:calc(100vh - 32px);box-sizing:border-box;line-height:1.6;word-wrap:break-word}"
        "h1,h2,h3,h4,h5,h6{margin-top:20px;margin-bottom:10px;font-weight:600;line-height:1.25}"
        "h1{font-size:1.85em;padding-bottom:.28em;border-bottom:1px solid var(--md-hr)}"
        "h2{font-size:1.45em;padding-bottom:.25em;border-bottom:1px solid var(--md-hr)}"
        "h3{font-size:1.2em}p{margin:0 0 12px 0}hr{border:0;border-top:1px solid var(--md-hr);margin:18px 0}"
        "code,kbd,pre{font-family:Consolas,'Cascadia Mono','Courier New',monospace}"
        "code{background:var(--md-code-bg);padding:.18em .4em;border-radius:4px;font-size:.9em}"
        "pre{background:var(--pele-surface);border:1px solid var(--md-pre-bd);padding:12px 14px;border-radius:6px;overflow:auto;line-height:1.45;margin:12px 0}"
        "pre code{background:transparent;padding:0;border-radius:0;font-size:.9em}"
        "kbd{background:var(--md-kbd-bg);border:1px solid var(--md-bq-bd);border-bottom-width:2px;border-radius:4px;padding:1px 6px;font-size:.85em}"
        "blockquote{border-left:4px solid var(--md-bq-bd);margin:12px 0;padding:2px 14px;color:var(--pele-muted)}"
        "blockquote>:first-child{margin-top:0}blockquote>:last-child{margin-bottom:0}"
        ".markdown-alert{padding:8px 14px;margin:12px 0;border-left:4px solid;border-radius:4px}.markdown-alert>:last-child{margin-bottom:0}.markdown-alert-title{font-weight:600;margin:0 0 4px 0}"
        ".markdown-alert-note{border-color:var(--al-note-bd);background:var(--al-note-bg)}.markdown-alert-note .markdown-alert-title{color:var(--al-note-fg)}"
        ".markdown-alert-tip{border-color:var(--al-tip-bd);background:var(--al-tip-bg)}.markdown-alert-tip .markdown-alert-title{color:var(--al-tip-fg)}"
        ".markdown-alert-important{border-color:var(--al-imp-bd);background:var(--al-imp-bg)}.markdown-alert-important .markdown-alert-title{color:var(--al-imp-fg)}"
        ".markdown-alert-warning{border-color:var(--al-warn-bd);background:var(--al-warn-bg)}.markdown-alert-warning .markdown-alert-title{color:var(--al-warn-fg)}"
        ".markdown-alert-caution{border-color:var(--al-caut-bd);background:var(--al-caut-bg)}.markdown-alert-caution .markdown-alert-title{color:var(--al-caut-fg)}"
        "ul,ol{padding-left:2em;margin:0 0 12px 0}ul ul,ul ol,ol ul,ol ol{margin-bottom:0}li+li{margin-top:.2em}"
        "li.task-list-item{list-style-type:none}li.task-list-item input[type=checkbox]{margin:0 .4em .15em -1.4em;vertical-align:middle}"
        "a{color:var(--pele-accent);text-decoration:none}a:hover{text-decoration:underline}"
        "table{border-collapse:collapse;margin:12px 0}th,td{border:1px solid var(--md-tbl-bd);padding:6px 13px}th{background:var(--md-kbd-bg);font-weight:600}tr:nth-child(2n){background:var(--md-tr-even)}"
        ".pele-diagram{position:relative;margin:12px 0;padding:14px 16px;border-radius:6px;border:1px solid var(--md-pre-bd);background:var(--pele-bg);text-align:center;overflow:hidden}"
        ".pele-diagram.pele-full{margin:0;min-height:calc(100vh - 34px);box-sizing:border-box}"
        ".pele-diagram>svg.pele{display:block;margin:0 auto}"
        ".pele-zoom button:hover{background:var(--pele-surface-alt)!important}"
        ".miu-trunc{margin:16px 0;padding:8px 12px;border-radius:6px;border:1px dashed var(--md-bq-bd);background:var(--pele-surface);color:var(--pele-muted);font-size:12px}"
        "</style>";

    return kMdThemeStyle;
}


inline const std::string& getPreviewCspNonce() noexcept {
    static std::string s_nonce;
    if (s_nonce.empty()) {
        std::random_device rd;
        uint64_t a = ((uint64_t)rd() << 32) ^ (uint64_t)rd() ^ (uint64_t)std::chrono::high_resolution_clock::now().time_since_epoch().count();
        uint64_t b = ((uint64_t)rd() << 32) ^ (uint64_t)rd() ^ (uintptr_t)&s_nonce ^ 0x9E3779B97F4A7C15ULL;
        a ^= a >> 30; a *= 0xBF58476D1CE4E5B9ULL; a ^= a >> 27; a *= 0x94D049BB133111EBULL; a ^= a >> 31;
        b ^= b >> 30; b *= 0xBF58476D1CE4E5B9ULL; b *= 0x94D049BB133111EBULL; b ^= b >> 31;
        char hex[36] = {};
        std::snprintf(hex, sizeof(hex), "%08X%08X%08X%08X", (uint32_t)(a >> 32), (uint32_t)a, (uint32_t)(b >> 32), (uint32_t)b);
        s_nonce = hex;
    }
    return s_nonce;
}

inline std::string buildDomUpdateJs(const std::string& bodyContent, bool isDarkMode) {
    std::string js;
    js.reserve(bodyContent.size() + 128);
    js += "(function(h,d){window.__miuUpdate?window.__miuUpdate(h,d):(window.__miuPending={h:h,dk:d});})(\"";
    for (size_t i = 0; i < bodyContent.size(); ++i) {
        unsigned char c = (unsigned char)bodyContent[i];
        if (c == '\\') js += "\\\\";
        else if (c == '"') js += "\\\"";
        else if (c == '\n') js += "\\n";
        else if (c == '\r') js += "\\r";
        else if (c == 0xE2 && i + 2 < bodyContent.size() && (unsigned char)bodyContent[i + 1] == 0x80 &&
                 ((unsigned char)bodyContent[i + 2] == 0xA8 || (unsigned char)bodyContent[i + 2] == 0xA9)) {
            js += ((unsigned char)bodyContent[i + 2] == 0xA8) ? "\\u2028" : "\\u2029";
            i += 2;
        } else {
            js += (char)c;
        }
    }
    js += isDarkMode ? "\",true);" : "\",false);";
    return js;
}

struct PreviewRenderResult {
    bool isMdOrMermaidShell = false;
    std::string bodyContent;
    std::string bodyHtml;
    std::string fullHtml;
    std::string domUpdateJs;
};

inline PreviewRenderResult renderPreview(
    const std::string& rawInput,
    bool isTruncated,
    const std::string& truncatedMessageUtf8,
    const std::wstring& currentExtension,
    bool isDarkMode,
    const std::string& baseHrefTag = "")
{
    PreviewRenderResult res;
    std::string truncNotice;
    if (isTruncated) {
        truncNotice = "<div class=\"miu-trunc\">&#x2026; " + escapeHtmlBasic(truncatedMessageUtf8) + "</div>";
    }
    const char* kMdThemeStyle = getMdThemeStyle();
    std::string lowerHead = Emmet::toLowerStr(rawInput.substr(0, std::min(rawInput.size(), (size_t)2048)));
    bool looksLikeHtmlDoc = (lowerHead.find("<!doctype") != std::string::npos || lowerHead.find("<html") != std::string::npos ||
                             lowerHead.find("<head") != std::string::npos || lowerHead.find("<body") != std::string::npos ||
                             lowerHead.find("<script") != std::string::npos || lowerHead.find("<style") != std::string::npos ||
                             lowerHead.find("<svg") != std::string::npos);
    bool isMermaid = (currentExtension == L"mmd" || currentExtension == L"mermaid" ||
                      ((currentExtension.empty() || currentExtension == L"txt") && !looksLikeHtmlDoc && looksLikeMermaid(rawInput)));
    bool isMd = (!isMermaid && (currentExtension == L"md" || currentExtension == L"markdown" || currentExtension == L"mdx" ||
                 (currentExtension.empty() && !looksLikeHtmlDoc && (rawInput.find('<') == std::string::npos || rawInput.find('#') != std::string::npos || rawInput.find("```") != std::string::npos || rawInput.find("~~~") != std::string::npos || rawInput.find("|") != std::string::npos || rawInput.find("**") != std::string::npos || rawInput.find("- ") != std::string::npos || rawInput.find("> ") != std::string::npos || rawInput.find("$") != std::string::npos))));
    if (isMermaid || isMd) {
        res.isMdOrMermaidShell = true;
        if (isMermaid) {
            const std::string& svg = cachedMermaidRenderSvg(rawInput);
            if (!svg.empty()) {
                res.bodyContent = "<div class=\"pele-diagram pele-full\">";
                res.bodyContent += svg;
                res.bodyContent += "</div>";
            } else {
                res.bodyContent = "<div class=\"pele-diagram pele-full\" data-empty=\"1\"></div>";
            }
            res.bodyContent += truncNotice;
        } else {
            res.bodyContent = convertMarkdownToHtml(rawInput);
            res.bodyContent += truncNotice;
        }
        res.bodyHtml = res.bodyContent;
        res.domUpdateJs = buildDomUpdateJs(res.bodyContent, isDarkMode);
        const std::string& nonce = getPreviewCspNonce();
        std::string& html = res.fullHtml;
        html.reserve(res.bodyContent.size() + 3072);
        html += "<!DOCTYPE html><html data-theme=\"";
        html += (isDarkMode ? "dark" : "light");
        html += "\"><head><meta charset=\"utf-8\"><meta http-equiv=\"Content-Security-Policy\" content=\"default-src 'none'; script-src 'nonce-";
        html += nonce;
        html += "'; style-src 'unsafe-inline'; img-src https: http: data: file:; font-src https: data: file:; media-src https: http: data: file:; connect-src 'none'; object-src 'none'; frame-src 'none'; form-action 'none';\">";
        html += baseHrefTag;
        html += kMdThemeStyle;
        html += "</head><body><div id=\"__miu_root\">";
        html += res.bodyContent;
        html += "</div><script nonce=\"";
        html += nonce;
        html += "\">(function(){"
            "window.__miuUpdate=function(h,dk){"
            "document.documentElement.setAttribute('data-theme',dk?'dark':'light');"
            "var r=document.getElementById('__miu_root');if(!r)return;"
            "var t=document.createElement('div');t.innerHTML=h;"
            "var od=r.querySelectorAll('.pele-diagram'),nd=t.querySelectorAll('.pele-diagram');"
            "var m=Math.min(od.length,nd.length);"
            "for(var i=0;i<m;i++){"
            "if(nd[i].getAttribute('data-empty')==='1'&&od[i].querySelector('svg.pele')){"
            "nd[i].innerHTML=od[i].innerHTML;nd[i].removeAttribute('data-empty');"
            "}"
            "}"
            "var nc=Array.from(t.childNodes);"
            "for(var j=0;j<nc.length;j++){"
            "if(j<r.childNodes.length){if(!r.childNodes[j].isEqualNode(nc[j]))r.replaceChild(nc[j],r.childNodes[j]);}"
            "else r.appendChild(nc[j]);"
            "}"
            "while(r.childNodes.length>nc.length)r.removeChild(r.lastChild);"
            "};"
            "document.addEventListener('change',function(e){"
            "var t=e.target;"
            "if(t&&t.tagName==='INPUT'&&t.type==='checkbox'&&t.hasAttribute('data-line')){"
            "var dl=t.getAttribute('data-line');"
            "if(window.webkit&&window.webkit.messageHandlers&&window.webkit.messageHandlers.miuPreview){"
            "window.webkit.messageHandlers.miuPreview.postMessage('toggle_task:'+dl);"
            "}else if(window.chrome&&window.chrome.webview){"
            "window.chrome.webview.postMessage('toggle_task:'+dl);"
            "}"
            "}"
            "});"
            "if(window.__miuPending){var p=window.__miuPending;window.__miuPending=null;window.__miuUpdate(p.h,p.dk);}"
            "})();</script></body></html>";
        return res;
    }

    res.isMdOrMermaidShell = false;
    std::string& html = res.fullHtml;
    html.reserve(rawInput.size() + 1024);
    const char* webDefStyle = isDarkMode
        ? "<style>:where(:root,body){color-scheme:dark;background:#000000;color:#e6edf3}html,body{min-height:100%;scrollbar-width:none}html::-webkit-scrollbar,body::-webkit-scrollbar{display:none}</style>"
        : "<style>:where(:root,body){color-scheme:light;background:#ffffff;color:#1f2328}html,body{min-height:100%;scrollbar-width:none}html::-webkit-scrollbar,body::-webkit-scrollbar{display:none}</style>";
    std::string lower = Emmet::toLowerStr(rawInput.substr(0, std::min(rawInput.size(), (size_t)1024)));
    bool hasFullHtml = (lower.find("<html") != std::string::npos || lower.find("<!doctype") != std::string::npos);
    if (hasFullHtml) {
        const char* bTag = (!baseHrefTag.empty() && lower.find("<base") == std::string::npos) ? baseHrefTag.c_str() : "";
        size_t hp = lower.find("<head>");
        if (hp != std::string::npos) { html.append(rawInput, 0, hp + 6); html += bTag; html += webDefStyle; html.append(rawInput, hp + 6, std::string::npos); }
        else { html += bTag; html += webDefStyle; html += rawInput; }
        html += truncNotice;
    } else {
        bool isWebExt = (currentExtension.empty() || currentExtension == L"html" || currentExtension == L"htm" || currentExtension == L"xhtml" || currentExtension == L"shtml" || currentExtension == L"svg" || currentExtension == L"xml" || currentExtension == L"vue" || currentExtension == L"svelte" || currentExtension == L"astro" || currentExtension == L"php");
        if (isWebExt || rawInput.find('<') != std::string::npos) {
            html += "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"; html += baseHrefTag; html += webDefStyle; html += "</head><body>"; html += rawInput; html += truncNotice; html += "</body></html>";
        } else {
            html += "<!DOCTYPE html><html data-theme=\""; html += (isDarkMode ? "dark" : "light"); html += "\"><head><meta charset=\"utf-8\">"; html += kMdThemeStyle; html += "</head><body><pre>"; html += escapeHtmlBasic(rawInput); html += "</pre>"; html += truncNotice; html += "</body></html>";
        }
    }
    res.bodyHtml = res.bodyContent;
    return res;
}

inline PreviewRenderResult renderPreview(
    const std::string& rawInput,
    const std::string& currentFileExtUtf8,
    bool isDarkMode,
    const std::string& /*customNonce*/ = "")
{
    std::wstring wext;
    for (unsigned char c : currentFileExtUtf8) {
        if (wext.empty() && c == '.') continue;
        wext.push_back((wchar_t)std::tolower(c));
    }
    return renderPreview(rawInput, false, "", wext, isDarkMode, "");
}

} // namespace MiuPreview
