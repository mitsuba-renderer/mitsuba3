# Target of the precompiled PTX file shipped with Mitsuba:
#   - CUDA 12.2 (nvcc 12.2.140)
#   - PTX ISA version 8.2
#   - Minimum CUDA driver: 535 (the one bundled with CUDA 12.2)
#   - Compute Capability >= 7.5 (Turing and newer)

COMPUTE_CAPABILITY=compute_75
CUDA_VER=12.2
HOST_CC=/usr/bin/gcc-12
NVCC=/usr/local/cuda-$(CUDA_VER)/bin/nvcc -ccbin $(HOST_CC) -m64 --ptx --use_fast_math --expt-relaxed-constexpr -std=c++14 -Xcudafe --diag_suppress=550 -Xcudafe --diag_suppress=177 -Xcudafe --diag_suppress=2912 -Xcudafe --display_error_number

mts_include_folder = ../../include
mts_shape_folder   = ../../src/shapes/optix
mts_optix_main     = ../../src/render/optix/optix_rt.cu

OPTIX_PATH := $(or $(OPTIX_PATH),/opt/optix/include/)

all: optix_rt.ptx

optix_rt.ptx: $(mts_optix_main) $(mts_include_folder)/* $(mts_shape_folder)/*
	$(NVCC) -Wno-deprecated-gpu-targets -gencode arch=$(COMPUTE_CAPABILITY),code=$(COMPUTE_CAPABILITY) \
		-I $(mts_include_folder) -I $(mts_shape_folder) -I $(OPTIX_PATH) \
		$(mts_optix_main) -o optix_rt.ptx

clean:
	rm -f optix_rt.ptx
