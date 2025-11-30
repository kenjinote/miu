// FastMiniEditor.cpp
// Minimal, high-performance text editor for huge files using Win32 + DirectWrite.
// Features: memory-mapped original file, piece table for edits, undo/redo, caret, basic input, fast visible-range rendering.
// ... (Features list abbreviated for brevity) ...
// Search Feature: Global F3/Shift+F3 support (works inside Find Dialog).

// Build (MSVC):
// rc FastMiniEditor.rc
// cl /std:c++17 /O2 /EHsc FastMiniEditor.cpp FastMiniEditor.res /link d2d1.lib dwrite.lib user32.lib ole32.lib imm32.lib comdlg32.lib comctl32.lib

#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define NOMINMAX
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <imm.h>
#include <commdlg.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <memory>
#include <cassert>
#include <algorithm>
#include <fstream>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <regex> // Regex support
#include "resource.h"

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "imm32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "comctl32.lib")

// --- UTF helpers --------------------------------------------------
static std::wstring UTF8ToW(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), NULL, 0);
    if (n <= 0) return {};
    std::wstring w;
    w.resize(n);
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}

static std::string WToUTF8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), NULL, 0, NULL, NULL);
    if (n <= 0) return {};
    std::string s;
    s.resize(n);
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &s[0], n, NULL, NULL);
    return s;
}

// --- Piece Table core ---
struct Piece {
    bool isOriginal;
    size_t start;
    size_t len;
};

struct PieceTable {
    const char* origPtr = nullptr;
    size_t origSize = 0;
    std::string addBuf;
    std::vector<Piece> pieces;

    void initFromFile(const char* data, size_t size) {
        origPtr = data; origSize = size;
        pieces.clear();
        addBuf.clear();
        if (size > 0) pieces.push_back({ true, 0, size });
    }

    void initEmpty() {
        origPtr = nullptr; origSize = 0;
        pieces.clear();
        addBuf.clear();
    }

    size_t length() const {
        size_t s = 0;
        for (auto& p : pieces) s += p.len;
        return s;
    }

    std::string getRange(size_t pos, size_t count) const {
        std::string out;
        out.reserve(std::min(count, (size_t)4096));
        size_t cur = 0;
        for (const auto& p : pieces) {
            if (cur + p.len <= pos) { cur += p.len; continue; }
            size_t localStart = (pos > cur) ? (pos - cur) : 0;
            size_t take = std::min(p.len - localStart, count - out.size());
            if (take == 0) break;
            if (p.isOriginal) out.append(origPtr + p.start + localStart, take);
            else out.append(addBuf.data() + p.start + localStart, take);
            if (out.size() >= count) break;
            cur += p.len;
        }
        return out;
    }

    void insert(size_t pos, const std::string& s) {
        if (s.empty()) return;
        size_t cur = 0; size_t idx = 0;
        while (idx < pieces.size() && cur + pieces[idx].len < pos) { cur += pieces[idx].len; ++idx; }
        if (idx < pieces.size()) {
            Piece p = pieces[idx];
            size_t offsetInPiece = pos - cur;
            if (offsetInPiece > 0 && offsetInPiece < p.len) {
                Piece left = { p.isOriginal, p.start, offsetInPiece };
                Piece right = { p.isOriginal, p.start + offsetInPiece, p.len - offsetInPiece };
                pieces[idx] = left;
                pieces.insert(pieces.begin() + idx + 1, right);
                idx++;
            }
            else if (offsetInPiece == p.len) idx++;
        }
        else idx = pieces.size();
        size_t addStart = addBuf.size();
        addBuf.append(s);
        pieces.insert(pieces.begin() + idx, { false, addStart, s.size() });
        coalesceAround(idx);
    }

    void erase(size_t pos, size_t count) {
        if (count == 0) return;
        size_t cur = 0; size_t idx = 0;
        while (idx < pieces.size() && cur + pieces[idx].len <= pos) { cur += pieces[idx].len; ++idx; }
        size_t remaining = count;
        if (idx >= pieces.size()) return;
        if (pos > cur) {
            Piece p = pieces[idx];
            size_t leftLen = pos - cur;
            pieces[idx] = { p.isOriginal, p.start, leftLen };
            pieces.insert(pieces.begin() + idx + 1, { p.isOriginal, p.start + leftLen, p.len - leftLen });
            idx++;
        }
        while (idx < pieces.size() && remaining > 0) {
            if (pieces[idx].len <= remaining) {
                remaining -= pieces[idx].len;
                pieces.erase(pieces.begin() + idx);
            }
            else {
                pieces[idx].start += remaining;
                pieces[idx].len -= remaining;
                remaining = 0;
            }
        }
        coalesceAround(idx > 0 ? idx - 1 : 0);
    }

    void coalesceAround(size_t idx) {
        if (pieces.empty()) return;
        if (idx >= pieces.size()) idx = pieces.size() - 1;
        if (idx > 0) {
            Piece& a = pieces[idx - 1]; Piece& b = pieces[idx];
            if (!a.isOriginal && !b.isOriginal && (a.start + a.len == b.start)) {
                a.len += b.len; pieces.erase(pieces.begin() + idx); idx--;
            }
        }
        if (idx + 1 < pieces.size()) {
            Piece& a = pieces[idx]; Piece& b = pieces[idx + 1];
            if (!a.isOriginal && !b.isOriginal && (a.start + a.len == b.start)) {
                a.len += b.len; pieces.erase(pieces.begin() + idx + 1);
            }
        }
    }

    char charAt(size_t pos) const {
        size_t cur = 0;
        for (const auto& p : pieces) {
            if (cur + p.len <= pos) { cur += p.len; continue; }
            size_t local = pos - cur;
            if (p.isOriginal) return origPtr[p.start + local];
            else return addBuf[p.start + local];
        }
        return ' ';
    }
};

// --- Cursor / Selection ---
struct Cursor {
    size_t head;
    size_t anchor;
    float desiredX;
    size_t start() const { return std::min(head, anchor); }
    size_t end() const { return std::max(head, anchor); }
    bool hasSelection() const { return head != anchor; }
    void clearSelection() { anchor = head; }
};

// --- Undo/Redo ---
struct EditOp {
    enum Type { Insert, Erase } type;
    size_t pos;
    std::string text;
};

struct EditBatch {
    std::vector<EditOp> ops;
    std::vector<Cursor> beforeCursors;
    std::vector<Cursor> afterCursors;
};

struct UndoManager {
    std::vector<EditBatch> undoStack;
    std::vector<EditBatch> redoStack;
    int savePoint = 0;

    void clear() { undoStack.clear(); redoStack.clear(); savePoint = 0; }
    void markSaved() { savePoint = (int)undoStack.size(); }
    bool isModified() const { return (int)undoStack.size() != savePoint; }
    void push(const EditBatch& batch) {
        if (savePoint > (int)undoStack.size()) savePoint = -1;
        undoStack.push_back(batch); redoStack.clear();
    }
    bool canUndo() const { return !undoStack.empty(); }
    bool canRedo() const { return !redoStack.empty(); }
    EditBatch popUndo() { EditBatch e = undoStack.back(); undoStack.pop_back(); redoStack.push_back(e); return e; }
    EditBatch popRedo() { EditBatch e = redoStack.back(); redoStack.pop_back(); undoStack.push_back(e); return e; }
};

// --- File mapping helper ---
struct MappedFile {
    HANDLE hFile = INVALID_HANDLE_VALUE;
    HANDLE hMap = NULL;
    const char* ptr = nullptr;
    size_t size = 0;

    bool open(const wchar_t* path) {
        hFile = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return false;
        LARGE_INTEGER li; if (!GetFileSizeEx(hFile, &li)) return false; size = (size_t)li.QuadPart;
        if (size == 0) { ptr = nullptr; return true; }
        hMap = CreateFileMappingW(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
        if (!hMap) return false;
        ptr = (const char*)MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
        if (!ptr) return false;
        return true;
    }
    void close() {
        if (ptr) { UnmapViewOfFile(ptr); ptr = nullptr; }
        if (hMap) { CloseHandle(hMap); hMap = NULL; }
        if (hFile != INVALID_HANDLE_VALUE) { CloseHandle(hFile); hFile = INVALID_HANDLE_VALUE; }
    }
    ~MappedFile() { close(); }
};

// --- Global editor state ---
struct Editor {
    HWND hwnd = NULL;
    HWND hFindDlg = NULL; // Find Dialog Handle
    PieceTable pt;
    UndoManager undo;

    std::unique_ptr<MappedFile> fileMap;
    std::wstring currentFilePath;
    bool isDirty = false;

    // Search State
    std::string searchQuery;
    bool searchMatchCase = false;
    bool searchWholeWord = false;
    bool searchRegex = false;

    // Multi-cursor state
    std::vector<Cursor> cursors;
    EditBatch pendingPadding;

    bool isDragging = false;
    bool isRectSelecting = false;
    float rectAnchorX = 0, rectAnchorY = 0;
    float rectHeadX = 0, rectHeadY = 0;

    // Text Drag & Drop
    bool isDragMovePending = false;
    bool isDragMoving = false;
    size_t dragMoveSourceStart = 0;
    size_t dragMoveSourceEnd = 0;
    size_t dragMoveDestPos = 0;

    wchar_t highSurrogate = 0;
    std::string imeComp;

    // Scroll state
    int vScrollPos = 0;
    int hScrollPos = 0;
    std::vector<size_t> lineStarts;
    float maxLineWidth = 100.0f;
    float gutterWidth = 50.0f;

    // Click handling
    DWORD lastClickTime = 0;
    int clickCount = 0;
    int lastClickX = 0, lastClickY = 0;

    // Zoom state
    float currentFontSize = 21.0f;
    DWORD zoomPopupEndTime = 0;
    std::wstring zoomPopupText;

    // rendering
    ID2D1Factory* d2dFactory = nullptr;
    ID2D1HwndRenderTarget* rend = nullptr;
    IDWriteFactory* dwFactory = nullptr;
    IDWriteTextFormat* textFormat = nullptr;
    IDWriteTextFormat* popupTextFormat = nullptr;

    ID2D1StrokeStyle* dotStyle = nullptr;
    ID2D1StrokeStyle* roundJoinStyle = nullptr;

    D2D1::ColorF background = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
    D2D1::ColorF textColor = D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f);
    D2D1::ColorF gutterBg = D2D1::ColorF(0.95f, 0.95f, 0.95f, 1.0f);
    D2D1::ColorF gutterText = D2D1::ColorF(0.6f, 0.6f, 0.6f, 1.0f);
    D2D1::ColorF selColor = D2D1::ColorF(0.7f, 0.8f, 1.0f, 1.0f);
    D2D1::ColorF highlightColor = D2D1::ColorF(1.0f, 1.0f, 0.0f, 0.4f); // Yellow for search hits

    float dpiScaleX = 1.0f, dpiScaleY = 1.0f;
    float lineHeight = 17.5f;
    float charWidth = 8.0f;

