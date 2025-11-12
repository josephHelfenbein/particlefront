#include <Entity.h>
#include <EntityManager.h>
#include <Model.h>

AABB Entity::getWorldBounds(const glm::mat4& worldTransform) const {
    Model* model = getModel();
    if (!model) {
        glm::vec3 worldPos = glm::vec3(worldTransform * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        return AABB{worldPos, worldPos};
    }

    glm::vec3 modelMax = model->getBoundsMax();
    glm::vec3 modelMin = model->getBoundsMin();

    glm::vec3 corners[8] = {
        glm::vec3(modelMin.x, modelMin.y, modelMin.z),
        glm::vec3(modelMax.x, modelMin.y, modelMin.z),
        glm::vec3(modelMin.x, modelMax.y, modelMin.z),
        glm::vec3(modelMax.x, modelMax.y, modelMin.z),
        glm::vec3(modelMin.x, modelMin.y, modelMax.z),
        glm::vec3(modelMax.x, modelMin.y, modelMax.z),
        glm::vec3(modelMin.x, modelMax.y, modelMax.z),
        glm::vec3(modelMax.x, modelMax.y, modelMax.z)
    };

    AABB worldAABB;
    worldAABB.min = glm::vec3(FLT_MAX, FLT_MAX, FLT_MAX);
    worldAABB.max = glm::vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    for(int i=0; i<8; i++) {
        glm::vec3 corner = glm::vec3(worldTransform * glm::vec4(corners[i], 1.0f));
        worldAABB.min = glm::min(worldAABB.min, corner);
        worldAABB.max = glm::max(worldAABB.max, corner);
    }
    return worldAABB;
}

void Entity::addChild(Entity* child) {
    if (child->parent) {
        child->parent->removeChild(child);
    }
    children.push_back(child);
    child->parent = this;
}

void Entity::removeChild(Entity* child) {
    children.erase(std::remove(children.begin(), children.end(), child), children.end());
    child->parent = nullptr;
}

void Entity::moveToParent(Entity* newParent) {
    if (parent) {
        parent->removeChild(this);
    }
    if (newParent) {
        newParent->addChild(this);
    }
}

Entity* Entity::getChild(const std::string& name) {
    for (auto* child : children) {
        if (child->getName() == name) {
            return child;
        }
    }
    return nullptr;
}

glm::vec3 Entity::getWorldPosition() { return worldPosition; }

glm::vec3 Entity::getWorldRotation() { return worldRotation; }

glm::vec3 Entity::getWorldScale() { return worldScale; }

glm::mat4 Entity::getWorldTransform() { return worldTransform; }

void Entity::updateWorldTransform() {
    glm::mat4 transform(1.0f);
    std::vector<Entity*> hierarchy;
    for (Entity* current = this; current != nullptr; current = current->getParent()) {
        hierarchy.push_back(current);
    }

    for (int i = static_cast<int>(hierarchy.size()) - 1; i >= 0; --i) {
        Entity* current = hierarchy[i];
        transform = glm::translate(transform, current->getPosition());
        glm::vec3 rot = current->getRotation();
        transform = glm::rotate(transform, glm::radians(rot.x), glm::vec3(1.0f, 0.0f, 0.0f));
        transform = glm::rotate(transform, glm::radians(rot.y), glm::vec3(0.0f, 1.0f, 0.0f));
        transform = glm::rotate(transform, glm::radians(rot.z), glm::vec3(0.0f, 0.0f, 1.0f));
        transform = glm::scale(transform, current->getScale());
    }
    
    worldTransform = transform;

    worldPosition = glm::vec3(transform[3]);
    
    glm::vec3 scaleX = glm::vec3(transform[0]);
    glm::vec3 scaleY = glm::vec3(transform[1]);
    glm::vec3 scaleZ = glm::vec3(transform[2]);
    worldScale = glm::vec3(glm::length(scaleX), glm::length(scaleY), glm::length(scaleZ));
    
    glm::mat3 rotationMatrix;
    rotationMatrix[0] = scaleX / worldScale.x;
    rotationMatrix[1] = scaleY / worldScale.y;
    rotationMatrix[2] = scaleZ / worldScale.z;
    
    worldRotation.x = glm::degrees(std::atan2(rotationMatrix[1][2], rotationMatrix[2][2]));
    worldRotation.y = glm::degrees(std::atan2(-rotationMatrix[0][2], std::sqrt(rotationMatrix[1][2] * rotationMatrix[1][2] + rotationMatrix[2][2] * rotationMatrix[2][2])));
    worldRotation.z = glm::degrees(std::atan2(rotationMatrix[0][1], rotationMatrix[0][0]));
}

void Entity::loadTextures() {
    if (shader == "") {
        return;
    }
    TextureManager* texMgr = TextureManager::getInstance();
    Renderer* renderer = Renderer::getInstance();
    ShaderManager* shaderMgr = renderer ? renderer->getShaderManager() : nullptr;
    Shader* shaderUsed = shaderMgr ? shaderMgr->getShader(shader) : nullptr;
    if (!shaderUsed) {
        std::cerr << "Shader " << shader << " not found!" << std::endl;
        return;
    }
    const int fragmentBindingCount = std::max(shaderUsed->fragmentBitBindings, 0);
    std::vector<Image*> textureResources;
    if (texMgr && fragmentBindingCount > 0) {
        if (shader == "gbuffer") {
            static const std::array<std::string, 4> defaultTextures = {
                "materials_default_albedo", "materials_default_metallic", "materials_default_roughness", "materials_default_normal"
            };
            const int maxSlots = static_cast<int>(std::min<std::size_t>(defaultTextures.size(), static_cast<size_t>(fragmentBindingCount)));
            for (int i = 0; i < maxSlots; ++i) {
                const std::string& preferred = (i < static_cast<int>(textures.size())) ? textures[i] : defaultTextures[i];
                Image* img = texMgr->getTexture(preferred);
                if (!img) {
                    img = texMgr->getTexture(defaultTextures[i]);
                }
                if (img) {
                    textureResources.push_back(img);
                }
            }
            for (int i = static_cast<int>(textureResources.size()); i < fragmentBindingCount && i < maxSlots; ++i) {
                Image* fallback = texMgr->getTexture(defaultTextures[i]);
                if (!fallback) break;
                textureResources.push_back(fallback);
            }
        } else {
            for (const auto& textureName : textures) {
                Image* img = texMgr->getTexture(textureName);
                if (!img) {
                    std::cerr << "Texture " << textureName << " not found!" << std::endl;
                } else {
                    textureResources.push_back(img);
                }
            }
        }
        if (textureResources.size() < static_cast<size_t>(fragmentBindingCount)) {
            Image* fallback = texMgr->getTexture("default_albedo");
            while (fallback && textureResources.size() < static_cast<size_t>(fragmentBindingCount)) {
                textureResources.push_back(fallback);
            }
        }
    }
    if (fragmentBindingCount > 0 && textureResources.size() < static_cast<size_t>(fragmentBindingCount)) {
        std::cerr << "Insufficient textures provided for shader " << shader << std::endl;
        descriptorSets.clear();
        return;
    }

    ensureUniformBuffers(renderer, shaderUsed->vertexBitBindings);

    descriptorSets = renderer->createDescriptorSets(
        shaderUsed->descriptorPool,
        shaderUsed->descriptorSetLayout,
        shaderUsed->vertexBitBindings,
        shaderUsed->fragmentBitBindings,
        textureResources,
        uniformBuffers
    );
}

void Entity::updateUniformBuffer(uint32_t frameIndex, const UniformBufferObject& ubo) {
    if (uniformBufferStride == 0) return;
    const size_t bufferIndex = static_cast<size_t>(frameIndex) * uniformBufferStride;
    if (bufferIndex >= uniformBuffers.size()) return;
    Renderer* renderer = Renderer::getInstance();
    if (!renderer) return;
    VkDevice device = renderer->getDevice();
    if (device == VK_NULL_HANDLE) return;
    VkDeviceMemory memory = uniformBuffersMemory[bufferIndex];
    VkBuffer buffer = uniformBuffers[bufferIndex];
    if (memory == VK_NULL_HANDLE || buffer == VK_NULL_HANDLE) return;
    void* mapped = nullptr;
    if (vkMapMemory(device, memory, 0, sizeof(UniformBufferObject), 0, &mapped) != VK_SUCCESS || !mapped) {
        return;
    }
    std::memcpy(mapped, &ubo, sizeof(UniformBufferObject));
    vkUnmapMemory(device, memory);
}

void Entity::ensureUniformBuffers(Renderer* renderer, int vertexBindings) {
    if (!renderer) return;
    if (vertexBindings <= 0) {
        destroyUniformBuffers();
        return;
    }
    const size_t frames = static_cast<size_t>(renderer->getFramesInFlight());
    const size_t requiredStride = static_cast<size_t>(vertexBindings);
    if (requiredStride == 0 || frames == 0) {
        destroyUniformBuffers();
        return;
    }
    if (uniformBufferStride == requiredStride && uniformBuffers.size() == frames * requiredStride) {
        return;
    }
    destroyUniformBuffers();
    uniformBufferStride = requiredStride;
    uniformBuffers.resize(frames * requiredStride, VK_NULL_HANDLE);
    uniformBuffersMemory.resize(frames * requiredStride, VK_NULL_HANDLE);
    for (size_t frame = 0; frame < frames; ++frame) {
        for (size_t binding = 0; binding < requiredStride; ++binding) {
            const size_t index = frame * requiredStride + binding;
            renderer->createBuffer(
                sizeof(UniformBufferObject),
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                uniformBuffers[index],
                uniformBuffersMemory[index]
            );
        }
    }
}

void Entity::destroyUniformBuffers() {
    Renderer* renderer = Renderer::getInstance();
    if (!renderer) {
        uniformBuffers.clear();
        uniformBuffersMemory.clear();
        uniformBufferStride = 0;
        return;
    }
    VkDevice device = renderer->getDevice();
    if (device == VK_NULL_HANDLE) {
        uniformBuffers.clear();
        uniformBuffersMemory.clear();
        uniformBufferStride = 0;
        return;
    }
    for (size_t i = 0; i < uniformBuffers.size(); ++i) {
        if (uniformBuffers[i] != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, uniformBuffers[i], nullptr);
            uniformBuffers[i] = VK_NULL_HANDLE;
        }
        if (i < uniformBuffersMemory.size() && uniformBuffersMemory[i] != VK_NULL_HANDLE) {
            vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
            uniformBuffersMemory[i] = VK_NULL_HANDLE;
        }
    }
    uniformBuffers.clear();
    uniformBuffersMemory.clear();
    uniformBufferStride = 0;
}