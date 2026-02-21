#define NOMINMAX
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <fstream>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <regex>
#include <chrono>
#include <numeric>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#import <Cocoa/Cocoa.h>
#import <CoreText/CoreText.h>

const std::wstring APP_VERSION = L"miu v1.0.14";
const std::wstring APP_TITLE = L"miu";
enum Encoding { ENC_UTF8_NOBOM = 0, ENC_UTF8_BOM, ENC_UTF16LE, ENC_UTF16BE, ENC_ANSI };
static NSString *const kMiuRectangularSelectionType = @"com.kenji.miu.rectangular";

struct Editor;

@interface EditorView : NSView <NSTextInputClient>
{
@public
    std::shared_ptr<Editor> editor;
}
- (void)updateScrollers;
- (void)applyZoom:(float)val relative:(bool)rel;
@end

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
    std::vector<UniChar> buf(len);
    CFStringGetCharacters(cfStr, CFRangeMake(0, len), buf.data());
    std::wstring wstr;
    for(CFIndex i=0; i<len; ++i) wstr.push_back((wchar_t)buf[i]);
    return wstr;
}
static std::string WToUTF8(const std::wstring& w) {
    CFStringRef str = CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8*)w.data(), w.size() * sizeof(wchar_t), kCFStringEncodingUTF32LE, false);
    std::string res = CFStringToStdString(str);
    if (str) CFRelease(str);
    return res;
}
static std::wstring UTF8ToW(const std::string& s) {
    CFStringRef str = CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8*)s.data(), s.size(), kCFStringEncodingUTF8, false);
    std::wstring res = CFStringToStdWString(str);
    if (str) CFRelease(str);
    return res;
}
static std::string Utf16ToUtf8(const char* data, size_t len, bool isBigEndian) {
    if (len < 2) return "";
    CFStringRef str = CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8*)(data + 2), len - 2, isBigEndian ? kCFStringEncodingUTF16BE : kCFStringEncodingUTF16LE, false);
    std::string res = CFStringToStdString(str);
    if (str) CFRelease(str);
    return res;
}
static std::string AnsiToUtf8(const char* data, size_t len) {
    CFStringRef str = CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8*)data, len, kCFStringEncodingWindowsLatin1, false);
    std::string res = CFStringToStdString(str);
    if (str) CFRelease(str);
    return res;
}
static Encoding DetectEncoding(const char* buf, size_t len) {
    if (len >= 3 && (unsigned char)buf[0] == 0xEF && (unsigned char)buf[1] == 0xBB && (unsigned char)buf[2] == 0xBF) return ENC_UTF8_BOM;
    if (len >= 2) {
        if ((unsigned char)buf[0] == 0xFF && (unsigned char)buf[1] == 0xFE) return ENC_UTF16LE;
        if ((unsigned char)buf[0] == 0xFE && (unsigned char)buf[1] == 0xFF) return ENC_UTF16BE;
    }
    return ENC_UTF8_NOBOM;
}
static std::string ConvertCase(const std::string& s, bool toUpper) {
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
struct Piece { bool isOriginal; size_t start; size_t len; };
struct PieceTable {
    const char* origPtr = nullptr; size_t origSize = 0;
    std::string addBuf; std::vector<Piece> pieces;
    void initFromFile(const char* data, size_t size) { origPtr = data; origSize = size; pieces.clear(); addBuf.clear(); if (size > 0) pieces.push_back({ true, 0, size }); }
    void initEmpty() { origPtr = nullptr; origSize = 0; pieces.clear(); addBuf.clear(); }
    size_t length() const { size_t s = 0; for (auto& p : pieces) s += p.len; return s; }
    std::string getRange(size_t pos, size_t count) const {
        std::string out; out.reserve(std::min(count, (size_t)4096));
        size_t cur = 0;
        for (const auto& p : pieces) {
            if (cur + p.len <= pos) { cur += p.len; continue; }
            size_t localStart = (pos > cur) ? (pos - cur) : 0;
            size_t take = std::min(p.len - localStart, count - out.size());
            if (take == 0) break;
            p.isOriginal ? out.append(origPtr + p.start + localStart, take) : out.append(addBuf.data() + p.start + localStart, take);
            if (out.size() >= count) break;
            cur += p.len;
        }
        return out;
    }
    char charAt(size_t pos) const {
        size_t cur = 0;
        for (const auto& p : pieces) {
            if (cur + p.len <= pos) { cur += p.len; continue; }
            return p.isOriginal ? origPtr[p.start + pos - cur] : addBuf[p.start + pos - cur];
        }
        return ' ';
    }
    void insert(size_t pos, const std::string& s) {
        if (s.empty()) return;
        size_t cur = 0, idx = 0;
        while (idx < pieces.size() && cur + pieces[idx].len < pos) { cur += pieces[idx].len; ++idx; }
        if (idx < pieces.size()) {
            Piece p = pieces[idx]; size_t offset = pos - cur;
            if (offset > 0 && offset < p.len) {
                pieces[idx] = { p.isOriginal, p.start, offset };
                pieces.insert(pieces.begin() + idx + 1, { p.isOriginal, p.start + offset, p.len - offset });
                idx++;
            } else if (offset == p.len) idx++;
        }
        size_t addStart = addBuf.size(); addBuf.append(s);
        pieces.insert(pieces.begin() + idx, { false, addStart, (size_t)s.size() });
    }
    void erase(size_t pos, size_t count) {
        if (count == 0) return;
        size_t cur = 0, idx = 0;
        while (idx < pieces.size() && cur + pieces[idx].len <= pos) { cur += pieces[idx].len; ++idx; }
        if (idx >= pieces.size()) return;
        if (pos > cur) {
            Piece p = pieces[idx]; size_t leftLen = pos - cur;
            pieces[idx] = { p.isOriginal, p.start, leftLen };
            pieces.insert(pieces.begin() + idx + 1, { p.isOriginal, p.start + leftLen, p.len - leftLen });
            idx++;
        }
        size_t remaining = count;
        while (idx < pieces.size() && remaining > 0) {
            if (pieces[idx].len <= remaining) { remaining -= pieces[idx].len; pieces.erase(pieces.begin() + idx); }
            else { pieces[idx].start += remaining; pieces[idx].len -= remaining; remaining = 0; }
        }
    }
};
struct Cursor {
    size_t head, anchor;
    float desiredX;
    float originalAnchorX;
    bool isVirtual;
    size_t start() const { return std::min(head, anchor); }
    size_t end() const { return std::max(head, anchor); }
    bool hasSelection() const { return head != anchor; }
};
struct EditOp { enum Type { Insert, Erase } type; size_t pos; std::string text; };
struct EditBatch { std::vector<EditOp> ops; std::vector<Cursor> beforeCursors, afterCursors; };
struct UndoManager {
    std::vector<EditBatch> undoStack, redoStack; int savePoint = 0;
    void clear() { undoStack.clear(); redoStack.clear(); savePoint = 0; }
    void markSaved() { savePoint = (int)undoStack.size(); }
    bool isModified() const { return (int)undoStack.size() != savePoint; }
    void push(const EditBatch& b) { undoStack.push_back(b); redoStack.clear(); }
    EditBatch popUndo() { EditBatch e = undoStack.back(); undoStack.pop_back(); redoStack.push_back(e); return e; }
    EditBatch popRedo() { EditBatch e = redoStack.back(); redoStack.pop_back(); undoStack.push_back(e); return e; }
};
struct MappedFile {
    int fd = -1; char* ptr = nullptr; size_t size = 0;
    bool open(const char* path) {
        fd = ::open(path, O_RDONLY); if (fd == -1) return false;
        struct stat sb; if (fstat(fd, &sb) == -1) { ::close(fd); return false; }
        size = sb.st_size; if (size == 0) { ptr = nullptr; return true; }
        ptr = (char*)mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0); return (ptr != MAP_FAILED);
    }
    void close() { if (ptr && ptr != MAP_FAILED) munmap(ptr, size); if (fd != -1) ::close(fd); ptr = nullptr; fd = -1; }
    ~MappedFile() { close(); }
};

