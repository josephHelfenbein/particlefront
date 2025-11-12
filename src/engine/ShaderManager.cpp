#include <ShaderManager.h>
#include <Renderer.h>

ShaderManager::ShaderManager() {
    renderer = Renderer::getInstance();
    std::vector<std::variant<Shader*, ComputeShader*>> defaultShaders = {
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
            .enableDepth = true,
            .useTextVertex = false,
            .cullMode = VK_CULL_MODE_BACK_BIT,
            .depthWrite = true,
            .depthCompare = VK_COMPARE_OP_LESS,
            .renderPassToUse = Renderer::getInstance()->getGBufferRenderPass(),
            .colorAttachmentCount = 3,
            .noVertexInput = false,
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
            .fragmentBitBindings = 5,
            .enableDepth = false,
            .useTextVertex = true,
            .cullMode = VK_CULL_MODE_BACK_BIT,
            .depthWrite = false,
            .depthCompare = VK_COMPARE_OP_LESS,
            .renderPassToUse = Renderer::getInstance()->getLightingRenderPass(),
            .colorAttachmentCount = 1,
            .noVertexInput = true,
        },
        new Shader{
            .name = "composite",
            .vertexPath = "src/assets/shaders/compiled/composite.vert.spv",
            .fragmentPath = "src/assets/shaders/compiled/composite.frag.spv",
            .pushConstantRange = {},
            .poolMultiplier = 4,
            .vertexBitBindings = 0,
            .fragmentBitBindings = 2,
            .enableDepth = false,
            .useTextVertex = true,
            .cullMode = VK_CULL_MODE_BACK_BIT,
            .depthWrite = false,
            .depthCompare = VK_COMPARE_OP_LESS,
            .renderPassToUse = Renderer::getInstance()->getCompositeRenderPass(),
            .colorAttachmentCount = 1,
            .noVertexInput = true,
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
            .enableDepth = false,
            .useTextVertex = true,
            .cullMode = VK_CULL_MODE_BACK_BIT,
            .depthWrite = false,
            .depthCompare = VK_COMPARE_OP_LESS,
            .renderPassToUse = Renderer::getInstance()->getCompositeRenderPass(),
            .colorAttachmentCount = 1,
            .noVertexInput = false,
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
            .enableDepth = true,
            .useTextVertex = false,
            .cullMode = VK_CULL_MODE_NONE,
            .depthWrite = false,
            .depthCompare = VK_COMPARE_OP_LESS_OR_EQUAL,
            .renderPassToUse = Renderer::getInstance()->getGBufferRenderPass(),
            .colorAttachmentCount = 3,
            .noVertexInput = false,
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
        new Shader{
            .name = "shadowmap",
            .vertexPath = "src/assets/shaders/compiled/shadowmap.vert.spv",
            .fragmentPath = "src/assets/shaders/compiled/shadowmap.frag.spv",
            .pushConstantRange = {
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                .offset = 0,
                .size = sizeof(ShadowMapPushConstants),
            },
            .poolMultiplier = 64,
            .vertexBitBindings = 0,
            .fragmentBitBindings = 0,
            .enableDepth = true,
            .useTextVertex = false,
            .cullMode = VK_CULL_MODE_NONE,
            .depthWrite = true,
            .depthCompare = VK_COMPARE_OP_LESS,
            .renderPassToUse = Renderer::getInstance()->getShadowMapRenderPass(),
            .colorAttachmentCount = 0,
            .noVertexInput = false,
        },
    };
    for (auto& shader : defaultShaders) {
        if (std::holds_alternative<Shader*>(shader)) {
            Shader* s = std::get<Shader*>(shader);
            loadShader(s);
            delete s;
        } else if (std::holds_alternative<ComputeShader*>(shader)) {
            ComputeShader* cs = std::get<ComputeShader*>(shader);
            loadShader(cs);
            delete cs;
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
void ShaderManager::loadShader(Shader* shader) {
    std::vector<uint32_t> fragmentDescriptorCounts;
    const std::vector<uint32_t>* fragmentDescriptorCountsPtr = nullptr;
    if (shader->name == "lighting") {
        fragmentDescriptorCounts = {1u, 1u, 1u, 1u, kMaxPointLights};
        fragmentDescriptorCountsPtr = &fragmentDescriptorCounts;
    }
    renderer->createDescriptorSetLayout(shader->vertexBitBindings, shader->fragmentBitBindings, shader->descriptorSetLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, fragmentDescriptorCountsPtr);
    VkPushConstantRange* pPCR = (shader->pushConstantRange.size > 0) ? &shader->pushConstantRange : nullptr;
    
    const VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;

    renderer->createGraphicsPipeline(shader->vertexPath, shader->fragmentPath, shader->pipeline, shader->pipelineLayout, shader->descriptorSetLayout, pPCR, shader->enableDepth, shader->useTextVertex, shader->cullMode, frontFace, shader->depthWrite, shader->depthCompare, shader->renderPassToUse, shader->colorAttachmentCount, sampleCount, shader->noVertexInput);
    renderer->createDescriptorPool(shader->vertexBitBindings, shader->fragmentBitBindings, shader->descriptorPool, shader->poolMultiplier, false, fragmentDescriptorCountsPtr);
    shaders[shader->name] = *shader;
}
void ShaderManager::loadShader(ComputeShader* shader) {
    int samplerCount = shader->computeBitBindings - shader->storageImageCount;

    renderer->createDescriptorSetLayout(shader->storageImageCount, samplerCount, shader->descriptorSetLayout, VK_SHADER_STAGE_COMPUTE_BIT);
    VkPushConstantRange* pPCR = (shader->pushConstantRange.size > 0) ? &shader->pushConstantRange : nullptr;
    renderer->createComputePipeline(shader->computePath, shader->pipeline, shader->pipelineLayout, shader->descriptorSetLayout, pPCR);
    renderer->createDescriptorPool(shader->storageImageCount, samplerCount, shader->descriptorPool, shader->poolMultiplier, true);
    shaders[shader->name] = *shader;
}
ShaderManager* ShaderManager::getInstance() {
    static ShaderManager instance;
    return &instance;
}