#include <iostream>
#include <string>

#include "util.h"
#include "rewind.h"
#include "hole_close.h"

static void printUsage(const char* programName) {
    std::cerr << "MeshCleaner - Remove degenerate faces, non-manifold edges, and close holes in OBJ meshes." << std::endl;
    std::cerr << std::endl;
    std::cerr << "Usage:" << std::endl;
    std::cerr << "  " << programName << " <input.obj> <output.obj>                  Remove bad geometry and close holes" << std::endl;
    std::cerr << "  " << programName << " rewind <input.obj> <output.obj>           Same as above, then orient normals" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc != 3 && argc != 4) {
        printUsage(argv[0]);
        return 1;
    }

    std::string operation;
    std::string inputPath;
    std::string outputPath;

    if (argc == 3) {
        inputPath = argv[1];
        outputPath = argv[2];
    } else {
        operation = argv[1];
        inputPath = argv[2];
        outputPath = argv[3];
    }

    std::cout << "Loading OBJ file: " << inputPath << std::endl;
    OBJMesh mesh;
    if (!loadOBJ(inputPath, mesh)) {
        std::cerr << "Error: Failed to load OBJ file" << std::endl;
        return 1;
    }
    std::cout << "Loaded " << mesh.faces.size() << " faces, " << mesh.vertices.size() << " vertices." << std::endl;

    // Always run cleanup and hole closing first
    std::cout << "Removing bad geometry..." << std::endl;
    removeBadGeometry(mesh);
    std::cout << "Mesh after cleanup: " << mesh.faces.size() << " faces." << std::endl;

    std::cout << "Finding and closing holes..." << std::endl;
    closeHoles(mesh);

    // Then execute mode-specific operation
    if (operation.empty()) {
        // No mode specified - just close holes
    } else if (operation == "rewind") {
        std::cout << "Orienting normals via BFS..." << std::endl;
        orientNormalsBFS(mesh);
    } else {
        std::cerr << "Error: Unknown operation '" << operation << "'" << std::endl;
        printUsage(argv[0]);
        return 1;
    }

    std::cout << "Saving to: " << outputPath << std::endl;
    saveOBJ(outputPath, mesh);
    std::cout << "Done. Processed " << mesh.faces.size() << " faces." << std::endl;

    return 0;
}