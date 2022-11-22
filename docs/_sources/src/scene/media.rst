Media
=====

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