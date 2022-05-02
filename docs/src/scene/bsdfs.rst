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

Smooth diffuse (:monosp:`diffuse`)
----------------------------------

.. objectparameters::

 * - reflectance
   - |color|
   - 0.5
   - Albedo

.. subfigstart::
  
.. figure:: images/mat_diffuse.jpg
  :width: 90%

  Diffuse

.. subfigend::
  :width: 0.6
  :label: fig-diffuse

.. _bsdf-roughdiffuse:

Rough diffuse (:monosp:`roughdiffuse`)
--------------------------------------

.. objectparameters::

 * - reflectance
   - |color|
   - 0.5
   - Albedo

 * - alpha
   - |number|
   - 0
   - Isotropic roughness.

.. subfigstart::

.. figure::  images/mat_rough_diffuse.jpg
  :width: 90%
  :align: center
  
  Rough diffuse, also called Oren-Nayar 

.. subfigend::
  :width: 0.6
  :label: fig-rough-diffuse

.. _bsdf-dielectric:

Smooth dielectric (:monosp:`dielectric`)
----------------------------------------

.. objectparameters::

 * - specular_reflectance
   - |color|
   - 1.0
   - TODO
 * - specular_transmittance
   - |color|
   - 1.0
   - TODO
 * - ext_ior, int_ior
   - |number|
   - 1.00027, 1.55
   - TODO
 * - thin
   - |bool|
   - false
   - Thin


.. subfigstart::

.. figure::  images/mat_dielectric.jpg
  :width: 90%
  :align: center
  
  Dielectric

.. figure::  images/mat_thindielectric.jpg
  :width: 90%
  :align: center
  
  Dielectric with thin approximation 

.. subfigend::
  :width: 0.30
  :label: fig-dielectric 

.. _bsdf-roughdielectric:

Rough dielectric (:monosp:`roughdielectric`)
--------------------------------------------

.. objectparameters::

 * - specular_reflectance
   - |color|
   - 1.0
   - TODO
 * - specular_transmittance
   - |color|
   - 1.0
   - TODO
 * - ext_ior, int_ior
   - |number|
   - 1.00027, 1.55
   - TODO
 * - alpha_u, alpha_v
   - |number|
   - 0.1, 0.1
   - Roughness terms. Can be specified isotropically with just `alpha` as well. 

.. subfigstart::

.. figure::  images/mat_roughdielectric.jpg
  :width: 90%
  :align: center
  
  Rough dielectric

.. subfigend::
  :width: 0.6
  :label: fig-rough-dielectric

.. _bsdf-mirror:

Perfect smooth mirror (:monosp:`mirror`)
----------------------------------------

.. objectparameters::

 * - specular_reflectance
   - |texture|
   - 1.0
   - TODO

.. subfigstart::

.. figure::  images/mat_mirror.jpg
  :width: 90%
  :align: center
  
  Mirror, or a perfect conductor

.. subfigend::
  :width: 0.6
  :label: fig-mirror

.. _bsdf-conductor:

Smooth conductor (:monosp:`conductor`)
--------------------------------------

.. objectparameters::

 * - eta, k
   - |number|
   - none
   - Real and imaginary components of the material's index of refraction.
 * - specular_reflectance
   - |color|
   - 1.0
   - Optional factor that can be used to modulate the specular reflection component.
     Note that for physical realism, this parameter should never be touched. 

.. subfigstart::

.. figure::  images/mat_conductor.jpg
  :width: 90%
  :align: center
  
  Conductor

.. subfigend::
  :width: 0.6
  :label: fig-conductor

.. _bsdf-roughconductor:

Rough conductor (:monosp:`roughconductor`)
------------------------------------------

.. objectparameters::

 * - eta, k
   - |number|
   - none
   - Real and imaginary components of the material's index of refraction.
 * - specular_reflectance
   - |color|
   - 1.0
   - Optional factor that can be used to modulate the specular reflection component.
     Note that for physical realism, this parameter should never be touched. 
 * - alpha_u, alpha_v
   - |number|
   - 0.1, 0.1
   - Roughness terms. Can be specified isotropically with just `alpha` as well.
   
.. subfigstart::

.. figure::  images/mat_rough_conductor.jpg
  :width: 90%
  :align: center
  
  Rough conductor

.. subfigend::
  :width: 0.6
  :label: fig-rough-conductor

.. _bsdf-plastic:

Smooth plastic (:monosp:`plastic`)
----------------------------------

.. objectparameters::

 * - specular_reflectance
   - |color|
   - 1.0
   - TODO
 * - diffuse_reflectance
   - |color|
   - 0.5
   - TODO
 * - ext_ior, int_ior
   - |number|
   - 1.00027, 1.55
   - TODO

.. subfigstart::

.. figure::  images/mat_plastic.jpg
  :width: 90%
  :align: center
  
  Plastic

.. subfigend::
  :width: 0.6
  :label: fig-plastic

.. _bsdf-roughplastic:

Rough plastic (:monosp:`roughplastic`)
--------------------------------------

.. objectparameters::

 * - specular_reflectance
   - |color|
   - 1.0
   - TODO
 * - diffuse_reflectance
   - |color|
   - 0.5
   - TODO
 * - ext_ior, int_ior
   - |number|
   - 1.00027, 1.55
   - TODO
 * - alpha_u, alpha_v
   - |number|
   - 0.1, 0.1
   - Roughness terms. Can be specified isotropically with just `alpha` as well. 

.. subfigstart::

.. figure::  images/mat_rough_plastic.jpg
  :width: 90%
  :align: center
  
  Rough plastic

.. subfigend::
  :width: 0.6
  :label: fig-rough-plastic

.. _bsdf-phong:

