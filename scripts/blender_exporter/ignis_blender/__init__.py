from . import exporter_ui

bl_info = {
    "name": "Ignis Scene",
    "author": "Ömercan Yazici, Maher Rayes, Pascal Grittmann",
    "description": "Export scene to Ignis",
    "version": (0, 3, 0),
    "blender": (2, 92, 0),
    "location": "File > Import-Export",
    "category": "Import-Export",
    "tracker_url": "https://github.com/PearCoding/Ignis/issues/new",
    "doc_url": "https://pearcoding.github.io/Ignis/",
    "support": "COMMUNITY",
}


def register():
    exporter_ui.register()


def unregister():
    exporter_ui.unregister()
