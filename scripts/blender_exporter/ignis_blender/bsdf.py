import bpy
import math

from .node import export_node, handle_node_group_begin, handle_node_group_end, handle_node_reroute
from .utils import *


# Apply normal mapping
def _handle_normal(ctx, bsdf, inner_bsdf):
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


def export_error_material(mat_name):
    return {"type": "diffuse", "name": mat_name, "reflectance": [1, 0.75, 0.8]}


def export_black_material(mat_name):
    return {"type": "diffuse", "name": mat_name, "reflectance": 0}


def _export_diffuse_bsdf(ctx, bsdf, export_name):
    reflectance = export_node(ctx, bsdf.inputs["Color"])
    roughness = export_node(ctx, bsdf.inputs["Roughness"])

    has_roughness = try_extract_node_value(roughness, default=1) > 0

    if has_roughness:
        return _handle_normal(ctx, bsdf,
                              {"type": "roughdiffuse", "name": export_name,
                                  "reflectance": reflectance, "roughness": roughness})
    else:
        return _handle_normal(ctx, bsdf,
                              {"type": "diffuse", "name": export_name,
                               "reflectance": reflectance})


def _export_glass_bsdf(ctx, bsdf, export_name):
    reflectance = export_node(ctx, bsdf.inputs["Color"])
    roughness = export_node(ctx, bsdf.inputs["Roughness"])
    ior = export_node(ctx, bsdf.inputs["IOR"])

    has_roughness = try_extract_node_value(roughness, default=1) > 0

    if not has_roughness:
        return _handle_normal(ctx, bsdf,
                              {"type": "dielectric", "name": export_name,
                               "specular_reflectance": reflectance, "specular_transmittance": reflectance, "ext_ior": ior})
    else:
        return _handle_normal(ctx, bsdf,
                              {"type": "roughdielectric", "name": export_name,
                               "specular_reflectance": reflectance, "specular_transmittance": reflectance, "roughness": roughness, "ext_ior": ior})


def _export_refraction_bsdf(ctx, bsdf, export_name):
    base_color = export_node(ctx, bsdf.inputs["Color"])
    roughness = export_node(ctx, bsdf.inputs["Roughness"])
    ior = export_node(ctx, bsdf.inputs["IOR"])
    return _handle_normal(ctx, bsdf,
                          {"type": "principled", "name": export_name,
                           "base_color": base_color, "roughness": roughness, "specular_transmission": 1, "metallic": 1, "ior": ior})


def _export_transparent_bsdf(ctx, bsdf, export_name):
    reflectance = export_node(ctx, bsdf.inputs["Color"])
    return {"type": "dielectric", "name": export_name,
            "specular_reflectance": reflectance, "specular_transmittance": reflectance, "ext_ior": 1, "int_ior": 1}


def _export_translucent_bsdf(ctx, bsdf, export_name):
    # A simple principled shader
    base_color = export_node(ctx, bsdf.inputs["Color"])

    return _handle_normal(ctx, bsdf,
                          {"type": "principled", "name": export_name,
                           "base_color": base_color, "roughness": 1,
                           "metallic": 0, "diffuse_transmission": 1, "thin": True})


def _export_glossy_bsdf(ctx, bsdf, export_name):
    # A simple principled shader
    base_color = export_node(ctx, bsdf.inputs["Color"])
    roughness = export_node(ctx, bsdf.inputs["Roughness"])

    return _handle_normal(ctx, bsdf,
                          {"type": "principled", "name": export_name,
                           "base_color": base_color, "roughness": roughness,
                           "metallic": 1})


def _map_specular_to_ior(specular):
    value = try_extract_node_value(specular, default=None)
    if not value:
        return f"((1 + sqrt(0.08*{specular})) / max(0.001, 1 - sqrt(0.08*{specular})))"
    else:  # Compute actual value to simplify code generation
        return (1 + math.sqrt(0.08*value)) / max(0.001, 1 - math.sqrt(0.08*value))


def _export_principled_bsdf(ctx, bsdf, export_name):
    base_color = export_node(ctx, bsdf.inputs["Base Color"])
    roughness = export_node(ctx, bsdf.inputs["Roughness"])
    subsurface = export_node(ctx, bsdf.inputs["Subsurface"])
    metallic = export_node(ctx, bsdf.inputs["Metallic"])
    specular = export_node(ctx, bsdf.inputs["Specular"])
    specular_tint = export_node(ctx, bsdf.inputs["Specular Tint"])
    transmission = export_node(ctx, bsdf.inputs["Transmission"])
    anisotropic = export_node(ctx, bsdf.inputs["Anisotropic"])
    # anisotropic_rotation = export_node(ctx, bsdf.inputs["Anisotropic Rotation"])
    sheen = export_node(ctx, bsdf.inputs["Sheen"])
    sheen_tint = export_node(ctx, bsdf.inputs["Sheen Tint"])
    clearcoat = export_node(ctx, bsdf.inputs["Clearcoat"])
    clearcoat_roughness = export_node(
        ctx, bsdf.inputs["Clearcoat Roughness"])
    ior = export_node(ctx, bsdf.inputs["IOR"])

    has_transmission = try_extract_node_value(transmission, default=1) > 0

    if not has_transmission:
        # Map specular variable to our IOR interpretation
        ior = _map_specular_to_ior(specular)

    return _handle_normal(ctx, bsdf,
                          {"type": "principled", "name": export_name, "base_color": base_color, "metallic": metallic,
                           "roughness": roughness, "anisotropic": anisotropic, "sheen": sheen, "sheen_tint": sheen_tint,
                           "clearcoat": clearcoat, "clearcoat_roughness": clearcoat_roughness, "flatness": subsurface,
                           "specular_transmission": transmission, "specular_tint": specular_tint, "ior": ior})


