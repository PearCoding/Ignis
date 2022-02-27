#!/usr/bin/env python
# coding: utf-8

# This is testing a scene with full white interior
# As the camera itself is not consuming energy,
# constant energy is put into the scene, which is only limited by the max_path length

import os
script_dir = os.path.dirname(os.path.realpath(__file__))
root_dir   = os.path.join(script_dir, "..", "..")

build_dir  = os.path.join(root_dir, "build", "Release") if os.path.exists(os.path.join(root_dir, "build", "Release")) else os.path.join(root_dir, "build")
api_dir    = os.path.join(build_dir, "api")
mod_dir    = os.path.join(build_dir, "lib")

import numpy as np
import json

import sys
sys.path.insert(0, api_dir)
import ignis

def luminance(r,g,b):
    return 0.2126 * r + 0.7152 * g + 0.0722 * b

def image_mean(runtime):
    if runtime.iterationCount == 0:
        return 0
    
    pixels = np.asarray(runtime.getFramebuffer(0)) / runtime.iterationCount
    return np.average(luminance(pixels[:,:,0],pixels[:,:,1],pixels[:,:,2]))

def run_scene(max_path):
    options = ignis.RuntimeOptions()
    options.ModulePath = mod_dir

    scene = {
        "technique": { "type": "path", "max_depth": max_path },
        "externals": [ {"filename": os.path.join(root_dir, "scenes", "diamond_scene_uniform_closed.json")} ],
    }

    scene_str = json.dumps(scene)
    
    with ignis.loadFromString(scene_str, options) as runtime:
        if runtime is None:
            print("Could not load scene")
            mean = 0
        else:
            ignis.flush_log()
            
            for _i in range(16):
                runtime.step()
                ignis.flush_log()
            
            mean = image_mean(runtime)
    
    return mean

for i in [2,4,6,8]:
    print(f"[{i}] Mean {run_scene(i)}")