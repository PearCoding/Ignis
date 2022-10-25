import os
import sys
from pathlib import Path
import importlib
import json
import numpy as np


def get_root_dir():
    # Return root directory of the ignis setup

    this_dir = Path(os.path.realpath(os.path.dirname(__file__)))
    return this_dir.parent.parent.parent.parent


def _load_utils():
    # Load the utils and return a module object

    root_dir = get_root_dir()
    utils_dir = root_dir.joinpath("scripts", "api", "utils")
    if not utils_dir.exists():
        raise RuntimeError("Could not find the scripts/api/utils directory!")

    spec = importlib.util.spec_from_file_location(
        "utils", utils_dir.joinpath("__init__.py"))
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)

    return module


def load_api():
    return _load_utils().load_api()


def create_flat_scene():
    return {
        "technique": {
            "type": "path",
            "max_depth": 2
        },
        "camera": {
            "type": "perspective",
            "fov": 90,
            "near_clip": 0.01,
            "far_clip": 100,
            "transform": [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, -1]
        },
        "film": {
            "size": [1000, 1000]
        },
        "bsdfs": [
            {"type": "diffuse", "name": "ground",
                "reflectance": [1, 1, 1]}
        ],
        "shapes": [
            {"type": "rectangle", "name": "Bottom", "width": 2, "height": 2}
        ],
        "entities": [
            {"name": "Bottom", "shape": "Bottom", "bsdf": "ground"}
        ],
        "lights": []
    }


def compute_scene_average(scene, spp=8):
    scene_str = json.dumps(scene)

    ignis = load_api()
    with ignis.loadFromString(scene_str) as runtime:
        for _i in range(spp):
            runtime.step()
        return np.average(runtime.getFramebuffer())

