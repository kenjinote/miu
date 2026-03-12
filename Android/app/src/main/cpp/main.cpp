#include <android_native_app_glue.h>
#include <android/log.h>
#include <android/configuration.h>
#include <android/asset_manager.h>
#include <jni.h>
#include <vector>
#include <string>
#include <algorithm>
#include <memory>
#include <unordered_map>
#include <fstream>
#include <chrono>
#include <cmath>
#include <mutex>
#include <deque>
#include <regex>

#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include <hb-ft.h>

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

struct Engine;

struct TextAtlas {
    int width = 4096;  // 2048 から 4096 に拡張
    int height = 4096; // 1024 から 4096 に拡張
    std::vector<uint8_t> pixels;
    std::unordered_map<uint64_t, GlyphInfo> glyphs; // キーを fontIndex + glyphIndex の合成値に変更
    int currentX = 0;
    int currentY = 0;
    int maxRowHeight = 0;
    bool isDirty = false;
    int dirtyMinX = 0, dirtyMinY = 0, dirtyMaxX = 0, dirtyMaxY = 0;
    void init() {
        glyphs.clear();
        pixels.assign(width * height * 4, 0);

        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 2; ++x) {
                int idx = (y * width + x) * 4;
                pixels[idx + 0] = 255; pixels[idx + 1] = 255; pixels[idx + 2] = 255; pixels[idx + 3] = 255;
            }
        }

        // ★修正: キャンバスサイズを大きく（高解像度に）して、線をしっかり太くする
        int nlX = 3; int nlY = 0;
        int nlW = 48; int nlH = 48; // 32 -> 48 に拡大
        float thickness = 4.5f;     // 2.0 -> 4.5 に太くする

        auto distToSeg = [](float px, float py, float ax, float ay, float bx, float by) {
            float l2 = (bx - ax)*(bx - ax) + (by - ay)*(by - ay);
            if (l2 == 0.0f) return std::sqrt((px - ax)*(px - ax) + (py - ay)*(py - ay));
            float t = std::max(0.0f, std::min(1.0f, ((px - ax)*(bx - ax) + (py - ay)*(by - ay)) / l2));
            float projx = ax + t * (bx - ax);
            float projy = ay + t * (by - ay);
            return std::sqrt((px - projx)*(px - projx) + (py - projy)*(py - projy));
        };

        float cy = nlH * 0.5f;
        float cx = nlW * 0.5f;
        float halfSz = nlH * 0.4f;
        float arrowSize = halfSz * 0.35f;
        float hLineRight = cx + halfSz * 0.6f;
        float hLineLeft = cx - halfSz * 0.6f;
        float vLineTop = cy - halfSz;
        float vLineBottom = cy + halfSz * 0.3f;

        for (int y = 0; y < nlH; ++y) {
            for (int x = 0; x < nlW; ++x) {
                float px = (float)x; float py = (float)y;

                float d1 = distToSeg(px, py, hLineRight, vLineTop, hLineRight, vLineBottom);
                float d2 = distToSeg(px, py, hLineRight, vLineBottom, hLineLeft, vLineBottom);
                float d3 = distToSeg(px, py, hLineLeft, vLineBottom, hLineLeft + arrowSize, vLineBottom - arrowSize);
                float d4 = distToSeg(px, py, hLineLeft, vLineBottom, hLineLeft + arrowSize, vLineBottom + arrowSize);

                float d = std::min({d1, d2, d3, d4});
                float alpha = std::max(0.0f, std::min(1.0f, (thickness / 2.0f) - d + 0.5f));
                uint8_t val = (uint8_t)(alpha * 255.0f);

                int idx = ((nlY + y) * width + (nlX + x)) * 4;
                pixels[idx + 0] = val; pixels[idx + 1] = val; pixels[idx + 2] = val; pixels[idx + 3] = val;
            }
        }

        GlyphInfo nlInfo;
        nlInfo.width = (float)nlW;
        nlInfo.height = (float)nlH;
        nlInfo.bearingX = 0.0f;
        nlInfo.bearingY = 24.0f;
        nlInfo.advance = 32.0f;
        nlInfo.u0 = (float)nlX / width; nlInfo.v0 = (float)nlY / height;
        nlInfo.u1 = (float)(nlX + nlW) / width; nlInfo.v1 = (float)(nlY + nlH) / height;
        nlInfo.isColor = false;

        uint64_t NEWLINE_KEY = 0xFFFFFFFFFFFFFFFF;
        glyphs[NEWLINE_KEY] = nlInfo;

        currentX = nlX + nlW + 1;
        currentY = 0;
        maxRowHeight = nlH; // 修正

        isDirty = true;
        dirtyMinX = 0; dirtyMinY = 0;
        dirtyMaxX = width; dirtyMaxY = maxRowHeight;
    }
    bool loadGlyph(Engine* engine, int fontIndex, uint32_t glyphIndex);
};

