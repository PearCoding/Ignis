# Ignis Blender Plugin

The Ignis Blender exporter enables users to export scenes from Blender in a json format compatible with Ignis.
The scene supported by the plugin include: 
 - Objects (reduced to triangle meshes)
 - Camera (perspective and orthogonal, only the active camera will be exported)
 - Lights (point light, spot light, directional light, area light, background)
 - Materials with extensive but not complete node support

To build the plugin simply run build_blender and add the generated zip file to Blender.

The plugin ´see_blender´ from the [SeeSharp](https://github.com/pgrit/SeeSharp) project by Pascal Grittmann was used as a initial base for the plugin.
Major part of the material mapping and node support is from the [PearRay](https://github.com/PearCoding/PearRay) [Blender plugin](https://github.com/PearCoding/PearRay-Blender).