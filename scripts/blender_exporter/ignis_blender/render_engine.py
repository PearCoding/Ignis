import bpy
from bpy.props import *
import os
import subprocess
import tempfile

from . import exporter

class IgnisRenderEngine(bpy.types.RenderEngine):
    # These three members are used by blender to set up the
    # RenderEngine; define its internal name, visible name and capabilities.
    bl_idname = "SEE_SHARP"
    bl_label = "Ignis"
    bl_use_preview = False

    # Init is called whenever a new render engine instance is created. Multiple
    # instances may exist at the same time, for example for a viewport and final
    # render.
    def __init__(self):
        self.scene_data = None
        self.draw_data = None

    # When the render engine instance is destroy, this is called. Clean up any
    # render engine data here, for example stopping running render threads.
    def __del__(self):
        pass

    # This is the method called by Blender for both final renders (F12) and
    # small preview for materials, world and lights.
    def render(self, depsgraph):
        scene = depsgraph.scene
        scale = scene.render.resolution_percentage / 100.0
        size_x = int(scene.render.resolution_x * scale)
        size_y = int(scene.render.resolution_y * scale)

        config = scene.ignis.config

        exe = os.path.dirname(__file__) + "/bin/SeeSharp.PreviewRender.dll"
        with tempfile.TemporaryDirectory() as tempdir:
            exporter.export_scene(tempdir + "/scene.json", scene, depsgraph)
            args = ["dotnet", exe]
            args.extend([
                "--scene", tempdir + "/scene.json",
                "--output", tempdir + "/Render.hdr",
                "--resx", str(size_x),
                "--resy", str(size_y),
                "--samples", str(config.samples),
                "--algo", str(config.engine),
                "--maxdepth", str(config.maxdepth),
                "--denoise", str(config.denoise)
            ])
            subprocess.call(args)

            result = self.begin_result(0, 0, size_x, size_y)
            result.layers[0].load_from_file(tempdir + "/Render.hdr")
            self.end_result(result)


# RenderEngines also need to tell UI Panels that they are compatible with.
# We recommend to enable all panels marked as BLENDER_RENDER, and then
# exclude any panels that are replaced by custom panels registered by the
# render engine, or that are not supported.
def get_panels():
    exclude_panels = {
        'VIEWLAYER_PT_filter',
        'VIEWLAYER_PT_layer_passes',
    }

    include_panels = {
        'MATERIAL_PT_preview',
    }

    panels = []
    for panel in bpy.types.Panel.__subclasses__():
        if hasattr(panel, 'COMPAT_ENGINES') and 'BLENDER_RENDER' in panel.COMPAT_ENGINES:
            if panel.__name__ not in exclude_panels:
                panels.append(panel)
        elif hasattr(panel, 'COMPAT_ENGINES') and panel.__name__ in include_panels:
            panels.append(panel)

    return panels

class IgnisPanel(bpy.types.Panel):
    bl_idname = "IGNIS_RENDER_PT_sampling"
    bl_label = "Ignis Settings"
    bl_space_type = "PROPERTIES"
    bl_region_type = "WINDOW"
    bl_context = "render"
    COMPAT_ENGINES = {"SEE_SHARP"}

    def draw(self, context):
        config = context.scene.ignis.config

        col = self.layout.column(align=True)
        col.prop(config, "samples", text="Samples per pixel")
        col.prop(config, "engine", text="Algorithm")
        col.prop(config, "maxdepth", text="Max. depth")
        col.prop(config, "denoise", text="Denoise")

PATH_DESC = (
    "Simple unidirectional path tracer with next event estimation.\n"
)

VCM_DESC = (
    "Vertex connection and merging.\n"
)

SAMPLES_DESC = (
    "Number of samples per pixel in the rendered image."
)

MAXDEPTH_DESC = (
    "Maximum number of bounces to render. If set to one, only directly visible light\n"
    "sources will be rendered. If set to two, only direct illumination, and so on."
)

DENOISE_DESC = (
    "Whether or not to run a denoiser (Open Image Denoise) on the rendered image"
)

class IgnisConfig(bpy.types.PropertyGroup):
    engines = [
        ("PT", "Path Tracer", PATH_DESC, 0),
        ("VCM", "VCM", VCM_DESC, 1),
    ]
    engine: EnumProperty(name="Engine", items=engines, default="PT")
    samples: IntProperty(name="Samples", default=8, min=1, description=SAMPLES_DESC)
    maxdepth: IntProperty(name="Max. bounces", default=8, min=1, description=MAXDEPTH_DESC)
    denoise: BoolProperty(name="Denoise", default=True, description=DENOISE_DESC)

class IgnisScene(bpy.types.PropertyGroup):
    config: PointerProperty(type=IgnisConfig)

    @classmethod
    def register(cls):
        bpy.types.Scene.ignis = PointerProperty(
            name="Ignis Scene Settings",
            description="Ignis scene settings",
            type=cls,
        )

    @classmethod
    def unregister(cls):
        del bpy.types.Scene.ignis

def register():
    bpy.utils.register_class(IgnisConfig)
    bpy.utils.register_class(IgnisScene)
    bpy.utils.register_class(IgnisPanel)
    bpy.utils.register_class(IgnisRenderEngine)
    for panel in get_panels():
        panel.COMPAT_ENGINES.add('SEE_SHARP')

def unregister():
    bpy.utils.unregister_class(IgnisConfig)
    bpy.utils.unregister_class(IgnisScene)
    bpy.utils.unregister_class(IgnisPanel)
    bpy.utils.unregister_class(IgnisRenderEngine)
    for panel in get_panels():
        if 'SEE_SHARP' in panel.COMPAT_ENGINES:
            panel.COMPAT_ENGINES.remove('SEE_SHARP')

if __name__ == "__main__":
    register()