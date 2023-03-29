import math
from .utils import *


def export_camera(result, scene):
    camera = scene.camera
    if camera is None:
        return

    matrix = orient_y_up_z_forward(camera.matrix_world, skip_scale=True)

    if camera.data.type == 'ORTHO':
        result["camera"] = {
            "type": "orthogonal",
            "scale": camera.data.ortho_scale / 2,
            "near_clip": camera.data.clip_start,
            "far_clip": camera.data.clip_end,
            "transform": flat_matrix(matrix)
        }
    else:
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
