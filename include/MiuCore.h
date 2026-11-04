#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cctype>

#if defined(__AVX2__)
#include <immintrin.h>
#elif defined(_M_X64) || defined(_M_AMD64) || defined(__SSE2__)
#include <emmintrin.h>
#if defined(_MSC_VER)
#include <intrin.h>
#endif
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#endif

namespace MiuCore {

// ============================================================================
// 1. UTF-8 / UTF-16 / Encoding & Line-Ending Utilities
// ============================================================================

inline bool IsValidUtf8(const char* buf, size_t len) noexcept {
    if (!buf || len == 0) return true;
    size_t i = 0;
    while (i < len) {
        unsigned char c = (unsigned char)buf[i];
        if (c == 0x00) return false;
        if (c == 0x1B && i + 2 < len) {
            unsigned char b1 = (unsigned char)buf[i + 1];
            unsigned char b2 = (unsigned char)buf[i + 2];
            if (b1 == '$' && (b2 == '@' || b2 == 'B' || b2 == '(')) return false;
            if (b1 == '(' && (b2 == 'B' || b2 == 'J' || b2 == 'I')) return false;
        }
        if (c <= 0x7F) {
            i++;
        } else if (c >= 0xC2 && c <= 0xDF) {
            if (i + 1 >= len || ((unsigned char)buf[i + 1] & 0xC0) != 0x80) return false;
            i += 2;
        } else if (c >= 0xE0 && c <= 0xEF) {
            if (i + 2 >= len) return false;
            unsigned char b1 = (unsigned char)buf[i + 1];
            unsigned char b2 = (unsigned char)buf[i + 2];
            if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80 || (c == 0xE0 && b1 < 0xA0) || (c == 0xED && b1 >= 0xA0)) return false;
            i += 3;
        } else if (c >= 0xF0 && c <= 0xF4) {
            if (i + 3 >= len) return false;
            unsigned char b1 = (unsigned char)buf[i + 1];
            unsigned char b2 = (unsigned char)buf[i + 2];
            unsigned char b3 = (unsigned char)buf[i + 3];
            if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80 || (b3 & 0xC0) != 0x80 || (c == 0xF0 && b1 < 0x90) || (c == 0xF4 && b1 >= 0x90)) return false;
            i += 4;
        } else {
            return false;
        }
    }
    return true;
}

inline std::string SanitizeUtf8(const char* buf, size_t len) {
    if (!buf || len == 0) return "";
    bool clean = true;
    for (size_t i = 0; i < len; ) {
        unsigned char c = (unsigned char)buf[i];
        if (c == 0x00) { clean = false; break; }
        if (c <= 0x7F) {
            i++;
        } else if (c >= 0xC2 && c <= 0xDF) {
            if (i + 1 >= len || ((unsigned char)buf[i + 1] & 0xC0) != 0x80) { clean = false; break; }
            i += 2;
        } else if (c >= 0xE0 && c <= 0xEF) {
            if (i + 2 >= len) { clean = false; break; }
            unsigned char b1 = (unsigned char)buf[i + 1];
            unsigned char b2 = (unsigned char)buf[i + 2];
            if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80 || (c == 0xE0 && b1 < 0xA0) || (c == 0xED && b1 >= 0xA0)) {
                clean = false; break;
            }
            i += 3;
        } else if (c >= 0xF0 && c <= 0xF4) {
            if (i + 3 >= len) { clean = false; break; }
            unsigned char b1 = (unsigned char)buf[i + 1];
            unsigned char b2 = (unsigned char)buf[i + 2];
            unsigned char b3 = (unsigned char)buf[i + 3];
            if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80 || (b3 & 0xC0) != 0x80 || (c == 0xF0 && b1 < 0x90) || (c == 0xF4 && b1 >= 0x90)) {
                clean = false; break;
            }
            i += 4;
        } else {
            clean = false; break;
        }
    }
    if (clean) return std::string(buf, len);

    std::string out;
    out.reserve(len);
    for (size_t i = 0; i < len; ) {
        unsigned char c = (unsigned char)buf[i];
        if (c == 0x00) {
            out.push_back('?');
            i++;
        } else if (c <= 0x7F) {
            out.push_back((char)c);
            i++;
        } else if (c >= 0xC2 && c <= 0xDF && i + 1 < len && ((unsigned char)buf[i + 1] & 0xC0) == 0x80) {
            out.append(buf + i, 2);
            i += 2;
        } else if (c >= 0xE0 && c <= 0xEF && i + 2 < len) {
            unsigned char b1 = (unsigned char)buf[i + 1];
            unsigned char b2 = (unsigned char)buf[i + 2];
            if ((b1 & 0xC0) == 0x80 && (b2 & 0xC0) == 0x80 && !(c == 0xE0 && b1 < 0xA0) && !(c == 0xED && b1 >= 0xA0)) {
                out.append(buf + i, 3);
                i += 3;
            } else {
                out.append("\xEF\xBF\xBD");
                i++;
            }
        } else if (c >= 0xF0 && c <= 0xF4 && i + 3 < len) {
            unsigned char b1 = (unsigned char)buf[i + 1];
            unsigned char b2 = (unsigned char)buf[i + 2];
            unsigned char b3 = (unsigned char)buf[i + 3];
            if ((b1 & 0xC0) == 0x80 && (b2 & 0xC0) == 0x80 && (b3 & 0xC0) == 0x80 && !(c == 0xF0 && b1 < 0x90) && !(c == 0xF4 && b1 >= 0x90)) {
                out.append(buf + i, 4);
                i += 4;
            } else {
                out.append("\xEF\xBF\xBD");
                i++;
            }
        } else {
            out.append("\xEF\xBF\xBD");
            i++;
        }
    }
    return out;
}

