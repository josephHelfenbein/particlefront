#include <EntityManager.h>
#include <Entity.h>
#include <Light.h>
#include <unordered_set>
#include <functional>

void EntityManager::addEntity(const std::string& name, Entity* entity) {
    entities[name] = entity;
    if (!entity->getParent()) {
        rootEntities.push_back(entity);
    }
    if (Light* light = dynamic_cast<Light*>(entity)) {
        allLights.push_back(light);
        dirtyLights.push_back(light);
    }
    if (entity->isMovable()) {
        movableEntities.push_back(entity);
    }
    if (entity->getChildren().size() > 0) {
        for (auto* child : entity->getChildren()) {
            if (this->entities.find(child->getName()) == this->entities.end()) {
                this->addEntity(child->getName(), child);
            }
        }
    }
}

void EntityManager::removeEntity(const std::string& name) {
    if (entities.find(name) == entities.end()) return;
    Entity* entity = entities[name];

    std::vector<Entity*> hierarchy;
    hierarchy.reserve(8);
    auto collect = [&](auto&& self, Entity* current) {
        if (!current) {
            return;
        }
        hierarchy.push_back(current);
        for (Entity* child : current->getChildren()) {
            self(self, child);
        }
    };
    collect(collect, entity);

    if (Entity* parent = entity->getParent()) {
        parent->removeChild(entity);
    } else {
        rootEntities.erase(std::remove(rootEntities.begin(), rootEntities.end(), entity), rootEntities.end());
    }

    for (Entity* member : hierarchy) {
        entities.erase(member->getName());
        if (member->isMovable()) {
            movableEntities.erase(std::remove(movableEntities.begin(), movableEntities.end(), member), movableEntities.end());
        }
        if (Light* light = dynamic_cast<Light*>(member)) {
            dirtyLights.erase(std::remove(dirtyLights.begin(), dirtyLights.end(), light), dirtyLights.end());
            allLights.erase(std::remove(allLights.begin(), allLights.end(), light), allLights.end());
        }
    }

    delete entity;
}

void EntityManager::updateAll(float deltaTime) {
    for (auto& pair : entities) {
        pair.second->update(deltaTime);
    }
}

void EntityManager::shutdown() {
    std::vector<std::string> roots;
    roots.reserve(rootEntities.size());
    for (Entity* root : rootEntities) {
        if (root) {
            roots.push_back(root->getName());
        }
    }

    for (const std::string& rootName : roots) {
        removeEntity(rootName);
    }

    entities.clear();
    rootEntities.clear();
    movableEntities.clear();
    dirtyLights.clear();
    allLights.clear();
}

std::vector<Light*> EntityManager::getDirtyLights() {
    std::vector<Light*> dirty;
    std::vector<Light*> newDirty;
    for (auto& light : dirtyLights) {
        dirty.push_back(light);
        if (light->isMovable()) {
            newDirty.push_back(light);
        }
    }
    dirtyLights = newDirty;
    return dirty;
}
