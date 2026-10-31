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
#include <cstring>
#include <cstdint>

#if defined(__APPLE__)
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>
#endif

#if __has_include("compact_enc_det/compact_enc_det.h")
#include "compact_enc_det/compact_enc_det.h"
#elif __has_include("../third_party/ced/compact_enc_det/compact_enc_det.h")
#include "../third_party/ced/compact_enc_det/compact_enc_det.h"
#elif __has_include("../../macOS/third_party/ced/compact_enc_det/compact_enc_det.h")
#include "../../macOS/third_party/ced/compact_enc_det/compact_enc_det.h"
#endif

#if __has_include("MiuEmmet.h")
#include "MiuEmmet.h"
#else
#include "../../include/MiuEmmet.h"
#endif

#if __has_include("MiuSyntax.h")
#include "MiuSyntax.h"
#else
#include "../../include/MiuSyntax.h"
#endif

#if __has_include("MiuPreview.h")
#include "MiuPreview.h"
#else
#include "../../include/MiuPreview.h"
#endif

extern const std::wstring APP_VERSION;
extern const std::wstring APP_TITLE;

enum MiuEncoding {
    ENC_UTF8_NOBOM = 0,
    ENC_UTF8_BOM,
    ENC_UTF16LE,
    ENC_UTF16BE,
    ENC_LOCAL
};

struct DetectResult {
    MiuEncoding type;
    uint32_t codePage;
};

std::string WToUTF8(const std::wstring& w);
std::wstring UTF8ToW(const std::string& s);
std::string Utf16ToUtf8(const char* data, size_t len, bool isBigEndian);
Encoding DetectEncoding(const char* buf, size_t len);
DetectResult DetectEncodingEx(const char* buf, size_t len);
bool IsValidUtf8(const char* buf, size_t len);
std::string ConvertCase(const std::string& s, bool toUpper);
bool MiuIsLikelyTemporaryGuidFileName(const std::string& fileName);
std::string MiuExtractRealFileNameFromStagingName(const std::string& fileName);
std::string MiuBuildExportFileName(const std::string& currentPathUtf8,
                                   const std::string& currentDisplayNameUtf8,
                                   const std::string& currentFileExtUtf8,
                                   const std::string& localizedUntitledUtf8 = "Untitled");
