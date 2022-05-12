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