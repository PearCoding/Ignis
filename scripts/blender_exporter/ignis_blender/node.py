from numpy import isin
import bpy
import os

from .utils import *


def _export_default(result, socket, factor=1, asLight=False):
    default_value = getattr(socket, "default_value")
    if default_value is None:
        print(f"Socket {socket.name} has no default value")
        return 0
    else:
        try:  # Try color
            return map_rgb(default_value)
        except Exception:
            return default_value


def _export_scalar_value(result, node, path):
    return node.outputs[0].default_value


def _export_scalar_clamp(result, node, path):
    clamp_type = node.clamp_type  # TODO: ???

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
    if node.interpolation_type == "LINEAR":
        from_range = f"{from_max} - {from_min})"
        to_range = f"{to_max} - {to_min})"
        to_unit = f"({val} - {from_min}) / ({from_range})"
        ops = f"((({to_unit}) * {to_range}) + {to_min})"
    else:
        print(
            f"Not supported interpolation type {node.interpolation_type} for node {node.name}")
        return 0

    if node.clamp:
        return f"clamp({ops}, {to_min}, {to_max})"
    else:
        return ops


def _export_scalar_math(result, node, path):
    # TODO: LESS_THAN, GREATER_THAN, SIGN, COMPARE, SMOOTH_MIN, SMOOTH_MAX, TRUNC, FRACT, MODULO, WRAP, SNAP, PINGPONG
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
        ops = f"pow({export_node(result, node.inputs[0], path)}, {export_node(result, node.inputs[1], path)})"
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
    elif node.operation == "ROUND":
        ops = f"round({export_node(result, node.inputs[0], path)})"
    elif node.operation == "FLOOR":
        ops = f"floor({export_node(result, node.inputs[0], path)})"
    elif node.operation == "CEIL":
        ops = f"ceil({export_node(result, node.inputs[0], path)})"
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
    return map_rgb(node.outputs[0].default_value)


def _export_rgb_math(result, node, path):
    # See https://docs.gimp.org/en/gimp-concepts-layer-modes.html
    # TODO: HUE, SATURATION, COLOR, VALUE

    fac = export_node(result, node.inputs[0], path)
    col1 = export_node(result, node.inputs[1], path)  # Background (I)
    col2 = export_node(result, node.inputs[2], path)  # Foreground (M)

    ops = ""
    if node.blend_type == "MIX":
        ops = col2
    elif node.blend_type == "BURN":  # Only valid if between 0 1
        ops = f"burn({col1}, {col2})"
    elif node.blend_type == "DARKEN":
        ops = f"min({col1}, {col2})"
    elif node.blend_type == "LIGHTEN":
        ops = f"max({col1}, {col2})"
    elif node.blend_type == "SCREEN":  # Only valid if between 0 1
        ops = f"screen({col1}, {col2})"
    elif node.blend_type == "DODGE":  # Only valid if between 0 1
        ops = f"dodge({col1}, {col2})"
    elif node.blend_type == "OVERLAY":   # Only valid if between 0 1
        ops = f"overlay({col1}, {col2})"
    elif node.blend_type == "SOFT_LIGHT":   # Only valid if between 0 1
        ops = f"softlight({col1}, {col2})"
    elif node.blend_type == "LINEAR_LIGHT":  # Only valid if between 0 1
        ops = f"hardlight({col1}, {col2})"
    elif node.blend_type == "DIFFERENCE":
        ops = f"max(0, {col1} - {col2})"
    elif node.blend_type == "ADD":
        ops = f"({col1} + {col2})"
    elif node.blend_type == "SUBTRACT":
        ops = f"({col1} - {col2})"
    elif node.blend_type == "MULTIPLY":
        ops = f"({col1} * {col2})"
    elif node.blend_type == "DIVIDE":
        ops = f"({col1} / ({col2} + 1))"
    else:
        print(
            f"Not supported rgb math operation type {node.operation} for node {node.name}")
        return 0

    if node.inputs[0].is_linked or node.inputs[0].default_value != 1:  # TODO: Prevent copying?
        ops = f"lerp({fac}, {col1}, {ops})"

    if node.use_clamp:
        return f"clamp({ops}, 0, 1)"
    else:
        return ops


def _export_rgb_gamma(result, node, path):
    color_node = export_node(result, node.inputs[0], path)
    gamma_node = export_node(result, node.inputs[1], path)
    return f"({color_node} * exp({gamma_node}))"


def _export_rgb_brightcontrast(result, node, path):
    # TODO
    color_node = export_node(result, node.inputs[0], path)
    bright_node = export_node(result, node.inputs[1], path)
    contrast_node = export_node(result, node.inputs[2], path)

    return 0


def _export_rgb_invert(result, node, path):
    # Only valid if values between 0 and 1

    fac = export_node(result, node.inputs[0], path)
    col1 = export_node(result, node.inputs[1], path)

    ops = f"(1-{col1})"
    if node.inputs[0].is_linked or node.inputs[0].default_value != 1:
        return f"lerp({fac}, {col1}, {ops})"
    else:
        return ops


def _export_hsv(result, node, path):
    hue = export_node(result, node.inputs[0], path)
    sat = export_node(result, node.inputs[1], path)
    val = export_node(result, node.inputs[2], path)
    fac = export_node(result, node.inputs[3], path)
    col = export_node(result, node.inputs[4], path)

    # TODO
    return 0


def _export_rgb(result, node, path):
    # TODO
    return [0, 0, 0]


def _export_blackbody(result, node, path):
    # TODO
    return 0


def _export_checkerboard(result, node, path):
    color1 = export_node(result, node.inputs["Color1"], path)
    color2 = export_node(result, node.inputs["Color2"], path)
    scale = export_node(result, node.inputs["Scale"], path)
    # TODO
    return 0


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

    # TODO: Add uv mapping!
    if img.channels == 1:
        return f"{tex_name}.r"
    else:
        return tex_name


def _export_node(result, socket, node, path):
    if isinstance(node, bpy.types.ShaderNodeTexImage):
        return _export_image_texture(result, node, path)
    elif isinstance(node, bpy.types.ShaderNodeTexChecker):
        return _export_checkerboard(result, node, path)
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
    elif isinstance(node, bpy.types.ShaderNodeRGB):
        return _export_rgb_value(result, node, path)
    elif isinstance(node, bpy.types.ShaderNodeGamma):
        return _export_rgb_gamma(result, node, path)
    elif isinstance(node, bpy.types.ShaderNodeBrightContrast):
        return _export_rgb_brightcontrast(result, node, path)
    elif isinstance(node, bpy.types.ShaderNodeBlackbody):
        return _export_blackbody(result, node, path)
    elif isinstance(node, bpy.types.ShaderNodeHueSaturation):
        return _export_hsv(result, node, path)
    elif isinstance(node, bpy.types.ShaderNodeValToRGB):
        return _export_rgb(result, node, path)
    else:
        print(
            f"Shader node {node.name} of type {type(node).__name__} is not supported")
        return ""


def export_node(result, socket, path):
    if socket.is_linked:
        return _export_node(result, socket, socket.links[0].from_node, path)
    else:
        return _export_default(result, socket, path)
