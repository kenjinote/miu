// Source.cpp
// Minimal, high-performance text editor for huge files using Win32 + DirectWrite.
// Features: memory-mapped original file, piece table for edits, undo/redo, caret, basic input, fast visible-range rendering.

// Build (MSVC):
// rc miu.rc
// cl /std:c++17 /O2 /EHsc Source.cpp miu.res /link d2d1.lib dwrite.lib user32.lib ole32.lib imm32.lib comdlg32.lib comctl32.lib

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
#include <regex> 
#include "resource.h"

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "imm32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "comctl32.lib")

const std::wstring APP_VERSION = L"miu v1.0.4";
const std::wstring HELP_TEXT =
APP_VERSION + L"\n\n"
L"[Shortcuts]\n"
L"F1                  Help\n"
L"Ctrl+N              New\n"
L"Ctrl+O / Drag&Drop  Open\n"
L"Ctrl+S              Save\n"
L"Ctrl+Shift+S        Save As\n"
L"Ctrl+F              Find\n"
L"Ctrl+H              Replace\n"
L"F3                  Find Next\n"
L"Shift+F3            Find Prev\n"
L"Ctrl+Z              Undo\n"
L"Ctrl+Y              Redo\n"
L"Ctrl+X/C/V          Cut/Copy/Paste\n"
L"Ctrl+U              Upper Case\n"
L"Ctrl+Shift+U        Lower Case\n"
L"Alt+Up/Down         Move Line\n"
L"Alt+Shift+Up/Down   Copy Line\n"
L"Ctrl+D              Select Word / Next\n"
L"Ctrl+A              Select All\n"
L"Alt+Drag            Rect Select\n"
L"Ctrl+Wheel/+/-      Zoom\n"
L"Ctrl+0              Reset Zoom";

static std::wstring UTF8ToW(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), NULL, 0);
    if (n <= 0) return {};
    std::wstring w; w.resize(n);
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}
static std::string WToUTF8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), NULL, 0, NULL, NULL);
    if (n <= 0) return {};
    std::string s; s.resize(n);
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &s[0], n, NULL, NULL);
    return s;
}

static std::string UnescapeString(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            switch (s[i + 1]) {
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case '\\': out += '\\'; break;
            default: out += s[i]; out += s[i + 1]; break;
            }
            i++;
        }
        else {
            out += s[i];
        }
    }
    return out;
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
                pieces[idx] = { p.isOriginal, p.start, offsetInPiece };
                pieces.insert(pieces.begin() + idx + 1, { p.isOriginal, p.start + offsetInPiece, p.len - offsetInPiece });
                idx++;
            }
            else if (offsetInPiece == p.len) idx++;
        }
        else idx = pieces.size();
        size_t addStart = addBuf.size(); addBuf.append(s);
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
            Piece p = pieces[idx]; size_t leftLen = pos - cur;
            pieces[idx] = { p.isOriginal, p.start, leftLen };
            pieces.insert(pieces.begin() + idx + 1, { p.isOriginal, p.start + leftLen, p.len - leftLen });
            idx++;
        }
        while (idx < pieces.size() && remaining > 0) {
            if (pieces[idx].len <= remaining) { remaining -= pieces[idx].len; pieces.erase(pieces.begin() + idx); }
            else { pieces[idx].start += remaining; pieces[idx].len -= remaining; remaining = 0; }
        }
        coalesceAround(idx > 0 ? idx - 1 : 0);
    }
    void coalesceAround(size_t idx) {
        if (pieces.empty()) return;
        if (idx >= pieces.size()) idx = pieces.size() - 1;
        if (idx > 0) {
            Piece& a = pieces[idx - 1]; Piece& b = pieces[idx];
            if (!a.isOriginal && !b.isOriginal && (a.start + a.len == b.start)) { a.len += b.len; pieces.erase(pieces.begin() + idx); idx--; }
        }
        if (idx + 1 < pieces.size()) {
            Piece& a = pieces[idx]; Piece& b = pieces[idx + 1];
            if (!a.isOriginal && !b.isOriginal && (a.start + a.len == b.start)) { a.len += b.len; pieces.erase(pieces.begin() + idx + 1); }
        }
    }
    char charAt(size_t pos) const {
        size_t cur = 0;
        for (const auto& p : pieces) {
            if (cur + p.len <= pos) { cur += p.len; continue; }
            size_t local = pos - cur;
            if (p.isOriginal) return origPtr[p.start + local]; else return addBuf[p.start + local];
        }
        return ' ';
    }
};

struct Cursor {
    size_t head; size_t anchor; float desiredX;
    size_t start() const { return std::min(head, anchor); }
    size_t end() const { return std::max(head, anchor); }
    bool hasSelection() const { return head != anchor; }
    void clearSelection() { anchor = head; }
};

struct EditOp { enum Type { Insert, Erase } type; size_t pos; std::string text; };
struct EditBatch { std::vector<EditOp> ops; std::vector<Cursor> beforeCursors; std::vector<Cursor> afterCursors; };
struct UndoManager {
    std::vector<EditBatch> undoStack; std::vector<EditBatch> redoStack; int savePoint = 0;
    void clear() { undoStack.clear(); redoStack.clear(); savePoint = 0; }
    void markSaved() { savePoint = (int)undoStack.size(); }
    bool isModified() const { return (int)undoStack.size() != savePoint; }
    void push(const EditBatch& batch) { if (savePoint > (int)undoStack.size()) savePoint = -1; undoStack.push_back(batch); redoStack.clear(); }
    bool canUndo() const { return !undoStack.empty(); }
    bool canRedo() const { return !redoStack.empty(); }
    EditBatch popUndo() { EditBatch e = undoStack.back(); undoStack.pop_back(); redoStack.push_back(e); return e; }
    EditBatch popRedo() { EditBatch e = redoStack.back(); redoStack.pop_back(); undoStack.push_back(e); return e; }
};

struct MappedFile {
    HANDLE hFile = INVALID_HANDLE_VALUE; HANDLE hMap = NULL; const char* ptr = nullptr; size_t size = 0;
    bool open(const wchar_t* path) {
        hFile = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return false;
        LARGE_INTEGER li; if (!GetFileSizeEx(hFile, &li)) return false; size = (size_t)li.QuadPart;
        if (size == 0) { ptr = nullptr; return true; }
        hMap = CreateFileMappingW(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
        if (!hMap) return false; ptr = (const char*)MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0); return !!ptr;
    }
    void close() { if (ptr) { UnmapViewOfFile(ptr); ptr = nullptr; } if (hMap) { CloseHandle(hMap); hMap = NULL; } if (hFile != INVALID_HANDLE_VALUE) { CloseHandle(hFile); hFile = INVALID_HANDLE_VALUE; } }
    ~MappedFile() { close(); }
};

