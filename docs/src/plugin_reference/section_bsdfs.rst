.. _sec-bsdfs:

BSDFs
=====

Acoustic materials describe how a surface reflects, absorbs, and scatters
incident sound energy — the acoustic analogue of a light-transport BSDF.
misuka currently ships a single acoustic material, :ref:`acousticbsdf
<bsdf-acousticbsdf>`, which combines a frequency-dependent absorption
coefficient with a specular/diffuse scattering split (see :ref:`frequency-domain
spectra <sec-spectra-acoustic>` for how the frequency-value curves are defined).

As in Mitsuba, BSDFs are assigned to *shapes*, either nested directly or
declared once and referenced by name — useful when the same material is
reused across many shapes, e.g. all six walls of a shoebox room:

.. tabs::
    .. code-tab:: xml

        <scene version="3.0.0">
            <!-- Creating a named acoustic material for later use -->
            <bsdf type="acousticbsdf" id="wall">
                <spectrum name="absorption" value="100:0.1, 20000:0.4"/>
                <spectrum name="scattering" value="100:0.2, 20000:0.8"/>
            </bsdf>

            <shape type="rectangle">
                <!-- Example of referencing a named material -->
                <ref id="wall"/>
            </shape>
        </scene>

    .. code-tab:: python

        'type': 'scene',
        # Create a named acoustic material for later use
        'wall': {
            'type': 'acousticbsdf',
            'absorption': {'type': 'spectrum', 'value': [(100, 0.1), (20000, 0.4)]},
            'scattering': {'type': 'spectrum', 'value': [(100, 0.2), (20000, 0.8)]},
        },

        'shape_id': {
            'type': 'rectangle',
            # Example of referencing a named material
            'bsdf': {'type': 'ref', 'id': 'wall'},
        }

It is generally more economical to use named materials when the same one is
reused across several shapes, since this reduces internal memory usage.
