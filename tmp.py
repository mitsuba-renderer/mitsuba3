import mitsuba as mi
import drjit as dr
import sys

mi.set_variant('cuda_ad_rgb')

dr.set_log_level(dr.LogLevel.Info)

#dr.set_flag(dr.JitFlag.LoopRecord, False)
#dr.set_flag(dr.JitFlag.VCallRecord, False)
dr.set_flag(dr.JitFlag.KernelHistory, True)
#dr.set_flag(dr.JitFlag.ForceOptiX, True)
dr.set_flag(dr.JitFlag.PrintIR, True)

if bool(int(sys.argv[1])):
    dr.set_flag(dr.JitFlag.ShaderExecutionReordering, True)

print("Loading scene...")
#scene = mi.load_file('/home/nroussel/rgl/scenes/staircase/scene.xml')
scene = mi.load_file('/home/nroussel/rgl/scenes/bathroom/scene.xml')
#scene = mi.load_dict(mi.cornell_box())
print("Done.")

integrator =  mi.load_dict({
    'type': 'path',
})
img = mi.render(scene, integrator=integrator, spp=int(sys.argv[2]))
mi.Bitmap(img).write(f'tmp{sys.argv[1]}.exr')


### KernelHistory
kernels = dr.kernel_history()
filtered = []
keys = ['execution_time', 'codegen_time', 'backend_time']
for kernel in kernels:
    if kernel['type'] == dr.KernelType.JIT:
        new_kernel = {key: kernel[key] for key in keys}
        filtered.append(new_kernel)
print()
for kernel in filtered:
    print(kernel)
