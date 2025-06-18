import mitsuba as mi
mi.set_variant('cuda_ad_rgb')

tmp = mi.load_dict({
 'type': 'custom_sensor'
})
