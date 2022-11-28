import bpy

from bpy.types import (
    PropertyGroup,
)

from bpy.props import (
    IntProperty,
    FloatProperty,
    EnumProperty,
    PointerProperty,
)

from . import enums


class IgnisSceneProperties(PropertyGroup):
    max_ray_depth: IntProperty(
        name="Max Ray Depth",
        description="Maximum ray depth",
        min=1,
        soft_max=4096,
        subtype="UNSIGNED",
        default=64
    )
    max_light_ray_depth: IntProperty(
        name="Max Light Ray Depth",
        description="Maximum ray depth for rays starting from light sources",
        min=1,
        soft_max=4096,
        subtype="UNSIGNED",
        default=16
    )
    integrator: EnumProperty(
        name="Integrator",
        description="Integrator to be used",
        items=enums.enum_integrator_mode,
        default='PATH'
    )
    clamp_value: FloatProperty(
        name="Clamp Value",
        description="If non-zero the maximum number for a sample splat to the framebuffer",
        min=0.0,
        soft_max=10,
        default=0
    )
    ppm_photons_per_pass: IntProperty(
        name="Max Photons per Pass",
        description="Maximum amount of photons to trace each pass",
        min=1000,
        soft_max=100000000,
        subtype="UNSIGNED",
        default=1000000
    )
    max_samples: IntProperty(
        name="Max Samples",
        description="Maximum samples",
        min=1,
        soft_max=4096,
        subtype="UNSIGNED",
        default=512
    )
    target: EnumProperty(
        name="Target",
        description="Target device to use in rendering",
        items=enums.enum_target,
        default='GPU'
    )


def register():
    bpy.utils.register_class(IgnisSceneProperties)
    bpy.types.Scene.ignis = PointerProperty(type=IgnisSceneProperties)


def unregister():
    del bpy.types.Scene.ignis
    bpy.utils.unregister_class(IgnisSceneProperties)
