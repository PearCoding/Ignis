# Source Directory

Contains all source code.

## artic/

This directory contains artic source code used in the driver or while JIT compiling.
The directory can be used as the destination for the `--script-dir` command-line argument.

### core/, render/, traversal/

These directories contain important source code used in the driver or for interfacing with the shaders.

### impl/

Code here is only JIT compiled and can be changed without long compile time. As the internal source code is packed into the executable, a standard `cmake --build`, `make` or `ninja` is still needed.

## backend/

Contains backend related stuff. Major part of the framework is contained here.

## frontend/

Contains source code of the multiple frontends available.

## tests/

Contains tests.

## tools/

Contains multiple useful, but optional tools.
