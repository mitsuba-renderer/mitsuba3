mts_include_folder = ../../include
mts_shape_folder   = ../../src/shapes/optix
mts_optix_main     = ../../src/render/optix/optix_rt.cu

OPTIX_PATH := $(or $(OPTIX_PATH),/opt/optix/include/)

all: optix_rt.ptx

optix_rt.ptx: $(mts_optix_main) $(mts_include_folder)/* $(mts_shape_folder)/*
	nvcc $(mts_optix_main) \
		 -I $(mts_include_folder) -I $(mts_shape_folder) -I $(OPTIX_PATH) \
		 -O3 -gencode arch=compute_50,code=compute_50 --ptx

clean:
	rm -f optix_rt.ptx
