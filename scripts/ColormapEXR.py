#!/bin/python3
# Colormap image

import argparse
import simpleimageio as sio
from pathlib import Path
import matplotlib as mp
import numpy as np

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('InputFile', type=Path,
                        help='Path to a .exr file')
    parser.add_argument('OutputFile', type=Path,
                        help='Path to export')
    parser.add_argument('-e', '--exposure', type=float,
                        help='Exposure to apply', default=1)
    parser.add_argument('-o', '--offset', type=float,
                        help='Exposure to apply', default=0)

    args = parser.parse_args()
    input_path = args.InputFile
    output_path = args.OutputFile

    if (not input_path.exists()):
        print("The specified file does not exist.")
        exit(-1)

    img = sio.read_layered_exr(str(input_path))
    layer = img[''] if '' in img else img['Default']
    
    avg = np.power(2, args.exposure) * np.average(layer, axis=2) + args.offset
    cmap = mp.colormaps.get_cmap('inferno')
    output = cmap(np.clip(avg, 0, 1))
        
    sio.write(str(output_path), output)
