#pragma once
#include <Entity.h>
#include <EntityManager.h>
#include <TextureManager.h>
#include <ShaderManager.h>
#include <Renderer.h>
#include <ModelManager.h>
#include <stb/stb_image.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cmath>
#include <vector>
#include <cstring>
#include <iostream>

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

class Skybox : public Entity {
public:
    Skybox() : Entity("skybox", "skybox", {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, glm::vec3(100.0f), ensureCubemapTexture()) {
        this->setModel(ModelManager::getInstance()->getModel("cube"));
    }
    void update(float deltaTime) override;

private:
    static std::vector<std::string> ensureCubemapTexture();
    static bool createCubemapTexture();
    Entity* camera = EntityManager::getInstance()->getEntity("camera");
};