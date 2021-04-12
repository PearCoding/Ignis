#!/bin/python3
# Converts mitsuba (2.0) project file to Ignis scene description

import argparse
import json
import numpy as np
from pathlib import Path
from lxml import etree
import re
import os

lightCount = 0
nameCount = 0
matCount = 0
texCount = 0
defaults = {}
bsdfs = []
textures = []


def getDefaults(child):
    global defaults
    var = child.attrib['name']
    val = child.attrib['value']
    defaults[var] = val


def toInt(var):
    try:
        var = int(var)
    except:
        varname = var.split('$')[1]
        var = int(defaults[varname])
    return var


def toFloat(var):
    try:
        var = float(var)
    except:
        varname = var.split('$')[1]
        var = float(defaults[varname])
    return var


CamelCasePattern = pattern = re.compile(r'((?<=[a-z0-9])[A-Z]|(?!^)[A-Z](?=[a-z]))')
def toSnakeCase(name):
    return pattern.sub(r'_\1', name).lower()


def computeTransformation(child):
    matrix = []
    # Start with Identity Matrix to build up the final transformation matrix
    matrix_transform = np.identity(4)

    for transform in child:

        # Start with identity matrix and only modify relevant entries
        matrix_action = np.identity(4)

        if (transform.tag == "translate"):
            vec = transform.attrib['value']
            vec = re.split(',\s|\s', vec)

            matrix_action[0][3] = vec[0]
            matrix_action[1][3] = vec[1]
            matrix_action[2][3] = vec[2]

        elif (transform.tag == "rotate"):
            # In this case, the rotation is around a vector
            if ("value" in transform.attrib):
                vec = transform.attrib['value']
                vec = re.split(',\s|\s', vec)
                angle = toFloat(transform.attrib['angle'])
                sine = np.sin(angle)
                cosine = np.cos(angle)
                x = toFloat(vec[0])
                y = toFloat(vec[1])
                z = toFloat(vec[2])

                matrix_action[0][0] = cosine + x*x*(1-cosine)
                matrix_action[0][1] = x*y*(1-cosine) - z*sine
                matrix_action[0][2] = x*z*(1-cosine) - y*sine
                matrix_action[1][0] = y*x*(1-cosine) + z * sine
                matrix_action[1][1] = cosine + y*y*(1-cosine)
                matrix_action[1][2] = y * z * (1-cosine) - x*sine
                matrix_action[2][0] = z*x*(1-cosine) - y * sine
                matrix_action[2][1] = z*y*(1-cosine) + x*sine
                matrix_action[2][2] = cosine + z*z*(1-cosine)
            # In this case, the rotation is around x-axis
            elif ("x" in transform.attrib):
                angle = toFloat(transform.attrib['angle'])
                sine = np.sin(angle)
                cosine = np.cos(angle)

                matrix_action[1][1] = cosine
                matrix_action[1][2] = -sine
                matrix_action[2][1] = sine
                matrix_action[2][2] = cosine
            # In this case, the rotation is around y-axis
            elif ("y" in transform.attrib):
                angle = toFloat(transform.attrib['angle'])
                sine = np.sin(angle)
                cosine = np.cos(angle)

                matrix_action[0][0] = cosine
                matrix_action[0][2] = sine
                matrix_action[2][0] = -sine
                matrix_action[2][2] = cosine
            # In this case, the rotation is around z-axis
            elif ("z" in transform.attrib):
                angle = toFloat(transform.attrib['angle'])
                sine = np.sin(angle)
                cosine = np.cos(angle)

                matrix_action[0][0] = cosine
                matrix_action[0][1] = -sine
                matrix_action[1][0] = sine
                matrix_action[1][1] = cosine

        elif (transform.tag == "scale"):
            vec = transform.attrib['value']
            vec = re.split(',\s|\s', vec)
            if (len(vec) == 1):
                matrix_action[0][0] = vec[0]
                matrix_action[1][1] = vec[0]
                matrix_action[2][2] = vec[0]

            else:
                matrix_action[0][0] = vec[0]
                matrix_action[1][1] = vec[1]
                matrix_action[2][2] = vec[2]

        elif (transform.tag == "lookat"):
            origin = re.split(',\s|\s', transform.attrib['origin'])
            target = re.split(',\s|\s', transform.attrib['target'])
            origin = np.array([toInt(o) for o in origin])
            target = np.array([toInt(t) for t in target])
            if ('up' in child.attrib):
                up = re.split(',\s|\s', transform.attrib['up'])
                up = np.array([toInt(u) for u in up])
            else:
                up = np.array([0, 1, 0])

            fwd_h = np.array(
                [target[0]-origin[0], target[1]-origin[1], target[2]-origin[2]])
            fwd = fwd_h / np.linalg.norm(fwd_h)
            left_h = np.cross(up, fwd)
            left = left_h / np.linalg.norm(left_h)
            alt_up_h = np.cross(fwd, left)
            alt_up = alt_up_h / np.linalg.norm(alt_up_h)

            matrix_action[0][0] = left[0]
            matrix_action[0][1] = alt_up[0]
            matrix_action[0][2] = fwd[0]
            matrix_action[0][3] = origin[0]
            matrix_action[1][0] = left[1]
            matrix_action[1][1] = alt_up[1]
            matrix_action[1][2] = fwd[1]
            matrix_action[1][3] = origin[1]
            matrix_action[2][0] = left[2]
            matrix_action[2][1] = alt_up[2]
            matrix_action[2][2] = fwd[2]
            matrix_action[2][3] = origin[2]

        elif(transform.tag == "matrix"):
            vec = transform.attrib['value']
            vec = re.split(',\s|\s', vec)

            matrix_action[0][0] = vec[0]
            matrix_action[0][1] = vec[1]
            matrix_action[0][2] = vec[2]
            matrix_action[0][3] = vec[3]
            matrix_action[1][0] = vec[4]
            matrix_action[1][1] = vec[5]
            matrix_action[1][2] = vec[6]
            matrix_action[1][3] = vec[7]
            matrix_action[2][0] = vec[8]
            matrix_action[2][1] = vec[9]
            matrix_action[2][2] = vec[10]
            matrix_action[2][3] = vec[11]
            matrix_action[3][0] = vec[12]
            matrix_action[3][1] = vec[13]
            matrix_action[3][2] = vec[14]
            matrix_action[3][3] = vec[15]
        # This happens e.g. for the transformation name, or for invalid transformation steps
        else:
            print("Unknown transfomation")
        # Compute the transformation step to the end-Transformation matrix
        matrix_transform = matrix_action.dot(matrix_transform)
    matrix = [item for sublist in matrix_transform for item in sublist]
    return matrix


