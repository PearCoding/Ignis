Techniques
==========

Techniques, also called integrators, are the heart of the rendering framework.
They allow the calculation of global illumination based on different methods or the acquisition of information about the underlying scene.

A technique is specified in the :monosp:`technique` block with a :monosp:`type`.
The type has to be one of the techniques listed at this section below.

Path Tracer (:monosp:`path`)
---------------------------------------------

.. objectparameters::

 * - max_depth
   - |int|
   - 64
   - Maximum depth of rays to be traced.

This is the default and probably most used type. It calculates the full global illumination in the scene.

Ambient Occlusion (:monosp:`ao`)
---------------------------------------------

This technique calculates the ambient occlusion in the scene. Currently no parameters are available to tinkle around.

Debug (:monosp:`debug`)
---------------------------------------------

This is a special technique only usable with the :monosp:`igview` frontend. It displays scene specific information on the screen.