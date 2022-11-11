#!/bin/python3

import simpleimageio as sio
import figuregen
import sys
import numpy as np
import matplotlib.cm as cm


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


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Invalid arguments. Expected result_dir ref_dir")
        exit(-1)

    result_dir = sys.argv[1]
    ref_dir = sys.argv[2]

    default_eps = 1e-4
    scenes = [
        ("plane", None, 1, default_eps),
        ("plane", None, 6, default_eps),
        ("cbox", None, 1, default_eps),
        ("cbox", None, 6, 5e-3),
        ("room", None, 4, 1e-3),
        ("emissive-plane", None, 4, default_eps),
        ("emissive-plane-nopt", "emissive-plane", 4, default_eps),
        ("emissive-plane-scale", None, 4, default_eps),
        ("emissive-plane-scale-nopt", "emissive-plane-scale", 4, default_eps),
        ("volume", None, 12, 5e-3),
        ("env", None, 6, default_eps),
        ("env4k", None, 6, 8e-2),
        ("env4kNoCDF", "env4k", 6, 8e-2),
        ("multilight-uniform", "multilight", 4, 3e-4),
        ("multilight-simple", "multilight", 4, 3e-4),
        ("multilight-hierarchy", "multilight", 4, 3e-4),
        ("point", None, 4, default_eps),
        ("sphere-light-ico", "sphere-light", 4, 2e-3),
        ("sphere-light-ico-nopt", "sphere-light", 4, 2e-3),
        ("sphere-light-uv", "sphere-light", 4, 2e-3),
        ("sphere-light-pure", "sphere-light", 4, 2e-3),
    ]

    image_names = [f"{scene}4096-d{depth}" for (scene, _, depth, _) in scenes]

    title = figuregen.Grid(1, 1)
    title.get_element(0, 0).set_image(
        figuregen.PNG(np.tile([255, 255, 255], (1, 500))))
    title.set_title("top", "Reference Evaluations")

    errors = []
    cpu_errors = []
    grid = figuregen.Grid(len(image_names), 5)
    i = 0
    for scene, ref_name, depth, eps in scenes:
        image_name = f"{scene}4096-d{depth}"
        image_cpu_name = f"{scene}4096-d{depth}-cpu"
        ref_name = "ref-" + (image_name if ref_name is None else f"{ref_name}4096-d{depth}")

        img = sio.read(f"{result_dir}/{image_name}.exr")
        img_cpu = sio.read(f"{result_dir}/{image_cpu_name}.exr")
        ref_img = sio.read(f"{ref_dir}/{ref_name}.exr")

        err = sio.relative_mse_outlier_rejection(img, ref_img, 0.001)
        err_cpu = sio.relative_mse_outlier_rejection(img_cpu, ref_img, 0.001)

        errors.append(err)
        cpu_errors.append(err_cpu)

        if not err < eps:
            print(f"-> Scene {scene} fails on the GPU")
        if not err_cpu < eps:
            print(f"-> Scene {scene} fails on the CPU")

        grid[i, 0].set_image(map_img(ref_img))
        grid[i, 1].set_image(map_img(img))
        grid[i, 2].set_image(map_img(error_image(img, ref_img)))
        grid[i, 2].set_frame(1, [0,255,0] if err < eps else [255,0,0])
        grid[i, 3].set_image(map_img(img_cpu))
        grid[i, 4].set_image(map_img(error_image(img_cpu, ref_img)))
        grid[i, 4].set_frame(1, [0,255,0] if err_cpu < eps else [255,0,0])

        i += 1

    grid.set_col_titles("top", ["Reference", "Render GPU", "Rel. Error GPU", "Render CPU", "Rel. Error CPU"])
    grid.set_row_titles(
        "left", [f"{scene}-d{depth}" for (scene, _, depth, _) in scenes])
    grid.set_row_titles("right", [f"RelMSE (GPU,CPU)\\\\ {err:.2E} | {cerr:.2E}" for err, cerr in zip(errors, cpu_errors)])
    grid.layout.set_row_titles("right", field_size_mm=10, fontsize=6)
    grid.layout.set_row_titles("left", field_size_mm=8, fontsize=6)

    rows = []
    rows.append([title])
    rows.append([grid])

    print("Writing output to Evaluation.pdf")

    # Generate the figure with the pdflatex backend and default settings
    figuregen.figure(rows, width_cm=10, filename="Evaluation.pdf")
