//
// Created by Federico Andrucci on 21/08/23.
//

#include "Renderer.h"
#include "Random.h"
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

        m_Width = width;
        m_Height = height;
        m_RenderedImage->Resize(width, height);
    } else {
        m_RenderedImage = std::make_shared<Image>(width, height);
    }
}

void Renderer::Initialize(const Camera& camera, const World& world) {
    m_Camera = &camera;
    m_World = &world;

    m_Shader = std::make_shared<Shader>("shaders/vScreenQuad.vert", "shaders/fScreenQuad.frag");
    m_ComputeShader = std::make_shared<ComputeShader>("shaders/PathTracing.comp");
    m_ComputeShader->UpdateWorldBuffer(world);

    m_Shader->use();
    m_Shader->setInt("tex", 0);
}

void Renderer::Render(const Camera& camera, const World& world) {
    m_ComputeShader->Use();
    m_ComputeShader->SetInt("width", m_Width);
    m_ComputeShader->SetInt("height", m_Height);
    m_ComputeShader->SetInt("numberOfSpheres", (int)m_World->Spheres.size());
    m_ComputeShader->SetInt("numberOfMaterials", (int)m_World->Materials.size());
    m_ComputeShader->SetInt("rendererFrame", m_PTCounter);
    m_ComputeShader->SetVec3("cameraPosition", m_Camera->GetPosition());
    m_ComputeShader->SetVec3("BackgroundColor", m_World->BackgroundColor);
    m_ComputeShader->SetMat4("inverseProjection", m_Camera->GetInverseProjection());
    m_ComputeShader->SetMat4("inverseView", m_Camera->GetInverseView());
    m_ComputeShader->SetWorld();
    m_ComputeShader->UpdateWorldBuffer(world);

	glDispatchCompute((unsigned int)m_Width, (unsigned int)m_Height, 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_Shader->use();

    DrawQuad();

    /*
    if (m_PTCounter == 1) {
        memset(m_BufferImage, 0, m_RenderedImage->GetWidth() * m_RenderedImage->GetHeight() * sizeof(glm::vec4));
    }

    std::for_each(std::execution::par, m_HeightIterator.begin(), m_HeightIterator.end(), [this](uint32_t y) {
        std::for_each(std::execution::par, m_WidthIterator.begin(), m_WidthIterator.end(), [this, y](uint32_t x) {
            glm::vec4 color = PerPixel(x, y);
            m_BufferImage[x + y * m_RenderedImage->GetWidth()] += color;

            glm::vec4 tempColor = m_BufferImage[x + y * m_RenderedImage->GetWidth()];
            tempColor /= (float)m_PTCounter;

            tempColor = glm::clamp(tempColor, glm::vec4(0.0f), glm::vec4(1.0f));

            m_ImageData[(x + y * m_RenderedImage->GetWidth()) * 4] = Utils::ConvertToRGBA(tempColor.x);  // R
            m_ImageData[(x + y * m_RenderedImage->GetWidth()) * 4 + 1] = Utils::ConvertToRGBA(tempColor.y);  // G
            m_ImageData[(x + y * m_RenderedImage->GetWidth()) * 4 + 2] = Utils::ConvertToRGBA(tempColor.z);   // B
            m_ImageData[(x + y * m_RenderedImage->GetWidth()) * 4 + 3] = 255; // Alpha
		});
    });

    m_RenderedImage->SetData(m_ImageData);
    */

    if (PathTracing)
        m_PTCounter++;
    else
        m_PTCounter = 1;
}

void Renderer::DrawQuad() {
    if (m_QuadVAO == 0) {
        float quadVertices[] = {
            // positions        // texture Coords
            -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
            -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
             1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
             1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
        };
        // setup plane VAO
        glGenVertexArrays(1, &m_QuadVAO);
        glGenBuffers(1, &m_QuadVBO);
        glBindVertexArray(m_QuadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_QuadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    }
    glBindVertexArray(m_QuadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

glm::vec4 Renderer::PerPixel(uint32_t x, uint32_t y) {
    Ray ray;
    ray.Origin = m_Camera->GetPosition();
    ray.Direction = m_Camera->GetRayDirections()[x + y * m_RenderedImage->GetWidth()];
    glm::vec4 pixelColor(0.0f);
    bool shouldRefract = false;

    int depth = 5;
    glm::vec4 light(0.0f);
    glm::vec4 contribution(1.0f);

    for (int i = 0; i < depth; i++) {
		HitInfo hit = TraceRay(ray);
		Material material;
        glm::vec4 objColor;

        if (hit.Type == ObjectType::BACKGROUND) {
			light += glm::vec4(m_World->BackgroundColor, 1.0f) * contribution * m_World->AmbientOcclusionIntensity;
            break;
        }
		else if (hit.Type == ObjectType::SPHERE) {
			material = m_World->Materials.at(m_World->Spheres.at(hit.ObjectIndex).MaterialIndex);
		}
		else if (hit.Type == ObjectType::QUAD) {
			material = m_World->Materials.at(m_World->Quads.at(hit.ObjectIndex).MaterialIndex);
		}

		contribution *= material.Color;
        light += (material.EmissiveColor * material.EmissiveStrenght);

        if (material.RefractionRatio > 1.0f) {
            glm::vec3 refractedDirection;

            glm::vec3 unitDirection = glm::normalize(ray.Direction);
            float ni_over_nt = glm::dot(unitDirection, hit.Normal) > 0 ? material.RefractionRatio : 1.0f / material.RefractionRatio;

            refractedDirection = glm::refract(unitDirection, hit.Normal, ni_over_nt);

            if (glm::length(refractedDirection)) {
                ray.Origin = hit.HitPosition + refractedDirection * 0.001f;
                ray.Direction = refractedDirection;
            }
            else {
                // Total Internal Reflection
                glm::vec3 reflectedDirection = glm::reflect(unitDirection, hit.Normal);
                ray.Origin = hit.HitPosition + reflectedDirection * 0.001f;
                ray.Direction = reflectedDirection;
            }
        }
        else {
			glm::vec3 diffuse = glm::normalize(hit.Normal + Random::GetVec3(-1.0f, 1.0f));
			glm::vec3 specular = glm::reflect(ray.Direction, hit.Normal);

			ray.Origin = hit.HitPosition + hit.Normal * 0.001f; // shadow acne fix
			ray.Direction = glm::normalize(glm::lerp(specular, diffuse, material.Roughness));
        }
    }

    return light;
}

Renderer::HitInfo Renderer::TraceRay(const Ray &ray) {
    HitInfo hitInfo;
    hitInfo.Type = ObjectType::BACKGROUND;
    hitInfo.HitDistance = std::numeric_limits<float>::max();
    hitInfo.ObjectIndex = -1;

    for (size_t i = 0; m_World->Spheres.size() > 0 && i < m_World->Spheres.size(); i++) {
		const Sphere& sphere = m_World->Spheres.at(i);

        //float distance = sphere.Hit(ray);
        float distance = 5;

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
    glm::vec3 closestPosition;

    if (hit.Type == ObjectType::SPHERE) {
        origin = ray.Origin - m_World->Spheres.at(hit.ObjectIndex).Position;
        closestPosition = m_World->Spheres.at(hit.ObjectIndex).Position;

		hit.HitPosition = origin + ray.Direction * hit.HitDistance;
		hit.Normal = glm::normalize(hit.HitPosition);
		hit.HitPosition += closestPosition;
    }
    else if (hit.Type == ObjectType::QUAD) {
        glm::vec3 U = m_World->Quads.at(hit.ObjectIndex).U;
        glm::vec3 V = m_World->Quads.at(hit.ObjectIndex).V;

        origin = ray.Origin - m_World->Quads.at(hit.ObjectIndex).PositionLLC;
        closestPosition = m_World->Quads.at(hit.ObjectIndex).PositionLLC;
        
		hit.HitPosition = origin + ray.Direction * hit.HitDistance;
        hit.Normal = normalize(glm::cross(U, V));
		hit.HitPosition += closestPosition;
    }

    return hit;
}

Renderer::HitInfo Renderer::NoHit() {
    return { -1.0f, glm::vec3(0.0f), glm::vec3(0.0f), -1, ObjectType::BACKGROUND};
}
