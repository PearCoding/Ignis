#!/bin/sh

NAME=ignis_blender
zip -qr ${NAME} ${NAME}

echo "Blender plugin built. Open Blender and go to 'Edit - Preferences - Addons - Install...'"
echo "Browse to the '${NAME}.zip' file in this directory and install it."