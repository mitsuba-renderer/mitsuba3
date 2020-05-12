mts_include_folder = ../../include
mts_shape_folder   = ../../src/shapes/optix
mts_optix_main     = ../../src/librender/optix/optix_rt.cu

all: optix_rt.ptx

optix_rt.ptx: $(mts_optix_main) $(mts_include_folder)/* $(mts_shape_folder)/*
	nvcc $(mts_optix_main) \
		 -I $(mts_include_folder) -I $(mts_shape_folder) -I /opt/optix-7.0.0/include/ \
		 -O3 -gencode arch=compute_61,code=compute_61 --ptx

clean:
	rm -f optix_rt.ptx
