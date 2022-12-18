BSDFs
=====

This section contains a list of bsdfs supported by the Ignis renderer.
All number and color parameters can be connected to a shading network or texture via :ref:`PExpr <PExpr>`.

A bsdf is specified in the :monosp:`bsdfs` block with a :monosp:`name` and a :monosp:`type`.
The type has to be one of the bsdfs listed at this section below.

.. code-block:: javascript
    
    {
        // ...
        "bsdfs": [
            // ...
            {"name":"NAME", "type":"TYPE", /* DEPENDS ON TYPE */},
            // ...
        ]
        // ...
    }

.. _bsdf-diffuse:

Diffuse (:monosp:`diffuse`)
----------------------------------

.. objectparameters::

  * - reflectance
    - |color|
    - :code:`0.8`
    - Yes
    - Albedo
  * - roughness
    - |number|
    - :code:`0`
    - Yes
    - Isotropic roughness. If :paramtype:`roughness` > 0, the Oren-Nayar model will be used.

.. subfigstart::
  
.. subfigure:: images/bsdf_diffuse.jpg

  Diffuse

.. subfigure::  images/bsdf_roughdiffuse.jpg
  
  Rough diffuse, also called Oren-Nayar 

.. subfigend::
  :label: fig-diffuse

.. _bsdf-dielectric:

Dielectric (:monosp:`dielectric`)
----------------------------------------

.. objectparameters::

  * - specular_reflectance
    - |color|
    - :code:`1`
    - Yes
    - TODO
  * - specular_transmittance
    - |color|
    - :code:`1`
    - Yes
    - TODO
  * - ext_ior, int_ior
    - |number|
    - ~vacuum, ~bk7
    - Yes
    - Specifies exterior and interior index of refraction.
  * - ext_ior_material, int_ior_material
    - |string|
    - *None*, *None*
    - No
    - Has to be one of the available presets listed :ref:`here <bsdf-dielectric-list>`.
  * - thin
    - |bool|
    - |false|
    - No
    - True if the glass should be treated as a thin interface. :paramtype:`int_ior` will be always the inside of the thin surface, regardless of the direction of the surface normal.
  * - roughness_u, roughness_v
    - |number|
    - :code:`0`, :code:`0`
    - Yes
    - Roughness of the dielectric. Can be specified isotropic using :paramtype:`roughness` as well. :paramtype:`thin` will be ignored if roughness is greater than zero.

.. subfigstart::

.. subfigure::  images/bsdf_dielectric.jpg
  :align: center
  
  Dielectric

.. subfigure::  images/bsdf_thindielectric.jpg
  :align: center
  
  Dielectric with thin approximation 

.. subfigure::  images/bsdf_roughdielectric.jpg
  :align: center
  
  Rough dielectric

.. subfigend::
  :label: fig-dielectric 

.. _bsdf-conductor:

Conductor (:monosp:`conductor`)
--------------------------------------

.. objectparameters::

  * - eta, k
    - |color|
    - ~Perfect mirror
    - Yes
    - Real and imaginary components of the material's index of refraction.
  * - material
    - |string|
    - *None*
    - No
    - Instead of :paramtype:`eta`, :paramtype:`k` a material name can be specified. Available presets are listed :ref:`here <bsdf-conductor-list>`.
  * - specular_reflectance
    - |color|
    - :code:`1`
    - Yes
    - Optional factor that can be used to modulate the specular reflection component.
      Note that for physical realism, this parameter should never be touched. 
  * - roughness_u, roughness_v
    - |number|
    - :code:`0`, :code:`0`
    - Yes
    - Roughness terms. Can be specified isotropic using :paramtype:`roughness` as well.

.. subfigstart::

.. subfigure::  images/bsdf_conductor.jpg
  :align: center
  
  Gold conductor

.. subfigure::  images/bsdf_roughconductor.jpg
  :align: center
  
  Rough gold conductor

.. subfigure::  images/bsdf_mirror.jpg
  :align: center
  
  A perfect conductor (also called a mirror), can be specified with :paramtype:`eta` = 0 and :paramtype:`k` = 1

.. subfigend::
  :label: fig-conductor

.. _bsdf-plastic:

Plastic (:monosp:`plastic`)
----------------------------------

.. objectparameters::

  * - specular_reflectance
    - |color|
    - :code:`1`
    - Yes
    - TODO
  * - diffuse_reflectance
    - |color|
    - :code:`0.8`
    - Yes
    - TODO
  * - ext_ior, int_ior
    - |number|
    - ~vacuum, ~bk7
    - Yes   
    - Specifies exterior and interior index of refraction.
  * - ext_ior_material, int_ior_material
    - |string|
    - *None*, *None*
    - No
    - Has to be one of the available presets listed :ref:`here <bsdf-dielectric-list>`.
  * - roughness_u, roughness_v
    - |number|
    - :code:`0`, :code:`0`
    - Yes
    - Roughness terms. Can be specified isotropic using :paramtype:`roughness` as well. 

