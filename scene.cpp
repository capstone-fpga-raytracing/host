
#include <cassert>
#include <string>
#include <string_view>
#include <iostream>
#include <charconv>
#include <cmath>

#include "rapidobj/rapidobj.hpp"
#include "defs.hpp"

// missing in Windows
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static inline bbox get_tri_bbox(
    const std::vector<vec3>& V, const std::array<int, 3>& tri)
{
    bbox bb;
    for (int i = 0; i < 3; ++i)
    {
        bb.cmin = bb.cmin.cwiseMin(V[tri[i]]);
        bb.cmax = bb.cmax.cwiseMax(V[tri[i]]);
    }
    return bb;
}

static inline bbox get_nodes_bbox(
    const tri* tris_beg, const tri* tris_end)
{
    bbox bb;
    for (auto* p = tris_beg; p < tris_end; ++p)
    {
        bb.cmin = bb.cmin.cwiseMin(p->bb.cmin);
        bb.cmax = bb.cmax.cwiseMax(p->bb.cmax);
    }
    return bb;
}

// https://www.euclideanspace.com/maths/geometry/rotations/conversions/angleToMatrix/
static void axis_angle_to_uvw(vec3 axis, float angle, vec3& u, vec3& v, vec3& w)
{
    axis.normalize();

    angle = M_PI * angle / 180.f;
    float c = std::cos(angle);
    float s = std::sin(angle);

    vec3 vt = (1.f - c) * axis;
    vec3 vs = s * axis;

    float txx = vt.x() * axis.x();
    float txy = vt.x() * axis.y();
    float txz = vt.x() * axis.z();
    float tyy = vt.y() * axis.y();
    float tyz = vt.y() * axis.z();
    float tzz = vt.z() * axis.z();

    u = { txx + c, txy + vs.z(), txz - vs.y() };
    v = { txy - vs.z(), tyy + c, tyz + vs.x() };
    w = { txz + vs.y(), tyz - vs.x(), tzz + c };
}

template <typename T>
static bool parsenum(std::string_view& str, T& val)
{
    const char* beg = str.data();
    const char* end = str.data() + str.length();
    // skip whitespace
    while (beg != end && is_ws(*beg)) { beg++; }

    auto res = std::from_chars(beg, end, val);
    str = { res.ptr, end };
    return res.ec == std::errc();
}

template <typename T>
static inline bool parsenum3(std::string_view& str, T& a, T& b, T& c)
{
    return parsenum(str, a) && parsenum(str, b) && parsenum(str, c);
}

// stops when section ends (i.e. line is empty)
static bool sc_getsubline(std::string_view& str, std::string_view& line, int& lineno)
{
    bool gotline = sv_getline(str, line);
    if (gotline) {
        lineno++;
        gotline = !line.empty();
    }
    return gotline;
}

