import bpy
import os

from .utils import *


# Extend 2d vector to 3d, as VECTOR is always 3d
TEXCOORD_UV = "vec3(uv.x, uv.y, 0)"


def _export_default(socket):
    default_value = getattr(socket, "default_value")
    if default_value is None:
        print(f"Socket {socket.name} has no default value")
        if socket.type == 'VECTOR':
            return "vec3(0)"
        elif socket.type == 'RGBA':
            return "color(0)"
        else:
            return 0
    else:
        if socket.type == 'VECTOR':
            return f"vec3({default_value[0]}, {default_value[1]}, {default_value[2]})"
        elif socket.type == 'RGBA':
            return f"color({default_value[0]}, {default_value[1]}, {default_value[2]}, {default_value[3]})"
        else:
            return default_value


def _export_scalar_value(result, node, path):
    return node.outputs[0].default_value


def _export_scalar_clamp(result, node, path):
    val = export_node(result, node.inputs[0], path)
    minv = export_node(result, node.inputs[1], path)
    maxv = export_node(result, node.inputs[2], path)

    return f"clamp({val}, {minv}, {maxv})"


def _export_scalar_maprange(result, node, path):
    val = export_node(result, node.inputs[0], path)
    from_min = export_node(result, node.inputs[1], path)
    from_max = export_node(result, node.inputs[2], path)
    to_min = export_node(result, node.inputs[3], path)
    to_max = export_node(result, node.inputs[4], path)

    ops = ""
    from_range = f"{from_max} - {from_min})"
    to_range = f"{to_max} - {to_min})"
    to_unit = f"({val} - {from_min}) / ({from_range})"
    if node.interpolation_type == "LINEAR":
        interp = to_unit
    elif node.interpolation_type == "SMOOTHSTEP":
        interp = f"smoothstep({to_unit})"
    elif node.interpolation_type == "SMOOTHERSTEP":
        interp = f"smootherstep({to_unit})"
    else:
        print(
            f"Not supported interpolation type {node.interpolation_type} for node {node.name}")
        return 0

    ops = f"((({interp}) * {to_range}) + {to_min})"
    if node.clamp:
        return f"clamp({ops}, {to_min}, {to_max})"
    else:
        return ops


