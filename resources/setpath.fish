# This script adds Mitsuba and the Python bindings to the shell's search
# path. It must be executed via the 'source' command so that it can modify
# the relevant environment variables.

set MI_DIR ""
set MI_VARIANTS "@MI_VARIANT_NAMES_STR@"

if [ "$FISH_VERSION" ]
    if string match -q -- "source" (status current-command)
        set MI_DIR (dirname (realpath (status current-filename)))
    end
end

if not string length -q -- $MI_DIR
    echo "This script must be executed via the 'source' command, i.e.:"
    echo "\$ source" (status current-filename)
    exit 0
end

set -gx PYTHONPATH $MI_DIR/python $PYTHONPATH
set -gx PATH $MI_DIR $PATH

# Completions
complete -c mitsuba -s h -l help -d "Print a short help text and exit"
complete -c mitsuba -s m -l mode --no-files -ra $MI_VARIANTS -d "Request a specific mode/variant of the renderer"
complete -c mitsuba -s v -l verbose -d "Be more verbose"
complete -c mitsuba -s t -l threads -d "Render with the specified number of threads"
complete -c mitsuba -s D -l define -d "Define a constant that can be referenced"
complete -c mitsuba -s s -l sensor -d "Index of the sensor to render with"
complete -c mitsuba -s u -l update -d "Update the scene's XML description to the latest version"
complete -c mitsuba -s a -l append -d "Add one or more entries to the resource search path"
complete -c mitsuba -s o -l output -d "Write the output image to a file"
