#define DO_TEST 0

#include "defs.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdio>

#include "cxxopts/cxxopts.hpp"

int main(int argc, char** argv)
{
    cxxopts::Options opts("host", "Host-side of FPGA raytracer.");
    opts.add_options()
        ("h,help", "Show usage.")
        ("f,file", "Input model file (.obj).", cxxopts::value<std::string>(), "<file>");

    auto res = opts.parse(argc, argv);
    if (res["help"].as<bool>())
    {
        std::cout << opts.help() << std::endl;
        return EXIT_SUCCESS;
    }
    if (res["file"].count() == 0)
    {
        std::cerr << "error: no input file.\n";
        return EXIT_FAILURE;
    }

    auto model_path = res["file"].as<std::string>();

    ModelData model;
    if (!read_model(model_path.c_str(), model))
    {
        std::cerr << "error: failed to read model file\n";
        return EXIT_FAILURE;
    }
#if DO_TEST
    if (!write_model("testobj.obj", nullptr, model))
    {
        std::cerr << "error: failed to test write model file\n";
        return EXIT_FAILURE;
    }
#endif

    auto nserial = model.nserial();
    byte* serbuf = new byte[nserial];
    model.serialize(serbuf);

    std::ofstream outf("serfile.bin", std::ios::binary);
    if (!outf)
    {
        std::cerr << "error: could not open output serfile";
        return EXIT_FAILURE;
    }

    outf.write((const char *)serbuf, nserial);
    outf.close();

    delete[] serbuf;

    BVTree tree(model);
    nserial = tree.nserial();
    byte* bvserbuf = new byte[nserial];
    tree.serialize(bvserbuf);
    
    std::ofstream outbf("bvserfile.bin", std::ios::binary);
    if (!outf)
    {
        std::cerr << "error: could not open output bvserfile";
        return EXIT_FAILURE;
    }

    outbf.write((const char*)bvserbuf, nserial);
    outbf.close();

    delete[] bvserbuf;

    return 0;
}