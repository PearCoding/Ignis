# Scripts Directory

This directory contains multiple scripts used in evaluation, showcase and feature testing.

## Mts2Json

Instead of `mts2json.py` it is recommended to use the `igutil` tool. It is listed here for historic reasons.

## Rad2Json

`rad2json` can be used to convert Radiance geometry data into Ignis scene format.
Support for basic (non `mod`) material and light is planned, but yet missing.

## gen_spherical_probe.py

Use `gen_spherical_probe.py` to generate ray orientations useable with `igtrace` or `rtrace` (Radiance) to validate scenes.
You can also pipe the output as usual with the Radiance workflow.

## benchmark*.sh

Used internally to test different branches or feature sets after major changes.

## Run*.py

Used internally to render all the showcases and evaluations embedded into the documentation.

## run_*.sh

Scripts used for comparison and benchmarking between Ignis and Radiance.

## api/

Uses the Ignis PythonAPI to test scenes.

## bash/

Contains a bash completion script.

## blender_exporter/

The official Ignis Blender plugin.

## docker/

Scripts for internal docker creation and setup.

## setup/

Automatic setup script for Windows. Will setup AnyDSL and initialize Ignis for further development for you.
