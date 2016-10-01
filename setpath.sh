#
# This script adds Mitsuba to the current path.
# It works with both Bash and Zsh.
#
# NOTE: this script must be sourced and not run, i.e.
#    . setpath.sh        for Bash
#    source setpath.sh   for Zsh or Bash
#

if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
	echo "The setpath.sh script must be sourced, not executed. In other words, run\n"
	echo "$ source setpath.sh\n"
	exit 0
fi

if [ "$BASH_VERSION" ]; then
	MITSUBA_DIR=$(dirname "$BASH_SOURCE")
	export MITSUBA_DIR=$(builtin cd "$MITSUBA_DIR"; builtin pwd)
elif [ "$ZSH_VERSION" ]; then
	export MITSUBA_DIR=$(dirname "$0:A")
fi

export PYTHONPATH="$MITSUBA_DIR/dist/python:$MITSUBA_DIR/build/dist/python"
export PATH="$MITSUBA_DIR/dist:$MITSUBA_DIR/build/dist:$PATH"
export LD_LIBRARY_PATH="$MITSUBA_DIR/dist:$MITSUBA_DIR/build/dist:$LD_LIBRARY_PATH"