#include "KlemsLoader.h"
#include "Logger.h"
#include "serialization/FileSerializer.h"

#include <sstream>

#include <pugixml.hpp>

namespace IG {
static inline void assignSpecification(KlemsComponent& component, KlemsComponentSpecification& spec)
{
    // IG_LOG(L_INFO) << "MinPSA: " << component.computeMinimumProjectedSolidAngle() << " MaxHemiS: " << component.computeMaxHemisphericalScattering() << " Diff: " << component.extractDiffuse() << std::endl;
    spec.total       = component.computeTotal();
    spec.theta_count = { component.row()->thetaCount(), component.column()->thetaCount() };
    spec.entry_count = { component.row()->entryCount(), component.column()->entryCount() };
}

bool KlemsLoader::check(const Path& in_xml)
{
    // Read Radiance based klems BSDF xml document
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(in_xml.c_str());
    if (!result)
        return false;

    const auto layer = doc.child("WindowElement").child("Optical").child("Layer");
    if (!layer)
        return false;

    // Extract data definition and therefor also angle basis
    const auto datadefinition = layer.child("DataDefinition");
    if (!datadefinition)
        return false;

    std::string type = datadefinition.child_value("IncidentDataStructure");
    return type == "Rows" || type == "Columns";
}

bool KlemsLoader::load(const Path& in_xml, Klems& klems)
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
    bool rowBased    = type == "Rows";
    if (!rowBased && type != "Columns") {
        IG_LOG(L_ERROR) << "Could not parse " << in_xml << ": Expected IncidentDataStructure of 'Columns' or 'Rows' but got '" << type << "' instead" << std::endl;
        return false;
    }

    std::unordered_map<std::string, std::shared_ptr<KlemsBasis>> allbasis;
    for (const auto& anglebasis : datadefinition.children("AngleBasis")) {
        const char* name = anglebasis.child_value("AngleBasisName");
        if (!name) {
            IG_LOG(L_ERROR) << "Could not parse " << in_xml << ": AngleBasis has no name" << std::endl;
            return false;
        }

        // Extract basis information
        std::shared_ptr<KlemsBasis> fullbasis = std::make_shared<KlemsBasis>();
        for (const auto& child : anglebasis.children("AngleBasisBlock")) {
            KlemsThetaBasis basis{};

            const auto bounds = child.child("ThetaBounds");
            basis.LowerTheta  = Deg2Rad * bounds.child("LowerTheta").text().as_float(0);
            basis.UpperTheta  = Deg2Rad * bounds.child("UpperTheta").text().as_float(0);
            basis.PhiCount    = child.child("nPhis").text().as_uint(0);

            const float solidA  = std::cos(basis.LowerTheta);
            const float solidB  = std::cos(basis.UpperTheta);
            basis.PhiSolidAngle = basis.PhiCount > 0 ? (Pi * (solidA * solidA - solidB * solidB) / basis.PhiCount) : 0.0f;

            const auto theta = child.child("Theta");
            if (theta)
                basis.CenterTheta = Deg2Rad * theta.text().as_float(0);
            else
                basis.CenterTheta = (basis.UpperTheta + basis.LowerTheta) / 2;

            if (!basis.isValid()) {
                IG_LOG(L_ERROR) << "Could not parse " << in_xml << ": Invalid AngleBasisBlock given" << std::endl;
                return false;
            }
            fullbasis->addBasis(basis);
        }

        fullbasis->setup();
        allbasis[name] = std::move(fullbasis);
    }

    if (allbasis.empty()) {
        IG_LOG(L_ERROR) << "Could not parse " << in_xml << ": No basis given" << std::endl;
        return false;
    }