int Scene::read_scenefile(const fs::path& scpath, std::vector<fs::path>& objpaths)
{
    const char* pscname = m_scname.c_str();

#define scERROR(msg) mERROR("%s:%d: %s", pscname, lineno, msg)

    // okay, we must parse the entire file
    BufWithSize<char> scbuf;  
    int e = read_file(scpath, scbuf);
    if (e) { return e; }

    int lineno = 0;
    std::string_view line;
    std::string_view scstr(scbuf.get(), scbuf.size);
    auto scdir = scpath.parent_path();   

    bool has_scene = false, 
        has_cam = false;

    while (sv_getline(scstr, line))
    {
        lineno++;
        if (line.empty()) { continue; }

        if (line == "obj")
        {
            while (sc_getsubline(scstr, line, lineno)) {
                objpaths.push_back(scdir / line);
            }
        }
        else if (line == "scene")
        {
            bool has_res = false;
            while (sc_getsubline(scstr, line, lineno))
            {
                if (line.starts_with("res "))
                {
                    line.remove_prefix(sizeof("res ") - 1);
                    if (!parsenum(line, R.first) ||
                        !parsenum(line, R.second)) {
                        return scERROR("invalid resolution");
                    }
                    has_res = true;
                }
                else { return scERROR("unrecognized prop"); }
            }
            if (!has_res) {
                return scERROR("missing render prop(s)");
            }
            has_scene = true;
        }
        else if (line == "camera")
        {
            bool has_eye = false, 
                has_uvw = false, 
                has_flen = false, 
                has_proj = false;

            while (sc_getsubline(scstr, line, lineno))
            {
                if (line.starts_with("eye "))
                {
                    line.remove_prefix(sizeof("eye ") - 1);
                    if (!parsenum3(line, C.eye.x(), C.eye.y(), C.eye.z())) {
                        return scERROR("invalid eye");
                    }
                    has_eye = true;
                }
                else if (line.starts_with("axis_angle "))
                {
                    line.remove_prefix(sizeof("axis_angle ") - 1);
                    vec3 axis; float angle;
                    if (!parsenum3(line, axis.x(), axis.y(), axis.z()) ||
                        !parsenum(line, angle)) {
                        return scERROR("invalid axis angle");
                    }
                    axis_angle_to_uvw(axis, angle, C.u, C.v, C.w);
                    has_uvw = true;
                }
                else if (line.starts_with("uvw "))
                {
                    line.remove_prefix(sizeof("uvw ") - 1);
                    vec3& u = C.u; vec3& v = C.v; vec3& w = C.w;
                    if (!parsenum3(line, u.x(), u.y(), u.z()) ||
                        !parsenum3(line, v.x(), v.y(), v.z()) ||
                        !parsenum3(line, w.x(), w.y(), w.z())) {
                        return scERROR("invalid uvw");
                    }
                    has_uvw = true;
                }
                else if (line.starts_with("focal_len "))
                {
                    line.remove_prefix(sizeof("focal_len ") - 1);
                    if (!parsenum(line, C.focal_len) || C.focal_len <= 0) {
                        return scERROR("invalid focal length");
                    }
                    has_flen = true;
                }
                else if (line.starts_with("proj_size "))
                {
                    line.remove_prefix(sizeof("proj_size ") - 1);
                    if (!parsenum(line, C.width) ||
                        !parsenum(line, C.height) ||
                        C.width <= 0 || C.height <= 0) {
                        return scERROR("invalid projection size");
                    }
                    has_proj = true;
                }
                else { return scERROR("unrecognized prop"); }
            }
            if (!has_eye || !has_uvw || !has_flen || !has_proj) {
                return scERROR("missing camera prop(s)");
            }
            has_cam = true;
        }
        else if (line == "light")
        {
            light lt;
            bool has_pos = false, 
                has_rgb = false;

            while (sc_getsubline(scstr, line, lineno))
            {
                if (line.starts_with("pos "))
                {
                    line.remove_prefix(sizeof("pos ") - 1);
                    if (!parsenum3(line, lt.pos.x(), lt.pos.y(), lt.pos.z())) {
                        return scERROR("invalid position");
                    }
                    has_pos = true;
                }
                else if (line.starts_with("rgb "))
                {
                    line.remove_prefix(sizeof("rgb ") - 1);         
                    if (!parsenum3(line, lt.rgb.x(), lt.rgb.y(), lt.rgb.z()) ||
                        lt.rgb.x() < 0 || lt.rgb.y() < 0 || lt.rgb.z() < 0 ||
                        lt.rgb.x() > 1 || lt.rgb.y() > 1 || lt.rgb.z() > 1) {
                        return scERROR("invalid color, must be in [0,1]");
                    }
                    has_rgb = true;
                }
                else { return scERROR("unrecognized prop"); }
            }
            if (!has_pos || !has_rgb) {
                return scERROR("missing light prop(s)");
            }

            L.push_back(lt);
        }
        else { return scERROR("unrecognized prop"); }
    }

    if (objpaths.empty()) {
        return mERROR("%s: no obj files found\n", pscname);
    } else if (!has_cam) {
        return mERROR("%s: no camera found\n", pscname);
    } else if (L.empty()) {
        return mERROR("%s: no lights found\n", pscname);
    } else if (!has_scene) {
        return mERROR("%s: no resolution found\n", pscname);
    }

    if (m_verbose) {
        std::printf("%s: using resolution %ux%u\n", pscname, R.first, R.second);
        std::printf("%s: found %zu obj file(s)\n", pscname, objpaths.size());
    }
    std::printf("%s: found %zu light(s)\n", pscname, L.size());
    return 0;

#undef scERROR
}

