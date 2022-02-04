#include "TensorTreeLoader.h"
#include "Logger.h"

#include <fstream>
#include <sstream>

#include <pugixml.hpp>

namespace IG {
// TODO
class TensorTreeComponent {
public:
    inline TensorTreeComponent()
    {
    }

    inline void write(std::ostream& os)
    {
    }
};

bool TensorTreeLoader::prepare(const std::filesystem::path& in_xml, const std::filesystem::path& out_data)
{
    // Read Radiance based klems BSDF xml document
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(in_xml.c_str());
    if (!result) {
        IG_LOG(L_ERROR) << "Could not load file " << in_xml << ": " << result.description() << std::endl;
        return false;
    }

    const auto layer = doc.child("WindowElement").child("Optical").child("Layer");
    if (!layer) {
        IG_LOG(L_ERROR) << "Could not parse " << in_xml << ": No Layer tag" << std::endl;
        return false;
    }

    // Extract data definition and therefor also angle basis
    const auto datadefinition = layer.child("DataDefinition");
    if (!datadefinition) {
        IG_LOG(L_ERROR) << "Could not parse " << in_xml << ": No DataDefinition tag" << std::endl;
        return false;
    }

    std::string type = datadefinition.child_value("IncidentDataStructure");
    bool dim4        = type == "TensorTree4";
    if (!dim4 && type != "TensorTree3") {
        IG_LOG(L_ERROR) << "Could not parse " << in_xml << ": Expected IncidentDataStructure of 'TensorTree4' or 'TensorTree3' but got '" << type << "' instead" << std::endl;
        return false;
    }

    std::shared_ptr<TensorTreeComponent> reflectionFront;
    std::shared_ptr<TensorTreeComponent> transmissionFront;
    std::shared_ptr<TensorTreeComponent> reflectionBack;
    std::shared_ptr<TensorTreeComponent> transmissionBack;
    // Extract wavelengths
    for (const auto& data : layer.children("WavelengthData")) {
        const char* type = data.child_value("Wavelength");
        if (!type || strcmp(type, "Visible") != 0) // Skip entries for non-visible wavelengths
            continue;

        const auto block = data.child("WavelengthDataBlock");
        if (!block) {
            IG_LOG(L_ERROR) << "Could not parse " << in_xml << ": No WavelengthDataBlock given" << std::endl;
            return false;
        }

        // Connect angle basis
        std::string basisName = block.child_value("AngleBasis");
        if (basisName.empty()) {
            IG_LOG(L_ERROR) << "Could not parse " << in_xml << ": WavelengthDataBlock has no angle basis given" << std::endl;
            return false;
        }

        if (basisName != "LBNL/Shirley-Chiu") {
            IG_LOG(L_ERROR) << "Could not parse " << in_xml << ": AngleBasis is not 'LBNL/Shirley-Chiu'" << std::endl;
            return false;
        }

        // Setup component
        std::shared_ptr<TensorTreeComponent> component = std::make_shared<TensorTreeComponent>();

        // Parse list of floats
        const char* scat_str = block.child_value("ScatteringData");
        char* end            = nullptr;
        Eigen::Index ind     = 0;
        // TODO
        // while (ind < component->matrix().size() && *scat_str) {
        //     const float value = std::strtof(scat_str, &end);
        //     if (scat_str == end && value == 0) {
        //         scat_str = scat_str + 1; // Skip entry
        //         continue;
        //     }
        //     const Eigen::Index row = ind / columnBasis->entryCount(); //Outgoing direction
        //     const Eigen::Index col = ind % columnBasis->entryCount(); //Incoming direction

        //     component->matrix()(row, col) = value;
        //     ++ind;
        //     if (scat_str == end)
        //         break;
        //     scat_str = end;
        // }

        // Select correct component
        const std::string direction = block.child_value("WavelengthDataDirection");
        if (direction == "Transmission Front")
            transmissionFront = component;
        else if (direction == "Scattering Back")
            reflectionBack = component;
        else if (direction == "Transmission Back")
            transmissionBack = component;
        else
            reflectionFront = component;
    }

    // FIXME: This is not the standard radiance uses. It should be handled as "black"
    if (!reflectionBack)
        reflectionBack = reflectionFront;
    if (!reflectionFront)
        reflectionFront = reflectionBack;

    // Make sure both transmission parts are equal if not specified otherwise
    if (!transmissionBack)
        transmissionBack = transmissionFront;
    if (!transmissionFront)
        transmissionFront = transmissionBack;

    if (!reflectionFront && !reflectionBack && !transmissionFront && !transmissionBack) {
        IG_LOG(L_ERROR) << "Could not parse " << in_xml << ": No valid data found" << std::endl;
        return false;
    }

    if (!transmissionFront && !transmissionBack) {
        IG_LOG(L_ERROR) << "While parsing " << in_xml << ": No transmission data found" << std::endl;
        return false;
    }

    if (!reflectionFront || !reflectionBack || !transmissionFront || !transmissionBack) {
        IG_LOG(L_ERROR) << "Could not parse " << in_xml << ": Incomplete data found" << std::endl;
        return false;
    }

    std::ofstream stream(out_data, std::ios::binary);
    reflectionFront->write(stream);
    transmissionFront->write(stream);
    reflectionBack->write(stream);
    transmissionBack->write(stream);

    return true;
}

} // namespace IG