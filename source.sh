
# ***************************************************************
# * This script adds Ignis to the current path.
# * It assumes that Ignis is either compiled within a 
# * a subdirectory named 'build' or within a subdirectory named 'build/Release'.
# ***************************************************************

if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    echo "The source.sh script must be sourced, not executed. In other words, run\n"
    echo "$ source source.sh\n"
    exit 0
fi

if [ "$BASH_VERSION" ]; then
    IGNIS_DIR=$(dirname "$BASH_SOURCE")
    export IGNIS_DIR=$(builtin cd "$IGNIS_DIR"; builtin pwd)
elif [ "$ZSH_VERSION" ]; then
    export IGNIS_DIR=$(dirname "$0:A")
fi

if [ -d "$IGNIS_DIR/build/Release" ]; then
    BUILD_DIR="$IGNIS_DIR/build/Release"
else
    BUILD_DIR="$IGNIS_DIR/build/"
fi

export PATH="$BUILD_DIR/bin:$PATH"
export PATH="$BUILD_DIR/lib:$PATH"
export PYTHONPATH="$BUILD_DIR/api:$PYTHONPATH"

# Include completion scripts if available
if [ -f "${IGNIS_DIR}/scripts/bash/ignis-completion.bash" ]; then
    source "${IGNIS_DIR}/scripts/bash/ignis-completion.bash"
fi
