#define GLFW_INCLUDE_VULKAN
#include <glfw/include/GLFW/glfw3.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#include <freetype/include/ft2build.h>
#include FT_FREETYPE_H
#include <omp.h>
#include <iostream>
#include <vector>
#include <stdexcept>
#include <cstdlib>
#include <set>
#include <map>
#include <cstdint>
#include <string>
#include <limits>
#include <algorithm>
#include <array>
#include <cstring>
#include <optional>
#include <cmath>
#include <fstream>
#include <variant>
#include <queue>
#include <Renderer.h>
#include <UIManager.h>
#include <ShaderManager.h>
#include <FontManager.h>
#include <SceneManager.h>
#include <TextureManager.h>
#include <EntityManager.h>
#include <ModelManager.h>
#include <Model.h>
#include <UIObject.h>
#include <TextObject.h>
#include <InputManager.h>
#include <ButtonObject.h>
#include <Camera.h>
#include <Frustrum.h>
#include <utils.h>

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

#define PI 3.14159265358979323846
const int MAX_FRAMES_IN_FLIGHT = static_cast<int>(Renderer::kMaxFramesInFlight);
uint32_t currentFrame = 0;

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};
std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
#ifdef __APPLE__
    , "VK_KHR_portability_subset"
#endif
};
#ifndef VK_EXT_SHADER_ATOMIC_FLOAT_EXTENSION_NAME
#define VK_EXT_SHADER_ATOMIC_FLOAT_EXTENSION_NAME "VK_EXT_shader_atomic_float"
#endif

