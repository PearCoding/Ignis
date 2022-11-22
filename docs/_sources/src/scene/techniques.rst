Techniques
==========

Techniques, also called integrators, are the heart of the rendering framework.
They allow the calculation of global illumination based on different methods or the acquisition of information about the underlying scene.

A technique is specified in the :monosp:`technique` block with a :monosp:`type`.
The type has to be one of the techniques listed at this section below.

.. NOTE:: Techniques do not support PExpr expressions.

Path Tracer (:monosp:`path`)
---------------------------------------------

.. objectparameters::

  * - max_depth
    - |int|
    - :code:`64`
    - Maximum depth of rays to be traced.
  * - clamp
    - |number|
    - :code:`0`
    - Value to clamp contributions to. This introduces bias in favour of omitting outlier. 0 disables clamping.
  * - use_uniform_light_selector
    - |bool|
    - |false|
    - Use uniform light selection technique. The default adaptive technique is better in most cases, use with caution.
  * - aov_normals
    - |bool|
    - |false|
    - Enable normal output as aov.
  * - aov_mis
    - |bool|
    - |false|
    - Enable two aovs displaying mis related weights.

This is the default and probably most used type. It calculates the full global illumination in the scene.
If participating media is used, it is recommended to use the volumetric path tracer instead.

Volume Path Tracer (:monosp:`volpath`)
---------------------------------------------

.. objectparameters::

  * - max_depth
    - |int|
    - :code:`64`
    - Maximum depth of rays to be traced.
  * - clamp
    - |number|
    - :code:`0`
    - Value to clamp contributions to. This introduces bias in favour of omitting outlier. 0 disables clamping.
  * - use_uniform_light_selector
    - |bool|
    - |false|
    - Use uniform light selection technique. The default adaptive technique is better in most cases, use with caution.

A simple volumetric path tracer. It calculates the full global illumination in the scene.

Photonmapper (:monosp:`photonmapper`)
---------------------------------------------

.. objectparameters::

  * - max_depth
    - |int|
    - :code:`8`
    - Maximum depth of rays to be traced.
  * - radius
    - |number|
    - :code:`0.01`
    - Initial merging radius.
  * - clamp
    - |number|
    - :code:`0`
    - Value to clamp contributions to. This introduces bias in favour of omitting outlier. 0 disables clamping.
  * - aov
    - |bool|
    - |false|
    - Enable aovs displaying internal weights.

Ambient Occlusion (:monosp:`ao`)
---------------------------------------------

This technique calculates the ambient occlusion in the scene. Currently no parameters are available to tinkle around.

Wireframe (:monosp:`wireframe`)
---------------------------------------------

This technique renders the scene in wireframe. Currently no parameters are available to tinkle around.

Debug (:monosp:`debug`)
---------------------------------------------

This is a special technique only usable with the :monosp:`igview` frontend. It displays scene specific information on the screen.