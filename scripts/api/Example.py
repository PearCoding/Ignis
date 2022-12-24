#!/usr/bin/env python3
# coding: utf-8

from utils import get_root_dir, load_api


def first_variant(scene_file):
    opts = ignis.RuntimeOptions.makeDefault()
    # opts.Target = ignis.Target.pickCPU()
    with ignis.loadFromFile(str(scene_file), opts) as runtime:
        if runtime is None:
            raise RuntimeError("Could not load scene")

        print(f"Loaded {runtime.Target}")
        for _i in range(16):
            runtime.step()

        ignis.flushLog()
        print(runtime.getFramebuffer())
        print("DONE")


def second_variant(scene_file):
    opts = ignis.RuntimeOptions.makeDefault()
    wrap = ignis.loadFromFile(str(scene_file), opts)

    if wrap is None:
        raise RuntimeError("Could not load scene")

    # Not the same as with the context manager above!
    runtime = wrap.instance

    print(f"Loaded {runtime.Target}")
    for _i in range(16):
        runtime.step()

    ignis.flushLog()
    print(runtime.getFramebuffer())
    print("DONE")

    # You have to explicitly shutdown the context
    wrap.shutdown()


if __name__ == "__main__":
    ignis = load_api()
    ignis.setVerbose(True)

    first_variant(get_root_dir().joinpath("scenes", "diamond_scene.json"))
    second_variant(get_root_dir().joinpath("scenes", "sky_scene.json"))
