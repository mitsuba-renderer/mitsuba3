Mitsuba 3
=========

.. only:: not latex

    .. image:: images/mitsuba-logo-white-bg.png
        :width: 75%
        :align: center

Mitsuba 3 is a research-oriented rendering system for forward and inverse
simulation. It consists of a small set of core libraries and a wide variety of
plugins that implement functionality ranging from materials and light sources to complete
rendering algorithms. Mitsuba 3 strives to retain scene compatibility with its
predecessors: `Mitsuba 0.6 <https://github.com/mitsuba-renderer/mitsuba>`_ and
`Mitsuba 2 <https://github.com/mitsuba-renderer/mitsuba2>`_.
However, in most other respects, it is a completely new system following a
different set of goals.

.. .............................................................................

.. panels::
    :header: text-center font-weight-bold
    :body: text-center

    Getting Started
    ^^^^^^^^^^^^^^^

    .. image:: ../resources/data/docs/images/logos/getting-started-logo.png
        :width: 20%
        :align: center

    Learn about the main concepts.

    .. link-button:: src/getting_started/intro
        :type: ref
        :text:
        :classes: btn-link stretched-link

    ---

    How-to Guides
    ^^^^^^^^^^^^^

    .. image:: ../resources/data/docs/images/logos/how-to-guide-logo.png
        :width: 20%
        :align: center

    Explore specific problems step by step.

    .. link-button:: src/how_to_guides/intro
        :type: ref
        :text:
        :classes: btn-link stretched-link

    ---

    Key Topics
    ^^^^^^^^^^

    .. image:: ../resources/data/docs/images/logos/key-topics-logo.png
        :width: 20%
        :align: center

    Dive deep in advanced topics.

    .. link-button:: src/key_topics/overview
        :type: ref
        :text:
        :classes: btn-link stretched-link

    ---

    Developer's Guide
    ^^^^^^^^^^^^^^^^^

    .. image:: ../resources/data/docs/images/logos/developer-logo.png
        :width: 20%
        :align: center

    For the bravest of all.

    .. link-button:: src/developer_guide/intro
        :type: ref
        :text:
        :classes: btn-link stretched-link

    ---

    Plugin reference
    ^^^^^^^^^^^^^^^^

    .. image:: ../resources/data/docs/images/logos/plugin-reference-logo.png
        :width: 20%
        :align: center


    .. link-button:: src/plugin_reference/intro
        :type: ref
        :text:
        :classes: btn-link stretched-link

    ---

    API reference
    ^^^^^^^^^^^^^

    .. image:: ../resources/data/docs/images/logos/api-reference-logo.png
        :width: 20%
        :align: center

    .. link-button:: src/api_reference/intro
        :type: ref
        :text:
        :classes: btn-link stretched-link

.. icons from https://uxwing.com

.. .............................................................................

.. toctree::
    :maxdepth: 1
    :caption: Getting Started
    :hidden:

    src/getting_started/intro
    tutorials/basics/index
    tutorials/inverse_rendering/index

.. .............................................................................

.. toctree::
    :maxdepth: 1
    :caption: How-to Guides
    :hidden:

    src/how_to_guides/intro

.. .............................................................................

.. toctree::
    :maxdepth: 1
    :caption: Key Topics
    :hidden:

    src/key_topics/overview
    src/key_topics/variants
    src/key_topics/file_format
    src/key_topics/polarization

.. .............................................................................

.. toctree::
    :maxdepth: 1
    :caption: Developer's Guide
    :hidden:

    src/developer_guide/intro
    src/developer_guide/compiling

.. .............................................................................

.. toctree::
    :maxdepth: 1
    :caption: Plugin reference
    :hidden:
    :glob:

    src/plugin_reference/intro
    generated/plugins_*

.. .............................................................................

.. toctree::
    :maxdepth: 1
    :caption: API reference
    :hidden:

    src/api_reference/intro
    generated/core_api
    generated/render_api
    generated/python_api

.. .............................................................................

.. toctree::
    :maxdepth: 1
    :caption: Miscellaneous
    :hidden:

    zz_bibliography
