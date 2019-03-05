all: optix_rt.ptx optix_attr.ptx

optix_rt.ptx: optix_rt.cu
	nvcc optix_rt.cu -O3 -gencode arch=compute_61,code=compute_61 --ptx -I /opt/optix/include/

optix_attr.ptx: optix_attr.cu
	nvcc optix_attr.cu -O3 -gencode arch=compute_61,code=compute_61 --ptx -I /opt/optix/include/

clean:
	rm -f optix_rt.ptx optix_attr.ptx