struct Editor {
    EditorView* view = nullptr;
    PieceTable pt;
    UndoManager undo;
    std::unique_ptr<MappedFile> fileMap;
    std::wstring currentFilePath;
    Encoding currentEncoding = ENC_UTF8_NOBOM;
    bool isDirty = false;
    bool isDarkMode = false;
    bool showHelpPopup = false;
    std::chrono::steady_clock::time_point zoomPopupEndTime;
    std::string zoomPopupText = "";
    float currentFontSize = 14.0f;
    float lineHeight = 18.0f;
    float charWidth = 8.0f;
    float gutterWidth = 40.0f;
    float maxLineWidth = 100.0f;
    int vScrollPos = 0, hScrollPos = 0;
    float visibleVScrollWidth = 0.0f;
    float visibleHScrollHeight = 0.0f;
    std::vector<Cursor> cursors;
    std::vector<size_t> lineStarts;
    std::string imeComp;
    std::string newlineStr = "\n";
    CGColorRef colBackground=NULL, colText=NULL, colGutterBg=NULL, colGutterText=NULL, colSel=NULL, colCaret=NULL;
    CTFontRef fontRef = nullptr;
    std::wstring helpTextStr =
            L"[Shortcuts]\n"
            L"F1                    Help\n"
            L"Cmd+N                 New\n"
            L"Cmd+O / Drag&Drop     Open\n"
            L"Cmd+S                 Save\n"
            L"Cmd+Shift+S           Save As\n"
            L"Cmd+F                 Find\n"
            L"Cmd+H                 Replace\n"
            L"F3 / Shift+F3         Find Next/Prev\n"
            L"Cmd+G                 Go To Line\n"
            L"Cmd+Z/Shift+Z         Undo/Redo\n"
            L"Cmd+X/C/V             Cut/Copy/Paste\n"
            L"Cmd+Shift+K           Delete Line\n"
            L"Cmd+U                 Upper Case\n"
            L"Cmd+Shift+U           Lower Case\n"
            L"Option+Up/Down        Move Line\n"
            L"Option+Shift+Up/Down  Copy Line\n"
            L"Cmd+D                 Select Word / Next\n"
            L"Cmd+A                 Select All\n"
            L"Option+Drag           Rect Select\n"
            L"Cmd+Wheel/+/-         Zoom\n"
            L"Cmd+0                 Reset Zoom\n"
            L"Ctrl+Cmd+F            Full Screen\n";
    void detectNewlineStyle(const char* buf, size_t len) {
        size_t checkLen = (len > 4096) ? 4096 : len;
        for (size_t i = 0; i < checkLen; ++i) {
            if (buf[i] == '\r') {
                if (i + 1 < checkLen && buf[i + 1] == '\n') {
                    newlineStr = "\r\n";
                    return;
                }
                newlineStr = "\r";
                return;
            }
            else if (buf[i] == '\n') {
                newlineStr = "\n";
                return;
            }
        }
        newlineStr = "\n";
    }
    void insertAtCursorsWithPadding(const std::string& text) {
        if (cursors.empty()) return;
        EditBatch batch; batch.beforeCursors = cursors;
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
                c.head = c.anchor = st; c.desiredX = getXInLine(li, st); lineEnd -= len;
            }
            float currentLineEndX = getXInLine(li, lineEnd);
            size_t targetPos = c.head;
            if (c.desiredX > currentLineEndX + (charWidth * 0.5f)) {
                int spacesNeeded = (int)((c.desiredX - currentLineEndX) / charWidth + 0.5f);
                if (spacesNeeded > 0) {
                    std::string padding(spacesNeeded, ' '); pt.insert(lineEnd, padding);
                    batch.ops.push_back({ EditOp::Insert, lineEnd, padding }); targetPos = lineEnd + padding.size();
                    for (auto& oc : cursors) { if (oc.head >= lineEnd) oc.head += padding.size(); if (oc.anchor >= lineEnd) oc.anchor += padding.size(); }
                }
            }
            pt.insert(targetPos, text); batch.ops.push_back({ EditOp::Insert, targetPos, text });
            for (auto& oc : cursors) { if (oc.head >= targetPos) oc.head += text.size(); if (oc.anchor >= targetPos) oc.anchor += text.size(); }
            c.desiredX = getXInLine(li, c.head); c.originalAnchorX = c.desiredX; c.isVirtual = false;
        }
        batch.afterCursors = cursors; undo.push(batch); rebuildLineStarts(); ensureCaretVisible(); updateDirtyFlag();
    }
    void insertNewlineWithAutoIndent() {
        if (cursors.empty()) return;
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
                c.head = start; c.anchor = start;
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
                }
                else {
                    break;
                }
                p++;
            }
            std::string textToInsert = newlineStr + indentStr;
            pt.insert(start, textToInsert);
            batch.ops.push_back({ EditOp::Insert, start, textToInsert });
            size_t insLen = textToInsert.size();
            for (auto& o : cursors) {
                if (o.head >= start) o.head += insLen;
                if (o.anchor >= start) o.anchor += insLen;
                if (&o == &c) {
                    o.desiredX = getXFromPos(o.head);
                    o.originalAnchorX = o.desiredX;
                    o.isVirtual = false;
                }
            }
        }
        batch.afterCursors = cursors;
        undo.push(batch);
        rebuildLineStarts();
        ensureCaretVisible();
        updateDirtyFlag();
    }
    void insertRectangularBlock(const std::string& text) {
        if (cursors.empty()) return;
        std::vector<std::string> lines; std::stringstream ss(text); std::string line;
        while (std::getline(ss, line)) { if (!line.empty() && line.back() == '\r') line.pop_back(); lines.push_back(line); }
        if (lines.empty()) return;
        EditBatch batch; batch.beforeCursors = cursors;
        auto sortedCursors = cursors;
        std::sort(sortedCursors.begin(), sortedCursors.end(), [](const Cursor& a, const Cursor& b) { return a.start() < b.start(); });
        size_t basePos = sortedCursors[0].head; float baseX = sortedCursors[0].desiredX; int startLineIdx = getLineIdx(basePos);
        int requiredTotalLines = startLineIdx + (int)lines.size();
        if (requiredTotalLines > (int)lineStarts.size()) {
            size_t eofPos = pt.length(); std::string newLines = "";
            for (int k = 0; k < (requiredTotalLines - (int)lineStarts.size()); ++k) newLines += "\n";
            pt.insert(eofPos, newLines); batch.ops.push_back({ EditOp::Insert, eofPos, newLines }); rebuildLineStarts();
        }
        for (int i = (int)lines.size() - 1; i >= 0; --i) {
            int targetLineIdx = startLineIdx + i; std::string content = lines[i];
            size_t lineStart = lineStarts[targetLineIdx];
            size_t lineEnd = (targetLineIdx + 1 < (int)lineStarts.size()) ? lineStarts[targetLineIdx + 1] : pt.length();
            if (lineEnd > lineStart && pt.charAt(lineEnd - 1) == '\n') lineEnd--;
            if (lineEnd > lineStart && pt.charAt(lineEnd - 1) == '\r') lineEnd--;
            float currentLineEndX = getXInLine(targetLineIdx, lineEnd); size_t insertPos = lineEnd;
            if (baseX > currentLineEndX + 1.0f) {
                int spacesNeeded = (int)((baseX - currentLineEndX) / charWidth + 0.5f);
                if (spacesNeeded > 0) {
                    std::string padding(spacesNeeded, ' '); pt.insert(lineEnd, padding);
                    batch.ops.push_back({ EditOp::Insert, lineEnd, padding }); insertPos = lineEnd + padding.size(); rebuildLineStarts();
                }
            } else { insertPos = getPosFromLineAndX(targetLineIdx, baseX); }
            pt.insert(insertPos, content); batch.ops.push_back({ EditOp::Insert, insertPos, content }); rebuildLineStarts();
        }
        cursors.clear();
        for (int i = 0; i < (int)lines.size(); ++i) {
            int li = startLineIdx + i; float finalX = baseX + (float)UTF8ToW(lines[i]).length() * charWidth;
            size_t p = getPosFromLineAndX(li, finalX); cursors.push_back({ p, p, finalX, finalX, false });
        }
        batch.afterCursors = cursors; undo.push(batch); rebuildLineStarts(); ensureCaretVisible(); updateDirtyFlag();
    }
    std::vector<int> getUniqueLineIndices() {
        std::vector<int> lines;
        for (const auto& c : cursors) {
            if (!c.hasSelection()) { lines.push_back(getLineIdx(c.head)); }
            else {
                size_t start = c.start(), end = c.end(); int lStart = getLineIdx(start), lEnd = getLineIdx(end);
                if (lEnd > lStart) { size_t lineStartPos = lineStarts[lEnd]; if (end == lineStartPos) lEnd--; }
                for (int i = lStart; i <= lEnd; ++i) lines.push_back(i);
            }
        }
        std::sort(lines.begin(), lines.end()); lines.erase(std::unique(lines.begin(), lines.end()), lines.end()); return lines;
    }
    void deleteLine() {
        std::vector<int> lines = getUniqueLineIndices(); if (lines.empty()) return;
        EditBatch batch; batch.beforeCursors = cursors;
        for (int i = (int)lines.size() - 1; i >= 0; --i) {
            int li = lines[i]; size_t start = lineStarts[li];
            size_t end = (li + 1 < (int)lineStarts.size()) ? lineStarts[li + 1] : pt.length(); size_t len = end - start;
            if (len > 0) {
                batch.ops.push_back({EditOp::Erase, start, pt.getRange(start, len)}); pt.erase(start, len);
                for (auto& c : cursors) { if (c.head > start) c.head = (c.head >= start + len) ? c.head - len : start; if (c.anchor > start) c.anchor = (c.anchor >= start + len) ? c.anchor - len : start; }
            } else if (len == 0 && li > 0) {
                size_t delStart = lineStarts[li]; size_t delLen = 0;
                if(delStart > 0 && pt.charAt(delStart-1) == '\n') { delStart--; delLen++; }
                if(delStart > 0 && pt.charAt(delStart-1) == '\r') { delStart--; delLen++; }
                if (delLen > 0) {
                    batch.ops.push_back({EditOp::Erase, delStart, pt.getRange(delStart, delLen)}); pt.erase(delStart, delLen);
                    for (auto& c : cursors) { if (c.head > delStart) c.head -= delLen; if (c.anchor > delStart) c.anchor -= delLen; }
                }
            }
        }
        for (auto& c : cursors) { c.anchor = c.head; c.desiredX = getXFromPos(c.head); c.isVirtual=false; }
        batch.afterCursors = cursors; undo.push(batch); rebuildLineStarts(); ensureCaretVisible(); updateDirtyFlag();
    }
    void moveLines(bool up) {
        std::vector<int> lines = getUniqueLineIndices(); if (lines.empty()) return;
        if (up && lines.front() == 0) return; if (!up && lines.back() >= (int)lineStarts.size() - 1) return;
        EditBatch batch; batch.beforeCursors = cursors;
        std::vector<std::pair<int, int>> groups; int start = lines[0], prev = lines[0];
        for (size_t i = 1; i < lines.size(); ++i) { if (lines[i] != prev + 1) { groups.push_back({start, prev}); start = lines[i]; } prev = lines[i]; } groups.push_back({start, prev});
        if (up) {
            for (const auto& g : groups) {
                int gStart = g.first, gEnd = g.second, targetLine = gStart - 1;
                size_t posTarget = lineStarts[targetLine], posStart = lineStarts[gStart];
                size_t posEnd = (gEnd + 1 < (int)lineStarts.size()) ? lineStarts[gEnd + 1] : pt.length();
                bool isEOF = (posEnd == pt.length());
                std::string movingText = pt.getRange(posStart, posEnd - posStart);
                size_t moveLen = movingText.length();
                size_t prevLineLen = posStart - posTarget;
                bool movingIsLastNoNewline = (isEOF && (moveLen > 0 && movingText.back() != '\n'));
                pt.erase(posStart, moveLen);
                batch.ops.push_back({EditOp::Erase, posStart, movingText});
                if (movingIsLastNoNewline) {
                    if (posStart > 0 && pt.charAt(posStart - 1) == '\n') {
                        pt.erase(posStart - 1, 1);
                        batch.ops.push_back({EditOp::Erase, posStart - 1, "\n"});
                        movingText += "\n";
                        moveLen++;
                    }
                }
                pt.insert(posTarget, movingText);
                batch.ops.push_back({EditOp::Insert, posTarget, movingText});
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
                    c.desiredX = getXFromPos(c.head); c.isVirtual = false;
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
                bool movingHasNewline = (moveLen > 0 && movingText.back() == '\n');
                pt.erase(posStart, moveLen);
                batch.ops.push_back({EditOp::Erase, posStart, movingText});
                size_t insertPos = posStart + swapLineLen;
                if (isSwapEndEOF) {
                    bool precedingCharIsNewline = (insertPos > 0 && pt.charAt(insertPos - 1) == '\n');
                    if (insertPos > 0 && !precedingCharIsNewline) {
                        if (movingHasNewline) {
                            pt.insert(insertPos, "\n");
                            batch.ops.push_back({EditOp::Insert, insertPos, "\n"});
                            insertPos++; swapLineLen++;
                            movingText.pop_back(); moveLen--;
                        }
                    } else if (swapLineLen == 0) {
                        if (movingHasNewline) {
                            pt.insert(insertPos, "\n");
                            batch.ops.push_back({EditOp::Insert, insertPos, "\n"});
                            insertPos++; swapLineLen++;
                            movingText.pop_back(); moveLen--;
                        }
                    }
                }
                pt.insert(insertPos, movingText);
                batch.ops.push_back({EditOp::Insert, insertPos, movingText});
                for (auto& c : cursors) {
                    bool headInA = (c.head >= posStart && c.head < posEnd);
                    if (c.head == posEnd && c.hasSelection() && c.end() == c.head) headInA = true;
                    if (isPosEndEOF && c.head == posEnd) headInA = true;
                    bool anchorInA = (c.anchor >= posStart && c.anchor < posEnd);
                    if (c.anchor == posEnd && c.hasSelection() && c.end() == c.anchor) anchorInA = true;
                    if (isPosEndEOF && c.anchor == posEnd) anchorInA = true;
                    if (headInA) { c.head += swapLineLen; } else if (c.head >= posEnd && c.head < posSwapEnd) { c.head -= moveLen; }
                    if (anchorInA) { c.anchor += swapLineLen; } else if (c.anchor >= posEnd && c.anchor < posSwapEnd) { c.anchor -= moveLen; }
                    c.desiredX = getXFromPos(c.head); c.isVirtual = false;
                }
                rebuildLineStarts();
            }
        }
        batch.afterCursors = cursors; undo.push(batch); ensureCaretVisible(); updateDirtyFlag();
    }
    void copyLines(bool up) {
        std::vector<int> lines = getUniqueLineIndices(); if (lines.empty()) return;
        size_t len = pt.length(); if (len > 0 && pt.charAt(len-1) != '\n') { pt.insert(len, "\n"); rebuildLineStarts(); }
        EditBatch batch; batch.beforeCursors = cursors;
        std::vector<std::pair<int, int>> groups; int start = lines[0], prev = lines[0];
        for (size_t i = 1; i < lines.size(); ++i) { if (lines[i] != prev + 1) { groups.push_back({start, prev}); start = lines[i]; } prev = lines[i]; } groups.push_back({start, prev});
        if (up) {
            for (int i = (int)groups.size() - 1; i >= 0; --i) {
                int gStart = groups[i].first, gEnd = groups[i].second;
                size_t posStart = lineStarts[gStart], posEnd = (gEnd + 1 < (int)lineStarts.size()) ? lineStarts[gEnd + 1] : pt.length();
                std::string text = pt.getRange(posStart, posEnd - posStart);
                pt.insert(posStart, text); batch.ops.push_back({EditOp::Insert, posStart, text}); size_t insLen = text.length();
                for (auto& c : cursors) {
                    if (c.head >= posStart) c.head += insLen; if (c.anchor >= posStart) c.anchor += insLen;
                    if (c.head >= posStart + insLen && c.head < posEnd + insLen) c.head -= insLen;
                    if (c.anchor >= posStart + insLen && c.anchor < posEnd + insLen) c.anchor -= insLen;
                    c.desiredX = getXFromPos(c.head); c.isVirtual = false;
                }
                rebuildLineStarts();
            }
        } else {
            for (int i = (int)groups.size() - 1; i >= 0; --i) {
                int gStart = groups[i].first, gEnd = groups[i].second;
                size_t posStart = lineStarts[gStart], posEnd = (gEnd + 1 < (int)lineStarts.size()) ? lineStarts[gEnd + 1] : pt.length();
                std::string text = pt.getRange(posStart, posEnd - posStart);
                pt.insert(posEnd, text); batch.ops.push_back({EditOp::Insert, posEnd, text}); size_t insLen = text.length();
                for (auto& c : cursors) {
                    if (c.head >= posEnd) c.head += insLen; if (c.anchor >= posEnd) c.anchor += insLen;
                    if (c.head >= posStart && c.head < posEnd) c.head += insLen;
                    if (c.anchor >= posStart && c.anchor < posEnd) c.anchor += insLen;
                    c.desiredX = getXFromPos(c.head); c.isVirtual = false;
                }
                rebuildLineStarts();
            }
        }
        batch.afterCursors = cursors; undo.push(batch); ensureCaretVisible(); updateDirtyFlag();
    }
    void indentLines(bool forceLineIndent = false) {
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
                c.desiredX = getXFromPos(c.head);
                c.originalAnchorX = getXFromPos(c.anchor);
                c.isVirtual = false;
            }
        }
        batch.afterCursors = cursors;
        undo.push(batch);
        rebuildLineStarts();
        ensureCaretVisible();
        updateDirtyFlag();
    }
    void unindentLines() {
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
            // Windows版と同じくタブ1文字またはスペース1文字を削除
            if (c == '\t') eraseLen = 1;
            else if (c == ' ') eraseLen = 1;
            
            if (eraseLen > 0) {
                std::string deleted = pt.getRange(pos, eraseLen);
                pt.erase(pos, eraseLen);
                batch.ops.push_back({ EditOp::Erase, pos, deleted });
                for (auto& cur : cursors) {
                    if (cur.head > pos) cur.head -= std::min(cur.head - pos, eraseLen);
                    if (cur.anchor > pos) cur.anchor -= std::min(cur.anchor - pos, eraseLen);
                    cur.desiredX = getXFromPos(cur.head);
                    cur.originalAnchorX = cur.desiredX;
                    cur.isVirtual = false;
                }
            }
        }
        if (!batch.ops.empty()) {
            batch.afterCursors = cursors;
            undo.push(batch);
            rebuildLineStarts();
            ensureCaretVisible();
            updateDirtyFlag();
        }
    }
    void updateGutterWidth() {
        int totalLines = (int)lineStarts.size(), digits = 1; int tempLines = totalLines;
        while (tempLines >= 10) { tempLines /= 10; digits++; }
        gutterWidth = (float)(digits * charWidth) + (charWidth * 1.5f);
    }
    void updateMaxLineWidth() {
        if (!fontRef) return; maxLineWidth = 0.0f;
        for (int i = 0; i < (int)lineStarts.size(); ++i) {
            float w = getXInLine(i, (i + 1 < (int)lineStarts.size() ? lineStarts[i + 1] : pt.length()));
            if (w > maxLineWidth) maxLineWidth = w;
        }
        maxLineWidth += charWidth * 2.0f;
    }
    std::pair<std::string, bool> getHighlightTarget() {
        if (cursors.empty() || cursors.size() > 1) return { "", false };
        const Cursor& c = cursors.back();
        if (c.hasSelection()) {
            size_t len = c.end() - c.start(); if (len == 0 || len > 200) return { "", false };
            std::string s = pt.getRange(c.start(), len); if (s.empty() || s.find('\n') != std::string::npos) return { "", false };
            return { s, false };
        }
        size_t pos = c.head, len = pt.length(); if (pos > len) pos = len;
        bool charRight = (pos < len && isWordChar(pt.charAt(pos))), charLeft = (pos > 0 && isWordChar(pt.charAt(pos - 1)));
        if (!charRight && !charLeft) return { "", true };
        size_t start = pos, end = pos;
        while (start > 0 && isWordChar(pt.charAt(start - 1))) start--; while (end < len && isWordChar(pt.charAt(end))) end++;
        if (end > start) return { pt.getRange(start, end - start), true };
        return { "", true };
    }
    size_t findText(size_t startPos, const std::string& query, bool forward) {
        if (query.empty()) return std::string::npos;
        size_t len = pt.length();
        if (len == 0) return std::string::npos;
        size_t cur = startPos;
        if (forward) { if (cur >= len) cur = 0; }
        else { if (cur == 0) cur = len - 1; else cur--; }
        for (size_t count = 0; count < len; ++count) {
            bool match = true;
            for (size_t i = 0; i < query.length(); ++i) {
                if (cur + i >= len || pt.charAt(cur + i) != query[i]) { match = false; break; }
            }
            if (match) return cur;
            if (forward) { cur++; if (cur >= len) cur = 0; }
            else { if (cur == 0) cur = len - 1; else cur--; }
        }
        return std::string::npos;
    }
    void selectNextOccurrence() {
        if (cursors.empty()) return;
        Cursor c = cursors.back();
        if (!c.hasSelection()) {
            size_t targetPos = c.head;
            if (targetPos > 0) {
                char currChar = targetPos < pt.length() ? pt.charAt(targetPos) : '\0';
                char prevChar = pt.charAt(targetPos - 1);
                if (!isWordChar(currChar) && isWordChar(prevChar)) {
                    targetPos--;
                }
            }
            size_t s, e;
            getWordBoundaries(targetPos, s, e);
            if (s != e) {
                cursors.clear();
                cursors.push_back({e, s, getXFromPos(e), getXFromPos(s), false});
            }
            return;
        }
        size_t start = c.start();
        size_t len = c.end() - start;
        std::string query = pt.getRange(start, len);
        size_t nextPos = findText(std::max(c.head, c.anchor), query, true);
        if (nextPos != std::string::npos) {
            for (const auto& cur : cursors) {
                if (cur.start() == nextPos) return;
            }
            size_t newHead = nextPos + len;
            cursors.push_back({newHead, nextPos, getXFromPos(newHead), getXFromPos(nextPos), false});
            ensureCaretVisible();
        }
    }
    void updateTitleBar() {
        std::wstring t = (isDirty ? L"*" : L"") + (currentFilePath.empty() ? L"Untitled" : currentFilePath.substr(currentFilePath.find_last_of(L"/")+1)) + L" - " + APP_TITLE;
        if (view) [[(NSView*)view window] setTitle:[NSString stringWithUTF8String:WToUTF8(t).c_str()]];
        if (view) [[(NSView*)view window] setDocumentEdited:isDirty];
    }
    void updateScrollBars() { if (view) [(EditorView*)view updateScrollers]; }
    void updateDirtyFlag() { bool nd = undo.isModified(); if (isDirty != nd) { isDirty = nd; updateTitleBar(); } }
    void updateFont(float s) {
        float oldCharWidth = charWidth; s = std::clamp(s, 6.0f, 200.0f);
        if (fontRef) CFRelease(fontRef); fontRef = CTFontCreateWithName(CFSTR("Menlo"), s, NULL);
        currentFontSize = s; lineHeight = std::ceil(s * 1.4f);
        UniChar c = '0'; CGGlyph g; CGSize adv; CTFontGetGlyphsForCharacters(fontRef, &c, &g, 1); CTFontGetAdvancesForGlyphs(fontRef, kCTFontOrientationHorizontal, &g, &adv, 1);
        charWidth = adv.width;
        if (oldCharWidth > 0.0f && charWidth > 0.0f) { float ratio = charWidth / oldCharWidth; for (auto& cur : cursors) { cur.desiredX *= ratio; cur.originalAnchorX *= ratio; } }
        updateGutterWidth(); updateMaxLineWidth(); if (view) updateScrollBars();
    }
    void rebuildLineStarts() {
        lineStarts.clear(); lineStarts.push_back(0); size_t go = 0;
        for (const auto& p : pt.pieces) {
            const char* b = p.isOriginal ? (pt.origPtr + p.start) : (pt.addBuf.data() + p.start);
            for (size_t i = 0; i < p.len; ++i) if (b[i] == '\n') lineStarts.push_back(go + i + 1);
            go += p.len;
        }
        updateGutterWidth(); if (fontRef) updateMaxLineWidth(); if (view) [(EditorView*)view updateScrollers];
    }
    int getLineIdx(size_t pos) { auto it = std::upper_bound(lineStarts.begin(), lineStarts.end(), pos); return std::max(0, (int)std::distance(lineStarts.begin(), it) - 1); }
    float getXInLine(int li, size_t pos) {
        if (li < 0 || li >= (int)lineStarts.size()) return 0.0f;
        size_t s = lineStarts[li], e = (li + 1 < (int)lineStarts.size()) ? lineStarts[li + 1] : pt.length();
        if (e > s && pt.charAt(e-1) == '\n') e--;
        if (e > s && pt.charAt(e-1) == '\r') e--;
        std::string lstr = pt.getRange(s, e - s); size_t rp = std::clamp(pos, s, e) - s;
        if (!imeComp.empty() && !cursors.empty() && getLineIdx(cursors.back().head) == li) { size_t cp = cursors.back().head; if (cp >= s && cp <= e) { lstr.insert(cp - s, imeComp); if (pos >= cp) rp += imeComp.size(); } }
        if (lstr.empty()) return 0.0f;
        CFStringRef cf = CFStringCreateWithBytes(NULL, (const UInt8*)lstr.data(), lstr.size(), kCFStringEncodingUTF8, false); if (!cf) return 0.0f;
        const void* k[] = { kCTFontAttributeName }; const void* v[] = { fontRef }; CFDictionaryRef d = CFDictionaryCreate(NULL, k, v, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFAttributedStringRef as = CFAttributedStringCreate(NULL, cf, d); CTLineRef line = CTLineCreateWithAttributedString(as);
        CFStringRef sub = CFStringCreateWithBytes(NULL, (const UInt8*)lstr.data(), rp, kCFStringEncodingUTF8, false);
        CGFloat x = CTLineGetOffsetForStringIndex(line, sub ? CFStringGetLength(sub) : 0, NULL);
        if (sub) CFRelease(sub); CFRelease(line); CFRelease(as); CFRelease(d); CFRelease(cf); return (float)x;
    }
    float getXFromPos(size_t p) { return getXInLine(getLineIdx(p), p); }
    size_t getPosFromLineAndX(int li, float tx) {
        if (li < 0 || li >= (int)lineStarts.size()) return cursors.empty() ? 0 : cursors.back().head;
        size_t s = lineStarts[li], e = (li + 1 < (int)lineStarts.size()) ? lineStarts[li + 1] : pt.length();
        if (e > s && pt.charAt(e-1) == '\n') e--;
        if (e > s && pt.charAt(e-1) == '\r') e--;
        std::string lstr = pt.getRange(s, e - s); if (lstr.empty()) return s;
        CFStringRef cf = CFStringCreateWithBytes(NULL, (const UInt8*)lstr.data(), lstr.size(), kCFStringEncodingUTF8, false); if (!cf) return s;
        const void* k[]={kCTFontAttributeName}; const void* v[]={fontRef}; CFDictionaryRef d = CFDictionaryCreate(NULL, k, v, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFAttributedStringRef as = CFAttributedStringCreate(NULL, cf, d); CTLineRef line = CTLineCreateWithAttributedString(as);
        CFIndex ci = CTLineGetStringIndexForPosition(line, CGPointMake(tx, 0));
        CFIndex bytes = 0; if (ci > 0) {
            CFStringRef sub = CFStringCreateWithSubstring(NULL, cf, CFRangeMake(0, std::min(ci, CFStringGetLength(cf))));
            if (sub) { CFStringGetBytes(sub, CFRangeMake(0, CFStringGetLength(sub)), kCFStringEncodingUTF8, 0, false, NULL, 0, &bytes); CFRelease(sub); }
        }
        CFRelease(line); CFRelease(as); CFRelease(d); CFRelease(cf); return s + bytes;
    }
    size_t getDocPosFromPoint(float x, float y) {
        float absY = y + (float)vScrollPos * lineHeight;
        int li = std::clamp((int)std::floor(absY / lineHeight), 0, (int)lineStarts.size() - 1);
        return getPosFromLineAndX(li, x - gutterWidth + (float)hScrollPos);
    }
    void ensureCaretVisible() {
        if (cursors.empty() || !view) return; Cursor& c = cursors.back(); NSRect b = [(NSView*)view bounds];
        float ch = b.size.height - visibleHScrollHeight, cw = b.size.width - gutterWidth - visibleVScrollWidth;
        int li = getLineIdx(c.head), vis = (int)(ch/lineHeight);
        if (li < vScrollPos) vScrollPos = li; else if (li >= vScrollPos + vis) vScrollPos = li - vis + 1;
        float cx = getXFromPos(c.head), m = charWidth*2; if (cx < (float)hScrollPos + m) hScrollPos = (int)(cx - m); else if (cx > (float)hScrollPos + cw - m) hScrollPos = (int)(cx - cw + m);
        vScrollPos = std::max(0, vScrollPos); hScrollPos = std::max(0, hScrollPos); updateScrollBars();
    }
    size_t moveCaretVisual(size_t pos, bool f) {
        size_t len = pt.length();
        if (f) { if (pos >= len) return len; unsigned char c = pt.charAt(pos); int sl = 1; if ((c&0x80)==0) sl=1; else if((c&0xE0)==0xC0) sl=2; else if((c&0xF0)==0xE0) sl=3; else if((c&0xF8)==0xF0) sl=4; if(c=='\r'&&pos+1<len&&pt.charAt(pos+1)=='\n') sl=2; return std::min(len, pos+sl); }
        else { if (pos == 0) return 0; size_t p = pos-1; while(p>0 && (pt.charAt(p)&0xC0)==0x80) p--; if(p>0 && pt.charAt(p-1)=='\r'&&pt.charAt(p)=='\n') p--; return p; }
    }
    void insertAtCursors(const std::string& t) {
        EditBatch b; b.beforeCursors=cursors; auto sorted = cursors;
        std::sort(sorted.begin(), sorted.end(), [](const Cursor& a, const Cursor& b){return a.start()>b.start();});
        for(auto& c:sorted){
            size_t st=c.start(), l=c.end()-st;
            if(l>0){
                b.ops.push_back({EditOp::Erase, st, pt.getRange(st,l)});
                pt.erase(st,l);
                for(auto& o : cursors){
                    if(o.head>st)o.head-=l;
                    if(o.anchor>st)o.anchor-=l;
                }
            }
            pt.insert(st,t);
            b.ops.push_back({EditOp::Insert, st, t});
            for(auto& o : cursors){
                if(o.head>=st)o.head+=(size_t)t.size();
                if(o.anchor>=st)o.anchor+=(size_t)t.size();
            }
        }
        for (auto& c : cursors) { c.desiredX = getXFromPos(c.head); c.originalAnchorX = c.desiredX; c.isVirtual = false; }
        b.afterCursors=cursors; undo.push(b); rebuildLineStarts(); ensureCaretVisible(); updateDirtyFlag();
    }
    void backspaceAtCursors() {
        if (cursors.empty()) return; EditBatch b; b.beforeCursors = cursors;
        std::vector<size_t> indices(cursors.size()); std::iota(indices.begin(), indices.end(), 0);
        std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) { return cursors[a].head > cursors[b].head; });
        bool changed = false;
        for (size_t idx : indices) {
            Cursor& c = cursors[idx];
            if (c.hasSelection()) {
                size_t st = c.start(), len = c.end() - st;
                b.ops.push_back({EditOp::Erase, st, pt.getRange(st, len)}); pt.erase(st, len); changed = true;
                for (auto& o : cursors) { if (&o == &c) continue; if (o.head > st) { if (o.head >= st + len) o.head -= len; else o.head = st; } if (o.anchor > st) { if (o.anchor >= st + len) o.anchor -= len; else o.anchor = st; } }
                c.head = st; c.anchor = st; c.desiredX = getXFromPos(c.head); c.originalAnchorX = c.desiredX; c.isVirtual = false; continue;
            }
            int li = getLineIdx(c.head); float physX = getXInLine(li, c.head);
            if (c.isVirtual && c.desiredX > physX + 0.1f) { c.desiredX = std::max(physX, c.desiredX - charWidth); c.originalAnchorX = c.desiredX; if (c.desiredX <= physX + 0.1f) { c.desiredX = physX; c.isVirtual = false; } continue; }
            if (c.head > 0) {
                size_t st = c.head, prev = moveCaretVisual(st, false), len = st - prev;
                if (len > 0) {
                    b.ops.push_back({EditOp::Erase, prev, pt.getRange(prev, len)}); pt.erase(prev, len); changed = true;
                    for (auto& o : cursors) { if (&o == &c) continue; if (o.head >= st) o.head -= len; else if (o.head > prev) o.head = prev; if (o.anchor >= st) o.anchor -= len; else if (o.anchor > prev) o.anchor = prev; }
                    c.head = prev; c.anchor = prev; c.desiredX = getXFromPos(c.head); c.originalAnchorX = c.desiredX; c.isVirtual = false;
                }
            }
        }
        if (changed) { b.afterCursors = cursors; undo.push(b); rebuildLineStarts(); updateDirtyFlag(); } ensureCaretVisible(); if (view) [(NSView*)view setNeedsDisplay:YES];
    }
    void deleteForwardAtCursors() {
        if (cursors.empty()) return; EditBatch b; b.beforeCursors = cursors;
        std::vector<size_t> indices(cursors.size()); std::iota(indices.begin(), indices.end(), 0);
        std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) { return cursors[a].start() > cursors[b].start(); });
        bool changed = false;
        for (size_t idx : indices) {
            Cursor& c = cursors[idx]; size_t st = c.start(), len = 0;
            if (c.hasSelection()) len = c.end() - st; else { size_t next = moveCaretVisual(st, true); len = next - st; }
            if (len > 0) {
                b.ops.push_back({EditOp::Erase, st, pt.getRange(st, len)}); pt.erase(st, len); changed = true;
                for (auto& o : cursors) { if (o.head > st + len) o.head -= len; else if (o.head > st) o.head = st; if (o.anchor > st + len) o.anchor -= len; else if (o.anchor > st) o.anchor = st; }
                c.head = st; c.anchor = st;
            }
            c.desiredX = getXFromPos(c.head); c.originalAnchorX = c.desiredX; c.isVirtual = false;
        }
        if (changed) { b.afterCursors = cursors; undo.push(b); rebuildLineStarts(); ensureCaretVisible(); updateDirtyFlag(); }
    }
    void selectAll() { cursors.clear(); size_t len = pt.length(); cursors.push_back({len, 0, getXFromPos(len), getXFromPos(len), false}); }
    void convertSelectedText(bool toUpper) {
        if (cursors.empty()) return;
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
                batch.ops.push_back({EditOp::Erase, start, text});
                pt.erase(start, len);
                for(auto& o : cursors) {
                    if(o.head > start) o.head -= len;
                    if(o.anchor > start) o.anchor -= len;
                }
                pt.insert(start, newText);
                batch.ops.push_back({EditOp::Insert, start, newText});
                size_t newLen = newText.length();
                for(auto& o : cursors) {
                    if(o.head >= start) o.head += newLen;
                    if(o.anchor >= start) o.anchor += newLen;
                }
                if (isHeadStart) {
                    c.head = start;
                    c.anchor = start + newLen;
                } else {
                    c.anchor = start;
                    c.head = start + newLen;
                }
                c.desiredX = getXFromPos(c.head);
                c.originalAnchorX = getXFromPos(c.anchor);
                c.isVirtual = false;
                changed = true;
            }
        }
        if (changed) {
            batch.afterCursors = cursors;
            undo.push(batch);
            rebuildLineStarts();
            updateDirtyFlag();
            if (view) [(NSView*)view setNeedsDisplay:YES];
        }
    }
    void copyToClipboard() {
        bool hasSelection = false; for (const auto& c : cursors) if (c.hasSelection()) { hasSelection = true; break; }
        std::string t; bool isRect = (cursors.size() > 1);
        if (!hasSelection) {
            std::vector<int> lines = getUniqueLineIndices();
            for (int li : lines) {
                size_t start = lineStarts[li], end = (li + 1 < (int)lineStarts.size()) ? lineStarts[li + 1] : pt.length();
                t += pt.getRange(start, end - start); if (t.empty() || t.back() != '\n') t += "\n";
            }
        } else {
            auto sorted = cursors; std::sort(sorted.begin(), sorted.end(), [](const Cursor& a, const Cursor& b){ return a.start() < b.start(); });
            for(size_t i=0; i<sorted.size(); ++i) { if(sorted[i].hasSelection()) { t += pt.getRange(sorted[i].start(), sorted[i].end() - sorted[i].start()); if(isRect && i < sorted.size() - 1) t += "\n"; } }
        }
        if(!t.empty()) {
            NSPasteboard *pb = [NSPasteboard generalPasteboard]; [pb clearContents]; [pb setString:[NSString stringWithUTF8String:t.c_str()] forType:NSPasteboardTypeString];
            if(hasSelection && isRect) [pb setData:[NSData data] forType:kMiuRectangularSelectionType];
        }
    }
    void cutToClipboard() {
        bool hasSelection = false; for (const auto& c : cursors) if (c.hasSelection()) { hasSelection = true; break; }
        copyToClipboard();
        if (!hasSelection) deleteLine(); else insertAtCursors("");
    }
    void pasteFromClipboard() {
        NSPasteboard *pb = [NSPasteboard generalPasteboard]; NSString *s = [pb stringForType:NSPasteboardTypeString]; if(!s) return;
        std::string utf8 = [s UTF8String]; bool isRectMarker = [pb dataForType:kMiuRectangularSelectionType] != nil;
        if (isRectMarker) {
            if (cursors.size() <= 1) {
                size_t basePos = cursors.empty() ? 0 : cursors[0].head; float baseX = getXFromPos(basePos); int startLine = getLineIdx(basePos);
                int lineCount = 1; for(char c : utf8) if(c == '\n') lineCount++;
                cursors.clear(); for(int i=0; i < lineCount; ++i) { int targetLine = startLine + i; if(targetLine < (int)lineStarts.size()) { size_t p = getPosFromLineAndX(targetLine, baseX); cursors.push_back({p, p, baseX, baseX, false}); } }
            }
            insertRectangularBlock(utf8);
        } else { insertAtCursors(utf8); }
        if (view) [(NSView*)view setNeedsDisplay:YES];
    }
    void performUndo() { if(!undo.undoStack.empty()){ auto b = undo.popUndo(); for(int i=(int)b.ops.size()-1;i>=0;--i){ if(b.ops[i].type==EditOp::Insert) pt.erase(b.ops[i].pos, (int)b.ops[i].text.size()); else pt.insert(b.ops[i].pos, b.ops[i].text); } cursors=b.beforeCursors; rebuildLineStarts(); ensureCaretVisible(); updateDirtyFlag(); } }
    void performRedo() { if(!undo.redoStack.empty()){ auto b = undo.popRedo(); for(const auto& o:b.ops){ if(o.type==EditOp::Insert) pt.insert(o.pos, o.text); else pt.erase(o.pos, (int)o.text.size()); } cursors=b.afterCursors; rebuildLineStarts(); ensureCaretVisible(); updateDirtyFlag(); } }
    
    bool checkUnsavedChanges() {
        if(!isDirty) return true;
        NSAlert *a = [NSAlert new]; [a setMessageText:@"Save changes?"]; [a addButtonWithTitle:@"Save"]; [a addButtonWithTitle:@"Cancel"]; [a addButtonWithTitle:@"Discard"];
        NSModalResponse r = [a runModal];
        if(r==NSAlertFirstButtonReturn) return currentFilePath.empty()?saveFileAs():saveFile(currentFilePath);
        if(r==NSAlertThirdButtonReturn){ isDirty=false; updateTitleBar(); return true; }
        return false;
    }
    bool saveFile(const std::wstring& p) { std::string s = pt.getRange(0,pt.length()); std::ofstream f(WToUTF8(p), std::ios::binary); if(!f) return false; f.write(s.data(), s.size()); f.close(); currentFilePath=p; undo.markSaved(); isDirty=false; updateTitleBar(); return true; }
    bool saveFileAs() { NSSavePanel *p = [NSSavePanel savePanel]; if([p runModal]==NSModalResponseOK) return saveFile(UTF8ToW([p.URL.path UTF8String])); return false; }
    bool openFileFromPath(const std::string& p) {
        fileMap.reset(new MappedFile());
        if(fileMap->open(p.c_str())){
            currentEncoding=DetectEncoding(fileMap->ptr,fileMap->size); const char* ptr=fileMap->ptr; size_t sz=fileMap->size; std::string conv = "";
            if(currentEncoding==ENC_UTF16LE) conv=Utf16ToUtf8(ptr,sz,false); else if(currentEncoding == ENC_UTF16BE) conv=Utf16ToUtf8(ptr,sz,true); else if(currentEncoding == ENC_ANSI) conv=AnsiToUtf8(ptr,sz); else if(currentEncoding == ENC_UTF8_BOM){ptr+=3;sz-=3;}
            if(!conv.empty()) {
                pt.initFromFile(conv.data(),conv.size());
                detectNewlineStyle(conv.data(), conv.size());
            } else {
                pt.initFromFile(ptr, sz);
                detectNewlineStyle(ptr, sz);
            }
            currentFilePath = UTF8ToW(p); undo.clear(); isDirty = false; vScrollPos = 0; hScrollPos = 0;
            cursors.clear(); cursors.push_back({0,0,0.0f,0.0f,false}); rebuildLineStarts(); updateTitleBar(); if (view) { [(EditorView*)view updateScrollers]; [(NSView*)view setNeedsDisplay:YES]; }
            return true;
        } return false;
    }
    bool openFile() { if(!checkUnsavedChanges()) return false; NSOpenPanel *p = [NSOpenPanel openPanel]; if([p runModal]==NSModalResponseOK) return openFileFromPath([[[p URLs] objectAtIndex:0].path UTF8String]); return false; }
    void newFile() {
        if(checkUnsavedChanges()){
            pt.initEmpty();
            currentFilePath.clear();
            newlineStr = "\n";
            undo.clear();
            isDirty=false;
            cursors.clear();
            cursors.push_back({0,0,0.0f,0.0f,false});
            rebuildLineStarts();
            updateTitleBar();
        }
    }
    bool isWordChar(char c) { if (isalnum((unsigned char)c) || c == '_') return true; if ((unsigned char)c >= 0x80) return true; return false; }
    void getWordBoundaries(size_t pos, size_t& start, size_t& end) {
            size_t len = pt.length();
            if (len == 0 || pos >= len) { start = end = pos; return; }
            
            char c = pt.charAt(pos);
            if (c == '\r') {
                if (pos + 1 < len && pt.charAt(pos + 1) == '\n') {
                    start = pos; end = pos + 2; return;
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
    
    void initGraphics() { if (!fontRef) fontRef = CTFontCreateWithName(CFSTR("Menlo"), currentFontSize, NULL); rebuildLineStarts(); updateThemeColors(); if (cursors.empty()) cursors.push_back({0, 0, 0.0f, 0.0f, false}); updateTitleBar(); }
    void updateThemeColors() {
        if (colBackground) { CGColorRelease(colBackground); CGColorRelease(colText); CGColorRelease(colGutterBg); CGColorRelease(colGutterText); CGColorRelease(colSel); CGColorRelease(colCaret); }
        if (isDarkMode) {
            colBackground = CGColorCreateGenericRGB(0.12, 0.12, 0.12, 1.0); colText = CGColorCreateGenericRGB(0.9, 0.9, 0.9, 1.0);
            colGutterBg = CGColorCreateGenericRGB(0.18, 0.18, 0.18, 1.0); colGutterText = CGColorCreateGenericRGB(0.5, 0.5, 0.5, 1.0);
            colSel = CGColorCreateGenericRGB(0.26, 0.40, 0.60, 1.0); colCaret = CGColorCreateGenericRGB(1.0, 1.0, 1.0, 1.0);
        } else {
            colBackground = CGColorCreateGenericRGB(1.0, 1.0, 1.0, 1.0); colText = CGColorCreateGenericRGB(0.0, 0.0, 0.0, 1.0);
            colGutterBg = CGColorCreateGenericRGB(0.95, 0.95, 0.95, 1.0); colGutterText = CGColorCreateGenericRGB(0.6, 0.6, 0.6, 1.0);
            colSel = CGColorCreateGenericRGB(0.70, 0.80, 1.0, 1.0); colCaret = CGColorCreateGenericRGB(0.0, 0.0, 0.0, 1.0);
        }
    }
    void render(CGContextRef ctx, float w, float h) {
        CGContextSetFillColorWithColor(ctx, colBackground); CGContextFillRect(ctx, CGRectMake(0, 0, w, h));
        CGContextSetTextMatrix(ctx, CGAffineTransformMakeScale(1.0, -1.0));
        float vw = std::max(0.0f, w - gutterWidth - visibleVScrollWidth);
        float vh = std::max(0.0f, h - visibleHScrollHeight);
        int start = vScrollPos; int vis = (int)(vh / lineHeight) + 2; int end = std::min((int)lineStarts.size(), start + vis);
        CGFloat asc = CTFontGetAscent(fontRef);
        CGContextSaveGState(ctx); CGContextClipToRect(ctx, CGRectMake(gutterWidth, 0, vw, vh));
        auto [autoStr, isWholeWord] = getHighlightTarget();
        if (!autoStr.empty()) {
            CGColorRef autoHlColor = isDarkMode ? CGColorCreateGenericRGB(0.35, 0.35, 0.35, 0.5) : CGColorCreateGenericRGB(0.85, 0.85, 0.85, 0.5);
            CGContextSetFillColorWithColor(ctx, autoHlColor);
            size_t searchRangeStart = lineStarts[start]; size_t searchRangeEnd = (end < lineStarts.size()) ? lineStarts[end] : pt.length();
            std::string visibleText = pt.getRange(searchRangeStart, searchRangeEnd - searchRangeStart);
            size_t searchPos = 0;
            while ((searchPos = visibleText.find(autoStr, searchPos)) != std::string::npos) {
                size_t docPos = searchRangeStart + searchPos; bool shouldHighlight = true;
                if (isWholeWord) {
                    if (docPos > 0 && isWordChar(pt.charAt(docPos - 1))) shouldHighlight = false;
                    if (shouldHighlight && (docPos + autoStr.length() < pt.length()) && isWordChar(pt.charAt(docPos + autoStr.length()))) shouldHighlight = false;
                }
                if (shouldHighlight) {
                    int li = getLineIdx(docPos);
                    if (li >= start && li < end) {
                        float y = (float)(li - start) * lineHeight;
                        float x1 = getXInLine(li, docPos); float x2 = getXInLine(li, docPos + autoStr.length());
                        CGContextFillRect(ctx, CGRectMake(gutterWidth - (float)hScrollPos + x1, y, x2 - x1, lineHeight));
                    }
                }
                searchPos += 1;
            }
            CGColorRelease(autoHlColor);
        }
        CGContextSetFillColorWithColor(ctx, colSel); bool isRectMode = (cursors.size() > 1);
        for (const auto& c : cursors) {
            if (isRectMode) {
                float visualX1 = std::min(c.desiredX, c.originalAnchorX); float visualX2 = std::max(c.desiredX, c.originalAnchorX);
                if (visualX2 - visualX1 < 0.5f) continue;
                int lHead = getLineIdx(c.head), lAnchor = getLineIdx(c.anchor);
                int startL = std::min(lHead, lAnchor), endL = std::max(lHead, lAnchor);
                for (int l = startL; l <= endL; ++l) {
                    if (l < start || l >= end) continue;
                    float y = (float)(l - start) * lineHeight; float drawX = gutterWidth - (float)hScrollPos + visualX1; float drawW = visualX2 - visualX1;
                    CGContextFillRect(ctx, CGRectMake(drawX, y, drawW, lineHeight));
                }
            } else {
                if (!c.hasSelection()) continue;
                size_t pStart = c.start(), pEnd = c.end(); int lStart = getLineIdx(pStart), lEnd = getLineIdx(pEnd);
                for (int l = lStart; l <= lEnd; ++l) {
                    if (l < start || l >= end) continue;
                    float y = (float)(l - start) * lineHeight;
                    size_t lineBegin = lineStarts[l], lineEnd = (l + 1 < (int)lineStarts.size() ? lineStarts[l + 1] : pt.length());
                    size_t selS = std::max(pStart, lineBegin), selE = std::min(pEnd, lineEnd);
                    float x1 = getXInLine(l, selS), x2 = getXInLine(l, selE);
                    if (selE == lineEnd && pEnd >= lineEnd) x2 += charWidth;
                    if (x2 > x1) CGContextFillRect(ctx, CGRectMake(gutterWidth - (float)hScrollPos + x1, y, x2 - x1, lineHeight));
                }
            }
        }
        for (int i = start; i < end; ++i) {
            size_t s = lineStarts[i], e = (i + 1 < lineStarts.size() ? lineStarts[i + 1] : pt.length());
            std::string ls = pt.getRange(s, std::max((size_t)0, e - s)); size_t imIdx = std::string::npos;
            if (!imeComp.empty() && !cursors.empty() && getLineIdx(cursors.back().head) == i) { size_t cp = cursors.back().head; if (cp >= s && cp <= e) { imIdx = cp - s; ls.insert(imIdx, imeComp); } }
            if (!ls.empty()) {
                CFStringRef cf = CFStringCreateWithBytes(NULL, (const UInt8*)ls.data(), ls.size(), kCFStringEncodingUTF8, false);
                if (cf) {
                    CFMutableAttributedStringRef mas = CFAttributedStringCreateMutable(NULL, 0);
                    CFAttributedStringReplaceString(mas, CFRangeMake(0, 0), cf);
                    CFAttributedStringSetAttribute(mas, CFRangeMake(0, CFStringGetLength(cf)), kCTFontAttributeName, fontRef);
                    CFAttributedStringSetAttribute(mas, CFRangeMake(0, CFStringGetLength(cf)), kCTForegroundColorAttributeName, colText);
                    if (imIdx != std::string::npos) {
                        CFStringRef prefix = CFStringCreateWithBytes(NULL, (const UInt8*)ls.data(), imIdx, kCFStringEncodingUTF8, false);
                        CFIndex prefixLen = prefix ? CFStringGetLength(prefix) : 0;
                        CFStringRef icf = CFStringCreateWithBytes(NULL, (const UInt8*)imeComp.data(), imeComp.size(), kCFStringEncodingUTF8, false);
                        CFIndex imeLen = icf ? CFStringGetLength(icf) : 0;
                        int32_t style = kCTUnderlineStyleSingle; CFNumberRef n = CFNumberCreate(NULL, kCFNumberSInt32Type, &style);
                        CFAttributedStringSetAttribute(mas, CFRangeMake(prefixLen, imeLen), kCTUnderlineStyleAttributeName, n);
                        CFRelease(n); if (prefix) CFRelease(prefix); if (icf) CFRelease(icf);
                    }
                    CTLineRef tl = CTLineCreateWithAttributedString(mas);
                    CGContextSetTextPosition(ctx, gutterWidth - (float)hScrollPos, (float)(i - start) * lineHeight + asc + 2.0f);
                    CTLineDraw(tl, ctx); CFRelease(tl); CFRelease(mas); CFRelease(cf);
                }
            }
            
            if (e > s) {
                bool isCRLF = false, isLF = false, isCR = false;
                size_t newlinePos = e;
                if (e - s >= 2 && pt.charAt(e - 2) == '\r' && pt.charAt(e - 1) == '\n') {
                    isCRLF = true; newlinePos = e - 2;
                } else if (pt.charAt(e - 1) == '\n') {
                    isLF = true; newlinePos = e - 1;
                } else if (pt.charAt(e - 1) == '\r') {
                    isCR = true; newlinePos = e - 1;
                }
                if (isCRLF || isLF || isCR) {
                    float px = getXInLine(i, newlinePos);
                    float cx = gutterWidth - (float)hScrollPos + px + charWidth * 0.5f;
                    float cy = (float)(i - start) * lineHeight + lineHeight * 0.5f;
                    CGContextSetRGBStrokeColor(ctx, 0.5f, 0.5f, 0.5f, 0.3f);
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
                    } else { // LF
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
        }
        CGContextSetFillColorWithColor(ctx, colCaret);
        for (const auto& c : cursors) {
            int l = getLineIdx(c.head);
            if (l >= start && l < end) {
                float physX = getXInLine(l, c.head); float drawX = physX; if (c.isVirtual) drawX = std::max(physX, c.desiredX);
                CGContextFillRect(ctx, CGRectMake(gutterWidth - (float)hScrollPos + drawX, (float)(l - start) * lineHeight, 2, lineHeight));
            }
        }
        CGContextRestoreGState(ctx);
        CGContextSetFillColorWithColor(ctx, colGutterBg); CGContextFillRect(ctx, CGRectMake(0, 0, gutterWidth, h));
        for (int i = start; i < end; ++i) {
            CFStringRef n = CFStringCreateWithCString(NULL, std::to_string(i + 1).c_str(), kCFStringEncodingUTF8);
            if (n) {
                NSDictionary *attr = @{ (id)kCTFontAttributeName: (__bridge id)fontRef, (id)kCTForegroundColorAttributeName: (__bridge id)colGutterText };
                NSAttributedString *nas = [[NSAttributedString alloc] initWithString:(__bridge NSString*)n attributes:attr];
                CTLineRef nl = CTLineCreateWithAttributedString((CFAttributedStringRef)nas);
                double ascent, descent, leading; double lineWidth = CTLineGetTypographicBounds(nl, &ascent, &descent, &leading);
                float xPos = gutterWidth - (float)lineWidth - (charWidth * 0.5f); if (xPos < 5.0f) xPos = 5.0f;
                CGContextSetTextPosition(ctx, xPos, (float)(i - start) * lineHeight + asc + 2.0f);
                CTLineDraw(nl, ctx); CFRelease(nl); CFRelease(n);
            }
        }
        auto now = std::chrono::steady_clock::now();
        if (now < zoomPopupEndTime) {
            float zw = 160.0f, zh = 80.0f;
            CGRect zpr = CGRectMake((w - zw) / 2.0f, (h - zh) / 2.0f, zw, zh);
            CGContextSetFillColorWithColor(ctx, CGColorCreateGenericRGB(0.0, 0.0, 0.0, 0.7));
            CGPathRef path = CGPathCreateWithRoundedRect(zpr, 10.0f, 10.0f, NULL);
            CGContextAddPath(ctx, path); CGContextFillPath(ctx); CGPathRelease(path);
            CFStringRef zcf = CFStringCreateWithCString(NULL, zoomPopupText.c_str(), kCFStringEncodingUTF8);
            CTFontRef zf = CTFontCreateWithName(CFSTR("Helvetica-Bold"), 24.0f, NULL);
            CGColorRef white = CGColorCreateGenericRGB(1, 1, 1, 1);
            NSDictionary *za = @{ (id)kCTFontAttributeName: (__bridge id)zf, (id)kCTForegroundColorAttributeName: (__bridge id)white };
            NSAttributedString *zas = [[NSAttributedString alloc] initWithString:(__bridge NSString*)zcf attributes:za];
            CTLineRef zl = CTLineCreateWithAttributedString((CFAttributedStringRef)zas);
            double lineWidth = CTLineGetTypographicBounds(zl, NULL, NULL, NULL);
            CGFloat asc = CTFontGetAscent(zf);
            CGFloat desc = CTFontGetDescent(zf);
            float textY = zpr.origin.y + (zh - (asc + desc)) / 2.0f + asc;
            CGContextSetTextPosition(ctx, zpr.origin.x + (zw - (float)lineWidth) / 2.0f, textY);
            CTLineDraw(zl, ctx);
            CFRelease(zl); CFRelease(zf); CFRelease(zcf); CGColorRelease(white);
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{ [(NSView*)view setNeedsDisplay:YES]; });
        }
        if (showHelpPopup) {
            float hw = 500.0f, hh = 550.0f;
            CGRect hr = CGRectMake((w - hw) / 2.0f, (h - hh) / 2.0f, hw, hh);
            CGContextSetFillColorWithColor(ctx, CGColorCreateGenericRGB(0.1, 0.1, 0.1, 0.5));
            CGPathRef hpath = CGPathCreateWithRoundedRect(hr, 10.0f, 10.0f, NULL);
            CGContextAddPath(ctx, hpath); CGContextFillPath(ctx); CGPathRelease(hpath);
            std::wstring fullHelp = APP_VERSION + L"\n\n" + helpTextStr;
            CFStringRef hcf = CFStringCreateWithBytes(NULL, (const UInt8*)fullHelp.data(), fullHelp.size()*sizeof(wchar_t), kCFStringEncodingUTF32LE, false);
            CTFontRef hf = CTFontCreateWithName(CFSTR("Menlo"), 16.0f, NULL);
            NSDictionary *ha = @{(id)kCTFontAttributeName: (__bridge id)hf, (id)kCTForegroundColorAttributeName: (__bridge id)CGColorCreateGenericRGB(1,1,1,1)};
            NSAttributedString *has = [[NSAttributedString alloc] initWithString:(__bridge NSString*)hcf attributes:ha];
            CTFramesetterRef fs = CTFramesetterCreateWithAttributedString((CFAttributedStringRef)has);
            CGContextSaveGState(ctx);
            CGContextSetTextMatrix(ctx, CGAffineTransformIdentity);
            CGContextTranslateCTM(ctx, hr.origin.x, hr.origin.y + hr.size.height);
            CGContextScaleCTM(ctx, 1.0f, -1.0f);
            CGMutablePathRef pth = CGPathCreateMutable();
            CGPathAddRect(pth, NULL, CGRectMake(20.0f, 20.0f, hw - 40.0f, hh - 30.0f));
            CTFrameRef frame = CTFramesetterCreateFrame(fs, CFRangeMake(0, 0), pth, NULL);
            CTFrameDraw(frame, ctx);
            CGContextRestoreGState(ctx);
            CFRelease(frame); CFRelease(pth); CFRelease(fs); CFRelease(hf); CFRelease(hcf);
        }
    }
};

@implementation EditorView {
    NSScroller *vScroller, *hScroller;
    NSPoint boxSelectStartPoint;
    size_t cursorsSnapshotCount;
}

- (instancetype)initWithFrame:(NSRect)fr {
    if (self = [super initWithFrame:fr]) {
        editor = std::make_shared<Editor>();
        editor->view = self;
        CGFloat sw = [NSScroller scrollerWidthForControlSize:NSControlSizeRegular scrollerStyle:NSScrollerStyleLegacy];
        vScroller = [[NSScroller alloc] initWithFrame:NSMakeRect(fr.size.width-sw, 0, sw, fr.size.height-sw)];
        [vScroller setAutoresizingMask:NSViewMinXMargin|NSViewHeightSizable];
        [vScroller setTarget:self]; [vScroller setAction:@selector(scrollAction:)]; [self addSubview:vScroller];
        hScroller = [[NSScroller alloc] initWithFrame:NSMakeRect(0, fr.size.height-sw, fr.size.width-sw, sw)];
        [hScroller setAutoresizingMask:NSViewWidthSizable|NSViewMinYMargin];
        [hScroller setTarget:self]; [hScroller setAction:@selector(scrollAction:)]; [self addSubview:hScroller];
        [self registerForDraggedTypes:@[NSPasteboardTypeFileURL]];
    }
    return self;
}
- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender {
    NSPasteboard *pboard = [sender draggingPasteboard];
    if ([[pboard types] containsObject:NSPasteboardTypeFileURL]) return NSDragOperationCopy;
    return NSDragOperationNone;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender {
    NSPasteboard *pboard = [sender draggingPasteboard];
    if ([[pboard types] containsObject:NSPasteboardTypeFileURL]) {
        NSArray *urls = [pboard readObjectsForClasses:@[[NSURL class]] options:nil];
        if (urls.count > 0) {
            if (editor->checkUnsavedChanges()) {
                editor->openFileFromPath([[urls[0] path] UTF8String]);
                [[self window] makeKeyAndOrderFront:nil];
                return YES;
            }
        }
    }
    return NO;
}

- (BOOL)acceptsFirstResponder { return YES; }
- (BOOL)becomeFirstResponder { return YES; }
- (BOOL)isFlipped { return YES; }
- (void)viewDidMoveToWindow {
    if (![self window]) return;
    editor->initGraphics();
    BOOL isDark = [[self.effectiveAppearance bestMatchFromAppearancesWithNames:@[NSAppearanceNameAqua, NSAppearanceNameDarkAqua]] isEqualToString:NSAppearanceNameDarkAqua];
    editor->isDarkMode = isDark;
    editor->updateThemeColors(); [self updateScrollers];
    [[self window] makeFirstResponder:self];
}
- (void)viewDidChangeEffectiveAppearance {
    [super viewDidChangeEffectiveAppearance];
    if (editor) {
        BOOL isDark = [[self.effectiveAppearance bestMatchFromAppearancesWithNames:@[NSAppearanceNameAqua, NSAppearanceNameDarkAqua]] isEqualToString:NSAppearanceNameDarkAqua];
        if (editor->isDarkMode != isDark) {
            editor->isDarkMode = isDark;
            editor->updateThemeColors();
            [self setNeedsDisplay:YES];
        }
    }
}
- (void)drawRect:(NSRect)r {
    editor->render([[NSGraphicsContext currentContext] CGContext], (float)self.bounds.size.width, (float)self.bounds.size.height);
}
- (void)updateScrollers {
    NSRect b = [self bounds];
    CGFloat sw = [NSScroller scrollerWidthForControlSize:NSControlSizeRegular scrollerStyle:NSScrollerStyleLegacy];
    float visibleHeight = b.size.height;
    float visibleWidth = b.size.width - editor->gutterWidth;
    int totalLines = (int)editor->lineStarts.size();
    float maxLineWidth = editor->maxLineWidth;
    bool needsV = totalLines > 1;
    if (needsV) visibleWidth -= sw;
    bool needsH = maxLineWidth > visibleWidth;
    if (needsH) visibleHeight -= sw;    [vScroller setHidden:!needsV];
    [hScroller setHidden:!needsH];
    editor->visibleVScrollWidth = needsV ? sw : 0.0f;
    editor->visibleHScrollHeight = needsH ? sw : 0.0f;
    int visibleLines = (int)std::floor(visibleHeight / editor->lineHeight); if (visibleLines < 1) visibleLines = 1;
    [vScroller setKnobProportion:std::min(1.0, (double)visibleLines / (totalLines + visibleLines - 1))];
    int maxV = std::max(0, totalLines - 1);
    if (needsV && maxV > 0) { [vScroller setDoubleValue:(double)editor->vScrollPos / maxV]; [vScroller setEnabled:YES]; } else { [vScroller setDoubleValue:0.0]; [vScroller setEnabled:NO]; editor->vScrollPos = 0; }
    if (visibleWidth < 1) visibleWidth = 1;
    [hScroller setKnobProportion:std::min(1.0, (double)visibleWidth / maxLineWidth)];
    float maxH = maxLineWidth - visibleWidth;
    if (needsH && maxH > 0) { [hScroller setDoubleValue:(double)editor->hScrollPos / maxH]; [hScroller setEnabled:YES]; } else { [hScroller setDoubleValue:0.0]; [hScroller setEnabled:NO]; editor->hScrollPos = 0; }
    [self setNeedsDisplay:YES];
}
- (void)scrollAction:(NSScroller*)s {
    NSRect b = [self bounds]; CGFloat sw = [NSScroller scrollerWidthForControlSize:NSControlSizeRegular scrollerStyle:NSScrollerStyleLegacy];
    if (s == vScroller) {
        int maxV = std::max(0, (int)editor->lineStarts.size() - 1);
        editor->vScrollPos = std::max(0, (int)std::round([s doubleValue] * maxV));
    } else {
        float visibleWidth = b.size.width - editor->gutterWidth - sw; float maxH = editor->maxLineWidth - visibleWidth;
        editor->hScrollPos = std::max(0, (int)([s doubleValue] * maxH));
    }
    [self setNeedsDisplay:YES];
}
- (void)applyZoom:(float)val relative:(bool)rel {
    editor->updateFont(rel ? editor->currentFontSize * val : val);
    editor->zoomPopupEndTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(1000);
    editor->zoomPopupText = std::to_string((int)std::round(editor->currentFontSize)) + "px";
    [self setNeedsDisplay:YES];
}

- (void)mouseDown:(NSEvent *)e {
    if (editor->showHelpPopup) { editor->showHelpPopup = false; [self setNeedsDisplay:YES]; }
    [[self window] makeFirstResponder:self];
    NSPoint p = [self convertPoint:[e locationInWindow] fromView:nil];
    if (p.x > self.bounds.size.width - editor->visibleVScrollWidth || p.y > self.bounds.size.height - editor->visibleHScrollHeight) return;
    NSInteger clicks = [e clickCount];
    bool isOption = ([e modifierFlags] & NSEventModifierFlagOption);
    bool isShift = ([e modifierFlags] & NSEventModifierFlagShift);
    if (!isOption && !isShift) editor->cursors.clear();
    boxSelectStartPoint = p;
    cursorsSnapshotCount = editor->cursors.size();
    float docX = (float)p.x - editor->gutterWidth + (float)editor->hScrollPos;
    float snappedX = docX;
    if (editor->charWidth > 0) { int cols = (int)std::round(docX / editor->charWidth); if (cols < 0) cols = 0; snappedX = (float)cols * editor->charWidth; }
    size_t pos = editor->getDocPosFromPoint((float)p.x, (float)p.y);
    int li = editor->getLineIdx(pos);
    float physX = editor->getXInLine(li, pos);
    Cursor newCursor; newCursor.head = pos; newCursor.anchor = pos; newCursor.isVirtual = false;
    if (isOption && (snappedX > physX + (editor->charWidth * 0.5f))) { newCursor.desiredX = snappedX; newCursor.isVirtual = true; }
    else { newCursor.desiredX = physX; newCursor.isVirtual = false; }
    newCursor.originalAnchorX = newCursor.desiredX;
    if (clicks == 2) {
        size_t s, end; editor->getWordBoundaries(pos, s, end);
        newCursor.head = end; newCursor.anchor = s; newCursor.desiredX = editor->getXInLine(editor->getLineIdx(end), end); newCursor.originalAnchorX = newCursor.desiredX; newCursor.isVirtual = false;
        editor->cursors.push_back(newCursor);
    } else if (clicks >= 3) {
        size_t s = editor->lineStarts[li], end = (li + 1 < (int)editor->lineStarts.size()) ? editor->lineStarts[li+1] : editor->pt.length();
        newCursor.head = end; newCursor.anchor = s; newCursor.desiredX = editor->getXInLine(editor->getLineIdx(end), end); newCursor.originalAnchorX = newCursor.desiredX; newCursor.isVirtual = false;
        editor->cursors.push_back(newCursor);
    } else { editor->cursors.push_back(newCursor); }
    [self setNeedsDisplay:YES];
}

- (void)mouseDragged:(NSEvent *)e {
    NSPoint p = [self convertPoint:[e locationInWindow] fromView:nil];
    bool isOption = ([e modifierFlags] & NSEventModifierFlagOption);
    auto snapToGrid = [&](float rawX) -> float {
        if (editor->charWidth <= 0) return rawX; int cols = (int)std::round(rawX / editor->charWidth); if (cols < 0) cols = 0; return (float)cols * editor->charWidth;
    };
    if (isOption) {
        editor->cursors.clear();
        float startY = std::min((float)boxSelectStartPoint.y, (float)p.y), endY = std::max((float)boxSelectStartPoint.y, (float)p.y);
        int startLine = (int)std::floor((startY + (float)editor->vScrollPos * editor->lineHeight) / editor->lineHeight);
        int endLine = (int)std::floor((endY + (float)editor->vScrollPos * editor->lineHeight) / editor->lineHeight);
        startLine = std::clamp(startLine, 0, (int)editor->lineStarts.size() - 1); endLine = std::clamp(endLine, 0, (int)editor->lineStarts.size() - 1);
        float rawStartX = (float)boxSelectStartPoint.x - editor->gutterWidth + (float)editor->hScrollPos;
        float rawEndX = (float)p.x - editor->gutterWidth + (float)editor->hScrollPos;
        float gridStartX = snapToGrid(rawStartX), gridEndX = snapToGrid(rawEndX);
        for (int i = startLine; i <= endLine; ++i) {
            size_t pHead = editor->getPosFromLineAndX(i, gridEndX), pAnchor = editor->getPosFromLineAndX(i, gridStartX);
            Cursor c; c.head = pHead; c.anchor = pAnchor; c.desiredX = gridEndX; c.originalAnchorX = gridStartX; c.isVirtual = true;
            editor->cursors.push_back(c);
        }
    } else {
        size_t pos = editor->getDocPosFromPoint((float)p.x, (float)p.y);
        if (!editor->cursors.empty()) { Cursor& c = editor->cursors.back(); c.head = pos; int li = editor->getLineIdx(pos); c.desiredX = editor->getXInLine(li, pos); c.isVirtual = false; }
    }
    editor->ensureCaretVisible(); [self setNeedsDisplay:YES];
}
- (void)mouseUp:(NSEvent *)e { [self setNeedsDisplay:YES]; }

- (void)keyDown:(NSEvent *)e {
    unsigned short code = [e keyCode]; NSString *chars = [e charactersIgnoringModifiers];
    bool cmd = ([e modifierFlags] & NSEventModifierFlagCommand);
    bool shift = ([e modifierFlags] & NSEventModifierFlagShift);
    bool ctrl = ([e modifierFlags] & NSEventModifierFlagControl);
    if (editor->showHelpPopup) { editor->showHelpPopup = false; [self setNeedsDisplay:YES]; if (code == 122) return; }
    if (code == 122) { editor->showHelpPopup = true; [self setNeedsDisplay:YES]; return; }
    if (code == 53) { if (editor->cursors.size() > 1 || (editor->cursors.size() == 1 && editor->cursors[0].hasSelection())) { Cursor lastC = editor->cursors.back(); lastC.anchor = lastC.head; editor->cursors.clear(); editor->cursors.push_back(lastC); [self setNeedsDisplay:YES]; return; } }
    if (code == 48) { // Tab キー
        if (shift) {
            editor->unindentLines();
        } else {
            bool isRectMode = editor->cursors.size() > 1;
            if (isRectMode) {
                // 矩形選択など複数カーソル時は現在の位置にタブを挿入
                editor->insertAtCursors("\t");
            } else {
                // 範囲選択時 または 通常のインデント
                editor->indentLines(false);
            }
        }
        [self setNeedsDisplay:YES];
        return;
    }
    if (code == 115 || code == 119) {
        for (auto& c : editor->cursors) {
            if (code == 115) { if (cmd) c.head = 0; else c.head = editor->lineStarts[editor->getLineIdx(c.head)]; }
            else { if (cmd) c.head = editor->pt.length(); else { int li = editor->getLineIdx(c.head); size_t nextLineStart = (li + 1 < (int)editor->lineStarts.size()) ? editor->lineStarts[li + 1] : editor->pt.length(); if (nextLineStart > editor->lineStarts[li] && editor->pt.charAt(nextLineStart - 1) == '\n') { nextLineStart--; if (nextLineStart > editor->lineStarts[li] && editor->pt.charAt(nextLineStart - 1) == '\r') nextLineStart--; } c.head = nextLineStart; } }
            
            c.desiredX = editor->getXFromPos(c.head);
            if (!shift) { c.anchor = c.head; c.originalAnchorX = c.desiredX; }
            c.isVirtual = false;
        }
        editor->ensureCaretVisible(); [self setNeedsDisplay:YES]; return;
    }
    if (code == 116 || code == 121) {
        float viewHeight = [self bounds].size.height - editor->visibleHScrollHeight;
        int pageLines = std::max(1, (int)(viewHeight / editor->lineHeight) - 1);
        int totalLines = (int)editor->lineStarts.size();
        for (auto& c : editor->cursors) {
            int currentLine = editor->getLineIdx(c.head); int newLine = currentLine;
            if (code == 116) { newLine = currentLine - pageLines; if (newLine < 0) newLine = 0; }
            else { newLine = currentLine + pageLines; if (newLine >= totalLines) newLine = totalLines - 1; }
            if (code == 121 && newLine == totalLines - 1 && currentLine == totalLines - 1) c.head = editor->pt.length();
            else c.head = editor->getPosFromLineAndX(newLine, c.desiredX);
            if (!shift) { c.anchor = c.head; c.originalAnchorX = c.desiredX; }
            c.isVirtual = false;
        }
        if (code == 116) editor->vScrollPos = std::max(0, editor->vScrollPos - pageLines); else editor->vScrollPos = std::min(totalLines - 1, editor->vScrollPos + pageLines);
        editor->ensureCaretVisible(); [self setNeedsDisplay:YES]; return;
    }
    if (cmd) {
        NSString *lowerChar = [chars lowercaseString];
        if (ctrl && [lowerChar isEqualToString:@"f"]) { [[self window] toggleFullScreen:nil]; return; }
        if ([lowerChar isEqualToString:@"q"]) { [NSApp terminate:nil]; return; }
        if ([lowerChar isEqualToString:@"u"]) { editor->convertSelectedText(!shift); return; }
        if (shift && [lowerChar isEqualToString:@"k"]) { editor->deleteLine(); [self setNeedsDisplay:YES]; return; }
        if ([lowerChar isEqualToString:@"d"]) { editor->selectNextOccurrence(); [self setNeedsDisplay:YES]; return; }
        if ([lowerChar isEqualToString:@"a"]) { editor->selectAll(); [self setNeedsDisplay:YES]; return; }
        if ([lowerChar isEqualToString:@"s"]) { shift ? editor->saveFileAs() : (editor->currentFilePath.empty() ? editor->saveFileAs() : editor->saveFile(editor->currentFilePath)); [self setNeedsDisplay:YES]; return; }
        if ([lowerChar isEqualToString:@"c"]) { editor->copyToClipboard(); return; }
        if ([lowerChar isEqualToString:@"x"]) { editor->cutToClipboard(); [self setNeedsDisplay:YES]; return; }
        if ([lowerChar isEqualToString:@"v"]) { editor->pasteFromClipboard(); [self setNeedsDisplay:YES]; return; }
        if ([lowerChar isEqualToString:@"z"]) { shift ? editor->performRedo() : editor->performUndo(); [self setNeedsDisplay:YES]; return; }
        if ([lowerChar isEqualToString:@"o"]) { [NSApp sendAction:@selector(openDocument:) to:nil from:self]; return; }
        if ([lowerChar isEqualToString:@"n"]) { [NSApp sendAction:@selector(newDocument:) to:nil from:self]; return; }
        if ([lowerChar isEqualToString:@"0"]) { [self applyZoom:14.0f relative:false]; return; }
        if ([lowerChar isEqualToString:@"+"] || [lowerChar isEqualToString:@"="]) { [self applyZoom:1.1f relative:true]; return; }
        if ([lowerChar isEqualToString:@"-"]) { [self applyZoom:0.9f relative:true]; return; }
        if ([lowerChar isEqualToString:@"]"]) { editor->indentLines(true); [self setNeedsDisplay:YES]; return; }
        if ([lowerChar isEqualToString:@"["]) { editor->unindentLines(); [self setNeedsDisplay:YES]; return; }
    }
    if (code >= 123 && code <= 126) {
        bool opt = ([e modifierFlags] & NSEventModifierFlagOption);
        if (code == 126 && opt) { if (shift) editor->copyLines(true); else editor->moveLines(true); [self setNeedsDisplay:YES]; return; }
        if (code == 125 && opt) { if (shift) editor->copyLines(false); else editor->moveLines(false); [self setNeedsDisplay:YES]; return; }
        for (auto& c : editor->cursors) {
            if (code == 123) {
                size_t p = c.head; int li = editor->getLineIdx(p); size_t lineStart = editor->lineStarts[li];
                if (cmd) { if (p == lineStart && p > 0) p--; else { while (p > lineStart && !editor->isWordChar(editor->pt.charAt(p - 1))) p--; while (p > lineStart && editor->isWordChar(editor->pt.charAt(p - 1))) p--; } c.head = p; }
                else c.head = editor->moveCaretVisual(c.head, false);
                c.isVirtual = false;
            } else if (code == 124) {
                size_t p = c.head; size_t len = editor->pt.length(); int li = editor->getLineIdx(p);
                size_t lineStart = editor->lineStarts[li], lineEnd = (li + 1 < (int)editor->lineStarts.size()) ? editor->lineStarts[li + 1] : len;
                size_t physEnd = lineEnd; if (physEnd > lineStart && editor->pt.charAt(physEnd - 1) == '\n') physEnd--; if (physEnd > lineStart && editor->pt.charAt(physEnd - 1) == '\r') physEnd--;
                if (cmd) { if (p == physEnd && p < len) p = lineEnd; else { while (p < physEnd && editor->isWordChar(editor->pt.charAt(p))) p++; while (p < physEnd && !editor->isWordChar(editor->pt.charAt(p))) p++; } c.head = p; }
                else c.head = editor->moveCaretVisual(c.head, true);
                c.isVirtual = false;
            } else if (code == 126) {
                int l = editor->getLineIdx(c.head);
                if (l > 0) { size_t nextPos = editor->getPosFromLineAndX(l - 1, c.desiredX); size_t prevLineStart = editor->lineStarts[l - 1]; size_t prevLineEnd = editor->lineStarts[l]; if (prevLineEnd > prevLineStart && editor->pt.charAt(prevLineEnd - 1) == '\n') prevLineEnd--; if (prevLineEnd > prevLineStart && editor->pt.charAt(prevLineEnd - 1) == '\r') prevLineEnd--; if (nextPos > prevLineEnd) c.head = prevLineEnd; else c.head = nextPos; }
                c.isVirtual = false;
            } else if (code == 125) {
                int l = editor->getLineIdx(c.head);
                if (l + 1 < (int)editor->lineStarts.size()) { size_t nextPos = editor->getPosFromLineAndX(l + 1, c.desiredX); size_t nextLineStart = editor->lineStarts[l + 1]; size_t nextLineEnd = (l + 2 < (int)editor->lineStarts.size()) ? editor->lineStarts[l + 2] : editor->pt.length(); if (nextLineEnd > nextLineStart && editor->pt.charAt(nextLineEnd - 1) == '\n') nextLineEnd--; if (nextLineEnd > nextLineStart && editor->pt.charAt(nextLineEnd - 1) == '\r') nextLineEnd--; if (nextPos > nextLineEnd) c.head = nextLineEnd; else c.head = nextPos; }
                c.isVirtual = false;
            }
            if (code == 123 || code == 124) c.desiredX = editor->getXFromPos(c.head);
            if (!shift) { c.anchor = c.head; c.originalAnchorX = c.desiredX; }
        }
        editor->ensureCaretVisible(); [self setNeedsDisplay:YES]; return;
    }
    if (![self.inputContext handleEvent:e]) [super keyDown:e];
}

- (void)scrollWheel:(NSEvent *)e {
    if ([e modifierFlags] & NSEventModifierFlagCommand) { float dy = [e scrollingDeltaY]; if (dy == 0) dy = [e deltaY]; if (dy != 0) { float factor = (dy > 0) ? 1.1f : 0.9f; editor->updateFont(editor->currentFontSize * factor); editor->zoomPopupText = std::to_string((int)std::round(editor->currentFontSize)) + "px"; editor->zoomPopupEndTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(1000); [self setNeedsDisplay:YES]; } return; }
    CGFloat sw = [NSScroller scrollerWidthForControlSize:NSControlSizeRegular scrollerStyle:NSScrollerStyleLegacy];
    NSRect b = [self bounds];
    float visibleHeight = b.size.height - editor->visibleHScrollHeight;
    int visibleLines = (int)(visibleHeight / editor->lineHeight);
    int totalLines = (int)editor->lineStarts.size();
    int maxV = std::max(0, totalLines - 1);
    editor->vScrollPos = std::clamp(editor->vScrollPos - (int)std::round([e deltaY]), 0, maxV);
    float visibleWidth = b.size.width - editor->gutterWidth - editor->visibleVScrollWidth;
    int maxH = std::max(0, (int)(editor->maxLineWidth - visibleWidth + editor->charWidth * 4));
    editor->hScrollPos = std::clamp(editor->hScrollPos - (int)[e deltaX], 0, maxH);
    [self updateScrollers]; [self setNeedsDisplay:YES];
}
- (void)insertText:(id)s replacementRange:(NSRange)r {
    NSString* t = [s isKindOfClass:[NSAttributedString class]] ? [s string] : s;
    if ([t isEqualToString:@"\r"] || [t isEqualToString:@"\n"]) {
        editor->insertNewlineWithAutoIndent();
    } else {
        if (editor->cursors.size() > 1) editor->insertAtCursorsWithPadding([t UTF8String]);
        else editor->insertAtCursors([t UTF8String]);
    }
    editor->imeComp = "";
    [self setNeedsDisplay:YES];
}
- (void)setMarkedText:(id)s selectedRange:(NSRange)sr replacementRange:(NSRange)rr { editor->imeComp = [([s isKindOfClass:[NSAttributedString class]] ? [s string] : s) UTF8String]; [self setNeedsDisplay:YES]; }
- (void)unmarkText { editor->imeComp = ""; [self setNeedsDisplay:YES]; }
- (BOOL)hasMarkedText { return !editor->imeComp.empty(); }
- (NSRange)markedRange { return [self hasMarkedText] ? NSMakeRange(0, editor->imeComp.length()) : NSMakeRange(NSNotFound, 0); }
- (NSRange)selectedRange { return NSMakeRange(NSNotFound, 0); }
- (void)doCommandBySelector:(SEL)s {
    if (s == @selector(deleteBackward:)) editor->backspaceAtCursors();
    else if (s == @selector(deleteForward:)) editor->deleteForwardAtCursors();
    else if (s == @selector(insertNewline:)) editor->insertNewlineWithAutoIndent(); // ここを変更
    [self setNeedsDisplay:YES]; }
- (nullable NSAttributedString *)attributedSubstringForProposedRange:(NSRange)r actualRange:(NSRangePointer)ar { return nil; }
- (NSArray*)validAttributesForMarkedText { return @[]; }
- (NSRect)firstRectForCharacterRange:(NSRange)r actualRange:(NSRangePointer)ar { if (editor->cursors.empty()) return NSZeroRect; float x = editor->getXFromPos(editor->cursors.back().head), y = (float)(editor->getLineIdx(editor->cursors.back().head) - editor->vScrollPos) * editor->lineHeight; return [[self window] convertRectToScreen:[self convertRect:NSMakeRect(editor->gutterWidth-(float)editor->hScrollPos+x, y, 2, editor->lineHeight) toView:nil]]; }
- (NSUInteger)characterIndexForPoint:(NSPoint)p { return 0; }
@end

@interface CustomWindow : NSWindow @end
@implementation CustomWindow
- (BOOL)canBecomeKeyWindow { return YES; }
- (BOOL)canBecomeMainWindow { return YES; }
@end

@interface AppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate>
@property (strong) NSMutableArray<CustomWindow *> *windows;
@end

@implementation AppDelegate
- (instancetype)init { if (self = [super init]) { self.windows = [NSMutableArray array]; } return self; }
- (void)createWindowWithPath:(const char*)path {
    CustomWindow *win = [[CustomWindow alloc] initWithContentRect:NSMakeRect(0,0,800,600) styleMask:NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskResizable|NSWindowStyleMaskMiniaturizable backing:NSBackingStoreBuffered defer:NO];
    [win center]; [win setDelegate:self];
    if (self.windows.count > 0) { NSRect lastFrame = self.windows.lastObject.frame; [win setFrameTopLeftPoint:NSMakePoint(lastFrame.origin.x + 22, lastFrame.origin.y - 22)]; }
    EditorView *v = [[EditorView alloc] initWithFrame:win.contentView.bounds];
    if (path) v->editor->openFileFromPath(path); else v->editor->newFile();
    [win setContentView:v]; [win makeKeyAndOrderFront:nil]; [win makeFirstResponder:v];
    [self.windows addObject:win];
}
- (void)applicationDidFinishLaunching:(NSNotification *)n {
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular]; [NSApp activateIgnoringOtherApps:YES];
    NSArray *args = [[NSProcessInfo processInfo] arguments];
    if (args.count > 1) [self createWindowWithPath:[args[1] UTF8String]]; else if (self.windows.count == 0) [self createWindowWithPath:nullptr];
}
- (void)application:(NSApplication *)sender openFiles:(NSArray<NSString *> *)filenames { for (NSString *path in filenames) [self createWindowWithPath:[path UTF8String]]; [sender replyToOpenOrPrint:NSApplicationDelegateReplySuccess]; }
- (void)newDocument:(id)sender { NSWindow *win = [NSApp keyWindow]; if (win && [win isKindOfClass:[CustomWindow class]]) { EditorView *v = (EditorView *)win.contentView; v->editor->newFile(); } else { [self createWindowWithPath:nullptr]; } }
- (void)openDocument:(id)sender { NSWindow *win = [NSApp keyWindow]; if (win && [win isKindOfClass:[CustomWindow class]]) { EditorView *v = (EditorView *)win.contentView; v->editor->openFile(); } else { [self createWindowWithPath:nullptr]; CustomWindow *newWin = self.windows.lastObject; EditorView *v = (EditorView *)newWin.contentView; if (!v->editor->openFile()) {} } }
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender { return YES; }
- (BOOL)windowShouldClose:(NSWindow *)sender { EditorView *v = (EditorView *)sender.contentView; if (v && v->editor) { if (!v->editor->checkUnsavedChanges()) return NO; } return YES; }
- (void)windowWillClose:(NSNotification *)notification { CustomWindow *win = (CustomWindow *)notification.object; [self.windows removeObject:win]; }
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender { for (CustomWindow *win in self.windows) { EditorView *v = (EditorView *)win.contentView; if (v && v->editor) { if (!v->editor->checkUnsavedChanges()) return NSTerminateCancel; } } return NSTerminateNow; }
@end

int main(int argc, const char * argv[]) { @autoreleasepool { NSApplication *a = [NSApplication sharedApplication]; AppDelegate *d = [AppDelegate new]; [a setDelegate:d]; [a run]; } return 0; }
