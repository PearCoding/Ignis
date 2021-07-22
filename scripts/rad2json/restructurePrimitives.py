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


def processShape(primitive):
    
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
        
        transform = {"translate": primitive.floatList[0:3]}
        entity["transform"] = transform

    if (p_type == "ring"):
        shape["type"] = "disk"
        if(primitive.floatList[6] != 0):
            logging.warning("Inner radius of the disk not equal to 0. Converting ring into disk.")
        shape["origin"] = [0, 0, 0]
        shape["radius"] = primitive.floatList[7]
        shape["normal"] = primitive.floatList[3:6]

        transform = {"translate": primitive.floatList[0:3]}
        entity["transform"] = transform

    if (p_type == "polygon"):
        if(primitive.noOfFloats != 12):
            logging.error("Can't process the polygon. " + primitive.noOfFloats + " float arguments provided.")
        #assuming that 12 arguments are used only to define a rectangle, and no irregular shapes:
        shape["type"] = "rectangle"
        shape["p0"] = primitive.floatList[0:3]
        shape["p1"] = primitive.floatList[3:6]
        shape["p2"] = primitive.floatList[6:9]
        shape["p3"] = primitive.floatList[9:12]

        transform = {"translate": [0,0,0]}
        entity["transform"] = transform

    if (p_type == "cylinder"):
        shape["type"] = "cylinder"
        shape["baseCenter"] = primitive.floatList[0:3]
        shape["tipCenter"] = primitive.floatList[3:6]
        shape["radius"] = primitive.floatList[6]
        
        #not explicitly defining transform as the base and tip center already are defined in global space here
        # transform = {"translate": primitive.floatList[0:3]}
        # entity["transform"] = transform
            
    if (p_type == "cone"):
        shape["type"] = "cone"
        if(min(primitive.floatList[6], primitive.floatList[7]) != 0):
            logging.warning("Smaller radius of the cone not equal to 0. Converting frustum into cone.")
        shape["baseCenter"] = primitive.floatList[0:3]
        shape["tipCenter"] = primitive.floatList[3:6]
        shape["radius"] = max(primitive.floatList[6], primitive.floatList[7])
        
        # transform = {"translate": primitive.floatList[0:3]}
        # entity["transform"] = transform

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


def handleMeshFile(fileName, transformations):
    
    #we assume that the rad file specifies the rtm file path relative to itself.
    fileName = os.path.join(config.RAD_FOLDER_PATH, fileName)
    logging.info(f"Radiance mesh file name: {fileName}")

    if (not fileName.endswith('.rtm')):
        logging.warning("The specified mesh file is not a .rtm file. Ignoring the mesh.")
        return
    if (not os.path.isfile(fileName)):
        logging.warning("The specified mesh .rtm file does not exist. Ignoring the mesh.")
        return    
    
    with open(fileName, 'rb') as rtmFile:
        #read the second line
        line = rtmFile.readline()
        line = rtmFile.readline().decode('utf-8')
        objFilenames = [w for w in shlex.split(line) if w.endswith('.obj')]
        
    if len(objFilenames) > 1:
        logging.warning("Multiple obj files mentioned in .rtm, selecting the first one.")
    objFilename = objFilenames[0]

    logging.info("Obj file to be loaded: " + objFilename)

    #Check if the obj file exists
    if (not os.path.isfile(os.path.join(os.path.dirname(fileName), objFilename))):
        logging.warning("The specified mesh .obj file does not exist. Make sure it is present before running the Ignis software.")


    #parse the transformations
    #set defaults
    tform = Transformation()

    while len(transformations) > 0:
        argument = transformations.pop(0)
        if argument == "-t":
            tx = float(transformations.pop(0))
            ty = float(transformations.pop(0))
            tz = float(transformations.pop(0))
            tform.translate(tx, ty, tz)
        elif argument == "-rx":
            rx = float(transformations.pop(0))
            # tform.rotate(rx, 0, 0)
            tform.rotate_x(rx)
        elif argument == "-ry":
            ry = float(transformations.pop(0))
            # tform.rotate(0, ry, 0)
            tform.rotate_y(ry)
        elif argument == "-rz":
            rz = float(transformations.pop(0))
            # tform.rotate(0, 0, rz)
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
            logging.warning(f"Unknown transformation argument '{argument}'' for {fileName} ignored.")

    # transform = {"translate": [tform.tx, tform.ty, tform.tz],
    #             "rotate": [tform.rx, tform.ry, tform.rz],
    #             "scale": [tform.sx, tform.sy, tform.sz]}
    transform = {"matrix": tform.transformation.flatten().tolist()}

    shape = {}
    entity = {}

    global nameCount
    name = "mesh" + str(nameCount)
    nameCount = nameCount + 1
    shape["type"] = "obj"
    shape["name"] = name
    shape["filename"] = objFilename
    shape["transform"] = transform
    shapes.append(shape)


    #check if transformations has been specified by a preceeding xform, create one entity corresponding to each
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
            # transform = {"translate": [tf.tx, tf.ty, tf.tz],
            #             "rotate": [tf.rx, tf.ry, tf.rz],
            #             "scale": [tf.sx, tf.sy, tf.sz]}'
            transform = {"matrix": tf.transformation.flatten().tolist()}
            entity["transform"] = transform
            entities.append(entity)
    else:
        entity = {}
        entity["name"] = name
        entity["shape"] = name
        entity["bsdf"] = bsdfs[0]["name"]
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
            # transform = {"translate": [tf.tx, tf.ty, tf.tz],
            #             "rotate": [tf.rx, tf.ry, tf.rz],
            #             "scale": [tf.sx, tf.sy, tf.sz]}'
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

