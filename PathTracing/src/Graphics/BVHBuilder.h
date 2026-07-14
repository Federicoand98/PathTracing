#ifndef PATHTRACING_BVH_BUILDER_H
#define PATHTRACING_BVH_BUILDER_H

#include "ptpch.h"
#include "../Renderer/Primitives.h"
#include "../Graphics/Model.h"

namespace PathTracer {

    struct AABB {
        glm::vec3 min{ 1e30f };
        glm::vec3 max{ -1e30f };

        void expand(const glm::vec3& p) {
            min = glm::min(min, p);
            max = glm::max(max, p);
        }

        void expand(const AABB& other) {
            min = glm::min(min, other.min);
            max = glm::max(max, other.max);
        }

        // meta' della superficie della box; usata come costo nella SAH
        float area() const {
            glm::vec3 e = max - min;
            if (e.x < 0.0f) return 0.0f; // box vuota
            return e.x * e.y + e.y * e.z + e.z * e.x;
        }
    };

    // 32 byte. In std430 vec3 ha allineamento 16 ma dimensione 12: l'int successivo
    // entra nel padding, quindi il layout C++ e quello GLSL coincidono senza buchi.
    struct BVHNodeNew {
        glm::vec3 aabbMin{ 1e30f };
        int left = 0;      // foglia: primo indice in triangleIndices | interno: figlio sinistro
        glm::vec3 aabbMax{ -1e30f };
        int triCount = 0;  // 0 => nodo interno
    };

    static_assert(sizeof(BVHNodeNew) == 32, "BVHNodeNew deve essere 32 byte per combaciare con lo std430 dello shader");

    class BVH {
    public:
        // Costruisce un BVH sul sotto-range [firstTriangle, firstTriangle + count) di
        // 'triangles'. Gli indici restituiti da GetTrianglesIndices() sono LOCALI al
        // range (0..count-1): chi chiama li rimappa a indici globali.
        BVH(const std::vector<Triangle>& triangles, int firstTriangle, int count)
            : tris(&triangles), firstTri(firstTriangle) {
            const int N = count;
            if (N == 0) return;

            nodes.resize(static_cast<size_t>(2 * N));

            centroids.reserve(N);
            triangleIndices.reserve(N);
            for (int i = 0; i < N; i++) {
                const Triangle& t = triangles[firstTriangle + i];
                centroids.push_back(glm::vec3(t.A + t.B + t.C) / 3.0f);
                triangleIndices.push_back(i);
            }

            BVHNodeNew& root = nodes[rootNodeIndex];
            root.left = 0;
            root.triCount = N;

            UpdateNodeBounds(rootNodeIndex);
            Subdivide(rootNodeIndex);

            nodes.resize(nodeUsed); // scarta i nodi preallocati non usati
        }

        std::vector<BVHNodeNew>& GetNodes() { return nodes; }
        std::vector<int>& GetTrianglesIndices() { return triangleIndices; }

    private:
        struct Bin {
            AABB bounds;
            int triCount = 0;
        };

        // triangleIndices contiene indici locali al range: qui li si riporta a globali
        const Triangle& Tri(int localIndex) const { return (*tris)[firstTri + localIndex]; }

        void UpdateNodeBounds(int nodeIndex) {
            BVHNodeNew& node = nodes[nodeIndex];
            node.aabbMin = glm::vec3(1e30f);
            node.aabbMax = glm::vec3(-1e30f);

            for (int i = 0; i < node.triCount; i++) {
                const Triangle& leaf = Tri(triangleIndices[node.left + i]);

                node.aabbMin = glm::min(node.aabbMin, glm::vec3(leaf.A));
                node.aabbMin = glm::min(node.aabbMin, glm::vec3(leaf.B));
                node.aabbMin = glm::min(node.aabbMin, glm::vec3(leaf.C));
                node.aabbMax = glm::max(node.aabbMax, glm::vec3(leaf.A));
                node.aabbMax = glm::max(node.aabbMax, glm::vec3(leaf.B));
                node.aabbMax = glm::max(node.aabbMax, glm::vec3(leaf.C));
            }
        }

        // Costo di tenere il nodo come foglia: n. triangoli * area della sua box.
        float NodeCost(const BVHNodeNew& node) const {
            glm::vec3 e = node.aabbMax - node.aabbMin;
            return node.triCount * (e.x * e.y + e.y * e.z + e.z * e.x);
        }

