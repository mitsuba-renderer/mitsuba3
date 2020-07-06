import numpy as np
import os
from shutil import copy2

class Files:
    '''
    Enum for different files or dicts containing specific info
    '''
    MAIN = 0 # Either all, or defaults and includes
    MATS = 1 # Materials
    GEOM = 2 # Geometry
    EMIT = 3 # Emitters
    CAMS = 4 # Camera and rendering params (sensor, integrator, sampler...)
    #TODO: Volumes

class WriteXML:
    '''
    File Writing API
    Populates a dictionary with scene data, then writes it to XML.
    '''

    # List of defaults to add to the scene dict (see self.configure_defaults)
    defaults = { # Arg name: default name
        'sample_count': 'spp',
        'width': 'resx',
        'height': 'resy'
    }

    def __init__(self, path, split_files=False):
        from mitsuba import variant
        if not variant():
            mitsuba.set_variant('scalar_rgb')
        from mitsuba.core import PluginManager
        self.pmgr = PluginManager.instance()
        self.split_files = split_files
        self.scene_data = [{'type': 'scene'}, #MAIN
                           {}, #MATS
                           {}, #GEOM
                           {}, #EMIT
                           {}] #CAMS
        self.com_count = 0 # Counter for comment ids
        self.exported_ids = set()
        self.copy_count = {} # Counters for giving unique names to copied files with duplicate names
        self.copied_paths = {}
        self.files = []
        self.file_names = [] # Relative paths to the fragment files
        self.file_tabs = []
        self.file_stack = []
        self.current_file = Files.MAIN
        self.directory = '' # scene foler
        self.set_filename(path)

    def data_add(self, key, value, file=Files.MAIN):
        '''
        Add an entry to a given subdict.

        Params
        ------

        key: dict key
        value: entry
        file: the subdict to which to add the data
        '''
        self.scene_data[file].update([(key, value)])

    def add_comment(self, comment, file=Files.MAIN):
        '''
        Add a comment to the scene dict

        Params
        ------

        comment: text of the comment
        file: the subdict to which to add the comment
        '''
        key = "__com__%d" % self.com_count
        self.com_count += 1
        self.data_add(key,{'type':'comment', 'value': comment}, file)

    def add_include(self, file):
        '''
        Add an include tag to the main file.
        This is used when splitting the XML scene file in multiple fragments.

        Params
        ------

        file: the file to include
        '''
        key = "__include__%d" % file
        value = {'type':'include', 'filename':self.file_names[file]}
        self.data_add(key, value) # Add to main

    def wf(self, ind, st, tabs=0):
        '''
        Write a string to file index ind.
        Optionally indent the string by a number of tabs

        Params
        ------

        ind: index of the file to write to
        st: text to write
        tabs: optional number of tabs to add
        '''

        # Prevent trying to write to a file that isn't open
        if self.files[ind] is None:
            ind = 0

        self.files[ind].write('%s%s' % ('\t' * tabs, st))
        self.files[ind].flush()

    def set_filename(self, name):
        '''
        Open the files for output,
        using filenames based on the given base name.
        Create the necessary folders to create the file at the specified path.

        Params
        ------

        name: path to the scene.xml file to write.
        '''
        # If any files happen to be open, close them and start again
        for f in self.files:
            if f is not None:
                f.close()

        self.files = []
        self.file_names = []
        self.file_tabs = []
        self.file_stack = []

        self.directory, main_file = os.path.split(name)
        base_name = os.path.splitext(main_file)[0] # Remove the extension

        if not os.path.isdir(self.directory):
            os.makedirs(self.directory)

        self.file_names.append(name)
        self.files.append(open(self.file_names[Files.MAIN], 'w', encoding='utf-8', newline="\n"))
        self.file_tabs.append(0)
        self.file_stack.append([])
        if self.split_files:
            self.write_header(Files.MAIN, '# Main Scene File')
        else:
            self.write_header(Files.MAIN)


        print('Scene File: %s' % self.file_names[Files.MAIN])
        print('Scene Folder: %s' % self.directory)

        # Set texture directory name
        self.textures_folder = os.path.join(self.directory, "textures")
        # Create geometry export directory
        geometry_folder = os.path.join(self.directory, "meshes")
        if not os.path.isdir(geometry_folder):
            os.mkdir(geometry_folder)

        if self.split_files:
            fragments_folder = os.path.join(self.directory, "fragments")
            if not os.path.isdir(fragments_folder):
                os.mkdir(fragments_folder)

            self.file_names.append('fragments/%s-materials.xml' % base_name)
            self.files.append(open(os.path.join(self.directory, self.file_names[Files.MATS]), 'w', encoding='utf-8', newline="\n"))
            self.file_tabs.append(0)
            self.file_stack.append([])
            self.write_header(Files.MATS, '# Materials File')

            self.file_names.append('fragments/%s-geometry.xml' % base_name)
            self.files.append(open(os.path.join(self.directory, self.file_names[Files.GEOM]), 'w', encoding='utf-8', newline="\n"))
            self.file_tabs.append(0)
            self.file_stack.append([])
            self.write_header(Files.GEOM, '# Geometry File')

            self.file_names.append('fragments/%s-emitters.xml' % base_name)
            self.files.append(open(os.path.join(self.directory, self.file_names[Files.EMIT]), 'w', encoding='utf-8', newline="\n"))
            self.file_tabs.append(0)
            self.file_stack.append([])
            self.write_header(Files.EMIT, '# Emitters File')

            self.file_names.append('fragments/%s-render.xml' % base_name)
            self.files.append(open(os.path.join(self.directory, self.file_names[Files.CAMS]), 'w', encoding='utf-8', newline="\n"))
            self.file_tabs.append(0)
            self.file_stack.append([])
            self.write_header(Files.CAMS, '# Cameras and Render Parameters File')

        self.set_output_file(Files.MAIN)

    def set_output_file(self, file):
        '''
        Switch next output to the given file index

        Params
        ------

        file: index of the file to start writing to
        '''

        self.current_file = file

    def write_comment(self, comment, file=None):
        '''
        Write an XML comment to file.

        Params
        ------

        comment: The text of the comment to write
        file: Index of the file to write to
        '''
        if not file:
            file = self.current_file
        self.wf(file, '\n')
        self.wf(file, '<!-- %s -->\n' % comment)
        self.wf(file, '\n')

    def write_header(self, file, comment=None):
        '''
        Write an XML header to a specified file.
        Optionally add a comment to describe the file.

        Params
        ------

        file: The file to write to
        comment: Optional comment to add (e.g. "# Geometry file")
        '''
        if comment:
            self.write_comment(comment, file)

    def open_element(self, name, attributes={}, file=None):
        '''
        Open an XML tag (e.g. emitter, bsdf...)

        Params
        ------

        name: Name of the tag (emitter, bsdf, shape...)
        attributes: Additional fileds to add to the opening tag (e.g. name, type...)
        file: File to write to
        '''
        if file is not None:
            self.set_output_file(file)

        self.wf(self.current_file, '<%s' % name, self.file_tabs[self.current_file])

        for (k, v) in attributes.items():
            self.wf(self.current_file, ' %s=\"%s\"' % (k, v.replace('"', '')))

        self.wf(self.current_file, '>\n')

        # Indent
        self.file_tabs[self.current_file] = self.file_tabs[self.current_file] + 1
        self.file_stack[self.current_file].append(name)

    def close_element(self, file=None):
        '''
        Close the last tag we opened in a given file.

        Params
        ------

        file: The file to write to
        '''
        if file is not None:
            self.set_output_file(file)

        # Un-indent
        self.file_tabs[self.current_file] = self.file_tabs[self.current_file] - 1
        name = self.file_stack[self.current_file].pop()

        self.wf(self.current_file, '</%s>\n' % name, self.file_tabs[self.current_file])

    def element(self, name, attributes={}, file=None):
        '''
        Write a single-line XML element.

        Params
        ------

        name: Name of the element (e.g. integer, string, rotate...)
        attributes: Additional fields to add to the element (e.g. name, value...)
        file: The file to write to
        '''
        if file is not None:
            self.set_output_file(file)

        self.wf(self.current_file, '<%s' % name, self.file_tabs[self.current_file])

        for (k, v) in attributes.items():
            self.wf(self.current_file, ' %s=\"%s\"' % (k, v))

        self.wf(self.current_file, '/>\n')

    def get_plugin_tag(self, plugin_type):
        '''
        Get the corresponding tag of a given plugin (e.g. 'bsdf' for 'diffuse')
        If the given type (e.g. 'transform') is not a plugin, returns None.

        Params
        ------

        plugin_type: Name of the type (e.g. 'diffuse', 'ply'...)
        '''
        from mitsuba import variant
        class_ =  self.pmgr.get_plugin_class(plugin_type, variant())
        if not class_: # If get_plugin_class returns None, there is not corresponding plugin
            return None
        class_ = class_.parent()
        while class_.alias() == class_.name():
            class_ = class_.parent()
        return class_.alias()

    def current_tag(self):
        '''
        Get the tag in which we are currently writing
        '''
        return self.file_stack[self.current_file][-1]

    def configure_defaults(self, scene_dict):
        '''
        Traverse the scene graph and look for properties in the defaults dict.
        For such properties, store their value in a default tag and replace the value by $name in the prop.

        Params
        ------

        scene_dict: The dictionary containing the scene info
        '''
        scene_dict = scene_dict.copy()
        for key, value in scene_dict.items():
            if isinstance(value, dict):
                scene_dict[key] = self.configure_defaults(value)
            elif key in self.defaults:
                if not '$%s'%self.defaults[key] in self.scene_data[Files.MAIN]:
                    # Store the value of the first occurence of the parameter in the default
                    params = {
                        'type': 'default',
                        'name': self.defaults[key],
                        'value': value
                    }
                    self.data_add('$%s'%self.defaults[key], params)
                if isinstance(value, int):
                    scene_dict[key] = {'type': 'integer'}
                elif isinstance(value, float):
                    scene_dict[key] = {'type': 'float'}
                elif isinstance(value, str):
                    scene_dict[key] = {'type': 'string'}
                elif isinstance(value, bool):
                    scene_dict[key] = {'type': 'boolean'}
                else:
                    raise ValueError("Unsupported default type: %s" % value)
                #TODO: for now, the onlydefaults we add are ints, so that works. This may not always be the case though
                if 'name' not in scene_dict[key]:
                    scene_dict[key]['name'] = key
                scene_dict[key]['value'] = '$%s' % self.defaults[key]
        return scene_dict

    def preprocess_scene(self, scene_dict):
        '''
        Preprocess the scene dictionary before writing it to file:
            - Add default properties.
            - Reorder the scene dict before writing it to file.
            - Separate the dict into different category-specific subdicts.
            - If not splitting files, merge them in the end.

        Params
        ------

        scene_dict: The dictionary containing the scene data
        '''

        if self.split_files:
            for dic in self.scene_data[1:]:
                dic.update([('type','scene')])

        if 'type' not in scene_dict:
            raise ValueError("Missing key: 'type'!")
        if scene_dict['type'] != 'scene': # We try to save a plugin, not a scene
            self.data_add('plugin', scene_dict, Files.MAIN) # In this case, don't bother with multiple files
            return

        # Add defaults to MAIN file
        self.add_comment("Defaults, these can be set via the command line: -Darg=value")
        scene_dict = self.configure_defaults(scene_dict)
        for key, value in scene_dict.items():
            if key == 'type':
                continue # Ignore 'scene' tag

            if isinstance(value, dict):# Should always be the case
                item_type = value['type']

                tag = self.get_plugin_tag(item_type)
                if tag:
                    if tag == 'emitter':
                        self.data_add(key, value, Files.EMIT)
                    elif tag == 'shape':
                        if 'emitter' in value.keys(): # Emitter nested in a shape (area light)
                            self.data_add(key, value, Files.EMIT)
                        else:
                            self.data_add(key ,value, Files.GEOM)
                    elif tag == 'bsdf':
                        self.data_add(key, value, Files.MATS)
                    else: # The rest is sensor, integrator and other render stuff
                        self.data_add(key, value, Files.CAMS)
                else:
                    self.data_add(key, value, Files.MAIN)
            else:
                raise ValueError("Unsupported item: %s:%s" % (key,value))

        # Fill the main file either with includes or with the ordered XML tags.
        self.add_comment("Camera and Rendering Parameters")
        if self.split_files:
            self.add_include(Files.CAMS)
        else:
            self.scene_data[Files.MAIN].update(self.scene_data[Files.CAMS])
        # Either include subfiles or add the corresponding data to the MAIN dict so data is ordered in the scene file
        self.add_comment("Materials")
        if self.split_files:
            self.add_include(Files.MATS)
        else:
            self.scene_data[Files.MAIN].update(self.scene_data[Files.MATS])

        self.add_comment("Emitters")

        if self.split_files:
            self.add_include(Files.EMIT)
        else:
            self.scene_data[Files.MAIN].update(self.scene_data[Files.EMIT])

        self.add_comment("Shapes")
        if self.split_files:
            self.add_include(Files.GEOM)
        else:
            self.scene_data[Files.MAIN].update(self.scene_data[Files.GEOM])

        self.set_output_file(Files.MAIN)

    def format_spectrum(self, entry, entry_type):
        '''
        Format rgb or spectrum tags to the proper XML output.
        The entry should contain the name and value of the spectrum entry.
        The type is passed separately, since it is popped from the dict in write_dict

        Params
        ------

        entry: the dict containing the spectrum
        entry_type: either 'spectrum' or 'rgb'
        '''
        from mitsuba.core import Color3f
        if entry_type == 'rgb':
            if len(entry.keys()) != 2 or 'value' not in entry:
                raise ValueError("Invalid entry of type rgb: %s" % entry)
            elif isinstance(entry['value'], (float, int)):
                entry['value'] = "%f" % entry['value']
            elif isinstance(entry['value'], (list, Color3f, np.ndarray)) and len(entry['value']) == 3:
                entry['value'] = "%f %f %f" % tuple(entry['value'])
            else:
                raise ValueError("Invalid value for type rgb: %s" % entry)

        elif entry_type == 'spectrum':
            if len(entry.keys()) != 2:
                raise ValueError("Dict of type 'spectrum': %s has to many entries!" % entry)
            if 'filename' in entry:
                entry['filename'] = self.format_path(entry['filename'], 'spectrum')
            elif 'value' in entry:
                spval = entry['value']
                if isinstance(spval, float):
                    #uniform spectrum
                    entry['value'] = "%f" % spval
                elif isinstance(spval, list):
                    #list of wavelengths
                    if any(spval[i][0] >= spval[i+1][0] for i in range(len(spval)-1)):
                        raise ValueError("Wavelengths must be sorted in strictly increasing order!")
                    try:
                        entry['value'] = ', '.join(["%f:%f" % (x[0],x[1]) for x in spval])
                    except IndexError:
                        raise ValueError("Invalid entry in 'spectrum' wavelength list: %s" % spval)
                else:
                    raise ValueError("Invalid value type in 'spectrum' dict: %s" % spval)
            else:
                raise ValueError("Invalid key in 'spectrum' dict: %s" % entry)

        return entry_type, entry

    def format_path(self, filepath, tag):
        '''
        Given a filepath, either copy it in the scene folder (in the corresponding directory)
        or convert it to a relative path.

        Params
        ------

        filepath: the path to the given file
        tag: the tag this path property belongs to in (shape, texture, spectrum)
        '''
        #TODO: centralize subdir names somewhere
        subfolders = {'texture': 'textures', 'emitter': 'textures', 'shape': 'meshes', 'spectrum': 'spectra'}

        if tag not in subfolders:
            raise ValueError("Unsupported tag for a filename: %s" % tag)
        abs_path = os.path.join(self.directory, subfolders[tag])
        filepath = os.path.abspath(filepath) # We may have a relative path here, convert to absolute first
        if not os.path.isfile(filepath):
            raise ValueError("File '%s' not found!" % filepath)
        if abs_path in filepath: # The file is at the proper place already
            return os.path.relpath(filepath, self.directory)
        else: # We need to copy the file in the scene directory
            if filepath in self.copied_paths: # File was already copied, don't copy it again
                return self.copied_paths[filepath]
            if not os.path.isdir(abs_path):
                os.mkdir(abs_path)
            base_name = os.path.basename(filepath)
            name, ext = os.path.splitext(base_name)
            if base_name in self.copy_count: # Add a number at the end of the name, since it already exists
                name = "%s(%d)" % (name, self.copy_count[base_name])
                self.copy_count[base_name] += 1
            else:
                self.copy_count[base_name] = 1
            target_path = os.path.join(abs_path, "%s%s" % (name, ext))
            copy2(filepath, target_path)
            rel_path = os.path.relpath(target_path, self.directory)
            self.copied_paths[filepath] = rel_path
            return rel_path

    def write_dict(self, data):
        '''
        Main XML writing routine.
        Given a dictionary, iterate over its entries and write them to file.
        Calls itself for nested dictionaries.

        Params
        ------

        data: The dictionary to write to file.
        '''
        from mitsuba.core import Transform4f, Point3f

        if 'type' in data: # Scene tag
            self.open_element(data.pop('type'), {'version': '2.1.0'})

        for key, value in data.items():
            if isinstance(value, dict):
                value = value.copy()
                try:
                    value_type = value.pop('type')
                except KeyError:
                    raise ValueError("Missing key 'type'!")
                tag = self.get_plugin_tag(value_type)
                if value_type in {'rgb', 'spectrum'}:
                    value['name'] = key
                    name, args = self.format_spectrum(value, value_type)
                    self.element(name, args)
                elif value_type == 'comment':
                    self.write_comment(value['value'])
                elif tag: # Plugin
                    args = {'type': value_type}
                    if tag == 'texture' and self.current_tag() != 'scene':
                        args['name'] = key
                    if 'id' in value:
                        args['id'] = value.pop('id')
                    elif key[:7] != '__elm__' and self.current_tag() == 'scene':
                        args['id'] = key # Top level keys are IDs, lower level ones are param names
                    if 'id' in args:
                        if args['id'] in self.exported_ids:
                            raise ValueError("Id: %s is already used!" % args['id'])
                        self.exported_ids.add(args['id'])
                    if len(value) > 0: # Open a tag if there is still stuff to write
                        self.open_element(tag, args)
                        self.write_dict(value)
                        self.close_element()
                    else:
                        self.element(tag, args) # Write dict in one line (e.g. integrator)
                else:
                    if value_type == 'ref':
                        if 'name' not in value:
                            value['name'] = key
                        if value['id'] not in self.exported_ids:
                            raise ValueError("Id: %s referenced before export." % value['id'])
                    self.element(value_type, value)
            elif isinstance(value, str):
                if os.path.exists(value):
                    # Copy path if necessary and convert it to relative
                    value = self.format_path(value, self.current_tag())
                self.element('string', {'name':key, 'value': '%s' % value})
            elif isinstance(value, bool):
                self.element('boolean', {'name':key, 'value': str(value).lower()})
            elif isinstance(value, int):
                self.element('integer', {'name':key, 'value': '%d' % value})
            elif isinstance(value, float):
                self.element('float', {'name':key, 'value': '%f' % value})
            elif any(isinstance(value, x) for x in [list, Point3f, np.ndarray]):
                # Cast to point
                if len(value) == 3:
                    args = {'name': key, 'x' : value[0] , 'y' :  value[1] , 'z' :  value[2]}
                    self.element('point', args)
                else:
                    raise ValueError("Expected 3 values for a point. Got %d instead." % len(value))
            elif isinstance(value, Transform4f):
                # In which plugin are we adding a transform?
                parent_plugin = self.current_tag()
                if parent_plugin == 'sensor':
                    # Decompose into rotation and translation
                    params = self.decompose_transform(value)
                else:
                    # Simply write the matrix
                    params = self.transform_matrix(value)
                self.open_element('transform', {'name': key})
                self.write_dict(params)
                self.close_element()
            else:
                raise ValueError("Unsupported entry: (%s,%s)" % (key,value))

        if len(self.file_stack[self.current_file]) == 1:
            # Close scene tag
            self.close_element()

    def process(self, scene_dict):
        '''
        Preprocess then write the input dict to XML file format

        Params
        ------

        scene_dict: The dictionary containing all the scene info.
        '''
        self.preprocess_scene(scene_dict) # Re order elements
        if self.split_files:
            for file in [Files.MAIN, Files.CAMS, Files.MATS, Files.GEOM, Files.EMIT]:
                self.set_output_file(file)
                self.write_dict(self.scene_data[file])
        else:
            self.write_dict(self.scene_data[Files.MAIN])

        # Close files
        print('Wrote scene files.')
        for f in self.files:
            if f is not None:
                f.close()
                print(' %s' % f.name)

    def exit(self):
        # If any files happen to be open, close them and start again
        for f in self.files:
            if f is not None:
                f.close()

    def transform_matrix(self, transform):
        '''
        Converts a mitsuba Transform4f into a dict entry.
        This dict entry won't have a 'type' because it's handled in a specific case.

        Params
        ------

        transform: the given transform matrix
        '''

        value = " ".join(["%f" % x for x in transform.matrix.numpy().flatten()])

        params = {
            'matrix': {
                'type': 'matrix',
                'value': value,
            }
        }
        return params

    def decompose_transform(self, transform, export_scale = False):
        '''
        Export a transform as a combination of rotation, scale and translation.
        This helps manually modifying the transform after export (for cameras for instance)

        Params
        ------

        transform: The Transform4f transform matrix to decompose
        export_scale: Whether to add a scale property or not. (e.g. don't do it for cameras to avoid clutter)
        '''
        from enoki import transform_decompose, quat_to_euler
        scale, quat, trans = transform_decompose(transform.matrix)
        rot = quat_to_euler(quat)
        params = {}
        if rot[0] != 0.0:
            params['rotate_x'] = {
                'type': 'rotate',
                'x': '1',
                'angle': rot[0] * 180 / np.pi
            }
        if rot[1] != 0.0:
            params['rotate_y'] = {
                'type': 'rotate',
                'y': '1',
                'angle': rot[1] * 180 / np.pi
            }
        if rot[2] != 0.0:
            params['rotate_z'] = {
                'type': 'rotate',
                'z': '1',
                'angle': rot[2] * 180 / np.pi
            }
        if export_scale and scale != 1.0:
            params['scale'] = {
                'type': 'scale',
                'value': "%f %f %f" % (scale[0,0], scale[1,1], scale[2,2])
            }
        if trans != 0.0:
            params['translate'] = {
                'type': 'translate',
                'value': "%f %f %f" % (trans[0], trans[1], trans[2])
            }

        return params

def dict_to_xml(scene_dict, filename, split_files=False):
    writer = WriteXML(filename, split_files)
    writer.process(scene_dict)
