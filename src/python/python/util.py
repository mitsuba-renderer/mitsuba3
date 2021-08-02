from collections.abc import Mapping
from contextlib import contextmanager

import mitsuba
import enoki as ek


class SceneParameters(Mapping):
    """
    Dictionary-like object that references various parameters used in a Mitsuba
    scene graph. Parameters can be read and written using standard syntax
    (``parameter_map[key]``). The class exposes several non-standard functions,
    specifically :py:meth:`~mitsuba.python.util.SceneParameters.torch()`,
    :py:meth:`~mitsuba.python.util.SceneParameters.update()`, and
    :py:meth:`~mitsuba.python.util.SceneParameters.keep()`.
    """

    def __init__(self, properties=None, hierarchy=None):
        """
        Private constructor (use
        :py:func:`mitsuba.python.util.traverse()` instead)
        """
        self.properties = properties if properties is not None else {}
        self.hierarchy  = hierarchy  if hierarchy  is not None else {}
        self.update_list = {}

        from mitsuba.core import set_property, get_property
        self.set_property = set_property
        self.get_property = get_property

    def __contains__(self, key: str):
        return self.properties.__contains__(key)

    def __getitem__(self, key: str):
        value, value_type, node = self.properties[key]
        if value_type is None:
            return value
        else:
            return self.get_property(value, value_type, node)

    def __setitem__(self, key: str, value):
        cur, value_type, node = self.set_dirty(key)
        if value_type is None:
            cur.assign(value)
        else:
            self.set_property(cur, value_type, value)

    def __delitem__(self, key: str) -> None:
        del self.properties[key]

    def __len__(self) -> int:
        return len(self.properties)

    def __repr__(self) -> str:
        param_list = ''
        for k, v in self.items():
            is_diff = ek.is_diff_array_v(v) and ek.is_floating_point_v(v)
            param_list += '  %s %s,\n' % (('*' if is_diff else ' '), k)
        return 'SceneParameters[\n%s]' % param_list

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

    def all_differentiable(self):
        for k, v in self.items():
            if not (ek.is_diff_array_v(v) and ek.is_floating_point_v(v)):
                return False
        return True

    def torch(self) -> dict:
        """
        Converts all Enoki arrays into PyTorch arrays and return them as a
        dictionary. This is mainly useful when using PyTorch to optimize a
        Mitsuba scene.
        """
        return {k: v.torch().requires_grad_() for k, v in self.items()}

    def set_dirty(self, key: str):
        """
        Marks a specific parameter and its parent objects as dirty. A subsequent call
        to :py:meth:`~mitsuba.python.util.SceneParameters.update()` will refresh their internal
        state. This function is automatically called when overwriting a parameter using
        :py:meth:`~mitsuba.python.util.SceneParameters.__setitem__()`.
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

    def keep(self, keys: list) -> None:
        """
        Reduce the size of the dictionary by only keeping elements,
        whose keys are part of the provided list 'keys'.
        """
        keys = set(keys)
        self.properties = {
            k: v for k, v in self.properties.items() if k in keys
        }

    def set_grad_suspended(self, state: bool) -> None:
        """
        Suspend/Resume tracking of all gradients in the scene.

        Params
        ------

        - state : whether to stop or resume recording gradients.
        """

        for obj, (parent, depth) in self.hierarchy.items():
            if depth == 0: # root node
                suspend_gradients(obj, state)
                break

    @contextmanager
    def suspend_gradients(self):
        """Temporarily disable the generation of gradients."""
        self.set_grad_suspended(True)
        try:
            yield
        finally:
            self.set_grad_suspended(False)

    @contextmanager
    def resume_gradients(self):
        """Temporarily enable the generation of gradients"""
        self.set_grad_suspended(False)
        try:
            yield
        finally:
            self.set_grad_suspended(True)


def traverse(node: 'mitsuba.core.Object') -> SceneParameters:
    """
    Traverse a node of Mitsuba's scene graph and return a dictionary-like
    object that can be used to read and write associated scene parameters.

    See also :py:class:`mitsuba.python.util.SceneParameters`.
    """
    from mitsuba.core import TraversalCallback

    class SceneTraversal(TraversalCallback):
        def __init__(self, node, parent=None, properties=None,
                     hierarchy=None, prefixes=None, name=None, depth=0):
            TraversalCallback.__init__(self)
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

        def put_parameter(self, name, ptr, cpptype=None):
            name = name if self.name is None else self.name + '.' + name
            self.properties[name] = (ptr, cpptype, self.node)

        def put_object(self, name, node):
            if node in self.hierarchy:
                return
            cb = SceneTraversal(
                node=node,
                parent=self.node,
                properties=self.properties,
                hierarchy=self.hierarchy,
                prefixes=self.prefixes,
                name=name if self.name is None else self.name + '.' + name,
                depth=self.depth + 1
            )
            node.traverse(cb)

    cb = SceneTraversal(node)
    node.traverse(cb)

    return SceneParameters(cb.properties, cb.hierarchy)


def suspend_gradients(node: 'mitsuba.core.Object', state: 'bool') -> None:
    """
    Traverse a node of Mitsuba's scene graph and suspend/resume keeping track of derivatives.
    """
    from mitsuba.core import TraversalCallback

    class SuspendCallback(TraversalCallback):
        def __init__(self, node, state):
            TraversalCallback.__init__(self)
            self.state = state
            self.node = node

            from mitsuba.core import get_property
            self.get_property = get_property

        def put_parameter(self, name, value, cpptype=None):
            if cpptype is None:
                ek.set_grad_suspended(value, self.state)
            else:
                ek.set_grad_suspended(self.get_property(value, cpptype, self.node), self.state)

        def put_object(self, name, node):
            node.set_grad_suspended(self.state) # Suspend hidden parameters if necessary
            cb = SuspendCallback(
                node=node,
                state=self.state,
            )
            node.traverse(cb)

    cb = SuspendCallback(node, state)
    node.traverse(cb)


def convert_to_bitmap(data, resolution, uint8_srgb=True):
    """
    Convert the linearized RGB image in `data` to a `Bitmap` with
    resolution `resolution`. `uint8_srgb` defines whether the resulting
    bitmap should be translated to a uint8 sRGB bitmap.
    """
    from mitsuba.core import Bitmap, Struct

    if type(data).__name__ == 'Tensor':
        data = data.detach().cpu()

    bitmap = Bitmap(data.numpy().reshape(resolution[1], resolution[0], -1))
    if uint8_srgb:
        bitmap = bitmap.convert(Bitmap.PixelFormat.RGB,
                                Struct.Type.UInt8, True)
    return bitmap


def write_bitmap(filename, data, resolution, write_async=True, quality=-1):
    """
    Write the linearized RGB image in `data` to a PNG/EXR/.. file with
    resolution `resolution`.
    """
    uint8_srgb = filename.endswith('.png') or \
                 filename.endswith('.jpg') or \
                 filename.endswith('.jpeg')

    bitmap = convert_to_bitmap(data, resolution, uint8_srgb)

    if write_async:
        bitmap.write_async(filename, quality=quality)
    else:
        bitmap.write(filename, quality=quality)
