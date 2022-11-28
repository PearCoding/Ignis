import bpy
from bpy_extras.node_utils import find_node_input

from . import api, render


class IgnisButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "render"
    COMPAT_ENGINES = {'IGNIS_RENDER'}
    # COMPAT_ENGINES must be defined in each subclass, external engines can add themselves here

    @classmethod
    def poll(cls, context):
        return context.engine in getattr(cls, 'COMPAT_ENGINES', [])


def draw_header(self, context):
    layout = self.layout
    layout.use_property_split = True
    layout.use_property_decorate = False

    if context.engine == 'IGNIS_RENDER':
        if not api.check_api():
            row = layout.row(align=True)
            row.alert = True
            row.label(text="Can not find Ignis API", icon="ERROR")

        row = layout.row(align=True)
        row.prop(context.scene.ignis, "target", expand=True)


def panel_node_draw(layout, id_data, output_type, input_name):
    if not id_data.use_nodes:
        # Totally fine to use the cycles operator, as we can assume it is always available
        layout.operator("cycles.use_shading_nodes", icon='NODETREE')
        return False

    ntree = id_data.node_tree

    node = ntree.get_output_node('CYCLES')
    if node:
        input = find_node_input(node, input_name)
        if input:
            layout.template_node_view(ntree, node, input)
        else:
            layout.label(text="Incompatible output node")
    else:
        layout.label(text="No output node")

    return True


class IGNIS_RENDER_PT_pr_render(IgnisButtonsPanel, bpy.types.Panel):
    bl_label = "Render"

    def draw(self, context):
        layout = self.layout
        scene = context.scene

        row = layout.row(align=True)
        row.operator("render.render", text="Render", icon='RENDER_STILL')
        row.operator("render.render", text="Animation",
                     icon='RENDER_ANIMATION').animation = True
        row.operator("export_scene.ignis", text="Export",
                     icon='FILE_BLANK')  # TODO

        layout.separator()
        layout.prop(scene.ignis, "integrator")


class IGNIS_RENDER_PT_pr_performance(IgnisButtonsPanel, bpy.types.Panel):
    bl_label = "Performance"
    bl_options = {'DEFAULT_CLOSED'}

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


class IGNIS_RENDER_PT_pr_sampler(IgnisButtonsPanel, bpy.types.Panel):
    bl_label = "Sampler"

    def draw(self, context):
        layout = self.layout
        scene = context.scene

        layout.prop(scene.ignis, "max_samples", text="SPP")
        layout.separator()


class IGNIS_RENDER_PT_pr_integrator(IgnisButtonsPanel, bpy.types.Panel):
    bl_label = "Integrator"

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


class IGNIS_LIGHT_PT_preview(IgnisButtonsPanel, bpy.types.Panel):
    bl_label = "Preview"
    bl_context = "data"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return context.light and IgnisButtonsPanel.poll(context)

    def draw(self, context):
        self.layout.template_preview(context.light)


class IGNIS_LIGHT_PT_nodes(IgnisButtonsPanel, bpy.types.Panel):
    bl_label = "Nodes"
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        return context.light and IgnisButtonsPanel.poll(context)

    def draw(self, context):
        layout = self.layout

        layout.use_property_split = True

        light = context.light
        panel_node_draw(layout, light, 'OUTPUT_LIGHT', 'Surface')


class IGNIS_WORLD_PT_preview(IgnisButtonsPanel, bpy.types.Panel):
    bl_label = "Preview"
    bl_context = "world"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return context.world and IgnisButtonsPanel.poll(context)

    def draw(self, context):
        self.layout.template_preview(context.world)


class IGNIS_WORLD_PT_background(IgnisButtonsPanel, bpy.types.Panel):
    bl_label = "Background"
    bl_context = "world"

    @classmethod
    def poll(cls, context):
        return context.world and IgnisButtonsPanel.poll(context)

    def draw(self, context):
        layout = self.layout

        layout.use_property_split = True

        world = context.world

        if not panel_node_draw(layout, world, 'OUTPUT_WORLD', 'Surface'):
            layout.prop(world, "color")


class IGNIS_MATERIAL_PT_preview(IgnisButtonsPanel, bpy.types.Panel):
    bl_label = "Preview"
    bl_context = "material"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        mat = context.material
        return mat and (not mat.grease_pencil) and IgnisButtonsPanel.poll(context)

    def draw(self, context):
        self.layout.template_preview(context.material)


class IGNIS_MATERIAL_PT_surface(IgnisButtonsPanel, bpy.types.Panel):
    bl_label = "Surface"
    bl_context = "material"

    @classmethod
    def poll(cls, context):
        mat = context.material
        return mat and (not mat.grease_pencil) and IgnisButtonsPanel.poll(context)

    def draw(self, context):
        layout = self.layout

        layout.use_property_split = True

        mat = context.material
        if not panel_node_draw(layout, mat, 'OUTPUT_MATERIAL', 'Surface'):
            layout.prop(mat, "diffuse_color")


def get_panels():
    exclude_panels = {
        'DATA_PT_camera_dof',
        'DATA_PT_falloff_curve',
        'OBJECT_PT_visibility',
        'VIEWLAYER_PT_filter',
        'VIEWLAYER_PT_layer_passes',
        'RENDER_PT_post_processing',
        'RENDER_PT_simplify',
    }

    panels = []
    for panel in bpy.types.Panel.__subclasses__():
        if hasattr(panel, 'COMPAT_ENGINES') and 'BLENDER_RENDER' in panel.COMPAT_ENGINES:
            if panel.__name__ not in exclude_panels:
                panels.append(panel)

    return panels


register_sub, unregister_sub = bpy.utils.register_classes_factory([
    IGNIS_RENDER_PT_pr_render,
    IGNIS_RENDER_PT_pr_performance,
    IGNIS_RENDER_PT_pr_sampler,
    IGNIS_RENDER_PT_pr_integrator,
    IGNIS_LIGHT_PT_preview,
    IGNIS_LIGHT_PT_nodes,
    IGNIS_WORLD_PT_preview,
    IGNIS_WORLD_PT_background,
    IGNIS_MATERIAL_PT_preview,
    IGNIS_MATERIAL_PT_surface
])


def register():
    bpy.types.RENDER_PT_context.append(draw_header)
    register_sub()

    for panel in get_panels():
        panel.COMPAT_ENGINES.add(render.IgnisRender.bl_idname)


def unregister():
    for panel in get_panels():
        if render.IgnisRender.bl_idname in panel.COMPAT_ENGINES:
            panel.COMPAT_ENGINES.remove(render.IgnisRender.bl_idname)

    unregister_sub()
    bpy.types.RENDER_PT_context.append(draw_header)
