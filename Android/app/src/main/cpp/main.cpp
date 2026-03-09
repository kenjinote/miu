#include <android_native_app_glue.h>
#include <android/log.h>
#include <algorithm>
#include <vector>

// Android特有のVulkan拡張を有効化
#define VK_USE_PLATFORM_ANDROID_KHR
#include <vulkan/vulkan.h>

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "miu", __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, "miu", __VA_ARGS__))

// --- 構造体定義 ---
struct Engine {
    struct android_app* app;
    bool isWindowReady;

    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue graphicsQueue;
    uint32_t queueFamilyIndex;

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
};

// --- リソース解放 ---
void cleanupVulkan(Engine* engine) {
    if (engine->device == VK_NULL_HANDLE) return;
    vkDeviceWaitIdle(engine->device);

    if (engine->imageAvailableSemaphore != VK_NULL_HANDLE) vkDestroySemaphore(engine->device, engine->imageAvailableSemaphore, nullptr);
    if (engine->renderFinishedSemaphore != VK_NULL_HANDLE) vkDestroySemaphore(engine->device, engine->renderFinishedSemaphore, nullptr);
    if (engine->inFlightFence != VK_NULL_HANDLE) vkDestroyFence(engine->device, engine->inFlightFence, nullptr);

    if (engine->commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(engine->device, engine->commandPool, nullptr);
    }

    for (auto fb : engine->framebuffers) {
        if (fb != VK_NULL_HANDLE) vkDestroyFramebuffer(engine->device, fb, nullptr);
    }
    engine->framebuffers.clear();

    if (engine->renderPass != VK_NULL_HANDLE) vkDestroyRenderPass(engine->device, engine->renderPass, nullptr);

    for (auto iv : engine->swapchainImageViews) {
        if (iv != VK_NULL_HANDLE) vkDestroyImageView(engine->device, iv, nullptr);
    }
    engine->swapchainImageViews.clear();

    if (engine->swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(engine->device, engine->swapchain, nullptr);
        engine->swapchain = VK_NULL_HANDLE;
    }

    vkDestroyDevice(engine->device, nullptr);
    engine->device = VK_NULL_HANDLE;

    if (engine->surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(engine->instance, engine->surface, nullptr);
        engine->surface = VK_NULL_HANDLE;
    }

    if (engine->instance != VK_NULL_HANDLE) {
        vkDestroyInstance(engine->instance, nullptr);
        engine->instance = VK_NULL_HANDLE;
    }
    LOGI("Vulkanリソースを破棄しました。");
}

// --- 初期化補助関数群 ---

bool createSwapchain(Engine* engine) {
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(engine->physicalDevice, engine->surface, &capabilities);

    if (capabilities.currentExtent.width != 0xFFFFFFFF) {
        engine->swapchainExtent = capabilities.currentExtent;
    } else {
        int32_t w = ANativeWindow_getWidth(engine->app->window);
        int32_t h = ANativeWindow_getHeight(engine->app->window);
        engine->swapchainExtent.width = std::clamp((uint32_t)w, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        engine->swapchainExtent.height = std::clamp((uint32_t)h, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    }

    if (engine->swapchainExtent.width == 0 || engine->swapchainExtent.height == 0) return false;

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(engine->physicalDevice, engine->surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(engine->physicalDevice, engine->surface, &formatCount, formats.data());

    VkSurfaceFormatKHR selectedFormat = formats[0];
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM || f.format == VK_FORMAT_R8G8B8A8_UNORM) {
            selectedFormat = f;
            break;
        }
    }
    engine->swapchainFormat = selectedFormat.format;

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) imageCount = capabilities.maxImageCount;

    VkSwapchainCreateInfoKHR createInfo = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    createInfo.surface = engine->surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = engine->swapchainFormat;
    createInfo.imageColorSpace = selectedFormat.colorSpace;
    createInfo.imageExtent = engine->swapchainExtent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    createInfo.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(engine->device, &createInfo, nullptr, &engine->swapchain) != VK_SUCCESS) return false;

    vkGetSwapchainImagesKHR(engine->device, engine->swapchain, &imageCount, nullptr);
    engine->swapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(engine->device, engine->swapchain, &imageCount, engine->swapchainImages.data());

    engine->swapchainImageViews.resize(imageCount);
    for (size_t i = 0; i < imageCount; i++) {
        VkImageViewCreateInfo viewInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = engine->swapchainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = engine->swapchainFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;
        vkCreateImageView(engine->device, &viewInfo, nullptr, &engine->swapchainImageViews[i]);
    }
    LOGI("スワップチェーン作成成功: %dx%d", engine->swapchainExtent.width, engine->swapchainExtent.height);
    return true;
}

