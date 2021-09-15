import parseRad
from primitive import Primitive
from command import Command
import json
import os
import shlex
import logging
import config
import itertools
import copy
from transformation import Transformation
from jsonmerge import merge
import struct

#variables relevant for reading
shapeList = ["sphere", "polygon", "ring", "cylinder", "cone"]
lightDict = {}
nameCount = 0

#list of xforms to applied on the geometry
#number of entries = number of instances to be created
xform_tform_list = []

#variables relevant for writing
technique = {}
camera = {}
film = {}
bsdfs = []
shapes = []
entities = []
lights = []
sampler = {}


def processShape(primitive, global_tform):
    
    #process the geometry
    p_type = primitive.type
    shape = {}
    entity = {}

    global nameCount
    name = p_type + str(nameCount)
    nameCount = nameCount + 1
    shape["name"] = name

    entity["name"] = name
    entity["shape"] = name
    entity["bsdf"] = bsdfs[0]["name"]

    if (p_type == "sphere"):
        shape["type"] = "sphere"
        shape["center"] = [0, 0, 0]
        shape["radius"] = primitive.floatList[3]
        
        tform = Transformation()
        tx = float(primitive.floatList[0])
        ty = float(primitive.floatList[1])
        tz = float(primitive.floatList[2])
        tform.translate(tx, ty, tz)
        transform = {"matrix": tform.transformation.flatten().tolist()}
        shape["transform"] = transform

        transform = {"matrix": global_tform.transformation.flatten().tolist()}
        entity["transform"] = transform

    if (p_type == "ring"):
        shape["type"] = "disk"
        if(primitive.floatList[6] != 0):
            logging.warning("Inner radius of the disk not equal to 0. Converting ring into disk.")
        shape["origin"] = [0, 0, 0]
        shape["radius"] = primitive.floatList[7]
        shape["normal"] = primitive.floatList[3:6]
        
        tform = Transformation()
        tx = float(primitive.floatList[0])
        ty = float(primitive.floatList[1])
        tz = float(primitive.floatList[2])
        tform.translate(tx, ty, tz)
        transform = {"matrix": tform.transformation.flatten().tolist()}
        shape["transform"] = transform

        transform = {"matrix": global_tform.transformation.flatten().tolist()}
        entity["transform"] = transform

    if (p_type == "polygon"):

        N = primitive.noOfFloats
        if(N % 3 != 0):
            logging.error(f"Can't process the polygon with {primitive.noOfFloats} elements (should be a multiple of 3).")
            return
        if(N == 9):
            shape["type"] = "triangle"
            shape["p0"] = primitive.floatList[0:3]
            shape["p1"] = primitive.floatList[3:6]
            shape["p2"] = primitive.floatList[6:9]
        elif(N == 12):
            #assuming that 12 arguments are used only to define a rectangle, and not an irregular shape:
            shape["type"] = "rectangle"
            shape["p0"] = primitive.floatList[0:3]
            shape["p1"] = primitive.floatList[3:6]
            shape["p2"] = primitive.floatList[6:9]
            shape["p3"] = primitive.floatList[9:12]
        else:
            #save the polygon geometry in a .ply file
            plyFileName = f'{name}.ply'         #this file will be created in the same folder as the output JSON file
            shape["type"] = "ply"
            shape["filename"] = plyFileName

            # Write header of .ply file
            fid = open(os.path.join(config.JSON_FILE_FOLDER, plyFileName),'wb')
            fid.write(bytes('ply\n', 'utf-8'))
            fid.write(bytes('format ascii 1.0\n', 'utf-8'))
            fid.write(bytes('comment polygon created by rad2json\n', 'utf-8'))
            fid.write(bytes(f'element vertex {int(N/3)}\n', 'utf-8'))
            fid.write(bytes('property float x\n', 'utf-8'))
            fid.write(bytes('property float y\n', 'utf-8'))
            fid.write(bytes('property float z\n', 'utf-8'))
            fid.write(bytes('element face 1\n', 'utf-8'))  #there is 1 "face" elements in the file
            fid.write(bytes('property list uchar int vertex_indices\n', 'utf-8'))	#"vertex_indices" is a list of ints
            fid.write(bytes('end_header\n', 'utf-8'))
        
            # Write 3D points to .ply file
            for i in range(int(N/3)):
                fid.write(bytes(f'{primitive.floatList[3*i]} {primitive.floatList[3*i+1]} {primitive.floatList[3*i+2]}\n', 'utf-8'))

            fid.write(bytes(f'{int(N/3)}', 'utf-8'))
            for i in range(int(N/3)):
                fid.write(bytes(f' {i}', 'utf-8'))
            fid.write(bytes('\n', 'utf-8'))

            fid.close()   

        transform = {"matrix": global_tform.transformation.flatten().tolist()}
        entity["transform"] = transform

    if (p_type == "cylinder"):
        shape["type"] = "cylinder"
        shape["baseCenter"] = primitive.floatList[0:3]
        shape["tipCenter"] = primitive.floatList[3:6]
        shape["radius"] = primitive.floatList[6]
        
        transform = {"matrix": global_tform.transformation.flatten().tolist()}
        entity["transform"] = transform
            
    if (p_type == "cone"):
        shape["type"] = "cone"
        if(min(primitive.floatList[6], primitive.floatList[7]) != 0):
            logging.warning("Smaller radius of the cone not equal to 0. Converting frustum into cone.")
        shape["baseCenter"] = primitive.floatList[0:3]
        shape["tipCenter"] = primitive.floatList[3:6]
        shape["radius"] = max(primitive.floatList[6], primitive.floatList[7])
        
        transform = {"matrix": global_tform.transformation.flatten().tolist()}
        entity["transform"] = transform

    shapes.append(shape)
    entities.append(entity)
    

    #process other properties
    light = {}
    if (primitive.modifier in lightDict):
        light["type"] = "area"
        light["name"] = "AreaLight" + str(nameCount)
        light["entity"] = entity["name"]
        light["radiance"] = lightDict[primitive.modifier]
        lights.append(light)


