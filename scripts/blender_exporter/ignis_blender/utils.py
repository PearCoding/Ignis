import mathutils


def flat_matrix(matrix):
    return [x for row in matrix for x in row]


def orient_y_up_z_forward(matrix, skip_scale=False):
    loc, rot, sca = matrix.decompose()
    return mathutils.Matrix.LocRotScale(loc, rot @ mathutils.Quaternion((0, 0, 1, 0)), mathutils.Vector.Fill(3, 1) if skip_scale else sca)


def map_rgb(rgb, scale=1):
    return [rgb[0]*scale, rgb[1]*scale, rgb[2]*scale]


def map_vector(vec):
    return [vec.x, vec.y, vec.z]


def map_texture(texture, out_dir, result):
    path = texture.filepath_raw.replace('//', '')
    if path == '':
        path = texture.name + ".exr"
        texture.file_format = "OPEN_EXR"

    # Make sure the image is loaded to memory, so we can write it out
    if not texture.has_data:
        texture.pixels[0]

    os.makedirs(os.path.join(out_dir, "Textures"), exist_ok=True)

    # Export the texture and store its path
    name = os.path.basename(path)
    old = texture.filepath_raw
    try:
        texture.filepath_raw = os.path.join(out_dir, "Textures", name)
        texture.save()
    finally:  # Never break the scene!
        texture.filepath_raw = old

    result["textures"].append(
        {
            "type": "image",
            "name": name[:-4],
            "filename": "Meshes/Textures/"+name,
        }
    )

    return name[:-4]
