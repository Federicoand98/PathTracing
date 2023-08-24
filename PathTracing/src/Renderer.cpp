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

    if (hit.Type == ObjectType::BACKGROUND)
        return m_World->BackgroundColor;
    else if (hit.Type == ObjectType::SPHERE) 
		pixelColor = m_World->Spheres.at(hit.ObjectIndex).Color;
    else if (hit.Type == ObjectType::QUAD) 
		pixelColor = m_World->Quads.at(hit.ObjectIndex).Color;

    glm::vec3 lightDirection = glm::normalize(glm::vec3(-1.0, -1.0, -1.0));
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

    for (rsize_t i = 0; m_World->Quads.size() > 0 && i < m_World->Quads.size(); i++) {
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

    glm::vec3 origin;
    if (hitInfo.Type == ObjectType::SPHERE)
        origin = ray.Origin - m_World->Spheres.at(hitInfo.ObjectIndex).Position;
    else if(hitInfo.Type == ObjectType::QUAD)
        origin = ray.Origin - m_World->Quads.at(hitInfo.ObjectIndex).PositionLLC;

    hitInfo.HitPosition = origin + ray.Direction * hitInfo.HitDistance;
    hitInfo.Normal = glm::normalize(hitInfo.HitPosition);

    return hitInfo;
}

Renderer::HitInfo Renderer::ClosestHit(const Ray &ray, float hitDistance) {
    return {};
}

Renderer::HitInfo Renderer::NoHit() {
    return { -1.0f, glm::vec3(0.0f), glm::vec3(0.0f), -1, ObjectType::BACKGROUND};
}
