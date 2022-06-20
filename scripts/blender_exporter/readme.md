# Ignis blender exporter plugin

The ignis blender exporter enables users to export basic scenes from blender in a json format compatible with ignis.
The basic scene supported by the plugin include: objects, camera (one), lights (point light, spot light, directional light, area light, environmental light for both RGB and Texture), materials (no nodes).

To build the plugin simply run build_blender then add the zip file to blender.

The plugin see_blender from the project [SeeSharp](https://github.com/pgrit/SeeSharp) by Pascal Grittmann was used as a base for the code.