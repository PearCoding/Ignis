import bpy
import os
import json

from .context import ExportContext, NodeContext
from .light import export_light, export_background
from .shape import export_shape, get_shape_name_base
from .camera import export_camera
from .technique import export_technique
from .entity import export_entity
from .bsdf import export_material, export_error_material, export_black_material
from .defaults import *
from .addon_preferences import get_prefs


def export_all(result, export_ctx: ExportContext):
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
    for inst in export_ctx.depsgraph.object_instances:
        object_eval = inst.object
        if object_eval is None:
            continue
        if export_ctx.settings.use_selection and not object_eval.original.select_get():
            continue
        if not export_ctx.settings.use_selection and not inst.show_self:
            continue

        objType = object_eval.type
        if objType in {'MESH', 'CURVE', 'SURFACE', 'META', 'FONT', 'CURVES'}:
            name = get_shape_name_base(object_eval, inst)
            if name in exported_shapes:
                shapes = exported_shapes[name]
            else:
                shapes = export_shape(
                    result, name, object_eval, export_ctx)
                exported_shapes[name] = shapes

            if len(shapes) == 0:
                export_ctx.report_warning(
                    f"Entity {object_eval.name} has no material or shape and will be ignored")

            for shape in shapes:
                export_entity(result, export_ctx, inst, shape[0], shape[1])
        elif objType == "LIGHT" and export_ctx.settings.export_lights:
            export_light(
                result, inst)

    # Export materials
    if export_ctx.settings.export_materials:
        for material in result["_materials"]:
            mat = export_material(NodeContext(result, export_ctx), material)
            if mat is not None:
                result["bsdfs"].append(mat)
            else:
                result["bsdfs"].append(export_error_material(material.name))
    else:
        result["bsdfs"].append({"type": "diffuse", "name": BSDF_DEFAULT_NAME})


def delete_none(_dict):
    """Delete None values and empty sets recursively from all of the dictionaries, tuples, lists, sets"""
    if isinstance(_dict, dict):
        for key, value in list(_dict.items()):
            if isinstance(value, (list, dict, tuple, set)):
                val = delete_none(value)
                if val is not None:
                    _dict[key] = val
                else:
                    del _dict[key]
            elif value is None or key is None:
                del _dict[key]

    elif isinstance(_dict, (list, set, tuple)):
        _dict = type(_dict)(delete_none(item)
                            for item in _dict if item is not None)
        if len(_dict) == 0:
            _dict = None

    return _dict


def export_scene(filepath, op: bpy.types.Operator | bpy.types.RenderEngine, context, settings):
    depsgraph = context.evaluated_depsgraph_get() if not isinstance(
        context, bpy.types.Depsgraph) else context

    rootPath = os.path.dirname(filepath)
    export_ctx = ExportContext(rootPath, op, depsgraph, settings)

    # Write all materials and cameras to a dict with the layout of our json file
    result = {}
    # This will not be exported, but removed later on
    result["_images"] = set()
    result["_image_textures"] = dict()
    result["_materials"] = set()

    # Export technique (set to default path tracing with 64 rays)
    if settings.enable_technique:
        export_technique(result, export_ctx)

    # Export camera
    if settings.enable_camera:
        export_camera(result, export_ctx)

    # Create a path for meshes & textures
    meshDir = os.path.join(rootPath, get_prefs().mesh_dir_name)
    texDir = os.path.join(rootPath, get_prefs().tex_dir_name)
    os.makedirs(meshDir, exist_ok=True)
    os.makedirs(texDir, exist_ok=True)

    # Export all objects, materials, textures and lights
    export_all(result, export_ctx)

    if settings.enable_background:
        # Export background/environmental light
        export_background(result, export_ctx)

    # Cleanup
    del result["_images"]
    del result["_image_textures"]
    del result["_materials"]
    result = delete_none(result)

    # Remove mesh & texture directory if empty
    try:
        if len(os.listdir(meshDir)) == 0:
            os.rmdir(meshDir)
        if len(os.listdir(texDir)) == 0:
            os.rmdir(texDir)
    except:
        pass  # Ignore any errors

    return result


def export_scene_to_file(filepath, op, context, settings):
    result = export_scene(filepath, op, context, settings)

    # Write the result into the .json
    with open(filepath, 'w') as fp:
        json.dump(result, fp, indent=2)
