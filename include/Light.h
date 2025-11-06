#pragma once
#include <Entity.h>
#include <glm/glm.hpp>
#include <ShaderManager.h>


class Light : public Entity {
public:
    Light(const std::string& name, float radius, const glm::vec3& color, float intensity, const glm::vec3& position = {0.0f, 0.0f, 0.0f}, const glm::vec3& rotation = {0.0f, 0.0f, 0.0f})
        : Entity(name, "", position, rotation), radius(radius), color(color), intensity(intensity) {}
    virtual ~Light() = default;

    float getRadius() const { return radius; }
    void setRadius(float r) { radius = r; }
    glm::vec3 getColor() const { return color; }
    void setColor(const glm::vec3& c) { color = c; }
    float getIntensity() const { return intensity; }
    void setIntensity(float i) { intensity = i; }

    PointLight getPointLightData() {
        PointLight pl = {
            .position = getWorldPosition(),
            .radius = radius,
            .color = color,
            .intensity = intensity
        };
        return pl;
    }

private:
    float radius;
    glm::vec3 color;
    float intensity;
};