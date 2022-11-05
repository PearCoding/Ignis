.. _pexpr:

PExpr
=====

.. NOTE:: The PExpr documentation is not complete. For further details it is recommended to have a look at the scenes distributed together with the framework.

**P**\ ear\ **Expr**\ ession is a single line, math like, functional language.
It is, by design, easy to transpile to other languages like ``artic``, like in the case of the Ignis rendering framework.
It has **no** support for control flow, like ``if`` or ``while``, and statements in general.
For more complex shading networks multiple expressions can be chained with textures and function calls. 

Some examples for the PExpr language:

.. code-block:: maxima

    sin(uv.x) * uv.xyxy + color(0.2, 0.4, 0.1, 0)
    /* or */
    fract(P.x) * perlin(N.yx)


A very similar language is `SeExpr <https://github.com/wdas/SeExpr>`_ by Walt Disney Animation Studios.
SeExpr is more complex and industry proven, however, more difficult to transpile to other languages. Nevertheless, both languages are easy to learn and to tinkle around.

An expression can be defined in many places where a number, vector or color can be given as a parameter to a light, texture, bsdf, medium, etc...

An example for a PExpr used inside a BSDF definition:

.. code-block:: javascript
    
    {
        // ...
        "bsdfs": [
            // ...
            { "name":"NAME", "type":"diffuse", "reflectance":"some_tex.r * perlin(uv.yx * abs(sin(uv.x * 10 * Pi))) * some_other_tex(uv.yx)" }
            // ...
        ]
        // ...
    }

A direct connection to a texture is just a simple use-case of a PExpr expression. Keep in mind that throughout the use of textures and expressions cycles are prohibited at all times.

Types
-----

PExpr is strongly typed. Only a single implicit cast from ``int`` to ``num`` is available.

All available types are:

-   ``bool`` Boolean with ``true`` and ``false`` states.
-   ``int`` Signed integer.
-   ``num`` Floating point number.
-   ``vec2`` Two-dimensional vector.
-   ``vec3`` Three-dimensional vector.
-   ``vec4`` Four-dimensional vector. Is used also as a color.
-   ``str`` String.

Vectorized types like ``vec2``, ``vec3`` and ``vec4`` can be cast or extended to each other by the access/swizzle operation like:

.. code-block:: maxima

    vec3(1,2,3).zyxx /* Return type is vec4 (same as color) with values (3,2,1,1) */
    

Variables
---------

Some special variables given are available as ``vec2``:

-   ``uv`` The current texture coordinates.
-   ``prim_coords`` Primitive specific coordinates if surface, else a zero.

some as ``vec3``:

-   ``uvw`` The current texture coordinates given in as a triplet.
-   ``V`` World ray direction facing from a surface or any other source outwards.
-   ``Rd`` Same as ``V``.
-   ``Ro`` World ray origin.
-   ``P`` World position if bsdf or medium, else zero.
-   ``Np`` Normalized position if bsdf or medium, else a zero.
-   ``N`` World shading normal if bsdf, else a zero.
-   ``Ng`` World geometry normal if bsdf, else a zero.
-   ``Nx`` World shading tangent if bsdf, else a zero.
-   ``Ny`` World shading bitangent if bsdf, else a zero.

few as ``int``:

-   ``entity_id`` Contains the id of the current entity or ``-1`` if not on a surface.
-   ``Ix`` Contains the current pixel x index or ``0`` if not available.
-   ``Iy`` Contains the current pixel y index or ``0`` if not available.

and a few as ``bool``:

-   ``frontside`` ``true`` if normal and ray orientation is front facing. This will be always ``true`` if the point is not lying on a surface.

.. NOTE:: More special variables might be introduced in the future.

Predefined constants of type ``num`` are:

-   ``Pi`` The famous pi constant.
-   ``E`` The famous euler constant.
-   ``Eps`` The float epsilon defined by the system.
-   ``NumMax`` The maximum number a float can represent by the system.
-   ``NumMin`` The minimum number a float can represent by the system.
-   ``Inf`` Infinity float constant.

All textures defined in the scene representation are also available as variables of type ``vec4``.
These texture variables use the variable ``uv`` as their texture coordinate implicitly.
The above defined special variable and constant names have precedence over texture names.

Functions
---------

.. NOTE:: TODO: Add function signatures

-   ``abs`` TODO
-   ``acos`` TODO
-   ``angle`` TODO
-   ``asin`` TODO
-   ``atan`` TODO
-   ``atan2`` TODO
-   ``avg`` TODO
-   ``cbrt`` TODO
-   ``ccellnoise`` TODO
-   ``ceil`` TODO
-   ``cellnoise`` TODO
-   ``cfbm`` TODO
-   ``check_ray_flag`` TODO
-   ``checkerboard`` TODO
-   ``clamp`` TODO
-   ``cnoise`` TODO
-   ``color`` TODO
-   ``cos`` TODO
-   ``cperlin`` TODO
-   ``cpnoise`` TODO
-   ``cross`` TODO
-   ``cvoronoi`` TODO
-   ``deg`` TODO
-   ``dist`` TODO
-   ``dot`` TODO
-   ``exp`` TODO
-   ``exp2`` TODO
-   ``fbm`` TODO
-   ``floor`` TODO
-   ``hash`` TODO
-   ``hsltorgb`` TODO
-   ``hsvtorgb`` TODO
-   ``length`` TODO
-   ``log`` TODO
-   ``log10`` TODO
-   ``max`` TODO
-   ``min`` TODO
-   ``mix`` TODO
-   ``noise`` TODO
-   ``norm`` TODO
-   ``perlin`` TODO
-   ``pnoise`` TODO
-   ``pow`` TODO
-   ``rad`` TODO
-   ``rgbtohsl`` TODO
-   ``rgbtohsv`` TODO
-   ``rgbtoxyz`` TODO
-   ``round`` TODO
-   ``select`` TODO
-   ``sin`` TODO
-   ``smootherstep`` TODO
-   ``smoothstep`` TODO
-   ``snoise`` TODO
-   ``sperlin`` TODO
-   ``sqrt`` TODO
-   ``sum`` TODO
-   ``tan`` TODO
-   ``transform_point`` TODO
-   ``transform_direction`` TODO
-   ``transform_normal`` TODO
-   ``vec2`` TODO
-   ``vec3`` TODO
-   ``vec4`` TODO
-   ``voronoi`` TODO
-   ``xyztorgb`` TODO

.. NOTE:: More functions will be added in the future.

All textures defined in the scene representation are also available as functions with signature ``vec4 TEXTURE(vec2)``, with ``TEXTURE`` being the texture name.
The above defined function names have precedence over texture names, if the signature matches.

