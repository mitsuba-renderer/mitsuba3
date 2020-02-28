def traverse(node):
    """
    Traverse a node of Mitsuba's scene graph and return a dictionary-like
    object that can be used to read and write associated scene parameters.

    This dictionary exposes multiple non-standard methods:

    1. ``keep(self, keys: list) -> None``:

       Reduce the size of the dictionary by only keeping elements,
       whose keys are part of the provided list 'keys'.

    2. ``update(self) -> None``:

       This function should be called at the end of a sequence of writes
       to the dictionary. It automatically notifies all modified Mitsuba
       objects and their parent objects that they should refresh their
       internal state. For instance, the scene may rebuild the kd-tree
       when a shape was modified, etc.

    3. ``torch(self) -> dict``:

       Converts all Enoki arrays into PyTorch arrays and return them as a
       dictionary. This is mainly useful when using PyTorch to optimize a
       Mitsuba scene.

    """
    from mitsuba.core import TraversalCallback, \
        set_property, get_property

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

    class ParameterMap:
        def __init__(self, properties, hierarchy):
            self.properties = properties
            self.hierarchy = hierarchy
            self.update_list = []

        def __contains__(self, key):
            return self.properties.__contains__(key)

        def __getitem__(self, key):
            return get_property(*(self.properties[key]))

        def __setitem__(self, key, value):
            item = self.properties[key]
            node = item[2]
            while node is not None:
                parent, depth = self.hierarchy[node]
                self.update_list.append((depth, node))
                node = parent
            return set_property(item[0], item[1], value)

        def __delitem__(self, key):
            del self.properties[key]

        def __len__(self):
            return len(self.properties)

        def __repr__(self):
            return 'ParameterMap[\n    ' + ',\n    '.join(self.keys()) + '\n]'

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

        def torch(self):
            return {k: v.torch().requires_grad_() for k, v in self.items()}

        def update(self):
            work_list = sorted(set(self.update_list), key=lambda x: x[0])
            for depth, node in reversed(work_list):
                node.parameters_changed()
            self.update_list.clear()

        def keep(self, keys):
            keys = set(keys)
            self.properties = {
                k: v for k, v in self.properties.items() if k in keys
            }

    return ParameterMap(cb.properties, cb.hierarchy)
