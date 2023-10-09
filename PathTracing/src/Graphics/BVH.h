//
// Created by feder on 29/09/2023.
//

#ifndef PATHTRACING_BVH_H
#define PATHTRACING_BVH_H

#define INF 114514.0

#include <iostream>
#include <vector>
#include <glm/glm.hpp>
#include <algorithm>
#include "../Renderer/Primitives.h"

namespace PathTracer {

    struct BVHNode {
        glm::vec4 AABBMin;
        glm::vec4 AABBMax;
        float leftNode;
        float firstTriIndex;
        float triCount;

        float padding = 0;
    };

    class BVHBuilder {
    public:
        BVHBuilder(const std::vector<Triangle*>& triangles) {
            N = triangles.size();
            for (int i = 0; i < N * 2 - 1; i++) {
                BVHNode node = { glm::vec4(0.0), glm::vec4(0.0), 0, 0, 0, 0 };
            }
            bvhNode = new BVHNode[N * 2 - 1];
            triIndex = new int[N];
        }

        ~BVHBuilder() {
            delete[] bvhNode;
            delete[] triIndex;
        }

        std::vector<BVHNode> CalculateBVH(const std::vector<Triangle*>& triangles) {
            for (int i = 0; i < N; i++)
                triIndex[i] = i;

            BVHNode& root = bvhNode[rootNodeIndex];
            root.leftNode = 0;
            root.firstTriIndex = 0;
            root.triCount = N;

            UpdateNodeBounds(rootNodeIndex, triangles);
            Subdivide(rootNodeIndex, triangles);

            std::vector<BVHNode> nodes;
            for (int i = 0; i < N * 2 - 1; i++) {
                BVHNode& node = bvhNode[i];
                if (node.triCount > 0)
                    nodes.push_back(node);
            }

            return nodes;
        }

        void UpdateNodeBounds(int nodeIndex, const std::vector<Triangle*> triangles) {
            BVHNode& node = bvhNode[nodeIndex];

            node.AABBMin = glm::vec4(0);
            node.AABBMax = glm::vec4(0);

            for (int first = node.firstTriIndex, i = 0; i < node.triCount; i++) {
                Triangle* leafTri = triangles.at(first + i);

                node.AABBMin = glm::min(node.AABBMin, leafTri->A);
                node.AABBMin = glm::min(node.AABBMin, leafTri->B);
                node.AABBMin = glm::min(node.AABBMin, leafTri->C);
                node.AABBMax = glm::max(node.AABBMax, leafTri->A);
                node.AABBMax = glm::max(node.AABBMax, leafTri->B);
                node.AABBMax = glm::max(node.AABBMax, leafTri->C);
            }
        }

        void Subdivide(int nodeIndex, const std::vector<Triangle*> triangles) {
            // Terminate recursion
            BVHNode& node = bvhNode[nodeIndex];
            if (node.triCount <= 2)
                return;

            // determine split axis and position
            int axis = 0;
            glm::vec3 extent = glm::vec3(node.AABBMax - node.AABBMin);

            if (extent.y > extent.x) axis = 1;
            if (extent.z > extent[axis]) axis = 2;

            float splitPosition = node.AABBMin[axis] + extent[axis] * 0.5f;
            int i = node.firstTriIndex;
            int j = i + node.triCount - 1;

            while (i <= j) {
                glm::vec3 centroid = (triangles.at(triIndex[i])->A + triangles.at(triIndex[i])->B + triangles.at(triIndex[i])->C) * 0.3333f;
                if (centroid[axis] < splitPosition)
                    i++;
                else
                    std::swap(triIndex[i], triIndex[j--]);
            }

            // abort split if one of the sides is empty
            int leftCount = i - node.firstTriIndex;
            if (leftCount == 0 || leftCount == node.triCount)
                return;

            // create child nodes
            int leftChildIndex = nodeUsed++;
            int rightChildIndex = nodeUsed++;
            bvhNode[leftChildIndex].firstTriIndex = node.firstTriIndex;
            bvhNode[leftChildIndex].triCount = leftCount;
            bvhNode[rightChildIndex].firstTriIndex = i;
            bvhNode[rightChildIndex].triCount = node.triCount - leftCount;
            node.leftNode = leftChildIndex;
            node.triCount = 0;

            std::cout << "Node n" << nodeIndex << " leftNode: " << node.leftNode << " triCount: " << node.triCount << std::endl;

            UpdateNodeBounds(leftChildIndex, triangles);
            UpdateNodeBounds(rightChildIndex, triangles);

            // recurse
            Subdivide(leftChildIndex, triangles);
            Subdivide(rightChildIndex, triangles);
        }

        std::vector<int> GetTriIndex() {
            std::vector<int> idx;
            for (int i = 0; i < N; i++) {
                idx.push_back(triIndex[i]);
            }

            return idx;
        }

    private:
        std::vector<BVHNode> bvhNodes;
        BVHNode *bvhNode;
        int *triIndex;
        int N = 0;
        int rootNodeIndex = 0;
        int nodeUsed = 1;
    };
}

#endif //PATHTRACING_BVH_H
