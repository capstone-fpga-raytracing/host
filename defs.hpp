
#ifndef HOST_DEFS_HPP
#define HOST_DEFS_HPP

#include <cstring>
#include <cassert>
#include <cmath>
#include <vector>
#include <array>
#include <limits>
#include <ranges>
#include <algorithm>
#ifdef _MSC_VER
#include <bit>
#endif

#define TEST_SCENE 1
#define TEST_MODELIO 0
#define ENABLE_BSWAP 1


static_assert(std::numeric_limits<unsigned char>::digits == 8);
static_assert(sizeof(long) == sizeof(uint32_t) || sizeof(long) == sizeof(uint64_t));
static_assert(sizeof(int) == sizeof(int32_t));

using byte = unsigned char;
using uint = unsigned int;
namespace ranges = std::ranges;


inline uint32_t to_fixedpt(float val)
{
    double v = double(val) * (1 << 16);
    
    if constexpr (sizeof(long) == sizeof(uint32_t))
        return std::lround(v);
    else if constexpr (sizeof(long) == sizeof(uint64_t))
        return std::llround(v);
}

inline uint32_t bswap(uint32_t v)
{
#if ENABLE_BSWAP
#ifdef _MSC_VER
    return std::byteswap(v);
#else
    return __builtin_bswap32(v);
#endif
#else
    return v;
#endif
}

// 3d vector
struct vec3
{
    vec3() = default;

    constexpr vec3(float x, float y, float z)
    {
        v[0] = x;
        v[1] = y;
        v[2] = z;
    }

    constexpr vec3(float val) : 
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
        float norm = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
        v[0] /= norm; v[1] /= norm; v[2] /= norm;
    }

    vec3 cross(vec3 rhs)
    {
        return {
            y() * rhs.z() - z() * rhs.y(),
            z() * rhs.x() - x() * rhs.z(),
            x() * rhs.y() - y() * rhs.x() };
    }

    static constexpr uint nserial = 12;

    void serialize(byte buf[nserial]) const
    {
        uint d[3];
        d[0] = bswap(to_fixedpt(v[0]));
        d[1] = bswap(to_fixedpt(v[1]));
        d[2] = bswap(to_fixedpt(v[2]));
        std::memcpy(buf, d, 12);
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
    vec3 ka, kd, ks, km;
    // specular (phong) exponent
    // determine roughness from ns using 1000-2000x+1000x^{2}
    float ns;

    // Default for faces without material.
    static constexpr mat default_mat()
    {
        // these numbers from Blender.
        // (gray plastic)
        mat m;
        m.ka = { 1.f, 1.f, 1.f };
        m.kd = { 0.8f, 0.8f, 0.8f };
        m.ks = { 0.5f, 0.5f, 0.5f };
        m.km = { 0.05f, 0.05f, 0.05f };
        m.ns = 250.f;
        return m;
    }

    static constexpr uint nserial = 4 * vec3::nserial + 4;

    void serialize(byte buf[nserial]) const
    {
        auto* p = buf;
        ka.serialize(p); p += vec3::nserial;
        kd.serialize(p); p += vec3::nserial;
        ks.serialize(p); p += vec3::nserial;
        km.serialize(p); p += vec3::nserial;

        uint d = bswap(to_fixedpt(ns));
        std::memcpy(p, &d, 4);
    }
};

// Texture coordinate
struct uv { float u, v; };

struct light
{
    vec3 pos; // position
    vec3 rgb; // color

    static constexpr uint nserial = 2 * vec3::nserial;

    void serialize(byte buf[nserial]) const
    {
        pos.serialize(buf);
        rgb.serialize(buf + vec3::nserial);
    }
};

struct camera
{
    vec3 eye; // position
    vec3 u, v, w; // axes (-w is viewing direction)
    float focal_len; // focal length (distance to img plane)
    float width, height; // size of projected image (in world space)

    static constexpr uint nserial = 4 * vec3::nserial + 12;

