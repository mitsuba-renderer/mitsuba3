import drjit as dr
import mitsuba as mi

mi.set_variant('cuda_ad_rgb')

#dr.set_log_level(dr.LogLevel.Debug)

#dr.set_flag(dr.JitFlag.Debug, True)
#dr.set_flag(dr.JitFlag.ReuseIndices, False)

target = dr.arange(mi.UInt32, 10) + 5
dr.make_opaque(target)

old_value = mi.UInt32(20, 20, 20,  8,  9, 20)
new_value = mi.UInt32(30, 30, 30, 13, 14, 20)
index =     mi.UInt32( 1,  0,  4,  3,  4,  5)


#dr.set_flag(dr.JitFlag.PrintIR, True)

old, swapped = dr.scatter_cas(target, old_value, new_value, index, True)
dr.eval(old, swapped)

# 6, 5, 9, 8, 9, 10
print(f"{old=}")

# False, Faslse, False, True, True, False
print(f"{swapped=}")

# 5, 6, 7, 13, 14, 10, 11, 12, 13, 14
print(f"{target=}")


old, swapped = dr.scatter_cas(target, old_value, new_value * 2, index, True)
print(f"{old=}")


