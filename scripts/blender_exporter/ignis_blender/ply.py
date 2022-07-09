# This is a copy of the io_mesh_ply/export_ply.py (except the save function) from Blender
# for the purpose of being a (proposed) fix for https://developer.blender.org/T99249
# This file will be removed and the official function  will be used instead 
# when the bug is fixed in some way

import bpy

def _write_binary(fw, ply_verts: list, ply_faces: list) -> None:
    from struct import pack

    # Vertex data
    # ---------------------------

    for v, normal, uv, color in ply_verts:
        fw(pack("<3f", *v.co))
        if normal is not None:
            fw(pack("<3f", *normal))
        if uv is not None:
            fw(pack("<2f", *uv))
        if color is not None:
            fw(pack("<4B", *color))

    # Face data
    # ---------------------------

    for pf in ply_faces:
        length = len(pf)
        fw(pack(f"<B{length}I", length, *pf))

def _write_ascii(fw, ply_verts: list, ply_faces: list) -> None:

    # Vertex data
    # ---------------------------

    for v, normal, uv, color in ply_verts:
        fw(b"%.6f %.6f %.6f" % v.co[:])
        if normal is not None:
            fw(b" %.6f %.6f %.6f" % normal[:])
        if uv is not None:
            fw(b" %.6f %.6f" % uv)
        if color is not None:
            fw(b" %u %u %u %u" % color)
        fw(b"\n")

    # Face data
    # ---------------------------

    for pf in ply_faces:
        fw(b"%d" % len(pf))
        for index in pf:
            fw(b" %d" % index)
        fw(b"\n")

def save_mesh(filepath, bm, use_ascii, use_normals, use_uv, use_color):
    uv_lay = bm.loops.layers.uv.active
    col_lay = bm.loops.layers.color.active

    use_uv = use_uv and uv_lay is not None
    use_color = use_color and col_lay is not None
    normal = uv = color = None

    ply_faces = []
    ply_verts = []
    ply_vert_map = {}
    ply_vert_id = 0

    for f in bm.faces:
        pf = []
        ply_faces.append(pf)

        for loop in f.loops:
            v = loop.vert

# Begin change diff to upstream Blender (6th. July 2022)
            if use_uv:
                uv = loop[uv_lay].uv[:]
            if use_normals:
                normal = v.normal if f.smooth else f.normal
            if use_color:
                color = tuple(int(x * 255.0) for x in loop[col_lay])

            map_id = (v.co[:], normal[:], uv, color)

            # Identify unique vertex.
            if (_id := ply_vert_map.get(map_id)) is not None:
                pf.append(_id)
                continue
# End of change diff to upstream Blender

            ply_verts.append((v, normal, uv, color))
            ply_vert_map[map_id] = ply_vert_id
            pf.append(ply_vert_id)
            ply_vert_id += 1

    with open(filepath, "wb") as file:
        fw = file.write
        file_format = b"ascii" if use_ascii else b"binary_little_endian"

        # Header
        # ---------------------------

        fw(b"ply\n")
        fw(b"format %s 1.0\n" % file_format)
        fw(b"comment Created by Blender %s - www.blender.org\n" %
            bpy.app.version_string.encode("utf-8"))

        fw(b"element vertex %d\n" % len(ply_verts))
        fw(
            b"property float x\n"
            b"property float y\n"
            b"property float z\n"
        )
        if use_normals:
            fw(
                b"property float nx\n"
                b"property float ny\n"
                b"property float nz\n"
            )
        if use_uv:
            fw(
                b"property float s\n"
                b"property float t\n"
            )
        if use_color:
            fw(
                b"property uchar red\n"
                b"property uchar green\n"
                b"property uchar blue\n"
                b"property uchar alpha\n"
            )

        fw(b"element face %d\n" % len(ply_faces))
        fw(b"property list uchar uint vertex_indices\n")
        fw(b"end_header\n")

        # Geometry
        # ---------------------------

        if use_ascii:
            _write_ascii(fw, ply_verts, ply_faces)
        else:
            _write_binary(fw, ply_verts, ply_faces)
