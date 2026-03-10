#include <android_native_app_glue.h>
#include <android/log.h>
#include <jni.h> // ★JNI連携用
#include <vector>
#include <string>
#include <algorithm>
#include <memory>
#include <unordered_map>
#include <fstream>

// --- FreeType のインクルード ---
#include <ft2build.h>
#include FT_FREETYPE_H

// --- Vulkan のインクルード ---
#define VK_USE_PLATFORM_ANDROID_KHR
#include <vulkan/vulkan.h>

// ============================================================================
// ログ出力マクロ (これを必ず一番上に配置します)
// ============================================================================
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "miu", __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, "miu", __VA_ARGS__))

// ============================================================================
// フォント・アトラス関連の構造体と関数
// ============================================================================
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
        // ★追加：アトラスの左上(0,0)に2x2の純白ピクセルを書き込んでおく
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 2; ++x) {
                int idx = (y * width + x) * 4;
                pixels[idx + 0] = 255; pixels[idx + 1] = 255; pixels[idx + 2] = 255; pixels[idx + 3] = 255;
            }
        }
        currentX = 3; // 文字は(3,0)から配置スタート
        currentY = 0;
        maxRowHeight = 2;
    }

    bool loadChar(FT_Face mainFace, FT_Face emojiFace, uint32_t charCode) {
        if (glyphs.count(charCode) > 0) return true;

        FT_Face targetFace = mainFace;

        // 1. まずメインフォントで文字を探す
        FT_UInt glyphIndex = FT_Get_Char_Index(mainFace, charCode);

        // 2. メインフォントに文字がなく、絵文字フォントがある場合
        if (glyphIndex == 0 && emojiFace != nullptr) {
            FT_UInt emojiIndex = FT_Get_Char_Index(emojiFace, charCode);
            if (emojiIndex != 0) {
                targetFace = emojiFace; // 絵文字フォントへ切り替え
                glyphIndex = emojiIndex;
            }
        }

        // 豆腐（.notdef）さえ無い緊急事態
        if (glyphIndex == 0 && charCode != 0) {
            FT_Load_Glyph(mainFace, 0, FT_LOAD_RENDER | FT_LOAD_COLOR);
            targetFace = mainFace;
        } else {
            // 3. グリフをロード (NotoColorEmojiの場合、ここでPNGが展開される)
            if (FT_Load_Glyph(targetFace, glyphIndex, FT_LOAD_RENDER | FT_LOAD_COLOR) != 0) {
                FT_Load_Glyph(mainFace, 0, FT_LOAD_RENDER | FT_LOAD_COLOR); // ロードエラーならメイン豆腐
                targetFace = mainFace;
            }
        }

        FT_Bitmap* bmp = &targetFace->glyph->bitmap;

        // ★★★ 今回の肝： advance（文字幅）はあるが、Bitmapが無い（展開失敗）の場合 ★★★
        if (charCode != 0x20 && charCode != 0xA0 && (bmp->width == 0 || bmp->rows == 0) && targetFace->glyph->advance.x != 0 && charCode != 0 && glyphIndex != 0) {
            LOGE("😎グリフ展開失敗（豆腐フォールバック）: cp:%x avance:%ld font:%s",
                 charCode, targetFace->glyph->advance.x, (targetFace == mainFace) ? "Main" : "Emoji");

            // 強制的にメインフォントの豆腐をロード
            if (FT_Load_Glyph(mainFace, 0, FT_LOAD_RENDER | FT_LOAD_COLOR) == 0) {
                targetFace = mainFace;
                bmp = &targetFace->glyph->bitmap;
            } else {
                return false; // それでもダメなら表示不能
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
                    pixels[dstIdx + 0] = bgr[2]; // R
                    pixels[dstIdx + 1] = bgr[1]; // G
                    pixels[dstIdx + 2] = bgr[0]; // B
                    pixels[dstIdx + 3] = bgr[3]; // A
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
            scale = 48.0f / (float)bmp->rows; // カラー絵文字のみ縮小
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

    // 1バイト文字 (ASCII)
    if (c < 0x80) return c;

    // 2バイト文字
    if ((c & 0xE0) == 0xC0) {
        if (*ptr >= end) return 0;
        uint32_t cp = (c & 0x1F) << 6;
        cp |= (**ptr & 0x3F);
        (*ptr)++;
        return cp;
    }
    // 3バイト文字 (日本語の大部分)
    if ((c & 0xF0) == 0xE0) {
        if (*ptr + 1 >= end) { *ptr = end; return 0; }
        uint32_t cp = (c & 0x0F) << 12;
        cp |= (**ptr & 0x3F) << 6; (*ptr)++;
        cp |= (**ptr & 0x3F);      (*ptr)++;
        return cp;
    }
    // 4バイト文字 (絵文字や一部の特殊漢字)
    if ((c & 0xF8) == 0xF0) {
        if (*ptr + 2 >= end) { *ptr = end; return 0; }
        uint32_t cp = (c & 0x07) << 18;
        cp |= (**ptr & 0x3F) << 12; (*ptr)++;
        cp |= (**ptr & 0x3F) << 6;  (*ptr)++;
        cp |= (**ptr & 0x3F);       (*ptr)++;
        return cp;
    }
    return '?'; // 不正なバイト列のフォールバック
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
            LOGI("フォント読み込み成功: %s (%zu bytes)", path, size);
            return buffer;
        }
    }
    LOGE("システムフォントが見つかりませんでした。");
    return {};
}

