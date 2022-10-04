Overview
========

This section gives an overview of the Ignis rendering framework.

Frontends
---------

The frontends of the raytracer communicate with the underlying runtime.
Currently, four frontends are available:

``igview``
^^^^^^^^^^
     
This is the standard UI interface which displays the scene getting progressively rendered.

This frontend is very good to get a first impression of the rendered scene and fly around to pick the one best camera position.
Keep in mind that some power of your underlying hardware is used to render the UI and the tonemapping algorithms.
Switching to the UI-less frontend ``igcli`` might be a good idea if no preview is necessary.

Note, ``igview`` will be only available if the UI feature is enabled and SDL2 is available on your system.
Disable this frontend by setting the CMake option ``IG_WITH_VIEWER`` to ``Off``.
 
``igcli`` 
^^^^^^^^^

The commandline only frontend is the same as ``igview`` but without any UI specific features and no interactive controls.

In contrary to ``igview``, ``igcli`` requires a maximum iteration or time budget to be specified by the user.
Progressive rendering is not that useful without a preview.
(We might add progressive rendering back, but I need a convincing argument for that...)
 
``igtrace``
^^^^^^^^^^^
   
This commandline only frontend ignores camera specific information and expects a list of rays from the user.
It returns the contribution back to the user for each ray initially specified.
 
Python API
^^^^^^^^^^
   
This simple python API allows to communicate with the runtime and allows you to work with the raytracer in interactive notebooks and more.

The API is only available if Python3 was found in the system.
You might disable the API by setting the CMake option ``IG_WITH_PYTHON_API`` to ``Off``.

More about the Python API can be found at the following section :doc:`../python/overview`.

Running
-------

Run a frontend of your choice like this:

.. code-block:: bash

    igview scene/diamond_scene.json

Tiny tools
----------

Two tiny tools ``exr2hdr`` and ``hdr2exr`` are available to convert between the Radiance favorite image format HDR to the advanced OpenEXR format and vice versa.

This is useful to ease the transfer from Radiance to our raytracer, but you can disable them by setting the CMake option ``IG_WITH_TOOLS`` to ``Off``.

Actually, the tool might convert from any format the ``stb_image`` framework supports to the second format.