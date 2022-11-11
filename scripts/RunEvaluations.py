from api.utils import load_api, get_root_dir
import os
from pathlib import Path
import numpy as np


def get_output_path(scene_file, spp, target):
    root_dir = get_root_dir()
    out_dir = os.path.join(root_dir, "build", "evaluation")
    os.makedirs(out_dir, exist_ok=True)

    basename = Path(scene_file).stem
    return os.path.join(out_dir, f"{basename}-{spp}-{target}.exr")


def evaluate_target(ignis, scene_file, spp, target):
    print(f"Using {target}")

    opts = ignis.RuntimeOptions.makeDefault()
    opts.Target = ignis.Target.pickCPU() if target == "cpu" else ignis.Target.pickGPU(0)

    out_file = get_output_path(scene_file, spp, target)
    with ignis.loadFromFile(str(scene_file), opts) as runtime:
        if runtime is None:
            raise RuntimeError("Could not load scene")

        while runtime.SampleCount < spp:
            runtime.step()

        ignis.saveExr(out_file, np.divide(runtime.getFramebuffer(), runtime.IterationCount))


def evaluate(ignis, scene_file, spp):
    evaluate_target(ignis, scene_file, spp, "gpu")
    evaluate_target(ignis, scene_file, spp, "cpu")


if __name__ == '__main__':
    SPP = 1024

    ignis = load_api()
    root_dir = get_root_dir()
    print(f"Evaluating Ignis {ignis.__version__} in {root_dir}")

    eval_dir = os.path.join(root_dir, "scenes", "evaluation")

    # Load all scene files but ignore files with -base in name
    scenes = []
    for file in os.listdir(eval_dir):
        if file.endswith('.json') and "-base" not in file:
            scenes.append(os.path.join(eval_dir, file))

    for scene in scenes:
        print(f"Evaluating {scene}")
        evaluate(ignis, scene, SPP)
        print("------------------------------------")
