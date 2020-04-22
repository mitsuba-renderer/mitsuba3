all: optix_rt.ptx vector.cuh

optix_rt.ptx: optix_rt.cu vector.cuh
	nvcc optix_rt.cu -I /opt/optix-7.0.0/include/ -O3 -gencode arch=compute_61,code=compute_61 --ptx

clean:
	rm -f optix_rt.ptx