int Scene::read_objs(const std::vector<fs::path>& objpaths)
{
    const char* pscname = m_scname.c_str();

    bool missing_mat = false;
#if ENABLE_TEXTURES
    bool missing_uv = false;
#endif
    std::vector<int> badFidx; // faces that need fixing later

    for (const auto& objpath : objpaths)
    {
        auto objname = objpath.filename();
        DECL_UTF8PATH_CSTR(objname)

        rapidobj::Result res = rapidobj::ParseFile(objpath);
        if (res.error || !rapidobj::Triangulate(res))
        {
            return mERROR("%s:%d: %s", pobjname, 
                int(res.error.line_num), res.error.code.message().c_str());
        }

        int baseVidx = int(V.size());
        auto& objverts = res.attributes.positions;
        for (size_t i = 0; i < objverts.size(); i += 3) {
            V.push_back({ objverts[i], objverts[i + 1], objverts[i + 2] });
        }

        int baseNVidx = int(NV.size());
        auto& objnormals = res.attributes.normals;
        for (size_t i = 0; i < objnormals.size(); i += 3) {
            NV.push_back({ objnormals[i], objnormals[i + 1], objnormals[i + 2] });
        }
#if ENABLE_TEXTURES
        int baseUVidx = int(UV.size());
        auto& objUV = res.attributes.texcoords;
        for (size_t i = 0; i < objUV.size(); i += 2) {
            UV.push_back({ objUV[i], objUV[i + 1] });
        }
#endif
        int baseMid = int(M.size());
        auto& objmats = res.materials;
        for (size_t i = 0; i < objmats.size(); ++i)
        {
            auto& mobj = objmats[i];
            mat m;
            m.ka = mobj.ambient;
            m.kd = mobj.diffuse;
            m.ks = mobj.specular;
            m.ns = mobj.shininess;

            // solve 1000-2000r+1000r^{2} = ns to get roughness (blender's formula).
            // then approximate refl as 1-roughness (stupid but should work).
            // this simplifies to sqrt(ns/1000)
            assert(m.ns >= 0);
            float ref_ns = m.ns > 1000 ? 1 : m.ns / 1000;
            float refl = std::sqrt(ref_ns);
            m.km = { refl, refl, refl };

            M.push_back(m);
        }

        for (const auto& shape : res.shapes)
        {
            if (shape.lines.indices.size() != 0 ||
                shape.points.indices.size() != 0) {
                return mERROR("%s: polylines/points not supported", pobjname);
            }

            auto& meshidx = shape.mesh.indices;
            auto& matids = shape.mesh.material_ids;
            assert(meshidx.size() / 3 == matids.size());

            for (size_t i = 0; i < meshidx.size(); i += 3)
            {
                tri t;
                bool bad = false;

                t.Vidx = {
                    baseVidx + meshidx[i].position_index,
                    baseVidx + meshidx[i + 1].position_index,
                    baseVidx + meshidx[i + 2].position_index };

                if (meshidx[i].normal_index != -1 &&
                    meshidx[i + 1].normal_index != -1 &&
                    meshidx[i + 2].normal_index != -1) [[likely]] 
                {
                    t.NVidx = {
                        baseNVidx + meshidx[i].normal_index,
                        baseNVidx + meshidx[i + 1].normal_index,
                        baseNVidx + meshidx[i + 2].normal_index
                    };
                }
                else { t.NVidx[0] = -1; bad = true; }

#if ENABLE_TEXTURES
                if (meshidx[i].texcoord_index != -1 &&
                    meshidx[i + 1].texcoord_index != -1 &&
                    meshidx[i + 2].texcoord_index != -1) [[likely]]
                {
                    t.UVidx = {
                        baseUVidx + meshidx[i].texcoord_index,
                        baseUVidx + meshidx[i + 1].texcoord_index,
                        baseUVidx + meshidx[i + 2].texcoord_index
                    };
                }
                else { 
                    t.UVidx[0] = -1; 
                    bad = true; 
                    missing_uv = true; 
                }
#endif
                if (matids[i / 3] != -1) [[likely]] {
                    t.matid = baseMid + matids[i / 3];
                }
                else { 
                    t.matid = -1;
                    bad = true; 
                    missing_mat = true; 
                }

                t.bb = get_tri_bbox(V, t.Vidx);

                F.push_back(std::move(t));

                // It is highly likely that if one face is bad, all are bad.
                // Put them all in one array to avoid iterating through all faces
                // multiple times.
                if (bad) [[unlikely]] {
                    badFidx.push_back(int(F.size() - 1));
                }
            }
        }
    }

    std::printf("%s: found %zu triangle(s), %zu vertices, %zu normal(s)\n",
        pscname, F.size(), V.size(), NV.size());
#if ENABLE_TEXTURES
    std::printf("%s: found %zu UV(s), %zu material(s)\n",
        pscname, UV.size(), M.size());
#else
    std::printf("%s: found %zu material(s)\n", pscname, M.size());
#endif

    // --------------- Fix bad faces ---------------  
    if (badFidx.size() != 0)
    {
        if (m_verbose) {
            std::printf("%s: detected %zu faces "
                "with missing information\n", pscname, badFidx.size());
        }

        int default_matid = -1;
        if (missing_mat) {
            M.push_back(mat::default_mat());
            default_matid = int(M.size() - 1);
        }
#if ENABLE_TEXTURES
        int default_uvid = -1;
        if (missing_uv) {
            UV.push_back({ 0 });
            default_uvid = int(UV.size() - 1);
        }
#endif
        size_t nmissingmat = 0;
#if ENABLE_TEXTURES
        size_t nmissinguv = 0;
#endif
        size_t oldNVsize = NV.size();
        for (size_t i = 0; i < badFidx.size(); ++i)
        {
            auto& t = F[badFidx[i]];

            // missing material
            if (t.matid < 0)
            {
                assert(default_matid >= 0);
                t.matid = default_matid;
                nmissingmat++;
            }
#if ENABLE_TEXTURES
            // missing UV
            if (t.UVidx[0] < 0)
            {
                assert(default_uvid >= 0);
                t.UVidx = { default_uvid,
                    default_uvid, default_uvid };
                nmissinguv++;
            }
#endif
            // missing normals
            if (t.NVidx[0] < 0)
            {
                // do the simple thing (flat shading).
                // ideally this would use smoothing groups or do some
                // kind of automatic smoothing
                vec3 e0 = V[t.Vidx[1]] - V[t.Vidx[0]];
                vec3 e1 = V[t.Vidx[2]] - V[t.Vidx[0]];
                vec3 nv = e0.cross(e1).normalized();
                NV.push_back(nv);

                int nvid = int(NV.size() - 1);
                t.NVidx = { nvid, nvid, nvid };
            }
        }

        if (m_verbose) {
            if (oldNVsize != NV.size()) {
                std::printf("%s: fixed %zu missing normal IDs\n", 
                    pscname, NV.size() - oldNVsize);
            }
            if (nmissingmat != 0) {
                std::printf("%s: fixed %zu missing material IDs\n", 
                    pscname, nmissingmat);
            }
#if ENABLE_TEXTURES
            if (nmissinguv != 0) {
                std::printf("%s: fixed %zu missing UV IDs\n", 
                    pscname, nmissinguv);
            }
#endif
        }
    }
    if (F.empty() || V.empty()) {
        return mERROR("%s: no faces or vertices found\n", pscname);
    }
    return 0;
}