def getAdditionalProperties(child, res_dict):
    global texCount, textures
    for elem in child:
        if len(elem):
            if(elem.tag == "film" or elem.tag == "transform" or elem.tag == "sampler" or elem.tag == "bsdf"):
                continue
            if(elem.tag == "texture"):
                tex_name = "texture_" + str(texCount)
                texCount = texCount + 1
                prop = toSnakeCase(elem.attrib['name'])
                res_dict[prop] = tex_name
                getTexture(elem, textures, tex_name)
                continue
            getAdditionalProperties(elem, res_dict)
        else:
            if(elem.tag == "integer"):
                if (child.tag == "camera" or child.tag == "film"):
                    continue  # Already done
                prop = toSnakeCase(elem.attrib['name'])
                val = toInt(elem.attrib['value'])
                res_dict[prop] = val
            elif (elem.tag == "rgb"):
                prop = toSnakeCase(elem.attrib['name'])
                vec = elem.attrib['value']
                vec = re.split(',\s|\s', vec)
                vec = [toFloat(v) for v in vec]
                res_dict[prop] = vec
            elif (elem.tag == "float"):
                prop = toSnakeCase(elem.attrib['name'])
                val = toFloat(elem.attrib['value'])
                res_dict[prop] = val
            elif (elem.tag == "point" or elem.tag == "vector"):
                prop = toSnakeCase(elem.attrib['name'])
                vec = [0, 0, 0]
                if('value' in elem.attrib):
                    vec = elem.attrib['value']
                    vec = re.split(',\s|\s', vec)
                else:
                    if('x' in elem.attrib):
                        vec[0] = elem.attrib['x']
                    if('y' in elem.attrib):
                        vec[1] = elem.attrib['y']
                    if('z' in elem.attrib):
                        vec[2] = elem.attrib['z']
                vec = [toFloat(v) for v in vec]
                res_dict[prop] = vec
            elif (elem.tag == "spectrum"):
                prop = toSnakeCase(elem.attrib['name'])
                if ('value' in elem.attrib):
                    vec = elem.attrib['value']
                    vec = re.split(',\s|\s', vec)
                    vec = [toFloat(v) for v in vec]
                    res_dict[prop] = vec
                elif ('filename' in elem.attrib):
                    res_dict[prop] = elem.attrib['filename']
            elif (elem.tag == "boolean" or elem.tag == "string"):
                val = elem.attrib['value']
                prop = toSnakeCase(elem.attrib['name'])
                res_dict[prop] = val
            elif(elem.tag == "rfilter"):
                res_dict["rfilter"] = elem.attrib['type']
            elif(elem.tag == "ref"):
                continue  # can be ignored atm
            else:
                if (not str(elem.tag).startswith("<cyfunction") and not elem.tag == "bsdf"):
                    print("Found unhandled element:", elem.tag)
                    print(res_dict)


