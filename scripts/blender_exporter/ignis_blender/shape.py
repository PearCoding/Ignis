import os
import bmesh

from .addon_preferences import get_prefs

def get_shape_name_base(obj):
    return obj.original.data.name  # We use the original mesh name!


def get_shape_name_by_material(obj, mat_id):
    name = get_shape_name_base(obj)
    if len(obj.original.data.materials) > 1:
        return _shape_name_material(name, mat_id)
    else:
        return name


def _shape_name_material(name, mat_id):
    return f"_m_{mat_id}_{name}"


def _export_bmesh_by_material(me, name, rootPath):
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

        bm.normal_update()

        if len(bm.verts) == 0 or len(bm.faces) == 0 or not bm.is_valid:
            print(f"Obsolete material slot {mat_id} for shape {name}")
            bm.free()
            continue

        shape_name = name if mat_count == 1 else _shape_name_material(
            name, mat_id)

        abs_filepath = os.path.join(rootPath, get_prefs().mesh_dir_name, shape_name+".ply")
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


def export_shape(result, obj, depsgraph, meshpath):
    name = get_shape_name_base(obj)

    try:
        me = obj.to_mesh(preserve_all_data_layers=False, depsgraph=depsgraph)
    except RuntimeError:
        return []

    shapes = _export_bmesh_by_material(me, name, meshpath)
    obj.to_mesh_clear()

    for shape in shapes:
        result["shapes"].append(
            {"type": "ply", "name": shape[0],
                "filename": f"{get_prefs().mesh_dir_name}/{shape[0]}.ply"},
        )

    return shapes
