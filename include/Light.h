#pragma once
#include <Entity.h>
#include <glm/glm.hpp>
#include <ShaderManager.h>
#include <TextureManager.h>
#include <EntityManager.h>
#include <string>
#include <Image.h>
#include <Renderer.h>
#include <stdexcept>
#include <array>
#include <algorithm>

constexpr uint32_t kShadowMapSize = 1024;

class Light : public Entity {
public:
    Light(const std::string& name, float radius, const glm::vec3& color, float intensity,
          const glm::vec3& position = {0.0f, 0.0f, 0.0f}, const glm::vec3& rotation = {0.0f, 0.0f, 0.0f})
        : Entity(name, "", position, rotation), radius(radius), color(color), intensity(intensity), shadowTextureName(this->getName() + "_shadow_cubemap") {
        shadowFarPlane = std::max(radius, shadowFarPlane);
        initializeShadowResources();
        EntityManager::getInstance()->markLightDirty(this);
    }
    virtual ~Light() {
        destroyShadowResources();
    }

    float getRadius() const { return radius; }
    void setRadius(float r) { radius = r; }
    glm::vec3 getColor() const { return color; }
    void setColor(const glm::vec3& c) { color = c; }
    float getIntensity() const { return intensity; }
    void setIntensity(float i) { intensity = i; }

    bool getCastsShadows() const { return castsShadows; }
    void setCastsShadows(bool value) { castsShadows = value; }

    float getShadowBias() const { return shadowBias; }
    void setShadowBias(float bias) { shadowBias = bias; }

    float getShadowNearPlane() const { return shadowNearPlane; }
    void setShadowNearPlane(float nearPlane) { shadowNearPlane = nearPlane; }

    float getShadowFarPlane() const { return shadowFarPlane; }
    void setShadowFarPlane(float farPlane) { shadowFarPlane = farPlane; }

    Image* getShadowMap() {
        TextureManager* textureManager = TextureManager::getInstance();
        if (!textureManager) {
            return nullptr;
        }
        Image* existing = textureManager->getTexture(shadowTextureName);
        if (existing && existing->image != VK_NULL_HANDLE && existing->imageView != VK_NULL_HANDLE) {
            return existing;
        }

        Renderer* rendererInstance = Renderer::getInstance();
        if (!rendererInstance) {
            throw std::runtime_error("Renderer instance is not available for shadow map creation.");
        }
        VkDevice deviceHandle = rendererInstance->getDevice();
        if (deviceHandle == VK_NULL_HANDLE) {
            throw std::runtime_error("Renderer device is not initialized for shadow map creation.");
        }
        const VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

        Image shadowImage;
        shadowImage.width = static_cast<int>(kShadowMapSize);
        shadowImage.height = static_cast<int>(kShadowMapSize);
        shadowImage.format = depthFormat;

        std::array<VkImageView, 6> tempFaceViews{};
        tempFaceViews.fill(VK_NULL_HANDLE);

        auto cleanupOnFailure = [&](uint32_t createdFaceCount) {
            for (uint32_t j = 0; j < createdFaceCount; ++j) {
                if (tempFaceViews[j] != VK_NULL_HANDLE) {
                    vkDestroyImageView(deviceHandle, tempFaceViews[j], nullptr);
                    tempFaceViews[j] = VK_NULL_HANDLE;
                }
            }
            if (shadowImage.imageSampler != VK_NULL_HANDLE) {
                vkDestroySampler(deviceHandle, shadowImage.imageSampler, nullptr);
                shadowImage.imageSampler = VK_NULL_HANDLE;
            }
            if (shadowImage.imageView != VK_NULL_HANDLE) {
                vkDestroyImageView(deviceHandle, shadowImage.imageView, nullptr);
                shadowImage.imageView = VK_NULL_HANDLE;
            }
            if (shadowImage.image != VK_NULL_HANDLE) {
                vkDestroyImage(deviceHandle, shadowImage.image, nullptr);
                shadowImage.image = VK_NULL_HANDLE;
            }
            if (shadowImage.imageMemory != VK_NULL_HANDLE) {
                vkFreeMemory(deviceHandle, shadowImage.imageMemory, nullptr);
                shadowImage.imageMemory = VK_NULL_HANDLE;
            }
        };

        rendererInstance->createImage(
            kShadowMapSize,
            kShadowMapSize,
            1,
            VK_SAMPLE_COUNT_1_BIT,
            depthFormat,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            shadowImage.image,
            shadowImage.imageMemory,
            6,
            VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT
        );

        uint32_t createdFaceCount = 0;
        try {
            rendererInstance->transitionImageLayout(
                shadowImage.image,
                depthFormat,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                1,
                6
            );
            shadowImageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            shadowImage.imageView = rendererInstance->createImageView(
                shadowImage.image,
                depthFormat,
                1,
                VK_IMAGE_ASPECT_DEPTH_BIT,
                VK_IMAGE_VIEW_TYPE_CUBE,
                6
            );

            for (uint32_t i = 0; i < 6; ++i) {
                VkImageViewCreateInfo viewInfo = {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                    .image = shadowImage.image,
                    .viewType = VK_IMAGE_VIEW_TYPE_2D,
                    .format = depthFormat,
                    .subresourceRange = {
                        .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = i,
                        .layerCount = 1,
                    },
                };
                if (vkCreateImageView(deviceHandle, &viewInfo, nullptr, &tempFaceViews[i]) != VK_SUCCESS) {
                    cleanupOnFailure(createdFaceCount);
                    throw std::runtime_error("Failed to create shadow cubemap face image view.");
                }
                createdFaceCount = i + 1;
            }

            VkSamplerCreateInfo samplerInfo = {
                .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                .magFilter = VK_FILTER_LINEAR,
                .minFilter = VK_FILTER_LINEAR,
                .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
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
                .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
                .unnormalizedCoordinates = VK_FALSE,
            };

            if (vkCreateSampler(deviceHandle, &samplerInfo, nullptr, &shadowImage.imageSampler) != VK_SUCCESS) {
                cleanupOnFailure(createdFaceCount);
                throw std::runtime_error("Failed to create shadow cubemap sampler.");
            }
        } catch (...) {
            cleanupOnFailure(createdFaceCount);
            throw;
        }

        for (uint32_t i = 0; i < 6; ++i) {
            shadowImageViews[i] = tempFaceViews[i];
        }

        textureManager->registerTexture(shadowTextureName, shadowImage);
        return textureManager->getTexture(shadowTextureName);
    }

