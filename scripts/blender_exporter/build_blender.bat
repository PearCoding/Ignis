@echo off


zip -r ignis_blender ignis_blender
if %errorlevel% neq 0 exit /b %errorlevel%


echo Blender plugin built. Open Blender and go to 'Edit - Preferences - Addons - Install...'

echo Browse to the 'ignis_blender.zip' file in this directory and install it.