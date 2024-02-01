
#ifndef HOST_DEFS_HPP
#define HOST_DEFS_HPP

#include <cstring>
#include <cassert>
#include <cmath>
#include <limits>
#include <vector>
#include <array>

#include "rapidobj/rapidobj.hpp"
#include "utils.hpp"

#define ENABLE_TEXTURES 0

#define ERROR(msg) \
    do { \
        std::cerr << "Error: " << msg << "\n"; \
        return -1; \
    } while (0)

static_assert(std::numeric_limits<unsigned>::digits == 32, "int is not 32-bit");

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

    template <typename T>
    constexpr vec3(std::array<T, 3> a)
    {
        v[0] = a[0];
        v[1] = a[1];
        v[2] = a[2];
    }

    constexpr vec3(float val) : 
        vec3(val, val, val) 
    {}

    float& x() { return v[0]; }
    float& y() { return v[1]; }
    float& z() { return v[2]; }

    float x() const { return v[0]; }
    float y() const { return v[1]; }
    float z() const { return v[2]; }

    float& operator[](int pos) { return v[pos]; }
    const float& operator[](int pos) const { return v[pos]; }

    float dot(const vec3& rhs) const 
    {
        return x() * rhs.x() + y() * rhs.y() + z() * rhs.z();
    }

    vec3 cross(const vec3& rhs) const
    {
        return {
            y() * rhs.z() - z() * rhs.y(),
            z() * rhs.x() - x() * rhs.z(),
            x() * rhs.y() - y() * rhs.x() };
    }

    float norm() const 
    {
        return std::sqrt(x() * x() + y() * y() + z() * z());
    }

    void normalize()
    {
        float n = norm();
        x() /= n; y() /= n; z() /= n;
    }

    vec3 normalized() const
    {
        float n = norm();
        return { x() / n, y() / n, z() / n };
    }

    vec3 cwiseMin(const vec3& rhs) const
    {
        return {
            std::min(v[0], rhs.v[0]),
            std::min(v[1], rhs.v[1]),
            std::min(v[2], rhs.v[2]) };
    }

    vec3 cwiseMax(const vec3& rhs) const
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
    float v[3];
};

inline vec3 operator+(const vec3& lhs, const vec3& rhs) noexcept
{
    return { lhs[0] + rhs[0], lhs[1] + rhs[1], lhs[2] + rhs[2]};
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
    // ambient, diffuse, specular, reflection coefficients (rgb)
    // range: [0, 1]
    vec3 ka, kd, ks, km;
    // specular exponent (aka shininess)
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
        m.ns = 250;
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
struct uv { float u, v; };

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
    float focal_len; // focal length (distance to img plane)
    float width, height; // projected img size (in world space)

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

// Axis-aligned bounding box
struct bbox
{
    vec3 cmin; // Min corner
    vec3 cmax; // Max corner

    bbox() :
        cmin(std::numeric_limits<float>::infinity()),
        cmax(-std::numeric_limits<float>::infinity())
    {}

    vec3 center() const { return 0.5 * (cmin + cmax); }

    static constexpr uint nserial = 2 * vec3::nserial;

    void serialize(uint* p) const
    {
        cmin.serialize(p);
        cmax.serialize(p + vec3::nserial);
    }
};

struct tri
{
    std::array<int, 3> Vidx; // vertex indices
    std::array<int, 3> NVidx; // normal indices
#if ENABLE_TEXTURES
    std::array<int, 3> UVidx; // texcooord indices
#endif
    int matid; // material index
    bbox bb; // bounding box. not serialized
};

struct Scene
{
    // max_bv must be a power of 2.
    Scene(const fs::path& scene_path, const uint max_bv);

    camera C; // camera
    std::vector<light> L; // lights
    std::pair<uint, uint> R; // resolution
    
    std::vector<vec3> V; // vertices 
    std::vector<vec3> NV; // normals
#if ENABLE_TEXTURES
    std::vector<uv> UV;// Texture coords
#endif
    std::vector<mat> M; // materials

    std::vector<tri> F; // triangles

    // bounding volumes and num tris held by each
    std::vector<std::pair<bbox, uint>> BV;

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

    int read_scene(const fs::path& scenepath, std::vector<fs::path>& out_objpaths);
    int read_objs(const std::vector<fs::path>& objpaths);

    int init_bvs(const uint max_bv);
    void gather_bvs(tri* tris_beg, tri* tris_end, uint depth = 0);

private:
    uint bv_stop_depth;
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


#endif