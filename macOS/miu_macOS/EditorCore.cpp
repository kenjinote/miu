#include "EditorCore.h"
#include <sys/stat.h>
#include <cerrno>
#include <random>

#if defined(__APPLE__)
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <pwd.h>
#include <CoreFoundation/CoreFoundation.h>
#include <dispatch/dispatch.h>
#elif defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <direct.h>
#endif

#if defined(__AVX2__)
#include <immintrin.h>
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#endif

const std::wstring APP_VERSION = L"v1.0.44";
const std::wstring APP_TITLE = L"miu";

#if defined(__APPLE__)
static std::string CFStringToStdString(CFStringRef cfStr) {
    if (!cfStr) return "";
    CFIndex len = CFStringGetLength(cfStr);
    CFIndex maxSize = CFStringGetMaximumSizeForEncoding(len, kCFStringEncodingUTF8) + 1;
    std::vector<char> buf(maxSize);
    if (CFStringGetCString(cfStr, buf.data(), maxSize, kCFStringEncodingUTF8)) return std::string(buf.data());
    return "";
}

static std::wstring CFStringToStdWString(CFStringRef cfStr) {
    if (!cfStr) return L"";
    CFIndex len = CFStringGetLength(cfStr);
    if (len <= 0) return L"";
    CFIndex maxBytes = CFStringGetMaximumSizeForEncoding(len, kCFStringEncodingUTF32LE);
    std::vector<wchar_t> buf((maxBytes / sizeof(wchar_t)) + 1, 0);
    CFIndex usedBytes = 0;
    CFStringGetBytes(cfStr, CFRangeMake(0, len), kCFStringEncodingUTF32LE, 0, false, (UInt8*)buf.data(), buf.size() * sizeof(wchar_t), &usedBytes);
    return std::wstring(buf.data(), usedBytes / sizeof(wchar_t));
}

std::string WToUTF8(const std::wstring& w) {
    if (w.empty()) return "";
    CFStringRef str = CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8*)w.data(), w.size() * sizeof(wchar_t), kCFStringEncodingUTF32LE, false);
    std::string res = CFStringToStdString(str);
    if (str) CFRelease(str);
    return res;
}

std::wstring UTF8ToW(const std::string& s) {
    if (s.empty()) return L"";
    CFStringRef str = CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8*)s.data(), s.size(), kCFStringEncodingUTF8, false);
    std::wstring res = CFStringToStdWString(str);
    if (str) CFRelease(str);
    return res;
}

std::string Utf16ToUtf8(const char* data, size_t len, bool isBigEndian) {
    if (len < 2) return "";
    size_t offset = 0;
    if ((unsigned char)data[0] == 0xFF && (unsigned char)data[1] == 0xFE) offset = 2;
    else if ((unsigned char)data[0] == 0xFE && (unsigned char)data[1] == 0xFF) offset = 2;
    if (len <= offset) return "";
    CFStringRef str = CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8*)(data + offset), len - offset, isBigEndian ? kCFStringEncodingUTF16BE : kCFStringEncodingUTF16LE, false);
    std::string res = CFStringToStdString(str);
    if (str) CFRelease(str);
    return res;
}

static CFStringEncoding MapCedEncodingToCFEncoding(Encoding enc) {
    switch (enc) {
        case JAPANESE_SHIFT_JIS:
        case JAPANESE_CP932:
        case KDDI_SHIFT_JIS:
        case DOCOMO_SHIFT_JIS:
        case SOFTBANK_SHIFT_JIS:   return kCFStringEncodingDOSJapanese;
        case JAPANESE_EUC_JP:      return kCFStringEncodingEUC_JP;
        case JAPANESE_JIS:
        case KDDI_ISO_2022_JP:
        case SOFTBANK_ISO_2022_JP: return kCFStringEncodingISO_2022_JP;
        case CHINESE_GB:
        case GBK:
        case GB18030:
        case CHINESE_EUC_CN:
        case HZ_GB_2312:
        case ISO_2022_CN:          return kCFStringEncodingGB_18030_2000;
        case CHINESE_BIG5:
        case CHINESE_BIG5_CP950:
        case BIG5_HKSCS:           return kCFStringEncodingBig5;
        case CHINESE_EUC_DEC:
        case CHINESE_CNS:          return kCFStringEncodingEUC_TW;
        case KOREAN_EUC_KR:        return kCFStringEncodingEUC_KR;
        case ISO_2022_KR:          return kCFStringEncodingISO_2022_KR;
        case RUSSIAN_CP1251:       return kCFStringEncodingWindowsCyrillic;
        case RUSSIAN_KOI8_R:
        case RUSSIAN_KOI8_RU:      return kCFStringEncodingKOI8_R;
        case ISO_8859_5:           return kCFStringEncodingISOLatinCyrillic;
        case RUSSIAN_CP866:        return kCFStringEncodingDOSRussian;
        case MSFT_CP1250:          return kCFStringEncodingWindowsLatin2;
        case ISO_8859_2:           return kCFStringEncodingISOLatin2;
        case CZECH_CP852:          return kCFStringEncodingDOSLatin2;
        case MSFT_CP1252:
        case ISO_8859_1:           return kCFStringEncodingWindowsLatin1;
        case ISO_8859_15:          return kCFStringEncodingISOLatin9;
        case MSFT_CP1253:          return kCFStringEncodingWindowsGreek;
        case ISO_8859_7:           return kCFStringEncodingISOLatinGreek;
        case MSFT_CP1254:          return kCFStringEncodingWindowsLatin5;
        case ISO_8859_9:           return kCFStringEncodingISOLatin5;
        case MSFT_CP1255:          return kCFStringEncodingWindowsHebrew;
        case ISO_8859_8:
        case ISO_8859_8_I:
        case HEBREW_VISUAL:        return kCFStringEncodingISOLatinHebrew;
        case MSFT_CP1256:          return kCFStringEncodingWindowsArabic;
        case ISO_8859_6:           return kCFStringEncodingISOLatinArabic;
        case MSFT_CP1257:          return kCFStringEncodingWindowsBalticRim;
        case ISO_8859_4:           return kCFStringEncodingISOLatin4;
        case ISO_8859_13:          return kCFStringEncodingISOLatin7;
        case MSFT_CP874:
        case ISO_8859_11:          return kCFStringEncodingDOSThai;
        case MACINTOSH_ROMAN:      return kCFStringEncodingMacRoman;
        case ASCII_7BIT:
        default:                   return kCFStringEncodingUTF8;
    }
}

static std::string LocalToUtf8(const char* data, size_t len, CFStringEncoding encoding) {
    if (len == 0) return "";
    CFStringRef str = CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8*)data, len, encoding, false);
    if (!str && encoding == kCFStringEncodingDOSJapanese) {
        str = CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8*)data, len, kCFStringEncodingShiftJIS, false);
        if (!str) str = CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8*)data, len, kCFStringEncodingMacJapanese, false);
    }
    if (!str) {
        str = CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8*)data, len, kCFStringEncodingWindowsLatin1, false);
    }
    if (!str) return "";
    std::string res = CFStringToStdString(str);
    CFRelease(str);
    return res;
}

static std::string Utf8ToLocal(const std::string& utf8, CFStringEncoding encoding) {
    if (utf8.empty()) return "";
    CFStringRef str = CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8*)utf8.data(), utf8.size(), kCFStringEncodingUTF8, false);
    if (!str) return "";
    CFIndex len = CFStringGetLength(str);
    CFIndex maxSize = CFStringGetMaximumSizeForEncoding(len, encoding) + 1;
    std::vector<char> buf(maxSize);
    CFIndex usedBufLen = 0;
    CFStringGetBytes(str, CFRangeMake(0, len), encoding, 0, false, (UInt8*)buf.data(), maxSize, &usedBufLen);
    CFRelease(str);
    if (usedBufLen > 0) return std::string(buf.data(), usedBufLen);
    return "";
}
#else
// Cross-platform Windows implementation for automated test suite
std::string WToUTF8(const std::wstring& w) {
    if (w.empty()) return "";
    int sz = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    if (sz <= 0) return "";
    std::string res(sz, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), res.data(), sz, nullptr, nullptr);
    return res;
}

std::wstring UTF8ToW(const std::string& s) {
    if (s.empty()) return L"";
    int sz = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    if (sz <= 0) return L"";
    std::wstring res(sz, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), res.data(), sz);
    return res;
}

std::string Utf16ToUtf8(const char* data, size_t len, bool isBigEndian) {
    if (len < 2) return "";
    size_t offset = 0;
    if ((unsigned char)data[0] == 0xFF && (unsigned char)data[1] == 0xFE) offset = 2;
    else if ((unsigned char)data[0] == 0xFE && (unsigned char)data[1] == 0xFF) offset = 2;
    if (len <= offset) return "";
    size_t wlen = (len - offset) / 2;
    std::wstring w(wlen, L'\0');
    const unsigned char* p = (const unsigned char*)(data + offset);
    for (size_t i = 0; i < wlen; ++i) {
        if (isBigEndian) w[i] = (wchar_t)((p[i * 2] << 8) | p[i * 2 + 1]);
        else w[i] = (wchar_t)(p[i * 2] | (p[i * 2 + 1] << 8));
    }
    return WToUTF8(w);
}

static uint32_t MapCedEncodingToWinCodePage(Encoding enc) {
    switch (enc) {
        case JAPANESE_SHIFT_JIS:
        case JAPANESE_CP932:
        case KDDI_SHIFT_JIS:
        case DOCOMO_SHIFT_JIS:
        case SOFTBANK_SHIFT_JIS:   return 932;
        case JAPANESE_EUC_JP:      return 20932;
        case JAPANESE_JIS:
        case KDDI_ISO_2022_JP:
        case SOFTBANK_ISO_2022_JP: return 50220;
        case CHINESE_GB:
        case GBK:
        case HZ_GB_2312:
        case ISO_2022_CN:          return 936;
        case GB18030:              return 54936;
        case CHINESE_BIG5:
        case CHINESE_BIG5_CP950:
        case BIG5_HKSCS:           return 950;
        case KOREAN_EUC_KR:
        case ISO_2022_KR:          return 949;
        case RUSSIAN_CP1251:       return 1251;
        case RUSSIAN_KOI8_R:       return 20866;
        case RUSSIAN_KOI8_RU:      return 21866;
        case ISO_8859_5:           return 28595;
        case RUSSIAN_CP866:        return 866;
        case MSFT_CP1250:          return 1250;
        case ISO_8859_2:           return 28592;
        case CZECH_CP852:          return 852;
        case MSFT_CP1252:
        case ISO_8859_1:           return 1252;
        case ISO_8859_15:          return 28605;
        case MACINTOSH_ROMAN:      return 10000;
        case MSFT_CP1253:          return 1253;
        case ISO_8859_7:           return 28597;
        case MSFT_CP1254:          return 1254;
        case ISO_8859_9:           return 28599;
        case MSFT_CP1255:          return 1255;
        case ISO_8859_8:
        case ISO_8859_8_I:
        case HEBREW_VISUAL:        return 28598;
        case MSFT_CP1256:          return 1256;
        case ISO_8859_6:           return 28596;
        case MSFT_CP1257:          return 1257;
        case ISO_8859_4:           return 28594;
        case ISO_8859_13:          return 28603;
        case MSFT_CP874:
        case ISO_8859_11:          return 874;
        default:                   return CP_UTF8;
    }
}

static std::string LocalToUtf8(const char* data, size_t len, uint32_t codePage) {
    if (len == 0) return "";
    int wsz = MultiByteToWideChar(codePage, 0, data, (int)len, nullptr, 0);
    if (wsz <= 0) return std::string(data, len);
    std::wstring w(wsz, L'\0');
    MultiByteToWideChar(codePage, 0, data, (int)len, w.data(), wsz);
    return WToUTF8(w);
}

static std::string Utf8ToLocal(const std::string& utf8, uint32_t codePage) {
    if (utf8.empty()) return "";
    std::wstring w = UTF8ToW(utf8);
    int sz = WideCharToMultiByte(codePage, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    if (sz <= 0) return utf8;
    std::string res(sz, '\0');
    WideCharToMultiByte(codePage, 0, w.data(), (int)w.size(), res.data(), sz, nullptr, nullptr);
    return res;
}
#endif

static std::string Utf8ToUtf16Bytes(const std::string& utf8, bool isBigEndian) {
    std::string out;
    out.reserve(2 + utf8.size() * 2);
    if (isBigEndian) {
        out.push_back((char)0xFE);
        out.push_back((char)0xFF);
    } else {
        out.push_back((char)0xFF);
        out.push_back((char)0xFE);
    }
    auto appendU16 = [&](uint16_t u16) {
        if (isBigEndian) {
            out.push_back((char)((u16 >> 8) & 0xFF));
            out.push_back((char)(u16 & 0xFF));
        } else {
            out.push_back((char)(u16 & 0xFF));
            out.push_back((char)((u16 >> 8) & 0xFF));
        }
    };
    size_t i = 0;
    size_t n = utf8.size();
    while (i < n) {
        unsigned char b0 = (unsigned char)utf8[i];
        uint32_t cp = 0;
        if (b0 < 0x80) {
            cp = b0;
            i += 1;
        } else if ((b0 & 0xE0) == 0xC0 && i + 1 < n) {
            cp = ((uint32_t)(b0 & 0x1F) << 6) | ((uint32_t)((unsigned char)utf8[i + 1] & 0x3F));
            i += 2;
        } else if ((b0 & 0xF0) == 0xE0 && i + 2 < n) {
            cp = ((uint32_t)(b0 & 0x0F) << 12) |
                 ((uint32_t)((unsigned char)utf8[i + 1] & 0x3F) << 6) |
                 ((uint32_t)((unsigned char)utf8[i + 2] & 0x3F));
            i += 3;
        } else if ((b0 & 0xF8) == 0xF0 && i + 3 < n) {
            cp = ((uint32_t)(b0 & 0x07) << 18) |
                 ((uint32_t)((unsigned char)utf8[i + 1] & 0x3F) << 12) |
                 ((uint32_t)((unsigned char)utf8[i + 2] & 0x3F) << 6) |
                 ((uint32_t)((unsigned char)utf8[i + 3] & 0x3F));
            i += 4;
        } else {
            cp = 0xFFFD;
            i += 1;
        }
        if (cp <= 0xFFFF) {
            appendU16((uint16_t)cp);
        } else if (cp <= 0x10FFFF) {
            uint32_t v = cp - 0x10000;
            appendU16((uint16_t)(0xD800 + (v >> 10)));
            appendU16((uint16_t)(0xDC00 + (v & 0x3FF)));
        }
    }
    return out;
}

bool MappedFile::open(const char* path) {
#if defined(__APPLE__)
    fd = ::open(path, O_RDONLY);
    if (fd == -1) return false;
    struct stat sb;
    if (fstat(fd, &sb) == -1) { ::close(fd); fd = -1; return false; }
    size = sb.st_size;
    if (size == 0) { ptr = nullptr; return true; }
    ptr = (char*)mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    return (ptr != MAP_FAILED);
#else
    std::ifstream ifs(UTF8ToW(path), std::ios::binary | std::ios::ate);
    if (!ifs) return false;
    size = (size_t)ifs.tellg();
    ifs.seekg(0, std::ios::beg);
    winFallbackBuf.resize(size);
    if (size > 0) ifs.read(winFallbackBuf.data(), size);
    ptr = size > 0 ? winFallbackBuf.data() : nullptr;
    return true;
#endif
}

void MappedFile::close() {
#if defined(__APPLE__)
    if (ptr && ptr != MAP_FAILED) munmap(ptr, size);
    if (fd != -1) ::close(fd);
    ptr = nullptr;
    fd = -1;
#else
    winFallbackBuf.clear();
    ptr = nullptr;
    size = 0;
#endif
}

bool IsValidUtf8(const char* buf, size_t len) {
    if (len == 0) return true;
    size_t check_len = (len > 65536) ? 65536 : len;
    size_t i = 0;
    while (i < check_len) {
        unsigned char c = (unsigned char)buf[i];
        if (c <= 0x7F) {
            i++;
        } else if (c >= 0xC2 && c <= 0xDF) {
            if (i + 1 >= check_len) break;
            if (((unsigned char)buf[i + 1] & 0xC0) != 0x80) return false;
            i += 2;
        } else if (c >= 0xE0 && c <= 0xEF) {
            if (i + 2 >= check_len) break;
            unsigned char b1 = (unsigned char)buf[i + 1];
            unsigned char b2 = (unsigned char)buf[i + 2];
            if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80) return false;
            if (c == 0xE0 && b1 < 0xA0) return false;
            if (c == 0xED && b1 >= 0xA0) return false;
            i += 3;
        } else if (c >= 0xF0 && c <= 0xF4) {
            if (i + 3 >= check_len) break;
            unsigned char b1 = (unsigned char)buf[i + 1];
            unsigned char b2 = (unsigned char)buf[i + 2];
            unsigned char b3 = (unsigned char)buf[i + 3];
            if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80 || (b3 & 0xC0) != 0x80) return false;
            if (c == 0xF0 && b1 < 0x90) return false;
            if (c == 0xF4 && b1 >= 0x90) return false;
            i += 4;
        } else {
            return false;
        }
    }
    return true;
}

Encoding DetectEncoding(const char* buf, size_t len) {
    int bytes_consumed = 0;
    bool is_reliable = false;
    size_t ced_len = (len > 65536) ? 65536 : len;
    return CompactEncDet::DetectEncoding(
        buf, static_cast<int>(ced_len),
        nullptr, nullptr, nullptr,
        UNKNOWN_ENCODING,
        UNKNOWN_LANGUAGE,
        CompactEncDet::WEB_CORPUS,
        false,
        &bytes_consumed,
        &is_reliable
    );
}

DetectResult DetectEncodingEx(const char* buf, size_t len) {
#if defined(__APPLE__)
    DetectResult res = { ENC_UTF8_NOBOM, kCFStringEncodingUTF8 };
#else
    DetectResult res = { ENC_UTF8_NOBOM, 65001 };
#endif
    if (len >= 3 && (unsigned char)buf[0] == 0xEF && (unsigned char)buf[1] == 0xBB && (unsigned char)buf[2] == 0xBF) {
        res.type = ENC_UTF8_BOM;
        return res;
    }
    if (len >= 2) {
        if ((unsigned char)buf[0] == 0xFF && (unsigned char)buf[1] == 0xFE) { res.type = ENC_UTF16LE; return res; }
        if ((unsigned char)buf[0] == 0xFE && (unsigned char)buf[1] == 0xFF) { res.type = ENC_UTF16BE; return res; }
    }
    if (IsValidUtf8(buf, len)) {
        res.type = ENC_UTF8_NOBOM;
        return res;
    }
    Encoding ced_enc = DetectEncoding(buf, len);
    res.type = ENC_LOCAL;
#if defined(__APPLE__)
    res.codePage = MapCedEncodingToCFEncoding(ced_enc);
#else
    res.codePage = MapCedEncodingToWinCodePage(ced_enc);
#endif
    return res;
}

std::string ConvertCase(const std::string& s, bool toUpper) {
    std::wstring w = UTF8ToW(s);
    for (auto& c : w) {
        if (toUpper) {
            if (c >= 'a' && c <= 'z') c -= 32;
            else if (c >= 0xFF41 && c <= 0xFF5A) c -= 32;
        } else {
            if (c >= 'A' && c <= 'Z') c += 32;
            else if (c >= 0xFF21 && c <= 0xFF3A) c += 32;
        }
    }
    return WToUTF8(w);
}

void Editor::detectNewlineStyle(const char* buf, size_t len) {
    size_t checkLen = (len > 4096) ? 4096 : len;
    for (size_t i = 0; i < checkLen; ++i) {
        if (buf[i] == '\r') {
            if (i + 1 < checkLen && buf[i + 1] == '\n') {
                newlineStr = "\r\n";
                return;
            }
            newlineStr = "\r";
            return;
        } else if (buf[i] == '\n') {
            newlineStr = "\n";
            return;
        }
    }
    newlineStr = "\n";
}

void Editor::rollbackPadding() {
    if (pendingPadding.empty()) return;
    for (auto it = pendingPadding.rbegin(); it != pendingPadding.rend(); ++it) {
        pt.erase(it->pos, it->len);
        for (auto& c : cursors) {
            if (c.head > it->pos) c.head = (c.head >= it->pos + it->len) ? (c.head - it->len) : it->pos;
            if (c.anchor > it->pos) c.anchor = (c.anchor >= it->pos + it->len) ? (c.anchor - it->len) : it->pos;
        }
    }
    pendingPadding.clear();
    rebuildLineStarts();
}

void Editor::commitPadding(EditBatch& batch) {
    if (pendingPadding.empty()) return;
    std::vector<EditOp> padOps;
    padOps.reserve(pendingPadding.size());
    for (const auto& p : pendingPadding) {
        padOps.push_back({ EditOp::Insert, p.pos, std::string(p.len, ' ') });
    }
    batch.ops.insert(batch.ops.begin(), padOps.begin(), padOps.end());
    pendingPadding.clear();
}

void Editor::updateRectSelection() {
    rollbackPadding();
    cursors.clear();
    int totalLines = (int)lineStarts.size();
    if (totalLines == 0) return;
    int rStart = std::clamp(std::min(rectAnchorLine, rectHeadLine), 0, totalLines - 1);
    int rEnd = std::clamp(std::max(rectAnchorLine, rectHeadLine), 0, totalLines - 1);
    bool isZeroWidth = (std::abs(rectAnchorX - rectHeadX) < 1.0f);

    for (int i = rStart; i <= rEnd; ++i) {
        size_t lineStart = lineStarts[i];
        size_t lineEnd = (i + 1 < (int)lineStarts.size()) ? lineStarts[i + 1] : pt.length();
        if (lineEnd > lineStart && pt.charAt(lineEnd - 1) == '\n') lineEnd--;
        if (lineEnd > lineStart && pt.charAt(lineEnd - 1) == '\r') lineEnd--;

        float lineEndX = getXInLine(i, lineEnd);
        if (isZeroWidth) {
            size_t pos = getPosFromLineAndX(i, rectHeadX);
            bool virt = (rectHeadX > lineEndX + charWidth * 0.5f);
            if (virt) pos = lineEnd;
            cursors.push_back({ pos, pos, rectHeadX, rectAnchorX, virt, false });
        } else {
            float maxReqX = std::max(rectAnchorX, rectHeadX);
            if (maxReqX > lineEndX + (charWidth * 0.5f)) {
                int spacesNeeded = (int)((maxReqX - lineEndX) / charWidth + 0.5f);
                if (spacesNeeded > 0) {
                    std::string pad(spacesNeeded, ' ');
                    pt.insert(lineEnd, pad);
                    pendingPadding.push_back({ lineEnd, (size_t)spacesNeeded });
                    for (auto& c : cursors) {
                        if (c.head >= lineEnd) c.head += spacesNeeded;
                        if (c.anchor >= lineEnd) c.anchor += spacesNeeded;
                    }
                    rebuildLineStarts();
                }
            }
            size_t pAnchor = getPosFromLineAndX(i, rectAnchorX);
            size_t pHead = getPosFromLineAndX(i, rectHeadX);
            cursors.push_back({ pHead, pAnchor, rectHeadX, rectAnchorX, false, false });
        }
    }
}

void Editor::moveRectSelection(int dLine, int dCol) {
    if (cursors.empty()) return;
    if (!isRectSelecting) {
        isRectSelecting = true;
        const Cursor& c = cursors.back();
        rectAnchorLine = getLineIdx(c.anchor);
        rectHeadLine = getLineIdx(c.head);
        rectAnchorX = getXFromPos(c.anchor);
        rectHeadX = c.isVirtual ? c.desiredX : getXFromPos(c.head);
    }
    int totalLines = (int)lineStarts.size();
    rectHeadLine = std::clamp(rectHeadLine + dLine, 0, std::max(0, totalLines - 1));
    if (dCol != 0) {
        rectHeadX = std::max(0.0f, rectHeadX + (float)dCol * charWidth);
    }
    updateRectSelection();
    ensureCaretVisible();
    if (cbNeedsDisplay) cbNeedsDisplay();
}

void Editor::addCursor(bool up) {
    if (cursors.empty()) return;
    rollbackPadding();
    isRectSelecting = false;
    auto sorted = cursors;
    std::sort(sorted.begin(), sorted.end(), [](const Cursor& a, const Cursor& b) { return a.head < b.head; });
    const Cursor& ref = up ? sorted.front() : sorted.back();
    int curLine = getLineIdx(ref.head);
    int targetLine = up ? (curLine - 1) : (curLine + 1);
    if (targetLine < 0 || targetLine >= (int)lineStarts.size()) return;
    float targetX = ref.desiredX;
    size_t newPos = getPosFromLineAndX(targetLine, targetX);
    cursors.push_back({ newPos, newPos, targetX, targetX, false, false });
    mergeCursors();
    ensureCaretVisible();
    if (cbNeedsDisplay) cbNeedsDisplay();
}

void Editor::mergeCursors() {
    if (cursors.size() <= 1) return;
    std::sort(cursors.begin(), cursors.end(), [](const Cursor& a, const Cursor& b) {
        if (a.start() != b.start()) return a.start() < b.start();
        return a.end() < b.end();
    });
    std::vector<Cursor> merged;
    merged.reserve(cursors.size());
    for (const auto& c : cursors) {
        if (merged.empty()) {
            merged.push_back(c);
            continue;
        }
        Cursor& prev = merged.back();
        bool overlap = false;
        if (!prev.hasSelection() && !c.hasSelection()) {
            overlap = (prev.head == c.head && ((!prev.isVirtual && !c.isVirtual) || std::abs(prev.desiredX - c.desiredX) < 0.5f));
        } else if (prev.hasSelection() && c.hasSelection()) {
            overlap = (c.start() < prev.end());
        } else {
            overlap = (c.start() >= prev.start() && c.end() <= prev.end());
        }
        if (overlap) {
            size_t newStart = std::min(prev.start(), c.start());
            size_t newEnd = std::max(prev.end(), c.end());
            if (c.head >= c.anchor) {
                prev.anchor = newStart;
                prev.head = newEnd;
            } else {
                prev.anchor = newEnd;
                prev.head = newStart;
            }
            prev.desiredX = getXFromPos(prev.head);
            prev.originalAnchorX = getXFromPos(prev.anchor);
            prev.isVirtual = false;
        } else {
            merged.push_back(c);
        }
    }
    cursors = std::move(merged);
}

void Editor::selectWordAt(size_t pos, bool addSelection) {
    rollbackPadding();
    isRectSelecting = false;
    size_t s = pos, e = pos;
    getWordBoundaries(pos, s, e);
    Cursor nc = { e, s, getXFromPos(e), getXFromPos(s), false, false };
    if (!addSelection) cursors.clear();
    cursors.push_back(nc);
    mergeCursors();
    if (cbNeedsDisplay) cbNeedsDisplay();
}

void Editor::selectLineAt(size_t pos, bool addSelection) {
    rollbackPadding();
    isRectSelecting = false;
    int li = getLineIdx(pos);
    size_t s = lineStarts[li];
    size_t e = (li + 1 < (int)lineStarts.size()) ? lineStarts[li + 1] : pt.length();
    Cursor nc = { e, s, getXFromPos(e), getXFromPos(s), false, false };
    if (!addSelection) cursors.clear();
    cursors.push_back(nc);
    mergeCursors();
    if (cbNeedsDisplay) cbNeedsDisplay();
}

void Editor::selectLinesRange(int anchorLine, int headLine, bool addSelection) {
    rollbackPadding();
    isRectSelecting = false;
    int total = (int)lineStarts.size();
    if (total == 0) return;
    anchorLine = std::clamp(anchorLine, 0, total - 1);
    headLine = std::clamp(headLine, 0, total - 1);
    size_t s, e;
    if (headLine >= anchorLine) {
        s = lineStarts[anchorLine];
        e = (headLine + 1 < total) ? lineStarts[headLine + 1] : pt.length();
    } else {
        s = (anchorLine + 1 < total) ? lineStarts[anchorLine + 1] : pt.length();
        e = lineStarts[headLine];
    }
    Cursor nc = { e, s, getXFromPos(e), getXFromPos(s), false, false };
    if (!addSelection) cursors.clear();
    cursors.push_back(nc);
    mergeCursors();
    ensureCaretVisible();
    if (cbNeedsDisplay) cbNeedsDisplay();
}

bool Editor::isPointInSelection(size_t pos) const {
    for (const auto& c : cursors) {
        if (c.hasSelection() && pos >= c.start() && pos < c.end()) return true;
    }
    return false;
}

