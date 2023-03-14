import bpy
import os
import json

from collections import namedtuple

import numpy as np

from . import exporter, addon_preferences, api


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

    def render(self, depsgraph):
        prefs = addon_preferences.get_prefs()
        ig = api.load_api()
        ig.setVerbose(prefs.verbose)

        import tempfile
        scene = depsgraph.scene
        render = scene.render
        spp = scene.ignis.max_samples

        x = int(render.resolution_x * render.resolution_percentage * 0.01)
        y = int(render.resolution_y * render.resolution_percentage * 0.01)

        print("<<< START IGNIS >>>")

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
            sceneFile, depsgraph,
            settings=namedtuple("Settings",
                                ["export_materials", "use_selection", "export_lights", "enable_background", "enable_camera", "enable_technique", "triangulate_shapes", "copy_images"])(False, True, True, True, True, True, True, False)
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

        with ig.loadFromString(json.dumps(exported_scene), sceneDir, opts) as runtime:
            if not runtime:
                self.report(
                    {'ERROR'}, "Ignis: could not load environment from file")
                print("<<< IGNIS FAILED >>>")
                return

            if self.test_break():
                return

            # Update image
            result = self.begin_result(0, 0, x, y)
            layer = result.layers[0]

            def update_image():
                # runtime.tonemap(layer.passes["Combined"].rect)
                scale = 1 / runtime.IterationCount if runtime.IterationCount > 0 else 1
                buffer = np.flip(np.asarray(runtime.getFramebuffer()), axis=0).reshape(
                    x*y, 3) * scale
                buffer = np.hstack([buffer, np.ones(shape=(x*y, 1))])

                layer.passes["Combined"].rect = buffer
                self.update_result(result)

            while runtime.SampleCount < spp:
                if self.test_break():
                    break
                runtime.step()
                self._handle_render_stat(runtime, spp)
                if self.test_break():
                    break
                update_image()

            update_image()
            self.end_result(result)

        self.update_stats("", "")


def register():
    bpy.utils.register_class(IgnisRender)


def unregister():
    bpy.utils.unregister_class(IgnisRender)
