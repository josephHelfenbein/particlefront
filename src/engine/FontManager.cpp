#include <FontManager.h>
#include <Renderer.h>
#include <ShaderManager.h>
#include <freetype/include/ft2build.h>
#include FT_FREETYPE_H
#include <iostream>
#include <vector>
#include <algorithm>
#include <utility>
#include <filesystem>
#include <utils.h>

FontManager::FontManager() {
    renderer = Renderer::getInstance();
}
FontManager::~FontManager() {
    shutdown();
}

void FontManager::loadFont(const std::string& fontPath, const std::string& fontName, int fontSize) {
    FT_Library ft;
    if(FT_Init_FreeType(&ft)) {
        throw std::runtime_error("could not init FreeType library");
    }
    FT_Face face;
    const std::filesystem::path resolvedFontPath = resolvePath(fontPath);
    if(!std::filesystem::exists(resolvedFontPath)) {
        throw std::runtime_error("font file not found: " + resolvedFontPath.string());
    }
    if(FT_New_Face(ft, resolvedFontPath.string().c_str(), 0, &face)) {
        throw std::runtime_error("failed to load font: " + resolvedFontPath.string());
    }
    FT_Set_Pixel_Sizes(face, 0, fontSize);
    FT_ULong codepoints[128];
    for(FT_ULong c = 0; c < 128; c++) codepoints[c] = c;
    Font& font = fonts[fontName];
    font.fontName = fontName;
    font.fontSize = fontSize;
    font.ascent = static_cast<int>(face->size->metrics.ascender >> 6);
    font.descent = static_cast<int>(face->size->metrics.descender >> 6);
    font.lineHeight = static_cast<int>(face->size->metrics.height >> 6);
    font.maxGlyphHeight = 0;
    font.characters.clear();
    for(FT_ULong c : codepoints) {
        if(FT_Load_Char(face, c, FT_LOAD_RENDER)) {
            std::cerr<<"failed to load glyph: "<<(char)c<<std::endl;
            continue;
        }
        Character character{};
        character.size = glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows);
        character.bearing = glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top);
        const FT_Pos rawAdvance = std::max<FT_Pos>(face->glyph->advance.x, static_cast<FT_Pos>(0));
        character.advance = static_cast<unsigned int>(rawAdvance >> 6);
        if(face->glyph->bitmap.width == 0 || face->glyph->bitmap.rows == 0 || face->glyph->bitmap.buffer == nullptr) {
            unsigned char dummyPixel = 0;
            renderer->createTextureImage(1, 1, &dummyPixel, character.texture.image, character.texture.imageMemory, VK_FORMAT_R8_UNORM);
        } else renderer->createTextureImage(face->glyph->bitmap.width, face->glyph->bitmap.rows, face->glyph->bitmap.buffer, character.texture.image, character.texture.imageMemory, VK_FORMAT_R8_UNORM);
        renderer->createTextureImageView(VK_FORMAT_R8_UNORM, character.texture.image, character.texture.imageView);
        Shader* uiShader = ShaderManager::getInstance()->getShader("ui");
        Image* imgPtr = &character.texture;
        std::vector<Image*> textures = {imgPtr};
        std::vector<VkBuffer> uniformBuffers;
        character.descriptorSets = renderer->createDescriptorSets(
            uiShader->descriptorPool,
            uiShader->descriptorSetLayout,
            uiShader->vertexBitBindings,
            uiShader->fragmentBitBindings,
            textures,
            uniformBuffers
        );
        const char glyph = static_cast<char>(c);
        const int glyphHeight = static_cast<int>(character.size.y);
        font.characters.insert_or_assign(glyph, std::move(character));
        if (glyphHeight > font.maxGlyphHeight) {
            font.maxGlyphHeight = glyphHeight;
        }
    }
    FT_Done_Face(face);
    FT_Done_FreeType(ft);
}
Font* FontManager::getFont(const std::string& fontName) {
    auto it = fonts.find(fontName);
    if(it != fonts.end()) {
        return &it->second;
    }
    return nullptr;
}

FontManager* FontManager::getInstance() {
    static FontManager instance;
    return &instance;
}

void FontManager::shutdown() {
    if (!renderer || renderer->device == VK_NULL_HANDLE) {
        fonts.clear();
        return;
    }
    for (auto& [fontName, font] : fonts) {
        for (auto& [ch, character] : font.characters) {
            Image& img = character.texture;
            if (img.imageSampler) {
                vkDestroySampler(renderer->device, img.imageSampler, nullptr);
                img.imageSampler = VK_NULL_HANDLE;
            }
            if (img.imageView) {
                vkDestroyImageView(renderer->device, img.imageView, nullptr);
                img.imageView = VK_NULL_HANDLE;
            }
            if (img.image) {
                vkDestroyImage(renderer->device, img.image, nullptr);
                img.image = VK_NULL_HANDLE;
            }
            if (img.imageMemory) {
                vkFreeMemory(renderer->device, img.imageMemory, nullptr);
                img.imageMemory = VK_NULL_HANDLE;
            }
            character.descriptorSets.clear();
        }
        font.characters.clear();
    }
    fonts.clear();
}