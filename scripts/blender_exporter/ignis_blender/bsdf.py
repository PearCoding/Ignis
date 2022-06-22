import bpy

from .node import export_node
from .utils import *


def export_error_material(mat_name):
    return {"type": "diffuse", "name": mat_name, "reflectance": [1, 0.75, 0.8]}


def export_black_material(mat_name):
    return {"type": "diffuse", "name": mat_name, "reflectance": 0}


def _export_diffuse_bsdf(result, bsdf, export_name, path):
    reflectance = export_node(result, bsdf.inputs["Color"], path)
    roughness = export_node(result, bsdf.inputs["Roughness"], path)

    has_roughness = try_extract_node_value(roughness, default=1) > 0

    if has_roughness:
        return {"type": "roughdiffuse", "name": export_name,
                "reflectance": reflectance, "roughness": roughness}
    else:
        return {"type": "diffuse", "name": export_name,
                "reflectance": reflectance}


def _export_glass_bsdf(result, bsdf, export_name, path):
    reflectance = export_node(result, bsdf.inputs["Color"], path)
    roughness = export_node(result, bsdf.inputs["Roughness"], path)
    ior = export_node(result, bsdf.inputs["IOR"])

    has_roughness = try_extract_node_value(roughness, default=1) > 0

    if not has_roughness:
        return {"type": "dielectric", "name": export_name,
                "specular_reflectance": reflectance, "specular_transmittance": reflectance, "ext_ior": ior}
    else:
        return {"type": "roughdielectric", "name": export_name,
                "specular_reflectance": reflectance, "specular_transmittance": reflectance, "roughness": roughness, "ext_ior": ior}


def _export_transparent_bsdf(result, bsdf, export_name, path):
    reflectance = export_node(result, bsdf.inputs["Color"], path)
    return {"type": "dielectric", "name": export_name,
            "specular_reflectance": reflectance, "specular_transmittance": reflectance, "ext_ior": 1, "int_ior": 1}


def _export_glossy_bsdf(result, bsdf, export_name, path):
    # A simple principled shader
    base_color = export_node(result, bsdf.inputs["Color"], path)
    roughness = export_node(result, bsdf.inputs["Roughness"], path)

    return {"type": "principled", "name": export_name,
            "base_color": base_color, "roughness": roughness}


def _export_principled_bsdf(result, bsdf, export_name, path):
    base_color = export_node(result, bsdf.inputs["Base Color"], path)
    roughness = export_node(result, bsdf.inputs["Roughness"], path)
    subsurface = export_node(result, bsdf.inputs["Subsurface"], path)
    metallic = export_node(result, bsdf.inputs["Metallic"], path)
    specular = export_node(result, bsdf.inputs["Specular"], path)
    specular_tint = export_node(result, bsdf.inputs["Specular Tint"], path)
    transmission = export_node(result, bsdf.inputs["Transmission"], path)
    anisotropic = export_node(result, bsdf.inputs["Anisotropic"], path)
    # anisotropic_rotation = export_node(result, bsdf.inputs["Anisotropic Rotation"], path)
    sheen = export_node(result, bsdf.inputs["Sheen"], path)
    sheen_tint = export_node(result, bsdf.inputs["Sheen Tint"], path)
    clearcoat = export_node(result, bsdf.inputs["Clearcoat"], path)
    clearcoat_roughness = export_node(
        result, bsdf.inputs["Clearcoat Roughness"], path)
    ior = export_node(result, bsdf.inputs["IOR"], path)

    has_transmission = try_extract_node_value(transmission, default=1) > 0

    if not has_transmission:
        # Map specular variable to our IOR interpretation
        ior = f"((1 + sqrt(0.08*{specular})) / max(0.001, 1 - sqrt(0.08*{specular})))"

    return {"type": "principled", "name": export_name, "base_color": base_color, "metallic": metallic,
            "roughness": roughness, "anisotropic": anisotropic, "sheen": sheen, "sheen_tint": sheen_tint,
            "clearcoat": clearcoat, "clearcoat_roughness": clearcoat_roughness, "flatness": subsurface,
            "specular_transmission": transmission, "specular_tint": specular_tint, "ior": ior}


def _export_add_bsdf(result, bsdf, export_name, path):
    mat1 = _export_bsdf_inline(
        result, bsdf.inputs[0], export_name + "__1", path)
    mat2 = _export_bsdf_inline(
        result, bsdf.inputs[1], export_name + "__2", path)

    if mat1 is None or mat2 is None:
        print(f"Mix BSDF {export_name} has no valid bsdf input")
        return None

    result["bsdfs"].append(mat1)
    result["bsdfs"].append(mat2)
    return {"type": "add", "name": export_name,
                           "first": export_name + "__1", "second": export_name + "__2"}


