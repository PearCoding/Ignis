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



def parseRad(radFile):
    #parses the provided .rad file, and adds the retrieved primitives and commands to a list.
    logging.debug("parseRad() called.")
    logging.debug("Walking through .rad, and saving the geometry declarations on a stack.")

    # primitives = []
    declarations = []       #declarations refer to both primitives and commands

    line = radFile.readline()
    while line:
        line = line.strip()
        logging.debug("New line: " + repr(line))

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
            p = Primitive(elements[0], elements[1], elements[2])
            identifierList.append(p.identifier)
            logging.debug(repr(p))
            line = radFile.readline()
            continue
                
        #read the string arguments once modifier has been defined
        if p.state == 'initialized':
            elements = shlex.split(line)    #this preserves quoted substrings
            # logging.debug("elements: ",elements)
            if p.noOfStrings == -1:
                p.noOfStrings = int(elements.pop(0))
                logging.debug("noOfStrings: " + str(p.noOfStrings))

            p.stringList.extend(elements)
            logging.debug("strings p: " + ' '.join(p.stringList))

            if(p.noOfStrings == len(p.stringList)):
                p.state = 'stringsRead'
                line = radFile.readline()
                continue
            if(p.noOfStrings < len(p.stringList)):
                logging.error(f"More string arguments provided for '{p.identifier}' primitive than expected. ABORTING.")
                exit()

        #read the int arguments
        if p.state == 'stringsRead':
            elements = line.split()
            elements = list(map(int, elements))
            # logging.debug("elements: ",elements)
            if p.noOfInts == -1:
                p.noOfInts = elements.pop(0)
                logging.debug("noOfInts: " + str(p.noOfInts))

            p.intList.extend(elements)
            logging.debug("ints p: " + ' '.join(p.intList))

            if(p.noOfInts == len(p.intList)):
                p.state = 'intsRead'
                line = radFile.readline()
                continue
            if(p.noOfInts < len(p.intList)):
                logging.error(f"More int arguments provided for '{p.identifier}' primitive than expected. ABORTING.")
                exit()

        #read the float arguments
        if p.state == 'intsRead':
            elements = line.split()
            elements = list(map(float, elements))
            # logging.debug("elements: ",elements)
            if p.noOfFloats == -1:
                p.noOfFloats = int(elements.pop(0))
                logging.debug("noOfFloats: " + str(p.noOfFloats))

            p.floatList.extend(elements)
            logging.debug("floats of p: " + ' '.join(map(str,p.floatList)))

            if(p.noOfFloats == len(p.floatList)):
                p.state = 'floatsRead'
            if(p.noOfFloats < len(p.floatList)):
                logging.error(f"More float arguments provided for '{p.identifier}' primitive than expected. ABORTING.")
                exit()

        if p.state == 'floatsRead':
            declarations.append(copy.deepcopy(p))
            del p

        line = radFile.readline()

    logging.info(f"number of declarations: {len(declarations)}")
    # for d in declarations:
    #     logging.info(d)
        # logging.debug(repr(p))

    return declarations
