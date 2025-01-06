#include <vulkan/vulkan.h>
#ifdef WINDOWS_BUILD
#include <vulkan/vulkan_win32.h>
#elif LINUX_BUILD
#endif
#include <iostream>
#include "vk_init.hpp"
#include <wtypes.h>
#include <platform.h>

// #include "vk_init.cpp"

#define ArraySize(arr) sizeof((arr)) / sizeof((arr)[0])

#define VK_CHECK(result)                                           \
    if (result != VK_SUCCESS)                                      \
    {                                                              \
        std::cout << "Vulkan Error Code: " << result << std::endl; \
        __debugbreak();                                            \
        return false;                                              \
    }

static VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT msgSeverity,
    VkDebugUtilsMessageTypeFlagsEXT msgTFlags,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void *pUserData){
        std::cout << "Validation error: " << pCallbackData->pMessage << std::endl;
        return false;
    };

struct VkContext
{
    VkExtent2D screenSize;

    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkSurfaceKHR surface;
    VkSurfaceFormatKHR surfaceFormat;
    VkPhysicalDevice gpu;
    VkDevice device;
    VkQueue graphicsQueue;
    VkSwapchainKHR swapchain;
    VkRenderPass renderPass;
    VkCommandPool commandPool;

    VkSemaphore submitSemaphore;
    VkSemaphore acquireSemaphore;


    uint32_t scImgCount;
    // TODO: Suballocate from Main memory allocation
    VkImage scImages[5];
    VkImageView scImageViews[5];
    VkFramebuffer frameBuffers[5];

    int graphicsIndex;
};

