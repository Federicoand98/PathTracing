#ifndef PATHTRACING_BVH_BUILDER_H
#define PATHTRACING_BVH_BUILDER_H

#include "ptpch.h"
#include "../Renderer/Primitives.h"
#include "../Graphics/Model.h"

namespace PathTracer {
    
    struct AABB {
        glm::vec4 min;
        glm::vec4 max;

        AABB() {
            min = glm::vec4(1e30f);
            max = glm::vec4(-1e30f);
        }

        void expand(const glm::vec4& p) {
            min = glm::min(min, p);
            max = glm::max(max, p);
        }

        void expand(const Triangle& tri) {
            expand(tri.A);
            expand(tri.B);
            expand(tri.C);
        }
    };

    struct BVHNodeNew {
        glm::vec4 aabbMin {1e30f};
        glm::vec4 aabbMax {-1e30f};
        float left = 0.0f;
        float triCount = 0.0f;
        glm::vec2 padding {0.0f};
    };

    class BVH {
    public:
        BVH(std::vector<Triangle>& triangles) {
            // preallocate triangles.size() * 2 nodes
            nodes.reserve(triangles.size() * 2);
            for(int i = 0; i < triangles.size() * 2; i++) {
                nodes.push_back(BVHNodeNew());
            }

            for(int i = 0; i < triangles.size(); i++) {
                allTriangles.push_back(triangles[i]);
                centroids.push_back((triangles[i].A + triangles[i].B + triangles[i].C) / 3.0f);
                triangleIndices.push_back(i);
            }

            BVHNodeNew& root = nodes.at(rootNodeIndex);
            root.left = 0;
            root.triCount = allTriangles.size();

            UpdateNodeBounds(rootNodeIndex);
            Subdivide(rootNodeIndex);
        }

        void UpdateNodeBounds(int nodeIndex) {
            BVHNodeNew& node = nodes.at(nodeIndex);
            node.aabbMin = glm::vec4(1e30f);
            node.aabbMax = glm::vec4(-1e30f);

            for(int first = node.left, i = 0; i < node.triCount; i++) {
                int leafTriIdx = triangleIndices.at(first + i);
                Triangle& leaf = allTriangles.at(leafTriIdx);
                node.aabbMin = glm::min(node.aabbMin, leaf.A);
                node.aabbMin = glm::min(node.aabbMin, leaf.B);
                node.aabbMin = glm::min(node.aabbMin, leaf.C);
                node.aabbMax = glm::max(node.aabbMax, leaf.A);
                node.aabbMax = glm::max(node.aabbMax, leaf.B);
                node.aabbMax = glm::max(node.aabbMax, leaf.C);
            }
        }

        void Subdivide(int nodeIndex) {
            BVHNodeNew& node = nodes.at(nodeIndex);
            if(node.triCount <= 2) return;

            glm::vec4 extent = node.aabbMax - node.aabbMin;
            int axis = 0;

            if(extent.y > extent.x) axis = 1;
            if(extent.z > extent[axis]) axis = 2;
            float splitPos = node.aabbMin[axis] + extent[axis] * 0.5f;

            // in-place partitioning
            int i = node.left;
            int j = i + node.triCount - 1;

            while(i <= j) {
                if(centroids.at(triangleIndices.at(i))[axis] < splitPos) {
                    i++;
                } else {
                    std::swap(triangleIndices.at(i), triangleIndices.at(j--));
                }
            }

            int leftCount = i - node.left;
            if(leftCount == 0 || leftCount == node.triCount) return;
            int leftChildIndex = nodeUsed++;
            int rightChildIndex = nodeUsed++;

            nodes.at(leftChildIndex).left = node.left;
            nodes.at(leftChildIndex).triCount = leftCount;
            nodes.at(rightChildIndex).left = i;
            nodes.at(rightChildIndex).triCount = node.triCount - leftCount;
            node.left = leftChildIndex;
            node.triCount = 0;

            UpdateNodeBounds(leftChildIndex);
            UpdateNodeBounds(rightChildIndex);

            Subdivide(leftChildIndex);
            Subdivide(rightChildIndex);
        }

        std::vector<BVHNodeNew>& GetNodes() { return nodes; }
        std::vector<int>& GetTrianglesIndices() { return triangleIndices; }

    private:
        std::vector<BVHNodeNew> nodes;
        std::vector<Triangle> allTriangles;
        std::vector<glm::vec4>centroids;
        std::vector<int> triangleIndices;
        int rootNodeIndex = 0;
        int nodeUsed = 1;
        const int maxTrianglesInLeaf = 4;
    };
}

#endif // !PATHTRACING_BVH_BUILDER_H