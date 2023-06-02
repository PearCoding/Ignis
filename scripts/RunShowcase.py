from api.utils import load_api, get_root_dir
import os
from pathlib import Path
import numpy as np
import argparse
import simpleimageio as sio
import numpy as np


def get_output_path(scene_file, out_dir, prefix):
    basename = Path(scene_file).stem
    return os.path.join(out_dir, f"{prefix}{basename}.exr"), os.path.join(out_dir, f"{prefix}{basename}.jpg")


def render(ignis, scene_file, prefix, args):
    opts = ignis.RuntimeOptions.makeDefault()
    opts.Target = ignis.Target.pickCPU() if args.cpu else ignis.Target.pickGPU(
        0) if args.gpu else ignis.Target.pickBest()

    if args.technique:
        opts.OverrideTechnique = args.technique

    out_file_exr, out_file_jpg = get_output_path(
        scene_file, args.OutputDir, prefix)

    # Check if scene can be skipped
    if not args.force:
        if os.path.exists(out_file_exr) and os.path.exists(out_file_jpg):
            lmt_scene = os.path.getmtime(scene_file)
            lmt_exr = os.path.getmtime(out_file_exr)
            lmt_jpg = os.path.getmtime(out_file_jpg)
            if lmt_exr >= lmt_scene and lmt_jpg >= lmt_scene:
                print(f"Skipping {scene_file}")
                return  # Skip render

    print(f"Rendering {scene_file}")
    with ignis.loadFromFile(str(scene_file), opts) as runtime:
        if runtime is None:
            raise RuntimeError("Could not load scene")

        while runtime.SampleCount < args.spp:
            runtime.step()

        img = np.divide(runtime.getFramebufferForHost(), runtime.IterationCount)
        ignis.saveExr(out_file_exr, img)
        sio.write(str(out_file_jpg), img)


def render_block(ignis, block, args):
    root_dir = get_root_dir()
    scene_dir = os.path.join(root_dir, "scenes", "showcase", block)

    # Load all scene files but ignore files with -base in name
    scenes = []
    for file in os.listdir(scene_dir):
        if file.endswith('.json') and "base" not in file:
            scenes.append(os.path.join(scene_dir, file))

    for scene_file in scenes:
        render(ignis, scene_file, f"{block}_", args)


if __name__ == '__main__':
    root_dir = get_root_dir()
    out_dir = os.path.join(root_dir, "build", "showcase")

    parser = argparse.ArgumentParser()
    parser.add_argument('OutputDir', nargs='?', type=Path, default=out_dir,
                        help='Path to export showcase data')
    parser.add_argument('--gpu', action="store_true",
                        help="Force GPU")
    parser.add_argument('--cpu', action="store_true",
                        help="Force CPU")
    parser.add_argument('--spp', type=int, default=1024,
                        help="Number of samples to acquire for evaluations")
    parser.add_argument('--technique', type=str,
                        help="Technique to use")
    parser.add_argument('--no-bsdfs', action="store_true",
                        help="Skip BSDFs")
    parser.add_argument('--no-lights', action="store_true",
                        help="Skip lights")
    parser.add_argument('--no-textures', action="store_true",
                        help="Skip textures")
    parser.add_argument('--no-shapes', action="store_true",
                        help="Skip shapes")
    parser.add_argument('--no-cameras', action="store_true",
                        help="Skip cameras")
    parser.add_argument('--no-techniques', action="store_true",
                        help="Skip techniques")
    parser.add_argument('--force', '-f', action="store_true",
                        help="Force rendering all pictures, regardless if already rendered or not ")
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

    if not args.no_bsdfs:
        render_block(ignis, "bsdf", args)
    if not args.no_lights:
        render_block(ignis, "light", args)
    if not args.no_textures:
        render_block(ignis, "texture", args)
    if not args.no_shapes:
        render_block(ignis, "shape", args)
    if not args.no_cameras:
        render_block(ignis, "camera", args)

    # Should be last
    if not args.no_techniques:
        args.spp = args.spp / 4  # We want an unfinished render
        render_block(ignis, "technique", args)
