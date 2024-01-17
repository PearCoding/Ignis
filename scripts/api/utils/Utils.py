import os
import sys
from importlib import util as imp_util
from pathlib import Path


def get_root_dir():
    # Return root directory of the ignis setup

    script_dir = Path(os.path.realpath(os.path.dirname(__file__)))
    return script_dir.parent.parent.parent


def load_api():
    # Load the api and return a module object

    root_dir = get_root_dir()
    build_dir = root_dir.joinpath("build")
    if not build_dir.exists():
        raise RuntimeError("Could not find the build directory!")

    # Some setups have an additional "Release" folder
    build_dir2 = build_dir.joinpath("Release")
    if build_dir2.exists():
        build_dir = build_dir2

    api_dir = build_dir.joinpath("api")
    if not api_dir.exists():
        raise RuntimeError(
            "Could not find the api directory! Maybe the Python API is not enabled via cmake?")

    ig_dir = api_dir.joinpath("ignis")
    if not ig_dir.exists():
        raise RuntimeError(
            "Could not find the ignis directory inside the api directory! Maybe the Python API is not enabled via cmake?")

    spec = imp_util.spec_from_file_location(
        "ignis", ig_dir.joinpath("__init__.py"))
    module = imp_util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)

    return module
