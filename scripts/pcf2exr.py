#!/bin/python3
# Converts pcf files to exr

import argparse
import simpleimageio as sio
import numpy as np
from pathlib import Path
import os
import re

WIDTH_RE = re.compile(r'.*Columns=(\d+)', re.DOTALL)
HEIGHT_RE = re.compile(r'.*Lines=(\d+)', re.DOTALL)

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

        sio.write(str(output_path), data)