def _export_scalar_math(result, node, path):
    ops = ""
    if node.operation == "ADD":
        ops = f"({export_node(result, node.inputs[0], path)} + {export_node(result, node.inputs[1], path)})"
    elif node.operation == "SUBTRACT":
        ops = f"({export_node(result, node.inputs[0], path)} - {export_node(result, node.inputs[1], path)})"
    elif node.operation == "MULTIPLY":
        ops = f"({export_node(result, node.inputs[0], path)} * {export_node(result, node.inputs[1], path)})"
    elif node.operation == "DIVIDE":
        ops = f"({export_node(result, node.inputs[0], path)} / {export_node(result, node.inputs[1], path)})"
    elif node.operation == "MULTIPLY_ADD":
        ops = f"(({export_node(result, node.inputs[0], path)} * {export_node(result, node.inputs[1], path)}) + {export_node(result, node.inputs[2], path)})"
    elif node.operation == "POWER":
        ops = f"(({export_node(result, node.inputs[0], path)})^({export_node(result, node.inputs[1], path)}))"
    elif node.operation == "LOGARITHM":
        ops = f"log({export_node(result, node.inputs[0], path)})"
    elif node.operation == "SQRT":
        ops = f"sqrt({export_node(result, node.inputs[0], path)})"
    elif node.operation == "INVERSE_SQRT":
        ops = f"(1/sqrt({export_node(result, node.inputs[0], path)}))"
    elif node.operation == "ABSOLUTE":
        ops = f"abs({export_node(result, node.inputs[0], path)})"
    elif node.operation == "EXPONENT":
        ops = f"exp({export_node(result, node.inputs[0], path)})"
    elif node.operation == "MINIMUM":
        ops = f"min({export_node(result, node.inputs[0], path)}, {export_node(result, node.inputs[1], path)})"
    elif node.operation == "MAXIMUM":
        ops = f"max({export_node(result, node.inputs[0], path)}, {export_node(result, node.inputs[1], path)})"
    elif node.operation == "LESS_THAN":
        ops = f"select({export_node(result, node.inputs[0], path)} < {export_node(result, node.inputs[1], path)}, 1, 0)"
    elif node.operation == "GREATER_THAN":
        ops = f"select({export_node(result, node.inputs[0], path)} > {export_node(result, node.inputs[1], path)}, 1, 0)"
    elif node.operation == "SIGN":
        # TODO: If value is zero, zero should be returned!
        ops = f"select({export_node(result, node.inputs[0], path)} < 0, -1, 1)"
    elif node.operation == "COMPARE":
        ops = f"select(abs({export_node(result, node.inputs[0], path)} - {export_node(result, node.inputs[1], path)}) <= Eps, 1, 0)"
    elif node.operation == "ROUND":
        ops = f"round({export_node(result, node.inputs[0], path)})"
    elif node.operation == "FLOOR":
        ops = f"floor({export_node(result, node.inputs[0], path)})"
    elif node.operation == "CEIL":
        ops = f"ceil({export_node(result, node.inputs[0], path)})"
    elif node.operation == "TRUNC":
        ops = f"trunc({export_node(result, node.inputs[0], path)})"
    elif node.operation == "FRACTION":
        ops = f"fract({export_node(result, node.inputs[0], path)})"
    elif node.operation == "MODULO":
        ops = f"fmod({export_node(result, node.inputs[0], path)}, {export_node(result, node.inputs[1], path)})"
    elif node.operation == "SMOOTH_MIN":
        ops = f"smin({export_node(result, node.inputs[0], path)}, {export_node(result, node.inputs[1], path)}, {export_node(result, node.inputs[2], path)})"
    elif node.operation == "SMOOTH_MAX":
        ops = f"smax({export_node(result, node.inputs[0], path)}, {export_node(result, node.inputs[1], path)}, {export_node(result, node.inputs[2], path)})"
    elif node.operation == "WRAP":
        ops = f"wrap({export_node(result, node.inputs[0], path)}, {export_node(result, node.inputs[1], path)}, {export_node(result, node.inputs[2], path)})"
    elif node.operation == "SNAP":
        ops = f"snap({export_node(result, node.inputs[0], path)}, {export_node(result, node.inputs[1], path)})"
    elif node.operation == "PINGPONG":
        ops = f"pingpong({export_node(result, node.inputs[0], path)}, {export_node(result, node.inputs[1], path)})"
    elif node.operation == "SINE":
        ops = f"sin({export_node(result, node.inputs[0], path)})"
    elif node.operation == "COSINE":
        ops = f"cos({export_node(result, node.inputs[0], path)})"
    elif node.operation == "TANGENT":
        ops = f"tan({export_node(result, node.inputs[0], path)})"
    elif node.operation == "ARCSINE":
        ops = f"asin({export_node(result, node.inputs[0], path)})"
    elif node.operation == "ARCCOSINE":
        ops = f"acos({export_node(result, node.inputs[0], path)})"
    elif node.operation == "ARCTANGENT":
        ops = f"atan({export_node(result, node.inputs[0], path)})"
    elif node.operation == "SINH":
        ops = f"sinh({export_node(result, node.inputs[0], path)})"
    elif node.operation == "COSH":
        ops = f"cosh({export_node(result, node.inputs[0], path)})"
    elif node.operation == "TANH":
        ops = f"tanh({export_node(result, node.inputs[0], path)})"
    elif node.operation == "ARCTAN2":
        ops = f"atan2({export_node(result, node.inputs[0], path)}, {export_node(result, node.inputs[1], path)})"
    elif node.operation == "RADIANS":
        ops = f"({export_node(result, node.inputs[0], path)} * Pi / 180)"
    elif node.operation == "DEGREES":
        ops = f"({export_node(result, node.inputs[0], path)} * 180 / Pi)"
    else:
        print(
            f"Not supported math operation type {node.operation} for node {node.name}")
        return 0

    if node.use_clamp:
        return f"clamp({ops}, 0, 1)"
    else:
        return ops


def _export_rgb_value(result, node, path):
    default_value = node.outputs[0].default_value
    return f"color({default_value[0]}, {default_value[1]}, {default_value[2]}, {default_value[3]})"


