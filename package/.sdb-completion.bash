_sdb()
{
    #sdb path is defined in PATH environment variable
    COMPREPLY=()
#    cur="${COMP_WORDS[COMP_CWORD]}"
    ARGS="autocomplete,${COMP_CWORD}"

    local IFS=$','
    for ((i=1; i < $((${COMP_CWORD} + 1)) ; i++))
    do
        #processing for echo options
        if [ "${COMP_WORDS[i]}" == "-e" ]; then
            convertedarg=-e
        elif [ "${COMP_WORDS[i]}" == "-n" ]; then
            convertedarg=-n
        else
            convertedarg=$(eval eval echo \$\{COMP_WORDS\[i\]\})
        fi
        ARGS="${ARGS}${IFS}${convertedarg}"
    done

    SDB_PATH=$(eval eval echo \$\{COMP_WORDS\[0\]\})

    next=($("${SDB_PATH}" ${ARGS}))
    local IFS=$'\n'
    COMPREPLY=(${next})
#    COMPREPLY=($(compgen -W "${next}" -- ${cur}))

    return 0
}

complete -o filenames -F _sdb sdb
