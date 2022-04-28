#!/bin/python3

import simpleimageio as sio
import figuregen
import sys
import os
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

    scenes = [
        ("plane", 1),
        ("plane", 6),
        ("cbox", 1),
        ("cbox", 6),
        ("room", 4),
        ("plane-scale", 4),
        ("volume", 12),
    ]

    image_names = [f"{scene}4096-d{depth}" for (scene, depth) in scenes]
    ref_names = ["ref-" + name for name in image_names]

    title = figuregen.Grid(1, 1)
    title.get_element(0, 0).set_image(
        figuregen.PNG(np.tile([255, 255, 255], (1, 500))))
    title.set_title("top", "Reference Evaluations")

    errors = []
    grid = figuregen.Grid(len(image_names), 3)
    i = 0
    for scene, depth in scenes:
        image_name = f"{scene}4096-d{depth}"
        ref_name = "ref-" + image_name

        img = sio.read(f"{result_dir}/{image_name}.exr")
        ref_img = sio.read(f"{ref_dir}/{ref_name}.exr")

        errors.append(sio.relative_mse_outlier_rejection(img, ref_img, 0.001))

        grid[i, 0].set_image(map_img(ref_img))
        grid[i, 1].set_image(map_img(img))
        grid[i, 2].set_image(map_img(error_image(img, ref_img)))

        i += 1

    grid.set_col_titles("top", ["Reference", "Render", "Rel. Error"])
    grid.set_row_titles(
        "left", [f"{scene}-d{depth}" for (scene, depth) in scenes])
    grid.set_row_titles("right", [f"RelMSE {err:.2E}" for err in errors])

    rows = []
    rows.append([title])
    rows.append([grid])

    print("Writing output to Evaluation.pdf")

    # Generate the figure with the pdflatex backend and default settings
    figuregen.figure(rows, width_cm=10, filename="Evaluation.pdf")
