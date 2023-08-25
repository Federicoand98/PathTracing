//
// Created by Federico Andrucci on 21/08/23.
//

#include "Renderer.h"
#include <algorithm>
#include <execution>

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

    for (uint32_t i = 0; i < m_RenderedImage->GetHeight(); i++)
        m_HeightIterator.push_back(i);

    for (uint32_t i = 0; i < m_RenderedImage->GetWidth(); i++)
        m_WidthIterator.push_back(i);
}

void Renderer::Render(const Camera& camera, const World& world) {
    m_Camera = &camera;
    m_World = &world;

#define MULTITHREADING 1
#if MULTITHREADING
    std::for_each(std::execution::par, m_HeightIterator.begin(), m_HeightIterator.end(), [this](uint32_t y) {
        std::for_each(std::execution::par, m_WidthIterator.begin(), m_WidthIterator.end(), [this, y](uint32_t x) {
            glm::vec2 coord = {(float)x / (float)m_RenderedImage->GetWidth(), (float)y / (float)m_RenderedImage->GetHeight()};
            glm::vec4 color = PerPixel(x, y);

            m_ImageData[(x + y * m_RenderedImage->GetWidth()) * 4] = Utils::ConvertToRGBA(color.x);  // R
            m_ImageData[(x + y * m_RenderedImage->GetWidth()) * 4 + 1] = Utils::ConvertToRGBA(color.y);  // G
            m_ImageData[(x + y * m_RenderedImage->GetWidth()) * 4 + 2] = Utils::ConvertToRGBA(color.z);   // B
            m_ImageData[(x + y * m_RenderedImage->GetWidth()) * 4 + 3] = 255; // Alpha
		});
    });
#else
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
#endif

    m_RenderedImage->SetData(m_ImageData);
}

glm::vec4 Renderer::PerPixel(uint32_t x, uint32_t y) {
    Ray ray;
    ray.Origin = m_Camera->GetPosition();
    ray.Direction = m_Camera->GetRayDirections()[x + y * m_RenderedImage->GetWidth()];
    glm::vec4 pixelColor;

    HitInfo hit = TraceRay(ray);

    if (hit.Type == ObjectType::BACKGROUND)
        return m_World->BackgroundColor;
    else if (hit.Type == ObjectType::SPHERE) 
		pixelColor = m_World->Spheres.at(hit.ObjectIndex).Color;
    else if (hit.Type == ObjectType::QUAD) 
		pixelColor = m_World->Quads.at(hit.ObjectIndex).Color;

    glm::vec3 lightDirection = glm::normalize(m_World->LightPosition);
    float lightIntensity = glm::max(glm::dot(hit.Normal, -lightDirection), 0.0f);

    pixelColor *= lightIntensity;

    return pixelColor;
}

Renderer::HitInfo Renderer::TraceRay(const Ray &ray) {
    HitInfo hitInfo;
    hitInfo.Type = ObjectType::BACKGROUND;
    hitInfo.HitDistance = std::numeric_limits<float>::max();
    hitInfo.ObjectIndex = -1;

    for (size_t i = 0; m_World->Spheres.size() > 0 && i < m_World->Spheres.size(); i++) {
		const Sphere& sphere = m_World->Spheres.at(i);

        float distance = sphere.Hit(ray);

        if (distance >= 0.0f && distance < hitInfo.HitDistance) {
            hitInfo.HitDistance = distance;
            hitInfo.ObjectIndex = i;
            hitInfo.Type = ObjectType::SPHERE;
        }
    }

    for (size_t i = 0; m_World->Quads.size() > 0 && i < m_World->Quads.size(); i++) {
        const Quad& quad = m_World->Quads.at(i);

        float distance = quad.Hit(ray);

        if (distance >= 0.0f && distance < hitInfo.HitDistance) {
            hitInfo.HitDistance = distance;
            hitInfo.ObjectIndex = i;
            hitInfo.Type = ObjectType::QUAD;
        }
    }

    if (hitInfo.HitDistance == std::numeric_limits<float>::max())
       return NoHit();

    return HandleHit(ray, hitInfo);
}

Renderer::HitInfo Renderer::HandleHit(const Ray &ray, HitInfo& hit) {
    glm::vec3 origin;

    if (hit.Type == ObjectType::SPHERE)
        origin = ray.Origin - m_World->Spheres.at(hit.ObjectIndex).Position;
    else if(hit.Type == ObjectType::QUAD)
        origin = ray.Origin - m_World->Quads.at(hit.ObjectIndex).PositionLLC;

    hit.HitPosition = origin + ray.Direction * hit.HitDistance;
    hit.Normal = glm::normalize(hit.HitPosition);

    return hit;
}

Renderer::HitInfo Renderer::NoHit() {
    return { -1.0f, glm::vec3(0.0f), glm::vec3(0.0f), -1, ObjectType::BACKGROUND};
}
