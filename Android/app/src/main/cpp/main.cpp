#include <android_native_app_glue.h>
#include <android/log.h>
#include <algorithm>
#include <vector>
#include <string>
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
};

struct TextAtlas {
    int width = 1024;
    int height = 1024;
    std::vector<uint8_t> pixels;
    std::unordered_map<uint32_t, GlyphInfo> glyphs;

    int currentX = 0;
    int currentY = 0;
    int maxRowHeight = 0;

    void init() {
        pixels.resize(width * height, 0);
    }

    bool loadChar(FT_Face face, uint32_t charCode) {
        if (glyphs.count(charCode) > 0) return true;

        if (FT_Load_Char(face, charCode, FT_LOAD_RENDER)) {
            LOGE("文字のロードに失敗: %u", charCode);
            return false;
        }

        FT_Bitmap* bmp = &face->glyph->bitmap;

        if (currentX + bmp->width + 1 >= width) {
            currentX = 0;
            currentY += maxRowHeight + 1;
            maxRowHeight = 0;
        }

        if (currentY + bmp->rows + 1 >= height) {
            LOGE("テクスチャアトラスの容量が一杯です！");
            return false;
        }

        for (unsigned int row = 0; row < bmp->rows; ++row) {
            for (unsigned int col = 0; col < bmp->width; ++col) {
                pixels[(currentY + row) * width + (currentX + col)] = bmp->buffer[row * bmp->pitch + col];
            }
        }

        GlyphInfo info;
        info.width = (float)bmp->width;
        info.height = (float)bmp->rows;
        info.bearingX = (float)face->glyph->bitmap_left;
        info.bearingY = (float)face->glyph->bitmap_top;
        info.advance = (float)(face->glyph->advance.x >> 6);

        info.u0 = (float)currentX / (float)width;
        info.v0 = (float)currentY / (float)height;
        info.u1 = (float)(currentX + bmp->width) / (float)width;
        info.v1 = (float)(currentY + bmp->rows) / (float)height;

        glyphs[charCode] = info;

        currentX += bmp->width + 1;
        if (bmp->rows > maxRowHeight) maxRowHeight = bmp->rows;

        return true;
    }
};

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
        pieces.clear(); addBuf.clear();
        if (size > 0) pieces.push_back({ true, 0, size });
    }

    void initEmpty() {
        origPtr = nullptr; origSize = 0;
        pieces.clear(); addBuf.clear();
    }

    size_t length() const {
        size_t s = 0; for (auto& p : pieces) s += p.len; return s;
    }

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

// ============================================================================
// [2] Vulkan & Android システム構造体 (WindowsのEditorクラスに相当)
// ============================================================================

// ============================================================================
// 描画用データ構造体 (Engine構造体の上に配置)
// ============================================================================
struct Vertex {
    float pos[2]; // 画面上のX, Y座標
    float uv[2];  // テクスチャ上のU, V座標
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
    bool isWindowReady;

    // --- エディタの状態 ---
    PieceTable pt;
    UndoManager undo;
    std::vector<Cursor> cursors;
    std::vector<size_t> lineStarts;
    bool isDirty = false;
    int vScrollPos = 0;
    int hScrollPos = 0;
    float maxLineWidth = 100.0f;
    float gutterWidth = 50.0f;

    float currentFontSize = 48.0f; // 21.0f から 48.0f へ
    float lineHeight = 60.0f;      // 26.25f から 60.0f へ (48の1.25倍)
    float charWidth = 24.0f;       // 10.0f から 24.0f へ (48の半分程度)

    // --- Vulkan オブジェクト ---
    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue graphicsQueue;
    uint32_t queueFamilyIndex;

// --- 追加するフォント関連変数 ---
    FT_Library ftLibrary;
    FT_Face ftFace;
    std::vector<uint8_t> fontData;
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
};

// ============================================================================
// [3] Vulkan 初期化・破棄・描画ロジック (前回の内容)
// ============================================================================

// ※コードが長くなるため、前回の cleanupVulkan, createSwapchain, createRenderPass,
// createFramebuffers, createCommandBuffers, createSyncObjects, initVulkan は
// 変更なしでそのままここに配置します。

