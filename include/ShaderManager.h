#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

class Renderer;

struct Shader {
    std::string name;
    std::string vertexPath;
    std::string fragmentPath;
    VkPipeline pipeline{};
    VkPipelineLayout pipelineLayout{};
    VkDescriptorSetLayout descriptorSetLayout{};
    VkDescriptorPool descriptorPool{};
    VkPushConstantRange pushConstantRange{};
    int poolMultiplier = 1;
    int vertexBitBindings = 1;
    int fragmentBitBindings = 4;
};

struct ComputeShader {
    std::string name;
    std::string computePath;
    VkPipeline pipeline{};
    VkPipelineLayout pipelineLayout{};
    VkDescriptorSetLayout descriptorSetLayout{};
    VkDescriptorPool descriptorPool{};
    VkPushConstantRange pushConstantRange{};
    int poolMultiplier = 1;
    int computeBitBindings = 1;
    int storageImageCount = 1;
};

struct alignas(16) UIPushConstants {
    glm::vec3 color;
    uint32_t isUI;
    glm::mat4 model = glm::mat4(1.0f);
    uint32_t padding[3];
};

struct alignas(16) UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec3 cameraPos;
    float padding;
};

struct alignas(16) LightingPushConstants {
    glm::mat4 invView;
    glm::mat4 invProj;
    glm::vec3 cameraPos;
};

struct alignas(16) SSRPushConstants {
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 invView;
    glm::mat4 invProj;
};

class ShaderManager {
private:
    std::unordered_map<std::string, std::variant<Shader, ComputeShader>> shaders;
    Renderer* renderer;

public:
    ShaderManager(std::vector<std::variant<Shader*, ComputeShader*>>& shaders);
    ~ShaderManager();

    Shader* getShader(const std::string& name);
    ComputeShader* getComputeShader(const std::string& name);

    void loadShader(const std::string& name, const std::string& vertexPath, const std::string& fragmentPath, int vertexBitBindings, int fragmentBitBindings, VkPushConstantRange pushConstantRange = {}, int poolMultiplier = 1);
    void loadShader(const std::string& name, const std::string& computePath, int computeBitBindings, int storageImageCount, VkPushConstantRange pushConstantRange = {}, int poolMultiplier = 1);
    void shutdown();

    static ShaderManager* getInstance();
};
