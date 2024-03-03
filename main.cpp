
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

    if (verbose) {
        std::printf("Sending scene to FPGA...\n");
        std::printf(DASHES);
    }
    
    socket_t socket = TCP_connect2(host.data(), port.data(), verbose);
    if (socket == INV_SOCKET) {
        return -1;
    }
    if (TCP_send2(socket, (char*)buf.get(), nbytes_sc, verbose) != nbytes_sc) {
        return -1;
    }

    if (verbose) {
        std::printf(DASHES);
        std::printf("Waiting for image...\n");
        std::printf(DASHES);
    }
    
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
    if (verbose) {
        std::printf(DASHES);
    }

    DECL_UTF8PATH_CSTR(outpath)
    fs::path outext = outpath.extension();

    bool wr_err = false;
    if (outext == ".bmp") {
        wr_err = !write_bmp(poutpath, data.get(), resn.first, resn.second, 3);
    } 
    else if (outext == ".png") {
        wr_err = !write_png(poutpath, data.get(), resn.first, resn.second, 3);
    }
    else { wr_err = write_file(outpath, data.get(), nbytes_img); }

    if (wr_err) {
        return mERROR("failed to save image");
    }
    if (verbose) {
        std::printf("Saved image to %s\n", poutpath);
    }
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
    size_t ncandtris = 0;
    for (uint i = 0; i < sc.R.second; ++i) 
    {
        for (uint j = 0; j < sc.R.first; ++j) 
        {
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
                    ncandtris += bv.ntris;
                }
            }
            rdir += incr_diru;
        }
        rdir -= (float(sc.R.first) * incr_diru); // reset
        rdir += incr_dirv;
    }

    auto nrays = size_t(sc.R.first) * sc.R.second;
    float candavg = float(ncandtris) / (sc.F.size() * nrays);

    std::cout << "----------- BV report -----------\n";
    std::cout << "Num BVs: " << sc.BV.size() << "\n";
    std::cout << "Percent tris eliminated: " << 100 * (1 - candavg) << "%\n";
    std::cout << "Avg candidate tris per ray: " << float(ncandtris) / nrays << "\n";
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
    // cxxopts is slow but is also very convenient.
    cxxopts::Options opts("rthost", "FPGA raytracer host.");
    opts.add_options()
        ("h,help", "Show usage.")
        ("i,in", "Input file (.scene or .bin).", cxxopts::value<std::string>(), "<file>")
        ("o,out", "Output file (.bmp, .png, or .bin).", cxxopts::value<std::string>(), "<file>")
        ("rt", "Raytrace scene on FPGA. Faster if scene is in binary format.",
            cxxopts::value<std::string>()->implicit_value(RT_DEFAULTARGS), "<host>,<port>")
        ("max-bv", "Max bounding volumes. Must be a power of 2.", cxxopts::value<uint>()->default_value("128"), "<uint>")
        ("bv-report", "Report efficiency of bounding volumes (takes a few seconds).")
        ("ser-fmt", "Serialization format.", cxxopts::value<std::string>()->default_value("dup"), "<dup|nodup>")
        ("b,tobin", "Convert scene to binary format.")
        ("c,tohdr", "Convert scene to C header.")
        ("e,eswap", "Swap endianness.")
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
    bool tofpga = args["rt"].count() != 0;
    bool bv_report = args["bv-report"].count() != 0;
    
    int tototal = int(tobin) + int(tohdr) + int(tofpga);
    if (tototal == 0) {
        if (!bv_report) { return mERROR("nothing to do"); }
    }
    else if (tototal > 1) {
        return mERROR("more than one target");
    }

    bool eswap = args["eswap"].as<bool>();
    if (eswap && tohdr) {
        return mERROR("cannot apply eswap when target " 
            "is C header (makes no sense)");
    }

    if (tototal != 0 && bv_report) {
        std::cout <<
            "BV report takes a few seconds to generate. "
            "Timing will not be accurate.\n";
    }

    auto& serfmtstr = args["ser-fmt"].as<std::string>();
    bool dup = serfmtstr == "dup";
    bool nodup = serfmtstr == "nodup";
    serial_format serfmt = dup ?
        serial_format::Duplicate : serial_format::NoDuplicate;

    if (!dup && !nodup) {
        return mERROR("invalid serialization format");
    }
   
    fs::path inpath = args["in"].as<std::string>();

    fs::path outpath;
    bool has_outpath = args["out"].count() != 0;
    if (tototal != 0 && !has_outpath) {
        return mERROR("missing output file");
    }
    if (has_outpath) {
        outpath = args["out"].as<std::string>();
    }

    std::string rthost, rtport;
    if (tofpga) {
        auto& rtargs = args["rt"].as<std::string>();

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

    bool verbose = args["verbose"].count() != 0;
    uint max_bv = args["max-bv"].as<uint>();

    // the real work begins
    auto tbeg = chrono::high_resolution_clock::now();

    // ------------ Serialize or read serialized scene ------------ 
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
        if (tobin && !eswap) {
            return mERROR("scene is already in binary format");
        }
        int e = read_file(inpath, Scbuf);
        if (e) { return e; }

        // todo: may not always work, depends on endianness.
        // An endian swap may be necessary even if not specified on cmdline
        Scres.first = Scbuf.ptr[1];
        Scres.second = Scbuf.ptr[2];
    }

    if (eswap) {
        for (uint i = 0; i < Scbuf.size; ++i) {
            Scbuf.ptr[i] = bswap(Scbuf.ptr[i]);
        }
    }

    // ------------ Do output ------------ 
    int err = 0;
    if (tofpga) {
        err = raytrace(outpath, rthost, rtport, Scres, Scbuf, verbose);
    }
    else {
        if (verbose) {
            DECL_UTF8PATH_CSTR(outpath)
            std::printf("Writing to file %s\n", poutpath);
        }

        if (tobin) {
            err = write_file(outpath, Scbuf);
        } else if (tohdr) {
            err = to_hdr(outpath, Scbuf);
        }
        else if (!bv_report) {
            assert(false && "no output");
        }
    } 
    if (err) { return err; }

    if (verbose) {
        auto tend = chrono::high_resolution_clock::now();
        std::cout << "Completed in ";
        print_duration(std::cout, tend - tbeg);
        std::cout << ".\n";
    }

    return 0;
}