bool createRenderPass(Engine* engine) {
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = engine->swapchainFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkRenderPassCreateInfo renderPassInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    return vkCreateRenderPass(engine->device, &renderPassInfo, nullptr, &engine->renderPass) == VK_SUCCESS;
}

bool createFramebuffers(Engine* engine) {
    engine->framebuffers.resize(engine->swapchainImageViews.size());
    for (size_t i = 0; i < engine->swapchainImageViews.size(); i++) {
        VkImageView attachments[] = { engine->swapchainImageViews[i] };
        VkFramebufferCreateInfo fbInfo = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fbInfo.renderPass = engine->renderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = attachments;
        fbInfo.width = engine->swapchainExtent.width;
        fbInfo.height = engine->swapchainExtent.height;
        fbInfo.layers = 1;
        if (vkCreateFramebuffer(engine->device, &fbInfo, nullptr, &engine->framebuffers[i]) != VK_SUCCESS) return false;
    }
    return true;
}

bool createCommandBuffers(Engine* engine) {
    VkCommandPoolCreateInfo poolInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolInfo.queueFamilyIndex = engine->queueFamilyIndex;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(engine->device, &poolInfo, nullptr, &engine->commandPool) != VK_SUCCESS) return false;

    engine->commandBuffers.resize(engine->framebuffers.size());
    VkCommandBufferAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocInfo.commandPool = engine->commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)engine->commandBuffers.size();
    return vkAllocateCommandBuffers(engine->device, &allocInfo, engine->commandBuffers.data()) == VK_SUCCESS;
}

bool createSyncObjects(Engine* engine) {
    VkSemaphoreCreateInfo semInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fenceInfo = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    return vkCreateSemaphore(engine->device, &semInfo, nullptr, &engine->imageAvailableSemaphore) == VK_SUCCESS &&
           vkCreateSemaphore(engine->device, &semInfo, nullptr, &engine->renderFinishedSemaphore) == VK_SUCCESS &&
           vkCreateFence(engine->device, &fenceInfo, nullptr, &engine->inFlightFence) == VK_SUCCESS;
}

// --- メインロジック ---

