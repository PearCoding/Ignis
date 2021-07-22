import copy
import numpy as np
import math

class Transformation:
    def __init__(self, is_array = False, num_instances = 1):
        # self.tx = 0.0
        # self.ty = 0.0
        # self.tz = 0.0
        # self.rx = 0.0
        # self.ry = 0.0
        # self.rz = 0.0
        # self.sx = 1.0
        # self.sy = 1.0
        # self.sz = 1.0
        self.transformation = np.eye(4)
        #variables relevant to xform
        self.isArray = is_array
        self.num_instances = num_instances

    def translate(self, tx_, ty_, tz_):
        # self.tx += tx_    
        # self.ty += ty_
        # self.tz += tz_   
        translation = np.array([[1, 0, 0, tx_], 
                                [0, 1, 0, ty_],
                                [0, 0, 1, tz_],
                                [0, 0, 0, 1]])
        self.transformation = np.matmul(translation, self.transformation)

    # def rotate(self, rx_, ry_, rz_):
    #     self.rx += rx_   
    #     self.ry += ry_  
    #     self.rz += rz_    

    def rotate_x(self, rx):
        c = math.cos(math.radians(rx))
        s = math.sin(math.radians(rx))
        rotation = np.array([[1, 0, 0, 0], 
                             [0, c, s, 0],
                             [0, -s, c, 0],
                             [0, 0, 0, 1]])
        self.transformation = np.matmul(rotation, self.transformation)

    def rotate_y(self, ry):
        c = math.cos(math.radians(ry))
        s = math.sin(math.radians(ry))
        rotation = np.array([[c, 0, -s, 0], 
                             [0, 1, 0, 0],
                             [s, 0, c, 0],
                             [0, 0, 0, 1]])
        self.transformation = np.matmul(rotation, self.transformation)

    def rotate_z(self, rz):
        c = math.cos(math.radians(rz))
        s = math.sin(math.radians(rz))
        rotation = np.array([[c, -s, 0, 0], 
                             [s, c, 0, 0],
                             [0, 0, 1, 0],
                             [0, 0, 0, 1]])
        self.transformation = np.matmul(rotation, self.transformation)

    def scale(self, s):
        # self.sx *= s   
        # self.sy *= s  
        # self.sz *= s   
        scaling = np.array([[s, 0, 0, 0], 
                            [0, s, 0, 0],
                            [0, 0, s, 0],
                            [0, 0, 0, 1]])
        self.transformation = np.matmul(scaling, self.transformation)

    def mirror_x(self):
        # self.sx *= -1 
        self.transformation[0,0] *= -1

    def mirror_y(self):
        # self.sy *= -1 
        self.transformation[1,1] *= -1

    def mirror_z(self):
        # self.sz *= -1 
        self.transformation[2,2] *= -1

    def transform(self, tform):
        # self.tx += tform.tx    
        # self.ty += tform.ty
        # self.tz += tform.tz
        # self.rx += tform.rx    
        # self.ry += tform.ry
        # self.rz += tform.rz
        # self.sx *= tform.sx    
        # self.sy *= tform.sy
        # self.sz *= tform.sz
        self.transformation = np.matmul(tform.transformation, self.transformation)


    def expandToList(self):
        #take a 'transformation array' (specified by a '-a' flag) and expand it to a separate transformations
        #The first object will have zero applications of the transform.
        
        xform_array = []
        base_xform = Transformation()
        
        for i in range(self.num_instances):
            xform_array.append(copy.deepcopy(base_xform))
            base_xform.transform(self)

        return xform_array
    
    def __str__(self):
        #To print readable object details
        return f"Xform(isArray: {self.isArray}, num_instances:{self.num_instances})"
    
    def __repr__(self):
        #To print detailed object details
        return (
            f"Xform(isArray: {self.isArray}, num_instances:{self.num_instances},\n"
            # f"tx: {self.tx}, ty:{self.ty}, tz:{self.tz},\n"
            # f"rx: {self.rx}, ry:{self.ry}, rz:{self.rz},\n"
            # f"sx: {self.sx}, sy:{self.sy}, sz:{self.sz})\n"
            "Transformation:\n"
            f"{self.transformation[0][0]} {self.transformation[0][1]} {self.transformation[0][2]} {self.transformation[0][3]}\n"
            f"{self.transformation[1][0]} {self.transformation[1][1]} {self.transformation[1][2]} {self.transformation[1][3]}\n"
            f"{self.transformation[2][0]} {self.transformation[2][1]} {self.transformation[2][2]} {self.transformation[2][3]}\n"
            f"{self.transformation[3][0]} {self.transformation[3][1]} {self.transformation[3][2]} {self.transformation[3][3]}\n"
        )

