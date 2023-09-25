import bpy
from .context import ExportContext
from .emission import get_material_emission
from .node import NodeContext
from .defaults import *
from .utils import *


def export_entity(result, ctx: ExportContext, inst: bpy.types.DepsgraphObjectInstance, shape_name: str, mat_i: int):
    shadow_visibility = True
    if ctx.settings.export_materials:
        if(len(inst.object.material_slots) > mat_i):
            inst_mat = inst.object.material_slots[mat_i].material
            if inst_mat is not None:
                result["_materials"].add(inst_mat)
                mat_name = inst_mat.name
                emission = get_material_emission(NodeContext(
                    result, ctx), inst_mat)
                if bpy.context.engine == "EEVEE":
                    shadow_visibility = inst_mat.shadow_method != "NONE"
            else:
                ctx.report_warning(
                    f"Obsolete material slot {mat_i} for shape {shape_name} with instance {inst.object.data.name}")
                mat_name = BSDF_BLACK_NAME
                emission = None
        else:
            ctx.report_warning(f"Entity {inst.object.name} has no material")
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
    if (emission is not None) and ctx.settings.export_lights:
        result["lights"].append(
            {"type": "area", "name": entity_name, "entity": entity_name,
                "radiance": emission}
        )
