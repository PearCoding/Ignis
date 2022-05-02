Textures
========

All number and color parameters can be connected to a shading network or texture via :ref:`PExpr <PExpr>` to build an actual tree. Cycles are prohibited.

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
-------------------------------

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
 * - transform
   - |transform|
   - Identity
   - Optional 2d transformation applied to texture coordinates.

.. subfigstart::

.. figure::  images/tex_image.jpg
  :width: 90%
  :align: center
  
  Image texture

.. subfigend::
  :width: 0.6
  :label: fig-image

Checkerboard (:monosp:`checkerboard`)
-------------------------------------

.. objectparameters::

 * - color0, color1
   - |color|
   - (0,0,0), (1,1,1)
   - The colors to use in the checkerboard.
 * - scale_x, scale_y
   - |number|
   - 2, 2
   - Numbers of grids in a normalized frame [0,0]x[1,1].
 * - transform
   - |transform|
   - Identity
   - Optional 2d transformation applied to texture coordinates.

.. subfigstart::

.. figure::  images/tex_checkerboard.jpg
  :width: 90%
  :align: center
  
  Checkerboard texture

.. subfigend::
  :width: 0.6
  :label: fig-checkerboard

Noise (:monosp:`noise`)
-----------------------

.. objectparameters::

 * - color
   - |color|
   - (1,1,1)
   - Tint
 * - colored
   - |bool|
   - false
   - True will generate a colored texture, instead of a grayscale one.

.. subfigstart::

.. figure::  images/tex_noise.jpg
  :width: 90%
  :align: center
  
  Noise texture

.. subfigend::
  :width: 0.6
  :label: fig-noise

PNoise (:monosp:`pnoise`)
-------------------------

Original noise used in legacy perlin implementation.

.. objectparameters::

 * - color
   - |color|
   - (1,1,1)
   - Tint
 * - colored
   - |bool|
   - false
   - True will generate a colored texture, instead of a grayscale one.
 * - scale_x, scale_y
   - |number|
   - 20, 20
   - Numbers of grids used for noise in a normalized frame [0,0]x[1,1].
 * - transform
   - |transform|
   - Identity
   - Optional 2d transformation applied to texture coordinates.

.. subfigstart::

.. figure::  images/tex_pnoise.jpg
  :width: 90%
  :align: center
  
  PNoise texture

.. subfigend::
  :width: 0.6
  :label: fig-pnoise

Cell Noise (:monosp:`cellnoise`)
--------------------------------

.. objectparameters::

 * - color
   - |color|
   - (1,1,1)
   - Tint
 * - colored
   - |bool|
   - false
   - True will generate a colored texture, instead of a grayscale one.
 * - scale_x, scale_y
   - |number|
   - 20, 20
   - Numbers of grids used for noise in a normalized frame [0,0]x[1,1].
 * - transform
   - |transform|
   - Identity
   - Optional 2d transformation applied to texture coordinates.

.. subfigstart::

.. figure::  images/tex_cellnoise.jpg
  :width: 90%
  :align: center
  
  Cell noise texture

.. subfigend::
  :width: 0.6
  :label: fig-cellnoise

Perlin Noise (:monosp:`perlin`)
-------------------------------

.. objectparameters::

 * - color
   - |color|
   - (1,1,1)
   - Tint
 * - colored
   - |bool|
   - false
   - True will generate a colored texture, instead of a grayscale one.
 * - scale_x, scale_y
   - |number|
   - 20, 20
   - Numbers of grids used for noise in a normalized frame [0,0]x[1,1].
 * - transform
   - |transform|
   - Identity
   - Optional 2d transformation applied to texture coordinates.

.. subfigstart::

.. figure::  images/tex_perlin.jpg
  :width: 90%
  :align: center
  
  Perlin noise texture

.. subfigend::
  :width: 0.6
  :label: fig-perlin

Voronoi Noise (:monosp:`voronoi`)
---------------------------------

.. objectparameters::

 * - color
   - |color|
   - (1,1,1)
   - Tint
 * - colored
   - |bool|
   - false
   - True will generate a colored texture, instead of a grayscale one.
 * - scale_x, scale_y
   - |number|
   - 20, 20
   - Numbers of grids used for noise in a normalized frame [0,0]x[1,1].
 * - transform
   - |transform|
   - Identity
   - Optional 2d transformation applied to texture coordinates.

.. subfigstart::

.. figure::  images/tex_voronoi.jpg
  :width: 90%
  :align: center
  
  Voronoi texture

.. subfigend::
  :width: 0.6
  :label: fig-voronoi

Fractional Brownian Motion (:monosp:`fbm`)
------------------------------------------

.. objectparameters::

 * - color
   - |color|
   - (1,1,1)
   - Tint
 * - colored
   - |bool|
   - false
   - True will generate a colored texture, instead of a grayscale one.
 * - scale_x, scale_y
   - |number|
   - 20, 20
   - Numbers of grids used for noise in a normalized frame [0,0]x[1,1].
 * - transform
   - |transform|
   - Identity
   - Optional 2d transformation applied to texture coordinates.

.. subfigstart::

.. figure::  images/tex_fbm.jpg
  :width: 90%
  :align: center
  
  FBM texture

.. subfigend::
  :width: 0.6
  :label: fig-fbm

Texture transform (:monosp:`transform`)
---------------------------------------------

.. objectparameters::

 * - texture
   - |color|
   - None
   - The texture the transform is applied to.
 * - transform
   - |transform|
   - Identity
   - 2d transformation applied to texture coordinates.
