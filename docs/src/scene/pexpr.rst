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

-   ``bool`` Boolean with |true| and |false| states.
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
-   ``prim_coords`` Primitive specific coordinates if surface, else zero vector.

some as ``vec3``:

-   ``uvw`` The current texture coordinates given as triplet.
-   ``V`` World ray direction facing from a surface or any other source outwards.
-   ``Rd`` Same as ``V``.
-   ``Ro`` World ray origin.
-   ``P`` World position if bsdf or medium, else zero vector.
-   ``Np`` Normalized position if bsdf or medium, else zero vector.
-   ``N`` World shading normal if bsdf, else zero vector.
-   ``Ng`` World geometry normal if bsdf, else zero vector.
-   ``Nx`` World shading tangent if bsdf, else zero vector.
-   ``Ny`` World shading bitangent if bsdf, else zero vector.

few as ``int``:

-   ``entity_id`` Contains the id of the current entity or ``-1`` if not on a surface.
-   ``Ix`` Contains the current pixel x index or ``0`` if not available.
-   ``Iy`` Contains the current pixel y index or ``0`` if not available.

and a few as ``bool``:

-   ``frontside`` |true| if normal and ray orientation is front facing. This will be always ``true`` if the point is not lying on a surface.

Predefined constants of type ``num`` are:

-   ``Pi`` The famous pi constant.
-   ``E`` The famous euler constant.
-   ``Eps`` The float epsilon defined by the system.
-   ``NumMax`` The maximum number a float can represent by the system.
-   ``NumMin`` The minimum number a float can represent by the system.
-   ``Inf`` Infinity float constant.

All textures defined in the scene representation are also available as variables of type ``vec4``.
These texture variables use the variable ``uv`` as their texture coordinate implicitly.
The above defined special variable and constant names have precedence over texture names and parameters as explained below.

.. _pexpr_parameters:

Parameters
----------

Defining parameters inside the :code:`"parameters"` section in the scene description allows the convenient usage of user input in the viewer and the cli. All defined parameters are available as variables inside a PExpr expression.
Parameters can be defined as follow:

.. code-block:: javascript
    
    {
        // ...
        "parameters": [
            // ...
            { "name":"NAME1", "type":"number", "value":42 },
            { "name":"NAME2", "type":"vector", "value":[1,0,0] },
            { "name":"NAME3", "type":"color",  "value":[1,0,1] }
            // ...
        ]
        // ...
    }

Currently only the types ``number`` for ``num``, ``vector`` for ``vec3`` and ``color`` for ``vec4`` are available. The given ``value`` can not be a PExpr!
The above defined parameters have precedence over texture names, but not the above defined special variables and constants.

.. NOTE:: Defining color with alpha is currently not supported. Use an array of three numbers to define color.

Functions
---------

