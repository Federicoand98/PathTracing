//
// Created by Federico Andrucci on 21/08/23.
//

#include "Renderer.h"

void Renderer::Render() {

}

void Renderer::OnResize(uint32_t width, uint32_t height) {

}

glm::vec4 Renderer::PerPixel(uint32_t x, uint32_t y) {
    return glm::vec4();
}

Renderer::Color Renderer::TraceRay(const Ray &ray) {
    return Renderer::Color();
}

Renderer::Color Renderer::ClosestHit(const Ray &ray, float hitDistance) {
    return Renderer::Color();
}

Renderer::Color Renderer::NoHit(const Ray &ray) {
    return Renderer::Color();
}
