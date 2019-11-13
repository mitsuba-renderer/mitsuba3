from collections import OrderedDict
import json
import os
from os.path import join, dirname, realpath
try:
    from io import StringIO
except ImportError:
    from StringIO import StringIO

ROOT = realpath(dirname(dirname(__file__)))

def write_config(f):
    with open(join(ROOT, "mitsuba.conf"), "r") as conf:
        # Load while preserving order of keys
        configurations = json.load(conf, object_pairs_hook=OrderedDict)

    # Let's start with some validation
    assert 'enabled' in configurations and 'default' in configurations
    assert isinstance(configurations['default'], str)
    assert isinstance(configurations['enabled'], list)

    # Extract enabled configurations
    enabled = []
    float_types = set()
    for name in configurations['enabled']:
        if name not in configurations:
            raise ValueError('"enabled" refers to an unknown configuration "%s"' % name)
        item = configurations[name]
        spectrum = item['spectrum'].replace('Float', item['float'])
        float_types.add(item['float'])
        enabled.append((name, item['float'], spectrum))

    if not enabled:
        raise ValueError("There must be at least one enabled build configuration!")

    # Use first configuration if default mode is not specified
    default_mode = configurations.get('default', enabled[0][0])

    # Write header file
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
        w('    template class MTS_EXPORT Name<%s, %s>;' % (float_, spectrum))
        if float_.startswith("Packet"):
            float_x = "DynamicArray<%s>" % float_
            spectrum_x = spectrum.replace(float_, float_x)
            w('    template class MTS_EXPORT Name<%s, %s>;' % (float_x, spectrum_x))
    f.write('\n\n')

    w('#define MTS_INSTANTIATE_STRUCT(Name)')
    for index, (name, float_, spectrum) in enumerate(enabled):
        w('    template struct MTS_EXPORT Name<%s, %s>;' % (float_, spectrum))
        if float_.startswith("Packet"):
            float_x = "DynamicArray<%s>" % float_
            spectrum_x = spectrum.replace(float_, float_x)
            w('    template struct MTS_EXPORT Name<%s, %s>;' % (float_x, spectrum_x))
    f.write('\n\n')

    w('#define MTS_EXTERN_STRUCT(Name)')
    for index, (name, float_, spectrum) in enumerate(enabled):
        w('    extern template struct MTS_EXPORT Name<%s, %s>;' % (float_, spectrum))
        if float_.startswith("Packet"):
            float_x = "DynamicArray<%s>" % float_
            spectrum_x = spectrum.replace(float_, float_x)
            w('    extern template struct MTS_EXPORT Name<%s, %s>;' % (float_x, spectrum_x))
    f.write('\n\n')

    w('#define MTS_EXTERN_CLASS(Name)')
    for index, (name, float_, spectrum) in enumerate(enabled):
        w('    extern template class MTS_EXPORT Name<%s, %s>;' % (float_, spectrum))
        if float_.startswith("Packet"):
            float_x = "DynamicArray<%s>" % float_
            spectrum_x = spectrum.replace(float_, float_x)
            w('    extern template class MTS_EXPORT Name<%s, %s>;' % (float_x, spectrum_x))
    f.write('\n\n')

    w('#define MTS_EXTERN_STRUCT_FLOAT(Name)')
    for float_ in float_types:
        w('    extern template struct MTS_EXPORT Name<%s>;' % float_)
        if float_.startswith("Packet"):
            float_x = "DynamicArray<%s>" % float_
            w('    extern template struct MTS_EXPORT Name<%s>;' % float_x)
    f.write('\n\n')

    w('#define MTS_EXTERN_CLASS_FLOAT(Name)')
    for float_ in float_types:
        w('    extern template class MTS_EXPORT Name<%s>;' % float_)
        if float_.startswith("Packet"):
            float_x = "DynamicArray<%s>" % float_
            w('    extern template class MTS_EXPORT Name<%s>;' % float_x)
    f.write('\n\n')

    w('#define MTS_IMPLEMENT_PLUGIN(Name, Parent, Descr)')
    w('    extern "C" {')
    w('        MTS_EXPORT const char *plugin_name() { return #Name; }')
    w('        MTS_EXPORT const char *plugin_descr() { return Descr; }')
    w('    }')
    w('    MTS_INSTANTIATE_OBJECT(Name)')
    f.write('\n\n')

    w('#define MTS_PY_DECLARE_VARIANTS(name)')
    for index, (name, float_, spectrum) in enumerate(enabled):
        w('    extern void python_export_variants_%s_##name(py::module &);' % (name))
    f.write('\n\n')

    w('#define MTS_PY_DEF_SUBMODULE_VARIANTS(lib)')
    for index, (name, float_, spectrum) in enumerate(enabled):
        w('    auto __submodule__%s =  m.def_submodule("%s").def_submodule(#lib);' % (name, name))
    f.write('\n\n')

    w('#define MTS_PY_IMPORT_VARIANTS(name)')
    for index, (name, float_, spectrum) in enumerate(enabled):
        w('    python_export_variants_%s_##name(__submodule__%s);' % (name, name))
    f.write('\n\n')

    w('#define MTS_PY_EXPORT_VARIANTS(name)')
    w('    template <typename Float, typename Spectrum,')
    w('              typename FloatP, typename SpectrumP>')
    w('    void instantiate_##name(py::module m);')
    w('')
    for index, (name, float_, spectrum) in enumerate(enabled):
        # for packets of float, the bindings should use the vectorize wrapper on dynamic arrays
        float_p = float_
        spectrum_p = spectrum
        if float_.startswith("Packet"):
            float_x = "DynamicArray<%s>" % float_
            spectrum = spectrum.replace(float_, float_x)
            float_ = float_x
        w('    void python_export_variants_%s_##name(py::module &m) {' % (name))
        w('        instantiate_##name<%s, %s, %s, %s>(m);' % (float_, spectrum, float_p, spectrum_p))
        w('    }')
    w('')
    w('    template <typename Float, typename Spectrum,')
    w('              typename FloatP, typename SpectrumP>')
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
        w('    if (auto tmp = dynamic_cast<Name<%s, %s> *>(o); tmp)' % (float_, spectrum))
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
    f.write('NAMESPACE_END(detail)\n')
    f.write('NAMESPACE_END(mitsuba)\n')


def write_to_file_if_changed(filename, contents):
    """Writes the given contents to file, only if they do not already match."""
    if os.path.isfile(filename):
        with open(filename, 'r') as f:
            existing = f.read()
            if existing == contents:
                return False

    with open(filename, 'w') as f:
        f.write(contents)
    print('Generated configuration header: ' + filename)


def main():
    fname = realpath(join(ROOT, "include/mitsuba/core/config.h"))
    output = StringIO()
    write_config(output)
    write_to_file_if_changed(fname, output.getvalue())

if __name__ == '__main__':
    main()