bool initVulkan(Engine* engine) {
    VkApplicationInfo appInfo = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    appInfo.apiVersion = VK_API_VERSION_1_0;

    std::vector<const char*> instExt = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_ANDROID_SURFACE_EXTENSION_NAME};
    VkInstanceCreateInfo instInfo = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    instInfo.pApplicationInfo = &appInfo;
    instInfo.enabledExtensionCount = (uint32_t)instExt.size();
    instInfo.ppEnabledExtensionNames = instExt.data();
    if (vkCreateInstance(&instInfo, nullptr, &engine->instance) != VK_SUCCESS) return false;

    VkAndroidSurfaceCreateInfoKHR surfInfo = {VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR};
    surfInfo.window = engine->app->window;
    if (vkCreateAndroidSurfaceKHR(engine->instance, &surfInfo, nullptr, &engine->surface) != VK_SUCCESS) return false;

    uint32_t gpuCount = 0;
    vkEnumeratePhysicalDevices(engine->instance, &gpuCount, nullptr);
    std::vector<VkPhysicalDevice> gpus(gpuCount);
    vkEnumeratePhysicalDevices(engine->instance, &gpuCount, gpus.data());
    engine->physicalDevice = gpus[0];

    uint32_t qfCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(engine->physicalDevice, &qfCount, nullptr);
    std::vector<VkQueueFamilyProperties> qfProps(qfCount);
    vkGetPhysicalDeviceQueueFamilyProperties(engine->physicalDevice, &qfCount, qfProps.data());
    for (uint32_t i = 0; i < qfCount; i++) {
        if (qfProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            VkBool32 support = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(engine->physicalDevice, i, engine->surface, &support);
            if (support) { engine->queueFamilyIndex = i; break; }
        }
    }

    float priority = 1.0f;
    VkDeviceQueueCreateInfo qInfo = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qInfo.queueFamilyIndex = engine->queueFamilyIndex;
    qInfo.queueCount = 1;
    qInfo.pQueuePriorities = &priority;

    std::vector<const char*> devExt = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    VkDeviceCreateInfo devInfo = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    devInfo.queueCreateInfoCount = 1;
    devInfo.pQueueCreateInfos = &qInfo;
    devInfo.enabledExtensionCount = (uint32_t)devExt.size();
    devInfo.ppEnabledExtensionNames = devExt.data();
    if (vkCreateDevice(engine->physicalDevice, &devInfo, nullptr, &engine->device) != VK_SUCCESS) return false;

    vkGetDeviceQueue(engine->device, engine->queueFamilyIndex, 0, &engine->graphicsQueue);

    if (!createSwapchain(engine)) return false;
    if (!createRenderPass(engine)) return false;
    if (!createFramebuffers(engine)) return false;
    if (!createCommandBuffers(engine)) return false;
    if (!createSyncObjects(engine)) return false;

    return true;
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
    rpBegin.renderPass = engine->renderPass;
    rpBegin.framebuffer = engine->framebuffers[imageIndex];
    rpBegin.renderArea.extent = engine->swapchainExtent;
    VkClearValue clearColor = {{{0.1f, 0.1f, 0.1f, 1.0f}}};
    rpBegin.clearValueCount = 1;
    rpBegin.pClearValues = &clearColor;

    vkCmdBeginRenderPass(engine->commandBuffers[imageIndex], &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdEndRenderPass(engine->commandBuffers[imageIndex]);
    vkEndCommandBuffer(engine->commandBuffers[imageIndex]);

    VkSubmitInfo submit = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    VkSemaphore waitSem[] = {engine->imageAvailableSemaphore};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = waitSem;
    submit.pWaitDstStageMask = waitStages;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &engine->commandBuffers[imageIndex];
    VkSemaphore sigSem[] = {engine->renderFinishedSemaphore};
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = sigSem;

    vkQueueSubmit(engine->graphicsQueue, 1, &submit, engine->inFlightFence);

    VkPresentInfoKHR present = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = sigSem;
    present.swapchainCount = 1;
    present.pSwapchains = &engine->swapchain;
    present.pImageIndices = &imageIndex;
    vkQueuePresentKHR(engine->graphicsQueue, &present);
}

// --- OSイベントコールバック ---

static void onAppCmd(struct android_app* app, int32_t cmd) {
    Engine* engine = (Engine*)app->userData;
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            if (app->window != nullptr) {
                LOGI("ウィンドウが生成されました。Vulkanを初期化します。");
                if (initVulkan(engine)) {
                    engine->isWindowReady = true;
                }
            }
            break;
        case APP_CMD_TERM_WINDOW:
            LOGI("ウィンドウが破棄されます。");
            engine->isWindowReady = false;
            cleanupVulkan(engine);
            break;
    }
}

// --- エントリポイント ---

void android_main(struct android_app* app) {
    Engine engine = {};
    engine.app = app;
    app->userData = &engine;
    app->onAppCmd = onAppCmd;

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