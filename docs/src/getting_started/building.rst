Building
========

The active development is done on a linux operating system (Ubuntu in particular).
The raytracer is platform independent to a large extend and works on Linux, Windows and Mac OS operating systems.

Linux & Mac OS
--------------

A quick and dirty tutorial to show how to setup AnyDSL and Ignis on a typical Linux OS.
The following will need ``ccmake``, but works also with ``cmake-gui`` or with plain ``cmake`` as well.

1.  Clone AnyDSL from https://github.com/AnyDSL/anydsl

    1.  Copy the ``config.sh.template`` to ``config.sh``.
    2.  Change ``: ${BUILD_TYPE:=Debug}`` to ``: ${BUILD_TYPE:=Release}`` inside ``config.sh``
    3.  Optional: Use ``ninja`` instead of ``make`` as it is superior in all regards.
    4.  Run ``./setup.sh`` inside a terminal at the root directory and get a coffee.

2.  Clone Ignis from https://github.com/PearCoding/Ignis

    1.  Make sure all the dependencies listed in official top `README.md <https://github.com/PearCoding/Ignis/blob/master/README.md>`_ are installed.
    2.  Run ``mkdir build && cd build && cmake ..``
    3.  Run ``ccmake .`` inside the ``build/`` directory.
    4.  Change ``AnyDSL_runtime_DIR`` to ``ANYDSL_ROOT/runtime/build/share/anydsl/cmake``, with ``ANYDSL_ROOT`` being the path to the root of the AnyDSL framework.
    5.  Select the drivers you want to compile. All interesting options regarding Ignis start with ``IG_``
    6.  Run ``cmake --build .`` inside the ``build/`` directory and get your second coffee.

3.  Run ``./bin/igview ../scenes/diamond_scene.json`` inside the ``build/`` directory to see if your setup works. This particular render requires the perspective camera and path tracer technique.

.. NOTE:: MacOS CPU vectorization and GPU support is still very experimental and limited. 

Windows
-------

This mini tutorial is expecting some basic knowledge about CMake and the Windows build system based on Visual Studio. I highly recommend using the CMake Ninja generator in favour of the Visual Studio ones, as Visual Studio as an IDE itself has support for both.

1.  Clone AnyDSL from https://github.com/AnyDSL/anydsl. You have to use the ``cmake-based-setup`` branch.

    1.  Patch the repo according to the `AnyDSLWindows.patch <https://github.com/PearCoding/Ignis/blob/master/docs/AnyDSLWindows.patch>`_:
    
        .. literalinclude:: ../../AnyDSLWindows.patch
            :language: diff

    2.  Make sure all the necessary dependencies are installed. Especially zlib and potentially CUDA.
    3.  Create a new directory named ``build``
    4.  Open the command line interface in the newly created directory. Make sure the recent VC environment is available.
    5.  Use CMake and the following command line to configure the project. It is very likely that you have to change some paths. 
    
        Also make sure that the following snippet is written in a single line and, if necessary, the ``\`` in paths is properly escaped:

        .. code-block:: console

            cmake 
                -DRUNTIME_JIT=ON
                -DBUILD_SHARED_LIBS=ON
                -DCMAKE_BUILD_TYPE="Release"
                -DAnyDSL_runtime_BUILD_SHARED=ON
                -DAnyDSL_PKG_LLVM_AUTOBUILD=ON
                -DAnyDSL_PKG_LLVM_VERSION="12.0.0"
                -DAnyDSL_PKG_RV_TAG="origin/release/12.x"
                -DAnyDSL_PKG_LLVM_URL="https://github.com/llvm/llvm-project/releases/download/llvmorg-12.0.0/llvm-project-12.0.0.src.tar.xz"
                -DTHORIN_PROFILE=OFF
                -DBUILD_SHARED_LIBS=OFF
                -DCUDAToolkit_NVVM_LIBRARY="C:\\Program Files\\NVIDIA GPU Computing Toolkit\\CUDA\\v11.6\\nvvm\\lib\\x64\\nvvm.lib"
                -DZLIB_LIBRARY="C:\\Development\\Dependencies\\zlib\\lib\\zlib.lib"
                -DZLIB_INCLUDE_DIR="C:\\Development\\Dependencies\\zlib\\include"
                ..

    6.  If you get a similar cmake error like the following:
        
        .. code-block:: console

            CMake Error in build/_deps/rv-src/src/CMakeLists.txt:
              Target "RV" INTERFACE_INCLUDE_DIRECTORIES property contains path:

                "C:/Development/Projects/AnyDSL/build/_deps/rv-src/include"

              which is prefixed in the build directory.

        Just ignore it. A file named ``AnyDSL.sln`` should still be created in the build folder. This might only be relevant for cmake configs with Visual Studio generators, however.
    7.  If you are using the Visual Studio generator, you can now use the generated ``.sln`` to compile the project. This will take some time. Make sure that you use the ``Release`` configuration.
    8.  For other IDEs use ``cmake --build ..``

