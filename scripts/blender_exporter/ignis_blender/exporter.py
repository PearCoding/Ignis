import os
import json

from .material import convert_material
from .light import export_light, export_background
from .shape import export_shape
from .camera import export_camera
from .utils import *

import bpy


def material_to_json(material, mat_name, out_dir, result):
    # Create a principled material out of any type of material by calculating its properties.

    if material.base_texture:
        base_color = map_texture(material.base_texture, out_dir, result)
    else:
        base_color = map_rgb(material.base_color)

    return {
        "name": mat_name,
        "type": "principled",
        "base_color":  base_color,
        "roughness": material.roughness,
        "anisotropic": material.anisotropic,
        "diffuse_transmittance": material.diffuseTransmittance,
        "ior": material.indexOfRefraction,
        "metallic": material.metallic,
        "specular_tint": material.specularTintStrength,
        "specular_transmission": material.specularTransmittance,
        "thin": material.thin,
    }


def export_technique(result):
    result["technique"] = {
        "type": "path",
        "max_depth": 64
    }


def export_entity(result, inst, filepath):
    if(len(inst.object.material_slots) > 0):
        material = inst.object.material_slots[0].material.ignis
        mat_name = inst.object.material_slots[0].material.name

        matrix = inst.matrix_world
        result["bsdfs"].append(material_to_json(
            material, mat_name, filepath, result))
        result["entities"].append(
            {"name": inst.object.name, "shape": inst.object.data.name,
                "bsdf": mat_name, "transform": flat_matrix(matrix)}
        )


def export_all(filepath, result, depsgraph, use_selection):
    # TODO: use_modifiers

    # Update materials
    for material in bpy.data.materials:
        if material.node_tree is None:
            continue
        convert_material(material)

    result["shapes"] = []
    result["bsdfs"] = []
    result["entities"] = []
    result["lights"] = []
    result["textures"] = []

    # Export all given objects
    exported_shapes = set()

    for inst in depsgraph.object_instances:
        object_eval = inst.object
        if use_selection and not object_eval.original.select_get():
            continue
        if not use_selection and not inst.show_self:
            continue

        objType = object_eval.type
        if objType == "MESH":
            shape_name = object_eval.data.name
            if shape_name not in exported_shapes:
                export_shape(result, object_eval, filepath)
                exported_shapes.add(shape_name)

            export_entity(result, inst, filepath)
        elif objType == "LIGHT":
            export_light(
                result, inst)


def export_scene(filepath, context, use_selection):
    depsgraph = context.evaluated_depsgraph_get()

    # Write all materials and cameras to a dict with the layout of our json file
    result = {}

    # Export technique (set to default path tracing with 64 rays)
    export_technique(result)

    # Export camera
    export_camera(result, depsgraph.scene)

    # Create a path for meshes
    meshPath = os.path.join(os.path.dirname(filepath), 'Meshes')
    os.makedirs(meshPath, exist_ok=True)

    # Export all objects, materials, textures and lights
    export_all(meshPath, result, depsgraph, use_selection)

    # Export background/environmental light
    export_background(result, meshPath, depsgraph.scene)

    # Cleanup
    result = {key: value for key, value in result.items(
    ) if not isinstance(value, list) or len(value) > 0}

    # Write the result into the .json
    with open(filepath, 'w') as fp:
        json.dump(result, fp, indent=2)
