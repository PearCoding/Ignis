#pragma once

#include "IG_Config.h"

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

struct KlemsBasis {
	std::vector<KlemsThetaBasis> ThetaBasis;
	std::vector<uint32_t> ThetaLinearOffset;
	uint32_t EntryCount;
};

struct KlemsComponent {
	KlemsBasis RowBasis;
	KlemsBasis ColumnBasis;
	std::vector<float> Matrix;

	inline uint32_t matrixSize() const { return RowBasis.EntryCount * ColumnBasis.EntryCount; }
};
struct KlemsModel {
	KlemsComponent FrontReflection;
	KlemsComponent BackReflection;
	KlemsComponent FrontTransmission;
	KlemsComponent BackTransmission;
};

class KlemsLoader {
public:
	static bool prepare(const std::filesystem::path& in_xml, const std::filesystem::path& out_data);
	static bool load(const std::filesystem::path& in_data, KlemsModel& model);
};
} // namespace IG