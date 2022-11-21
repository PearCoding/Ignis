from api.utils import load_api, get_root_dir
import os
from pathlib import Path
import numpy as np
import argparse
import numpy as np
import json


def get_output_path(out_dir, args, hour):
    basename = f"{args.year if args.year else 'D'}_{args.month if args.month else 'D'}_{args.day if args.day else 'D'}_{hour}.exr"
    return os.path.join(out_dir, basename)


def generate_scene(args, hour):
    scene = {
        "technique": {"type": "path"},
        "camera": {"type": "fishlens", "near_clip": 0.1, "far_clip": 100, "transform": [{"lookat": {"direction": [0, 1, 0]}}]},
        "film": {"size": [256, 256]},
        "lights": [{"type": "perez", "name": "Sky"}]
    }

    # TODO:
    scene["lights"][0]["diffuse_irradiance"] = 57.029998779296875
    scene["lights"][0]["direct_irradiance"] = 0.38999998569488525
    scene["lights"][0]["luminance"] = 6.774
    scene["lights"][0]["ground"] = 2.904
    if args.year is not None:
        scene["lights"][0]["year"] = args.year
    if args.month is not None:
        scene["lights"][0]["month"] = args.month
    if args.day is not None:
        scene["lights"][0]["day"] = args.day
    scene["lights"][0]["hour"] = hour
    scene["lights"][0]["minute"] = 0
    scene["lights"][0]["seconds"] = 0

    return json.dumps(scene)


def render(ignis, scene, args, hour):
    opts = ignis.RuntimeOptions.makeDefault()
    opts.Target = ignis.Target.pickCPU() if args.cpu else ignis.Target.pickGPU(
        0) if args.gpu else ignis.Target.pickBest()

    out_file_exr = get_output_path(args.OutputDir, args, hour)

    with ignis.loadFromString(scene, opts) as runtime:
        if runtime is None:
            raise RuntimeError("Could not load scene")

        while runtime.SampleCount < args.spp:
            runtime.step()

        img = np.divide(runtime.getFramebuffer(), runtime.IterationCount)
        ignis.saveExr(out_file_exr, img)


def render_time(ignis, args, hour):
    scene = generate_scene(args, hour)
    render(ignis, scene, args, hour)


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

    for h in range(0, 24):
        render_time(ignis, args, h)