std::string MiuResolveDisplayFileName(const std::vector<std::string>& candidatesUtf8,
                                      const std::string& fallbackNameUtf8,
                                      const std::string& localizedUntitledUtf8 = "Untitled");

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
    size_t totalLength = 0;
    mutable size_t cachedIdx = 0;
    mutable size_t cachedStart = 0;

    void initFromFile(const char* data, size_t size) {
        origPtr = data;
        origSize = size;
        pieces.clear();
        addBuf.clear();
        totalLength = size;
        cachedIdx = 0;
        cachedStart = 0;
        if (size > 0) pieces.push_back({ true, 0, size });
    }

    void initEmpty() {
        origPtr = nullptr;
        origSize = 0;
        pieces.clear();
        addBuf.clear();
        totalLength = 0;
        cachedIdx = 0;
        cachedStart = 0;
    }

    size_t length() const { return totalLength; }

    void coalesceAround(size_t idx) {
        if (pieces.empty()) return;
        size_t start = (idx > 0) ? (idx - 1) : 0;
        while (start + 1 < pieces.size()) {
            Piece& a = pieces[start];
            Piece& b = pieces[start + 1];
            if (a.isOriginal == b.isOriginal && a.start + a.len == b.start) {
                a.len += b.len;
                pieces.erase(pieces.begin() + start + 1);
            } else {
                if (start > idx + 1) break;
                start++;
            }
        }
        cachedIdx = 0;
        cachedStart = 0;
    }

    void getRange(size_t pos, size_t count, std::string& out) const {
        out.clear();
        if (pos >= totalLength || count == 0) return;
        count = std::min(count, totalLength - pos);
        out.reserve(count);
        size_t cur = 0, idx = 0;
        if (cachedIdx < pieces.size() && cachedStart <= pos) {
            cur = cachedStart;
            idx = cachedIdx;
        }
        for (; idx < pieces.size(); ++idx) {
            const auto& p = pieces[idx];
            if (cur + p.len <= pos) { cur += p.len; continue; }
            if (out.empty()) { cachedIdx = idx; cachedStart = cur; }
            size_t localStart = (pos > cur) ? (pos - cur) : 0;
            size_t take = std::min(p.len - localStart, count - out.size());
            if (take == 0) break;
            p.isOriginal ? out.append(origPtr + p.start + localStart, take) : out.append(addBuf.data() + p.start + localStart, take);
            if (out.size() >= count) break;
            cur += p.len;
        }
    }

    std::string getRange(size_t pos, size_t count) const {
        std::string out;
        getRange(pos, count, out);
        return out;
    }

    char charAt(size_t pos) const {
        if (pos >= totalLength) return ' ';
        if (cachedIdx < pieces.size()) {
            const Piece& cp = pieces[cachedIdx];
            if (pos >= cachedStart && pos < cachedStart + cp.len) {
                size_t off = pos - cachedStart;
                return cp.isOriginal ? origPtr[cp.start + off] : addBuf[cp.start + off];
            }
            if (pos >= cachedStart + cp.len && cachedIdx + 1 < pieces.size()) {
                const Piece& np = pieces[cachedIdx + 1];
                size_t nextStart = cachedStart + cp.len;
                if (pos < nextStart + np.len) {
                    cachedIdx++;
                    cachedStart = nextStart;
                    size_t off = pos - nextStart;
                    return np.isOriginal ? origPtr[np.start + off] : addBuf[np.start + off];
                }
            }
        }
        size_t cur = 0;
        for (size_t i = 0; i < pieces.size(); ++i) {
            const auto& p = pieces[i];
            if (cur + p.len <= pos) { cur += p.len; continue; }
            cachedIdx = i;
            cachedStart = cur;
            return p.isOriginal ? origPtr[p.start + pos - cur] : addBuf[p.start + pos - cur];
        }
        return ' ';
    }

    void insert(size_t pos, const std::string& s) {
        if (s.empty()) return;
        if (pos > totalLength) pos = totalLength;
        size_t cur = 0, idx = 0;
        while (idx < pieces.size() && cur + pieces[idx].len < pos) { cur += pieces[idx].len; ++idx; }
        if (idx < pieces.size()) {
            Piece p = pieces[idx];
            size_t offset = pos - cur;
            if (offset > 0 && offset < p.len) {
                pieces[idx] = { p.isOriginal, p.start, offset };
                pieces.insert(pieces.begin() + idx + 1, { p.isOriginal, p.start + offset, p.len - offset });
                idx++;
            } else if (offset == p.len) {
                idx++;
            }
        }
        size_t addStart = addBuf.size();
        addBuf.append(s);
        pieces.insert(pieces.begin() + idx, { false, addStart, (size_t)s.size() });
        totalLength += s.size();
        coalesceAround(idx);
    }

    void erase(size_t pos, size_t count) {
        if (count == 0 || pos >= totalLength) return;
        if (pos + count > totalLength) count = totalLength - pos;
        size_t cur = 0, idx = 0;
        while (idx < pieces.size() && cur + pieces[idx].len <= pos) { cur += pieces[idx].len; ++idx; }
        if (idx >= pieces.size()) return;
        if (pos > cur) {
            Piece p = pieces[idx];
            size_t leftLen = pos - cur;
            pieces[idx] = { p.isOriginal, p.start, leftLen };
            pieces.insert(pieces.begin() + idx + 1, { p.isOriginal, p.start + leftLen, p.len - leftLen });
            idx++;
        }
        size_t remaining = count;
        while (idx < pieces.size() && remaining > 0) {
            if (pieces[idx].len <= remaining) {
                remaining -= pieces[idx].len;
                pieces.erase(pieces.begin() + idx);
            } else {
                pieces[idx].start += remaining;
                pieces[idx].len -= remaining;
                remaining = 0;
            }
        }
        totalLength -= count;
        coalesceAround(idx);
    }
};

