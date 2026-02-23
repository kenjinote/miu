#include "EditorCore.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>

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

std::string WToUTF8(const std::wstring& w) {
    CFStringRef str = CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8*)w.data(), w.size() * sizeof(wchar_t), kCFStringEncodingUTF32LE, false);
    std::string res = CFStringToStdString(str);
    if (str) CFRelease(str);
    return res;
}

std::wstring UTF8ToW(const std::string& s) {
    CFStringRef str = CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8*)s.data(), s.size(), kCFStringEncodingUTF8, false);
    std::wstring res = CFStringToStdWString(str);
    if (str) CFRelease(str);
    return res;
}

std::string Utf16ToUtf8(const char* data, size_t len, bool isBigEndian) {
    if (len < 2) return "";
    CFStringRef str = CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8*)(data + 2), len - 2, isBigEndian ? kCFStringEncodingUTF16BE : kCFStringEncodingUTF16LE, false);
    std::string res = CFStringToStdString(str);
    if (str) CFRelease(str);
    return res;
}

std::string AnsiToUtf8(const char* data, size_t len) {
    CFStringRef str = CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8*)data, len, kCFStringEncodingWindowsLatin1, false);
    std::string res = CFStringToStdString(str);
    if (str) CFRelease(str);
    return res;
}
#endif

const std::wstring APP_VERSION = L"miu v1.0.15";
const std::wstring APP_TITLE = L"miu";

bool MappedFile::open(const char* path) {
    fd = ::open(path, O_RDONLY); if (fd == -1) return false;
    struct stat sb; if (fstat(fd, &sb) == -1) { ::close(fd); return false; }
    size = sb.st_size; if (size == 0) { ptr = nullptr; return true; }
    ptr = (char*)mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0); return (ptr != MAP_FAILED);
}
void MappedFile::close() { if (ptr && ptr != MAP_FAILED) munmap(ptr, size); if (fd != -1) ::close(fd); ptr = nullptr; fd = -1; }

Encoding DetectEncoding(const char* buf, size_t len) {
    if (len >= 3 && (unsigned char)buf[0] == 0xEF && (unsigned char)buf[1] == 0xBB && (unsigned char)buf[2] == 0xBF) return ENC_UTF8_BOM;
    if (len >= 2) {
        if ((unsigned char)buf[0] == 0xFF && (unsigned char)buf[1] == 0xFE) return ENC_UTF16LE;
        if ((unsigned char)buf[0] == 0xFE && (unsigned char)buf[1] == 0xFF) return ENC_UTF16BE;
    }
    return ENC_UTF8_NOBOM;
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
        }
        else if (buf[i] == '\n') {
            newlineStr = "\n";
            return;
        }
    }
    newlineStr = "\n";
}

