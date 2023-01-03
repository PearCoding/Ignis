# Ignis
'Ignis' is a high-performance raytracer implemented using the Artic frontend of the AnyDSL compiler framework (https://anydsl.github.io/) and based on Rodent (https://github.com/AnyDSL/rodent). The renderer is usable on all three major platforms (Linux, Windows, MacOs).

![A scene containing diamonds rendered by Ignis with photonmapping](docs/screenshot.jpeg)

## Gallery

Some scenes rendered with Ignis. Acquired from https://benedikt-bitterli.me/resources/ and converted from Mitsuba to our own format. Both images took roughly one minute to render. With an RTX 2080 Super you can even have an interactive view of the scene.

![Bedroom scene by SlykDrako](docs/gallery1.jpeg)

![Livingroom scene by Jay-Artist](docs/gallery2.jpeg)

A sample scene from https://github.com/KhronosGroup/glTF-Sample-Models directly rendered within `igview`.

![DragonAttenuation scene by Stanford Scan and Morgan McGuire's Computer Graphics Archive](docs/gallery3.jpeg)

## Dependencies

 - AnyDSL <https://github.com/AnyDSL/anydsl>
 - Eigen3 <http://eigen.tuxfamily.org>
 - Intel® Threading Building Blocks https://www.threadingbuildingblocks.org/
 - ZLib <https://zlib.net/>

### Optional

 - SDL2 <https://www.libsdl.org/>

### Integrated
The following dependencies will be downloaded and compiled automatically.
Have a look at [CPM](https://github.com/cpm-cmake/CPM.cmake) for more information. 

 - bvh <https://github.com/madmann91/bvh>
 - Catch2 <https://github.com/catchorg/Catch2>
 - CLI11 <https://github.com/CLIUtils/CLI11>
 - imgui <https://github.com/ocornut/imgui>
 - imgui-markdown <https://github.com/juliettef/imgui_markdown>
 - implot <https://github.com/epezent/implot>
 - PExpr <https://github.com/PearCoding/PExpr>
 - pugixml <https://github.com/zeux/pugixml>
 - pybind11 <https://github.com/pybind/pybind11>
 - RapidJSON <https://rapidjson.org/>
 - stb <https://github.com/nothings/stb>
 - tinyexr <https://github.com/syoyo/tinyexr>
 - tinygltf <https://github.com/syoyo/tinygltf>
 - tinyobjloader <https://github.com/tinyobjloader/tinyobjloader>
 - tinyparser-mitsuba <https://github.com/PearCoding/TinyParser-Mitsuba>

## Docker Image

Ignis is available on docker hub with some preconfigured flavours! [pearcoding/ignis](https://hub.docker.com/repository/docker/pearcoding/ignis)

More information is available here [docker/README.md](docker/README.md)

## Building

Information about building Ignis is available in the documentation [online](https://pearcoding.github.io/Ignis/src/getting_started/building.html) or in the offline version of the documentation inside `docs/`

## Frontends

The frontends of the raytracer communicate with the user and the runtime.
Currently, four frontends are available:

 - `igview` This is the standard UI interface which displays the scene getting progressively rendered. This frontend is very good to get a first impression of the rendered scene and fly around to pick the one best camera position. Keep in mind that some power of your underlying hardware is used to render the UI and the tonemapping algorithms. Switching to the UI-less frontend `igcli` might be a good idea if no preview is necessary. Note, `igview` will be only available if the UI feature is enabled and SDL2 is available on your system. Disable this frontend by setting the CMake option `IG_WITH_VIEWER` to Off.
 - `igcli` The commandline only frontend is the same as `igview` but without any UI specific features and no interactive controls. In contrary to `igview`, `igcli` requires a maximum iteration or time budget to be specified by the user. Progressive rendering is not that useful without a preview. (We might add progressive rendering back, but I need a convincing argument for that...)
 - `igtrace` This commandline only frontend ignores camera specific information and expects a list of rays from the user. It returns the contribution back to the user for each ray initially specified.
 - `Python API` This simple python API allows to communicate with the runtime and allows you to work with the raytracer in interactive notebooks and more. The API is only available if Python3 was found in the system. You might disable the API by setting the CMake option `IG_WITH_PYTHON_API` to Off.

Use the `--help` argument on each of the executables to get information of possible arguments for each frontend.

## Running

Run a frontend of your choice like this:

    igview scene/diamond_scene.json


## Documentation

All available components are documented in the `docs/` folder. A documentation can be created with 

    cmake --build . -t ig_documentation

from the `build/` folder.

A quite recent version of the above documentation is available at: https://pearcoding.github.io/Ignis/ 

## Scene description

Ignis uses a JSON based flat scene description with instancing. Support for shading nodes is available via PExpr, image and procedural textures.
A schema is available at [docs/refs/ignis.schema.json](docs/refs/ignis.schema.json)

You might use the `mts2ig` to convert a Mitsuba scene description to our own format. Keep in mind that this feature is very experimental and not all BSDFs work out of the box.

You can also use `rad2json` to convert geometry used in the Radiance framework to our tool. Keep in mind that no BSDF and lights are mapped as the two raytracers are vastile different in these regards.

Ignis is able to understand glTF files. You can embed glTF files in Ignis's own scene description file or directly use the glTF file as an input to the multiple frontends.

A Blender plugin is available in `scripts/blender_exporter/`.

## Tiny tools

Two tiny tools `exr2hdr` and `hdr2exr` are available to convert between the Radiance favorite image format HDR to the advanced OpenEXR format and vice versa.

This is useful to ease the transfer from Radiance to our raytracer, but you can disable them by setting the CMake option `IG_WITH_TOOLS` to Off.

Actually, the tool might convert from any format the stb_image framework supports to the second format...

## How to use `igview`

The Ignis client has an optional UI and multiple ways to interact with the scene:

 - `1..9` number keys to switch between views.
 - `1..9` and `Strg/Ctrl` to save the current view on that slot.
 - `F1` to toggle the help window. 
 - `F2` to toggle the UI.
 - `F3` to toggle the interaction lock. If enabled, no view changing interaction is possible.
 - `F11` to save a snapshot of the current rendering. HDR information will be preserved. Use with `Strg/Ctrl` to make a LDR screenshot of the current render including UI and tonemapping. The image will be saved in the current working directory.
 - `R` to reset to initial view.
 - `P` to pause current rendering. Also implies an interaction lock.
 - `T` to toggle automatic tonemapping.
 - `G` to reset tonemapping properties. Only works if automatic tonemapping is disabled.
 - `F` to increase (or with `Shift` to decrease) tonemapping exposure. Step size can be decreased with `Strg/Ctrl`. Only works if automatic tonemapping is disabled.
 - `V` to increase (or with `Shift` to decrease) tonemapping offset. Step size can be decreased with `Strg/Ctrl`. Only works if automatic tonemapping is disabled.
 - `WASD` or arrow keys to travel through the scene.
 - `Q/E` to roll the camera around the viewing direction. 
 - `PageUp/PageDown` to pan the camera up and down. 
 - `Notepad +/-` to change the travel speed.
 - `Numpad 1` to switch to front view.
 - `Numpad 3` to switch to side view.
 - `Numpad 7` to switch to top view.
 - `Numpad 9` to look behind you.
 - `Numpad 2468` to rotate the camera.
 - Use with `Strg/Ctrl` to rotate the camera around the center of the scene. Use with `Alt` to enable first person camera behaviour. 

## Funding and Cooperation

The project is funded by the [Velux Stiftung](https://veluxstiftung.ch) and developed in cooperation with the [Computer Graphics](https://graphics.cg.uni-saarland.de/) chair of the [Saarland University](https://www.uni-saarland.de/en/home.html), [Fraunhofer Institute for Solar Energy Systems ISE](https://www.ise.fraunhofer.de/en.html) and [DFKI](https://www.dfki.de/en/web).

<a href="https://veluxstiftung.ch">
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/funding/PNG_VeluxStiftung_Logo_negative.png">
  <img alt="Velux Stiftung Logo" src="docs/funding/PNG_VeluxStiftung_Logo_positive.png" width="33%">
</picture>
</a>