    VkFramebuffer getShadowFrameBuffer(int index) {
        if (index < 0 || index >= 6) {
            throw std::out_of_range("Shadow framebuffer index out of range (0-5).");
        }
        if (shadowFrameBuffers[index] != VK_NULL_HANDLE) {
            return shadowFrameBuffers[index];
        }
        VkFramebufferCreateInfo framebufferInfo = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = Renderer::getInstance()->getShadowMapRenderPass(),
            .attachmentCount = 1,
            .pAttachments = &shadowImageViews[index],
            .width = static_cast<uint32_t>(kShadowMapSize),
            .height = static_cast<uint32_t>(kShadowMapSize),
            .layers = 1,
        };
        if (vkCreateFramebuffer(Renderer::getInstance()->getDevice(), &framebufferInfo, nullptr, &shadowFrameBuffers[index]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create shadow framebuffer.");
        }
        return shadowFrameBuffers[index];
    }

    VkRenderPassBeginInfo getShadowRenderPassBeginInfo(VkFramebuffer framebuffer, VkExtent2D extent, int faceIndex) const {
        VkRenderPassBeginInfo renderPassInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = Renderer::getInstance()->getShadowMapRenderPass(),
            .framebuffer = framebuffer,
            .renderArea = {
                .offset = {0, 0},
                .extent = extent
            },
            .clearValueCount = 1,
            .pClearValues = &clearValue,
        };
        return renderPassInfo;
    }

    float getShadowStrength() const { return shadowStrength; }
    void setShadowStrength(float strength) { shadowStrength = strength; }

    uint32_t getShadowMapIndex() const { return shadowMapIndex; }
    void setShadowMapIndex(uint32_t index) { shadowMapIndex = index; }

    const glm::mat4* getShadowViewProjections() const { return shadowViewProjections; }
    void setShadowViewProjections(const glm::mat4 vps[6]) { 
        for (int i = 0; i < 6; ++i) {
            shadowViewProjections[i] = vps[i];
        }
    }
    void setShadowViewProjection(int index, const glm::mat4& vp) {
        if (index >= 0 && index < 6) {
            shadowViewProjections[index] = vp;
        }
    }

    void transitionShadowMapLayout(VkCommandBuffer commandBuffer, VkImageLayout newLayout, Image* shadowImage = nullptr) {
        Renderer* rendererInstance = Renderer::getInstance();
        if (!rendererInstance || commandBuffer == VK_NULL_HANDLE) {
            return;
        }

        Image* targetImage = shadowImage ? shadowImage : getShadowMap();
        if (!targetImage || targetImage->image == VK_NULL_HANDLE) {
            return;
        }
        if (shadowImageLayout == newLayout) {
            return;
        }

        VkImageLayout oldLayout = shadowImageLayout;
        VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        VkAccessFlags srcAccess = 0;
        VkAccessFlags dstAccess = VK_ACCESS_SHADER_READ_BIT;

        if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ||
            oldLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL) {
            srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            srcAccess = VK_ACCESS_SHADER_READ_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            srcStage = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            srcAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        }

        if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            dstAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        } else if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL) {
            dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            dstAccess = VK_ACCESS_SHADER_READ_BIT;
        }

        VkImageMemoryBarrier barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = srcAccess,
            .dstAccessMask = dstAccess,
            .oldLayout = oldLayout == VK_IMAGE_LAYOUT_UNDEFINED ? VK_IMAGE_LAYOUT_UNDEFINED : oldLayout,
            .newLayout = newLayout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = targetImage->image,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 6,
            },
        };

        vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        shadowImageLayout = newLayout;
    }

    PointLight getPointLightData() {
        glm::vec3 worldPos = getWorldPosition();
        PointLight pl = {
            .positionRadius = glm::vec4(worldPos, radius),
            .colorIntensity = glm::vec4(color, intensity),
            .lightViewProj = {
                shadowViewProjections[0],
                shadowViewProjections[1],
                shadowViewProjections[2],
                shadowViewProjections[3],
                shadowViewProjections[4],
                shadowViewProjections[5]
            },
            .shadowParams = glm::vec4(shadowBias, shadowNearPlane, shadowFarPlane, shadowStrength),
            .shadowData = glm::uvec4(shadowMapIndex, castsShadows ? 1u : 0u, 0u, 0u)
        };
        return pl;
    }

