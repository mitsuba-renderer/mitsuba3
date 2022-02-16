from __future__ import annotations # Delayed parsing of type annotations

from collections.abc import Mapping
from contextlib import contextmanager

import drjit as dr
import mitsuba as mi

class SceneParameters(Mapping):
    """
    Dictionary-like object that references various parameters used in a Mitsuba
    scene graph. Parameters can be read and written using standard syntax
    (``parameter_map[key]``). The class exposes several non-standard functions,
    specifically :py:meth:`~mitsuba.SceneParameters.torch()`,
    :py:meth:`~mitsuba.SceneParameters.update()`, and
    :py:meth:`~mitsuba.SceneParameters.keep()`.
    """

    def __init__(self, properties=None, hierarchy=None):
        """
        Private constructor (use
        :py:func:`mitsuba.traverse()` instead)
        """
        self.properties = properties if properties is not None else {}
        self.hierarchy  = hierarchy  if hierarchy  is not None else {}
        self.update_list = {}

        self.set_property = mi.set_property
        self.get_property = mi.get_property

    def copy(self):
        return SceneParameters(
            dict(self.properties),
            dict(self.hierarchy))

    def __contains__(self, key: str):
        return self.properties.__contains__(key)

    def __getitem__(self, key: str):
        value, value_type, node, _ = self.properties[key]
        if value_type is None:
            return value
        else:
            return self.get_property(value, value_type, node)

    def __setitem__(self, key: str, value):
        cur, value_type, node, _ = self.set_dirty(key)
        if value_type is None:
            cur.assign(value)
        else:
            self.set_property(cur, value_type, value)

    def __delitem__(self, key: str) -> None:
        del self.properties[key]

    def __len__(self) -> int:
        return len(self.properties)

    def __repr__(self) -> str:
        param_list = '\n'
        for k, v in self.items():
            def is_diff(x):
                return dr.is_diff_array_v(x) and dr.is_floating_point_v(x)
            diff = is_diff(v)
            if dr.is_drjit_struct_v(v):
                for k2 in type(v).DRJIT_STRUCT.keys():
                    diff |= is_diff(getattr(v, k2))
            param_list += '  %s %s,\n' % (('*' if is_diff else ' '), k)
        return 'SceneParameters[%s\n]' % param_list[:-2]

    def __iter__(self):
        class SceneParametersItemIterator:
            def __init__(self, pmap):
                self.pmap = pmap
                self.it = pmap.keys().__iter__()

            def __iter__(self):
                return self

            def __next__(self):
                key = next(self.it)
                return (key, self.pmap[key])

        return SceneParametersItemIterator(self)

    def items(self):
        return self.__iter__()

    def keys(self):
        return self.properties.keys()

    def flags(self, key: str):
        """Return parameter flags"""
        return self.properties[key][3]

    def set_dirty(self, key: str):
        """
        Marks a specific parameter and its parent objects as dirty. A subsequent call
        to :py:meth:`~mitsuba.SceneParameters.update()` will refresh their internal
        state. This function is automatically called when overwriting a parameter using
        :py:meth:`~mitsuba.SceneParameters.__setitem__()`.
        """
        item = self.properties[key]
        node = item[2]
        while node is not None:
            parent, depth = self.hierarchy[node]

            name = key
            if parent is not None:
                key, name = key.rsplit('.', 1)

            self.update_list.setdefault((depth, node), [])
            self.update_list[(depth, node)].append(name)

            node = parent

        return item

    def update(self) -> None:
        """
        This function should be called at the end of a sequence of writes
        to the dictionary. It automatically notifies all modified Mitsuba
        objects and their parent objects that they should refresh their
        internal state. For instance, the scene may rebuild the kd-tree
        when a shape was modified, etc.
        """
        work_list = [(d, n, k) for (d, n), k in self.update_list.items()]
        work_list = reversed(sorted(work_list, key=lambda x: x[0]))
        for depth, node, keys in work_list:
            node.parameters_changed(keys)
        self.update_list.clear()
        dr.eval()

    def keep(self, keys: list) -> None:
        """
        Reduce the size of the dictionary by only keeping elements,
        whose keys are part of the provided list 'keys'.
        """
        assert isinstance(keys, list)

        keys = set(keys)
        self.properties = {
            k: v for k, v in self.properties.items() if k in keys
        }


def traverse(node: mi.Object) -> SceneParameters:
    """
    Traverse a node of Mitsuba's scene graph and return a dictionary-like
    object that can be used to read and write associated scene parameters.

    See also :py:class:`mitsuba.SceneParameters`.
    """

    class SceneTraversal(mi.TraversalCallback):
        def __init__(self, node, parent=None, properties=None,
                     hierarchy=None, prefixes=None, name=None, depth=0,
                     flags=+mi.ParamFlags.Empty):
            mi.TraversalCallback.__init__(self)
            self.properties = dict() if properties is None else properties
            self.hierarchy = dict() if hierarchy is None else hierarchy
            self.prefixes = set() if prefixes is None else prefixes

            if name is not None:
                ctr, name_len = 1, len(name)
                while name in self.prefixes:
                    name = "%s_%i" % (name[:name_len], ctr)
                    ctr += 1
                self.prefixes.add(name)

            self.name = name
            self.node = node
            self.depth = depth
            self.hierarchy[node] = (parent, depth)
            self.flags = flags

        def put_parameter(self, name, ptr, cpptype=None, flags=+mi.ParamFlags.Empty):
            name = name if self.name is None else self.name + '.' + name
            self.properties[name] = (ptr, cpptype, self.node, self.flags | flags)

        def put_object(self, name, node, flags):
            if node in self.hierarchy:
                return
            cb = SceneTraversal(
                node=node,
                parent=self.node,
                properties=self.properties,
                hierarchy=self.hierarchy,
                prefixes=self.prefixes,
                name=name if self.name is None else self.name + '.' + name,
                depth=self.depth + 1,
                flags=self.flags | flags
            )
            node.traverse(cb)

    cb = SceneTraversal(node)
    node.traverse(cb)

    return SceneParameters(cb.properties, cb.hierarchy)


def convert_to_bitmap(data, uint8_srgb=True):
    """
    Convert the RGB image in `data` to a `Bitmap`. `uint8_srgb` defines whether
    the resulting bitmap should be translated to a uint8 sRGB bitmap.
    """

    if isinstance(data, mi.Bitmap):
        bitmap = data
    else:
        if type(data).__name__ == 'Tensor':
            data = data.detach().cpu().numpy()
        bitmap = mi.Bitmap(data)

    if uint8_srgb:
        bitmap = bitmap.convert(mi.Bitmap.PixelFormat.RGB,
                                mi.Struct.Type.UInt8, True)

    return bitmap


def write_bitmap(filename, data, write_async=True, quality=-1):
    """
    Write the RGB image in `data` to a PNG/EXR/.. file.
    """
    uint8_srgb = filename.endswith('.png') or \
                 filename.endswith('.jpg') or \
                 filename.endswith('.jpeg')

    bitmap = convert_to_bitmap(data, uint8_srgb)

    if write_async:
        bitmap.write_async(filename, quality=quality)
    else:
        bitmap.write(filename, quality=quality)
