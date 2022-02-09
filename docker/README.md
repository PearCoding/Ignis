# Docker Image

Ignis is available on docker hub with the image [pearcoding/ignis](https://hub.docker.com/repository/docker/pearcoding/ignis)

It is preconfigured for specific flavours and only contains the runtime without source code or scenes. The underlying distribution is Debian.

Available flavours:

 - `AVX2` with igtrace, igcli, igview for AVX2 feature sets
 - `AVX2-PY` with igtrace, igcli, igview and Python API for AVX2 feature sets (TODO)
 - `CUDA` with igtrace, igcli, igview for AVX2 & CUDA feature sets (TODO)
 - `CUDA-PY` with igtrace, igcli, igview and Python API for AVX2 & CUDA feature sets (TODO)
 - `ROCM` with igtrace, igcli, igview for AVX2 & ROCM (AMD) feature sets (TODO)
 - `ROCM-PY` with igtrace, igcli, igview and Python API for AVX2 & ROCM (AMD) feature sets (TODO)

## Entrypoint

The default entry point is `igcli`.

## Usage

As resources have to be loaded and exported it is necessary to read&write mount a volume to let the corresponding frontend access the data.

Example for `igcli`

    docker run -v ~/scenes:/scenes -it pearcoding/ignis:latest-avx2 /scenes/diamond_scene.json --spp 1024 -o /scenes/output/test.exr

Example for `igtrace`

    docker run -v ~/scenes:/scenes --entrypoint igtrace -it pearcoding/ignis:latest-avx2 /scenes/diamond_scene.json

The usage of `igview` is still experimental as a X11 server is required.