def handleMeshFile(rtmFilename, transformations, global_tform):
    
    #we assume that the rad file specifies the rtm file path relative to itself.
    path = os.path.join(*config.path_stack)
    rtmFilename = os.path.join(path, rtmFilename)
    logging.info(f"Radiance mesh file name: {rtmFilename}")

    if (not rtmFilename.endswith('.rtm')):
        logging.warning("The specified mesh file is not a .rtm file. Ignoring the mesh.")
        return
    if (not os.path.isfile(rtmFilename)):
        logging.warning("The specified mesh .rtm file does not exist. Ignoring the mesh.")
        return    
    
    with open(rtmFilename, 'rb') as rtmFile:
        #read the second line
        line = rtmFile.readline()
        line = rtmFile.readline().decode('utf-8')
        objFilenames = [w for w in shlex.split(line) if w.endswith('.obj')]
        
    if len(objFilenames) > 1:
        logging.warning("Multiple obj files mentioned in .rtm, selecting the first one.")
    objFilename = objFilenames[0]

    #we assume that the rtm file specifies the obj file path relative to itself.
    objRelativePath = os.path.join(os.path.dirname(rtmFilename), objFilename)
    logging.info(f"Obj file to be referenced: {objRelativePath}")

    #Check if the obj file exists
    if (not os.path.isfile(objRelativePath)):
        logging.warning(f"The specified {objRelativePath} file does not exist. Make sure it is present before running the Ignis software.")


    #parse the transformations
    tform = Transformation()        #default transformation

    while len(transformations) > 0:
        argument = transformations.pop(0)
        if argument == "-t":
            tx = float(transformations.pop(0))
            ty = float(transformations.pop(0))
            tz = float(transformations.pop(0))
            tform.translate(tx, ty, tz)
        elif argument == "-rx":
            rx = float(transformations.pop(0))
            tform.rotate_x(rx)
        elif argument == "-ry":
            ry = float(transformations.pop(0))
            tform.rotate_y(ry)
        elif argument == "-rz":
            rz = float(transformations.pop(0))
            tform.rotate_z(rz)
        elif argument == "-s":
            s = float(transformations.pop(0))
            tform.scale(s)
        elif argument == "-mx":
            tform.mirror_x()
        elif argument == "-my":
            tform.mirror_y()
        elif argument == "-mz":
            tform.mirror_z()
        else:
            logging.warning(f"Unknown transformation argument '{argument}'' for {rtmFilename} ignored.")

    transform = {"matrix": tform.transformation.flatten().tolist()}

    shape = {}
    entity = {}

    global nameCount
    name = "mesh" + str(nameCount)
    nameCount = nameCount + 1
    shape["type"] = "obj"
    shape["name"] = name
    # shape["filename"] = objFilename
    shape["filename"] =  os.path.relpath(objRelativePath, config.JSON_FILE_FOLDER)
    shape["transform"] = transform
    shapes.append(shape)


    #check if transformations has been specified by a preceeding xform. If yes, create one entity corresponding to each
    #note that the tforms in xform_tform_list already contain global_tform. So, we explicitly apply global_tform only when xform_tform_list is not specified

    #get the transformation from a preceeding xform command.
    global xform_tform_list
    if len(xform_tform_list) > 0:
        xform_tforms = copy.deepcopy(xform_tform_list)
        xform_tform_list.clear()

        logging.debug(f"{len(xform_tforms)} unique tforms provided")
        for count, tf in enumerate(xform_tforms):
            entity = {}
            entity["name"] = name + '_' + str(count)
            entity["shape"] = name
            entity["bsdf"] = bsdfs[0]["name"]
            transform = {"matrix": tf.transformation.flatten().tolist()}
            entity["transform"] = transform
            entities.append(entity)
    else:
        entity = {}
        entity["name"] = name
        entity["shape"] = name
        entity["bsdf"] = bsdfs[0]["name"]
        transform = {"matrix": global_tform.transformation.flatten().tolist()}
        entity["transform"] = transform
        entities.append(entity)

    

