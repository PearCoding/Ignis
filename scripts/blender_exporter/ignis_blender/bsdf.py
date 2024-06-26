import bpy
import math

from .context import NodeContext
from .node import export_node, handle_node_group_begin, handle_node_group_end, handle_node_reroute, handle_node_implicit_mappings
from .utils import *


# Apply normal mapping
def _handle_normal(ctx: NodeContext, bsdf: bpy.types.Node, inner_bsdf: {}):
    name = inner_bsdf["name"]
    inner_name = f"{name}_inner"
    if bsdf.inputs["Normal"].is_linked:
        inner_bsdf["name"] = inner_name
        ctx.result["bsdfs"].append(inner_bsdf)

        normal = export_node(ctx, bsdf.inputs["Normal"])
        if "Tangent" in bsdf.inputs and bsdf.inputs["Tangent"].is_linked:
            tangent = export_node(ctx, bsdf.inputs["Tangent"])
            return {"type": "transform", "name": name, "bsdf": inner_name, "normal": normal, "tangent": tangent}
        else:
            return {"type": "transform", "name": name, "bsdf": inner_name, "normal": normal}
    else:
        return inner_bsdf


def export_error_material(mat_name: str):
    return {"type": "diffuse", "name": mat_name, "reflectance": [1, 0.75, 0.8]}


def export_black_material(mat_name: str):
    return {"type": "diffuse", "name": mat_name, "reflectance": 0}


def _export_diffuse_bsdf(ctx: NodeContext, bsdf: bpy.types.Node, export_name: str):
    reflectance = export_node(ctx, bsdf.inputs["Color"])
    roughness = export_node(ctx, bsdf.inputs["Roughness"])

    has_roughness = try_extract_node_value(roughness, default=1) > 0

    if has_roughness:
        roughness = f"({roughness})^2"  # Square it
        return _handle_normal(ctx, bsdf,
                              {"type": "diffuse", "name": export_name,
                                  "reflectance": reflectance, "roughness": roughness})
    else:
        return _handle_normal(ctx, bsdf,
                              {"type": "diffuse", "name": export_name,
                               "reflectance": reflectance})


def _export_glass_bsdf(ctx: NodeContext, bsdf: bpy.types.Node, export_name: str):
    reflectance = export_node(ctx, bsdf.inputs["Color"])
    if bsdf.distribution == 'SHARP':
        roughness = 0
    else:
        roughness = export_node(ctx, bsdf.inputs["Roughness"])
    ior = export_node(ctx, bsdf.inputs["IOR"])

    has_roughness = try_extract_node_value(roughness, default=1) > 0

    if not has_roughness:
        return _handle_normal(ctx, bsdf,
                              {"type": "dielectric", "name": export_name,
                               "specular_reflectance": reflectance, "specular_transmittance": reflectance, "int_ior": ior})
    else:
        roughness = f"({roughness})^2"  # Square it
        return _handle_normal(ctx, bsdf,
                              {"type": "dielectric", "name": export_name,
                               "specular_reflectance": reflectance, "specular_transmittance": reflectance, "roughness": roughness, "int_ior": ior})  # Square roughness?


def _export_refraction_bsdf(ctx: NodeContext, bsdf: bpy.types.Node, export_name: str):
    # TODO: Need better support for this?
    base_color = export_node(ctx, bsdf.inputs["Color"])
    if bsdf.distribution == 'SHARP':
        roughness = 0
    else:
        roughness = export_node(ctx, bsdf.inputs["Roughness"])
    ior = export_node(ctx, bsdf.inputs["IOR"])
    return _handle_normal(ctx, bsdf,
                          {"type": "principled", "name": export_name,
                           "base_color": base_color, "roughness": roughness, "specular_transmission": 1, "metallic": 0, "ior": ior})


def _export_transparent_bsdf(ctx: NodeContext, bsdf: bpy.types.Node, export_name: str):
    color = export_node(ctx, bsdf.inputs["Color"])
    return {"type": "transparent", "name": export_name, "color": color}


def _export_translucent_bsdf(ctx: NodeContext, bsdf: bpy.types.Node, export_name: str):
    # A simple principled shader
    base_color = export_node(ctx, bsdf.inputs["Color"])

    return _handle_normal(ctx, bsdf,
                          {"type": "principled", "name": export_name,
                           "base_color": base_color, "roughness": 1,
                           "metallic": 0, "diffuse_transmission": 1, "thin": True})


def _export_glossy_bsdf(ctx: NodeContext, bsdf: bpy.types.Node, export_name: str):
    # A simple principled shader
    base_color = export_node(ctx, bsdf.inputs["Color"])

    if bsdf.distribution == 'SHARP':
        roughness = 0
    else:  # Only supports GGX
        roughness = export_node(ctx, bsdf.inputs["Roughness"])

    return _handle_normal(ctx, bsdf,
                          {"type": "principled", "name": export_name,
                           "base_color": base_color, "roughness": roughness,
                           "metallic": 1})


