import bpy
import os
import json

from .light import export_light, export_background
from .shape import export_shape, get_shape_name_base
from .camera import export_camera
from .bsdf import export_material, export_error_material, export_black_material
from .emission import get_material_emission
from .node import NodeContext
from .utils import *
from .defaults import *
from .addon_preferences import get_prefs


def export_technique(result, scene):
    if getattr(scene, 'ignis', None) is not None and bpy.context.engine == 'IGNIS_RENDER':
        if scene.ignis.integrator == 'PATH':
            result["technique"] = {
                "type": "path",
                "max_depth": scene.ignis.max_ray_depth,
                "clamp": scene.ignis.clamp_value
            }
            return
        elif scene.ignis.integrator == 'VOLPATH':
            result["technique"] = {
                "type": "volpath",
                "max_depth": scene.ignis.max_ray_depth,
                "clamp": scene.ignis.clamp_value
            }
            return
        elif scene.ignis.integrator == 'PPM':
            result["technique"] = {
                "type": "ppm",
                "max_depth": scene.ignis.max_ray_depth,  # TODO
                "clamp": scene.ignis.clamp_value,
                "photons": scene.ignis.ppm_photons_per_pass
            }
            return
        elif scene.ignis.integrator == 'AO':
            result["technique"] = {"type": "ao"}
            return
    elif getattr(scene, 'cycles', None) is not None and bpy.context.engine == 'CYCLES':
        max_depth = scene.cycles.max_bounces
        clamp = max(scene.cycles.sample_clamp_direct,
                    scene.cycles.sample_clamp_indirect)
    elif getattr(scene, 'eevee', None) is not None and bpy.context.engine == 'BLENDER_EEVEE':
        max_depth = scene.eevee.gi_diffuse_bounces
        clamp = scene.eevee.gi_glossy_clamp
    else:
        max_depth = 8
        clamp = 0

    result["technique"] = {
        "type": "path",
        "max_depth": max_depth,
        "clamp": clamp
    }


def export_entity(result, depsgraph, inst, filepath, shape_name, mat_i, settings):
    shadow_visibility = True
    if settings.export_materials:
        if(len(inst.object.material_slots) > mat_i):
            result["_materials"].add(
                inst.object.material_slots[mat_i].material)
            mat_name = inst.object.material_slots[mat_i].material.name
            emission = get_material_emission(NodeContext(
                result, filepath, depsgraph, settings), inst.object.material_slots[mat_i].material)
            if bpy.context.engine == "EEVEE":
                shadow_visibility = inst.object.material_slots[mat_i].material.shadow_method != "NONE"
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
    entity_dict = {"name": entity_name, "shape": shape_name,
                   "bsdf": mat_name, "transform": flat_matrix(matrix)}

    if not shadow_visibility or not inst.object.visible_shadow:
        entity_dict["shadow_visible"] = False
    if not inst.object.visible_camera:
        entity_dict["camera_visible"] = False
    if not inst.object.visible_diffuse and not inst.object.visible_glossy and not inst.object.visible_transmission:
        entity_dict["bounce_visible"] = False

    result["entities"].append(entity_dict)

    # Export entity as area light if necessary
    if (emission is not None) and settings.export_lights:
        result["lights"].append(
            {"type": "area", "name": entity_name, "entity": entity_name,
                "radiance": emission}
        )


def export_all(filepath, result, depsgraph, settings):
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
        if object_eval is None:
            continue
        if settings.use_selection and not object_eval.original.select_get():
            continue
        if not settings.use_selection and not inst.show_self:
            continue

        objType = object_eval.type
        if objType in {'MESH', 'CURVE', 'SURFACE', 'META', 'FONT', 'CURVES'}:
            name = get_shape_name_base(object_eval, inst)
            if name in exported_shapes:
                shapes = exported_shapes[name]
            else:
                shapes = export_shape(
                    result, name, object_eval, depsgraph, filepath, settings)
                exported_shapes[name] = shapes

            if len(shapes) == 0:
                print(
                    f"Entity {object_eval.name} has no material or shape and will be ignored")

            for shape in shapes:
                export_entity(result, depsgraph, inst, filepath,
                              shape[0], shape[1], settings)
        elif objType == "LIGHT" and settings.export_lights:
            export_light(
                result, inst)

    # Export materials
    if settings.export_materials:
        for material in result["_materials"]:
            mat = export_material(NodeContext(
                result, filepath, depsgraph, settings), material)
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


def export_scene(filepath, context, settings):
    depsgraph = context.evaluated_depsgraph_get() if not isinstance(
        context, bpy.types.Depsgraph) else context

    # Write all materials and cameras to a dict with the layout of our json file
    result = {}
    # This will not be exported, but removed later on
    result["_images"] = set()
    result["_image_textures"] = dict()
    result["_materials"] = set()

    # Export technique (set to default path tracing with 64 rays)
    if settings.enable_technique:
        export_technique(result, depsgraph.scene)

    # Export camera
    if settings.enable_camera:
        export_camera(result, depsgraph.scene)

    # Create a path for meshes & textures
    rootPath = os.path.dirname(filepath)
    meshDir = os.path.join(rootPath, get_prefs().mesh_dir_name)
    texDir = os.path.join(rootPath, get_prefs().tex_dir_name)
    os.makedirs(meshDir, exist_ok=True)
    os.makedirs(texDir, exist_ok=True)

    # Export all objects, materials, textures and lights
    export_all(rootPath, result, depsgraph, settings)

    if settings.enable_background:
        # Export background/environmental light
        export_background(result, rootPath, depsgraph, settings)

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


def export_scene_to_file(filepath, context, settings):
    result = export_scene(filepath, context, settings)

    # Write the result into the .json
    with open(filepath, 'w') as fp:
        json.dump(result, fp, indent=2)
