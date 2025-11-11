#pragma once
#include <map>
#include <string>
#include <vector>
#include <Entity.h>

class Light;

class EntityManager {
public:
    EntityManager() = default;
    ~EntityManager() {
        shutdown();
    }

    std::map<std::string, Entity*>& getAllEntities() { return entities; }

    void addEntity(const std::string& name, Entity* entity) {
        entities[name] = entity;
        if (!entity->getParent()) {
            rootEntities.push_back(entity);
        }
    }

    std::vector<Entity*>& getRootEntities() { return rootEntities; }

    Entity* getEntity(const std::string& name) {
        if (entities.find(name) != entities.end()) {
            return entities[name];
        }
        return nullptr;
    }

    void removeEntity(const std::string& name) {
        if (entities.find(name) == entities.end()) return;
        Entity* entity = entities[name];
        if (!entity->getParent()) {
            rootEntities.erase(std::remove(rootEntities.begin(), rootEntities.end(), entity), rootEntities.end());
        }
        delete entity;
        entities.erase(name);
    }

    void updateAll(float deltaTime) {
        for (auto& pair : entities) {
            pair.second->update(deltaTime);
        }
    }

    void shutdown() {
        for (auto& [name, entity] : entities) {
            delete entity;
        }
        entities.clear();
    }

    static EntityManager* getInstance() {
        static EntityManager instance;
        return &instance;
    }

    std::vector<Light*> getDirtyLights() {
        std::vector<Light*> dirty;
        for (auto& light : dirtyLights) {
            dirty.push_back(light);
        }
        dirtyLights.clear();
        return dirty;
    }

    void markLightDirty(Light* light) {
        dirtyLights.push_back(light);
    }

private:
    std::map<std::string, Entity*> entities;
    std::vector<Entity*> rootEntities;
    std::vector<Light*> dirtyLights;
};