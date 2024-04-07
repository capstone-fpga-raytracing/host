
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <string_view>
#include <memory>
#include <charconv>

#include "cxxopts.hpp"
#include "defs.hpp"

#include "io.h"

// using Intel's DE1 Linux build
#define RT_DEFAULT_HOST "de1soclinux"
#define RT_DEFAULT_PORT "50000"
#define RT_DEFAULTARGS RT_DEFAULT_HOST "," RT_DEFAULT_PORT

#ifdef _WIN32
static bool tcp_init = false;

// no multithreading, so this is okay.
static bool tcp_win32_initonce() {
    if (!tcp_init) {
        tcp_init = (TCP_win32_init() == 0);
    }
    return tcp_init;
}
#endif

static int raytrace(const fs::path& outpath, std::string_view host, std::string_view port, 
    std::pair<uint, uint> resn, BufWithSize<uint>& buf, bool verbose = false)
{
#ifdef _WIN32
    if (!tcp_win32_initonce()) {
        return mERROR("failed to initialize TCP");
    }
#endif 
#define DASHES "----------------------------\n"

    const uint nbytes_sc = uint(buf.size * 4);
    const uint nbytes_img = resn.first * resn.second * 3;

    std::printf("Sending scene to FPGA at '%s'...\n", host.data());
    if (verbose) { std::printf(DASHES); }
    
    socket_t socket = TCP_connect2(host.data(), port.data(), verbose);
    if (socket == INV_SOCKET) {
        return -1;
    }
    if (TCP_send2(socket, (char*)buf.get(), nbytes_sc, verbose) != nbytes_sc) {
        return -1;
    }

    if (verbose) { std::printf(DASHES); }
    std::printf("Waiting for image...\n");
    if (verbose) { std::printf(DASHES); }
    
    char* pdata;
    int nrecv = TCP_recv2(socket, &pdata, verbose);
    auto data = scoped_cptr<char[]>(pdata);
    if (nrecv < 0) {
        if (verbose) { std::printf(DASHES); }
        return mERROR("failed to receive image");
    }
    else if (nrecv != nbytes_img) {
        TCP_close(socket);
        if (verbose) { std::printf(DASHES); }
        return mERROR("received %d bytes, expected %u", nrecv, nbytes_img);
    }

    TCP_close(socket);
    if (verbose) { std::printf(DASHES); }

    DECL_UTF8PATH_CSTR(outpath)
    fs::path outext = outpath.extension();

    bool wr_err = false;
    if (outext == ".bmp") {
        wr_err = !write_bmp(poutpath, data.get(), resn.first, resn.second, 3);
    } else if (outext == ".png") {
        wr_err = !write_png(poutpath, data.get(), resn.first, resn.second, 3);
    } 
    else { wr_err = write_file(outpath, data.get(), nbytes_img); }

    if (wr_err) {
        return mERROR("failed to save image");
    }
    
    std::printf("Saved image to %s\n", poutpath);
    return 0;

#undef DASHES
}

