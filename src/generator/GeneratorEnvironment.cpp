#include "GeneratorEnvironment.h"

namespace IG {
void GeneratorEnvironment::calculateBoundingBox()
{
	SceneBBox = BoundingBox::Empty();
	for (size_t i = 0; i < Mesh.vertices.size(); ++i)
		SceneBBox.extend(Mesh.vertices[i]);
	SceneDiameter = (SceneBBox.max - SceneBBox.min).norm();
}
} // namespace IG