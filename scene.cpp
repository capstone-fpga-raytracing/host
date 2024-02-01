
#include <cassert>
#include <string>
#include <string_view>
#include <iostream>
#include <charconv>

#include "defs.hpp"


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

#define SCERROR(lineno, msg) \
    do { \
        std::cerr << "line " << lineno << ": " << msg << "\n"; \
        return -1; \
    } while (0)


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

int Scene::read_scene(const fs::path& scpath, std::vector<fs::path>& objpaths)
{
    BufWithSize<char> scbuf;
    {
        scopedFILE scfile = SAFE_FOPEN(scpath.c_str(), "rb");
        if (!scfile) {
            ERROR("could not open scene file");
        }   
        scbuf.size = fs::file_size(scpath);
        scbuf.ptr = std::make_unique<char[]>(scbuf.size);

        // read the whole file. this is okay as a .scene file is always small
        // and we always need to parse all of it
        if (std::fread(scbuf.get(), 1, scbuf.size, scfile.get()) != scbuf.size) {
            ERROR("could not read scene file");
        }
    }

    auto scdir = scpath.parent_path();

    int lineno = 0;
    size_t scpos = 0;
    std::string_view line;
    std::string_view scstr(scbuf.get(), scbuf.size);

    while (sv_getline(scstr, scpos, line))
    {
        lineno++;
        if (line.empty()) { continue; }

        if (line == "obj")
        {
            while (sv_getline(scstr, scpos, line))
            {
                lineno++;
                if (line.empty()) { break; }
                objpaths.push_back(scdir / line);
            }
        }
        else if (line == "render")
        {
            bool has_res = false;
            while (sv_getline(scstr, scpos, line))
            {
                lineno++;
                if (line.empty()) { break; }

                if (line.starts_with("res "))
                {
                    line.remove_prefix(sizeof("res ") - 1);
                    if (!parsenum(line, R.first) ||
                        !parsenum(line, R.second)) {
                        SCERROR(lineno, "invalid resolution");
                    }
                    has_res = true;
                }
                else { SCERROR(lineno, "unrecognized prop"); }
            }
            if (!has_res) {
                SCERROR(lineno, "missing render prop(s)");
            }
        }
        else if (line == "camera")
        {
            bool has_eye = false, 
                has_uvw = false, 
                has_focus = false, 
                has_proj_size = false;

            while (sv_getline(scstr, scpos, line))
            {
                lineno++;
                if (line.empty()) { break; }

                if (line.starts_with("eye "))
                {
                    line.remove_prefix(sizeof("eye ") - 1);
                    if (!parsenum3(line, C.eye.x(), C.eye.y(), C.eye.z())) {
                        SCERROR(lineno, "invalid eye");
                    }
                    has_eye = true;
                }
                else if (line.starts_with("uvw "))
                {
                    line.remove_prefix(sizeof("uvw ") - 1);

                    vec3& u = C.u; vec3& v = C.v; vec3& w = C.w;
                    if (!parsenum3(line, u.x(), u.y(), u.z()) ||
                        !parsenum3(line, v.x(), v.y(), v.z()) ||
                        !parsenum3(line, w.x(), w.y(), w.z())) {
                        SCERROR(lineno, "invalid uvw");
                    }
                    has_uvw = true;
                }
                else if (line.starts_with("focal_len "))
                {
                    line.remove_prefix(sizeof("focal_len ") - 1);
                    if (!parsenum(line, C.focal_len) || C.focal_len <= 0) {
                        SCERROR(lineno, "invalid focal length");
                    }
                    has_focus = true;
                }
                else if (line.starts_with("proj_size "))
                {
                    line.remove_prefix(sizeof("proj_size ") - 1);

                    if (!parsenum(line, C.width) ||
                        !parsenum(line, C.height) ||
                        C.width <= 0 || C.height <= 0) {
                        SCERROR(lineno, "invalid projection size");
                    }
                    has_proj_size = true;
                }
                else { SCERROR(lineno, "unrecognized prop"); }
            }

            if (!has_eye || !has_uvw || !has_focus || !has_proj_size) {
                SCERROR(lineno, "missing camera prop(s)");
            }
        }
        else if (line.starts_with("light"))
        {
            light lt{ 0 };
            bool has_pos = false, has_rgb = false;

            while (sv_getline(scstr, scpos, line))
            {
                lineno++;
                if (line.empty()) { break; }

                if (line.starts_with("pos "))
                {
                    line.remove_prefix(sizeof("pos ") - 1);
                    if (!parsenum3(line, lt.pos.x(), lt.pos.y(), lt.pos.z())) {
                        SCERROR(lineno, "invalid position");
                    }
                    has_pos = true;
                }
                else if (line.starts_with("rgb "))
                {
                    line.remove_prefix(sizeof("rgb ") - 1);
                    
                    if (!parsenum3(line, lt.rgb.x(), lt.rgb.y(), lt.rgb.z()) ||
                        lt.rgb.x() < 0 || lt.rgb.y() < 0 || lt.rgb.z() < 0 ||
                        lt.rgb.x() > 1 || lt.rgb.y() > 1 || lt.rgb.z() > 1) {
                        SCERROR(lineno, "invalid color, must be in [0,1]");
                    }
                    has_rgb = true;
                }
                else { SCERROR(lineno, "unrecognized prop"); }
            }
            if (!has_pos || !has_rgb) {
                SCERROR(lineno, "missing light prop(s)");
            }

            L.push_back(lt);
        }
        else { SCERROR(lineno, "unrecognized prop"); }
    }

    return 0;
}

