import mathutils
import math
from .utils import *
from .node import NodeContext
from .emission import get_emission

from .defaults import *


def export_background(result, out_dir, depsgraph, copy_images):
    scene = depsgraph.scene

    # It might be that no world is given at all
    if not scene.world:
        return

    # Export basic world if no shading nodes are given
    if not scene.world.node_tree:
        if scene.world.color[0] > 0 and scene.world.color[1] > 0 and scene.world.color[2] > 0:
            result["lights"].append(
                {"type": "env", "name": "__scene_world", "radiance": map_rgb(scene.world.color), "scale": 0.5, "transform": ENVIRONMENT_MAP_TRANSFORM})
        return

    output = scene.world.node_tree.nodes.get("World Output")
    if output is None:
        print(f"World {scene.world.name} has no output node")
        return None

    surface = output.inputs.get("Surface")
    if surface is None or not surface.is_linked:
        print(f"World {scene.world.name} has no surface node")
        return None

    radiance = get_emission(NodeContext(
        result, out_dir, depsgraph, copy_images), surface)

    if not radiance:
        return

    result["lights"].append({"type": "env", "name": "__scene_world",
                            "radiance": radiance, "transform": ENVIRONMENT_MAP_TRANSFORM})


def export_light(result, inst):
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
                position), "intensity": map_rgb(power)}
        )
    elif l.type == "SPOT":  # TODO: Support radius?
        cutoff = l.spot_size / 2
        falloff = math.acos(
            l.spot_blend + (1 - l.spot_blend) * math.cos(cutoff))
        # factor = 1 / (math.pi * l.radius * l.radius) if l.radius > 0 else 1
        result["lights"].append(
            {"type": "spot", "name": light.name, "position": map_vector(position), "direction": map_vector(direction.normalized(
            )), "intensity": map_rgb(power), "cutoff": math.degrees(cutoff), "falloff": math.degrees(falloff)}
        )
    elif l.type == "SUN":
        result["lights"].append(
            {"type": "direction", "name": light.name, "direction": map_vector(
                direction.normalized()), "irradiance": map_rgb(power)}
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
            area = math.pi * size_x * size_x / 4
        elif l.shape == "ELLIPSE":
            # Approximate by non-uniformly scaling the uniform disk
            size_y = l.size_y
            result["shapes"].append(
                {"type": "disk", "name": light.name +
                    "-shape", "radius": 1, "flip_normals": True, "transform": [size_x/2, 0, 0, 0, size_y/2, 0, 0, 0, 1]}
            )
            area = math.pi * size_x * size_y / 4
        else:
            result["shapes"].append(
                {"type": "rectangle", "name": light.name +
                    "-shape", "width": size_x, "height": size_y, "flip_normals": True}
            )
            area = size_x * size_y

        result["entities"].append(
            {"name": light.name,
             "shape": light.name + "-shape",
             "bsdf": BSDF_BLACK_NAME,
             "transform": flat_matrix(inst.matrix_world),
             "camera_visible": light.visible_camera,
             "bounce_visible": (light.visible_diffuse or light.visible_glossy or light.visible_transmission)}
        )

        factor = 1 / (4 * area)  # No idea why there is the factor 4 in it
        result["lights"].append(
            {"type": "area", "name": light.name, "entity": light.name,
                "radiance": map_rgb([power[0] * factor, power[1] * factor, power[2] * factor])}
        )
