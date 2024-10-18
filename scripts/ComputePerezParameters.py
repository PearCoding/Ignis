from api.utils import load_api, get_root_dir
import os
from pathlib import Path
import numpy as np
import argparse
import json

import figuregen
import simpleimageio as sio


def get_output_path(out_dir, args, brightness, clearness):
    basename = f"{args.year if args.year else 'D'}_{args.month if args.month else 'D'}_{args.day if args.day else 'D'}_B{brightness}_C{clearness}.exr"
    return os.path.join(out_dir, basename)


def generate_scene(args, brightness, clearness):
    scene = {
        "technique": {"type": "path"},
        "camera": {"type": "fishlens", "near_clip": 0.1, "far_clip": 100, "transform": [{"lookat": {"direction": [0, 1, 0]}}]},
        "film": {"size": [256, 256]},
        "lights": [{"type": "perez", "name": "Sky"}]
    }

    scene["lights"][0]["brightness"] = brightness
    scene["lights"][0]["clearness"] = clearness
    scene["lights"][0]["color"] = 1
    scene["lights"][0]["ground"] = 0
    if args.year is not None:
        scene["lights"][0]["year"] = args.year
    if args.month is not None:
        scene["lights"][0]["month"] = args.month
    if args.day is not None:
        scene["lights"][0]["day"] = args.day
    scene["lights"][0]["hour"] = 12
    scene["lights"][0]["minute"] = 0
    scene["lights"][0]["seconds"] = 0

    return json.dumps(scene)


def render(ignis, scene, args, brightness, clearness):
    opts = ignis.RuntimeOptions.makeDefault()
    opts.Target = ignis.Target.pickCPU() if args.cpu else ignis.Target.pickGPU(
        0) if args.gpu else ignis.Target.pickBest()

    out_file_exr = get_output_path(args.OutputDir, args, brightness, clearness)

    with ignis.loadFromString(scene, opts) as runtime:
        if runtime is None:
            raise RuntimeError("Could not load scene")

        while runtime.SampleCount < args.spp:
            runtime.step()

        img = np.divide(runtime.getFramebufferForHost(), runtime.IterationCount)
        ignis.saveExr(out_file_exr, img)
        return img


def render_parameter(ignis, args, brightness, clearness):
    scene = generate_scene(args, brightness, clearness)
    return render(ignis, scene, args, brightness, clearness)

EXPOSURE = -9
BRIGHTNESS = [0.01, 0.05, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6]
CLEARNESS = [1, 1.05, 1.1, 2, 3, 4, 6, 8, 10, 12]

if __name__ == '__main__':
    root_dir = get_root_dir()
    out_dir = os.path.join(root_dir, "build", "perez")

    parser = argparse.ArgumentParser()
    parser.add_argument('OutputDir', nargs='?', type=Path, default=out_dir,
                        help='Path to export image data')
    parser.add_argument('--gpu', action="store_true",
                        help="Force GPU")
    parser.add_argument('--cpu', action="store_true",
                        help="Force CPU")
    parser.add_argument('-d', '--day', type=int, default=None,
                        help="Day of simulation")
    parser.add_argument('-m', '--month', type=int, default=None,
                        help="Month of simulation")
    parser.add_argument('-y', '--year', type=int, default=None,
                        help="Year of simulation")
    parser.add_argument('--spp', type=int, default=32,
                        help="Number of samples to acquire for each day")
    parser.add_argument('--verbose', action="store_true",
                        help="Set logging verbosity to debug")
    parser.add_argument('--quiet', action="store_true",
                        help="Make logging quiet")

    args = parser.parse_args()
    os.makedirs(args.OutputDir, exist_ok=True)

    ignis = load_api()
    ignis.setVerbose(args.verbose)
    ignis.setQuiet(args.quiet)

    root_dir = get_root_dir()
    print(f"Rendering using Ignis {ignis.__version__} in {root_dir}")

    grid = figuregen.Grid(len(BRIGHTNESS), len(CLEARNESS))
    for i, b in enumerate(BRIGHTNESS):
        for j, c in enumerate(CLEARNESS):
            img = render_parameter(ignis, args, b, c)
            grid[i, j].set_image(figuregen.JPEG(sio.lin_to_srgb(sio.exposure(img, EXPOSURE))))

    grid.set_row_titles(figuregen.LEFT, [str(b) for b in BRIGHTNESS])
    grid.set_col_titles(figuregen.TOP, [str(c) for c in CLEARNESS])
    rows = [[grid]]

    print(f"Writing summary to {out_dir}/PerezParameters.pdf")

    # Generate the figure with the pdflatex backend and default settings
    figuregen.figure(rows, width_cm=10, filename=f"{out_dir}/PerezParameters.pdf")
