#pragma once
#include <vector>
#include <array>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <glm/glm.hpp>
#include <string>
#include <TextureManager.h>
#include <ShaderManager.h>
#include <Renderer.h>
#include <Frustrum.h>
#include <cfloat>

class Model;

class Entity {
public:
    Entity(std::string name, std::string shader, glm::vec3 position, glm::vec3 rotation, glm::vec3 scale = glm::vec3(1.0f), std::vector<std::string> textures = {}) : name(std::move(name)), shader(std::move(shader)), position(position), rotation(rotation), scale(scale), textures(std::move(textures)) {
        loadTextures();
        updateWorldTransform();
    }
    ~Entity() {
        destroyUniformBuffers();
        for (auto& child : children) {
            delete child;
        }
        children.clear();
    }
    virtual void update(float deltaTime) {}

    void addChild(Entity* child);
    void removeChild(Entity* child);
    void moveToParent(Entity* newParent);

    glm::vec3 getPosition() const { return position; }
    void setPosition(const glm::vec3& pos) { position = pos; }
    glm::vec3 getRotation() const { return rotation; }
    void setRotation(const glm::vec3& rot) { rotation = rot; }
    glm::vec3 getScale() const { return scale; }
    void setScale(const glm::vec3& s) { scale = s; }
    std::string getName() const { return name; }
    std::string getShader() const { return shader; }
    bool isActive() const { return active; }
    void setActive(bool state) { active = state; }
    Model* getModel() const { return model; }
    void setModel(Model* m) { model = m; }
    std::vector<Entity*>& getChildren() { return children; }
    Entity* getChild(const std::string& name);
    Entity* getParent() const { return parent; }
    bool isMovable() const { return movable; }
    void setMovable(bool state) { movable = state; }

    glm::vec3 getWorldPosition();
    glm::vec3 getWorldRotation();
    glm::vec3 getWorldScale();
    glm::mat4 getWorldTransform();

    void updateWorldTransform();

    void loadTextures();
    const std::vector<VkDescriptorSet>& getDescriptorSets() const { return descriptorSets; }
    void updateUniformBuffer(uint32_t frameIndex, const UniformBufferObject& ubo);

    AABB getWorldBounds(const glm::mat4& worldTransform) const;

private:
    std::string name;
    std::string shader = "gbuffer";
    std::vector<std::string> textures;
    std::vector<VkDescriptorSet> descriptorSets;
    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    size_t uniformBufferStride = 0;
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 rotation = glm::vec3(0.0f);
    glm::vec3 scale = glm::vec3(1.0f);
    glm::vec3 worldPosition = glm::vec3(0.0f);
    glm::vec3 worldRotation = glm::vec3(0.0f);
    glm::vec3 worldScale = glm::vec3(1.0f);
    glm::mat4 worldTransform = glm::mat4(1.0f);
    std::vector<Entity*> children;
    Entity* parent = nullptr;
    Model* model = nullptr;
    bool active = true;
    bool movable = false;

    void ensureUniformBuffers(Renderer* renderer, int vertexBindings);

    void destroyUniformBuffers();
};