def _export_mix_bsdf(result, bsdf, export_name, path):
    mat1 = _export_bsdf_inline(
        result, bsdf.inputs[1], export_name + "__1", path)
    mat2 = _export_bsdf_inline(
        result, bsdf.inputs[2], export_name + "__2", path)
    factor = export_node(result, bsdf.inputs["Fac"], path)

    if mat1 is None or mat2 is None:
        print(f"Mix BSDF {export_name} has no valid bsdf input")
        return None

    result["bsdfs"].append(mat1)
    result["bsdfs"].append(mat2)
    return {"type": "blend", "name": export_name,
            "first": export_name + "__1", "second": export_name + "__2", "weight": factor}


def _export_emission_bsdf(result, bsdf, export_name, path):
    # Emission is handled separately
    return {"type": "diffuse", "name": export_name, "reflectance": 0}


def _export_bsdf(result, bsdf, name, path):
    if bsdf is None:
        print(f"Material {name} has no valid bsdf")
        return None
    elif isinstance(bsdf, bpy.types.ShaderNodeBsdfDiffuse):
        return _export_diffuse_bsdf(result, bsdf, name, path)
    elif isinstance(bsdf, bpy.types.ShaderNodeBsdfGlass):
        return _export_glass_bsdf(result, bsdf, name, path)
    elif isinstance(bsdf, bpy.types.ShaderNodeBsdfTransparent):
        return _export_transparent_bsdf(result, bsdf, name, path)
    elif isinstance(bsdf, bpy.types.ShaderNodeBsdfGlossy):
        return _export_glossy_bsdf(result, bsdf, name, path)
    elif isinstance(bsdf, bpy.types.ShaderNodeBsdfPrincipled):
        return _export_principled_bsdf(result, bsdf, name, path)
    elif isinstance(bsdf, bpy.types.ShaderNodeMixShader):
        return _export_mix_bsdf(result, bsdf, name, path)
    elif isinstance(bsdf, bpy.types.ShaderNodeAddShader):
        return _export_add_bsdf(result, bsdf, name, path)
    elif isinstance(bsdf, bpy.types.ShaderNodeEmission):
        return _export_emission_bsdf(result, bsdf, name, path)
    else:
        print(
            f"Material {name} has a bsdf of type {type(bsdf).__name__}  which is not supported")
        return None


def _export_bsdf_inline(result, socket, name, path):
    if not socket.is_linked:
        return None

    return _export_bsdf(result, socket.links[0].from_node, name, path)


def _get_bsdf(material):
    if material.node_tree is None:
        return None

    output = material.node_tree.nodes.get("Material Output")
    if output is None:
        return None

    surface = output.inputs.get("Surface")
    if surface is None or not surface.is_linked:
        return None

    return surface.links[0].from_node


def export_material(result, material, filepath):
    if not material:
        return

    return _export_bsdf(result, _get_bsdf(material), material.name, filepath)


################################
def _get_emission_mix(result, bsdf, path):
    mat1 = _get_emission(
        result, bsdf.inputs[0], path)
    mat2 = _get_emission(
        result, bsdf.inputs[1], path)
    factor = export_node(result, bsdf.inputs["Fac"], path)

    if mat1 is None and mat2 is None:
        return None

    if mat1 is None:
        mat1 = "color(0,0,0)"
    if mat2 is None:
        mat2 = "color(0,0,0)"

    return f"mix({factor}, {mat1}, {mat2})"


def _get_emission_add(result, bsdf, path):
    mat1 = _get_emission(
        result, bsdf.inputs[0], path)
    mat2 = _get_emission(
        result, bsdf.inputs[1], path)

    if mat1 is None:
        return mat2
    if mat2 is None:
        return mat1

    return f"({mat1} + {mat2})"


def _get_emission_pure(result, bsdf, path):
    color = export_node(result, bsdf.inputs["Color"], path)
    strength = export_node(result, bsdf.inputs["Strength"], path)

    return f"({color} * {strength})"


def _get_emission(result, bsdf, path):
    if isinstance(bsdf, bpy.types.ShaderNodeMixShader):
        return _get_emission_mix(result, bsdf, path)
    elif isinstance(bsdf, bpy.types.ShaderNodeAddShader):
        return _get_emission_add(result, bsdf, path)
    elif isinstance(bsdf, bpy.types.ShaderNodeEmission):
        return _get_emission_pure(result, bsdf, path)
    else:
        return None


def get_material_emission(result, material, filepath):
    if not material:
        return

    return _get_emission(result, _get_bsdf(material), filepath)
