# ***************************************************************
# * This script adds Mitsuba to the current path on Windows.
# * It assumes that Mitsuba is compiled within a subdirectory
# * named 'build'.
# ***************************************************************

$env:MITSUBA_DIR = Get-Location
$env:PATH = $env:MITSUBA_DIR + ";" + $env:PATH
$env:PYTHONPATH = $env:MITSUBA_DIR + "\python" + ";" + $env:PYTHONPATH