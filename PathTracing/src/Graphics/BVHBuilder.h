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

    // ---- SBVH (Spatial split BVH, Stich et al. 2009) --------------------------
    // Un BVH a object-split (come BVH) mette ogni triangolo interamente da un lato: se i
    // triangoli sono grandi/diagonali (tende, pavimenti di Sponza) le AABB dei due figli si
    // SOVRAPPONGONO e i raggi scendono in entrambi = lavoro sprecato (il "giallo" in heatmap).
    // L'SBVH aggiunge gli SPATIAL SPLIT: puo' tagliare un triangolo lungo il piano, mettendone
    // un riferimento in ENTRAMBI i figli ma con bounds clippati (piu' stretti) -> meno overlap.
    // Il prezzo e' la duplicazione dei riferimenti (un triangolo puo' stare in piu' foglie),
    // limitata dalla restrizione alpha (spatial solo dove l'object split ha overlap notevole).
    //
    // Espone la STESSA interfaccia di BVH (GetNodes/GetTrianglesIndices), quindi il collapse a
    // BVH4 e la traversal restano identici. GetTrianglesIndices() puo' contenere duplicati: un
    // triangolo testato due volte da' lo stesso hit, ray.t se ne occupa.
    class SBVH {
    public:
        SBVH(const std::vector<Triangle>& triangles, int firstTriangle, int count)
            : tris(&triangles), firstTri(firstTriangle) {
            if (count == 0) return;

            std::vector<Ref> refs;
            refs.reserve(count);
            for (int i = 0; i < count; i++) {
                const Triangle& t = triangles[firstTriangle + i];
                AABB b;
                b.expand(glm::vec3(t.A)); b.expand(glm::vec3(t.B)); b.expand(glm::vec3(t.C));
                refs.push_back({ i, b });
            }

            AABB rootBounds;
            for (const Ref& r : refs) rootBounds.expand(r.box);
            rootArea = rootBounds.area();

            nodes.reserve(count * 2);
            nodes.emplace_back();          // radice
            Subdivide(0, refs, 0);
        }

        std::vector<BVHNodeNew>& GetNodes() { return nodes; }
        std::vector<int>& GetTrianglesIndices() { return triIndices; } // indici LOCALI, con duplicati

    private:
        struct Ref { int tri; AABB box; };   // tri = indice locale (0..count-1); box = bounds (clippati)
        struct Bin { AABB bounds; int enter = 0, exit = 0; };
        struct Split { int axis = -1; float pos = 0.0f; float cost = 1e30f; };

        const Triangle& Tri(int local) const { return (*tris)[firstTri + local]; }

        static float HalfArea(const AABB& b) {
            glm::vec3 e = b.max - b.min;
            if (e.x < 0.0f) return 0.0f;
            return e.x * e.y + e.y * e.z + e.z * e.x;
        }

        // Clip di un poligono (n vertici in 'in') contro un piano assiale, risultato in 'out'.
        // keepBelow: tieni la parte con coord <= pos. Ritorna il nuovo numero di vertici.
        // Niente heap: 'in'/'out' sono array su stack (Sutherland-Hodgman a un piano).
        static int ClipPlane(const glm::vec3* in, int n, int axis, float pos, bool keepBelow, glm::vec3* out) {
            int m = 0;
            for (int i = 0; i < n; i++) {
                const glm::vec3& a = in[i];
                const glm::vec3& b = in[(i + 1) % n];
                float da = a[axis] - pos, db = b[axis] - pos;
                bool ina = keepBelow ? (da <= 0.0f) : (da >= 0.0f);
                bool inb = keepBelow ? (db <= 0.0f) : (db >= 0.0f);
                if (ina) out[m++] = a;
                if (ina != inb) {
                    float t = da / (da - db);
                    out[m++] = a + t * (b - a);
                }
            }
            return m;
        }

        // AABB della parte di triangolo dentro lo slab [lo,hi] sull'asse, intersecata con la box
        // del riferimento (che puo' gia' essere clippata da split precedenti). Box vuota se niente.
        // Un triangolo clippato da 2 piani ha al massimo 5 vertici: gli array da 8 bastano.
        AABB ClipToSlab(int local, int axis, float lo, float hi, const AABB& refBox) const {
            const Triangle& t = Tri(local);
            glm::vec3 bufA[8], bufB[8];
            bufA[0] = glm::vec3(t.A); bufA[1] = glm::vec3(t.B); bufA[2] = glm::vec3(t.C);
            int n = ClipPlane(bufA, 3, axis, lo, false, bufB);      // tieni >= lo
            AABB out;
            if (n == 0) return out;                                  // vuoto (min>max)
            n = ClipPlane(bufB, n, axis, hi, true, bufA);           // tieni <= hi
            for (int i = 0; i < n; i++) out.expand(bufA[i]);
            out.min = glm::max(out.min, refBox.min);
            out.max = glm::min(out.max, refBox.max);
            return out;
        }

        // Object split: SAH binned sui centroidi dei riferimenti (nessun clipping).
        Split FindObjectSplit(const std::vector<Ref>& refs, const AABB& centroidB) const {
            Split best;
            for (int axis = 0; axis < 3; axis++) {
                float lo = centroidB.min[axis], hi = centroidB.max[axis];
                if (hi - lo < 1e-9f) continue;
                float scale = BINS / (hi - lo);

                Bin bins[BINS];
                for (const Ref& r : refs) {
                    glm::vec3 c = (r.box.min + r.box.max) * 0.5f;
                    int bi = glm::clamp(int((c[axis] - lo) * scale), 0, BINS - 1);
                    bins[bi].bounds.expand(r.box);
                    bins[bi].enter++;
                }

                float leftArea[BINS - 1], rightArea[BINS - 1];
                int leftCount[BINS - 1], rightCount[BINS - 1];
                AABB lb, rb; int ls = 0, rs = 0;
                for (int i = 0; i < BINS - 1; i++) {
                    ls += bins[i].enter; leftCount[i] = ls; lb.expand(bins[i].bounds); leftArea[i] = HalfArea(lb);
                    rs += bins[BINS - 1 - i].enter; rightCount[BINS - 2 - i] = rs; rb.expand(bins[BINS - 1 - i].bounds); rightArea[BINS - 2 - i] = HalfArea(rb);
                }
                for (int i = 0; i < BINS - 1; i++) {
                    float cost = leftCount[i] * leftArea[i] + rightCount[i] * rightArea[i];
                    if (cost > 0.0f && cost < best.cost) {
                        best.cost = cost; best.axis = axis;
                        best.pos = lo + (hi - lo) * (i + 1) / BINS;
                    }
                }
            }
            return best;
        }

        // Spatial split: binned con clipping dei triangoli agli slab (Stich). Valutato SOLO
        // sull'asse piu' lungo del nodo (dove il taglio riduce di piu' l'area): il clipping e'
        // la parte costosa, farlo su un asse invece che 3 dimezza abbondantemente il build con
        // perdita di qualita' trascurabile.
        Split FindSpatialSplit(const std::vector<Ref>& refs, const AABB& nodeB) const {
            Split best;
            glm::vec3 e = nodeB.max - nodeB.min;
            int axis = (e.x >= e.y && e.x >= e.z) ? 0 : (e.y >= e.z ? 1 : 2);
            float lo = nodeB.min[axis], hi = nodeB.max[axis];
            if (hi - lo < 1e-9f) return best;
            float scale = SPATIAL_BINS / (hi - lo);

            Bin bins[SPATIAL_BINS];
            for (const Ref& r : refs) {
                int b0 = glm::clamp(int((r.box.min[axis] - lo) * scale), 0, SPATIAL_BINS - 1);
                int b1 = glm::clamp(int((r.box.max[axis] - lo) * scale), 0, SPATIAL_BINS - 1);
                for (int b = b0; b <= b1; b++) {
                    float blo = lo + (hi - lo) * b / SPATIAL_BINS;
                    float bhi = lo + (hi - lo) * (b + 1) / SPATIAL_BINS;
                    AABB c = ClipToSlab(r.tri, axis, blo, bhi, r.box);
                    if (c.max[axis] >= c.min[axis]) bins[b].bounds.expand(c);
                }
                bins[b0].enter++; bins[b1].exit++;
            }

            float leftArea[SPATIAL_BINS - 1], rightArea[SPATIAL_BINS - 1];
            int leftCount[SPATIAL_BINS - 1], rightCount[SPATIAL_BINS - 1];
            AABB lb, rb; int ls = 0, rs = 0;
            for (int i = 0; i < SPATIAL_BINS - 1; i++) {
                ls += bins[i].enter; leftCount[i] = ls; lb.expand(bins[i].bounds); leftArea[i] = HalfArea(lb);
                rs += bins[SPATIAL_BINS - 1 - i].exit; rightCount[SPATIAL_BINS - 2 - i] = rs; rb.expand(bins[SPATIAL_BINS - 1 - i].bounds); rightArea[SPATIAL_BINS - 2 - i] = HalfArea(rb);
            }
            for (int i = 0; i < SPATIAL_BINS - 1; i++) {
                float cost = leftCount[i] * leftArea[i] + rightCount[i] * rightArea[i];
                if (cost > 0.0f && cost < best.cost) {
                    best.cost = cost; best.axis = axis;
                    best.pos = lo + (hi - lo) * (i + 1) / SPATIAL_BINS;
                }
            }
            return best;
        }

        void MakeLeaf(int nodeIndex, const std::vector<Ref>& refs) {
            nodes[nodeIndex].left = static_cast<int>(triIndices.size());
            nodes[nodeIndex].triCount = static_cast<int>(refs.size());
            for (const Ref& r : refs) triIndices.push_back(r.tri);
        }

        void Subdivide(int nodeIndex, std::vector<Ref>& refs, int depth) {
            AABB bounds, centroidB;
            for (const Ref& r : refs) {
                bounds.expand(r.box);
                centroidB.expand((r.box.min + r.box.max) * 0.5f);
            }
            nodes[nodeIndex].aabbMin = bounds.min;
            nodes[nodeIndex].aabbMax = bounds.max;

            if (static_cast<int>(refs.size()) <= maxTrianglesInLeaf || depth >= MAX_DEPTH) {
                MakeLeaf(nodeIndex, refs);
                return;
            }

            Split object = FindObjectSplit(refs, centroidB);

            // restrizione alpha: valuta lo spatial split solo se i due figli dell'object split
            // si sovrappongono in modo non trascurabile (altrimenti duplicare non conviene)
            Split spatial;
            if (object.axis >= 0) {
                AABB lB, rB;
                for (const Ref& r : refs) {
                    float c = (r.box.min[object.axis] + r.box.max[object.axis]) * 0.5f;
                    if (c < object.pos) lB.expand(r.box); else rB.expand(r.box);
                }
                AABB ov; ov.min = glm::max(lB.min, rB.min); ov.max = glm::min(lB.max, rB.max);
                float overlap = (ov.max.x >= ov.min.x && ov.max.y >= ov.min.y && ov.max.z >= ov.min.z) ? HalfArea(ov) : 0.0f;
                if (overlap > ALPHA * rootArea)
                    spatial = FindSpatialSplit(refs, bounds);
            }

            float leafCost = refs.size() * HalfArea(bounds);
            if (object.axis < 0 && spatial.axis < 0) { MakeLeaf(nodeIndex, refs); return; }
            if (leafCost <= object.cost && leafCost <= spatial.cost) { MakeLeaf(nodeIndex, refs); return; }

            std::vector<Ref> leftRefs, rightRefs;
            bool useSpatial = spatial.axis >= 0 && spatial.cost < object.cost;

            if (useSpatial) {
                int axis = spatial.axis; float pos = spatial.pos;

                // pass 1: assegna i riferimenti che NON attraversano il piano, accumula i bounds
                // dei due lati e raccogli quelli che lo attraversano (candidati alla duplicazione)
                AABB bL, bR;
                std::vector<Ref> straddlers;
                for (const Ref& r : refs) {
                    bool toLeft = r.box.min[axis] < pos;
                    bool toRight = r.box.max[axis] > pos;
                    if (toLeft && toRight) straddlers.push_back(r);
                    else if (toLeft) { leftRefs.push_back(r); bL.expand(r.box); }
                    else { rightRefs.push_back(r); bR.expand(r.box); }
                }

                // pass 2: reference unsplitting (Stich). Per ogni ref che attraversa, confronta il
                // costo (in area) di 3 scelte e prende la piu' economica: duplicare (split), o
                // metterlo tutto a sinistra/destra. Cosi' si duplica solo quando conviene davvero.
                for (const Ref& r : straddlers) {
                    AABB lc = ClipToSlab(r.tri, axis, -1e30f, pos, r.box);
                    AABB rc = ClipToSlab(r.tri, axis, pos, 1e30f, r.box);
                    bool lcValid = lc.max[axis] >= lc.min[axis];
                    bool rcValid = rc.max[axis] >= rc.min[axis];

                    AABB blSplit = bL; if (lcValid) blSplit.expand(lc);
                    AABB brSplit = bR; if (rcValid) brSplit.expand(rc);
                    AABB blLeft = bL; blLeft.expand(r.box);
                    AABB brRight = bR; brRight.expand(r.box);

                    float cSplit = HalfArea(blSplit) + HalfArea(brSplit);
                    float cLeft = HalfArea(blLeft) + HalfArea(bR);
                    float cRight = HalfArea(bL) + HalfArea(brRight);

                    if (cSplit <= cLeft && cSplit <= cRight) {           // duplica
                        if (lcValid) { leftRefs.push_back({ r.tri, lc }); bL = blSplit; }
                        if (rcValid) { rightRefs.push_back({ r.tri, rc }); bR = brSplit; }
                    } else if (cLeft <= cRight) {                        // tutto a sinistra
                        leftRefs.push_back(r); bL = blLeft;
                    } else {                                             // tutto a destra
                        rightRefs.push_back(r); bR = brRight;
                    }
                }
            } else {
                int axis = object.axis; float pos = object.pos;
                for (const Ref& r : refs) {
                    float c = (r.box.min[axis] + r.box.max[axis]) * 0.5f;
                    if (c < pos) leftRefs.push_back(r); else rightRefs.push_back(r);
                }
            }

            if (leftRefs.empty() || rightRefs.empty()) { MakeLeaf(nodeIndex, refs); return; }

            refs.clear(); refs.shrink_to_fit(); // libera memoria prima di ricorrere

            int l = static_cast<int>(nodes.size());
            nodes.emplace_back();          // figlio sinistro
            nodes.emplace_back();          // figlio destro (indice l+1, consecutivo: lo pretende il collapse)
            nodes[nodeIndex].left = l;
            nodes[nodeIndex].triCount = 0;

            Subdivide(l, leftRefs, depth + 1);
            Subdivide(l + 1, rightRefs, depth + 1);
        }

    private:
        static constexpr int BINS = 12;
        static constexpr int SPATIAL_BINS = 16;
        static constexpr int MAX_DEPTH = 48;
        static constexpr int maxTrianglesInLeaf = 4;
        static constexpr float ALPHA = 1e-5f; // soglia di overlap per abilitare lo spatial split

        std::vector<BVHNodeNew> nodes;
        std::vector<int> triIndices;
        const std::vector<Triangle>* tris = nullptr;
        int firstTri = 0;
        float rootArea = 0.0f;
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
