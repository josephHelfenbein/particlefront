#include <ShaderManager.h>
#include <Renderer.h>

ShaderManager::ShaderManager(std::vector<std::variant<Shader*, ComputeShader*>>& shaders) {
    renderer = Renderer::getInstance();
    for (auto& shader : shaders) {
        if (std::holds_alternative<Shader*>(shader)) {
            Shader* s = std::get<Shader*>(shader);
            loadShader(s->name, s->vertexPath, s->fragmentPath, s->vertexBitBindings, s->fragmentBitBindings, s->pushConstantRange, s->poolMultiplier);
        } else if (std::holds_alternative<ComputeShader*>(shader)) {
            ComputeShader* cs = std::get<ComputeShader*>(shader);
            loadShader(cs->name, cs->computePath, cs->computeBitBindings, cs->storageImageCount, cs->pushConstantRange, cs->poolMultiplier);
        }
    }
}
ShaderManager::~ShaderManager() {
    shutdown();
}
Shader* ShaderManager::getShader(const std::string& name) {
    if (shaders.find(name) == shaders.end() || !std::holds_alternative<Shader>(shaders[name])) {
        return nullptr;
    }
    return &std::get<Shader>(shaders[name]);
}
ComputeShader* ShaderManager::getComputeShader(const std::string& name) {
    if (shaders.find(name) == shaders.end() || !std::holds_alternative<ComputeShader>(shaders[name])) {
        return nullptr;
    }
    return &std::get<ComputeShader>(shaders[name]);
}
void ShaderManager::shutdown() {
    if (!renderer || renderer->device == VK_NULL_HANDLE) {
        shaders.clear();
        return;
    }
    for (auto& [name, shader] : shaders) {
        if (std::holds_alternative<Shader>(shader)) {
            auto& s = std::get<Shader>(shader);
            if (s.pipeline) {
                vkDestroyPipeline(renderer->device, s.pipeline, nullptr);
                s.pipeline = VK_NULL_HANDLE;
            }
            if (s.pipelineLayout) {
                vkDestroyPipelineLayout(renderer->device, s.pipelineLayout, nullptr);
                s.pipelineLayout = VK_NULL_HANDLE;
            }
            if (s.descriptorSetLayout) {
                vkDestroyDescriptorSetLayout(renderer->device, s.descriptorSetLayout, nullptr);
                s.descriptorSetLayout = VK_NULL_HANDLE;
            }
            if (s.descriptorPool) {
                vkDestroyDescriptorPool(renderer->device, s.descriptorPool, nullptr);
                s.descriptorPool = VK_NULL_HANDLE;
            }
        } else if (std::holds_alternative<ComputeShader>(shader)) {
            auto& cs = std::get<ComputeShader>(shader);
            if (cs.pipeline) {
                vkDestroyPipeline(renderer->device, cs.pipeline, nullptr);
                cs.pipeline = VK_NULL_HANDLE;
            }
            if (cs.pipelineLayout) {
                vkDestroyPipelineLayout(renderer->device, cs.pipelineLayout, nullptr);
                cs.pipelineLayout = VK_NULL_HANDLE;
            }
            if (cs.descriptorSetLayout) {
                vkDestroyDescriptorSetLayout(renderer->device, cs.descriptorSetLayout, nullptr);
                cs.descriptorSetLayout = VK_NULL_HANDLE;
            }
            if (cs.descriptorPool) {
                vkDestroyDescriptorPool(renderer->device, cs.descriptorPool, nullptr);
                cs.descriptorPool = VK_NULL_HANDLE;
            }
        }
    }
    shaders.clear();
}
void ShaderManager::loadShader(const std::string& name, const std::string& vertexPath, const std::string& fragmentPath, int vertexBitBindings, int fragmentBitBindings, VkPushConstantRange pushConstantRange, int poolMultiplier) {
    Shader shader = {
        .name = name,
        .vertexPath = vertexPath,
        .fragmentPath = fragmentPath,
        .pushConstantRange = pushConstantRange,
        .poolMultiplier = poolMultiplier,
        .vertexBitBindings = vertexBitBindings,
        .fragmentBitBindings = fragmentBitBindings,
    };

    renderer->createDescriptorSetLayout(shader.vertexBitBindings, shader.fragmentBitBindings, shader.descriptorSetLayout);
    VkPushConstantRange* pPCR = (shader.pushConstantRange.size > 0) ? &shader.pushConstantRange : nullptr;
    const bool isUI = (name == "ui");
    const bool isSkybox = (name == "skybox");
    const bool isGBuffer = (name == "gbuffer");
    const bool isDeferredLighting = (name == "lighting");
    const bool isComposite = (name == "composite");
    const bool enableDepth = !isUI && !isDeferredLighting && !isComposite;
    const bool useTextVertex = isUI || isDeferredLighting || isComposite;
    const VkCullModeFlags cullMode = isSkybox ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;
    const bool depthWrite = isSkybox ? false : enableDepth;
    const VkCompareOp depthCompare = isSkybox ? VK_COMPARE_OP_LESS_OR_EQUAL : VK_COMPARE_OP_LESS;
    const VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    VkRenderPass renderPassToUse = VK_NULL_HANDLE;
    uint32_t colorAttachmentCount = 1;
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;
    bool noVertexInput = false;
    
    if (isGBuffer) {
        renderPassToUse = renderer->getGBufferRenderPass();
        colorAttachmentCount = 3;
    } else if (isDeferredLighting) {
        renderPassToUse = renderer->getLightingRenderPass();
        noVertexInput = true;
    } else if (isComposite) {
        renderPassToUse = renderer->getCompositeRenderPass();
        noVertexInput = true;
    } else if (isUI) {
        renderPassToUse = renderer->getCompositeRenderPass();
    } else if (isSkybox) {
        renderPassToUse = renderer->getGBufferRenderPass();
        colorAttachmentCount = 3;
    }
    renderer->createGraphicsPipeline(shader.vertexPath, shader.fragmentPath, shader.pipeline, shader.pipelineLayout, shader.descriptorSetLayout, pPCR, enableDepth, useTextVertex, cullMode, frontFace, depthWrite, depthCompare, renderPassToUse, colorAttachmentCount, sampleCount, noVertexInput);
    renderer->createDescriptorPool(shader.vertexBitBindings, shader.fragmentBitBindings, shader.descriptorPool, shader.poolMultiplier);
    shaders[name] = shader;
}
void ShaderManager::loadShader(const std::string& name, const std::string& computePath, int computeBitBindings, int storageImageCount, VkPushConstantRange pushConstantRange, int poolMultiplier) {
    ComputeShader computeShader = {
        .name = name,
        .computePath = computePath,
        .pushConstantRange = pushConstantRange,
        .poolMultiplier = poolMultiplier,
        .computeBitBindings = computeBitBindings,
    };

    int samplerCount = computeBitBindings - storageImageCount;
    
    renderer->createDescriptorSetLayout(storageImageCount, samplerCount, computeShader.descriptorSetLayout, VK_SHADER_STAGE_COMPUTE_BIT);
    VkPushConstantRange* pPCR = (computeShader.pushConstantRange.size > 0) ? &computeShader.pushConstantRange : nullptr;
    renderer->createComputePipeline(computeShader.computePath, computeShader.pipeline, computeShader.pipelineLayout, computeShader.descriptorSetLayout, pPCR);
    renderer->createDescriptorPool(storageImageCount, samplerCount, computeShader.descriptorPool, computeShader.poolMultiplier, true);
    shaders[name] = computeShader;
}
ShaderManager* ShaderManager::getInstance() {
    static std::vector<std::variant<Shader*, ComputeShader*>> defaultShaders = {
        new Shader{
            .name = "gbuffer",
            .vertexPath = "src/assets/shaders/compiled/gbuffer.vert.spv",
            .fragmentPath = "src/assets/shaders/compiled/gbuffer.frag.spv",
            .pushConstantRange = {
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                .offset = 0,
                .size = sizeof(UniformBufferObject),
            },
            .poolMultiplier = 256,
            .vertexBitBindings = 1,
            .fragmentBitBindings = 4,
        },
        new Shader{
            .name = "lighting",
            .vertexPath = "src/assets/shaders/compiled/lighting.vert.spv",
            .fragmentPath = "src/assets/shaders/compiled/lighting.frag.spv",
            .pushConstantRange = {
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                .offset = 0,
                .size = sizeof(LightingPushConstants),
            },
            .poolMultiplier = 4,
            .vertexBitBindings = 1,
            .fragmentBitBindings = 4,
        },
        new Shader{
            .name = "composite",
            .vertexPath = "src/assets/shaders/compiled/composite.vert.spv",
            .fragmentPath = "src/assets/shaders/compiled/composite.frag.spv",
            .pushConstantRange = {},
            .poolMultiplier = 4,
            .vertexBitBindings = 0,
            .fragmentBitBindings = 2,
        },
        new Shader{
            .name = "ui",
            .vertexPath = "src/assets/shaders/compiled/ui.vert.spv",
            .fragmentPath = "src/assets/shaders/compiled/ui.frag.spv",
            .pushConstantRange = {
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                .offset = 0,
                .size = sizeof(UIPushConstants),
            },
            .poolMultiplier = 256,
            .vertexBitBindings = 0,
            .fragmentBitBindings = 1,
        },
        new Shader{
            .name = "skybox",
            .vertexPath = "src/assets/shaders/compiled/skybox.vert.spv",
            .fragmentPath = "src/assets/shaders/compiled/skybox.frag.spv",
            .pushConstantRange = {
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                .offset = 0,
                .size = sizeof(UniformBufferObject),
            },
            .poolMultiplier = 64,
            .vertexBitBindings = 1,
            .fragmentBitBindings = 1,
        },
        new ComputeShader{
            .name = "ssr",
            .computePath = "src/assets/shaders/compiled/ssr.comp.spv",
            .pushConstantRange = {
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                .offset = 0,
                .size = sizeof(SSRPushConstants),
            },
            .poolMultiplier = 4,
            .computeBitBindings = 4,
            .storageImageCount = 1,
        },
    };
    static ShaderManager instance(defaultShaders);
    return &instance;
}