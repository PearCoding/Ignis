import os

from io_mesh_ply.export_ply import save_mesh as ply_save


def get_shape_name(obj):
    return obj.original.data.name  # We use the original mesh name!


def export_shape(result, obj, depsgraph, meshpath):
    import bmesh

    name = get_shape_name(obj)
    bm = bmesh.new()

    try:
        me = obj.to_mesh(preserve_all_data_layers=False, depsgraph=depsgraph)
    except RuntimeError:
        return

    bm.from_mesh(me)
    obj.to_mesh_clear()

    bm.normal_update()

    ply_save(filepath=os.path.join(meshpath, name+".ply"),
             bm=bm,
             use_ascii=False,
             use_normals=True,
             use_uv=True,
             use_color=False,
             )

    bm.free()

    result["shapes"].append(
        {"type": "ply", "name": name,
            "filename": f"Meshes/{name}.ply"},
    )
