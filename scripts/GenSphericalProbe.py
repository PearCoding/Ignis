#!/bin/python3
# Generates a list of rays with spherical distributed directions to be used with igtrace or other tracers

import numpy as np
import sys

THETA_C = 16
PHI_C = 32


def gen_directions():
    theta = np.linspace(-np.pi/2, np.pi/2, THETA_C)
    phi = np.linspace(0, 2*np.pi, PHI_C)
    theta_v, phi_v = np.meshgrid(theta, phi)

    x = np.sin(theta_v) * np.sin(phi_v)
    y = np.sin(theta_v) * np.cos(phi_v)
    z = np.cos(theta_v)

    return np.stack([x.flatten(), y.flatten(), z.flatten()])


if __name__ == "__main__":
    Origin = (0, 0, 0)
    if len(sys.argv) >= 4:
        Origin = (sys.argv[1], sys.argv[2], sys.argv[3])

    if len(sys.argv) >= 6:
        THETA_C = int(sys.argv[4])
        PHI_C = int(sys.argv[5])

    if len(sys.argv) == 5:
        THETA_C = int(sys.argv[4])
        PHI_C = THETA_C * 2

    directions = gen_directions()
    for i in range(0, directions.shape[1]):
        print(
            f"{Origin[0]} {Origin[1]} {Origin[2]} {directions[0, i]} {directions[1, i]} {directions[2, i]}")
