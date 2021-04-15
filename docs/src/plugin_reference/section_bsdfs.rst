.. _sec-bsdfs:

BSDFs
=====

    .. image:: ../../resources/data/docs/images/bsdf/bsdf_overview.jpg
        :width: 100%
        :align: center

    Schematic overview of the most important surface scattering models in Mitsuba 2.
    The arrows indicate possible outcomes of an interaction with a surface that has
    the respective model applied to it.

Surface scattering models describe the manner in which light interacts
with surfaces in the scene. They conveniently summarize the mesoscopic
scattering processes that take place within the material and
cause it to look the way it does.
This represents one central component of the material system in Mitsuba 2---another
part of the renderer concerns itself with what happens
*in between* surface interactions. For more information on this aspect,
please refer to the sections regarding participating media.
This section presents an overview of all surface scattering models that are
supported, along with their parameters.

To achieve realistic results, Mitsuba 2 comes with a library of general-purpose
surface scattering models such as glass, metal, or plastic.
Some model plugins can also act as modifiers that are applied on top of one or
more scattering models.

Throughout the documentation and within the scene description
language, the word BSDF is used synonymously with the term *surface
scattering model*. This is an abbreviation for *Bidirectional
Scattering Distribution Function*, a more precise technical
term.

In Mitsuba 2, BSDFs are assigned to *shapes*, which describe the visible surfaces in
the scene. In the scene description language, this assignment can
either be performed by nesting BSDFs within shapes, or they can
be named and then later referenced by their name.
The following fragment shows an example of both kinds of usages:

.. code-block:: xml

    <scene version=2.0.0>
        <!-- Creating a named BSDF for later use -->
        <bsdf type=".. BSDF type .." id="my_named_material">
            <!-- BSDF parameters go here -->
        </bsdf>

        <shape type="sphere">
            <!-- Example of referencing a named material -->
            <ref id="my_named_material"/>
        </shape>

        <shape type="sphere">
            <!-- Example of instantiating an unnamed material -->
            <bsdf type=".. BSDF type ..">
                <!-- BSDF parameters go here -->
            </bsdf>
        </shape>
    </scene>

It is generally more economical to use named BSDFs when they
are used in several places, since this reduces the internal memory usage.

.. _bsdf-correctness:

Correctness considerations
--------------------------

A vital consideration when modeling a scene in a physically-based rendering
system is that the used materials do not violate physical properties, and
that their arrangement is meaningful. For instance, imagine having designed
an architectural interior scene that looks good except for a white desk that
seems a bit too dark. A closer inspection reveals that it uses a Lambertian
material with a diffuse reflectance of 0.9.

In many rendering systems, it would be feasible to increase the
reflectance value above 1.0 in such a situation. But in Mitsuba, even a
small surface that reflects a little more light than it receives will
likely break the available rendering algorithms, or cause them to produce otherwise
unpredictable results. In fact, the right solution in this case would be to switch to
a different the lighting setup that causes more illumination to be received by
the desk and then *reduce* the material's reflectance---after all, it is quite unlikely that
one could find a real-world desk that reflects 90% of all incident light.

As another example of the necessity for a meaningful material description, consider
the glass model illustrated in the figure below. Here, careful thinking
is needed to decompose the object into boundaries that mark index of
refraction-changes. If this is done incorrectly and a beam of light can
potentially pass through a sequence of incompatible index of refraction changes (e.g. 1.00 to 1.33
followed by 1.50 to 1.33), the output is undefined and will quite likely
even contain inaccuracies in parts of the scene that are far
away from the glass.

.. figtable::
    :label: fig-glass-explanation
    :caption: Some of the scattering models in Mitsuba need to know the indices of refraction on the exterior and
              interior-facing side of a surface. It is therefore important to decompose the mesh into meaningful
              separate surfaces corresponding to each index of refraction change. The example here shows such a
              decomposition for a water-filled Glass.
    :alt: Glass interfaces explanation

    .. figure:: ../../resources/data/docs/images/bsdf/glass_explanation.svg
        :alt: Glass interfaces explanation
        :width: 95%
        :align: center


