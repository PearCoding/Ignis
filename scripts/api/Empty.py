#!/usr/bin/env python3
# coding: utf-8

from utils import load_api


if __name__ == "__main__":
    ignis = load_api()
    ignis.setVerbose(True)

    opts = ignis.RuntimeOptions.makeDefault()
    scene = ignis.Scene()

    with ignis.loadFromScene(scene, opts) as runtime:
        if runtime is None:
            raise RuntimeError("Could not load empty scene")

        print(f"Loaded {runtime.Target}")
        while runtime.SampleCount < 16:
            runtime.step()

        ignis.flushLog()
        print(runtime.getFramebufferForHost())
        print("DONE")