// ============================================================================
// エディタコアロジック (PieceTable等)
// ============================================================================

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

// ============================================================================
// [2] Vulkan & Android システム構造体 (WindowsのEditorクラスに相当)
// ============================================================================

// ============================================================================
// 描画用データ構造体 (Engine構造体の上に配置)
// ============================================================================
struct Vertex {
    float pos[2]; // 画面上のX, Y座標
    float uv[2];  // テクスチャ上のU, V座標
    float isColor;
    float color[4]; // ★追加
};

// 毎フレームGPUに送る軽量な設定データ（Push Constants）
struct PushConstants {
    float screenWidth;
    float screenHeight;
    float padding[2]; // ★GPUのルール（16バイト区切り）に合わせるための隙間
    float color[4];   // 文字色 (R, G, B, A)
};

struct Engine {
    struct android_app* app;
    bool isWindowReady = false;

    PieceTable pt;
    UndoManager undo;
    std::vector<Cursor> cursors;
    std::vector<size_t> lineStarts;
    std::string imeComp; // ★IME変換中の未確定文字列

    float scrollX = 0.0f; // ピクセル単位の横スクロール量
    float scrollY = 0.0f; // ピクセル単位の縦スクロール量
    float maxLineWidth = 0.0f;
    float bottomInset = 0.0f; // キーボードの高さ

    float lastTouchX = 0.0f;
    float lastTouchY = 0.0f;
    bool isDragging = false;

    int vScrollPos = 0;
    int hScrollPos = 0;
    float lineHeight = 60.0f;
    float charWidth = 24.0f;
    float gutterWidth = 100.0f;

    float currentFontSize = 48.0f; // 21.0f から 48.0f へ

    // --- Vulkan オブジェクト ---
    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue graphicsQueue;
    uint32_t queueFamilyIndex;

// --- 追加するフォント関連変数 ---
    FT_Library ftLibrary;
    FT_Face ftFaceMain;
    FT_Face ftFaceEmoji = nullptr; // ★今回追加 (NotoColorEmoji)
    std::vector<uint8_t> fontDataMain;  // データも保持
    std::vector<uint8_t> fontDataEmoji; // 絵文字フォントデータ
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

    // GPU用フォントテクスチャリソース
    VkImage fontImage = VK_NULL_HANDLE;
    VkDeviceMemory fontImageMemory = VK_NULL_HANDLE;
    VkImageView fontImageView = VK_NULL_HANDLE;
    VkSampler fontSampler = VK_NULL_HANDLE;

