#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <optional>
#include <cstdint>

struct GLFWwindow;
class UIManager;
class FontManager;
class SceneManager;
class EntityManager;
class ModelManager;
class ShaderManager;
class TextureManager;
class InputManager;
class ButtonObject;
class Camera;
class Image;
struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class Renderer {
public:
    static constexpr uint32_t kMaxFramesInFlight = 2;
    VkDevice device;

    Renderer();
    ~Renderer();
    static Renderer* getInstance();

    void run();
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t layerCount = 1, VkDeviceSize layerSize = 0);
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels = 1, uint32_t layerCount = 1);
    void createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory, uint32_t arrayLayers = 1, VkImageCreateFlags flags = 0);
    void createTextureImage(int width, int height, unsigned char* imageBuffer, VkImage& textureImage, VkDeviceMemory& textureImageMemory, VkFormat format);
    VkShaderModule createShaderModule(const std::vector<char>& code);
    void createTextureImageView(VkFormat textureFormat, VkImage textureImage, VkImageView &textureImageView);
    VkImageView createImageView(VkImage image, VkFormat format, uint32_t mipLevels, VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT, VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D, uint32_t layerCount = 1);
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    void createDescriptorSetLayout(int vertexBitBindings, int fragmentBitBindings, VkDescriptorSetLayout& descriptorSetLayout, VkShaderStageFlags shaderStage = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    void createDescriptorPool(int vertexBitBindings, int fragmentBitBindings, VkDescriptorPool &descriptorPool, int multiplier = 1, bool isCompute = false);
    std::vector<VkDescriptorSet> createDescriptorSets(VkDescriptorPool pool, VkDescriptorSetLayout& descriptorSetLayout, int vertexBindingCount, int fragmentBindingCount, std::vector<Image*>& textures, std::vector<VkBuffer>& uniformBuffers);
    void createGraphicsPipeline(const std::string& vertexShaderPath, const std::string& fragmentShaderPath, VkPipeline& pipeline, VkPipelineLayout& pipelineLayout, VkDescriptorSetLayout& descriptorSetLayout, VkPushConstantRange* pushConstantRange = nullptr, bool enableDepth = true, bool useTextVertex = false, VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT, VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE, bool depthWrite = true, VkCompareOp depthCompare = VK_COMPARE_OP_LESS, VkRenderPass renderPassOverride = VK_NULL_HANDLE, uint32_t colorAttachmentCount = 1, VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT, bool noVertexInput = false);
    void createComputePipeline(const std::string& computeShaderPath, VkPipeline& pipeline, VkPipelineLayout& pipelineLayout, VkDescriptorSetLayout& descriptorSetLayout, VkPushConstantRange* pushConstantRange = nullptr);
    void createCommandBuffers();
    void createSyncObjects();
    void setUIMode(bool enabled);
    void setActiveCamera(Camera* camera);
    Camera* getActiveCamera() const { return activeCamera; }
    void createTextureSampler();
    void createTextureSampler(VkSampler &sampler, uint32_t mipLevels = 1);

    VkDevice getDevice() const { return device; }
    VkRenderPass getGBufferRenderPass() const { return gBufferRenderPass; }
    VkRenderPass getLightingRenderPass() const { return lightingRenderPass; }
    VkRenderPass getCompositeRenderPass() const { return compositeRenderPass; }
    ShaderManager* getShaderManager() const;
    uint32_t getFramesInFlight() const { return kMaxFramesInFlight; }
    bool isCursorLocked() const { return cursorLocked; }
    bool isUIMode() const { return uiMode; }

private:
    void initWindow();
    void cleanup();
    void initVulkan();
    void mainLoop();
    void drawFrame();

    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapChain();
    void recreateSwapChain();
    void cleanupSwapChain();
    void createImageViews();
    void createGBufferResources();
    void createGBufferRenderPass();
    void createGBufferFramebuffers();
    void createGBufferSampler();
    void createLightingResources();
    void createLightingRenderPass();
    void createLightingFramebuffers();
    void createSSRResources();
    void createCompositeRenderPass();
    void createCompositeFramebuffers();
    void createDeferredDescriptorSets();
    void recreateDeferredDescriptorSets();
    VkFormat findDepthFormat();
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
    void createCommandPool();
    void createQuadBuffers();
    void setupUI();
    void renderUI(VkCommandBuffer commandBuffer);
    void updateEntities();
    void renderEntitiesGeometry(VkCommandBuffer commandBuffer);
    void transitionGBufferForReading(VkCommandBuffer commandBuffer);
    void renderDeferredLighting(VkCommandBuffer commandBuffer);
    void renderComposite(VkCommandBuffer commandBuffer);
    void recordDeferredCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex);
    void createColorResources();
    void createDepthResources();
    int rateDeviceSuitability(VkPhysicalDevice device);
    VkSampleCountFlagBits getMaxUsableSampleCount();
    bool checkValidationLayerSupport();
    std::vector<const char*> getRequiredExtensions();
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo);
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
    bool hasStencilComponent(VkFormat format);
    bool hasDeviceExtension(VkPhysicalDevice dev, const char* extName);
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);
    static void processInput(GLFWwindow* window);
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
    static void mouseMoveCallback(GLFWwindow* window, double xpos, double ypos);

    GLFWwindow* window = nullptr;
    VkInstance instance{};
    VkDebugUtilsMessengerEXT debugMessenger{};
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkQueue graphicsQueue{};
    VkQueue presentQueue{};
    VkSurfaceKHR surface{};
    VkSwapchainKHR swapChain{};
    std::vector<VkImage> swapChainImages{};
    std::vector<VkImageView> swapChainImageViews{};
    std::vector<VkDeviceMemory> swapChainImageMemory{};
    VkImage gBufferAlbedoImage{};
    VkDeviceMemory gBufferAlbedoMemory{};
    VkImageView gBufferAlbedoView{};
    VkImage gBufferNormalImage{};
    VkDeviceMemory gBufferNormalMemory{};
    VkImageView gBufferNormalView{};
    VkImage gBufferMaterialImage{};
    VkDeviceMemory gBufferMaterialMemory{};
    VkImageView gBufferMaterialView{};
    VkImage gBufferDepthImage{};
    VkDeviceMemory gBufferDepthMemory{};
    VkImageView gBufferDepthView{};
    VkImage lightingImage{};
    VkDeviceMemory lightingMemory{};
    VkImageView lightingView{};
    VkImage ssrImage{};
    VkDeviceMemory ssrMemory{};
    VkImageView ssrView{};
    std::vector<VkDescriptorSet> ssrDescriptorSets{};
    std::vector<VkDescriptorSet> lightingDescriptorSets{};
    std::vector<VkDescriptorSet> compositeDescriptorSets{};
    std::vector<VkFramebuffer> gBufferFramebuffers;
    std::vector<VkFramebuffer> lightingFramebuffers;
    std::vector<VkFramebuffer> compositeFramebuffers;
    std::vector<VkFramebuffer> swapChainFramebuffers;
    VkRenderPass gBufferRenderPass{};
    VkRenderPass lightingRenderPass{};
    VkRenderPass compositeRenderPass{};
    VkFormat swapChainImageFormat{};
    VkExtent2D swapChainExtent{};
    VkPipelineLayout pipelineLayout{};
    VkPipeline graphicsPipeline{};
    VkCommandPool commandPool{};
    VkSampler textureSampler{};
    VkSampler gBufferSampler{};
    std::vector<VkCommandBuffer> commandBuffers;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    VkImage colorImage{};
    VkDeviceMemory colorImageMemory{};
    VkImageView colorImageView{};
    VkImage depthImage{};
    VkDeviceMemory depthImageMemory{};
    VkImageView depthImageView{};
    VkBuffer quadVertexBuffer{};
    VkDeviceMemory quadVertexBufferMemory{};
    VkBuffer quadIndexBuffer{};
    VkDeviceMemory quadIndexBufferMemory{};
    UIManager* uiManager = nullptr;
    FontManager* fontManager = nullptr;
    VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
    ShaderManager* shaderManager = nullptr;
    SceneManager* sceneManager = nullptr;
    TextureManager* textureManager = nullptr;
    ModelManager* modelManager = nullptr;
    EntityManager* entityManager = nullptr;
    InputManager* inputManager = nullptr;
    ButtonObject* hoveredObject = nullptr;
    bool framebufferResized = false;
    bool firstMouse = true;
    bool cursorLocked = false;
    bool uiMode = true;
    float uiScale = 1.0f;
    float textScale = 1.0f;
    float textSizeScale = 1.0f;
    float deltaTime = 0.0f;
    float currentTime = 0.0f;
    Camera* activeCamera = nullptr;
};
