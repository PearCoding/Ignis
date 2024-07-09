Linux & Mac OS
==============

.. WARNING:: It is recommended to use the new :ref:`automatic setup<automatic setup>` instead of the following one! The following may get outdated.

A quick and dirty tutorial to show how to setup AnyDSL and Ignis on a typical Linux OS or an Apple machine.
The following will need ``cmake`` and a working C++ development environment.

1.  Clone AnyDSL from https://github.com/AnyDSL/anydsl

    1.  Copy the ``config.sh.template`` to ``config.sh``.
    2.  Change ``: ${BUILD_TYPE:=Debug}`` to ``: ${BUILD_TYPE:=Release}`` inside ``config.sh``
    3.  Optional: Use ``ninja`` instead of ``make`` as it is superior in all regards.
    4.  Run ``./setup.sh`` inside a terminal at the root directory and get a coffee.

2.  Clone Ignis from https://github.com/PearCoding/Ignis

    1.  Make sure all the dependencies listed in `README.md <https://github.com/PearCoding/Ignis/blob/master/README.md>`_ are installed.
    2.  Run ``mkdir build && cd build && cmake -DAnyDSL_runtime_DIR="ANYDSL_ROOT/runtime/build/share/anydsl/cmake" ..``, with ``ANYDSL_ROOT`` being the path to the root of the AnyDSL framework.
    3.  Run ``cmake --build .`` inside the ``build/`` directory and get your second coffee.

3.  Run ``./bin/igview ../scenes/diamond_scene.json`` inside the ``build/`` directory to see if your setup works. You should be able to see three diamonds lit by an area light.

.. NOTE:: MacOS CPU vectorization and GPU support is still very experimental and limited. 