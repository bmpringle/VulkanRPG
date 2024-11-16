/*

NOTE:

This was created in significant part by following vulkan-tutorial.com.
Large parts of it have been modified in their structure, but it
should still be noted that it was created with frequent reference and 
(in the case of a large part of the boilerplate structures), copy/paste
before making any necessary modifications.

There are also some helper functions that have undergone no modification at all
from their versions at vulkan-tutorial.com. An example of this is readFile, since
it is simple and needed no modifications for my use case.

*/

#define VK_ENABLE_BETA_EXTENSIONS
#include "volk/volk.h"
#include <SDL2/SDL.h>
#include <iostream>
#include <SDL2/SDL_vulkan.h>
#include <vector>
#include <tuple>
#include <set>
#include <unordered_map>
#include <fstream>
#include <vulkan/vk_enum_string_helper.h>
#include <glm/glm.hpp>

typedef std::unordered_map<std::string, std::function<bool(VkQueueFamilyProperties, VkPhysicalDevice, VkSurfaceKHR, int)>> QUEUE_REQUIREMENT_TYPE;

const QUEUE_REQUIREMENT_TYPE default_queue_requirements = { 
    {
        "graphics_queue", 
        [](VkQueueFamilyProperties properties, VkPhysicalDevice physical_device, VkSurfaceKHR surface, int index) {
            return properties.queueFlags & VK_QUEUE_GRAPHICS_BIT;
        }
    }, 
    {
        "presentation_queue", 
        [](VkQueueFamilyProperties properties, VkPhysicalDevice physical_device, VkSurfaceKHR surface, int index) {
            VkBool32 can_present = false; 
            vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, index, surface, &can_present); 
            return can_present;
        }
    }
};

enum OS {
    MacOS,
    Linux,
    Windows
};

struct VkQueueWrapper {
    VkQueue queue;
    int queue_index;

    VkQueueWrapper() : queue(VK_NULL_HANDLE), queue_index(-1) {

    }

    VkQueueWrapper(VkQueue queue, int queue_index) : queue(queue), queue_index(queue_index) {
        
    }
};

#ifdef __APPLE__
OS current_os = MacOS;
#elif _WIN32
OS current_os = Windows;
#else
OS current_os = Linux;
#endif 

// Load Vulkan and SDL

void load_vulkan() {
    if(VkResult loaded = volkInitialize(); loaded != VK_SUCCESS) {
        throw std::runtime_error("Vulkan Initialization through VOLK failed: " + std::string(string_VkResult(loaded)));
    }
    std::cout << "Vulkan Initialized through VOLK" << std::endl;
}

void sdl_init() {
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        SDL_Quit();
        throw std::runtime_error("SDL2 Initialization failed: " + std::string(SDL_GetError()));
    }
    std::cout << "SDL2 Video & Events Initialized" << std::endl;
}

SDL_Window* get_sdl_window() {
    SDL_Window* window = SDL_CreateWindow("RPG", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1000, 1000, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

    if (!window) {
        throw std::runtime_error("Failed to create window: " + std::string(SDL_GetError()));
    }

    return window;
}

// Create Vulkan Types

VkInstance get_vk_instance(SDL_Window* window) {
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "RPG";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    unsigned int count = 0;
    SDL_Vulkan_GetInstanceExtensions(window, &count, NULL);
    std::vector<const char*> instance_extensions = std::vector<const char*>(count);
    SDL_Vulkan_GetInstanceExtensions(window, &count, instance_extensions.data());

    if (current_os == MacOS) {
        createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        instance_extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        instance_extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    }

    std::cout << "Loading required instance extensions..." << std::endl;
    for (unsigned int i = 0; i < instance_extensions.size(); ++i) {
        std::cout << " - " << instance_extensions[i] << std::endl;
    }

    createInfo.enabledExtensionCount = instance_extensions.size();
    createInfo.ppEnabledExtensionNames = instance_extensions.data();
    createInfo.enabledLayerCount = 0;

    #ifndef NDEBUG
    std::vector<const char*> validation_layers = {"VK_LAYER_KHRONOS_validation"};
    createInfo.enabledLayerCount = validation_layers.size();
    createInfo.ppEnabledLayerNames = validation_layers.data();
    #endif

    VkInstance instance;

    if (VkResult result = vkCreateInstance(&createInfo, nullptr, &instance); result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create vulkan instance: " + std::string(string_VkResult(result)));
    }

    volkLoadInstance(instance);

    return instance;
}

VkSurfaceKHR get_vk_surface(SDL_Window* window, VkInstance instance) {
    VkSurfaceKHR surface;
    SDL_Vulkan_CreateSurface(window, instance, &surface);
    return surface;
}

bool find_required_queue_indices(VkPhysicalDevice physical_device, VkSurfaceKHR surface, 
                                QUEUE_REQUIREMENT_TYPE requirements, std::unordered_map<int, std::vector<std::string>>* indices_map) {
    unsigned int count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, NULL);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, families.data());

    std::vector<std::string> assigned_queues = {};

    // Find suitable queue families
    for (uint32_t i = 0; i < count; ++i) {
        for (auto [queue_name, queue_requirements] : requirements) {
            if (std::find(assigned_queues.begin(), assigned_queues.end(), queue_name) == assigned_queues.end() 
                    && queue_requirements(families[i], physical_device, surface, i)) {
                if(indices_map != nullptr) {
                    (*indices_map)[i].push_back(queue_name);
                }
                assigned_queues.push_back(queue_name);
            }
        }
        
        if (assigned_queues.size() == requirements.size()) {
            return true;
        }
    }
    return false;
}

