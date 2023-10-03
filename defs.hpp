
#ifndef HOST_DEFS_HPP
#define HOST_DEFS_HPP

#include <cmath>
#include <vector>
#include <array>
#include <limits>
#include <ranges>
#include <algorithm>

// assumptions for serialization
static_assert(std::numeric_limits<unsigned char>::digits == 8, "");
static_assert(sizeof(int) == sizeof(int32_t), "");

using byte = unsigned char;
using uint = unsigned int;
namespace ranges = std::ranges;


inline uint32_t to_fixedpt(float val)
{
    static_assert(sizeof(long) == sizeof(uint32_t), "");
    return std::lround(double(val) * (1 << 16));
}

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

    static constexpr uint nsbytes = 12; // num serialized bytes

    void serialize(byte buf[nsbytes]) const
    {
        uint32_t fixed[3];
        fixed[0] = to_fixedpt(v[0]);
        fixed[1] = to_fixedpt(v[1]);
        fixed[2] = to_fixedpt(v[2]);
        std::memcpy(buf, fixed, nsbytes);
    }

private:
    float v[3];
};

inline vec3 operator+(const vec3& lhs, const vec3& rhs) noexcept
{
    return { lhs[0] + rhs[0], lhs[1] + rhs[1], lhs[2] + rhs[2] };
}
inline vec3 operator-(const vec3& lhs, const vec3& rhs) noexcept
{
    return { lhs[0] - rhs[0], lhs[1] - rhs[1], lhs[2] - rhs[2] };
}
inline vec3 operator*(float sc, const vec3& rhs) noexcept
{
    return { sc * rhs[0], sc * rhs[1], sc * rhs[2] };
}
inline vec3 operator*(const vec3& rhs, float sc) noexcept { return operator*(sc, rhs); }


// Material
struct mat
{
    // ambient, diffuse, specular, mirror coefficients (rgb)
    // determine km from ns using 1000-2000x+1000x^{2}
    vec3 ka, kd, ks, km;
    // specular (phong) exponent
    float ns;

    static constexpr uint nsbytes = 4 * vec3::nsbytes + 4;

    void serialize(byte buf[nsbytes]) const
    {
        auto* p = buf;
        ka.serialize(p); p += vec3::nsbytes;
        kd.serialize(p); p += vec3::nsbytes;
        ks.serialize(p); p += vec3::nsbytes;
        km.serialize(p); p += vec3::nsbytes;

        uint32_t ns_fixed = to_fixedpt(ns);
        std::memcpy(p, &ns_fixed, 4);
    }
};

// Texture coordinate
struct uv { float u, v; };

template <class T>
inline byte* vserialize(const std::vector<T>& v, byte* p)
{
    for (int i = 0; i < int(v.size()); ++i)
    {
        v[i].serialize(p);
        p += T::nsbytes;
    }
    return p;
}
inline byte* vserialize(const std::vector<int>& v, byte* p)
{
    for (int i = 0; i < int(v.size()); ++i)
    {
        std::memcpy(p, &v[i], 4);
        p += 4;
    }
    return p;
}

template <int N>
inline byte* vserialize(const std::vector<std::array<int, N>>& v, byte* p)
{
    for (int i = 0; i < int(v.size()); ++i) 
    {
        for (int j = 0; j < N; ++j) {
            std::memcpy(p, &v[i][j], 4);
            p += 4;
        }
    }
    return p;
}

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


    // serial sizes
    uint nsV() { return uint(V.size()) * vec3::nsbytes; }
    uint nsNV() { return uint(NV.size()) * vec3::nsbytes; }
    uint nsF() { return uint(F.size()) * sizeof(F[0]); }
    uint nsNF() { return uint(NF.size()) * sizeof(NF[0]); }
    uint nsM() { return uint(M.size()) * mat::nsbytes; }
    uint nsMF() { return uint(MF.size()) * sizeof(MF[0]); }

    uint nsbytes()
    {
        return
            // numV, numF, and NV, F, NF, M, MF offsets
            uint(4 + 4 + 4 + 4 + 4 + 4 + 4) +          
            nsV() + nsNV() + nsF() + nsNF() + nsM() + nsMF();
    }

    void serialize(byte buf[])
    {
        uint numV = uint(V.size());
        uint numF = uint(F.size());
        uint NVoff = nsV();
        uint Foff = NVoff + nsNV();
        uint NFoff = Foff + nsF();
        uint Moff = NFoff + nsNF();
        uint MFoff = Moff + nsM();

        auto* p = buf;
        std::memcpy(p, &numV, 4); p += 4;
        std::memcpy(p, &numF, 4); p += 4;
        std::memcpy(p, &NVoff, 4); p += 4;
        std::memcpy(p, &Foff, 4); p += 4;
        std::memcpy(p, &NFoff, 4); p += 4;
        std::memcpy(p, &Moff, 4); p += 4;
        std::memcpy(p, &MFoff, 4); p += 4;
        
        p = vserialize(V, p);
        p = vserialize(NV, p);
        p = vserialize(F, p);
        p = vserialize(NF, p);
        p = vserialize(M, p);
        p = vserialize(MF, p);
    }
};

struct BBox
{
    vec3 cmin; // Min corner
    vec3 cmax; // Max corner

    BBox() :
        cmin({ std::numeric_limits<float>::infinity() }),
        cmax({ -std::numeric_limits<float>::infinity() })
    {}

    vec3 center() { return 0.5 * (cmin + cmax); }

    static constexpr std::size_t nsbytes = 2 * vec3::nsbytes;

    void serialize(byte buf[nsbytes])
    {
        cmin.serialize(buf);
        cmax.serialize(buf + vec3::nsbytes);
    }
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
bool read_wavefront_model(const char* obj_file, const char* mtl_file, ModelData& model);

// Write .obj + .mtl files.
bool write_wavefront_model(const char* obj_file, const char* mtl_file, ModelData& model);



#endif