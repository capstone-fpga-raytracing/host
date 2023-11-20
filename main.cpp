
#include "defs.hpp"
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdio>

#include "cxxopts/cxxopts.hpp"


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
        ("o,outfile", "Output serialized model (.bin).", cxxopts::value<std::string>(), "<outfile>");

    auto res = opts.parse(argc, argv);
    if (res["help"].as<bool>())
    {
        std::cout << opts.help();
        return EXIT_SUCCESS;
    }

    if (res["infile"].count() == 0)
        bail("error: no input file.\n");

    if (res["outfile"].count() == 0)
        bail("error: no output file.\n");

    auto& infile = res["infile"].as<std::string>();
    auto& outfile = res["outfile"].as<std::string>();

    SceneData model;
    if (!read_model(infile.c_str(), model))
        bail("error: failed to read model file.\n");

#if TEST_MODELIO
    if (!write_model("testobj.obj", nullptr, model))
        bail("error: failed to test write model file.\n");
#endif

#if TEST_SCENE
    // These numbers mostly from blender
    model.C.eye = { 8.4585f, -2.5662f, 10.108f };
    model.C.focal_len = 5;
    model.C.width = 3.6;
    model.C.height = model.C.width * (240. / 320.); // 800x600 render
    model.C.u = { 1, 1, 0 };
    model.C.v = { -1, 1, std::sqrtf(2.f) };
    model.C.w = { 1, -1, std::sqrtf(2.f) };

    light l1, l2;
    l1.pos = { 3.6746, 2.0055, 3.1325 };
    l1.rgb = { 0, 0, 1 }; // blue
    l2.pos = { 1.5699, 0.87056, 3.1325 };   
    l2.rgb = { 1, 1, 0 }; // yellow

    model.L.push_back(l1);
    model.L.push_back(l2);
#endif

    BVTree bvh(model);

    std::ofstream outf(outfile, std::ios::binary);
    if (!outf)
        bail("error: could not open output file.\n");

    auto nsmodel = model.nserial();
    auto nserial = nsmodel + bvh.nserial();

    byte* outbuf = new byte[nserial];
    model.serialize(outbuf);
    bvh.serialize(outbuf + nsmodel);

    outf.write((const char *)outbuf, nserial);
    outf.close();

    delete[] outbuf;

    return 0;
}