def handleCommand(command):
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

        generateBox(material, name, width, height, depth, tform)

    elif elements[0] == "xform":
        #NOTE: we keep track of only the last xform, which can be used by a subsequent command

        logging.debug("Parsing xform command.")
        elements.pop(0)     #Removes the "xform" keyword

        #files passed with the command
        xform_files = []

        #variable to store the partial transformations (mainly used to handle the -a flag)
        xform_holder = []

        #set defaults
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
                # xform.rotate(rx, 0, 0)
                xform.rotate_x(rx)
            elif argument == "-ry":
                ry = float(elements.pop(0))
                # xform.rotate(0, ry, 0)
                xform.rotate_y(ry)
            elif argument == "-rz":
                rz = float(elements.pop(0))
                # xform.rotate(0, 0, rz)
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
            elif argument == "-i":          #TODO: not processing -i flag which is not associated with a '-a' flag
                #we ignore the int passed with the argument for now
                _ = elements.pop(0)
                #end the current tform by putting it in a list
                xform_holder.append(xform)
                #create a new tform for subsequent transformations
                xform = Transformation()
            elif argument.endswith(".rad"):
                xform_files.append(argument)
            else:
                logging.warning(f"Unknown argument {argument} in xform ignored.")
        
        #end the current tform by putting it in a list
        xform_holder.append(xform)
        #go through the xforms and generate one tform corresponding to each instance
        final_tforms = combine_tforms(xform_holder)
        
        #process all the files which have been passed, else it will be saved and used later by a command (basically by a standard input).
        if len(xform_files) > 0:
            #TODO: actually process the file
            ...
        else:
            xform_tform_list = final_tforms

    else:
        logging.warning(f"Can't parse the command: {command}. Ignoring it.")


def restructurePrimitives(declarations):
    #iterates through a list of primitives and restructures them to Ignis compatible form
    logging.debug("restructurePrimitives() called.")
    logging.debug("Retrieving geometries from stacks and saving them in json compatible structures.")

    #move this inside processShape() when each material has a different bsdf
    bsdfs.append({"type":"diffuse", "name": "gray", "reflectance":[0.8,0.8,0.8]})

    for d in declarations:
        if isinstance(d, Command): 
            handleCommand(d.command)
        elif isinstance(d, Primitive): 
            if d.type == 'light':
                #store the radiance value; to be used later when the corresponding shape gets processed.
                lightDict[d.identifier] = d.floatList
            elif d.type in shapeList:
                processShape(d)
            elif d.type == 'mesh': 
                handleMeshFile(d.stringList.pop(0), d.stringList) 
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
    
    # uncomment if you need env light too 
    # lights.append({"type":"env", "name":"Light", "radiance":[1,1,1]})

    res_dict = {}
    res_dict['technique'] = technique
    res_dict['camera'] = camera
    res_dict['film'] = film
    res_dict['bsdfs'] = bsdfs
    res_dict['shapes'] = shapes
    res_dict['entities'] = entities
    res_dict['lights'] = lights

    jsonData = json.dumps(res_dict, indent=4)
    
    return jsonData
