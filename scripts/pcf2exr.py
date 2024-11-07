#!/bin/python3
# Converts (LMK) pcf (CIERGB E) files to linear exr (sRGB)

import argparse
import simpleimageio as sio
import numpy as np
from pathlib import Path
import os
import re

WIDTH_RE = re.compile(r'.*Columns=(\d+)', re.DOTALL)
HEIGHT_RE = re.compile(r'.*Lines=(\d+)', re.DOTALL)

def ciergb_to_xyz(rgb):
    """ Convert CIE-RGB to XYZ with Illuminant E"""
    # Matrix from TechnoTeam's LMK4.pdf (p309, February 7, 2019 version)
    M = np.array([[2.7689,  1.7518,  1.1302],
                  [1.0000,  4.5907,  0.0601],
                  [0.0000,  0.0565,  5.5943]])
    
    # M = np.array([[ 0.4887180, 0.3106803, 0.2006017],
    #               [ 0.1762044, 0.8129847, 0.0108109],
    #               [ 0.0000000, 0.0102048, 0.9897952]])
    return (rgb @ M.T)


def e_to_d65(xyz):
    """ Convert from Illuminant E to Illuminant D65 (Bradford)"""
    # Matrix from http://www.brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html
    M = np.array([[ 0.9531874, -0.0265906, 0.0238731],
                  [-0.0382467,  1.0288406, 0.0094060],
                  [ 0.0026068, -0.0030332, 1.0892565]])
    return (xyz @ M.T)


def d65_to_e(xyz):
    """ Convert from Illuminant E to Illuminant D65 (Bradford)"""
    # Matrix from http://www.brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html
    M = np.array([[ 1.0502616, 0.0270757, -0.0232523],
                  [ 0.0390650, 0.9729502, -0.0092579],
                  [-0.0024047, 0.0026446,  0.9180873]])
    return (xyz @ M.T)


def xyz_to_srgb(xyz):
    """ Convert XYZ to sRGB with Illuminant D65"""
    # Matrix from http://www.brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html
    M = np.array([[ 3.2405, -1.5371, -0.4985],
                  [-0.9693,  1.8760,  0.0416],
                  [ 0.0556, -0.2040,  1.0572]])
    return (xyz @ M.T)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('InputFile', type=Path,
                        help='Path to a .pcf file')
    parser.add_argument('OutputFile', nargs='?', type=Path, default=None,
                        help='Path to export')

    args = parser.parse_args()
    input_path = args.InputFile
    output_path = args.OutputFile if args.OutputFile is not None else (
        os.path.splitext(input_path)[0] + ".exr")

    if (not input_path.exists()):
        print("The specified file does not exist.")
        exit(-1)

    with open(input_path, "rb") as file:
        header = []
        while True:
            c = file.read(1)[0]
            if c == 0:
                break
            header.append(chr(c))

        header = "".join(header)
        header = header.strip()
        # print(header)

        width = WIDTH_RE.match(header)
        height = HEIGHT_RE.match(header)

        if width:
            width = int(width.group(1))
        if height:
            height = int(height.group(1))

        if width is None or height is None:
            print("Given pcf is invalid")
            exit(-1)

        n = width * height * 3
        data = np.fromfile(file, dtype=np.float32, count=n, sep='')
        data = data.reshape((height, width, 3)) # HxWxC
        data = np.flip(data, axis=2) # BGR -> RGB

        data = ciergb_to_xyz(data)
        # data = e_to_d65(data)
        data = xyz_to_srgb(data)
        # data = d65_to_e(data)

        sio.write(str(output_path), data)
