Entities
========

In contrary to many other rendering frameworks only one type of entity is available in Ignis.

All entities are defined by a :monosp:`name`, :monosp:`shape`, :monosp:`bsdf`, :monosp:`transform` and some other properties defined below. 
Only the :monosp:`shape` and :monosp:`name` parameter are mandatory.

If no transformation is specified, the identity transformation is used. If no bsdf is specified, a black, non-scattering bsdf will be used.

.. code-block:: javascript

    {
        // ...
        "entities": [
            // ...
            {"name":"NAME", "shape":"SHAPE", "bsdf":"BSDF", "transform":TRANSFORM}},
            // ...
        ]
        // ...
    }

.. NOTE:: Entities do not support PExpr expressions.

.. objectparameters::

  * - shape
    - |string|
    - *None*
    - The name of the actual shape.
  * - bsdf
    - |string|
    - *None*
    - The name of the actual bsdf.
  * - inner_medium
    - |string|
    - *None*
    - Optional name for the interior medium.
  * - outer_medium
    - |string|
    - *None*
    - Optional name for the outerior medium.
  * - transform
    - |transform|
    - Identity
    - Apply given transformation to shape.
  * - camera_visible
    - |bool|
    - |true|
    - Set |true| if entity shall be directly visible from the camera
  * - light_visible
    - |bool|
    - |true|
    - Set |true| if entity shall be visible for a ray starting from a light source
  * - bounce_visible
    - |bool|
    - |true|
    - Set |true| if entity shall be visible for an indirect ray
  * - shadow_visible
    - |bool|
    - |true|
    - Set |true| if entity shall be visible for a so called shadow ray. A shadow ray is a ray emitted to test if non-occluded connection between a point on a surface or medium and a light source is available.