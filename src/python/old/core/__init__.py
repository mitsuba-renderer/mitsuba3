""" Mitsuba core support library (generic mathematical and I/O routines) """

from . import (xml, filesystem, util, math, warp, spline)

__import__("mitsuba.core.mitsuba_core_ext")


def traverse(node):
    from mitsuba.scalar_rgb.core import TraversalCallback, \
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
            self.properties[name] = (self.node, ptr, cpptype)

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
            item = self.properties[key]
            return get_property(item[1], item[2])

        def __setitem__(self, key, value):
            item = self.properties[key]
            node = item[0]
            while node is not None:
                parent, depth = self.hierarchy[node]
                self.update_list.append((depth, node))
                node = parent
            return set_property(item[1], item[2], value)

        def __delitem__(self, key):
            del self.properties[key]

        def __len__(self):
            return len(self.properties)

        def __repr__(self):
            return 'ParameterMap[\n    ' + ',\n    '.join(self.keys()) + '\n]'

        def keys(self):
            return self.properties.keys()

        def update(self):
            work_list = sorted(set(self.update_list), key=lambda x: x[0])
            for depth, node in reversed(work_list):
                node.parameters_changed()
            self.update_list.clear()

    return ParameterMap(cb.properties, cb.hierarchy)
