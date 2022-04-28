
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

export PATH="$IGNIS_DIR/build/bin:$IGNIS_DIR/build/Release/bin:$PATH"
export PATH="$IGNIS_DIR/build/lib:$IGNIS_DIR/build/Release/lib:$PATH"
export PYTHONPATH="$IGNIS_DIR/build/api:$IGNIS_DIR/build/Release/api:$PYTHONPATH"