struct Editor {
    HWND hwnd = NULL;
    HWND hFindDlg = NULL;
    PieceTable pt;
    UndoManager undo;
    std::unique_ptr<MappedFile> fileMap;
    std::wstring currentFilePath;
    bool isDirty = false;
    UINT cfMsDevCol = 0;
    std::string searchQuery;
    std::string replaceQuery;
    bool searchMatchCase = false;
    bool searchWholeWord = false;
    bool searchRegex = false;
    bool isReplaceMode = false;
    bool showHelpPopup = false;
    std::vector<Cursor> cursors;
    EditBatch pendingPadding;
    bool isDragging = false; bool isRectSelecting = false;
    float rectAnchorX = 0, rectAnchorY = 0; float rectHeadX = 0, rectHeadY = 0;
    bool isDragMovePending = false; bool isDragMoving = false;
    size_t dragMoveSourceStart = 0; size_t dragMoveSourceEnd = 0; size_t dragMoveDestPos = 0;
    wchar_t highSurrogate = 0; std::string imeComp;
    int vScrollPos = 0; int hScrollPos = 0; std::vector<size_t> lineStarts;
    float maxLineWidth = 100.0f; float gutterWidth = 50.0f;
    DWORD lastClickTime = 0; int clickCount = 0; int lastClickX = 0, lastClickY = 0;
    float currentFontSize = 21.0f; DWORD zoomPopupEndTime = 0; std::wstring zoomPopupText;
    bool suppressUI = false;
    ID2D1Factory* d2dFactory = nullptr; ID2D1HwndRenderTarget* rend = nullptr;
    IDWriteFactory* dwFactory = nullptr; IDWriteTextFormat* textFormat = nullptr; IDWriteTextFormat* popupTextFormat = nullptr;
    IDWriteTextFormat* helpTextFormat = nullptr;
    ID2D1StrokeStyle* dotStyle = nullptr; ID2D1StrokeStyle* roundJoinStyle = nullptr;
    D2D1::ColorF background = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f); D2D1::ColorF textColor = D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f);
    D2D1::ColorF gutterBg = D2D1::ColorF(0.95f, 0.95f, 0.95f, 1.0f); D2D1::ColorF gutterText = D2D1::ColorF(0.6f, 0.6f, 0.6f, 1.0f);
    D2D1::ColorF selColor = D2D1::ColorF(0.7f, 0.8f, 1.0f, 1.0f); D2D1::ColorF highlightColor = D2D1::ColorF(1.0f, 1.0f, 0.0f, 0.4f);
    float dpiScaleX = 1.0f, dpiScaleY = 1.0f; float lineHeight = 17.5f; float charWidth = 8.0f;

    void initGraphics(HWND h) {
        hwnd = h;
        D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2dFactory);
        DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&dwFactory));
        RECT r; GetClientRect(hwnd, &r);
        d2dFactory->CreateHwndRenderTarget(D2D1::RenderTargetProperties(), D2D1::HwndRenderTargetProperties(hwnd, D2D1::SizeU(r.right - r.left, r.bottom - r.top)), &rend);
        FLOAT dpix, dpiy; rend->GetDpi(&dpix, &dpiy); dpiScaleX = dpix / 96.0f; dpiScaleY = dpiy / 96.0f;
        dwFactory->CreateTextFormat(L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 24.0f, L"en-us", &popupTextFormat);
        if (popupTextFormat) { popupTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER); popupTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER); }
        dwFactory->CreateTextFormat(L"Consolas", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 18.0f, L"en-us", &helpTextFormat);
        if (helpTextFormat) {
            helpTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            helpTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        }
        float dashes[] = { 2.0f, 2.0f }; D2D1_STROKE_STYLE_PROPERTIES props = D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_FLAT, D2D1_CAP_STYLE_FLAT, D2D1_CAP_STYLE_FLAT, D2D1_LINE_JOIN_MITER, 10.0f, D2D1_DASH_STYLE_CUSTOM, 0.0f); d2dFactory->CreateStrokeStyle(&props, dashes, 2, &dotStyle);
        D2D1_STROKE_STYLE_PROPERTIES roundProps = D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_LINE_JOIN_ROUND, 10.0f, D2D1_DASH_STYLE_SOLID, 0.0f); d2dFactory->CreateStrokeStyle(&roundProps, nullptr, 0, &roundJoinStyle);
        cfMsDevCol = RegisterClipboardFormatW(L"MSDEVColumnSelect");
        updateFont(currentFontSize); rebuildLineStarts(); cursors.push_back({ 0, 0, 0.0f }); updateTitleBar();
    }
    void updateFont(float size) {
        if (size < 6.0f) size = 6.0f; if (size > 200.0f) size = 200.0f; currentFontSize = size;
        if (textFormat) { textFormat->Release(); textFormat = nullptr; }
        dwFactory->CreateTextFormat(L"Consolas", NULL, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, currentFontSize, L"en-us", &textFormat);
        lineHeight = currentFontSize * 1.25f;
        textFormat->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, lineHeight, lineHeight * 0.8f);
        textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING); textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        IDWriteTextLayout* layout = nullptr;
        if (SUCCEEDED(dwFactory->CreateTextLayout(L"0", 1, textFormat, 100.0f, 100.0f, &layout))) { DWRITE_TEXT_METRICS m; layout->GetMetrics(&m); charWidth = m.width; layout->Release(); }
        updateGutterWidth(); updateScrollBars();
    }
    void destroyGraphics() {
        if (popupTextFormat) popupTextFormat->Release();
        if (helpTextFormat) helpTextFormat->Release();
        if (dotStyle) dotStyle->Release(); if (roundJoinStyle) roundJoinStyle->Release();
        if (textFormat) textFormat->Release(); if (dwFactory) dwFactory->Release(); if (rend) rend->Release(); if (d2dFactory) d2dFactory->Release();
    }
    void updateTitleBar() {
        if (!hwnd) return; std::wstring title = L"miu - "; if (currentFilePath.empty()) title += L"無題"; else title += currentFilePath; if (isDirty) title += L" *"; SetWindowTextW(hwnd, title.c_str());
    }
    void updateDirtyFlag() { bool newDirty = undo.isModified(); if (isDirty != newDirty) { isDirty = newDirty; updateTitleBar(); } }
    void updateGutterWidth() {
        if (suppressUI) return;
        int lines = (int)lineStarts.size(); int digits = 1; while (lines >= 10) { lines /= 10; digits++; }
        float digitWidth = 10.0f * (currentFontSize / 14.0f); gutterWidth = (float)(digits * digitWidth + 20.0f);
    }
    void rebuildLineStarts() {
        lineStarts.clear(); lineStarts.push_back(0); size_t len = pt.length(); size_t globalOffset = 0; size_t maxBytes = 0;
        for (const auto& p : pt.pieces) {
            const char* buf = p.isOriginal ? (pt.origPtr + p.start) : (pt.addBuf.data() + p.start);
            for (size_t i = 0; i < p.len; ++i) { if (buf[i] == '\n') lineStarts.push_back(globalOffset + i + 1); }
            globalOffset += p.len;
        }
        for (size_t i = 0; i < lineStarts.size(); ++i) {
            size_t s = lineStarts[i]; size_t e = (i + 1 < lineStarts.size()) ? lineStarts[i + 1] : len;
            size_t lineLen = e - s; if (lineLen > maxBytes) maxBytes = lineLen;
        }
        maxLineWidth = maxBytes * charWidth + 100.0f; updateGutterWidth(); updateScrollBars();
    }
    int getLineIdx(size_t pos) {
        if (lineStarts.empty()) return 0;
        auto it = std::upper_bound(lineStarts.begin(), lineStarts.end(), pos); int idx = (int)std::distance(lineStarts.begin(), it) - 1;
        if (idx < 0) idx = 0; if (idx >= (int)lineStarts.size()) idx = (int)lineStarts.size() - 1; return idx;
    }
    float getXFromPos(size_t pos) {
        int lineIdx = getLineIdx(pos); size_t start = lineStarts[lineIdx];
        size_t end = (lineIdx + 1 < (int)lineStarts.size()) ? lineStarts[lineIdx + 1] : pt.length(); size_t len = (end > start) ? (end - start) : 0;
        std::string lineStr = pt.getRange(start, len); std::wstring wLine = UTF8ToW(lineStr);
        IDWriteTextLayout* layout = nullptr;
        HRESULT hr = dwFactory->CreateTextLayout(wLine.c_str(), (UINT32)wLine.size(), textFormat, 10000.0f, (FLOAT)lineHeight, &layout);
        float x = 0;
        if (SUCCEEDED(hr) && layout) {
            size_t utf8Len = (pos >= start) ? (pos - start) : 0;
            if (utf8Len > lineStr.size()) utf8Len = lineStr.size();
            std::string subUtf8 = lineStr.substr(0, utf8Len);
            std::wstring subUtf16 = UTF8ToW(subUtf8);
            UINT32 u16Idx = (UINT32)subUtf16.size();
            if (u16Idx > wLine.size()) u16Idx = (UINT32)wLine.size();
            DWRITE_HIT_TEST_METRICS m; FLOAT px, py;
            layout->HitTestTextPosition(u16Idx, FALSE, &px, &py, &m);
            x = px;
            layout->Release();
        }
        return x;
    }
    size_t getPosFromLineAndX(int lineIdx, float targetX) {
        if (lineIdx < 0 || lineIdx >= (int)lineStarts.size()) return cursors.empty() ? 0 : cursors.back().head;
        size_t start = lineStarts[lineIdx]; size_t end = (lineIdx + 1 < (int)lineStarts.size()) ? lineStarts[lineIdx + 1] : pt.length(); size_t len = (end > start) ? (end - start) : 0;
        std::string lineStr = pt.getRange(start, len); std::wstring wLine = UTF8ToW(lineStr);
        IDWriteTextLayout* layout = nullptr;
        HRESULT hr = dwFactory->CreateTextLayout(wLine.c_str(), (UINT32)wLine.size(), textFormat, 10000.0f, (FLOAT)lineHeight, &layout);
        size_t resultPos = start;
        if (SUCCEEDED(hr) && layout) {
            BOOL isTrailing, isInside; DWRITE_HIT_TEST_METRICS m; layout->HitTestPoint(targetX, 1.0f, &isTrailing, &isInside, &m);
            size_t local = m.textPosition; if (isTrailing) local += m.length;
            bool hasNewline = (!wLine.empty() && wLine.back() == L'\n');
            if (hasNewline) { if (local >= wLine.size()) local = wLine.size() - 1; }
            else { if (local > wLine.size()) local = wLine.size(); }
            std::wstring wSub = wLine.substr(0, local); std::string sub = WToUTF8(wSub); resultPos = start + sub.size(); layout->Release();
        }
        return resultPos;
    }
    void updateScrollBars() {
        if (suppressUI) return;
        if (!hwnd) return; RECT rc; GetClientRect(hwnd, &rc);
        float clientH = (rc.bottom - rc.top) / dpiScaleY; float clientW = (rc.right - rc.left) / dpiScaleX - gutterWidth; if (clientW < 0) clientW = 0;
        int linesVisible = (int)(clientH / lineHeight);
        SCROLLINFO si = {}; si.cbSize = sizeof(SCROLLINFO); si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
        si.nMin = 0; si.nMax = (int)lineStarts.size() + linesVisible - 2; if (si.nMax < 0) si.nMax = 0; si.nPage = linesVisible; si.nPos = vScrollPos; SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
        si.nMin = 0; si.nMax = (int)maxLineWidth; si.nPage = (int)clientW; si.nPos = hScrollPos; SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
    }
    void getCaretPoint(float& x, float& y) {
        if (cursors.empty()) { x = 0; y = 0; return; }
        size_t pos = cursors.back().head; int line = getLineIdx(pos); float docY = line * lineHeight; float localX = getXFromPos(pos);
        x = (localX - hScrollPos + gutterWidth) * dpiScaleX; y = (docY - vScrollPos * lineHeight) * dpiScaleY;
    }
    void ensureCaretVisible() {
        if (cursors.empty()) return;
        Cursor& mainCursor = cursors.back();
        RECT rc; GetClientRect(hwnd, &rc);
        float clientH = (rc.bottom - rc.top) / dpiScaleY;
        float clientW = (rc.right - rc.left) / dpiScaleX;
        int linesVisible = (int)(clientH / lineHeight);
        int caretLine = getLineIdx(mainCursor.head);
        if (caretLine < vScrollPos) vScrollPos = caretLine;
        else if (caretLine >= vScrollPos + linesVisible - 1) vScrollPos = caretLine - linesVisible + 2;
        if (vScrollPos < 0) vScrollPos = 0;
        float visibleTextW = clientW - gutterWidth;
        if (visibleTextW < charWidth) visibleTextW = charWidth;
        float caretX = getXFromPos(mainCursor.head);
        float margin = charWidth * 2.0f;
        if (caretX < hScrollPos + margin) {
            hScrollPos = (int)(caretX - margin);
        }
        else if (caretX > hScrollPos + visibleTextW - margin) {
            hScrollPos = (int)(caretX - visibleTextW + margin);
        }
        if (hScrollPos < 0) hScrollPos = 0;
        updateScrollBars();
        InvalidateRect(hwnd, NULL, FALSE);
    }
    std::string buildVisibleText(int numLines) {
        if (lineStarts.empty()) return "";
        size_t startOffset = (vScrollPos < (int)lineStarts.size()) ? lineStarts[vScrollPos] : lineStarts.back();
        size_t endOffset = pt.length(); int endLineIdx = vScrollPos + numLines; if (endLineIdx < (int)lineStarts.size()) endOffset = lineStarts[endLineIdx];
        return pt.getRange(startOffset, (endOffset > startOffset) ? (endOffset - startOffset) : 0);
    }
    size_t getDocPosFromPoint(int x, int y) {
        float dipX = x / dpiScaleX; float dipY = y / dpiScaleY; if (dipX < gutterWidth) dipX = gutterWidth;
        float virtualX = dipX - gutterWidth + hScrollPos; float virtualY = dipY;
        RECT rc; GetClientRect(hwnd, &rc); float clientH = (rc.bottom - rc.top) / dpiScaleY; float clientW = (rc.right - rc.left) / dpiScaleX - gutterWidth;
        int linesVisible = (int)(clientH / lineHeight) + 2; std::string text = buildVisibleText(linesVisible); std::wstring wtext = UTF8ToW(text);
        float layoutWidth = maxLineWidth + clientW;
        IDWriteTextLayout* layout = nullptr; HRESULT hr = dwFactory->CreateTextLayout(wtext.c_str(), (UINT32)wtext.size(), textFormat, layoutWidth, clientH, &layout);
        size_t resultPos = 0; size_t visibleStartOffset = (vScrollPos < (int)lineStarts.size()) ? lineStarts[vScrollPos] : pt.length();
        if (SUCCEEDED(hr) && layout) {
            BOOL isTrailing, isInside; DWRITE_HIT_TEST_METRICS metrics; layout->HitTestPoint(virtualX, virtualY, &isTrailing, &isInside, &metrics);
            UINT32 utf16Index = metrics.textPosition; if (isTrailing) utf16Index += metrics.length;
            if (utf16Index > wtext.size()) utf16Index = (UINT32)wtext.size(); std::wstring wsub = wtext.substr(0, utf16Index); std::string sub = WToUTF8(wsub);
            resultPos = visibleStartOffset + sub.size(); layout->Release();
        }
        if (resultPos > pt.length()) resultPos = pt.length(); return resultPos;
    }
    bool isWordChar(char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_' ||
            (unsigned char)c >= 0x80;
    }
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
                    if (startPos >= fullText.size()) startPos = 0;
                    std::smatch m;
                    std::string::const_iterator searchStart = fullText.begin() + startPos;
                    if (std::regex_search(searchStart, fullText.cend(), m, re)) return startPos + m.position();
                    if (std::regex_search(fullText.cbegin(), fullText.cend(), m, re)) return m.position();
                }
                else {
                    auto words_begin = std::sregex_iterator(fullText.begin(), fullText.end(), re);
                    auto words_end = std::sregex_iterator();
                    size_t bestPos = std::string::npos; size_t lastMatch = std::string::npos;
                    size_t limit = (startPos == 0) ? len : startPos;
                    for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
                        size_t pos = i->position();
                        if (pos < limit) bestPos = pos;
                        lastMatch = pos;
                    }
                    if (bestPos != std::string::npos) return bestPos;
                    if (lastMatch != std::string::npos) return lastMatch;
                }
            }
            catch (...) { return std::string::npos; }
            return std::string::npos;
        }
        size_t qLen = query.length();
        auto toLower = [](char c) { return (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c; };
        size_t cur = startPos;
        if (forward) { if (cur >= len) cur = 0; }
        else { if (cur == 0) cur = len; else cur--; }
        size_t count = 0;
        while (count < len) {
            bool match = true;
            for (size_t i = 0; i < qLen; ++i) {
                size_t p = cur + i; if (p >= len) { match = false; break; }
                char c1 = pt.charAt(p); char c2 = query[i];
                if (!matchCase) { c1 = toLower(c1); c2 = toLower(c2); }
                if (c1 != c2) { match = false; break; }
            }
            if (match && wholeWord) {
                if (cur > 0 && isWordChar(pt.charAt(cur - 1))) match = false;
                if (match && (cur + qLen < len) && isWordChar(pt.charAt(cur + qLen))) match = false;
            }
            if (match) return cur;
            if (forward) { cur++; if (cur >= len) cur = 0; }
            else { if (cur == 0) cur = len - 1; else cur--; }
            count++;
        }
        return std::string::npos;
    }
    void findNext(bool forward) {
        if (searchQuery.empty()) { showFindDialog(false); return; }
        size_t startPos = forward ? (cursors.empty() ? 0 : cursors.back().end()) : (cursors.empty() ? 0 : cursors.back().start());
        size_t pos = findText(startPos, searchQuery, forward, searchMatchCase, searchWholeWord, searchRegex);
        if (pos != std::string::npos) {
            size_t matchLen = searchQuery.length();
            if (searchRegex) {
                std::string fullText = pt.getRange(0, pt.length());
                try {
                    std::regex_constants::syntax_option_type flags = std::regex_constants::ECMAScript;
                    if (!searchMatchCase) flags |= std::regex_constants::icase;
                    std::regex re(searchQuery, flags); std::smatch m;
                    std::string::const_iterator searchStart = fullText.begin() + pos;
                    if (std::regex_search(searchStart, fullText.cend(), m, re, std::regex_constants::match_continuous)) matchLen = m.length();
                }
                catch (...) {}
            }
            cursors.clear(); cursors.push_back({ pos + matchLen, pos, getXFromPos(pos + matchLen) });
            ensureCaretVisible(); updateTitleBar();
        }
        else MessageBeep(MB_ICONWARNING);
    }
    void replaceNext() {
        if (cursors.empty() || searchQuery.empty()) return;
        Cursor& c = cursors.back();
        if (!c.hasSelection()) { findNext(true); return; }
        size_t len = c.end() - c.start();
        std::string selText = pt.getRange(c.start(), len);
        bool match = false;
        std::string replacement = replaceQuery;
        if (searchRegex) {
            try {
                std::regex_constants::syntax_option_type flags = std::regex_constants::ECMAScript;
                if (!searchMatchCase) flags |= std::regex_constants::icase;
                std::regex re(searchQuery, flags);
                std::smatch m;
                if (std::regex_match(selText, m, re)) {
                    match = true;
                    std::string fmt = UnescapeString(replaceQuery);
                    replacement = m.format(fmt);
                }
            }
            catch (...) {}
        }
        else {
            if (len == searchQuery.length()) {
                match = true;
                for (size_t i = 0; i < len; ++i) {
                    char c1 = selText[i]; char c2 = searchQuery[i];
                    if (!searchMatchCase) { c1 = (c1 >= 'A' && c1 <= 'Z') ? c1 + ('a' - 'A') : c1; c2 = (c2 >= 'A' && c2 <= 'Z') ? c2 + ('a' - 'A') : c2; }
                    if (c1 != c2) { match = false; break; }
                }
            }
        }
        if (match) {
            insertAtCursors(replacement);
            findNext(true);
        }
        else {
            findNext(true);
        }
    }
    void replaceAll() {
        if (searchQuery.empty()) return;
        struct Match { size_t start; size_t len; std::string replacementText; };
        std::vector<Match> matches;
        size_t currentPos = 0;
        size_t docLen = pt.length();
        if (searchRegex) {
            std::string fullText = pt.getRange(0, docLen);
            std::string fmt = UnescapeString(replaceQuery);
            try {
                std::regex_constants::syntax_option_type flags = std::regex_constants::ECMAScript;
                if (!searchMatchCase) flags |= std::regex_constants::icase;
                std::regex re(searchQuery, flags);
                auto begin = std::sregex_iterator(fullText.begin(), fullText.end(), re);
                auto end = std::sregex_iterator();
                for (auto i = begin; i != end; ++i) {
                    matches.push_back({ (size_t)i->position(), (size_t)i->length(), i->format(fmt) });
                }
            }
            catch (...) { return; }
        }
        else {
            while (true) {
                size_t pos = findText(currentPos, searchQuery, true, searchMatchCase, searchWholeWord, false);
                if (pos == std::string::npos || pos < currentPos) break;
                matches.push_back({ pos, searchQuery.length(), replaceQuery });
                currentPos = pos + searchQuery.length();
                if (currentPos > docLen) break;
            }
        }
        if (matches.empty()) { MessageBeep(MB_ICONASTERISK); return; }
        commitPadding();
        EditBatch batch;
        batch.beforeCursors = cursors;
        for (auto it = matches.rbegin(); it != matches.rend(); ++it) {
            size_t start = it->start;
            size_t len = it->len;
            std::string deleted = pt.getRange(start, len);
            pt.erase(start, len);
            batch.ops.push_back({ EditOp::Erase, start, deleted });
            pt.insert(start, it->replacementText);
            batch.ops.push_back({ EditOp::Insert, start, it->replacementText });
        }
        cursors.clear();
        cursors.push_back({ 0, 0, 0.0f });
        batch.afterCursors = cursors;
        undo.push(batch);
        rebuildLineStarts();
        updateDirtyFlag();
        InvalidateRect(hwnd, NULL, FALSE);
        ShowTaskDialog(L"置換完了", (std::to_wstring(matches.size()) + L" 個の項目を置換しました。").c_str(), nullptr, TDCBF_OK_BUTTON, TD_INFORMATION_ICON);
        if (hFindDlg && IsWindowVisible(hFindDlg)) {
            SetFocus(hFindDlg);
        }
    }
    void updateFindReplaceUI(HWND dlg, bool replaceMode) {
        if (!dlg) return;
        isReplaceMode = replaceMode;
        int show = replaceMode ? SW_SHOW : SW_HIDE;
        ShowWindow(GetDlgItem(dlg, IDC_REPLACE_LABEL), show);
        ShowWindow(GetDlgItem(dlg, IDC_REPLACE_EDIT), show);
        ShowWindow(GetDlgItem(dlg, IDC_REPLACE_BTN), show);
        ShowWindow(GetDlgItem(dlg, IDC_REPLACE_ALL_BTN), show);
        SetWindowTextW(dlg, replaceMode ? L"置換" : L"検索");
    }
    static INT_PTR CALLBACK FindDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
        Editor* pThis = (Editor*)GetWindowLongPtr(hDlg, GWLP_USERDATA);
        switch (message) {
        case WM_INITDIALOG:
            pThis = (Editor*)lParam;
            SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)pThis);
            {
                RECT rcParent, rcDlg; GetWindowRect(pThis->hwnd, &rcParent); GetWindowRect(hDlg, &rcDlg);
                int x = rcParent.left + ((rcParent.right - rcParent.left) - (rcDlg.right - rcDlg.left)) / 2;
                int y = rcParent.top + ((rcParent.bottom - rcParent.top) - (rcDlg.bottom - rcDlg.top)) / 2;
                SetWindowPos(hDlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            }
            SetDlgItemTextW(hDlg, IDC_FIND_EDIT, UTF8ToW(pThis->searchQuery).c_str());
            SetDlgItemTextW(hDlg, IDC_REPLACE_EDIT, UTF8ToW(pThis->replaceQuery).c_str());
            CheckDlgButton(hDlg, IDC_FIND_CASE, pThis->searchMatchCase ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hDlg, IDC_FIND_WORD, pThis->searchWholeWord ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hDlg, IDC_FIND_REGEX, pThis->searchRegex ? BST_CHECKED : BST_UNCHECKED);
            pThis->updateFindReplaceUI(hDlg, pThis->isReplaceMode);
            SetFocus(GetDlgItem(hDlg, IDC_FIND_EDIT));
            SendMessage(GetDlgItem(hDlg, IDC_FIND_EDIT), EM_SETSEL, 0, -1);
            return FALSE;
        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_FIND_CASE) { pThis->searchMatchCase = IsDlgButtonChecked(hDlg, IDC_FIND_CASE) == BST_CHECKED; InvalidateRect(pThis->hwnd, NULL, FALSE); }
            if (LOWORD(wParam) == IDC_FIND_WORD) { pThis->searchWholeWord = IsDlgButtonChecked(hDlg, IDC_FIND_WORD) == BST_CHECKED; InvalidateRect(pThis->hwnd, NULL, FALSE); }
            if (LOWORD(wParam) == IDC_FIND_REGEX) { pThis->searchRegex = IsDlgButtonChecked(hDlg, IDC_FIND_REGEX) == BST_CHECKED; InvalidateRect(pThis->hwnd, NULL, FALSE); }
            if (HIWORD(wParam) == EN_CHANGE) {
                wchar_t wbuf[1024];
                if (LOWORD(wParam) == IDC_FIND_EDIT) { GetDlgItemTextW(hDlg, IDC_FIND_EDIT, wbuf, 1024); pThis->searchQuery = WToUTF8(wbuf); InvalidateRect(pThis->hwnd, NULL, FALSE); }
                if (LOWORD(wParam) == IDC_REPLACE_EDIT) { GetDlgItemTextW(hDlg, IDC_REPLACE_EDIT, wbuf, 1024); pThis->replaceQuery = WToUTF8(wbuf); }
            }
            if (LOWORD(wParam) == IDC_FIND_NEXT || LOWORD(wParam) == IDOK) {
                pThis->findNext(true); return TRUE;
            }
            if (LOWORD(wParam) == IDC_REPLACE_BTN) {
                if (!pThis->isReplaceMode) return TRUE;
                pThis->replaceNext(); return TRUE;
            }
            if (LOWORD(wParam) == IDC_REPLACE_ALL_BTN) {
                if (!pThis->isReplaceMode) return TRUE;
                pThis->replaceAll(); return TRUE;
            }
            if (LOWORD(wParam) == IDC_FIND_CANCEL || LOWORD(wParam) == IDCANCEL) {
                DestroyWindow(hDlg); pThis->hFindDlg = NULL; return TRUE;
            }
            break;
        }
        return FALSE;
    }
    void showFindDialog(bool replaceMode) {
        isReplaceMode = replaceMode;
        if (hFindDlg) {
            updateFindReplaceUI(hFindDlg, isReplaceMode);
            SetFocus(hFindDlg);
            if (!cursors.empty() && cursors.back().hasSelection()) {
                size_t s = cursors.back().start(); size_t len = cursors.back().end() - s;
                if (len < 100) {
                    searchQuery = pt.getRange(s, len);
                    SetDlgItemTextW(hFindDlg, IDC_FIND_EDIT, UTF8ToW(searchQuery).c_str());
                    SendMessage(GetDlgItem(hFindDlg, IDC_FIND_EDIT), EM_SETSEL, 0, -1);
                }
            }
            return;
        }
        if (!cursors.empty() && cursors.back().hasSelection()) {
            size_t s = cursors.back().start(); size_t len = cursors.back().end() - s;
            if (len < 100) searchQuery = pt.getRange(s, len);
        }
        hFindDlg = CreateDialogParamW(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_FIND_DIALOG), hwnd, FindDlgProc, (LPARAM)this);
        ShowWindow(hFindDlg, SW_SHOW);
    }
    void rollbackPadding() { if (pendingPadding.ops.empty()) return; for (int i = (int)pendingPadding.ops.size() - 1; i >= 0; --i) { const auto& op = pendingPadding.ops[i]; if (op.type == EditOp::Insert) pt.erase(op.pos, op.text.size()); } pendingPadding.ops.clear(); rebuildLineStarts(); }
    void commitPadding() {
        if (pendingPadding.ops.empty()) return;
        undo.push(pendingPadding);
        pendingPadding.ops.clear();
        pendingPadding.beforeCursors.clear();
        pendingPadding.afterCursors.clear();
    }
    void updateRectSelection() {
        suppressUI = true;
        if (pendingPadding.ops.empty()) {
            pendingPadding.beforeCursors = cursors;
        }
        rollbackPadding();
        float startY = std::min(rectAnchorY, rectHeadY);
        float endY = std::max(rectAnchorY, rectHeadY);
        int startLineIdx = (int)(startY / lineHeight);
        int endLineIdx = (int)(endY / lineHeight);
        if (startLineIdx < 0) startLineIdx = 0;
        int currentMaxLine = (int)lineStarts.size() - 1;
        if (endLineIdx > currentMaxLine) {
            int linesToAdd = endLineIdx - currentMaxLine;
            size_t insertPos = pt.length();
            std::string newLines(linesToAdd, '\n');
            pt.insert(insertPos, newLines);
            pendingPadding.ops.push_back({ EditOp::Insert, insertPos, newLines });
            rebuildLineStarts();
        }
        if (endLineIdx >= (int)lineStarts.size()) endLineIdx = (int)lineStarts.size() - 1;
        float targetAnchorX = rectAnchorX;
        float targetHeadX = rectHeadX;
        std::vector<int> lines;
        for (int i = startLineIdx; i <= endLineIdx; ++i) lines.push_back(i);
        std::reverse(lines.begin(), lines.end());
        cursors.clear();
        float requiredX = std::max(targetAnchorX, targetHeadX);
        for (int lineIdx : lines) {
            size_t start = lineStarts[lineIdx];
            size_t nextStart = (lineIdx + 1 < (int)lineStarts.size()) ? lineStarts[lineIdx + 1] : pt.length();
            size_t end = nextStart;
            if (end > start && pt.charAt(end - 1) == '\n') end--;
            std::string lineStr = pt.getRange(start, end - start);
            std::wstring wLine = UTF8ToW(lineStr);
            float currentWidth = (float)wLine.length() * charWidth;
            if (requiredX > currentWidth) {
                int spacesNeeded = (int)((requiredX - currentWidth) / charWidth + 0.5f);
                if (spacesNeeded > 0) {
                    std::string spaces(spacesNeeded, ' ');
                    pt.insert(end, spaces);
                    pendingPadding.ops.push_back({ EditOp::Insert, end, spaces });
                }
            }
        }
        if (!pendingPadding.ops.empty()) rebuildLineStarts();
        for (int i = startLineIdx; i <= endLineIdx; ++i) {
            size_t anc = getPosFromLineAndX(i, targetAnchorX);
            size_t hd = getPosFromLineAndX(i, targetHeadX);
            cursors.push_back({ hd, anc, targetHeadX });
        }
        suppressUI = false;
        rebuildLineStarts();
        InvalidateRect(hwnd, NULL, FALSE);
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
            if (!searchQuery.empty()) {
                if (searchRegex) {
                    try {
                        std::regex_constants::syntax_option_type flags = std::regex_constants::ECMAScript;
                        if (!searchMatchCase) flags |= std::regex_constants::icase;
                        std::regex re(searchQuery, flags);
                        auto words_begin = std::sregex_iterator(text.begin(), text.end(), re); auto words_end = std::sregex_iterator();
                        for (auto i = words_begin; i != words_end; ++i) {
                            size_t offset = i->position(); size_t len = i->length();
                            size_t startU16 = UTF8ToW(text.substr(0, offset)).length(); size_t lenU16 = UTF8ToW(text.substr(offset, len)).length();
                            UINT32 count = 0; layout->HitTestTextRange((UINT32)startU16, (UINT32)lenU16, 0, 0, 0, 0, &count);
                            if (count > 0) {
                                std::vector<DWRITE_HIT_TEST_METRICS> m(count); layout->HitTestTextRange((UINT32)startU16, (UINT32)lenU16, 0, 0, &m[0], count, &count);
                                for (const auto& mm : m) { float top = std::floor((mm.top + lineHeight * 0.5f) / lineHeight) * lineHeight; rend->FillRectangle(D2D1::RectF(mm.left, top, mm.left + mm.width, top + lineHeight), hlBrush); }
                            }
                        }
                    }
                    catch (...) {}
                }
                else {
                    std::string q = searchQuery; std::string t = text;
                    if (!searchMatchCase) { std::transform(q.begin(), q.end(), q.begin(), ::tolower); std::transform(t.begin(), t.end(), t.begin(), ::tolower); }
                    size_t offset = 0;
                    while ((offset = t.find(q, offset)) != std::string::npos) {
                        bool match = true;
                        if (searchWholeWord) { if (offset > 0 && isWordChar(text[offset - 1])) match = false; if (match && (offset + q.length() < text.length()) && isWordChar(text[offset + q.length()])) match = false; }
                        if (match) {
                            size_t startU16 = UTF8ToW(text.substr(0, offset)).length(); size_t lenU16 = UTF8ToW(text.substr(offset, q.length())).length();
                            UINT32 count = 0; layout->HitTestTextRange((UINT32)startU16, (UINT32)lenU16, 0, 0, 0, 0, &count);
                            if (count > 0) {
                                std::vector<DWRITE_HIT_TEST_METRICS> m(count); layout->HitTestTextRange((UINT32)startU16, (UINT32)lenU16, 0, 0, &m[0], count, &count);
                                for (const auto& mm : m) { float top = std::floor((mm.top + lineHeight * 0.5f) / lineHeight) * lineHeight; rend->FillRectangle(D2D1::RectF(mm.left, top, mm.left + mm.width, top + lineHeight), hlBrush); }
                            }
                        }
                        offset += 1;
                    }
                }
            }
            ID2D1Geometry* unifiedSelectionGeo = nullptr; std::vector<D2D1_RECT_F> rawRects; float hInset = 4.0f; float vInset = 0.0f;
            for (const auto& cursor : cursors) {
                size_t s = cursor.start(); size_t e = cursor.end(); size_t relS = (s > visibleStartOffset) ? s - visibleStartOffset : 0; size_t relE = (e > visibleStartOffset) ? e - visibleStartOffset : 0;
                if (hasIME) { if (relS >= caretOffsetInVisible) relS += imeComp.size(); if (relE >= caretOffsetInVisible) relE += imeComp.size(); }
                if (relS < text.size() && relS != relE) {
                    if (relE > text.size()) relE = text.size();
                    if (relE > relS) {
                        std::string subS = text.substr(0, relS); std::string subRange = text.substr(relS, relE - relS);
                        size_t utf16Start = UTF8ToW(subS).size(); size_t utf16Len = UTF8ToW(subRange).size();
                        UINT32 count = 0; layout->HitTestTextRange((UINT32)utf16Start, (UINT32)utf16Len, 0, 0, 0, 0, &count);
                        if (count > 0) {
                            std::vector<DWRITE_HIT_TEST_METRICS> m(count); layout->HitTestTextRange((UINT32)utf16Start, (UINT32)utf16Len, 0, 0, &m[0], count, &count);
                            for (const auto& mm : m) { float top = std::floor((mm.top + lineHeight * 0.5f) / lineHeight) * lineHeight; rawRects.push_back(D2D1::RectF(mm.left, top, mm.left + mm.width, top + lineHeight)); }
                        }
                        for (size_t k = 0; k < subRange.size(); ++k) {
                            if (subRange[k] == '\n') {
                                std::string pre = text.substr(0, relS + k); UINT32 idx16 = (UINT32)UTF8ToW(pre).size();
                                DWRITE_HIT_TEST_METRICS m; FLOAT px, py; layout->HitTestTextPosition(idx16, FALSE, &px, &py, &m);
                                float top = std::floor((m.top + lineHeight * 0.5f) / lineHeight) * lineHeight; rawRects.push_back(D2D1::RectF(px - 0.5f, top, px + charWidth, top + lineHeight));
                            }
                        }
                    }
                }
            }
            std::sort(rawRects.begin(), rawRects.end(), [](const D2D1_RECT_F& a, const D2D1_RECT_F& b) { if (std::abs(a.top - b.top) > 1.0f) return a.top < b.top; return a.left < b.left; });
            std::vector<D2D1_RECT_F> mergedRects;
            if (!rawRects.empty()) {
                mergedRects.push_back(rawRects[0]);
                for (size_t i = 1; i < rawRects.size(); ++i) {
                    D2D1_RECT_F& curr = mergedRects.back(); const D2D1_RECT_F& next = rawRects[i];
                    bool sameLine = std::abs(curr.top - next.top) < 1.0f; bool touches = next.left <= curr.right + 1.0f;
                    if (sameLine && touches) { curr.right = std::max(curr.right, next.right); curr.bottom = std::max(curr.bottom, next.bottom); }
                    else mergedRects.push_back(next);
                }
            }
            if (!mergedRects.empty()) {
                ID2D1RectangleGeometry* firstRect = nullptr; D2D1_RECT_F r = mergedRects[0]; r.left += hInset; r.top += vInset; r.right -= hInset; r.bottom -= vInset;
                d2dFactory->CreateRectangleGeometry(&r, &firstRect); unifiedSelectionGeo = firstRect;
                for (size_t i = 1; i < mergedRects.size(); ++i) {
                    D2D1_RECT_F rNext = mergedRects[i]; rNext.left += hInset; rNext.top += vInset; rNext.right -= hInset; rNext.bottom -= vInset;
                    ID2D1RectangleGeometry* nextGeo = nullptr; d2dFactory->CreateRectangleGeometry(&rNext, &nextGeo);
                    ID2D1PathGeometry* pathGeo = nullptr; d2dFactory->CreatePathGeometry(&pathGeo);
                    ID2D1GeometrySink* sink = nullptr; pathGeo->Open(&sink); unifiedSelectionGeo->CombineWithGeometry(nextGeo, D2D1_COMBINE_MODE_UNION, nullptr, sink);
                    sink->Close(); sink->Release(); unifiedSelectionGeo->Release(); unifiedSelectionGeo = pathGeo; nextGeo->Release();
                }
                if (unifiedSelectionGeo) { rend->FillGeometry(unifiedSelectionGeo, selBrush); rend->DrawGeometry(unifiedSelectionGeo, selBrush, 8.0f, roundJoinStyle); unifiedSelectionGeo->Release(); }
            }
            if (isDragMoving) {
                size_t relPos = (dragMoveDestPos > visibleStartOffset) ? dragMoveDestPos - visibleStartOffset : 0;
                if (relPos <= text.size()) {
                    std::string beforeCaret = text.substr(0, relPos); std::wstring wBefore = UTF8ToW(beforeCaret);
                    DWRITE_HIT_TEST_METRICS m; FLOAT px, py; layout->HitTestTextPosition((UINT32)wBefore.size(), FALSE, &px, &py, &m); rend->DrawLine(D2D1::Point2F(px, py), D2D1::Point2F(px, py + lineHeight), caretBrush, 2.0f);
                }
            }
            for (const auto& cursor : cursors) {
                size_t head = cursor.head; size_t relHead = (head > visibleStartOffset) ? head - visibleStartOffset : 0;
                if (hasIME && relHead >= caretOffsetInVisible) relHead += imeComp.size();
                if (relHead <= text.size()) {
                    std::string beforeCaret = text.substr(0, relHead); std::wstring wBefore = UTF8ToW(beforeCaret);
                    DWRITE_HIT_TEST_METRICS m; FLOAT px, py; layout->HitTestTextPosition((UINT32)wBefore.size(), FALSE, &px, &py, &m); rend->DrawLine(D2D1::Point2F(px, py), D2D1::Point2F(px, py + lineHeight), caretBrush);
                    if (&cursor == &cursors.back()) { imeCx = px; imeCy = py; }
                }
            }
            selBrush->Release(); caretBrush->Release(); hlBrush->Release();
            ID2D1SolidColorBrush* brush = nullptr; rend->CreateSolidColorBrush(textColor, &brush); rend->DrawTextLayout(D2D1::Point2F(0, 0), layout, brush, D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT); brush->Release();
            if (hasIME) {
                std::string prefixUtf8 = text.substr(0, caretOffsetInVisible); std::wstring prefixWide = UTF8ToW(prefixUtf8); UINT32 imeStart = (UINT32)prefixWide.size(); std::wstring imeCompWide = UTF8ToW(imeComp); UINT32 imeLen = (UINT32)imeCompWide.size(); UINT32 count = 0; layout->HitTestTextRange(imeStart, imeLen, 0, 0, 0, 0, &count);
                if (count > 0) {
                    std::vector<DWRITE_HIT_TEST_METRICS> m(count); layout->HitTestTextRange(imeStart, imeLen, 0, 0, &m[0], count, &count);
                    ID2D1SolidColorBrush* underlineBrush = nullptr; rend->CreateSolidColorBrush(textColor, &underlineBrush);
                    for (const auto& mm : m) {
                        float x = mm.left; float y = std::floor(mm.top + mm.height - 2.0f) + 0.5f; float w = mm.width;
                        if (dotStyle) rend->DrawLine(D2D1::Point2F(x, y), D2D1::Point2F(x + w, y), underlineBrush, 1.5f, dotStyle); else rend->DrawLine(D2D1::Point2F(x, y), D2D1::Point2F(x + w, y), underlineBrush, 1.0f);
                    }
                    underlineBrush->Release();
                }
            }
            layout->Release();
        }
        rend->SetTransform(D2D1::Matrix3x2F::Identity());
        ID2D1SolidColorBrush* gutterBgBrush = nullptr; rend->CreateSolidColorBrush(gutterBg, &gutterBgBrush); rend->FillRectangle(D2D1::RectF(0, 0, gutterWidth, clientH), gutterBgBrush); gutterBgBrush->Release();
        ID2D1SolidColorBrush* gutterTextBrush = nullptr; rend->CreateSolidColorBrush(gutterText, &gutterTextBrush);
        int startLine = vScrollPos; int endLine = startLine + linesVisible; if (endLine > (int)lineStarts.size()) endLine = (int)lineStarts.size();
        for (int i = startLine; i < endLine; i++) {
            std::wstring numStr = std::to_wstring(i + 1); float yPos = (float)((i - startLine)) * lineHeight; IDWriteTextLayout* numLayout = nullptr;
            if (SUCCEEDED(dwFactory->CreateTextLayout(numStr.c_str(), (UINT32)numStr.size(), textFormat, gutterWidth, lineHeight, &numLayout))) {
                numLayout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING); rend->DrawTextLayout(D2D1::Point2F(0, yPos), numLayout, gutterTextBrush); numLayout->Release();
            }
        }
        gutterTextBrush->Release();
        HIMC hIMC = ImmGetContext(hwnd); if (hIMC) { COMPOSITIONFORM cf = {}; cf.dwStyle = CFS_POINT; cf.ptCurrentPos.x = (LONG)(imeCx + gutterWidth - hScrollPos); cf.ptCurrentPos.y = (LONG)imeCy; ImmSetCompositionWindow(hIMC, &cf); CANDIDATEFORM cdf = {}; cdf.dwIndex = 0; cdf.dwStyle = CFS_CANDIDATEPOS; cdf.ptCurrentPos.x = (LONG)(imeCx + gutterWidth - hScrollPos); cdf.ptCurrentPos.y = (LONG)(imeCy + lineHeight); ImmSetCandidateWindow(hIMC, &cdf); ImmReleaseContext(hwnd, hIMC); }
        if (GetTickCount() < zoomPopupEndTime) {
            D2D1_RECT_F popupRect = D2D1::RectF(clientW / 2 - 80, clientH / 2 - 40, clientW / 2 + 80, clientH / 2 + 40);
            ID2D1SolidColorBrush* popupBg = nullptr; rend->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.7f), &popupBg);
            ID2D1SolidColorBrush* popupText = nullptr; rend->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), &popupText);
            rend->FillRoundedRectangle(D2D1::RoundedRect(popupRect, 10.0f, 10.0f), popupBg);
            if (popupTextFormat) rend->DrawText(zoomPopupText.c_str(), (UINT32)zoomPopupText.size(), popupTextFormat, popupRect, popupText);
            popupBg->Release(); popupText->Release();
        }
        if (showHelpPopup) {
            float helpW = 500.0f; float helpH = 550.0f;
            D2D1_RECT_F helpRect = D2D1::RectF((clientW - helpW) / 2, (clientH - helpH) / 2, (clientW + helpW) / 2, (clientH + helpH) / 2);
            ID2D1SolidColorBrush* popupBg = nullptr; rend->CreateSolidColorBrush(D2D1::ColorF(0.1f, 0.1f, 0.1f, 0.5f), &popupBg);
            ID2D1SolidColorBrush* popupText = nullptr; rend->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), &popupText);
            rend->FillRoundedRectangle(D2D1::RoundedRect(helpRect, 10.0f, 10.0f), popupBg);
            IDWriteTextLayout* helpLayout = nullptr;
            if (SUCCEEDED(dwFactory->CreateTextLayout(HELP_TEXT.c_str(), (UINT32)HELP_TEXT.size(), helpTextFormat, helpW - 40, helpH - 40, &helpLayout))) {
                rend->DrawTextLayout(D2D1::Point2F(helpRect.left + 20, helpRect.top + 20), helpLayout, popupText);
                helpLayout->Release();
            }
            popupBg->Release(); popupText->Release();
        }
        rend->EndDraw(); EndPaint(hwnd, &ps);
    }
    void insertAtCursors(const std::string& text) { commitPadding(); if (cursors.empty()) return; EditBatch batch; batch.beforeCursors = cursors; std::vector<int> indices(cursors.size()); for (size_t i = 0; i < cursors.size(); ++i) indices[i] = (int)i; std::sort(indices.begin(), indices.end(), [&](int a, int b) {return cursors[a].start() > cursors[b].start(); }); for (int idx : indices) { Cursor& c = cursors[idx]; if (c.hasSelection()) { size_t s = c.start(); size_t l = c.end() - s; std::string d = pt.getRange(s, l); pt.erase(s, l); batch.ops.push_back({ EditOp::Erase,s,d }); for (auto& o : cursors) { if (o.head > s)o.head -= l; if (o.anchor > s)o.anchor -= l; }c.head = s; c.anchor = s; } } for (int idx : indices) { Cursor& c = cursors[idx]; size_t p = c.head; pt.insert(p, text); batch.ops.push_back({ EditOp::Insert,p,text }); size_t l = text.size(); for (auto& o : cursors) { if (o.head >= p)o.head += l; if (o.anchor >= p)o.anchor += l; } } batch.afterCursors = cursors; undo.push(batch); rebuildLineStarts(); ensureCaretVisible(); updateDirtyFlag(); }
    void deleteForwardAtCursors() { commitPadding(); if (cursors.empty()) return; EditBatch batch; batch.beforeCursors = cursors; std::vector<int> indices(cursors.size()); for (size_t i = 0; i < cursors.size(); ++i) indices[i] = (int)i; std::sort(indices.begin(), indices.end(), [&](int a, int b) {return cursors[a].start() > cursors[b].start(); }); for (int idx : indices) { Cursor& c = cursors[idx]; size_t s = c.start(); size_t l = 0; if (c.hasSelection())l = c.end() - s; else { size_t n = moveCaretVisual(s, true); if (n > s)l = n - s; }if (l > 0 && s + l <= pt.length()) { std::string d = pt.getRange(s, l); pt.erase(s, l); batch.ops.push_back({ EditOp::Erase,s,d }); for (auto& o : cursors) { if (o.head > s)o.head -= l; if (o.anchor > s)o.anchor -= l; }c.head = s; c.anchor = s; } } batch.afterCursors = cursors; undo.push(batch); rebuildLineStarts(); ensureCaretVisible(); updateDirtyFlag(); }
    void backspaceAtCursors() { commitPadding(); if (cursors.empty()) return; EditBatch batch; batch.beforeCursors = cursors; std::vector<int> indices(cursors.size()); for (size_t i = 0; i < cursors.size(); ++i) indices[i] = (int)i; std::sort(indices.begin(), indices.end(), [&](int a, int b) {return cursors[a].start() > cursors[b].start(); }); for (int idx : indices) { Cursor& c = cursors[idx]; size_t s = c.start(); size_t l = 0; if (c.hasSelection())l = c.end() - s; else if (s > 0) { size_t p = moveCaretVisual(s, false); if (p < s) { l = s - p; s = p; } }if (l > 0) { std::string d = pt.getRange(s, l); pt.erase(s, l); batch.ops.push_back({ EditOp::Erase,s,d }); for (auto& o : cursors) { if (o.head > s)o.head -= l; if (o.anchor > s)o.anchor -= l; } } } batch.afterCursors = cursors; undo.push(batch); rebuildLineStarts(); ensureCaretVisible(); updateDirtyFlag(); }
    void copyToClipboard() {
        std::string t;
        std::vector<Cursor> s = cursors;
        std::sort(s.begin(), s.end(), [](const Cursor& a, const Cursor& b) {return a.start() < b.start(); });
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i].hasSelection())t += pt.getRange(s[i].start(), s[i].end() - s[i].start());
            if (i < s.size() - 1 && s[i].hasSelection())t += "\r\n";
        }
        if (t.empty())return;
        if (OpenClipboard(hwnd)) {
            EmptyClipboard();
            std::wstring w = UTF8ToW(t);
            HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, (w.size() + 1) * sizeof(wchar_t));
            if (h) {
                void* p = GlobalLock(h);
                memcpy(p, w.c_str(), (w.size() + 1) * sizeof(wchar_t));
                GlobalUnlock(h);
                SetClipboardData(CF_UNICODETEXT, h);
            }
            if (cursors.size() > 1) {
                HGLOBAL hCol = GlobalAlloc(GMEM_MOVEABLE, 1);
                if (hCol) SetClipboardData(cfMsDevCol, hCol);
            }
            CloseClipboard();
        }
    }
    void insertRectangularBlock(const std::string& text) {
        commitPadding();
        if (cursors.empty()) return;
        size_t basePos = cursors.back().head;
        float baseX = getXFromPos(basePos);
        int startLine = getLineIdx(basePos);
        std::vector<std::string> lines;
        std::stringstream ss(text);
        std::string line;
        while (std::getline(ss, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            lines.push_back(line);
        }
        EditBatch batch;
        batch.beforeCursors = cursors;
        std::vector<Cursor> newCursors;
        size_t accumulatedDelta = 0;
        for (size_t i = 0; i < lines.size(); ++i) {
            int targetLineIdx = startLine + (int)i;
            std::string content = lines[i];
            if (targetLineIdx >= (int)lineStarts.size()) {
                size_t insertAt = pt.length();
                std::string nl = "\n";
                pt.insert(insertAt, nl);
                batch.ops.push_back({ EditOp::Insert, insertAt, nl });
                int spacesNeeded = (int)((baseX) / charWidth + 0.5f);
                std::string spaces = "";
                if (spacesNeeded > 0) spaces = std::string(spacesNeeded, ' ');
                size_t contentPos = insertAt + 1;
                if (!spaces.empty()) {
                    pt.insert(contentPos, spaces);
                    batch.ops.push_back({ EditOp::Insert, contentPos, spaces });
                    contentPos += spaces.size();
                }
                pt.insert(contentPos, content);
                batch.ops.push_back({ EditOp::Insert, contentPos, content });
                size_t endPos = contentPos + content.size();
                newCursors.push_back({ endPos, contentPos, baseX + (float)UTF8ToW(content).length() * charWidth });
            }
            else {
                size_t lineStart = lineStarts[targetLineIdx] + accumulatedDelta;
                size_t scanPos = lineStart;
                size_t maxLen = pt.length();
                while (scanPos < maxLen && pt.charAt(scanPos) != '\n') {
                    scanPos++;
                }
                size_t lineEnd = scanPos;
                std::string currentLineStr = pt.getRange(lineStart, lineEnd - lineStart);
                std::wstring wCurrentLine = UTF8ToW(currentLineStr);
                size_t insertOffset = wCurrentLine.length();
                float actualLineWidth = 0.0f;
                IDWriteTextLayout* layout = nullptr;
                HRESULT hr = dwFactory->CreateTextLayout(wCurrentLine.c_str(), (UINT32)wCurrentLine.size(), textFormat, 10000.0f, (FLOAT)lineHeight, &layout);
                if (SUCCEEDED(hr) && layout) {
                    BOOL isTrailing, isInside;
                    DWRITE_HIT_TEST_METRICS m;
                    layout->HitTestPoint(baseX, 1.0f, &isTrailing, &isInside, &m);
                    size_t u16Pos = m.textPosition;
                    if (isTrailing) u16Pos += m.length;
                    std::string pre = WToUTF8(wCurrentLine.substr(0, u16Pos));
                    insertOffset = pre.size();
                    DWRITE_TEXT_METRICS tm;
                    if (SUCCEEDED(layout->GetMetrics(&tm))) {
                        actualLineWidth = tm.widthIncludingTrailingWhitespace;
                    }
                    else {
                        actualLineWidth = (float)wCurrentLine.length() * charWidth;
                    }
                    layout->Release();
                }
                else {
                    actualLineWidth = (float)wCurrentLine.length() * charWidth;
                }
                size_t insertPos = lineStart + insertOffset;
                std::string spaces = "";
                if (insertPos == lineEnd && baseX > actualLineWidth + 1.0f) {
                    int spacesNeeded = (int)((baseX - actualLineWidth) / charWidth + 0.5f);
                    if (spacesNeeded > 0) spaces = std::string(spacesNeeded, ' ');
                }
                size_t addedBytes = 0;
                if (!spaces.empty()) {
                    pt.insert(insertPos, spaces);
                    batch.ops.push_back({ EditOp::Insert, insertPos, spaces });
                    insertPos += spaces.size();
                    addedBytes += spaces.size();
                }
                pt.insert(insertPos, content);
                batch.ops.push_back({ EditOp::Insert, insertPos, content });
                addedBytes += content.size();
                accumulatedDelta += addedBytes;
                size_t endPos = insertPos + content.size();
                newCursors.push_back({ endPos, insertPos, baseX + (float)UTF8ToW(content).length() * charWidth });
            }
        }
        cursors = newCursors;
        batch.afterCursors = cursors;
        undo.push(batch);
        rebuildLineStarts();
        ensureCaretVisible();
        updateDirtyFlag();
        InvalidateRect(hwnd, NULL, FALSE);
    }
    void convertCase(bool toUpper) {
        commitPadding();
        if (cursors.empty()) return;
        EditBatch batch;
        batch.beforeCursors = cursors;
        bool isChanged = false;
        std::vector<int> indices(cursors.size());
        for (size_t i = 0; i < cursors.size(); ++i) indices[i] = (int)i;
        std::sort(indices.begin(), indices.end(), [&](int a, int b) {
            return cursors[a].start() > cursors[b].start();
            });
        for (int idx : indices) {
            Cursor& c = cursors[idx];
            if (!c.hasSelection()) continue;
            size_t start = c.start();
            size_t len = c.end() - start;
            std::string text = pt.getRange(start, len);
            std::wstring wText = UTF8ToW(text);
            if (toUpper) CharUpperBuffW(&wText[0], (DWORD)wText.size());
            else CharLowerBuffW(&wText[0], (DWORD)wText.size());
            std::string newText = WToUTF8(wText);
            if (text == newText) continue;
            isChanged = true;
            pt.erase(start, len);
            batch.ops.push_back({ EditOp::Erase, start, text });
            pt.insert(start, newText);
            batch.ops.push_back({ EditOp::Insert, start, newText });
            long long diff = (long long)newText.size() - (long long)len;
            if (c.head > c.anchor) {
                c.head = start + newText.size();
                c.anchor = start;
            }
            else {
                c.head = start;
                c.anchor = start + newText.size();
            }
            if (diff != 0) {
                for (size_t k = 0; k < cursors.size(); ++k) {
                    if ((int)k == idx) continue;
                    Cursor& other = cursors[k];
                    if (other.start() > start) {
                        if (other.head > start) other.head = (size_t)((long long)other.head + diff);
                        if (other.anchor > start) other.anchor = (size_t)((long long)other.anchor + diff);
                    }
                }
            }
        }
        if (isChanged) {
            batch.afterCursors = cursors;
            undo.push(batch);
            rebuildLineStarts();
            ensureCaretVisible();
            updateDirtyFlag();
            InvalidateRect(hwnd, NULL, FALSE);
        }
    }
    std::vector<int> getSelectedLineIndices() {
        std::vector<int> lines;
        for (const auto& c : cursors) {
            int startLine = getLineIdx(c.start());
            int endLine = getLineIdx(c.end());
            if (c.hasSelection() && c.end() > c.start()) {
                if (c.end() > 0 && pt.charAt(c.end() - 1) == '\n') {
                    if (endLine > startLine) endLine--;
                }
            }
            for (int i = startLine; i <= endLine; ++i) lines.push_back(i);
        }
        std::sort(lines.begin(), lines.end());
        lines.erase(std::unique(lines.begin(), lines.end()), lines.end());
        return lines;
    }
    void duplicateLines(bool up) {
        commitPadding();
        if (cursors.empty()) return;
        std::vector<int> lines = getSelectedLineIndices();
        if (lines.empty()) return;
        EditBatch batch;
        batch.beforeCursors = cursors;
        std::string blockText;
        size_t blockStart = lineStarts[lines.front()];
        size_t blockEnd = (lines.back() + 1 < (int)lineStarts.size()) ? lineStarts[lines.back() + 1] : pt.length();
        blockText = pt.getRange(blockStart, blockEnd - blockStart);
        bool needNewline = false;
        if (!blockText.empty() && blockText.back() != '\n') {
            blockText += '\n';
            needNewline = true;
        }
        size_t insertPos;
        if (up) {
            insertPos = blockStart;
        }
        else {
            insertPos = blockEnd;
            if (needNewline && blockEnd == pt.length() && pt.charAt(blockEnd - 1) != '\n') {
                pt.insert(blockEnd, "\n");
                batch.ops.push_back({ EditOp::Insert, blockEnd, "\n" });
                insertPos++;
            }
        }
        pt.insert(insertPos, blockText);
        batch.ops.push_back({ EditOp::Insert, insertPos, blockText });
        batch.afterCursors.clear();
        size_t newSelectionStart = insertPos;
        size_t newSelectionEnd = insertPos + blockText.size();
        batch.afterCursors.push_back({ newSelectionEnd, newSelectionStart, getXFromPos(newSelectionEnd) });
        cursors = batch.afterCursors;
        undo.push(batch);
        rebuildLineStarts();
        ensureCaretVisible();
        updateDirtyFlag();
        InvalidateRect(hwnd, NULL, FALSE);
    }
    void moveLines(bool up) {
        commitPadding();
        if (cursors.empty()) return;
        std::vector<int> lines = getSelectedLineIndices();
        if (lines.empty()) return;
        if (up && lines.front() == 0) return;
        if (!up && lines.back() >= (int)lineStarts.size() - 1) return;
        int startLine = lines.front();
        int endLine = lines.back();
        size_t rangeStart = lineStarts[startLine];
        size_t rangeEnd = (endLine + 1 < (int)lineStarts.size()) ? lineStarts[endLine + 1] : pt.length();
        std::string textToMove = pt.getRange(rangeStart, rangeEnd - rangeStart);
        bool isLastLineNoNewline = (rangeEnd == pt.length()) && (!textToMove.empty() && textToMove.back() != '\n');
        EditBatch batch;
        batch.beforeCursors = cursors;
        if (up) {
            int targetLineIdx = startLine - 1;
            size_t targetStart = lineStarts[targetLineIdx];
            size_t targetEnd = rangeStart;
            std::string lineAbove = pt.getRange(targetStart, targetEnd - targetStart);
            long long diff = -(long long)(rangeStart - targetStart);
            if (isLastLineNoNewline) {
                textToMove += '\n';
                if (!lineAbove.empty() && lineAbove.back() == '\n') lineAbove.pop_back();
            }
            size_t deleteLen = rangeEnd - targetStart;
            std::string deletedAll = pt.getRange(targetStart, deleteLen);
            pt.erase(targetStart, deleteLen);
            batch.ops.push_back({ EditOp::Erase, targetStart, deletedAll });
            std::string newText = textToMove + lineAbove;
            pt.insert(targetStart, newText);
            batch.ops.push_back({ EditOp::Insert, targetStart, newText });
            for (auto& c : cursors) {
                c.head = (size_t)((long long)c.head + diff);
                c.anchor = (size_t)((long long)c.anchor + diff);
                c.desiredX = getXFromPos(c.head);
            }
        }
        else {
            int targetLineIdx = endLine + 1;
            size_t targetStart = rangeEnd;
            size_t targetEnd = (targetLineIdx + 1 < (int)lineStarts.size()) ? lineStarts[targetLineIdx + 1] : pt.length();
            std::string lineBelow = pt.getRange(targetStart, targetEnd - targetStart);
            if (targetEnd == pt.length() && (lineBelow.empty() || lineBelow.back() != '\n')) {
                lineBelow += '\n';
                if (!textToMove.empty() && textToMove.back() == '\n') textToMove.pop_back();
            }
            size_t deleteLen = targetEnd - rangeStart;
            std::string deletedAll = pt.getRange(rangeStart, deleteLen);
            pt.erase(rangeStart, deleteLen);
            batch.ops.push_back({ EditOp::Erase, rangeStart, deletedAll });
            std::string newText = lineBelow + textToMove;
            pt.insert(rangeStart, newText);
            batch.ops.push_back({ EditOp::Insert, rangeStart, newText });
            long long diff = (long long)lineBelow.size();
            for (auto& c : cursors) {
                c.head = (size_t)((long long)c.head + diff);
                c.anchor = (size_t)((long long)c.anchor + diff);
                c.desiredX = getXFromPos(c.head);
            }
        }
        batch.afterCursors = cursors;
        undo.push(batch);
        rebuildLineStarts();
        ensureCaretVisible();
        updateDirtyFlag();
        InvalidateRect(hwnd, NULL, FALSE);
    }
    void pasteFromClipboard() {
        if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) return;
        if (OpenClipboard(hwnd)) {
            bool isRect = IsClipboardFormatAvailable(cfMsDevCol);
            HGLOBAL h = GetClipboardData(CF_UNICODETEXT);
            if (h) {
                const wchar_t* p = (const wchar_t*)GlobalLock(h);
                if (p) {
                    std::wstring w(p);
                    GlobalUnlock(h);
                    std::string utf8 = WToUTF8(w);
                    if (isRect) {
                        insertRectangularBlock(utf8);
                    }
                    else {
                        insertAtCursors(utf8);
                    }
                }
            }
            CloseClipboard();
        }
    }
    void cutToClipboard() { copyToClipboard(); insertAtCursors(""); }
    void doInsert(size_t pos, const std::string& s) { cursors.clear(); cursors.push_back({ pos, pos, getXFromPos(pos) }); insertAtCursors(s); }
    void performUndo() { if (!undo.canUndo())return; EditBatch b = undo.popUndo(); for (int i = (int)b.ops.size() - 1; i >= 0; --i) { const auto& o = b.ops[i]; if (o.type == EditOp::Insert)pt.erase(o.pos, o.text.size()); else pt.insert(o.pos, o.text); }cursors = b.beforeCursors; rebuildLineStarts(); ensureCaretVisible(); updateDirtyFlag(); }
    void performRedo() { if (!undo.canRedo())return; EditBatch b = undo.popRedo(); for (const auto& o : b.ops) { if (o.type == EditOp::Insert)pt.insert(o.pos, o.text); else pt.erase(o.pos, o.text.size()); }cursors = b.afterCursors; rebuildLineStarts(); ensureCaretVisible(); updateDirtyFlag(); }
    int ShowTaskDialog(const wchar_t* title, const wchar_t* instruction, const wchar_t* content, TASKDIALOG_COMMON_BUTTON_FLAGS buttons, PCWSTR icon) { TASKDIALOGCONFIG c = { 0 }; c.cbSize = sizeof(c); c.hwndParent = hwnd; c.hInstance = GetModuleHandle(NULL); c.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW; c.pszWindowTitle = title; c.pszMainInstruction = instruction; c.pszContent = content; c.dwCommonButtons = buttons; c.pszMainIcon = icon; int n = 0; TaskDialogIndirect(&c, &n, NULL, NULL); return n; }
    bool checkUnsavedChanges() { if (!isDirty)return true; int r = ShowTaskDialog(L"確認", L"変更を保存しますか?", currentFilePath.empty() ? L"無題" : currentFilePath.c_str(), TDCBF_YES_BUTTON | TDCBF_NO_BUTTON | TDCBF_CANCEL_BUTTON, TD_WARNING_ICON); if (r == IDCANCEL)return false; if (r == IDYES) { if (currentFilePath.empty())return saveFileAs(); else return saveFile(currentFilePath); }return true; }
    bool openFile() { if (!checkUnsavedChanges())return false; WCHAR f[MAX_PATH] = { 0 }; OPENFILENAMEW o = { 0 }; o.lStructSize = sizeof(o); o.hwndOwner = hwnd; o.lpstrFile = f; o.nMaxFile = MAX_PATH; o.lpstrFilter = L"All\0*.*\0Text\0*.txt\0"; o.nFilterIndex = 1; o.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST; if (GetOpenFileNameW(&o)) { fileMap.reset(new MappedFile()); if (fileMap->open(f)) { pt.initFromFile(fileMap->ptr, fileMap->size); currentFilePath = f; undo.clear(); isDirty = false; undo.markSaved(); cursors.clear(); cursors.push_back({ 0,0,0.0f }); rebuildLineStarts(); updateTitleBar(); InvalidateRect(hwnd, NULL, FALSE); return true; } else ShowTaskDialog(L"エラー", L"開けません", f, TDCBF_OK_BUTTON, TD_ERROR_ICON); }return false; }
    bool saveFile(const std::wstring& p) {
        std::wstring t = p + L".tmp";
        HANDLE h = CreateFileW(t.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h == INVALID_HANDLE_VALUE) {
            ShowTaskDialog(L"エラー", L"一時ファイルの作成に失敗しました。", t.c_str(), TDCBF_OK_BUTTON, TD_ERROR_ICON);
            return false;
        }
        bool ok = true;
        for (const auto& piece : pt.pieces) {
            const char* ptr = piece.isOriginal ? (pt.origPtr + piece.start) : (pt.addBuf.data() + piece.start);
            DWORD w = 0;
            if (!WriteFile(h, ptr, (DWORD)piece.len, &w, NULL) || w != piece.len) {
                ok = false; break;
            }
        }
        CloseHandle(h);
        if (!ok) {
            DeleteFileW(t.c_str());
            ShowTaskDialog(L"エラー", L"データの書き込みに失敗しました。", p.c_str(), TDCBF_OK_BUTTON, TD_ERROR_ICON);
            return false;
        }
        std::vector<Cursor> savedCursors = cursors;
        int savedV = vScrollPos;
        int savedH = hScrollPos;
        std::wstring oldPath = currentFilePath;
        if (fileMap) {
            fileMap->close();
        }
        if (MoveFileExW(t.c_str(), p.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED) == 0) {
            DWORD err = GetLastError();
            DeleteFileW(t.c_str());
            if (!oldPath.empty()) {
                if (fileMap) fileMap->open(oldPath.c_str());
                if (fileMap->ptr) pt.origPtr = fileMap->ptr;
            }
            std::wstring msg = L"ファイルの保存に失敗しました。\nエラーコード: " + std::to_wstring(err);
            ShowTaskDialog(L"エラー", msg.c_str(), p.c_str(), TDCBF_OK_BUTTON, TD_ERROR_ICON);
            return false;
        }
        if (!openFileFromPath(p)) {
            ShowTaskDialog(L"致命的エラー", L"保存後のファイルを開けませんでした。", p.c_str(), TDCBF_OK_BUTTON, TD_ERROR_ICON);
            return false;
        }
        cursors = savedCursors;
        vScrollPos = savedV;
        hScrollPos = savedH;
        updateScrollBars();
        ensureCaretVisible();
        updateTitleBar();
        return true;
    }
    bool saveFileAs() { WCHAR f[MAX_PATH] = { 0 }; OPENFILENAMEW o = { 0 }; o.lStructSize = sizeof(o); o.hwndOwner = hwnd; o.lpstrFile = f; o.nMaxFile = MAX_PATH; o.lpstrFilter = L"All\0*.*\0Text\0*.txt\0"; o.nFilterIndex = 1; o.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT; if (GetSaveFileNameW(&o))return saveFile(f); return false; }
    void newFile() { if (!checkUnsavedChanges())return; pt.initEmpty(); currentFilePath.clear(); undo.clear(); isDirty = false; cursors.clear(); cursors.push_back({ 0,0,0.0f }); vScrollPos = 0; hScrollPos = 0; fileMap.reset(); rebuildLineStarts(); updateTitleBar(); InvalidateRect(hwnd, NULL, FALSE); }
    void selectNextOccurrence() {
        if (cursors.empty()) return;
        Cursor c = cursors.back();
        if (!c.hasSelection()) {
            size_t targetPos = c.head;
            if (targetPos > 0) {
                char currChar = pt.charAt(targetPos);
                char prevChar = pt.charAt(targetPos - 1);
                if (!isWordChar(currChar) && isWordChar(prevChar)) {
                    targetPos--;
                }
            }
            selectWordAt(targetPos);
            InvalidateRect(hwnd, NULL, FALSE);
            return;
        }
        size_t start = c.start();
        size_t len = c.end() - start;
        std::string query = pt.getRange(start, len);
        size_t nextPos = findText(std::max(c.head, c.anchor), query, true, true, false, false);
        if (nextPos != std::string::npos) {
            for (const auto& cur : cursors) {
                if (cur.start() == nextPos) return;
            }
            cursors.push_back({ nextPos + len, nextPos, getXFromPos(nextPos + len) });
            ensureCaretVisible();
            InvalidateRect(hwnd, NULL, FALSE);
        }
    }
    bool openFileFromPath(const std::wstring& path) {
        fileMap.reset(new MappedFile());
        if (fileMap->open(path.c_str())) {
            pt.initFromFile(fileMap->ptr, fileMap->size);
            currentFilePath = path;
            undo.clear();
            isDirty = false;
            undo.markSaved();
            cursors.clear();
            cursors.push_back({ 0, 0, 0.0f });
            vScrollPos = 0; hScrollPos = 0;
            rebuildLineStarts();
            updateTitleBar();
            InvalidateRect(hwnd, NULL, FALSE);
            return true;
        }
        else {
            ShowTaskDialog(L"エラー", L"ファイルを開けませんでした。", path.c_str(), TDCBF_OK_BUTTON, TD_ERROR_ICON);
            return false;
        }
    }
} g_editor;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        g_editor.initGraphics(hwnd);
        DragAcceptFiles(hwnd, TRUE);
        break;
    case WM_SIZE: if (g_editor.rend) { RECT rc; GetClientRect(hwnd, &rc); g_editor.rend->Resize(D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top)); g_editor.updateScrollBars(); InvalidateRect(hwnd, NULL, FALSE); } break;
    case WM_LBUTTONDOWN: {
        if (g_editor.showHelpPopup) { g_editor.showHelpPopup = false; InvalidateRect(hwnd, NULL, FALSE); }
        int x = (short)LOWORD(lParam), y = (short)HIWORD(lParam); SetCapture(hwnd); g_editor.isDragging = true; g_editor.rollbackPadding();
        if (abs(x - g_editor.lastClickX) < 5 && abs(y - g_editor.lastClickY) < 5 && (GetMessageTime() - g_editor.lastClickTime < GetDoubleClickTime())) g_editor.clickCount++; else g_editor.clickCount = 1;
        g_editor.lastClickTime = GetMessageTime(); g_editor.lastClickX = x; g_editor.lastClickY = y;
        if (g_editor.clickCount == 1 && !(GetKeyState(VK_SHIFT) & 0x8000)) {
            size_t p = g_editor.getDocPosFromPoint(x, y);
            bool inSel = false; for (const auto& c : g_editor.cursors) if (c.hasSelection() && p >= c.start() && p < c.end()) inSel = true;
            if (inSel) { g_editor.isDragMovePending = true; g_editor.dragMoveSourceStart = g_editor.cursors.back().start(); g_editor.dragMoveSourceEnd = g_editor.cursors.back().end(); return 0; }
        }
        g_editor.isDragMovePending = false; g_editor.isDragMoving = false;
        if (GetKeyState(VK_MENU) & 0x8000) { g_editor.isRectSelecting = true; float vx = x / g_editor.dpiScaleX - g_editor.gutterWidth + g_editor.hScrollPos; float vy = y / g_editor.dpiScaleY + (g_editor.vScrollPos * g_editor.lineHeight); g_editor.rectAnchorX = g_editor.rectHeadX = vx; g_editor.rectAnchorY = g_editor.rectHeadY = vy; g_editor.updateRectSelection(); }
        else g_editor.isRectSelecting = false;
        if (x / g_editor.dpiScaleX < g_editor.gutterWidth) {
            int line = g_editor.vScrollPos + (int)(y / g_editor.dpiScaleY / g_editor.lineHeight);
            if (line >= 0 && line < (int)g_editor.lineStarts.size()) { size_t s = g_editor.lineStarts[line]; size_t e = (line + 1 < (int)g_editor.lineStarts.size()) ? g_editor.lineStarts[line + 1] : g_editor.pt.length(); g_editor.cursors.clear(); g_editor.cursors.push_back({ e, s, g_editor.getXFromPos(e) }); }
        }
        else {
            size_t p = g_editor.getDocPosFromPoint(x, y);
            if (g_editor.clickCount == 2) g_editor.selectWordAt(p); else if (g_editor.clickCount == 3) g_editor.selectLineAt(p);
            else { if (GetKeyState(VK_SHIFT) & 0x8000) { if (!g_editor.cursors.empty()) { g_editor.cursors.back().head = p; g_editor.cursors.back().desiredX = g_editor.getXFromPos(p); } } else if (GetKeyState(VK_CONTROL) & 0x8000) g_editor.cursors.push_back({ p, p, g_editor.getXFromPos(p) }); else { g_editor.cursors.clear(); g_editor.cursors.push_back({ p, p, g_editor.getXFromPos(p) }); } }
        }
        InvalidateRect(hwnd, NULL, FALSE);
    } break;
    case WM_MOUSEMOVE: {
        int x = (short)LOWORD(lParam), y = (short)HIWORD(lParam);
        if (g_editor.isDragMovePending) {
            if (abs(x - g_editor.lastClickX) > 5 || abs(y - g_editor.lastClickY) > 5) {
                g_editor.isDragMovePending = false;
                g_editor.isDragMoving = true;
                SetCursor(LoadCursor(NULL, IDC_ARROW));
            }
        }
        if (g_editor.isDragMoving) {
            g_editor.dragMoveDestPos = g_editor.getDocPosFromPoint(x, y);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        if (g_editor.isDragging && !g_editor.isDragMovePending) {
            if (g_editor.isRectSelecting) {
                float vx = x / g_editor.dpiScaleX - g_editor.gutterWidth + g_editor.hScrollPos;
                float vy = y / g_editor.dpiScaleY + (g_editor.vScrollPos * g_editor.lineHeight);
                g_editor.rectHeadX = vx;
                g_editor.rectHeadY = vy;
                g_editor.updateRectSelection();
            }
            else {
                size_t p = g_editor.getDocPosFromPoint(x, y);
                if (!g_editor.cursors.empty()) {
                    g_editor.cursors.back().head = p;
                    g_editor.cursors.back().desiredX = g_editor.getXFromPos(p);
                }
            }
            InvalidateRect(hwnd, NULL, FALSE);
        }
    } break;
    case WM_LBUTTONUP:
        if (g_editor.isDragMovePending) { g_editor.isDragMovePending = false; size_t p = g_editor.getDocPosFromPoint((short)LOWORD(lParam), (short)HIWORD(lParam)); g_editor.cursors.clear(); g_editor.cursors.push_back({ p, p, g_editor.getXFromPos(p) }); InvalidateRect(hwnd, NULL, FALSE); }
        else if (g_editor.isDragMoving) { g_editor.performDragMove(); }
        g_editor.isDragging = false; g_editor.isDragMoving = false; g_editor.isRectSelecting = false; g_editor.mergeCursors(); ReleaseCapture(); break;
    case WM_VSCROLL: {
        RECT rc; GetClientRect(hwnd, &rc); int page = (int)((rc.bottom / g_editor.dpiScaleY) / g_editor.lineHeight);
    switch (LOWORD(wParam)) { case SB_LINEUP: g_editor.vScrollPos--; break; case SB_LINEDOWN: g_editor.vScrollPos++; break; case SB_PAGEUP: g_editor.vScrollPos -= page; break; case SB_PAGEDOWN: g_editor.vScrollPos += page; break; case SB_THUMBTRACK: { SCROLLINFO si = { sizeof(SCROLLINFO), SIF_TRACKPOS }; GetScrollInfo(hwnd, SB_VERT, &si); g_editor.vScrollPos = si.nTrackPos; } break; }
                                            if (g_editor.vScrollPos < 0) g_editor.vScrollPos = 0; if (g_editor.vScrollPos > (int)g_editor.lineStarts.size()) g_editor.vScrollPos = (int)g_editor.lineStarts.size(); g_editor.updateScrollBars(); InvalidateRect(hwnd, NULL, FALSE);
    } break;
    case WM_HSCROLL: {
    switch (LOWORD(wParam)) { case SB_LINELEFT: g_editor.hScrollPos -= 10; break; case SB_LINERIGHT: g_editor.hScrollPos += 10; break; case SB_PAGELEFT: g_editor.hScrollPos -= 100; break; case SB_PAGERIGHT: g_editor.hScrollPos += 100; break; case SB_THUMBTRACK: { SCROLLINFO si = { sizeof(SCROLLINFO), SIF_TRACKPOS }; GetScrollInfo(hwnd, SB_HORZ, &si); g_editor.hScrollPos = si.nTrackPos; } break; }
                                              if (g_editor.hScrollPos < 0) g_editor.hScrollPos = 0; g_editor.updateScrollBars(); InvalidateRect(hwnd, NULL, FALSE);
    } break;
    case WM_MOUSEWHEEL:
        if (GET_KEYSTATE_WPARAM(wParam) & MK_CONTROL) {
            float s = (GET_WHEEL_DELTA_WPARAM(wParam) > 0) ? 1.1f : 0.9f; g_editor.updateFont(g_editor.currentFontSize * s);
            g_editor.zoomPopupEndTime = GetTickCount() + 1000; std::wstringstream ss; ss << (int)g_editor.currentFontSize << L"px"; g_editor.zoomPopupText = ss.str(); SetTimer(hwnd, 1, 1000, NULL);
        }
        else {
            g_editor.vScrollPos -= GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA * 3;
            if (g_editor.vScrollPos < 0) g_editor.vScrollPos = 0; if (g_editor.vScrollPos > (int)g_editor.lineStarts.size()) g_editor.vScrollPos = (int)g_editor.lineStarts.size();
            g_editor.updateScrollBars();
        }
        InvalidateRect(hwnd, NULL, FALSE); break;
    case WM_TIMER: if (wParam == 1) { KillTimer(hwnd, 1); InvalidateRect(hwnd, NULL, FALSE); } break;
    case WM_CHAR: {
        if (g_editor.showHelpPopup) { g_editor.showHelpPopup = false; InvalidateRect(hwnd, NULL, FALSE); }

        wchar_t c = (wchar_t)wParam; if (c < 32 && c != 8 && c != 13) break;
        if (c == 8) { g_editor.highSurrogate = 0; g_editor.backspaceAtCursors(); }
        else if (c == 13) { g_editor.highSurrogate = 0; g_editor.insertAtCursors("\n"); }
        else {
            if (c >= 0xD800 && c <= 0xDBFF) { g_editor.highSurrogate = c; return 0; }
            std::wstring s; if (c >= 0xDC00 && c <= 0xDFFF) { if (g_editor.highSurrogate) { s += g_editor.highSurrogate; s += c; g_editor.highSurrogate = 0; } else return 0; }
            else { g_editor.highSurrogate = 0; s += c; }
            g_editor.insertAtCursors(WToUTF8(s));
        }
    } break;
    case WM_IME_STARTCOMPOSITION: return 0;
    case WM_IME_COMPOSITION: {
        HIMC h = ImmGetContext(hwnd); if (h) {
            if (lParam & GCS_RESULTSTR) { DWORD s = ImmGetCompositionStringW(h, GCS_RESULTSTR, NULL, 0); if (s) { std::vector<wchar_t> b(s / 2); ImmGetCompositionStringW(h, GCS_RESULTSTR, b.data(), s); g_editor.insertAtCursors(WToUTF8(std::wstring(b.begin(), b.end()))); g_editor.imeComp.clear(); } }
            if (lParam & GCS_COMPSTR) { DWORD s = ImmGetCompositionStringW(h, GCS_COMPSTR, NULL, 0); if (s) { std::vector<wchar_t> b(s / 2); ImmGetCompositionStringW(h, GCS_COMPSTR, b.data(), s); g_editor.imeComp = WToUTF8(std::wstring(b.begin(), b.end())); } else g_editor.imeComp.clear(); }
            ImmReleaseContext(hwnd, h); InvalidateRect(hwnd, NULL, FALSE);
        } return 0;
    } break;
    case WM_IME_ENDCOMPOSITION: g_editor.imeComp.clear(); InvalidateRect(hwnd, NULL, FALSE); break;
    case WM_IME_SETCONTEXT: lParam &= ~ISC_SHOWUICOMPOSITIONWINDOW; return DefWindowProc(hwnd, msg, wParam, lParam);
    case WM_SYSKEYDOWN:
        if (wParam == VK_UP || wParam == VK_DOWN) {
            bool shift = (GetKeyState(VK_SHIFT) & 0x8000);
            if (shift) {
                g_editor.duplicateLines(wParam == VK_UP);
            }
            else {
                g_editor.moveLines(wParam == VK_UP);
            }
            return 0;
        }
        if (wParam != VK_LEFT && wParam != VK_RIGHT && wParam != VK_F4) return DefWindowProc(hwnd, msg, wParam, lParam);
        break;
    case WM_KEYDOWN:
        if (GetKeyState(VK_CONTROL) & 0x8000) {
            switch (wParam) {
            case 'O': g_editor.openFile(); return 0;
            case 'N': g_editor.newFile(); return 0;
            case 'S':
                if (GetKeyState(VK_SHIFT) & 0x8000) g_editor.saveFileAs();
                else if (g_editor.currentFilePath.empty()) g_editor.saveFileAs();
                else g_editor.saveFile(g_editor.currentFilePath);
                return 0;
            case 'Z': g_editor.performUndo(); return 0;
            case 'Y': g_editor.performRedo(); return 0;
            case 'C': case VK_INSERT: g_editor.copyToClipboard(); return 0;
            case 'X': g_editor.cutToClipboard(); return 0;
            case 'V': g_editor.pasteFromClipboard(); return 0;
            case 'D': g_editor.selectNextOccurrence(); return 0;
            case 'U':
                if (GetKeyState(VK_SHIFT) & 0x8000) g_editor.convertCase(false);
                else g_editor.convertCase(true);
                return 0;
            case 'A': { g_editor.rollbackPadding(); g_editor.cursors.clear(); g_editor.cursors.push_back({ g_editor.pt.length(), 0, 0.0f }); InvalidateRect(hwnd, NULL, FALSE); return 0; }
            case VK_ADD: case VK_OEM_PLUS: {
                g_editor.updateFont(g_editor.currentFontSize * 1.1f);
                g_editor.zoomPopupEndTime = GetTickCount() + 1000;
                std::wstringstream ss; ss << (int)g_editor.currentFontSize << L"px"; g_editor.zoomPopupText = ss.str();
                SetTimer(hwnd, 1, 1000, NULL);
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            case VK_SUBTRACT: case VK_OEM_MINUS: {
                g_editor.updateFont(g_editor.currentFontSize * 0.9f);
                g_editor.zoomPopupEndTime = GetTickCount() + 1000;
                std::wstringstream ss; ss << (int)g_editor.currentFontSize << L"px"; g_editor.zoomPopupText = ss.str();
                SetTimer(hwnd, 1, 1000, NULL);
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            case '0': case VK_NUMPAD0: {
                g_editor.updateFont(21.0f);
                g_editor.zoomPopupEndTime = GetTickCount() + 1000;
                std::wstringstream ss; ss << (int)g_editor.currentFontSize << L"px"; g_editor.zoomPopupText = ss.str();
                SetTimer(hwnd, 1, 1000, NULL);
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            default: break;
            }
        }
        if ((GetKeyState(VK_SHIFT) & 0x8000) && wParam == VK_INSERT) { g_editor.pasteFromClipboard(); return 0; }
        if (wParam == VK_ESCAPE) { g_editor.rollbackPadding(); if (!g_editor.cursors.empty()) { Cursor c = g_editor.cursors.back(); c.anchor = c.head; g_editor.cursors.clear(); g_editor.cursors.push_back(c); g_editor.isRectSelecting = false; InvalidateRect(hwnd, NULL, FALSE); } return 0; }
        if (wParam == VK_DELETE) { g_editor.rollbackPadding(); g_editor.isRectSelecting = false; g_editor.deleteForwardAtCursors(); return 0; }
        if (g_editor.showHelpPopup) { g_editor.showHelpPopup = false; InvalidateRect(hwnd, NULL, FALSE); }
        if (wParam == VK_LEFT || wParam == VK_RIGHT || wParam == VK_UP || wParam == VK_DOWN ||
            wParam == VK_HOME || wParam == VK_END || wParam == VK_PRIOR || wParam == VK_NEXT) {
            bool shift = (GetKeyState(VK_SHIFT) & 0x8000);
            bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000);
            bool alt = (GetKeyState(VK_MENU) & 0x8000);
            if (alt && shift && (wParam == VK_LEFT || wParam == VK_RIGHT || wParam == VK_UP || wParam == VK_DOWN)) {
                if (!g_editor.isRectSelecting) {
                    g_editor.isRectSelecting = true;
                    float vx = 0, vy = 0;
                    g_editor.getCaretPoint(vx, vy);
                    g_editor.rectAnchorX = g_editor.rectHeadX = vx / g_editor.dpiScaleX - g_editor.gutterWidth + g_editor.hScrollPos;
                    g_editor.rectAnchorY = g_editor.rectHeadY = vy / g_editor.dpiScaleY + (g_editor.vScrollPos * g_editor.lineHeight);
                }
                if (wParam == VK_LEFT || wParam == VK_RIGHT) {
                    int lineIdx = (int)(g_editor.rectHeadY / g_editor.lineHeight);
                    if (lineIdx < 0) lineIdx = 0;
                    if (lineIdx >= (int)g_editor.lineStarts.size()) lineIdx = (int)g_editor.lineStarts.size() - 1;
                    size_t pos = g_editor.getPosFromLineAndX(lineIdx, g_editor.rectHeadX);
                    float textEndX = g_editor.getXFromPos(pos);
                    bool inVirtualSpace = (g_editor.rectHeadX > textEndX + 1.0f);
                    if (inVirtualSpace) {
                        if (wParam == VK_LEFT) {
                            g_editor.rectHeadX -= g_editor.charWidth;
                            if (g_editor.rectHeadX < textEndX) g_editor.rectHeadX = textEndX;
                        }
                        else {
                            g_editor.rectHeadX += g_editor.charWidth;
                        }
                    }
                    else {
                        bool forward = (wParam == VK_RIGHT);
                        size_t nextPos = g_editor.moveCaretVisual(pos, forward);
                        g_editor.rectHeadX = g_editor.getXFromPos(nextPos);
                    }
                }
                if (wParam == VK_UP) g_editor.rectHeadY -= g_editor.lineHeight;
                if (wParam == VK_DOWN) g_editor.rectHeadY += g_editor.lineHeight;
                g_editor.updateRectSelection();
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            g_editor.rollbackPadding(); g_editor.isRectSelecting = false;
            for (auto& c : g_editor.cursors) {
                if (wParam == VK_LEFT) { if (c.hasSelection() && !shift) { c.head = c.start(); c.anchor = c.head; } else { if (ctrl) c.head = g_editor.moveWordLeft(c.head); else c.head = g_editor.moveCaretVisual(c.head, false); if (!shift) c.anchor = c.head; } }
                else if (wParam == VK_RIGHT) { if (c.hasSelection() && !shift) { c.head = c.end(); c.anchor = c.head; } else { if (ctrl) c.head = g_editor.moveWordRight(c.head); else c.head = g_editor.moveCaretVisual(c.head, true); if (!shift) c.anchor = c.head; } }
                else if (wParam == VK_UP) { int l = g_editor.getLineIdx(c.head); if (l > 0) c.head = g_editor.getPosFromLineAndX(l - 1, c.desiredX); if (!shift) c.anchor = c.head; }
                else if (wParam == VK_DOWN) { int l = g_editor.getLineIdx(c.head); if (l + 1 < (int)g_editor.lineStarts.size()) c.head = g_editor.getPosFromLineAndX(l + 1, c.desiredX); if (!shift) c.anchor = c.head; }
                else if (wParam == VK_HOME) { if (ctrl) c.head = 0; else { size_t p = c.head; while (p > 0 && g_editor.pt.charAt(p - 1) != '\n') p--; c.head = p; } if (!shift) c.anchor = c.head; }
                else if (wParam == VK_END) { if (ctrl) c.head = g_editor.pt.length(); else { size_t p = c.head; size_t len = g_editor.pt.length(); while (p < len && g_editor.pt.charAt(p) != '\n') p++; c.head = p; } if (!shift) c.anchor = c.head; }
                else if (wParam == VK_PRIOR) { RECT r; GetClientRect(hwnd, &r); int p = (int)((r.bottom / g_editor.dpiScaleY) / g_editor.lineHeight); int l = g_editor.getLineIdx(c.head); c.head = g_editor.getPosFromLineAndX(std::max(0, l - p), c.desiredX); if (!shift) c.anchor = c.head; }
                else if (wParam == VK_NEXT) { RECT r; GetClientRect(hwnd, &r); int p = (int)((r.bottom / g_editor.dpiScaleY) / g_editor.lineHeight); int l = g_editor.getLineIdx(c.head); c.head = g_editor.getPosFromLineAndX(std::min((int)g_editor.lineStarts.size() - 1, l + p), c.desiredX); if (!shift) c.anchor = c.head; }
                if (wParam == VK_LEFT || wParam == VK_RIGHT || wParam == VK_HOME || wParam == VK_END) c.desiredX = g_editor.getXFromPos(c.head);
            }
            g_editor.mergeCursors(); g_editor.ensureCaretVisible(); InvalidateRect(hwnd, NULL, FALSE);
        }
        break;
    case WM_DROPFILES: {
        if (g_editor.checkUnsavedChanges()) {
            HDROP hDrop = (HDROP)wParam;
            WCHAR file[MAX_PATH];
            if (DragQueryFileW(hDrop, 0, file, MAX_PATH) && g_editor.openFileFromPath(file) && g_editor.showHelpPopup) {
                g_editor.showHelpPopup = false; InvalidateRect(hwnd, NULL, FALSE);
            }
            DragFinish(hDrop);
        }
    } break;
    case WM_CLOSE: if (g_editor.checkUnsavedChanges()) DestroyWindow(hwnd); return 0;
    case WM_PAINT: g_editor.render(); break;
    case WM_DESTROY: g_editor.destroyGraphics(); PostQuitMessage(0); break;
    default: return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    WNDCLASS wc = { 0 }; wc.lpfnWndProc = WndProc; wc.hInstance = hInstance; wc.lpszClassName = L"miu"; wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1)); wc.hCursor = LoadCursor(NULL, IDC_IBEAM); wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); RegisterClass(&wc);
    HWND hwnd = CreateWindowEx(0, wc.lpszClassName, L"miu", WS_OVERLAPPEDWINDOW | WS_VSCROLL | WS_HSCROLL, CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, NULL, NULL, hInstance, NULL);
    if (!hwnd) return 0; ShowWindow(hwnd, nCmdShow);
    if (g_editor.currentFilePath.empty()) {
        int argc; wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (argc >= 2) {
            g_editor.openFileFromPath(argv[1]);
        }
        else {
            g_editor.showHelpPopup = true;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        LocalFree(argv);
    }
    g_editor.updateTitleBar();
    MSG msg; while (GetMessage(&msg, NULL, 0, 0)) {
        if (msg.message == WM_KEYDOWN) {
            if (msg.wParam == VK_F1) {
                g_editor.showHelpPopup = true;
                InvalidateRect(hwnd, NULL, FALSE);
                continue;
            }
            if (msg.wParam == VK_F3) {
                bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                g_editor.findNext(!shift);
                continue;
            }
            if (GetKeyState(VK_CONTROL) & 0x8000) {
                if (msg.wParam == 'F') {
                    g_editor.showFindDialog(false);
                    continue;
                }
                if (msg.wParam == 'H') {
                    g_editor.showFindDialog(true);
                    continue;
                }
            }
        }
        if (g_editor.showHelpPopup && (msg.message == WM_KEYDOWN || msg.message == WM_CHAR || msg.message == WM_LBUTTONDOWN)) {
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_F1) {}
            else {
                g_editor.showHelpPopup = false;
                InvalidateRect(hwnd, NULL, FALSE);
            }
        }
        if (!g_editor.hFindDlg || !IsDialogMessage(g_editor.hFindDlg, &msg)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    }
    return 0;
}