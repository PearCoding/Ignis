from . import exporter_ui, render_ui, render_properties, render, addon_preferences

bl_info = {
    "name": "Ignis Scene",
    "author": "Ömercan Yazici, Maher Rayes, Pascal Grittmann",
    "description": "Export scene to Ignis or render within Blender",
    "version": (0, 5, 1),
    "blender": (2, 92, 0),
    "location": "File > Import-Export",
    "category": "Import-Export",
    "tracker_url": "https://github.com/PearCoding/Ignis/issues/new",
    "doc_url": "https://pearcoding.github.io/Ignis/",
    "support": "COMMUNITY",
}


def register():
    addon_preferences.register()
    render_properties.register()
    exporter_ui.register()
    render_ui.register()
    render.register()


def unregister():
    render.unregister()
    render_ui.unregister()
    exporter_ui.unregister()
    render_properties.unregister()
    addon_preferences.unregister()
