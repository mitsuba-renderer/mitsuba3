Porting to Mitsuba 3.6.0
========================

.. _dr_main: https://drjit.readthedocs.io/en/latest/
.. _dr_change_log: https://drjit.readthedocs.io/en/latest/changelog.html

Mitsuba 3.6.0 contains a number of significant changes relative to the 
prior release, some breaking, that are predominantly driven by the dependence 
to the new version of `Dr.Jit v1.0.0 <dr_main>`_.

This guide is intended to assist users of prior releases of 
Mitsuba 3 to quickly update their existing codebases to be compatible 
with Mitsuba 3.6.0 and we further highlight some key changes that are potential 
pitfalls.

This guide is by no means comprehensive and we direct users to the 
`Dr.Jit v1.0.0 changelog <dr_change_log>`_ for a more in-depth breakdown of all 
the major Dr.Jit changes.

Symbolic control-flow
---------------------

.. _dr_cflow: https://drjit.readthedocs.io/en/latest/cflow.html#symbolic-mode
.. _dr_while_loop: https://drjit.readthedocs.io/en/latest/reference.html#drjit.while_loop
.. _dr_if_stmt: https://drjit.readthedocs.io/en/latest/reference.html#drjit.if_stmt
.. _dr_select: https://drjit.readthedocs.io/en/latest/reference.html#drjit.select

`Symbolic loops <dr_cflow>`_
in Mitsuba are no longer initialized by constructing a :py:func:`mi.Loop` instance. 
Instead, Dr.Jit v1.0.0 introduces the :py:func:`@dr.syntax`
function decorator that allows users to express symbolic loops as if they were
immediately-evaluated Python control-flow. An example of a simple loop using 
:py:func:`mi.Loop` previously looked like,

.. code-block:: python

  import mitsuba as mi

  var     = mi.Float(32)
  rng     = mi.PCG32(size=102400)
  count   = mi.Float(0)
  loop    = mi.Loop(state=lambda: (var, rng, count))

  while loop(count < 10):
      var     += rng.next_float32()
      count   += 1

  var += 1

and porting this to use the :py:func:`dr.syntax` decorator is relatively 
straight-forward,

.. code-block:: python

  import drjit as dr
  import mitsuba as mi

  var = mi.Float(32)
  rng = mi.PCG32(size=102400)

  @dr.syntax
  def my_loop(var, rng):
      count = mi.Float(0)
      while count < 10:
          var     += rng.next_float32()
          count   += 1

      return var, rng

  var, rng = my_loop(var, rng)
  var += 1


Internally, the :py:func:`@dr.syntax` decorator determines which JIT 
variables should be tracked by the loop that is subsequently re-expressed as a 
:py:func:`dr.while_loop` call. While the `Dr.Jit documentation <dr_while_loop>`_
for :py:func:`dr.while_loop` further describes the implementation details, in 
practice  users should prefer using :py:func:`@dr.syntax` when porting their \
existing code rather than calling :py:func:`dr.while_loop` directly.

The decorator :py:func:`@dr.syntax` is not just limited to loops however and 
`symbolic if-statements <dr_if_stmt>`_, a feature that wasn't available prior to
Dr.Jit v1.0.0, can also be expressed as if they were standard Python if-statements

.. code-block:: python

  import drjit as dr
  import mitsuba as mi

  var = dr.ones(mi.Float, 4)
  rng = mi.PCG32(size=102400)

  @dr.syntax
  def my_if(var, rng):
      count = mi.Float(0, 20, 10, 4)
      if count < 10:
          var += rng.next_float32()
      else:
          var -= 1

      return var, rng

  var, rng = my_if(var, rng)
  var += 1

Similar to loops, :py:func:`@dr.syntax` will internally re-express the 
if-statements as a :py:func:`dr.if_stmt` call.

.. warning::

  Users familiar with Dr.Jit's `select function <dr_select>`_ may be tempted to 
  modify calls to ``dr.select`` to instead use if-statements within a 
  ``@dr.syntax`` decorated function. This is by no means necessary and in many 
  cases may actually be harmful to performance. As a contrived example consider

  .. code-block:: python

    x = dr.arange(mi.Float, 5)
    y = dr.select(x < 2, 1, 2)

  which if we were to unwisely express as an if-statement

  .. code-block:: python

    # Don't do this!
    @dr.syntax 
    def bad_code(x : mi.Float):
      out : mi.Float  = mi.Float(0)
      if x < 2:
        out = mi.Float(1)
      else
        out = mi.Float(2)

      return out

    x = dr.arange(mi.Float, 5)
    y = bad_code()

  is not only more cumbersome to write but will also give you worse performance 
  relative to the ``dr.select`` call. This is because now computantially during
  evaluation, we have to check the condition, perform a jump to either the
  true or false branch of the if-statement and *then* step through the branch
  to perform the output assignment. In contrast, evaluating a ``dr.select`` call
  involves no additonal branching.

  The real benefit of symbolic if-statements are when you have relatively expensive
  operations that only need to be computed within a given branch, because unlike 
  ``dr.select`` calls, computations for both the true or false conditions
  do not have to be evaluated *prior* to evaluating the if-statement itself. 
  In other words, you can potentially avoid a lot of expensive, branch-specific
  computations when the condition for evaluating a particular branch is relatively rare.

  In short, the use of ``dr.select`` still remains perfectly valid in existing 
  codebases and users can judiciously decide when symbolic if-statements should
  be applied.


