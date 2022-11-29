from api.utils import load_api, get_root_dir
import os
from pathlib import Path, PurePath
import numpy as np
import argparse
import simpleimageio as sio
import figuregen
import numpy as np
import matplotlib.cm as cm


def get_output_path(scene_file, out_dir, spp, target):
    basename = Path(scene_file).stem
    return os.path.join(out_dir, f"{basename}-{spp}-{target}.exr")


def get_reference_path(scene_file, ref_dir):
    basename = Path(scene_file).stem
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

    print(f"Could not find a reference in {ref_dir} for {scene_file}")
    return None


def evaluate_target(ignis, scene_file, out_dir, spp, target):
    print(f"Using {target}")

    opts = ignis.RuntimeOptions.makeDefault()
    opts.Target = ignis.Target.pickCPU() if target == "cpu" else ignis.Target.pickGPU(0)

    out_file = get_output_path(scene_file, out_dir, spp, target)
    with ignis.loadFromFile(str(scene_file), opts) as runtime:
        if runtime is None:
            raise RuntimeError("Could not load scene")

        while runtime.SampleCount < spp:
            runtime.step()

        ignis.saveExr(out_file, np.divide(
            runtime.getFramebuffer(), runtime.IterationCount))


def evaluate(ignis, scene_file, args):
    if not args.no_gpu:
        evaluate_target(ignis, scene_file, args.OutputDir, args.spp, "gpu")
    if not args.no_cpu:
        evaluate_target(ignis, scene_file, args.OutputDir, args.spp, "cpu")


def map_img(img):
    return figuregen.PNG(sio.lin_to_srgb(img))


def colormap(img, cmap=cm.inferno):
    return cm.ScalarMappable(cmap=cmap).to_rgba(img)[..., 0:3]


def colorbar(cmap=cm.inferno):
    gradient = np.linspace(1, 0, 256)
    gradient = np.vstack((gradient, gradient))
    bar = np.swapaxes(cmap(gradient), 0, 1)
    bar = np.repeat(bar, 5, axis=1)
    return bar


def error_image(img, ref):
    mask = ref != 0
    raw = np.zeros_like(ref)
    raw[mask] = np.square((img[mask] - ref[mask]) / ref[mask])
    max = np.percentile(raw, 99)
    return np.clip(raw / max, 0, 1) if max != 0 else np.zeros_like(ref)


# Some predefined eps
predef_eps = {
    "cbox-d1": 5e-3,
    "cbox-d6": 5e-3,
    "room": 1e-3,
    "volume":  5e-3,
    "env4k":  8e-2,
    "env4k-nocdf":  8e-2,
    "multilight-uniform":  3e-4,
    "multilight-simple":  3e-4,
    "multilight-hierarchy":  3e-4,
    "sphere-light-ico":  2e-3,
    "sphere-light-ico-nopt":  2e-3,
    "sphere-light-uv":  2e-3,
    "sphere-light-pure":  2e-3,
}


