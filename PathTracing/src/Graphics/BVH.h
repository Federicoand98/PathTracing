//
// Created by feder on 29/09/2023.
//

#ifndef PATHTRACING_BVH_H
#define PATHTRACING_BVH_H

#define INF 114514.0

#include <vector>
#include <glm/glm.hpp>
#include <algorithm>
#include "../Renderer/Primitives.h"

namespace PathTracer {

    struct BVHNode {
        int left;
        int right;
        int n;
        int index;
        glm::vec3 AA;
        glm::vec3 BB;
    };

    struct BVHNode_encoded {
        glm::vec3 childs;        // (left, right, padding)
        glm::vec3 leafInfo;      // (n, index, )
        glm::vec3 AA, BB;
    };

    class BVHBuilder {
    public:
        static int BuildBVHWithSAH(std::vector<Triangle>& triangles, std::vector<BVHNode>& nodes, int l, int r, int n) {
            if (l > r) return 0;

            nodes.push_back(BVHNode());
            int id = nodes.size() - 1;
            nodes[id].left = nodes[id].right = nodes[id].n = nodes[id].index = 0;
            nodes[id].AA = glm::vec3(1145141919, 1145141919, 1145141919);
            nodes[id].BB = glm::vec3(-1145141919, -1145141919, -1145141919);

            // AABB
            for (int i = l; i <= r; i++) {
                // AA
                float minx = glm::min(triangles[i].A.x, glm::min(triangles[i].B.x, triangles[i].C.x));
                float miny = glm::min(triangles[i].A.y, glm::min(triangles[i].B.y, triangles[i].C.y));
                float minz = glm::min(triangles[i].A.z, glm::min(triangles[i].B.z, triangles[i].C.z));
                nodes[id].AA.x = glm::min(nodes[id].AA.x, minx);
                nodes[id].AA.y = glm::min(nodes[id].AA.y, miny);
                nodes[id].AA.z = glm::min(nodes[id].AA.z, minz);
                // 最大点 BB
                float maxx = glm::max(triangles[i].A.x, glm::max(triangles[i].B.x, triangles[i].C.x));
                float maxy = glm::max(triangles[i].A.y, glm::max(triangles[i].B.y, triangles[i].C.y));
                float maxz = glm::max(triangles[i].A.z, glm::max(triangles[i].B.z, triangles[i].C.z));
                nodes[id].BB.x = glm::max(nodes[id].BB.x, maxx);
                nodes[id].BB.y = glm::max(nodes[id].BB.y, maxy);
                nodes[id].BB.z = glm::max(nodes[id].BB.z, maxz);
            }

            // 不多于 n 个三角形 返回叶子节点
            if ((r - l + 1) <= n) {
                nodes[id].n = r - l + 1;
                nodes[id].index = l;
                return id;
            }

            // 否则递归建树
            float Cost = INF;
            int Axis = 0;
            int Split = (l + r) / 2;
            for (int axis = 0; axis < 3; axis++) {
                // 分别按 x，y，z 轴排序
                if (axis == 0) std::sort(&triangles[0] + l, &triangles[0] + r + 1, Cmpx);
                if (axis == 1) std::sort(&triangles[0] + l, &triangles[0] + r + 1, Cmpy);
                if (axis == 2) std::sort(&triangles[0] + l, &triangles[0] + r + 1, Cmpz);

                // leftMax[i]: [l, i] 中最大的 xyz 值
                // leftMin[i]: [l, i] 中最小的 xyz 值
                std::vector<glm::vec3> leftMax(r - l + 1, glm::vec3(-INF, -INF, -INF));
                std::vector<glm::vec3> leftMin(r - l + 1, glm::vec3(INF, INF, INF));
                // 计算前缀 注意 i-l 以对齐到下标 0
                for (int i = l; i <= r; i++) {
                    Triangle& t = triangles[i];
                    int bias = (i == l) ? 0 : 1;  // 第一个元素特殊处理

                    leftMax[i - l].x = glm::max(leftMax[i - l - bias].x, glm::max(t.A.x, glm::max(t.B.x, t.C.x)));
                    leftMax[i - l].y = glm::max(leftMax[i - l - bias].y, glm::max(t.A.y, glm::max(t.B.y, t.C.y)));
                    leftMax[i - l].z = glm::max(leftMax[i - l - bias].z, glm::max(t.A.z, glm::max(t.B.z, t.C.z)));

                    leftMin[i - l].x = glm::min(leftMin[i - l - bias].x, glm::min(t.A.x, glm::min(t.B.x, t.C.x)));
                    leftMin[i - l].y = glm::min(leftMin[i - l - bias].y, glm::min(t.A.y, glm::min(t.B.y, t.C.y)));
                    leftMin[i - l].z = glm::min(leftMin[i - l - bias].z, glm::min(t.A.z, glm::min(t.B.z, t.C.z)));
                }

                // rightMax[i]: [i, r] 中最大的 xyz 值
                // rightMin[i]: [i, r] 中最小的 xyz 值
                std::vector<glm::vec3> rightMax(r - l + 1, glm::vec3(-INF, -INF, -INF));
                std::vector<glm::vec3> rightMin(r - l + 1, glm::vec3(INF, INF, INF));
                // 计算后缀 注意 i-l 以对齐到下标 0
                for (int i = r; i >= l; i--) {
                    Triangle& t = triangles[i];
                    int bias = (i == r) ? 0 : 1;  // 第一个元素特殊处理

                    rightMax[i - l].x = glm::max(rightMax[i - l + bias].x, glm::max(t.A.x, glm::max(t.B.x, t.C.x)));
                    rightMax[i - l].y = glm::max(rightMax[i - l + bias].y, glm::max(t.A.y, glm::max(t.B.y, t.C.y)));
                    rightMax[i - l].z = glm::max(rightMax[i - l + bias].z, glm::max(t.A.z, glm::max(t.B.z, t.C.z)));

                    rightMin[i - l].x = glm::min(rightMin[i - l + bias].x, glm::min(t.A.x, glm::min(t.B.x, t.C.x)));
                    rightMin[i - l].y = glm::min(rightMin[i - l + bias].y, glm::min(t.A.y, glm::min(t.B.y, t.C.y)));
                    rightMin[i - l].z = glm::min(rightMin[i - l + bias].z, glm::min(t.A.z, glm::min(t.B.z, t.C.z)));
                }

                // 遍历寻找分割
                float cost = INF;
                int split = l;
                for (int i = l; i <= r - 1; i++) {
                    float lenx, leny, lenz;
                    // 左侧 [l, i]
                    glm::vec3 leftAA = leftMin[i - l];
                    glm::vec3 leftBB = leftMax[i - l];
                    lenx = leftBB.x - leftAA.x;
                    leny = leftBB.y - leftAA.y;
                    lenz = leftBB.z - leftAA.z;
                    float leftS = 2.0 * ((lenx * leny) + (lenx * lenz) + (leny * lenz));
                    float leftCost = leftS * (i - l + 1);

                    // 右侧 [i+1, r]
                    glm::vec3 rightAA = rightMin[i + 1 - l];
                    glm::vec3 rightBB = rightMax[i + 1 - l];
                    lenx = rightBB.x - rightAA.x;
                    leny = rightBB.y - rightAA.y;
                    lenz = rightBB.z - rightAA.z;
                    float rightS = 2.0 * ((lenx * leny) + (lenx * lenz) + (leny * lenz));
                    float rightCost = rightS * (r - i);

                    // 记录每个分割的最小答案
                    float totalCost = leftCost + rightCost;
                    if (totalCost < cost) {
                        cost = totalCost;
                        split = i;
                    }
                }
                // 记录每个轴的最佳答案
                if (cost < Cost) {
                    Cost = cost;
                    Axis = axis;
                    Split = split;
                }
            }

            // 按最佳轴分割
            if (Axis == 0) std::sort(&triangles[0] + l, &triangles[0] + r + 1, Cmpx);
            if (Axis == 1) std::sort(&triangles[0] + l, &triangles[0] + r + 1, Cmpy);
            if (Axis == 2) std::sort(&triangles[0] + l, &triangles[0] + r + 1, Cmpz);

            // 递归
            int left  = BuildBVHWithSAH(triangles, nodes, l, Split, n);
            int right = BuildBVHWithSAH(triangles, nodes, Split + 1, r, n);

            nodes[id].left = left;
            nodes[id].right = right;

            return id;
        }

        static bool Cmpx(const Triangle& t1, const Triangle& t2) {
            glm::vec3 center1 = glm::vec3(t1.A + t1.B + t1.C) / glm::vec3(3, 3, 3);
            glm::vec3 center2 = glm::vec3(t2.A + t2.B + t2.C) / glm::vec3(3, 3, 3);
            return center1.x < center2.x;
        }

        static bool Cmpy(const Triangle& t1, const Triangle& t2) {
            glm::vec3 center1 = glm::vec3(t1.A + t1.B + t1.C) / glm::vec3(3, 3, 3);
            glm::vec3 center2 = glm::vec3(t2.A + t2.B + t2.C) / glm::vec3(3, 3, 3);
            return center1.y < center2.y;
        }

        static bool Cmpz(const Triangle& t1, const Triangle& t2) {
            glm::vec3 center1 = glm::vec3(t1.A + t1.B + t1.C) / glm::vec3(3, 3, 3);
            glm::vec3 center2 = glm::vec3(t2.A + t2.B + t2.C) / glm::vec3(3, 3, 3);
            return center1.z < center2.z;
        }
    };
}

#endif //PATHTRACING_BVH_H
