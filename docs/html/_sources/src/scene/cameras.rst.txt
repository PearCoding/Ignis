Cameras
=======

Ignis contains several useful camera models. Only one camera can be specified per scene.
The camera is specified in the :monosp:`camera` block with a :monosp:`type` listed in this section below.
The actual image size is specified in the :monosp:`film` block.

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
------------------------------------------

.. objectparameters::

 * - fov
   - |number|
   - 60
   - Field of view given in degrees.

 * - near_clip, far_clip
   - |number|
   - 0, inf
   - Near and far clip of the camera.

 * - focal_length
   - |number|
   - 1
   - Focal length given in scene units. Only useful if aperture_radius != 0.

 * - aperture_radius
   - |number|
   - 0
   - Aperture radius in scene units. 0 disables depth of field.
   

Orthogonal Camera (:monosp:`orthogonal`)
----------------------------------------

.. objectparameters::

 * - near_clip, far_clip
   - |number|
   - 0, inf
   - Near and far clip of the camera.

 * - scale
   - |number|
   - 1
   - Scale factor given in scene units.


Fisheye Camera (:monosp:`fisheye`, :monosp:`fishlens`)
------------------------------------------------------

.. objectparameters::

 * - near_clip, far_clip
   - |number|
   - 0, inf
   - Near and far clip of the camera.

 * - mode
   - |string|
   - "circular"
   - Clipping mode. Must be one of "circular", "cropped" or "full".