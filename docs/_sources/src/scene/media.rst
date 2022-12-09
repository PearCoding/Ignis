Participating Media
===================

This section contains a list of media supported by the Ignis renderer.
All number and color parameters can be connected to a shading network or texture via :ref:`PExpr <PExpr>`.

A medium is specified in the :monosp:`media` block with a :monosp:`name` and a :monosp:`type`.
The type has to be one of the media listed at this section below.

.. code-block:: javascript
    
    {
        // ...
        "media": [
            // ...
            {"name":"NAME", "type":"TYPE", /* DEPENDS ON TYPE */},
            // ...
        ]
        // ...
    }


.. WARNING:: The participating media interface allows usage of :ref:`PExpr <PExpr>` but the expressions are evaluated via a surface context and do **not** vary inside the volume, but along the surface! This behavior is considered non-intuitive and might change in the future.

.. _bsdf-homogeneous:

Homogeneous (:monosp:`homogeneous`, :monosp:`constant`)
-------------------------------------------------------

.. objectparameters::

  * - sigma_s
    - |color|
    - :code:`0`
    - Yes
    - Scattering term.
  * - sigma_a
    - |color|
    - :code:`0`
    - Yes
    - Attenuation term.
  * - g
    - |number|
    - :code:`0`
    - Yes
    - Henyey-Greenstein phase parameter. Valid range is [-1, 1]. 0 = Isotropic, -1 = Full back scattering, 1 = Full forward scattering.


.. _bsdf-vacuum:

Vacuum (:monosp:`vacuum`)
-------------------------------------------------------

This medium has no properties and is used internally for the absence of volumes. It is better to **not** define a medium in the scene description instead of using this type.
