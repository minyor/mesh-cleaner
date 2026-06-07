#pragma once

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

struct Vector3 {
    float x, y, z;

    Vector3() : x(0), y(0), z(0) {}
    Vector3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

    Vector3 operator-(const Vector3& other) const {
        return Vector3(x - other.x, y - other.y, z - other.z);
    }

    Vector3 operator+(const Vector3& other) const {
        return Vector3(x + other.x, y + other.y, z + other.z);
    }

    Vector3 operator*(float scalar) const {
        return Vector3(x * scalar, y * scalar, z * scalar);
    }

    bool operator==(const Vector3& other) const {
        return x == other.x && y == other.y && z == other.z;
    }

    bool operator<(const Vector3& other) const {
        if (x != other.x) return x < other.x;
        if (y != other.y) return y < other.y;
        return z < other.z;
    }

    static Vector3 cross(const Vector3& a, const Vector3& b) {
        return Vector3(
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        );
    }

    static float dot(const Vector3& a, const Vector3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    float length() const {
        return std::sqrt(x * x + y * y + z * z);
    }

    Vector3 normalized() const {
        float len = length();
        if (len < 1e-8f) return Vector3();
        return Vector3(x / len, y / len, z / len);
    }

    static float dist(const Vector3& a, const Vector3& b) {
        Vector3 d = a - b;
        return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
    }
};

struct OBJFace {
    int v0, v1, v2; // vertex indices (0-based)
};

struct OBJMesh {
    std::string name;
    std::vector<Vector3> vertices;
    std::vector<OBJFace> faces;
};

static bool loadOBJ(const std::string& filepath, OBJMesh& mesh) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file " << filepath << std::endl;
        return false;
    }

    mesh.name = "MeshCleaner";
    std::string line;

    while (std::getline(file, line)) {
        // Skip empty lines and comments
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        if (line[start] == '#') continue;

        if (line.substr(start, 2) == "o " || line.substr(start, 2) == "g ") {
            // Object or group name
            mesh.name = line.substr(start + 2);
            size_t end = mesh.name.find_first_of(" \t\r\n");
            if (end != std::string::npos) mesh.name = mesh.name.substr(0, end);
        }
        else if (line.substr(start, 2) == "v ") {
            float x, y, z;
            sscanf(line.c_str(), "v %f %f %f", &x, &y, &z);
            mesh.vertices.push_back(Vector3(x, y, z));
        }
        else if (line.substr(start, 2) == "f ") {
            OBJFace face;
            // Parse face: f v1 v2 v3 (or f v1/vt1/vn1 v2/vt2/vn2 v3/vt3/vn3)
            char buf[256];
            snprintf(buf, sizeof(buf), "%s", line.c_str());
            char* token = strtok(buf, " \t\r\n");
            token = strtok(nullptr, " \t\r\n"); // skip "f"

            auto parseVertexIndex = [](const char* vertStr) -> int {
                // Handle v, v//vn, v/vt/vn formats - extract first number
                int idx;
                sscanf(vertStr, "%d", &idx);
                return idx - 1; // OBJ is 1-based
            };

            face.v0 = parseVertexIndex(token);
            token = strtok(nullptr, " \t\r\n");
            face.v1 = parseVertexIndex(token);
            token = strtok(nullptr, " \t\r\n");
            face.v2 = parseVertexIndex(token);

            mesh.faces.push_back(face);
        }
    }

    file.close();
    return true;
}

static void saveOBJ(const std::string& filepath, const OBJMesh& mesh) {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open output file " << filepath << std::endl;
        return;
    }

    file << "# " << mesh.name << std::endl;
    file << "o " << mesh.name << std::endl;

    for (const auto& v : mesh.vertices) {
        file << "v " << v.x << " " << v.y << " " << v.z << std::endl;
    }

    for (const auto& f : mesh.faces) {
        file << "f " << (f.v0 + 1) << " " << (f.v1 + 1) << " " << (f.v2 + 1) << std::endl;
    }

    file.close();
}

// Flip a face by swapping v1 and v2
static void flipFace(OBJFace& face) {
    int temp = face.v1;
    face.v1 = face.v2;
    face.v2 = temp;
}