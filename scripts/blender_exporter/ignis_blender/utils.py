import mathutils
import re


def escape_identifier(name):
    return re.sub('[^a-zA-Z0-9_]', '_', name)


def flat_matrix(matrix):
    return [x for row in matrix for x in row]


def orient_y_up_z_forward(matrix, skip_scale=False):
    loc, rot, sca = matrix.decompose()
    return mathutils.Matrix.LocRotScale(loc, rot @ mathutils.Quaternion((0, 0, 1, 0)), mathutils.Vector.Fill(3, 1) if skip_scale else sca)


def map_rgb(rgb, scale=1):
    return [rgb[0]*scale, rgb[1]*scale, rgb[2]*scale]


def map_vector(vec):
    return [vec.x, vec.y, vec.z]


def try_extract_node_value(value, default=0):
    if type(value) == int or type(value) == float:
        return value
    else:
        return default
