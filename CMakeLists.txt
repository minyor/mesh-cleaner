cmake_minimum_required(VERSION 3.10)
project(MeshCleaner LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(MeshCleaner main.cpp)

# Optional: enable warnings
target_compile_options(MeshCleaner PRIVATE -Wall -Wextra)