2.  Clone Ignis from https://github.com/PearCoding/Ignis. This time the ``master`` branch is fine.

    1.  Getting AnyDSL to work is the hardest part. Congrats if you made it so far. However, Ignis requires some dependencies and configurations to work with AnyDSL.
    2.  Make sure zlib, Intel oneAPI TBB and Eigen 3 are installed on your system. It is also recommended to install SDL2 to be able to use the viewer.
    3.  Create a new directory named ``build``
    4.  Open the command line interface in the newly created directory. Make sure the recent VC environment is available.
    5.  In the command line interface write the following and adapt it to your AnyDSL setup:
        
        .. code-block:: console

            set PATH=%PATH%;C:\Development\Projects\AnyDSL\build\_deps\llvm-build\Release\bin
    6.  Use CMake and the following command line to configure the project. Make sure you use the ``Makefile`` or ``Ninja`` generator, as the Visual Studio one is not working. It is very likely that you have to change some paths. Especially, adapt it to your AnyDSL setup. 
        
        Also make sure that the following snippet is written in a single line and, if necessary, the ``\`` in paths is properly escaped:
        
        .. code-block:: console

            cmake 
                -DCMAKE_BUILD_TYPE="Release"
                -DClang_BIN="C:\\Development\\Projects\\AnyDSL\\build\\_deps\\llvm-build\\Release\\bin\\clang.exe" 
                -DAnyDSL_runtime_DIR="C:\\Development\\Projects\\AnyDSL\\build\\share\\anydsl\\cmake" 
                -DArtic_BINARY_DIR="C:\\Development\\Projects\\AnyDSL\\build\\bin\\Release" 
                -DArtic_BIN="C:\\Development\\Projects\\AnyDSL\\build\\bin\\Release\\artic.exe"
                -DTBB_tbb_LIBRARY_RELEASE="C:\\Program Files (x86)\\Intel\\oneAPI\\tbb\\2021.1.1\\lib\\intel64\\vc_mt\\tbb12.lib"
                -DTBB_tbbmalloc_LIBRARY_RELEASE="C:\\Program Files (x86)\\Intel\\oneAPI\\tbb\\2021.1.1\\lib\\intel64\\vc_mt\\tbbmalloc.lib" 
                -DTBB_INCLUDE_DIR="C:\\Program Files (x86)\\Intel\\oneAPI\\tbb\\2021.1.1\\include" 
                -DZLIB_LIBRARY_RELEASE="C:\\Development\\Dependencies\\zlib\\lib\\zlib.lib" 
                -DZLIB_INCLUDE_DIR="C:\\Development\\Dependencies\\zlib\\include" 
                -DSDL2_LIBRARY="C:\\Development\\Dependencies\\SDL2\\lib\\x64\\SDL2.lib" 
                -DSDL2_INCLUDE_DIR="C:\\Development\\Dependencies\\SDL2\\include"
                ..
    
        You can ignore the ``SDL2`` entries if you decide not to use ``igview`` or change the build type to ``Debug`` if necessary.

    7.  In contrary to the AnyDSL setup you can **not** use the newly generated ``.sln`` file directly. Use ``cmake --build . --config Release`` or use Visual Studio with the CMake interface.
    8.  To run the frontends you might have to add multiple shared libraries (``*.dlls``) to the ``PATH`` environment variable or copy it next to the executables.
        Currently the shared libraries ``runtime.dll``, ``runtime_jit_artic.dll``, ``nvvm64.dll``, ``tbb.dll``, ``tbb_malloc.dll``, ``SDL2.dll``, ``zlib.dll`` are known to be required.
        The list is not exhaustive however, as the final list of dependencies depends on the system, current state of development and other external factors.
        If a module (e.g., ``ig_driver_avx2.dll``) can not been found, but exists on the filesystem, a reason for the error might be a missing shared library.
        Use one of the many dll dependency viewers available on Windows to find the exact missing dll and copy it next to the build executable or add it to the ``PATH`` environment variable.

Known Issues
------------

-   If you get a ``CommandLine Error: Option 'help-list' registred more than once!``, most likely the AnyDSL LLVM library and system LLVM library with exposed symbols are loaded at the same time.
    A known cause is that ``igview`` and SDL are using a graphic driver which is loading the system LLVM library in the background.
    On Linux, using accelerated rendering load the X11 drivers, which in return load the system LLVM, which in return clash with the custom LLVM.
    Setting the environment variable ``SDL_RENDER_DRIVER=software`` and ``SDL_FRAMEBUFFER_ACCELERATION=0`` should be a good workaround. This will not prevent you of using the GPU for raytracing however, only the UI will be software rendered.
