#include "AreaLight.h"
#include "Logger.h"
#include "loader/LoaderEntity.h"
#include "loader/LoaderShape.h"
#include "loader/LoaderUtils.h"
#include "loader/Parser.h"
#include "loader/ShadingTree.h"
#include "serialization/VectorSerializer.h"
#include "table/SceneDatabase.h"

namespace IG {
static inline float approximate_area_scale(const Transformf& transform, const BoundingBox& local_bbox)
{
    // Assume area distribution is uniform over bounding box in local space (which is not the case most of the time, but hopefully close to it).
    // Extract scale factor after applying transformation to given bounding box, making sure the rotation is not accounted for.
    // Only taking the determinant of the linear transformation would not account for the non-uniform size of an object.

    const Vector3f ls = local_bbox.diameter();
    const float w     = (transform.linear() * Vector3f::UnitX() * ls.x()).norm();
    const float h     = (transform.linear() * Vector3f::UnitY() * ls.y()).norm();
    const float d     = (transform.linear() * Vector3f::UnitZ() * ls.z()).norm();

    return (w * h + w * d + h * d) / local_bbox.halfArea();
}

static inline float approximate_ellipsoid_area(const Transformf& transform, float local_radius)
{
    const float w = (transform.linear() * Vector3f::UnitX() * local_radius).norm();
    const float h = (transform.linear() * Vector3f::UnitY() * local_radius).norm();
    const float d = (transform.linear() * Vector3f::UnitZ() * local_radius).norm();

    // See https://en.wikipedia.org/wiki/Ellipsoid
    constexpr float P = 1.6075f;
    return 4 * Pi * std::pow((std::pow(w * h, P) + std::pow(w * d, P) + std::pow(h * d, P)) / 3, 1 / P);
}

AreaLight::AreaLight(const std::string& name, const LoaderContext& ctx, const std::shared_ptr<SceneObject>& light)
    : Light(name, light->pluginType())
    , mLight(light)
{
    mEntity   = light->property("entity").getString();
    mUsePower = light->hasProperty("power");

    const auto entity = ctx.Entities->getEmissiveEntity(mEntity);
    if (!entity.has_value()) {
        IG_LOG(L_ERROR) << "No entity named '" << mEntity << "' exists for area light" << std::endl;
        return;
    }

    const bool opt = light->property("optimize").getBool(true) || !ctx.Shapes->isTriShape(entity->ShapeID);

    if (opt && ctx.Shapes->isPlaneShape(entity->ShapeID))
        mRepresentation = RepresentationType::Plane;
    else if (opt && ctx.Shapes->isSphereShape(entity->ShapeID))
        mRepresentation = RepresentationType::Sphere;
    else
        mRepresentation = RepresentationType::None;

    switch (mRepresentation) {
    case RepresentationType::Plane: {
        IG_LOG(L_DEBUG) << "Using specialized plane sampler for area light '" << name << "'" << std::endl;

        const auto& shape = ctx.Shapes->getPlaneShape(entity->ShapeID);
        Vector3f origin   = entity->Transform * shape.Origin;
        Vector3f x_axis   = entity->Transform.linear() * shape.XAxis;
        Vector3f y_axis   = entity->Transform.linear() * shape.YAxis;
        Vector3f normal   = x_axis.cross(y_axis).normalized();

        mPosition  = origin + x_axis * 0.5f + y_axis * 0.5f;
        mDirection = normal;
        mArea      = x_axis.cross(y_axis).norm();
    } break;
    case RepresentationType::Sphere: {
        IG_LOG(L_DEBUG) << "Using specialized sphere sampler for area light '" << name << "'" << std::endl;

        const auto& shape = ctx.Shapes->getSphereShape(entity->ShapeID);
        Vector3f origin   = entity->Transform * shape.Origin;

        mPosition  = origin;
        mDirection = Vector3f::Zero();
        mArea      = approximate_ellipsoid_area(entity->Transform, shape.Radius);
    } break;
    default:
    case RepresentationType::None:
        if (ctx.Shapes->isTriShape(entity->ShapeID)) {
            const auto& shape    = ctx.Shapes->getShape(entity->ShapeID);
            const auto& trishape = ctx.Shapes->getTriShape(entity->ShapeID);

            mPosition  = entity->Transform * shape.BoundingBox.center();
            mDirection = Vector3f::Zero();
            mArea      = trishape.Area * approximate_area_scale(entity->Transform, shape.BoundingBox);
        } else {
            IG_LOG(L_ERROR) << "Given entity '" << mEntity << "' primitive type is not triangular" << std::endl;
        }
        break;
    }
}

float AreaLight::computeFlux(ShadingTree& tree) const
{
    if (mUsePower)
        return tree.computeNumber("power", *mLight, mArea * Pi);
    else
        return tree.computeNumber("radiance", *mLight, 1.0f) * mArea * Pi;
}

void AreaLight::serialize(const SerializationInput& input) const
{
    input.Tree.beginClosure(name());

    if (mUsePower)
        input.Tree.addColor("power", *mLight, Vector3f::Constant(mArea * Pi));
    else
        input.Tree.addColor("radiance", *mLight, Vector3f::Constant(1.0f));

    const auto entity = input.Tree.context().Entities->getEmissiveEntity(mEntity);
    if (!entity.has_value()) {
        IG_LOG(L_ERROR) << "No entity named '" << mEntity << "' exists for area light" << std::endl;
        return;
    }

    const std::string light_id = input.Tree.currentClosureID();
    input.Stream << input.Tree.pullHeader();

    switch (mRepresentation) {
    case RepresentationType::Plane: {
        const auto& shape = input.Tree.context().Shapes->getPlaneShape(entity->ShapeID);
        Vector3f origin   = entity->Transform * shape.Origin;
        Vector3f x_axis   = entity->Transform.linear() * shape.XAxis;
        Vector3f y_axis   = entity->Transform.linear() * shape.YAxis;
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
        const auto& shape = input.Tree.context().Shapes->getSphereShape(entity->ShapeID);

        input.Stream << "  let ae_" << light_id << " = make_sphere_area_emitter(" << LoaderUtils::inlineEntity(*entity)
                     << ",  Sphere{ origin = " << LoaderUtils::inlineVector(shape.Origin)
                     << ", radius = " << shape.Radius
                     << " });" << std::endl;
    } break;
    default:
    case RepresentationType::None:
        if (input.Tree.context().Shapes->isTriShape(entity->ShapeID)) {
            const auto& shape = input.Tree.context().Shapes->getShape(entity->ShapeID);
            const auto& tri   = input.Tree.context().Shapes->getTriShape(entity->ShapeID);
            input.Stream << "  let trimesh_" << light_id << " = load_trimesh_entry(device, "
                         << shape.TableOffset
                         << ", " << tri.FaceCount
                         << ", " << tri.VertexCount
                         << ", " << tri.NormalCount
                         << ", " << tri.TexCount << ");" << std::endl
                         << "  let ae_" << light_id << " = make_shape_area_emitter(" << LoaderUtils::inlineEntity(*entity)
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
                 << ", ae_" << light_id;

    if (mUsePower)
        input.Stream << ", @|ctx| { maybe_unused(ctx); color_mulf(" << input.Tree.getInline("power") << ", flt_inv_pi / " << mArea << ") });" << std::endl;
    else
        input.Stream << ", @|ctx| { maybe_unused(ctx); " << input.Tree.getInline("radiance") << " });" << std::endl;

    input.Tree.endClosure();
}

std::optional<std::string> AreaLight::getEmbedClass() const
{
    // TODO: Basic texture?
    const auto radiance = mUsePower ? mLight->property("power") : mLight->property("radiance");
    const bool simple   = (!radiance.isValid() || radiance.canBeNumber() || radiance.type() == SceneProperty::PT_VECTOR3);

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
    const Vector3f radiance      = mUsePower ? input.Tree.computeColor("power", *mLight, Vector3f::Constant(mArea * Pi)) / (mArea * Pi) : input.Tree.computeColor("radiance", *mLight, Vector3f::Ones());

    const auto& ctx   = input.Tree.context();
    const auto entity = ctx.Entities->getEmissiveEntity(entityName);
    if (!entity.has_value()) {
        IG_LOG(L_ERROR) << "No entity named '" << entityName << "' exists for area light" << std::endl;
        return;
    }

    const Matrix34f localMat  = entity->computeLocalMatrix();        // To Local
    const Matrix34f globalMat = entity->computeGlobalMatrix();       // To Global
    const Matrix3f normalMat  = entity->computeGlobalNormalMatrix(); // To Global [Normal]

    switch (mRepresentation) {
    case RepresentationType::Plane: {
        const auto& shape = input.Tree.context().Shapes->getPlaneShape(entity->ShapeID);
        Vector3f origin   = entity->Transform * shape.Origin;
        Vector3f x_axis   = entity->Transform.linear() * shape.XAxis;
        Vector3f y_axis   = entity->Transform.linear() * shape.YAxis;
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
        const auto& shape = input.Tree.context().Shapes->getSphereShape(entity->ShapeID);

        input.Serializer.write(localMat, true);  // +3x4 = 12
        input.Serializer.write(globalMat, true); // +3x4 = 24
        input.Serializer.write(normalMat, true); // +3x3 = 33
        input.Serializer.write(shape.Origin);    // +3   = 36
        input.Serializer.write(radiance);        // +3   = 39
        input.Serializer.write(shape.Radius);    // +1   = 40
    } break;
    default:
    case RepresentationType::None:
        if (input.Tree.context().Shapes->isTriShape(entity->ShapeID)) {
            input.Serializer.write(localMat, true);                           // +3x4 = 12
            input.Serializer.write(globalMat, true);                          // +3x4 = 24
            input.Serializer.write(normalMat, true);                          // +3x3 = 33
            input.Serializer.write((uint32)entity->ShapeID);                  // +1   = 34
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