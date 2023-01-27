
# ***************************************************************
# * This script adds Ignis to the current path.
# * It assumes that Ignis is either compiled within a 
# * a subdirectory named 'build' or within a subdirectory named 'build/Release'.
# ***************************************************************

# See https://stackoverflow.com/questions/2683279/how-to-detect-if-a-script-is-being-sourced
sourced=0
if [ -n "$ZSH_VERSION" ]; then 
  case $ZSH_EVAL_CONTEXT in *:file) sourced=1;; esac
elif [ -n "$KSH_VERSION" ]; then
  [ "$(cd -- "$(dirname -- "$0")" && pwd -P)/$(basename -- "$0")" != "$(cd -- "$(dirname -- "${.sh.file}")" && pwd -P)/$(basename -- "${.sh.file}")" ] && sourced=1
elif [ -n "$BASH_VERSION" ]; then
  (return 0 2>/dev/null) && sourced=1 
else # All other shells: examine $0 for known shell binary filenames.
  # Detects `sh` and `dash`; add additional shell filenames as needed.
  case ${0##*/} in sh|-sh|dash|-dash) sourced=1;; esac
fi

if [ "${sourced}" = "0" ]; then
    echo "The source.sh script must be sourced, not executed. In other words, run"
    echo "$ source source.sh"
    exit 0
fi

# Acquire parameters
quiet=false
no_completion=false

while [ -n "$1" ]; do
    case "$1" in
    --no-completion)
        no_completion=true
        shift
        ;;
    -q) 
        quiet=true 
        shift
        ;;
    --)
        shift
        break
        ;;
    *) echo "$1 is not an option" ;;
    esac
    shift
done

# Get directories
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

if [ "$no_completion" = false ]; then
    # Include completion scripts if available
    if [ -f "${IGNIS_DIR}/scripts/bash/ignis-completion.bash" ]; then
        source "${IGNIS_DIR}/scripts/bash/ignis-completion.bash"
    fi
fi

if [ "$quiet" = false ]; then
    echo $(igcli --version)
    echo "  - Root:   $IGNIS_DIR" 
    echo "  - Build:  $BUILD_DIR" 
    echo "  - Bin:    $BUILD_DIR/bin" 
    echo "  - Lib:    $BUILD_DIR/lib" 
    echo "  - Python: $BUILD_DIR/api" 
fi
