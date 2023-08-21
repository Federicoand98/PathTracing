//
// Created by Federico Andrucci on 21/08/23.
//

#include "Renderer.h"

void Renderer::OnResize(uint32_t width, uint32_t height) {
    if(m_RenderedImage) {
        if (m_RenderedImage->GetWidth() == width && m_RenderedImage->GetHeight() == height)
            return;

        m_RenderedImage->Resize(width, height);
    } else {
        m_RenderedImage = std::make_shared<Image>(width, height);
    }

    delete[] m_ImageData;
    m_ImageData = new unsigned char[width * height];
}

void Renderer::Render() {
    for(uint32_t y = 0; y < m_RenderedImage->GetHeight(); y++) {
        for(uint32_t x = 0; x < m_RenderedImage->GetWidth(); x++) {
            glm::vec4 color = PerPixel(x, y);
        }
    }
}

glm::vec4 Renderer::PerPixel(uint32_t x, uint32_t y) {
    glm::vec3 rayOrigin(0.0f);
    glm::vec3 rayDirection(x, y, -1.0f);
    float radius = 0.5f;

    float a = glm::dot(rayDirection, rayDirection);
    float b = 2.0f * glm::dot(rayOrigin, rayDirection);
    float c = glm::dot(rayOrigin, rayOrigin) - radius * radius;
    float discriminant = b * b - 4.0f * a * c;

    if(discriminant >= 0.0f)
        return glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);

    return glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
}

glm::vec4 Renderer::TraceRay(const Ray &ray) {
    return glm::vec4(0.0f);
}

glm::vec4 Renderer::ClosestHit(const Ray &ray, float hitDistance) {
    return glm::vec4(0.0f);
}

glm::vec4 Renderer::NoHit(const Ray &ray) {
    return glm::vec4(0.0f);
}