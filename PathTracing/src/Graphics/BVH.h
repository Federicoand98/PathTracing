//
// Created by feder on 29/09/2023.
//

#ifndef PATHTRACING_BVH_H
#define PATHTRACING_BVH_H

#define INF 114514.0

#include "ptpch.h"
#include "../Renderer/Primitives.h"

namespace PathTracer {

    struct BVHNode {
        glm::vec4 AABBMin;
        glm::vec4 AABBMax;
        float leftFirst;
        float triCount;
    };

    struct BVHNodeAlt {
        glm::vec4 AA;
        glm::vec4 BB;
        float left;
        float right;
        float n;
        float index;
    };

    struct aabb {
        glm::vec3 bmin = glm::vec3(1e30f);
        glm::vec3 bmax = glm::vec3(- 1e30f);

        void grow(glm::vec3 p) { 
            bmin = glm::min(bmin, p);
            bmax = glm::max(bmax, p); 
        }

        float area() {
            glm::vec3 e = bmax - bmin; // box extent
            return e.x * e.y + e.y * e.z + e.z * e.x;
        }
    };

	inline bool Cmpx(const Triangle* t1, const Triangle* t2) {
		glm::vec4 center1 = (t1->A + t1->B + t1->C) / glm::vec4(3, 3, 3, 3);
		glm::vec4 center2 = (t2->A + t2->B + t2->C) / glm::vec4(3, 3, 3, 3);
		return center1.x < center2.x;
	}
	
	inline bool Cmpy(const Triangle* t1, const Triangle* t2) {
		glm::vec4 center1 = (t1->A + t1->B + t1->C) / glm::vec4(3, 3, 3, 3);
		glm::vec4 center2 = (t2->A + t2->B + t2->C) / glm::vec4(3, 3, 3, 3);
		return center1.y < center2.y;
	}

	inline bool Cmpz(const Triangle* t1, const Triangle* t2) {
		glm::vec4 center1 = (t1->A + t1->B + t1->C) / glm::vec4(3, 3, 3, 3);
		glm::vec4 center2 = (t2->A + t2->B + t2->C) / glm::vec4(3, 3, 3, 3);
		return center1.z < center2.z;
	}

    class BVHBuilder {
    public:
        BVHBuilder(const std::vector<Triangle*>& triangles) {
            N = triangles.size();
            /*
            for (int i = 0; i < N * 2 - 1; i++) {
                BVHNode node = { glm::vec4(0.0), glm::vec4(0.0), 0,  0, 0 };
            }
            */
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
            root.leftFirst = 0.0;
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

            node.AABBMin = glm::vec4(1e30f);
            node.AABBMax = glm::vec4(-1e30f);

            for (int first = node.leftFirst, i = 0; i < node.triCount; i++) {
                int leafTriIndex = triIndex[first + i];
                Triangle* leafTri = triangles.at(leafTriIndex);

                node.AABBMin = glm::min(node.AABBMin, leafTri->A);
                node.AABBMin = glm::min(node.AABBMin, leafTri->B);
                node.AABBMin = glm::min(node.AABBMin, leafTri->C);
                node.AABBMax = glm::max(node.AABBMax, leafTri->A);
                node.AABBMax = glm::max(node.AABBMax, leafTri->B);
                node.AABBMax = glm::max(node.AABBMax, leafTri->C);
            }
        }

        float EvaluateSAH(BVHNode& node, int axis, float pos, std::vector<Triangle*> triangles) {
            aabb leftBox, rightBox;
            int leftCount = 0, rightCount = 0;

            for (int i = 0; i < (int)node.triCount; i++) {
                Triangle& tri = *triangles.at(triIndex[(int)node.leftFirst + i]);
				glm::vec3 centroid = (tri.A + tri.B + tri.C) * 0.3333f;

                if (centroid[axis] < pos) {
                    leftCount++;
                    leftBox.grow(tri.A);
                    leftBox.grow(tri.B);
                    leftBox.grow(tri.C);
                }
                else {
                    rightCount++;
                    rightBox.grow(tri.A);
                    rightBox.grow(tri.B);
                    rightBox.grow(tri.C);
                }
            }

            float cost = leftCount * leftBox.area() + rightCount * rightBox.area();
            return cost > 0 ? cost : 1e30f;
        }

#define OLD_SUBDIVIDE 0

