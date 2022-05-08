Lights
======

Several light and sky models are contained inside the Ignis rendering framework.
Most number and color parameters can be connected to a shading network or texture via :ref:`PExpr <PExpr>`.

A light is specified in the :monosp:`lights` block with a :monosp:`name` and a :monosp:`type`.
The type has to be one of the lights listed at this section below.

.. code-block:: javascript
    
    {
        // ...
        "lights": [
            // ...
            {"name":"NAME", "type":"TYPE", /* DEPENDS ON TYPE */},
            // ...
        ]
        // ...
    }

Point Light (:monosp:`point`)
---------------------------------------------

.. objectparameters::

 * - position
   - |vector|
   - (0,0,0)
   - Position of the point light.
   
 * - intensity
   - |color|
   - (1,1,1)
   - Intensity of the point light.

Spot Light (:monosp:`spot`)
---------------------------------------------

.. objectparameters::

 * - cutoff
   - |number|
   - 30
   - Cutoff angle in degree. Greater angles will be completly black.
 * - falloff
   - |number|
   - 20
   - Falloff angle in degree. Greater angles will linearly falloff towards the cutoff angle. Falloff angle should be less or equal to the cutoff angle.
 * - position
   - |vector|
   - (0,0,0)
   - Position of the light.
 * - direction
   - |vector|
   - (0,0,1)
   - Direction of the light towards the scene.
 * - theta, phi
   - |number|
   - 0, 0
   - Instead of :monosp:`direction` theta and phi given in degrees can be used.
 * - elevation, azimuth
   - |number|
   - 0, 0
   - Instead of :monosp:`direction` the elevation and azimuth of a celestial object given in degrees can be used.
 * - year, month, day, hour, minute, seconds, latitude, longitude, timezone
   - |number|
   - 2020, 5, 6, 12, 0, 0, 6.9965744, 49.235422, 2
   - Instead of :monosp:`direction` the time and location can be used. This will give the approximated direction from the sun.
 * - intensity
   - |color|
   - (1,1,1)
   - Intensity of the light.
   
Directional Light (:monosp:`directional`)
---------------------------------------------

.. objectparameters::

 * - direction
   - |vector|
   - (0,0,1)
   - Direction of the light towards the scene.
   
 * - theta, phi
   - |number|
   - 0, 0
   - Instead of :monosp:`direction` theta and phi given in degrees can be used.
   
 * - elevation, azimuth
   - |number|
   - 0, 0
   - Instead of :monosp:`direction` the elevation and azimuth of a celestial object given in degrees can be used.
   
 * - year, month, day, hour, minute, seconds, latitude, longitude, timezone
   - |number|
   - 2020, 5, 6, 12, 0, 0, 6.9965744, 49.235422, 2
   - Instead of :monosp:`direction` the time and location can be used. This will give the approximated direction from the sun.

 * - irradiance
   - |color|
   - (1,1,1)
   - Output of the directional light.

Area Light (:monosp:`area`)
---------------------------------------------

.. objectparameters::

 * - entity
   - |entity|
   - *None*
   - A valid entity.

 * - radiance
   - |color|
   - (1,1,1)
   - Output of the area light.
   
Sun Light (:monosp:`sun`)
---------------------------------------------

.. objectparameters::

 * - direction
   - |vector|
   - (0,0,1)
   - Direction of the incoming sun towards the scene.
   
 * - theta, phi
   - |number|
   - 0, 0
   - Instead of :monosp:`direction` theta and phi given in degrees can be used.
   
 * - elevation, azimuth
   - |number|
   - 0, 0
   - Instead of :monosp:`direction` the elevation and azimuth of a celestial object given in degrees can be used.
   
 * - year, month, day, hour, minute, seconds, latitude, longitude, timezone
   - |number|
   - 2020, 5, 6, 12, 0, 0, 6.9965744, 49.235422, 2
   - Instead of :monosp:`direction` the time and location can be used. This will give the approximated direction from the sun.

 * - sun_scale
   - |number|
   - 1
   - Scale of the sun power in the sky.
   
 * - sun_radius_scale
   - |number|
   - 1
   - Scale of the sun radius in the sky.