.. subfigstart::

.. subfigure::  images/bsdf_plastic.jpg
  :align: center
  
  Plastic

.. subfigure::  images/bsdf_roughplastic.jpg
  :align: center
  
  Rough plastic

.. subfigend::
  :label: fig-plastic

.. _bsdf-phong:

Phong (:monosp:`phong`)
-----------------------

.. objectparameters::

  * - specular_reflectance
    - |color|
    - :code:`1`
    - Yes
    - TODO
  * - exponent
    - |number|
    - :code:`30`
    - Yes
    - TODO

.. subfigstart::

.. subfigure::  images/bsdf_phong.jpg
  :align: center
  
  Phong

.. subfigend::
  :label: fig-phong

.. NOTE:: It is not recommended to use this BSDF for new projects as it disregards some PBR principles and is only included for legacy purposes.

.. _bsdf-principled:

Principled (:monosp:`principled`)
------------------------------------------

.. objectparameters::

  * - base_color
    - |color|
    - :code:`0.8`
    - Yes
    - TODO
  * - metallic
    - |number|
    - :code:`0`
    - Yes
    - TODO
  * - roughness
    - |number|
    - :code:`0.5`
    - Yes
    - TODO
  * - anisotropic
    - |number|
    - :code:`0`
    - Yes
    - TODO
  * - ior
    - |number|
    - ~bk7   
    - Yes
    - Specifies index of refraction.
  * - ior_material
    - |string|
    - *None*
    - No
    - Has to be one of the available presets listed :ref:`here <bsdf-dielectric-list>`.
  * - thin
    - |bool|
    - |false|
    - No
    - TODO
  * - flatness
    - |number|
    - :code:`0`
    - Yes
    - TODO
  * - specular_transmission
    - |number|
    - :code:`0`
    - Yes
    - TODO
  * - specular_tint
    - |number|
    - :code:`0`
    - Yes
    - TODO
  * - sheen
    - |number|
    - :code:`0`
    - Yes
    - TODO
  * - sheen_tint
    - |number|
    - :code:`0`
    - Yes
    - TODO
  * - clearcoat
    - |number|
    - :code:`0`
    - Yes
    - TODO
  * - clearcoat_gloss
    - |number|
    - :code:`0`
    - Yes
    - TODO
  * - clearcoat_top_only
    - |bool|
    - |true|
    - No
    - |true| if clearcoat should only be applied to the front side of the surface only.
  * - clearcoat_roughness
    - |number|
    - :code:`0.1`
    - Yes
    - TODO
  * - diffuse_transmission
    - |number|
    - :code:`0`
    - Yes
    - TODO

.. subfigstart::

.. subfigure::  images/bsdf_principled.jpg
  :align: center
  
  Disney *Principled* 

.. subfigend::
  :label: fig-principled

.. _bsdf-blend:

Blend (:monosp:`blend`)
-----------------------

.. objectparameters::

  * - first, second
    - |bsdf|
    - *None*
    - No
    - TODO
  * - weight
    - |number|
    - :code:`0.5`
    - Yes
    - TODO

.. subfigstart::

.. subfigure::  images/bsdf_blend.jpg
  :align: center
  
  Blend of two bsdfs. One dielectric, one diffuse

.. subfigend::
  :label: fig-blend

.. _bsdf-mask:

Mask (:monosp:`mask`)
---------------------

.. objectparameters::

  * - bsdf
    - |bsdf|
    - *None*
    - No
    - TODO
  * - weight
    - |number|
    - :code:`0.5`
    - Yes
    - TODO
  * - inverted
    - |bool|
    - |false|
    - No
    - TODO

.. subfigstart::

.. subfigure::  images/bsdf_mask.jpg
  :align: center
  
  Mask with a uniform weight

.. subfigend::
  :label: fig-mask

.. _bsdf-cutoff:

Cutoff (:monosp:`cutoff`)
-------------------------

.. objectparameters::

  * - bsdf
    - |bsdf|
    - *None*
    - No
    - TODO
  * - weight
    - |number|
    - :code:`0.5`
    - Yes
    - TODO
  * - cutoff
    - |number|
    - :code:`0.5`
    - Yes
    - TODO
  * - inverted
    - |bool|
    - |false|
    - No
    - TODO

.. subfigstart::

