import bpy

from . import exporter, material_ui, material

bl_info = {
    "name": "Ignis Renderer",
    "author": "Maher Rayes, Ömercan Yazici, Pascal Grittmann",
    "version": (0, 1),
    "blender": (2, 92, 0),
    "category": "Render",
}


def register():
    exporter.register()
    material_ui.register()
    material.register()


def unregister():
    exporter.unregister()
    material_ui.unregister()
    material.unregister()
