# Docker Image

Ignis is available on docker hub with the image [pearcoding/ignis](https://hub.docker.com/repository/docker/pearcoding/ignis)
and a standard AnyDSL version with JIT is available on docker hub with the image [pearcoding/anydsl](https://hub.docker.com/repository/docker/pearcoding/anydsl)

The images are preconfigured for specific flavours and only contain the runtime without source code or scenes. The underlying distribution is Debian.

Available flavours:

 - `CPU` with igtrace, igcli, igview and Python API for cpu only
 - `CUDA` with igtrace, igcli, igview and Python API with cpu and CUDA support
 - `ROCM` with igtrace, igcli, igview and Python API with cpu and ROCM (AMD) support (TODO)

## Entrypoint

The default entry point is `igcli`.

## Usage

As resources have to be loaded and exported it is necessary to read&write mount a volume to let the corresponding frontend access the data.

Example for `igcli`

    docker run -v ~/scenes:/scenes -it pearcoding/ignis:latest-cpu /scenes/diamond_scene.json --spp 1024 -o /scenes/output/test.exr

Example for `igtrace`

    docker run -v ~/scenes:/scenes --entrypoint igtrace -it pearcoding/ignis:latest-cpu /scenes/diamond_scene.json

The usage of `igview` is still experimental as a X11 server is required.

## CUDA

To use the cuda docker flavour have a look at the userguide from NVIDIA:
https://docs.nvidia.com/datacenter/cloud-native/container-toolkit/user-guide.html and https://docs.nvidia.com/datacenter/cloud-native/container-toolkit/install-guide.html
