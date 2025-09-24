import drjit as dr
import mitsuba as mi

mi.set_variant('cuda_ad_rgb')

ctr = mi.UInt32(0)
active = mi.Bool([True, False, True])
data_compact_1 = mi.Float(0, 1)
data_compact_2 = mi.Float(10, 11)



my_index = dr.scatter_inc(ctr, mi.UInt32(0), active)
dr.scatter(
    target=data_compact_1,
    value=mi.Float(2, 3, 4),
    index=my_index,
    active=active
)

yo = my_index * 2

dr.set_flag(dr.JitFlag.PrintIR, True)

dr.eval(data_compact_1) # Run Kernel #1
print(data_compact_1)


dr.eval(yo)
print(yo)
