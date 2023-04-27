#include <nanobind/eigen/dense.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/filesystem.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/unordered_map.h>
#include <nanobind/stl/vector.h>

#include "Scene.h"
#include "loader/Parser.h"

namespace nb = nanobind;
using namespace nb::literals;

using namespace IG;

void scene_module(nb::module_& m)
{
    auto prop = nb::class_<SceneProperty>(m, "SceneProperty", "Property of an object in the scene")
                    .def_prop_ro("type", &SceneProperty::type)
                    .def("isValid", &SceneProperty::isValid)
                    .def("canBeNumber", &SceneProperty::canBeNumber)
                    .def(
                        "getBool", [](const SceneProperty& prop, bool def) { return prop.getBool(def); }, nb::arg("def") = false)
                    .def_static("fromBool", &SceneProperty::fromBool)
                    .def(
                        "getInteger", [](const SceneProperty& prop, SceneProperty::Integer def) { return prop.getInteger(def); }, nb::arg("def") = (SceneProperty::Integer)0)
                    .def_static("fromInteger", &SceneProperty::fromInteger)
                    .def("getIntegerArray", [](const SceneProperty& prop) { return prop.getIntegerArray(); })
                    .def_static("fromIntegerArray", nb::overload_cast<const SceneProperty::IntegerArray&>(&SceneProperty::fromIntegerArray))
                    .def(
                        "getNumber", [](const SceneProperty& prop, SceneProperty::Number def) { return prop.getNumber(def); }, nb::arg("def") = (SceneProperty::Number)0)
                    .def_static("fromNumber", &SceneProperty::fromNumber)
                    .def("getNumberArray", [](const SceneProperty& prop) { return prop.getNumberArray(); })
                    .def_static("fromNumberArray", nb::overload_cast<const SceneProperty::NumberArray&>(&SceneProperty::fromNumberArray))
                    .def(
                        "getString", [](const SceneProperty& prop, const std::string& def) { return prop.getString(def); }, nb::arg("def") = "")
                    .def_static("fromString", &SceneProperty::fromString)
                    .def(
                        "getTransform", [](const SceneProperty& prop, const Matrix4f& def) -> Matrix4f { return prop.getTransform(Transformf(def)).matrix(); }, nb::arg("def") = Matrix4f::Identity())
                    .def_static("fromTransform", [](const Matrix4f& mat) { return SceneProperty::fromTransform(Transformf(mat)); })
                    .def(
                        "getVector2", [](const SceneProperty& prop, const Vector2f& def) { return prop.getVector2(def); }, nb::arg("def") = Vector2f::Zero())
                    .def_static("fromVector2", &SceneProperty::fromVector2)
                    .def(
                        "getVector3", [](const SceneProperty& prop, const Vector3f& def) { return prop.getVector3(def); }, nb::arg("def") = Vector3f::Zero())
                    .def_static("fromVector3", &SceneProperty::fromVector3);

    nb::enum_<SceneProperty::Type>(prop, "Type", "Enum holding type of scene property")
        .value("None", SceneProperty::PT_NONE)
        .value("Bool", SceneProperty::PT_BOOL)
        .value("Integer", SceneProperty::PT_INTEGER)
        .value("Number", SceneProperty::PT_NUMBER)
        .value("String", SceneProperty::PT_STRING)
        .value("Transform", SceneProperty::PT_TRANSFORM)
        .value("Vector2", SceneProperty::PT_VECTOR2)
        .value("Vector3", SceneProperty::PT_VECTOR3)
        .value("IntegerArray", SceneProperty::PT_INTEGER_ARRAY)
        .value("NumberArray", SceneProperty::PT_NUMBER_ARRAY)
        .export_values();

    auto obj = nb::class_<SceneObject>(m, "SceneObject", "Class representing an object in the scene")
                   .def(nb::init<SceneObject::Type, const std::string&, const Path&>())
                   .def_prop_ro("type", &SceneObject::type)
                   .def_prop_ro("baseDir", &SceneObject::baseDir)
                   .def_prop_ro("pluginType", &SceneObject::pluginType)
                   .def("property", &SceneObject::property)
                   .def("setProperty", &SceneObject::setProperty)
                   .def("hasProperty", &SceneObject::hasProperty)
                   .def("__getitem__", &SceneObject::property)
                   .def("__setitem__", &SceneObject::setProperty)
                   .def("__contains__", &SceneObject::hasProperty)
                   .def_prop_ro("properties", &SceneObject::properties);

    nb::enum_<SceneObject::Type>(obj, "Type", "Enum holding type of scene object")
        .value("Bsdf", SceneObject::OT_BSDF)
        .value("Camera", SceneObject::OT_CAMERA)
        .value("Entity", SceneObject::OT_ENTITY)
        .value("Film", SceneObject::OT_FILM)
        .value("Light", SceneObject::OT_LIGHT)
        .value("Medium", SceneObject::OT_MEDIUM)
        .value("Shape", SceneObject::OT_SHAPE)
        .value("Technique", SceneObject::OT_TECHNIQUE)
        .value("Texture", SceneObject::OT_TEXTURE)
        .export_values();

    nb::class_<Scene>(m, "Scene", "Class representing a whole scene")
        .def(nb::init<>())
        .def_prop_rw("camera", &Scene::camera, &Scene::setCamera)
        .def_prop_rw("technique", &Scene::technique, &Scene::setTechnique)
        .def_prop_rw("film", &Scene::film, &Scene::setFilm)
        .def("addTexture", &Scene::addTexture)
        .def("texture", &Scene::texture)
        .def_prop_ro("textures", &Scene::textures)
        .def("addBSDF", &Scene::addBSDF)
        .def("bsdf", &Scene::bsdf)
        .def_prop_ro("bsdfs", &Scene::bsdfs)
        .def("addShape", &Scene::addShape)
        .def("shape", &Scene::shape)
        .def_prop_ro("shapes", &Scene::shapes)
        .def("addLight", &Scene::addLight)
        .def("light", &Scene::light)
        .def_prop_ro("lights", &Scene::lights)
        .def("addMedium", &Scene::addMedium)
        .def("medium", &Scene::medium)
        .def_prop_ro("media", &Scene::media)
        .def("addEntity", &Scene::addEntity)
        .def("entity", &Scene::entity)
        .def_prop_ro("entities", &Scene::entities)
        .def("addConstantEnvLight", &Scene::addConstantEnvLight)
        .def("addFrom", &Scene::addFrom)
        .def_static(
            "loadFromFile", [](const Path& path, uint32 flags) {
                return SceneParser().loadFromFile(path, flags);
            },
            nb::arg("path"), nb::arg("flags") = (uint32)SceneParser::F_LoadAll)
        .def_static(
            "loadFromString", [](const std::string& str, const Path& dir, uint32 flags) {
                return SceneParser().loadFromString(str, dir, flags);
            },
            nb::arg("str"), nb::arg("opt_dir") = "", nb::arg("flags") = (uint32)SceneParser::F_LoadAll);

    auto parser = nb::class_<SceneParser>(m, "SceneParser", "Parser for standard JSON and glTF scene description")
                      .def(nb::init<>())
                      .def(
                          "loadFromFile", &SceneParser::loadFromFile,
                          nb::arg("path"), nb::arg("flags") = (uint32)SceneParser::F_LoadAll)
                      .def(
                          "loadFromString", &SceneParser::loadFromString,
                          nb::arg("str"), nb::arg("opt_dir") = "", nb::arg("flags") = (uint32)SceneParser::F_LoadAll);

    nb::enum_<SceneParser::Flags>(parser, "Flags", nb::is_arithmetic(), "Flags modifying parsing behaviour and allowing partial scene loads")
        .value("F_LoadCamera", SceneParser::F_LoadCamera)
        .value("F_LoadFilm", SceneParser::F_LoadFilm)
        .value("F_LoadTechnique", SceneParser::F_LoadTechnique)
        .value("F_LoadBSDFs", SceneParser::F_LoadBSDFs)
        .value("F_LoadMedia", SceneParser::F_LoadMedia)
        .value("F_LoadLights", SceneParser::F_LoadLights)
        .value("F_LoadTextures", SceneParser::F_LoadTextures)
        .value("F_LoadShapes", SceneParser::F_LoadShapes)
        .value("F_LoadEntities", SceneParser::F_LoadEntities)
        .value("F_LoadExternals", SceneParser::F_LoadExternals)
        .value("F_LoadAll", SceneParser::F_LoadAll)
        .export_values();
}