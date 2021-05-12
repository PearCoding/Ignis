#pragma once

namespace IG {
enum class DebugMode {
	Normal = 0,
	Tangent,
	Bitangent,
	GeometryNormal,
	TexCoords,
	UVCoords,
	HitDistance,
	PrimID,
	EntityID
};
}