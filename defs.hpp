
#ifndef HOST_DEFS_HPP
#define HOST_DEFS_HPP

#include <cstring>
#include <cassert>
#include <cstdio>
#include <cmath>
#include <vector>
#include <array>
#include <limits>
#include <memory>
#include <ranges>
#include <algorithm>
#include <filesystem>
#ifdef _MSC_VER
#include <bit>
#endif

#include "rapidobj/rapidobj.hpp"


#define ENABLE_TEXTURES 0
#define CONCAT(x, y) x##y

namespace ranges = std::ranges;
namespace fs = std::filesystem;

using uint = unsigned int;
using byte = unsigned char;
static_assert(std::numeric_limits<int>::digits == 31, "int is not 32-bit");

using scopedFILE = std::unique_ptr<std::FILE, int(*)(std::FILE*)>;

template <typename T>
using scopedMallocPtr = std::unique_ptr<T, void(*)(void*)>;

//
// On Linux, ifstream.read() and fread() are just as fast.
// On Windows, fread() is about 2x as fast as ifstream.read()
// https://gist.github.com/mayawarrier/7ed71f1f91a7f8588b7f8bd96a561892
//
#ifdef _MSC_VER
#define SAFE_AFOPEN(fname, mode) scopedFILE(std::fopen(fname, mode), std::fclose)
#define SAFE_FOPEN(fname, mode) scopedFILE(::_wfopen(fname, CONCAT(L, mode)), std::fclose)
#else
#define SAFE_AFOPEN(fname, mode) scopedFILE(std::fopen(fname, mode), std::fclose)
#define SAFE_FOPEN(fname, mode) SAFE_AFOPEN(fname, mode)
#endif

template <typename T, typename Ptr>
scopedMallocPtr<T> scoped_mallocptr(Ptr ptr) { return { ptr, std::free }; }


#define WS " \t\n\r\f\v"

inline void rtrim(std::string& str) {
    str.erase(str.find_last_not_of(WS) + 1);
}

inline uint to_fixedpt(double val)
{
    return uint(std::lround(val * (1 << 16)));
}

inline double from_fixedpt(uint val)
{
    // int(val) interprets the bits of val as a signed number, 
    // which only works from c++20 onwards
    return double(int(val)) / (1 << 16);
}

inline uint bswap(uint v)
{
#ifdef _MSC_VER
    return std::byteswap(v);
#else
    return __builtin_bswap32(v);
#endif
}

// 3d vector
struct vec3
{
    vec3() = default;

    constexpr vec3(double x, double y, double z)
    {
        v[0] = x;
        v[1] = y;
        v[2] = z;
    }

    template <typename T>
    constexpr vec3(std::array<T, 3> a)
    {
        v[0] = a[0];
        v[1] = a[1];
        v[2] = a[2];
    }

    constexpr vec3(double val) : 
        vec3(val, val, val) 
    {}

    double& x() { return v[0]; }
    double& y() { return v[1]; }
    double& z() { return v[2]; }

    double x() const { return v[0]; }
    double y() const { return v[1]; }
    double z() const { return v[2]; }

    double& operator[](int pos) { return v[pos]; }
    const double& operator[](int pos) const { return v[pos]; }

    double dot(vec3 rhs) const 
    {
        return x() * rhs.x() + y() * rhs.y() + z() * rhs.z();
    }

    vec3 cross(vec3 rhs) const
    {
        return {
            y() * rhs.z() - z() * rhs.y(),
            z() * rhs.x() - x() * rhs.z(),
            x() * rhs.y() - y() * rhs.x() };
    }

    double norm() const 
    {
        return std::sqrt(x() * x() + y() * y() + z() * z());
    }

    void normalize()
    {
        double n = norm();
        x() /= n; y() /= n; z() /= n;
    }

    vec3 normalized() const
    {
        double n = norm();
        return { x() / n, y() / n, z() / n };
    }

    vec3 cwiseMin(vec3 rhs) const
    {
        return {
            std::min(v[0], rhs.v[0]),
            std::min(v[1], rhs.v[1]),
            std::min(v[2], rhs.v[2]) };
    }

    vec3 cwiseMax(vec3 rhs) const
    {
        return {
            std::max(v[0], rhs.v[0]),
            std::max(v[1], rhs.v[1]),
            std::max(v[2], rhs.v[2]) };
    }

    int maxDim() const { return int(std::max_element(v, v + 3) - v); }

    static constexpr uint nserial = 3;

    void serialize(uint* p) const
    {
        p[0] = to_fixedpt(x());
        p[1] = to_fixedpt(y());
        p[2] = to_fixedpt(z());
    }

private:
    double v[3];
};

