_sdb()
{
    #check if the input is local or remote path.
    INPUT_IS_PATH=0

    #sdb path is defined in PATH environment variable
    SDB_PATH=$(eval eval echo \$\{COMP_WORDS\[0\]\})

    COMPREPLY=()

    if [ ! -f ${SDB_PATH} ];
    then
        return 0;
    fi

#    cur="${COMP_WORDS[COMP_CWORD]}"
    ARGS="autocomplete,${COMP_CWORD}"

    local IFS=$','

    if [ -n ${COMP_WORDS[1]} ]; then
        if [ "${COMP_WORDS[1]}" == "push" ] || [ "${COMP_WORDS[1]}" == "pull" ]; then
	    INPUT_IS_PATH=1
	fi
    fi

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

    next=($("${SDB_PATH}" ${ARGS}))
    local IFS=$'\n'
    COMPREPLY=(${next})

    if [ $INPUT_IS_PATH == 0 ]; then 
        COMPREPLY=( "${COMPREPLY[@]/%/ }" )   #add trailing space to each
    fi

    return 0
}

complete -o nospace -F _sdb sdb