private:
    float radius;
    glm::vec3 color;
    float intensity;
    bool castsShadows = true;
    float shadowBias = 0.02f;
    float shadowNearPlane = 0.1f;
    float shadowFarPlane = 300.0f;
    std::string shadowTextureName = "";
    float shadowStrength = 1.0f;
    uint32_t shadowMapIndex = 0;
    glm::mat4 shadowViewProjections[6];
    VkFramebuffer shadowFrameBuffers[6] = { VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE };
    VkImageView shadowImageViews[6] = { VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE };
    VkImageLayout shadowImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkClearValue clearValue = {
        .depthStencil = {1.0f, 0},
    };

    void initializeShadowResources() {
        if (!castsShadows) {
            return;
        }
        Image* shadowImage = getShadowMap();
        if (!shadowImage) {
            return;
        }
        for (uint32_t i = 0; i < 6; ++i) {
            if (shadowImageViews[i] == VK_NULL_HANDLE) {
                VkImageViewCreateInfo viewInfo = {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                    .image = shadowImage->image,
                    .viewType = VK_IMAGE_VIEW_TYPE_2D,
                    .format = shadowImage->format,
                    .subresourceRange = {
                        .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = i,
                        .layerCount = 1,
                    },
                };
                if (vkCreateImageView(Renderer::getInstance()->getDevice(), &viewInfo, nullptr, &shadowImageViews[i]) != VK_SUCCESS) {
                    throw std::runtime_error("Failed to create per-face shadow image view.");
                }
            }
            if (shadowFrameBuffers[i] == VK_NULL_HANDLE) {
                VkFramebufferCreateInfo framebufferInfo = {
                    .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                    .renderPass = Renderer::getInstance()->getShadowMapRenderPass(),
                    .attachmentCount = 1,
                    .pAttachments = &shadowImageViews[i],
                    .width = static_cast<uint32_t>(kShadowMapSize),
                    .height = static_cast<uint32_t>(kShadowMapSize),
                    .layers = 1,
                };
                if (vkCreateFramebuffer(Renderer::getInstance()->getDevice(), &framebufferInfo, nullptr, &shadowFrameBuffers[i]) != VK_SUCCESS) {
                    throw std::runtime_error("Failed to create shadow framebuffer.");
                }
            }
        }
    }

    void destroyShadowResources() {
        Renderer* rendererInstance = Renderer::getInstance();
        VkDevice deviceHandle = rendererInstance ? rendererInstance->getDevice() : VK_NULL_HANDLE;
        if (deviceHandle == VK_NULL_HANDLE) {
            std::fill(std::begin(shadowFrameBuffers), std::end(shadowFrameBuffers), VK_NULL_HANDLE);
            std::fill(std::begin(shadowImageViews), std::end(shadowImageViews), VK_NULL_HANDLE);
            return;
        }
        for (auto& framebuffer : shadowFrameBuffers) {
            if (framebuffer != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(deviceHandle, framebuffer, nullptr);
                framebuffer = VK_NULL_HANDLE;
            }
        }
        for (auto& imageView : shadowImageViews) {
            if (imageView != VK_NULL_HANDLE) {
                vkDestroyImageView(deviceHandle, imageView, nullptr);
                imageView = VK_NULL_HANDLE;
            }
        }
        shadowImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    }
};