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

Brick (:monosp:`brick`)
-------------------------------------

.. objectparameters::

 * - color0, color1
   - |color|
   - (0,0,0), (1,1,1)
   - The colors to used for the brick. color0 is the mortar, color1 is the actual brick.
 * - scale_x, scale_y
   - |number|
   - 6, 3
   - Numbers of grids in a normalized frame [0,0]x[1,1].
 * - gap_x, gap_y
   - |number|
   - 0.05, 0.1
   - Normalized gap size.
 * - transform
   - |transform|
   - Identity
   - Optional 2d transformation applied to texture coordinates.

.. subfigstart::

.. figure::  images/tex_brick.jpg
  :width: 90%
  :align: center
  
  Brick texture

.. subfigend::
  :width: 0.6
  :label: fig-brick

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
  
  Noise texture, a slight color noise is visible

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

Expression (:monosp:`expr`)
------------------------------------------

A custom :ref:`PExpr <PExpr>` expression with optional parameters.

Available are color (vec4), vector (vec3), number (num) and bool variables. 
The parameters used inside the expression have to be prefixed with `color\_`, `vec\_`, `num\_` and `bool\_` respectively.
E.g., `color_tint` will be called `tint` inside the expression.

.. objectparameters::

 * - expr
   - |string|
   - None
   - A :ref:`PExpr <PExpr>` based expression

.. subfigstart::

.. figure::  images/tex_expr.jpg
  :width: 90%
  :align: center
  
  Custom texture generated by an expression

.. subfigend::
  :width: 0.6
  :label: fig-expr

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

.. subfigstart::

.. figure::  images/tex_transform.jpg
  :width: 90%
  :align: center
  
  Transformed texture as a texture

.. subfigend::
  :width: 0.6
  :label: fig-expr
