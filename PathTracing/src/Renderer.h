//
// Created by Federico Andrucci on 21/08/23.
//

#ifndef PATHTRACING_RENDERER_H
#define PATHTRACING_RENDERER_H

#include "Image.h"
#include "Ray.h"
#include "Camera.h"
#include <memory>
#include <iostream>

class Renderer {
public:
    Renderer() = default;

    void Render(const Camera& camera);
    void OnResize(uint32_t width, uint32_t height);
    std::shared_ptr<Image> GetRenderedImage() const { return m_RenderedImage; }
private:
    struct Color {
        float R, G, B, A;
    };

    glm::vec4 PerPixel(uint32_t x, uint32_t y);
    glm::vec4 TraceRay(const Ray& ray);
    glm::vec4 ClosestHit(const Ray& ray, float hitDistance);
    glm::vec4 NoHit(const Ray& ray);
private:
    const Camera* m_Camera = nullptr;
    std::shared_ptr<Image> m_RenderedImage;
    unsigned char* m_ImageData;
};

#endif //PATHTRACING_RENDERER_H