    void serialize(byte buf[nserial]) const
    {
        auto* p = buf;
        eye.serialize(p); p += vec3::nserial;
        u.serialize(p); p += vec3::nserial;
        v.serialize(p); p += vec3::nserial;
        w.serialize(p); p += vec3::nserial;

        uint d[3];
        d[0] = bswap(to_fixedpt(focal_len));
        d[1] = bswap(to_fixedpt(width));
        d[2] = bswap(to_fixedpt(height));
        std::memcpy(p, d, 12);
    }
};

template <class T>
inline byte* vserialize(const std::vector<T>& v, byte* p)
{
    for (int i = 0; i < int(v.size()); ++i)
    {
        v[i].serialize(p);
        p += T::nserial;
    }
    return p;
}
inline byte* vserialize(const std::vector<int>& v, byte* p)
{
    for (int i = 0; i < int(v.size()); ++i)
    {
        uint d = bswap(uint(v[i]));
        std::memcpy(p, &d, 4);
        p += 4;
    }
    return p;
}

inline byte* vserialize(const std::vector<std::array<int, 3>>& v, byte* p)
{
    for (int i = 0; i < int(v.size()); ++i) 
    {
        for (int j = 0; j < 3; ++j) 
        {
            uint d = bswap(uint(v[i][j]));
            std::memcpy(p, &d, 4);
            p += 4;
        }
    }
    return p;
}

struct SceneData
{
    // scene
    camera C;
    std::vector<light> L; // lights
    
    // geometry
    std::vector<vec3> V; // Vertices 
    std::vector<vec3> NV; // Normals
    std::vector<std::array<int, 3>> F; // Face indices
    std::vector<std::array<int, 3>> NF; // Face normal indices

    // materials
    std::vector<mat> M; // Materials
    std::vector<int> MF; // Face material indices

    // textures (not serialized for now)
    std::vector<uv> UV;// Texture coords
    std::vector<std::array<int, 3>> UF; // Face texture coord indices

    uint nserial() const;
    void serialize(byte buf[]) const;


private:
    static constexpr int nwordshdr = 11; // # words in header
    static constexpr uint nserialhdr = 4 * nwordshdr; // serial size of header

    // sizes of each member when serialized
    uint nsL()  const { return uint(L.size() * light::nserial); }
    uint nsV()  const { return uint(V.size() * vec3::nserial); }
    uint nsNV() const { return uint(NV.size() * vec3::nserial); }
    uint nsF()  const { return uint(F.size() * sizeof(F[0])); }
    uint nsNF() const { return uint(NF.size() * sizeof(NF[0])); }
    uint nsM()  const { return uint(M.size() * mat::nserial); }
    uint nsMF() const { return uint(MF.size() * sizeof(MF[0])); }
};

// Read .obj + .mtl files.
bool read_model(const char* obj_file, SceneData& model);

// Write .obj + .mtl files.
bool write_model(const char* obj_file, const char* mtl_file, SceneData& model);


struct BBox
{
    vec3 cmin; // Min corner
    vec3 cmax; // Max corner

    BBox() :
        cmin(std::numeric_limits<float>::infinity()),
        cmax(-std::numeric_limits<float>::infinity())
    {}

    vec3 center() { return 0.5 * (cmin + cmax); }

    static constexpr uint nserial = 2 * vec3::nserial;

    void serialize(byte buf[nserial]) const
    {
        cmin.serialize(buf);
        cmax.serialize(buf + vec3::nserial);
    }
};

struct BVNode
{
    // if tri==-1 this node is a bbox, else it
    // is a triangle node with face index 'tri'
    int tri;
    int ndesc; // num descendants (leaves + tree nodes)
    int nleaves; // num leaves
    BBox bbox;
    BVNode* left;
    BVNode* right;
};

// Bounding-volume tree (axis-aligned)
struct BVTree
{
    BVTree(const SceneData& model);
    ~BVTree();
    
    BVNode* root() { return m_root; }

    uint nserial() const;
    void serialize(byte buf[]) const;

private:
    BVNode* m_root;
};

#endif