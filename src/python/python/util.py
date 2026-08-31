from __future__ import annotations as __annotations__ # Delayed parsing of type annotations

import contextlib
import copy as _copy
from collections.abc import Mapping
from typing import Any, Optional, Union

import drjit as dr
import mitsuba as mi

class SceneParameters(Mapping):
    """
    Dictionary-like object that references various parameters used in a Mitsuba
    scene graph. Parameters can be read and written using standard syntax
    (``parameter_map[key]``). The class exposes several non-standard functions,
    specifically :py:meth:`~mitsuba.SceneParameters.update()`, and
    :py:meth:`~mitsuba.SceneParameters.keep()`.

    The traversal itself and the storage of its result live in
    :py:class:`mitsuba.ParameterTable`. Keys and the Python objects of
    traversed nodes are produced on demand, hence a large scene graph costs
    little until its parameters are actually used.
    """

    def __init__(self, table=None):
        """
        Private constructor (use
        :py:func:`mitsuba.traverse()` instead)
        """
        self._table = table if table is not None else mi.ParameterTable()
        self.update_candidates = {}
        self._keys = None

    def copy(self):
        return SceneParameters(_copy.copy(self._table))

    def _index(self, key: str) -> int:
        index = self._table.lookup(key)
        if index < 0:
            raise KeyError(key)
        return index

    def __contains__(self, key: str):
        return self._table.lookup(key) >= 0

    def __get_value(self, key: str):
        return self._table.get(self._index(key))

    def __getitem__(self, key: str):
        value = self._table.get(self._index(key))

        if key not in self.update_candidates:
            self.update_candidates[key] = _jit_id_hash(value)

        return value

    def __setitem__(self, key: str, value):
        index = self._index(key)
        flags = self._table.flags(index)

        if (flags & mi.ParamFlags.ReadOnly) != 0:
            raise Exception(f'{key} is a read-only parameter!')

        cur_value = self._table.get(index)

        try:
            if (_jit_id_hash(cur_value) == _jit_id_hash(value) and
                dr.all(cur_value == value, axis=None)):
                # Turn this into a no-op when the set value is identical to the new value
                return
        except Exception:
            # Incomparable (e.g. mismatched shapes): let the write proceed so
            # that the parameter owner can report a meaningful error
            pass

        self.set_dirty(key)
        self._table.set(index, value)

    def __delitem__(self, key: str) -> None:
        index = self._index(key)
        self._table.keep([i for i in range(len(self._table)) if i != index])
        self._keys = None

    def __len__(self) -> int:
        return len(self._table)

    def __repr__(self) -> str:
        if len(self) == 0:
            return f'SceneParameters[]'
        keys = self.keys()
        rows = []
        for index, k in enumerate(keys):
            value = self._table.get(index)
            flags = self._table.flags(index)

            flags_str = ''
            if (flags & mi.ParamFlags.NonDifferentiable) == 0 and (flags & mi.ParamFlags.ReadOnly) == 0:
                flags_str += '∂'
            if (flags & mi.ParamFlags.ReadOnly) != 0:
                flags_str += 'R'
            if (flags & mi.ParamFlags.Discontinuous) != 0:
                flags_str += ', D'

            rows.append((k, flags_str, type(value).__name__,
                         self._table.owner(index).class_name()))

        name_length = int(max(len(r[0]) for r in rows) + 2)
        type_length = int(max(len(r[2]) for r in rows))
        param_list = '\n'
        param_list += '  ' + '-' * (name_length + 53) + '\n'
        param_list += f"  {'Name':{name_length}}  {'Flags':7}  {'Type':{type_length}} {'Parent'}\n"
        param_list += '  ' + '-' * (name_length + 53) + '\n'
        for k, flags_str, type_name, parent in rows:
            param_list += f'  {k:{name_length}}  {flags_str:7}  {type_name:{type_length}} {parent}\n'
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
        if self._keys is None:
            self._keys = self._table.keys()
        return self._keys

    def _ipython_key_completions_(self):
        return self.keys()

    def flags(self, key: str):
        """Return parameter flags"""
        return self._table.flags(self._index(key))

    def owner(self, key: str):
        """
        Return the Mitsuba object that reported the parameter ``key``.

        For example, the owner of ``'light.emitter.radiance.value'`` is the
        texture whose member ``value`` refers to.

        Raises ``KeyError`` when no parameter with this key exists.
        """
        return self._table.owner(self._index(key))

    def set_dirty(self, key: str):
        """
        Marks a specific parameter and its parent objects as dirty. A subsequent call
        to :py:meth:`~mitsuba.SceneParameters.update()` will refresh their internal
        state.

        This method should rarely be called explicitly. The
        :py:class:`~mitsuba.SceneParameters` will detect most operations on
        its values and automatically flag them as dirty. A common exception to
        the detection mechanism is the :py:func:`~drjit.scatter` operation which
        needs an explicit call to :py:meth:`~mitsuba.SceneParameters.set_dirty()`.
        """
        index = self._index(key)

        if (self._table.flags(index) & mi.ParamFlags.NonDifferentiable) and \
           dr.grad_enabled(self._table.get(index)):
            mi.Log(
                mi.LogLevel.Warn,
                f"Parameter '{key}' is marked as non-differentiable but has "
                "gradients enabled, unexpected results may occur!"
            )

        self._table.set_dirty(index)

    def update(self, values: Optional[Mapping] = None) -> list[tuple[Any, set]]:
        """
        This function should be called at the end of a sequence of writes
        to the dictionary. It automatically notifies all modified Mitsuba
        objects and their parent objects that they should refresh their
        internal state. For instance, the scene may rebuild its acceleration
        structures when a shape was modified, etc.

        The return value of this function is a list of tuples where each tuple
        corresponds to a Mitsuba node/object that is updated. The tuple's first
        element is the node itself. The second element is the set of keys that
        the node is being updated for.

        Args:
            values: Optional dictionary-like object containing a set of keys
                and values to be used to overwrite scene parameters. This
                operation will happen before propagating the update further
                into the scene internal state.
        """
        if values is not None:
            for k, v in values.items():
                if k in self:
                    self[k] = v

        for key in list(self.update_candidates.keys()):
            # Candidate objects might have been modified inplace, we must check
            # the JIT identifiers to see if the object has truly changed.
            if _jit_id_hash(self.__get_value(key)) == self.update_candidates[key]:
                continue

            self.set_dirty(key)

        out = self._table.update()

        self.update_candidates.clear()
        dr.eval()

        return out

    def keep(self, keys: str | list[str]) -> None:
        """
        Reduce the size of the dictionary by only keeping elements,
        whose keys are defined by 'keys'.

        Args:
            keys: Specifies which parameters should be kept. Regex are
                supported to define a subset of parameters at once.
        """
        if type(keys) is not list:
            keys = [keys]

        import re
        regexps = [re.compile(k).match for k in keys]

        self._table.keep([i for i, k in enumerate(self.keys())
                         if any(r(k) for r in regexps)])
        self._keys = None


