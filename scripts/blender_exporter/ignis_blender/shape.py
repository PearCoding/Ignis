import os
import bmesh


# The following "solidification" does not work due to:
# https://developer.blender.org/T99249
# After fixing, it should work without explicit handling "non-smooth" faces from our side
def _solidify_bmesh(bm):
    '''Will fix vertex normals if given face is solid '''

    solid_faces = [f for f in bm.faces if not f.smooth]
    for f in solid_faces:
        verts_modified = False
        new_verts = []
        for vert in f.verts:
            if len(vert.link_faces) > 1:  # Only care about vertices shared by multiple faces
                new_vert = bm.verts.new(vert.co, vert)
                new_vert.copy_from(vert)
                new_vert.normal = f.normal
                new_vert.index = len(bm.verts) - 1
                new_verts.append(new_vert)
                verts_modified = True
            else:
                vert.normal = f.normal
                new_verts.append(vert)

        if verts_modified:
            bm.faces.new(new_verts, f)
            bm.faces.remove(f)


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


def _export_bmesh_by_material(me, name, meshpath):
    from io_mesh_ply.export_ply import save_mesh as ply_save

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

        # Solidify if necessary
        _solidify_bmesh(bm)

        bm.normal_update()

        if len(bm.verts) == 0 or len(bm.faces) == 0 or not bm.is_valid:
            print(f"Obsolete material slot {mat_id} for shape {name}")
            bm.free()
            continue

        shape_name = name if mat_count == 1 else _shape_name_material(
            name, mat_id)

        ply_save(filepath=os.path.join(meshpath, shape_name+".ply"),
                 bm=bm,
                 use_ascii=True,
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
                "filename": f"Meshes/{shape[0]}.ply"},
        )

    return shapes