static void BV_report(const Scene& sc) 
{
    // this is viewing_ray from raytracing-basic, optimized
    // and with stretching issue fixed
    float world_du = sc.C.width / sc.R.first;
    float world_dv = sc.C.height / sc.R.second;
    float aspratio = float(sc.R.first) / sc.R.second;

    float base_u = aspratio * (world_du - sc.C.width) / 2;
    float base_v = (sc.C.height - world_dv) / 2;

    const vec3 base_dir = base_u * sc.C.u + base_v * sc.C.v - sc.C.focal_len * sc.C.w;
    const vec3 incr_diru = aspratio * world_du * sc.C.u;
    const vec3 incr_dirv = -world_dv * sc.C.v;

    // camera ray
    vec3 rorig = sc.C.eye, rdir = base_dir;

    // intersect every ray with every bounding volume and count intersection
    // "candidates" (triangles that cannot be eliminated by BVs)
    size_t total_candtris = 0, total_candbvs = 0;
    size_t max_candtris = 0, max_candbvs = 0;
    size_t nrays_inter = 0;
    for (uint i = 0; i < sc.R.second; ++i) 
    {
        for (uint j = 0; j < sc.R.first; ++j) 
        {
            size_t candtris = 0, candbvs = 0;
            for (const bv& bv : sc.BV) 
            {
                auto& bb = bv.bb;
                float t_entry = -std::numeric_limits<float>::infinity();
                float t_exit = std::numeric_limits<float>::infinity();

                for (int k = 0; k < 3; ++k)
                {
                    if (rdir[k] == 0) {
                        continue;
                    }
                    float t1 = (bb.cmin[k] - rorig[k]) / rdir[k];
                    float t2 = (bb.cmax[k] - rorig[k]) / rdir[k];
                    
                    if (rdir[k] > 0) {
                        t_entry = std::max(t_entry, t1);
                        t_exit = std::min(t_exit, t2);
                    }
                    else {
                        t_entry = std::max(t_entry, t2);
                        t_exit = std::min(t_exit, t1);
                    }
                }

                if (t_exit >= t_entry && t_exit >= 0) {
                    candtris += bv.ntris;
                    candbvs++;
                }
            }
            if (candbvs > 0) {
                nrays_inter++;
            }
            max_candtris = std::max(max_candtris, candtris);
            max_candbvs = std::max(max_candbvs, candbvs);

            total_candtris += candtris;
            total_candbvs += candbvs;

            rdir += incr_diru;
        }
        rdir -= (float(sc.R.first) * incr_diru); // reset
        rdir += incr_dirv;
    }

    auto nrays = size_t(sc.R.first) * sc.R.second;
    float candavg = float(total_candtris) / (sc.F.size() * nrays);

    std::cout << "----------- BV report -----------\n";
    std::cout << "Num BVs: " << sc.BV.size() << "\n";
    std::cout << "Percent tris eliminated: " << 100 * (1 - candavg) << "%\n";
    std::cout << "Avg candidate tris per ray: " << float(total_candtris) / nrays << "\n";
    std::cout << "Avg candidate BVs per ray: " << float(total_candbvs) / nrays << "\n";
    std::cout << "Avg candidate tris per intersecting ray: " << float(total_candtris) / nrays_inter << "\n";
    std::cout << "Avg candidate BVs per intersecting ray: " << float(total_candbvs) / nrays_inter << "\n";
    std::cout << "Avg cand tris per cand BV: " << float(total_candtris) / total_candbvs << "\n";
    std::cout << "Max candidate tris: " << max_candtris << "\n";
    std::cout << "Max candidate BVs: " << max_candbvs << "\n";
    std::cout << "---------------------------------\n";
}

static int to_hdr(const fs::path& outpath, BufWithSize<uint>& buf)
{
    std::string out = "static const int bin[] = {";

    char strbuf[10] = { '0', 'x' };
    char* const begin = strbuf + 2;

    for (uint i = 0; i < buf.size; ++i)
    {
        if (i % 12 == 0) {
            out.append("\n    ");
        }
        auto ret = std::to_chars(begin, std::end(strbuf), buf.ptr[i], 16);

        ptrdiff_t nchars = ret.ptr - begin;
        std::memmove(std::end(strbuf) - nchars, begin, nchars);
        std::memset(begin, '0', 8 - nchars);

        out.append(strbuf, 10);
        if (i != buf.size - 1) {
            out.append(", ");
        }
    }
    out.append("\n};\n");

    return write_file(outpath, out.c_str(), out.length());
}

