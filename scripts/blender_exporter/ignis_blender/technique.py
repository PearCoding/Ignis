import bpy
from .utils import *
from .context import ExportContext


def export_technique(result, ctx: ExportContext):
    if ctx.is_renderer:
        if ctx.scene.ignis.integrator == 'PATH':
            result["technique"] = {
                "type": "path",
                "max_depth": ctx.scene.ignis.max_ray_depth,
                "clamp": ctx.scene.ignis.clamp_value
            }
            return
        elif ctx.scene.ignis.integrator == 'VOLPATH':
            result["technique"] = {
                "type": "volpath",
                "max_depth": ctx.scene.ignis.max_ray_depth,
                "clamp": ctx.scene.ignis.clamp_value
            }
            return
        elif ctx.scene.ignis.integrator == 'PPM':
            result["technique"] = {
                "type": "ppm",
                "max_depth": ctx.scene.ignis.max_ray_depth,  # TODO
                "clamp": ctx.scene.ignis.clamp_value,
                "photons": ctx.scene.ignis.ppm_photons_per_pass
            }
            return
        elif ctx.scene.ignis.integrator == 'AO':
            result["technique"] = {"type": "ao"}
            return
    elif getattr(ctx.scene, 'cycles', None) is not None and bpy.context.engine == 'CYCLES':
        max_depth = ctx.scene.cycles.max_bounces
        clamp = max(ctx.scene.cycles.sample_clamp_direct,
                    ctx.scene.cycles.sample_clamp_indirect)
    elif getattr(ctx.scene, 'eevee', None) is not None and bpy.context.engine == 'BLENDER_EEVEE':
        max_depth = ctx.scene.eevee.gi_diffuse_bounces
        clamp = ctx.scene.eevee.gi_glossy_clamp
    else:
        max_depth = 8
        clamp = 0

    result["technique"] = {
        "type": "path",
        "max_depth": max_depth,
        "clamp": clamp
    }