def make_figure(scenes, args):
    title = figuregen.Grid(1, 1)
    title.get_element(0, 0).set_image(
        figuregen.PNG(np.tile([255, 255, 255], (1, 500))))
    title.set_title("top", "Evaluation")

    scenes.sort()

    scene_data = []
    for scene in scenes:
        image_gpu_name = get_output_path(
            scene, args.OutputDir, args.spp, "gpu")
        image_cpu_name = get_output_path(
            scene, args.OutputDir, args.spp, "cpu")
        image_ref_name = get_reference_path(scene, args.reference_dir)

        if image_gpu_name is None or image_cpu_name is None or image_ref_name is None:
            print(f"Could not find all results for {scene}")
            continue

        if not os.path.exists(image_gpu_name) or not os.path.exists(image_cpu_name) or not os.path.exists(image_ref_name):
            print(
                f"Could not find all results for {scene} as some results do not exist")
            continue

        scene_data.append((Path(scene).stem, image_gpu_name,
                          image_cpu_name, image_ref_name))

    errors = []
    cpu_errors = []
    names = []
    grid = figuregen.Grid(len(scene_data), 5)
    i = 0

    eps = 1e-3
    for name, image_gpu_name, image_cpu_name, image_ref_name in scene_data:
        img = sio.read(str(image_gpu_name))
        img_cpu = sio.read(str(image_cpu_name))
        ref_img = sio.read(str(image_ref_name))

        err = sio.relative_mse_outlier_rejection(img, ref_img, 0.001)
        err_cpu = sio.relative_mse_outlier_rejection(img_cpu, ref_img, 0.001)

        errors.append(err)
        cpu_errors.append(err_cpu)
        names.append(str(name))

        leps = predef_eps[name] if name in predef_eps else eps
        if not err < leps:
            print(f"-> Scene {name} fails on the GPU")
        if not err_cpu < leps:
            print(f"-> Scene {name} fails on the CPU")

        grid[i, 0].set_image(map_img(ref_img))
        grid[i, 1].set_image(map_img(img))
        grid[i, 2].set_image(map_img(error_image(img, ref_img)))
        grid[i, 2].set_frame(1, [0, 255, 0] if err < leps else [255, 0, 0])
        grid[i, 3].set_image(map_img(img_cpu))
        grid[i, 4].set_image(map_img(error_image(img_cpu, ref_img)))
        grid[i, 4].set_frame(1, [0, 255, 0] if err_cpu < leps else [255, 0, 0])

        i += 1

    grid.set_col_titles("top", ["Reference", "Render GPU",
                        "Rel. Error GPU", "Render CPU", "Rel. Error CPU"])
    grid.set_row_titles("left", names)
    grid.set_row_titles("right", [
                        f"RelMSE (GPU,CPU)\\\\ {err:.2E} | {cerr:.2E}" for err, cerr in zip(errors, cpu_errors)])
    grid.layout.set_row_titles("right", field_size_mm=10, fontsize=6)
    grid.layout.set_row_titles("left", field_size_mm=8, fontsize=6)

    rows = []
    rows.append([title])
    rows.append([grid])

    print("Writing summary to Evaluation.pdf")

    # Generate the figure with the pdflatex backend and default settings
    figuregen.figure(rows, width_cm=10, filename=f"{out_dir}/Evaluation.pdf")


if __name__ == '__main__':
    root_dir = get_root_dir()
    out_dir = os.path.join(root_dir, "build", "evaluation")
    ref_dir = os.path.join(root_dir, "scenes", "evaluation")

    parser = argparse.ArgumentParser()
    parser.add_argument('OutputDir', nargs='?', type=Path, default=out_dir,
                        help='Path to export evaluation data')
    parser.add_argument('--no-gpu', action="store_true",
                        help="Disable gpu evaluation")
    parser.add_argument('--no-cpu', action="store_true",
                        help="Disable cpu evaluation")
    parser.add_argument('--only-summary', action="store_true",
                        help="Only generate summary and do not rerun evaluations")
    parser.add_argument('--spp', type=int, default=1024,
                        help="Number of samples to acquire for evaluations")
    parser.add_argument('--reference_dir', type=Path, default=ref_dir,
                        help="Path to directory containing references")
    parser.add_argument("-f", "--filter", type=str,
                        help="Filter scene names and only include matching patterns")
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
    print(f"Evaluating Ignis {ignis.__version__} in {root_dir}")

    eval_dir = os.path.join(root_dir, "scenes", "evaluation")

    # Load all scene files but ignore files with -base in name
    scenes = []
    for file in os.listdir(eval_dir):
        if file.endswith('.json') and "-base" not in file:
            if args.filter:
                if not PurePath(Path(file).stem).match(args.filter):
                    continue
            scenes.append(os.path.join(eval_dir, file))

    if len(scenes) == 0:
        print("No scenes available. Maybe reset filter if used?")
        exit()

    if not args.only_summary:
        for scene in scenes:
            print(f"Evaluating {scene}")
            evaluate(ignis, scene, args)
            print("------------------------------------")

    make_figure(scenes, args)