inline std::string SanitizeUtf8(const std::string& s) {
    return SanitizeUtf8(s.data(), s.size());
}

inline std::string Utf16ToUtf8(const char* data, size_t len, bool isBigEndian) {
    if (!data || len < 2) return "";
    size_t offset = 0;
    if (!isBigEndian && (unsigned char)data[0] == 0xFF && (unsigned char)data[1] == 0xFE) offset = 2;
    else if (isBigEndian && (unsigned char)data[0] == 0xFE && (unsigned char)data[1] == 0xFF) offset = 2;

    std::string res;
    res.reserve(len);
    for (size_t i = offset; i + 1 < len; i += 2) {
        uint16_t u1 = isBigEndian
            ? (((uint16_t)(unsigned char)data[i] << 8) | (unsigned char)data[i + 1])
            : (((uint16_t)(unsigned char)data[i + 1] << 8) | (unsigned char)data[i]);
        if (u1 == 0) {
            res.push_back('?');
            continue;
        }
        uint32_t cp = u1;
        if (u1 >= 0xD800 && u1 <= 0xDBFF) {
            if (i + 3 < len) {
                uint16_t u2 = isBigEndian
                    ? (((uint16_t)(unsigned char)data[i + 2] << 8) | (unsigned char)data[i + 3])
                    : (((uint16_t)(unsigned char)data[i + 3] << 8) | (unsigned char)data[i + 2]);
                if (u2 >= 0xDC00 && u2 <= 0xDFFF) {
                    cp = 0x10000u + (((uint32_t)(u1 - 0xD800u) << 10) | (uint32_t)(u2 - 0xDC00u));
                    i += 2;
                } else {
                    cp = 0xFFFDu;
                }
            } else {
                cp = 0xFFFDu;
            }
        } else if (u1 >= 0xDC00 && u1 <= 0xDFFF) {
            cp = 0xFFFDu;
        }

        if (cp < 0x80u) {
            res.push_back((char)cp);
        } else if (cp < 0x800u) {
            res.push_back((char)(0xC0u | (cp >> 6)));
            res.push_back((char)(0x80u | (cp & 0x3Fu)));
        } else if (cp < 0x10000u) {
            res.push_back((char)(0xE0u | (cp >> 12)));
            res.push_back((char)(0x80u | ((cp >> 6) & 0x3Fu)));
            res.push_back((char)(0x80u | (cp & 0x3Fu)));
        } else {
            res.push_back((char)(0xF0u | (cp >> 18)));
            res.push_back((char)(0x80u | ((cp >> 12) & 0x3Fu)));
            res.push_back((char)(0x80u | ((cp >> 6) & 0x3Fu)));
            res.push_back((char)(0x80u | (cp & 0x3Fu)));
        }
    }
    return res;
}

inline std::string Utf8ToUtf16Bytes(const char* data, size_t len, bool isBigEndian, bool includeBom = true) {
    std::string out;
    out.reserve(2 + len * 2);
    if (includeBom) {
        if (isBigEndian) {
            out.push_back((char)0xFE);
            out.push_back((char)0xFF);
        } else {
            out.push_back((char)0xFF);
            out.push_back((char)0xFE);
        }
    }
    auto appendU16 = [&](uint16_t w) {
        if (isBigEndian) {
            out.push_back((char)((w >> 8) & 0xFF));
            out.push_back((char)(w & 0xFF));
        } else {
            out.push_back((char)(w & 0xFF));
            out.push_back((char)((w >> 8) & 0xFF));
        }
    };
    const unsigned char* p = (const unsigned char*)data;
    const unsigned char* end = p + len;
    while (p < end) {
        uint32_t cp = 0;
        if (*p < 0x80) {
            cp = *p++;
        } else if ((*p & 0xE0) == 0xC0 && p + 1 < end) {
            cp = ((uint32_t)(p[0] & 0x1F) << 6) | (uint32_t)(p[1] & 0x3F);
            p += 2;
        } else if ((*p & 0xF0) == 0xE0 && p + 2 < end) {
            cp = ((uint32_t)(p[0] & 0x0F) << 12) | ((uint32_t)(p[1] & 0x3F) << 6) | (uint32_t)(p[2] & 0x3F);
            p += 3;
        } else if ((*p & 0xF8) == 0xF0 && p + 3 < end) {
            cp = ((uint32_t)(p[0] & 0x07) << 18) | ((uint32_t)(p[1] & 0x3F) << 12) | ((uint32_t)(p[2] & 0x3F) << 6) | (uint32_t)(p[3] & 0x3F);
            p += 4;
        } else {
            cp = 0xFFFDu;
            p++;
        }
        if (cp <= 0xFFFFu) {
            appendU16((uint16_t)cp);
        } else if (cp <= 0x10FFFFu) {
            cp -= 0x10000u;
            appendU16((uint16_t)(0xD800u + (cp >> 10)));
            appendU16((uint16_t)(0xDC00u + (cp & 0x3FFu)));
        } else {
            appendU16(0xFFFDu);
        }
    }
    return out;
}

