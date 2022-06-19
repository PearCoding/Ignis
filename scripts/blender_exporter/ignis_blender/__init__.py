import bpy

from . import exporter, render_engine, material_ui, material, world

bl_info = {
    "name": "Ignis Renderer",
    "author": "Maher Rayes, Pascal Grittmann",
    "version": (0, 1),
    "blender": (2, 92, 0),
    "category": "Render",
}

def register():
    exporter.register()
    render_engine.register()
    material_ui.register()
    material.register()
    world.register()

def unregister():
    exporter.unregister()
    render_engine.unregister()
    material_ui.unregister()
    material.unregister()
    world.unregister()