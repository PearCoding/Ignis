#!/usr/bin/env python3
# coding: utf-8

# This is testing a scene with full white interior
# As the camera itself is not consuming energy,
# constant energy is put into the scene, which is only limited by the max_path length

import os
import numpy as np
import json

from utils import get_root_dir, load_api


def luminance(r, g, b):
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def image_mean(runtime):
    if runtime.IterationCount == 0:
        return 0

    pixels = np.asarray(runtime.getFramebuffer()) / runtime.IterationCount
    return np.average(luminance(pixels[:, :, 0], pixels[:, :, 1], pixels[:, :, 2]))


def run_scene(max_path):
    options = ignis.RuntimeOptions.makeDefault(False) # False means no trace, but standard rendering

    scene = {
        "technique": {"type": "path", "max_depth": max_path},
        "externals": [{"filename": os.path.join(get_root_dir(), "scenes", "diamond_scene_uniform_closed.json")}],
    }

    scene_str = json.dumps(scene)

    mean = 0
    with ignis.loadFromString(scene_str, options) as runtime:
        if runtime is None:
            print("Could not load scene")
            mean = 0
        else:
            for _i in range(16):
                runtime.step()

            mean = image_mean(runtime)

    return mean


if __name__ == "__main__":
    ignis = load_api()
    for i in [2, 4, 6, 8]:
        if i != 2:
            ignis.setQuiet(True)
        print(f"[{i}] Mean {run_scene(i)}")