Dr.Jit horizontal reductions now default to axis=0
--------------------------------------------------

Dr.Jit horizontal reductions such as :py:func:`dr.sum`,
:py:func:`dr.prod` and :py:func:`dr.mean` provide an ``axis`` argument to specify
which axis the reduction will be performed on multi-dimensional types such as
tensors. Prior to Dr.Jit v1.0.0, the omission of a provided ``axis`` argument 
would default to applying the reduction across all axes with the 
argument ``axis=None``.

However, from Dr.Jit v1.0.0 onwards, the default axis argument is set to 
``axis=0``.

The implication of this is that for existing codebases, any horizontal reduction 
calls that omitted specifying an axis, such as

.. code-block:: python

   y = dr.mean(tensor)

will now have to be modified to explicity specify that the reduction has to be 
performed across all axes

.. code-block:: python

   y = dr.mean(tensor, axis=None)


Removal of static ``mi.Transform*`` functions
---------------------------------------------

In prior releases of Mitsuba 3, the collection of ``mi.Transform*`` types could be
instatiated via an initial static function call and then subsequent chained
instance calls, such as

.. code-block:: python

  x = mi.Transform4f.translate([1,2,3]).scale(3.0).rotate([1, 0, 0], 0.5)

However, a common pitfall was that subsequently calling

.. code-block:: python

  y = x.scale(3.0)

would in fact call the static function implementation and hence the value of
``y.matrix`` would unexpectedly be

.. code-block:: python

  [[[3, 0, 0, 0],
  [0, 3, 0, 0],
  [0, 0, 3, 0],
  [0, 0, 0, 1]]]

rather than applied to the existing transform ``x``.

From Mitsuba 3.6.0 onwards, all ``mi.Transform*`` static function have been 
removed and instead a user can default construct the identity transform before
chaining any subsequent transforms

.. code-block:: python

  # mi.Transform4f() is the identity transform
  x = mi.Transform4f().translate([1,2,3]).scale(3.0).rotate([1, 0, 0], 0.5)


Bitmap textures: Half-precision storage by default where possible
-----------------------------------------------------------------

.. _dr_texture: https://drjit.readthedocs.io/en/latest/textures.html
.. _spec_up: https://rgl.epfl.ch/publications/Jakob2019Spectral

Dr.Jit v1.0.0 includes support for half-precision arrays and tensors, and further
extends support for FP16 `Dr.Jit textures <dr_texture>`_ that are 
hardware-accelerated on CUDA backends.

From Mitsuba v3.6.0 onwards, bitmap textures initialized from data with bit
depth 16 or lower will instantiate an underlying half-precision Dr.Jit texture.

.. note::
  Using spectral Mitsuba variants is an exception to this default behavior, and 
  the underlying storage of the bitmap texture will remain consistent to the variant 
  as with previous versions of Mitsuba 3. This is because here sampling a 
  texture requires `spectral upsampling <spec_up>`_ and RGB input data is 
  first converted to their corresponding spectral coefficients.

There may be cases where this default behavior is undersiable. For instance, if a 
user is performing an iterative optimization of a given bitmap texture, a 
potential pitfall is highlighted in the following example

.. code-block:: python

  import mitsuba as mi
  mi.set_variant('cuda_ad_rgb')

  # Bit depth of my_image.png is less than 16 so storage of texture is FP16
  bitmap = mi.load_dict({
      "type" : "bitmap",
      "filename" : "my_image.png"
  })

  params = mi.traverse(bitmap)

  # Want to update the associated tensor but using TensorXf (single-precision)
  x = dr.ones(mi.TensorXf, shape=(9,10,3))

  # Implicit conversion from TensorXf to TensorXf16
  params['data'] = x
  params.update()

  type(params['data']) # TensorXf16 not TensorXf

The above example is somewhat contrived because in practice, for an optimization, a 
user would likely initialize their bitmap texture from a tensor and hence the 
underlying storage precision would be explicitly specified. Regardless, opting
out of this default behavior is possible by setting the plugin ``format`` 
parameter to ``variant``

.. code-block:: python

  import mitsuba as mi
  mi.set_variant('cuda_ad_rgb')

  # Storage precision is consistent with variant specified (i.e. float)
  bitmap = mi.load_dict({
      "type" : "bitmap",
      "filename" : "my_image.png"
      "format" : "variant"
  })

  params = mi.traverse(bitmap)
  type(params['data']) # TensorXf

Miscellaneous
-------------

* Dr.Jit v1.0.0 raises the minimum supported LLVM version to 11
* Rename of function ``dr.clamp`` to ``dr.clip``
* Rename of function ``dr.sqr`` to ``dr.square``
* Rename of function decorator ``dr.wrap_ad`` to ``dr.wrap``
