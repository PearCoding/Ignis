import bpy

from bpy.props import (
    StringProperty,
    BoolProperty
)
from bpy.types import AddonPreferences

package_name = __import__(__name__.split('.')[0])


class IgnisPreferences(AddonPreferences):
    bl_idname = package_name.__package__

    api_dir: StringProperty(
        name="API Directory",
        description="Path to Ignis python api",
        subtype='DIR_PATH',
    )
    verbose: BoolProperty(
        name="Verbose",
        description="Display verbose information while rendering",
        default=True
    )

    def draw(self, context):
        layout = self.layout
        layout.prop(self, "api_dir")
        row = layout.row(align=True)
        row.prop(self, "verbose")


def register():
    bpy.utils.register_class(IgnisPreferences)


def unregister():
    bpy.utils.unregister_class(IgnisPreferences)


def get_prefs():
    return bpy.context.preferences.addons[package_name.__package__].preferences
