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

inline bool isUtf8CharBoundary(const std::string& s, size_t pos) noexcept {
    if (pos >= s.size()) return true;
    return (((unsigned char)s[pos]) & 0xC0) != 0x80;
}

inline size_t utf8CharLenAt(const std::string& s, size_t pos) noexcept {
    if (pos >= s.size()) return 1;
    unsigned char c = (unsigned char)s[pos];
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0 && pos + 1 < s.size() && ((unsigned char)s[pos + 1] & 0xC0) == 0x80) return 2;
    if ((c & 0xF0) == 0xE0 && pos + 2 < s.size() &&
        ((unsigned char)s[pos + 1] & 0xC0) == 0x80 && ((unsigned char)s[pos + 2] & 0xC0) == 0x80) return 3;
    if ((c & 0xF8) == 0xF0 && pos + 3 < s.size() &&
        ((unsigned char)s[pos + 1] & 0xC0) == 0x80 && ((unsigned char)s[pos + 2] & 0xC0) == 0x80 &&
        ((unsigned char)s[pos + 3] & 0xC0) == 0x80) return 4;
    return 1;
}

inline size_t utf8CharLenAt(const PieceTable& pt, size_t pos) noexcept {
    size_t len = pt.length();
    if (pos >= len) return 1;
    unsigned char c = (unsigned char)pt.charAt(pos);
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0 && pos + 1 < len && ((unsigned char)pt.charAt(pos + 1) & 0xC0) == 0x80) return 2;
    if ((c & 0xF0) == 0xE0 && pos + 2 < len &&
        ((unsigned char)pt.charAt(pos + 1) & 0xC0) == 0x80 && ((unsigned char)pt.charAt(pos + 2) & 0xC0) == 0x80) return 3;
    if ((c & 0xF8) == 0xF0 && pos + 3 < len &&
        ((unsigned char)pt.charAt(pos + 1) & 0xC0) == 0x80 && ((unsigned char)pt.charAt(pos + 2) & 0xC0) == 0x80 &&
        ((unsigned char)pt.charAt(pos + 3) & 0xC0) == 0x80) return 4;
    return 1;
}

