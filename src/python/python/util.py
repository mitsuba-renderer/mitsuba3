from __future__ import annotations as __annotations__ # Delayed parsing of type annotations

from collections.abc import Mapping

import drjit as dr
import mitsuba as mi

import typing
if typing.TYPE_CHECKING:
    from typing import Any, Optional, Union

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
        self.update_candidates = {}
        self.nodes_to_update = {}

        self.set_property = mi.set_property
        self.get_property = mi.get_property

    def copy(self):
        return SceneParameters(
            dict(self.properties),
            dict(self.hierarchy))

    def __contains__(self, key: str):
        return self.properties.__contains__(key)

    def __get_value(self, key: str):
        value, value_type, node, _ = self.properties[key]

        if value_type is not None:
            value = self.get_property(value, value_type, node)

        return value

    def __getitem__(self, key: str):
        value = self.__get_value(key)

        if key not in self.update_candidates:
            self.update_candidates[key] = _jit_id_hash(value)

        return value

    def __setitem__(self, key: str, value):
        cur, value_type, node, _ = self.properties[key]

        cur_value = cur
        if value_type is not None:
            cur_value = self.get_property(cur, value_type, node)

        if _jit_id_hash(cur_value) == _jit_id_hash(value) and cur_value == value:
            # Turn this into a no-op when the set value is identical to the new value
            return

        self.set_dirty(key)

        if value_type is None:
            try:
                cur.assign(value)
            except AttributeError as e:
                if "has no attribute 'assign'" in str(e):
                    mi.Log(
                        mi.LogLevel.Warn,
                        f"Parameter '{key}' cannot be modified! This usually "
                        "happens when the parameter is not a Mitsuba type."
                        "Please use non-scalar Mitsuba types in your custom "
                        "plugins."
                    )
                else:
                    raise e
        else:
            self.set_property(cur, value_type, value)

    def __delitem__(self, key: str) -> None:
        del self.properties[key]

    def __len__(self) -> int:
        return len(self.properties)

    def __repr__(self) -> str:
        if len(self) == 0:
            return f'SceneParameters[]'
        name_length = int(max([len(k) for k in self.properties.keys()]) + 2)
        type_length = int(max([len(type(v).__name__) for k, v in self.properties.items()]))
        param_list = '\n'
        param_list += '  ' + '-' * (name_length + 53) + '\n'
        param_list += f"  {'Name':{name_length}}  {'Flags':7}  {'Type':{type_length}} {'Parent'}\n"
        param_list += '  ' + '-' * (name_length + 53) + '\n'
        for k, v in self.properties.items():
            value, value_type, node, flags = v

            if value_type is not None:
                value = self.get_property(value, value_type, node)

            flags_str = ''
            if (flags & mi.ParamFlags.NonDifferentiable) == 0:
                flags_str += '∂'
            if (flags & mi.ParamFlags.Discontinuous) != 0:
                flags_str += ', D'

            param_list += f'  {k:{name_length}}  {flags_str:7}  {type(value).__name__:{type_length}} {node.class_().name()}\n'
        return f'SceneParameters[{param_list}]'

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
        state.

        This method should rarely be called explicitly. The
        :py:class:`~mitsuba.SceneParameters` will detect most operations on
        its values and automatically flag them as dirty. A common exception to
        the detection mechanism is the :py:meth:`~drjit.scatter` operation which
        needs an explicit call to :py:meth:`~mitsuba.SceneParameters.set_dirty()`.
        """
        value, _, node, flags = self.properties[key]

        is_nondifferentiable = (flags & mi.ParamFlags.NonDifferentiable.value)
        if is_nondifferentiable and dr.grad_enabled(value):
            mi.Log(
                mi.LogLevel.Warn,
                f"Parameter '{key}' is marked as non-differentiable but has "
                "gradients enabled, unexpected results may occur!"
            )

        node_key = key
        while node is not None:
            parent, depth = self.hierarchy[node]

            name = node_key
            if parent is not None:
                node_key, name = node_key.rsplit('.', 1)

            self.nodes_to_update.setdefault((depth, node), set())
            self.nodes_to_update[(depth, node)].add(name)

            node = parent

        return self.properties[key]

    def update(self, values: dict = None) -> list[tuple[Any, set]]:
        """
        This function should be called at the end of a sequence of writes
        to the dictionary. It automatically notifies all modified Mitsuba
        objects and their parent objects that they should refresh their
        internal state. For instance, the scene may rebuild the kd-tree
        when a shape was modified, etc.

        The return value of this function is a list of tuples where each tuple
        corresponds to a Mitsuba node/object that is updated. The tuple's first
        element is the node itself. The second element is the set of keys that
        the node is being updated for.

        Parameter ``values`` (``dict``):
            Optional dictionary-like object containing a set of keys and values
            to be used to overwrite scene parameters. This operation will happen
            before propagating the update further into the scene internal state.
        """
        if values is not None:
            for k, v in values.items():
                if k in self:
                    self[k] = v

        update_candidate_keys = list(self.update_candidates.keys())
        for key in update_candidate_keys:
            # Candidate objects might have been modified inplace, we must check
            # the JIT identifiers to see if the object has truly changed.
            if _jit_id_hash(self.__get_value(key)) == self.update_candidates[key]:
                continue

            self.set_dirty(key)

        for key in self.keys():
            dr.schedule(self.__get_value(key))

        # Notify nodes from bottom to top
        work_list = [(d, n, k) for (d, n), k in self.nodes_to_update.items()]
        work_list = reversed(sorted(work_list, key=lambda x: x[0]))
        out = []
        for _, node, keys in work_list:
            node.parameters_changed(list(keys))
            out.append((node, keys))

        self.nodes_to_update.clear()
        self.update_candidates.clear()
        dr.eval()

        return out

    def keep(self, keys: None | str | list[str]) -> None:
        """
        Reduce the size of the dictionary by only keeping elements,
        whose keys are defined by 'keys'.

        Parameter ``keys`` (``None``, ``str``, ``[str]``):
            Specifies which parameters should be kept. Regex are supported to define
            a subset of parameters at once. If set to ``None``, all differentiable
            scene parameters will be loaded.
        """
        if type(keys) is not list:
            keys = [keys]

        import re
        regexps = [re.compile(k).match for k in keys]
        keys = [k for k in self.keys() if any (r(k) for r in regexps)]

        self.properties = {
            k: v for k, v in self.properties.items() if k in keys
        }

def _jit_id_hash(value: Any) -> int:
    """
    Recursively retrieves all JIT identifiers of the input and returns them in
    a list of tuples where each tuple is: `(JIT identifier, AD identifier)`.
    Any non-JIT object referenced by the input will also be added to the list,
    its corresponding tuple is: `(object value, None)`.
    """

    def jit_ids(value: Any) -> list[tuple[int, Optional[int]]]:
        ids = []

        if dr.is_static_array_v(value):
            for i in range(len(value)):
                ids.extend(jit_ids(value[i]))
        elif dr.is_diff_v(value):
            ids.append((value.index, value.index_ad))
        elif dr.is_tensor_v(value):
            ids.extend(jit_ids(value.array))
        elif dr.is_jit_v(value):
            ids.append((value.index, 0))
        elif dr.is_array_v(value) and dr.is_dynamic_array_v(value):
            for i in range(len(value)):
                ids.extend(jit_ids(value[i]))
        elif dr.is_struct_v(value):
            for k in value.DRJIT_STRUCT.keys():
                ids.extend(jit_ids(getattr(value, k)))
        else:
            # Scalars: None is used to differentiate from non-diff JIT array case
            try:
                ids.append((hash(value), None))
            except TypeError:
                ids.append((id(value), None))

        return ids

    return hash(tuple(jit_ids(value)))

def traverse(node: mi.Object) -> SceneParameters:
    """
    Traverse a node of Mitsuba's scene graph and return a dictionary-like
    object that can be used to read and write associated scene parameters.

    See also :py:class:`mitsuba.SceneParameters`.
    """

    class SceneTraversal(mi.TraversalCallback):
        def __init__(self, node, parent=None, properties=None,
                     hierarchy=None, prefixes=None, name=None, depth=0,
                     flags=+mi.ParamFlags.Differentiable):
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

        def put_parameter(self, name, ptr, flags, cpptype=None):
            name = name if self.name is None else self.name + '.' + name

            flags = self.flags | flags
            # Non differentiable parameters shouldn't be flagged as discontinuous
            if (flags & mi.ParamFlags.NonDifferentiable) != 0:
                flags = flags & ~mi.ParamFlags.Discontinuous

            self.properties[name] = (ptr, cpptype, self.node, self.flags | flags)

        def put_object(self, name, node, flags):
            if node is None or node in self.hierarchy:
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

# ------------------------------------------------------------------------------
#                          Rendering Custom Operation
# ------------------------------------------------------------------------------

class _RenderOp(dr.CustomOp):
    """
    This class is an implementation detail of the render() function. It
    realizes a CustomOp that provides evaluation, and forward/reverse-mode
    differentiation callbacks that will be invoked as needed (e.g. when a
    rendering operation is encountered by an AD graph traversal).
    """

    def __init__(self) -> None:
        super().__init__()
        self.variant = mi.variant()

    def eval(self, scene, sensor, params, integrator, seed, spp):
        self.scene = scene
        self.sensor = sensor
        self.params = params
        self.integrator = integrator
        self.seed = seed
        self.spp = spp

        with dr.suspend_grad():
            return self.integrator.render(
                scene=self.scene,
                sensor=sensor,
                seed=seed[0],
                spp=spp[0],
                develop=True,
                evaluate=False
            )

    def forward(self):
        mi.set_variant(self.variant)
        if not isinstance(self.params, mi.SceneParameters):
            raise Exception('An instance of mi.SceneParameter containing the '
                            'scene parameter to be differentiated should be '
                            'provided to mi.render() if forward derivatives are '
                            'desired!')
        self.set_grad_out(
            self.integrator.render_forward(self.scene, self.params, self.sensor,
                                           self.seed[1], self.spp[1]))

    def backward(self):
        mi.set_variant(self.variant)
        if not isinstance(self.params, mi.SceneParameters):
            raise Exception('An instance of mi.SceneParameter containing the '
                            'scene parameter to be differentiated should be '
                            'provided to mi.render() if backward derivatives are '
                            'desired!')
        self.integrator.render_backward(self.scene, self.params, self.grad_out(),
                                        self.sensor, self.seed[1], self.spp[1])

    def name(self):
        return "RenderOp"

def render(scene: mi.Scene,
           params: Any = None,
           sensor: Union[int, mi.Sensor] = 0,
           integrator: mi.Integrator = None,
           seed: int = 0,
           seed_grad: int = 0,
           spp: int = 0,
           spp_grad: int = 0) -> mi.TensorXf:
    """
    This function provides a convenient high-level interface to differentiable
    rendering algorithms in Mi. The function returns a rendered image that can
    be used in subsequent differentiable computation steps. At any later point,
    the entire computation graph can be differentiated end-to-end in either
    forward or reverse mode (i.e., using ``dr.forward()`` and
    ``dr.backward()``).

    Under the hood, the differentiation operation will be intercepted and routed
    to ``Integrator.render_forward()`` or ``Integrator.render_backward()``,
    which evaluate the derivative using either naive AD or a more specialized
    differential simulation.

    Note the default implementation of this functionality relies on naive
    automatic differentiation (AD), which records a computation graph of the
    primal rendering step that is subsequently traversed to propagate
    derivatives. This tends to be relatively inefficient due to the need to
    track intermediate program state. In particular, it means that
    differentiation of nontrivial scenes at high sample counts will often run
    out of memory. Integrators like ``rb`` (Radiative Backpropagation) and
    ``prb`` (Path Replay Backpropagation) that are specifically designed for
    differentiation can be significantly more efficient.

    Parameter ``scene`` (``mi.Scene``):
        Reference to the scene being rendered in a differentiable manner.

    Parameter ``params``:
       An optional container of scene parameters that should receive gradients.
       This argument isn't optional when computing forward mode derivatives. It
       should be an instance of type ``mi.SceneParameters`` obtained via
       ``mi.traverse()``. Gradient tracking must be explicitly enabled on these
       parameters using ``dr.enable_grad(params['parameter_name'])`` (i.e.
       ``render()`` will not do this for you). Furthermore, ``dr.set_grad(...)``
       must be used to associate specific gradient values with parameters if
       forward mode derivatives are desired. When the scene parameters are
       derived from other variables that have gradient tracking enabled,
       gradient values should be propagated to the scene parameters by calling
       ``dr.forward_to(params, dr.ADFlag.ClearEdges)`` before calling this
       function.

    Parameter ``sensor`` (``int``, ``mi.Sensor``):
        Specify a sensor or a (sensor index) to render the scene from a
        different viewpoint. By default, the first sensor within the scene
        description (index 0) will take precedence.

    Parameter ``integrator`` (``mi.Integrator``):
        Optional parameter to override the rendering technique to be used. By
        default, the integrator specified in the original scene description will
        be used.

    Parameter ``seed`` (``int``)
        This parameter controls the initialization of the random number
        generator during the primal rendering step. It is crucial that you
        specify different seeds (e.g., an increasing sequence) if subsequent
        calls should produce statistically independent images (e.g. to
        de-correlate gradient-based optimization steps).

    Parameter ``seed_grad`` (``int``)
        This parameter is analogous to the ``seed`` parameter but targets the
        differential simulation phase. If not specified, the implementation will
        automatically compute a suitable value from the primal ``seed``.

    Parameter ``spp`` (``int``):
        Optional parameter to override the number of samples per pixel for the
        primal rendering step. The value provided within the original scene
        specification takes precedence if ``spp=0``.

    Parameter ``spp_grad`` (``int``):
        This parameter is analogous to the ``seed`` parameter but targets the
        differential simulation phase. If not specified, the implementation will
        copy the value from ``spp``.
    """

    if params is not None and not isinstance(params, mi.SceneParameters):
        raise Exception('params should be an instance of mi.SceneParameter!')

    assert isinstance(scene, mi.Scene)

    if integrator is None:
        integrator = scene.integrator()

    if integrator is None:
        raise Exception('No integrator specified! Add an integrator in the scene '
                        'description or provide an integrator directly as argument.')

    if isinstance(sensor, int):
        if len(scene.sensors()) == 0:
            raise Exception('No sensor specified! Add a sensor in the scene '
                            'description or provide a sensor directly as argument.')
        sensor = scene.sensors()[sensor]

    assert isinstance(integrator, mi.Integrator)
    assert isinstance(sensor, mi.Sensor)

    if spp_grad == 0:
        spp_grad = spp

    if seed_grad == 0:
        # Compute a seed that de-correlates the primal and differential phase
        seed_grad = mi.sample_tea_32(seed, 1)[0]
    elif seed_grad == seed:
        raise Exception('The primal and differential seed should be different '
                        'to ensure unbiased gradient computation!')

    return dr.custom(_RenderOp, scene, sensor, params, integrator,
                     (seed, seed_grad), (spp, spp_grad))

# ------------------------------------------------------------------------------

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

# ------------------------------------------------------------------------------
#                            Cornell Box scene
# ------------------------------------------------------------------------------

def cornell_box():
    '''
    Returns a dictionary containing a description of the Cornell Box scene.
    '''
    T = mi.ScalarTransform4f
    return {
        'type': 'scene',
        'integrator': {
            'type': 'path',
            'max_depth': 8
        },
        # -------------------- Sensor --------------------
        'sensor': {
            'type': 'perspective',
            'fov_axis': 'smaller',
            'near_clip': 0.001,
            'far_clip': 100.0,
            'focus_distance': 1000,
            'fov': 39.3077,
            'to_world': T.look_at(
                origin=[0, 0, 3.90],
                target=[0, 0, 0],
                up=[0, 1, 0]
            ),
            'sampler': {
                'type': 'independent',
                'sample_count': 64
            },
            'film': {
                'type': 'hdrfilm',
                'width' : 256,
                'height': 256,
                'rfilter': {
                    'type': 'gaussian',
                },
                'pixel_format': 'rgb',
                'component_format': 'float32',
            }
        },
        # -------------------- BSDFs --------------------
        'white': {
            'type': 'diffuse',
            'reflectance': {
                'type': 'rgb',
                'value': [0.885809, 0.698859, 0.666422],
            }
        },
        'green': {
            'type': 'diffuse',
            'reflectance': {
                'type': 'rgb',
                'value': [0.105421, 0.37798, 0.076425],
            }
        },
        'red': {
            'type': 'diffuse',
            'reflectance': {
                'type': 'rgb',
                'value': [0.570068, 0.0430135, 0.0443706],
            }
        },
        # -------------------- Light --------------------
        'light': {
            'type': 'rectangle',
            'to_world': T.translate([0.0, 0.99, 0.01]).rotate([1, 0, 0], 90).scale([0.23, 0.19, 0.19]),
            'bsdf': {
                'type': 'ref',
                'id': 'white'
            },
            'emitter': {
                'type': 'area',
                'radiance': {
                    'type': 'rgb',
                    'value': [18.387, 13.9873, 6.75357],
                }
            }
        },
        # -------------------- Shapes --------------------
        'floor': {
            'type': 'rectangle',
            'to_world': T.translate([0.0, -1.0, 0.0]).rotate([1, 0, 0], -90),
            'bsdf': {
                'type': 'ref',
                'id':  'white'
            }
        },
        'ceiling': {
            'type': 'rectangle',
            'to_world': T.translate([0.0, 1.0, 0.0]).rotate([1, 0, 0], 90),
            'bsdf': {
                'type': 'ref',
                'id':  'white'
            }
        },
        'back': {
            'type': 'rectangle',
            'to_world': T.translate([0.0, 0.0, -1.0]),
            'bsdf': {
                'type': 'ref',
                'id':  'white'
            }
        },
        'green-wall': {
            'type': 'rectangle',
            'to_world': T.translate([1.0, 0.0, 0.0]).rotate([0, 1, 0], -90),
            'bsdf': {
                'type': 'ref',
                'id':  'green'
            }
        },
        'red-wall': {
            'type': 'rectangle',
            'to_world': T.translate([-1.0, 0.0, 0.0]).rotate([0, 1, 0], 90),
            'bsdf': {
                'type': 'ref',
                'id':  'red'
            }
        },
        'small-box': {
            'type': 'cube',
            'to_world': T.translate([0.335, -0.7, 0.38]).rotate([0, 1, 0], -17).scale(0.3),
            'bsdf': {
                'type': 'ref',
                'id':  'white'
            }
        },
        'large-box': {
            'type': 'cube',
            'to_world': T.translate([-0.33, -0.4, -0.28]).rotate([0, 1, 0], 18.25).scale([0.3, 0.61, 0.3]),
            'bsdf': {
                'type': 'ref',
                'id':  'white'
            }
        },
    }
