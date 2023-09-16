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
    m_ComputeShader->SetInt("rendererFrame", m_PTCounter);
    m_ComputeShader->SetInt("samplesPerPixel", m_SamplesPerPixel);
    m_ComputeShader->SetInt("rayDepth", m_RayDepth);
    m_ComputeShader->SetVec3("cameraPosition", m_Camera->GetPosition());
    m_ComputeShader->SetVec3("BackgroundColor", m_World->BackgroundColor);
    m_ComputeShader->SetMat4("inverseProjection", m_Camera->GetInverseProjection());
    m_ComputeShader->SetMat4("inverseView", m_Camera->GetInverseView());
    m_ComputeShader->SetWorld();

    if(m_PTCounter == 1)
		m_ComputeShader->UpdateWorldBuffer(world, m_sceneReset);

    m_sceneReset = false;

	glDispatchCompute((unsigned int)m_Width / 8, (unsigned int)m_Height / 8, 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_Shader->use();
    m_Shader->setBool("PostProcessing", PostProcessing);

    DrawQuad();

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

    return light;
}

Renderer::HitInfo Renderer::TraceRay(const Ray &ray) {
    HitInfo hitInfo;
    hitInfo.Type = ObjectType::BACKGROUND;
    hitInfo.HitDistance = std::numeric_limits<float>::max();
    hitInfo.ObjectIndex = -1;

    if (hitInfo.HitDistance == std::numeric_limits<float>::max())
       return NoHit();

    return HandleHit(ray, hitInfo);
}

Renderer::HitInfo Renderer::HandleHit(const Ray &ray, HitInfo& hit) {
    glm::vec3 origin;
    glm::vec3 closestPosition;

    return hit;
}

Renderer::HitInfo Renderer::NoHit() {
    return { -1.0f, glm::vec3(0.0f), glm::vec3(0.0f), -1, ObjectType::BACKGROUND};
}