    // --- 今回追加するパイプライン関連変数 ---
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


#include <mutex>
#include <deque>

// 入力イベントのリスト
struct ImeEvent {
    enum Type { Commit, Composing, Delete } type;
    std::string text;
};

std::mutex g_imeMutex;
std::deque<ImeEvent> g_imeQueue;



Engine* g_engine = nullptr;

// ============================================================================
// 3. Android用: テキストレイアウトとカーソル制御
// ============================================================================
void rebuildLineStarts(Engine* engine) {
    engine->lineStarts.clear();
    engine->lineStarts.push_back(0);
    size_t totalLen = engine->pt.length();
    for (size_t i = 0; i < totalLen; ++i) {
        if (engine->pt.charAt(i) == '\n') {
            engine->lineStarts.push_back(i + 1);
        }
    }
}

int getLineIdx(Engine* engine, size_t pos) {
    if (engine->lineStarts.empty()) return 0;
    auto it = std::upper_bound(engine->lineStarts.begin(), engine->lineStarts.end(), pos);
    int idx = (int)std::distance(engine->lineStarts.begin(), it) - 1;
    return std::max(0, std::min(idx, (int)engine->lineStarts.size() - 1));
}

// ★ DirectWrite の layout->HitTestTextPosition の代わり
float getXFromPos(Engine* engine, size_t pos) {
    int lineIdx = getLineIdx(engine, pos);
    // 安全対策：行インデックスの範囲外アクセス防止
    if (lineIdx < 0 || lineIdx >= engine->lineStarts.size()) return 0.0f;

    size_t start = engine->lineStarts[lineIdx];
    size_t len = pos - start;
    if (len <= 0) return 0.0f;

    std::string text = engine->pt.getRange(start, len);
    const char* ptr = text.data();
    const char* end = ptr + text.size();
    float x = 0.0f;

    while (ptr < end) {
        uint32_t cp = decodeUtf8(&ptr, end);
        if (cp == 0) break;

        // ★修正：アトラスに無い文字は、ここで動的にロードを試みる
        if (engine->atlas.glyphs.count(cp) == 0) {
            // ftFaceMain や ftFaceEmoji が null の場合でもクラッシュしないようチェック
            if (engine->ftFaceMain != nullptr) {
                engine->atlas.loadChar(engine->ftFaceMain, engine->ftFaceEmoji, cp);
            }
        }

        // ロードに成功したか、すでに存在する場合はその幅を足す
        if (engine->atlas.glyphs.count(cp) > 0) {
            x += engine->atlas.glyphs[cp].advance;
        } else {
            // ロードできなかった場合（豆腐すら出ない異常事態）はデフォルト幅
            x += engine->charWidth;
        }
    }
    return x;
}
// タッチされたピクセル座標(X, Y)から、PieceTable上の文字インデックス(pos)を計算する
size_t getDocPosFromPoint(Engine* engine, float touchX, float touchY) {
    // 1. タップされたY座標にスクロール量を足して「仮想Y座標」にする
    float virtualY = touchY + engine->scrollY - 100.0f; // 100.0fの余白分を引く
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

    // 2. X座標のスタート位置にもスクロールを考慮
    float currentX = engine->gutterWidth - engine->scrollX;
    size_t currentPos = start;

    while (ptr < strEnd) {
        const char* prevPtr = ptr;
        uint32_t cp = decodeUtf8(&ptr, strEnd);
        if (cp == 0) break;

        // アトラスに無い文字は動的にロード（getXFromPosと同じ安全対策）
        if (engine->atlas.glyphs.count(cp) == 0 && engine->ftFaceMain != nullptr) {
            engine->atlas.loadChar(engine->ftFaceMain, engine->ftFaceEmoji, cp);
        }

        float advance = engine->charWidth;
        if (engine->atlas.glyphs.count(cp) > 0) {
            advance = engine->atlas.glyphs[cp].advance;
        }

        // ★ タッチされたX座標が、この文字の「中心」より左なら、この文字の直前にカーソルを置く
        if (touchX < currentX + (advance / 2.0f)) {
            break;
        }

        currentX += advance;
        currentPos += (ptr - prevPtr); // UTF-8のバイト数分だけインデックスを進める
    }

    return currentPos;
}

// カーソルが画面内に収まるようにスクロール量を自動調整する
void ensureCaretVisible(Engine* engine) {
    if (engine->cursors.empty()) return;
    size_t pos = engine->cursors.back().head;

    int lineIdx = getLineIdx(engine, pos);
    float caretY = 100.0f + lineIdx * engine->lineHeight;
    float caretX = getXFromPos(engine, pos);

    float winW = 1080.0f;
    float winH = 2000.0f;
    if (engine->app->window != nullptr) {
        winW = (float)ANativeWindow_getWidth(engine->app->window);
        winH = (float)ANativeWindow_getHeight(engine->app->window);
    }

    // ★修正: 画面の高さからキーボードの高さ(bottomInset)を引いたものを可視領域とする
    float visibleH = winH - engine->bottomInset;
    if (visibleH < winH * 0.3f) visibleH = winH * 0.5f; // 念のための安全策

    // --- 縦(Y)スクロールの調整 ---
    if (caretY < engine->scrollY + 100.0f) {
        engine->scrollY = caretY - 100.0f;
    } else if (caretY + engine->lineHeight > engine->scrollY + visibleH) {
        // ★キーボードの真上(visibleH)にカーソルが来るようにスクロール
        engine->scrollY = caretY + engine->lineHeight - visibleH;
    }
    // --- 横(X)スクロールの調整 ---
    if (caretX < engine->scrollX) {
        engine->scrollX = caretX;
    } else if (caretX + engine->charWidth * 2.0f > engine->scrollX + winW - engine->gutterWidth) {
        engine->scrollX = caretX + engine->charWidth * 2.0f - (winW - engine->gutterWidth);
    }

    if (engine->scrollX < 0.0f) engine->scrollX = 0.0f;
    if (engine->scrollY < 0.0f) engine->scrollY = 0.0f;
}

void insertAtCursors(Engine* engine, const std::string& text) {
    if (engine->cursors.empty() || text.empty()) return; // 空文字の挿入は無視
    EditBatch batch;
    batch.beforeCursors = engine->cursors;

    // シンプル化のため、最初のカーソルにのみ挿入する処理（マルチカーソル対応はWindows版準拠で拡張可）
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
    rebuildLineStarts(engine);
    ensureCaretVisible(engine);
}

void backspaceAtCursors(Engine* engine) {
    if (engine->cursors.empty()) return;
    Cursor& c = engine->cursors.back();
    if (c.head > 0 && !c.hasSelection()) {
        // UTF-8の文字境界を考慮したバックスペース
        size_t eraseLen = 1;
        while (c.head - eraseLen > 0 && (engine->pt.charAt(c.head - eraseLen) & 0xC0) == 0x80) {
            eraseLen++; // マルチバイト文字の先頭バイトを探す
        }
        std::string d = engine->pt.getRange(c.head - eraseLen, eraseLen);
        engine->pt.erase(c.head - eraseLen, eraseLen);
        c.head -= eraseLen;
        c.anchor = c.head;
        rebuildLineStarts(engine);
    }
    ensureCaretVisible(engine);
}

// ============================================================================
// 4. JNIによるIME・キーボード連携 (Java/Kotlinからの呼び出し口)
// ============================================================================
// ============================================================================
// JNIによるIME・キーボード連携 (Javaからの呼び出し口)
// ============================================================================
#include <jni.h>

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
    g_engine->bottomInset = (float)bottomInset;
    // 高さが変わったらカーソルが隠れないように再調整する
    ensureCaretVisible(g_engine);
}
}


// ============================================================================
// [3] Vulkan 初期化・破棄・描画ロジック (前回の内容)
// ============================================================================

// ※コードが長くなるため、前回の cleanupVulkan, createSwapchain, createRenderPass,
// createFramebuffers, createCommandBuffers, createSyncObjects, initVulkan は
// 変更なしでそのままここに配置します。

