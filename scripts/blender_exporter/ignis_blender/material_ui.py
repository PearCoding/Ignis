import bpy
from bpy.types import Panel, Menu
from bl_ui.properties_material import MaterialButtonsPanel

from .material import *

class IGNIS_PT_context_material(MaterialButtonsPanel, Panel):
    """
    Main UI panel for material settings.
    """
    COMPAT_ENGINES = {"SEE_SHARP"}
    bl_label = ""
    bl_options = {"HIDE_HEADER"}
    bl_order = 1

    @classmethod
    def poll(cls, context):
        engine = context.scene.render.engine
        return (context.material or context.object) and (engine == "SEE_SHARP")

    def draw(self, context):
        layout = self.layout
        mat = context.material
        obj = context.object
        slot = context.material_slot

        # Material slot selection UI
        if obj:
            row = layout.row()

            row.template_list("MATERIAL_UL_matslots", "", obj, "material_slots", obj, "active_material_index",
                rows=4 if len(obj.material_slots) > 1 else 1)

            col = row.column(align=True)
            col.operator("object.material_slot_add", icon='ADD', text="")
            col.operator("object.material_slot_remove", icon='REMOVE', text="")
            col.menu("MATERIAL_MT_context_menu", icon='DOWNARROW_HLT', text="")

            if (len(obj.material_slots) > 1):
                col.separator()
                col.operator("object.material_slot_move", icon='TRIA_UP', text="").direction = 'UP'
                col.operator("object.material_slot_move", icon='TRIA_DOWN', text="").direction = 'DOWN'

            if obj.mode == 'EDIT':
                row = layout.row(align=True)
                row.operator("object.material_slot_assign", text="Assign")
                row.operator("object.material_slot_select", text="Select")
                row.operator("object.material_slot_deselect", text="Deselect")

        # Material select / create / edit
        row = layout.row()
        if obj:
            row.template_ID(obj, "active_material", new="material.new")
            if slot:
                row.prop(slot, "link", text="")
            else:
                row.label()
        elif mat:
            row.template_ID(context.space_data, "pin_id")
            row.separator()

class IGNIS_PT_material(MaterialButtonsPanel, Panel):
    COMPAT_ENGINES = {"SEE_SHARP"}
    bl_label = 'Ignis Material'

    @classmethod
    def poll(cls, context):
        engine = context.scene.render.engine
        return (context.material or context.object) and (engine == "SEE_SHARP")

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
    bpy.utils.register_class(IGNIS_PT_context_material)
    bpy.utils.register_class(IGNIS_PT_material)

def unregister():
    bpy.utils.unregister_class(IGNIS_PT_context_material)
    bpy.utils.unregister_class(IGNIS_PT_material)