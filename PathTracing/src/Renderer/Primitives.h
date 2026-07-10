//
// Created by feder on 29/09/2023.
//

#ifndef PATHTRACING_PRIMITIVES_H
#define PATHTRACING_PRIMITIVES_H

#include "ptpch.h"

namespace PathTracer {

    enum class ObjectType {
        SPHERE,
        QUAD,
        BACKGROUND
    };

    struct Test {
        glm::vec3 a;
        glm::vec3 b;
    };

    // Struttura di authoring lato CPU: comoda da costruire, mai caricata sulla GPU.
    struct Triangle {
        glm::vec4 A;
        glm::vec4 B;
        glm::vec4 C;
        glm::vec4 NormalA;
        glm::vec4 NormalB;
        glm::vec4 NormalC;
        glm::vec2 UVA{ 0.0f };
        glm::vec2 UVB{ 0.0f };
        glm::vec2 UVC{ 0.0f };
    };

    // Sulla GPU posizioni e normali stanno in due buffer distinti: il loop caldo del
    // BVH legge solo le posizioni, dimezzando la banda nel punto piu' critico.
    struct TrianglePosition {
        glm::vec4 A;
        glm::vec4 B;
        glm::vec4 C;
    };

    struct TriangleNormal {
        glm::vec4 NormalA;
        glm::vec4 NormalB;
        glm::vec4 NormalC;
    };

    // Anch'esso "freddo": letto una sola volta sull'hit finale, mai nel loop del BVH.
    // La uv sta in .xy; i vec4 evitano sorprese di stride con lo std430.
    struct TriangleUV {
        glm::vec4 A{ 0.0f };
        glm::vec4 B{ 0.0f };
        glm::vec4 C{ 0.0f };
    };

    struct Face {
        int vertex_ins[3];
        int uv_ins[3];
        int normal_ins[3];
    };

    // Istanza di mesh in un BVH a due livelli: i triangoli restano in LOCAL SPACE e
    // hanno un proprio BVH (BLAS) con radice RootNode. Trasformare la mesh significa
    // aggiornare Transform, non ri-bakare i triangoli ne' ricostruire il BVH: e' il
    // raggio a essere portato nello spazio locale della mesh.
    struct MeshInfo {
        glm::mat4 Transform{ 1.0f };
        glm::mat4 InvTransform{ 1.0f };
        glm::vec4 BoundsMin{ 0.0f };  // AABB in world space (per il reject rapido)
        glm::vec4 BoundsMax{ 0.0f };
        glm::vec4 LocalMin{ 0.0f };   // AABB in local space (per ricalcolare quella world)
        glm::vec4 LocalMax{ 0.0f };
        float FirstTriangle = 0;
        float NumTriangles = 0;
        float MaterialIndex = 0;
        float RootNode = 0;           // indice della radice del BLAS in BVHNodes
    };

    struct Sphere {
        glm::vec4 Position{0.0f}; // (x, y, z, RADIUS)
        float MaterialIndex = 0;
        glm::vec3 padding{ 0.0f };
    };

    struct Quad {
        glm::vec4 PositionLLC{0.0f};
        glm::vec4 U{0.0f};
        glm::vec4 V{0.0f};
        float MaterialIndex = 0;
        float Width = 1.0f;
        float Height = 1.0f;
        float padding = 0.0f;
    };

    struct Box {
        glm::vec4 Min{ 0.0f };
        glm::vec4 Max{ 0.0f };
        float index = 0;
        float MaterialIndex = 0;
        glm::vec2 padding{ 0.0f };

        void UpdateBox(std::vector<Quad> &Quads) {
            glm::vec3 dx = glm::vec3(Max.x - Min.x, 0, 0);
            glm::vec3 dy = glm::vec3(0, Max.y - Min.y, 0);
            glm::vec3 dz = glm::vec3(0, 0, Max.z - Min.z);

            glm::vec3 nx = glm::normalize(dx);
            glm::vec3 ny = glm::normalize(dy);
            glm::vec3 nz = glm::normalize(dz);

            Quad quad;

            quad.MaterialIndex = MaterialIndex;

            // front
            {
                quad.PositionLLC = {Min.x, Min.y, Max.z, 0};
                quad.U = { nx, 0 };
                quad.V = { ny, 0 };
                quad.Width = dx.x;
                quad.Height = dy.y;

                Quads[index] = quad;
            }

            // right
            {
                quad.PositionLLC = { Max.x, Min.y, Max.z, 0 };
                quad.U = { -nz, 0 };
                quad.V = { ny, 0 };
                quad.Width = dz.z;
                quad.Height = dy.y;

                Quads[index+1] = quad;
            }

            // back
            {
                quad.PositionLLC = { Max.x, Min.y, Min.z, 0 };
                quad.U = { -nx, 0 };
                quad.V = { ny, 0 };
                quad.Width = dx.x;
                quad.Height = dy.y;

                Quads[index + 2] = quad;
            }

            // left
            {
                quad.PositionLLC = { Min.x, Min.y, Min.z, 0 };
                quad.U = { nz, 0 };
                quad.V = { ny, 0 };
                quad.Width = dz.z;
                quad.Height = dy.y;

                Quads[index + 3] = quad;
            }

            // top
            {
                quad.PositionLLC = { Min.x, Max.y, Max.z, 0 };
                quad.U = { nx, 0 };
                quad.V = { -nz, 0 };
                quad.Width = dx.x;
                quad.Height = dz.z;

                Quads[index + 4] = quad;
            }

            // bottom
            {
                quad.PositionLLC = { Min.x, Min.y, Min.z, 0 };
                quad.U = { nx, 0 };
                quad.V = { nz, 0 };
                quad.Width = dx.x;
                quad.Height = dz.z;

                Quads[index + 5] = quad;
            }
        }
    };
}

#endif //PATHTRACING_PRIMITIVES_H
