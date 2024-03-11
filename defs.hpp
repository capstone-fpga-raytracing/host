
#ifndef HOST_DEFS_HPP
#define HOST_DEFS_HPP

#include <cstring>
#include <cassert>
#include <cmath>
#include <limits>
#include <vector>
#include <array>

#include "utils.hpp"

#define ENABLE_TEXTURES 0

constexpr bool textures_enabled()
{
#if ENABLE_TEXTURES
    return true;
#else
    return false;
#endif
}

// 3d vector with all the C++
// bells and whistles.
struct vec3
{
    vec3() = default;

    constexpr vec3(float x, float y, float z) 
        noexcept
    {
        v[0] = x;
        v[1] = y;
        v[2] = z;
    }

    template <typename T>
    constexpr vec3(std::array<T, 3> a) 
        noexcept
    {
        v[0] = a[0];
        v[1] = a[1];
        v[2] = a[2];
    }

    float& x() noexcept { return v[0]; }
    float& y() noexcept { return v[1]; }
    float& z() noexcept { return v[2]; }

    float x() const noexcept { return v[0]; }
    float y() const noexcept { return v[1]; }
    float z() const noexcept { return v[2]; }

    float& operator[](int pos) noexcept { return v[pos]; }
    const float& operator[](int pos) const noexcept { return v[pos]; }

    float dot(const vec3& rhs) const noexcept
    {
        return x() * rhs.x() + y() * rhs.y() + z() * rhs.z();
    }

    vec3 cross(const vec3& rhs) const noexcept
    {
        return {
            y() * rhs.z() - z() * rhs.y(),
            z() * rhs.x() - x() * rhs.z(),
            x() * rhs.y() - y() * rhs.x() };
    }

    float norm() const noexcept
    {
        return std::sqrt(x() * x() + y() * y() + z() * z());
    }

    void normalize() noexcept
    {
        float n = norm();
        x() /= n; y() /= n; z() /= n;
    }

    vec3 normalized() const noexcept
    {
        float n = norm();
        return { x() / n, y() / n, z() / n };
    }

    vec3 cwiseMin(const vec3& rhs) const noexcept
    {
        return {
            std::min(v[0], rhs.v[0]),
            std::min(v[1], rhs.v[1]),
            std::min(v[2], rhs.v[2]) };
    }

    vec3 cwiseMax(const vec3& rhs) const noexcept
    {
        return {
            std::max(v[0], rhs.v[0]),
            std::max(v[1], rhs.v[1]),
            std::max(v[2], rhs.v[2]) };
    }

    int maxDim() const noexcept { return int(std::max_element(v, v + 3) - v); }

    vec3& operator+=(const vec3& rhs) noexcept
    {
        x() += rhs.x(); y() += rhs.y(); z() += rhs.z();
        return *this;
    }
    vec3& operator-=(const vec3& rhs) noexcept
    {
        x() -= rhs.x(); y() -= rhs.y(); z() -= rhs.z();
        return *this;
    }

    static constexpr vec3 infinity() noexcept {
        return {
            std::numeric_limits<float>::infinity(),
            std::numeric_limits<float>::infinity(),
            std::numeric_limits<float>::infinity()
        };
    }

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

inline vec3 operator+(const vec3& lhs, const vec3& rhs) noexcept { 
    return { lhs[0] + rhs[0], lhs[1] + rhs[1], lhs[2] + rhs[2] };
}
inline vec3 operator-(const vec3& lhs, const vec3& rhs) noexcept {
    return { lhs[0] - rhs[0], lhs[1] - rhs[1], lhs[2] - rhs[2] };
}
inline vec3 operator*(float lhs, const vec3& rhs) noexcept {
    return { lhs * rhs[0], lhs * rhs[1], lhs * rhs[2] };
}
inline vec3 operator*(const vec3& lhs, float rhs) noexcept {
    return operator*(rhs, lhs);
}
inline vec3 operator/(const vec3& lhs, float rhs) noexcept {
    return { lhs[0] / rhs, lhs[1] / rhs, lhs[2] / rhs };
}

inline std::ostream& operator<<(std::ostream& os, const vec3& vec) {
    os << "(" << vec.x() << ", " << vec.y() << ", " << vec.z() << ")";
    return os;
}

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
struct uv 
{
    float u, v; 
    static constexpr uint nserial = 2;
};

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
    vec3 u, v, w; // rotation axes (columns of rotation matrix)
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
        cmin(vec3::infinity()),
        cmax(-1 * vec3::infinity())
    {}

    vec3 center() const { return 0.5 * (cmin + cmax); }

    static constexpr uint nserial = 2 * vec3::nserial;

    void serialize(uint* p) const
    {
        cmin.serialize(p);
        cmax.serialize(p + vec3::nserial);
    }
};

struct bv
{
    bbox bb;
    uint ntris;
    static constexpr uint nserial = bbox::nserial + 1;

    void serialize(uint* p) const
    {
        bb.serialize(p);
        p[bbox::nserial] = ntris;
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
    bbox bb; // not serialized

    static constexpr uint nserial = textures_enabled() ? 10 : 7;
};

enum class serial_format
{
    // duplicate vertices, normals, and UVs instead of using indices.
    // easier + quicker to read on FPGA, but increases
    // memory usage significantly
    Duplicate,
    // keep the indices and don't duplicate.
    NoDuplicate
};

struct Scene
{
    // max_bv must be a power of 2.
    Scene(const fs::path& scene_path, const uint max_bv, 
        serial_format ser_fmt = serial_format::Duplicate, 
        bool verbose = false);

    camera C; // camera
    std::pair<uint, uint> R; // resolution
    std::vector<light> L; // lights
    
    std::vector<vec3> V; // vertices 
    std::vector<vec3> NV; // normals
#if ENABLE_TEXTURES
    std::vector<uv> UV;// Texture coords
#endif
    std::vector<mat> M; // materials

    std::vector<tri> F; // triangles
    std::vector<bv> BV; // bounding volumes

    const std::string& name() const { return m_scname; }

    bool ok() const { return m_ok; }
    operator bool() const { return ok(); }

    // magic number used for serialization endianness check
    static constexpr uint MAGIC = 0x5343454E;

    uint nserial() const;
    void serialize(uint* buf) const;

private:
    int read_scenefile(const fs::path& scenepath, std::vector<fs::path>& out_objpaths);
    int read_objs(const std::vector<fs::path>& objpaths);

    int init_bvs(const uint max_bv);
    void gather_bvs(tri* tris_beg, tri* tris_end, uint depth = 0);

private:
    std::string m_scname;
    serial_format m_serfmt;
    bool m_verbose;
    uint m_bv_stop_depth;
    bool m_ok;
};

template <typename T>
concept has_nserial = std::is_same_v<
    std::remove_cvref_t<decltype(T::nserial)>, uint>; // scary

template <typename T>
concept serializable = has_nserial<T> && requires(T t) {
    t.serialize(std::declval<uint*>());
};

// Get size of vector when serialized. 
template <typename T>
inline uint vnserial(const std::vector<T>& vec)
{
    static_assert(has_nserial<T>, "type does not have nserial");
    return uint(vec.size()) * T::nserial;
}

// Serialize vector.
template <typename T>
inline uint* vserialize(const std::vector<T>& v, uint* p)
{
    static_assert(serializable<T>, "type is not serializable");
    for (size_t i = 0; i < v.size(); ++i)
    {
        v[i].serialize(p);
        p += T::nserial;
    }
    return p;
}

#endif