        // Surface Area Heuristic "binned": invece di provare ogni centroide come piano
        // di taglio (O(n^2)), si distribuiscono i triangoli in BINS intervalli lungo
        // l'asse e si valutano solo i BINS-1 piani fra un bin e l'altro. Il costo di un
        // piano e' (n_sx * area_sx + n_dx * area_dx): approssima la probabilita' che un
        // raggio colpisca ciascun lato, pesata per quanti triangoli dovra' testare.
        float FindBestSplitPlane(const BVHNodeNew& node, int& bestAxis, float& bestPos) const {
            float bestCost = 1e30f;
            bestAxis = -1;
            bestPos = 0.0f;

            for (int axis = 0; axis < 3; axis++) {
                float boundsMin = 1e30f, boundsMax = -1e30f;
                for (int i = 0; i < node.triCount; i++) {
                    const glm::vec3& c = centroids[triangleIndices[node.left + i]];
                    boundsMin = std::min(boundsMin, c[axis]);
                    boundsMax = std::max(boundsMax, c[axis]);
                }

                if (boundsMin == boundsMax) continue; // niente estensione su questo asse

                Bin bins[BINS];
                float scale = BINS / (boundsMax - boundsMin);

                for (int i = 0; i < node.triCount; i++) {
                    int triIdx = triangleIndices[node.left + i];
                    const Triangle& tri = Tri(triIdx);

                    int binIdx = std::min(BINS - 1, static_cast<int>((centroids[triIdx][axis] - boundsMin) * scale));
                    bins[binIdx].triCount++;
                    bins[binIdx].bounds.expand(glm::vec3(tri.A));
                    bins[binIdx].bounds.expand(glm::vec3(tri.B));
                    bins[binIdx].bounds.expand(glm::vec3(tri.C));
                }

                // sweep dai due lati per avere, per ogni piano, area e conteggio cumulati
                float leftArea[BINS - 1]{}, rightArea[BINS - 1]{};
                int leftCount[BINS - 1]{}, rightCount[BINS - 1]{};
                AABB leftBox, rightBox;
                int leftSum = 0, rightSum = 0;

                for (int i = 0; i < BINS - 1; i++) {
                    leftSum += bins[i].triCount;
                    leftCount[i] = leftSum;
                    leftBox.expand(bins[i].bounds);
                    leftArea[i] = leftBox.area();

                    rightSum += bins[BINS - 1 - i].triCount;
                    rightCount[BINS - 2 - i] = rightSum;
                    rightBox.expand(bins[BINS - 1 - i].bounds);
                    rightArea[BINS - 2 - i] = rightBox.area();
                }

                float binWidth = (boundsMax - boundsMin) / BINS;
                for (int i = 0; i < BINS - 1; i++) {
                    float planeCost = leftCount[i] * leftArea[i] + rightCount[i] * rightArea[i];
                    if (planeCost > 0.0f && planeCost < bestCost) {
                        bestCost = planeCost;
                        bestAxis = axis;
                        bestPos = boundsMin + binWidth * (i + 1);
                    }
                }
            }

            return bestCost;
        }

        void Subdivide(int nodeIndex) {
            BVHNodeNew& node = nodes[nodeIndex];
            if (node.triCount <= maxTrianglesInLeaf) return;

            int axis;
            float splitPos;
            float splitCost = FindBestSplitPlane(node, axis, splitPos);

            if (axis < 0) return;
            if (splitCost >= NodeCost(node)) return; // conviene lasciarlo foglia

            // partizionamento in-place attorno al piano scelto
            int i = node.left;
            int j = i + node.triCount - 1;
            while (i <= j) {
                if (centroids[triangleIndices[i]][axis] < splitPos)
                    i++;
                else
                    std::swap(triangleIndices[i], triangleIndices[j--]);
            }

            int leftCount = i - node.left;
            if (leftCount == 0 || leftCount == node.triCount) return;

            int leftChildIndex = nodeUsed++;
            int rightChildIndex = nodeUsed++;

            nodes[leftChildIndex].left = node.left;
            nodes[leftChildIndex].triCount = leftCount;
            nodes[rightChildIndex].left = i;
            nodes[rightChildIndex].triCount = node.triCount - leftCount;

            node.left = leftChildIndex;
            node.triCount = 0;

            UpdateNodeBounds(leftChildIndex);
            UpdateNodeBounds(rightChildIndex);

            Subdivide(leftChildIndex);
            Subdivide(rightChildIndex);
        }

    private:
        static constexpr int BINS = 12;

        std::vector<BVHNodeNew> nodes;
        const std::vector<Triangle>* tris = nullptr; // riferimento: niente copia da 18 MB
        int firstTri = 0;
        std::vector<glm::vec3> centroids;
        std::vector<int> triangleIndices;
        int rootNodeIndex = 0;
        int nodeUsed = 1;
        static constexpr int maxTrianglesInLeaf = 4;
    };

