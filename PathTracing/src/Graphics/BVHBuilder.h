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
        explicit BVH(const std::vector<Triangle>& triangles)
            : tris(&triangles) {
            const int N = static_cast<int>(triangles.size());
            if (N == 0) return;

            nodes.resize(static_cast<size_t>(2 * N));

            centroids.reserve(N);
            triangleIndices.reserve(N);
            for (int i = 0; i < N; i++) {
                centroids.push_back(glm::vec3(triangles[i].A + triangles[i].B + triangles[i].C) / 3.0f);
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

        void UpdateNodeBounds(int nodeIndex) {
            BVHNodeNew& node = nodes[nodeIndex];
            node.aabbMin = glm::vec3(1e30f);
            node.aabbMax = glm::vec3(-1e30f);

            for (int i = 0; i < node.triCount; i++) {
                const Triangle& leaf = (*tris)[triangleIndices[node.left + i]];

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
                    const Triangle& tri = (*tris)[triIdx];

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
        std::vector<glm::vec3> centroids;
        std::vector<int> triangleIndices;
        int rootNodeIndex = 0;
        int nodeUsed = 1;
        static constexpr int maxTrianglesInLeaf = 4;
    };
}

#endif // !PATHTRACING_BVH_BUILDER_H
