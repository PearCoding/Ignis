#!/usr/bin/bash 

# Contains bash completion for Ignis (igcli, igview and igtrace)

_completion_handler()
{
    local args=$($1 --list-cli-options | xargs)
    COMPREPLY=($(compgen -W "${args}" "${COMP_WORDS[1]}"))
    COMPREPLY+=($(compgen -f -X "!*.json" "${COMP_WORDS[1]}"))
    COMPREPLY+=($(compgen -f -X "!*.gltf" "${COMP_WORDS[1]}"))
    COMPREPLY+=($(compgen -f -X "!*.glb" "${COMP_WORDS[1]}"))
}

complete -o default -o nospace -D -F _completion_handler igcli
complete -o default -o nospace -D -F _completion_handler igview
complete -o default -o nospace -D -F _completion_handler igtrace
