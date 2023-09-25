import mathutils
import math
import bpy

from .utils import *
from .context import ExportContext, NodeContext
from .emission import get_emission

from .defaults import *


def _get_active_output_world(world: bpy.types.World):
    for node in world.node_tree.nodes:
        if node.type == 'OUTPUT_WORLD' and node.is_active_output:
            return node
    return None


def export_background(result, export_ctx: ExportContext):
    scene = export_ctx.scene

    # It might be that no world is given at all
    if not scene.world:
        return

    # Export basic world if no shading nodes are given
    if not scene.world.node_tree:
        if scene.world.color[0] > 0 or scene.world.color[1] > 0 or scene.world.color[2] > 0:
            result["lights"].append(
                {"type": "env", "name": "__scene_world", "radiance": map_rgb(scene.world.color), "scale": 0.5, "transform": ENVIRONMENT_MAP_TRANSFORM})
        return

    output = _get_active_output_world(scene.world)
    if output is None:
        export_ctx.report_warning(
            f"World {scene.world.name} has no output node")
        return None

    surface = output.inputs.get("Surface")
    if surface is None or not surface.is_linked:
        export_ctx.report_warning(
            f"World {scene.world.name} has no surface node")
        return None

    radiance = get_emission(NodeContext(result, export_ctx), surface)

    if not radiance:
        return

    result["lights"].append({"type": "env", "name": "__scene_world",
                            "radiance": radiance, "transform": ENVIRONMENT_MAP_TRANSFORM})


def export_light(result, inst: bpy.types.DepsgraphObjectInstance):
    light = inst.object
    l = light.data
    power = [l.color[0] * l.energy, l.color[1]
             * l.energy, l.color[2] * l.energy]
    position = inst.matrix_world @ mathutils.Vector((0, 0, 0, 1))
    direction = inst.matrix_world @ mathutils.Vector((0, 0, -1, 0))

    if l.type == "POINT":  # TODO: Support radius
        # factor = 1 / (math.pi * l.radius * l.radius) if l.radius > 0 else 1
        result["lights"].append(
            {"type": "point", "name": light.name, "position": map_vector(
                position), "power": map_rgb(power)}
        )
    elif l.type == "SPOT":  # TODO: Support radius?
        cutoff = l.spot_size / 2
        falloff = math.acos(
            l.spot_blend + (1 - l.spot_blend) * math.cos(cutoff))
        # Ignis allows power as input, but defines it as the actual emitted output. Cycles only scales it by a factor independent of cutoff & falloff
        factor = 1 / (4 * math.pi)
        result["lights"].append(
            {"type": "spot", "name": light.name, "position": map_vector(position), "direction": map_vector(direction.normalized(
            )), "intensity": map_rgb([power[0] * factor, power[1] * factor, power[2] * factor]), "cutoff": math.degrees(cutoff), "falloff": math.degrees(falloff)}
        )
    elif l.type == "SUN":
        if l.angle < 1e-4:
            result["lights"].append(
                {"type": "directional", "name": light.name, "direction": map_vector(
                    direction.normalized()), "irradiance": map_rgb(power)}
            )
        else:
            result["lights"].append(
                {"type": "sun", "name": light.name, "direction": map_vector(
                    direction.normalized()), "irradiance": map_rgb(power), "angle": math.degrees(l.angle)}
            )
    elif l.type == "AREA":
        size_x = l.size
        if l.shape == 'SQUARE':
            size_y = l.size
        else:
            size_y = l.size_y

        if l.shape == "DISK":
            result["shapes"].append(
                {"type": "disk", "name": light.name +
                    "-shape", "radius": size_x/2, "flip_normals": True}
            )
        elif l.shape == "ELLIPSE":
            # Approximate by non-uniformly scaling the uniform disk
            size_y = l.size_y
            result["shapes"].append(
                {"type": "disk", "name": light.name +
                    "-shape", "radius": 1, "flip_normals": True, "transform": [size_x/2, 0, 0, 0, size_y/2, 0, 0, 0, 1]}
            )
        else:
            result["shapes"].append(
                {"type": "rectangle", "name": light.name +
                    "-shape", "width": size_x, "height": size_y, "flip_normals": True}
            )

        entity_dict = {"name": light.name,
                       "shape": light.name + "-shape",
                       "bsdf": BSDF_BLACK_NAME,
                       "transform": flat_matrix(inst.matrix_world)}

        if not light.visible_camera:
            entity_dict["camera_visible"] = False
        if not inst.object.visible_diffuse and not inst.object.visible_glossy and not inst.object.visible_transmission:
            entity_dict["bounce_visible"] = False

        result["entities"].append(entity_dict)

        result["lights"].append(
            {"type": "area", "name": light.name, "entity": light.name,
                "power": map_rgb(power)}
        )
