#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cctype>
#include <climits>

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
        unsigned char c = p[0];
        if (c < 0x80) {
            cp = c;
            p += 1;
        } else if (c >= 0xC2 && c <= 0xDF && p + 1 < end && (p[1] & 0xC0) == 0x80) {
            cp = ((uint32_t)(c & 0x1F) << 6) | (uint32_t)(p[1] & 0x3F);
            p += 2;
        } else if (c >= 0xE0 && c <= 0xEF && p + 2 < end &&
                   (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 &&
                   !(c == 0xE0 && p[1] < 0xA0) && !(c == 0xED && p[1] >= 0xA0)) {
            cp = ((uint32_t)(c & 0x0F) << 12) | ((uint32_t)(p[1] & 0x3F) << 6) | (uint32_t)(p[2] & 0x3F);
            p += 3;
        } else if (c >= 0xF0 && c <= 0xF4 && p + 3 < end &&
                   (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 && (p[3] & 0xC0) == 0x80 &&
                   !(c == 0xF0 && p[1] < 0x90) && !(c == 0xF4 && p[1] >= 0x90)) {
            cp = ((uint32_t)(c & 0x07) << 18) | ((uint32_t)(p[1] & 0x3F) << 12) | ((uint32_t)(p[2] & 0x3F) << 6) | (uint32_t)(p[3] & 0x3F);
            p += 4;
        } else {
            cp = 0xFFFDu;
            p += 1;
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
        if (count > totalLength - pos) count = totalLength - pos;
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
        if (count > totalLength - pos) count = totalLength - pos;
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

inline bool isWordCodePoint(uint32_t cp) noexcept {
    if (cp < 0x80u) {
        return (cp >= '0' && cp <= '9') || (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z') || cp == '_';
    }
    if (cp > 0x10FFFFu || (cp >= 0xD800u && cp <= 0xDFFFu)) return false;
    // Latin-1 C1 controls, NBSP, and symbols/punctuation (except feminine/micro/masculine ordinals)
    if (cp >= 0x0080u && cp <= 0x00BFu) {
        return cp == 0x00AAu || cp == 0x00B5u || cp == 0x00BAu;
    }
    if (cp == 0x00D7u || cp == 0x00F7u) return false;
    // General Punctuation & Unicode Spaces (U+2000..U+206F)
    if (cp >= 0x2000u && cp <= 0x206Fu) return false;
    // Arrows, Mathematical Operators, Box Drawing, Geometric Shapes, Misc Symbols & Dingbats (U+2190..U+2BFF)
    if (cp >= 0x2190u && cp <= 0x2BFFu) return false;
    // Supplemental Punctuation (U+2E00..U+2E7F)
    if (cp >= 0x2E00u && cp <= 0x2E7Fu) return false;
    // CJK Symbols & Punctuation (U+3000..U+303F), except iteration/number letters U+3005..U+3007 and U+303B..U+303C
    if (cp >= 0x3000u && cp <= 0x303Fu) {
        return (cp >= 0x3005u && cp <= 0x3007u) || cp == 0x303Bu || cp == 0x303Cu;
    }
    // Katakana double hyphen U+30A0 and middle dot U+30FB
    if (cp == 0x30A0u || cp == 0x30FBu) return false;
    // Vertical Forms & CJK Compatibility Forms / Small Form Variants (U+FE10..U+FE1F, U+FE30..U+FE6F) & BOM
    if ((cp >= 0xFE10u && cp <= 0xFE1Fu) || (cp >= 0xFE30u && cp <= 0xFE6Fu) || cp == 0xFEFFu) return false;
    // Fullwidth ASCII punctuation & halfwidth CJK punctuation
    if ((cp >= 0xFF01u && cp <= 0xFF0Fu) ||
        (cp >= 0xFF1Au && cp <= 0xFF20u) ||
        (cp >= 0xFF3Bu && cp <= 0xFF40u) ||
        (cp >= 0xFF5Bu && cp <= 0xFF65u) ||
        (cp >= 0xFFE0u && cp <= 0xFFFFu)) {
        return false;
    }
    return true;
}

inline bool isUtf8CharBoundary(const std::string& s, size_t pos) noexcept {
    if (pos >= s.size()) return true;
    return (((unsigned char)s[pos]) & 0xC0) != 0x80;
}

inline size_t utf8CharLenAt(std::string_view s, size_t pos) noexcept {
    if (pos >= s.size()) return 1;
    unsigned char c = (unsigned char)s[pos];
    if (c < 0x80u) return 1;
    if (c >= 0xC2u && c <= 0xDFu && pos + 1 < s.size() && ((unsigned char)s[pos + 1] & 0xC0u) == 0x80u) {
        return 2;
    }
    if (c >= 0xE0u && c <= 0xEFu && pos + 2 < s.size()) {
        unsigned char b1 = (unsigned char)s[pos + 1];
        unsigned char b2 = (unsigned char)s[pos + 2];
        if ((b1 & 0xC0u) == 0x80u && (b2 & 0xC0u) == 0x80u &&
            !(c == 0xE0u && b1 < 0xA0u) && !(c == 0xEDu && b1 >= 0xA0u)) {
            return 3;
        }
    }
    if (c >= 0xF0u && c <= 0xF4u && pos + 3 < s.size()) {
        unsigned char b1 = (unsigned char)s[pos + 1];
        unsigned char b2 = (unsigned char)s[pos + 2];
        unsigned char b3 = (unsigned char)s[pos + 3];
        if ((b1 & 0xC0u) == 0x80u && (b2 & 0xC0u) == 0x80u && (b3 & 0xC0u) == 0x80u &&
            !(c == 0xF0u && b1 < 0x90u) && !(c == 0xF4u && b1 >= 0x90u)) {
            return 4;
        }
    }
    return 1;
}

inline size_t utf8CharLenAt(const PieceTable& pt, size_t pos) noexcept {
    size_t len = pt.length();
    if (pos >= len) return 1;
    unsigned char c = (unsigned char)pt.charAt(pos);
    if (c < 0x80u) return 1;
    if (c >= 0xC2u && c <= 0xDFu && pos + 1 < len && ((unsigned char)pt.charAt(pos + 1) & 0xC0u) == 0x80u) {
        return 2;
    }
    if (c >= 0xE0u && c <= 0xEFu && pos + 2 < len) {
        unsigned char b1 = (unsigned char)pt.charAt(pos + 1);
        unsigned char b2 = (unsigned char)pt.charAt(pos + 2);
        if ((b1 & 0xC0u) == 0x80u && (b2 & 0xC0u) == 0x80u &&
            !(c == 0xE0u && b1 < 0xA0u) && !(c == 0xEDu && b1 >= 0xA0u)) {
            return 3;
        }
    }
    if (c >= 0xF0u && c <= 0xF4u && pos + 3 < len) {
        unsigned char b1 = (unsigned char)pt.charAt(pos + 1);
        unsigned char b2 = (unsigned char)pt.charAt(pos + 2);
        unsigned char b3 = (unsigned char)pt.charAt(pos + 3);
        if ((b1 & 0xC0u) == 0x80u && (b2 & 0xC0u) == 0x80u && (b3 & 0xC0u) == 0x80u &&
            !(c == 0xF0u && b1 < 0x90u) && !(c == 0xF4u && b1 >= 0x90u)) {
            return 4;
        }
    }
    return 1;
}

inline uint32_t decodeUtf8CodePointAt(std::string_view s, size_t pos, size_t& outLen) noexcept {
    if (pos >= s.size()) { outLen = 1; return 0; }
    unsigned char c = (unsigned char)s[pos];
    if (c < 0x80u) { outLen = 1; return (uint32_t)c; }
    if (c >= 0xC2u && c <= 0xDFu && pos + 1 < s.size()) {
        unsigned char b1 = (unsigned char)s[pos + 1];
        if ((b1 & 0xC0u) == 0x80u) {
            outLen = 2;
            return ((uint32_t)(c & 0x1Fu) << 6) | (uint32_t)(b1 & 0x3Fu);
        }
    } else if (c >= 0xE0u && c <= 0xEFu && pos + 2 < s.size()) {
        unsigned char b1 = (unsigned char)s[pos + 1];
        unsigned char b2 = (unsigned char)s[pos + 2];
        if ((b1 & 0xC0u) == 0x80u && (b2 & 0xC0u) == 0x80u &&
            !(c == 0xE0u && b1 < 0xA0u) && !(c == 0xEDu && b1 >= 0xA0u)) {
            outLen = 3;
            return ((uint32_t)(c & 0x0Fu) << 12) |
                   ((uint32_t)(b1 & 0x3Fu) << 6) |
                   (uint32_t)(b2 & 0x3Fu);
        }
    } else if (c >= 0xF0u && c <= 0xF4u && pos + 3 < s.size()) {
        unsigned char b1 = (unsigned char)s[pos + 1];
        unsigned char b2 = (unsigned char)s[pos + 2];
        unsigned char b3 = (unsigned char)s[pos + 3];
        if ((b1 & 0xC0u) == 0x80u && (b2 & 0xC0u) == 0x80u && (b3 & 0xC0u) == 0x80u &&
            !(c == 0xF0u && b1 < 0x90u) && !(c == 0xF4u && b1 >= 0x90u)) {
            outLen = 4;
            return ((uint32_t)(c & 0x07u) << 18) |
                   ((uint32_t)(b1 & 0x3Fu) << 12) |
                   ((uint32_t)(b2 & 0x3Fu) << 6) |
                   (uint32_t)(b3 & 0x3Fu);
        }
    }
    outLen = 1;
    return 0xFFFDu;
}

inline uint32_t decodeUtf8CodePointBefore(std::string_view s, size_t pos, size_t& outLen) noexcept {
    if (pos == 0 || pos > s.size()) { outLen = 0; return 0; }
    unsigned char prev = (unsigned char)s[pos - 1];
    if (prev < 0x80u) { outLen = 1; return (uint32_t)prev; }
    size_t start = pos - 1;
    while (start > 0 && (pos - start) < 4 && (((unsigned char)s[start]) & 0xC0u) == 0x80u) {
        --start;
    }
    size_t decLen = 1;
    uint32_t cp = decodeUtf8CodePointAt(s, start, decLen);
    if (start + decLen == pos) {
        outLen = decLen;
        return cp;
    }
    outLen = 1;
    return 0xFFFDu;
}

inline bool isWordBoundaryBefore(std::string_view text, size_t pos) noexcept {
    if (pos == 0 || pos > text.size()) return true;
    unsigned char prev = (unsigned char)text[pos - 1];
    if (prev < 0x80u) return !isWordCodePoint((uint32_t)prev);
    if (pos < text.size() && (((unsigned char)text[pos]) & 0xC0u) == 0x80u) {
        size_t start = pos - 1;
        while (start > 0 && (pos - start) < 4 && (((unsigned char)text[start]) & 0xC0u) == 0x80u) {
            --start;
        }
        size_t decLen = 1;
        decodeUtf8CodePointAt(text, start, decLen);
        if (start + decLen > pos) return false;
    }
    size_t prevLen = 1;
    uint32_t cp = decodeUtf8CodePointBefore(text, pos, prevLen);
    return !isWordCodePoint(cp);
}

inline bool isWordBoundaryAfter(std::string_view text, size_t endPos) noexcept {
    if (endPos >= text.size()) return true;
    unsigned char next = (unsigned char)text[endPos];
    if (next < 0x80u) return !isWordCodePoint((uint32_t)next);
    if ((next & 0xC0u) == 0x80u) return false;
    size_t nextLen = 1;
    uint32_t cp = decodeUtf8CodePointAt(text, endPos, nextLen);
    return !isWordCodePoint(cp);
}

inline bool isWordBoundaryBefore(const PieceTable& pt, size_t pos) noexcept {
    size_t docLen = pt.length();
    if (pos == 0 || pos > docLen) return true;
    unsigned char prev = (unsigned char)pt.charAt(pos - 1);
    if (prev < 0x80u) return !isWordCodePoint((uint32_t)prev);
    size_t winStart = (pos >= 4) ? (pos - 4) : 0;
    size_t winEnd = std::min<size_t>(docLen, pos + 4);
    std::string win = pt.getRange(winStart, winEnd - winStart);
    return isWordBoundaryBefore(std::string_view(win), pos - winStart);
}

inline bool isWordBoundaryAfter(const PieceTable& pt, size_t endPos) noexcept {
    size_t docLen = pt.length();
    if (endPos >= docLen) return true;
    unsigned char next = (unsigned char)pt.charAt(endPos);
    if (next < 0x80u) return !isWordCodePoint((uint32_t)next);
    size_t winEnd = std::min<size_t>(docLen, endPos + 4);
    std::string win = pt.getRange(endPos, winEnd - endPos);
    return isWordBoundaryAfter(std::string_view(win), 0);
}

struct RegexBracketAtom {
    bool valid = true;
    bool isClassSet = false;
    uint32_t cp = 0;
    size_t nextPos = 0;
};

inline int regexHexDigitVal(char ch) noexcept {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return 10 + (ch - 'a');
    if (ch >= 'A' && ch <= 'F') return 10 + (ch - 'A');
    return -1;
}

inline size_t findRegexBracketClose(std::string_view q, size_t openIdx, bool& outMalformedPosix) noexcept {
    outMalformedPosix = false;
    for (size_t j = openIdx + 1; j < q.size(); ++j) {
        if (q[j] == '\\') {
            if (j + 1 >= q.size()) return std::string::npos;
            size_t escLen = ((unsigned char)q[j + 1] >= 0x80u) ? utf8CharLenAt(q, j + 1) : 1;
            j += escLen;
            continue;
        }
        if (q[j] == '[' && j + 1 < q.size() &&
            (q[j + 1] == ':' || q[j + 1] == '.' || q[j + 1] == '=')) {
            char pDelim = q[j + 1];
            size_t m = j + 2;
            while (m + 1 < q.size() && !(q[m] == pDelim && q[m + 1] == ']')) {
                if (q[m] == '[' || q[m] == ']' || q[m] == '\\') {
                    outMalformedPosix = true;
                    return std::string::npos;
                }
                m++;
            }
            if (m <= j + 2 || m + 1 >= q.size()) {
                outMalformedPosix = true;
                return std::string::npos;
            }
            j = m + 1;
            continue;
        }
        if (q[j] == ']' && !(j == openIdx + 1 || (j == openIdx + 2 && q[openIdx + 1] == '^'))) {
            return j;
        }
    }
    return std::string::npos;
}

inline RegexBracketAtom parseRegexBracketAtom(std::string_view q, size_t pos, size_t closeIdx) noexcept {
    if (pos >= closeIdx) return { false, false, 0, closeIdx };
    if (q[pos] == '[' && pos + 1 < closeIdx &&
        (q[pos + 1] == ':' || q[pos + 1] == '.' || q[pos + 1] == '=')) {
        char delim = q[pos + 1];
        size_t m = pos + 2;
        while (m + 1 < closeIdx && !(q[m] == delim && q[m + 1] == ']')) {
            if (q[m] == '[' || q[m] == ']' || q[m] == '\\') {
                return { false, false, 0, closeIdx };
            }
            m++;
        }
        if (m <= pos + 2 || m + 1 >= closeIdx) {
            return { false, false, 0, closeIdx };
        }
        return { true, true, 0, m + 2 };
    }
    if (q[pos] == '\\') {
        if (pos + 1 >= closeIdx) return { false, false, 0, closeIdx };
        char esc = q[pos + 1];
        if (esc == 'd' || esc == 'D' || esc == 'w' || esc == 'W' || esc == 's' || esc == 'S') {
            return { true, true, 0, pos + 2 };
        }
        if (esc == 't') return { true, false, 0x09u, pos + 2 };
        if (esc == 'n') return { true, false, 0x0Au, pos + 2 };
        if (esc == 'v') return { true, false, 0x0Bu, pos + 2 };
        if (esc == 'f') return { true, false, 0x0Cu, pos + 2 };
        if (esc == 'r') return { true, false, 0x0Du, pos + 2 };
        if (esc == 'b') return { true, false, 0x08u, pos + 2 };
        if (esc == '0') return { true, false, 0x00u, pos + 2 };
        if (esc == 'c') {
            if (pos + 2 < closeIdx && std::isalpha((unsigned char)q[pos + 2])) {
                return { true, false, (uint32_t)((unsigned char)q[pos + 2] & 0x1Fu), pos + 3 };
            }
            return { false, false, 0, closeIdx };
        }
        if (esc == 'x') {
            if (pos + 3 < closeIdx) {
                int h1 = regexHexDigitVal(q[pos + 2]);
                int h2 = regexHexDigitVal(q[pos + 3]);
                if (h1 >= 0 && h2 >= 0) {
                    return { true, false, (uint32_t)((h1 << 4) | h2), pos + 4 };
                }
            }
            return { false, false, 0, closeIdx };
        }
        if (esc == 'u') {
            if (pos + 5 < closeIdx) {
                int h1 = regexHexDigitVal(q[pos + 2]);
                int h2 = regexHexDigitVal(q[pos + 3]);
                int h3 = regexHexDigitVal(q[pos + 4]);
                int h4 = regexHexDigitVal(q[pos + 5]);
                if (h1 >= 0 && h2 >= 0 && h3 >= 0 && h4 >= 0) {
                    uint32_t cp = (uint32_t)((h1 << 12) | (h2 << 8) | (h3 << 4) | h4);
                    if (cp >= 0xD800u && cp <= 0xDBFFu) {
                        if (pos + 11 < closeIdx && q[pos + 6] == '\\' && q[pos + 7] == 'u') {
                            int l1 = regexHexDigitVal(q[pos + 8]);
                            int l2 = regexHexDigitVal(q[pos + 9]);
                            int l3 = regexHexDigitVal(q[pos + 10]);
                            int l4 = regexHexDigitVal(q[pos + 11]);
                            if (l1 >= 0 && l2 >= 0 && l3 >= 0 && l4 >= 0) {
                                uint32_t lo = (uint32_t)((l1 << 12) | (l2 << 8) | (l3 << 4) | l4);
                                if (lo >= 0xDC00u && lo <= 0xDFFFu) {
                                    uint32_t fullCp = 0x10000u + (((cp - 0xD800u) << 10) | (lo - 0xDC00u));
                                    return { true, false, fullCp, pos + 12 };
                                }
                            }
                        }
                        return { false, false, 0, closeIdx };
                    }
                    if (cp >= 0xDC00u && cp <= 0xDFFFu) {
                        return { false, false, 0, closeIdx };
                    }
                    return { true, false, cp, pos + 6 };
                }
            }
            return { false, false, 0, closeIdx };
        }
        size_t uLen = 1;
        uint32_t cp = decodeUtf8CodePointAt(q, pos + 1, uLen);
        if (pos + 1 + uLen > closeIdx) return { false, false, 0, closeIdx };
        return { true, false, cp, pos + 1 + uLen };
    }
    size_t uLen = 1;
    uint32_t cp = decodeUtf8CodePointAt(q, pos, uLen);
    if (pos + uLen > closeIdx) return { false, false, 0, closeIdx };
    return { true, false, cp, pos + uLen };
}

inline bool validateRegexBracketContent(std::string_view q, size_t contentStart, size_t closeIdx) noexcept {
    if (contentStart >= closeIdx) return false;
    size_t p = contentStart;
    while (p < closeIdx) {
        RegexBracketAtom atom1 = parseRegexBracketAtom(q, p, closeIdx);
        if (!atom1.valid) return false;
        if (atom1.nextPos < closeIdx && q[atom1.nextPos] == '-' && atom1.nextPos + 1 < closeIdx) {
            RegexBracketAtom atom2 = parseRegexBracketAtom(q, atom1.nextPos + 1, closeIdx);
            if (!atom2.valid || atom1.isClassSet || atom2.isClassSet || atom1.cp > atom2.cp) {
                return false;
            }
            p = atom2.nextPos;
            continue;
        }
        p = atom1.nextPos;
    }
    return true;
}

inline bool isSafeUserRegex(const std::string& q, size_t maxLen = 1024) noexcept {
    if (q.empty() || q.size() > maxLen) return false;
    int parenDepth = 0;
    bool groupHasQuantifier[32] = {};
    bool groupCanMatchEmpty[32] = {};
    bool branchHasMandatory[32] = {};
    bool groupIsLookaround[32] = {};
    bool groupHasDupAlt[32] = {};
    bool groupHasAmbiguousAlt[32] = {};
    bool groupHasBackref[32] = {};
    int groupAltCount[32] = {};
    int groupUnboundedCount[32] = {};
    int groupFixedMult[32] = {};
    bool groupSeenLead[32][128] = {};
    bool branchSawFirstToken[32] = {};
    int branchLeadAscii[32] = {};
    bool canQuantifyAtom = false;
    bool prevAtomIsBackref = false;
    int prevAtomFixedMult = 1;
    int totalVarQuantifiers = 0;
    int totalUnboundedQuantifiers = 0;

    auto asciiLower = [](char ch) noexcept -> unsigned char {
        unsigned char u = (unsigned char)ch;
        return (u >= 'A' && u <= 'Z') ? (unsigned char)(u + 32) : u;
    };

    auto parseBraceQuantifier = [&](size_t pos, int& minRep, int& maxRep, size_t& closeBracePos, bool& invalidRange) noexcept -> bool {
        minRep = 0;
        maxRep = 0;
        closeBracePos = pos;
        invalidRange = false;
        if (pos >= q.size() || q[pos] != '{') return false;
        size_t k = pos + 1;
        if (k >= q.size() || !std::isdigit((unsigned char)q[k])) return false;
        while (k < q.size() && std::isdigit((unsigned char)q[k])) {
            minRep = std::min(100000, minRep * 10 + (q[k] - '0'));
            k++;
        }
        maxRep = minRep;
        if (k < q.size() && q[k] == ',') {
            k++;
            if (k < q.size() && std::isdigit((unsigned char)q[k])) {
                maxRep = 0;
                while (k < q.size() && std::isdigit((unsigned char)q[k])) {
                    maxRep = std::min(100000, maxRep * 10 + (q[k] - '0'));
                    k++;
                }
            } else {
                maxRep = -1; // unbounded {m,}
            }
        }
        if (k < q.size() && q[k] == '}') {
            closeBracePos = k;
            if (minRep > 256 || (maxRep != -1 && (maxRep > 256 || maxRep < minRep))) {
                invalidRange = true;
            }
            return true;
        }
        return false;
    };

    auto isZeroMinQuantifierAt = [&](size_t pos, size_t& skipEnd) noexcept -> bool {
        skipEnd = pos;
        if (pos >= q.size()) return false;
        char qc = q[pos];
        if (qc == '?' || qc == '*') {
            skipEnd = pos + 1;
            if (skipEnd < q.size() && q[skipEnd] == '?') skipEnd++;
            return true;
        }
        int minRep = 0, maxRep = 0;
        size_t closePos = pos;
        bool invalidRange = false;
        if (parseBraceQuantifier(pos, minRep, maxRep, closePos, invalidRange) && !invalidRange) {
            skipEnd = closePos + 1;
            if (skipEnd < q.size() && q[skipEnd] == '?') skipEnd++;
            return minRep == 0;
        }
        return false;
    };

    auto resetBranchState = [&](int d) noexcept {
        branchHasMandatory[d] = false;
        branchSawFirstToken[d] = false;
        branchLeadAscii[d] = -1;
    };

    auto finishBranch = [&](int d, bool hasAltInGroup) noexcept {
        if (d <= 0 || d >= 32) return;
        if (hasAltInGroup) {
            int lead = branchLeadAscii[d];
            if (lead < 0 || lead >= 128) {
                groupHasAmbiguousAlt[d] = true;
            } else if (groupSeenLead[d][lead]) {
                groupHasDupAlt[d] = true;
                groupHasAmbiguousAlt[d] = true;
            } else {
                groupSeenLead[d][lead] = true;
            }
        }
    };

    groupUnboundedCount[0] = 0;
    groupFixedMult[0] = 1;

    for (size_t i = 0; i < q.size(); ++i) {
        char c = q[i];
        if (c == '\\') {
            if (i + 1 >= q.size()) return false;
            char esc = q[i + 1];
            i++;
            bool isAssertion = (esc == 'b' || esc == 'B');
            bool isBackref = (esc >= '1' && esc <= '9');
            bool isSingleAsciiLit = false;
            unsigned char litCh = 0;
            if (esc == 't') { isSingleAsciiLit = true; litCh = '\t'; }
            else if (esc == 'r') { isSingleAsciiLit = true; litCh = '\r'; }
            else if ((unsigned char)esc < 0x80 && !std::isalnum((unsigned char)esc)) {
                isSingleAsciiLit = true;
                litCh = asciiLower(esc);
            }
            if (isBackref) {
                if (parenDepth > 0) {
                    groupHasQuantifier[parenDepth] = true;
                    groupHasBackref[parenDepth] = true;
                }
            }
            if (parenDepth > 0) {
                size_t se = i + 1;
                bool zMin = isZeroMinQuantifierAt(i + 1, se);
                if (!branchSawFirstToken[parenDepth]) {
                    branchSawFirstToken[parenDepth] = true;
                    branchLeadAscii[parenDepth] = (isSingleAsciiLit && !zMin) ? (int)litCh : -1;
                }
                if (!isAssertion && !isBackref && !zMin) {
                    branchHasMandatory[parenDepth] = true;
                }
            }
            if ((unsigned char)esc >= 0x80) {
                size_t uLen = utf8CharLenAt(q, i);
                if (uLen > 1) i += uLen - 1;
            }
            canQuantifyAtom = !isAssertion;
            prevAtomIsBackref = isBackref;
            prevAtomFixedMult = 1;
            continue;
        }
        if (c == '[') {
            bool malformedPosix = false;
            size_t closeIdx = findRegexBracketClose(q, i, malformedPosix);
            if (malformedPosix || closeIdx == std::string::npos) return false;
            bool negated = (i + 1 < closeIdx && q[i + 1] == '^');
            size_t contentStart = negated ? (i + 2) : (i + 1);
            if (!validateRegexBracketContent(q, contentStart, closeIdx)) return false;
            canQuantifyAtom = true;
            prevAtomIsBackref = false;
            prevAtomFixedMult = 1;
            if (parenDepth > 0) {
                if (!branchSawFirstToken[parenDepth]) {
                    branchSawFirstToken[parenDepth] = true;
                    branchLeadAscii[parenDepth] = -1;
                }
                size_t se = closeIdx + 1;
                if (!isZeroMinQuantifierAt(closeIdx + 1, se)) branchHasMandatory[parenDepth] = true;
            }
            i = closeIdx;
            continue;
        }
        if (c == '(') {
            if (parenDepth > 0 && !branchSawFirstToken[parenDepth]) {
                branchSawFirstToken[parenDepth] = true;
                branchLeadAscii[parenDepth] = -1;
            }
            parenDepth++;
            if (parenDepth >= 32) return false;
            groupHasQuantifier[parenDepth] = false;
            groupCanMatchEmpty[parenDepth] = false;
            groupIsLookaround[parenDepth] = false;
            groupHasDupAlt[parenDepth] = false;
            groupHasAmbiguousAlt[parenDepth] = false;
            groupHasBackref[parenDepth] = false;
            groupAltCount[parenDepth] = 0;
            groupUnboundedCount[parenDepth] = 0;
            groupFixedMult[parenDepth] = 1;
            std::memset(groupSeenLead[parenDepth], 0, sizeof(groupSeenLead[parenDepth]));
            resetBranchState(parenDepth);
            if (i + 1 < q.size() && q[i + 1] == '?') {
                if (i + 2 >= q.size() || (q[i + 2] != ':' && q[i + 2] != '=' && q[i + 2] != '!')) {
                    return false;
                }
                if (q[i + 2] == '=' || q[i + 2] == '!') groupIsLookaround[parenDepth] = true;
                i += 2;
            }
            canQuantifyAtom = false;
            prevAtomIsBackref = false;
            prevAtomFixedMult = 1;
        } else if (c == '|') {
            if (parenDepth > 0) {
                groupAltCount[parenDepth]++;
                finishBranch(parenDepth, true);
                if (!branchHasMandatory[parenDepth]) groupCanMatchEmpty[parenDepth] = true;
                resetBranchState(parenDepth);
            }
            canQuantifyAtom = false;
            prevAtomIsBackref = false;
            prevAtomFixedMult = 1;
        } else if (c == ')') {
            if (parenDepth <= 0) return false;
            int altCnt = groupAltCount[parenDepth];
            finishBranch(parenDepth, altCnt > 0);
            bool hadQ = groupHasQuantifier[parenDepth];
            bool canEmpty = groupCanMatchEmpty[parenDepth] || !branchHasMandatory[parenDepth] || groupIsLookaround[parenDepth];
            bool dupAlt = groupHasDupAlt[parenDepth];
            bool ambigAlt = groupHasAmbiguousAlt[parenDepth];
            bool hasBackref = groupHasBackref[parenDepth];
            bool isLook = groupIsLookaround[parenDepth];
            int innerMult = groupFixedMult[parenDepth];
            parenDepth--;

            if (parenDepth > 0) {
                if (hadQ || hasBackref || altCnt > 0 || (!isLook && canEmpty)) {
                    groupHasQuantifier[parenDepth] = true;
                }
                if (hasBackref) groupHasBackref[parenDepth] = true;
                if (dupAlt) groupHasDupAlt[parenDepth] = true;
                if (ambigAlt) groupHasAmbiguousAlt[parenDepth] = true;
                if (innerMult > groupFixedMult[parenDepth]) groupFixedMult[parenDepth] = innerMult;
            }

            if (i + 1 < q.size()) {
                char next = q[i + 1];
                bool repeatedQuant = (next == '+' || next == '*');
                int bMin = 0, bMax = 0;
                size_t bClose = i + 1;
                bool bInvalid = false;
                if (next == '{' && parseBraceQuantifier(i + 1, bMin, bMax, bClose, bInvalid)) {
                    if (bInvalid) return false;
                    if (bMax == -1 || bMax > 1) repeatedQuant = true;
                }
                if (repeatedQuant && (hadQ || canEmpty || dupAlt || ambigAlt || hasBackref || isLook)) {
                    return false;
                }
            }
            if (parenDepth > 0 && !isLook && !canEmpty) {
                size_t se = i + 1;
                if (!isZeroMinQuantifierAt(i + 1, se)) branchHasMandatory[parenDepth] = true;
            }
            canQuantifyAtom = !isLook;
            prevAtomIsBackref = hasBackref;
            prevAtomFixedMult = innerMult;
        } else if (c == '+' || c == '*' || c == '?' || c == '{') {
            int bMin = 0, bMax = 0;
            size_t bClose = i;
            bool bInvalid = false;
            bool isBraceQ = false;
            if (c == '{') {
                isBraceQ = parseBraceQuantifier(i, bMin, bMax, bClose, bInvalid);
                if (isBraceQ && bInvalid) return false;
                if (!isBraceQ) {
                    // Literal '{' in ECMAScript Annex B
                    if (parenDepth > 0) {
                        size_t se = i + 1;
                        bool zMin = isZeroMinQuantifierAt(i + 1, se);
                        if (!branchSawFirstToken[parenDepth]) {
                            branchSawFirstToken[parenDepth] = true;
                            branchLeadAscii[parenDepth] = !zMin ? (int)'{' : -1;
                        }
                        if (!zMin) branchHasMandatory[parenDepth] = true;
                    }
                    canQuantifyAtom = true;
                    prevAtomIsBackref = false;
                    prevAtomFixedMult = 1;
                    continue;
                }
            }
            if (!canQuantifyAtom) return false;

            bool isUnbounded = (c == '+' || c == '*' || (isBraceQ && bMax == -1));
            bool isRepeated = isUnbounded || (isBraceQ && bMax > 1);
            bool isVarQuant = (c == '+' || c == '*' || c == '?' || (isBraceQ && (bMax == -1 || bMin != bMax)));

            if (prevAtomIsBackref && isRepeated) return false;

            if (isBraceQ) {
                int effMax = (bMax == -1) ? bMin : bMax;
                if (effMax > 1) {
                    int prod = prevAtomFixedMult * effMax;
                    if (prod > 256) return false;
                    if (prod > groupFixedMult[parenDepth]) groupFixedMult[parenDepth] = prod;
                }
                i = bClose;
            }
            // Consume optional lazy quantifier modifier '?' (e.g. .*?, .+?, a??, {m,n}?)
            if (i + 1 < q.size() && q[i + 1] == '?') {
                i++;
            }

            if (isVarQuant) {
                totalVarQuantifiers++;
                if (totalVarQuantifiers > 24) return false;
                if (parenDepth > 0) groupHasQuantifier[parenDepth] = true;
            } else if (isBraceQ && bMin == 0 && bMax == 0) {
                if (parenDepth > 0) groupHasQuantifier[parenDepth] = true;
            }
            if (isUnbounded) {
                totalUnboundedQuantifiers++;
                groupUnboundedCount[parenDepth]++;
                if (groupUnboundedCount[parenDepth] > 8 || totalUnboundedQuantifiers > 16) return false;
            }
            canQuantifyAtom = false;
            prevAtomIsBackref = false;
            prevAtomFixedMult = 1;
        } else if (c == '^' || c == '$') {
            if (parenDepth > 0 && !branchSawFirstToken[parenDepth]) {
                branchSawFirstToken[parenDepth] = true;
                branchLeadAscii[parenDepth] = -1;
            }
            canQuantifyAtom = false;
            prevAtomIsBackref = false;
            prevAtomFixedMult = 1;
        } else {
            size_t uLen = ((unsigned char)c >= 0x80) ? utf8CharLenAt(q, i) : 1;
            if (parenDepth > 0) {
                size_t se = i + uLen;
                bool zMin = isZeroMinQuantifierAt(i + uLen, se);
                if (!branchSawFirstToken[parenDepth]) {
                    branchSawFirstToken[parenDepth] = true;
                    if (uLen == 1 && c != '.' && !zMin) {
                        branchLeadAscii[parenDepth] = (int)asciiLower(c);
                    } else {
                        branchLeadAscii[parenDepth] = -1;
                    }
                }
                if (!zMin) branchHasMandatory[parenDepth] = true;
            }
            if (uLen > 1) i += uLen - 1;
            canQuantifyAtom = true;
            prevAtomIsBackref = false;
            prevAtomFixedMult = 1;
        }
    }
    return parenDepth == 0;
}

inline void appendRegexHexByte(std::string& out, unsigned char b) {
    static const char hex[] = "0123456789ABCDEF";
    out += "\\x";
    out += hex[(b >> 4) & 0xF];
    out += hex[b & 0xF];
}

inline void appendRegexByteRange(std::string& out, unsigned char b1, unsigned char b2) {
    if (b1 == b2) {
        appendRegexHexByte(out, b1);
    } else {
        out += '[';
        appendRegexHexByte(out, b1);
        out += '-';
        appendRegexHexByte(out, b2);
        out += ']';
    }
}

inline void appendAsciiBracketChar(std::string& out, unsigned char b) {
    if (b == '\\' || b == ']' || b == '[' || b == '-' || b == '^') {
        out += '\\';
        out += (char)b;
    } else if (b < 0x20u || b == 0x7Fu) {
        appendRegexHexByte(out, b);
    } else {
        out += (char)b;
    }
}

inline int encodeCodePointUtf8Bytes(uint32_t cp, unsigned char out[4]) noexcept {
    if (cp > 0x10FFFFu || (cp >= 0xD800u && cp <= 0xDFFFu)) {
        cp = 0xFFFDu;
    }
    if (cp <= 0x7Fu) {
        out[0] = (unsigned char)cp;
        return 1;
    } else if (cp <= 0x7FFu) {
        out[0] = (unsigned char)(0xC0u | (cp >> 6));
        out[1] = (unsigned char)(0x80u | (cp & 0x3Fu));
        return 2;
    } else if (cp <= 0xFFFFu) {
        out[0] = (unsigned char)(0xE0u | (cp >> 12));
        out[1] = (unsigned char)(0x80u | ((cp >> 6) & 0x3Fu));
        out[2] = (unsigned char)(0x80u | (cp & 0x3Fu));
        return 3;
    } else {
        out[0] = (unsigned char)(0xF0u | (cp >> 18));
        out[1] = (unsigned char)(0x80u | ((cp >> 12) & 0x3Fu));
        out[2] = (unsigned char)(0x80u | ((cp >> 6) & 0x3Fu));
        out[3] = (unsigned char)(0x80u | (cp & 0x3Fu));
        return 4;
    }
}

inline void decomposeUtf8TupleRange(const unsigned char* s, const unsigned char* e, int depth, int len,
                                    std::vector<std::string>& alts, const std::string& prefix = "") {
    if (depth == len - 1) {
        std::string pat = prefix;
        appendRegexByteRange(pat, s[depth], e[depth]);
        alts.push_back(pat);
        return;
    }
    if (s[depth] == e[depth]) {
        std::string nextPrefix = prefix;
        appendRegexHexByte(nextPrefix, s[depth]);
        decomposeUtf8TupleRange(s, e, depth + 1, len, alts, nextPrefix);
        return;
    }
    bool sAllMin = true;
    bool eAllMax = true;
    for (int k = depth + 1; k < len; ++k) {
        if (s[k] != 0x80) sAllMin = false;
        if (e[k] != 0xBF) eAllMax = false;
    }
    unsigned char midStart = s[depth];
    unsigned char midEnd = e[depth];
    if (!sAllMin) {
        unsigned char hi[4] = { 0xBF, 0xBF, 0xBF, 0xBF };
        std::string p = prefix;
        appendRegexHexByte(p, s[depth]);
        decomposeUtf8TupleRange(s, hi, depth + 1, len, alts, p);
        midStart++;
    }
    if (!eAllMax) {
        midEnd--;
    }
    if (midStart <= midEnd) {
        std::string p = prefix;
        appendRegexByteRange(p, midStart, midEnd);
        for (int k = depth + 1; k < len; ++k) {
            p += "[\\x80-\\xBF]";
        }
        alts.push_back(p);
    }
    if (!eAllMax) {
        unsigned char lo[4] = { 0x80, 0x80, 0x80, 0x80 };
        std::string p = prefix;
        appendRegexHexByte(p, e[depth]);
        decomposeUtf8TupleRange(lo, e, depth + 1, len, alts, p);
    }
}

inline void appendCodePointRangeAlternates(uint32_t cp1, uint32_t cp2, std::vector<std::string>& alts) {
    if (cp1 > cp2) return;
    const uint32_t bounds[5][2] = {
        { 0x0000u, 0x007Fu },
        { 0x0080u, 0x07FFu },
        { 0x0800u, 0xD7FFu },
        { 0xE000u, 0xFFFFu },
        { 0x10000u, 0x10FFFFu }
    };
    for (int i = 0; i < 5; ++i) {
        uint32_t lo = std::max(cp1, bounds[i][0]);
        uint32_t hi = std::min(cp2, bounds[i][1]);
        if (lo <= hi) {
            unsigned char bLo[4], bHi[4];
            int len = encodeCodePointUtf8Bytes(lo, bLo);
            encodeCodePointUtf8Bytes(hi, bHi);
            decomposeUtf8TupleRange(bLo, bHi, 0, len, alts);
        }
    }
}

inline int countCapturingGroups(std::string_view rawQuery) noexcept {
    int count = 0;
    for (size_t i = 0; i < rawQuery.size(); ++i) {
        char c = rawQuery[i];
        if (c == '\\') {
            if (i + 1 < rawQuery.size()) {
                size_t escLen = ((unsigned char)rawQuery[i + 1] >= 0x80u) ? utf8CharLenAt(rawQuery, i + 1) : 1;
                i += escLen;
            }
            continue;
        }
        if (c == '[') {
            bool malformed = false;
            size_t closeIdx = findRegexBracketClose(rawQuery, i, malformed);
            if (closeIdx == std::string::npos) break;
            i = closeIdx;
            continue;
        }
        if (c == '(') {
            if (!(i + 1 < rawQuery.size() && rawQuery[i + 1] == '?')) {
                count++;
            }
        }
    }
    return count;
}

inline bool splitTopLevelRegexBranches(std::string_view q, std::vector<std::string_view>& outBranches) {
    outBranches.clear();
    int parenDepth = 0;
    size_t branchStart = 0;
    for (size_t i = 0; i < q.size(); ++i) {
        char c = q[i];
        if (c == '\\') {
            if (i + 1 >= q.size()) return false;
            size_t escLen = ((unsigned char)q[i + 1] >= 0x80u) ? utf8CharLenAt(q, i + 1) : 1;
            i += escLen;
            continue;
        }
        if (c == '[') {
            bool malformed = false;
            size_t closeIdx = findRegexBracketClose(q, i, malformed);
            if (malformed || closeIdx == std::string::npos) return false;
            i = closeIdx;
            continue;
        }
        if (c == '(') {
            parenDepth++;
            if (i + 1 < q.size() && q[i + 1] == '?' && i + 2 < q.size() &&
                (q[i + 2] == ':' || q[i + 2] == '=' || q[i + 2] == '!')) {
                i += 2;
            }
            continue;
        }
        if (c == ')') {
            if (parenDepth <= 0) return false;
            parenDepth--;
            continue;
        }
        if (c == '|' && parenDepth == 0) {
            outBranches.push_back(q.substr(branchStart, i - branchStart));
            branchStart = i + 1;
            continue;
        }
        if ((unsigned char)c >= 0x80u) {
            size_t uLen = utf8CharLenAt(q, i);
            if (uLen > 1) i += uLen - 1;
        }
    }
    if (parenDepth != 0) return false;
    outBranches.push_back(q.substr(branchStart, q.size() - branchStart));
    return !outBranches.empty();
}

inline bool hasCaretWrapperGroup2(std::string_view rawQuery) noexcept {
    if (rawQuery.empty() || rawQuery[0] != '^') return false;
    int parenDepth = 0;
    for (size_t i = 0; i < rawQuery.size(); ++i) {
        char c = rawQuery[i];
        if (c == '\\') {
            if (i + 1 >= rawQuery.size()) return false;
            size_t escLen = ((unsigned char)rawQuery[i + 1] >= 0x80u) ? utf8CharLenAt(rawQuery, i + 1) : 1;
            i += escLen;
            continue;
        }
        if (c == '[') {
            bool malformed = false;
            size_t closeIdx = findRegexBracketClose(rawQuery, i, malformed);
            if (malformed || closeIdx == std::string::npos) return false;
            i = closeIdx;
            continue;
        }
        if (c == '(') {
            parenDepth++;
            if (i + 1 < rawQuery.size() && rawQuery[i + 1] == '?' && i + 2 < rawQuery.size() &&
                (rawQuery[i + 2] == ':' || rawQuery[i + 2] == '=' || rawQuery[i + 2] == '!')) {
                i += 2;
            }
            continue;
        }
        if (c == ')') {
            if (parenDepth <= 0) return false;
            parenDepth--;
            continue;
        }
        if (c == '|' && parenDepth == 0) {
            if (i + 1 >= rawQuery.size() || rawQuery[i + 1] != '^') return false;
        }
        if ((unsigned char)c >= 0x80u) {
            size_t uLen = utf8CharLenAt(rawQuery, i);
            if (uLen > 1) i += uLen - 1;
        }
    }
    return parenDepth == 0;
}

inline bool isZeroMinRegexQuantifierAfter(std::string_view q, size_t nextIdx) noexcept {
    if (nextIdx >= q.size()) return false;
    char qc = q[nextIdx];
    if (qc == '*' || qc == '?') return true;
    if (qc == '{') {
        size_t k = nextIdx + 1;
        if (k < q.size() && std::isdigit((unsigned char)q[k])) {
            int minRep = 0;
            while (k < q.size() && std::isdigit((unsigned char)q[k])) {
                minRep = std::min(1000, minRep * 10 + (q[k] - '0'));
                k++;
            }
            if (k < q.size() && (q[k] == ',' || q[k] == '}')) {
                return minRep == 0;
            }
        }
    }
    return false;
}

inline std::string preprocessRegexBody(
    std::string_view query,
    bool captureLeadingCaret,
    int backrefShift,
    int capCount,
    const std::vector<int>* explicitUserGroupMap = nullptr,
    std::vector<int>* outBolAnchorGroups = nullptr,
    int* ioNextSmatchGroup = nullptr)
{
    static const char* kUtf8NonNewlineChar =
        "(?:[^\\r\\n\\x80-\\xFF]|[\\xC0-\\xDF][\\x80-\\xBF]|[\\xE0-\\xEF][\\x80-\\xBF][\\x80-\\xBF]|[\\xF0-\\xF7][\\x80-\\xBF][\\x80-\\xBF][\\xF0-\\xF7]?[\\x80-\\xBF]?)";
    (void)kUtf8NonNewlineChar;
    static const char* kUtf8NonNewlineExact =
        "(?:[^\\r\\n\\x80-\\xFF]|[\\xC0-\\xDF][\\x80-\\xBF]|[\\xE0-\\xEF][\\x80-\\xBF][\\x80-\\xBF]|[\\xF0-\\xF7][\\x80-\\xBF][\\x80-\\xBF][\\x80-\\xBF])";
    static const char* kUtf8AnyChar =
        "(?:[^\\x80-\\xFF]|[\\xC0-\\xDF][\\x80-\\xBF]|[\\xE0-\\xEF][\\x80-\\xBF][\\x80-\\xBF]|[\\xF0-\\xF7][\\x80-\\xBF][\\x80-\\xBF][\\x80-\\xBF])";
    static const char* kUtf8MultiByteOnly =
        "[\\xC0-\\xDF][\\x80-\\xBF]|[\\xE0-\\xEF][\\x80-\\xBF][\\x80-\\xBF]|[\\xF0-\\xF7][\\x80-\\xBF][\\x80-\\xBF][\\x80-\\xBF]";

    auto skipOpenParensToBolCaret = [&](size_t startPos, size_t& outCaretPos, std::string& outOpenSeq, int& outUserCapOpens) -> bool {
        outOpenSeq.clear();
        outUserCapOpens = 0;
        size_t k = startPos;
        while (k < query.size() && query[k] == '(') {
            if (k + 2 < query.size() && query[k + 1] == '?' && query[k + 2] == ':') {
                outOpenSeq += "(?:";
                k += 3;
            } else if (k + 1 < query.size() && query[k + 1] == '?') {
                return false;
            } else {
                outOpenSeq += '(';
                outUserCapOpens++;
                k += 1;
            }
        }
        if (k > startPos && k < query.size() && query[k] == '^') {
            outCaretPos = k;
            return true;
        }
        return false;
    };

    auto mapBackrefGroup = [&](int userGrp) -> int {
        if (explicitUserGroupMap && userGrp >= 1 && (size_t)userGrp < explicitUserGroupMap->size()) {
            return (*explicitUserGroupMap)[userGrp];
        }
        return userGrp + backrefShift;
    };

    int localNextGrp = ioNextSmatchGroup ? *ioNextSmatchGroup : 1;
    bool atBranchStart = true;
    bool afterNewlineAtom = false;

    std::string processed;
    processed.reserve(query.size() * 6);
    for (size_t i = 0; i < query.size(); ++i) {
        unsigned char uc = (unsigned char)query[i];
        char c = query[i];
        if (c == '\\' && i + 1 < query.size()) {
            char next = query[i + 1];
            if (next == 'n') {
                bool isPrecededByCR = false;
                if (i >= 2 && query[i - 1] == 'r') {
                    size_t bs = 0;
                    size_t k = i - 1;
                    while (k > 0 && query[k - 1] == '\\') {
                        ++bs;
                        --k;
                    }
                    isPrecededByCR = ((bs & 1u) != 0);
                }
                if (!isPrecededByCR) {
                    processed += "(?:\\r\\n|\\r(?!\\n)|\\n)";
                } else {
                    processed += "\\n";
                }
                atBranchStart = false;
                afterNewlineAtom = !isZeroMinRegexQuantifierAfter(query, i + 2);
                i++;
                continue;
            }
            if (next == 'u' && i + 5 < query.size()) {
                int h1 = regexHexDigitVal(query[i + 2]);
                int h2 = regexHexDigitVal(query[i + 3]);
                int h3 = regexHexDigitVal(query[i + 4]);
                int h4 = regexHexDigitVal(query[i + 5]);
                if (h1 >= 0 && h2 >= 0 && h3 >= 0 && h4 >= 0) {
                    uint32_t cp = (uint32_t)((h1 << 12) | (h2 << 8) | (h3 << 4) | h4);
                    if (cp >= 0xD800u && cp <= 0xDBFFu) {
                        if (i + 11 < query.size() && query[i + 6] == '\\' && query[i + 7] == 'u') {
                            int l1 = regexHexDigitVal(query[i + 8]);
                            int l2 = regexHexDigitVal(query[i + 9]);
                            int l3 = regexHexDigitVal(query[i + 10]);
                            int l4 = regexHexDigitVal(query[i + 11]);
                            if (l1 >= 0 && l2 >= 0 && l3 >= 0 && l4 >= 0) {
                                uint32_t lo = (uint32_t)((l1 << 12) | (l2 << 8) | (l3 << 4) | l4);
                                if (lo >= 0xDC00u && lo <= 0xDFFFu) {
                                    uint32_t fullCp = 0x10000u + (((cp - 0xD800u) << 10) | (lo - 0xDC00u));
                                    unsigned char u8[4];
                                    int nBytes = encodeCodePointUtf8Bytes(fullCp, u8);
                                    processed += "(?:";
                                    for (int b = 0; b < nBytes; ++b) appendRegexHexByte(processed, u8[b]);
                                    processed += ')';
                                    atBranchStart = false;
                                    afterNewlineAtom = false;
                                    i += 11;
                                    continue;
                                }
                            }
                        }
                        processed += "(?=a)b";
                        atBranchStart = false;
                        afterNewlineAtom = false;
                        i += 5;
                        continue;
                    } else if (cp >= 0xDC00u && cp <= 0xDFFFu) {
                        processed += "(?=a)b";
                        atBranchStart = false;
                        afterNewlineAtom = false;
                        i += 5;
                        continue;
                    } else if (cp >= 0x80u) {
                        unsigned char u8[4];
                        int nBytes = encodeCodePointUtf8Bytes(cp, u8);
                        processed += "(?:";
                        for (int b = 0; b < nBytes; ++b) appendRegexHexByte(processed, u8[b]);
                        processed += ')';
                        atBranchStart = false;
                        afterNewlineAtom = false;
                        i += 5;
                        continue;
                    } else if (cp == 0x0Au) {
                        processed += "\\x0A";
                        atBranchStart = false;
                        afterNewlineAtom = !isZeroMinRegexQuantifierAfter(query, i + 6);
                        i += 5;
                        continue;
                    } else {
                        appendRegexHexByte(processed, (unsigned char)cp);
                        atBranchStart = false;
                        afterNewlineAtom = false;
                        i += 5;
                        continue;
                    }
                }
            }
            if ((backrefShift > 0 || explicitUserGroupMap != nullptr) && next >= '1' && next <= '9') {
                int d1 = next - '0';
                size_t k = i + 1;
                int fullNum = 0;
                while (k < query.size() && std::isdigit((unsigned char)query[k])) {
                    fullNum = std::min(10000, fullNum * 10 + (query[k] - '0'));
                    k++;
                }
                atBranchStart = false;
                afterNewlineAtom = false;
                if (k == i + 2) {
                    processed += '\\';
                    processed += std::to_string(mapBackrefGroup(d1));
                    i++;
                    continue;
                } else if (fullNum <= capCount) {
                    processed += '\\';
                    processed += std::to_string(mapBackrefGroup(fullNum));
                    i = k - 1;
                    continue;
                } else {
                    processed += "(?:\\";
                    processed += std::to_string(mapBackrefGroup(d1));
                    processed += ')';
                    i++;
                    continue;
                }
            }
            size_t escLen = 1;
            if ((unsigned char)next >= 0x80u) {
                escLen = utf8CharLenAt(query, i + 1);
            }
            processed += c;
            processed.append(query.data() + i + 1, escLen);
            atBranchStart = false;
            afterNewlineAtom = false;
            i += escLen;
            continue;
        }
        if (c == '[') {
            atBranchStart = false;
            afterNewlineAtom = false;
            bool malformedPosix = false;
            size_t closeIdx = findRegexBracketClose(query, i, malformedPosix);
            if (malformedPosix || closeIdx == std::string::npos) {
                processed += "(?=a)b";
                return processed;
            }
            bool negated = (i + 1 < closeIdx && query[i + 1] == '^');
            size_t contentStart = negated ? (i + 2) : (i + 1);
            if (!validateRegexBracketContent(query, contentStart, closeIdx)) {
                processed += "(?=a)b";
                i = closeIdx;
                continue;
            }
            bool hasMultiByte = false;
            bool hasRawHexEscape = false;
            for (size_t j = contentStart; j < closeIdx; ++j) {
                if ((unsigned char)query[j] >= 0x80u) hasMultiByte = true;
                if (query[j] == '\\' && j + 1 < closeIdx) {
                    if (query[j + 1] == 'x') {
                        hasRawHexEscape = true;
                    } else if (query[j + 1] == 'u') {
                        hasRawHexEscape = true;
                        hasMultiByte = true;
                    }
                    j++;
                }
            }
            if (!hasMultiByte) {
                if (negated && !hasRawHexEscape) {
                    processed += "(?:[^";
                    if (contentStart < closeIdx && query[contentStart] == ']') {
                        processed += "\\]";
                        processed.append(query.data() + contentStart + 1, closeIdx - (contentStart + 1));
                    } else {
                        processed.append(query.data() + contentStart, closeIdx - contentStart);
                    }
                    processed += "\\x80-\\xFF]|";
                    processed += kUtf8MultiByteOnly;
                    processed += ')';
                } else if (contentStart < closeIdx && query[contentStart] == ']') {
                    processed += (negated ? "[^\\]" : "[\\]");
                    processed.append(query.data() + contentStart + 1, closeIdx - (contentStart + 1) + 1);
                } else {
                    processed.append(query.data() + i, closeIdx - i + 1);
                }
                i = closeIdx;
                continue;
            }
            // Multi-byte UTF-8 character class: decompose into safe ASCII bracket part + UTF-8 alternatives
            std::string asciiPart;
            std::vector<std::string> mbAlts;
            size_t p = contentStart;
            while (p < closeIdx) {
                RegexBracketAtom atom1 = parseRegexBracketAtom(query, p, closeIdx);
                if (!atom1.valid) {
                    asciiPart.clear();
                    mbAlts.clear();
                    break;
                }
                if (atom1.nextPos < closeIdx && query[atom1.nextPos] == '-' && atom1.nextPos + 1 < closeIdx) {
                    RegexBracketAtom atom2 = parseRegexBracketAtom(query, atom1.nextPos + 1, closeIdx);
                    if (!atom2.valid || atom1.isClassSet || atom2.isClassSet || atom1.cp > atom2.cp) {
                        asciiPart.clear();
                        mbAlts.clear();
                        break;
                    }
                    if (atom1.cp < 0x80u && atom2.cp < 0x80u) {
                        appendAsciiBracketChar(asciiPart, (unsigned char)atom1.cp);
                        asciiPart += '-';
                        appendAsciiBracketChar(asciiPart, (unsigned char)atom2.cp);
                    } else {
                        appendCodePointRangeAlternates(atom1.cp, atom2.cp, mbAlts);
                    }
                    p = atom2.nextPos;
                    continue;
                }
                if (atom1.isClassSet) {
                    asciiPart.append(query.data() + p, atom1.nextPos - p);
                } else if (atom1.cp < 0x80u) {
                    appendAsciiBracketChar(asciiPart, (unsigned char)atom1.cp);
                } else if (query[p] == '\\' && p + 1 < closeIdx && query[p + 1] == 'x') {
                    std::string seq;
                    appendRegexHexByte(seq, (unsigned char)atom1.cp);
                    mbAlts.push_back(seq);
                } else {
                    unsigned char u8[4];
                    int nBytes = encodeCodePointUtf8Bytes(atom1.cp, u8);
                    std::string seq;
                    for (int k = 0; k < nBytes; ++k) appendRegexHexByte(seq, u8[k]);
                    mbAlts.push_back(seq);
                }
                p = atom1.nextPos;
            }
            if (asciiPart.empty() && mbAlts.empty()) {
                processed += "(?=a)b";
                i = closeIdx;
                continue;
            }
            std::string posGroup = "(?:";
            bool firstAlt = true;
            if (!asciiPart.empty()) {
                posGroup += '[';
                posGroup += asciiPart;
                posGroup += ']';
                firstAlt = false;
            }
            for (const auto& alt : mbAlts) {
                if (!firstAlt) posGroup += '|';
                posGroup += alt;
                firstAlt = false;
            }
            posGroup += ')';
            if (!negated) {
                processed += posGroup;
            } else {
                processed += "(?:(?!";
                processed += posGroup;
                processed += ')';
                processed += kUtf8AnyChar;
                processed += ')';
            }
            i = closeIdx;
            continue;
        }
        if (c == '(') {
            if (outBolAnchorGroups && atBranchStart && !afterNewlineAtom) {
                size_t caretPos = 0;
                std::string openSeq;
                int userCapOpens = 0;
                if (skipOpenParensToBolCaret(i, caretPos, openSeq, userCapOpens)) {
                    processed += "((?:^|(?:\\r\\n|\\r(?!\\n)|[\\n])))";
                    outBolAnchorGroups->push_back(localNextGrp++);
                    processed += openSeq;
                    localNextGrp += userCapOpens;
                    atBranchStart = false;
                    afterNewlineAtom = false;
                    i = caretPos;
                    continue;
                }
            }
            if (i + 2 < query.size() && query[i + 1] == '?' &&
                (query[i + 2] == ':' || query[i + 2] == '=' || query[i + 2] == '!')) {
                char kind = query[i + 2];
                processed.append(query.data() + i, 3);
                if (kind != ':') {
                    atBranchStart = false;
                    afterNewlineAtom = false;
                }
                i += 2;
                continue;
            }
            if (!(i + 1 < query.size() && query[i + 1] == '?')) {
                localNextGrp++;
            }
            processed += '(';
            continue;
        }
        if (c == '|') {
            processed += '|';
            atBranchStart = true;
            afterNewlineAtom = false;
            continue;
        }
        if (c == '^') {
            if (afterNewlineAtom) {
                if (i + 1 < query.size() && (query[i + 1] == '*' || query[i + 1] == '+' || query[i + 1] == '?' || query[i + 1] == '{')) {
                    processed += "(?=$|\\s|\\S)";
                }
                atBranchStart = false;
                continue;
            }
            if (outBolAnchorGroups && atBranchStart) {
                processed += "((?:^|(?:\\r\\n|\\r(?!\\n)|[\\n])))";
                outBolAnchorGroups->push_back(localNextGrp++);
                atBranchStart = false;
                continue;
            }
            if (i == 0 && captureLeadingCaret) {
                processed += "((?:^|(?:\\r\\n|\\r(?!\\n)|[\\n])))";
                localNextGrp++;
            } else if (atBranchStart) {
                processed += "(?:^|(?:\\r\\n|\\r(?!\\n)|[\\n]))";
            } else {
                processed += "(?=a)b";
            }
            atBranchStart = false;
            continue;
        }
        if (c == '$') {
            processed += "(?=(?:\\r\\n|\\r(?!\\n)|\\n|$))";
            atBranchStart = false;
            afterNewlineAtom = false;
            continue;
        }
        if (c == '.') {
            processed += kUtf8NonNewlineExact;
            atBranchStart = false;
            afterNewlineAtom = false;
            continue;
        }
        if (c == '\n') {
            processed += "\\n";
            atBranchStart = false;
            afterNewlineAtom = !isZeroMinRegexQuantifierAfter(query, i + 1);
            continue;
        }
        if (c == '+') {
            processed += '+';
            atBranchStart = false;
            continue;
        }
        if (uc >= 0x80u) {
            atBranchStart = false;
            afterNewlineAtom = false;
            size_t uLen = utf8CharLenAt(query, i);
            if (uLen > 1 && i + uLen < query.size()) {
                char next = query[i + uLen];
                if (next == '*' || next == '+' || next == '?' || next == '{') {
                    processed += "(?:";
                    processed.append(query.data() + i, uLen);
                    processed += ')';
                    i += uLen - 1;
                    continue;
                }
            }
            processed.append(query.data() + i, uLen);
            i += uLen - 1;
            continue;
        }
        atBranchStart = false;
        afterNewlineAtom = false;
        processed += c;
    }
    if (ioNextSmatchGroup) *ioNextSmatchGroup = localNextGrp;
    return processed;
}

struct RegexPreprocessInfo {
    std::string pattern;
    std::vector<int> bolAnchorGroups;
    std::vector<int> userGroupMap;
    bool hasAnyBolCaret = false;
    bool endsWithDollar = false;
};

inline bool hasUnescapedTrailingDollar(std::string_view query) noexcept;

inline RegexPreprocessInfo preprocessRegexEx(std::string_view query) {
    RegexPreprocessInfo info;
    const int capCount = countCapturingGroups(query);
    info.userGroupMap.assign((size_t)capCount + 1, 0);
    info.endsWithDollar = hasUnescapedTrailingDollar(query);

    if (hasCaretWrapperGroup2(query)) {
        std::vector<std::string_view> branches;
        if (splitTopLevelRegexBranches(query, branches)) {
            info.bolAnchorGroups.push_back(1);
            info.hasAnyBolCaret = true;
            for (int g = 1; g <= capCount; ++g) info.userGroupMap[(size_t)g] = g + 2;
            info.pattern = "((?:^|(?:\\r\\n|\\r(?!\\n)|[\\n])))(";
            for (size_t b = 0; b < branches.size(); ++b) {
                if (b > 0) info.pattern += '|';
                info.pattern += preprocessRegexBody(branches[b].substr(1), false, 2, capCount, &info.userGroupMap, nullptr, nullptr);
            }
            info.pattern += ')';
            return info;
        }
    }

    // Pass 1: compute exact smatch group indices for user capturing groups after BOL caret hoisting
    {
        auto skipOpenParensToBolCaret = [&](size_t startPos, size_t& outCaretPos, int& outUserCapOpens) -> bool {
            outUserCapOpens = 0;
            size_t k = startPos;
            while (k < query.size() && query[k] == '(') {
                if (k + 2 < query.size() && query[k + 1] == '?' && query[k + 2] == ':') {
                    k += 3;
                } else if (k + 1 < query.size() && query[k + 1] == '?') {
                    return false;
                } else {
                    outUserCapOpens++;
                    k += 1;
                }
            }
            if (k > startPos && k < query.size() && query[k] == '^') {
                outCaretPos = k;
                return true;
            }
            return false;
        };

        int nextSmatchGrp = 1;
        int nextUserGrp = 1;
        bool atBranchStart = true;
        bool afterNewlineAtom = false;
        for (size_t i = 0; i < query.size(); ++i) {
            char c = query[i];
            if (c == '\\' && i + 1 < query.size()) {
                char next = query[i + 1];
                if (next == 'n') {
                    atBranchStart = false;
                    afterNewlineAtom = !isZeroMinRegexQuantifierAfter(query, i + 2);
                    i++;
                    continue;
                }
                if (next == 'u' && i + 5 < query.size()) {
                    int h1 = regexHexDigitVal(query[i + 2]);
                    int h2 = regexHexDigitVal(query[i + 3]);
                    int h3 = regexHexDigitVal(query[i + 4]);
                    int h4 = regexHexDigitVal(query[i + 5]);
                    if (h1 >= 0 && h2 >= 0 && h3 >= 0 && h4 >= 0) {
                        uint32_t cp = (uint32_t)((h1 << 12) | (h2 << 8) | (h3 << 4) | h4);
                        atBranchStart = false;
                        if (cp >= 0xD800u && cp <= 0xDBFFu && i + 11 < query.size() && query[i + 6] == '\\' && query[i + 7] == 'u') {
                            int l1 = regexHexDigitVal(query[i + 8]);
                            int l2 = regexHexDigitVal(query[i + 9]);
                            int l3 = regexHexDigitVal(query[i + 10]);
                            int l4 = regexHexDigitVal(query[i + 11]);
                            if (l1 >= 0 && l2 >= 0 && l3 >= 0 && l4 >= 0) {
                                uint32_t lo = (uint32_t)((l1 << 12) | (l2 << 8) | (l3 << 4) | l4);
                                if (lo >= 0xDC00u && lo <= 0xDFFFu) {
                                    afterNewlineAtom = false;
                                    i += 11;
                                    continue;
                                }
                            }
                        }
                        afterNewlineAtom = (cp == 0x0Au) && !isZeroMinRegexQuantifierAfter(query, i + 6);
                        i += 5;
                        continue;
                    }
                }
                size_t escLen = ((unsigned char)next >= 0x80u) ? utf8CharLenAt(query, i + 1) : 1;
                atBranchStart = false;
                afterNewlineAtom = false;
                i += escLen;
                continue;
            }
            if (c == '[') {
                atBranchStart = false;
                afterNewlineAtom = false;
                bool malformed = false;
                size_t closeIdx = findRegexBracketClose(query, i, malformed);
                if (malformed || closeIdx == std::string::npos) break;
                i = closeIdx;
                continue;
            }
            if (c == '(') {
                if (atBranchStart && !afterNewlineAtom) {
                    size_t caretPos = 0;
                    int userCapOpens = 0;
                    if (skipOpenParensToBolCaret(i, caretPos, userCapOpens)) {
                        nextSmatchGrp++; // synthetic BOL anchor group
                        for (int u = 0; u < userCapOpens && nextUserGrp <= capCount; ++u) {
                            info.userGroupMap[(size_t)nextUserGrp++] = nextSmatchGrp++;
                        }
                        atBranchStart = false;
                        afterNewlineAtom = false;
                        i = caretPos;
                        continue;
                    }
                }
                if (i + 2 < query.size() && query[i + 1] == '?' &&
                    (query[i + 2] == ':' || query[i + 2] == '=' || query[i + 2] == '!')) {
                    if (query[i + 2] != ':') {
                        atBranchStart = false;
                        afterNewlineAtom = false;
                    }
                    i += 2;
                    continue;
                }
                if (!(i + 1 < query.size() && query[i + 1] == '?') && nextUserGrp <= capCount) {
                    info.userGroupMap[(size_t)nextUserGrp++] = nextSmatchGrp++;
                }
                continue;
            }
            if (c == '|') {
                atBranchStart = true;
                afterNewlineAtom = false;
                continue;
            }
            if (c == '^') {
                if (!afterNewlineAtom && atBranchStart) {
                    nextSmatchGrp++;
                }
                atBranchStart = false;
                continue;
            }
            if (c == '\n') {
                atBranchStart = false;
                afterNewlineAtom = !isZeroMinRegexQuantifierAfter(query, i + 1);
                continue;
            }
            if (c == '+') {
                atBranchStart = false;
                continue;
            }
            if ((unsigned char)c >= 0x80u) {
                size_t uLen = utf8CharLenAt(query, i);
                if (uLen > 1) i += uLen - 1;
            }
            atBranchStart = false;
            afterNewlineAtom = false;
        }
    }

    int emitNextGrp = 1;
    info.pattern = preprocessRegexBody(query, false, 0, capCount, &info.userGroupMap, &info.bolAnchorGroups, &emitNextGrp);
    info.hasAnyBolCaret = !info.bolAnchorGroups.empty();
    return info;
}

inline size_t computeMatchAnchorLen(const std::smatch& m, const RegexPreprocessInfo& info, bool* outMatchedBolGroup = nullptr) noexcept {
    if (outMatchedBolGroup) *outMatchedBolGroup = false;
    for (int g : info.bolAnchorGroups) {
        if (g > 0 && (size_t)g < m.size() && m[g].matched && m.position(g) == m.position(0)) {
            if (outMatchedBolGroup) *outMatchedBolGroup = true;
            return (size_t)m.length(g);
        }
    }
    return 0;
}

inline std::string formatRegexReplacement(
    const std::smatch& m,
    std::string_view fullText,
    size_t contentPos,
    size_t contentLen,
    const std::string& rawFmt,
    const RegexPreprocessInfo& info)
{
    const int capCount = info.userGroupMap.empty() ? 0 : ((int)info.userGroupMap.size() - 1);
    auto appendUserGroup = [&](std::string& out, int userGrp) {
        if (userGrp >= 1 && userGrp <= capCount) {
            int smIdx = info.userGroupMap[(size_t)userGrp];
            if (smIdx > 0 && (size_t)smIdx < m.size() && m[smIdx].matched) {
                out += m[smIdx].str();
            }
        }
    };
    auto appendWholeContent = [&](std::string& out) {
        if (contentPos <= fullText.size() && contentLen <= fullText.size() - contentPos) {
            out.append(fullText.data() + contentPos, contentLen);
        }
    };

    std::string out;
    out.reserve(rawFmt.size() + contentLen);
    for (size_t i = 0; i < rawFmt.size(); ++i) {
        if (rawFmt[i] != '$') {
            out += rawFmt[i];
            continue;
        }
        if (i + 1 >= rawFmt.size()) {
            out += '$';
            continue;
        }
        char next = rawFmt[i + 1];
        if (next == '$') {
            out += '$';
            i += 1;
            continue;
        }
        if (next == '&') {
            appendWholeContent(out);
            i += 1;
            continue;
        }
        if (next == '`') {
            if (contentPos <= fullText.size()) out.append(fullText.data(), contentPos);
            i += 1;
            continue;
        }
        if (next == '\'') {
            size_t afterPos = contentPos + contentLen;
            if (afterPos <= fullText.size()) out.append(fullText.data() + afterPos, fullText.size() - afterPos);
            i += 1;
            continue;
        }
        if (next == '{') {
            size_t k = i + 2;
            int grp = 0;
            while (k < rawFmt.size() && std::isdigit((unsigned char)rawFmt[k])) {
                grp = std::min(1000, grp * 10 + (rawFmt[k] - '0'));
                k++;
            }
            if (k > i + 2 && k < rawFmt.size() && rawFmt[k] == '}') {
                i = k;
                if (grp == 0) appendWholeContent(out);
                else appendUserGroup(out, grp);
                continue;
            }
            out += '$';
            continue;
        }
        if (std::isdigit((unsigned char)next)) {
            int d1 = next - '0';
            if (i + 2 < rawFmt.size() && std::isdigit((unsigned char)rawFmt[i + 2])) {
                int d2 = rawFmt[i + 2] - '0';
                int grp2 = d1 * 10 + d2;
                if (grp2 == 0) {
                    i += 2;
                    appendWholeContent(out);
                } else if (grp2 <= capCount) {
                    i += 2;
                    appendUserGroup(out, grp2);
                } else if (d1 > 0 && d1 <= capCount) {
                    i += 1;
                    appendUserGroup(out, d1);
                } else {
                    i += 2;
                }
            } else {
                i += 1;
                if (d1 == 0) appendWholeContent(out);
                else appendUserGroup(out, d1);
            }
            continue;
        }
        out += '$';
    }
    return out;
}

inline std::string preprocessRegexQuery(const std::string& query) {
    return preprocessRegexEx(query).pattern;
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

inline bool hasUnescapedTrailingDollarImpl(std::string_view q, int depth) noexcept {
    if (q.empty() || depth > 32) return false;
    int parenDepth = 0;
    size_t branchStart = 0;
    bool lastWasTopLevelDollar = false;
    size_t firstGroupOpen = std::string::npos;
    size_t firstGroupClose = std::string::npos;
    int topGroupCount = 0;

    auto resetBranch = [&](size_t nextStart) noexcept {
        branchStart = nextStart;
        lastWasTopLevelDollar = false;
        firstGroupOpen = std::string::npos;
        firstGroupClose = std::string::npos;
        topGroupCount = 0;
    };

    auto checkCurrentBranch = [&](size_t branchEnd) noexcept -> bool {
        if (branchEnd <= branchStart) return false;
        if (lastWasTopLevelDollar) return true;
        if (topGroupCount == 1 && firstGroupOpen == branchStart && firstGroupClose + 1 == branchEnd) {
            std::string_view grp = q.substr(branchStart, branchEnd - branchStart);
            if (grp.size() >= 2 && grp.front() == '(' && grp.back() == ')') {
                if (grp.size() >= 3 && grp[1] == '?') {
                    if (grp.size() >= 4 && grp[2] == ':') {
                        return hasUnescapedTrailingDollarImpl(grp.substr(3, grp.size() - 4), depth + 1);
                    }
                    return false;
                }
                return hasUnescapedTrailingDollarImpl(grp.substr(1, grp.size() - 2), depth + 1);
            }
        }
        return false;
    };

    for (size_t i = 0; i < q.size(); ++i) {
        char c = q[i];
        if (c == '\\') {
            if (i + 1 >= q.size()) return false;
            size_t escLen = ((unsigned char)q[i + 1] >= 0x80u) ? utf8CharLenAt(q, i + 1) : 1;
            i += escLen;
            if (parenDepth == 0) lastWasTopLevelDollar = false;
            continue;
        }
        if (c == '[') {
            bool malformed = false;
            size_t closeIdx = findRegexBracketClose(q, i, malformed);
            if (malformed || closeIdx == std::string::npos) return false;
            i = closeIdx;
            if (parenDepth == 0) lastWasTopLevelDollar = false;
            continue;
        }
        if (c == '(') {
            if (parenDepth == 0) {
                topGroupCount++;
                if (topGroupCount == 1) firstGroupOpen = i;
                lastWasTopLevelDollar = false;
            }
            parenDepth++;
            if (i + 1 < q.size() && q[i + 1] == '?' && i + 2 < q.size() &&
                (q[i + 2] == ':' || q[i + 2] == '=' || q[i + 2] == '!')) {
                i += 2;
            }
            continue;
        }
        if (c == ')') {
            if (parenDepth <= 0) return false;
            parenDepth--;
            if (parenDepth == 0) {
                if (topGroupCount == 1) firstGroupClose = i;
                lastWasTopLevelDollar = false;
            }
            continue;
        }
        if (c == '|' && parenDepth == 0) {
            if (!checkCurrentBranch(i)) return false;
            resetBranch(i + 1);
            continue;
        }
        if (parenDepth == 0) {
            lastWasTopLevelDollar = (c == '$');
        }
        if ((unsigned char)c >= 0x80u) {
            size_t uLen = utf8CharLenAt(q, i);
            if (uLen > 1) i += uLen - 1;
        }
    }
    if (parenDepth != 0) return false;
    return checkCurrentBranch(q.size());
}

inline bool hasUnescapedTrailingDollar(std::string_view query) noexcept {
    return hasUnescapedTrailingDollarImpl(query, 0);
}

inline std::string adjustCaretGroupReferences(
    const std::string& rawFmt,
    bool startsWithCaret,
    std::string_view rawQuery = "")
{
    int shift = 0;
    if (startsWithCaret) {
        shift = (rawQuery.empty() || hasCaretWrapperGroup2(rawQuery)) ? 2 : 1;
    }
    const bool hasCapLimit = !rawQuery.empty();
    const int capCount = hasCapLimit ? countCapturingGroups(rawQuery) : -1;

    auto appendTwoDigitGroup = [](std::string& out, int actualGrp) {
        if (actualGrp >= 1 && actualGrp <= 99) {
            out += '$';
            if (actualGrp < 10) out += '0';
            out += std::to_string(actualGrp);
        }
    };

    std::string fmt;
    fmt.reserve(rawFmt.size() + 12);
    for (size_t i = 0; i < rawFmt.size(); ++i) {
        if (rawFmt[i] != '$') {
            fmt += rawFmt[i];
            continue;
        }
        if (i + 1 >= rawFmt.size()) {
            fmt += "$$";
            continue;
        }
        char next = rawFmt[i + 1];
        if (next == '{') {
            size_t k = i + 2;
            int grp = 0;
            while (k < rawFmt.size() && std::isdigit((unsigned char)rawFmt[k])) {
                grp = std::min(1000, grp * 10 + (rawFmt[k] - '0'));
                k++;
            }
            if (k > i + 2 && k < rawFmt.size() && rawFmt[k] == '}') {
                i = k;
                if (grp == 0) {
                    fmt += (shift == 2) ? "$02" : "$&";
                } else if (!hasCapLimit || grp <= capCount) {
                    appendTwoDigitGroup(fmt, grp + shift);
                }
                continue;
            }
            fmt += "$$";
            continue;
        }
        if (std::isdigit((unsigned char)next)) {
            int d1 = next - '0';
            if (i + 2 < rawFmt.size() && std::isdigit((unsigned char)rawFmt[i + 2])) {
                int d2 = rawFmt[i + 2] - '0';
                int grp2 = d1 * 10 + d2;
                if (grp2 == 0) {
                    i += 2;
                    fmt += (shift == 2) ? "$02" : "$&";
                } else if (hasCapLimit) {
                    if (grp2 <= capCount) {
                        i += 2;
                        appendTwoDigitGroup(fmt, grp2 + shift);
                    } else if (d1 > 0 && d1 <= capCount) {
                        i += 1;
                        appendTwoDigitGroup(fmt, d1 + shift);
                    } else {
                        i += 2;
                    }
                } else {
                    i += 2;
                    appendTwoDigitGroup(fmt, grp2 + shift);
                }
            } else {
                i += 1;
                if (d1 == 0) {
                    fmt += (shift == 2) ? "$02" : "$&";
                } else if (!hasCapLimit || d1 <= capCount) {
                    appendTwoDigitGroup(fmt, d1 + shift);
                }
            }
            continue;
        }
        if (next == '&') {
            i += 1;
            fmt += (shift == 2) ? "$02" : "$&";
            continue;
        }
        if (next == '$' || next == '`' || next == '\'') {
            fmt += '$';
            fmt += next;
            i += 1;
            continue;
        }
        fmt += "$$";
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
            if (!isWordBoundaryBefore(fullText, p)) return false;
            if (!isWordBoundaryAfter(fullText, p + qLen)) return false;
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

// ============================================================================
// 5. Dialog Work-Area Clamping & Visible-Parent Placement
// ============================================================================

struct DialogRect {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
};

inline int clampInt64ToInt(int64_t v) noexcept {
    if (v < (int64_t)INT_MIN) return INT_MIN;
    if (v > (int64_t)INT_MAX) return INT_MAX;
    return (int)v;
}

inline int64_t clampCoordToWorkSpan(
    int64_t pos,
    int64_t winExtent,
    int64_t workStart,
    int64_t workEnd,
    int64_t margin) noexcept
{
    const int64_t workSpan = workEnd - workStart;
    if (workSpan <= 0) return pos;
    const int64_t w = (winExtent > 0) ? winExtent : 0;
    int64_t effMargin = (margin > 0) ? margin : 0;
    if (workSpan <= w) {
        effMargin = 0;
    } else {
        const int64_t maxMargin = (workSpan - w) / 2;
        if (effMargin > maxMargin) effMargin = maxMargin;
    }
    const int64_t minPos = workStart + effMargin;
    int64_t maxPos = (workEnd - effMargin) - w;
    if (maxPos < minPos) maxPos = minPos;
    return std::clamp<int64_t>(pos, minPos, maxPos);
}

inline void clampRectPositionToWorkArea(
    int& ioX,
    int& ioY,
    int winW,
    int winH,
    const DialogRect& workArea,
    int margin = 0) noexcept
{
    const int64_t workW = (int64_t)workArea.right - (int64_t)workArea.left;
    const int64_t workH = (int64_t)workArea.bottom - (int64_t)workArea.top;
    if (workW <= 0 || workH <= 0) return;

    const int64_t clampedW = std::max<int64_t>(0, (int64_t)winW);
    const int64_t clampedH = std::max<int64_t>(0, (int64_t)winH);
    const int64_t x = clampCoordToWorkSpan((int64_t)ioX, clampedW, (int64_t)workArea.left, (int64_t)workArea.right, (int64_t)margin);
    const int64_t y = clampCoordToWorkSpan((int64_t)ioY, clampedH, (int64_t)workArea.top, (int64_t)workArea.bottom, (int64_t)margin);
    ioX = clampInt64ToInt(x);
    ioY = clampInt64ToInt(y);
}

inline void computeClampedDialogPosition(
    int& outX,
    int& outY,
    int winW,
    int winH,
    const DialogRect& workArea,
    const DialogRect* parentRect,
    int margin = 0) noexcept
{
    const int64_t clampedW = std::max<int64_t>(0, (int64_t)winW);
    const int64_t clampedH = std::max<int64_t>(0, (int64_t)winH);
    const int64_t workW = (int64_t)workArea.right - (int64_t)workArea.left;
    const int64_t workH = (int64_t)workArea.bottom - (int64_t)workArea.top;
    if (workW <= 0 || workH <= 0) {
        if (parentRect && (int64_t)parentRect->right > (int64_t)parentRect->left &&
            (int64_t)parentRect->bottom > (int64_t)parentRect->top) {
            const int64_t pLeft = (int64_t)parentRect->left;
            const int64_t pTop  = (int64_t)parentRect->top;
            const int64_t pW    = (int64_t)parentRect->right - pLeft;
            const int64_t pH    = (int64_t)parentRect->bottom - pTop;
            const int64_t cx    = pLeft + (pW - clampedW) / 2;
            const int64_t cy    = pTop  + (pH - clampedH) / 2;
            outX = clampInt64ToInt(cx);
            outY = clampInt64ToInt(cy);
        } else {
            outX = 0;
            outY = 0;
        }
        return;
    }

    int64_t x = (int64_t)workArea.left + (workW - clampedW) / 2;
    int64_t y = (int64_t)workArea.top  + (workH - clampedH) / 2;

    if (parentRect && (int64_t)parentRect->right > (int64_t)parentRect->left &&
        (int64_t)parentRect->bottom > (int64_t)parentRect->top) {
        const int64_t visLeft   = std::max<int64_t>(parentRect->left,   workArea.left);
        const int64_t visTop    = std::max<int64_t>(parentRect->top,    workArea.top);
        const int64_t visRight  = std::min<int64_t>(parentRect->right,  workArea.right);
        const int64_t visBottom = std::min<int64_t>(parentRect->bottom, workArea.bottom);
        if (visRight > visLeft && visBottom > visTop) {
            x = visLeft + ((visRight - visLeft) - clampedW) / 2;
            y = visTop  + ((visBottom - visTop) - clampedH) / 2;
        }
    }

    x = clampCoordToWorkSpan(x, clampedW, (int64_t)workArea.left, (int64_t)workArea.right, (int64_t)margin);
    y = clampCoordToWorkSpan(y, clampedH, (int64_t)workArea.top, (int64_t)workArea.bottom, (int64_t)margin);
    outX = clampInt64ToInt(x);
    outY = clampInt64ToInt(y);
}

} // namespace MiuCore
