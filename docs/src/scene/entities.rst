Entities
========

In contary to many other rendering frameworks only one type of entity is available in Ignis.

All entities are defined by a :monosp:`name`, :monosp:`shape`, :monosp:`bsdf` and :monosp:`transform`. 
Only the :monosp:`shape` and :monosp:`name` parameter are mandatory.

If no transformation is specified, the identity transformation is used. If no bsdf is specified, a black, non-scattering bsdf will be used.

.. NOTE:: Keep in mind that Ignis only supports flat transformation hierachies at the current stage of development. 

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