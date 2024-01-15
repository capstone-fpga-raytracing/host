// https://en.wikipedia.org/wiki/Wavefront_.obj_file

#include <cassert>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <charconv>

#include "defs.hpp"
#include "rapidobj/rapidobj.hpp"


static void scerror(int lineno, const char* msg)
{
    std::cerr << "line " << lineno;
    std::cerr << ": " << msg << "\n";
}

SceneData::SceneData(const fs::path& scpath) : m_ok(false), C({0}), R(0, 0)
{
    std::ifstream scfile(scpath);
    if (!scfile) {
        std::cerr << "could not open scene file\n";
        return;
    }

    auto scdir = scpath.parent_path();
    std::vector<fs::path> objpaths;

    int lineno = 0;
    std::string line;
    std::istringstream is;
    while (std::getline(scfile, line))
    {
        lineno++;
        if (line.empty()) { continue; }

        if (line == "camera")
        {
            bool has_eye = false, 
                has_uvw = false, 
                has_focus = false, 
                has_proj_size = false;

            while (std::getline(scfile, line))
            {
                lineno++;
                if (line.empty()) { break; }
                is.str(line);

                if (line.starts_with("eye "))
                {
                    is.ignore(sizeof("eye ") - 1);
                    vec3& eye = C.eye;
                    is >> eye.x() >> eye.y() >> eye.z();
                    has_eye = true;
                }
                else if (line.starts_with("uvw "))
                {
                    is.ignore(sizeof("uvw ") - 1);
                    vec3& u = C.u; vec3& v = C.v; vec3& w = C.w;
                    is >> u.x() >> u.y() >> u.z()
                        >> v.x() >> v.y() >> v.z()
                        >> w.x() >> w.y() >> w.z();
                    has_uvw = true;
                }
                else if (line.starts_with("focal_len "))
                {
                    is.ignore(sizeof("focal_len ") - 1);
                    is >> C.focal_len;
                    if (C.focal_len <= 0) {
                        scerror(lineno, "Invalid focal length");
                        return;
                    }
                    has_focus = true;
                }
                else if (line.starts_with("proj_size "))
                {
                    is.ignore(sizeof("proj_size ") - 1);
                    is >> C.width >> C.height;
                    if (C.width <= 0 || C.height <= 0) {
                        scerror(lineno, "Invalid projection size");
                        return;
                    }
                    has_proj_size = true;
                }
                else { scerror(lineno, "ignored prop"); }

                is.clear();
            }
            if (!has_eye || !has_uvw || !has_focus || !has_proj_size)
            {
                scerror(lineno, "missing camera prop(s)");
                return;
            }
        }
        else if (line.starts_with("light"))
        {
            light lt{ 0 };
            bool has_pos = false, has_rgb = false;

            while (std::getline(scfile, line))
            {
                lineno++;
                if (line.empty()) { break; }
                is.str(line);

                if (line.starts_with("pos "))
                {
                    is.ignore(sizeof("pos ") - 1);
                    is >> lt.pos.x() >> lt.pos.y() >> lt.pos.z();
                    has_pos = true;
                }
                else if (line.starts_with("rgb "))
                {
                    is.ignore(sizeof("rgb ") - 1);
                    is >> lt.rgb.x() >> lt.rgb.y() >> lt.rgb.z();
                    if (lt.rgb.x() < 0 || lt.rgb.y() < 0 || lt.rgb.z() < 0) {
                        scerror(lineno, "Color must be in [0,1]");
                        return;
                    }
                    has_rgb = true;
                }
                else { scerror(lineno, "ignored prop"); }

                is.clear();
            }
            if (!has_pos || !has_rgb) {
                scerror(lineno, "missing light prop(s)");
                return;
            }

            L.push_back(lt);
        }
        else if (line == "render") 
        {
            bool has_res = false;
            while (std::getline(scfile, line)) 
            {
                lineno++;
                if (line.empty()) { break; }
                is.str(line);

                if (line.starts_with("res "))
                {
                    is.ignore(sizeof("res ") - 1);
                    is >> R.first >> R.second;
                    has_res = true;
                }
                else { scerror(lineno, "ignored prop"); }

                is.clear();
            }
            if (!has_res) {
                scerror(lineno, "missing render prop(s)");
                return;
            }
        }
        else if (line == "obj")
        {
            while (std::getline(scfile, line)) {
                lineno++;
                if (line.empty()) { break; }

                objpaths.push_back(scdir / fs::path(line));
            }
        }
    }
    scfile.close();

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
                std::cerr << ": polylines/points not supported";
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
