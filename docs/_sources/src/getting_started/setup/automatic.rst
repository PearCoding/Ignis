.. _Automatic Setup:

Automatic Setup
===============

The recommended way to setup Ignis for development on any operating system.
This mini tutorial is expecting some basic knowledge about build systems and a recent PowerShell (also available on Linux).

1.  Clone Ignis from https://github.com/PearCoding/Ignis.

    1.  (Optional) Copy the config file ``scripts/setup/config.json`` and change it to your needs. Normally you do not have to change anything. The option ``LOCATION`` can be used to specify a different location for the dependencies.
    2.  Run the script ``scripts/setup/setup.ps1``. If you use a different config file, specify the path as an argument to the script. The script will download all the necessary dependencies, except CUDA, compile it and create a new directory ``deps/`` on the directory on top of Ignis or the location specified in the config. This step has to be done only once, or if some of the dependencies got updated.
    3.  By default the ``build/`` directory contains a Ninja based ``Release`` configuration. 
    4.  The ``Release`` configuration is well tested and should be used for most purposes. Any other configuration is experimental and may fail at any time.

.. NOTE:: It might be necessary to run the script ``scripts/setup/vc_dev_env.ps1`` every time a new terminal is opened to ensure the correct visual studio environment is available on Windows.

To change from the single build configuration using ```Ninja`` to a multiconfig-generator, remove or adapt the following line in ``scripts/setup/config.json``:

.. code-block:: json

    "CMAKE_EXTRA_ARGS": ["-GNinja", "-UCMAKE_CONFIGURATION_TYPES"]