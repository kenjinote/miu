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
// --- 定数・列挙型 ---
const std::wstring APP_VERSION = L"miu v1.0.15 (macOS)";
const std::wstring APP_TITLE = L"miu";
enum Encoding { ENC_UTF8_NOBOM = 0, ENC_UTF8_BOM, ENC_UTF16LE, ENC_UTF16BE, ENC_ANSI };
static NSString *const kMiuRectangularSelectionType = @"com.kenji.miu.rectangular";
// --- 1. 文字コード変換ヘルパー ---
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
// --- 2. 前方宣言 ---
@interface EditorView : NSView <NSTextInputClient>
- (void)updateScrollers;
- (void)applyZoom:(float)val relative:(bool)rel;
@end
// --- 3. データ構造 ---
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
    bool isVirtual; // 仮想スペースモードフラグ
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

// --- 4. Editor Core ---
struct Editor {
    EditorView* view = nullptr;
    PieceTable pt;
    UndoManager undo;
    std::unique_ptr<MappedFile> fileMap;
    std::wstring currentFilePath;
    
    // --- 状態管理 ---
    Encoding currentEncoding = ENC_UTF8_NOBOM;
    bool isDirty = false;
    bool isDarkMode = false;
    bool showHelpPopup = false;
    
    // ズームポップアップ用
    std::chrono::steady_clock::time_point zoomPopupEndTime;
    std::string zoomPopupText = "";
    
    // レイアウト設定
    float currentFontSize = 14.0f;
    float lineHeight = 18.0f;
    float charWidth = 8.0f;
    float gutterWidth = 40.0f;
    float maxLineWidth = 100.0f;
    int vScrollPos = 0, hScrollPos = 0;
    
    std::vector<Cursor> cursors;
    std::vector<size_t> lineStarts;
    std::string imeComp;
    
    // 描画リソース
    CGColorRef colBackground=NULL, colText=NULL, colGutterBg=NULL, colGutterText=NULL, colSel=NULL, colCaret=NULL;
    CTFontRef fontRef = nullptr;
    std::wstring helpTextStr = L"F1: Toggle Help\nCmd+D: Select Word / Next\nCmd+S: Save\nCmd+O: Open\nCmd+N: New\nCmd+A: Select All\nCmd+C/X/V: Copy/Cut/Paste\nCmd+Z/Shift+Z: Undo/Redo\nCmd+Shift+K: Delete Line\nAlt+Up/Down: Move Line\nAlt+Shift+Up/Down: Copy Line\nOption+Drag: Box Select";
    
