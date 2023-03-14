#include <pybind11/eigen.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "Scene.h"
#include "loader/Parser.h"

namespace py = pybind11;
using namespace IG;

void scene_module(py::module_& m)
{
    auto prop = py::class_<SceneProperty>(m, "SceneProperty", "Property of an object in the scene")
                    .def_property_readonly("type", &SceneProperty::type)
                    .def("isValid", &SceneProperty::isValid)
                    .def("canBeNumber", &SceneProperty::canBeNumber)
                    .def(
                        "getBool", [](const SceneProperty& prop, bool def) { return prop.getBool(def); }, py::arg("def") = false)
                    .def_static("fromBool", &SceneProperty::fromBool)
                    .def(
                        "getInteger", [](const SceneProperty& prop, SceneProperty::Integer def) { return prop.getInteger(def); }, py::arg("def") = (SceneProperty::Integer)0)
                    .def_static("fromInteger", &SceneProperty::fromInteger)
                    .def("getIntegerArray", [](const SceneProperty& prop) { return prop.getIntegerArray(); })
                    .def_static("fromIntegerArray", py::overload_cast<const SceneProperty::IntegerArray&>(&SceneProperty::fromIntegerArray))
                    .def(
                        "getNumber", [](const SceneProperty& prop, SceneProperty::Number def) { return prop.getNumber(def); }, py::arg("def") = (SceneProperty::Number)0)
                    .def_static("fromNumber", &SceneProperty::fromNumber)
                    .def("getNumberArray", [](const SceneProperty& prop) { return prop.getNumberArray(); })
                    .def_static("fromNumberArray", py::overload_cast<const SceneProperty::NumberArray&>(&SceneProperty::fromNumberArray))
                    .def(
                        "getString", [](const SceneProperty& prop, const std::string& def) { return prop.getString(def); }, py::arg("def") = "")
                    .def_static("fromString", &SceneProperty::fromString)
                    .def(
                        "getTransform", [](const SceneProperty& prop, const Matrix4f& def) -> Matrix4f { return prop.getTransform(Transformf(def)).matrix(); }, py::arg("def") = Matrix4f::Identity())
                    .def_static("fromTransform", [](const Matrix4f& mat) { return SceneProperty::fromTransform(Transformf(mat)); })
                    .def(
                        "getVector2", [](const SceneProperty& prop, const Vector2f& def) { return prop.getVector2(def); }, py::arg("def") = Vector2f::Zero())
                    .def_static("fromVector2", &SceneProperty::fromVector2)
                    .def(
                        "getVector3", [](const SceneProperty& prop, const Vector3f& def) { return prop.getVector3(def); }, py::arg("def") = Vector3f::Zero())
                    .def_static("fromVector3", &SceneProperty::fromVector3);

    py::enum_<SceneProperty::Type>(prop, "Type", "Enum holding type of scene property")
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

    auto obj = py::class_<SceneObject, std::shared_ptr<SceneObject>>(m, "SceneObject", "Class representing an object in the scene")
                   .def(py::init([](SceneObject::Type type, const std::string& pType, const std::string& dir) {
                       return SceneObject(type, pType, dir);
                   }))
                   .def_property_readonly("type", &SceneObject::type)
                   .def_property_readonly("baseDir", [](const SceneObject& o) { return o.baseDir().generic_u8string(); })
                   .def_property_readonly("pluginType", &SceneObject::pluginType)
                   .def("property", &SceneObject::property)
                   .def("setProperty", &SceneObject::setProperty)
                   .def("hasProperty", &SceneObject::hasProperty)
                   .def("__getitem__", &SceneObject::property)
                   .def("__setitem__", &SceneObject::setProperty)
                   .def("__contains__", &SceneObject::hasProperty)
                   .def_property_readonly("properties", &SceneObject::properties);

    py::enum_<SceneObject::Type>(obj, "Type", "Enum holding type of scene object")
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

    py::class_<Scene, std::shared_ptr<Scene>>(m, "Scene", "Class representing a whole scene")
        .def(py::init<>())
        .def_property("camera", &Scene::camera, &Scene::setCamera)
        .def_property("technique", &Scene::technique, &Scene::setTechnique)
        .def_property("film", &Scene::film, &Scene::setFilm)
        .def("addTexture", &Scene::addTexture)
        .def("texture", &Scene::texture)
        .def_property_readonly("textures", &Scene::textures)
        .def("addBSDF", &Scene::addBSDF)
        .def("bsdf", &Scene::bsdf)
        .def_property_readonly("bsdfs", &Scene::bsdfs)
        .def("addShape", &Scene::addShape)
        .def("shape", &Scene::shape)
        .def_property_readonly("shapes", &Scene::shapes)
        .def("addLight", &Scene::addLight)
        .def("light", &Scene::light)
        .def_property_readonly("lights", &Scene::lights)
        .def("addMedium", &Scene::addMedium)
        .def("medium", &Scene::medium)
        .def_property_readonly("media", &Scene::media)
        .def("addEntity", &Scene::addEntity)
        .def("entity", &Scene::entity)
        .def_property_readonly("entities", &Scene::entities)
        .def("addConstantEnvLight", &Scene::addConstantEnvLight)
        .def("addFrom", &Scene::addFrom)
        .def_static(
            "loadFromFile", [](const std::string& path, uint32 flags) {
                return SceneParser().loadFromFile(path, flags);
            },
            py::arg("path"), py::arg("flags") = (uint32)SceneParser::F_LoadAll)
        .def_static(
            "loadFromString", [](const std::string& str, const std::string& dir, uint32 flags) {
                return SceneParser().loadFromString(str, dir, flags);
            },
            py::arg("str"), py::arg("opt_dir") = "", py::arg("flags") = (uint32)SceneParser::F_LoadAll);

    auto parser = py::class_<SceneParser>(m, "SceneParser", "Parser for standard JSON and glTF scene description")
                      .def(py::init<>())
                      .def(
                          "loadFromFile", [](SceneParser& parser, const std::string& path, uint32 flags) {
                              return parser.loadFromFile(path, flags);
                          },
                          py::arg("path"), py::arg("flags") = (uint32)SceneParser::F_LoadAll)
                      .def(
                          "loadFromString", [](SceneParser& parser, const std::string& str, const std::string& dir, uint32 flags) {
                              return parser.loadFromString(str, dir, flags);
                          },
                          py::arg("str"), py::arg("opt_dir") = "", py::arg("flags") = (uint32)SceneParser::F_LoadAll);

    py::enum_<SceneParser::Flags>(parser, "Flags", py::arithmetic(), "Flags modifying parsing behaviour and allowing partial scene loads")
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