def generateBox(material, name, width, height, depth, tform = None):
    shape = {}

    global nameCount
    name = "box" + str(nameCount)
    nameCount = nameCount + 1
    shape["type"] = "cube"
    shape["name"] = name
    shape["width"] = width
    shape["height"] = height
    shape["depth"] = depth
    shape["origin"] = [0, 0, 0]
    shapes.append(shape)

    #check if transformations has been specified by a preceeding xform, create one entity corresponding to each
    if tform:
        logging.debug(f"{len(tform)} unique tforms provided")
        for count, tf in enumerate(tform):
            entity = {}
            entity["name"] = name + '_' + str(count)
            entity["shape"] = name
            entity["bsdf"] = bsdfs[0]["name"]
            transform = {"matrix": tf.transformation.flatten().tolist()}
            entity["transform"] = transform
            entities.append(entity)
    else:
        entity = {}
        entity["name"] = name
        entity["shape"] = name
        entity["bsdf"] = bsdfs[0]["name"]
        entities.append(entity)



def combine_tforms(tform_holder):
    
    expanded_tform_holder = []

    #go through the tforms, and convert them to lists if they define an array of transformations
    for tform in tform_holder:
        # logging.debug(repr(tform))
        if tform.isArray:
            tform_list = tform.expandToList()
            expanded_tform_holder.append(tform_list)
            # logging.debug("expanded tform array: ")
            # logging.debug(tform_list)
        else:
            expanded_tform_holder.append([tform])

    #permute all transformations: will return a list of tuples, and each of those tuples will have all the transformations which will go together
    tform_tuples = list(itertools.product(*expanded_tform_holder))
    # logging.debug("list of tform tuples: ")
    # logging.debug(tform_tuples)

    #combine all transformations in a tuple
    composite_tforms = []
    for tform_tuple in tform_tuples:
        composite_tform = Transformation()
        for tform in tform_tuple:
            composite_tform.transform(tform)
        composite_tforms.append(composite_tform)
    
    # logging.debug("composite tforms: ")
    # logging.debug(composite_tforms)

    return composite_tforms

def handleCommand(command, global_tform, depth):
    elements = shlex.split(command)

    global xform_tform_list

    if elements[0] == "genbox":
        logging.debug("Creating a box from genbox command.")
        material = elements[1]
        name = elements[2]
        width = float(elements[3])
        height = float(elements[4])
        depth = float(elements[5])

        #get the transformation from a preceeding xform command.
        tform = None
        if len(xform_tform_list) > 0:
            tform = copy.deepcopy(xform_tform_list)
            xform_tform_list.clear()
        else:
            #note that the tforms in xform_tform_list already contain global_tform. So, we explicitly apply global_tform only when xform tforms are not used
            tform = [global_tform]          #generateBox() function expects the transformations in a list
            
        generateBox(material, name, width, height, depth, tform)

        return None

    elif elements[0] == "xform":
        #NOTE: if a rad file is not passed with xform, we keep track of the latest xform, which can be used by a subsequent command

        logging.debug("Parsing xform command.")
        elements.pop(0)     #Removes the "xform" keyword

        #files passed with the command
        xform_files = []

        #variable to store the partial transformations (mainly used to handle the -a flag)
        xform_holder = []

        #default transformation
        xform = Transformation()

        while len(elements) > 0:
            argument = elements.pop(0)
            
            if argument == "-t":
                tx = float(elements.pop(0))
                ty = float(elements.pop(0))
                tz = float(elements.pop(0))
                xform.translate(tx, ty, tz)
            elif argument == "-rx":
                rx = float(elements.pop(0))
                xform.rotate_x(rx)
            elif argument == "-ry":
                ry = float(elements.pop(0))
                xform.rotate_y(ry)
            elif argument == "-rz":
                rz = float(elements.pop(0))
                xform.rotate_z(rz)
            elif argument == "-s":
                s = float(elements.pop(0))
                xform.scale(s)
            elif argument == "-mx":
                xform.mirror_x()
            elif argument == "-my":
                xform.mirror_y()
            elif argument == "-mz":
                xform.mirror_z()
            elif argument == "-a":          #we have to estimate the cumulative transformation from this 'a' flag to the next 'a'/'i' flag,
                #end the current tform by putting it in a list
                xform_holder.append(xform)
                #create a new xform and let the subsequent transformations be applied to this
                xform = Transformation(is_array = True, num_instances = int(elements.pop(0)))
            elif argument == "-i":          
                logging.warning("'-i' flag currently works only in association with a preceding '-a' flag.")
                #we ignore the int passed with the argument as it is used only with independent '-i' flag
                _ = elements.pop(0)
                #end the current tform by putting it in a list
                xform_holder.append(xform)
                #create a new tform for subsequent transformations
                xform = Transformation()
            elif argument.endswith(".rad"):
                # filename = os.path.join(config.RAD_FILE_FOLDER, argument)
                filename = argument
                logging.debug(f"{filename} provided with xform")
                xform_files.append(filename)
            else:
                logging.warning(f"Unknown argument {argument} in xform ignored.")

        #end the current tform by putting it in a list
        xform_holder.append(xform)

        #ignore the -a sections of xform when .rad files are specified with it (to ignore complications)
        if len(xform_files) > 0:
            for t in xform_holder:
                if t.isArray is True:
                    logging.warning("Ignoring the -a sections of xform specified for .rad files")
                    t = Transformation()
        
        #go through the xforms and generate one tform corresponding to each instance
        xform_tforms = combine_tforms(xform_holder)

        # logging.debug(f"Intermediate combined xform tforms: {xform_tforms}")
        # logging.debug(f"Global tform: {repr(global_tform)}")

        #to each of the xform tform, apply the global tform
        final_tform_list = []
        for xform_tform in xform_tforms:
            xform_tform.transform(global_tform)
            final_tform_list.append(copy.deepcopy(xform_tform))

        logging.debug(f"Final xform tforms: {final_tform_list}")

        #process all the files which have been passed, else the xform will be saved and used later by a command (basically by a standard input).
        if len(xform_files) > 0:
            radJSONdump = parseRad.parseRad(xform_files, final_tform_list[0], depth+1)       #we allow only a single transformation to be passed here since '-a' flag is ignored
            return radJSONdump
        else:
            xform_tform_list = final_tform_list
            return None

    else:
        logging.warning(f"Can't parse the command: {command}. Ignoring it.")
        return None