inline std::string Utf8ToUtf16Bytes(const std::string& utf8, bool isBigEndian, bool includeBom = true) {
    return Utf8ToUtf16Bytes(utf8.data(), utf8.size(), isBigEndian, includeBom);
}

// Returns 1 for BOM-less UTF-16LE, 2 for BOM-less UTF-16BE, 0 otherwise
inline int DetectBomlessUtf16(const unsigned char* udata, size_t len) noexcept {
    if (!udata || len < 4 || (len % 2) != 0) return 0;
    size_t samplePairs = (len / 2 > 2048) ? 2048 : (len / 2);
    size_t evenZeros = 0, oddZeros = 0;
    for (size_t k = 0; k < samplePairs; ++k) {
        if (udata[k * 2] == 0x00) evenZeros++;
        if (udata[k * 2 + 1] == 0x00) oddZeros++;
    }
    if (oddZeros * 4 >= samplePairs && evenZeros * 20 <= samplePairs) return 1;
    if (evenZeros * 4 >= samplePairs && oddZeros * 20 <= samplePairs) return 2;
    return 0;
}

inline int DetectBomlessUtf16(const char* data, size_t len) noexcept {
    return DetectBomlessUtf16((const unsigned char*)data, len);
}

inline const char* SelectCedSampleWindow(const char* data, size_t len, size_t& outSampleLen) noexcept {
    size_t maxSample = 65536;
    if (len <= maxSample) {
        outSampleLen = len;
        return data;
    }
    const unsigned char* udata = (const unsigned char*)data;
    size_t firstNonAscii = 0;
    while (firstNonAscii < len) {
        unsigned char ch = udata[firstNonAscii];
        if (ch >= 0x80 || ch == 0x1B || ch == 0x00) break;
        firstNonAscii++;
    }
    if (firstNonAscii < len && firstNonAscii > 4096) {
        size_t startOff = firstNonAscii - 4096;
        size_t rem = len - startOff;
        outSampleLen = (rem > maxSample) ? maxSample : rem;
        return data + startOff;
    }
    outSampleLen = maxSample;
    return data;
}

inline void SelectCedSampleWindow(const char* data, size_t len, const char*& outSampleBuf, size_t& outSampleLen) noexcept {
    outSampleBuf = SelectCedSampleWindow(data, len, outSampleLen);
}

inline std::string DetectLineEnding(const char* buf, size_t len, const char* defaultNewline = "\r\n") {
    if (!buf || len == 0) return defaultNewline;
    size_t checkLen = (len > 65536) ? 65536 : len;
    for (size_t i = 0; i < checkLen; ++i) {
        if (buf[i] == '\r') {
            if (i + 1 < len && buf[i + 1] == '\n') return "\r\n";
            return "\r";
        } else if (buf[i] == '\n') {
            return "\n";
        }
    }
    return defaultNewline;
}

inline std::string NormalizeLineEndings(std::string_view text, std::string_view targetNewline) {
    if (text.empty()) return "";
    // Fast check if any normalization is needed
    bool needsChange = false;
    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (c == '\r') {
            if (i + 1 < text.size() && text[i + 1] == '\n') {
                if (targetNewline != "\r\n") { needsChange = true; break; }
                i++;
            } else {
                if (targetNewline != "\r") { needsChange = true; break; }
            }
        } else if (c == '\n') {
            if (targetNewline != "\n") { needsChange = true; break; }
        }
    }
    if (!needsChange) return std::string(text);

    std::string out;
    out.reserve(text.size() + 16);
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\r') {
            if (i + 1 < text.size() && text[i + 1] == '\n') i++;
            out.append(targetNewline);
        } else if (text[i] == '\n') {
            out.append(targetNewline);
        } else {
            out.push_back(text[i]);
        }
    }
    return out;
}

// ============================================================================
// 2. Optimized PieceTable, Cursor, EditBatch & UndoManager (O(1) Piece Cache)
// ============================================================================

struct Piece {
    bool isOriginal;
    size_t start;
    size_t len;
};

struct PieceTable {
    const char* origPtr = nullptr;
    size_t origSize = 0;
    mutable size_t totalLength = 0;
    std::string addBuf;
    std::vector<Piece> pieces;
    mutable size_t cachedIdx = 0;
    mutable size_t cachedStart = 0;

