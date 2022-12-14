#!/bin/bash
# Will export Ignis scenes from the blend files. Only works if Blender has the Ignis plugin enabled

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

export_scene () {
    temp_file=$(mktemp)
    echo "
import bpy
bpy.ops.export_scene.ignis('INVOKE_DEFAULT', filepath='$2')
    " > $temp_file
    blender "$1" -b -P $temp_file
    rm $temp_file
}

export_scene "${SCRIPT_DIR}/box.blend" "${SCRIPT_DIR}/../cycles-box.json"
export_scene "${SCRIPT_DIR}/lights.blend" "${SCRIPT_DIR}/../cycles-lights.json"
export_scene "${SCRIPT_DIR}/principled.blend" "${SCRIPT_DIR}/../cycles-principled.json"
export_scene "${SCRIPT_DIR}/tex.blend" "${SCRIPT_DIR}/../cycles-tex.json"
