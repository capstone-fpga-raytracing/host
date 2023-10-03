// https://en.wikipedia.org/wiki/Wavefront_.obj_file

#include <cassert>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>

#include "defs.hpp"


// does not check file for errors!
bool read_wavefront_model(const char* obj_file, const char* mtl_file, ModelData& m)
{
    std::ifstream file(obj_file);
    if (!file)
        return false;

    std::string line;
    std::istringstream is;
    std::size_t line_num = 1;

    while (std::getline(file, line))
    {
        if (line.empty())
            continue;

        if (line.starts_with("v "))
        {
            vec3 v;
            is.str(line.substr(2));
            is >> v.x() >> v.y() >> v.z();

            is >> std::ws; // set eof
            if (!is.eof())
            {
                float w; is >> w;
                if (w != 1.0f) {
                    std::cerr << "line " << line_num;
                    std::cerr << ": got 4d vertex\n";
                    return false;
                }
            }
            m.V.push_back(v);
        }
        else if (line.starts_with("vt "))
        {
            uv uv;
            is.str(line.substr(3));
            is >> uv.u;

            is >> std::ws; // set eof
            if (!is.eof())
            {
                is >> uv.v;
                is >> std::ws;
                if (!is.eof())
                {
                    float w; is >> w;
                    if (w != 0.0f) {
                        std::cerr << "line " << line_num;
                        std::cerr << ": got 3d texture\n";
                        return false;
                    }
                }
            }
            else uv.v = 0.0f;

            m.UV.push_back(uv);
        }
        else if (line.starts_with("vn "))
        {
            vec3 n;
            is.str(line.substr(3));
            is >> n.x() >> n.y() >> n.z();

            n.normalize();
            m.NV.push_back(n);
        }
        else if (line.starts_with("f "))
        {
            std::array<int, 3> f, uf, nf;
            int f3 = 0, uf3 = 0, nf3 = 0;
            is.str(line.substr(2));

            // horrible
            auto extract_face_vert =
                [](std::istream& is, int& fi, int& ufi, int& nfi)
                {
                    // format: vertex/texture/normal (1-indexed)
                    is >> fi;
                    if (is.peek() == '/')
                    {
                        is.get(); // skip slash
                        if (is.peek() == '/')
                        {
                            is.get();
                            ufi = 0;
                            is >> nfi;
                        }
                        else {
                            is >> ufi;
                            if (is.peek() == '/')
                            {
                                is.get();
                                is >> nfi;
                            }
                            else nfi = 0;
                        }
                    } else {
                        ufi = 0; // will be set to -1 later
                        nfi = 0;
                    }
                };

            extract_face_vert(is, f[0], uf[0], nf[0]);
            extract_face_vert(is, f[1], uf[1], nf[1]);
            extract_face_vert(is, f[2], uf[2], nf[2]);

            is >> std::ws; // set eof
            if (!is.eof()) {
                // this is a quad
                extract_face_vert(is, f3, uf3, nf3);
            }

            is >> std::ws;
            if (!is.eof())
            {
                std::cerr << "line " << line_num;
                std::cerr << ": got face with more than 4 verts\n";
                return false;
            }

            m.F.push_back(f);
            m.UF.push_back(uf);
            m.NF.push_back(nf);

            if (f3 != 0)
            {
                // convert quad into two triangles
                m.F.push_back({ f[0], f[2], f3 });
                m.UF.push_back({ uf[0], uf[2], uf3 });
                m.NF.push_back({ nf[0], nf[2], nf3 });
            }
        }
        else {
            std::cerr << "line " << line_num;
            std::cerr << ": unrecognized syntax\n";
            return false;
        }

        is.clear(); // clear eof
        line_num++;
    }

    assert(m.UF.size() == m.F.size() && m.NF.size() == m.F.size());

    // make face indices 0-indexed and wrap-around negative indices
    for (int i = 0; i < m.F.size(); ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            auto& fi = m.F[i][j];
            auto& ufi = m.UF[i][j];
            auto& nfi = m.NF[i][j];

            fi = fi < 0 ? fi + int(m.V.size()) : fi - 1;
            ufi = ufi < 0 ? ufi + int(m.UV.size()) : ufi - 1;
            nfi = nfi < 0 ? nfi + int(m.NV.size()) : nfi - 1;

            // todo: fix missing normals or report an error 
        }
    }

    return true;
}


bool write_wavefront_model(const char* obj_file, const char* mtl_file, ModelData& m)
{
    if (m.UF.size() != m.F.size() || 
        m.NF.size() != m.F.size()) 
    {
        std::cerr << "error: invalid geometry\n";
        return false;
    }

    std::ofstream file(obj_file);
    if (!file)
        return false;

    for (std::size_t i = 0; i < m.V.size(); ++i)
        file << "v " << m.V[i].x() << " " << m.V[i].y() << " " << m.V[i].z() << "\n";

    for (std::size_t i = 0; i < m.UV.size(); ++i)
        file << "vt " << m.UV[i].u << " " << m.UV[i].v << "\n";

    for (std::size_t i = 0; i < m.NV.size(); ++i)
        file << "vn " << m.NV[i].x() << " " << m.NV[i].y() << " " << m.NV[i].z() << "\n";

    // format: vertex/texture/normal (1-indexed)
    for (std::size_t irow = 0; irow < m.F.size(); ++irow)
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