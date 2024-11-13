Evalglare Tutorial
==================

A quick and dirty tutorial to run a simple evalglare analysis.

1.  Make sure you have a running setup of Ignis. Have a look at :doc:`../setup` to understand how to compile the Ignis framework.  
2.  Build your own scene using one of the available tools.

    1.  We will use the ``scene/radiance/office/office_scene.json`` example.

3.  Apply ``source source.sh`` or run ``source.bat`` or ``source.ps1`` depending on your operating system to make sure the tools are available in the current shell environment.
4.  Run ``igview SCENE_FILE --camera fishlens`` to check out your scene. The ``SCENE_FILE`` parameter is the path to your preferred scene file.
5.  Look around and find a good spot. In the best case, it is an interesting spot for the analysis of the glare probability.

    1.  Note down the ``Cam Eye``, ``Cam Dir`` and ``Cam Up`` somewhere. These observables are located on the ``Stats`` panel.
    2.  You can create a new view file ``output.vf`` based on the following template ``rvu -vta -vp Px Py Pz -vd Dx Dy Dz -vu Ux Uy Uz -vh 180 -vv 180 -vo 0 -va 0 -vs 0 -vl 0`` with P, D and U being the previously noted coordinates.

6.  Run ``igcli SCENE_FILE --spp SPP --camera fishlens -o output.exr``. Setting SPP to a high value will make the image less noisy, but will also take longer to finish. The higher the SPP the greater the coffee you may consume. For example, ``igcli scene/radiance/office/office_scene.json --spp 512 --camera fishlens -o output.exr``
7.  Run ``igutil convert output.exr output.hdr`` to convert the modern OpenEXR file to Radiance hdr file. This will create a new ``output.hdr`` file.
8.  Finally, run ``evalglare -vf output.vf -d`` from the Radiance tools.

    1.  This will output the DGP and many other measures. Have a look at the ``evalglare`` manual to understand them. 

.. note:: You can use the python script scripts/ExtractRadView.py to generate a radiance view file for you directly from the settings inside the scene description.