    // ---- Wide BVH (4 vie) -----------------------------------------------------
    // Nodo a 4 figli in layout SoA (Structure of Arrays): le 4 AABB sono
    // impacchettate per componente, cosi' lo shader testa i 4 box con una manciata
    // di operazioni vec4 invece di 4 slab test scalari. Dimezza i livelli rispetto
    // al binario -> meno fetch di nodi e meno latenza di memoria per raggio.
    //
    // 128 byte, tutti membri di 16 byte: gli offset cadono naturalmente a multipli
    // di 16, quindi il layout C++ e quello std430 dello shader coincidono senza padding.
    struct BVH4Node {
        glm::vec4 bminx, bminy, bminz;   // min.{x,y,z} dei 4 figli (componente per componente)
        glm::vec4 bmaxx, bmaxy, bmaxz;   // max.{x,y,z} dei 4 figli
        glm::ivec4 child;  // interno: indice del nodo figlio | foglia: primo indice in TriIndex
        glm::ivec4 count;  // 0 = figlio interno, >0 = foglia con N triangoli, <0 = slot vuoto
    };

    static_assert(sizeof(BVH4Node) == 128, "BVH4Node deve essere 128 byte per lo std430 dello shader");

    // Collassa un BVH binario (BVHNodeNew, radice a 0) in un BVH a 4 vie. Ogni nodo
    // wide raccoglie fino a 4 "nipoti" del nodo binario di partenza: si parte dai 2
    // figli e si espande ricorsivamente il figlio interno di area maggiore finche' non
    // si arriva a 4 (o restano solo foglie). Gli indici prodotti sono LOCALI al BLAS:
    // World li rimappa ai globali (child interni += offset nodi, foglie += offset TriIndex).
    class WideBVH {
    public:
        static std::vector<BVH4Node> Collapse(const std::vector<BVHNodeNew>& bin, int& outMaxDepth) {
            std::vector<BVH4Node> wide;
            outMaxDepth = 0;
            if (bin.empty()) return wide;
            wide.reserve(bin.size()); // il wide ha meno nodi del binario
            BuildWide(bin, 0, wide, 1, outMaxDepth);
            return wide;
        }

    private:
        static float SurfaceArea(const BVHNodeNew& n) {
            glm::vec3 e = n.aabbMax - n.aabbMin;
            return e.x * e.y + e.y * e.z + e.z * e.x;
        }

        // Crea il nodo wide per il sottoalbero binario 'binIndex' e ritorna il suo indice
        // in 'wide'. Riserva prima il proprio slot (wi), poi ricorre: la ricorsione fa
        // crescere 'wide', ma wi resta valido perche' e' un indice, non un puntatore.
        static int BuildWide(const std::vector<BVHNodeNew>& bin, int binIndex,
                             std::vector<BVH4Node>& wide, int depth, int& maxDepth) {
            maxDepth = std::max(maxDepth, depth);
            const int wi = static_cast<int>(wide.size());
            wide.emplace_back();

            // raccogli fino a 4 figli binari
            std::vector<int> children;
            if (bin[binIndex].triCount > 0) {
                children.push_back(binIndex); // sottoalbero degenere: una sola foglia
            } else {
                children.push_back(bin[binIndex].left);
                children.push_back(bin[binIndex].left + 1);
                while (children.size() < 4) {
                    int bestSlot = -1; float bestArea = -1.0f;
                    for (int s = 0; s < static_cast<int>(children.size()); s++) {
                        const BVHNodeNew& c = bin[children[s]];
                        if (c.triCount == 0) { // interno: espandibile
                            float a = SurfaceArea(c);
                            if (a > bestArea) { bestArea = a; bestSlot = s; }
                        }
                    }
                    if (bestSlot < 0) break; // restano solo foglie
                    int ci = children[bestSlot];
                    children[bestSlot] = bin[ci].left;    // sostituisci col figlio sinistro...
                    children.push_back(bin[ci].left + 1); // ...e aggiungi il destro
                }
            }

            BVH4Node out{};
            for (int s = 0; s < 4; s++) {
                if (s >= static_cast<int>(children.size())) { out.count[s] = -1; continue; } // vuoto

                const BVHNodeNew& c = bin[children[s]];
                out.bminx[s] = c.aabbMin.x; out.bminy[s] = c.aabbMin.y; out.bminz[s] = c.aabbMin.z;
                out.bmaxx[s] = c.aabbMax.x; out.bmaxy[s] = c.aabbMax.y; out.bmaxz[s] = c.aabbMax.z;

                if (c.triCount > 0) {          // foglia: indici invariati dal binario
                    out.child[s] = c.left;
                    out.count[s] = c.triCount;
                } else {                        // interno: ricorri
                    out.child[s] = BuildWide(bin, children[s], wide, depth + 1, maxDepth);
                    out.count[s] = 0;
                }
            }
            wide[wi] = out;
            return wi;
        }
    };
}

#endif // !PATHTRACING_BVH_BUILDER_H