void cleanupVulkan(Engine* engine) {
    if (engine->device == VK_NULL_HANDLE) return;

    if (engine->graphicsPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(engine->device, engine->graphicsPipeline, nullptr);
        engine->graphicsPipeline = VK_NULL_HANDLE; // ★念のためNULLを入れる
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
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    LOGE("適切なメモリタイプが見つかりません！");
    return 0;
}

// --- バッファ（データの一時置き場）を作成する関数 ---
void createBuffer(Engine* engine, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCreateBuffer(engine->device, &bufferInfo, nullptr, &buffer);

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(engine->device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(engine->physicalDevice, memRequirements.memoryTypeBits, properties);

    vkAllocateMemory(engine->device, &allocInfo, nullptr, &bufferMemory);
    vkBindBufferMemory(engine->device, buffer, bufferMemory, 0);
}

// --- 単発コマンド（データ転送など）を開始・終了する関数 ---
VkCommandBuffer beginSingleTimeCommands(Engine* engine) {
    VkCommandBufferAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = engine->commandPool;
    allocInfo.commandBufferCount = 1;

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
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(engine->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(engine->graphicsQueue); // 完了まで待機
    vkFreeCommandBuffers(engine->device, engine->commandPool, 1, &commandBuffer);
}

// --- 画像のレイアウト（用途）を変換する関数 ---
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
        LOGE("未対応のレイアウト遷移です！"); return;
    }
    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    endSingleTimeCommands(engine, commandBuffer);
}

// --- バッファから画像へデータをコピーする関数 ---
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
    // ★修正：RGBAなのでピクセル数 × 4バイト 必要
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
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM; // RGBAフォーマット
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

    LOGI("GPUテクスチャ転送完了！");
    return true;
}

bool createSwapchain(Engine* engine) {
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(engine->physicalDevice, engine->surface, &capabilities);
    if (capabilities.currentExtent.width != 0xFFFFFFFF) { engine->swapchainExtent = capabilities.currentExtent; }
    else {
        int32_t w = ANativeWindow_getWidth(engine->app->window);
        int32_t h = ANativeWindow_getHeight(engine->app->window);
        engine->swapchainExtent.width = std::clamp((uint32_t)w, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        engine->swapchainExtent.height = std::clamp((uint32_t)h, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    }
    if (engine->swapchainExtent.width == 0 || engine->swapchainExtent.height == 0) return false;

    uint32_t formatCount; vkGetPhysicalDeviceSurfaceFormatsKHR(engine->physicalDevice, engine->surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount); vkGetPhysicalDeviceSurfaceFormatsKHR(engine->physicalDevice, engine->surface, &formatCount, formats.data());
    VkSurfaceFormatKHR selectedFormat = formats[0];
    for (const auto& f : formats) if (f.format == VK_FORMAT_B8G8R8A8_UNORM || f.format == VK_FORMAT_R8G8B8A8_UNORM) { selectedFormat = f; break; }
    engine->swapchainFormat = selectedFormat.format;

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) imageCount = capabilities.maxImageCount;

    VkSwapchainCreateInfoKHR createInfo = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    createInfo.surface = engine->surface; createInfo.minImageCount = imageCount; createInfo.imageFormat = engine->swapchainFormat;
    createInfo.imageColorSpace = selectedFormat.colorSpace; createInfo.imageExtent = engine->swapchainExtent;
    createInfo.imageArrayLayers = 1; createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR; createInfo.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(engine->device, &createInfo, nullptr, &engine->swapchain) != VK_SUCCESS) return false;
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

bool createSyncObjects(Engine* engine) {
    VkSemaphoreCreateInfo semInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fenceInfo = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO}; fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    return vkCreateSemaphore(engine->device, &semInfo, nullptr, &engine->imageAvailableSemaphore) == VK_SUCCESS &&
           vkCreateSemaphore(engine->device, &semInfo, nullptr, &engine->renderFinishedSemaphore) == VK_SUCCESS &&
           vkCreateFence(engine->device, &fenceInfo, nullptr, &engine->inFlightFence) == VK_SUCCESS;
}

// --- テクスチャをシェーダに渡すための「窓口」を作成 ---
bool createDescriptors(Engine* engine) {
    // 1. どんなデータを渡すかの設計図 (テクスチャ1枚)
    VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
    samplerLayoutBinding.binding = 0;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &samplerLayoutBinding;

    if (vkCreateDescriptorSetLayout(engine->device, &layoutInfo, nullptr, &engine->descriptorSetLayout) != VK_SUCCESS) return false;

    // 2. 窓口の実体を確保するためのプール
    VkDescriptorPoolSize poolSize = {};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;

    if (vkCreateDescriptorPool(engine->device, &poolInfo, nullptr, &engine->descriptorPool) != VK_SUCCESS) return false;

    // 3. 窓口の実体（ディスクリプタセット）を割り当て
    VkDescriptorSetAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool = engine->descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &engine->descriptorSetLayout;

    if (vkAllocateDescriptorSets(engine->device, &allocInfo, &engine->descriptorSet) != VK_SUCCESS) return false;

    // 4. 先ほど作ったフォントテクスチャを窓口に接続！
    VkDescriptorImageInfo imageInfo = {};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = engine->fontImageView;
    imageInfo.sampler = engine->fontSampler;

    VkWriteDescriptorSet descriptorWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    descriptorWrite.dstSet = engine->descriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(engine->device, 1, &descriptorWrite, 0, nullptr);

    LOGI("ディスクリプタセットの作成完了");
    return true;
}