    void syncTotalLengthIfNeeded() const noexcept {
        if (totalLength == 0 && !pieces.empty()) {
            size_t s = 0;
            for (const auto& p : pieces) s += p.len;
            totalLength = s;
        }
    }

    void initFromFile(const char* data, size_t size) {
        origPtr = data;
        origSize = size;
        totalLength = size;
        pieces.clear();
        addBuf.clear();
        cachedIdx = 0;
        cachedStart = 0;
        if (size > 0) pieces.push_back({ true, 0, size });
    }

    void initEmpty() {
        origPtr = nullptr;
        origSize = 0;
        totalLength = 0;
        pieces.clear();
        addBuf.clear();
        cachedIdx = 0;
        cachedStart = 0;
    }

    size_t length() const noexcept {
        syncTotalLengthIfNeeded();
        return totalLength;
    }

    // Locate the piece containing byte offset `pos` (where 0 <= pos < totalLength) in O(1) for sequential/nearby access
    bool locatePiece(size_t pos, size_t& outIdx, size_t& outStart) const noexcept {
        syncTotalLengthIfNeeded();
        if (pieces.empty() || pos >= totalLength) return false;
        if (pieces.size() == 1) {
            outIdx = 0;
            outStart = 0;
            cachedIdx = 0;
            cachedStart = 0;
            return true;
        }
        if (cachedIdx < pieces.size()) {
            const Piece& cp = pieces[cachedIdx];
            if (pos >= cachedStart && pos < cachedStart + cp.len) {
                outIdx = cachedIdx;
                outStart = cachedStart;
                return true;
            }
            if (pos >= cachedStart + cp.len) {
                // Walk forward from cachedIdx
                size_t cur = cachedStart + cp.len;
                for (size_t i = cachedIdx + 1; i < pieces.size(); ++i) {
                    const Piece& p = pieces[i];
                    if (pos < cur + p.len) {
                        cachedIdx = i;
                        cachedStart = cur;
                        outIdx = i;
                        outStart = cur;
                        return true;
                    }
                    cur += p.len;
                }
            } else if (cachedIdx > 0) {
                // Decide whether walking backward from cachedIdx or forward from 0 is shorter
                if (cachedStart - pos <= pos) {
                    size_t cur = cachedStart;
                    for (size_t i = cachedIdx; i > 0; --i) {
                        const Piece& p = pieces[i - 1];
                        cur -= p.len;
                        if (pos >= cur && pos < cur + p.len) {
                            cachedIdx = i - 1;
                            cachedStart = cur;
                            outIdx = i - 1;
                            outStart = cur;
                            return true;
                        }
                    }
                }
            }
        }
        size_t cur = 0;
        for (size_t i = 0; i < pieces.size(); ++i) {
            const Piece& p = pieces[i];
            if (cur + p.len <= pos) { cur += p.len; continue; }
            cachedIdx = i;
            cachedStart = cur;
            outIdx = i;
            outStart = cur;
            return true;
        }
        return false;
    }

    std::string getRange(size_t pos, size_t count) const {
        std::string out;
        getRange(pos, count, out);
        return out;
    }

    std::string getText(size_t pos, size_t count) const {
        return getRange(pos, count);
    }

    void getRange(size_t pos, size_t count, std::string& out) const {
        syncTotalLengthIfNeeded();
        out.clear();
        if (count == 0 || pos >= totalLength) return;
        if (pos + count > totalLength) count = totalLength - pos;
        out.reserve(count);

        size_t idx = 0, cur = 0;
        if (!locatePiece(pos, idx, cur)) return;

        for (; idx < pieces.size(); ++idx) {
            const Piece& p = pieces[idx];
            size_t localStart = (pos > cur) ? (pos - cur) : 0;
            size_t take = std::min(p.len - localStart, count - out.size());
            if (take == 0) break;
            if (p.isOriginal) out.append(origPtr + p.start + localStart, take);
            else out.append(addBuf.data() + p.start + localStart, take);
            if (out.size() >= count) {
                cachedIdx = idx;
                cachedStart = cur;
                break;
            }
            cur += p.len;
        }
    }

    void insert(size_t pos, const std::string& s) {
        if (s.empty()) return;
        syncTotalLengthIfNeeded();
        if (pos > totalLength) pos = totalLength;
        size_t cur = 0;
        size_t idx = 0;
        if (!pieces.empty() && pos > 0) {
            if (pos == totalLength) {
                idx = pieces.size();
                cur = totalLength;
            } else if (locatePiece(pos, idx, cur)) {
                if (pos == cur) {
                    // Insert right before pieces[idx]
                }
            }
        }
        if (idx < pieces.size()) {
            Piece p = pieces[idx];
            size_t offsetInPiece = pos - cur;
            if (offsetInPiece > 0 && offsetInPiece < p.len) {
                pieces[idx] = { p.isOriginal, p.start, offsetInPiece };
                pieces.insert(pieces.begin() + idx + 1, { p.isOriginal, p.start + offsetInPiece, p.len - offsetInPiece });
                cur += offsetInPiece;
                idx++;
            } else if (offsetInPiece == p.len) {
                cur += p.len;
                idx++;
            }
        } else {
            idx = pieces.size();
            cur = totalLength;
        }
        size_t addStart = addBuf.size();
        addBuf.append(s);
        pieces.insert(pieces.begin() + idx, { false, addStart, s.size() });
        totalLength += s.size();
        coalesceAround(idx, cur);
    }

