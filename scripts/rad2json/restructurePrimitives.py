import json
import os
import shlex
import logging
import config

#variables relevant for reading
shapeList = ["sphere", "polygon", "ring", "cylinder", "cone"]
lightDict = {}
nameCount = 0
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
        
        transform = {"position": primitive.floatList[0:3]}
        entity["transform"] = transform

    if (p_type == "ring"):
        shape["type"] = "disk"
        if(primitive.floatList[6] != 0):
            logging.warning("Inner radius of the disk not equal to 0. Converting ring into disk.")
        shape["origin"] = [0, 0, 0]
        shape["radius"] = primitive.floatList[7]
        shape["normal"] = primitive.floatList[3:6]

        transform = {"position": primitive.floatList[0:3]}
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

        transform = {"position": [0,0,0]}
        entity["transform"] = transform

    if (p_type == "cylinder"):
        shape["type"] = "cylinder"
        shape["baseCenter"] = primitive.floatList[0:3]
        shape["tipCenter"] = primitive.floatList[3:6]
        shape["radius"] = primitive.floatList[6]
        
        #not explicitly defining transform as the base and tip center already are defined in global space here
        # transform = {"position": primitive.floatList[0:3]}
        # entity["transform"] = transform
            
    if (p_type == "cone"):
        shape["type"] = "cone"
        if(min(primitive.floatList[6], primitive.floatList[7]) != 0):
            logging.warning("Smaller radius of the cone not equal to 0. Converting frustum into cone.")
        shape["baseCenter"] = primitive.floatList[0:3]
        shape["tipCenter"] = primitive.floatList[3:6]
        shape["radius"] = max(primitive.floatList[6], primitive.floatList[7])
        
        # transform = {"position": primitive.floatList[0:3]}
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


def handleMeshFile(fileName):
    #TODO: handle "transform" passed with the file
    
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

    shape = {}
    entity = {}

    global nameCount
    name = "mesh" + str(nameCount)
    nameCount = nameCount + 1
    shape["type"] = "obj"
    shape["name"] = name
    shape["filename"] = objFilename

    entity["name"] = name
    entity["shape"] = name
    entity["bsdf"] = bsdfs[0]["name"]   

    # transform = {"position": primitive.floatList[0:3]}
    # entity["transform"] = transform 
    
    shapes.append(shape)
    entities.append(entity)
    

def generateBox(material, name, width, height, depth, tform = None):
    shape = {}
    entity = {}

    global nameCount
    name = "box" + str(nameCount)
    nameCount = nameCount + 1
    shape["type"] = "cube"
    shape["name"] = name
    shape["width"] = width
    shape["height"] = height
    shape["depth"] = depth
    shape["origin"] = [0, 0, 0]
    # shape["origin"] = [0.5, 0.5, 0.5]

    entity["name"] = name
    entity["shape"] = name
    entity["bsdf"] = bsdfs[0]["name"]

    #check if a transformation has been specified by a preceeding xform
    if tform:
        transform = {"position": [tform["tx"], tform["ty"], tform["tz"]],
                    "rotation": [tform["rx"], tform["ry"], tform["rz"]],
                    "scale": tform["s"]}     #TODO: handling mirroring operation
        entity["transform"] = transform


    shapes.append(shape)
    entities.append(entity)
    
    
def handleCommand(command):
    elements = shlex.split(command)

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
            tform = xform_tform_list.pop(0)
            
        generateBox(material, name, width, height, depth, tform)

    elif elements[0] == "xform":
        logging.debug("Parsing xform command.")
        elements.pop(0)

        #set defaults
        xform = {}
        xform["tx"] = 0; xform["ty"] = 0; xform["tz"] = 0
        xform["rx"] = 0; xform["ry"] = 0; xform["rz"] = 0
        xform["s"] = 1

        while len(elements) > 1:
            flag = elements.pop(0)
            
            if flag == "-t":
                xform["tx"] = float(elements.pop(0))
                xform["ty"] = float(elements.pop(0))
                xform["tz"] = float(elements.pop(0))
            elif flag == "-rx":
                xform["rx"] = float(elements.pop(0))
            elif flag == "-ry":
                xform["ry"] = float(elements.pop(0))
            elif flag == "-rz":
                xform["rz"] = float(elements.pop(0))
            elif flag == "-s":
                xform["s"] = float(elements.pop(0))
            elif flag == "-mx":
                xform["mx"] = float(elements.pop(0))
            elif flag == "-my":
                xform["my"] = float(elements.pop(0))
            elif flag == "-mz":
                xform["mz"] = float(elements.pop(0))
            elif flag == "-i":
                ...
            else:
                logging.warning(f"Unknown flag {flag} in xform ignored.")
        #if a single element still remains at the end, its the file name. Else, it is to be read from the standard input.
        if len(elements) == 1:
            file = elements.pop(0)
            #TODO: actually process the file
        else:
            xform_tform_list.append(xform)

    else:
        logging.warning(f"Can't parse the command: {command}. Ignoring it.")


def restructurePrimitives(primitives):
    #iterates through a list of primitives and restructures them to Ignis compatible form

    #move this inside processShape() when each material has a different bsdf
    bsdfs.append({"type":"diffuse", "name": "gray", "reflectance":[0.8,0.8,0.8]})

    for p in primitives:
        if p.type == 'light':
            #store the radiance value; to be used later when the corresponding shape gets processed.
            lightDict[p.identifier] = p.floatList
        elif p.type in shapeList:
            processShape(p)
        elif p.type == 'mesh': 
            handleMeshFile(p.stringList[0]) 
        else:
            logging.warning(f"Can't handle primitive of type {p.type}.")

    #Right now all commands are processed togehter at the end. This might be changed later.
    for c in config.commands:
        handleCommand(c)

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
    # position = [0.49, 0, 0]
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
