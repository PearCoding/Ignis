import mathutils

import bpy
from bpy.props import (
    BoolProperty,
    FloatProperty,
    StringProperty
)
from bpy_extras.io_utils import (
    ExportHelper,
    orientation_helper,
    axis_conversion
)
from bpy_extras.wm_utils.progress_report import (
    ProgressReport
)

from .exporter import *


@orientation_helper(axis_forward='-Z', axis_up='Y')
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
        description="If true, writes .json and .obj files for each frame in the animation.",
        default=False,
    )

    use_modifiers: BoolProperty(
        name="Apply Modifiers",
        description="Apply modifiers",
        default=True,
    )

    global_scale: FloatProperty(
        name="Scale",
        min=0.01, max=1000.0,
        default=1.0,
    )

    check_extension = True

    def execute(self, context):
        keywords = self.as_keywords(
            ignore=(
                "filepath",
                "filter_glob",
                "axis_forward",
                "axis_up",
                "global_scale",
                "animations",
                "check_existing"
            ),
        )

        global_matrix = (
            mathutils.Matrix.Scale(self.global_scale, 4) @
            axis_conversion(
                to_forward=self.axis_forward,
                to_up=self.axis_up,
            ).to_4x4()
        )

        camera_matrix = axis_conversion(
                to_forward=self.axis_forward,
                to_up=self.axis_up,
            ).to_4x4()

        keywords["global_matrix"] = global_matrix
        keywords["camera_matrix"] = camera_matrix

        with ProgressReport(context.window_manager) as progress:
            # Exit edit mode before exporting, so current object states are exported properly.
            if bpy.ops.object.mode_set.poll():
                bpy.ops.object.mode_set(mode='OBJECT')

            if self.animations is True:
                scene_frames = range(
                    context.scene.frame_start, context.scene.frame_end + 1)
                progress.enter_substeps(len(scene_frames))
                for frame in scene_frames:
                    context.scene.frame_set(frame)
                    depsgraph = context.evaluated_depsgraph_get()
                    progress.enter_substeps(1)
                    export_scene(self.filepath.replace(
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

        layout.prop(operator, 'animation')


class IGNIS_PT_export_transform(bpy.types.Panel):
    bl_space_type = 'FILE_BROWSER'
    bl_region_type = 'TOOL_PROPS'
    bl_label = "Transform"
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

        layout.prop(operator, 'global_scale')
        layout.prop(operator, 'axis_forward')
        layout.prop(operator, 'axis_up')


def menu_func_export(self, context):
    self.layout.operator(ExportIgnis.bl_idname, text="Ignis (.json)")


classes = (
    ExportIgnis,
    IGNIS_PT_export_include,
    IGNIS_PT_export_transform
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
