#include "GeneratorEntity.h"

#include "Logger.h"

namespace IG {

using namespace Loader;
void GeneratorEntity::setup(GeneratorContext& ctx)
{
	for (const auto& pair : ctx.Scene.entities()) {
		const auto child = pair.second;

		const std::string shapeName = child->property("shape").getString();
		if (ctx.Environment.Shapes.count(shapeName) == 0) {
			IG_LOG(L_ERROR) << "Entity " << pair.first << " has unknown shape " << shapeName << std::endl;
			continue;
		}

		const std::string bsdfName = child->property("bsdf").getString();
		if (!bsdfName.empty() && !ctx.Scene.bsdf(bsdfName)) {
			IG_LOG(L_ERROR) << "Entity " << pair.first << " has unknown bsdf " << bsdfName << std::endl;
			continue;
		}

		ctx.Environment.Entities.insert({ pair.first, Entity{ child->property("transform").getTransform(), shapeName, bsdfName } });
	}
}

} // namespace IG