def _export_rgb_math(result, node, path):
    # See https://docs.gimp.org/en/gimp-concepts-layer-modes.html

    fac = export_node(result, node.inputs[0], path)
    col1 = export_node(result, node.inputs[1], path)  # Background (I)
    col2 = export_node(result, node.inputs[2], path)  # Foreground (M)

    ops = ""
    if node.blend_type == "MIX":
        ops = f"mix({col1}, {col2}, {fac})"
    elif node.blend_type == "BURN":
        ops = f"mix_burn({col1}, {col2}, {fac})"
    elif node.blend_type == "DARKEN":
        ops = f"mix({col1}, min({col1}, {col2}), {fac})"
    elif node.blend_type == "LIGHTEN":
        ops = f"mix({col1}, max({col1}, {col2}), {fac})"
    elif node.blend_type == "SCREEN":
        ops = f"mix_screen({col1}, {col2}, {fac})"
    elif node.blend_type == "DODGE":
        ops = f"mix_dodge({col1}, {col2}, {fac})"
    elif node.blend_type == "OVERLAY":
        ops = f"mix_overlay({col1}, {col2}, {fac})"
    elif node.blend_type == "SOFT_LIGHT":
        ops = f"mix_soft({col1}, {col2}, {fac})"
    elif node.blend_type == "LINEAR_LIGHT":
        ops = f"mix_linear({col1}, {col2}, {fac})"
    elif node.blend_type == "HUE":
        ops = f"mix_hue({col1}, {col2}, {fac})"
    elif node.blend_type == "SATURATION":
        ops = f"mix_saturation({col1}, {col2}, {fac})"
    elif node.blend_type == "VALUE":
        ops = f"mix_value({col1}, {col2}, {fac})"
    elif node.blend_type == "COLOR":
        ops = f"mix_color({col1}, {col2}, {fac})"
    elif node.blend_type == "DIFFERENCE":
        ops = f"mix({col1}, abs({col1} - {col2}), {fac})"
    elif node.blend_type == "ADD":
        ops = f"mix({col1}, {col1} + {col2}, {fac})"
    elif node.blend_type == "SUBTRACT":
        ops = f"mix({col1}, {col1} - {col2}, {fac})"
    elif node.blend_type == "MULTIPLY":
        ops = f"mix({col1}, {col1} * {col2}, {fac})"
    elif node.blend_type == "DIVIDE":
        ops = f"mix({col1}, {col1} / ({col2} + color(1)), {fac})"
    else:
        print(
            f"Not supported rgb math operation type {node.operation} for node {node.name}")
        return "color(0)"

    if node.use_clamp:
        return f"clamp({ops}, 0, 1)"
    else:
        return ops


def _export_rgb_gamma(result, node, path):
    color_node = export_node(result, node.inputs[0], path)
    gamma_node = export_node(result, node.inputs[1], path)
    return f"(({color_node})^({gamma_node}))"


def _export_rgb_brightcontrast(result, node, path):
    color = export_node(result, node.inputs["Color"], path)
    bright = export_node(result, node.inputs["Bright"], path)
    contrast = export_node(result, node.inputs["Contrast"], path)

    return f"max(color(0), (1+{contrast})*{color} + color({bright}-{contrast}*0.5))"


def _export_rgb_invert(result, node, path):
    # Only valid if values between 0 and 1

    fac = export_node(result, node.inputs[0], path)
    col1 = export_node(result, node.inputs[1], path)

    ops = f"(color(1) - {col1})"

    if node.inputs[0].is_linked or node.inputs[0].default_value != 1:
        return f"mix({col1}, {ops}, {fac})"
    else:
        return ops


def _export_combine_hsv(result, node, path):
    hue = export_node(result, node.inputs["H"], path)
    sat = export_node(result, node.inputs["S"], path)
    val = export_node(result, node.inputs["V"], path)

    return f"hsvtorgb(color({hue}, {sat}, {val}))"


def _export_combine_rgb(result, node, path):
    r = export_node(result, node.inputs["R"], path)
    g = export_node(result, node.inputs["G"], path)
    b = export_node(result, node.inputs["B"], path)

    return f"color({r}, {g}, {b})"


