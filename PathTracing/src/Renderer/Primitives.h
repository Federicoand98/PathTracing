//
// Created by feder on 29/09/2023.
//

#ifndef PATHTRACING_PRIMITIVES_H
#define PATHTRACING_PRIMITIVES_H

#include <glm/glm.hpp>

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

    struct Triangle {
        glm::vec4 A;
        glm::vec4 B;
        glm::vec4 C;
        glm::vec4 NormalA;
        glm::vec4 NormalB;
        glm::vec4 NormalC;
    };

    struct Face {
        int vertex_ins[3];
        int normal_ins[3];
    };

    struct MeshInfo {
        glm::vec4 Position{0.0f};
        glm::vec4 BoundsMin;
        glm::vec4 BoundsMax;
        float FirstTriangle;
        float NumTriangles;
        float MaterialIndex = 0;
        float paddind = 0.0f;
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