void Editor::performDragMove(size_t targetPos) {
    if (cursors.empty()) return;
    rollbackPadding();
    isRectSelecting = false;
    if (isPointInSelection(targetPos)) return;

    auto sorted = cursors;
    std::sort(sorted.begin(), sorted.end(), [](const Cursor& a, const Cursor& b) { return a.start() < b.start(); });
    std::string combined;
    for (size_t i = 0; i < sorted.size(); ++i) {
        if (sorted[i].hasSelection()) {
            combined += pt.getRange(sorted[i].start(), sorted[i].end() - sorted[i].start());
        }
    }
    if (combined.empty()) return;

    EditBatch batch;
    batch.beforeCursors = cursors;
    size_t adjustedTarget = targetPos;

    for (auto it = sorted.rbegin(); it != sorted.rend(); ++it) {
        if (!it->hasSelection()) continue;
        size_t st = it->start();
        size_t len = it->end() - st;
        std::string del = pt.getRange(st, len);
        pt.erase(st, len);
        batch.ops.push_back({ EditOp::Erase, st, del });
        if (adjustedTarget > st) {
            adjustedTarget -= std::min(adjustedTarget - st, len);
        }
    }

    pt.insert(adjustedTarget, combined);
    batch.ops.push_back({ EditOp::Insert, adjustedTarget, combined });
    size_t newEnd = adjustedTarget + combined.size();
    rebuildLineStarts();
    cursors.clear();
    cursors.push_back({ newEnd, adjustedTarget, getXFromPos(newEnd), getXFromPos(adjustedTarget), false, false });
    batch.afterCursors = cursors;
    undo.push(batch);
    applyBatchToEmmetTabstops(batch);
    ensureCaretVisible();
    updateDirtyFlag();
    if (cbUpdatePreview && showPreview) cbUpdatePreview();
    if (cbNeedsDisplay) cbNeedsDisplay();
}

void Editor::insertAtCursorsWithPadding(const std::string& text) {
    if (cursors.empty()) return;
    EditBatch batch;
    batch.beforeCursors = cursors;
    commitPadding(batch);
    auto sortedIndices = std::vector<size_t>(cursors.size());
    std::iota(sortedIndices.begin(), sortedIndices.end(), 0);
    std::sort(sortedIndices.begin(), sortedIndices.end(), [&](size_t a, size_t b) { return cursors[a].start() > cursors[b].start(); });
    for (size_t idx : sortedIndices) {
        Cursor& c = cursors[idx];
        int li = getLineIdx(c.head);
        size_t lineStart = lineStarts[li];
        size_t lineEnd = (li + 1 < (int)lineStarts.size()) ? lineStarts[li + 1] : pt.length();
        if (lineEnd > lineStart && pt.charAt(lineEnd - 1) == '\n') lineEnd--;
        if (lineEnd > lineStart && pt.charAt(lineEnd - 1) == '\r') lineEnd--;
        if (c.hasSelection()) {
            size_t st = c.start(), len = c.end() - st;
            batch.ops.push_back({ EditOp::Erase, st, pt.getRange(st, len) });
            pt.erase(st, len);
            for (auto& oc : cursors) { if (oc.head > st) oc.head -= len; if (oc.anchor > st) oc.anchor -= len; }
            c.head = c.anchor = st;
            c.desiredX = getXInLine(li, st);
            lineEnd -= len;
        }
        float currentLineEndX = getXInLine(li, lineEnd);
        size_t targetPos = c.head;
        if (c.desiredX > currentLineEndX + (charWidth * 0.5f)) {
            int spacesNeeded = (int)((c.desiredX - currentLineEndX) / charWidth + 0.5f);
            if (spacesNeeded > 0) {
                std::string padding(spacesNeeded, ' ');
                pt.insert(lineEnd, padding);
                batch.ops.push_back({ EditOp::Insert, lineEnd, padding });
                targetPos = lineEnd + padding.size();
                for (auto& oc : cursors) { if (oc.head >= lineEnd) oc.head += padding.size(); if (oc.anchor >= lineEnd) oc.anchor += padding.size(); }
            }
        }
        pt.insert(targetPos, text);
        batch.ops.push_back({ EditOp::Insert, targetPos, text });
        for (auto& oc : cursors) { if (oc.head >= targetPos) oc.head += text.size(); if (oc.anchor >= targetPos) oc.anchor += text.size(); }
        c.desiredX = getXInLine(li, c.head);
        c.originalAnchorX = c.desiredX;
        c.isVirtual = false;
        c.affinity = false;
    }
    isRectSelecting = false;
    batch.afterCursors = cursors;
    undo.push(batch);
    applyBatchToEmmetTabstops(batch);
    rebuildLineStarts();
    ensureCaretVisible();
    updateDirtyFlag();
    if (cbUpdatePreview && showPreview) cbUpdatePreview();
}

void Editor::insertNewlineWithAutoIndent() {
    if (cursors.empty()) return;
    rollbackPadding();
    isRectSelecting = false;
    EditBatch batch;
    batch.beforeCursors = cursors;
    std::vector<size_t> indices(cursors.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
        return cursors[a].start() > cursors[b].start();
    });
    for (size_t idx : indices) {
        Cursor& c = cursors[idx];
        size_t start = c.start();
        if (c.hasSelection()) {
            size_t len = c.end() - start;
            std::string deleted = pt.getRange(start, len);
            pt.erase(start, len);
            batch.ops.push_back({ EditOp::Erase, start, deleted });
            for (auto& o : cursors) {
                if (o.head > start) o.head -= len;
                if (o.anchor > start) o.anchor -= len;
            }
            c.head = start;
            c.anchor = start;
        }
        int lineIdx = getLineIdx(start);
        size_t lineStart = lineStarts[lineIdx];
        std::string indentStr = "";
        size_t p = lineStart;
        size_t maxLen = pt.length();
        while (p < maxLen && p < start) {
            char ch = pt.charAt(p);
            if (ch == ' ' || ch == '\t') {
                indentStr += ch;
            } else {
                break;
            }
            p++;
        }

        // Check for smart bracket pair {|}, (|), [|] or HTML tag pair <tag>|</tag>
        char prevCh = (start > lineStart) ? pt.charAt(start - 1) : '\0';
        char nextCh = (start < pt.length()) ? pt.charAt(start) : '\0';
        bool isBracketPair = ((prevCh == '{' && nextCh == '}') ||
                              (prevCh == '(' && nextCh == ')') ||
                              (prevCh == '[' && nextCh == ']'));
        bool isTagPair = false;
        if (!isBracketPair && prevCh == '>' && nextCh == '<' && start + 2 < pt.length() && pt.charAt(start + 1) == '/') {
            // Check that preceding '>' is not a self-closing '/>'
            if (start < 2 || pt.charAt(start - 2) != '/') {
                isTagPair = true;
            }
        }

        if (isBracketPair || isTagPair) {
            std::string innerIndent = indentStr + "    ";
            std::string textToInsert = newlineStr + innerIndent + newlineStr + indentStr;
            size_t caretOffsetInInsert = newlineStr.size() + innerIndent.size();
            pt.insert(start, textToInsert);
            batch.ops.push_back({ EditOp::Insert, start, textToInsert });
            size_t insLen = textToInsert.size();
            for (auto& o : cursors) {
                if (&o == &c) {
                    o.head = start + caretOffsetInInsert;
                    o.anchor = o.head;
                } else {
                    if (o.head >= start) o.head += insLen;
                    if (o.anchor >= start) o.anchor += insLen;
                }
            }
        } else {
            std::string extraIndent = "";
            if (prevCh == '{' || prevCh == '(' || prevCh == '[' || prevCh == ':') {
                extraIndent = "    ";
            }
            std::string textToInsert = newlineStr + indentStr + extraIndent;
            pt.insert(start, textToInsert);
            batch.ops.push_back({ EditOp::Insert, start, textToInsert });
            size_t insLen = textToInsert.size();
            for (auto& o : cursors) {
                if (o.head >= start) o.head += insLen;
                if (o.anchor >= start) o.anchor += insLen;
            }
        }
    }
    rebuildLineStarts();
    for (auto& o : cursors) {
        o.desiredX = getXFromPos(o.head);
        o.originalAnchorX = o.desiredX;
        o.isVirtual = false;
        o.affinity = false;
    }
    batch.afterCursors = cursors;
    undo.push(batch);
    applyBatchToEmmetTabstops(batch);
    ensureCaretVisible();
    updateDirtyFlag();
    if (cbUpdatePreview && showPreview) cbUpdatePreview();
}

void Editor::insertRectangularBlock(const std::string& text) {
    if (cursors.empty()) return;
    rollbackPadding();
    isRectSelecting = false;
    std::vector<std::string> lines;
    std::stringstream ss(text);
    std::string line;
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }
    if (lines.empty()) return;
    EditBatch batch;
    batch.beforeCursors = cursors;
    auto sortedCursors = cursors;
    std::sort(sortedCursors.begin(), sortedCursors.end(), [](const Cursor& a, const Cursor& b) { return a.start() < b.start(); });
    size_t basePos = sortedCursors[0].head;
    float baseX = sortedCursors[0].desiredX;
    int startLineIdx = getLineIdx(basePos);
    int requiredTotalLines = startLineIdx + (int)lines.size();
    if (requiredTotalLines > (int)lineStarts.size()) {
        size_t eofPos = pt.length();
        std::string newLines = "";
        for (int k = 0; k < (requiredTotalLines - (int)lineStarts.size()); ++k) newLines += newlineStr;
        pt.insert(eofPos, newLines);
        batch.ops.push_back({ EditOp::Insert, eofPos, newLines });
        rebuildLineStarts();
    }
    for (int i = (int)lines.size() - 1; i >= 0; --i) {
        int targetLineIdx = startLineIdx + i;
        std::string content = lines[i];
        size_t lineStart = lineStarts[targetLineIdx];
        size_t lineEnd = (targetLineIdx + 1 < (int)lineStarts.size()) ? lineStarts[targetLineIdx + 1] : pt.length();
        if (lineEnd > lineStart && pt.charAt(lineEnd - 1) == '\n') lineEnd--;
        if (lineEnd > lineStart && pt.charAt(lineEnd - 1) == '\r') lineEnd--;
        float currentLineEndX = getXInLine(targetLineIdx, lineEnd);
        size_t insertPos = lineEnd;
        if (baseX > currentLineEndX + 1.0f) {
            int spacesNeeded = (int)((baseX - currentLineEndX) / charWidth + 0.5f);
            if (spacesNeeded > 0) {
                std::string padding(spacesNeeded, ' ');
                pt.insert(lineEnd, padding);
                batch.ops.push_back({ EditOp::Insert, lineEnd, padding });
                insertPos = lineEnd + padding.size();
                rebuildLineStarts();
            }
        } else {
            insertPos = getPosFromLineAndX(targetLineIdx, baseX);
        }
        pt.insert(insertPos, content);
        batch.ops.push_back({ EditOp::Insert, insertPos, content });
        rebuildLineStarts();
    }
    cursors.clear();
    for (int i = 0; i < (int)lines.size(); ++i) {
        int li = startLineIdx + i;
        float finalX = baseX + (float)UTF8ToW(lines[i]).length() * charWidth;
        size_t p = getPosFromLineAndX(li, finalX);
        cursors.push_back({ p, p, finalX, finalX, false, false });
    }
    batch.afterCursors = cursors;
    undo.push(batch);
    rebuildLineStarts();
    ensureCaretVisible();
    updateDirtyFlag();
    if (cbUpdatePreview && showPreview) cbUpdatePreview();
}

std::vector<int> Editor::getUniqueLineIndices() {
    std::vector<int> lines;
    for (const auto& c : cursors) {
        if (!c.hasSelection()) {
            lines.push_back(getLineIdx(c.head));
        } else {
            size_t start = c.start(), end = c.end();
            int lStart = getLineIdx(start), lEnd = getLineIdx(end);
            if (lEnd > lStart) {
                size_t lineStartPos = lineStarts[lEnd];
                if (end == lineStartPos) lEnd--;
            }
            for (int i = lStart; i <= lEnd; ++i) lines.push_back(i);
        }
    }
    std::sort(lines.begin(), lines.end());
    lines.erase(std::unique(lines.begin(), lines.end()), lines.end());
    return lines;
}

void Editor::deleteLine() {
    rollbackPadding();
    isRectSelecting = false;
    std::vector<int> lines = getUniqueLineIndices();
    if (lines.empty()) return;
    EditBatch batch;
    batch.beforeCursors = cursors;
    std::vector<std::pair<int, int>> groups;
    int gStart0 = lines[0], prev0 = lines[0];
    for (size_t i = 1; i < lines.size(); ++i) {
        if (lines[i] != prev0 + 1) {
            groups.push_back({ gStart0, prev0 });
            gStart0 = lines[i];
        }
        prev0 = lines[i];
    }
    groups.push_back({ gStart0, prev0 });

    for (int i = (int)groups.size() - 1; i >= 0; --i) {
        int gStart = groups[i].first, gEnd = groups[i].second;
        size_t start = lineStarts[gStart];
        size_t end = (gEnd + 1 < (int)lineStarts.size()) ? lineStarts[gEnd + 1] : pt.length();
        if (end == pt.length() && start > 0) {
            char prev = pt.charAt(start - 1);
            if (prev == '\n') {
                start--;
                if (start > 0 && pt.charAt(start - 1) == '\r') start--;
            } else if (prev == '\r') {
                start--;
            }
        }
        size_t len = end - start;
        if (len > 0) {
            batch.ops.push_back({ EditOp::Erase, start, pt.getRange(start, len) });
            pt.erase(start, len);
            for (auto& c : cursors) {
                if (c.head > start) c.head = (c.head >= start + len) ? c.head - len : start;
                if (c.anchor > start) c.anchor = (c.anchor >= start + len) ? c.anchor - len : start;
            }
        }
    }
    rebuildLineStarts();
    for (auto& c : cursors) {
        c.anchor = c.head;
        c.desiredX = getXFromPos(c.head);
        c.isVirtual = false;
        c.affinity = false;
    }
    mergeCursors();
    batch.afterCursors = cursors;
    undo.push(batch);
    ensureCaretVisible();
    updateDirtyFlag();
    if (cbUpdatePreview && showPreview) cbUpdatePreview();
}

void Editor::moveLines(bool up) {
    rollbackPadding();
    isRectSelecting = false;
    std::vector<int> lines = getUniqueLineIndices();
    if (lines.empty()) return;
    if (up && lines.front() == 0) return;
    if (!up && lines.back() >= (int)lineStarts.size() - 1) return;
    EditBatch batch;
    batch.beforeCursors = cursors;
    std::vector<std::pair<int, int>> groups;
    int start = lines[0], prev = lines[0];
    for (size_t i = 1; i < lines.size(); ++i) {
        if (lines[i] != prev + 1) { groups.push_back({ start, prev }); start = lines[i]; }
        prev = lines[i];
    }
    groups.push_back({ start, prev });
    if (up) {
        for (const auto& g : groups) {
            int gStart = g.first, gEnd = g.second, targetLine = gStart - 1;
            size_t posTarget = lineStarts[targetLine], posStart = lineStarts[gStart];
            size_t posEnd = (gEnd + 1 < (int)lineStarts.size()) ? lineStarts[gEnd + 1] : pt.length();
            bool isEOF = (posEnd == pt.length());
            std::string movingText = pt.getRange(posStart, posEnd - posStart);
            size_t moveLen = movingText.length();
            size_t prevLineLen = posStart - posTarget;
            bool movingIsLastNoNewline = (isEOF && (moveLen == 0 || (movingText.back() != '\n' && movingText.back() != '\r')));
            pt.erase(posStart, moveLen);
            batch.ops.push_back({ EditOp::Erase, posStart, movingText });
            if (movingIsLastNoNewline) {
                size_t nlLen = 0;
                if (posStart > 0 && pt.charAt(posStart - 1) == '\n') {
                    nlLen = 1;
                    if (posStart > 1 && pt.charAt(posStart - 2) == '\r') nlLen = 2;
                } else if (posStart > 0 && pt.charAt(posStart - 1) == '\r') {
                    nlLen = 1;
                }
                if (nlLen > 0) {
                    std::string nlStr = pt.getRange(posStart - nlLen, nlLen);
                    pt.erase(posStart - nlLen, nlLen);
                    batch.ops.push_back({ EditOp::Erase, posStart - nlLen, nlStr });
                    movingText += nlStr;
                    moveLen += nlLen;
                }
            }
            pt.insert(posTarget, movingText);
            batch.ops.push_back({ EditOp::Insert, posTarget, movingText });
            for (auto& c : cursors) {
                bool headInA = (c.head >= posStart && c.head < posEnd);
                if (c.head == posEnd && c.hasSelection() && c.end() == c.head) headInA = true;
                if (isEOF && c.head == posEnd) headInA = true;
                bool anchorInA = (c.anchor >= posStart && c.anchor < posEnd);
                if (c.anchor == posEnd && c.hasSelection() && c.end() == c.anchor) anchorInA = true;
                if (isEOF && c.anchor == posEnd) anchorInA = true;
                if (headInA) {
                    if (c.head >= prevLineLen) c.head -= prevLineLen; else c.head = 0;
                } else if (c.head >= posTarget && c.head < posStart) {
                    c.head += moveLen;
                }
                if (anchorInA) {
                    if (c.anchor >= prevLineLen) c.anchor -= prevLineLen; else c.anchor = 0;
                } else if (c.anchor >= posTarget && c.anchor < posStart) {
                    c.anchor += moveLen;
                }
                c.desiredX = getXFromPos(c.head);
                c.isVirtual = false;
            }
            rebuildLineStarts();
        }
    } else {
        for (int i = (int)groups.size() - 1; i >= 0; --i) {
            int gStart = groups[i].first, gEnd = groups[i].second, swapLine = gEnd + 1;
            size_t posStart = lineStarts[gStart], posEnd = (gEnd + 1 < (int)lineStarts.size()) ? lineStarts[gEnd + 1] : pt.length();
            size_t posSwapEnd = (swapLine + 1 < (int)lineStarts.size()) ? lineStarts[swapLine + 1] : pt.length();
            bool isPosEndEOF = (posEnd == pt.length());
            bool isSwapEndEOF = (posSwapEnd == pt.length());
            std::string movingText = pt.getRange(posStart, posEnd - posStart);
            size_t moveLen = movingText.length();
            size_t swapLineLen = posSwapEnd - posEnd;
            bool movingHasNewline = (moveLen > 0 && (movingText.back() == '\n' || movingText.back() == '\r'));
            pt.erase(posStart, moveLen);
            batch.ops.push_back({ EditOp::Erase, posStart, movingText });
            size_t insertPos = posStart + swapLineLen;
            if (isSwapEndEOF && movingHasNewline) {
                bool precedingCharIsNewline = (insertPos > 0 && (pt.charAt(insertPos - 1) == '\n' || pt.charAt(insertPos - 1) == '\r'));
                if ((insertPos > 0 && !precedingCharIsNewline) || swapLineLen == 0) {
                    std::string nlStr = "\n";
                    if (moveLen >= 2 && movingText[moveLen - 2] == '\r' && movingText[moveLen - 1] == '\n') {
                        nlStr = "\r\n";
                    } else if (movingText.back() == '\r') {
                        nlStr = "\r";
                    }
                    pt.insert(insertPos, nlStr);
                    batch.ops.push_back({ EditOp::Insert, insertPos, nlStr });
                    insertPos += nlStr.size();
                    swapLineLen += nlStr.size();
                    movingText.erase(movingText.size() - nlStr.size());
                    moveLen -= nlStr.size();
                }
            }
            pt.insert(insertPos, movingText);
            batch.ops.push_back({ EditOp::Insert, insertPos, movingText });
            for (auto& c : cursors) {
                bool headInA = (c.head >= posStart && c.head < posEnd);
                if (c.head == posEnd && c.hasSelection() && c.end() == c.head) headInA = true;
                if (isPosEndEOF && c.head == posEnd) headInA = true;
                bool anchorInA = (c.anchor >= posStart && c.anchor < posEnd);
                if (c.anchor == posEnd && c.hasSelection() && c.end() == c.anchor) anchorInA = true;
                if (isPosEndEOF && c.anchor == posEnd) anchorInA = true;
                if (headInA) { c.head += swapLineLen; } else if (c.head >= posEnd && c.head < posSwapEnd) { c.head -= moveLen; }
                if (anchorInA) { c.anchor += swapLineLen; } else if (c.anchor >= posEnd && c.anchor < posSwapEnd) { c.anchor -= moveLen; }
                c.desiredX = getXFromPos(c.head);
                c.isVirtual = false;
            }
            rebuildLineStarts();
        }
    }
    batch.afterCursors = cursors;
    undo.push(batch);
    ensureCaretVisible();
    updateDirtyFlag();
    if (cbUpdatePreview && showPreview) cbUpdatePreview();
}

void Editor::duplicateLines(bool up) {
    rollbackPadding();
    isRectSelecting = false;
    std::vector<int> lines = getUniqueLineIndices();
    if (lines.empty()) return;
    EditBatch batch;
    batch.beforeCursors = cursors;
    std::vector<std::pair<int, int>> groups;
    int start = lines[0], prev = lines[0];
    for (size_t i = 1; i < lines.size(); ++i) {
        if (lines[i] != prev + 1) { groups.push_back({ start, prev }); start = lines[i]; }
        prev = lines[i];
    }
    groups.push_back({ start, prev });

    for (int i = (int)groups.size() - 1; i >= 0; --i) {
        int gStart = groups[i].first, gEnd = groups[i].second;
        size_t posStart = lineStarts[gStart];
        size_t posEnd = (gEnd + 1 < (int)lineStarts.size()) ? lineStarts[gEnd + 1] : pt.length();
        std::string blockText = pt.getRange(posStart, posEnd - posStart);
        bool endsWithNewline = (!blockText.empty() && (blockText.back() == '\n' || blockText.back() == '\r'));

        if (!endsWithNewline) {
            std::string toInsert = newlineStr + blockText;
            size_t insLen = toInsert.size();
            pt.insert(posEnd, toInsert);
            batch.ops.push_back({ EditOp::Insert, posEnd, toInsert });
            for (auto& c : cursors) {
                if (!up) {
                    if (c.head >= posStart && c.head <= posEnd) c.head += insLen;
                    else if (c.head > posEnd) c.head += insLen;
                    if (c.anchor >= posStart && c.anchor <= posEnd) c.anchor += insLen;
                    else if (c.anchor > posEnd) c.anchor += insLen;
                } else {
                    if (c.head > posEnd) c.head += insLen;
                    if (c.anchor > posEnd) c.anchor += insLen;
                }
            }
        } else {
            if (up) {
                pt.insert(posStart, blockText);
                batch.ops.push_back({ EditOp::Insert, posStart, blockText });
                size_t insLen = blockText.size();
                for (auto& c : cursors) {
                    if (c.head >= posStart) c.head += insLen;
                    if (c.anchor >= posStart) c.anchor += insLen;
                    if (c.head >= posStart + insLen && c.head < posEnd + insLen) c.head -= insLen;
                    if (c.anchor >= posStart + insLen && c.anchor < posEnd + insLen) c.anchor -= insLen;
                }
            } else {
                pt.insert(posEnd, blockText);
                batch.ops.push_back({ EditOp::Insert, posEnd, blockText });
                size_t insLen = blockText.size();
                for (auto& c : cursors) {
                    if (c.head >= posEnd) c.head += insLen;
                    if (c.anchor >= posEnd) c.anchor += insLen;
                    if (c.head >= posStart && c.head < posEnd) c.head += insLen;
                    if (c.anchor >= posStart && c.anchor < posEnd) c.anchor += insLen;
                }
            }
        }
        rebuildLineStarts();
    }
    for (auto& c : cursors) {
        c.desiredX = getXFromPos(c.head);
        c.isVirtual = false;
    }
    batch.afterCursors = cursors;
    undo.push(batch);
    ensureCaretVisible();
    updateDirtyFlag();
    if (cbUpdatePreview && showPreview) cbUpdatePreview();
}

void Editor::copyLines(bool up) {
    duplicateLines(up);
}

void Editor::indentLines(bool forceLineIndent) {
    rollbackPadding();
    bool hasSelection = false;
    for (const auto& c : cursors) if (c.hasSelection()) hasSelection = true;
    if (!hasSelection && !forceLineIndent) {
        insertAtCursors("\t");
        return;
    }
    std::vector<int> lines = getUniqueLineIndices();
    if (lines.empty()) return;
    EditBatch batch;
    batch.beforeCursors = cursors;
    std::sort(lines.rbegin(), lines.rend());
    for (int lineIdx : lines) {
        size_t pos = lineStarts[lineIdx];
        std::string indentStr = "\t";
        pt.insert(pos, indentStr);
        batch.ops.push_back({ EditOp::Insert, pos, indentStr });
        for (auto& c : cursors) {
            if (!c.hasSelection()) {
                if (c.head >= pos) c.head += indentStr.size();
                if (c.anchor >= pos) c.anchor += indentStr.size();
            } else {
                if (c.head < c.anchor) {
                    if (c.head > pos) c.head += indentStr.size();
                    if (c.anchor >= pos) c.anchor += indentStr.size();
                } else {
                    if (c.anchor > pos) c.anchor += indentStr.size();
                    if (c.head >= pos) c.head += indentStr.size();
                }
            }
        }
    }
    rebuildLineStarts();
    for (auto& c : cursors) {
        c.desiredX = getXFromPos(c.head);
        c.originalAnchorX = getXFromPos(c.anchor);
        c.isVirtual = false;
    }
    batch.afterCursors = cursors;
    undo.push(batch);
    ensureCaretVisible();
    updateDirtyFlag();
    if (cbUpdatePreview && showPreview) cbUpdatePreview();
}

void Editor::unindentLines() {
    rollbackPadding();
    std::vector<int> lines = getUniqueLineIndices();
    if (lines.empty()) return;
    EditBatch batch;
    batch.beforeCursors = cursors;
    std::sort(lines.rbegin(), lines.rend());
    for (int lineIdx : lines) {
        size_t pos = lineStarts[lineIdx];
        if (pos >= pt.length()) continue;
        char c = pt.charAt(pos);
        size_t eraseLen = 0;
        if (c == '\t') eraseLen = 1;
        else if (c == ' ') {
            while (eraseLen < 4 && pos + eraseLen < pt.length() && pt.charAt(pos + eraseLen) == ' ') {
                eraseLen++;
            }
        }
        if (eraseLen > 0) {
            std::string deleted = pt.getRange(pos, eraseLen);
            pt.erase(pos, eraseLen);
            batch.ops.push_back({ EditOp::Erase, pos, deleted });
            for (auto& cur : cursors) {
                if (cur.head > pos) cur.head -= std::min(cur.head - pos, eraseLen);
                if (cur.anchor > pos) cur.anchor -= std::min(cur.anchor - pos, eraseLen);
            }
        }
    }
    if (!batch.ops.empty()) {
        rebuildLineStarts();
        for (auto& cur : cursors) {
            cur.desiredX = getXFromPos(cur.head);
            cur.originalAnchorX = cur.desiredX;
            cur.isVirtual = false;
        }
        batch.afterCursors = cursors;
        undo.push(batch);
        ensureCaretVisible();
        updateDirtyFlag();
        if (cbUpdatePreview && showPreview) cbUpdatePreview();
    }
}

