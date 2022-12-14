#include "KlemsLoader.h"
#include "Logger.h"
#include "serialization/FileSerializer.h"

#include <numeric>
#include <sstream>

#include <pugixml.hpp>

namespace IG {
struct KlemsThetaBasis {
    float CenterTheta;
    float LowerTheta;
    float UpperTheta;
    uint32 PhiCount;
    float PhiSolidAngle; // Per phi solid angle

    [[nodiscard]] inline bool isValid() const
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
        // First create a permutation vector
        // We need it later to sort the actual matrix
        std::vector<Eigen::Index> perm(mThetaBasis.size());
        std::iota(perm.begin(), perm.end(), 0);
        std::sort(perm.begin(), perm.end(),
                  [&](size_t a, size_t b) { return mThetaBasis[a].UpperTheta < mThetaBasis[b].UpperTheta; });

        // Sort the actual basis
        std::sort(mThetaBasis.begin(), mThetaBasis.end(),
                  [](const KlemsThetaBasis& a, const KlemsThetaBasis& b) { return a.UpperTheta < b.UpperTheta; });

        // Construct linear offsets
        mThetaLinearOffset.resize(mThetaBasis.size());
        uint32 off = 0;
        for (size_t i = 0; i < mThetaBasis.size(); ++i) {
            mThetaLinearOffset[i] = off;
            off += mThetaBasis[i].PhiCount;
        }

        mEntryCount = off;

        // Enlarge for faster access
        mPermutation.resize(mEntryCount);
        size_t k = 0;
        for (size_t i = 0; i < mThetaBasis.size(); ++i) {
            size_t ri    = perm[i];
            size_t count = mThetaBasis[ri].PhiCount;
            size_t l     = mThetaLinearOffset[ri];
            for (size_t j = 0; j < count; ++j)
                mPermutation[k++] = l + j;
        }
    }

    [[nodiscard]] inline uint32 entryCount() const { return mEntryCount; }
    [[nodiscard]] inline size_t thetaCount() const { return mThetaBasis.size(); }

    inline void write(Serializer& os)
    {
        for (auto& basis : mThetaBasis) {
            os.write(basis.CenterTheta);
            os.write(basis.LowerTheta);
            os.write(basis.UpperTheta);
            os.write(basis.PhiCount);
        }

        os.write(mThetaLinearOffset, true);
    }

    [[nodiscard]] inline const std::vector<Eigen::Index>& permutation() const { return mPermutation; }
    [[nodiscard]] inline const std::vector<KlemsThetaBasis>& thetaBasis() const { return mThetaBasis; }
    [[nodiscard]] inline const std::vector<uint32>& thetaLinearOffset() const { return mThetaLinearOffset; }

private:
    std::vector<Eigen::Index> mPermutation;
    std::vector<KlemsThetaBasis> mThetaBasis;
    std::vector<uint32> mThetaLinearOffset;
    uint32 mEntryCount = 0;
};

using KlemsMatrix = Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic>;
class KlemsComponent {
public:
    inline KlemsComponent(const std::shared_ptr<KlemsBasis>& row, const std::shared_ptr<KlemsBasis>& column)
        : mRowBasis(row)
        , mColumnBasis(column)
        , mMatrix(row->entryCount(), column->entryCount())
        , mCDFMatrix(row->entryCount(), column->entryCount())
    {
        makeBlack();
    }

    [[nodiscard]] inline KlemsMatrix& matrix() { return mMatrix; }
    [[nodiscard]] inline size_t size() const { return mMatrix.size(); }
    [[nodiscard]] inline KlemsMatrix& cdfmatrix() { return mCDFMatrix; }

    inline void makeBlack()
    {
        mMatrix.fill(0);
    }

    inline void buildCDF_Rowwise()
    {
        const auto& thetas = mColumnBasis->thetaBasis();
        for (Eigen::Index row = 0; row < mMatrix.rows(); ++row) {
            Eigen::Index col = 0;
            for (size_t i = 0; i < thetas.size(); ++i) {
                const float solid  = thetas.at(i).PhiSolidAngle;
                const uint32 count = thetas.at(i).PhiCount;
                for (size_t j = 0; j < count; ++j) {
                    const float value    = mMatrix(row, col);
                    mCDFMatrix(row, col) = (col != 0 ? mCDFMatrix(row, col - 1) : 0) + value * solid;
                    ++col;
                }
            }

            IG_ASSERT(col == mMatrix.cols(), "Expected valid cdf loop generation");

            float mag = mCDFMatrix(row, col - 1); // Last entry
            if (mag <= FltEps)
                mag = 1;

            const float norm = 1 / mag;
            for (col = 0; col < mMatrix.cols(); ++col)
                mCDFMatrix(row, col) *= norm;

            mCDFMatrix(row, col - 1) = 1; // Force last entry to 1 for precision
        }
    }

    inline void buildCDF_Colwise()
    {
        const auto& thetas = mRowBasis->thetaBasis();
        for (Eigen::Index col = 0; col < mMatrix.cols(); ++col) {
            Eigen::Index row = 0;
            for (size_t i = 0; i < thetas.size(); ++i) {
                const float solid  = thetas.at(i).PhiSolidAngle;
                const uint32 count = thetas.at(i).PhiCount;
                for (size_t j = 0; j < count; ++j) {
                    const float value    = mMatrix(row, col);
                    mCDFMatrix(row, col) = (row != 0 ? mCDFMatrix(row - 1, col) : 0) + value * solid;
                    ++row;
                }
            }

            IG_ASSERT(row == mMatrix.rows(), "Expected valid cdf loop generation");

            float mag = mCDFMatrix(row - 1, col); // Last entry
            if (mag <= FltEps)
                mag = 1;

            const float norm = 1 / mag;
            for (row = 0; row < mMatrix.rows(); ++row)
                mCDFMatrix(row, col) *= norm;

            mCDFMatrix(row - 1, col) = 1; // Force last entry to 1 for precision
        }

        mCDFMatrix.transposeInPlace(); // For better memory alignment, we transpose the matrix
    }

