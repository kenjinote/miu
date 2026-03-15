#include <android_native_app_glue.h>
#include <android/log.h>
#include <android/configuration.h>
#include <android/asset_manager.h>
#include <jni.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <atomic>
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
#include <numeric>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include <hb-ft.h>
#define VK_USE_PLATFORM_ANDROID_KHR
#include <vulkan/vulkan.h>
#include "compact_enc_det/compact_enc_det.h"
#include "util/encodings/encodings.h"
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "miu", __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, "miu", __VA_ARGS__))
void setEngineColorFromInt(float* target, int argb) {
    target[3] = ((argb >> 24) & 0xFF) / 255.0f;
    target[0] = ((argb >> 16) & 0xFF) / 255.0f;
    target[1] = ((argb >> 8) & 0xFF) / 255.0f;
    target[2] = (argb & 0xFF) / 255.0f;
}
struct GlyphInfo {
    float u0, v0, u1, v1;
    float width, height;
    float bearingX, bearingY;
    float advance;
    bool isColor;
};
struct MappedFile {
    int fd = -1;
    size_t size = 0;
    char* ptr = nullptr;
    bool open(const char* path) {
        fd = ::open(path, O_RDONLY);
        if (fd == -1) return false;
        struct stat sb;
        if (fstat(fd, &sb) == -1) { ::close(fd); return false; }
        size = sb.st_size;
        if (size == 0) { ptr = nullptr; return true; }
        ptr = (char*)mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
        return (ptr != MAP_FAILED);
    }
    void close() {
        if (ptr && ptr != MAP_FAILED) munmap(ptr, size);
        if (fd != -1) ::close(fd);
        ptr = nullptr;
        fd = -1;
        size = 0;
    }
    ~MappedFile() { close(); }
};
struct Engine;
struct TextAtlas {
    int width = 4096;
    int height = 4096;
    std::vector<uint8_t> pixels;
    std::unordered_map<uint64_t, GlyphInfo> glyphs;
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
        int nlX = 3; int nlY = 0;
        int nlW = 48; int nlH = 48;
        float thickness = 4.5f;
        auto distToSeg = [](float px, float py, float ax, float ay, float bx, float by) {
            float l2 = (bx - ax)*(bx - ax) + (by - ay)*(by - ay);
            if (l2 == 0.0f) return std::sqrt((px - ax)*(px - ax) + (py - ay)*(py - ay));
            float t = std::max(0.0f, std::min(1.0f, ((px - ax)*(bx - ax) + (py - ay)*(by - ay)) / l2));
            float projx = ax + t * (bx - ax);
            float projy = ay + t * (by - ay);
            return std::sqrt((px - projx)*(px - projx) + (py - projy)*(py - projy));
        };
        int crlfX = nlX;             int crlfY = nlY;
        int crX   = crlfX + nlW + 1; int crY   = nlY;
        int lfX   = crX + nlW + 1;   int lfY   = nlY;
        float cy = nlH * 0.5f;
        float cx = nlW * 0.5f;
        float halfSz = nlH * 0.4f;
        float arrowSize = halfSz * 0.35f;
        float hLineRight = cx + halfSz * 0.6f;
        float hLineLeft = cx - halfSz * 0.6f;
        float vLineTop = cy - halfSz;
        float vLineBottom = cy + halfSz * 0.3f;
        float stemTop = cy - halfSz * 0.8f;
        float stemBottom = cy + halfSz * 0.8f;
        for (int y = 0; y < nlH; ++y) {
            for (int x = 0; x < nlW; ++x) {
                float px = (float)x; float py = (float)y;
                float d1_crlf = distToSeg(px, py, hLineRight, vLineTop, hLineRight, vLineBottom);
                float d2_crlf = distToSeg(px, py, hLineRight, vLineBottom, hLineLeft, vLineBottom);
                float d3_crlf = distToSeg(px, py, hLineLeft, vLineBottom, hLineLeft + arrowSize, vLineBottom - arrowSize);
                float d4_crlf = distToSeg(px, py, hLineLeft, vLineBottom, hLineLeft + arrowSize, vLineBottom + arrowSize);
                float d_crlf = std::min({d1_crlf, d2_crlf, d3_crlf, d4_crlf});
                float alpha_crlf = std::max(0.0f, std::min(1.0f, (thickness / 2.0f) - d_crlf + 0.5f));
                uint8_t val_crlf = (uint8_t)(alpha_crlf * 255.0f);
                int idx_crlf = ((crlfY + y) * width + (crlfX + x)) * 4;
                pixels[idx_crlf + 0] = val_crlf; pixels[idx_crlf + 1] = val_crlf; pixels[idx_crlf + 2] = val_crlf; pixels[idx_crlf + 3] = val_crlf;
                float d1_cr = distToSeg(px, py, hLineRight, cy, hLineLeft, cy);
                float d2_cr = distToSeg(px, py, hLineLeft, cy, hLineLeft + arrowSize, cy - arrowSize);
                float d3_cr = distToSeg(px, py, hLineLeft, cy, hLineLeft + arrowSize, cy + arrowSize);
                float d_cr = std::min({d1_cr, d2_cr, d3_cr});
                float alpha_cr = std::max(0.0f, std::min(1.0f, (thickness / 2.0f) - d_cr + 0.5f));
                uint8_t val_cr = (uint8_t)(alpha_cr * 255.0f);
                int idx_cr = ((crY + y) * width + (crX + x)) * 4;
                pixels[idx_cr + 0] = val_cr; pixels[idx_cr + 1] = val_cr; pixels[idx_cr + 2] = val_cr; pixels[idx_cr + 3] = val_cr;
                float d1_lf = distToSeg(px, py, cx, stemTop, cx, stemBottom);
                float d2_lf = distToSeg(px, py, cx, stemBottom, cx - arrowSize, stemBottom - arrowSize);
                float d3_lf = distToSeg(px, py, cx, stemBottom, cx + arrowSize, stemBottom - arrowSize);
                float d_lf = std::min({d1_lf, d2_lf, d3_lf});
                float alpha_lf = std::max(0.0f, std::min(1.0f, (thickness / 2.0f) - d_lf + 0.5f));
                uint8_t val_lf = (uint8_t)(alpha_lf * 255.0f);
                int idx_lf = ((lfY + y) * width + (lfX + x)) * 4;
                pixels[idx_lf + 0] = val_lf; pixels[idx_lf + 1] = val_lf; pixels[idx_lf + 2] = val_lf; pixels[idx_lf + 3] = val_lf;
            }
        }
        auto createGlyphInfo = [&](int gx, int gy) {
            GlyphInfo info;
            info.width = (float)nlW;
            info.height = (float)nlH;
            info.bearingX = 0.0f;
            info.bearingY = 24.0f;
            info.advance = 32.0f;
            info.u0 = (float)gx / width; info.v0 = (float)gy / height;
            info.u1 = (float)(gx + nlW) / width; info.v1 = (float)(gy + nlH) / height;
            info.isColor = false;
            return info;
        };
        glyphs[0xFFFFFFFFFFFFFFFF] = createGlyphInfo(crlfX, crlfY);
        glyphs[0xFFFFFFFFFFFFFFFE] = createGlyphInfo(crX, crY);
        glyphs[0xFFFFFFFFFFFFFFFD] = createGlyphInfo(lfX, lfY);
        currentX = lfX + nlW + 1;
        currentY = 0;
        maxRowHeight = nlH;
        isDirty = true;
        dirtyMinX = 0; dirtyMinY = 0;
        dirtyMaxX = currentX; dirtyMaxY = maxRowHeight;
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
        while (idx < pieces.size() && cur + pieces[idx].len < pos) { cur += pieces[idx].len; ++idx; }
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
        if (idx > 0 && !pieces[idx - 1].isOriginal && pieces[idx - 1].start + pieces[idx - 1].len == addStart) {
            pieces[idx - 1].len += s.size();
        } else {
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
struct BlurPushConstants {
    float topMargin;
    float screenHeight;
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
enum MiuEncoding {
    ENC_UTF8_NOBOM = 0,
    ENC_UTF8_BOM,
    ENC_UTF16LE,
    ENC_UTF16BE,
    ENC_LOCAL
};
struct DetectResult {
    MiuEncoding type;
    std::string charsetName;
};
struct Engine {
    struct android_app* app;
    bool isWindowReady = false;
    PieceTable pt;
    UndoManager undo;
    MappedFile fileMap;
    std::string convertedFileBuffer;
    std::vector<Cursor> cursors;
    std::vector<size_t> lineStarts;
    std::mutex asyncParseMutex;
    std::vector<std::vector<size_t>> pendingLineChunks;
    std::atomic<bool> cancelParse{false};
    std::atomic<bool> isAsyncParsing{false};
    std::thread asyncParseThread;
    std::string imeComp;
    std::string currentFilePath;
    std::string displayFileName;
    bool isDirty = false;
    std::string searchQuery;
    std::string replaceQuery;
    bool searchMatchCase = false;
    bool searchWholeWord = false;
    bool searchRegex = false;
    bool isReplaceMode = false;
    std::string newlineStr = "\n";
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
    bool isSelecting = false;
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
    float autoHlColor[4] = {0.85f, 0.85f, 0.85f, 0.5f};
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
    VkImage offscreenImage = VK_NULL_HANDLE;
    VkDeviceMemory offscreenImageMemory = VK_NULL_HANDLE;
    VkImageView offscreenImageView = VK_NULL_HANDLE;
    VkRenderPass offscreenRenderPass = VK_NULL_HANDLE;
    VkFramebuffer offscreenFramebuffer = VK_NULL_HANDLE;
    VkSampler offscreenSampler = VK_NULL_HANDLE;
    VkDescriptorSetLayout blurDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool blurDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet blurDescriptorSet = VK_NULL_HANDLE;
    VkPipelineLayout blurPipelineLayout = VK_NULL_HANDLE;
    VkPipeline blurPipeline = VK_NULL_HANDLE;
    MiuEncoding currentEncoding = ENC_UTF8_NOBOM;
    std::string currentCharset = "UTF-8";
    std::string lastTitleStr = "<UNINITIALIZED>";
};
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
    if (engine->fallbackFaces.empty()) return -1;
    if (cp == 0x200D || (cp >= 0xFE00 && cp <= 0xFE0F)) return prevFontIndex;
    for (int i = 0; i < engine->fallbackFaces.size(); ++i) {
        if (FT_Get_Char_Index(engine->fallbackFaces[i], cp) != 0) return i;
    }
    return 0;
}
void ensureLineShaped(Engine* engine, int lineIdx);
void rebuildLineStarts(Engine* engine);
void ensureCaretVisible(Engine* engine);
float getXFromPos(Engine* engine, size_t pos);
void updateTitleBarIfNeeded(Engine* engine);
void stopAsyncParsing(Engine* engine);
void startAsyncLineParsing(Engine* engine, const char* buffer, size_t size);
void pollAsyncParsing(Engine* engine);
void updateDirtyFlag(Engine* engine) {
    bool wasDirty = engine->isDirty;
    engine->isDirty = !engine->undo.undoStack.empty();
    if (wasDirty != engine->isDirty) {
        updateTitleBarIfNeeded(engine);
    }
}
void performNewDocument(Engine* engine) {
    if (!engine) return;
    stopAsyncParsing(engine);
    engine->fileMap.close();
    engine->convertedFileBuffer.clear();
    engine->pt.initEmpty();
    engine->currentFilePath.clear();
    engine->displayFileName.clear();
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
    engine->newlineStr = "\n";
    engine->currentEncoding = ENC_UTF8_NOBOM;
    engine->currentCharset = "UTF-8";
    updateTitleBarIfNeeded(engine);
}
std::string UnescapeString(const std::string& s, const std::string& newline) {
    std::string out; out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            switch (s[i + 1]) {
                case 'n': out += newline; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case '\\': out += '\\'; break;
                default: out += s[i]; out += s[i + 1]; break;
            } i++;
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
                        if (searchStartIdx > 0 && fullText[searchStartIdx - 1] == '\r') {
                            searchStartIdx--;
                        }
                    } else if (prevChar == '\r') {
                        searchStartIdx--;
                    }
                }
                std::string::const_iterator searchStartIter = fullText.begin() + searchStartIdx;
                while (std::regex_search(searchStartIter, fullText.cend(), m, re, searchFlags)) {
                    size_t currentMatchPos = searchStartIdx + m.position();
                    size_t currentMatchLen = m.length();
                    size_t anchorLen = 0;
                    if (startsWithCaret && m.size() > 1 && m[1].matched) {
                        anchorLen = m.length(1);
                    }
                    size_t contentPos = currentMatchPos + anchorLen;
                    size_t contentLen = currentMatchLen - anchorLen;
                    bool isValid = false;
                    if (contentPos >= startPos) isValid = true;
                    if (isValid && endsWithDollar) {
                        bool isAtLineEnd = false;
                        size_t checkPos = contentPos + contentLen;
                        if (checkPos >= fullText.size()) {
                            isAtLineEnd = true;
                        } else {
                            char c = fullText[checkPos];
                            if (c == '\r' || c == '\n') {
                                isAtLineEnd = true;
                                if (contentLen == 0 && c == '\n' && checkPos > 0 && fullText[checkPos - 1] == '\r') {
                                    isAtLineEnd = false;
                                }
                            }
                        }
                        if (!isAtLineEnd) isValid = false;
                    }
                    if (isValid) {
                        foundPos = contentPos;
                        foundLen = contentLen;
                        if (startsWithCaret && endsWithDollar && foundLen == 0 && foundPos == fullText.size() && !fullText.empty()) {
                            bool isAfterNewline = false;
                            if (foundPos > 0) {
                                char c = fullText[foundPos - 1];
                                if (c == '\r' || c == '\n') isAfterNewline = true;
                            }
                            if (!isAfterNewline) isValid = false;
                        }
                        if (isValid) break;
                    }
                    size_t step = currentMatchLen;
                    if (step == 0) {
                        bool isBolCaret = (startsWithCaret && !(searchFlags & std::regex_constants::match_not_bol));
                        if (isBolCaret) step = 0;
                        else step = 1;
                    }
                    if (step == 0 && (searchFlags & std::regex_constants::match_not_bol)) step = 1;
                    size_t dist = std::distance(searchStartIter, fullText.cend());
                    if ((size_t)m.position() + step > dist) break;
                    size_t advance = m.position() + step;
                    std::advance(searchStartIter, advance);
                    searchStartIdx += advance;
                    searchFlags |= std::regex_constants::match_not_bol;
                }
                if (foundPos == std::string::npos && startPos > 0) {
                    if (std::regex_search(fullText.cbegin(), fullText.cend(), m, re)) {
                        size_t mPos = m.position();
                        size_t mLen = m.length();
                        size_t aLen = 0;
                        if (startsWithCaret && m.size() > 1 && m[1].matched) {
                            aLen = m.length(1);
                        }
                        bool split = false;
                        if (startsWithCaret && aLen == 1 && mPos < fullText.size() && fullText[mPos] == '\r') {
                            if (mPos + 1 < fullText.size() && fullText[mPos + 1] == '\n') split = true;
                        }
                        if (!split) {
                            size_t cPos = mPos + aLen;
                            size_t cLen = mLen - aLen;
                            bool valid = true;
                            if (endsWithDollar) {
                                size_t check = cPos + cLen;
                                if (check < fullText.size()) {
                                    char c = fullText[check];
                                    if (c == '\r' || c == '\n') {
                                        if (cLen == 0 && c == '\n' && check > 0 && fullText[check - 1] == '\r') {
                                            valid = false;
                                        }
                                    } else {
                                        valid = false;
                                    }
                                }
                            }
                            if (startsWithCaret && endsWithDollar && cLen == 0 && cPos == fullText.size() && !fullText.empty()) {
                                bool isAfterNL = false;
                                if (cPos > 0) {
                                    char c = fullText[cPos - 1];
                                    if (c == '\r' || c == '\n') isAfterNL = true;
                                }
                                if (!isAfterNL) valid = false;
                            }
                            if (valid) {
                                foundPos = cPos;
                                foundLen = cLen;
                            }
                        }
                    }
                }
            } else {
                auto words_begin = std::sregex_iterator(fullText.begin(), fullText.end(), re);
                auto words_end = std::sregex_iterator();
                size_t bestPos = std::string::npos;
                size_t bestLen = 0;
                size_t limit = (startPos == 0) ? len : startPos;
                for (auto i = words_begin; i != words_end; ++i) {
                    size_t mPos = i->position();
                    size_t mLen = i->length();
                    size_t aLen = 0;
                    if (startsWithCaret && i->size() > 1 && (*i)[1].matched) {
                        aLen = i->length(1);
                    }
                    if (startsWithCaret && aLen == 1 && mPos < fullText.size() && fullText[mPos] == '\r') {
                        if (mPos + 1 < fullText.size() && fullText[mPos + 1] == '\n') continue;
                    }
                    size_t contentStart = mPos + aLen;
                    size_t contentLen = mLen - aLen;
                    if (endsWithDollar) {
                        bool isAtEnd = (contentStart + contentLen >= fullText.size());
                        if (!isAtEnd) {
                            char c = fullText[contentStart + contentLen];
                            if (c == '\r' || c == '\n') {
                                if (contentLen == 0 && c == '\n' && contentStart + contentLen > 0 && fullText[contentStart + contentLen - 1] == '\r') {
                                    continue;
                                }
                            } else {
                                continue;
                            }
                        }
                    }
                    if (startsWithCaret && endsWithDollar && contentLen == 0 && contentStart == fullText.size() && !fullText.empty()) {
                        bool isAfterNL = false;
                        if (contentStart > 0) {
                            char c = fullText[contentStart - 1];
                            if (c == '\r' || c == '\n') isAfterNL = true;
                        }
                        if (!isAfterNL) continue;
                    }
                    if (contentStart < limit) {
                        bestPos = contentStart;
                        bestLen = contentLen;
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
    std::string replacement = UnescapeString(engine->replaceQuery, engine->newlineStr);
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
    bool wasDirty = engine->isDirty;
    engine->isDirty = true;
    rebuildLineStarts(engine);
    ensureCaretVisible(engine);
    if (!wasDirty) updateTitleBarIfNeeded(engine);
    findNextCommand(engine, true);
}
void replaceAllCommand(Engine* engine) {
    if (engine->searchQuery.empty()) return;
    struct Match { size_t start; size_t len; std::string replacementText; };
    std::vector<Match> matches;
    size_t docLen = engine->pt.length();
    if (engine->searchRegex) {
        std::string actualQuery = preprocessRegexQuery(engine->searchQuery);
        try {
            std::regex_constants::syntax_option_type rxFlags = std::regex_constants::ECMAScript;
            if (!engine->searchMatchCase) rxFlags |= std::regex_constants::icase;
            std::regex re(actualQuery, rxFlags);
            std::string fullText = engine->pt.getRange(0, docLen);
            std::string rawFmt = UnescapeString(engine->replaceQuery, engine->newlineStr);
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
            bool startsWithCaret = (!engine->searchQuery.empty() && engine->searchQuery[0] == '^');
            bool endsWithDollar = (!engine->searchQuery.empty() && engine->searchQuery.back() == '$');
            auto searchStart = fullText.cbegin();
            std::smatch m;
            size_t currentOffset = 0;
            std::regex_constants::match_flag_type flags = std::regex_constants::match_default;
            while (std::regex_search(searchStart, fullText.cend(), m, re, flags)) {
                size_t matchPos = currentOffset + m.position();
                size_t matchLen = m.length();
                size_t anchorLen = 0;
                if (startsWithCaret && m.size() > 1 && m[1].matched) {
                    anchorLen = m.length(1);
                }
                size_t contentPos = matchPos + anchorLen;
                size_t contentLen = matchLen - anchorLen;
                bool shouldHighlight = true;
                if (endsWithDollar) {
                    bool isAtLineEnd = false;
                    if (contentPos + contentLen >= fullText.size()) {
                        isAtLineEnd = true;
                    } else {
                        char c = fullText[contentPos + contentLen];
                        if (c == '\r' || c == '\n') {
                            isAtLineEnd = true;
                            if (contentLen == 0 && c == '\n' && contentPos + contentLen > 0 && fullText[contentPos + contentLen - 1] == '\r') {
                                isAtLineEnd = false;
                            }
                        }
                    }
                    if (!isAtLineEnd) shouldHighlight = false;
                    if (shouldHighlight && contentPos > 0 && contentPos < fullText.size()) {
                        if (fullText[contentPos] == '\n' && fullText[contentPos - 1] == '\r') {
                            shouldHighlight = false;
                        }
                    }
                }
                if (shouldHighlight && startsWithCaret && endsWithDollar && contentLen == 0) {
                    size_t globalPos = contentPos;
                    if (globalPos == fullText.size() && !fullText.empty()) {
                        bool isAfterNewline = false;
                        if (globalPos > 0) {
                            char c = fullText[globalPos - 1];
                            if (c == '\r' || c == '\n') isAfterNewline = true;
                        }
                        if (!isAfterNewline) shouldHighlight = false;
                    }
                }
                if (shouldHighlight && !matches.empty()) {
                    const auto& last = matches.back();
                    if (last.start == contentPos && last.len == 0 && contentLen == 0) {
                        shouldHighlight = false;
                    }
                }
                if (shouldHighlight) {
                    std::string rText = m.format(fmt);
                    matches.push_back({ contentPos, contentLen, rText });
                }
                size_t step = matchLen;
                if (step == 0) {
                    bool isBolCaret = (startsWithCaret && !(flags & std::regex_constants::match_not_bol));
                    if (isBolCaret) step = 0;
                    else step = 1;
                }
                if (step == 0 && (flags & std::regex_constants::match_not_bol)) step = 1;
                size_t relativeAdvance = m.position() + step;
                size_t remaining = std::distance(searchStart, fullText.cend());
                if (relativeAdvance > remaining) break;
                std::advance(searchStart, relativeAdvance);
                currentOffset += relativeAdvance;
                flags |= std::regex_constants::match_not_bol;
            }
        } catch (...) { return; }
    } else {
        size_t currentPos = 0;
        while (true) {
            size_t matchLen = 0;
            size_t pos = findText(engine, currentPos, engine->searchQuery, true, engine->searchMatchCase, engine->searchWholeWord, false, &matchLen);
            if (pos == std::string::npos || pos < currentPos) break;
            std::string rText = UnescapeString(engine->replaceQuery, engine->newlineStr);
            matches.push_back({ pos, matchLen, rText });
            currentPos = pos + matchLen;
            if (currentPos > docLen) break;
        }
    }
    if (matches.empty()) return;
    EditBatch batch;
    batch.beforeCursors = engine->cursors;
    for (auto it = matches.rbegin(); it != matches.rend(); ++it) {
        size_t start = it->start;
        size_t len = it->len;
        if (start + len > engine->pt.length()) continue;
        std::string deleted = engine->pt.getRange(start, len);
        engine->pt.erase(start, len);
        batch.ops.push_back({ EditOp::Erase, start, deleted });
        engine->pt.insert(start, it->replacementText);
        batch.ops.push_back({ EditOp::Insert, start, it->replacementText });
    }
    size_t finalMatchIdx = matches.size() - 1;
    long long offsetBeforeFinal = 0;
    for (size_t i = 0; i < finalMatchIdx; ++i) {
        offsetBeforeFinal += (long long)matches[i].replacementText.size() - (long long)matches[i].len;
    }
    size_t lastReplaceStart = (size_t)((long long)matches.back().start + offsetBeforeFinal);
    size_t lastReplaceEnd = lastReplaceStart + matches.back().replacementText.size();
    engine->cursors.clear();
    engine->cursors.push_back({ lastReplaceEnd, lastReplaceStart, getXFromPos(engine, lastReplaceEnd) });
    batch.afterCursors = engine->cursors;
    engine->undo.push(batch);
    bool wasDirty = engine->isDirty;
    engine->isDirty = true;
    rebuildLineStarts(engine);
    ensureCaretVisible(engine);
    if (!wasDirty) updateTitleBarIfNeeded(engine);
}
static std::string MapCedEncodingToCharset(Encoding enc) {
    switch (enc) {
        case JAPANESE_SHIFT_JIS: return "Shift_JIS";
        case JAPANESE_EUC_JP:    return "EUC-JP";
        case CHINESE_GB:         return "GBK";
        case CHINESE_BIG5:       return "Big5";
        case KOREAN_EUC_KR:      return "EUC-KR";
        case RUSSIAN_CP1251:     return "windows-1251";
        case LATIN1:             return "ISO-8859-1";
        case ASCII_7BIT:         return "UTF-8";
        default:                 return "UTF-8";
    }
}
static bool IsValidUtf8(const char* buf, size_t len) {
    if (len == 0) return true;
    size_t check_len = (len > 4096) ? 4096 : len;
    size_t i = 0;
    while (i < check_len) {
        unsigned char c = buf[i];
        if (c <= 0x7F) i++;
        else if (c >= 0xC2 && c <= 0xDF) {
            if (i + 1 >= check_len) break;
            if ((buf[i + 1] & 0xC0) != 0x80) return false;
            i += 2;
        }
        else if (c >= 0xE0 && c <= 0xEF) {
            if (i + 2 >= check_len) break;
            if ((buf[i + 1] & 0xC0) != 0x80 || (buf[i + 2] & 0xC0) != 0x80) return false;
            i += 3;
        }
        else if (c >= 0xF0 && c <= 0xF4) {
            if (i + 3 >= check_len) break;
            if ((buf[i + 1] & 0xC0) != 0x80 || (buf[i + 2] & 0xC0) != 0x80 || (buf[i + 3] & 0xC0) != 0x80) return false;
            i += 4;
        }
        else return false;
    }
    return true;
}
static DetectResult DetectEncodingEx(const char* buf, size_t len) {
    DetectResult res = { ENC_UTF8_NOBOM, "UTF-8" };
    if (len >= 3 && (unsigned char)buf[0] == 0xEF && (unsigned char)buf[1] == 0xBB && (unsigned char)buf[2] == 0xBF) {
        res.type = ENC_UTF8_BOM; return res;
    }
    if (len >= 2) {
        if ((unsigned char)buf[0] == 0xFF && (unsigned char)buf[1] == 0xFE) { res.type = ENC_UTF16LE; res.charsetName = "UTF-16LE"; return res; }
        if ((unsigned char)buf[0] == 0xFE && (unsigned char)buf[1] == 0xFF) { res.type = ENC_UTF16BE; res.charsetName = "UTF-16BE"; return res; }
    }
    if (IsValidUtf8(buf, len)) {
        res.type = ENC_UTF8_NOBOM; return res;
    }
    int bytes_consumed = 0;
    bool is_reliable = false;
    size_t ced_len = (len > 65536) ? 65536 : len;
    Encoding ced_enc = CompactEncDet::DetectEncoding(
            buf, static_cast<int>(ced_len),
            nullptr, nullptr, nullptr,
            UNKNOWN_ENCODING,
            UNKNOWN_LANGUAGE,
            CompactEncDet::WEB_CORPUS,
            false,
            &bytes_consumed,
            &is_reliable
    );
    res.type = ENC_LOCAL;
    res.charsetName = MapCedEncodingToCharset(ced_enc);
    return res;
}
static std::string ConvertToUtf8(JNIEnv* env, const char* data, size_t len, const std::string& charsetName) {
    if (len == 0) return "";
    jbyteArray bytes = env->NewByteArray(len);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return "Error: File is too large to open (Memory Allocation Failed).";
    }
    env->SetByteArrayRegion(bytes, 0, len, (const jbyte*)data);
    jstring charset = env->NewStringUTF(charsetName.c_str());
    jclass stringClass = env->FindClass("java/lang/String");
    jmethodID ctor = env->GetMethodID(stringClass, "<init>", "([BLjava/lang/String;)V");
    jstring jstr = (jstring)env->NewObject(stringClass, ctor, bytes, charset);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        env->DeleteLocalRef(bytes);
        env->DeleteLocalRef(charset);
        env->DeleteLocalRef(stringClass);
        return "Error: File is too large to open (String Conversion Failed).";
    }
    const char* utf8Str = env->GetStringUTFChars(jstr, nullptr);
    if (!utf8Str) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return "Error: Failed to read file content.";
    }
    std::string result(utf8Str);
    env->ReleaseStringUTFChars(jstr, utf8Str);
    env->DeleteLocalRef(bytes);
    env->DeleteLocalRef(charset);
    env->DeleteLocalRef(jstr);
    env->DeleteLocalRef(stringClass);
    return result;
}
bool openDocumentFromFile(Engine* engine, const std::string& path, JNIEnv* env) {
    if (!engine) return false;
    stopAsyncParsing(engine);
    engine->fileMap.close();
    engine->convertedFileBuffer.clear();
    engine->pt.initEmpty();
    if (!engine->fileMap.open(path.c_str())) return false;
    const char* ptr = engine->fileMap.ptr;
    size_t size = engine->fileMap.size;
    if (size == 0) {
        engine->currentEncoding = ENC_UTF8_NOBOM;
        engine->currentCharset = "UTF-8";
        engine->newlineStr = "\n";
    } else {
        DetectResult encRes = DetectEncodingEx(ptr, size);
        engine->currentEncoding = encRes.type;
        engine->currentCharset = encRes.charsetName;
        if (engine->currentEncoding == ENC_UTF8_NOBOM) {
            engine->pt.origPtr = ptr;
            engine->pt.origSize = size;
            engine->pt.pieces.push_back({ true, 0, size });
        } else {
            std::string contentUtf8;
            switch (engine->currentEncoding) {
                case ENC_UTF8_BOM:
                    contentUtf8 = std::string(ptr + 3, size - 3);
                    break;
                case ENC_UTF16LE:
                case ENC_UTF16BE:
                case ENC_LOCAL:
                    contentUtf8 = ConvertToUtf8(env, ptr, size, engine->currentCharset);
                    break;
                default:
                    contentUtf8 = std::string(ptr, size);
                    break;
            }
            engine->convertedFileBuffer = std::move(contentUtf8);
            engine->pt.origPtr = engine->convertedFileBuffer.data();
            engine->pt.origSize = engine->convertedFileBuffer.size();
            engine->pt.pieces.push_back({ true, 0, engine->pt.origSize });
        }
        engine->newlineStr = "\n";
        const char* checkPtr = engine->pt.origPtr;
        size_t checkLen = std::min(engine->pt.origSize, (size_t)4096);
        for (size_t i = 0; i < checkLen; ++i) {
            if (checkPtr[i] == '\r') {
                if (i + 1 < engine->pt.origSize && checkPtr[i + 1] == '\n') engine->newlineStr = "\r\n";
                else engine->newlineStr = "\r";
                break;
            } else if (checkPtr[i] == '\n') {
                engine->newlineStr = "\n";
                break;
            }
        }
    }
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
    engine->imeComp.clear();
    updateTitleBarIfNeeded(engine);
    if (engine->pt.origPtr && engine->pt.origSize > 0) {
        startAsyncLineParsing(engine, engine->pt.origPtr, engine->pt.origSize);
    } else {
        rebuildLineStarts(engine);
    }
    return true;
}
void updateThemeColors(Engine* engine) {
    if (!engine->app || !engine->app->activity || !engine->app->activity->vm) return;
    JNIEnv* env = nullptr;
    JavaVM* vm = engine->app->activity->vm;
    bool needDetach = false;
    if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) == JNI_EDETACHED) {
        if (vm->AttachCurrentThread(&env, nullptr) != 0) return;
        needDetach = true;
    }
    int32_t uiMode = AConfiguration_getUiModeNight(engine->app->config);
    engine->isDarkMode = (uiMode == ACONFIGURATION_UI_MODE_NIGHT_YES);
    int isDarkInt = engine->isDarkMode ? 1 : 0;
    jclass activityClass = env->GetObjectClass(engine->app->activity->clazz);
    jmethodID getThemeColorMid = env->GetMethodID(activityClass, "getThemeColor", "(II)I");
    auto setEngineColor = [](float* target, int argb, float alphaOverride = -1.0f) {
        target[0] = ((argb >> 16) & 0xFF) / 255.0f;
        target[1] = ((argb >> 8) & 0xFF) / 255.0f;
        target[2] = (argb & 0xFF) / 255.0f;
        target[3] = (alphaOverride >= 0.0f) ? alphaOverride : (((argb >> 24) & 0xFF) / 255.0f);
    };
    if (getThemeColorMid) {
        int bgArgb         = env->CallIntMethod(engine->app->activity->clazz, getThemeColorMid, 0, isDarkInt);
        int textArgb       = env->CallIntMethod(engine->app->activity->clazz, getThemeColorMid, 1, isDarkInt);
        int gutterBgArgb   = env->CallIntMethod(engine->app->activity->clazz, getThemeColorMid, 2, isDarkInt);
        int gutterTextArgb = env->CallIntMethod(engine->app->activity->clazz, getThemeColorMid, 3, isDarkInt);
        int accentArgb     = env->CallIntMethod(engine->app->activity->clazz, getThemeColorMid, 4, isDarkInt);
        setEngineColor(engine->bgColor, bgArgb);
        setEngineColor(engine->textColor, textArgb);
        setEngineColor(engine->gutterBgColor, gutterBgArgb);
        setEngineColor(engine->gutterTextColor, gutterTextArgb);
        setEngineColor(engine->selColor, accentArgb, 0.45f);
        setEngineColor(engine->caretColor, accentArgb, 1.0f);
        setEngineColor(engine->autoHlColor, accentArgb, 0.20f);
    } else {
        if (engine->isDarkMode) {
            engine->bgColor[0] = 0.11f; engine->bgColor[1] = 0.11f; engine->bgColor[2] = 0.11f; engine->bgColor[3] = 1.0f;
            engine->textColor[0] = 0.95f; engine->textColor[1] = 0.95f; engine->textColor[2] = 0.95f; engine->textColor[3] = 1.0f;
            engine->gutterBgColor[0] = 0.11f; engine->gutterBgColor[1] = 0.11f; engine->gutterBgColor[2] = 0.11f; engine->gutterBgColor[3] = 1.0f;
            engine->gutterTextColor[0] = 0.45f; engine->gutterTextColor[1] = 0.45f; engine->gutterTextColor[2] = 0.45f; engine->gutterTextColor[3] = 1.0f;
            engine->selColor[0] = 0.15f; engine->selColor[1] = 0.35f; engine->selColor[2] = 0.60f; engine->selColor[3] = 0.6f;
            engine->caretColor[0] = 1.0f; engine->caretColor[1] = 1.0f; engine->caretColor[2] = 1.0f; engine->caretColor[3] = 1.0f;
            engine->autoHlColor[0] = 0.35f; engine->autoHlColor[1] = 0.35f; engine->autoHlColor[2] = 0.35f; engine->autoHlColor[3] = 0.5f;
        } else {
            engine->bgColor[0] = 1.0f; engine->bgColor[1] = 1.0f; engine->bgColor[2] = 1.0f; engine->bgColor[3] = 1.0f;
            engine->textColor[0] = 0.12f; engine->textColor[1] = 0.12f; engine->textColor[2] = 0.12f; engine->textColor[3] = 1.0f;
            engine->gutterBgColor[0] = 0.97f; engine->gutterBgColor[1] = 0.97f; engine->gutterBgColor[2] = 0.97f; engine->gutterBgColor[3] = 1.0f;
            engine->gutterTextColor[0] = 0.60f; engine->gutterTextColor[1] = 0.60f; engine->gutterTextColor[2] = 0.60f; engine->gutterTextColor[3] = 1.0f;
            engine->selColor[0] = 0.73f; engine->selColor[1] = 0.82f; engine->selColor[2] = 0.94f; engine->selColor[3] = 0.7f;
            engine->caretColor[0] = 0.0f; engine->caretColor[1] = 0.0f; engine->caretColor[2] = 0.0f; engine->caretColor[3] = 1.0f;
            engine->autoHlColor[0] = 0.85f; engine->autoHlColor[1] = 0.85f; engine->autoHlColor[2] = 0.85f; engine->autoHlColor[3] = 0.5f;
        }
    }
    env->DeleteLocalRef(activityClass);
    if (needDetach) {
        vm->DetachCurrentThread();
    }
}
struct ImeEvent {
    enum Type { Commit, Composing, Delete, DeleteForward, FinishComposing } type;
    std::string text;
};
std::mutex g_imeMutex;
std::deque<ImeEvent> g_imeQueue;
Engine* g_engine = nullptr;
void updateGutterWidth(Engine* engine) {
    int totalLines = (int)engine->lineStarts.size();
    if (totalLines == 0) totalLines = 1;
    if (engine->fallbackFaces.empty()) {
        engine->gutterWidth = 100.0f;
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
void stopAsyncParsing(Engine* engine) {
    if (engine->isAsyncParsing || engine->asyncParseThread.joinable()) {
        engine->cancelParse = true;
        if (engine->asyncParseThread.joinable()) {
            engine->asyncParseThread.join();
        }
        engine->isAsyncParsing = false;
    }
    std::lock_guard<std::mutex> lock(engine->asyncParseMutex);
    engine->pendingLineChunks.clear();
}
void asyncParseWorker(Engine* engine, const char* buffer, size_t size, size_t startOffset, size_t startPos) {
    engine->isAsyncParsing = true;
    size_t currentPos = startPos;
    std::vector<size_t> localChunk;
    localChunk.reserve(10000);
    const char* ptr = buffer + startOffset;
    const char* end = buffer + size;
    while (ptr < end && !engine->cancelParse.load(std::memory_order_relaxed)) {
        char c = *ptr;
        if (c == '\n') {
            localChunk.push_back(currentPos + 1);
            ptr++; currentPos++;
        } else if (c == '\r') {
            size_t step = 1;
            if (ptr + 1 < end && *(ptr + 1) == '\n') step = 2;
            localChunk.push_back(currentPos + step);
            ptr += step; currentPos += step;
        } else {
            ptr++; currentPos++;
        }
        if (localChunk.size() >= 10000) {
            std::lock_guard<std::mutex> lock(engine->asyncParseMutex);
            engine->pendingLineChunks.push_back(std::move(localChunk));
            localChunk = std::vector<size_t>();
            localChunk.reserve(10000);
        }
    }
    if (!localChunk.empty() && !engine->cancelParse.load(std::memory_order_relaxed)) {
        std::lock_guard<std::mutex> lock(engine->asyncParseMutex);
        engine->pendingLineChunks.push_back(std::move(localChunk));
    }
    engine->isAsyncParsing = false;
}
void startAsyncLineParsing(Engine* engine, const char* buffer, size_t size) {
    stopAsyncParsing(engine);
    engine->lineStarts.clear();
    engine->lineStarts.push_back(0);
    size_t syncLimit = std::min(size, (size_t)(512 * 1024));
    size_t currentPos = 0;
    const char* ptr = buffer;
    const char* end = buffer + syncLimit;
    while (ptr < end) {
        char c = *ptr;
        if (c == '\n') {
            engine->lineStarts.push_back(currentPos + 1);
            ptr++; currentPos++;
        } else if (c == '\r') {
            size_t step = 1;
            if (ptr + 1 < (buffer + size) && *(ptr + 1) == '\n') step = 2;
            engine->lineStarts.push_back(currentPos + step);
            ptr += step; currentPos += step;
        } else {
            ptr++; currentPos++;
        }
    }
    updateGutterWidth(engine);
    engine->lineCaches.resize(engine->lineStarts.size());
    size_t actualOffset = ptr - buffer;
    if (actualOffset < size) {
        engine->cancelParse = false;
        engine->asyncParseThread = std::thread(asyncParseWorker, engine, buffer, size, actualOffset, currentPos);
    }
}
void pollAsyncParsing(Engine* engine) {
    std::lock_guard<std::mutex> lock(engine->asyncParseMutex);
    if (!engine->pendingLineChunks.empty()) {
        for (auto& chunk : engine->pendingLineChunks) {
            engine->lineStarts.insert(engine->lineStarts.end(),
                                      std::make_move_iterator(chunk.begin()),
                                      std::make_move_iterator(chunk.end()));
        }
        engine->pendingLineChunks.clear();
        updateGutterWidth(engine);
        engine->lineCaches.resize(engine->lineStarts.size());
    }
}
void rebuildLineStarts(Engine* engine) {
    stopAsyncParsing(engine);
    engine->lineStarts.clear();
    engine->lineStarts.push_back(0);
    size_t currentPos = 0;
    for (const auto& p : engine->pt.pieces) {
        const char* buf = p.isOriginal ? engine->pt.origPtr : engine->pt.addBuf.data();
        size_t startIdx = p.start;
        size_t len = p.len;
        const char* ptr = buf + startIdx;
        const char* end = ptr + len;
        while (ptr < end) {
            char c = *ptr;
            if (c == '\n') {
                size_t offsetInPiece = ptr - (buf + startIdx);
                engine->lineStarts.push_back(currentPos + offsetInPiece + 1);
                ptr++;
            } else if (c == '\r') {
                size_t offsetInPiece = ptr - (buf + startIdx);
                size_t globalPos = currentPos + offsetInPiece;
                size_t totalLen = engine->pt.length();
                size_t step = 1;
                if (ptr + 1 < end) {
                    if (*(ptr + 1) == '\n') step = 2;
                } else if (globalPos + 1 < totalLen) {
                    if (engine->pt.charAt(globalPos + 1) == '\n') step = 2;
                }
                engine->lineStarts.push_back(globalPos + step);
                ptr += step;
            } else {
                ptr++;
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
        uint32_t cp = decodeUtf8(&ptr, end_ptr);
        int fIdx = getFontIndexForChar(engine, cp, currentFont == -1 ? 0 : currentFont);
        if (fIdx != currentFont) {
            runs.push_back({fIdx, {}, {}});
            currentFont = fIdx;
        }
        runs.back().codepoints.push_back(cp);
        runs.back().byteOffsets.push_back(prevOffset);
    }
    hb_buffer_t* hb_buf = hb_buffer_create();

    // ★追加: 行内での現在X座標（Unscaled基準）を追跡する
    float current_line_x = 0.0f;

    for (const auto& run : runs) {
        hb_buffer_clear_contents(hb_buf);
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
            size_t charIdx = info[i].cluster;
            size_t rawCluster = run.byteOffsets[charIdx];

            // ★追加: タブ文字の場合、幅（xAdvance）を半角4文字分のタブストップに揃える
            if (text[rawCluster] == '\t') {
                float tabWidthUnscaled = 24.0f * 4.0f; // 基準半角文字幅(24.0f) × 4
                float nextStop = std::floor(current_line_x / tabWidthUnscaled + 1.0f) * tabWidthUnscaled;
                sg.xAdvance = nextStop - current_line_x;
                if (sg.xAdvance < 1.0f) sg.xAdvance += tabWidthUnscaled; // 近すぎる場合の保護
            }
            current_line_x += sg.xAdvance;

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
    while (i < glyphs.size()) {
        size_t cluster = glyphs[i].cluster;
        bool isIME = glyphs[i].isIME;
        float clusterAdvance = 0.0f;
        size_t next_i = i;
        while (next_i < glyphs.size() && glyphs[next_i].cluster == cluster && glyphs[next_i].isIME == isIME) {
            clusterAdvance += glyphs[next_i].xAdvance * scale;
            next_i++;
        }
        if (touchX < currentX + (clusterAdvance / 2.0f)) {
            return isIME ? engine->cursors.back().head : cluster;
        }
        currentX += clusterAdvance;
        i = next_i;
    }
    size_t end = (lineIdx + 1 < engine->lineStarts.size()) ? engine->lineStarts[lineIdx + 1] : engine->pt.length();
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
void selectLineAt(Engine* engine, size_t pos) {
    int lineIdx = getLineIdx(engine, pos);
    size_t start = engine->lineStarts[lineIdx];
    size_t end = (lineIdx + 1 < (int)engine->lineStarts.size()) ? engine->lineStarts[lineIdx + 1] : engine->pt.length();
    if (end > start && engine->pt.charAt(end - 1) == '\n') {
        end--;
        if (end > start && engine->pt.charAt(end - 1) == '\r') {
            end--;
        }
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
    if (caretX < engine->scrollX + engine->gutterWidth) {
        engine->scrollX = caretX - engine->gutterWidth;
    } else if (caretX + engine->charWidth * 2.0f > engine->scrollX + winW) {
        engine->scrollX = caretX + engine->charWidth * 2.0f - winW;
    }
    if (engine->scrollX < 0.0f) engine->scrollX = 0.0f;
    if (engine->scrollY < 0.0f) engine->scrollY = 0.0f;
}
void insertAtCursors(Engine* engine, const std::string& text) {
    if (engine->cursors.empty()) return;
    EditBatch batch;
    batch.beforeCursors = engine->cursors;

    // 後ろから順に処理することで、先頭の挿入によるオフセットの狂いを防ぐ
    std::vector<size_t> indices(engine->cursors.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
        return engine->cursors[a].start() > engine->cursors[b].start();
    });

    for (size_t idx : indices) {
        Cursor& c = engine->cursors[idx];
        size_t st = c.start();
        size_t l = c.end() - st;

        if (l > 0) {
            std::string d = engine->pt.getRange(st, l);
            engine->pt.erase(st, l);
            batch.ops.push_back({ EditOp::Erase, st, d });
            // 他のカーソル位置の調整
            for (auto& o : engine->cursors) {
                if (o.head > st) o.head -= std::min(o.head - st, l);
                if (o.anchor > st) o.anchor -= std::min(o.anchor - st, l);
            }
            c.head = st; c.anchor = st;
        }

        if (!text.empty()) {
            engine->pt.insert(st, text);
            batch.ops.push_back({ EditOp::Insert, st, text });
            // 挿入による他のカーソル位置の調整
            for (auto& o : engine->cursors) {
                if (o.head >= st) o.head += text.size();
                if (o.anchor >= st) o.anchor += text.size();
            }
        }
    }

    for (auto& c : engine->cursors) {
        c.desiredX = getXFromPos(engine, c.head);
    }

    batch.afterCursors = engine->cursors;
    engine->undo.push(batch);
    bool wasDirty = engine->isDirty;
    engine->isDirty = true;
    rebuildLineStarts(engine);
    ensureCaretVisible(engine);
    if (!wasDirty) updateTitleBarIfNeeded(engine);
}
void insertNewlineWithAutoIndent(Engine* engine) {
    if (engine->cursors.empty()) return;
    EditBatch batch;
    batch.beforeCursors = engine->cursors;

    // 複数カーソルがある場合、後方（ファイル末尾側）から処理してインデックスのズレを防ぐ
    std::vector<size_t> indices(engine->cursors.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
        return engine->cursors[a].start() > engine->cursors[b].start();
    });

    for (size_t idx : indices) {
        Cursor& c = engine->cursors[idx];
        size_t start = c.start();

        // 選択範囲がある場合は先に削除
        if (c.hasSelection()) {
            size_t len = c.end() - start;
            std::string deleted = engine->pt.getRange(start, len);
            engine->pt.erase(start, len);
            batch.ops.push_back({ EditOp::Erase, start, deleted });
            for (auto& o : engine->cursors) {
                if (o.head > start) o.head -= std::min(o.head - start, len);
                if (o.anchor > start) o.anchor -= std::min(o.anchor - start, len);
            }
            c.head = start; c.anchor = start;
        }

        // 現在行の行頭からカーソル位置までの間にあるスペースとタブを抽出
        int lineIdx = getLineIdx(engine, start);
        size_t lineStart = engine->lineStarts[lineIdx];
        std::string indentStr = "";
        size_t p = lineStart;
        size_t maxLen = engine->pt.length();

        while (p < maxLen && p < start) {
            char ch = engine->pt.charAt(p);
            if (ch == ' ' || ch == '\t') {
                indentStr += ch;
            } else {
                break;
            }
            p++;
        }

        // 改行コード ＋ 抽出したインデント を挿入
        std::string textToInsert = engine->newlineStr + indentStr;
        engine->pt.insert(start, textToInsert);
        batch.ops.push_back({ EditOp::Insert, start, textToInsert });
        size_t insLen = textToInsert.size();

        for (auto& o : engine->cursors) {
            if (o.head >= start) o.head += insLen;
            if (o.anchor >= start) o.anchor += insLen;
        }
    }

    for (auto& c : engine->cursors) {
        c.desiredX = getXFromPos(engine, c.head);
    }

    batch.afterCursors = engine->cursors;
    engine->undo.push(batch);
    bool wasDirty = engine->isDirty;
    engine->isDirty = true;
    rebuildLineStarts(engine);
    ensureCaretVisible(engine);
    if (!wasDirty) updateTitleBarIfNeeded(engine);
}
void performUndo(Engine* engine) {
    if (engine->undo.undoStack.empty()) return;
    EditBatch b = engine->undo.undoStack.back();
    engine->undo.undoStack.pop_back();
    engine->undo.redoStack.push_back(b);
    for (int i = (int)b.ops.size() - 1; i >= 0; --i) {
        const auto& o = b.ops[i];
        if (o.type == EditOp::Insert) engine->pt.erase(o.pos, o.text.size());
        else engine->pt.insert(o.pos, o.text);
    }
    engine->cursors = b.beforeCursors;
    rebuildLineStarts(engine);
    ensureCaretVisible(engine);
    updateDirtyFlag(engine);
}
void performRedo(Engine* engine) {
    if (engine->undo.redoStack.empty()) return;
    EditBatch b = engine->undo.redoStack.back();
    engine->undo.redoStack.pop_back();
    engine->undo.undoStack.push_back(b);
    for (const auto& o : b.ops) {
        if (o.type == EditOp::Insert) engine->pt.insert(o.pos, o.text);
        else engine->pt.erase(o.pos, o.text.size());
    }
    engine->cursors = b.afterCursors;
    rebuildLineStarts(engine);
    ensureCaretVisible(engine);
    updateDirtyFlag(engine);
}
void backspaceAtCursors(Engine* engine) {
    if (engine->cursors.empty()) return;
    EditBatch batch;
    batch.beforeCursors = engine->cursors;

    std::vector<size_t> indices(engine->cursors.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
        return engine->cursors[a].head > engine->cursors[b].head;
    });

    bool changed = false;
    for (size_t idx : indices) {
        Cursor& c = engine->cursors[idx];
        if (c.hasSelection()) {
            size_t st = c.start();
            size_t len = c.end() - st;
            std::string d = engine->pt.getRange(st, len);
            engine->pt.erase(st, len);
            batch.ops.push_back({ EditOp::Erase, st, d });
            changed = true;
            for (auto& o : engine->cursors) {
                if (o.head > st) o.head -= std::min(o.head - st, len);
                if (o.anchor > st) o.anchor -= std::min(o.anchor - st, len);
            }
            c.head = st; c.anchor = st;
            continue;
        }

        if (c.head > 0) {
            size_t eraseStart = c.head - 1;
            bool foundBoundary = false;
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
            if (!foundBoundary) {
                char ch = engine->pt.charAt(c.head - 1);
                if (ch == '\n' || ch == '\r') {
                    eraseStart = c.head - 1;
                    if (ch == '\n' && c.head >= 2 && engine->pt.charAt(c.head - 2) == '\r') eraseStart = c.head - 2;
                } else {
                    eraseStart = c.head - 1;
                    while (eraseStart > 0 && ((unsigned char)engine->pt.charAt(eraseStart) & 0xC0) == 0x80) eraseStart--;
                    if (c.head - eraseStart == 3 && eraseStart >= 3) {
                        std::string bytes = engine->pt.getRange(eraseStart, 3);
                        if ((unsigned char)bytes[0] == 0xED && (unsigned char)bytes[1] >= 0xB0 && (unsigned char)bytes[1] <= 0xBF) {
                            size_t highStart = eraseStart - 1;
                            while (highStart > 0 && ((unsigned char)engine->pt.charAt(highStart) & 0xC0) == 0x80) highStart--;
                            if (eraseStart - highStart == 3) {
                                std::string hBytes = engine->pt.getRange(highStart, 3);
                                if ((unsigned char)hBytes[0] == 0xED && (unsigned char)hBytes[1] >= 0xA0 && (unsigned char)hBytes[1] <= 0xAF) eraseStart = highStart;
                            }
                        }
                    }
                }
            }

            size_t eraseLen = c.head - eraseStart;
            std::string d = engine->pt.getRange(eraseStart, eraseLen);
            engine->pt.erase(eraseStart, eraseLen);
            batch.ops.push_back({ EditOp::Erase, eraseStart, d });
            changed = true;

            for (auto& o : engine->cursors) {
                if (&o == &c) continue;
                if (o.head > eraseStart) o.head -= std::min(o.head - eraseStart, eraseLen);
                if (o.anchor > eraseStart) o.anchor -= std::min(o.anchor - eraseStart, eraseLen);
            }
            c.head = eraseStart; c.anchor = eraseStart;
        }
    }

    for (auto& c : engine->cursors) c.desiredX = getXFromPos(engine, c.head);

    if (changed) {
        batch.afterCursors = engine->cursors;
        engine->undo.push(batch);
        bool wasDirty = engine->isDirty;
        engine->isDirty = true;
        rebuildLineStarts(engine);
        if (!wasDirty) updateTitleBarIfNeeded(engine);
    }
    ensureCaretVisible(engine);
}
void deleteForwardAtCursors(Engine* engine) {
    if (engine->cursors.empty()) return;
    EditBatch batch;
    batch.beforeCursors = engine->cursors;

    std::vector<size_t> indices(engine->cursors.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
        return engine->cursors[a].start() > engine->cursors[b].start();
    });

    bool changed = false;
    for (size_t idx : indices) {
        Cursor& c = engine->cursors[idx];
        if (c.hasSelection()) {
            size_t st = c.start();
            size_t len = c.end() - st;
            std::string d = engine->pt.getRange(st, len);
            engine->pt.erase(st, len);
            batch.ops.push_back({ EditOp::Erase, st, d });
            changed = true;
            for (auto& o : engine->cursors) {
                if (o.head > st) o.head -= std::min(o.head - st, len);
                if (o.anchor > st) o.anchor -= std::min(o.anchor - st, len);
            }
            c.head = st; c.anchor = st;
            continue;
        }

        size_t len = engine->pt.length();
        if (c.head >= len) continue;

        size_t eraseStart = c.head;
        size_t eraseEnd = c.head + 1;
        int lineIdx = getLineIdx(engine, c.head);
        if (lineIdx >= 0 && lineIdx < engine->lineStarts.size()) {
            ensureLineShaped(engine, lineIdx);
            const auto& cache = engine->lineCaches[lineIdx];
            bool found = false;
            for (const auto& sg : cache.glyphs) {
                if (!sg.isIME && sg.cluster > c.head) {
                    eraseEnd = sg.cluster; found = true; break;
                }
            }
            if (!found) {
                size_t p = c.head;
                if (p < len) {
                    if (engine->pt.charAt(p) == '\r' && p + 1 < len && engine->pt.charAt(p + 1) == '\n') {
                        eraseEnd = p + 2;
                    } else {
                        p++;
                        while (p < len && ((unsigned char)engine->pt.charAt(p) & 0xC0) == 0x80) p++;
                        eraseEnd = p;
                    }
                }
            }
        }

        size_t eraseLen = eraseEnd - eraseStart;
        std::string d = engine->pt.getRange(eraseStart, eraseLen);
        engine->pt.erase(eraseStart, eraseLen);
        batch.ops.push_back({ EditOp::Erase, eraseStart, d });
        changed = true;

        for (auto& o : engine->cursors) {
            if (&o == &c) continue;
            if (o.head > eraseStart) o.head -= std::min(o.head - eraseStart, eraseLen);
            if (o.anchor > eraseStart) o.anchor -= std::min(o.anchor - eraseStart, eraseLen);
        }
    }

    for (auto& c : engine->cursors) c.desiredX = getXFromPos(engine, c.head);

    if (changed) {
        batch.afterCursors = engine->cursors;
        engine->undo.push(batch);
        bool wasDirty = engine->isDirty;
        engine->isDirty = true;
        rebuildLineStarts(engine);
        if (!wasDirty) updateTitleBarIfNeeded(engine);
    }
    ensureCaretVisible(engine);
}
std::string internalCopy(Engine* engine) {
    bool hasSelection = false;
    for (const auto& c : engine->cursors) { if (c.hasSelection()) { hasSelection = true; break; } }
    std::string t;
    if (hasSelection) {
        std::vector<Cursor> s = engine->cursors;
        std::sort(s.begin(), s.end(), [](const Cursor& a, const Cursor& b) { return a.start() < b.start(); });
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i].hasSelection()) {
                std::string part = engine->pt.getRange(s[i].start(), s[i].end() - s[i].start());
                t += part;
                if (i < s.size() - 1 && !part.empty() && part.back() != '\n' && part.back() != '\r') {
                    t += engine->newlineStr;
                }
            }
        }
    } else {
        std::vector<int> processedLines;
        std::vector<Cursor> s = engine->cursors;
        std::sort(s.begin(), s.end(), [](const Cursor& a, const Cursor& b) { return a.head < b.head; });
        for (const auto& c : s) {
            int lineIdx = getLineIdx(engine, c.head);
            bool dup = false;
            for (int p : processedLines) if (p == lineIdx) dup = true;
            if (dup) continue;
            processedLines.push_back(lineIdx);
            size_t start = engine->lineStarts[lineIdx];
            size_t end = (lineIdx + 1 < (int)engine->lineStarts.size()) ? engine->lineStarts[lineIdx + 1] : engine->pt.length();
            std::string lineText = engine->pt.getRange(start, end - start);
            t += lineText;
            if (lineIdx == (int)engine->lineStarts.size() - 1) {
                if (lineText.empty() || lineText.back() != '\n') t += engine->newlineStr;
            }
        }
    }
    return t;
}
void getWordBoundaries(Engine* engine, size_t pos, size_t& start, size_t& end) {
    size_t len = engine->pt.length();
    if (len == 0 || pos >= len) { start = end = pos; return; }
    char c = engine->pt.charAt(pos);
    if (c == '\r') {
        if (pos + 1 < len && engine->pt.charAt(pos + 1) == '\n') {
            start = pos; end = pos + 2; return;
        }
    }
    bool targetType = isWordChar(c);
    if (c == '\n') { start = pos; end = pos + 1; return; }
    start = pos;
    while (start > 0) {
        char p = engine->pt.charAt(start - 1);
        if (isWordChar(p) != targetType || p == '\n' || p == '\r') break;
        start--;
    }
    end = pos;
    while (end < len) {
        char p = engine->pt.charAt(end);
        if (isWordChar(p) != targetType || p == '\n' || p == '\r') break;
        end++;
    }
}
#include <utility>
std::pair<std::string, bool> getHighlightTarget(Engine* engine) {
    if (engine->cursors.size() > 1) return { "", false };
    if (engine->cursors.empty()) return { "", false };
    const Cursor& c = engine->cursors.back();
    if (c.hasSelection()) {
        size_t len = c.end() - c.start();
        if (len == 0 || len > 200) return { "", false };
        std::string s = engine->pt.getRange(c.start(), len);
        if (s.empty() || s.find('\n') != std::string::npos || s.find('\r') != std::string::npos) return { "", false };
        return { s, false };
    }
    size_t pos = c.head;
    size_t len = engine->pt.length();
    if (pos > len) pos = len;
    bool charRight = (pos < len && isWordChar(engine->pt.charAt(pos)));
    bool charLeft = (pos > 0 && isWordChar(engine->pt.charAt(pos - 1)));
    if (!charRight && !charLeft) return { "", true };
    size_t start = pos;
    size_t end = pos;
    if (!charRight && charLeft) start--;
    while (start > 0 && isWordChar(engine->pt.charAt(start - 1))) start--;
    while (end < len && isWordChar(engine->pt.charAt(end))) end++;
    if (end > start) return { engine->pt.getRange(start, end - start), true };
    return { "", true };
}
std::vector<int> getUniqueLineIndices(Engine* engine) {
    std::vector<int> lines;
    for (const auto& c : engine->cursors) {
        if (!c.hasSelection()) {
            lines.push_back(getLineIdx(engine, c.head));
        } else {
            size_t start = c.start(), end = c.end();
            int lStart = getLineIdx(engine, start), lEnd = getLineIdx(engine, end);
            if (lEnd > lStart) {
                size_t lineStartPos = engine->lineStarts[lEnd];
                if (end == lineStartPos) lEnd--;
            }
            for (int i = lStart; i <= lEnd; ++i) lines.push_back(i);
        }
    }
    std::sort(lines.begin(), lines.end());
    lines.erase(std::unique(lines.begin(), lines.end()), lines.end());
    return lines;
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
        success = openDocumentFromFile(g_engine, strPath, env);
        env->ReleaseStringUTFChars(path, strPath);
        if (success) {
            rebuildLineStarts(g_engine);
            ensureCaretVisible(g_engine);
        }
    }
    return success ? JNI_TRUE : JNI_FALSE;
}
JNIEXPORT jstring JNICALL Java_jp_hack_miu_MainActivity_cmdGetCharset(JNIEnv* env, jobject thiz) {
    if (!g_engine) return env->NewStringUTF("UTF-8");
    std::lock_guard<std::mutex> lock(g_imeMutex);
    return env->NewStringUTF(g_engine->currentCharset.c_str());
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
    if (g_engine->isDirty) {
        g_engine->isDirty = false;
        updateTitleBarIfNeeded(g_engine);
    }
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
JNIEXPORT void JNICALL Java_jp_hack_miu_MainActivity_cmdSetDisplayFileName(JNIEnv* env, jobject thiz, jstring name) {
    if (!g_engine) return;
    const char* strName = env->GetStringUTFChars(name, nullptr);
    if (strName) {
        std::lock_guard<std::mutex> lock(g_imeMutex);
        g_engine->displayFileName = strName;
        env->ReleaseStringUTFChars(name, strName);
        updateTitleBarIfNeeded(g_engine);
    }
}
JNIEXPORT void JNICALL Java_jp_hack_miu_MainActivity_cmdTop(JNIEnv* env, jobject thiz) {
    if (!g_engine) return;
    std::lock_guard<std::mutex> lock(g_imeMutex);
    g_engine->scrollY = 0.0f;
    g_engine->isFlinging = false;
    g_engine->velocityY = 0.0f;
    g_engine->cursors.clear();
    g_engine->cursors.push_back({0, 0, getXFromPos(g_engine, 0)});
}
JNIEXPORT void JNICALL Java_jp_hack_miu_MainActivity_cmdBottom(JNIEnv* env, jobject thiz) {
    if (!g_engine) return;
    std::lock_guard<std::mutex> lock(g_imeMutex);
    size_t len = g_engine->pt.length();
    g_engine->cursors.clear();
    g_engine->cursors.push_back({len, len, getXFromPos(g_engine, len)});
    ensureCaretVisible(g_engine);
}
JNIEXPORT void JNICALL Java_jp_hack_miu_MainActivity_cmdGoToLine(JNIEnv* env, jobject thiz, jint line) {
    if (!g_engine) return;
    std::lock_guard<std::mutex> lock(g_imeMutex);
    int totalLines = (int)g_engine->lineStarts.size();
    if (totalLines == 0) return;
    int target = line;
    if (target < 1) target = 1;
    if (target > totalLines) target = totalLines;
    int lineIdx = target - 1;
    size_t newPos = g_engine->lineStarts[lineIdx];
    g_engine->cursors.clear();
    g_engine->cursors.push_back({newPos, newPos, getXFromPos(g_engine, newPos)});
    ensureCaretVisible(g_engine);
}
JNIEXPORT jint JNICALL Java_jp_hack_miu_MainActivity_cmdGetCurrentLine(JNIEnv* env, jobject thiz) {
    if (!g_engine) return 1;
    std::lock_guard<std::mutex> lock(g_imeMutex);
    size_t pos = g_engine->cursors.empty() ? 0 : g_engine->cursors.back().head;
    return getLineIdx(g_engine, pos) + 1;
}
JNIEXPORT void JNICALL Java_jp_hack_miu_MainActivity_cmdUndo(JNIEnv* env, jobject thiz) {
    if (!g_engine) return;
    std::lock_guard<std::mutex> lock(g_imeMutex);
    performUndo(g_engine);
}
JNIEXPORT void JNICALL Java_jp_hack_miu_MainActivity_cmdRedo(JNIEnv* env, jobject thiz) {
    if (!g_engine) return;
    std::lock_guard<std::mutex> lock(g_imeMutex);
    performRedo(g_engine);
}
JNIEXPORT void JNICALL Java_jp_hack_miu_MainActivity_cmdSelectAll(JNIEnv* env, jobject thiz) {
    if (!g_engine) return;
    std::lock_guard<std::mutex> lock(g_imeMutex);
    g_engine->cursors.clear();
    g_engine->cursors.push_back({g_engine->pt.length(), 0, 0.0f});
    ensureCaretVisible(g_engine);
}
JNIEXPORT jstring JNICALL Java_jp_hack_miu_MainActivity_cmdCopy(JNIEnv* env, jobject thiz) {
    if (!g_engine) return env->NewStringUTF("");
    std::lock_guard<std::mutex> lock(g_imeMutex);
    std::string t = internalCopy(g_engine);
    return env->NewStringUTF(t.c_str());
}
JNIEXPORT jstring JNICALL Java_jp_hack_miu_MainActivity_cmdCut(JNIEnv* env, jobject thiz) {
    if (!g_engine) return env->NewStringUTF("");
    std::lock_guard<std::mutex> lock(g_imeMutex);
    std::string t = internalCopy(g_engine);
    bool hasSelection = false;
    for (const auto& c : g_engine->cursors) { if (c.hasSelection()) { hasSelection = true; break; } }
    if (hasSelection) {
        insertAtCursors(g_engine, "");
    } else {
        std::vector<Cursor> delRanges;
        for (const auto& c : g_engine->cursors) {
            int lineIdx = getLineIdx(g_engine, c.head);
            size_t start = g_engine->lineStarts[lineIdx];
            size_t end = (lineIdx + 1 < (int)g_engine->lineStarts.size()) ? g_engine->lineStarts[lineIdx + 1] : g_engine->pt.length();
            if (end == g_engine->pt.length() && start > 0) {
                char prev = g_engine->pt.charAt(start - 1);
                if (prev == '\n') {
                    start--;
                    if (start > 0 && g_engine->pt.charAt(start - 1) == '\r') start--;
                } else if (prev == '\r') start--;
            }
            delRanges.push_back({ end, start, 0.0f });
        }
        g_engine->cursors = delRanges;
        insertAtCursors(g_engine, "");
    }
    return env->NewStringUTF(t.c_str());
}
JNIEXPORT void JNICALL Java_jp_hack_miu_MainActivity_cmdPaste(JNIEnv* env, jobject thiz, jstring text) {
    if (!g_engine) return;
    const char* str = env->GetStringUTFChars(text, nullptr);
    if (str) {
        std::lock_guard<std::mutex> lock(g_imeMutex);
        std::string rawStr(str);
        std::string normalizedStr;
        normalizedStr.reserve(rawStr.size());
        for (size_t i = 0; i < rawStr.size(); ++i) {
            if (rawStr[i] == '\r') {
                normalizedStr += g_engine->newlineStr;
                if (i + 1 < rawStr.size() && rawStr[i + 1] == '\n') {
                    i++;
                }
            } else if (rawStr[i] == '\n') {
                normalizedStr += g_engine->newlineStr;
            } else {
                normalizedStr += rawStr[i];
            }
        }
        insertAtCursors(g_engine, normalizedStr);
        env->ReleaseStringUTFChars(text, str);
    }
}
JNIEXPORT jbyteArray JNICALL Java_jp_hack_miu_MainActivity_cmdGetSaveData(JNIEnv* env, jobject thiz) {
    if (!g_engine) return nullptr;
    std::lock_guard<std::mutex> lock(g_imeMutex);
    std::string utf8Text = g_engine->pt.getRange(0, g_engine->pt.length());
    jbyteArray utf8Array = env->NewByteArray(utf8Text.size());
    env->SetByteArrayRegion(utf8Array, 0, utf8Text.size(), (const jbyte*)utf8Text.data());
    jstring utf8CharsetStr = env->NewStringUTF("UTF-8");
    jclass stringClass = env->FindClass("java/lang/String");
    jmethodID strCtor = env->GetMethodID(stringClass, "<init>", "([BLjava/lang/String;)V");
    jstring javaStr = (jstring)env->NewObject(stringClass, strCtor, utf8Array, utf8CharsetStr);
    jmethodID getBytesMid = env->GetMethodID(stringClass, "getBytes", "(Ljava/lang/String;)[B");
    jstring targetCharsetStr = env->NewStringUTF(g_engine->currentCharset.c_str());
    jbyteArray targetBytes = (jbyteArray)env->CallObjectMethod(javaStr, getBytesMid, targetCharsetStr);
    jsize targetLen = env->GetArrayLength(targetBytes);
    jbyte* targetElements = env->GetByteArrayElements(targetBytes, nullptr);
    std::vector<uint8_t> bom;
    if (g_engine->currentEncoding == ENC_UTF8_BOM) {
        bom = { 0xEF, 0xBB, 0xBF };
    } else if (g_engine->currentEncoding == ENC_UTF16LE) {
        bom = { 0xFF, 0xFE };
    } else if (g_engine->currentEncoding == ENC_UTF16BE) {
        bom = { 0xFE, 0xFF };
    }
    jbyteArray finalArray = env->NewByteArray(bom.size() + targetLen);
    if (!bom.empty()) {
        env->SetByteArrayRegion(finalArray, 0, bom.size(), (const jbyte*)bom.data());
    }
    if (targetLen > 0) {
        env->SetByteArrayRegion(finalArray, bom.size(), targetLen, targetElements);
    }
    env->ReleaseByteArrayElements(targetBytes, targetElements, JNI_ABORT);
    env->DeleteLocalRef(utf8Array);
    env->DeleteLocalRef(utf8CharsetStr);
    env->DeleteLocalRef(stringClass);
    env->DeleteLocalRef(javaStr);
    env->DeleteLocalRef(targetCharsetStr);
    env->DeleteLocalRef(targetBytes);
    return finalArray;
}
JNIEXPORT jstring JNICALL Java_jp_hack_miu_MainActivity_cmdGetAutoSearchText(JNIEnv* env, jobject thiz) {
    if (!g_engine) return env->NewStringUTF("");
    std::lock_guard<std::mutex> lock(g_imeMutex);
    std::pair<std::string, bool> target = getHighlightTarget(g_engine);
    std::string candidate = target.first;
    bool isWord = target.second;
    bool hasSelection = !g_engine->cursors.empty() && g_engine->cursors.back().hasSelection();
    std::string result = g_engine->searchQuery;
    if (hasSelection) {
        if (!candidate.empty()) {
            result = candidate;
        }
    }
    else {
        if (g_engine->searchQuery.empty() && !candidate.empty() && isWord) {
            result = candidate;
        }
    }
    g_engine->searchQuery = result;
    return env->NewStringUTF(result.c_str());
}
JNIEXPORT void JNICALL Java_jp_hack_miu_MainActivity_cmdMoveCursor(JNIEnv* env, jobject thiz, jint direction, jboolean isCtrl, jboolean keepAnchor) {
    if (!g_engine) return;
    std::lock_guard<std::mutex> lock(g_imeMutex);
    size_t len = g_engine->pt.length();
    for (auto& c : g_engine->cursors) {
        if (direction == 0) {
            if (!keepAnchor && c.hasSelection()) {
                c.head = c.start(); c.anchor = c.head;
            } else if (c.head > 0) {
                if (isCtrl) {
                    size_t p = c.head;
                    p--;
                    while (p > 0 && ((unsigned char)g_engine->pt.charAt(p) & 0xC0) == 0x80) p--;
                    char ch = g_engine->pt.charAt(p);
                    if (ch == '\n') {
                        if (p > 0 && g_engine->pt.charAt(p - 1) == '\r') p--;
                    } else if (ch != '\r') {
                        bool isWord = isWordChar(ch);
                        while (p > 0) {
                            size_t prev = p - 1;
                            while (prev > 0 && ((unsigned char)g_engine->pt.charAt(prev) & 0xC0) == 0x80) prev--;
                            char prevCh = g_engine->pt.charAt(prev);
                            if (prevCh == '\r' || prevCh == '\n' || isWordChar(prevCh) != isWord) break;
                            p = prev;
                        }
                    }
                    c.head = p;
                } else {
                    int lineIdx = getLineIdx(g_engine, c.head);
                    size_t prevPos = 0;
                    bool found = false;
                    if (lineIdx >= 0 && lineIdx < g_engine->lineStarts.size()) {
                        ensureLineShaped(g_engine, lineIdx);
                        const auto& cache = g_engine->lineCaches[lineIdx];
                        for (const auto& sg : cache.glyphs) {
                            if (!sg.isIME && sg.cluster < c.head) {
                                if (!found || sg.cluster > prevPos) {
                                    prevPos = sg.cluster;
                                    found = true;
                                }
                            }
                        }
                    }
                    if (found) {
                        c.head = prevPos;
                    } else {
                        size_t lineStart = (lineIdx >= 0 && lineIdx < g_engine->lineStarts.size()) ? g_engine->lineStarts[lineIdx] : 0;
                        if (c.head > lineStart) {
                            c.head = lineStart;
                        } else {
                            size_t p = c.head - 1;
                            if (p > 0 && g_engine->pt.charAt(p - 1) == '\r' && g_engine->pt.charAt(p) == '\n') p--;
                            c.head = p;
                        }
                    }
                }
                if (!keepAnchor) c.anchor = c.head;
            }
            c.desiredX = getXFromPos(g_engine, c.head);
        } else if (direction == 1) {
            if (!keepAnchor && c.hasSelection()) {
                c.head = c.end(); c.anchor = c.head;
            } else if (c.head < len) {
                if (isCtrl) {
                    // 単語単位で右へ移動
                    size_t p = c.head;
                    char ch = g_engine->pt.charAt(p);
                    if (ch == '\r' || ch == '\n') {
                        p++;
                        if (p < len && ch == '\r' && g_engine->pt.charAt(p) == '\n') p++;
                    } else {
                        bool isWord = isWordChar(ch);
                        while (p < len) {
                            size_t next = p + 1;
                            while (next < len && ((unsigned char)g_engine->pt.charAt(next) & 0xC0) == 0x80) next++;
                            if (next >= len) { p = len; break; }
                            char nextCh = g_engine->pt.charAt(next);
                            if (nextCh == '\r' || nextCh == '\n' || isWordChar(nextCh) != isWord) {
                                p = next; break;
                            }
                            p = next;
                        }
                    }
                    c.head = p;
                } else {
                    int lineIdx = getLineIdx(g_engine, c.head);
                    size_t nextPos = len;
                    bool found = false;
                    size_t textEnd = len;
                    if (lineIdx >= 0 && lineIdx < g_engine->lineStarts.size()) {
                        ensureLineShaped(g_engine, lineIdx);
                        const auto& cache = g_engine->lineCaches[lineIdx];
                        for (const auto& sg : cache.glyphs) {
                            if (!sg.isIME && sg.cluster > c.head) {
                                if (!found || sg.cluster < nextPos) {
                                    nextPos = sg.cluster;
                                    found = true;
                                }
                            }
                        }
                        size_t end = (lineIdx + 1 < g_engine->lineStarts.size()) ? g_engine->lineStarts[lineIdx + 1] : len;
                        textEnd = end;
                        if (textEnd > g_engine->lineStarts[lineIdx] && g_engine->pt.charAt(textEnd - 1) == '\n') textEnd--;
                        if (textEnd > g_engine->lineStarts[lineIdx] && g_engine->pt.charAt(textEnd - 1) == '\r') textEnd--;
                    }
                    if (found) {
                        c.head = nextPos;
                    } else {
                        if (c.head < textEnd) {
                            c.head = textEnd;
                        } else {
                            size_t p = c.head;
                            if (p + 1 < len && g_engine->pt.charAt(p) == '\r' && g_engine->pt.charAt(p + 1) == '\n') p += 2;
                            else p++;
                            c.head = p;
                        }
                    }
                }
                if (!keepAnchor) c.anchor = c.head;
            }
            c.desiredX = getXFromPos(g_engine, c.head);
        } else if (direction == 2 || direction == 3) {
            if (isCtrl) {
                if (direction == 2) {
                    c.head = 0;
                } else {
                    c.head = len;
                }
                if (!keepAnchor) c.anchor = c.head;
                c.desiredX = getXFromPos(g_engine, c.head);
            } else {
                int lineIdx = getLineIdx(g_engine, c.head);
                int targetLineIdx = (direction == 2) ? (lineIdx - 1) : (lineIdx + 1);
                if (targetLineIdx >= 0 && targetLineIdx < (int)g_engine->lineStarts.size()) {
                    float targetVirtualY = targetLineIdx * g_engine->lineHeight + (g_engine->lineHeight * 0.5f);
                    float touchY = targetVirtualY - g_engine->scrollY + g_engine->topMargin;
                    float touchX = c.desiredX - g_engine->scrollX;
                    size_t newPos = getDocPosFromPoint(g_engine, touchX, touchY);
                    c.head = newPos;
                    if (!keepAnchor) c.anchor = newPos;
                } else {
                    if (direction == 2) {
                        c.head = 0;
                        if (!keepAnchor) c.anchor = 0;
                        c.desiredX = getXFromPos(g_engine, 0);
                    } else {
                        c.head = len;
                        if (!keepAnchor) c.anchor = len;
                        c.desiredX = getXFromPos(g_engine, len);
                    }
                }
            }
        }
    }
    ensureCaretVisible(g_engine);
}
JNIEXPORT void JNICALL Java_jp_hack_miu_MainActivity_deleteForwardText(JNIEnv* env, jobject thiz) {
    if (!g_engine) return;
    std::lock_guard<std::mutex> lock(g_imeMutex);
    g_imeQueue.push_back({ ImeEvent::DeleteForward, "" });
}
JNIEXPORT void JNICALL Java_jp_hack_miu_MainActivity_cmdMoveHomeEnd(JNIEnv* env, jobject thiz, jboolean isHome, jboolean isCtrl, jboolean keepAnchor) {
    if (!g_engine) return;
    std::lock_guard<std::mutex> lock(g_imeMutex);
    size_t len = g_engine->pt.length();
    for (auto& c : g_engine->cursors) {
        if (isHome) {
            if (isCtrl) {
                c.head = 0;
            } else {
                // Home: 行頭
                int lineIdx = getLineIdx(g_engine, c.head);
                if (lineIdx >= 0 && lineIdx < g_engine->lineStarts.size()) {
                    c.head = g_engine->lineStarts[lineIdx];
                } else {
                    c.head = 0;
                }
            }
        } else {
            if (isCtrl) {
                c.head = len;
            } else {
                int lineIdx = getLineIdx(g_engine, c.head);
                if (lineIdx >= 0 && lineIdx < g_engine->lineStarts.size()) {
                    size_t lineEnd = (lineIdx + 1 < g_engine->lineStarts.size()) ? g_engine->lineStarts[lineIdx + 1] : len;
                    if (lineEnd > g_engine->lineStarts[lineIdx] && g_engine->pt.charAt(lineEnd - 1) == '\n') lineEnd--;
                    if (lineEnd > g_engine->lineStarts[lineIdx] && g_engine->pt.charAt(lineEnd - 1) == '\r') lineEnd--;
                    c.head = lineEnd;
                } else {
                    c.head = len;
                }
            }
        }
        if (!keepAnchor) {
            c.anchor = c.head;
        }
        c.desiredX = getXFromPos(g_engine, c.head);
    }
    ensureCaretVisible(g_engine);
}
JNIEXPORT void JNICALL Java_jp_hack_miu_MainActivity_cmdSelectNextOccurrence(JNIEnv* env, jobject thiz) {
    if (!g_engine || g_engine->cursors.empty()) return;
    std::lock_guard<std::mutex> lock(g_imeMutex);

    // 【重要】参照(&)ではなくコピーで受け取る（push_back時のクラッシュ防止）
    Cursor c = g_engine->cursors.back();

    if (!c.hasSelection()) {
        // 選択範囲がない場合は、現在の単語を選択する（1回目のCtrl+D）
        size_t targetPos = c.head;
        if (targetPos > 0) {
            char currChar = targetPos < g_engine->pt.length() ? g_engine->pt.charAt(targetPos) : '\0';
            char prevChar = g_engine->pt.charAt(targetPos - 1);
            if (!isWordChar(currChar) && isWordChar(prevChar)) { targetPos--; }
        }
        size_t s, e;
        getWordBoundaries(g_engine, targetPos, s, e);
        if (s != e) {
            g_engine->cursors.clear();
            g_engine->cursors.push_back({e, s, getXFromPos(g_engine, e)});
        }
        ensureCaretVisible(g_engine);
        return;
    }

    // すでに選択範囲がある場合は、次に出現する同じ単語を探してマルチカーソル化する（2回目以降のCtrl+D）
    size_t start = c.start();
    size_t len = c.end() - start;
    std::string query = g_engine->pt.getRange(start, len);

    size_t matchLen = 0;
    // 検索開始位置は現在のカーソルの末尾（ヘッドとアンカーの大きい方）
    size_t nextPos = findText(g_engine, std::max(c.head, c.anchor), query, true, true, false, false, &matchLen);

    if (nextPos != std::string::npos) {
        // すでに選択されている箇所かどうかをチェック
        for (const auto& cur : g_engine->cursors) {
            if (cur.start() == nextPos) return;
        }
        // 未選択の新しいマッチ箇所なら、カーソルを追加
        size_t newHead = nextPos + matchLen;
        g_engine->cursors.push_back({newHead, nextPos, getXFromPos(g_engine, newHead)});
        ensureCaretVisible(g_engine);
    }
}
JNIEXPORT void JNICALL Java_jp_hack_miu_MainActivity_cmdClearSelectionAndMultiCursor(JNIEnv* env, jobject thiz) {
    if (!g_engine || g_engine->cursors.empty()) return;
    std::lock_guard<std::mutex> lock(g_imeMutex);
   Cursor lastCursor = g_engine->cursors.back();
    if (g_engine->cursors.size() > 1 || lastCursor.hasSelection()) {
        g_engine->cursors.clear();
        lastCursor.anchor = lastCursor.head;
        g_engine->cursors.push_back(lastCursor);
        ensureCaretVisible(g_engine);
    }
}
JNIEXPORT void JNICALL Java_jp_hack_miu_MainActivity_cmdPageMove(JNIEnv* env, jobject thiz, jboolean isUp, jboolean keepAnchor) {
    if (!g_engine) return;
    std::lock_guard<std::mutex> lock(g_imeMutex);
    float winH = 2000.0f;
    if (g_engine->app->window != nullptr) {
        winH = (float)ANativeWindow_getHeight(g_engine->app->window);
    }
    float visibleH = winH - g_engine->bottomInset;
    if (visibleH < winH * 0.3f) visibleH = winH * 0.5f; // 安全対策
    int linesPerPage = (int)(visibleH / g_engine->lineHeight);
    if (linesPerPage > 1) linesPerPage -= 1;
    if (linesPerPage < 1) linesPerPage = 1;
    size_t len = g_engine->pt.length();
    for (auto& c : g_engine->cursors) {
        int lineIdx = getLineIdx(g_engine, c.head);
        int targetLineIdx = isUp ? (lineIdx - linesPerPage) : (lineIdx + linesPerPage);
        if (targetLineIdx >= 0 && targetLineIdx < (int)g_engine->lineStarts.size()) {
            float targetVirtualY = targetLineIdx * g_engine->lineHeight + (g_engine->lineHeight * 0.5f);
            float touchY = targetVirtualY - g_engine->scrollY + g_engine->topMargin;
            float touchX = c.desiredX - g_engine->scrollX;
            size_t newPos = getDocPosFromPoint(g_engine, touchX, touchY);
            c.head = newPos;
            if (!keepAnchor) c.anchor = newPos;
        } else {
            if (isUp) {
                c.head = 0;
                if (!keepAnchor) c.anchor = 0;
                c.desiredX = getXFromPos(g_engine, 0);
            } else {
                c.head = len;
                if (!keepAnchor) c.anchor = len;
                c.desiredX = getXFromPos(g_engine, len);
            }
        }
    }
    ensureCaretVisible(g_engine);
}
JNIEXPORT void JNICALL Java_jp_hack_miu_MainActivity_cmdIndentLines(JNIEnv* env, jobject thiz, jboolean isUnindent) {
    if (!g_engine || g_engine->cursors.empty()) return;
    std::lock_guard<std::mutex> lock(g_imeMutex);
    bool hasSelection = false;
    for (const auto& c : g_engine->cursors) {
        if (c.hasSelection()) { hasSelection = true; break; }
    }
    if (!hasSelection && !isUnindent) {
        insertAtCursors(g_engine, "\t");
        return;
    }
    std::vector<int> lines = getUniqueLineIndices(g_engine);
    if (lines.empty()) return;
    EditBatch batch;
    batch.beforeCursors = g_engine->cursors;
    std::sort(lines.rbegin(), lines.rend());
    for (int lineIdx : lines) {
        size_t pos = g_engine->lineStarts[lineIdx];
        if (isUnindent) {
            if (pos >= g_engine->pt.length()) continue;
            char c = g_engine->pt.charAt(pos);
            size_t eraseLen = 0;
            if (c == '\t') eraseLen = 1;
            else if (c == ' ') eraseLen = 1;
            if (eraseLen > 0) {
                std::string deleted = g_engine->pt.getRange(pos, eraseLen);
                g_engine->pt.erase(pos, eraseLen);
                batch.ops.push_back({ EditOp::Erase, pos, deleted });
                for (auto& cur : g_engine->cursors) {
                    if (cur.head > pos) cur.head -= std::min(cur.head - pos, eraseLen);
                    if (cur.anchor > pos) cur.anchor -= std::min(cur.anchor - pos, eraseLen);
                }
            }
        } else {
            std::string indentStr = "\t";
            g_engine->pt.insert(pos, indentStr);
            batch.ops.push_back({ EditOp::Insert, pos, indentStr });
            for (auto& c : g_engine->cursors) {
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
    }
    for (auto& c : g_engine->cursors) {
        c.desiredX = getXFromPos(g_engine, c.head);
    }
    if (!batch.ops.empty()) {
        batch.afterCursors = g_engine->cursors;
        g_engine->undo.push(batch);
        bool wasDirty = g_engine->isDirty;
        g_engine->isDirty = true;
        rebuildLineStarts(g_engine);
        ensureCaretVisible(g_engine);
        if (!wasDirty) updateTitleBarIfNeeded(g_engine);
    }
}
JNIEXPORT void JNICALL Java_jp_hack_miu_MainActivity_cmdZoom(JNIEnv* env, jobject thiz, jint mode) {
    if (!g_engine) return;
    std::lock_guard<std::mutex> lock(g_imeMutex);
    float newSize = g_engine->currentFontSize;
    if (mode == 0) {
        newSize = 48.0f;
    } else if (mode > 0) {
        newSize *= 1.2f;
    } else {
        newSize /= 1.2f;
    }
    if (newSize < 16.0f) newSize = 16.0f;
    if (newSize > 200.0f) newSize = 200.0f;
    if (newSize != g_engine->currentFontSize) {
        g_engine->currentFontSize = newSize;
        float scale = newSize / 48.0f;
        g_engine->lineHeight = 60.0f * scale;
        g_engine->charWidth = 24.0f * scale;
        for (auto& c : g_engine->cursors) {
            c.desiredX = getXFromPos(g_engine, c.head);
        }
        updateGutterWidth(g_engine);
        ensureCaretVisible(g_engine);
    }
}
JNIEXPORT jstring JNICALL Java_jp_hack_miu_MainActivity_cmdGetTextBeforeCursor(JNIEnv* env, jobject thiz, jint length) {
    if (!g_engine || g_engine->cursors.empty()) return env->NewStringUTF("");
    std::lock_guard<std::mutex> lock(g_imeMutex);
    size_t pos = g_engine->cursors.back().head;
    size_t start = pos;
    int charsFound = 0;
    while (start > 0 && charsFound < length) {
        start--;
        while (start > 0 && (g_engine->pt.charAt(start) & 0xC0) == 0x80) {
            start--;
        }
        charsFound++;
    }
    std::string text = g_engine->pt.getRange(start, pos - start);
    if (!g_engine->imeComp.empty()) {
        text += g_engine->imeComp;
    }
    return env->NewStringUTF(text.c_str());
}
JNIEXPORT jstring JNICALL Java_jp_hack_miu_MainActivity_cmdGetTextAfterCursor(JNIEnv* env, jobject thiz, jint length) {
    if (!g_engine || g_engine->cursors.empty()) return env->NewStringUTF("");
    std::lock_guard<std::mutex> lock(g_imeMutex);

    size_t pos = g_engine->cursors.back().head;
    size_t docLen = g_engine->pt.length();
    size_t end = pos;
    int charsFound = 0;
    while (end < docLen && charsFound < length) {
        unsigned char c = g_engine->pt.charAt(end);
        if (c < 0x80) end += 1;
        else if ((c & 0xE0) == 0xC0) end += 2;
        else if ((c & 0xF0) == 0xE0) end += 3;
        else if ((c & 0xF8) == 0xF0) end += 4;
        else end += 1;
        charsFound++;
    }
    if (end > docLen) end = docLen;
    std::string text = g_engine->pt.getRange(pos, end - pos);
    return env->NewStringUTF(text.c_str());
}
}
void cleanupVulkan(Engine* engine) {
    if (engine->device == VK_NULL_HANDLE) return;
    if (engine->blurPipeline != VK_NULL_HANDLE) vkDestroyPipeline(engine->device, engine->blurPipeline, nullptr);
    if (engine->blurPipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(engine->device, engine->blurPipelineLayout, nullptr);
    if (engine->blurDescriptorPool != VK_NULL_HANDLE) vkDestroyDescriptorPool(engine->device, engine->blurDescriptorPool, nullptr);
    if (engine->blurDescriptorSetLayout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(engine->device, engine->blurDescriptorSetLayout, nullptr);
    if (engine->offscreenFramebuffer != VK_NULL_HANDLE) vkDestroyFramebuffer(engine->device, engine->offscreenFramebuffer, nullptr);
    if (engine->offscreenRenderPass != VK_NULL_HANDLE) vkDestroyRenderPass(engine->device, engine->offscreenRenderPass, nullptr);
    if (engine->offscreenSampler != VK_NULL_HANDLE) vkDestroySampler(engine->device, engine->offscreenSampler, nullptr);
    if (engine->offscreenImageView != VK_NULL_HANDLE) vkDestroyImageView(engine->device, engine->offscreenImageView, nullptr);
    if (engine->offscreenImage != VK_NULL_HANDLE) vkDestroyImage(engine->device, engine->offscreenImage, nullptr);
    if (engine->offscreenImageMemory != VK_NULL_HANDLE) vkFreeMemory(engine->device, engine->offscreenImageMemory, nullptr);
    if (engine->graphicsPipeline != VK_NULL_HANDLE) vkDestroyPipeline(engine->device, engine->graphicsPipeline, nullptr);
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
    for (hb_font_t* hb_font : engine->fallbackHbFonts) if (hb_font) hb_font_destroy(hb_font);
    engine->fallbackHbFonts.clear();
    engine->lineCaches.clear();
    for (FT_Face face : engine->fallbackFaces) if (face) FT_Done_Face(face);
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
    } else return;
    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    endSingleTimeCommands(engine, commandBuffer);
}
void copyBufferToImage(Engine* engine, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands(engine);
    VkBufferImageCopy region = {};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {width, height, 1};
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
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; viewInfo.subresourceRange.levelCount = 1; viewInfo.subresourceRange.layerCount = 1;
    if (vkCreateImageView(engine->device, &viewInfo, nullptr, &engine->fontImageView) != VK_SUCCESS) return false;
    VkSamplerCreateInfo samplerInfo = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.magFilter = VK_FILTER_LINEAR; samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
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
    createInfo.preTransform = (capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = oldSwapchain;
    VkSwapchainKHR newSwapchain;
    if (vkCreateSwapchainKHR(engine->device, &createInfo, nullptr, &newSwapchain) != VK_SUCCESS) return false;
    if (oldSwapchain != VK_NULL_HANDLE) vkDestroySwapchainKHR(engine->device, oldSwapchain, nullptr);
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
bool createOffscreenRenderPass(Engine* engine) {
    VkAttachmentDescription attachment = {};
    attachment.format = engine->swapchainFormat; attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkAttachmentReference colorRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass = {}; subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1; subpass.pColorAttachments = &colorRef;
    VkSubpassDependency dependency = {}; dependency.srcSubpass = VK_SUBPASS_EXTERNAL; dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    VkRenderPassCreateInfo rpInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rpInfo.attachmentCount = 1; rpInfo.pAttachments = &attachment; rpInfo.subpassCount = 1; rpInfo.pSubpasses = &subpass;
    rpInfo.dependencyCount = 1; rpInfo.pDependencies = &dependency;
    return vkCreateRenderPass(engine->device, &rpInfo, nullptr, &engine->offscreenRenderPass) == VK_SUCCESS;
}
bool createOffscreenResources(Engine* engine) {
    VkImageCreateInfo imageInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D; imageInfo.extent.width = engine->swapchainExtent.width;
    imageInfo.extent.height = engine->swapchainExtent.height; imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1; imageInfo.arrayLayers = 1; imageInfo.format = engine->swapchainFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL; imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    if (vkCreateImage(engine->device, &imageInfo, nullptr, &engine->offscreenImage) != VK_SUCCESS) return false;
    VkMemoryRequirements memReq; vkGetImageMemoryRequirements(engine->device, engine->offscreenImage, &memReq);
    VkMemoryAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = memReq.size; allocInfo.memoryTypeIndex = findMemoryType(engine->physicalDevice, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(engine->device, &allocInfo, nullptr, &engine->offscreenImageMemory) != VK_SUCCESS) return false;
    vkBindImageMemory(engine->device, engine->offscreenImage, engine->offscreenImageMemory, 0);
    VkImageViewCreateInfo viewInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = engine->offscreenImage; viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D; viewInfo.format = engine->swapchainFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; viewInfo.subresourceRange.levelCount = 1; viewInfo.subresourceRange.layerCount = 1;
    if (vkCreateImageView(engine->device, &viewInfo, nullptr, &engine->offscreenImageView) != VK_SUCCESS) return false;
    VkSamplerCreateInfo samplerInfo = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.magFilter = VK_FILTER_LINEAR; samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    if (vkCreateSampler(engine->device, &samplerInfo, nullptr, &engine->offscreenSampler) != VK_SUCCESS) return false;
    VkFramebufferCreateInfo fbInfo = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fbInfo.renderPass = engine->offscreenRenderPass; fbInfo.attachmentCount = 1; fbInfo.pAttachments = &engine->offscreenImageView;
    fbInfo.width = engine->swapchainExtent.width; fbInfo.height = engine->swapchainExtent.height; fbInfo.layers = 1;
    if (vkCreateFramebuffer(engine->device, &fbInfo, nullptr, &engine->offscreenFramebuffer) != VK_SUCCESS) return false;
    return true;
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
void updateBlurDescriptorSet(Engine* engine) {
    VkDescriptorImageInfo imageInfo = {};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; imageInfo.imageView = engine->offscreenImageView; imageInfo.sampler = engine->offscreenSampler;
    VkWriteDescriptorSet descriptorWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    descriptorWrite.dstSet = engine->blurDescriptorSet; descriptorWrite.dstBinding = 0; descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; descriptorWrite.descriptorCount = 1; descriptorWrite.pImageInfo = &imageInfo;
    vkUpdateDescriptorSets(engine->device, 1, &descriptorWrite, 0, nullptr);
}
void recreateSwapchain(Engine* engine) {
    if (engine->device == VK_NULL_HANDLE || engine->app->window == nullptr) return;
    int width = ANativeWindow_getWidth(engine->app->window);
    int height = ANativeWindow_getHeight(engine->app->window);
    if (width == 0 || height == 0) return;
    vkDeviceWaitIdle(engine->device);
    for (auto fb : engine->framebuffers) if (fb != VK_NULL_HANDLE) vkDestroyFramebuffer(engine->device, fb, nullptr);
    engine->framebuffers.clear();
    if (!engine->commandBuffers.empty()) {
        vkFreeCommandBuffers(engine->device, engine->commandPool, static_cast<uint32_t>(engine->commandBuffers.size()), engine->commandBuffers.data());
        engine->commandBuffers.clear();
    }
    for (auto iv : engine->swapchainImageViews) if (iv != VK_NULL_HANDLE) vkDestroyImageView(engine->device, iv, nullptr);
    engine->swapchainImageViews.clear();
    if (engine->offscreenFramebuffer != VK_NULL_HANDLE) { vkDestroyFramebuffer(engine->device, engine->offscreenFramebuffer, nullptr); engine->offscreenFramebuffer = VK_NULL_HANDLE; }
    if (engine->offscreenSampler != VK_NULL_HANDLE) { vkDestroySampler(engine->device, engine->offscreenSampler, nullptr); engine->offscreenSampler = VK_NULL_HANDLE; }
    if (engine->offscreenImageView != VK_NULL_HANDLE) { vkDestroyImageView(engine->device, engine->offscreenImageView, nullptr); engine->offscreenImageView = VK_NULL_HANDLE; }
    if (engine->offscreenImage != VK_NULL_HANDLE) { vkDestroyImage(engine->device, engine->offscreenImage, nullptr); engine->offscreenImage = VK_NULL_HANDLE; }
    if (engine->offscreenImageMemory != VK_NULL_HANDLE) { vkFreeMemory(engine->device, engine->offscreenImageMemory, nullptr); engine->offscreenImageMemory = VK_NULL_HANDLE; }
    createSwapchain(engine);
    createFramebuffers(engine);
    createCommandBuffers(engine);
    createOffscreenResources(engine);
    updateBlurDescriptorSet(engine);
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
    samplerLayoutBinding.binding = 0; samplerLayoutBinding.descriptorCount = 1; samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
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
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; imageInfo.imageView = engine->fontImageView; imageInfo.sampler = engine->fontSampler;
    VkWriteDescriptorSet descriptorWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    descriptorWrite.dstSet = engine->descriptorSet; descriptorWrite.dstBinding = 0; descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; descriptorWrite.descriptorCount = 1; descriptorWrite.pImageInfo = &imageInfo;
    vkUpdateDescriptorSets(engine->device, 1, &descriptorWrite, 0, nullptr);
    VkDescriptorSetLayoutBinding blurBinding = {};
    blurBinding.binding = 0; blurBinding.descriptorCount = 1; blurBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; blurBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo blurLayoutInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    blurLayoutInfo.bindingCount = 1; blurLayoutInfo.pBindings = &blurBinding;
    if (vkCreateDescriptorSetLayout(engine->device, &blurLayoutInfo, nullptr, &engine->blurDescriptorSetLayout) != VK_SUCCESS) return false;
    VkDescriptorPoolSize blurPoolSize = {};
    blurPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; blurPoolSize.descriptorCount = 1;
    VkDescriptorPoolCreateInfo blurPoolInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    blurPoolInfo.poolSizeCount = 1; blurPoolInfo.pPoolSizes = &blurPoolSize; blurPoolInfo.maxSets = 1;
    if (vkCreateDescriptorPool(engine->device, &blurPoolInfo, nullptr, &engine->blurDescriptorPool) != VK_SUCCESS) return false;
    VkDescriptorSetAllocateInfo blurAllocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    blurAllocInfo.descriptorPool = engine->blurDescriptorPool; blurAllocInfo.descriptorSetCount = 1; blurAllocInfo.pSetLayouts = &engine->blurDescriptorSetLayout;
    if (vkAllocateDescriptorSets(engine->device, &blurAllocInfo, &engine->blurDescriptorSet) != VK_SUCCESS) return false;
    updateBlurDescriptorSet(engine);
    return true;
}
bool createPipelineLayout(Engine* engine) {
    VkPushConstantRange pushConstantRange = {};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT; pushConstantRange.offset = 0; pushConstantRange.size = sizeof(PushConstants);
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineLayoutInfo.setLayoutCount = 1; pipelineLayoutInfo.pSetLayouts = &engine->descriptorSetLayout; pipelineLayoutInfo.pushConstantRangeCount = 1; pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    if (vkCreatePipelineLayout(engine->device, &pipelineLayoutInfo, nullptr, &engine->pipelineLayout) != VK_SUCCESS) return false;
    VkPushConstantRange blurPushRange = {};
    blurPushRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT; blurPushRange.offset = 0; blurPushRange.size = sizeof(BlurPushConstants);
    VkPipelineLayoutCreateInfo blurLayoutInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    blurLayoutInfo.setLayoutCount = 1; blurLayoutInfo.pSetLayouts = &engine->blurDescriptorSetLayout; blurLayoutInfo.pushConstantRangeCount = 1; blurLayoutInfo.pPushConstantRanges = &blurPushRange;
    if (vkCreatePipelineLayout(engine->device, &blurLayoutInfo, nullptr, &engine->blurPipelineLayout) != VK_SUCCESS) return false;
    return true;
}
std::vector<uint32_t> loadShaderAsset(Engine* engine, const char* filename) {
    AAsset* asset = AAssetManager_open(engine->app->activity->assetManager, filename, AASSET_MODE_BUFFER);
    if (!asset) return {};
    size_t size = AAsset_getLength(asset); std::vector<uint32_t> buffer(size / sizeof(uint32_t)); AAsset_read(asset, buffer.data(), size); AAsset_close(asset); return buffer;
}
VkShaderModule createShaderModule(Engine* engine, const std::vector<uint32_t>& code) {
    VkShaderModuleCreateInfo createInfo = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    createInfo.codeSize = code.size() * sizeof(uint32_t); createInfo.pCode = code.data(); VkShaderModule shaderModule; vkCreateShaderModule(engine->device, &createInfo, nullptr, &shaderModule); return shaderModule;
}
std::vector<uint8_t> loadAsset(Engine* engine, const char* filename) {
    AAsset* asset = AAssetManager_open(engine->app->activity->assetManager, filename, AASSET_MODE_BUFFER);
    if (!asset) return {};
    size_t size = AAsset_getLength(asset); std::vector<uint8_t> buffer(size); AAsset_read(asset, buffer.data(), size); AAsset_close(asset); return buffer;
}
bool createGraphicsPipeline(Engine* engine) {
    auto vertCode = loadShaderAsset(engine, "shaders/text.vert.spv");
    auto fragCode = loadShaderAsset(engine, "shaders/text.frag.spv");
    if (vertCode.empty() || fragCode.empty()) return false;
    VkShaderModule vertModule = createShaderModule(engine, vertCode); VkShaderModule fragModule = createShaderModule(engine, fragCode);
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
    colorBlendAttachment.blendEnable = VK_TRUE; colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
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
    pipelineInfo.layout = engine->pipelineLayout; pipelineInfo.renderPass = engine->offscreenRenderPass; pipelineInfo.subpass = 0;
    if (vkCreateGraphicsPipelines(engine->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &engine->graphicsPipeline) != VK_SUCCESS) return false;
    vkDestroyShaderModule(engine->device, fragModule, nullptr); vkDestroyShaderModule(engine->device, vertModule, nullptr);
    auto blurVertCode = loadShaderAsset(engine, "shaders/blur.vert.spv");
    auto blurFragCode = loadShaderAsset(engine, "shaders/blur.frag.spv");
    if (blurVertCode.empty() || blurFragCode.empty()) return false;
    VkShaderModule blurVertModule = createShaderModule(engine, blurVertCode);
    VkShaderModule blurFragModule = createShaderModule(engine, blurFragCode);
    VkPipelineShaderStageCreateInfo blurStages[] = {
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, blurVertModule, "main", nullptr},
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, blurFragModule, "main", nullptr}
    };
    VkPipelineVertexInputStateCreateInfo emptyVertexInput = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkGraphicsPipelineCreateInfo blurPipelineInfo = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    blurPipelineInfo.stageCount = 2; blurPipelineInfo.pStages = blurStages; blurPipelineInfo.pVertexInputState = &emptyVertexInput;
    blurPipelineInfo.pInputAssemblyState = &inputAssembly; blurPipelineInfo.pViewportState = &viewportState;
    blurPipelineInfo.pRasterizationState = &rasterizer; blurPipelineInfo.pMultisampleState = &multisampling;
    VkPipelineColorBlendAttachmentState blurBlend = {};
    blurBlend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT; blurBlend.blendEnable = VK_FALSE;
    VkPipelineColorBlendStateCreateInfo blurColorBlending = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    blurColorBlending.attachmentCount = 1; blurColorBlending.pAttachments = &blurBlend;
    blurPipelineInfo.pColorBlendState = &blurColorBlending; blurPipelineInfo.pDynamicState = &dynamicState;
    blurPipelineInfo.layout = engine->blurPipelineLayout; blurPipelineInfo.renderPass = engine->renderPass; blurPipelineInfo.subpass = 0;
    if (vkCreateGraphicsPipelines(engine->device, VK_NULL_HANDLE, 1, &blurPipelineInfo, nullptr, &engine->blurPipeline) != VK_SUCCESS) return false;
    vkDestroyShaderModule(engine->device, blurFragModule, nullptr); vkDestroyShaderModule(engine->device, blurVertModule, nullptr);
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
    if (!createOffscreenRenderPass(engine)) return false;
    if (!createOffscreenResources(engine)) return false;
    if (!createFramebuffers(engine)) return false;
    if (!createCommandBuffers(engine)) return false;
    if (!createSyncObjects(engine)) return false;
    {
        uint32_t imageIndex;
        vkAcquireNextImageKHR(engine->device, engine->swapchain, UINT64_MAX, engine->imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
        VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        vkBeginCommandBuffer(engine->commandBuffers[imageIndex], &beginInfo);
        VkRenderPassBeginInfo offRpb = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        offRpb.renderPass = engine->offscreenRenderPass; offRpb.framebuffer = engine->offscreenFramebuffer; offRpb.renderArea.extent = engine->swapchainExtent;
        VkClearValue clearColor = {{{engine->bgColor[0], engine->bgColor[1], engine->bgColor[2], engine->bgColor[3]}}};
        offRpb.clearValueCount = 1; offRpb.pClearValues = &clearColor;
        vkCmdBeginRenderPass(engine->commandBuffers[imageIndex], &offRpb, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdEndRenderPass(engine->commandBuffers[imageIndex]);
        VkRenderPassBeginInfo rpBegin = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rpBegin.renderPass = engine->renderPass; rpBegin.framebuffer = engine->framebuffers[imageIndex]; rpBegin.renderArea.extent = engine->swapchainExtent;
        VkClearValue blackClear = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
        rpBegin.clearValueCount = 1; rpBegin.pClearValues = &blackClear;
        vkCmdBeginRenderPass(engine->commandBuffers[imageIndex], &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
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
        vkQueuePresentKHR(engine->graphicsQueue, &present);
        vkQueueWaitIdle(engine->graphicsQueue);
    }
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
            size_t size = file.tellg(); std::vector<uint8_t> buffer(size); file.seekg(0, std::ios::beg); file.read((char*)buffer.data(), size);
            engine->fallbackFontData.push_back(std::move(buffer));
            FT_Face face;
            if (FT_New_Memory_Face(engine->ftLibrary, engine->fallbackFontData.back().data(), size, 0, &face) == 0) {
                FT_Set_Pixel_Sizes(face, 0, 48); engine->fallbackFaces.push_back(face); engine->fallbackHbFonts.push_back(hb_ft_font_create(face, nullptr)); engine->fallbackFontScales.push_back(1.0f);
            } else engine->fallbackFontData.pop_back();
        }
    }
    std::vector<uint8_t> emojiBuffer = loadAsset(engine, "fonts/NotoColorEmoji.ttf");
    if (!emojiBuffer.empty()) {
        engine->fallbackFontData.push_back(std::move(emojiBuffer));
        FT_Face face = nullptr;
        if (FT_New_Memory_Face(engine->ftLibrary, engine->fallbackFontData.back().data(), engine->fallbackFontData.back().size(), 0, &face) == 0) {
            float emScale = 1.0f;
            if (FT_HAS_FIXED_SIZES(face)) { FT_Select_Size(face, 0); if (face->size->metrics.y_ppem > 0) emScale = 48.0f / (float)face->size->metrics.y_ppem; }
            else FT_Set_Pixel_Sizes(face, 0, 48);
            engine->fallbackFaces.push_back(face);
            if (face != nullptr) { engine->fallbackHbFonts.push_back(hb_ft_font_create(face, nullptr)); engine->fallbackFontScales.push_back(emScale); }
        } else engine->fallbackFontData.pop_back();
    }
    if (!createTextTexture(engine)) return false;
    if (!createDescriptors(engine)) return false;
    if (!createPipelineLayout(engine)) return false;
    if (!createGraphicsPipeline(engine)) return false;
    return true;
}
std::string GetResString(Engine* engine, const char* resName) {
    if (!engine || !engine->app || !engine->app->activity || !engine->app->activity->vm) return resName;
    JNIEnv* env = nullptr; JavaVM* vm = engine->app->activity->vm; bool needDetach = false;
    if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) == JNI_EDETACHED) { if (vm->AttachCurrentThread(&env, nullptr) != 0) return resName; needDetach = true; }
    if (!env) return resName;
    jclass activityClass = env->GetObjectClass(engine->app->activity->clazz);
    jmethodID getStrMethod = env->GetMethodID(activityClass, "getStringResourceByName", "(Ljava/lang/String;)Ljava/lang/String;");
    std::string result = resName;
    if (getStrMethod) {
        jstring jResName = env->NewStringUTF(resName); jstring jResult = (jstring)env->CallObjectMethod(engine->app->activity->clazz, getStrMethod, jResName);
        if (jResult) { const char* utf8Str = env->GetStringUTFChars(jResult, nullptr); if (utf8Str) { result = utf8Str; env->ReleaseStringUTFChars(jResult, utf8Str); } env->DeleteLocalRef(jResult); }
        env->DeleteLocalRef(jResName);
    }
    env->DeleteLocalRef(activityClass);
    if (needDetach) vm->DetachCurrentThread();
    return result;
}
void updateTextVertices(Engine* engine) {
    float scale = engine->currentFontSize / 48.0f; float baselineOffset = engine->lineHeight * 0.8f; engine->maxLineWidth = engine->gutterWidth;
    std::vector<Vertex> bgVertices; std::vector<Vertex> lineVertices; std::vector<Vertex> charVertices; std::vector<Vertex> cursorVertices;
    float whiteU = 1.0f / engine->atlas.width; float whiteV = 1.0f / engine->atlas.height;
    auto addRect = [&](std::vector<Vertex>& verts, float rx, float ry, float rw, float rh, float r, float g, float b, float a) {
        verts.push_back({{rx, ry}, {whiteU, whiteV}, 0.0f, {r, g, b, a}}); verts.push_back({{rx, ry + rh}, {whiteU, whiteV}, 0.0f, {r, g, b, a}});
        verts.push_back({{rx + rw, ry}, {whiteU, whiteV}, 0.0f, {r, g, b, a}}); verts.push_back({{rx + rw, ry}, {whiteU, whiteV}, 0.0f, {r, g, b, a}});
        verts.push_back({{rx, ry + rh}, {whiteU, whiteV}, 0.0f, {r, g, b, a}}); verts.push_back({{rx + rw, ry + rh}, {whiteU, whiteV}, 0.0f, {r, g, b, a}});
    };
    float textR = engine->textColor[0], textG = engine->textColor[1], textB = engine->textColor[2], textA = engine->textColor[3];
    float cursorWidth = std::max(2.0f, 4.0f * scale);
    std::vector<std::pair<size_t, size_t>> searchMatches;
    if (!engine->searchQuery.empty()) {
        size_t docLen = engine->pt.length();
        if (engine->searchRegex) {
            std::string fullText = engine->pt.getRange(0, docLen); std::string actualQuery = preprocessRegexQuery(engine->searchQuery);
            try {
                std::regex_constants::syntax_option_type rxFlags = std::regex_constants::ECMAScript; if (!engine->searchMatchCase) rxFlags |= std::regex_constants::icase;
                std::regex re(actualQuery, rxFlags); bool startsWithCaret = (!engine->searchQuery.empty() && engine->searchQuery[0] == '^'); bool endsWithDollar = (!engine->searchQuery.empty() && engine->searchQuery.back() == '$');
                auto searchStart = fullText.cbegin(); std::smatch m; size_t currentOffset = 0; std::regex_constants::match_flag_type flags = std::regex_constants::match_default;
                while (std::regex_search(searchStart, fullText.cend(), m, re, flags)) {
                    size_t matchPos = currentOffset + m.position(); size_t matchLen = m.length(); size_t anchorLen = 0;
                    if (startsWithCaret && m.size() > 1 && m[1].matched) anchorLen = m.length(1);
                    size_t contentPos = matchPos + anchorLen; size_t contentLen = matchLen - anchorLen; bool shouldHighlight = true;
                    if (endsWithDollar) {
                        bool isAtLineEnd = false;
                        if (contentPos + contentLen >= fullText.size()) isAtLineEnd = true;
                        else {
                            char c = fullText[contentPos + contentLen];
                            if (c == '\r' || c == '\n') { isAtLineEnd = true; if (contentLen == 0 && c == '\n' && contentPos + contentLen > 0 && fullText[contentPos + contentLen - 1] == '\r') isAtLineEnd = false; }
                        }
                        if (!isAtLineEnd) shouldHighlight = false;
                        if (shouldHighlight && contentPos > 0 && contentPos < fullText.size()) { if (fullText[contentPos] == '\n' && fullText[contentPos - 1] == '\r') shouldHighlight = false; }
                    }
                    if (shouldHighlight && startsWithCaret && endsWithDollar && contentLen == 0) {
                        size_t globalPos = contentPos;
                        if (globalPos == fullText.size() && !fullText.empty()) { bool isAfterNewline = false; if (globalPos > 0) { char c = fullText[globalPos - 1]; if (c == '\r' || c == '\n') isAfterNewline = true; } if (!isAfterNewline) shouldHighlight = false; }
                    }
                    if (shouldHighlight && !searchMatches.empty()) { const auto& last = searchMatches.back(); if (last.first == contentPos && (last.second - last.first) == 0 && contentLen == 0) shouldHighlight = false; }
                    if (shouldHighlight) searchMatches.push_back({ contentPos, contentPos + contentLen });
                    size_t step = matchLen;
                    if (step == 0) { bool isBolCaret = (startsWithCaret && !(flags & std::regex_constants::match_not_bol)); if (isBolCaret) step = 0; else step = 1; }
                    if (step == 0 && (flags & std::regex_constants::match_not_bol)) step = 1;
                    size_t relativeAdvance = m.position() + step; size_t remaining = std::distance(searchStart, fullText.cend());
                    if (relativeAdvance > remaining) break;
                    std::advance(searchStart, relativeAdvance); currentOffset += relativeAdvance; flags |= std::regex_constants::match_not_bol;
                }
            } catch (...) {}
        } else {
            size_t currentSearchPos = 0;
            while (currentSearchPos <= docLen) {
                size_t matchLen = 0; size_t found = findText(engine, currentSearchPos, engine->searchQuery, true, engine->searchMatchCase, engine->searchWholeWord, false, &matchLen);
                if (found == std::string::npos || found < currentSearchPos) break;
                searchMatches.push_back({found, found + matchLen});
                if (matchLen == 0) currentSearchPos = found + 1; else currentSearchPos = found + matchLen;
            }
        }
    }
    std::vector<std::pair<size_t, size_t>> autoMatches; auto [autoStr, isWholeWord] = getHighlightTarget(engine);
    if (!autoStr.empty() && autoStr != engine->searchQuery) {
        size_t currentSearchPos = 0; size_t docLen = engine->pt.length();
        while (currentSearchPos <= docLen) {
            size_t matchLen = 0; size_t found = findText(engine, currentSearchPos, autoStr, true, true, isWholeWord, false, &matchLen);
            if (found == std::string::npos || found < currentSearchPos) break;
            autoMatches.push_back({found, found + matchLen});
            if (matchLen == 0) currentSearchPos = found + 1; else currentSearchPos = found + matchLen;
        }
    }
    float winH = 5000.0f; float winW = 1080.0f;
    if (engine->app->window != nullptr) { winH = (float)ANativeWindow_getHeight(engine->app->window); winW = (float)ANativeWindow_getWidth(engine->app->window); }
    for (int lineIdx = 0; lineIdx < engine->lineStarts.size(); ++lineIdx) {
        float lineY = engine->topMargin + baselineOffset - engine->scrollY + lineIdx * engine->lineHeight;
        if (lineY < -engine->lineHeight || lineY > winH + engine->lineHeight) continue;
        if (lineIdx >= engine->lineCaches.size()) break;
        ensureLineShaped(engine, lineIdx);
        float x = engine->gutterWidth - engine->scrollX; size_t lineStart = engine->lineStarts[lineIdx]; size_t lineEnd = (lineIdx + 1 < engine->lineStarts.size()) ? engine->lineStarts[lineIdx + 1] : engine->pt.length();
        for (const auto& match : searchMatches) {
            if (match.first == match.second) {
                if (match.first >= lineStart && (lineIdx + 1 >= engine->lineStarts.size() || match.first < engine->lineStarts[lineIdx + 1])) {
                    float drawX = engine->gutterWidth;
                    if (match.first > lineStart) { float s = engine->currentFontSize / 48.0f; for (const auto &sg: engine->lineCaches[lineIdx].glyphs) { if (sg.cluster >= match.first) break; drawX += sg.xAdvance * s; } }
                    drawX -= engine->scrollX; float bgY = lineY - engine->lineHeight * 0.8f;
                    addRect(bgVertices, drawX, bgY, engine->charWidth, engine->lineHeight, 1.0f, 1.0f, 0.0f, 0.4f);
                }
            }
        }
        for (const auto& sg : engine->lineCaches[lineIdx].glyphs) {
            uint64_t key = ((uint64_t)sg.fontIndex << 32) | sg.glyphIndex;
            if (engine->atlas.glyphs.count(key) == 0) engine->atlas.loadGlyph(engine, sg.fontIndex, sg.glyphIndex);

            bool isSelected = false;
            // 【変更】全てのカーソルをループして選択状態を判定
            for (const auto& cur : engine->cursors) {
                if (cur.hasSelection() && sg.cluster >= cur.start() && sg.cluster < cur.end()) {
                    isSelected = true; break;
                }
            }

            bool isSearchResult = false;
            for (const auto& match : searchMatches) { if (match.first < match.second && sg.cluster >= match.first && sg.cluster < match.second) { isSearchResult = true; break; } }
            bool isAutoHlResult = false;
            for (const auto& match : autoMatches) { if (match.first < match.second && sg.cluster >= match.first && sg.cluster < match.second) { isAutoHlResult = true; break; } }

            if (sg.isIME) {
                float bgY = lineY - engine->lineHeight * 0.8f; addRect(bgVertices, x, bgY, sg.xAdvance * scale, engine->lineHeight, 0.2f, 0.6f, 1.0f, 0.3f);
                float underY = lineY + engine->lineHeight * 0.1f; addRect(lineVertices, x, underY, sg.xAdvance * scale, 2.0f, textR, textG, textB, 1.0f);
            } else if (isSelected) {
                float bgY = lineY - engine->lineHeight * 0.8f; addRect(bgVertices, x, bgY, sg.xAdvance * scale, engine->lineHeight, engine->selColor[0], engine->selColor[1], engine->selColor[2], engine->selColor[3]);
            } else if (isSearchResult) {
                float bgY = lineY - engine->lineHeight * 0.8f; addRect(bgVertices, x, bgY, sg.xAdvance * scale, engine->lineHeight, 1.0f, 1.0f, 0.0f, 0.4f);
            } else if (isAutoHlResult) {
                float bgY = lineY - engine->lineHeight * 0.8f; addRect(bgVertices, x, bgY, sg.xAdvance * scale, engine->lineHeight, engine->autoHlColor[0], engine->autoHlColor[1], engine->autoHlColor[2], engine->autoHlColor[3]);
            }

            if (engine->atlas.glyphs.count(key) > 0) {
                GlyphInfo& info = engine->atlas.glyphs[key]; float isColorFlag = info.isColor ? 1.0f : 0.0f;
                float xpos = x + sg.xOffset * scale + info.bearingX * scale; float ypos = lineY - sg.yOffset * scale - info.bearingY * scale; float w = info.width * scale; float h = info.height * scale;
                charVertices.push_back({{xpos, ypos}, {info.u0, info.v0}, isColorFlag, {textR, textG, textB, textA}}); charVertices.push_back({{xpos, ypos + h}, {info.u0, info.v1}, isColorFlag, {textR, textG, textB, textA}});
                charVertices.push_back({{xpos + w, ypos}, {info.u1, info.v0}, isColorFlag, {textR, textG, textB, textA}}); charVertices.push_back({{xpos + w, ypos}, {info.u1, info.v0}, isColorFlag, {textR, textG, textB, textA}});
                charVertices.push_back({{xpos, ypos + h}, {info.u0, info.v1}, isColorFlag, {textR, textG, textB, textA}}); charVertices.push_back({{xpos + w, ypos + h}, {info.u1, info.v1}, isColorFlag, {textR, textG, textB, textA}});
            }
            x += sg.xAdvance * scale; float absoluteX = x + engine->scrollX; if (absoluteX > engine->maxLineWidth) engine->maxLineWidth = absoluteX;
        }
        if (lineEnd > lineStart) {
            char lastChar = engine->pt.charAt(lineEnd - 1);
            if (lastChar == '\n' || lastChar == '\r') {
                size_t newlinePos = lineEnd - 1; uint64_t nlKey = 0xFFFFFFFFFFFFFFFD; bool skipDraw = false;
                if (lastChar == '\n') { if (newlinePos > lineStart && engine->pt.charAt(newlinePos - 1) == '\r') { newlinePos--; nlKey = 0xFFFFFFFFFFFFFFFF; } else nlKey = 0xFFFFFFFFFFFFFFFD; }
                else if (lastChar == '\r') { if (lineEnd < engine->pt.length() && engine->pt.charAt(lineEnd) == '\n') skipDraw = true; else nlKey = 0xFFFFFFFFFFFFFFFE; }
                if (!skipDraw) {
                    bool isNewlineSelected = false;
                    // 【変更】全てのカーソルをループして改行の選択状態を判定
                    for (const auto& cur : engine->cursors) {
                        if (cur.hasSelection() && newlinePos >= cur.start() && newlinePos < cur.end()) {
                            isNewlineSelected = true; break;
                        }
                    }
                    bool isNewlineMatched = false; for (const auto& match : searchMatches) { if (match.first < match.second && match.first <= newlinePos && match.second > newlinePos) { isNewlineMatched = true; break; } }
                    bool isNewlineAutoMatched = false; for (const auto& match : autoMatches) { if (match.first < match.second && match.first <= newlinePos && match.second > newlinePos) { isNewlineAutoMatched = true; break; } }
                    if (isNewlineSelected) { float bgY = lineY - engine->lineHeight * 0.8f; addRect(bgVertices, x, bgY, engine->charWidth, engine->lineHeight, engine->selColor[0], engine->selColor[1], engine->selColor[2], engine->selColor[3]); }
                    else if (isNewlineMatched) { float bgY = lineY - engine->lineHeight * 0.8f; addRect(bgVertices, x, bgY, engine->charWidth, engine->lineHeight, 1.0f, 1.0f, 0.0f, 0.4f); }
                    else if (isNewlineAutoMatched) { float bgY = lineY - engine->lineHeight * 0.8f; addRect(bgVertices, x, bgY, engine->charWidth, engine->lineHeight, engine->autoHlColor[0], engine->autoHlColor[1], engine->autoHlColor[2], engine->autoHlColor[3]); }
                    if (engine->atlas.glyphs.count(nlKey) > 0) {
                        GlyphInfo& info = engine->atlas.glyphs[nlKey]; float nlR = engine->textColor[0], nlG = engine->textColor[1], nlB = engine->textColor[2], nlA = engine->textColor[3] * 0.3f;
                        float targetSize = engine->charWidth * 1.0f; float iconScale = targetSize / info.width; float w = info.width * iconScale, h = info.height * iconScale;
                        float xpos = x + (engine->charWidth - w) * 0.5f; float rowCenterY = engine->topMargin - engine->scrollY + lineIdx * engine->lineHeight + engine->lineHeight * 0.5f; float ypos = rowCenterY - h * 0.5f;
                        charVertices.push_back({{xpos, ypos}, {info.u0, info.v0}, 0.0f, {nlR, nlG, nlB, nlA}}); charVertices.push_back({{xpos, ypos + h}, {info.u0, info.v1}, 0.0f, {nlR, nlG, nlB, nlA}});
                        charVertices.push_back({{xpos + w, ypos}, {info.u1, info.v0}, 0.0f, {nlR, nlG, nlB, nlA}}); charVertices.push_back({{xpos + w, ypos}, {info.u1, info.v0}, 0.0f, {nlR, nlG, nlB, nlA}});
                        charVertices.push_back({{xpos, ypos + h}, {info.u0, info.v1}, 0.0f, {nlR, nlG, nlB, nlA}}); charVertices.push_back({{xpos + w, ypos + h}, {info.u1, info.v1}, 0.0f, {nlR, nlG, nlB, nlA}});
                    }
                }
            }
        }
        for (const auto& c : engine->cursors) {
            size_t vPos = c.head;
            if (vPos >= lineStart && (lineIdx + 1 >= engine->lineStarts.size() || vPos < engine->lineStarts[lineIdx + 1])) {
                float cx = getXFromPos(engine, vPos); float curY = lineY - engine->lineHeight * 0.8f;
                addRect(cursorVertices, cx - engine->scrollX, curY, cursorWidth, engine->lineHeight, engine->caretColor[0], engine->caretColor[1], engine->caretColor[2], engine->caretColor[3]);
            }
        }
    }
    std::vector<Vertex> gutterBgVertices; std::vector<Vertex> gutterTextVertices;
    addRect(gutterBgVertices, 0.0f, 0.0f, engine->gutterWidth, winH, engine->gutterBgColor[0], engine->gutterBgColor[1], engine->gutterBgColor[2], engine->gutterBgColor[3]);
    float gutterTextR = engine->gutterTextColor[0], gutterTextG = engine->gutterTextColor[1], gutterTextB = engine->gutterTextColor[2];
    for (int i = 0; i < engine->lineStarts.size(); ++i) {
        float lineTop = engine->topMargin - engine->scrollY + i * engine->lineHeight;
        if (lineTop + engine->lineHeight < 0.0f || lineTop > winH) continue;
        std::string lineNumStr = std::to_string(i + 1); float numWidth = 0.0f;
        for(char c : lineNumStr) {
            uint32_t cp = c; int fontIdx = getFontIndexForChar(engine, cp, 0); uint32_t glyphIdx = FT_Get_Char_Index(engine->fallbackFaces[fontIdx], cp); uint64_t key = ((uint64_t)fontIdx << 32) | glyphIdx;
            if (engine->atlas.glyphs.count(key) == 0 && !engine->fallbackFaces.empty()) engine->atlas.loadGlyph(engine, fontIdx, glyphIdx);
            if (engine->atlas.glyphs.count(key) > 0) numWidth += engine->atlas.glyphs[key].advance * scale; else numWidth += engine->charWidth;
        }
        float rightMargin = engine->charWidth * 0.5f; float numX = engine->gutterWidth - rightMargin - numWidth; if (numX < rightMargin * 0.5f) numX = rightMargin * 0.5f;
        float lineY = lineTop + baselineOffset;
        for(char c : lineNumStr) {
            uint32_t cp = c; int fontIdx = getFontIndexForChar(engine, cp, 0); uint32_t glyphIdx = FT_Get_Char_Index(engine->fallbackFaces[fontIdx], cp); uint64_t key = ((uint64_t)fontIdx << 32) | glyphIdx;
            if (engine->atlas.glyphs.count(key) > 0) {
                GlyphInfo& info = engine->atlas.glyphs[key]; float xpos = numX + info.bearingX * scale; float ypos = lineY - info.bearingY * scale; float w = info.width * scale; float h = info.height * scale;
                gutterTextVertices.push_back({{xpos, ypos}, {info.u0, info.v0}, 0.0f, {gutterTextR, gutterTextG, gutterTextB, 1.0f}}); gutterTextVertices.push_back({{xpos, ypos + h}, {info.u0, info.v1}, 0.0f, {gutterTextR, gutterTextG, gutterTextB, 1.0f}});
                gutterTextVertices.push_back({{xpos + w, ypos}, {info.u1, info.v0}, 0.0f, {gutterTextR, gutterTextG, gutterTextB, 1.0f}}); gutterTextVertices.push_back({{xpos + w, ypos}, {info.u1, info.v0}, 0.0f, {gutterTextR, gutterTextG, gutterTextB, 1.0f}});
                gutterTextVertices.push_back({{xpos, ypos + h}, {info.u0, info.v1}, 0.0f, {gutterTextR, gutterTextG, gutterTextB, 1.0f}}); gutterTextVertices.push_back({{xpos + w, ypos + h}, {info.u1, info.v1}, 0.0f, {gutterTextR, gutterTextG, gutterTextB, 1.0f}});
                numX += info.advance * scale;
            } else numX += engine->charWidth;
        }
    }
    std::vector<Vertex> vertices;
    vertices.insert(vertices.end(), bgVertices.begin(), bgVertices.end()); vertices.insert(vertices.end(), lineVertices.begin(), lineVertices.end());
    vertices.insert(vertices.end(), charVertices.begin(), charVertices.end()); vertices.insert(vertices.end(), cursorVertices.begin(), cursorVertices.end());
    vertices.insert(vertices.end(), gutterBgVertices.begin(), gutterBgVertices.end()); vertices.insert(vertices.end(), gutterTextVertices.begin(), gutterTextVertices.end());
    float visibleH = winH - engine->bottomInset; float contentH = engine->lineStarts.size() * engine->lineHeight + engine->topMargin + engine->lineHeight;
    float maxScrollY = std::max(0.0f, contentH - visibleH); float maxScrollX = std::max(0.0f, engine->maxLineWidth - winW + engine->charWidth * 2.0f);
    float sbThickness = 12.0f; float minBarLen = 40.0f; float sbR = engine->isDarkMode ? 1.0f : 0.0f; float sbG = engine->isDarkMode ? 1.0f : 0.0f; float sbB = engine->isDarkMode ? 1.0f : 0.0f; float sbA = 0.25f;
    if (maxScrollY > 0.0f) {
        float displayAreaH = visibleH - engine->topMargin; float viewportRatio = std::min(1.0f, displayAreaH / (contentH - engine->topMargin));
        float barHeight = std::max(minBarLen, displayAreaH * viewportRatio); float scrollRatio = engine->scrollY / maxScrollY; float barY = engine->topMargin + scrollRatio * (displayAreaH - barHeight);
        addRect(vertices, winW - sbThickness - 4.0f, barY, sbThickness, barHeight, sbR, sbG, sbB, sbA);
    }
    if (maxScrollX > 0.0f) {
        float displayAreaW = winW - engine->gutterWidth; float contentW = displayAreaW + maxScrollX; float viewportRatio = std::min(1.0f, displayAreaW / contentW);
        float barWidth = std::max(minBarLen, displayAreaW * viewportRatio); float scrollRatio = engine->scrollX / maxScrollX; float barX = engine->gutterWidth + scrollRatio * (displayAreaW - barWidth);
        addRect(vertices, barX, visibleH - sbThickness - 4.0f, barWidth, sbThickness, sbR, sbG, sbB, sbA);
    }
    engine->vertexCount = static_cast<uint32_t>(vertices.size());
    if (engine->vertexCount == 0) return;
    if (engine->vertexCount > 1000000) engine->vertexCount = 1000000;
    void* data; vkMapMemory(engine->device, engine->vertexBufferMemory, 0, sizeof(Vertex) * engine->vertexCount, 0, &data);
    memcpy(data, vertices.data(), sizeof(Vertex) * engine->vertexCount); vkUnmapMemory(engine->device, engine->vertexBufferMemory);
}
void renderFrame(Engine* engine) {
    if (engine->device == VK_NULL_HANDLE || !engine->isWindowReady) return;
    vkWaitForFences(engine->device, 1, &engine->inFlightFence, VK_TRUE, UINT64_MAX);
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(engine->device, engine->swapchain, UINT64_MAX, engine->imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) { recreateSwapchain(engine); return; } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) return;
    vkResetFences(engine->device, 1, &engine->inFlightFence);
    vkResetCommandBuffer(engine->commandBuffers[imageIndex], 0);
    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(engine->commandBuffers[imageIndex], &beginInfo);
    {
        std::lock_guard<std::mutex> lock(g_imeMutex);
        updateTextVertices(engine);
        if (engine->atlas.isDirty) {
            vkDeviceWaitIdle(engine->device); uint32_t dy = engine->atlas.dirtyMinY; uint32_t dh = engine->atlas.dirtyMaxY - engine->atlas.dirtyMinY; uint32_t dw = engine->atlas.width;
            if (dh > 0) {
                void* mapData = nullptr;
                if (vkMapMemory(engine->device, engine->atlasStagingMemory, 0, engine->atlas.width * engine->atlas.height * 4, 0, &mapData) == VK_SUCCESS) {
                    uint8_t* dst = (uint8_t*)mapData; const uint8_t* src = engine->atlas.pixels.data();
                    for (uint32_t row = 0; row < dh; ++row) memcpy(dst + ((dy + row) * dw) * 4, src + ((dy + row) * engine->atlas.width) * 4, dw * 4);
                    vkUnmapMemory(engine->device, engine->atlasStagingMemory);
                    VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER}; barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; barrier.image = engine->fontImage;
                    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; barrier.subresourceRange.levelCount = 1; barrier.subresourceRange.layerCount = 1;
                    barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT; barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                    vkCmdPipelineBarrier(engine->commandBuffers[imageIndex], VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
                    VkBufferImageCopy region = {}; region.bufferOffset = dy * dw * 4; region.bufferRowLength = 0; region.bufferImageHeight = 0;
                    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; region.imageSubresource.layerCount = 1; region.imageOffset = {0, (int32_t)dy, 0}; region.imageExtent = {dw, dh, 1};
                    vkCmdCopyBufferToImage(engine->commandBuffers[imageIndex], engine->atlasStagingBuffer, engine->fontImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
                    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                    vkCmdPipelineBarrier(engine->commandBuffers[imageIndex], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
                }
            }
            engine->atlas.isDirty = false;
        }
    }
    VkRenderPassBeginInfo offscreenRpBegin = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    offscreenRpBegin.renderPass = engine->offscreenRenderPass; offscreenRpBegin.framebuffer = engine->offscreenFramebuffer; offscreenRpBegin.renderArea.extent = engine->swapchainExtent;
    VkClearValue clearColor = {{{engine->bgColor[0], engine->bgColor[1], engine->bgColor[2], engine->bgColor[3]}}}; offscreenRpBegin.clearValueCount = 1; offscreenRpBegin.pClearValues = &clearColor;
    vkCmdBeginRenderPass(engine->commandBuffers[imageIndex], &offscreenRpBegin, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(engine->commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, engine->graphicsPipeline);
    VkViewport viewport = {0.0f, 0.0f, (float)engine->swapchainExtent.width, (float)engine->swapchainExtent.height, 0.0f, 1.0f}; vkCmdSetViewport(engine->commandBuffers[imageIndex], 0, 1, &viewport);
    VkRect2D scissor = {{0, 0}, engine->swapchainExtent}; vkCmdSetScissor(engine->commandBuffers[imageIndex], 0, 1, &scissor);
    if (engine->vertexCount > 0) {
        VkDeviceSize offsets[] = {0}; vkCmdBindVertexBuffers(engine->commandBuffers[imageIndex], 0, 1, &engine->vertexBuffer, offsets);
        vkCmdBindDescriptorSets(engine->commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, engine->pipelineLayout, 0, 1, &engine->descriptorSet, 0, nullptr);
        PushConstants pc; pc.screenWidth = (float)engine->swapchainExtent.width; pc.screenHeight = (float)engine->swapchainExtent.height;
        pc.color[0] = engine->textColor[0]; pc.color[1] = engine->textColor[1]; pc.color[2] = engine->textColor[2]; pc.color[3] = engine->textColor[3];
        vkCmdPushConstants(engine->commandBuffers[imageIndex], engine->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pc);
        vkCmdDraw(engine->commandBuffers[imageIndex], engine->vertexCount, 1, 0, 0);
    }
    vkCmdEndRenderPass(engine->commandBuffers[imageIndex]);
    VkRenderPassBeginInfo swapchainRpBegin = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    swapchainRpBegin.renderPass = engine->renderPass; swapchainRpBegin.framebuffer = engine->framebuffers[imageIndex]; swapchainRpBegin.renderArea.extent = engine->swapchainExtent;
    VkClearValue blackClear = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    swapchainRpBegin.clearValueCount = 1; swapchainRpBegin.pClearValues = &blackClear;
    vkCmdBeginRenderPass(engine->commandBuffers[imageIndex], &swapchainRpBegin, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(engine->commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, engine->blurPipeline);
    vkCmdSetViewport(engine->commandBuffers[imageIndex], 0, 1, &viewport);
    vkCmdSetScissor(engine->commandBuffers[imageIndex], 0, 1, &scissor);
    vkCmdBindDescriptorSets(engine->commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, engine->blurPipelineLayout, 0, 1, &engine->blurDescriptorSet, 0, nullptr);
    BlurPushConstants blurPc; blurPc.topMargin = engine->topMargin; blurPc.screenHeight = (float)engine->swapchainExtent.height;
    vkCmdPushConstants(engine->commandBuffers[imageIndex], engine->blurPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(BlurPushConstants), &blurPc);
    vkCmdDraw(engine->commandBuffers[imageIndex], 3, 1, 0, 0);
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
        size_t pos = engine->cursors.back().head; int lineIdx = getLineIdx(engine, pos); float scale = engine->currentFontSize / 48.0f; float currentLineHeight = 60.0f * scale; float caretY = engine->topMargin + lineIdx * currentLineHeight; float displayY = caretY - engine->scrollY; float winH = (float)ANativeWindow_getHeight(engine->app->window); float visibleH = winH - engine->bottomInset;
        bool isOutsideY = (displayY + currentLineHeight < engine->topMargin) || (displayY > visibleH);
        if (isOutsideY) {
            engine->isKeyboardShowing = false; JNIEnv* env = nullptr; engine->app->activity->vm->AttachCurrentThread(&env, nullptr);
            jclass activityClass = env->GetObjectClass(engine->app->activity->clazz); jmethodID hideImeMethod = env->GetMethodID(activityClass, "hideSoftwareKeyboard", "()V");
            if (hideImeMethod) env->CallVoidMethod(engine->app->activity->clazz, hideImeMethod);
            env->DeleteLocalRef(activityClass); engine->app->activity->vm->DetachCurrentThread();
        }
    }
}
static int32_t handleInput(struct android_app* app, AInputEvent* event) {
    Engine* engine = (Engine*)app->userData; std::lock_guard<std::mutex> lock(g_imeMutex);
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
        int32_t action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
        size_t pointerCount = AMotionEvent_getPointerCount(event);

        // ★追加: 入力ソースがマウスかどうか、左クリックされているかを判定
        int32_t source = AInputEvent_getSource(event);
        bool isMouse = (source & AINPUT_SOURCE_MOUSE) == AINPUT_SOURCE_MOUSE;
        bool isPrimaryBtn = (AMotionEvent_getButtonState(event) & AMOTION_EVENT_BUTTON_PRIMARY) != 0;

        // ピンチズーム（2本指タッチ）
        if (pointerCount >= 2) {
            float x0 = AMotionEvent_getX(event, 0); float y0 = AMotionEvent_getY(event, 0); float x1 = AMotionEvent_getX(event, 1); float y1 = AMotionEvent_getY(event, 1);
            float dx = x1 - x0; float dy = y1 - y0; float distance = std::sqrt(dx * dx + dy * dy);
            if (action == AMOTION_EVENT_ACTION_POINTER_DOWN) { engine->isPinching = true; engine->lastPinchDistance = distance; engine->isDragging = false; engine->isSelecting = false; }
            else if (action == AMOTION_EVENT_ACTION_MOVE && engine->isPinching) {
                if (engine->lastPinchDistance > 0.0f) {
                    float ratio = distance / engine->lastPinchDistance; float newSize = engine->currentFontSize * ratio;
                    if (newSize < 16.0f) newSize = 16.0f; if (newSize > 200.0f) newSize = 200.0f;
                    engine->currentFontSize = newSize; float scale = newSize / 48.0f; engine->lineHeight = 60.0f * scale; engine->charWidth = 24.0f * scale;
                    updateGutterWidth(engine); engine->lastPinchDistance = distance; ensureCaretVisible(engine);
                }
            } else if (action == AMOTION_EVENT_ACTION_POINTER_UP) {
                engine->isPinching = false; int upIndex = (AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT; int remainIndex = (upIndex == 0) ? 1 : 0;
                if (remainIndex < pointerCount) { engine->lastTouchX = AMotionEvent_getX(event, remainIndex); engine->lastTouchY = AMotionEvent_getY(event, remainIndex); }
            }
            return 1;
        }

        if (engine->isPinching) {
            if (action == AMOTION_EVENT_ACTION_UP) { engine->isPinching = false; engine->isDragging = false; }
            engine->lastTouchX = AMotionEvent_getX(event, 0); engine->lastTouchY = AMotionEvent_getY(event, 0); return 1;
        }

        float x = AMotionEvent_getX(event, 0); float y = AMotionEvent_getY(event, 0);
        if (action == AMOTION_EVENT_ACTION_SCROLL && isMouse) {
            int32_t metaState = AMotionEvent_getMetaState(event);
            if ((metaState & AMETA_CTRL_ON) != 0 || (metaState & AMETA_META_ON) != 0) {
                return 0;
            }
            float scrollY = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_VSCROLL, 0);
            float scrollX = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_HSCROLL, 0);
            engine->scrollY -= scrollY * engine->lineHeight * 3.0f;
            engine->scrollX -= scrollX * engine->charWidth * 5.0f;
            if (engine->scrollX < 0.0f) engine->scrollX = 0.0f;
            if (engine->scrollY < 0.0f) engine->scrollY = 0.0f;
            if (app->window != nullptr) {
                float winW = (float)ANativeWindow_getWidth(app->window); float winH = (float)ANativeWindow_getHeight(app->window); float visibleH = winH - engine->bottomInset;
                float contentH = engine->lineStarts.size() * engine->lineHeight + engine->topMargin + engine->lineHeight; float maxScrollY = std::max(0.0f, contentH - visibleH); float maxScrollX = std::max(0.0f, engine->maxLineWidth - winW + engine->charWidth * 2.0f);
                if (engine->scrollY > maxScrollY) engine->scrollY = maxScrollY; if (engine->scrollX > maxScrollX) engine->scrollX = maxScrollX;
            }
            return 1;
        }

        if (action == AMOTION_EVENT_ACTION_DOWN) {
            int64_t now = getCurrentTimeMs();
            if (now - engine->lastClickTime < 400 && std::abs(x - engine->lastClickX) < 30.0f && std::abs(y - engine->lastClickY) < 30.0f) engine->clickCount++; else engine->clickCount = 1;
            engine->lastClickTime = now; engine->lastClickX = x; engine->lastClickY = y;
            engine->isDragging = false; engine->lastTouchX = x; engine->lastTouchY = y;
            engine->isFlinging = false; engine->velocityX = 0.0f; engine->velocityY = 0.0f; engine->lastMoveTime = now;
            if (isMouse && isPrimaryBtn) {
                engine->isSelecting = true;
                size_t targetPos = getDocPosFromPoint(engine, x, y);
                if (engine->clickCount >= 3) {
                    selectLineAt(engine, targetPos);
                } else if (engine->clickCount == 2) {
                    selectWordAt(engine, targetPos);
                } else {
                    engine->cursors.clear();
                    engine->cursors.push_back({ targetPos, targetPos, getXFromPos(engine, targetPos) });
                }
                ensureCaretVisible(engine);
            } else {
                engine->isSelecting = false;
            }
        } else if (action == AMOTION_EVENT_ACTION_MOVE) {
            int64_t now = getCurrentTimeMs(); float dx = x - engine->lastTouchX; float dy = y - engine->lastTouchY; int64_t dt = now - engine->lastMoveTime;
            if (dt > 0) { float instVelX = dx / (float)dt; float instVelY = dy / (float)dt; engine->velocityX = engine->velocityX * 0.4f + instVelX * 0.6f; engine->velocityY = engine->velocityY * 0.4f + instVelY * 0.6f; }
            engine->lastMoveTime = now;

            // ★変更: テキスト選択モード中（マウスドラッグ中）の処理
            if (engine->isSelecting && isMouse && isPrimaryBtn) {
                if (!engine->cursors.empty()) {
                    size_t targetPos = getDocPosFromPoint(engine, x, y);
                    engine->cursors.back().head = targetPos; // カーソルの先端(head)だけを動かす
                    engine->cursors.back().desiredX = getXFromPos(engine, targetPos);
                    ensureCaretVisible(engine);
                }
                engine->lastTouchX = x; engine->lastTouchY = y;
                return 1; // 選択中はスクロール処理にフォールスルーさせない
            }

            // 通常のタッチによるドラッグ（スクロール処理）
            if (!engine->isDragging && !engine->isSelecting && (std::abs(x - engine->lastClickX) > 10.0f || std::abs(y - engine->lastClickY) > 10.0f)) { engine->isDragging = true; engine->lastTouchX = x; engine->lastTouchY = y; dx = 0.0f; dy = 0.0f; }
            if (engine->isDragging) {
                engine->scrollX -= dx; engine->scrollY -= dy; if (engine->scrollX < 0.0f) engine->scrollX = 0.0f; if (engine->scrollY < 0.0f) engine->scrollY = 0.0f;
                if (app->window != nullptr) {
                    float winW = (float)ANativeWindow_getWidth(app->window); float winH = (float)ANativeWindow_getHeight(app->window); float visibleH = winH - engine->bottomInset;
                    float contentH = engine->lineStarts.size() * engine->lineHeight + engine->topMargin + engine->lineHeight; float maxScrollY = std::max(0.0f, contentH - visibleH); float maxScrollX = std::max(0.0f, engine->maxLineWidth - winW + engine->charWidth * 2.0f);
                    if (engine->scrollY > maxScrollY) engine->scrollY = maxScrollY; if (engine->scrollX > maxScrollX) engine->scrollX = maxScrollX;
                }
                engine->lastTouchX = x; engine->lastTouchY = y; checkKeyboardVisibility(engine);
            }

        } else if (action == AMOTION_EVENT_ACTION_UP) {
            // ★変更: マウス選択の終了、または指のタップ・フリックの処理
            if (engine->isSelecting) {
                engine->isSelecting = false;

                // 選択終了後、念のためソフトウェアキーボードの表示命令を出す
                engine->isKeyboardShowing = true; JNIEnv* env = nullptr; app->activity->vm->AttachCurrentThread(&env, nullptr);
                jclass activityClass = env->GetObjectClass(app->activity->clazz); jmethodID showImeMethod = env->GetMethodID(activityClass, "showSoftwareKeyboard", "()V");
                if (showImeMethod) env->CallVoidMethod(app->activity->clazz, showImeMethod); env->DeleteLocalRef(activityClass); app->activity->vm->DetachCurrentThread();

            } else if (!engine->isDragging) {
                // 単なるタップ（マウスクリック または 指でのタップ）
                engine->isKeyboardShowing = true; JNIEnv* env = nullptr; app->activity->vm->AttachCurrentThread(&env, nullptr);
                jclass activityClass = env->GetObjectClass(app->activity->clazz); jmethodID showImeMethod = env->GetMethodID(activityClass, "showSoftwareKeyboard", "()V");
                if (showImeMethod) env->CallVoidMethod(app->activity->clazz, showImeMethod); env->DeleteLocalRef(activityClass); app->activity->vm->DetachCurrentThread();

                size_t targetPos = getDocPosFromPoint(engine, x, y);
                if (engine->clickCount >= 3) selectLineAt(engine, targetPos);
                else if (engine->clickCount == 2) selectWordAt(engine, targetPos);
                else { engine->cursors.clear(); engine->cursors.push_back({ targetPos, targetPos, getXFromPos(engine, targetPos) }); }
                ensureCaretVisible(engine);

            } else {
                // 指でのフリック操作
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
    Engine* engine = (Engine*)app->userData; std::lock_guard<std::mutex> lock(g_imeMutex);
    switch (cmd) {
        case APP_CMD_INIT_WINDOW: if (app->window != nullptr) { LOGI("ウィンドウ生成。Vulkan初期化開始。"); updateThemeColors(engine); if (initVulkan(engine)) { engine->isWindowReady = true; rebuildLineStarts(engine); updateGutterWidth(engine); updateTitleBarIfNeeded(engine); } } break;
        case APP_CMD_CONFIG_CHANGED: updateThemeColors(engine); if (engine->isWindowReady) recreateSwapchain(engine); break;
        case APP_CMD_WINDOW_RESIZED:
        case APP_CMD_WINDOW_REDRAW_NEEDED: if (engine->isWindowReady) recreateSwapchain(engine); break;
        case APP_CMD_TERM_WINDOW: LOGI("ウィンドウ破棄。"); engine->isWindowReady = false; cleanupVulkan(engine); break;
    }
}
void updateTitleBarIfNeeded(Engine* engine) {
    static std::string cachedUntitled = "";
    if (cachedUntitled.empty()) { cachedUntitled = GetResString(engine, "untitled"); if (cachedUntitled.empty()) cachedUntitled = "untitled"; }
    std::string titleStr;
    if (!engine->displayFileName.empty()) titleStr = engine->displayFileName;
    else if (!engine->currentFilePath.empty()) { size_t slashPos = engine->currentFilePath.find_last_of("/\\"); if (slashPos != std::string::npos) titleStr = engine->currentFilePath.substr(slashPos + 1); else titleStr = cachedUntitled; }
    else titleStr = cachedUntitled;
    if (engine->isDirty) titleStr = "*" + titleStr;
    if (engine->lastTitleStr != titleStr) {
        engine->lastTitleStr = titleStr; LOGI("Title changed to: %s", titleStr.c_str()); JNIEnv* env = nullptr; JavaVM* vm = engine->app->activity->vm; bool needDetach = false;
        if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) == JNI_EDETACHED) { if (vm->AttachCurrentThread(&env, nullptr) != 0) return; needDetach = true; }
        jclass clazz = env->GetObjectClass(engine->app->activity->clazz); jmethodID mid = env->GetMethodID(clazz, "setOverlayTitle", "(Ljava/lang/String;)V");
        if (mid) { jstring jTitle = env->NewStringUTF(titleStr.c_str()); env->CallVoidMethod(engine->app->activity->clazz, mid, jTitle); env->DeleteLocalRef(jTitle); }
        env->DeleteLocalRef(clazz); if (needDetach) vm->DetachCurrentThread();
    }
}
void android_main(struct android_app* app) {
    Engine engine = {}; engine.app = app; app->userData = &engine; app->onAppCmd = onAppCmd; app->onInputEvent = handleInput; g_engine = &engine;
    engine.pt.initEmpty(); rebuildLineStarts(&engine); engine.cursors.push_back({0, 0, 0.0f});
    while (true) {
        int events; struct android_poll_source* source; int timeout = engine.isWindowReady ? 0 : -1;
        while (ALooper_pollOnce(timeout, nullptr, &events, (void**)&source) >= 0) { if (source != nullptr) source->process(app, source); if (app->destroyRequested != 0) { cleanupVulkan(&engine); return; } timeout = engine.isWindowReady ? 0 : -1; }
        if (engine.isWindowReady) {
            if (engine.isFlinging) {
                int64_t now = getCurrentTimeMs(); int64_t dt = now - engine.lastMoveTime;
                if (dt > 0) {
                    if (dt > 50) dt = 50; engine.scrollX -= engine.velocityX * dt; engine.scrollY -= engine.velocityY * dt; float friction = std::pow(0.998f, (float)dt); engine.velocityX *= friction; engine.velocityY *= friction;
                    if (std::abs(engine.velocityX) < 0.02f && std::abs(engine.velocityY) < 0.02f) engine.isFlinging = false;
                    if (engine.app->window != nullptr) {
                        float winW = (float)ANativeWindow_getWidth(engine.app->window); float winH = (float)ANativeWindow_getHeight(engine.app->window); float visibleH = winH - engine.bottomInset;
                        float contentH = engine.lineStarts.size() * engine.lineHeight + engine.topMargin + engine.lineHeight; float maxScrollY = std::max(0.0f, contentH - visibleH); float maxScrollX = std::max(0.0f, engine.maxLineWidth - winW + engine.charWidth * 2.0f);
                        if (engine.scrollX < 0.0f) { engine.scrollX = 0.0f; engine.velocityX = 0.0f; } else if (engine.scrollX > maxScrollX) { engine.scrollX = maxScrollX; engine.velocityX = 0.0f; }
                        if (engine.scrollY < 0.0f) { engine.scrollY = 0.0f; engine.velocityY = 0.0f; } else if (engine.scrollY > maxScrollY) { engine.scrollY = maxScrollY; engine.velocityY = 0.0f; }
                        checkKeyboardVisibility(&engine);
                    }
                    engine.lastMoveTime = now;
                }
            }
            pollAsyncParsing(&engine);
            {
                std::lock_guard<std::mutex> lock(g_imeMutex);
                while (!g_imeQueue.empty()) {
                    ImeEvent ev = g_imeQueue.front(); g_imeQueue.pop_front();
                    if (ev.type == ImeEvent::Commit) { engine.imeComp.clear(); if (ev.text == "\n") insertNewlineWithAutoIndent(&engine); else insertAtCursors(&engine, ev.text); }
                    else if (ev.type == ImeEvent::Composing) { engine.imeComp = ev.text; if (!engine.cursors.empty()) { int lineIdx = getLineIdx(&engine, engine.cursors.back().head); if (lineIdx >= 0 && lineIdx < engine.lineCaches.size()) engine.lineCaches[lineIdx].isShaped = false; } }
                    else if (ev.type == ImeEvent::FinishComposing) { if (!engine.imeComp.empty()) { std::string textToCommit = engine.imeComp; engine.imeComp.clear(); insertAtCursors(&engine, textToCommit); } }
                    else if (ev.type == ImeEvent::Delete) backspaceAtCursors(&engine);
                    else if (ev.type == ImeEvent::DeleteForward) deleteForwardAtCursors(&engine);
                }
            }
            renderFrame(&engine);
        }
    }
}