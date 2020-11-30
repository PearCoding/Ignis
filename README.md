# Ignis Rodent

'Ignis Rodent' is a raytracer for the RENEGADE project implemented using the Artic frontend of the AnyDSL compiler framework (https://anydsl.github.io/) and based on Rodent (https://github.com/AnyDSL/rodent).

## Dependencies

 - AnyDSL <https://github.com/AnyDSL/anydsl>
 - Eigen3 <http://eigen.tuxfamily.org>
 - OpenImageIO <https://sites.google.com/site/openimageio/home>
 - LZ4 <https://github.com/lz4/lz4>
 - RapidJSON <https://rapidjson.org/>
 - ZLib <https://zlib.net/>

### Optional

 - SDL2 <https://www.libsdl.org/>
 - imgui <https://github.com/ocornut/imgui>

## Building

Once the dependencies are installed, first create a directory to build the application in:

    mkdir build
    cd build

Use your favorite generator (e.g. `Ninja`) and set the scene to be build using `IG_SCENE_FILE`

    cmake -G Ninja .. -DIG_SCENE_FILE=../scenes/diamond_scene.json
    cmake --build .

If `Ninja` is not available you may skip the `-G Ninja` parameter.

## Scene description

Ignis Rodent uses a JSON based flat scene description with instancing. Support for shading nodes is planned. Image and procedural texture support is available.
A schema is available at [refs/ignis.schema.json](refs/ignis.schema.json)

## How to use

The Ignis Rodent client has an optional UI and multiple ways to interact with the scene:

 - `1..9` number keys to switch between views.
 - `1..9` and `Strg/Ctrl` to save the current view on that slot.
 - `F2` to toggle the UI.
 - `F3` to toggle the interaction lock. If enabled, no view changing interaction is possible.
 - `F11` to save a screenshot. The image will be saved in the current working directory.
 - `t` to toggle automatic tonemapping.
 - `g` to reset tonemapping properties. Only works if automatic tonemapping is disabled.
 - `f` to increase (or with `Shift` to decrease) tonemapping exposure. Step size can be decreased with `Strg/Ctrl`. Only works if automatic tonemapping is disabled.
 - `v` to increase (or with `Shift` to decrease) tonemapping offset. Step size can be decreased with `Strg/Ctrl`. Only works if automatic tonemapping is disabled.
 - `WASD` or arrow keys to travel through the scene.
 - `Q/E` to rotate the camera around the viewing direction. 
 - `PageUp/PageDown` to pan the camera up and down. 
 - `Notepad +/-` to change the travel speed.
 - `Numpad 1` to switch to front view.
 - `Numpad 3` to switch to side view.
 - `Numpad 7` to switch to top view.
 - `Numpad 9` look behind you.
 - `Numpad 2468` to rotate the camera.
 - Mouse to rotate the camera.