// Runtime toggle: use CAS fallback when float atomicAdd isn’t available via VK_EXT_shader_atomic_float
static bool g_useCASAdvection = true;
#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger){
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if(func != nullptr) return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    else return VK_ERROR_EXTENSION_NOT_PRESENT;
}
void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator){
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if(func != nullptr) func(instance, debugMessenger, pAllocator);
}
struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 texCoord;
    glm::vec3 tangent;
    static VkVertexInputBindingDescription getBindingDescription(){
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }
    static std::array<VkVertexInputAttributeDescription, 4> getAttributeDescriptions(){
        std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions{};
        attributeDescriptions[0] = {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(Vertex, pos),
        };
        attributeDescriptions[1] = {
            .location = 1,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(Vertex, normal),
        };
        attributeDescriptions[2] = {
            .location = 2,
            .binding = 0,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = offsetof(Vertex, texCoord),
        };
        attributeDescriptions[3] = {
            .location = 3,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(Vertex, tangent),
        };
        return attributeDescriptions;
    }
};
struct TextVertex{
    glm::vec2 pos;
    glm::vec2 texCoord;
    static VkVertexInputBindingDescription getBindingDescription(){
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(TextVertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }
    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions(){
        std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};
        attributeDescriptions[0] = {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = offsetof(TextVertex, pos),
        };
        attributeDescriptions[1] = {
            .location = 1,
            .binding = 0,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = offsetof(TextVertex, texCoord),
        };
        return attributeDescriptions;
    }
};


    Renderer::Renderer() {}
    Renderer::~Renderer() { cleanup(); }
    Renderer* Renderer::getInstance() {
        static Renderer instance;
        return &instance;
    }
    void Renderer::run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }
    ShaderManager* Renderer::getShaderManager() const { return shaderManager; }
    VkCommandBuffer Renderer::beginSingleTimeCommands() {
        VkCommandBufferAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        VkCommandBuffer commandBuffer;
        if(vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffer!");
        }
        VkCommandBufferBeginInfo beginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };
        if(vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin command buffer!");
        }
        return commandBuffer;
    }
    void Renderer::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
        if(vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }
        VkSubmitInfo submitInfo = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffer,
        };
        if(vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit command buffer!");
        }
        vkQueueWaitIdle(graphicsQueue);
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    }
    void Renderer::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();
        VkBufferCopy copyRegion = {
            .srcOffset = 0,
            .dstOffset = 0,
            .size = size,
        };
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
        endSingleTimeCommands(commandBuffer);
    }
    void Renderer::createTextureImage(int width, int height, unsigned char* imageBuffer, VkImage& textureImage, VkDeviceMemory& textureImageMemory, VkFormat format) {
        if(width == 0 || height == 0) {
            width = std::max(1, width);
            height = std::max(1, height);
        }
        VkDeviceSize imageSize = width * height;
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
        void* data;
        vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
        memcpy(data, imageBuffer, static_cast<size_t>(imageSize));
        vkUnmapMemory(device, stagingBufferMemory);
        createImage(width, height, 1, VK_SAMPLE_COUNT_1_BIT, format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory);
        transitionImageLayout(textureImage, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        copyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(width), static_cast<uint32_t>(height));
        transitionImageLayout(textureImage, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);
    }
    VkShaderModule Renderer::createShaderModule(const std::vector<char>& code) {
        VkShaderModuleCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = code.size(),
            .pCode = reinterpret_cast<const uint32_t*>(code.data()),
        };
        VkShaderModule shaderModule;
        if(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("failed to create shader module!");
        }
        return shaderModule;
    }
    void Renderer::createComputePipeline(const std::string& computeShaderPath, VkPipeline& pipeline, VkPipelineLayout& pipelineLayout, VkDescriptorSetLayout& descriptorSetLayout, VkPushConstantRange* pushConstantRange) {
        std::vector<char> compShaderCode = readFile(computeShaderPath);
        VkShaderModule computeShader = createShaderModule(compShaderCode);
        VkPipelineShaderStageCreateInfo compShaderStageInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = computeShader,
            .pName = "main",
        };
        VkPipelineShaderStageCreateInfo shaderStages[] = {compShaderStageInfo};
        const uint32_t shaderStageCount = 1u;
        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1u,
            .pSetLayouts = &descriptorSetLayout,
            .pushConstantRangeCount = pushConstantRange ? 1u : 0u,
            .pPushConstantRanges = pushConstantRange,
        };
        if(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create compute pipeline layout!");
        }
        VkComputePipelineCreateInfo pipelineInfo = {
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = shaderStages[0],
            .layout = pipelineLayout,
        };
        if(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create compute pipeline!");
        }
        vkDestroyShaderModule(device, computeShader, nullptr);
    }
    void Renderer::createGraphicsPipeline(const std::string& vertexShaderPath, const std::string& fragmentShaderPath, VkPipeline& pipeline, VkPipelineLayout& pipelineLayout, VkDescriptorSetLayout& descriptorSetLayout, VkPushConstantRange* pushConstantRange, bool enableDepth, bool useTextVertex, VkCullModeFlags cullMode, VkFrontFace frontFace, bool depthWrite, VkCompareOp depthCompare, VkRenderPass renderPassOverride, uint32_t colorAttachmentCount, VkSampleCountFlagBits sampleCount, bool noVertexInput) {
        std::vector<char> vertShaderCode = readFile(vertexShaderPath);
        std::vector<char> fragShaderCode = readFile(fragmentShaderPath);
        VkShaderModule vertexShader = createShaderModule(vertShaderCode);
        VkShaderModule fragmentShader = createShaderModule(fragShaderCode);
        VkPipelineShaderStageCreateInfo vertShaderStageInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertexShader,
            .pName = "main",
        };
        VkPipelineShaderStageCreateInfo fragShaderStageInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragmentShader,
            .pName = "main",
        };
        VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};
        const uint32_t shaderStageCount = 2u;
        std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };
        VkPipelineDynamicStateCreateInfo dynamicState = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
            .pDynamicStates = dynamicStates.data(),
        };
        VkVertexInputBindingDescription bindingDescription{};
        std::vector<VkVertexInputAttributeDescription> attributeDescriptionsVec;
        
        if (!noVertexInput) {
            bindingDescription = useTextVertex ? TextVertex::getBindingDescription() : Vertex::getBindingDescription();
            if (useTextVertex) {
                auto attrs = TextVertex::getAttributeDescriptions();
                attributeDescriptionsVec.assign(attrs.begin(), attrs.end());
            } else {
                auto attrs = Vertex::getAttributeDescriptions();
                attributeDescriptionsVec.assign(attrs.begin(), attrs.end());
            }
        }
        
        VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = noVertexInput ? 0u : 1u,
            .pVertexBindingDescriptions = noVertexInput ? nullptr : &bindingDescription,
            .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptionsVec.size()),
            .pVertexAttributeDescriptions = attributeDescriptionsVec.empty() ? nullptr : attributeDescriptionsVec.data(),
        };
        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE,
        };
        VkViewport viewport = {
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>(swapChainExtent.width),
            .height = static_cast<float>(swapChainExtent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };
        VkRect2D scissor = {
            .offset = {0, 0},
            .extent = swapChainExtent,
        };
        VkPipelineViewportStateCreateInfo viewportState = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1u,
            .pViewports = &viewport,
            .scissorCount = 1u,
            .pScissors = &scissor,
        };
        VkPipelineRasterizationStateCreateInfo rasterizer = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .depthClampEnable = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = cullMode,
            .frontFace = frontFace,
            .depthBiasEnable = VK_FALSE,
            .depthBiasConstantFactor = 0.0f,
            .depthBiasClamp = 0.0f,
            .depthBiasSlopeFactor = 0.0f,
            .lineWidth = 1.0f,
        };
        VkPipelineMultisampleStateCreateInfo multisampling = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = sampleCount,
            .sampleShadingEnable = (sampleCount > VK_SAMPLE_COUNT_1_BIT) ? VK_TRUE : VK_FALSE,
            .minSampleShading = 0.2f,
            .pSampleMask = nullptr,
            .alphaToCoverageEnable = VK_FALSE,
            .alphaToOneEnable = VK_FALSE,
        };
        if (useTextVertex) {
            rasterizer.cullMode = VK_CULL_MODE_NONE;
            multisampling.sampleShadingEnable = VK_FALSE;
        }
        VkPipelineColorBlendAttachmentState colorBlendAttachment = {
            .blendEnable = VK_TRUE,
            .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .colorBlendOp = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .alphaBlendOp = VK_BLEND_OP_ADD,
            .colorWriteMask = static_cast<VkColorComponentFlags>(
                VK_COLOR_COMPONENT_R_BIT |
                VK_COLOR_COMPONENT_G_BIT |
                VK_COLOR_COMPONENT_B_BIT |
                VK_COLOR_COMPONENT_A_BIT),
        };
        std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments(colorAttachmentCount, colorBlendAttachment);
        VkPipelineColorBlendStateCreateInfo colorBlending = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .logicOpEnable = VK_FALSE,
            .logicOp = VK_LOGIC_OP_COPY,
            .attachmentCount = colorAttachmentCount,
            .pAttachments = colorBlendAttachments.data(),
            .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f},
        };
        VkPipelineDepthStencilStateCreateInfo depthStencil = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable = enableDepth ? VK_TRUE : VK_FALSE,
            .depthWriteEnable = (enableDepth && depthWrite) ? VK_TRUE : VK_FALSE,
            .depthCompareOp = depthCompare,
            .depthBoundsTestEnable = VK_FALSE,
            .stencilTestEnable = VK_FALSE,
            .front = {},
            .back = {},
            .minDepthBounds = 0.0f,
            .maxDepthBounds = 1.0f,
        };
        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1u,
            .pSetLayouts = &descriptorSetLayout,
            .pushConstantRangeCount = pushConstantRange ? 1u : 0u,
            .pPushConstantRanges = pushConstantRange,
        };
        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout!");
        }
        VkGraphicsPipelineCreateInfo pipelineInfo = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = shaderStageCount,
            .pStages = shaderStages,
            .pVertexInputState = &vertexInputInfo,
            .pInputAssemblyState = &inputAssembly,
            .pViewportState = &viewportState,
            .pRasterizationState = &rasterizer,
            .pMultisampleState = &multisampling,
            .pDepthStencilState = &depthStencil,
            .pColorBlendState = &colorBlending,
            .pDynamicState = &dynamicState,
            .layout = pipelineLayout,
            .renderPass = renderPassOverride != VK_NULL_HANDLE ? renderPassOverride : compositeRenderPass,
            .subpass = 0,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = -1,
        };
        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics pipeline!");
        }
        vkDestroyShaderModule(device, fragmentShader, nullptr);
        vkDestroyShaderModule(device, vertexShader, nullptr);
    }
    void Renderer::createDescriptorSetLayout(int vertexBitBindings, int fragmentBitBindings, VkDescriptorSetLayout& descriptorSetLayout, VkShaderStageFlags shaderStage) {
        const int totalVertexBindings = std::max(vertexBitBindings, 0);
        const int totalFragmentBindings = std::max(fragmentBitBindings, 0);
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        bindings.reserve(static_cast<size_t>(totalVertexBindings + totalFragmentBindings));
        
        bool isCompute = (shaderStage & VK_SHADER_STAGE_COMPUTE_BIT) != 0;
        
        for (int bindingIndex = 0; bindingIndex < totalVertexBindings; ++bindingIndex) {
            VkDescriptorSetLayoutBinding vertexLayoutBinding = {
                .binding = static_cast<uint32_t>(bindingIndex),
                .descriptorType = isCompute ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .stageFlags = isCompute ? VK_SHADER_STAGE_COMPUTE_BIT : VK_SHADER_STAGE_VERTEX_BIT,
                .pImmutableSamplers = nullptr,
            };
            bindings.push_back(vertexLayoutBinding);
        }
        for (int offset = 0; offset < totalFragmentBindings; ++offset) {
            VkDescriptorSetLayoutBinding fragmentLayoutBinding = {
                .binding = static_cast<uint32_t>(totalVertexBindings + offset),
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = isCompute ? VK_SHADER_STAGE_COMPUTE_BIT : VK_SHADER_STAGE_FRAGMENT_BIT,
                .pImmutableSamplers = nullptr,
            };
            bindings.push_back(fragmentLayoutBinding);
        }
        VkDescriptorSetLayoutCreateInfo layoutInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = static_cast<uint32_t>(bindings.size()),
            .pBindings = bindings.data(),
        };
        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor set layout!");
        }
    }
    void Renderer::createDescriptorPool(int vertexBitBindings, int fragmentBitBindings, VkDescriptorPool &descriptorPool, int multiplier, bool isCompute) {
        std::vector<VkDescriptorPoolSize> poolSizes;
        if (vertexBitBindings > 0) {
            VkDescriptorPoolSize vertexPoolSize = {
                .type = isCompute ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = static_cast<uint32_t>(vertexBitBindings * MAX_FRAMES_IN_FLIGHT * multiplier),
            };
            poolSizes.push_back(vertexPoolSize);
        }
        if (fragmentBitBindings > 0) {
            VkDescriptorPoolSize fragmentPoolSize = {
                .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = static_cast<uint32_t>(fragmentBitBindings * MAX_FRAMES_IN_FLIGHT * multiplier),
            };
            poolSizes.push_back(fragmentPoolSize);
        }
        VkDescriptorPoolCreateInfo poolInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * multiplier),
            .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
            .pPoolSizes = poolSizes.data(),
        };
        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor pool!");
        }
    }
    std::vector<VkDescriptorSet> Renderer::createDescriptorSets(VkDescriptorPool pool, VkDescriptorSetLayout& descriptorSetLayout, int vertexBindingCount, int fragmentBindingCount, std::vector<Image*>& textures, std::vector<VkBuffer>& uniformBuffers) {
        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
        VkDescriptorSetAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = pool,
            .descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
            .pSetLayouts = layouts.data(),
        };
        std::vector<VkDescriptorSet> descriptorSets(MAX_FRAMES_IN_FLIGHT);
        if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate descriptor sets!");
        }
        const uint32_t vertexBindingCountClamped = vertexBindingCount > 0 ? static_cast<uint32_t>(vertexBindingCount) : 0u;
        const uint32_t fragmentBindingCountClamped = fragmentBindingCount > 0 ? static_cast<uint32_t>(fragmentBindingCount) : 0u;
        const size_t expectedUniformBuffers = static_cast<size_t>(vertexBindingCountClamped) * static_cast<size_t>(MAX_FRAMES_IN_FLIGHT);
        if (vertexBindingCountClamped > 0 && uniformBuffers.size() < expectedUniformBuffers) {
            throw std::runtime_error("insufficient uniform buffers supplied for descriptor allocation");
        }
        const size_t expectedTextures = static_cast<size_t>(fragmentBindingCountClamped);
        if (fragmentBindingCountClamped > 0 && textures.size() < expectedTextures) {
            throw std::runtime_error("insufficient textures supplied for descriptor allocation");
        }

        std::vector<VkDescriptorBufferInfo> bufferInfos;
        bufferInfos.reserve(expectedUniformBuffers);
        std::vector<VkDescriptorImageInfo> imageInfos;
        imageInfos.reserve(static_cast<size_t>(fragmentBindingCountClamped) * static_cast<size_t>(MAX_FRAMES_IN_FLIGHT));
        std::vector<VkWriteDescriptorSet> descriptorWrites;
        descriptorWrites.reserve(static_cast<size_t>(vertexBindingCountClamped + fragmentBindingCountClamped) * static_cast<size_t>(MAX_FRAMES_IN_FLIGHT));

        for (uint32_t frame = 0; frame < static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT); ++frame) {
            for (uint32_t u = 0; u < vertexBindingCountClamped; ++u) {
                const size_t bufferIndex = static_cast<size_t>(frame) * vertexBindingCountClamped + u;
                VkBuffer bufferHandle = uniformBuffers[bufferIndex];
                if (bufferHandle == VK_NULL_HANDLE) {
                    throw std::runtime_error("uniform buffer handle is null during descriptor allocation");
                }
                bufferInfos.push_back({
                    .buffer = bufferHandle,
                    .offset = 0,
                    .range = VK_WHOLE_SIZE,
                });
                VkWriteDescriptorSet write = {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = descriptorSets[frame],
                    .dstBinding = u,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .pBufferInfo = &bufferInfos.back(),
                };
                descriptorWrites.push_back(write);
            }
            for (uint32_t t = 0; t < fragmentBindingCountClamped; ++t) {
                Image* img = textures[t];
                if (!img || img->imageView == VK_NULL_HANDLE) {
                    throw std::runtime_error("texture image view is null during descriptor allocation");
                }
                imageInfos.push_back({
                    .sampler = textureSampler,
                    .imageView = img->imageView,
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                });
                VkWriteDescriptorSet write = {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = descriptorSets[frame],
                    .dstBinding = vertexBindingCountClamped + t,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = &imageInfos.back(),
                };
                descriptorWrites.push_back(write);
            }
        }
        if (!descriptorWrites.empty()) {
            vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        }
        return descriptorSets;
    }
    void Renderer::createCommandBuffers() {
        commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = (uint32_t) commandBuffers.size();
        if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate command buffers!");
        }
    }
    void Renderer::createSyncObjects(){
        imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
        VkSemaphoreCreateInfo semaphoreInfo = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        };
        VkFenceCreateInfo fenceInfo = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT
        };
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS
            ||  vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS
            ||  vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create sync objects!");
            }
        }
    }
    void Renderer::createTextureImageView(VkFormat textureFormat, VkImage textureImage, VkImageView &textureImageView) {
        textureImageView = createImageView(textureImage, textureFormat, 1);
    }
    VkImageView Renderer::createImageView(VkImage image, VkFormat format, uint32_t mipLevels, VkImageAspectFlags aspectFlags, VkImageViewType viewType, uint32_t layerCount){
        VkImageAspectFlags resolvedAspect = aspectFlags;
        if (format == VK_FORMAT_D16_UNORM ||
            format == VK_FORMAT_X8_D24_UNORM_PACK32 ||
            format == VK_FORMAT_D32_SFLOAT ||
            format == VK_FORMAT_D16_UNORM_S8_UINT ||
            format == VK_FORMAT_D24_UNORM_S8_UINT ||
            format == VK_FORMAT_D32_SFLOAT_S8_UINT) {
            resolvedAspect = VK_IMAGE_ASPECT_DEPTH_BIT;
            if (hasStencilComponent(format)) {
                resolvedAspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
            }
        }
        VkImageViewCreateInfo viewInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = image,
            .viewType = viewType,
            .format = format,
            .subresourceRange = {
                .aspectMask = resolvedAspect,
                .baseMipLevel = 0,
                .levelCount = mipLevels,
                .baseArrayLayer = 0,
                .layerCount = layerCount,
            },
        };
        VkImageView imageView;
        if(vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture image view!");
        }
        return imageView;
    }
    void Renderer::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
        VkBufferCreateInfo bufferInfo = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = size,
            .usage = usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        if(vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to create buffer!");
        }
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, buffer, &memRequirements);
        VkMemoryAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = memRequirements.size,
            .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties),
        };
        if(vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate buffer memory!");
        }
        vkBindBufferMemory(device, buffer, bufferMemory, 0);
    }
    void Renderer::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t layerCount, VkDeviceSize layerSize) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();
        if (layerCount == 0) {
            layerCount = 1;
        }
        const VkDeviceSize inferredLayerSize = layerSize != 0 ? layerSize : static_cast<VkDeviceSize>(width) * static_cast<VkDeviceSize>(height) * 4;
        std::vector<VkBufferImageCopy> regions(layerCount);
        for (uint32_t layer = 0; layer < layerCount; ++layer) {
            VkBufferImageCopy region = {
                .bufferOffset = inferredLayerSize * layer,
                .bufferRowLength = 0,
                .bufferImageHeight = 0,
                .imageSubresource = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = 0,
                    .baseArrayLayer = layer,
                    .layerCount = 1,
                },
                .imageOffset = {0, 0, 0},
                .imageExtent = {
                    .width = width,
                    .height = height,
                    .depth = 1,
                },
            };
            regions[layer] = region;
        }
        vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<uint32_t>(regions.size()), regions.data());
        endSingleTimeCommands(commandBuffer);
    }
    void Renderer::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels, uint32_t layerCount) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();
        VkImageMemoryBarrier barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .oldLayout = oldLayout,
            .newLayout = newLayout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = mipLevels,
                .baseArrayLayer = 0,
                .layerCount = layerCount,
            },
        };
        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;
        if(oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if(oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else if(oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        } else if(oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        } else {
            throw std::invalid_argument("unsupported layout transition!");
        }
        if(newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            if(hasStencilComponent(format)) {
                barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
            }
        } else {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }
        vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        endSingleTimeCommands(commandBuffer);
    }
    void Renderer::createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory, uint32_t arrayLayers, VkImageCreateFlags flags) {
        VkImageCreateInfo imageInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .flags = flags,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = format,
            .extent = {
                .width = width,
                .height = height,
                .depth = 1
            },
            .mipLevels = mipLevels,
            .arrayLayers = arrayLayers,
            .samples = numSamples,
            .tiling = tiling,
            .usage = usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        };
        if(vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image!");
        }
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device, image, &memRequirements);
        VkMemoryAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = memRequirements.size,
            .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties),
        };
        if(vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate image memory!");
        }
        vkBindImageMemory(device, image, imageMemory, 0);
    }
    void Renderer::initWindow() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(WIDTH, HEIGHT, "ParticleFront", nullptr, nullptr);
        glfwSetWindowUserPointer(window, this);
        glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
        glfwSetCursorPosCallback(window, mouseMoveCallback);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        cursorLocked = false;
        float xscale = 1.0f, yscale = 1.0f;
        glfwGetWindowContentScale(window, &xscale, &yscale);
        float deviceScale = std::max(xscale, 1.0f);
        #ifdef __APPLE__
        if (xscale >= 1.5f || yscale >= 1.5f) {
            constexpr float kRetinaMultiplier = 1.25f;
            deviceScale *= kRetinaMultiplier;
        }
        #endif
        uiScale = deviceScale;
        textScale = deviceScale;
        #ifdef __APPLE__
        textSizeScale = 1.0f;
        #else
        textSizeScale = 1.5f;
        #endif
    }
    void Renderer::cleanup() {
        if (uiManager) {
            uiManager->clear();
            uiManager = nullptr;
        }
        if (fontManager) {
            fontManager->shutdown();
            fontManager = nullptr;
        }
        if (textureManager) {
            textureManager->shutdown();
            textureManager = nullptr;
        }
        if (shaderManager) {
            shaderManager->shutdown();
            shaderManager = nullptr;
        }
        if (sceneManager) {
            sceneManager->shutdown();
            sceneManager = nullptr;
        }
        if (entityManager) {
            entityManager->shutdown();
            entityManager = nullptr;
        }
        if (modelManager) {
            modelManager->shutdown();
            modelManager = nullptr;
        }
        if (device == VK_NULL_HANDLE) {
            return;
        }
        vkDeviceWaitIdle(device);
        cleanupSwapChain();
        if (gBufferRenderPass) {
            vkDestroyRenderPass(device, gBufferRenderPass, nullptr);
            gBufferRenderPass = VK_NULL_HANDLE;
        }
        if (lightingRenderPass) {
            vkDestroyRenderPass(device, lightingRenderPass, nullptr);
            lightingRenderPass = VK_NULL_HANDLE;
        }
        if (compositeRenderPass) {
            vkDestroyRenderPass(device, compositeRenderPass, nullptr);
            compositeRenderPass = VK_NULL_HANDLE;
        }
        if (gBufferSampler) {
            vkDestroySampler(device, gBufferSampler, nullptr);
            gBufferSampler = VK_NULL_HANDLE;
        }
        if (textureSampler) {
            vkDestroySampler(device, textureSampler, nullptr);
            textureSampler = VK_NULL_HANDLE;
        }
        if (!commandBuffers.empty() && commandPool) {
            vkFreeCommandBuffers(device, commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
            commandBuffers.clear();
        }
        for (size_t i = 0; i < imageAvailableSemaphores.size(); ++i) {
            if (imageAvailableSemaphores[i]) {
                vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
            }
        }
        imageAvailableSemaphores.clear();
        for (size_t i = 0; i < renderFinishedSemaphores.size(); ++i) {
            if (renderFinishedSemaphores[i]) {
                vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
            }
        }
        renderFinishedSemaphores.clear();
        for (size_t i = 0; i < inFlightFences.size(); ++i) {
            if (inFlightFences[i]) {
                vkDestroyFence(device, inFlightFences[i], nullptr);
            }
        }
        inFlightFences.clear();
        if (quadVertexBuffer) {
            vkDestroyBuffer(device, quadVertexBuffer, nullptr);
            quadVertexBuffer = VK_NULL_HANDLE;
        }
        if (quadVertexBufferMemory) {
            vkFreeMemory(device, quadVertexBufferMemory, nullptr);
            quadVertexBufferMemory = VK_NULL_HANDLE;
        }
        if (quadIndexBuffer) {
            vkDestroyBuffer(device, quadIndexBuffer, nullptr);
            quadIndexBuffer = VK_NULL_HANDLE;
        }
        if (quadIndexBufferMemory) {
            vkFreeMemory(device, quadIndexBufferMemory, nullptr);
            quadIndexBufferMemory = VK_NULL_HANDLE;
        }
        if (commandPool) {
            vkDestroyCommandPool(device, commandPool, nullptr);
            commandPool = VK_NULL_HANDLE;
        }
        if (device != VK_NULL_HANDLE) {
            vkDestroyDevice(device, nullptr);
            device = VK_NULL_HANDLE;
        }
        if (surface) {
            vkDestroySurfaceKHR(instance, surface, nullptr);
            surface = VK_NULL_HANDLE;
        }
        if (enableValidationLayers && debugMessenger) {
            DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
            debugMessenger = VK_NULL_HANDLE;
        }
        if (instance) {
            vkDestroyInstance(instance, nullptr);
            instance = VK_NULL_HANDLE;
        }
        if (window) {
            glfwDestroyWindow(window);
            window = nullptr;
        }
        glfwTerminate();
    }
    void Renderer::initVulkan() {
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createGBufferResources();
        createGBufferRenderPass();
        createGBufferFramebuffers();
        createLightingResources();
        createLightingRenderPass();
        createLightingFramebuffers();
        createCompositeRenderPass();
        createCompositeFramebuffers();
        createCommandPool();
        createSSRResources();
        createTextureSampler();
        createGBufferSampler();
        shaderManager = ShaderManager::getInstance();
        setupUI();
        sceneManager = SceneManager::getInstance();
        sceneManager->switchScene(0);
        modelManager = ModelManager::getInstance();
        entityManager = EntityManager::getInstance();
        inputManager = InputManager::getInstance();
        createDeferredDescriptorSets();
        createColorResources();
        createDepthResources();
        textureManager = TextureManager::getInstance();
        uiManager->loadTextures();
        createQuadBuffers();
        createCommandBuffers();
        createSyncObjects();
    }
    void Renderer::mainLoop() {
        while(!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            processInput(window);
            drawFrame();
        }
        vkDeviceWaitIdle(device);
    }
    void Renderer::drawFrame() {
        deltaTime = static_cast<float>(glfwGetTime()) - currentTime;
        currentTime = static_cast<float>(glfwGetTime());
        vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapChain();
            return;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }
        vkResetFences(device, 1, &inFlightFences[currentFrame]);
        vkResetCommandBuffer(commandBuffers[currentFrame], 0);
        recordDeferredCommandBuffer(commandBuffers[currentFrame], imageIndex);
        VkPipelineStageFlags waitStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submitInfo = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &imageAvailableSemaphores[currentFrame],
            .pWaitDstStageMask = &waitStages,
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffers[currentFrame],
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &renderFinishedSemaphores[currentFrame],
        };
        if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }
        VkPresentInfoKHR presentInfo = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &renderFinishedSemaphores[currentFrame],
            .swapchainCount = 1,
            .pSwapchains = &swapChain,
            .pImageIndices = &imageIndex,
        };
        result = vkQueuePresentKHR(presentQueue, &presentInfo);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
            framebufferResized = false;
            recreateSwapChain();
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }
        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}