    std::shared_ptr<KlemsComponent> reflectionFront;
    std::shared_ptr<KlemsComponent> transmissionFront;
    std::shared_ptr<KlemsComponent> reflectionBack;
    std::shared_ptr<KlemsComponent> transmissionBack;
    // Extract wavelengths
    for (const auto& data : layer.children("WavelengthData")) {
        const char* wvl_type = data.child_value("Wavelength");
        if (!wvl_type || strcmp(wvl_type, "Visible") != 0) // Skip entries for non-visible wavelengths
            continue;

        const auto block = data.child("WavelengthDataBlock");
        if (!block) {
            IG_LOG(L_ERROR) << "Could not parse " << in_xml << ": No WavelengthDataBlock given" << std::endl;
            return false;
        }

        // Connect angle basis
        const char* columnBasisName = block.child_value("ColumnAngleBasis"); // Incoming direction
        const char* rowBasisName    = block.child_value("RowAngleBasis");    // Outgoing direction
        if (!columnBasisName || !rowBasisName) {
            IG_LOG(L_ERROR) << "Could not parse " << in_xml << ": WavelengthDataBlock has no column or row basis given" << std::endl;
            return false;
        }

        std::shared_ptr<KlemsBasis> columnBasis;
        std::shared_ptr<KlemsBasis> rowBasis;
        if (allbasis.count(columnBasisName))
            columnBasis = allbasis.at(columnBasisName);
        if (allbasis.count(rowBasisName))
            rowBasis = allbasis.at(rowBasisName);
        if (!columnBasis || !rowBasis) {
            IG_LOG(L_ERROR) << "Could not parse " << in_xml << ": WavelengthDataBlock has no known column or row basis given" << std::endl;
            return false;
        }

        // Setup component
        std::shared_ptr<KlemsComponent> component = std::make_shared<KlemsComponent>(rowBasis, columnBasis);

        // Load with the permutation in mind
        const auto& rowPerm = rowBasis->permutation();
        const auto& colPerm = columnBasis->permutation();

        // Parse list of floats
        const char* scat_str = block.child_value("ScatteringData");
        char* end            = nullptr;
        Eigen::Index ind     = 0;
        bool did_warn_sign   = false;
        bool did_warn_fin    = false;
        while (ind < component->matrix().size() && *scat_str) {
            float value = std::strtof(scat_str, &end);
            if (scat_str == end && value == 0) {
                scat_str = scat_str + 1; // Skip entry
                continue;
            }

            if (std::signbit(value)) {
                value = 0;
                if (!did_warn_sign) {
                    IG_LOG(L_WARNING) << "Data contains negative values in " << in_xml << ": Using absolute value instead" << std::endl;
                    did_warn_sign = true;
                }
            }

            if (!std::isfinite(value)) {
                value = 0;
                if (!did_warn_fin) {
                    IG_LOG(L_WARNING) << "Data contains non-finite values in " << in_xml << ": Replacing them with 0" << std::endl;
                    did_warn_fin = true;
                }
            }

            Eigen::Index row, col;
            if (rowBased) {
                row = ind % columnBasis->entryCount();
                col = ind / columnBasis->entryCount();
            } else {
                row = ind / columnBasis->entryCount();
                col = ind % columnBasis->entryCount();
            }

            component->matrix()(rowPerm.at(row), colPerm.at(col)) = value;
            ++ind;
            if (scat_str == end)
                break;
            scat_str = end;
        }

        if (ind != component->matrix().size()) {
            IG_LOG(L_ERROR) << "Could not parse " << in_xml << ": Given scattered data is not of length " << component->matrix().size() << std::endl;
            return false;
        }

        component->buildCDF_Colwise();

        // Select correct component
        // The window definition flips the front & back
        const std::string direction = block.child_value("WavelengthDataDirection");
        if (direction == "Transmission Front")
            transmissionBack = component;
        else if (direction == "Scattering Back" || direction == "Reflection Back")
            reflectionFront = component;
        else if (direction == "Transmission Back")
            transmissionFront = component;
        else
            reflectionBack = component;
    }

    // If reflection components are not given, make them black
    // See doc/notes/BSDFdirections.txt in Radiance for more information
    if (!reflectionBack) {
        auto basis     = allbasis.begin()->second;
        reflectionBack = std::make_shared<KlemsComponent>(basis, basis);
    }
    if (!reflectionFront) {
        auto basis      = allbasis.begin()->second;
        reflectionFront = std::make_shared<KlemsComponent>(basis, basis);
    }

    // Make sure both transmission parts are equal if not specified otherwise
    if (!transmissionBack || (transmissionFront && transmissionBack->computeTotal() <= FltEps))
        transmissionBack = transmissionFront;
    if (!transmissionFront || (transmissionBack && transmissionFront->computeTotal() <= FltEps))
        transmissionFront = transmissionBack;

    // TODO: Weird behaviour in Radiance: Radiance expects reciprocity and keeps both transmission sides the same
    // Maybe we should do the same?

    if (!transmissionFront && !transmissionBack) {
        IG_LOG(L_ERROR) << "While parsing " << in_xml << ": No transmission data found" << std::endl;
        return false;
    }

    if (!reflectionFront || !reflectionBack || !transmissionFront || !transmissionBack) {
        IG_LOG(L_ERROR) << "Could not parse " << in_xml << ": Incomplete data found" << std::endl;
        return false;
    }

    klems.front_reflection   = std::move(reflectionFront);
    klems.back_reflection    = std::move(reflectionBack);
    klems.front_transmission = std::move(transmissionFront);
    klems.back_transmission  = std::move(transmissionBack);

    return true;
}

bool KlemsLoader::prepare(const Path& in_xml, const Path& out_data, KlemsSpecification& spec)
{
    Klems klems;
    if (!load(in_xml, klems))
        return false;

    assignSpecification(*klems.front_reflection, spec.front_reflection);
    assignSpecification(*klems.front_transmission, spec.front_transmission);
    assignSpecification(*klems.back_reflection, spec.back_reflection);
    assignSpecification(*klems.back_transmission, spec.back_transmission);

    if (!out_data.empty()) {
        // Note: Order matters!
        FileSerializer serializer(out_data, false);
        klems.front_reflection->write(serializer);
        klems.front_transmission->write(serializer);
        klems.back_reflection->write(serializer);
        klems.back_transmission->write(serializer);
    }

    return true;
}

} // namespace IG