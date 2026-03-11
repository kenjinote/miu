#include <android_native_app_glue.h>
#include <android/log.h>
#include <android/configuration.h>
#include <jni.h>
#include <vector>
#include <string>
#include <algorithm>
#include <memory>
#include <unordered_map>
#include <fstream>
#include <chrono>
#include <cmath>
#include <ft2build.h>
#include FT_FREETYPE_H
#define VK_USE_PLATFORM_ANDROID_KHR
#include <vulkan/vulkan.h>
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "miu", __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, "miu", __VA_ARGS__))
struct GlyphInfo {
    float u0, v0, u1, v1;
    float width, height;
    float bearingX, bearingY;
    float advance;
    bool isColor;
};
struct TextAtlas {
    int width = 2048;
    int height = 1024;
    std::vector<uint8_t> pixels;
    std::unordered_map<uint32_t, GlyphInfo> glyphs;
    int currentX = 0;
    int currentY = 0;
    int maxRowHeight = 0;
    bool isDirty = false;
    int dirtyMinX = 0, dirtyMinY = 0, dirtyMaxX = 0, dirtyMaxY = 0;
    void init() {
        pixels.resize(width * height * 4, 0);
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 2; ++x) {
                int idx = (y * width + x) * 4;
                pixels[idx + 0] = 255; pixels[idx + 1] = 255; pixels[idx + 2] = 255; pixels[idx + 3] = 255;
            }
        }
        currentX = 3;
        currentY = 0;
        maxRowHeight = 2;
    }
    bool loadChar(FT_Face mainFace, FT_Face emojiFace, uint32_t charCode) {
        if (glyphs.count(charCode) > 0) return true;
        FT_Face targetFace = mainFace;
        FT_UInt glyphIndex = FT_Get_Char_Index(mainFace, charCode);
        if (glyphIndex == 0 && emojiFace != nullptr) {
            FT_UInt emojiIndex = FT_Get_Char_Index(emojiFace, charCode);
            if (emojiIndex != 0) {
                targetFace = emojiFace;
                glyphIndex = emojiIndex;
            }
        }
        if (glyphIndex == 0 && charCode != 0) {
            FT_Load_Glyph(mainFace, 0, FT_LOAD_RENDER | FT_LOAD_COLOR);
            targetFace = mainFace;
        } else {
            if (FT_Load_Glyph(targetFace, glyphIndex, FT_LOAD_RENDER | FT_LOAD_COLOR) != 0) {
                FT_Load_Glyph(mainFace, 0, FT_LOAD_RENDER | FT_LOAD_COLOR);
                targetFace = mainFace;
            }
        }
        FT_Bitmap* bmp = &targetFace->glyph->bitmap;
        if (charCode != 0x20 && charCode != 0xA0 && (bmp->width == 0 || bmp->rows == 0) && targetFace->glyph->advance.x != 0 && charCode != 0 && glyphIndex != 0) {
            LOGE("グリフ展開失敗: cp:%x", charCode);
            if (FT_Load_Glyph(mainFace, 0, FT_LOAD_RENDER | FT_LOAD_COLOR) == 0) {
                targetFace = mainFace;
                bmp = &targetFace->glyph->bitmap;
            } else {
                return false;
            }
        }
        bool isColorGlyph = (bmp->pixel_mode == FT_PIXEL_MODE_BGRA);
        if (currentX + bmp->width + 1 >= width) {
            currentX = 0; currentY += maxRowHeight + 1; maxRowHeight = 0;
        }
        if (currentY + bmp->rows + 1 >= height) { LOGE("アトラスオーバー"); return false; }
        for (unsigned int row = 0; row < bmp->rows; ++row) {
            for (unsigned int col = 0; col < bmp->width; ++col) {
                size_t dstIdx = ((currentY + row) * width + (currentX + col)) * 4;
                if (isColorGlyph) {
                    uint8_t* bgr = &bmp->buffer[row * bmp->pitch + col * 4];
                    pixels[dstIdx + 0] = bgr[2];
                    pixels[dstIdx + 1] = bgr[1];
                    pixels[dstIdx + 2] = bgr[0];
                    pixels[dstIdx + 3] = bgr[3];
                } else {
                    uint8_t gray = bmp->buffer[row * bmp->pitch + col];
                    pixels[dstIdx + 0] = gray; pixels[dstIdx + 1] = gray; pixels[dstIdx + 2] = gray; pixels[dstIdx + 3] = gray;
                }
            }
        }
        if (!isDirty) {
            dirtyMinX = currentX; dirtyMaxX = currentX + bmp->width;
            dirtyMinY = currentY; dirtyMaxY = currentY + bmp->rows;
            isDirty = true;
        } else {
            if (currentX < dirtyMinX) dirtyMinX = currentX;
            if (currentX + (int)bmp->width > dirtyMaxX) dirtyMaxX = currentX + (int)bmp->width;
            if (currentY < dirtyMinY) dirtyMinY = currentY;
            if (currentY + (int)bmp->rows > dirtyMaxY) dirtyMaxY = currentY + (int)bmp->rows;
        }
        float scale = 1.0f;
        if (isColorGlyph && bmp->rows > 0) {
            scale = 48.0f / (float)bmp->rows;
        }
        GlyphInfo info;
        info.width = (float)bmp->width * scale;
        info.height = (float)bmp->rows * scale;
        info.bearingX = (float)targetFace->glyph->bitmap_left * scale;
        info.bearingY = (float)targetFace->glyph->bitmap_top * scale;
        info.advance = (float)(targetFace->glyph->advance.x >> 6) * scale;
        info.u0 = (float)currentX / (float)width; info.v0 = (float)currentY / (float)height;
        info.u1 = (float)(currentX + bmp->width) / (float)width; info.v1 = (float)(currentY + bmp->rows) / (float)height;
        info.isColor = isColorGlyph;
        glyphs[charCode] = info;
        currentX += bmp->width + 1;
        if (bmp->rows > maxRowHeight) maxRowHeight = bmp->rows;
        return true;
    }
};
uint32_t decodeUtf8(const char** ptr, const char* end) {
    if (*ptr >= end) return 0;
    unsigned char c = **ptr;
    (*ptr)++;
    if (c < 0x80) return c;
    if ((c & 0xE0) == 0xC0) {
        if (*ptr >= end) return 0;
        uint32_t cp = (c & 0x1F) << 6;
        cp |= (**ptr & 0x3F);
        (*ptr)++;
        return cp;
    }
    if ((c & 0xF0) == 0xE0) {
        if (*ptr + 1 >= end) { *ptr = end; return 0; }
        uint32_t cp = (c & 0x0F) << 12;
        cp |= (**ptr & 0x3F) << 6; (*ptr)++;
        cp |= (**ptr & 0x3F);      (*ptr)++;
        return cp;
    }
    if ((c & 0xF8) == 0xF0) {
        if (*ptr + 2 >= end) { *ptr = end; return 0; }
        uint32_t cp = (c & 0x07) << 18;
        cp |= (**ptr & 0x3F) << 12; (*ptr)++;
        cp |= (**ptr & 0x3F) << 6;  (*ptr)++;
        cp |= (**ptr & 0x3F);       (*ptr)++;
        return cp;
    }
    return '?';
}
std::vector<uint8_t> loadSystemFont() {
    const char* paths[] = {
            "/system/fonts/NotoSansCJK-Regular.ttc",
            "/system/fonts/NotoSansJP-Regular.otf",
            "/system/fonts/Roboto-Regular.ttf"
    };
    for (const char* path : paths) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (file.is_open()) {
            size_t size = file.tellg();
            std::vector<uint8_t> buffer(size);
            file.seekg(0, std::ios::beg);
            file.read((char*)buffer.data(), size);
            return buffer;
        }
    }
    return {};
}
struct Piece { bool isOriginal; size_t start; size_t len; };
struct PieceTable {
    const char* origPtr = nullptr; size_t origSize = 0;
    std::string addBuf; std::vector<Piece> pieces;
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
            Piece p = pieces[idx]; size_t offsetInPiece = pos - cur;
            if (offsetInPiece > 0 && offsetInPiece < p.len) {
                pieces[idx] = { p.isOriginal, p.start, offsetInPiece };
                pieces.insert(pieces.begin() + idx + 1, { p.isOriginal, p.start + offsetInPiece, p.len - offsetInPiece });
                idx++;
            } else if (offsetInPiece == p.len) idx++;
        } else idx = pieces.size();
        size_t addStart = addBuf.size(); addBuf.append(s);
        pieces.insert(pieces.begin() + idx, { false, addStart, s.size() });
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
    }
    char charAt(size_t pos) const {
        size_t cur = 0;
        for (const auto& p : pieces) {
            if (cur + p.len <= pos) { cur += p.len; continue; }
            return p.isOriginal ? origPtr[p.start + (pos - cur)] : addBuf[p.start + (pos - cur)];
        }
        return '\0';
    }
};
struct Cursor {
    size_t head; size_t anchor; float desiredX;
    size_t start() const { return std::min(head, anchor); }
    size_t end() const { return std::max(head, anchor); }
    bool hasSelection() const { return head != anchor; }
};
struct EditOp { enum Type { Insert, Erase } type; size_t pos; std::string text; };
struct EditBatch { std::vector<EditOp> ops; std::vector<Cursor> beforeCursors; std::vector<Cursor> afterCursors; };
struct UndoManager {
    std::vector<EditBatch> undoStack; std::vector<EditBatch> redoStack;
    void push(const EditBatch& batch) { undoStack.push_back(batch); redoStack.clear(); }
};
int64_t getCurrentTimeMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
}
bool isWordChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_' ||
           (unsigned char)c >= 0x80;
}
struct Vertex {
    float pos[2];
    float uv[2];
    float isColor;
    float color[4];
};
struct PushConstants {
    float screenWidth;
    float screenHeight;
    float padding[2];
    float color[4];
};
struct Engine {
    struct android_app* app;
    bool isWindowReady = false;
    PieceTable pt;
    UndoManager undo;
    std::vector<Cursor> cursors;
    std::vector<size_t> lineStarts;
    std::string imeComp;
    std::string currentFilePath;
    bool isDirty = false;
    std::string searchQuery;
    std::string replaceQuery;
    bool searchMatchCase = false;
    bool searchWholeWord = false;
    bool searchRegex = false;
    bool isReplaceMode = false;
    float scrollX = 0.0f;
    float scrollY = 0.0f;
    float maxLineWidth = 0.0f;
    float bottomInset = 0.0f;
    float topMargin = 100.0f;
    int64_t lastClickTime = 0;
    int clickCount = 0;
    float lastClickX = 0.0f;
    float lastClickY = 0.0f;
    float lastTouchX = 0.0f;
    float lastTouchY = 0.0f;
    bool isDragging = false;
    bool isFlinging = false;
    float velocityX = 0.0f;
    float velocityY = 0.0f;
    int64_t lastMoveTime = 0;
    bool isPinching = false;
    float lastPinchDistance = 0.0f;
    bool isKeyboardShowing = false;
    int vScrollPos = 0;
    int hScrollPos = 0;
    float lineHeight = 60.0f;
    float charWidth = 24.0f;
    float gutterWidth = 100.0f;
    float currentFontSize = 48.0f;
    bool isDarkMode = false;
    float bgColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float textColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    float gutterBgColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float gutterTextColor[4] = {0.66f, 0.66f, 0.66f, 1.0f};
    float selColor[4] = {0.7f, 0.8f, 1.0f, 0.5f};
    float caretColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue graphicsQueue;
    uint32_t queueFamilyIndex;
    FT_Library ftLibrary;
    FT_Face ftFaceMain;
    FT_Face ftFaceEmoji = nullptr;
    std::vector<uint8_t> fontDataMain;
    std::vector<uint8_t> fontDataEmoji;
    TextAtlas atlas;
    VkSwapchainKHR swapchain;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    VkFormat swapchainFormat;
    VkExtent2D swapchainExtent;
    VkRenderPass renderPass;
    std::vector<VkFramebuffer> framebuffers;
    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;
    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;
    VkFence inFlightFence;
    VkImage fontImage = VK_NULL_HANDLE;
    VkDeviceMemory fontImageMemory = VK_NULL_HANDLE;
    VkImageView fontImageView = VK_NULL_HANDLE;
    VkSampler fontSampler = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline = VK_NULL_HANDLE;
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
    uint32_t vertexCount = 0;
    VkBuffer atlasStagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory atlasStagingMemory = VK_NULL_HANDLE;
};
void rebuildLineStarts(Engine* engine);
void ensureCaretVisible(Engine* engine);
float getXFromPos(Engine* engine, size_t pos);
void updateDirtyFlag(Engine* engine) {
    // Undoスタックの状態で変更があったかを判定する（簡易実装）
    // 本格的にはUndoManagerにsavePoint等の概念を入れると完璧です
    engine->isDirty = !engine->undo.undoStack.empty();
}
void performNewDocument(Engine* engine) {
    if (!engine) return;
    engine->pt.initEmpty();
    engine->currentFilePath.clear();
    engine->undo.undoStack.clear();
    engine->undo.redoStack.clear();
    engine->isDirty = false;
    engine->cursors.clear();
    engine->cursors.push_back({0, 0, 0.0f});
    engine->vScrollPos = 0;
    engine->hScrollPos = 0;
    engine->scrollX = 0.0f;
    engine->scrollY = 0.0f;
}
#include <regex>
std::string UnescapeString(const std::string& s) {
    std::string out; out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            switch (s[i + 1]) { case 'n': out += '\n'; break; case 'r': out += '\r'; break; case 't': out += '\t'; break; case '\\': out += '\\'; break; default: out += s[i]; out += s[i + 1]; break; } i++;
        } else out += s[i];
    } return out;
}
std::string preprocessRegexQuery(const std::string& query) {
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
            bool inClass = (i > 0 && query[i - 1] == '[');
            if (!inClass) { processed += "((?:^|(?:\\r\\n|\\r(?!\\n)|[\\n])))"; continue; }
        } else if (c == '$') {
            bool inClass = (i > 0 && query[i - 1] == '[');
            if (!inClass) { processed += "(?=(?:\\r\\n|[\\r\\n]|$))"; continue; }
        }
        processed += c;
    }
    return processed;
}
size_t findText(Engine* engine, size_t startPos, const std::string& query, bool forward, bool matchCase, bool wholeWord, bool isRegex, size_t* outLen) {
    if (query.empty()) return std::string::npos;
    size_t len = engine->pt.length();
    std::string actualQuery = query;
    if (isRegex) actualQuery = preprocessRegexQuery(query);

    if (isRegex) {
        std::string fullText = engine->pt.getRange(0, len);
        try {
            std::regex_constants::syntax_option_type flags = std::regex_constants::ECMAScript;
            if (!matchCase) flags |= std::regex_constants::icase;
            std::regex re(actualQuery, flags);
            std::smatch m;
            size_t foundPos = std::string::npos;
            size_t foundLen = 0;

            if (forward) {
                if (startPos > fullText.size()) startPos = 0;
                size_t searchStartIdx = startPos;
                std::string::const_iterator searchStartIter = fullText.begin() + searchStartIdx;
                if (std::regex_search(searchStartIter, fullText.cend(), m, re)) {
                    foundPos = searchStartIdx + m.position();
                    foundLen = m.length();
                } else if (startPos > 0 && std::regex_search(fullText.cbegin(), fullText.cend(), m, re)) {
                    foundPos = m.position();
                    foundLen = m.length();
                }
            } else {
                // 後方検索は簡易的に全件取得して直前のものを返す
                auto words_begin = std::sregex_iterator(fullText.begin(), fullText.end(), re);
                auto words_end = std::sregex_iterator();
                size_t bestPos = std::string::npos;
                size_t bestLen = 0;
                size_t limit = (startPos == 0) ? len : startPos;
                for (auto i = words_begin; i != words_end; ++i) {
                    if ((size_t)i->position() < limit) {
                        bestPos = i->position();
                        bestLen = i->length();
                    }
                }
                foundPos = bestPos;
                foundLen = bestLen;
            }
            if (foundPos != std::string::npos) {
                if (outLen) *outLen = foundLen;
                return foundPos;
            }
        } catch (...) { return std::string::npos; }
        return std::string::npos;
    }

    size_t qLen = query.length();
    if (outLen) *outLen = qLen;
    auto toLower = [](char c) { return (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c; };
    size_t cur = startPos;
    if (forward) { if (cur >= len) cur = 0; } else { if (cur == 0) cur = len; else cur--; }
    size_t count = 0;

    while (count < len) {
        bool match = true;
        for (size_t i = 0; i < qLen; ++i) {
            size_t p = cur + i;
            if (p >= len) { match = false; break; }
            char c1 = engine->pt.charAt(p); char c2 = query[i];
            if (!matchCase) { c1 = toLower(c1); c2 = toLower(c2); }
            if (c1 != c2) { match = false; break; }
        }
        if (match && wholeWord) {
            if (cur > 0 && isWordChar(engine->pt.charAt(cur - 1))) match = false;
            if (match && (cur + qLen < len) && isWordChar(engine->pt.charAt(cur + qLen))) match = false;
        }
        if (match) return cur;
        if (forward) { cur++; if (cur >= len) cur = 0; } else { if (cur == 0) cur = len - 1; else cur--; }
        count++;
    }
    return std::string::npos;
}
void findNextCommand(Engine* engine, bool forward) {
    if (engine->searchQuery.empty()) return;
    size_t currentCursorPos = forward ? (engine->cursors.empty() ? 0 : engine->cursors.back().end()) : (engine->cursors.empty() ? 0 : engine->cursors.back().start());
    size_t matchLen = 0;
    size_t pos = findText(engine, currentCursorPos, engine->searchQuery, forward, engine->searchMatchCase, engine->searchWholeWord, engine->searchRegex, &matchLen);

    if (pos != std::string::npos) {
        engine->cursors.clear();
        engine->cursors.push_back({ pos + matchLen, pos, getXFromPos(engine, pos + matchLen) });
        ensureCaretVisible(engine);
    }
}
void replaceNextCommand(Engine* engine) {
    if (engine->cursors.empty() || engine->searchQuery.empty()) return;
    Cursor& c = engine->cursors.back();
    if (!c.hasSelection()) { findNextCommand(engine, true); return; }

    size_t len = c.end() - c.start();
    size_t start = c.start();
    std::string selText = engine->pt.getRange(start, len);
    std::string replacement = engine->replaceQuery; // Regexのキャプチャグループ置換は今回簡易化

    EditBatch batch;
    batch.beforeCursors = engine->cursors;
    engine->pt.erase(start, len);
    batch.ops.push_back({ EditOp::Erase, start, selText });
    engine->pt.insert(start, replacement);
    batch.ops.push_back({ EditOp::Insert, start, replacement });

    engine->cursors.clear();
    size_t newEnd = start + replacement.size();
    engine->cursors.push_back({ newEnd, start, getXFromPos(engine, newEnd) });
    batch.afterCursors = engine->cursors;
    engine->undo.push(batch);
    engine->isDirty = true;

    rebuildLineStarts(engine);
    ensureCaretVisible(engine);
    findNextCommand(engine, true);
}
void replaceAllCommand(Engine* engine) {
    if (engine->searchQuery.empty()) return;
    size_t currentPos = 0;
    std::vector<std::pair<size_t, size_t>> matches;
    while (true) {
        size_t matchLen = 0;
        size_t pos = findText(engine, currentPos, engine->searchQuery, true, engine->searchMatchCase, engine->searchWholeWord, engine->searchRegex, &matchLen);
        if (pos == std::string::npos || pos < currentPos) break;
        matches.push_back({ pos, matchLen });
        currentPos = pos + matchLen;
    }
    if (matches.empty()) return;

    EditBatch batch;
    batch.beforeCursors = engine->cursors;
    for (auto it = matches.rbegin(); it != matches.rend(); ++it) {
        size_t start = it->first;
        size_t len = it->second;
        std::string deleted = engine->pt.getRange(start, len);
        engine->pt.erase(start, len);
        batch.ops.push_back({ EditOp::Erase, start, deleted });
        engine->pt.insert(start, engine->replaceQuery);
        batch.ops.push_back({ EditOp::Insert, start, engine->replaceQuery });
    }
    engine->isDirty = true;
    engine->undo.push(batch);
    rebuildLineStarts(engine);
    ensureCaretVisible(engine);
}
bool openDocumentFromFile(Engine* engine, const std::string& path) {
    if (!engine) return false;
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> buffer(size);
    if (file.read(buffer.data(), size)) {
        engine->pt.initEmpty();
        engine->pt.insert(0, std::string(buffer.data(), size));
        engine->currentFilePath = path;
        engine->undo.undoStack.clear();
        engine->undo.redoStack.clear();
        engine->isDirty = false;
        engine->cursors.clear();
        engine->cursors.push_back({0, 0, 0.0f});
        engine->vScrollPos = 0;
        engine->hScrollPos = 0;
        engine->scrollX = 0.0f;
        engine->scrollY = 0.0f;
        return true;
    }
    return false;
}
void updateThemeColors(Engine* engine) {
    if (!engine->app || !engine->app->config) return;
    int32_t uiMode = AConfiguration_getUiModeNight(engine->app->config);
    engine->isDarkMode = (uiMode == ACONFIGURATION_UI_MODE_NIGHT_YES);
    if (engine->isDarkMode) {
        engine->bgColor[0] = 0.0f; engine->bgColor[1] = 0.0f; engine->bgColor[2] = 0.0f; engine->bgColor[3] = 1.0f;
        engine->textColor[0] = 1.0f; engine->textColor[1] = 1.0f; engine->textColor[2] = 1.0f; engine->textColor[3] = 1.0f;
        engine->gutterBgColor[0] = 0.0f; engine->gutterBgColor[1] = 0.0f; engine->gutterBgColor[2] = 0.0f; engine->gutterBgColor[3] = 1.0f;
        engine->gutterTextColor[0] = 0.33f; engine->gutterTextColor[1] = 0.33f; engine->gutterTextColor[2] = 0.33f; engine->gutterTextColor[3] = 1.0f;
        engine->selColor[0] = 0.0f; engine->selColor[1] = 0.47f; engine->selColor[2] = 0.84f; engine->selColor[3] = 0.5f;
        engine->caretColor[0] = 1.0f; engine->caretColor[1] = 1.0f; engine->caretColor[2] = 1.0f; engine->caretColor[3] = 1.0f;
    } else {
        engine->bgColor[0] = 1.0f; engine->bgColor[1] = 1.0f; engine->bgColor[2] = 1.0f; engine->bgColor[3] = 1.0f;
        engine->textColor[0] = 0.0f; engine->textColor[1] = 0.0f; engine->textColor[2] = 0.0f; engine->textColor[3] = 1.0f;
        engine->gutterBgColor[0] = 1.0f; engine->gutterBgColor[1] = 1.0f; engine->gutterBgColor[2] = 1.0f; engine->gutterBgColor[3] = 1.0f;
        engine->gutterTextColor[0] = 0.66f; engine->gutterTextColor[1] = 0.66f; engine->gutterTextColor[2] = 0.66f; engine->gutterTextColor[3] = 1.0f;
        engine->selColor[0] = 0.7f; engine->selColor[1] = 0.8f; engine->selColor[2] = 1.0f; engine->selColor[3] = 0.5f;
        engine->caretColor[0] = 0.0f; engine->caretColor[1] = 0.0f; engine->caretColor[2] = 0.0f; engine->caretColor[3] = 1.0f;
    }
}
#include <mutex>
#include <deque>
struct ImeEvent {
    enum Type { Commit, Composing, Delete } type;
    std::string text;
};
std::mutex g_imeMutex;
std::deque<ImeEvent> g_imeQueue;
Engine* g_engine = nullptr;
void updateGutterWidth(Engine* engine) {
    int totalLines = (int)engine->lineStarts.size();
    if (totalLines == 0) totalLines = 1;
    int digits = 1;
    int tempLines = totalLines;
    while (tempLines >= 10) {
        tempLines /= 10;
        digits++;
    }
    engine->gutterWidth = (float)(digits * engine->charWidth) + (engine->charWidth * 1.0f);
}
void rebuildLineStarts(Engine* engine) {
    engine->lineStarts.clear();
    engine->lineStarts.push_back(0);
    size_t totalLen = engine->pt.length();
    for (size_t i = 0; i < totalLen; ++i) {
        if (engine->pt.charAt(i) == '\n') {
            engine->lineStarts.push_back(i + 1);
        }
    }
    updateGutterWidth(engine);
}
int getLineIdx(Engine* engine, size_t pos) {
    if (engine->lineStarts.empty()) return 0;
    auto it = std::upper_bound(engine->lineStarts.begin(), engine->lineStarts.end(), pos);
    int idx = (int)std::distance(engine->lineStarts.begin(), it) - 1;
    return std::max(0, std::min(idx, (int)engine->lineStarts.size() - 1));
}
float getXFromPos(Engine* engine, size_t pos) {
    int lineIdx = getLineIdx(engine, pos);
    if (lineIdx < 0 || lineIdx >= engine->lineStarts.size()) return 0.0f;
    size_t start = engine->lineStarts[lineIdx];
    size_t len = pos - start;
    if (len <= 0) return 0.0f;
    std::string text = engine->pt.getRange(start, len);
    const char* ptr = text.data();
    const char* end = ptr + text.size();
    float x = 0.0f;
    float scale = engine->currentFontSize / 48.0f;
    while (ptr < end) {
        uint32_t cp = decodeUtf8(&ptr, end);
        if (cp == 0) break;
        if (engine->atlas.glyphs.count(cp) == 0 && engine->ftFaceMain != nullptr) {
            engine->atlas.loadChar(engine->ftFaceMain, engine->ftFaceEmoji, cp);
        }
        if (engine->atlas.glyphs.count(cp) > 0) {
            x += engine->atlas.glyphs[cp].advance * scale;
        } else {
            x += engine->charWidth;
        }
    }
    return x;
}
size_t getDocPosFromPoint(Engine* engine, float touchX, float touchY) {
    float virtualY = touchY + engine->scrollY - engine->topMargin;
    if (virtualY < 0.0f) virtualY = 0.0f;
    int lineIdx = (int)(virtualY / engine->lineHeight);
    if (lineIdx < 0) return 0;
    if (lineIdx >= engine->lineStarts.size()) return engine->pt.length();
    size_t start = engine->lineStarts[lineIdx];
    size_t end = (lineIdx + 1 < engine->lineStarts.size()) ? engine->lineStarts[lineIdx + 1] : engine->pt.length();
    if (end > start && engine->pt.charAt(end - 1) == '\n') end--;
    if (end > start && engine->pt.charAt(end - 1) == '\r') end--;
    size_t len = end - start;
    if (len == 0) return start;
    std::string lineStr = engine->pt.getRange(start, len);
    const char* ptr = lineStr.data();
    const char* strEnd = ptr + lineStr.size();
    float currentX = engine->gutterWidth - engine->scrollX;
    size_t currentPos = start;
    float scale = engine->currentFontSize / 48.0f;
    while (ptr < strEnd) {
        const char* prevPtr = ptr;
        uint32_t cp = decodeUtf8(&ptr, strEnd);
        if (cp == 0) break;
        if (engine->atlas.glyphs.count(cp) == 0 && engine->ftFaceMain != nullptr) {
            engine->atlas.loadChar(engine->ftFaceMain, engine->ftFaceEmoji, cp);
        }
        float advance = engine->charWidth;
        if (engine->atlas.glyphs.count(cp) > 0) {
            advance = engine->atlas.glyphs[cp].advance * scale;
        }
        if (touchX < currentX + (advance / 2.0f)) {
            break;
        }
        currentX += advance;
        currentPos += (ptr - prevPtr);
    }
    return currentPos;
}
void selectWordAt(Engine* engine, size_t pos) {
    if (pos >= engine->pt.length()) {
        engine->cursors.clear();
        engine->cursors.push_back({ pos, pos, getXFromPos(engine, pos) });
        return;
    }
    char c = engine->pt.charAt(pos);
    if (c == '\n' || c == '\r') {
        engine->cursors.clear();
        engine->cursors.push_back({ pos, pos, getXFromPos(engine, pos) });
        return;
    }
    bool targetType = isWordChar(c);
    size_t start = pos;
    while (start > 0) {
        char p = engine->pt.charAt(start - 1);
        if (isWordChar(p) != targetType || p == '\n' || p == '\r') break;
        start--;
    }
    size_t end = pos;
    size_t len = engine->pt.length();
    while (end < len) {
        char p = engine->pt.charAt(end);
        if (isWordChar(p) != targetType || p == '\n' || p == '\r') break;
        end++;
    }
    engine->cursors.clear();
    engine->cursors.push_back({ end, start, getXFromPos(engine, end) });
}
void ensureCaretVisible(Engine* engine) {
    if (engine->cursors.empty()) return;
    size_t pos = engine->cursors.back().head;
    int lineIdx = getLineIdx(engine, pos);
    float caretY = engine->topMargin + lineIdx * engine->lineHeight;
    float caretX = getXFromPos(engine, pos);
    float winW = 1080.0f;
    float winH = 2000.0f;
    if (engine->app->window != nullptr) {
        winW = (float)ANativeWindow_getWidth(engine->app->window);
        winH = (float)ANativeWindow_getHeight(engine->app->window);
    }
    float visibleH = winH - engine->bottomInset;
    if (visibleH < winH * 0.3f) visibleH = winH * 0.5f;
    if (caretY < engine->scrollY + engine->topMargin) {
        engine->scrollY = caretY - engine->topMargin;
    } else if (caretY + engine->lineHeight > engine->scrollY + visibleH) {
        engine->scrollY = caretY + engine->lineHeight - visibleH;
    }
    if (caretX < engine->scrollX) {
        engine->scrollX = caretX;
    } else if (caretX + engine->charWidth * 2.0f > engine->scrollX + winW - engine->gutterWidth) {
        engine->scrollX = caretX + engine->charWidth * 2.0f - (winW - engine->gutterWidth);
    }
    if (engine->scrollX < 0.0f) engine->scrollX = 0.0f;
    if (engine->scrollY < 0.0f) engine->scrollY = 0.0f;
}
void insertAtCursors(Engine* engine, const std::string& text) {
    if (engine->cursors.empty() || text.empty()) return;
    EditBatch batch;
    batch.beforeCursors = engine->cursors;
    Cursor& c = engine->cursors.back();
    if (c.hasSelection()) {
        size_t s = c.start(); size_t l = c.end() - s;
        std::string d = engine->pt.getRange(s, l);
        engine->pt.erase(s, l);
        batch.ops.push_back({ EditOp::Erase, s, d });
        c.head = s; c.anchor = s;
    }
    engine->pt.insert(c.head, text);
    batch.ops.push_back({ EditOp::Insert, c.head, text });
    c.head += text.size();
    c.anchor = c.head;
    c.desiredX = getXFromPos(engine, c.head);
    batch.afterCursors = engine->cursors;
    engine->undo.push(batch);
    engine->isDirty = true;
    rebuildLineStarts(engine);
    ensureCaretVisible(engine);
}
void backspaceAtCursors(Engine* engine) {
    if (engine->cursors.empty()) return;
    Cursor& c = engine->cursors.back();
    if (c.head > 0 && !c.hasSelection()) {
        size_t eraseLen = 1;
        while (c.head - eraseLen > 0 && (engine->pt.charAt(c.head - eraseLen) & 0xC0) == 0x80) {
            eraseLen++;
        }
        std::string d = engine->pt.getRange(c.head - eraseLen, eraseLen);
        engine->pt.erase(c.head - eraseLen, eraseLen);
        c.head -= eraseLen;
        c.anchor = c.head;
        engine->isDirty = true;
        rebuildLineStarts(engine);
    }
    ensureCaretVisible(engine);
}
extern "C" {
JNIEXPORT void JNICALL Java_jp_hack_miu_MainActivity_commitText(JNIEnv* env, jobject thiz, jstring text) {
    if (!g_engine) return;
    const char* str = env->GetStringUTFChars(text, nullptr);
    if (str) {
        std::lock_guard<std::mutex> lock(g_imeMutex);
        g_imeQueue.push_back({ ImeEvent::Commit, str });
        env->ReleaseStringUTFChars(text, str);
    }
}
JNIEXPORT void JNICALL Java_jp_hack_miu_MainActivity_setComposingText(JNIEnv* env, jobject thiz, jstring text) {
    if (!g_engine) return;
    const char* str = env->GetStringUTFChars(text, nullptr);
    if (str) {
        std::lock_guard<std::mutex> lock(g_imeMutex);
        g_imeQueue.push_back({ ImeEvent::Composing, str });
        env->ReleaseStringUTFChars(text, str);
    }
}
JNIEXPORT void JNICALL Java_jp_hack_miu_MainActivity_deleteSurroundingText(JNIEnv* env, jobject thiz) {
    if (!g_engine) return;
    std::lock_guard<std::mutex> lock(g_imeMutex);
    g_imeQueue.push_back({ ImeEvent::Delete, "" });
}
JNIEXPORT void JNICALL Java_jp_hack_miu_MainActivity_updateVisibleHeight(JNIEnv* env, jobject thiz, jint bottomInset) {
    if (!g_engine) return;
    float oldInset = g_engine->bottomInset;
    g_engine->bottomInset = (float)bottomInset;
    g_engine->isKeyboardShowing = (bottomInset > 0);
    if (g_engine->bottomInset > oldInset) {
        ensureCaretVisible(g_engine);
    }
}
JNIEXPORT void JNICALL Java_jp_hack_miu_MainActivity_updateTopMargin(JNIEnv* env, jobject thiz, jint topMargin) {
    if (!g_engine) return;
    g_engine->topMargin = (float)topMargin;
    ensureCaretVisible(g_engine);
}
JNIEXPORT void JNICALL Java_jp_hack_miu_MainActivity_cmdNewDocument(JNIEnv* env, jobject thiz) {
    if (!g_engine) return;
    std::lock_guard<std::mutex> lock(g_imeMutex);
    performNewDocument(g_engine);
    rebuildLineStarts(g_engine);
    ensureCaretVisible(g_engine);
}
JNIEXPORT jboolean JNICALL Java_jp_hack_miu_MainActivity_cmdOpenDocument(JNIEnv* env, jobject thiz, jstring path) {
    if (!g_engine) return JNI_FALSE;
    const char* strPath = env->GetStringUTFChars(path, nullptr);
    bool success = false;
    if (strPath) {
        std::lock_guard<std::mutex> lock(g_imeMutex);
        success = openDocumentFromFile(g_engine, strPath);
        env->ReleaseStringUTFChars(path, strPath);
        if (success) {
            rebuildLineStarts(g_engine);
            ensureCaretVisible(g_engine);
        }
    }
    return success ? JNI_TRUE : JNI_FALSE;
}
JNIEXPORT jboolean JNICALL Java_jp_hack_miu_MainActivity_cmdIsDirty(JNIEnv* env, jobject thiz) {
    if (!g_engine) return JNI_FALSE;
    return g_engine->isDirty ? JNI_TRUE : JNI_FALSE;
}
JNIEXPORT jstring JNICALL Java_jp_hack_miu_MainActivity_cmdGetTextContent(JNIEnv* env, jobject thiz) {
    if (!g_engine) return env->NewStringUTF("");
    std::lock_guard<std::mutex> lock(g_imeMutex);
    std::string text = g_engine->pt.getRange(0, g_engine->pt.length());
    return env->NewStringUTF(text.c_str());
}
JNIEXPORT void JNICALL Java_jp_hack_miu_MainActivity_cmdMarkSaved(JNIEnv* env, jobject thiz) {
    if (!g_engine) return;
    std::lock_guard<std::mutex> lock(g_imeMutex);
    g_engine->isDirty = false;
}
JNIEXPORT void JNICALL Java_jp_hack_miu_MainActivity_cmdSetSearchOptions(JNIEnv* env, jobject thiz, jstring query, jstring replace, jboolean matchCase, jboolean wholeWord, jboolean regex) {
    if (!g_engine) return;
    std::lock_guard<std::mutex> lock(g_imeMutex);
    if (query) {
        const char* q = env->GetStringUTFChars(query, nullptr);
        g_engine->searchQuery = q ? q : "";
        env->ReleaseStringUTFChars(query, q);
    } else g_engine->searchQuery = "";
    if (replace) {
        const char* r = env->GetStringUTFChars(replace, nullptr);
        g_engine->replaceQuery = r ? r : "";
        env->ReleaseStringUTFChars(replace, r);
    } else g_engine->replaceQuery = "";
    g_engine->searchMatchCase = matchCase;
    g_engine->searchWholeWord = wholeWord;
    g_engine->searchRegex = regex;
}
JNIEXPORT void JNICALL Java_jp_hack_miu_MainActivity_cmdFindNext(JNIEnv* env, jobject thiz, jboolean forward) {
    if (!g_engine) return;
    std::lock_guard<std::mutex> lock(g_imeMutex);
    findNextCommand(g_engine, forward);
}
JNIEXPORT void JNICALL Java_jp_hack_miu_MainActivity_cmdReplaceNext(JNIEnv* env, jobject thiz) {
    if (!g_engine) return;
    std::lock_guard<std::mutex> lock(g_imeMutex);
    replaceNextCommand(g_engine);
}
JNIEXPORT void JNICALL Java_jp_hack_miu_MainActivity_cmdReplaceAll(JNIEnv* env, jobject thiz) {
    if (!g_engine) return;
    std::lock_guard<std::mutex> lock(g_imeMutex);
    replaceAllCommand(g_engine);
}
}
void cleanupVulkan(Engine* engine) {
    if (engine->device == VK_NULL_HANDLE) return;
    if (engine->graphicsPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(engine->device, engine->graphicsPipeline, nullptr);
        engine->graphicsPipeline = VK_NULL_HANDLE;
    }
    vkDeviceWaitIdle(engine->device);
    if (engine->vertexBuffer != VK_NULL_HANDLE) vkDestroyBuffer(engine->device, engine->vertexBuffer, nullptr);
    if (engine->vertexBufferMemory != VK_NULL_HANDLE) vkFreeMemory(engine->device, engine->vertexBufferMemory, nullptr);
    if (engine->atlasStagingBuffer != VK_NULL_HANDLE) vkDestroyBuffer(engine->device, engine->atlasStagingBuffer, nullptr);
    if (engine->atlasStagingMemory != VK_NULL_HANDLE) vkFreeMemory(engine->device, engine->atlasStagingMemory, nullptr);
    if (engine->pipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(engine->device, engine->pipelineLayout, nullptr);
    if (engine->descriptorPool != VK_NULL_HANDLE) vkDestroyDescriptorPool(engine->device, engine->descriptorPool, nullptr);
    if (engine->descriptorSetLayout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(engine->device, engine->descriptorSetLayout, nullptr);
    if (engine->fontSampler != VK_NULL_HANDLE) vkDestroySampler(engine->device, engine->fontSampler, nullptr);
    if (engine->fontImageView != VK_NULL_HANDLE) vkDestroyImageView(engine->device, engine->fontImageView, nullptr);
    if (engine->fontImage != VK_NULL_HANDLE) vkDestroyImage(engine->device, engine->fontImage, nullptr);
    if (engine->fontImageMemory != VK_NULL_HANDLE) vkFreeMemory(engine->device, engine->fontImageMemory, nullptr);
    if (engine->ftFaceMain) { FT_Done_Face(engine->ftFaceMain); engine->ftFaceMain = nullptr; }
    if (engine->ftFaceEmoji) { FT_Done_Face(engine->ftFaceEmoji); engine->ftFaceEmoji = nullptr; }
    if (engine->ftLibrary) { FT_Done_FreeType(engine->ftLibrary); engine->ftLibrary = nullptr; }
    if (engine->imageAvailableSemaphore != VK_NULL_HANDLE) vkDestroySemaphore(engine->device, engine->imageAvailableSemaphore, nullptr);
    if (engine->renderFinishedSemaphore != VK_NULL_HANDLE) vkDestroySemaphore(engine->device, engine->renderFinishedSemaphore, nullptr);
    if (engine->inFlightFence != VK_NULL_HANDLE) vkDestroyFence(engine->device, engine->inFlightFence, nullptr);
    if (engine->commandPool != VK_NULL_HANDLE) vkDestroyCommandPool(engine->device, engine->commandPool, nullptr);
    for (auto fb : engine->framebuffers) if (fb != VK_NULL_HANDLE) vkDestroyFramebuffer(engine->device, fb, nullptr);
    engine->framebuffers.clear();
    if (engine->renderPass != VK_NULL_HANDLE) vkDestroyRenderPass(engine->device, engine->renderPass, nullptr);
    for (auto iv : engine->swapchainImageViews) if (iv != VK_NULL_HANDLE) vkDestroyImageView(engine->device, iv, nullptr);
    engine->swapchainImageViews.clear();
    if (engine->swapchain != VK_NULL_HANDLE) { vkDestroySwapchainKHR(engine->device, engine->swapchain, nullptr); engine->swapchain = VK_NULL_HANDLE; }
    vkDestroyDevice(engine->device, nullptr); engine->device = VK_NULL_HANDLE;
    if (engine->surface != VK_NULL_HANDLE) { vkDestroySurfaceKHR(engine->instance, engine->surface, nullptr); engine->surface = VK_NULL_HANDLE; }
    if (engine->instance != VK_NULL_HANDLE) { vkDestroyInstance(engine->instance, nullptr); engine->instance = VK_NULL_HANDLE; }
}
uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) return i;
    }
    return 0;
}
void createBuffer(Engine* engine, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = size; bufferInfo.usage = usage; bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCreateBuffer(engine->device, &bufferInfo, nullptr, &buffer);
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(engine->device, buffer, &memRequirements);
    VkMemoryAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(engine->physicalDevice, memRequirements.memoryTypeBits, properties);
    vkAllocateMemory(engine->device, &allocInfo, nullptr, &bufferMemory);
    vkBindBufferMemory(engine->device, buffer, bufferMemory, 0);
}
VkCommandBuffer beginSingleTimeCommands(Engine* engine) {
    VkCommandBufferAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; allocInfo.commandPool = engine->commandPool; allocInfo.commandBufferCount = 1;
    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(engine->device, &allocInfo, &commandBuffer);
    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    return commandBuffer;
}
void endSingleTimeCommands(Engine* engine, VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);
    VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1; submitInfo.pCommandBuffers = &commandBuffer;
    vkQueueSubmit(engine->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(engine->graphicsQueue);
    vkFreeCommandBuffers(engine->device, engine->commandPool, 1, &commandBuffer);
}
void transitionImageLayout(Engine* engine, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands(engine);
    VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = oldLayout; barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0; barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0; barrier.subresourceRange.layerCount = 1;
    VkPipelineStageFlags sourceStage, destinationStage;
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0; barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT; destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT; destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        return;
    }
    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    endSingleTimeCommands(engine, commandBuffer);
}
void copyBufferToImage(Engine* engine, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands(engine);
    VkBufferImageCopy region = {};
    region.bufferOffset = 0; region.bufferRowLength = 0; region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0; region.imageSubresource.baseArrayLayer = 0; region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0}; region.imageExtent = {width, height, 1};
    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    endSingleTimeCommands(engine, commandBuffer);
}
bool createTextTexture(Engine* engine) {
    VkDeviceSize imageSize = (VkDeviceSize)engine->atlas.width * engine->atlas.height * 4;
    if (imageSize == 0 || engine->atlas.pixels.empty()) return false;
    VkBuffer stagingBuffer; VkDeviceMemory stagingBufferMemory;
    createBuffer(engine, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
    void* data;
    vkMapMemory(engine->device, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, engine->atlas.pixels.data(), static_cast<size_t>(imageSize));
    vkUnmapMemory(engine->device, stagingBufferMemory);
    VkImageCreateInfo imageInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = engine->atlas.width; imageInfo.extent.height = engine->atlas.height; imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1; imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT; imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateImage(engine->device, &imageInfo, nullptr, &engine->fontImage) != VK_SUCCESS) return false;
    VkMemoryRequirements memRequirements; vkGetImageMemoryRequirements(engine->device, engine->fontImage, &memRequirements);
    VkMemoryAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(engine->physicalDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(engine->device, &allocInfo, nullptr, &engine->fontImageMemory);
    vkBindImageMemory(engine->device, engine->fontImage, engine->fontImageMemory, 0);
    transitionImageLayout(engine, engine->fontImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(engine, stagingBuffer, engine->fontImage, engine->atlas.width, engine->atlas.height);
    transitionImageLayout(engine, engine->fontImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    vkDestroyBuffer(engine->device, stagingBuffer, nullptr);
    vkFreeMemory(engine->device, stagingBufferMemory, nullptr);
    VkImageViewCreateInfo viewInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = engine->fontImage; viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D; viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1; viewInfo.subresourceRange.baseArrayLayer = 0; viewInfo.subresourceRange.layerCount = 1;
    if (vkCreateImageView(engine->device, &viewInfo, nullptr, &engine->fontImageView) != VK_SUCCESS) return false;
    VkSamplerCreateInfo samplerInfo = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.magFilter = VK_FILTER_LINEAR; samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE; samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE; samplerInfo.compareEnable = VK_FALSE; samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    if (vkCreateSampler(engine->device, &samplerInfo, nullptr, &engine->fontSampler) != VK_SUCCESS) return false;
    return true;
}

// ★修正1: 物理ウィンドウの最新サイズを強制的に取得する
bool createSwapchain(Engine* engine) {
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(engine->physicalDevice, engine->surface, &capabilities);

    int32_t w = ANativeWindow_getWidth(engine->app->window);
    int32_t h = ANativeWindow_getHeight(engine->app->window);

    engine->swapchainExtent.width = std::clamp((uint32_t)w, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    engine->swapchainExtent.height = std::clamp((uint32_t)h, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

    if (engine->swapchainExtent.width == 0 || engine->swapchainExtent.height == 0) return false;

    uint32_t formatCount; vkGetPhysicalDeviceSurfaceFormatsKHR(engine->physicalDevice, engine->surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount); vkGetPhysicalDeviceSurfaceFormatsKHR(engine->physicalDevice, engine->surface, &formatCount, formats.data());
    VkSurfaceFormatKHR selectedFormat = formats[0];
    for (const auto& f : formats) if (f.format == VK_FORMAT_B8G8R8A8_UNORM || f.format == VK_FORMAT_R8G8B8A8_UNORM) { selectedFormat = f; break; }
    engine->swapchainFormat = selectedFormat.format;

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) imageCount = capabilities.maxImageCount;

    VkSwapchainKHR oldSwapchain = engine->swapchain;
    VkSwapchainCreateInfoKHR createInfo = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    createInfo.surface = engine->surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = engine->swapchainFormat;
    createInfo.imageColorSpace = selectedFormat.colorSpace;
    createInfo.imageExtent = engine->swapchainExtent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
        createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    } else {
        createInfo.preTransform = capabilities.currentTransform;
    }
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = oldSwapchain;

    VkSwapchainKHR newSwapchain;
    if (vkCreateSwapchainKHR(engine->device, &createInfo, nullptr, &newSwapchain) != VK_SUCCESS) return false;
    if (oldSwapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(engine->device, oldSwapchain, nullptr);
    }
    engine->swapchain = newSwapchain;

    vkGetSwapchainImagesKHR(engine->device, engine->swapchain, &imageCount, nullptr);
    engine->swapchainImages.resize(imageCount); vkGetSwapchainImagesKHR(engine->device, engine->swapchain, &imageCount, engine->swapchainImages.data());
    engine->swapchainImageViews.resize(imageCount);
    for (size_t i = 0; i < imageCount; i++) {
        VkImageViewCreateInfo viewInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = engine->swapchainImages[i]; viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D; viewInfo.format = engine->swapchainFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; viewInfo.subresourceRange.levelCount = 1; viewInfo.subresourceRange.layerCount = 1;
        vkCreateImageView(engine->device, &viewInfo, nullptr, &engine->swapchainImageViews[i]);
    }
    return true;
}

bool createRenderPass(Engine* engine) {
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = engine->swapchainFormat; colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkAttachmentReference colorAttachmentRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass = {}; subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1; subpass.pColorAttachments = &colorAttachmentRef;
    VkRenderPassCreateInfo renderPassInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    renderPassInfo.attachmentCount = 1; renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1; renderPassInfo.pSubpasses = &subpass;
    return vkCreateRenderPass(engine->device, &renderPassInfo, nullptr, &engine->renderPass) == VK_SUCCESS;
}

bool createFramebuffers(Engine* engine) {
    engine->framebuffers.resize(engine->swapchainImageViews.size());
    for (size_t i = 0; i < engine->swapchainImageViews.size(); i++) {
        VkImageView attachments[] = { engine->swapchainImageViews[i] };
        VkFramebufferCreateInfo fbInfo = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fbInfo.renderPass = engine->renderPass; fbInfo.attachmentCount = 1; fbInfo.pAttachments = attachments;
        fbInfo.width = engine->swapchainExtent.width; fbInfo.height = engine->swapchainExtent.height; fbInfo.layers = 1;
        if (vkCreateFramebuffer(engine->device, &fbInfo, nullptr, &engine->framebuffers[i]) != VK_SUCCESS) return false;
    }
    return true;
}

bool createCommandBuffers(Engine* engine) {
    VkCommandPoolCreateInfo poolInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolInfo.queueFamilyIndex = engine->queueFamilyIndex; poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(engine->device, &poolInfo, nullptr, &engine->commandPool) != VK_SUCCESS) return false;
    engine->commandBuffers.resize(engine->framebuffers.size());
    VkCommandBufferAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocInfo.commandPool = engine->commandPool; allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; allocInfo.commandBufferCount = (uint32_t)engine->commandBuffers.size();
    return vkAllocateCommandBuffers(engine->device, &allocInfo, engine->commandBuffers.data()) == VK_SUCCESS;
}

// ★修正2: RenderPass はフォーマットが変わらない限り破棄しない (パイプラインが壊れる原因になるため)
void recreateSwapchain(Engine* engine) {
    if (engine->device == VK_NULL_HANDLE || engine->app->window == nullptr) return;
    int width = ANativeWindow_getWidth(engine->app->window);
    int height = ANativeWindow_getHeight(engine->app->window);
    if (width == 0 || height == 0) return;
    vkDeviceWaitIdle(engine->device);

    for (auto fb : engine->framebuffers) {
        if (fb != VK_NULL_HANDLE) vkDestroyFramebuffer(engine->device, fb, nullptr);
    }
    engine->framebuffers.clear();

    if (!engine->commandBuffers.empty()) {
        vkFreeCommandBuffers(engine->device, engine->commandPool, static_cast<uint32_t>(engine->commandBuffers.size()), engine->commandBuffers.data());
        engine->commandBuffers.clear();
    }

    for (auto iv : engine->swapchainImageViews) {
        if (iv != VK_NULL_HANDLE) vkDestroyImageView(engine->device, iv, nullptr);
    }
    engine->swapchainImageViews.clear();

    createSwapchain(engine);
    createFramebuffers(engine);
    createCommandBuffers(engine);
}

bool createSyncObjects(Engine* engine) {
    VkSemaphoreCreateInfo semInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fenceInfo = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO}; fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    return vkCreateSemaphore(engine->device, &semInfo, nullptr, &engine->imageAvailableSemaphore) == VK_SUCCESS &&
           vkCreateSemaphore(engine->device, &semInfo, nullptr, &engine->renderFinishedSemaphore) == VK_SUCCESS &&
           vkCreateFence(engine->device, &fenceInfo, nullptr, &engine->inFlightFence) == VK_SUCCESS;
}

bool createDescriptors(Engine* engine) {
    VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
    samplerLayoutBinding.binding = 0; samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo layoutInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutInfo.bindingCount = 1; layoutInfo.pBindings = &samplerLayoutBinding;
    if (vkCreateDescriptorSetLayout(engine->device, &layoutInfo, nullptr, &engine->descriptorSetLayout) != VK_SUCCESS) return false;
    VkDescriptorPoolSize poolSize = {};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; poolSize.descriptorCount = 1;
    VkDescriptorPoolCreateInfo poolInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.poolSizeCount = 1; poolInfo.pPoolSizes = &poolSize; poolInfo.maxSets = 1;
    if (vkCreateDescriptorPool(engine->device, &poolInfo, nullptr, &engine->descriptorPool) != VK_SUCCESS) return false;
    VkDescriptorSetAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool = engine->descriptorPool; allocInfo.descriptorSetCount = 1; allocInfo.pSetLayouts = &engine->descriptorSetLayout;
    if (vkAllocateDescriptorSets(engine->device, &allocInfo, &engine->descriptorSet) != VK_SUCCESS) return false;
    VkDescriptorImageInfo imageInfo = {};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = engine->fontImageView; imageInfo.sampler = engine->fontSampler;
    VkWriteDescriptorSet descriptorWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    descriptorWrite.dstSet = engine->descriptorSet; descriptorWrite.dstBinding = 0; descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1; descriptorWrite.pImageInfo = &imageInfo;
    vkUpdateDescriptorSets(engine->device, 1, &descriptorWrite, 0, nullptr);
    return true;
}

bool createPipelineLayout(Engine* engine) {
    VkPushConstantRange pushConstantRange = {};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0; pushConstantRange.size = sizeof(PushConstants);
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineLayoutInfo.setLayoutCount = 1; pipelineLayoutInfo.pSetLayouts = &engine->descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1; pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    if (vkCreatePipelineLayout(engine->device, &pipelineLayoutInfo, nullptr, &engine->pipelineLayout) != VK_SUCCESS) return false;
    return true;
}

#include <android/asset_manager.h>
std::vector<uint32_t> loadShaderAsset(Engine* engine, const char* filename) {
    AAsset* asset = AAssetManager_open(engine->app->activity->assetManager, filename, AASSET_MODE_BUFFER);
    if (!asset) return {};
    size_t size = AAsset_getLength(asset);
    std::vector<uint32_t> buffer(size / sizeof(uint32_t));
    AAsset_read(asset, buffer.data(), size);
    AAsset_close(asset);
    return buffer;
}

VkShaderModule createShaderModule(Engine* engine, const std::vector<uint32_t>& code) {
    VkShaderModuleCreateInfo createInfo = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    createInfo.codeSize = code.size() * sizeof(uint32_t); createInfo.pCode = code.data();
    VkShaderModule shaderModule;
    vkCreateShaderModule(engine->device, &createInfo, nullptr, &shaderModule);
    return shaderModule;
}

std::vector<uint8_t> loadAsset(Engine* engine, const char* filename) {
    AAsset* asset = AAssetManager_open(engine->app->activity->assetManager, filename, AASSET_MODE_BUFFER);
    if (!asset) return {};
    size_t size = AAsset_getLength(asset);
    std::vector<uint8_t> buffer(size);
    AAsset_read(asset, buffer.data(), size);
    AAsset_close(asset);
    return buffer;
}

bool createGraphicsPipeline(Engine* engine) {
    auto vertCode = loadShaderAsset(engine, "shaders/text.vert.spv");
    auto fragCode = loadShaderAsset(engine, "shaders/text.frag.spv");
    if (vertCode.empty() || fragCode.empty()) return false;
    VkShaderModule vertModule = createShaderModule(engine, vertCode);
    VkShaderModule fragModule = createShaderModule(engine, fragCode);
    VkPipelineShaderStageCreateInfo vertStageInfo = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    vertStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT; vertStageInfo.module = vertModule; vertStageInfo.pName = "main";
    VkPipelineShaderStageCreateInfo fragStageInfo = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    fragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT; fragStageInfo.module = fragModule; fragStageInfo.pName = "main";
    VkPipelineShaderStageCreateInfo shaderStages[] = {vertStageInfo, fragStageInfo};
    VkVertexInputBindingDescription bindingDescription = {};
    bindingDescription.binding = 0; bindingDescription.stride = sizeof(Vertex); bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    VkVertexInputAttributeDescription attributeDescriptions[4] = {};
    attributeDescriptions[0].binding = 0; attributeDescriptions[0].location = 0; attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT; attributeDescriptions[0].offset = offsetof(Vertex, pos);
    attributeDescriptions[1].binding = 0; attributeDescriptions[1].location = 1; attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT; attributeDescriptions[1].offset = offsetof(Vertex, uv);
    attributeDescriptions[2].binding = 0; attributeDescriptions[2].location = 2; attributeDescriptions[2].format = VK_FORMAT_R32_SFLOAT; attributeDescriptions[2].offset = offsetof(Vertex, isColor);
    attributeDescriptions[3].binding = 0; attributeDescriptions[3].location = 3; attributeDescriptions[3].format = VK_FORMAT_R32G32B32A32_SFLOAT; attributeDescriptions[3].offset = offsetof(Vertex, color);
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInputInfo.vertexBindingDescriptionCount = 1; vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = 4; vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions;
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; inputAssembly.primitiveRestartEnable = VK_FALSE;
    VkPipelineViewportStateCreateInfo viewportState = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportState.viewportCount = 1; viewportState.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo rasterizer = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterizer.depthClampEnable = VK_FALSE; rasterizer.rasterizerDiscardEnable = VK_FALSE; rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f; rasterizer.cullMode = VK_CULL_MODE_NONE; rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    VkPipelineMultisampleStateCreateInfo multisampling = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisampling.sampleShadingEnable = VK_FALSE; multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    VkPipelineColorBlendStateCreateInfo colorBlending = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    colorBlending.logicOpEnable = VK_FALSE; colorBlending.attachmentCount = 1; colorBlending.pAttachments = &colorBlendAttachment;
    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamicState.dynamicStateCount = 2; dynamicState.pDynamicStates = dynamicStates;
    VkGraphicsPipelineCreateInfo pipelineInfo = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pipelineInfo.stageCount = 2; pipelineInfo.pStages = shaderStages; pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly; pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer; pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending; pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = engine->pipelineLayout; pipelineInfo.renderPass = engine->renderPass; pipelineInfo.subpass = 0;
    if (vkCreateGraphicsPipelines(engine->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &engine->graphicsPipeline) != VK_SUCCESS) return false;
    vkDestroyShaderModule(engine->device, fragModule, nullptr);
    vkDestroyShaderModule(engine->device, vertModule, nullptr);
    return true;
}

bool initVulkan(Engine* engine) {
    VkApplicationInfo appInfo = {VK_STRUCTURE_TYPE_APPLICATION_INFO}; appInfo.apiVersion = VK_API_VERSION_1_0;
    std::vector<const char*> instExt = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_ANDROID_SURFACE_EXTENSION_NAME};
    VkInstanceCreateInfo instInfo = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    instInfo.pApplicationInfo = &appInfo; instInfo.enabledExtensionCount = (uint32_t)instExt.size(); instInfo.ppEnabledExtensionNames = instExt.data();
    if (vkCreateInstance(&instInfo, nullptr, &engine->instance) != VK_SUCCESS) return false;
    VkAndroidSurfaceCreateInfoKHR surfInfo = {VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR}; surfInfo.window = engine->app->window;
    if (vkCreateAndroidSurfaceKHR(engine->instance, &surfInfo, nullptr, &engine->surface) != VK_SUCCESS) return false;
    uint32_t gpuCount = 0; vkEnumeratePhysicalDevices(engine->instance, &gpuCount, nullptr);
    std::vector<VkPhysicalDevice> gpus(gpuCount); vkEnumeratePhysicalDevices(engine->instance, &gpuCount, gpus.data());
    engine->physicalDevice = gpus[0];
    uint32_t qfCount = 0; vkGetPhysicalDeviceQueueFamilyProperties(engine->physicalDevice, &qfCount, nullptr);
    std::vector<VkQueueFamilyProperties> qfProps(qfCount); vkGetPhysicalDeviceQueueFamilyProperties(engine->physicalDevice, &qfCount, qfProps.data());
    for (uint32_t i = 0; i < qfCount; i++) {
        if (qfProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            VkBool32 support = false; vkGetPhysicalDeviceSurfaceSupportKHR(engine->physicalDevice, i, engine->surface, &support);
            if (support) { engine->queueFamilyIndex = i; break; }
        }
    }
    float priority = 1.0f; VkDeviceQueueCreateInfo qInfo = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qInfo.queueFamilyIndex = engine->queueFamilyIndex; qInfo.queueCount = 1; qInfo.pQueuePriorities = &priority;
    std::vector<const char*> devExt = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkDeviceCreateInfo devInfo = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    devInfo.queueCreateInfoCount = 1; devInfo.pQueueCreateInfos = &qInfo; devInfo.enabledExtensionCount = (uint32_t)devExt.size(); devInfo.ppEnabledExtensionNames = devExt.data();
    if (vkCreateDevice(engine->physicalDevice, &devInfo, nullptr, &engine->device) != VK_SUCCESS) return false;
    vkGetDeviceQueue(engine->device, engine->queueFamilyIndex, 0, &engine->graphicsQueue);
    if (!createSwapchain(engine)) return false;
    if (!createRenderPass(engine)) return false;
    if (!createFramebuffers(engine)) return false;
    if (!createCommandBuffers(engine)) return false;
    if (!createSyncObjects(engine)) return false;
    VkDeviceSize bufferSize = sizeof(Vertex) * 60000;
    createBuffer(engine, bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, engine->vertexBuffer, engine->vertexBufferMemory);
    createBuffer(engine, engine->atlas.width * engine->atlas.height * 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, engine->atlasStagingBuffer, engine->atlasStagingMemory);
    if (FT_Init_FreeType(&engine->ftLibrary)) return false;
    engine->atlas.init();
    engine->fontDataMain = loadSystemFont();
    if (!engine->fontDataMain.empty()) {
        if (FT_New_Memory_Face(engine->ftLibrary, engine->fontDataMain.data(), engine->fontDataMain.size(), 0, &engine->ftFaceMain) == 0) {
            FT_Set_Pixel_Sizes(engine->ftFaceMain, 0, 48);
        }
    }
    engine->fontDataEmoji = loadAsset(engine, "fonts/NotoColorEmoji.ttf");
    if (!engine->fontDataEmoji.empty()) {
        if (FT_New_Memory_Face(engine->ftLibrary, engine->fontDataEmoji.data(), engine->fontDataEmoji.size(), 0, &engine->ftFaceEmoji) == 0) {
            if (FT_HAS_FIXED_SIZES(engine->ftFaceEmoji)) {
                FT_Select_Size(engine->ftFaceEmoji, 0);
            } else {
                FT_Set_Pixel_Sizes(engine->ftFaceEmoji, 0, 48);
            }
        }
    }
    if (!createTextTexture(engine)) return false;
    if (!createDescriptors(engine)) return false;
    if (!createPipelineLayout(engine)) return false;
    if (!createGraphicsPipeline(engine)) return false;
    return true;
}

