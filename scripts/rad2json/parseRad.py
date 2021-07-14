import shlex
import logging
import config
from primitive import Primitive

#TODO
#logging across modules
#while accessing variables from config, do I need to use global keyword?


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
    #parses the provided .rad file, and adds the retrieved primitives to a list.
    logging.debug("parseRad() called.")

    primitives = []

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
            config.commands.append(line[1:])      #Remove the "!" at the front and store the command.
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
            if(p.noOfStrings < len(p.stringList)):
                logging.error("More string arguments provided than expected. Aborting.")
                break

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
            if(p.noOfInts < len(p.intList)):
                logging.error("More int arguments provided than expected. Aborting.")
                break

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
                line = radFile.readline()
            if(p.noOfFloats < len(p.floatList)):
                logging.error("More float arguments provided than expected. Aborting.")
                break

        if p.state == 'floatsRead':
            primitives.append(p)
            p.state = "idle"
            #TODO: ideally the object should be deleted here.
            # del p

        line = radFile.readline()

    logging.info(f"number of primitives: {len(primitives)}")
    # for p in primitives:
    #     logging.info(p)
        # logging.debug(repr(p))

    return primitives
