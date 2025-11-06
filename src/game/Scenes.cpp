#include <vector>
#include <functional>
#include <iostream>
#include <cmath>
#include <glm/glm.hpp>
#include <UIManager.h>
#include <TextObject.h>
#include <ButtonObject.h>
#include "MainMenu/MainMenuFunctions.h"
#include <UIObject.h>
#include <EntityManager.h>
#include <Entity.h>
#include <ModelManager.h>
#include "Prefabs/Player.h"
#include <Camera.h>
#include <Renderer.h>
#include <Collider.h>
#include <Skybox.h>
#include <Light.h>
#include <utils.h>
#include "Scenes.h"

#define PI 3.14159265358979323846

Skybox* skybox;

void MainMenu() {
    Renderer::getInstance()->setUIMode(true);
    UIManager* uiMgr = UIManager::getInstance();
    TextObject* titleText = new TextObject("ParticleFront", "Lato", {0.0f, 0.0f}, {50.0f, 500.0f}, {1, 1}, "titleText", {1.0f, 1.0f, 1.0f});
    UIObject* container = new UIObject({0.0f, 0.0f}, {0.5f, 0.5f}, {1, 1}, "mainContainer", "window");
    container->addChild(titleText);
    ButtonObject* startButton = new ButtonObject({0.0f, -60.0f}, {200.0f, 50.0f}, {1, 1}, "startButton", "window", "Start Game", StartGame);
    container->addChild(startButton);
    uiMgr->addUIObject(container);
    skybox = new Skybox();
}

void Scene1() {
    Renderer::getInstance()->setUIMode(false);
    ModelManager* modelMgr = ModelManager::getInstance();
    EntityManager* entityMgr = EntityManager::getInstance();
    Entity* cube1 = new Entity("exampleCube", "gbuffer", blenderPosToEngine({16.226f, -9.377f, 0.338f}), blenderRotToEngine({-0.915f, -4.86f, 36.1f}), {1.0f, 1.0f, 1.0f}, {"materials_crate_albedo", "materials_crate_metallic", "materials_crate_roughness", "materials_crate_normal"});
    cube1->setModel(modelMgr->getModel("cube"));
    OBBCollider* box1 = new OBBCollider({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, cube1->getName(), {1.0f, 1.0f, 1.0f});
    cube1->addChild(box1);
    entityMgr->addEntity("exampleCube", cube1);
    Entity* cube2 = new Entity("exampleCube2", "gbuffer", blenderPosToEngine({-3.428f, 9.697f, 0.038f}), blenderRotToEngine({0.805f, -5.75f, 20.8f}), {1.0f, 1.0f, 1.0f}, {"materials_crate_albedo", "materials_crate_metallic", "materials_crate_roughness", "materials_crate_normal"});
    cube2->setModel(modelMgr->getModel("cube"));
    OBBCollider* box2 = new OBBCollider({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, cube2->getName(), {1.0f, 1.0f, 1.0f});
    cube2->addChild(box2);
    entityMgr->addEntity("exampleCube2", cube2);
    Entity* cube3 = new Entity("exampleCube3", "gbuffer", blenderPosToEngine({-13.327f, -10.937f, 0.063f}), blenderRotToEngine({47.1f, -83.4f, 60.2f}), {1.0f, 1.0f, 1.0f}, {"materials_crate_albedo", "materials_crate_metallic", "materials_crate_roughness", "materials_crate_normal"});
    cube3->setModel(modelMgr->getModel("cube"));
    OBBCollider* box3 = new OBBCollider({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, cube3->getName(), {1.0f, 1.0f, 1.0f});
    cube3->addChild(box3);
    entityMgr->addEntity("exampleCube3", cube3);
    Entity* cube4 = new Entity("exampleCube4", "gbuffer", blenderPosToEngine({-5.744f, 1.052f, 0.364f}), blenderRotToEngine({96.4f, -1.02f, 83.8f}), {1.0f, 1.0f, 1.0f}, {"materials_crate_albedo", "materials_crate_metallic", "materials_crate_roughness", "materials_crate_normal"});
    cube4->setModel(modelMgr->getModel("cube"));
    OBBCollider* box4 = new OBBCollider({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, cube4->getName(), {1.0f, 1.0f, 1.0f});
    cube4->addChild(box4);
    entityMgr->addEntity("exampleCube4", cube4);
    Entity* cube5 = new Entity("exampleCube5", "gbuffer", blenderPosToEngine({-5.676f, -1.421f, 0.405f}), blenderRotToEngine({-92.9f, -5.97f, -1.38f}), {1.0f, 1.0f, 1.0f}, {"materials_crate_albedo", "materials_crate_metallic", "materials_crate_roughness", "materials_crate_normal"});
    cube5->setModel(modelMgr->getModel("cube"));
    OBBCollider* box5 = new OBBCollider({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, cube5->getName(), {1.0f, 1.0f, 1.0f});
    cube5->addChild(box5);
    entityMgr->addEntity("exampleCube5", cube5);
    

    Entity* floor = new Entity("floor", "gbuffer", {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {"materials_ground_albedo", "materials_ground_metallic", "materials_ground_roughness", "materials_ground_normal"});
    floor->setModel(ModelManager::getInstance()->getModel("ground"));
    ConvexCollider* floorBox = new ConvexCollider({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, floor->getName());
    floorBox->setVerticesInterleaved(modelMgr->getModel("ground-collider")->getVertices(), 11, 0, modelMgr->getModel("ground-collider")->getIndices(), {0.0f, 0.0f, 0.0f});
    floor->addChild(floorBox);
    entityMgr->addEntity("floor", floor);

    Entity* walls = new Entity("walls", "gbuffer", {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.2f, 1.2f, 1.2f}, {"materials_walls_albedo", "materials_walls_metallic", "materials_walls_roughness", "materials_walls_normal"});
    walls->setModel(ModelManager::getInstance()->getModel("walls"));
    entityMgr->addEntity("walls", walls);

    for (int i = 0; i < 8; ++i) {
        Entity* lightHolder = new Entity("lightHolder" + std::to_string(i), "gbuffer", {0.0f, 0.0f, 0.0f}, {0.0f, i * 45.0f, 0.0f}, {1.2f, 1.2f, 1.2f}, {"materials_light_albedo", "materials_light_metallic", "materials_light_roughness", "materials_light_normal"});
        lightHolder->setModel(ModelManager::getInstance()->getModel("light"));
        Light* light = new Light("light" + std::to_string(i), 100.0f, {1.0f, 1.0f, 1.0f}, 5.0f, blenderPosToEngine({-74.345f, 0.0f, 21.414f}));
        lightHolder->addChild(light);
        entityMgr->addEntity("lightHolder" + std::to_string(i), lightHolder);
    }

    Player* player = new Player({16.0f, 10.0f, -9.0f}, {0.0f, 0.0f, 0.0f});
    
    entityMgr->addEntity("skybox", skybox);

    entityMgr->addEntity("player", player);
}

std::map<int, std::function<void()>> Scenes::sceneList = {
    {0, MainMenu},
    {1, Scene1},
};