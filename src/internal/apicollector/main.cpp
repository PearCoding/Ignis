#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

static inline void replace_all(std::string& data, const std::string& from, const std::string& to)
{
    size_t pos = data.find(from);

    // Repeat till end is reached
    while (pos != std::string::npos) {
        data.replace(pos, from.size(), to);
        pos = data.find(from, pos + to.size());
    }
}

static inline std::string get_name(const std::filesystem::path& path)
{
    const std::string filename  = path.stem().generic_u8string();
    const std::string directory = path.parent_path().stem().generic_u8string();
    return directory + "_" + filename;
}

/// Simple tool to collect all artic related source code and embed it into a c file for on demand loading
/// without mangling with files in the given host operating system.
int main(int argc, char** argv)
{
    std::vector<std::filesystem::path> inputs;
    std::filesystem::path output;

    if (argc < 3) {
        std::cerr << "Not enough arguments given..." << std::endl;
        return EXIT_FAILURE;
    }

    // Get parameters
    for (int i = 1; i < argc - 1; ++i)
        inputs.emplace_back(argv[i]);

    output = argv[argc - 1];

    std::ofstream stream(output);
    stream << "// ig_api_collector generated API sources" << std::endl;

    // Read all files
    for (const auto& input : inputs) {
        const std::string name = get_name(input);

        stream << "static const char* const s_" << name << " =" << std::endl;

        std::ifstream infile(input.generic_u8string());
        std::string line;
        while (std::getline(infile, line)) {
            // Make sure its properly escaped
            replace_all(line, "\\", "\\\\");
            replace_all(line, "\"", "\\\"");
            replace_all(line, "\t", "  ");
            if (!line.empty() && line != "\n")
                stream << "\"" << line << "\\n\"" << std::endl;
        }

        stream << ";" << std::endl;
    }

    // Construct header
    stream << std::endl
           << "const char* ig_api[] = {" << std::endl;
    for (const auto& input : inputs) {
        const std::string name = get_name(input);
        stream << "  s_" << name << "," << std::endl;
    }
    stream << "  nullptr" << std::endl
           << "};" << std::endl;

    stream << std::endl
           << "const char* ig_api_paths[] = {" << std::endl;
    for (const auto& input : inputs) {
        const std::string filename  = input.filename().generic_u8string();
        const std::string directory = input.parent_path().stem().generic_u8string();
        stream << "  \"" << directory << "/" << filename << "\"," << std::endl;
    }
    stream << "  nullptr" << std::endl
           << "};" << std::endl;

    return EXIT_SUCCESS;
}
