#include <iostream>

#include "Target.h"

int main(int argc, char **argv)
{
    if (argc <= 1)
    {
        std::cerr << "Not enough arguments. Run with --help to get a list of options." << std::endl;
        return 1;
    }

    return 0;
}