// Gather bboxes at stop_depth, and sort triangles in order of bboxes.
void Scene::gather_bvs(tri* tris_beg, tri* tris_end, uint depth)
{
    bbox bb = get_nodes_bbox(tris_beg, tris_end);

    const int max_dim = (bb.cmax - bb.cmin).maxDim();
    // sort along longest dimension
    std::sort(tris_beg, tris_end, [=](const auto& lhs, const auto& rhs) {
        return lhs.bb.center()[max_dim] < rhs.bb.center()[max_dim]; });

    auto ntris = tris_end - tris_beg;
    if (depth != m_bv_stop_depth)
    {
        auto lhs_size = ntris / 2;
        assert(lhs_size != 0 && "should not be possible");

        gather_bvs(tris_beg, tris_beg + lhs_size, depth + 1);
        gather_bvs(tris_beg + lhs_size, tris_end, depth + 1);
    }
    else { BV.emplace_back(std::move(bb), uint(ntris)); }
}

int Scene::init_bvs(const uint max_bv)
{
    if (!is_powof2(max_bv)) {
        return mERROR("max-bv is not a power of 2");
    }

    m_bv_stop_depth = ulog2(max_bv);
    uint last_full_depth = uint(ulog2(F.size()));

    // limit to one before last so that bvs are never empty
    if (m_bv_stop_depth >= last_full_depth && m_bv_stop_depth != 0) {
        m_bv_stop_depth = last_full_depth - 1;
    }

    gather_bvs(F.data(), F.data() + F.size());

    if (m_verbose) {
        std::printf("%s: collected %zu BV(s) at depth %u\n",
            m_scname.c_str(), BV.size(), m_bv_stop_depth);
    }
    return 0;
}