def _export_add_bsdf(ctx, bsdf, export_name):
    mat1 = _export_bsdf_inline(
        ctx, bsdf.inputs[0], export_name + "__1")
    mat2 = _export_bsdf_inline(
        ctx, bsdf.inputs[1], export_name + "__2")

    if mat1 is None or mat2 is None:
        print(f"Add BSDF {export_name} has no valid bsdf input")
        return None

    ctx.result["bsdfs"].append(mat1)
    ctx.result["bsdfs"].append(mat2)
    return {"type": "add", "name": export_name,
                           "first": export_name + "__1", "second": export_name + "__2"}


def _export_mix_bsdf(ctx, bsdf, export_name):
    mat1 = _export_bsdf_inline(
        ctx, bsdf.inputs[1], export_name + "__1")
    mat2 = _export_bsdf_inline(
        ctx, bsdf.inputs[2], export_name + "__2")
    factor = export_node(ctx, bsdf.inputs["Fac"])

    if mat1 is None or mat2 is None:
        print(f"Mix BSDF {export_name} has no valid bsdf input")
        return None

    ctx.result["bsdfs"].append(mat1)
    ctx.result["bsdfs"].append(mat2)
    return {"type": "blend", "name": export_name,
            "first": export_name + "__1", "second": export_name + "__2", "weight": factor}


def _export_emission_bsdf(ctx, bsdf, export_name):
    # Emission is handled separately
    return {"type": "diffuse", "name": export_name, "reflectance": 0}


def _export_bsdf(ctx, bsdf, name, output_name):
    if bsdf is None:
        print(f"Material {name} has no valid bsdf")
        return None
    elif isinstance(bsdf, bpy.types.ShaderNodeBsdfDiffuse):
        return _export_diffuse_bsdf(ctx, bsdf, name)
    elif isinstance(bsdf, bpy.types.ShaderNodeBsdfGlass):
        return _export_glass_bsdf(ctx, bsdf, name)
    elif isinstance(bsdf, bpy.types.ShaderNodeBsdfRefraction):
        return _export_refraction_bsdf(ctx, bsdf, name)
    elif isinstance(bsdf, bpy.types.ShaderNodeBsdfTransparent):
        return _export_transparent_bsdf(ctx, bsdf, name)
    elif isinstance(bsdf, bpy.types.ShaderNodeBsdfTranslucent):
        return _export_translucent_bsdf(ctx, bsdf, name)
    elif isinstance(bsdf, bpy.types.ShaderNodeBsdfGlossy):
        return _export_glossy_bsdf(ctx, bsdf, name)
    elif isinstance(bsdf, bpy.types.ShaderNodeBsdfPrincipled):
        return _export_principled_bsdf(ctx, bsdf, name)
    elif isinstance(bsdf, bpy.types.ShaderNodeMixShader):
        return _export_mix_bsdf(ctx, bsdf, name)
    elif isinstance(bsdf, bpy.types.ShaderNodeAddShader):
        return _export_add_bsdf(ctx, bsdf, name)
    elif isinstance(bsdf, bpy.types.ShaderNodeEmission):
        return _export_emission_bsdf(ctx, bsdf, name)
    elif isinstance(bsdf, bpy.types.ShaderNodeGroup):
        return handle_node_group_begin(ctx, bsdf, output_name, lambda ctx2, socket: _export_bsdf_inline(ctx2, socket, name))
    elif isinstance(bsdf, bpy.types.NodeGroupInput):
        return handle_node_group_end(ctx, bsdf, output_name, lambda ctx2, socket: _export_bsdf_inline(ctx2, socket, name))
    elif isinstance(bsdf, bpy.types.NodeReroute):
        return handle_node_reroute(ctx, bsdf, lambda ctx2, socket: _export_bsdf_inline(ctx2, socket, name))
    else:
        print(
            f"Material {name} has a bsdf of type {type(bsdf).__name__}  which is not supported")
        return None


def _export_bsdf_inline(ctx, socket, name):
    if not socket.is_linked:
        return None

    return _export_bsdf(ctx, socket.links[0].from_node, name, socket.links[0].from_socket.name)


def _get_bsdf_link(material):
    if material.node_tree is None:
        print(f"Material {material.name} has no valid node tree")
        return None

    output = material.node_tree.nodes.get("Material Output")
    if output is None:
        print(f"Material {material.name} has no output node")
        return None

    surface = output.inputs.get("Surface")
    if surface is None or not surface.is_linked:
        print(f"Material {material.name} has no surface node")
        return None

    return surface.links[0]


# Will export basic material without node support
def _export_basic_material(material):
    # Map specular variable to our IOR interpretation
    ior = _map_specular_to_ior(material.specular_intensity)

    return {"type": "principled", "name": material.name, "base_color": map_rgb(material.diffuse_color), "metallic": material.metallic,
            "roughness": material.roughness, "ior": ior}


def export_material(ctx, material):
    if not material:
        return None

    if not material.node_tree:
        return _export_basic_material(material)

    link = _get_bsdf_link(material)
    if not link:
        return None

    return _export_bsdf(ctx, link.from_node, material.name, link.from_socket.name)


################################
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

    return f"mix({factor}, {mat1}, {mat2})"


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
