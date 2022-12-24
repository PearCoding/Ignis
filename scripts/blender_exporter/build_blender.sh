#!/bin/bash

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)
CUR_DIR=$(pwd)

cd ${SCRIPT_DIR}

NAME=ignis_blender
zip -qr ${NAME} ${NAME}

echo "Blender plugin built. Open Blender and go to 'Edit - Preferences - Addons - Install...'"
echo "Install '${SCRIPT_DIR}/${NAME}.zip'."

cd ${CUR_DIR}