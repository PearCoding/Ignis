class Primitive:
    def __init__(self, modifier = None, type_ = None, identifier = None):
        self.modifier = modifier
        self.type = type_
        self.identifier = identifier
        self.noOfStrings = -1
        self.stringList = []
        self.noOfInts = -1
        self.intList = []
        self.noOfFloats = -1
        self.floatList = []
        self.state = 'initialized'

    def __str__(self):
        #To print readable object details
        return f"Primitive(Type: {self.type}, Identifier:{self.identifier})"
    
    def __repr__(self):
        #To print detailed object details
        return f"Primitive(Modifier: {self.modifier}, Type: {self.type}, Identifier:{self.identifier})"
