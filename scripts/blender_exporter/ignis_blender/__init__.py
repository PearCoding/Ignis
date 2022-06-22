from . import exporter_ui

bl_info = {
    "name": "Ignis Scene",
    "author": "Maher Rayes, Ömercan Yazici, Pascal Grittmann",
    "description": "Export scene to Ignis",
    "version": (0, 2, 0),
    "blender": (2, 92, 0),
    "location": "File > Import-Export",
    "category": "Import-Export",
    "tracker_url": "https://github.com/PearCoding/Ignis/issues/new",
    "support": "COMMUNITY",
}


def register():
    exporter_ui.register()


def unregister():
    exporter_ui.unregister()
