import os
import shutil
import simpleimageio as sio
import numpy as np
import json
import re
from pathlib import Path, PurePath
import argparse


def get_reference_path(basename, ref_dir):
    filenames = [str(p) for p in Path(ref_dir).glob(f"ref-{basename}*.exr")]
    if len(filenames) > 0:
        return min(filenames, key=len)

    # If not found, drop one '-' section
    basename = basename[:basename.rfind('-')]
    filenames = [str(p) for p in Path(ref_dir).glob(f"ref-{basename}*.exr")]
    if len(filenames) > 0:
        return min(filenames, key=len)

    # If not found, drop another '-' section
    basename = basename[:basename.rfind('-')]
    filenames = [str(p) for p in Path(ref_dir).glob(f"ref-{basename}*.exr")]
    if len(filenames) > 0:
        return min(filenames, key=len)

    print(f"Could not find a reference in {ref_dir} for {basename}")
    return None


RESULT_REGEX = re.compile(r"^(.*)-(\d+)-(\w+)$")
def get_scene_info(evaluation_result: Path):
    mat = RESULT_REGEX.match(evaluation_result.stem)
    if mat is None:
        return None
    
    original = str(mat.group(1))
    spp = int(mat.group(2))
    target = str(mat.group(3))

    return (original, spp, target)

if __name__ == "__main__":
    root_dir = Path(__file__).parent.parent.parent.resolve()
    out_dir = os.path.join(root_dir, "build", "evaluation_html")
    evl_dir = os.path.join(root_dir, "build", "evaluation")
    ref_dir = os.path.join(root_dir, "scenes", "evaluation", "references")

    parser = argparse.ArgumentParser()
    parser.add_argument('OutputDir', nargs='?', type=Path, default=out_dir,
                        help='Path to export the html page')
    parser.add_argument('--evaluation_dir', type=Path, default=evl_dir,
                        help="Path to directory containing evaluations previously conducted by RunEvaluations.py")
    parser.add_argument('--reference_dir', type=Path, default=ref_dir,
                        help="Path to directory containing references")
    parser.add_argument("-f", "--filter", type=str,
                        help="Filter scene names and only include matching patterns")
    parser.add_argument('--verbose', action="store_true",
                        help="Set logging verbosity to debug")
    parser.add_argument('--quiet', action="store_true",
                        help="Make logging quiet")

    args = parser.parse_args()

    img_out_dir = os.path.join(args.OutputDir, "imgs")
    js_out_dir = os.path.join(args.OutputDir, "js")

    os.makedirs(args.OutputDir, exist_ok=True)
    os.makedirs(img_out_dir, exist_ok=True)
    os.makedirs(js_out_dir, exist_ok=True)

    # Copy all required files
    shutil.copy2(os.path.join(root_dir, "scripts", "evaluation", "index.html"), os.path.join(args.OutputDir, "index.html"))
    shutil.copytree(os.path.join(root_dir, "scripts", "evaluation", "js"), os.path.join(args.OutputDir, "js"), dirs_exist_ok=True)
    shutil.copytree(os.path.join(root_dir, "scripts", "evaluation", "css"), os.path.join(args.OutputDir, "css"), dirs_exist_ok=True)

    # Load all scene files but ignore files with -base in name
    scene_files = []
    for file in sorted(os.listdir(evl_dir)):
        if file.endswith('.exr'):
            if args.filter:
                if not PurePath(Path(file).stem).match(args.filter):
                    continue
            scene_files.append(os.path.join(evl_dir, file))

    if len(scene_files) == 0:
        print("No scenes available. Maybe reset filter if used?")
        exit()

    # Get all the targets and references
    scenes = {}
    for scene in scene_files:
        (name, spp, target) = get_scene_info(Path(scene))
        if name not in scenes:
            scenes[name] = {}
        
        scenes[name][target] = { "spp": spp, "filename": scene }

        if "ref" not in scenes[name]:
            ref_file = get_reference_path(name, ref_dir)
            scenes[name]["ref"] = { "filename": ref_file }

    # Compute error metrics
    for name, info in scenes.items():
        ref_file = info["ref"]["filename"]
        ref_img = sio.read_layered_exr(ref_file)['']

        auto_max = np.quantile(np.average(ref_img), 0.99)

        # Copy reference to directory
        sio.write(os.path.join(img_out_dir, f"ref-{name}.png"), sio.lin_to_srgb(ref_img/auto_max))

        for target_name, target_info in info.items():
            if target_name == "ref":
                continue

            target_file = target_info["filename"]
            target_img  = sio.read_layered_exr(target_file)['']

            error_mse    = sio.error_metrics.mse_outlier_rejection(img=target_img, ref=ref_img)
            error_relmse = sio.error_metrics.relative_mse_outlier_rejection(img=target_img, ref=ref_img)

            target_info["error"] = {
                "mse": error_mse,
                "relmse": error_relmse
            }

            sio.write(os.path.join(img_out_dir, f"{target_name}-{name}.png"), sio.lin_to_srgb(target_img/auto_max))

    # Delete filenames from dict
    for name, info in scenes.items():
        for _, target_info in info.items():
            del target_info["filename"]

    # print(scenes)

    with open(os.path.join(js_out_dir, "info.js"), "w") as f:
        data = json.dumps(scenes, separators=(',', ':'))
        f.write(f"var SCENES = {data};")