void updateTextVertices(Engine* engine) {
    float scale = engine->currentFontSize / 48.0f;
    float baselineOffset = engine->lineHeight * 0.8f;
    float x = engine->gutterWidth - engine->scrollX;
    float y = engine->topMargin + baselineOffset - engine->scrollY;
    engine->maxLineWidth = engine->gutterWidth;
    size_t len = engine->pt.length();
    std::string text = engine->pt.getRange(0, len);
    size_t mainCaretPos = engine->cursors.empty() ? 0 : engine->cursors.back().head;
    bool hasIME = !engine->imeComp.empty();
    if (hasIME) {
        if (mainCaretPos <= text.size()) text.insert(mainCaretPos, engine->imeComp);
        else text.append(engine->imeComp);
    }
    std::vector<size_t> visualCursors;
    for (const auto& c : engine->cursors) {
        size_t vPos = c.head;
        if (hasIME && vPos >= mainCaretPos) {
            vPos += engine->imeComp.size();
        }
        visualCursors.push_back(vPos);
    }
    const char* ptr = text.data();
    const char* end = ptr + text.size();
    size_t currentByteOffset = 0;
    std::vector<Vertex> bgVertices;
    std::vector<Vertex> lineVertices;
    std::vector<Vertex> charVertices;
    std::vector<Vertex> cursorVertices;
    float whiteU = 1.0f / engine->atlas.width;
    float whiteV = 1.0f / engine->atlas.height;
    auto addRect = [&](std::vector<Vertex>& verts, float rx, float ry, float rw, float rh, float r, float g, float b, float a) {
        verts.push_back({{rx,      ry     }, {whiteU, whiteV}, 0.0f, {r, g, b, a}});
        verts.push_back({{rx,      ry + rh}, {whiteU, whiteV}, 0.0f, {r, g, b, a}});
        verts.push_back({{rx + rw, ry     }, {whiteU, whiteV}, 0.0f, {r, g, b, a}});
        verts.push_back({{rx + rw, ry     }, {whiteU, whiteV}, 0.0f, {r, g, b, a}});
        verts.push_back({{rx,      ry + rh}, {whiteU, whiteV}, 0.0f, {r, g, b, a}});
        verts.push_back({{rx + rw, ry + rh}, {whiteU, whiteV}, 0.0f, {r, g, b, a}});
    };
    float textR = engine->textColor[0], textG = engine->textColor[1], textB = engine->textColor[2], textA = engine->textColor[3];
    float cursorWidth = std::max(2.0f, 4.0f * scale);
    std::vector<std::pair<size_t, size_t>> searchMatches;
    if (!engine->searchQuery.empty()) {
        size_t currentSearchPos = 0;
        size_t docLen = engine->pt.length();
        while (currentSearchPos < docLen) {
            size_t matchLen = 0;
            size_t found = findText(engine, currentSearchPos, engine->searchQuery, true, engine->searchMatchCase, engine->searchWholeWord, engine->searchRegex, &matchLen);
            if (found == std::string::npos || matchLen == 0 || found < currentSearchPos) break;
            searchMatches.push_back({found, found + matchLen});
            currentSearchPos = found + matchLen;
        }
    }
    while (ptr < end) {
        for (size_t vPos : visualCursors) {
            if (currentByteOffset == vPos) {
                float curY = y - engine->lineHeight * 0.8f;
                addRect(cursorVertices, x, curY, cursorWidth, engine->lineHeight,
                        engine->caretColor[0], engine->caretColor[1], engine->caretColor[2], engine->caretColor[3]);
            }
        }
        const char* prevPtr = ptr;
        uint32_t cp = 0;
        bool isNewlineChar = false;
        if (*ptr == '\n' || *ptr == '\r') {
            isNewlineChar = true;
            if (*ptr == '\n') {
                cp = 0x2193;
                ptr++;
            } else if (*ptr == '\r') {
                if (ptr + 1 < end && *(ptr + 1) == '\n') {
                    cp = 0x21B5;
                    ptr += 2;
                } else {
                    cp = 0x2190;
                    ptr++;
                }
            }
        } else {
            cp = decodeUtf8(&ptr, end);
        }
        if (cp == 0) break;
        size_t charBytes = ptr - prevPtr;
        if (engine->atlas.glyphs.count(cp) == 0 && engine->ftFaceMain != nullptr) {
            engine->atlas.loadChar(engine->ftFaceMain, engine->ftFaceEmoji, cp);
        }
        if (engine->atlas.glyphs.count(cp) == 0) {
            currentByteOffset += charBytes;
            if (isNewlineChar) {
                x = engine->gutterWidth - engine->scrollX;
                y += engine->lineHeight;
            }
            continue;
        }
        GlyphInfo& info = engine->atlas.glyphs[cp];
        float advanceForHighlight = isNewlineChar ? (engine->charWidth * scale) : (info.advance * scale);
        bool isSearchResult = false;
        for (const auto& match : searchMatches) {
            if (currentByteOffset >= match.first && currentByteOffset < match.second) {
                isSearchResult = true;
                break;
            }
        }
        bool isComposingChar = (hasIME && currentByteOffset >= mainCaretPos && currentByteOffset < mainCaretPos + engine->imeComp.size());
        bool isSelected = false;
        if (!engine->cursors.empty() && engine->cursors.back().hasSelection()) {
            size_t selStart = engine->cursors.back().start();
            size_t selEnd = engine->cursors.back().end();
            if (currentByteOffset >= selStart && currentByteOffset < selEnd) {
                isSelected = true;
            }
        }

        if (isComposingChar) {
            float bgY = y - engine->lineHeight * 0.8f;
            addRect(bgVertices, x, bgY, advanceForHighlight, engine->lineHeight, 0.2f, 0.6f, 1.0f, 0.3f);
            float lineY = y + engine->lineHeight * 0.1f;
            addRect(lineVertices, x, lineY, advanceForHighlight, 2.0f, textR, textG, textB, 1.0f);
        }
        else if (isSelected) {
            float bgY = y - engine->lineHeight * 0.8f;
            addRect(bgVertices, x, bgY, advanceForHighlight, engine->lineHeight,
                    engine->selColor[0], engine->selColor[1], engine->selColor[2], engine->selColor[3]);
        }
        else if (isSearchResult) {
            // 検索ハイライト（黄色半透明）
            float bgY = y - engine->lineHeight * 0.8f;
            addRect(bgVertices, x, bgY, advanceForHighlight, engine->lineHeight, 1.0f, 1.0f, 0.0f, 0.4f);
        }
        float isColorFlag = info.isColor ? 1.0f : 0.0f;
        float charColorR = textR, charColorG = textG, charColorB = textB, charColorA = textA;
        if (isNewlineChar) {
            charColorR = 0.50f; charColorG = 0.50f; charColorB = 0.50f; charColorA = 0.40f;
        }
        float xpos = x + info.bearingX * scale;
        float ypos = y - info.bearingY * scale;
        float w = info.width * scale;
        float h = info.height * scale;
        charVertices.push_back({{xpos,     ypos    }, {info.u0, info.v0}, isColorFlag, {charColorR, charColorG, charColorB, charColorA}});
        charVertices.push_back({{xpos,     ypos + h}, {info.u0, info.v1}, isColorFlag, {charColorR, charColorG, charColorB, charColorA}});
        charVertices.push_back({{xpos + w, ypos    }, {info.u1, info.v0}, isColorFlag, {charColorR, charColorG, charColorB, charColorA}});
        charVertices.push_back({{xpos + w, ypos    }, {info.u1, info.v0}, isColorFlag, {charColorR, charColorG, charColorB, charColorA}});
        charVertices.push_back({{xpos,     ypos + h}, {info.u0, info.v1}, isColorFlag, {charColorR, charColorG, charColorB, charColorA}});
        charVertices.push_back({{xpos + w, ypos + h}, {info.u1, info.v1}, isColorFlag, {charColorR, charColorG, charColorB, charColorA}});
        if (isNewlineChar) {
            x = engine->gutterWidth - engine->scrollX;
            y += engine->lineHeight;
        } else {
            x += info.advance * scale;
        }
        currentByteOffset += charBytes;
        float absoluteX = x + engine->scrollX;
        if (absoluteX > engine->maxLineWidth) {
            engine->maxLineWidth = absoluteX;
        }
    }
    for (size_t vPos : visualCursors) {
        if (currentByteOffset == vPos) {
            float curY = y - engine->lineHeight * 0.8f;
            addRect(cursorVertices, x, curY, cursorWidth, engine->lineHeight,
                    engine->caretColor[0], engine->caretColor[1], engine->caretColor[2], engine->caretColor[3]);
        }
    }
    std::vector<Vertex> gutterBgVertices;
    std::vector<Vertex> gutterTextVertices;
    float winH = 5000.0f;
    if (engine->app->window != nullptr) {
        winH = (float)ANativeWindow_getHeight(engine->app->window);
    }
    addRect(gutterBgVertices, 0.0f, 0.0f, engine->gutterWidth, winH,
            engine->gutterBgColor[0], engine->gutterBgColor[1], engine->gutterBgColor[2], engine->gutterBgColor[3]);
    float gutterTextR = engine->gutterTextColor[0], gutterTextG = engine->gutterTextColor[1], gutterTextB = engine->gutterTextColor[2];
    for (int i = 0; i < engine->lineStarts.size(); ++i) {
        float lineTop = engine->topMargin - engine->scrollY + i * engine->lineHeight;
        if (lineTop + engine->lineHeight < 0.0f || lineTop > winH) continue;
        std::string lineNumStr = std::to_string(i + 1);
        float numWidth = 0;
        for(char c : lineNumStr) {
            uint32_t cp = c;
            if (engine->atlas.glyphs.count(cp) == 0 && engine->ftFaceMain != nullptr) {
                engine->atlas.loadChar(engine->ftFaceMain, engine->ftFaceEmoji, cp);
            }
            if (engine->atlas.glyphs.count(cp) > 0) numWidth += engine->atlas.glyphs[cp].advance * scale;
            else numWidth += engine->charWidth;
        }
        float rightMargin = engine->charWidth * 0.5f;
        float numX = engine->gutterWidth - rightMargin - numWidth;
        float lineY = lineTop + baselineOffset;
        for(char c : lineNumStr) {
            uint32_t cp = c;
            if (engine->atlas.glyphs.count(cp) > 0) {
                GlyphInfo& info = engine->atlas.glyphs[cp];
                float xpos = numX + info.bearingX * scale;
                float ypos = lineY - info.bearingY * scale;
                float w = info.width * scale;
                float h = info.height * scale;
                gutterTextVertices.push_back({{xpos,     ypos    }, {info.u0, info.v0}, 0.0f, {gutterTextR, gutterTextG, gutterTextB, 1.0f}});
                gutterTextVertices.push_back({{xpos,     ypos + h}, {info.u0, info.v1}, 0.0f, {gutterTextR, gutterTextG, gutterTextB, 1.0f}});
                gutterTextVertices.push_back({{xpos + w, ypos    }, {info.u1, info.v0}, 0.0f, {gutterTextR, gutterTextG, gutterTextB, 1.0f}});
                gutterTextVertices.push_back({{xpos + w, ypos    }, {info.u1, info.v0}, 0.0f, {gutterTextR, gutterTextG, gutterTextB, 1.0f}});
                gutterTextVertices.push_back({{xpos,     ypos + h}, {info.u0, info.v1}, 0.0f, {gutterTextR, gutterTextG, gutterTextB, 1.0f}});
                gutterTextVertices.push_back({{xpos + w, ypos + h}, {info.u1, info.v1}, 0.0f, {gutterTextR, gutterTextG, gutterTextB, 1.0f}});
                numX += info.advance * scale;
            } else {
                numX += engine->charWidth;
            }
        }
    }
    std::vector<Vertex> vertices;
    vertices.insert(vertices.end(), bgVertices.begin(), bgVertices.end());
    vertices.insert(vertices.end(), lineVertices.begin(), lineVertices.end());
    vertices.insert(vertices.end(), charVertices.begin(), charVertices.end());
    vertices.insert(vertices.end(), cursorVertices.begin(), cursorVertices.end());
    vertices.insert(vertices.end(), gutterBgVertices.begin(), gutterBgVertices.end());
    vertices.insert(vertices.end(), gutterTextVertices.begin(), gutterTextVertices.end());
    float topH = engine->topMargin;
    float bgR = engine->bgColor[0], bgG = engine->bgColor[1], bgB = engine->bgColor[2];
    float winWidth = 5000.0f;
    float fadeHeight = topH * 0.8f;
    float solidHeight = topH - fadeHeight;
    if (solidHeight > 0.0f) {
        vertices.push_back({{0.0f,     0.0f},        {whiteU, whiteV}, 0.0f, {bgR, bgG, bgB, 1.0f}});
        vertices.push_back({{0.0f,     solidHeight}, {whiteU, whiteV}, 0.0f, {bgR, bgG, bgB, 1.0f}});
        vertices.push_back({{winWidth, 0.0f},        {whiteU, whiteV}, 0.0f, {bgR, bgG, bgB, 1.0f}});
        vertices.push_back({{winWidth, 0.0f},        {whiteU, whiteV}, 0.0f, {bgR, bgG, bgB, 1.0f}});
        vertices.push_back({{0.0f,     solidHeight}, {whiteU, whiteV}, 0.0f, {bgR, bgG, bgB, 1.0f}});
        vertices.push_back({{winWidth, solidHeight}, {whiteU, whiteV}, 0.0f, {bgR, bgG, bgB, 1.0f}});
    }
    if (fadeHeight > 0.0f) {
        vertices.push_back({{0.0f,     solidHeight}, {whiteU, whiteV}, 0.0f, {bgR, bgG, bgB, 1.0f}});
        vertices.push_back({{0.0f,     topH},        {whiteU, whiteV}, 0.0f, {bgR, bgG, bgB, 0.0f}});
        vertices.push_back({{winWidth, solidHeight}, {whiteU, whiteV}, 0.0f, {bgR, bgG, bgB, 1.0f}});
        vertices.push_back({{winWidth, solidHeight}, {whiteU, whiteV}, 0.0f, {bgR, bgG, bgB, 1.0f}});
        vertices.push_back({{0.0f,     topH},        {whiteU, whiteV}, 0.0f, {bgR, bgG, bgB, 0.0f}});
        vertices.push_back({{winWidth, topH},        {whiteU, whiteV}, 0.0f, {bgR, bgG, bgB, 0.0f}});
    }
    engine->vertexCount = static_cast<uint32_t>(vertices.size());
    if (engine->vertexCount == 0) return;
    void* data;
    vkMapMemory(engine->device, engine->vertexBufferMemory, 0, sizeof(Vertex) * engine->vertexCount, 0, &data);
    memcpy(data, vertices.data(), sizeof(Vertex) * engine->vertexCount);
    vkUnmapMemory(engine->device, engine->vertexBufferMemory);
}

