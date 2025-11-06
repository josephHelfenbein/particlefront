#include <TextureManager.h>
#include <Renderer.h>
#include <filesystem>
#include <stb/stb_image.h>
#include <utils.h>

static uint16_t floatToHalf(float value) {
    union { float f; uint32_t i; } v;
    v.f = value;
    uint32_t i = v.i;
    
    uint32_t sign = (i >> 16) & 0x8000;
    int32_t exponent = ((i >> 23) & 0xff) - 127 + 15;
    uint32_t mantissa = i & 0x7fffff;
    
    if (exponent <= 0) {
        if (exponent < -10) return static_cast<uint16_t>(sign);
        mantissa = (mantissa | 0x800000) >> (1 - exponent);
        return static_cast<uint16_t>(sign | (mantissa >> 13));
    } else if (exponent >= 0x1f) {
        return static_cast<uint16_t>(sign | 0x7c00);
    }
    
    return static_cast<uint16_t>(sign | (exponent << 10) | (mantissa >> 13));
}

TextureManager::TextureManager() {
    renderer = Renderer::getInstance();
    findAllTextures();
    prepareTextureAtlas();
}
TextureManager::~TextureManager() {
    shutdown();
}
void TextureManager::findAllTextures(std::string path, std::string prevName) {
    namespace fs = std::filesystem;
    fs::path searchPath = resolvePath(path);
    std::error_code ec;
    if ((!fs::exists(searchPath, ec) || !fs::is_directory(searchPath, ec)) && prevName.empty()) {
        searchPath = resolvePath("src/assets/textures/");
    }
    if (!fs::exists(searchPath, ec) || !fs::is_directory(searchPath, ec)) {
        return;
    }

    for (const auto& entry : fs::directory_iterator(searchPath)) {
        std::string name = entry.path().stem().string();
        if (entry.path().extension() == ".png" || entry.path().extension() == ".hdr") {
            textureAtlas[prevName + name] = Image{};
            textureAtlas[prevName + name].path = entry.path().string();
        } else if (entry.is_directory()) {
            findAllTextures(entry.path().string(), prevName + name + "_");
        }
    }
}
void TextureManager::prepareTextureAtlas() {
    struct ImageLoadResult {
        std::string name;
        void* pixels;
        int width;
        int height;
        bool isHDR;
    };
    std::vector<ImageLoadResult> loadedImages;
    
#if defined(USE_OPENMP)
    #pragma omp parallel for
#endif
    for (int i = 0; i < static_cast<int>(textureAtlas.size()); ++i) {
        auto it = std::next(textureAtlas.begin(), i);
        stbi_set_flip_vertically_on_load(false);
        int texWidth, texHeight, texChannels;
        void* pixels = nullptr;
        bool isHDR = false;
        
        if (it->second.path.ends_with(".hdr")) {
            pixels = stbi_loadf(it->second.path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
            isHDR = true;
        } else {
            pixels = stbi_load(it->second.path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
            isHDR = false;
        }
        
#if defined(USE_OPENMP)
        #pragma omp critical
#endif
        {
            loadedImages.push_back({it->first, pixels, texWidth, texHeight, isHDR});
        }
    }
    
    for (auto& imageData : loadedImages) {
        Image& texture = textureAtlas[imageData.name];
        void* pixels = imageData.pixels;
        if (!pixels || imageData.width <= 0 || imageData.height <= 0) continue;
        
        VkFormat imageFormat;
        VkDeviceSize pixelSize;
        if (imageData.isHDR) {
            imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
            float* floatPixels = static_cast<float*>(pixels);
            size_t numPixels = static_cast<size_t>(imageData.width) * static_cast<size_t>(imageData.height);
            std::vector<uint16_t> float16Pixels(numPixels * 4);
            for (size_t i = 0; i < numPixels * 4; ++i) {
                float16Pixels[i] = floatToHalf(floatPixels[i]);
            }
            pixelSize = numPixels * 4 * sizeof(uint16_t);
            VkBuffer stagingBuffer;
            VkDeviceMemory stagingBufferMemory;
            renderer->createBuffer(pixelSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                stagingBuffer, stagingBufferMemory);
            void* data;
            vkMapMemory(renderer->device, stagingBufferMemory, 0, pixelSize, 0, &data);
            memcpy(data, float16Pixels.data(), static_cast<size_t>(pixelSize));
            vkUnmapMemory(renderer->device, stagingBufferMemory);
            VkImage textureImage;
            VkDeviceMemory textureImageMemory;
            renderer->createImage(imageData.width, imageData.height, 1, VK_SAMPLE_COUNT_1_BIT, 
                imageFormat, VK_IMAGE_TILING_OPTIMAL, 
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory);
            renderer->transitionImageLayout(textureImage, imageFormat, 
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            renderer->copyBufferToImage(stagingBuffer, textureImage, 
                static_cast<uint32_t>(imageData.width), static_cast<uint32_t>(imageData.height));
            renderer->transitionImageLayout(textureImage, imageFormat, 
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            vkDestroyBuffer(renderer->device, stagingBuffer, nullptr);
            vkFreeMemory(renderer->device, stagingBufferMemory, nullptr);
            texture.image = textureImage;
            texture.imageMemory = textureImageMemory;
            renderer->createTextureImageView(imageFormat, textureImage, texture.imageView);
        } else {
            imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
            pixelSize = static_cast<VkDeviceSize>(imageData.width) * 
                static_cast<VkDeviceSize>(imageData.height) * 4;
            VkBuffer stagingBuffer;
            VkDeviceMemory stagingBufferMemory;
            renderer->createBuffer(pixelSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                stagingBuffer, stagingBufferMemory);
            void* data;
            vkMapMemory(renderer->device, stagingBufferMemory, 0, pixelSize, 0, &data);
            memcpy(data, pixels, static_cast<size_t>(pixelSize));
            vkUnmapMemory(renderer->device, stagingBufferMemory);
            VkImage textureImage;
            VkDeviceMemory textureImageMemory;
            renderer->createImage(imageData.width, imageData.height, 1, VK_SAMPLE_COUNT_1_BIT, 
                imageFormat, VK_IMAGE_TILING_OPTIMAL, 
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory);
            renderer->transitionImageLayout(textureImage, imageFormat, 
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            renderer->copyBufferToImage(stagingBuffer, textureImage, 
                static_cast<uint32_t>(imageData.width), static_cast<uint32_t>(imageData.height));
            renderer->transitionImageLayout(textureImage, imageFormat, 
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            vkDestroyBuffer(renderer->device, stagingBuffer, nullptr);
            vkFreeMemory(renderer->device, stagingBufferMemory, nullptr);
            texture.image = textureImage;
            texture.imageMemory = textureImageMemory;
            renderer->createTextureImageView(imageFormat, textureImage, texture.imageView);
        }
        
        stbi_image_free(pixels);
    }
}
Image* TextureManager::getTexture(const std::string& name) {
    auto it = textureAtlas.find(name);
    if(it != textureAtlas.end()) {
        return &it->second;
    }
    return nullptr;
}
void TextureManager::registerTexture(const std::string& name, const Image& texture) {
    if (!renderer) {
        renderer = Renderer::getInstance();
    }
    VkDevice deviceHandle = renderer ? renderer->device : VK_NULL_HANDLE;
    auto it = textureAtlas.find(name);
    if (it != textureAtlas.end()) {
        if (deviceHandle != VK_NULL_HANDLE) {
            if (it->second.imageSampler) {
                vkDestroySampler(deviceHandle, it->second.imageSampler, nullptr);
            }
            if (it->second.imageView) {
                vkDestroyImageView(deviceHandle, it->second.imageView, nullptr);
            }
            if (it->second.image) {
                vkDestroyImage(deviceHandle, it->second.image, nullptr);
            }
            if (it->second.imageMemory) {
                vkFreeMemory(deviceHandle, it->second.imageMemory, nullptr);
            }
        }
    }
    textureAtlas[name] = texture;
}
void TextureManager::shutdown() {
    if (!renderer || renderer->device == VK_NULL_HANDLE) {
        textureAtlas.clear();
        return;
    }
    for(auto& [name, texture] : textureAtlas) {
        if (texture.imageSampler) {
            vkDestroySampler(renderer->device, texture.imageSampler, nullptr);
            texture.imageSampler = VK_NULL_HANDLE;
        }
        if (texture.imageView) {
            vkDestroyImageView(renderer->device, texture.imageView, nullptr);
            texture.imageView = VK_NULL_HANDLE;
        }
        if (texture.image) {
            vkDestroyImage(renderer->device, texture.image, nullptr);
            texture.image = VK_NULL_HANDLE;
        }
        if (texture.imageMemory) {
            vkFreeMemory(renderer->device, texture.imageMemory, nullptr);
            texture.imageMemory = VK_NULL_HANDLE;
        }
    }
    textureAtlas.clear();
}
TextureManager* TextureManager::getInstance() {
    static TextureManager instance;
    return &instance;
}