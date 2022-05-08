# Compiling Ignis on Windows

This mini tutorial is expecting some basic knowledge about CMake and the Windows build system based on Visual Studio. I highly recommend using the CMake Ninja generator in favour of the Visual Studio ones, as Visual Studio as an IDE itself has support for both.

## AnyDSL

- First get AnyDSL from the [Github repository](https://github.com/AnyDSL/anydsl). You have to use the `cmake-based-setup` branch.
- Patch the repo according to the `AnyDSLWindows.patch`.
- Make sure all the necessary dependencies are installed. Especially zlib and potentially CUDA.
- Create a new directory named `build`
- Open the command line interface in the newly created directory. Make sure the recent VC environment is available.
- Use CMake and the following command line to configure the project. It is very likely that you have to change some paths. Also make sure that the following snippet is written in a single line.

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
	    -DCUDAToolkit_NVVM_LIBRARY="C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v11.6\nvvm\lib\x64\nvvm.lib"
	    -DZLIB_LIBRARY="C:\Development\Dependencies\zlib\lib\zlib.lib"
	    -DZLIB_INCLUDE_DIR="C:\Development\Dependencies\zlib\include"
	    ..

- If you are using the Visual Studio generator, you can now use the generated `.sln` to compile the project. This will take some time. Make sure that you use the `Release` configuration.
- For other IDEs use `cmake --build ..`

## Ignis

Getting AnyDSL to work is the hardest part. Congrats if you made it so far. However, Ignis requires some dependencies and configurations to work with AnyDSL.

- First get the most recent Ignis version from the [Github repository](https://github.com/PearCoding/Ignis). This time the `master` branch is fine.
- Make sure zlib, Intel oneAPI TBB and Eigen 3 are installed on your system. It is also recommended to install SDL2 to be able to use the viewer.
- Create a new directory named `build`
- Open the command line interface in the newly created directory. Make sure the recent VC environment is available.
- In the command line interface write the following and adapt it to your AnyDSL setup:

	   set PATH=%PATH%;C:\Development\Projects\AnyDSL\build\_deps\llvm-build\Release\bin
	
- Use CMake and the following command line to configure the project. Make sure you use the `Makefile` or `Ninja` generator, as the Visual Studio one is not working. It is very likely that you have to change some paths. Especially, adapt it to your AnyDSL setup. Also make sure that the following snippet is written in a single line.

	   cmake 
	    -DCMAKE_BUILD_TYPE="Release"
	    -DClang_BIN="C:\Development\Projects\AnyDSL\build\_deps\llvm-build\Release\bin\clang.exe" 
	    -DAnyDSL_runtime_DIR="C:\Development\Projects\AnyDSL\build\share\anydsl\cmake" 
	    -DArtic_BINARY_DIR="C:\Development\Projects\AnyDSL\build\bin\Release" 
	    -DArtic_BIN="C:\Development\Projects\AnyDSL\build\bin\Release\artic.exe"
	    -DTBB_tbb_LIBRARY_RELEASE="C:\Program Files (x86)\Intel\oneAPI\tbb\2021.1.1\lib\intel64\vc_mt\tbb12.lib"
	    -DTBB_tbbmalloc_LIBRARY_RELEASE="C:\Program Files (x86)\Intel\oneAPI\tbb\2021.1.1\lib\intel64\vc_mt\tbbmalloc.lib" 
	    -DTBB_INCLUDE_DIR="C:\Program Files (x86)\Intel\oneAPI\tbb\2021.1.1\include" 
	    -DZLIB_LIBRARY_RELEASE="C:\Development\Dependencies\zlib\lib\zlib.lib" 
	    -DZLIB_INCLUDE_DIR="C:\Development\Dependencies\zlib\include" 
	    -DSDL2_LIBRARY="C:\Development\Dependencies\SDL2\lib\x64\SDL2.lib" 
	    -DSDL2_INCLUDE_DIR="C:\Development\Dependencies\SDL2\include"
	    ..

  You can ignore the `SDL2` entries if you decide not to use `igview`. You can change the build type to `Debug` if necessary.
- In contrary to the AnyDSL setup you can **not** use the newly generated `.sln` file directly. Use `cmake --build ..` or use Visual Studio with the CMake interface.