void Editor::toggleComment() {
    if (cursors.empty()) return;
    if (currentSyntax.lineComment.empty() && (currentSyntax.blockCommentStart.empty() || currentSyntax.blockCommentEnd.empty())) {
        return;
    }
    rollbackPadding();
    isRectSelecting = false;
    std::vector<int> lines = getUniqueLineIndices();
    if (lines.empty()) return;
    EditBatch batch;
    batch.beforeCursors = cursors;

    struct LineCommentInfo {
        int lineIdx;
        size_t lineStart;
        std::string text;
        size_t firstNonWs;
        size_t lastNonWs;
        bool isBlank;
        bool isCommented;
        size_t openRemLen;
        size_t closeRemLen;
    };
    std::vector<LineCommentInfo> infos;
    infos.reserve(lines.size());

    bool useLineComment = !currentSyntax.lineComment.empty();
    const std::string& lineTok = currentSyntax.lineComment;
    const std::string& blkOpen = currentSyntax.blockCommentStart;
    const std::string& blkClose = currentSyntax.blockCommentEnd;
    bool isBat = useLineComment && (Emmet::toLowerStr(lineTok) == "rem");

    bool hasNonBlank = false;
    bool allCommented = true;
    size_t minIndent = std::string::npos;

    for (int lineIdx : lines) {
        if (lineIdx < 0 || lineIdx >= (int)lineStarts.size()) continue;
        size_t start = lineStarts[lineIdx];
        size_t end = (lineIdx + 1 < (int)lineStarts.size()) ? lineStarts[lineIdx + 1] : pt.length();
        std::string s = pt.getRange(start, end - start);
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
        size_t f = 0;
        while (f < s.size() && (s[f] == ' ' || s[f] == '\t')) f++;
        size_t l = s.size();
        while (l > f && (s[l - 1] == ' ' || s[l - 1] == '\t')) l--;
        bool blank = (f >= s.size());
        bool commented = false;
        size_t openRem = 0;
        size_t closeRem = 0;

        if (!blank) {
            hasNonBlank = true;
            if (minIndent == std::string::npos || f < minIndent) minIndent = f;
            if (useLineComment) {
                if (isBat) {
                    if (f + 2 <= s.size() && s.compare(f, 2, "::") == 0) {
                        commented = true;
                        openRem = (f + 2 < s.size() && s[f + 2] == ' ') ? 3 : 2;
                    } else if (f + 3 <= s.size() && Emmet::toLowerStr(s.substr(f, 3)) == "rem" && (f + 3 == s.size() || s[f + 3] == ' ' || s[f + 3] == '\t')) {
                        commented = true;
                        openRem = (f + 3 < s.size() && s[f + 3] == ' ') ? 4 : 3;
                    }
                } else if (f + lineTok.size() <= s.size() && s.compare(f, lineTok.size(), lineTok) == 0) {
                    commented = true;
                    openRem = (f + lineTok.size() < s.size() && s[f + lineTok.size()] == ' ') ? (lineTok.size() + 1) : lineTok.size();
                }
            } else {
                if (l - f >= blkOpen.size() + blkClose.size() &&
                    s.compare(f, blkOpen.size(), blkOpen) == 0 &&
                    s.compare(l - blkClose.size(), blkClose.size(), blkClose) == 0) {
                    commented = true;
                    openRem = (f + blkOpen.size() < l - blkClose.size() && s[f + blkOpen.size()] == ' ') ? (blkOpen.size() + 1) : blkOpen.size();
                    size_t closePos = l - blkClose.size();
                    closeRem = (closePos > f + openRem && s[closePos - 1] == ' ') ? (blkClose.size() + 1) : blkClose.size();
                }
            }
            if (!commented) allCommented = false;
        }
        infos.push_back({ lineIdx, start, std::move(s), f, l, blank, commented, openRem, closeRem });
    }

    if (!hasNonBlank) allCommented = false;

    auto applyInsert = [&](size_t pos, const std::string& ins, bool advanceCaretAtPos) {
        pt.insert(pos, ins);
        batch.ops.push_back({ EditOp::Insert, pos, ins });
        size_t len = ins.size();
        for (auto& c : cursors) {
            if (c.hasSelection()) {
                if (c.head > pos) c.head += len;
                if (c.anchor > pos) c.anchor += len;
            } else {
                if (advanceCaretAtPos ? (c.head >= pos) : (c.head > pos)) {
                    c.head += len;
                    c.anchor = c.head;
                }
            }
        }
    };

    auto applyErase = [&](size_t pos, size_t rem) {
        if (rem == 0) return;
        std::string deleted = pt.getRange(pos, rem);
        pt.erase(pos, rem);
        batch.ops.push_back({ EditOp::Erase, pos, deleted });
        for (auto& c : cursors) {
            if (c.head > pos) c.head = (c.head >= pos + rem) ? (c.head - rem) : pos;
            if (c.anchor > pos) c.anchor = (c.anchor >= pos + rem) ? (c.anchor - rem) : pos;
        }
    };

    for (auto it = infos.rbegin(); it != infos.rend(); ++it) {
        const auto& info = *it;
        if (allCommented) {
            if (info.isBlank || !info.isCommented) continue;
            if (useLineComment) {
                applyErase(info.lineStart + info.firstNonWs, info.openRemLen);
            } else {
                size_t closeErasePos = info.lineStart + info.lastNonWs - info.closeRemLen;
                applyErase(closeErasePos, info.closeRemLen);
                applyErase(info.lineStart + info.firstNonWs, info.openRemLen);
            }
        } else {
            if (hasNonBlank && info.isBlank) continue;
            if (useLineComment) {
                size_t indentOffset = hasNonBlank ? std::min(info.firstNonWs, minIndent) : info.text.size();
                std::string ins = lineTok + " ";
                applyInsert(info.lineStart + indentOffset, ins, true);
            } else {
                if (!info.isBlank) {
                    std::string closeIns = " " + blkClose;
                    applyInsert(info.lineStart + info.lastNonWs, closeIns, false);
                    size_t indentOffset = std::min(info.firstNonWs, minIndent);
                    std::string openIns = blkOpen + " ";
                    applyInsert(info.lineStart + indentOffset, openIns, true);
                } else {
                    size_t pos = info.lineStart + info.text.size();
                    std::string closeIns = " " + blkClose;
                    applyInsert(pos, closeIns, false);
                    std::string openIns = blkOpen + " ";
                    applyInsert(pos, openIns, true);
                }
            }
        }
    }

    if (!batch.ops.empty()) {
        rebuildLineStarts();
        for (auto& c : cursors) {
            c.desiredX = getXFromPos(c.head);
            c.originalAnchorX = getXFromPos(c.anchor);
            c.isVirtual = false;
        }
        batch.afterCursors = cursors;
        undo.push(batch);
        applyBatchToEmmetTabstops(batch);
        ensureCaretVisible();
        updateDirtyFlag();
        if (cbUpdatePreview && showPreview) cbUpdatePreview();
        if (cbNeedsDisplay) cbNeedsDisplay();
    }
}

bool Editor::tryExpandEmmet() {
    if (cursors.empty() || isRectSelecting) return false;
    for (const auto& c : cursors) {
        if (c.hasSelection()) return false;
    }
    if (cursors.size() == 1 && activeEmmetSession.active &&
        activeEmmetSession.currentGroupIndex >= 0 &&
        activeEmmetSession.currentGroupIndex < (int)activeEmmetSession.groups.size() &&
        !activeEmmetSession.groups[activeEmmetSession.currentGroupIndex].ranges.empty()) {
        const auto& r = activeEmmetSession.groups[activeEmmetSession.currentGroupIndex].ranges[0];
        size_t curStart = activeEmmetSession.baseOffset + r.first;
        size_t curEnd = activeEmmetSession.baseOffset + r.second;
        if (cursors[0].head < curStart || cursors[0].head > curEnd) {
            activeEmmetSession.active = false;
            activeEmmetSession.groups.clear();
            activeEmmetSession.currentGroupIndex = -1;
        } else if (curStart == curEnd) {
            return false;
        }
    }

    auto getEmmetModeAndContext = [&](size_t cursorPos, size_t lineStartPos, bool& outInsideCssBlock) -> Emmet::Mode {
        outInsideCssBlock = false;
        std::string ext = currentFileExt;
        if (!ext.empty() && ext[0] == '.') ext = ext.substr(1);
        ext = Emmet::toLowerStr(ext);

        if (ext == "css" || ext == "scss" || ext == "sass" || ext == "less" || ext == "styl" ||
            ext == "stylus" || ext == "pcss" || ext == "postcss" || ext == "wxss") {
            outInsideCssBlock = true;
            return Emmet::Mode::Css;
        }
        if (ext == "jsx" || ext == "tsx") {
            return Emmet::Mode::Jsx;
        }
        if (ext == "xml" || ext == "svg" || ext == "xhtml" || ext == "xht" || ext == "xsl" ||
            ext == "xslt" || ext == "xsd" || ext == "rss" || ext == "atom" || ext == "mathml" ||
            ext == "xaml" || ext == "plist" || ext == "wsdl" || ext == "csproj" || ext == "vcxproj" ||
            ext == "props" || ext == "targets") {
            return Emmet::Mode::Xml;
        }
        Emmet::Mode mode = Emmet::Mode::Auto;
        if (ext == "html" || ext == "htm" || ext == "shtml" || ext == "vue" || ext == "svelte" ||
            ext == "astro" || ext == "php" || ext == "phtml" || ext == "erb" || ext == "ejs" ||
            ext == "hbs" || ext == "handlebars" || ext == "mustache" || ext == "twig" || ext == "njk" ||
            ext == "liquid" || ext == "latte" || ext == "ftl" || ext == "vm" || ext == "jinja" ||
            ext == "jinja2" || ext == "j2" || ext == "cfm" || ext == "cfml" || ext == "cshtml" ||
            ext == "razor" || ext == "jsp" || ext == "asp" || ext == "aspx" || ext == "mdx") {
            mode = Emmet::Mode::Html;
        } else if (ext == "tsv" || ext == "csv" || ext == "c" || ext == "cpp" || ext == "cc" ||
                   ext == "cxx" || ext == "h" || ext == "hpp" || ext == "hxx" || ext == "py" ||
                   ext == "rb" || ext == "go" || ext == "rs" || ext == "java" || ext == "cs" ||
                   ext == "swift" || ext == "kt" || ext == "scala" || ext == "lua" || ext == "pl" ||
                   ext == "sh" || ext == "bash" || ext == "zsh" || ext == "bat" || ext == "cmd" ||
                   ext == "ps1" || ext == "sql" || ext == "json" || ext == "yaml" || ext == "yml" ||
                   ext == "toml" || ext == "ini" || ext == "cfg" || ext == "conf" || ext == "log") {
            mode = Emmet::Mode::CodeStrict;
        }
        size_t searchLen = std::min(cursorPos, (size_t)32768);
        if (searchLen > 0) {
            std::string pre = pt.getRange(cursorPos - searchLen, searchLen);
            std::string preLower = Emmet::toLowerStr(pre);
            size_t lastStyleOpen = preLower.rfind("<style");
            size_t lastStyleClose = preLower.rfind("</style");
            if (lastStyleOpen != std::string::npos && (lastStyleClose == std::string::npos || lastStyleOpen > lastStyleClose)) {
                size_t gt = preLower.find('>', lastStyleOpen);
                if (gt != std::string::npos) {
                    outInsideCssBlock = true;
                }
            } else if (mode == Emmet::Mode::Auto) {
                size_t checkEnd = (cursorPos >= lineStartPos && searchLen >= (cursorPos - lineStartPos))
                    ? (searchLen - (cursorPos - lineStartPos))
                    : searchLen;
                int braceBalance = 0;
                bool sawHtmlTagAfterOpenBrace = false;
                bool openBraceLooksLikeCode = false;
                for (size_t k = 0; k < checkEnd; ++k) {
                    char ch = pre[k];
                    if (ch == '{') {
                        braceBalance++;
                        sawHtmlTagAfterOpenBrace = false;
                        size_t pNonWs = (k > 0) ? pre.find_last_not_of(" \t\r\n", k - 1) : std::string::npos;
                        size_t lineBegin = (k > 0) ? pre.find_last_of("\r\n", k - 1) : std::string::npos;
                        lineBegin = (lineBegin == std::string::npos) ? 0 : lineBegin + 1;
                        std::string selLine = preLower.substr(lineBegin, k - lineBegin);
                        if (pNonWs != std::string::npos && (pre[pNonWs] == '=' || (pre[pNonWs] == ')' && selLine.find('@') == std::string::npos && selLine.find(':') == std::string::npos))) {
                            openBraceLooksLikeCode = true;
                        } else {
                            openBraceLooksLikeCode = false;
                        }
                    } else if (ch == '}') {
                        if (braceBalance > 0) braceBalance--;
                    } else if (ch == '<' && k + 1 < checkEnd && (Emmet::isAsciiAlpha(pre[k + 1]) || pre[k + 1] == '/')) {
                        sawHtmlTagAfterOpenBrace = true;
                    }
                }
                if (braceBalance > 0 && !sawHtmlTagAfterOpenBrace && !openBraceLooksLikeCode) {
                    outInsideCssBlock = true;
                }
            }
        }
        return mode;
    };

    struct CursorExpansion {
        int cursorIdx;
        size_t erasePos;
        size_t eraseLen;
        Emmet::ExpandResult result;
    };
    std::vector<CursorExpansion> expansions;
    expansions.reserve(cursors.size());

    for (size_t i = 0; i < cursors.size(); ++i) {
        const Cursor& c = cursors[i];
        int lineIdx = getLineIdx(c.head);
        if (lineIdx < 0 || lineIdx >= (int)lineStarts.size()) return false;
        size_t lineStart = lineStarts[lineIdx];
        size_t lineEnd = (lineIdx + 1 < (int)lineStarts.size()) ? lineStarts[lineIdx + 1] : pt.length();
        if (c.head < lineStart || c.head > lineEnd) return false;
        std::string lineText = pt.getRange(lineStart, lineEnd - lineStart);
        while (!lineText.empty() && (lineText.back() == '\n' || lineText.back() == '\r')) {
            lineText.pop_back();
        }
        size_t caretCol = c.head - lineStart;
        if (caretCol == 0 || caretCol > lineText.size()) return false;

        bool insideCssBlock = false;
        Emmet::Mode mode = getEmmetModeAndContext(c.head, lineStart, insideCssBlock);
        Emmet::ExpandResult res = Emmet::expandOnLine(lineText, caretCol, mode, insideCssBlock, newlineStr);
        if (!res.matched) return false;

        size_t erasePos = lineStart + res.abbrStartCol;
        if (cursors.size() == 1 && activeEmmetSession.active &&
            activeEmmetSession.currentGroupIndex >= 0 &&
            activeEmmetSession.currentGroupIndex < (int)activeEmmetSession.groups.size() &&
            !activeEmmetSession.groups[activeEmmetSession.currentGroupIndex].ranges.empty()) {
            const auto& r = activeEmmetSession.groups[activeEmmetSession.currentGroupIndex].ranges[0];
            size_t curStart = activeEmmetSession.baseOffset + r.first;
            size_t curEnd = activeEmmetSession.baseOffset + r.second;
            if (c.head >= curStart && c.head <= curEnd) {
                if (erasePos != curStart) return false;
                if (res.abbrStartCol > 0 && lineText[res.abbrStartCol - 1] != ' ' && lineText[res.abbrStartCol - 1] != '\t') {
                    bool hasOp = false;
                    for (size_t k = 0; k < res.abbrLen; ++k) {
                        char ch = lineText[res.abbrStartCol + k];
                        if (!Emmet::isAsciiAlpha(ch)) { hasOp = true; break; }
                    }
                    if (!hasOp) return false;
                }
            }
        }
        expansions.push_back({ (int)i, erasePos, res.abbrLen, std::move(res) });
    }

    EditBatch batch;
    batch.beforeCursors = cursors;
    commitPadding(batch);

    std::sort(expansions.begin(), expansions.end(), [](const CursorExpansion& a, const CursorExpansion& b) {
        return a.erasePos > b.erasePos;
    });

    for (size_t k = 0; k < expansions.size(); ++k) {
        if (k > 0 && expansions[k].erasePos + expansions[k].eraseLen > expansions[k - 1].erasePos) {
            return false;
        }
    }

    for (const auto& exp : expansions) {
        std::string deleted = pt.getRange(exp.erasePos, exp.eraseLen);
        pt.erase(exp.erasePos, exp.eraseLen);
        batch.ops.push_back({ EditOp::Erase, exp.erasePos, deleted });
        pt.insert(exp.erasePos, exp.result.expanded);
        batch.ops.push_back({ EditOp::Insert, exp.erasePos, exp.result.expanded });

        long long delta = (long long)exp.result.expanded.size() - (long long)exp.eraseLen;
        for (size_t ci = 0; ci < cursors.size(); ++ci) {
            if ((int)ci == exp.cursorIdx) continue;
            Cursor& other = cursors[ci];
            if (other.head >= exp.erasePos + exp.eraseLen) other.head = (size_t)((long long)other.head + delta);
            if (other.anchor >= exp.erasePos + exp.eraseLen) other.anchor = (size_t)((long long)other.anchor + delta);
        }

        Cursor& cur = cursors[exp.cursorIdx];
        if (!exp.result.tabstops.empty()) {
            cur.anchor = exp.erasePos + exp.result.tabstops[0].startOffset;
            cur.head = exp.erasePos + exp.result.tabstops[0].endOffset;
        } else {
            cur.anchor = cur.head = exp.erasePos + exp.result.expanded.size();
        }
    }

    rebuildLineStarts();
    for (auto& c : cursors) {
        c.desiredX = getXFromPos(c.head);
        c.originalAnchorX = getXFromPos(c.anchor);
        c.isVirtual = false;
    }

    activeEmmetSession.groups.clear();
    activeEmmetSession.baseOffset = 0;
    activeEmmetSession.currentGroupIndex = -1;
    activeEmmetSession.active = false;

    if (expansions.size() == 1 && expansions[0].result.tabstops.size() > 1) {
        activeEmmetSession.baseOffset = expansions[0].erasePos;
        for (size_t i = 0; i < expansions[0].result.tabstops.size(); ++i) {
            Emmet::TabstopGroup g;
            g.id = (int)(i + 1);
            g.ranges.push_back({ expansions[0].result.tabstops[i].startOffset, expansions[0].result.tabstops[i].endOffset });
            activeEmmetSession.groups.push_back(std::move(g));
        }
        activeEmmetSession.currentGroupIndex = 0;
        activeEmmetSession.active = true;
    }

    batch.afterCursors = cursors;
    undo.push(batch);
    ensureCaretVisible();
    updateDirtyFlag();
    if (cbUpdatePreview && showPreview) cbUpdatePreview();
    if (cbNeedsDisplay) cbNeedsDisplay();
    return true;
}

bool Editor::jumpEmmetTabstop(bool forward) {
    if (!activeEmmetSession.active || activeEmmetSession.groups.empty()) return false;
    int nextIdx = activeEmmetSession.currentGroupIndex + (forward ? 1 : -1);
    if (nextIdx < 0 || nextIdx >= (int)activeEmmetSession.groups.size()) {
        if (forward) activeEmmetSession.active = false;
        return false;
    }
    activeEmmetSession.currentGroupIndex = nextIdx;
    const auto& grp = activeEmmetSession.groups[nextIdx];
    cursors.clear();
    for (const auto& r : grp.ranges) {
        size_t s = std::min(pt.length(), activeEmmetSession.baseOffset + r.first);
        size_t e = std::min(pt.length(), activeEmmetSession.baseOffset + r.second);
        cursors.push_back({ e, s, getXFromPos(e), getXFromPos(s), false, false });
    }
    if (grp.id == 0 && nextIdx == (int)activeEmmetSession.groups.size() - 1) {
        activeEmmetSession.active = false;
    }
    ensureCaretVisible();
    if (cbNeedsDisplay) cbNeedsDisplay();
    return true;
}

void Editor::applyBatchToEmmetTabstops(const EditBatch& batch) {
    if (!activeEmmetSession.active) return;
    for (const auto& op : batch.ops) {
        if (op.pos < activeEmmetSession.baseOffset) {
            activeEmmetSession.active = false;
            return;
        }
        size_t relPos = op.pos - activeEmmetSession.baseOffset;
        ptrdiff_t delta = (op.type == EditOp::Insert) ? (ptrdiff_t)op.text.size() : -(ptrdiff_t)op.text.size();
        for (auto& g : activeEmmetSession.groups) {
            for (auto& r : g.ranges) {
                if (op.type == EditOp::Insert) {
                    if (relPos < r.first) {
                        r.first += delta;
                        r.second += delta;
                    } else if (relPos >= r.first && relPos <= r.second) {
                        r.second += delta;
                    }
                } else {
                    size_t eraseEnd = relPos + op.text.size();
                    if (eraseEnd <= r.first) {
                        r.first += delta;
                        r.second += delta;
                    } else if (relPos >= r.first && eraseEnd <= r.second) {
                        r.second += delta;
                    }
                }
            }
        }
    }
}

char Editor::getMatchingBracketChar(char c) const {
    switch (c) {
        case '(': return ')';
        case ')': return '(';
        case '{': return '}';
        case '}': return '{';
        case '[': return ']';
        case ']': return '[';
        default: return 0;
    }
}

bool Editor::isForwardBracket(char c) const {
    return c == '(' || c == '{' || c == '[';
}

size_t Editor::findMatchingBracketFromIter(size_t pos, char openB, char closeB, bool forward) const {
    const size_t MAX_SCAN = 200000;
    size_t len = pt.length();
    int depth = 1;
    size_t scanned = 0;
    if (forward) {
        size_t cur = pos + 1;
        while (cur < len && scanned < MAX_SCAN) {
            char ch = pt.charAt(cur);
            if (ch == openB) depth++;
            else if (ch == closeB) {
                depth--;
                if (depth == 0) return cur;
            }
            cur++;
            scanned++;
        }
    } else {
        if (pos == 0) return std::string::npos;
        size_t cur = pos - 1;
        while (scanned < MAX_SCAN) {
            char ch = pt.charAt(cur);
            if (ch == closeB) depth++;
            else if (ch == openB) {
                depth--;
                if (depth == 0) return cur;
            }
            if (cur == 0) break;
            cur--;
            scanned++;
        }
    }
    return std::string::npos;
}

bool Editor::getAdjacentBrackets(size_t caret, size_t& b1, size_t& b2) const {
    size_t len = pt.length();
    if (len == 0) return false;
    if (caret < len) {
        char ch = pt.charAt(caret);
        char match = getMatchingBracketChar(ch);
        if (match != 0) {
            bool fwd = isForwardBracket(ch);
            size_t mPos = findMatchingBracketFromIter(caret, fwd ? ch : match, fwd ? match : ch, fwd);
            if (mPos != std::string::npos) {
                b1 = caret;
                b2 = mPos;
                return true;
            }
        }
    }
    if (caret > 0 && caret - 1 < len) {
        char ch = pt.charAt(caret - 1);
        char match = getMatchingBracketChar(ch);
        if (match != 0) {
            bool fwd = isForwardBracket(ch);
            size_t mPos = findMatchingBracketFromIter(caret - 1, fwd ? ch : match, fwd ? match : ch, fwd);
            if (mPos != std::string::npos) {
                b1 = caret - 1;
                b2 = mPos;
                return true;
            }
        }
    }
    return false;
}

void Editor::jumpToMatchingBracket(bool select) {
    if (cursors.empty()) return;
    rollbackPadding();
    Cursor& c = cursors.back();
    size_t b1 = 0, b2 = 0;
    if (getAdjacentBrackets(c.head, b1, b2)) {
        size_t target = b2;
        if (!select) c.anchor = target;
        c.head = target;
        c.desiredX = getXFromPos(c.head);
        c.isVirtual = false;
        c.affinity = false;
        ensureCaretVisible();
        updateTitleBar();
        if (cbNeedsDisplay) cbNeedsDisplay();
    }
}

void Editor::loadSyntaxForExtension(const std::string& pathOrExt) {
    currentFileExt = extractSyntaxExtensionFromPath(pathOrExt);
    loadSyntaxDefForExtension(currentFileExt, isDarkMode, currentSyntax);
    commentStateCache.clear();
}

bool Editor::checkInBlockComment(int targetLine) {
    if (!currentSyntax.active || currentSyntax.blockCommentStart.empty() || currentSyntax.blockCommentEnd.empty() || targetLine <= 0) {
        return false;
    }
    if ((int)commentStateCache.size() <= targetLine) {
        int startLine = (int)commentStateCache.size();
        if (startLine == 0) {
            commentStateCache.push_back(0);
            startLine = 1;
        }
        bool inComment = (commentStateCache[startLine - 1] != 0);
        const std::string& bcs = currentSyntax.blockCommentStart;
        const std::string& bce = currentSyntax.blockCommentEnd;
        const std::string& lc = currentSyntax.lineComment;

        for (int li = startLine; li <= targetLine && li <= (int)lineStarts.size(); ++li) {
            int prevLi = li - 1;
            size_t s = lineStarts[prevLi];
            size_t e = (prevLi + 1 < (int)lineStarts.size()) ? lineStarts[prevLi + 1] : pt.length();
            std::string lineStr = pt.getRange(s, e - s);
            size_t i = 0, n = lineStr.size();
            bool inStr = false;
            char strQuote = 0;
            while (i < n) {
                if (inComment) {
                    if (i + bce.size() <= n && lineStr.compare(i, bce.size(), bce) == 0) {
                        inComment = false;
                        i += bce.size();
                    } else {
                        i++;
                    }
                } else if (inStr) {
                    if (lineStr[i] == '\\' && i + 1 < n) i += 2;
                    else if (lineStr[i] == strQuote) { inStr = false; i++; }
                    else i++;
                } else {
                    if (!lc.empty() && i + lc.size() <= n && lineStr.compare(i, lc.size(), lc) == 0) {
                        break; // Rest of line is line comment
                    }
                    if (i + bcs.size() <= n && lineStr.compare(i, bcs.size(), bcs) == 0) {
                        inComment = true;
                        i += bcs.size();
                    } else if (lineStr[i] == '"' || lineStr[i] == '\'') {
                        inStr = true;
                        strQuote = lineStr[i];
                        i++;
                    } else {
                        i++;
                    }
                }
            }
            commentStateCache.push_back(inComment ? 1 : 0);
        }
    }
    return commentStateCache[targetLine] != 0;
}

float Editor::getEffectiveTextWidth(float viewW) const {
    return std::max(charWidth * 4.0f, viewW - gutterWidth - visibleVScrollWidth);
}

float Editor::measureLineWidth(int lineIdx) {
    if (lineIdx < 0 || lineIdx >= (int)lineStarts.size()) return 100.0f;
    size_t s = lineStarts[lineIdx];
    size_t e = (lineIdx + 1 < (int)lineStarts.size()) ? lineStarts[lineIdx + 1] : pt.length();
    if (e > s && pt.charAt(e - 1) == '\n') e--;
    if (e > s && pt.charAt(e - 1) == '\r') e--;
    if (e <= s) return 20.0f;
    return getXInLine(lineIdx, e) + 20.0f;
}

void Editor::updateGutterWidth() {
    int totalLines = (int)lineStarts.size(), digits = 1;
    int tempLines = totalLines;
    while (tempLines >= 10) { tempLines /= 10; digits++; }
    gutterWidth = (float)(digits * charWidth) + (charWidth * 1.5f);
}

void Editor::updateMaxLineWidth() {
    if (lineStarts.empty()) {
        maxLineBytes = 0;
        maxLineBytesIdx = 0;
        maxLineWidth = 100.0f;
        return;
    }
    maxLineBytes = 0;
    maxLineBytesIdx = 0;
    size_t docLen = pt.length();
    for (int i = 0; i < (int)lineStarts.size(); ++i) {
        size_t s = lineStarts[i];
        size_t e = (i + 1 < (int)lineStarts.size()) ? lineStarts[i + 1] : docLen;
        if (e > s && pt.charAt(e - 1) == '\n') e--;
        if (e > s && pt.charAt(e - 1) == '\r') e--;
        size_t bLen = (e > s) ? (e - s) : 0;
        if (bLen > maxLineBytes) {
            maxLineBytes = bLen;
            maxLineBytesIdx = i;
        }
    }
    maxLineWidth = std::max(100.0f, measureLineWidth(maxLineBytesIdx));
}

bool Editor::canFastAdjustLineStarts(const std::string& s) const {
    if (lineStarts.empty()) return false;
    for (char c : s) {
        if (c == '\n' || c == '\r') return false;
    }
    return true;
}

void Editor::adjustLineStartsSingleLine(size_t editPos, ptrdiff_t delta) {
    if (lineStarts.empty() || delta == 0) return;
    int li = getLineIdx(editPos);
    for (size_t i = (size_t)li + 1; i < lineStarts.size(); ++i) {
        lineStarts[i] = (size_t)((ptrdiff_t)lineStarts[i] + delta);
    }
    if ((int)commentStateCache.size() > li) {
        commentStateCache.resize(li + 1);
    }
    size_t s = lineStarts[li];
    size_t e = (li + 1 < (int)lineStarts.size()) ? lineStarts[li + 1] : pt.length();
    if (e > s && pt.charAt(e - 1) == '\n') e--;
    if (e > s && pt.charAt(e - 1) == '\r') e--;
    size_t newLen = (e > s) ? (e - s) : 0;

    if (li == maxLineBytesIdx) {
        if (newLen >= maxLineBytes) {
            maxLineBytes = newLen;
            maxLineWidth = std::max(100.0f, measureLineWidth(li));
        } else {
            updateMaxLineWidth();
        }
    } else if (newLen > maxLineBytes) {
        maxLineBytes = newLen;
        maxLineBytesIdx = li;
        maxLineWidth = std::max(100.0f, measureLineWidth(li));
    }
    updateScrollBars();
}

ScrollBarLayout Editor::getScrollBarLayout(float clientW, float clientH) {
    ScrollBarLayout L;
    L.totalLines = std::max(1, (int)lineStarts.size());
    L.totalVisualLines = L.totalLines;
#if defined(TARGET_OS_IOS) && TARGET_OS_IOS
    float barSize = 0.0f;
#else
    float barSize = 14.0f;
#endif

    if (wordWrapEnabled) {
        L.needH = false;
        L.hBarH = 0.0f;
        L.visibleLines = std::max(1, (int)(clientH / lineHeight));
        L.needV = (L.totalLines > L.visibleLines) || (vScrollPos > 0) || (vScrollSubLine > 0);
        float prelimWrapW = std::max(1.0f, clientW - gutterWidth - (L.needV ? barSize : 0.0f));
        if (L.totalLines == 1) {
            int count = getLineVisualCount(0, prelimWrapW);
            L.totalVisualLines = std::max(1, count);
            if (L.totalVisualLines > L.visibleLines) L.needV = true;
        } else if (!L.needV) {
            int visSum = 0;
            for (int i = 0; i < L.totalLines; ++i) {
                visSum += getLineVisualCount(i, prelimWrapW);
                if (visSum > L.visibleLines) {
                    L.needV = true;
                    break;
                }
            }
            L.totalVisualLines = std::max(L.totalLines, visSum);
        }
        L.vBarW = L.needV ? barSize : 0.0f;
        L.textAreaW = std::max(1.0f, clientW - gutterWidth - L.vBarW);
        L.textAreaH = std::max(1.0f, clientH);
        return L;
    }

    int visLinesNoH = std::max(1, (int)(clientH / lineHeight));
    L.needV = (L.totalLines > visLinesNoH);
    L.vBarW = L.needV ? barSize : 0.0f;
    float availW = std::max(1.0f, clientW - gutterWidth - L.vBarW);
    L.needH = (maxLineWidth > availW + 1.0f) || (hScrollPos > 0);
    L.hBarH = L.needH ? barSize : 0.0f;

    float availH = std::max(1.0f, clientH - L.hBarH);
    L.visibleLines = std::max(1, (int)(availH / lineHeight));
    if (!L.needV && L.totalLines > L.visibleLines) {
        L.needV = true;
        L.vBarW = barSize;
        availW = std::max(1.0f, clientW - gutterWidth - L.vBarW);
        L.needH = (maxLineWidth > availW + 1.0f) || (hScrollPos > 0);
        L.hBarH = L.needH ? barSize : 0.0f;
    }
    L.textAreaW = availW;
    L.textAreaH = availH;
    return L;
}

