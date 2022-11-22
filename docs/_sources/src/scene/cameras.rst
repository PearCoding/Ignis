Cameras
=======

Ignis contains several useful camera models. Only one camera can be specified per scene.
The camera is specified in the :monosp:`camera` block with a :monosp:`type` listed in this section below.

The actual image (or viewport) size is specified in the :monosp:`film` block with an optional sample strategy in :monosp:`sampler`.
The sample strategy has to be one of "independent" (default), "mjitt" or "halton".

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
            "size": [SX, SZ],
            "sampler": "independent"
        }
        // ...
    }

Perspective Camera (:monosp:`perspective`)
------------------------------------------

.. objectparameters::

 * - fov
   - |number|
   - 60
   - Horizontal field of view given in degrees. Can be also given via the name :monosp:`hfov`.
 * - vfov
   - |number|
   - *None*
   - Vertical field of view given in degrees. Can not be defined together with :monosp:`fov` or :monosp:`hfov`.
 * - aspect_ratio
   - |number|
   - *None*
   - Aspect ratio (width over height). If not specified the current viewport will be used.
 * - near_clip, far_clip
   - |number|
   - 0, inf
   - Near and far clip of the camera.
 * - focal_length
   - |number|
   - 1
   - Focal length given in scene units. Only used if aperture_radius != 0.
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
   - Horizontal scale factor given in scene units.
 * - aspect_ratio
   - |number|
   - *None*
   - Aspect ratio (width over height). If not specified the current viewport will be used.


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