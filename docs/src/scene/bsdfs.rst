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
    - Reflectance factor. Should be less or equal to 1 for each component for physical correctness.
  * - specular_transmittance
    - |color|
    - :code:`1`
    - Yes
    - Transmittance factor. Should be less or equal to 1 for each component for physical correctness.
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
    - |true| if the glass should be treated as a thin interface. :paramtype:`int_ior` will be always the inside of the thin surface, regardless of the direction of the surface normal.
  * - roughness_u, roughness_v
    - |number|
    - :code:`0`, :code:`0`
    - Yes
    - Roughness of the dielectric. Can be specified implicitly using :paramtype:`roughness` and :paramtype:`anisotropic` instead. :paramtype:`thin` will be ignored if roughness is greater than zero.
  * - roughness, anisotropic
    - |number|
    - :code:`0`, :code:`0`
    - Yes
    - Roughness and anisotropic terms. Can be specified explicitly using :paramtype:`roughness_u` and :paramtype:`roughness_v` instead. Anisotropic is the amount of anisotropy in the roughness distribution. :paramtype:`thin` will be ignored if roughness is greater than zero.

.. subfigstart::

.. subfigure::  images/bsdf_dielectric.jpg
  :align: center
  
  Smooth dielectric

.. subfigure::  images/bsdf_thindielectric.jpg
  :align: center
  
  Smooth dielectric with thin approximation 

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
    - Roughness terms. Can be specified implicitly using :paramtype:`roughness` and :paramtype:`anisotropic` instead.
  * - roughness, anisotropic
    - |number|
    - :code:`0`, :code:`0`
    - Yes
    - Roughness and anisotropic terms. Can be specified explicitly using :paramtype:`roughness_u` and :paramtype:`roughness_v` instead. Anisotropic is the amount of anisotropy in the roughness distribution.

.. subfigstart::

.. subfigure::  images/bsdf_conductor.jpg
  :align: center
  
  Smooth gold conductor

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
    - Specular reflectance. Should be less or equal to 1 for each component for physical correctness.
  * - diffuse_reflectance
    - |color|
    - :code:`0.8`
    - Yes
    - Diffuse reflectance. Should be less or equal to 1 for each component for physical correctness.
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
    - Roughness terms. Can be specified implicitly using :paramtype:`roughness` and :paramtype:`anisotropic` instead.
  * - roughness, anisotropic
    - |number|
    - :code:`0`, :code:`0`
    - Yes
    - Roughness and anisotropic terms. Can be specified explicitly using :paramtype:`roughness_u` and :paramtype:`roughness_v` instead. Anisotropic is the amount of anisotropy in the roughness distribution.

.. subfigstart::

