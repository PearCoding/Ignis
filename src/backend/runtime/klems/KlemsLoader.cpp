#include "KlemsLoader.h"
#include "Logger.h"

#include <fstream>
#include <sstream>

#include <pugixml.hpp>

namespace IG {
struct KlemsThetaBasis {
    float CenterTheta;
    float LowerTheta;
    float UpperTheta;
    uint32_t PhiCount;

    inline bool isValid() const
    {
        return PhiCount > 0 && LowerTheta < UpperTheta;
    }
};

class KlemsBasis {
public:
    using MultiIndex = std::pair<int, int>;

    KlemsBasis() = default;

    inline void addBasis(const KlemsThetaBasis& basis)
    {
        mThetaBasis.push_back(basis);
    }

    inline void setup()
    {
        std::sort(mThetaBasis.begin(), mThetaBasis.end(),
                  [](const KlemsThetaBasis& a, const KlemsThetaBasis& b) { return a.UpperTheta < b.UpperTheta; });

        mThetaLinearOffset.resize(mThetaBasis.size());
        uint32_t off = 0;
        for (size_t i = 0; i < mThetaBasis.size(); ++i) {
            mThetaLinearOffset[i] = off;
            off += mThetaBasis[i].PhiCount;
        }

        mEntryCount = off;
    }

    inline uint32_t entryCount() const { return mEntryCount; }

    inline void write(std::ostream& os)
    {
        uint32_t theta_count = mThetaBasis.size();
        std::sort(mThetaBasis.begin(), mThetaBasis.end(),
                  [](const KlemsThetaBasis& a, const KlemsThetaBasis& b) { return a.UpperTheta < b.UpperTheta; });

        std::vector<uint32_t> offsets(mThetaBasis.size());
        uint32_t off = 0;
        for (size_t i = 0; i < mThetaBasis.size(); ++i) {
            offsets[i] = off;
            off += mThetaBasis[i].PhiCount;
        }

        os.write(reinterpret_cast<const char*>(&theta_count), sizeof(uint32_t));
        os.write(reinterpret_cast<const char*>(&off), sizeof(uint32_t));
        os.write(reinterpret_cast<const char*>(&off), sizeof(uint32_t)); // Pad
        os.write(reinterpret_cast<const char*>(&off), sizeof(uint32_t)); // Pad
        for (auto& basis : mThetaBasis) {
            os.write(reinterpret_cast<const char*>(&basis.CenterTheta), sizeof(float));
            os.write(reinterpret_cast<const char*>(&basis.LowerTheta), sizeof(float));
            os.write(reinterpret_cast<const char*>(&basis.UpperTheta), sizeof(float));
            os.write(reinterpret_cast<const char*>(&basis.PhiCount), sizeof(uint32_t));
        }

        os.write(reinterpret_cast<const char*>(offsets.data()), offsets.size() * sizeof(uint32_t));
    }

    inline const std::vector<KlemsThetaBasis>& thetaBasis() const { return mThetaBasis; }
    inline const std::vector<uint32_t>& thetaLinearOffset() const { return mThetaLinearOffset; }

private:
    std::vector<KlemsThetaBasis> mThetaBasis;
    std::vector<uint32_t> mThetaLinearOffset;
    uint32_t mEntryCount = 0;
};

using KlemsMatrix = Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic>;
class KlemsComponent {
public:
    inline KlemsComponent(const std::shared_ptr<KlemsBasis>& row, const std::shared_ptr<KlemsBasis>& column)
        : mRowBasis(row)
        , mColumnBasis(column)
        , mMatrix(row->entryCount(), column->entryCount())
    {
    }

    inline KlemsMatrix& matrix() { return mMatrix; }
    inline size_t size() const { return mMatrix.size(); }

    inline void makeBlack()
    {
        mMatrix.fill(0);
    }

    inline void transpose()
    {
        mMatrix.transposeInPlace();
        std::swap(mRowBasis, mColumnBasis);
        buildCDF();
    }

    inline void buildCDF()
    {
        //TODO
    }

    inline std::shared_ptr<KlemsBasis> row() const { return mRowBasis; }
    inline std::shared_ptr<KlemsBasis> column() const { return mColumnBasis; }

    inline void write(std::ostream& os)
    {
        mRowBasis->write(os);
        mColumnBasis->write(os);

        os.write(reinterpret_cast<const char*>(mMatrix.data()), sizeof(float) * mMatrix.size());
    }

private:
    std::shared_ptr<KlemsBasis> mRowBasis;
    std::shared_ptr<KlemsBasis> mColumnBasis;
    KlemsMatrix mMatrix;
};

bool KlemsLoader::prepare(const std::filesystem::path& in_xml, const std::filesystem::path& out_data)
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
            KlemsThetaBasis basis;

            const auto bounds = child.child("ThetaBounds");
            basis.LowerTheta  = Deg2Rad * bounds.child("LowerTheta").text().as_float(0);
            basis.UpperTheta  = Deg2Rad * bounds.child("UpperTheta").text().as_float(0);
            basis.PhiCount    = child.child("nPhis").text().as_uint(0);

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
        const char* type = data.child_value("Wavelength");
        if (!type || strcmp(type, "Visible") != 0) // Skip entries for non-visible wavelengths
            continue;

        const auto block = data.child("WavelengthDataBlock");
        if (!block) {
            IG_LOG(L_ERROR) << "Could not parse " << in_xml << ": No WavelengthDataBlock given" << std::endl;
            return false;
        }

        // Connect angle basis
        const char* columnBasisName = block.child_value("ColumnAngleBasis");
        const char* rowBasisName    = block.child_value("RowAngleBasis");
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

        // Parse list of floats
        const char* scat_str = block.child_value("ScatteringData");
        char* end            = nullptr;
        Eigen::Index ind     = 0;
        while (ind < component->matrix().size() && *scat_str) {
            const float value = std::strtof(scat_str, &end);
            if (scat_str == end && value == 0) {
                scat_str = scat_str + 1; // Skip entry
                continue;
            }
            const Eigen::Index row = ind / columnBasis->entryCount(); //Outgoing direction
            const Eigen::Index col = ind % columnBasis->entryCount(); //Incoming direction

            component->matrix()(row, col) = value;
            ++ind;
            if (scat_str == end)
                break;
            scat_str = end;
        }

        if (ind != component->matrix().size()) {
            IG_LOG(L_ERROR) << "Could not parse " << in_xml << ": Given scattered data is not of length " << component->matrix().size() << std::endl;
            return false;
        }

        if (rowBased)
            component->transpose();

        component->buildCDF();

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

    // If reflection components are not given, make them black
    // See docs/notes/BSDFdirections.txt in Radiance for more information
    if (!reflectionBack) {
        auto basis     = allbasis.begin()->second;
        reflectionBack = std::make_shared<KlemsComponent>(basis, basis);
        reflectionBack->makeBlack();
    }
    if (!reflectionFront) {
        auto basis     = allbasis.begin()->second;
        reflectionFront = std::make_shared<KlemsComponent>(basis, basis);
        reflectionFront->makeBlack();
    }

    // Make sure both transmission parts are equal if not specified otherwise
    if (!transmissionBack)
        transmissionBack = transmissionFront;
    if (!transmissionFront)
        transmissionFront = transmissionBack;

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