import os
import json
from math import degrees, atan, tan
from .material import convert_material

import mathutils

import bpy


def flat_matrix(matrix):
    # Spread the matrix out to a list of elements
    xss = [list(row) for row in matrix]
    return [x for xs in xss for x in xs]


def map_rgb(rgb):
    return [rgb[0], rgb[1], rgb[2]]


def map_texture(texture, out_dir, result, background, global_matrix):
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

    # Incase we are exporting a background, rotate it according to the global matrix
    if(background):
        rotation = global_matrix.to_quaternion()
        result["textures"].append(
            {
                "type": "image",
                "name": name[:-4],
                "filename": "Meshes/Textures/"+name,
                "transform": {"qrotate": [rotation.w, rotation.x, rotation.y, rotation.z]}
            }
        )
    else:
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
        base_color = map_texture(material.base_texture, out_dir, result, False)
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


def export_all(filepath, result, context, scene, depsgraph, use_selection, use_modifiers, global_matrix):
    # TODO: use_modifiers

    # Update materials
    for material in list(bpy.data.materials):
        if material.node_tree is None:
            continue
        convert_material(material)

    result["shapes"] = []
    result["bsdfs"] = []
    result["entities"] = []
    result["lights"] = []
    result["textures"] = []

    # Save selection
    selection = context.selected_objects

    # Iterate through selected objects or all
    if use_selection:
        objects = selection
    else:
        objects = scene.objects

    # Export all given objects
    for i in list(objects):
        objType = i.type
        if(objType != "CAMERA" and objType != "LIGHT"):
            # Deselect all objects -> select the one we want to export -> export it as ply and add it as a shape -> add its materials as bsdfs -> connect the shapes and bsdfs as entities.

            bpy.ops.object.select_all(action='DESELECT')
            i.select_set(True)
            bpy.ops.export_mesh.ply(filepath=os.path.join(
                filepath, i.data.name+".ply"), check_existing=False, axis_forward='Y', axis_up='Z', use_selection=True, use_mesh_modifiers=True, use_normals=True, use_uv_coords=True, use_colors=False)
            result["shapes"].append(
                {"type": "ply", "name": i.name,
                    "filename": "Meshes/" + i.name + ".ply"},
            )

            if(len(i.material_slots) > 0):
                material = i.material_slots[0].material.ignis
                mat_name = i.material_slots[0].material.name

                result["bsdfs"].append(material_to_json(
                    material, mat_name, filepath, result))
                result["entities"].append(
                    {"name": i.name, "shape": i.name, "bsdf": mat_name,
                     "transform": flat_matrix(global_matrix)}
                )

        elif(objType == "LIGHT"):
            l = i.data
            if(l.type == "POINT"):
                result["lights"].append(
                    {"type": "point", "name": i.name, "transform": {"translate": [
                        i.location.x, i.location.y, i.location.z]}, "intensity": [l.energy, l.energy, l.energy]}
                )

            elif(l.type == "SPOT"):
                result["lights"].append(
                    {"type": "spot", "name": i.name, "transform": {"translate": [i.location.x, i.location.y, i.location.z]}, "direction": [degrees(
                        i.rotation_euler.x), degrees(i.rotation_euler.z) + 180, degrees(i.rotation_euler.y)], "intensity": [l.energy, l.energy, l.energy]}
                )

            elif(l.type == "SUN"):
                result["lights"].append(
                    {"type": "sun", "name": i.name, "transform": {"translate": [i.location.x, i.location.y, i.location.z]}, "direction": [
                        degrees(i.rotation_euler.x), degrees(i.rotation_euler.z) + 180, degrees(i.rotation_euler.y)], "sun_scale": l.energy}
                )

            elif(l.type == "AREA"):
                # We need to construct a world matrix that factors in 180 degree rotation around y and scaling proportional to light size (we apply similar process to the camera):

                # Decompose the world matrix of the object which contains info about the location, rotation and scale
                loc, rot, sca = i.matrix_world.decompose()

                # Construct loction matrix without applying any changes to the loction
                mat_loc = mathutils.Matrix.Translation(loc)

                # Construct scale matrix and multiply it with size of l divided by 2 to account for the light size
                mat_sca = mathutils.Matrix.Identity(4)
                mat_sca[0][0] = sca[0]
                mat_sca[1][1] = sca[1]
                mat_sca[2][2] = sca[2]
                mat_sca[3][3] = 1
                mat_sca *= (l.size/2)

                # We apply 180 degree rotation around the y axis to the original rotation matrix since blender uses -Z as forward direction while Ignis uses Z.
                # This is done by multiplying the quaternion with (0,1,0,0) which effectively is done by exchanging elements and negating some of them.
                new_rot = [-rot[2], -rot[3], rot[0], rot[1]]
                mat_rot = mathutils.Quaternion(new_rot).to_matrix().to_4x4()

                # Construnction final 4X4 matrix
                mat_out = mat_loc @ mat_rot @ mat_sca

                result["shapes"].append(
                    {"type": "rectangle", "name": i.name +
                        "-shape", "transform": flat_matrix(mat_out)}
                )
                result["bsdfs"].append(
                    {"type": "diffuse", "name": i.name +
                        "-bsdf", "reflectance": [0, 0, 0]}
                )
                result["entities"].append(
                    {"name": i.name, "shape": i.name +
                        "-shape", "bsdf": i.name+"-bsdf"}
                )
                result["lights"].append(
                    {"type": "area", "name": i.name, "entity": i.name,
                        "radiance": [l.energy, l.energy, l.energy]}
                )

    # Restore selection
    bpy.ops.object.select_all(action='DESELECT')
    for obj in selection:
        obj.select_set(True)


