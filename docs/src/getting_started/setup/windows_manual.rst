Windows
=======

.. WARNING:: It is recommended to use the new :ref:`automatic setup<automatic setup>` instead of the following one! The following may get outdated.

This mini tutorial is expecting some basic knowledge about CMake and the Windows build system based on Visual Studio. I highly recommend using the CMake Ninja generator in favour of the Visual Studio ones, as Visual Studio as an IDE itself has support for both.

1.  Clone AnyDSL from https://github.com/AnyDSL/anydsl. You have to use the ``cmake-based-setup`` branch.

    1.  Make sure all the necessary dependencies are installed. Especially zlib and potentially CUDA.
    2.  Create a new directory named ``build``
    3.  Open the command line interface in the newly created directory. Make sure the recent VC environment is available.
    4.  Use CMake and the following command line to configure the project. It is very likely that you have to change some paths. 
    
        Also make sure that the following snippet is written in a single line and, if necessary, the ``\`` in paths is properly escaped:

        .. code-block:: console

            cmake 
                -DRUNTIME_JIT=ON
                -DBUILD_SHARED_LIBS=ON
                -DCMAKE_BUILD_TYPE="Release"
                -DAnyDSL_runtime_BUILD_SHARED=ON
                -DAnyDSL_PKG_LLVM_AUTOBUILD=ON
                -DAnyDSL_PKG_LLVM_VERSION="14.0.6"
                -DAnyDSL_PKG_RV_TAG="origin/release/14.x"
                -DAnyDSL_PKG_LLVM_URL="https://github.com/llvm/llvm-project/releases/download/llvmorg-14.0.6/llvm-project-14.0.6.src.tar.xz"
                -DTHORIN_PROFILE=OFF
                -DBUILD_SHARED_LIBS=OFF
                -DCUDAToolkit_NVVM_LIBRARY="C:\\Program Files\\NVIDIA GPU Computing Toolkit\\CUDA\\v11.7\\nvvm\\lib\\x64\\nvvm.lib"
                -DZLIB_LIBRARY="C:\\Development\\Dependencies\\zlib\\lib\\zlib.lib"
                -DZLIB_INCLUDE_DIR="C:\\Development\\Dependencies\\zlib\\include"
                ..

    5.  If you get a similar cmake error like the following:
        
        .. code-block:: console

            CMake Error in build/_deps/rv-src/src/CMakeLists.txt:
              Target "RV" INTERFACE_INCLUDE_DIRECTORIES property contains path:

                "C:/Development/Projects/AnyDSL/build/_deps/rv-src/include"

              which is prefixed in the build directory.

        Just ignore it. A file named ``AnyDSL.sln`` should still be created in the build folder. This might only be relevant for cmake configs with Visual Studio generators, however.
    6.  If you are using the Visual Studio generator, you can now use the generated ``.sln`` to compile the project. This will take some time. Make sure that you use the ``Release`` configuration. Make sure the ``runtime``, ``clang`` and ``artic`` project are built successfully, the others might fail for unknown reasons.
    7.  For other IDEs use ``cmake --build ..``

2.  Clone Ignis from https://github.com/PearCoding/Ignis. This time the ``master`` branch is fine.

    1.  Getting AnyDSL to work is the hardest part. Congrats if you made it so far. However, Ignis requires some dependencies and configurations to work with AnyDSL.
    2.  Make sure zlib and Intel oneAPI TBB are installed on your system. It is also recommended to install SDL2 to be able to use the viewer.
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
                -DBUILD_TESTING=OFF
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
        Currently the shared libraries ``runtime.dll``, ``runtime_jit_artic.dll``, ``nvvm64.dll`` or ``nvvm64_40_0.dll``, ``tbb.dll``, ``SDL2.dll``, ``zlib.dll`` are known to be required.
        The list is not exhaustive however, as the final list of dependencies depends on the system, current state of development and other external factors.
        Use one of the many dll dependency viewers available on Windows to find missing dlls and copy it next to the build executable or add it to the ``PATH`` environment variable.