void Editor::insertAtCursorsWithPadding(const std::string& text) {
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

void Editor::insertNewlineWithAutoIndent() {
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

void Editor::insertRectangularBlock(const std::string& text) {
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

std::vector<int> Editor::getUniqueLineIndices() {
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

void Editor::deleteLine() {
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

void Editor::moveLines(bool up) {
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

void Editor::copyLines(bool up) {
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

void Editor::indentLines(bool forceLineIndent) {
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

void Editor::unindentLines() {
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

void Editor::updateGutterWidth() {
    int totalLines = (int)lineStarts.size(), digits = 1; int tempLines = totalLines;
    while (tempLines >= 10) { tempLines /= 10; digits++; }
    gutterWidth = (float)(digits * charWidth) + (charWidth * 1.5f);
}

void Editor::updateMaxLineWidth() {
#if defined(__APPLE__)
    if (!fontRef) return;
#endif
    maxLineWidth = 0.0f;
    for (int i = 0; i < (int)lineStarts.size(); ++i) {
        float w = getXInLine(i, (i + 1 < (int)lineStarts.size() ? lineStarts[i + 1] : pt.length()));
        if (w > maxLineWidth) maxLineWidth = w;
    }
    maxLineWidth += charWidth * 2.0f;
}

std::pair<std::string, bool> Editor::getHighlightTarget() {
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

std::string Editor::preprocessRegexQuery(const std::string& query) {
    std::string processed; processed.reserve(query.size() * 4);
    for (size_t i = 0; i < query.size(); ++i) {
        char c = query[i];
        if (c == '\\' && i + 1 < query.size()) {
            char next = query[i + 1];
            if (next == 'n') {
                bool isPrecededByCR = (i >= 2 && query[i - 2] == '\\' && query[i - 1] == 'r');
                if (!isPrecededByCR) { processed += "(?:\\r\\n|[\\r\\n])"; i++; continue; }
            }
            processed += c; processed += next; i++; continue;
        } else if (c == '^') {
            bool inClass = false; if (i > 0 && query[i - 1] == '[') inClass = true;
            if (!inClass) { processed += "(?:^|(?:\\r\\n|[\\r\\n]))"; continue; }
        }
        processed += c;
    }
    return processed;
}

std::string Editor::UnescapeString(const std::string& s, const std::string& newline) {
    std::string out; out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            switch (s[i + 1]) { case 'n': out += newline; break; case 'r': out += '\r'; break; case 't': out += '\t'; break; case '\\': out += '\\'; break; default: out += s[i]; out += s[i + 1]; break; } i++;
        } else out += s[i];
    } return out;
}

size_t Editor::findText(size_t startPos, const std::string& query, bool forward, bool matchCase, bool wholeWord, bool isRegex, size_t* outLen) {
    if (query.empty()) return std::string::npos;
    size_t len = pt.length(); std::string actualQuery = query;
    if (isRegex) {
        actualQuery = preprocessRegexQuery(query); std::string fullText = pt.getRange(0, len);
        try {
            std::regex_constants::syntax_option_type flags = std::regex_constants::ECMAScript;
            if (!matchCase) flags |= std::regex_constants::icase;
            std::regex re(actualQuery, flags); std::smatch m; size_t foundPos = std::string::npos; size_t foundLen = 0;
            bool startsWithCaret = (!query.empty() && query[0] == '^');
            if (forward) {
                if (startPos >= fullText.size()) startPos = 0;
                size_t searchStartIdx = startPos; std::regex_constants::match_flag_type searchFlags = std::regex_constants::match_default;
                if (searchStartIdx > 0) {
                    searchFlags |= std::regex_constants::match_not_bol; char prevChar = fullText[searchStartIdx - 1];
                    if (prevChar == '\n') { searchStartIdx--; if (searchStartIdx > 0 && fullText[searchStartIdx - 1] == '\r') searchStartIdx--; } else if (prevChar == '\r') searchStartIdx--;
                }
                std::string::const_iterator searchStartIter = fullText.begin() + searchStartIdx;
                if (std::regex_search(searchStartIter, fullText.cend(), m, re, searchFlags)) { foundPos = searchStartIdx + m.position(); foundLen = m.length(); }
                else if (startPos > 0) { if (std::regex_search(fullText.cbegin(), fullText.cend(), m, re)) { foundPos = m.position(); foundLen = m.length(); } }
            } else {
                auto words_begin = std::sregex_iterator(fullText.begin(), fullText.end(), re); auto words_end = std::sregex_iterator();
                size_t bestPos = std::string::npos; size_t limit = (startPos == 0) ? len : startPos;
                for (auto i = words_begin; i != words_end; ++i) { if (i->position() < (std::ptrdiff_t)limit) { bestPos = i->position(); foundLen = i->length(); } }
                if (bestPos != std::string::npos) foundPos = bestPos;
            }
            if (foundPos != std::string::npos) {
                if (foundPos > 0 && startsWithCaret) {
                    std::string matchStr = fullText.substr(foundPos, foundLen); size_t adj = 0;
                    if (matchStr.size() >= 2 && matchStr[0] == '\r' && matchStr[1] == '\n') adj = 2; else if (matchStr.size() >= 1 && (matchStr[0] == '\n' || matchStr[0] == '\r')) adj = 1;
                    foundPos += adj; foundLen -= adj;
                }
                if (outLen) *outLen = foundLen; return foundPos;
            }
        } catch (...) { return std::string::npos; }
        return std::string::npos;
    }
    size_t qLen = query.length(); if (outLen) *outLen = qLen;
    auto toLower = [](char c) { return (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c; };
    size_t cur = startPos; if (forward) { if (cur >= len) cur = 0; } else { if (cur == 0) cur = len; else cur--; }
    for (size_t count = 0; count < len; ++count) {
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
        if (forward) { cur++; if (cur >= len) cur = 0; } else { if (cur == 0) cur = len - 1; else cur--; }
    }
    return std::string::npos;
}

void Editor::findNext(bool forward) {
    if (searchQuery.empty()) return;
    size_t startPos = forward ? (cursors.empty() ? 0 : cursors.back().end()) : (cursors.empty() ? 0 : cursors.back().start());
    size_t matchLen = 0; size_t pos = findText(startPos, searchQuery, forward, searchMatchCase, searchWholeWord, searchRegex, &matchLen);
    if (pos != std::string::npos) {
        cursors.clear(); cursors.push_back({ pos + matchLen, pos, getXFromPos(pos + matchLen), getXFromPos(pos), false });
        ensureCaretVisible();
        if (cbNeedsDisplay) cbNeedsDisplay();
    } else {
        if (cbBeep) cbBeep();
    }
}

void Editor::replaceNext() {
    if (cursors.empty() || searchQuery.empty()) return;
    Cursor& c = cursors.back(); if (!c.hasSelection()) { findNext(true); return; }
    size_t len = c.end() - c.start(); size_t start = c.start();
    std::string selText = pt.getRange(start, len); bool match = false; std::string replacement = replaceQuery;
    if (searchRegex) {
        try {
            std::string actualQuery = preprocessRegexQuery(searchQuery); std::regex_constants::syntax_option_type flags = std::regex_constants::ECMAScript;
            if (!searchMatchCase) flags |= std::regex_constants::icase; std::regex re(actualQuery, flags); std::smatch m;
            if (std::regex_match(selText, m, re)) match = true;
            else if (start > 0) {
                int backStep = 0; if (pt.charAt(start - 1) == '\n') { backStep = 1; if (start > 1 && pt.charAt(start - 2) == '\r') backStep = 2; } else if (pt.charAt(start - 1) == '\r') backStep = 1;
                if (backStep > 0) { std::string extText = pt.getRange(start - backStep, len + backStep); if (std::regex_match(extText, m, re)) match = true; }
            }
            if (match) replacement = m.format(UnescapeString(replaceQuery, newlineStr));
        } catch (...) {}
    } else {
        if (len == searchQuery.length()) {
            match = true;
            for (size_t i = 0; i < len; ++i) {
                char c1 = selText[i]; char c2 = searchQuery[i];
                if (!searchMatchCase) { c1 = (c1 >= 'A' && c1 <= 'Z') ? c1 + ('a' - 'A') : c1; c2 = (c2 >= 'A' && c2 <= 'Z') ? c2 + ('a' - 'A') : c2; }
                if (c1 != c2) { match = false; break; }
            }
        }
        if (match) replacement = replaceQuery;
    }
    if (match) {
        EditBatch batch; batch.beforeCursors = cursors; pt.erase(start, len); batch.ops.push_back({ EditOp::Erase, start, selText });
        pt.insert(start, replacement); batch.ops.push_back({ EditOp::Insert, start, replacement });
        cursors.clear(); size_t newEnd = start + replacement.size(); cursors.push_back({ newEnd, start, getXFromPos(newEnd), getXFromPos(start), false });
        batch.afterCursors = cursors; undo.push(batch); rebuildLineStarts(); ensureCaretVisible(); updateDirtyFlag();
        if (cbNeedsDisplay) cbNeedsDisplay();
    } else findNext(true);
}

void Editor::replaceAll() {
    if (searchQuery.empty()) return;
    struct Match { size_t start; size_t len; std::string replacementText; }; std::vector<Match> matches;
    size_t currentPos = 0, docLen = pt.length(); std::string actualQuery = searchQuery;
    if (searchRegex) {
        actualQuery = preprocessRegexQuery(searchQuery); std::string fullText = pt.getRange(0, docLen); std::string fmt = UnescapeString(replaceQuery, newlineStr);
        try {
            std::regex_constants::syntax_option_type flags = std::regex_constants::ECMAScript; if (!searchMatchCase) flags |= std::regex_constants::icase; std::regex re(actualQuery, flags);
            bool startsWithCaret = (!searchQuery.empty() && searchQuery[0] == '^');
            auto begin = std::sregex_iterator(fullText.begin(), fullText.end(), re); auto end = std::sregex_iterator();
            for (auto i = begin; i != end; ++i) {
                size_t pos = i->position(), len = i->length(); std::string rText = i->format(fmt);
                if (startsWithCaret) {
                    std::string matchStr = i->str(); size_t adj = 0;
                    if (matchStr.size() >= 2 && matchStr[0] == '\r' && matchStr[1] == '\n') adj = 2; else if (matchStr.size() >= 1 && (matchStr[0] == '\n' || matchStr[0] == '\r')) adj = 1;
                    if (adj > 0) { pos += adj; len -= adj; }
                }
                matches.push_back({ pos, len, rText });
            }
        } catch (...) { return; }
    } else {
        while (true) {
            size_t matchLen = 0; size_t pos = findText(currentPos, searchQuery, true, searchMatchCase, searchWholeWord, false, &matchLen);
            if (pos == std::string::npos || pos < currentPos) break;
            matches.push_back({ pos, matchLen, replaceQuery }); currentPos = pos + matchLen; if (currentPos > docLen) break;
        }
    }
    if (matches.empty()) { if (cbBeep) cbBeep(); return; }
    EditBatch batch; batch.beforeCursors = cursors;
    for (auto it = matches.rbegin(); it != matches.rend(); ++it) {
        size_t start = it->start, len = it->len; std::string deleted = pt.getRange(start, len);
        pt.erase(start, len); batch.ops.push_back({ EditOp::Erase, start, deleted });
        pt.insert(start, it->replacementText); batch.ops.push_back({ EditOp::Insert, start, it->replacementText });
    }
    size_t finalMatchIdx = matches.size() - 1; long long offsetBeforeFinal = 0;
    for (size_t i = 0; i < finalMatchIdx; ++i) offsetBeforeFinal += (long long)matches[i].replacementText.size() - (long long)matches[i].len;
    size_t lastReplaceStart = (size_t)((long long)matches.back().start + offsetBeforeFinal); size_t lastReplaceEnd = lastReplaceStart + matches.back().replacementText.size();
    cursors.clear(); cursors.push_back({ lastReplaceEnd, lastReplaceStart, getXFromPos(lastReplaceEnd), getXFromPos(lastReplaceStart), false });
    batch.afterCursors = cursors; undo.push(batch); rebuildLineStarts(); ensureCaretVisible(); updateDirtyFlag();
    if (cbNeedsDisplay) cbNeedsDisplay();
    
    if (cbShowReplaceAlert) cbShowReplaceAlert((int)matches.size());
}

void Editor::selectNextOccurrence() {
    if (cursors.empty()) return;
    Cursor c = cursors.back();
    if (!c.hasSelection()) {
        size_t targetPos = c.head;
        if (targetPos > 0) {
            char currChar = targetPos < pt.length() ? pt.charAt(targetPos) : '\0';
            char prevChar = pt.charAt(targetPos - 1);
            if (!isWordChar(currChar) && isWordChar(prevChar)) { targetPos--; }
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
    size_t nextPos = findText(std::max(c.head, c.anchor), query, true, true, false, false);
    if (nextPos != std::string::npos) {
        for (const auto& cur : cursors) { if (cur.start() == nextPos) return; }
        size_t newHead = nextPos + len;
        cursors.push_back({newHead, nextPos, getXFromPos(newHead), getXFromPos(nextPos), false});
        ensureCaretVisible();
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
    if (isDirty != nd) { isDirty = nd; updateTitleBar(); }
}

void Editor::updateFont(float s) {
    float oldCharWidth = charWidth; s = std::clamp(s, 6.0f, 200.0f);
#if defined(__APPLE__)
    if (fontRef) CFRelease(fontRef); fontRef = CTFontCreateWithName(CFSTR("Menlo"), s, NULL);
    currentFontSize = s; lineHeight = std::ceil(s * 1.4f);
    UniChar c = '0'; CGGlyph g; CGSize adv; CTFontGetGlyphsForCharacters(fontRef, &c, &g, 1); CTFontGetAdvancesForGlyphs(fontRef, kCTFontOrientationHorizontal, &g, &adv, 1);
    charWidth = adv.width;
#endif
    if (oldCharWidth > 0.0f && charWidth > 0.0f) { float ratio = charWidth / oldCharWidth; for (auto& cur : cursors) { cur.desiredX *= ratio; cur.originalAnchorX *= ratio; } }
    updateGutterWidth(); updateMaxLineWidth(); updateScrollBars();
}

void Editor::rebuildLineStarts() {
    lineStarts.clear(); lineStarts.push_back(0); size_t go = 0;
    for (const auto& p : pt.pieces) {
        const char* b = p.isOriginal ? (pt.origPtr + p.start) : (pt.addBuf.data() + p.start);
        for (size_t i = 0; i < p.len; ++i) if (b[i] == '\n') lineStarts.push_back(go + i + 1);
        go += p.len;
    }
    updateGutterWidth(); updateMaxLineWidth(); updateScrollBars();
}

int Editor::getLineIdx(size_t pos) {
    auto it = std::upper_bound(lineStarts.begin(), lineStarts.end(), pos);
    return std::max(0, (int)std::distance(lineStarts.begin(), it) - 1);
}

float Editor::getXInLine(int li, size_t pos) {
    if (li < 0 || li >= (int)lineStarts.size()) return 0.0f;
    size_t s = lineStarts[li], e = (li + 1 < (int)lineStarts.size()) ? lineStarts[li + 1] : pt.length();
    if (e > s && pt.charAt(e-1) == '\n') e--;
    if (e > s && pt.charAt(e-1) == '\r') e--;
    std::string lstr = pt.getRange(s, e - s); size_t rp = std::clamp(pos, s, e) - s;
    if (!imeComp.empty() && !cursors.empty() && getLineIdx(cursors.back().head) == li) { size_t cp = cursors.back().head; if (cp >= s && cp <= e) { lstr.insert(cp - s, imeComp); if (pos >= cp) rp += imeComp.size(); } }
    if (lstr.empty()) return 0.0f;
    
#if defined(__APPLE__)
    CFStringRef cf = CFStringCreateWithBytes(NULL, (const UInt8*)lstr.data(), lstr.size(), kCFStringEncodingUTF8, false); if (!cf) return 0.0f;
    const void* keys[] = { kCTFontAttributeName };
    const void* values[] = { fontRef };
    CFDictionaryRef d = CFDictionaryCreate(NULL, keys, values, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFAttributedStringRef as = CFAttributedStringCreate(NULL, cf, d); CTLineRef line = CTLineCreateWithAttributedString(as);
    CFStringRef sub = CFStringCreateWithBytes(NULL, (const UInt8*)lstr.data(), rp, kCFStringEncodingUTF8, false);
    CGFloat x = CTLineGetOffsetForStringIndex(line, sub ? CFStringGetLength(sub) : 0, NULL);
    if (sub) CFRelease(sub); CFRelease(line); CFRelease(as); CFRelease(d); CFRelease(cf); return (float)x;
#else
    return 0.0f;
#endif
}

float Editor::getXFromPos(size_t p) { return getXInLine(getLineIdx(p), p); }

size_t Editor::getPosFromLineAndX(int li, float tx) {
    if (li < 0 || li >= (int)lineStarts.size()) return cursors.empty() ? 0 : cursors.back().head;
    size_t s = lineStarts[li], e = (li + 1 < (int)lineStarts.size()) ? lineStarts[li + 1] : pt.length();
    if (e > s && pt.charAt(e-1) == '\n') e--;
    if (e > s && pt.charAt(e-1) == '\r') e--;
    std::string lstr = pt.getRange(s, e - s); if (lstr.empty()) return s;
    
#if defined(__APPLE__)
    CFStringRef cf = CFStringCreateWithBytes(NULL, (const UInt8*)lstr.data(), lstr.size(), kCFStringEncodingUTF8, false); if (!cf) return s;
    const void* keys[] = { kCTFontAttributeName };
    const void* values[] = { fontRef };
    CFDictionaryRef d = CFDictionaryCreate(NULL, keys, values, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFAttributedStringRef as = CFAttributedStringCreate(NULL, cf, d); CTLineRef line = CTLineCreateWithAttributedString(as);
    CFIndex ci = CTLineGetStringIndexForPosition(line, CGPointMake(tx, 0));
    CFIndex bytes = 0; if (ci > 0) {
        CFStringRef sub = CFStringCreateWithSubstring(NULL, cf, CFRangeMake(0, std::min(ci, CFStringGetLength(cf))));
        if (sub) { CFStringGetBytes(sub, CFRangeMake(0, CFStringGetLength(sub)), kCFStringEncodingUTF8, 0, false, NULL, 0, &bytes); CFRelease(sub); }
    }
    CFRelease(line); CFRelease(as); CFRelease(d); CFRelease(cf); return s + bytes;
#else
    return s;
#endif
}

size_t Editor::getDocPosFromPoint(float x, float y) {
    float absY = y + (float)vScrollPos * lineHeight;
    int li = std::clamp((int)std::floor(absY / lineHeight), 0, (int)lineStarts.size() - 1);
    return getPosFromLineAndX(li, x - gutterWidth + (float)hScrollPos);
}

void Editor::ensureCaretVisible() {
    if (cursors.empty() || !cbGetViewSize) return;
    Cursor& c = cursors.back();
    float viewWidth = 0.0f, viewHeight = 0.0f;
    cbGetViewSize(viewWidth, viewHeight);
    
    float ch = viewHeight - visibleHScrollHeight, cw = viewWidth - gutterWidth - visibleVScrollWidth;
    int li = getLineIdx(c.head), vis = (int)(ch/lineHeight);
    if (li < vScrollPos) vScrollPos = li; else if (li >= vScrollPos + vis) vScrollPos = li - vis + 1;
    float cx = getXFromPos(c.head), m = charWidth*2; if (cx < (float)hScrollPos + m) hScrollPos = (int)(cx - m); else if (cx > (float)hScrollPos + cw - m) hScrollPos = (int)(cx - cw + m);
    vScrollPos = std::max(0, vScrollPos); hScrollPos = std::max(0, hScrollPos); updateScrollBars();
}

size_t Editor::moveCaretVisual(size_t pos, bool f) {
    size_t len = pt.length();
    if (f) { if (pos >= len) return len; unsigned char c = pt.charAt(pos); int sl = 1; if ((c&0x80)==0) sl=1; else if((c&0xE0)==0xC0) sl=2; else if((c&0xF0)==0xE0) sl=3; else if((c&0xF8)==0xF0) sl=4; if(c=='\r'&&pos+1<len&&pt.charAt(pos+1)=='\n') sl=2; return std::min(len, pos+sl); }
    else { if (pos == 0) return 0; size_t p = pos-1; while(p>0 && (pt.charAt(p)&0xC0)==0x80) p--; if(p>0 && pt.charAt(p-1)=='\r'&&pt.charAt(p)=='\n') p--; return p; }
}

void Editor::insertAtCursors(const std::string& t) {
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

void Editor::backspaceAtCursors() {
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
    if (changed) { b.afterCursors = cursors; undo.push(b); rebuildLineStarts(); updateDirtyFlag(); } ensureCaretVisible();
    if (cbNeedsDisplay) cbNeedsDisplay();
}

void Editor::deleteForwardAtCursors() {
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

void Editor::selectAll() { cursors.clear(); size_t len = pt.length(); cursors.push_back({len, 0, getXFromPos(len), getXFromPos(len), false}); }

void Editor::convertSelectedText(bool toUpper) {
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
        if (cbNeedsDisplay) cbNeedsDisplay();
    }
}

void Editor::copyToClipboard() {
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
    if(!t.empty() && cbSetClipboard) {
        cbSetClipboard(t, hasSelection && isRect);
    }
}

void Editor::cutToClipboard() {
    bool hasSelection = false; for (const auto& c : cursors) if (c.hasSelection()) { hasSelection = true; break; }
    copyToClipboard();
    if (!hasSelection) deleteLine(); else insertAtCursors("");
}

void Editor::pasteFromClipboard() {
    if (!cbGetClipboard) return;
    bool isRectMarker = false;
    std::string utf8 = cbGetClipboard(isRectMarker);
    if(utf8.empty()) return;
    
    if (isRectMarker) {
        if (cursors.size() <= 1) {
            size_t basePos = cursors.empty() ? 0 : cursors[0].head; float baseX = getXFromPos(basePos); int startLine = getLineIdx(basePos);
            int lineCount = 1; for(char c : utf8) if(c == '\n') lineCount++;
            cursors.clear(); for(int i=0; i < lineCount; ++i) { int targetLine = startLine + i; if(targetLine < (int)lineStarts.size()) { size_t p = getPosFromLineAndX(targetLine, baseX); cursors.push_back({p, p, baseX, baseX, false}); } }
        }
        insertRectangularBlock(utf8);
    } else { insertAtCursors(utf8); }
    if (cbNeedsDisplay) cbNeedsDisplay();
}

void Editor::performUndo() { if(!undo.undoStack.empty()){ auto b = undo.popUndo(); for(int i=(int)b.ops.size()-1;i>=0;--i){ if(b.ops[i].type==EditOp::Insert) pt.erase(b.ops[i].pos, (int)b.ops[i].text.size()); else pt.insert(b.ops[i].pos, b.ops[i].text); } cursors=b.beforeCursors; rebuildLineStarts(); ensureCaretVisible(); updateDirtyFlag(); } }
void Editor::performRedo() { if(!undo.redoStack.empty()){ auto b = undo.popRedo(); for(const auto& o:b.ops){ if(o.type==EditOp::Insert) pt.insert(o.pos, o.text); else pt.erase(o.pos, (int)o.text.size()); } cursors=b.afterCursors; rebuildLineStarts(); ensureCaretVisible(); updateDirtyFlag(); } }

bool Editor::checkUnsavedChanges() {
    if(!isDirty) return true;
    if(cbShowUnsavedAlert) return cbShowUnsavedAlert();
    return true;
}

bool Editor::saveFile(const std::wstring& p) {
    std::string s = pt.getRange(0,pt.length()); std::ofstream f(WToUTF8(p), std::ios::binary); if(!f) return false; f.write(s.data(), s.size()); f.close(); currentFilePath=p; undo.markSaved(); isDirty=false; updateTitleBar(); return true;
}

bool Editor::saveFileAs() {
    if (cbSaveFileAs) return cbSaveFileAs();
    return false;
}

bool Editor::openFileFromPath(const std::string& p) {
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
        cursors.clear(); cursors.push_back({0,0,0.0f,0.0f,false}); rebuildLineStarts(); updateTitleBar();
        updateScrollBars();
        if (cbNeedsDisplay) cbNeedsDisplay();
        return true;
    } return false;
}

bool Editor::openFile() {
    if(!checkUnsavedChanges()) return false;
    if (cbOpenFile) return cbOpenFile();
    return false;
}

void Editor::newFile() {
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

bool Editor::isWordChar(char c) { if (isalnum((unsigned char)c) || c == '_') return true; if ((unsigned char)c >= 0x80) return true; return false; }
void Editor::getWordBoundaries(size_t pos, size_t& start, size_t& end) {
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

void Editor::initGraphics() {
#if defined(__APPLE__)
    if (!fontRef) fontRef = CTFontCreateWithName(CFSTR("Menlo"), currentFontSize, NULL);
#endif
    rebuildLineStarts(); updateThemeColors();
    if (cursors.empty()) cursors.push_back({0, 0, 0.0f, 0.0f, false});
    updateTitleBar();
}

void Editor::updateThemeColors() {
#if defined(__APPLE__)
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
#endif
}

#if defined(__APPLE__)
void Editor::render(CGContextRef ctx, float w, float h) {
    CGContextSetFillColorWithColor(ctx, colBackground); CGContextFillRect(ctx, CGRectMake(0, 0, w, h));
    
    // 【最適化】テキスト全体のY軸を反転させる設定を1度だけ適用（描画を高速化）
    CGContextSetTextMatrix(ctx, CGAffineTransformMakeScale(1.0, -1.0));
    
    float vw = std::max(0.0f, w - gutterWidth - visibleVScrollWidth);
    float vh = std::max(0.0f, h - visibleHScrollHeight);
    int start = vScrollPos; int vis = (int)(vh / lineHeight) + 2; int end = std::min((int)lineStarts.size(), start + vis);
    CGFloat asc = CTFontGetAscent(fontRef);
    
    CGContextSaveGState(ctx); CGContextClipToRect(ctx, CGRectMake(gutterWidth, 0, vw, vh));
    
    auto [autoStr, isWholeWord] = getHighlightTarget();
    if (!autoStr.empty() && autoStr != searchQuery) {
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
    
    if (!searchQuery.empty()) {
        CGColorRef hlColor = isDarkMode ? CGColorCreateGenericRGB(0.4, 0.4, 0.0, 0.6) : CGColorCreateGenericRGB(1.0, 1.0, 0.0, 0.4);
        CGContextSetFillColorWithColor(ctx, hlColor);
        
        size_t searchRangeStart = lineStarts[start];
        size_t searchRangeEnd = (end < lineStarts.size()) ? lineStarts[end] : pt.length();
        std::string visibleText = pt.getRange(searchRangeStart, searchRangeEnd - searchRangeStart);
        
        if (searchRegex) {
            try {
                std::string actualQuery = preprocessRegexQuery(searchQuery);
                std::regex_constants::syntax_option_type flags = std::regex_constants::ECMAScript;
                if (!searchMatchCase) flags |= std::regex_constants::icase;
                std::regex re(actualQuery, flags);
                auto words_begin = std::sregex_iterator(visibleText.begin(), visibleText.end(), re);
                auto words_end = std::sregex_iterator();
                for (auto i = words_begin; i != words_end; ++i) {
                    size_t offset = i->position(); size_t len = i->length();
                    size_t docPos = searchRangeStart + offset;
                    int li = getLineIdx(docPos);
                    if (li >= start && li < end) {
                        float y = (float)(li - start) * lineHeight;
                        float x1 = getXInLine(li, docPos); float x2 = getXInLine(li, docPos + len);
                        CGContextFillRect(ctx, CGRectMake(gutterWidth - (float)hScrollPos + x1, y, x2 - x1, lineHeight));
                    }
                }
            } catch (...) {}
        } else {
            std::string q = searchQuery; std::string t = visibleText;
            if (!searchMatchCase) { std::transform(q.begin(), q.end(), q.begin(), ::tolower); std::transform(t.begin(), t.end(), t.begin(), ::tolower); }
            size_t offset = 0;
            while ((offset = t.find(q, offset)) != std::string::npos) {
                bool match = true;
                if (searchWholeWord) {
                    if (offset > 0 && isWordChar(visibleText[offset - 1])) match = false;
                    if (match && (offset + q.length() < visibleText.length()) && isWordChar(visibleText[offset + q.length()])) match = false;
                }
                if (match) {
                    size_t docPos = searchRangeStart + offset;
                    int li = getLineIdx(docPos);
                    if (li >= start && li < end) {
                        float y = (float)(li - start) * lineHeight;
                        float x1 = getXInLine(li, docPos); float x2 = getXInLine(li, docPos + q.length());
                        CGContextFillRect(ctx, CGRectMake(gutterWidth - (float)hScrollPos + x1, y, x2 - x1, lineHeight));
                    }
                }
                offset += 1;
            }
        }
        CGColorRelease(hlColor);
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
                
                // 【最適化】ステート保存なしのダイレクト描画
                CGContextSetTextPosition(ctx, gutterWidth - (float)hScrollPos, (float)(i - start) * lineHeight + asc + 2.0f);
                CTLineDraw(tl, ctx);
                
                CFRelease(tl); CFRelease(mas); CFRelease(cf);
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
    
    // ガター（行番号）の描画
    for (int i = start; i < end; ++i) {
        CFStringRef n = CFStringCreateWithCString(NULL, std::to_string(i + 1).c_str(), kCFStringEncodingUTF8);
        if (n) {
            const void* keys[] = { kCTFontAttributeName, kCTForegroundColorAttributeName };
            const void* values[] = { fontRef, colGutterText };
            CFDictionaryRef attr = CFDictionaryCreate(NULL, keys, values, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
            CFAttributedStringRef nas = CFAttributedStringCreate(NULL, n, attr);
            CTLineRef nl = CTLineCreateWithAttributedString(nas);
            double ascent, descent, leading;
            double lineWidth = CTLineGetTypographicBounds(nl, &ascent, &descent, &leading);
            float xPos = gutterWidth - (float)lineWidth - (charWidth * 0.5f); if (xPos < 5.0f) xPos = 5.0f;
            
            // 【最適化】ステート保存なしのダイレクト描画
            CGContextSetTextPosition(ctx, xPos, (float)(i - start) * lineHeight + asc + 2.0f);
            CTLineDraw(nl, ctx);
            
            CFRelease(nl); CFRelease(nas); CFRelease(attr); CFRelease(n);
        }
    }
    
    // ズームポップアップの描画
    auto now = std::chrono::steady_clock::now();
    if (now < zoomPopupEndTime) {
        float zw = 160.0f, zh = 80.0f;
        CGRect zpr = CGRectMake((w - zw) / 2.0f, (h - zh) / 2.0f, zw, zh);
        CGContextSetFillColorWithColor(ctx, CGColorCreateGenericRGB(0.2, 0.2, 0.2, 0.7));
        CGPathRef path = CGPathCreateWithRoundedRect(zpr, 10.0f, 10.0f, NULL);
        CGContextAddPath(ctx, path); CGContextFillPath(ctx); CGPathRelease(path);
        
        CFStringRef zcf = CFStringCreateWithCString(NULL, zoomPopupText.c_str(), kCFStringEncodingUTF8);
        CTFontRef zf = CTFontCreateWithName(CFSTR("Helvetica-Bold"), 24.0f, NULL);
        CGColorRef white = CGColorCreateGenericRGB(1, 1, 1, 1);
        
        const void* zKeys[] = { kCTFontAttributeName, kCTForegroundColorAttributeName };
        const void* zVals[] = { zf, white };
        CFDictionaryRef za = CFDictionaryCreate(NULL, zKeys, zVals, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFAttributedStringRef zas = CFAttributedStringCreate(NULL, zcf, za);
        CTLineRef zl = CTLineCreateWithAttributedString(zas);
        
        double lineWidth = CTLineGetTypographicBounds(zl, NULL, NULL, NULL);
        CGFloat zAsc = CTFontGetAscent(zf);
        CGFloat zDesc = CTFontGetDescent(zf);
        float textY = zpr.origin.y + (zh - (zAsc + zDesc)) / 2.0f + zAsc;
        
        CGContextSetTextPosition(ctx, zpr.origin.x + (zw - (float)lineWidth) / 2.0f, textY);
        CTLineDraw(zl, ctx);
        
        CFRelease(zl); CFRelease(zas); CFRelease(za); CFRelease(zf); CFRelease(zcf); CGColorRelease(white);
        
        if (cbNeedsDisplay) {
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
                cbNeedsDisplay();
            });
        }
    }
    
    // ヘルプポップアップの描画
    if (showHelpPopup) {
        std::wstring fullHelp = APP_VERSION + L"\n\n" + helpTextStr;
        CFStringRef hcf = CFStringCreateWithBytes(NULL, (const UInt8*)fullHelp.data(), fullHelp.size()*sizeof(wchar_t), kCFStringEncodingUTF32LE, false);
        CTFontRef hf = CTFontCreateWithName(CFSTR("Menlo"), 14.0f, NULL);
        CGFloat helpLineHeight = 15.0f;
        CTParagraphStyleSetting settings[] = {
            { kCTParagraphStyleSpecifierMinimumLineHeight, sizeof(CGFloat), &helpLineHeight },
            { kCTParagraphStyleSpecifierMaximumLineHeight, sizeof(CGFloat), &helpLineHeight }
        };
        CTParagraphStyleRef ps = CTParagraphStyleCreate(settings, 2);
        CGColorRef helpWhite = CGColorCreateGenericRGB(1.0, 1.0, 1.0, 1.0);
        
        const void* hKeys[] = { kCTFontAttributeName, kCTForegroundColorAttributeName, kCTParagraphStyleAttributeName };
        const void* hVals[] = { hf, helpWhite, ps };
        CFDictionaryRef ha = CFDictionaryCreate(NULL, hKeys, hVals, 3, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFAttributedStringRef has = CFAttributedStringCreate(NULL, hcf, ha);
        
        CTFramesetterRef fs = CTFramesetterCreateWithAttributedString(has);
        CGSize maxConstraints = CGSizeMake(w * 0.9f, h * 0.9f);
        CGSize suggestedSize = CTFramesetterSuggestFrameSizeWithConstraints(fs, CFRangeMake(0, 0), NULL, maxConstraints, NULL);
        float padding = 20.0f;
        float hw = suggestedSize.width + (padding * 2.0f);
        float hh = suggestedSize.height + (padding * 2.0f);
        CGRect hr = CGRectMake((w - hw) / 2.0f, (h - hh) / 2.0f, hw, hh);
        
        CGContextSetFillColorWithColor(ctx, CGColorCreateGenericRGB(0.2, 0.2, 0.2, 0.7));
        CGPathRef hpath = CGPathCreateWithRoundedRect(hr, 10.0f, 10.0f, NULL);
        CGContextAddPath(ctx, hpath); CGContextFillPath(ctx); CGPathRelease(hpath);
        
        CGContextSaveGState(ctx);
        CGContextSetTextMatrix(ctx, CGAffineTransformIdentity);
        CGContextTranslateCTM(ctx, hr.origin.x, hr.origin.y + hr.size.height);
        CGContextScaleCTM(ctx, 1.0f, -1.0f);
        
        CGMutablePathRef pth = CGPathCreateMutable();
        CGPathAddRect(pth, NULL, CGRectMake(padding, padding, suggestedSize.width, suggestedSize.height));
        CTFrameRef frame = CTFramesetterCreateFrame(fs, CFRangeMake(0, 0), pth, NULL);
        CTFrameDraw(frame, ctx);
        
        CGContextRestoreGState(ctx);
        CFRelease(frame); CFRelease(pth); CFRelease(fs); CFRelease(has); CFRelease(ha); CFRelease(hf); CFRelease(hcf); CFRelease(ps); CGColorRelease(helpWhite);
    }
}
#endif

Editor::~Editor() {
#if defined(__APPLE__)
    if (colBackground) CGColorRelease(colBackground);
    if (colText) CGColorRelease(colText);
    if (colGutterBg) CGColorRelease(colGutterBg);
    if (colGutterText) CGColorRelease(colGutterText);
    if (colSel) CGColorRelease(colSel);
    if (colCaret) CGColorRelease(colCaret);
    if (fontRef) CFRelease(fontRef);
#endif
}
