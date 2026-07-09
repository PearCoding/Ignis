# Version 0.4.0

Multiple frontends and features:

- `igexplorer` Frontend for glare evaluation
- `igview` Interactive frontend for scene exploration
- `igcli` Command-Line tool for final renders
- `igutil` Command-Line tool to convert between images
- `igevalglare` Command-Line tool for glare evaluation (experimental)
- `igbsdfinspector` Basic viewer for Klems and Tensor BSDFs (experimental)
- Python API using Python 3.8 and above
- [Blender](https://www.blender.org/) extension
- Cuda accelerated
- Denoising with [Intel Open Image Denoise](https://www.openimagedenoise.org/) (v2.3.1)

## Highlights

- The interactive frontends moved from SDL2 to GLFW and OpenGL, which removes a
  runtime dependency and fixes the window handling on all platforms.
- Experimental path guiding, including a one-lobe vMF variant, sun-focused
  guiding and learned guiding for environment maps.
- Glare evaluation is now part of the runtime and no longer depends on the
  color space of the input.
- Reworked AOV handling with support for mono AOVs.
- The Python module is built against the stable ABI, so a single wheel works
  for Python 3.8 and above.
- Ignis falls back to the CPU when a GPU target fails to initialize.
- Hardened scene loading: the glTF, PLY, Klems, TensorTree and serialized mesh
  parsers now reject malformed and out-of-range input instead of reading past
  their buffers.
- Several correctness fixes for TensorTree and Klems BSDFs, validated against
  [Radiance](https://www.radiance-online.org/).

## Download

This release has a binary for Windows with CUDA support included. You can
download the installer version (.exe). Keep in mind that Ignis is still in Beta
and only `igexplorer` and `igbsdfinspector` can be opened without the terminal.

## Status

This is the first release built and published automatically from a version tag.

Progressive photon mapping is currently disabled and the guiding techniques are
experimental. As always, expect rough edges.
