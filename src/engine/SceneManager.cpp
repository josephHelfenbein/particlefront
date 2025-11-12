#include <SceneManager.h>
#include <UIManager.h>
#include "../game/Scenes.h"
#include <utility>

SceneManager::SceneManager() {
    for (const auto& [id, sceneFunc] : Scenes().sceneList) {
        addScene(id, sceneFunc);
    }
}
SceneManager::~SceneManager() {
    shutdown();
}

void SceneManager::shutdown() {
    currentScene = 0;
    scenes.clear();
}

void SceneManager::addScene(int id, std::function<void()> func) {
    scenes[id] = std::move(func);
}

void SceneManager::switchScene(int id) {
    auto it = scenes.find(id);
    if (it != scenes.end()) {
        UIManager* uiMgr = UIManager::getInstance();
        uiMgr->clear();
        currentScene = id;
        it->second();
    }
}

SceneManager* SceneManager::getInstance() {
    static SceneManager instance;
    return &instance;
}