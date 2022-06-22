import os
import json

from .light import export_light, export_background
from .shape import export_shape
from .camera import export_camera
from .bsdf import export_material, export_error_material
from .utils import *

import bpy


def export_technique(result):
    result["technique"] = {
        "type": "path",
        "max_depth": 64
    }


def export_entity(result, inst):
    if(len(inst.object.material_slots) == 1):
        mat_name = inst.object.material_slots[0].material.name
    elif(len(inst.object.material_slots) > 1):
        print(f"Entity {inst.object.name} has multiple materials associated, but only one is supported. Using first entry")
        mat_name = inst.object.material_slots[0].material.name
    else:
        print(f"Entity {inst.object.name} has no material")
        mat_name = f"{inst.object.name}_bsdf_error"
        export_error_material(result, mat_name)

    matrix = inst.matrix_world
    result["entities"].append(
        {"name": inst.object.name, "shape": inst.object.data.name,
            "bsdf": mat_name, "transform": flat_matrix(matrix)}
    )


def export_all(filepath, result, depsgraph, use_selection):
    result["shapes"] = []
    result["bsdfs"] = []
    result["entities"] = []
    result["lights"] = []
    result["textures"] = []

    # Export all given objects
    exported_shapes = set()

    for material in bpy.data.materials:
        if material.node_tree is None:
            continue
        export_material(result, material, filepath)

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

            export_entity(result, inst)
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
