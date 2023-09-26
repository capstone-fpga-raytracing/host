
#ifndef HOST_DEFS_HPP
#define HOST_DEFS_HPP

#include <cmath>
#include <vector>
#include <array>
#include <algorithm>


// 3d vector
struct vec3
{
    vec3() = default;

    vec3(float x, float y, float z)
    {
        v[0] = x;
        v[1] = y;
        v[2] = z;
    }

    vec3(float val) : 
        vec3(val, val, val) 
    {}

    float& x() { return v[0]; }
    float& y() { return v[1]; }
    float& z() { return v[2]; }

    float& operator[](int pos) { return v[pos]; }
    const float& operator[](int pos) const { return v[pos]; }

    vec3 cwiseMin(vec3 rhs)
    {
        return {
            std::min(v[0], rhs.v[0]),
            std::min(v[1], rhs.v[1]),
            std::min(v[2], rhs.v[2]) };
    }
    vec3 cwiseMax(vec3 rhs)
    {
        return {
            std::max(v[0], rhs.v[0]),
            std::max(v[1], rhs.v[1]),
            std::max(v[2], rhs.v[2]) };
    }

    int maxDim() { return int(std::max_element(v, v + 3) - v); }

    void normalize()
    {
        float norm = std::sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
        v[0] /= norm; v[1] /= norm; v[2] /= norm;
    }

    float v[3];
};

inline vec3 operator+(const vec3& lhs, const vec3& rhs)
{
    return { lhs[0] + rhs[0], lhs[1] + rhs[1], lhs[2] + rhs[2] };
}
inline vec3 operator-(const vec3& lhs, const vec3& rhs)
{
    return { lhs[0] - rhs[0], lhs[1] - rhs[1], lhs[2] - rhs[2] };
}
inline vec3 operator*(float sc, const vec3& rhs)
{
    return { sc * rhs[0], sc * rhs[1], sc * rhs[2] };
}
inline vec3 operator*(const vec3& rhs, float sc) { return operator*(sc, rhs); }


// Material
struct mat
{
    // ambient, diffuse, specular, mirror coefficients (rgb)
    // determine km from ns using 1000-2000x+1000x^{2}
    vec3 ka, kd, ks, km;
    // specular (phong) exponent
    float ns;
};

// Texture coordinate
struct uv { float u, v; };

struct ModelData
{
    // geometry
    std::vector<vec3> V; // Vertices 
    std::vector<vec3> NV; // Normals
    std::vector<std::array<int, 3>> F; // Face indices
    std::vector<std::array<int, 3>> NF; // Face normal indices

    // materials
    std::vector<mat> M; // Materials
    std::vector<int> MF; // Face material indices

    // texture data (if we have time)
    std::vector<uv> UV;// Texture coords
    std::vector<std::array<int, 3>> UF; // Face texture coord indices
};

struct BBox
{
    vec3 cmin; // Min corner
    vec3 cmax; // Max corner

    vec3 center() { return 0.5 * (cmin + cmax); }

    BBox() :
        cmin({ std::numeric_limits<float>::infinity() }),
        cmax({ -std::numeric_limits<float>::infinity() })
    {}
};

struct BVNode
{
    BBox bbox;
    BVNode* left;
    BVNode* right;
    // if tri==-1 this node is a bbox, else it
    // is a triangle node with face index 'tri'
    int tri;
};

// Bounding-volume tree (axis-aligned)
struct BVTree
{
    BVTree(const ModelData& model);
    ~BVTree();

    BVNode* root() { return m_root; }

private:
    BVNode* m_root;
};


// Read .obj + .mtl files.
bool read_model(const char* obj_file, const char* mtl_file, ModelData& model);

// Write .obj + .mtl files.
bool write_model(const char* obj_file, const char* mtl_file, ModelData& model);



#endif