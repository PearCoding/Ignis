#!/bin/python3
# Converts radiance scene file to Ignis scene description

#TODO:
#handle -i flag in xform when passed on its own (i.e. independent of -a flag)
#simplify how the transformation matrix of the camera (and others) is passed: a lookat dictionary can be passed now. (Note target is a position, target = origin + direction).y

import argparse
import numpy as np
from pathlib import Path
import os
import logging
import config
from parseRad import parseRad


if __name__ == "__main__":
    
    parser = argparse.ArgumentParser()
    parser.add_argument('InputFile', type=Path,
                        help='Path of the .rad file to be parsed.')
    parser.add_argument('OutputFile', nargs='?', type=Path, default=None,
                        help='Path to export')
    parser.add_argument('-d', '--debug', help="Print lots of debugging statements",
                        action="store_const", dest="loglevel", const=logging.DEBUG,
                        default=logging.WARNING,)
    parser.add_argument('-v', '--verbose', help="Be verbose",
                        action="store_const", dest="loglevel", const=logging.INFO,)
    args = parser.parse_args()    
    logging.basicConfig(level=args.loglevel)
    # logging.basicConfig(level=args.loglevel, filename='mts2json.log', format='%(asctime)s %(levelname)s:%(message)s')

    #get the input file
    radFilePath = args.InputFile
    if (not radFilePath.suffix == '.rad'):
        logging.error("The specified input file is not a .rad file.")
        exit()

    config.RAD_FILE_FOLDER = os.path.relpath(os.path.dirname(os.path.abspath(radFilePath)))         #doing abspath() to give path to a file when it is in the current folder
    logging.info(f"Relative path of the master rad file: {config.RAD_FILE_FOLDER}")
    radFilePath = os.path.join(config.RAD_FILE_FOLDER, os.path.basename(radFilePath))

    #output path
    jsonFilePath = args.OutputFile
    if jsonFilePath is None:
        jsonFilePath = os.path.splitext(radFilePath)[0]+'.json'
    config.JSON_FILE_FOLDER = os.path.dirname(jsonFilePath)

    #parse the .rad file to Ignis compatible .json format
    jsonData = parseRad([radFilePath])


    #write data in the output file
    with open(jsonFilePath, "w") as jsonFile:
        jsonFile.write(jsonData)

    print(f"Scene written to {jsonFilePath}.")