    void initGraphics(HWND h) {
        hwnd = h;
        D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2dFactory);
        DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&dwFactory));
        RECT r; GetClientRect(hwnd, &r);
        D2D1_SIZE_U size = D2D1::SizeU(r.right - r.left, r.bottom - r.top);
        d2dFactory->CreateHwndRenderTarget(D2D1::RenderTargetProperties(), D2D1::HwndRenderTargetProperties(hwnd, size), &rend);
        FLOAT dpix = 96, dpiy = 96; rend->GetDpi(&dpix, &dpiy);
        dpiScaleX = dpix / 96.0f; dpiScaleY = dpiy / 96.0f;

        dwFactory->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 24.0f, L"en-us", &popupTextFormat);
        if (popupTextFormat) {
            popupTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            popupTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }

        float dashes[] = { 2.0f, 2.0f };
        D2D1_STROKE_STYLE_PROPERTIES props = D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_FLAT, D2D1_CAP_STYLE_FLAT, D2D1_CAP_STYLE_FLAT, D2D1_LINE_JOIN_MITER, 10.0f, D2D1_DASH_STYLE_CUSTOM, 0.0f);
        d2dFactory->CreateStrokeStyle(&props, dashes, 2, &dotStyle);

        D2D1_STROKE_STYLE_PROPERTIES roundProps = D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_LINE_JOIN_ROUND, 10.0f, D2D1_DASH_STYLE_SOLID, 0.0f);
        d2dFactory->CreateStrokeStyle(&roundProps, nullptr, 0, &roundJoinStyle);

        updateFont(currentFontSize);
        rebuildLineStarts();
        cursors.push_back({ 0, 0, 0.0f });
        updateTitleBar();
    }

    void updateFont(float size) {
        if (size < 6.0f) size = 6.0f;
        if (size > 200.0f) size = 200.0f;
        currentFontSize = size;
        if (textFormat) { textFormat->Release(); textFormat = nullptr; }
        dwFactory->CreateTextFormat(L"Consolas", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, currentFontSize, L"en-us", &textFormat);
        lineHeight = currentFontSize * 1.25f;
        textFormat->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, lineHeight, lineHeight * 0.8f);
        textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

        IDWriteTextLayout* layout = nullptr;
        if (SUCCEEDED(dwFactory->CreateTextLayout(L"0", 1, textFormat, 100.0f, 100.0f, &layout))) {
            DWRITE_TEXT_METRICS m; layout->GetMetrics(&m);
            charWidth = m.width; layout->Release();
        }
        updateGutterWidth(); updateScrollBars();
    }

    void destroyGraphics() {
        if (popupTextFormat) { popupTextFormat->Release(); popupTextFormat = nullptr; }
        if (dotStyle) { dotStyle->Release(); dotStyle = nullptr; }
        if (roundJoinStyle) { roundJoinStyle->Release(); roundJoinStyle = nullptr; }
        if (textFormat) { textFormat->Release(); textFormat = nullptr; }
        if (dwFactory) { dwFactory->Release(); dwFactory = nullptr; }
        if (rend) { rend->Release(); rend = nullptr; }
        if (d2dFactory) { d2dFactory->Release(); d2dFactory = nullptr; }
    }

    // ... (rest of standard helpers from previous version) ...
    void updateTitleBar() {
        if (!hwnd) return;
        std::wstring title = L"FastMiniEditor - ";
        if (currentFilePath.empty()) title += L"Untitled"; else title += currentFilePath;
        if (isDirty) title += L" *";
        SetWindowTextW(hwnd, title.c_str());
    }

    void updateDirtyFlag() {
        bool newDirty = undo.isModified();
        if (isDirty != newDirty) { isDirty = newDirty; updateTitleBar(); }
    }

    void updateGutterWidth() {
        int lines = (int)lineStarts.size(); int digits = 1;
        while (lines >= 10) { lines /= 10; digits++; }
        float digitWidth = 10.0f * (currentFontSize / 14.0f);
        gutterWidth = (float)(digits * digitWidth + 20.0f);
    }

    void rebuildLineStarts() {
        lineStarts.clear(); lineStarts.push_back(0);
        size_t len = pt.length(); size_t globalOffset = 0; size_t maxBytes = 0;
        for (const auto& p : pt.pieces) {
            const char* buf = p.isOriginal ? (pt.origPtr + p.start) : (pt.addBuf.data() + p.start);
            for (size_t i = 0; i < p.len; ++i) { if (buf[i] == '\n') lineStarts.push_back(globalOffset + i + 1); }
            globalOffset += p.len;
        }
        for (size_t i = 0; i < lineStarts.size(); ++i) {
            size_t s = lineStarts[i]; size_t e = (i + 1 < lineStarts.size()) ? lineStarts[i + 1] : len;
            size_t lineLen = e - s; if (lineLen > maxBytes) maxBytes = lineLen;
        }
        maxLineWidth = maxBytes * charWidth + 100.0f;
        updateGutterWidth(); updateScrollBars();
    }

    int getLineIdx(size_t pos) {
        if (lineStarts.empty()) return 0;
        auto it = std::upper_bound(lineStarts.begin(), lineStarts.end(), pos);
        int idx = (int)std::distance(lineStarts.begin(), it) - 1;
        if (idx < 0) idx = 0; if (idx >= (int)lineStarts.size()) idx = (int)lineStarts.size() - 1;
        return idx;
    }

    float getXFromPos(size_t pos) {
        int lineIdx = getLineIdx(pos);
        size_t start = lineStarts[lineIdx];
        size_t end = (lineIdx + 1 < (int)lineStarts.size()) ? lineStarts[lineIdx + 1] : pt.length();
        size_t len = (end > start) ? (end - start) : 0;
        std::string lineStr = pt.getRange(start, len); std::wstring wLine = UTF8ToW(lineStr);
        IDWriteTextLayout* layout = nullptr;
        HRESULT hr = dwFactory->CreateTextLayout(wLine.c_str(), (UINT32)wLine.size(), textFormat, 10000.0f, (FLOAT)lineHeight, &layout);
        float x = 0;
        if (SUCCEEDED(hr) && layout) {
            size_t localIdx = pos - start;
            if (localIdx > wLine.size()) localIdx = wLine.size();
            DWRITE_HIT_TEST_METRICS m; FLOAT px, py;
            layout->HitTestTextPosition((UINT32)localIdx, FALSE, &px, &py, &m);
            x = px; layout->Release();
        }
        return x;
    }

    size_t getPosFromLineAndX(int lineIdx, float targetX) {
        if (lineIdx < 0 || lineIdx >= (int)lineStarts.size()) return cursors.empty() ? 0 : cursors.back().head;
        size_t start = lineStarts[lineIdx];
        size_t end = (lineIdx + 1 < (int)lineStarts.size()) ? lineStarts[lineIdx + 1] : pt.length();
        size_t len = (end > start) ? (end - start) : 0;
        std::string lineStr = pt.getRange(start, len); std::wstring wLine = UTF8ToW(lineStr);
        IDWriteTextLayout* layout = nullptr;
        HRESULT hr = dwFactory->CreateTextLayout(wLine.c_str(), (UINT32)wLine.size(), textFormat, 10000.0f, (FLOAT)lineHeight, &layout);
        size_t resultPos = start;
        if (SUCCEEDED(hr) && layout) {
            BOOL isTrailing, isInside; DWRITE_HIT_TEST_METRICS m;
            layout->HitTestPoint(targetX, 1.0f, &isTrailing, &isInside, &m);
            size_t local = m.textPosition;
            if (isTrailing) local += m.length;
            bool hasNewline = (!wLine.empty() && wLine.back() == L'\n');
            if (hasNewline) { if (local >= wLine.size()) local = wLine.size() - 1; }
            else { if (local > wLine.size()) local = wLine.size(); }
            std::wstring wSub = wLine.substr(0, local); std::string sub = WToUTF8(wSub);
            resultPos = start + sub.size(); layout->Release();
        }
        return resultPos;
    }

    void updateScrollBars() {
        if (!hwnd) return;
        RECT rc; GetClientRect(hwnd, &rc);
        float clientH = (rc.bottom - rc.top) / dpiScaleY;
        float clientW = (rc.right - rc.left) / dpiScaleX - gutterWidth;
        if (clientW < 0) clientW = 0;
        int linesVisible = (int)(clientH / lineHeight);
        SCROLLINFO si = {}; si.cbSize = sizeof(SCROLLINFO); si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
        si.nMin = 0; si.nMax = (int)lineStarts.size() + linesVisible - 2;
        if (si.nMax < 0) si.nMax = 0; si.nPage = linesVisible; si.nPos = vScrollPos;
        SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
        si.nMin = 0; si.nMax = (int)maxLineWidth; si.nPage = (int)clientW; si.nPos = hScrollPos;
        SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
    }

    void getCaretPoint(float& x, float& y) {
        if (cursors.empty()) { x = 0; y = 0; return; }
        size_t pos = cursors.back().head; int line = getLineIdx(pos);
        float docY = line * lineHeight;
        float localX = getXFromPos(pos);
        x = (localX - hScrollPos + gutterWidth) * dpiScaleX;
        y = (docY - vScrollPos * lineHeight) * dpiScaleY;
    }

    void ensureCaretVisible() {
        if (cursors.empty()) return;
        Cursor& mainCursor = cursors.back();
        RECT rc; GetClientRect(hwnd, &rc);
        float clientH = (rc.bottom - rc.top) / dpiScaleY;
        int linesVisible = (int)(clientH / lineHeight);
        int caretLine = getLineIdx(mainCursor.head);
        if (caretLine < vScrollPos) vScrollPos = caretLine;
        else if (caretLine >= vScrollPos + linesVisible - 1) vScrollPos = caretLine - linesVisible + 2;
        if (vScrollPos < 0) vScrollPos = 0;
        updateScrollBars(); InvalidateRect(hwnd, NULL, FALSE);
    }

    std::string buildVisibleText(int numLines) {
        if (lineStarts.empty()) return "";
        size_t startOffset = (vScrollPos < (int)lineStarts.size()) ? lineStarts[vScrollPos] : lineStarts.back();
        size_t endOffset = pt.length();
        int endLineIdx = vScrollPos + numLines;
        if (endLineIdx < (int)lineStarts.size()) endOffset = lineStarts[endLineIdx];
        return pt.getRange(startOffset, (endOffset > startOffset) ? (endOffset - startOffset) : 0);
    }

    size_t getDocPosFromPoint(int x, int y) {
        float dipX = x / dpiScaleX; float dipY = y / dpiScaleY;
        if (dipX < gutterWidth) dipX = gutterWidth;
        float virtualX = dipX - gutterWidth + hScrollPos; float virtualY = dipY;
        RECT rc; GetClientRect(hwnd, &rc);
        float clientH = (rc.bottom - rc.top) / dpiScaleY;
        float clientW = (rc.right - rc.left) / dpiScaleX - gutterWidth;
        int linesVisible = (int)(clientH / lineHeight) + 2;
        std::string text = buildVisibleText(linesVisible); std::wstring wtext = UTF8ToW(text);
        float layoutWidth = maxLineWidth + clientW;
        IDWriteTextLayout* layout = nullptr;
        HRESULT hr = dwFactory->CreateTextLayout(wtext.c_str(), (UINT32)wtext.size(), textFormat, layoutWidth, clientH, &layout);
        size_t resultPos = 0;
        size_t visibleStartOffset = (vScrollPos < (int)lineStarts.size()) ? lineStarts[vScrollPos] : pt.length();
        if (SUCCEEDED(hr) && layout) {
            BOOL isTrailing, isInside; DWRITE_HIT_TEST_METRICS metrics;
            layout->HitTestPoint(virtualX, virtualY, &isTrailing, &isInside, &metrics);
            UINT32 utf16Index = metrics.textPosition;
            if (isTrailing) utf16Index += metrics.length;
            if (utf16Index > wtext.size()) utf16Index = (UINT32)wtext.size();
            std::wstring wsub = wtext.substr(0, utf16Index); std::string sub = WToUTF8(wsub);
            resultPos = visibleStartOffset + sub.size(); layout->Release();
        }
        if (resultPos > pt.length()) resultPos = pt.length();
        return resultPos;
    }

    // --- Search Logic ---
    bool isWordChar(char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
    }

    // Returns position of found text
    size_t findText(size_t startPos, const std::string& query, bool forward, bool matchCase, bool wholeWord, bool isRegex) {
        if (query.empty()) return std::string::npos;
        size_t len = pt.length();

        if (isRegex) {
            std::string fullText = pt.getRange(0, len);

            try {
                std::regex_constants::syntax_option_type flags = std::regex_constants::ECMAScript;
                if (!matchCase) flags |= std::regex_constants::icase;
                std::regex re(query, flags);

                if (forward) {
                    if (startPos >= fullText.size()) startPos = 0; // Wrap basic check

                    std::smatch m;
                    std::string::const_iterator searchStart = fullText.begin() + startPos;
                    if (std::regex_search(searchStart, fullText.cend(), m, re)) {
                        return startPos + m.position();
                    }
                    if (std::regex_search(fullText.cbegin(), fullText.cend(), m, re)) {
                        return m.position();
                    }
                }
                else {
                    auto words_begin = std::sregex_iterator(fullText.begin(), fullText.end(), re);
                    auto words_end = std::sregex_iterator();

                    size_t bestPos = std::string::npos;
                    size_t lastMatch = std::string::npos;

                    size_t limit = (startPos == 0) ? len : startPos;

                    for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
                        std::smatch match = *i;
                        size_t pos = match.position();
                        if (pos < limit) {
                            bestPos = pos;
                        }
                        lastMatch = pos;
                    }

                    if (bestPos != std::string::npos) return bestPos;
                    if (lastMatch != std::string::npos) return lastMatch;
                }
            }
            catch (const std::regex_error&) {
                return std::string::npos;
            }
            return std::string::npos;
        }

        // Standard linear search (Optimized for PieceTable without allocation)
        size_t qLen = query.length();
        auto toLower = [](char c) { return (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c; };

        size_t cur = startPos;
        if (forward) { if (cur >= len) cur = 0; }
        else { if (cur == 0) cur = len; else cur--; }

        size_t count = 0;
        while (count < len) {
            bool match = true;
            for (size_t i = 0; i < qLen; ++i) {
                size_t p = cur + i;
                if (p >= len) { match = false; break; }
                char c1 = pt.charAt(p);
                char c2 = query[i];
                if (!matchCase) { c1 = toLower(c1); c2 = toLower(c2); }
                if (c1 != c2) { match = false; break; }
            }

            if (match && wholeWord) {
                if (cur > 0 && isWordChar(pt.charAt(cur - 1))) match = false;
                if (match && (cur + qLen < len) && isWordChar(pt.charAt(cur + qLen))) match = false;
            }

            if (match) return cur;

            if (forward) {
                cur++; if (cur >= len) cur = 0;
            }
            else {
                if (cur == 0) cur = len - 1; else cur--;
            }
            count++;
        }
        return std::string::npos;
    }

    void findNext(bool forward) {
        if (searchQuery.empty()) { showFindDialog(); return; }

        size_t startPos;
        if (forward) {
            // Search after current selection
            startPos = cursors.empty() ? 0 : cursors.back().end();
        }
        else {
            // Search before current selection
            startPos = cursors.empty() ? 0 : cursors.back().start();
        }

        size_t pos = findText(startPos, searchQuery, forward, searchMatchCase, searchWholeWord, searchRegex);

        if (pos != std::string::npos) {
            size_t matchLen = searchQuery.length();
            if (searchRegex) {
                std::string fullText = pt.getRange(0, pt.length());
                try {
                    std::regex_constants::syntax_option_type flags = std::regex_constants::ECMAScript;
                    if (!searchMatchCase) flags |= std::regex_constants::icase;
                    std::regex re(searchQuery, flags);
                    std::smatch m;
                    std::string::const_iterator searchStart = fullText.begin() + pos;
                    if (std::regex_search(searchStart, fullText.cend(), m, re, std::regex_constants::match_continuous)) {
                        matchLen = m.length();
                    }
                }
                catch (...) {}
            }

            cursors.clear();
            cursors.push_back({ pos + matchLen, pos, getXFromPos(pos + matchLen) });
            ensureCaretVisible();
            updateTitleBar();
        }
        else {
            MessageBeep(MB_ICONWARNING);
        }
    }

    static INT_PTR CALLBACK FindDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
        Editor* pThis = (Editor*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
        switch (message) {
        case WM_INITDIALOG:
            pThis = (Editor*)lParam;
            SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)pThis);
            // Center Dialog
            {
                RECT rcParent, rcDlg;
                GetWindowRect(pThis->hwnd, &rcParent);
                GetWindowRect(hDlg, &rcDlg);
                int dlgW = rcDlg.right - rcDlg.left;
                int dlgH = rcDlg.bottom - rcDlg.top;
                int parentW = rcParent.right - rcParent.left;
                int parentH = rcParent.bottom - rcParent.top;
                int x = rcParent.left + (parentW - dlgW) / 2;
                int y = rcParent.top + (parentH - dlgH) / 2;
                SetWindowPos(hDlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            }
            // Fix: Use Wide API to support Emojis
            SetDlgItemTextW(hDlg, IDC_FIND_EDIT, UTF8ToW(pThis->searchQuery).c_str());
            CheckDlgButton(hDlg, IDC_FIND_CASE, pThis->searchMatchCase ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hDlg, IDC_FIND_WORD, pThis->searchWholeWord ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hDlg, IDC_FIND_REGEX, pThis->searchRegex ? BST_CHECKED : BST_UNCHECKED);
            SetFocus(GetDlgItem(hDlg, IDC_FIND_EDIT));
            SendMessage(GetDlgItem(hDlg, IDC_FIND_EDIT), EM_SETSEL, 0, -1);
            return FALSE;
        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_FIND_CASE) {
                pThis->searchMatchCase = IsDlgButtonChecked(hDlg, IDC_FIND_CASE) == BST_CHECKED;
                InvalidateRect(pThis->hwnd, NULL, FALSE);
            }
            else if (LOWORD(wParam) == IDC_FIND_WORD) {
                pThis->searchWholeWord = IsDlgButtonChecked(hDlg, IDC_FIND_WORD) == BST_CHECKED;
                InvalidateRect(pThis->hwnd, NULL, FALSE);
            }
            else if (LOWORD(wParam) == IDC_FIND_REGEX) {
                pThis->searchRegex = IsDlgButtonChecked(hDlg, IDC_FIND_REGEX) == BST_CHECKED;
                InvalidateRect(pThis->hwnd, NULL, FALSE);
            }
            else if (LOWORD(wParam) == IDC_FIND_EDIT && HIWORD(wParam) == EN_CHANGE) {
                wchar_t wbuf[1024]; // Fix: Use Wide buffer
                GetDlgItemTextW(hDlg, IDC_FIND_EDIT, wbuf, 1024);
                pThis->searchQuery = WToUTF8(wbuf);
                InvalidateRect(pThis->hwnd, NULL, FALSE);
            }
            else if (LOWORD(wParam) == IDC_FIND_NEXT || LOWORD(wParam) == IDOK) {
                wchar_t wbuf[1024]; // Fix: Use Wide buffer
                GetDlgItemTextW(hDlg, IDC_FIND_EDIT, wbuf, 1024);
                pThis->searchQuery = WToUTF8(wbuf);
                pThis->searchMatchCase = IsDlgButtonChecked(hDlg, IDC_FIND_CASE) == BST_CHECKED;
                pThis->searchWholeWord = IsDlgButtonChecked(hDlg, IDC_FIND_WORD) == BST_CHECKED;
                pThis->searchRegex = IsDlgButtonChecked(hDlg, IDC_FIND_REGEX) == BST_CHECKED;
                pThis->findNext(true);
                return TRUE;
            }
            else if (LOWORD(wParam) == IDC_FIND_CANCEL || LOWORD(wParam) == IDCANCEL) {
                DestroyWindow(hDlg);
                pThis->hFindDlg = NULL;
                return TRUE;
            }
            break;
        }
        return FALSE;
    }

    void showFindDialog() {
        if (hFindDlg) { SetFocus(hFindDlg); return; }

        if (!cursors.empty() && cursors.back().hasSelection()) {
            size_t s = cursors.back().start();
            size_t len = cursors.back().end() - s;
            if (len < 100) {
                searchQuery = pt.getRange(s, len);
            }
        }

        hFindDlg = CreateDialogParamW(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_FIND_DIALOG), hwnd, FindDlgProc, (LPARAM)this);
        ShowWindow(hFindDlg, SW_SHOW);
    }

    // ... (Existing helper methods: rollbackPadding, commitPadding, updateRectSelection, performDragMove) ...
    void rollbackPadding() {
        if (pendingPadding.ops.empty()) return;
        for (int i = (int)pendingPadding.ops.size() - 1; i >= 0; --i) {
            const auto& op = pendingPadding.ops[i]; if (op.type == EditOp::Insert) pt.erase(op.pos, op.text.size());
        }
        pendingPadding.ops.clear(); rebuildLineStarts();
    }
    void commitPadding() { if (pendingPadding.ops.empty()) return; undo.push(pendingPadding); pendingPadding.ops.clear(); }
    void updateRectSelection() {
        rollbackPadding();
        float startY = std::min(rectAnchorY, rectHeadY); float endY = std::max(rectAnchorY, rectHeadY);
        int startLineIdx = (int)(startY / lineHeight); int endLineIdx = (int)(endY / lineHeight);
        if (startLineIdx < 0) startLineIdx = 0; if (endLineIdx >= (int)lineStarts.size()) endLineIdx = (int)lineStarts.size() - 1;
        float targetAnchorX = rectAnchorX; float targetHeadX = rectHeadX;
        std::vector<int> lines; for (int i = startLineIdx; i <= endLineIdx; ++i) lines.push_back(i);
        std::reverse(lines.begin(), lines.end());
        cursors.clear();
        for (int lineIdx : lines) {
            size_t start = lineStarts[lineIdx]; size_t nextStart = (lineIdx + 1 < (int)lineStarts.size()) ? lineStarts[lineIdx + 1] : pt.length();
            size_t end = nextStart; if (end > start && pt.charAt(end - 1) == '\n') end--;
            float currentWidth = 0; std::string lineStr = pt.getRange(start, end - start); std::wstring wLine = UTF8ToW(lineStr); currentWidth = (float)wLine.length() * charWidth;
            float neededX = std::max(targetAnchorX, targetHeadX);
            if (neededX > currentWidth) {
                int spacesNeeded = (int)std::ceil((neededX - currentWidth) / charWidth);
                if (spacesNeeded > 0) {
                    std::string spaces(spacesNeeded, ' '); pt.insert(end, spaces);
                    EditOp op; op.type = EditOp::Insert; op.pos = end; op.text = spaces; pendingPadding.ops.push_back(op);
                }
            }
        }
        if (!pendingPadding.ops.empty()) rebuildLineStarts();
        for (int i = startLineIdx; i <= endLineIdx; ++i) {
            size_t anc = getPosFromLineAndX(i, targetAnchorX); size_t hd = getPosFromLineAndX(i, targetHeadX); cursors.push_back({ hd, anc, targetHeadX });
        }
    }
    void performDragMove() {
        if (dragMoveDestPos >= dragMoveSourceStart && dragMoveDestPos <= dragMoveSourceEnd) return;
        std::string text = pt.getRange(dragMoveSourceStart, dragMoveSourceEnd - dragMoveSourceStart);
        EditBatch batch; batch.beforeCursors = cursors;
        pt.erase(dragMoveSourceStart, text.size()); batch.ops.push_back({ EditOp::Erase, dragMoveSourceStart, text });
        size_t insertPos = dragMoveDestPos; if (insertPos > dragMoveSourceStart) insertPos -= text.size();
        pt.insert(insertPos, text); batch.ops.push_back({ EditOp::Insert, insertPos, text });
        cursors.clear(); cursors.push_back({ insertPos + text.size(), insertPos, getXFromPos(insertPos + text.size()) });
        batch.afterCursors = cursors; undo.push(batch); rebuildLineStarts(); ensureCaretVisible(); updateDirtyFlag();
    }

    void render() {
        if (!rend) return;
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
        rend->BeginDraw(); rend->Clear(background);
        RECT rc; GetClientRect(hwnd, &rc); D2D1_SIZE_F size = rend->GetSize();
        float clientW = size.width; float clientH = size.height;
        int linesVisible = (int)(clientH / lineHeight) + 2;
        std::string text = buildVisibleText(linesVisible);
        size_t visibleStartOffset = (vScrollPos < (int)lineStarts.size()) ? lineStarts[vScrollPos] : pt.length();
        size_t mainCaretPos = cursors.empty() ? 0 : cursors.back().head;
        size_t caretOffsetInVisible = std::string::npos;
        if (mainCaretPos >= visibleStartOffset) caretOffsetInVisible = mainCaretPos - visibleStartOffset;
        bool hasIME = !imeComp.empty() && caretOffsetInVisible != std::string::npos && caretOffsetInVisible <= text.size();
        if (hasIME) text.insert(caretOffsetInVisible, imeComp);
        std::wstring wtext = UTF8ToW(text);
        float layoutWidth = maxLineWidth + clientW;
        IDWriteTextLayout* layout = nullptr;
        HRESULT hr = dwFactory->CreateTextLayout(wtext.c_str(), (UINT32)wtext.size(), textFormat, layoutWidth, clientH, &layout);
        D2D1_MATRIX_3X2_F transform = D2D1::Matrix3x2F::Translation(gutterWidth - (float)hScrollPos, 0);
        rend->SetTransform(transform);
        float imeCx = 0, imeCy = 0;

        if (SUCCEEDED(hr) && layout) {
            ID2D1SolidColorBrush* selBrush = nullptr; rend->CreateSolidColorBrush(selColor, &selBrush);
            ID2D1SolidColorBrush* caretBrush = nullptr; rend->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f), &caretBrush);
            ID2D1SolidColorBrush* hlBrush = nullptr; rend->CreateSolidColorBrush(highlightColor, &hlBrush);

            // --- Search Highlight Rendering ---
            if (!searchQuery.empty()) {
                if (searchRegex) {
                    try {
                        std::regex_constants::syntax_option_type flags = std::regex_constants::ECMAScript;
                        if (!searchMatchCase) flags |= std::regex_constants::icase;
                        std::regex re(searchQuery, flags);

                        auto words_begin = std::sregex_iterator(text.begin(), text.end(), re);
                        auto words_end = std::sregex_iterator();

                        for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
                            std::smatch match = *i;
                            size_t offset = match.position();
                            size_t len = match.length();

                            size_t startU16 = UTF8ToW(text.substr(0, offset)).length();
                            size_t lenU16 = UTF8ToW(text.substr(offset, len)).length();

                            UINT32 count = 0;
                            layout->HitTestTextRange((UINT32)startU16, (UINT32)lenU16, 0, 0, 0, 0, &count);
                            if (count > 0) {
                                std::vector<DWRITE_HIT_TEST_METRICS> m(count);
                                layout->HitTestTextRange((UINT32)startU16, (UINT32)lenU16, 0, 0, &m[0], count, &count);
                                for (const auto& mm : m) {
                                    float top = std::floor((mm.top + lineHeight * 0.5f) / lineHeight) * lineHeight;
                                    rend->FillRectangle(D2D1::RectF(mm.left, top, mm.left + mm.width, top + lineHeight), hlBrush);
                                }
                            }
                        }
                    }
                    catch (...) {}
                }
                else {
                    std::string q = searchQuery;
                    std::string t = text;
                    if (!searchMatchCase) {
                        std::transform(q.begin(), q.end(), q.begin(), ::tolower);
                        std::transform(t.begin(), t.end(), t.begin(), ::tolower);
                    }

                    size_t offset = 0;
                    while ((offset = t.find(q, offset)) != std::string::npos) {
                        bool match = true;
                        if (searchWholeWord) {
                            if (offset > 0 && isWordChar(text[offset - 1])) match = false;
                            if (match && (offset + q.length() < text.length()) && isWordChar(text[offset + q.length()])) match = false;
                        }

                        if (match) {
                            size_t startU16 = UTF8ToW(text.substr(0, offset)).length();
                            size_t lenU16 = UTF8ToW(text.substr(offset, q.length())).length();
                            UINT32 count = 0;
                            layout->HitTestTextRange((UINT32)startU16, (UINT32)lenU16, 0, 0, 0, 0, &count);
                            if (count > 0) {
                                std::vector<DWRITE_HIT_TEST_METRICS> m(count);
                                layout->HitTestTextRange((UINT32)startU16, (UINT32)lenU16, 0, 0, &m[0], count, &count);
                                for (const auto& mm : m) {
                                    float top = std::floor((mm.top + lineHeight * 0.5f) / lineHeight) * lineHeight;
                                    rend->FillRectangle(D2D1::RectF(mm.left, top, mm.left + mm.width, top + lineHeight), hlBrush);
                                }
                            }
                        }
                        offset += 1;
                    }
                }
            }

            // --- Selection Rendering ---
            // (Standard selection rendering code...)
            ID2D1Geometry* unifiedSelectionGeo = nullptr;
            std::vector<D2D1_RECT_F> rawRects;
            float hInset = 4.0f; float vInset = 0.0f;
            for (const auto& cursor : cursors) {
                size_t s = cursor.start(); size_t e = cursor.end();
                size_t relS = (s > visibleStartOffset) ? s - visibleStartOffset : 0;
                size_t relE = (e > visibleStartOffset) ? e - visibleStartOffset : 0;
                if (hasIME) {
                    if (relS >= caretOffsetInVisible) relS += imeComp.size();
                    if (relE >= caretOffsetInVisible) relE += imeComp.size();
                }
                if (relS < text.size() && relS != relE) {
                    if (relE > text.size()) relE = text.size();
                    if (relE > relS) {
                        std::string subS = text.substr(0, relS); std::string subRange = text.substr(relS, relE - relS);
                        size_t utf16Start = UTF8ToW(subS).size(); size_t utf16Len = UTF8ToW(subRange).size();
                        UINT32 count = 0;
                        layout->HitTestTextRange((UINT32)utf16Start, (UINT32)utf16Len, 0, 0, 0, 0, &count);
                        if (count > 0) {
                            std::vector<DWRITE_HIT_TEST_METRICS> m(count);
                            layout->HitTestTextRange((UINT32)utf16Start, (UINT32)utf16Len, 0, 0, &m[0], count, &count);
                            for (const auto& mm : m) {
                                float top = std::floor((mm.top + lineHeight * 0.5f) / lineHeight) * lineHeight;
                                rawRects.push_back(D2D1::RectF(mm.left, top, mm.left + mm.width, top + lineHeight));
                            }
                        }
                        for (size_t k = 0; k < subRange.size(); ++k) {
                            if (subRange[k] == '\n') {
                                std::string pre = text.substr(0, relS + k); UINT32 idx16 = (UINT32)UTF8ToW(pre).size();
                                DWRITE_HIT_TEST_METRICS m; FLOAT px, py;
                                layout->HitTestTextPosition(idx16, FALSE, &px, &py, &m);
                                float top = std::floor((m.top + lineHeight * 0.5f) / lineHeight) * lineHeight;
                                rawRects.push_back(D2D1::RectF(px - 0.5f, top, px + charWidth, top + lineHeight));
                            }
                        }
                    }
                }
            }
            std::sort(rawRects.begin(), rawRects.end(), [](const D2D1_RECT_F& a, const D2D1_RECT_F& b) {
                if (std::abs(a.top - b.top) > 1.0f) return a.top < b.top; return a.left < b.left;
                });
            std::vector<D2D1_RECT_F> mergedRects;
            if (!rawRects.empty()) {
                mergedRects.push_back(rawRects[0]);
                for (size_t i = 1; i < rawRects.size(); ++i) {
                    D2D1_RECT_F& curr = mergedRects.back(); const D2D1_RECT_F& next = rawRects[i];
                    bool sameLine = std::abs(curr.top - next.top) < 1.0f;
                    bool touches = next.left <= curr.right + 1.0f;
                    if (sameLine && touches) { curr.right = std::max(curr.right, next.right); curr.bottom = std::max(curr.bottom, next.bottom); }
                    else mergedRects.push_back(next);
                }
            }
            if (!mergedRects.empty()) {
                ID2D1RectangleGeometry* firstRect = nullptr; D2D1_RECT_F r = mergedRects[0];
                r.left += hInset; r.top += vInset; r.right -= hInset; r.bottom -= vInset;
                d2dFactory->CreateRectangleGeometry(&r, &firstRect); unifiedSelectionGeo = firstRect;
                for (size_t i = 1; i < mergedRects.size(); ++i) {
                    D2D1_RECT_F rNext = mergedRects[i];
                    rNext.left += hInset; rNext.top += vInset; rNext.right -= hInset; rNext.bottom -= vInset;
                    ID2D1RectangleGeometry* nextGeo = nullptr; d2dFactory->CreateRectangleGeometry(&rNext, &nextGeo);
                    ID2D1PathGeometry* pathGeo = nullptr; d2dFactory->CreatePathGeometry(&pathGeo);
                    ID2D1GeometrySink* sink = nullptr; pathGeo->Open(&sink);
                    unifiedSelectionGeo->CombineWithGeometry(nextGeo, D2D1_COMBINE_MODE_UNION, nullptr, sink);
                    sink->Close(); sink->Release();
                    unifiedSelectionGeo->Release(); unifiedSelectionGeo = pathGeo; nextGeo->Release();
                }
                if (unifiedSelectionGeo) {
                    rend->FillGeometry(unifiedSelectionGeo, selBrush);
                    rend->DrawGeometry(unifiedSelectionGeo, selBrush, 8.0f, roundJoinStyle);
                    unifiedSelectionGeo->Release();
                }
            }

            // Carets
            if (isDragMoving) {
                size_t relPos = (dragMoveDestPos > visibleStartOffset) ? dragMoveDestPos - visibleStartOffset : 0;
                if (relPos <= text.size()) {
                    std::string beforeCaret = text.substr(0, relPos); std::wstring wBefore = UTF8ToW(beforeCaret);
                    DWRITE_HIT_TEST_METRICS m; FLOAT px, py;
                    layout->HitTestTextPosition((UINT32)wBefore.size(), FALSE, &px, &py, &m);
                    rend->DrawLine(D2D1::Point2F(px, py), D2D1::Point2F(px, py + lineHeight), caretBrush, 2.0f);
                }
            }
            for (const auto& cursor : cursors) {
                size_t head = cursor.head;
                size_t relHead = (head > visibleStartOffset) ? head - visibleStartOffset : 0;
                if (hasIME && relHead >= caretOffsetInVisible) relHead += imeComp.size();
                if (relHead <= text.size()) {
                    std::string beforeCaret = text.substr(0, relHead); std::wstring wBefore = UTF8ToW(beforeCaret);
                    DWRITE_HIT_TEST_METRICS m; FLOAT px, py;
                    layout->HitTestTextPosition((UINT32)wBefore.size(), FALSE, &px, &py, &m);
                    rend->DrawLine(D2D1::Point2F(px, py), D2D1::Point2F(px, py + lineHeight), caretBrush);
                    if (&cursor == &cursors.back()) { imeCx = px; imeCy = py; }
                }
            }
            selBrush->Release(); caretBrush->Release(); hlBrush->Release();

            // Text
            ID2D1SolidColorBrush* brush = nullptr; rend->CreateSolidColorBrush(textColor, &brush);
            rend->DrawTextLayout(D2D1::Point2F(0, 0), layout, brush, D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
            brush->Release();

            // IME Dotted Line
            if (hasIME) {
                std::string prefixUtf8 = text.substr(0, caretOffsetInVisible); std::wstring prefixWide = UTF8ToW(prefixUtf8);
                UINT32 imeStart = (UINT32)prefixWide.size(); std::wstring imeCompWide = UTF8ToW(imeComp); UINT32 imeLen = (UINT32)imeCompWide.size();
                UINT32 count = 0; layout->HitTestTextRange(imeStart, imeLen, 0, 0, 0, 0, &count);
                if (count > 0) {
                    std::vector<DWRITE_HIT_TEST_METRICS> m(count); layout->HitTestTextRange(imeStart, imeLen, 0, 0, &m[0], count, &count);
                    ID2D1SolidColorBrush* underlineBrush = nullptr; rend->CreateSolidColorBrush(textColor, &underlineBrush);
                    for (const auto& mm : m) {
                        float x = mm.left; float y = std::floor(mm.top + mm.height - 2.0f) + 0.5f; float w = mm.width;
                        if (dotStyle) rend->DrawLine(D2D1::Point2F(x, y), D2D1::Point2F(x + w, y), underlineBrush, 1.5f, dotStyle);
                        else rend->DrawLine(D2D1::Point2F(x, y), D2D1::Point2F(x + w, y), underlineBrush, 1.0f);
                    }
                    underlineBrush->Release();
                }
            }
            layout->Release();
        }
        rend->SetTransform(D2D1::Matrix3x2F::Identity());

        // Gutter
        ID2D1SolidColorBrush* gutterBgBrush = nullptr; rend->CreateSolidColorBrush(gutterBg, &gutterBgBrush);
        rend->FillRectangle(D2D1::RectF(0, 0, gutterWidth, clientH), gutterBgBrush); gutterBgBrush->Release();
        ID2D1SolidColorBrush* gutterTextBrush = nullptr; rend->CreateSolidColorBrush(gutterText, &gutterTextBrush);
        int startLine = vScrollPos; int endLine = startLine + linesVisible;
        if (endLine > (int)lineStarts.size()) endLine = (int)lineStarts.size();
        for (int i = startLine; i < endLine; i++) {
            std::wstring numStr = std::to_wstring(i + 1); float yPos = (float)((i - startLine)) * lineHeight;
            IDWriteTextLayout* numLayout = nullptr;
            if (SUCCEEDED(dwFactory->CreateTextLayout(numStr.c_str(), (UINT32)numStr.size(), textFormat, gutterWidth, lineHeight, &numLayout))) {
                numLayout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
                rend->DrawTextLayout(D2D1::Point2F(0, yPos), numLayout, gutterTextBrush); numLayout->Release();
            }
        }
        gutterTextBrush->Release();

        // IME
        HIMC hIMC = ImmGetContext(hwnd);
        if (hIMC) {
            COMPOSITIONFORM cf = {}; cf.dwStyle = CFS_POINT; cf.ptCurrentPos.x = (LONG)(imeCx + gutterWidth - hScrollPos); cf.ptCurrentPos.y = (LONG)imeCy;
            ImmSetCompositionWindow(hIMC, &cf);
            CANDIDATEFORM cdf = {}; cdf.dwIndex = 0; cdf.dwStyle = CFS_CANDIDATEPOS; cdf.ptCurrentPos.x = (LONG)(imeCx + gutterWidth - hScrollPos); cdf.ptCurrentPos.y = (LONG)(imeCy + lineHeight);
            ImmSetCandidateWindow(hIMC, &cdf); ImmReleaseContext(hwnd, hIMC);
        }

        // Zoom Overlay
        if (GetTickCount() < zoomPopupEndTime) {
            D2D1_RECT_F popupRect = D2D1::RectF(clientW / 2 - 80, clientH / 2 - 40, clientW / 2 + 80, clientH / 2 + 40);
            ID2D1SolidColorBrush* popupBg = nullptr; rend->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.7f), &popupBg);
            ID2D1SolidColorBrush* popupText = nullptr; rend->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), &popupText);
            rend->FillRoundedRectangle(D2D1::RoundedRect(popupRect, 10.0f, 10.0f), popupBg);
            if (popupTextFormat) rend->DrawText(zoomPopupText.c_str(), (UINT32)zoomPopupText.size(), popupTextFormat, popupRect, popupText);
            popupBg->Release(); popupText->Release();
        }
        rend->EndDraw(); EndPaint(hwnd, &ps);
    }

    // ... (mergeCursors, etc. same as before) ...
    void mergeCursors() {
        if (cursors.empty()) return;
        std::sort(cursors.begin(), cursors.end(), [](const Cursor& a, const Cursor& b) { return a.head < b.head; });
        std::vector<Cursor> merged; merged.push_back(cursors[0]);
        for (size_t i = 1; i < cursors.size(); ++i) {
            Cursor& prev = merged.back(); Cursor& curr = cursors[i];
            if (curr.start() <= prev.end()) { size_t newStart = std::min(prev.start(), curr.start()); size_t newEnd = std::max(prev.end(), curr.end()); bool prevForward = prev.head >= prev.anchor; prev.anchor = prevForward ? newStart : newEnd; prev.head = prevForward ? newEnd : newStart; }
            else { merged.push_back(curr); }
        }
        cursors = merged;
    }
    void selectWordAt(size_t pos) {
        if (pos >= pt.length()) { cursors.clear(); cursors.push_back({ pos, pos, getXFromPos(pos) }); return; }
        char c = pt.charAt(pos); bool targetType = isWordChar(c);
        if (c == '\n') { cursors.clear(); cursors.push_back({ pos + 1, pos, getXFromPos(pos + 1) }); return; }
        size_t start = pos; while (start > 0) { char p = pt.charAt(start - 1); if (isWordChar(p) != targetType || p == '\n') break; start--; }
        size_t end = pos; size_t len = pt.length(); while (end < len) { char p = pt.charAt(end); if (isWordChar(p) != targetType || p == '\n') break; end++; }
        cursors.clear(); cursors.push_back({ end, start, getXFromPos(end) });
    }
    void selectLineAt(size_t pos) {
        int lineIdx = getLineIdx(pos); size_t start = lineStarts[lineIdx]; size_t end = (lineIdx + 1 < (int)lineStarts.size()) ? lineStarts[lineIdx + 1] : pt.length();
        cursors.clear(); cursors.push_back({ end, start, getXFromPos(end) });
    }
    size_t moveWordLeft(size_t pos) {
        if (pos == 0) return 0; size_t curr = pos;
        if (curr > 0 && pt.charAt(curr - 1) == '\n') return curr - 1;
        while (curr > 0) { char c = pt.charAt(curr - 1); if (c == '\n' || !isspace(c)) break; curr--; }
        if (curr == 0) return 0; if (pt.charAt(curr - 1) == '\n') return curr;
        bool type = isWordChar(pt.charAt(curr - 1));
        while (curr > 0) { char c = pt.charAt(curr - 1); if (c == '\n' || isspace(c) || isWordChar(c) != type) break; curr--; }
        return curr;
    }
    size_t moveWordRight(size_t pos) {
        size_t len = pt.length(); if (pos >= len) return len; size_t curr = pos;
        if (pt.charAt(curr) == '\n') return curr + 1;
        if (!isspace(pt.charAt(curr))) {
            bool type = isWordChar(pt.charAt(curr));
            while (curr < len) { char c = pt.charAt(curr); if (c == '\n' || isspace(c) || isWordChar(c) != type) break; curr++; }
        }
        while (curr < len) { char c = pt.charAt(curr); if (c == '\n' || !isspace(c)) break; curr++; }
        return curr;
    }
    size_t moveCaretVisual(size_t pos, bool forward) {
        size_t len = pt.length(); if (pos == 0 && !forward) return 0; if (pos >= len && forward) return len;
        char c = pt.charAt(pos); if (forward) { if (c == '\n') return pos + 1; }
        else { if (pos > 0 && pt.charAt(pos - 1) == '\n') return pos - 1; }
        int lineIdx = getLineIdx(pos); size_t lineStart = lineStarts[lineIdx];
        size_t nextLineStart = (lineIdx + 1 < (int)lineStarts.size()) ? lineStarts[lineIdx + 1] : len;
        size_t lineEnd = nextLineStart; if (lineEnd > lineStart && pt.charAt(lineEnd - 1) == '\n') lineEnd--;
        if (pos < lineStart || pos > lineEnd) return forward ? std::min(pos + 1, len) : std::max(pos - 1, (size_t)0);
        std::string lineUtf8 = pt.getRange(lineStart, lineEnd - lineStart); std::wstring lineUtf16 = UTF8ToW(lineUtf8);
        size_t offsetInLine = pos - lineStart; std::string preUtf8 = lineUtf8.substr(0, offsetInLine); size_t u16Pos = UTF8ToW(preUtf8).length();
        IDWriteTextLayout* layout = nullptr;
        HRESULT hr = dwFactory->CreateTextLayout(lineUtf16.c_str(), (UINT32)lineUtf16.length(), textFormat, 10000.0f, (FLOAT)lineHeight, &layout);
        size_t newU16Pos = u16Pos;
        if (SUCCEEDED(hr) && layout) {
            UINT32 clusterCount = 0; layout->GetClusterMetrics(NULL, 0, &clusterCount);
            if (clusterCount > 0) {
                std::vector<DWRITE_CLUSTER_METRICS> clusters(clusterCount); layout->GetClusterMetrics(clusters.data(), clusterCount, &clusterCount);
                size_t currentU16 = 0; bool found = false;
                if (forward) {
                    for (const auto& cm : clusters) {
                        size_t nextU16 = currentU16 + cm.length;
                        if (u16Pos >= currentU16 && u16Pos < nextU16) { newU16Pos = nextU16; found = true; break; }
                        currentU16 = nextU16;
                    }
                    if (!found) newU16Pos = u16Pos;
                }
                else {
                    size_t currentU16 = 0;
                    for (const auto& cm : clusters) {
                        size_t nextU16 = currentU16 + cm.length;
                        if (u16Pos > currentU16 && u16Pos <= nextU16) { newU16Pos = currentU16; found = true; break; }
                        currentU16 = nextU16;
                    }
                    if (!found && u16Pos > 0) { if (u16Pos == lineUtf16.length()) { size_t c = 0; for (const auto& cm : clusters) { if (c + cm.length == u16Pos) { newU16Pos = c; break; } c += cm.length; } } }
                }
            }
            layout->Release();
        }
        if (newU16Pos != u16Pos) {
            std::wstring preNewW = lineUtf16.substr(0, newU16Pos); size_t newOffset = WToUTF8(preNewW).length();
            return lineStart + newOffset;
        }
        return forward ? std::min(pos + 1, len) : std::max(pos - 1, (size_t)0);
    }
    // ... (rest of edit methods same as before) ...
    void insertAtCursors(const std::string& text) {
        commitPadding(); if (cursors.empty()) return;
        EditBatch batch; batch.beforeCursors = cursors;
        std::vector<int> indices(cursors.size()); for (size_t i = 0; i < cursors.size(); ++i) indices[i] = (int)i;
        std::sort(indices.begin(), indices.end(), [&](int a, int b) { return cursors[a].start() > cursors[b].start(); });
        for (int idx : indices) {
            Cursor& c = cursors[idx];
            if (c.hasSelection()) {
                size_t start = c.start(); size_t len = c.end() - start; std::string deleted = pt.getRange(start, len); pt.erase(start, len);
                batch.ops.push_back({ EditOp::Erase, start, deleted });
                for (auto& other : cursors) { if (other.head > start) other.head -= len; if (other.anchor > start) other.anchor -= len; }
                c.head = start; c.anchor = start;
            }
        }
        for (int idx : indices) {
            Cursor& c = cursors[idx]; size_t insertionPos = c.head; pt.insert(insertionPos, text);
            batch.ops.push_back({ EditOp::Insert, insertionPos, text });
            size_t len = text.size(); for (auto& other : cursors) { if (other.head >= insertionPos) other.head += len; if (other.anchor >= insertionPos) other.anchor += len; }
        }
        batch.afterCursors = cursors; undo.push(batch); rebuildLineStarts(); ensureCaretVisible(); updateDirtyFlag();
    }
    void deleteForwardAtCursors() {
        commitPadding(); if (cursors.empty()) return;
        EditBatch batch; batch.beforeCursors = cursors;
        std::vector<int> indices(cursors.size()); for (size_t i = 0; i < cursors.size(); ++i) indices[i] = (int)i;
        std::sort(indices.begin(), indices.end(), [&](int a, int b) { return cursors[a].start() > cursors[b].start(); });
        for (int idx : indices) {
            Cursor& c = cursors[idx]; size_t start = c.start(); size_t len = 0;
            if (c.hasSelection()) len = c.end() - start;
            else { size_t nextPos = moveCaretVisual(start, true); if (nextPos > start) len = nextPos - start; }
            if (len > 0 && start + len <= pt.length()) {
                std::string deleted = pt.getRange(start, len); pt.erase(start, len);
                batch.ops.push_back({ EditOp::Erase, start, deleted });
                for (auto& other : cursors) { if (other.head > start) other.head -= len; if (other.anchor > start) other.anchor -= len; }
                c.head = start; c.anchor = start;
            }
        }
        batch.afterCursors = cursors; undo.push(batch); rebuildLineStarts(); ensureCaretVisible(); updateDirtyFlag();
    }
    void backspaceAtCursors() {
        commitPadding(); if (cursors.empty()) return;
        EditBatch batch; batch.beforeCursors = cursors;
        std::vector<int> indices(cursors.size()); for (size_t i = 0; i < cursors.size(); ++i) indices[i] = (int)i;
        std::sort(indices.begin(), indices.end(), [&](int a, int b) { return cursors[a].start() > cursors[b].start(); });
        for (int idx : indices) {
            Cursor& c = cursors[idx]; size_t start = c.start(); size_t len = 0;
            if (c.hasSelection()) len = c.end() - start;
            else if (start > 0) {
                size_t prevPos = moveCaretVisual(start, false);
                if (prevPos < start) { len = start - prevPos; start = prevPos; }
            }
            if (len > 0) {
                std::string deleted = pt.getRange(start, len); pt.erase(start, len);
                batch.ops.push_back({ EditOp::Erase, start, deleted });
                for (auto& other : cursors) { if (other.head > start) other.head -= len; if (other.anchor > start) other.anchor -= len; }
            }
        }
        batch.afterCursors = cursors; undo.push(batch); rebuildLineStarts(); ensureCaretVisible(); updateDirtyFlag();
    }
    void copyToClipboard() {
        std::string text = ""; std::vector<Cursor> sorted = cursors;
        std::sort(sorted.begin(), sorted.end(), [](const Cursor& a, const Cursor& b) { return a.start() < b.start(); });
        for (size_t i = 0; i < sorted.size(); ++i) {
            const auto& c = sorted[i]; if (c.hasSelection()) text += pt.getRange(c.start(), c.end() - c.start());
            if (i < sorted.size() - 1 && c.hasSelection()) text += "\r\n";
        }
        if (text.empty()) return;
        if (OpenClipboard(hwnd)) {
            EmptyClipboard(); std::wstring wText = UTF8ToW(text);
            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (wText.size() + 1) * sizeof(wchar_t));
            if (hMem) { void* ptr = GlobalLock(hMem); memcpy(ptr, wText.c_str(), (wText.size() + 1) * sizeof(wchar_t)); GlobalUnlock(hMem); SetClipboardData(CF_UNICODETEXT, hMem); }
            CloseClipboard();
        }
    }
    void pasteFromClipboard() {
        if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) return;
        if (OpenClipboard(hwnd)) {
            HGLOBAL hMem = GetClipboardData(CF_UNICODETEXT);
            if (hMem) { const wchar_t* ptr = (const wchar_t*)GlobalLock(hMem); if (ptr) { std::wstring wText(ptr); GlobalUnlock(hMem); insertAtCursors(WToUTF8(wText)); } }
            CloseClipboard();
        }
    }
    void cutToClipboard() { copyToClipboard(); insertAtCursors(""); }
    void doInsert(size_t pos, const std::string& s) { cursors.clear(); cursors.push_back({ pos, pos, getXFromPos(pos) }); insertAtCursors(s); }
    void performUndo() {
        if (!undo.canUndo()) return; EditBatch b = undo.popUndo();
        for (int i = (int)b.ops.size() - 1; i >= 0; --i) { const auto& op = b.ops[i]; if (op.type == EditOp::Insert) pt.erase(op.pos, op.text.size()); else pt.insert(op.pos, op.text); }
        cursors = b.beforeCursors; rebuildLineStarts(); ensureCaretVisible(); updateDirtyFlag();
    }
    void performRedo() {
        if (!undo.canRedo()) return; EditBatch b = undo.popRedo();
        for (const auto& op : b.ops) { if (op.type == EditOp::Insert) pt.insert(op.pos, op.text); else pt.erase(op.pos, op.text.size()); }
        cursors = b.afterCursors; rebuildLineStarts(); ensureCaretVisible(); updateDirtyFlag();
    }
    int ShowTaskDialog(const wchar_t* title, const wchar_t* instruction, const wchar_t* content, TASKDIALOG_COMMON_BUTTON_FLAGS buttons, PCWSTR icon) {
        TASKDIALOGCONFIG config = { 0 }; config.cbSize = sizeof(config); config.hwndParent = hwnd; config.hInstance = GetModuleHandle(NULL);
        config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW;
        config.pszWindowTitle = title; config.pszMainInstruction = instruction; config.pszContent = content; config.dwCommonButtons = buttons; config.pszMainIcon = icon;
        int nButtonPressed = 0; TaskDialogIndirect(&config, &nButtonPressed, NULL, NULL); return nButtonPressed;
    }
    bool checkUnsavedChanges() {
        if (!isDirty) return true;
        int result = ShowTaskDialog(L"確認", L"変更を保存しますか?", currentFilePath.empty() ? L"無題のファイルへの変更内容を保存しますか?" : (L"ファイル '" + currentFilePath + L"' への変更内容を保存しますか?").c_str(), TDCBF_YES_BUTTON | TDCBF_NO_BUTTON | TDCBF_CANCEL_BUTTON, TD_WARNING_ICON);
        if (result == IDCANCEL) return false;
        if (result == IDYES) { if (currentFilePath.empty()) return saveFileAs(); else return saveFile(currentFilePath); }
        return true;
    }
    bool openFile() {
        if (!checkUnsavedChanges()) return false;
        WCHAR szFile[MAX_PATH] = { 0 }; OPENFILENAMEW ofn = { 0 }; ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = hwnd; ofn.lpstrFile = szFile; ofn.nMaxFile = sizeof(szFile); ofn.lpstrFilter = L"All Files\0*.*\0Text Files\0*.txt\0"; ofn.nFilterIndex = 1; ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
        if (GetOpenFileNameW(&ofn) == TRUE) {
            fileMap.reset(new MappedFile());
            if (fileMap->open(szFile)) {
                pt.initFromFile(fileMap->ptr, fileMap->size); currentFilePath = szFile; undo.clear(); isDirty = false; undo.markSaved(); cursors.clear(); cursors.push_back({ 0, 0, 0.0f }); vScrollPos = 0; hScrollPos = 0; rebuildLineStarts(); updateTitleBar(); InvalidateRect(hwnd, NULL, FALSE); return true;
            }
            else ShowTaskDialog(L"エラー", L"ファイルを開けませんでした。", szFile, TDCBF_OK_BUTTON, TD_ERROR_ICON);
        }
        return false;
    }
    bool saveFile(const std::wstring& path) {
        std::wstring tempPath = path + L".tmp";
        HANDLE hTemp = CreateFileW(tempPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hTemp == INVALID_HANDLE_VALUE) { ShowTaskDialog(L"エラー", L"一時ファイルを作成できませんでした。", tempPath.c_str(), TDCBF_OK_BUTTON, TD_ERROR_ICON); return false; }
        bool success = true;
        for (const auto& p : pt.pieces) {
            const char* dataPtr = p.isOriginal ? (pt.origPtr + p.start) : (pt.addBuf.data() + p.start);
            DWORD written = 0; if (!WriteFile(hTemp, dataPtr, (DWORD)p.len, &written, NULL) || written != p.len) { success = false; break; }
        }
        CloseHandle(hTemp);
        if (!success) { DeleteFileW(tempPath.c_str()); ShowTaskDialog(L"エラー", L"書き込みに失敗しました。", path.c_str(), TDCBF_OK_BUTTON, TD_ERROR_ICON); return false; }
        if (MoveFileExW(tempPath.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED) == 0) { ShowTaskDialog(L"エラー", L"ファイルを上書きできませんでした。", L"ファイルが他のプロセスによってロックされている可能性があります。", TDCBF_OK_BUTTON, TD_ERROR_ICON); DeleteFileW(tempPath.c_str()); return false; }
        currentFilePath = path; undo.markSaved(); updateDirtyFlag(); return true;
    }
    bool saveFileAs() {
        WCHAR szFile[MAX_PATH] = { 0 }; OPENFILENAMEW ofn = { 0 }; ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = hwnd; ofn.lpstrFile = szFile; ofn.nMaxFile = sizeof(szFile); ofn.lpstrFilter = L"All Files\0*.*\0Text Files\0*.txt\0"; ofn.nFilterIndex = 1; ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
        if (GetSaveFileNameW(&ofn) == TRUE) return saveFile(szFile); return false;
    }
    void newFile() {
        if (!checkUnsavedChanges()) return;
        pt.initEmpty(); currentFilePath.clear(); undo.clear(); isDirty = false; cursors.clear(); cursors.push_back({ 0, 0, 0.0f }); vScrollPos = 0; hScrollPos = 0; fileMap.reset(); rebuildLineStarts(); updateTitleBar(); InvalidateRect(hwnd, NULL, FALSE);
    }
} g_editor;