VkPhysicalDevice choose_physical_device(std::vector<VkPhysicalDevice> devices, VkSurfaceKHR surface, QUEUE_REQUIREMENT_TYPE requirements) {
    std::vector<std::tuple<VkPhysicalDevice, int>> device_scores = {};

    for (VkPhysicalDevice device : devices) {
        int device_score = 0;
        VkPhysicalDeviceProperties deviceProperties;
        VkPhysicalDeviceFeatures deviceFeatures;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);
        vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

        // Get score based on features.

        // Check for required queues in device.
        if (!find_required_queue_indices(device, surface, requirements, nullptr)) {
            continue;
        }

        // Get surface capabilities.
        VkSurfaceCapabilitiesKHR caps;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &caps);

        // Get surface formats.
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

        if (formatCount == 0) {
            continue;
        }

        std::vector<VkSurfaceFormatKHR> formats (formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, formats.data());

        // Get surface present modes.
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

        if (presentModeCount == 0) {
            continue;
        }

        std::vector<VkPresentModeKHR> presentModes (presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, presentModes.data());

        // Minimum recs met. Choose best device based on surface details.

        for (const auto& availableFormat : formats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                device_score += 100;
            }
        }

        for (const auto& availablePresentMode : presentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                device_score += 100;
            }
        }

        device_scores.push_back(std::tie(device, device_score));
    }

    if (device_scores.size() == 0) {
        throw std::runtime_error("Couldn't find any physical devices with the minimum feature / queue requirements.");
    }    

    return std::get<0>(*std::max_element(device_scores.begin(), device_scores.end(), 
                            [](const std::tuple<VkPhysicalDevice, int>& lhs, const std::tuple<VkPhysicalDevice, int>& rhs) {
                                return std::get<1>(lhs) < std::get<1>(rhs);
                            }));
}

void get_vk_devices_and_queues(VkInstance instance, VkSurfaceKHR surface, VkPhysicalDevice& physical_device, VkDevice& logical_device, 
                                std::unordered_map<std::string, VkQueueWrapper>& queue_map, QUEUE_REQUIREMENT_TYPE queue_requirement_map = default_queue_requirements) {
    // Get a list of all physical devices.
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        throw std::runtime_error("No GPU with Vulkan support found.");
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    // Choose the most suitable physical device.
    physical_device = choose_physical_device(devices, surface, queue_requirement_map);

    // Get the queue indices for each of the required queues.
    std::unordered_map<int, std::vector<std::string>> queue_index_map = {};
    find_required_queue_indices(physical_device, surface, queue_requirement_map, &queue_index_map);
    
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos = std::vector<VkDeviceQueueCreateInfo>();
    float priority = 1.0f;

    // Create VkDeviceQueueCreateInfo structs for each unique queue that needs to be created by the logical device.
    for(auto [index, queue_names] : queue_index_map) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = index;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &priority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};

    // Determine the necessary device extensions.
    std::vector<const char*> device_extensions = {"VK_KHR_swapchain"};
    if (current_os == MacOS) {
        device_extensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
    }

    std::cout << "Loading required device extensions..." << std::endl;
    for (unsigned int i = 0; i < device_extensions.size(); ++i) {
        std::cout << " - " << device_extensions[i] << std::endl;
    }

    // Fill out the logical device creation struct.
    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.queueCreateInfoCount = queueCreateInfos.size();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledLayerCount = 0;
    createInfo.enabledExtensionCount = device_extensions.size();
    createInfo.ppEnabledExtensionNames = device_extensions.data();
    
    // Attempt to create the logical device.
    if (VkResult result = vkCreateDevice(physical_device, &createInfo, nullptr, &logical_device); result != VK_SUCCESS) {
        throw std::runtime_error("Could not create logical device: " + std::string(string_VkResult(result)));
    }

    // Get all the created queues from the logical device and relate them to the required queue names provided.
    for(auto [index, queue_names] : queue_index_map) {
        VkQueue queue;
        vkGetDeviceQueue(logical_device, index, 0, &queue);
        for (std::string queue_name : queue_names) {
            queue_map[queue_name] = VkQueueWrapper(queue, index);
        }
    }
}

VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }

    return availableFormats[0];
}

VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, SDL_Window* window) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    } else {
        int width, height;
        SDL_Vulkan_GetDrawableSize(window, &width, &height);

        VkExtent2D actualExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

void get_vk_swapchain_and_images(SDL_Window* window, VkSurfaceKHR surface, VkPhysicalDevice physical_device, VkDevice device, 
                                VkQueueWrapper graphics_queue, VkQueueWrapper presentation_queue, VkSwapchainKHR& swapchain, std::vector<VkImage>& images, std::vector<VkImageView>& imageViews, 
                                VkFormat& format, VkExtent2D& extent, int preferred_additional_image_count = 1) {
    // Get surface capabilities.
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &caps);

    // Get surface formats.
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &formatCount, nullptr);

    std::vector<VkSurfaceFormatKHR> formats (formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &formatCount, formats.data());

    // Get surface present modes.
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &presentModeCount, nullptr);

    std::vector<VkPresentModeKHR> presentModes (presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &presentModeCount, presentModes.data());

    // Don't go over the maximum image count for the implementation.
    uint32_t imageCount = caps.minImageCount + preferred_additional_image_count;
    while (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
        --imageCount;
    }
    
    extent = chooseSwapExtent(caps, window);
    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(formats);
    format = surfaceFormat.format;
    VkPresentModeKHR presentMode = chooseSwapPresentMode(presentModes);

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;

    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    
    uint32_t queueFamilyIndices[] = {static_cast<uint32_t>(graphics_queue.queue_index), static_cast<uint32_t>(presentation_queue.queue_index)};

    if (graphics_queue.queue_index != presentation_queue.queue_index) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }

    createInfo.preTransform = caps.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (VkResult result = vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain); result != VK_SUCCESS) {
        throw std::runtime_error("Couldn't create the swapchain: " + std::string(string_VkResult(result)));
    }

    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
    images.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, images.data());

    imageViews.resize(imageCount);
    VkImageViewCreateInfo imageViewCreateInfo{};
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.format = surfaceFormat.format;
    imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    imageViewCreateInfo.subresourceRange.levelCount = 1;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount = 1;

    for (int i = 0; i < images.size(); ++i) {
        imageViewCreateInfo.image = images[i];

        if (VkResult result = vkCreateImageView(device, &imageViewCreateInfo, nullptr, &imageViews[i]); result != VK_SUCCESS) {
            throw std::runtime_error("Could not create an image view: " + std::string(string_VkResult(result)));
        }
    }
}

std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }

    size_t fileSize = (size_t) file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();

    return buffer;
}

VkShaderModule createShaderModule(const std::vector<char>& code, VkDevice device) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (VkResult result = vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule); result != VK_SUCCESS) {
        throw std::runtime_error("Could not create shader module: " + std::string(string_VkResult(result)));
    }

    return shaderModule;
}

VkPipelineLayout create_vk_pipeline_layout(VkDevice device) {
    VkPipelineLayout pipeline_layout;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0; // Optional
    pipelineLayoutInfo.pSetLayouts = nullptr; // Optional
    pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
    pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

    if (VkResult result = vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipeline_layout); result != VK_SUCCESS) {
        throw std::runtime_error("Could not create the pipeline layout!" + std::string(string_VkResult(result)));
    }

    return pipeline_layout;
}

VkRenderPass create_vk_render_pass(VkDevice device, VkFormat swapchain_image_format) {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchain_image_format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency external_dependency {};
    external_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    external_dependency.dstSubpass = 0;
    external_dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    external_dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    external_dependency.srcAccessMask = 0;
    external_dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPass renderPass;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &external_dependency;

    if (VkResult result = vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass); result != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!" + std::string(string_VkResult(result)));
    }

    return renderPass;
}

