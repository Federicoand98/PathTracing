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

    struct AABB {
        glm::vec3 min{ 1e30f };
        glm::vec3 max{ -1e30f };

        void expand(const glm::vec3& p) {
            min = glm::min(min, p);
            max = glm::max(max, p);
        }
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
        // -1 = eredita il materiale della mesh (mono-materiale); >= 0 = materiale esplicito
        // per triangolo, per gli OBJ multi-materiale (usemtl). Finisce in TriangleUV.A.z.
        float MaterialIndex = -1.0f;
    };

    // Sulla GPU posizioni e normali stanno in due buffer distinti: le posizioni sono anche
    // l'ingresso dei BLAS, che non devono vedere nient'altro.
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

    // Anch'esso "freddo": letto una sola volta sull'hit finale.
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
        int material = -1; // indice LOCALE nel MTL del modello, -1 = nessun usemtl
    };

    // Istanza di mesh in una struttura a due livelli: i triangoli restano in LOCAL SPACE e
    // hanno un proprio BLAS. Trasformare la mesh significa aggiornare Transform, non
    // ri-bakare i triangoli ne' ricostruire il BLAS: e' il
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
        // BuildTriangleBuffers ordina i triangoli della mesh: prima gli opachi, poi quelli con un
        // materiale mascherato. Il BLAS ne fa due geometrie (ADR 0008).
        float NumOpaque = 0;
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

    // Emettitore per la Next Event Estimation (light sampling + MIS). Costruito da
    // World::CollectLights() a partire dalle sfere/quad con materiale emissivo, caricato
    // in un SSBO (binding 11). Layout std430: solo vec4/ivec4 -> stride 96, niente padding.
    // Deve combaciare bit-per-bit con lo struct Light in PathTracing.comp.
    struct GPULight {
        glm::vec4 Emission{ 0.0f };  // rgb = radianza (EmissiveColor*Strength), a = area
        glm::vec4 P{ 0.0f };         // sfera: centro.xyz + raggio(w) ; quad: LLC.xyz
        glm::vec4 U{ 0.0f };         // quad: vettore-spigolo U (fisico) ; sfera: inutilizzato
        glm::vec4 V{ 0.0f };         // quad: vettore-spigolo V (fisico) ; sfera: inutilizzato
        glm::vec4 Normal{ 0.0f };    // quad: normale.xyz ; sfera: inutilizzato
        glm::ivec4 Meta{ 0 };        // x = tipo (0 sfera, 1 quad, 2 Sole, 3 punto, 4 spot), y = objType, z = objIndex
    };

    static_assert(sizeof(GPULight) == 96, "GPULight deve essere 96 byte per combaciare con lo std430");

    // Il Sole: l'Emitter senza geometria (ADR 0006). Angoli in gradi. Elevazione 0 = orizzonte,
    // 90 = zenit; azimut attorno all'asse y, 0 = verso +z, 90 = verso +x.
    struct SunParams {
        bool Enabled = false;
        float Elevation = 45.0f;
        float Azimuth = 0.0f;
        float AngularRadius = 0.27f;   // raggio del disco, non il diametro: e' quello del sole reale
        glm::vec3 Color{ 1.0f };
        float Irradiance = 3.0f;       // luce su una superficie perpendicolare al Sole

        // Verso il Sole, dalla superficie alla luce: la stessa convenzione di wi nella NEE.
        glm::vec3 Direction() const {
            const float el = glm::radians(Elevation), az = glm::radians(Azimuth);
            return glm::vec3(std::cos(el) * std::sin(az), std::sin(el), std::cos(el) * std::cos(az));
        }

        float CosMax() const { return std::cos(glm::radians(AngularRadius)); }
        float SolidAngle() const { return 2.0f * 3.14159265f * (1.0f - CosMax()); }

        // Radianza del disco ricavata dall'irradianza: E = L * pi * sin^2(theta) per un disco
        // uniforme visto di fronte. Cosi' allargare il disco ammorbidisce le ombre senza
        // cambiare quanta luce arriva.
        glm::vec3 Radiance() const {
            const float s = std::sin(glm::radians(AngularRadius));
            return Color * (Irradiance / (3.14159265f * s * s));
        }
    };

    // Luce puntiforme o spot (ADR 0009): un Emitter delta, senza geometria. Non la colpisce
    // nessun raggio, quindi illumina solo tramite la NEE, con peso MIS 1.
    struct PunctualLight {
        bool Spot = false;
        glm::vec3 Position{ 0.0f };
        glm::vec3 Direction{ 0.0f, -1.0f, 0.0f }; // dove punta lo spot; ignorata dalla luce punto
        glm::vec3 Color{ 1.0f };
        float Intensity = 10.0f;     // intensita' radiante (W/sr): a distanza d arriva Intensity / d^2
        float InnerAngle = 30.0f;    // gradi dall'asse: dentro, luce piena
        float OuterAngle = 45.0f;    // gradi dall'asse: fuori, buio; in mezzo la sfumatura glTF
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