struct Cursor {
    size_t head = 0;
    size_t anchor = 0;
    float desiredX = 0.0f;
    float originalAnchorX = 0.0f;
    bool isVirtual = false;
    bool affinity = false;

    size_t start() const { return std::min(head, anchor); }
    size_t end() const { return std::max(head, anchor); }
    bool hasSelection() const { return head != anchor; }
};

struct EditOp {
    enum Type { Insert, Erase } type;
    size_t pos;
    std::string text;
};

struct EditBatch {
    std::vector<EditOp> ops;
    std::vector<Cursor> beforeCursors, afterCursors;
};

struct UndoManager {
    std::vector<EditBatch> undoStack, redoStack;
    int savePoint = 0;
    void clear() { undoStack.clear(); redoStack.clear(); savePoint = 0; }
    void markSaved() { savePoint = (int)undoStack.size(); }
    bool isModified() const { return (int)undoStack.size() != savePoint; }
    void push(const EditBatch& b) { undoStack.push_back(b); redoStack.clear(); }
    EditBatch popUndo() { EditBatch e = undoStack.back(); undoStack.pop_back(); redoStack.push_back(e); return e; }
    EditBatch popRedo() { EditBatch e = redoStack.back(); redoStack.pop_back(); undoStack.push_back(e); return e; }
};

struct MappedFile {
    int fd = -1;
    char* ptr = nullptr;
    size_t size = 0;
    std::string winFallbackBuf;
    bool open(const char* path);
    void close();
    ~MappedFile() { close(); }
};

struct PaddingRecord {
    size_t pos;
    size_t len;
};

struct ScrollBarLayout {
    bool needV = false;
    bool needH = false;
    float vBarW = 0.0f;
    float hBarH = 0.0f;
    float textAreaW = 0.0f;
    float textAreaH = 0.0f;
    int visibleLines = 1;
    int totalLines = 1;
    int totalVisualLines = 1;
};

struct ScrollMetrics {
    float trackStart = 0.0f;
    float trackEnd = 0.0f;
    float trackLen = 0.0f;
    float thumbSize = 0.0f;
    float thumbPos = 0.0f;
    int maxScroll = 0;
};

struct VisualSubLine {
    size_t byteStart = 0;
    size_t byteLen = 0;
    size_t utf16Start = 0;
    size_t utf16Len = 0;
};

struct ActiveEmmetSession {
    bool active = false;
    size_t baseOffset = 0;
    std::vector<Emmet::TabstopGroup> groups;
    int currentGroupIndex = -1;
};

struct FileViewState {
    int vScrollPos = 0;
    int vScrollSubLine = 0;
    int hScrollPos = 0;
    size_t cursorHead = 0;
    size_t cursorAnchor = 0;
    bool wordWrap = false;
};

struct Editor {
    PieceTable pt;
    UndoManager undo;
    std::unique_ptr<MappedFile> fileMap;
    std::wstring currentFilePath;
    std::string currentFileExt;
    MiuEncoding currentEncoding = ENC_UTF8_NOBOM;
    uint32_t currentCodePage = 0;
    bool isDirty = false;
    bool isDarkMode = false;
    bool isOverwriteMode = false;
    bool showHelpPopup = false;
    bool isAlwaysOnTop = false;
    bool wordWrapEnabled = false;
    bool lastCopyWasLine = false;
    std::string lastLineCopyText;

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
    float tabWidth = 32.0f;
    float gutterWidth = 40.0f;
    float maxLineWidth = 100.0f;
    size_t maxLineBytes = 0;
    int maxLineBytesIdx = 0;