inline bool isSafeUserRegex(const std::string& q, size_t maxLen = 1024) noexcept {
    if (q.empty() || q.size() > maxLen) return false;
    int parenDepth = 0;
    bool groupHasQuantifier[64] = {};
    bool groupCanMatchEmpty[64] = {};
    bool branchHasMandatory[64] = {};
    bool groupIsLookaround[64] = {};
    bool groupHasDupAlt[64] = {};
    struct LitBranch { char bytes[32]; uint8_t len; };
    LitBranch groupBranches[32][16] = {};
    uint8_t groupBranchCount[32] = {};
    LitBranch curLit[32] = {};
    bool curPureLit[32] = {};
    bool inBracket = false;
    size_t bracketStart = 0;

    auto finishLitBranch = [&](int d) noexcept {
        if (d <= 0 || d >= 32) return;
        if (curPureLit[d] && curLit[d].len > 0) {
            const uint8_t n = curLit[d].len;
            for (uint8_t k = 0; k < groupBranchCount[d]; ++k) {
                const uint8_t m = groupBranches[d][k].len;
                const uint8_t common = (n < m) ? n : m;
                if (common > 0 && std::memcmp(curLit[d].bytes, groupBranches[d][k].bytes, common) == 0) {
                    groupHasDupAlt[d] = true;
                    break;
                }
            }
            if (groupBranchCount[d] < 16) {
                groupBranches[d][groupBranchCount[d]++] = curLit[d];
            }
        }
        curLit[d].len = 0;
        curPureLit[d] = true;
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
        if (qc == '{') {
            size_t k = pos + 1;
            if (k < q.size() && std::isdigit((unsigned char)q[k])) {
                int minRep = 0;
                while (k < q.size() && std::isdigit((unsigned char)q[k])) {
                    minRep = std::min(100000, minRep * 10 + (q[k] - '0'));
                    k++;
                }
                int maxRep = minRep;
                if (k < q.size() && q[k] == ',') {
                    k++;
                    if (k < q.size() && std::isdigit((unsigned char)q[k])) {
                        maxRep = 0;
                        while (k < q.size() && std::isdigit((unsigned char)q[k])) {
                            maxRep = std::min(100000, maxRep * 10 + (q[k] - '0'));
                            k++;
                        }
                    } else {
                        maxRep = 100000;
                    }
                }
                if (k < q.size() && q[k] == '}') {
                    if (maxRep > 1000 || (maxRep < minRep)) return false;
                    skipEnd = k + 1;
                    if (skipEnd < q.size() && q[skipEnd] == '?') skipEnd++;
                    return minRep == 0;
                }
            }
        }
        return false;
    };

    for (size_t i = 0; i < q.size(); ++i) {
        char c = q[i];
        if (inBracket) {
            if (c == '\\') {
                if (i + 1 >= q.size()) return false;
                i++;
                continue;
            }
            if (c == ']' && !(i == bracketStart + 1 || (i == bracketStart + 2 && q[bracketStart + 1] == '^'))) {
                inBracket = false;
                if (parenDepth > 0) {
                    if (parenDepth < 32) curPureLit[parenDepth] = false;
                    size_t se = i + 1;
                    if (!isZeroMinQuantifierAt(i + 1, se)) branchHasMandatory[parenDepth] = true;
                }
            }
            continue;
        }
        if (c == '\\') {
            if (i + 1 >= q.size()) return false;
            char esc = q[i + 1];
            i++;
            if (parenDepth > 0 && parenDepth < 32) curPureLit[parenDepth] = false;
            if (parenDepth > 0 && esc != 'b' && esc != 'B') {
                size_t se = i + 1;
                if (!isZeroMinQuantifierAt(i + 1, se)) branchHasMandatory[parenDepth] = true;
            }
            continue;
        }
        if (c == '[') {
            inBracket = true;
            bracketStart = i;
            if (parenDepth > 0 && parenDepth < 32) curPureLit[parenDepth] = false;
            continue;
        }
        if (c == '(') {
            if (parenDepth > 0 && parenDepth < 32) curPureLit[parenDepth] = false;
            parenDepth++;
            if (parenDepth >= 32) return false;
            groupHasQuantifier[parenDepth] = false;
            groupCanMatchEmpty[parenDepth] = false;
            branchHasMandatory[parenDepth] = false;
            groupIsLookaround[parenDepth] = false;
            groupHasDupAlt[parenDepth] = false;
            groupBranchCount[parenDepth] = 0;
            curLit[parenDepth].len = 0;
            curPureLit[parenDepth] = true;
            if (i + 2 < q.size() && q[i + 1] == '?' && (q[i + 2] == ':' || q[i + 2] == '=' || q[i + 2] == '!')) {
                if (q[i + 2] == '=' || q[i + 2] == '!') groupIsLookaround[parenDepth] = true;
                i += 2;
            }
        } else if (c == '|') {
            if (parenDepth > 0) {
                finishLitBranch(parenDepth);
                if (!branchHasMandatory[parenDepth]) groupCanMatchEmpty[parenDepth] = true;
                branchHasMandatory[parenDepth] = false;
            }
        } else if (c == ')') {
            if (parenDepth <= 0) return false;
            finishLitBranch(parenDepth);
            bool hadQ = groupHasQuantifier[parenDepth];
            bool canEmpty = groupCanMatchEmpty[parenDepth] || !branchHasMandatory[parenDepth] || groupIsLookaround[parenDepth];
            bool dupAlt = groupHasDupAlt[parenDepth];
            bool isLook = groupIsLookaround[parenDepth];
            parenDepth--;
            if (parenDepth > 0 && hadQ) groupHasQuantifier[parenDepth] = true;
            if (i + 1 < q.size()) {
                char next = q[i + 1];
                if ((next == '+' || next == '*' || next == '{') && (hadQ || canEmpty || dupAlt)) {
                    return false;
                }
            }
            if (parenDepth > 0 && !isLook && !canEmpty) {
                size_t se = i + 1;
                if (!isZeroMinQuantifierAt(i + 1, se)) branchHasMandatory[parenDepth] = true;
            }
        } else if (c == '+' || c == '*' || c == '{') {
            if (parenDepth > 0 && parenDepth < 32) curPureLit[parenDepth] = false;
            if (c == '{') {
                size_t k = i + 1;
                if (k < q.size() && std::isdigit((unsigned char)q[k])) {
                    int minRep = 0;
                    while (k < q.size() && std::isdigit((unsigned char)q[k])) {
                        minRep = std::min(100000, minRep * 10 + (q[k] - '0'));
                        k++;
                    }
                    int maxRep = minRep;
                    if (k < q.size() && q[k] == ',') {
                        k++;
                        if (k < q.size() && std::isdigit((unsigned char)q[k])) {
                            maxRep = 0;
                            while (k < q.size() && std::isdigit((unsigned char)q[k])) {
                                maxRep = std::min(100000, maxRep * 10 + (q[k] - '0'));
                                k++;
                            }
                        }
                    }
                    if (k < q.size() && q[k] == '}' && (minRep > 1000 || maxRep > 1000 || maxRep < minRep)) {
                        return false;
                    }
                }
            }
            if (parenDepth > 0) groupHasQuantifier[parenDepth] = true;
        } else if (c == '?') {
            if (parenDepth > 0 && parenDepth < 32) curPureLit[parenDepth] = false;
            // Optional / lazy modifier handled via lookahead on preceding atom
        } else if (c != '^' && c != '$') {
            if (parenDepth > 0) {
                size_t uLen = ((unsigned char)c >= 0x80) ? utf8CharLenAt(q, i) : 1;
                size_t se = i + uLen;
                bool zMin = isZeroMinQuantifierAt(i + uLen, se);
                if (!zMin) branchHasMandatory[parenDepth] = true;
                if (parenDepth < 32) {
                    if (c == '.' || zMin || curLit[parenDepth].len + uLen > 32) {
                        curPureLit[parenDepth] = false;
                    } else if (curPureLit[parenDepth]) {
                        for (size_t b = 0; b < uLen; ++b) {
                            curLit[parenDepth].bytes[curLit[parenDepth].len++] = q[i + b];
                        }
                    }
                }
                if (uLen > 1) i += uLen - 1;
            }
        }
    }
    return parenDepth == 0 && !inBracket;
}