void renderFrame(Engine* engine) {
    if (engine->device == VK_NULL_HANDLE || !engine->isWindowReady) return;
    vkWaitForFences(engine->device, 1, &engine->inFlightFence, VK_TRUE, UINT64_MAX);
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(engine->device, engine->swapchain, UINT64_MAX, engine->imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain(engine);
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        return;
    }
    vkResetFences(engine->device, 1, &engine->inFlightFence);
    vkResetCommandBuffer(engine->commandBuffers[imageIndex], 0);
    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(engine->commandBuffers[imageIndex], &beginInfo);
    updateTextVertices(engine);
    if (engine->atlas.isDirty) {
        uint32_t dx = engine->atlas.dirtyMinX;
        uint32_t dy = engine->atlas.dirtyMinY;
        uint32_t dw = engine->atlas.dirtyMaxX - engine->atlas.dirtyMinX;
        uint32_t dh = engine->atlas.dirtyMaxY - engine->atlas.dirtyMinY;
        if (dw > 0 && dh > 0) {
            void* mapData = nullptr;
            if (vkMapMemory(engine->device, engine->atlasStagingMemory, 0, dw * dh * 4, 0, &mapData) == VK_SUCCESS) {
                uint8_t* dst = (uint8_t*)mapData;
                const uint8_t* src = engine->atlas.pixels.data();
                for (uint32_t row = 0; row < dh; ++row) {
                    memcpy(dst + (row * dw) * 4, src + ((dy + row) * engine->atlas.width + dx) * 4, dw * 4);
                }
                vkUnmapMemory(engine->device, engine->atlasStagingMemory);
                VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
                barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.image = engine->fontImage;
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                barrier.subresourceRange.levelCount = 1; barrier.subresourceRange.layerCount = 1;
                barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT; barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                vkCmdPipelineBarrier(engine->commandBuffers[imageIndex], VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
                VkBufferImageCopy region = {};
                region.bufferOffset = 0; region.bufferRowLength = dw; region.bufferImageHeight = dh;
                region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; region.imageSubresource.layerCount = 1;
                region.imageOffset = {(int32_t)dx, (int32_t)dy, 0}; region.imageExtent = {dw, dh, 1};
                vkCmdCopyBufferToImage(engine->commandBuffers[imageIndex], engine->atlasStagingBuffer, engine->fontImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
                barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                vkCmdPipelineBarrier(engine->commandBuffers[imageIndex], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
            }
        }
        engine->atlas.isDirty = false;
    }
    VkRenderPassBeginInfo rpBegin = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpBegin.renderPass = engine->renderPass;
    rpBegin.framebuffer = engine->framebuffers[imageIndex];
    rpBegin.renderArea.extent = engine->swapchainExtent;
    VkClearValue clearColor = {{{engine->bgColor[0], engine->bgColor[1], engine->bgColor[2], engine->bgColor[3]}}};
    rpBegin.clearValueCount = 1;
    rpBegin.pClearValues = &clearColor;
    vkCmdBeginRenderPass(engine->commandBuffers[imageIndex], &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(engine->commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, engine->graphicsPipeline);
    VkViewport viewport = {};
    viewport.width = (float)engine->swapchainExtent.width; viewport.height = (float)engine->swapchainExtent.height; viewport.maxDepth = 1.0f;
    vkCmdSetViewport(engine->commandBuffers[imageIndex], 0, 1, &viewport);
    VkRect2D scissor = {}; scissor.extent = engine->swapchainExtent;
    vkCmdSetScissor(engine->commandBuffers[imageIndex], 0, 1, &scissor);
    if (engine->vertexCount > 0) {
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(engine->commandBuffers[imageIndex], 0, 1, &engine->vertexBuffer, offsets);
        vkCmdBindDescriptorSets(engine->commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, engine->pipelineLayout, 0, 1, &engine->descriptorSet, 0, nullptr);
        PushConstants pc;
        pc.screenWidth = (float)engine->swapchainExtent.width; pc.screenHeight = (float)engine->swapchainExtent.height;
        pc.color[0] = engine->textColor[0];
        pc.color[1] = engine->textColor[1];
        pc.color[2] = engine->textColor[2];
        pc.color[3] = engine->textColor[3];
        vkCmdPushConstants(engine->commandBuffers[imageIndex], engine->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pc);
        vkCmdDraw(engine->commandBuffers[imageIndex], engine->vertexCount, 1, 0, 0);
    }
    vkCmdEndRenderPass(engine->commandBuffers[imageIndex]);
    vkEndCommandBuffer(engine->commandBuffers[imageIndex]);
    VkSubmitInfo submit = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    VkSemaphore waitSem[] = {engine->imageAvailableSemaphore}; VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit.waitSemaphoreCount = 1; submit.pWaitSemaphores = waitSem; submit.pWaitDstStageMask = waitStages;
    submit.commandBufferCount = 1; submit.pCommandBuffers = &engine->commandBuffers[imageIndex];
    VkSemaphore sigSem[] = {engine->renderFinishedSemaphore}; submit.signalSemaphoreCount = 1; submit.pSignalSemaphores = sigSem;
    vkQueueSubmit(engine->graphicsQueue, 1, &submit, engine->inFlightFence);
    VkPresentInfoKHR present = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present.waitSemaphoreCount = 1; present.pWaitSemaphores = sigSem; present.swapchainCount = 1; present.pSwapchains = &engine->swapchain; present.pImageIndices = &imageIndex;
    VkResult presentResult = vkQueuePresentKHR(engine->graphicsQueue, &present);
    // ★修正3: VK_SUBOPTIMAL_KHRを無視する
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain(engine);
    }
}

void checkKeyboardVisibility(Engine* engine) {
    if (engine->isKeyboardShowing && !engine->cursors.empty() && engine->app->window != nullptr) {
        size_t pos = engine->cursors.back().head;
        int lineIdx = getLineIdx(engine, pos);
        float scale = engine->currentFontSize / 48.0f;
        float currentLineHeight = 60.0f * scale;
        float caretY = engine->topMargin + lineIdx * currentLineHeight;
        float displayY = caretY - engine->scrollY;
        float winH = (float)ANativeWindow_getHeight(engine->app->window);
        float visibleH = winH - engine->bottomInset;
        bool isOutsideY = (displayY + currentLineHeight < engine->topMargin) || (displayY > visibleH);
        if (isOutsideY) {
            engine->isKeyboardShowing = false;
            JNIEnv* env = nullptr;
            engine->app->activity->vm->AttachCurrentThread(&env, nullptr);
            jclass activityClass = env->GetObjectClass(engine->app->activity->clazz);
            jmethodID hideImeMethod = env->GetMethodID(activityClass, "hideSoftwareKeyboard", "()V");
            if (hideImeMethod) env->CallVoidMethod(engine->app->activity->clazz, hideImeMethod);
            env->DeleteLocalRef(activityClass);
            engine->app->activity->vm->DetachCurrentThread();
        }
    }
}

static int32_t handleInput(struct android_app* app, AInputEvent* event) {
    Engine* engine = (Engine*)app->userData;
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
        int32_t action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
        size_t pointerCount = AMotionEvent_getPointerCount(event);
        if (pointerCount >= 2) {
            float x0 = AMotionEvent_getX(event, 0);
            float y0 = AMotionEvent_getY(event, 0);
            float x1 = AMotionEvent_getX(event, 1);
            float y1 = AMotionEvent_getY(event, 1);
            float dx = x1 - x0;
            float dy = y1 - y0;
            float distance = std::sqrt(dx * dx + dy * dy);
            if (action == AMOTION_EVENT_ACTION_POINTER_DOWN) {
                engine->isPinching = true;
                engine->lastPinchDistance = distance;
                engine->isDragging = false;
            } else if (action == AMOTION_EVENT_ACTION_MOVE && engine->isPinching) {
                if (engine->lastPinchDistance > 0.0f) {
                    float ratio = distance / engine->lastPinchDistance;
                    float newSize = engine->currentFontSize * ratio;
                    if (newSize < 16.0f) newSize = 16.0f;
                    if (newSize > 200.0f) newSize = 200.0f;
                    engine->currentFontSize = newSize;
                    float scale = newSize / 48.0f;
                    engine->lineHeight = 60.0f * scale;
                    engine->charWidth = 24.0f * scale;
                    updateGutterWidth(engine);
                    engine->lastPinchDistance = distance;
                    ensureCaretVisible(engine);
                }
            } else if (action == AMOTION_EVENT_ACTION_POINTER_UP) {
                engine->isPinching = false;
                int upIndex = (AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
                int remainIndex = (upIndex == 0) ? 1 : 0;
                if (remainIndex < pointerCount) {
                    engine->lastTouchX = AMotionEvent_getX(event, remainIndex);
                    engine->lastTouchY = AMotionEvent_getY(event, remainIndex);
                }
            }
            return 1;
        }
        if (engine->isPinching) {
            if (action == AMOTION_EVENT_ACTION_UP) {
                engine->isPinching = false;
                engine->isDragging = false;
            }
            engine->lastTouchX = AMotionEvent_getX(event, 0);
            engine->lastTouchY = AMotionEvent_getY(event, 0);
            return 1;
        }
        float x = AMotionEvent_getX(event, 0);
        float y = AMotionEvent_getY(event, 0);
        if (action == AMOTION_EVENT_ACTION_DOWN) {
            int64_t now = getCurrentTimeMs();
            if (now - engine->lastClickTime < 400 &&
                std::abs(x - engine->lastClickX) < 30.0f &&
                std::abs(y - engine->lastClickY) < 30.0f) {
                engine->clickCount++;
            } else {
                engine->clickCount = 1;
            }
            engine->lastClickTime = now;
            engine->lastClickX = x;
            engine->lastClickY = y;
            engine->isDragging = false;
            engine->lastTouchX = x;
            engine->lastTouchY = y;
            engine->isFlinging = false;
            engine->velocityX = 0.0f;
            engine->velocityY = 0.0f;
            engine->lastMoveTime = now;
        } else if (action == AMOTION_EVENT_ACTION_MOVE) {
            int64_t now = getCurrentTimeMs();
            float dx = x - engine->lastTouchX;
            float dy = y - engine->lastTouchY;
            int64_t dt = now - engine->lastMoveTime;
            if (dt > 0) {
                float instVelX = dx / (float)dt;
                float instVelY = dy / (float)dt;
                engine->velocityX = engine->velocityX * 0.4f + instVelX * 0.6f;
                engine->velocityY = engine->velocityY * 0.4f + instVelY * 0.6f;
            }
            engine->lastMoveTime = now;
            if (!engine->isDragging && (std::abs(x - engine->lastClickX) > 10.0f || std::abs(y - engine->lastClickY) > 10.0f)) {
                engine->isDragging = true;
                engine->lastTouchX = x;
                engine->lastTouchY = y;
                dx = 0.0f;
                dy = 0.0f;
            }
            if (engine->isDragging) {
                engine->scrollX -= dx;
                engine->scrollY -= dy;
                if (engine->scrollX < 0.0f) engine->scrollX = 0.0f;
                if (engine->scrollY < 0.0f) engine->scrollY = 0.0f;
                if (app->window != nullptr) {
                    float winW = (float)ANativeWindow_getWidth(app->window);
                    float winH = (float)ANativeWindow_getHeight(app->window);
                    float visibleH = winH - engine->bottomInset;
                    float contentH = engine->lineStarts.size() * engine->lineHeight + engine->topMargin + engine->lineHeight;
                    float maxScrollY = std::max(0.0f, contentH - visibleH);
                    float maxScrollX = std::max(0.0f, engine->maxLineWidth - winW + engine->charWidth * 2.0f);
                    if (engine->scrollY > maxScrollY) engine->scrollY = maxScrollY;
                    if (engine->scrollX > maxScrollX) engine->scrollX = maxScrollX;
                }
                engine->lastTouchX = x;
                engine->lastTouchY = y;
                checkKeyboardVisibility(engine);
            }
        } else if (action == AMOTION_EVENT_ACTION_UP) {
            if (!engine->isDragging) {
                engine->isKeyboardShowing = true;
                JNIEnv* env = nullptr;
                app->activity->vm->AttachCurrentThread(&env, nullptr);
                jclass activityClass = env->GetObjectClass(app->activity->clazz);
                jmethodID showImeMethod = env->GetMethodID(activityClass, "showSoftwareKeyboard", "()V");
                if (showImeMethod) env->CallVoidMethod(app->activity->clazz, showImeMethod);
                env->DeleteLocalRef(activityClass);
                app->activity->vm->DetachCurrentThread();
                size_t targetPos = getDocPosFromPoint(engine, x, y);
                if (engine->clickCount == 2) {
                    selectWordAt(engine, targetPos);
                } else {
                    engine->cursors.clear();
                    engine->cursors.push_back({ targetPos, targetPos, getXFromPos(engine, targetPos) });
                }
            } else {
                float speed = std::sqrt(engine->velocityX * engine->velocityX + engine->velocityY * engine->velocityY);
                if (speed > 0.5f) {
                    engine->isFlinging = true;
                    engine->lastMoveTime = getCurrentTimeMs();
                }
            }
            engine->isDragging = false;
        }
        return 1;
    }
    return 0;
}
static void onAppCmd(struct android_app* app, int32_t cmd) {
    Engine* engine = (Engine*)app->userData;
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            if (app->window != nullptr) {
                LOGI("ウィンドウ生成。Vulkan初期化開始。");
                updateThemeColors(engine);
                if (initVulkan(engine)) {
                    engine->isWindowReady = true;
                }
            }
            break;
        case APP_CMD_CONFIG_CHANGED:
            updateThemeColors(engine);
            if (engine->isWindowReady) {
                recreateSwapchain(engine);
            }
            break;
        case APP_CMD_WINDOW_RESIZED:
        case APP_CMD_WINDOW_REDRAW_NEEDED:
            if (engine->isWindowReady) {
                recreateSwapchain(engine);
            }
            break;
        case APP_CMD_TERM_WINDOW:
            LOGI("ウィンドウ破棄。");
            engine->isWindowReady = false;
            cleanupVulkan(engine);
            break;
    }
}
void android_main(struct android_app* app) {
    Engine engine = {};
    engine.app = app;
    app->userData = &engine;
    app->onAppCmd = onAppCmd;
    app->onInputEvent = handleInput;
    g_engine = &engine;
    engine.pt.initEmpty();
    rebuildLineStarts(&engine);
    engine.cursors.push_back({0, 0, 0.0f});

    while (true) {
        int events;
        struct android_poll_source* source;
        int timeout = engine.isWindowReady ? 0 : -1;
        while (ALooper_pollOnce(timeout, nullptr, &events, (void**)&source) >= 0) {
            if (source != nullptr) source->process(app, source);
            if (app->destroyRequested != 0) { cleanupVulkan(&engine); return; }
            timeout = engine.isWindowReady ? 0 : -1;
        }
        if (engine.isWindowReady) {
            if (engine.isFlinging) {
                int64_t now = getCurrentTimeMs();
                int64_t dt = now - engine.lastMoveTime;
                if (dt > 0) {
                    if (dt > 50) dt = 50;
                    engine.scrollX -= engine.velocityX * dt;
                    engine.scrollY -= engine.velocityY * dt;
                    float friction = std::pow(0.992f, (float)dt);
                    engine.velocityX *= friction;
                    engine.velocityY *= friction;
                    if (std::abs(engine.velocityX) < 0.05f && std::abs(engine.velocityY) < 0.05f) {
                        engine.isFlinging = false;
                    }
                    if (engine.app->window != nullptr) {
                        float winW = (float)ANativeWindow_getWidth(engine.app->window);
                        float winH = (float)ANativeWindow_getHeight(engine.app->window);
                        float visibleH = winH - engine.bottomInset;
                        float contentH = engine.lineStarts.size() * engine.lineHeight + engine.topMargin + engine.lineHeight;
                        float maxScrollY = std::max(0.0f, contentH - visibleH);
                        float maxScrollX = std::max(0.0f, engine.maxLineWidth - winW + engine.charWidth * 2.0f);
                        if (engine.scrollX < 0.0f) { engine.scrollX = 0.0f; engine.velocityX = 0.0f; }
                        else if (engine.scrollX > maxScrollX) { engine.scrollX = maxScrollX; engine.velocityX = 0.0f; }
                        if (engine.scrollY < 0.0f) { engine.scrollY = 0.0f; engine.velocityY = 0.0f; }
                        else if (engine.scrollY > maxScrollY) { engine.scrollY = maxScrollY; engine.velocityY = 0.0f; }
                        checkKeyboardVisibility(&engine);
                    }
                    engine.lastMoveTime = now;
                }
            }
            {
                std::lock_guard<std::mutex> lock(g_imeMutex);
                while (!g_imeQueue.empty()) {
                    ImeEvent ev = g_imeQueue.front();
                    g_imeQueue.pop_front();
                    if (ev.type == ImeEvent::Commit) {
                        engine.imeComp.clear();
                        insertAtCursors(&engine, ev.text);
                    } else if (ev.type == ImeEvent::Composing) {
                        engine.imeComp = ev.text;
                    } else if (ev.type == ImeEvent::Delete) {
                        backspaceAtCursors(&engine);
                    }
                }
            }
            renderFrame(&engine);
        }
    }
}