import os

from io_mesh_ply.export_ply import save_mesh as ply_save


def export_shape(result, obj, meshpath):
    import bmesh

    bm = bmesh.new()

    try:
        me = obj.to_mesh()
    except RuntimeError:
        return

    bm.from_mesh(me)
    obj.to_mesh_clear()

    bm.normal_update()

    ply_save(filepath=os.path.join(meshpath, obj.data.name+".ply"),
             bm=bm,
             use_ascii=False,
             use_normals=True,
             use_uv=True,
             use_color=False,
             )

    bm.free()

    result["shapes"].append(
        {"type": "ply", "name": obj.data.name,
            "filename": "Meshes/" + obj.data.name + ".ply"},
    )
