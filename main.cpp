
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