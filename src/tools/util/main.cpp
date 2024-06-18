
#include "Logger.h"
#include "UtilOptions.h"
#include "config/Build.h"

#include "ImageUtils.h"
#include "MeshUtils.h"
#include "MtsUtils.h"

using namespace IG;

int main(int argc, char** argv)
{
    UtilOptions cmd(argc, argv, "Ignis Utility Tool");
    if (cmd.ShouldExit)
        return EXIT_SUCCESS;

    if (!cmd.Quiet && !cmd.NoLogo)
        std::cout << Build::getCopyrightString() << std::endl;

    try {
        switch (cmd.Op) {
        case Operation::Info:
            dump_metadata(cmd.InputFile);
            break;
        case Operation::Convert: {
            switch (cmd.ConvertFormat) {
            case ConvertOption::BMP:
                convert_stb(cmd.InputFile, cmd.OutputFile, ConvertToStdImage::BMP, cmd.ToneMapExposure, cmd.ToneMapOffset);
                break;
            case ConvertOption::JPG:
                convert_stb(cmd.InputFile, cmd.OutputFile, ConvertToStdImage::JPG, cmd.ToneMapExposure, cmd.ToneMapOffset, cmd.JPGQuality);
                break;
            case ConvertOption::PNG:
                convert_stb(cmd.InputFile, cmd.OutputFile, ConvertToStdImage::PNG, cmd.ToneMapExposure, cmd.ToneMapOffset);
                break;
            case ConvertOption::TGA:
                convert_stb(cmd.InputFile, cmd.OutputFile, ConvertToStdImage::TGA, cmd.ToneMapExposure, cmd.ToneMapOffset);
                break;

            case ConvertOption::HDR:
                convert_hdr(cmd.InputFile, cmd.OutputFile, cmd.ToneMapExposure, cmd.ToneMapOffset);
                break;
            case ConvertOption::EXR:
                convert_exr(cmd.InputFile, cmd.OutputFile, cmd.ToneMapExposure, cmd.ToneMapOffset);
                break;

            case ConvertOption::OBJ:
                convert_ply_obj(cmd.InputFile, cmd.OutputFile);
                break;
            case ConvertOption::PLY:
                convert_obj_ply(cmd.InputFile, cmd.OutputFile);
                break;

#ifdef IG_WITH_CONVERTER_MITSUBA
            case ConvertOption::IG:
                convert_from_mts(cmd.InputFile, cmd.OutputFile, cmd.MtsLookupDirs, cmd.MtsDefs, true /*TODO*/, cmd.Quiet);
                break;
#endif

            default:
                std::cerr << "Invalid convert format" << std::endl;
                break;
            }

        } break;
        default:
            std::cerr << "Invalid operation" << std::endl;
            return EXIT_FAILURE;
        }

    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}