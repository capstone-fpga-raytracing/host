
#ifndef HOST_DEFS_HPP
#define HOST_DEFS_HPP

#include <cmath>
#include <vector>
#include <array>

struct uv { float u, v; };
struct xyz
{
    float x, y, z;

    void normalize()
    {
        float norm = std::sqrtf(x * x + y * y + z * z);
        x /= norm; y /= norm; z /= norm;
    }
};

struct ModelGeom
{
    std::vector<xyz> V; // Vertices
    std::vector<uv> UV;// Texture coords
    std::vector<xyz> NV; // Normal vectors
    std::vector<std::array<int, 3>> F; // Face indices
    std::vector<std::array<int, 3>> UF; // Face texture indices
    std::vector<std::array<int, 3>> NF; // Face normal indices
};

// Read .obj model file format.
bool read_obj_model(const char* filename, ModelGeom& model);

// Write .obj model file format.
bool write_obj_model(const char* filename, ModelGeom& model);

#endif