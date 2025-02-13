import bpy
import os
import json

from collections import namedtuple

import numpy as np

from . import exporter, addon_preferences, api


class IgnisRenderUpdater:
    def __init__(self, renderer:bpy.types.RenderEngine, size: tuple[int, int]):
        self.renderer = renderer
        self.size = size
        self.result = None
        self.aov_names= []

    def update(self, runtime):
        if self.result is None:
            self._setup(runtime)
        
        layer = self.result.layers[0]
        for aov_name in self.aov_names:
            scale = 1 / runtime.IterationCount if runtime.IterationCount > 0 else 1
            if aov_name == "Normals" or aov_name == "Albedo":
                scale = 1
        
            aov = runtime.getFramebufferForHost(aov_name)

            buffer = np.flip(aov, axis=0).reshape(self.size[0] * self.size[1], 3) * scale
            buffer = np.hstack([buffer, np.ones(shape=(self.size[0] * self.size[1], 1))])

            pass_name = "Combined" if aov_name == "Color" else aov_name
            layer.passes[pass_name].rect = buffer

        self.renderer.update_result(self.result)

    def finalize(self):
        if self.result is not None:
            self.renderer.end_result(self.result)

    def _setup(self, runtime):
        # Add aovs as passes and assume the
        self.aov_names = list(runtime.FramebufferNames)
        for aov_name in self.aov_names:
            if aov_name != "Color":
                self.add_pass(aov_name)
            
        self.result = self.begin_result(0, 0, self.size[0], self.size[1])


class IgnisRender(bpy.types.RenderEngine):
    bl_idname = 'IGNIS_RENDER'
    bl_label = "Ignis"
    # bl_use_preview = True
    bl_use_exclude_layers = True
    bl_use_eevee_viewport = True
    bl_use_shading_nodes_custom = False

    def _handle_render_stat(self, renderer, max_spp):
        line = "Iter %i SPP %i" % (
            renderer.IterationCount, renderer.SampleCount)

        self.update_stats("", "Ignis: Rendering [%s]..." % (line))
        self.update_progress(min(1, renderer.SampleCount / max_spp))

    def render(self, depsgraph: bpy.types.Depsgraph):
        prefs = addon_preferences.get_prefs()
        ig = api.load_api()
        ig.setVerbose(prefs.verbose)

        import tempfile
        scene = depsgraph.scene
        render = scene.render
        spp = scene.ignis.max_samples

        x = int(render.resolution_x * render.resolution_percentage * 0.01)
        y = int(render.resolution_y * render.resolution_percentage * 0.01)

        sceneFile = ""
        renderPath = bpy.path.resolve_ncase(
            bpy.path.abspath(render.frame_path()))
        if not os.path.isdir(renderPath):
            renderPath = os.path.dirname(renderPath)

        if not renderPath:
            renderPath = tempfile.gettempdir() + "/ignis/"

        if not os.path.exists(renderPath):
            os.makedirs(renderPath)

        sceneFile = tempfile.NamedTemporaryFile(suffix=".json").name
        sceneDir = os.path.dirname(sceneFile)

        if self.test_break():
            return

        self.update_stats("", "Ignis: Exporting data")
        exported_scene = exporter.export_scene(
            sceneFile, self, depsgraph,
            settings=namedtuple("Settings",
                                ["export_materials", "use_selection", "export_lights", "enable_background", "enable_camera", "enable_technique", "triangulate_shapes", "copy_images"])(True, False, True, True, True, True, True, False)
        )

        if exported_scene is None:
            return

        if self.test_break():
            return

        self.update_stats("", "Ignis: Starting render")
        threads = 0
        if render.threads_mode == 'FIXED':
            threads = render.threads

        opts = ig.RuntimeOptions.makeDefault()
        if scene.ignis.target == 'CPU':
            opts.Target = ig.Target.pickCPU()
        else:
            opts.Target = ig.Target.pickGPU()
        opts.Target.ThreadCount = threads
        opts.OverrideFilmSize = [x, y]
        opts.Denoiser.Enabled = scene.ignis.use_denoiser

        with ig.loadFromString(json.dumps(exported_scene), sceneDir, opts) as runtime:
            if not runtime:
                self.report(
                    {'ERROR'}, "Ignis: could not load environment from file")
                return

            if self.test_break():
                return

            # Update image
            updater = IgnisRenderUpdater(self, (x, y))

            while runtime.SampleCount < spp:
                if self.test_break():
                    break
                runtime.step()
                self._handle_render_stat(runtime, spp)
                if self.test_break():
                    break
                updater.update(runtime)

            updater.update(runtime)
            updater.finalize()
        
        self.update_stats("", "")


def register():
    bpy.utils.register_class(IgnisRender)


def unregister():
    bpy.utils.unregister_class(IgnisRender)
