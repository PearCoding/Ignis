import json
from jsonmerge import merge
from extractDeclarations import extractDeclarations
from restructureDeclarations import restructureDeclarations
from transformation import Transformation
import logging
import os
import config

def parseRad(radFilePaths, tforms = Transformation(), depth = 1):    # a tform can be passed with the rad file, which will be applied to all the objects in it

    if depth > 5:
        logging.error("parseRad() can not have more than 5 recurssions.")
        exit()

    jsonData = json.dumps(None, indent=4)

    #parse the input file(s): single file if called from rad2json, possibly multiple files if called from xform command
    for radFilePath in radFilePaths:

        #NOTE: rad files can call more rad files using xform. 
        #Thus, we maintain their paths in a stack to get the cumulative path (wrt the execution directory).
        config.path_stack.append(os.path.dirname(radFilePath))      #assuming a relative (and not absolute) path for now
        path = os.path.join(*config.path_stack)     #get the cumulative path
        logging.info(f"{os.path.basename(radFilePath)} file path: {path}")
       
        radFilePath = os.path.join(path, os.path.basename(radFilePath))

        if (not os.path.exists(radFilePath)):
            logging.error(f"The specified input file {radFilePath} does not exist.")
            exit()

        with open(radFilePath, 'r') as radFile:
            declarations = extractDeclarations(radFile)

        #structure the data
        jsonDataPerFile = restructureDeclarations(declarations, tforms, depth)

        jsonData = merge(jsonData, jsonDataPerFile)

        #once a file (and thus all the rad files it calls) is processed, its path can be removed from the stack
        config.path_stack.pop()

    return jsonData