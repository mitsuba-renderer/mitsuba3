import json

print('Reading mitsuba.conf ..')
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

with open("include/mitsuba/core/config.h", 'w') as f:
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
    for index, (name, float_, spectrum) in enumerate(enabled):
        w('            %sif (strcmp(config, "%s") == 0)' %
          ("" if index == 0 else "else ", name))
        w('                return new Name<%s, %s>(props);'
          % (float_, spectrum))
    w('            else')
    w('                return nullptr;')
    w('        }')
    w('    }')
    w('')
    for index, (name, float_, spectrum) in enumerate(enabled):
        w('    template class MTS_EXPORT_CORE Name<%s, %s>;' %
          (float_, spectrum))
    f.write('\n\n')