def _export_combine_xyz(result, node, path):
    x = export_node(result, node.inputs["X"], path)
    y = export_node(result, node.inputs["Y"], path)
    z = export_node(result, node.inputs["Z"], path)

    return f"vec3({x}, {y}, {z})"


def _export_separate_hsv(result, node, path, output_name):
    color = export_node(result, node.inputs["Color"], path)
    ops = f"rgbtohsv({color})"
    if output_name == "H":
        return f"{ops}.r"
    elif output_name == "S":
        return f"{ops}.g"
    else:
        return f"{ops}.b"


def _export_separate_rgb(result, node, path, output_name):
    color = export_node(result, node.inputs["Color"], path)
    if output_name == "R":
        return f"{color}.r"
    elif output_name == "G":
        return f"{color}.g"
    else:
        return f"{color}.b"


def _export_separate_xyz(result, node, path, output_name):
    vec = export_node(result, node.inputs["Vector"], path)
    if output_name == "X":
        return f"{vec}.x"
    elif output_name == "Y":
        return f"{vec}.y"
    else:
        return f"{vec}.z"


def _export_hsv(result, node, path):
    hue = export_node(result, node.inputs[0], path)
    sat = export_node(result, node.inputs[1], path)
    val = export_node(result, node.inputs[2], path)
    fac = export_node(result, node.inputs[3], path)
    col = export_node(result, node.inputs[4], path)

    return f"hsvtorgb(mix(rgbtohsv({col}), color({hue}, {sat}, {val}), {fac}))"


def _export_val_to_rgb(result, node, path):
    val = export_node(result, node.inputs[0], path)
    return f"color({val})"


def _export_rgb_to_bw(result, node, path):
    color = export_node(result, node.inputs["Color"], path)
    return f"luminance({color})"


# TRS
def _export_vector_mapping(result, node, path):
    if node.inputs["Vector"].is_linked:
        vec = export_node(result, node.inputs["Vector"], path)
    else:
        vec = 'vec3(0)'

    sca = export_node(result, node.inputs["Scale"], path)
    rot = export_node(result, node.inputs["Rotation"], path)

    if node.vector_type == 'POINT':
        loc = export_node(result, node.inputs["Location"], path)
        out = f"({vec} * {sca})"
        out = f"rotate_euler({out}, {rot})"
        return f"({out} + {loc})"
    elif node.vector_type == 'TEXTURE':
        loc = export_node(result, node.inputs["Location"], path)
        out = f"({vec} - {loc})"
        out = f"rotate_euler_inverse({out}, {rot})"
        return f"({out} / {sca})"
    elif node.vector_type == 'NORMAL':
        out = f"({vec} / {sca})"
        out = f"rotate_euler({out}, {rot})"
        return f"norm({out})"
    else:
        out = f"({vec} * {sca})"
        return f"rotate_euler({out}, {rot})"


