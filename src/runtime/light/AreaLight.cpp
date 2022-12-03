#include "AreaLight.h"
#include "Logger.h"
#include "loader/LoaderShape.h"
#include "loader/LoaderUtils.h"
#include "loader/Parser.h"
#include "loader/ShadingTree.h"
#include "serialization/VectorSerializer.h"
#include "table/SceneDatabase.h"

namespace IG {
AreaLight::AreaLight(const std::string& name, const LoaderContext& ctx, const std::shared_ptr<Parser::Object>& light)
    : Light(name, light->pluginType())
    , mLight(light)
{
    mEntity = light->property("entity").getString();

    Entity entity;
    if (!ctx.EmissiveEntities.count(mEntity)) {
        IG_LOG(L_ERROR) << "No entity named '" << mEntity << "' exists for area light" << std::endl;
        return;
    } else {
        entity = ctx.EmissiveEntities.at(mEntity);
    }

    const bool opt = light->property("optimize").getBool(true) || !ctx.Shapes->isTriShape(entity.ShapeID);

    if (opt && ctx.Shapes->isPlaneShape(entity.ShapeID))
        mRepresentation = RepresentationType::Plane;
    else if (opt && ctx.Shapes->isSphereShape(entity.ShapeID))
        mRepresentation = RepresentationType::Sphere;
    else
        mRepresentation = RepresentationType::None;

    switch (mRepresentation) {
    case RepresentationType::Plane: {
        IG_LOG(L_DEBUG) << "Using specialized plane sampler for area light '" << name << "'" << std::endl;

        const auto& shape = ctx.Shapes->getPlaneShape(entity.ShapeID);
        Vector3f origin   = entity.Transform * shape.Origin;
        Vector3f x_axis   = entity.Transform.linear() * shape.XAxis;
        Vector3f y_axis   = entity.Transform.linear() * shape.YAxis;
        Vector3f normal   = x_axis.cross(y_axis).normalized();

        mPosition  = origin + x_axis * 0.5f + y_axis * 0.5f;
        mDirection = normal;
        mArea      = x_axis.cross(y_axis).norm();
    } break;
    case RepresentationType::Sphere: {
        IG_LOG(L_DEBUG) << "Using specialized sphere sampler for area light '" << name << "'" << std::endl;

        const auto& shape = ctx.Shapes->getSphereShape(entity.ShapeID);
        Vector3f origin   = entity.Transform * shape.Origin;
        float radius      = entity.Transform.linear().diagonal().cwiseAbs().maxCoeff() * shape.Radius; // TODO: Ignoring non-uniform scale

        mPosition  = origin;
        mDirection = Vector3f::Zero();
        mArea      = 4 * Pi * radius * radius;
    } break;
    default:
    case RepresentationType::None:
        if (ctx.Shapes->isTriShape(entity.ShapeID)) {
            const auto& shape    = ctx.Shapes->getShape(entity.ShapeID);
            const auto& trishape = ctx.Shapes->getTriShape(entity.ShapeID);

            mPosition  = entity.Transform * shape.BoundingBox.center();
            mDirection = Vector3f::Zero();
            mArea      = trishape.Area * std::abs(entity.computeGlobalMatrix().block<3, 3>(0, 0).determinant());
        } else {
            IG_LOG(L_ERROR) << "Given entity '" << mEntity << "' primitive type is not triangular" << std::endl;
        }
        break;
    }
}

float AreaLight::computeFlux(const ShadingTree& tree) const
{
    const float power = tree.computeNumber("radiance", *mLight, 1);
    return power * mArea * Pi;
}

void AreaLight::serialize(const SerializationInput& input) const
{
    input.Tree.beginClosure(name());

    input.Tree.addColor("radiance", *mLight, Vector3f::Constant(1.0f), true);

    Entity entity;
    if (!input.Tree.context().EmissiveEntities.count(mEntity)) {
        IG_LOG(L_ERROR) << "No entity named '" << mEntity << "' exists for area light" << std::endl;
        return;
    } else {
        entity = input.Tree.context().EmissiveEntities.at(mEntity);
    }

    const std::string light_id = input.Tree.currentClosureID();
    input.Stream << input.Tree.pullHeader();

    switch (mRepresentation) {
    case RepresentationType::Plane: {
        const auto& shape = input.Tree.context().Shapes->getPlaneShape(entity.ShapeID);
        Vector3f origin   = entity.Transform * shape.Origin;
        Vector3f x_axis   = entity.Transform.linear() * shape.XAxis;
        Vector3f y_axis   = entity.Transform.linear() * shape.YAxis;
        Vector3f normal   = x_axis.cross(y_axis).normalized();
        float area        = x_axis.cross(y_axis).norm();

        input.Stream << "  let ae_" << light_id << " = make_plane_area_emitter(" << LoaderUtils::inlineVector(origin)
                     << ", " << LoaderUtils::inlineVector(x_axis)
                     << ", " << LoaderUtils::inlineVector(y_axis)
                     << ", " << LoaderUtils::inlineVector(normal)
                     << ", " << area
                     << ", " << LoaderUtils::inlineVector2d(shape.TexCoords[0])
                     << ", " << LoaderUtils::inlineVector2d(shape.TexCoords[1])
                     << ", " << LoaderUtils::inlineVector2d(shape.TexCoords[2])
                     << ", " << LoaderUtils::inlineVector2d(shape.TexCoords[3])
                     << ");" << std::endl;
    } break;
    case RepresentationType::Sphere: {
        const auto& shape = input.Tree.context().Shapes->getSphereShape(entity.ShapeID);

        input.Stream << "  let ae_" << light_id << " = make_sphere_area_emitter(" << LoaderUtils::inlineEntity(entity, entity.ShapeID)
                     << ",  Sphere{ origin = " << LoaderUtils::inlineVector(shape.Origin)
                     << ", radius = " << shape.Radius
                     << " });" << std::endl;
    } break;
    default:
    case RepresentationType::None:
        if (input.Tree.context().Shapes->isTriShape(entity.ShapeID)) {
            const auto& shape = input.Tree.context().Shapes->getShape(entity.ShapeID);
            const auto& tri   = input.Tree.context().Shapes->getTriShape(entity.ShapeID);
            input.Stream << "  let trimesh_" << light_id << " = load_trimesh_entry(device, "
                         << shape.TableOffset
                         << ", " << tri.FaceCount
                         << ", " << tri.VertexCount
                         << ", " << tri.NormalCount
                         << ", " << tri.TexCount << ");" << std::endl
                         << "  let ae_" << light_id << " = make_shape_area_emitter(" << LoaderUtils::inlineEntity(entity, entity.ShapeID)
                         << ", make_trimesh_shape(trimesh_" << light_id << ")"
                         << ", trimesh_" << light_id << ");" << std::endl;
        } else {
            // Error already messaged
            input.Tree.signalError();
            input.Tree.endClosure();
            return;
        }
        break;
    }

    input.Stream << "  let light_" << light_id << " = make_area_light(" << input.ID
                 << ", ae_" << light_id
                 << ", @|ctx| { maybe_unused(ctx); " << input.Tree.getInline("radiance") << " });" << std::endl;

    input.Tree.endClosure();
}

std::optional<std::string> AreaLight::getEmbedClass() const
{
    // TODO: Basic texture?
    const auto radiance = mLight->property("radiance");
    const bool simple   = (!radiance.isValid() || radiance.canBeNumber() || radiance.type() == Parser::PT_VECTOR3);

    if (!simple)
        return std::nullopt;

    switch (mRepresentation) {
    case RepresentationType::Plane:
        return std::make_optional("SimplePlaneLight");
    case RepresentationType::Sphere:
        return std::make_optional("SimpleSphereLight");
    default:
        return std::make_optional("SimpleAreaLight");
    }
}

void AreaLight::embed(const EmbedInput& input) const
{
    const std::string entityName = mLight->property("entity").getString();
    const Vector3f radiance      = input.Tree.computeColor("radiance", *mLight, Vector3f::Ones());

    const auto& ctx = input.Tree.context();
    Entity entity;
    if (!ctx.EmissiveEntities.count(entityName)) {
        IG_LOG(L_ERROR) << "No entity named '" << entityName << "' exists for area light" << std::endl;
        return;
    } else {
        entity = ctx.EmissiveEntities.at(entityName);
    }

    const Eigen::Matrix<float, 3, 4> localMat  = entity.computeLocalMatrix();        // To Local
    const Eigen::Matrix<float, 3, 4> globalMat = entity.computeGlobalMatrix();       // To Global
    const Matrix3f normalMat                   = entity.computeGlobalNormalMatrix(); // To Global [Normal]

    switch (mRepresentation) {
    case RepresentationType::Plane: {
        const auto& shape = input.Tree.context().Shapes->getPlaneShape(entity.ShapeID);
        Vector3f origin   = entity.Transform * shape.Origin;
        Vector3f x_axis   = entity.Transform.linear() * shape.XAxis;
        Vector3f y_axis   = entity.Transform.linear() * shape.YAxis;
        Vector3f normal   = x_axis.cross(y_axis).normalized();
        float area        = x_axis.cross(y_axis).norm();

        input.Serializer.write(origin);             // +3 = 3
        input.Serializer.write(normal.x());         // +1 = 4
        input.Serializer.write(x_axis);             // +3 = 7
        input.Serializer.write(normal.y());         // +1 = 8
        input.Serializer.write(y_axis);             // +3 = 11
        input.Serializer.write(normal.z());         // +1 = 12
        input.Serializer.write(shape.TexCoords[0]); // +2 = 14
        input.Serializer.write(shape.TexCoords[1]); // +2 = 16
        input.Serializer.write(shape.TexCoords[2]); // +2 = 18
        input.Serializer.write(shape.TexCoords[3]); // +2 = 20
        input.Serializer.write(radiance);           // +3 = 23
        input.Serializer.write(area);               // +1 = 24
    } break;
    case RepresentationType::Sphere: {
        const auto& shape = input.Tree.context().Shapes->getSphereShape(entity.ShapeID);

        input.Serializer.write(localMat, true);  // +3x4 = 12
        input.Serializer.write(globalMat, true); // +3x4 = 24
        input.Serializer.write(normalMat, true); // +3x3 = 33
        input.Serializer.write(shape.Origin);    // +3   = 36
        input.Serializer.write(radiance);        // +3   = 39
        input.Serializer.write(shape.Radius);    // +1   = 40
    } break;
    default:
    case RepresentationType::None:
        if (input.Tree.context().Shapes->isTriShape(entity.ShapeID)) {
            input.Serializer.write(localMat, true);                           // +3x4 = 12
            input.Serializer.write(globalMat, true);                          // +3x4 = 24
            input.Serializer.write(normalMat, true);                          // +3x3 = 33
            input.Serializer.write((uint32)entity.ShapeID);                   // +1   = 34
            input.Serializer.write((float)std::abs(normalMat.determinant())); // +1   = 35
            input.Serializer.write((uint32)0 /*Padding*/);                    // +1   = 36
            input.Serializer.write(radiance);                                 // +3   = 39
            input.Serializer.write((uint32)0 /*Padding*/);                    // +1   = 40
        } else {
            // Error already messaged
            input.Tree.signalError();
            return;
        }
        break;
    }
}

} // namespace IG