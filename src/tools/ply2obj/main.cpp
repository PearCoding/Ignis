#include <filesystem>
#include <iostream>

#include "mesh/ObjFile.h"
#include "mesh/PlyFile.h"

using namespace IG;
int main(int argc, char** argv)
{
    if (argc != 2 && argc != 3) {
        std::cout << "Expected ply2obj INPUT (OUTPUT)" << std::endl;
        return EXIT_FAILURE;
    }

    const std::string input  = argv[1];
    const std::string output = argc == 3 ? argv[2] : Path(input).replace_extension(".obj").generic_string();

    try {
        // Input
        const auto mesh = ply::load(input);
        if (mesh.vertices.size() == 0)
            return EXIT_FAILURE;

        // Output
        if (!obj::save(mesh, output))
            return EXIT_FAILURE;
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}