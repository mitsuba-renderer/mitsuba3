# Regenerate the committed intersection_functions.metallib from the MSL source.
#
# This is a developer-only step that requires the full Metal toolchain
# (`xcrun metal`), unlike the Mitsuba build itself, which only embeds the
# committed metallib. Run `make` here after editing intersection_functions.metal.
#
# The language version (metal3.2) and deployment target (macOS 13.0) match the
# combination used by Dr.Jit-Core's resources/Makefile; metal3.2 is required
# because the SDF-grid intersector uses MSL lambdas.

METAL_FLAGS := -mmacosx-version-min=13.0 -std=metal3.2 -fno-fast-math -I../../../include

all: intersection_functions.metallib

intersection_functions.metallib: intersection_functions.metal
	xcrun -sdk macosx metal $(METAL_FLAGS) -c $< -o intersection_functions.air
	xcrun -sdk macosx metallib intersection_functions.air -o $@
	rm -f intersection_functions.air

clean:
	rm -f intersection_functions.air

.PHONY: all clean