def _jit_id_hash(value: Any) -> int:
    """
    Recursively retrieves all JIT identifiers of the input and returns them in
    a list of tuples where each tuple is: `(JIT identifier, AD identifier)`.
    Any non-JIT object referenced by the input will also be added to the list,
    its corresponding tuple is: `(object value, None)`.
    """

    def jit_ids(value: Any) -> list[tuple[int, Optional[int]]]:
        return dr.detail.collect_indices(value, dr.detail.TraverseRole.Freeze)

    return hash(tuple(jit_ids(value)))

def traverse(node: mi.Object) -> SceneParameters:
    """
    Traverse a node of Mitsuba's scene graph and return a dictionary-like
    object that can be used to read and write associated scene parameters.

    See also :py:class:`mitsuba.SceneParameters`.
    """
    return SceneParameters(mi.ParameterTable(node))

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

    def eval(self, scene, sensor, _, params, integrator, seed, spp):
        self.scene = scene
        self.sensor = sensor
        # The argument `_` is a `dict` of the parameters that is detached,
        # whereas `params` is a `SceneParameters` object that still contains
        # a referece to the attached paraamters
        self.params = params
        self.integrator = integrator
        self.seed = seed
        self.spp = spp

        with dr.suspend_grad():
            res = self.integrator.render(
                scene=self.scene,
                sensor=sensor,
                seed=seed[0],
                spp=spp[0],
                develop=True,
                evaluate=False
            )
            return res

    def forward(self):
        self.set_grad_out(
            self.integrator.render_forward(self.scene, self.params, self.sensor,
                                           self.seed[1], self.spp[1]))

    def backward(self):
        self.integrator.render_backward(self.scene, self.params, self.grad_out(),
                                        self.sensor, self.seed[1], self.spp[1])

    def name(self):
        return "RenderOp"

