#pragma once

#include <algorithm>
#include <chrono>
#include <iostream>
#include <map>
#include <set>
#include <vector>

#include "util.h"

// Edge key for undirected edges (sorted vertex indices)
struct HoleEdgeKey {
    int a, b;

    HoleEdgeKey(int va, int vb) {
        if (va < vb) { a = va; b = vb; }
        else { a = vb; b = va; }
    }

    bool operator<(const HoleEdgeKey& o) const {
        if (a < o.a) return true;
        if (o.a < a) return false;
        return b < o.b;
    }

    bool operator==(const HoleEdgeKey& o) const {
        return a == o.a && b == o.b;
    }
};

// Find all boundary edges (edges belonging to exactly one face)
static std::vector<HoleEdgeKey> findBoundaryEdges(const OBJMesh& mesh) {
    std::map<HoleEdgeKey, int> edgeCount;

    for (const auto& face : mesh.faces) {
        int verts[3] = {face.v0, face.v1, face.v2};
        for (int e = 0; e < 3; ++e) {
            int va = verts[e];
            int vb = verts[(e + 1) % 3];
            edgeCount[HoleEdgeKey(va, vb)]++;
        }
    }

    std::vector<HoleEdgeKey> boundary;
    for (const auto& [key, count] : edgeCount) {
        if (count == 1) {
            boundary.push_back(key);
        }
    }

    return boundary;
}

// Group boundary edges into ordered loops
// Each loop is a sequence of vertices forming a closed boundary
// Handles vertices shared by multiple loops (degree > 2 in boundary adjacency)
static std::vector<std::vector<int>> groupBoundaryLoops(const std::vector<HoleEdgeKey>& boundaryEdges) {
    // Build adjacency: for each vertex, store connected vertices via boundary edges
    std::map<int, std::vector<int>> adjacency;

    for (const auto& edge : boundaryEdges) {
        adjacency[edge.a].push_back(edge.b);
        adjacency[edge.b].push_back(edge.a);
    }

    // Track which undirected edges have been used
    std::set<HoleEdgeKey> usedEdges;
    std::vector<std::vector<int>> loops;

    // For each boundary edge, try to form a loop starting from it
    for (const auto& edge : boundaryEdges) {
        if (usedEdges.count(edge)) continue;

        // Start a new loop: traverse edge.a -> edge.b
        std::vector<int> loop;
        loop.push_back(edge.a);
        loop.push_back(edge.b);
        usedEdges.insert(edge);

        bool closed = false;
        int maxIter = (int)boundaryEdges.size() + 1;
        int iter = 0;

        while (iter < maxIter) {
            iter++;
            int current = loop.back();
            int prev = loop[loop.size() - 2];
            bool found = false;

            for (int neighbor : adjacency[current]) {
                // Skip the vertex we came from
                if (neighbor == prev) continue;

                HoleEdgeKey candidateEdge(current, neighbor);
                if (usedEdges.count(candidateEdge)) continue;

                // Check if this leads back to the start
                if (neighbor == loop[0]) {
                    usedEdges.insert(candidateEdge);
                    closed = true;
                    found = true;
                    break;
                }

                // Extend the loop
                usedEdges.insert(candidateEdge);
                loop.push_back(neighbor);
                found = true;
                break;
            }

            if (!found) break;
            if (closed) break;
        }

        if (loop.size() >= 3 && closed) {
            loops.push_back(loop);
        } else {
            // Rollback: remove edges added in this failed attempt
            for (size_t i = 0; i < loop.size() - 1; ++i) {
                usedEdges.erase(HoleEdgeKey(loop[i], loop[i + 1]));
            }
        }
    }

    return loops;
}

// Compute normal of a loop (average of edge cross products)
static Vector3 computeLoopNormal(const OBJMesh& mesh, const std::vector<int>& loop) {
    Vector3 normal;
    int n = loop.size();

    for (int i = 0; i < n; ++i) {
        int next = (i + 1) % n;
        Vector3 vi = mesh.vertices[loop[i]];
        Vector3 vj = mesh.vertices[loop[next]];
        Vector3 edge = vj - vi;
        // Cross position with edge to get normal contribution
        normal = normal + Vector3::cross(vi, edge);
    }

    return normal.normalized();
}

// Triangulate a loop using fan triangulation from the first vertex
static std::vector<OBJFace> fanTriangulate(const std::vector<int>& loop) {
    std::vector<OBJFace> faces;
    int n = loop.size();

    if (n < 3) return faces;

    for (int i = 1; i < n - 1; ++i) {
        OBJFace face;
        face.v0 = loop[0];
        face.v1 = loop[i];
        face.v2 = loop[i + 1];
        faces.push_back(face);
    }

    return faces;
}

