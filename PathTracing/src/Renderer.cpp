//
// Created by Federico Andrucci on 21/08/23.
//

#include "Renderer.h"

namespace Utils {
    static unsigned char ConvertToRGBA(float value) {
        return (unsigned char)(value * 255.0f);
    }
}

void Renderer::OnResize(uint32_t width, uint32_t height) {
    if(m_RenderedImage) {
        if (m_RenderedImage->GetWidth() == width && m_RenderedImage->GetHeight() == height)
            return;

        m_RenderedImage->Resize(width, height);
    } else {
        m_RenderedImage = std::make_shared<Image>(width, height);
    }

    //delete[] m_ImageData;
    m_ImageData = new unsigned char[width * height * 4];
}

void Renderer::Render(const Camera& camera, const World& world) {
    m_Camera = &camera;
    m_World = &world;

    for(uint32_t y = 0; y < m_RenderedImage->GetHeight(); y++) {
        for(uint32_t x = 0; x < m_RenderedImage->GetWidth(); x++) {
            glm::vec2 coord = {(float)x / (float)m_RenderedImage->GetWidth(), (float)y / (float)m_RenderedImage->GetHeight()};
            glm::vec4 color = PerPixel(x, y);

            m_ImageData[(x + y * m_RenderedImage->GetWidth()) * 4] = Utils::ConvertToRGBA(color.x);  // R
            m_ImageData[(x + y * m_RenderedImage->GetWidth()) * 4 + 1] = Utils::ConvertToRGBA(color.y);  // G
            m_ImageData[(x + y * m_RenderedImage->GetWidth()) * 4 + 2] = Utils::ConvertToRGBA(color.z);   // B
            m_ImageData[(x + y * m_RenderedImage->GetWidth()) * 4 + 3] = 255; // Alpha
        }
    }

    m_RenderedImage->SetData(m_ImageData);
}

glm::vec4 Renderer::PerPixel(uint32_t x, uint32_t y) {
    Ray ray;
    ray.Origin = m_Camera->GetPosition();
    ray.Direction = m_Camera->GetRayDirections()[x + y * m_RenderedImage->GetWidth()];
    glm::vec4 pixelColor;

    HitInfo hit = TraceRay(ray);

    if (hit.ObjectIndex >= 0)
        pixelColor = m_World->Spheres.at(hit.ObjectIndex).Color;
    else
       return m_World->BackgroundColor;

    /*
    float a = glm::dot(rayDirection, rayDirection);
    float b = 2.0f * glm::dot(rayOrigin, rayDirection);
    float c = glm::dot(rayOrigin, rayOrigin) - radius * radius;
    float discriminant = b * b - 4.0f * a * c;

    if(discriminant < 0.0f)
        return glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

    float closestHit = (-b - glm::sqrt(discriminant)) / (2.0f * a);
    */

    glm::vec3 hitPoint = ray.Origin + ray.Direction * hit.HitDistance;
    glm::vec3 normal = glm::normalize(hitPoint);
    glm::vec3 lightDirection = glm::normalize(glm::vec3(-1.0, -1.0, -1.0));

    float d = glm::max(glm::dot(normal, -lightDirection), 0.0f);

    pixelColor *= d;

    return pixelColor;
}

Renderer::HitInfo Renderer::TraceRay(const Ray &ray) {
    int closestIndex = -1;
    float closestDistance = std::numeric_limits<float>::max();

    for (size_t i = 0; i < m_World->Spheres.size(); i++) {
        const Sphere& sphere = m_World->Spheres.at(i);

        float distance = sphere.Hit(ray);

        if (distance >= 0.0f && distance < closestDistance) {
            closestDistance = distance;
            closestIndex = i;
        }
    }

    HitInfo hitInfo;

    if (closestDistance == std::numeric_limits<float>::max())
       return NoHit();
        

    hitInfo.HitDistance = closestDistance;
    hitInfo.ObjectIndex = closestIndex;

    return hitInfo;
}

Renderer::HitInfo Renderer::ClosestHit(const Ray &ray, float hitDistance) {
    return {};
}

Renderer::HitInfo Renderer::NoHit() {
    return { -1.0f, -1 };
}
