import mitsuba as mi

mi.set_variant('llvm_ad_rgb')

bsdf = mi.load_dict({
    'type': 'tinteddielectric',
    'eta': 1.2,
    'tint': [0.1, 0.8, 0.01]
})
print(f"{bsdf=}")

bsdf2 = mi.load_dict({
    'type': 'weirddielectric',
    'other_eta': 1.2,
    'other_tint': [0.1, 0.8, 0.01]
})
print(f"{bsdf2=}")