// Attribute Input Description and Binding Input Description helper functions

template <class ...VertexTypes>
std::vector<VkVertexInputAttributeDescription> get_all_attribute_descriptions(typename std::enable_if<sizeof...(VertexTypes) == 0>::type* _ = nullptr) {
    return {};
}

template <class VertexType, class ...VertexTypes> std::vector<VkVertexInputAttributeDescription> get_all_attribute_descriptions() {
    std::vector<VkVertexInputAttributeDescription> attribute_descriptions = VertexType::get_attribute_description();
    for (VkVertexInputAttributeDescription attribute_description : get_all_attribute_descriptions<VertexTypes...>()) {
        attribute_descriptions.push_back(attribute_description);
    }
    return attribute_descriptions;
}

template <class ... VertexTypes>
std::vector<VkVertexInputBindingDescription> get_all_binding_descriptions(typename std::enable_if<sizeof...(VertexTypes) == 0>::type* _ = nullptr) {
    return {};
}

template <class VertexType, class ...VertexTypes> std::vector<VkVertexInputBindingDescription> get_all_binding_descriptions() {
    std::vector<VkVertexInputBindingDescription> binding_descriptions = {VertexType::get_binding_description()};
    for (VkVertexInputBindingDescription binding_description : get_all_binding_descriptions<VertexTypes... >()) {
        binding_descriptions.push_back(binding_description);
    }
    return binding_descriptions;
}

// Usage note: pass all vertex types that need attribute / binding descriptors as template arguments to the function.
// Make sure to implement VetexType::get_attribute_description and VertexType::get_binding_description first.
template <class ...VertexTypes>
VkPipeline create_vk_graphics_pipeline(VkDevice device, VkPipelineLayout pipeline_layout, VkRenderPass render_pass, VkShaderModule vertex_shader_module, VkShaderModule fragment_shader_module, VkExtent2D extent) {
    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertex_shader_module;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragment_shader_module;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
    vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float) extent.width;
    viewport.height = (float) extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = extent;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f; // Optional
    rasterizer.depthBiasClamp = 0.0f; // Optional
    rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f; // Optional
    multisampling.pSampleMask = nullptr; // Optional
    multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
    multisampling.alphaToOneEnable = VK_FALSE; // Optional

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f; // Optional
    colorBlending.blendConstants[1] = 0.0f; // Optional
    colorBlending.blendConstants[2] = 0.0f; // Optional
    colorBlending.blendConstants[3] = 0.0f; // Optional

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = nullptr; // Optional
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;

    pipelineInfo.layout = pipeline_layout;

    pipelineInfo.renderPass = render_pass;
    pipelineInfo.subpass = 0;

    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
    pipelineInfo.basePipelineIndex = -1; // Optional

    auto binding_descriptions = get_all_binding_descriptions<VertexTypes...>();
    auto attribute_descriptions = get_all_attribute_descriptions<VertexTypes...>();

    // Create the pipeline vertex input state.
    VkPipelineVertexInputStateCreateInfo pipeline_vertex_input_state_info {};
    pipeline_vertex_input_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    pipeline_vertex_input_state_info.vertexAttributeDescriptionCount = attribute_descriptions.size();
    pipeline_vertex_input_state_info.vertexBindingDescriptionCount = binding_descriptions.size();

    pipeline_vertex_input_state_info.pVertexAttributeDescriptions = attribute_descriptions.data();
    pipeline_vertex_input_state_info.pVertexBindingDescriptions = binding_descriptions.data();

    pipelineInfo.pVertexInputState = &pipeline_vertex_input_state_info;


    VkPipeline graphicsPipeline;

    if (VkResult result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline); result != VK_SUCCESS) {
        throw std::runtime_error("Could not create graphics pipeline: " + std::string(string_VkResult(result)));
    }

    return graphicsPipeline;
}

std::vector<VkFramebuffer> get_vk_swapchain_framebuffers(VkDevice device, std::vector<VkImageView> swapchain_image_views, VkRenderPass render_pass, VkExtent2D extent) {
    std::vector<VkFramebuffer> swapchain_framebuffers (swapchain_image_views.size());

    for (int i = 0; i < swapchain_image_views.size(); ++i) {
        VkImageView attachments[] = {
            swapchain_image_views[i]
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = render_pass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = extent.width;
        framebufferInfo.height = extent.height;
        framebufferInfo.layers = 1;

        if (VkResult result = vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapchain_framebuffers[i]); result != VK_SUCCESS) {
            throw std::runtime_error("Could not create framebuffer: " + std::string(string_VkResult(result)));
        }
    }

    return swapchain_framebuffers;
}