uint32_t decodeUtf8(const char** ptr, const char* end) {
    if (*ptr >= end) return 0;

    auto decodeSingle = [](const char** p, const char* e) -> uint32_t {
        if (*p >= e) return 0;
        unsigned char c = **p;
        (*p)++;
        if (c < 0x80) return c;
        if ((c & 0xE0) == 0xC0) {
            if (*p >= e) return 0;
            uint32_t cp = (c & 0x1F) << 6;
            cp |= (**p & 0x3F); (*p)++;
            return cp;
        }
        if ((c & 0xF0) == 0xE0) {
            if (*p + 1 >= e) { *p = e; return 0; }
            uint32_t cp = (c & 0x0F) << 12;
            cp |= (**p & 0x3F) << 6; (*p)++;
            cp |= (**p & 0x3F);      (*p)++;
            return cp;
        }
        if ((c & 0xF8) == 0xF0) {
            if (*p + 2 >= e) { *p = e; return 0; }
            uint32_t cp = (c & 0x07) << 18;
            cp |= (**p & 0x3F) << 12; (*p)++;
            cp |= (**p & 0x3F) << 6;  (*p)++;
            cp |= (**p & 0x3F);       (*p)++;
            return cp;
        }
        return '?';
    };

    uint32_t cp = decodeSingle(ptr, end);

    // MUTF-8サロゲートペア合成処理
    if (cp >= 0xD800 && cp <= 0xDBFF) {
        const char* nextPtr = *ptr;
        uint32_t nextCp = decodeSingle(&nextPtr, end);
        if (nextCp >= 0xDC00 && nextCp <= 0xDFFF) {
            cp = 0x10000 + ((cp - 0xD800) << 10) + (nextCp - 0xDC00);
            *ptr = nextPtr;
        }
    }
    return cp;
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

        // 挿入位置の特定
        while (idx < pieces.size() && cur + pieces[idx].len < pos) { cur += pieces[idx].len; ++idx; }

        // ピースの分割（途中に挿入された場合）
        if (idx < pieces.size()) {
            Piece p = pieces[idx]; size_t offsetInPiece = pos - cur;
            if (offsetInPiece > 0 && offsetInPiece < p.len) {
                pieces[idx] = { p.isOriginal, p.start, offsetInPiece };
                pieces.insert(pieces.begin() + idx + 1, { p.isOriginal, p.start + offsetInPiece, p.len - offsetInPiece });
                idx++;
            } else if (offsetInPiece == p.len) idx++;
        } else idx = pieces.size();

        size_t addStart = addBuf.size();
        addBuf.append(s);

        // ★大最適化: 「断片化の防止（ピースの結合）」
        // もし直前のピースが「追加バッファ」であり、かつ今回の追加文字と連続しているなら、
        // 新しいピースを作らずに、直前のピースの長さを伸ばすだけで済ませる！
        if (idx > 0 && !pieces[idx - 1].isOriginal && pieces[idx - 1].start + pieces[idx - 1].len == addStart) {
            pieces[idx - 1].len += s.size();
        } else {
            // 結合できない場合のみ新しいピースを作る
            pieces.insert(pieces.begin() + idx, { false, addStart, s.size() });
        }
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

struct ShapedGlyph {
    int fontIndex;
    uint32_t glyphIndex;
    float xAdvance, yAdvance, xOffset, yOffset;
    size_t cluster;
    bool isIME;
};

struct LineCache {
    std::vector<ShapedGlyph> glyphs;
    bool isShaped = false;
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

    std::vector<std::vector<uint8_t>> fallbackFontData;
    std::vector<FT_Face> fallbackFaces;
    std::vector<hb_font_t*> fallbackHbFonts;
    std::vector<float> fallbackFontScales;
    std::vector<LineCache> lineCaches;

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

// TextAtlasの実装
bool TextAtlas::loadGlyph(Engine* engine, int fontIndex, uint32_t glyphIndex) {
    uint64_t key = ((uint64_t)fontIndex << 32) | glyphIndex;
    if (glyphs.count(key) > 0) return true;
    if (fontIndex < 0 || fontIndex >= engine->fallbackFaces.size()) return false;

    FT_Face targetFace = engine->fallbackFaces[fontIndex];
    if (FT_Load_Glyph(targetFace, glyphIndex, FT_LOAD_RENDER | FT_LOAD_COLOR) != 0) {
        return false;
    }

    FT_Bitmap* bmp = &targetFace->glyph->bitmap;

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

    glyphs[key] = info;
    currentX += bmp->width + 1;
    if (bmp->rows > maxRowHeight) maxRowHeight = bmp->rows;

    return true;
}
int getFontIndexForChar(Engine* engine, uint32_t cp, int prevFontIndex) {
    // ★追加: フォントが全くロードされていない場合は安全のために -1 を返すなど
    if (engine->fallbackFaces.empty()) return -1;

    if (cp == 0x200D || (cp >= 0xFE00 && cp <= 0xFE0F)) return prevFontIndex;

    for (int i = 0; i < engine->fallbackFaces.size(); ++i) {
        if (FT_Get_Char_Index(engine->fallbackFaces[i], cp) != 0) return i;
    }
    return 0; // 見つからなければ最初のフォント(Roboto等)を強制
}
void ensureLineShaped(Engine* engine, int lineIdx);
void rebuildLineStarts(Engine* engine);
void ensureCaretVisible(Engine* engine);
float getXFromPos(Engine* engine, size_t pos);

void updateDirtyFlag(Engine* engine) {
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
    engine->lineCaches.clear();
}

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
    std::string replacement = engine->replaceQuery;

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

        // ★修正: UTF-8 BOM (EF BB BF) が先頭にあれば除去する
        size_t startOffset = 0;
        if (size >= 3 &&
            (unsigned char)buffer[0] == 0xEF &&
            (unsigned char)buffer[1] == 0xBB &&
            (unsigned char)buffer[2] == 0xBF) {
            startOffset = 3;
        }

        // BOMを除去した純粋なテキストを挿入
        engine->pt.insert(0, std::string(buffer.data() + startOffset, size - startOffset));

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
        engine->lineCaches.clear();
        engine->imeComp.clear(); // ★修正: 前のファイルの入力途中状態(IMEゴミ)をクリア
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

struct ImeEvent {
    enum Type { Commit, Composing, Delete, FinishComposing } type;
    std::string text;
};

std::mutex g_imeMutex;
std::deque<ImeEvent> g_imeQueue;
Engine* g_engine = nullptr;

void updateGutterWidth(Engine* engine) {
    int totalLines = (int)engine->lineStarts.size();
    if (totalLines == 0) totalLines = 1;
    if (engine->fallbackFaces.empty()) {
        engine->gutterWidth = 100.0f; // 適当なデフォルト値
        return;
    }
    std::string maxLineStr = std::to_string(totalLines);
    float width = 0.0f;
    float scale = engine->currentFontSize / 48.0f;

    for (char c : maxLineStr) {
        uint32_t cp = c;
        int fontIdx = getFontIndexForChar(engine, cp, 0);
        uint32_t glyphIdx = FT_Get_Char_Index(engine->fallbackFaces[fontIdx], cp);
        uint64_t key = ((uint64_t)fontIdx << 32) | glyphIdx;

        if (engine->atlas.glyphs.count(key) == 0 && !engine->fallbackFaces.empty()) {
            engine->atlas.loadGlyph(engine, fontIdx, glyphIdx);
        }
        if (engine->atlas.glyphs.count(key) > 0) {
            width += engine->atlas.glyphs[key].advance * scale;
        } else {
            width += engine->charWidth;
        }
    }
    engine->gutterWidth = width + engine->charWidth * 1.0f;
}

void rebuildLineStarts(Engine* engine) {
    engine->lineStarts.clear();
    engine->lineStarts.push_back(0);

    size_t currentPos = 0;

    // ★大最適化: テキスト全体のコピー(getRange)をやめ、
    // PieceTableのメモリ断片を直接覗き込んで改行を高速スキャンする
    for (const auto& p : engine->pt.pieces) {
        // 現在のピースが元のファイルか、追加バッファかを判定
        const char* buf = p.isOriginal ? engine->pt.origPtr : engine->pt.addBuf.data();
        size_t startIdx = p.start;
        size_t len = p.len;

        for (size_t i = 0; i < len; ++i) {
            if (buf[startIdx + i] == '\n') {
                engine->lineStarts.push_back(currentPos + i + 1);
            }
        }
        currentPos += len;
    }

    updateGutterWidth(engine);
    engine->lineCaches.clear();
    engine->lineCaches.resize(engine->lineStarts.size());
}

int getLineIdx(Engine* engine, size_t pos) {
    if (engine->lineStarts.empty()) return 0;
    auto it = std::upper_bound(engine->lineStarts.begin(), engine->lineStarts.end(), pos);
    int idx = (int)std::distance(engine->lineStarts.begin(), it) - 1;
    return std::max(0, std::min(idx, (int)engine->lineStarts.size() - 1));
}

void ensureLineShaped(Engine* engine, int lineIdx) {
    if (lineIdx < 0 || lineIdx >= engine->lineCaches.size()) return;

    // ★修正: ウィンドウの準備(全フォントの読み込み)が終わっていない間は、
    // 中途半端なフォントで「豆腐」がキャッシュされるのを防ぐため、計算をスキップする
    if (!engine->isWindowReady || engine->fallbackHbFonts.empty() || engine->fallbackFaces.empty()) {
        return;
    }
    LineCache& cache = engine->lineCaches[lineIdx];
    if (cache.isShaped) return;

    cache.glyphs.clear();
    cache.isShaped = true;

    size_t start = engine->lineStarts[lineIdx];
    size_t end = (lineIdx + 1 < engine->lineStarts.size()) ? engine->lineStarts[lineIdx + 1] : engine->pt.length();

    size_t textEnd = end;
    if (textEnd > start && engine->pt.charAt(textEnd - 1) == '\n') textEnd--;
    if (textEnd > start && engine->pt.charAt(textEnd - 1) == '\r') textEnd--;

    std::string text = engine->pt.getRange(start, textEnd - start);

    bool hasIME = !engine->imeComp.empty() && !engine->cursors.empty();
    size_t imeInsertPos = 0;
    if (hasIME) {
        size_t mainCaret = engine->cursors.back().head;
        if (mainCaret >= start && mainCaret <= textEnd) {
            imeInsertPos = mainCaret - start;
            text.insert(imeInsertPos, engine->imeComp);
        } else { hasIME = false; }
    }

    if (text.empty()) return;

    // ★修正: UTF-8文字列を直接渡すのをやめ、コードポイント(UTF-32)とバイト位置の対応表を作る
    struct Run {
        int fontIdx;
        std::vector<uint32_t> codepoints;
        std::vector<size_t> byteOffsets;
    };
    std::vector<Run> runs;
    const char* ptr = text.data();
    const char* end_ptr = ptr + text.size();
    int currentFont = -1;

    while (ptr < end_ptr) {
        size_t prevOffset = ptr - text.data();
        uint32_t cp = decodeUtf8(&ptr, end_ptr); // MUTF-8を正しくデコード
        int fIdx = getFontIndexForChar(engine, cp, currentFont == -1 ? 0 : currentFont);

        if (fIdx != currentFont) {
            runs.push_back({fIdx, {}, {}});
            currentFont = fIdx;
        }
        runs.back().codepoints.push_back(cp);
        runs.back().byteOffsets.push_back(prevOffset);
    }

    hb_buffer_t* hb_buf = hb_buffer_create();
    for (const auto& run : runs) {
        hb_buffer_clear_contents(hb_buf);

        // ★修正: デコード済みの配列をHarfBuzzに渡す（結合絵文字が完璧に認識される）
        hb_buffer_add_codepoints(hb_buf, run.codepoints.data(), run.codepoints.size(), 0, run.codepoints.size());
        hb_buffer_set_direction(hb_buf, HB_DIRECTION_LTR);
        hb_buffer_set_script(hb_buf, HB_SCRIPT_COMMON);
        hb_buffer_guess_segment_properties(hb_buf);

        hb_shape(engine->fallbackHbFonts[run.fontIdx], hb_buf, nullptr, 0);

        unsigned int count;
        hb_glyph_info_t* info = hb_buffer_get_glyph_infos(hb_buf, &count);
        hb_glyph_position_t* pos = hb_buffer_get_glyph_positions(hb_buf, &count);
        float fontScale = engine->fallbackFontScales[run.fontIdx];

        for (unsigned int i = 0; i < count; i++) {
            ShapedGlyph sg;
            sg.fontIndex = run.fontIdx;
            sg.glyphIndex = info[i].codepoint;
            sg.xAdvance = (pos[i].x_advance / 64.0f) * fontScale;
            sg.yAdvance = (pos[i].y_advance / 64.0f) * fontScale;
            sg.xOffset  = (pos[i].x_offset / 64.0f) * fontScale;
            sg.yOffset  = (pos[i].y_offset / 64.0f) * fontScale;

            // ★修正: 対応表から元のバイト位置（クラスター）を復元
            size_t charIdx = info[i].cluster;
            size_t rawCluster = run.byteOffsets[charIdx];

            if (hasIME) {
                if (rawCluster >= imeInsertPos && rawCluster < imeInsertPos + engine->imeComp.size()) {
                    sg.isIME = true;
                    sg.cluster = start + imeInsertPos;
                } else if (rawCluster >= imeInsertPos + engine->imeComp.size()) {
                    sg.isIME = false;
                    sg.cluster = start + rawCluster - engine->imeComp.size();
                } else {
                    sg.isIME = false;
                    sg.cluster = start + rawCluster;
                }
            } else {
                sg.isIME = false;
                sg.cluster = start + rawCluster;
            }
            cache.glyphs.push_back(sg);
        }
    }
    hb_buffer_destroy(hb_buf);
}

float getXFromPos(Engine* engine, size_t pos) {
    int lineIdx = getLineIdx(engine, pos);
    if (lineIdx < 0 || lineIdx >= engine->lineStarts.size()) return engine->gutterWidth;

    ensureLineShaped(engine, lineIdx);

    float x = engine->gutterWidth;
    float scale = engine->currentFontSize / 48.0f;
    bool hasIME = !engine->imeComp.empty() && !engine->cursors.empty();
    size_t mainCaret = engine->cursors.empty() ? 0 : engine->cursors.back().head;

    for (const auto& sg : engine->lineCaches[lineIdx].glyphs) {
        if (hasIME && pos == mainCaret) {
            if (!sg.isIME && sg.cluster >= pos) break;
        } else {
            if (sg.cluster >= pos) break;
        }
        x += sg.xAdvance * scale;
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
    ensureLineShaped(engine, lineIdx);

    float currentX = engine->gutterWidth - engine->scrollX;
    float scale = engine->currentFontSize / 48.0f;

    const auto& glyphs = engine->lineCaches[lineIdx].glyphs;
    size_t i = 0;

    // ★修正: 1つの視覚的な文字（クラスター）単位で幅を計算し、タップ判定を行う
    while (i < glyphs.size()) {
        size_t cluster = glyphs[i].cluster;
        bool isIME = glyphs[i].isIME;
        float clusterAdvance = 0.0f;

        // 同じクラスターに属する全グリフ（結合絵文字や合字）の幅をすべて合計する
        size_t next_i = i;
        while (next_i < glyphs.size() && glyphs[next_i].cluster == cluster && glyphs[next_i].isIME == isIME) {
            clusterAdvance += glyphs[next_i].xAdvance * scale;
            next_i++;
        }

        // タップしたX座標が、このクラスターの「左半分」なら、クラスターの先頭位置にカーソルを置く
        if (touchX < currentX + (clusterAdvance / 2.0f)) {
            return isIME ? engine->cursors.back().head : cluster;
        }

        // 「右半分」をタップしていた場合は、X座標をクラスターの幅だけ進め、次のクラスターの判定へ移る
        // （もしこのクラスターが最後だった場合、ループを抜けて自動的に行末尾にカーソルが置かれます）
        currentX += clusterAdvance;
        i = next_i;
    }

    // 行の最後までタップ判定が引っかからなかった場合は、行の末尾を返す
    size_t end = (lineIdx + 1 < engine->lineStarts.size()) ? engine->lineStarts[lineIdx + 1] : engine->pt.length();

    // 改行文字（\r や \n）の右側（次の行の先頭）に行かないよう、改行文字の直前にカーソルを補正する
    if (end > start && engine->pt.charAt(end - 1) == '\n') {
        end--;
        if (end > start && engine->pt.charAt(end - 1) == '\r') end--;
    } else if (end > start && engine->pt.charAt(end - 1) == '\r') {
        end--;
    }

    return end;
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

    // 選択範囲がある場合はそれを削除して終了
    if (c.hasSelection()) {
        size_t s = c.start(); size_t l = c.end() - s;
        std::string d = engine->pt.getRange(s, l);
        EditBatch batch;
        batch.beforeCursors = engine->cursors;
        engine->pt.erase(s, l);
        batch.ops.push_back({ EditOp::Erase, s, d });
        c.head = s; c.anchor = s;
        batch.afterCursors = engine->cursors;
        engine->undo.push(batch);
        engine->isDirty = true;
        rebuildLineStarts(engine);
        ensureCaretVisible(engine);
        return;
    }

    if (c.head == 0) return;

    size_t eraseStart = c.head - 1;
    bool foundBoundary = false;

    // ★修正: HarfBuzzのクラスター情報を利用して、結合絵文字などの「正しい切れ目」を特定する
    int lineIdx = getLineIdx(engine, c.head);
    if (lineIdx >= 0 && lineIdx < engine->lineStarts.size()) {
        ensureLineShaped(engine, lineIdx);
        const auto& cache = engine->lineCaches[lineIdx];

        size_t bestStart = 0;
        for (const auto& sg : cache.glyphs) {
            if (!sg.isIME && sg.cluster < c.head) {
                if (!foundBoundary || sg.cluster > bestStart) {
                    bestStart = sg.cluster;
                    foundBoundary = true;
                }
            }
        }
        if (foundBoundary) {
            eraseStart = bestStart;
        }
    }

    // キャッシュが見つからない場合（改行の削除など）のフォールバック
    if (!foundBoundary) {
        char ch = engine->pt.charAt(c.head - 1);
        if (ch == '\n' || ch == '\r') {
            eraseStart = c.head - 1;
            if (ch == '\n' && c.head >= 2 && engine->pt.charAt(c.head - 2) == '\r') {
                eraseStart = c.head - 2;
            }
        } else {
            eraseStart = c.head - 1;
            while (eraseStart > 0 && (engine->pt.charAt(eraseStart) & 0xC0) == 0x80) {
                eraseStart--;
            }
            // MUTF-8サロゲートペア対応
            if (c.head - eraseStart == 3 && eraseStart >= 3) {
                std::string bytes = engine->pt.getRange(eraseStart, 3);
                if ((unsigned char)bytes[0] == 0xED && (unsigned char)bytes[1] >= 0xB0 && (unsigned char)bytes[1] <= 0xBF) {
                    size_t highStart = eraseStart - 1;
                    while (highStart > 0 && (engine->pt.charAt(highStart) & 0xC0) == 0x80) highStart--;
                    if (eraseStart - highStart == 3) {
                        std::string hBytes = engine->pt.getRange(highStart, 3);
                        if ((unsigned char)hBytes[0] == 0xED && (unsigned char)hBytes[1] >= 0xA0 && (unsigned char)hBytes[1] <= 0xAF) {
                            eraseStart = highStart;
                        }
                    }
                }
            }
        }
    }

    // 特定した切れ目からカーソル位置までを一気に削除
    size_t eraseLen = c.head - eraseStart;
    std::string d = engine->pt.getRange(eraseStart, eraseLen);

    EditBatch batch;
    batch.beforeCursors = engine->cursors;
    engine->pt.erase(eraseStart, eraseLen);
    batch.ops.push_back({ EditOp::Erase, eraseStart, d });

    c.head = eraseStart; c.anchor = c.head;
    batch.afterCursors = engine->cursors;
    engine->undo.push(batch);

    engine->isDirty = true;
    rebuildLineStarts(engine);
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
JNIEXPORT void JNICALL Java_jp_hack_miu_MainActivity_finishComposingTextNative(JNIEnv* env, jobject thiz) {
    if (!g_engine) return;
    std::lock_guard<std::mutex> lock(g_imeMutex);
    g_imeQueue.push_back({ ImeEvent::FinishComposing, "" });
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

    for (hb_font_t* hb_font : engine->fallbackHbFonts) {
        if (hb_font) hb_font_destroy(hb_font);
    }
    engine->fallbackHbFonts.clear();
    engine->lineCaches.clear();
    for (FT_Face face : engine->fallbackFaces) {
        if (face) FT_Done_Face(face);
    }
    engine->fallbackFaces.clear();
    engine->fallbackFontData.clear();

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

    VkDeviceSize bufferSize = sizeof(Vertex) * 1000000;
    createBuffer(engine, bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, engine->vertexBuffer, engine->vertexBufferMemory);
    createBuffer(engine, engine->atlas.width * engine->atlas.height * 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, engine->atlasStagingBuffer, engine->atlasStagingMemory);

    if (FT_Init_FreeType(&engine->ftLibrary)) return false;
    engine->atlas.init();

    const char* fontPaths[] = {
            "/system/fonts/Roboto-Regular.ttf",
            "/system/fonts/NotoSansCJK-Regular.ttc",
            "/system/fonts/NotoSansJP-Regular.otf",
            "/system/fonts/NotoSansSymbols-Regular-Subsetted.ttf",
            "/system/fonts/NotoSansSymbols-Regular.ttf",
            "/system/fonts/DroidSansFallback.ttf"
    };

    for (const char* path : fontPaths) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (file.is_open()) {
            size_t size = file.tellg();
            std::vector<uint8_t> buffer(size);
            file.seekg(0, std::ios::beg);
            file.read((char*)buffer.data(), size);

            engine->fallbackFontData.push_back(std::move(buffer));
            FT_Face face;
            if (FT_New_Memory_Face(engine->ftLibrary, engine->fallbackFontData.back().data(), size, 0, &face) == 0) {
                FT_Set_Pixel_Sizes(face, 0, 48);
                engine->fallbackFaces.push_back(face);
                engine->fallbackHbFonts.push_back(hb_ft_font_create(face, nullptr));
                engine->fallbackFontScales.push_back(1.0f);
            } else {
                engine->fallbackFontData.pop_back();
            }
        }
    }
    std::vector<uint8_t> emojiBuffer = loadAsset(engine, "fonts/NotoColorEmoji.ttf");
    if (!emojiBuffer.empty()) {
        engine->fallbackFontData.push_back(std::move(emojiBuffer));
        FT_Face face = nullptr;
        if (FT_New_Memory_Face(engine->ftLibrary, engine->fallbackFontData.back().data(), engine->fallbackFontData.back().size(), 0, &face) == 0) {
            float emScale = 1.0f; // ★追加
            if (FT_HAS_FIXED_SIZES(face)) {
                FT_Select_Size(face, 0);
                // ★追加: 絵文字の固定サイズ(例:109px)を48ptに縮小するスケールを計算
                if (face->size->metrics.y_ppem > 0) {
                    emScale = 48.0f / (float)face->size->metrics.y_ppem;
                }
            } else {
                FT_Set_Pixel_Sizes(face, 0, 48);
            }
            engine->fallbackFaces.push_back(face);
            if (face != nullptr) {
                engine->fallbackHbFonts.push_back(hb_ft_font_create(face, nullptr));
                engine->fallbackFontScales.push_back(emScale); // ★追加
            }
        } else {
            engine->fallbackFontData.pop_back();
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
    engine->maxLineWidth = engine->gutterWidth;

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

            // ★修正: 見つからなかった場合、または過去に戻ってしまった場合は終了
            if (found == std::string::npos || found < currentSearchPos) break;

            searchMatches.push_back({found, found + matchLen});

            // ★修正: マッチ長が0の場合は無限ループを防ぐため1文字進める
            if (matchLen == 0) {
                currentSearchPos = found + 1;
            } else {
                currentSearchPos = found + matchLen;
            }
        }
    }

    float winH = 5000.0f;
    float winW = 1080.0f; // ★追加
    if (engine->app->window != nullptr) {
        winH = (float)ANativeWindow_getHeight(engine->app->window);
        winW = (float)ANativeWindow_getWidth(engine->app->window); // ★追加
    }

    for (int lineIdx = 0; lineIdx < engine->lineStarts.size(); ++lineIdx) {
        float lineY = engine->topMargin + baselineOffset - engine->scrollY + lineIdx * engine->lineHeight;
        if (lineY < -engine->lineHeight || lineY > winH + engine->lineHeight) continue;

        if (lineIdx >= engine->lineCaches.size()) {
            break; // キャッシュ配列を超えたら描画ループを安全に抜ける
        }

        ensureLineShaped(engine, lineIdx);
        float x = engine->gutterWidth - engine->scrollX;
        size_t lineStart = engine->lineStarts[lineIdx];
        bool hasZeroLengthMatchAtLineStart = false;
        float lineStartX = x;
        for (const auto& match : searchMatches) {
            if (match.first == match.second && match.first == lineStart) {
                hasZeroLengthMatchAtLineStart = true;
                break;
            }
        }
        if (hasZeroLengthMatchAtLineStart) {
            float bgY = lineY - engine->lineHeight * 0.8f;
            addRect(bgVertices, lineStartX, bgY, cursorWidth, engine->lineHeight, 1.0f, 1.0f, 0.0f, 0.8f);
        }
        size_t lineEndForSearch = (lineIdx + 1 < engine->lineStarts.size()) ? engine->lineStarts[lineIdx + 1] : engine->pt.length();
        if (lineEndForSearch > lineStart) {
            char lastChar = engine->pt.charAt(lineEndForSearch - 1);
            if (lastChar == '\n' || lastChar == '\r') {
                size_t newlinePos = lineEndForSearch - 1;
                if (lastChar == '\n' && newlinePos > lineStart && engine->pt.charAt(newlinePos - 1) == '\r') {
                    newlinePos--; // CRLFの場合は \r の位置を起点とする
                }

                bool isNewlineMatched = false;
                for (const auto& match : searchMatches) {
                    if (match.first <= newlinePos && match.second > newlinePos) {
                        isNewlineMatched = true;
                        break;
                    }
                }
                if (isNewlineMatched) {
                    // 改行文字のハイライトとして、行の最後に charWidth 分の黄色い矩形を描画
                    float eolX = getXFromPos(engine, newlinePos) - engine->scrollX;
                    float bgY = lineY - engine->lineHeight * 0.8f;
                    addRect(bgVertices, eolX, bgY, engine->charWidth, engine->lineHeight, 1.0f, 1.0f, 0.0f, 0.4f);
                }
            }
        }
        for (const auto& sg : engine->lineCaches[lineIdx].glyphs) {
            uint64_t key = ((uint64_t)sg.fontIndex << 32) | sg.glyphIndex;
            if (engine->atlas.glyphs.count(key) == 0) {
                engine->atlas.loadGlyph(engine, sg.fontIndex, sg.glyphIndex);
            }

            bool isSelected = false;
            if (!engine->cursors.empty() && engine->cursors.back().hasSelection()) {
                size_t selStart = engine->cursors.back().start();
                size_t selEnd = engine->cursors.back().end();
                if (sg.cluster >= selStart && sg.cluster < selEnd) isSelected = true;
            }

            bool isSearchResult = false;
            bool isZeroLengthMatchAfter = false; // ★追加: 文字の後ろに付く長さ0マッチ用

            for (const auto& match : searchMatches) {
                // 通常の文字をまたぐハイライト
                if (match.first < match.second && sg.cluster >= match.first && sg.cluster < match.second) {
                    isSearchResult = true;
                    break;
                }
                // ★追加: 文字の直後（次のクラスター位置）が長さ0マッチの場合
                if (match.first == match.second && match.first == (sg.cluster + 1)) {
                    isZeroLengthMatchAfter = true;
                }
            }

            if (sg.isIME) {
                float bgY = lineY - engine->lineHeight * 0.8f;
                addRect(bgVertices, x, bgY, sg.xAdvance * scale, engine->lineHeight, 0.2f, 0.6f, 1.0f, 0.3f);
                float underY = lineY + engine->lineHeight * 0.1f;
                addRect(lineVertices, x, underY, sg.xAdvance * scale, 2.0f, textR, textG, textB, 1.0f);
            } else if (isSelected) {
                float bgY = lineY - engine->lineHeight * 0.8f;
                addRect(bgVertices, x, bgY, sg.xAdvance * scale, engine->lineHeight, engine->selColor[0], engine->selColor[1], engine->selColor[2], engine->selColor[3]);
            } else if (isSearchResult) {
                float bgY = lineY - engine->lineHeight * 0.8f;
                addRect(bgVertices, x, bgY, sg.xAdvance * scale, engine->lineHeight, 1.0f, 1.0f, 0.0f, 0.4f);
            }

            if (engine->atlas.glyphs.count(key) > 0) {
                GlyphInfo& info = engine->atlas.glyphs[key];
                float isColorFlag = info.isColor ? 1.0f : 0.0f;
                float xpos = x + sg.xOffset * scale + info.bearingX * scale;
                float ypos = lineY - sg.yOffset * scale - info.bearingY * scale;
                float w = info.width * scale;
                float h = info.height * scale;

                charVertices.push_back({{xpos,     ypos    }, {info.u0, info.v0}, isColorFlag, {textR, textG, textB, textA}});
                charVertices.push_back({{xpos,     ypos + h}, {info.u0, info.v1}, isColorFlag, {textR, textG, textB, textA}});
                charVertices.push_back({{xpos + w, ypos    }, {info.u1, info.v0}, isColorFlag, {textR, textG, textB, textA}});
                charVertices.push_back({{xpos + w, ypos    }, {info.u1, info.v0}, isColorFlag, {textR, textG, textB, textA}});
                charVertices.push_back({{xpos,     ypos + h}, {info.u0, info.v1}, isColorFlag, {textR, textG, textB, textA}});
                charVertices.push_back({{xpos + w, ypos + h}, {info.u1, info.v1}, isColorFlag, {textR, textG, textB, textA}});
            }

            x += sg.xAdvance * scale;

            // ★追加: 文字の直後にある長さ0マッチのハイライトを描画（行末の $ など）
            if (isZeroLengthMatchAfter) {
                float bgY = lineY - engine->lineHeight * 0.8f;
                addRect(bgVertices, x, bgY, cursorWidth, engine->lineHeight, 1.0f, 1.0f, 0.0f, 0.8f);
            }

            float absoluteX = x + engine->scrollX;
            if (absoluteX > engine->maxLineWidth) engine->maxLineWidth = absoluteX;
        }
        size_t lineEnd = (lineIdx + 1 < engine->lineStarts.size()) ? engine->lineStarts[lineIdx + 1] : engine->pt.length();
        if (lineEnd > lineStart) {
            char lastChar = engine->pt.charAt(lineEnd - 1);
            if (lastChar == '\n' || lastChar == '\r') {
                uint64_t NEWLINE_KEY = 0xFFFFFFFFFFFFFFFF;
                if (engine->atlas.glyphs.count(NEWLINE_KEY) > 0) {
                    GlyphInfo& info = engine->atlas.glyphs[NEWLINE_KEY];
                    // ★ここが常に textColor ベースになっていればOK
                    float nlR = engine->textColor[0];
                    float nlG = engine->textColor[1];
                    float nlB = engine->textColor[2];
                    float nlA = engine->textColor[3] * 0.3f;
                    float targetSize = engine->charWidth * 1.4f;
                    float iconScale = targetSize / info.width;
                    float w = info.width * iconScale;
                    float h = info.height * iconScale;
                    float xpos = x + engine->charWidth * 0.3f;
                    float rowCenterY = engine->topMargin - engine->scrollY + lineIdx * engine->lineHeight + engine->lineHeight * 0.5f;
                    float ypos = rowCenterY - h * 0.5f;
                    charVertices.push_back({{xpos,     ypos    }, {info.u0, info.v0}, 0.0f, {nlR, nlG, nlB, nlA}});
                    charVertices.push_back({{xpos,     ypos + h}, {info.u0, info.v1}, 0.0f, {nlR, nlG, nlB, nlA}});
                    charVertices.push_back({{xpos + w, ypos    }, {info.u1, info.v0}, 0.0f, {nlR, nlG, nlB, nlA}});
                    charVertices.push_back({{xpos + w, ypos    }, {info.u1, info.v0}, 0.0f, {nlR, nlG, nlB, nlA}});
                    charVertices.push_back({{xpos,     ypos + h}, {info.u0, info.v1}, 0.0f, {nlR, nlG, nlB, nlA}});
                    charVertices.push_back({{xpos + w, ypos + h}, {info.u1, info.v1}, 0.0f, {nlR, nlG, nlB, nlA}});
                }
            }
        }
        for (const auto& c : engine->cursors) {
            size_t vPos = c.head;
            if (vPos >= lineStart && (lineIdx + 1 >= engine->lineStarts.size() || vPos < engine->lineStarts[lineIdx + 1])) {
                float cx = getXFromPos(engine, vPos);
                float curY = lineY - engine->lineHeight * 0.8f;
                addRect(cursorVertices, cx - engine->scrollX, curY, cursorWidth, engine->lineHeight,
                        engine->caretColor[0], engine->caretColor[1], engine->caretColor[2], engine->caretColor[3]);
            }
        }
    }

    std::vector<Vertex> gutterBgVertices;
    std::vector<Vertex> gutterTextVertices;
    addRect(gutterBgVertices, 0.0f, 0.0f, engine->gutterWidth, winH,
            engine->gutterBgColor[0], engine->gutterBgColor[1], engine->gutterBgColor[2], engine->gutterBgColor[3]);
    float gutterTextR = engine->gutterTextColor[0], gutterTextG = engine->gutterTextColor[1], gutterTextB = engine->gutterTextColor[2];
    for (int i = 0; i < engine->lineStarts.size(); ++i) {
        float lineTop = engine->topMargin - engine->scrollY + i * engine->lineHeight;
        if (lineTop + engine->lineHeight < 0.0f || lineTop > winH) continue;
        std::string lineNumStr = std::to_string(i + 1);
        float numWidth = 0.0f;
        for(char c : lineNumStr) {
            uint32_t cp = c;
            int fontIdx = getFontIndexForChar(engine, cp, 0);
            uint32_t glyphIdx = FT_Get_Char_Index(engine->fallbackFaces[fontIdx], cp);
            uint64_t key = ((uint64_t)fontIdx << 32) | glyphIdx;
            if (engine->atlas.glyphs.count(key) == 0 && !engine->fallbackFaces.empty()) {
                engine->atlas.loadGlyph(engine, fontIdx, glyphIdx);
            }
            if (engine->atlas.glyphs.count(key) > 0) numWidth += engine->atlas.glyphs[key].advance * scale;
            else numWidth += engine->charWidth;
        }
        float rightMargin = engine->charWidth * 0.5f;
        float numX = engine->gutterWidth - rightMargin - numWidth;
        if (numX < rightMargin * 0.5f) numX = rightMargin * 0.5f;
        float lineY = lineTop + baselineOffset;
        for(char c : lineNumStr) {
            uint32_t cp = c;
            int fontIdx = getFontIndexForChar(engine, cp, 0);
            uint32_t glyphIdx = FT_Get_Char_Index(engine->fallbackFaces[fontIdx], cp);
            uint64_t key = ((uint64_t)fontIdx << 32) | glyphIdx;
            if (engine->atlas.glyphs.count(key) > 0) {
                GlyphInfo& info = engine->atlas.glyphs[key];
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

    // ---------- ★ここから追加: タイトル(ファイル名)の描画 ----------
    std::string titleStr;
    if (engine->currentFilePath.empty()) {
        titleStr = "無題";
    } else {
        size_t slashPos = engine->currentFilePath.find_last_of("/\\");
        if (slashPos != std::string::npos) {
            titleStr = engine->currentFilePath.substr(slashPos + 1);
        } else {
            titleStr = engine->currentFilePath;
        }
    }
    // 編集状態(Dirty)なら先頭にアスタリスクを付ける
    if (engine->isDirty) {
        titleStr = "*" + titleStr;
    }

    float titleFontSize = 40.0f; // 本文より少し小さめ
    float titleScale = titleFontSize / 48.0f;
    float titleWidth = 0.0f;

    // UTF-8からコードポイントを抽出
    std::vector<uint32_t> titleCodepoints;
    const char* tptr = titleStr.data();
    const char* tend = tptr + titleStr.size();
    while (tptr < tend) {
        uint32_t cp = decodeUtf8(&tptr, tend);
        if (cp > 0) titleCodepoints.push_back(cp);
    }

    // まず全体の文字列幅を計算して中央揃えの基準位置を割り出す
    for (uint32_t cp : titleCodepoints) {
        int fontIdx = getFontIndexForChar(engine, cp, 0);
        if (fontIdx >= 0) {
            uint32_t glyphIdx = FT_Get_Char_Index(engine->fallbackFaces[fontIdx], cp);
            uint64_t key = ((uint64_t)fontIdx << 32) | glyphIdx;
            if (engine->atlas.glyphs.count(key) == 0 && !engine->fallbackFaces.empty()) {
                engine->atlas.loadGlyph(engine, fontIdx, glyphIdx);
            }
            if (engine->atlas.glyphs.count(key) > 0) {
                titleWidth += engine->atlas.glyphs[key].advance * titleScale;
            } else {
                titleWidth += 24.0f * titleScale; // 未知の文字のフォールバック幅
            }
        }
    }

    // 画面中央＆ステータスバーと被らない絶妙な高さに配置
    float titleX = (winW - titleWidth) * 0.5f;
    float titleBaselineY = engine->topMargin - 20.0f;

    // 実際に文字を描画（頂点を追加）
    for (uint32_t cp : titleCodepoints) {
        int fontIdx = getFontIndexForChar(engine, cp, 0);
        if (fontIdx >= 0) {
            uint32_t glyphIdx = FT_Get_Char_Index(engine->fallbackFaces[fontIdx], cp);
            uint64_t key = ((uint64_t)fontIdx << 32) | glyphIdx;
            if (engine->atlas.glyphs.count(key) > 0) {
                GlyphInfo& info = engine->atlas.glyphs[key];
                float isColorFlag = info.isColor ? 1.0f : 0.0f;
                float xpos = titleX + info.bearingX * titleScale;
                float ypos = titleBaselineY - info.bearingY * titleScale;
                float w = info.width * titleScale;
                float h = info.height * titleScale;

                // 本文より少しだけ透明度を下げて(0.8)控えめに表示
                float tr = engine->textColor[0], tg = engine->textColor[1], tb = engine->textColor[2];
                float ta = engine->textColor[3] * 0.8f;

                vertices.push_back({{xpos,     ypos    }, {info.u0, info.v0}, isColorFlag, {tr, tg, tb, ta}});
                vertices.push_back({{xpos,     ypos + h}, {info.u0, info.v1}, isColorFlag, {tr, tg, tb, ta}});
                vertices.push_back({{xpos + w, ypos    }, {info.u1, info.v0}, isColorFlag, {tr, tg, tb, ta}});
                vertices.push_back({{xpos + w, ypos    }, {info.u1, info.v0}, isColorFlag, {tr, tg, tb, ta}});
                vertices.push_back({{xpos,     ypos + h}, {info.u0, info.v1}, isColorFlag, {tr, tg, tb, ta}});
                vertices.push_back({{xpos + w, ypos + h}, {info.u1, info.v1}, isColorFlag, {tr, tg, tb, ta}});

                titleX += info.advance * titleScale;
            } else {
                titleX += 24.0f * titleScale;
            }
        }
    }
    // ---------- ★追加ここまで ----------

    engine->vertexCount = static_cast<uint32_t>(vertices.size());
    if (engine->vertexCount == 0) return;
    if (engine->vertexCount > 1000000) {
        engine->vertexCount = 1000000;
    }
    void* data;
    vkMapMemory(engine->device, engine->vertexBufferMemory, 0, sizeof(Vertex) * engine->vertexCount, 0, &data);
    memcpy(data, vertices.data(), sizeof(Vertex) * engine->vertexCount);
    vkUnmapMemory(engine->device, engine->vertexBufferMemory);
}
void renderFrame(Engine* engine) {
    if (engine->device == VK_NULL_HANDLE || !engine->isWindowReady) return;
    // GPUの完了待機 (★ロックなしで実行するため、キーボード入力の邪魔をしない)
    vkWaitForFences(engine->device, 1, &engine->inFlightFence, VK_TRUE, UINT64_MAX);
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(engine->device, engine->swapchain, UINT64_MAX, engine->imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) { recreateSwapchain(engine); return; }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) { return; }
    vkResetFences(engine->device, 1, &engine->inFlightFence);
    vkResetCommandBuffer(engine->commandBuffers[imageIndex], 0);
    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(engine->commandBuffers[imageIndex], &beginInfo);
    // ==========================================================
    // ★追加: メモリにアクセスするCPU処理の区間だけをロックする
    {
        std::lock_guard<std::mutex> lock(g_imeMutex);
        // テキストデータの読み取りと頂点バッファの構築
        updateTextVertices(engine);
        // アトラス（フォント画像）の更新
        if (engine->atlas.isDirty) {
            vkDeviceWaitIdle(engine->device);
            uint32_t dy = engine->atlas.dirtyMinY;
            uint32_t dh = engine->atlas.dirtyMaxY - engine->atlas.dirtyMinY;
            uint32_t dw = engine->atlas.width;
            if (dh > 0) {
                void* mapData = nullptr;
                if (vkMapMemory(engine->device, engine->atlasStagingMemory, 0, engine->atlas.width * engine->atlas.height * 4, 0, &mapData) == VK_SUCCESS) {
                    uint8_t* dst = (uint8_t*)mapData;
                    const uint8_t* src = engine->atlas.pixels.data();
                    for (uint32_t row = 0; row < dh; ++row) {
                        memcpy(dst + ((dy + row) * dw) * 4,
                               src + ((dy + row) * engine->atlas.width) * 4,
                               dw * 4);
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
                    region.bufferOffset = dy * dw * 4;
                    region.bufferRowLength = 0;
                    region.bufferImageHeight = 0;
                    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    region.imageSubresource.layerCount = 1;
                    region.imageOffset = {0, (int32_t)dy, 0};
                    region.imageExtent = {dw, dh, 1};
                    vkCmdCopyBufferToImage(engine->commandBuffers[imageIndex], engine->atlasStagingBuffer, engine->fontImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
                    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                    vkCmdPipelineBarrier(engine->commandBuffers[imageIndex], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
                }
            }
            engine->atlas.isDirty = false;
        }
    }
    // ★追加: ここでロック解除！
    // ==========================================================
    // レンダリングパスの開始 (★ロックなしでGPUコマンドを積む)
    VkRenderPassBeginInfo rpBegin = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpBegin.renderPass = engine->renderPass;
    rpBegin.framebuffer = engine->framebuffers[imageIndex];
    rpBegin.renderArea.extent = engine->swapchainExtent;
    VkClearValue clearColor = {{{engine->bgColor[0], engine->bgColor[1], engine->bgColor[2], engine->bgColor[3]}}};
    rpBegin.clearValueCount = 1; rpBegin.pClearValues = &clearColor;
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
        pc.color[0] = engine->textColor[0]; pc.color[1] = engine->textColor[1]; pc.color[2] = engine->textColor[2]; pc.color[3] = engine->textColor[3];
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
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR) { recreateSwapchain(engine); }
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
    std::lock_guard<std::mutex> lock(g_imeMutex);
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
        int32_t action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
        size_t pointerCount = AMotionEvent_getPointerCount(event);
        if (pointerCount >= 2) {
            float x0 = AMotionEvent_getX(event, 0); float y0 = AMotionEvent_getY(event, 0);
            float x1 = AMotionEvent_getX(event, 1); float y1 = AMotionEvent_getY(event, 1);
            float dx = x1 - x0; float dy = y1 - y0;
            float distance = std::sqrt(dx * dx + dy * dy);
            if (action == AMOTION_EVENT_ACTION_POINTER_DOWN) {
                engine->isPinching = true; engine->lastPinchDistance = distance; engine->isDragging = false;
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
            if (action == AMOTION_EVENT_ACTION_UP) { engine->isPinching = false; engine->isDragging = false; }
            engine->lastTouchX = AMotionEvent_getX(event, 0); engine->lastTouchY = AMotionEvent_getY(event, 0);
            return 1;
        }
        float x = AMotionEvent_getX(event, 0);
        float y = AMotionEvent_getY(event, 0);
        if (action == AMOTION_EVENT_ACTION_DOWN) {
            int64_t now = getCurrentTimeMs();
            if (now - engine->lastClickTime < 400 && std::abs(x - engine->lastClickX) < 30.0f && std::abs(y - engine->lastClickY) < 30.0f) {
                engine->clickCount++;
            } else { engine->clickCount = 1; }
            engine->lastClickTime = now; engine->lastClickX = x; engine->lastClickY = y;
            engine->isDragging = false; engine->lastTouchX = x; engine->lastTouchY = y;
            engine->isFlinging = false; engine->velocityX = 0.0f; engine->velocityY = 0.0f; engine->lastMoveTime = now;
        } else if (action == AMOTION_EVENT_ACTION_MOVE) {
            int64_t now = getCurrentTimeMs();
            float dx = x - engine->lastTouchX; float dy = y - engine->lastTouchY;
            int64_t dt = now - engine->lastMoveTime;
            if (dt > 0) {
                float instVelX = dx / (float)dt; float instVelY = dy / (float)dt;
                engine->velocityX = engine->velocityX * 0.4f + instVelX * 0.6f;
                engine->velocityY = engine->velocityY * 0.4f + instVelY * 0.6f;
            }
            engine->lastMoveTime = now;
            if (!engine->isDragging && (std::abs(x - engine->lastClickX) > 10.0f || std::abs(y - engine->lastClickY) > 10.0f)) {
                engine->isDragging = true; engine->lastTouchX = x; engine->lastTouchY = y; dx = 0.0f; dy = 0.0f;
            }
            if (engine->isDragging) {
                engine->scrollX -= dx; engine->scrollY -= dy;
                if (engine->scrollX < 0.0f) engine->scrollX = 0.0f;
                if (engine->scrollY < 0.0f) engine->scrollY = 0.0f;
                if (app->window != nullptr) {
                    float winW = (float)ANativeWindow_getWidth(app->window); float winH = (float)ANativeWindow_getHeight(app->window);
                    float visibleH = winH - engine->bottomInset;
                    float contentH = engine->lineStarts.size() * engine->lineHeight + engine->topMargin + engine->lineHeight;
                    float maxScrollY = std::max(0.0f, contentH - visibleH);
                    float maxScrollX = std::max(0.0f, engine->maxLineWidth - winW + engine->charWidth * 2.0f);
                    if (engine->scrollY > maxScrollY) engine->scrollY = maxScrollY;
                    if (engine->scrollX > maxScrollX) engine->scrollX = maxScrollX;
                }
                engine->lastTouchX = x; engine->lastTouchY = y;
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
                if (speed > 0.5f) { engine->isFlinging = true; engine->lastMoveTime = getCurrentTimeMs(); }
            }
            engine->isDragging = false;
        }
        return 1;
    }
    return 0;
}
static void onAppCmd(struct android_app* app, int32_t cmd) {
    Engine* engine = (Engine*)app->userData;
    std::lock_guard<std::mutex> lock(g_imeMutex);
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            if (app->window != nullptr) {
                LOGI("ウィンドウ生成。Vulkan初期化開始。");
                updateThemeColors(engine);
                if (initVulkan(engine)) {
                    engine->isWindowReady = true;

                    // ★修正: ロック解除等での復帰時、空になったキャッシュを再構築して画面に文字を復活させる
                    rebuildLineStarts(engine);
                    updateGutterWidth(engine);

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
                    engine.scrollX -= engine.velocityX * dt; engine.scrollY -= engine.velocityY * dt;
                    float friction = std::pow(0.992f, (float)dt);
                    engine.velocityX *= friction; engine.velocityY *= friction;
                    if (std::abs(engine.velocityX) < 0.05f && std::abs(engine.velocityY) < 0.05f) { engine.isFlinging = false; }
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
                        if (!engine.cursors.empty()) {
                            int lineIdx = getLineIdx(&engine, engine.cursors.back().head);
                            if (lineIdx >= 0 && lineIdx < engine.lineCaches.size()) {
                                engine.lineCaches[lineIdx].isShaped = false;
                            }
                        }
                    } else if (ev.type == ImeEvent::FinishComposing) {
                        if (!engine.imeComp.empty()) {
                            std::string textToCommit = engine.imeComp;
                            engine.imeComp.clear();
                            insertAtCursors(&engine, textToCommit);
                        }
                    } else if (ev.type == ImeEvent::Delete) {
                        backspaceAtCursors(&engine);
                    }
                }
            } // ← ここでロックが解除され、UIスレッドが即座に解放される！
            // ★修正: 描画関数はロックの外で呼び出す
            renderFrame(&engine);
        }
    }
}