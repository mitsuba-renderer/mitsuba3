import json
from collections import OrderedDict

print('Reading mitsuba.conf...')
with open("mitsuba.conf", "r") as f:
    # Load while preserving order of keys
    configurations = json.load(f, object_pairs_hook=OrderedDict)

enabled = []
for name, config in configurations.items():
    if not isinstance(config, (dict, OrderedDict)):
        continue
    if 'enabled' not in config or \
       'float' not in config or \
       'spectrum' not in config:
        raise Exception('Invalid configuration entry "%s": must have '
                        '"enabled", "float", and "spectrum" fields!' % name)
    if not config['enabled']:
        continue
    spectrum = config['spectrum'].replace('Float', config['float'])
    enabled.append((name, config['float'], spectrum))

if len(enabled) == 0:
    raise Exception("There must be at least one enabled build configuration!")

# Use first configuration if default mode is not specified
default_mode = configurations.get('default', enabled[0][0])

# Write header file
fname = "include/mitsuba/core/config.h"
with open(fname, 'w') as f:
    def w(s):
        f.write(s.ljust(75) + ' \\\n')
    f.write('#pragma once\n\n')
    f.write('#include <mitsuba/core/fwd.h>\n\n')

    w('#define MTS_CONFIGURATIONS')
    for index, (name, float_, spectrum) in enumerate(enabled):
        w('    "%s\\n"' % name)
    f.write('\n\n')

    w('#define MTS_CONFIGURATIONS_INDENTED')
    for index, (name, float_, spectrum) in enumerate(enabled):
        w('    "            %s\\n"' % name)
    f.write('\n\n')

    w('#define MTS_DEFAULT_MODE "%s"' % default_mode)
    f.write('\n\n')

    w('#define MTS_INSTANTIATE_OBJECT(Name)')
    for index, (name, float_, spectrum) in enumerate(enabled):
        w('    template class MTS_EXPORT_RENDER Name<%s, %s>;' % (float_, spectrum))
    f.write('\n\n')

    w('#define MTS_INSTANTIATE_STRUCT(Name)')
    for index, (name, float_, spectrum) in enumerate(enabled):
        w('    template struct MTS_EXPORT_RENDER Name<%s, %s>;' % (float_, spectrum))
    f.write('\n\n')

    w('#define MTS_IMPLEMENT_PLUGIN(Name, Parent, Descr)')
    w('    MTS_EXPORT const char *plugin_descr = Descr;')
    w('    MTS_EXPORT const char *plugin_name = #Name;')
    w('    MTS_INSTANTIATE_OBJECT(Name)')
    f.write('\n\n')

    w('#define MTS_PY_EXPORT_VARIANTS(name)')
    w('    template <typename Float, typename Spectrum>')
    w('    void instantiate_##name(py::module m);')
    w('')
    w('    MTS_PY_EXPORT(name) {')
    for index, (name, float_, spectrum) in enumerate(enabled):
        w('        instantiate_##name<%s, %s>(' % (float_, spectrum))
        w('            m.def_submodule("%s"));' % (name))
    w('    }')
    w('')
    w('    template <typename Float, typename Spectrum>')
    w('    void instantiate_##name(py::module m)')
    f.write('\n\n')

    w('#define MTS_ROUTE_MODE(mode, function, ...)')
    w('    [&]() {')
    for index, (name, float_, spectrum) in enumerate(enabled):
        iff = 'if' if index == 0 else 'else if'
        w('        %s (mode == "%s")' % (iff, name))
        w('            return function<%s, %s>(__VA_ARGS__);' % (float_, spectrum))
    w('        else')
    w('            Throw("Unsupported mode: %s", mode);')
    w('    }()')
    f.write('\n\n')

    w('#define PY_CAST_VARIANTS(Name)')
    for index, (name, float_, spectrum) in enumerate(enabled):
        spectrum = spectrum.replace('Float', float_)
        w('    if (auto tmp = dynamic_cast<Name<%s, %s> *>(o))' % (float_, spectrum))
        w('        return py::cast(tmp);')
    f.write('\n\n')

    f.write('NAMESPACE_BEGIN(mitsuba)\n')
    f.write('NAMESPACE_BEGIN(detail)\n')
    f.write('template <typename Float, typename Spectrum_> constexpr const char *get_variant() {\n')
    for index, (name, float_, spectrum) in enumerate(enabled):
        f.write('    %sif constexpr (std::is_same_v<Float, %s> && std::is_same_v<Spectrum_, %s>)\n' % ('else ' if index > 0 else '', float_, spectrum))
        f.write('        return "%s";\n' % name)
    f.write('    else\n')
    f.write('        return "";\n')
    f.write('}\n')
    f.write('NAMESPACE_END(detail)')
    f.write('NAMESPACE_END(mitsuba)')

print('Generated configuration header: ' + fname)