int main(int argc, char** argv)
{
    cxxopts::Options opts("rthost", "FPGA raytracer host.");
    opts.add_options()
        ("h,help", "Show usage.")
        ("i,in", "Scene to render (.scene or binary file).", cxxopts::value<std::string>(), "<file>")
        ("o,out", "Output (.bmp, .png, or binary file).", cxxopts::value<std::string>(), "<file>")
        ("dest", "FPGA network destination.", cxxopts::value<std::string>()->default_value(RT_DEFAULTARGS), "<host>,<port>")
        ("max-bv", "Max bounding volumes. Must be a power of 2.", cxxopts::value<uint>()->default_value("128"), "<uint>")      
        ("serfmt", "Serialization format.", cxxopts::value<std::string>()->default_value("dup"), "<dup|nodup>")
        ("b,tobin", "Convert scene to .bin.")
        ("c,tohdr", "Convert scene to C header.")
        ("bv-report", "Report on BV efficiency (might take a few seconds).")
        ("v,verbose", "Verbose mode.");

    cxxopts::ParseResult args;
    try {
        args = opts.parse(argc, argv);
    }
    catch (std::exception& e) {
        return mERROR(e.what());
    }

    if (args["help"].as<bool>()) {
        std::cout << opts.help();
        return 0;
    }

    if (args["in"].count() == 0) {
        return mERROR("no input file");
    }

    bool tobin = args["tobin"].count() != 0;
    bool tohdr = args["tohdr"].count() != 0;
    bool bv_report = args["bv-report"].count() != 0;

    int run_util = int(tobin) + int(tohdr) + int(bv_report);
    if (run_util > 1) {
        return mERROR("more than one target");
    }

    bool run_rt = !run_util;
    std::string rthost, rtport;
    if (run_rt) {
        auto& rtargs = args["dest"].as<std::string>();

        size_t sepoff = rtargs.find(',');
        if (sepoff == rtargs.npos) {
            rthost = rtargs;
            rtport = RT_DEFAULT_PORT;
        }
        else if (sepoff == 0) {
            return mERROR("missing FPGA hostname/ipaddr");
        }
        else {
            rthost = rtargs.substr(0, sepoff);
            rtport = rtargs.substr(sepoff + 1);
        }
    }
    else if (args["dest"].count() != 0) {
        return mERROR("option --dest is invalid");
    }

    fs::path inpath = args["in"].as<std::string>();
    
    bool needs_outpath = run_rt || tobin || tohdr;
    bool has_outpath = args["out"].count() != 0;
    if (needs_outpath && !has_outpath) {
        return mERROR("missing output file");
    }
    else if (!needs_outpath && has_outpath) {
        return mERROR("option --out is invalid");
    }

    fs::path outpath;
    if (has_outpath) {
        outpath = args["out"].as<std::string>(); 
    }
    
    auto& serfmtstr = args["serfmt"].as<std::string>();
    bool dup = serfmtstr == "dup";
    bool nodup = serfmtstr == "nodup";
    serial_format serfmt = dup ?
        serial_format::Duplicate : serial_format::NoDuplicate;

    if (!dup && !nodup) {
        return mERROR("invalid serialization format");
    }

    bool verbose = args["verbose"].count() != 0;
    uint max_bv = args["max-bv"].as<uint>();

    // the real work begins
    auto tbeg = chrono::high_resolution_clock::now();

    // ---------------- Read scene ----------------- 
    BufWithSize<uint> Scbuf;
    std::pair<uint, uint> Scres;
    if (inpath.extension() == ".scene")
    {
        Scene scene(inpath, max_bv, serfmt, verbose);
        if (!scene) { return EXIT_FAILURE; }

        Scbuf.size = scene.nserial();
        Scbuf.ptr = std::make_unique<uint[]>(Scbuf.size);
        scene.serialize(Scbuf.get());
        Scres = scene.R;

        if (bv_report) { BV_report(scene); }
    } 
    else {
        if (bv_report) {
            return mERROR("bv report expects .scene file");
        }
        int e = read_file(inpath, Scbuf);
        if (e) { return e; }

        if (Scbuf.ptr[0] == Scene::MAGIC) {
            Scres = { Scbuf.ptr[1], Scbuf.ptr[2] };
        }
        else if (Scbuf.ptr[0] == bswap32(Scene::MAGIC)) {         
            for (uint i = 0; i < Scbuf.size; ++i) {
                Scbuf.ptr[i] = bswap32(Scbuf.ptr[i]);
            }
            Scres = { Scbuf.ptr[1], Scbuf.ptr[2] };
        }
        else return mERROR("missing magic number");
    }

    // ------------ Do output ------------ 
    int err = 0;
    if (run_rt) {
        err = raytrace(outpath, rthost, rtport, Scres, Scbuf, verbose);
    } else {
        if (tobin) {
            err = write_file(outpath, Scbuf);
        } 
        else if (tohdr) {
            err = to_hdr(outpath, Scbuf);
        } 
        else if (!bv_report) {
            assert(false && "no output");
        }
        if (!err && !bv_report) {
            std::cout << "Saved output to " << outpath << "\n";
        }
    }
    if (err) { return err; }

    auto tend = chrono::high_resolution_clock::now();
    std::cout << "Completed in ";
    print_duration(std::cout, tend - tbeg);
    std::cout << ".\n";

    return 0;
}
