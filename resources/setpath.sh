# This script adds Mitsuba and the Python bindings to the shell's search
# path. It must be executed via the 'source' command so that it can modify
# the relevant environment variables.

MTS_DIR=""
MTS_VARIANTS="@MTS_VARIANT_NAMES_STR@"

if [ "$BASH_VERSION" ]; then
    if [[ "${BASH_SOURCE[0]}" != "${0}" ]]; then
        MTS_DIR=$(dirname "$BASH_SOURCE")
        MTS_DIR=$(builtin cd "$MTS_DIR"; builtin pwd)
    fi
elif [ "$ZSH_VERSION" ]; then
    if [[ -n ${(M)zsh_eval_context:#file} ]]; then
        MTS_DIR=$(dirname "$0:A")
    fi
fi

if [ -z "$MTS_DIR" ]; then
    echo "This script must be executed via the 'source' command, i.e.:"
    echo "$ source ${0}"
    exit 0
fi

export PYTHONPATH="$MTS_DIR/python:$PYTHONPATH"
export PATH="$MTS_DIR:$PATH"

if [ "$BASH_VERSION" ]; then
    _mitsuba() {
        local FLAGS
        FLAGS="-a -h -m -o -s -t -u -v -D"

        if [[ $2 == -* ]]; then
            COMPREPLY=( $(compgen -W "${FLAGS}" -- $2) )
        elif [[ $3 == -m ]]; then
            COMPREPLY=( $(compgen -W "${MTS_VARIANTS}" -- $2) )
        else
            COMPREPLY=()
        fi

        return 0
    }
    complete -F _mitsuba -o default mitsuba
elif [ "$ZSH_VERSION" ]; then
    _mitsuba() {
        _arguments \
            '-a[Add an entry to the search path]' \
            '-h[Help]' \
            '-m[Select the variant/mode (e.g. "llvm_rgb")]' \
            '-o[Specify the output filename]' \
            '-s[Sensor/camera index to use for rendering]' \
            '-t[Override the number of threads]' \
            '-u[Update the scene description to the latest version]' \
            '-v[Be more verbose. Can be specified multiple times]' \
            '-D[Define a constant that can be referenced in the scene]' \
            '*: :->file'

        if [[ $state == '-' ]]; then
            :
        elif [[ $words[CURRENT-1] == '-m' ]]; then
            compadd "$@" ${=MTS_VARIANTS}
        else
            _files
        fi

    }
    compdef _mitsuba mitsuba
fi
