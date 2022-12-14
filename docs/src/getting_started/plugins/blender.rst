Blender Plugin
==============

The Ignis framework has an official integration plugin for the open source 3d content creation software `Blender <https://www.blender.org/>`_.

Installation
------------

The plugin can be created running 

   scripts/blender_exporter/build_blender.sh

or 

   scripts/blender_exporter/build_blender.bat

depending on your operating system. This will create a zip file ``ignis_blender.zip`` in ``scripts/blender_exporter``. Use the zip file to install the plugin via Blender as shown in the following two images:

.. subfigstart::
  
.. subfigure:: images/screenshot_blender_addon_1.jpg
   :width: 0.45
   :caption: Go to install

.. subfigure:: images/screenshot_blender_addon_2.jpg
   :width: 0.45
   :caption: Select the ``ignis_blender.zip`` zip file

.. subfigend::
  :label: fig-blender_install

Configuration
-------------

.. subfigstart::
.. subfigure:: images/screenshot_blender_addon_3.jpg
   :caption: Set the API directory to the correct path
.. subfigend::
  :label: fig-blender_config

Usage
-----

The Blender plugin can be used to export the scene into a Ignis compatible scene description or, when used as the active render engine, render the scene directly in Blender.
The current state supports most Blender shading nodes.

.. NOTE:: Support for curves is planned, but currently not available.
