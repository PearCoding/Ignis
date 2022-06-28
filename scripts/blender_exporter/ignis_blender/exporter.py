import os
import json

from .light import export_light, export_background
from .shape import export_shape, get_shape_name_base
from .camera import export_camera
from .bsdf import export_material, export_error_material, export_black_material, get_material_emission
from .node import NodeContext
from .utils import *
from .defaults import *

import bpy


def export_technique(result):
    result["technique"] = {
        "type": "path",
        "max_depth": 64
    }


def export_entity(result, inst, filepath, shape_name, mat_i, export_materials, export_lights):
    if export_materials:
        if(len(inst.object.material_slots) >= mat_i):
            result["_materials"].add(
                inst.object.material_slots[mat_i].material)
            mat_name = inst.object.material_slots[mat_i].material.name
            emission = get_material_emission(NodeContext(
                result, filepath), inst.object.material_slots[mat_i].material)
        else:
            print(f"Entity {inst.object.name} has no material")
            mat_name = BSDF_BLACK_NAME
            emission = None
    else:
        mat_name = BSDF_DEFAULT_NAME
        emission = None

    # Export actual entity
    matrix = inst.matrix_world
    entity_name = f"{inst.object.name}-{shape_name}"
    result["entities"].append(
        {"name": entity_name, "shape": shape_name,
            "bsdf": mat_name, "transform": flat_matrix(matrix)}
    )

    # Export entity as area light if necessary
    if (emission is not None) and export_lights:
        result["lights"].append(
            {"type": "area", "name": entity_name, "entity": entity_name,
                "radiance": emission}
        )


def export_all(filepath, result, depsgraph, use_selection, export_materials, export_lights):
    result["shapes"] = []
    result["bsdfs"] = []
    result["entities"] = []
    result["lights"] = []
    result["textures"] = []

    # Export all given objects
    exported_shapes = {}

    # Export default materials
    result["bsdfs"].append(export_black_material(BSDF_BLACK_NAME))
    result["bsdfs"].append(export_error_material(BSDF_ERROR_NAME))

    # Export entities & shapes
    for inst in depsgraph.object_instances:
        object_eval = inst.object
        if use_selection and not object_eval.original.select_get():
            continue
        if not use_selection and not inst.show_self:
            continue

        objType = object_eval.type
        if objType == "MESH" or objType == "CURVE" or objType == "SURFACE":
            name = get_shape_name_base(object_eval)
            if name in exported_shapes:
                shapes = exported_shapes[name]
            else:
                shapes = export_shape(result, object_eval, depsgraph, filepath)
                exported_shapes[name] = shapes

            for shape in shapes:
                export_entity(result, inst, filepath, shape[0], shape[1],
                              export_materials, export_lights)
        elif objType == "LIGHT" and export_lights:
            export_light(
                result, inst)

    # Export materials
    if export_materials:
        for material in result["_materials"]:
            mat = export_material(NodeContext(result, filepath), material)
            if mat is not None:
                result["bsdfs"].append(mat)
            else:
                result["bsdfs"].append(export_error_material(material.name))
    else:
        result["bsdfs"].append({"type": "diffuse", "name": BSDF_DEFAULT_NAME})


def export_scene(filepath, context, use_selection, export_materials, export_lights, enable_background, enable_camera, enable_technique):
    depsgraph = context.evaluated_depsgraph_get()

    # Write all materials and cameras to a dict with the layout of our json file
    result = {}
    # This will not be exported, but removed later on
    result["_images"] = set()
    result["_materials"] = set()

    # Export technique (set to default path tracing with 64 rays)
    if enable_technique:
        export_technique(result)

    # Export camera
    if enable_camera:
        export_camera(result, depsgraph.scene)

    # Create a path for meshes
    meshPath = os.path.join(os.path.dirname(filepath), 'Meshes')
    os.makedirs(meshPath, exist_ok=True)

    # Export all objects, materials, textures and lights
    export_all(meshPath, result, depsgraph, use_selection,
               export_materials, export_lights)

    if enable_background:
        # Export background/environmental light
        export_background(result, meshPath, depsgraph.scene)

    # Cleanup
    result["_materials"] = None
    result["_images"] = None
    result = {key: value for key, value in result.items(
    ) if not isinstance(value, list) or len(value) > 0}

    # Write the result into the .json
    with open(filepath, 'w') as fp:
        json.dump(result, fp, indent=2)
