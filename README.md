# MeshCleaner

Remove degenerate faces, non-manifold edges, and close holes in OBJ meshes.

## Features

- **Remove degenerate faces** - Eliminates zero-area faces and faces with duplicate vertices
- **Resolve non-manifold edges** - Removes excess faces sharing edges with more than 2 faces
- **Close holes** - Detects boundary edge loops and fills them with fan triangulation
- **Orient normals** - BFS-based normal orientation for consistent winding

## Build

```bash
mkdir build && cd build
cmake ..
make
```

## Usage

```bash
# Remove bad geometry and close holes (overwrite input file)
./build/MeshCleaner input.obj

# Remove bad geometry and close holes (save to output file)
./build/MeshCleaner input.obj output.obj

# Same as above, then orient normals
./build/MeshCleaner rewind input.obj
./build/MeshCleaner rewind input.obj output.obj
```

## License

MIT