def computeTechnique(child, res_dict):
    res_dict["type"] = child.attrib['type']  # Required
    getAdditionalProperties(child, res_dict)


def computeCameraFilm(child, camera_dict, film_dict, smapler_dict):
    # Camera
    if ('type' in child.attrib):
        camera_dict['type'] = child.attrib['type']
    getAdditionalProperties(child, camera_dict)

    if (len(child.find('transform'))):
        camera_dict['transform'] = {
            'matrix':  computeTransformation(child.find('transform'))}

    # Film
    film = child.find('film')
    film_dict['type'] = film.attrib['type']
    x = -1
    y = -1
    for elem in film:
        if(elem.tag == "integer"):
            if(elem.attrib['name'] == 'width'):
                x = toInt(elem.attrib['value'])
            else:
                y = toInt(elem.attrib['value'])
    film_dict["size"] = [x, y]
    getAdditionalProperties(film, film_dict)

    if(child.find('sampler') is not None):
        sampler = child.find('sampler')
        smapler_dict['type'] = sampler.attrib['type']
        getAdditionalProperties(sampler, smapler_dict)


def computeBsdfs(child, bsdf_dict):
    global matCount
    if ('id' in child.attrib):
        bsdf_name = child.attrib['id']
    else:
        bsdf_name = "material_" + str(matCount)
        matCount = matCount + 1
    bsdf_type = child.attrib['type']
    bsdf = {"type": bsdf_type, "name": bsdf_name}

    if(bsdf_type == 'blendbsdf'):
        childs = []
        for elem in child:
            if (elem.tag == "bsdf"):
                childs.append(computeBsdfs(elem, bsdf_dict))
        bsdf["first"] = childs[0]
        bsdf["second"] = childs[1]
        getAdditionalProperties(child, bsdf)
    else:
        for elem in child:
            if(elem.tag == "ref"):
                if ('name' in elem.attrib):
                    refname = elem.attrib["name"]
                    bsdf[refname] = elem.attrib['id']
                else:
                    bsdf["texture"] = elem.attrib['id']
            elif (elem.tag == "bsdf"):
                bsdf_child = computeBsdfs(elem, bsdf_dict)
                bsdf['bsdf'] = bsdf_child
        getAdditionalProperties(child, bsdf)

    bsdf_dict.append(bsdf)
    return bsdf_name


def getTexture(child, texture, tex_name):
    tex_type = child.attrib['type']

    tex = {"type": tex_type, "name": tex_name}
    getAdditionalProperties(child, tex)

    texture.append(tex)


def computeTextures(child, textures):
    tex_name = child.attrib['name']
    tex_type = child.attrib['type']

    texture = {"type": tex_type, "name": tex_name}
    getAdditionalProperties(child, texture)

    textures.append(texture)


