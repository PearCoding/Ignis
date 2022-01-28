Externals
=========

Ignis allows to embed external scene files. 

An external resource is specified in the :monosp:`externals` block with a :monosp:`type`.
The type has to be one of the externals listed at this section below.
In contrary to other blocks, an external block has the default type :monosp:`ignis`.
Therefore, if not specified explicitly, an external ignis json file will be included.

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

Ingis resource file (:monosp:`ignis`)
-------------------------------------

.. objectparameters::

 * - filename
   - |string|
   - *None*
   - Path to a valid .json file containing ignis resources. 
     If a technique, camera or film is specified it will only be used if the parent file has not specified it yet.
     If multiple external resource files specify a technique, camera or film only the first one will be used - of course if the parent file has not specified it yet.

Graphics Language Transmission Format "glTF" resource file (:monosp:`gltf`)
---------------------------------------------------------------------------

.. objectparameters::

 * - filename
   - |string|
   - *None*
   - Path to a valid .gltf or .glb file containing resources. Camera settings will only be used if no camera is specified yet.
