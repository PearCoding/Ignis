# Ignis Evalglare Tutorial

A quick and dirty tutorial to run a simple evalglare analyses.

 1. Make sure you have a running setup of Ignis. Have a look at [docs/TutorialSetup.md](TutorialSetup.md) to compile the Ignis framework. 
    1. Make sure the fishlens camera is available. It is enabled by default.
 2. Build your own scene using one of the available tools.
    1. We will use the `scene/diamond_scene.json` example. Keep in mind that this is a horrible scene for evalglare...
 3. Make sure you are in the Ignis `build/` directory and run `./bin/igview SCENE_FILE --camera fishlens` to check out your scene. The `SCENE_FILE` parameter is a path to your preferred scene file.
 4. Look around and find a good spot, which would make sense to analyse the glare probability.
    1. Note down the `Cam Eye`, `Cam Dir` and `Cam Up` somewhere, which you can find on the `Stats` panel.
    2. You can create a new view file `output.vf` based on the following template `rvu -vth -vp Px Py Pz -vd Dx Dy Dz -vu Ux Uy Uz -vh 180 -vv 180 -vo 0 -va 0 -vs 0 -vl 0` with P, D and U being the previously noted coordinates.
 5. Run `./bin/igcli SCENE_FILE --spp SPP --camera fishlens -o output.exr`. Setting SPP to a high value will make the image less noisy, but will also take longer to finish. The higher the SPP the greater the coffee you may consume. For example, `./bin/igcli ../scenes/diamond_scene.json --spp 512 --camera fishlens -o output.exr`
 6. Run `./bin/exr2hdr output.exr` to convert the modern OpenEXR file to radiance hdr file. This will create a new `output.hdr` file.
 7. Finally, run `evalglare -vf output.vf -d`.
    1. This will output the DGP and many other measures. Have a look at `evalglare` manual to understand them. 