def restructureDeclarations(declarations, global_tform, depth):
    #iterates through a list of primitives and commands and restructures them to Ignis compatible form
    logging.debug(f"restructureDeclarations() called with tform: {repr(global_tform)} at depth {depth}")
    logging.debug("Retrieving geometries from stacks and saving them in json compatible structures.")


    if depth == 1:
        bsdfs.append({"type":"diffuse", "name": "gray", "reflectance":[0.8,0.8,0.8]})   #move this inside processShape() when each material has a different bsdf 
        lights.append({"type":"env", "name":"Light", "radiance":[1,1,1]})

    commandJSONdump = None

    for d in declarations:
        if isinstance(d, Command): 
            commandJSONdump_new = handleCommand(d.command, global_tform, depth)
            #commandJSONdump is valid only when there is an xform command with .rad file mentioned in it (which calls parseRad() and returns the rad geometry as json data)
            commandJSONdump = merge(commandJSONdump_new, commandJSONdump)
        elif isinstance(d, Primitive): 
            if d.type == 'light':
                #store the radiance value; to be used later when the corresponding shape gets processed.
                lightDict[d.identifier] = d.floatList
            elif d.type in shapeList:
                processShape(d, global_tform)
            elif d.type == 'mesh': 
                handleMeshFile(d.stringList.pop(0), d.stringList, global_tform) 
            else:
                logging.warning(f"Can't handle primitive of type '{d.type}'.")


    #set the default parameters
    technique["type"] = "path"  #"debug"
    technique["max_depth"] = 1

    # film["type"] = "hdr"
    film["size"] = [1000, 1000]
    # film["spp"] = 4

    camera["type"] = "perspective"
    camera["fov"] = "60"
    camera["near_clip"] = 0.01
    camera["far_clip"] = 100
    # translate = [0.49, 0, 0]
    # up = [0, 0, 1]
    # forward = []
    camera["transform"] = [ 0,0,-1,0.49, 
                            1,0,0,0, 
                            0,1,0,0, 
                            0,0,0,1]
    # rview -vtv -vp 0.49 0 0 -vd -1 0 0 -vu 0 0 1 -vh 80 -vv 80 -vo 0 -va 0 -vs 0 -vl 0
    #https://www.3dgep.com/understanding-the-view-matrix/


    res_dict = {}
    res_dict['technique'] = technique
    res_dict['camera'] = camera
    res_dict['film'] = film
    res_dict['bsdfs'] = bsdfs
    res_dict['lights'] = lights
    res_dict['shapes'] = shapes
    res_dict['entities'] = entities

    jsonData = json.dumps(res_dict, indent=4)
    
    if commandJSONdump is not None:
        jsonData = merge(commandJSONdump, jsonData)

    return jsonData
