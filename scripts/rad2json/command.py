class Command:
    def __init__(self, c):
        self.command = c

    def __str__(self):
        #To print readable object details
        return f"Command: {self.command}"
    
    def __repr__(self):
        #To print detailed object details
        return f"Command: {self.command}"