ScrollMetrics Editor::getVScrollMetrics(float clientW, float clientH) {
    ScrollBarLayout L = getScrollBarLayout(clientW, clientH);
    ScrollMetrics M;
    M.trackStart = 2.0f;
    M.trackEnd = std::max(M.trackStart + 1.0f, L.textAreaH - 2.0f);
    M.trackLen = M.trackEnd - M.trackStart;
    int totalForScroll = (wordWrapEnabled && L.totalVisualLines > L.totalLines) ? L.totalVisualLines : L.totalLines;
    int currentPos = (wordWrapEnabled && L.totalLines == 1 && L.totalVisualLines > 1) ? vScrollSubLine : vScrollPos;
    M.maxScroll = std::max(0, totalForScroll - L.visibleLines);
    float ratio = (totalForScroll > 0) ? std::clamp((float)L.visibleLines / (float)totalForScroll, 0.05f, 1.0f) : 1.0f;
    M.thumbSize = std::max(24.0f, M.trackLen * ratio);
    if (M.thumbSize > M.trackLen) M.thumbSize = M.trackLen;
    float scrollableTrack = std::max(0.0f, M.trackLen - M.thumbSize);
    float posRatio = (M.maxScroll > 0) ? std::clamp((float)currentPos / (float)M.maxScroll, 0.0f, 1.0f) : 0.0f;
    M.thumbPos = M.trackStart + scrollableTrack * posRatio;
    return M;
}

ScrollMetrics Editor::getHScrollMetrics(float clientW, float clientH) {
    ScrollBarLayout L = getScrollBarLayout(clientW, clientH);
    ScrollMetrics M;
    M.trackStart = gutterWidth + 2.0f;
    M.trackEnd = std::max(M.trackStart + 1.0f, clientW - L.vBarW - 2.0f);
    M.trackLen = M.trackEnd - M.trackStart;
    M.maxScroll = std::max(0, (int)std::ceil(maxLineWidth - L.textAreaW));
    float ratio = (maxLineWidth > 0.0f) ? std::clamp(L.textAreaW / maxLineWidth, 0.05f, 1.0f) : 1.0f;
    M.thumbSize = std::max(24.0f, M.trackLen * ratio);
    if (M.thumbSize > M.trackLen) M.thumbSize = M.trackLen;
    float scrollableTrack = std::max(0.0f, M.trackLen - M.thumbSize);
    float posRatio = (M.maxScroll > 0) ? std::clamp((float)hScrollPos / (float)M.maxScroll, 0.0f, 1.0f) : 0.0f;
    M.thumbPos = M.trackStart + scrollableTrack * posRatio;
    return M;
}

