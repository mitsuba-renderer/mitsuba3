import mitsuba


"""
Returns whether a parameter's type is a differentiable enoki type.
"""
def is_differentiable(p):
    p_type = type(p)
    return p_type.__module__ == 'enoki.cuda_autodiff' \
           and not p_type.__name__.endswith('u')      \
           and not p_type.__name__.endswith('i')


class ParameterMap:
    """
    Dictionary-like object that references various parameters used in a Mitsuba
    scene graph. Parameters can be read and written using standard syntax
    (``parameter_map[key]``). The class exposes several non-standard functions,
    specifically :py:meth:`~mitsuba.python.util.ParameterMap.torch()`,
    :py:meth:`~mitsuba.python.util.ParameterMap.update()`, and
    :py:meth:`~mitsuba.python.util.ParameterMap.keep()`.
    """

    def __init__(self, properties, hierarchy):
        """
        Private constructor (use
        :py:func:`mitsuba.python.util.traverse()` instead)
        """
        self.properties = properties
        self.hierarchy = hierarchy
        self.update_list = {}

        from mitsuba.core import set_property, get_property
        self.set_property = set_property
        self.get_property = get_property

    def __contains__(self, key: str):
        return self.properties.__contains__(key)

    def __getitem__(self, key: str):
        return self.get_property(*(self.properties[key]))

    def __setitem__(self, key: str, value):
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
        return self.set_property(item[0], item[1], value)

    def __delitem__(self, key: str) -> None:
        del self.properties[key]

    def __len__(self) -> int:
        return len(self.properties)

    def __repr__(self) -> str:
        param_list = ''
        for k in self.keys():
            param_list += '  %s %s,\n' % (
                ('*' if is_differentiable(self[k]) else ' '), k)
        return 'ParameterMap[\n%s]' % param_list

    def keys(self):
        return self.properties.keys()

    def items(self):
        class ParameterMapItemIterator:
            def __init__(self, pmap):
                self.pmap = pmap
                self.it = pmap.keys().__iter__()

            def __iter__(self):
                return self

            def __next__(self):
                key = next(self.it)
                return (key, self.pmap[key])

        return ParameterMapItemIterator(self)

    def all_differentiable(self):
        for k in self.keys():
            if not is_differentiable(self[k]):
                return False
        return True

    def torch(self) -> dict:
        """
        Converts all Enoki arrays into PyTorch arrays and return them as a
        dictionary. This is mainly useful when using PyTorch to optimize a
        Mitsuba scene.
        """
        return {k: v.torch().requires_grad_() for k, v in self.items()}

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


def traverse(node: 'mitsuba.core.Object') -> ParameterMap:
    """
    Traverse a node of Mitsuba's scene graph and return a dictionary-like
    object that can be used to read and write associated scene parameters.

    See also :py:class:`mitsuba.python.util.ParameterMap`.
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

        def put_parameter(self, name, cpptype, ptr):
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

    return ParameterMap(cb.properties, cb.hierarchy)