-   ``abs(num) -> num``
-   ``abs(vec2) -> vec2``
-   ``abs(vec3) -> vec3``
-   ``abs(vec4) -> vec4``
-   ``abs(int) -> int``
-   ``acos(num) -> num``
-   ``acos(vec2) -> vec2``
-   ``acos(vec3) -> vec3``
-   ``acos(vec4) -> vec4``
-   ``angle(vec2, vec2) -> num``
-   ``angle(vec3, vec3) -> num``
-   ``angle(vec4, vec4) -> num``
-   ``asin(num) -> num``
-   ``asin(vec2) -> vec2``
-   ``asin(vec3) -> vec3``
-   ``asin(vec4) -> vec4``
-   ``atan(num) -> num``
-   ``atan(vec2) -> vec2``
-   ``atan(vec3) -> vec3``
-   ``atan(vec4) -> vec4``
-   ``atan2(num, num) -> num``
-   ``atan2(vec2, vec2) -> vec2``
-   ``atan2(vec3, vec3) -> vec3``
-   ``atan2(vec4, vec4) -> vec4``
-   ``avg(vec2) -> num``
-   ``avg(vec3) -> num``
-   ``avg(vec4) -> num``
-   ``blackbody(num) -> vec4``
-   ``cbrt(num) -> num``
-   ``cbrt(vec2) -> vec2``
-   ``cbrt(vec3) -> vec3``
-   ``cbrt(vec4) -> vec4``
-   ``ccellnoise(num) -> vec4``
-   ``ccellnoise(num, num) -> vec4``
-   ``ccellnoise(vec2) -> vec4``
-   ``ccellnoise(vec2, num) -> vec4``
-   ``ccellnoise(vec3) -> vec4``
-   ``ccellnoise(vec3, num) -> vec4``
-   ``ceil(num) -> num``
-   ``ceil(vec2) -> vec2``
-   ``ceil(vec3) -> vec3``
-   ``ceil(vec4) -> vec4``
-   ``cellnoise(num) -> num``
-   ``cellnoise(num, num) -> num``
-   ``cellnoise(vec2) -> num``
-   ``cellnoise(vec2, num) -> num``
-   ``cellnoise(vec3) -> num``
-   ``cellnoise(vec3, num) -> num``
-   ``cfbm(vec2) -> vec4``
-   ``cfbm(vec2, num) -> vec4``
-   ``check_ray_flag(str) -> bool``
-   ``checkerboard(vec2) -> num``
-   ``checkerboard(vec3) -> num``
-   ``clamp(num, num, num) -> num``
-   ``clamp(vec2, vec2, vec2) -> vec2``
-   ``clamp(vec3, vec3, vec3) -> vec3``
-   ``clamp(vec4, vec4, vec4) -> vec4``
-   ``clamp(int, int, int) -> int``
-   ``cnoise(num) -> vec4``
-   ``cnoise(num, num) -> vec4``
-   ``cnoise(vec2) -> vec4``
-   ``cnoise(vec2, num) -> vec4``
-   ``cnoise(vec3) -> vec4``
-   ``cnoise(vec3, num) -> vec4``
-   ``color(num, num, num, num) -> vec4``
-   ``color(num, num, num) -> vec4``
-   ``color(num) -> vec4``
-   ``cos(num) -> num``
-   ``cos(vec2) -> vec2``
-   ``cos(vec3) -> vec3``
-   ``cos(vec4) -> vec4``
-   ``cperlin(vec2) -> vec4``
-   ``cperlin(vec2, num) -> vec4``
-   ``cpnoise(num) -> vec4``
-   ``cpnoise(num, num) -> vec4``
-   ``cpnoise(vec2) -> vec4``
-   ``cpnoise(vec2, num) -> vec4``
-   ``cpnoise(vec3) -> vec4``
-   ``cpnoise(vec3, num) -> vec4``
-   ``cross(vec3, vec3) -> vec3``
-   ``cvoronoi(vec2) -> vec4``
-   ``cvoronoi(vec2, num) -> vec4``
-   ``deg(num) -> num``
-   ``deg(vec2) -> vec2``
-   ``deg(vec3) -> vec3``
-   ``deg(vec4) -> vec4``
-   ``dist(vec2, vec2) -> num``
-   ``dist(vec3, vec3) -> num``
-   ``dist(vec4, vec4) -> num``
-   ``dot(vec2, vec2) -> num``
-   ``dot(vec3, vec3) -> num``
-   ``dot(vec4, vec4) -> num``
-   ``exp(num) -> num``
-   ``exp(vec2) -> vec2``
-   ``exp(vec3) -> vec3``
-   ``exp(vec4) -> vec4``
-   ``exp2(num) -> num``
-   ``exp2(vec2) -> vec2``
-   ``exp2(vec3) -> vec3``
-   ``exp2(vec4) -> vec4``
-   ``fbm(vec2) -> num``
-   ``fbm(vec2, num) -> num``
-   ``floor(num) -> num``
-   ``floor(vec2) -> vec2``
-   ``floor(vec3) -> vec3``
-   ``floor(vec4) -> vec4``
-   ``fmod(num, num) -> num``
-   ``fmod(vec2, vec2) -> vec2``
-   ``fmod(vec3, vec3) -> vec3``
-   ``fmod(vec4, vec4) -> vec4``
-   ``fract(num) -> num``
-   ``fract(vec2) -> vec2``
-   ``fract(vec3) -> vec3``
-   ``fract(vec4) -> vec4``
-   ``fresnel_conductor(num, num, num) -> num``
-   ``fresnel_dielectric(num, num) -> num``
-   ``hash(num) -> num``
-   ``hsltorgb(vec4) -> vec4``
-   ``hsvtorgb(vec4) -> vec4``
-   ``length(vec2) -> num``
-   ``length(vec3) -> num``
-   ``length(vec4) -> num``
-   ``log(num) -> num``
-   ``log(vec2) -> vec2``
-   ``log(vec3) -> vec3``
-   ``log(vec4) -> vec4``
-   ``log10(num) -> num``
-   ``log10(vec2) -> vec2``
-   ``log10(vec3) -> vec3``
-   ``log10(vec4) -> vec4``
-   ``log2(num) -> num``
-   ``log2(vec2) -> vec2``
-   ``log2(vec3) -> vec3``
-   ``log2(vec4) -> vec4``
-   ``lookup(str, bool, num, vec2, ...) -> num``
-   ``luminance(vec4) -> num``
-   ``max(num, num) -> num``
-   ``max(vec2, vec2) -> vec2``
-   ``max(vec3, vec3) -> vec3``
-   ``max(vec4, vec4) -> vec4``
-   ``max(int, int) -> int``
-   ``min(num, num) -> num``
-   ``min(vec2, vec2) -> vec2``
-   ``min(vec3, vec3) -> vec3``
-   ``min(vec4, vec4) -> vec4``
-   ``min(int, int) -> int``
-   ``mix(num, num, num) -> num``
-   ``mix(vec2, vec2, num) -> vec2``
-   ``mix(vec3, vec3, num) -> vec3``
-   ``mix(vec4, vec4, num) -> vec4``
-   ``mix_burn(vec4, vec4, num) -> vec4``
-   ``mix_color(vec4, vec4, num) -> vec4``
-   ``mix_dodge(vec4, vec4, num) -> vec4``
-   ``mix_hue(vec4, vec4, num) -> vec4``
-   ``mix_linear(vec4, vec4, num) -> vec4``
-   ``mix_overlay(vec4, vec4, num) -> vec4``
-   ``mix_saturation(vec4, vec4, num) -> vec4``
-   ``mix_screen(vec4, vec4, num) -> vec4``
-   ``mix_soft(vec4, vec4, num) -> vec4``
-   ``mix_value(vec4, vec4, num) -> vec4``
-   ``noise(num) -> num``
-   ``noise(num, num) -> num``
-   ``noise(vec2) -> num``
-   ``noise(vec2, num) -> num``
-   ``noise(vec3) -> num``
-   ``noise(vec3, num) -> num``
-   ``norm(vec2) -> vec2``
-   ``norm(vec3) -> vec3``
-   ``norm(vec4) -> vec4``
-   ``perlin(vec2) -> num``
-   ``perlin(vec2, num) -> num``
-   ``pingpong(num, num) -> num``
-   ``pingpong(vec2, vec2) -> vec2``
-   ``pingpong(vec3, vec3) -> vec3``
-   ``pingpong(vec4, vec4) -> vec4``
-   ``pnoise(num) -> num``
-   ``pnoise(num, num) -> num``
-   ``pnoise(vec2) -> num``
-   ``pnoise(vec2, num) -> num``
-   ``pnoise(vec3) -> num``
-   ``pnoise(vec3, num) -> num``
-   ``pow(num, num) -> num``
-   ``pow(vec2, vec2) -> vec2``
-   ``pow(vec3, vec3) -> vec3``
-   ``pow(vec4, vec4) -> vec4``
-   ``rad(num) -> num``
-   ``rad(vec2) -> vec2``
-   ``rad(vec3) -> vec3``
-   ``rad(vec4) -> vec4``
-   ``reflect(vec3, vec3) -> vec3``
-   ``rgbtohsl(vec4) -> vec4``
-   ``rgbtohsv(vec4) -> vec4``
-   ``rgbtoxyz(vec4) -> vec4``
-   ``rotate_axis(vec3, num, vec3) -> vec3``
-   ``rotate_euler(vec3, vec3) -> vec3``
-   ``rotate_euler_inverse(vec3, vec3) -> vec3``
-   ``round(num) -> num``
-   ``round(vec2) -> vec2``
-   ``round(vec3) -> vec3``
-   ``round(vec4) -> vec4``
-   ``select(bool, bool, bool) -> bool``
-   ``select(bool, int, int) -> int``
-   ``select(bool, num, num) -> num``
-   ``select(bool, vec2, vec2) -> vec2``
-   ``select(bool, vec3, vec3) -> vec3``
-   ``select(bool, vec4, vec4) -> vec4``
-   ``select(bool, str, str) -> str``
-   ``sin(num) -> num``
-   ``sin(vec2) -> vec2``
-   ``sin(vec3) -> vec3``
-   ``sin(vec4) -> vec4``
-   ``smax(num, num, num) -> num``
-   ``smax(vec2, vec2, vec2) -> vec2``
-   ``smax(vec3, vec3, vec3) -> vec3``
-   ``smax(vec4, vec4, vec4) -> vec4``
-   ``smin(num, num, num) -> num``
-   ``smin(vec2, vec2, vec2) -> vec2``
-   ``smin(vec3, vec3, vec3) -> vec3``
-   ``smin(vec4, vec4, vec4) -> vec4``
-   ``smootherstep(num) -> num``
-   ``smoothstep(num) -> num``
-   ``snap(num, num) -> num``
-   ``snap(vec2, vec2) -> vec2``
-   ``snap(vec3, vec3) -> vec3``
-   ``snap(vec4, vec4) -> vec4``
-   ``snoise(num) -> num``
-   ``snoise(num, num) -> num``
-   ``snoise(vec2) -> num``
-   ``snoise(vec2, num) -> num``
-   ``snoise(vec3) -> num``
-   ``snoise(vec3, num) -> num``
-   ``sperlin(vec2) -> num``
-   ``sperlin(vec2, num) -> num``
-   ``sqrt(num) -> num``
-   ``sqrt(vec2) -> vec2``
-   ``sqrt(vec3) -> vec3``
-   ``sqrt(vec4) -> vec4``
-   ``sum(vec2) -> num``
-   ``sum(vec3) -> num``
-   ``sum(vec4) -> num``
-   ``tan(num) -> num``
-   ``tan(vec2) -> vec2``
-   ``tan(vec3) -> vec3``
-   ``tan(vec4) -> vec4``
-   ``transform_direction(vec3, str, str) -> vec3``
-   ``transform_normal(vec3, str, str) -> vec3``
-   ``transform_point(vec3, str, str) -> vec3``
-   ``trunc(num) -> num``
-   ``trunc(vec2) -> vec2``
-   ``trunc(vec3) -> vec3``
-   ``trunc(vec4) -> vec4``
-   ``vec2(num, num) -> vec2``
-   ``vec2(num) -> vec2``
-   ``vec3(num, num, num) -> vec3``
-   ``vec3(num) -> vec3``
-   ``vec4(num, num, num, num) -> vec4``
-   ``vec4(num) -> vec4``
-   ``voronoi(vec2) -> num``
-   ``voronoi(vec2, num) -> num``
-   ``wrap(num, num, num) -> num``
-   ``wrap(vec2, vec2, vec2) -> vec2``
-   ``wrap(vec3, vec3, vec3) -> vec3``
-   ``wrap(vec4, vec4, vec4) -> vec4``
-   ``xyztorgb(vec4) -> vec4``

All textures defined in the scene representation are also available as functions with signature ``TEXTURE(vec2) -> vec4``, with ``TEXTURE`` being the texture name.
The above defined function names have precedence over texture names, if the signature matches.