def _map_specular_to_ior(specular):
    value = try_extract_node_value(specular, default=None)
    if value is None:
        return f"(2/(1-sqrt(0.08*{specular}))-1)"
    else:  # Compute actual value to simplify code generation
        return 2 / (1 - math.sqrt(0.08 * value)) - 1


def _export_principled_bsdf(ctx: NodeContext, bsdf: bpy.types.Node, export_name: str):
    base_color = export_node(ctx, bsdf.inputs["Base Color"])
    roughness = export_node(ctx, bsdf.inputs["Roughness"])
    subsurface = export_node(ctx, bsdf.inputs.get("Subsurface", bsdf.inputs.get("Subsurface Weight")))
    metallic = export_node(ctx, bsdf.inputs["Metallic"])
    specular = export_node(ctx, bsdf.inputs.get("Specular", bsdf.inputs.get("Specular Weight")))
    specular_tint = export_node(ctx, bsdf.inputs["Specular Tint"])
    transmission = export_node(ctx, bsdf.inputs.get("Transmission", bsdf.inputs.get("Transmission Weight")))
    anisotropic = export_node(ctx, bsdf.inputs.get("Anisotropic", bsdf.inputs.get("Specular Anisotropic")))
    # anisotropic_rotation = export_node(ctx, bsdf.inputs["Anisotropic Rotation"])
    sheen = export_node(ctx, bsdf.inputs.get("Sheen", bsdf.inputs.get("Sheen Weight")))
    sheen_tint = export_node(ctx, bsdf.inputs["Sheen Tint"])
    clearcoat = export_node(ctx, bsdf.inputs.get("Clearcoat", bsdf.inputs.get("Coat Weight")))
    clearcoat_roughness = export_node(ctx, bsdf.inputs.get("Clearcoat Roughness", bsdf.inputs.get("Coat Roughness")))

    # TODO: Blender 4+ Changed Specular quite drastically

    refr_ior = export_node(ctx, bsdf.inputs["IOR"])
    # Map specular variable to our IOR interpretation
    refl_ior = _map_specular_to_ior(specular) if not specular is None else refr_ior

    return _handle_normal(ctx, bsdf,
                          {"type": "principled", "name": export_name, "base_color": base_color, "metallic": metallic,
                           "roughness": roughness, "anisotropic": anisotropic, "sheen": sheen, "sheen_tint": sheen_tint,
                           "clearcoat": clearcoat, "clearcoat_roughness": clearcoat_roughness, "flatness": subsurface,
                           "specular_transmission": transmission, "specular_tint": specular_tint,
                           "reflective_ior": refl_ior, "refractive_ior": refr_ior})


def _export_add_bsdf(ctx: NodeContext, bsdf: bpy.types.Node, export_name: str):
    mat1 = _export_bsdf(
        ctx, bsdf.inputs[0], export_name + "__1")
    mat2 = _export_bsdf(
        ctx, bsdf.inputs[1], export_name + "__2")

    if mat1 is not None and mat2 is None:
        ctx.report_warning(
            f"Add BSDF {export_name} has one invalid bsdf input. Using first bsdf")
        return mat1
    elif mat1 is None and mat2 is not None:
        ctx.report_warning(
            f"Add BSDF {export_name} has one invalid bsdf input. Using second bsdf")
        return mat2
    elif mat1 is None or mat2 is None:
        ctx.report_error(f"Add BSDF {export_name} has no valid bsdf input")
        return None

    ctx.result["bsdfs"].append(mat1)
    ctx.result["bsdfs"].append(mat2)
    return {"type": "add", "name": export_name,
                           "first": export_name + "__1", "second": export_name + "__2"}


def _export_mix_bsdf(ctx: NodeContext, bsdf: bpy.types.Node, export_name: str):
    mat1 = _export_bsdf(
        ctx, bsdf.inputs[1], export_name + "__1")
    mat2 = _export_bsdf(
        ctx, bsdf.inputs[2], export_name + "__2")
    factor = export_node(ctx, bsdf.inputs[0])

    # TODO: Weighted case?
    if mat1 is not None and mat2 is None:
        ctx.report_warning(
            f"Mix BSDF {export_name} has one invalid bsdf input. Using first bsdf, but ignoring weight")
        return mat1
    elif mat1 is None and mat2 is not None:
        ctx.report_warning(
            f"Mix BSDF {export_name} has one invalid bsdf input. Using second bsdf, but ignoring weight")
        return mat2
    elif mat1 is None or mat2 is None:
        ctx.report_error(f"Mix BSDF {export_name} has no valid bsdf input")
        return None

    ctx.result["bsdfs"].append(mat1)
    ctx.result["bsdfs"].append(mat2)
    return {"type": "blend", "name": export_name,
            "first": export_name + "__1", "second": export_name + "__2", "weight": factor}