VkCommandPool get_vk_command_pool(VkDevice device, int queue_index, int command_pool_flags) {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = command_pool_flags;
    poolInfo.queueFamilyIndex = queue_index;

    VkCommandPool command_pool;

    if (VkResult result = vkCreateCommandPool(device, &poolInfo, nullptr, &command_pool); result != VK_SUCCESS) {
        throw std::runtime_error("Could not create command pool: " + std::string(string_VkResult(result)));
    }

    return command_pool;
}

std::vector<VkCommandBuffer> get_vk_command_buffers(VkDevice device, VkCommandPool command_pool, int count) {
    std::vector<VkCommandBuffer> command_buffers (count);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = command_pool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = count;

    if (VkResult result = vkAllocateCommandBuffers(device, &allocInfo, command_buffers.data()); result != VK_SUCCESS) {
        throw std::runtime_error("Could not create command buffer: " + std::string(string_VkResult(result)));
    }

    return command_buffers;
}

std::tuple<VkBuffer, VkDeviceMemory> get_vk_buffer(VkPhysicalDevice physical_device, VkDevice logical_device, VkBufferUsageFlags buffer_usage_flags, VkSharingMode buffer_sharing_mode, 
                                                        std::size_t buffer_size, uint32_t buffer_queue_index, int required_memory_properties) {
    VkBuffer buffer;
    VkDeviceMemory buffer_memory;

    VkBufferCreateInfo buffer_create_info {};
    buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.usage = buffer_usage_flags;
    buffer_create_info.queueFamilyIndexCount = 1;
    buffer_create_info.pQueueFamilyIndices = &buffer_queue_index;
    buffer_create_info.size = buffer_size;
    buffer_create_info.sharingMode = buffer_sharing_mode;
    
    if (VkResult result = vkCreateBuffer(logical_device, &buffer_create_info, nullptr, &buffer); result != VK_SUCCESS) {
        throw std::runtime_error("Could not create vertex buffer: " + std::string(string_VkResult(result)));
    }

    // Find the best type of available memory for VkBuffer.
    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(logical_device, buffer, &memory_requirements);

    uint32_t type_filter = memory_requirements.memoryTypeBits;

    int chosen_memory_type = -1;

    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);

    for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) && (memory_properties.memoryTypes[i].propertyFlags & required_memory_properties) == required_memory_properties) {
            chosen_memory_type = i;
            break;
        }
    }

    if (chosen_memory_type == -1) {
        throw std::runtime_error("Could not find memory type that matched requirements.");
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memory_requirements.size;
    allocInfo.memoryTypeIndex = chosen_memory_type;

    if (VkResult result = vkAllocateMemory(logical_device, &allocInfo, nullptr, &buffer_memory); result != VK_SUCCESS) {
        throw std::runtime_error("Could not allocate vertex buffer memory: " + std::string(string_VkResult(result)));
    }

    vkBindBufferMemory(logical_device, buffer, buffer_memory, 0);
    return std::tie(buffer, buffer_memory);
}

void vk_cpy_host_to_gpu(VkDevice logical_device, const void* src, VkDeviceMemory memory, VkDeviceSize map_size, VkDeviceSize map_offset = 0, VkMemoryMapFlags map_flags = 0) {
    void* host_memory_pointer;
    vkMapMemory(logical_device, memory, map_offset, map_size, map_flags, &host_memory_pointer);
    memcpy(host_memory_pointer, src, (size_t) map_size);
    vkUnmapMemory(logical_device, memory);
}

void vk_cpy_buffer(VkQueue queue, VkCommandPool command_pool, VkDevice logical_device, VkBuffer src, VkBuffer dst, VkDeviceSize size) {
    // Get a command buffer
    VkCommandBuffer command_buffer = get_vk_command_buffers(logical_device, command_pool, 1).front();

    // Begin the command buffer
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(command_buffer, &beginInfo);

    // Execute the copy command
    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0; // Optional
    copyRegion.dstOffset = 0; // Optional
    copyRegion.size = size;

    vkCmdCopyBuffer(command_buffer, src, dst, 1, &copyRegion);

    // End the command buffer

    vkEndCommandBuffer(command_buffer);

    // Submit the command buffer to its queue

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &command_buffer;

    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    // Once the copy is done, free the command buffer

    vkFreeCommandBuffers(logical_device, command_pool, 1, &command_buffer);
}

