
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <string_view>
#include <memory>
#include <charconv>

#include "cxxopts.hpp"
#include "defs.hpp"

#include "io.h"



static int from_bin(const fs::path& inpath, BufWithSize<uint>& buf)
{
    scopedFILE f = SAFE_FOPEN(inpath.c_str(), "rb");
    if (!f) { ERROR("could not open input file"); }

    buf.size = fs::file_size(inpath) / 4;
    buf.ptr = std::make_unique<uint[]>(buf.size);

    if (std::fread(buf.get(), 4, buf.size, f.get()) != buf.size) {
        ERROR("could not read input file");
    }
    return 0;
}

static int to_bin(std::string_view outpath, BufWithSize<uint>& buf)
{
    scopedFILE f = SAFE_AFOPEN(outpath.data(), "wb");
    if (!f) { ERROR("could not open output file"); }

    if (std::fwrite(buf.get(), 4, buf.size, f.get()) != buf.size) {
        ERROR("could not write output file");
    }
    return 0;
}

static int to_hdr(std::string_view outpath, BufWithSize<uint>& buf)
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

    scopedFILE f = SAFE_AFOPEN(outpath.data(), "wb");
    if (!f) { ERROR("could not open output file"); }

    if (std::fwrite(out.c_str(), 1, out.length(), f.get()) != out.length()) {
        ERROR("could not write output file");
    }

    return 0;
}

static int raytrace(std::string_view outpath, std::string_view name,
    std::string_view ipaddr, std::string_view port, BufWithSize<uint>& buf)
{
    std::cout << "Sending " << name << "to FPGA...\n";

    const uint nbytes = uint(buf.size * 4);
    if (name.length() > NET_MAX_STRING) {
        name = name.substr(0, NET_MAX_STRING);
    } 
    if (TCP_send((byte*)buf.get(), nbytes, name.data(), ipaddr.data(), port.data()) != nbytes) {
        ERROR("failed to send scene");
    }

    std::cout << "Waiting for image...\n";

    byte* pdata; char* recv_name;
    int nrecv = TCP_recv(&pdata, &recv_name, port.data(), true);

    std::free(recv_name);
    auto data = scoped_cptr<byte[]>(pdata);

    uint resX = buf.ptr[11], resY = buf.ptr[12]; // nasty
    if (nrecv < 0) {
        ERROR("failed to receive image");
    }
    else if (nrecv != (resX * resY * 3)) {
        ERROR("received image dimensions are incorrect");
    }

#define ERRORSAVE ERROR("failed to save image")

    fs::path outfspath = outpath;
    fs::path outext = outfspath.extension();

    if (outext == ".bmp") {
        if (!write_bmp(outpath.data(), data.get(), resX, resY, 3)) { ERRORSAVE; }
    } 
    else if (outext == ".png") {
        if (!write_png(outpath.data(), data.get(), resX, resY, 3)) { ERRORSAVE; }
    } 
    else {
        auto p = outfspath.replace_extension(".bmp").string();
        if (!write_bmp(p.c_str(), data.get(), resX, resY, 3)) { ERRORSAVE; }
    }
#undef ERRORSAVE

    std::cout << "Saved image to " << outpath << "\n";
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
        ("max-bv", "Max bounding volumes. Must be a power of 2.", cxxopts::value<uint>()->default_value("128"))
        ("b,tobin", "Serialize scene file to binary format.")
        ("c,tohdr", "Convert scene file into C header.")       
        ("e,eswap", "Swap endianness.");
        
    cxxopts::ParseResult args;
    try {
        args = opts.parse(argc, argv);
    }
    catch (std::exception& e) {
        ERROR(e.what());
    }
   
    if (args["help"].as<bool>()) {
        std::cout << opts.help();
        return 0;
    }

    if (args["in"].count() == 0) {
        ERROR("no input file");
    }
    
    bool tobin = args["tobin"].count() != 0;
    bool tohdr = args["tohdr"].count() != 0;
    bool tofpga = args["rt"].count() != 0;
    bool eswap = args["eswap"].as<bool>();

    int tototal = int(tobin) + int(tohdr) + int(tofpga);
    if (tototal != 1) { 
        ERROR("provide one of tobin, tohdr, or rt"); 
    }

    fs::path inpath = args["in"].as<std::string>();
    auto& outpath = args["out"].as<std::string>();

    // ------------ Serialize or read serialized scene ------------ 
    BufWithSize<uint> Scbuf;
    if (inpath.extension() == ".scene")
    {
        Scene scene(inpath, args["max-bv"].as<uint>());
        if (!scene) { return EXIT_FAILURE; }

        Scbuf.size = scene.nserial();
        Scbuf.ptr = std::make_unique<uint[]>(Scbuf.size);
        scene.serialize(Scbuf.get());
    } 
    else {
        if (tobin && !eswap) {
            ERROR("scene is already in binary format");
        }
        int e = from_bin(inpath, Scbuf);
        if (e) { return e; }
    }

    if (eswap)
    {
        if (!tohdr) {
            for (uint i = 0; i < Scbuf.size; ++i) {
                Scbuf.ptr[i] = bswap(Scbuf.ptr[i]);
            }
        } else { std::cout << "Warning: ignoring eswap...\n"; }
    }

    // ------------ Do output ------------ 
    if (tofpga)
    {
        auto& rtargs = args["rt"].as<std::vector<std::string>>();
        if (rtargs.size() != 2) { 
            ERROR("missing rt options"); 
        }
        return raytrace(outpath, inpath.filename().string(), rtargs[0], rtargs[1], Scbuf);
    }
    else if (tobin) {
        return to_bin(outpath, Scbuf);
    }
    else if (tohdr) { 
        return to_hdr(outpath, Scbuf); 
    }
    else { 
        assert("unimplemented"); 
        std::unreachable(); 
    }

    auto tend = chrono::high_resolution_clock::now();

    std::cout << "Done in ";
    print_duration(std::cout, tend - tbeg);
    std::cout << ".\n";

    return 0;
}
