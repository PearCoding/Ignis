import bpy
from bpy.types import Panel, Menu
from bl_ui.properties_material import MaterialButtonsPanel

from .material import *


class IGNIS_PT_material(MaterialButtonsPanel, Panel):
    bl_label = 'Ignis Material'

    @classmethod
    def poll(cls, context):
        return (context.material or context.object)

    def draw(self, context):
        if context.material is None:
            return

        mat = context.material.ignis

        layout = self.layout

        row = layout.row()
        row.label(text="Convert")
        row.operator("ignis.convert_material")

        layout.separator()

        layout.prop(mat, "base_color")
        layout.template_ID(mat, "base_texture", open="image.open")

        layout.separator()

        layout.prop(mat, "roughness", slider=True)
        layout.template_ID(mat, "rough_texture", open="image.open")

        layout.separator()

        layout.prop(mat, "metallic", slider=True)
        layout.prop(mat, "specularTintStrength", slider=True)
        layout.prop(mat, "anisotropic", slider=True)
        layout.prop(mat, "specularTransmittance", slider=True)
        layout.prop(mat, "indexOfRefraction", slider=True)
        layout.prop(mat, "diffuseTransmittance", slider=True)
        layout.prop(mat, "thin")

        layout.separator()

        layout.prop(mat, "emission_color")
        layout.prop(mat, "emission_strength")


def register():
    bpy.utils.register_class(IGNIS_PT_material)


def unregister():
    bpy.utils.unregister_class(IGNIS_PT_material)