.. subfigure::  images/bsdf_plastic.jpg
  :align: center
  
  Smooth plastic

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
    - Reflectance factor. Should be less or equal to 1 for each component for physical correctness. See note below for PBR consideration.
  * - exponent
    - |number|
    - :code:`30`
    - Yes
    - Exponent of the lobe. Greater number results into a greater peak, therefore the visible peak spot will be smaller.

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
    - Base color of the principled bsdf. Should be less or equal to 1 for each component for physical correctness.
  * - metallic
    - |number|
    - :code:`0`
    - Yes
    - A number between 0 and 1. A metallic value of 1 displays a full metallic (conductor), which can not transmit via refraction.
  * - roughness_u, roughness_v
    - |number|
    - :code:`0.5`, :code:`0.5`
    - Yes
    - Anisotropic microfacet roughness for specular, diffuse and sheen reflection terms. Can be specified implicitly using :paramtype:`roughness` and :paramtype:`anisotropic` instead. The roughness is computed via the GGX method.
  * - roughness, anisotropic
    - |number|
    - :code:`0.5`, :code:`0`
    - Yes
    - The microfacet roughness for specular, diffuse and sheen reflection and anisotropic terms. The roughness is computed via the GGX method. Can be specified explicitly using :paramtype:`roughness_u` and :paramtype:`roughness_v` instead. Anisotropic is the amount of anisotropy in the roughness distribution.
  * - ior, reflective_ior, refractive_ior
    - |number|
    - ~bk7   
    - Yes
    - Specifies index of refraction. To distinguish between the ior used for the reflective fresnel term use :paramtype:`reflective_ior` and for the actual refraction :paramtype:`refractive_ior`. If one of the latter two is specified, :paramtype:`ior` will be ignored.
  * - ior_material, reflective_ior_material, refractive_ior_material
    - |string|
    - *None*
    - No
    - Has to be one of the available presets listed :ref:`here <bsdf-dielectric-list>`. See above for the reflective and refractive use case.
  * - thin
    - |bool|
    - |false|
    - No
    - |true| if the bsdf should be treated as a thin interface, which affects the refraction and subsurface behavior.
  * - flatness
    - |number|
    - :code:`0`
    - Yes
    - Amount of subsurface approximation. Only available if :paramtype:`thin` is |true|.
  * - specular_transmission
    - |number|
    - :code:`0`
    - Yes
    - Amount of specular transmission. Should be less or equal to 1 for physical correctness.
  * - specular_tint
    - |number|
    - :code:`0`
    - Yes
    - Mix factor of white and :paramtype:`base_color` for specular reflections.
  * - diffuse_transmission
    - |number|
    - :code:`0`
    - Yes
    - Amount of diffuse transmission. Should be less or equal to 1 for physical correctness. This is often named *translucency* in other applications.
  * - sheen
    - |number|
    - :code:`0`
    - Yes
    - Amount of soft velvet layer resulting into a soft reflection near the edges.
  * - sheen_tint
    - |number|
    - :code:`0`
    - Yes
    - Mix factor of white and :paramtype:`base_color` for sheen reflections.
  * - clearcoat
    - |number|
    - :code:`0`
    - Yes
    - Amount of white specular layer at the top.
  * - clearcoat_gloss
    - |number|
    - :code:`0`
    - Yes
    - Amount of shininess of the clearcoat layer.
  * - clearcoat_top_only
    - |bool|
    - |true|
    - No
    - |true| if clearcoat should only be applied to the front side of the surface only.
  * - clearcoat_roughness
    - |number|
    - :code:`0.1`
    - Yes
    - The isotropic microfacet roughness for clearcoat. Higher values result into a rougher appearance. The roughness computed via the GGX method.

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
    - The two bsdfs which should be blended.
  * - weight
    - |number|
    - :code:`0.5`
    - Yes
    - Amount of blend between the first and second bsdf.

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
    - The bsdf which should be masked out.
  * - weight
    - |number|
    - :code:`0.5`
    - Yes
    - Amount of masking. This is only useful if the weight is spatially varying, else it behaves like opacity.
  * - inverted
    - |bool|
    - |false|
    - No
    - |true| if the weight should be inverted.

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
    - The bsdf which should be cutoff.
  * - weight
    - |number|
    - :code:`0.5`
    - Yes
    - Weight factor for cutoff. This is only useful if the weight or :paramtype:`cutoff` is spatially varying.
  * - cutoff
    - |number|
    - :code:`0.5`
    - Yes
    - Threshold for cutoff. This is only useful if cutoff or :paramtype:`weight` is spatially varying.
  * - inverted
    - |bool|
    - |false|
    - No
    - |true| if the weight should be inverted. Keep in mind that :paramtype:`cutoff` is not inverted.

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
    - The bsdf the new normal will be forwarded to.
  * - map
    - |color|
    - :code:`1`
    - Yes
    - Usually a texture used for normal mapping.
  * - strength
    - |number|
    - :code:`1`
    - Yes
    - Interpolation factor between the old and new normal.

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
    - The bsdf the new normal will be forwarded to.
  * - map
    - |texture|
    - *None*
    - Yes
    - A grayscale texture used for texture mapping.
  * - strength
    - |number|
    - :code:`1`
    - Yes
    - Interpolation factor between the old and new normal.

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
    - Bsdf the normal transformation will be forwarded to.
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