    // --- 挿入・編集ロジック ---
    void insertAtCursorsWithPadding(const std::string& text) {
        if (cursors.empty()) return;
        
        EditBatch batch;
        batch.beforeCursors = cursors;
        
        auto sortedIndices = std::vector<size_t>(cursors.size());
        std::iota(sortedIndices.begin(), sortedIndices.end(), 0);
        std::sort(sortedIndices.begin(), sortedIndices.end(), [&](size_t a, size_t b) {
            return cursors[a].start() > cursors[b].start();
        });
        
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
                
                for (auto& oc : cursors) {
                    if (oc.head > st) oc.head -= len;
                    if (oc.anchor > st) oc.anchor -= len;
                }
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
                    for (auto& oc : cursors) {
                        if (oc.head >= lineEnd) oc.head += padding.size();
                        if (oc.anchor >= lineEnd) oc.anchor += padding.size();
                    }
                }
            }
            
            pt.insert(targetPos, text);
            batch.ops.push_back({ EditOp::Insert, targetPos, text });
            for (auto& oc : cursors) {
                if (oc.head >= targetPos) oc.head += text.size();
                if (oc.anchor >= targetPos) oc.anchor += text.size();
            }
            // --- 修正箇所 ---
            c.desiredX = getXInLine(li, c.head);
            c.originalAnchorX = c.desiredX; // 【追加】選択開始位置も現在のカーソル位置に揃える
            c.isVirtual = false;
        }
        
        batch.afterCursors = cursors;
        undo.push(batch);
        rebuildLineStarts();
        ensureCaretVisible();
        updateDirtyFlag();
    }
    
    void insertRectangularBlock(const std::string& text) {
        if (cursors.empty()) return;
        
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
        std::sort(sortedCursors.begin(), sortedCursors.end(), [](const Cursor& a, const Cursor& b) {
            return a.start() < b.start();
        });
        
        size_t basePos = sortedCursors[0].head;
        float baseX = sortedCursors[0].desiredX;
        int startLineIdx = getLineIdx(basePos);
        
        int requiredTotalLines = startLineIdx + (int)lines.size();
        if (requiredTotalLines > (int)lineStarts.size()) {
            size_t eofPos = pt.length();
            std::string newLines = "";
            for (int k = 0; k < (requiredTotalLines - (int)lineStarts.size()); ++k) newLines += "\n";
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
            cursors.push_back({ p, p, finalX, finalX, false });
        }
        
        batch.afterCursors = cursors;
        undo.push(batch);
        rebuildLineStarts();
        ensureCaretVisible();
        updateDirtyFlag();
    }
    
    // 行操作用ヘルパー
    // 修正版: 選択範囲が含まれるすべての行を列挙する
    std::vector<int> getUniqueLineIndices() {
        std::vector<int> lines;
        for (const auto& c : cursors) {
            if (!c.hasSelection()) {
                // 選択なし：カーソル行のみ
                lines.push_back(getLineIdx(c.head));
            } else {
                // 選択あり：範囲内の全行
                size_t start = c.start();
                size_t end = c.end();
                int lStart = getLineIdx(start);
                int lEnd = getLineIdx(end);
                
                // 【重要】終了位置が行頭(0文字目)の場合、その行は操作対象に含めない
                // (例: 1行目〜2行目の頭まで選択している場合、2行目は移動しない)
                if (lEnd > lStart) {
                    size_t lineStartPos = lineStarts[lEnd];
                    if (end == lineStartPos) {
                        lEnd--;
                    }
                }
                
                for (int i = lStart; i <= lEnd; ++i) {
                    lines.push_back(i);
                }
            }
        }
        // 重複削除とソート
        std::sort(lines.begin(), lines.end());
        lines.erase(std::unique(lines.begin(), lines.end()), lines.end());
        return lines;
    }
    
    void deleteLine() {
        std::vector<int> lines = getUniqueLineIndices();
        if (lines.empty()) return;
        EditBatch batch;
        batch.beforeCursors = cursors;
        for (int i = (int)lines.size() - 1; i >= 0; --i) {
            int li = lines[i];
            size_t start = lineStarts[li];
            size_t end = (li + 1 < (int)lineStarts.size()) ? lineStarts[li + 1] : pt.length();
            size_t len = end - start;
            if (len > 0) {
                batch.ops.push_back({EditOp::Erase, start, pt.getRange(start, len)});
                pt.erase(start, len);
                for (auto& c : cursors) {
                    if (c.head > start) c.head = (c.head >= start + len) ? c.head - len : start;
                    if (c.anchor > start) c.anchor = (c.anchor >= start + len) ? c.anchor - len : start;
                }
            } else if (len == 0 && li > 0) {
                size_t delStart = lineStarts[li];
                size_t delLen = 0;
                if(delStart > 0 && pt.charAt(delStart-1) == '\n') { delStart--; delLen++; }
                if(delStart > 0 && pt.charAt(delStart-1) == '\r') { delStart--; delLen++; }
                if (delLen > 0) {
                    batch.ops.push_back({EditOp::Erase, delStart, pt.getRange(delStart, delLen)});
                    pt.erase(delStart, delLen);
                    for (auto& c : cursors) {
                        if (c.head > delStart) c.head -= delLen;
                        if (c.anchor > delStart) c.anchor -= delLen;
                    }
                }
            }
        }
        for (auto& c : cursors) { c.anchor = c.head; c.desiredX = getXFromPos(c.head); c.isVirtual=false; }
        batch.afterCursors = cursors;
        undo.push(batch);
        rebuildLineStarts();
        ensureCaretVisible();
        updateDirtyFlag();
    }
    
    void moveLines(bool up) {
        // 1. 末尾改行の保証
        size_t len = pt.length();
        if (len > 0 && pt.charAt(len-1) != '\n') {
            pt.insert(len, "\n");
            rebuildLineStarts();
        }
        
        // 2. 移動対象行の取得
        std::vector<int> lines = getUniqueLineIndices();
        if (lines.empty()) return;
        
        if (up && lines.front() == 0) return;
        if (!up && lines.back() >= (int)lineStarts.size() - 1) return;
        
        EditBatch batch;
        batch.beforeCursors = cursors;
        
        // グループ化
        std::vector<std::pair<int, int>> groups;
        int start = lines[0];
        int prev = lines[0];
        for (size_t i = 1; i < lines.size(); ++i) {
            if (lines[i] != prev + 1) {
                groups.push_back({start, prev});
                start = lines[i];
            }
            prev = lines[i];
        }
        groups.push_back({start, prev});
        
        if (up) {
            // --- 上へ移動 ---
            for (const auto& g : groups) {
                int gStart = g.first;
                int gEnd = g.second;
                int targetLine = gStart - 1;
                
                size_t posTarget = lineStarts[targetLine];
                size_t posStart = lineStarts[gStart];
                size_t posEnd = (gEnd + 1 < (int)lineStarts.size()) ? lineStarts[gEnd + 1] : pt.length();
                
                std::string movingText = pt.getRange(posStart, posEnd - posStart);
                size_t moveLen = movingText.length();
                size_t prevLineLen = posStart - posTarget; // 上の行の長さ
                
                bool isLastLine = (posEnd == pt.length());
                bool noNewline = (moveLen > 0 && movingText.back() != '\n');
                bool targetHasNewline = (prevLineLen > 0 && pt.charAt(posStart - 1) == '\n');
                
                pt.erase(posStart, moveLen);
                batch.ops.push_back({EditOp::Erase, posStart, movingText});
                
                if (isLastLine && noNewline && targetHasNewline) {
                    pt.erase(posStart - 1, 1);
                    batch.ops.push_back({EditOp::Erase, posStart - 1, "\n"});
                    movingText += "\n";
                    moveLen++;
                }
                
                pt.insert(posTarget, movingText);
                batch.ops.push_back({EditOp::Insert, posTarget, movingText});
                
                for (auto& c : cursors) {
                    // 【修正】Block A（移動する行）に含まれるかどうかの判定を厳密化
                    // 範囲内、または「選択範囲の終端として境界(posEnd)にある」場合は移動対象とする
                    bool headInA = (c.head >= posStart && c.head < posEnd);
                    if (c.head == posEnd && c.hasSelection() && c.end() == c.head) headInA = true;
                    // 特例: EOFを含む最終行移動の場合
                    if (isLastLine && c.head == posEnd) headInA = true;
                    
                    bool anchorInA = (c.anchor >= posStart && c.anchor < posEnd);
                    if (c.anchor == posEnd && c.hasSelection() && c.end() == c.anchor) anchorInA = true;
                    if (isLastLine && c.anchor == posEnd) anchorInA = true;
                    
                    // Headの移動
                    if (headInA) {
                        if (c.head >= prevLineLen) c.head -= prevLineLen; else c.head = 0;
                    } else if (c.head >= posTarget && c.head < posStart) {
                        c.head += moveLen; // 押し出される行
                    }
                    
                    // Anchorの移動
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
            // --- 下へ移動 ---
            for (int i = (int)groups.size() - 1; i >= 0; --i) {
                int gStart = groups[i].first;
                int gEnd = groups[i].second;
                int swapLine = gEnd + 1;
                
                size_t posStart = lineStarts[gStart];
                size_t posEnd = (gEnd + 1 < (int)lineStarts.size()) ? lineStarts[gEnd + 1] : pt.length();
                size_t posSwapEnd = (swapLine + 1 < (int)lineStarts.size()) ? lineStarts[swapLine + 1] : pt.length();
                
                std::string movingText = pt.getRange(posStart, posEnd - posStart);
                size_t moveLen = movingText.length();
                size_t swapLineLen = posSwapEnd - posEnd;
                
                bool movingHasNewline = (moveLen > 0 && movingText.back() == '\n');
                bool targetIsEOF = (posSwapEnd == pt.length());
                bool targetNoNewline = (swapLineLen > 0 && pt.charAt(posSwapEnd - 1) != '\n');
                
                pt.erase(posStart, moveLen);
                batch.ops.push_back({EditOp::Erase, posStart, movingText});
                
                size_t insertPos = posStart + swapLineLen;
                
                if (movingHasNewline && targetIsEOF && (targetNoNewline || swapLineLen == 0)) {
                    pt.insert(insertPos, "\n");
                    batch.ops.push_back({EditOp::Insert, insertPos, "\n"});
                    swapLineLen++;
                    insertPos++;
                    movingText.pop_back();
                    moveLen--;
                }
                
                pt.insert(insertPos, movingText);
                batch.ops.push_back({EditOp::Insert, insertPos, movingText});
                
                for (auto& c : cursors) {
                    // 【修正】Block A（移動する行）に含まれるかどうかの判定
                    bool headInA = (c.head >= posStart && c.head < posEnd);
                    if (c.head == posEnd && c.hasSelection() && c.end() == c.head) headInA = true;
                    
                    bool anchorInA = (c.anchor >= posStart && c.anchor < posEnd);
                    if (c.anchor == posEnd && c.hasSelection() && c.end() == c.anchor) anchorInA = true;
                    
                    // Headの移動
                    if (headInA) {
                        c.head += swapLineLen;
                    } else if (c.head >= posEnd && c.head < posSwapEnd) {
                        c.head -= moveLen; // 入れ替わりで上がる行
                    }
                    
                    // Anchorの移動
                    if (anchorInA) {
                        c.anchor += swapLineLen;
                    } else if (c.anchor >= posEnd && c.anchor < posSwapEnd) {
                        c.anchor -= moveLen;
                    }
                    
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
    }
    
    void copyLines(bool up) {
        std::vector<int> lines = getUniqueLineIndices();
        if (lines.empty()) return;
        
        size_t len = pt.length();
        if (len > 0 && pt.charAt(len-1) != '\n') {
            pt.insert(len, "\n");
            rebuildLineStarts();
        }
        
        EditBatch batch;
        batch.beforeCursors = cursors;
        
        std::vector<std::pair<int, int>> groups;
        int start = lines[0];
        int prev = lines[0];
        for (size_t i = 1; i < lines.size(); ++i) {
            if (lines[i] != prev + 1) {
                groups.push_back({start, prev});
                start = lines[i];
            }
            prev = lines[i];
        }
        groups.push_back({start, prev});
        
        if (up) {
            // 上にコピー: 複製された新しい行(上側)にカーソルを移動
            for (int i = (int)groups.size() - 1; i >= 0; --i) {
                int gStart = groups[i].first;
                int gEnd = groups[i].second;
                
                size_t posStart = lineStarts[gStart];
                size_t posEnd = (gEnd + 1 < (int)lineStarts.size()) ? lineStarts[gEnd + 1] : pt.length();
                std::string text = pt.getRange(posStart, posEnd - posStart);
                
                pt.insert(posStart, text);
                batch.ops.push_back({EditOp::Insert, posStart, text});
                size_t insLen = text.length();
                
                for (auto& c : cursors) {
                    // 全カーソルを物理的にずらす
                    if (c.head >= posStart) c.head += insLen;
                    if (c.anchor >= posStart) c.anchor += insLen;
                    
                    // コピー対象だったカーソルを、新しくできた上のブロックへ戻す
                    // (元々 [posStart, posEnd) にあったカーソルは、今 [posStart+insLen, posEnd+insLen) にある)
                    if (c.head >= posStart + insLen && c.head < posEnd + insLen) c.head -= insLen;
                    if (c.anchor >= posStart + insLen && c.anchor < posEnd + insLen) c.anchor -= insLen;
                    
                    c.desiredX = getXFromPos(c.head);
                    c.isVirtual = false;
                }
                rebuildLineStarts();
            }
        } else {
            // 下にコピー: 複製された新しい行(下側)にカーソルを移動
            for (int i = (int)groups.size() - 1; i >= 0; --i) {
                int gStart = groups[i].first;
                int gEnd = groups[i].second;
                
                size_t posStart = lineStarts[gStart];
                size_t posEnd = (gEnd + 1 < (int)lineStarts.size()) ? lineStarts[gEnd + 1] : pt.length();
                std::string text = pt.getRange(posStart, posEnd - posStart);
                
                pt.insert(posEnd, text);
                batch.ops.push_back({EditOp::Insert, posEnd, text});
                size_t insLen = text.length();
                
                for (auto& c : cursors) {
                    // 下にあるカーソルはずらす
                    if (c.head >= posEnd) c.head += insLen;
                    if (c.anchor >= posEnd) c.anchor += insLen;
                    
                    // コピー対象だったカーソルを、新しくできた下のブロックへ進める
                    if (c.head >= posStart && c.head < posEnd) c.head += insLen;
                    if (c.anchor >= posStart && c.anchor < posEnd) c.anchor += insLen;
                    
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
    }
    
    // --- 表示系ヘルパー ---
    void updateGutterWidth() {
        int totalLines = (int)lineStarts.size();
        int digits = 1;
        int tempLines = totalLines;
        while (tempLines >= 10) { tempLines /= 10; digits++; }
        gutterWidth = (float)(digits * charWidth) + (charWidth * 1.5f);
    }
    
    void updateMaxLineWidth() {
        if (!fontRef) return;
        maxLineWidth = 0.0f;
        for (int i = 0; i < (int)lineStarts.size(); ++i) {
            float w = getXInLine(i, (i + 1 < (int)lineStarts.size() ? lineStarts[i + 1] : pt.length()));
            if (w > maxLineWidth) maxLineWidth = w;
        }
        maxLineWidth += charWidth * 2.0f;
    }
    
    std::pair<std::string, bool> getHighlightTarget() {
        if (cursors.empty()) return { "", false };
        if (cursors.size() > 1) return { "", false };
        const Cursor& c = cursors.back();
        if (c.hasSelection()) {
            size_t len = c.end() - c.start();
            if (len == 0 || len > 200) return { "", false };
            std::string s = pt.getRange(c.start(), len);
            if (s.empty() || s.find('\n') != std::string::npos) return { "", false };
            return { s, false };
        }
        size_t pos = c.head;
        size_t len = pt.length();
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
    
    size_t findText(size_t startPos, const std::string& query, bool forward) {
        if (query.empty()) return std::string::npos;
        size_t len = pt.length();
        size_t cur = startPos;
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
        Cursor& lastC = cursors.back();
        if (!lastC.hasSelection()) {
            size_t s, e; getWordBoundaries(lastC.head, s, e);
            if (s != e) { lastC.anchor = s; lastC.head = e; lastC.desiredX = getXFromPos(e); lastC.isVirtual=false; }
            return;
        }
        size_t start = lastC.start(), len = lastC.end() - start;
        std::string query = pt.getRange(start, len);
        size_t nextPos = findText(lastC.end(), query, true);
        if (nextPos != std::string::npos) {
            for (const auto& c : cursors) if (c.start() == nextPos) return;
            size_t newHead = nextPos + len;
            cursors.push_back({newHead, nextPos, getXFromPos(newHead), getXFromPos(newHead), false});
            ensureCaretVisible();
        }
    }
    
    void updateTitleBar() {
        std::wstring t = (isDirty ? L"*" : L"") + (currentFilePath.empty() ? L"Untitled" : currentFilePath.substr(currentFilePath.find_last_of(L"/")+1)) + L" - " + APP_TITLE;
        [[(NSView*)view window] setTitle:[NSString stringWithUTF8String:WToUTF8(t).c_str()]]; [[(NSView*)view window] setDocumentEdited:isDirty];
    }
    void updateScrollBars() { [(EditorView*)view updateScrollers]; }
    void updateDirtyFlag() { bool nd = undo.isModified(); if (isDirty != nd) { isDirty = nd; updateTitleBar(); } }
    
    void updateFont(float s) {
        // 1. 変更前の文字幅を保存しておく
        float oldCharWidth = charWidth;
        
        s = std::clamp(s, 6.0f, 200.0f);
        if (fontRef) CFRelease(fontRef);
        fontRef = CTFontCreateWithName(CFSTR("Menlo"), s, NULL);
        currentFontSize = s;
        lineHeight = std::ceil(s * 1.4f);
        
        UniChar c = '0'; CGGlyph g; CGSize adv;
        CTFontGetGlyphsForCharacters(fontRef, &c, &g, 1);
        CTFontGetAdvancesForGlyphs(fontRef, kCTFontOrientationHorizontal, &g, &adv, 1);
        charWidth = adv.width;
        
        // 2. カーソルの仮想座標を新しい文字幅に合わせてスケーリング補正
        if (oldCharWidth > 0.0f && charWidth > 0.0f) {
            float ratio = charWidth / oldCharWidth;
            for (auto& cur : cursors) {
                cur.desiredX *= ratio;
                cur.originalAnchorX *= ratio;
            }
        }
        
        updateGutterWidth();
        updateMaxLineWidth();
        if (view) updateScrollBars();
    }
    void rebuildLineStarts() {
        lineStarts.clear(); lineStarts.push_back(0);
        size_t go = 0;
        for (const auto& p : pt.pieces) {
            const char* b = p.isOriginal ? (pt.origPtr + p.start) : (pt.addBuf.data() + p.start);
            for (size_t i = 0; i < p.len; ++i) if (b[i] == '\n') lineStarts.push_back(go + i + 1);
            go += p.len;
        }
        updateGutterWidth(); if (fontRef) updateMaxLineWidth();
        if (view) [view updateScrollers];
    }
    
    int getLineIdx(size_t pos) { auto it = std::upper_bound(lineStarts.begin(), lineStarts.end(), pos); return std::max(0, (int)std::distance(lineStarts.begin(), it) - 1); }
    float getXInLine(int li, size_t pos) {
        if (li < 0 || li >= (int)lineStarts.size()) return 0.0f;
        size_t s = lineStarts[li], e = (li + 1 < (int)lineStarts.size()) ? lineStarts[li + 1] : pt.length();
        if (e > s && pt.charAt(e-1) == '\n') e--;
        std::string lstr = pt.getRange(s, e - s); size_t rp = std::clamp(pos, s, e) - s;
        if (!imeComp.empty() && !cursors.empty() && getLineIdx(cursors.back().head) == li) {
            size_t cp = cursors.back().head; if (cp >= s && cp <= e) { lstr.insert(cp - s, imeComp); if (pos >= cp) rp += imeComp.size(); }
        }
        if (lstr.empty()) return 0.0f;
        CFStringRef cf = CFStringCreateWithBytes(NULL, (const UInt8*)lstr.data(), lstr.size(), kCFStringEncodingUTF8, false);
        if (!cf) return 0.0f; // 【修正】不正なUTF-8等で生成失敗した場合はクラッシュ回避
        
        const void* k[] = { kCTFontAttributeName }; const void* v[] = { fontRef }; CFDictionaryRef d = CFDictionaryCreate(NULL, k, v, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFAttributedStringRef as = CFAttributedStringCreate(NULL, cf, d); CTLineRef line = CTLineCreateWithAttributedString(as);
        CFStringRef sub = CFStringCreateWithBytes(NULL, (const UInt8*)lstr.data(), rp, kCFStringEncodingUTF8, false);
        CGFloat x = CTLineGetOffsetForStringIndex(line, sub ? CFStringGetLength(sub) : 0, NULL);
        if (sub) CFRelease(sub); CFRelease(line); CFRelease(as); CFRelease(d); CFRelease(cf);
        return (float)x;
    }
    
    float getXFromPos(size_t p) { return getXInLine(getLineIdx(p), p); }
    size_t getPosFromLineAndX(int li, float tx) {
        if (li < 0 || li >= (int)lineStarts.size()) return cursors.empty() ? 0 : cursors.back().head;
        size_t s = lineStarts[li], e = (li + 1 < (int)lineStarts.size()) ? lineStarts[li + 1] : pt.length();
        if (e > s && pt.charAt(e-1) == '\n') e--;
        std::string lstr = pt.getRange(s, e - s); if (lstr.empty()) return s;
        
        CFStringRef cf = CFStringCreateWithBytes(NULL, (const UInt8*)lstr.data(), lstr.size(), kCFStringEncodingUTF8, false);
        if (!cf) return s; // 【修正】生成失敗時は行頭を返す
        
        const void* k[]={kCTFontAttributeName}; const void* v[]={fontRef}; CFDictionaryRef d = CFDictionaryCreate(NULL, k, v, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFAttributedStringRef as = CFAttributedStringCreate(NULL, cf, d); CTLineRef line = CTLineCreateWithAttributedString(as);
        CFIndex ci = CTLineGetStringIndexForPosition(line, CGPointMake(tx, 0));
        CFIndex bytes = 0; if (ci > 0) {
            CFStringRef sub = CFStringCreateWithSubstring(NULL, cf, CFRangeMake(0, std::min(ci, CFStringGetLength(cf))));
            if (sub) { // sub生成チェック
                CFStringGetBytes(sub, CFRangeMake(0, CFStringGetLength(sub)), kCFStringEncodingUTF8, 0, false, NULL, 0, &bytes);
                CFRelease(sub);
            }
        }
        CFRelease(line); CFRelease(as); CFRelease(d); CFRelease(cf); return s + bytes;
    }
    
    size_t getDocPosFromPoint(float x, float y) {
        float absY = y + (float)vScrollPos * lineHeight;
        int li = std::clamp((int)std::floor(absY / lineHeight), 0, (int)lineStarts.size() - 1);
        return getPosFromLineAndX(li, x - gutterWidth + (float)hScrollPos);
    }
    void ensureCaretVisible() {
        if (cursors.empty() || !view) return; Cursor& c = cursors.back(); NSRect b = [(NSView*)view bounds]; CGFloat sw = [NSScroller scrollerWidthForControlSize:NSControlSizeRegular scrollerStyle:NSScrollerStyleLegacy];
        float ch = b.size.height - sw, cw = b.size.width - gutterWidth - sw; int li = getLineIdx(c.head), vis = (int)(ch/lineHeight);
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
        for(auto& c:sorted){ size_t st=c.start(), l=c.end()-st; if(l>0){ b.ops.push_back({EditOp::Erase, st, pt.getRange(st,l)}); pt.erase(st,l); for(auto& o : cursors){ if(o.head>st)o.head-=l; if(o.anchor>st)o.anchor-=l; } } pt.insert(st,t); b.ops.push_back({EditOp::Insert, st, t}); for(auto& o : cursors){ if(o.head>=st)o.head+=(size_t)t.size(); if(o.anchor>=st)o.anchor+=(size_t)t.size(); } c.desiredX=getXFromPos(c.head); c.isVirtual=false; }
        b.afterCursors=cursors; undo.push(b); rebuildLineStarts(); ensureCaretVisible(); updateDirtyFlag();
    }
    
    void backspaceAtCursors() {
        if (cursors.empty()) return;
        EditBatch b;
        b.beforeCursors = cursors;
        
        // 削除処理は後ろのカーソルから行うとインデックスずれが起きにくい
        // ただし、マルチカーソル(矩形)の場合はすべてのカーソルを維持する必要がある
        std::vector<size_t> indices(cursors.size());
        std::iota(indices.begin(), indices.end(), 0);
        
        // 物理位置(head)の降順でソート（後ろから削除するため）
        std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
            return cursors[a].head > cursors[b].head;
        });
        
        bool changed = false;
        
        // 削除処理ループ
        for (size_t idx : indices) {
            Cursor& c = cursors[idx];
            
            // -------------------------------------------------
            // Case A: 範囲選択中 (矩形選択含む)
            // -------------------------------------------------
            if (c.hasSelection()) {
                size_t st = c.start();
                size_t len = c.end() - st;
                
                b.ops.push_back({EditOp::Erase, st, pt.getRange(st, len)});
                pt.erase(st, len);
                changed = true;
                
                // 他のカーソルの位置調整
                for (auto& o : cursors) {
                    // 自分自身(c)は後で更新するのでスキップしないとバグる可能性があるが、
                    // ここではインデックスアクセスしているので o は c を含む
                    if (&o == &c) continue;
                    
                    if (o.head > st) {
                        if (o.head >= st + len) o.head -= len;
                        else o.head = st;
                    }
                    if (o.anchor > st) {
                        if (o.anchor >= st + len) o.anchor -= len;
                        else o.anchor = st;
                    }
                }
                
                c.head = st;
                c.anchor = st;
                c.desiredX = getXFromPos(c.head);
                c.originalAnchorX = c.desiredX;
                c.isVirtual = false;
                continue;
            }
            
            // -------------------------------------------------
            // Case B: 仮想スペース (文字がない場所)
            // -------------------------------------------------
            // カーソルが仮想モード、かつX座標が行末より右にある場合
            int li = getLineIdx(c.head);
            float physX = getXInLine(li, c.head);
            
            if (c.isVirtual && c.desiredX > physX + 0.1f) {
                // 文字は消さずに、カーソルを左（文字幅分）に移動するだけ
                c.desiredX = std::max(physX, c.desiredX - charWidth);
                c.originalAnchorX = c.desiredX; // 選択範囲もリセット
                
                // もし物理位置まで戻ったら仮想モード終了
                if (c.desiredX <= physX + 0.1f) {
                    c.desiredX = physX;
                    c.isVirtual = false;
                }
                // 画面更新のためにchangedフラグは立てるが、テキスト変更はない
                // (undoスタックには積まない、あるいはダミーを積む)
                continue;
            }
            
            // -------------------------------------------------
            // Case C: 通常の文字削除 (左の文字を消す)
            // -------------------------------------------------
            if (c.head > 0) {
                size_t st = c.head;
                size_t prev = moveCaretVisual(st, false);
                size_t len = st - prev;
                
                if (len > 0) {
                    b.ops.push_back({EditOp::Erase, prev, pt.getRange(prev, len)});
                    pt.erase(prev, len);
                    changed = true;
                    
                    // 全カーソルの位置調整
                    for (auto& o : cursors) {
                        if (&o == &c) continue;
                        
                        if (o.head >= st) o.head -= len;
                        else if (o.head > prev) o.head = prev;
                        
                        if (o.anchor >= st) o.anchor -= len;
                        else if (o.anchor > prev) o.anchor = prev;
                    }
                    
                    c.head = prev;
                    c.anchor = prev;
                    c.desiredX = getXFromPos(c.head);
                    c.originalAnchorX = c.desiredX;
                    c.isVirtual = false;
                }
            }
        }
        
        if (changed) {
            b.afterCursors = cursors;
            undo.push(b);
            rebuildLineStarts();
            updateDirtyFlag();
        }
        // 仮想移動のみの場合もカーソル表示更新のため呼び出す
        ensureCaretVisible();
        if (view) [view setNeedsDisplay:YES];
    }
    void deleteForwardAtCursors() {
        if (cursors.empty()) return;
        EditBatch b;
        b.beforeCursors = cursors;
        
        std::vector<size_t> indices(cursors.size());
        std::iota(indices.begin(), indices.end(), 0);
        std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
            return cursors[a].start() > cursors[b].start();
        });
        
        bool changed = false;
        for (size_t idx : indices) {
            Cursor& c = cursors[idx];
            size_t st = c.start();
            size_t len = 0;
            
            if (c.hasSelection()) {
                len = c.end() - st;
            } else {
                size_t next = moveCaretVisual(st, true);
                len = next - st;
            }
            
            if (len > 0) {
                b.ops.push_back({EditOp::Erase, st, pt.getRange(st, len)});
                pt.erase(st, len);
                changed = true;
                
                for (auto& o : cursors) {
                    if (o.head > st + len) o.head -= len;
                    else if (o.head > st) o.head = st;
                    
                    if (o.anchor > st + len) o.anchor -= len;
                    else if (o.anchor > st) o.anchor = st;
                }
                
                c.head = st;
                c.anchor = st;
            }
            
            // 【重要】削除後は選択範囲を解除し、仮想座標もリセットする
            c.desiredX = getXFromPos(c.head);
            c.originalAnchorX = c.desiredX;
            c.isVirtual = false;
        }
        
        if (changed) {
            b.afterCursors = cursors;
            undo.push(b);
            rebuildLineStarts();
            ensureCaretVisible();
            updateDirtyFlag();
        }
    }
    
    void selectAll() { cursors.clear(); size_t len = pt.length(); cursors.push_back({len, 0, getXFromPos(len), getXFromPos(len), false}); }
    
    // --- コピー・切り取り・貼り付け ---
    void copyToClipboard() {
        bool hasSelection = false;
        for (const auto& c : cursors) if (c.hasSelection()) { hasSelection = true; break; }
        std::string t; bool isRect = (cursors.size() > 1);
        if (!hasSelection) {
            std::vector<int> lines = getUniqueLineIndices();
            for (int li : lines) {
                size_t start = lineStarts[li];
                size_t end = (li + 1 < (int)lineStarts.size()) ? lineStarts[li + 1] : pt.length();
                t += pt.getRange(start, end - start);
                if (t.empty() || t.back() != '\n') t += "\n";
            }
        } else {
            auto sorted = cursors;
            std::sort(sorted.begin(), sorted.end(), [](const Cursor& a, const Cursor& b){ return a.start() < b.start(); });
            for(size_t i=0; i<sorted.size(); ++i) {
                if(sorted[i].hasSelection()) {
                    t += pt.getRange(sorted[i].start(), sorted[i].end() - sorted[i].start());
                    if(isRect && i < sorted.size() - 1) t += "\n";
                }
            }
        }
        if(!t.empty()) {
            NSPasteboard *pb = [NSPasteboard generalPasteboard];
            [pb clearContents];
            [pb setString:[NSString stringWithUTF8String:t.c_str()] forType:NSPasteboardTypeString];
            if(hasSelection && isRect) [pb setData:[NSData data] forType:kMiuRectangularSelectionType];
        }
    }
    void cutToClipboard() {
        bool hasSelection = false; for (const auto& c : cursors) if (c.hasSelection()) { hasSelection = true; break; }
        copyToClipboard();
        if (!hasSelection) deleteLine(); else insertAtCursors("");
    }
    void pasteFromClipboard() {
        NSPasteboard *pb = [NSPasteboard generalPasteboard];
        NSString *s = [pb stringForType:NSPasteboardTypeString];
        if(!s) return;
        std::string utf8 = [s UTF8String];
        bool isRectMarker = [pb dataForType:kMiuRectangularSelectionType] != nil;
        if (isRectMarker) {
            if (cursors.size() <= 1) {
                size_t basePos = cursors.empty() ? 0 : cursors[0].head;
                float baseX = getXFromPos(basePos);
                int startLine = getLineIdx(basePos);
                int lineCount = 1; for(char c : utf8) if(c == '\n') lineCount++;
                cursors.clear();
                for(int i=0; i < lineCount; ++i) {
                    int targetLine = startLine + i;
                    if(targetLine < (int)lineStarts.size()) {
                        size_t p = getPosFromLineAndX(targetLine, baseX);
                        cursors.push_back({p, p, baseX, baseX, false});
                    }
                }
            }
            insertRectangularBlock(utf8);
        } else {
            insertAtCursors(utf8);
        }
        if (view) [view setNeedsDisplay:YES];
    }
    void performUndo() { if(!undo.undoStack.empty()){ auto b = undo.popUndo(); for(int i=(int)b.ops.size()-1;i>=0;--i){ if(b.ops[i].type==EditOp::Insert) pt.erase(b.ops[i].pos, (int)b.ops[i].text.size()); else pt.insert(b.ops[i].pos, b.ops[i].text); } cursors=b.beforeCursors; rebuildLineStarts(); ensureCaretVisible(); updateDirtyFlag(); } }
    void performRedo() { if(!undo.redoStack.empty()){ auto b = undo.popRedo(); for(const auto& o:b.ops){ if(o.type==EditOp::Insert) pt.insert(o.pos, o.text); else pt.erase(o.pos, (int)o.text.size()); } cursors=b.afterCursors; rebuildLineStarts(); ensureCaretVisible(); updateDirtyFlag(); } }
    
    // --- ファイル操作 ---
    bool checkUnsavedChanges() { if(!isDirty) return true; NSAlert *a = [NSAlert new]; [a setMessageText:@"Save changes?"]; [a addButtonWithTitle:@"Save"]; [a addButtonWithTitle:@"Cancel"]; [a addButtonWithTitle:@"Discard"]; NSModalResponse r = [a runModal]; if(r==NSAlertFirstButtonReturn) return currentFilePath.empty()?saveFileAs():saveFile(currentFilePath); if(r==NSAlertThirdButtonReturn){ isDirty=false; updateTitleBar(); return true; } return false; }
    bool saveFile(const std::wstring& p) { std::string s = pt.getRange(0,pt.length()); std::ofstream f(WToUTF8(p), std::ios::binary); if(!f) return false; f.write(s.data(), s.size()); f.close(); currentFilePath=p; undo.markSaved(); isDirty=false; updateTitleBar(); return true; }
    bool saveFileAs() { NSSavePanel *p = [NSSavePanel savePanel]; if([p runModal]==NSModalResponseOK) return saveFile(UTF8ToW([p.URL.path UTF8String])); return false; }
    bool openFileFromPath(const std::string& p) { fileMap.reset(new MappedFile()); if(fileMap->open(p.c_str())){ currentEncoding=DetectEncoding(fileMap->ptr,fileMap->size); const char* ptr=fileMap->ptr; size_t sz=fileMap->size; std::string conv = ""; if(currentEncoding==ENC_UTF16LE) conv=Utf16ToUtf8(ptr,sz,false); else if(currentEncoding == ENC_UTF16BE) conv=Utf16ToUtf8(ptr,sz,true); else if(currentEncoding == ENC_ANSI) conv=AnsiToUtf8(ptr,sz); else if(currentEncoding == ENC_UTF8_BOM){ptr+=3;sz-=3;} if(!conv.empty()) pt.initFromFile(conv.data(),conv.size()); else pt.initFromFile(ptr, sz);
        currentFilePath = UTF8ToW(p); undo.clear(); isDirty = false; vScrollPos = 0; hScrollPos = 0;
        cursors.clear(); cursors.push_back({0,0,0.0f,0.0f,false});
        rebuildLineStarts(); updateTitleBar(); if (view) { [view updateScrollers]; [view setNeedsDisplay:YES]; }
        return true;
    } return false; }
    bool openFile() { if(!checkUnsavedChanges())return false; NSOpenPanel *p = [NSOpenPanel openPanel]; if([p runModal]==NSModalResponseOK) return openFileFromPath([[[p URLs] objectAtIndex:0].path UTF8String]); return false; }
    void newFile() { if(checkUnsavedChanges()){ pt.initEmpty(); currentFilePath.clear(); undo.clear(); isDirty=false; cursors.clear(); cursors.push_back({0,0,0.0f,0.0f,false}); rebuildLineStarts(); updateTitleBar(); } }
    
    // 単語判定
    bool isWordChar(char c) { if (isalnum((unsigned char)c) || c == '_') return true; if ((unsigned char)c >= 0x80) return true; return false; }
    void getWordBoundaries(size_t pos, size_t& start, size_t& end) {
        size_t len = pt.length(); if (len == 0) { start = end = 0; return; }
        pos = std::min(pos, len); start = end = pos;
        bool charRight = (pos < len && isWordChar(pt.charAt(pos))); bool charLeft = (pos > 0 && isWordChar(pt.charAt(pos - 1)));
        if (charRight || charLeft) { if (!charRight && charLeft) { start--; end--; } while (start > 0 && isWordChar(pt.charAt(start - 1))) start--; while (end < len && isWordChar(pt.charAt(end))) end++; }
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
    
    // --- 描画処理 ---
    void render(CGContextRef ctx, float w, float h, float ss) {
        CGContextSetFillColorWithColor(ctx, colBackground); CGContextFillRect(ctx, CGRectMake(0, 0, w, h));
        CGContextSetTextMatrix(ctx, CGAffineTransformMakeScale(1.0, -1.0));
        float vw = std::max(0.0f, w - gutterWidth - ss); float vh = std::max(0.0f, h - ss);
        int start = vScrollPos; int vis = (int)(vh / lineHeight) + 2; int end = std::min((int)lineStarts.size(), start + vis);
        CGFloat asc = CTFontGetAscent(fontRef);
        
        CGContextSaveGState(ctx); CGContextClipToRect(ctx, CGRectMake(gutterWidth, 0, vw, vh));
        
        // 自動ハイライト
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
        
        // 選択範囲 (通常/矩形書き分け)
        CGContextSetFillColorWithColor(ctx, colSel);
        bool isRectMode = (cursors.size() > 1);
        
        for (const auto& c : cursors) {
            // 【重要】ここで hasSelection() をチェックしてはいけない
            // 矩形選択では文字を選択していなくても、空間を選択している場合があるため
            
            if (isRectMode) {
                // --- A. 矩形・マルチカーソルモード ---
                // 座標ベースで描画判定を行う
                float visualX1 = std::min(c.desiredX, c.originalAnchorX);
                float visualX2 = std::max(c.desiredX, c.originalAnchorX);
                
                // 幅がない場合は描画しない
                if (visualX2 - visualX1 < 0.5f) continue;
                
                int lHead = getLineIdx(c.head);
                int lAnchor = getLineIdx(c.anchor);
                int startL = std::min(lHead, lAnchor);
                int endL = std::max(lHead, lAnchor);
                
                for (int l = startL; l <= endL; ++l) {
                    if (l < start || l >= end) continue;
                    
                    float y = (float)(l - start) * lineHeight;
                    float drawX = gutterWidth - (float)hScrollPos + visualX1;
                    float drawW = visualX2 - visualX1;
                    
                    CGContextFillRect(ctx, CGRectMake(drawX, y, drawW, lineHeight));
                }
            } else {
                // --- B. 通常選択モード ---
                // こちらは文字選択がない場合は描画しない
                if (!c.hasSelection()) continue;
                
                size_t pStart = c.start();
                size_t pEnd = c.end();
                int lStart = getLineIdx(pStart);
                int lEnd = getLineIdx(pEnd);
                
                for (int l = lStart; l <= lEnd; ++l) {
                    if (l < start || l >= end) continue;
                    
                    float y = (float)(l - start) * lineHeight;
                    size_t lineBegin = lineStarts[l];
                    size_t lineEnd = (l + 1 < (int)lineStarts.size() ? lineStarts[l + 1] : pt.length());
                    
                    size_t selS = std::max(pStart, lineBegin);
                    size_t selE = std::min(pEnd, lineEnd);
                    
                    float x1 = getXInLine(l, selS);
                    float x2 = getXInLine(l, selE);
                    
                    // 改行選択時の視覚補正
                    if (selE == lineEnd && pEnd > lineEnd) {
                        x2 += (charWidth * 0.5f);
                    }
                    
                    if (x2 > x1) {
                        CGContextFillRect(ctx, CGRectMake(gutterWidth - (float)hScrollPos + x1, y, x2 - x1, lineHeight));
                    }
                }
            }
        }
        // テキスト
        for (int i = start; i < end; ++i) {
            size_t s = lineStarts[i], e = (i + 1 < lineStarts.size() ? lineStarts[i + 1] : pt.length());
            std::string ls = pt.getRange(s, std::max((size_t)0, e - s)); size_t imIdx = std::string::npos;
            if (!imeComp.empty() && !cursors.empty() && getLineIdx(cursors.back().head) == i) {
                size_t cp = cursors.back().head; if (cp >= s && cp <= e) { imIdx = cp - s; ls.insert(imIdx, imeComp); }
            }
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
        }
        
        // カーソル (仮想スペース対応)
        CGContextSetFillColorWithColor(ctx, colCaret);
        for (const auto& c : cursors) {
            int l = getLineIdx(c.head);
            if (l >= start && l < end) {
                float physX = getXInLine(l, c.head);
                float drawX = physX;
                if (c.isVirtual) drawX = std::max(physX, c.desiredX);
                CGContextFillRect(ctx, CGRectMake(gutterWidth - (float)hScrollPos + drawX, (float)(l - start) * lineHeight, 2, lineHeight));
            }
        }
        CGContextRestoreGState(ctx);
        
        // ガーター
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
        
        // ポップアップ描画
        auto now = std::chrono::steady_clock::now();
        if (now < zoomPopupEndTime) {
            float zw = 100, zh = 40; CGRect zpr = CGRectMake((w - zw) / 2, (h - zh) / 2, zw, zh);
            CGContextSetFillColorWithColor(ctx, CGColorCreateGenericRGB(0.1, 0.1, 0.1, 0.7));
            CGPathRef path = CGPathCreateWithRoundedRect(zpr, 8, 8, NULL); CGContextAddPath(ctx, path); CGContextFillPath(ctx); CGPathRelease(path);
            CFStringRef zcf = CFStringCreateWithCString(NULL, zoomPopupText.c_str(), kCFStringEncodingUTF8);
            CTFontRef zf = CTFontCreateWithName(CFSTR("Helvetica-Bold"), 16.0f, NULL);
            CGColorRef white = CGColorCreateGenericRGB(1, 1, 1, 1);
            NSDictionary *za = @{ (id)kCTFontAttributeName: (__bridge id)zf, (id)kCTForegroundColorAttributeName: (__bridge id)white };
            NSAttributedString *zas = [[NSAttributedString alloc] initWithString:(__bridge NSString*)zcf attributes:za];
            CTLineRef zl = CTLineCreateWithAttributedString((CFAttributedStringRef)zas); CGRect zb = CTLineGetImageBounds(zl, ctx);
            CGContextSetTextPosition(ctx, (w - zb.size.width) / 2, (h / 2) + 6); CTLineDraw(zl, ctx);
            CFRelease(zl); CFRelease(zf); CFRelease(zcf); CGColorRelease(white);
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{ [view setNeedsDisplay:YES]; });
        }
        
        if (showHelpPopup) {
            float hw = 400, hh = 300; CGRect hr = CGRectMake((w - hw) / 2, (h - hh) / 2, hw, hh);
            CGContextSetFillColorWithColor(ctx, CGColorCreateGenericRGB(0.1, 0.1, 0.1, 0.9));
            CGPathRef hpath = CGPathCreateWithRoundedRect(hr, 12, 12, NULL); CGContextAddPath(ctx, hpath); CGContextFillPath(ctx); CGPathRelease(hpath);
            CFStringRef hcf = CFStringCreateWithBytes(NULL, (const UInt8*)helpTextStr.data(), helpTextStr.size()*sizeof(wchar_t), kCFStringEncodingUTF32LE, false);
            CTFontRef hf = CTFontCreateWithName(CFSTR("Menlo"), 14.0f, NULL);
            NSDictionary *ha = @{(id)kCTFontAttributeName: (__bridge id)hf, (id)kCTForegroundColorAttributeName: (__bridge id)CGColorCreateGenericRGB(1,1,1,1)};
            NSAttributedString *has = [[NSAttributedString alloc] initWithString:(__bridge NSString*)hcf attributes:ha];
            CTFramesetterRef fs = CTFramesetterCreateWithAttributedString((CFAttributedStringRef)has);
            CGMutablePathRef pth = CGPathCreateMutable(); CGPathAddRect(pth, NULL, CGRectInset(hr, 20, 20));
            CTFrameRef frame = CTFramesetterCreateFrame(fs, CFRangeMake(0, 0), pth, NULL); CTFrameDraw(frame, ctx);
            CFRelease(frame); CFRelease(pth); CFRelease(fs); CFRelease(hf); CFRelease(hcf);
        }
    }
};

Editor g_editor;

// --- 6. EditorView Implementation ---
@implementation EditorView {
    NSScroller *vScroller, *hScroller;
    NSPoint boxSelectStartPoint;
    size_t cursorsSnapshotCount;
}

- (instancetype)initWithFrame:(NSRect)fr {
    if (self = [super initWithFrame:fr]) {
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
- (void)registerForDraggedTypes { [self registerForDraggedTypes:@[NSPasteboardTypeFileURL]]; }
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
            if (!g_editor.checkUnsavedChanges()) return NO;
            if (g_editor.openFileFromPath([[[urls objectAtIndex:0] path] UTF8String])) { [self setNeedsDisplay:YES]; return YES; }
        }
    }
    return NO;
}
- (BOOL)acceptsFirstResponder { return YES; }
- (BOOL)becomeFirstResponder { return YES; }
- (BOOL)isFlipped { return YES; }
- (void)viewDidMoveToWindow {
    g_editor.view = self; g_editor.initGraphics();
    NSString *m = [[NSUserDefaults standardUserDefaults] stringForKey:@"AppleInterfaceStyle"];
    g_editor.isDarkMode = (m && [m isEqualToString:@"Dark"]);
    g_editor.updateThemeColors(); [self updateScrollers];
    [[self window] makeFirstResponder:self];
}
- (void)drawRect:(NSRect)r { CGFloat sw = [NSScroller scrollerWidthForControlSize:NSControlSizeRegular scrollerStyle:NSScrollerStyleLegacy]; g_editor.render([[NSGraphicsContext currentContext] CGContext], (float)self.bounds.size.width, (float)self.bounds.size.height, (float)sw); }
- (void)updateScrollers {
    NSRect b = [self bounds];
    CGFloat sw = [NSScroller scrollerWidthForControlSize:NSControlSizeRegular scrollerStyle:NSScrollerStyleLegacy];
    float visibleHeight = b.size.height - sw;
    int visibleLines = (int)std::floor(visibleHeight / g_editor.lineHeight); if (visibleLines < 1) visibleLines = 1;
    int totalLines = (int)g_editor.lineStarts.size();
    [vScroller setKnobProportion:std::min(1.0, (double)visibleLines / totalLines)];
    int maxV = totalLines - visibleLines;
    if (maxV > 0) { [vScroller setDoubleValue:(double)g_editor.vScrollPos / maxV]; [vScroller setEnabled:YES]; } else { [vScroller setDoubleValue:0.0]; [vScroller setEnabled:NO]; }
    float visibleWidth = b.size.width - g_editor.gutterWidth - sw; if (visibleWidth < 1) visibleWidth = 1;
    [hScroller setKnobProportion:std::min(1.0, (double)visibleWidth / g_editor.maxLineWidth)];
    float maxH = g_editor.maxLineWidth - visibleWidth;
    if (maxH > 0) { [hScroller setDoubleValue:(double)g_editor.hScrollPos / maxH]; [hScroller setEnabled:YES]; } else { [hScroller setDoubleValue:0.0]; [hScroller setEnabled:NO]; }
    [self setNeedsDisplay:YES];
}
- (void)scrollAction:(NSScroller*)s {
    NSRect b = [self bounds]; CGFloat sw = [NSScroller scrollerWidthForControlSize:NSControlSizeRegular scrollerStyle:NSScrollerStyleLegacy];
    if (s == vScroller) {
        int visibleLines = (int)std::floor((b.size.height - sw) / g_editor.lineHeight);
        int maxV = (int)g_editor.lineStarts.size() - visibleLines; g_editor.vScrollPos = std::max(0, (int)([s doubleValue] * maxV));
    } else {
        float visibleWidth = b.size.width - g_editor.gutterWidth - sw; float maxH = g_editor.maxLineWidth - visibleWidth;
        g_editor.hScrollPos = std::max(0, (int)([s doubleValue] * maxH));
    }
    [self setNeedsDisplay:YES];
}
- (void)applyZoom:(float)val relative:(bool)rel {
    g_editor.updateFont(rel ? g_editor.currentFontSize * val : val);
    g_editor.zoomPopupEndTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(1000);
    g_editor.zoomPopupText = std::to_string((int)std::round(g_editor.currentFontSize)) + "px";
    [self setNeedsDisplay:YES];
}

- (void)mouseDown:(NSEvent *)e {
    if (g_editor.showHelpPopup) { g_editor.showHelpPopup = false; [self setNeedsDisplay:YES]; }
    [[self window] makeFirstResponder:self];
    NSPoint p = [self convertPoint:[e locationInWindow] fromView:nil];
    CGFloat sw = [NSScroller scrollerWidthForControlSize:NSControlSizeRegular scrollerStyle:NSScrollerStyleLegacy];
    if (p.x > self.bounds.size.width - sw || p.y > self.bounds.size.height - sw) return;
    NSInteger clicks = [e clickCount];
    bool isOption = ([e modifierFlags] & NSEventModifierFlagOption);
    bool isShift = ([e modifierFlags] & NSEventModifierFlagShift);
    if (!isOption && !isShift) g_editor.cursors.clear();
    boxSelectStartPoint = p;
    cursorsSnapshotCount = g_editor.cursors.size();
    
    float docX = (float)p.x - g_editor.gutterWidth + (float)g_editor.hScrollPos;
    float snappedX = docX;
    if (g_editor.charWidth > 0) { int cols = (int)std::round(docX / g_editor.charWidth); if (cols < 0) cols = 0; snappedX = (float)cols * g_editor.charWidth; }
    size_t pos = g_editor.getDocPosFromPoint((float)p.x, (float)p.y);
    int li = g_editor.getLineIdx(pos);
    float physX = g_editor.getXInLine(li, pos);
    
    Cursor newCursor; newCursor.head = pos; newCursor.anchor = pos; newCursor.isVirtual = false;
    if (isOption && (snappedX > physX + (g_editor.charWidth * 0.5f))) { newCursor.desiredX = snappedX; newCursor.isVirtual = true; }
    else { newCursor.desiredX = physX; newCursor.isVirtual = false; }
    newCursor.originalAnchorX = newCursor.desiredX;
    
    if (clicks == 2) {
        size_t s, end; g_editor.getWordBoundaries(pos, s, end);
        newCursor.head = end; newCursor.anchor = s; newCursor.desiredX = g_editor.getXInLine(g_editor.getLineIdx(end), end); newCursor.originalAnchorX = newCursor.desiredX; newCursor.isVirtual = false;
        g_editor.cursors.push_back(newCursor);
    } else if (clicks >= 3) {
        size_t s = g_editor.lineStarts[li], end = (li + 1 < (int)g_editor.lineStarts.size()) ? g_editor.lineStarts[li+1] : g_editor.pt.length();
        newCursor.head = end; newCursor.anchor = s; newCursor.desiredX = g_editor.getXInLine(g_editor.getLineIdx(end), end); newCursor.originalAnchorX = newCursor.desiredX; newCursor.isVirtual = false;
        g_editor.cursors.push_back(newCursor);
    } else {
        g_editor.cursors.push_back(newCursor);
    }
    [self setNeedsDisplay:YES];
}

- (void)mouseDragged:(NSEvent *)e {
    NSPoint p = [self convertPoint:[e locationInWindow] fromView:nil];
    bool isOption = ([e modifierFlags] & NSEventModifierFlagOption);
    auto snapToGrid = [&](float rawX) -> float {
        if (g_editor.charWidth <= 0) return rawX; int cols = (int)std::round(rawX / g_editor.charWidth); if (cols < 0) cols = 0; return (float)cols * g_editor.charWidth;
    };
    if (isOption) {
        g_editor.cursors.clear();
        float startY = std::min((float)boxSelectStartPoint.y, (float)p.y), endY = std::max((float)boxSelectStartPoint.y, (float)p.y);
        int startLine = (int)std::floor((startY + (float)g_editor.vScrollPos * g_editor.lineHeight) / g_editor.lineHeight);
        int endLine = (int)std::floor((endY + (float)g_editor.vScrollPos * g_editor.lineHeight) / g_editor.lineHeight);
        startLine = std::clamp(startLine, 0, (int)g_editor.lineStarts.size() - 1); endLine = std::clamp(endLine, 0, (int)g_editor.lineStarts.size() - 1);
        float rawStartX = (float)boxSelectStartPoint.x - g_editor.gutterWidth + (float)g_editor.hScrollPos;
        float rawEndX = (float)p.x - g_editor.gutterWidth + (float)g_editor.hScrollPos;
        float gridStartX = snapToGrid(rawStartX), gridEndX = snapToGrid(rawEndX);
        for (int i = startLine; i <= endLine; ++i) {
            size_t pHead = g_editor.getPosFromLineAndX(i, gridEndX), pAnchor = g_editor.getPosFromLineAndX(i, gridStartX);
            Cursor c; c.head = pHead; c.anchor = pAnchor; c.desiredX = gridEndX; c.originalAnchorX = gridStartX; c.isVirtual = true;
            g_editor.cursors.push_back(c);
        }
    } else {
        size_t pos = g_editor.getDocPosFromPoint((float)p.x, (float)p.y);
        if (!g_editor.cursors.empty()) {
            Cursor& c = g_editor.cursors.back(); c.head = pos; int li = g_editor.getLineIdx(pos);
            c.desiredX = g_editor.getXInLine(li, pos); c.isVirtual = false;
        }
    }
    g_editor.ensureCaretVisible(); [self setNeedsDisplay:YES];
}
- (void)mouseUp:(NSEvent *)e { [self setNeedsDisplay:YES]; }

- (void)keyDown:(NSEvent *)e {
    unsigned short code = [e keyCode];
    NSString *chars = [e charactersIgnoringModifiers];
    bool cmd = ([e modifierFlags] & NSEventModifierFlagCommand);
    bool shift = ([e modifierFlags] & NSEventModifierFlagShift);
    if (g_editor.showHelpPopup) { g_editor.showHelpPopup = false; [self setNeedsDisplay:YES]; if (code == 122) return; }
    if (code == 122) { g_editor.showHelpPopup = true; [self setNeedsDisplay:YES]; return; }
    if (code == 53) {
        if (g_editor.cursors.size() > 1 || (g_editor.cursors.size() == 1 && g_editor.cursors[0].hasSelection())) {
            Cursor lastC = g_editor.cursors.back(); lastC.anchor = lastC.head; g_editor.cursors.clear(); g_editor.cursors.push_back(lastC); [self setNeedsDisplay:YES]; return;
        }
    }
    if (code == 115 || code == 119) {
        for (auto& c : g_editor.cursors) {
            if (code == 115) { if (cmd) c.head = 0; else c.head = g_editor.lineStarts[g_editor.getLineIdx(c.head)]; }
            else { if (cmd) c.head = g_editor.pt.length(); else { int li = g_editor.getLineIdx(c.head); size_t nextLineStart = (li + 1 < (int)g_editor.lineStarts.size()) ? g_editor.lineStarts[li + 1] : g_editor.pt.length(); if (nextLineStart > g_editor.lineStarts[li] && g_editor.pt.charAt(nextLineStart - 1) == '\n') { nextLineStart--; if (nextLineStart > g_editor.lineStarts[li] && g_editor.pt.charAt(nextLineStart - 1) == '\r') nextLineStart--; } c.head = nextLineStart; } }
            if (!shift) c.anchor = c.head; c.desiredX = g_editor.getXFromPos(c.head); c.isVirtual = false;
        }
        g_editor.ensureCaretVisible(); [self setNeedsDisplay:YES]; return;
    }
    if (code == 116 || code == 121) {
        CGFloat sw = [NSScroller scrollerWidthForControlSize:NSControlSizeRegular scrollerStyle:NSScrollerStyleLegacy];
        float viewHeight = [self bounds].size.height - sw; int pageLines = std::max(1, (int)(viewHeight / g_editor.lineHeight) - 1); int totalLines = (int)g_editor.lineStarts.size();
        for (auto& c : g_editor.cursors) {
            int currentLine = g_editor.getLineIdx(c.head); int newLine = currentLine;
            if (code == 116) { newLine = currentLine - pageLines; if (newLine < 0) newLine = 0; }
            else { newLine = currentLine + pageLines; if (newLine >= totalLines) newLine = totalLines - 1; }
            if (code == 121 && newLine == totalLines - 1 && currentLine == totalLines - 1) c.head = g_editor.pt.length();
            else c.head = g_editor.getPosFromLineAndX(newLine, c.desiredX);
            if (!shift) c.anchor = c.head; c.isVirtual = false;
        }
        if (code == 116) g_editor.vScrollPos = std::max(0, g_editor.vScrollPos - pageLines); else g_editor.vScrollPos = std::min(totalLines - 1, g_editor.vScrollPos + pageLines);
        g_editor.ensureCaretVisible(); [self setNeedsDisplay:YES]; return;
    }
    if (cmd) {
        NSString *lowerChar = [chars lowercaseString];
        if (shift && [lowerChar isEqualToString:@"k"]) { g_editor.deleteLine(); [self setNeedsDisplay:YES]; return; }
        if ([lowerChar isEqualToString:@"d"]) { g_editor.selectNextOccurrence(); [self setNeedsDisplay:YES]; return; }
        if ([lowerChar isEqualToString:@"a"]) { g_editor.selectAll(); [self setNeedsDisplay:YES]; return; }
        if ([lowerChar isEqualToString:@"s"]) { shift ? g_editor.saveFileAs() : (g_editor.currentFilePath.empty() ? g_editor.saveFileAs() : g_editor.saveFile(g_editor.currentFilePath)); [self setNeedsDisplay:YES]; return; }
        if ([lowerChar isEqualToString:@"c"]) { g_editor.copyToClipboard(); return; }
        if ([lowerChar isEqualToString:@"x"]) { g_editor.cutToClipboard(); [self setNeedsDisplay:YES]; return; }
        if ([lowerChar isEqualToString:@"v"]) { g_editor.pasteFromClipboard(); [self setNeedsDisplay:YES]; return; }
        if ([lowerChar isEqualToString:@"z"]) { shift ? g_editor.performRedo() : g_editor.performUndo(); [self setNeedsDisplay:YES]; return; }
        if ([lowerChar isEqualToString:@"o"]) { g_editor.openFile(); [self setNeedsDisplay:YES]; return; }
        if ([lowerChar isEqualToString:@"n"]) { g_editor.newFile(); [self setNeedsDisplay:YES]; return; }
        if ([lowerChar isEqualToString:@"0"]) { [self applyZoom:14.0f relative:false]; return; }
        if ([lowerChar isEqualToString:@"+"] || [lowerChar isEqualToString:@"="]) { [self applyZoom:1.1f relative:true]; return; }
        if ([lowerChar isEqualToString:@"-"]) { [self applyZoom:0.9f relative:true]; return; }
    }
    if (code >= 123 && code <= 126) {
        bool opt = ([e modifierFlags] & NSEventModifierFlagOption);
        if (code == 126 && opt) { if (shift) g_editor.copyLines(true); else g_editor.moveLines(true); [self setNeedsDisplay:YES]; return; }
        if (code == 125 && opt) { if (shift) g_editor.copyLines(false); else g_editor.moveLines(false); [self setNeedsDisplay:YES]; return; }
        for (auto& c : g_editor.cursors) {
            if (code == 123) {
                size_t p = c.head; int li = g_editor.getLineIdx(p); size_t lineStart = g_editor.lineStarts[li];
                if (cmd) {
                    if (p == lineStart && p > 0) p--; else { while (p > lineStart && !g_editor.isWordChar(g_editor.pt.charAt(p - 1))) p--; while (p > lineStart && g_editor.isWordChar(g_editor.pt.charAt(p - 1))) p--; }
                    c.head = p;
                } else c.head = g_editor.moveCaretVisual(c.head, false);
                c.isVirtual = false;
            } else if (code == 124) {
                size_t p = c.head; size_t len = g_editor.pt.length(); int li = g_editor.getLineIdx(p);
                size_t lineStart = g_editor.lineStarts[li], lineEnd = (li + 1 < (int)g_editor.lineStarts.size()) ? g_editor.lineStarts[li + 1] : len;
                size_t physEnd = lineEnd; if (physEnd > lineStart && g_editor.pt.charAt(physEnd - 1) == '\n') physEnd--; if (physEnd > lineStart && g_editor.pt.charAt(physEnd - 1) == '\r') physEnd--;
                if (cmd) {
                    if (p == physEnd && p < len) p = lineEnd; else { while (p < physEnd && g_editor.isWordChar(g_editor.pt.charAt(p))) p++; while (p < physEnd && !g_editor.isWordChar(g_editor.pt.charAt(p))) p++; }
                    c.head = p;
                } else c.head = g_editor.moveCaretVisual(c.head, true);
                c.isVirtual = false;
            } else if (code == 126) {
                int l = g_editor.getLineIdx(c.head);
                if (l > 0) {
                    size_t nextPos = g_editor.getPosFromLineAndX(l - 1, c.desiredX);
                    size_t prevLineStart = g_editor.lineStarts[l - 1];
                    size_t prevLineEnd = g_editor.lineStarts[l];
                    if (prevLineEnd > prevLineStart && g_editor.pt.charAt(prevLineEnd - 1) == '\n') prevLineEnd--;
                    if (prevLineEnd > prevLineStart && g_editor.pt.charAt(prevLineEnd - 1) == '\r') prevLineEnd--;
                    if (nextPos > prevLineEnd) c.head = prevLineEnd; else c.head = nextPos;
                }
                c.isVirtual = false;
            } else if (code == 125) {
                int l = g_editor.getLineIdx(c.head);
                if (l + 1 < (int)g_editor.lineStarts.size()) {
                    size_t nextPos = g_editor.getPosFromLineAndX(l + 1, c.desiredX);
                    size_t nextLineStart = g_editor.lineStarts[l + 1];
                    size_t nextLineEnd = (l + 2 < (int)g_editor.lineStarts.size()) ? g_editor.lineStarts[l + 2] : g_editor.pt.length();
                    if (nextLineEnd > nextLineStart && g_editor.pt.charAt(nextLineEnd - 1) == '\n') nextLineEnd--;
                    if (nextLineEnd > nextLineStart && g_editor.pt.charAt(nextLineEnd - 1) == '\r') nextLineEnd--;
                    if (nextPos > nextLineEnd) c.head = nextLineEnd; else c.head = nextPos;
                }
                c.isVirtual = false;
            }
            if (!shift) c.anchor = c.head;
            if (code == 123 || code == 124) c.desiredX = g_editor.getXFromPos(c.head);
        }
        g_editor.ensureCaretVisible(); [self setNeedsDisplay:YES]; return;
    }
    if (![self.inputContext handleEvent:e]) [super keyDown:e];
}

- (void)scrollWheel:(NSEvent *)e {
    if ([e modifierFlags] & NSEventModifierFlagCommand) {
        float dy = [e scrollingDeltaY]; if (dy == 0) dy = [e deltaY];
        if (dy != 0) { float factor = (dy > 0) ? 1.1f : 0.9f; g_editor.updateFont(g_editor.currentFontSize * factor); g_editor.zoomPopupText = std::to_string((int)std::round(g_editor.currentFontSize)) + "px"; g_editor.zoomPopupEndTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(1000); [self setNeedsDisplay:YES]; }
        return;
    }
    g_editor.vScrollPos = std::max(0, (int)((float)g_editor.vScrollPos - [e deltaY]));
    g_editor.hScrollPos = std::max(0, (int)((float)g_editor.hScrollPos - [e deltaX]));
    [self updateScrollers];
}
- (void)insertText:(id)s replacementRange:(NSRange)r {
    NSString* t = [s isKindOfClass:[NSAttributedString class]] ? [s string] : s;
    if ([t isEqualToString:@"\r"] || [t isEqualToString:@"\n"]) { g_editor.insertAtCursors("\n"); }
    else { if (g_editor.cursors.size() > 1) g_editor.insertAtCursorsWithPadding([t UTF8String]); else g_editor.insertAtCursors([t UTF8String]); }
    g_editor.imeComp = ""; [self setNeedsDisplay:YES];
}
- (void)setMarkedText:(id)s selectedRange:(NSRange)sr replacementRange:(NSRange)rr { g_editor.imeComp = [([s isKindOfClass:[NSAttributedString class]] ? [s string] : s) UTF8String]; [self setNeedsDisplay:YES]; }
- (void)unmarkText { g_editor.imeComp = ""; [self setNeedsDisplay:YES]; }
- (BOOL)hasMarkedText { return !g_editor.imeComp.empty(); }
- (NSRange)markedRange { return [self hasMarkedText] ? NSMakeRange(0, g_editor.imeComp.length()) : NSMakeRange(NSNotFound, 0); }
- (NSRange)selectedRange { return NSMakeRange(NSNotFound, 0); }
- (void)doCommandBySelector:(SEL)s {
    if (s == @selector(deleteBackward:)) g_editor.backspaceAtCursors();
    else if (s == @selector(deleteForward:)) g_editor.deleteForwardAtCursors();
    else if (s == @selector(insertNewline:)) g_editor.insertAtCursors("\n");
    [self setNeedsDisplay:YES];
}
- (nullable NSAttributedString *)attributedSubstringForProposedRange:(NSRange)r actualRange:(NSRangePointer)ar { return nil; }
- (NSArray*)validAttributesForMarkedText { return @[]; }
- (NSRect)firstRectForCharacterRange:(NSRange)r actualRange:(NSRangePointer)ar {
    if (g_editor.cursors.empty()) return NSZeroRect;
    float x = g_editor.getXFromPos(g_editor.cursors.back().head), y = (float)(g_editor.getLineIdx(g_editor.cursors.back().head) - g_editor.vScrollPos) * g_editor.lineHeight;
    return [[self window] convertRectToScreen:[self convertRect:NSMakeRect(g_editor.gutterWidth-(float)g_editor.hScrollPos+x, y, 2, g_editor.lineHeight) toView:nil]];
}
- (NSUInteger)characterIndexForPoint:(NSPoint)p { return 0; }
@end

// --- 7. AppDelegate ---
@interface CustomWindow : NSWindow @end
@implementation CustomWindow - (BOOL)canBecomeKeyWindow { return YES; } - (BOOL)canBecomeMainWindow { return YES; } @end

@interface AppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate>
@property (strong) CustomWindow *window;
@end
@implementation AppDelegate
- (void)applicationDidFinishLaunching:(NSNotification *)n {
    self.window = [[CustomWindow alloc] initWithContentRect:NSMakeRect(0,0,800,600) styleMask:NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskResizable|NSWindowStyleMaskMiniaturizable backing:NSBackingStoreBuffered defer:NO];
    [self.window center]; [self.window setDelegate:self]; EditorView *v = [[EditorView alloc] initWithFrame:self.window.contentView.bounds];
    [self.window setContentView:v]; [self.window makeKeyAndOrderFront:nil]; [self.window makeFirstResponder:v];
    [NSApp activateIgnoringOtherApps:YES];
    if([[NSProcessInfo processInfo] arguments].count > 1) g_editor.openFileFromPath([[[NSProcessInfo processInfo] arguments][1] UTF8String]);
}
- (BOOL)windowShouldClose:(NSWindow *)s { return g_editor.checkUnsavedChanges(); }
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)s { if (g_editor.checkUnsavedChanges()) { g_editor.isDirty = false; return NSTerminateNow; } return NSTerminateCancel; }
@end
int main(int argc, const char * argv[]) { @autoreleasepool { NSApplication *a = [NSApplication sharedApplication]; AppDelegate *d = [AppDelegate new]; [a setDelegate:d]; [a setActivationPolicy:NSApplicationActivationPolicyRegular]; [a run]; } return 0; }