    void erase(size_t pos, size_t count) {
        syncTotalLengthIfNeeded();
        if (count == 0 || pos >= totalLength) return;
        size_t idx = 0, cur = 0;
        if (!locatePiece(pos, idx, cur)) return;
        size_t remaining = count;
        if (pos > cur) {
            Piece p = pieces[idx];
            size_t leftLen = pos - cur;
            pieces[idx] = { p.isOriginal, p.start, leftLen };
            pieces.insert(pieces.begin() + idx + 1, { p.isOriginal, p.start + leftLen, p.len - leftLen });
            cur += leftLen;
            idx++;
        }
        size_t erased = 0;
        while (idx < pieces.size() && remaining > 0) {
            if (pieces[idx].len <= remaining) {
                remaining -= pieces[idx].len;
                erased += pieces[idx].len;
                pieces.erase(pieces.begin() + idx);
            } else {
                pieces[idx].start += remaining;
                pieces[idx].len -= remaining;
                erased += remaining;
                remaining = 0;
            }
        }
        totalLength = (totalLength >= erased) ? (totalLength - erased) : 0;
        if (idx > 0 && idx >= pieces.size()) {
            idx = pieces.size() - 1;
            cur = (cur >= pieces[idx].len) ? (cur - pieces[idx].len) : 0;
        }
        coalesceAround(idx, cur);
    }

    void coalesceAround(size_t idx, size_t curAtIdx = 0) {
        if (pieces.empty()) {
            cachedIdx = 0;
            cachedStart = 0;
            return;
        }
        if (idx >= pieces.size()) {
            idx = pieces.size() - 1;
        }
        if (idx > 0) {
            Piece& a = pieces[idx - 1];
            Piece& b = pieces[idx];
            if (!a.isOriginal && !b.isOriginal && (a.start + a.len == b.start)) {
                if (curAtIdx >= a.len) curAtIdx -= a.len;
                a.len += b.len;
                pieces.erase(pieces.begin() + idx);
                idx--;
            }
        }
        if (idx + 1 < pieces.size()) {
            Piece& a = pieces[idx];
            Piece& b = pieces[idx + 1];
            if (!a.isOriginal && !b.isOriginal && (a.start + a.len == b.start)) {
                a.len += b.len;
                pieces.erase(pieces.begin() + idx + 1);
            }
        }
        // Re-verify cachedIdx / cachedStart so subsequent operations stay O(1)
        if (curAtIdx > 0 && idx < pieces.size()) {
            cachedIdx = idx;
            cachedStart = curAtIdx;
        } else {
            cachedIdx = 0;
            cachedStart = 0;
        }
    }

    char charAt(size_t pos) const noexcept {
        size_t idx = 0, cur = 0;
        if (!locatePiece(pos, idx, cur)) return ' ';
        const Piece& p = pieces[idx];
        size_t local = pos - cur;
        return p.isOriginal ? origPtr[p.start + local] : addBuf[p.start + local];
    }
};

struct Cursor {
    size_t head = 0;
    size_t anchor = 0;
    float desiredX = 0.0f;
    bool affinity = false;
    size_t start() const noexcept { return std::min(head, anchor); }
    size_t end() const noexcept { return std::max(head, anchor); }
    bool hasSelection() const noexcept { return head != anchor; }
    void clearSelection() noexcept { anchor = head; }
};

struct EditOp {
    enum Type { Insert, Erase } type;
    size_t pos;
    std::string text;
};

template <typename CursorT = Cursor>
struct EditBatchT {
    std::vector<EditOp> ops;
    std::vector<CursorT> beforeCursors;
    std::vector<CursorT> afterCursors;
};

using EditBatch = EditBatchT<Cursor>;

template <typename CursorT = Cursor>
struct UndoManagerT {
    using BatchType = EditBatchT<CursorT>;
    std::vector<BatchType> undoStack;
    std::vector<BatchType> redoStack;
    int savePoint = 0;
    void clear() { undoStack.clear(); redoStack.clear(); savePoint = 0; }
    void markSaved() noexcept { savePoint = (int)undoStack.size(); }
    bool isModified() const noexcept { return (int)undoStack.size() != savePoint; }
    void push(const BatchType& batch) {
        if (savePoint > (int)undoStack.size()) savePoint = -1;
        undoStack.push_back(batch);
        redoStack.clear();
    }
    bool canUndo() const noexcept { return !undoStack.empty(); }
    bool canRedo() const noexcept { return !redoStack.empty(); }
    BatchType popUndo() {
        BatchType e = undoStack.back();
        undoStack.pop_back();
        redoStack.push_back(e);
        return e;
    }
    BatchType popRedo() {
        BatchType e = redoStack.back();
        redoStack.pop_back();
        undoStack.push_back(e);
        return e;
    }
};

