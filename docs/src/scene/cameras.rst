Cameras
=======

Ignis contains several useful camera models. Only one camera can be specified per scene.
The camera is specified in the :monosp:`camera` block with a :monosp:`type` listed in this section below.
The actual image size is specified in the :monosp:`film` block.

.. NOTE:: Currently there is no way to specifiy type-specific parameters for camera models and film types.

.. code-block:: javascript
    
    {
        // ...
       "camera": {
            "type": "TYPE",
            "fov": FOV,
            "near_clip": 0.1,
            "far_clip": 100,
            "transform": TRANSFORM
        },
        "film": {
            "size": [SX, SZ]
        }
        // ...
    }

Perspective Camera (:monosp:`perspective`)
---------------------------------------------

// TODO

Orthogonal Camera (:monosp:`orthogonal`)
---------------------------------------------

// TODO

Fisheye Camera (:monosp:`fisheye`)
---------------------------------------------

// TODO