// --- Win32 Boilerplate ---
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: g_editor.initGraphics(hwnd); break;
    case WM_SIZE: if (g_editor.rend) { RECT rc; GetClientRect(hwnd, &rc); D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top); g_editor.rend->Resize(size); g_editor.updateScrollBars(); InvalidateRect(hwnd, NULL, FALSE); } break;
    case WM_LBUTTONDOWN: {
        int x = (short)LOWORD(lParam); int y = (short)HIWORD(lParam); SetCapture(hwnd); g_editor.isDragging = true; g_editor.rollbackPadding();
        DWORD curTime = GetMessageTime(); bool samePos = (abs(x - g_editor.lastClickX) < 5 && abs(y - g_editor.lastClickY) < 5);
        if (samePos && (curTime - g_editor.lastClickTime < GetDoubleClickTime())) g_editor.clickCount++; else g_editor.clickCount = 1;
        g_editor.lastClickTime = curTime; g_editor.lastClickX = x; g_editor.lastClickY = y;
        float dipX = x / g_editor.dpiScaleX; float dipY = y / g_editor.dpiScaleY;
        if (g_editor.clickCount == 1 && !(GetKeyState(VK_SHIFT) & 0x8000) && !(GetKeyState(VK_MENU) & 0x8000) && !(GetKeyState(VK_CONTROL) & 0x8000)) {
            size_t clickedPos = g_editor.getDocPosFromPoint(x, y);
            bool insideSelection = false; size_t selStart = 0, selEnd = 0;
            for (const auto& c : g_editor.cursors) { if (c.hasSelection() && clickedPos >= c.start() && clickedPos < c.end()) { insideSelection = true; selStart = c.start(); selEnd = c.end(); break; } }
            if (insideSelection) { g_editor.isDragMovePending = true; g_editor.dragMoveSourceStart = selStart; g_editor.dragMoveSourceEnd = selEnd; return 0; }
        }
        g_editor.isDragMovePending = false; g_editor.isDragMoving = false;
        if (GetKeyState(VK_MENU) & 0x8000) {
            g_editor.isRectSelecting = true; float virtualX = dipX - g_editor.gutterWidth + g_editor.hScrollPos; float virtualY = dipY + (g_editor.vScrollPos * g_editor.lineHeight);
            g_editor.rectAnchorX = virtualX; g_editor.rectAnchorY = virtualY; g_editor.rectHeadX = virtualX; g_editor.rectHeadY = virtualY;
            g_editor.updateRectSelection(); InvalidateRect(hwnd, NULL, FALSE); return 0;
        }
        else g_editor.isRectSelecting = false;
        if (dipX < g_editor.gutterWidth) {
            int clickedLine = g_editor.vScrollPos + (int)(dipY / g_editor.lineHeight);
            if (clickedLine >= 0 && clickedLine < (int)g_editor.lineStarts.size()) {
                size_t start = g_editor.lineStarts[clickedLine]; size_t end = (clickedLine + 1 < (int)g_editor.lineStarts.size()) ? g_editor.lineStarts[clickedLine + 1] : g_editor.pt.length();
                if (GetKeyState(VK_SHIFT) & 0x8000 && !g_editor.cursors.empty()) { g_editor.cursors.clear(); g_editor.cursors.push_back({ end, start, g_editor.getXFromPos(end) }); }
                else { g_editor.cursors.clear(); g_editor.cursors.push_back({ end, start, g_editor.getXFromPos(end) }); }
                InvalidateRect(hwnd, NULL, FALSE);
            }
        }
        else {
            size_t clickedPos = g_editor.getDocPosFromPoint(x, y);
            if (g_editor.clickCount == 2) g_editor.selectWordAt(clickedPos);
            else if (g_editor.clickCount == 3) g_editor.selectLineAt(clickedPos);
            else {
                if (GetKeyState(VK_SHIFT) & 0x8000) { if (!g_editor.cursors.empty()) { g_editor.cursors.back().head = clickedPos; g_editor.cursors.back().desiredX = g_editor.getXFromPos(clickedPos); } }
                else if (GetKeyState(VK_CONTROL) & 0x8000) g_editor.cursors.push_back({ clickedPos, clickedPos, g_editor.getXFromPos(clickedPos) });
                else { g_editor.cursors.clear(); g_editor.cursors.push_back({ clickedPos, clickedPos, g_editor.getXFromPos(clickedPos) }); }
            }
            InvalidateRect(hwnd, NULL, FALSE);
        }
    } break;
    case WM_MOUSEMOVE: {
        int x = (short)LOWORD(lParam); int y = (short)HIWORD(lParam); float dipX = x / g_editor.dpiScaleX; float dipY = y / g_editor.dpiScaleY;
        if (g_editor.isDragMovePending) { if (abs(x - g_editor.lastClickX) > 5 || abs(y - g_editor.lastClickY) > 5) { g_editor.isDragMovePending = false; g_editor.isDragMoving = true; SetCursor(LoadCursor(NULL, IDC_ARROW)); } }
        if (g_editor.isDragMoving) { g_editor.dragMoveDestPos = g_editor.getDocPosFromPoint(x, y); InvalidateRect(hwnd, NULL, FALSE); return 0; }
        if (g_editor.isDragging && !g_editor.isDragMoving && !g_editor.isDragMovePending) {
            if (g_editor.isRectSelecting) { float virtualX = dipX - g_editor.gutterWidth + g_editor.hScrollPos; float virtualY = dipY + (g_editor.vScrollPos * g_editor.lineHeight); g_editor.rectHeadX = virtualX; g_editor.rectHeadY = virtualY; g_editor.updateRectSelection(); }
            else { size_t pos = g_editor.getDocPosFromPoint(x, y); if (!g_editor.cursors.empty()) { g_editor.cursors.back().head = pos; g_editor.cursors.back().desiredX = g_editor.getXFromPos(pos); } }
            InvalidateRect(hwnd, NULL, FALSE);
        }
    } break;
    case WM_LBUTTONUP: {
        if (g_editor.isDragMovePending) {
            g_editor.isDragMovePending = false; size_t clickedPos = g_editor.getDocPosFromPoint((short)LOWORD(lParam), (short)HIWORD(lParam));
            g_editor.cursors.clear(); g_editor.cursors.push_back({ clickedPos, clickedPos, g_editor.getXFromPos(clickedPos) });
            InvalidateRect(hwnd, NULL, FALSE); g_editor.isDragging = false; ReleaseCapture();
        }
        else if (g_editor.isDragMoving) { g_editor.performDragMove(); g_editor.isDragMoving = false; g_editor.isDragging = false; ReleaseCapture(); }
        else if (g_editor.isDragging) { ReleaseCapture(); g_editor.isDragging = false; g_editor.isRectSelecting = false; g_editor.mergeCursors(); }
    } break;
    case WM_VSCROLL: {
        int action = LOWORD(wParam); RECT rc; GetClientRect(hwnd, &rc); int page = (int)((rc.bottom / g_editor.dpiScaleY) / g_editor.lineHeight); int maxPos = (int)g_editor.lineStarts.size();
    switch (action) { case SB_LINEUP: g_editor.vScrollPos -= 1; break; case SB_LINEDOWN: g_editor.vScrollPos += 1; break; case SB_PAGEUP: g_editor.vScrollPos -= page; break; case SB_PAGEDOWN: g_editor.vScrollPos += page; break; case SB_THUMBTRACK: SCROLLINFO si; si.cbSize = sizeof(si); si.fMask = SIF_TRACKPOS; GetScrollInfo(hwnd, SB_VERT, &si); g_editor.vScrollPos = si.nTrackPos; break; }
                                    if (g_editor.vScrollPos < 0) g_editor.vScrollPos = 0; if (g_editor.vScrollPos > maxPos) g_editor.vScrollPos = maxPos; g_editor.updateScrollBars(); InvalidateRect(hwnd, NULL, FALSE);
    } break;
    case WM_HSCROLL: {
        int action = LOWORD(wParam); int page = 100;
    switch (action) { case SB_LINELEFT: g_editor.hScrollPos -= 10; break; case SB_LINERIGHT: g_editor.hScrollPos += 10; break; case SB_PAGELEFT: g_editor.hScrollPos -= page; break; case SB_PAGERIGHT: g_editor.hScrollPos += page; break; case SB_THUMBTRACK: SCROLLINFO si; si.cbSize = sizeof(si); si.fMask = SIF_TRACKPOS; GetScrollInfo(hwnd, SB_HORZ, &si); g_editor.hScrollPos = si.nTrackPos; break; }
                                      if (g_editor.hScrollPos < 0) g_editor.hScrollPos = 0; if (g_editor.hScrollPos > (int)g_editor.maxLineWidth) g_editor.hScrollPos = (int)g_editor.maxLineWidth; g_editor.updateScrollBars(); InvalidateRect(hwnd, NULL, FALSE);
    } break;
    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        if (GET_KEYSTATE_WPARAM(wParam) & MK_CONTROL) {
            float oldSize = g_editor.currentFontSize; float scale = (delta > 0) ? 1.1f : 0.9f; g_editor.updateFont(oldSize * scale);
            g_editor.zoomPopupEndTime = GetTickCount() + 1000; std::wstringstream ss; ss << (int)g_editor.currentFontSize << L"px"; g_editor.zoomPopupText = ss.str(); SetTimer(hwnd, 1, 1000, NULL); InvalidateRect(hwnd, NULL, FALSE);
        }
        else {
            int lines = -delta / WHEEL_DELTA * 3; g_editor.vScrollPos += lines; int maxPos = (int)g_editor.lineStarts.size();
            if (g_editor.vScrollPos < 0) g_editor.vScrollPos = 0; if (g_editor.vScrollPos > maxPos) g_editor.vScrollPos = maxPos;
            g_editor.updateScrollBars(); InvalidateRect(hwnd, NULL, FALSE);
        }
    } break;
    case WM_TIMER: { if (wParam == 1) { KillTimer(hwnd, 1); InvalidateRect(hwnd, NULL, FALSE); } } break;
    case WM_CHAR: {
        wchar_t ch = (wchar_t)wParam; if (ch == 3 || ch == 26 || ch == 1 || ch == 15 || ch == 19 || ch == 14 || ch == 6) break; // Skip Ctrl+C,Z,A,O,S,N,F
        if (ch == 8) { g_editor.highSurrogate = 0; g_editor.backspaceAtCursors(); }
        else if (ch == '\r') { g_editor.highSurrogate = 0; g_editor.insertAtCursors("\n"); }
        else if (ch >= 32) {
            if (ch >= 0xD800 && ch <= 0xDBFF) { g_editor.highSurrogate = ch; return 0; }
            std::wstring ws; if (ch >= 0xDC00 && ch <= 0xDFFF) { if (g_editor.highSurrogate) { ws.push_back(g_editor.highSurrogate); ws.push_back(ch); g_editor.highSurrogate = 0; } else return 0; }
            else { g_editor.highSurrogate = 0; ws.push_back(ch); }
            g_editor.insertAtCursors(WToUTF8(ws));
        }
    } break;
    case WM_IME_STARTCOMPOSITION: return 0;
    case WM_IME_COMPOSITION: {
        HIMC hIMC = ImmGetContext(hwnd);
        if (hIMC) {
            if (lParam & GCS_RESULTSTR) { DWORD size = ImmGetCompositionStringW(hIMC, GCS_RESULTSTR, NULL, 0); if (size > 0) { std::vector<wchar_t> buf(size / sizeof(wchar_t)); ImmGetCompositionStringW(hIMC, GCS_RESULTSTR, buf.data(), size); std::wstring ws(buf.begin(), buf.end()); g_editor.insertAtCursors(WToUTF8(ws)); g_editor.imeComp.clear(); } }
            if (lParam & GCS_COMPSTR) { DWORD size = ImmGetCompositionStringW(hIMC, GCS_COMPSTR, NULL, 0); if (size > 0) { std::vector<wchar_t> buf(size / sizeof(wchar_t)); ImmGetCompositionStringW(hIMC, GCS_COMPSTR, buf.data(), size); std::wstring ws(buf.begin(), buf.end()); g_editor.imeComp = WToUTF8(ws); } else { g_editor.imeComp.clear(); } }
            ImmReleaseContext(hwnd, hIMC); InvalidateRect(hwnd, NULL, FALSE); return 0;
        }
    } break;
    case WM_IME_ENDCOMPOSITION: g_editor.imeComp.clear(); InvalidateRect(hwnd, NULL, FALSE); break;
    case WM_IME_SETCONTEXT: lParam &= ~ISC_SHOWUICOMPOSITIONWINDOW; return DefWindowProc(hwnd, msg, wParam, lParam);
    case WM_SYSKEYDOWN: if (wParam != VK_LEFT && wParam != VK_RIGHT && wParam != VK_UP && wParam != VK_DOWN) return DefWindowProc(hwnd, msg, wParam, lParam);
    case WM_KEYDOWN:
        if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'F') g_editor.showFindDialog();
        else if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'O') g_editor.openFile();
        else if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'N') g_editor.newFile();
        else if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'S') { if (g_editor.currentFilePath.empty()) g_editor.saveFileAs(); else g_editor.saveFile(g_editor.currentFilePath); }
        else if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'Z') g_editor.performUndo();
        else if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'Y') g_editor.performRedo();
        else if ((GetKeyState(VK_CONTROL) & 0x8000) && (wParam == 'C' || wParam == VK_INSERT)) g_editor.copyToClipboard();
        else if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'X') g_editor.cutToClipboard();
        else if (((GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'V') || ((GetKeyState(VK_SHIFT) & 0x8000) && wParam == VK_INSERT)) g_editor.pasteFromClipboard();
        else if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'A') { g_editor.rollbackPadding(); g_editor.cursors.clear(); g_editor.cursors.push_back({ g_editor.pt.length(), 0, 0.0f }); InvalidateRect(hwnd, NULL, FALSE); }
        else if (wParam == VK_ESCAPE) { g_editor.rollbackPadding(); if (!g_editor.cursors.empty()) { Cursor last = g_editor.cursors.back(); last.anchor = last.head; g_editor.cursors.clear(); g_editor.cursors.push_back(last); g_editor.isRectSelecting = false; InvalidateRect(hwnd, NULL, FALSE); } }
        else if (wParam == VK_LEFT) {
            bool isAlt = (GetKeyState(VK_MENU) & 0x8000) != 0; bool isShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0; bool isCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            if (isAlt && isShift) {
                if (!g_editor.isRectSelecting) { g_editor.isRectSelecting = true; float x, y; g_editor.getCaretPoint(x, y); float contentX = (x / g_editor.dpiScaleX - g_editor.gutterWidth + g_editor.hScrollPos); float contentY = (y / g_editor.dpiScaleY + (g_editor.vScrollPos * g_editor.lineHeight)); g_editor.rectAnchorX = g_editor.rectHeadX = contentX; g_editor.rectAnchorY = g_editor.rectHeadY = contentY; }
                g_editor.rectHeadX -= g_editor.charWidth; g_editor.updateRectSelection(); InvalidateRect(hwnd, NULL, FALSE); return 0;
            }
            g_editor.rollbackPadding(); g_editor.isRectSelecting = false;
            for (auto& c : g_editor.cursors) {
                if (c.hasSelection() && !isShift) { c.head = c.start(); c.anchor = c.head; }
                else { if (isCtrl) c.head = g_editor.moveWordLeft(c.head); else c.head = g_editor.moveCaretVisual(c.head, false); if (!isShift) c.anchor = c.head; }
                c.desiredX = g_editor.getXFromPos(c.head);
            }
            g_editor.mergeCursors(); g_editor.ensureCaretVisible();
        }
        else if (wParam == VK_RIGHT) {
            bool isAlt = (GetKeyState(VK_MENU) & 0x8000) != 0; bool isShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0; bool isCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            if (isAlt && isShift) {
                if (!g_editor.isRectSelecting) { g_editor.isRectSelecting = true; float x, y; g_editor.getCaretPoint(x, y); float contentX = (x / g_editor.dpiScaleX - g_editor.gutterWidth + g_editor.hScrollPos); float contentY = (y / g_editor.dpiScaleY + (g_editor.vScrollPos * g_editor.lineHeight)); g_editor.rectAnchorX = g_editor.rectHeadX = contentX; g_editor.rectAnchorY = g_editor.rectHeadY = contentY; }
                g_editor.rectHeadX += g_editor.charWidth; g_editor.updateRectSelection(); InvalidateRect(hwnd, NULL, FALSE); return 0;
            }
            g_editor.rollbackPadding(); g_editor.isRectSelecting = false;
            for (auto& c : g_editor.cursors) {
                if (c.hasSelection() && !isShift) { c.head = c.end(); c.anchor = c.head; }
                else { if (isCtrl) c.head = g_editor.moveWordRight(c.head); else c.head = g_editor.moveCaretVisual(c.head, true); if (!isShift) c.anchor = c.head; }
                c.desiredX = g_editor.getXFromPos(c.head);
            }
            g_editor.mergeCursors(); g_editor.ensureCaretVisible();
        }
        else if (wParam == VK_UP) {
            bool isAlt = (GetKeyState(VK_MENU) & 0x8000) != 0; bool isShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            if (isAlt && isShift) {
                if (!g_editor.isRectSelecting) { g_editor.isRectSelecting = true; float x, y; g_editor.getCaretPoint(x, y); float contentX = (x / g_editor.dpiScaleX - g_editor.gutterWidth + g_editor.hScrollPos); float contentY = (y / g_editor.dpiScaleY + (g_editor.vScrollPos * g_editor.lineHeight)); g_editor.rectAnchorX = g_editor.rectHeadX = contentX; g_editor.rectAnchorY = g_editor.rectHeadY = contentY; }
                g_editor.rectHeadY -= g_editor.lineHeight; g_editor.updateRectSelection(); InvalidateRect(hwnd, NULL, FALSE); return 0;
            }
            g_editor.rollbackPadding(); g_editor.isRectSelecting = false;
            for (auto& c : g_editor.cursors) { int line = g_editor.getLineIdx(c.head); if (line > 0) c.head = g_editor.getPosFromLineAndX(line - 1, c.desiredX); if (!(GetKeyState(VK_SHIFT) & 0x8000)) c.anchor = c.head; }
            g_editor.mergeCursors(); g_editor.ensureCaretVisible();
        }
        else if (wParam == VK_DOWN) {
            bool isAlt = (GetKeyState(VK_MENU) & 0x8000) != 0; bool isShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            if (isAlt && isShift) {
                if (!g_editor.isRectSelecting) { g_editor.isRectSelecting = true; float x, y; g_editor.getCaretPoint(x, y); float contentX = (x / g_editor.dpiScaleX - g_editor.gutterWidth + g_editor.hScrollPos); float contentY = (y / g_editor.dpiScaleY + (g_editor.vScrollPos * g_editor.lineHeight)); g_editor.rectAnchorX = g_editor.rectHeadX = contentX; g_editor.rectAnchorY = g_editor.rectHeadY = contentY; }
                g_editor.rectHeadY += g_editor.lineHeight; g_editor.updateRectSelection(); InvalidateRect(hwnd, NULL, FALSE); return 0;
            }
            g_editor.rollbackPadding(); g_editor.isRectSelecting = false;
            for (auto& c : g_editor.cursors) { int line = g_editor.getLineIdx(c.head); if (line + 1 < (int)g_editor.lineStarts.size()) c.head = g_editor.getPosFromLineAndX(line + 1, c.desiredX); if (!(GetKeyState(VK_SHIFT) & 0x8000)) c.anchor = c.head; }
            g_editor.mergeCursors(); g_editor.ensureCaretVisible();
        }
        else if (wParam == VK_HOME) {
            g_editor.rollbackPadding(); g_editor.isRectSelecting = false;
            if (GetKeyState(VK_CONTROL) & 0x8000) { size_t target = 0; size_t anchor = target; if (!g_editor.cursors.empty() && (GetKeyState(VK_SHIFT) & 0x8000)) anchor = g_editor.cursors.back().anchor; g_editor.cursors.clear(); g_editor.cursors.push_back({ target, anchor, g_editor.getXFromPos(target) }); }
            else { for (auto& c : g_editor.cursors) { size_t pos = c.head; while (pos > 0 && g_editor.pt.charAt(pos - 1) != '\n') pos--; c.head = pos; if (!(GetKeyState(VK_SHIFT) & 0x8000)) c.anchor = c.head; c.desiredX = g_editor.getXFromPos(c.head); } g_editor.mergeCursors(); }
            g_editor.ensureCaretVisible();
        }
        else if (wParam == VK_END) {
            g_editor.rollbackPadding(); g_editor.isRectSelecting = false;
            if (GetKeyState(VK_CONTROL) & 0x8000) { size_t target = g_editor.pt.length(); size_t anchor = target; if (!g_editor.cursors.empty() && (GetKeyState(VK_SHIFT) & 0x8000)) anchor = g_editor.cursors.back().anchor; g_editor.cursors.clear(); g_editor.cursors.push_back({ target, anchor, g_editor.getXFromPos(target) }); }
            else { for (auto& c : g_editor.cursors) { size_t pos = c.head; size_t len = g_editor.pt.length(); while (pos < len && g_editor.pt.charAt(pos) != '\n') pos++; c.head = pos; if (!(GetKeyState(VK_SHIFT) & 0x8000)) c.anchor = c.head; c.desiredX = g_editor.getXFromPos(c.head); } g_editor.mergeCursors(); }
            g_editor.ensureCaretVisible();
        }
        else if (wParam == VK_DELETE) { g_editor.rollbackPadding(); g_editor.isRectSelecting = false; g_editor.deleteForwardAtCursors(); }
        else if (wParam == VK_PRIOR) { g_editor.rollbackPadding(); g_editor.isRectSelecting = false; RECT rc; GetClientRect(hwnd, &rc); int pageLines = (int)((rc.bottom / g_editor.dpiScaleY) / g_editor.lineHeight); for (auto& c : g_editor.cursors) { int currentLine = g_editor.getLineIdx(c.head); int targetLine = std::max(0, currentLine - pageLines); c.head = g_editor.getPosFromLineAndX(targetLine, c.desiredX); if (!(GetKeyState(VK_SHIFT) & 0x8000)) c.anchor = c.head; } g_editor.mergeCursors(); g_editor.ensureCaretVisible(); }
        else if (wParam == VK_NEXT) { g_editor.rollbackPadding(); g_editor.isRectSelecting = false; RECT rc; GetClientRect(hwnd, &rc); int pageLines = (int)((rc.bottom / g_editor.dpiScaleY) / g_editor.lineHeight); for (auto& c : g_editor.cursors) { int currentLine = g_editor.getLineIdx(c.head); int targetLine = std::min((int)g_editor.lineStarts.size() - 1, currentLine + pageLines); c.head = g_editor.getPosFromLineAndX(targetLine, c.desiredX); if (!(GetKeyState(VK_SHIFT) & 0x8000)) c.anchor = c.head; } g_editor.mergeCursors(); g_editor.ensureCaretVisible(); }
        break;
    case WM_CLOSE:
        if (g_editor.checkUnsavedChanges()) { DestroyWindow(hwnd); }
        return 0;
    case WM_PAINT: g_editor.render(); break;
    case WM_DESTROY: g_editor.destroyGraphics(); PostQuitMessage(0); break;
    default: return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int nCmdShow) {
    int argc; wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    const wchar_t* filePath = nullptr; if (argc >= 2) filePath = argv[1];
    WNDCLASS wc = {}; wc.lpfnWndProc = WndProc; wc.hInstance = hInstance; wc.lpszClassName = L"FastMiniEditorClass"; wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1)); wc.hCursor = LoadCursor(NULL, IDC_IBEAM); wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); RegisterClass(&wc);
    HWND hwnd = CreateWindowEx(0, wc.lpszClassName, L"FastMiniEditor - Minimal", WS_OVERLAPPEDWINDOW | WS_VSCROLL | WS_HSCROLL, CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, NULL, NULL, hInstance, NULL);
    if (!hwnd) return 0;
    ShowWindow(hwnd, nCmdShow);

    if (filePath) {
        g_editor.fileMap.reset(new MappedFile());
        if (g_editor.fileMap->open(filePath)) {
            g_editor.pt.initFromFile(g_editor.fileMap->ptr, g_editor.fileMap->size);
            g_editor.currentFilePath = filePath;
            g_editor.undo.clear(); g_editor.isDirty = false; g_editor.undo.markSaved();
            g_editor.cursors.clear(); g_editor.cursors.push_back({ 0, 0, 0.0f });
            g_editor.rebuildLineStarts(); g_editor.updateTitleBar();
        }
    }
    g_editor.updateTitleBar();

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_F3) {
            bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            g_editor.findNext(!shift);
            continue;
        }

        if (!g_editor.hFindDlg || !IsDialogMessage(g_editor.hFindDlg, &msg)) {
            TranslateMessage(&msg); DispatchMessage(&msg);
        }
    }
    LocalFree(argv); return 0;
}