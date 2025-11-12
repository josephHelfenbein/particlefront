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

    void addEntity(const std::string& name, Entity* entity);

    std::vector<Entity*>& getRootEntities() { return rootEntities; }
    std::vector<Entity*>& getMovableEntities() { return movableEntities; }

    Entity* getEntity(const std::string& name) {
        if (entities.find(name) != entities.end()) {
            return entities[name];
        }
        return nullptr;
    }

    void removeEntity(const std::string& name);

    void updateAll(float deltaTime);

    void shutdown();

    static EntityManager* getInstance() {
        static EntityManager instance;
        return &instance;
    }

    std::vector<Light*> getDirtyLights();
    std::vector<Light*>& getAllLights() { return allLights; }

    void registerMovableEntity(Entity* entity) {
        movableEntities.push_back(entity);
    }
    void unregisterMovableEntity(Entity* entity) {
        movableEntities.erase(std::remove(movableEntities.begin(), movableEntities.end(), entity), movableEntities.end());
    }

    void markLightDirty(Light* light) {
        dirtyLights.push_back(light);
    }

private:
    std::map<std::string, Entity*> entities;
    std::vector<Entity*> rootEntities;
    std::vector<Entity*> movableEntities;
    std::vector<Light*> dirtyLights;
    std::vector<Light*> allLights;
};