// --- シェーダ全体の設定（パイプラインレイアウト）を作成 ---
bool createPipelineLayout(Engine* engine) {
    // PushConstants (画面サイズなどの軽量データ) の設定
    VkPushConstantRange pushConstantRange = {};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &engine->descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(engine->device, &pipelineLayoutInfo, nullptr, &engine->pipelineLayout) != VK_SUCCESS) return false;

    LOGI("パイプラインレイアウトの作成完了");
    return true;
}

#include <android/asset_manager.h>

// アセットからコンパイル済みシェーダ(.spv)を読み込む
std::vector<uint32_t> loadShaderAsset(Engine* engine, const char* filename) {
    AAsset* asset = AAssetManager_open(engine->app->activity->assetManager, filename, AASSET_MODE_BUFFER);
    if (!asset) {
        LOGE("シェーダの読み込みに失敗: %s", filename);
        return {};
    }
    size_t size = AAsset_getLength(asset);
    std::vector<uint32_t> buffer(size / sizeof(uint32_t));
    AAsset_read(asset, buffer.data(), size);
    AAsset_close(asset);
    return buffer;
}

VkShaderModule createShaderModule(Engine* engine, const std::vector<uint32_t>& code) {
    VkShaderModuleCreateInfo createInfo = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    createInfo.codeSize = code.size() * sizeof(uint32_t);
    createInfo.pCode = code.data();
    VkShaderModule shaderModule;
    vkCreateShaderModule(engine->device, &createInfo, nullptr, &shaderModule);
    return shaderModule;
}

std::vector<uint8_t> loadAsset(Engine* engine, const char* filename) {
    AAsset* asset = AAssetManager_open(engine->app->activity->assetManager, filename, AASSET_MODE_BUFFER);
    if (!asset) {
        LOGE("アセットの読み込みに失敗: %s", filename);
        return {};
    }
    size_t size = AAsset_getLength(asset);
    std::vector<uint8_t> buffer(size);
    AAsset_read(asset, buffer.data(), size);
    AAsset_close(asset);
    return buffer;
}

// グラフィックスパイプラインの構築
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

    // 頂点データのレイアウト設定
    VkVertexInputBindingDescription bindingDescription = {};
    bindingDescription.binding = 0; bindingDescription.stride = sizeof(Vertex); bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attributeDescriptions[4] = {};
    attributeDescriptions[0].binding = 0; attributeDescriptions[0].location = 0; attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT; attributeDescriptions[0].offset = offsetof(Vertex, pos);
    attributeDescriptions[1].binding = 0; attributeDescriptions[1].location = 1; attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT; attributeDescriptions[1].offset = offsetof(Vertex, uv);
    attributeDescriptions[2].binding = 0; attributeDescriptions[2].location = 2; attributeDescriptions[2].format = VK_FORMAT_R32_SFLOAT; attributeDescriptions[2].offset = offsetof(Vertex, isColor);

    // ★追加: 頂点カラーの設定
    attributeDescriptions[3].binding = 0;
    attributeDescriptions[3].location = 3;
    attributeDescriptions[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributeDescriptions[3].offset = offsetof(Vertex, color);

    // vertexInputInfo の属性数を3にする
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = 4; // ★ここを 4 に変更
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportState.viewportCount = 1; viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterizer.depthClampEnable = VK_FALSE; rasterizer.rasterizerDiscardEnable = VK_FALSE; rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f; rasterizer.cullMode = VK_CULL_MODE_NONE; rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisampling.sampleShadingEnable = VK_FALSE; multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // ★アルファブレンド（文字の背景の透過）を有効化
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

    LOGI("グラフィックスパイプラインの作成完了！");
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

    // ★修正：RGBAなので * 4 を追加
    createBuffer(engine, engine->atlas.width * engine->atlas.height * 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, engine->atlasStagingBuffer, engine->atlasStagingMemory);

    if (FT_Init_FreeType(&engine->ftLibrary)) {
        LOGE("FreeTypeの初期化に失敗！"); return false;
    }

    // ★必須：アトラスの配列メモリを確保
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

            // ビットマップフォントの場合は固定サイズを選択する
            if (FT_HAS_FIXED_SIZES(engine->ftFaceEmoji)) {
                FT_Select_Size(engine->ftFaceEmoji, 0); // 用意されている最初のサイズ（109px）を選択
                LOGI("絵文字フォントの読み込み成功（固定サイズ選択）");
            } else {
                FT_Set_Pixel_Sizes(engine->ftFaceEmoji, 0, 48);
                LOGI("絵文字フォントの読み込み成功（動的サイズ設定）");
            }
        } else {
            LOGE("絵文字フォントのFT_Face生成に失敗");
        }
    }

    // ★必須：ここで初期テクスチャをGPUに作成
    if (!createTextTexture(engine)) return false;

    if (!createDescriptors(engine)) return false;
    if (!createPipelineLayout(engine)) return false;
    if (!createGraphicsPipeline(engine)) return false;

    return true;
}