inline vec3 operator+(const vec3& lhs, const vec3& rhs) noexcept
{
    return { lhs[0] + rhs[0], lhs[1] + rhs[1], lhs[2] + rhs[2]};
}
inline vec3 operator-(const vec3& lhs, const vec3& rhs) noexcept
{
    return { lhs[0] - rhs[0], lhs[1] - rhs[1], lhs[2] - rhs[2] };
}
inline vec3 operator*(double sc, const vec3& rhs) noexcept
{
    return { sc * rhs[0], sc * rhs[1], sc * rhs[2] };
}
inline vec3 operator*(const vec3& rhs, double sc) noexcept { return operator*(sc, rhs); }


// Material
struct mat
{
    // ambient, diffuse, specular, reflection coefficients (rgb)
    // range: [0, 1]
    vec3 ka, kd, ks, km;
    // specular (phong) exponent
    double ns;

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

    static constexpr uint nserial = 4 * vec3::nserial + 1;

    void serialize(uint* p) const
    {
        ka.serialize(p); p += vec3::nserial;
        kd.serialize(p); p += vec3::nserial;
        ks.serialize(p); p += vec3::nserial;
        km.serialize(p); p += vec3::nserial;

        p[0] = to_fixedpt(ns);
    }
};

// Texture coordinate
struct uv { double u, v; };

struct light
{
    vec3 pos; // position
    vec3 rgb; // color, range [0, 1]

    static constexpr uint nserial = 2 * vec3::nserial;

    void serialize(uint* p) const
    {
        pos.serialize(p);
        rgb.serialize(p + vec3::nserial);
    }
};

struct camera
{
    vec3 eye; // position
    vec3 u, v, w; // rotation axes (-w is viewing direction)
    double focal_len; // focal length (distance to img plane)
    double width, height; // projected img size (in world space)

    static constexpr uint nserial = 4 * vec3::nserial + 3;

    void serialize(uint* p) const
    {
        eye.serialize(p); p += vec3::nserial;
        u.serialize(p); p += vec3::nserial;
        v.serialize(p); p += vec3::nserial;
        w.serialize(p); p += vec3::nserial;

        p[0] = to_fixedpt(focal_len);
        p[1] = to_fixedpt(width);
        p[2] = to_fixedpt(height);
    }
};

using TriIndexVector = std::vector<std::array<int, 3>>;

struct SceneData
{
    SceneData(const fs::path& scene_path);

    // scene
    camera C; // camera
    std::vector<light> L; // lights
    std::pair<uint, uint> R; // resolution
    
    // geometry
    std::vector<vec3> V; // Vertices 
    std::vector<vec3> NV; // Normals
    TriIndexVector F; // Face indices
    TriIndexVector NF; // Face normal indices

    // materials
    std::vector<mat> M; // Materials
    std::vector<int> MF; // Face material indices

    // textures (not serialized for now)
    std::vector<uv> UV;// Texture coords
    TriIndexVector UF; // Face texture coord indices


    bool ok() const { return m_ok; }
    operator bool() const { return ok(); }

    uint nserial() const;
    void serialize(uint* buf) const;

private:
    static constexpr int Nwordshdr = 13; // # words in header

    // sizes of each member when serialized
    uint nsL()  const { return uint(L.size() * light::nserial); }
    uint nsV()  const { return uint(V.size() * vec3::nserial); }
    uint nsNV() const { return uint(NV.size() * vec3::nserial); }
    uint nsF()  const { return uint(F.size() * F[0].size()); }
    uint nsNF() const { return uint(NF.size() * NF[0].size()); }
    uint nsM()  const { return uint(M.size() * mat::nserial); }
    uint nsMF() const { return uint(MF.size()); }

private:
    bool m_ok;
};

template <class T>
inline uint* vserialize(const std::vector<T>& v, uint* p)
{
    for (size_t i = 0; i < v.size(); ++i)
    {
        v[i].serialize(p);
        p += T::nserial;
    }
    return p;
}

inline uint* vserialize(const std::vector<int>& v, uint* p)
{
    std::copy(v.begin(), v.end(), p);
    return p + v.size();
}

inline uint* vserialize(const TriIndexVector& v, uint* p)
{
    for (size_t i = 0; i < v.size(); ++i)
    {
        std::copy(v[i].begin(), v[i].end(), p);
        p += 3;
    }
    return p;
}

struct BBox
{
    vec3 cmin; // Min corner
    vec3 cmax; // Max corner

    BBox() :
        cmin(std::numeric_limits<double>::infinity()),
        cmax(-std::numeric_limits<double>::infinity())
    {}

    vec3 center() const { return 0.5 * (cmin + cmax); }

    static constexpr uint nserial = 2 * vec3::nserial;

    void serialize(uint* p) const
    {
        cmin.serialize(p);
        cmax.serialize(p + vec3::nserial);
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

    bool ok() const { return m_ok; }
    operator bool() const { return ok(); }

    uint nserial() const;
    void serialize(uint* buf) const;

private:
    BVNode* m_root;
    bool m_ok;
};

#endif