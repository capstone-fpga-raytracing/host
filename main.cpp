
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

int main(int argc, char** argv)
{
    auto tbeg = chrono::high_resolution_clock::now();

    // cxxopts is slow but is also very convenient.
    cxxopts::Options opts("host", "Host-side of FPGA raytracer.");
    opts.add_options()
        ("h,help", "Show usage.")
        ("i,in", "Input file (.scene or .bin)", cxxopts::value<std::string>(), "<file>")
        ("o,out", "Output file (.bin, .bmp or .png).", cxxopts::value<std::string>(), "<file>")
        ("rt", "Raytrace scene on FPGA. Faster if scene is in binary format.", 
            cxxopts::value<std::vector<std::string>>(), "<fpga_ipaddr>,<port>")
        ("max-bv", "Max bounding volumes. Must be a power of 2.",
            cxxopts::value<uint>()->default_value("128"))
        ("ser-fmt", "Serialization format.",
            cxxopts::value<std::string>()->default_value("dup"), "{dup|nodup}")
        ("b,tobin", "Serialize scene to binary format.")
        ("c,tohdr", "Serialize scene into C header.")      
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
    bool eswap = args["eswap"].as<bool>();

    int tototal = int(tobin) + int(tohdr) + int(tofpga);
    if (tototal != 1) {
        hERROR("provide one of tobin, tohdr, or rt");
    }

    auto& serfmtstr = args["ser-fmt"].as<std::string>();
    bool dup = serfmtstr == "dup";
    bool nodup = serfmtstr == "nodup";
    serial_format serfmt = dup ?
        serial_format::Duplicate : serial_format::NoDuplicate;

    if (!dup && !nodup) {
        hERROR("invalid serialization format");
    }

    fs::path inpath = args["in"].as<std::string>();
    fs::path outpath = args["out"].as<std::string>();

    // ------------ Serialize or read serialized scene ------------ 
    BufWithSize<uint> Scbuf;
    if (inpath.extension() == ".scene")
    {
        Scene scene(inpath, args["max-bv"].as<uint>(), serfmt);
        if (!scene) { return EXIT_FAILURE; }

        Scbuf.size = scene.nserial();
        Scbuf.ptr = std::make_unique<uint[]>(Scbuf.size);
        scene.serialize(Scbuf.get());
    } 
    else {
        if (tobin && !eswap) {
            hERROR("scene is already in binary format");
        }
        int e = read_file(inpath, Scbuf);
        if (e) { return e; }
    }

    if (eswap)
    {
        if (!tohdr) {
            for (uint i = 0; i < Scbuf.size; ++i) {
                Scbuf.ptr[i] = bswap(Scbuf.ptr[i]);
            }
        } else { std::cout << "Ignored --eswap since target is C header.\n"; }
    }

    // ------------ Do output ------------ 
    int err;
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
    else { 
        assert(false);
        std::unreachable(); 
    }
    if (err) { return err; }

    auto tend = chrono::high_resolution_clock::now();
    std::cout << "Done in ";
    print_duration(std::cout, tend - tbeg);
    std::cout << ".\n";

    return 0;
}