    int vScrollPos = 0;
    int vScrollSubLine = 0;
    int hScrollPos = 0;
    float visibleVScrollWidth = 0.0f;
    float visibleHScrollHeight = 0.0f;
    bool isHoverVScroll = false;
    bool isHoverHScroll = false;
    bool isDraggingVScroll = false;
    bool isDraggingHScroll = false;
    float scrollDragStartMouse = 0.0f;
    int scrollDragStartPos = 0;

    std::vector<Cursor> cursors;
    std::vector<size_t> lineStarts;
    std::string imeComp;
    std::string newlineStr = "\n";

    // Rectangular selection & virtual padding state
    std::vector<PaddingRecord> pendingPadding;
    bool isRectSelecting = false;
    float rectAnchorX = 0.0f;
    float rectHeadX = 0.0f;
    int rectAnchorLine = 0;
    int rectHeadLine = 0;

    // Gutter line drag & Drag-and-drop selection move state
    bool isGutterDragging = false;
    int gutterAnchorLine = 0;
    bool isDraggingSelection = false;
    size_t dragDropTargetPos = 0;

    // Visible range UTF-8 -> UTF-16 offset map
    size_t utf16MapBasePos = 0;
    std::vector<uint32_t> utf16Cumulative;

    // Syntax highlighting & block comment state cache
    SyntaxDef currentSyntax;
    std::vector<int> commentStateCache;

    // Native Emmet session
    ActiveEmmetSession activeEmmetSession;

    // Live Preview (WKWebView) state
    bool showPreview = false;
    float previewSplitRatio = 0.5f;
    std::string previewNonce;
    bool previewInitialLoaded = false;

    // File history & external modification state
    std::vector<std::string> recentFiles;
    std::string customAppSupportDir;
    uint64_t lastKnownMtime = 0;
    uint64_t lastKnownFileSize = 0;

#if defined(__APPLE__)
    CGColorRef colBackground = NULL, colText = NULL, colGutterBg = NULL, colGutterText = NULL, colSel = NULL, colCaret = NULL;
    CGColorRef colNewlineMark = NULL, colBracketBg = NULL, colBracketBorder = NULL;
    CTFontRef fontRef = nullptr;
    CTParagraphStyleRef paragraphStyle = nullptr;
#endif

    std::wstring helpTextStr;
    std::wstring appVersionStr;
    std::string currentFileBuffer;

    // Callbacks
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
    std::function<void()> cbUpdatePreview;
    std::function<void(bool)> cbSetAlwaysOnTop;
    std::function<void()> cbUpdateRecentMenu;
    std::function<bool()> cbShowExternalModifyAlert;
    std::function<bool(const std::string& utf8Path, const std::string& rawBytes)> cbCoordinateWriteFile;

    // Core methods
    void detectNewlineStyle(const char* buf, size_t len);
    void rollbackPadding();
    void commitPadding(EditBatch& batch);
    void updateRectSelection();
    void moveRectSelection(int dLine, int dCol);
    void addCursor(bool up);
    void mergeCursors();
    void selectWordAt(size_t pos, bool addSelection = false);
    void selectLineAt(size_t pos, bool addSelection = false);
    void selectLinesRange(int anchorLine, int headLine, bool addSelection = false);
    bool isPointInSelection(size_t pos) const;
    void performDragMove(size_t targetPos);

    void insertAtCursorsWithPadding(const std::string& text);
    void insertNewlineWithAutoIndent();
    void insertRectangularBlock(const std::string& text);
    std::vector<int> getUniqueLineIndices();
    void deleteLine();
    void moveLines(bool up);
    void duplicateLines(bool up);
    void copyLines(bool up);
    void indentLines(bool forceLineIndent = false);
    void unindentLines();
    void toggleComment();

    // Emmet methods
    bool tryExpandEmmet();
    bool jumpEmmetTabstop(bool forward);
    void applyBatchToEmmetTabstops(const EditBatch& batch);

