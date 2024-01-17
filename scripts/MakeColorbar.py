#!/bin/python3
# Take absolute difference of two images and output a colormapped image

import argparse
import simpleimageio as sio
from pathlib import Path
import matplotlib.pyplot as plt
import matplotlib as mpl

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('OutputFile', type=Path,
                        help='Path to export')
    parser.add_argument('-s', '--start', type=float,
                        help='Start of the colorbar', default=0)
    parser.add_argument('-e', '--end', type=float,
                        help='End of the colorbar', default=1)
    parser.add_argument('--vertical', action="store_true", help="Make colormap vertical instead of horizontal")
    parser.add_argument('--colormap', type=str, help="Matplotlib colormap", default="inferno")

    args = parser.parse_args()
    
    output_path = args.OutputFile
    
    fig = plt.figure()
    
    w=0.05
    if args.vertical:
        ax = fig.add_axes([0.80, 0.5, w, 0.9])
        cb = mpl.colorbar.ColorbarBase(ax, orientation='vertical', cmap=args.colormap)
    else:
        ax = fig.add_axes([0.05, 0.80, 0.9, w])
        cb = mpl.colorbar.ColorbarBase(ax, orientation='horizontal', cmap=args.colormap)

    plt.savefig(str(output_path), bbox_inches='tight', transparent=True)