using UndoManager = UndoManagerT<Cursor>;

// ============================================================================
// 3. SIMD Line-Start Scanner (AVX2 / SSE2 / ARM NEON, CR / CRLF / LF aware)
// ============================================================================

inline void scanPieceTableLineStartsSIMD(const PieceTable& pt, std::vector<size_t>& lineStarts) {
    lineStarts.clear();
    lineStarts.reserve(std::max<size_t>(16, pt.length() / 32));
    lineStarts.push_back(0);
    size_t currentPos = 0;
    bool prevEndedWithCR = false;

    for (const auto& p : pt.pieces) {
        if (p.len == 0) continue;
        const char* buf = p.isOriginal ? pt.origPtr : pt.addBuf.data();
        const char* ptr = buf + p.start;
        const char* const pieceBegin = ptr;
        const char* const end = ptr + p.len;

        if (prevEndedWithCR) {
            if (*ptr == '\n') {
                if (!lineStarts.empty()) lineStarts.back() = currentPos + 1;
                ptr++;
            }
            prevEndedWithCR = false;
        }

#if defined(__AVX2__)
        const __m256i vLF = _mm256_set1_epi8('\n');
        const __m256i vCR = _mm256_set1_epi8('\r');
        while (ptr + 32 <= end) {
            __m256i chunk = _mm256_loadu_si256((const __m256i*)ptr);
            uint32_t maskLF = (uint32_t)_mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, vLF));
            uint32_t maskCR = (uint32_t)_mm256_movemask_epi8(_mm256_cmpeq_epi8(chunk, vCR));
            uint32_t mask = maskLF | maskCR;
            if (mask == 0) {
                ptr += 32;
                continue;
            }
            if (maskCR == 0) {
                while (maskLF != 0) {
#if defined(_MSC_VER)
                    unsigned long bit = 0;
                    _BitScanForward(&bit, maskLF);
#else
                    int bit = __builtin_ctz(maskLF);
#endif
                    lineStarts.push_back(currentPos + (size_t)(ptr - pieceBegin) + (size_t)bit + 1);
                    maskLF &= (maskLF - 1);
                }
                ptr += 32;
                continue;
            }
            break; // Fall through to scalar for CR/CRLF handling in this chunk
        }
#elif defined(_M_X64) || defined(_M_AMD64) || defined(__SSE2__)
        const __m128i vLF = _mm_set1_epi8('\n');
        const __m128i vCR = _mm_set1_epi8('\r');
        while (ptr + 16 <= end) {
            __m128i chunk = _mm_loadu_si128((const __m128i*)ptr);
            uint32_t maskLF = (uint32_t)_mm_movemask_epi8(_mm_cmpeq_epi8(chunk, vLF));
            uint32_t maskCR = (uint32_t)_mm_movemask_epi8(_mm_cmpeq_epi8(chunk, vCR));
            if ((maskLF | maskCR) == 0) {
                ptr += 16;
                continue;
            }
            if (maskCR == 0) {
                while (maskLF != 0) {
#if defined(_MSC_VER)
                    unsigned long bit = 0;
                    _BitScanForward(&bit, maskLF);
#else
                    int bit = __builtin_ctz(maskLF);
#endif
                    lineStarts.push_back(currentPos + (size_t)(ptr - pieceBegin) + (size_t)bit + 1);
                    maskLF &= (maskLF - 1);
                }
                ptr += 16;
                continue;
            }
            break;
        }
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
        const uint8x16_t vLF = vdupq_n_u8('\n');
        const uint8x16_t vCR = vdupq_n_u8('\r');
        while (ptr + 16 <= end) {
            uint8x16_t chunk = vld1q_u8((const uint8_t*)ptr);
            uint8x16_t eqLF = vceqq_u8(chunk, vLF);
            uint8x16_t eqCR = vceqq_u8(chunk, vCR);
            uint8x16_t eqAny = vorrq_u8(eqLF, eqCR);
#if defined(__aarch64__) || defined(_M_ARM64)
            if (vmaxvq_u8(eqAny) == 0) {
#else
            uint8x8_t folded = vorr_u8(vget_low_u8(eqAny), vget_high_u8(eqAny));
            if (vget_lane_u64(vreinterpret_u64_u8(folded), 0) == 0) {
#endif
                ptr += 16;
                continue;
            }
            break;
        }
#endif

        while (ptr < end) {
            char c = *ptr;
            if (c == '\n') {
                lineStarts.push_back(currentPos + (size_t)(ptr - pieceBegin) + 1);
                ptr++;
            } else if (c == '\r') {
                size_t offsetInPiece = (size_t)(ptr - pieceBegin);
                if (ptr + 1 < end) {
                    if (*(ptr + 1) == '\n') {
                        lineStarts.push_back(currentPos + offsetInPiece + 2);
                        ptr += 2;
                    } else {
                        lineStarts.push_back(currentPos + offsetInPiece + 1);
                        ptr++;
                    }
                } else {
                    lineStarts.push_back(currentPos + offsetInPiece + 1);
                    prevEndedWithCR = true;
                    ptr++;
                }
            } else {
                ptr++;
            }
        }
        currentPos += p.len;
    }
}

