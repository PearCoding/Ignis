#include "SphereProvider.h"

#include "loader/LoaderResult.h"
#include "loader/LoaderShape.h"
#include "serialization/VectorSerializer.h"
#include "shader/ShaderUtils.h"

#include "Logger.h"

namespace IG {
void SphereProvider::handle(LoaderContext& ctx, LoaderResult& result, const std::string& name, const Parser::Object& elem)
{
    float radius    = 1;
    Vector3f origin = Vector3f::Zero();

    if (elem.pluginType() == "sphere") {
        origin = elem.property("center").getVector3(origin);
        radius = elem.property("radius").getNumber(radius);
    } else {
        IG_LOG(L_ERROR) << "Shape '" << name << "': Can not load shape type '" << elem.pluginType() << "'" << std::endl;
        return;
    }

    if (radius <= 0) {
        IG_LOG(L_ERROR) << "Shape '" << name << "': While loading shape type '" << elem.pluginType() << "' an invalid radius of " << radius << " was given" << std::endl;
        return;
    }

    // Build bounding box
    BoundingBox bbox = BoundingBox::Empty();
    bbox.extend(origin + Vector3f::UnitX() * radius);
    bbox.extend(origin - Vector3f::UnitX() * radius);
    bbox.extend(origin + Vector3f::UnitY() * radius);
    bbox.extend(origin - Vector3f::UnitY() * radius);
    bbox.extend(origin + Vector3f::UnitZ() * radius);
    bbox.extend(origin - Vector3f::UnitZ() * radius);
    bbox.inflate(1e-5f); // Make sure it has a volume

    // Make sure the id used in shape is same as in the dyntable later
    result.DatabaseAccessMutex.lock();

    auto& table         = result.Database.DynTables["shapes"];
    auto& data          = table.addLookup((uint32)this->id(), 0, DefaultAlignment);
    const size_t offset = table.currentOffset();

    VectorSerializer serializer(data, false);
    serializer.write(origin);
    serializer.write(radius);
    const uint32 id = ctx.Shapes->addShape(name, Shape{ this, 0, 0, 0, bbox, offset });

    ctx.Shapes->addSphereShape(id, SphereShape{ origin, radius });

    result.DatabaseAccessMutex.unlock();
}

std::string SphereProvider::generateShapeCode(const LoaderContext& ctx)
{
    IG_UNUSED(ctx);
    return "make_sphere_shape(load_sphere(data))";
}

std::string SphereProvider::generateTraversalCode(const LoaderContext& ctx)
{
    std::stringstream stream;
    stream << ShaderUtils::generateShapeLookup("sphere_shapes", this, ctx) << std::endl;

    stream << std::endl
           << "  let spheres = load_sphere_table(device);" << std::endl
           << "  let trace   = TraceAccessor { shapes = sphere_shapes, entities = entities };" << std::endl
           << "  let handler = make_scene_local_handler_sphere(spheres);" << std::endl;
    return stream.str();
}
} // namespace IG