def computeLights(child, light_dict):
    global lightCount
    light = {}
    name = "light_" + str(lightCount)
    lightCount = lightCount + 1
    light["name"] = name
    light["type"] = child.attrib['type']
    getAdditionalProperties(child, light)
    light_dict.append(light)


def computeShapes(child, shape_dict, entity_dict, light_dict):

    shape_type = child.attrib['type']
    shape = {"type": shape_type}
    entity = {}
    global nameCount, bsdfs, matCount

    name = child.attrib['type'] + str(nameCount)
    nameCount = nameCount + 1
    for elem in child:
        if(elem.tag == "string"):
            filename = elem.attrib['value']
            name = filename.split("/")[-1].split(".")[0]
            shape["filename"] = filename
        if (elem.tag == "bsdf" or elem.tag == "ref"):
            bsdf_name = ""
            if(elem.tag == "bsdf"):
                bsdf_name = computeBsdfs(elem, bsdfs)
            else:
                if ('name' in elem.attrib):
                    bsdf_name = elem.attrib["name"]
                    entity[bsdf_name] = elem.attrib['id']
                else:
                    bsdf_name = elem.attrib['id']
            entity["bsdf"] = bsdf_name

        if (elem.tag == "emitter"):  # isLight
            light = {}
            light["name"] = name
            light["type"] = elem.attrib['type']
            light["entity"] = name
            getAdditionalProperties(elem, light)
            light_dict.append(light)
        if (elem.tag == "transform"):
            entity['transform'] = {'matrix':  computeTransformation(elem)}
    getAdditionalProperties(child, entity)
    getAdditionalProperties(child, shape)

    shape["name"] = name
    entity["name"] = name
    entity["shape"] = name

    shape_dict.append(shape)
    entity_dict.append(entity)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('InputFile', type=Path,
                        help='Path of the XML file to be converted.')
    parser.add_argument('OutputFile', nargs='?', type=Path, default=None,
                        help='Path to export')

    xmlFilePath = parser.parse_args().InputFile

    if (not xmlFilePath.exists()):
        print("The specified file does not exist.")
        exit()
    if (not xmlFilePath.suffix == '.xml'):
        print("The specified file is not a XML file.")
        exit()

    with open(xmlFilePath) as xmlFile:
        tree = etree.parse(xmlFile)
    xmlFile.close()

    technique = {}
    camera = {}
    film = {}
    shapes = []
    entities = []
    lights = []
    sampler = {}

    for child in tree.getroot():
        if (child.tag == "integrator"):
            computeTechnique(child, technique)
        elif (child.tag == "sensor"):
            computeCameraFilm(child, camera, film, sampler)
        elif (child.tag == "bsdf"):
            computeBsdfs(child, bsdfs)
        elif (child.tag == "shape"):
            if ('type' in child.attrib):
                if (child.attrib["type"] == "shapegroup"):
                    for elem in child:
                        computeShapes(elem, shapes, entities, lights)
                    continue
            computeShapes(child, shapes, entities, lights)
        elif(child.tag == "texture"):
            computeTextures(child, textures)
        elif(child.tag == "default"):
            getDefaults(child)
        elif(child.tag == "emitter"):
            computeLights(child, lights)
        else:
            if (not str(child.tag).startswith("<cyfunction")):
                print("Found unhandled element:", elem.tag)

    res_dict = {}
    if (technique):
        res_dict['technique'] = technique
    # Those two are required
    res_dict['camera'] = camera
    res_dict['film'] = film
    if (sampler):
        res_dict['sampler'] = sampler
    if (textures):
        res_dict['textures'] = textures
    if (bsdfs):
        res_dict['bsdfs'] = bsdfs
    if (shapes):
        res_dict['shapes'] = shapes
    if (entities):
        res_dict['entities'] = entities
    if (lights):
        res_dict['lights'] = lights

    json_data = json.dumps(res_dict, indent=4)

    outFilePath = parser.parse_args().OutputFile
    if outFilePath is None:
        outFilePath = os.path.splitext(xmlFilePath)[0]+'.json'
    with open(outFilePath, "w") as json_file:
        json_file.write(json_data)

    json_file.close()