int Scene::read_objs(const std::vector<fs::path>& objpaths)
{
    bool any_missing_mat = false;
#if ENABLE_TEXTURES
    bool any_missing_uv = false;
#endif
    std::vector<int> badFidx; // faces that need fixing later

    for (const auto& objpath : objpaths)
    {
        rapidobj::Result res = rapidobj::ParseFile(objpath);
        if (res.error || !rapidobj::Triangulate(res))
        {
            std::cerr << objpath << ", line " << res.error.line_num;
            std::cerr << ": " << res.error.code.message() << "\n";
            return -1;
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

            // get roughness from shininess by solving
            // 1000-2000r+1000r^{2} = ns, this is what blender appears to use.
            // then approximate refl as 1-r (stupid but should work).
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
                shape.points.indices.size() != 0) 
            {
                std::cerr << objpath;
                std::cerr << ": polylines/points not supported\n";
                return -1;
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
                    any_missing_uv = true; 
                }
#endif
                if (matids[i / 3] != -1) [[likely]] {
                    t.matid = baseMid + matids[i / 3];
                }
                else { 
                    t.matid = -1;
                    bad = true; 
                    any_missing_mat = true; 
                }

                t.bb = get_tri_bbox(V, t.Vidx);

                F.push_back(std::move(t));

                if (bad) [[unlikely]] {
                    badFidx.push_back(int(F.size() - 1));
                }
            }
        }
    }

    // --------------- Fix bad faces ---------------  
    if (badFidx.size() != 0)
    {
        int default_matid = -1;
        if (any_missing_mat) {
            M.push_back(mat::default_mat());
            default_matid = int(M.size() - 1);
        }
#if ENABLE_TEXTURES
        int default_uvid = -1;
        if (any_missing_uv) {
            UV.push_back({ 0 });
            default_uvid = int(UV.size() - 1);
        }
#endif
        for (size_t i = 0; i < badFidx.size(); ++i)
        {
            auto& t = F[badFidx[i]];

            // missing material
            if (t.matid < 0)
            {
                assert(default_matid >= 0);
                t.matid = default_matid;
            }
#if ENABLE_TEXTURES
            // missing UV
            if (t.UVidx[0] < 0)
            {
                assert(default_uvid >= 0);
                t.UVidx = { default_uvid,
                    default_uvid, default_uvid };
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
    }

    if (F.empty() || V.empty()) {
        ERROR("no faces or vertices found");
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
    if (depth != bv_stop_depth)
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
        ERROR("max-bv is not a power of 2");
    }

    bv_stop_depth = ulog2(max_bv);
    uint last_full_depth = ulog2(F.size());

    // limit to one before last so that bvs are never empty
    if (bv_stop_depth >= last_full_depth && bv_stop_depth != 0) {
        bv_stop_depth = last_full_depth - 1;
    }

    gather_bvs(F.data(), F.data() + F.size());
    return 0;
}

Scene::Scene(const fs::path& scpath, const uint max_bv) :
    C({ 0 }), R(0, 0), m_ok(false)
{
    std::vector<fs::path> objpaths;
    m_ok = 
        read_scene(scpath, objpaths) == 0 &&
        read_objs(objpaths) == 0 &&
        init_bvs(max_bv) == 0;
}

uint Scene::nserial() const
{
    return Nwordshdr + camera::nserial +
        nsL() + nsV() + nsNV() + nsF() + nsNF() + nsM() + nsMF();
}

void Scene::serialize(uint* p) const
{ 
    /* size  */ p[0] = nserial();
    /* numV  */ p[1] = uint(V.size());
    /* numF  */ p[2] = uint(F.size());
    /* numL  */ p[3] = uint(L.size());
    /* Loff  */ p[4] = Nwordshdr + camera::nserial;
    /* Voff  */ p[5] = p[4] + nsL();
    /* NVoff */ p[6] = p[5] + nsV();
    /* Foff  */ p[7] = p[6] + nsNV();
    /* NFoff */ p[8] = p[7] + nsF();
    /* Moff  */ p[9] = p[8] + nsNF();
    /* MFoff */ p[10] = p[9] + nsM();
    /* resX  */ p[11] = R.first;
    /* resY  */ p[12] = R.second;
    
    assert(p[10] + nsMF() == nserial() 
        && "Header size not set correctly");
    p += Nwordshdr;

    C.serialize(p); 
    p += camera::nserial;
    p = vserialize(L, p);
    p = vserialize(V, p);
    p = vserialize(NV, p);
    p = vserialize(F, p);
    p = vserialize(NF, p);
    p = vserialize(M, p);
    p = vserialize(MF, p);
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