template<class Vertex>
std::tuple<VkBuffer, VkDeviceMemory> get_vk_vertex_buffer(VkPhysicalDevice physical_device, VkDevice logical_device, VkQueueWrapper transfer_queue, VkCommandPool transfer_command_pool, std::vector<Vertex> vertices) {
    auto [vertex_staging_buffer, vertex_staging_buffer_memory] = get_vk_buffer(physical_device, logical_device, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
                                                                    VK_SHARING_MODE_EXCLUSIVE, sizeof(Vertex) * vertices.size(), transfer_queue.queue_index, 
                                                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vk_cpy_host_to_gpu(logical_device, vertices.data(), vertex_staging_buffer_memory, sizeof(Vertex) * vertices.size());

    auto [vertex_buffer, vertex_buffer_memory] = get_vk_buffer(physical_device, logical_device, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 
                                                                    VK_SHARING_MODE_EXCLUSIVE, sizeof(Vertex) * vertices.size(), transfer_queue.queue_index, 
                                                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vk_cpy_buffer(transfer_queue.queue, transfer_command_pool, logical_device, vertex_staging_buffer, vertex_buffer, sizeof(Vertex) * vertices.size());

    vkDestroyBuffer(logical_device, vertex_staging_buffer, nullptr);
    vkFreeMemory(logical_device, vertex_staging_buffer_memory, nullptr);

    return std::tie(vertex_buffer, vertex_buffer_memory);
}

// GPU Data Input Types

struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;

    Vertex() : pos(glm::vec2(0, 0)), color(glm::vec3(0, 0, 0)) {

    }

    Vertex(glm::vec2 _pos) : pos(_pos), color(glm::vec3(0, 0, 0)) {

    }

    Vertex(glm::vec3 _color) : pos(glm::vec2(0, 0)), color(_color) {

    }

    Vertex(glm::vec2 _pos, glm::vec3 _color) : pos(_pos), color(_color) {

    }

    Vertex(float x, float y) : pos(glm::vec2(x, y)), color(glm::vec3(0, 0, 0)) {

    }

    Vertex(float r, float g, float b) : pos(glm::vec2(0, 0)), color(glm::vec3(r, g, b)) {

    }

    Vertex(float x, float y, float r, float g, float b) : pos(glm::vec2(x, y)), color(glm::vec3(r, g, b)) {

    }

    Vertex(float x, float y, glm::vec3 _color) : pos(glm::vec2(x, y)), color(_color) {

    }

    Vertex(glm::vec2 _pos, float r, float g, float b) : pos(_pos), color(glm::vec3(r, g, b)) {

    }

    static std::vector<VkVertexInputAttributeDescription> get_attribute_description() {
        // Create vertex attribute and binding descriptions.
        VkVertexInputAttributeDescription desc0 {};
        desc0.binding = 0;
        desc0.location = 0;
        desc0.offset = offsetof(Vertex, pos);
        desc0.format = VK_FORMAT_R32G32_SFLOAT;

        VkVertexInputAttributeDescription desc1 {};
        desc1.binding = 0;
        desc1.location = 1;
        desc1.offset = offsetof(Vertex, color);
        desc1.format = VK_FORMAT_R32G32B32_SFLOAT;
        return {desc0, desc1};
    }

    static VkVertexInputBindingDescription get_binding_description() {
        VkVertexInputBindingDescription desc {};
        desc.binding = 0;
        desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        desc.stride = sizeof(Vertex);
        return desc;
    }
};

struct ObjectData {
    glm::vec2 pos;

    ObjectData() : pos(glm::vec2(0, 0)) {

    }

    ObjectData(glm::vec2 _pos) : pos(_pos) {

    }

    ObjectData(float x, float y) : pos(glm::vec2(x, y)) {

    }

    static std::vector<VkVertexInputAttributeDescription> get_attribute_description() {
        // Create vertex attribute and binding descriptions.
        VkVertexInputAttributeDescription desc0 {};
        desc0.binding = 1;
        desc0.location = 2;
        desc0.offset = offsetof(ObjectData, pos);
        desc0.format = VK_FORMAT_R32G32_SFLOAT;

        return {desc0};
    }

    static VkVertexInputBindingDescription get_binding_description() {
        VkVertexInputBindingDescription desc {};
        desc.binding = 1;
        desc.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
        desc.stride = sizeof(ObjectData);
        return desc;
    }
};

// Context Storage Structures

template<class Vertex>
struct VertexBufferBacked {
    VkBuffer buffer;
    VkDeviceMemory memory;
    int length;

    VertexBufferBacked<Vertex>(VkPhysicalDevice physical_device, VkDevice logical_device, VkQueueWrapper transfer_queue, VkCommandPool transfer_command_pool, std::vector<Vertex> data) {
        auto [buf, mem] = get_vk_vertex_buffer<Vertex>(physical_device, logical_device, transfer_queue, transfer_command_pool, data);
        buffer = buf;
        memory = mem;
        length = data.size();
    }

    void destroy(VkDevice device) {
        vkDestroyBuffer(device, buffer, nullptr);
        vkFreeMemory(device, memory, nullptr);
    }
};

struct GraphicsPipeline {
    VkShaderModule vertex_shader_module;
    VkShaderModule fragment_shader_module;
    VkPipeline graphics_pipeline;
    VkRenderPass render_pass;
    VkPipelineLayout pipeline_layout;

   void vk_destroy(VkDevice device) {
        if (graphics_pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, graphics_pipeline, nullptr);
        }

        if (pipeline_layout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
        }

        if (render_pass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(device, render_pass, nullptr);
        }

        if (vertex_shader_module != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device, vertex_shader_module, nullptr);
        }
        
        if (fragment_shader_module != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device, fragment_shader_module, nullptr);
        }
    }

    GraphicsPipeline(const GraphicsPipeline&) = delete;

    GraphicsPipeline() : vertex_shader_module(VK_NULL_HANDLE), fragment_shader_module(VK_NULL_HANDLE), graphics_pipeline(VK_NULL_HANDLE), render_pass(VK_NULL_HANDLE), 
                            pipeline_layout(VK_NULL_HANDLE) {
    }

    GraphicsPipeline(VkDevice device, std::string vertex_shader_loc, std::string fragment_shader_loc, VkExtent2D extent, VkFormat swapchain_format) : 
        vertex_shader_module(createShaderModule(readFile(vertex_shader_loc), device)), fragment_shader_module(createShaderModule(readFile(fragment_shader_loc), device)) {
        render_pass = create_vk_render_pass(device, swapchain_format);
        pipeline_layout = create_vk_pipeline_layout(device);
        graphics_pipeline = create_vk_graphics_pipeline<Vertex, ObjectData>(device, pipeline_layout, render_pass, vertex_shader_module, fragment_shader_module, extent);
    } 
};

