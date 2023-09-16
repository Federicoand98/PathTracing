#pragma once

#ifndef RENDERER_H
#define RENDERER_H

#include <memory>
#include <iostream>
#include <glm/gtx/compatibility.hpp>
#include "Image.h"
#include "Ray.h"
#include "Camera.h"
#include "Shader.h"
#include "ComputeShader.h"

class Renderer {
public:
    Renderer() = default;

    void Initialize(const Camera& camera, const World& world);
    void Render(const Camera& camera, const World& world);
    void OnResize(uint32_t width, uint32_t height);
    void ResetPathTracingCounter(bool sceneReset = false) { m_PTCounter = 1; m_sceneReset = sceneReset; }
    std::shared_ptr<Image> GetRenderedImage() const { return m_RenderedImage; }
public:
    bool PathTracing = true;
    bool PostProcessing = true;
    int m_SamplesPerPixel = 1;
    int m_RayDepth = 5;
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

    void DrawQuad();
    glm::vec4 PerPixel(uint32_t x, uint32_t y);
    HitInfo TraceRay(const Ray& ray);
    HitInfo HandleHit(const Ray& ray, HitInfo& hit);
    HitInfo NoHit();
private:
    const Camera* m_Camera = nullptr;
    const World* m_World = nullptr;
    std::shared_ptr<Image> m_RenderedImage;
    std::shared_ptr<Shader> m_Shader;
    std::shared_ptr<ComputeShader> m_ComputeShader;
    unsigned int m_QuadVAO = 0;
    unsigned int m_QuadVBO = 0;
    uint32_t m_Width = 0, m_Height = 0;
    unsigned char* m_ImageData;
    int m_PTCounter = 1;
    bool m_sceneReset = false;
};

#endif // RENDERER_H