Phong (:monosp:`phong`)
-----------------------

.. objectparameters::

 * - specular_reflectance
   - |color|
   - 1.0
   - TODO
 * - exponent
   - |number|
   - 30
   - TODO

.. subfigstart::

.. figure::  images/mat_phong.jpg
  :width: 90%
  :align: center
  
  Phong

.. subfigend::
  :width: 0.6
  :label: fig-phong

.. NOTE:: We do not recommend to use this bsdf for new projects as it disregards some PBR principles

.. _bsdf-principled:

Disney *Principled* (:monosp:`principled`)
------------------------------------------

.. objectparameters::

 * - base_color
   - |color|
   - 0.8
   - TODO
 * - metallic
   - |number|
   - 0.0
   - TODO
 * - roughness
   - |number|
   - 0.5
   - TODO
 * - anisotropic
   - |number|
   - 0.0
   - TODO
 * - ior
   - |number|
   - 1.55
   - TODO
 * - thin
   - |bool|
   - false
   - TODO
 * - flatness
   - |number|
   - 0.0
   - TODO
 * - specular_transmission
   - |number|
   - 0.0
   - TODO
 * - specular_tint
   - |number|
   - 0.0
   - TODO
 * - sheen
   - |number|
   - 0.0
   - TODO
 * - sheen_tint
   - |number|
   - 0.0
   - TODO
 * - clearcoat
   - |number|
   - 0.0
   - TODO
 * - clearcoat_gloss
   - |number|
   - 0.0
   - TODO
 * - clearcoat_roughness
   - |number|
   - 0.1
   - TODO
 * - diffuse_transmission
   - |number|
   - 0.0
   - TODO

.. subfigstart::

.. figure::  images/mat_disney.jpg
  :width: 90%
  :align: center
  
  Disney *Principled* 

.. subfigend::
  :width: 0.6
  :label: fig-principled

.. _bsdf-blend:

Blend (:monosp:`blend`)
-----------------------

.. objectparameters::

 * - first, second
   - |bsdf|
   - None
   - TODO
 * - weight
   - |number|
   - 0.5
   - TODO

.. subfigstart::

.. figure::  images/mat_blend.jpg
  :width: 90%
  :align: center
  
  Blend of two bsdfs. One dielectric, one diffuse

.. subfigend::
  :width: 0.6
  :label: fig-blend

.. _bsdf-mask:

Mask (:monosp:`mask`)
---------------------

.. objectparameters::

 * - bsdf
   - |bsdf|
   - None
   - TODO
 * - weight
   - |number|
   - 0.5
   - TODO
 * - inverted
   - |bool|
   - false
   - TODO

.. subfigstart::

.. figure::  images/mat_mask.jpg
  :width: 90%
  :align: center
  
  Mask with a uniform weight

.. subfigend::
  :width: 0.6
  :label: fig-mask

.. _bsdf-cutoff:

Cutoff (:monosp:`cutoff`)
-------------------------

.. objectparameters::

 * - bsdf
   - |bsdf|
   - None
   - TODO
 * - weight
   - |number|
   - 0.5
   - TODO
 * - cutoff
   - |number|
   - 0.5
   - TODO
 * - inverted
   - |bool|
   - false
   - TODO

.. subfigstart::

.. figure::  images/mat_cutoff.jpg
  :width: 90%
  :align: center
  
  Cutoff with a uniform weight

.. subfigend::
  :width: 0.6
  :label: fig-cutoff

.. _bsdf-passthrough:

Passthrough (:monosp:`passthrough`)
-----------------------------------

.. subfigstart::

.. figure::  images/mat_passthrough.jpg
  :width: 90%
  :align: center
  
  Passthrough

.. subfigend::
  :width: 0.6
  :label: fig-passthrough

.. NOTE:: The :monosp:`passthrough` bsdf should be used carefully, as simple techniques like Next-Event Estimation still see the object geometry.

.. _bsdf-normalmap:

Normal mapping (:monosp:`normalmap`)
------------------------------------

.. objectparameters::

 * - bsdf
   - |bsdf|
   - None
   - TODO
 * - map
   - |color|
   - 1
   - Usually a texture used for normal mapping.
 * - strength
   - |number|
   - 1
   - TODO

.. subfigstart::

.. figure::  images/mat_normalmap.jpg
  :width: 90%
  :align: center
  
  Normal mapping

.. subfigend::
  :width: 0.6
  :label: fig-normalmap

.. _bsdf-bumpmap:

Bump mapping (:monosp:`bumpmap`)
--------------------------------

.. objectparameters::

 * - bsdf
   - |bsdf|
   - None
   - TODO
 * - map
   - |texture|
   - None
   - A grayscale texture used for texture mapping.
 * - strength
   - |number|
   - 1
   - TODO

.. subfigstart::

.. figure::  images/mat_bumpmap.jpg
  :width: 90%
  :align: center
  
  Bump mapping

.. subfigend::
  :width: 0.6
  :label: fig-bumpmap

.. _bsdf-klems:

Klems (:monosp:`klems`)
-----------------------

.. objectparameters::

 * - filename
   - |string|
   - None
   - Path to a valid windows xml specifying a klems bsdf.
 * - base_color
   - |color|
   - 1
   - Tint.

.. NOTE:: The :monosp:`klems` bsdf is still experimental.

.. _bsdf-tensortree:

TensorTree (:monosp:`tensortree`)
---------------------------------

.. objectparameters::

 * - filename
   - |string|
   - None
   - Path to a valid windows xml specifying a tensortree bsdf.
 * - base_color
   - |color|
   - 1
   - Tint.

.. NOTE:: The :monosp:`tensortree` bsdf is still experimental.