void create_synchronization_objects(VkDevice logical_device, std::vector<VkFence>* command_buffer_fences, std::vector<VkSemaphore>* image_available_semaphores, std::vector<VkSemaphore>* image_done_rendering_semaphores, int MAX_FRAMES_IN_FLIGHT) {
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkFence command_buffer_fence;
        VkFenceCreateInfo fence_creation_info {};
        fence_creation_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_creation_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        vkCreateFence(logical_device, &fence_creation_info, nullptr, &command_buffer_fence);

        VkSemaphore image_available_semaphore;
        VkSemaphore image_done_rendering_semaphore;
        VkSemaphoreCreateInfo semaphore_creation_info {};
        semaphore_creation_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        vkCreateSemaphore(logical_device, &semaphore_creation_info, nullptr, &image_available_semaphore);
        vkCreateSemaphore(logical_device, &semaphore_creation_info, nullptr, &image_done_rendering_semaphore);

        command_buffer_fences->push_back(command_buffer_fence);
        image_available_semaphores->push_back(image_available_semaphore);
        image_done_rendering_semaphores->push_back(image_done_rendering_semaphore);
    }
}

struct VkContext {
    SDL_Window* window;
    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice physical_device;
    VkDevice logical_device;
    std::unordered_map<std::string, VkQueueWrapper> queue_map;
    VkSwapchainKHR swapchain;
    std::vector<VkImage> images;
    std::vector<VkImageView> image_views;
    std::vector<VkFramebuffer> swapchain_framebuffers;
    VkFormat swapchain_format;
    VkExtent2D swapchain_extent;
    GraphicsPipeline graphics_pipeline;
    
    const int MAX_FRAMES_IN_FLIGHT = 2;

    VkCommandPool command_pool;
    VkCommandPool transient_command_pool;

    std::vector<VkCommandBuffer> command_buffers;
    std::vector<VkFence> command_buffer_fences;
    std::vector<VkSemaphore> image_available_semaphores;
    std::vector<VkSemaphore> image_done_rendering_semaphores;

    std::vector<VertexBufferBacked<Vertex>> vertex_buffers;
    std::vector<VertexBufferBacked<ObjectData>> object_position_buffers;

    VkContext(const VkContext&) = delete;

