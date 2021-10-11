Evalglare Tutorial
==================

A quick and dirty tutorial to run a simple evalglare analysis.

 1. Make sure you have a running setup of Ignis. Have a look at :doc:`../compiling` to compile the Ignis framework.  
 2. Build your own scene using one of the available tools.
 
   1. We will use the ``scene/diamond_scene.json`` example. Keep in mind that this is a horrible scene for evalglare...
 
 3. Make sure you are in the Ignis ``build/`` directory and run ``./bin/igview SCENE_FILE --camera fishlens`` to check out your scene. The ``SCENE_FILE`` parameter is the path to your preferred scene file.
 4. Look around and find a good spot. In the best case, it is an interesting spot for the analysis of the glare probability.
 
   1. Note down the ``Cam Eye``, ``Cam Dir`` and ``Cam Up`` somewhere. These observables are located on the ``Stats`` panel.
   2. You can create a new view file ``output.vf`` based on the following template ``rvu -vth -vp Px Py Pz -vd Dx Dy Dz -vu Ux Uy Uz -vh 180 -vv 180 -vo 0 -va 0 -vs 0 -vl 0`` with P, D and U being the previously noted coordinates.
 
 5. Run ``./bin/igcli SCENE_FILE --spp SPP --camera fishlens -o output.exr``. Setting SPP to a high value will make the image less noisy, but will also take longer to finish. The higher the SPP the greater the coffee you may consume. For example, ``./bin/igcli ../scenes/diamond_scene.json --spp 512 --camera fishlens -o output.exr``
 6. Run ``./bin/exr2hdr output.exr`` to convert the modern OpenEXR file to Radiance hdr file. This will create a new ``output.hdr`` file.
 7. Finally, run ``evalglare -vf output.vf -d``.
 
   1. This will output the DGP and many other measures. Have a look at the ``evalglare`` manual to understand them. 