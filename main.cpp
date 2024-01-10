
#include "defs.hpp"
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdio>

#include "cxxopts/cxxopts.hpp"


#define TEST_SCENE 1
#define TEST_MODELIO 0


[[noreturn]] void bail(const char* msg)
{
    std::cerr << msg;
    std::exit(EXIT_FAILURE);
}

int main(int argc, char** argv)
{
    cxxopts::Options opts("host", "Host-side of FPGA raytracer.");
    opts.add_options()
        ("h,help", "Show usage.")
        ("i,infile", "Input model file (.obj).", cxxopts::value<std::string>(), "<infile>")
        ("o,outfile", "Output serialized model (.bin).", cxxopts::value<std::string>(), "<outfile>")
        ("e,eswap", "Swap endianness.");

    auto args = opts.parse(argc, argv);
    if (args["help"].as<bool>())
    {
        std::cout << opts.help();
        return EXIT_SUCCESS;
    }

    if (args["infile"].count() == 0)
        bail("error: no input file.\n");

    if (args["outfile"].count() == 0)
        bail("error: no output file.\n");

    auto& infile = args["infile"].as<std::string>();
    auto& outfile = args["outfile"].as<std::string>();

    SceneData scene;
    if (!read_model(infile.c_str(), scene))
        bail("error: failed to read model file.\n");

#if TEST_MODELIO
    if (!write_model("testobj.obj", nullptr, scene))
        bail("error: failed to test write model file.\n");
#endif

#if TEST_SCENE
    // These numbers mostly from blender
    //scene.C.eye = { 8.4585f, -2.5662f, 10.108f };
    scene.C.eye = { 7.0827, -3.4167, 7.4254 };
    scene.C.focal_len = 5;
    scene.C.width = 3.6;
    scene.C.height = scene.C.width * (240. / 320.); // 320x240 render  
    scene.C.u = { 1, 1, 0 };
    scene.C.v = { -1, 1, std::sqrt(2) };
    scene.C.w = { 1, -1, std::sqrt(2) };

    light l1, l2;
    //l1.pos = { 0.9502, 1.953, 4.1162 };
    //l2.pos = { -2.24469, 1.953, 4.1162 };  
    l1.pos = { 3.6746, 2.0055, 3.1325 };
    l2.pos = { 1.5699, 0.87056, 3.1325 };   

    //l1.rgb = { 1, 1, 0 }; // yellow
    //l2.rgb = { 1, 1, 0 }; // yellow
    l1.rgb = { 1, 1, 1 }; // white
    l2.rgb = { 1, 1, 1 }; // white
    
    scene.L.push_back(l1);
    scene.L.push_back(l2);
#endif

    BVTree bvh(scene);

    std::ofstream outf(outfile, std::ios::binary);
    if (!outf)
        bail("error: could not open output file.\n");

    uint nsmodel = scene.nserial();
    uint nserial = nsmodel + bvh.nserial();

    uint* buf = new uint[nserial];
    scene.serialize(buf);
    bvh.serialize(buf + nsmodel);

    // swap endianness
    if (args["eswap"].as<bool>()) {
        for (uint i = 0; i < nserial; ++i) {
            buf[i] = bswap(buf[i]);
        }
    }

    outf.write((const char *)buf, 4 * nserial);
    outf.close();

    delete[] buf;
    return 0;
}