def _export_vector_math(result, node, path):
    if node.operation == "ADD":
        return f"({export_node(result, node.inputs[0], path)} + {export_node(result, node.inputs[1], path)})"
    elif node.operation == "SUBTRACT":
        return f"({export_node(result, node.inputs[0], path)} - {export_node(result, node.inputs[1], path)})"
    elif node.operation == "MULTIPLY":
        return f"({export_node(result, node.inputs[0], path)} * {export_node(result, node.inputs[1], path)})"
    elif node.operation == "SCALE":
        return f"({export_node(result, node.inputs[0], path)} * {export_node(result, node.inputs[1], path)})"
    elif node.operation == "DIVIDE":
        return f"({export_node(result, node.inputs[0], path)} / {export_node(result, node.inputs[1], path)})"
    elif node.operation == "MULTIPLY_ADD":
        return f"(({export_node(result, node.inputs[0], path)} * {export_node(result, node.inputs[1], path)}) + {export_node(result, node.inputs[2], path)})"
    elif node.operation == "CROSS_PRODUCT":
        return f"cross({export_node(result, node.inputs[0], path)}, {export_node(result, node.inputs[1], path)})"
    elif node.operation == "PROJECT":
        return f"project({export_node(result, node.inputs[0], path)}, {export_node(result, node.inputs[1], path)})"
    elif node.operation == "REFLECT":
        return f"reflect({export_node(result, node.inputs[0], path)}, {export_node(result, node.inputs[1], path)})"
    elif node.operation == "REFRACT":
        return f"refract({export_node(result, node.inputs[0], path)}, {export_node(result, node.inputs[1], path)}, {export_node(result, node.inputs[2], path)})"
    elif node.operation == "FACEFORWARD":
        A = export_node(result, node.inputs[0], path)
        return f"select(dot({export_node(result, node.inputs[1], path)}, {export_node(result, node.inputs[2], path)}) < 0, {A}, -{A})"
    elif node.operation == "DOT_PRODUCT":
        return f"dot({export_node(result, node.inputs[0], path)}, {export_node(result, node.inputs[1], path)})"
    elif node.operation == "DISTANCE":
        return f"dist({export_node(result, node.inputs[0], path)}, {export_node(result, node.inputs[1], path)})"
    elif node.operation == "LENGTH":
        return f"length({export_node(result, node.inputs[0], path)})"
    elif node.operation == "NORMALIZE":
        return f"norm({export_node(result, node.inputs[0], path)})"
    elif node.operation == "ABSOLUTE":
        return f"abs({export_node(result, node.inputs[0], path)})"
    elif node.operation == "EXPONENT":
        return f"exp({export_node(result, node.inputs[0], path)})"
    elif node.operation == "MINIMUM":
        return f"min({export_node(result, node.inputs[0], path)}, {export_node(result, node.inputs[1], path)})"
    elif node.operation == "MAXIMUM":
        return f"max({export_node(result, node.inputs[0], path)}, {export_node(result, node.inputs[1], path)})"
    elif node.operation == "WRAP":
        return f"wrap({export_node(result, node.inputs[0], path)}, {export_node(result, node.inputs[1], path)}, {export_node(result, node.inputs[2], path)})"
    elif node.operation == "SNAP":
        return f"snap({export_node(result, node.inputs[0], path)}, {export_node(result, node.inputs[1], path)})"
    elif node.operation == "ROUND":
        return f"round({export_node(result, node.inputs[0], path)})"
    elif node.operation == "FLOOR":
        return f"floor({export_node(result, node.inputs[0], path)})"
    elif node.operation == "CEIL":
        return f"ceil({export_node(result, node.inputs[0], path)})"
    elif node.operation == "FRACTION":
        return f"fract({export_node(result, node.inputs[0], path)})"
    elif node.operation == "MODULO":
        return f"fmod({export_node(result, node.inputs[0], path)}, {export_node(result, node.inputs[1], path)})"
    elif node.operation == "SINE":
        return f"sin({export_node(result, node.inputs[0], path)})"
    elif node.operation == "COSINE":
        return f"cos({export_node(result, node.inputs[0], path)})"
    elif node.operation == "TANGENT":
        return f"tan({export_node(result, node.inputs[0], path)})"
    elif node.operation == "ARCSINE":
        return f"asin({export_node(result, node.inputs[0], path)})"
    elif node.operation == "ARCCOSINE":
        return f"acos({export_node(result, node.inputs[0], path)})"
    elif node.operation == "ARCTANGENT":
        return f"atan({export_node(result, node.inputs[0], path)})"
    elif node.operation == "SINH":
        return f"sinh({export_node(result, node.inputs[0], path)})"
    elif node.operation == "COSH":
        return f"cosh({export_node(result, node.inputs[0], path)})"
    elif node.operation == "TANH":
        return f"tanh({export_node(result, node.inputs[0], path)})"
    elif node.operation == "ARCTAN2":
        return f"atan2({export_node(result, node.inputs[0], path)}, {export_node(result, node.inputs[1], path)})"
    elif node.operation == "RADIANS":
        return f"({export_node(result, node.inputs[0], path)} * Pi / 180)"
    elif node.operation == "DEGREES":
        return f"({export_node(result, node.inputs[0], path)} * 180 / Pi)"
    else:
        print(
            f"Not supported vector math operation type {node.operation} for node {node.name}")
        return "vec3(0)"


def _export_checkerboard(result, node, path, output_name):
    scale = export_node(result, node.inputs["Scale"], path)

    if node.inputs["Vector"].is_linked:
        uv = export_node(result, node.inputs["Vector"], path)
    else:
        uv = TEXCOORD_UV

    raw = f"checkerboard({uv} * {scale})"
    if output_name == "Color":
        color1 = export_node(result, node.inputs["Color1"], path)
        color2 = export_node(result, node.inputs["Color2"], path)
        return f"select({raw} == 1, {color1}, {color2})"
    else:
        return raw


