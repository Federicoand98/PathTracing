//
// Created by Federico Andrucci on 21/08/23.
//

#ifndef PATHTRACING_RENDERER_H
#define PATHTRACING_RENDERER_H

#include "Image.h"
#include "Ray.h"
#include "Camera.h"
#include "World.h"
#include "Material.h"
#include <memory>
#include <iostream>

class Renderer {
public:
    Renderer() = default;

    void Render(const Camera& camera, const World& world);
    void OnResize(uint32_t width, uint32_t height);
    std::shared_ptr<Image> GetRenderedImage() const { return m_RenderedImage; }
private:
    struct Color {
        float R, G, B, A;
    };

    struct HitInfo {
        float HitDistance;
        glm::vec3 HitPosition;
        glm::vec3 Normal;
        int ObjectIndex;
        ObjectType Type;
    };

    glm::vec4 PerPixel(uint32_t x, uint32_t y);
    HitInfo TraceRay(const Ray& ray);
    HitInfo HandleHit(const Ray& ray, HitInfo& hit);
    HitInfo NoHit();
private:
    const Camera* m_Camera = nullptr;
    const World* m_World = nullptr;
    std::shared_ptr<Image> m_RenderedImage;
    unsigned char* m_ImageData;
    std::vector<uint32_t> m_HeightIterator;
    std::vector<uint32_t> m_WidthIterator;
};

#endif //PATHTRACING_RENDERER_H