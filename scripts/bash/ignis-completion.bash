#!/usr/bin/bash 

# Contains bash completion for Ignis (igcli, igview and igtrace)

_completion_handler()
{
    local args cur prev opts
 	
    # Filter valid executable names (no idea why this is needed)
    case $(basename $1) in
    igcli) ;;
    igview) ;;
    igtrace) ;;
    *)
        return ;;
    esac


    COMPREPLY=()
 	cur="${COMP_WORDS[COMP_CWORD]}"
 	prev="${COMP_WORDS[COMP_CWORD-1]}"
 	opts=""

    args=$($1 --list-cli-options | xargs)
    COMPREPLY=($(compgen -W "${args}" -- "${cur}"))
    COMPREPLY+=($(compgen -f -X "!*.json" -- "${cur}"))
    COMPREPLY+=($(compgen -f -X "!*.gltf" -- "${cur}"))
    COMPREPLY+=($(compgen -f -X "!*.glb" -- "${cur}"))
}

complete -o default -o nospace -D -F _completion_handler igcli
complete -o default -o nospace -D -F _completion_handler igview
complete -o default -o nospace -D -F _completion_handler igtrace