inline void scanPieceTableLineStartsSIMD(const PieceTable& pt, std::vector<size_t>& lineStarts, size_t& outMaxBytes, int& outMaxBytesLineIdx) {
    scanPieceTableLineStartsSIMD(pt, lineStarts);
    size_t maxBytes = 0;
    int maxIdx = -1;
    const size_t totalLen = pt.length();
    const size_t nLines = lineStarts.size();
    for (size_t i = 0; i < nLines; ++i) {
        size_t start = lineStarts[i];
        size_t end = (i + 1 < nLines) ? lineStarts[i + 1] : totalLen;
        size_t len = (end > start) ? (end - start) : 0;
        if (len > maxBytes) {
            maxBytes = len;
            maxIdx = (int)i;
        }
    }
    outMaxBytes = maxBytes;
    outMaxBytesLineIdx = maxIdx;
}

// ============================================================================
// 4. Unified Regex Preprocessor, ReDoS Guard & Fast Substring Search
// ============================================================================

inline bool isWordChar(char c) noexcept {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || (unsigned char)c >= 0x80;
}

inline bool isSafeUserRegex(const std::string& q) noexcept {
    if (q.empty() || q.size() > 1024) return false;
    int parenDepth = 0;
    bool groupHasQuantifier[64] = {};
    bool inBracket = false;
    for (size_t i = 0; i < q.size(); ++i) {
        char c = q[i];
        if (c == '\\') { i++; continue; }
        if (inBracket) {
            if (c == ']') inBracket = false;
            continue;
        }
        if (c == '[') { inBracket = true; continue; }
        if (c == '(') {
            parenDepth++;
            if (parenDepth >= 32) return false;
            groupHasQuantifier[parenDepth] = false;
        } else if (c == ')') {
            if (parenDepth <= 0) return false;
            bool hadQ = groupHasQuantifier[parenDepth];
            parenDepth--;
            if (parenDepth > 0 && hadQ) groupHasQuantifier[parenDepth] = true;
            if (i + 1 < q.size()) {
                char next = q[i + 1];
                if ((next == '+' || next == '*' || next == '{') && hadQ) {
                    return false;
                }
            }
        } else if (c == '+' || c == '*' || c == '{') {
            if (parenDepth > 0) groupHasQuantifier[parenDepth] = true;
        }
    }
    return parenDepth == 0 && !inBracket;
}

inline std::string preprocessRegexQuery(const std::string& query) {
    std::string processed;
    processed.reserve(query.size() * 4);
    bool inCharClass = false;
    size_t charClassStart = 0;
    for (size_t i = 0; i < query.size(); ++i) {
        char c = query[i];
        if (c == '\\' && i + 1 < query.size()) {
            char next = query[i + 1];
            if (!inCharClass && next == 'n') {
                bool isPrecededByCR = (i >= 2 && query[i - 2] == '\\' && query[i - 1] == 'r');
                if (!isPrecededByCR) {
                    processed += "(?:\\r\\n|[\\r\\n])";
                    i++;
                    continue;
                }
            }
            processed += c;
            processed += next;
            i++;
            continue;
        }
        if (inCharClass) {
            if (c == ']' && !(i == charClassStart + 1 || (i == charClassStart + 2 && query[charClassStart + 1] == '^'))) {
                inCharClass = false;
            }
            processed += c;
            continue;
        }
        if (c == '[') {
            inCharClass = true;
            charClassStart = i;
            processed += c;
            continue;
        }
        if (c == '^') {
            if (i == 0) processed += "((?:^|(?:\\r\\n|\\r(?!\\n)|[\\n])))";
            else processed += "(?:^|(?:\\r\\n|\\r(?!\\n)|[\\n]))";
            continue;
        }
        if (c == '$') {
            processed += "(?=(?:\\r\\n|[\\r\\n]|$))";
            continue;
        }
        if (c == '.') {
            processed += "[^\\r\\n]";
            continue;
        }
        processed += c;
    }
    return processed;
}

inline std::string unescapeReplaceString(const std::string& s, const std::string& newline) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            switch (s[i + 1]) {
                case 'n': out += newline; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case '\\': out += '\\'; break;
                default: out += s[i]; out += s[i + 1]; break;
            }
            i++;
        } else {
            out += s[i];
        }
    }
    return out;
}

