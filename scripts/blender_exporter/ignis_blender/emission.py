import bpy

from .node import export_node, handle_node_group_begin, handle_node_group_end, handle_node_reroute
from .utils import *
from .bsdf import _get_bsdf_link


def _get_emission_mix(ctx, bsdf):
    mat1 = _get_emission_inline(
        ctx, bsdf.inputs[0])
    mat2 = _get_emission_inline(
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
    mat1 = _get_emission_inline(
        ctx, bsdf.inputs[0])
    mat2 = _get_emission_inline(
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


def _get_emission(ctx, bsdf, output_name):
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
        return handle_node_group_begin(ctx, bsdf, output_name, _get_emission_inline)
    elif isinstance(bsdf, bpy.types.NodeGroupInput):
        return handle_node_group_end(ctx, bsdf, output_name, _get_emission_inline)
    elif isinstance(bsdf, bpy.types.NodeReroute):
        return handle_node_reroute(ctx, bsdf, _get_emission_inline)
    else:
        return None


def _get_emission_inline(ctx, socket):
    if not socket.is_linked:
        return None

    return _get_emission(ctx, socket.links[0].from_node, socket.links[0].from_socket.name)


def get_material_emission(ctx, material):
    if not material:
        return None

    if not material.node_tree:
        return None

    link = _get_bsdf_link(material)
    if not link:
        return None

    return _get_emission(ctx, link.from_node, link.from_socket.name)
