import os
import json
from math import degrees, atan, tan
from .material import convert_material
import math
import mathutils

from numpy import append
import bpy
from bpy_extras.io_utils import ExportHelper
from bpy.props import StringProperty, BoolProperty
from bpy.types import Operator
from io_scene_fbx.export_fbx_bin import save_single


def map_rgb(rgb):
    return { "type": "rgb", "value": [ rgb[0], rgb[1], rgb[2] ] }

def map_texture(texture, out_dir, result, background):
    path = texture.filepath_raw.replace('//', '')
    if path == '':
        path = texture.name + ".exr"
        texture.file_format = "OPEN_EXR"

    # Make sure the image is loaded to memory, so we can write it out
    if not texture.has_data:
        texture.pixels[0]

    os.makedirs(f"{out_dir}/Textures", exist_ok=True)

    # Export the texture and store its path
    name = os.path.basename(path)
    old = texture.filepath_raw
    try:
        texture.filepath_raw = f"{out_dir}/Textures/{name}"
        texture.save()
    finally: # Never break the scene!
        texture.filepath_raw = old

    # Incase we are exporting a background, rotate it 180 degrees around the y axis to match the result in blender
    if(background):
        result["textures"].append(
            {
                "type": "image",
                "name": name[:-4],
                "filename": "Meshes/Textures/"+name,
                "transform": {"rotate":[0,180]}
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
    

    return  name[:-4]

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

def export_all(filepath, result):

    #Update materials
    for material in list(bpy.data.materials):
            if material.node_tree is None: continue
            convert_material(material)

    result["shapes"] = []
    result["bsdfs"] = []
    result["entities"] = []
    result["lights"] = []
    result["textures"] = []

    for i in list(bpy.context.scene.objects):
        objType = i.type
        if(objType != "CAMERA" and objType != "LIGHT"):
            # Deselect all objects -> select the one we want to export -> export it as ply and add it as a shape -> add its materials as bsdfs -> connect the shapes and bsdfs as entities.

            bpy.ops.object.select_all(action='DESELECT')
            i.select_set(True)
            bpy.ops.export_mesh.ply(filepath=filepath+i.data.name+".ply", axis_forward='Y', axis_up='Z', use_selection=True)
            result["shapes"].append(
                {"type":"ply", "name": i.name, "filename" : "Meshes/" + i.name + ".ply"},
            )

            if(len(i.material_slots) > 0):
                material = i.material_slots[0].material.ignis
                mat_name = i.material_slots[0].material.name
                
                result["bsdfs"].append(material_to_json(material, mat_name, filepath, result))
                result["entities"].append(
                    {"name":i.name, "shape":i.name, "bsdf": mat_name}
                )        

        elif(objType == "LIGHT"):
            l = i.data
            if(l.type == "POINT"):
                result["lights"].append(
                    {"type": "point", "name":i.name, "transform":{"translate":[i.location.x, i.location.y, i.location.z]}, "intensity": [l.energy, l.energy, l.energy]}
                    )

            elif(l.type == "SPOT"):
                result["lights"].append(
                    {"type": "spot", "name":i.name, "transform":{"translate":[i.location.x, i.location.y, i.location.z]}, "direction": [degrees(i.rotation_euler.x), degrees(i.rotation_euler.z) + 180, degrees(i.rotation_euler.y)], "intensity": [l.energy, l.energy, l.energy]}
                    )

            elif(l.type == "SUN"):
                result["lights"].append(
                    {"type": "sun", "name":i.name, "transform":{"translate":[i.location.x, i.location.y, i.location.z]}, "direction": [degrees(i.rotation_euler.x), degrees(i.rotation_euler.z) + 180, degrees(i.rotation_euler.y)], "sun_scale": l.energy}
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

                # Spread the matrix out to a list of elements
                xss = [list(row) for row in mat_out]
                flat_matrix = [x for xs in xss for x in xs]

                result["shapes"].append(
                    {"type": "rectangle", "name": i.name+"-shape", "transform":flat_matrix}
                    )
                result["bsdfs"].append(
                    {"type": "diffuse", "name": i.name+"-bsdf", "reflectance": [0,0,0]}
                    )
                result["entities"].append(
                    {"name":i.name, "shape": i.name+"-shape", "bsdf": i.name+"-bsdf"}
                    )
                result["lights"].append(
                    {"type": "area", "name":i.name, "entity": i.name, "radiance":[l.energy, l.energy, l.energy]}
                    )


def export_background(result, out_dir, scene):
    tree = scene.world.node_tree

    if "Environment Texture" in tree.nodes:
        # Export the background as texture and add it as environmental light
        tex = map_texture(scene.world.node_tree.nodes["Environment Texture"].image, out_dir, result, True)
        result["lights"].append({"type": "env", "name": tex, "radiance": tex, "scale":[0.5,0.5,0.5]})
    elif "RGB" in tree.nodes:
        color = scene.world.node_tree.nodes["RGB"].outputs[0].default_value
        result["lights"].append({"type": "env", "name": "rgb", "radiance": [color[0], color[1], color[2]]})

def export_camera(result, scene):
    camera = scene.camera
    aspect_ratio = scene.render.resolution_y / scene.render.resolution_x

    # We need to construct a world matrix that factors in 180 degree rotation around y (we apply similar process to the area light):

    # Decompose the world matrix of the object which contains info about the location, rotation and scale
    loc, rot, sca = camera.matrix_world.decompose()

    # Construct loction matrix without applying any changes to the loction
    mat_loc = mathutils.Matrix.Translation(loc)

    # Construct scale matrix without applying any changes to the scale
    mat_sca = mathutils.Matrix.Identity(4)
    mat_sca[3][3] = 1

    # We apply 180 degree rotation around the y axis to the original rotation matrix since blender uses -Z as forward direction while Ignis uses Z.
    # This is done by multiplying the quaternion with (0,1,0,0) which effectively is done by exchanging elements and negating some of them.
    new_rot = [-rot[2], -rot[3], rot[0], rot[1]]
    mat_rot = mathutils.Quaternion(new_rot).to_matrix().to_4x4()

    # Construnction final 4X4 matrix
    mat_out = mat_loc @ mat_rot @ mat_sca

    # Spread the matrix out to a list of elements
    xss = [list(row) for row in mat_out]
    flat_matrix = [x for xs in xss for x in xs]

    result["camera"] = {
            "type": "perspective",
            "fov" : degrees(2 * atan(camera.data.sensor_width / (2 * camera.data.lens))),
            "near_clip" : 0.1,
            "far_clip" : 100,
            "transform": flat_matrix
            
        }

    result["film"] = {"size" : [scene.render.resolution_x, scene.render.resolution_y]}
    

def export_scene(filepath, scene, depsgraph):

    # Write all materials and cameras to a dict with the layout of our json file
    result = {}

    # Export technique (set to default path tracing with 64 rays)
    export_technique(result)

    # Export camera
    export_camera(result, scene)

    # Create a path for meshes
    objPath = filepath[:filepath.rindex('\\')+1] + 'Meshes'
    os.mkdir(objPath)

    # Export all objects, materials, textures and lights
    export_all(objPath + '\\', result)
    
    # Export background/environmental light
    export_background(result, objPath + '\\', scene)

    # Write the result into the .json
    with open(filepath, 'w') as fp:
        json.dump(result, fp, indent=2)

class IgnisExport(Operator, ExportHelper):
    """Ignis scene exporter"""
    bl_idname = "export.to_ignis"
    bl_label = "Ignis Scene Exporter"

    # ExportHelper mixin class uses this
    filename_ext = ".json"

    filter_glob: StringProperty(
        default="*.json",
        options={'HIDDEN'},
        maxlen=255,  # Max internal buffer length, longer would be clamped.
    )

    animations: BoolProperty(
        name="Export Animations",
        description="If true, writes .json and .obj files for each frame in the animation.",
        default=False,
    )

    def execute(self, context):
        if self.animations is True:
            for frame in range(context.scene.frame_start, context.scene.frame_end+1):
                context.scene.frame_set(frame)
                depsgraph = context.evaluated_depsgraph_get()
                export_scene(self.filepath.replace('.json', f'{frame:04}.json'), context.scene, depsgraph)
        else:
            depsgraph = context.evaluated_depsgraph_get()
            export_scene(self.filepath, context.scene, depsgraph)
        return {'FINISHED'}

def menu_func_export(self, context):
    self.layout.operator(IgnisExport.bl_idname, text="Ignis Export")

def register():
    bpy.utils.register_class(IgnisExport)
    bpy.types.TOPBAR_MT_file_export.append(menu_func_export)

def unregister():
    bpy.utils.unregister_class(IgnisExport)
    bpy.types.TOPBAR_MT_file_export.remove(menu_func_export)

if __name__ == "__main__":
    register()

    # test call
    bpy.ops.export.to_ignis('INVOKE_DEFAULT')