bool vk_init(VkContext *vkcontext, void *window)
{
    platform_get_window_size(&vkcontext->screenSize.width, &vkcontext->screenSize.height);

    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Pong";
    appInfo.pEngineName = "PongEngine";

    // Create Vulkan instance information sType and pApplication
    VkInstanceCreateInfo instanceInfo = {};

    char *extensions[] = {
#ifdef WINDOWS_BUILD
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif LINUX_BUILD
#endif
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
        VK_KHR_SURFACE_EXTENSION_NAME,
    };

    char *layers[] = {
        "VK_LAYER_KHRONOS_validation",
    };

    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &appInfo;
    instanceInfo.ppEnabledExtensionNames = extensions;
    instanceInfo.enabledExtensionCount = ArraySize(extensions);
    instanceInfo.ppEnabledLayerNames = layers;
    instanceInfo.enabledLayerCount = ArraySize(layers);

    // Initialize Vulkan instance with default settings
    VK_CHECK(vkCreateInstance(&instanceInfo, 0, &vkcontext->instance));

    auto vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vkcontext->instance, "vkCreateDebugUtilsMessengerEXT");

    if (vkCreateDebugUtilsMessengerEXT) {
        VkDebugUtilsMessengerCreateInfoEXT debugInfo = {};
        debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
        debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
        debugInfo.pfnUserCallback = vk_debug_callback;
        vkCreateDebugUtilsMessengerEXT(vkcontext->instance, &debugInfo, 0, &vkcontext->debugMessenger);
    }
    else
    {
        return false;
    }
    

    {
    // Create Vulkan surface
#ifdef WINDOWS_BUILD
        VkWin32SurfaceCreateInfoKHR surfaceInfo = {};
        surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        surfaceInfo.hwnd = (HWND)window;
        surfaceInfo.hinstance = GetModuleHandle(0);
        VK_CHECK(vkCreateWin32SurfaceKHR(vkcontext->instance, &surfaceInfo, 0, &vkcontext->surface));
#elif LINUX_BUILD
#endif
    }

    // Choose GPU implementation and Selection
    {
        vkcontext->graphicsIndex = -1;
        uint32_t gpuCount = 0;
        // TODO: Suballocate from Main memory allocation
        VkPhysicalDevice gpus[10];
        VK_CHECK(vkEnumeratePhysicalDevices(vkcontext->instance, &gpuCount, 0));
        VK_CHECK(vkEnumeratePhysicalDevices(vkcontext->instance, &gpuCount, gpus));

        for (uint32_t i = 0; i < gpuCount; i++)
        {
            VkPhysicalDevice gpu = gpus[i];

            uint32_t queueFamilyCount = 0;
            // TODO: Suballocate from Main memory allocation
            VkQueueFamilyProperties queueFamilyProps[10];
            vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, 0);
            vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, queueFamilyProps);

            for (uint32_t j = 0; j < queueFamilyCount; j++)
            {
                if (queueFamilyProps[j].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                {
                    VkBool32 surfaceSupport = VK_FALSE;
                    VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(gpu, j, vkcontext->surface, &surfaceSupport));

                    if (surfaceSupport == VK_TRUE)
                    {
                        vkcontext->graphicsIndex = j;
                        vkcontext->gpu = gpu;
                        break;
                    }
                }
            }
        }

        if (vkcontext->graphicsIndex < 0)
        {
            return false;
        }
    }

    // Logical Device
    {
        float queuePriority = 1.0f;

        VkDeviceQueueCreateInfo queueInfo = {};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = vkcontext->graphicsIndex;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &queuePriority;

        char *extensions[] = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        };

        VkDeviceCreateInfo deviceInfo = {};
        deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceInfo.pQueueCreateInfos = &queueInfo;
        deviceInfo.queueCreateInfoCount = 1;
        deviceInfo.ppEnabledExtensionNames = extensions;
        deviceInfo.enabledExtensionCount = ArraySize(extensions);

        VK_CHECK(vkCreateDevice(vkcontext->gpu, &deviceInfo, 0, &vkcontext->device));

        vkGetDeviceQueue(vkcontext->device, vkcontext->graphicsIndex, 0, &vkcontext->graphicsQueue);
    }

    // SwapChain Device
    {
        {
        uint32_t formatCount = 0;
        //TODO: Suballocation from Main Memory
        VkSurfaceFormatKHR surfaceFormats[10];
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(vkcontext->gpu, vkcontext->surface, &formatCount, 0));
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(vkcontext->gpu, vkcontext->surface, &formatCount, surfaceFormats));

        for (uint32_t i = 0; i < formatCount; i++)
        {
            VkSurfaceFormatKHR format = surfaceFormats[i];

            if (format.format == VK_FORMAT_B8G8R8A8_SRGB)
            {
                vkcontext->surfaceFormat = format;
                break;
            }
        }

        VkSurfaceCapabilitiesKHR surfaceCaps = {};
        VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkcontext->gpu, vkcontext->surface, &surfaceCaps));
        uint32_t imgCount = surfaceCaps.minImageCount + 1;
        imgCount = imgCount > surfaceCaps.maxImageCount ? imgCount - 1 : imgCount;

        VkSwapchainCreateInfoKHR scInfo = {};
        scInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        scInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        scInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        scInfo.surface = vkcontext->surface;
        scInfo.imageFormat = vkcontext->surfaceFormat.format;
        scInfo.preTransform = surfaceCaps.currentTransform;
        scInfo.imageExtent = surfaceCaps.currentExtent;
        scInfo.minImageCount = imgCount;
        scInfo.imageArrayLayers = 1;

        VK_CHECK(vkCreateSwapchainKHR(vkcontext->device, &scInfo, 0, &vkcontext->swapchain));

        VK_CHECK(vkGetSwapchainImagesKHR(vkcontext->device, vkcontext->swapchain, &vkcontext->scImgCount, 0));
        VK_CHECK(vkGetSwapchainImagesKHR(vkcontext->device, vkcontext->swapchain, &vkcontext->scImgCount, vkcontext->scImages));

        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.format = vkcontext->surfaceFormat.format;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;
        for (uint32_t i = 0; i < vkcontext->scImgCount; i++)
        {
            viewInfo.image = vkcontext->scImages[i];
            VK_CHECK(vkCreateImageView(vkcontext->device, &viewInfo, 0, &vkcontext->scImageViews[i]));
        }
        }
    }

    // Render pass
    {
        VkAttachmentDescription colorAttachment = {};
        colorAttachment.format = vkcontext->surfaceFormat.format;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentDescription attachments[] = {
            colorAttachment
        };

        VkAttachmentReference colorAttachmentRef = {};
        colorAttachmentRef.attachment = 0; // This is an index into the attachments array
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpassDesc = {};
        subpassDesc.colorAttachmentCount = 1;
        subpassDesc.pColorAttachments = &colorAttachmentRef;


        VkRenderPassCreateInfo rpInfo = {};
        rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpInfo.pAttachments = attachments;
        rpInfo.attachmentCount = ArraySize(attachments);
        rpInfo.subpassCount = 1;
        rpInfo.pSubpasses = &subpassDesc;

        VK_CHECK(vkCreateRenderPass(vkcontext->device, &rpInfo, 0, &vkcontext->renderPass));
    }

    // Frame Buffer
    {
        VkFramebufferCreateInfo fbInfo = {};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.height = vkcontext->screenSize.height;
        fbInfo.width = vkcontext->screenSize.width;
        fbInfo.renderPass = vkcontext->renderPass;
        fbInfo.layers = 1;
        fbInfo.attachmentCount = 1;

        for (uint32_t i = 0; i < vkcontext->scImgCount; i++)
        {
            fbInfo.pAttachments = &vkcontext->scImageViews[i];
            VK_CHECK(vkCreateFramebuffer(vkcontext->device, &fbInfo, 0, &vkcontext->frameBuffers[i]));
        }
        

    }

    // Command Pool
    { 
        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = vkcontext->graphicsIndex;
        VK_CHECK(vkCreateCommandPool(vkcontext->device,&poolInfo, 0, &vkcontext->commandPool));
    }

    // Sync Object
    {
        VkSemaphoreCreateInfo semaInfo = {};
        semaInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VK_CHECK(vkCreateSemaphore(vkcontext->device, &semaInfo, 0, &vkcontext->acquireSemaphore));
        VK_CHECK(vkCreateSemaphore(vkcontext->device, &semaInfo, 0, &vkcontext->submitSemaphore));
    }

    return true;
}

