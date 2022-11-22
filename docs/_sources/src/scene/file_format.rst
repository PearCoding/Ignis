File Format
===========

Ignis uses a flat hierarchy of entities specified in a single JSON file.
You can include other supported formats within the :monosp:`externals` block.
The JSON format is not strict and allows comments in the style of ``//`` and ``/* */``.
Trailing commas are allowed to ease automatic generation of scene files as well.

.. literalinclude:: ../../../scenes/diamond_scene.json
    :language: javascript
    :caption: The included ``diamond_scene.json`` is a good example for an Ignis scene. 

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

Another way to describe transformation is by using simple operators. The operators will be applied at the order of their appearance from left to right inside the array.
The last entry will be applied first to the incoming point.

.. code-block:: javascript
    
    {
        // ...
        "transform": [{ "translate": [X,Y,Z] }, { "rotate": [RX,RY,RZ] }, { "scale":[SX,SY,SZ] }, /* AND MORE */  ],
        //
    }

The available operators are listed below.

Translate (:monosp:`translate`)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Specified with an array of three numbers.

.. code-block:: javascript
    
    {
        // ...
        "transform": [{ "translate": [X,Y,Z] }],
        //
    }

Rotate (:monosp:`rotate`)
^^^^^^^^^^^^^^^^^^^^^^^^^

Specified with an array of three numbers given in degrees rotating around the respective euler axis.

.. code-block:: javascript
    
    {
        // ...
        "transform": [{ "rotate": [RX,RY,RZ] }],
        //
    }

Each rotation around an axis is applied individually. The rotation above is equal to the following one:

.. code-block:: javascript
    
    {
        // ...
        "transform": [{ "rotate": [RX,0,0] }, { "rotate": [0,RY,0] }, { "rotate": [0,0,RZ] }],
        //
    }

Rotate using a quaternion (:monosp:`qrotate`)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Specified with an array of four numbers representing a quaternion given as [w, x, y, z].
Have a look at your favorite math book to understand what that means.

.. code-block:: javascript
    
    {
        // ...
        "transform": [{ "qrotate": [RW,RX,RY,RZ] }],
        //
    }

Scale (:monosp:`scale`)
^^^^^^^^^^^^^^^^^^^^^^^

Specified with an array of three numbers scaling the respective euler axis or as a single number scaling uniformly.

.. code-block:: javascript
    
    {
        // ...
        "transform": [{ "scale": [SX,SY,SZ] }, { "scale": S }],
        //
    }

Lookat (:monosp:`lookat`)
^^^^^^^^^^^^^^^^^^^^^^^^^

Specified with multiple parameters.
You are able to specify a :monosp:`direction` instead of an explicit :monosp:`target` location.

.. code-block:: javascript
    
    {
        // ...
        "transform": [{ "lookat": { "origin": [OX,OY,OZ], "up": [UX,UY,UZ], "target": [TX,TY,TZ], /* or */ "direction": [DX,DY,DZ] } }],
        //
    }


Matrix (:monosp:`matrix`)
^^^^^^^^^^^^^^^^^^^^^^^^^

Specifies a matrix of the size 4x4, 3x4 or 3x3 to be applied to the full transformation.

.. code-block:: javascript
    
    {
        // ...
        "transform": [{ "matrix": [m00, m01, m02, /*...*/, m31, m32, m33] }],
        //
    }