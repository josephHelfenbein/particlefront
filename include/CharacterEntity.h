#pragma once
#include <algorithm>
#include <cmath>
#include <Entity.h>
#include <Collider.h>
#include <glm/gtc/matrix_transform.hpp>

struct collision {
    Collider* other = nullptr;
    glm::vec3 mtv = glm::vec3(0.0f);
};

class CharacterEntity : public Entity {
public:
    CharacterEntity(
        const std::string& name,
        const std::string& shader,
        const glm::vec3& position = {0.0f, 0.0f, 0.0f},
        const glm::vec3& rotation = {0.0f, 0.0f, 0.0f}
    ) : Entity(name, shader, position, rotation) {
        this->setMovable(true);
    }
    void update(float deltaTime) override;
    void move(const glm::vec3& delta);
    void stopMove(const glm::vec3& delta);
    void jump();
    void resetVelocity();
    void rotate(const glm::vec3& delta);
private:
    glm::vec3 velocity = glm::vec3(0.0f);
    glm::vec3 pressed = glm::vec3(0.0f);
    const float moveSpeed = 10.0f;
    const float jumpSpeed = 10.0f;
    const float groundedNormalThreshold = 0.5f;
    const float coyoteTime = 0.10f;
    bool grounded = false;
    float groundedTimer = 1.0f;

    static bool aabbIntersects(const ColliderAABB& a, const ColliderAABB& b, float margin = 0.0f) {
        if (a.min.x > b.max.x + margin || a.max.x < b.min.x - margin) return false;
        if (a.min.y > b.max.y + margin || a.max.y < b.min.y - margin) return false;
        if (a.min.z > b.max.z + margin || a.max.z < b.min.z - margin) return false;
        return true;
    }

    collision willCollide(const glm::vec3& deltaPos, const glm::vec3& deltaRot = glm::vec3(0.0f));
};