Sky Light (:monosp:`sky`)
---------------------------------------------

.. objectparameters::

 * - ground
   - |color|
   - (1,1,1)
   - Ground color of the sky model.
 * - turbidity
   - |number|
   - 3
   - Turbidity factor of the sky model.
 * - direction
   - |vector|
   - (0,0,1)
   - Direction of the incoming sun towards the scene.
 * - theta, phi
   - |number|
   - 0, 0
   - Instead of :monosp:`direction` theta and phi given in degrees can be used. 
 * - elevation, azimuth
   - |number|
   - 0, 0
   - Instead of :monosp:`direction` the elevation and azimuth of a celestial object given in degrees can be used.
 * - year, month, day, hour, minute, seconds, latitude, longitude, timezone
   - |number|
   - 2020, 5, 6, 12, 0, 0, 6.9965744, 49.235422, 2
   - Instead of :monosp:`direction` the time and location can be used. This will give the approximated direction from the sun.
 * - scale
   - |color|
   - (1,1,1)
   - Scale factor multiplied to the radiance.
   
This sky model is based on the paper "An Analytic Model for Full Spectral Sky-Dome Radiance"
and the 2013 IEEE CG&A paper "Adding a Solar Radiance Function to the Hosek Skylight Model" both by 
Lukas Hosek and Alexander Wilkie, both Charles University in Prague, Czech Republic at that time.

CIE Uniform Sky Model (:monosp:`cie_uniform`)
---------------------------------------------

.. objectparameters::

 * - zenith
   - |color|
   - (1,1,1)
   - Zenith color of the sky model.

 * - ground
   - |color|
   - (1,1,1)
   - Ground color of the sky model.

 * - ground_brightness
   - |number|
   - 0.2
   - Brightness of the ground.
   
CIE Cloudy Sky Model (:monosp:`cie_cloudy`)
---------------------------------------------

.. objectparameters::

 * - zenith
   - |color|
   - (1,1,1)
   - Zenith color of the sky model.

 * - ground
   - |color|
   - (1,1,1)
   - Ground color of the sky model.

 * - ground_brightness
   - |number|
   - 0.2
   - Brightness of the ground.
   
Perez Sky Model (:monosp:`perez`)
---------------------------------------------

.. objectparameters::

 * - direction
   - |vector|
   - (0,0,1)
   - Direction of the light towards the scene.
   
 * - theta, phi
   - |number|
   - 0, 0
   - Instead of :monosp:`direction` theta and phi given in degrees can be used.
   
 * - elevation, azimuth
   - |number|
   - 0, 0
   - Instead of :monosp:`direction` the elevation and azimuth of a celestial object given in degrees can be used.
   
 * - year, month, day, hour, minute, seconds, latitude, longitude, timezone
   - |number|
   - 2020, 5, 6, 12, 0, 0, 6.9965744, 49.235422, 2
   - Instead of :monosp:`direction` the time and location can be used. This will give the approximated direction from the sun.

 * - luminance
   - |color|
   - (1,1,1)
   - Luminance of the sky model.

 * - zenith
   - |color|
   - (1,1,1)
   - Zenith color of the sky model. This can not be used together with :monosp:`luminance`.

 * - a, b, c, d, e
   - |number|
   - 1,1,1,1,1
   - Perez specific parameters.
   
Environment Light (:monosp:`env`)
---------------------------------------------

.. objectparameters::

 * - radiance
   - |color|
   - (1,1,1)
   - Radiance of the sky model. This can also be a texture.
 * - scale
   - |color|
   - (1,1,1)
   - Scale factor multiplied to the radiance. Only really useful in combination with a texture.
 * - cdf
   - |bool|
   - true
   - Construct a 2d cdf for sampling purposes. Will only be considered if parameter `radiance` is an :monosp:`image` texture (without PExpr and other terms)
