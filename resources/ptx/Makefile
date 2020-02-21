all: optix_rt.ptx vector.cuh optix_attr.ptx

optix_rt.ptx: optix_rt.cu vector.cuh
	nvcc optix_rt.cu -I ~/NVIDIA-OptiX-SDK-7.0.0-linux64/include/ -O3 -gencode arch=compute_61,code=compute_61 --ptx -I /opt/optix/include/

optix_attr.ptx: optix_attr.cu
	nvcc optix_attr.cu -O3 -gencode arch=compute_50,code=compute_50 --ptx -I /opt/optix/include/

clean:
	rm -f optix_rt.ptx optix_attr.ptx
