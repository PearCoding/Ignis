#pragma once

#include "serialization/Serializer.h"
#include <numeric>

namespace IG {
struct KlemsComponentSpecification {
    std::pair<size_t, size_t> theta_count;
    std::pair<size_t, size_t> entry_count;
    float total;
};
struct KlemsSpecification {
    KlemsComponentSpecification front_reflection;
    KlemsComponentSpecification back_reflection;
    KlemsComponentSpecification front_transmission;
    KlemsComponentSpecification back_transmission;
};

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
        constexpr size_t Alignment = 4 * sizeof(float);

        mRowBasis->write(os);
        os.writeAlignmentPad(Alignment);
        mColumnBasis->write(os);
        os.writeAlignmentPad(Alignment);

        os.write(mMatrix);
        os.write(mCDFMatrix);
        os.writeAlignmentPad(Alignment);
    }

private:
    std::shared_ptr<KlemsBasis> mRowBasis;
    std::shared_ptr<KlemsBasis> mColumnBasis;
    KlemsMatrix mMatrix;
    KlemsMatrix mCDFMatrix;
};

class Klems {
public:
    std::shared_ptr<KlemsComponent> front_reflection;
    std::shared_ptr<KlemsComponent> back_reflection;
    std::shared_ptr<KlemsComponent> front_transmission;
    std::shared_ptr<KlemsComponent> back_transmission;
};

class IG_LIB KlemsLoader {
public:
    static bool load(const Path& in_xml, Klems& klems);
    static bool prepare(const Path& in_xml, const Path& out_data, KlemsSpecification& spec);
};
} // namespace IG