void Renderer::createInstance() {
    if(!glfwVulkanSupported()) {
        throw std::runtime_error(
            "GLFW reports that Vulkan is unavailable. Ensure the Vulkan SDK/MoltenVK is installed and "
            "relaunch the application with VK_ICD_FILENAMES pointing at MoltenVK_icd.json."
        );
    }
    if (enableValidationLayers && !checkValidationLayerSupport()) {
        throw std::runtime_error("validation layers requested, but not available!");
    }
    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pApplicationName = "ParticleFront",
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .pEngineName = "No Engine",
            .engineVersion = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion = VK_API_VERSION_1_1,
        };
        VkInstanceCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pApplicationInfo = &appInfo,
        };
        #ifdef __APPLE__
            createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        #endif
        std::vector<const char*> extensions = getRequiredExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo {};
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
            populateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
        } else {
            createInfo.enabledLayerCount = 0;
            createInfo.pNext = nullptr;
        }
        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("failed to create instance!");
        }
    }
    void Renderer::setupDebugMessenger() {
        if(!enableValidationLayers) return;
        VkDebugUtilsMessengerCreateInfoEXT createInfo;
        populateDebugMessengerCreateInfo(createInfo);
        if(CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("failed to set up debug messenger!");
        }
    }
    void Renderer::createSurface() {
        if(!window) {
            throw std::runtime_error("GLFW window was not created before attempting to create a Vulkan surface.");
        }
        VkResult result = glfwCreateWindowSurface(instance, window, nullptr, &surface);
        if(result != VK_SUCCESS) {
            throw std::runtime_error(
                "Failed to create Vulkan window surface (VkResult " + std::to_string(static_cast<int32_t>(result)) +
                "). Ensure MoltenVK is installed and discoverable via VK_ICD_FILENAMES."
            );
        }
    }
    void Renderer::pickPhysicalDevice() {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if(deviceCount == 0) {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
        std::multimap<int, VkPhysicalDevice> candidates;
        for(const auto &device : devices) {
            int score = rateDeviceSuitability(device);
            candidates.insert(std::make_pair(score, device));
        }
        if(candidates.rbegin()->first > 0 && candidates.rbegin()->first > 0) {
            physicalDevice = candidates.rbegin()->second;
        } else {
            throw std::runtime_error("failed to find a suitable GPU!");
        }
        msaaSamples = getMaxUsableSampleCount();
    }
    void Renderer::createLogicalDevice() {
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = {
            indices.graphicsFamily.value(),
            indices.presentFamily.value()
        };
        float queuePriority = 1.0f;
        for(uint32_t queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = queueFamily,
                .queueCount = 1,
                .pQueuePriorities = &queuePriority,
            };
            queueCreateInfos.push_back(queueCreateInfo);
        }
        VkPhysicalDeviceFeatures deviceFeatures = {
            .sampleRateShading = VK_TRUE,
            .samplerAnisotropy = VK_TRUE,
        };
        bool enableAtomicFloatExt = false;
        bool canUseBufferFloat32AtomicAdd = false;
        VkPhysicalDeviceShaderAtomicFloatFeaturesEXT atomicFloatFeatures = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT,
        };
        VkPhysicalDeviceFeatures2 features2 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .pNext = &atomicFloatFeatures,
        };
        vkGetPhysicalDeviceFeatures2(physicalDevice, &features2);
        if(hasDeviceExtension(physicalDevice, VK_EXT_SHADER_ATOMIC_FLOAT_EXTENSION_NAME)) {
            enableAtomicFloatExt = true;
            canUseBufferFloat32AtomicAdd = atomicFloatFeatures.shaderBufferFloat32AtomicAdd == VK_TRUE;
        }
        g_useCASAdvection = !(enableAtomicFloatExt && canUseBufferFloat32AtomicAdd);
        VkDeviceCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
            .pQueueCreateInfos = queueCreateInfos.data(),
        };
        VkPhysicalDeviceFeatures2 enabledFeatures2 = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .features = deviceFeatures,
        };
        VkPhysicalDeviceShaderAtomicFloatFeaturesEXT enabledAtomicFloat{};
        if(!g_useCASAdvection && enableAtomicFloatExt && canUseBufferFloat32AtomicAdd) {
            enabledAtomicFloat.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_FLOAT_FEATURES_EXT;
            enabledAtomicFloat.shaderBufferFloat32AtomicAdd = VK_TRUE;
            enabledFeatures2.pNext = &enabledAtomicFloat;
        }
        createInfo.pNext = &enabledFeatures2;
        createInfo.pEnabledFeatures = nullptr;
        std::vector<const char*> enabledExts = deviceExtensions;
        if(!g_useCASAdvection && enableAtomicFloatExt) {
            enabledExts.push_back(VK_EXT_SHADER_ATOMIC_FLOAT_EXTENSION_NAME);
        }
        createInfo.enabledExtensionCount = static_cast<uint32_t>(enabledExts.size());
        createInfo.ppEnabledExtensionNames = enabledExts.data();
        if(enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        } else {
            createInfo.enabledLayerCount = 0;
        }
        if(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("failed to create logical device!");
        }
        vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
        vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
    }
    void Renderer::createSwapChain() {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);
        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);
        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }
        VkSwapchainCreateInfoKHR createInfo = {
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface = surface,
            .minImageCount = imageCount,
            .imageFormat = surfaceFormat.format,
            .imageColorSpace = surfaceFormat.colorSpace,
            .imageExtent = extent,
            .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .preTransform = swapChainSupport.capabilities.currentTransform,
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode = presentMode,
            .clipped = VK_TRUE,
            .oldSwapchain = VK_NULL_HANDLE,
        };
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        uint32_t queueFamilyIndices[] = {
            indices.graphicsFamily.value(),
            indices.presentFamily.value()
        };
        if (indices.graphicsFamily != indices.presentFamily) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }
        if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
            throw std::runtime_error("failed to create swap chain!");
        }
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
        swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());
        swapChainImageFormat = surfaceFormat.format;
        swapChainExtent = extent;
    }
    void Renderer::recreateSwapChain() {
        int width = 0, height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        while(width == 0 || height == 0) {
            glfwGetFramebufferSize(window, &width, &height);
            glfwWaitEvents();
        }
        vkDeviceWaitIdle(device);
        cleanupSwapChain();
        createSwapChain();
        createImageViews();
        createGBufferResources();
        createGBufferFramebuffers();
        createLightingResources();
        createLightingFramebuffers();
        createSSRResources();
        createCompositeFramebuffers();
        recreateDeferredDescriptorSets();
    }
    void Renderer::cleanupSwapChain() {
        if (device == VK_NULL_HANDLE) {
            swapChainFramebuffers.clear();
            swapChainImageViews.clear();
            swapChain = VK_NULL_HANDLE;
            depthImageView = VK_NULL_HANDLE;
            depthImage = VK_NULL_HANDLE;
            depthImageMemory = VK_NULL_HANDLE;
            colorImageView = VK_NULL_HANDLE;
            colorImage = VK_NULL_HANDLE;
            colorImageMemory = VK_NULL_HANDLE;
            return;
        }
        for (auto framebuffer : gBufferFramebuffers) {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }
        gBufferFramebuffers.clear();
        for (auto framebuffer : lightingFramebuffers) {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }
        lightingFramebuffers.clear();
        for (auto framebuffer : compositeFramebuffers) {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }
        compositeFramebuffers.clear();
        if (gBufferAlbedoView) vkDestroyImageView(device, gBufferAlbedoView, nullptr);
        if (gBufferAlbedoImage) vkDestroyImage(device, gBufferAlbedoImage, nullptr);
        if (gBufferAlbedoMemory) vkFreeMemory(device, gBufferAlbedoMemory, nullptr);
        if (gBufferNormalView) vkDestroyImageView(device, gBufferNormalView, nullptr);
        if (gBufferNormalImage) vkDestroyImage(device, gBufferNormalImage, nullptr);
        if (gBufferNormalMemory) vkFreeMemory(device, gBufferNormalMemory, nullptr);
        if (gBufferMaterialView) vkDestroyImageView(device, gBufferMaterialView, nullptr);
        if (gBufferMaterialImage) vkDestroyImage(device, gBufferMaterialImage, nullptr);
        if (gBufferMaterialMemory) vkFreeMemory(device, gBufferMaterialMemory, nullptr);
        if (gBufferDepthView) vkDestroyImageView(device, gBufferDepthView, nullptr);
        if (gBufferDepthImage) vkDestroyImage(device, gBufferDepthImage, nullptr);
        if (gBufferDepthMemory) vkFreeMemory(device, gBufferDepthMemory, nullptr);
        if (lightingView) vkDestroyImageView(device, lightingView, nullptr);
        if (lightingImage) vkDestroyImage(device, lightingImage, nullptr);
        if (lightingMemory) vkFreeMemory(device, lightingMemory, nullptr);
        if (ssrView) vkDestroyImageView(device, ssrView, nullptr);
        if (ssrImage) vkDestroyImage(device, ssrImage, nullptr);
        if (ssrMemory) vkFreeMemory(device, ssrMemory, nullptr);
        for (auto framebuffer : swapChainFramebuffers) {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }
        swapChainFramebuffers.clear();
        if (depthImageView) vkDestroyImageView(device, depthImageView, nullptr);
        if (depthImage) vkDestroyImage(device, depthImage, nullptr);
        if (depthImageMemory) vkFreeMemory(device, depthImageMemory, nullptr);
        if (colorImageView) vkDestroyImageView(device, colorImageView, nullptr);
        if (colorImage) vkDestroyImage(device, colorImage, nullptr);
        if (colorImageMemory) vkFreeMemory(device, colorImageMemory, nullptr);
        for (auto imageView : swapChainImageViews) {
            vkDestroyImageView(device, imageView, nullptr);
        }
        swapChainImageViews.clear();
        if (swapChain) vkDestroySwapchainKHR(device, swapChain, nullptr);
    }
    void Renderer::createImageViews() {
        swapChainImageViews.resize(swapChainImages.size());
        for(size_t i=0; i<swapChainImages.size(); i++) {
            swapChainImageViews[i] = createImageView(swapChainImages[i], swapChainImageFormat, 1);
        }
    }
    void Renderer::createGBufferResources() {
        createImage(swapChainExtent.width, swapChainExtent.height, 1, VK_SAMPLE_COUNT_1_BIT,
            VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, gBufferAlbedoImage, gBufferAlbedoMemory);
        gBufferAlbedoView = createImageView(gBufferAlbedoImage, VK_FORMAT_R16G16B16A16_SFLOAT, 1);

        createImage(swapChainExtent.width, swapChainExtent.height, 1, VK_SAMPLE_COUNT_1_BIT,
            VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, gBufferNormalImage, gBufferNormalMemory);
        gBufferNormalView = createImageView(gBufferNormalImage, VK_FORMAT_R16G16B16A16_SFLOAT, 1);

        createImage(swapChainExtent.width, swapChainExtent.height, 1, VK_SAMPLE_COUNT_1_BIT,
            VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, gBufferMaterialImage, gBufferMaterialMemory);
        gBufferMaterialView = createImageView(gBufferMaterialImage, VK_FORMAT_R8G8B8A8_UNORM, 1);

        VkFormat depthFormat = findDepthFormat();
        createImage(swapChainExtent.width, swapChainExtent.height, 1, VK_SAMPLE_COUNT_1_BIT,
            depthFormat, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, gBufferDepthImage, gBufferDepthMemory);
        gBufferDepthView = createImageView(gBufferDepthImage, depthFormat, 1, VK_IMAGE_ASPECT_DEPTH_BIT);
    }
    void Renderer::createGBufferRenderPass() {
        VkAttachmentDescription albedoAttachment = {
            .format = VK_FORMAT_R16G16B16A16_SFLOAT,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
        
        VkAttachmentDescription normalAttachment = {
            .format = VK_FORMAT_R16G16B16A16_SFLOAT,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
        
        VkAttachmentDescription materialAttachment = {
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
        
        VkAttachmentDescription depthAttachment = {
            .format = findDepthFormat(),
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
        
        VkAttachmentReference colorRefs[3] = {
            {.attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},  // Albedo
            {.attachment = 1, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},  // Normal
            {.attachment = 2, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},  // Material
        };
        
        VkAttachmentReference depthRef = {
            .attachment = 3,
            .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        };
        
        VkSubpassDescription subpass = {
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .colorAttachmentCount = 3,
            .pColorAttachments = colorRefs,
            .pDepthStencilAttachment = &depthRef,
        };
        
        VkSubpassDependency dependency = {
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        };
        
        std::array<VkAttachmentDescription, 4> attachments = {
            albedoAttachment,
            normalAttachment,
            materialAttachment,
            depthAttachment
        };
        
        VkRenderPassCreateInfo renderPassInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .attachmentCount = 4,
            .pAttachments = attachments.data(),
            .subpassCount = 1,
            .pSubpasses = &subpass,
            .dependencyCount = 1,
            .pDependencies = &dependency,
        };
        
        if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &gBufferRenderPass) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create G-Buffer render pass!");
        }
    }
    void Renderer::createLightingResources() {
        createImage(swapChainExtent.width, swapChainExtent.height, 1, VK_SAMPLE_COUNT_1_BIT,
            VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, lightingImage, lightingMemory);
        lightingView = createImageView(lightingImage, VK_FORMAT_R16G16B16A16_SFLOAT, 1);
    }
    void Renderer::createLightingRenderPass() {
        VkAttachmentDescription lightingAttachment = {
            .format = VK_FORMAT_R16G16B16A16_SFLOAT,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
        VkAttachmentReference colorRef = {
            .attachment = 0,
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        };
        VkSubpassDescription subpass = {
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorRef,
        };
        VkSubpassDependency dependency = {
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        };
        VkRenderPassCreateInfo renderPassInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments = &lightingAttachment,
            .subpassCount = 1,
            .pSubpasses = &subpass,
            .dependencyCount = 1,
            .pDependencies = &dependency,
        };
        if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &lightingRenderPass) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create lighting render pass!");
        }
    }
    void Renderer::createGBufferFramebuffers() {
        gBufferFramebuffers.resize(swapChainImageViews.size());
        for (size_t i = 0; i < swapChainImageViews.size(); i++) {
            std::array<VkImageView, 4> attachments = {
                gBufferAlbedoView,
                gBufferNormalView,
                gBufferMaterialView,
                gBufferDepthView
            };
            VkFramebufferCreateInfo framebufferInfo = {
                .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .renderPass = gBufferRenderPass,
                .attachmentCount = static_cast<uint32_t>(attachments.size()),
                .pAttachments = attachments.data(),
                .width = swapChainExtent.width,
                .height = swapChainExtent.height,
                .layers = 1,
            };
            
            if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &gBufferFramebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create G-Buffer framebuffer!");
            }
        }
    }
    void Renderer::createGBufferSampler() {
        VkSamplerCreateInfo samplerInfo = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_NEAREST,
            .minFilter = VK_FILTER_NEAREST,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .mipLodBias = 0.0f,
            .anisotropyEnable = VK_FALSE,
            .maxAnisotropy = 1.0f,
            .compareEnable = VK_FALSE,
            .compareOp = VK_COMPARE_OP_ALWAYS,
            .minLod = 0.0f,
            .maxLod = 0.0f,
            .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
            .unnormalizedCoordinates = VK_FALSE,
        };
        if (vkCreateSampler(device, &samplerInfo, nullptr, &gBufferSampler) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create G-Buffer sampler!");
        }
    }
    void Renderer::createLightingFramebuffers() {
        lightingFramebuffers.resize(swapChainImageViews.size());
        for (size_t i = 0; i < swapChainImageViews.size(); i++) {
            VkImageView attachments[] = {
                lightingView
            };
            VkFramebufferCreateInfo framebufferInfo = {
                .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .renderPass = lightingRenderPass,
                .attachmentCount = 1,
                .pAttachments = attachments,
                .width = swapChainExtent.width,
                .height = swapChainExtent.height,
                .layers = 1,
            };
            if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &lightingFramebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create lighting framebuffer!");
            }
        }
    }
    void Renderer::createSSRResources() {
        createImage(swapChainExtent.width, swapChainExtent.height, 1, VK_SAMPLE_COUNT_1_BIT,
            VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, ssrImage, ssrMemory);
        ssrView = createImageView(ssrImage, VK_FORMAT_R16G16B16A16_SFLOAT, 1);
        transitionImageLayout(ssrImage, VK_FORMAT_R16G16B16A16_SFLOAT, 
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    }
    void Renderer::createCompositeRenderPass() {
        VkAttachmentDescription colorAttachment = {
            .format = swapChainImageFormat,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        };
        VkAttachmentReference colorRef = {
            .attachment = 0,
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        };
        VkSubpassDescription subpass = {
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorRef,
        };
        VkSubpassDependency dependency = {
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        };
        VkRenderPassCreateInfo renderPassInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments = &colorAttachment,
            .subpassCount = 1,
            .pSubpasses = &subpass,
            .dependencyCount = 1,
            .pDependencies = &dependency,
        };
        if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &compositeRenderPass) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create composite render pass!");
        }
    }
    void Renderer::createCompositeFramebuffers() {
        compositeFramebuffers.resize(swapChainImageViews.size());
        for (size_t i = 0; i < swapChainImageViews.size(); i++) {
            VkImageView attachments[] = {
                swapChainImageViews[i]
            };
            VkFramebufferCreateInfo framebufferInfo = {
                .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .renderPass = compositeRenderPass,
                .attachmentCount = 1,
                .pAttachments = attachments,
                .width = swapChainExtent.width,
                .height = swapChainExtent.height,
                .layers = 1,
            };
            if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &compositeFramebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create composite framebuffer!");
            }
        }
    }
    void Renderer::createDeferredDescriptorSets() {
        Shader* lightingShader = shaderManager->getShader("lighting");
        Shader* compositeShader = shaderManager->getShader("composite");
        
        if (!lightingShader || !compositeShader) {
            throw std::runtime_error("Failed to get lighting or composite shader for descriptor set creation!");
        }
        std::vector<VkDescriptorSetLayout> lightingLayouts(MAX_FRAMES_IN_FLIGHT, lightingShader->descriptorSetLayout);
        VkDescriptorSetAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = lightingShader->descriptorPool,
            .descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
            .pSetLayouts = lightingLayouts.data(),
        };
        lightingDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
        if (vkAllocateDescriptorSets(device, &allocInfo, lightingDescriptorSets.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate lighting descriptor sets!");
        }
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            std::array<VkDescriptorImageInfo, 4> imageInfos = {{
                {
                    .sampler = gBufferSampler,
                    .imageView = gBufferAlbedoView,
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                },
                {
                    .sampler = gBufferSampler,
                    .imageView = gBufferNormalView,
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                },
                {
                    .sampler = gBufferSampler,
                    .imageView = gBufferMaterialView,
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                },
                {
                    .sampler = gBufferSampler,
                    .imageView = gBufferDepthView,
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                },
            }};
            std::array<VkWriteDescriptorSet, 4> descriptorWrites = {{
                {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = lightingDescriptorSets[i],
                    .dstBinding = 0,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = &imageInfos[0],
                },
                {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = lightingDescriptorSets[i],
                    .dstBinding = 1,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = &imageInfos[1],
                },
                {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = lightingDescriptorSets[i],
                    .dstBinding = 2,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = &imageInfos[2],
                },
                {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = lightingDescriptorSets[i],
                    .dstBinding = 3,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = &imageInfos[3],
                },
            }};
            vkUpdateDescriptorSets(device, 4, descriptorWrites.data(), 0, nullptr);
        }
        std::vector<VkDescriptorSetLayout> compositeLayouts(MAX_FRAMES_IN_FLIGHT, compositeShader->descriptorSetLayout);
        VkDescriptorSetAllocateInfo compositeAllocInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = compositeShader->descriptorPool,
            .descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
            .pSetLayouts = compositeLayouts.data(),
        };
        
        compositeDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
        if (vkAllocateDescriptorSets(device, &compositeAllocInfo, compositeDescriptorSets.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate composite descriptor sets!");
        }
        
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            std::array<VkDescriptorImageInfo, 2> imageInfos = {{
                {
                    .sampler = gBufferSampler,
                    .imageView = lightingView,
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                },
                {
                    .sampler = gBufferSampler,
                    .imageView = ssrView,
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                },
            }};
            
            std::array<VkWriteDescriptorSet, 2> descriptorWrites = {{
                {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = compositeDescriptorSets[i],
                    .dstBinding = 0,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = &imageInfos[0],
                },
                {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = compositeDescriptorSets[i],
                    .dstBinding = 1,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = &imageInfos[1],
                },
            }};
            
            vkUpdateDescriptorSets(device, 2, descriptorWrites.data(), 0, nullptr);
        }
        ComputeShader* ssrShader = shaderManager->getComputeShader("ssr");
        if (!ssrShader) {
            throw std::runtime_error("Failed to get SSR compute shader for descriptor set creation!");
        }
        std::vector<VkDescriptorSetLayout> ssrLayouts(MAX_FRAMES_IN_FLIGHT, ssrShader->descriptorSetLayout);
        VkDescriptorSetAllocateInfo ssrAllocInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = ssrShader->descriptorPool,
            .descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
            .pSetLayouts = ssrLayouts.data(),
        };
        
        ssrDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
        if (vkAllocateDescriptorSets(device, &ssrAllocInfo, ssrDescriptorSets.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate SSR descriptor sets!");
        }
        
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            VkDescriptorImageInfo storageImageInfo = {
                .sampler = VK_NULL_HANDLE,
                .imageView = ssrView,
                .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
            };
            std::array<VkDescriptorImageInfo, 3> samplerInfos = {{
                {
                    .sampler = gBufferSampler,
                    .imageView = lightingView,
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                },
                {
                    .sampler = gBufferSampler,
                    .imageView = gBufferDepthView,
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                },
                {
                    .sampler = gBufferSampler,
                    .imageView = gBufferNormalView,
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                },
            }};
            std::array<VkWriteDescriptorSet, 4> descriptorWrites = {{
                {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = ssrDescriptorSets[i],
                    .dstBinding = 0,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                    .pImageInfo = &storageImageInfo,
                },
                {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = ssrDescriptorSets[i],
                    .dstBinding = 1,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = &samplerInfos[0],
                },
                {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = ssrDescriptorSets[i],
                    .dstBinding = 2,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = &samplerInfos[1],
                },
                {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = ssrDescriptorSets[i],
                    .dstBinding = 3,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = &samplerInfos[2],
                },
            }};
            vkUpdateDescriptorSets(device, 4, descriptorWrites.data(), 0, nullptr);
        }
    }
    void Renderer::recreateDeferredDescriptorSets() {
        Shader* lightingShader = shaderManager->getShader("lighting");
        Shader* compositeShader = shaderManager->getShader("composite");
        ComputeShader* ssrShader = shaderManager->getComputeShader("ssr");
        
        if (lightingShader && lightingShader->descriptorPool != VK_NULL_HANDLE) {
            vkResetDescriptorPool(device, lightingShader->descriptorPool, 0);
        }
        if (compositeShader && compositeShader->descriptorPool != VK_NULL_HANDLE) {
            vkResetDescriptorPool(device, compositeShader->descriptorPool, 0);
        }
        if (ssrShader && ssrShader->descriptorPool != VK_NULL_HANDLE) {
            vkResetDescriptorPool(device, ssrShader->descriptorPool, 0);
        }
        
        lightingDescriptorSets.clear();
        compositeDescriptorSets.clear();
        ssrDescriptorSets.clear();
        createDeferredDescriptorSets();
    }
    VkFormat Renderer::findDepthFormat() {
        return findSupportedFormat({
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_D24_UNORM_S8_UINT
        }, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        );
    }
    VkFormat Renderer::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
        for(VkFormat format : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
                return format;
            } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
                return format;
            }
        }
        throw std::runtime_error("failed to find supported format!");
    }
    void Renderer::createCommandPool() {
        QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);
        VkCommandPoolCreateInfo poolInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = queueFamilyIndices.graphicsFamily.value(),
        };
        if(vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create command pool!");
        }
    }
    void Renderer::createQuadBuffers() {
        std::vector<TextVertex> vertices = {
            {{-1.0f, -1.0f}, {0.0f, 0.0f}},
            {{ 1.0f, -1.0f}, {1.0f, 0.0f}},
            {{ 1.0f,  1.0f}, {1.0f, 1.0f}},
            {{-1.0f,  1.0f}, {0.0f, 1.0f}},
        };
        std::vector<uint32_t> indices = {
            0, 1, 2, 2, 3, 0
        };
        VkDeviceSize vertexBufferSize = sizeof(vertices[0]) * vertices.size();
        VkDeviceSize indexBufferSize = sizeof(indices[0]) * indices.size();
        VkBuffer stagingVertexBuffer;
        VkDeviceMemory stagingVertexBufferMemory;
        createBuffer(vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingVertexBuffer, stagingVertexBufferMemory);
        void* data;
        vkMapMemory(device, stagingVertexBufferMemory, 0, vertexBufferSize, 0, &data);
        memcpy(data, vertices.data(), (size_t) vertexBufferSize);
        vkUnmapMemory(device, stagingVertexBufferMemory);
        createBuffer(vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, quadVertexBuffer, quadVertexBufferMemory);
        copyBuffer(stagingVertexBuffer, quadVertexBuffer, vertexBufferSize);
        vkDestroyBuffer(device, stagingVertexBuffer, nullptr);
        vkFreeMemory(device, stagingVertexBufferMemory, nullptr);
        VkBuffer stagingIndexBuffer;
        VkDeviceMemory stagingIndexBufferMemory;
        createBuffer(indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingIndexBuffer, stagingIndexBufferMemory);
        vkMapMemory(device, stagingIndexBufferMemory, 0, indexBufferSize, 0, &data);
        memcpy(data, indices.data(), (size_t) indexBufferSize);
        vkUnmapMemory(device, stagingIndexBufferMemory);
        createBuffer(indexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, quadIndexBuffer, quadIndexBufferMemory);
        copyBuffer(stagingIndexBuffer, quadIndexBuffer, indexBufferSize);
        vkDestroyBuffer(device, stagingIndexBuffer, nullptr);
        vkFreeMemory(device, stagingIndexBufferMemory, nullptr);
    }
    void Renderer::setupUI() {
        uiManager = UIManager::getInstance();
        fontManager = FontManager::getInstance();
        fontManager->loadFont("src/assets/fonts/Lato.ttf", "Lato", 48);
    }
    void Renderer::updateEntities() {
        auto& entities = entityManager->getAllEntities();
        auto traverse = [&](auto&& self, Entity* entity) -> void {
            entity->updateWorldTransform();
            entity->update(deltaTime);
            for (Entity* child : entity->getChildren()) {
                self(self, child);
            }
        };
        for (auto& [name, entity] : entities) {
            if (entity->getParent() == nullptr) {
                traverse(traverse, entity);
            }
        }
    }
    void Renderer::renderEntitiesGeometry(VkCommandBuffer commandBuffer) {
        auto& entities = entityManager->getAllEntities();
        glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 5.0f);
        float cameraFOV = 45.0f;
        glm::mat4 view = glm::mat4(1.0f);

        int culledEntities = 0;
        Frustum frustrum;
        if (activeCamera) {
            glm::mat4 cameraWorld = activeCamera->getWorldTransform();
            glm::vec4 worldPos = cameraWorld * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            cameraPos = glm::vec3(worldPos);
            cameraFOV = activeCamera->getFOV();
            float aspectRatio = static_cast<float>(swapChainExtent.width) / std::max(static_cast<float>(swapChainExtent.height), 1.0f);
            frustrum = activeCamera->getFrustrum(aspectRatio, 0.1f, 100.0f, cameraWorld);
            view = glm::inverse(cameraWorld);
        }
        auto renderEntity = [&](Entity* entity) -> bool {
            if (!entity->isActive()) {
                return false;
            }
            std::string shaderName = entity->getShader();
            if (shaderName != "gbuffer" && shaderName != "skybox") {
                return true;
            }
            glm::mat4 modelMatrix = entity->getWorldTransform();
            if (activeCamera && entity->getModel() && entity->getName() != "skybox") {
                AABB bounds = entity->getWorldBounds(modelMatrix);
                if (!frustrum.intersectsAABB(bounds.min, bounds.max)) {
                    culledEntities++;
                    return true;  // Culled: skip rendering
                }
            }
            Model* model = entity->getModel();
            Shader* shader = shaderManager->getShader(shaderName);
            if (!model || !shader) return true;
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shader->pipeline);
            
            VkViewport viewport = {
                .x = 0.0f,
                .y = 0.0f,
                .width = static_cast<float>(swapChainExtent.width),
                .height = static_cast<float>(swapChainExtent.height),
                .minDepth = 0.0f,
                .maxDepth = 1.0f,
            };
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
            VkRect2D scissor = {
                .offset = {0, 0},
                .extent = swapChainExtent,
            };
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
            
            UniformBufferObject ubo{};
            ubo.model = modelMatrix;
            ubo.view = view;
            ubo.proj = glm::perspective(glm::radians(cameraFOV), static_cast<float>(swapChainExtent.width) / std::max(static_cast<float>(swapChainExtent.height), 1.0f), 0.1f, 200.0f);
            ubo.proj[1][1] *= -1;
            ubo.cameraPos = cameraPos;
            entity->updateUniformBuffer(currentFrame, ubo);
            const uint32_t indexCount = model->getIndexCount();
            if (indexCount > 0) {
                VkBuffer vertexBuffer = model->getVertexBuffer();
                VkBuffer indexBuffer = model->getIndexBuffer();
                if (vertexBuffer != VK_NULL_HANDLE && indexBuffer != VK_NULL_HANDLE) {
                    VkDeviceSize offsets[] = {0};
                    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
                    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
                    auto descriptorSets = entity->getDescriptorSets();
                    if (descriptorSets.size() == MAX_FRAMES_IN_FLIGHT && descriptorSets[currentFrame] != VK_NULL_HANDLE) {
                        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shader->pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);
                        vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
                    }
                }
            }
            return true;
        };
        
        if (entities.empty()) return;
        
        auto traverse = [&](auto&& self, Entity* entity) -> void {
            if (!renderEntity(entity)) {
                return;
            }
            for (Entity* child : entity->getChildren()) {
                self(self, child);
            }
        };
        
        for (auto& [name, entity] : entities) {
            if (entity->getParent() == nullptr) {
                traverse(traverse, entity);
            }
        }
    }
    void Renderer::transitionGBufferForReading(VkCommandBuffer commandBuffer) {
        VkImageMemoryBarrier barriers[4] = {};

        // Albedo
        barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barriers[0].pNext = nullptr;
        barriers[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barriers[0].oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barriers[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[0].image = gBufferAlbedoImage;
        barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barriers[0].subresourceRange.baseMipLevel = 0;
        barriers[0].subresourceRange.levelCount = 1;
        barriers[0].subresourceRange.baseArrayLayer = 0;
        barriers[0].subresourceRange.layerCount = 1;
        // Normal
        barriers[1] = barriers[0];
        barriers[1].image = gBufferNormalImage;
        // Material
        barriers[2] = barriers[0];
        barriers[2].image = gBufferMaterialImage;
        // Depth
        barriers[3].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barriers[3].pNext = nullptr;
        barriers[3].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        barriers[3].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barriers[3].oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        barriers[3].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barriers[3].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[3].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[3].image = gBufferDepthImage;
        barriers[3].subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        barriers[3].subresourceRange.baseMipLevel = 0;
        barriers[3].subresourceRange.levelCount = 1;
        barriers[3].subresourceRange.baseArrayLayer = 0;
        barriers[3].subresourceRange.layerCount = 1;
        
        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 4, barriers
        );
    }
    void Renderer::renderDeferredLighting(VkCommandBuffer commandBuffer) {
        Shader* lightingShader = shaderManager->getShader("lighting");
        if (!lightingShader) {
            return;
        }
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lightingShader->pipeline);
        VkViewport viewport = {
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>(swapChainExtent.width),
            .height = static_cast<float>(swapChainExtent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        VkRect2D scissor = {
            .offset = {0, 0},
            .extent = swapChainExtent,
        };
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        
        if (!lightingDescriptorSets.empty() && lightingDescriptorSets[currentFrame] != VK_NULL_HANDLE) {
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                lightingShader->pipelineLayout, 0, 1, &lightingDescriptorSets[currentFrame], 0, nullptr);
        }

        glm::vec3 cameraPos = glm::vec3(0.0f);
        float cameraFOV = 90.0f;
        glm::mat4 invView = glm::mat4(1.0f);
        glm::mat4 invProj = glm::mat4(1.0f);
        if (activeCamera) {
            glm::mat4 cameraWorld = activeCamera->getWorldTransform();
            glm::vec4 worldPos = cameraWorld * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            cameraPos = glm::vec3(worldPos);
            cameraFOV = activeCamera->getFOV();
            float aspectRatio = static_cast<float>(swapChainExtent.width) / std::max(static_cast<float>(swapChainExtent.height), 1.0f);
            invView = cameraWorld;
            glm::mat4 proj = glm::perspective(glm::radians(cameraFOV), aspectRatio, 0.1f, 200.0f);
            proj[1][1] *= -1;
            invProj = glm::inverse(proj);
        }
        LightingPushConstants pushConstants = {
            .invView = invView,
            .invProj = invProj,
            .cameraPos = cameraPos,
        };
        vkCmdPushConstants(commandBuffer, lightingShader->pipelineLayout, 
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 
            0, sizeof(pushConstants), &pushConstants);
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    }
    void Renderer::renderComposite(VkCommandBuffer commandBuffer) {
        Shader* compositeShader = shaderManager->getShader("composite");
        if (!compositeShader) {
            return;
        }
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, compositeShader->pipeline);
        VkViewport viewport = {
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>(swapChainExtent.width),
            .height = static_cast<float>(swapChainExtent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        VkRect2D scissor = {
            .offset = {0, 0},
            .extent = swapChainExtent,
        };
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        
        if (!compositeDescriptorSets.empty() && compositeDescriptorSets[currentFrame] != VK_NULL_HANDLE) {
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                compositeShader->pipelineLayout, 0, 1, &compositeDescriptorSets[currentFrame], 0, nullptr);
        }
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    }
    void Renderer::renderUI(VkCommandBuffer commandBuffer){
        Shader* uiShader = getShaderManager()->getShader("ui");
        struct UIPushConstants {
            glm::vec3 color = glm::vec3(1.0f);
            uint32_t isUI = 1;
            glm::mat4 model = glm::mat4(1.0f);
            uint32_t padding[3];
        } pushData;
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, uiShader->pipeline);
        VkDeviceSize offsets[] = {0};
        VkBuffer vb = quadVertexBuffer;
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vb, offsets);
        vkCmdBindIndexBuffer(commandBuffer, quadIndexBuffer, 0, VK_INDEX_TYPE_UINT32);

        const glm::vec2 swapExtentF(static_cast<float>(swapChainExtent.width), static_cast<float>(swapChainExtent.height));
        constexpr glm::vec2 designResolution(800.0f, 600.0f);
        float layoutScale = std::max(uiScale, 0.0001f);
        glm::vec2 canvasSize = designResolution * layoutScale;
        glm::vec2 canvasOrigin = 0.5f * (swapExtentF - canvasSize);

        glm::mat4 pixelToNdc(1.0f);
        pixelToNdc[0][0] = 2.0f / std::max(swapExtentF.x, 1.0f);
        pixelToNdc[1][1] = -2.0f / std::max(swapExtentF.y, 1.0f);
        pixelToNdc[3][0] = -1.0f;
        pixelToNdc[3][1] = 1.0f;

        auto drawUIObject = [&](UIObject* uiObj, const LayoutRect& rect) {
            auto const& initialSets = uiObj->getDescriptorSets();
            const std::vector<VkDescriptorSet>* activeSets = &initialSets;

            if (initialSets.size() != MAX_FRAMES_IN_FLIGHT && !uiObj->getTexture().empty()) {
                uiObj->loadTexture();
                activeSets = &uiObj->getDescriptorSets();
            }

            const auto& sets = *activeSets;
            if (sets.size() != MAX_FRAMES_IN_FLIGHT) return;
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, uiShader->pipelineLayout, 0, 1, &sets[currentFrame], 0, nullptr);
            glm::vec2 center = rect.pos + rect.size * 0.5f;
            glm::mat4 pixelModel(1.0f);
            pixelModel = glm::translate(pixelModel, glm::vec3(center, 0.0f));
            pixelModel = glm::scale(pixelModel, glm::vec3(rect.size * 0.5f, 1.0f));
            pushData.isUI = 1;
            pushData.color = glm::vec3(1.0f);
            pushData.model = pixelToNdc * pixelModel;
            vkCmdPushConstants(commandBuffer, uiShader->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(UIPushConstants), &pushData);
            vkCmdDrawIndexed(commandBuffer, 6, 1, 0, 0, 0);
        };

        auto drawTextObject = [&](TextObject* textObj, const LayoutRect& designRect, const LayoutRect& pixelRect) {
            if(!textObj || !textObj->isEnabled() || textObj->text.empty()) return;
            Font* font = fontManager->getFont(textObj->font);
            if (!font) return;

            pushData.isUI = 0;
            pushData.color = textObj->color;

            const float baseHeight = static_cast<float>(font->lineHeight > 0 ? font->lineHeight : (font->maxGlyphHeight > 0 ? font->maxGlyphHeight : font->fontSize));
            const float designReferenceHeight = 720.0f;
            float defaultDesignHeight = designRect.size.y;
            if (defaultDesignHeight <= 0.0f) {
                float designScaleFactor = designResolution.y / std::max(designReferenceHeight, 1.0f);
                defaultDesignHeight = baseHeight * std::max(designScaleFactor, 0.001f);
            }
            const float defaultPixelHeight = defaultDesignHeight * layoutScale * std::max(textSizeScale, 0.001f);
            float requestedHeightPx = pixelRect.size.y;
            if (requestedHeightPx <= 0.0f) {
                requestedHeightPx = defaultPixelHeight;
            }
            const float pixelScale = requestedHeightPx / std::max(defaultPixelHeight, 1.0f);

            float maxAboveBaseline = 0.0f;
            float maxBelowBaseline = 0.0f;
            float totalAdvance = 0.0f;
            for (char c : textObj->text) {
                auto glyphIt = font->characters.find(c);
                if (glyphIt == font->characters.end()) {
                    continue;
                }
                const Character& glyph = glyphIt->second;
                maxAboveBaseline = std::max(maxAboveBaseline, static_cast<float>(glyph.bearing.y));
                maxBelowBaseline = std::max(maxBelowBaseline, static_cast<float>(glyph.size.y - glyph.bearing.y));
                totalAdvance += static_cast<float>(glyph.advance);
            }
            if (maxAboveBaseline <= 0.0f) {
                maxAboveBaseline = baseHeight * 0.8f;
            }
            if (maxBelowBaseline < 0.0f) {
                maxBelowBaseline = 0.0f;
            }
            const float ascender = static_cast<float>(font->ascent != 0 ? font->ascent : maxAboveBaseline);
            const float actualTextWidthPx = totalAdvance * pixelScale;
            const glm::ivec2 corner = textObj->getCorner();

            float baselineX = pixelRect.pos.x;
            if (corner.x == 2) {
                baselineX = pixelRect.pos.x + pixelRect.size.x - actualTextWidthPx;
            } else if (corner.x == 1) {
                baselineX = pixelRect.pos.x + (pixelRect.size.x - actualTextWidthPx) * 0.5f;
            }

            float baselineY;
            if (corner.y == 2) {
                baselineY = pixelRect.pos.y + pixelRect.size.y - (maxBelowBaseline * pixelScale);
            } else if (corner.y == 1) {
                baselineY = pixelRect.pos.y + (pixelRect.size.y - (maxAboveBaseline + maxBelowBaseline) * pixelScale) * 0.5f;
            } else {
                baselineY = pixelRect.pos.y + ascender * pixelScale;
            }

            float cursor = 0.0f;
            for (char c : textObj->text) {
                auto it = font->characters.find(c);
                if (it == font->characters.end()) continue;
                Character& ch = it->second;
                if (ch.descriptorSets.size() != MAX_FRAMES_IN_FLIGHT) continue;

                const glm::vec2 glyphSizePx = glm::vec2(ch.size) * pixelScale;
                const float xpos = baselineX + (cursor + static_cast<float>(ch.bearing.x)) * pixelScale;
                const float ypos = baselineY - static_cast<float>(ch.size.y - ch.bearing.y) * pixelScale;
                const glm::vec2 glyphTopLeft(xpos, ypos);
                const glm::vec2 glyphCenter = glyphTopLeft + glyphSizePx * 0.5f;

                glm::mat4 glyphModel(1.0f);
                glyphModel = glm::translate(glyphModel, glm::vec3(glyphCenter, 0.0f));
                glyphModel = glm::scale(glyphModel, glm::vec3(glyphSizePx * 0.5f, 1.0f));

                pushData.model = pixelToNdc * glyphModel;
                vkCmdPushConstants(commandBuffer, uiShader->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(UIPushConstants), &pushData);
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, uiShader->pipelineLayout, 0, 1, &ch.descriptorSets[currentFrame], 0, nullptr);
                vkCmdDrawIndexed(commandBuffer, 6, 1, 0, 0, 0);

                cursor += static_cast<float>(ch.advance);
            }
        };

        auto traverse = [&](auto&& self, UIObject* node, const LayoutRect& parentDesignRect) -> void {
            if (!node || !node->isEnabled()) return;

            if (auto* textNode = dynamic_cast<TextObject*>(node)) {
                LayoutRect designRect = resolveDesignRect(textNode, parentDesignRect);
                LayoutRect pixelRect = toPixelRect(designRect, canvasOrigin, layoutScale);
                drawTextObject(textNode, designRect, pixelRect);
                return;
            }

            LayoutRect designRect = resolveDesignRect(node, parentDesignRect);
            LayoutRect pixelRect = toPixelRect(designRect, canvasOrigin, layoutScale);
            drawUIObject(node, pixelRect);
            for (auto& childEntry : node->children) {
                if (UIObject* child = childEntry.second) {
                    self(self, child, designRect);
                }
            }
        };

        LayoutRect rootDesignRect{glm::vec2(0.0f), designResolution};
        for (auto& [name, obj] : uiManager->getUIObjects()) {
            if (obj->getParent() == nullptr) {
                traverse(traverse, obj, rootDesignRect);
            }
        }
    }
    void Renderer::recordDeferredCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
        updateEntities();
        VkCommandBufferBeginInfo beginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = 0,
            .pInheritanceInfo = nullptr,
        };
        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }
        {
            VkClearValue clearValues[4];
            clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};  // Albedo
            clearValues[1].color = {{0.5f, 0.5f, 1.0f, 1.0f}};  // Normal (0,0,1 encoded)
            clearValues[2].color = {{0.0f, 0.5f, 0.0f, 1.0f}};  // Material
            clearValues[3].depthStencil = {1.0f, 0};
            
            VkRenderPassBeginInfo renderPassInfo = {
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                .renderPass = gBufferRenderPass,
                .framebuffer = gBufferFramebuffers[imageIndex],
                .renderArea = {.offset = {0, 0}, .extent = swapChainExtent},
                .clearValueCount = 4,
                .pClearValues = clearValues,
            };
            
            vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
            renderEntitiesGeometry(commandBuffer);
            vkCmdEndRenderPass(commandBuffer);

            transitionGBufferForReading(commandBuffer);
        }
        {
            VkClearValue clearValue;
            clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
            
            VkRenderPassBeginInfo renderPassInfo = {
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                .renderPass = lightingRenderPass,
                .framebuffer = lightingFramebuffers[imageIndex],
                .renderArea = {.offset = {0, 0}, .extent = swapChainExtent},
                .clearValueCount = 1,
                .pClearValues = &clearValue,
            };
            
            vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
            renderDeferredLighting(commandBuffer);
            vkCmdEndRenderPass(commandBuffer);
        }
        {
            ComputeShader* ssrShader = shaderManager->getComputeShader("ssr");
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, ssrShader->pipeline);

            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, ssrShader->pipelineLayout, 0, 1, &ssrDescriptorSets[currentFrame], 0, nullptr);

            SSRPushConstants ssrPushConstants{};
            if (activeCamera) {
                glm::mat4 cameraWorld = activeCamera->getWorldTransform();
                ssrPushConstants.view = glm::inverse(cameraWorld);
                ssrPushConstants.proj = glm::perspective(glm::radians(activeCamera->getFOV()), 
                    static_cast<float>(swapChainExtent.width) / std::max(static_cast<float>(swapChainExtent.height), 1.0f), 
                    0.1f, 200.0f);
                ssrPushConstants.proj[1][1] *= -1;
                ssrPushConstants.invView = cameraWorld;
                ssrPushConstants.invProj = glm::inverse(ssrPushConstants.proj);
            }
            vkCmdPushConstants(commandBuffer, ssrShader->pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(SSRPushConstants), &ssrPushConstants);

            uint32_t groupX = (swapChainExtent.width + 7) / 8;
            uint32_t groupY = (swapChainExtent.height + 7) / 8;
            vkCmdDispatch(commandBuffer, groupX, groupY, 1);
            
            VkMemoryBarrier barrier = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                .pNext = nullptr,
                .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            };
            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
        }
        {
            VkClearValue clearValue;
            clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
            
            VkRenderPassBeginInfo renderPassInfo = {
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                .renderPass = compositeRenderPass,
                .framebuffer = compositeFramebuffers[imageIndex],
                .renderArea = {.offset = {0, 0}, .extent = swapChainExtent},
                .clearValueCount = 1,
                .pClearValues = &clearValue,
            };
            
            vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
            renderComposite(commandBuffer);
            renderUI(commandBuffer);
            vkCmdEndRenderPass(commandBuffer);
        }
        
        vkEndCommandBuffer(commandBuffer);
    }
    void Renderer::createColorResources() {
        VkFormat colorFormat = swapChainImageFormat;
        createImage(swapChainExtent.width, swapChainExtent.height, 1, msaaSamples, colorFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, colorImage, colorImageMemory);
        colorImageView = createImageView(colorImage, colorFormat, 1);
    }
    void Renderer::createDepthResources() {
        VkFormat depthFormat = findDepthFormat();
        createImage(swapChainExtent.width, swapChainExtent.height, 1, msaaSamples, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageMemory);
        depthImageView = createImageView(depthImage, depthFormat, 1);
    }
    void Renderer::createTextureSampler() {
        VkSamplerCreateInfo samplerInfo = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .mipLodBias = 0.0f,
            .anisotropyEnable = VK_TRUE,
            .maxAnisotropy = 16,
            .compareEnable = VK_FALSE,
            .compareOp = VK_COMPARE_OP_ALWAYS,
            .minLod = 0.0f,
            .maxLod = 0.0f,
            .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
            .unnormalizedCoordinates = VK_FALSE,
        };
        if(vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture sampler!");
        }
    }
    void Renderer::createTextureSampler(VkSampler &sampler, uint32_t mipLevels){
        VkSamplerCreateInfo samplerInfo = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .mipLodBias = 0.0f,
            .anisotropyEnable = VK_TRUE,
            .maxAnisotropy = 16,
            .compareEnable = VK_FALSE,
            .compareOp = VK_COMPARE_OP_ALWAYS,
            .minLod = 0.0f,
            .maxLod = static_cast<float>(mipLevels),
            .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
            .unnormalizedCoordinates = VK_FALSE,
        };
        if(vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS)
            throw std::runtime_error("Failed to create texture sampler!");
    }
    void Renderer::setUIMode(bool enabled) {
        uiMode = enabled;
        hoveredObject = nullptr;
        if (uiMode) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            cursorLocked = false;
            activeCamera = nullptr;
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            cursorLocked = true;
        }
        firstMouse = true;
        if (inputManager) {
            inputManager->resetMouseDelta();
        }
    }
    void Renderer::setActiveCamera(Camera* camera) {
        activeCamera = camera;
    }
    int Renderer::rateDeviceSuitability(VkPhysicalDevice device) {
        VkPhysicalDeviceProperties deviceProperties;
        VkPhysicalDeviceFeatures deviceFeatures;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);
        vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
        int score = 0;
        if(deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 1000;
        #ifdef __APPLE__
            score += 500;
        #endif
        score += deviceProperties.limits.maxImageDimension2D;
        return score;
    }
    VkSampleCountFlagBits Renderer::getMaxUsableSampleCount(){
        VkPhysicalDeviceProperties physicalDeviceProperties;
        vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);
        VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
        if(counts & VK_SAMPLE_COUNT_64_BIT) return VK_SAMPLE_COUNT_64_BIT;
        if(counts & VK_SAMPLE_COUNT_32_BIT) return VK_SAMPLE_COUNT_32_BIT;
        if(counts & VK_SAMPLE_COUNT_16_BIT) return VK_SAMPLE_COUNT_16_BIT;
        if(counts & VK_SAMPLE_COUNT_8_BIT) return VK_SAMPLE_COUNT_8_BIT;
        if(counts & VK_SAMPLE_COUNT_4_BIT) return VK_SAMPLE_COUNT_4_BIT;
        if(counts & VK_SAMPLE_COUNT_2_BIT) return VK_SAMPLE_COUNT_2_BIT;
        return VK_SAMPLE_COUNT_1_BIT;
    }
    bool Renderer::checkValidationLayerSupport() {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
        for(const char* layerName : validationLayers) {
            bool layerFound = false;
            for(const auto &layerProperties : availableLayers) {
                if(strcmp(layerName, layerProperties.layerName) == 0) {
                    layerFound = true;
                    break;
                }
            }
            if(!layerFound) return false;
        }
        return true;
    }
    std::vector<const char*> Renderer::getRequiredExtensions() {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
        if(enableValidationLayers) extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        #ifdef __APPLE__
            extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
            extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
        #endif
        return extensions;
    }
    uint32_t Renderer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties){
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
        for(uint32_t i=0; i<memProperties.memoryTypeCount; i++) {
            if(typeFilter & (1 << i) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }
        throw std::runtime_error("Failed to find suitable memory type!");
    }
    void Renderer::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo) {
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = 
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
        |   VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
        |   VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = 
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
        |   VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
        |   VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback;
    }
    QueueFamilyIndices Renderer::findQueueFamilies(VkPhysicalDevice device) {
        QueueFamilyIndices indices;
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
        for(uint32_t i=0; i<queueFamilyCount; i++) {
            if(queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) indices.graphicsFamily = i;
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
            if(presentSupport) indices.presentFamily = i;
            if(indices.isComplete()) break;
        }
        return indices;
    }
    bool Renderer::hasStencilComponent(VkFormat format){
        return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
    }
    bool Renderer::hasDeviceExtension(VkPhysicalDevice dev, const char* extName) {
        uint32_t extensionCount = 0;
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &extensionCount, availableExtensions.data());
        for(const auto& e : availableExtensions) {
            if(std::string(e.extensionName) == extName) return true;
        }
        return false;
    }
    SwapChainSupportDetails Renderer::querySwapChainSupport(VkPhysicalDevice device) {
        SwapChainSupportDetails details;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
        if(formatCount != 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
        }
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
        if(presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
        }
        return details;
    }
    VkSurfaceFormatKHR Renderer::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats) {
        for(const auto &availableFormat : availableFormats) {
            if(availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
            }
        }
        return availableFormats[0];
    }
    VkPresentModeKHR Renderer::chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes) {
        for(const auto &availablePresentMode : availablePresentModes) {
            if(availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return availablePresentMode;
            }
        }
        return VK_PRESENT_MODE_FIFO_KHR;
    }
    VkExtent2D Renderer::chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities) {
        if(capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        } else {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);
            VkExtent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };
            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
            return actualExtent;
        }
    }
    VKAPI_ATTR VkBool32 VKAPI_CALL Renderer::debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData
    ){
        std::cerr<<"Validation layer: "<<pCallbackData->pMessage<<std::endl;
        return VK_FALSE;
    }
    void Renderer::processInput(GLFWwindow* window) {
        auto app = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
        app->inputManager->processInput(window);
        static bool wasPressed = false;
        static double lastClickTime = 0.0;
        static bool escapeWasPressed = false;

        bool escapePressed = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
        if (escapePressed && !escapeWasPressed && app->cursorLocked) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            app->cursorLocked = false;
            app->firstMouse = true;
            app->inputManager->resetMouseDelta();
        }
        escapeWasPressed = escapePressed;

        bool isPressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        double currentTime = glfwGetTime();

        if (!app->cursorLocked && !app->uiMode) {
            if (isPressed && !wasPressed) {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                app->cursorLocked = true;
                app->firstMouse = true;
                app->inputManager->resetMouseDelta();
            }
            wasPressed = isPressed;
            return;
        }

        if(isPressed && !wasPressed) {
            if(currentTime - lastClickTime < 0.2) {
                wasPressed = isPressed;
                return;
            }
            lastClickTime = currentTime;

            if (app->hoveredObject && app->hoveredObject->onClick) {
                app->hoveredObject->onClick();
                app->hoveredObject = nullptr;
            }
        }
        wasPressed = isPressed;
        if(!isPressed) {
            app->firstMouse = true;
        }
    }
    void Renderer::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
        auto app = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
        app->framebufferResized = true;
    }
    void Renderer::mouseMoveCallback(GLFWwindow* window, double xpos, double ypos) {
        auto app = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));

        const glm::vec2 swapExtentF(static_cast<float>(app->swapChainExtent.width), static_cast<float>(app->swapChainExtent.height));
        constexpr glm::vec2 designResolution(800.0f, 600.0f);
        float layoutScale = std::max(app->uiScale, 0.0001f);
        glm::vec2 canvasSize = designResolution * layoutScale;
        glm::vec2 canvasOrigin = 0.5f * (swapExtentF - canvasSize);

        float xscale = 1.0f, yscale = 1.0f;
        glfwGetWindowContentScale(window, &xscale, &yscale);
        glm::vec2 mousePosF(static_cast<float>(xpos) * std::max(xscale, 1.0f), static_cast<float>(ypos) * std::max(yscale, 1.0f));
        mousePosF.y = swapExtentF.y - mousePosF.y;

        bool foundHover = false;
        auto traverse = [&](auto&& self, UIObject* node, const LayoutRect& parentDesignRect) -> void {
            if (!node || !node->isEnabled()) return;

            if (auto* textNode = dynamic_cast<TextObject*>(node)) {
                return;
            }

            LayoutRect designRect = resolveDesignRect(node, parentDesignRect);
            LayoutRect pixelRect = toPixelRect(designRect, canvasOrigin, layoutScale);
            const float invLayout = layoutScale > 0.0f ? (1.0f / layoutScale) : 0.0f;
            glm::vec2 mouseDesign = (mousePosF - canvasOrigin) * invLayout;

            if (auto* buttonNode = dynamic_cast<ButtonObject*>(node)) {
                bool isHovered =
                    mouseDesign.x >= designRect.pos.x && mouseDesign.x <= (designRect.pos.x + designRect.size.x) &&
                    mouseDesign.y >= designRect.pos.y && mouseDesign.y <= (designRect.pos.y + designRect.size.y);
                if (isHovered) {
                    app->hoveredObject = buttonNode;
                    foundHover = true;
                    return;
                }
            }

            for (auto& childEntry : node->children) {
                if (UIObject* child = childEntry.second) {
                    self(self, child, designRect);
                }
            }
        };
        LayoutRect rootDesignRect{glm::vec2(0.0f), designResolution};
        for (auto& [name, obj] : app->uiManager->getUIObjects()) {
            if (obj->getParent() == nullptr) {
                traverse(traverse, obj, rootDesignRect);
            }
        }
        if (!foundHover) {
            app->hoveredObject = nullptr;
        }
    }
