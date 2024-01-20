
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <memory>
#include <charconv>

#include "cxxopts/cxxopts.hpp"
#include "defs.hpp"

#include "io.h"


#define BAIL(msg) \
    do { \
        std::cerr << msg << "\n"; \
        return EXIT_FAILURE; \
    } while (0)

int main(int argc, char** argv)
{
    auto tbeg = chrono::high_resolution_clock::now();

    // this library is slow but is also very convenient.
    cxxopts::Options opts("host", "Host-side of FPGA raytracer.");
    opts.add_options()
        ("h,help", "Show usage.")
        ("i,in", "Input scene (.scene or .bin).", cxxopts::value<std::string>(), "<file>")
        ("f,tofpga", "Send scene to FPGA for raytracing and save returned image."
            " Faster if scene is in binary format.", cxxopts::value<std::vector<std::string>>(), "<ipaddr>,<port>")
        ("b,tobin", "Serialize .scene to binary format.", cxxopts::value<std::string>(), "<file>")
        ("c,tohdr", "Convert scene into C header.", cxxopts::value<std::string>(), "<file>")
        ("e,eswap", "Swap endianness.");

    cxxopts::ParseResult args;
    try {
        args = opts.parse(argc, argv);
    }
    catch (std::exception& e) {
        BAIL(e.what());
    }
    
    if (args["help"].as<bool>())
    {
        std::cout << opts.help();
        return EXIT_SUCCESS;
    }

    if (args["in"].count() == 0) {
        BAIL("no input file");
    }
    
    bool tobin = args["tobin"].count() != 0;
    bool tohdr = args["tohdr"].count() != 0;
    bool tofpga = args["tofpga"].count() != 0;
    bool swap_endian = args["eswap"].as<bool>();

    int tototal = int(tobin) + int(tohdr) + int(tofpga);
    if (tototal != 1) { 
        BAIL("provide one of tobin, tohdr, or tofpga"); 
    }

    fs::path inpath = args["in"].as<std::string>();

    BufWithSize<uint> S; // serialized scene
    std::pair<uint, uint> res; // resolution
    if (inpath.extension() == ".scene")
    {
        SceneData scene(inpath);
        if (!scene) { return EXIT_FAILURE; }

        BVTree btree(scene);
        if (!btree) { return EXIT_FAILURE; }

        uint nscene = scene.nserial();
        uint ntotal = nscene + btree.nserial();

        S.size = ntotal;
        S.buf = std::make_unique<uint[]>(S.size);
        scene.serialize(S.buf.get());
        btree.serialize(S.buf.get() + nscene);

        res = scene.R;
    } 
    else {
        if (tobin && !swap_endian) {
            BAIL("scene is already in binary format");
        }

        S.size = fs::file_size(inpath) / 4;
        S.buf = std::make_unique<uint[]>(S.size);

        scopedFILE f = SAFE_FOPEN(inpath.c_str(), "rb");
        if (!f) { BAIL("could not open input file"); }

        if (std::fread(S.buf.get(), 4, S.size, f.get()) != S.size) {
            BAIL("could not read input file");
        }
    }
    if (swap_endian && !tohdr)
    {
        for (uint i = 0; i < S.size; ++i) {
            S.buf[i] = bswap(S.buf[i]);
        }
    }

    if (tobin)
    {
        auto& outpath = args["tobin"].as<std::string>();

        scopedFILE f = SAFE_AFOPEN(outpath.c_str(), "wb");
        if (!f) { BAIL("could not open output file"); }

        if (std::fwrite(S.buf.get(), 4, S.size, f.get()) != S.size) {
            BAIL("could not write output file");
        }
    }
    else if (tohdr)
    {
        auto& outpath = args["tohdr"].as<std::string>();

        std::string out = "static const int bin[] = {";

        char strbuf[10] = { '0', 'x' };
        char* const begin = strbuf + 2;

        for (uint i = 0; i < S.size; ++i)
        {
            if (i % 12 == 0) {
                out.append("\n    ");
            }
            auto ret = std::to_chars(begin, std::end(strbuf), S.buf[i], 16);

            ptrdiff_t nchars = ret.ptr - begin;
            std::memmove(std::end(strbuf) - nchars, begin, nchars);
            std::memset(begin, '0', 8 - nchars);

            out.append(strbuf, 10);
            if (i != S.size - 1) {
                out.append(", ");
            }
        }
        out.append("\n};\n");

        scopedFILE f = SAFE_AFOPEN(outpath.c_str(), "wb");
        if (!f) { BAIL("could not open output file"); }

        if (std::fwrite(out.c_str(), 1, out.length(), f.get()) != out.length()) {
            BAIL("could not write output file");
        }
    }
    else if (tofpga)
    {
        auto& fpga_args = args["tofpga"].as<std::vector<std::string>>();
        if (fpga_args.size() != 2) {
            BAIL("missing or extra arguments");
        }

        std::string tcp_name = inpath.filename().string();
        const char* addr = fpga_args[0].c_str();
        const char* port = fpga_args[1].c_str();

        std::cout << "Sending " << tcp_name << " to FPGA...\n";
        if (TCP_send((byte*)S.buf.get(), uint(S.size * 4), tcp_name.c_str(), addr, port) != 0) {
            BAIL("failed to send scene");
        }

        std::cout << "Waiting for image...\n";

        byte* data; char* name;
        int nrecv = TCP_recv(&data, &name, port, false);

        auto data_s = scoped_mallocptr<byte[]>(data);
        auto name_s = scoped_mallocptr<char[]>(name);

        if (nrecv < 0 || std::strcmp(name, tcp_name.c_str()) != 0) {
            BAIL("failed to receive image");
        }

        if (!write_bmp(tcp_name.c_str(), data, res.first, res.second, 3)) { 
            BAIL("failed to save image");
        }
    }

    auto tend = chrono::high_resolution_clock::now();
    auto ttaken = tend - tbeg;

    std::cout << "Done in ";
    if (ttaken > chrono::milliseconds(1)) { // chrono format does not work on gcc, use count
        std::cout << chrono::duration_cast<chrono::milliseconds>(ttaken).count() << "ms";
    } else { 
        std::cout << chrono::duration_cast<chrono::microseconds>(ttaken).count() << "us";
    }
    std::cout << ".\n";

    return EXIT_SUCCESS;
    
}