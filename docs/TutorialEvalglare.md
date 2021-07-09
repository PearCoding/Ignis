# Ignis Evalglare Tutorial

A quick and dirty tutorial to setup AnyDSL & Ignis and run a simple evalglare analyses.

 1. Get AnyDSL from https://github.com/AnyDSL/anydsl
    1. Copy the `config.sh.template` to `config.sh`.
    2. Change `: ${BUILD_TYPE:=Debug}` to `: ${BUILD_TYPE:=Release}` inside `config.sh`
    3. Optional: Use `ninja` instead of `make` as it is superior in any regard.
    4. Run `./setup.sh` and get a coffee.
 2. Get Ignis from https://github.com/PearCoding/Ignis
    1. Make sure the dependencies are available. See README.md for a list of necessary dependencies.
    2. Run `mkdir build && cd build && cmake ..`
    3. If you want to change the drivers to build, use `ccmake`. Regarding cameras, only the fishlens are necessary for this tutorial.
    4. Run `cmake --build` inside the `build/` directory and get your second coffee.
 3. Build your own scene using one of the available tools.
    1. We will use the `scene/diamond_scene.json` example. Keep in mind that this is a horrible scene for evalglare...
 4. Make sure you are still in the Ignis `build` directory and run `./bin/igview SCENE_FILE --camera fishlens` to check out your scene. The `SCENE_FILE` parameter is a path to your preferred scene file.
 5. Look around and find a good spot, which would make sense to analyse the glare probability.
    1. Note down the `Cam Eye`, `Cam Dir` and `Cam Up` somewhere, which you can find on the `Stats` panel.
    2. You can create a new view file `output.vf` based on the following template `rvu -vth -vp Px Py Pz -vd Dx Dy Dz -vu Ux Uy Uz -vh 180 -vv 180 -vo 0 -va 0 -vs 0 -vl 0` with P, D and U being the previously noted coordinates.
 6. Run `./bin/igcli SCENE_FILE --spp SPP --camera fishlens -o output.exr`. Setting SPP to a high value will make the image less noisy, but will also take longer to finish. The higher the SPP the greater the coffee you may consume. For example, `./bin/igcli ../scenes/diamond_scene.json --spp 512 --camera fishlens -o output.exr`
 7. Run `./bin/exr2hdr output.exr` to convert the modern OpenEXR file to radiance hdr file. This will create a new `output.hdr` file.
 8. Finally, run `evalglare -vf output.vf -d`.
    1. This will output the DGP and many other measures. Have a look at `evalglare` manual to understand them. 