inline uint32_t decodeUtf8CodePointAt(const std::string& s, size_t pos, size_t& outLen) noexcept {
    if (pos >= s.size()) { outLen = 1; return 0; }
    unsigned char c = (unsigned char)s[pos];
    if (c < 0x80) { outLen = 1; return c; }
    if ((c & 0xE0) == 0xC0 && pos + 1 < s.size() && ((unsigned char)s[pos + 1] & 0xC0) == 0x80) {
        outLen = 2;
        return ((uint32_t)(c & 0x1F) << 6) | (uint32_t)((unsigned char)s[pos + 1] & 0x3F);
    }
    if ((c & 0xF0) == 0xE0 && pos + 2 < s.size() &&
        ((unsigned char)s[pos + 1] & 0xC0) == 0x80 && ((unsigned char)s[pos + 2] & 0xC0) == 0x80) {
        outLen = 3;
        return ((uint32_t)(c & 0x0F) << 12) |
               ((uint32_t)((unsigned char)s[pos + 1] & 0x3F) << 6) |
               (uint32_t)((unsigned char)s[pos + 2] & 0x3F);
    }
    if ((c & 0xF8) == 0xF0 && pos + 3 < s.size() &&
        ((unsigned char)s[pos + 1] & 0xC0) == 0x80 && ((unsigned char)s[pos + 2] & 0xC0) == 0x80 &&
        ((unsigned char)s[pos + 3] & 0xC0) == 0x80) {
        outLen = 4;
        return ((uint32_t)(c & 0x07) << 18) |
               ((uint32_t)((unsigned char)s[pos + 1] & 0x3F) << 12) |
               ((uint32_t)((unsigned char)s[pos + 2] & 0x3F) << 6) |
               (uint32_t)((unsigned char)s[pos + 3] & 0x3F);
    }
    outLen = 1;
    return c;
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

inline int encodeCodePointUtf8Bytes(uint32_t cp, unsigned char out[4]) noexcept {
    if (cp <= 0x7F) {
        out[0] = (unsigned char)cp;
        return 1;
    } else if (cp <= 0x7FF) {
        out[0] = (unsigned char)(0xC0 | (cp >> 6));
        out[1] = (unsigned char)(0x80 | (cp & 0x3F));
        return 2;
    } else if (cp <= 0xFFFF) {
        out[0] = (unsigned char)(0xE0 | (cp >> 12));
        out[1] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (unsigned char)(0x80 | (cp & 0x3F));
        return 3;
    } else {
        out[0] = (unsigned char)(0xF0 | (cp >> 18));
        out[1] = (unsigned char)(0x80 | ((cp >> 12) & 0x3F));
        out[2] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
        out[3] = (unsigned char)(0x80 | (cp & 0x3F));
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
    if (cp1 > cp2) std::swap(cp1, cp2);
    const uint32_t bounds[4][2] = {
        { 0x0000, 0x007F },
        { 0x0080, 0x07FF },
        { 0x0800, 0xFFFF },
        { 0x10000, 0x10FFFF }
    };
    for (int i = 0; i < 4; ++i) {
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

inline std::string preprocessRegexQuery(const std::string& query) {
    static const char* kUtf8NonNewlineChar =
        "(?:[^\\r\\n\\x80-\\xFF]|[\\xC0-\\xDF][\\x80-\\xBF]|[\\xE0-\\xEF][\\x80-\\xBF][\\x80-\\xBF]|[\\xF0-\\xF7][\\x80-\\xBF][\\x80-\\xBF][\\x80-\\xBF])";
    static const char* kUtf8AnyChar =
        "(?:[^\\x80-\\xFF]|[\\xC0-\\xDF][\\x80-\\xBF]|[\\xE0-\\xEF][\\x80-\\xBF][\\x80-\\xBF]|[\\xF0-\\xF7][\\x80-\\xBF][\\x80-\\xBF][\\x80-\\xBF])";
    static const char* kUtf8MultiByteOnly =
        "[\\xC0-\\xDF][\\x80-\\xBF]|[\\xE0-\\xEF][\\x80-\\xBF][\\x80-\\xBF]|[\\xF0-\\xF7][\\x80-\\xBF][\\x80-\\xBF][\\x80-\\xBF]";

    std::string processed;
    processed.reserve(query.size() * 6);
    for (size_t i = 0; i < query.size(); ++i) {
        unsigned char uc = (unsigned char)query[i];
        char c = query[i];
        if (c == '\\' && i + 1 < query.size()) {
            char next = query[i + 1];
            if (next == 'n') {
                bool isPrecededByCR = (i >= 2 && query[i - 2] == '\\' && query[i - 1] == 'r');
                if (!isPrecededByCR) {
                    processed += "(?:\\r\\n|[\\r\\n])";
                    i++;
                    continue;
                }
            }
            size_t escLen = 1;
            if ((unsigned char)next >= 0x80) {
                escLen = utf8CharLenAt(query, i + 1);
            }
            processed += c;
            processed.append(query, i + 1, escLen);
            i += escLen;
            continue;
        }
        if (c == '[') {
            // Find matching closing ']'
            size_t closeIdx = std::string::npos;
            for (size_t j = i + 1; j < query.size(); ++j) {
                if (query[j] == '\\' && j + 1 < query.size()) {
                    j++;
                    continue;
                }
                if (query[j] == ']' && !(j == i + 1 || (j == i + 2 && query[i + 1] == '^'))) {
                    closeIdx = j;
                    break;
                }
            }
            if (closeIdx == std::string::npos) {
                processed += c;
                continue;
            }
            bool negated = (i + 1 < closeIdx && query[i + 1] == '^');
            size_t contentStart = negated ? (i + 2) : (i + 1);
            bool hasMultiByte = false;
            bool hasRawHexEscape = false;
            for (size_t j = contentStart; j < closeIdx; ++j) {
                if ((unsigned char)query[j] >= 0x80) hasMultiByte = true;
                if (query[j] == '\\' && j + 1 < closeIdx && (query[j + 1] == 'x' || query[j + 1] == 'u')) hasRawHexEscape = true;
            }
            if (!hasMultiByte) {
                if (negated && !hasRawHexEscape) {
                    processed += "(?:[^";
                    if (contentStart < closeIdx && query[contentStart] == ']') {
                        processed += "\\]";
                        processed.append(query, contentStart + 1, closeIdx - (contentStart + 1));
                    } else {
                        processed.append(query, contentStart, closeIdx - contentStart);
                    }
                    processed += "\\x80-\\xFF]|";
                    processed += kUtf8MultiByteOnly;
                    processed += ')';
                } else if (contentStart < closeIdx && query[contentStart] == ']') {
                    processed += (negated ? "[^\\]" : "[\\]");
                    processed.append(query, contentStart + 1, closeIdx - (contentStart + 1) + 1);
                } else {
                    processed.append(query, i, closeIdx - i + 1);
                }
                i = closeIdx;
                continue;
            }
            // Multi-byte UTF-8 character class: decompose into ASCII bracket part + UTF-8 alternatives
            std::string asciiPart;
            std::vector<std::string> mbAlts;
            size_t p = contentStart;
            while (p < closeIdx) {
                if (query[p] == '\\' && p + 1 < closeIdx) {
                    size_t nextLen = utf8CharLenAt(query, p + 1);
                    if (nextLen == 1) {
                        asciiPart.append(query, p, 2);
                    } else {
                        mbAlts.push_back(query.substr(p + 1, nextLen));
                    }
                    p += 1 + nextLen;
                    continue;
                }
                size_t len1 = 1;
                uint32_t cp1 = decodeUtf8CodePointAt(query, p, len1);
                if (p + len1 + 1 < closeIdx && query[p + len1] == '-' && query[p + len1 + 1] != '\\') {
                    size_t len2 = 1;
                    uint32_t cp2 = decodeUtf8CodePointAt(query, p + len1 + 1, len2);
                    if (cp1 < 0x80 && cp2 < 0x80) {
                        if (query[p] == ']') asciiPart += '\\';
                        asciiPart.append(query, p, len1 + 1);
                        if (query[p + len1 + 1] == ']') asciiPart += '\\';
                        asciiPart.append(query, p + len1 + 1, len2);
                    } else {
                        appendCodePointRangeAlternates(cp1, cp2, mbAlts);
                    }
                    p += len1 + 1 + len2;
                    continue;
                }
                if (len1 == 1) {
                    if (query[p] == ']') asciiPart += "\\]";
                    else asciiPart += query[p];
                } else {
                    std::string seq;
                    for (size_t k = 0; k < len1; ++k) appendRegexHexByte(seq, (unsigned char)query[p + k]);
                    mbAlts.push_back(seq);
                }
                p += len1;
            }
            std::string posGroup = "(?:";
            bool firstAlt = true;
            if (!asciiPart.empty()) {
                posGroup += '[';
                if (asciiPart[0] == '^') posGroup += '\\';
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
            processed += kUtf8NonNewlineChar;
            continue;
        }
        if (uc >= 0x80) {
            size_t uLen = utf8CharLenAt(query, i);
            if (uLen > 1 && i + uLen < query.size()) {
                char next = query[i + uLen];
                if (next == '*' || next == '+' || next == '?' || next == '{') {
                    processed += "(?:";
                    processed.append(query, i, uLen);
                    processed += ')';
                    i += uLen - 1;
                    continue;
                }
            }
            processed.append(query, i, uLen);
            i += uLen - 1;
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

inline bool hasUnescapedTrailingDollar(std::string_view query) noexcept {
    if (query.empty() || query.back() != '$') return false;
    size_t bs = 0;
    size_t idx = query.size() - 1;
    while (idx > 0 && query[idx - 1] == '\\') {
        ++bs;
        --idx;
    }
    return (bs & 1u) == 0;
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
                    int d1 = rawFmt[i] - '0';
                    if (i + 1 < rawFmt.size() && std::isdigit((unsigned char)rawFmt[i + 1])) {
                        int grp = d1 * 10 + (rawFmt[i + 1] - '0');
                        i++;
                        if (grp > 0 && grp < 99) grp++;
                        if (grp < 10) fmt += '0';
                        fmt += std::to_string(grp);
                    } else {
                        int grp = d1;
                        if (grp > 0) grp++;
                        fmt += std::to_string(grp);
                    }
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
