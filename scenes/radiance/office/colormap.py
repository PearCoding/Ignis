#!/bin/python3

import simpleimageio as sio
import numpy as np
import matplotlib.cm as cm
import sys


def colormap(img, cmap=cm.inferno):
    return cm.ScalarMappable(cmap=cmap).to_rgba(img)[..., 0:3]


def colorbar(cmap=cm.inferno):
    gradient = np.linspace(1, 0, 256)
    gradient = np.vstack((gradient, gradient))
    bar = np.swapaxes(cmap(gradient), 0, 1)
    bar = np.repeat(bar, 5, axis=1)
    return bar


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Invalid arguments. Expected src dst")
        exit(-1)

    img = sio.read(sys.argv[1])
    sio.write(sys.argv[2], colormap(
        np.clip(np.average(sio.exposure(img, -6), axis=2), 0, 1)))

    sio.write("colorbar.png", colorbar())
