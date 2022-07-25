import bpy

from .node import export_node, handle_node_group_begin, handle_node_group_end, handle_node_reroute
from .utils import *
from .bsdf import _get_bsdf_link


def _get_emission_mix(ctx, bsdf):
    mat1 = get_emission(
        ctx, bsdf.inputs[0])
    mat2 = get_emission(
        ctx, bsdf.inputs[1])
    factor = export_node(ctx, bsdf.inputs["Fac"])

    if mat1 is None and mat2 is None:
        return None

    if mat1 is None:
        mat1 = "color(0)"
    if mat2 is None:
        mat2 = "color(0)"

    mix_f = try_extract_node_value(factor, default=-1)
    if mix_f == 0:
        return mat1
    elif mix_f == 1:
        return mat2
    else:
        return f"mix({mat1}, {mat2}, {factor})"


def _get_emission_add(ctx, bsdf):
    mat1 = get_emission(
        ctx, bsdf.inputs[0])
    mat2 = get_emission(
        ctx, bsdf.inputs[1])

    if mat1 is None:
        return mat2
    if mat2 is None:
        return mat1

    return f"({mat1} + {mat2})"


def _get_emission_principled(ctx, bsdf):
    color_n = bsdf.inputs["Emission"]
    if check_socket_if_black(color_n):
        return None

    strength_n = bsdf.inputs["Emission Strength"]
    if check_socket_if_black(strength_n):
        return None

    color = export_node(ctx, color_n)
    strength = export_node(ctx, strength_n)

    if try_extract_node_value(strength, default=0) == 1:
        return color
    else:
        return f"({color} * {strength})"


def _get_emission_pure(ctx, bsdf):
    color_n = bsdf.inputs["Color"]
    if check_socket_if_black(color_n):
        return None

    strength_n = bsdf.inputs["Strength"]
    if check_socket_if_black(strength_n):
        return None

    color = export_node(ctx, color_n)
    strength = export_node(ctx, strength_n)

    if try_extract_node_value(strength, default=0) == 1:
        return color
    else:
        return f"({color} * {strength})"


def get_emission(ctx, socket):
    if not socket.is_linked:
        return None

    bsdf = socket.links[0].from_node
    output_name = socket.links[0].from_socket.name

    if isinstance(bsdf, bpy.types.ShaderNodeMixShader):
        return _get_emission_mix(ctx, bsdf)
    elif isinstance(bsdf, bpy.types.ShaderNodeAddShader):
        return _get_emission_add(ctx, bsdf)
    elif isinstance(bsdf, bpy.types.ShaderNodeBsdfPrincipled):
        return _get_emission_principled(ctx, bsdf)
    elif isinstance(bsdf, bpy.types.ShaderNodeEmission):
        return _get_emission_pure(ctx, bsdf)
    elif isinstance(bsdf, bpy.types.ShaderNodeBackground):  # Same as emission node
        return _get_emission_pure(ctx, bsdf)
    elif isinstance(bsdf, bpy.types.ShaderNodeGroup):
        return handle_node_group_begin(ctx, bsdf, output_name, get_emission)
    elif isinstance(bsdf, bpy.types.NodeGroupInput):
        return handle_node_group_end(ctx, bsdf, output_name, get_emission)
    elif isinstance(bsdf, bpy.types.NodeReroute):
        return handle_node_reroute(ctx, bsdf, get_emission)
    elif isinstance(bsdf, bpy.types.ShaderNode) and socket.links[0].from_socket.type in ['VALUE', 'INT', 'RGBA', 'VECTOR']:
        # Used if a non-shader node is connected directly to the surface output
        return export_node(ctx, socket)
    else:
        return None


def get_material_emission(ctx, material):
    if not material:
        return None

    if not material.node_tree:
        return None

    link = _get_bsdf_link(material)
    if not link:
        return None

    return get_emission(ctx, link.to_socket)
