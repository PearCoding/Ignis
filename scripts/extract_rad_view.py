#!/bin/python3
# Convert view from Ignis scene file into Radiance view file (or display it on the standard output)

from api.utils import load_api
import argparse
from pathlib import Path
import numpy as np
import sys

if __name__ == "__main__":
    # See https://floyd.lbl.gov/radiance/man_html/rpict.1.html for more information

    parser = argparse.ArgumentParser()
    parser.add_argument('InputFile', type=Path,
                        help='Path to a scene file')
    parser.add_argument('OutputFile', nargs='?', type=Path,
                        help='Radiance view file to export, else will be dumped to standard output')
    parser.add_argument('--add-prefix', action="store_true",
                        help="Add standard 'rvu ' prefix")

    args = parser.parse_args()

    ignis = load_api()

    # Get scene, but only load camera information
    parser = ignis.SceneParser()
    scene = parser.loadFromFile(
        str(args.InputFile.resolve()), ignis.SceneParser.F_LoadCamera | ignis.SceneParser.F_LoadFilm | ignis.SceneParser.F_LoadExternals)

    if scene.camera is None:
        print(
            f"Scene has no camera defined", file=sys.stderr)
        exit(-1)

    # Extract orientation
    view = scene.camera["transform"].getTransform()

    eye = view @ np.asarray([0, 0, 0, 1])
    dir = view @ np.asarray([0, 0, 1, 0])
    up = view @ np.asarray([0, 1, 0, 0])

    eye = eye / eye[3]
    dir = dir / np.linalg.norm(dir)
    up = up / np.linalg.norm(up)

    eye_s = f"-vp {eye[0]:.4f} {eye[1]:.4f} {eye[2]:.4f}"
    dir_s = f"-vd {dir[0]:.4f} {dir[1]:.4f} {dir[2]:.4f}"
    up_s = f"-vu {up[0]:.4f} {up[1]:.4f} {up[2]:.4f}"

    # Extract type info
    if scene.camera.pluginType == 'perspective':
        type_s = "-vtv"
        if "vfov" in scene.camera:
            vfov = scene.camera["vfov"].getNumber(60)
            hfov = scene.camera["hfov"].getNumber(60)
        else:
            vfov = scene.camera["fov"].getNumber(60)
            hfov = vfov
        size_s = f"-vh {hfov:.4f} -vv {vfov:.4f}"
    elif scene.camera.pluginType == 'orthogonal':
        type_s = "-vtl"
        scale = scene.camera["scale"].getNumber(1)
        size_s = f"-vh {scale:.4f} -vv {scale:.4f}"  # Apply aspect ratio ?
    elif scene.camera.pluginType == 'fishlens':
        type_s = "-vta"  # TODO: Be more diverse
        size_s = "-vh 180 -vv 180"
    else:
        print(
            f"Can not map {scene.camera.pluginType} to a Radiance known view type, using perspective instead", file=sys.stderr)
        type_s = "-vtv"
        size_s = "-vh 60 -vv 60"

    # Construct strings
    # We skip the near & far clipping plane (-vo, -va) and also do not apply shifts (-vs, -vl)
    prefix = "rvu " if args.add_prefix else ""
    suffix = "-vo 0 -va 0 -vs 0 -vl 0"
    output = f"{prefix}{type_s} {eye_s} {dir_s} {up_s} {size_s} {suffix}"

    # Output the desired way
    if args.OutputFile:
        with open(args.OutputFile, "w") as f:
            f.write(output)
    else:
        print(output, file=sys.stdout)
