#!/bin/python3
# Take absolute difference of two images and output a colormapped image

import argparse
import simpleimageio as sio
from pathlib import Path
import matplotlib as mp
import numpy as np

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('InputFile1', type=Path,
                        help='Path to a .exr file')
    parser.add_argument('InputFile2', type=Path,
                        help='Path to a .exr file')
    parser.add_argument('OutputFile', type=Path,
                        help='Path to export')
    parser.add_argument('-e', '--exposure', type=float,
                        help='Exposure to apply', default=1)
    parser.add_argument('-o', '--offset', type=float,
                        help='Exposure to apply', default=0)

    args = parser.parse_args()
    input_path1 = args.InputFile1
    input_path2 = args.InputFile2
    output_path = args.OutputFile

    if (not input_path1.exists()):
        print("The specified file does not exist.")
        exit(-1)
    if (not input_path2.exists()):
        print("The specified file does not exist.")
        exit(-1)

    img1 = sio.read_layered_exr(str(input_path1))
    layer1 = img1[''] if '' in img1 else img1['Default']
    img2 = sio.read_layered_exr(str(input_path2))
    layer2 = img2[''] if '' in img2 else img2['Default']
    
    diff = np.absolute(layer1[:,:,1:3] - layer2[:,:,1:3])
    avg = np.power(2, args.exposure) * np.average(diff, axis=2) + args.offset
    
    cmap = mp.colormaps.get_cmap('inferno')
    output = cmap(np.clip(avg, 0, 1))
    
    sio.write(str(output_path), output)
    
    max = np.amax(diff)
    min = np.amin(diff)
    print(f"Min {min} Max {max}")
