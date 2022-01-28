File Format
===========

Ignis uses a flat hierachy of entities specified in a single JSON file.
Future extensions might introduce linked data and transform hierachies to allow more complex scenes.
The JSON format is not strict and allows comments in the style of // and `/* */`.
Also trailing commas are allowed to ease automatic generation of scene files.

Below is a complete scene usable with Ignis as an example.

.. code-block:: javascript
    :caption: diamond_scene.json

    {
        "technique": {
            "type": "path",
            "max_depth": 64
        },
        "camera": {
            "type": "perspective",
            "fov": 39.597755,
            "near_clip": 0.1,
            "far_clip": 100,
            "transform": [ -1,0,0,0, 0,1,0,0, 0,0,-1,3.849529, 0,0,0,1 ]
        },
        "film": {
            "size": [1000, 1000]
        },
        "externals": [],
        "textures": [],
        "bsdfs": [
            {"type":"diffuse", "name": "mat-Light", "reflectance":[0,0,0]},
            {"type":"diffuse", "name": "mat-GrayWall", "reflectance":[1,1,1]},
            {"type":"diffuse", "name": "mat-ColoredWall", "reflectance":[0.106039, 0.195687, 0.800000]},
            {"type":"roughdielectric", "name": "mat-Diamond", "alpha": 0.0025, "int_ior": 2.3, "specular_transmittance":[1,1,1]}
        ],
        "shapes": [
            {"type":"rectangle", "name":"AreaLight", "flip_normals":true, "transform": [0, 0.084366, -0.053688, -0.7, 0, 0.053688, 0.084366, 0.1, 0.1, 0, 0, 0, 0, 0, 0, 1]},
            {"type":"ply", "name":"Bottom", "filename":"meshes/Bottom.ply"},
            {"type":"ply", "name":"Top", "filename":"meshes/Top.ply"},
            {"type":"ply", "name":"Left", "filename":"meshes/Left.ply"},
            {"type":"ply", "name":"Right", "filename":"meshes/Right.ply"},
            {"type":"ply", "name":"Back", "filename":"meshes/Back.ply"},
            {"type":"ply", "name":"dobj", "filename":"meshes/dobj.ply"}
        ],
        "entities": [
            {"name":"AreaLight", "shape":"AreaLight", "bsdf":"mat-Light"},
            {"name":"Bottom","shape":"Bottom", "bsdf":"mat-GrayWall"},
            {"name":"Top","shape":"Top", "bsdf":"mat-GrayWall"},
            {"name":"Left","shape":"Left", "bsdf":"mat-ColoredWall"},
            {"name":"Right","shape":"Right", "bsdf":"mat-ColoredWall"},
            {"name":"Back","shape":"Back", "bsdf":"mat-GrayWall"},
            {"name":"dobj1","shape":"dobj", "bsdf":"mat-Diamond"},
            {"name":"dobj2","shape":"dobj", "bsdf":"mat-Diamond", "transform":{"translate":[0.6,0,0]}},
            {"name":"dobj3","shape":"dobj", "bsdf":"mat-Diamond", "transform":{"translate":[-0.6,0,0]}}
        ],
        "lights": [
            {"type":"area", "name":"AreaLight", "entity":"AreaLight", "radiance":[62,62,62]}
        ]
    }

Transformation
--------------

A transformation can be specified for many objects in the scene. 

.. code-block:: javascript
    
    {
        // ...
        "transform": TRANSFORM,
        //
    }

The transformation can be specified as a 4x4 matrix:

.. code-block:: javascript
    
    {
        // ...
        "transform": [ m00, m01, m02, /* ... */, m31, m32, m33],
        //
    }

as a 3x4 matrix with last row set to (0, 0, 0, 1) and a 3x3 matrix with also sets the last column (the translation) to (0, 0, 0).

Another way to describe transformation is by using simple operators. The operators will be applied at the order of their appearance.

.. code-block:: javascript
    
    {
        // ...
        "transform": { "translate": [X,Y,Z], "rotate": [RX,RY,RZ], "scale":[SX,SY,SZ], /* AND MORE */  },
        //
    }

The available operators are listed below.

Translate (:monosp:`translate`)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Specified with an array of three numbers.

.. code-block:: javascript
    
    {
        // ...
        "transform": { "translate": [X,Y,Z] },
        //
    }

Rotate (:monosp:`rotate`)
^^^^^^^^^^^^^^^^^^^^^^^^^

Specified with an array of three numbers given in degrees rotating around the respective euler axis.

.. code-block:: javascript
    
    {
        // ...
        "transform": { "rotate": [RX,RY,RZ] },
        //
    }

Rotate using a quaternion (:monosp:`qrotate`)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Specified with an array of four numbers representing a quaterion given as [w, x, y, z].
Have a look at your favourite math book to understand what that means.

.. code-block:: javascript
    
    {
        // ...
        "transform": { "qrotate": [RW,RX,RY,RZ] },
        //
    }

Scale (:monosp:`scale`)
^^^^^^^^^^^^^^^^^^^^^^^

Specified with an array of three numbers scaling the respective euler axis or as a single number scaling uniformly.

.. code-block:: javascript
    
    {
        // ...
        "transform": { "scale": [SX,SY,SZ], "scale": S },
        //
    }

Lookat (:monosp:`lookat`)
^^^^^^^^^^^^^^^^^^^^^^^^^

Specified with multiple parameters.
You are able to specify a :monosp:`direction` instead of a explicit :monosp:`target` location.

.. code-block:: javascript
    
    {
        // ...
        "transform": { "lookat": { "origin": [OX,OY,OZ], "target": [TX,TY,TZ], "up": [UX,UY,UZ], /* or */ "direction": [DX,DY,DZ] } },
        //
    }


Matrix (:monosp:`matrix`)
^^^^^^^^^^^^^^^^^^^^^^^^^

Explicitly specify a matrix of the size 4x4, 3x4 or 3x3 to be applied to the full transformation.

.. code-block:: javascript
    
    {
        // ...
        "transform": { "matrix": [m00,/*...*/,m33] },
        //
    }