Scene::Scene(const fs::path& scpath, const uint max_bv, serial_format ser_fmt, bool verbose) :
    C{}, R(0, 0), m_scname(scpath.filename().string()), 
    m_serfmt(ser_fmt), m_verbose(verbose), m_ok(false)
{
    std::vector<fs::path> objpaths;
    m_ok = 
        read_scenefile(scpath, objpaths) == 0 &&
        read_objs(objpaths) == 0 &&
        init_bvs(max_bv) == 0;
}

// magic, resX, resY, numL, numBV, camOff, BVoff, 
// Voff, NVoff, Foff, NFoff, MFoff, Moff, Loff, optional: UVoff, UFoff
static constexpr int nhdr_noduplicate = 14 + 2 * textures_enabled();

// magic, resX, resY, numL, numBV, camOff, BVoff, 
// FVoff, FNVoff, FMoff, Loff, optional: FUVoff
static constexpr int nhdr_duplicate = 11 + textures_enabled();

uint Scene::nserial() const
{
    uint ret = 0;
    switch (m_serfmt)
    {
    case serial_format::NoDuplicate:
    
        ret = nhdr_noduplicate + camera::nserial +
            vnserial(BV) + vnserial(V) + vnserial(NV) +
            vnserial(F) + vnserial(M) + vnserial(L);
#if ENABLE_TEXTURES
        ret += vnserial(UV);
#endif
        break;
    
    case serial_format::Duplicate:
        ret = nhdr_duplicate + camera::nserial +
            vnserial(BV) +
            (uint(F.size()) * (6 * vec3::nserial + mat::nserial)) +
            vnserial(L);
#if ENABLE_TEXTURES
        ret += (uint(F.size()) * 3 * uv::nserial);
#endif
        break;
    }
    return ret;
}

void Scene::serialize(uint* p) const
{
    if (m_verbose) {
        std::printf("%s: serialization format is %s\n", m_scname.c_str(), 
            m_serfmt == serial_format::Duplicate ? "duplicate" : "no duplicate");
    }

    *p++ = MAGIC;
    *p++ = R.first;
    *p++ = R.second;
    *p++ = uint(L.size());
    *p++ = uint(BV.size());
     
    switch (m_serfmt)
    {
    case serial_format::NoDuplicate:
    {
        uint off = nhdr_noduplicate;
        *p++ = off; off += camera::nserial;
        *p++ = off; off += vnserial(BV);
        *p++ = off; off += vnserial(V);
        *p++ = off; off += vnserial(NV);
        *p++ = off; off += (uint(F.size()) * 3);
        *p++ = off; off += (uint(F.size()) * 3);
        *p++ = off; off += uint(F.size());
        *p++ = off; off += vnserial(M);      
        *p++ = off; off += vnserial(L);
#if ENABLE_TEXTURES
        *p++ = off; off += vnserial(UV);
        *p++ = off; off += (uint(F.size()) * 3);
#endif
        C.serialize(p);
        p += camera::nserial;

        p = vserialize(BV, p);
        p = vserialize(V, p);
        p = vserialize(NV, p);

        for (size_t i = 0; i < F.size(); ++i) {
            ranges::copy(F[i].Vidx, p); p += 3;
        }
        for (size_t i = 0; i < F.size(); ++i) {
            ranges::copy(F[i].NVidx, p); p += 3;
        }
        for (size_t i = 0; i < F.size(); ++i) {
            *p++ = F[i].matid;
        }

        p = vserialize(M, p);
        p = vserialize(L, p);
#if ENABLE_TEXTURES
        p = vserialize(UV, p);
        for (size_t i = 0; i < F.size(); ++i) {
            ranges::copy(F[i].UVidx, p); p += 3;
        }
#endif
        break;
    }

    case serial_format::Duplicate:
        uint off = nhdr_duplicate;
        *p++ = off; off += camera::nserial;
        *p++ = off; off += vnserial(BV);
        *p++ = off; off += (uint(F.size()) * 3 * vec3::nserial);
        *p++ = off; off += (uint(F.size()) * 3 * vec3::nserial);
        *p++ = off; off += (uint(F.size()) * mat::nserial);
        *p++ = off; off += vnserial(L);
#if ENABLE_TEXTURES
        *p++ = off; off += (uint(F.size()) * 3 * uv::nserial);
#endif
        C.serialize(p);
        p += camera::nserial;

        p = vserialize(BV, p);

        for (size_t i = 0; i < F.size(); ++i) {
            for (int j = 0; j < 3; ++j) {
                V[F[i].Vidx[j]].serialize(p); 
                p += vec3::nserial;
            }
        }
        for (size_t i = 0; i < F.size(); ++i) {
            for (int j = 0; j < 3; ++j) {
                NV[F[i].NVidx[j]].serialize(p);
                p += vec3::nserial;
            }
        }
        for (size_t i = 0; i < F.size(); ++i) {
            M[F[i].matid].serialize(p);
            p += mat::nserial;
        }

        p = vserialize(L, p);

#if ENABLE_TEXTURES
        for (size_t i = 0; i < F.size(); ++i) {
            for (int j = 0; j < 3; ++j) {
                UV[F[i].UVidx[j]].serialize(p);
                p += uv::nserial;
            }
        }
#endif
        break;
    }
}

