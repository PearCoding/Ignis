Known Issues
============

-   If you get a ``CommandLine Error: Option 'help-list' registered more than once!``, most likely the AnyDSL LLVM library and system LLVM library with exposed symbols are loaded at the same time.
    A known cause is that ``igview`` and SDL are using a graphic driver which is loading the system LLVM library in the background.
    On Linux, using accelerated rendering load the X11 drivers, which in return load the system LLVM, which in return clash with the custom LLVM.
    Setting the environment variable ``SDL_RENDER_DRIVER=software`` and ``SDL_FRAMEBUFFER_ACCELERATION=0`` should be a good workaround. This will not prevent you of using the GPU for raytracing however, only the UI will be software rendered.

-   If running ``artic`` or ``clang`` fails when building Ignis it might be due to the two executables not able to find ``zlib.dll``. Make sure it is available for them. A simple solution is to just copy the ``zlib.dll`` next to the executables.

-   Getting the following cmake error in LLVM:

    .. code-block:: console

        CMake Error at build/_deps/llvm-src/llvm/cmake/modules/AddLLVM.cmake:1985 (string):
        string begin index: -1 is out of range 0 - 23
        Call Stack (most recent call first):
        build/_deps/llvm-src/llvm/tools/llvm-ar/CMakeLists.txt:20 (add_llvm_tool_symlink)

    can be fixed by explicitly unsetting the cmake variable ``CMAKE_CONFIGURATION_TYPES`` via ``-UCMAKE_CONFIGURATION_TYPES`` in the command line or in the ``scripts/setup/windows/config.json`` when using the automatic script.
