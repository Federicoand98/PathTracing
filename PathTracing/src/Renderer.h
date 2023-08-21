//
// Created by Federico Andrucci on 21/08/23.
//

#ifndef PATHTRACING_RENDERER_H
#define PATHTRACING_RENDERER_H

#include "Image.h"
#include "Ray.h"
#include <memory>

class Renderer {
public:
    Renderer() = default;

    void Render();
    void OnResize(uint32_t width, uint32_t height);
    std::shared_ptr<Image> GetRenderedImage() const { return m_RenderedImage; }
private:
    struct Color {
        float R, G, B, A;
    };

    glm::vec4 PerPixel(uint32_t x, uint32_t y);
    Color TraceRay(const Ray& ray);
    Color ClosestHit(const Ray& ray, float hitDistance);
    Color NoHit(const Ray& ray);
private:
    std::shared_ptr<Image> m_RenderedImage;
    unsigned char* m_ImageData;
};


#endif //PATHTRACING_RENDERER_H
