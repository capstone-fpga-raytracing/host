
#include "defs.hpp"
#include <iostream>
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
    if (!read_wavefront_model(model_path.c_str(), nullptr, model))
    {
        std::cerr << "error: failed to read model file\n";
        return EXIT_FAILURE;
    }

    byte* serbuf = new byte[model.nsbytes()];
    model.serialize(serbuf);

    //BVTree tree(model);
    // todo: serialize bvtree 

    return 0;
}