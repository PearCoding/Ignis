import bpy
import sys
from pathlib import Path
import importlib

from . import addon_preferences


def load_api():
    addon_prefs = addon_preferences.get_prefs()

    if not addon_prefs.api_dir:
        raise RuntimeError("Ignis API directory not given!")

    ig_dir = Path(bpy.path.resolve_ncase(
        bpy.path.abspath(addon_prefs.api_dir)))
    if not ig_dir.exists():
        raise RuntimeError(
            f"Given API directory '{ig_dir}' does not exist!")

    spec = importlib.util.spec_from_file_location(
        "ignis", ig_dir.joinpath("__init__.py"))
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)

    return module


def check_api():
    try:
        return load_api() is not None
    except:
        return False
