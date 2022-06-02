Externals
=========

Ignis allows to embed external scene files. 

An external resource is specified in the :monosp:`externals` block with a :monosp:`type`.
The type has to be one of the externals listed at this section below.
In contrary to other blocks, the type parameter is optional. If none is specified the actual type will be determined by the filename extension.

.. code-block:: javascript
    
    {
        // ...
        "externals": [
            // ...
            {"type":"TYPE" /*Optional*/, /* DEPENDS ON TYPE */},
            // ...
        ]
        // ...
    }

Ignis resource file (:monosp:`ignis`)
-------------------------------------

.. objectparameters::

 * - filename
   - |string|
   - *None*
   - Path to a valid .json file containing ignis resources. 
     Content will only be added if no object with the same name is defined yet. Same applies for the technique, camera or film settings.

Graphics Language Transmission Format "glTF" resource file (:monosp:`gltf`)
---------------------------------------------------------------------------

.. objectparameters::

 * - filename
   - |string|
   - *None*
   - Path to a valid .gltf or .glb file containing resources. Camera settings will only be used if no camera is specified yet.
