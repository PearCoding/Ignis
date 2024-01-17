#!/bin/python3
# Flatten EXR file. This means only the default RGBA channel will be kept

import argparse
import simpleimageio as sio
from pathlib import Path

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('InputFile', type=Path,
                        help='Path to a .exr file')
    parser.add_argument('OutputFile', type=Path,
                        help='Path to export')

    args = parser.parse_args()
    input_path = args.InputFile
    output_path = args.OutputFile

    if (not input_path.exists()):
        print("The specified file does not exist.")
        exit(-1)

    img = sio.read_layered_exr(str(input_path))
    layer = img[''] if '' in img else img['Default']
    sio.write(str(output_path), layer)