void cleanupVulkan(Engine* engine) {
    if (engine->graphicsPipeline != VK_NULL_HANDLE) vkDestroyPipeline(engine->device, engine->graphicsPipeline, nullptr);

    if (engine->device == VK_NULL_HANDLE) return;
    vkDeviceWaitIdle(engine->device);

    if (engine->vertexBuffer != VK_NULL_HANDLE) vkDestroyBuffer(engine->device, engine->vertexBuffer, nullptr);
    if (engine->vertexBufferMemory != VK_NULL_HANDLE) vkFreeMemory(engine->device, engine->vertexBufferMemory, nullptr);

    if (engine->pipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(engine->device, engine->pipelineLayout, nullptr);
    if (engine->descriptorPool != VK_NULL_HANDLE) vkDestroyDescriptorPool(engine->device, engine->descriptorPool, nullptr);
    if (engine->descriptorSetLayout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(engine->device, engine->descriptorSetLayout, nullptr);

    if (engine->fontSampler != VK_NULL_HANDLE) vkDestroySampler(engine->device, engine->fontSampler, nullptr);
    if (engine->fontImageView != VK_NULL_HANDLE) vkDestroyImageView(engine->device, engine->fontImageView, nullptr);
    if (engine->fontImage != VK_NULL_HANDLE) vkDestroyImage(engine->device, engine->fontImage, nullptr);
    if (engine->fontImageMemory != VK_NULL_HANDLE) vkFreeMemory(engine->device, engine->fontImageMemory, nullptr);

    if (engine->ftFace) { FT_Done_Face(engine->ftFace); engine->ftFace = nullptr; }
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
    VkDeviceSize imageSize = engine->atlas.width * engine->atlas.height;
    if (imageSize == 0 || engine->atlas.pixels.empty()) return false;

    // 1. CPUとGPUの橋渡し用バッファ（ステージングバッファ）の作成
    VkBuffer stagingBuffer; VkDeviceMemory stagingBufferMemory;
    createBuffer(engine, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    // 2. ピクセルデータをバッファにコピー
    void* data;
    vkMapMemory(engine->device, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, engine->atlas.pixels.data(), static_cast<size_t>(imageSize));
    vkUnmapMemory(engine->device, stagingBufferMemory);

    // 3. GPU上に最適な形式でImageを作成（R8_UNORM: 1チャンネルのグレースケール）
    VkImageCreateInfo imageInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = engine->atlas.width; imageInfo.extent.height = engine->atlas.height; imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1; imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8_UNORM;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT; imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateImage(engine->device, &imageInfo, nullptr, &engine->fontImage) != VK_SUCCESS) return false;

    // メモリ割り当てとバインド
    VkMemoryRequirements memRequirements; vkGetImageMemoryRequirements(engine->device, engine->fontImage, &memRequirements);
    VkMemoryAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(engine->physicalDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkAllocateMemory(engine->device, &allocInfo, nullptr, &engine->fontImageMemory);
    vkBindImageMemory(engine->device, engine->fontImage, engine->fontImageMemory, 0);

    // 4. 画像を「データ受信待ち」にして、バッファからコピーし、最後に「シェーダ読取用」にする
    transitionImageLayout(engine, engine->fontImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(engine, stagingBuffer, engine->fontImage, engine->atlas.width, engine->atlas.height);
    transitionImageLayout(engine, engine->fontImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // 一時バッファの破棄
    vkDestroyBuffer(engine->device, stagingBuffer, nullptr);
    vkFreeMemory(engine->device, stagingBufferMemory, nullptr);

    // 5. 画像へアクセスするための ImageView の作成
    VkImageViewCreateInfo viewInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = engine->fontImage; viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D; viewInfo.format = VK_FORMAT_R8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1; viewInfo.subresourceRange.baseArrayLayer = 0; viewInfo.subresourceRange.layerCount = 1;
    if (vkCreateImageView(engine->device, &viewInfo, nullptr, &engine->fontImageView) != VK_SUCCESS) return false;

    // 6. 拡大縮小時の滑らかさを決める Sampler の作成
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

    VkVertexInputAttributeDescription attributeDescriptions[2] = {};
    attributeDescriptions[0].binding = 0; attributeDescriptions[0].location = 0; attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT; attributeDescriptions[0].offset = offsetof(Vertex, pos);
    attributeDescriptions[1].binding = 0; attributeDescriptions[1].location = 1; attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT; attributeDescriptions[1].offset = offsetof(Vertex, uv);

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInputInfo.vertexBindingDescriptionCount = 1; vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = 2; vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions;

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

    // --- 頂点バッファの作成 (10,000文字 = 60,000頂点分を確保) ---
    VkDeviceSize bufferSize = sizeof(Vertex) * 60000;
    createBuffer(engine, bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, engine->vertexBuffer, engine->vertexBufferMemory);

    // --- FreeType初期化（重複を削除し1回だけにしました） ---
    if (FT_Init_FreeType(&engine->ftLibrary)) {
        LOGE("FreeTypeの初期化に失敗！"); return false;
    }
    engine->fontData = loadSystemFont();
    if (!engine->fontData.empty()) {
        if (FT_New_Memory_Face(engine->ftLibrary, engine->fontData.data(), engine->fontData.size(), 0, &engine->ftFace) == 0) {
            FT_Set_Pixel_Sizes(engine->ftFace, 0, 48);
            engine->atlas.init();
            for (uint32_t i = 32; i < 127; ++i) engine->atlas.loadChar(engine->ftFace, i);
            engine->atlas.loadChar(engine->ftFace, 0x3042); // 「あ」

            createTextTexture(engine); // GPUへ転送
        }
    }

    if (!createDescriptors(engine)) return false;
    if (!createPipelineLayout(engine)) return false;
    if (!createGraphicsPipeline(engine)) return false;

    return true;
}

// 毎フレーム、テキストから頂点データを計算してGPUバッファに送る
void updateTextVertices(Engine* engine) {
    std::vector<Vertex> vertices;
    float x = 50.0f; // 左からのマージン
    float y = 100.0f; // 上からのマージン

    // PieceTableからテキスト全体を取得
    size_t len = engine->pt.length();
    std::string text = engine->pt.getRange(0, len);

    for (char c : text) {
        if (c == '\n') {
            x = 50.0f;
            y += engine->lineHeight;
            continue;
        }

        // 簡易的なASCII文字の描画（今回はUTF-8デコードを省略し1バイトずつ処理）
        uint32_t charCode = (uint8_t)c;
        if (engine->atlas.glyphs.count(charCode) == 0) continue; // アトラスに無い文字はスキップ

        GlyphInfo& info = engine->atlas.glyphs[charCode];

        float xpos = x + info.bearingX;
        float ypos = y - info.bearingY;
        float w = info.width;
        float h = info.height;

        // 1文字につき四角形（三角形2つ = 6頂点）を作成
        vertices.push_back({{xpos,     ypos    }, {info.u0, info.v0}});
        vertices.push_back({{xpos,     ypos + h}, {info.u0, info.v1}});
        vertices.push_back({{xpos + w, ypos    }, {info.u1, info.v0}});

        vertices.push_back({{xpos + w, ypos    }, {info.u1, info.v0}});
        vertices.push_back({{xpos,     ypos + h}, {info.u0, info.v1}});
        vertices.push_back({{xpos + w, ypos + h}, {info.u1, info.v1}});

        x += info.advance;
    }

    engine->vertexCount = static_cast<uint32_t>(vertices.size());
    if (engine->vertexCount == 0) return;

    // 生成した頂点データをVulkanのバッファ（メモリ）にコピー
    void* data;
    vkMapMemory(engine->device, engine->vertexBufferMemory, 0, sizeof(Vertex) * engine->vertexCount, 0, &data);
    memcpy(data, vertices.data(), sizeof(Vertex) * engine->vertexCount);
    vkUnmapMemory(engine->device, engine->vertexBufferMemory);
}

void renderFrame(Engine* engine) {
    if (engine->device == VK_NULL_HANDLE || !engine->isWindowReady) return;
    vkWaitForFences(engine->device, 1, &engine->inFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(engine->device, 1, &engine->inFlightFence);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(engine->device, engine->swapchain, UINT64_MAX, engine->imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
    if (result != VK_SUCCESS) return;

    vkResetCommandBuffer(engine->commandBuffers[imageIndex], 0);
    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(engine->commandBuffers[imageIndex], &beginInfo);

    VkRenderPassBeginInfo rpBegin = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpBegin.renderPass = engine->renderPass; rpBegin.framebuffer = engine->framebuffers[imageIndex]; rpBegin.renderArea.extent = engine->swapchainExtent;
    VkClearValue clearColor = {{{0.1f, 0.1f, 0.1f, 1.0f}}}; // ダークモード背景色相当
    rpBegin.clearValueCount = 1; rpBegin.pClearValues = &clearColor;

    vkCmdBeginRenderPass(engine->commandBuffers[imageIndex], &rpBegin, VK_SUBPASS_CONTENTS_INLINE);



// ========================================================================
    // テキストの描画処理
    // ========================================================================
    updateTextVertices(engine); // 頂点を更新

    vkCmdBindPipeline(engine->commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, engine->graphicsPipeline);


    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)engine->swapchainExtent.width;
    viewport.height = (float)engine->swapchainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(engine->commandBuffers[imageIndex], 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = engine->swapchainExtent;
    vkCmdSetScissor(engine->commandBuffers[imageIndex], 0, 1, &scissor);


    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(engine->commandBuffers[imageIndex], 0, 1, &engine->vertexBuffer, offsets);

    vkCmdBindDescriptorSets(engine->commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, engine->pipelineLayout, 0, 1, &engine->descriptorSet, 0, nullptr);

    // 画面サイズと文字色（白）をシェーダに送信
    PushConstants pc;
    pc.screenWidth = (float)engine->swapchainExtent.width;
    pc.screenHeight = (float)engine->swapchainExtent.height;
    pc.color[0] = 1.0f; pc.color[1] = 1.0f; pc.color[2] = 1.0f; pc.color[3] = 1.0f;
    vkCmdPushConstants(engine->commandBuffers[imageIndex], engine->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pc);

    if (engine->vertexCount > 0) {
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
static int32_t handleInput(struct android_app* app, AInputEvent* event) {
    Engine* engine = (Engine*)app->userData;

    // タッチイベントの処理 (Windowsの WM_LBUTTONDOWN 等に相当)
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
        int32_t action = AMotionEvent_getAction(event);
        if (action == AMOTION_EVENT_ACTION_DOWN) {
            float x = AMotionEvent_getX(event, 0);
            float y = AMotionEvent_getY(event, 0);
            LOGI("Touch Down: x=%f, y=%f", x, y);
            // 今後、ここに getDocPosFromPoint などを呼び出すロジックを接続します
        }
        return 1; // 処理済み
    }
        // キーボードイベントの処理 (Windowsの WM_KEYDOWN, WM_CHAR 等に相当)
    else if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_KEY) {
        int32_t action = AKeyEvent_getAction(event);
        if (action == AKEY_EVENT_ACTION_DOWN) {
            int32_t keyCode = AKeyEvent_getKeyCode(event);
            LOGI("Key Down: %d", keyCode);
            // 今後、ここに insertAtCursors や Delete 処理を接続します
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
                    // テスト用: 初期テキストの挿入
                    engine->pt.initEmpty();
                    engine->pt.insert(0, "Welcome to miu Android!\nThis is the core PieceTable running.");
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
    app->onInputEvent = handleInput; // 入力コールバックを登録

    while (true) {
        int events;
        struct android_poll_source* source;
        int timeout = engine.isWindowReady ? 0 : -1;

        while (ALooper_pollOnce(timeout, nullptr, &events, (void**)&source) >= 0) {
            if (source != nullptr) source->process(app, source);
            if (app->destroyRequested != 0) {
                cleanupVulkan(&engine);
                return;
            }
            timeout = engine.isWindowReady ? 0 : -1;
        }

        if (engine.isWindowReady) {
            renderFrame(&engine);
        }
    }
}