.. subfigure::  images/bsdf_cutoff.jpg
  :align: center
  
  Cutoff with a uniform weight

.. subfigend::
  :label: fig-cutoff

.. _bsdf-passthrough:

Passthrough (:monosp:`passthrough`)
-----------------------------------

.. subfigstart::

.. subfigure::  images/bsdf_passthrough.jpg
  :align: center
  
  Passthrough

.. subfigend::
  :label: fig-passthrough

.. WARNING:: The :monosp:`passthrough` bsdf should be used carefully, as simple techniques like Next-Event Estimation still intersect the object geometry.

.. _bsdf-normalmap:

Normal mapping (:monosp:`normalmap`)
------------------------------------

.. objectparameters::

  * - bsdf
    - |bsdf|
    - *None*
    - No
    - TODO
  * - map
    - |color|
    - :code:`1`
    - Yes
    - Usually a texture used for normal mapping.
  * - strength
    - |number|
    - :code:`1`
    - Yes
    - TODO

.. subfigstart::

.. subfigure::  images/bsdf_normalmap.jpg
  :align: center
  
  Normal mapping

.. subfigend::
  :label: fig-normalmap

.. _bsdf-bumpmap:

Bump mapping (:monosp:`bumpmap`)
--------------------------------

.. objectparameters::

  * - bsdf
    - |bsdf|
    - *None*
    - No
    - TODO
  * - map
    - |texture|
    - *None*
    - Yes
    - A grayscale texture used for texture mapping.
  * - strength
    - |number|
    - :code:`1`
    - Yes
    - TODO

.. subfigstart::

.. subfigure::  images/bsdf_bumpmap.jpg
  :align: center
  
  Bump mapping

.. subfigend::
  :label: fig-bumpmap

.. _bsdf-transform:

Transform (:monosp:`transform`)
-------------------------------

.. objectparameters::

  * - bsdf
    - |bsdf|
    - *None*
    - No
    - Bsdf the normal transformation will be applied to.
  * - normal
    - |vector|
    - :code:`(0,0,1)`
    - Yes
    - Normal to use instead of the callee normal (e.g., surface normal).
  * - tangent
    - |vector|
    - *None*
    - Yes
    - Tangent to use instead of the callee tangent (e.g., surface tangent). Will be calculated from the normal parameter if not set.

.. subfigstart::

.. subfigure::  images/bsdf_transform.jpg
  :align: center
  
  Normal transformed by a PExpr to mimic a normal map

.. subfigend::
  :label: fig-transform

.. _bsdf-djmeasured:

Dupuy & Jakob measured materials (:monosp:`djmeasured`)
-------------------------------------------------------

.. objectparameters::

  * - filename
    - |string|
    - *None*
    - No
    - Path to a valid brdf.
  * - tint
    - |color|
    - :code:`1`
    - Yes
    - Tint.

.. subfigstart::

.. subfigure::  images/bsdf_djmeasured.jpg
  :align: center
  
  Dupuy & Jakob based measured material. More information and measured data available at https://rgl.epfl.ch/materials

.. subfigend::
  :label: fig-djmeasured

.. _bsdf-klems:

Klems (:monosp:`klems`)
-----------------------

.. objectparameters::

  * - filename
    - |string|
    - *None*
    - No
    - Path to a valid windows xml specifying a klems bsdf.
  * - base_color
    - |color|
    - :code:`1`
    - Yes
    - Tint.
  * - up
    - |vector|
    - :code:`(0,0,1)`
    - Yes
    - Up vector

.. WARNING:: The :monosp:`klems` bsdf is still experimental and has some issues.

.. _bsdf-tensortree:

TensorTree (:monosp:`tensortree`)
---------------------------------

.. objectparameters::

  * - filename
    - |string|
    - *None*
    - No
    - Path to a valid windows xml specifying a tensortree bsdf.
  * - base_color
    - |color|
    - :code:`1`
    - Yes
    - Tint.
  * - up
    - |vector|
    - :code:`(0,0,1)`
    - Yes
    - Up vector

.. _bsdf-dielectric-list:

List of preset index of refractions
-----------------------------------

Currently the following materials are available as presets:

* **vacuum**
* **bk7**
* **glass** `same as bk7`
* **helium**
* **hydrogen**
* **air**
* **water**
* **ethanol**
* **diamond**
* **polypropylene**

.. _bsdf-conductor-list:

List of preset conductors
-------------------------

Currently the following materials are available as presets:

* **aluminum**
* **brass**
* **copper**
* **gold**
* **iron**
* **lead**
* **mercury**
* **platinum**
* **silver**
* **titanium**
* **none** `~ a perfect mirror`