// not used. needs to be updated for scene, obj and mtl files
#if 0
bool write_model(const char* obj_file, const char* mtl_file, Scene& m)
{
    if (m.UF.size() != m.F.size() ||
        m.NF.size() != m.F.size())
    {
        std::cerr << "error: invalid geometry\n";
        return false;
    }

    // binary opt allows comparison b/w linux and windows builds
    std::ofstream file(obj_file, std::ios::binary);
    if (!file)
        return false;

    for (int i = 0; i < int(m.V.size()); ++i)
        file << "v " << m.V[i].x() << " " << m.V[i].y() << " " << m.V[i].z() << "\n";

    for (int i = 0; i < int(m.UV.size()); ++i)
        file << "vt " << m.UV[i].u << " " << m.UV[i].v << "\n";

    for (int i = 0; i < int(m.NV.size()); ++i)
        file << "vn " << m.NV[i].x() << " " << m.NV[i].y() << " " << m.NV[i].z() << "\n";

    // format: vertex/texture/normal (1-indexed)
    for (int irow = 0; irow < int(m.F.size()); ++irow)
    {
        file << "f";
        for (int icol = 0; icol < 3; ++icol)
        {
            file << " " << m.F[irow][icol] + 1;

            bool has_uf = m.UF[irow][icol] != -1;
            bool has_nf = m.NF[irow][icol] != -1;

            if (has_uf || has_nf) file << "/";
            if (has_uf) file << m.UF[irow][icol] + 1;
            if (has_nf) file << "/" << m.NF[irow][icol] + 1;
        }
        file << "\n";
    }

    file.close();
    return true;
}
#endif

// old test scene
// These numbers mostly from blender
//scene.C.eye = { 8.4585f, -2.5662f, 10.108f };
//scene.C.eye = { 7.0827, -3.4167, 7.4254 };
//scene.C.focal_len = 5;
//scene.C.width = 3.6;
//scene.C.height = scene.C.width * (240. / 320.); // 320x240 render  
//scene.C.u = { 1, 1, 0 };
//scene.C.v = { -1, 1, std::sqrt(2) };
//scene.C.w = { 1, -1, std::sqrt(2) };
//
//light l1, l2;
////l1.pos = { 0.9502, 1.953, 4.1162 };
////l2.pos = { -2.24469, 1.953, 4.1162 };  
//l1.pos = { 3.6746, 2.0055, 3.1325 };
//l2.pos = { 1.5699, 0.87056, 3.1325 };
//
////l1.rgb = { 1, 1, 0 }; // yellow
////l2.rgb = { 1, 1, 0 }; // yellow
//l1.rgb = { 1, 1, 1 }; // white
//l2.rgb = { 1, 1, 1 }; // white
//
//scene.L.push_back(l1);
//scene.L.push_back(l2);