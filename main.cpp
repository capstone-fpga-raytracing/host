
#include <cstdlib>
#include <iostream>
#include <memory>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <charconv>

#include "cxxopts/cxxopts.hpp"
#include "defs.hpp"

//#define TEST_MODELIO 0

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


[[noreturn]] void bail(const char* msg)
{
    std::cerr << msg << "\n";
    std::exit(EXIT_FAILURE);
}

int main(int argc, char** argv)
{
    cxxopts::Options opts("host", "Host-side of FPGA raytracer.");
    opts.add_options()
        ("h,help", "Show usage.")
        ("i,infile", "Input scene file (.scene)", cxxopts::value<std::string>(), "<infile>")
        ("o,outfile", "Output serialized scene (.bin or .h)", cxxopts::value<std::string>(), "<outfile>")
        ("e,eswap", "Swap endianness (only applies to binary file)");

    auto args = opts.parse(argc, argv);
    if (args["help"].as<bool>())
    {
        std::cout << opts.help();
        return EXIT_SUCCESS;
    }

    if (args["infile"].count() == 0) {
        bail("no input file");
    }
    if (args["outfile"].count() == 0) {
        bail("no output file");
    }

    fs::path inpath = args["infile"].as<std::string>();
    fs::path outpath = args["outfile"].as<std::string>();

    SceneData scene(inpath);
    if (!scene) {
        return EXIT_FAILURE;
    }

    BVTree bvh(scene);
    if (!bvh) {
        return EXIT_FAILURE;
    }

    uint nsmodel = scene.nserial();
    uint nserial = nsmodel + bvh.nserial();

    auto buf = std::make_unique<uint[]>(nserial);
    scene.serialize(buf.get());
    bvh.serialize(buf.get() + nsmodel);

    
    // write file
    std::ofstream outf(outpath, std::ios::binary);
    if (!outf) {
        bail("could not open output file");
    }
    auto outext = outpath.extension();
    if (outext == ".bin" || outext.empty()) 
    {
        // swap endianness
        if (args["eswap"].as<bool>()) {
            for (uint i = 0; i < nserial; ++i) {
                buf[i] = bswap(buf[i]);
            }
        }
        outf.write((const char*)buf.get(), 4 * nserial);
    }
    else if (outext == ".h") 
    {
        std::string out = "static const int bin[] = {";
        
        char strbuf[10] = { '0', 'x' };
        char* const begin = strbuf + 2;

        for (uint i = 0; i < nserial; ++i)
        {
            if (i % 12 == 0) { 
                out.append("\n    "); 
            }
            auto ret = std::to_chars(begin, std::end(strbuf), buf[i], 16);

            ptrdiff_t nchars = ret.ptr - begin;
            std::memmove(std::end(strbuf) - nchars, begin, nchars);
            std::memset(begin, '0', 8 - nchars);

            out.append(strbuf, 10);
            if (i != nserial - 1) {
                out.append(", ");
            }
        }
        out.append("\n};\n");

        outf.write(out.c_str(), out.length());
    }

    return EXIT_SUCCESS;
}