    // Bracket matching
    char getMatchingBracketChar(char c) const;
    bool isForwardBracket(char c) const;
    size_t findMatchingBracketFromIter(size_t pos, char openB, char closeB, bool forward) const;
    bool getAdjacentBrackets(size_t caret, size_t& b1, size_t& b2) const;
    void jumpToMatchingBracket(bool select = false);

    // Syntax highlighting
    void loadSyntaxForExtension(const std::string& pathOrExt);
    bool checkInBlockComment(int targetLine);

    // Layout, Soft Wrap & Scrollbars
    float getEffectiveTextWidth(float viewW) const;
    float measureLineWidth(int lineIdx);
    void updateGutterWidth();
    void updateMaxLineWidth();
    bool canFastAdjustLineStarts(const std::string& s) const;
    void adjustLineStartsSingleLine(size_t editPos, ptrdiff_t delta);
    ScrollBarLayout getScrollBarLayout(float clientW, float clientH);
    ScrollMetrics getVScrollMetrics(float clientW, float clientH);
    ScrollMetrics getHScrollMetrics(float clientW, float clientH);
    std::vector<VisualSubLine> getVisualSubLines(int lineIdx, float wrapWidth);
    int getLineVisualCount(int lineIdx, float wrapWidth);
    bool isVisualLineWrapBoundary(int lineIdx, size_t pos, float wrapWidth);
    void scrollVisualLines(int deltaVisualLines);
    void moveVisualUpDown(bool up, bool extendSelection);
    void moveVisualHome(bool extendSelection);
    void moveVisualEnd(bool extendSelection);
    void toggleWordWrap();
    void toggleAlwaysOnTop();

    // Live Preview
    void togglePreview();
    MiuPreview::PreviewRenderResult getPreviewRenderResult();
    bool handlePreviewTaskToggle(int targetLineIdx);

    // Search & Replace
    std::pair<std::string, bool> getHighlightTarget();
    std::string preprocessRegexQuery(const std::string& query);
    std::string UnescapeString(const std::string& s, const std::string& newline);
    size_t findText(size_t startPos, const std::string& query, bool forward, bool matchCase, bool wholeWord, bool isRegex, size_t* outLen = nullptr);
    void findNext(bool forward);
    void replaceNext();
    void replaceAll();
    void selectNextOccurrence();

    // UI & Document Management
    void showToastPopup(const std::string& text, int durationMs = 1200);
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
    void jumpToFileEdge(bool start, bool select);
    void convertSelectedText(bool toUpper);
    void copyToClipboard();
    void cutToClipboard();
    void pasteFromClipboard();
    void performUndo();
    void performRedo();
    bool checkUnsavedChanges();
    std::string getEncodedContentForSaving();
    bool saveFileElevated(const std::string& utf8Path, const std::string& rawBytes);
    bool saveFile(const std::wstring& p);
    bool saveFileAs();
    bool openFileFromPath(const std::string& p);
    bool openFile();
    void newFile();
    bool isWordChar(char c);
    void getWordBoundaries(size_t pos, size_t& start, size_t& end);
    void initGraphics();
    void updateThemeColors();

    // UTF-16 offset mapping
    void buildVisibleUtf16Map(size_t startPos, const std::string& text);
    size_t utf8OffsetToUtf16Count(size_t startPos, size_t byteLen);
    size_t countUTF16Length(size_t start, size_t byteLen);
    size_t getPositionByUTF16Offset(size_t startPos, int offset);

    // View state & Recent files & External modification
    std::string getAppSupportDir() const;
    void saveCurrentViewState();
    bool restoreViewState(const std::wstring& filePath);
    void loadRecentFiles();
    void saveRecentFiles();
    void addToRecentFiles(const std::string& path);
    void clearRecentFiles();
    void recordFileMtime(const std::string& path);
    bool checkFileModification();

#if defined(__APPLE__)
    void render(CGContextRef ctx, float w, float h);
#endif
    ~Editor();
};
