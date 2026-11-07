//
// Created by feder on 29/09/2023.
//

#ifndef PATHTRACING_PRIMITIVES_H
#define PATHTRACING_PRIMITIVES_H

#include "ptpch.h"
#include "Core/Fields.h"
#include "Core/ObjectId.h"

namespace Lucerna {

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

    struct Face {
        int vertex_ins[3];
        int uv_ins[3];
        int normal_ins[3];
        int material = -1; // indice LOCALE nel MTL del modello, -1 = nessun usemtl
    };

    // Un'istanza di mesh (issue #26): dove sta e quale asset di geometria mostra. Due istanze
    // dello stesso asset condividono triangoli e BLAS; trasformarle cambia solo Transform.
    struct Mesh {
        glm::mat4 Transform{ 1.0f };
        ObjectId Asset;     // un MeshAsset della AssetLibrary
        ObjectId Material;  // per i triangoli dell'asset senza slot (MaterialSlot = -1)
    };

    constexpr auto JsonFields(const Mesh*) {
        return std::make_tuple(Field("transform", &Mesh::Transform), Field("asset", &Mesh::Asset), Field("material", &Mesh::Material));
    }

    struct Sphere {
        glm::vec4 Position{0.0f}; // (x, y, z, RADIUS)
        ObjectId Material;
    };

    struct Quad {
        glm::vec4 PositionLLC{0.0f};
        glm::vec4 U{0.0f};
        glm::vec4 V{0.0f};
        ObjectId Material;
        float Width = 1.0f;
        float Height = 1.0f;
    };

    constexpr auto JsonFields(const Sphere*) {
        return std::make_tuple(Field("position", &Sphere::Position), Field("material", &Sphere::Material));
    }

    constexpr auto JsonFields(const Quad*) {
        using Q = Quad;
        return std::make_tuple(Field("posLLC", &Q::PositionLLC), Field("u", &Q::U), Field("v", &Q::V),
                               Field("width", &Q::Width), Field("height", &Q::Height), Field("material", &Q::Material));
    }

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

    // La camera della scena (ADR 0011): l'inquadratura, salvata con la scena. La viewport ha
    // la sua camera di navigazione, che ne prende in prestito l'obiettivo.
    struct SceneCamera {
        glm::vec3 Position{ 0.0f, 0.0f, 6.0f };
        glm::vec3 Direction{ 0.0f, 0.0f, -1.0f };
        float VerticalFOV = 45.0f;   // gradi
        // Obiettivo thin-lens
        float Aperture = 0.0f;       // raggio della lente; 0 = pinhole, niente profondita' di campo
        float FocusDistance = 5.0f;  // distanza del piano di fuoco

        bool SamePose(const SceneCamera& o) const { return Position == o.Position && Direction == o.Direction; }
    };

    constexpr auto JsonFields(const SceneCamera*) {
        using C = SceneCamera;
        return std::make_tuple(Field("position", &C::Position), Field("direction", &C::Direction),
                               Field("verticalFov", &C::VerticalFOV), Field("aperture", &C::Aperture),
                               Field("focusDistance", &C::FocusDistance));
    }

    constexpr auto JsonFields(const SunParams*) {
        using S = SunParams;
        return std::make_tuple(Field("enabled", &S::Enabled), Field("elevation", &S::Elevation), Field("azimuth", &S::Azimuth),
                               Field("angularRadius", &S::AngularRadius), Field("color", &S::Color), Field("irradiance", &S::Irradiance));
    }

    // Nel file il tipo e' una stringa, "point" o "spot": piu' leggibile del bool, ed estendibile.
    constexpr auto JsonFields(const PunctualLight*) {
        using L = PunctualLight;
        return std::make_tuple(
            Custom<L>("type", [](const L& l) { return std::string(l.Spot ? "spot" : "point"); },
                              [](L& l, const std::string& s) { l.Spot = s == "spot"; }),
            Field("position", &L::Position), Field("direction", &L::Direction), Field("color", &L::Color),
            Field("intensity", &L::Intensity), Field("innerAngle", &L::InnerAngle), Field("outerAngle", &L::OuterAngle));
    }

    struct Box {
        glm::vec4 Min{ 0.0f };
        glm::vec4 Max{ 0.0f };
        float index = 0;    // il primo dei suoi 6 quad, consecutivi
        ObjectId Material;

        // Riscrive le 6 facce del box nella lista dei quad (un ObjectList, o un vettore di Quad)
        template<class QuadList>
        void UpdateBox(QuadList& Quads) {
            glm::vec3 dx = glm::vec3(Max.x - Min.x, 0, 0);
            glm::vec3 dy = glm::vec3(0, Max.y - Min.y, 0);
            glm::vec3 dz = glm::vec3(0, 0, Max.z - Min.z);

            glm::vec3 nx = glm::normalize(dx);
            glm::vec3 ny = glm::normalize(dy);
            glm::vec3 nz = glm::normalize(dz);

            Quad quad;

            quad.Material = Material;

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

    // Le sue 6 facce non si salvano: le rigenera CreateBox al load
    constexpr auto JsonFields(const Box*) {
        return std::make_tuple(Field("min", &Box::Min), Field("max", &Box::Max), Field("material", &Box::Material));
    }
}

#endif //PATHTRACING_PRIMITIVES_H
