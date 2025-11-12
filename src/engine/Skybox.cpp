#include <Skybox.h>
#include <functional>

void Skybox::update(float deltaTime) {
    if (camera) {
        setPosition(camera->getPosition());
    }
}

std::vector<std::string> Skybox::ensureCubemapTexture() {
    static bool attempted = false;
    static std::vector<std::string> textures = {"skybox_cubemap"};
    if (!attempted) {
        attempted = true;
        if (!createCubemapTexture()) {
            textures.clear();
        }
    }
    std::cout<<"Skybox textures size: "<<textures.size()<<std::endl;
    return textures;
}

bool Skybox::createCubemapTexture() {
    TextureManager* textureManager = TextureManager::getInstance();
    Renderer* renderer = Renderer::getInstance();
    if (!textureManager || !renderer || renderer->device == VK_NULL_HANDLE) {
        return false;
    }
    if (textureManager->getTexture("skybox_cubemap")) {
        return true;
    }
    Image* source = textureManager->getTexture("skybox");
    if (!source || source->path.empty()) {
        return false;
    }

    stbi_set_flip_vertically_on_load(true);
    int width = 0;
    int height = 0;
    int channels = 0;
    constexpr int desiredChannels = 3;
    const bool isHDR = stbi_is_hdr(source->path.c_str()) != 0;
    float* hdrPixels = nullptr;
    unsigned char* ldrPixels = nullptr;
    if (isHDR) {
        hdrPixels = stbi_loadf(source->path.c_str(), &width, &height, &channels, desiredChannels);
    } else {
        ldrPixels = stbi_load(source->path.c_str(), &width, &height, &channels, desiredChannels);
    }
    if ((!hdrPixels && !ldrPixels) || width <= 0 || height <= 0) {
        if (hdrPixels) {
            stbi_image_free(hdrPixels);
        }
        if (ldrPixels) {
            stbi_image_free(ldrPixels);
        }
        return false;
    }

    const uint32_t inferredFaceWidth = static_cast<uint32_t>(std::max(1, width / 4));
    const uint32_t inferredFaceHeight = static_cast<uint32_t>(std::max(1, height / 2));
    const uint32_t faceSize = std::max(inferredFaceWidth, inferredFaceHeight);
    const VkDeviceSize facePixelCount = static_cast<VkDeviceSize>(faceSize) * static_cast<VkDeviceSize>(faceSize);
    const VkDeviceSize faceStrideBytes = facePixelCount * 4 * sizeof(float);
    std::vector<float> cubemapData(static_cast<size_t>(facePixelCount) * 4u * 6u, 0.0f);

    std::function<glm::vec3(float, float)> sampleTexel = [&](float u, float v) -> glm::vec3 {
        u = std::clamp(u, 0.0f, 1.0f);
        v = std::clamp(v, 0.0f, 1.0f);
        const float x = u * static_cast<float>(width - 1);
        const float y = v * static_cast<float>(height - 1);
        const int x0 = static_cast<int>(std::floor(x));
        const int y0 = static_cast<int>(std::floor(y));
        const int x1 = std::min(x0 + 1, width - 1);
        const int y1 = std::min(y0 + 1, height - 1);
        const float tx = x - static_cast<float>(x0);
        const float ty = y - static_cast<float>(y0);
        std::function<glm::vec3(int, int)> fetch = [&](int px, int py) -> glm::vec3 {
            const size_t index = static_cast<size_t>(py) * static_cast<size_t>(width) + static_cast<size_t>(px);
            if (isHDR) {
                const float* data = hdrPixels + index * desiredChannels;
                return glm::vec3(data[0], data[1], data[2]);
            }
            const unsigned char* data = ldrPixels + index * desiredChannels;
            return glm::vec3(data[0], data[1], data[2]) / 255.0f;
        };
        const glm::vec3 c00 = fetch(x0, y0);
        const glm::vec3 c10 = fetch(x1, y0);
        const glm::vec3 c01 = fetch(x0, y1);
        const glm::vec3 c11 = fetch(x1, y1);
        const glm::vec3 cx0 = glm::mix(c00, c10, tx);
        const glm::vec3 cx1 = glm::mix(c01, c11, tx);
        return glm::mix(cx0, cx1, ty);
    };

    auto faceDirection = [](uint32_t face, float u, float v) -> glm::vec3 {
        switch(face) {
            case 0: return glm::normalize(glm::vec3(1.0f, v, -u));
            case 1: return glm::normalize(glm::vec3(-1.0f, v, u));
            case 2: return glm::normalize(glm::vec3(u, -1.0f, v));
            case 3: return glm::normalize(glm::vec3(u, 1.0f, -v));
            case 4: return glm::normalize(glm::vec3(u, v, 1.0f));
            default: return glm::normalize(glm::vec3(-u, v, -1.0f));
        }
    };

    const int faceSizeInt = static_cast<int>(faceSize);
#if defined(USE_OPENMP)
    #pragma omp parallel for collapse(3)
#endif
    for (int face = 0; face < 6; ++face) {
        for (int y = 0; y < faceSizeInt; ++y) {
            for (int x = 0; x < faceSizeInt; ++x) {
                const float s = (static_cast<float>(x) + 0.5f) / static_cast<float>(faceSizeInt) * 2.0f - 1.0f;
                const float t = (static_cast<float>(y) + 0.5f) / static_cast<float>(faceSizeInt) * 2.0f - 1.0f;
                const glm::vec3 dir = faceDirection(static_cast<uint32_t>(face), s, t);
                const float phi = std::atan2(dir.z, dir.x);
                const float theta = std::acos(std::clamp(dir.y, -1.0f, 1.0f));
                const float texU = phi / (2.0f * glm::pi<float>()) + 0.5f;
                const float texV = theta / glm::pi<float>();
                const glm::vec3 color = sampleTexel(texU, texV);
                const size_t baseIndex = (
                    static_cast<size_t>(face) * static_cast<size_t>(facePixelCount) +
                    static_cast<size_t>(y) * static_cast<size_t>(faceSizeInt) +
                    static_cast<size_t>(x)
                ) * 4;
                cubemapData[baseIndex + 0] = color.r;
                cubemapData[baseIndex + 1] = color.g;
                cubemapData[baseIndex + 2] = color.b;
                cubemapData[baseIndex + 3] = 1.0f;
            }
        }
    }

    if (hdrPixels) {
        stbi_image_free(hdrPixels);
    }
    if (ldrPixels) {
        stbi_image_free(ldrPixels);
    }

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    const VkDeviceSize totalSize = faceStrideBytes * 6;
    renderer->createBuffer(totalSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingMemory);
    void* mapped = nullptr;
    if (vkMapMemory(renderer->device, stagingMemory, 0, totalSize, 0, &mapped) != VK_SUCCESS || !mapped) {
        vkDestroyBuffer(renderer->device, stagingBuffer, nullptr);
        vkFreeMemory(renderer->device, stagingMemory, nullptr);
        return false;
    }
    std::memcpy(mapped, cubemapData.data(), static_cast<size_t>(totalSize));
    vkUnmapMemory(renderer->device, stagingMemory);

    Image cubemap;
    cubemap.width = static_cast<int>(faceSize);
    cubemap.height = static_cast<int>(faceSize);
    cubemap.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    cubemap.path = source->path;

    renderer->createImage(faceSize, faceSize, 1, VK_SAMPLE_COUNT_1_BIT, cubemap.format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, cubemap.image, cubemap.imageMemory, 6, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);
    renderer->transitionImageLayout(cubemap.image, cubemap.format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, 6);
    renderer->copyBufferToImage(stagingBuffer, cubemap.image, faceSize, faceSize, 6, faceStrideBytes);
    renderer->transitionImageLayout(cubemap.image, cubemap.format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1, 6);

    vkDestroyBuffer(renderer->device, stagingBuffer, nullptr);
    vkFreeMemory(renderer->device, stagingMemory, nullptr);

    cubemap.imageView = renderer->createImageView(cubemap.image, cubemap.format, 1, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_CUBE, 6);

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
    if (vkCreateSampler(renderer->device, &samplerInfo, nullptr, &cubemap.imageSampler) != VK_SUCCESS) {
        vkDestroyImageView(renderer->device, cubemap.imageView, nullptr);
        vkDestroyImage(renderer->device, cubemap.image, nullptr);
        vkFreeMemory(renderer->device, cubemap.imageMemory, nullptr);
        return false;
    }

    textureManager->registerTexture("skybox_cubemap", cubemap);
    return true;
}