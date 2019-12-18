""" Mitsuba core support library (generic mathematical and I/O routines) """

from . import (xml, filesystem, util, math, warp, spline)

__import__("mitsuba.core.mitsuba_core_ext")


def traverse(node):
    from mitsuba.scalar_rgb.core import TraversalCallback, \
        set_property, get_property

    class SceneTraversal(TraversalCallback):
        def __init__(self, node):
            TraversalCallback.__init__(self)
            self.properties = dict()
            self.hierarchy = dict()
            self.prefixes = set()
            self.name = ''
            self.node = node
            self.depth = 0
            self.hierarchy[node] = (None, 0)

        def put_parameter(self, name, cpptype, ptr):
            name = self.name + '.' + name if len(self.name) > 0 else name
            self.properties[name] = (self.node, ptr, cpptype)

        def put_object(self, name, node):
            if node in self.hierarchy:
                return
            old_node = self.node
            old_name = self.name
            self.name = self.name + '.' + name if len(self.name) > 0 else name
            ctr, name_len = 1, len(self.name)
            while self.name in self.prefixes:
                self.name = "%s_%i" % (self.name[:name_len], ctr)
                ctr += 1
            self.prefixes.add(self.name)
            self.depth += 1
            self.hierarchy[node] = (self.node, self.depth)
            self.node = node
            node.traverse(self)
            self.node = old_node
            self.name = old_name
            self.depth -= 1


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
            return 'ParameterMap' + repr(list(self.keys()))

        def keys(self):
            return self.properties.keys()

        def update(self):
            work_list = sorted(set(self.update_list), key=lambda x: x[0])
            for depth, node in reversed(work_list):
                node.parameters_changed()
            self.update_list.clear()

    return ParameterMap(cb.properties, cb.hierarchy)
