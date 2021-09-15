import shlex
import logging
import config
import copy
from primitive import Primitive
from command import Command


identifierList = []


def isComment(line):
    return line.startswith("#")

def isBlank(line):
    if len(line) == 0:
        return True
    return False

def isCommand(line):
    return line.startswith("!")

def isModifierDef(line):
    elements = shlex.split(line)    #this preserves quoted substrings
    return (elements[0] == 'void') or (elements[0] in identifierList)    



def extractDeclarations(radFile):
    #goes through the provided .rad file, and adds the retrieved primitives and commands to a list.
    logging.debug("extractDeclarations() called.")
    logging.debug("Walking through .rad, and saving the geometry declarations on a stack.")

    declarations = []       #declarations refer to both primitives and commands

    line = radFile.readline()
    while line:
        line = line.strip()
        # logging.debug("New line: " + repr(line))

        if isComment(line):
            line = radFile.readline()
            continue
        if isBlank(line):
            line = radFile.readline()
            continue
        if isCommand(line):
            logging.debug("Its a command def")
            c = Command(line[1:])      #Remove the "!" at the front and store the command.
            declarations.append(c)
            line = radFile.readline()
            continue
        if isModifierDef(line):
            logging.debug("Its a modifier def")
            elements = shlex.split(line)    #this preserves quoted substrings

            #create a Primitive object by passing modifier, type and identifier parameters
            mod = elements.pop(0)
            typ = elements.pop(0)
            ide = elements.pop(0)
            p = Primitive(mod, typ, ide)
            identifierList.append(p.identifier)
 
        #read the string arguments once modifier has been defined
        if p.state == 'initialized':
            if len(elements) == 0:
                line = radFile.readline()
                line = line.strip()
                # logging.debug("New line: " + repr(line))
                elements = shlex.split(line)    #this preserves quoted substrings

            if p.noOfStrings == -1:
                p.noOfStrings = int(elements.pop(0))

            readIndices = min(p.noOfStrings, len(elements))         #two extreme cases need to be handled: string list spanning multiple lines or entire declaration (str, int, float) in the same line.
            p.stringList.extend(elements[0:readIndices])
            del elements[0:readIndices]

            if(p.noOfStrings == len(p.stringList)):
                p.state = 'stringsRead'

        #read the int arguments
        if p.state == 'stringsRead':
            if len(elements) == 0:
                line = line.strip()
                # logging.debug("New line: " + repr(line))
                line = radFile.readline()
                elements = line.split()

            if p.noOfInts == -1:
                p.noOfInts = int(elements.pop(0))

            readIndices = min(p.noOfInts, len(elements)) 
            intElements = list(map(int, elements[0:readIndices]))
            p.intList.extend(intElements)
            del elements[0:readIndices]

            if(p.noOfInts == len(p.intList)):
                p.state = 'intsRead'

        #read the float arguments
        if p.state == 'intsRead':
            if len(elements) == 0:
                line = line.strip()
                # logging.debug("New line: " + repr(line))
                line = radFile.readline()
                elements = line.split()

            if p.noOfFloats == -1:
                p.noOfFloats = int(elements.pop(0))

            readIndices = min(p.noOfFloats, len(elements)) 
            floatElements = list(map(float, elements[0:readIndices]))
            p.floatList.extend(floatElements)
            del elements[0:readIndices]

            if(p.noOfFloats == len(p.floatList)):
                p.state = 'floatsRead'

        if p.state == 'floatsRead':
            declarations.append(copy.deepcopy(p))
            del p
            line = radFile.readline()

    logging.info(f"number of declarations: {len(declarations)}")
    for d in declarations:
        # logging.info(d)
        logging.debug(repr(d))

    return declarations
