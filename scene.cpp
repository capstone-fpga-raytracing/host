
#include <cassert>
#include <string>
#include <string_view>
#include <iostream>
#include <charconv>

#include "defs.hpp"

#define ENABLE_TEXTURES 0

static void scerror(int lineno, const char* msg)
{
    std::cerr << "line " << lineno;
    std::cerr << ": " << msg << "\n";
}

#define SCBAIL(lineno, msg) \
    do { \
        scerror(lineno, msg); \
        return; \
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

SceneData::SceneData(const fs::path& scpath) : m_ok(false), C({0}), R(0, 0)
{
    // read entire file into memory.
    // this is okay as a .scene file is always small.
    BufWithSize<char> scbuf;
    {
        scopedFILE scfile = SAFE_FOPEN(scpath.c_str(), "rb");
        if (!scfile) {
            std::cerr << "could not open scene file\n";
            return;
        }   
        scbuf.size = fs::file_size(scpath);
        scbuf.buf = std::make_unique<char[]>(scbuf.size);

        if (std::fread(scbuf.get(), 1, scbuf.size, scfile.get()) != scbuf.size) {
            std::cerr << "could not read scene file\n";
            return;
        }
    }

    auto scdir = scpath.parent_path();
    std::vector<fs::path> objpaths;

    int lineno = 0;
    size_t scpos = 0;
    std::string_view line;
    std::string_view scstr(scbuf.get(), scbuf.size);

    // Parse scene file
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
                        SCBAIL(lineno, "invalid resolution");
                    }
                    has_res = true;
                }
                else { SCBAIL(lineno, "unrecognized prop"); }
            }
            if (!has_res) {
                SCBAIL(lineno, "missing render prop(s)");
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
                        SCBAIL(lineno, "invalid eye");
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
                        SCBAIL(lineno, "invalid uvw");
                    }
                    has_uvw = true;
                }
                else if (line.starts_with("focal_len "))
                {
                    line.remove_prefix(sizeof("focal_len ") - 1);
                    if (!parsenum(line, C.focal_len) || C.focal_len <= 0) {
                        SCBAIL(lineno, "invalid focal length");
                    }
                    has_focus = true;
                }
                else if (line.starts_with("proj_size "))
                {
                    line.remove_prefix(sizeof("proj_size ") - 1);

                    if (!parsenum(line, C.width) ||
                        !parsenum(line, C.height) ||
                        C.width <= 0 || C.height <= 0) {
                        SCBAIL(lineno, "invalid projection size");
                    }
                    has_proj_size = true;
                }
                else { SCBAIL(lineno, "unrecognized prop"); }
            }

            if (!has_eye || !has_uvw || !has_focus || !has_proj_size) {
                SCBAIL(lineno, "missing camera prop(s)");
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
                        SCBAIL(lineno, "invalid position");
                    }
                    has_pos = true;
                }
                else if (line.starts_with("rgb "))
                {
                    line.remove_prefix(sizeof("rgb ") - 1);
                    
                    if (!parsenum3(line, lt.rgb.x(), lt.rgb.y(), lt.rgb.z()) ||
                        lt.rgb.x() < 0 || lt.rgb.y() < 0 || lt.rgb.z() < 0 ||
                        lt.rgb.x() > 1 || lt.rgb.y() > 1 || lt.rgb.z() > 1) {
                        SCBAIL(lineno, "invalid color, must be in [0,1]");
                    }
                    has_rgb = true;
                }
                else { SCBAIL(lineno, "unrecognized prop"); }
            }
            if (!has_pos || !has_rgb) {
                SCBAIL(lineno, "missing light prop(s)");
            }

            L.push_back(lt);
        }
        else { SCBAIL(lineno, "unrecognized prop"); }
    }

    // Parse obj + mtl files
    std::vector<int> missing_matFids;
    for (const auto& objpath : objpaths)
    {
        rapidobj::Result res = rapidobj::ParseFile(objpath);
        if (res.error || !rapidobj::Triangulate(res))
        {
            std::cerr << objpath << ", line " << res.error.line_num;
            std::cerr << ": " << res.error.code.message() << "\n";
            return;
        }
        
        int baseVidx = int(V.size());
        auto& objverts = res.attributes.positions;
        for (size_t i = 0; i < objverts.size(); i += 3) {
            V.push_back({ objverts[i], objverts[i + 1], objverts[i + 2] });
        }

#if ENABLE_TEXTURES
        int baseUVidx = int(UV.size());
        auto& objUV = res.attributes.texcoords;
        for (size_t i = 0; i < objUV.size(); i += 2) {
            UV.push_back({ objUV[i], objUV[i + 1] });
        }
#endif
        int baseNVidx = int(NV.size());
        auto& objnormals = res.attributes.normals;
        for (size_t i = 0; i < objnormals.size(); i += 3) {
            NV.push_back({ objnormals[i], objnormals[i + 1], objnormals[i + 2] });
        }

        int baseMidx = int(M.size());
        auto& objmats = res.materials;
        for (size_t i = 0; i < objmats.size(); ++i)
        {
            auto& mobj = objmats[i];
            mat m;
            m.ka = mobj.ambient;
            m.kd = mobj.diffuse;
            m.ks = mobj.specular;
            m.ns = mobj.shininess;

            // approximate roughness from phong exponent by solving
            // 1000-2000x+1000x^{2} = ns, then refl=1-roughness (stupid but should work).
            // this is the what blender appears to use
            assert(m.ns >= 0);
            double ref_ns = m.ns > 1000 ? 1 : m.ns / 1000;
            double refl = std::sqrt(4 * ref_ns) / 2;
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
                return;
            }

            auto& meshidx = shape.mesh.indices;
            auto& matids = shape.mesh.material_ids;
            // sanity check
            assert(meshidx.size() / 3 == matids.size());

            for (size_t i = 0; i < meshidx.size(); i += 3)
            {
                F.push_back({
                    baseVidx + meshidx[i].position_index,
                    baseVidx + meshidx[i + 1].position_index,
                    baseVidx + meshidx[i + 2].position_index });

#if ENABLE_TEXTURES
                std::array<int, 3> ufidx;
                for (int j = 0; j < 3; ++j) {
                    int idx = meshidx[i + j].texcoord_index;
                    ufidx[j] = idx == -1 ? -1 : baseUVidx + idx;
                }
                UF.push_back(ufidx);
#endif
                std::array<int, 3> nfidx;
                for (int j = 0; j < 3; ++j) {
                    int idx = meshidx[i + j].normal_index;
                    nfidx[j] = idx == -1 ? -1 : baseNVidx + idx;
                }
                NF.push_back(nfidx);
            }

            for (size_t i = 0; i < matids.size(); ++i)
            {
                if (matids[i] < 0) [[unlikely]] {                  
                    MF.push_back(-1);
                    missing_matFids.push_back(int(MF.size() - 1));
                }
                else { MF.push_back(baseMidx + matids[i]); }
            }
        }
    }
    if (F.empty() || V.empty()) {
        std::cerr << "No faces or vertices found in " << scpath << "\n";
        return;
    }

    // assign default to faces with no material
    if (missing_matFids.size() != 0) 
    {
        M.push_back(mat::default_mat());

        int default_matid = int(M.size() - 1);
        for (size_t i = 0; i < missing_matFids.size(); ++i) {
            MF[missing_matFids[i]] = default_matid;
        }
    }

    m_ok = true;
}

uint SceneData::nserial() const
{
    return Nwordshdr + camera::nserial +
        nsL() + nsV() + nsNV() + nsF() + nsNF() + nsM() + nsMF();
}

void SceneData::serialize(uint* p) const
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

// not used. needs to be updated for scene and mtl files
#if 0
bool write_model(const char* obj_file, const char* mtl_file, SceneData& m)
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