std::vector<VisualSubLine> Editor::getVisualSubLines(int lineIdx, float wrapWidth) {
    std::vector<VisualSubLine> subs;
    if (lineIdx < 0 || lineIdx >= (int)lineStarts.size()) {
        subs.push_back({ 0, 0, 0, 0 });
        return subs;
    }
    size_t s = lineStarts[lineIdx];
    size_t e = (lineIdx + 1 < (int)lineStarts.size()) ? lineStarts[lineIdx + 1] : pt.length();
    if (e > s && pt.charAt(e - 1) == '\n') e--;
    if (e > s && pt.charAt(e - 1) == '\r') e--;
    size_t lineByteLen = (e > s) ? (e - s) : 0;

    if (!wordWrapEnabled || wrapWidth <= charWidth * 2.0f || lineByteLen == 0) {
        size_t u16Len = countUTF16Length(s, lineByteLen);
        subs.push_back({ 0, lineByteLen, 0, u16Len });
        return subs;
    }

    std::string lstr = pt.getRange(s, lineByteLen);
#if defined(__APPLE__)
    if (fontRef && paragraphStyle) {
        CFStringRef cf = CFStringCreateWithBytes(NULL, (const UInt8*)lstr.data(), lstr.size(), kCFStringEncodingUTF8, false);
        if (cf) {
            CFIndex totalU16 = CFStringGetLength(cf);
            const void* keys[] = { kCTFontAttributeName, kCTParagraphStyleAttributeName };
            const void* values[] = { fontRef, paragraphStyle };
            CFDictionaryRef d = CFDictionaryCreate(NULL, keys, values, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
            CFAttributedStringRef as = CFAttributedStringCreate(NULL, cf, d);
            CTTypesetterRef ts = CTTypesetterCreateWithAttributedString(as);

            // Build byte->u16 map for this line
            std::vector<size_t> u16ToByte;
            u16ToByte.reserve(totalU16 + 1);
            size_t bPos = 0;
            while (bPos < lstr.size()) {
                unsigned char c = (unsigned char)lstr[bPos];
                size_t step = 1, u16Step = 1;
                if (c < 0x80) { step = 1; u16Step = 1; }
                else if ((c & 0xE0) == 0xC0) { step = 2; u16Step = 1; }
                else if ((c & 0xF0) == 0xE0) { step = 3; u16Step = 1; }
                else if ((c & 0xF8) == 0xF0) { step = 4; u16Step = 2; }
                for (size_t k = 0; k < u16Step; ++k) u16ToByte.push_back(bPos);
                bPos = std::min(lstr.size(), bPos + step);
            }
            while ((CFIndex)u16ToByte.size() <= totalU16) u16ToByte.push_back(lstr.size());

            CFIndex curU16 = 0;
            double availW = std::max((double)charWidth * 2.0, (double)wrapWidth - 4.0);
            while (curU16 < totalU16) {
                CFIndex count = CTTypesetterSuggestClusterBreak(ts, curU16, availW);
                if (count <= 0) count = 1;
                if (curU16 + count > totalU16) count = totalU16 - curU16;
                size_t bStart = u16ToByte[curU16];
                size_t bEnd = u16ToByte[curU16 + count];
                subs.push_back({ bStart, bEnd - bStart, (size_t)curU16, (size_t)count });
                curU16 += count;
            }
            CFRelease(ts);
            CFRelease(as);
            CFRelease(d);
            CFRelease(cf);
            if (subs.empty()) subs.push_back({ 0, 0, 0, 0 });
            return subs;
        }
    }
#endif
    // Platform-independent fallback character cluster width wrapper
    float maxW = std::max(charWidth * 2.0f, wrapWidth - 4.0f);
    size_t curByte = 0;
    size_t curU16 = 0;
    size_t subStartByte = 0;
    size_t subStartU16 = 0;
    float curW = 0.0f;

    while (curByte < lstr.size()) {
        unsigned char c = (unsigned char)lstr[curByte];
        size_t step = 1, u16Step = 1;
        float cw = charWidth;
        if (c == '\t') {
            float nextTab = (std::floor(curW / tabWidth) + 1.0f) * tabWidth;
            cw = nextTab - curW;
        } else if (c < 0x80) {
            step = 1; u16Step = 1; cw = charWidth;
        } else if ((c & 0xE0) == 0xC0) {
            step = 2; u16Step = 1; cw = charWidth;
        } else if ((c & 0xF0) == 0xE0) {
            step = 3; u16Step = 1; cw = charWidth * 2.0f;
        } else if ((c & 0xF8) == 0xF0) {
            step = 4; u16Step = 2; cw = charWidth * 2.0f;
        }
        step = std::min(step, lstr.size() - curByte);

        if (curW + cw > maxW && curByte > subStartByte) {
            subs.push_back({ subStartByte, curByte - subStartByte, subStartU16, curU16 - subStartU16 });
            subStartByte = curByte;
            subStartU16 = curU16;
            curW = 0.0f;
        }
        curW += cw;
        curByte += step;
        curU16 += u16Step;
    }
    if (curByte > subStartByte || subs.empty()) {
        subs.push_back({ subStartByte, curByte - subStartByte, subStartU16, curU16 - subStartU16 });
    }
    return subs;
}

int Editor::getLineVisualCount(int lineIdx, float wrapWidth) {
    if (!wordWrapEnabled) return 1;
    return (int)getVisualSubLines(lineIdx, wrapWidth).size();
}

bool Editor::isVisualLineWrapBoundary(int lineIdx, size_t pos, float wrapWidth) {
    if (!wordWrapEnabled || lineIdx < 0 || lineIdx >= (int)lineStarts.size()) return false;
    size_t s = lineStarts[lineIdx];
    if (pos <= s) return false;
    size_t relPos = pos - s;
    auto subs = getVisualSubLines(lineIdx, wrapWidth);
    for (size_t i = 1; i < subs.size(); ++i) {
        if (subs[i].byteStart == relPos) return true;
    }
    return false;
}

void Editor::scrollVisualLines(int deltaVisualLines) {
    if (deltaVisualLines == 0 || lineStarts.empty()) return;
    if (!wordWrapEnabled) {
        vScrollPos = std::clamp(vScrollPos + deltaVisualLines, 0, std::max(0, (int)lineStarts.size() - 1));
        vScrollSubLine = 0;
        updateScrollBars();
        if (cbNeedsDisplay) cbNeedsDisplay();
        return;
    }
    float vw = 800.0f, vh = 600.0f;
    if (cbGetViewSize) cbGetViewSize(vw, vh);
    float wrapW = getEffectiveTextWidth(vw);
    int totalLines = (int)lineStarts.size();

    if (deltaVisualLines > 0) {
        int rem = deltaVisualLines;
        while (rem > 0) {
            int subCount = getLineVisualCount(vScrollPos, wrapW);
            if (vScrollSubLine + 1 < subCount) {
                vScrollSubLine++;
            } else if (vScrollPos + 1 < totalLines) {
                vScrollPos++;
                vScrollSubLine = 0;
            } else {
                break;
            }
            rem--;
        }
    } else {
        int rem = -deltaVisualLines;
        while (rem > 0) {
            if (vScrollSubLine > 0) {
                vScrollSubLine--;
            } else if (vScrollPos > 0) {
                vScrollPos--;
                int subCount = getLineVisualCount(vScrollPos, wrapW);
                vScrollSubLine = std::max(0, subCount - 1);
            } else {
                break;
            }
            rem--;
        }
    }
    updateScrollBars();
    if (cbNeedsDisplay) cbNeedsDisplay();
}

void Editor::moveVisualUpDown(bool up, bool extendSelection) {
    if (cursors.empty()) return;
    rollbackPadding();
    isRectSelecting = false;
    float vw = 800.0f, vh = 600.0f;
    if (cbGetViewSize) cbGetViewSize(vw, vh);
    float wrapW = getEffectiveTextWidth(vw);

    for (auto& c : cursors) {
        int li = getLineIdx(c.head);
        size_t lineStart = lineStarts[li];
        size_t relPos = (c.head >= lineStart) ? (c.head - lineStart) : 0;

        if (!wordWrapEnabled) {
            int targetLine = up ? (li - 1) : (li + 1);
            if (targetLine >= 0 && targetLine < (int)lineStarts.size()) {
                c.head = getPosFromLineAndX(targetLine, c.desiredX);
                if (!extendSelection) c.anchor = c.head;
                c.isVirtual = false;
                c.affinity = false;
            }
            continue;
        }

        auto subs = getVisualSubLines(li, wrapW);
        int curSub = 0;
        for (int sIdx = 0; sIdx < (int)subs.size(); ++sIdx) {
            size_t subEnd = subs[sIdx].byteStart + subs[sIdx].byteLen;
            if (relPos < subEnd || (relPos == subEnd && (c.affinity || sIdx == (int)subs.size() - 1))) {
                curSub = sIdx;
                break;
            }
        }
        float subStartX = getXInLine(li, lineStart + subs[curSub].byteStart);
        float relDesiredX = std::max(0.0f, c.desiredX);

        int targetLi = li;
        int targetSub = curSub + (up ? -1 : 1);
        if (targetSub < 0) {
            if (li > 0) {
                targetLi = li - 1;
                auto prevSubs = getVisualSubLines(targetLi, wrapW);
                targetSub = std::max(0, (int)prevSubs.size() - 1);
                subs = std::move(prevSubs);
            } else {
                targetSub = 0;
            }
        } else if (targetSub >= (int)subs.size()) {
            if (li + 1 < (int)lineStarts.size()) {
                targetLi = li + 1;
                subs = getVisualSubLines(targetLi, wrapW);
                targetSub = 0;
            } else {
                targetSub = (int)subs.size() - 1;
            }
        }
        (void)subStartX;
        size_t tLineStart = lineStarts[targetLi];
        float targetSubStartX = getXInLine(targetLi, tLineStart + subs[targetSub].byteStart);
        size_t newPos = getPosFromLineAndX(targetLi, targetSubStartX + relDesiredX);
        size_t subMin = tLineStart + subs[targetSub].byteStart;
        size_t subMax = subMin + subs[targetSub].byteLen;
        newPos = std::clamp(newPos, subMin, subMax);
        c.affinity = (newPos == subMax && targetSub + 1 < (int)subs.size());
        c.head = newPos;
        if (!extendSelection) c.anchor = c.head;
        c.isVirtual = false;
    }
    mergeCursors();
    ensureCaretVisible();
    if (cbNeedsDisplay) cbNeedsDisplay();
}

void Editor::moveVisualHome(bool extendSelection) {
    if (cursors.empty()) return;
    rollbackPadding();
    isRectSelecting = false;
    float vw = 800.0f, vh = 600.0f;
    if (cbGetViewSize) cbGetViewSize(vw, vh);
    float wrapW = getEffectiveTextWidth(vw);

    for (auto& c : cursors) {
        int li = getLineIdx(c.head);
        size_t lineStart = lineStarts[li];
        if (!wordWrapEnabled) {
            c.head = lineStart;
            if (!extendSelection) c.anchor = c.head;
            c.desiredX = 0.0f;
            c.isVirtual = false;
            c.affinity = false;
            continue;
        }
        size_t relPos = (c.head >= lineStart) ? (c.head - lineStart) : 0;
        auto subs = getVisualSubLines(li, wrapW);
        int curSub = 0;
        for (int sIdx = 0; sIdx < (int)subs.size(); ++sIdx) {
            size_t subEnd = subs[sIdx].byteStart + subs[sIdx].byteLen;
            if (relPos < subEnd || (relPos == subEnd && (c.affinity || sIdx == (int)subs.size() - 1))) {
                curSub = sIdx;
                break;
            }
        }
        c.head = lineStart + subs[curSub].byteStart;
        if (!extendSelection) c.anchor = c.head;
        c.desiredX = 0.0f;
        c.isVirtual = false;
        c.affinity = false;
    }
    mergeCursors();
    ensureCaretVisible();
    if (cbNeedsDisplay) cbNeedsDisplay();
}

void Editor::moveVisualEnd(bool extendSelection) {
    if (cursors.empty()) return;
    rollbackPadding();
    isRectSelecting = false;
    float vw = 800.0f, vh = 600.0f;
    if (cbGetViewSize) cbGetViewSize(vw, vh);
    float wrapW = getEffectiveTextWidth(vw);

    for (auto& c : cursors) {
        int li = getLineIdx(c.head);
        size_t lineStart = lineStarts[li];
        size_t lineEnd = (li + 1 < (int)lineStarts.size()) ? lineStarts[li + 1] : pt.length();
        if (lineEnd > lineStart && pt.charAt(lineEnd - 1) == '\n') lineEnd--;
        if (lineEnd > lineStart && pt.charAt(lineEnd - 1) == '\r') lineEnd--;

        if (!wordWrapEnabled) {
            c.head = lineEnd;
            if (!extendSelection) c.anchor = c.head;
            c.desiredX = getXInLine(li, c.head);
            c.isVirtual = false;
            c.affinity = false;
            continue;
        }
        size_t relPos = (c.head >= lineStart) ? (c.head - lineStart) : 0;
        auto subs = getVisualSubLines(li, wrapW);
        int curSub = 0;
        for (int sIdx = 0; sIdx < (int)subs.size(); ++sIdx) {
            size_t subEnd = subs[sIdx].byteStart + subs[sIdx].byteLen;
            if (relPos < subEnd || (relPos == subEnd && (c.affinity || sIdx == (int)subs.size() - 1))) {
                curSub = sIdx;
                break;
            }
        }
        size_t subStartPos = lineStart + subs[curSub].byteStart;
        size_t subEndPos = subStartPos + subs[curSub].byteLen;
        c.head = subEndPos;
        if (!extendSelection) c.anchor = c.head;
        c.desiredX = getXInLine(li, subEndPos) - getXInLine(li, subStartPos);
        c.isVirtual = false;
        c.affinity = (curSub + 1 < (int)subs.size());
    }
    mergeCursors();
    ensureCaretVisible();
    if (cbNeedsDisplay) cbNeedsDisplay();
}

void Editor::showToastPopup(const std::string& text, int durationMs) {
    zoomPopupText = text;
    zoomPopupEndTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(durationMs);
    if (cbNeedsDisplay) cbNeedsDisplay();
}

void Editor::toggleWordWrap() {
    wordWrapEnabled = !wordWrapEnabled;
    hScrollPos = 0;
    vScrollSubLine = 0;
    updateScrollBars();
    ensureCaretVisible();
    saveCurrentViewState();
    showToastPopup(wordWrapEnabled ? "Wrap: ON" : "Wrap: OFF");
}

void Editor::toggleAlwaysOnTop() {
    isAlwaysOnTop = !isAlwaysOnTop;
    if (cbSetAlwaysOnTop) cbSetAlwaysOnTop(isAlwaysOnTop);
    updateTitleBar();
    showToastPopup(isAlwaysOnTop ? "Top: ON" : "Top: OFF");
}

void Editor::togglePreview() {
    showHelpPopup = false;
    showPreview = !showPreview;
    previewInitialLoaded = false;
    if (previewNonce.empty()) {
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::ostringstream oss;
        oss << std::hex << std::setfill('0') << std::setw(16) << gen() << std::setw(16) << gen();
        previewNonce = oss.str();
    }
    if (cbUpdatePreview) cbUpdatePreview();
    updateScrollBars();
    if (cbNeedsDisplay) cbNeedsDisplay();
}

MiuPreview::PreviewRenderResult Editor::getPreviewRenderResult() {
    if (previewNonce.empty()) {
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::ostringstream oss;
        oss << std::hex << std::setfill('0') << std::setw(16) << gen() << std::setw(16) << gen();
        previewNonce = oss.str();
    }
    std::string content = pt.getRange(0, pt.length());
    return MiuPreview::renderPreview(content, currentFileExt, isDarkMode, previewNonce);
}

bool Editor::handlePreviewTaskToggle(int targetLineIdx) {
    if (targetLineIdx < 0 || targetLineIdx >= (int)lineStarts.size()) return false;
    size_t s = lineStarts[targetLineIdx];
    size_t e = (targetLineIdx + 1 < (int)lineStarts.size()) ? lineStarts[targetLineIdx + 1] : pt.length();
    std::string lineStr = pt.getRange(s, e - s);

    size_t bracketPos = lineStr.find('[');
    if (bracketPos == std::string::npos || bracketPos + 2 >= lineStr.size() || lineStr[bracketPos + 2] != ']') {
        return false;
    }
    char curState = lineStr[bracketPos + 1];
    if (curState != ' ' && curState != 'x' && curState != 'X') return false;
    std::string newChar = (curState == ' ') ? "x" : " ";
    size_t docPos = s + bracketPos + 1;

    EditBatch batch;
    batch.beforeCursors = cursors;
    std::string oldChar(1, curState);
    pt.erase(docPos, 1);
    batch.ops.push_back({ EditOp::Erase, docPos, oldChar });
    pt.insert(docPos, newChar);
    batch.ops.push_back({ EditOp::Insert, docPos, newChar });
    batch.afterCursors = cursors;
    undo.push(batch);
    updateDirtyFlag();
    if (cbNeedsDisplay) cbNeedsDisplay();
    if (cbUpdatePreview && showPreview) cbUpdatePreview();
    return true;
}

std::pair<std::string, bool> Editor::getHighlightTarget() {
    if (cursors.empty() || cursors.size() > 1) return { "", false };
    const Cursor& c = cursors.back();
    if (c.hasSelection()) {
        size_t len = c.end() - c.start();
        if (len == 0 || len > 200) return { "", false };
        std::string s = pt.getRange(c.start(), len);
        if (s.empty() || s.find('\n') != std::string::npos) return { "", false };
        return { s, false };
    }
    size_t pos = c.head, len = pt.length();
    if (pos > len) pos = len;
    bool charRight = (pos < len && isWordChar(pt.charAt(pos)));
    bool charLeft = (pos > 0 && isWordChar(pt.charAt(pos - 1)));
    if (!charRight && !charLeft) return { "", true };
    size_t start = pos, end = pos;
    while (start > 0 && isWordChar(pt.charAt(start - 1))) start--;
    while (end < len && isWordChar(pt.charAt(end))) end++;
    if (end > start) return { pt.getRange(start, end - start), true };
    return { "", true };
}

std::string Editor::UnescapeString(const std::string& s, const std::string& newline) {
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

std::string Editor::preprocessRegexQuery(const std::string& query) {
    std::string processed;
    processed.reserve(query.size() * 4);
    for (size_t i = 0; i < query.size(); ++i) {
        char c = query[i];
        if (c == '\\') {
            if (i + 1 < query.size()) {
                char next = query[i + 1];
                if (next == 'n') {
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
        } else if (c == '^') {
            bool inClass = (i > 0 && query[i - 1] == '[');
            if (!inClass) {
                processed += "((?:^|(?:\\r\\n|\\r(?!\\n)|[\\n])))";
                continue;
            }
        } else if (c == '$') {
            bool inClass = (i > 0 && query[i - 1] == '[');
            if (!inClass) {
                processed += "(?=(?:\\r\\n|[\\r\\n]|$))";
                continue;
            }
        }
        processed += c;
    }
    return processed;
}

static const char* fastFindLiteralBuffer(const char* hay, size_t hayLen, const char* needle, size_t needleLen) {
    if (needleLen == 0) return hay;
    if (hayLen < needleLen) return nullptr;
    const char first = needle[0];
    const char last = needle[needleLen - 1];
    size_t limit = hayLen - needleLen + 1;
    size_t i = 0;

#if defined(__AVX2__)
    if (limit >= 32) {
        __m256i vFirst = _mm256_set1_epi8(first);
        __m256i vLast = _mm256_set1_epi8(last);
        for (; i + 32 <= limit; i += 32) {
            __m256i blockFirst = _mm256_loadu_si256((const __m256i*)(hay + i));
            __m256i blockLast = _mm256_loadu_si256((const __m256i*)(hay + i + needleLen - 1));
            uint32_t mask = (uint32_t)_mm256_movemask_epi8(_mm256_and_si256(
                _mm256_cmpeq_epi8(blockFirst, vFirst),
                _mm256_cmpeq_epi8(blockLast, vLast)));
            while (mask != 0) {
#if defined(_MSC_VER)
                unsigned long bit = 0;
                _BitScanForward(&bit, mask);
#else
                int bit = __builtin_ctz(mask);
#endif
                size_t cand = i + (size_t)bit;
                if (needleLen <= 2 || std::memcmp(hay + cand + 1, needle + 1, needleLen - 2) == 0) {
                    return hay + cand;
                }
                mask &= (mask - 1);
            }
        }
    }
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    if (limit >= 16) {
        uint8x16_t vFirst = vdupq_n_u8((uint8_t)first);
        uint8x16_t vLast = vdupq_n_u8((uint8_t)last);
        for (; i + 16 <= limit; i += 16) {
            uint8x16_t bFirst = vld1q_u8((const uint8_t*)(hay + i));
            uint8x16_t bLast = vld1q_u8((const uint8_t*)(hay + i + needleLen - 1));
            uint8x16_t eq = vandq_u8(vceqq_u8(bFirst, vFirst), vceqq_u8(bLast, vLast));
            uint64_t lo = vgetq_lane_u64(vreinterpretq_u64_u8(eq), 0);
            uint64_t hi = vgetq_lane_u64(vreinterpretq_u64_u8(eq), 1);
            if ((lo | hi) != 0) {
                for (size_t k = 0; k < 16; ++k) {
                    size_t cand = i + k;
                    if (hay[cand] == first && hay[cand + needleLen - 1] == last) {
                        if (needleLen <= 2 || std::memcmp(hay + cand + 1, needle + 1, needleLen - 2) == 0) {
                            return hay + cand;
                        }
                    }
                }
            }
        }
    }
#endif
    while (i < limit) {
        const char* p = (const char*)std::memchr(hay + i, (unsigned char)first, limit - i);
        if (!p) break;
        size_t cand = (size_t)(p - hay);
        if (hay[cand + needleLen - 1] == last) {
            if (needleLen <= 2 || std::memcmp(hay + cand + 1, needle + 1, needleLen - 2) == 0) {
                return hay + cand;
            }
        }
        i = cand + 1;
    }
    return nullptr;
}

size_t Editor::findText(size_t startPos, const std::string& query, bool forward, bool matchCase, bool wholeWord, bool isRegex, size_t* outLen) {
    if (query.empty()) return std::string::npos;
    size_t len = pt.length();
    if (len == 0) return std::string::npos;

    if (isRegex) {
        if (!isSafeUserRegex(query)) return std::string::npos;
        std::string actualQuery = preprocessRegexQuery(query);
        if (!isSafeUserRegex(actualQuery)) return std::string::npos;
        std::string fullText = pt.getRange(0, len);
        try {
            std::regex_constants::syntax_option_type flags = std::regex_constants::ECMAScript;
            if (!matchCase) flags |= std::regex_constants::icase;
            std::regex re(actualQuery, flags);
            std::smatch m;
            size_t foundPos = std::string::npos;
            size_t foundLen = 0;
            bool startsWithCaret = (!query.empty() && query[0] == '^');
            bool endsWithDollar = (!query.empty() && query.back() == '$');
            if (forward) {
                if (startPos > fullText.size()) startPos = 0;
                size_t searchStartIdx = startPos;
                std::regex_constants::match_flag_type searchFlags = std::regex_constants::match_default;
                if (searchStartIdx > 0) {
                    searchFlags |= std::regex_constants::match_not_bol;
                    char prevChar = fullText[searchStartIdx - 1];
                    if (prevChar == '\n') {
                        searchStartIdx--;
                        if (searchStartIdx > 0 && fullText[searchStartIdx - 1] == '\r') searchStartIdx--;
                    } else if (prevChar == '\r') {
                        searchStartIdx--;
                    }
                }
                std::string::const_iterator searchStartIter = fullText.begin() + searchStartIdx;
                while (std::regex_search(searchStartIter, fullText.cend(), m, re, searchFlags)) {
                    size_t currentMatchPos = searchStartIdx + m.position();
                    size_t currentMatchLen = m.length();
                    size_t anchorLen = 0;
                    if (startsWithCaret && m.size() > 1 && m[1].matched) anchorLen = m.length(1);
                    size_t contentPos = currentMatchPos + anchorLen;
                    size_t contentLen = currentMatchLen - anchorLen;
                    bool isValid = (contentPos >= startPos);
                    if (isValid && endsWithDollar) {
                        bool isAtLineEnd = false;
                        size_t checkPos = contentPos + contentLen;
                        if (checkPos >= fullText.size()) isAtLineEnd = true;
                        else {
                            char c = fullText[checkPos];
                            if (c == '\r' || c == '\n') {
                                isAtLineEnd = true;
                                if (contentLen == 0 && c == '\n' && checkPos > 0 && fullText[checkPos - 1] == '\r') isAtLineEnd = false;
                            }
                        }
                        if (!isAtLineEnd) isValid = false;
                    }
                    if (isValid) {
                        foundPos = contentPos;
                        foundLen = contentLen;
                        if (startsWithCaret && endsWithDollar && foundLen == 0 && foundPos == fullText.size() && !fullText.empty()) {
                            bool isAfterNewline = (foundPos > 0 && (fullText[foundPos - 1] == '\r' || fullText[foundPos - 1] == '\n'));
                            if (!isAfterNewline) isValid = false;
                        }
                        if (isValid) break;
                    }
                    size_t step = currentMatchLen;
                    if (step == 0) {
                        bool isBolCaret = (startsWithCaret && !(searchFlags & std::regex_constants::match_not_bol));
                        step = isBolCaret ? 0 : 1;
                    }
                    if (step == 0 && (searchFlags & std::regex_constants::match_not_bol)) step = 1;
                    size_t dist = std::distance(searchStartIter, fullText.cend());
                    if ((size_t)m.position() + step > dist) break;
                    size_t advance = m.position() + step;
                    std::advance(searchStartIter, advance);
                    searchStartIdx += advance;
                    searchFlags |= std::regex_constants::match_not_bol;
                }
            } else {
                auto words_begin = std::sregex_iterator(fullText.begin(), fullText.end(), re);
                auto words_end = std::sregex_iterator();
                size_t bestPos = std::string::npos, bestLen = 0;
                size_t limit = (startPos == 0) ? len : startPos;
                for (auto i = words_begin; i != words_end; ++i) {
                    size_t mPos = i->position(), mLen = i->length(), aLen = 0;
                    if (startsWithCaret && i->size() > 1 && (*i)[1].matched) aLen = i->length(1);
                    if (startsWithCaret && aLen == 1 && mPos < fullText.size() && fullText[mPos] == '\r') {
                        if (mPos + 1 < fullText.size() && fullText[mPos + 1] == '\n') continue;
                    }
                    size_t contentStart = mPos + aLen;
                    size_t contentLen = mLen - aLen;
                    if (contentStart < limit) {
                        bestPos = contentStart;
                        bestLen = contentLen;
                    }
                }
                if (bestPos != std::string::npos) {
                    foundPos = bestPos;
                    foundLen = bestLen;
                }
            }
            if (foundPos != std::string::npos) {
                if (outLen) *outLen = foundLen;
                return foundPos;
            }
        } catch (...) { return std::string::npos; }
        return std::string::npos;
    }

    size_t qLen = query.length();
    if (qLen > len) return std::string::npos;
    if (outLen) *outLen = qLen;

    auto isGraphemeSplit = [&](size_t matchPos) -> bool {
        size_t nextPos = matchPos + qLen;
        if (nextPos < len) {
            unsigned char b1 = (unsigned char)pt.charAt(nextPos);
            if (b1 == 0xE2 && nextPos + 2 < len) {
                if ((unsigned char)pt.charAt(nextPos + 1) == 0x80 && (unsigned char)pt.charAt(nextPos + 2) == 0x8D) return true;
            } else if (b1 == 0xEF && nextPos + 2 < len) {
                if ((unsigned char)pt.charAt(nextPos + 1) == 0xB8 && (unsigned char)pt.charAt(nextPos + 2) == 0x8F) return true;
            } else if (b1 == 0xF0 && nextPos + 3 < len) {
                if ((unsigned char)pt.charAt(nextPos + 1) == 0x9F && (unsigned char)pt.charAt(nextPos + 2) == 0x8F) {
                    unsigned char b4 = (unsigned char)pt.charAt(nextPos + 3);
                    if (b4 >= 0xBB && b4 <= 0xBF) return true;
                }
            }
        }
        return false;
    };

    auto checkValidMatch = [&](size_t cand) -> bool {
        if (wholeWord) {
            if (cand > 0 && isWordChar(pt.charAt(cand - 1))) return false;
            if (cand + qLen < len && isWordChar(pt.charAt(cand + qLen))) return false;
        }
        if (isGraphemeSplit(cand)) return false;
        return true;
    };

    // Ultra-fast piece-direct forward scan when matchCase is true
    if (forward && matchCase) {
        size_t curStart = (startPos >= len) ? 0 : startPos;
        size_t pieceDocOffset = 0;
        for (size_t pIdx = 0; pIdx < pt.pieces.size(); ++pIdx) {
            const auto& p = pt.pieces[pIdx];
            size_t pieceEnd = pieceDocOffset + p.len;
            if (pieceEnd <= curStart) {
                pieceDocOffset = pieceEnd;
                continue;
            }
            const char* pData = p.isOriginal ? (pt.origPtr + p.start) : (pt.addBuf.data() + p.start);
            size_t localStart = (curStart > pieceDocOffset) ? (curStart - pieceDocOffset) : 0;
            while (localStart + qLen <= p.len) {
                const char* found = fastFindLiteralBuffer(pData + localStart, p.len - localStart, query.data(), qLen);
                if (!found) break;
                size_t foundLocal = (size_t)(found - pData);
                size_t globalCand = pieceDocOffset + foundLocal;
                if (checkValidMatch(globalCand)) return globalCand;
                localStart = foundLocal + 1;
            }
            // Check boundary spanning into next piece(s)
            size_t boundaryStart = (p.len > qLen - 1) ? std::max(localStart, p.len - (qLen - 1)) : localStart;
            if (boundaryStart < p.len && pieceEnd < len) {
                size_t globalBStart = pieceDocOffset + boundaryStart;
                size_t spanLen = std::min(len - globalBStart, (p.len - boundaryStart) + qLen - 1);
                if (spanLen >= qLen) {
                    std::string span = pt.getRange(globalBStart, spanLen);
                    size_t off = 0;
                    while (off + qLen <= span.size()) {
                        const char* found = fastFindLiteralBuffer(span.data() + off, span.size() - off, query.data(), qLen);
                        if (!found) break;
                        size_t fOff = (size_t)(found - span.data());
                        size_t globalCand = globalBStart + fOff;
                        if (globalCand >= curStart && checkValidMatch(globalCand)) return globalCand;
                        off = fOff + 1;
                    }
                }
            }
            pieceDocOffset = pieceEnd;
        }
        return std::string::npos;
    }

    auto toLower = [](char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + ('a' - 'A')) : c; };
    size_t cur = startPos;
    if (forward) {
        if (cur >= len) cur = 0;
    } else {
        if (cur == 0) cur = len - 1;
        else cur--;
    }

    while (true) {
        if (cur + qLen <= len) {
            bool match = true;
            for (size_t i = 0; i < qLen; ++i) {
                char c1 = pt.charAt(cur + i);
                char c2 = query[i];
                if (!matchCase) { c1 = toLower(c1); c2 = toLower(c2); }
                if (c1 != c2) { match = false; break; }
            }
            if (match && checkValidMatch(cur)) return cur;
        }
        if (forward) {
            cur++;
            if (cur + qLen > len) break;
        } else {
            if (cur == 0) break;
            cur--;
        }
    }
    return std::string::npos;
}

void Editor::findNext(bool forward) {
    if (searchQuery.empty()) return;
    size_t currentCursorPos = forward ? (cursors.empty() ? 0 : cursors.back().end()) : (cursors.empty() ? 0 : cursors.back().start());
    size_t searchStart = currentCursorPos;
    bool hasWrapped = false;
    while (true) {
        size_t matchLen = 0;
        size_t pos = findText(searchStart, searchQuery, forward, searchMatchCase, searchWholeWord, searchRegex, &matchLen);
        if (pos == std::string::npos) {
            if (forward && !hasWrapped) {
                searchStart = 0;
                hasWrapped = true;
                continue;
            } else if (!forward && !hasWrapped) {
                searchStart = pt.length();
                hasWrapped = true;
                continue;
            }
            if (cbBeep) cbBeep();
            return;
        }
        size_t matchEnd = pos + matchLen;
        if (forward && !hasWrapped && matchEnd <= currentCursorPos) {
            size_t nextStart = searchStart + 1;
            if (searchStart + 1 < pt.length() && pt.charAt(searchStart) == '\r' && pt.charAt(searchStart + 1) == '\n') {
                nextStart++;
            }
            searchStart = nextStart;
            if (searchStart > pt.length()) {
                searchStart = 0;
                hasWrapped = true;
            }
            continue;
        }
        cursors.clear();
        cursors.push_back({ matchEnd, pos, getXFromPos(matchEnd), getXFromPos(pos), false, false });
        ensureCaretVisible();
        updateTitleBar();
        if (cbNeedsDisplay) cbNeedsDisplay();
        break;
    }
}

void Editor::replaceNext() {
    if (cursors.empty() || searchQuery.empty()) return;
    rollbackPadding();
    Cursor& c = cursors.back();
    if (!c.hasSelection()) { findNext(true); return; }
    size_t len = c.end() - c.start();
    size_t start = c.start();
    std::string selText = pt.getRange(start, len);
    bool match = false;
    std::string replacement = replaceQuery;
    if (searchRegex) {
        if (!isSafeUserRegex(searchQuery)) return;
        try {
            std::string actualQuery = preprocessRegexQuery(searchQuery);
            if (!isSafeUserRegex(actualQuery)) return;
            std::regex_constants::syntax_option_type flags = std::regex_constants::ECMAScript;
            if (!searchMatchCase) flags |= std::regex_constants::icase;
            std::regex re(actualQuery, flags);
            std::smatch m;
            std::string rawFmt = UnescapeString(replaceQuery, newlineStr);
            std::string fmt;
            for (size_t i = 0; i < rawFmt.size(); ++i) {
                if (rawFmt[i] == '$') {
                    fmt += '$';
                    if (i + 1 < rawFmt.size()) {
                        if (isdigit((unsigned char)rawFmt[i + 1])) {
                            i++;
                            std::string numStr;
                            while (i < rawFmt.size() && isdigit((unsigned char)rawFmt[i])) {
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
            if (std::regex_match(selText, m, re)) {
                match = true;
            } else if (start > 0) {
                int backStep = 0;
                if (pt.charAt(start - 1) == '\n') {
                    backStep = 1;
                    if (start > 1 && pt.charAt(start - 2) == '\r') backStep = 2;
                } else if (pt.charAt(start - 1) == '\r') {
                    backStep = 1;
                }
                if (backStep > 0) {
                    std::string extendedText = pt.getRange(start - backStep, len + backStep);
                    if (std::regex_match(extendedText, m, re)) match = true;
                }
            }
            if (match) replacement = m.format(fmt);
        } catch (...) {}
    } else {
        if (len == searchQuery.length()) {
            match = true;
            for (size_t i = 0; i < len; ++i) {
                char c1 = selText[i], c2 = searchQuery[i];
                if (!searchMatchCase) {
                    c1 = (c1 >= 'A' && c1 <= 'Z') ? (char)(c1 + ('a' - 'A')) : c1;
                    c2 = (c2 >= 'A' && c2 <= 'Z') ? (char)(c2 + ('a' - 'A')) : c2;
                }
                if (c1 != c2) { match = false; break; }
            }
        }
        if (match) replacement = replaceQuery;
    }
    if (match) {
        EditBatch batch;
        batch.beforeCursors = cursors;
        pt.erase(start, len);
        batch.ops.push_back({ EditOp::Erase, start, selText });
        pt.insert(start, replacement);
        batch.ops.push_back({ EditOp::Insert, start, replacement });
        rebuildLineStarts();
        cursors.clear();
        size_t newEnd = start + replacement.size();
        cursors.push_back({ newEnd, start, getXFromPos(newEnd), getXFromPos(start), false, false });
        batch.afterCursors = cursors;
        undo.push(batch);
        ensureCaretVisible();
        updateDirtyFlag();
        if (cbUpdatePreview && showPreview) cbUpdatePreview();
        if (cbNeedsDisplay) cbNeedsDisplay();
        findNext(true);
    } else {
        findNext(true);
    }
}

void Editor::replaceAll() {
    if (searchQuery.empty()) return;
    rollbackPadding();
    isRectSelecting = false;
    struct Match { size_t start; size_t len; std::string replacementText; };
    std::vector<Match> matches;
    size_t docLen = pt.length();
    std::string fullText = pt.getRange(0, docLen);

    if (searchRegex) {
        if (!isSafeUserRegex(searchQuery)) return;
        std::string actualQuery = preprocessRegexQuery(searchQuery);
        if (!isSafeUserRegex(actualQuery)) return;
        std::string rawFmt = UnescapeString(replaceQuery, newlineStr);
        std::string fmt;
        for (size_t i = 0; i < rawFmt.size(); ++i) {
            if (rawFmt[i] == '$') {
                fmt += '$';
                if (i + 1 < rawFmt.size()) {
                    if (isdigit((unsigned char)rawFmt[i + 1])) {
                        i++;
                        std::string numStr;
                        while (i < rawFmt.size() && isdigit((unsigned char)rawFmt[i])) {
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
        try {
            bool startsWithCaret = (!searchQuery.empty() && searchQuery[0] == '^');
            bool endsWithDollar = (!searchQuery.empty() && searchQuery.back() == '$');
            auto searchStart = fullText.cbegin();
            std::smatch m;
            size_t currentOffset = 0;
            std::regex_constants::match_flag_type flags = std::regex_constants::match_default;
            std::regex_constants::syntax_option_type reFlags = std::regex_constants::ECMAScript;
            if (!searchMatchCase) reFlags |= std::regex_constants::icase;
            std::regex cachedRegex(actualQuery, reFlags);
            while (std::regex_search(searchStart, fullText.cend(), m, cachedRegex, flags)) {
                size_t matchPos = currentOffset + m.position();
                size_t matchLen = m.length();
                size_t anchorLen = 0;
                if (startsWithCaret && m.size() > 1 && m[1].matched) anchorLen = m.length(1);
                size_t contentPos = matchPos + anchorLen;
                size_t contentLen = matchLen - anchorLen;
                if (endsWithDollar && contentLen > 0) {
                    if (fullText[contentPos + contentLen - 1] == '\n') {
                        contentLen--;
                        if (contentLen > 0 && fullText[contentPos + contentLen - 1] == '\r') contentLen--;
                    }
                }
                bool isValid = true;
                if (endsWithDollar) {
                    bool isAtLineEnd = false;
                    size_t checkPos = contentPos + contentLen;
                    if (checkPos >= fullText.size()) isAtLineEnd = true;
                    else {
                        char c = fullText[checkPos];
                        if (c == '\r' || c == '\n') {
                            isAtLineEnd = true;
                            if (contentLen == 0 && c == '\n' && checkPos > 0 && fullText[checkPos - 1] == '\r') isAtLineEnd = false;
                        }
                    }
                    if (!isAtLineEnd) isValid = false;
                }
                if (isValid && startsWithCaret && endsWithDollar && contentLen == 0 && contentPos == fullText.size() && !fullText.empty()) {
                    bool isAfterNewline = (contentPos > 0 && (fullText[contentPos - 1] == '\r' || fullText[contentPos - 1] == '\n'));
                    if (!isAfterNewline) isValid = false;
                }
                if (isValid && !matches.empty()) {
                    const auto& last = matches.back();
                    if (last.start == contentPos && last.len == 0 && contentLen == 0) isValid = false;
                }
                if (isValid) {
                    matches.push_back({ contentPos, contentLen, m.format(fmt) });
                }
                size_t step = matchLen;
                if (step == 0) {
                    bool isBolCaret = (startsWithCaret && !(flags & std::regex_constants::match_not_bol));
                    step = isBolCaret ? 0 : 1;
                }
                if (step == 0 && (flags & std::regex_constants::match_not_bol)) step = 1;
                size_t relativeAdvance = m.position() + step;
                size_t remaining = std::distance(searchStart, fullText.cend());
                if (relativeAdvance > remaining) break;
                std::advance(searchStart, relativeAdvance);
                currentOffset += relativeAdvance;
                flags |= std::regex_constants::match_not_bol;
            }
            if (startsWithCaret && fullText.empty() && matches.empty()) {
                matches.push_back({ 0, 0, fmt });
            }
        } catch (...) { return; }
    } else {
        size_t currentPos = 0;
        while (currentPos <= docLen) {
            size_t matchLen = 0;
            size_t pos = findText(currentPos, searchQuery, true, searchMatchCase, searchWholeWord, false, &matchLen);
            if (pos == std::string::npos || pos < currentPos) break;
            matches.push_back({ pos, matchLen, replaceQuery });
            currentPos = pos + std::max((size_t)1, matchLen);
        }
    }

    if (matches.empty()) {
        if (cbBeep) cbBeep();
        return;
    }

    // Single-pass O(N+M) linear replacement construction
    ptrdiff_t totalDelta = 0;
    for (const auto& m : matches) {
        totalDelta += (ptrdiff_t)m.replacementText.size() - (ptrdiff_t)m.len;
    }
    std::string newContent;
    newContent.reserve((size_t)std::max<ptrdiff_t>(0, (ptrdiff_t)fullText.size() + totalDelta));
    size_t prevEnd = 0;
    size_t lastReplaceStart = 0;
    size_t lastReplaceEnd = 0;
    for (size_t i = 0; i < matches.size(); ++i) {
        const auto& m = matches[i];
        if (m.start > prevEnd) {
            newContent.append(fullText.data() + prevEnd, m.start - prevEnd);
        }
        if (i + 1 == matches.size()) {
            lastReplaceStart = newContent.size();
        }
        newContent.append(m.replacementText);
        if (i + 1 == matches.size()) {
            lastReplaceEnd = newContent.size();
        }
        prevEnd = m.start + m.len;
    }
    if (prevEnd < fullText.size()) {
        newContent.append(fullText.data() + prevEnd, fullText.size() - prevEnd);
    }

    EditBatch batch;
    batch.beforeCursors = cursors;
    if (!fullText.empty()) {
        pt.erase(0, fullText.size());
        batch.ops.push_back({ EditOp::Erase, 0, fullText });
    }
    if (!newContent.empty()) {
        pt.insert(0, newContent);
        batch.ops.push_back({ EditOp::Insert, 0, newContent });
    }
    rebuildLineStarts();
    cursors.clear();
    cursors.push_back({ lastReplaceEnd, lastReplaceStart, getXFromPos(lastReplaceEnd), getXFromPos(lastReplaceStart), false, false });
    batch.afterCursors = cursors;
    undo.push(batch);
    ensureCaretVisible();
    updateDirtyFlag();
    if (cbUpdatePreview && showPreview) cbUpdatePreview();
    if (cbNeedsDisplay) cbNeedsDisplay();
    if (cbShowReplaceAlert) cbShowReplaceAlert((int)matches.size());
}

void Editor::selectNextOccurrence() {
    if (cursors.empty()) return;
    rollbackPadding();
    isRectSelecting = false;
    Cursor c = cursors.back();
    if (!c.hasSelection()) {
        size_t targetPos = c.head;
        if (targetPos > 0) {
            char currChar = targetPos < pt.length() ? pt.charAt(targetPos) : '\0';
            char prevChar = pt.charAt(targetPos - 1);
            if (!isWordChar(currChar) && isWordChar(prevChar)) targetPos--;
        }
        size_t s, e;
        getWordBoundaries(targetPos, s, e);
        if (s != e) {
            cursors.clear();
            cursors.push_back({ e, s, getXFromPos(e), getXFromPos(s), false, false });
        }
        return;
    }
    size_t start = c.start();
    size_t len = c.end() - start;
    std::string query = pt.getRange(start, len);
    size_t nextPos = findText(std::max(c.head, c.anchor), query, true, true, false, false);
    if (nextPos == std::string::npos) {
        nextPos = findText(0, query, true, true, false, false);
    }
    if (nextPos != std::string::npos) {
        for (const auto& cur : cursors) { if (cur.start() == nextPos) return; }
        size_t newHead = nextPos + len;
        cursors.push_back({ newHead, nextPos, getXFromPos(newHead), getXFromPos(nextPos), false, false });
        ensureCaretVisible();
        if (cbNeedsDisplay) cbNeedsDisplay();
    }
}

void Editor::updateTitleBar() {
    if (cbUpdateTitleBar) cbUpdateTitleBar();
}

void Editor::updateScrollBars() {
    if (cbUpdateScrollers) cbUpdateScrollers();
}

void Editor::updateDirtyFlag() {
    bool nd = undo.isModified();
    if (isDirty != nd) {
        isDirty = nd;
        updateTitleBar();
    }
}

void Editor::updateFont(float s) {
    float oldCharWidth = charWidth;
    s = std::clamp(s, 6.0f, 200.0f);
    currentFontSize = s;
    lineHeight = std::ceil(s * 1.4f);
#if defined(__APPLE__)
    if (fontRef) CFRelease(fontRef);
    fontRef = CTFontCreateWithName(CFSTR("Menlo"), s, NULL);
    UniChar c = ' ';
    CGGlyph g;
    CGSize adv;
    CTFontGetGlyphsForCharacters(fontRef, &c, &g, 1);
    CTFontGetAdvancesForGlyphs(fontRef, kCTFontOrientationHorizontal, &g, &adv, 1);
    charWidth = (float)adv.width;
    tabWidth = charWidth * 4.0f;
    if (paragraphStyle) CFRelease(paragraphStyle);
    CGFloat tabInterval = (CGFloat)tabWidth;
    CGFloat fixedLineH = (CGFloat)lineHeight;
    CFArrayRef emptyTabStops = CFArrayCreate(NULL, NULL, 0, NULL);
    CTParagraphStyleSetting settings[] = {
        { kCTParagraphStyleSpecifierDefaultTabInterval, sizeof(CGFloat), &tabInterval },
        { kCTParagraphStyleSpecifierTabStops, sizeof(CFArrayRef), &emptyTabStops },
        { kCTParagraphStyleSpecifierMinimumLineHeight, sizeof(CGFloat), &fixedLineH },
        { kCTParagraphStyleSpecifierMaximumLineHeight, sizeof(CGFloat), &fixedLineH }
    };
    paragraphStyle = CTParagraphStyleCreate(settings, 4);
    CFRelease(emptyTabStops);
#else
    charWidth = std::max(4.0f, std::round(s * 0.6f));
    tabWidth = charWidth * 4.0f;
#endif
    if (oldCharWidth > 0.0f && charWidth > 0.0f) {
        float ratio = charWidth / oldCharWidth;
        for (auto& cur : cursors) {
            cur.desiredX *= ratio;
            cur.originalAnchorX *= ratio;
        }
    }
    updateGutterWidth();
    updateMaxLineWidth();
    updateScrollBars();
}

void Editor::rebuildLineStarts() {
    lineStarts.clear();
    lineStarts.push_back(0);
    maxLineBytes = 0;
    maxLineBytesIdx = 0;
    size_t go = 0;
    size_t curLineStart = 0;

    for (const auto& p : pt.pieces) {
        const char* b = p.isOriginal ? (pt.origPtr + p.start) : (pt.addBuf.data() + p.start);
        size_t i = 0;
        while (i < p.len) {
            const char* nl = (const char*)std::memchr(b + i, '\n', p.len - i);
            if (!nl) break;
            size_t idxInPiece = (size_t)(nl - b);
            size_t nlDocPos = go + idxInPiece;
            size_t bLen = nlDocPos - curLineStart;
            if (bLen > maxLineBytes) {
                maxLineBytes = bLen;
                maxLineBytesIdx = (int)lineStarts.size() - 1;
            }
            curLineStart = nlDocPos + 1;
            lineStarts.push_back(curLineStart);
            i = idxInPiece + 1;
        }
        go += p.len;
    }
    size_t lastLineBytes = (go >= curLineStart) ? (go - curLineStart) : 0;
    if (lastLineBytes > maxLineBytes) {
        maxLineBytes = lastLineBytes;
        maxLineBytesIdx = (int)lineStarts.size() - 1;
    }
    commentStateCache.clear();
    updateGutterWidth();
    maxLineWidth = std::max(100.0f, measureLineWidth(maxLineBytesIdx));
    updateScrollBars();
}

int Editor::getLineIdx(size_t pos) {
    if (lineStarts.empty()) return 0;
    auto it = std::upper_bound(lineStarts.begin(), lineStarts.end(), pos);
    return std::max(0, (int)std::distance(lineStarts.begin(), it) - 1);
}

float Editor::getXInLine(int li, size_t pos) {
    if (li < 0 || li >= (int)lineStarts.size()) return 0.0f;
    size_t s = lineStarts[li], e = (li + 1 < (int)lineStarts.size()) ? lineStarts[li + 1] : pt.length();
    if (e > s && pt.charAt(e - 1) == '\n') e--;
    if (e > s && pt.charAt(e - 1) == '\r') e--;
    std::string lstr = pt.getRange(s, e - s);
    size_t rp = std::clamp(pos, s, e) - s;
    if (!imeComp.empty() && !cursors.empty() && getLineIdx(cursors.back().head) == li) {
        size_t cp = cursors.back().head;
        if (cp >= s && cp <= e) {
            lstr.insert(cp - s, imeComp);
            if (pos >= cp) rp += imeComp.size();
        }
    }
    if (lstr.empty() || rp == 0) return 0.0f;
#if defined(__APPLE__)
    if (fontRef && paragraphStyle) {
        CFStringRef cf = CFStringCreateWithBytes(NULL, (const UInt8*)lstr.data(), lstr.size(), kCFStringEncodingUTF8, false);
        if (!cf) return 0.0f;
        const void* keys[] = { kCTFontAttributeName, kCTParagraphStyleAttributeName };
        const void* values[] = { fontRef, paragraphStyle };
        CFDictionaryRef d = CFDictionaryCreate(NULL, keys, values, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFAttributedStringRef as = CFAttributedStringCreate(NULL, cf, d);
        CTLineRef line = CTLineCreateWithAttributedString(as);
        CFStringRef sub = CFStringCreateWithBytes(NULL, (const UInt8*)lstr.data(), std::min(rp, lstr.size()), kCFStringEncodingUTF8, false);
        CGFloat x = CTLineGetOffsetForStringIndex(line, sub ? CFStringGetLength(sub) : 0, NULL);
        if (sub) CFRelease(sub);
        CFRelease(line);
        CFRelease(as);
        CFRelease(d);
        CFRelease(cf);
        return (float)x;
    }
#endif
    float x = 0.0f;
    size_t i = 0;
    size_t target = std::min(rp, lstr.size());
    while (i < target) {
        unsigned char c = (unsigned char)lstr[i];
        if (c == '\t') {
            x = (std::floor(x / tabWidth) + 1.0f) * tabWidth;
            i++;
        } else if (c < 0x80) {
            x += charWidth;
            i++;
        } else if ((c & 0xE0) == 0xC0) {
            x += charWidth;
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            x += charWidth * 2.0f;
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            x += charWidth * 2.0f;
            i += 4;
        } else {
            x += charWidth;
            i++;
        }
    }
    return x;
}

float Editor::getXFromPos(size_t p) {
    return getXInLine(getLineIdx(p), p);
}

size_t Editor::getPosFromLineAndX(int li, float tx) {
    if (li < 0 || li >= (int)lineStarts.size()) return cursors.empty() ? 0 : cursors.back().head;
    size_t s = lineStarts[li], e = (li + 1 < (int)lineStarts.size()) ? lineStarts[li + 1] : pt.length();
    if (e > s && pt.charAt(e - 1) == '\n') e--;
    if (e > s && pt.charAt(e - 1) == '\r') e--;
    std::string lstr = pt.getRange(s, e - s);
    if (lstr.empty() || tx <= 0.0f) return s;
#if defined(__APPLE__)
    if (fontRef && paragraphStyle) {
        CFStringRef cf = CFStringCreateWithBytes(NULL, (const UInt8*)lstr.data(), lstr.size(), kCFStringEncodingUTF8, false);
        if (!cf) return s;
        const void* keys[] = { kCTFontAttributeName, kCTParagraphStyleAttributeName };
        const void* values[] = { fontRef, paragraphStyle };
        CFDictionaryRef d = CFDictionaryCreate(NULL, keys, values, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFAttributedStringRef as = CFAttributedStringCreate(NULL, cf, d);
        CTLineRef line = CTLineCreateWithAttributedString(as);
        CFIndex ci = CTLineGetStringIndexForPosition(line, CGPointMake(tx, 0));
        CFIndex bytes = 0;
        if (ci > 0) {
            CFStringRef sub = CFStringCreateWithSubstring(NULL, cf, CFRangeMake(0, std::min(ci, CFStringGetLength(cf))));
            if (sub) {
                CFStringGetBytes(sub, CFRangeMake(0, CFStringGetLength(sub)), kCFStringEncodingUTF8, 0, false, NULL, 0, &bytes);
                CFRelease(sub);
            }
        }
        CFRelease(line);
        CFRelease(as);
        CFRelease(d);
        CFRelease(cf);
        return s + (size_t)bytes;
    }
#endif
    float x = 0.0f;
    size_t i = 0;
    while (i < lstr.size()) {
        unsigned char c = (unsigned char)lstr[i];
        size_t step = 1;
        float cw = charWidth;
        if (c == '\t') {
            float nextTab = (std::floor(x / tabWidth) + 1.0f) * tabWidth;
            cw = nextTab - x;
        } else if (c < 0x80) {
            step = 1; cw = charWidth;
        } else if ((c & 0xE0) == 0xC0) {
            step = 2; cw = charWidth;
        } else if ((c & 0xF0) == 0xE0) {
            step = 3; cw = charWidth * 2.0f;
        } else if ((c & 0xF8) == 0xF0) {
            step = 4; cw = charWidth * 2.0f;
        }
        step = std::min(step, lstr.size() - i);
        if (x + cw * 0.5f >= tx) break;
        x += cw;
        i += step;
    }
    return s + i;
}

size_t Editor::getDocPosFromPoint(float x, float y) {
    if (lineStarts.empty()) return 0;
    if (!wordWrapEnabled) {
        float absY = y + (float)vScrollPos * lineHeight;
        int li = std::clamp((int)std::floor(absY / lineHeight), 0, (int)lineStarts.size() - 1);
        return getPosFromLineAndX(li, x - gutterWidth + (float)hScrollPos);
    }
    float vw = 800.0f, vh = 600.0f;
    if (cbGetViewSize) cbGetViewSize(vw, vh);
    float wrapW = getEffectiveTextWidth(vw);
    int visualRowOffset = std::max(0, (int)std::floor(y / lineHeight));
    int curLi = std::clamp(vScrollPos, 0, (int)lineStarts.size() - 1);
    int curSub = std::max(0, vScrollSubLine);
    while (visualRowOffset > 0 && curLi < (int)lineStarts.size()) {
        auto subs = getVisualSubLines(curLi, wrapW);
        int remInLine = (int)subs.size() - 1 - curSub;
        if (visualRowOffset <= remInLine) {
            curSub += visualRowOffset;
            visualRowOffset = 0;
            break;
        }
        visualRowOffset -= (remInLine + 1);
        if (curLi + 1 < (int)lineStarts.size()) {
            curLi++;
            curSub = 0;
        } else {
            curSub = (int)subs.size() - 1;
            break;
        }
    }
    auto subs = getVisualSubLines(curLi, wrapW);
    curSub = std::clamp(curSub, 0, (int)subs.size() - 1);
    size_t lineStart = lineStarts[curLi];
    size_t subStartPos = lineStart + subs[curSub].byteStart;
    size_t subEndPos = subStartPos + subs[curSub].byteLen;
    float subStartX = getXInLine(curLi, subStartPos);
    size_t pos = getPosFromLineAndX(curLi, subStartX + std::max(0.0f, x - gutterWidth));
    return std::clamp(pos, subStartPos, subEndPos);
}

void Editor::ensureCaretVisible() {
    if (cursors.empty()) return;
    Cursor& c = cursors.back();
    float viewWidth = 800.0f, viewHeight = 600.0f;
    if (cbGetViewSize) cbGetViewSize(viewWidth, viewHeight);
    if (viewWidth <= 0.0f) viewWidth = 800.0f;
    if (viewHeight <= 0.0f) viewHeight = 600.0f;

    float ch = std::max(lineHeight, viewHeight - visibleHScrollHeight);
    float cw = std::max(charWidth * 4.0f, viewWidth - gutterWidth - visibleVScrollWidth);
    int li = getLineIdx(c.head);
    int vis = std::max(1, (int)(ch / lineHeight));

    if (wordWrapEnabled) {
        hScrollPos = 0;
        float wrapW = getEffectiveTextWidth(viewWidth);
        auto subs = getVisualSubLines(li, wrapW);
        size_t relPos = (c.head >= lineStarts[li]) ? (c.head - lineStarts[li]) : 0;
        int caretSub = 0;
        for (int sIdx = 0; sIdx < (int)subs.size(); ++sIdx) {
            size_t se = subs[sIdx].byteStart + subs[sIdx].byteLen;
            if (relPos < se || (relPos == se && (c.affinity || sIdx == (int)subs.size() - 1))) {
                caretSub = sIdx;
                break;
            }
        }
        if (li < vScrollPos || (li == vScrollPos && caretSub < vScrollSubLine)) {
            vScrollPos = li;
            vScrollSubLine = caretSub;
        } else {
            if (li > vScrollPos + vis) {
                vScrollPos = li - vis;
                vScrollSubLine = 0;
            }
            int visDist = 0;
            for (int i = vScrollPos; i < li; ++i) {
                int sc = getLineVisualCount(i, wrapW);
                visDist += (i == vScrollPos) ? std::max(0, sc - vScrollSubLine) : sc;
            }
            visDist += (li == vScrollPos) ? (caretSub - vScrollSubLine) : caretSub;
            if (visDist >= vis) {
                int excess = visDist - vis + 1;
                while (excess > 0) {
                    int sc = getLineVisualCount(vScrollPos, wrapW);
                    if (vScrollSubLine + 1 < sc) {
                        vScrollSubLine++;
                    } else if (vScrollPos + 1 < (int)lineStarts.size()) {
                        vScrollPos++;
                        vScrollSubLine = 0;
                    } else {
                        break;
                    }
                    excess--;
                }
            }
        }
        vScrollPos = std::max(0, vScrollPos);
        vScrollSubLine = std::max(0, vScrollSubLine);
        ScrollBarLayout layout = getScrollBarLayout(viewWidth, viewHeight);
        visibleVScrollWidth = layout.vBarW;
        visibleHScrollHeight = layout.hBarH;
        updateScrollBars();
        return;
    }

    vScrollSubLine = 0;
    if (li < vScrollPos) vScrollPos = li;
    else if (li >= vScrollPos + vis) vScrollPos = li - vis + 1;
    vScrollPos = std::max(0, vScrollPos);

    // v1.0.44 precision horizontal scrollbar control
    float caretX = c.isVirtual ? c.desiredX : getXFromPos(c.head);
    float margin = charWidth * 3.0f;
    if (cw > margin * 2.0f) {
        if (caretX < (float)hScrollPos + margin) {
            hScrollPos = (int)(caretX - margin);
        } else if (caretX > (float)hScrollPos + cw - margin) {
            hScrollPos = (int)(caretX - cw + margin);
        }
    }
    if (hScrollPos < 0) hScrollPos = 0;

    maxLineWidth = std::max(100.0f, measureLineWidth(maxLineBytesIdx));
    if (hScrollPos > 0) {
        float requiredWidth = caretX + margin;
        if (requiredWidth > maxLineWidth) {
            maxLineWidth = requiredWidth;
        }
    }
    int maxHScroll = std::max(0, (int)std::ceil(maxLineWidth - cw));
    if (hScrollPos > maxHScroll) hScrollPos = maxHScroll;

    ScrollBarLayout layout = getScrollBarLayout(viewWidth, viewHeight);
    visibleVScrollWidth = layout.vBarW;
    visibleHScrollHeight = layout.hBarH;
    updateScrollBars();
}

static uint32_t decodeUtf8CodePointAt(const PieceTable& pt, size_t pos, size_t len, size_t& outByteLen) {
    if (pos >= len) { outByteLen = 0; return 0; }
    unsigned char b0 = (unsigned char)pt.charAt(pos);
    if (b0 < 0x80) { outByteLen = 1; return b0; }
    if ((b0 & 0xE0) == 0xC0 && pos + 1 < len) {
        outByteLen = 2;
        return ((uint32_t)(b0 & 0x1F) << 6) | ((uint32_t)((unsigned char)pt.charAt(pos + 1) & 0x3F));
    }
    if ((b0 & 0xF0) == 0xE0 && pos + 2 < len) {
        outByteLen = 3;
        return ((uint32_t)(b0 & 0x0F) << 12) |
               ((uint32_t)((unsigned char)pt.charAt(pos + 1) & 0x3F) << 6) |
               ((uint32_t)((unsigned char)pt.charAt(pos + 2) & 0x3F));
    }
    if ((b0 & 0xF8) == 0xF0 && pos + 3 < len) {
        outByteLen = 4;
        return ((uint32_t)(b0 & 0x07) << 18) |
               ((uint32_t)((unsigned char)pt.charAt(pos + 1) & 0x3F) << 12) |
               ((uint32_t)((unsigned char)pt.charAt(pos + 2) & 0x3F) << 6) |
               ((uint32_t)((unsigned char)pt.charAt(pos + 3) & 0x3F));
    }
    outByteLen = 1;
    return b0;
}

static size_t prevUtf8CodePointStart(const PieceTable& pt, size_t pos) {
    if (pos == 0) return 0;
    size_t p = pos - 1;
    int safety = 0;
    while (p > 0 && ((unsigned char)pt.charAt(p) & 0xC0) == 0x80 && safety < 4) {
        p--;
        safety++;
    }
    return p;
}

static bool isExtendingCodePoint(uint32_t cp) {
    if (cp == 0xFE0E || cp == 0xFE0F) return true; // Variation Selectors
    if (cp >= 0xE0100 && cp <= 0xE01EF) return true; // Variation Selectors Supplement
    if (cp >= 0x1F3FB && cp <= 0x1F3FF) return true; // Emoji Skin Tone Modifiers
    if (cp >= 0x0300 && cp <= 0x036F) return true; // Combining Diacritical Marks
    if (cp >= 0x1AB0 && cp <= 0x1AFF) return true;
    if (cp >= 0x1DC0 && cp <= 0x1DFF) return true;
    if (cp >= 0x20D0 && cp <= 0x20FF) return true; // Combining Marks for Symbols (incl. 0x20E3 keycap)
    if (cp >= 0xFE20 && cp <= 0xFE2F) return true;
    if (cp == 0x3099 || cp == 0x309A) return true; // Japanese combining voiced/semi-voiced marks
    if (cp >= 0xE0020 && cp <= 0xE007F) return true; // Emoji Tag sequence characters
    return false;
}

static bool isRegionalIndicator(uint32_t cp) {
    return (cp >= 0x1F1E6 && cp <= 0x1F1FF);
}

size_t Editor::moveCaretVisual(size_t pos, bool f) {
    size_t len = pt.length();
    if (f) {
        if (pos >= len) return len;
        if (pt.charAt(pos) == '\r' && pos + 1 < len && pt.charAt(pos + 1) == '\n') {
            return pos + 2;
        }
        size_t cpLen = 1;
        uint32_t cp = decodeUtf8CodePointAt(pt, pos, len, cpLen);
        size_t cur = std::min(len, pos + cpLen);

        if (isRegionalIndicator(cp) && cur < len) {
            size_t nextLen = 1;
            uint32_t nextCp = decodeUtf8CodePointAt(pt, cur, len, nextLen);
            if (isRegionalIndicator(nextCp)) {
                cur = std::min(len, cur + nextLen);
            }
        }

        while (cur < len) {
            size_t nextLen = 1;
            uint32_t nextCp = decodeUtf8CodePointAt(pt, cur, len, nextLen);
            if (isExtendingCodePoint(nextCp)) {
                cur = std::min(len, cur + nextLen);
                continue;
            }
            if (nextCp == 0x200D) { // Zero Width Joiner
                cur = std::min(len, cur + nextLen);
                if (cur < len) {
                    size_t joinedLen = 1;
                    decodeUtf8CodePointAt(pt, cur, len, joinedLen);
                    cur = std::min(len, cur + joinedLen);
                }
                continue;
            }
            break;
        }
        return cur;
    } else {
        if (pos == 0) return 0;
        if (pos >= 2 && pt.charAt(pos - 2) == '\r' && pt.charAt(pos - 1) == '\n') {
            return pos - 2;
        }
        size_t cur = prevUtf8CodePointStart(pt, pos);
        while (cur > 0) {
            size_t cpLen = 1;
            uint32_t cp = decodeUtf8CodePointAt(pt, cur, len, cpLen);
            if (isExtendingCodePoint(cp)) {
                cur = prevUtf8CodePointStart(pt, cur);
                continue;
            }
            size_t prevPos = prevUtf8CodePointStart(pt, cur);
            size_t prevLen = 1;
            uint32_t prevCp = decodeUtf8CodePointAt(pt, prevPos, len, prevLen);
            if (prevCp == 0x200D && prevPos > 0) { // Preceded by ZWJ
                cur = prevUtf8CodePointStart(pt, prevPos);
                continue;
            }
            if (isRegionalIndicator(cp) && isRegionalIndicator(prevCp)) {
                cur = prevPos;
                break;
            }
            break;
        }
        return cur;
    }
}

void Editor::insertAtCursors(const std::string& t) {
    if (cursors.empty()) return;
    if (isRectSelecting || !pendingPadding.empty()) {
        insertAtCursorsWithPadding(t);
        return;
    }
    EditBatch b;
    b.beforeCursors = cursors;
    bool canFastSingle = (cursors.size() == 1 && !cursors[0].hasSelection() && canFastAdjustLineStarts(t));
    size_t singleEditPos = cursors[0].head;

    std::vector<size_t> indices(cursors.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(), [&](size_t x, size_t y) { return cursors[x].start() > cursors[y].start(); });

    for (size_t idx : indices) {
        Cursor& c = cursors[idx];
        size_t st = c.start(), l = c.end() - st;
        if (l > 0) {
            canFastSingle = false;
            b.ops.push_back({ EditOp::Erase, st, pt.getRange(st, l) });
            pt.erase(st, l);
            for (auto& o : cursors) {
                if (o.head > st) o.head -= std::min(o.head - st, l);
                if (o.anchor > st) o.anchor -= std::min(o.anchor - st, l);
            }
        } else if (isOverwriteMode && !t.empty() && t != "\n" && t != "\r" && t != "\r\n") {
            char ch = (st < pt.length()) ? pt.charAt(st) : 0;
            if (ch != 0 && ch != '\n' && ch != '\r') {
                size_t nextPos = moveCaretVisual(st, true);
                size_t charLen = (nextPos > st) ? (nextPos - st) : 0;
                if (charLen > 0) {
                    canFastSingle = false;
                    std::string del = pt.getRange(st, charLen);
                    b.ops.push_back({ EditOp::Erase, st, del });
                    pt.erase(st, charLen);
                    for (auto& o : cursors) {
                        if (o.head > st) o.head -= std::min(o.head - st, charLen);
                        if (o.anchor > st) o.anchor -= std::min(o.anchor - st, charLen);
                    }
                }
            }
        }
        if (!t.empty()) {
            pt.insert(st, t);
            b.ops.push_back({ EditOp::Insert, st, t });
            for (auto& o : cursors) {
                if (o.head >= st) o.head += t.size();
                if (o.anchor >= st) o.anchor += t.size();
            }
        }
        c.anchor = c.head;
    }
    if (canFastSingle) {
        adjustLineStartsSingleLine(singleEditPos, (ptrdiff_t)t.size());
    } else {
        rebuildLineStarts();
    }
    for (auto& c : cursors) {
        c.desiredX = getXFromPos(c.head);
        c.originalAnchorX = c.desiredX;
        c.isVirtual = false;
        c.affinity = false;
    }
    b.afterCursors = cursors;
    undo.push(b);
    applyBatchToEmmetTabstops(b);
    ensureCaretVisible();
    updateDirtyFlag();
    if (cbUpdatePreview && showPreview) cbUpdatePreview();
}

void Editor::backspaceAtCursors() {
    if (cursors.empty()) return;
    rollbackPadding();
    isRectSelecting = false;
    EditBatch b;
    b.beforeCursors = cursors;
    std::vector<size_t> indices(cursors.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(), [&](size_t a, size_t x) { return cursors[a].start() > cursors[x].start(); });
    bool changed = false;
    bool allSingleLine = (cursors.size() == 1);
    size_t singleErasePos = 0;
    ptrdiff_t singleEraseDelta = 0;

    for (size_t idx : indices) {
        Cursor& c = cursors[idx];
        if (c.hasSelection()) {
            size_t st = c.start(), len = c.end() - st;
            std::string del = pt.getRange(st, len);
            if (!canFastAdjustLineStarts(del)) allSingleLine = false;
            b.ops.push_back({ EditOp::Erase, st, del });
            pt.erase(st, len);
            changed = true;
            singleErasePos = st;
            singleEraseDelta -= (ptrdiff_t)len;
            for (auto& o : cursors) {
                if (&o == &c) continue;
                if (o.head > st) o.head = (o.head >= st + len) ? o.head - len : st;
                if (o.anchor > st) o.anchor = (o.anchor >= st + len) ? o.anchor - len : st;
            }
            c.head = st;
            c.anchor = st;
            c.isVirtual = false;
            c.affinity = false;
            continue;
        }
        int li = getLineIdx(c.head);
        float physX = getXInLine(li, c.head);
        if (c.isVirtual && c.desiredX > physX + 0.1f) {
            c.desiredX = std::max(physX, c.desiredX - charWidth);
            c.originalAnchorX = c.desiredX;
            if (c.desiredX <= physX + 0.1f) {
                c.desiredX = physX;
                c.isVirtual = false;
            }
            continue;
        }
        if (c.head > 0) {
            size_t st = c.head, prev = moveCaretVisual(st, false), len = st - prev;
            if (len > 0) {
                std::string del = pt.getRange(prev, len);
                if (!canFastAdjustLineStarts(del)) allSingleLine = false;
                b.ops.push_back({ EditOp::Erase, prev, del });
                pt.erase(prev, len);
                changed = true;
                singleErasePos = prev;
                singleEraseDelta -= (ptrdiff_t)len;
                for (auto& o : cursors) {
                    if (&o == &c) continue;
                    if (o.head >= st) o.head -= len;
                    else if (o.head > prev) o.head = prev;
                    if (o.anchor >= st) o.anchor -= len;
                    else if (o.anchor > prev) o.anchor = prev;
                }
                c.head = prev;
                c.anchor = prev;
                c.isVirtual = false;
                c.affinity = false;
            }
        }
    }
    if (changed) {
        if (allSingleLine) {
            adjustLineStartsSingleLine(singleErasePos, singleEraseDelta);
        } else {
            rebuildLineStarts();
        }
        for (auto& c : cursors) {
            c.desiredX = getXFromPos(c.head);
            c.originalAnchorX = c.desiredX;
        }
        mergeCursors();
        b.afterCursors = cursors;
        undo.push(b);
        applyBatchToEmmetTabstops(b);
        updateDirtyFlag();
        if (cbUpdatePreview && showPreview) cbUpdatePreview();
    }
    ensureCaretVisible();
    if (cbNeedsDisplay) cbNeedsDisplay();
}

void Editor::deleteForwardAtCursors() {
    if (cursors.empty()) return;
    rollbackPadding();
    isRectSelecting = false;
    EditBatch b;
    b.beforeCursors = cursors;
    std::vector<size_t> indices(cursors.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(), [&](size_t a, size_t x) { return cursors[a].start() > cursors[x].start(); });
    bool changed = false;
    bool allSingleLine = (cursors.size() == 1);
    size_t singleErasePos = 0;
    ptrdiff_t singleEraseDelta = 0;

    for (size_t idx : indices) {
        Cursor& c = cursors[idx];
        size_t st = c.start(), len = 0;
        if (c.hasSelection()) len = c.end() - st;
        else {
            size_t next = moveCaretVisual(st, true);
            len = next - st;
        }
        if (len > 0) {
            std::string del = pt.getRange(st, len);
            if (!canFastAdjustLineStarts(del)) allSingleLine = false;
            b.ops.push_back({ EditOp::Erase, st, del });
            pt.erase(st, len);
            changed = true;
            singleErasePos = st;
            singleEraseDelta -= (ptrdiff_t)len;
            for (auto& o : cursors) {
                if (o.head > st + len) o.head -= len;
                else if (o.head > st) o.head = st;
                if (o.anchor > st + len) o.anchor -= len;
                else if (o.anchor > st) o.anchor = st;
            }
            c.head = st;
            c.anchor = st;
        }
        c.isVirtual = false;
        c.affinity = false;
    }
    if (changed) {
        if (allSingleLine) {
            adjustLineStartsSingleLine(singleErasePos, singleEraseDelta);
        } else {
            rebuildLineStarts();
        }
        for (auto& c : cursors) {
            c.desiredX = getXFromPos(c.head);
            c.originalAnchorX = c.desiredX;
        }
        mergeCursors();
        b.afterCursors = cursors;
        undo.push(b);
        applyBatchToEmmetTabstops(b);
        ensureCaretVisible();
        updateDirtyFlag();
        if (cbUpdatePreview && showPreview) cbUpdatePreview();
    }
}

void Editor::selectAll() {
    rollbackPadding();
    isRectSelecting = false;
    cursors.clear();
    size_t len = pt.length();
    cursors.push_back({ len, 0, getXFromPos(len), 0.0f, false, false });
}

void Editor::jumpToFileEdge(bool start, bool select) {
    rollbackPadding();
    isRectSelecting = false;
    size_t target = start ? 0 : pt.length();
    float targetX = start ? 0.0f : getXFromPos(target);
    if (!select) {
        cursors.clear();
        cursors.push_back({ target, target, targetX, targetX, false, false });
    } else {
        for (auto& c : cursors) {
            c.head = target;
            c.desiredX = targetX;
            c.originalAnchorX = targetX;
            c.isVirtual = false;
            c.affinity = false;
        }
    }
    if (start) {
        vScrollPos = 0;
        vScrollSubLine = 0;
        hScrollPos = 0;
    }
    ensureCaretVisible();
    updateScrollBars();
    updateDirtyFlag();
    if (cbNeedsDisplay) cbNeedsDisplay();
}

void Editor::convertSelectedText(bool toUpper) {
    if (cursors.empty()) return;
    rollbackPadding();
    EditBatch batch;
    batch.beforeCursors = cursors;
    bool changed = false;
    std::vector<size_t> indices(cursors.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
        return cursors[a].start() > cursors[b].start();
    });
    for (size_t idx : indices) {
        Cursor& c = cursors[idx];
        if (!c.hasSelection()) continue;
        size_t start = c.start();
        size_t len = c.end() - start;
        bool isHeadStart = (c.head <= c.anchor);
        std::string text = pt.getRange(start, len);
        std::string newText = ConvertCase(text, toUpper);
        if (text != newText) {
            batch.ops.push_back({ EditOp::Erase, start, text });
            pt.erase(start, len);
            for (auto& o : cursors) {
                if (o.head > start) o.head -= len;
                if (o.anchor > start) o.anchor -= len;
            }
            pt.insert(start, newText);
            batch.ops.push_back({ EditOp::Insert, start, newText });
            size_t newLen = newText.length();
            for (auto& o : cursors) {
                if (o.head >= start) o.head += newLen;
                if (o.anchor >= start) o.anchor += newLen;
            }
            if (isHeadStart) {
                c.head = start;
                c.anchor = start + newLen;
            } else {
                c.anchor = start;
                c.head = start + newLen;
            }
            c.isVirtual = false;
            changed = true;
        }
    }
    if (changed) {
        rebuildLineStarts();
        for (auto& c : cursors) {
            c.desiredX = getXFromPos(c.head);
            c.originalAnchorX = getXFromPos(c.anchor);
        }
        batch.afterCursors = cursors;
        undo.push(batch);
        updateDirtyFlag();
        if (cbUpdatePreview && showPreview) cbUpdatePreview();
        if (cbNeedsDisplay) cbNeedsDisplay();
    }
}

void Editor::copyToClipboard() {
    bool hasSelection = false;
    for (const auto& c : cursors) if (c.hasSelection()) { hasSelection = true; break; }
    std::string t;
    bool isRect = (cursors.size() > 1);
    if (!hasSelection) {
        std::vector<int> lines = getUniqueLineIndices();
        for (int li : lines) {
            size_t start = lineStarts[li], end = (li + 1 < (int)lineStarts.size()) ? lineStarts[li + 1] : pt.length();
            t += pt.getRange(start, end - start);
            if (t.empty() || (t.back() != '\n' && t.back() != '\r')) t += newlineStr;
        }
        lastCopyWasLine = true;
        lastLineCopyText = t;
    } else {
        auto sorted = cursors;
        std::sort(sorted.begin(), sorted.end(), [](const Cursor& a, const Cursor& b) { return a.start() < b.start(); });
        for (size_t i = 0; i < sorted.size(); ++i) {
            if (sorted[i].hasSelection()) {
                std::string part = pt.getRange(sorted[i].start(), sorted[i].end() - sorted[i].start());
                t += part;
                if (i < sorted.size() - 1 && (isRect || (!part.empty() && part.back() != '\n' && part.back() != '\r'))) {
                    t += newlineStr;
                }
            }
        }
        lastCopyWasLine = false;
        lastLineCopyText.clear();
    }
    if (!t.empty() && cbSetClipboard) {
        cbSetClipboard(t, hasSelection && isRect);
    }
}

void Editor::cutToClipboard() {
    bool hasSelection = false;
    for (const auto& c : cursors) if (c.hasSelection()) { hasSelection = true; break; }
    copyToClipboard();
    if (!hasSelection) deleteLine();
    else insertAtCursors("");
}

void Editor::pasteFromClipboard() {
    if (!cbGetClipboard) return;
    bool isRectMarker = false;
    std::string utf8 = cbGetClipboard(isRectMarker);
    if (utf8.empty()) return;
    if (isRectMarker) {
        if (cursors.size() <= 1) {
            size_t basePos = cursors.empty() ? 0 : cursors[0].head;
            float baseX = getXFromPos(basePos);
            int startLine = getLineIdx(basePos);
            int lineCount = 1;
            for (char c : utf8) if (c == '\n') lineCount++;
            cursors.clear();
            for (int i = 0; i < lineCount; ++i) {
                int targetLine = startLine + i;
                if (targetLine < (int)lineStarts.size()) {
                    size_t p = getPosFromLineAndX(targetLine, baseX);
                    cursors.push_back({ p, p, baseX, baseX, false, false });
                }
            }
        }
        insertRectangularBlock(utf8);
    } else if (lastCopyWasLine && (lastLineCopyText.empty() || utf8 == lastLineCopyText)) {
        bool hasAnySelection = false;
        for (const auto& c : cursors) if (c.hasSelection()) { hasAnySelection = true; break; }
        if (hasAnySelection) {
            insertAtCursors(utf8);
        } else {
            rollbackPadding();
            rebuildLineStarts();
            std::vector<Cursor> originalCursors = cursors;
            std::vector<size_t> relativeOffsets;
            for (auto& c : cursors) {
                int lineIdx = getLineIdx(c.head);
                size_t lineStart = lineStarts[lineIdx];
                relativeOffsets.push_back((c.head > lineStart) ? (c.head - lineStart) : 0);
                c.head = lineStart;
                c.anchor = lineStart;
            }
            insertAtCursors(utf8);
            for (size_t i = 0; i < cursors.size() && i < relativeOffsets.size(); ++i) {
                int lineIdx = getLineIdx(cursors[i].head);
                size_t lineStart = lineStarts[lineIdx];
                size_t lineEnd = (lineIdx + 1 < (int)lineStarts.size()) ? lineStarts[lineIdx + 1] : pt.length();
                if (lineEnd > lineStart && pt.charAt(lineEnd - 1) == '\n') lineEnd--;
                if (lineEnd > lineStart && pt.charAt(lineEnd - 1) == '\r') lineEnd--;
                cursors[i].head = std::min(lineStart + relativeOffsets[i], lineEnd);
                cursors[i].anchor = cursors[i].head;
                cursors[i].desiredX = getXFromPos(cursors[i].head);
            }
            if (!undo.undoStack.empty()) {
                undo.undoStack.back().beforeCursors = originalCursors;
                undo.undoStack.back().afterCursors = cursors;
            }
            ensureCaretVisible();
            updateDirtyFlag();
        }
    } else {
        insertAtCursors(utf8);
    }
    if (cbNeedsDisplay) cbNeedsDisplay();
}

void Editor::performUndo() {
    rollbackPadding();
    isRectSelecting = false;
    activeEmmetSession.active = false;
    if (!undo.undoStack.empty()) {
        auto b = undo.popUndo();
        for (int i = (int)b.ops.size() - 1; i >= 0; --i) {
            if (b.ops[i].type == EditOp::Insert) pt.erase(b.ops[i].pos, b.ops[i].text.size());
            else pt.insert(b.ops[i].pos, b.ops[i].text);
        }
        cursors = b.beforeCursors;
        rebuildLineStarts();
        ensureCaretVisible();
        updateDirtyFlag();
        if (cbUpdatePreview && showPreview) cbUpdatePreview();
        if (cbNeedsDisplay) cbNeedsDisplay();
    }
}

void Editor::performRedo() {
    rollbackPadding();
    isRectSelecting = false;
    activeEmmetSession.active = false;
    if (!undo.redoStack.empty()) {
        auto b = undo.popRedo();
        for (const auto& o : b.ops) {
            if (o.type == EditOp::Insert) pt.insert(o.pos, o.text);
            else pt.erase(o.pos, o.text.size());
        }
        cursors = b.afterCursors;
        rebuildLineStarts();
        ensureCaretVisible();
        updateDirtyFlag();
        if (cbUpdatePreview && showPreview) cbUpdatePreview();
        if (cbNeedsDisplay) cbNeedsDisplay();
    }
}

bool Editor::checkUnsavedChanges() {
    if (!isDirty) return true;
    if (cbShowUnsavedAlert) return cbShowUnsavedAlert();
    return true;
}

bool Editor::saveFileElevated(const std::string& utf8Path, const std::string& rawBytes) {
#if defined(__APPLE__) && (!defined(TARGET_OS_IPHONE) || !TARGET_OS_IPHONE)
    char tmpTemplate[] = "/tmp/miu_elevated_XXXXXX";
    int tmpFd = mkstemp(tmpTemplate);
    if (tmpFd == -1) return false;
    ssize_t written = ::write(tmpFd, rawBytes.data(), rawBytes.size());
    ::close(tmpFd);
    if (written < 0 || (size_t)written != rawBytes.size()) {
        ::unlink(tmpTemplate);
        return false;
    }
    auto escapeShSingleQuote = [](const std::string& s) {
        std::string out = "'";
        for (char c : s) {
            if (c == '\'') out += "'\\''";
            else out += c;
        }
        out += "'";
        return out;
    };
    std::string shellCmd = "cp -f " + escapeShSingleQuote(tmpTemplate) + " " + escapeShSingleQuote(utf8Path) +
                           " && rm -f " + escapeShSingleQuote(tmpTemplate);
    std::string appleScript;
    appleScript.reserve(shellCmd.size() * 2 + 64);
    appleScript += "do shell script \"";
    for (char c : shellCmd) {
        if (c == '\\' || c == '"') appleScript += '\\';
        appleScript += c;
    }
    appleScript += "\" with administrator privileges";
    std::string fullCmd = "/usr/bin/osascript -e " + escapeShSingleQuote(appleScript);
    int ret = std::system(fullCmd.c_str());
    ::unlink(tmpTemplate);
    return (ret == 0);
#else
    (void)utf8Path;
    (void)rawBytes;
    return false;
#endif
}

bool Editor::saveFile(const std::wstring& p) {
    rollbackPadding();
    std::string contentUtf8 = pt.getRange(0, pt.length());
    std::string outBytes;

    if (currentEncoding == ENC_UTF16LE || currentEncoding == ENC_UTF16BE) {
        outBytes = Utf8ToUtf16Bytes(contentUtf8, currentEncoding == ENC_UTF16BE);
    } else if (currentEncoding == ENC_LOCAL) {
        std::string localStr = Utf8ToLocal(contentUtf8, currentCodePage);
        outBytes = !localStr.empty() ? localStr : contentUtf8;
    } else if (currentEncoding == ENC_UTF8_BOM) {
        outBytes.assign("\xEF\xBB\xBF", 3);
        outBytes += contentUtf8;
    } else {
        outBytes = contentUtf8;
    }

    std::string utf8Path = WToUTF8(p);
#if defined(_WIN32)
    std::ofstream f(p, std::ios::binary);
#else
    std::ofstream f(utf8Path, std::ios::binary);
#endif
    if (!f) {
        if (errno == EACCES || errno == EPERM) {
            if (!saveFileElevated(utf8Path, outBytes)) return false;
        } else {
            return false;
        }
    } else {
        f.write(outBytes.data(), outBytes.size());
        f.close();
        if (!f) return false;
    }

    currentFilePath = p;
    loadSyntaxForExtension(utf8Path);
    recordFileMtime(utf8Path);
    addToRecentFiles(utf8Path);
    saveCurrentViewState();
    undo.markSaved();
    isDirty = false;
    updateTitleBar();
    return true;
}

bool Editor::saveFileAs() {
    if (cbSaveFileAs) return cbSaveFileAs();
    return false;
}

bool Editor::openFileFromPath(const std::string& p) {
    saveCurrentViewState();
    rollbackPadding();
    isRectSelecting = false;
    activeEmmetSession.active = false;
    fileMap.reset(new MappedFile());
    if (fileMap->open(p.c_str())) {
        DetectResult encRes = DetectEncodingEx(fileMap->ptr, fileMap->size);
        currentEncoding = encRes.type;
        currentCodePage = encRes.codePage;
        const char* ptr = fileMap->ptr;
        size_t sz = fileMap->size;
        this->currentFileBuffer = "";
        bool usedDecodedBuffer = false;
        if (currentEncoding == ENC_UTF16LE) {
            this->currentFileBuffer = Utf16ToUtf8(ptr, sz, false);
            usedDecodedBuffer = true;
        } else if (currentEncoding == ENC_UTF16BE) {
            this->currentFileBuffer = Utf16ToUtf8(ptr, sz, true);
            usedDecodedBuffer = true;
        } else if (currentEncoding == ENC_UTF8_BOM) {
            if (sz >= 3) { ptr += 3; sz -= 3; }
        } else if (currentEncoding == ENC_LOCAL) {
            this->currentFileBuffer = LocalToUtf8(ptr, sz, currentCodePage);
            if (!this->currentFileBuffer.empty() || sz == 0) usedDecodedBuffer = true;
        }
        if (usedDecodedBuffer) {
            pt.initFromFile(this->currentFileBuffer.data(), this->currentFileBuffer.size());
            detectNewlineStyle(this->currentFileBuffer.data(), this->currentFileBuffer.size());
        } else {
            pt.initFromFile(ptr, sz);
            detectNewlineStyle(ptr, sz);
        }
        currentFilePath = UTF8ToW(p);
        loadSyntaxForExtension(p);
        recordFileMtime(p);
        addToRecentFiles(p);
        undo.clear();
        isDirty = false;
        vScrollPos = 0;
        vScrollSubLine = 0;
        hScrollPos = 0;
        cursors.clear();
        cursors.push_back({ 0, 0, 0.0f, 0.0f, false, false });
        rebuildLineStarts();
        restoreViewState(currentFilePath);
        updateTitleBar();
        updateScrollBars();
        if (cbUpdatePreview && showPreview) {
            previewInitialLoaded = false;
            cbUpdatePreview();
        }
        if (cbNeedsDisplay) cbNeedsDisplay();
        return true;
    }
    return false;
}

bool Editor::openFile() {
    if (!checkUnsavedChanges()) return false;
    if (cbOpenFile) return cbOpenFile();
    return false;
}

void Editor::newFile() {
    if (checkUnsavedChanges()) {
        saveCurrentViewState();
        rollbackPadding();
        isRectSelecting = false;
        activeEmmetSession.active = false;
        pt.initEmpty();
        currentFilePath.clear();
        currentFileExt.clear();
        currentEncoding = ENC_UTF8_NOBOM;
        currentCodePage = 0;
        currentSyntax = SyntaxDef();
        commentStateCache.clear();
        this->currentFileBuffer.clear();
        newlineStr = "\n";
        undo.clear();
        isDirty = false;
        vScrollPos = 0;
        vScrollSubLine = 0;
        hScrollPos = 0;
        lastKnownMtime = 0;
        lastKnownFileSize = 0;
        cursors.clear();
        cursors.push_back({ 0, 0, 0.0f, 0.0f, false, false });
        rebuildLineStarts();
        updateTitleBar();
        if (cbUpdatePreview && showPreview) {
            previewInitialLoaded = false;
            cbUpdatePreview();
        }
        if (cbNeedsDisplay) cbNeedsDisplay();
    }
}

bool Editor::isWordChar(char c) {
    if (isalnum((unsigned char)c) || c == '_') return true;
    if ((unsigned char)c >= 0x80) return true;
    return false;
}

void Editor::getWordBoundaries(size_t pos, size_t& start, size_t& end) {
    size_t len = pt.length();
    if (len == 0 || pos >= len) { start = end = pos; return; }
    char c = pt.charAt(pos);
    if (c == '\r') {
        if (pos + 1 < len && pt.charAt(pos + 1) == '\n') {
            start = pos;
            end = pos + 2;
            return;
        }
    }
    bool targetType = isWordChar(c);
    if (c == '\n') { start = pos; end = pos + 1; return; }
    start = pos;
    while (start > 0) {
        char p = pt.charAt(start - 1);
        if (isWordChar(p) != targetType || p == '\n' || p == '\r') break;
        start--;
    }
    end = pos;
    while (end < len) {
        char p = pt.charAt(end);
        if (isWordChar(p) != targetType || p == '\n' || p == '\r') break;
        end++;
    }
}

void Editor::initGraphics() {
#if defined(__APPLE__)
    if (!fontRef) fontRef = CTFontCreateWithName(CFSTR("Menlo"), currentFontSize, NULL);
#endif
    updateFont(currentFontSize);
    rebuildLineStarts();
    updateThemeColors();
    loadRecentFiles();
    if (cursors.empty()) cursors.push_back({ 0, 0, 0.0f, 0.0f, false, false });
    updateTitleBar();
}

void Editor::updateThemeColors() {
    if (!currentFileExt.empty()) {
        loadSyntaxDefForExtension(currentFileExt, isDarkMode, currentSyntax);
    }
#if defined(__APPLE__)
    if (colBackground) {
        CGColorRelease(colBackground);
        CGColorRelease(colText);
        CGColorRelease(colGutterBg);
        CGColorRelease(colGutterText);
        CGColorRelease(colSel);
        CGColorRelease(colCaret);
    }
    if (colNewlineMark) CGColorRelease(colNewlineMark);
    if (colBracketBg) CGColorRelease(colBracketBg);
    if (colBracketBorder) CGColorRelease(colBracketBorder);

    if (isDarkMode) {
        colText = CGColorCreateGenericRGB(1.0, 1.0, 1.0, 1.0);
        colBackground = CGColorCreateGenericRGB(0.0, 0.0, 0.0, 1.0);
        colGutterBg = CGColorCreateGenericRGB(0.0, 0.0, 0.0, 1.0);
        colGutterText = CGColorCreateGenericRGB(0.45, 0.45, 0.48, 1.0);
        colSel = CGColorCreateGenericRGB(0.2, 0.3, 0.5, 1.0);
        colCaret = CGColorCreateGenericRGB(1.0, 1.0, 1.0, 1.0);
        colNewlineMark = CGColorCreateGenericRGB(1.0, 1.0, 1.0, 0.18);
        colBracketBg = CGColorCreateGenericRGB(0.35, 0.55, 0.85, 0.35);
        colBracketBorder = CGColorCreateGenericRGB(0.50, 0.75, 1.0, 0.85);
    } else {
        colText = CGColorCreateGenericRGB(0.0, 0.0, 0.0, 1.0);
        colBackground = CGColorCreateGenericRGB(1.0, 1.0, 1.0, 1.0);
        colGutterBg = CGColorCreateGenericRGB(1.0, 1.0, 1.0, 1.0);
        colGutterText = CGColorCreateGenericRGB(0.55, 0.55, 0.58, 1.0);
        colSel = CGColorCreateGenericRGB(0.70, 0.80, 1.0, 1.0);
        colCaret = CGColorCreateGenericRGB(0.0, 0.0, 0.0, 1.0);
        colNewlineMark = CGColorCreateGenericRGB(0.50, 0.50, 0.50, 0.20);
        colBracketBg = CGColorCreateGenericRGB(0.20, 0.50, 0.90, 0.22);
        colBracketBorder = CGColorCreateGenericRGB(0.15, 0.45, 0.85, 0.80);
    }
#endif
    if (cbUpdatePreview && showPreview) {
        previewInitialLoaded = false;
        cbUpdatePreview();
    }
}

void Editor::buildVisibleUtf16Map(size_t startPos, const std::string& text) {
    utf16MapBasePos = startPos;
    utf16Cumulative.assign(text.size() + 1, 0);
    uint32_t acc = 0;
    size_t i = 0, n = text.size();
    while (i < n) {
        unsigned char c = (unsigned char)text[i];
        size_t step = 1;
        uint32_t u16Step = 1;
        if (c < 0x80) { step = 1; u16Step = 1; }
        else if ((c & 0xE0) == 0xC0) { step = 2; u16Step = 1; }
        else if ((c & 0xF0) == 0xE0) { step = 3; u16Step = 1; }
        else if ((c & 0xF8) == 0xF0) { step = 4; u16Step = 2; }
        step = std::min(step, n - i);
        for (size_t k = 0; k < step; ++k) {
            utf16Cumulative[i + k] = acc;
        }
        acc += u16Step;
        i += step;
    }
    utf16Cumulative[n] = acc;
}

size_t Editor::utf8OffsetToUtf16Count(size_t startPos, size_t byteLen) {
    if (byteLen == 0) return 0;
    if (!utf16Cumulative.empty() &&
        startPos >= utf16MapBasePos &&
        (startPos + byteLen - utf16MapBasePos) < utf16Cumulative.size()) {
        size_t relS = startPos - utf16MapBasePos;
        size_t relE = relS + byteLen;
        return (size_t)(utf16Cumulative[relE] - utf16Cumulative[relS]);
    }
    return countUTF16Length(startPos, byteLen);
}

size_t Editor::countUTF16Length(size_t start, size_t byteLen) {
    size_t end = std::min(start + byteLen, pt.length());
    size_t utf16Count = 0;
    size_t p = start;
    while (p < end) {
        unsigned char c = (unsigned char)pt.charAt(p);
        if (c < 0x80) {
            p += 1; utf16Count += 1;
        } else if ((c & 0xE0) == 0xC0) {
            p += 2; utf16Count += 1;
        } else if ((c & 0xF0) == 0xE0) {
            p += 3; utf16Count += 1;
        } else if ((c & 0xF8) == 0xF0) {
            p += 4; utf16Count += 2;
        } else {
            p += 1; utf16Count += 1;
        }
    }
    return utf16Count;
}

size_t Editor::getPositionByUTF16Offset(size_t startPos, int offset) {
    if (offset == 0) return startPos;
    size_t docLen = pt.length();
    if (offset > 0) {
        size_t p = startPos;
        int count = 0;
        while (p < docLen && count < offset) {
            unsigned char c = (unsigned char)pt.charAt(p);
            if (c < 0x80) {
                p += 1; count += 1;
            } else if ((c & 0xE0) == 0xC0) {
                p += 2; count += 1;
            } else if ((c & 0xF0) == 0xE0) {
                p += 3; count += 1;
            } else if ((c & 0xF8) == 0xF0) {
                p += 4; count += 2;
            } else {
                p += 1; count += 1;
            }
        }
        return std::min(p, docLen);
    } else {
        size_t p = startPos;
        int count = 0;
        int target = -offset;
        while (p > 0 && count < target) {
            p--;
            while (p > 0 && ((unsigned char)pt.charAt(p) & 0xC0) == 0x80) {
                p--;
            }
            unsigned char c = (unsigned char)pt.charAt(p);
            if ((c & 0xF8) == 0xF0) {
                count += 2;
            } else {
                count += 1;
            }
        }
        return p;
    }
}

std::string Editor::getAppSupportDir() const {
    if (!customAppSupportDir.empty()) return customAppSupportDir;
#if defined(__APPLE__)
    const char* home = getenv("HOME");
    if (!home) {
        struct passwd* pw = getpwuid(getuid());
        if (pw) home = pw->pw_dir;
    }
    if (!home) return "/tmp";
    std::string libDir = std::string(home) + "/Library";
    mkdir(libDir.c_str(), 0755);
    std::string appSupDir = libDir + "/Application Support";
    mkdir(appSupDir.c_str(), 0755);
    std::string dir = appSupDir + "/miu";
    mkdir(dir.c_str(), 0755);
    return dir;
#else
    char tmpPath[MAX_PATH] = { 0 };
    GetTempPathA(MAX_PATH, tmpPath);
    std::string dir = std::string(tmpPath) + "miu_appdata";
    _mkdir(dir.c_str());
    return dir;
#endif
}

struct PersistedViewStateEntry {
    std::wstring path;
    FileViewState state;
};

static std::vector<PersistedViewStateEntry> loadAllViewStates(const std::string& binPath) {
    std::vector<PersistedViewStateEntry> list;
    std::ifstream ifs(binPath, std::ios::binary);
    if (!ifs) return list;
    uint32_t magic = 0, count = 0;
    ifs.read((char*)&magic, sizeof(magic));
    ifs.read((char*)&count, sizeof(count));
    if (!ifs || magic != 0x3153564D || count > 1000) return list;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t pathUtf8Len = 0;
        ifs.read((char*)&pathUtf8Len, sizeof(pathUtf8Len));
        if (!ifs || pathUtf8Len == 0 || pathUtf8Len > 8192) break;
        std::string pUtf8(pathUtf8Len, '\0');
        ifs.read(pUtf8.data(), pathUtf8Len);
        FileViewState st;
        ifs.read((char*)&st.vScrollPos, sizeof(st.vScrollPos));
        ifs.read((char*)&st.vScrollSubLine, sizeof(st.vScrollSubLine));
        ifs.read((char*)&st.hScrollPos, sizeof(st.hScrollPos));
        uint64_t ch = 0, ca = 0;
        uint8_t ww = 0;
        ifs.read((char*)&ch, sizeof(ch));
        ifs.read((char*)&ca, sizeof(ca));
        ifs.read((char*)&ww, sizeof(ww));
        if (!ifs) break;
        st.cursorHead = (size_t)ch;
        st.cursorAnchor = (size_t)ca;
        st.wordWrap = (ww != 0);
        list.push_back({ UTF8ToW(pUtf8), st });
    }
    return list;
}

static void writeAllViewStates(const std::string& binPath, const std::vector<PersistedViewStateEntry>& list) {
    std::ofstream ofs(binPath, std::ios::binary | std::ios::trunc);
    if (!ofs) return;
    uint32_t magic = 0x3153564D; // "MVS1"
    uint32_t count = (uint32_t)list.size();
    ofs.write((const char*)&magic, sizeof(magic));
    ofs.write((const char*)&count, sizeof(count));
    for (const auto& item : list) {
        std::string pUtf8 = WToUTF8(item.path);
        uint32_t pathLen = (uint32_t)pUtf8.size();
        ofs.write((const char*)&pathLen, sizeof(pathLen));
        ofs.write(pUtf8.data(), pathLen);
        ofs.write((const char*)&item.state.vScrollPos, sizeof(item.state.vScrollPos));
        ofs.write((const char*)&item.state.vScrollSubLine, sizeof(item.state.vScrollSubLine));
        ofs.write((const char*)&item.state.hScrollPos, sizeof(item.state.hScrollPos));
        uint64_t ch = (uint64_t)item.state.cursorHead;
        uint64_t ca = (uint64_t)item.state.cursorAnchor;
        uint8_t ww = item.state.wordWrap ? 1 : 0;
        ofs.write((const char*)&ch, sizeof(ch));
        ofs.write((const char*)&ca, sizeof(ca));
        ofs.write((const char*)&ww, sizeof(ww));
    }
}

void Editor::saveCurrentViewState() {
    if (currentFilePath.empty()) return;
    std::string binPath = getAppSupportDir() + "/viewstates.bin";
    auto list = loadAllViewStates(binPath);
    list.erase(std::remove_if(list.begin(), list.end(), [&](const PersistedViewStateEntry& e) {
        return e.path == currentFilePath;
    }), list.end());

    FileViewState st;
    st.vScrollPos = vScrollPos;
    st.vScrollSubLine = vScrollSubLine;
    st.hScrollPos = hScrollPos;
    st.cursorHead = cursors.empty() ? 0 : cursors.back().head;
    st.cursorAnchor = cursors.empty() ? 0 : cursors.back().anchor;
    st.wordWrap = wordWrapEnabled;
    list.push_back({ currentFilePath, st });
    if (list.size() > 200) {
        list.erase(list.begin(), list.begin() + (list.size() - 200));
    }
    writeAllViewStates(binPath, list);
}

bool Editor::restoreViewState(const std::wstring& filePath) {
    if (filePath.empty()) return false;
    std::string binPath = getAppSupportDir() + "/viewstates.bin";
    auto list = loadAllViewStates(binPath);
    for (auto it = list.rbegin(); it != list.rend(); ++it) {
        if (it->path == filePath) {
            const FileViewState& st = it->state;
            int maxLine = std::max(0, (int)lineStarts.size() - 1);
            vScrollPos = std::clamp(st.vScrollPos, 0, maxLine);
            vScrollSubLine = std::max(0, st.vScrollSubLine);
            hScrollPos = std::max(0, st.hScrollPos);
            wordWrapEnabled = st.wordWrap;
            if (wordWrapEnabled) hScrollPos = 0;
            size_t docLen = pt.length();
            size_t ch = std::min(st.cursorHead, docLen);
            size_t ca = std::min(st.cursorAnchor, docLen);
            cursors.clear();
            cursors.push_back({ ch, ca, getXFromPos(ch), getXFromPos(ca), false, false });
            return true;
        }
    }
    return false;
}

void Editor::loadRecentFiles() {
    recentFiles.clear();
    std::string path = getAppSupportDir() + "/recent_files.txt";
    std::ifstream ifs(path);
    if (!ifs) return;
    std::string line;
    while (std::getline(ifs, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) recentFiles.push_back(line);
    }
}

void Editor::saveRecentFiles() {
    std::string path = getAppSupportDir() + "/recent_files.txt";
    std::ofstream ofs(path, std::ios::trunc);
    if (!ofs) return;
    for (const auto& f : recentFiles) {
        ofs << f << "\n";
    }
}

void Editor::addToRecentFiles(const std::string& path) {
    if (path.empty()) return;
    if (recentFiles.empty()) loadRecentFiles();
    recentFiles.erase(std::remove(recentFiles.begin(), recentFiles.end(), path), recentFiles.end());
    recentFiles.insert(recentFiles.begin(), path);
    if (recentFiles.size() > 15) recentFiles.resize(15);
    saveRecentFiles();
    if (cbUpdateRecentMenu) cbUpdateRecentMenu();
}

void Editor::clearRecentFiles() {
    recentFiles.clear();
    saveRecentFiles();
    if (cbUpdateRecentMenu) cbUpdateRecentMenu();
}

void Editor::recordFileMtime(const std::string& path) {
    struct stat sb;
    if (stat(path.c_str(), &sb) == 0) {
        lastKnownMtime = (uint64_t)sb.st_mtime;
        lastKnownFileSize = (uint64_t)sb.st_size;
    } else {
        lastKnownMtime = 0;
        lastKnownFileSize = 0;
    }
}

bool Editor::checkFileModification() {
    if (currentFilePath.empty() || lastKnownMtime == 0) return false;
    std::string utf8Path = WToUTF8(currentFilePath);
    struct stat sb;
    if (stat(utf8Path.c_str(), &sb) != 0) return false;
    uint64_t diskMtime = (uint64_t)sb.st_mtime;
    uint64_t diskSize = (uint64_t)sb.st_size;
    if (diskMtime == lastKnownMtime && diskSize == lastKnownFileSize) return false;

    lastKnownMtime = diskMtime;
    lastKnownFileSize = diskSize;
    if (!isDirty) {
        openFileFromPath(utf8Path);
        return true;
    }
    if (cbShowExternalModifyAlert && cbShowExternalModifyAlert()) {
        isDirty = false;
        openFileFromPath(utf8Path);
        return true;
    }
    return false;
}

#if defined(__APPLE__)
void Editor::render(CGContextRef ctx, float w, float h) {
    ScrollBarLayout sbLayout = getScrollBarLayout(w, h);
    visibleVScrollWidth = sbLayout.vBarW;
    visibleHScrollHeight = sbLayout.hBarH;

#if TARGET_OS_IOS
    CGContextClearRect(ctx, CGRectMake(0, -200.0f, gutterWidth, h + 200.0f));
    CGContextSetFillColorWithColor(ctx, colBackground);
    CGContextFillRect(ctx, CGRectMake(gutterWidth, -200.0f, w - gutterWidth, h + 200.0f));
#else
    CGContextClearRect(ctx, CGRectMake(0, 0, w, h));
    CGContextSetFillColorWithColor(ctx, colBackground);
    CGContextFillRect(ctx, CGRectMake(gutterWidth, 0, w - gutterWidth, h));
#endif
    CGContextSetTextMatrix(ctx, CGAffineTransformMakeScale(1.0, -1.0));
    float vw = std::max(0.0f, w - gutterWidth - visibleVScrollWidth);
    float vh = std::max(0.0f, h - visibleHScrollHeight);
    int start = std::clamp(vScrollPos, 0, std::max(0, (int)lineStarts.size() - 1));
    int vis = (int)(vh / lineHeight) + 2;
    int end = std::min((int)lineStarts.size(), start + vis);
#if TARGET_OS_IOS
    int renderStart = std::max(0, start - 15);
#else
    int renderStart = start;
#endif
    // Uniform baseline offset across gutter and body text to prevent CJK/Emoji vertical drift
    float baselineOffset = lineHeight * 0.8f;

    size_t visByteStart = lineStarts[renderStart];
    size_t visByteEnd = (end < (int)lineStarts.size()) ? lineStarts[end] : pt.length();
    std::string visibleText = pt.getRange(visByteStart, visByteEnd - visByteStart);
    buildVisibleUtf16Map(visByteStart, visibleText);

    CGContextSaveGState(ctx);
#if TARGET_OS_IOS
    CGContextClipToRect(ctx, CGRectMake(gutterWidth, -200.0f, vw, vh + 200.0f));
#else
    CGContextClipToRect(ctx, CGRectMake(gutterWidth, 0, vw, vh));
#endif

    // Precompute visual Y offset of each logical line in [renderStart, end)
    std::vector<float> lineBaseVisualY(std::max(1, end - renderStart), 0.0f);
    if (wordWrapEnabled) {
        float accY = 0.0f;
        for (int li = start; li < end; ++li) {
            lineBaseVisualY[li - renderStart] = accY;
            int sc = getLineVisualCount(li, vw);
            int startSub = (li == start) ? std::clamp(vScrollSubLine, 0, std::max(0, sc - 1)) : 0;
            accY += (float)std::max(0, sc - startSub) * lineHeight;
        }
        float negY = 0.0f;
        for (int li = start - 1; li >= renderStart; --li) {
            int sc = getLineVisualCount(li, vw);
            negY -= (float)std::max(1, sc) * lineHeight;
            lineBaseVisualY[li - renderStart] = negY;
        }
    } else {
        for (int li = renderStart; li < end; ++li) {
            lineBaseVisualY[li - renderStart] = (float)(li - start) * lineHeight;
        }
    }

    auto forEachVisualSpanRect = [&](int li, size_t spanStart, size_t spanEnd, float extraEndW, auto&& rectFn) {
        if (li < renderStart || li >= end) return;
        if (!wordWrapEnabled) {
            float y = lineBaseVisualY[li - renderStart];
            float x1 = getXInLine(li, spanStart);
            float x2 = getXInLine(li, spanEnd) + extraEndW;
            if (x2 <= x1) x2 = x1 + charWidth;
            rectFn(CGRectMake(gutterWidth - (float)hScrollPos + x1, y, x2 - x1, lineHeight));
            return;
        }
        size_t lineBegin = lineStarts[li];
        auto subs = getVisualSubLines(li, vw);
        int startSub = (li == start) ? std::clamp(vScrollSubLine, 0, std::max(0, (int)subs.size() - 1)) : 0;
        for (int subIdx = startSub; subIdx < (int)subs.size(); ++subIdx) {
            size_t subDocS = lineBegin + subs[subIdx].byteStart;
            size_t subDocE = subDocS + subs[subIdx].byteLen;
            size_t ovS = std::max(spanStart, subDocS);
            size_t ovE = std::min(spanEnd, subDocE);
            if (ovS < ovE || (spanStart == spanEnd && spanStart == ovS && subIdx == (int)subs.size() - 1)) {
                float subOriginX = getXInLine(li, subDocS);
                float x1 = getXInLine(li, ovS) - subOriginX;
                float x2 = getXInLine(li, ovE) - subOriginX;
                if (subIdx == (int)subs.size() - 1 && ovE == subDocE) x2 += extraEndW;
                if (x2 <= x1) x2 = x1 + charWidth;
                float y = lineBaseVisualY[li - renderStart] + (float)(subIdx - startSub) * lineHeight;
                rectFn(CGRectMake(gutterWidth + x1, y, x2 - x1, lineHeight));
            }
        }
    };

    // 1. Auto-highlight occurrences of current word / selection
    auto [autoStr, isWholeWord] = getHighlightTarget();
    if (!autoStr.empty() && autoStr != searchQuery) {
        CGColorRef autoHlColor = isDarkMode ? CGColorCreateGenericRGB(0.35, 0.35, 0.35, 0.5) : CGColorCreateGenericRGB(0.85, 0.85, 0.85, 0.5);
        CGContextSetFillColorWithColor(ctx, autoHlColor);
        size_t searchPos = 0;
        while ((searchPos = visibleText.find(autoStr, searchPos)) != std::string::npos) {
            size_t docPos = visByteStart + searchPos;
            bool shouldHighlight = true;
            if (isWholeWord) {
                if (docPos > 0 && isWordChar(pt.charAt(docPos - 1))) shouldHighlight = false;
                if (shouldHighlight && (docPos + autoStr.length() < pt.length()) && isWordChar(pt.charAt(docPos + autoStr.length()))) shouldHighlight = false;
            }
            if (shouldHighlight) {
                int li = getLineIdx(docPos);
                forEachVisualSpanRect(li, docPos, docPos + autoStr.length(), 0.0f, [&](CGRect r) {
                    CGContextFillRect(ctx, r);
                });
            }
            searchPos += 1;
        }
        CGColorRelease(autoHlColor);
    }

    // 2. Search query highlight
    if (!searchQuery.empty()) {
        CGColorRef hlColor = isDarkMode ? CGColorCreateGenericRGB(0.4, 0.4, 0.0, 0.6) : CGColorCreateGenericRGB(1.0, 1.0, 0.0, 0.4);
        CGContextSetFillColorWithColor(ctx, hlColor);
        if (searchRegex && isSafeUserRegex(searchQuery)) {
            try {
                std::string actualQuery = preprocessRegexQuery(searchQuery);
                if (isSafeUserRegex(actualQuery)) {
                    std::regex_constants::syntax_option_type flags = std::regex_constants::ECMAScript;
                    if (!searchMatchCase) flags |= std::regex_constants::icase;
                    std::regex re(actualQuery, flags);
                    bool startsWithCaret = (!searchQuery.empty() && searchQuery[0] == '^');
                    auto searchStartIter = visibleText.cbegin();
                    std::smatch m;
                    std::regex_constants::match_flag_type matchFlags = std::regex_constants::match_default;
                    while (std::regex_search(searchStartIter, visibleText.cend(), m, re, matchFlags)) {
                        size_t matchPos = std::distance(visibleText.cbegin(), searchStartIter) + m.position();
                        size_t matchLen = m.length();
                        size_t anchorLen = (startsWithCaret && m.size() > 1 && m[1].matched) ? (size_t)m.length(1) : 0;
                        size_t contentPos = matchPos + anchorLen;
                        size_t contentLen = matchLen - anchorLen;
                        if (contentLen > 0) {
                            size_t currentDrawPos = visByteStart + contentPos;
                            int li = getLineIdx(currentDrawPos);
                            forEachVisualSpanRect(li, currentDrawPos, currentDrawPos + contentLen, 0.0f, [&](CGRect r) {
                                CGContextFillRect(ctx, r);
                            });
                        }
                        size_t step = std::max((size_t)1, matchLen);
                        size_t advance = m.position() + step;
                        if (std::distance(searchStartIter, visibleText.cend()) < (ptrdiff_t)advance) break;
                        std::advance(searchStartIter, advance);
                        matchFlags |= std::regex_constants::match_not_bol;
                    }
                }
            } catch (...) {}
        } else if (!searchRegex) {
            std::string q = searchQuery;
            std::string t = visibleText;
            if (!searchMatchCase) {
                std::transform(q.begin(), q.end(), q.begin(), ::tolower);
                std::transform(t.begin(), t.end(), t.begin(), ::tolower);
            }
            size_t offset = 0;
            while ((offset = t.find(q, offset)) != std::string::npos) {
                bool match = true;
                if (searchWholeWord) {
                    if (offset > 0 && isWordChar(visibleText[offset - 1])) match = false;
                    if (match && (offset + q.length() < visibleText.length()) && isWordChar(visibleText[offset + q.length()])) match = false;
                }
                if (match) {
                    size_t currentDrawPos = visByteStart + offset;
                    int li = getLineIdx(currentDrawPos);
                    forEachVisualSpanRect(li, currentDrawPos, currentDrawPos + q.length(), 0.0f, [&](CGRect r) {
                        CGContextFillRect(ctx, r);
                    });
                }
                offset += 1;
            }
        }
        CGColorRelease(hlColor);
    }

    // 3. Bracket matching highlight
    if (!cursors.empty()) {
        size_t b1 = 0, b2 = 0;
        if (getAdjacentBrackets(cursors.back().head, b1, b2)) {
            size_t brs[2] = { b1, b2 };
            for (size_t bp : brs) {
                int li = getLineIdx(bp);
                forEachVisualSpanRect(li, bp, bp + 1, 0.0f, [&](CGRect r) {
                    CGRect brRect = CGRectMake(r.origin.x, r.origin.y + 1.0f, r.size.width, lineHeight - 2.0f);
                    if (colBracketBg) {
                        CGContextSetFillColorWithColor(ctx, colBracketBg);
                        CGContextFillRect(ctx, brRect);
                    }
                    if (colBracketBorder) {
                        CGContextSetStrokeColorWithColor(ctx, colBracketBorder);
                        CGContextSetLineWidth(ctx, 1.0f);
                        CGContextStrokeRect(ctx, brRect);
                    }
                });
            }
        }
    }

    // 4. Selection rectangles
    CGContextSetFillColorWithColor(ctx, colSel);
    bool isRectMode = (cursors.size() > 1);
    for (const auto& c : cursors) {
        if (!c.hasSelection()) continue;
        size_t pStart = c.start(), pEnd = c.end();
        int lStart = getLineIdx(pStart), lEnd = getLineIdx(pEnd);
        for (int l = std::max(lStart, renderStart); l <= std::min(lEnd, end - 1); ++l) {
            size_t lineBegin = lineStarts[l], lineEnd = (l + 1 < (int)lineStarts.size()) ? lineStarts[l + 1] : pt.length();
            if (lineEnd > lineBegin && pt.charAt(lineEnd - 1) == '\n') lineEnd--;
            if (lineEnd > lineBegin && pt.charAt(lineEnd - 1) == '\r') lineEnd--;
            if (isRectMode && !wordWrapEnabled) {
                float y = lineBaseVisualY[l - renderStart];
                float rx1 = std::min(c.originalAnchorX, c.desiredX), rx2 = std::max(c.originalAnchorX, c.desiredX);
                if (rx2 > rx1) CGContextFillRect(ctx, CGRectMake(gutterWidth - (float)hScrollPos + rx1, y, rx2 - rx1, lineHeight));
            } else {
                size_t selS = std::max(pStart, lineBegin), selE = std::min(pEnd, lineEnd);
                float extraW = (selE == lineEnd && pEnd > lineEnd) ? charWidth : 0.0f;
                if (selS < selE || extraW > 0.0f) {
                    forEachVisualSpanRect(l, selS, selE, extraW, [&](CGRect r) {
                        CGContextFillRect(ctx, r);
                    });
                }
            }
        }
    }

    // 5. Text lines + 49-category Syntax Highlighting + Soft Wrap + Newline symbols
    struct GutterEntry { int lineNumber; float y; };
    std::vector<GutterEntry> gutterEntries;
    gutterEntries.reserve(vis + 4);
    std::vector<SyntaxColor> lineSyntaxColors;

    float curVisualY = lineBaseVisualY[0];
    for (int i = renderStart; i < (int)lineStarts.size() && curVisualY < vh + lineHeight; ++i) {
        size_t s = lineStarts[i];
        size_t e = (i + 1 < (int)lineStarts.size()) ? lineStarts[i + 1] : pt.length();
        size_t contentEnd = e;
        if (contentEnd > s && pt.charAt(contentEnd - 1) == '\n') contentEnd--;
        if (contentEnd > s && pt.charAt(contentEnd - 1) == '\r') contentEnd--;

        std::string ls = pt.getRange(s, contentEnd - s);
        size_t imIdx = std::string::npos;
        if (!imeComp.empty() && !cursors.empty() && getLineIdx(cursors.back().head) == i) {
            size_t cp = cursors.back().head;
            if (cp >= s && cp <= contentEnd) {
                imIdx = cp - s;
                ls.insert(imIdx, imeComp);
            }
        }

        bool inBlockComment = checkInBlockComment(i);
        if (currentSyntax.active && !ls.empty()) {
            scanSyntaxHighlighting(ls, currentSyntax, inBlockComment, lineSyntaxColors);
        } else {
            lineSyntaxColors.clear();
        }

        auto subs = getVisualSubLines(i, vw);
        int startSub = (i == start && wordWrapEnabled) ? std::clamp(vScrollSubLine, 0, (int)subs.size() - 1) : 0;

        int imeOwnerSub = -1;
        if (imIdx != std::string::npos && !subs.empty()) {
            bool caretAffinity = cursors.back().affinity;
            imeOwnerSub = (int)subs.size() - 1;
            for (int sIdx = 0; sIdx < (int)subs.size(); ++sIdx) {
                size_t se = subs[sIdx].byteStart + subs[sIdx].byteLen;
                if (imIdx < se || (imIdx == se && (caretAffinity || sIdx == (int)subs.size() - 1))) {
                    imeOwnerSub = sIdx;
                    break;
                }
            }
        }

        for (int subIdx = startSub; subIdx < (int)subs.size(); ++subIdx) {
            float drawY = wordWrapEnabled ? curVisualY : ((float)(i - start) * lineHeight);
            if (subIdx == 0) {
                gutterEntries.push_back({ i + 1, drawY });
            }
            const auto& sub = subs[subIdx];
            size_t lsSubStart = sub.byteStart;
            size_t lsSubLen = sub.byteLen;
            if (imIdx != std::string::npos) {
                if (subIdx == imeOwnerSub) {
                    lsSubLen += imeComp.size();
                } else if (subIdx > imeOwnerSub) {
                    lsSubStart += imeComp.size();
                }
            }
            if (lsSubLen > 0 && lsSubStart + lsSubLen <= ls.size()) {
                std::string subStr = ls.substr(lsSubStart, lsSubLen);
                CFStringRef cf = CFStringCreateWithBytes(NULL, (const UInt8*)subStr.data(), subStr.size(), kCFStringEncodingUTF8, false);
                if (cf) {
                    CFIndex cfLen = CFStringGetLength(cf);
                    CFMutableAttributedStringRef mas = CFAttributedStringCreateMutable(NULL, 0);
                    CFAttributedStringReplaceString(mas, CFRangeMake(0, 0), cf);
                    CFAttributedStringSetAttribute(mas, CFRangeMake(0, cfLen), kCTFontAttributeName, fontRef);
                    CFAttributedStringSetAttribute(mas, CFRangeMake(0, cfLen), kCTForegroundColorAttributeName, colText);
                    CFAttributedStringSetAttribute(mas, CFRangeMake(0, cfLen), kCTParagraphStyleAttributeName, paragraphStyle);

                    // Apply syntax highlight runs
                    if (!lineSyntaxColors.empty()) {
                        size_t runStart = 0;
                        while (runStart < lsSubLen && lsSubStart + runStart < lineSyntaxColors.size()) {
                            SyntaxColor sc = lineSyntaxColors[lsSubStart + runStart];
                            size_t runEnd = runStart + 1;
                            while (runEnd < lsSubLen && lsSubStart + runEnd < lineSyntaxColors.size() && lineSyntaxColors[lsSubStart + runEnd] == sc) {
                                runEnd++;
                            }
                            if (sc.a > 0.0f) {
                                CFStringRef preCf = CFStringCreateWithBytes(NULL, (const UInt8*)subStr.data(), runStart, kCFStringEncodingUTF8, false);
                                CFStringRef segCf = CFStringCreateWithBytes(NULL, (const UInt8*)(subStr.data() + runStart), runEnd - runStart, kCFStringEncodingUTF8, false);
                                CFIndex u16S = preCf ? CFStringGetLength(preCf) : 0;
                                CFIndex u16L = segCf ? CFStringGetLength(segCf) : 0;
                                if (u16L > 0 && u16S + u16L <= cfLen) {
                                    CGColorRef synCol = CGColorCreateGenericRGB(sc.r, sc.g, sc.b, sc.a);
                                    CFAttributedStringSetAttribute(mas, CFRangeMake(u16S, u16L), kCTForegroundColorAttributeName, synCol);
                                    CGColorRelease(synCol);
                                }
                                if (preCf) CFRelease(preCf);
                                if (segCf) CFRelease(segCf);
                            }
                            runStart = runEnd;
                        }
                    }

                    if (imIdx != std::string::npos && subIdx == imeOwnerSub && imIdx >= lsSubStart && imIdx <= lsSubStart + lsSubLen) {
                        size_t localIm = imIdx - lsSubStart;
                        CFStringRef prefix = CFStringCreateWithBytes(NULL, (const UInt8*)subStr.data(), localIm, kCFStringEncodingUTF8, false);
                        CFIndex prefixLen = prefix ? CFStringGetLength(prefix) : 0;
                        CFStringRef icf = CFStringCreateWithBytes(NULL, (const UInt8*)imeComp.data(), imeComp.size(), kCFStringEncodingUTF8, false);
                        CFIndex imeLen = icf ? CFStringGetLength(icf) : 0;
                        int32_t style = kCTUnderlineStyleSingle;
                        CFNumberRef n = CFNumberCreate(NULL, kCFNumberSInt32Type, &style);
                        if (prefixLen + imeLen <= cfLen) {
                            CFAttributedStringSetAttribute(mas, CFRangeMake(prefixLen, imeLen), kCTUnderlineStyleAttributeName, n);
                        }
                        CFRelease(n);
                        if (prefix) CFRelease(prefix);
                        if (icf) CFRelease(icf);
                    }

                    CTLineRef tl = CTLineCreateWithAttributedString(mas);
                    float drawX = gutterWidth - (wordWrapEnabled ? 0.0f : (float)hScrollPos);
                    CGContextSetTextPosition(ctx, drawX, drawY + baselineOffset);
                    CTLineDraw(tl, ctx);
                    CFRelease(tl);
                    CFRelease(mas);
                    CFRelease(cf);
                }
            }

            // Draw newline symbol on the last visual sub-line
            if (subIdx == (int)subs.size() - 1 && e > s) {
                bool isCRLF = false, isLF = false, isCR = false;
                size_t newlinePos = contentEnd;
                if (e - s >= 2 && pt.charAt(e - 2) == '\r' && pt.charAt(e - 1) == '\n') isCRLF = true;
                else if (pt.charAt(e - 1) == '\n') isLF = true;
                else if (pt.charAt(e - 1) == '\r') isCR = true;

                if (isCRLF || isLF || isCR) {
                    float px = getXInLine(i, newlinePos) - (wordWrapEnabled ? getXInLine(i, s + sub.byteStart) : 0.0f);
                    float cx = gutterWidth - (wordWrapEnabled ? 0.0f : (float)hScrollPos) + px + charWidth * 0.5f;
                    float cy = drawY + lineHeight * 0.5f;
                    if (colNewlineMark) CGContextSetStrokeColorWithColor(ctx, colNewlineMark);
                    else CGContextSetRGBStrokeColor(ctx, isDarkMode ? 1.0f : 0.50f, isDarkMode ? 1.0f : 0.50f, isDarkMode ? 1.0f : 0.50f, isDarkMode ? 0.18f : 0.20f);
                    float strokeWidth = std::max(1.5f, currentFontSize * 0.05f);
                    CGContextSetLineWidth(ctx, strokeWidth);
                    CGContextSetLineCap(ctx, kCGLineCapRound);
                    CGContextSetLineJoin(ctx, kCGLineJoinRound);
                    float sz = charWidth * 1.0f;
                    float halfSz = sz * 0.5f;
                    float arrowSize = sz * 0.35f;
                    CGContextBeginPath(ctx);
                    if (isCRLF) {
                        float vLineTop = cy - halfSz;
                        float vLineBottom = cy + halfSz * 0.3f;
                        float hLineRight = cx + halfSz * 0.6f;
                        float hLineLeft = cx - halfSz * 0.6f;
                        CGContextMoveToPoint(ctx, hLineRight, vLineTop);
                        CGContextAddLineToPoint(ctx, hLineRight, vLineBottom);
                        CGContextAddLineToPoint(ctx, hLineLeft, vLineBottom);
                        CGContextMoveToPoint(ctx, hLineLeft, vLineBottom);
                        CGContextAddLineToPoint(ctx, hLineLeft + arrowSize, vLineBottom - arrowSize);
                        CGContextMoveToPoint(ctx, hLineLeft, vLineBottom);
                        CGContextAddLineToPoint(ctx, hLineLeft + arrowSize, vLineBottom + arrowSize);
                    } else if (isCR) {
                        float hLineRight = cx + halfSz * 0.6f;
                        float hLineLeft = cx - halfSz * 0.6f;
                        CGContextMoveToPoint(ctx, hLineRight, cy);
                        CGContextAddLineToPoint(ctx, hLineLeft, cy);
                        CGContextMoveToPoint(ctx, hLineLeft, cy);
                        CGContextAddLineToPoint(ctx, hLineLeft + arrowSize, cy - arrowSize);
                        CGContextMoveToPoint(ctx, hLineLeft, cy);
                        CGContextAddLineToPoint(ctx, hLineLeft + arrowSize, cy + arrowSize);
                    } else {
                        float stemTop = cy - halfSz * 0.8f;
                        float stemBottom = cy + halfSz * 0.8f;
                        CGContextMoveToPoint(ctx, cx, stemTop);
                        CGContextAddLineToPoint(ctx, cx, stemBottom);
                        CGContextMoveToPoint(ctx, cx, stemBottom);
                        CGContextAddLineToPoint(ctx, cx - arrowSize, stemBottom - arrowSize);
                        CGContextMoveToPoint(ctx, cx, stemBottom);
                        CGContextAddLineToPoint(ctx, cx + arrowSize, stemBottom - arrowSize);
                    }
                    CGContextStrokePath(ctx);
                }
            }
            curVisualY += lineHeight;
            if (!wordWrapEnabled) break;
        }
    }

    // 6. Carets & Drag-drop target caret
    CGColorRef caretColorToUse = colCaret;
    CGColorRef overwriteColor = nullptr;
    if (isOverwriteMode) {
        overwriteColor = isDarkMode ? CGColorCreateGenericRGB(1.0, 1.0, 1.0, 0.45) : CGColorCreateGenericRGB(0.0, 0.0, 0.0, 0.45);
        caretColorToUse = overwriteColor;
    }
    CGContextSetFillColorWithColor(ctx, caretColorToUse);
    for (const auto& c : cursors) {
        int l = getLineIdx(c.head);
        if (l >= start && l < end) {
            float physX = getXInLine(l, c.head);
            float drawX = c.isVirtual ? std::max(physX, c.desiredX) : physX;
            float drawY = lineBaseVisualY[l - renderStart];
            if (wordWrapEnabled) {
                auto subs = getVisualSubLines(l, vw);
                size_t rel = (c.head >= lineStarts[l]) ? (c.head - lineStarts[l]) : 0;
                int subIdx = 0;
                for (int sIdx = 0; sIdx < (int)subs.size(); ++sIdx) {
                    size_t se = subs[sIdx].byteStart + subs[sIdx].byteLen;
                    if (rel < se || (rel == se && (c.affinity || sIdx == (int)subs.size() - 1))) {
                        subIdx = sIdx;
                        break;
                    }
                }
                int startSub = (l == start) ? std::clamp(vScrollSubLine, 0, std::max(0, (int)subs.size() - 1)) : 0;
                drawX = physX - getXInLine(l, lineStarts[l] + subs[subIdx].byteStart);
                drawY = lineBaseVisualY[l - renderStart] + (float)(subIdx - startSub) * lineHeight;
            }
            float caretW = 2.0f;
            if (isOverwriteMode && !c.hasSelection()) {
                char ch = (c.head < pt.length()) ? pt.charAt(c.head) : 0;
                if (ch != 0 && ch != '\n' && ch != '\r') {
                    size_t nextPos = moveCaretVisual(c.head, true);
                    caretW = std::max(charWidth, getXInLine(l, nextPos) - physX);
                } else {
                    caretW = charWidth;
                }
            }
            CGContextFillRect(ctx, CGRectMake(gutterWidth - (wordWrapEnabled ? 0.0f : (float)hScrollPos) + drawX, drawY, caretW, lineHeight));
        }
    }
    if (overwriteColor) CGColorRelease(overwriteColor);
    if (isDraggingSelection) {
        CGContextSetFillColorWithColor(ctx, colCaret);
        int dl = getLineIdx(dragDropTargetPos);
        if (dl >= start && dl < end) {
            forEachVisualSpanRect(dl, dragDropTargetPos, dragDropTargetPos, 0.0f, [&](CGRect r) {
                CGContextFillRect(ctx, CGRectMake(r.origin.x, r.origin.y, 2.5f, lineHeight));
            });
        }
    }
    CGContextRestoreGState(ctx);

    // 7. Gutter line numbers (aligned to the exact same baselineOffset as body text)
    CGContextSetFillColorWithColor(ctx, colGutterBg);
#if TARGET_OS_IOS
    CGContextFillRect(ctx, CGRectMake(0, -200.0f, gutterWidth, h + 200.0f));
#else
    CGContextFillRect(ctx, CGRectMake(0, 0, gutterWidth, h));
#endif
    for (const auto& ge : gutterEntries) {
        CFStringRef n = CFStringCreateWithCString(NULL, std::to_string(ge.lineNumber).c_str(), kCFStringEncodingUTF8);
        if (n) {
            const void* keys[] = { kCTFontAttributeName, kCTForegroundColorAttributeName, kCTParagraphStyleAttributeName };
            const void* values[] = { fontRef, colGutterText, paragraphStyle };
            CFDictionaryRef attr = CFDictionaryCreate(NULL, keys, values, 3, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
            CFAttributedStringRef nas = CFAttributedStringCreate(NULL, n, attr);
            CTLineRef nl = CTLineCreateWithAttributedString(nas);
            double lineWidth = CTLineGetTypographicBounds(nl, NULL, NULL, NULL);
            float xPos = gutterWidth - (float)lineWidth - (charWidth * 0.5f);
            if (xPos < 5.0f) xPos = 5.0f;
            CGContextSetTextPosition(ctx, xPos, ge.y + baselineOffset);
            CTLineDraw(nl, ctx);
            CFRelease(nl);
            CFRelease(nas);
            CFRelease(attr);
            CFRelease(n);
        }
    }

    // 8. Zoom / Toast Popup
    auto now = std::chrono::steady_clock::now();
    if (now < zoomPopupEndTime) {
        float zw = 180.0f, zh = 72.0f;
        CGRect zpr = CGRectMake((w - zw) / 2.0f, (h - zh) / 2.0f, zw, zh);
        CGColorRef bgCol = isDarkMode
            ? CGColorCreateGenericRGB(0.20, 0.20, 0.20, 0.70)
            : CGColorCreateGenericRGB(0.93, 0.93, 0.93, 0.85);
        CGContextSetFillColorWithColor(ctx, bgCol);
        CGPathRef path = CGPathCreateWithRoundedRect(zpr, 10.0f, 10.0f, NULL);
        CGContextAddPath(ctx, path);
        CGContextFillPath(ctx);
        CGPathRelease(path);
        CGColorRelease(bgCol);

        CFStringRef zcf = CFStringCreateWithCString(NULL, zoomPopupText.c_str(), kCFStringEncodingUTF8);
        if (zcf) {
            CTFontRef zf = CTFontCreateWithName(CFSTR("Helvetica-Bold"), 22.0f, NULL);
            CGColorRef zTextCol = isDarkMode
                ? CGColorCreateGenericRGB(1.0, 1.0, 1.0, 1.0)
                : CGColorCreateGenericRGB(0.0, 0.0, 0.0, 1.0);
            const void* zKeys[] = { kCTFontAttributeName, kCTForegroundColorAttributeName };
            const void* zVals[] = { zf, zTextCol };
            CFDictionaryRef za = CFDictionaryCreate(NULL, zKeys, zVals, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
            CFAttributedStringRef zas = CFAttributedStringCreate(NULL, zcf, za);
            CTLineRef zl = CTLineCreateWithAttributedString(zas);
            double lineWidth = CTLineGetTypographicBounds(zl, NULL, NULL, NULL);
            CGFloat zAsc = CTFontGetAscent(zf);
            CGFloat zDesc = CTFontGetDescent(zf);
            float textY = zpr.origin.y + (zh - (zAsc + zDesc)) / 2.0f + zAsc;
            CGContextSetTextPosition(ctx, zpr.origin.x + (zw - (float)lineWidth) / 2.0f, textY);
            CTLineDraw(zl, ctx);
            CFRelease(zl);
            CFRelease(zas);
            CFRelease(za);
            CFRelease(zf);
            CFRelease(zcf);
            CGColorRelease(zTextCol);
        }
        if (cbNeedsDisplay) {
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
                cbNeedsDisplay();
            });
        }
    }

    // 9. F1 Help Popup
    if (showHelpPopup) {
        std::wstring fullHelp = appVersionStr + L"\n\n" + helpTextStr;
        while (!fullHelp.empty() && (fullHelp.back() == L'\n' || fullHelp.back() == L'\r')) {
            fullHelp.pop_back();
        }
        CTFontRef hf = CTFontCreateWithName(CFSTR("Menlo"), 13.0f, NULL);
        CGFloat helpLineHeight = 16.0f;
        CGFloat helpAscent = CTFontGetAscent(hf);
        CGColorRef helpTextCol = isDarkMode
            ? CGColorCreateGenericRGB(1.0, 1.0, 1.0, 1.0)
            : CGColorCreateGenericRGB(0.0, 0.0, 0.0, 1.0);
        const void* hKeys[] = { kCTFontAttributeName, kCTForegroundColorAttributeName };
        const void* hVals[] = { hf, helpTextCol };
        CFDictionaryRef ha = CFDictionaryCreate(NULL, hKeys, hVals, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

        float helpCharW = 7.83f;
        {
            CFAttributedStringRef zeroAs = CFAttributedStringCreate(NULL, CFSTR("0"), ha);
            if (zeroAs) {
                CTLineRef zeroLine = CTLineCreateWithAttributedString(zeroAs);
                if (zeroLine) {
                    helpCharW = (float)CTLineGetTypographicBounds(zeroLine, NULL, NULL, NULL);
                    CFRelease(zeroLine);
                }
                CFRelease(zeroAs);
            }
        }
        float col2Offset = 20.0f * helpCharW;

        struct HelpRenderLine {
            CTLineRef leftLine = nullptr;
            CTLineRef rightLine = nullptr;
            float rightX = 0.0f;
        };
        std::vector<HelpRenderLine> renderLines;
        float maxLineW = 0.0f;

        auto makeCTLine = [&](const std::wstring& wstr) -> CTLineRef {
            if (wstr.empty()) return nullptr;
            CFStringRef cfs = CFStringCreateWithBytes(NULL, (const UInt8*)wstr.data(), wstr.size() * sizeof(wchar_t), kCFStringEncodingUTF32LE, false);
            if (!cfs) return nullptr;
            CFAttributedStringRef as = CFAttributedStringCreate(NULL, cfs, ha);
            CFRelease(cfs);
            if (!as) return nullptr;
            CTLineRef tl = CTLineCreateWithAttributedString(as);
            CFRelease(as);
            return tl;
        };

        size_t pos = 0;
        while (pos <= fullHelp.size()) {
            size_t nextPos = fullHelp.find(L'\n', pos);
            std::wstring line = (nextPos == std::wstring::npos) ? fullHelp.substr(pos) : fullHelp.substr(pos, nextPos - pos);
            if (!line.empty() && line.back() == L'\r') line.pop_back();

            HelpRenderLine rl;
            size_t sepStart = line.find(L"  ");
            if (sepStart != std::wstring::npos && sepStart > 0) {
                size_t sepEnd = sepStart;
                while (sepEnd < line.size() && line[sepEnd] == L' ') sepEnd++;
                std::wstring leftWStr = line.substr(0, sepStart);
                std::wstring rightWStr = (sepEnd < line.size()) ? line.substr(sepEnd) : L"";
                CTLineRef lLine = makeCTLine(leftWStr);
                double lW = lLine ? CTLineGetTypographicBounds(lLine, NULL, NULL, NULL) : 0.0;
                if (lLine && !rightWStr.empty() && (float)lW <= 18.8f * helpCharW) {
                    rl.leftLine = lLine;
                    rl.rightLine = makeCTLine(rightWStr);
                    rl.rightX = col2Offset;
                    double rW = rl.rightLine ? CTLineGetTypographicBounds(rl.rightLine, NULL, NULL, NULL) : 0.0;
                    maxLineW = std::max(maxLineW, col2Offset + (float)rW);
                } else {
                    if (lLine) CFRelease(lLine);
                    rl.leftLine = makeCTLine(line);
                    double fullW = rl.leftLine ? CTLineGetTypographicBounds(rl.leftLine, NULL, NULL, NULL) : 0.0;
                    maxLineW = std::max(maxLineW, (float)fullW);
                }
            } else {
                rl.leftLine = makeCTLine(line);
                double fullW = rl.leftLine ? CTLineGetTypographicBounds(rl.leftLine, NULL, NULL, NULL) : 0.0;
                maxLineW = std::max(maxLineW, (float)fullW);
            }
            renderLines.push_back(rl);
            if (nextPos == std::wstring::npos) break;
            pos = nextPos + 1;
        }

        float padding = 20.0f;
        float contentW = maxLineW;
        float contentH = (float)renderLines.size() * helpLineHeight;
        float hw = contentW + (padding * 2.0f);
        float hh = contentH + (padding * 2.0f);
        float scale = 1.0f;
        if (hw > w * 0.94f && hw > 1.0f) scale = std::min(scale, (w * 0.94f) / hw);
        if (hh > h * 0.94f && hh > 1.0f) scale = std::min(scale, (h * 0.94f) / hh);
        float boxW = hw * scale;
        float boxH = hh * scale;
        CGRect hr = CGRectMake((w - boxW) / 2.0f, (h - boxH) / 2.0f, boxW, boxH);

        CGColorRef hBg = isDarkMode
            ? CGColorCreateGenericRGB(0.20, 0.20, 0.20, 0.70)
            : CGColorCreateGenericRGB(0.93, 0.93, 0.93, 0.85);
        CGContextSetFillColorWithColor(ctx, hBg);
        CGPathRef hpath = CGPathCreateWithRoundedRect(hr, 10.0f, 10.0f, NULL);
        CGContextAddPath(ctx, hpath);
        CGContextFillPath(ctx);
        if (!isDarkMode) {
            CGColorRef hBorder = CGColorCreateGenericRGB(0.80, 0.80, 0.80, 1.0);
            CGContextSetStrokeColorWithColor(ctx, hBorder);
            CGContextSetLineWidth(ctx, 1.0f);
            CGContextAddPath(ctx, hpath);
            CGContextStrokePath(ctx);
            CGColorRelease(hBorder);
        }
        CGPathRelease(hpath);
        CGColorRelease(hBg);

        CGContextSaveGState(ctx);
        CGContextTranslateCTM(ctx, hr.origin.x, hr.origin.y);
        CGContextScaleCTM(ctx, scale, scale);
        CGContextSetTextMatrix(ctx, CGAffineTransformMakeScale(1.0f, -1.0f));
        for (size_t i = 0; i < renderLines.size(); ++i) {
            float y = padding + (float)i * helpLineHeight + helpAscent;
            if (renderLines[i].leftLine) {
                CGContextSetTextPosition(ctx, padding, y);
                CTLineDraw(renderLines[i].leftLine, ctx);
                CFRelease(renderLines[i].leftLine);
            }
            if (renderLines[i].rightLine) {
                CGContextSetTextPosition(ctx, padding + renderLines[i].rightX, y);
                CTLineDraw(renderLines[i].rightLine, ctx);
                CFRelease(renderLines[i].rightLine);
            }
        }
        CGContextRestoreGState(ctx);
        CFRelease(ha);
        CFRelease(hf);
        CGColorRelease(helpTextCol);
    }
}
#endif

Editor::~Editor() {
    saveCurrentViewState();
#if defined(__APPLE__)
    if (colBackground) CGColorRelease(colBackground);
    if (colText) CGColorRelease(colText);
    if (colGutterBg) CGColorRelease(colGutterBg);
    if (colGutterText) CGColorRelease(colGutterText);
    if (colSel) CGColorRelease(colSel);
    if (colCaret) CGColorRelease(colCaret);
    if (colNewlineMark) CGColorRelease(colNewlineMark);
    if (colBracketBg) CGColorRelease(colBracketBg);
    if (colBracketBorder) CGColorRelease(colBracketBorder);
    if (fontRef) CFRelease(fontRef);
    if (paragraphStyle) CFRelease(paragraphStyle);
#endif
}
