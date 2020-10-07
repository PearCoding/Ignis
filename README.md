# Ignis

Ignis is a raytracer for the RENEGADE project implemented using the Artic frontend of the AnyDSL compiler framework (https://anydsl.github.io/) and based on Rodent (https://github.com/AnyDSL/rodent).

## Dependencies

 - AnyDSL <https://github.com/AnyDSL/anydsl>
 - Eigen3 <http://eigen.tuxfamily.org>
 - OpenImageIO <https://sites.google.com/site/openimageio/home>
 - LZ4 <https://github.com/lz4/lz4>
 - TinyParser-Mitsuba <https://github.com/PearCoding/TinyParser-Mitsuba>
 - ZLib <https://zlib.net/>

### Optional

 - SDL2 <https://www.libsdl.org/>
 - imgui <https://github.com/ocornut/imgui>

## Building

Once the dependencies are installed, first create a directory to build the application in:

    mkdir build
    cd build

Use your favorite generator (e.g. Ninja) and set the scene to be build using SCENE_FILE

    cmake -G Ninja .. -DSCENE_FILE=myfile.xml
    ninja