bool vk_render(VkContext* vkcontext) {
    uint32_t imgIdx;
    VK_CHECK(vkAcquireNextImageKHR(vkcontext->device, vkcontext->swapchain, 0, vkcontext->acquireSemaphore, 0, &imgIdx));

    // Command Buffer
    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandBufferCount = 1;
    allocInfo.commandPool = vkcontext->commandPool;
    VK_CHECK(vkAllocateCommandBuffers(vkcontext->device, &allocInfo, &cmd));

    // Begin Command Buffer
    VkCommandBufferBeginInfo beginInfo = cmd_begin_info();
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    // Render Pass

    VkClearValue clearValue = {};
    clearValue.color = {1, 1, 0, 1};

    VkRenderPassBeginInfo rpBeginInfo = {};
    rpBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBeginInfo.renderPass = vkcontext->renderPass;
    rpBeginInfo.renderArea.extent = vkcontext->screenSize;
    rpBeginInfo.framebuffer = vkcontext->frameBuffers[imgIdx];
    rpBeginInfo.pClearValues = &clearValue;
    rpBeginInfo.clearValueCount = 1;

    vkCmdBeginRenderPass(cmd, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);    

    vkCmdEndRenderPass(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    submitInfo.pSignalSemaphores = &vkcontext->submitSemaphore;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &vkcontext->acquireSemaphore;
    submitInfo.waitSemaphoreCount = 1;
    VK_CHECK(vkQueueSubmit(vkcontext->graphicsQueue, 1, &submitInfo, 0));
    
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pSwapchains = &vkcontext->swapchain;
    presentInfo.swapchainCount = 1;
    presentInfo.pImageIndices = &imgIdx;
    presentInfo.pWaitSemaphores = &vkcontext->submitSemaphore;
    presentInfo.waitSemaphoreCount = 1;
    VK_CHECK(vkQueuePresentKHR(vkcontext->graphicsQueue, &presentInfo));

    VK_CHECK(vkDeviceWaitIdle(vkcontext->device));

    vkFreeCommandBuffers(vkcontext->device, vkcontext->commandPool, 1, &cmd);

    return true;
}

