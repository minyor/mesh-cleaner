#pragma once

#include <chrono>
#include <iostream>
#include <map>
#include <queue>
#include <vector>

#include "util.h"

// Neighbor with winding info
struct Neighbor {
    int faceIdx;
    bool sameWinding; // true = both traverse shared edge same direction = INCONSISTENT winding
};

// Edge key: two vertex indices sorted so direction doesn't matter
struct EdgeKey {
    int a, b;

    EdgeKey(int va, int vb) {
        if (va < vb) { a = va; b = vb; }
        else { a = vb; b = va; }
    }

    bool operator<(const EdgeKey& o) const {
        if (a < o.a) return true;
        if (o.a < a) return false;
        return b < o.b;
    }
};

// Edge entry: stores face index and the "from" vertex index in original winding
struct EdgeEntry {
    int faceIdx;
    int dirA; // the "from" vertex index
};

// Build adjacency with winding direction tracking
// Uses vector of EdgeEntry per edge to handle non-manifold edges (shared by >2 faces)
static std::vector<std::vector<Neighbor>> buildAdjacency(const OBJMesh& mesh) {
    // Map edge -> list of all half-edges sharing that edge
    std::map<EdgeKey, std::vector<EdgeEntry>> edgeMap;
    std::vector<std::vector<Neighbor>> adjacency(mesh.faces.size());
    int matchedEdges = 0;
    int nonManifoldEdges = 0;

    // Phase 1: Collect all half-edges into the map
    for (size_t i = 0; i < mesh.faces.size(); ++i) {
        const auto& f = mesh.faces[i];
        int verts[3] = {f.v0, f.v1, f.v2};

        for (int e = 0; e < 3; ++e) {
            int va = verts[e];
            int vb = verts[(e + 1) % 3];
            EdgeKey key(va, vb);
            edgeMap[key].push_back({static_cast<int>(i), va});
        }
    }

    // Phase 2: For each edge with 2+ entries, create adjacency
    for (auto& [key, entries] : edgeMap) {
        if (entries.size() < 2) continue;

        if (entries.size() > 2) nonManifoldEdges++;

        for (size_t i = 0; i < entries.size(); ++i) {
            for (size_t j = i + 1; j < entries.size(); ++j) {
                bool sameWinding = (entries[i].dirA == entries[j].dirA);
                adjacency[entries[i].faceIdx].push_back({entries[j].faceIdx, sameWinding});
                adjacency[entries[j].faceIdx].push_back({entries[i].faceIdx, sameWinding});
                matchedEdges++;
            }
        }
    }

    // Count faces with no neighbors
    int isolated = 0;
    for (size_t i = 0; i < mesh.faces.size(); ++i) {
        if (adjacency[i].empty()) isolated++;
    }

    std::cout << "  Matched " << matchedEdges << " / " << (mesh.faces.size() * 3 / 2) << " edges" << std::endl;
    std::cout << "  Non-manifold edges: " << nonManifoldEdges << std::endl;
    std::cout << "  Isolated faces (no neighbors): " << isolated << std::endl;

    return adjacency;
}

// BFS flood-fill using edge winding consistency
// Tracks flip state so that winding relationships update correctly after flips
static void orientNormalsBFS(OBJMesh& mesh) {
    if (mesh.faces.empty()) return;

    auto startTime = std::chrono::steady_clock::now();

    auto adjacency = buildAdjacency(mesh);

    std::vector<bool> processed(mesh.faces.size(), false);
    std::vector<bool> flipped(mesh.faces.size(), false);
    std::queue<int> q;
    int totalFlips = 0;
    int processedCount = 0;
    int componentCount = 0;

    for (size_t seed = 0; seed < mesh.faces.size(); ++seed) {
        if (processed[seed]) continue;

        componentCount++;
        q.push(static_cast<int>(seed));
        processed[seed] = true;
        flipped[seed] = false;
        processedCount++;

        while (!q.empty()) {
            int cur = q.front();
            q.pop();

            for (const auto& nbEntry : adjacency[cur]) {
                int nb = nbEntry.faceIdx;
                if (processed[nb]) continue;

                // The effective winding relationship depends on flip state of current face
                bool needFlip = nbEntry.sameWinding ^ flipped[cur];

                if (needFlip) {
                    flipFace(mesh.faces[nb]);
                    flipped[nb] = true;
                    totalFlips++;
                } else {
                    flipped[nb] = false;
                }

                processed[nb] = true;
                processedCount++;
                q.push(nb);
            }
        }
    }

    auto endTime = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(endTime - startTime).count();
    std::cout << "  Processed " << processedCount << " / " << mesh.faces.size()
              << " faces, " << componentCount << " components, " << totalFlips << " flips ("
              << std::fixed << elapsed << "s)" << std::endl;
}