// Orient the loop so its normal aligns with surrounding face normals
static void orientLoop(const OBJMesh& mesh, std::vector<int>& loop) {
    if (loop.size() < 3) return;

    // Compute loop normal
    Vector3 loopNormal = computeLoopNormal(mesh, loop);

    // Find adjacent faces and compute average normal
    Vector3 adjacentNormal;
    int count = 0;

    for (const auto& face : mesh.faces) {
        int verts[3] = {face.v0, face.v1, face.v2};
        for (int e = 0; e < 3; ++e) {
            int va = verts[e];
            int vb = verts[(e + 1) % 3];
            // Check if this face shares a vertex with the loop
            for (int v : loop) {
                if (va == v || vb == v) {
                    Vector3 v0 = mesh.vertices[face.v0];
                    Vector3 v1 = mesh.vertices[face.v1];
                    Vector3 v2 = mesh.vertices[face.v2];
                    Vector3 faceNormal = Vector3::cross(v1 - v0, v2 - v0).normalized();
                    adjacentNormal = adjacentNormal + faceNormal;
                    count++;
                    break;
                }
            }
        }
    }

    if (count > 0) {
        adjacentNormal = adjacentNormal.normalized();
        // If loop normal points opposite to adjacent normals, reverse the loop
        float dot = Vector3::dot(loopNormal, adjacentNormal);
        if (dot < 0) {
            std::reverse(loop.begin(), loop.end());
        }
    }
}

// Remove degenerate faces and resolve non-manifold edges
// Result: each edge belongs to at most one face (manifold mesh)
static void removeBadGeometry(OBJMesh& mesh) {
    int removedDegenerate = 0;
    int removedNonManifold = 0;

    // Phase 1: Remove degenerate faces (zero-area, duplicate vertices)
    std::vector<OBJFace> goodFaces;
    for (const auto& face : mesh.faces) {
        // Check for duplicate vertices
        if (face.v0 == face.v1 || face.v1 == face.v2 || face.v0 == face.v2) {
            removedDegenerate++;
            continue;
        }
        // Check for zero-area (collinear vertices)
        Vector3 v0 = mesh.vertices[face.v0];
        Vector3 v1 = mesh.vertices[face.v1];
        Vector3 v2 = mesh.vertices[face.v2];
        Vector3 edge1 = v1 - v0;
        Vector3 edge2 = v2 - v0;
        Vector3 cross = Vector3::cross(edge1, edge2);
        float area = cross.length();
        if (area < 1e-12f) {
            removedDegenerate++;
            continue;
        }
        goodFaces.push_back(face);
    }
    mesh.faces = std::move(goodFaces);

    // Phase 2: Resolve non-manifold edges
    // Iteratively remove faces that share edges with too many neighbors
    bool changed = true;
    while (changed) {
        changed = false;

        // Build edge -> face list
        std::map<HoleEdgeKey, std::vector<size_t>> edgeToFaces;
        for (size_t i = 0; i < mesh.faces.size(); ++i) {
            const auto& f = mesh.faces[i];
            int verts[3] = {f.v0, f.v1, f.v2};
            for (int e = 0; e < 3; ++e) {
                int va = verts[e];
                int vb = verts[(e + 1) % 3];
                edgeToFaces[HoleEdgeKey(va, vb)].push_back(static_cast<int>(i));
            }
        }

        // Find non-manifold edges (shared by more than 2 faces)
        std::set<int> facesToRemove;
        for (const auto& [key, faceIndices] : edgeToFaces) {
            if ((int)faceIndices.size() > 2) {
                // Keep the first two faces, remove the rest
                for (size_t i = 2; i < faceIndices.size(); ++i) {
                    facesToRemove.insert(faceIndices[i]);
                }
            }
        }

        if (!facesToRemove.empty()) {
            changed = true;
            removedNonManifold += facesToRemove.size();

            // Build new face list without removed faces
            std::vector<OBJFace> keptFaces;
            for (size_t i = 0; i < mesh.faces.size(); ++i) {
                if (facesToRemove.find(static_cast<int>(i)) == facesToRemove.end()) {
                    keptFaces.push_back(mesh.faces[i]);
                }
            }
            mesh.faces = std::move(keptFaces);
        }
    }

    std::cout << "  Removed " << removedDegenerate << " degenerate faces, "
              << removedNonManifold << " non-manifold faces" << std::endl;
}

// Main function: Find and close holes in the mesh
static void closeHoles(OBJMesh& mesh) {
    if (mesh.faces.empty()) return;

    auto startTime = std::chrono::steady_clock::now();

    // Step 1: Find boundary edges
    std::vector<HoleEdgeKey> boundaryEdges = findBoundaryEdges(mesh);
    std::cout << "  Found " << boundaryEdges.size() << " boundary edges" << std::endl;

    if (boundaryEdges.empty()) {
        std::cout << "  No holes found (mesh is watertight)" << std::endl;
        return;
    }

    // Step 2: Group into loops
    std::vector<std::vector<int>> loops = groupBoundaryLoops(boundaryEdges);
    std::cout << "  Found " << loops.size() << " hole(s)" << std::endl;

    // Step 3: Close each hole
    int totalNewFaces = 0;
    for (size_t i = 0; i < loops.size(); ++i) {
        const auto& loop = loops[i];
        std::cout << "  Hole " << (i + 1) << ": " << loop.size() << " vertices" << std::endl;

        // Orient the loop to match surrounding geometry
        orientLoop(mesh, const_cast<std::vector<int>&>(loop));

        // Triangulate the loop
        std::vector<OBJFace> newFaces = fanTriangulate(loop);
        std::cout << "    Added " << newFaces.size() << " faces" << std::endl;

        // Add new faces to mesh
        for (const auto& face : newFaces) {
            mesh.faces.push_back(face);
            totalNewFaces++;
        }
    }

    auto endTime = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(endTime - startTime).count();
    std::cout << "  Closed " << loops.size() << " hole(s) with " << totalNewFaces
              << " new faces (" << std::fixed << elapsed << "s)" << std::endl;
}
