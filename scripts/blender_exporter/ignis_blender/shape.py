import os
import bpy
import bmesh

from .addon_preferences import get_prefs
from .context import ExportContext


def get_shape_name_base(obj: bpy.types.Object, inst: bpy.types.DepsgraphObjectInstance):
    modifiers = [mod.type for mod in obj.original.modifiers]
    has_nodes = 'NODES' in modifiers

    if has_nodes:
        # Not sure how to ensure shapes with nodes are handled as uniques
        # TODO: We better join them by material
        id = hex(inst.random_id).replace("0x", "").replace('-', 'M').upper()
        return f"{obj.name}_{id}"

    try:
        return obj.data.name
    except:
        return obj.original.data.name  # We use the original mesh name!


def _shape_name_material(name: str, mat_id: int):
    return f"_m_{mat_id}_{name}"


def _export_bmesh_by_material(me: bpy.types.Mesh, name: str, export_ctx: ExportContext):
    mat_count = len(me.materials)
    shapes = []

    def _export_for_mat(mat_id):
        from .ply import save_mesh as ply_save

        bm = bmesh.new()
        bm.from_mesh(me)

        # remove faces with other materials
        if mat_count > 1:
            for f in bm.faces:
                if f.material_index != mat_id:
                    bm.faces.remove(f)

        # Make sure all faces are convex
        bmesh.ops.connect_verts_concave(bm, faces=bm.faces)

        if export_ctx.settings.triangulate_shapes:
            bmesh.ops.triangulate(bm, faces=bm.faces)

        bm.normal_update()

        if len(bm.verts) == 0 or len(bm.faces) == 0 or not bm.is_valid:
            export_ctx.report_warning(
                f"Obsolete material slot {mat_id} for shape {name}")
            bm.free()
            return

        shape_name = name if mat_count <= 1 else _shape_name_material(
            name, mat_id)

        abs_filepath = os.path.join(
            export_ctx.root, get_prefs().mesh_dir_name, shape_name+".ply")
        ply_save(filepath=abs_filepath,
                 bm=bm,
                 use_ascii=False,
                 use_normals=True,
                 use_uv=True,
                 use_color=False,
                 )

        bm.free()
        shapes.append((shape_name, mat_id))

    if mat_count == 0:
        # special case if the mesh has no slots available
        _export_for_mat(0)
    else:
        # split bms by materials
        for mat_id in range(0, mat_count):
            _export_for_mat(mat_id)

    return shapes


def export_shape(result, name: str, obj: bpy.types.Object, export_ctx: ExportContext):
    # TODO: We want the mesh to be evaluated with renderer (or viewer) depending on user input
    # This is not possible currently, as access to `mesh_get_eval_final` (COLLADA) is not available
    # nor is it possible to setup via dependency graph, see https://devtalk.blender.org/t/get-render-dependency-graph/12164
    try:
        me = obj.to_mesh(preserve_all_data_layers=False,
                         depsgraph=export_ctx.depsgraph)
    except RuntimeError:
        return []

    shapes = _export_bmesh_by_material(me, name, export_ctx)
    obj.to_mesh_clear()

    for shape in shapes:
        result["shapes"].append(
            {"type": "ply", "name": shape[0],
                "filename": f"{get_prefs().mesh_dir_name}/{shape[0]}.ply"},
        )

    return shapes
