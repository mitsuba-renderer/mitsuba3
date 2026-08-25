.. _sec-distributing-plugins:

Distributing Python plugins
===========================

This section describes how to *redistribute* Python plugins that extend
Mitsuba. By packaging them as described below, users can run ``pip install``
on their device to make the plugin available to Mitsuba.

A minimal package
-----------------

Create a new folder holding your Python extensions along with a file
``pyproject.toml`` providing metadata about the package.

.. code-block:: toml

    # pyproject.toml
    [project]
    name = "mitsuba-myplugin"
    version = "0.1.0"
    dependencies = ["mitsuba>=3.10,<4"]

    [project.entry-points.mitsuba]
    myplugin = "mitsuba_myplugin"

In this case ``mitsuba-myplugin`` is the name of the package on PyPI,
``myplugin`` is an arbitrary key, and ``mitsuba_myplugin`` is the installed
module name. The ``project.entry-points.mitsuba`` entry point allows Mitsuba
to automatically find this module when the package is installed.

The second file is the module itself. It looks exactly like the code from the
:ref:`custom plugin tutorial <sec-other-tutos>`: it defines its classes and
registers them at module scope.

.. code-block:: python

    # mitsuba_myplugin/__init__.py
    import mitsuba as mi

    class MyBSDF(mi.BSDF):
        def __init__(self, props):
            super().__init__(props)
        # ... sample(), eval() and pdf() as in the tutorial ...

    mi.register_bsdf('mybsdf', lambda props: MyBSDF(props))

Mitsuba imports this module the first time a variant becomes active and reloads
it after every subsequent variant change. Reloading is needed because
``mi.BSDF`` refers to a different class in each variant, and because
:py:func:`mitsuba.register_bsdf` and its siblings (see the :ref:`API reference
<sec-api>`) only apply to the variant that is active at the time of the call.

After a user installs this package via ``pip install`` (e.g., from
[PyPI](https://pypi.org/), the plugin can be used:

.. code-block:: python

    import mitsuba as mi
    mi.set_variant('cuda_ad_rgb')

    scene = mi.load_dict({
        'type': 'scene',
        'sphere': {'type': 'sphere', 'bsdf': {'type': 'mybsdf'}}
    })

A package that spreads its plugins over several files should declare one entry
point per file, since reloading a package does not reload its submodules:

.. code-block:: toml

    [project.entry-points.mitsuba]
    myplugin_bsdf = "mitsuba_myplugin.bsdf"
    myplugin_integrator = "mitsuba_myplugin.integrator"

Introspection
-------------

Plugin code may need to adapt to the Mitsuba build that loaded it. The
following are available at module scope:

- ``mi.MI_VERSION`` and :py:class:`mitsuba.Version` for comparisons such as
  ``mi.Version(mi.MI_VERSION) < mi.Version('3.10.0')``, e.g. to work around an
  incompatibility in a specific release.
- ``mi.variant()`` and the flags ``mi.is_jit``, ``mi.is_ad``, ``mi.is_cuda``,
  ``mi.is_spectral``, etc. described in :ref:`sec-variants-introspection`.
- ``mi.MI_ENABLE_EMBREE``, ``mi.MI_ENABLE_CUDA`` and ``mi.MI_ENABLE_METAL``
  report what the installed binary supports.

If a plugin module fails to import, Mitsuba emits a warning that identifies the
responsible entry point and continues loading the others. Setting the
environment variable ``MI_DISABLE_AUTOLOAD`` skips the search entirely.
