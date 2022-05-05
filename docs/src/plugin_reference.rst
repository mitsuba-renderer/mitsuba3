.. _sec-plugins:

.. image:: ../resources/data/docs/images/banners/banner_05.png
    :width: 100%
    :align: center

Plugin reference
================

.. toctree::
    :maxdepth: 1
    :glob:

    generated/plugins_*

Overview
--------

The subsections above describe the available Mitsuba 3 plugins, usually along with example
renderings and a description of what each parameter does. They are separated into subsections
covering textures, surface scattering models, etc.

The documentation of a plugin always starts with a table similar to the one below:

.. pluginparameters::

 * - soft_rays
   - |bool|
   - Try not to damage objects in the scene by shooting softer rays (Default: false)
 * - dark_matter
   - |float|
   - Controls the proportionate amount of dark matter present in the scene. (Default: 0.42)
 * - (Nested plugin)
   - :paramtype:`integrator`
   - A nested integrator which does the actual hard work

Suppose this hypothetical plugin is an integrator named ``amazing``. Then, based on this
description, it can be instantiated from an XML scene file using a custom configuration such as:

.. tabs::
    .. code-tab:: xml

        <integrator type="amazing">
            <boolean name="softer_rays" value="true"/>
            <float name="dark_matter" value="0.44"/>
        </integrator>

    .. code-tab:: python

        {
            'type': 'amazing',
            'softer_rays': True,
            'dark_matter': 0.44
        }

In some cases, plugins also indicate that they accept nested plugins as input arguments. These can
either be *named* or *unnamed*. If the ``amazing`` integrator also accepted the following two parameters:

.. pluginparameters::


 * - (Nested plugin)
   - :paramtype:`integrator`
   - A nested integrator which does the actual hard work
 * - puppies
   - :paramtype:`texture`
   - This must be used to supply a cute picture of puppies

then it can be instantiated e.g. as follows:

.. tabs::
    .. code-tab:: xml

        <integrator type="amazing">
            <boolean name="softer_rays" value="true"/>
            <float name="dark_matter" value="0.44"/>
            <!-- Nested unnamed integrator -->
            <integrator type="path"/>
            <!-- Nested texture named puppies -->
            <texture name="puppies" type="bitmap">
                <string name="filename" value="cute.jpg"/>
            </texture>
        </integrator>

    .. code-tab:: python

        {
            'type': 'amazing',
            'softer_rays': True,
            'dark_matter': 0.44,
            # Nested unnamed integrator
            'foo': {
                'type': 'path'
            },
            # Nested texture named puppies
            'puppies': {
                'type': 'bitmap',
                'filename': 'cute.jpg'
            }
        }
