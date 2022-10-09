#!/usr/bin/env python3
# coding: utf-8

from utils import get_root_dir, load_api


if __name__ == "__main__":
    ignis = load_api()
    ignis.setVerbose(True)
    print(dir(ignis))

    scene_file = get_root_dir().joinpath("scenes", "diamond_scene.json")

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