def _export_emission_bsdf_black(export_name: str):
    # Emission is handled separately
    return {"type": "diffuse", "name": export_name, "reflectance": 0}


def _export_bsdf(ctx: NodeContext, socket: bpy.types.NodeSocket, name: str):
    if not socket.is_linked:
        return None

    bsdf = socket.links[0].from_node
    output_name = socket.links[0].from_socket.name

    def check_instance(typename):
        return hasattr(bpy.types, typename) and isinstance(bsdf, getattr(bpy.types, typename))

    if bsdf is None:
        ctx.report_error(f"Material {name} has no valid bsdf")
        return None
    elif check_instance("ShaderNodeBsdfDiffuse"):
        expr = _export_diffuse_bsdf(ctx, bsdf, name)
    elif check_instance("ShaderNodeBsdfGlass"):
        expr = _export_glass_bsdf(ctx, bsdf, name)
    elif check_instance("ShaderNodeBsdfRefraction"):
        expr = _export_refraction_bsdf(ctx, bsdf, name)
    elif check_instance("ShaderNodeBsdfTransparent"):
        expr = _export_transparent_bsdf(ctx, bsdf, name)
    elif check_instance("ShaderNodeBsdfTranslucent"):
        expr = _export_translucent_bsdf(ctx, bsdf, name)
    elif check_instance("ShaderNodeBsdfGlossy"):
        expr = _export_glossy_bsdf(ctx, bsdf, name)
    elif check_instance("ShaderNodeBsdfPrincipled"):
        expr = _export_principled_bsdf(ctx, bsdf, name)
    elif check_instance("ShaderNodeMixShader"):
        expr = _export_mix_bsdf(ctx, bsdf, name)
    elif check_instance("ShaderNodeAddShader"):
        expr = _export_add_bsdf(ctx, bsdf, name)
    elif check_instance("ShaderNodeEmission"):
        expr = _export_emission_bsdf_black(name)
    elif check_instance("ShaderNodeGroup"):
        expr = handle_node_group_begin(
            ctx, bsdf, output_name, lambda ctx2, socket2: _export_bsdf(ctx2, socket2, name))
    elif check_instance("NodeGroupInput"):
        expr = handle_node_group_end(
            ctx, bsdf, output_name, lambda ctx2, socket2: _export_bsdf(ctx2, socket2, name))
    elif check_instance("NodeReroute"):
        expr = handle_node_reroute(
            ctx, bsdf, lambda ctx2, socket2: _export_bsdf(ctx2, socket2, name))
    elif isinstance(bsdf, bpy.types.ShaderNode) and socket.links[0].from_socket.type in ['VALUE', 'INT', 'RGBA', 'VECTOR']:
        # Used if a non-shader node is connected directly to the surface output (Implicits already handled)
        return _export_emission_bsdf_black(name)
    else:
        ctx.report_error(
            f"Material {name} has a bsdf of type {type(bsdf).__name__}  which is not supported")
        return None

    return handle_node_implicit_mappings(ctx, socket, expr)


def _get_active_output(material: bpy.types.Material):
    for node in material.node_tree.nodes:
        if node.type == 'OUTPUT_MATERIAL' and node.is_active_output:
            return node
    return None


def _get_bsdf_link(ctx: NodeContext, material: bpy.types.Material):
    if material.node_tree is None:
        ctx.report_warning(f"Material {material.name} has no valid node tree")
        return None

    output = _get_active_output(material)
    if output is None:
        ctx.report_warning(f"Material {material.name} has no output node")
        return None

    surface = output.inputs.get("Surface")
    if surface is None or not surface.is_linked:
        ctx.report_warning(f"Material {material.name} has no surface node")
        return None

    return surface.links[0]


# Will export basic material without node support
def _export_basic_material(material: bpy.types.Material):
    # Map specular variable to our IOR interpretation
    ior = _map_specular_to_ior(material.specular_intensity)

    return {"type": "principled", "name": material.name, "base_color": map_rgb(material.diffuse_color), "metallic": material.metallic,
            "roughness": material.roughness, "ior": ior}


def export_material(ctx: NodeContext, material: bpy.types.Material):
    if not material:
        return None

    if not material.node_tree:
        return _export_basic_material(material)

    link = _get_bsdf_link(ctx, material)
    if not link:
        return None

    return _export_bsdf(ctx, link.to_socket, material.name)