def _export_image_texture(result, node, path):
    tex_name = f"_tex_{escape_identifier(node.name)}"

    # Check if texture is already loaded
    for e in result["textures"]:
        if e["name"] == tex_name:
            return tex_name

    img = node.image
    img_path = img.filepath_raw.replace('//', '')
    if img_path == '':
        img_path = img.name + ".exr"
        img.file_format = "OPEN_EXR"

    # Make sure the image is loaded to memory, so we can write it out
    if not img.has_data:
        img.pixels[0]

    os.makedirs(os.path.join(path, "Textures"), exist_ok=True)

    # Export the texture and store its path
    img_name = os.path.basename(img_path)
    old = img.filepath_raw
    try:
        img.filepath_raw = os.path.join(path, "Textures", img_name)
        img.save()
    finally:  # Never break the scene!
        img.filepath_raw = old

    if node.extension == "EXTEND":
        wrap_mode = "clamp"
    elif node.extension == "CLIP":
        wrap_mode = "clamp"  # Not really supported
    else:
        wrap_mode = "repeat"

    if node.interpolation == "Closest":
        filter_type = "nearest"
    else:
        filter_type = "bilinear"

    result["textures"].append(
        {
            "type": "image",
            "name": tex_name,
            "filename": "Meshes/Textures/"+img_name,
            "wrap_mode": wrap_mode,
            "filter_type": filter_type
        }
    )

    if node.inputs["Vector"].is_linked:
        uv = export_node(result, node.inputs["Vector"], path)
        tex_access = f"{tex_name}(({uv}).xy)"
    else:
        tex_access = tex_name

    return tex_access


def _get_noise_vector(result, node, path):
    if node.inputs["Vector"].is_linked:
        uv = export_node(result, node.inputs["Vector"], path)
    else:
        uv = TEXCOORD_UV

    if node.noise_dimensions == '1D':
        w = export_node(result, node.inputs["W"], path)
        return f"{w}"
    elif node.noise_dimensions == '2D':
        return f"{uv}.xy"
    elif node.noise_dimensions == '3D':
        return f"{uv}"
    else:
        print(f"Four dimensional noise is not supported")
        return f"{uv}"


def _export_white_noise(result, node, path, output_name):
    ops = _get_noise_vector(result, node, path)

    if output_name == "Color":
        return f"cnoise({ops})"
    else:
        return f"noise({ops})"


def _export_noise(result, node, path, output_name):
    # TODO: Missing Detail, Roughness and Distortion
    ops = _get_noise_vector(result, node, path)
    scale = export_node(result, node.inputs["Scale"], path)

    if output_name == "Color":
        return f"cpnoise(abs({ops}*{scale}))"
    else:
        return f"pnoise(abs({ops}*{scale}))"


def _export_voronoi(result, node, path, output_name):
    # TODO: Missing a lot of parameters
    if node.inputs["Vector"].is_linked:
        uv = export_node(result, node.inputs["Vector"], path)
    else:
        uv = TEXCOORD_UV

    ops = f"{uv}.xy"
    if node.voronoi_dimensions != '2D':
        print(f"Voronoi currently only supports 2d vectors")
        return f"{uv}"

    scale = export_node(result, node.inputs["Scale"], path)

    if output_name == "Color":
        return f"cvoronoi(abs({ops}*{scale}))"
    elif output_name == "Position":
        # TODO
        return f"vec3(voronoi(abs({ops}*{scale})))"
    else:
        return f"voronoi(abs({ops}*{scale}))"


def _export_musgrave(result, node, path, output_name):
    # TODO: Missing a lot of parameters and only supports fbm
    if node.inputs["Vector"].is_linked:
        uv = export_node(result, node.inputs["Vector"], path)
    else:
        uv = TEXCOORD_UV

    ops = f"{uv}.xy"
    if node.musgrave_dimensions != '2D':
        print(f"Musgrave currently only supports 2d vectors")
        return f"{uv}"

    scale = export_node(result, node.inputs["Scale"], path)

    return f"fbm(abs({ops}*{scale}))"


