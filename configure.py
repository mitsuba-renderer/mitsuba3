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
    w('#define MTS_CONFIGURATIONS')
    for index, (name, float_, spectrum) in enumerate(enabled):
        w('    "%s\\n"' % name)
    f.write('\n\n')

    w('#define MTS_DECLARE_PLUGIN()')
    w('    MTS_DECLARE_CLASS()')
    w('    MTS_IMPORT_TYPES()')
    f.write('\n\n')

    w('#define MTS_IMPLEMENT_PLUGIN(Name, Parent, Descr)')
    w('    MTS_IMPLEMENT_CLASS_TEMPLATE(Name, Parent)')
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
    for index, (name, float_, spectrum) in enumerate(enabled):
        # TODO: this would be better with a `using` declaration
        spectrum = spectrum.replace('Float', float_)
        w('    template class MTS_EXPORT_CORE Name<%s, %s>;' % (float_, spectrum))
    f.write('\n\n')

    w('#define MTS_IMPLEMENT_OBJECT(Name, Parent)')
    w('    MTS_IMPLEMENT_CLASS_TEMPLATE(Name, Parent)')
    for index, (name, float_, spectrum) in enumerate(enabled):
        # TODO: this would be better with a `using` declaration
        spectrum = spectrum.replace('Float', float_)
        # TODO: which export module to use? Is this instantiation useful?
        w('    template class MTS_EXPORT_CORE Name<%s, %s>;' % (float_, spectrum))

    f.write('\n')
print('Generated configuration header: ' + fname)
