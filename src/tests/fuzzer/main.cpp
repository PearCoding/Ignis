#include <fstream>
#include <iostream>
#include <random>

#include "Logger.h"
#include "loader/Parser.h"

inline void func(const std::string& s)
{
    IG::SceneParser parser;
    auto scene = parser.loadFromString(s, {});
    IG_UNUSED(scene);
}

// Very basic fuzzer with no security requirements, checking if the application will crash or not
int main(int argc, char** argv)
{
    constexpr size_t MaxSize = 1024 * 1024 * 1024; // 1 Gb

    IG_LOGGER.setQuiet(true);

    if (argc != 2) {
        std::cout << "Not enough arguments given, expected: " << argv[0] << " ITER" << std::endl;
        return EXIT_FAILURE;
    }
    const size_t iterations = (size_t)std::stoi(argv[1]);

    // First test some standard inputs
    func({});
    std::cout << "." << std::flush;
    func("");
    std::cout << "." << std::flush;
    func("\0");
    std::cout << "." << std::flush;
    func("\n");
    std::cout << "." << std::flush;

    // Random tests
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> size_distrib(0, MaxSize);
    std::uniform_int_distribution<char> char_distrib(0, std::numeric_limits<char>::max());

    for (size_t i = 0; i < iterations; ++i) {
        size_t size = size_distrib(gen);

        std::string string;
        string.resize(size);

        for (auto& c : string)
            c = char_distrib(gen);

        func(string);
        std::cout << "." << std::flush;
    }

    // TODO: Random semantic tests (e.g., build a random "valid" json string)

    std::cout << std::endl
              << "Success" << std::endl;

    return EXIT_SUCCESS;
}