def _export_wave(result, node, path, output_name):
    # TODO: Add distortion
    if node.inputs["Vector"].is_linked:
        uv = export_node(result, node.inputs["Vector"], path)
    else:
        uv = TEXCOORD_UV

    scale = export_node(result, node.inputs["Scale"], path)
    uv = f"({uv}*{scale})"

    if node.wave_type == "BANDS":
        if node.bands_direction == "X":
            coord = f"{uv}.x*20"
        elif node.bands_direction == "Y":
            coord = f"{uv}.y*20"
        elif node.bands_direction == "Z":
            coord = f"{uv}.z*20"
        else:  # DIAGONAL
            coord = f"sum({uv})*10"
    else:  # RINGS
        if node.bands_direction == "X":
            coord = f"length({uv}.yz)*20"
        elif node.bands_direction == "Y":
            coord = f"length({uv}.xz)*20"
        elif node.bands_direction == "Z":
            coord = f"length({uv}.xy)*20"
        else:  # DIAGONAL
            coord = f"length({uv})*20"

    phase = export_node(result, node.inputs["Phase Offset"], path)
    coord = f"({coord} + {phase})"

    if node.wave_profile == "SIN":
        ops = f"(0.5 + 0.5 * sin({coord} - Pi/2))"
    elif node.wave_profile == "SAW":
        ops = f"fract(0.5*{coord}/Pi)"
    else:  # TRI
        ops = f"2*abs(0.5*{coord}/Pi - floor(0.5*{coord}/Pi + 0.5))"

    if output_name == "Color":
        return f"color({ops})"
    else:
        return ops


def _export_surface_attributes(result, node, path, output_name):
    if output_name == "Position":
        return "P"
    elif output_name == "Normal":
        return "N"
    else:
        print(f"Given geometry attribute '{output_name}' not supported")
        return "N"


def _export_tex_coordinate(result, node, path, output_name):
    # Currently no other type of output is supported
    if output_name != 'UV':
        print(
            f"Given texture coordinate output '{output_name}' is not supported")
    return TEXCOORD_UV


