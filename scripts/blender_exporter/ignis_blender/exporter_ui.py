import bpy

from bpy.props import (
    BoolProperty,
    StringProperty
)
from bpy_extras.io_utils import (
    ExportHelper
)
from bpy_extras.wm_utils.progress_report import (
    ProgressReport
)

from .exporter import *


class ExportIgnis(bpy.types.Operator, ExportHelper):
    """Export scene to Ignis"""

    bl_idname = "export_scene.ignis"
    bl_label = "Export Ignis Scene"
    bl_description = "Export scene to Ignis"
    bl_options = {'PRESET'}

    filename_ext = ".json"
    filter_glob: StringProperty(
        default="*.json",
        options={'HIDDEN'}
    )

    use_selection: BoolProperty(
        name="Selection Only",
        description="Export selected objects only",
        default=False,
    )

    animations: BoolProperty(
        name="Export Animations",
        description="If true, writes .json for each frame in the animation.",
        default=False,
    )

    export_materials: BoolProperty(
        name="Export Materials",
        description="If true, materials will be exported, else a single diffuse material will be used.",
        default=True,
    )

    export_lights: BoolProperty(
        name="Export Lights",
        description="If true, lights will be exported.",
        default=True,
    )

    enable_background: BoolProperty(
        name="Export Background",
        description="If true, background will be exported as a light.",
        default=True,
    )

    enable_camera: BoolProperty(
        name="Export Camera",
        description="If true, active camera will be exported.",
        default=True,
    )

    enable_technique: BoolProperty(
        name="Export Technique",
        description="If true, current integration technique will be mapped to Ignis.",
        default=True,
    )

    copy_images: BoolProperty(
        name="Copy all Images",
        description="If true, copy all images to Textures/, not only Generated or Packed images.",
        default=False,
    )

    check_extension = True

    def execute(self, context):
        keywords = self.as_keywords(
            ignore=(
                "filepath",
                "filter_glob",
                "animations",
                "check_existing"
            ),
        )

        # Exit edit mode before exporting, so current object states are exported properly.
        if bpy.ops.object.mode_set.poll():
            bpy.ops.object.mode_set(mode='OBJECT')

        with ProgressReport(context.window_manager) as progress:
            if self.animations is True:
                scene_frames = range(
                    context.scene.frame_start, context.scene.frame_end + 1)
                progress.enter_substeps(len(scene_frames))
                for frame in scene_frames:
                    context.scene.frame_set(frame)
                    progress.enter_substeps(1)
                    export_scene_to_file(self.filepath.replace(
                        '.json', f'{frame:04}.json'), context, **keywords)
                progress.leave_substeps()
            else:
                export_scene(self.filepath, context, **keywords)
        return {'FINISHED'}

    def draw(self, context):
        pass


class IGNIS_PT_export_include(bpy.types.Panel):
    bl_space_type = 'FILE_BROWSER'
    bl_region_type = 'TOOL_PROPS'
    bl_label = "Include"
    bl_parent_id = "FILE_PT_operator"

    @classmethod
    def poll(cls, context):
        sfile = context.space_data
        operator = sfile.active_operator

        return operator.bl_idname == "EXPORT_SCENE_OT_ignis"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        sfile = context.space_data
        operator = sfile.active_operator

        col = layout.column(heading="Limit to")
        col.prop(operator, 'use_selection')

        layout.separator()
        col = layout.column(heading="Export")
        col.prop(operator, 'animations', text="Animations")
        col.prop(operator, 'export_materials', text="Materials")
        col.prop(operator, 'export_lights', text="Lights")
        col.prop(operator, 'enable_background', text="Background")
        col.prop(operator, 'enable_camera', text="Camera")
        col.prop(operator, 'enable_technique', text="Technique")

        layout.separator()
        col = layout.column(heading="Images")
        col.prop(operator, 'copy_images')


def menu_func_export(self, context):
    self.layout.operator(ExportIgnis.bl_idname, text="Ignis (.json)")


classes = (
    ExportIgnis,
    IGNIS_PT_export_include
)


def register():
    for cls in classes:
        bpy.utils.register_class(cls)
    bpy.types.TOPBAR_MT_file_export.append(menu_func_export)


def unregister():
    bpy.types.TOPBAR_MT_file_export.remove(menu_func_export)
    for cls in classes:
        bpy.utils.unregister_class(cls)


if __name__ == "__main__":
    register()
