Porting to Mitsuba 3.6.0
========================

.. _dr_main: https://drjit.readthedocs.io/en/latest/

Mitsuba 3.6.0 contains a number of significant changes relative to the 
prior release, some breaking, that are predominantly driven by the dependence 
to the new version of `Dr.Jit 1.0.0 <dr_main>`_.

This guide is intended to assist users of prior releases of 
Mitsuba 3 to quickly update their existing codebases to be compatible 
with Mitsuba 3.6.0 and we further highlight some key changes that are potential 
pitfalls.

This guide is by no means comprehensive and we direct users to the 
`Dr.Jit documentation <dr_main>`_ that contains several dedicated sections
on the design and core features of Dr.Jit 1.0.0 that Mitsuba users will find 
invaluable. Historically, the Dr.Jit documentation in past releases has been 
sparse so it's recommended that even more advanced users begin here.

Symbolic control-flow
---------------------

.. _dr_cflow: https://drjit.readthedocs.io/en/latest/cflow.html#symbolic-mode

`Symbolic loops <dr_cflow>`_
in Mitsuba are no longer initialized by constructing a ``mitsuba.Loop`` 
instance. Instead, Dr.Jit 1.0.0 introduces the :py:func:`drjit.syntax` function 
decorator that allows users to express symbolic loops as if they were 
immediately-evaluated Python control-flow. An example of a simple loop using 
``mitsuba.Loop`` previously looked like,

.. code-block:: python

  import mitsuba as mi

  var = mi.Float(32)
  rng = mi.PCG32(size=102400)

  def foo(var, rng):
    count   = mi.UInt(0)
    loop    = mi.Loop(state=lambda: (var, rng, count))

    while loop(count < 10):
      var     += rng.next_float32()
      count   += 1

    return var, rng

  var, rng = foo(var, rng)
  var += 1

and porting this to use the :py:func:`drjit.syntax` decorator is relatively 
straightforward,

.. code-block:: python

  import drjit as dr
  import mitsuba as mi

  var = mi.Float(32)
  rng = mi.PCG32(size=102400)

  @dr.syntax
  def foo(var, rng):
    count = mi.UInt(0)

    while count < 10:
      var     += rng.next_float32()
      count   += 1

    return var, rng

  var, rng = foo(var, rng)
  var += 1


Previously when using ``mitsuba.Loop``, it was necessary for the user to 
determine the loop variables required, which was bug-prone. 
:py:func:`drjit.syntax` automates this step and internally reexpresses the loop 
as a :py:func:`drjit.while_loop` call. While the Dr.Jit API reference details 
how to directly call :py:func:`drjit.while_loop`, users should prefer using 
:py:func:`drjit.syntax` when porting their existing code.

Prior to Dr.Jit 1.0.0, tracing if-statements containing JIT variables was not 
possible, and the only alternative was to replace conditionals with masked 
operations, for example using :py:func:`drjit.select`. As experience has shown, 
converting conditional code into masked form can be rather tedious and bug-prone.

Therefore, the :py:func:`dr.syntax` annotation additionally handles if-statements 
analogously to while loops, where internally such statements are reexpressed as 
:py:func:`drjit.if_stmt` calls. Masked code remains valid but it is often no 
longer needed.

.. warning::

  While changing existing codebases to leverage symbolic if-statements can 
  improve both readability and performance, it's important to highlight 
  computational differences relative to :py:func:`drjit.select`. As a 
  contrived example consider

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
  relative to the :py:func:`drjit.select` call. This is because now during 
  evaluation, we have to check the condition, perform a jump to either the true 
  or false branch of the if-statement and *then* step through the branch to 
  perform the output assignment. In contrast, evaluating a 
  :py:func:`drjit.select` call involves no additonal branching.

  The real performance benefit of symbolic if-statements are when you have 
  relatively expensive operations that only need to be computed within a given 
  branch, because unlike  :py:func:`drjit.select` calls, computations for both 
  the true or false conditions do not have to be evaluated *prior* to evaluating 
  the if-statement itself. In other words, you can potentially avoid a lot of 
  expensive, branch-specific computations when the condition for evaluating a 
  particular branch is relatively rare.


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

Dr.Jit 1.0.0 includes support for half-precision arrays and tensors, and further
extends support for FP16 `Dr.Jit textures <dr_texture>`_ that are 
hardware-accelerated on CUDA backends.

From Mitsuba 3.6.0 onwards, bitmap textures initialized from data with bit
depth 16 or lower will instantiate an underlying half-precision Dr.Jit texture.

.. note::
  Using spectral Mitsuba variants is an exception to this default behavior, and 
  the underlying storage of the bitmap texture will remain consistent to the variant 
  as with previous versions of Mitsuba 3. This is because here sampling a 
  texture requires `spectral upsampling <spec_up>`_ and RGB input data is 
  first converted to their corresponding spectral coefficients.

There may be cases where this default behavior is undesirable. For instance, if 
a user is performing an iterative optimization of a given bitmap texture, a 
potential pitfall is highlighted in the following example

.. code-block:: python

  import mitsuba as mi
  import drjit as dr
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
* Rename of function ``drjit.clamp`` to :py:func:`drjit.clip`
* Rename of function ``drjit.sqr`` to :py:func:`drjit.square`
* Rename of function decorator ``drjit.wrap_ad`` to :py:func:`drjit.wrap`
