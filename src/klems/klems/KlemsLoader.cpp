#include "KlemsLoader.h"
#include "Logger.h"

#include <fstream>
#include <sstream>

#include <pugixml.hpp>

namespace IG {

class KlemsBasis {
public:
	using MultiIndex = std::pair<int, int>;

	KlemsBasis() = default;

	struct ThetaBasis {
		float CenterTheta;
		float LowerTheta;
		float UpperTheta;
		uint32_t PhiCount;

		inline bool isValid() const
		{
			return PhiCount > 0 && LowerTheta < UpperTheta;
		}
	};

	inline void addBasis(const ThetaBasis& basis)
	{
		mThetaBasis.push_back(basis);
	}

	inline void setup()
	{
		std::sort(mThetaBasis.begin(), mThetaBasis.end(),
				  [](const ThetaBasis& a, const ThetaBasis& b) { return a.UpperTheta < b.UpperTheta; });

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
		os.write(reinterpret_cast<const char*>(&theta_count), sizeof(uint32_t));
		for (auto& basis : mThetaBasis) {
			os.write(reinterpret_cast<const char*>(&basis.CenterTheta), sizeof(float));
			os.write(reinterpret_cast<const char*>(&basis.LowerTheta), sizeof(float));
			os.write(reinterpret_cast<const char*>(&basis.UpperTheta), sizeof(float));
			os.write(reinterpret_cast<const char*>(&basis.PhiCount), sizeof(uint32_t));
		}
	}

	inline void read(std::istream& is)
	{
		mThetaBasis.clear();

		uint32_t theta_count = 0;
		is.read(reinterpret_cast<char*>(&theta_count), sizeof(uint32_t));
		for (uint32_t i = 0; i < theta_count; ++i) {
			ThetaBasis basis;
			is.read(reinterpret_cast<char*>(&basis.CenterTheta), sizeof(float));
			is.read(reinterpret_cast<char*>(&basis.LowerTheta), sizeof(float));
			is.read(reinterpret_cast<char*>(&basis.UpperTheta), sizeof(float));
			is.read(reinterpret_cast<char*>(&basis.PhiCount), sizeof(uint32_t));
			mThetaBasis.push_back(basis);
		}

		setup();
	}

private:
	std::vector<ThetaBasis> mThetaBasis;
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

	inline void read(std::istream& is)
	{
		mRowBasis->read(is);
		mColumnBasis->read(is);

		mMatrix = KlemsMatrix(mRowBasis->entryCount(), mColumnBasis->entryCount());
		is.read(reinterpret_cast<char*>(mMatrix.data()), sizeof(float) * mMatrix.size());
	}

private:
	std::shared_ptr<KlemsBasis> mRowBasis;
	std::shared_ptr<KlemsBasis> mColumnBasis;
	KlemsMatrix mMatrix;
};

enum AllowedComponents {
	AC_FrontReflection	 = 0x1,
	AC_FrontTransmission = 0x2,
	AC_BackReflection	 = 0x4,
	AC_BackTransmission	 = 0x8,
	AC_FrontAll			 = AC_FrontReflection | AC_FrontTransmission,
	AC_BackAll			 = AC_BackReflection | AC_BackTransmission,
	AC_ReflectionAll	 = AC_FrontReflection | AC_BackReflection,
	AC_TransmissionAll	 = AC_FrontTransmission | AC_BackTransmission,
	AC_All				 = AC_FrontAll | AC_BackAll
};

bool KlemsLoader::prepare(const std::string& in_xml, const std::string& out_data)
{
	const int allowedComponents = AC_All;

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
	bool rowBased	 = type == "Rows";
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
			KlemsBasis::ThetaBasis basis;

			const auto bounds = child.child("ThetaBounds");
			basis.LowerTheta  = Deg2Rad * bounds.child("LowerTheta").text().as_float(0);
			basis.UpperTheta  = Deg2Rad * bounds.child("UpperTheta").text().as_float(0);
			basis.PhiCount	  = child.child("nPhis").text().as_uint(0);

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
		const char* rowBasisName	= block.child_value("RowAngleBasis");
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
		char* end			 = nullptr;
		Eigen::Index ind	 = 0;
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
		if (direction == "Transmission Front" && (allowedComponents & AC_FrontTransmission))
			transmissionFront = component;
		else if (direction == "Scattering Back" && (allowedComponents & AC_BackReflection))
			reflectionBack = component;
		else if (direction == "Transmission Back" && (allowedComponents & AC_BackTransmission))
			transmissionBack = component;
		else if (allowedComponents & AC_FrontReflection)
			reflectionFront = component;
	}

	// Make sure both transmission parts are equal if not specified otherwise
	if (!reflectionBack)
		reflectionBack = reflectionFront;
	if (!reflectionFront)
		reflectionFront = reflectionBack;
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

	std::ofstream stream(out_data, std::ios::binary);
	reflectionFront->write(stream);
	transmissionFront->write(stream);
	reflectionBack->write(stream);
	transmissionBack->write(stream);

	return true;
}

} // namespace IG