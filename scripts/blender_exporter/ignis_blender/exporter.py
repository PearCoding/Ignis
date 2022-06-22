import os
import json
import math

from .material import convert_material

import mathutils

import bpy

from io_mesh_ply.export_ply import save_mesh as ply_save


def flat_matrix(matrix):
    return [x for row in matrix for x in row]


def orient_y_up_z_forward(matrix, skip_scale=False):
    loc, rot, sca = matrix.decompose()
    return mathutils.Matrix.LocRotScale(loc, rot @ mathutils.Quaternion((0, 0, 1, 0)), mathutils.Vector.Fill(3, 1) if skip_scale else sca)


def map_rgb(rgb, scale=1):
    return [rgb[0]*scale, rgb[1]*scale, rgb[2]*scale]


def map_vector(vec):
    return [vec.x, vec.y, vec.z]


def map_texture(texture, out_dir, result):
    path = texture.filepath_raw.replace('//', '')
    if path == '':
        path = texture.name + ".exr"
        texture.file_format = "OPEN_EXR"

    # Make sure the image is loaded to memory, so we can write it out
    if not texture.has_data:
        texture.pixels[0]

    os.makedirs(os.path.join(out_dir, "Textures"), exist_ok=True)

    # Export the texture and store its path
    name = os.path.basename(path)
    old = texture.filepath_raw
    try:
        texture.filepath_raw = os.path.join(out_dir, "Textures", name)
        texture.save()
    finally:  # Never break the scene!
        texture.filepath_raw = old

    result["textures"].append(
        {
            "type": "image",
            "name": name[:-4],
            "filename": "Meshes/Textures/"+name,
        }
    )

    return name[:-4]


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


def export_shape(result, obj, meshpath):
    import bmesh

    bm = bmesh.new()

    try:
        me = obj.to_mesh()
    except RuntimeError:
        return

    bm.from_mesh(me)
    obj.to_mesh_clear()

    bm.normal_update()

    ply_save(filepath=os.path.join(meshpath, obj.data.name+".ply"),
             bm=bm,
             use_ascii=False,
             use_normals=True,
             use_uv=True,
             use_color=False,
             )

    bm.free()

    result["shapes"].append(
        {"type": "ply", "name": obj.data.name,
            "filename": "Meshes/" + obj.data.name + ".ply"},
    )


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


# Make sure the Z axis is handled as up, instead of Y
ENVIRONMENT_MAP_TRANSFORM = [0, 1, 0, 0, 0, 1, 1, 0, 0]


def export_background(result, out_dir, scene):
    if "Background" not in scene.world.node_tree.nodes:
        return

    tree = scene.world.node_tree.nodes["Background"]

    if tree.type == "BACKGROUND":
        # TODO: Add strength parameter
        input = tree.inputs["Color"]

        if input.is_linked:
            # Export the background as texture and add it as environmental light
            tex_node = input.links[0].from_node
            if tex_node.image is not None:
                tex = map_texture(tex_node.image, out_dir, result)
                result["lights"].append(
                    {"type": "env", "name": tex, "radiance": tex, "scale": 0.5, "transform": ENVIRONMENT_MAP_TRANSFORM})
        elif input.type == "RGB" or input.type == "RGBA":
            color = input.default_value
            if color[0] > 0 or color[1] > 0 or color[2] > 0:
                result["lights"].append(
                    {"type": "env", "name": "__scene_world", "radiance": map_rgb(color)})


def export_camera(result, scene):
    camera = scene.camera
    if camera is None:
        return

    matrix = orient_y_up_z_forward(camera.matrix_world, skip_scale=True)

    result["camera"] = {
        "type": "perspective",
        "fov": math.degrees(2 * math.atan(camera.data.sensor_width / (2 * camera.data.lens))),
        "near_clip": camera.data.clip_start,
        "far_clip": camera.data.clip_end,
        "transform": flat_matrix(matrix)
    }

    res_x = scene.render.resolution_x * scene.render.resolution_percentage * 0.01
    res_y = scene.render.resolution_y * scene.render.resolution_percentage * 0.01
    result["film"] = {"size": [res_x, res_y]}


def export_light(result, inst):
    light = inst.object
    l = light.data
    power = [l.color[0] * l.energy, l.color[1]
             * l.energy, l.color[2] * l.energy]
    position = inst.matrix_world @ mathutils.Vector((0, 0, 0, 1))
    direction = inst.matrix_world @ mathutils.Vector((0, 0, 1, 0))

    if l.type == "POINT":
        result["lights"].append(
            {"type": "point", "name": light.name, "position": map_vector(
                position), "intensity": map_rgb(power)}
        )
    elif l.type == "SPOT":
        result["lights"].append(
            {"type": "spot", "name": light.name, "position": map_vector(position), "direction": map_vector(direction.normalized(
            )), "intensity": map_rgb(power), "cutoff": math.degrees(l.spot_size/2), "falloff": math.degrees(l.spot_size/2)*(1 - l.spot_blend)}
        )
    elif l.type == "SUN":
        result["lights"].append(
            {"type": "direction", "name": light.name, "direction": map_vector(
                -direction.normalized()), "irradiance": map_rgb(power)}
        )
    elif l.type == "AREA":
        size_x = l.size
        if l.shape == 'SQUARE':
            size_y = l.size
        else:
            size_y = l.size_y

        result["shapes"].append(
            {"type": "rectangle", "name": light.name +
                "-shape", "width": size_x, "height": size_y, "flip_normals": True}
        )
        result["bsdfs"].append(
            {"type": "diffuse", "name": light.name +
                "-bsdf", "reflectance": [0, 0, 0]}
        )
        result["entities"].append(
            {"name": light.name, "shape": light.name +
                "-shape", "bsdf": light.name+"-bsdf", "transform": flat_matrix(inst.matrix_world)}
        )
        result["lights"].append(
            {"type": "area", "name": light.name, "entity": light.name,
                "radiance": map_rgb(power)}
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
