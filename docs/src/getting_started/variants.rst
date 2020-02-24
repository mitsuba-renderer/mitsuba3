Rendering variants
==================

Mitsuba 2 is a *retargetable* rendering system: it can be run in different rendering
modes that either change the representation of color (enabling spectral or polarized
rendering) or even the basic arithmetic type used by the system (lifting the system
to a vectorized renderer or enabling automatic differentiation).
All of this is achieved with a single codebase.

Spectral rendering
------------------

Compared to usual RGB rendering modes, Mitsuba 2 can also perform full spectral
rendering by performing additional Monte Carlo integration over the (visible)
wavelengths. This can considerably improve accuracy especially in scenarios where
measured spectral data is available. Consider for example the two Cornell box
renderings below: on the left side, the spectral reflectance data of all materials
is first converted to RGB and rendering using the RGB rendering mode (`scalar_rgb`).
In contrast, running the simulation in full spectral mode (`scalar_spectral`) results
in a surprisingly different image.

.. subfigstart::
.. subfigure:: ../../../resources/data/docs/images/render/variants_cbox_rgb.jpg
   :caption: RGB Mode
.. subfigure:: ../../../resources/data/docs/images/render/variants_cbox_spectral.jpg
   :caption: Spectral Mode
.. subfigend::
   :label: fig-cbox-spectral

This is achieved with an implementation of *Hero wavelength sampling* :cite:`Wilkie2014Hero`.
In case some data is only available in a RGB format (e.g. for image textures), Mitsuba 2
performs spectral upsampling :cite:`Jakob2019Spectral` and will transform the data to
continuous spectra that can be sampled for arbitrary wavelengths.

Polarized rendering
-------------------

Optionally, Mitsuba 2 supports polarized rendering modes (e.g. `scalar_spectral_polarized`)
which, in addition to normal radiance, also track the full polarization state of light.
Inside the light transport simulation, *Stokes vectors* are used to parameterize
the elliptical shape of the transverse oscillations, and *Mueller matrices* are used
to compute the effect of surface scattering on the polarization :cite:`Collett1993PolarizedLight`.

.. image:: ../../images/polarization_wave.svg
    :width: 60%
    :align: center

For more details regarding the implementation of the polarized rendering modes, please
refer to the :ref:`developer_guide-polarization` section in the developer guide.

Differentiable rendering
------------------------

*TODO*

Vectorized rendering
--------------------

*TODO*