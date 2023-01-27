import os
import bmesh

from .addon_preferences import get_prefs


def get_shape_name_base(obj, inst):
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


def _shape_name_material(name, mat_id):
    return f"_m_{mat_id}_{name}"


def _export_bmesh_by_material(me, name, rootPath, settings):
    from .ply import save_mesh as ply_save

    mat_count = len(me.materials)
    shapes = []

    # split bms by materials
    for mat_id in range(0, mat_count):
        if not me.materials[mat_id]:
            continue

        bm = bmesh.new()
        bm.from_mesh(me)

        # remove faces with other materials
        if mat_count != 1:
            for f in bm.faces:
                if f.material_index != mat_id:
                    bm.faces.remove(f)

        # Make sure all faces are convex
        bmesh.ops.connect_verts_concave(bm, faces=bm.faces)

        if settings.triangulate_shapes:
            bmesh.ops.triangulate(bm, faces=bm.faces)

        bm.normal_update()

        if len(bm.verts) == 0 or len(bm.faces) == 0 or not bm.is_valid:
            print(f"Obsolete material slot {mat_id} for shape {name}")
            bm.free()
            continue

        shape_name = name if mat_count == 1 else _shape_name_material(
            name, mat_id)

        abs_filepath = os.path.join(
            rootPath, get_prefs().mesh_dir_name, shape_name+".ply")
        ply_save(filepath=abs_filepath,
                 bm=bm,
                 use_ascii=False,
                 use_normals=True,
                 use_uv=True,
                 use_color=False,
                 )

        bm.free()
        shapes.append((shape_name, mat_id))

    return shapes


def export_shape(result, name, obj, depsgraph, meshpath, settings):
    # TODO: We want the mesh to be evaluated with renderer (or viewer) depending on user input
    # This is not possible currently, as access to `mesh_get_eval_final` (COLLADA) is not available
    # nor is it possible to setup via dependency graph, see https://devtalk.blender.org/t/get-render-dependency-graph/12164
    try:
        me = obj.to_mesh(preserve_all_data_layers=False, depsgraph=depsgraph)
    except RuntimeError:
        return []

    shapes = _export_bmesh_by_material(me, name, meshpath, settings)
    obj.to_mesh_clear()

    for shape in shapes:
        result["shapes"].append(
            {"type": "ply", "name": shape[0],
                "filename": f"{get_prefs().mesh_dir_name}/{shape[0]}.ply"},
        )

    return shapes