    VkContext() {
        // Load Vulkan and SDL
        load_vulkan();
        sdl_init();

        // Create a window.
        window = get_sdl_window();

        // Create an instance of vulkan.
        instance = get_vk_instance(window);

        // Create the window surface.
        surface = get_vk_surface(window, instance);

        // Create a physical device, a logical device and get a graphics queue and presentation queue from it.
        get_vk_devices_and_queues(instance, surface, physical_device, logical_device, queue_map);

        // Create the swapchain.
        get_vk_swapchain_and_images(window, surface, physical_device, logical_device, queue_map["graphics_queue"], queue_map["presentation_queue"], swapchain, images, image_views, swapchain_format, swapchain_extent);
    
        // Create the graphics pipeline.
        graphics_pipeline = GraphicsPipeline(logical_device, "shaders/bin/shader_2d_vert.spv", "shaders/bin/shader_2d_frag.spv", swapchain_extent, swapchain_format);

        // Create swapchain framebuffers.
        swapchain_framebuffers = get_vk_swapchain_framebuffers(logical_device, image_views, graphics_pipeline.render_pass, swapchain_extent);

        // Create command pool.
        command_pool = get_vk_command_pool(logical_device, get_graphics_queue_index(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
        transient_command_pool = get_vk_command_pool(logical_device, get_graphics_queue_index(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

        // Create command buffer.
        command_buffers = get_vk_command_buffers(logical_device, command_pool, MAX_FRAMES_IN_FLIGHT);

        // Create synchronization objects.
        create_synchronization_objects(logical_device, &command_buffer_fences, &image_available_semaphores, &image_done_rendering_semaphores, MAX_FRAMES_IN_FLIGHT);
    }

    int create_vertex_buffer(std::vector<Vertex> vertex_data) {
        vertex_buffers.push_back(VertexBufferBacked<Vertex>(physical_device, logical_device, queue_map["graphics_queue"], transient_command_pool, vertex_data));
        return vertex_buffers.size() - 1;
    }

    int create_object_position_buffer(std::vector<ObjectData> object_position_data) {
        object_position_buffers.push_back(VertexBufferBacked<ObjectData>(physical_device, logical_device, queue_map["graphics_queue"], transient_command_pool, object_position_data));
        return object_position_buffers.size() - 1;
    }

    void rebuild_swapchain() {
        vkDeviceWaitIdle(logical_device);
        vk_destroy_swapchain();
        get_vk_swapchain_and_images(window, surface, physical_device, logical_device, queue_map["graphics_queue"], queue_map["presentation_queue"], swapchain, images, image_views, swapchain_format, swapchain_extent);
        swapchain_framebuffers = get_vk_swapchain_framebuffers(logical_device, image_views, graphics_pipeline.render_pass, swapchain_extent);
    }

    void vk_destroy_swapchain() {  
        for (VkFramebuffer framebuffer : swapchain_framebuffers) {
            vkDestroyFramebuffer(logical_device, framebuffer, nullptr);
        }
        for (VkImageView image_view : image_views) {
            vkDestroyImageView(logical_device, image_view, nullptr);
        }
        vkDestroySwapchainKHR(logical_device, swapchain, nullptr);
    }

    void vk_destroy() {
        vkDeviceWaitIdle(logical_device);
        for (VertexBufferBacked vertex_buffer : vertex_buffers) {
            vertex_buffer.destroy(logical_device);
        }
        for (VertexBufferBacked object_position_buffer : object_position_buffers) {
            object_position_buffer.destroy(logical_device);
        }
        vkDestroyCommandPool(logical_device, command_pool, nullptr);
        vkDestroyCommandPool(logical_device, transient_command_pool, nullptr);
        graphics_pipeline.vk_destroy(logical_device);
        vk_destroy_swapchain();
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            vkDestroyFence(logical_device, command_buffer_fences[i], nullptr);
            vkDestroySemaphore(logical_device, image_available_semaphores[i], nullptr);
            vkDestroySemaphore(logical_device, image_done_rendering_semaphores[i], nullptr);
        }
        vkDestroyDevice(logical_device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);
        SDL_Quit();
    }

    ~VkContext() {
        vk_destroy();
    }

    VkQueue get_graphics_queue() {
        return queue_map["graphics_queue"].queue;
    }

    int get_graphics_queue_index() {
        return queue_map["graphics_queue"].queue_index;
    }

    VkQueue get_presentation_queue() {
        return queue_map["presentation_queue"].queue;
    }

    int get_presentation_queue_index() {
        return queue_map["presentation_queue"].queue_index;
    }
};