inline std::string adjustCaretGroupReferences(const std::string& rawFmt, bool startsWithCaret) {
    if (!startsWithCaret) return rawFmt;
    std::string fmt;
    fmt.reserve(rawFmt.size() + 8);
    for (size_t i = 0; i < rawFmt.size(); ++i) {
        if (rawFmt[i] == '$') {
            fmt += '$';
            if (i + 1 < rawFmt.size()) {
                if (std::isdigit((unsigned char)rawFmt[i + 1])) {
                    i++;
                    std::string numStr;
                    while (i < rawFmt.size() && std::isdigit((unsigned char)rawFmt[i])) {
                        numStr += rawFmt[i];
                        i++;
                    }
                    i--;
                    int grp = std::stoi(numStr);
                    if (grp > 0) grp++;
                    fmt += std::to_string(grp);
                } else {
                    fmt += rawFmt[i + 1];
                    i++;
                }
            }
        } else {
            fmt += rawFmt[i];
        }
    }
    return fmt;
}

inline size_t findSubstringFast(
    std::string_view fullText,
    size_t startPos,
    std::string_view query,
    bool forward,
    bool matchCase,
    bool wholeWord) noexcept
{
    size_t len = fullText.size();
    size_t qLen = query.size();
    if (qLen == 0 || qLen > len) return std::string::npos;

    const char* data = fullText.data();
    const char* qData = query.data();
    size_t maxStart = len - qLen;
    auto toLower = [](char c) noexcept -> char { return (c >= 'A' && c <= 'Z') ? (char)(c + ('a' - 'A')) : c; };
    char fLow = matchCase ? qData[0] : toLower(qData[0]);
    char fUp  = matchCase ? qData[0] : (char)((fLow >= 'a' && fLow <= 'z') ? (fLow - ('a' - 'A')) : fLow);
    char lLow = matchCase ? qData[qLen - 1] : toLower(qData[qLen - 1]);
    char lUp  = matchCase ? qData[qLen - 1] : (char)((lLow >= 'a' && lLow <= 'z') ? (lLow - ('a' - 'A')) : lLow);

    auto checkAt = [&](size_t p) noexcept -> bool {
        char cFirst = data[p];
        if (cFirst != fLow && cFirst != fUp) return false;
        char cLast = data[p + qLen - 1];
        if (cLast != lLow && cLast != lUp) return false;
        if (qLen > 2) {
            if (matchCase) {
                if (std::memcmp(data + p + 1, qData + 1, qLen - 2) != 0) return false;
            } else {
                for (size_t i = 1; i + 1 < qLen; ++i) {
                    if (toLower(data[p + i]) != toLower(qData[i])) return false;
                }
            }
        }
        if (wholeWord) {
            if (p > 0 && isWordChar(data[p - 1])) return false;
            if (p + qLen < len && isWordChar(data[p + qLen])) return false;
        }
        return true;
    };

    auto scanForwardRange = [&](size_t from, size_t to) noexcept -> size_t {
        if (from > to) return std::string::npos;
        if (matchCase && !wholeWord && fLow == fUp) {
            size_t p = from;
            while (p <= to) {
                const char* hit = (const char*)std::memchr(data + p, (unsigned char)fLow, to - p + 1);
                if (!hit) break;
                size_t idx = (size_t)(hit - data);
                if (checkAt(idx)) return idx;
                p = idx + 1;
            }
            return std::string::npos;
        }
        for (size_t p = from; p <= to; ++p) {
            if (checkAt(p)) return p;
        }
        return std::string::npos;
    };

    if (forward) {
        size_t cur = (startPos <= maxStart) ? startPos : 0;
        size_t pos = scanForwardRange(cur, maxStart);
        if (pos != std::string::npos) return pos;
        if (cur > 0) return scanForwardRange(0, cur - 1);
    } else {
        size_t firstStart = (startPos == 0) ? maxStart : std::min(startPos - 1, maxStart);
        for (size_t p = firstStart + 1; p-- > 0; ) {
            if (checkAt(p)) return p;
        }
        for (size_t p = maxStart + 1; p-- > firstStart + 1; ) {
            if (checkAt(p)) return p;
        }
    }
    return std::string::npos;
}

inline size_t findSubstringFast(
    const PieceTable& pt,
    size_t startPos,
    std::string_view query,
    bool forward,
    bool matchCase,
    bool wholeWord,
    size_t* outMatchLen = nullptr)
{
    size_t docLen = pt.length();
    if (query.empty() || query.size() > docLen) return std::string::npos;
    size_t pos = std::string::npos;
    if (pt.pieces.size() == 1) {
        const auto& p = pt.pieces[0];
        const char* buf = p.isOriginal ? pt.origPtr : pt.addBuf.data();
        pos = findSubstringFast(std::string_view(buf + p.start, p.len), startPos, query, forward, matchCase, wholeWord);
    } else {
        std::string fullText = pt.getRange(0, docLen);
        pos = findSubstringFast(std::string_view(fullText), startPos, query, forward, matchCase, wholeWord);
    }
    if (pos != std::string::npos && outMatchLen) *outMatchLen = query.size();
    return pos;
}

} // namespace MiuCore
