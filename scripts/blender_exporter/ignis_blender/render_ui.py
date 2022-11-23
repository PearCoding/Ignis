import bpy


from . import api


class RenderButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "render"
    # COMPAT_ENGINES must be defined in each subclass, external engines can add themselves here

    @classmethod
    def poll(cls, context):
        rd = context.scene.render
        return (rd.engine in getattr(cls, 'COMPAT_ENGINES', []))


class RENDER_PT_pr_render(RenderButtonsPanel, bpy.types.Panel):
    bl_label = "Render"
    COMPAT_ENGINES = {'IGNIS_RENDER'}

    def draw(self, context):
        layout = self.layout
        scene = context.scene

        if not api.check_api():
            row = layout.row(align=True)
            row.alert = True
            row.label(text="Can not find Ignis API", icon="ERROR")

        row = layout.row(align=True)
        row.operator("render.render", text="Render", icon='RENDER_STILL')
        row.operator("render.render", text="Animation",
                     icon='RENDER_ANIMATION').animation = True
        row.operator("export_scene.ignis", text="Export",
                     icon='FILE_BLANK')  # TODO

        layout.separator()
        layout.prop(scene.ignis, "integrator")


class RENDER_PT_pr_performance(RenderButtonsPanel, bpy.types.Panel):
    bl_label = "Performance"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'IGNIS_RENDER'}

    def draw(self, context):
        layout = self.layout

        rd = context.scene.render

        split = layout.split()

        col = split.column(align=True)
        col.label(text="Threads:")
        col.row(align=True).prop(rd, "threads_mode", expand=True)
        sub = col.column(align=True)
        sub.enabled = rd.threads_mode == 'FIXED'
        sub.prop(rd, "threads")

        layout.separator()
        layout.prop(context.scene.ignis, "target")


class RENDER_PT_pr_sampler(RenderButtonsPanel, bpy.types.Panel):
    bl_label = "Sampler"
    COMPAT_ENGINES = {'IGNIS_RENDER'}

    def draw(self, context):
        layout = self.layout
        scene = context.scene

        layout.prop(scene.ignis, "max_samples", text="SPP")
        layout.separator()


class RENDER_PT_pr_integrator(RenderButtonsPanel, bpy.types.Panel):
    bl_label = "Integrator"
    COMPAT_ENGINES = {'IGNIS_RENDER'}

    def draw(self, context):
        layout = self.layout
        scene = context.scene

        if scene.ignis.integrator == 'PATH' or scene.ignis.integrator == 'VOLPATH':
            layout.prop(scene.ignis, "max_ray_depth")
            layout.prop(scene.ignis, "clamp_value", text="Clamp")
        elif scene.ignis.integrator == 'PPM':
            layout.prop(scene.ignis, "max_ray_depth")
            layout.prop(scene.ignis, "max_light_ray_depth")
            layout.prop(scene.ignis, "clamp_value", text="Clamp")
            layout.prop(scene.ignis, "ppm_photons_per_pass")
        elif scene.ignis.integrator == 'AO':
            pass


register, unregister = bpy.utils.register_classes_factory([
    RENDER_PT_pr_render,
    RENDER_PT_pr_performance,
    RENDER_PT_pr_sampler,
    RENDER_PT_pr_integrator
])