void updateTextVertices(Engine* engine) {
    float x = engine->gutterWidth - engine->scrollX;
    float y = 100.0f - engine->scrollY; // 100.0fは上部の余白

    engine->maxLineWidth = engine->gutterWidth;

    size_t len = engine->pt.length();
    std::string text = engine->pt.getRange(0, len);

    size_t mainCaretPos = engine->cursors.empty() ? 0 : engine->cursors.back().head;
    bool hasIME = !engine->imeComp.empty();

    if (hasIME) {
        if (mainCaretPos <= text.size()) text.insert(mainCaretPos, engine->imeComp);
        else text.append(engine->imeComp);
    }

    // ==========================================================
    // ★追加: カーソルの「視覚的な位置」を計算 (IME挿入によるズレを補正)
    // ==========================================================
    std::vector<size_t> visualCursors;
    for (const auto& c : engine->cursors) {
        size_t vPos = c.head;
        // メインカーソル以降の位置にあるカーソルは、変換中文字列の長さ分だけ右にズレる
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
    std::vector<Vertex> cursorVertices; // ★追加: カーソル描画用バッファ

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

    float textR = 1.0f, textG = 1.0f, textB = 1.0f, textA = 1.0f;
    float cursorWidth = 5.0f;

    while (ptr < end) {
        // ==========================================================
        // ★追加: 文字を処理する前に、現在位置にカーソルがあるか判定して描画
        // ==========================================================
        for (size_t vPos : visualCursors) {
            if (currentByteOffset == vPos) {
                float curY = y - engine->lineHeight * 0.8f;
                // 幅2.0f、高さlineHeightの白い矩形を追加
                addRect(cursorVertices, x, curY, cursorWidth, engine->lineHeight, textR, textG, textB, 1.0f);
            }
        }

        const char* prevPtr = ptr;

        if (*ptr == '\r') {
            ptr++; currentByteOffset += (ptr - prevPtr);
            if (ptr < end && *ptr == '\n') { prevPtr = ptr; ptr++; currentByteOffset += (ptr - prevPtr); }
            // ★修正: ここで scrollX を引く
            x = engine->gutterWidth - engine->scrollX;
            y += engine->lineHeight;
            continue;
        }
        if (*ptr == '\n') {
            ptr++; currentByteOffset += (ptr - prevPtr);
            // ★修正: ここで scrollX を引く
            x = engine->gutterWidth - engine->scrollX;
            y += engine->lineHeight;
            continue;
        }

        uint32_t cp = decodeUtf8(&ptr, end);
        if (cp == 0) break;
        size_t charBytes = ptr - prevPtr;

        if (engine->atlas.glyphs.count(cp) == 0 && engine->ftFaceMain != nullptr) {
            engine->atlas.loadChar(engine->ftFaceMain, engine->ftFaceEmoji, cp);
        }

        if (engine->atlas.glyphs.count(cp) == 0) {
            currentByteOffset += charBytes;
            continue;
        }

        GlyphInfo& info = engine->atlas.glyphs[cp];
        float advance = info.advance;

        bool isComposingChar = (hasIME && currentByteOffset >= mainCaretPos && currentByteOffset < mainCaretPos + engine->imeComp.size());

        if (isComposingChar) {
            float bgY = y - engine->lineHeight * 0.8f;
            addRect(bgVertices, x, bgY, advance, engine->lineHeight, 0.2f, 0.6f, 1.0f, 0.3f);

            float lineY = y + engine->lineHeight * 0.1f;
            addRect(lineVertices, x, lineY, advance, 2.0f, textR, textG, textB, 1.0f);
        }

        float isColorFlag = info.isColor ? 1.0f : 0.0f;
        float xpos = x + info.bearingX;
        float ypos = y - info.bearingY;
        float w = info.width;
        float h = info.height;

        charVertices.push_back({{xpos,     ypos    }, {info.u0, info.v0}, isColorFlag, {textR, textG, textB, textA}});
        charVertices.push_back({{xpos,     ypos + h}, {info.u0, info.v1}, isColorFlag, {textR, textG, textB, textA}});
        charVertices.push_back({{xpos + w, ypos    }, {info.u1, info.v0}, isColorFlag, {textR, textG, textB, textA}});
        charVertices.push_back({{xpos + w, ypos    }, {info.u1, info.v0}, isColorFlag, {textR, textG, textB, textA}});
        charVertices.push_back({{xpos,     ypos + h}, {info.u0, info.v1}, isColorFlag, {textR, textG, textB, textA}});
        charVertices.push_back({{xpos + w, ypos + h}, {info.u1, info.v1}, isColorFlag, {textR, textG, textB, textA}});

        x += advance;
        currentByteOffset += charBytes;

        // ★追加: 現在の文字の右端の「絶対座標（スクロール無し状態の座標）」を計算して最大幅を更新
        float absoluteX = x + engine->scrollX;
        if (absoluteX > engine->maxLineWidth) {
            engine->maxLineWidth = absoluteX;
        }

    }

    // ==========================================================
    // ★追加: テキストの「一番最後（末尾）」にカーソルがある場合の処理
    // ==========================================================
    for (size_t vPos : visualCursors) {
        if (currentByteOffset == vPos) {
            float curY = y - engine->lineHeight * 0.8f;
            addRect(cursorVertices, x, curY, cursorWidth, engine->lineHeight, textR, textG, textB, 1.0f);
        }
    }
    std::vector<Vertex> gutterBgVertices;
    std::vector<Vertex> gutterTextVertices;
    float winH = 5000.0f; // 十分な高さを確保
    if (engine->app->window != nullptr) {
        winH = (float)ANativeWindow_getHeight(engine->app->window);
    }
    // ① ガターの背景を描画（少し暗いグレーに。横スクロール時も左端に固定）
    addRect(gutterBgVertices, 0.0f, 0.0f, engine->gutterWidth, winH, 0.15f, 0.15f, 0.15f, 1.0f);
    float gutterTextR = 0.6f, gutterTextG = 0.6f, gutterTextB = 0.6f;
    // ② 各行の行番号テキストを描画
    for (int i = 0; i < engine->lineStarts.size(); ++i) {
        // スクロールを反映した行のY座標
        float lineY = 100.0f - engine->scrollY + i * engine->lineHeight;

        // 画面外なら描画をスキップ（カリング）
        if (lineY + engine->lineHeight < 0.0f || lineY > winH) continue;

        std::string lineNumStr = std::to_string(i + 1);

        // 右寄せにするための文字列幅計算
        float numWidth = 0;
        for(char c : lineNumStr) {
            uint32_t cp = c;
            if (engine->atlas.glyphs.count(cp) == 0 && engine->ftFaceMain != nullptr) {
                engine->atlas.loadChar(engine->ftFaceMain, engine->ftFaceEmoji, cp);
            }
            if (engine->atlas.glyphs.count(cp) > 0) numWidth += engine->atlas.glyphs[cp].advance;
            else numWidth += engine->charWidth;
        }

        // ガターの右端から少し(10px)内側に配置
        float numX = engine->gutterWidth - 10.0f - numWidth;

        // 行番号の頂点生成
        for(char c : lineNumStr) {
            uint32_t cp = c;
            if (engine->atlas.glyphs.count(cp) > 0) {
                GlyphInfo& info = engine->atlas.glyphs[cp];
                float xpos = numX + info.bearingX;
                float ypos = lineY - info.bearingY;
                float w = info.width;
                float h = info.height;

                gutterTextVertices.push_back({{xpos,     ypos    }, {info.u0, info.v0}, 0.0f, {gutterTextR, gutterTextG, gutterTextB, 1.0f}});
                gutterTextVertices.push_back({{xpos,     ypos + h}, {info.u0, info.v1}, 0.0f, {gutterTextR, gutterTextG, gutterTextB, 1.0f}});
                gutterTextVertices.push_back({{xpos + w, ypos    }, {info.u1, info.v0}, 0.0f, {gutterTextR, gutterTextG, gutterTextB, 1.0f}});
                gutterTextVertices.push_back({{xpos + w, ypos    }, {info.u1, info.v0}, 0.0f, {gutterTextR, gutterTextG, gutterTextB, 1.0f}});
                gutterTextVertices.push_back({{xpos,     ypos + h}, {info.u0, info.v1}, 0.0f, {gutterTextR, gutterTextG, gutterTextB, 1.0f}});
                gutterTextVertices.push_back({{xpos + w, ypos + h}, {info.u1, info.v1}, 0.0f, {gutterTextR, gutterTextG, gutterTextB, 1.0f}});

                numX += info.advance;
            } else {
                numX += engine->charWidth;
            }
        }
    }
    // --- ↑ここまで追加 ---

    // ★修正：すべての頂点を 1つの配列に結合する順番を調整
    // (背景 → 下線 → 文字 → カーソル → ガター背景 → ガター文字 の順にすることで、横スクロール時に本文がガターの下に隠れる！)
    std::vector<Vertex> vertices;
    vertices.insert(vertices.end(), bgVertices.begin(), bgVertices.end());
    vertices.insert(vertices.end(), lineVertices.begin(), lineVertices.end());
    vertices.insert(vertices.end(), charVertices.begin(), charVertices.end());
    vertices.insert(vertices.end(), cursorVertices.begin(), cursorVertices.end());

    // ガターは最前面に描画
    vertices.insert(vertices.end(), gutterBgVertices.begin(), gutterBgVertices.end());
    vertices.insert(vertices.end(), gutterTextVertices.begin(), gutterTextVertices.end());

    engine->vertexCount = static_cast<uint32_t>(vertices.size());
    if (engine->vertexCount == 0) return;

    void* data;
    vkMapMemory(engine->device, engine->vertexBufferMemory, 0, sizeof(Vertex) * engine->vertexCount, 0, &data);
    memcpy(data, vertices.data(), sizeof(Vertex) * engine->vertexCount);
    vkUnmapMemory(engine->device, engine->vertexBufferMemory);
}

void renderFrame(Engine* engine) {
    if (engine->device == VK_NULL_HANDLE || !engine->isWindowReady) return;

    // 1. まず前のフレームの描画が終わるのを待つ
    vkWaitForFences(engine->device, 1, &engine->inFlightFence, VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(engine->device, engine->swapchain, UINT64_MAX, engine->imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

    // ★修正1: Androidで頻発する SUBOPTIMAL は「成功」としてそのまま描画を続行させる
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        return;
    }

    // ★修正2: 確実に描画が行われることが確定してから、フェンス（待機フラグ）を下ろす！
    // これにより永遠に待ち続けるデッドロックを防止します
    vkResetFences(engine->device, 1, &engine->inFlightFence);

    vkResetCommandBuffer(engine->commandBuffers[imageIndex], 0);
    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(engine->commandBuffers[imageIndex], &beginInfo);

    // ==========================================================
    // 1. データの準備・転送 (必ず RenderPass の外で行う！)
    // ==========================================================
    updateTextVertices(engine); // 頂点を更新し、未知の文字をアトラスに追加

    if (engine->atlas.isDirty) {
        uint32_t dx = engine->atlas.dirtyMinX;
        uint32_t dy = engine->atlas.dirtyMinY;
        uint32_t dw = engine->atlas.dirtyMaxX - engine->atlas.dirtyMinX;
        uint32_t dh = engine->atlas.dirtyMaxY - engine->atlas.dirtyMinY;

        // 安全対策：更新領域のサイズが0より大きい場合のみ転送
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
    VkClearValue clearColor = {{{0.1f, 0.1f, 0.1f, 1.0f}}};
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
        pc.color[0] = 1.0f; pc.color[1] = 1.0f; pc.color[2] = 1.0f; pc.color[3] = 1.0f;
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
    vkQueuePresentKHR(engine->graphicsQueue, &present);
}

// ============================================================================
// [4] Android 入力・イベントハンドラ (WindowsのWndProcに相当)
// ============================================================================

// Androidからのタッチやキーボード入力を処理するコールバック
// ============================================================================
// Android イベントハンドラ (AInputEvent)
// ============================================================================
static int32_t handleInput(struct android_app* app, AInputEvent* event) {
    Engine* engine = (Engine*)app->userData;

    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
        int32_t action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
        float x = AMotionEvent_getX(event, 0);
        float y = AMotionEvent_getY(event, 0);

        if (action == AMOTION_EVENT_ACTION_DOWN) {
            // タッチ開始位置を記録
            engine->lastTouchX = x;
            engine->lastTouchY = y;
            engine->isDragging = false;

            // キーボードを出す処理
            JNIEnv* env = nullptr;
            app->activity->vm->AttachCurrentThread(&env, nullptr);
            jclass activityClass = env->GetObjectClass(app->activity->clazz);
            jmethodID showImeMethod = env->GetMethodID(activityClass, "showSoftwareKeyboard", "()V");
            if (showImeMethod) env->CallVoidMethod(app->activity->clazz, showImeMethod);
            env->DeleteLocalRef(activityClass);
            app->activity->vm->DetachCurrentThread();

        } else if (action == AMOTION_EVENT_ACTION_MOVE) {
            float dx = x - engine->lastTouchX;
            float dy = y - engine->lastTouchY;

            if (!engine->isDragging && (std::abs(dx) > 10.0f || std::abs(dy) > 10.0f)) {
                engine->isDragging = true;
            }

            if (engine->isDragging) {
                engine->scrollX -= dx;
                engine->scrollY -= dy;

                // ① マイナス方向（一番上・一番左）の制限
                if (engine->scrollX < 0.0f) engine->scrollX = 0.0f;
                if (engine->scrollY < 0.0f) engine->scrollY = 0.0f;

                // ② プラス方向（一番下・一番右）の制限
                if (app->window != nullptr) {
                    float winW = (float)ANativeWindow_getWidth(app->window);
                    float winH = (float)ANativeWindow_getHeight(app->window);

                    // ★修正: 実質的な画面の高さを計算
                    float visibleH = winH - engine->bottomInset;

                    // 縦の最大スクロール量 (行数 × 行の高さ + 上の余白100 + 下に1行分の遊び)
                    float contentH = engine->lineStarts.size() * engine->lineHeight + 100.0f + engine->lineHeight;

                    // ★修正: winH ではなく visibleH を引くことで、キーボード分余計にスクロールできるようにする
                    float maxScrollY = std::max(0.0f, contentH - visibleH);

                    float maxScrollX = std::max(0.0f, engine->maxLineWidth - winW + engine->charWidth * 2.0f);

                    if (engine->scrollY > maxScrollY) engine->scrollY = maxScrollY;
                    if (engine->scrollX > maxScrollX) engine->scrollX = maxScrollX;
                }
                engine->lastTouchX = x;
                engine->lastTouchY = y;
            }
        } else if (action == AMOTION_EVENT_ACTION_UP) {
            if (!engine->isDragging) {
                // ドラッグしていなかった（単なるタップだった）場合はカーソルを移動
                size_t targetPos = getDocPosFromPoint(engine, x, y);
                engine->cursors.clear();
                engine->cursors.push_back({ targetPos, targetPos, getXFromPos(engine, targetPos) });
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
                if (initVulkan(engine)) {
                    engine->isWindowReady = true;
                    // --- テスト文字列を日本語に変更 ---
                    engine->pt.initEmpty();
                    engine->pt.insert(0, "😎薇にちは、miu Android！\r\n爆速UTF-8デコーダが稼働中です。");
                    engine->cursors.push_back({0, 0, 0.0f});
                }
            }
            break;
        case APP_CMD_TERM_WINDOW:
            LOGI("ウィンドウ破棄。");
            engine->isWindowReady = false;
            cleanupVulkan(engine);
            break;
    }
}

// ============================================================================
// [5] エントリポイント
// ============================================================================

void android_main(struct android_app* app) {
    Engine engine = {};
    engine.app = app;
    app->userData = &engine;
    app->onAppCmd = onAppCmd;
    app->onInputEvent = handleInput;
    g_engine = &engine;

    engine.pt.initEmpty(); // 空のドキュメントとして初期化
    rebuildLineStarts(&engine); // 1行目(0文字目スタート)のデータを作成
    engine.cursors.push_back({0, 0, 0.0f}); // カーソルを先頭に配置

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
            // ★描画の直前に、安全に入力イベントを処理する
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