def export_background(result, out_dir, scene, camera_matrix):
    tree = scene.world.node_tree.nodes["Background"]
    if tree.type == "BACKGROUND":
        # TODO: Add strength parameter
        input = tree.inputs["Color"]

        if input.is_linked:
            # Export the background as texture and add it as environmental light
            tex_node = input.links[0].from_node
            if tex_node.image is not None:
                tex = map_texture(tex_node.image, out_dir, result, True)
                result["lights"].append(
                    {"type": "env", "name": tex, "radiance": tex, "scale": [0.5, 0.5, 0.5]})
        elif input.type == "RGB" or input.type == "RGBA":
            color = input.default_value
            if color[0] > 0 or color[1] > 0 or color[2] > 0:
                result["lights"].append(
                    {"type": "env", "name": "__scene_world", "radiance": map_rgb(color)})


def export_camera(result, scene, camera_matrix):
    camera = scene.camera
    aspect_ratio = scene.render.resolution_y / scene.render.resolution_x

    # Construnction final 4X4 matrix
    matrix = camera_matrix @ camera.matrix_world

    result["camera"] = {
        "type": "perspective",
        "fov": degrees(2 * atan(camera.data.sensor_width / (2 * camera.data.lens))),
        "near_clip": camera.data.clip_start,
        "far_clip": camera.data.clip_end,
        "transform": flat_matrix(matrix)
    }

    res_x = scene.render.resolution_x * scene.render.resolution_percentage * 0.01
    res_y = scene.render.resolution_y * scene.render.resolution_percentage * 0.01
    result["film"] = {"size": [res_x, res_y]}


def export_scene(filepath, context, use_selection, use_modifiers, global_matrix, camera_matrix):
    scene = context.scene
    depsgraph = context.evaluated_depsgraph_get()

    # Write all materials and cameras to a dict with the layout of our json file
    result = {}

    # Export technique (set to default path tracing with 64 rays)
    export_technique(result)

    # Export camera
    export_camera(result, scene, camera_matrix)

    # Create a path for meshes
    meshPath = os.path.join(os.path.dirname(filepath), 'Meshes')
    os.makedirs(meshPath, exist_ok=True)

    # Export all objects, materials, textures and lights
    export_all(meshPath, result, context, scene, depsgraph,
               use_selection, use_modifiers, global_matrix)

    # Export background/environmental light
    export_background(result, meshPath, scene, global_matrix)

    # Write the result into the .json
    with open(filepath, 'w') as fp:
        json.dump(result, fp, indent=2)
