
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <string_view>
#include <memory>
#include <charconv>

#include "cxxopts.hpp"
#include "defs.hpp"

#include "io.h"


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

static int raytrace(const fs::path& outpath, 
    std::string_view ipaddr, std::string_view port, BufWithSize<uint>& buf)
{
    std::cout << "Sending scene to FPGA...\n";

    const uint nbytes = uint(buf.size * 4);
    if (TCP_send((byte*)buf.get(), nbytes, ipaddr.data(), port.data()) != nbytes) {
        hERROR("failed to send scene");
    }

    std::cout << "Waiting for image...\n";

    byte* pdata;
    int nrecv = TCP_recv(&pdata, port.data(), true);
    auto data = scoped_cptr<byte[]>(pdata);

    uint resX = buf.ptr[1], resY = buf.ptr[2]; // nasty
    if (nrecv < 0) {
        hERROR("failed to receive image");
    }
    else if (nrecv != (resX * resY * 3)) {
        hERROR("received image dimensions are incorrect");
    }

#define ERRORSAVE hERROR("failed to save image")

    fs::path outext = outpath.extension();
#ifdef _MSC_VER
    auto outpath_bstr = outpath.string();
    const char* outpathb = outpath_bstr.c_str();
#else
    const char* outpathb = outpath.c_str();
#endif

    if (outext == ".bmp") {
        if (!write_bmp(outpathb, data.get(), resX, resY, 3)) { ERRORSAVE; }
    } 
    else if (outext == ".png") {
        if (!write_png(outpathb, data.get(), resX, resY, 3)) { ERRORSAVE; }
    } 
    else {
        if (!write_bmp(outpathb, data.get(), resX, resY, 3)) { ERRORSAVE; }
    }
#undef ERRORSAVE

    std::cout << "Saved image to " << outpathb << "\n";
    return 0;
}

static void BV_report(const Scene& sc) 
{
    // this is viewing_ray from the reference code, optimized
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
        rdir -= (sc.R.first * incr_diru); // reset
        rdir += incr_dirv;
    }

    auto nrays = size_t(sc.R.first) * sc.R.second;
    float candavg = float(ncandtris) / (sc.F.size() * nrays);

    std::cout << "----------- BV report -----------\n";
    std::cout << "Num BVs: " << sc.BV.size() << "\n";
    std::cout << "Percent tris eliminated by bvs: " << 100 * (1 - candavg) << "%\n";
    std::cout << "Num candidate tris per ray: " << float(ncandtris) / nrays << "\n";
    std::cout << "---------------------------------\n";
}

int main(int argc, char** argv)
{
    // cxxopts is slow but is also very convenient.
    cxxopts::Options opts("host", "FPGA raytracer.");
    opts.add_options()
        ("h,help", "Show usage.")
        ("i,in", "Input scene (.scene or .bin).", cxxopts::value<std::string>(), "<file>")
        ("o,out", "Output file (.bin, .bmp or .png).", cxxopts::value<std::string>(), "<file>")
        ("rt", "Raytrace scene on FPGA. Faster if scene is in binary format.", 
            cxxopts::value<std::vector<std::string>>(), "<ipaddr>,<port>")
        ("max-bv", "Max bounding volumes. Must be a power of 2.",
            cxxopts::value<uint>()->default_value("128"), "<uint>")
        ("bv-report", "Report on efficiency of BVs. Input must be a .scene file.")
        ("b,tobin", "Convert scene to binary format.")
        ("c,tohdr", "Convert scene to C header.")
        ("ser-fmt", "Serialization format.",
            cxxopts::value<std::string>()->default_value("dup"), "{dup|nodup}")
        ("e,eswap", "Swap endianness.");
        
    cxxopts::ParseResult args;
    try {
        args = opts.parse(argc, argv);
    }
    catch (std::exception& e) {
        hERROR(e.what());
    }
   
    if (args["help"].as<bool>()) {
        std::cout << opts.help();
        return 0;
    }

    if (args["in"].count() == 0) {
        hERROR("no input file");
    }
    
    bool tobin = args["tobin"].count() != 0;
    bool tohdr = args["tohdr"].count() != 0;
    bool tofpga = args["rt"].count() != 0;
    bool bv_report = args["bv-report"].count() != 0;
    
    int tototal = int(tobin) + int(tohdr) + int(tofpga);
    if (tototal == 0) {
        if (!bv_report) { hERROR("nothing to do"); }
    }
    else if (tototal > 1) {
        hERROR("more than one target");
    }

    if (tototal == 1 && bv_report) {
        std::cout << 
            "Warning: BV report takes a few seconds to generate. " 
            "Timing report will not be accurate.\n";
    }

    auto& serfmtstr = args["ser-fmt"].as<std::string>();
    bool dup = serfmtstr == "dup";
    bool nodup = serfmtstr == "nodup";
    serial_format serfmt = dup ?
        serial_format::Duplicate : serial_format::NoDuplicate;

    if (!dup && !nodup) {
        hERROR("invalid serialization format");
    }

    bool eswap = args["eswap"].as<bool>();
    if (eswap && tohdr) {
        std::cout << "Ignored --eswap because of --tohdr.\n";
        eswap = false;
    }

    fs::path inpath = args["in"].as<std::string>();
    fs::path outpath;
    if (args["out"].count() != 0) {
        outpath = args["out"].as<std::string>();
    }

    auto tbeg = chrono::high_resolution_clock::now();

    // ------------ Serialize or read serialized scene ------------ 
    BufWithSize<uint> Scbuf;
    if (inpath.extension() == ".scene")
    {
        Scene scene(inpath, args["max-bv"].as<uint>(), serfmt);
        if (!scene) { return EXIT_FAILURE; }

        Scbuf.size = scene.nserial();
        Scbuf.ptr = std::make_unique<uint[]>(Scbuf.size);
        scene.serialize(Scbuf.get());

        if (bv_report) { BV_report(scene); }
    } 
    else {
        if (tobin && !eswap) {
            hERROR("scene is already in binary format");
        }
        int e = read_file(inpath, Scbuf);
        if (e) { return e; }
    }

    if (eswap) {
        for (uint i = 0; i < Scbuf.size; ++i) {
            Scbuf.ptr[i] = bswap(Scbuf.ptr[i]);
        }
    }

    // ------------ Do output ------------ 
    int err = 0;
    if (tofpga)
    {
        auto& rtargs = args["rt"].as<std::vector<std::string>>();
        if (rtargs.size() != 2) { 
            hERROR("missing rt options"); 
        }
        err = raytrace(outpath, rtargs[0], rtargs[1], Scbuf);
    }
    else if (tobin) {
        err = write_file(outpath, Scbuf);
    }
    else if (tohdr) { 
        err = to_hdr(outpath, Scbuf); 
    }
    else if (!bv_report) { 
        assert(false && "no output");
    }
    if (err) { return err; }

    auto tend = chrono::high_resolution_clock::now();
    std::cout << "Completed in ";
    print_duration(std::cout, tend - tbeg);
    std::cout << ".\n";

    return 0;
}
