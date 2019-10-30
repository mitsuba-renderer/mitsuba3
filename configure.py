import json

print('Reading mitsuba.conf...')
with open("mitsuba.conf", "r") as f:
    configurations = json.load(f)

enabled = []
for name, config in configurations.items():
    if type(config) is not dict:
        continue
    if 'enabled' not in config or \
       'float' not in config or \
       'spectrum' not in config:
        raise Exception('Invalid configuration entry "%s": must have '
                        '"enabled", "float", and "spectrum" fields!' % name)
    if not config['enabled']:
        continue
    enabled.append((name, config['float'], config['spectrum']))

if len(enabled) == 0:
    raise Exception("There must be at least one enabled build configuration!")

fname = "include/mitsuba/core/config.h"
with open(fname, 'w') as f:
    def w(s):
        f.write(s.ljust(75) + ' \\\n')
    f.write('#pragma once\n\n')

    w('#define MTS_CONFIGURATIONS')
    for index, (name, float_, spectrum) in enumerate(enabled):
        w('    "%s\\n"' % name)
    f.write('\n\n')

    w('#define MTS_INSTANTIATE_OBJECT(Name)')
    for index, (name, float_, spectrum) in enumerate(enabled):
        # TODO: this would be better with a `using` declaration
        spectrum = spectrum.replace('Float', float_)
        # TODO: which export module to use?
        w('    template class MTS_EXPORT_CORE Name<%s, %s>;' % (float_, spectrum))
    f.write('\n\n')

    w('#define MTS_INSTANTIATE_STRUCT(Name)')
    for index, (name, float_, spectrum) in enumerate(enabled):
        # TODO: this would be better with a `using` declaration
        spectrum = spectrum.replace('Float', float_)
        # TODO: which export module to use?
        w('    template struct MTS_EXPORT_CORE Name<%s, %s>;' % (float_, spectrum))
    f.write('\n\n')

    w('#define MTS_DECLARE_PLUGIN(Name, Parent)')
    w('    MTS_REGISTER_CLASS(Name, Parent)')
    w('    MTS_IMPORT_TYPES()')
    f.write('\n\n')

    w('#define MTS_IMPLEMENT_PLUGIN(Name, Parent, Descr)')
    w('    extern "C" {')
    w('        MTS_EXPORT const char *plugin_descr = Descr;')
    w('        MTS_EXPORT Object *plugin_create(const char *config,')
    w('                                         const Properties &props) {')
    w('            constexpr size_t PacketSize = enoki::max_packet_size / sizeof(float);')
    w('            ENOKI_MARK_USED(PacketSize);')
    for index, (name, float_, spectrum) in enumerate(enabled):
        w('            %sif (strcmp(config, "%s") == 0) {' %
          ("" if index == 0 else "} else ", name))
        w('                using Float = %s;' % float_)
        w('                return new Name<%s, %s>(props);'
          % (float_, spectrum))
    w('            } else {')
    w('                return nullptr;')
    w('            }')
    w('        }')
    w('    }')
    w('')
    w('    MTS_INSTANTIATE_OBJECT(Name)')
    f.write('\n\n')

    w('#define MTS_PY_EXPORT_VARIANTS(name)')
    w('    template <typename Float, typename Spectrum, typename name = mitsuba::name<Float, Spectrum>>')
    w('    void instantiate_##name(py::module m);')
    w('')
    w('    MTS_PY_EXPORT(name) {')
    for index, (name, float_, spectrum) in enumerate(enabled):
        spectrum = spectrum.replace('Float', float_)
        w('        instantiate_##name<%s, %s>(' % (float_, spectrum))
        w('            m.def_submodule("%s"));' % (name))
    w('    }')
    w('')
    w('    template <typename Float, typename Spectrum, typename name>')
    w('    void instantiate_##name(py::module m)')
    f.write('\n\n')

print('Generated configuration header: ' + fname)