    [[nodiscard]] inline std::shared_ptr<KlemsBasis> row() const { return mRowBasis; }
    [[nodiscard]] inline std::shared_ptr<KlemsBasis> column() const { return mColumnBasis; }
    [[nodiscard]] inline float computeTotal() const
    {
        float sum = 0;

        const auto& rowThetas = mRowBasis->thetaBasis();
        const auto& colThetas = mColumnBasis->thetaBasis();

        Eigen::Index row = 0;
        for (size_t i = 0; i < rowThetas.size(); ++i) {
            const float rowScale     = rowThetas.at(i).PhiSolidAngle;
            const uint32 rowPhiCount = rowThetas.at(i).PhiCount;
            for (size_t ip = 0; ip < rowPhiCount; ++ip) {
                Eigen::Index col = 0;
                for (size_t j = 0; j < colThetas.size(); ++j) {
                    const float colScale     = colThetas.at(j).PhiSolidAngle;
                    const uint32 colPhiCount = colThetas.at(j).PhiCount;
                    for (size_t jp = 0; jp < colPhiCount; ++jp) {
                        IG_ASSERT(row < mMatrix.rows() && col < mMatrix.cols(), "Expected klems index to be in boundary of matrix");
                        const float value = mMatrix(row, col);
                        IG_ASSERT(std::isfinite(value), "Expected Klems matrix to contain finite numbers only");

                        sum += value * rowScale * colScale;
                        ++col;
                    }
                }
                ++row;
            }
        }

        IG_ASSERT(std::isfinite(sum), "Expected Klems total computation to return finite numbers");
        return sum;
    }

    [[nodiscard]] inline float computeMinimumProjectedSolidAngle() const
    {
        float minValue = Pi;
        for (const auto& thetaBase : mRowBasis->thetaBasis())
            minValue = std::min(minValue, thetaBase.PhiSolidAngle);

        if (mRowBasis != mColumnBasis) {
            for (const auto& thetaBase : mColumnBasis->thetaBasis())
                minValue = std::min(minValue, thetaBase.PhiSolidAngle);
        }

        return minValue;
    }

    [[nodiscard]] inline float computeMaxHemisphericalScattering() const
    {
        const auto& colThetas = mColumnBasis->thetaBasis();

        float maxHemi = 0;
        for (Eigen::Index row = 0; row < mMatrix.rows(); ++row) {
            float sum        = 0;
            Eigen::Index col = 0;
            for (size_t j = 0; j < colThetas.size(); ++j) {
                const float colScale     = colThetas.at(j).PhiSolidAngle;
                const uint32 colPhiCount = colThetas.at(j).PhiCount;
                for (size_t jp = 0; jp < colPhiCount; ++jp) {
                    const float value = mMatrix(row, col);

                    sum += value * colScale;
                    ++col;
                }
            }
            maxHemi = std::max(maxHemi, sum);
        }

        return maxHemi;
    }

    [[nodiscard]] inline float extractDiffuse()
    {
        const float minValue = mMatrix.minCoeff();

        if (minValue <= 0.01f)
            return 0; // Not worth extracting diffuse part

        mMatrix.array() -= minValue;

        return Pi * minValue;
    }

    inline void write(Serializer& os)
    {
        constexpr size_t DefaultAlignment = 4 * sizeof(float);

        mRowBasis->write(os);
        os.writeAlignmentPad(DefaultAlignment);
        mColumnBasis->write(os);
        os.writeAlignmentPad(DefaultAlignment);

        os.write(mMatrix);
        os.write(mCDFMatrix);
        os.writeAlignmentPad(DefaultAlignment);
    }

private:
    std::shared_ptr<KlemsBasis> mRowBasis;
    std::shared_ptr<KlemsBasis> mColumnBasis;
    KlemsMatrix mMatrix;
    KlemsMatrix mCDFMatrix;
};

static inline void assignSpecification(KlemsComponent& component, KlemsComponentSpecification& spec)
{
    // IG_LOG(L_INFO) << "MinPSA: " << component.computeMinimumProjectedSolidAngle() << " MaxHemiS: " << component.computeMaxHemisphericalScattering() << " Diff: " << component.extractDiffuse() << std::endl;
    spec.total       = component.computeTotal();
    spec.theta_count = { component.row()->thetaCount(), component.column()->thetaCount() };
    spec.entry_count = { component.row()->entryCount(), component.column()->entryCount() };
}

bool KlemsLoader::prepare(const std::filesystem::path& in_xml, const std::filesystem::path& out_data, KlemsSpecification& spec)
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
    if (!transmissionBack)
        transmissionBack = transmissionFront;
    if (!transmissionFront)
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

    assignSpecification(*reflectionFront, spec.front_reflection);
    assignSpecification(*transmissionFront, spec.front_transmission);
    assignSpecification(*reflectionBack, spec.back_reflection);
    assignSpecification(*transmissionBack, spec.back_transmission);

    FileSerializer serializer(out_data, false);
    reflectionFront->write(serializer);
    transmissionFront->write(serializer);
    reflectionBack->write(serializer);
    transmissionBack->write(serializer);

    return true;
}

} // namespace IG