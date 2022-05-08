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

Vectorized types like ``vec2``, ``vec3`` and ``vec4`` can be cast or extended to eachother by the access/swizzle operation like:

.. code-block:: maxima

    vec3(1,2,3).zyxx /* Return type is vec4 (same as color) with values (3,2,1,1) */
    

Variables
---------

Some special variables are available:

-   ``uv`` The current texture coordinates as a ``vec2``.
-   ``P`` World position if bsdf or medium, else a zero ``vec3``.
-   ``N`` World shading normal if bsdf, else a zero ``vec3``.

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
-   ``vec2`` TODO
-   ``vec3`` TODO
-   ``vec4`` TODO
-   ``voronoi`` TODO

.. NOTE:: More functions will be added in the future.

All textures defined in the scene representation are also available as functions with signature ``vec4 TEXTURE(vec2)``, with ``TEXTURE`` being the texture name.
The above defined function names have precedence over texture names, if the signature matches.

