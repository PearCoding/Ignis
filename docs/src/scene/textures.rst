Textures
========

.. WARNING:: The texture system in Ignis is experimental and will get major reworks in the future

A texture is specified in the :monosp:`textures` block with a :monosp:`name` and a :monosp:`type`.
The type has to be one of the textures listed at this section below.

.. code-block:: javascript
    
    {
        // ...
        "textures": [
            // ...
            {"name":"NAME", "type":"TYPE", /* DEPENDS ON TYPE */},
            // ...
        ]
        // ...
    }

Image texture (:monosp:`image`)
---------------------------------------------

.. objectparameters::

 * - filename
   - |string|
   - *None*
   - Path to a valid image file.

 * - filter_type
   - |string|
   - "bilinear"
   - The filter type to be used. Has to be one of the following: ["bilinear", "nearest"].

 * - wrap_mode
   - |string|
   - "repeat"
   - The wrap method to be used. Has to be one of the following: ["repeat", "mirror", "clamp"].

Checkerboard (:monosp:`checkerboard`)
---------------------------------------------

.. objectparameters::

 * - color0, color1
   - |color|
   - (0,0,0), (1,1,1)
   - The colors to use in the checkerboard.

 * - scale_x, scale_y
   - |number|
   - 1, 1
   - Numbers of grids in a normalized frame [0,0]x[1,1].
