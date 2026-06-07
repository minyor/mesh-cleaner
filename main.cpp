#include <iostream>
#include <string>

#include "util.h"
#include "rewind.h"
#include "hole_close.h"

static void printUsage(const char* programName) {
    std::cerr << "MeshCleaner - Remove degenerate faces, non-manifold edges, and close holes in OBJ meshes." << std::endl;
    std::cerr << std::endl;
    std::cerr << "Usage:" << std::endl;
    std::cerr << "  " << programName << " <input.obj>                                Remove bad geometry and close holes (overwrite)" << std::endl;
    std::cerr << "  " << programName << " <input.obj> <output.obj>                   Remove bad geometry and close holes" << std::endl;
    std::cerr << "  " << programName << " rewind <input.obj> [<output.obj>]          Same as above, then orient normals" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 4) {
        printUsage(argv[0]);
        return 1;
    }

    std::string operation;
    std::string inputPath;
    std::string outputPath;

    // Check if first argument is a mode keyword
    bool hasMode = (argc >= 3 && (std::string(argv[1]) == "rewind"));

    if (hasMode) {
        operation = argv[1];
        inputPath = argv[2];
        if (argc == 4) {
            outputPath = argv[3];
        } else {
            outputPath = inputPath;
        }
    } else {
        inputPath = argv[1];
        if (argc == 3) {
            outputPath = argv[2];
        } else {
            outputPath = inputPath;
        }
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