        void Subdivide(int nodeIndex, const std::vector<Triangle*> triangles) {
#if OLD_SUBDIVIDE
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
            bvhNode[leftChildIndex].triCount = (float)leftCount;
            bvhNode[rightChildIndex].firstTriIndex = (float)i;
            bvhNode[rightChildIndex].triCount = (float)node.triCount - (float)leftCount;

            bvhNode[leftChildIndex].leftNode = (float)leftChildIndex;
            bvhNode[rightChildIndex].leftNode = (float)leftChildIndex;

            node.leftNode = (float)leftChildIndex;
            node.triCount = 0.0;

            UpdateNodeBounds(leftChildIndex, triangles);
            UpdateNodeBounds(rightChildIndex, triangles);

            // recurse
            Subdivide(leftChildIndex, triangles);
            Subdivide(rightChildIndex, triangles);
#else
            BVHNode& node = bvhNode[nodeIndex];
            int bestAxis = -1;
            float bestPos = 0, bestCost = 1e30f;

            for (int axis = 0; axis < 3; axis++) {
                for (int i = 0; i < node.triCount; i++) {
                    Triangle* triangle = triangles.at(triIndex[(int)node.leftFirst + i]);
					glm::vec3 centroid = (triangle->A + triangle->B + triangle->C) * 0.3333f;
                    float candidatePos = centroid[axis];

                    float cost = EvaluateSAH(node, axis, candidatePos, triangles);

                    if (cost < bestCost)
                        bestCost = candidatePos, bestAxis = axis, bestCost = cost;
                }
            }

            int axis = bestAxis;
            float splitPos = bestPos;
            glm::vec4 e = node.AABBMax - node.AABBMin;
            float parentArea = e.x * e.y + e.y * e.z + e.z * e.x;
            float parentCost = node.triCount * parentArea;
            if (bestCost >= parentCost)
                return;
            int i = node.leftFirst;
            int j = i + node.triCount - 1;
            while (i <= j) {
				glm::vec3 centroid = (triangles.at(triIndex[i])->A + triangles.at(triIndex[i])->B + triangles.at(triIndex[i])->C) * 0.3333f;
                if (centroid[axis] < splitPos)
                    i++;
                else
                    std::swap(triIndex[i], triIndex[j--]);
            }

            int leftCount = i - node.leftFirst;
            if (leftCount == 0 || leftCount == node.triCount)
                return;
            int leftChildIndex = nodeUsed++;
            int rightChildIndex = nodeUsed++;
            bvhNode[leftChildIndex].leftFirst = node.leftFirst;
            bvhNode[leftChildIndex].triCount = leftCount;
            bvhNode[rightChildIndex].leftFirst = i;
            bvhNode[rightChildIndex].triCount = node.triCount - leftCount;
            node.leftFirst = leftChildIndex;
            node.triCount = 0;

            UpdateNodeBounds(leftChildIndex, triangles);
            UpdateNodeBounds(rightChildIndex, triangles);

            Subdivide(leftChildIndex, triangles);
            Subdivide(rightChildIndex, triangles);
#endif // OLD_SUBDIVIDE
        }

        std::vector<int> GetTriIndex() {
            std::vector<int> idx;
            for (int i = 0; i < N; i++) {
                idx.push_back(triIndex[i]);
            }

            return idx;
        }


        int BuildBVHAlt(std::vector<Triangle*>& triangles, std::vector<BVHNodeAlt>& nodes, int l, int r, int n) {
            if (l > r)
                return 0;

            nodes.push_back(BVHNodeAlt());
            int id = nodes.size() - 1;
            nodes[id].left = nodes[id].right = nodes[id].n = nodes[id].index = 0;
            nodes[id].AA = glm::vec4(1145141919, 1145141919, 1145141919, 1145141919);
            nodes[id].BB = glm::vec4(-1145141919, -1145141919, -1145141919, -1145141919);

            for (int i = l; i <= r; i++) {
                float minx = fminf(triangles.at(i)->A.x, fminf(triangles.at(i)->B.x, triangles.at(i)->C.x));
                float miny = fminf(triangles.at(i)->A.y, fminf(triangles.at(i)->B.y, triangles.at(i)->C.y));
                float minz = fminf(triangles.at(i)->A.z, fminf(triangles.at(i)->B.z, triangles.at(i)->C.z));

                nodes[id].AA.x = fminf(nodes[id].AA.x, minx);
                nodes[id].AA.y = fminf(nodes[id].AA.y, miny);
                nodes[id].AA.z = fminf(nodes[id].AA.z, minz);

                float maxx = fmaxf(triangles.at(i)->A.x, fmaxf(triangles.at(i)->B.x, triangles.at(i)->C.x));
                float maxy = fmaxf(triangles.at(i)->A.y, fmaxf(triangles.at(i)->B.y, triangles.at(i)->C.y));
                float maxz = fmaxf(triangles.at(i)->A.z, fmaxf(triangles.at(i)->B.z, triangles.at(i)->C.z));

                nodes[id].BB.x = fmaxf(nodes[id].BB.x, maxx);
                nodes[id].BB.y = fmaxf(nodes[id].BB.y, maxy);
                nodes[id].BB.z = fmaxf(nodes[id].BB.z, maxz);
            }
               
            if ((r - l + 1) <= n) {
                nodes[id].n = r - l + 1;
                nodes[id].index = l;
                return id;
            }

            float lenx = nodes[id].BB.x - nodes[id].AA.x;
            float leny = nodes[id].BB.y - nodes[id].AA.y;
            float lenz = nodes[id].BB.z - nodes[id].AA.z;

            if (lenx >= leny && lenx >= lenz)
                std::sort(triangles.begin() + l, triangles.begin() + r + 1, Cmpx);

            if (leny >= lenx && leny >= lenz)
                std::sort(triangles.begin() + l, triangles.begin() + r + 1, Cmpy);

            if (lenz >= lenx && lenz >= leny)
                std::sort(triangles.begin() + l, triangles.begin() + r + 1, Cmpz);

            int mid = (l + r) / 2;
            int left = BuildBVHAlt(triangles, nodes, l, mid, n);
            int right = BuildBVHAlt(triangles, nodes, mid + 1, r, n);

            nodes[id].left = left;
            nodes[id].right = right;

            return id;
        }

    private:
        std::vector<BVHNode> bvhNodes;
        BVHNode *bvhNode;
        int *triIndex;
        int N = 0;
        int rootNodeIndex = 0;
        int nodeUsed = 2;
    };
}

#endif //PATHTRACING_BVH_H
