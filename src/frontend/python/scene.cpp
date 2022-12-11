#include <pybind11/eigen.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "Scene.h"

namespace py = pybind11;
using namespace IG;

void scene_module(py::module_& m)
{
    auto prop = py::class_<SceneProperty>(m, "SceneProperty")
                    .def_property_readonly("type", &SceneProperty::type)
                    .def("isValid", &SceneProperty::isValid)
                    .def("canBeNumber", &SceneProperty::canBeNumber)
                    .def("getBool", &SceneProperty::getBool)
                    .def_static("fromBool", &SceneProperty::fromBool)
                    .def("getInteger", &SceneProperty::getInteger)
                    .def_static("fromInteger", &SceneProperty::fromInteger)
                    .def("getIntegerArray", &SceneProperty::getIntegerArray)
                    .def_static("fromIntegerArray", py::overload_cast<const SceneProperty::IntegerArray&>(&SceneProperty::fromIntegerArray))
                    .def("getNumber", &SceneProperty::getNumber)
                    .def_static("fromNumber", &SceneProperty::fromNumber)
                    .def("getNumberArray", &SceneProperty::getNumberArray)
                    .def_static("fromNumberArray", py::overload_cast<const SceneProperty::NumberArray&>(&SceneProperty::fromNumberArray))
                    .def("getString", &SceneProperty::getString)
                    .def_static("fromString", &SceneProperty::fromString)
                    .def("getTransform", &SceneProperty::getTransform)
                    .def_static("fromTransform", &SceneProperty::fromTransform)
                    .def("getVector2", &SceneProperty::getVector2)
                    .def_static("fromVector2", &SceneProperty::fromVector2)
                    .def("getVector3", &SceneProperty::getVector3)
                    .def_static("fromVector3", &SceneProperty::fromVector3);

    py::enum_<SceneProperty::Type>(prop, "Type")
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

    auto obj = py::class_<SceneObject, std::shared_ptr<SceneObject>>(m, "SceneObject")
                   .def(py::init([](SceneObject::Type type, const std::string& pType, const std::string& dir) {
                       return SceneObject(type, pType, dir);
                   }))
                   .def_property_readonly("type", &SceneObject::type)
                   .def_property_readonly("baseDir", [](const SceneObject& o) { return o.baseDir().generic_u8string(); })
                   .def_property_readonly("pluginType", &SceneObject::pluginType)
                   .def("property", &SceneObject::property)
                   .def("setProperty", &SceneObject::setProperty)
                   .def_property_readonly("properties", &SceneObject::properties);

    py::enum_<SceneObject::Type>(obj, "Type")
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

    py::class_<Scene, std::shared_ptr<Scene>>(m, "Scene")
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
        .def("addFrom", &Scene::addFrom);
}