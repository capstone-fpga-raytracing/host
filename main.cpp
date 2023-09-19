
#include "defs.hpp"
#include <iostream>
#include <sstream>


int main(int argc, char** argv)
{
    // round-trip test
    ModelGeom geom;
    if (!read_obj_model("sphere.obj", geom))
    {
        std::cerr << "read model failed";
        return EXIT_FAILURE;
    }        
    if (!write_obj_model("sphere_copy.obj", geom))
    {
        std::cerr << "write model failed";
        return EXIT_FAILURE;
    }

    return 0;
}