def render(scene: mi.Scene,
           params: Any = None,
           sensor: Union[int, mi.Sensor] = 0,
           integrator: mi.Integrator = None,
           seed: mi.UInt32 = 0,
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
    to `mitsuba.SamplingIntegrator.render_forward` or `mitsuba.SamplingIntegrator.render_backward`,
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

    Args:
        scene: Reference to the scene being rendered in a differentiable
            manner.

        params: An optional container of scene parameters that should receive
            gradients. This argument isn't optional when computing forward
            mode derivatives. It should be an instance of type
            `mitsuba.SceneParameters` obtained via `mitsuba.traverse()`.
            Gradient tracking must be explicitly enabled on these parameters
            using ``dr.enable_grad(params['parameter_name'])`` (i.e.
            ``render()`` will not do this for you). Furthermore,
            ``dr.set_grad(...)`` must be used to associate specific gradient
            values with parameters if forward mode derivatives are desired.
            When the scene parameters are derived from other variables that
            have gradient tracking enabled, gradient values should be
            propagated to the scene parameters by calling
            ``dr.forward_to(params, dr.ADFlag.ClearEdges)`` before calling
            this function.

        sensor: Specify a sensor or a (sensor index) to render the scene from
            a different viewpoint. By default, the first sensor within the
            scene description (index 0) will take precedence.

        integrator: Optional parameter to override the rendering technique to
            be used. By default, the integrator specified in the original
            scene description will be used.

        seed: This parameter controls the initialization of the random
            number generator during the primal rendering step. It is crucial
            that you specify different seeds (e.g., an increasing sequence)
            if subsequent calls should produce statistically independent
            images (e.g. to de-correlate gradient-based optimization steps).

        seed_grad: This parameter is analogous to the ``seed`` parameter but
            targets the differential simulation phase. If not specified, the
            implementation will automatically compute a suitable value from
            the primal ``seed``.

        spp: Optional parameter to override the number of samples per pixel
            for the primal rendering step. The value provided within the
            original scene specification takes precedence if ``spp=0``.

        spp_grad: This parameter is analogous to the ``seed`` parameter but
            targets the differential simulation phase. If not specified, the
            implementation will copy the value from ``spp``.
    """

    if params is not None and not isinstance(params, mi.SceneParameters):
        raise Exception('The `params` argument should be an instance of `mi.SceneParameters`!')

    dict_params = dict()
    if params is not None:
        dict_params = dict(params) # Turn SceneParameters into a valid PyTree

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

    if 'scalar' in mi.variant():
        return integrator.render(
                scene=scene,
                sensor=sensor,
                seed=seed,
                spp=spp,
                develop=True,
                evaluate=False
            )

    # Both `dict_params` and `params` are passed. The former is necessary
    # because it allows the custom operation to detect any attached input
    # arguments. The latter is necessary because it will not be automatically
    # detached by the custom operation.
    return dr.custom(_RenderOp, scene, sensor, dict_params, params, integrator,
                     (seed, seed_grad), (spp, spp_grad))

# ------------------------------------------------------------------------------

def convert_to_bitmap(data, uint8_srgb=True):
    """
    Convert the RGB image in ``data`` to a `mitsuba.Bitmap`. ``uint8_srgb``
    defines whether the resulting bitmap should be translated to a uint8 sRGB
    bitmap.
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
    Write the RGB image in ``data`` to a PNG/EXR/.. file.
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
            'to_world': T().look_at(
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
            'to_world': T().translate([0.0, 0.99, 0.01]).rotate([1, 0, 0], 90).scale([0.23, 0.19, 0.19]),
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
            'to_world': T().translate([0.0, -1.0, 0.0]).rotate([1, 0, 0], -90),
            'bsdf': {
                'type': 'ref',
                'id':  'white'
            }
        },
        'ceiling': {
            'type': 'rectangle',
            'to_world': T().translate([0.0, 1.0, 0.0]).rotate([1, 0, 0], 90),
            'bsdf': {
                'type': 'ref',
                'id':  'white'
            }
        },
        'back': {
            'type': 'rectangle',
            'to_world': T().translate([0.0, 0.0, -1.0]),
            'bsdf': {
                'type': 'ref',
                'id':  'white'
            }
        },
        'green-wall': {
            'type': 'rectangle',
            'to_world': T().translate([1.0, 0.0, 0.0]).rotate([0, 1, 0], -90),
            'bsdf': {
                'type': 'ref',
                'id':  'green'
            }
        },
        'red-wall': {
            'type': 'rectangle',
            'to_world': T().translate([-1.0, 0.0, 0.0]).rotate([0, 1, 0], 90),
            'bsdf': {
                'type': 'ref',
                'id':  'red'
            }
        },
        'small-box': {
            'type': 'cube',
            'to_world': T().translate([0.335, -0.7, 0.38]).rotate([0, 1, 0], -17).scale(0.3),
            'bsdf': {
                'type': 'ref',
                'id':  'white'
            }
        },
        'large-box': {
            'type': 'cube',
            'to_world': T().translate([-0.33, -0.4, -0.28]).rotate([0, 1, 0], 18.25).scale([0.3, 0.61, 0.3]),
            'bsdf': {
                'type': 'ref',
                'id':  'white'
            }
        },
    }


@contextlib.contextmanager
def variant_context(*args) -> None:
    '''
    Temporarily override the active variant. Arguments are interpreted as
    they are in :func:`mitsuba.set_variant`.
    '''

    old_variant = mi.variant()
    try:
        mi.set_variant(*args)
        yield
    except Exception:
        raise
    finally:
        mi.set_variant(old_variant)

scoped_set_variant = variant_context
