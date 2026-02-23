#pragma once

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
#include <functional>

#if defined(__APPLE__)
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>
#endif

extern const std::wstring APP_VERSION;
extern const std::wstring APP_TITLE;

enum Encoding { ENC_UTF8_NOBOM = 0, ENC_UTF8_BOM, ENC_UTF16LE, ENC_UTF16BE, ENC_ANSI };

std::string WToUTF8(const std::wstring& w);
std::wstring UTF8ToW(const std::string& s);
std::string Utf16ToUtf8(const char* data, size_t len, bool isBigEndian);
std::string AnsiToUtf8(const char* data, size_t len);
Encoding DetectEncoding(const char* buf, size_t len);
std::string ConvertCase(const std::string& s, bool toUpper);

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
    bool open(const char* path);
    void close();
    ~MappedFile() { close(); }
};

struct Editor {
    PieceTable pt;
    UndoManager undo;
    std::unique_ptr<MappedFile> fileMap;
    std::wstring currentFilePath;
    Encoding currentEncoding = ENC_UTF8_NOBOM;
    bool isDirty = false;
    bool isDarkMode = false;
    bool showHelpPopup = false;
    std::string searchQuery;
    std::string replaceQuery;
    bool searchMatchCase = false;
    bool searchWholeWord = false;
    bool searchRegex = false;
    bool isReplaceMode = false;
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
    
#if defined(__APPLE__)
    CGColorRef colBackground=NULL, colText=NULL, colGutterBg=NULL, colGutterText=NULL, colSel=NULL, colCaret=NULL;
    CTFontRef fontRef = nullptr;
#endif
    std::wstring helpTextStr;

    // UI層とのコールバック (プラットフォーム非依存にするため)
    std::function<void()> cbNeedsDisplay;
    std::function<void()> cbUpdateScrollers;
    std::function<void()> cbUpdateTitleBar;
    std::function<void()> cbBeep;
    std::function<void(float&, float&)> cbGetViewSize;
    std::function<void(const std::string&, bool)> cbSetClipboard;
    std::function<std::string(bool&)> cbGetClipboard;
    std::function<void(int)> cbShowReplaceAlert;
    std::function<bool()> cbShowUnsavedAlert;
    std::function<bool()> cbSaveFileAs;
    std::function<bool()> cbOpenFile;

    void detectNewlineStyle(const char* buf, size_t len);
    void insertAtCursorsWithPadding(const std::string& text);
    void insertNewlineWithAutoIndent();
    void insertRectangularBlock(const std::string& text);
    std::vector<int> getUniqueLineIndices();
    void deleteLine();
    void moveLines(bool up);
    void copyLines(bool up);
    void indentLines(bool forceLineIndent = false);
    void unindentLines();
    void updateGutterWidth();
    void updateMaxLineWidth();
    std::pair<std::string, bool> getHighlightTarget();
    std::string preprocessRegexQuery(const std::string& query);
    std::string UnescapeString(const std::string& s, const std::string& newline);
    size_t findText(size_t startPos, const std::string& query, bool forward, bool matchCase, bool wholeWord, bool isRegex, size_t* outLen = nullptr);
    void findNext(bool forward);
    void replaceNext();
    void replaceAll();
    void selectNextOccurrence();
    
    void updateTitleBar();
    void updateScrollBars();
    void updateDirtyFlag();
    void updateFont(float s);
    void rebuildLineStarts();
    int getLineIdx(size_t pos);
    float getXInLine(int li, size_t pos);
    float getXFromPos(size_t p);
    size_t getPosFromLineAndX(int li, float tx);
    size_t getDocPosFromPoint(float x, float y);
    void ensureCaretVisible();
    size_t moveCaretVisual(size_t pos, bool f);
    void insertAtCursors(const std::string& t);
    void backspaceAtCursors();
    void deleteForwardAtCursors();
    void selectAll();
    void convertSelectedText(bool toUpper);
    
    void copyToClipboard();
    void cutToClipboard();
    void pasteFromClipboard();
    
    void performUndo();
    void performRedo();
    
    bool checkUnsavedChanges();
    bool saveFile(const std::wstring& p);
    bool saveFileAs();
    bool openFileFromPath(const std::string& p);
    bool openFile();
    void newFile();
    
    bool isWordChar(char c);
    void getWordBoundaries(size_t pos, size_t& start, size_t& end);
    void initGraphics();
    void updateThemeColors();
    
#if defined(__APPLE__)
    void render(CGContextRef ctx, float w, float h);
#endif
    
    ~Editor();
};