def _export_node(result, node, path, output_name):
    # Missing:
    # ShaderNodeAttribute, ShaderNodeBlackbody, ShaderNodeBevel, ShaderNodeBump, ShaderNodeCameraData,
    # ShaderNodeCustomGroup, ShaderNodeFloatCurve, ShaderNodeFresnel, ShaderNodeGroup,
    # ShaderNodeHairInfo, ShaderNodeLayerWeight, ShaderNodeLightFalloff,
    # ShaderNodeMapRange, ShaderNodeNormal,
    # ShaderNodeNormalMap, ShaderNodeObjectInfo, ShaderNodeRGBCurve, ShaderNodeScript,
    # ShaderNodeShaderToRGB, ShaderNodeSubsurfaceScattering, ShaderNodeTangent,
    # ShaderNodeTexBrick, ShaderNodeTexEnvironment,
    # ShaderNodeTexGradient, ShaderNodeTexIES, ShaderNodeTexMagic,
    # ShaderNodeTexPointDensity, ShaderNodeTexSky, ShaderNodeUVAlongStroke,
    # ShaderNodeUVMap, ShaderNodeVectorCurve,
    # ShaderNodeVectorDisplacement, ShaderNodeVectorRotate, ShaderNodeVectorTransform,
    # ShaderNodeVertexColor, ShaderNodeWavelength, ShaderNodeWireframe

    # No support planned:
    # ShaderNodeAmbientOcclusion, ShaderNodeLightPath, ShaderNodeOutputAOV,
    # ShaderNodeParticleInfo, ShaderNodeOutputLineStyle, ShaderNodePointInfo, ShaderNodeHoldout

    if isinstance(node, bpy.types.ShaderNodeTexImage):
        return _export_image_texture(result, node, path)
    elif isinstance(node, bpy.types.ShaderNodeTexChecker):
        return _export_checkerboard(result, node, path, output_name)
    elif isinstance(node, bpy.types.ShaderNodeTexCoord):
        return _export_tex_coordinate(result, node, path, output_name)
    elif isinstance(node, bpy.types.ShaderNodeTexNoise):
        return _export_noise(result, node, path, output_name)
    elif isinstance(node, bpy.types.ShaderNodeTexWhiteNoise):
        return _export_white_noise(result, node, path, output_name)
    elif isinstance(node, bpy.types.ShaderNodeTexVoronoi):
        return _export_voronoi(result, node, path, output_name)
    elif isinstance(node, bpy.types.ShaderNodeTexMusgrave):
        return _export_musgrave(result, node, path, output_name)
    elif isinstance(node, bpy.types.ShaderNodeTexWave):
        return _export_wave(result, node, path, output_name)
    elif isinstance(node, bpy.types.ShaderNodeNewGeometry):
        return _export_surface_attributes(result, node, path, output_name)
    elif isinstance(node, bpy.types.ShaderNodeMath):
        return _export_scalar_math(result, node, path)
    elif isinstance(node, bpy.types.ShaderNodeValue):
        return _export_scalar_value(result, node, path)
    elif isinstance(node, bpy.types.ShaderNodeClamp):
        return _export_scalar_clamp(result, node, path)
    elif isinstance(node, bpy.types.ShaderNodeMapRange):
        return _export_scalar_maprange(result, node, path)
    elif isinstance(node, bpy.types.ShaderNodeMixRGB):
        return _export_rgb_math(result, node, path)
    elif isinstance(node, bpy.types.ShaderNodeInvert):
        return _export_rgb_invert(result, node, path)
    elif isinstance(node, bpy.types.ShaderNodeGamma):
        return _export_rgb_gamma(result, node, path)
    elif isinstance(node, bpy.types.ShaderNodeBrightContrast):
        return _export_rgb_brightcontrast(result, node, path)
    elif isinstance(node, bpy.types.ShaderNodeHueSaturation):
        return _export_hsv(result, node, path)
    elif isinstance(node, bpy.types.ShaderNodeRGB):
        return _export_rgb_value(result, node, path)
    elif isinstance(node, bpy.types.ShaderNodeValToRGB):
        return _export_val_to_rgb(result, node, path)
    elif isinstance(node, bpy.types.ShaderNodeRGBToBW):
        return _export_rgb_to_bw(result, node, path)
    elif isinstance(node, bpy.types.ShaderNodeCombineHSV):
        return _export_combine_hsv(result, node, path)
    elif isinstance(node, bpy.types.ShaderNodeCombineRGB):
        return _export_combine_rgb(result, node, path)
    elif isinstance(node, bpy.types.ShaderNodeCombineXYZ):
        return _export_combine_xyz(result, node, path)
    elif isinstance(node, bpy.types.ShaderNodeSeparateHSV):
        return _export_separate_hsv(result, node, path, output_name)
    elif isinstance(node, bpy.types.ShaderNodeSeparateRGB):
        return _export_separate_rgb(result, node, path, output_name)
    elif isinstance(node, bpy.types.ShaderNodeSeparateXYZ):
        return _export_separate_xyz(result, node, path, output_name)
    elif isinstance(node, bpy.types.ShaderNodeMapping):
        return _export_vector_mapping(result, node, path)
    elif isinstance(node, bpy.types.ShaderNodeVectorMath):
        return _export_vector_math(result, node, path)
    else:
        print(
            f"Shader node {node.name} of type {type(node).__name__} is not supported")
        return None


def export_node(result, socket, path):
    if socket.is_linked:
        expr = _export_node(
            result, socket.links[0].from_node, path, socket.links[0].from_socket.name)
        if expr is None:
            return _export_default(socket)

        # Casts are implicitly handled with ShaderNodeValToRGB and ShaderNodeRGBToBW but we still want to make sure ;)
        to_type = socket.type
        from_type = socket.links[0].from_socket.type
        if to_type == from_type:
            return expr
        elif (from_type == 'VALUE' or from_type == 'INT') and to_type == 'RGBA':
            return f"color({expr})"
        elif from_type == 'RGBA' and (to_type == 'VALUE' or to_type == 'INT'):
            return f"luminance({expr})"
        else:
            print(
                f"Socket connection from {socket.links[0].from_socket.name} to {socket.name} requires cast from {from_type} to {to_type} which is not supported")
            return expr
    else:
        return _export_default(socket)
