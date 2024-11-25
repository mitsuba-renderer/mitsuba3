.. py:class:: mitsuba.AdjointIntegrator

    Base class: :py:obj:`mitsuba.Integrator`

    Abstract adjoint integrator that performs Monte Carlo sampling
    starting from the emitters.

    Subclasses of this interface must implement the sample() method, which
    performs recursive Monte Carlo integration starting from an emitter
    and directly accumulates the product of radiance and importance into
    the film. The render() method then repeatedly invokes this estimator
    to compute the rendered image.

    Remark:
        The adjoint integrator does not support renderings with arbitrary
        output variables (AOVs).

    .. py:method:: __init__(self, arg)

        Parameter ``arg`` (:py:obj:`mitsuba.Properties`, /):
            *no description available*


    .. py:method:: mitsuba.AdjointIntegrator.render_backward(self, scene, params, grad_in, sensor, seed=0, spp=0)

        Parameter ``scene`` (:py:obj:`mitsuba.Scene`):
            *no description available*

        Parameter ``params`` (object):
            *no description available*

        Parameter ``grad_in`` (drjit.llvm.ad.TensorXf):
            *no description available*

        Parameter ``sensor`` (:py:obj:`mitsuba.Sensor`):
            *no description available*

        Parameter ``seed`` (int):
            *no description available*

        Parameter ``spp`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.AdjointIntegrator.render_forward(self, scene, params, sensor, seed=0, spp=0)

        Parameter ``scene`` (:py:obj:`mitsuba.Scene`):
            *no description available*

        Parameter ``params`` (object):
            *no description available*

        Parameter ``sensor`` (:py:obj:`mitsuba.Sensor`):
            *no description available*

        Parameter ``seed`` (int):
            *no description available*

        Parameter ``spp`` (int):
            *no description available*

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.AdjointIntegrator.sample(self, scene, sensor, sampler, block, sample_scale)

        Sample the incident importance and splat the product of importance and
        radiance to the film.

        Parameter ``scene`` (:py:obj:`mitsuba.Scene`):
            The underlying scene

        Parameter ``sensor`` (:py:obj:`mitsuba.Sensor`):
            A sensor from which rays should be sampled

        Parameter ``sampler`` (:py:obj:`mitsuba.Sampler`):
            A source of (pseudo-/quasi-) random numbers

        Parameter ``block`` (:py:obj:`mitsuba.ImageBlock`):
            An image block that will be updated during the sampling process

        Parameter ``sample_scale`` (float):
            A scale factor that must be applied to each sample to account for
            the film resolution and number of samples.

        Returns → None:
            *no description available*

.. py:class:: mitsuba.Appender

    Base class: :py:obj:`mitsuba.Object`

    This class defines an abstract destination for logging-relevant
    information

    .. py:method:: __init__()


    .. py:method:: mitsuba.Appender.append(self, level, text)

        Append a line of text with the given log level

        Parameter ``level`` (:py:obj:`mitsuba.LogLevel`):
            *no description available*

        Parameter ``text`` (str):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Appender.log_progress(self, progress, name, formatted, eta, ptr=None)

        Process a progress message

        Parameter ``progress`` (float):
            Percentage value in [0, 100]

        Parameter ``name`` (str):
            Title of the progress message

        Parameter ``formatted`` (str):
            Formatted string representation of the message

        Parameter ``eta`` (str):
            Estimated time until 100% is reached.

        Parameter ``ptr`` (types.CapsuleType | None):
            Custom pointer payload. This is used to express the context of a
            progress message. When rendering a scene, it will usually contain
            a pointer to the associated ``RenderJob``.

        Returns → None:
            *no description available*

.. py:class:: mitsuba.ArgParser

    Minimal command line argument parser

    This class provides a minimal cross-platform command line argument
    parser in the spirit of to GNU getopt. Both short and long arguments
    that accept an optional extra value are supported.

    The typical usage is

    .. code-block:: c

        ArgParser p;
        auto arg0 = p.register("--myParameter");
        auto arg1 = p.register("-f", true);
        p.parse(argc, argv);
        if (*arg0)
            std::cout << "Got --myParameter" << std::endl;
        if (*arg1)
            std::cout << "Got -f " << arg1->value() << std::endl;


    .. py:method:: __init__()


    .. py:method:: mitsuba.ArgParser.add(self, prefix, extra=False)

        Overloaded function.

        1. ``add(self, prefix: str, extra: bool = False) -> :py:obj:`mitsuba.ArgParser.Arg```

        Register a new argument with the given list of prefixes

        Parameter ``prefixes``:
            A list of command prefixes (i.e. {"-f", "--fast"})

        Parameter ``extra`` (bool):
            Indicates whether the argument accepts an extra argument value

        2. ``add(self, prefixes: collections.abc.Sequence[str], extra: bool = False) -> :py:obj:`mitsuba.ArgParser.Arg```

        Register a new argument with the given prefix

        Parameter ``prefix`` (str):
            A single command prefix (i.e. "-f")

        Parameter ``extra`` (bool):
            Indicates whether the argument accepts an extra argument value

        Returns → :py:obj:`mitsuba.ArgParser.Arg`:
            *no description available*

    .. py:method:: mitsuba.ArgParser.executable_name()

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.ArgParser.parse(self, arg)

        Parse the given set of command line arguments

        Parameter ``arg`` (collections.abc.Sequence[str], /):
            *no description available*

        Returns → None:
            *no description available*

.. py:class:: mitsuba.ArrayXb

.. py:class:: mitsuba.ArrayXf

.. py:class:: mitsuba.ArrayXi

.. py:class:: mitsuba.ArrayXi64

.. py:class:: mitsuba.ArrayXu

.. py:class:: mitsuba.ArrayXu64

.. py:class:: mitsuba.AtomicFloat

    Atomic floating point data type

    The class implements an an atomic floating point data type (which is
    not possible with the existing overloads provided by ``std::atomic``).
    It internally casts floating point values to an integer storage format
    and uses atomic integer compare and exchange operations to perform
    changes.

    .. py:method:: __init__(self, arg)

        Initialize the AtomicFloat with a given floating point value

        Parameter ``arg`` (float, /):
            *no description available*

        
.. py:class:: mitsuba.BSDF

    Base class: :py:obj:`mitsuba.Object`

    Bidirectional Scattering Distribution Function (BSDF) interface

    This class provides an abstract interface to all %BSDF plugins in
    Mitsuba. It exposes functions for evaluating and sampling the model,
    and for querying associated probability densities.

    By default, functions in class sample and evaluate the complete BSDF,
    but it also allows to pick and choose individual components of multi-
    lobed BSDFs based on their properties and component indices. This
    selection is specified using a context data structure that is provided
    along with every operation.

    When polarization is enabled, BSDF sampling and evaluation returns 4x4
    Mueller matrices that describe how scattering changes the polarization
    state of incident light. Mueller matrices (e.g. for mirrors) are
    expressed with respect to a reference coordinate system for the
    incident and outgoing direction. The convention used here is that
    these coordinate systems are given by ``coordinate_system(wi)`` and
    ``coordinate_system(wo)``, where 'wi' and 'wo' are the incident and
    outgoing direction in local coordinates.

    See also:
        :py:obj:`mitsuba.BSDFContext`

    See also:
        :py:obj:`mitsuba.BSDFSample3f`

    .. py:method:: __init__(self, props)

        Parameter ``props`` (:py:obj:`mitsuba.Properties`):
            *no description available*


    .. py:method:: mitsuba.BSDF.component_count(self, active=True)

        Number of components this BSDF is comprised of.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.BSDF.eval(self, ctx, si, wo, active=True)

        Evaluate the BSDF f(wi, wo) or its adjoint version f^{*}(wi, wo) and
        multiply by the cosine foreshortening term.

        Based on the information in the supplied query context ``ctx``, this
        method will either evaluate the entire BSDF or query individual
        components (e.g. the diffuse lobe). Only smooth (i.e. non Dirac-delta)
        components are supported: calling ``eval()`` on a perfectly specular
        material will return zero.

        Note that the incident direction does not need to be explicitly
        specified. It is obtained from the field ``si.wi``.

        Parameter ``ctx`` (:py:obj:`mitsuba.BSDFContext`):
            A context data structure describing which lobes to evaluate, and
            whether radiance or importance are being transported.

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            A surface interaction data structure describing the underlying
            surface position. The incident direction is obtained from the
            field ``si.wi``.

        Parameter ``wo`` (:py:obj:`mitsuba.Vector3f`):
            The outgoing direction

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Color3f`:
            *no description available*

    .. py:method:: mitsuba.BSDF.eval_attribute(self, name, si, active=True)

        Evaluate a specific BSDF attribute at the given surface interaction.

        BSDF attributes are user-provided fields that provide extra
        information at an intersection. An example of this would be a per-
        vertex or per-face color on a triangle mesh.

        Parameter ``name`` (str):
            Name of the attribute to evaluate

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            Surface interaction associated with the query

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Color3f`:
            An unpolarized spectral power distribution or reflectance value

    .. py:method:: mitsuba.BSDF.eval_attribute_1(self, name, si, active=True)

        Monochromatic evaluation of a BSDF attribute at the given surface
        interaction

        This function differs from eval_attribute() in that it provided raw
        access to scalar intensity/reflectance values without any color
        processing (e.g. spectral upsampling).

        Parameter ``name`` (str):
            Name of the attribute to evaluate

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            Surface interaction associated with the query

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            An scalar intensity or reflectance value

    .. py:method:: mitsuba.BSDF.eval_attribute_3(self, name, si, active=True)

        Trichromatic evaluation of a BSDF attribute at the given surface
        interaction

        This function differs from eval_attribute() in that it provided raw
        access to RGB intensity/reflectance values without any additional
        color processing (e.g. RGB-to-spectral upsampling).

        Parameter ``name`` (str):
            Name of the attribute to evaluate

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            Surface interaction associated with the query

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Color3f`:
            An trichromatic intensity or reflectance value

    .. py:method:: mitsuba.BSDF.eval_diffuse_reflectance(self, si, active=True)

        Evaluate the diffuse reflectance

        This method approximates the total diffuse reflectance for a given
        direction. For some materials, an exact value can be computed
        inexpensively. When this is not possible, the value is approximated by
        evaluating the BSDF for a normal outgoing direction and returning this
        value multiplied by pi. This is the default behaviour of this method.

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            A surface interaction data structure describing the underlying
            surface position.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Color3f`:
            *no description available*

    .. py:method:: mitsuba.BSDF.eval_null_transmission(self, si, active=True)

        Evaluate un-scattered transmission component of the BSDF

        This method will evaluate the un-scattered transmission
        (BSDFFlags::Null) of the BSDF for light arriving from direction ``w``.
        The default implementation returns zero.

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            A surface interaction data structure describing the underlying
            surface position. The incident direction is obtained from the
            field ``si.wi``.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Color3f`:
            *no description available*

    .. py:method:: mitsuba.BSDF.eval_pdf(self, ctx, si, wo, active=True)

        Jointly evaluate the BSDF f(wi, wo) and the probability per unit solid
        angle of sampling the given direction. The result from the evaluated
        BSDF is multiplied by the cosine foreshortening term.

        Based on the information in the supplied query context ``ctx``, this
        method will either evaluate the entire BSDF or query individual
        components (e.g. the diffuse lobe). Only smooth (i.e. non Dirac-delta)
        components are supported: calling ``eval()`` on a perfectly specular
        material will return zero.

        This method provides access to the probability density that would
        result when supplying the same BSDF context and surface interaction
        data structures to the sample() method. It correctly handles changes
        in probability when only a subset of the components is chosen for
        sampling (this can be done using the BSDFContext::component and
        BSDFContext::type_mask fields).

        Note that the incident direction does not need to be explicitly
        specified. It is obtained from the field ``si.wi``.

        Parameter ``ctx`` (:py:obj:`mitsuba.BSDFContext`):
            A context data structure describing which lobes to evaluate, and
            whether radiance or importance are being transported.

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            A surface interaction data structure describing the underlying
            surface position. The incident direction is obtained from the
            field ``si.wi``.

        Parameter ``wo`` (:py:obj:`mitsuba.Vector3f`):
            The outgoing direction

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Color3f`, drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.BSDF.eval_pdf_sample(self, ctx, si, wo, sample1, sample2, active=True)

        Jointly evaluate the BSDF f(wi, wo) and the probability per unit solid
        angle of sampling the given direction. The result from the evaluated
        BSDF is multiplied by the cosine foreshortening term.

        Based on the information in the supplied query context ``ctx``, this
        method will either evaluate the entire BSDF or query individual
        components (e.g. the diffuse lobe). Only smooth (i.e. non Dirac-delta)
        components are supported: calling ``eval()`` on a perfectly specular
        material will return zero.

        This method provides access to the probability density that would
        result when supplying the same BSDF context and surface interaction
        data structures to the sample() method. It correctly handles changes
        in probability when only a subset of the components is chosen for
        sampling (this can be done using the BSDFContext::component and
        BSDFContext::type_mask fields).

        Note that the incident direction does not need to be explicitly
        specified. It is obtained from the field ``si.wi``.

        Parameter ``ctx`` (:py:obj:`mitsuba.BSDFContext`):
            A context data structure describing which lobes to evaluate, and
            whether radiance or importance are being transported.

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            A surface interaction data structure describing the underlying
            surface position. The incident direction is obtained from the
            field ``si.wi``.

        Parameter ``wo`` (:py:obj:`mitsuba.Vector3f`):
            The outgoing direction

        Parameter ``sample1`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``sample2`` (:py:obj:`mitsuba.Point2f`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Color3f`, drjit.llvm.ad.Float, :py:obj:`mitsuba.BSDFSample3f`, :py:obj:`mitsuba.Color3f`]:
            *no description available*

    .. py:method:: mitsuba.BSDF.flags(self, index, active=True)

        Overloaded function.

        1. ``flags(self, index: int, active: drjit.llvm.ad.Bool = True) -> int``

        Flags for a specific component of this BSDF.

        2. ``flags(self) -> int``

        Flags for all components combined.

        Parameter ``index`` (int):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.BSDF.has_attribute(self, name, active=True)

        Returns whether this BSDF contains the specified attribute.

        Parameter ``name`` (str):
            Name of the attribute

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.BSDF.id()

        Return a string identifier

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.BSDF.m_components
        :property:

        Flags for each component of this BSDF.

    .. py:method:: mitsuba.BSDF.m_flags
        :property:

        Combined flags for all components of this BSDF.

    .. py:method:: mitsuba.BSDF.needs_differentials()

        Does the implementation require access to texture-space differentials?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.BSDF.pdf(self, ctx, si, wo, active=True)

        Compute the probability per unit solid angle of sampling a given
        direction

        This method provides access to the probability density that would
        result when supplying the same BSDF context and surface interaction
        data structures to the sample() method. It correctly handles changes
        in probability when only a subset of the components is chosen for
        sampling (this can be done using the BSDFContext::component and
        BSDFContext::type_mask fields).

        Note that the incident direction does not need to be explicitly
        specified. It is obtained from the field ``si.wi``.

        Parameter ``ctx`` (:py:obj:`mitsuba.BSDFContext`):
            A context data structure describing which lobes to evaluate, and
            whether radiance or importance are being transported.

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            A surface interaction data structure describing the underlying
            surface position. The incident direction is obtained from the
            field ``si.wi``.

        Parameter ``wo`` (:py:obj:`mitsuba.Vector3f`):
            The outgoing direction

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.BSDF.sample(self, ctx, si, sample1, sample2, active=True)

        Importance sample the BSDF model

        The function returns a sample data structure along with the importance
        weight, which is the value of the BSDF divided by the probability
        density, and multiplied by the cosine foreshortening factor (if needed
        --- it is omitted for degenerate BSDFs like smooth
        mirrors/dielectrics).

        If the supplied context data structures selects subset of components
        in a multi-lobe BRDF model, the sampling is restricted to this subset.
        Depending on the provided transport type, either the BSDF or its
        adjoint version is sampled.

        When sampling a continuous/non-delta component, this method also
        multiplies by the cosine foreshortening factor with respect to the
        sampled direction.

        Parameter ``ctx`` (:py:obj:`mitsuba.BSDFContext`):
            A context data structure describing which lobes to sample, and
            whether radiance or importance are being transported.

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            A surface interaction data structure describing the underlying
            surface position. The incident direction is obtained from the
            field ``si.wi``.

        Parameter ``sample1`` (drjit.llvm.ad.Float):
            A uniformly distributed sample on :math:`[0,1]`. It is used to
            select the BSDF lobe in multi-lobe models.

        Parameter ``sample2`` (:py:obj:`mitsuba.Point2f`):
            A uniformly distributed sample on :math:`[0,1]^2`. It is used to
            generate the sampled direction.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.BSDFSample3f`, :py:obj:`mitsuba.Color3f`]:
            A pair (bs, value) consisting of

        bs: Sampling record, indicating the sampled direction, PDF values and
        other information. The contents are undefined if sampling failed.

        value: The BSDF value divided by the probability (multiplied by the
        cosine foreshortening factor when a non-delta component is sampled). A
        zero spectrum indicates that sampling failed.

.. py:class:: mitsuba.BSDFContext

    Context data structure for BSDF evaluation and sampling

    BSDF models in Mitsuba can be queried and sampled using a variety of
    different modes -- for instance, a rendering algorithm can indicate
    whether radiance or importance is being transported, and it can also
    restrict evaluation and sampling to a subset of lobes in a a multi-
    lobe BSDF model.

    The BSDFContext data structure encodes these preferences and is
    supplied to most BSDF methods.

    .. py:method:: __init__(self, mode=TransportMode.Radiance)

        Overloaded function.
        
        1. ``__init__(self, mode: :py:obj:`mitsuba.TransportMode` = TransportMode.Radiance) -> None``
        
        //! @}
        
        2. ``__init__(self, mode: :py:obj:`mitsuba.TransportMode`, type_mask: int, component: int) -> None``
        
        
        3. ``__init__(self, mode: :py:obj:`mitsuba.TransportMode`, type_mask: int, component: int | None = None) -> None``

        Parameter ``mode`` (:py:obj:`mitsuba.TransportMode`):
            *no description available*

        
    .. py:method:: mitsuba.BSDFContext.component
        :property:

        Integer value of requested BSDF component index to be
        sampled/evaluated.

    .. py:method:: mitsuba.BSDFContext.is_enabled(self, type, component=0)

        Checks whether a given BSDF component type and BSDF component index
        are enabled in this context.

        Parameter ``type`` (:py:obj:`mitsuba.BSDFFlags`):
            *no description available*

        Parameter ``component`` (int):
            *no description available*

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.BSDFContext.mode
        :property:

        Transported mode (radiance or importance)

    .. py:method:: mitsuba.BSDFContext.reverse()

        Reverse the direction of light transport in the record

        This updates the transport mode (radiance to importance and vice
        versa).

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.BSDFContext.type_mask
        :property:

        (self) -> int

.. py:class:: mitsuba.BSDFFlags

    This list of flags is used to classify the different types of lobes
    that are implemented in a BSDF instance.

    They are also useful for picking out individual components, e.g., by
    setting combinations in BSDFContext::type_mask.

    Valid values are as follows:

    .. py:data:: Empty

        No flags set (default value)

    .. py:data:: Null

        'null' scattering event, i.e. particles do not undergo deflection

    .. py:data:: DiffuseReflection

        Ideally diffuse reflection

    .. py:data:: DiffuseTransmission

        Ideally diffuse transmission

    .. py:data:: GlossyReflection

        Glossy reflection

    .. py:data:: GlossyTransmission

        Glossy transmission

    .. py:data:: DeltaReflection

        Reflection into a discrete set of directions

    .. py:data:: DeltaTransmission

        Transmission into a discrete set of directions

    .. py:data:: Anisotropic

        The lobe is not invariant to rotation around the normal

    .. py:data:: SpatiallyVarying

        The BSDF depends on the UV coordinates

    .. py:data:: NonSymmetric

        Flags non-symmetry (e.g. transmission in dielectric materials)

    .. py:data:: FrontSide

        Supports interactions on the front-facing side

    .. py:data:: BackSide

        Supports interactions on the back-facing side

    .. py:data:: Reflection

        Any reflection component (scattering into discrete, 1D, or 2D set of directions)

    .. py:data:: Transmission

        Any transmission component (scattering into discrete, 1D, or 2D set of directions)

    .. py:data:: Diffuse

        Diffuse scattering into a 2D set of directions

    .. py:data:: Glossy

        Non-diffuse scattering into a 2D set of directions

    .. py:data:: Smooth

        Scattering into a 2D set of directions

    .. py:data:: Delta

        Scattering into a discrete set of directions

    .. py:data:: Delta1D

        Scattering into a 1D space of directions

    .. py:data:: All

        Any kind of scattering

.. py:class:: mitsuba.BSDFPtr

    .. py:method:: mitsuba.BSDFPtr.eval(self, ctx, si, wo, active=True)

        Evaluate the BSDF f(wi, wo) or its adjoint version f^{*}(wi, wo) and
        multiply by the cosine foreshortening term.

        Based on the information in the supplied query context ``ctx``, this
        method will either evaluate the entire BSDF or query individual
        components (e.g. the diffuse lobe). Only smooth (i.e. non Dirac-delta)
        components are supported: calling ``eval()`` on a perfectly specular
        material will return zero.

        Note that the incident direction does not need to be explicitly
        specified. It is obtained from the field ``si.wi``.

        Parameter ``ctx`` (:py:obj:`mitsuba.BSDFContext`):
            A context data structure describing which lobes to evaluate, and
            whether radiance or importance are being transported.

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            A surface interaction data structure describing the underlying
            surface position. The incident direction is obtained from the
            field ``si.wi``.

        Parameter ``wo`` (:py:obj:`mitsuba.Vector3f`):
            The outgoing direction

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Color3f`:
            *no description available*

    .. py:method:: mitsuba.BSDFPtr.eval_attribute(self, name, si, active=True)

        Evaluate a specific BSDF attribute at the given surface interaction.

        BSDF attributes are user-provided fields that provide extra
        information at an intersection. An example of this would be a per-
        vertex or per-face color on a triangle mesh.

        Parameter ``name`` (str):
            Name of the attribute to evaluate

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            Surface interaction associated with the query

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Color3f`:
            An unpolarized spectral power distribution or reflectance value

    .. py:method:: mitsuba.BSDFPtr.eval_attribute_1(self, name, si, active=True)

        Monochromatic evaluation of a BSDF attribute at the given surface
        interaction

        This function differs from eval_attribute() in that it provided raw
        access to scalar intensity/reflectance values without any color
        processing (e.g. spectral upsampling).

        Parameter ``name`` (str):
            Name of the attribute to evaluate

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            Surface interaction associated with the query

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            An scalar intensity or reflectance value

    .. py:method:: mitsuba.BSDFPtr.eval_attribute_3(self, name, si, active=True)

        Trichromatic evaluation of a BSDF attribute at the given surface
        interaction

        This function differs from eval_attribute() in that it provided raw
        access to RGB intensity/reflectance values without any additional
        color processing (e.g. RGB-to-spectral upsampling).

        Parameter ``name`` (str):
            Name of the attribute to evaluate

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            Surface interaction associated with the query

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Color3f`:
            An trichromatic intensity or reflectance value

    .. py:method:: mitsuba.BSDFPtr.eval_diffuse_reflectance(self, si, active=True)

        Evaluate the diffuse reflectance

        This method approximates the total diffuse reflectance for a given
        direction. For some materials, an exact value can be computed
        inexpensively. When this is not possible, the value is approximated by
        evaluating the BSDF for a normal outgoing direction and returning this
        value multiplied by pi. This is the default behaviour of this method.

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            A surface interaction data structure describing the underlying
            surface position.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Color3f`:
            *no description available*

    .. py:method:: mitsuba.BSDFPtr.eval_null_transmission(self, si, active=True)

        Evaluate un-scattered transmission component of the BSDF

        This method will evaluate the un-scattered transmission
        (BSDFFlags::Null) of the BSDF for light arriving from direction ``w``.
        The default implementation returns zero.

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            A surface interaction data structure describing the underlying
            surface position. The incident direction is obtained from the
            field ``si.wi``.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Color3f`:
            *no description available*

    .. py:method:: mitsuba.BSDFPtr.eval_pdf(self, ctx, si, wo, active=True)

        Jointly evaluate the BSDF f(wi, wo) and the probability per unit solid
        angle of sampling the given direction. The result from the evaluated
        BSDF is multiplied by the cosine foreshortening term.

        Based on the information in the supplied query context ``ctx``, this
        method will either evaluate the entire BSDF or query individual
        components (e.g. the diffuse lobe). Only smooth (i.e. non Dirac-delta)
        components are supported: calling ``eval()`` on a perfectly specular
        material will return zero.

        This method provides access to the probability density that would
        result when supplying the same BSDF context and surface interaction
        data structures to the sample() method. It correctly handles changes
        in probability when only a subset of the components is chosen for
        sampling (this can be done using the BSDFContext::component and
        BSDFContext::type_mask fields).

        Note that the incident direction does not need to be explicitly
        specified. It is obtained from the field ``si.wi``.

        Parameter ``ctx`` (:py:obj:`mitsuba.BSDFContext`):
            A context data structure describing which lobes to evaluate, and
            whether radiance or importance are being transported.

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            A surface interaction data structure describing the underlying
            surface position. The incident direction is obtained from the
            field ``si.wi``.

        Parameter ``wo`` (:py:obj:`mitsuba.Vector3f`):
            The outgoing direction

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Color3f`, drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.BSDFPtr.eval_pdf_sample(self, ctx, si, wo, sample1, sample2, active=True)

        Jointly evaluate the BSDF f(wi, wo) and the probability per unit solid
        angle of sampling the given direction. The result from the evaluated
        BSDF is multiplied by the cosine foreshortening term.

        Based on the information in the supplied query context ``ctx``, this
        method will either evaluate the entire BSDF or query individual
        components (e.g. the diffuse lobe). Only smooth (i.e. non Dirac-delta)
        components are supported: calling ``eval()`` on a perfectly specular
        material will return zero.

        This method provides access to the probability density that would
        result when supplying the same BSDF context and surface interaction
        data structures to the sample() method. It correctly handles changes
        in probability when only a subset of the components is chosen for
        sampling (this can be done using the BSDFContext::component and
        BSDFContext::type_mask fields).

        Note that the incident direction does not need to be explicitly
        specified. It is obtained from the field ``si.wi``.

        Parameter ``ctx`` (:py:obj:`mitsuba.BSDFContext`):
            A context data structure describing which lobes to evaluate, and
            whether radiance or importance are being transported.

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            A surface interaction data structure describing the underlying
            surface position. The incident direction is obtained from the
            field ``si.wi``.

        Parameter ``wo`` (:py:obj:`mitsuba.Vector3f`):
            The outgoing direction

        Parameter ``sample1`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``sample2`` (:py:obj:`mitsuba.Point2f`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Color3f`, drjit.llvm.ad.Float, :py:obj:`mitsuba.BSDFSample3f`, :py:obj:`mitsuba.Color3f`]:
            *no description available*

    .. py:method:: mitsuba.BSDFPtr.flags()

        Flags for all components combined.

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.BSDFPtr.has_attribute(self, name, active=True)

        Returns whether this BSDF contains the specified attribute.

        Parameter ``name`` (str):
            Name of the attribute

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.BSDFPtr.needs_differentials()

        Does the implementation require access to texture-space differentials?

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.BSDFPtr.pdf(self, ctx, si, wo, active=True)

        Compute the probability per unit solid angle of sampling a given
        direction

        This method provides access to the probability density that would
        result when supplying the same BSDF context and surface interaction
        data structures to the sample() method. It correctly handles changes
        in probability when only a subset of the components is chosen for
        sampling (this can be done using the BSDFContext::component and
        BSDFContext::type_mask fields).

        Note that the incident direction does not need to be explicitly
        specified. It is obtained from the field ``si.wi``.

        Parameter ``ctx`` (:py:obj:`mitsuba.BSDFContext`):
            A context data structure describing which lobes to evaluate, and
            whether radiance or importance are being transported.

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            A surface interaction data structure describing the underlying
            surface position. The incident direction is obtained from the
            field ``si.wi``.

        Parameter ``wo`` (:py:obj:`mitsuba.Vector3f`):
            The outgoing direction

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.BSDFPtr.sample(self, ctx, si, sample1, sample2, active=True)

        Importance sample the BSDF model

        The function returns a sample data structure along with the importance
        weight, which is the value of the BSDF divided by the probability
        density, and multiplied by the cosine foreshortening factor (if needed
        --- it is omitted for degenerate BSDFs like smooth
        mirrors/dielectrics).

        If the supplied context data structures selects subset of components
        in a multi-lobe BRDF model, the sampling is restricted to this subset.
        Depending on the provided transport type, either the BSDF or its
        adjoint version is sampled.

        When sampling a continuous/non-delta component, this method also
        multiplies by the cosine foreshortening factor with respect to the
        sampled direction.

        Parameter ``ctx`` (:py:obj:`mitsuba.BSDFContext`):
            A context data structure describing which lobes to sample, and
            whether radiance or importance are being transported.

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            A surface interaction data structure describing the underlying
            surface position. The incident direction is obtained from the
            field ``si.wi``.

        Parameter ``sample1`` (drjit.llvm.ad.Float):
            A uniformly distributed sample on :math:`[0,1]`. It is used to
            select the BSDF lobe in multi-lobe models.

        Parameter ``sample2`` (:py:obj:`mitsuba.Point2f`):
            A uniformly distributed sample on :math:`[0,1]^2`. It is used to
            generate the sampled direction.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.BSDFSample3f`, :py:obj:`mitsuba.Color3f`]:
            A pair (bs, value) consisting of

        bs: Sampling record, indicating the sampled direction, PDF values and
        other information. The contents are undefined if sampling failed.

        value: The BSDF value divided by the probability (multiplied by the
        cosine foreshortening factor when a non-delta component is sampled). A
        zero spectrum indicates that sampling failed.

.. py:class:: mitsuba.BSDFSample3f

    Data structure holding the result of BSDF sampling operations.

    .. py:method:: __init__()

        Overloaded function.
        
        1. ``__init__(self) -> None``
        
        
        2. ``__init__(self, wo: :py:obj:`mitsuba.Vector3f`) -> None``
        
        Given a surface interaction and an incident/exitant direction pair
        (wi, wo), create a query record to evaluate the BSDF or its sampling
        density.
        
        By default, all components will be sampled regardless of what measure
        they live on.
        
        Parameter ``wo``:
            An outgoing direction in local coordinates. This should be a
            normalized direction vector that points *away* from the scattering
            event.
        
        3. ``__init__(self, bs: :py:obj:`mitsuba.BSDFSample3f`) -> None``
        
        Copy constructor

        
    .. py:method:: mitsuba.BSDFSample3f.assign(self, arg)

        Parameter ``arg`` (:py:obj:`mitsuba.BSDFSample3f`, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.BSDFSample3f.eta
        :property:

        Relative index of refraction in the sampled direction

    .. py:method:: mitsuba.BSDFSample3f.pdf
        :property:

        Probability density at the sample

    .. py:method:: mitsuba.BSDFSample3f.sampled_component
        :property:

        Stores the component index that was sampled by BSDF::sample()

    .. py:method:: mitsuba.BSDFSample3f.sampled_type
        :property:

        Stores the component type that was sampled by BSDF::sample()

    .. py:method:: mitsuba.BSDFSample3f.wo
        :property:

        Normalized outgoing direction in local coordinates

.. py:class:: mitsuba.Bitmap

    Base class: :py:obj:`mitsuba.Object`

    General-purpose bitmap class with read and write support for several
    common file formats.

    This class handles loading of PNG, JPEG, BMP, TGA, as well as OpenEXR
    files, and it supports writing of PNG, JPEG and OpenEXR files.

    PNG and OpenEXR files are optionally annotated with string-valued
    metadata, and the gamma setting can be stored as well. Please see the
    class methods and enumerations for further detail.

    .. py:method:: __init__(self, pixel_format, component_format, size, channel_count=0, channel_names=[])

        Overloaded function.
        
        1. ``__init__(self, pixel_format: :py:obj:`mitsuba.Bitmap.PixelFormat`, component_format: :py:obj:`mitsuba.Struct.Type`, size: :py:obj:`mitsuba.ScalarVector2u`, channel_count: int = 0, channel_names: collections.abc.Sequence[str] = []) -> None``
        
        Create a bitmap of the specified type and allocate the necessary
        amount of memory
        
        Parameter ``pixel_format`` (:py:obj:`mitsuba.Bitmap.PixelFormat`):
            Specifies the pixel format (e.g. RGBA or Luminance-only)
        
        Parameter ``component_format`` (:py:obj:`mitsuba.Struct.Type`):
            Specifies how the per-pixel components are encoded (e.g. unsigned
            8 bit integers or 32-bit floating point values). The component
            format struct_type_v<Float> will be translated to the
            corresponding compile-time precision type (Float32 or Float64).
        
        Parameter ``size`` (:py:obj:`mitsuba.ScalarVector2u`):
            Specifies the horizontal and vertical bitmap size in pixels
        
        Parameter ``channel_count`` (int):
            Channel count of the image. This parameter is only required when
            ``pixel_format`` = PixelFormat::MultiChannel
        
        Parameter ``channel_names`` (collections.abc.Sequence[str]):
            Channel names of the image. This parameter is optional, and only
            used when ``pixel_format`` = PixelFormat::MultiChannel
        
        Parameter ``data``:
            External pointer to the image data. If set to ``nullptr``, the
            implementation will allocate memory itself.
        
        2. ``__init__(self, arg: :py:obj:`mitsuba.Bitmap`) -> None``
        
        
        3. ``__init__(self, path: :py:obj:`mitsuba.filesystem.path`, format: :py:obj:`mitsuba.Bitmap.FileFormat` = FileFormat.Auto) -> None``
        
        
        4. ``__init__(self, stream: :py:obj:`mitsuba.Stream`, format: :py:obj:`mitsuba.Bitmap.FileFormat` = FileFormat.Auto) -> None``
        
        
        5. ``__init__(self, array: ndarray[order='C', device='cpu'], pixel_format: object | None = None, channel_names: collections.abc.Sequence[str] = []) -> None``
        
        Initialize a Bitmap from any array that implements the buffer or DLPack protocol.
        
        6. ``__init__(self, array: drjit.ArrayBase, pixel_format: object | None = None, channel_names: collections.abc.Sequence[str] = []) -> None``
        
        Initialize a Bitmap from any array that implements the buffer or DLPack protocol.

        
    .. py:class:: mitsuba.Bitmap.AlphaTransform

        Type of alpha transformation

        Valid values are as follows:

        .. py:data:: Empty

            No transformation (default)

        .. py:data:: Premultiply

            No transformation (default)

        .. py:data:: Unpremultiply

            No transformation (default)

    .. py:class:: mitsuba.Bitmap.FileFormat

        Supported image file formats

        Valid values are as follows:

        .. py:data:: PNG

            Portable network graphics  The following is supported:  * Loading and saving of 8/16-bit per component bitmaps for all pixel formats (Y, YA, RGB, RGBA)  * Loading and saving of 1-bit per component mask bitmaps  * Loading and saving of string-valued metadata fields

        .. py:data:: OpenEXR

            OpenEXR high dynamic range file format developed by Industrial Light & Magic (ILM)  The following is supported:  * Loading and saving of Float16 / Float32/ UInt32 bitmaps with all supported RGB/Luminance/Alpha combinations  * Loading and saving of spectral bitmaps  * Loading and saving of XYZ tristimulus bitmaps  * Loading and saving of string-valued metadata fields  The following is *not* supported:  * Saving of tiled images, tile-based read access  * Display windows that are different than the data window  * Loading of spectrum-valued bitmaps

        .. py:data:: RGBE

            RGBE image format by Greg Ward  The following is supported  * Loading and saving of Float32 - based RGB bitmaps

        .. py:data:: PFM

            PFM (Portable Float Map) image format  The following is supported  * Loading and saving of Float32 - based Luminance or RGB bitmaps

        .. py:data:: PPM

            PPM (Portable Pixel Map) image format  The following is supported  * Loading and saving of UInt8 and UInt16 - based RGB bitmaps

        .. py:data:: JPEG

            Joint Photographic Experts Group file format  The following is supported:  * Loading and saving of 8 bit per component RGB and luminance bitmaps

        .. py:data:: TGA

            Truevision Advanced Raster Graphics Array file format  The following is supported:  * Loading of uncompressed 8-bit RGB/RGBA files

        .. py:data:: BMP

            Windows Bitmap file format  The following is supported:  * Loading of uncompressed 8-bit luminance and RGBA bitmaps

        .. py:data:: Unknown

            Unknown file format

        .. py:data:: Auto

            Automatically detect the file format  Note: this flag only applies when loading a file. In this case, the source stream must support the ``seek()`` operation.

    .. py:class:: mitsuba.Bitmap.PixelFormat

        This enumeration lists all pixel format types supported by the Bitmap
        class. This both determines the number of channels, and how they
        should be interpreted

        Valid values are as follows:

        .. py:data:: Y

            Single-channel luminance bitmap

        .. py:data:: YA

            Two-channel luminance + alpha bitmap

        .. py:data:: RGB

            RGB bitmap

        .. py:data:: RGBA

            RGB bitmap + alpha channel

        .. py:data:: RGBAW

            RGB bitmap + alpha channel + weight (used by ImageBlock)

        .. py:data:: XYZ

            XYZ tristimulus bitmap

        .. py:data:: XYZA

            XYZ tristimulus + alpha channel

        .. py:data:: MultiChannel

            Arbitrary multi-channel bitmap without a fixed interpretation

    .. py:method:: mitsuba.Bitmap.accumulate(self, bitmap, source_offset, target_offset, size)

        Overloaded function.

        1. ``accumulate(self, bitmap: :py:obj:`mitsuba.Bitmap`, source_offset: :py:obj:`mitsuba.ScalarPoint2i`, target_offset: :py:obj:`mitsuba.ScalarPoint2i`, size: :py:obj:`mitsuba.ScalarVector2i`) -> None``

        Accumulate the contents of another bitmap into the region with the
        specified offset

        Out-of-bounds regions are safely ignored. It is assumed that ``bitmap
        != this``.

        Remark:
            This function throws an exception when the bitmaps use different
            component formats or channels.

        2. ``accumulate(self, bitmap: :py:obj:`mitsuba.Bitmap`, target_offset: :py:obj:`mitsuba.ScalarPoint2i`) -> None``

        Accumulate the contents of another bitmap into the region with the
        specified offset

        This convenience function calls the main ``accumulate()``
        implementation with ``size`` set to ``bitmap->size()`` and
        ``source_offset`` set to zero. Out-of-bounds regions are ignored. It
        is assumed that ``bitmap != this``.

        Remark:
            This function throws an exception when the bitmaps use different
            component formats or channels.

        3. ``accumulate(self, bitmap: :py:obj:`mitsuba.Bitmap`) -> None``

        Accumulate the contents of another bitmap into the region with the
        specified offset

        This convenience function calls the main ``accumulate()``
        implementation with ``size`` set to ``bitmap->size()`` and
        ``source_offset`` and ``target_offset`` set to zero. Out-of-bounds
        regions are ignored. It is assumed that ``bitmap != this``.

        Remark:
            This function throws an exception when the bitmaps use different
            component formats or channels.

        Parameter ``bitmap`` (:py:obj:`mitsuba.Bitmap`):
            *no description available*

        Parameter ``source_offset`` (:py:obj:`mitsuba.ScalarPoint2i`):
            *no description available*

        Parameter ``target_offset`` (:py:obj:`mitsuba.ScalarPoint2i`):
            *no description available*

        Parameter ``size`` (:py:obj:`mitsuba.ScalarVector2i`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Bitmap.buffer_size()

        Return the bitmap size in bytes (excluding metadata)

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Bitmap.bytes_per_pixel()

        Return the number bytes of storage used per pixel

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Bitmap.channel_count()

        Return the number of channels used by this bitmap

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Bitmap.clear()

        Clear the bitmap to zero

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Bitmap.component_format()

        Return the component format of this bitmap

        Returns → :py:obj:`mitsuba.Struct.Type`:
            *no description available*

    .. py:method:: mitsuba.Bitmap.convert(self, pixel_format=None, component_format=None, srgb_gamma=None, alpha_transform=AlphaTransform.Empty)

        Overloaded function.

        1. ``convert(self, pixel_format: object | None = None, component_format: object | None = None, srgb_gamma: object | None = None, alpha_transform: :py:obj:`mitsuba.Bitmap.AlphaTransform` = AlphaTransform.Empty) -> :py:obj:`mitsuba.Bitmap```

        Convert the bitmap into another pixel and/or component format

        This helper function can be used to efficiently convert a bitmap
        between different underlying representations. For instance, it can
        translate a uint8 sRGB bitmap to a linear float32 XYZ bitmap based on
        half-, single- or double-precision floating point-backed storage.

        This function roughly does the following:

        * For each pixel and channel, it converts the associated value into a
        normalized linear-space form (any gamma of the source bitmap is
        removed)

        * gamma correction (sRGB ramp) is applied if ``srgb_gamma`` is
        ``True``

        * The corrected value is clamped against the representable range of
        the desired component format.

        * The clamped gamma-corrected value is then written to the new bitmap

        If the pixel formats differ, this function will also perform basic
        conversions (e.g. spectrum to rgb, luminance to uniform spectrum
        values, etc.)

        Note that the alpha channel is assumed to be linear in both the source
        and target bitmap, hence it won't be affected by any gamma-related
        transformations.

        Remark:
            This ``convert()`` variant usually returns a new bitmap instance.
            When the conversion would just involve copying the original
            bitmap, the function becomes a no-op and returns the current
            instance.

        pixel_format Specifies the desired pixel format

        component_format Specifies the desired component format

        srgb_gamma Specifies whether a sRGB gamma ramp should be applied to
        the output values.

        2. ``convert(self, target: :py:obj:`mitsuba.Bitmap`) -> None``

        Parameter ``pixel_format`` (object | None):
            *no description available*

        Parameter ``component_format`` (object | None):
            *no description available*

        Parameter ``srgb_gamma`` (object | None):
            *no description available*

        Parameter ``alpha_transform`` (:py:obj:`mitsuba.Bitmap.AlphaTransform`):
            *no description available*

        Returns → :py:obj:`mitsuba.Bitmap`:
            *no description available*

    .. py:method:: mitsuba.Bitmap.has_alpha()

        Return whether this image has an alpha channel

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Bitmap.height()

        Return the bitmap's height in pixels

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Bitmap.metadata()

        Return a Properties object containing the image metadata

        Returns → :py:obj:`mitsuba.scalar_rgb.Properties`:
            *no description available*

    .. py:method:: mitsuba.Bitmap.pixel_count()

        Return the total number of pixels

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Bitmap.pixel_format()

        Return the pixel format of this bitmap

        Returns → :py:obj:`mitsuba.Bitmap.PixelFormat`:
            *no description available*

    .. py:method:: mitsuba.Bitmap.premultiplied_alpha()

        Return whether the bitmap uses premultiplied alpha

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Bitmap.resample(self, target, rfilter=None, bc=(FilterBoundaryCondition.Clamp, FilterBoundaryCondition.Clamp), clamp=(-inf, inf), temp=None)

        Overloaded function.

        1. ``resample(self, target: :py:obj:`mitsuba.Bitmap`, rfilter: :py:obj:`mitsuba.BitmapReconstructionFilter` | None = None, bc: tuple[:py:obj:`mitsuba.FilterBoundaryCondition`, :py:obj:`mitsuba.FilterBoundaryCondition`] = (FilterBoundaryCondition.Clamp, FilterBoundaryCondition.Clamp), clamp: tuple[float, float] = (-inf, inf), temp: :py:obj:`mitsuba.Bitmap` | None = None) -> None``

        Up- or down-sample this image to a different resolution

        Uses the provided reconstruction filter and accounts for the requested
        horizontal and vertical boundary conditions when looking up data
        outside of the input domain.

        A minimum and maximum image value can be specified to prevent to
        prevent out-of-range values that are created by the resampling
        process.

        The optional ``temp`` parameter can be used to pass an image of
        resolution ``Vector2u(target->width(), this->height())`` to avoid
        intermediate memory allocations.

        Parameter ``target`` (:py:obj:`mitsuba.Bitmap`):
            Pre-allocated bitmap of the desired target resolution

        Parameter ``rfilter`` (:py:obj:`mitsuba.BitmapReconstructionFilter` | None):
            A separable image reconstruction filter (default: 2-lobe Lanczos
            filter)

        Parameter ``bch``:
            Horizontal and vertical boundary conditions (default: clamp)

        Parameter ``clamp`` (tuple[float, float]):
            Filtered image pixels will be clamped to the following range.
            Default: -infinity..infinity (i.e. no clamping is used)

        Parameter ``temp`` (:py:obj:`mitsuba.Bitmap` | None):
            Optional: image for intermediate computations

        2. ``resample(self, res: :py:obj:`mitsuba.ScalarVector2u`, rfilter: :py:obj:`mitsuba.BitmapReconstructionFilter` | None = None, bc: tuple[:py:obj:`mitsuba.FilterBoundaryCondition`, :py:obj:`mitsuba.FilterBoundaryCondition`] = (FilterBoundaryCondition.Clamp, FilterBoundaryCondition.Clamp), clamp: tuple[float, float] = (-inf, inf)) -> :py:obj:`mitsuba.Bitmap```

        Up- or down-sample this image to a different resolution

        This version is similar to the above resample() function -- the main
        difference is that it does not work with preallocated bitmaps and
        takes the desired output resolution as first argument.

        Uses the provided reconstruction filter and accounts for the requested
        horizontal and vertical boundary conditions when looking up data
        outside of the input domain.

        A minimum and maximum image value can be specified to prevent to
        prevent out-of-range values that are created by the resampling
        process.

        Parameter ``res``:
            Desired output resolution

        Parameter ``rfilter`` (:py:obj:`mitsuba.BitmapReconstructionFilter` | None):
            A separable image reconstruction filter (default: 2-lobe Lanczos
            filter)

        Parameter ``bch``:
            Horizontal and vertical boundary conditions (default: clamp)

        Parameter ``clamp`` (tuple[float, float]):
            Filtered image pixels will be clamped to the following range.
            Default: -infinity..infinity (i.e. no clamping is used)

        Parameter ``bc`` (tuple[:py:obj:`mitsuba.FilterBoundaryCondition`, :py:obj:`mitsuba.FilterBoundaryCondition`]):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Bitmap.set_premultiplied_alpha(self, arg)

        Specify whether the bitmap uses premultiplied alpha

        Parameter ``arg`` (bool, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Bitmap.set_srgb_gamma(self, arg)

        Specify whether the bitmap uses an sRGB gamma encoding

        Parameter ``arg`` (bool, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Bitmap.size()

        Return the bitmap dimensions in pixels

        Returns → :py:obj:`mitsuba.ScalarVector2u`:
            *no description available*

    .. py:method:: mitsuba.Bitmap.split()

        Split an multi-channel image buffer (e.g. from an OpenEXR image with
        lots of AOVs) into its constituent layers

        Returns → list[tuple[str, :py:obj:`mitsuba.Bitmap`]]:
            *no description available*

    .. py:method:: mitsuba.Bitmap.srgb_gamma()

        Return whether the bitmap uses an sRGB gamma encoding

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Bitmap.struct_()

        Return a ``Struct`` instance describing the contents of the bitmap
        (const version)

        Returns → :py:obj:`mitsuba.Struct`:
            *no description available*

    .. py:method:: mitsuba.Bitmap.vflip()

        Vertically flip the bitmap

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Bitmap.width()

        Return the bitmap's width in pixels

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Bitmap.write(self, stream, format=FileFormat.Auto, quality=-1)

        Overloaded function.

        1. ``write(self, stream: :py:obj:`mitsuba.Stream`, format: :py:obj:`mitsuba.Bitmap.FileFormat` = FileFormat.Auto, quality: int = -1) -> None``

        Write an encoded form of the bitmap to a stream using the specified
        file format

        Parameter ``stream`` (:py:obj:`mitsuba.Stream`):
            Target stream that will receive the encoded output

        Parameter ``format`` (:py:obj:`mitsuba.Bitmap.FileFormat`):
            Target file format (OpenEXR, PNG, etc.) Detected from the filename
            by default.

        Parameter ``quality`` (int):
            Depending on the file format, this parameter takes on a slightly
            different meaning:

        * PNG images: Controls how much libpng will attempt to compress the
        output (with 1 being the lowest and 9 denoting the highest
        compression). The default argument uses the compression level 5.

        * JPEG images: denotes the desired quality (between 0 and 100). The
        default argument (-1) uses the highest quality (100).

        * OpenEXR images: denotes the quality level of the DWAB compressor,
        with higher values corresponding to a lower quality. A value of 45 is
        recommended as the default for lossy compression. The default argument
        (-1) causes the implementation to switch to the lossless PIZ
        compressor.

        2. ``write(self, path: :py:obj:`mitsuba.filesystem.path`, format: :py:obj:`mitsuba.Bitmap.FileFormat` = FileFormat.Auto, quality: int = -1) -> None``

        Write an encoded form of the bitmap to a file using the specified file
        format

        Parameter ``path``:
            Target file path on disk

        Parameter ``format`` (:py:obj:`mitsuba.Bitmap.FileFormat`):
            Target file format (FileFormat::OpenEXR, FileFormat::PNG, etc.)
            Detected from the filename by default.

        Parameter ``quality`` (int):
            Depending on the file format, this parameter takes on a slightly
            different meaning:

        * PNG images: Controls how much libpng will attempt to compress the
        output (with 1 being the lowest and 9 denoting the highest
        compression). The default argument uses the compression level 5.

        * JPEG images: denotes the desired quality (between 0 and 100). The
        default argument (-1) uses the highest quality (100).

        * OpenEXR images: denotes the quality level of the DWAB compressor,
        with higher values corresponding to a lower quality. A value of 45 is
        recommended as the default for lossy compression. The default argument
        (-1) causes the implementation to switch to the lossless PIZ
        compressor.

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Bitmap.write_async(self, path, format=FileFormat.Auto, quality=-1)

        Equivalent to write(), but executes asynchronously on a different
        thread

        Parameter ``path`` (:py:obj:`mitsuba.filesystem.path`):
            *no description available*

        Parameter ``format`` (:py:obj:`mitsuba.Bitmap.FileFormat`):
            *no description available*

        Parameter ``quality`` (int):
            *no description available*

        Returns → None:
            *no description available*

.. py:class:: mitsuba.BitmapReconstructionFilter

    Base class: :py:obj:`mitsuba.Object`

    Generic interface to separable image reconstruction filters

    When resampling bitmaps or adding samples to a rendering in progress,
    Mitsuba first convolves them with a image reconstruction filter.
    Various kinds are implemented as subclasses of this interface.

    Because image filters are generally too expensive to evaluate for each
    sample, the implementation of this class internally precomputes an
    discrete representation, whose resolution given by
    MI_FILTER_RESOLUTION.

    .. py:method:: mitsuba.BitmapReconstructionFilter.border_size()

        Return the block border size required when rendering with this filter

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.BitmapReconstructionFilter.eval(self, x, active=True)

        Evaluate the filter function

        Parameter ``x`` (float):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.BitmapReconstructionFilter.eval_discretized(self, x, active=True)

        Evaluate a discretized version of the filter (generally faster than
        'eval')

        Parameter ``x`` (float):
            *no description available*

        Parameter ``active`` (bool):
            Mask to specify active lanes.

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.BitmapReconstructionFilter.is_box_filter()

        Check whether this is a box filter?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.BitmapReconstructionFilter.radius()

        Return the filter's width

        Returns → float:
            *no description available*

.. py:class:: mitsuba.Bool

.. py:class:: mitsuba.BoundingBox2f

    Generic n-dimensional bounding box data structure

    Maintains a minimum and maximum position along each dimension and
    provides various convenience functions for querying and modifying
    them.

    This class is parameterized by the underlying point data structure,
    which permits the use of different scalar types and dimensionalities,
    e.g.

    .. code-block:: c

        BoundingBox<Point3i> integer_bbox(Point3i(0, 1, 3), Point3i(4, 5, 6));
        BoundingBox<Point2d> double_bbox(Point2d(0.0, 1.0), Point2d(4.0, 5.0));


    Template parameter ``T``:
        The underlying point data type (e.g. ``Point2d``)

    .. py:method:: __init__()

        Overloaded function.
        
        1. ``__init__(self) -> None``
        
        Create a new invalid bounding box
        
        Initializes the components of the minimum and maximum position to
        :math:`\infty` and :math:`-\infty`, respectively.
        
        2. ``__init__(self, p: :py:obj:`mitsuba.Point2f`) -> None``
        
        Create a collapsed bounding box from a single point
        
        3. ``__init__(self, min: :py:obj:`mitsuba.Point2f`, max: :py:obj:`mitsuba.Point2f`) -> None``
        
        Create a bounding box from two positions
        
        4. ``__init__(self, arg: :py:obj:`mitsuba.BoundingBox2f`) -> None``
        
        Copy constructor

        
    .. py:method:: mitsuba.BoundingBox2f.center()

        Return the center point

        Returns → :py:obj:`mitsuba.Point2f`:
            *no description available*

    .. py:method:: mitsuba.BoundingBox2f.clip(self, arg)

        Clip this bounding box to another bounding box

        Parameter ``arg`` (:py:obj:`mitsuba.BoundingBox2f`, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.BoundingBox2f.collapsed()

        Check whether this bounding box has collapsed to a point, line, or
        plane

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.BoundingBox2f.contains(self, p, strict=False)

        Overloaded function.

        1. ``contains(self, p: :py:obj:`mitsuba.Point2f`, strict: bool = False) -> drjit.llvm.ad.Bool``

        Check whether a point lies *on* or *inside* the bounding box

        Parameter ``p`` (:py:obj:`mitsuba.Point2f`):
            The point to be tested

        Template parameter ``Strict``:
            Set this parameter to ``True`` if the bounding box boundary should
            be excluded in the test

        Remark:
            In the Python bindings, the 'Strict' argument is a normal function
            parameter with default value ``False``.

        2. ``contains(self, bbox: :py:obj:`mitsuba.BoundingBox2f`, strict: bool = False) -> drjit.llvm.ad.Bool``

        Check whether a specified bounding box lies *on* or *within* the
        current bounding box

        Note that by definition, an 'invalid' bounding box (where
        min=:math:`\infty` and max=:math:`-\infty`) does not cover any space.
        Hence, this method will always return *true* when given such an
        argument.

        Template parameter ``Strict``:
            Set this parameter to ``True`` if the bounding box boundary should
            be excluded in the test

        Remark:
            In the Python bindings, the 'Strict' argument is a normal function
            parameter with default value ``False``.

        Parameter ``strict`` (bool):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.BoundingBox2f.corner(self, arg)

        Return the position of a bounding box corner

        Parameter ``arg`` (int, /):
            *no description available*

        Returns → :py:obj:`mitsuba.Point2f`:
            *no description available*

    .. py:method:: mitsuba.BoundingBox2f.distance(self, arg)

        Overloaded function.

        1. ``distance(self, arg: :py:obj:`mitsuba.Point2f`, /) -> drjit.llvm.ad.Float``

        Calculate the shortest distance between the axis-aligned bounding box
        and the point ``p``.

        2. ``distance(self, arg: :py:obj:`mitsuba.BoundingBox2f`, /) -> drjit.llvm.ad.Float``

        Calculate the shortest distance between the axis-aligned bounding box
        and ``bbox``.

        Parameter ``arg`` (:py:obj:`mitsuba.Point2f`, /):
            *no description available*

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.BoundingBox2f.expand(self, arg)

        Overloaded function.

        1. ``expand(self, arg: :py:obj:`mitsuba.Point2f`, /) -> None``

        Expand the bounding box to contain another point

        2. ``expand(self, arg: :py:obj:`mitsuba.BoundingBox2f`, /) -> None``

        Expand the bounding box to contain another bounding box

        Parameter ``arg`` (:py:obj:`mitsuba.Point2f`, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.BoundingBox2f.extents()

        Calculate the bounding box extents

        Returns → :py:obj:`mitsuba.Vector2f`:
            ``max - min``

    .. py:method:: mitsuba.BoundingBox2f.major_axis()

        Return the dimension index with the index associated side length

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.BoundingBox2f.max
        :property:

        (self) -> :py:obj:`mitsuba.Point2f`

    .. py:method:: mitsuba.BoundingBox2f.min
        :property:

        (self) -> :py:obj:`mitsuba.Point2f`

    .. py:method:: mitsuba.BoundingBox2f.minor_axis()

        Return the dimension index with the shortest associated side length

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.BoundingBox2f.overlaps(self, bbox, strict=False)

        Check two axis-aligned bounding boxes for possible overlap.

        Parameter ``Strict``:
            Set this parameter to ``True`` if the bounding box boundary should
            be excluded in the test

        Remark:
            In the Python bindings, the 'Strict' argument is a normal function
            parameter with default value ``False``.

        Parameter ``bbox`` (:py:obj:`mitsuba.BoundingBox2f`):
            *no description available*

        Parameter ``strict`` (bool):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            ``True`` If overlap was detected.

    .. py:method:: mitsuba.BoundingBox2f.reset()

        Mark the bounding box as invalid.

        This operation sets the components of the minimum and maximum position
        to :math:`\infty` and :math:`-\infty`, respectively.

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.BoundingBox2f.squared_distance(self, arg)

        Overloaded function.

        1. ``squared_distance(self, arg: :py:obj:`mitsuba.Point2f`, /) -> drjit.llvm.ad.Float``

        Calculate the shortest squared distance between the axis-aligned
        bounding box and the point ``p``.

        2. ``squared_distance(self, arg: :py:obj:`mitsuba.BoundingBox2f`, /) -> drjit.llvm.ad.Float``

        Calculate the shortest squared distance between the axis-aligned
        bounding box and ``bbox``.

        Parameter ``arg`` (:py:obj:`mitsuba.Point2f`, /):
            *no description available*

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.BoundingBox2f.surface_area()

        Calculate the 2-dimensional surface area of a 3D bounding box

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.BoundingBox2f.valid()

        Check whether this is a valid bounding box

        A bounding box ``bbox`` is considered to be valid when

        .. code-block:: c

            bbox.min[i] <= bbox.max[i]


        holds for each component ``i``.

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.BoundingBox2f.volume()

        Calculate the n-dimensional volume of the bounding box

        Returns → drjit.llvm.ad.Float:
            *no description available*

.. py:class:: mitsuba.BoundingBox3f

    Generic n-dimensional bounding box data structure

    Maintains a minimum and maximum position along each dimension and
    provides various convenience functions for querying and modifying
    them.

    This class is parameterized by the underlying point data structure,
    which permits the use of different scalar types and dimensionalities,
    e.g.

    .. code-block:: c

        BoundingBox<Point3i> integer_bbox(Point3i(0, 1, 3), Point3i(4, 5, 6));
        BoundingBox<Point2d> double_bbox(Point2d(0.0, 1.0), Point2d(4.0, 5.0));


    Template parameter ``T``:
        The underlying point data type (e.g. ``Point2d``)

    .. py:method:: __init__()

        Overloaded function.
        
        1. ``__init__(self) -> None``
        
        Create a new invalid bounding box
        
        Initializes the components of the minimum and maximum position to
        :math:`\infty` and :math:`-\infty`, respectively.
        
        2. ``__init__(self, p: :py:obj:`mitsuba.Point3f`) -> None``
        
        Create a collapsed bounding box from a single point
        
        3. ``__init__(self, min: :py:obj:`mitsuba.Point3f`, max: :py:obj:`mitsuba.Point3f`) -> None``
        
        Create a bounding box from two positions
        
        4. ``__init__(self, arg: :py:obj:`mitsuba.BoundingBox3f`) -> None``
        
        Copy constructor

        
    .. py:method:: mitsuba.BoundingBox3f.bounding_sphere()

        Create a bounding sphere, which contains the axis-aligned box

        Returns → :py:obj:`mitsuba.BoundingSphere3f`:
            *no description available*

    .. py:method:: mitsuba.BoundingBox3f.center()

        Return the center point

        Returns → :py:obj:`mitsuba.Point3f`:
            *no description available*

    .. py:method:: mitsuba.BoundingBox3f.clip(self, arg)

        Clip this bounding box to another bounding box

        Parameter ``arg`` (:py:obj:`mitsuba.BoundingBox3f`, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.BoundingBox3f.collapsed()

        Check whether this bounding box has collapsed to a point, line, or
        plane

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.BoundingBox3f.contains(self, p, strict=False)

        Overloaded function.

        1. ``contains(self, p: :py:obj:`mitsuba.Point3f`, strict: bool = False) -> drjit.llvm.ad.Bool``

        Check whether a point lies *on* or *inside* the bounding box

        Parameter ``p`` (:py:obj:`mitsuba.Point3f`):
            The point to be tested

        Template parameter ``Strict``:
            Set this parameter to ``True`` if the bounding box boundary should
            be excluded in the test

        Remark:
            In the Python bindings, the 'Strict' argument is a normal function
            parameter with default value ``False``.

        2. ``contains(self, bbox: :py:obj:`mitsuba.BoundingBox3f`, strict: bool = False) -> drjit.llvm.ad.Bool``

        Check whether a specified bounding box lies *on* or *within* the
        current bounding box

        Note that by definition, an 'invalid' bounding box (where
        min=:math:`\infty` and max=:math:`-\infty`) does not cover any space.
        Hence, this method will always return *true* when given such an
        argument.

        Template parameter ``Strict``:
            Set this parameter to ``True`` if the bounding box boundary should
            be excluded in the test

        Remark:
            In the Python bindings, the 'Strict' argument is a normal function
            parameter with default value ``False``.

        Parameter ``strict`` (bool):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.BoundingBox3f.corner(self, arg)

        Return the position of a bounding box corner

        Parameter ``arg`` (int, /):
            *no description available*

        Returns → :py:obj:`mitsuba.Point3f`:
            *no description available*

    .. py:method:: mitsuba.BoundingBox3f.distance(self, arg)

        Overloaded function.

        1. ``distance(self, arg: :py:obj:`mitsuba.Point3f`, /) -> drjit.llvm.ad.Float``

        Calculate the shortest distance between the axis-aligned bounding box
        and the point ``p``.

        2. ``distance(self, arg: :py:obj:`mitsuba.BoundingBox3f`, /) -> drjit.llvm.ad.Float``

        Calculate the shortest distance between the axis-aligned bounding box
        and ``bbox``.

        Parameter ``arg`` (:py:obj:`mitsuba.Point3f`, /):
            *no description available*

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.BoundingBox3f.expand(self, arg)

        Overloaded function.

        1. ``expand(self, arg: :py:obj:`mitsuba.Point3f`, /) -> None``

        Expand the bounding box to contain another point

        2. ``expand(self, arg: :py:obj:`mitsuba.BoundingBox3f`, /) -> None``

        Expand the bounding box to contain another bounding box

        Parameter ``arg`` (:py:obj:`mitsuba.Point3f`, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.BoundingBox3f.extents()

        Calculate the bounding box extents

        Returns → :py:obj:`mitsuba.Vector3f`:
            ``max - min``

    .. py:method:: mitsuba.BoundingBox3f.major_axis()

        Return the dimension index with the index associated side length

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.BoundingBox3f.max
        :property:

        (self) -> :py:obj:`mitsuba.Point3f`

    .. py:method:: mitsuba.BoundingBox3f.min
        :property:

        (self) -> :py:obj:`mitsuba.Point3f`

    .. py:method:: mitsuba.BoundingBox3f.minor_axis()

        Return the dimension index with the shortest associated side length

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.BoundingBox3f.overlaps(self, bbox, strict=False)

        Check two axis-aligned bounding boxes for possible overlap.

        Parameter ``Strict``:
            Set this parameter to ``True`` if the bounding box boundary should
            be excluded in the test

        Remark:
            In the Python bindings, the 'Strict' argument is a normal function
            parameter with default value ``False``.

        Parameter ``bbox`` (:py:obj:`mitsuba.BoundingBox3f`):
            *no description available*

        Parameter ``strict`` (bool):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            ``True`` If overlap was detected.

    .. py:method:: mitsuba.BoundingBox3f.ray_intersect(self, ray)

        Check if a ray intersects a bounding box

        Note that this function ignores the ``maxt`` value associated with the
        ray.

        Parameter ``ray`` (:py:obj:`mitsuba.Ray3f`):
            *no description available*

        Returns → tuple[drjit.llvm.ad.Bool, drjit.llvm.ad.Float, drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.BoundingBox3f.reset()

        Mark the bounding box as invalid.

        This operation sets the components of the minimum and maximum position
        to :math:`\infty` and :math:`-\infty`, respectively.

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.BoundingBox3f.squared_distance(self, arg)

        Overloaded function.

        1. ``squared_distance(self, arg: :py:obj:`mitsuba.Point3f`, /) -> drjit.llvm.ad.Float``

        Calculate the shortest squared distance between the axis-aligned
        bounding box and the point ``p``.

        2. ``squared_distance(self, arg: :py:obj:`mitsuba.BoundingBox3f`, /) -> drjit.llvm.ad.Float``

        Calculate the shortest squared distance between the axis-aligned
        bounding box and ``bbox``.

        Parameter ``arg`` (:py:obj:`mitsuba.Point3f`, /):
            *no description available*

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.BoundingBox3f.surface_area()

        Calculate the 2-dimensional surface area of a 3D bounding box

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.BoundingBox3f.valid()

        Check whether this is a valid bounding box

        A bounding box ``bbox`` is considered to be valid when

        .. code-block:: c

            bbox.min[i] <= bbox.max[i]


        holds for each component ``i``.

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.BoundingBox3f.volume()

        Calculate the n-dimensional volume of the bounding box

        Returns → drjit.llvm.ad.Float:
            *no description available*

.. py:class:: mitsuba.BoundingSphere3f

    Generic n-dimensional bounding sphere data structure

    .. py:method:: __init__()

        Overloaded function.
        
        1. ``__init__(self) -> None``
        
        Construct bounding sphere(s) at the origin having radius zero
        
        2. ``__init__(self, arg0: :py:obj:`mitsuba.Point3f`, arg1: drjit.llvm.ad.Float, /) -> None``
        
        Create bounding sphere(s) from given center point(s) with given
        size(s)
        
        3. ``__init__(self, arg: :py:obj:`mitsuba.BoundingSphere3f`) -> None``

        
    .. py:method:: mitsuba.BoundingSphere3f.center
        :property:

        (self) -> :py:obj:`mitsuba.Point3f`

    .. py:method:: mitsuba.BoundingSphere3f.contains(self, p, strict=False)

        Check whether a point lies *on* or *inside* the bounding sphere

        Parameter ``p`` (:py:obj:`mitsuba.Point3f`):
            The point to be tested

        Template parameter ``Strict``:
            Set this parameter to ``True`` if the bounding sphere boundary
            should be excluded in the test

        Remark:
            In the Python bindings, the 'Strict' argument is a normal function
            parameter with default value ``False``.

        Parameter ``strict`` (bool):
            *no description available*

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.BoundingSphere3f.empty()

        Return whether this bounding sphere has a radius of zero or less.

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.BoundingSphere3f.expand(self, arg)

        Expand the bounding sphere radius to contain another point.

        Parameter ``arg`` (:py:obj:`mitsuba.Point3f`, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.BoundingSphere3f.radius
        :property:

        (self) -> drjit.llvm.ad.Float

    .. py:method:: mitsuba.BoundingSphere3f.ray_intersect(self, ray)

        Check if a ray intersects a bounding box

        Parameter ``ray`` (:py:obj:`mitsuba.Ray3f`):
            *no description available*

        Returns → tuple[drjit.llvm.ad.Bool, drjit.llvm.ad.Float, drjit.llvm.ad.Float]:
            *no description available*

.. py:class:: mitsuba.Class

    Stores meta-information about Object instances.

    This class provides a thin layer of RTTI (run-time type information),
    which is useful for doing things like:

    * Checking if an object derives from a certain class

    * Determining the parent of a class at runtime

    * Instantiating a class by name

    * Unserializing a class from a binary data stream

    See also:
        ref, Object

    .. py:method:: mitsuba.Class.alias()

        Return the scene description-specific alias, if applicable

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.Class.name()

        Return the name of the class

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.Class.parent()

        Return the Class object associated with the parent class of nullptr if
        it does not have one.

        Returns → :py:obj:`mitsuba.Class`:
            *no description available*

    .. py:method:: mitsuba.Class.variant()

        Return the variant of the class

        Returns → str:
            *no description available*

.. py:class:: mitsuba.Color0d

.. py:class:: mitsuba.Color0f

.. py:class:: mitsuba.Color1d

.. py:class:: mitsuba.Color1f

.. py:class:: mitsuba.Color3d

.. py:class:: mitsuba.Color3f

.. py:class:: mitsuba.Complex2f

.. py:class:: mitsuba.ContinuousDistribution

    Continuous 1D probability distribution defined in terms of a regularly
    sampled linear interpolant

    This data structure represents a continuous 1D probability
    distribution that is defined as a linear interpolant of a regularly
    discretized signal. The class provides various routines for
    transforming uniformly distributed samples so that they follow the
    stored distribution. Note that unnormalized probability density
    functions (PDFs) will automatically be normalized during
    initialization. The associated scale factor can be retrieved using the
    function normalization().

    .. py:method:: __init__()

        Overloaded function.
        
        1. ``__init__(self) -> None``
        
        Continuous 1D probability distribution defined in terms of a regularly
        sampled linear interpolant
        
        This data structure represents a continuous 1D probability
        distribution that is defined as a linear interpolant of a regularly
        discretized signal. The class provides various routines for
        transforming uniformly distributed samples so that they follow the
        stored distribution. Note that unnormalized probability density
        functions (PDFs) will automatically be normalized during
        initialization. The associated scale factor can be retrieved using the
        function normalization().
        
        2. ``__init__(self, arg: :py:obj:`mitsuba.ContinuousDistribution`) -> None``
        
        Copy constructor
        
        3. ``__init__(self, range: :py:obj:`mitsuba.ScalarVector2f`, pdf: drjit.llvm.ad.Float) -> None``
        
        Initialize from a given density function on the interval ``range``

        
    .. py:method:: mitsuba.ContinuousDistribution.cdf
        :property:

        Return the unnormalized discrete cumulative distribution function over
        intervals

    .. py:method:: mitsuba.ContinuousDistribution.empty()

        Is the distribution object empty/uninitialized?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.ContinuousDistribution.eval_cdf(self, x, active=True)

        Evaluate the unnormalized cumulative distribution function (CDF) at
        position ``p``

        Parameter ``x`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.ContinuousDistribution.eval_cdf_normalized(self, x, active=True)

        Evaluate the unnormalized cumulative distribution function (CDF) at
        position ``p``

        Parameter ``x`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.ContinuousDistribution.eval_pdf(self, x, active=True)

        Evaluate the unnormalized probability mass function (PDF) at position
        ``x``

        Parameter ``x`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.ContinuousDistribution.eval_pdf_normalized(self, x, active=True)

        Evaluate the normalized probability mass function (PDF) at position
        ``x``

        Parameter ``x`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.ContinuousDistribution.integral()

        Return the original integral of PDF entries before normalization

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.ContinuousDistribution.interval_resolution()

        Return the minimum resolution of the discretization

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.ContinuousDistribution.max()

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.ContinuousDistribution.normalization()

        Return the normalization factor (i.e. the inverse of sum())

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.ContinuousDistribution.pdf
        :property:

        Return the unnormalized discretized probability density function

    .. py:method:: mitsuba.ContinuousDistribution.range
        :property:

        Return the range of the distribution

    .. py:method:: mitsuba.ContinuousDistribution.sample(self, value, active=True)

        %Transform a uniformly distributed sample to the stored distribution

        Parameter ``sample``:
            A uniformly distributed sample on the interval [0, 1].

        Parameter ``value`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            The sampled position.

    .. py:method:: mitsuba.ContinuousDistribution.sample_pdf(self, value, active=True)

        %Transform a uniformly distributed sample to the stored distribution

        Parameter ``sample``:
            A uniformly distributed sample on the interval [0, 1].

        Parameter ``value`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[drjit.llvm.ad.Float, drjit.llvm.ad.Float]:
            A tuple consisting of

        1. the sampled position. 2. the normalized probability density of the
        sample.

    .. py:method:: mitsuba.ContinuousDistribution.size()

        Return the number of discretizations

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.ContinuousDistribution.update()

        Update the internal state. Must be invoked when changing the pdf.

        Returns → None:
            *no description available*

.. py:class:: mitsuba.CppADIntegrator

    Base class: :py:obj:`mitsuba.SamplingIntegrator`

.. py:data:: mitsuba.DEBUG
    :type: bool
    :value: False

.. py:class:: mitsuba.DefaultFormatter

    Base class: :py:obj:`mitsuba.Formatter`

    The default formatter used to turn log messages into a human-readable
    form

    .. py:method:: __init__()


    .. py:method:: mitsuba.DefaultFormatter.has_class()

        See also:
            set_has_class

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.DefaultFormatter.has_date()

        See also:
            set_has_date

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.DefaultFormatter.has_log_level()

        See also:
            set_has_log_level

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.DefaultFormatter.has_thread()

        See also:
            set_has_thread

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.DefaultFormatter.set_has_class(self, arg)

        Should class information be included? The default is yes.

        Parameter ``arg`` (bool, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.DefaultFormatter.set_has_date(self, arg)

        Should date information be included? The default is yes.

        Parameter ``arg`` (bool, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.DefaultFormatter.set_has_log_level(self, arg)

        Should log level information be included? The default is yes.

        Parameter ``arg`` (bool, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.DefaultFormatter.set_has_thread(self, arg)

        Should thread information be included? The default is yes.

        Parameter ``arg`` (bool, /):
            *no description available*

        Returns → None:
            *no description available*

.. py:class:: mitsuba.DirectionSample3f

    Base class: :py:obj:`mitsuba.PositionSample3f`

    Record for solid-angle based area sampling techniques

    This data structure is used in techniques that sample positions
    relative to a fixed reference position in the scene. For instance,
    *direct illumination strategies* importance sample the incident
    radiance received by a given surface location. Mitsuba uses this
    approach in a wider bidirectional sense: sampling the incident
    importance due to a sensor also uses the same data structures and
    strategies, which are referred to as *direct sampling*.

    This record inherits all fields from PositionSample and extends it
    with two useful quantities that are cached so that they don't need to
    be recomputed: the unit direction and distance from the reference
    position to the sampled point.

    .. py:method:: __init__()

        Overloaded function.
        
        1. ``__init__(self) -> None``
        
        Construct an uninitialized direct sample
        
        2. ``__init__(self, other: :py:obj:`mitsuba.PositionSample3f`) -> None``
        
        Construct from a position sample
        
        3. ``__init__(self, other: :py:obj:`mitsuba.DirectionSample3f`) -> None``
        
        Copy constructor
        
        4. ``__init__(self, p: :py:obj:`mitsuba.Point3f`, n: :py:obj:`mitsuba.Normal3f`, uv: :py:obj:`mitsuba.Point2f`, time: drjit.llvm.ad.Float, pdf: drjit.llvm.ad.Float, delta: drjit.llvm.ad.Bool, d: :py:obj:`mitsuba.Vector3f`, dist: drjit.llvm.ad.Float, emitter: :py:obj:`mitsuba.EmitterPtr`) -> None``
        
        Element-by-element constructor
        
        5. ``__init__(self, scene: :py:obj:`mitsuba.Scene` | None, si: :py:obj:`mitsuba.SurfaceInteraction3f`, ref: :py:obj:`mitsuba.Interaction3f`) -> None``
        
        Create a position sampling record from a surface intersection
        
        This is useful to determine the hypothetical sampling density on a
        surface after hitting it using standard ray tracing. This happens for
        instance in path tracing with multiple importance sampling.

        
    .. py:method:: mitsuba.DirectionSample3f.assign(self, arg)

        Parameter ``arg`` (:py:obj:`mitsuba.DirectionSample3f`, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.DirectionSample3f.d
        :property:

        Unit direction from the reference point to the target shape

    .. py:method:: mitsuba.DirectionSample3f.dist
        :property:

        Distance from the reference point to the target shape

    .. py:method:: mitsuba.DirectionSample3f.emitter
        :property:

        Optional: pointer to an associated object

        In some uses of this record, sampling a position also involves
        choosing one of several objects (shapes, emitters, ..) on which the
        position lies. In that case, the ``object`` attribute stores a pointer
        to this object.

.. py:class:: mitsuba.DiscontinuityFlags

    This list of flags is used to control the behavior of discontinuity
    related routines.

    Valid values are as follows:

    .. py:data:: Empty

        No flags set (default value)

    .. py:data:: PerimeterType

        Open boundary or jumping normal type of discontinuity

    .. py:data:: InteriorType

        Smooth normal type of discontinuity

    .. py:data:: DirectionLune

        //! Encoding and projection flags

    .. py:data:: DirectionSphere

        //! Encoding and projection flags

    .. py:data:: HeuristicWalk

        //! Encoding and projection flags

    .. py:data:: AllTypes

        All types of discontinuities

.. py:class:: mitsuba.DiscreteDistribution

    Discrete 1D probability distribution

    This data structure represents a discrete 1D probability distribution
    and provides various routines for transforming uniformly distributed
    samples so that they follow the stored distribution. Note that
    unnormalized probability mass functions (PMFs) will automatically be
    normalized during initialization. The associated scale factor can be
    retrieved using the function normalization().

    .. py:method:: __init__()

        Overloaded function.
        
        1. ``__init__(self) -> None``
        
        Discrete 1D probability distribution
        
        This data structure represents a discrete 1D probability distribution
        and provides various routines for transforming uniformly distributed
        samples so that they follow the stored distribution. Note that
        unnormalized probability mass functions (PMFs) will automatically be
        normalized during initialization. The associated scale factor can be
        retrieved using the function normalization().
        
        2. ``__init__(self, arg: :py:obj:`mitsuba.DiscreteDistribution`) -> None``
        
        Copy constructor
        
        3. ``__init__(self, pmf: drjit.llvm.ad.Float) -> None``
        
        Initialize from a given probability mass function

        
    .. py:method:: mitsuba.DiscreteDistribution.cdf
        :property:

        Return the unnormalized cumulative distribution function

    .. py:method:: mitsuba.DiscreteDistribution.empty()

        Is the distribution object empty/uninitialized?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.DiscreteDistribution.eval_cdf(self, index, active=True)

        Evaluate the unnormalized cumulative distribution function (CDF) at
        index ``index``

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.DiscreteDistribution.eval_cdf_normalized(self, index, active=True)

        Evaluate the normalized cumulative distribution function (CDF) at
        index ``index``

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.DiscreteDistribution.eval_pmf(self, index, active=True)

        Evaluate the unnormalized probability mass function (PMF) at index
        ``index``

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.DiscreteDistribution.eval_pmf_normalized(self, index, active=True)

        Evaluate the normalized probability mass function (PMF) at index
        ``index``

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.DiscreteDistribution.normalization()

        Return the normalization factor (i.e. the inverse of sum())

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.DiscreteDistribution.pmf
        :property:

        Return the unnormalized probability mass function

    .. py:method:: mitsuba.DiscreteDistribution.sample(self, value, active=True)

        %Transform a uniformly distributed sample to the stored distribution

        Parameter ``sample``:
            A uniformly distributed sample on the interval [0, 1].

        Parameter ``value`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.UInt:
            The discrete index associated with the sample

    .. py:method:: mitsuba.DiscreteDistribution.sample_pmf(self, value, active=True)

        %Transform a uniformly distributed sample to the stored distribution

        Parameter ``value`` (drjit.llvm.ad.Float):
            A uniformly distributed sample on the interval [0, 1].

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[drjit.llvm.ad.UInt, drjit.llvm.ad.Float]:
            A tuple consisting of

        1. the discrete index associated with the sample, and 2. the
        normalized probability value of the sample.

    .. py:method:: mitsuba.DiscreteDistribution.sample_reuse(self, value, active=True)

        %Transform a uniformly distributed sample to the stored distribution

        The original sample is value adjusted so that it can be reused as a
        uniform variate.

        Parameter ``value`` (drjit.llvm.ad.Float):
            A uniformly distributed sample on the interval [0, 1].

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[drjit.llvm.ad.UInt, drjit.llvm.ad.Float]:
            A tuple consisting of

        1. the discrete index associated with the sample, and 2. the re-scaled
        sample value.

    .. py:method:: mitsuba.DiscreteDistribution.sample_reuse_pmf(self, value, active=True)

        %Transform a uniformly distributed sample to the stored distribution.

        The original sample is value adjusted so that it can be reused as a
        uniform variate.

        Parameter ``value`` (drjit.llvm.ad.Float):
            A uniformly distributed sample on the interval [0, 1].

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[drjit.llvm.ad.UInt, drjit.llvm.ad.Float, drjit.llvm.ad.Float]:
            A tuple consisting of

        1. the discrete index associated with the sample 2. the re-scaled
        sample value 3. the normalized probability value of the sample

    .. py:method:: mitsuba.DiscreteDistribution.size()

        Return the number of entries

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.DiscreteDistribution.sum()

        Return the original sum of PMF entries before normalization

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.DiscreteDistribution.update()

        Update the internal state. Must be invoked when changing the pmf.

        Returns → None:
            *no description available*

.. py:class:: mitsuba.DiscreteDistribution2D

    .. py:method:: mitsuba.DiscreteDistribution2D.eval(self, pos, active=True)

        Parameter ``pos`` (:py:obj:`mitsuba.Point2u`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.DiscreteDistribution2D.pdf(self, pos, active=True)

        Parameter ``pos`` (:py:obj:`mitsuba.Point2u`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.DiscreteDistribution2D.sample(self, sample, active=True)

        Parameter ``sample`` (:py:obj:`mitsuba.Point2f`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Point2u`, drjit.llvm.ad.Float, :py:obj:`mitsuba.Point2f`]:
            *no description available*

.. py:class:: mitsuba.DummyStream

    Base class: :py:obj:`mitsuba.Stream`

    Stream implementation that never writes to disk, but keeps track of
    the size of the content being written. It can be used, for example, to
    measure the precise amount of memory needed to store serialized
    content.

    .. py:method:: __init__()


.. py:class:: mitsuba.Emitter

    Base class: :py:obj:`mitsuba.Endpoint`

    .. py:method:: mitsuba.Emitter.flags(self, active=True)

        Flags for all components combined.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Emitter.is_environment()

        Is this an environment map light emitter?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Emitter.m_flags
        :property:

        Combined flags for all properties of this emitter.

    .. py:method:: mitsuba.Emitter.m_needs_sample_2
        :property:

        (self) -> bool

    .. py:method:: mitsuba.Emitter.m_needs_sample_3
        :property:

        (self) -> bool

    .. py:method:: mitsuba.Emitter.sampling_weight()

        The emitter's sampling weight.

        Returns → float:
            *no description available*

.. py:class:: mitsuba.EmitterFlags

    This list of flags is used to classify the different types of
    emitters.

    Valid values are as follows:

    .. py:data:: Empty

        No flags set (default value)

    .. py:data:: DeltaPosition

        The emitter lies at a single point in space

    .. py:data:: DeltaDirection

        The emitter emits light in a single direction

    .. py:data:: Infinite

        The emitter is placed at infinity (e.g. environment maps)

    .. py:data:: Surface

        The emitter is attached to a surface (e.g. area emitters)

    .. py:data:: SpatiallyVarying

        The emission depends on the UV coordinates

    .. py:data:: Delta

        Delta function in either position or direction

.. py:class:: mitsuba.EmitterPtr

    .. py:method:: mitsuba.EmitterPtr.eval(self, si, active=True)

        Given a ray-surface intersection, return the emitted radiance or
        importance traveling along the reverse direction

        This function is e.g. used when an area light source has been hit by a
        ray in a path tracing-style integrator, and it subsequently needs to
        be queried for the emitted radiance along the negative ray direction.
        The default implementation throws an exception, which states that the
        method is not implemented.

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            An intersect record that specifies both the query position and
            direction (using the ``si.wi`` field)

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Color3f`:
            The emitted radiance or importance

    .. py:method:: mitsuba.EmitterPtr.eval_direction(self, it, ds, active=True)

        Re-evaluate the incident direct radiance/importance of the
        sample_direction() method.

        This function re-evaluates the incident direct radiance or importance
        and sample probability due to the endpoint so that division by
        ``ds.pdf`` equals the sampling weight returned by sample_direction().
        This may appear redundant, and indeed such a function would not find
        use in "normal" rendering algorithms.

        However, the ability to re-evaluate the contribution of a generated
        sample is important for differentiable rendering. For example, we
        might want to track derivatives in the sampled direction (``ds.d``)
        without also differentiating the sampling technique.

        In contrast to pdf_direction(), evaluating this function can yield a
        nonzero result in the case of emission profiles containing a Dirac
        delta term (e.g. point or directional lights).

        Parameter ``ref``:
            A 3D reference location within the scene, which may influence the
            sampling process.

        Parameter ``ds`` (:py:obj:`mitsuba.DirectionSample3f`):
            A direction sampling record, which specifies the query location.

        Parameter ``it`` (:py:obj:`mitsuba.Interaction3f`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Color3f`:
            The incident direct radiance/importance associated with the
            sample.

    .. py:method:: mitsuba.EmitterPtr.flags()

        Flags for all components combined.

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.EmitterPtr.get_medium()

        Return a pointer to the medium that surrounds the emitter

        Returns → :py:obj:`mitsuba.MediumPtr`:
            *no description available*

    .. py:method:: mitsuba.EmitterPtr.get_shape()

        Return the shape, to which the emitter is currently attached

        Returns → :py:obj:`mitsuba.ShapePtr`:
            *no description available*

    .. py:method:: mitsuba.EmitterPtr.is_environment()

        Is this an environment map light emitter?

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.EmitterPtr.pdf_direction(self, it, ds, active=True)

        Evaluate the probability density of the *direct* sampling method
        implemented by the sample_direction() method.

        The returned probability will always be zero when the
        emission/sensitivity profile contains a Dirac delta term (e.g. point
        or directional emitters/sensors).

        Parameter ``ds`` (:py:obj:`mitsuba.DirectionSample3f`):
            A direct sampling record, which specifies the query location.

        Parameter ``it`` (:py:obj:`mitsuba.Interaction3f`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.EmitterPtr.pdf_position(self, ps, active=True)

        Evaluate the probability density of the position sampling method
        implemented by sample_position().

        In simple cases, this will be the reciprocal of the endpoint's surface
        area.

        Parameter ``ps`` (:py:obj:`mitsuba.PositionSample3f`):
            The sampled position record.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            The corresponding sampling density.

    .. py:method:: mitsuba.EmitterPtr.sample_direction(self, it, sample, active=True)

        Given a reference point in the scene, sample a direction from the
        reference point towards the endpoint (ideally proportional to the
        emission/sensitivity profile)

        This operation is a generalization of direct illumination techniques
        to both emitters *and* sensors. A direction sampling method is given
        an arbitrary reference position in the scene and samples a direction
        from the reference point towards the endpoint (ideally proportional to
        the emission/sensitivity profile). This reduces the sampling domain
        from 4D to 2D, which often enables the construction of smarter
        specialized sampling techniques.

        Ideally, the implementation should importance sample the product of
        the emission profile and the geometry term between the reference point
        and the position on the endpoint.

        The default implementation throws an exception.

        Parameter ``ref``:
            A reference position somewhere within the scene.

        Parameter ``sample`` (:py:obj:`mitsuba.Point2f`):
            A uniformly distributed 2D point on the domain ``[0,1]^2``.

        Parameter ``it`` (:py:obj:`mitsuba.Interaction3f`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.DirectionSample3f`, :py:obj:`mitsuba.Color3f`]:
            A DirectionSample instance describing the generated sample along
            with a spectral importance weight.

    .. py:method:: mitsuba.EmitterPtr.sample_position(self, time, sample, active=True)

        Importance sample the spatial component of the emission or importance
        profile of the endpoint.

        The default implementation throws an exception.

        Parameter ``time`` (drjit.llvm.ad.Float):
            The scene time associated with the position to be sampled.

        Parameter ``sample`` (:py:obj:`mitsuba.Point2f`):
            A uniformly distributed 2D point on the domain ``[0,1]^2``.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.PositionSample3f`, drjit.llvm.ad.Float]:
            A PositionSample instance describing the generated sample along
            with an importance weight.

    .. py:method:: mitsuba.EmitterPtr.sample_ray(self, time, sample1, sample2, sample3, active=True)

        Importance sample a ray proportional to the endpoint's
        sensitivity/emission profile.

        The endpoint profile is a six-dimensional quantity that depends on
        time, wavelength, surface position, and direction. This function takes
        a given time value and five uniformly distributed samples on the
        interval [0, 1] and warps them so that the returned ray follows the
        profile. Any discrepancies between ideal and actual sampled profile
        are absorbed into a spectral importance weight that is returned along
        with the ray.

        Parameter ``time`` (drjit.llvm.ad.Float):
            The scene time associated with the ray to be sampled

        Parameter ``sample1`` (drjit.llvm.ad.Float):
            A uniformly distributed 1D value that is used to sample the
            spectral dimension of the emission profile.

        Parameter ``sample2`` (:py:obj:`mitsuba.Point2f`):
            A uniformly distributed sample on the domain ``[0,1]^2``. For
            sensor endpoints, this argument corresponds to the sample position
            in fractional pixel coordinates relative to the crop window of the
            underlying film. This argument is ignored if ``needs_sample_2() ==
            false``.

        Parameter ``sample3`` (:py:obj:`mitsuba.Point2f`):
            A uniformly distributed sample on the domain ``[0,1]^2``. For
            sensor endpoints, this argument determines the position on the
            aperture of the sensor. This argument is ignored if
            ``needs_sample_3() == false``.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Ray3f`, :py:obj:`mitsuba.Color3f`]:
            The sampled ray and (potentially spectrally varying) importance
            weights. The latter account for the difference between the profile
            and the actual used sampling density function.

    .. py:method:: mitsuba.EmitterPtr.sample_wavelengths(self, si, sample, active=True)

        Importance sample a set of wavelengths according to the endpoint's
        sensitivity/emission spectrum.

        This function takes a uniformly distributed 1D sample and generates a
        sample that is approximately distributed according to the endpoint's
        spectral sensitivity/emission profile.

        For this, the input 1D sample is first replicated into
        ``Spectrum::Size`` separate samples using simple arithmetic
        transformations (see math::sample_shifted()), which can be interpreted
        as a type of Quasi-Monte-Carlo integration scheme. Following this, a
        standard technique (e.g. inverse transform sampling) is used to find
        the corresponding wavelengths. Any discrepancies between ideal and
        actual sampled profile are absorbed into a spectral importance weight
        that is returned along with the wavelengths.

        This function should not be called in RGB or monochromatic modes.

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            In the case of a spatially-varying spectral sensitivity/emission
            profile, this parameter conditions sampling on a specific spatial
            position. The ``si.uv`` field must be specified in this case.

        Parameter ``sample`` (drjit.llvm.ad.Float):
            A 1D uniformly distributed random variate

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Color0f`, :py:obj:`mitsuba.Color3f`]:
            The set of sampled wavelengths and (potentially spectrally
            varying) importance weights. The latter account for the difference
            between the profile and the actual used sampling density function.
            In the case of emitters, the weight will include the emitted
            radiance.

    .. py:method:: mitsuba.EmitterPtr.sampling_weight()

        The emitter's sampling weight.

        Returns → drjit.llvm.ad.Float:
            *no description available*

.. py:class:: mitsuba.Endpoint

    Base class: :py:obj:`mitsuba.Object`

    Abstract interface subsuming emitters and sensors in Mitsuba.

    This class provides an abstract interface to emitters and sensors in
    Mitsuba, which are named *endpoints* since they represent the first
    and last vertices of a light path. Thanks to symmetries underlying the
    equations of light transport and scattering, sensors and emitters can
    be treated as essentially the same thing, their main difference being
    type of emitted radiation: light sources emit *radiance*, while
    sensors emit a conceptual radiation named *importance*. This class
    casts these symmetries into a unified API that enables access to both
    types of endpoints using the same set of functions.

    Subclasses of this interface must implement functions to evaluate and
    sample the emission/response profile, and to compute probability
    densities associated with the provided sampling techniques.

    In addition to :py:meth:`mitsuba.Endpoint.sample_ray`, which generates
    a sample from the profile, subclasses also provide a specialized
    *direction sampling* method in
    :py:meth:`mitsuba.Endpoint.sample_direction`. This is a generalization
    of direct illumination techniques to both emitters *and* sensors. A
    direction sampling method is given an arbitrary reference position in
    the scene and samples a direction from the reference point towards the
    endpoint (ideally proportional to the emission/sensitivity profile).
    This reduces the sampling domain from 4D to 2D, which often enables
    the construction of smarter specialized sampling techniques.

    When rendering scenes involving participating media, it is important
    to know what medium surrounds the sensors and emitters. For this
    reason, every endpoint instance keeps a reference to a medium (which
    may be set to ``nullptr`` when the endpoint is surrounded by vacuum).

    In the context of polarized simulation, the perfect symmetry between
    emitters and sensors technically breaks down: the former emit 4D
    *Stokes vectors* encoding the polarization state of light, while
    sensors are characterized by 4x4 *Mueller matrices* that transform the
    incident polarization prior to measurement. We sidestep this non-
    symmetry by simply using Mueller matrices everywhere: in the case of
    emitters, only the first column will be used (the remainder being
    filled with zeros). This API simplification comes at a small extra
    cost in terms of register usage and arithmetic. The JIT (LLVM, CUDA)
    variants of Mitsuba can recognize these redundancies and remove them
    retroactively.

    .. py:method:: mitsuba.Endpoint.bbox()

        Return an axis-aligned box bounding the spatial extents of the emitter

        Returns → :py:obj:`mitsuba.ScalarBoundingBox3f`:
            *no description available*

    .. py:method:: mitsuba.Endpoint.eval(self, si, active=True)

        Given a ray-surface intersection, return the emitted radiance or
        importance traveling along the reverse direction

        This function is e.g. used when an area light source has been hit by a
        ray in a path tracing-style integrator, and it subsequently needs to
        be queried for the emitted radiance along the negative ray direction.
        The default implementation throws an exception, which states that the
        method is not implemented.

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            An intersect record that specifies both the query position and
            direction (using the ``si.wi`` field)

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Color3f`:
            The emitted radiance or importance

    .. py:method:: mitsuba.Endpoint.eval_direction(self, it, ds, active=True)

        Re-evaluate the incident direct radiance/importance of the
        sample_direction() method.

        This function re-evaluates the incident direct radiance or importance
        and sample probability due to the endpoint so that division by
        ``ds.pdf`` equals the sampling weight returned by sample_direction().
        This may appear redundant, and indeed such a function would not find
        use in "normal" rendering algorithms.

        However, the ability to re-evaluate the contribution of a generated
        sample is important for differentiable rendering. For example, we
        might want to track derivatives in the sampled direction (``ds.d``)
        without also differentiating the sampling technique.

        In contrast to pdf_direction(), evaluating this function can yield a
        nonzero result in the case of emission profiles containing a Dirac
        delta term (e.g. point or directional lights).

        Parameter ``ref``:
            A 3D reference location within the scene, which may influence the
            sampling process.

        Parameter ``ds`` (:py:obj:`mitsuba.DirectionSample3f`):
            A direction sampling record, which specifies the query location.

        Parameter ``it`` (:py:obj:`mitsuba.Interaction3f`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Color3f`:
            The incident direct radiance/importance associated with the
            sample.

    .. py:method:: mitsuba.Endpoint.get_medium()

        Return a pointer to the medium that surrounds the emitter

        Returns → :py:obj:`mitsuba.Medium`:
            *no description available*

    .. py:method:: mitsuba.Endpoint.get_shape()

        Return the shape, to which the emitter is currently attached

        Returns → :py:obj:`mitsuba.Shape`:
            *no description available*

    .. py:method:: mitsuba.Endpoint.needs_sample_2()

        Does the method sample_ray() require a uniformly distributed 2D sample
        for the ``sample2`` parameter?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Endpoint.needs_sample_3()

        Does the method sample_ray() require a uniformly distributed 2D sample
        for the ``sample3`` parameter?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Endpoint.pdf_direction(self, it, ds, active=True)

        Evaluate the probability density of the *direct* sampling method
        implemented by the sample_direction() method.

        The returned probability will always be zero when the
        emission/sensitivity profile contains a Dirac delta term (e.g. point
        or directional emitters/sensors).

        Parameter ``ds`` (:py:obj:`mitsuba.DirectionSample3f`):
            A direct sampling record, which specifies the query location.

        Parameter ``it`` (:py:obj:`mitsuba.Interaction3f`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Endpoint.pdf_position(self, ps, active=True)

        Evaluate the probability density of the position sampling method
        implemented by sample_position().

        In simple cases, this will be the reciprocal of the endpoint's surface
        area.

        Parameter ``ps`` (:py:obj:`mitsuba.PositionSample3f`):
            The sampled position record.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            The corresponding sampling density.

    .. py:method:: mitsuba.Endpoint.sample_direction(self, it, sample, active=True)

        Given a reference point in the scene, sample a direction from the
        reference point towards the endpoint (ideally proportional to the
        emission/sensitivity profile)

        This operation is a generalization of direct illumination techniques
        to both emitters *and* sensors. A direction sampling method is given
        an arbitrary reference position in the scene and samples a direction
        from the reference point towards the endpoint (ideally proportional to
        the emission/sensitivity profile). This reduces the sampling domain
        from 4D to 2D, which often enables the construction of smarter
        specialized sampling techniques.

        Ideally, the implementation should importance sample the product of
        the emission profile and the geometry term between the reference point
        and the position on the endpoint.

        The default implementation throws an exception.

        Parameter ``ref``:
            A reference position somewhere within the scene.

        Parameter ``sample`` (:py:obj:`mitsuba.Point2f`):
            A uniformly distributed 2D point on the domain ``[0,1]^2``.

        Parameter ``it`` (:py:obj:`mitsuba.Interaction3f`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.DirectionSample3f`, :py:obj:`mitsuba.Color3f`]:
            A DirectionSample instance describing the generated sample along
            with a spectral importance weight.

    .. py:method:: mitsuba.Endpoint.sample_position(self, ref, ds, active=True)

        Importance sample the spatial component of the emission or importance
        profile of the endpoint.

        The default implementation throws an exception.

        Parameter ``time``:
            The scene time associated with the position to be sampled.

        Parameter ``sample``:
            A uniformly distributed 2D point on the domain ``[0,1]^2``.

        Parameter ``ref`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``ds`` (:py:obj:`mitsuba.Point2f`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.PositionSample3f`, drjit.llvm.ad.Float]:
            A PositionSample instance describing the generated sample along
            with an importance weight.

    .. py:method:: mitsuba.Endpoint.sample_ray(self, time, sample1, sample2, sample3, active=True)

        Importance sample a ray proportional to the endpoint's
        sensitivity/emission profile.

        The endpoint profile is a six-dimensional quantity that depends on
        time, wavelength, surface position, and direction. This function takes
        a given time value and five uniformly distributed samples on the
        interval [0, 1] and warps them so that the returned ray follows the
        profile. Any discrepancies between ideal and actual sampled profile
        are absorbed into a spectral importance weight that is returned along
        with the ray.

        Parameter ``time`` (drjit.llvm.ad.Float):
            The scene time associated with the ray to be sampled

        Parameter ``sample1`` (drjit.llvm.ad.Float):
            A uniformly distributed 1D value that is used to sample the
            spectral dimension of the emission profile.

        Parameter ``sample2`` (:py:obj:`mitsuba.Point2f`):
            A uniformly distributed sample on the domain ``[0,1]^2``. For
            sensor endpoints, this argument corresponds to the sample position
            in fractional pixel coordinates relative to the crop window of the
            underlying film. This argument is ignored if ``needs_sample_2() ==
            false``.

        Parameter ``sample3`` (:py:obj:`mitsuba.Point2f`):
            A uniformly distributed sample on the domain ``[0,1]^2``. For
            sensor endpoints, this argument determines the position on the
            aperture of the sensor. This argument is ignored if
            ``needs_sample_3() == false``.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Ray3f`, :py:obj:`mitsuba.Color3f`]:
            The sampled ray and (potentially spectrally varying) importance
            weights. The latter account for the difference between the profile
            and the actual used sampling density function.

    .. py:method:: mitsuba.Endpoint.sample_wavelengths(self, si, sample, active=True)

        Importance sample a set of wavelengths according to the endpoint's
        sensitivity/emission spectrum.

        This function takes a uniformly distributed 1D sample and generates a
        sample that is approximately distributed according to the endpoint's
        spectral sensitivity/emission profile.

        For this, the input 1D sample is first replicated into
        ``Spectrum::Size`` separate samples using simple arithmetic
        transformations (see math::sample_shifted()), which can be interpreted
        as a type of Quasi-Monte-Carlo integration scheme. Following this, a
        standard technique (e.g. inverse transform sampling) is used to find
        the corresponding wavelengths. Any discrepancies between ideal and
        actual sampled profile are absorbed into a spectral importance weight
        that is returned along with the wavelengths.

        This function should not be called in RGB or monochromatic modes.

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            In the case of a spatially-varying spectral sensitivity/emission
            profile, this parameter conditions sampling on a specific spatial
            position. The ``si.uv`` field must be specified in this case.

        Parameter ``sample`` (drjit.llvm.ad.Float):
            A 1D uniformly distributed random variate

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Color0f`, :py:obj:`mitsuba.Color3f`]:
            The set of sampled wavelengths and (potentially spectrally
            varying) importance weights. The latter account for the difference
            between the profile and the actual used sampling density function.
            In the case of emitters, the weight will include the emitted
            radiance.

    .. py:method:: mitsuba.Endpoint.set_medium(self, medium)

        Set the medium that surrounds the emitter.

        Parameter ``medium`` (:py:obj:`mitsuba.Medium`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Endpoint.set_scene(self, scene)

        Inform the emitter about the properties of the scene

        Various emitters that surround the scene (e.g. environment emitters)
        must be informed about the scene dimensions to operate correctly. This
        function is invoked by the Scene constructor.

        Parameter ``scene`` (:py:obj:`mitsuba.Scene`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Endpoint.set_shape(self, shape)

        Set the shape associated with this endpoint.

        Parameter ``shape`` (:py:obj:`mitsuba.Shape`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Endpoint.world_transform()

        Return the local space to world space transformation

        Returns → :py:obj:`mitsuba.Transform4f`:
            *no description available*

.. py:class:: mitsuba.FileResolver

    Base class: :py:obj:`mitsuba.Object`

    Simple class for resolving paths on Linux/Windows/Mac OS

    This convenience class looks for a file or directory given its name
    and a set of search paths. The implementation walks through the search
    paths in order and stops once the file is found.

    .. py:method:: __init__()

        Overloaded function.
        
        1. ``__init__(self) -> None``
        
        Initialize a new file resolver with the current working directory
        
        2. ``__init__(self, arg: :py:obj:`mitsuba.FileResolver`) -> None``
        
        Copy constructor

        
    .. py:method:: mitsuba.FileResolver.append(self, arg)

        Append an entry to the end of the list of search paths

        Parameter ``arg`` (:py:obj:`mitsuba.filesystem.path`, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.FileResolver.clear()

        Clear the list of search paths

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.FileResolver.prepend(self, arg)

        Prepend an entry at the beginning of the list of search paths

        Parameter ``arg`` (:py:obj:`mitsuba.filesystem.path`, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.FileResolver.resolve(self, arg)

        Walk through the list of search paths and try to resolve the input
        path

        Parameter ``arg`` (:py:obj:`mitsuba.filesystem.path`, /):
            *no description available*

        Returns → :py:obj:`mitsuba.filesystem.path`:
            *no description available*

.. py:class:: mitsuba.FileStream

    Base class: :py:obj:`mitsuba.Stream`

    Simple Stream implementation backed-up by a file.

    The underlying file abstraction is ``std::fstream``, and so most
    operations can be expected to behave similarly.

    .. py:method:: __init__(self, p, mode=EMode.ERead)

        Constructs a new FileStream by opening the file pointed by ``p``.
        
        The file is opened in read-only or read/write mode as specified by
        ``mode``.
        
        Throws if trying to open a non-existing file in with write disabled.
        Throws an exception if the file cannot be opened / created.

        Parameter ``p`` (:py:obj:`mitsuba.filesystem.path`):
            *no description available*

        Parameter ``mode`` (:py:obj:`mitsuba.FileStream.EMode`):
            *no description available*

        
    .. py:method:: mitsuba.FileStream.path()

        Return the path descriptor associated with this FileStream

        Returns → :py:obj:`mitsuba.filesystem.path`:
            *no description available*

.. py:class:: mitsuba.Film

    Base class: :py:obj:`mitsuba.Object`

    Abstract film base class - used to store samples generated by
    Integrator implementations.

    To avoid lock-related bottlenecks when rendering with many cores,
    rendering threads first store results in an "image block", which is
    then committed to the film using the put() method.

    .. py:method:: __init__(self, props)

        Parameter ``props`` (:py:obj:`mitsuba.Properties`):
            *no description available*


    .. py:method:: mitsuba.Film.base_channels_count()

        Return the number of channels for the developed image (excluding AOVS)

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Film.bitmap(self, raw=False)

        Return a bitmap object storing the developed contents of the film

        Parameter ``raw`` (bool):
            *no description available*

        Returns → :py:obj:`mitsuba.Bitmap`:
            *no description available*

    .. py:method:: mitsuba.Film.clear()

        Clear the film contents to zero.

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Film.create_block(self, size=[0, 0], normalize=False, borders=False)

        Return an ImageBlock instance, whose internal representation is
        compatible with that of the film.

        Image blocks created using this method can later be merged into the
        film using put_block().

        Parameter ``size`` (:py:obj:`mitsuba.ScalarVector2u`):
            Desired size of the returned image block.

        Parameter ``normalize`` (bool):
            Force normalization of filter weights in ImageBlock::put()? See
            the ImageBlock constructor for details.

        Parameter ``border``:
            Should ``ImageBlock`` add an additional border region around
            around the image boundary? See the ImageBlock constructor for
            details.

        Parameter ``borders`` (bool):
            *no description available*

        Returns → :py:obj:`mitsuba.ImageBlock`:
            *no description available*

    .. py:method:: mitsuba.Film.crop_offset()

        Return the offset of the crop window

        Returns → :py:obj:`mitsuba.ScalarPoint2u`:
            *no description available*

    .. py:method:: mitsuba.Film.crop_size()

        Return the size of the crop window

        Returns → :py:obj:`mitsuba.ScalarVector2u`:
            *no description available*

    .. py:method:: mitsuba.Film.develop(self, raw=False)

        Return a image buffer object storing the developed image

        Parameter ``raw`` (bool):
            *no description available*

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.Film.flags()

        Flags for all properties combined.

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Film.m_flags
        :property:

        Combined flags for all properties of this film.

    .. py:method:: mitsuba.Film.prepare(self, aovs)

        Configure the film for rendering a specified set of extra channels
        (AOVS). Returns the total number of channels that the film will store

        Parameter ``aovs`` (collections.abc.Sequence[str]):
            *no description available*

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Film.prepare_sample(self, spec, wavelengths, nChannels, weight=1.0, alpha=1.0, active=True)

        Prepare spectrum samples to be in the format expected by the film

        It will be used if the Film contains the ``Special`` flag enabled.

        This method should be applied with films that deviate from HDR film
        behavior. Normally ``Films`` will store within the ``ImageBlock`` the
        samples following an RGB shape. But ``Films`` may want to store the
        samples with other structures (e.g. store several channels containing
        monochromatic information). In that situation, this method allows
        transforming the sample format generated by the integrators to the one
        that the Film will store inside the ImageBlock.

        Parameter ``spec`` (:py:obj:`mitsuba.Color3f`):
            Sample value associated with the specified wavelengths

        Parameter ``wavelengths`` (:py:obj:`mitsuba.Color0f`):
            Sample wavelengths in nanometers

        Parameter ``aovs``:
            Points to an array of length equal to the number of spectral
            sensitivities of the film, which specifies the sample value for
            each channel.

        Parameter ``weight`` (drjit.llvm.ad.Float):
            Value to be added to the weight channel of the sample

        Parameter ``alpha`` (drjit.llvm.ad.Float):
            Alpha value of the sample

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask indicating if the lanes are active

        Parameter ``nChannels`` (int):
            *no description available*

        Returns → list[drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.Film.put_block(self, block)

        Merge an image block into the film. This methods should be thread-
        safe.

        Parameter ``block`` (:py:obj:`mitsuba.ImageBlock`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Film.rfilter()

        Return the image reconstruction filter (const version)

        Returns → :py:obj:`mitsuba.ReconstructionFilter`:
            *no description available*

    .. py:method:: mitsuba.Film.sample_border()

        Should regions slightly outside the image plane be sampled to improve
        the quality of the reconstruction at the edges? This only makes sense
        when reconstruction filters other than the box filter are used.

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Film.schedule_storage()

        dr::schedule() variables that represent the internal film storage

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Film.sensor_response_function()

        Returns the specific Sensor Response Function (SRF) used by the film

        Returns → :py:obj:`mitsuba.Texture`:
            *no description available*

    .. py:method:: mitsuba.Film.size()

        Ignoring the crop window, return the resolution of the underlying
        sensor

        Returns → :py:obj:`mitsuba.ScalarVector2u`:
            *no description available*

    .. py:method:: mitsuba.Film.write(self, path)

        Write the developed contents of the film to a file on disk

        Parameter ``path`` (:py:obj:`mitsuba.filesystem.path`):
            *no description available*

        Returns → None:
            *no description available*

.. py:class:: mitsuba.FilmFlags

    This list of flags is used to classify the different types of films.

    Valid values are as follows:

    .. py:data:: Empty

        No flags set (default value)

    .. py:data:: Alpha

        The film stores an alpha channel

    .. py:data:: Spectral

        The film stores a spectral representation of the image

    .. py:data:: Special

        The film provides a customized prepare_sample() routine that implements a special treatment of the samples before storing them in the Image Block.

.. py:class:: mitsuba.FilterBoundaryCondition

    When resampling data to a different resolution using
    Resampler::resample(), this enumeration specifies how lookups
    *outside* of the input domain are handled.

    See also:
        Resampler

    Valid values are as follows:

    .. py:data:: Clamp

        Clamp to the outermost sample position (default)

    .. py:data:: Repeat

        Assume that the input repeats in a periodic fashion

    .. py:data:: Mirror

        Assume that the input is mirrored along the boundary

    .. py:data:: Zero

        Assume that the input function is zero outside of the defined domain

    .. py:data:: One

        Assume that the input function is equal to one outside of the defined domain

.. py:class:: mitsuba.Float

.. py:class:: mitsuba.Float64

.. py:class:: mitsuba.Formatter

    Base class: :py:obj:`mitsuba.Object`

    Abstract interface for converting log information into a human-
    readable format

    .. py:method:: __init__()


    .. py:method:: mitsuba.Formatter.format(self, level, class_, thread, file, line, msg)

        Turn a log message into a human-readable format

        Parameter ``level`` (:py:obj:`mitsuba.LogLevel`):
            The importance of the debug message

        Parameter ``class_`` (:py:obj:`mitsuba.Class`):
            Originating class or ``nullptr``

        Parameter ``thread`` (:py:obj:`mitsuba.Thread`):
            Thread, which is responsible for creating the message

        Parameter ``file`` (str):
            File, which is responsible for creating the message

        Parameter ``line`` (int):
            Associated line within the source file

        Parameter ``msg`` (str):
            Text content associated with the log message

        Returns → str:
            *no description available*

.. py:class:: mitsuba.Frame3f

    Stores a three-dimensional orthonormal coordinate frame

    This class is used to convert between different cartesian coordinate
    systems and to efficiently evaluate trigonometric functions in a
    spherical coordinate system whose pole is aligned with the ``n`` axis
    (e.g. cos_theta(), sin_phi(), etc.).

    .. py:method:: __init__()

        Overloaded function.
        
        1. ``__init__(self) -> None``
        
        Construct a new coordinate frame from a single vector
        
        2. ``__init__(self, arg: :py:obj:`mitsuba.Frame3f`) -> None``
        
        Copy constructor
        
        3. ``__init__(self, arg0: :py:obj:`mitsuba.Vector3f`, arg1: :py:obj:`mitsuba.Vector3f`, arg2: :py:obj:`mitsuba.Vector3f`, /) -> None``
        
        
        4. ``__init__(self, arg: :py:obj:`mitsuba.Vector3f`, /) -> None``

        
    .. py:method:: mitsuba.Frame3f.assign(self, arg)

        Parameter ``arg`` (:py:obj:`mitsuba.Frame3f`, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Frame3f.n
        :property:

        (self) -> :py:obj:`mitsuba.Normal3f`

    .. py:method:: mitsuba.Frame3f.s
        :property:

        (self) -> :py:obj:`mitsuba.Vector3f`

    .. py:method:: mitsuba.Frame3f.t
        :property:

        (self) -> :py:obj:`mitsuba.Vector3f`

    .. py:method:: mitsuba.Frame3f.to_local(self, v)

        Convert from world coordinates to local coordinates

        Parameter ``v`` (:py:obj:`mitsuba.Vector3f`):
            *no description available*

        Returns → :py:obj:`mitsuba.Vector3f`:
            *no description available*

    .. py:method:: mitsuba.Frame3f.to_world(self, v)

        Convert from local coordinates to world coordinates

        Parameter ``v`` (:py:obj:`mitsuba.Vector3f`):
            *no description available*

        Returns → :py:obj:`mitsuba.Vector3f`:
            *no description available*

.. py:class:: mitsuba.Hierarchical2D0

    Implements a hierarchical sample warping scheme for 2D distributions
    with linear interpolation and an optional dependence on additional
    parameters

    This class takes a rectangular floating point array as input and
    constructs internal data structures to efficiently map uniform
    variates from the unit square ``[0, 1]^2`` to a function on ``[0,
    1]^2`` that linearly interpolates the input array.

    The mapping is constructed from a sequence of ``log2(max(res))``
    hierarchical sample warping steps, where ``res`` is the input array
    resolution. It is bijective and generally very well-behaved (i.e. low
    distortion), which makes it a good choice for structured point sets
    such as the Halton or Sobol sequence.

    The implementation also supports *conditional distributions*, i.e. 2D
    distributions that depend on an arbitrary number of parameters
    (indicated via the ``Dimension`` template parameter).

    In this case, the input array should have dimensions ``N0 x N1 x ... x
    Nn x res.y() x res.x()`` (where the last dimension is contiguous in
    memory), and the ``param_res`` should be set to ``{ N0, N1, ..., Nn
    }``, and ``param_values`` should contain the parameter values where
    the distribution is discretized. Linear interpolation is used when
    sampling or evaluating the distribution for in-between parameter
    values.

    Remark:
        The Python API exposes explicitly instantiated versions of this
        class named Hierarchical2D0, Hierarchical2D1, and Hierarchical2D2
        for data that depends on 0, 1, and 2 parameters, respectively.

    .. py:method:: __init__(self, data, param_values=[], normalize=True, enable_sampling=True)

        Construct a hierarchical sample warping scheme for floating point data
        of resolution ``size``.
        
        ``param_res`` and ``param_values`` are only needed for conditional
        distributions (see the text describing the Hierarchical2D class).
        
        If ``normalize`` is set to ``False``, the implementation will not re-
        scale the distribution so that it integrates to ``1``. It can still be
        sampled (proportionally), but returned density values will reflect the
        unnormalized values.
        
        If ``enable_sampling`` is set to ``False``, the implementation will
        not construct the hierarchy needed for sample warping, which saves
        memory in case this functionality is not needed (e.g. if only the
        interpolation in ``eval()`` is used). In this case, ``sample()`` and
        ``invert()`` can still be called without triggering undefined
        behavior, but they will not return meaningful results.

        Parameter ``data`` (ndarray[dtype=float32, shape=(*, *), order='C']):
            *no description available*

        Parameter ``param_values`` (collections.abc.Sequence[collections.abc.Sequence[float]]):
            *no description available*

        Parameter ``normalize`` (bool):
            *no description available*

        Parameter ``enable_sampling`` (bool):
            *no description available*

        
    .. py:method:: mitsuba.Hierarchical2D0.eval(self, pos, param=[], active=True)

        Evaluate the density at position ``pos``. The distribution is
        parameterized by ``param`` if applicable.

        Parameter ``pos`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array0f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Hierarchical2D0.invert(self, sample, param=[], active=True)

        Inverse of the mapping implemented in ``sample()``

        Parameter ``sample`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array0f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.Hierarchical2D0.sample(self, sample, param=[], active=True)

        Given a uniformly distributed 2D sample, draw a sample from the
        distribution (parameterized by ``param`` if applicable)

        Returns the warped sample and associated probability density.

        Parameter ``sample`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array0f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
            *no description available*

.. py:class:: mitsuba.Hierarchical2D1

    Implements a hierarchical sample warping scheme for 2D distributions
    with linear interpolation and an optional dependence on additional
    parameters

    This class takes a rectangular floating point array as input and
    constructs internal data structures to efficiently map uniform
    variates from the unit square ``[0, 1]^2`` to a function on ``[0,
    1]^2`` that linearly interpolates the input array.

    The mapping is constructed from a sequence of ``log2(max(res))``
    hierarchical sample warping steps, where ``res`` is the input array
    resolution. It is bijective and generally very well-behaved (i.e. low
    distortion), which makes it a good choice for structured point sets
    such as the Halton or Sobol sequence.

    The implementation also supports *conditional distributions*, i.e. 2D
    distributions that depend on an arbitrary number of parameters
    (indicated via the ``Dimension`` template parameter).

    In this case, the input array should have dimensions ``N0 x N1 x ... x
    Nn x res.y() x res.x()`` (where the last dimension is contiguous in
    memory), and the ``param_res`` should be set to ``{ N0, N1, ..., Nn
    }``, and ``param_values`` should contain the parameter values where
    the distribution is discretized. Linear interpolation is used when
    sampling or evaluating the distribution for in-between parameter
    values.

    Remark:
        The Python API exposes explicitly instantiated versions of this
        class named Hierarchical2D0, Hierarchical2D1, and Hierarchical2D2
        for data that depends on 0, 1, and 2 parameters, respectively.

    .. py:method:: __init__(self, data, param_values, normalize=True, build_hierarchy=True)

        Construct a hierarchical sample warping scheme for floating point data
        of resolution ``size``.
        
        ``param_res`` and ``param_values`` are only needed for conditional
        distributions (see the text describing the Hierarchical2D class).
        
        If ``normalize`` is set to ``False``, the implementation will not re-
        scale the distribution so that it integrates to ``1``. It can still be
        sampled (proportionally), but returned density values will reflect the
        unnormalized values.
        
        If ``enable_sampling`` is set to ``False``, the implementation will
        not construct the hierarchy needed for sample warping, which saves
        memory in case this functionality is not needed (e.g. if only the
        interpolation in ``eval()`` is used). In this case, ``sample()`` and
        ``invert()`` can still be called without triggering undefined
        behavior, but they will not return meaningful results.

        Parameter ``data`` (ndarray[dtype=float32, shape=(*, *, *), order='C']):
            *no description available*

        Parameter ``param_values`` (collections.abc.Sequence[collections.abc.Sequence[float]]):
            *no description available*

        Parameter ``normalize`` (bool):
            *no description available*

        Parameter ``build_hierarchy`` (bool):
            *no description available*

        
    .. py:method:: mitsuba.Hierarchical2D1.eval(self, pos, param=[0], active=True)

        Evaluate the density at position ``pos``. The distribution is
        parameterized by ``param`` if applicable.

        Parameter ``pos`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array1f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Hierarchical2D1.invert(self, sample, param=[0], active=True)

        Inverse of the mapping implemented in ``sample()``

        Parameter ``sample`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array1f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.Hierarchical2D1.sample(self, sample, param=[0], active=True)

        Given a uniformly distributed 2D sample, draw a sample from the
        distribution (parameterized by ``param`` if applicable)

        Returns the warped sample and associated probability density.

        Parameter ``sample`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array1f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
            *no description available*

.. py:class:: mitsuba.Hierarchical2D2

    Implements a hierarchical sample warping scheme for 2D distributions
    with linear interpolation and an optional dependence on additional
    parameters

    This class takes a rectangular floating point array as input and
    constructs internal data structures to efficiently map uniform
    variates from the unit square ``[0, 1]^2`` to a function on ``[0,
    1]^2`` that linearly interpolates the input array.

    The mapping is constructed from a sequence of ``log2(max(res))``
    hierarchical sample warping steps, where ``res`` is the input array
    resolution. It is bijective and generally very well-behaved (i.e. low
    distortion), which makes it a good choice for structured point sets
    such as the Halton or Sobol sequence.

    The implementation also supports *conditional distributions*, i.e. 2D
    distributions that depend on an arbitrary number of parameters
    (indicated via the ``Dimension`` template parameter).

    In this case, the input array should have dimensions ``N0 x N1 x ... x
    Nn x res.y() x res.x()`` (where the last dimension is contiguous in
    memory), and the ``param_res`` should be set to ``{ N0, N1, ..., Nn
    }``, and ``param_values`` should contain the parameter values where
    the distribution is discretized. Linear interpolation is used when
    sampling or evaluating the distribution for in-between parameter
    values.

    Remark:
        The Python API exposes explicitly instantiated versions of this
        class named Hierarchical2D0, Hierarchical2D1, and Hierarchical2D2
        for data that depends on 0, 1, and 2 parameters, respectively.

    .. py:method:: __init__(self, data, param_values, normalize=True, build_hierarchy=True)

        Construct a hierarchical sample warping scheme for floating point data
        of resolution ``size``.
        
        ``param_res`` and ``param_values`` are only needed for conditional
        distributions (see the text describing the Hierarchical2D class).
        
        If ``normalize`` is set to ``False``, the implementation will not re-
        scale the distribution so that it integrates to ``1``. It can still be
        sampled (proportionally), but returned density values will reflect the
        unnormalized values.
        
        If ``enable_sampling`` is set to ``False``, the implementation will
        not construct the hierarchy needed for sample warping, which saves
        memory in case this functionality is not needed (e.g. if only the
        interpolation in ``eval()`` is used). In this case, ``sample()`` and
        ``invert()`` can still be called without triggering undefined
        behavior, but they will not return meaningful results.

        Parameter ``data`` (ndarray[dtype=float32, shape=(*, *, *, *), order='C']):
            *no description available*

        Parameter ``param_values`` (collections.abc.Sequence[collections.abc.Sequence[float]]):
            *no description available*

        Parameter ``normalize`` (bool):
            *no description available*

        Parameter ``build_hierarchy`` (bool):
            *no description available*

        
    .. py:method:: mitsuba.Hierarchical2D2.eval(self, pos, param=[0, 0], active=True)

        Evaluate the density at position ``pos``. The distribution is
        parameterized by ``param`` if applicable.

        Parameter ``pos`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Hierarchical2D2.invert(self, sample, param=[0, 0], active=True)

        Inverse of the mapping implemented in ``sample()``

        Parameter ``sample`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.Hierarchical2D2.sample(self, sample, param=[0, 0], active=True)

        Given a uniformly distributed 2D sample, draw a sample from the
        distribution (parameterized by ``param`` if applicable)

        Returns the warped sample and associated probability density.

        Parameter ``sample`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
            *no description available*

.. py:class:: mitsuba.Hierarchical2D3

    Implements a hierarchical sample warping scheme for 2D distributions
    with linear interpolation and an optional dependence on additional
    parameters

    This class takes a rectangular floating point array as input and
    constructs internal data structures to efficiently map uniform
    variates from the unit square ``[0, 1]^2`` to a function on ``[0,
    1]^2`` that linearly interpolates the input array.

    The mapping is constructed from a sequence of ``log2(max(res))``
    hierarchical sample warping steps, where ``res`` is the input array
    resolution. It is bijective and generally very well-behaved (i.e. low
    distortion), which makes it a good choice for structured point sets
    such as the Halton or Sobol sequence.

    The implementation also supports *conditional distributions*, i.e. 2D
    distributions that depend on an arbitrary number of parameters
    (indicated via the ``Dimension`` template parameter).

    In this case, the input array should have dimensions ``N0 x N1 x ... x
    Nn x res.y() x res.x()`` (where the last dimension is contiguous in
    memory), and the ``param_res`` should be set to ``{ N0, N1, ..., Nn
    }``, and ``param_values`` should contain the parameter values where
    the distribution is discretized. Linear interpolation is used when
    sampling or evaluating the distribution for in-between parameter
    values.

    Remark:
        The Python API exposes explicitly instantiated versions of this
        class named Hierarchical2D0, Hierarchical2D1, and Hierarchical2D2
        for data that depends on 0, 1, and 2 parameters, respectively.

    .. py:method:: __init__(self, data, param_values, normalize=True, build_hierarchy=True)

        Construct a hierarchical sample warping scheme for floating point data
        of resolution ``size``.
        
        ``param_res`` and ``param_values`` are only needed for conditional
        distributions (see the text describing the Hierarchical2D class).
        
        If ``normalize`` is set to ``False``, the implementation will not re-
        scale the distribution so that it integrates to ``1``. It can still be
        sampled (proportionally), but returned density values will reflect the
        unnormalized values.
        
        If ``enable_sampling`` is set to ``False``, the implementation will
        not construct the hierarchy needed for sample warping, which saves
        memory in case this functionality is not needed (e.g. if only the
        interpolation in ``eval()`` is used). In this case, ``sample()`` and
        ``invert()`` can still be called without triggering undefined
        behavior, but they will not return meaningful results.

        Parameter ``data`` (ndarray[dtype=float32, shape=(*, *, *, *, *), order='C']):
            *no description available*

        Parameter ``param_values`` (collections.abc.Sequence[collections.abc.Sequence[float]]):
            *no description available*

        Parameter ``normalize`` (bool):
            *no description available*

        Parameter ``build_hierarchy`` (bool):
            *no description available*

        
    .. py:method:: mitsuba.Hierarchical2D3.eval(self, pos, param=[0, 0, 0], active=True)

        Evaluate the density at position ``pos``. The distribution is
        parameterized by ``param`` if applicable.

        Parameter ``pos`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array3f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Hierarchical2D3.invert(self, sample, param=[0, 0, 0], active=True)

        Inverse of the mapping implemented in ``sample()``

        Parameter ``sample`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array3f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.Hierarchical2D3.sample(self, sample, param=[0, 0, 0], active=True)

        Given a uniformly distributed 2D sample, draw a sample from the
        distribution (parameterized by ``param`` if applicable)

        Returns the warped sample and associated probability density.

        Parameter ``sample`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array3f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
            *no description available*

.. py:class:: mitsuba.ImageBlock

    Base class: :py:obj:`mitsuba.Object`

    Intermediate storage for an image or image sub-region being rendered

    This class facilitates parallel rendering of images in both scalar and
    JIT-based variants of Mitsuba.

    In scalar mode, image blocks represent independent rectangular image
    regions that are simultaneously processed by worker threads. They are
    finally merged into a master ImageBlock controlled by the Film
    instance via the put_block() method. The smaller image blocks can
    include a border region storing contributions that are slightly
    outside of the block, which is required to correctly account for image
    reconstruction filters.

    In JIT variants there is only a single ImageBlock, whose contents are
    computed in parallel. A border region is usually not needed in this
    case.

    In addition to receiving samples via the put() method, the image block
    can also be queried via the read() method, in which case the
    reconstruction filter is used to compute suitable interpolation
    weights. This is feature is useful for differentiable rendering, where
    one needs to evaluate the reverse-mode derivative of the put() method.

    .. py:method:: __init__(self, size, offset, channel_count, rfilter=None, border=False, normalize=False, coalesce=True, compensate=False, warn_negative=False, warn_invalid=False)

        Parameter ``size`` (:py:obj:`mitsuba.ScalarVector2u`):
            *no description available*

        Parameter ``offset`` (:py:obj:`mitsuba.ScalarPoint2i`):
            *no description available*

        Parameter ``channel_count`` (int):
            *no description available*

        Parameter ``rfilter`` (:py:obj:`mitsuba.ReconstructionFilter` | None):
            *no description available*

        Parameter ``border`` (bool):
            *no description available*

        Parameter ``normalize`` (bool):
            *no description available*

        Parameter ``coalesce`` (bool):
            *no description available*

        Parameter ``compensate`` (bool):
            *no description available*

        Parameter ``warn_negative`` (bool):
            *no description available*

        Parameter ``warn_invalid`` (bool):
            *no description available*


    .. py:method:: mitsuba.ImageBlock.border_size()

        Return the border region used by the reconstruction filter

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.ImageBlock.channel_count()

        Return the number of channels stored by the image block

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.ImageBlock.clear()

        Clear the image block contents to zero.

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ImageBlock.coalesce()

        Try to coalesce reads/writes in JIT modes?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.ImageBlock.compensate()

        Use Kahan-style error-compensated floating point accumulation?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.ImageBlock.has_border()

        Does the image block have a border region?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.ImageBlock.height()

        Return the bitmap's height in pixels

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.ImageBlock.normalize()

        Re-normalize filter weights in put() and read()

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.ImageBlock.offset()

        Return the current block offset

        Returns → :py:obj:`mitsuba.ScalarPoint2i`:
            *no description available*

    .. py:method:: mitsuba.ImageBlock.put(self, pos, wavelengths, value, alpha=1.0, weight=1, active=True)

        Overloaded function.

        1. ``put(self, pos: :py:obj:`mitsuba.Point2f`, wavelengths: :py:obj:`mitsuba.Color0f`, value: :py:obj:`mitsuba.Color3f`, alpha: drjit.llvm.ad.Float = 1.0, weight: drjit.llvm.ad.Float = 1, active: drjit.llvm.ad.Bool = True) -> None``

        Accumulate a single sample or a wavefront of samples into the image
        block.

        Parameter ``pos`` (:py:obj:`mitsuba.Point2f`):
            Denotes the sample position in fractional pixel coordinates

        Parameter ``values``:
            Points to an array of length channel_count(), which specifies the
            sample value for each channel.

        2. ``put(self, pos: :py:obj:`mitsuba.Point2f`, values: collections.abc.Sequence[drjit.llvm.ad.Float], active: drjit.llvm.ad.Bool = True) -> None``

        Parameter ``wavelengths`` (:py:obj:`mitsuba.Color0f`):
            *no description available*

        Parameter ``value`` (:py:obj:`mitsuba.Color3f`):
            *no description available*

        Parameter ``alpha`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``weight`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ImageBlock.put_block(self, block)

        Accumulate another image block into this one

        Parameter ``block`` (:py:obj:`mitsuba.ImageBlock`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ImageBlock.read(self, pos, active=True)

        Parameter ``pos`` (:py:obj:`mitsuba.Point2f`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → list[drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.ImageBlock.rfilter()

        Return the image reconstruction filter underlying the ImageBlock

        Returns → :py:obj:`mitsuba.ReconstructionFilter`:
            *no description available*

    .. py:method:: mitsuba.ImageBlock.set_coalesce(self, arg)

        Try to coalesce reads/writes in JIT modes?

        Parameter ``arg`` (bool, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ImageBlock.set_compensate(self, arg)

        Use Kahan-style error-compensated floating point accumulation?

        Parameter ``arg`` (bool, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ImageBlock.set_normalize(self, arg)

        Re-normalize filter weights in put() and read()

        Parameter ``arg`` (bool, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ImageBlock.set_offset(self, offset)

        Set the current block offset.

        This corresponds to the offset from the top-left corner of a larger
        image (e.g. a Film) to the top-left corner of this ImageBlock
        instance.

        Parameter ``offset`` (:py:obj:`mitsuba.ScalarPoint2i`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ImageBlock.set_size(self, size)

        Set the block size. This potentially destroys the block's content.

        Parameter ``size`` (:py:obj:`mitsuba.ScalarVector2u`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ImageBlock.set_warn_invalid(self, value)

        Warn when writing invalid (NaN, +/- infinity) sample values?

        Parameter ``value`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ImageBlock.set_warn_negative(self, value)

        Warn when writing negative sample values?

        Parameter ``value`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ImageBlock.size()

        Return the current block size

        Returns → :py:obj:`mitsuba.ScalarVector2u`:
            *no description available*

    .. py:method:: mitsuba.ImageBlock.tensor()

        Return the underlying image tensor

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.ImageBlock.warn_invalid()

        Warn when writing invalid (NaN, +/- infinity) sample values?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.ImageBlock.warn_negative()

        Warn when writing negative sample values?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.ImageBlock.width()

        Return the bitmap's width in pixels

        Returns → int:
            *no description available*

.. py:class:: mitsuba.Int

.. py:class:: mitsuba.Int64

.. py:class:: mitsuba.Integrator

    Base class: :py:obj:`mitsuba.Object`

    Abstract integrator base class, which does not make any assumptions
    with regards to how radiance is computed.

    In Mitsuba, the different rendering techniques are collectively
    referred to as *integrators*, since they perform integration over a
    high-dimensional space. Each integrator represents a specific approach
    for solving the light transport equation---usually favored in certain
    scenarios, but at the same time affected by its own set of intrinsic
    limitations. Therefore, it is important to carefully select an
    integrator based on user-specified accuracy requirements and
    properties of the scene to be rendered.

    This is the base class of all integrators; it does not make any
    assumptions on how radiance is computed, which allows for many
    different kinds of implementations.

    .. py:method:: mitsuba.Integrator.aov_names()

        For integrators that return one or more arbitrary output variables
        (AOVs), this function specifies a list of associated channel names.
        The default implementation simply returns an empty vector.

        Returns → list[str]:
            *no description available*

    .. py:method:: mitsuba.Integrator.cancel()

        Cancel a running render job (e.g. after receiving Ctrl-C)

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Integrator.render(self, scene, sensor, seed=0, spp=0, develop=True, evaluate=True)

        Overloaded function.

        1. ``render(self, scene: :py:obj:`mitsuba.Scene`, sensor: :py:obj:`mitsuba.Sensor`, seed: int = 0, spp: int = 0, develop: bool = True, evaluate: bool = True) -> drjit.llvm.ad.TensorXf``

        Render the scene

        This function renders the scene from the viewpoint of ``sensor``. All
        other parameters are optional and control different aspects of the
        rendering process. In particular:

        Parameter ``seed`` (int):
            This parameter controls the initialization of the random number
            generator. It is crucial that you specify different seeds (e.g.,
            an increasing sequence) if subsequent ``render``() calls should
            produce statistically independent images.

        Parameter ``spp`` (int):
            Set this parameter to a nonzero value to override the number of
            samples per pixel. This value then takes precedence over whatever
            was specified in the construction of ``sensor->sampler()``. This
            parameter may be useful in research applications where an image
            must be rendered multiple times using different quality levels.

        Parameter ``develop`` (bool):
            If set to ``True``, the implementation post-processes the data
            stored in ``sensor->film()``, returning the resulting image as a
            TensorXf. Otherwise, it returns an empty tensor.

        Parameter ``evaluate`` (bool):
            This parameter is only relevant for JIT variants of Mitsuba (LLVM,
            CUDA). If set to ``True``, the rendering step evaluates the
            generated image and waits for its completion. A log message also
            denotes the rendering time. Otherwise, the returned tensor
            (``develop=true``) or modified film (``develop=false``) represent
            the rendering task as an unevaluated computation graph.

        2. ``render(self, scene: :py:obj:`mitsuba.Scene`, sensor: int = 0, seed: int = 0, spp: int = 0, develop: bool = True, evaluate: bool = True) -> drjit.llvm.ad.TensorXf``

        Render the scene

        This function is just a thin wrapper around the previous render()
        overload. It accepts a sensor *index* instead and renders the scene
        using sensor 0 by default.

        Parameter ``scene`` (:py:obj:`mitsuba.Scene`):
            *no description available*

        Parameter ``sensor`` (:py:obj:`mitsuba.Sensor`):
            *no description available*

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.Integrator.should_stop()

        Indicates whether cancel() or a timeout have occurred. Should be
        checked regularly in the integrator's main loop so that timeouts are
        enforced accurately.

        Note that accurate timeouts rely on m_render_timer, which needs to be
        reset at the beginning of the rendering phase.

        Returns → bool:
            *no description available*

.. py:class:: mitsuba.Interaction3f

    Generic surface interaction data structure

    .. py:method:: __init__()

        Overloaded function.
        
        1. ``__init__(self) -> None``
        
        Constructor
        
        2. ``__init__(self, arg: :py:obj:`mitsuba.Interaction3f`) -> None``
        
        Copy constructor
        
        3. ``__init__(self, t: drjit.llvm.ad.Float, time: drjit.llvm.ad.Float, wavelengths: :py:obj:`mitsuba.Color0f`, p: :py:obj:`mitsuba.Point3f`, n: :py:obj:`mitsuba.Normal3f` = 0) -> None``
        
        //! @}

        
    .. py:method:: mitsuba.Interaction3f.assign(self, arg)

        Parameter ``arg`` (:py:obj:`mitsuba.Interaction3f`, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Interaction3f.is_valid()

        Is the current interaction valid?

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.Interaction3f.n
        :property:

        Geometric normal (only valid for ``SurfaceInteraction``)

    .. py:method:: mitsuba.Interaction3f.p
        :property:

        Position of the interaction in world coordinates

    .. py:method:: mitsuba.Interaction3f.spawn_ray(self, d)

        Spawn a semi-infinite ray towards the given direction

        Parameter ``d`` (:py:obj:`mitsuba.Vector3f`):
            *no description available*

        Returns → :py:obj:`mitsuba.Ray3f`:
            *no description available*

    .. py:method:: mitsuba.Interaction3f.spawn_ray_to(self, t)

        Spawn a finite ray towards the given position

        Parameter ``t`` (:py:obj:`mitsuba.Point3f`):
            *no description available*

        Returns → :py:obj:`mitsuba.Ray3f`:
            *no description available*

    .. py:method:: mitsuba.Interaction3f.t
        :property:

        Distance traveled along the ray

    .. py:method:: mitsuba.Interaction3f.time
        :property:

        Time value associated with the interaction

    .. py:method:: mitsuba.Interaction3f.wavelengths
        :property:

        Wavelengths associated with the ray that produced this interaction

    .. py:method:: mitsuba.Interaction3f.zero_(self, size=1)

        Overloaded function.

        1. ``zero_(self, size: int = 1) -> None``


        2. ``zero_(self, arg: int, /) -> None``

        This callback method is invoked by dr::zeros<>, and takes care of
        fields that deviate from the standard zero-initialization convention.
        In this particular class, the ``t`` field should be set to an infinite
        value to mark invalid intersection records.

        Parameter ``size`` (int):
            *no description available*

        Returns → None:
            *no description available*

.. py:class:: mitsuba.IrregularContinuousDistribution

    Continuous 1D probability distribution defined in terms of an
    *irregularly* sampled linear interpolant

    This data structure represents a continuous 1D probability
    distribution that is defined as a linear interpolant of an irregularly
    discretized signal. The class provides various routines for
    transforming uniformly distributed samples so that they follow the
    stored distribution. Note that unnormalized probability density
    functions (PDFs) will automatically be normalized during
    initialization. The associated scale factor can be retrieved using the
    function normalization().

    .. py:method:: __init__()

        Overloaded function.
        
        1. ``__init__(self) -> None``
        
        Continuous 1D probability distribution defined in terms of an
        *irregularly* sampled linear interpolant
        
        This data structure represents a continuous 1D probability
        distribution that is defined as a linear interpolant of an irregularly
        discretized signal. The class provides various routines for
        transforming uniformly distributed samples so that they follow the
        stored distribution. Note that unnormalized probability density
        functions (PDFs) will automatically be normalized during
        initialization. The associated scale factor can be retrieved using the
        function normalization().
        
        2. ``__init__(self, arg: :py:obj:`mitsuba.IrregularContinuousDistribution`) -> None``
        
        Copy constructor
        
        3. ``__init__(self, nodes: drjit.llvm.ad.Float, pdf: drjit.llvm.ad.Float) -> None``
        
        Initialize from a given density function discretized on nodes
        ``nodes``

        
    .. py:method:: mitsuba.IrregularContinuousDistribution.cdf
        :property:

        Return the nodes of the underlying discretization

    .. py:method:: mitsuba.IrregularContinuousDistribution.empty()

        Is the distribution object empty/uninitialized?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.IrregularContinuousDistribution.eval_cdf(self, x, active=True)

        Evaluate the unnormalized cumulative distribution function (CDF) at
        position ``p``

        Parameter ``x`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.IrregularContinuousDistribution.eval_cdf_normalized(self, x, active=True)

        Evaluate the unnormalized cumulative distribution function (CDF) at
        position ``p``

        Parameter ``x`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.IrregularContinuousDistribution.eval_pdf(self, x, active=True)

        Evaluate the unnormalized probability mass function (PDF) at position
        ``x``

        Parameter ``x`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.IrregularContinuousDistribution.eval_pdf_normalized(self, x, active=True)

        Evaluate the normalized probability mass function (PDF) at position
        ``x``

        Parameter ``x`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.IrregularContinuousDistribution.integral()

        Return the original integral of PDF entries before normalization

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.IrregularContinuousDistribution.interval_resolution()

        Return the minimum resolution of the discretization

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.IrregularContinuousDistribution.max()

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.IrregularContinuousDistribution.nodes
        :property:

        Return the nodes of the underlying discretization

    .. py:method:: mitsuba.IrregularContinuousDistribution.normalization()

        Return the normalization factor (i.e. the inverse of sum())

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.IrregularContinuousDistribution.pdf
        :property:

        Return the nodes of the underlying discretization

    .. py:method:: mitsuba.IrregularContinuousDistribution.range
        :property:

        Return the range of the distribution

    .. py:method:: mitsuba.IrregularContinuousDistribution.sample(self, value, active=True)

        %Transform a uniformly distributed sample to the stored distribution

        Parameter ``sample``:
            A uniformly distributed sample on the interval [0, 1].

        Parameter ``value`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            The sampled position.

    .. py:method:: mitsuba.IrregularContinuousDistribution.sample_pdf(self, value, active=True)

        %Transform a uniformly distributed sample to the stored distribution

        Parameter ``sample``:
            A uniformly distributed sample on the interval [0, 1].

        Parameter ``value`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[drjit.llvm.ad.Float, drjit.llvm.ad.Float]:
            A tuple consisting of

        1. the sampled position. 2. the normalized probability density of the
        sample.

    .. py:method:: mitsuba.IrregularContinuousDistribution.size()

        Return the number of discretizations

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.IrregularContinuousDistribution.update()

        Update the internal state. Must be invoked when changing the pdf or
        range.

        Returns → None:
            *no description available*

.. py:function:: mitsuba.Log(level, msg)

    Parameter ``level`` (:py:obj:`mitsuba.LogLevel`):
        *no description available*

    Parameter ``msg`` (str):
        *no description available*

    Returns → None:
        *no description available*

.. py:class:: mitsuba.LogLevel

    Available Log message types

    Valid values are as follows:

    .. py:data:: Trace

        

    .. py:data:: Debug

        Trace message, for extremely verbose debugging

    .. py:data:: Info

        Debug message, usually turned off

    .. py:data:: Warn

        More relevant debug / information message

    .. py:data:: Error

        Warning message

.. py:class:: mitsuba.Logger

    Base class: :py:obj:`mitsuba.Object`

    Responsible for processing log messages

    Upon receiving a log message, the Logger class invokes a Formatter to
    convert it into a human-readable form. Following that, it sends this
    information to every registered Appender.

    .. py:method:: __init__(self, arg)

        Construct a new logger with the given minimum log level

        Parameter ``arg`` (:py:obj:`mitsuba.LogLevel`, /):
            *no description available*

        
    .. py:method:: mitsuba.Logger.add_appender(self, arg)

        Add an appender to this logger

        Parameter ``arg`` (:py:obj:`mitsuba.Appender`, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Logger.appender(self, arg)

        Return one of the appenders

        Parameter ``arg`` (int, /):
            *no description available*

        Returns → :py:obj:`mitsuba.Appender`:
            *no description available*

    .. py:method:: mitsuba.Logger.appender_count()

        Return the number of registered appenders

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Logger.clear_appenders()

        Remove all appenders from this logger

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Logger.error_level()

        Return the current error level

        Returns → :py:obj:`mitsuba.LogLevel`:
            *no description available*

    .. py:method:: mitsuba.Logger.formatter()

        Return the logger's formatter implementation

        Returns → :py:obj:`mitsuba.Formatter`:
            *no description available*

    .. py:method:: mitsuba.Logger.log_level()

        Return the current log level

        Returns → :py:obj:`mitsuba.LogLevel`:
            *no description available*

    .. py:method:: mitsuba.Logger.log_progress(self, progress, name, formatted, eta, ptr=None)

        Process a progress message

        Parameter ``progress`` (float):
            Percentage value in [0, 100]

        Parameter ``name`` (str):
            Title of the progress message

        Parameter ``formatted`` (str):
            Formatted string representation of the message

        Parameter ``eta`` (str):
            Estimated time until 100% is reached.

        Parameter ``ptr`` (types.CapsuleType | None):
            Custom pointer payload. This is used to express the context of a
            progress message. When rendering a scene, it will usually contain
            a pointer to the associated ``RenderJob``.

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Logger.read_log()

        Return the contents of the log file as a string

        Throws a runtime exception upon failure

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.Logger.remove_appender(self, arg)

        Remove an appender from this logger

        Parameter ``arg`` (:py:obj:`mitsuba.Appender`, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Logger.set_error_level(self, arg)

        Set the error log level (this level and anything above will throw
        exceptions).

        The value provided here can be used for instance to turn warnings into
        errors. But *level* must always be less than Error, i.e. it isn't
        possible to cause errors not to throw an exception.

        Parameter ``arg`` (:py:obj:`mitsuba.LogLevel`, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Logger.set_formatter(self, arg)

        Set the logger's formatter implementation

        Parameter ``arg`` (:py:obj:`mitsuba.Formatter`, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Logger.set_log_level(self, arg)

        Set the log level (everything below will be ignored)

        Parameter ``arg`` (:py:obj:`mitsuba.LogLevel`, /):
            *no description available*

        Returns → None:
            *no description available*

.. py:data:: mitsuba.MI_AUTHORS
    :type: str
    :value: Realistic Graphics Lab, EPFL

.. py:data:: mitsuba.MI_CIE_D65_NORMALIZATION
    :type: float
    :value: 0.010101273599490354

.. py:data:: mitsuba.MI_CIE_MAX
    :type: float
    :value: 830.0

.. py:data:: mitsuba.MI_CIE_MIN
    :type: float
    :value: 360.0

.. py:data:: mitsuba.MI_CIE_Y_NORMALIZATION
    :type: float
    :value: 0.009367658735689113

.. py:data:: mitsuba.MI_ENABLE_CUDA
    :type: bool
    :value: True

.. py:data:: mitsuba.MI_ENABLE_EMBREE
    :type: bool
    :value: True

.. py:data:: mitsuba.MI_FILTER_RESOLUTION
    :type: int
    :value: 31

.. py:data:: mitsuba.MI_VERSION
    :type: str
    :value: 3.6.0

.. py:data:: mitsuba.MI_VERSION_MAJOR
    :type: int
    :value: 3

.. py:data:: mitsuba.MI_VERSION_MINOR
    :type: int
    :value: 6

.. py:data:: mitsuba.MI_VERSION_PATCH
    :type: int
    :value: 0

.. py:data:: mitsuba.MI_YEAR
    :type: str
    :value: 2024

.. py:class:: mitsuba.MarginalContinuous2D0

    Implements a marginal sample warping scheme for 2D distributions with
    linear interpolation and an optional dependence on additional
    parameters

    This class takes a rectangular floating point array as input and
    constructs internal data structures to efficiently map uniform
    variates from the unit square ``[0, 1]^2`` to a function on ``[0,
    1]^2`` that linearly interpolates the input array.

    The mapping is constructed via the inversion method, which is applied
    to a marginal distribution over rows, followed by a conditional
    distribution over columns.

    The implementation also supports *conditional distributions*, i.e. 2D
    distributions that depend on an arbitrary number of parameters
    (indicated via the ``Dimension`` template parameter).

    In this case, the input array should have dimensions ``N0 x N1 x ... x
    Nn x res.y() x res.x()`` (where the last dimension is contiguous in
    memory), and the ``param_res`` should be set to ``{ N0, N1, ..., Nn
    }``, and ``param_values`` should contain the parameter values where
    the distribution is discretized. Linear interpolation is used when
    sampling or evaluating the distribution for in-between parameter
    values.

    There are two variants of ``Marginal2D:`` when ``Continuous=false``,
    discrete marginal/conditional distributions are used to select a
    bilinear bilinear patch, followed by a continuous sampling step that
    chooses a specific position inside the patch. When
    ``Continuous=true``, continuous marginal/conditional distributions are
    used instead, and the second step is no longer needed. The latter
    scheme requires more computation and memory accesses but produces an
    overall smoother mapping.

    Remark:
        The Python API exposes explicitly instantiated versions of this
        class named ``MarginalDiscrete2D0`` to ``MarginalDiscrete2D3`` and
        ``MarginalContinuous2D0`` to ``MarginalContinuous2D3`` for data
        that depends on 0 to 3 parameters.

    .. py:method:: __init__(self, data, param_values=[], normalize=True, enable_sampling=True)

        Construct a marginal sample warping scheme for floating point data of
        resolution ``size``.
        
        ``param_res`` and ``param_values`` are only needed for conditional
        distributions (see the text describing the Marginal2D class).
        
        If ``normalize`` is set to ``False``, the implementation will not re-
        scale the distribution so that it integrates to ``1``. It can still be
        sampled (proportionally), but returned density values will reflect the
        unnormalized values.
        
        If ``enable_sampling`` is set to ``False``, the implementation will
        not construct the cdf needed for sample warping, which saves memory in
        case this functionality is not needed (e.g. if only the interpolation
        in ``eval()`` is used).

        Parameter ``data`` (ndarray[dtype=float32, shape=(*, *), order='C']):
            *no description available*

        Parameter ``param_values`` (collections.abc.Sequence[collections.abc.Sequence[float]]):
            *no description available*

        Parameter ``normalize`` (bool):
            *no description available*

        Parameter ``enable_sampling`` (bool):
            *no description available*

        
    .. py:method:: mitsuba.MarginalContinuous2D0.eval(self, pos, param=[], active=True)

        Evaluate the density at position ``pos``. The distribution is
        parameterized by ``param`` if applicable.

        Parameter ``pos`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array0f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.MarginalContinuous2D0.invert(self, sample, param=[], active=True)

        Inverse of the mapping implemented in ``sample()``

        Parameter ``sample`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array0f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.MarginalContinuous2D0.sample(self, sample, param=[], active=True)

        Given a uniformly distributed 2D sample, draw a sample from the
        distribution (parameterized by ``param`` if applicable)

        Returns the warped sample and associated probability density.

        Parameter ``sample`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array0f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
            *no description available*

.. py:class:: mitsuba.MarginalContinuous2D1

    Implements a marginal sample warping scheme for 2D distributions with
    linear interpolation and an optional dependence on additional
    parameters

    This class takes a rectangular floating point array as input and
    constructs internal data structures to efficiently map uniform
    variates from the unit square ``[0, 1]^2`` to a function on ``[0,
    1]^2`` that linearly interpolates the input array.

    The mapping is constructed via the inversion method, which is applied
    to a marginal distribution over rows, followed by a conditional
    distribution over columns.

    The implementation also supports *conditional distributions*, i.e. 2D
    distributions that depend on an arbitrary number of parameters
    (indicated via the ``Dimension`` template parameter).

    In this case, the input array should have dimensions ``N0 x N1 x ... x
    Nn x res.y() x res.x()`` (where the last dimension is contiguous in
    memory), and the ``param_res`` should be set to ``{ N0, N1, ..., Nn
    }``, and ``param_values`` should contain the parameter values where
    the distribution is discretized. Linear interpolation is used when
    sampling or evaluating the distribution for in-between parameter
    values.

    There are two variants of ``Marginal2D:`` when ``Continuous=false``,
    discrete marginal/conditional distributions are used to select a
    bilinear bilinear patch, followed by a continuous sampling step that
    chooses a specific position inside the patch. When
    ``Continuous=true``, continuous marginal/conditional distributions are
    used instead, and the second step is no longer needed. The latter
    scheme requires more computation and memory accesses but produces an
    overall smoother mapping.

    Remark:
        The Python API exposes explicitly instantiated versions of this
        class named ``MarginalDiscrete2D0`` to ``MarginalDiscrete2D3`` and
        ``MarginalContinuous2D0`` to ``MarginalContinuous2D3`` for data
        that depends on 0 to 3 parameters.

    .. py:method:: __init__(self, data, param_values, normalize=True, build_hierarchy=True)

        Construct a marginal sample warping scheme for floating point data of
        resolution ``size``.
        
        ``param_res`` and ``param_values`` are only needed for conditional
        distributions (see the text describing the Marginal2D class).
        
        If ``normalize`` is set to ``False``, the implementation will not re-
        scale the distribution so that it integrates to ``1``. It can still be
        sampled (proportionally), but returned density values will reflect the
        unnormalized values.
        
        If ``enable_sampling`` is set to ``False``, the implementation will
        not construct the cdf needed for sample warping, which saves memory in
        case this functionality is not needed (e.g. if only the interpolation
        in ``eval()`` is used).

        Parameter ``data`` (ndarray[dtype=float32, shape=(*, *, *), order='C']):
            *no description available*

        Parameter ``param_values`` (collections.abc.Sequence[collections.abc.Sequence[float]]):
            *no description available*

        Parameter ``normalize`` (bool):
            *no description available*

        Parameter ``build_hierarchy`` (bool):
            *no description available*

        
    .. py:method:: mitsuba.MarginalContinuous2D1.eval(self, pos, param=[0], active=True)

        Evaluate the density at position ``pos``. The distribution is
        parameterized by ``param`` if applicable.

        Parameter ``pos`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array1f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.MarginalContinuous2D1.invert(self, sample, param=[0], active=True)

        Inverse of the mapping implemented in ``sample()``

        Parameter ``sample`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array1f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.MarginalContinuous2D1.sample(self, sample, param=[0], active=True)

        Given a uniformly distributed 2D sample, draw a sample from the
        distribution (parameterized by ``param`` if applicable)

        Returns the warped sample and associated probability density.

        Parameter ``sample`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array1f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
            *no description available*

.. py:class:: mitsuba.MarginalContinuous2D2

    Implements a marginal sample warping scheme for 2D distributions with
    linear interpolation and an optional dependence on additional
    parameters

    This class takes a rectangular floating point array as input and
    constructs internal data structures to efficiently map uniform
    variates from the unit square ``[0, 1]^2`` to a function on ``[0,
    1]^2`` that linearly interpolates the input array.

    The mapping is constructed via the inversion method, which is applied
    to a marginal distribution over rows, followed by a conditional
    distribution over columns.

    The implementation also supports *conditional distributions*, i.e. 2D
    distributions that depend on an arbitrary number of parameters
    (indicated via the ``Dimension`` template parameter).

    In this case, the input array should have dimensions ``N0 x N1 x ... x
    Nn x res.y() x res.x()`` (where the last dimension is contiguous in
    memory), and the ``param_res`` should be set to ``{ N0, N1, ..., Nn
    }``, and ``param_values`` should contain the parameter values where
    the distribution is discretized. Linear interpolation is used when
    sampling or evaluating the distribution for in-between parameter
    values.

    There are two variants of ``Marginal2D:`` when ``Continuous=false``,
    discrete marginal/conditional distributions are used to select a
    bilinear bilinear patch, followed by a continuous sampling step that
    chooses a specific position inside the patch. When
    ``Continuous=true``, continuous marginal/conditional distributions are
    used instead, and the second step is no longer needed. The latter
    scheme requires more computation and memory accesses but produces an
    overall smoother mapping.

    Remark:
        The Python API exposes explicitly instantiated versions of this
        class named ``MarginalDiscrete2D0`` to ``MarginalDiscrete2D3`` and
        ``MarginalContinuous2D0`` to ``MarginalContinuous2D3`` for data
        that depends on 0 to 3 parameters.

    .. py:method:: __init__(self, data, param_values, normalize=True, build_hierarchy=True)

        Construct a marginal sample warping scheme for floating point data of
        resolution ``size``.
        
        ``param_res`` and ``param_values`` are only needed for conditional
        distributions (see the text describing the Marginal2D class).
        
        If ``normalize`` is set to ``False``, the implementation will not re-
        scale the distribution so that it integrates to ``1``. It can still be
        sampled (proportionally), but returned density values will reflect the
        unnormalized values.
        
        If ``enable_sampling`` is set to ``False``, the implementation will
        not construct the cdf needed for sample warping, which saves memory in
        case this functionality is not needed (e.g. if only the interpolation
        in ``eval()`` is used).

        Parameter ``data`` (ndarray[dtype=float32, shape=(*, *, *, *), order='C']):
            *no description available*

        Parameter ``param_values`` (collections.abc.Sequence[collections.abc.Sequence[float]]):
            *no description available*

        Parameter ``normalize`` (bool):
            *no description available*

        Parameter ``build_hierarchy`` (bool):
            *no description available*

        
    .. py:method:: mitsuba.MarginalContinuous2D2.eval(self, pos, param=[0, 0], active=True)

        Evaluate the density at position ``pos``. The distribution is
        parameterized by ``param`` if applicable.

        Parameter ``pos`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.MarginalContinuous2D2.invert(self, sample, param=[0, 0], active=True)

        Inverse of the mapping implemented in ``sample()``

        Parameter ``sample`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.MarginalContinuous2D2.sample(self, sample, param=[0, 0], active=True)

        Given a uniformly distributed 2D sample, draw a sample from the
        distribution (parameterized by ``param`` if applicable)

        Returns the warped sample and associated probability density.

        Parameter ``sample`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
            *no description available*

.. py:class:: mitsuba.MarginalContinuous2D3

    Implements a marginal sample warping scheme for 2D distributions with
    linear interpolation and an optional dependence on additional
    parameters

    This class takes a rectangular floating point array as input and
    constructs internal data structures to efficiently map uniform
    variates from the unit square ``[0, 1]^2`` to a function on ``[0,
    1]^2`` that linearly interpolates the input array.

    The mapping is constructed via the inversion method, which is applied
    to a marginal distribution over rows, followed by a conditional
    distribution over columns.

    The implementation also supports *conditional distributions*, i.e. 2D
    distributions that depend on an arbitrary number of parameters
    (indicated via the ``Dimension`` template parameter).

    In this case, the input array should have dimensions ``N0 x N1 x ... x
    Nn x res.y() x res.x()`` (where the last dimension is contiguous in
    memory), and the ``param_res`` should be set to ``{ N0, N1, ..., Nn
    }``, and ``param_values`` should contain the parameter values where
    the distribution is discretized. Linear interpolation is used when
    sampling or evaluating the distribution for in-between parameter
    values.

    There are two variants of ``Marginal2D:`` when ``Continuous=false``,
    discrete marginal/conditional distributions are used to select a
    bilinear bilinear patch, followed by a continuous sampling step that
    chooses a specific position inside the patch. When
    ``Continuous=true``, continuous marginal/conditional distributions are
    used instead, and the second step is no longer needed. The latter
    scheme requires more computation and memory accesses but produces an
    overall smoother mapping.

    Remark:
        The Python API exposes explicitly instantiated versions of this
        class named ``MarginalDiscrete2D0`` to ``MarginalDiscrete2D3`` and
        ``MarginalContinuous2D0`` to ``MarginalContinuous2D3`` for data
        that depends on 0 to 3 parameters.

    .. py:method:: __init__(self, data, param_values, normalize=True, build_hierarchy=True)

        Construct a marginal sample warping scheme for floating point data of
        resolution ``size``.
        
        ``param_res`` and ``param_values`` are only needed for conditional
        distributions (see the text describing the Marginal2D class).
        
        If ``normalize`` is set to ``False``, the implementation will not re-
        scale the distribution so that it integrates to ``1``. It can still be
        sampled (proportionally), but returned density values will reflect the
        unnormalized values.
        
        If ``enable_sampling`` is set to ``False``, the implementation will
        not construct the cdf needed for sample warping, which saves memory in
        case this functionality is not needed (e.g. if only the interpolation
        in ``eval()`` is used).

        Parameter ``data`` (ndarray[dtype=float32, shape=(*, *, *, *, *), order='C']):
            *no description available*

        Parameter ``param_values`` (collections.abc.Sequence[collections.abc.Sequence[float]]):
            *no description available*

        Parameter ``normalize`` (bool):
            *no description available*

        Parameter ``build_hierarchy`` (bool):
            *no description available*

        
    .. py:method:: mitsuba.MarginalContinuous2D3.eval(self, pos, param=[0, 0, 0], active=True)

        Evaluate the density at position ``pos``. The distribution is
        parameterized by ``param`` if applicable.

        Parameter ``pos`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array3f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.MarginalContinuous2D3.invert(self, sample, param=[0, 0, 0], active=True)

        Inverse of the mapping implemented in ``sample()``

        Parameter ``sample`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array3f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.MarginalContinuous2D3.sample(self, sample, param=[0, 0, 0], active=True)

        Given a uniformly distributed 2D sample, draw a sample from the
        distribution (parameterized by ``param`` if applicable)

        Returns the warped sample and associated probability density.

        Parameter ``sample`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array3f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
            *no description available*

.. py:class:: mitsuba.MarginalDiscrete2D0

    Implements a marginal sample warping scheme for 2D distributions with
    linear interpolation and an optional dependence on additional
    parameters

    This class takes a rectangular floating point array as input and
    constructs internal data structures to efficiently map uniform
    variates from the unit square ``[0, 1]^2`` to a function on ``[0,
    1]^2`` that linearly interpolates the input array.

    The mapping is constructed via the inversion method, which is applied
    to a marginal distribution over rows, followed by a conditional
    distribution over columns.

    The implementation also supports *conditional distributions*, i.e. 2D
    distributions that depend on an arbitrary number of parameters
    (indicated via the ``Dimension`` template parameter).

    In this case, the input array should have dimensions ``N0 x N1 x ... x
    Nn x res.y() x res.x()`` (where the last dimension is contiguous in
    memory), and the ``param_res`` should be set to ``{ N0, N1, ..., Nn
    }``, and ``param_values`` should contain the parameter values where
    the distribution is discretized. Linear interpolation is used when
    sampling or evaluating the distribution for in-between parameter
    values.

    There are two variants of ``Marginal2D:`` when ``Continuous=false``,
    discrete marginal/conditional distributions are used to select a
    bilinear bilinear patch, followed by a continuous sampling step that
    chooses a specific position inside the patch. When
    ``Continuous=true``, continuous marginal/conditional distributions are
    used instead, and the second step is no longer needed. The latter
    scheme requires more computation and memory accesses but produces an
    overall smoother mapping.

    Remark:
        The Python API exposes explicitly instantiated versions of this
        class named ``MarginalDiscrete2D0`` to ``MarginalDiscrete2D3`` and
        ``MarginalContinuous2D0`` to ``MarginalContinuous2D3`` for data
        that depends on 0 to 3 parameters.

    .. py:method:: __init__(self, data, param_values=[], normalize=True, enable_sampling=True)

        Construct a marginal sample warping scheme for floating point data of
        resolution ``size``.
        
        ``param_res`` and ``param_values`` are only needed for conditional
        distributions (see the text describing the Marginal2D class).
        
        If ``normalize`` is set to ``False``, the implementation will not re-
        scale the distribution so that it integrates to ``1``. It can still be
        sampled (proportionally), but returned density values will reflect the
        unnormalized values.
        
        If ``enable_sampling`` is set to ``False``, the implementation will
        not construct the cdf needed for sample warping, which saves memory in
        case this functionality is not needed (e.g. if only the interpolation
        in ``eval()`` is used).

        Parameter ``data`` (ndarray[dtype=float32, shape=(*, *), order='C']):
            *no description available*

        Parameter ``param_values`` (collections.abc.Sequence[collections.abc.Sequence[float]]):
            *no description available*

        Parameter ``normalize`` (bool):
            *no description available*

        Parameter ``enable_sampling`` (bool):
            *no description available*

        
    .. py:method:: mitsuba.MarginalDiscrete2D0.eval(self, pos, param=[], active=True)

        Evaluate the density at position ``pos``. The distribution is
        parameterized by ``param`` if applicable.

        Parameter ``pos`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array0f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.MarginalDiscrete2D0.invert(self, sample, param=[], active=True)

        Inverse of the mapping implemented in ``sample()``

        Parameter ``sample`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array0f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.MarginalDiscrete2D0.sample(self, sample, param=[], active=True)

        Given a uniformly distributed 2D sample, draw a sample from the
        distribution (parameterized by ``param`` if applicable)

        Returns the warped sample and associated probability density.

        Parameter ``sample`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array0f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
            *no description available*

.. py:class:: mitsuba.MarginalDiscrete2D1

    Implements a marginal sample warping scheme for 2D distributions with
    linear interpolation and an optional dependence on additional
    parameters

    This class takes a rectangular floating point array as input and
    constructs internal data structures to efficiently map uniform
    variates from the unit square ``[0, 1]^2`` to a function on ``[0,
    1]^2`` that linearly interpolates the input array.

    The mapping is constructed via the inversion method, which is applied
    to a marginal distribution over rows, followed by a conditional
    distribution over columns.

    The implementation also supports *conditional distributions*, i.e. 2D
    distributions that depend on an arbitrary number of parameters
    (indicated via the ``Dimension`` template parameter).

    In this case, the input array should have dimensions ``N0 x N1 x ... x
    Nn x res.y() x res.x()`` (where the last dimension is contiguous in
    memory), and the ``param_res`` should be set to ``{ N0, N1, ..., Nn
    }``, and ``param_values`` should contain the parameter values where
    the distribution is discretized. Linear interpolation is used when
    sampling or evaluating the distribution for in-between parameter
    values.

    There are two variants of ``Marginal2D:`` when ``Continuous=false``,
    discrete marginal/conditional distributions are used to select a
    bilinear bilinear patch, followed by a continuous sampling step that
    chooses a specific position inside the patch. When
    ``Continuous=true``, continuous marginal/conditional distributions are
    used instead, and the second step is no longer needed. The latter
    scheme requires more computation and memory accesses but produces an
    overall smoother mapping.

    Remark:
        The Python API exposes explicitly instantiated versions of this
        class named ``MarginalDiscrete2D0`` to ``MarginalDiscrete2D3`` and
        ``MarginalContinuous2D0`` to ``MarginalContinuous2D3`` for data
        that depends on 0 to 3 parameters.

    .. py:method:: __init__(self, data, param_values, normalize=True, build_hierarchy=True)

        Construct a marginal sample warping scheme for floating point data of
        resolution ``size``.
        
        ``param_res`` and ``param_values`` are only needed for conditional
        distributions (see the text describing the Marginal2D class).
        
        If ``normalize`` is set to ``False``, the implementation will not re-
        scale the distribution so that it integrates to ``1``. It can still be
        sampled (proportionally), but returned density values will reflect the
        unnormalized values.
        
        If ``enable_sampling`` is set to ``False``, the implementation will
        not construct the cdf needed for sample warping, which saves memory in
        case this functionality is not needed (e.g. if only the interpolation
        in ``eval()`` is used).

        Parameter ``data`` (ndarray[dtype=float32, shape=(*, *, *), order='C']):
            *no description available*

        Parameter ``param_values`` (collections.abc.Sequence[collections.abc.Sequence[float]]):
            *no description available*

        Parameter ``normalize`` (bool):
            *no description available*

        Parameter ``build_hierarchy`` (bool):
            *no description available*

        
    .. py:method:: mitsuba.MarginalDiscrete2D1.eval(self, pos, param=[0], active=True)

        Evaluate the density at position ``pos``. The distribution is
        parameterized by ``param`` if applicable.

        Parameter ``pos`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array1f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.MarginalDiscrete2D1.invert(self, sample, param=[0], active=True)

        Inverse of the mapping implemented in ``sample()``

        Parameter ``sample`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array1f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.MarginalDiscrete2D1.sample(self, sample, param=[0], active=True)

        Given a uniformly distributed 2D sample, draw a sample from the
        distribution (parameterized by ``param`` if applicable)

        Returns the warped sample and associated probability density.

        Parameter ``sample`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array1f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
            *no description available*

.. py:class:: mitsuba.MarginalDiscrete2D2

    Implements a marginal sample warping scheme for 2D distributions with
    linear interpolation and an optional dependence on additional
    parameters

    This class takes a rectangular floating point array as input and
    constructs internal data structures to efficiently map uniform
    variates from the unit square ``[0, 1]^2`` to a function on ``[0,
    1]^2`` that linearly interpolates the input array.

    The mapping is constructed via the inversion method, which is applied
    to a marginal distribution over rows, followed by a conditional
    distribution over columns.

    The implementation also supports *conditional distributions*, i.e. 2D
    distributions that depend on an arbitrary number of parameters
    (indicated via the ``Dimension`` template parameter).

    In this case, the input array should have dimensions ``N0 x N1 x ... x
    Nn x res.y() x res.x()`` (where the last dimension is contiguous in
    memory), and the ``param_res`` should be set to ``{ N0, N1, ..., Nn
    }``, and ``param_values`` should contain the parameter values where
    the distribution is discretized. Linear interpolation is used when
    sampling or evaluating the distribution for in-between parameter
    values.

    There are two variants of ``Marginal2D:`` when ``Continuous=false``,
    discrete marginal/conditional distributions are used to select a
    bilinear bilinear patch, followed by a continuous sampling step that
    chooses a specific position inside the patch. When
    ``Continuous=true``, continuous marginal/conditional distributions are
    used instead, and the second step is no longer needed. The latter
    scheme requires more computation and memory accesses but produces an
    overall smoother mapping.

    Remark:
        The Python API exposes explicitly instantiated versions of this
        class named ``MarginalDiscrete2D0`` to ``MarginalDiscrete2D3`` and
        ``MarginalContinuous2D0`` to ``MarginalContinuous2D3`` for data
        that depends on 0 to 3 parameters.

    .. py:method:: __init__(self, data, param_values, normalize=True, build_hierarchy=True)

        Construct a marginal sample warping scheme for floating point data of
        resolution ``size``.
        
        ``param_res`` and ``param_values`` are only needed for conditional
        distributions (see the text describing the Marginal2D class).
        
        If ``normalize`` is set to ``False``, the implementation will not re-
        scale the distribution so that it integrates to ``1``. It can still be
        sampled (proportionally), but returned density values will reflect the
        unnormalized values.
        
        If ``enable_sampling`` is set to ``False``, the implementation will
        not construct the cdf needed for sample warping, which saves memory in
        case this functionality is not needed (e.g. if only the interpolation
        in ``eval()`` is used).

        Parameter ``data`` (ndarray[dtype=float32, shape=(*, *, *, *), order='C']):
            *no description available*

        Parameter ``param_values`` (collections.abc.Sequence[collections.abc.Sequence[float]]):
            *no description available*

        Parameter ``normalize`` (bool):
            *no description available*

        Parameter ``build_hierarchy`` (bool):
            *no description available*

        
    .. py:method:: mitsuba.MarginalDiscrete2D2.eval(self, pos, param=[0, 0], active=True)

        Evaluate the density at position ``pos``. The distribution is
        parameterized by ``param`` if applicable.

        Parameter ``pos`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.MarginalDiscrete2D2.invert(self, sample, param=[0, 0], active=True)

        Inverse of the mapping implemented in ``sample()``

        Parameter ``sample`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.MarginalDiscrete2D2.sample(self, sample, param=[0, 0], active=True)

        Given a uniformly distributed 2D sample, draw a sample from the
        distribution (parameterized by ``param`` if applicable)

        Returns the warped sample and associated probability density.

        Parameter ``sample`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
            *no description available*

.. py:class:: mitsuba.MarginalDiscrete2D3

    Implements a marginal sample warping scheme for 2D distributions with
    linear interpolation and an optional dependence on additional
    parameters

    This class takes a rectangular floating point array as input and
    constructs internal data structures to efficiently map uniform
    variates from the unit square ``[0, 1]^2`` to a function on ``[0,
    1]^2`` that linearly interpolates the input array.

    The mapping is constructed via the inversion method, which is applied
    to a marginal distribution over rows, followed by a conditional
    distribution over columns.

    The implementation also supports *conditional distributions*, i.e. 2D
    distributions that depend on an arbitrary number of parameters
    (indicated via the ``Dimension`` template parameter).

    In this case, the input array should have dimensions ``N0 x N1 x ... x
    Nn x res.y() x res.x()`` (where the last dimension is contiguous in
    memory), and the ``param_res`` should be set to ``{ N0, N1, ..., Nn
    }``, and ``param_values`` should contain the parameter values where
    the distribution is discretized. Linear interpolation is used when
    sampling or evaluating the distribution for in-between parameter
    values.

    There are two variants of ``Marginal2D:`` when ``Continuous=false``,
    discrete marginal/conditional distributions are used to select a
    bilinear bilinear patch, followed by a continuous sampling step that
    chooses a specific position inside the patch. When
    ``Continuous=true``, continuous marginal/conditional distributions are
    used instead, and the second step is no longer needed. The latter
    scheme requires more computation and memory accesses but produces an
    overall smoother mapping.

    Remark:
        The Python API exposes explicitly instantiated versions of this
        class named ``MarginalDiscrete2D0`` to ``MarginalDiscrete2D3`` and
        ``MarginalContinuous2D0`` to ``MarginalContinuous2D3`` for data
        that depends on 0 to 3 parameters.

    .. py:method:: __init__(self, data, param_values, normalize=True, build_hierarchy=True)

        Construct a marginal sample warping scheme for floating point data of
        resolution ``size``.
        
        ``param_res`` and ``param_values`` are only needed for conditional
        distributions (see the text describing the Marginal2D class).
        
        If ``normalize`` is set to ``False``, the implementation will not re-
        scale the distribution so that it integrates to ``1``. It can still be
        sampled (proportionally), but returned density values will reflect the
        unnormalized values.
        
        If ``enable_sampling`` is set to ``False``, the implementation will
        not construct the cdf needed for sample warping, which saves memory in
        case this functionality is not needed (e.g. if only the interpolation
        in ``eval()`` is used).

        Parameter ``data`` (ndarray[dtype=float32, shape=(*, *, *, *, *), order='C']):
            *no description available*

        Parameter ``param_values`` (collections.abc.Sequence[collections.abc.Sequence[float]]):
            *no description available*

        Parameter ``normalize`` (bool):
            *no description available*

        Parameter ``build_hierarchy`` (bool):
            *no description available*

        
    .. py:method:: mitsuba.MarginalDiscrete2D3.eval(self, pos, param=[0, 0, 0], active=True)

        Evaluate the density at position ``pos``. The distribution is
        parameterized by ``param`` if applicable.

        Parameter ``pos`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array3f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.MarginalDiscrete2D3.invert(self, sample, param=[0, 0, 0], active=True)

        Inverse of the mapping implemented in ``sample()``

        Parameter ``sample`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array3f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.MarginalDiscrete2D3.sample(self, sample, param=[0, 0, 0], active=True)

        Given a uniformly distributed 2D sample, draw a sample from the
        distribution (parameterized by ``param`` if applicable)

        Returns the warped sample and associated probability density.

        Parameter ``sample`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``param`` (drjit.llvm.ad.Array3f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
            *no description available*

.. py:class:: mitsuba.Matrix2f

.. py:class:: mitsuba.Matrix3f

.. py:class:: mitsuba.Matrix4f

.. py:class:: mitsuba.Medium

    Base class: :py:obj:`mitsuba.Object`

    .. py:method:: mitsuba.Medium.get_majorant(self, mi, active=True)

        Returns the medium's majorant used for delta tracking

        Parameter ``mi`` (:py:obj:`mitsuba.MediumInteraction3f`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Color3f`:
            *no description available*

    .. py:method:: mitsuba.Medium.get_scattering_coefficients(self, mi, active=True)

        Returns the medium coefficients Sigma_s, Sigma_n and Sigma_t evaluated
        at a given MediumInteraction mi

        Parameter ``mi`` (:py:obj:`mitsuba.MediumInteraction3f`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Color3f`, :py:obj:`mitsuba.Color3f`, :py:obj:`mitsuba.Color3f`]:
            *no description available*

    .. py:method:: mitsuba.Medium.has_spectral_extinction()

        Returns whether this medium has a spectrally varying extinction

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Medium.id()

        Return a string identifier

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.Medium.intersect_aabb(self, ray)

        Intersects a ray with the medium's bounding box

        Parameter ``ray`` (:py:obj:`mitsuba.Ray3f`):
            *no description available*

        Returns → tuple[drjit.llvm.ad.Bool, drjit.llvm.ad.Float, drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.Medium.is_homogeneous()

        Returns whether this medium is homogeneous

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Medium.m_has_spectral_extinction
        :property:

        (self) -> bool

    .. py:method:: mitsuba.Medium.m_is_homogeneous
        :property:

        (self) -> bool

    .. py:method:: mitsuba.Medium.m_sample_emitters
        :property:

        (self) -> bool

    .. py:method:: mitsuba.Medium.phase_function()

        Return the phase function of this medium

        Returns → :py:obj:`mitsuba.PhaseFunction`:
            *no description available*

    .. py:method:: mitsuba.Medium.sample_interaction(self, ray, sample, channel, active)

        Sample a free-flight distance in the medium.

        This function samples a (tentative) free-flight distance according to
        an exponential transmittance. It is then up to the integrator to then
        decide whether the MediumInteraction corresponds to a real or null
        scattering event.

        Parameter ``ray`` (:py:obj:`mitsuba.Ray3f`):
            Ray, along which a distance should be sampled

        Parameter ``sample`` (drjit.llvm.ad.Float):
            A uniformly distributed random sample

        Parameter ``channel`` (drjit.llvm.ad.UInt):
            The channel according to which we will sample the free-flight
            distance. This argument is only used when rendering in RGB modes.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.MediumInteraction3f`:
            This method returns a MediumInteraction. The MediumInteraction
            will always be valid, except if the ray missed the Medium's
            bounding box.

    .. py:method:: mitsuba.Medium.set_id(self, arg)

        Set a string identifier

        Parameter ``arg`` (str, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Medium.transmittance_eval_pdf(self, mi, si, active)

        Compute the transmittance and PDF

        This function evaluates the transmittance and PDF of sampling a
        certain free-flight distance The returned PDF takes into account if a
        medium interaction occurred (mi.t <= si.t) or the ray left the medium
        (mi.t > si.t)

        The evaluated PDF is spectrally varying. This allows to account for
        the fact that the free-flight distance sampling distribution can
        depend on the wavelength.

        Parameter ``mi`` (:py:obj:`mitsuba.MediumInteraction3f`):
            *no description available*

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Color3f`, :py:obj:`mitsuba.Color3f`]:
            This method returns a pair of (Transmittance, PDF).

    .. py:method:: mitsuba.Medium.use_emitter_sampling()

        Returns whether this specific medium instance uses emitter sampling

        Returns → bool:
            *no description available*

.. py:class:: mitsuba.MediumInteraction3f

    Base class: :py:obj:`mitsuba.Interaction3f`

    Stores information related to a medium scattering interaction

    .. py:method:: __init__()

        Overloaded function.
        
        1. ``__init__(self) -> None``
        
        //! @}
        
        2. ``__init__(self, arg: :py:obj:`mitsuba.MediumInteraction3f`) -> None``
        
        Copy constructor

        
    .. py:method:: mitsuba.MediumInteraction3f.assign(self, arg)

        Parameter ``arg`` (:py:obj:`mitsuba.MediumInteraction3f`, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.MediumInteraction3f.combined_extinction
        :property:

        (self) -> :py:obj:`mitsuba.Color3f`

    .. py:method:: mitsuba.MediumInteraction3f.medium
        :property:

        Pointer to the associated medium

    .. py:method:: mitsuba.MediumInteraction3f.mint
        :property:

        mint used when sampling the given distance ``t``

    .. py:method:: mitsuba.MediumInteraction3f.sh_frame
        :property:

        Shading frame

    .. py:method:: mitsuba.MediumInteraction3f.sigma_n
        :property:

        (self) -> :py:obj:`mitsuba.Color3f`

    .. py:method:: mitsuba.MediumInteraction3f.sigma_s
        :property:

        (self) -> :py:obj:`mitsuba.Color3f`

    .. py:method:: mitsuba.MediumInteraction3f.sigma_t
        :property:

        (self) -> :py:obj:`mitsuba.Color3f`

    .. py:method:: mitsuba.MediumInteraction3f.to_local(self, v)

        Convert a world-space vector into local shading coordinates (defined
        by ``wi``)

        Parameter ``v`` (:py:obj:`mitsuba.Vector3f`):
            *no description available*

        Returns → :py:obj:`mitsuba.Vector3f`:
            *no description available*

    .. py:method:: mitsuba.MediumInteraction3f.to_world(self, v)

        Convert a local shading-space (defined by ``wi``) vector into world
        space

        Parameter ``v`` (:py:obj:`mitsuba.Vector3f`):
            *no description available*

        Returns → :py:obj:`mitsuba.Vector3f`:
            *no description available*

    .. py:method:: mitsuba.MediumInteraction3f.wi
        :property:

        Incident direction in world frame

.. py:class:: mitsuba.MediumPtr

    .. py:method:: mitsuba.MediumPtr.get_majorant(self, mi, active=True)

        Returns the medium's majorant used for delta tracking

        Parameter ``mi`` (:py:obj:`mitsuba.MediumInteraction3f`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Color3f`:
            *no description available*

    .. py:method:: mitsuba.MediumPtr.get_scattering_coefficients(self, mi, active=True)

        Returns the medium coefficients Sigma_s, Sigma_n and Sigma_t evaluated
        at a given MediumInteraction mi

        Parameter ``mi`` (:py:obj:`mitsuba.MediumInteraction3f`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Color3f`, :py:obj:`mitsuba.Color3f`, :py:obj:`mitsuba.Color3f`]:
            *no description available*

    .. py:method:: mitsuba.MediumPtr.has_spectral_extinction()

        Returns whether this medium has a spectrally varying extinction

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.MediumPtr.intersect_aabb(self, ray)

        Intersects a ray with the medium's bounding box

        Parameter ``ray`` (:py:obj:`mitsuba.Ray3f`):
            *no description available*

        Returns → tuple[drjit.llvm.ad.Bool, drjit.llvm.ad.Float, drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.MediumPtr.is_homogeneous()

        Returns whether this medium is homogeneous

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.MediumPtr.phase_function()

        Return the phase function of this medium

        Returns → :py:obj:`mitsuba.PhaseFunctionPtr`:
            *no description available*

    .. py:method:: mitsuba.MediumPtr.sample_interaction(self, ray, sample, channel, active)

        Sample a free-flight distance in the medium.

        This function samples a (tentative) free-flight distance according to
        an exponential transmittance. It is then up to the integrator to then
        decide whether the MediumInteraction corresponds to a real or null
        scattering event.

        Parameter ``ray`` (:py:obj:`mitsuba.Ray3f`):
            Ray, along which a distance should be sampled

        Parameter ``sample`` (drjit.llvm.ad.Float):
            A uniformly distributed random sample

        Parameter ``channel`` (drjit.llvm.ad.UInt):
            The channel according to which we will sample the free-flight
            distance. This argument is only used when rendering in RGB modes.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.MediumInteraction3f`:
            This method returns a MediumInteraction. The MediumInteraction
            will always be valid, except if the ray missed the Medium's
            bounding box.

    .. py:method:: mitsuba.MediumPtr.transmittance_eval_pdf(self, mi, si, active)

        Compute the transmittance and PDF

        This function evaluates the transmittance and PDF of sampling a
        certain free-flight distance The returned PDF takes into account if a
        medium interaction occurred (mi.t <= si.t) or the ray left the medium
        (mi.t > si.t)

        The evaluated PDF is spectrally varying. This allows to account for
        the fact that the free-flight distance sampling distribution can
        depend on the wavelength.

        Parameter ``mi`` (:py:obj:`mitsuba.MediumInteraction3f`):
            *no description available*

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Color3f`, :py:obj:`mitsuba.Color3f`]:
            This method returns a pair of (Transmittance, PDF).

    .. py:method:: mitsuba.MediumPtr.use_emitter_sampling()

        Returns whether this specific medium instance uses emitter sampling

        Returns → drjit.llvm.ad.Bool:
            *no description available*

.. py:class:: mitsuba.MemoryMappedFile

    Base class: :py:obj:`mitsuba.Object`

    Basic cross-platform abstraction for memory mapped files

    Remark:
        The Python API has one additional constructor
        <tt>MemoryMappedFile(filename, array)<tt>, which creates a new
        file, maps it into memory, and copies the array contents.

    .. py:method:: __init__(self, filename, size)

        Overloaded function.
        
        1. ``__init__(self, filename: :py:obj:`mitsuba.filesystem.path`, size: int) -> None``
        
        Create a new memory-mapped file of the specified size
        
        2. ``__init__(self, filename: :py:obj:`mitsuba.filesystem.path`, write: bool = False) -> None``
        
        Map the specified file into memory
        
        3. ``__init__(self, filename: :py:obj:`mitsuba.filesystem.path`, array: ndarray[device='cpu']) -> None``

        Parameter ``filename`` (:py:obj:`mitsuba.filesystem.path`):
            *no description available*

        Parameter ``size`` (int):
            *no description available*

        
    .. py:method:: mitsuba.MemoryMappedFile.can_write()

        Return whether the mapped memory region can be modified

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.MemoryMappedFile.data()

        Return a pointer to the file contents in memory

        Returns → types.CapsuleType:
            *no description available*

    .. py:method:: mitsuba.MemoryMappedFile.filename()

        Return the associated filename

        Returns → :py:obj:`mitsuba.filesystem.path`:
            *no description available*

    .. py:method:: mitsuba.MemoryMappedFile.resize(self, arg)

        Resize the memory-mapped file

        This involves remapping the file, which will generally change the
        pointer obtained via data()

        Parameter ``arg`` (int, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.MemoryMappedFile.size()

        Return the size of the mapped region

        Returns → int:
            *no description available*

.. py:class:: mitsuba.MemoryStream

    Base class: :py:obj:`mitsuba.Stream`

    Simple memory buffer-based stream with automatic memory management. It
    always has read & write capabilities.

    The underlying memory storage of this implementation dynamically
    expands as data is written to the stream, à la ``std::vector``.

    .. py:method:: __init__(self, capacity=512)

        Creates a new memory stream, initializing the memory buffer with a
        capacity of ``capacity`` bytes. For best performance, set this
        argument to the estimated size of the content that will be written to
        the stream.

        Parameter ``capacity`` (int):
            *no description available*

        
    .. py:method:: mitsuba.MemoryStream.capacity()

        Return the current capacity of the underlying memory buffer

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.MemoryStream.owns_buffer()

        Return whether or not the memory stream owns the underlying buffer

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.MemoryStream.raw_buffer()

        Returns → bytes:
            *no description available*

.. py:class:: mitsuba.Mesh

    Base class: :py:obj:`mitsuba.Shape`

    __init__(self, name: str, vertex_count: int, face_count: int, props: :py:obj:`mitsuba.Properties` = Properties[
      plugin_name = "",
      id = "",
      elements = {
      }
    ]
    , has_vertex_normals: bool = False, has_vertex_texcoords: bool = False) -> None

    Overloaded function.

    1. ``__init__(self, props: :py:obj:`mitsuba.Properties`) -> None``


    2. ``__init__(self, name: str, vertex_count: int, face_count: int, props: :py:obj:`mitsuba.Properties` = Properties[
      plugin_name = "",
      id = "",
      elements = {
      }
    ]
    , has_vertex_normals: bool = False, has_vertex_texcoords: bool = False) -> None``

    Creates a zero-initialized mesh with the given vertex and face counts

    The vertex and face buffers can be filled using the ``mi.traverse``
    mechanism. When initializing these buffers through another method, an
    explicit call to initialize must be made once all buffers are filled.

    .. py:method:: mitsuba.Mesh.add_attribute(self, name, size, buffer)

        Add an attribute buffer with the given ``name`` and ``dim``

        Parameter ``name`` (str):
            *no description available*

        Parameter ``size`` (int):
            *no description available*

        Parameter ``buffer`` (collections.abc.Sequence[float]):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Mesh.attribute_buffer(self, name)

        Return the mesh attribute associated with ``name``

        Parameter ``name`` (str):
            *no description available*

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Mesh.build_directed_edges()

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Mesh.edge_indices(self, tri_index, edge_index, active=True)

        Returns the vertex indices associated with edge ``edge_index`` (0..2)
        of triangle ``tri_index``.

        Parameter ``tri_index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``edge_index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Array2u:
            *no description available*

    .. py:method:: mitsuba.Mesh.face_count()

        Return the total number of faces

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Mesh.face_indices(self, index, active=True)

        Returns the vertex indices associated with triangle ``index``

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Array3u:
            *no description available*

    .. py:method:: mitsuba.Mesh.face_normal(self, index, active=True)

        Returns the normal direction of the face with index ``index``

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Vector3f`:
            *no description available*

    .. py:method:: mitsuba.Mesh.faces_buffer()

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.Mesh.has_face_normals()

        Does this mesh use face normals?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Mesh.has_mesh_attributes()

        Does this mesh have additional mesh attributes?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Mesh.has_vertex_normals()

        Does this mesh have per-vertex normals?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Mesh.has_vertex_texcoords()

        Does this mesh have per-vertex texture coordinates?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Mesh.initialize()

        Must be called once at the end of the construction of a Mesh

        This method computes internal data structures and notifies the parent
        sensor or emitter (if there is one) that this instance is their
        internal shape.

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Mesh.merge(self, other)

        Merge two meshes into one

        Parameter ``other`` (:py:obj:`mitsuba.Mesh`):
            *no description available*

        Returns → ref<mitsuba::Mesh<drjit::DiffArray<(JitBackend)2, float>, mitsuba::Color<drjit::DiffArray<(JitBackend)2, float>, 3ul>>>:
            *no description available*

    .. py:method:: mitsuba.Mesh.opposite_dedge(self, index, active=True)

        Returns the opposite edge index associated with directed edge
        ``index``

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.Mesh.ray_intersect_triangle(self, index, ray, active=True)

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``ray`` (:py:obj:`mitsuba.Ray3f`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Mesh.recompute_bbox()

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Mesh.recompute_vertex_normals()

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Mesh.vertex_count()

        Return the total number of vertices

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Mesh.vertex_normal(self, index, active=True)

        Returns the normal direction of the vertex with index ``index``

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Normal3f`:
            *no description available*

    .. py:method:: mitsuba.Mesh.vertex_normals_buffer()

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Mesh.vertex_position(self, index, active=True)

        Returns the world-space position of the vertex with index ``index``

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Point3f`:
            *no description available*

    .. py:method:: mitsuba.Mesh.vertex_positions_buffer()

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Mesh.vertex_texcoord(self, index, active=True)

        Returns the UV texture coordinates of the vertex with index ``index``

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Point2f`:
            *no description available*

    .. py:method:: mitsuba.Mesh.vertex_texcoords_buffer()

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Mesh.write_ply(self, filename)

        Overloaded function.

        1. ``write_ply(self, filename: str) -> None``

        Write the mesh to a binary PLY file

        Parameter ``filename`` (str):
            Target file path on disk

        2. ``write_ply(self, stream: :py:obj:`mitsuba.Stream`) -> None``

        Write the mesh encoded in binary PLY format to a stream

        Parameter ``stream``:
            Target stream that will receive the encoded output

        Returns → None:
            *no description available*

.. py:class:: mitsuba.MeshPtr

    .. py:method:: mitsuba.MeshPtr.edge_indices(self, tri_index, edge_index, active=True)

        Returns the vertex indices associated with edge ``edge_index`` (0..2)
        of triangle ``tri_index``.

        Parameter ``tri_index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``edge_index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Array2u:
            *no description available*

    .. py:method:: mitsuba.MeshPtr.face_count()

        Return the total number of faces

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.MeshPtr.face_indices(self, index, active=True)

        Returns the vertex indices associated with triangle ``index``

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Array3u:
            *no description available*

    .. py:method:: mitsuba.MeshPtr.face_normal(self, index, active=True)

        Returns the normal direction of the face with index ``index``

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Vector3f`:
            *no description available*

    .. py:method:: mitsuba.MeshPtr.has_face_normals()

        Does this mesh use face normals?

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.MeshPtr.has_mesh_attributes()

        Does this mesh have additional mesh attributes?

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.MeshPtr.has_vertex_normals()

        Does this mesh have per-vertex normals?

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.MeshPtr.has_vertex_texcoords()

        Does this mesh have per-vertex texture coordinates?

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.MeshPtr.opposite_dedge(self, index, active=True)

        Returns the opposite edge index associated with directed edge
        ``index``

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.MeshPtr.ray_intersect_triangle(self, index, ray, active=True)

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``ray`` (:py:obj:`mitsuba.Ray3f`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.MeshPtr.vertex_count()

        Return the total number of vertices

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.MeshPtr.vertex_normal(self, index, active=True)

        Returns the normal direction of the vertex with index ``index``

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Normal3f`:
            *no description available*

    .. py:method:: mitsuba.MeshPtr.vertex_position(self, index, active=True)

        Returns the world-space position of the vertex with index ``index``

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Point3f`:
            *no description available*

    .. py:method:: mitsuba.MeshPtr.vertex_texcoord(self, index, active=True)

        Returns the UV texture coordinates of the vertex with index ``index``

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Point2f`:
            *no description available*

.. py:class:: mitsuba.MicrofacetDistribution

    Implementation of the Beckman and GGX / Trowbridge-Reitz microfacet
    distributions and various useful sampling routines

    Based on the papers

    "Microfacet Models for Refraction through Rough Surfaces" by Bruce
    Walter, Stephen R. Marschner, Hongsong Li, and Kenneth E. Torrance

    and

    "Importance Sampling Microfacet-Based BSDFs using the Distribution of
    Visible Normals" by Eric Heitz and Eugene D'Eon

    The visible normal sampling code was provided by Eric Heitz and Eugene
    D'Eon. An improvement of the Beckmann model sampling routine is
    discussed in

    "An Improved Visible Normal Sampling Routine for the Beckmann
    Distribution" by Wenzel Jakob

    An improvement of the GGX model sampling routine is discussed in "A
    Simpler and Exact Sampling Routine for the GGX Distribution of Visible
    Normals" by Eric Heitz

    .. py:method:: __init__(self, type, alpha, sample_visible=True)

        Parameter ``type`` (:py:obj:`mitsuba.MicrofacetType`):
            *no description available*

        Parameter ``alpha`` (float):
            *no description available*

        Parameter ``sample_visible`` (bool):
            *no description available*


    .. py:method:: mitsuba.MicrofacetDistribution.G(self, wi, wo, m)

        Smith's separable shadowing-masking approximation

        Parameter ``wi`` (:py:obj:`mitsuba.Vector3f`):
            *no description available*

        Parameter ``wo`` (:py:obj:`mitsuba.Vector3f`):
            *no description available*

        Parameter ``m`` (:py:obj:`mitsuba.Vector3f`):
            *no description available*

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.MicrofacetDistribution.alpha()

        Return the roughness (isotropic case)

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.MicrofacetDistribution.alpha_u()

        Return the roughness along the tangent direction

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.MicrofacetDistribution.alpha_v()

        Return the roughness along the bitangent direction

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.MicrofacetDistribution.eval(self, m)

        Evaluate the microfacet distribution function

        Parameter ``m`` (:py:obj:`mitsuba.Vector3f`):
            The microfacet normal

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.MicrofacetDistribution.is_anisotropic()

        Is this an anisotropic microfacet distribution?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.MicrofacetDistribution.is_isotropic()

        Is this an isotropic microfacet distribution?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.MicrofacetDistribution.pdf(self, wi, m)

        Returns the density function associated with the sample() function.

        Parameter ``wi`` (:py:obj:`mitsuba.Vector3f`):
            The incident direction (only relevant if visible normal sampling
            is used)

        Parameter ``m`` (:py:obj:`mitsuba.Vector3f`):
            The microfacet normal

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.MicrofacetDistribution.sample(self, wi, sample)

        Draw a sample from the microfacet normal distribution and return the
        associated probability density

        Parameter ``wi`` (:py:obj:`mitsuba.Vector3f`):
            The incident direction. Only used if visible normal sampling is
            enabled.

        Parameter ``sample`` (:py:obj:`mitsuba.Point2f`):
            A uniformly distributed 2D sample

        Returns → tuple[:py:obj:`mitsuba.Normal3f`, drjit.llvm.ad.Float]:
            A tuple consisting of the sampled microfacet normal and the
            associated solid angle density

    .. py:method:: mitsuba.MicrofacetDistribution.sample_visible()

        Return whether or not only visible normals are sampled?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.MicrofacetDistribution.sample_visible_11(self, cos_theta_i, sample)

        Visible normal sampling code for the alpha=1 case

        Parameter ``cos_theta_i`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``sample`` (:py:obj:`mitsuba.Point2f`):
            *no description available*

        Returns → :py:obj:`mitsuba.Vector2f`:
            *no description available*

    .. py:method:: mitsuba.MicrofacetDistribution.scale_alpha(self, value)

        Scale the roughness values by some constant

        Parameter ``value`` (drjit.llvm.ad.Float):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.MicrofacetDistribution.smith_g1(self, v, m)

        Smith's shadowing-masking function for a single direction

        Parameter ``v`` (:py:obj:`mitsuba.Vector3f`):
            An arbitrary direction

        Parameter ``m`` (:py:obj:`mitsuba.Vector3f`):
            The microfacet normal

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.MicrofacetDistribution.type()

        Return the distribution type

        Returns → :py:obj:`mitsuba.MicrofacetType`:
            *no description available*

.. py:class:: mitsuba.MicrofacetType

    Supported normal distribution functions

    Valid values are as follows:

    .. py:data:: Beckmann

        Beckmann distribution derived from Gaussian random surfaces

    .. py:data:: GGX

        GGX: Long-tailed distribution for very rough surfaces (aka. Trowbridge-Reitz distr.)

.. py:class:: mitsuba.MonteCarloIntegrator

    Base class: :py:obj:`mitsuba.SamplingIntegrator`

    Abstract integrator that performs *recursive* Monte Carlo sampling
    starting from the sensor

    This class is almost identical to SamplingIntegrator. It stores two
    additional fields that are helpful for recursive Monte Carlo
    techniques: the maximum path depth, and the depth at which the Russian
    Roulette path termination technique should start to become active.

.. py:class:: mitsuba.Normal3d

.. py:class:: mitsuba.Normal3f

.. py:class:: mitsuba.Object

    Object base class with builtin reference counting

    This class (in conjunction with the ``ref`` reference counter)
    constitutes the foundation of an efficient reference-counted object
    hierarchy. The implementation here is an alternative to standard
    mechanisms for reference counting such as ``std::shared_ptr`` from the
    STL.

    Why not simply use ``std::shared_ptr``? To be spec-compliant, such
    shared pointers must associate a special record with every instance,
    which stores at least two counters plus a deletion function.
    Allocating this record naturally incurs further overheads to maintain
    data structures within the memory allocator. In addition to this, the
    size of an individual ``shared_ptr`` references is at least two data
    words. All of this quickly adds up and leads to significant overheads
    for large collections of instances, hence the need for an alternative
    in Mitsuba.

    In contrast, the ``Object`` class allows for a highly efficient
    implementation that only adds 64 bits to the base object (for the
    counter) and has no overhead for references. In addition, when using
    Mitsuba in Python, this counter is shared with Python such that the
    ownerhsip and lifetime of any ``Object`` instance across C++ and
    Python is managed by it.

    .. py:method:: __init__()

        Overloaded function.
        
        1. ``__init__(self) -> None``
        
        Default constructor
        
        2. ``__init__(self, arg: :py:obj:`mitsuba.Object`) -> None``
        
        Copy constructor

        
    .. py:method:: mitsuba.Object.class_()

        Return a Class instance containing run-time type information about
        this Object

        See also:
            Class

        Returns → :py:obj:`mitsuba.Class`:
            *no description available*

    .. py:method:: mitsuba.Object.expand()

        Expand the object into a list of sub-objects and return them

        In some cases, an Object instance is merely a container for a number
        of sub-objects. In the context of Mitsuba, an example would be a
        combined sun & sky emitter instantiated via XML, which recursively
        expands into a separate sun & sky instance. This functionality is
        supported by any Mitsuba object, hence it is located this level.

        Returns → list:
            *no description available*

    .. py:method:: mitsuba.Object.id()

        Return an identifier of the current instance (if available)

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.Object.parameters_changed(self, keys=[])

        Update internal state after applying changes to parameters

        This function should be invoked when attributes (obtained via
        traverse) are modified in some way. The object can then update its
        internal state so that derived quantities are consistent with the
        change.

        Parameter ``keys`` (collections.abc.Sequence[str]):
            Optional list of names (obtained via traverse) corresponding to
            the attributes that have been modified. Can also be used to notify
            when this function is called from a parent object by adding a
            "parent" key to the list. When empty, the object should assume
            that any attribute might have changed.

        Remark:
            The default implementation does nothing.

        See also:
            TraversalCallback

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Object.ptr
        :property:

        (self) -> int

    .. py:method:: mitsuba.Object.set_id(self, id)

        Set an identifier to the current instance (if applicable)

        Parameter ``id`` (str):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Object.traverse(self, cb)

        Traverse the attributes and object graph of this instance

        Implementing this function enables recursive traversal of C++ scene
        graphs. It is e.g. used to determine the set of differentiable
        parameters when using Mitsuba for optimization.

        Remark:
            The default implementation does nothing.

        See also:
            TraversalCallback

        Parameter ``cb`` (:py:obj:`mitsuba.TraversalCallback`):
            *no description available*

        Returns → None:
            *no description available*

.. py:class:: mitsuba.ObjectPtr

.. py:class:: mitsuba.OptixDenoiser

    Base class: :py:obj:`mitsuba.Object`

    Wrapper for the OptiX AI denoiser

    The OptiX AI denoiser is wrapped in this object such that it can work
    directly with Mitsuba types and its conventions.

    The denoiser works best when applied to noisy renderings that were
    produced with a Film which used the `box` ReconstructionFilter. With a
    filter that spans multiple pixels, the denoiser might identify some
    local variance as a feature of the scene and will not denoise it.

    .. py:method:: __init__(self, input_size, albedo=False, normals=False, temporal=False)

        Constructs an OptiX denoiser
        
        Parameter ``input_size`` (:py:obj:`mitsuba.ScalarVector2u`):
            Resolution of noisy images that will be fed to the denoiser.
        
        Parameter ``albedo`` (bool):
            Whether or not albedo information will also be given to the
            denoiser.
        
        Parameter ``normals`` (bool):
            Whether or not shading normals information will also be given to
            the Denoiser.
        
        Returns:
            A callable object which will apply the OptiX denoiser.

        Parameter ``temporal`` (bool):
            *no description available*

        
    .. py:method:: mitsuba.OptixDenoiser.__call__(self, noisy, denoise_alpha=True, albedo, normals, to_sensor=None, flow, previous_denoised)

        Overloaded function.

        1. ``__call__(self, noisy: drjit.llvm.ad.TensorXf, denoise_alpha: bool = True, albedo: drjit.llvm.ad.TensorXf, normals: drjit.llvm.ad.TensorXf, to_sensor: object | None = None, flow: drjit.llvm.ad.TensorXf, previous_denoised: drjit.llvm.ad.TensorXf) -> drjit.llvm.ad.TensorXf``

        Apply denoiser on inputs which are TensorXf objects.

        Parameter ``noisy`` (drjit.llvm.ad.TensorXf):
            The noisy input. (tensor shape: (width, height, 3 | 4))

        Parameter ``denoise_alpha`` (bool):
            Whether or not the alpha channel (if specified in the noisy input)
            should be denoised too. This parameter is optional, by default it
            is true.

        Parameter ``albedo`` (drjit.llvm.ad.TensorXf):
            Albedo information of the noisy rendering. This parameter is
            optional unless the OptixDenoiser was built with albedo support.
            (tensor shape: (width, height, 3))

        Parameter ``normals`` (drjit.llvm.ad.TensorXf):
            Shading normal information of the noisy rendering. The normals
            must be in the coordinate frame of the sensor which was used to
            render the noisy input. This parameter is optional unless the
            OptixDenoiser was built with normals support. (tensor shape:
            (width, height, 3))

        Parameter ``to_sensor`` (object | None):
            A Transform4f which is applied to the ``normals`` parameter before
            denoising. This should be used to transform the normals into the
            correct coordinate frame. This parameter is optional, by default
            no transformation is applied.

        Parameter ``flow`` (drjit.llvm.ad.TensorXf):
            With temporal denoising, this parameter is the optical flow
            between the previous frame and the current one. It should capture
            the 2D motion of each individual pixel. When this parameter is
            unknown, it can been set to a zero-initialized TensorXf of the
            correct size and still produce convincing results. This parameter
            is optional unless the OptixDenoiser was built with temporal
            denoising support. (tensor shape: (width, height, 2))

        Parameter ``previous_denoised`` (drjit.llvm.ad.TensorXf):
            With temporal denoising, the previous denoised frame should be
            passed here. For the very first frame, the OptiX documentation
            recommends passing the noisy input for this argument. This
            parameter is optional unless the OptixDenoiser was built with
            temporal denoising support. (tensor shape: (width, height, 3 | 4))

        Returns → drjit.llvm.ad.TensorXf:
            The denoised input.

        2. ``__call__(self, noisy: :py:obj:`mitsuba.Bitmap`, denoise_alpha: bool = True, albedo_ch: str = '', normals_ch: str = '', to_sensor: object | None = None, flow_ch: str = '', previous_denoised_ch: str = '', noisy_ch: str = '<root>') -> :py:obj:`mitsuba.Bitmap```

        Apply denoiser on inputs which are Bitmap objects.

        Parameter ``noisy`` (drjit.llvm.ad.TensorXf):
            The noisy input. When passing additional information like albedo
            or normals to the denoiser, this Bitmap object must be a
            MultiChannel bitmap.

        Parameter ``denoise_alpha`` (bool):
            Whether or not the alpha channel (if specified in the noisy input)
            should be denoised too. This parameter is optional, by default it
            is true.

        Parameter ``albedo_ch``:
            The name of the channel in the ``noisy`` parameter which contains
            the albedo information of the noisy rendering. This parameter is
            optional unless the OptixDenoiser was built with albedo support.

        Parameter ``normals_ch``:
            The name of the channel in the ``noisy`` parameter which contains
            the shading normal information of the noisy rendering. The normals
            must be in the coordinate frame of the sensor which was used to
            render the noisy input. This parameter is optional unless the
            OptixDenoiser was built with normals support.

        Parameter ``to_sensor`` (object | None):
            A Transform4f which is applied to the ``normals`` parameter before
            denoising. This should be used to transform the normals into the
            correct coordinate frame. This parameter is optional, by default
            no transformation is applied.

        Parameter ``flow_ch``:
            With temporal denoising, this parameter is name of the channel in
            the ``noisy`` parameter which contains the optical flow between
            the previous frame and the current one. It should capture the 2D
            motion of each individual pixel. When this parameter is unknown,
            it can been set to a zero-initialized TensorXf of the correct size
            and still produce convincing results. This parameter is optional
            unless the OptixDenoiser was built with temporal denoising
            support.

        Parameter ``previous_denoised_ch``:
            With temporal denoising, this parameter is name of the channel in
            the ``noisy`` parameter which contains the previous denoised
            frame. For the very first frame, the OptiX documentation
            recommends passing the noisy input for this argument. This
            parameter is optional unless the OptixDenoiser was built with
            temporal denoising support.

        Parameter ``noisy_ch``:
            The name of the channel in the ``noisy`` parameter which contains
            the shading normal information of the noisy rendering.

        Returns → drjit.llvm.ad.TensorXf:
            The denoised input.

.. py:class:: mitsuba.PCG32

    Implementation of PCG32, a member of the PCG family of random number generators
    proposed by Melissa O'Neill.

    PCG combines a Linear Congruential Generator (LCG) with a permutation function
    that yields high-quality pseudorandom variates while at the same time requiring
    very low computational cost and internal state (only 128 bit in the case of
    PCG32).

    More detail on the PCG family of pseudorandom number generators can be found
    `here <https://www.pcg-random.org/index.html>`__.

    The :py:class:`PCG32` class is implemented as a :ref:`PyTree <pytrees>`, which
    means that it is compatible with symbolic function calls, loops, etc.

    .. py:method:: __init__(self, size=1, initstate=UInt64(0x853c49e6748fea9b), initseq=UInt64(0xda3e39cb94b95bdb))

        Overloaded function.
        
        1. ``__init__(self, size: int = 1, initstate: drjit.llvm.ad.UInt64 = UInt64(0x853c49e6748fea9b), initseq: drjit.llvm.ad.UInt64 = UInt64(0xda3e39cb94b95bdb)) -> None``
        
        Initialize a random number generator that generates ``size`` variates in parallel.
        
        The ``initstate`` and ``initseq`` inputs determine the initial state and increment
        of the linear congruential generator. Their defaults values are based on the
        original implementation.
        
        The implementation of this routine internally calls py:func:`seed`, with one
        small twist. When multiple random numbers are being generated in parallel, the
        constructor adds an offset equal to :py:func:`drjit.arange(UInt64, size)
        <drjit.arange>` to both ``initstate`` and ``initseq`` to de-correlate the
        generated sequences.
        
        2. ``__init__(self, arg: drjit.llvm.ad.PCG32) -> None``
        
        Copy-construct a new PCG32 instance from an existing instance.

        Parameter ``size`` (int):
            *no description available*

        Parameter ``initstate`` (drjit.llvm.ad.UInt64):
            *no description available*

        Parameter ``initseq`` (drjit.llvm.ad.UInt64):
            *no description available*

        
    .. py:method:: mitsuba.PCG32.inc
        :property:

        Sequence increment of the PCG32 PRNG (an unsigned 64-bit integer or integer array). Please see the original paper for details on this field.

    .. py:method:: mitsuba.PCG32.next_float32()

        Overloaded function.

        1. ``next_float32(self) -> drjit.llvm.ad.Float``


        2. ``next_float32(self, arg: drjit.llvm.ad.Bool, /) -> drjit.llvm.ad.Float``

        Generate a uniformly distributed single precision floating point number on the
        interval :math:`[0, 1)`.

        Two overloads of this function exist: the masked variant does not advance
        the PRNG state of entries ``i`` where ``mask[i] == False``.

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.PCG32.next_float64()

        Overloaded function.

        1. ``next_float64(self) -> drjit.llvm.ad.Float64``


        2. ``next_float64(self, arg: drjit.llvm.ad.Bool, /) -> drjit.llvm.ad.Float64``

        Generate a uniformly distributed double precision floating point number on the
        interval :math:`[0, 1)`.

        Two overloads of this function exist: the masked variant does not advance
        the PRNG state of entries ``i`` where ``mask[i] == False``.

        Returns → drjit.llvm.ad.Float64:
            *no description available*

    .. py:method:: mitsuba.PCG32.next_uint32()

        Overloaded function.

        1. ``next_uint32(self) -> drjit.llvm.ad.UInt``

        Generate a uniformly distributed unsigned 32-bit random number

        Two overloads of this function exist: the masked variant does not advance
        the PRNG state of entries ``i`` where ``mask[i] == False``.

        2. ``next_uint32(self, arg: drjit.llvm.ad.Bool, /) -> drjit.llvm.ad.UInt``

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.PCG32.next_uint32_bounded(self, bound, mask=Bool(True))

        Generate a uniformly distributed 32-bit integer number on the
        interval :math:`[0, \texttt{bound})`.

        To ensure an unbiased result, the implementation relies on an iterative
        scheme that typically finishes after 1-2 iterations.

        Parameter ``bound`` (int):
            *no description available*

        Parameter ``mask`` (drjit.llvm.ad.Bool):
            *no description available*

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.PCG32.next_uint64()

        Overloaded function.

        1. ``next_uint64(self) -> drjit.llvm.ad.UInt64``

        Generate a uniformly distributed unsigned 64-bit random number

        Internally, the function calls :py:func:`next_uint32` twice.

        Two overloads of this function exist: the masked variant does not advance
        the PRNG state of entries ``i`` where ``mask[i] == False``.

        2. ``next_uint64(self, arg: drjit.llvm.ad.Bool, /) -> drjit.llvm.ad.UInt64``

        Returns → drjit.llvm.ad.UInt64:
            *no description available*

    .. py:method:: mitsuba.PCG32.next_uint64_bounded(self, bound, mask=Bool(True))

        Generate a uniformly distributed 64-bit integer number on the
        interval :math:`[0, \texttt{bound})`.

        To ensure an unbiased result, the implementation relies on an iterative
        scheme that typically finishes after 1-2 iterations.

        Parameter ``bound`` (int):
            *no description available*

        Parameter ``mask`` (drjit.llvm.ad.Bool):
            *no description available*

        Returns → drjit.llvm.ad.UInt64:
            *no description available*

    .. py:method:: mitsuba.PCG32.seed(self, initstate=UInt64(0x853c49e6748fea9b), initseq=UInt64(0xda3e39cb94b95bdb))

        Seed the random number generator with the given initial state and sequence ID.

        The ``initstate`` and ``initseq`` inputs determine the initial state and increment
        of the linear congruential generator. Their values are the defaults from the
        original implementation.

        Parameter ``initstate`` (drjit.llvm.ad.UInt64):
            *no description available*

        Parameter ``initseq`` (drjit.llvm.ad.UInt64):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.PCG32.state
        :property:

        Sequence state of the PCG32 PRNG (an unsigned 64-bit integer or integer array). Please see the original paper for details on this field.

.. py:class:: mitsuba.ParamFlags

    This list of flags is used to classify the different types of
    parameters exposed by the plugins.

    For instance, in the context of differentiable rendering, it is
    important to know which parameters can be differentiated, and which of
    those might introduce discontinuities in the Monte Carlo simulation.

    Valid values are as follows:

    .. py:data:: Differentiable

        Tracking gradients w.r.t. this parameter is allowed

    .. py:data:: NonDifferentiable

        Tracking gradients w.r.t. this parameter is not allowed

    .. py:data:: Discontinuous

        Tracking gradients w.r.t. this parameter will introduce discontinuities

.. py:class:: mitsuba.PhaseFunction

    Base class: :py:obj:`mitsuba.Object`

    .. py:method:: mitsuba.PhaseFunction.component_count(self, active=True)

        Number of components this phase function is comprised of.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.PhaseFunction.eval_pdf(self, ctx, mi, wo, active=True)

        Evaluates the phase function model value and PDF

        The function returns the value (which often equals the PDF) of the
        phase function in the query direction.

        Parameter ``ctx`` (:py:obj:`mitsuba.PhaseFunctionContext`):
            A phase function sampling context, contains information about the
            transport mode

        Parameter ``mi`` (:py:obj:`mitsuba.MediumInteraction3f`):
            A medium interaction data structure describing the underlying
            medium position. The incident direction is obtained from the field
            ``mi.wi``.

        Parameter ``wo`` (:py:obj:`mitsuba.Vector3f`):
            An outgoing direction to evaluate.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Color3f`, drjit.llvm.ad.Float]:
            The value and the sampling PDF of the phase function in direction
            wo

    .. py:method:: mitsuba.PhaseFunction.flags(self, index, active=True)

        Overloaded function.

        1. ``flags(self, index: int, active: drjit.llvm.ad.Bool = True) -> int``

        Flags for a specific component of this phase function.

        2. ``flags(self, active: drjit.llvm.ad.Bool = True) -> int``

        Flags for this phase function.

        Parameter ``index`` (int):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.PhaseFunction.id()

        Return a string identifier

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.PhaseFunction.m_flags
        :property:

        Type of phase function (e.g. anisotropic)

    .. py:method:: mitsuba.PhaseFunction.max_projected_area()

        Return the maximum projected area of the microflake distribution

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.PhaseFunction.projected_area(self, mi, active=True)

        Returns the microflake projected area

        The function returns the projected area of the microflake distribution
        defining the phase function. For non-microflake phase functions, e.g.
        isotropic or Henyey-Greenstein, this should return a value of 1.

        Parameter ``mi`` (:py:obj:`mitsuba.MediumInteraction3f`):
            A medium interaction data structure describing the underlying
            medium position. The incident direction is obtained from the field
            ``mi.wi``.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            The projected area in direction ``mi.wi`` at position ``mi.p``

    .. py:method:: mitsuba.PhaseFunction.sample(self, ctx, mi, sample1, sample2, active=True)

        Importance sample the phase function model

        The function returns a sampled direction.

        Parameter ``ctx`` (:py:obj:`mitsuba.PhaseFunctionContext`):
            A phase function sampling context, contains information about the
            transport mode

        Parameter ``mi`` (:py:obj:`mitsuba.MediumInteraction3f`):
            A medium interaction data structure describing the underlying
            medium position. The incident direction is obtained from the field
            ``mi.wi``.

        Parameter ``sample1`` (drjit.llvm.ad.Float):
            A uniformly distributed sample on :math:`[0,1]`. It is used to
            select the phase function component in multi-component models.

        Parameter ``sample2`` (:py:obj:`mitsuba.Point2f`):
            A uniformly distributed sample on :math:`[0,1]^2`. It is used to
            generate the sampled direction.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Vector3f`, :py:obj:`mitsuba.Color3f`, drjit.llvm.ad.Float]:
            A sampled direction wo and its corresponding weight and PDF

.. py:class:: mitsuba.PhaseFunctionContext

    //! @}

    .. py:method:: mitsuba.PhaseFunctionContext.component
        :property:

        (self) -> int

    .. py:method:: mitsuba.PhaseFunctionContext.mode
        :property:

        Transported mode (radiance or importance)

    .. py:method:: mitsuba.PhaseFunctionContext.reverse()

        Reverse the direction of light transport in the record

        This updates the transport mode (radiance to importance and vice
        versa).

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.PhaseFunctionContext.sampler
        :property:

        Sampler object

    .. py:method:: mitsuba.PhaseFunctionContext.type_mask
        :property:

        (self) -> int

.. py:class:: mitsuba.PhaseFunctionFlags

    This enumeration is used to classify phase functions into different
    types, i.e. into isotropic, anisotropic and microflake phase
    functions.

    This can be used to optimize implementations to for example have less
    overhead if the phase function is not a microflake phase function.

    Valid values are as follows:

    .. py:data:: Empty

        

    .. py:data:: Isotropic

        

    .. py:data:: Anisotropic

        

    .. py:data:: Microflake

        

.. py:class:: mitsuba.PhaseFunctionPtr

    .. py:method:: mitsuba.PhaseFunctionPtr.component_count(self, active=True)

        Number of components this phase function is comprised of.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.UInt64:
            *no description available*

    .. py:method:: mitsuba.PhaseFunctionPtr.eval_pdf(self, ctx, mi, wo, active=True)

        Evaluates the phase function model value and PDF

        The function returns the value (which often equals the PDF) of the
        phase function in the query direction.

        Parameter ``ctx`` (:py:obj:`mitsuba.PhaseFunctionContext`):
            A phase function sampling context, contains information about the
            transport mode

        Parameter ``mi`` (:py:obj:`mitsuba.MediumInteraction3f`):
            A medium interaction data structure describing the underlying
            medium position. The incident direction is obtained from the field
            ``mi.wi``.

        Parameter ``wo`` (:py:obj:`mitsuba.Vector3f`):
            An outgoing direction to evaluate.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Color3f`, drjit.llvm.ad.Float]:
            The value and the sampling PDF of the phase function in direction
            wo

    .. py:method:: mitsuba.PhaseFunctionPtr.flags(self, active=True)

        Flags for this phase function.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.PhaseFunctionPtr.max_projected_area()

        Return the maximum projected area of the microflake distribution

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.PhaseFunctionPtr.projected_area(self, mi, active=True)

        Returns the microflake projected area

        The function returns the projected area of the microflake distribution
        defining the phase function. For non-microflake phase functions, e.g.
        isotropic or Henyey-Greenstein, this should return a value of 1.

        Parameter ``mi`` (:py:obj:`mitsuba.MediumInteraction3f`):
            A medium interaction data structure describing the underlying
            medium position. The incident direction is obtained from the field
            ``mi.wi``.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            The projected area in direction ``mi.wi`` at position ``mi.p``

    .. py:method:: mitsuba.PhaseFunctionPtr.sample(self, ctx, mi, sample1, sample2, active=True)

        Importance sample the phase function model

        The function returns a sampled direction.

        Parameter ``ctx`` (:py:obj:`mitsuba.PhaseFunctionContext`):
            A phase function sampling context, contains information about the
            transport mode

        Parameter ``mi`` (:py:obj:`mitsuba.MediumInteraction3f`):
            A medium interaction data structure describing the underlying
            medium position. The incident direction is obtained from the field
            ``mi.wi``.

        Parameter ``sample1`` (drjit.llvm.ad.Float):
            A uniformly distributed sample on :math:`[0,1]`. It is used to
            select the phase function component in multi-component models.

        Parameter ``sample2`` (:py:obj:`mitsuba.Point2f`):
            A uniformly distributed sample on :math:`[0,1]^2`. It is used to
            generate the sampled direction.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Vector3f`, :py:obj:`mitsuba.Color3f`, drjit.llvm.ad.Float]:
            A sampled direction wo and its corresponding weight and PDF

.. py:class:: mitsuba.PluginManager

    The object factory is responsible for loading plugin modules and
    instantiating object instances.

    Ordinarily, this class will be used by making repeated calls to the
    create_object() methods. The generated instances are then assembled
    into a final object graph, such as a scene. One such examples is the
    SceneHandler class, which parses an XML scene file by essentially
    translating the XML elements into calls to create_object().

    .. py:method:: mitsuba.PluginManager.create_object(self, arg)

        Instantiate a plugin, verify its type, and return the newly created
        object instance.

        Parameter ``props``:
            A Properties instance containing all information required to find
            and construct the plugin.

        Parameter ``class_type``:
            Expected type of the instance. An exception will be thrown if it
            turns out not to derive from this class.

        Parameter ``arg`` (:py:obj:`mitsuba._Properties`, /):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.PluginManager.get_plugin_class(self, name, variant)

        Return the class corresponding to a plugin for a specific variant

        Parameter ``name`` (str):
            *no description available*

        Parameter ``variant`` (str):
            *no description available*

        Returns → :py:obj:`mitsuba.Class`:
            *no description available*

.. py:class:: mitsuba.Point0d

.. py:class:: mitsuba.Point0f

.. py:class:: mitsuba.Point0i

.. py:class:: mitsuba.Point0u

.. py:class:: mitsuba.Point1d

.. py:class:: mitsuba.Point1f

.. py:class:: mitsuba.Point1i

.. py:class:: mitsuba.Point1u

.. py:class:: mitsuba.Point2d

.. py:class:: mitsuba.Point2f

.. py:class:: mitsuba.Point2i

.. py:class:: mitsuba.Point2u

.. py:class:: mitsuba.Point3d

.. py:class:: mitsuba.Point3f

.. py:class:: mitsuba.Point3i

.. py:class:: mitsuba.Point3u

.. py:class:: mitsuba.Point4d

.. py:class:: mitsuba.Point4f

.. py:class:: mitsuba.Point4i

.. py:class:: mitsuba.Point4u

.. py:class:: mitsuba.PositionSample3f

    Generic sampling record for positions

    This sampling record is used to implement techniques that draw a
    position from a point, line, surface, or volume domain in 3D and
    furthermore provide auxiliary information about the sample.

    Apart from returning the position and (optionally) the surface normal,
    the responsible sampling method must annotate the record with the
    associated probability density and delta.

    .. py:method:: __init__()

        Overloaded function.
        
        1. ``__init__(self) -> None``
        
        Construct an uninitialized position sample
        
        2. ``__init__(self, other: :py:obj:`mitsuba.PositionSample3f`) -> None``
        
        Copy constructor
        
        3. ``__init__(self, si: :py:obj:`mitsuba.SurfaceInteraction3f`) -> None``
        
        Create a position sampling record from a surface intersection
        
        This is useful to determine the hypothetical sampling density on a
        surface after hitting it using standard ray tracing. This happens for
        instance in path tracing with multiple importance sampling.

        
    .. py:method:: mitsuba.PositionSample3f.assign(self, arg)

        Parameter ``arg`` (:py:obj:`mitsuba.PositionSample3f`, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.PositionSample3f.delta
        :property:

        Set if the sample was drawn from a degenerate (Dirac delta)
        distribution

    .. py:method:: mitsuba.PositionSample3f.n
        :property:

        Sampled surface normal (if applicable)

    .. py:method:: mitsuba.PositionSample3f.p
        :property:

        Sampled position

    .. py:method:: mitsuba.PositionSample3f.pdf
        :property:

        Probability density at the sample

    .. py:method:: mitsuba.PositionSample3f.time
        :property:

        Associated time value

    .. py:method:: mitsuba.PositionSample3f.uv
        :property:

        Optional: 2D sample position associated with the record

        In some uses of this record, a sampled position may be associated with
        an important 2D quantity, such as the texture coordinates on a
        triangle mesh or a position on the aperture of a sensor. When
        applicable, such positions are stored in the ``uv`` attribute.

.. py:class:: mitsuba.PreliminaryIntersection3f

    Stores preliminary information related to a ray intersection

    This data structure is used as return type for the
    Shape::ray_intersect_preliminary efficient ray intersection routine.
    It stores whether the shape is intersected by a given ray, and cache
    preliminary information about the intersection if that is the case.

    If the intersection is deemed relevant, detailed intersection
    information can later be obtained via the create_surface_interaction()
    method.

    .. py:method:: __init__()

        Overloaded function.
        
        1. ``__init__(self) -> None``
        
        //! @}
        
        2. ``__init__(self, arg: :py:obj:`mitsuba.PreliminaryIntersection3f`) -> None``
        
        Copy constructor

        
    .. py:method:: mitsuba.PreliminaryIntersection3f.assign(self, arg)

        Parameter ``arg`` (:py:obj:`mitsuba.PreliminaryIntersection3f`, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.PreliminaryIntersection3f.compute_surface_interaction(self, ray, ray_flags=14, active=True)

        Compute and return detailed information related to a surface
        interaction

        Parameter ``ray`` (:py:obj:`mitsuba.Ray3f`):
            Ray associated with the ray intersection

        Parameter ``ray_flags`` (int):
            Flags specifying which information should be computed

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.SurfaceInteraction3f`:
            A data structure containing the detailed information

    .. py:method:: mitsuba.PreliminaryIntersection3f.instance
        :property:

        Stores a pointer to the parent instance (if applicable)

    .. py:method:: mitsuba.PreliminaryIntersection3f.is_valid()

        Is the current interaction valid?

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.PreliminaryIntersection3f.prim_index
        :property:

        Primitive index, e.g. the triangle ID (if applicable)

    .. py:method:: mitsuba.PreliminaryIntersection3f.prim_uv
        :property:

        2D coordinates on the primitive surface parameterization

    .. py:method:: mitsuba.PreliminaryIntersection3f.shape
        :property:

        Pointer to the associated shape

    .. py:method:: mitsuba.PreliminaryIntersection3f.shape_index
        :property:

        Shape index, e.g. the shape ID in shapegroup (if applicable)

    .. py:method:: mitsuba.PreliminaryIntersection3f.t
        :property:

        Distance traveled along the ray

    .. py:method:: mitsuba.PreliminaryIntersection3f.zero_(self, arg)

        This callback method is invoked by dr::zeros<>, and takes care of
        fields that deviate from the standard zero-initialization convention.
        In this particular class, the ``t`` field should be set to an infinite
        value to mark invalid intersection records.

        Parameter ``arg`` (int, /):
            *no description available*

        Returns → None:
            *no description available*

.. py:class:: mitsuba.ProjectiveCamera

    Base class: :py:obj:`mitsuba.Sensor`

    Projective camera interface

    This class provides an abstract interface to several types of sensors
    that are commonly used in computer graphics, such as perspective and
    orthographic camera models.

    The interface is meant to be implemented by any kind of sensor, whose
    world to clip space transformation can be explained using only linear
    operations on homogeneous coordinates.

    A useful feature of ProjectiveCamera sensors is that their view can be
    rendered using the traditional OpenGL pipeline.

    .. py:method:: mitsuba.ProjectiveCamera.far_clip()

        Return the far clip plane distance

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.ProjectiveCamera.focus_distance()

        Return the distance to the focal plane

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.ProjectiveCamera.near_clip()

        Return the near clip plane distance

        Returns → float:
            *no description available*

.. py:class:: mitsuba.Properties

    Base class: :py:obj:`mitsuba._Properties`

    Overloaded function.

    1. ``__init__(self) -> None``

    Construct an empty property container

    2. ``__init__(self, arg: str, /) -> None``

    Construct an empty property container with a specific plugin name

    3. ``__init__(self, arg: :py:obj:`mitsuba.Properties`) -> None``

    Copy constructor

    .. py:method:: mitsuba.Properties.as_string(self, arg)

        Return one of the parameters (converting it to a string if necessary)

        Parameter ``arg`` (str, /):
            *no description available*

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.Properties.copy_attribute(self, arg0, arg1, arg2)

        Copy a single attribute from another Properties object and potentially
        rename it

        Parameter ``arg0`` (:py:obj:`mitsuba._Properties`):
            *no description available*

        Parameter ``arg1`` (str):
            *no description available*

        Parameter ``arg2`` (str, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Properties.get(self, key, def_value=None)

        Return the value for the specified key it exists, otherwise return default value

        Parameter ``key`` (str):
            *no description available*

        Parameter ``def_value`` (object | None):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Properties.has_property(self, arg)

        Verify if a value with the specified name exists

        Parameter ``arg`` (str, /):
            *no description available*

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Properties.id()

        Returns a unique identifier associated with this instance (or an empty
        string)

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.Properties.mark_queried(self, arg)

        Manually mark a certain property as queried

        Parameter ``arg`` (str, /):
            *no description available*

        Returns → bool:
            ``True`` upon success

    .. py:method:: mitsuba.Properties.merge(self, arg)

        Merge another properties record into the current one.

        Existing properties will be overwritten with the values from ``props``
        if they have the same name.

        Parameter ``arg`` (:py:obj:`mitsuba._Properties`, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Properties.named_references()

        Returns → list[tuple[str, str]]:
            *no description available*

    .. py:method:: mitsuba.Properties.plugin_name()

        Get the associated plugin name

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.Properties.property_names()

        Return an array containing the names of all stored properties

        Returns → list[str]:
            *no description available*

    .. py:method:: mitsuba.Properties.remove_property(self, arg)

        Remove a property with the specified name

        Parameter ``arg`` (str, /):
            *no description available*

        Returns → bool:
            ``True`` upon success

    .. py:method:: mitsuba.Properties.set_id(self, arg)

        Set the unique identifier associated with this instance

        Parameter ``arg`` (str, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Properties.set_plugin_name(self, arg)

        Set the associated plugin name

        Parameter ``arg`` (str, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Properties.string(self, arg0, arg1)

        Retrieve a string value (use default value if no entry exists)

        Parameter ``arg0`` (str):
            *no description available*

        Parameter ``arg1`` (str, /):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Properties.type(self, arg)

        Returns the type of an existing property. If no property exists under
        that name, an error is logged and type ``void`` is returned.

        Parameter ``arg`` (str, /):
            *no description available*

        Returns → mitsuba::Properties::Type:
            *no description available*

    .. py:method:: mitsuba.Properties.unqueried()

        Return the list of un-queried attributed

        Returns → list[str]:
            *no description available*

    .. py:method:: mitsuba.Properties.was_queried(self, arg)

        Check if a certain property was queried

        Parameter ``arg`` (str, /):
            *no description available*

        Returns → bool:
            *no description available*

.. py:class:: mitsuba.Quaternion4f

.. py:class:: mitsuba.RadicalInverse

    Base class: :py:obj:`mitsuba.Object`

    Efficient implementation of a radical inverse function with prime
    bases including scrambled versions.

    This class is used to implement Halton and Hammersley sequences for
    QMC integration in Mitsuba.

    .. py:method:: __init__(self, max_base=8161, scramble=-1)

        Parameter ``max_base`` (int):
            *no description available*

        Parameter ``scramble`` (int):
            *no description available*


    .. py:method:: mitsuba.RadicalInverse.base(self, arg)

        Returns the n-th prime base used by the sequence

        These prime numbers are used as bases in the radical inverse function
        implementation.

        Parameter ``arg`` (int, /):
            *no description available*

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.RadicalInverse.bases()

        Return the number of prime bases for which precomputed tables are
        available

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.RadicalInverse.eval(self, base_index, index)

        Calculate the radical inverse function

        This function is used as a building block to construct Halton and
        Hammersley sequences. Roughly, it computes a b-ary representation of
        the input value ``index``, mirrors it along the decimal point, and
        returns the resulting fractional value. The implementation here uses
        prime numbers for ``b``.

        Parameter ``base_index`` (int):
            Selects the n-th prime that is used as a base when computing the
            radical inverse function (0 corresponds to 2, 1->3, 2->5, etc.).
            The value specified here must be between 0 and 1023.

        Parameter ``index`` (drjit.llvm.ad.UInt64):
            Denotes the index that should be mapped through the radical
            inverse function

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.RadicalInverse.inverse_permutation(self, arg)

        Return the inverse permutation corresponding to the given prime number
        basis

        Parameter ``arg`` (int, /):
            *no description available*

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.RadicalInverse.permutation(self, arg)

        Return the permutation corresponding to the given prime number basis

        Parameter ``arg`` (int, /):
            *no description available*

        Returns → numpy.ndarray[dtype=uint16]:
            *no description available*

    .. py:method:: mitsuba.RadicalInverse.scramble()

        Return the original scramble value

        Returns → int:
            *no description available*

.. py:class:: mitsuba.Ray2f

    Simple n-dimensional ray segment data structure

    Along with the ray origin and direction, this data structure
    additionally stores a maximum ray position ``maxt``, a time value
    ``time`` as well a the wavelength information associated with the ray.

    .. py:method:: __init__()

        Overloaded function.
        
        1. ``__init__(self) -> None``
        
        Create an uninitialized ray
        
        2. ``__init__(self, other: :py:obj:`mitsuba.Ray2f`) -> None``
        
        Copy constructor
        
        3. ``__init__(self, o: :py:obj:`mitsuba.Point2f`, d: :py:obj:`mitsuba.Vector2f`, time: drjit.llvm.ad.Float = 0.0, wavelengths: :py:obj:`mitsuba.Color0f` = []) -> None``
        
        Construct a new ray (o, d) with time
        
        4. ``__init__(self, o: :py:obj:`mitsuba.Point2f`, d: :py:obj:`mitsuba.Vector2f`, maxt: drjit.llvm.ad.Float, time: drjit.llvm.ad.Float, wavelengths: :py:obj:`mitsuba.Color0f`) -> None``
        
        Construct a new ray (o, d) with bounds
        
        5. ``__init__(self, other: :py:obj:`mitsuba.Ray2f`, maxt: drjit.llvm.ad.Float) -> None``
        
        Copy a ray, but change the maxt value

        
    .. py:method:: mitsuba.Ray2f.__call__(self, t)

        Return the position of a point along the ray

        Parameter ``t`` (drjit.llvm.ad.Float):
            *no description available*

        Returns → :py:obj:`mitsuba.Point2f`:
            *no description available*

    .. py:method:: mitsuba.Ray2f.assign(self, arg)

        Parameter ``arg`` (:py:obj:`mitsuba.Ray2f`, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Ray2f.d
        :property:

        Ray direction

    .. py:method:: mitsuba.Ray2f.maxt
        :property:

        Maximum position on the ray segment

    .. py:method:: mitsuba.Ray2f.o
        :property:

        Ray origin

    .. py:method:: mitsuba.Ray2f.time
        :property:

        Time value associated with this ray

    .. py:method:: mitsuba.Ray2f.wavelengths
        :property:

        Wavelength associated with the ray

.. py:class:: mitsuba.Ray3d

    Simple n-dimensional ray segment data structure

    Along with the ray origin and direction, this data structure
    additionally stores a maximum ray position ``maxt``, a time value
    ``time`` as well a the wavelength information associated with the ray.

    .. py:method:: __init__()

        Overloaded function.
        
        1. ``__init__(self) -> None``
        
        Create an uninitialized ray
        
        2. ``__init__(self, other: :py:obj:`mitsuba.Ray3d`) -> None``
        
        Copy constructor
        
        3. ``__init__(self, o: :py:obj:`mitsuba.Point3d`, d: :py:obj:`mitsuba.Vector3d`, time: drjit.llvm.ad.Float64 = 0.0, wavelengths: :py:obj:`mitsuba.Color0f` = []) -> None``
        
        Construct a new ray (o, d) with time
        
        4. ``__init__(self, o: :py:obj:`mitsuba.Point3d`, d: :py:obj:`mitsuba.Vector3d`, maxt: drjit.llvm.ad.Float64, time: drjit.llvm.ad.Float64, wavelengths: :py:obj:`mitsuba.Color0f`) -> None``
        
        Construct a new ray (o, d) with bounds
        
        5. ``__init__(self, other: :py:obj:`mitsuba.Ray3d`, maxt: drjit.llvm.ad.Float64) -> None``
        
        Copy a ray, but change the maxt value

        
    .. py:method:: mitsuba.Ray3d.__call__(self, t)

        Return the position of a point along the ray

        Parameter ``t`` (drjit.llvm.ad.Float64):
            *no description available*

        Returns → :py:obj:`mitsuba.Point3d`:
            *no description available*

    .. py:method:: mitsuba.Ray3d.assign(self, arg)

        Parameter ``arg`` (:py:obj:`mitsuba.Ray3d`, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Ray3d.d
        :property:

        Ray direction

    .. py:method:: mitsuba.Ray3d.maxt
        :property:

        Maximum position on the ray segment

    .. py:method:: mitsuba.Ray3d.o
        :property:

        Ray origin

    .. py:method:: mitsuba.Ray3d.time
        :property:

        Time value associated with this ray

    .. py:method:: mitsuba.Ray3d.wavelengths
        :property:

        Wavelength associated with the ray

.. py:class:: mitsuba.Ray3f

    Simple n-dimensional ray segment data structure

    Along with the ray origin and direction, this data structure
    additionally stores a maximum ray position ``maxt``, a time value
    ``time`` as well a the wavelength information associated with the ray.

    .. py:method:: __init__()

        Overloaded function.
        
        1. ``__init__(self) -> None``
        
        Create an uninitialized ray
        
        2. ``__init__(self, other: :py:obj:`mitsuba.Ray3f`) -> None``
        
        Copy constructor
        
        3. ``__init__(self, o: :py:obj:`mitsuba.Point3f`, d: :py:obj:`mitsuba.Vector3f`, time: drjit.llvm.ad.Float = 0.0, wavelengths: :py:obj:`mitsuba.Color0f` = []) -> None``
        
        Construct a new ray (o, d) with time
        
        4. ``__init__(self, o: :py:obj:`mitsuba.Point3f`, d: :py:obj:`mitsuba.Vector3f`, maxt: drjit.llvm.ad.Float, time: drjit.llvm.ad.Float, wavelengths: :py:obj:`mitsuba.Color0f`) -> None``
        
        Construct a new ray (o, d) with bounds
        
        5. ``__init__(self, other: :py:obj:`mitsuba.Ray3f`, maxt: drjit.llvm.ad.Float) -> None``
        
        Copy a ray, but change the maxt value

        
    .. py:method:: mitsuba.Ray3f.__call__(self, t)

        Return the position of a point along the ray

        Parameter ``t`` (drjit.llvm.ad.Float):
            *no description available*

        Returns → :py:obj:`mitsuba.Point3f`:
            *no description available*

    .. py:method:: mitsuba.Ray3f.assign(self, arg)

        Parameter ``arg`` (:py:obj:`mitsuba.Ray3f`, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Ray3f.d
        :property:

        Ray direction

    .. py:method:: mitsuba.Ray3f.maxt
        :property:

        Maximum position on the ray segment

    .. py:method:: mitsuba.Ray3f.o
        :property:

        Ray origin

    .. py:method:: mitsuba.Ray3f.time
        :property:

        Time value associated with this ray

    .. py:method:: mitsuba.Ray3f.wavelengths
        :property:

        Wavelength associated with the ray

.. py:class:: mitsuba.RayDifferential3f

    Base class: :py:obj:`mitsuba.Ray3f`

    Ray differential -- enhances the basic ray class with offset rays for
    two adjacent pixels on the view plane

    .. py:method:: __init__(self, arg)

        Overloaded function.
        
        1. ``__init__(self, arg: :py:obj:`mitsuba.Ray3f`, /) -> None``
        
        
        2. ``__init__(self) -> None``
        
        Create an uninitialized ray
        
        3. ``__init__(self, ray: :py:obj:`mitsuba.Ray3f`) -> None``
        
        
        4. ``__init__(self, o: :py:obj:`mitsuba.Point3f`, d: :py:obj:`mitsuba.Vector3f`, time: drjit.llvm.ad.Float = 0.0, wavelengths: :py:obj:`mitsuba.Color0f` = []) -> None``
        
        Initialize without differentials.

        Parameter ``arg`` (:py:obj:`mitsuba.Ray3f`, /):
            *no description available*

        
    .. py:method:: mitsuba.RayDifferential3f.assign(self, arg)

        Parameter ``arg`` (:py:obj:`mitsuba.RayDifferential3f`, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.RayDifferential3f.d_x
        :property:

        (self) -> :py:obj:`mitsuba.Vector3f`

    .. py:method:: mitsuba.RayDifferential3f.d_y
        :property:

        (self) -> :py:obj:`mitsuba.Vector3f`

    .. py:method:: mitsuba.RayDifferential3f.has_differentials
        :property:

        (self) -> bool

    .. py:method:: mitsuba.RayDifferential3f.o_x
        :property:

        (self) -> :py:obj:`mitsuba.Point3f`

    .. py:method:: mitsuba.RayDifferential3f.o_y
        :property:

        (self) -> :py:obj:`mitsuba.Point3f`

    .. py:method:: mitsuba.RayDifferential3f.scale_differential(self, amount)

        Parameter ``amount`` (drjit.llvm.ad.Float):
            *no description available*

        Returns → None:
            *no description available*

.. py:class:: mitsuba.RayFlags

    This list of flags is used to determine which members of
    SurfaceInteraction should be computed when calling
    compute_surface_interaction().

    It also specifies whether the SurfaceInteraction should be
    differentiable with respect to the shapes parameters.

    Valid values are as follows:

    .. py:data:: Empty

        No flags set

    .. py:data:: Minimal

        Compute position and geometric normal

    .. py:data:: UV

        Compute UV coordinates

    .. py:data:: dPdUV

        Compute position partials wrt. UV coordinates

    .. py:data:: dNGdUV

        Compute the geometric normal partials wrt. the UV coordinates

    .. py:data:: dNSdUV

        Compute the shading normal partials wrt. the UV coordinates

    .. py:data:: ShadingFrame

        Compute shading normal and shading frame

    .. py:data:: FollowShape

        Derivatives of the SurfaceInteraction fields follow shape's motion

    .. py:data:: DetachShape

        Derivatives of the SurfaceInteraction fields ignore shape's motion

    .. py:data:: All

        //! Compound compute flags

    .. py:data:: AllNonDifferentiable

        Compute all fields of the surface interaction ignoring shape's motion

.. py:class:: mitsuba.ReconstructionFilter

    Base class: :py:obj:`mitsuba.Object`

    Generic interface to separable image reconstruction filters

    When resampling bitmaps or adding samples to a rendering in progress,
    Mitsuba first convolves them with a image reconstruction filter.
    Various kinds are implemented as subclasses of this interface.

    Because image filters are generally too expensive to evaluate for each
    sample, the implementation of this class internally precomputes an
    discrete representation, whose resolution given by
    MI_FILTER_RESOLUTION.

    .. py:method:: mitsuba.ReconstructionFilter.border_size()

        Return the block border size required when rendering with this filter

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.ReconstructionFilter.eval(self, x, active=True)

        Evaluate the filter function

        Parameter ``x`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.ReconstructionFilter.eval_discretized(self, x, active=True)

        Evaluate a discretized version of the filter (generally faster than
        'eval')

        Parameter ``x`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.ReconstructionFilter.is_box_filter()

        Check whether this is a box filter?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.ReconstructionFilter.radius()

        Return the filter's width

        Returns → float:
            *no description available*

.. py:class:: mitsuba.Resampler

    Utility class for efficiently resampling discrete datasets to
    different resolutions

    Template parameter ``Scalar``:
        Denotes the underlying floating point data type (i.e. ``half``,
        ``float``, or ``double``)

    .. py:method:: __init__(self, rfilter, source_res, target_res)

        Create a new Resampler object that transforms between the specified
        resolutions
        
        This constructor precomputes all information needed to efficiently
        perform the desired resampling operation. For that reason, it is most
        efficient if it can be used over and over again (e.g. to resample the
        equal-sized rows of a bitmap)
        
        Parameter ``source_res`` (int):
            Source resolution
        
        Parameter ``target_res`` (int):
            Desired target resolution

        Parameter ``rfilter`` (:py:obj:`mitsuba.BitmapReconstructionFilter`):
            *no description available*

        
    .. py:method:: mitsuba.Resampler.boundary_condition()

        Return the boundary condition that should be used when looking up
        samples outside of the defined input domain

        Returns → :py:obj:`mitsuba.FilterBoundaryCondition`:
            *no description available*

    .. py:method:: mitsuba.Resampler.clamp()

        Returns the range to which resampled values will be clamped

        The default is -infinity to infinity (i.e. no clamping is used)

        Returns → tuple[float, float]:
            *no description available*

    .. py:method:: mitsuba.Resampler.resample(self, source, source_stride, target, target_stride, channels)

        Resample a multi-channel array and clamp the results to a specified
        valid range

        Parameter ``source`` (ndarray[dtype=float32, order='C', device='cpu']):
            Source array of samples

        Parameter ``target`` (ndarray[dtype=float32, order='C', device='cpu']):
            Target array of samples

        Parameter ``source_stride`` (int):
            Stride of samples in the source array. A value of '1' implies that
            they are densely packed.

        Parameter ``target_stride`` (int):
            Stride of samples in the source array. A value of '1' implies that
            they are densely packed.

        Parameter ``channels`` (int):
            Number of channels to be resampled

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Resampler.set_boundary_condition(self, arg)

        Set the boundary condition that should be used when looking up samples
        outside of the defined input domain

        The default is FilterBoundaryCondition::Clamp

        Parameter ``arg`` (:py:obj:`mitsuba.FilterBoundaryCondition`, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Resampler.set_clamp(self, arg)

        If specified, resampled values will be clamped to the given range

        Parameter ``arg`` (tuple[float, float], /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Resampler.source_resolution()

        Return the reconstruction filter's source resolution

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Resampler.taps()

        Return the number of taps used by the reconstruction filter

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Resampler.target_resolution()

        Return the reconstruction filter's target resolution

        Returns → int:
            *no description available*

.. py:class:: mitsuba.SGGXPhaseFunctionParams


    .. py:method:: ``__init__(self, arg0, arg1)

        Construct from a pair of 3D vectors [S_xx, S_yy, S_zz] and [S_xy,
        S_xz, S_yz] that correspond to the entries of a symmetric positive
        definite 3x3 matrix.

        Parameter ``arg0`` (drjit.llvm.ad.Array3f):
            *no description available*

        Parameter ``arg1`` (drjit.llvm.ad.Array3f, /):
            *no description available*

        Returns → None``:
            *no description available*

    .. py:method:: ``__init__(self, arg)

        Copy constructor

        Parameter ``arg`` (:py:obj:`mitsuba.SGGXPhaseFunctionParams`):
            *no description available*

        Returns → None``:
            *no description available*

    .. py:method:: ``__init__(self, arg)

        Parameter ``arg`` (list, /):
            *no description available*

        Returns → None``:
            *no description available*

    .. py:method:: mitsuba.SGGXPhaseFunctionParams.assign(self, arg)

        Parameter ``arg`` (:py:obj:`mitsuba.SGGXPhaseFunctionParams`, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.SGGXPhaseFunctionParams.diag
        :property:

        (self) -> drjit.llvm.ad.Array3f

    .. py:method:: mitsuba.SGGXPhaseFunctionParams.off_diag
        :property:

        (self) -> drjit.llvm.ad.Array3f

.. py:class:: mitsuba.Sampler

    Base class: :py:obj:`mitsuba.Object`

    Base class of all sample generators.

    A *sampler* provides a convenient abstraction around methods that
    generate uniform pseudo- or quasi-random points within a conceptual
    infinite-dimensional unit hypercube \f$[0,1]^\infty$\f. This involves
    two main operations: by querying successive component values of such
    an infinite-dimensional point (next_1d(), next_2d()), or by discarding
    the current point and generating another one (advance()).

    Scalar and vectorized rendering algorithms interact with the sampler
    interface in a slightly different way:

    Scalar rendering algorithm:

    1. The rendering algorithm first invokes seed() to initialize the
    sampler state.

    2. The first pixel sample can now be computed, after which advance()
    needs to be invoked. This repeats until all pixel samples have been
    generated. Note that some implementations need to be configured for a
    certain number of pixel samples, and exceeding these will lead to an
    exception being thrown.

    3. While computing a pixel sample, the rendering algorithm usually
    requests 1D or 2D component blocks using the next_1d() and next_2d()
    functions before moving on to the next sample.

    A vectorized rendering algorithm effectively queries multiple sample
    generators that advance in parallel. This involves the following
    steps:

    1. The rendering algorithm invokes set_samples_per_wavefront() if each
    rendering step is split into multiple passes (in which case fewer
    samples should be returned per sample_1d() or sample_2d() call).

    2. The rendering algorithm then invokes seed() to initialize the
    sampler state, and to inform the sampler of the wavefront size, i.e.,
    how many sampler evaluations should be performed in parallel,
    accounting for all passes. The initialization ensures that the set of
    parallel samplers is mutually statistically independent (in a
    pseudo/quasi-random sense).

    3. advance() can be used to advance to the next point.

    4. As in the scalar approach, the rendering algorithm can request
    batches of (pseudo-) random numbers using the next_1d() and next_2d()
    functions.

    .. py:method:: __init__(self, props)

        Parameter ``props`` (:py:obj:`mitsuba.Properties`):
            *no description available*


    .. py:method:: mitsuba.Sampler.advance()

        Advance to the next sample.

        A subsequent call to ``next_1d`` or ``next_2d`` will access the first
        1D or 2D components of this sample.

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Sampler.clone()

        Create a clone of this sampler.

        Subsequent calls to the cloned sampler will produce the same random
        numbers as the original sampler.

        Remark:
            This method relies on the overload of the copy constructor.

        May throw an exception if not supported.

        Returns → :py:obj:`mitsuba.Sampler`:
            *no description available*

    .. py:method:: mitsuba.Sampler.fork()

        Create a fork of this sampler.

        A subsequent call to ``seed`` is necessary to properly initialize the
        internal state of the sampler.

        May throw an exception if not supported.

        Returns → :py:obj:`mitsuba.Sampler`:
            *no description available*

    .. py:method:: mitsuba.Sampler.next_1d(self, active=True)

        Retrieve the next component value from the current sample

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Sampler.next_2d(self, active=True)

        Retrieve the next two component values from the current sample

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Point2f`:
            *no description available*

    .. py:method:: mitsuba.Sampler.sample_count()

        Return the number of samples per pixel

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Sampler.schedule_state()

        dr::schedule() variables that represent the internal sampler state

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Sampler.seed(self, seed, wavefront_size=4294967295)

        Deterministically seed the underlying RNG, if applicable.

        In the context of wavefront ray tracing & dynamic arrays, this
        function must be called with ``wavefront_size`` matching the size of
        the wavefront.

        Parameter ``seed`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``wavefront_size`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Sampler.set_sample_count(self, spp)

        Set the number of samples per pixel

        Parameter ``spp`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Sampler.set_samples_per_wavefront(self, samples_per_wavefront)

        Set the number of samples per pixel per pass in wavefront modes
        (default is 1)

        Parameter ``samples_per_wavefront`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Sampler.wavefront_size()

        Return the size of the wavefront (or 0, if not seeded)

        Returns → int:
            *no description available*

.. py:class:: mitsuba.SamplingIntegrator

    Base class: :py:obj:`mitsuba.Integrator`

    Abstract integrator that performs Monte Carlo sampling starting from
    the sensor

    Subclasses of this interface must implement the sample() method, which
    performs Monte Carlo integration to return an unbiased statistical
    estimate of the radiance value along a given ray.

    The render() method then repeatedly invokes this estimator to compute
    all pixels of the image.

    .. py:method:: __init__(self, arg)

        Parameter ``arg`` (:py:obj:`mitsuba.Properties`, /):
            *no description available*


    .. py:method:: mitsuba.SamplingIntegrator.hide_emitters
        :property:

        (self) -> bool

    .. py:method:: mitsuba.SamplingIntegrator.render_backward(self, scene, params, grad_in, sensor, seed=0, spp=0)

        Parameter ``scene`` (:py:obj:`mitsuba.Scene`):
            *no description available*

        Parameter ``params`` (object):
            *no description available*

        Parameter ``grad_in`` (drjit.llvm.ad.TensorXf):
            *no description available*

        Parameter ``sensor`` (:py:obj:`mitsuba.Sensor`):
            *no description available*

        Parameter ``seed`` (int):
            *no description available*

        Parameter ``spp`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.SamplingIntegrator.render_forward(self, scene, params, sensor, seed=0, spp=0)

        Parameter ``scene`` (:py:obj:`mitsuba.Scene`):
            *no description available*

        Parameter ``params`` (object):
            *no description available*

        Parameter ``sensor`` (:py:obj:`mitsuba.Sensor`):
            *no description available*

        Parameter ``seed`` (int):
            *no description available*

        Parameter ``spp`` (int):
            *no description available*

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.SamplingIntegrator.sample(self, scene, sampler, ray, medium=None, active=True)

        Sample the incident radiance along a ray.

        Parameter ``scene`` (:py:obj:`mitsuba.Scene`):
            The underlying scene in which the radiance function should be
            sampled

        Parameter ``sampler`` (:py:obj:`mitsuba.Sampler`):
            A source of (pseudo-/quasi-) random numbers

        Parameter ``ray`` (:py:obj:`mitsuba.RayDifferential3f`):
            A ray, optionally with differentials

        Parameter ``medium`` (:py:obj:`mitsuba.Medium` | None):
            If the ray is inside a medium, this parameter holds a pointer to
            that medium

        Parameter ``aov``:
            Integrators may return one or more arbitrary output variables
            (AOVs) via this parameter. If ``nullptr`` is provided to this
            argument, no AOVs should be returned. Otherwise, the caller
            guarantees that space for at least ``aov_names().size()`` entries
            has been allocated.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            A mask that indicates which SIMD lanes are active

        Returns → tuple[:py:obj:`mitsuba.Color3f`, drjit.llvm.ad.Bool, list[drjit.llvm.ad.Float]]:
            A pair containing a spectrum and a mask specifying whether a
            surface or medium interaction was sampled. False mask entries
            indicate that the ray "escaped" the scene, in which case the the
            returned spectrum contains the contribution of environment maps,
            if present. The mask can be used to estimate a suitable alpha
            channel of a rendered image.

        Remark:
            In the Python bindings, this function returns the ``aov`` output
            argument as an additional return value. In other words:

        .. code-block:: c

            (spec, mask, aov) = integrator.sample(scene, sampler, ray, medium, active)


.. py:class:: mitsuba.ScalarBoundingBox2f

    Generic n-dimensional bounding box data structure

    Maintains a minimum and maximum position along each dimension and
    provides various convenience functions for querying and modifying
    them.

    This class is parameterized by the underlying point data structure,
    which permits the use of different scalar types and dimensionalities,
    e.g.

    .. code-block:: c

        BoundingBox<Point3i> integer_bbox(Point3i(0, 1, 3), Point3i(4, 5, 6));
        BoundingBox<Point2d> double_bbox(Point2d(0.0, 1.0), Point2d(4.0, 5.0));


    Template parameter ``T``:
        The underlying point data type (e.g. ``Point2d``)

    .. py:method:: __init__()

        Overloaded function.
        
        1. ``__init__(self) -> None``
        
        Create a new invalid bounding box
        
        Initializes the components of the minimum and maximum position to
        :math:`\infty` and :math:`-\infty`, respectively.
        
        2. ``__init__(self, p: :py:obj:`mitsuba.ScalarPoint2f`) -> None``
        
        Create a collapsed bounding box from a single point
        
        3. ``__init__(self, min: :py:obj:`mitsuba.ScalarPoint2f`, max: :py:obj:`mitsuba.ScalarPoint2f`) -> None``
        
        Create a bounding box from two positions
        
        4. ``__init__(self, arg: :py:obj:`mitsuba.ScalarBoundingBox2f`) -> None``
        
        Copy constructor

        
    .. py:method:: mitsuba.ScalarBoundingBox2f.center()

        Return the center point

        Returns → :py:obj:`mitsuba.ScalarPoint2f`:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox2f.clip(self, arg)

        Clip this bounding box to another bounding box

        Parameter ``arg`` (:py:obj:`mitsuba.ScalarBoundingBox2f`, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox2f.collapsed()

        Check whether this bounding box has collapsed to a point, line, or
        plane

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox2f.contains(self, p, strict=False)

        Overloaded function.

        1. ``contains(self, p: :py:obj:`mitsuba.ScalarPoint2f`, strict: bool = False) -> bool``

        Check whether a point lies *on* or *inside* the bounding box

        Parameter ``p`` (:py:obj:`mitsuba.ScalarPoint2f`):
            The point to be tested

        Template parameter ``Strict``:
            Set this parameter to ``True`` if the bounding box boundary should
            be excluded in the test

        Remark:
            In the Python bindings, the 'Strict' argument is a normal function
            parameter with default value ``False``.

        2. ``contains(self, bbox: :py:obj:`mitsuba.ScalarBoundingBox2f`, strict: bool = False) -> bool``

        Check whether a specified bounding box lies *on* or *within* the
        current bounding box

        Note that by definition, an 'invalid' bounding box (where
        min=:math:`\infty` and max=:math:`-\infty`) does not cover any space.
        Hence, this method will always return *true* when given such an
        argument.

        Template parameter ``Strict``:
            Set this parameter to ``True`` if the bounding box boundary should
            be excluded in the test

        Remark:
            In the Python bindings, the 'Strict' argument is a normal function
            parameter with default value ``False``.

        Parameter ``strict`` (bool):
            *no description available*

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox2f.corner(self, arg)

        Return the position of a bounding box corner

        Parameter ``arg`` (int, /):
            *no description available*

        Returns → :py:obj:`mitsuba.ScalarPoint2f`:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox2f.distance(self, arg)

        Overloaded function.

        1. ``distance(self, arg: :py:obj:`mitsuba.ScalarPoint2f`, /) -> float``

        Calculate the shortest distance between the axis-aligned bounding box
        and the point ``p``.

        2. ``distance(self, arg: :py:obj:`mitsuba.ScalarBoundingBox2f`, /) -> float``

        Calculate the shortest distance between the axis-aligned bounding box
        and ``bbox``.

        Parameter ``arg`` (:py:obj:`mitsuba.ScalarPoint2f`, /):
            *no description available*

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox2f.expand(self, arg)

        Overloaded function.

        1. ``expand(self, arg: :py:obj:`mitsuba.ScalarPoint2f`, /) -> None``

        Expand the bounding box to contain another point

        2. ``expand(self, arg: :py:obj:`mitsuba.ScalarBoundingBox2f`, /) -> None``

        Expand the bounding box to contain another bounding box

        Parameter ``arg`` (:py:obj:`mitsuba.ScalarPoint2f`, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox2f.extents()

        Calculate the bounding box extents

        Returns → :py:obj:`mitsuba.ScalarVector2f`:
            ``max - min``

    .. py:method:: mitsuba.ScalarBoundingBox2f.major_axis()

        Return the dimension index with the index associated side length

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox2f.max
        :property:

        (self) -> :py:obj:`mitsuba.ScalarPoint2f`

    .. py:method:: mitsuba.ScalarBoundingBox2f.min
        :property:

        (self) -> :py:obj:`mitsuba.ScalarPoint2f`

    .. py:method:: mitsuba.ScalarBoundingBox2f.minor_axis()

        Return the dimension index with the shortest associated side length

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox2f.overlaps(self, bbox, strict=False)

        Check two axis-aligned bounding boxes for possible overlap.

        Parameter ``Strict``:
            Set this parameter to ``True`` if the bounding box boundary should
            be excluded in the test

        Remark:
            In the Python bindings, the 'Strict' argument is a normal function
            parameter with default value ``False``.

        Parameter ``bbox`` (:py:obj:`mitsuba.ScalarBoundingBox2f`):
            *no description available*

        Parameter ``strict`` (bool):
            *no description available*

        Returns → bool:
            ``True`` If overlap was detected.

    .. py:method:: mitsuba.ScalarBoundingBox2f.reset()

        Mark the bounding box as invalid.

        This operation sets the components of the minimum and maximum position
        to :math:`\infty` and :math:`-\infty`, respectively.

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox2f.squared_distance(self, arg)

        Overloaded function.

        1. ``squared_distance(self, arg: :py:obj:`mitsuba.ScalarPoint2f`, /) -> float``

        Calculate the shortest squared distance between the axis-aligned
        bounding box and the point ``p``.

        2. ``squared_distance(self, arg: :py:obj:`mitsuba.ScalarBoundingBox2f`, /) -> float``

        Calculate the shortest squared distance between the axis-aligned
        bounding box and ``bbox``.

        Parameter ``arg`` (:py:obj:`mitsuba.ScalarPoint2f`, /):
            *no description available*

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox2f.surface_area()

        Calculate the 2-dimensional surface area of a 3D bounding box

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox2f.valid()

        Check whether this is a valid bounding box

        A bounding box ``bbox`` is considered to be valid when

        .. code-block:: c

            bbox.min[i] <= bbox.max[i]


        holds for each component ``i``.

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox2f.volume()

        Calculate the n-dimensional volume of the bounding box

        Returns → float:
            *no description available*

.. py:class:: mitsuba.ScalarBoundingBox3f

    Generic n-dimensional bounding box data structure

    Maintains a minimum and maximum position along each dimension and
    provides various convenience functions for querying and modifying
    them.

    This class is parameterized by the underlying point data structure,
    which permits the use of different scalar types and dimensionalities,
    e.g.

    .. code-block:: c

        BoundingBox<Point3i> integer_bbox(Point3i(0, 1, 3), Point3i(4, 5, 6));
        BoundingBox<Point2d> double_bbox(Point2d(0.0, 1.0), Point2d(4.0, 5.0));


    Template parameter ``T``:
        The underlying point data type (e.g. ``Point2d``)

    .. py:method:: __init__()

        Overloaded function.
        
        1. ``__init__(self) -> None``
        
        Create a new invalid bounding box
        
        Initializes the components of the minimum and maximum position to
        :math:`\infty` and :math:`-\infty`, respectively.
        
        2. ``__init__(self, p: :py:obj:`mitsuba.ScalarPoint3f`) -> None``
        
        Create a collapsed bounding box from a single point
        
        3. ``__init__(self, min: :py:obj:`mitsuba.ScalarPoint3f`, max: :py:obj:`mitsuba.ScalarPoint3f`) -> None``
        
        Create a bounding box from two positions
        
        4. ``__init__(self, arg: :py:obj:`mitsuba.ScalarBoundingBox3f`) -> None``
        
        Copy constructor

        
    .. py:method:: mitsuba.ScalarBoundingBox3f.bounding_sphere()

        Create a bounding sphere, which contains the axis-aligned box

        Returns → :py:obj:`mitsuba.ScalarBoundingSphere3f`:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox3f.center()

        Return the center point

        Returns → :py:obj:`mitsuba.ScalarPoint3f`:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox3f.clip(self, arg)

        Clip this bounding box to another bounding box

        Parameter ``arg`` (:py:obj:`mitsuba.ScalarBoundingBox3f`, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox3f.collapsed()

        Check whether this bounding box has collapsed to a point, line, or
        plane

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox3f.contains(self, p, strict=False)

        Overloaded function.

        1. ``contains(self, p: :py:obj:`mitsuba.ScalarPoint3f`, strict: bool = False) -> bool``

        Check whether a point lies *on* or *inside* the bounding box

        Parameter ``p`` (:py:obj:`mitsuba.ScalarPoint3f`):
            The point to be tested

        Template parameter ``Strict``:
            Set this parameter to ``True`` if the bounding box boundary should
            be excluded in the test

        Remark:
            In the Python bindings, the 'Strict' argument is a normal function
            parameter with default value ``False``.

        2. ``contains(self, bbox: :py:obj:`mitsuba.ScalarBoundingBox3f`, strict: bool = False) -> bool``

        Check whether a specified bounding box lies *on* or *within* the
        current bounding box

        Note that by definition, an 'invalid' bounding box (where
        min=:math:`\infty` and max=:math:`-\infty`) does not cover any space.
        Hence, this method will always return *true* when given such an
        argument.

        Template parameter ``Strict``:
            Set this parameter to ``True`` if the bounding box boundary should
            be excluded in the test

        Remark:
            In the Python bindings, the 'Strict' argument is a normal function
            parameter with default value ``False``.

        Parameter ``strict`` (bool):
            *no description available*

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox3f.corner(self, arg)

        Return the position of a bounding box corner

        Parameter ``arg`` (int, /):
            *no description available*

        Returns → :py:obj:`mitsuba.ScalarPoint3f`:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox3f.distance(self, arg)

        Overloaded function.

        1. ``distance(self, arg: :py:obj:`mitsuba.ScalarPoint3f`, /) -> float``

        Calculate the shortest distance between the axis-aligned bounding box
        and the point ``p``.

        2. ``distance(self, arg: :py:obj:`mitsuba.ScalarBoundingBox3f`, /) -> float``

        Calculate the shortest distance between the axis-aligned bounding box
        and ``bbox``.

        Parameter ``arg`` (:py:obj:`mitsuba.ScalarPoint3f`, /):
            *no description available*

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox3f.expand(self, arg)

        Overloaded function.

        1. ``expand(self, arg: :py:obj:`mitsuba.ScalarPoint3f`, /) -> None``

        Expand the bounding box to contain another point

        2. ``expand(self, arg: :py:obj:`mitsuba.ScalarBoundingBox3f`, /) -> None``

        Expand the bounding box to contain another bounding box

        Parameter ``arg`` (:py:obj:`mitsuba.ScalarPoint3f`, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox3f.extents()

        Calculate the bounding box extents

        Returns → :py:obj:`mitsuba.ScalarVector3f`:
            ``max - min``

    .. py:method:: mitsuba.ScalarBoundingBox3f.major_axis()

        Return the dimension index with the index associated side length

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox3f.max
        :property:

        (self) -> :py:obj:`mitsuba.ScalarPoint3f`

    .. py:method:: mitsuba.ScalarBoundingBox3f.min
        :property:

        (self) -> :py:obj:`mitsuba.ScalarPoint3f`

    .. py:method:: mitsuba.ScalarBoundingBox3f.minor_axis()

        Return the dimension index with the shortest associated side length

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox3f.overlaps(self, bbox, strict=False)

        Check two axis-aligned bounding boxes for possible overlap.

        Parameter ``Strict``:
            Set this parameter to ``True`` if the bounding box boundary should
            be excluded in the test

        Remark:
            In the Python bindings, the 'Strict' argument is a normal function
            parameter with default value ``False``.

        Parameter ``bbox`` (:py:obj:`mitsuba.ScalarBoundingBox3f`):
            *no description available*

        Parameter ``strict`` (bool):
            *no description available*

        Returns → bool:
            ``True`` If overlap was detected.

    .. py:method:: mitsuba.ScalarBoundingBox3f.ray_intersect(self, ray)

        Check if a ray intersects a bounding box

        Note that this function ignores the ``maxt`` value associated with the
        ray.

        Parameter ``ray`` (:py:obj:`mitsuba.Ray3f`):
            *no description available*

        Returns → tuple[drjit.llvm.ad.Bool, drjit.llvm.ad.Float, drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox3f.reset()

        Mark the bounding box as invalid.

        This operation sets the components of the minimum and maximum position
        to :math:`\infty` and :math:`-\infty`, respectively.

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox3f.squared_distance(self, arg)

        Overloaded function.

        1. ``squared_distance(self, arg: :py:obj:`mitsuba.ScalarPoint3f`, /) -> float``

        Calculate the shortest squared distance between the axis-aligned
        bounding box and the point ``p``.

        2. ``squared_distance(self, arg: :py:obj:`mitsuba.ScalarBoundingBox3f`, /) -> float``

        Calculate the shortest squared distance between the axis-aligned
        bounding box and ``bbox``.

        Parameter ``arg`` (:py:obj:`mitsuba.ScalarPoint3f`, /):
            *no description available*

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox3f.surface_area()

        Calculate the 2-dimensional surface area of a 3D bounding box

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox3f.valid()

        Check whether this is a valid bounding box

        A bounding box ``bbox`` is considered to be valid when

        .. code-block:: c

            bbox.min[i] <= bbox.max[i]


        holds for each component ``i``.

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingBox3f.volume()

        Calculate the n-dimensional volume of the bounding box

        Returns → float:
            *no description available*

.. py:class:: mitsuba.ScalarBoundingSphere3f

    Generic n-dimensional bounding sphere data structure

    .. py:method:: __init__()

        Overloaded function.
        
        1. ``__init__(self) -> None``
        
        Construct bounding sphere(s) at the origin having radius zero
        
        2. ``__init__(self, arg0: :py:obj:`mitsuba.ScalarPoint3f`, arg1: float, /) -> None``
        
        Create bounding sphere(s) from given center point(s) with given
        size(s)
        
        3. ``__init__(self, arg: :py:obj:`mitsuba.ScalarBoundingSphere3f`) -> None``

        
    .. py:method:: mitsuba.ScalarBoundingSphere3f.center
        :property:

        (self) -> :py:obj:`mitsuba.ScalarPoint3f`

    .. py:method:: mitsuba.ScalarBoundingSphere3f.contains(self, p, strict=False)

        Check whether a point lies *on* or *inside* the bounding sphere

        Parameter ``p`` (:py:obj:`mitsuba.ScalarPoint3f`):
            The point to be tested

        Template parameter ``Strict``:
            Set this parameter to ``True`` if the bounding sphere boundary
            should be excluded in the test

        Remark:
            In the Python bindings, the 'Strict' argument is a normal function
            parameter with default value ``False``.

        Parameter ``strict`` (bool):
            *no description available*

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingSphere3f.empty()

        Return whether this bounding sphere has a radius of zero or less.

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingSphere3f.expand(self, arg)

        Expand the bounding sphere radius to contain another point.

        Parameter ``arg`` (:py:obj:`mitsuba.ScalarPoint3f`, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ScalarBoundingSphere3f.radius
        :property:

        (self) -> float

    .. py:method:: mitsuba.ScalarBoundingSphere3f.ray_intersect(self, ray)

        Check if a ray intersects a bounding box

        Parameter ``ray`` (:py:obj:`mitsuba.Ray3f`):
            *no description available*

        Returns → tuple[drjit.llvm.ad.Bool, drjit.llvm.ad.Float, drjit.llvm.ad.Float]:
            *no description available*

.. py:class:: mitsuba.ScalarColor0d

.. py:class:: mitsuba.ScalarColor0f

.. py:class:: mitsuba.ScalarColor1d

.. py:class:: mitsuba.ScalarColor1f

.. py:class:: mitsuba.ScalarColor3d

.. py:class:: mitsuba.ScalarColor3f

.. py:class:: mitsuba.ScalarNormal3d

.. py:class:: mitsuba.ScalarNormal3f

.. py:class:: mitsuba.ScalarPoint0d

.. py:class:: mitsuba.ScalarPoint0f

.. py:class:: mitsuba.ScalarPoint0i

.. py:class:: mitsuba.ScalarPoint0u

.. py:class:: mitsuba.ScalarPoint1d

.. py:class:: mitsuba.ScalarPoint1f

.. py:class:: mitsuba.ScalarPoint1i

.. py:class:: mitsuba.ScalarPoint1u

.. py:class:: mitsuba.ScalarPoint2d

.. py:class:: mitsuba.ScalarPoint2f

.. py:class:: mitsuba.ScalarPoint2i

.. py:class:: mitsuba.ScalarPoint2u

.. py:class:: mitsuba.ScalarPoint3d

.. py:class:: mitsuba.ScalarPoint3f

.. py:class:: mitsuba.ScalarPoint3i

.. py:class:: mitsuba.ScalarPoint3u

.. py:class:: mitsuba.ScalarPoint4d

.. py:class:: mitsuba.ScalarPoint4f

.. py:class:: mitsuba.ScalarPoint4i

.. py:class:: mitsuba.ScalarPoint4u

.. py:class:: mitsuba.ScalarTransform3d

    Encapsulates a 4x4 homogeneous coordinate transformation along with
    its inverse transpose

    The Transform class provides a set of overloaded matrix-vector
    multiplication operators for vectors, points, and normals (all of them
    behave differently under homogeneous coordinate transformations, hence
    the need to represent them using separate types)

    .. py:method:: __init__()

        Overloaded function.
        
        1. ``__init__(self) -> None``
        
        Initialize with the identity matrix
        
        2. ``__init__(self, arg: :py:obj:`mitsuba.ScalarTransform3d`) -> None``
        
        Copy constructor
        
        3. ``__init__(self, arg: ndarray[dtype=float64, shape=(3, 3), order='C', device='cpu'], /) -> None``
        
        
        4. ``__init__(self) -> None``
        
        
        5. ``__init__(self, arg: drjit.scalar.Matrix3f64, /) -> None``
        
        Initialize the transformation from the given matrix (and compute its
        inverse transpose)
        
        6. ``__init__(self, arg0: drjit.scalar.Matrix3f64, arg1: drjit.scalar.Matrix3f64, /) -> None``
        
        Initialize from a matrix and its inverse transpose

        
    .. py:method:: mitsuba.ScalarTransform3d.assign(self, arg)

        Parameter ``arg`` (:py:obj:`mitsuba.ScalarTransform3d`, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform3d.has_scale()

        Test for a scale component in each transform matrix by checking
        whether ``M . M^T == I`` (where ``M`` is the matrix in question and
        ``I`` is the identity).

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform3d.inverse()

        Compute the inverse of this transformation (involves just shuffles, no
        arithmetic)

        Returns → :py:obj:`mitsuba.ScalarTransform3d`:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform3d.inverse_transpose
        :property:

        (self) -> drjit.scalar.Matrix3f64

    .. py:method:: mitsuba.ScalarTransform3d.matrix
        :property:

        (self) -> drjit.scalar.Matrix3f64

    .. py:method:: mitsuba.ScalarTransform3d.rotate(self, angle)

        Create a rotation transformation in 2D. The angle is specified in
        degrees

        Parameter ``angle`` (float):
            *no description available*

        Returns → :py:obj:`mitsuba.ScalarTransform3d`:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform3d.scale(self, v)

        Create a scale transformation

        Parameter ``v`` (:py:obj:`mitsuba.ScalarPoint2d`):
            *no description available*

        Returns → :py:obj:`mitsuba.ScalarTransform3d`:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform3d.transform_affine(self, p)

        Transform a 3D vector/point/normal/ray by a transformation that is
        known to be an affine 3D transformation (i.e. no perspective)

        Parameter ``p`` (:py:obj:`mitsuba.ScalarPoint2d`):
            *no description available*

        Returns → :py:obj:`mitsuba.ScalarPoint2d`:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform3d.translate(self, v)

        Create a translation transformation

        Parameter ``v`` (:py:obj:`mitsuba.ScalarPoint2d`):
            *no description available*

        Returns → :py:obj:`mitsuba.ScalarTransform3d`:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform3d.translation()

        Get the translation part of a matrix

        Returns → :py:obj:`mitsuba.ScalarVector2d`:
            *no description available*

.. py:class:: mitsuba.ScalarTransform3f

    Encapsulates a 4x4 homogeneous coordinate transformation along with
    its inverse transpose

    The Transform class provides a set of overloaded matrix-vector
    multiplication operators for vectors, points, and normals (all of them
    behave differently under homogeneous coordinate transformations, hence
    the need to represent them using separate types)

    .. py:method:: __init__()

        Overloaded function.
        
        1. ``__init__(self) -> None``
        
        Initialize with the identity matrix
        
        2. ``__init__(self, arg: :py:obj:`mitsuba.ScalarTransform3f`) -> None``
        
        Copy constructor
        
        3. ``__init__(self, arg: ndarray[dtype=float32, shape=(3, 3), order='C', device='cpu'], /) -> None``
        
        
        4. ``__init__(self) -> None``
        
        
        5. ``__init__(self, arg: drjit.scalar.Matrix3f, /) -> None``
        
        Initialize the transformation from the given matrix (and compute its
        inverse transpose)
        
        6. ``__init__(self, arg0: drjit.scalar.Matrix3f, arg1: drjit.scalar.Matrix3f, /) -> None``
        
        Initialize from a matrix and its inverse transpose

        
    .. py:method:: mitsuba.ScalarTransform3f.assign(self, arg)

        Parameter ``arg`` (:py:obj:`mitsuba.ScalarTransform3f`, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform3f.has_scale()

        Test for a scale component in each transform matrix by checking
        whether ``M . M^T == I`` (where ``M`` is the matrix in question and
        ``I`` is the identity).

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform3f.inverse()

        Compute the inverse of this transformation (involves just shuffles, no
        arithmetic)

        Returns → :py:obj:`mitsuba.ScalarTransform3f`:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform3f.inverse_transpose
        :property:

        (self) -> drjit.scalar.Matrix3f

    .. py:method:: mitsuba.ScalarTransform3f.matrix
        :property:

        (self) -> drjit.scalar.Matrix3f

    .. py:method:: mitsuba.ScalarTransform3f.rotate(self, angle)

        Create a rotation transformation in 2D. The angle is specified in
        degrees

        Parameter ``angle`` (float):
            *no description available*

        Returns → :py:obj:`mitsuba.ScalarTransform3f`:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform3f.scale(self, v)

        Create a scale transformation

        Parameter ``v`` (:py:obj:`mitsuba.ScalarPoint2f`):
            *no description available*

        Returns → :py:obj:`mitsuba.ScalarTransform3f`:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform3f.transform_affine(self, p)

        Transform a 3D vector/point/normal/ray by a transformation that is
        known to be an affine 3D transformation (i.e. no perspective)

        Parameter ``p`` (:py:obj:`mitsuba.ScalarPoint2f`):
            *no description available*

        Returns → :py:obj:`mitsuba.ScalarPoint2f`:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform3f.translate(self, v)

        Create a translation transformation

        Parameter ``v`` (:py:obj:`mitsuba.ScalarPoint2f`):
            *no description available*

        Returns → :py:obj:`mitsuba.ScalarTransform3f`:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform3f.translation()

        Get the translation part of a matrix

        Returns → :py:obj:`mitsuba.ScalarVector2f`:
            *no description available*

.. py:class:: mitsuba.ScalarTransform4d

    Encapsulates a 4x4 homogeneous coordinate transformation along with
    its inverse transpose

    The Transform class provides a set of overloaded matrix-vector
    multiplication operators for vectors, points, and normals (all of them
    behave differently under homogeneous coordinate transformations, hence
    the need to represent them using separate types)

    .. py:method:: __init__()

        Overloaded function.
        
        1. ``__init__(self) -> None``
        
        Initialize with the identity matrix
        
        2. ``__init__(self, arg: :py:obj:`mitsuba.ScalarTransform4d`) -> None``
        
        Copy constructor
        
        3. ``__init__(self, arg: ndarray[dtype=float64, shape=(4, 4), order='C', device='cpu'], /) -> None``
        
        
        4. ``__init__(self, arg: list, /) -> None``
        
        
        5. ``__init__(self, arg: drjit.scalar.Matrix4f64, /) -> None``
        
        Initialize the transformation from the given matrix (and compute its
        inverse transpose)
        
        6. ``__init__(self, arg0: drjit.scalar.Matrix4f64, arg1: drjit.scalar.Matrix4f64, /) -> None``
        
        Initialize from a matrix and its inverse transpose

        
    .. py:method:: mitsuba.ScalarTransform4d.assign(self, arg)

        Parameter ``arg`` (:py:obj:`mitsuba.ScalarTransform4d`, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4d.extract()

        Extract a lower-dimensional submatrix

        Returns → :py:obj:`mitsuba.ScalarTransform3d`:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4d.from_frame(self, frame)

        Creates a transformation that converts from 'frame' to the standard
        basis

        Parameter ``frame`` (:py:obj:`mitsuba.Frame`):
            *no description available*

        Returns → :py:obj:`mitsuba.ScalarTransform4d`:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4d.has_scale()

        Test for a scale component in each transform matrix by checking
        whether ``M . M^T == I`` (where ``M`` is the matrix in question and
        ``I`` is the identity).

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4d.inverse()

        Compute the inverse of this transformation (involves just shuffles, no
        arithmetic)

        Returns → :py:obj:`mitsuba.ScalarTransform4d`:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4d.inverse_transpose
        :property:

        (self) -> drjit.scalar.Matrix4f64

    .. py:method:: mitsuba.ScalarTransform4d.look_at(self, origin, target, up)

        Create a look-at camera transformation

        Parameter ``origin`` (:py:obj:`mitsuba.ScalarPoint3d`):
            Camera position

        Parameter ``target`` (:py:obj:`mitsuba.ScalarPoint3d`):
            Target vector

        Parameter ``up`` (:py:obj:`mitsuba.ScalarPoint3d`):
            Up vector

        Returns → :py:obj:`mitsuba.ScalarTransform4d`:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4d.matrix
        :property:

        (self) -> drjit.scalar.Matrix4f64

    .. py:method:: mitsuba.ScalarTransform4d.orthographic(self, near, far)

        Create an orthographic transformation, which maps Z to [0,1] and
        leaves the X and Y coordinates untouched.

        Parameter ``near`` (float):
            Near clipping plane

        Parameter ``far`` (float):
            Far clipping plane

        Returns → :py:obj:`mitsuba.ScalarTransform4d`:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4d.perspective(self, fov, near, far)

        Create a perspective transformation. (Maps [near, far] to [0, 1])

        Projects vectors in camera space onto a plane at z=1:

        x_proj = x / z y_proj = y / z z_proj = (far * (z - near)) / (z * (far-
        near))

        Camera-space depths are not mapped linearly!

        Parameter ``fov`` (float):
            Field of view in degrees

        Parameter ``near`` (float):
            Near clipping plane

        Parameter ``far`` (float):
            Far clipping plane

        Returns → :py:obj:`mitsuba.ScalarTransform4d`:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4d.rotate(self, axis, angle)

        Create a rotation transformation around an arbitrary axis in 3D. The
        angle is specified in degrees

        Parameter ``axis`` (:py:obj:`mitsuba.ScalarPoint3d`):
            *no description available*

        Parameter ``angle`` (float):
            *no description available*

        Returns → :py:obj:`mitsuba.ScalarTransform4d`:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4d.scale(self, v)

        Create a scale transformation

        Parameter ``v`` (:py:obj:`mitsuba.ScalarPoint3d`):
            *no description available*

        Returns → :py:obj:`mitsuba.ScalarTransform4d`:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4d.to_frame(self, frame)

        Creates a transformation that converts from the standard basis to
        'frame'

        Parameter ``frame`` (:py:obj:`mitsuba.Frame`):
            *no description available*

        Returns → :py:obj:`mitsuba.ScalarTransform4d`:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4d.transform_affine(self, p)

        Transform a 3D vector/point/normal/ray by a transformation that is
        known to be an affine 3D transformation (i.e. no perspective)

        Parameter ``p`` (:py:obj:`mitsuba.ScalarPoint3d`):
            *no description available*

        Returns → :py:obj:`mitsuba.ScalarPoint3d`:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4d.translate(self, v)

        Create a translation transformation

        Parameter ``v`` (:py:obj:`mitsuba.ScalarPoint3d`):
            *no description available*

        Returns → :py:obj:`mitsuba.ScalarTransform4d`:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4d.translation()

        Get the translation part of a matrix

        Returns → :py:obj:`mitsuba.ScalarVector3d`:
            *no description available*

.. py:class:: mitsuba.ScalarTransform4f

    Encapsulates a 4x4 homogeneous coordinate transformation along with
    its inverse transpose

    The Transform class provides a set of overloaded matrix-vector
    multiplication operators for vectors, points, and normals (all of them
    behave differently under homogeneous coordinate transformations, hence
    the need to represent them using separate types)

    .. py:method:: __init__()

        Overloaded function.
        
        1. ``__init__(self) -> None``
        
        Initialize with the identity matrix
        
        2. ``__init__(self, arg: :py:obj:`mitsuba.ScalarTransform4f`) -> None``
        
        Copy constructor
        
        3. ``__init__(self, arg: ndarray[dtype=float32, shape=(4, 4), order='C', device='cpu'], /) -> None``
        
        
        4. ``__init__(self, arg: list, /) -> None``
        
        
        5. ``__init__(self, arg: drjit.scalar.Matrix4f, /) -> None``
        
        Initialize the transformation from the given matrix (and compute its
        inverse transpose)
        
        6. ``__init__(self, arg0: drjit.scalar.Matrix4f, arg1: drjit.scalar.Matrix4f, /) -> None``
        
        Initialize from a matrix and its inverse transpose

        
    .. py:method:: mitsuba.ScalarTransform4f.assign(self, arg)

        Parameter ``arg`` (:py:obj:`mitsuba.ScalarTransform4f`, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4f.extract()

        Extract a lower-dimensional submatrix

        Returns → :py:obj:`mitsuba.ScalarTransform3f`:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4f.from_frame(self, frame)

        Creates a transformation that converts from 'frame' to the standard
        basis

        Parameter ``frame`` (:py:obj:`mitsuba.Frame`):
            *no description available*

        Returns → :py:obj:`mitsuba.ScalarTransform4f`:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4f.has_scale()

        Test for a scale component in each transform matrix by checking
        whether ``M . M^T == I`` (where ``M`` is the matrix in question and
        ``I`` is the identity).

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4f.inverse()

        Compute the inverse of this transformation (involves just shuffles, no
        arithmetic)

        Returns → :py:obj:`mitsuba.ScalarTransform4f`:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4f.inverse_transpose
        :property:

        (self) -> drjit.scalar.Matrix4f

    .. py:method:: mitsuba.ScalarTransform4f.look_at(self, origin, target, up)

        Create a look-at camera transformation

        Parameter ``origin`` (:py:obj:`mitsuba.ScalarPoint3f`):
            Camera position

        Parameter ``target`` (:py:obj:`mitsuba.ScalarPoint3f`):
            Target vector

        Parameter ``up`` (:py:obj:`mitsuba.ScalarPoint3f`):
            Up vector

        Returns → :py:obj:`mitsuba.ScalarTransform4f`:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4f.matrix
        :property:

        (self) -> drjit.scalar.Matrix4f

    .. py:method:: mitsuba.ScalarTransform4f.orthographic(self, near, far)

        Create an orthographic transformation, which maps Z to [0,1] and
        leaves the X and Y coordinates untouched.

        Parameter ``near`` (float):
            Near clipping plane

        Parameter ``far`` (float):
            Far clipping plane

        Returns → :py:obj:`mitsuba.ScalarTransform4f`:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4f.perspective(self, fov, near, far)

        Create a perspective transformation. (Maps [near, far] to [0, 1])

        Projects vectors in camera space onto a plane at z=1:

        x_proj = x / z y_proj = y / z z_proj = (far * (z - near)) / (z * (far-
        near))

        Camera-space depths are not mapped linearly!

        Parameter ``fov`` (float):
            Field of view in degrees

        Parameter ``near`` (float):
            Near clipping plane

        Parameter ``far`` (float):
            Far clipping plane

        Returns → :py:obj:`mitsuba.ScalarTransform4f`:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4f.rotate(self, axis, angle)

        Create a rotation transformation around an arbitrary axis in 3D. The
        angle is specified in degrees

        Parameter ``axis`` (:py:obj:`mitsuba.ScalarPoint3f`):
            *no description available*

        Parameter ``angle`` (float):
            *no description available*

        Returns → :py:obj:`mitsuba.ScalarTransform4f`:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4f.scale(self, v)

        Create a scale transformation

        Parameter ``v`` (:py:obj:`mitsuba.ScalarPoint3f`):
            *no description available*

        Returns → :py:obj:`mitsuba.ScalarTransform4f`:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4f.to_frame(self, frame)

        Creates a transformation that converts from the standard basis to
        'frame'

        Parameter ``frame`` (:py:obj:`mitsuba.Frame`):
            *no description available*

        Returns → :py:obj:`mitsuba.ScalarTransform4f`:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4f.transform_affine(self, p)

        Transform a 3D vector/point/normal/ray by a transformation that is
        known to be an affine 3D transformation (i.e. no perspective)

        Parameter ``p`` (:py:obj:`mitsuba.ScalarPoint3f`):
            *no description available*

        Returns → :py:obj:`mitsuba.ScalarPoint3f`:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4f.translate(self, v)

        Create a translation transformation

        Parameter ``v`` (:py:obj:`mitsuba.ScalarPoint3f`):
            *no description available*

        Returns → :py:obj:`mitsuba.ScalarTransform4f`:
            *no description available*

    .. py:method:: mitsuba.ScalarTransform4f.translation()

        Get the translation part of a matrix

        Returns → :py:obj:`mitsuba.ScalarVector3f`:
            *no description available*

.. py:class:: mitsuba.ScalarVector0d

.. py:class:: mitsuba.ScalarVector0f

.. py:class:: mitsuba.ScalarVector0i

.. py:class:: mitsuba.ScalarVector0u

.. py:class:: mitsuba.ScalarVector1d

.. py:class:: mitsuba.ScalarVector1f

.. py:class:: mitsuba.ScalarVector1i

.. py:class:: mitsuba.ScalarVector1u

.. py:class:: mitsuba.ScalarVector2d

.. py:class:: mitsuba.ScalarVector2f

.. py:class:: mitsuba.ScalarVector2i

.. py:class:: mitsuba.ScalarVector2u

.. py:class:: mitsuba.ScalarVector3d

.. py:class:: mitsuba.ScalarVector3f

.. py:class:: mitsuba.ScalarVector3i

.. py:class:: mitsuba.ScalarVector3u

.. py:class:: mitsuba.ScalarVector4d

.. py:class:: mitsuba.ScalarVector4f

.. py:class:: mitsuba.ScalarVector4i

.. py:class:: mitsuba.ScalarVector4u

.. py:class:: mitsuba.Scene

    Base class: :py:obj:`mitsuba.Object`

    Central scene data structure

    Mitsuba's scene class encapsulates a tree of mitsuba Object instances
    including emitters, sensors, shapes, materials, participating media,
    the integrator (i.e. the method used to render the image) etc.

    It organizes these objects into groups that can be accessed through
    getters (see shapes(), emitters(), sensors(), etc.), and it provides
    three key abstractions implemented on top of these groups,
    specifically:

    * Ray intersection queries and shadow ray tests (See
    \ray_intersect_preliminary(), ray_intersect(), and ray_test()).

    * Sampling rays approximately proportional to the emission profile of
    light sources in the scene (see sample_emitter_ray())

    * Sampling directions approximately proportional to the direct
    radiance from emitters received at a given scene location (see
    sample_emitter_direction()).

    .. py:method:: __init__(self, arg)

        Parameter ``arg`` (:py:obj:`mitsuba._Properties`, /):
            *no description available*


    .. py:method:: mitsuba.Scene.bbox()

        Return a bounding box surrounding the scene

        Returns → :py:obj:`mitsuba.ScalarBoundingBox3f`:
            *no description available*

    .. py:method:: mitsuba.Scene.emitters()

        Return the list of emitters

        Returns → list[:py:obj:`mitsuba.Emitter`]:
            *no description available*

    .. py:method:: mitsuba.Scene.emitters_dr()

        Return the list of emitters as a Dr.Jit array

        Returns → :py:obj:`mitsuba.EmitterPtr`:
            *no description available*

    .. py:method:: mitsuba.Scene.environment()

        Return the environment emitter (if any)

        Returns → :py:obj:`mitsuba.Emitter`:
            *no description available*

    .. py:method:: mitsuba.Scene.eval_emitter_direction(self, ref, ds, active=True)

        Re-evaluate the incident direct radiance of the
        sample_emitter_direction() method.

        This function re-evaluates the incident direct radiance and sample
        probability due to the emitter *so that division by * ``ds.pdf``
        equals the sampling weight returned by sample_emitter_direction().
        This may appear redundant, and indeed such a function would not find
        use in "normal" rendering algorithms.

        However, the ability to re-evaluate the contribution of a direct
        illumination sample is important for differentiable rendering. For
        example, we might want to track derivatives in the sampled direction
        (``ds.d``) without also differentiating the sampling technique.

        In contrast to pdf_emitter_direction(), evaluating this function can
        yield a nonzero result in the case of emission profiles containing a
        Dirac delta term (e.g. point or directional lights).

        Parameter ``ref`` (:py:obj:`mitsuba.Interaction3f`):
            A 3D reference location within the scene, which may influence the
            sampling process.

        Parameter ``ds`` (:py:obj:`mitsuba.DirectionSample3f`):
            A direction sampling record, which specifies the query location.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Color3f`:
            The incident radiance and discrete or solid angle density of the
            sample.

    .. py:method:: mitsuba.Scene.integrator()

        Return the scene's integrator

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Scene.invert_silhouette_sample(self, ss, active=True)

        Map a silhouette segment to a point in boundary sample space

        This method is the inverse of sample_silhouette(). The mapping from
        boundary sample space to boundary segments is bijective.

        Parameter ``ss`` (:py:obj:`mitsuba.SilhouetteSample3f`):
            The sampled boundary segment

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Point3f`:
            The corresponding boundary sample space point

    .. py:method:: mitsuba.Scene.pdf_emitter(self, index, active=True)

        Evaluate the discrete probability of the sample_emitter() technique
        for the given a emitter index.

        Parameter ``index`` (drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Scene.pdf_emitter_direction(self, ref, ds, active=True)

        Evaluate the PDF of direct illumination sampling

        This function evaluates the probability density (per unit solid angle)
        of the sampling technique implemented by the sample_emitter_direct()
        function. The returned probability will always be zero when the
        emission profile contains a Dirac delta term (e.g. point or
        directional emitters/sensors).

        Parameter ``ref`` (:py:obj:`mitsuba.Interaction3f`):
            A 3D reference location within the scene, which may influence the
            sampling process.

        Parameter ``ds`` (:py:obj:`mitsuba.DirectionSample3f`):
            A direction sampling record, which specifies the query location.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            The solid angle density of the sample

    .. py:method:: mitsuba.Scene.ray_intersect(self, ray, active=True)

        Overloaded function.

        1. ``ray_intersect(self, ray: :py:obj:`mitsuba.Ray3f`, active: drjit.llvm.ad.Bool = True) -> :py:obj:`mitsuba.SurfaceInteraction3f```

        Intersect a ray with the shapes comprising the scene and return a
        detailed data structure describing the intersection, if one is found.

        In vectorized variants of Mitsuba (``cuda_*`` or ``llvm_*``), the
        function processes arrays of rays and returns arrays of surface
        interactions following the usual conventions.

        This method is a convenience wrapper of the generalized version of
        ``ray_intersect``() below. It assumes that incoherent rays are being
        traced, and that the user desires access to all fields of the
        SurfaceInteraction. In other words, it simply invokes the general
        ``ray_intersect``() overload with ``coherent=false`` and ``ray_flags``
        equal to RayFlags::All.

        Parameter ``ray`` (:py:obj:`mitsuba.Ray3f`):
            A 3D ray including maximum extent (Ray::maxt) and time (Ray::time)
            information, which matters when the shapes are in motion

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.SurfaceInteraction3f`:
            A detailed surface interaction record. Its ``is_valid()`` method
            should be queried to check if an intersection was actually found.

        2. ``ray_intersect(self, ray: :py:obj:`mitsuba.Ray3f`, ray_flags: int, coherent: drjit.llvm.ad.Bool, active: drjit.llvm.ad.Bool = True) -> :py:obj:`mitsuba.SurfaceInteraction3f```

        Intersect a ray with the shapes comprising the scene and return a
        detailed data structure describing the intersection, if one is found

        In vectorized variants of Mitsuba (``cuda_*`` or ``llvm_*``), the
        function processes arrays of rays and returns arrays of surface
        interactions following the usual conventions.

        This generalized ray intersection method exposes two additional flags
        to control the intersection process. Internally, it is split into two
        steps:

        <ol>

        * Finding a PreliminaryInteraction using the ray tracing backend
        underlying the current variant (i.e., Mitsuba's builtin kd-tree,
        Embree, or OptiX). This is done using the ray_intersect_preliminary()
        function that is also available directly below (and preferable if a
        full SurfaceInteraction is not needed.).

        * Expanding the PreliminaryInteraction into a full SurfaceInteraction
        (this part happens within Mitsuba/Dr.Jit and tracks derivative
        information in AD variants of the system).

        </ol>

        The SurfaceInteraction data structure is large, and computing its
        contents in the second step requires a non-trivial amount of
        computation and sequence of memory accesses. The ``ray_flags``
        parameter can be used to specify that only a sub-set of the full
        intersection data structure actually needs to be computed, which can
        improve performance.

        In the context of differentiable rendering, the ``ray_flags``
        parameter also influences how derivatives propagate between the input
        ray, the shape parameters, and the computed intersection (see
        RayFlags::FollowShape and RayFlags::DetachShape for details on this).
        The default, RayFlags::All, propagates derivatives through all steps
        of the intersection computation.

        The ``coherent`` flag is a hint that can improve performance in the
        first step of finding the PreliminaryInteraction if the input set of
        rays is coherent (e.g., when they are generated by
        Sensor::sample_ray(), which means that adjacent rays will traverse
        essentially the same region of space). This flag is currently only
        used by the combination of ``llvm_*`` variants and the Embree ray
        tracing backend.

        Parameter ``ray`` (:py:obj:`mitsuba.Ray3f`):
            A 3D ray including maximum extent (Ray::maxt) and time (Ray::time)
            information, which matters when the shapes are in motion

        Parameter ``ray_flags``:
            An integer combining flag bits from RayFlags (merged using binary
            or).

        Parameter ``coherent``:
            Setting this flag to ``True`` can noticeably improve performance
            when ``ray`` contains a coherent set of rays (e.g. primary camera
            rays), and when using ``llvm_*`` variants of the renderer along
            with Embree. It has no effect in scalar or CUDA/OptiX variants.

        Returns → :py:obj:`mitsuba.SurfaceInteraction3f`:
            A detailed surface interaction record. Its ``is_valid()`` method
            should be queried to check if an intersection was actually found.

    .. py:method:: mitsuba.Scene.ray_intersect_preliminary(self, ray, coherent=False, active=True)

        Intersect a ray with the shapes comprising the scene and return
        preliminary information, if one is found

        This function invokes the ray tracing backend underlying the current
        variant (i.e., Mitsuba's builtin kd-tree, Embree, or OptiX) and
        returns preliminary intersection information consisting of

        * the ray distance up to the intersection (if one is found).

        * the intersected shape and primitive index.

        * local UV coordinates of the intersection within the primitive.

        * A pointer to the intersected shape or instance.

        The information is only preliminary at this point, because it lacks
        various other information (geometric and shading frame, texture
        coordinates, curvature, etc.) that is generally needed by shading
        models. In variants of Mitsuba that perform automatic differentiation,
        it is important to know that computation done by the ray tracing
        backend is not reflected in Dr.Jit's computation graph. The
        ray_intersect() method will re-evaluate certain parts of the
        computation with derivative tracking to rectify this.

        In vectorized variants of Mitsuba (``cuda_*`` or ``llvm_*``), the
        function processes arrays of rays and returns arrays of preliminary
        intersection records following the usual conventions.

        The ``coherent`` flag is a hint that can improve performance if the
        input set of rays is coherent (e.g., when they are generated by
        Sensor::sample_ray(), which means that adjacent rays will traverse
        essentially the same region of space). This flag is currently only
        used by the combination of ``llvm_*`` variants and the Embree ray
        intersector.

        Parameter ``ray`` (:py:obj:`mitsuba.Ray3f`):
            A 3D ray including maximum extent (Ray::maxt) and time (Ray::time)
            information, which matters when the shapes are in motion

        Parameter ``coherent`` (drjit.llvm.ad.Bool):
            Setting this flag to ``True`` can noticeably improve performance
            when ``ray`` contains a coherent set of rays (e.g. primary camera
            rays), and when using ``llvm_*`` variants of the renderer along
            with Embree. It has no effect in scalar or CUDA/OptiX variants.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.PreliminaryIntersection3f`:
            A preliminary surface interaction record. Its ``is_valid()``
            method should be queried to check if an intersection was actually
            found.

    .. py:method:: mitsuba.Scene.ray_test(self, ray, active=True)

        Overloaded function.

        1. ``ray_test(self, ray: :py:obj:`mitsuba.Ray3f`, active: drjit.llvm.ad.Bool = True) -> drjit.llvm.ad.Bool``

        Intersect a ray with the shapes comprising the scene and return a
        boolean specifying whether or not an intersection was found.

        In vectorized variants of Mitsuba (``cuda_*`` or ``llvm_*``), the
        function processes arrays of rays and returns arrays of booleans
        following the usual conventions.

        Testing for the mere presence of intersections is considerably faster
        than finding an actual intersection, hence this function should be
        preferred over ray_intersect() when geometric information about the
        first visible intersection is not needed.

        This method is a convenience wrapper of the generalized version of
        ``ray_test``() below, which assumes that incoherent rays are being
        traced. In other words, it simply invokes the general ``ray_test``()
        overload with ``coherent=false``.

        Parameter ``ray`` (:py:obj:`mitsuba.Ray3f`):
            A 3D ray including maximum extent (Ray::maxt) and time (Ray::time)
            information, which matters when the shapes are in motion

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Bool:
            ``True`` if an intersection was found

        2. ``ray_test(self, ray: :py:obj:`mitsuba.Ray3f`, coherent: drjit.llvm.ad.Bool, active: drjit.llvm.ad.Bool = True) -> drjit.llvm.ad.Bool``

        Intersect a ray with the shapes comprising the scene and return a
        boolean specifying whether or not an intersection was found.

        In vectorized variants of Mitsuba (``cuda_*`` or ``llvm_*``), the
        function processes arrays of rays and returns arrays of booleans
        following the usual conventions.

        Testing for the mere presence of intersections is considerably faster
        than finding an actual intersection, hence this function should be
        preferred over ray_intersect() when geometric information about the
        first visible intersection is not needed.

        The ``coherent`` flag is a hint that can improve performance in the
        first step of finding the PreliminaryInteraction if the input set of
        rays is coherent, which means that adjacent rays will traverse
        essentially the same region of space. This flag is currently only used
        by the combination of ``llvm_*`` variants and the Embree ray tracing
        backend.

        Parameter ``ray`` (:py:obj:`mitsuba.Ray3f`):
            A 3D ray including maximum extent (Ray::maxt) and time (Ray::time)
            information, which matters when the shapes are in motion

        Parameter ``coherent``:
            Setting this flag to ``True`` can noticeably improve performance
            when ``ray`` contains a coherent set of rays (e.g. primary camera
            rays), and when using ``llvm_*`` variants of the renderer along
            with Embree. It has no effect in scalar or CUDA/OptiX variants.

        Returns → drjit.llvm.ad.Bool:
            ``True`` if an intersection was found

    .. py:method:: mitsuba.Scene.sample_emitter(self, sample, active=True)

        Sample one emitter in the scene and rescale the input sample for
        reuse.

        Currently, the sampling scheme implemented by the Scene class is very
        simplistic (uniform).

        Parameter ``sample`` (drjit.llvm.ad.Float):
            A uniformly distributed number in [0, 1).

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[drjit.llvm.ad.UInt, drjit.llvm.ad.Float, drjit.llvm.ad.Float]:
            The index of the chosen emitter along with the sampling weight
            (equal to the inverse PDF), and the transformed random sample for
            reuse.

    .. py:method:: mitsuba.Scene.sample_emitter_direction(self, ref, sample, test_visibility=True, active=True)

        Direct illumination sampling routine

        This method implements stochastic connections to emitters, which is
        variously known as *emitter sampling*, *direct illumination sampling*,
        or *next event estimation*.

        The function expects a 3D reference location ``ref`` as input, which
        may influence the sampling process. Normally, this would be the
        location of a surface position being shaded. Ideally, the
        implementation of this function should then draw samples proportional
        to the scene's emission profile and the inverse square distance
        between the reference point and the sampled emitter position. However,
        approximations are acceptable as long as these are reflected in the
        returned Monte Carlo sampling weight.

        Parameter ``ref`` (:py:obj:`mitsuba.Interaction3f`):
            A 3D reference location within the scene, which may influence the
            sampling process.

        Parameter ``sample`` (:py:obj:`mitsuba.Point2f`):
            A uniformly distributed 2D random variate

        Parameter ``test_visibility`` (bool):
            When set to ``True``, a shadow ray will be cast to ensure that the
            sampled emitter position and the reference point are mutually
            visible.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.DirectionSample3f`, :py:obj:`mitsuba.Color3f`]:
            A tuple ``(ds, spec)`` where

        * ``ds`` is a fully populated DirectionSample3f data structure, which
        provides further detail about the sampled emitter position (e.g. its
        surface normal, solid angle density, whether Dirac delta distributions
        were involved, etc.)

        *

        * ``spec`` is a Monte Carlo sampling weight specifying the ratio of
        the radiance incident from the emitter and the sample probability per
        unit solid angle.

    .. py:method:: mitsuba.Scene.sample_emitter_ray(self, time, sample1, sample2, sample3, active)

        Sample a ray according to the emission profile of scene emitters

        This function combines both steps of choosing a ray origin on a light
        source and an outgoing ray direction. It does not return any auxiliary
        sampling information and is mainly meant to be used by unidirectional
        rendering techniques like particle tracing.

        Sampling is ideally perfectly proportional to the emission profile,
        though approximations are acceptable as long as these are reflected in
        the returned Monte Carlo sampling weight.

        Parameter ``time`` (drjit.llvm.ad.Float):
            The scene time associated with the ray to be sampled.

        Parameter ``sample1`` (drjit.llvm.ad.Float):
            A uniformly distributed 1D value that is used to sample the
            spectral dimension of the emission profile.

        Parameter ``sample2`` (:py:obj:`mitsuba.Point2f`):
            A uniformly distributed sample on the domain ``[0,1]^2``.

        Parameter ``sample3`` (:py:obj:`mitsuba.Point2f`):
            A uniformly distributed sample on the domain ``[0,1]^2``.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Ray3f`, :py:obj:`mitsuba.Color3f`, :py:obj:`mitsuba.EmitterPtr`]:
            A tuple ``(ray, weight, emitter, radiance)``, where

        * ``ray`` is the sampled ray (e.g. starting on the surface of an area
        emitter)

        * ``weight`` returns the emitted radiance divided by the spatio-
        directional sampling density

        * ``emitter`` is a pointer specifying the sampled emitter

    .. py:method:: mitsuba.Scene.sample_silhouette(self, sample, flags, active=True)

        Map a point sample in boundary sample space to a silhouette segment

        This method will sample a SilhouetteSample3f object from all the
        shapes in the scene that are being differentiated and have non-zero
        sampling weight (see Shape::silhouette_sampling_weight).

        Parameter ``sample`` (:py:obj:`mitsuba.Point3f`):
            The boundary space sample (a point in the unit cube).

        Parameter ``flags`` (int):
            Flags to select the type of silhouettes to sample from (see
            DiscontinuityFlags). Multiple types of discontinuities can be
            sampled in a single call. If a single type of silhouette is
            specified, shapes that do not have that types might still be
            sampled. In which case, the SilhouetteSample3f field
            ``discontinuity_type`` will be DiscontinuityFlags::Empty.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.SilhouetteSample3f`:
            Silhouette sample record.

    .. py:method:: mitsuba.Scene.sensors()

        Return the list of sensors

        Returns → list:
            *no description available*

    .. py:method:: mitsuba.Scene.sensors_dr()

        Return the list of sensors as a Dr.Jit array

        Returns → :py:obj:`mitsuba.SensorPtr`:
            *no description available*

    .. py:method:: mitsuba.Scene.shapes()

        Return the list of shapes

        Returns → list:
            *no description available*

    .. py:method:: mitsuba.Scene.shapes_dr()

        Return the list of shapes as a Dr.Jit array

        Returns → :py:obj:`mitsuba.ShapePtr`:
            *no description available*

    .. py:method:: mitsuba.Scene.shapes_grad_enabled()

        Specifies whether any of the scene's shape parameters have gradient
        tracking enabled

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Scene.silhouette_shapes()

        Return the list of shapes that can have their silhouette sampled

        Returns → list:
            *no description available*

.. py:class:: mitsuba.SceneParameters

    Dictionary-like object that references various parameters used in a Mitsuba
    scene graph. Parameters can be read and written using standard syntax
    (``parameter_map[key]``). The class exposes several non-standard functions,
    specifically :py:meth:`~:py:obj:`mitsuba.SceneParameters.torch`()`,
    :py:meth:`~:py:obj:`mitsuba.SceneParameters.update`()`, and
    :py:meth:`~:py:obj:`mitsuba.SceneParameters.keep`()`.

    .. py:method:: __init__()

        Private constructor (use
        :py:func:`mitsuba.traverse()` instead)

        
    .. py:method:: mitsuba.SceneParameters.items()

        Returns → a set-like object providing a view on D's items:
            *no description available*

    .. py:method:: mitsuba.SceneParameters.keys()

        Returns → a set-like object providing a view on D's keys:
            *no description available*

    .. py:method:: mitsuba.SceneParameters.flags(key)

        Return parameter flags

        Parameter ``key`` (str):
            *no description available*

    .. py:method:: mitsuba.SceneParameters.set_dirty(key)

        Marks a specific parameter and its parent objects as dirty. A subsequent call
        to :py:meth:`~:py:obj:`mitsuba.SceneParameters.update`()` will refresh their internal
        state.

        This method should rarely be called explicitly. The
        :py:class:`~:py:obj:`mitsuba.SceneParameters`` will detect most operations on
        its values and automatically flag them as dirty. A common exception to
        the detection mechanism is the :py:meth:`~drjit.scatter` operation which
        needs an explicit call to :py:meth:`~:py:obj:`mitsuba.SceneParameters.set_dirty`()`.

        Parameter ``key`` (str):
            *no description available*

    .. py:method:: mitsuba.SceneParameters.update(values=None)

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

        Parameter ``values`` (dict):
            *no description available*

        Returns → list[tuple[Any, set]]:
            *no description available*

    .. py:method:: mitsuba.SceneParameters.keep(keys)

        Reduce the size of the dictionary by only keeping elements,
        whose keys are defined by 'keys'.

        Parameter ``keys`` (``None``, ``str``, ``[str]``):
            Specifies which parameters should be kept. Regex are supported to define
            a subset of parameters at once. If set to ``None``, all differentiable
            scene parameters will be loaded.

        Parameter ``keys`` (None | str | list[str]):
            *no description available*

        Returns → None:
            *no description available*

.. py:class:: mitsuba.ScopedSetThreadEnvironment

    RAII-style class to temporarily switch to another thread's logger/file
    resolver

    .. py:method:: __init__(self, arg)

        Parameter ``arg`` (:py:obj:`mitsuba.ThreadEnvironment`, /):
            *no description available*


.. py:class:: mitsuba.Sensor

    Base class: :py:obj:`mitsuba.Endpoint`

    .. py:method:: mitsuba.Sensor.eval(self, si, active=True)

        Given a ray-surface intersection, return the emitted radiance or
        importance traveling along the reverse direction

        This function is e.g. used when an area light source has been hit by a
        ray in a path tracing-style integrator, and it subsequently needs to
        be queried for the emitted radiance along the negative ray direction.
        The default implementation throws an exception, which states that the
        method is not implemented.

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            An intersect record that specifies both the query position and
            direction (using the ``si.wi`` field)

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Color3f`:
            The emitted radiance or importance

    .. py:method:: mitsuba.Sensor.eval_direction(self, it, ds, active=True)

        Re-evaluate the incident direct radiance/importance of the
        sample_direction() method.

        This function re-evaluates the incident direct radiance or importance
        and sample probability due to the endpoint so that division by
        ``ds.pdf`` equals the sampling weight returned by sample_direction().
        This may appear redundant, and indeed such a function would not find
        use in "normal" rendering algorithms.

        However, the ability to re-evaluate the contribution of a generated
        sample is important for differentiable rendering. For example, we
        might want to track derivatives in the sampled direction (``ds.d``)
        without also differentiating the sampling technique.

        In contrast to pdf_direction(), evaluating this function can yield a
        nonzero result in the case of emission profiles containing a Dirac
        delta term (e.g. point or directional lights).

        Parameter ``ref``:
            A 3D reference location within the scene, which may influence the
            sampling process.

        Parameter ``ds`` (:py:obj:`mitsuba.DirectionSample3f`):
            A direction sampling record, which specifies the query location.

        Parameter ``it`` (:py:obj:`mitsuba.Interaction3f`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Color3f`:
            The incident direct radiance/importance associated with the
            sample.

    .. py:method:: mitsuba.Sensor.film()

        Return the Film instance associated with this sensor

        Returns → :py:obj:`mitsuba.Film`:
            *no description available*

    .. py:method:: mitsuba.Sensor.get_shape()

        Return the shape, to which the emitter is currently attached

        Returns → :py:obj:`mitsuba.Shape`:
            *no description available*

    .. py:method:: mitsuba.Sensor.m_film
        :property:

        (self) -> :py:obj:`mitsuba.Film`

    .. py:method:: mitsuba.Sensor.m_needs_sample_2
        :property:

        (self) -> bool

    .. py:method:: mitsuba.Sensor.m_needs_sample_3
        :property:

        (self) -> bool

    .. py:method:: mitsuba.Sensor.needs_aperture_sample()

        Does the sampling technique require a sample for the aperture
        position?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Sensor.pdf_direction(self, it, ds, active=True)

        Evaluate the probability density of the *direct* sampling method
        implemented by the sample_direction() method.

        The returned probability will always be zero when the
        emission/sensitivity profile contains a Dirac delta term (e.g. point
        or directional emitters/sensors).

        Parameter ``ds`` (:py:obj:`mitsuba.DirectionSample3f`):
            A direct sampling record, which specifies the query location.

        Parameter ``it`` (:py:obj:`mitsuba.Interaction3f`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Sensor.pdf_position(self, ps, active=True)

        Evaluate the probability density of the position sampling method
        implemented by sample_position().

        In simple cases, this will be the reciprocal of the endpoint's surface
        area.

        Parameter ``ps`` (:py:obj:`mitsuba.PositionSample3f`):
            The sampled position record.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            The corresponding sampling density.

    .. py:method:: mitsuba.Sensor.sample_direction(self, it, sample, active=True)

        Given a reference point in the scene, sample a direction from the
        reference point towards the endpoint (ideally proportional to the
        emission/sensitivity profile)

        This operation is a generalization of direct illumination techniques
        to both emitters *and* sensors. A direction sampling method is given
        an arbitrary reference position in the scene and samples a direction
        from the reference point towards the endpoint (ideally proportional to
        the emission/sensitivity profile). This reduces the sampling domain
        from 4D to 2D, which often enables the construction of smarter
        specialized sampling techniques.

        Ideally, the implementation should importance sample the product of
        the emission profile and the geometry term between the reference point
        and the position on the endpoint.

        The default implementation throws an exception.

        Parameter ``ref``:
            A reference position somewhere within the scene.

        Parameter ``sample`` (:py:obj:`mitsuba.Point2f`):
            A uniformly distributed 2D point on the domain ``[0,1]^2``.

        Parameter ``it`` (:py:obj:`mitsuba.Interaction3f`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.DirectionSample3f`, :py:obj:`mitsuba.Color3f`]:
            A DirectionSample instance describing the generated sample along
            with a spectral importance weight.

    .. py:method:: mitsuba.Sensor.sample_position(self, time, sample, active=True)

        Importance sample the spatial component of the emission or importance
        profile of the endpoint.

        The default implementation throws an exception.

        Parameter ``time`` (drjit.llvm.ad.Float):
            The scene time associated with the position to be sampled.

        Parameter ``sample`` (:py:obj:`mitsuba.Point2f`):
            A uniformly distributed 2D point on the domain ``[0,1]^2``.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.PositionSample3f`, drjit.llvm.ad.Float]:
            A PositionSample instance describing the generated sample along
            with an importance weight.

    .. py:method:: mitsuba.Sensor.sample_ray(self, time, sample1, sample2, sample3, active=True)

        Importance sample a ray proportional to the endpoint's
        sensitivity/emission profile.

        The endpoint profile is a six-dimensional quantity that depends on
        time, wavelength, surface position, and direction. This function takes
        a given time value and five uniformly distributed samples on the
        interval [0, 1] and warps them so that the returned ray follows the
        profile. Any discrepancies between ideal and actual sampled profile
        are absorbed into a spectral importance weight that is returned along
        with the ray.

        Parameter ``time`` (drjit.llvm.ad.Float):
            The scene time associated with the ray to be sampled

        Parameter ``sample1`` (drjit.llvm.ad.Float):
            A uniformly distributed 1D value that is used to sample the
            spectral dimension of the emission profile.

        Parameter ``sample2`` (:py:obj:`mitsuba.Point2f`):
            A uniformly distributed sample on the domain ``[0,1]^2``. For
            sensor endpoints, this argument corresponds to the sample position
            in fractional pixel coordinates relative to the crop window of the
            underlying film. This argument is ignored if ``needs_sample_2() ==
            false``.

        Parameter ``sample3`` (:py:obj:`mitsuba.Point2f`):
            A uniformly distributed sample on the domain ``[0,1]^2``. For
            sensor endpoints, this argument determines the position on the
            aperture of the sensor. This argument is ignored if
            ``needs_sample_3() == false``.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Ray3f`, :py:obj:`mitsuba.Color3f`]:
            The sampled ray and (potentially spectrally varying) importance
            weights. The latter account for the difference between the profile
            and the actual used sampling density function.

    .. py:method:: mitsuba.Sensor.sample_ray_differential(self, time, sample1, sample2, sample3, active=True)

        Importance sample a ray differential proportional to the sensor's
        sensitivity profile.

        The sensor profile is a six-dimensional quantity that depends on time,
        wavelength, surface position, and direction. This function takes a
        given time value and five uniformly distributed samples on the
        interval [0, 1] and warps them so that the returned ray the profile.
        Any discrepancies between ideal and actual sampled profile are
        absorbed into a spectral importance weight that is returned along with
        the ray.

        In contrast to Endpoint::sample_ray(), this function returns
        differentials with respect to the X and Y axis in screen space.

        Parameter ``time`` (drjit.llvm.ad.Float):
            The scene time associated with the ray_differential to be sampled

        Parameter ``sample1`` (drjit.llvm.ad.Float):
            A uniformly distributed 1D value that is used to sample the
            spectral dimension of the sensitivity profile.

        Parameter ``sample2`` (:py:obj:`mitsuba.Point2f`):
            This argument corresponds to the sample position in fractional
            pixel coordinates relative to the crop window of the underlying
            film.

        Parameter ``sample3`` (:py:obj:`mitsuba.Point2f`):
            A uniformly distributed sample on the domain ``[0,1]^2``. This
            argument determines the position on the aperture of the sensor.
            This argument is ignored if ``needs_sample_3() == false``.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.RayDifferential3f`, :py:obj:`mitsuba.Color3f`]:
            The sampled ray differential and (potentially spectrally varying)
            importance weights. The latter account for the difference between
            the sensor profile and the actual used sampling density function.

    .. py:method:: mitsuba.Sensor.sample_wavelengths(self, si, sample, active=True)

        Importance sample a set of wavelengths according to the endpoint's
        sensitivity/emission spectrum.

        This function takes a uniformly distributed 1D sample and generates a
        sample that is approximately distributed according to the endpoint's
        spectral sensitivity/emission profile.

        For this, the input 1D sample is first replicated into
        ``Spectrum::Size`` separate samples using simple arithmetic
        transformations (see math::sample_shifted()), which can be interpreted
        as a type of Quasi-Monte-Carlo integration scheme. Following this, a
        standard technique (e.g. inverse transform sampling) is used to find
        the corresponding wavelengths. Any discrepancies between ideal and
        actual sampled profile are absorbed into a spectral importance weight
        that is returned along with the wavelengths.

        This function should not be called in RGB or monochromatic modes.

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            In the case of a spatially-varying spectral sensitivity/emission
            profile, this parameter conditions sampling on a specific spatial
            position. The ``si.uv`` field must be specified in this case.

        Parameter ``sample`` (drjit.llvm.ad.Float):
            A 1D uniformly distributed random variate

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Color0f`, :py:obj:`mitsuba.Color3f`]:
            The set of sampled wavelengths and (potentially spectrally
            varying) importance weights. The latter account for the difference
            between the profile and the actual used sampling density function.
            In the case of emitters, the weight will include the emitted
            radiance.

    .. py:method:: mitsuba.Sensor.sampler()

        Return the sensor's sample generator

        This is the *root* sampler, which will later be forked a number of
        times to provide each participating worker thread with its own
        instance (see Scene::sampler()). Therefore, this sampler should never
        be used for anything except creating forks.

        Returns → :py:obj:`mitsuba.Sampler`:
            *no description available*

    .. py:method:: mitsuba.Sensor.shutter_open()

        Return the time value of the shutter opening event

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.Sensor.shutter_open_time()

        Return the length, for which the shutter remains open

        Returns → float:
            *no description available*

.. py:class:: mitsuba.SensorPtr

    .. py:method:: mitsuba.SensorPtr.eval(self, si, active=True)

        Given a ray-surface intersection, return the emitted radiance or
        importance traveling along the reverse direction

        This function is e.g. used when an area light source has been hit by a
        ray in a path tracing-style integrator, and it subsequently needs to
        be queried for the emitted radiance along the negative ray direction.
        The default implementation throws an exception, which states that the
        method is not implemented.

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            An intersect record that specifies both the query position and
            direction (using the ``si.wi`` field)

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Color3f`:
            The emitted radiance or importance

    .. py:method:: mitsuba.SensorPtr.eval_direction(self, it, ds, active=True)

        Re-evaluate the incident direct radiance/importance of the
        sample_direction() method.

        This function re-evaluates the incident direct radiance or importance
        and sample probability due to the endpoint so that division by
        ``ds.pdf`` equals the sampling weight returned by sample_direction().
        This may appear redundant, and indeed such a function would not find
        use in "normal" rendering algorithms.

        However, the ability to re-evaluate the contribution of a generated
        sample is important for differentiable rendering. For example, we
        might want to track derivatives in the sampled direction (``ds.d``)
        without also differentiating the sampling technique.

        In contrast to pdf_direction(), evaluating this function can yield a
        nonzero result in the case of emission profiles containing a Dirac
        delta term (e.g. point or directional lights).

        Parameter ``ref``:
            A 3D reference location within the scene, which may influence the
            sampling process.

        Parameter ``ds`` (:py:obj:`mitsuba.DirectionSample3f`):
            A direction sampling record, which specifies the query location.

        Parameter ``it`` (:py:obj:`mitsuba.Interaction3f`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Color3f`:
            The incident direct radiance/importance associated with the
            sample.

    .. py:method:: mitsuba.SensorPtr.get_shape()

        Return the shape, to which the emitter is currently attached

        Returns → :py:obj:`mitsuba.ShapePtr`:
            *no description available*

    .. py:method:: mitsuba.SensorPtr.pdf_direction(self, it, ds, active=True)

        Evaluate the probability density of the *direct* sampling method
        implemented by the sample_direction() method.

        The returned probability will always be zero when the
        emission/sensitivity profile contains a Dirac delta term (e.g. point
        or directional emitters/sensors).

        Parameter ``ds`` (:py:obj:`mitsuba.DirectionSample3f`):
            A direct sampling record, which specifies the query location.

        Parameter ``it`` (:py:obj:`mitsuba.Interaction3f`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.SensorPtr.pdf_position(self, ps, active=True)

        Evaluate the probability density of the position sampling method
        implemented by sample_position().

        In simple cases, this will be the reciprocal of the endpoint's surface
        area.

        Parameter ``ps`` (:py:obj:`mitsuba.PositionSample3f`):
            The sampled position record.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            The corresponding sampling density.

    .. py:method:: mitsuba.SensorPtr.sample_direction(self, it, sample, active=True)

        Given a reference point in the scene, sample a direction from the
        reference point towards the endpoint (ideally proportional to the
        emission/sensitivity profile)

        This operation is a generalization of direct illumination techniques
        to both emitters *and* sensors. A direction sampling method is given
        an arbitrary reference position in the scene and samples a direction
        from the reference point towards the endpoint (ideally proportional to
        the emission/sensitivity profile). This reduces the sampling domain
        from 4D to 2D, which often enables the construction of smarter
        specialized sampling techniques.

        Ideally, the implementation should importance sample the product of
        the emission profile and the geometry term between the reference point
        and the position on the endpoint.

        The default implementation throws an exception.

        Parameter ``ref``:
            A reference position somewhere within the scene.

        Parameter ``sample`` (:py:obj:`mitsuba.Point2f`):
            A uniformly distributed 2D point on the domain ``[0,1]^2``.

        Parameter ``it`` (:py:obj:`mitsuba.Interaction3f`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.DirectionSample3f`, :py:obj:`mitsuba.Color3f`]:
            A DirectionSample instance describing the generated sample along
            with a spectral importance weight.

    .. py:method:: mitsuba.SensorPtr.sample_position(self, time, sample, active=True)

        Importance sample the spatial component of the emission or importance
        profile of the endpoint.

        The default implementation throws an exception.

        Parameter ``time`` (drjit.llvm.ad.Float):
            The scene time associated with the position to be sampled.

        Parameter ``sample`` (:py:obj:`mitsuba.Point2f`):
            A uniformly distributed 2D point on the domain ``[0,1]^2``.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.PositionSample3f`, drjit.llvm.ad.Float]:
            A PositionSample instance describing the generated sample along
            with an importance weight.

    .. py:method:: mitsuba.SensorPtr.sample_ray(self, time, sample1, sample2, sample3, active=True)

        Importance sample a ray proportional to the endpoint's
        sensitivity/emission profile.

        The endpoint profile is a six-dimensional quantity that depends on
        time, wavelength, surface position, and direction. This function takes
        a given time value and five uniformly distributed samples on the
        interval [0, 1] and warps them so that the returned ray follows the
        profile. Any discrepancies between ideal and actual sampled profile
        are absorbed into a spectral importance weight that is returned along
        with the ray.

        Parameter ``time`` (drjit.llvm.ad.Float):
            The scene time associated with the ray to be sampled

        Parameter ``sample1`` (drjit.llvm.ad.Float):
            A uniformly distributed 1D value that is used to sample the
            spectral dimension of the emission profile.

        Parameter ``sample2`` (:py:obj:`mitsuba.Point2f`):
            A uniformly distributed sample on the domain ``[0,1]^2``. For
            sensor endpoints, this argument corresponds to the sample position
            in fractional pixel coordinates relative to the crop window of the
            underlying film. This argument is ignored if ``needs_sample_2() ==
            false``.

        Parameter ``sample3`` (:py:obj:`mitsuba.Point2f`):
            A uniformly distributed sample on the domain ``[0,1]^2``. For
            sensor endpoints, this argument determines the position on the
            aperture of the sensor. This argument is ignored if
            ``needs_sample_3() == false``.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Ray3f`, :py:obj:`mitsuba.Color3f`]:
            The sampled ray and (potentially spectrally varying) importance
            weights. The latter account for the difference between the profile
            and the actual used sampling density function.

    .. py:method:: mitsuba.SensorPtr.sample_ray_differential(self, time, sample1, sample2, sample3, active=True)

        Importance sample a ray differential proportional to the sensor's
        sensitivity profile.

        The sensor profile is a six-dimensional quantity that depends on time,
        wavelength, surface position, and direction. This function takes a
        given time value and five uniformly distributed samples on the
        interval [0, 1] and warps them so that the returned ray the profile.
        Any discrepancies between ideal and actual sampled profile are
        absorbed into a spectral importance weight that is returned along with
        the ray.

        In contrast to Endpoint::sample_ray(), this function returns
        differentials with respect to the X and Y axis in screen space.

        Parameter ``time`` (drjit.llvm.ad.Float):
            The scene time associated with the ray_differential to be sampled

        Parameter ``sample1`` (drjit.llvm.ad.Float):
            A uniformly distributed 1D value that is used to sample the
            spectral dimension of the sensitivity profile.

        Parameter ``sample2`` (:py:obj:`mitsuba.Point2f`):
            This argument corresponds to the sample position in fractional
            pixel coordinates relative to the crop window of the underlying
            film.

        Parameter ``sample3`` (:py:obj:`mitsuba.Point2f`):
            A uniformly distributed sample on the domain ``[0,1]^2``. This
            argument determines the position on the aperture of the sensor.
            This argument is ignored if ``needs_sample_3() == false``.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.RayDifferential3f`, :py:obj:`mitsuba.Color3f`]:
            The sampled ray differential and (potentially spectrally varying)
            importance weights. The latter account for the difference between
            the sensor profile and the actual used sampling density function.

    .. py:method:: mitsuba.SensorPtr.sample_wavelengths(self, si, sample, active=True)

        Importance sample a set of wavelengths according to the endpoint's
        sensitivity/emission spectrum.

        This function takes a uniformly distributed 1D sample and generates a
        sample that is approximately distributed according to the endpoint's
        spectral sensitivity/emission profile.

        For this, the input 1D sample is first replicated into
        ``Spectrum::Size`` separate samples using simple arithmetic
        transformations (see math::sample_shifted()), which can be interpreted
        as a type of Quasi-Monte-Carlo integration scheme. Following this, a
        standard technique (e.g. inverse transform sampling) is used to find
        the corresponding wavelengths. Any discrepancies between ideal and
        actual sampled profile are absorbed into a spectral importance weight
        that is returned along with the wavelengths.

        This function should not be called in RGB or monochromatic modes.

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            In the case of a spatially-varying spectral sensitivity/emission
            profile, this parameter conditions sampling on a specific spatial
            position. The ``si.uv`` field must be specified in this case.

        Parameter ``sample`` (drjit.llvm.ad.Float):
            A 1D uniformly distributed random variate

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Color0f`, :py:obj:`mitsuba.Color3f`]:
            The set of sampled wavelengths and (potentially spectrally
            varying) importance weights. The latter account for the difference
            between the profile and the actual used sampling density function.
            In the case of emitters, the weight will include the emitted
            radiance.

.. py:class:: mitsuba.Shape

    Base class: :py:obj:`mitsuba.Object`

    Forward declaration for `SilhouetteSample`

    .. py:method:: mitsuba.Shape.bbox()

        Overloaded function.

        1. ``bbox(self) -> :py:obj:`mitsuba.ScalarBoundingBox3f```

        Return an axis aligned box that bounds all shape primitives (including
        any transformations that may have been applied to them)

        2. ``bbox(self, index: int) -> :py:obj:`mitsuba.ScalarBoundingBox3f```

        Return an axis aligned box that bounds a single shape primitive
        (including any transformations that may have been applied to it)

        Remark:
            The default implementation simply calls bbox()

        3. ``bbox(self, index: int, clip: :py:obj:`mitsuba.ScalarBoundingBox3f`) -> :py:obj:`mitsuba.ScalarBoundingBox3f```

        Return an axis aligned box that bounds a single shape primitive after
        it has been clipped to another bounding box.

        This is extremely important to construct high-quality kd-trees. The
        default implementation just takes the bounding box returned by
        bbox(ScalarIndex index) and clips it to *clip*.

        Returns → :py:obj:`mitsuba.ScalarBoundingBox3f`:
            *no description available*

    .. py:method:: mitsuba.Shape.bsdf()

        Return the shape's BSDF

        Returns → :py:obj:`mitsuba.BSDF`:
            *no description available*

    .. py:method:: mitsuba.Shape.compute_surface_interaction(self, ray, pi, ray_flags=14, active=True)

        Compute and return detailed information related to a surface
        interaction

        The implementation should at most compute the fields ``p``, ``uv``,
        ``n``, ``sh_frame``.n, ``dp_du``, ``dp_dv``, ``dn_du`` and ``dn_dv``.
        The ``flags`` parameter specifies which of those fields should be
        computed.

        The fields ``t``, ``time``, ``wavelengths``, ``shape``,
        ``prim_index``, ``instance``, will already have been initialized by
        the caller. The field ``wi`` is initialized by the caller following
        the call to compute_surface_interaction(), and ``duv_dx``, and
        ``duv_dy`` are left uninitialized.

        Parameter ``ray`` (:py:obj:`mitsuba.Ray3f`):
            Ray associated with the ray intersection

        Parameter ``pi`` (:py:obj:`mitsuba.PreliminaryIntersection3f`):
            Data structure carrying information about the ray intersection

        Parameter ``ray_flags`` (int):
            Flags specifying which information should be computed

        Parameter ``recursion_depth``:
            Integer specifying the recursion depth for nested virtual function
            call to this method (e.g. used for instancing).

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.SurfaceInteraction3f`:
            A data structure containing the detailed information

    .. py:method:: mitsuba.Shape.differential_motion(self, si, active=True)

        Return the attached (AD) point on the shape's surface

        This method is only useful when using automatic differentiation. The
        immediate/primal return value of this method is exactly equal to
        \`si.p\`.

        The input `si` does not need to be explicitly detached, it is done by
        the method itself.

        If the shape cannot be differentiated, this method will return the
        detached input point.

        note:: The returned attached point is exactly the same as a point
        which is computed by calling compute_surface_interaction with the
        RayFlags::FollowShape flag.

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            The surface point for which the function will be evaluated.

        Not all fields of the object need to be filled. Only the `prim_index`,
        `p` and `uv` fields are required. Certain shapes will only use a
        subset of these.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Point3f`:
            The same surface point as the input but attached (AD) to the
            shape's parameters.

    .. py:method:: mitsuba.Shape.effective_primitive_count()

        Return the number of primitives (triangles, hairs, ..) contributed to
        the scene by this shape

        Includes instanced geometry. The default implementation simply returns
        the same value as primitive_count().

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Shape.emitter()

        Return the area emitter associated with this shape (if any)

        Returns → :py:obj:`mitsuba.Emitter`:
            *no description available*

    .. py:method:: mitsuba.Shape.eval_attribute(self, name, si, active=True)

        Evaluate a specific shape attribute at the given surface interaction.

        Shape attributes are user-provided fields that provide extra
        information at an intersection. An example of this would be a per-
        vertex or per-face color on a triangle mesh.

        Parameter ``name`` (str):
            Name of the attribute to evaluate

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            Surface interaction associated with the query

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Color3f`:
            An unpolarized spectral power distribution or reflectance value

    .. py:method:: mitsuba.Shape.eval_attribute_1(self, name, si, active=True)

        Monochromatic evaluation of a shape attribute at the given surface
        interaction

        This function differs from eval_attribute() in that it provided raw
        access to scalar intensity/reflectance values without any color
        processing (e.g. spectral upsampling).

        Parameter ``name`` (str):
            Name of the attribute to evaluate

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            Surface interaction associated with the query

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            An scalar intensity or reflectance value

    .. py:method:: mitsuba.Shape.eval_attribute_3(self, name, si, active=True)

        Trichromatic evaluation of a shape attribute at the given surface
        interaction

        This function differs from eval_attribute() in that it provided raw
        access to RGB intensity/reflectance values without any additional
        color processing (e.g. RGB-to-spectral upsampling).

        Parameter ``name`` (str):
            Name of the attribute to evaluate

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            Surface interaction associated with the query

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Color3f`:
            An trichromatic intensity or reflectance value

    .. py:method:: mitsuba.Shape.eval_parameterization(self, uv, ray_flags=14, active=True)

        Parameterize the mesh using UV values

        This function maps a 2D UV value to a surface interaction data
        structure. Its behavior is only well-defined in regions where this
        mapping is bijective. The default implementation throws.

        Parameter ``uv`` (:py:obj:`mitsuba.Point2f`):
            *no description available*

        Parameter ``ray_flags`` (int):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.SurfaceInteraction3f`:
            *no description available*

    .. py:method:: mitsuba.Shape.exterior_medium()

        Return the medium that lies on the exterior of this shape

        Returns → :py:obj:`mitsuba.Medium`:
            *no description available*

    .. py:method:: mitsuba.Shape.has_attribute(self, name, active=True)

        Returns whether this shape contains the specified attribute.

        Parameter ``name`` (str):
            Name of the attribute

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.Shape.id()

        Return a string identifier

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.Shape.interior_medium()

        Return the medium that lies on the interior of this shape

        Returns → :py:obj:`mitsuba.Medium`:
            *no description available*

    .. py:method:: mitsuba.Shape.invert_silhouette_sample(self, ss, active=True)

        Map a silhouette segment to a point in boundary sample space

        This method is the inverse of sample_silhouette(). The mapping from/to
        boundary sample space to/from boundary segments is bijective.

        This method's behavior is undefined when used in non-JIT variants or
        when the shape is not being differentiated.

        Parameter ``ss`` (:py:obj:`mitsuba.SilhouetteSample3f`):
            The sampled boundary segment

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Point3f`:
            The corresponding boundary sample space point

    .. py:method:: mitsuba.Shape.is_emitter()

        Is this shape also an area emitter?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Shape.is_medium_transition()

        Does the surface of this shape mark a medium transition?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Shape.is_mesh()

        Is this shape a triangle mesh?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Shape.is_sensor()

        Is this shape also an area sensor?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Shape.parameters_grad_enabled()

        Return whether any shape's parameters that introduce visibility
        discontinuities require gradients (default return false)

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Shape.pdf_direction(self, it, ps, active=True)

        Query the probability density of sample_direction()

        Parameter ``it`` (:py:obj:`mitsuba.Interaction3f`):
            A reference position somewhere within the scene.

        Parameter ``ps`` (:py:obj:`mitsuba.DirectionSample3f`):
            A position record describing the sample in question

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            The probability density per unit solid angle

    .. py:method:: mitsuba.Shape.pdf_position(self, ps, active=True)

        Query the probability density of sample_position() for a particular
        point on the surface.

        Parameter ``ps`` (:py:obj:`mitsuba.PositionSample3f`):
            A position record describing the sample in question

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            The probability density per unit area

    .. py:method:: mitsuba.Shape.precompute_silhouette(self, viewpoint)

        Precompute the visible silhouette of this shape for a given viewpoint.

        This method is meant to be used for silhouettes that are shared
        between all threads, as is the case for primarily visible derivatives.

        The return values are respectively a list of indices and their
        corresponding weights. The semantic meaning of these indices is
        different for each shape. For example, a triangle mesh will return the
        indices of all of its edges that constitute its silhouette. These
        indices are meant to be re-used as an argument when calling
        sample_precomputed_silhouette.

        This method's behavior is undefined when used in non-JIT variants or
        when the shape is not being differentiated.

        Parameter ``viewpoint`` (:py:obj:`mitsuba.ScalarPoint3f`):
            The viewpoint which defines the silhouette of the shape

        Returns → tuple[drjit.llvm.ad.UInt, drjit.llvm.ad.Float]:
            A list of indices used by the shape internally to represent
            silhouettes, and a list of the same length containing the
            (unnormalized) weights associated to each index.

    .. py:method:: mitsuba.Shape.primitive_count()

        Returns the number of sub-primitives that make up this shape

        Remark:
            The default implementation simply returns ``1``

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Shape.primitive_silhouette_projection(self, viewpoint, si, flags, sample, active=True)

        Projects a point on the surface of the shape to its silhouette as seen
        from a specified viewpoint.

        This method only projects the `si.p` point within its primitive.

        Not all of the fields of the SilhouetteSample3f might be filled by
        this method. Each shape will at the very least fill its return value
        with enough information for it to be used by invert_silhouette_sample.

        The projection operation might not find the closest silhouette point
        to the given surface point. For example, it can be guided by a random
        number ``sample``. Not all shapes types need this random number, each
        shape implementation is free to define its own algorithm and
        guarantees about the projection operation.

        This method's behavior is undefined when used in non-JIT variants or
        when the shape is not being differentiated.

        Parameter ``viewpoint`` (:py:obj:`mitsuba.Point3f`):
            The viewpoint which defines the silhouette to project the point
            to.

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            The surface point which will be projected.

        Parameter ``flags`` (int):
            Flags to select the type of SilhouetteSample3f to generate from
            the projection. Only one type of discontinuity can be used per
            call.

        Parameter ``sample`` (drjit.llvm.ad.Float):
            A random number that can be used to define the projection
            operation.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.SilhouetteSample3f`:
            A boundary segment on the silhouette of the shape as seen from
            ``viewpoint``.

    .. py:method:: mitsuba.Shape.ray_intersect(self, ray, ray_flags=14, active=True)

        Test for an intersection and return detailed information

        This operation combines the prior ray_intersect_preliminary() and
        compute_surface_interaction() operations.

        Parameter ``ray`` (:py:obj:`mitsuba.Ray3f`):
            The ray to be tested for an intersection

        Parameter ``flags``:
            Describe how the detailed information should be computed

        Parameter ``ray_flags`` (int):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.SurfaceInteraction3f`:
            *no description available*

    .. py:method:: mitsuba.Shape.ray_intersect_preliminary(self, ray, prim_index=0, active=True)

        Fast ray intersection

        Efficiently test whether the shape is intersected by the given ray,
        and return preliminary information about the intersection if that is
        the case.

        If the intersection is deemed relevant (e.g. the closest to the ray
        origin), detailed intersection information can later be obtained via
        the create_surface_interaction() method.

        Parameter ``ray`` (:py:obj:`mitsuba.Ray3f`):
            The ray to be tested for an intersection

        Parameter ``prim_index`` (int):
            Index of the primitive to be intersected. This index is ignored by
            a shape that contains a single primitive. Otherwise, if no index
            is provided, the ray intersection will be performed on the shape's
            first primitive at index 0.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.PreliminaryIntersection3f`:
            *no description available*

    .. py:method:: mitsuba.Shape.ray_test(self, ray, active=True)

        Fast ray shadow test

        Efficiently test whether the shape is intersected by the given ray.

        No details about the intersection are returned, hence the function is
        only useful for visibility queries. For most shapes, the
        implementation will simply forward the call to
        ray_intersect_preliminary(). When the shape actually contains a nested
        kd-tree, some optimizations are possible.

        Parameter ``ray`` (:py:obj:`mitsuba.Ray3f`):
            The ray to be tested for an intersection

        Parameter ``prim_index``:
            Index of the primitive to be intersected

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.Shape.sample_direction(self, it, sample, active=True)

        Sample a direction towards this shape with respect to solid angles
        measured at a reference position within the scene

        An ideal implementation of this interface would achieve a uniform
        solid angle density within the surface region that is visible from the
        reference position ``it.p`` (though such an ideal implementation is
        usually neither feasible nor advisable due to poor efficiency).

        The function returns the sampled position and the inverse probability
        per unit solid angle associated with the sample.

        When the Shape subclass does not supply a custom implementation of
        this function, the Shape class reverts to a fallback approach that
        piggybacks on sample_position(). This will generally lead to a
        suboptimal sample placement and higher variance in Monte Carlo
        estimators using the samples.

        Parameter ``it`` (:py:obj:`mitsuba.Interaction3f`):
            A reference position somewhere within the scene.

        Parameter ``sample`` (:py:obj:`mitsuba.Point2f`):
            A uniformly distributed 2D point on the domain ``[0,1]^2``

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.DirectionSample3f`:
            A DirectionSample instance describing the generated sample

    .. py:method:: mitsuba.Shape.sample_position(self, time, sample, active=True)

        Sample a point on the surface of this shape

        The sampling strategy is ideally uniform over the surface, though
        implementations are allowed to deviate from a perfectly uniform
        distribution as long as this is reflected in the returned probability
        density.

        Parameter ``time`` (drjit.llvm.ad.Float):
            The scene time associated with the position sample

        Parameter ``sample`` (:py:obj:`mitsuba.Point2f`):
            A uniformly distributed 2D point on the domain ``[0,1]^2``

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.PositionSample3f`:
            A PositionSample instance describing the generated sample

    .. py:method:: mitsuba.Shape.sample_precomputed_silhouette(self, viewpoint, sample1, sample2, active=True)

        Samples a boundary segement on the shape's silhouette using
        precomputed information computed in precompute_silhouette.

        This method is meant to be used for silhouettes that are shared
        between all threads, as is the case for primarily visible derivatives.

        This method's behavior is undefined when used in non-JIT variants or
        when the shape is not being differentiated.

        Parameter ``viewpoint`` (:py:obj:`mitsuba.Point3f`):
            The viewpoint that was used for the precomputed silhouette
            information

        Parameter ``sample1`` (drjit.llvm.ad.UInt):
            A sampled index from the return values of precompute_silhouette

        Parameter ``sample2`` (drjit.llvm.ad.Float):
            A uniformly distributed sample in ``[0,1]``

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.SilhouetteSample3f`:
            A boundary segment on the silhouette of the shape as seen from
            ``viewpoint``.

    .. py:method:: mitsuba.Shape.sample_silhouette(self, sample, flags, active=True)

        Map a point sample in boundary sample space to a silhouette segment

        This method's behavior is undefined when used in non-JIT variants or
        when the shape is not being differentiated.

        Parameter ``sample`` (:py:obj:`mitsuba.Point3f`):
            The boundary space sample (a point in the unit cube).

        Parameter ``flags`` (int):
            Flags to select the type of silhouettes to sample from (see
            DiscontinuityFlags). Only one type of discontinuity can be sampled
            per call.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.SilhouetteSample3f`:
            Silhouette sample record.

    .. py:method:: mitsuba.Shape.sensor()

        Return the area sensor associated with this shape (if any)

        Returns → :py:obj:`mitsuba.Sensor`:
            *no description available*

    .. py:method:: mitsuba.Shape.shape_type()

        Returns the shape type ShapeType of this shape

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Shape.silhouette_discontinuity_types()

        //! @{ \name Silhouette sampling routines and other utilities

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Shape.silhouette_sampling_weight()

        Return this shape's sampling weight w.r.t. all shapes in the scene

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.Shape.surface_area()

        Return the shape's surface area.

        The function assumes that the object is not undergoing some kind of
        time-dependent scaling.

        The default implementation throws an exception.

        Returns → drjit.llvm.ad.Float:
            *no description available*

.. py:class:: mitsuba.ShapePtr

    .. py:method:: mitsuba.ShapePtr.bsdf()

        Return the shape's BSDF

        Returns → :py:obj:`mitsuba.BSDFPtr`:
            *no description available*

    .. py:method:: mitsuba.ShapePtr.compute_surface_interaction(self, ray, pi, ray_flags=14, active=True)

        Compute and return detailed information related to a surface
        interaction

        The implementation should at most compute the fields ``p``, ``uv``,
        ``n``, ``sh_frame``.n, ``dp_du``, ``dp_dv``, ``dn_du`` and ``dn_dv``.
        The ``flags`` parameter specifies which of those fields should be
        computed.

        The fields ``t``, ``time``, ``wavelengths``, ``shape``,
        ``prim_index``, ``instance``, will already have been initialized by
        the caller. The field ``wi`` is initialized by the caller following
        the call to compute_surface_interaction(), and ``duv_dx``, and
        ``duv_dy`` are left uninitialized.

        Parameter ``ray`` (:py:obj:`mitsuba.Ray3f`):
            Ray associated with the ray intersection

        Parameter ``pi`` (:py:obj:`mitsuba.PreliminaryIntersection3f`):
            Data structure carrying information about the ray intersection

        Parameter ``ray_flags`` (int):
            Flags specifying which information should be computed

        Parameter ``recursion_depth``:
            Integer specifying the recursion depth for nested virtual function
            call to this method (e.g. used for instancing).

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.SurfaceInteraction3f`:
            A data structure containing the detailed information

    .. py:method:: mitsuba.ShapePtr.differential_motion(self, si, active=True)

        Return the attached (AD) point on the shape's surface

        This method is only useful when using automatic differentiation. The
        immediate/primal return value of this method is exactly equal to
        \`si.p\`.

        The input `si` does not need to be explicitly detached, it is done by
        the method itself.

        If the shape cannot be differentiated, this method will return the
        detached input point.

        note:: The returned attached point is exactly the same as a point
        which is computed by calling compute_surface_interaction with the
        RayFlags::FollowShape flag.

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            The surface point for which the function will be evaluated.

        Not all fields of the object need to be filled. Only the `prim_index`,
        `p` and `uv` fields are required. Certain shapes will only use a
        subset of these.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Point3f`:
            The same surface point as the input but attached (AD) to the
            shape's parameters.

    .. py:method:: mitsuba.ShapePtr.emitter()

        Return the area emitter associated with this shape (if any)

        Returns → :py:obj:`mitsuba.EmitterPtr`:
            *no description available*

    .. py:method:: mitsuba.ShapePtr.eval_attribute(self, name, si, active=True)

        Evaluate a specific shape attribute at the given surface interaction.

        Shape attributes are user-provided fields that provide extra
        information at an intersection. An example of this would be a per-
        vertex or per-face color on a triangle mesh.

        Parameter ``name`` (str):
            Name of the attribute to evaluate

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            Surface interaction associated with the query

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Color3f`:
            An unpolarized spectral power distribution or reflectance value

    .. py:method:: mitsuba.ShapePtr.eval_attribute_1(self, name, si, active=True)

        Monochromatic evaluation of a shape attribute at the given surface
        interaction

        This function differs from eval_attribute() in that it provided raw
        access to scalar intensity/reflectance values without any color
        processing (e.g. spectral upsampling).

        Parameter ``name`` (str):
            Name of the attribute to evaluate

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            Surface interaction associated with the query

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            An scalar intensity or reflectance value

    .. py:method:: mitsuba.ShapePtr.eval_attribute_3(self, name, si, active=True)

        Trichromatic evaluation of a shape attribute at the given surface
        interaction

        This function differs from eval_attribute() in that it provided raw
        access to RGB intensity/reflectance values without any additional
        color processing (e.g. RGB-to-spectral upsampling).

        Parameter ``name`` (str):
            Name of the attribute to evaluate

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            Surface interaction associated with the query

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Color3f`:
            An trichromatic intensity or reflectance value

    .. py:method:: mitsuba.ShapePtr.eval_parameterization(self, uv, ray_flags=14, active=True)

        Parameterize the mesh using UV values

        This function maps a 2D UV value to a surface interaction data
        structure. Its behavior is only well-defined in regions where this
        mapping is bijective. The default implementation throws.

        Parameter ``uv`` (:py:obj:`mitsuba.Point2f`):
            *no description available*

        Parameter ``ray_flags`` (int):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.SurfaceInteraction3f`:
            *no description available*

    .. py:method:: mitsuba.ShapePtr.exterior_medium()

        Return the medium that lies on the exterior of this shape

        Returns → :py:obj:`mitsuba.MediumPtr`:
            *no description available*

    .. py:method:: mitsuba.ShapePtr.has_attribute(self, name, active=True)

        Returns whether this shape contains the specified attribute.

        Parameter ``name`` (str):
            Name of the attribute

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.ShapePtr.interior_medium()

        Return the medium that lies on the interior of this shape

        Returns → :py:obj:`mitsuba.MediumPtr`:
            *no description available*

    .. py:method:: mitsuba.ShapePtr.invert_silhouette_sample(self, ss, active=True)

        Map a silhouette segment to a point in boundary sample space

        This method is the inverse of sample_silhouette(). The mapping from/to
        boundary sample space to/from boundary segments is bijective.

        This method's behavior is undefined when used in non-JIT variants or
        when the shape is not being differentiated.

        Parameter ``ss`` (:py:obj:`mitsuba.SilhouetteSample3f`):
            The sampled boundary segment

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Point3f`:
            The corresponding boundary sample space point

    .. py:method:: mitsuba.ShapePtr.is_emitter()

        Is this shape also an area emitter?

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.ShapePtr.is_medium_transition()

        Does the surface of this shape mark a medium transition?

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.ShapePtr.is_mesh()

        Is this shape a triangle mesh?

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.ShapePtr.is_sensor()

        Is this shape also an area sensor?

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.ShapePtr.pdf_direction(self, it, ps, active=True)

        Query the probability density of sample_direction()

        Parameter ``it`` (:py:obj:`mitsuba.Interaction3f`):
            A reference position somewhere within the scene.

        Parameter ``ps`` (:py:obj:`mitsuba.DirectionSample3f`):
            A position record describing the sample in question

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            The probability density per unit solid angle

    .. py:method:: mitsuba.ShapePtr.pdf_position(self, ps, active=True)

        Query the probability density of sample_position() for a particular
        point on the surface.

        Parameter ``ps`` (:py:obj:`mitsuba.PositionSample3f`):
            A position record describing the sample in question

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            The probability density per unit area

    .. py:method:: mitsuba.ShapePtr.primitive_silhouette_projection(self, viewpoint, si, flags, sample, active=True)

        Projects a point on the surface of the shape to its silhouette as seen
        from a specified viewpoint.

        This method only projects the `si.p` point within its primitive.

        Not all of the fields of the SilhouetteSample3f might be filled by
        this method. Each shape will at the very least fill its return value
        with enough information for it to be used by invert_silhouette_sample.

        The projection operation might not find the closest silhouette point
        to the given surface point. For example, it can be guided by a random
        number ``sample``. Not all shapes types need this random number, each
        shape implementation is free to define its own algorithm and
        guarantees about the projection operation.

        This method's behavior is undefined when used in non-JIT variants or
        when the shape is not being differentiated.

        Parameter ``viewpoint`` (:py:obj:`mitsuba.Point3f`):
            The viewpoint which defines the silhouette to project the point
            to.

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            The surface point which will be projected.

        Parameter ``flags`` (int):
            Flags to select the type of SilhouetteSample3f to generate from
            the projection. Only one type of discontinuity can be used per
            call.

        Parameter ``sample`` (drjit.llvm.ad.Float):
            A random number that can be used to define the projection
            operation.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.SilhouetteSample3f`:
            A boundary segment on the silhouette of the shape as seen from
            ``viewpoint``.

    .. py:method:: mitsuba.ShapePtr.ray_intersect(self, ray, ray_flags=14, active=True)

        Test for an intersection and return detailed information

        This operation combines the prior ray_intersect_preliminary() and
        compute_surface_interaction() operations.

        Parameter ``ray`` (:py:obj:`mitsuba.Ray3f`):
            The ray to be tested for an intersection

        Parameter ``flags``:
            Describe how the detailed information should be computed

        Parameter ``ray_flags`` (int):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.SurfaceInteraction3f`:
            *no description available*

    .. py:method:: mitsuba.ShapePtr.ray_intersect_preliminary(self, ray, prim_index=0, active=True)

        Fast ray intersection

        Efficiently test whether the shape is intersected by the given ray,
        and return preliminary information about the intersection if that is
        the case.

        If the intersection is deemed relevant (e.g. the closest to the ray
        origin), detailed intersection information can later be obtained via
        the create_surface_interaction() method.

        Parameter ``ray`` (:py:obj:`mitsuba.Ray3f`):
            The ray to be tested for an intersection

        Parameter ``prim_index`` (int):
            Index of the primitive to be intersected. This index is ignored by
            a shape that contains a single primitive. Otherwise, if no index
            is provided, the ray intersection will be performed on the shape's
            first primitive at index 0.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.PreliminaryIntersection3f`:
            *no description available*

    .. py:method:: mitsuba.ShapePtr.ray_test(self, ray, active=True)

        Fast ray shadow test

        Efficiently test whether the shape is intersected by the given ray.

        No details about the intersection are returned, hence the function is
        only useful for visibility queries. For most shapes, the
        implementation will simply forward the call to
        ray_intersect_preliminary(). When the shape actually contains a nested
        kd-tree, some optimizations are possible.

        Parameter ``ray`` (:py:obj:`mitsuba.Ray3f`):
            The ray to be tested for an intersection

        Parameter ``prim_index``:
            Index of the primitive to be intersected

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.ShapePtr.sample_direction(self, it, sample, active=True)

        Sample a direction towards this shape with respect to solid angles
        measured at a reference position within the scene

        An ideal implementation of this interface would achieve a uniform
        solid angle density within the surface region that is visible from the
        reference position ``it.p`` (though such an ideal implementation is
        usually neither feasible nor advisable due to poor efficiency).

        The function returns the sampled position and the inverse probability
        per unit solid angle associated with the sample.

        When the Shape subclass does not supply a custom implementation of
        this function, the Shape class reverts to a fallback approach that
        piggybacks on sample_position(). This will generally lead to a
        suboptimal sample placement and higher variance in Monte Carlo
        estimators using the samples.

        Parameter ``it`` (:py:obj:`mitsuba.Interaction3f`):
            A reference position somewhere within the scene.

        Parameter ``sample`` (:py:obj:`mitsuba.Point2f`):
            A uniformly distributed 2D point on the domain ``[0,1]^2``

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.DirectionSample3f`:
            A DirectionSample instance describing the generated sample

    .. py:method:: mitsuba.ShapePtr.sample_position(self, time, sample, active=True)

        Sample a point on the surface of this shape

        The sampling strategy is ideally uniform over the surface, though
        implementations are allowed to deviate from a perfectly uniform
        distribution as long as this is reflected in the returned probability
        density.

        Parameter ``time`` (drjit.llvm.ad.Float):
            The scene time associated with the position sample

        Parameter ``sample`` (:py:obj:`mitsuba.Point2f`):
            A uniformly distributed 2D point on the domain ``[0,1]^2``

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.PositionSample3f`:
            A PositionSample instance describing the generated sample

    .. py:method:: mitsuba.ShapePtr.sample_precomputed_silhouette(self, viewpoint, sample1, sample2, active=True)

        Samples a boundary segement on the shape's silhouette using
        precomputed information computed in precompute_silhouette.

        This method is meant to be used for silhouettes that are shared
        between all threads, as is the case for primarily visible derivatives.

        This method's behavior is undefined when used in non-JIT variants or
        when the shape is not being differentiated.

        Parameter ``viewpoint`` (:py:obj:`mitsuba.Point3f`):
            The viewpoint that was used for the precomputed silhouette
            information

        Parameter ``sample1`` (drjit.llvm.ad.UInt):
            A sampled index from the return values of precompute_silhouette

        Parameter ``sample2`` (drjit.llvm.ad.Float):
            A uniformly distributed sample in ``[0,1]``

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.SilhouetteSample3f`:
            A boundary segment on the silhouette of the shape as seen from
            ``viewpoint``.

    .. py:method:: mitsuba.ShapePtr.sample_silhouette(self, sample, flags, active=True)

        Map a point sample in boundary sample space to a silhouette segment

        This method's behavior is undefined when used in non-JIT variants or
        when the shape is not being differentiated.

        Parameter ``sample`` (:py:obj:`mitsuba.Point3f`):
            The boundary space sample (a point in the unit cube).

        Parameter ``flags`` (int):
            Flags to select the type of silhouettes to sample from (see
            DiscontinuityFlags). Only one type of discontinuity can be sampled
            per call.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.SilhouetteSample3f`:
            Silhouette sample record.

    .. py:method:: mitsuba.ShapePtr.sensor()

        Return the area sensor associated with this shape (if any)

        Returns → :py:obj:`mitsuba.SensorPtr`:
            *no description available*

    .. py:method:: mitsuba.ShapePtr.shape_type()

        Returns the shape type ShapeType of this shape

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.ShapePtr.silhouette_discontinuity_types()

        //! @{ \name Silhouette sampling routines and other utilities

        Returns → drjit.llvm.ad.UInt:
            *no description available*

    .. py:method:: mitsuba.ShapePtr.silhouette_sampling_weight()

        Return this shape's sampling weight w.r.t. all shapes in the scene

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.ShapePtr.surface_area()

        Return the shape's surface area.

        The function assumes that the object is not undergoing some kind of
        time-dependent scaling.

        The default implementation throws an exception.

        Returns → drjit.llvm.ad.Float:
            *no description available*

.. py:class:: mitsuba.ShapeType

    Enumeration of all shape types in Mitsuba

    Valid values are as follows:

    .. py:data:: Mesh

        Meshes (`ply`, `obj`, `serialized`)

    .. py:data:: BSplineCurve

        B-Spline curves (`bsplinecurve`)

    .. py:data:: Cylinder

        Cylinders (`cylinder`)

    .. py:data:: Disk

        Disks (`disk`)

    .. py:data:: LinearCurve

        Linear curves (`linearcurve`)

    .. py:data:: Rectangle

        Rectangles (`rectangle`)

    .. py:data:: SDFGrid

        SDF Grids (`sdfgrid`)

    .. py:data:: Sphere

        Spheres (`sphere`)

    .. py:data:: Other

        Other shapes

.. py:class:: mitsuba.SilhouetteSample3f

    Base class: :py:obj:`mitsuba.PositionSample3f`

    Data structure holding the result of visibility silhouette sampling
    operations on geometry.

    .. py:method:: __init__()

        Overloaded function.
        
        1. ``__init__(self) -> None``
        
        Construct an uninitialized silhouette sample
        
        2. ``__init__(self, other: :py:obj:`mitsuba.SilhouetteSample3f`) -> None``
        
        Copy constructor

        
    .. py:method:: mitsuba.SilhouetteSample3f.assign(self, arg)

        Parameter ``arg`` (:py:obj:`mitsuba.SilhouetteSample3f`, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.SilhouetteSample3f.d
        :property:

        Direction of the boundary segment sample

    .. py:method:: mitsuba.SilhouetteSample3f.discontinuity_type
        :property:

        Type of discontinuity (DiscontinuityFlags)

    .. py:method:: mitsuba.SilhouetteSample3f.flags
        :property:

        The set of ``DiscontinuityFlags`` that were used to generate this
        sample

    .. py:method:: mitsuba.SilhouetteSample3f.foreshortening
        :property:

        Local-form boundary foreshortening term.

        It stores `sin_phi_B` for perimeter silhouettes or the normal
        curvature for interior silhouettes.

    .. py:method:: mitsuba.SilhouetteSample3f.is_valid()

        Is the current boundary segment valid=

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.SilhouetteSample3f.offset
        :property:

        Offset along the boundary segment direction (`d`) to avoid self-
        intersections.

    .. py:method:: mitsuba.SilhouetteSample3f.prim_index
        :property:

        Primitive index, e.g. the triangle ID (if applicable)

    .. py:method:: mitsuba.SilhouetteSample3f.projection_index
        :property:

        Projection index indicator

        For primitives like triangle meshes, a boundary segment is defined not
        only by the triangle index but also the edge index of the selected
        triangle. A value larger than 3 indicates a failed projection. For
        other primitives, zero indicates a failed projection.

        For triangle meshes, index 0 stands for the directed edge p0->p1 (not
        the opposite edge p1->p2), index 1 stands for the edge p1->p2, and
        index 2 for p2->p0.

    .. py:method:: mitsuba.SilhouetteSample3f.scene_index
        :property:

        Index of the shape in the scene (if applicable)

    .. py:method:: mitsuba.SilhouetteSample3f.shape
        :property:

        Pointer to the associated shape

    .. py:method:: mitsuba.SilhouetteSample3f.silhouette_d
        :property:

        Direction of the silhouette curve at the boundary point

    .. py:method:: mitsuba.SilhouetteSample3f.spawn_ray()

        Spawn a ray on the silhouette point in the direction of d

        The ray origin is offset in the direction of the segment (d) aswell as
        in the in the direction of the silhouette normal (n). Without this
        offsetting, during a ray intersection, the ray could potentially find
        an intersection point at its origin due to numerical instabilities in
        the intersection routines.

        Returns → :py:obj:`mitsuba.Ray3f`:
            *no description available*

.. py:class:: mitsuba.Spiral

    Base class: :py:obj:`mitsuba.Object`

    Generates a spiral of blocks to be rendered.

    Author:
        Adam Arbree Aug 25, 2005 RayTracer.java Used with permission.
        Copyright 2005 Program of Computer Graphics, Cornell University

    .. py:method:: __init__(self, size, offset, block_size=32, passes=1)

        Create a new spiral generator for the given size, offset into a larger
        frame, and block size

        Parameter ``size`` (:py:obj:`mitsuba.ScalarVector2u`):
            *no description available*

        Parameter ``offset`` (:py:obj:`mitsuba.ScalarVector2u`):
            *no description available*

        Parameter ``block_size`` (int):
            *no description available*

        Parameter ``passes`` (int):
            *no description available*

        
    .. py:method:: mitsuba.Spiral.block_count()

        Return the total number of blocks

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Spiral.max_block_size()

        Return the maximum block size

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Spiral.next_block()

        Return the offset, size, and unique identifier of the next block.

        A size of zero indicates that the spiral traversal is done.

        Returns → tuple[:py:obj:`mitsuba.ScalarVector2i`, :py:obj:`mitsuba.ScalarVector2u`, int]:
            *no description available*

    .. py:method:: mitsuba.Spiral.reset()

        Reset the spiral to its initial state. Does not affect the number of
        passes.

        Returns → None:
            *no description available*

.. py:class:: mitsuba.Stream

    Base class: :py:obj:`mitsuba.Object`

    Abstract seekable stream class

    Specifies all functions to be implemented by stream subclasses and
    provides various convenience functions layered on top of on them.

    All ``read*()`` and ``write*()`` methods support transparent
    conversion based on the endianness of the underlying system and the
    value passed to set_byte_order(). Whenever host_byte_order() and
    byte_order() disagree, the endianness is swapped.

    See also:
        FileStream, MemoryStream, DummyStream

    .. py:class:: mitsuba.Stream.EByteOrder

        Defines the byte order (endianness) to use in this Stream

        Valid values are as follows:

        .. py:data:: EBigEndian

            

        .. py:data:: ELittleEndian

            PowerPC, SPARC, Motorola 68K

    .. py:method:: mitsuba.Stream.byte_order()

        Returns the byte order of this stream.

        Returns → :py:obj:`mitsuba.Stream.EByteOrder`:
            *no description available*

    .. py:method:: mitsuba.Stream.can_read()

        Can we read from the stream?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Stream.can_write()

        Can we write to the stream?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Stream.close()

        Closes the stream.

        No further read or write operations are permitted.

        This function is idempotent. It may be called automatically by the
        destructor.

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Stream.flush()

        Flushes the stream's buffers, if any

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Stream.read(self, arg)

        Writes a specified amount of data into the stream. \note This does
        **not** handle endianness swapping.

        Throws an exception when not all data could be written.
        Implementations need to handle endianness swap when appropriate.

        Parameter ``arg`` (int, /):
            *no description available*

        Returns → bytes:
            *no description available*

    .. py:method:: mitsuba.Stream.read_bool()

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.read_double()

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.read_float()

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.read_int16()

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.read_int32()

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.read_int64()

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.read_int8()

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.read_line()

        Convenience function for reading a line of text from an ASCII file

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.Stream.read_single()

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.read_string()

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.read_uint16()

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.read_uint32()

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.read_uint64()

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.read_uint8()

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.seek(self, arg)

        Seeks to a position inside the stream.

        Seeking beyond the size of the buffer will not modify the length of
        its contents. However, a subsequent write should start at the sought
        position and update the size appropriately.

        Parameter ``arg`` (int, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Stream.set_byte_order(self, arg)

        Sets the byte order to use in this stream.

        Automatic conversion will be performed on read and write operations to
        match the system's native endianness.

        No consistency is guaranteed if this method is called after performing
        some read and write operations on the system using a different
        endianness.

        Parameter ``arg`` (:py:obj:`mitsuba.Stream.EByteOrder`, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Stream.size()

        Returns the size of the stream

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Stream.skip(self, arg)

        Skip ahead by a given number of bytes

        Parameter ``arg`` (int, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Stream.tell()

        Gets the current position inside the stream

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Stream.truncate(self, arg)

        Truncates the stream to a given size.

        The position is updated to ``min(old_position, size)``. Throws an
        exception if in read-only mode.

        Parameter ``arg`` (int, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Stream.write(self, arg)

        Writes a specified amount of data into the stream. \note This does
        **not** handle endianness swapping.

        Throws an exception when not all data could be written.
        Implementations need to handle endianness swap when appropriate.

        Parameter ``arg`` (bytes, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Stream.write_bool(self, arg)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Parameter ``arg`` (bool, /):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.write_double(self, arg)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Parameter ``arg`` (float, /):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.write_float(self, arg)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Parameter ``arg`` (float, /):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.write_int16(self, arg)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Parameter ``arg`` (int, /):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.write_int32(self, arg)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Parameter ``arg`` (int, /):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.write_int64(self, arg)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Parameter ``arg`` (int, /):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.write_int8(self, arg)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Parameter ``arg`` (int, /):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.write_line(self, arg)

        Convenience function for writing a line of text to an ASCII file

        Parameter ``arg`` (str, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Stream.write_single(self, arg)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Parameter ``arg`` (float, /):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.write_string(self, arg)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Parameter ``arg`` (str, /):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.write_uint16(self, arg)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Parameter ``arg`` (int, /):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.write_uint32(self, arg)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Parameter ``arg`` (int, /):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.write_uint64(self, arg)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Parameter ``arg`` (int, /):
            *no description available*

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.Stream.write_uint8(self, arg)

        Reads one object of type T from the stream at the current position by
        delegating to the appropriate ``serialization_helper``.

        Endianness swapping is handled automatically if needed.

        Parameter ``arg`` (int, /):
            *no description available*

        Returns → object:
            *no description available*

.. py:class:: mitsuba.StreamAppender

    Base class: :py:obj:`mitsuba.Appender`

    %Appender implementation, which writes to an arbitrary C++ output
    stream

    .. py:method:: __init__(self, arg)

        Create a new stream appender
        
        Remark:
            This constructor is not exposed in the Python bindings

        Parameter ``arg`` (str, /):
            *no description available*

        
    .. py:method:: mitsuba.StreamAppender.logs_to_file()

        Does this appender log to a file

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.StreamAppender.read_log()

        Return the contents of the log file as a string

        Returns → str:
            *no description available*

.. py:class:: mitsuba.Struct

    Base class: :py:obj:`mitsuba.Object`

    Descriptor for specifying the contents and in-memory layout of a POD-
    style data record

    Remark:
        The python API provides an additional ``dtype()`` method, which
        returns the NumPy ``dtype`` equivalent of a given ``Struct``
        instance.

    .. py:method:: __init__(self, pack=False, byte_order=ByteOrder.HostByteOrder)

        Create a new ``Struct`` and indicate whether the contents are packed
        or aligned

        Parameter ``pack`` (bool):
            *no description available*

        Parameter ``byte_order`` (:py:obj:`mitsuba.Struct.ByteOrder`):
            *no description available*

        
    .. py:class:: mitsuba.Struct.ByteOrder

        Valid values are as follows:

        .. py:data:: LittleEndian

            

        .. py:data:: BigEndian

            

        .. py:data:: HostByteOrder

            

    .. py:class:: mitsuba.Struct.Field

        Field specifier with size and offset

    .. py:method:: mitsuba.Struct.Field.blend
        :property:

        For use with StructConverter::convert()

        Specifies a pair of weights and source field names that will be
        linearly blended to obtain the output field value. Note that this only
        works for floating point fields or integer fields with the
        Flags::Normalized flag. Gamma-corrected fields will be blended in
        linear space.

    .. py:method:: mitsuba.Struct.Field.flags
        :property:

        Additional flags

    .. py:method:: mitsuba.Struct.Field.is_float()

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Struct.Field.is_integer()

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Struct.Field.is_signed()

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Struct.Field.is_unsigned()

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Struct.Field.name
        :property:

        Name of the field

    .. py:method:: mitsuba.Struct.Field.offset
        :property:

        Offset within the ``Struct`` (in bytes)

    .. py:method:: mitsuba.Struct.Field.range()

        Returns → tuple[float, float]:
            *no description available*

    .. py:method:: mitsuba.Struct.Field.size
        :property:

        Size in bytes

    .. py:method:: mitsuba.Struct.Field.type
        :property:

        Type identifier

    .. py:class:: mitsuba.Struct.Flags

        Valid values are as follows:

        .. py:data:: Empty

            No flags set (default value)

        .. py:data:: Normalized

            Specifies whether an integer field encodes a normalized value in the range [0, 1]. The flag is ignored if specified for floating point valued fields.

        .. py:data:: Gamma

            Specifies whether the field encodes a sRGB gamma-corrected value. Assumes ``Normalized`` is also specified.

        .. py:data:: Weight

            In FieldConverter::convert, when an input structure contains a weight field, the value of all entries are considered to be expressed relative to its value. Converting to an un-weighted structure entails a division by the weight.

        .. py:data:: Assert

            In FieldConverter::convert, check that the field value matches the specified default value. Otherwise, return a failure

        .. py:data:: Alpha

            Specifies whether the field encodes an alpha value

        .. py:data:: PremultipliedAlpha

            Specifies whether the field encodes an alpha premultiplied value

        .. py:data:: Default

            In FieldConverter::convert, when the field is missing in the source record, replace it by the specified default value

    .. py:class:: mitsuba.Struct.Type

        Valid values are as follows:

        .. py:data:: Int8

            

        .. py:data:: UInt8

            

        .. py:data:: Int16

            

        .. py:data:: UInt16

            

        .. py:data:: Int32

            

        .. py:data:: UInt32

            

        .. py:data:: Int64

            

        .. py:data:: UInt64

            

        .. py:data:: Float16

            

        .. py:data:: Float32

            

        .. py:data:: Float64

            

        .. py:data:: Invalid

            

    .. py:method:: mitsuba.Struct.alignment()

        Return the alignment (in bytes) of the data structure

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Struct.append(self, name, type, flags=Flags.Empty, default=0.0)

        Append a new field to the ``Struct``; determines size and offset
        automatically

        Parameter ``name`` (str):
            *no description available*

        Parameter ``type`` (:py:obj:`mitsuba.Struct.Type`):
            *no description available*

        Parameter ``flags`` (int):
            *no description available*

        Parameter ``default`` (float):
            *no description available*

        Returns → :py:obj:`mitsuba.Struct`:
            *no description available*

    .. py:method:: mitsuba.Struct.byte_order()

        Return the byte order of the ``Struct``

        Returns → :py:obj:`mitsuba.Struct.ByteOrder`:
            *no description available*

    .. py:method:: mitsuba.Struct.field(self, arg)

        Look up a field by name (throws an exception if not found)

        Parameter ``arg`` (str, /):
            *no description available*

        Returns → :py:obj:`mitsuba.Struct.Field`:
            *no description available*

    .. py:method:: mitsuba.Struct.field_count()

        Return the number of fields

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Struct.has_field(self, arg)

        Check if the ``Struct`` has a field of the specified name

        Parameter ``arg`` (str, /):
            *no description available*

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Struct.size()

        Return the size (in bytes) of the data structure, including padding

        Returns → int:
            *no description available*

.. py:class:: mitsuba.StructConverter

    Base class: :py:obj:`mitsuba.Object`

    This class solves the any-to-any problem: efficiently converting from
    one kind of structured data representation to another

    Graphics applications often need to convert from one kind of
    structured representation to another, for instance when loading/saving
    image or mesh data. Consider the following data records which both
    describe positions tagged with color data.

    .. code-block:: c

        struct Source { // <-- Big endian! :(
           uint8_t r, g, b; // in sRGB
           half x, y, z;
        };

        struct Target { // <-- Little endian!
           float x, y, z;
           float r, g, b, a; // in linear space
        };


    The record ``Source`` may represent what is stored in a file on disk,
    while ``Target`` represents the expected input of the implementation.
    Not only are the formats (e.g. float vs half or uint8_t, incompatible
    endianness) and encodings different (e.g. gamma correction vs linear
    space), but the second record even has a different order and extra
    fields that don't exist in the first one.

    This class provides a routine convert() which <ol>

    * reorders entries

    * converts between many different formats (u[int]8-64, float16-64)

    * performs endianness conversion

    * applies or removes gamma correction

    * optionally checks that certain entries have expected default values

    * substitutes missing values with specified defaults

    * performs linear transformations of groups of fields (e.g. between
    different RGB color spaces)

    * applies dithering to avoid banding artifacts when converting 2D
    images

    </ol>

    The above operations can be arranged in countless ways, which makes it
    hard to provide an efficient generic implementation of this
    functionality. For this reason, the implementation of this class
    relies on a JIT compiler that generates fast conversion code on demand
    for each specific conversion. The function is cached and reused in
    case the same conversion is needed later on. Note that JIT compilation
    only works on x86_64 processors; other platforms use a slow generic
    fallback implementation.

    .. py:method:: __init__(self, source, target, dither=False)

        Parameter ``source`` (:py:obj:`mitsuba.Struct`):
            *no description available*

        Parameter ``target`` (:py:obj:`mitsuba.Struct`):
            *no description available*

        Parameter ``dither`` (bool):
            *no description available*


    .. py:method:: mitsuba.StructConverter.convert(self, arg)

        Parameter ``arg`` (bytes, /):
            *no description available*

        Returns → bytes:
            *no description available*

    .. py:method:: mitsuba.StructConverter.source()

        Return the source ``Struct`` descriptor

        Returns → :py:obj:`mitsuba.Struct`:
            *no description available*

    .. py:method:: mitsuba.StructConverter.target()

        Return the target ``Struct`` descriptor

        Returns → :py:obj:`mitsuba.Struct`:
            *no description available*

.. py:class:: mitsuba.SurfaceInteraction3f

    Base class: :py:obj:`mitsuba.Interaction3f`

    Stores information related to a surface scattering interaction

    .. py:method:: __init__()

        Overloaded function.
        
        1. ``__init__(self) -> None``
        
        Construct from a position sample. Unavailable fields such as `wi` and
        the partial derivatives are left uninitialized. The `shape` pointer is
        left uninitialized because we can't guarantee that the given
        PositionSample::object points to a Shape instance.
        
        2. ``__init__(self, arg: :py:obj:`mitsuba.SurfaceInteraction3f`) -> None``
        
        Copy constructor
        
        3. ``__init__(self, ps: :py:obj:`mitsuba.PositionSample3f`, wavelengths: :py:obj:`mitsuba.Color0f`) -> None``
        
        Construct from a position sample. Unavailable fields such as `wi` and
        the partial derivatives are left uninitialized. The `shape` pointer is
        left uninitialized because we can't guarantee that the given
        PositionSample::object points to a Shape instance.

        
    .. py:method:: mitsuba.SurfaceInteraction3f.assign(self, arg)

        Parameter ``arg`` (:py:obj:`mitsuba.SurfaceInteraction3f`, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.SurfaceInteraction3f.bsdf(self, ray)

        Overloaded function.

        1. ``bsdf(self, ray: :py:obj:`mitsuba.RayDifferential3f`) -> :py:obj:`mitsuba.BSDFPtr```

        Returns the BSDF of the intersected shape.

        The parameter ``ray`` must match the one used to create the
        interaction record. This function computes texture coordinate partials
        if this is required by the BSDF (e.g. for texture filtering).

        Implementation in 'bsdf.h'

        2. ``bsdf(self) -> :py:obj:`mitsuba.BSDFPtr```

        Parameter ``ray`` (:py:obj:`mitsuba.RayDifferential3f`):
            *no description available*

        Returns → :py:obj:`mitsuba.BSDFPtr`:
            *no description available*

    .. py:method:: mitsuba.SurfaceInteraction3f.compute_uv_partials(self, ray)

        Computes texture coordinate partials

        Parameter ``ray`` (:py:obj:`mitsuba.RayDifferential3f`):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.SurfaceInteraction3f.dn_du
        :property:

        Normal partials wrt. the UV parameterization

    .. py:method:: mitsuba.SurfaceInteraction3f.dn_dv
        :property:

        Normal partials wrt. the UV parameterization

    .. py:method:: mitsuba.SurfaceInteraction3f.dp_du
        :property:

        Position partials wrt. the UV parameterization

    .. py:method:: mitsuba.SurfaceInteraction3f.dp_dv
        :property:

        Position partials wrt. the UV parameterization

    .. py:method:: mitsuba.SurfaceInteraction3f.duv_dx
        :property:

        UV partials wrt. changes in screen-space

    .. py:method:: mitsuba.SurfaceInteraction3f.duv_dy
        :property:

        UV partials wrt. changes in screen-space

    .. py:method:: mitsuba.SurfaceInteraction3f.emitter(self, scene, active=True)

        Return the emitter associated with the intersection (if any) \note
        Defined in scene.h

        Parameter ``scene`` (:py:obj:`mitsuba.Scene`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.EmitterPtr`:
            *no description available*

    .. py:method:: mitsuba.SurfaceInteraction3f.has_n_partials()

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.SurfaceInteraction3f.has_uv_partials()

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.SurfaceInteraction3f.initialize_sh_frame()

        Initialize local shading frame using Gram-schmidt orthogonalization

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.SurfaceInteraction3f.instance
        :property:

        Stores a pointer to the parent instance (if applicable)

    .. py:method:: mitsuba.SurfaceInteraction3f.is_medium_transition()

        Does the surface mark a transition between two media?

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.SurfaceInteraction3f.is_sensor()

        Is the intersected shape also a sensor?

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.SurfaceInteraction3f.prim_index
        :property:

        Primitive index, e.g. the triangle ID (if applicable)

    .. py:method:: mitsuba.SurfaceInteraction3f.sh_frame
        :property:

        Shading frame

    .. py:method:: mitsuba.SurfaceInteraction3f.shape
        :property:

        Pointer to the associated shape

    .. py:method:: mitsuba.SurfaceInteraction3f.target_medium(self, d)

        Overloaded function.

        1. ``target_medium(self, d: :py:obj:`mitsuba.Vector3f`) -> :py:obj:`mitsuba.MediumPtr```

        Determine the target medium

        When ``is_medium_transition()`` = ``True``, determine the medium that
        contains the ``ray(this->p, d)``

        2. ``target_medium(self, cos_theta: drjit.llvm.ad.Float) -> :py:obj:`mitsuba.MediumPtr```

        Determine the target medium based on the cosine of the angle between
        the geometric normal and a direction

        Returns the exterior medium when ``cos_theta > 0`` and the interior
        medium when ``cos_theta <= 0``.

        Parameter ``d`` (:py:obj:`mitsuba.Vector3f`):
            *no description available*

        Returns → :py:obj:`mitsuba.MediumPtr`:
            *no description available*

    .. py:method:: mitsuba.SurfaceInteraction3f.to_local(self, v)

        Convert a world-space vector into local shading coordinates

        Parameter ``v`` (:py:obj:`mitsuba.Vector3f`):
            *no description available*

        Returns → :py:obj:`mitsuba.Vector3f`:
            *no description available*

    .. py:method:: mitsuba.SurfaceInteraction3f.to_local_mueller(self, M_world, wi_world, wo_world)

        Converts a Mueller matrix defined in world space to a local frame

        A Mueller matrix operates from the (implicitly) defined frame
        stokes_basis(in_forward) to the frame stokes_basis(out_forward). This
        method converts a Mueller matrix defined on directions in world-space
        to a Mueller matrix defined in the local frame.

        This expands to a no-op in non-polarized modes.

        Parameter ``in_forward_local``:
            Incident direction (along propagation direction of light), given
            in world-space coordinates.

        Parameter ``wo_local``:
            Outgoing direction (along propagation direction of light), given
            in world-space coordinates.

        Parameter ``M_world`` (:py:obj:`mitsuba.Color3f`):
            *no description available*

        Parameter ``wi_world`` (:py:obj:`mitsuba.Vector3f`):
            *no description available*

        Parameter ``wo_world`` (:py:obj:`mitsuba.Vector3f`):
            *no description available*

        Returns → :py:obj:`mitsuba.Color3f`:
            Equivalent Mueller matrix that operates in local frame
            coordinates.

    .. py:method:: mitsuba.SurfaceInteraction3f.to_world(self, v)

        Convert a local shading-space vector into world space

        Parameter ``v`` (:py:obj:`mitsuba.Vector3f`):
            *no description available*

        Returns → :py:obj:`mitsuba.Vector3f`:
            *no description available*

    .. py:method:: mitsuba.SurfaceInteraction3f.to_world_mueller(self, M_local, wi_local, wo_local)

        Converts a Mueller matrix defined in a local frame to world space

        A Mueller matrix operates from the (implicitly) defined frame
        stokes_basis(in_forward) to the frame stokes_basis(out_forward). This
        method converts a Mueller matrix defined on directions in the local
        frame to a Mueller matrix defined on world-space directions.

        This expands to a no-op in non-polarized modes.

        Parameter ``M_local`` (:py:obj:`mitsuba.Color3f`):
            The Mueller matrix in local space, e.g. returned by a BSDF.

        Parameter ``in_forward_local``:
            Incident direction (along propagation direction of light), given
            in local frame coordinates.

        Parameter ``wo_local`` (:py:obj:`mitsuba.Vector3f`):
            Outgoing direction (along propagation direction of light), given
            in local frame coordinates.

        Parameter ``wi_local`` (:py:obj:`mitsuba.Vector3f`):
            *no description available*

        Returns → :py:obj:`mitsuba.Color3f`:
            Equivalent Mueller matrix that operates in world-space
            coordinates.

    .. py:method:: mitsuba.SurfaceInteraction3f.uv
        :property:

        UV surface coordinates

    .. py:method:: mitsuba.SurfaceInteraction3f.wi
        :property:

        Incident direction in the local shading frame

.. py:class:: mitsuba.TensorXb

.. py:class:: mitsuba.TensorXf

.. py:class:: mitsuba.TensorXi

.. py:class:: mitsuba.TensorXi64

.. py:class:: mitsuba.TensorXu

.. py:class:: mitsuba.TensorXu64

.. py:class:: mitsuba.Texture

    Base class: :py:obj:`mitsuba.Object`

    Base class of all surface texture implementations

    This class implements a generic texture map that supports evaluation
    at arbitrary surface positions and wavelengths (if compiled in
    spectral mode). It can be used to provide both intensities (e.g. for
    light sources) and unitless reflectance parameters (e.g. an albedo of
    a reflectance model).

    The spectrum can be evaluated at arbitrary (continuous) wavelengths,
    though the underlying function it is not required to be smooth or even
    continuous.

    .. py:method:: __init__(self, props)

        Parameter ``props`` (:py:obj:`mitsuba.Properties`):
            *no description available*


    .. py:method:: mitsuba.Texture.eval(self, si, active=True)

        Evaluate the texture at the given surface interaction

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            An interaction record describing the associated surface position

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Color3f`:
            An unpolarized spectral power distribution or reflectance value

    .. py:method:: mitsuba.Texture.eval_1(self, si, active=True)

        Monochromatic evaluation of the texture at the given surface
        interaction

        This function differs from eval() in that it provided raw access to
        scalar intensity/reflectance values without any color processing (e.g.
        spectral upsampling). This is useful in parts of the renderer that
        encode scalar quantities using textures, e.g. a height field.

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            An interaction record describing the associated surface position

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            An scalar intensity or reflectance value

    .. py:method:: mitsuba.Texture.eval_1_grad(self, si, active=True)

        Monochromatic evaluation of the texture gradient at the given surface
        interaction

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            An interaction record describing the associated surface position

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Vector2f`:
            A (u,v) pair of intensity or reflectance value gradients

    .. py:method:: mitsuba.Texture.eval_3(self, si, active=True)

        Trichromatic evaluation of the texture at the given surface
        interaction

        This function differs from eval() in that it provided raw access to
        RGB intensity/reflectance values without any additional color
        processing (e.g. RGB-to-spectral upsampling). This is useful in parts
        of the renderer that encode 3D quantities using textures, e.g. a
        normal map.

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            An interaction record describing the associated surface position

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Color3f`:
            An trichromatic intensity or reflectance value

    .. py:method:: mitsuba.Texture.is_spatially_varying()

        Does this texture evaluation depend on the UV coordinates

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Texture.max()

        Return the maximum value of the spectrum

        Not every implementation necessarily provides this function. The
        default implementation throws an exception.

        Even if the operation is provided, it may only return an
        approximation.

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.Texture.mean()

        Return the mean value of the spectrum over the support
        (MI_WAVELENGTH_MIN..MI_WAVELENGTH_MAX)

        Not every implementation necessarily provides this function. The
        default implementation throws an exception.

        Even if the operation is provided, it may only return an
        approximation.

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Texture.pdf_position(self, p, active=True)

        Returns the probability per unit area of sample_position()

        Parameter ``p`` (:py:obj:`mitsuba.Point2f`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Texture.pdf_spectrum(self, si, active=True)

        Evaluate the density function of the sample_spectrum() method as a
        probability per unit wavelength (in units of 1/nm).

        Not every implementation necessarily overrides this function. The
        default implementation throws an exception.

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            An interaction record describing the associated surface position

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Color0f`:
            A density value for each wavelength in ``si.wavelengths`` (hence
            the Wavelength type).

    .. py:method:: mitsuba.Texture.resolution()

        Returns the resolution of the texture, assuming that it is based on a
        discrete representation.

        The default implementation returns ``(1, 1)``

        Returns → :py:obj:`mitsuba.ScalarVector2i`:
            *no description available*

    .. py:method:: mitsuba.Texture.sample_position(self, sample, active=True)

        Importance sample a surface position proportional to the overall
        spectral reflectance or intensity of the texture

        This function assumes that the texture is implemented as a mapping
        from 2D UV positions to texture values, which is not necessarily true
        for all textures (e.g. 3D noise functions, mesh attributes, etc.). For
        this reason, not every will plugin provide a specialized
        implementation, and the default implementation simply return the input
        sample (i.e. uniform sampling is used).

        Parameter ``sample`` (:py:obj:`mitsuba.Point2f`):
            A 2D vector of uniform variates

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
            1. A texture-space position in the range :math:`[0, 1]^2`

        2. The associated probability per unit area in UV space

    .. py:method:: mitsuba.Texture.sample_spectrum(self, si, sample, active=True)

        Importance sample a set of wavelengths proportional to the spectrum
        defined at the given surface position

        Not every implementation necessarily provides this function, and it is
        a no-op when compiling non-spectral variants of Mitsuba. The default
        implementation throws an exception.

        Parameter ``si`` (:py:obj:`mitsuba.SurfaceInteraction3f`):
            An interaction record describing the associated surface position

        Parameter ``sample`` (:py:obj:`mitsuba.Color0f`):
            A uniform variate for each desired wavelength.

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Color0f`, :py:obj:`mitsuba.Color3f`]:
            1. Set of sampled wavelengths specified in nanometers

        2. The Monte Carlo importance weight (Spectral power distribution
        value divided by the sampling density)

    .. py:method:: mitsuba.Texture.spectral_resolution()

        Returns the resolution of the spectrum in nanometers (if discretized)

        Not every implementation necessarily provides this function. The
        default implementation throws an exception.

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.Texture.wavelength_range()

        Returns the range of wavelengths covered by the spectrum

        The default implementation returns ``(MI_CIE_MIN, MI_CIE_MAX)``

        Returns → :py:obj:`mitsuba.ScalarVector2f`:
            *no description available*

.. py:class:: mitsuba.Texture1f


    .. py:method:: ``__init__(self, shape, channels, use_accel=True, filter_mode=FilterMode.Linear, wrap_mode=WrapMode.Clamp)

        Create a new texture with the specified size and channel count

        On CUDA, this is a slow operation that synchronizes the GPU pipeline, so
        texture objects should be reused/updated via :py:func:`set_value()` and
        :py:func:`set_tensor()` as much as possible.

        When ``use_accel`` is set to ``False`` on CUDA mode, the texture will not
        use hardware acceleration (allocation and evaluation). In other modes
        this argument has no effect.

        The ``filter_mode`` parameter defines the interpolation method to be used
        in all evaluation routines. By default, the texture is linearly
        interpolated. Besides nearest/linear filtering, the implementation also
        provides a clamped cubic B-spline interpolation scheme in case a
        higher-order interpolation is needed. In CUDA mode, this is done using a
        series of linear lookups to optimally use the hardware (hence, linear
        filtering must be enabled to use this feature).

        When evaluating the texture outside of its boundaries, the ``wrap_mode``
        defines the wrapping method. The default behavior is ``drjit.WrapMode.Clamp``,
        which indefinitely extends the colors on the boundary along each dimension.

        Parameter ``shape`` (collections.abc.Sequence[int]):
            *no description available*

        Parameter ``channels`` (int):
            *no description available*

        Parameter ``use_accel`` (bool):
            *no description available*

        Parameter ``filter_mode`` (drjit.FilterMode):
            *no description available*

        Parameter ``wrap_mode`` (drjit.WrapMode):
            *no description available*

        Returns → None``:
            *no description available*

    .. py:method:: ``__init__(self, tensor, use_accel=True, migrate=True, filter_mode=FilterMode.Linear, wrap_mode=WrapMode.Clamp)

        Construct a new texture from a given tensor.

        This constructor allocates texture memory with the shape information
        deduced from ``tensor``. It subsequently invokes :py:func:`set_tensor(tensor)`
        to fill the texture memory with the provided tensor.

        When both ``migrate`` and ``use_accel`` are set to ``True`` in CUDA mode, the texture
        exclusively stores a copy of the input data as a CUDA texture to avoid
        redundant storage. Note that the texture is still differentiable even when migrated.

        Parameter ``tensor`` (drjit.llvm.ad.TensorXf):
            *no description available*

        Parameter ``use_accel`` (bool):
            *no description available*

        Parameter ``migrate`` (bool):
            *no description available*

        Parameter ``filter_mode`` (drjit.FilterMode):
            *no description available*

        Parameter ``wrap_mode`` (drjit.WrapMode):
            *no description available*

        Returns → None``:
            *no description available*

    .. py:method:: mitsuba.Texture1f.eval(self, pos, active=Bool(True))

        Evaluate the linear interpolant represented by this texture.

        Parameter ``pos`` (drjit.llvm.ad.Array1f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool | None):
            Mask to specify active lanes.

        Returns → list[drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.Texture1f.eval_cubic(self, pos, active=Bool(True), force_nonaccel=False)

        Evaluate a clamped cubic B-Spline interpolant represented by this
        texture

        Instead of interpolating the texture via B-Spline basis functions, the
        implementation transforms this calculation into an equivalent weighted
        sum of several linear interpolant evaluations. In CUDA mode, this can
        then be accelerated by hardware texture units, which runs faster than
        a naive implementation. More information can be found in:

            GPU Gems 2, Chapter 20, "Fast Third-Order Texture Filtering"
            by Christian Sigg.

        When the underlying grid data and the query position are differentiable,
        this transformation cannot be used as it is not linear with respect to position
        (thus the default AD graph gives incorrect results). The implementation
        calls :py:func:`eval_cubic_helper()` function to replace the AD graph with a
        direct evaluation of the B-Spline basis functions in that case.

        Parameter ``pos`` (drjit.llvm.ad.Array1f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool | None):
            Mask to specify active lanes.

        Parameter ``force_nonaccel`` (bool):
            *no description available*

        Returns → list[drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.Texture1f.eval_cubic_grad(self, pos, active=Bool(True))

        Evaluate the positional gradient of a cubic B-Spline

        This implementation computes the result directly from explicit
        differentiated basis functions. It has no autodiff support.

        The resulting gradient and hessian have been multiplied by the spatial extents
        to count for the transformation from the unit size volume to the size of its
        shape.

        Parameter ``pos`` (drjit.llvm.ad.Array1f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool | None):
            Mask to specify active lanes.

        Returns → tuple:
            *no description available*

    .. py:method:: mitsuba.Texture1f.eval_cubic_helper(self, pos, active=Bool(True))

        Helper function to evaluate a clamped cubic B-Spline interpolant

        This is an implementation detail and should only be called by the
        :py:func:`eval_cubic()` function to construct an AD graph. When only the cubic
        evaluation result is desired, the :py:func:`eval_cubic()` function is faster
        than this simple implementation

        Parameter ``pos`` (drjit.llvm.ad.Array1f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool | None):
            Mask to specify active lanes.

        Returns → list[drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.Texture1f.eval_cubic_hessian(self, pos, active=Bool(True))

        Evaluate the positional gradient and hessian matrix of a cubic B-Spline

        This implementation computes the result directly from explicit
        differentiated basis functions. It has no autodiff support.

        The resulting gradient and hessian have been multiplied by the spatial extents
        to count for the transformation from the unit size volume to the size of its
        shape.

        Parameter ``pos`` (drjit.llvm.ad.Array1f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool | None):
            Mask to specify active lanes.

        Returns → tuple:
            *no description available*

    .. py:method:: mitsuba.Texture1f.eval_fetch(self, pos, active=Bool(True))

        Fetch the texels that would be referenced in a texture lookup with
        linear interpolation without actually performing this interpolation.

        Parameter ``pos`` (drjit.llvm.ad.Array1f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool | None):
            Mask to specify active lanes.

        Returns → list[list[drjit.llvm.ad.Float]]:
            *no description available*

    .. py:method:: mitsuba.Texture1f.filter_mode()

        Return the filter mode

        Returns → drjit.FilterMode:
            *no description available*

    .. py:method:: mitsuba.Texture1f.migrated()

        Return whether textures with :py:func:`use_accel()` set to ``True`` only store
        the data as a hardware-accelerated CUDA texture.

        If ``False`` then a copy of the array data will additionally be retained .

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Texture1f.set_tensor(self, tensor, migrate=False)

        Override the texture contents with the provided tensor.

        This method updates the values of all texels. Changing the texture
        resolution or its number of channels is also supported. However, on CUDA,
        such operations have a significantly larger overhead (the GPU pipeline
        needs to be synchronized for new texture objects to be created).

        In CUDA mode, when both the argument ``migrate`` and :py:func:`use_accel()` are ``True``,
        the texture exclusively stores a copy of the input data as a CUDA texture to avoid
        redundant storage.Note that the texture is still differentiable even when migrated.

        Parameter ``tensor`` (drjit.llvm.ad.TensorXf):
            *no description available*

        Parameter ``migrate`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Texture1f.set_value(self, value, migrate=False)

        Override the texture contents with the provided linearized 1D array.

        In CUDA mode, when both the argument ``migrate`` and :py:func:`use_accel()` are ``True``,
        the texture exclusively stores a copy of the input data as a CUDA texture to avoid
        redundant storage.Note that the texture is still differentiable even when migrated.

        Parameter ``value`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``migrate`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Texture1f.shape
        :property:

        Return the texture shape

    .. py:method:: mitsuba.Texture1f.tensor()

        Return the texture data as a tensor object

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.Texture1f.use_accel()

        Return whether texture uses the GPU for storage and evaluation

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Texture1f.value()

        Return the texture data as an array object

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Texture1f.wrap_mode()

        Return the wrap mode

        Returns → drjit.WrapMode:
            *no description available*

.. py:class:: mitsuba.Texture2f


    .. py:method:: ``__init__(self, shape, channels, use_accel=True, filter_mode=FilterMode.Linear, wrap_mode=WrapMode.Clamp)

        Create a new texture with the specified size and channel count

        On CUDA, this is a slow operation that synchronizes the GPU pipeline, so
        texture objects should be reused/updated via :py:func:`set_value()` and
        :py:func:`set_tensor()` as much as possible.

        When ``use_accel`` is set to ``False`` on CUDA mode, the texture will not
        use hardware acceleration (allocation and evaluation). In other modes
        this argument has no effect.

        The ``filter_mode`` parameter defines the interpolation method to be used
        in all evaluation routines. By default, the texture is linearly
        interpolated. Besides nearest/linear filtering, the implementation also
        provides a clamped cubic B-spline interpolation scheme in case a
        higher-order interpolation is needed. In CUDA mode, this is done using a
        series of linear lookups to optimally use the hardware (hence, linear
        filtering must be enabled to use this feature).

        When evaluating the texture outside of its boundaries, the ``wrap_mode``
        defines the wrapping method. The default behavior is ``drjit.WrapMode.Clamp``,
        which indefinitely extends the colors on the boundary along each dimension.

        Parameter ``shape`` (collections.abc.Sequence[int]):
            *no description available*

        Parameter ``channels`` (int):
            *no description available*

        Parameter ``use_accel`` (bool):
            *no description available*

        Parameter ``filter_mode`` (drjit.FilterMode):
            *no description available*

        Parameter ``wrap_mode`` (drjit.WrapMode):
            *no description available*

        Returns → None``:
            *no description available*

    .. py:method:: ``__init__(self, tensor, use_accel=True, migrate=True, filter_mode=FilterMode.Linear, wrap_mode=WrapMode.Clamp)

        Construct a new texture from a given tensor.

        This constructor allocates texture memory with the shape information
        deduced from ``tensor``. It subsequently invokes :py:func:`set_tensor(tensor)`
        to fill the texture memory with the provided tensor.

        When both ``migrate`` and ``use_accel`` are set to ``True`` in CUDA mode, the texture
        exclusively stores a copy of the input data as a CUDA texture to avoid
        redundant storage. Note that the texture is still differentiable even when migrated.

        Parameter ``tensor`` (drjit.llvm.ad.TensorXf):
            *no description available*

        Parameter ``use_accel`` (bool):
            *no description available*

        Parameter ``migrate`` (bool):
            *no description available*

        Parameter ``filter_mode`` (drjit.FilterMode):
            *no description available*

        Parameter ``wrap_mode`` (drjit.WrapMode):
            *no description available*

        Returns → None``:
            *no description available*

    .. py:method:: mitsuba.Texture2f.eval(self, pos, active=Bool(True))

        Evaluate the linear interpolant represented by this texture.

        Parameter ``pos`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool | None):
            Mask to specify active lanes.

        Returns → list[drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.Texture2f.eval_cubic(self, pos, active=Bool(True), force_nonaccel=False)

        Evaluate a clamped cubic B-Spline interpolant represented by this
        texture

        Instead of interpolating the texture via B-Spline basis functions, the
        implementation transforms this calculation into an equivalent weighted
        sum of several linear interpolant evaluations. In CUDA mode, this can
        then be accelerated by hardware texture units, which runs faster than
        a naive implementation. More information can be found in:

            GPU Gems 2, Chapter 20, "Fast Third-Order Texture Filtering"
            by Christian Sigg.

        When the underlying grid data and the query position are differentiable,
        this transformation cannot be used as it is not linear with respect to position
        (thus the default AD graph gives incorrect results). The implementation
        calls :py:func:`eval_cubic_helper()` function to replace the AD graph with a
        direct evaluation of the B-Spline basis functions in that case.

        Parameter ``pos`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool | None):
            Mask to specify active lanes.

        Parameter ``force_nonaccel`` (bool):
            *no description available*

        Returns → list[drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.Texture2f.eval_cubic_grad(self, pos, active=Bool(True))

        Evaluate the positional gradient of a cubic B-Spline

        This implementation computes the result directly from explicit
        differentiated basis functions. It has no autodiff support.

        The resulting gradient and hessian have been multiplied by the spatial extents
        to count for the transformation from the unit size volume to the size of its
        shape.

        Parameter ``pos`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool | None):
            Mask to specify active lanes.

        Returns → tuple:
            *no description available*

    .. py:method:: mitsuba.Texture2f.eval_cubic_helper(self, pos, active=Bool(True))

        Helper function to evaluate a clamped cubic B-Spline interpolant

        This is an implementation detail and should only be called by the
        :py:func:`eval_cubic()` function to construct an AD graph. When only the cubic
        evaluation result is desired, the :py:func:`eval_cubic()` function is faster
        than this simple implementation

        Parameter ``pos`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool | None):
            Mask to specify active lanes.

        Returns → list[drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.Texture2f.eval_cubic_hessian(self, pos, active=Bool(True))

        Evaluate the positional gradient and hessian matrix of a cubic B-Spline

        This implementation computes the result directly from explicit
        differentiated basis functions. It has no autodiff support.

        The resulting gradient and hessian have been multiplied by the spatial extents
        to count for the transformation from the unit size volume to the size of its
        shape.

        Parameter ``pos`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool | None):
            Mask to specify active lanes.

        Returns → tuple:
            *no description available*

    .. py:method:: mitsuba.Texture2f.eval_fetch(self, pos, active=Bool(True))

        Fetch the texels that would be referenced in a texture lookup with
        linear interpolation without actually performing this interpolation.

        Parameter ``pos`` (drjit.llvm.ad.Array2f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool | None):
            Mask to specify active lanes.

        Returns → list[list[drjit.llvm.ad.Float]]:
            *no description available*

    .. py:method:: mitsuba.Texture2f.filter_mode()

        Return the filter mode

        Returns → drjit.FilterMode:
            *no description available*

    .. py:method:: mitsuba.Texture2f.migrated()

        Return whether textures with :py:func:`use_accel()` set to ``True`` only store
        the data as a hardware-accelerated CUDA texture.

        If ``False`` then a copy of the array data will additionally be retained .

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Texture2f.set_tensor(self, tensor, migrate=False)

        Override the texture contents with the provided tensor.

        This method updates the values of all texels. Changing the texture
        resolution or its number of channels is also supported. However, on CUDA,
        such operations have a significantly larger overhead (the GPU pipeline
        needs to be synchronized for new texture objects to be created).

        In CUDA mode, when both the argument ``migrate`` and :py:func:`use_accel()` are ``True``,
        the texture exclusively stores a copy of the input data as a CUDA texture to avoid
        redundant storage.Note that the texture is still differentiable even when migrated.

        Parameter ``tensor`` (drjit.llvm.ad.TensorXf):
            *no description available*

        Parameter ``migrate`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Texture2f.set_value(self, value, migrate=False)

        Override the texture contents with the provided linearized 1D array.

        In CUDA mode, when both the argument ``migrate`` and :py:func:`use_accel()` are ``True``,
        the texture exclusively stores a copy of the input data as a CUDA texture to avoid
        redundant storage.Note that the texture is still differentiable even when migrated.

        Parameter ``value`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``migrate`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Texture2f.shape
        :property:

        Return the texture shape

    .. py:method:: mitsuba.Texture2f.tensor()

        Return the texture data as a tensor object

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.Texture2f.use_accel()

        Return whether texture uses the GPU for storage and evaluation

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Texture2f.value()

        Return the texture data as an array object

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Texture2f.wrap_mode()

        Return the wrap mode

        Returns → drjit.WrapMode:
            *no description available*

.. py:class:: mitsuba.Texture3f


    .. py:method:: ``__init__(self, shape, channels, use_accel=True, filter_mode=FilterMode.Linear, wrap_mode=WrapMode.Clamp)

        Create a new texture with the specified size and channel count

        On CUDA, this is a slow operation that synchronizes the GPU pipeline, so
        texture objects should be reused/updated via :py:func:`set_value()` and
        :py:func:`set_tensor()` as much as possible.

        When ``use_accel`` is set to ``False`` on CUDA mode, the texture will not
        use hardware acceleration (allocation and evaluation). In other modes
        this argument has no effect.

        The ``filter_mode`` parameter defines the interpolation method to be used
        in all evaluation routines. By default, the texture is linearly
        interpolated. Besides nearest/linear filtering, the implementation also
        provides a clamped cubic B-spline interpolation scheme in case a
        higher-order interpolation is needed. In CUDA mode, this is done using a
        series of linear lookups to optimally use the hardware (hence, linear
        filtering must be enabled to use this feature).

        When evaluating the texture outside of its boundaries, the ``wrap_mode``
        defines the wrapping method. The default behavior is ``drjit.WrapMode.Clamp``,
        which indefinitely extends the colors on the boundary along each dimension.

        Parameter ``shape`` (collections.abc.Sequence[int]):
            *no description available*

        Parameter ``channels`` (int):
            *no description available*

        Parameter ``use_accel`` (bool):
            *no description available*

        Parameter ``filter_mode`` (drjit.FilterMode):
            *no description available*

        Parameter ``wrap_mode`` (drjit.WrapMode):
            *no description available*

        Returns → None``:
            *no description available*

    .. py:method:: ``__init__(self, tensor, use_accel=True, migrate=True, filter_mode=FilterMode.Linear, wrap_mode=WrapMode.Clamp)

        Construct a new texture from a given tensor.

        This constructor allocates texture memory with the shape information
        deduced from ``tensor``. It subsequently invokes :py:func:`set_tensor(tensor)`
        to fill the texture memory with the provided tensor.

        When both ``migrate`` and ``use_accel`` are set to ``True`` in CUDA mode, the texture
        exclusively stores a copy of the input data as a CUDA texture to avoid
        redundant storage. Note that the texture is still differentiable even when migrated.

        Parameter ``tensor`` (drjit.llvm.ad.TensorXf):
            *no description available*

        Parameter ``use_accel`` (bool):
            *no description available*

        Parameter ``migrate`` (bool):
            *no description available*

        Parameter ``filter_mode`` (drjit.FilterMode):
            *no description available*

        Parameter ``wrap_mode`` (drjit.WrapMode):
            *no description available*

        Returns → None``:
            *no description available*

    .. py:method:: mitsuba.Texture3f.eval(self, pos, active=Bool(True))

        Evaluate the linear interpolant represented by this texture.

        Parameter ``pos`` (drjit.llvm.ad.Array3f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool | None):
            Mask to specify active lanes.

        Returns → list[drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.Texture3f.eval_cubic(self, pos, active=Bool(True), force_nonaccel=False)

        Evaluate a clamped cubic B-Spline interpolant represented by this
        texture

        Instead of interpolating the texture via B-Spline basis functions, the
        implementation transforms this calculation into an equivalent weighted
        sum of several linear interpolant evaluations. In CUDA mode, this can
        then be accelerated by hardware texture units, which runs faster than
        a naive implementation. More information can be found in:

            GPU Gems 2, Chapter 20, "Fast Third-Order Texture Filtering"
            by Christian Sigg.

        When the underlying grid data and the query position are differentiable,
        this transformation cannot be used as it is not linear with respect to position
        (thus the default AD graph gives incorrect results). The implementation
        calls :py:func:`eval_cubic_helper()` function to replace the AD graph with a
        direct evaluation of the B-Spline basis functions in that case.

        Parameter ``pos`` (drjit.llvm.ad.Array3f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool | None):
            Mask to specify active lanes.

        Parameter ``force_nonaccel`` (bool):
            *no description available*

        Returns → list[drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.Texture3f.eval_cubic_grad(self, pos, active=Bool(True))

        Evaluate the positional gradient of a cubic B-Spline

        This implementation computes the result directly from explicit
        differentiated basis functions. It has no autodiff support.

        The resulting gradient and hessian have been multiplied by the spatial extents
        to count for the transformation from the unit size volume to the size of its
        shape.

        Parameter ``pos`` (drjit.llvm.ad.Array3f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool | None):
            Mask to specify active lanes.

        Returns → tuple:
            *no description available*

    .. py:method:: mitsuba.Texture3f.eval_cubic_helper(self, pos, active=Bool(True))

        Helper function to evaluate a clamped cubic B-Spline interpolant

        This is an implementation detail and should only be called by the
        :py:func:`eval_cubic()` function to construct an AD graph. When only the cubic
        evaluation result is desired, the :py:func:`eval_cubic()` function is faster
        than this simple implementation

        Parameter ``pos`` (drjit.llvm.ad.Array3f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool | None):
            Mask to specify active lanes.

        Returns → list[drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.Texture3f.eval_cubic_hessian(self, pos, active=Bool(True))

        Evaluate the positional gradient and hessian matrix of a cubic B-Spline

        This implementation computes the result directly from explicit
        differentiated basis functions. It has no autodiff support.

        The resulting gradient and hessian have been multiplied by the spatial extents
        to count for the transformation from the unit size volume to the size of its
        shape.

        Parameter ``pos`` (drjit.llvm.ad.Array3f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool | None):
            Mask to specify active lanes.

        Returns → tuple:
            *no description available*

    .. py:method:: mitsuba.Texture3f.eval_fetch(self, pos, active=Bool(True))

        Fetch the texels that would be referenced in a texture lookup with
        linear interpolation without actually performing this interpolation.

        Parameter ``pos`` (drjit.llvm.ad.Array3f):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool | None):
            Mask to specify active lanes.

        Returns → list[list[drjit.llvm.ad.Float]]:
            *no description available*

    .. py:method:: mitsuba.Texture3f.filter_mode()

        Return the filter mode

        Returns → drjit.FilterMode:
            *no description available*

    .. py:method:: mitsuba.Texture3f.migrated()

        Return whether textures with :py:func:`use_accel()` set to ``True`` only store
        the data as a hardware-accelerated CUDA texture.

        If ``False`` then a copy of the array data will additionally be retained .

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Texture3f.set_tensor(self, tensor, migrate=False)

        Override the texture contents with the provided tensor.

        This method updates the values of all texels. Changing the texture
        resolution or its number of channels is also supported. However, on CUDA,
        such operations have a significantly larger overhead (the GPU pipeline
        needs to be synchronized for new texture objects to be created).

        In CUDA mode, when both the argument ``migrate`` and :py:func:`use_accel()` are ``True``,
        the texture exclusively stores a copy of the input data as a CUDA texture to avoid
        redundant storage.Note that the texture is still differentiable even when migrated.

        Parameter ``tensor`` (drjit.llvm.ad.TensorXf):
            *no description available*

        Parameter ``migrate`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Texture3f.set_value(self, value, migrate=False)

        Override the texture contents with the provided linearized 1D array.

        In CUDA mode, when both the argument ``migrate`` and :py:func:`use_accel()` are ``True``,
        the texture exclusively stores a copy of the input data as a CUDA texture to avoid
        redundant storage.Note that the texture is still differentiable even when migrated.

        Parameter ``value`` (drjit.llvm.ad.Float):
            *no description available*

        Parameter ``migrate`` (bool):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Texture3f.shape
        :property:

        Return the texture shape

    .. py:method:: mitsuba.Texture3f.tensor()

        Return the texture data as a tensor object

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.Texture3f.use_accel()

        Return whether texture uses the GPU for storage and evaluation

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Texture3f.value()

        Return the texture data as an array object

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Texture3f.wrap_mode()

        Return the wrap mode

        Returns → drjit.WrapMode:
            *no description available*

.. py:class:: mitsuba.Thread

    Base class: :py:obj:`mitsuba.Object`

    Cross-platform thread implementation

    Mitsuba threads are internally implemented via the ``std::thread``
    class defined in C++11. This wrapper class is needed to attach
    additional state (Loggers, Path resolvers, etc.) that is inherited
    when a thread launches another thread.

    .. py:method:: __init__(self, name)

        Parameter ``name`` (str):
            *no description available*


    .. py:class:: mitsuba.Thread.EPriority

        Possible priority values for Thread::set_priority()

        Valid values are as follows:

        .. py:data:: EIdlePriority

            

        .. py:data:: ELowestPriority

            

        .. py:data:: ELowPriority

            

        .. py:data:: ENormalPriority

            

        .. py:data:: EHighPriority

            

        .. py:data:: EHighestPriority

            

        .. py:data:: ERealtimePriority

            

    .. py:method:: mitsuba.Thread.core_affinity()

        Return the core affinity

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Thread.detach()

        Detach the thread and release resources

        After a call to this function, join() cannot be used anymore. This
        releases resources, which would otherwise be held until a call to
        join().

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Thread.file_resolver()

        Return the file resolver associated with the current thread

        Returns → :py:obj:`mitsuba.FileResolver`:
            *no description available*

    .. py:method:: mitsuba.Thread.is_critical()

        Return the value of the critical flag

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Thread.is_running()

        Is this thread still running?

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.Thread.join()

        Wait until the thread finishes

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Thread.logger()

        Return the thread's logger instance

        Returns → :py:obj:`mitsuba.Logger`:
            *no description available*

    .. py:method:: mitsuba.Thread.name()

        Return the name of this thread

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.Thread.parent()

        Return the parent thread

        Returns → :py:obj:`mitsuba.Thread`:
            *no description available*

    .. py:method:: mitsuba.Thread.priority()

        Return the thread priority

        Returns → :py:obj:`mitsuba.Thread.EPriority`:
            *no description available*

    .. py:method:: mitsuba.Thread.set_core_affinity(self, arg)

        Set the core affinity

        This function provides a hint to the operating system scheduler that
        the thread should preferably run on the specified processor core. By
        default, the parameter is set to -1, which means that there is no
        affinity.

        Parameter ``arg`` (int, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Thread.set_critical(self, arg)

        Specify whether or not this thread is critical

        When an thread marked critical crashes from an uncaught exception, the
        whole process is brought down. The default is ``False``.

        Parameter ``arg`` (bool, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Thread.set_file_resolver(self, arg)

        Set the file resolver associated with the current thread

        Parameter ``arg`` (:py:obj:`mitsuba.FileResolver`, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Thread.set_logger(self, arg)

        Set the logger instance used to process log messages from this thread

        Parameter ``arg`` (:py:obj:`mitsuba.Logger`, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Thread.set_name(self, arg)

        Set the name of this thread

        Parameter ``arg`` (str, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Thread.set_priority(self, arg)

        Set the thread priority

        This does not always work -- for instance, Linux requires root
        privileges for this operation.

        Parameter ``arg`` (:py:obj:`mitsuba.Thread.EPriority`, /):
            *no description available*

        Returns → bool:
            ``True`` upon success.

    .. py:method:: mitsuba.Thread.start()

        Start the thread

        Returns → None:
            *no description available*

.. py:class:: mitsuba.ThreadEnvironment

    Captures a thread environment (logger and file resolver). Used with
    ScopedSetThreadEnvironment

    .. py:method:: __init__()


.. py:class:: mitsuba.Timer

    .. py:method:: mitsuba.Timer.begin_stage(self, arg)

        Parameter ``arg`` (str, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Timer.end_stage(self, arg)

        Parameter ``arg`` (str, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Timer.reset()

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Timer.value()

        Returns → int:
            *no description available*

.. py:class:: mitsuba.Transform3d

    Encapsulates a 4x4 homogeneous coordinate transformation along with
    its inverse transpose

    The Transform class provides a set of overloaded matrix-vector
    multiplication operators for vectors, points, and normals (all of them
    behave differently under homogeneous coordinate transformations, hence
    the need to represent them using separate types)

    .. py:method:: __init__()

        Overloaded function.
        
        1. ``__init__(self) -> None``
        
        Initialize with the identity matrix
        
        2. ``__init__(self, arg: :py:obj:`mitsuba.Transform3d`) -> None``
        
        Copy constructor
        
        3. ``__init__(self, arg: ndarray[dtype=float64, shape=(3, 3), order='C', device='cpu'], /) -> None``
        
        
        4. ``__init__(self) -> None``
        
        
        5. ``__init__(self, arg: drjit.llvm.ad.Matrix3f64, /) -> None``
        
        Initialize the transformation from the given matrix (and compute its
        inverse transpose)
        
        6. ``__init__(self, arg0: drjit.llvm.ad.Matrix3f64, arg1: drjit.llvm.ad.Matrix3f64, /) -> None``
        
        Initialize from a matrix and its inverse transpose
        
        7. ``__init__(self, arg: :py:obj:`mitsuba.ScalarTransform3d`, /) -> None``
        
        Broadcast constructor

        
    .. py:method:: mitsuba.Transform3d.assign(self, arg)

        Parameter ``arg`` (:py:obj:`mitsuba.Transform3d`, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Transform3d.has_scale()

        Test for a scale component in each transform matrix by checking
        whether ``M . M^T == I`` (where ``M`` is the matrix in question and
        ``I`` is the identity).

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.Transform3d.inverse()

        Compute the inverse of this transformation (involves just shuffles, no
        arithmetic)

        Returns → :py:obj:`mitsuba.Transform3d`:
            *no description available*

    .. py:method:: mitsuba.Transform3d.inverse_transpose
        :property:

        (self) -> drjit.llvm.ad.Matrix3f64

    .. py:method:: mitsuba.Transform3d.matrix
        :property:

        (self) -> drjit.llvm.ad.Matrix3f64

    .. py:method:: mitsuba.Transform3d.rotate(self, angle)

        Create a rotation transformation in 2D. The angle is specified in
        degrees

        Parameter ``angle`` (drjit.llvm.ad.Float64):
            *no description available*

        Returns → :py:obj:`mitsuba.Transform3d`:
            *no description available*

    .. py:method:: mitsuba.Transform3d.scale(self, v)

        Create a scale transformation

        Parameter ``v`` (:py:obj:`mitsuba.Point2d`):
            *no description available*

        Returns → :py:obj:`mitsuba.Transform3d`:
            *no description available*

    .. py:method:: mitsuba.Transform3d.transform_affine(self, p)

        Transform a 3D vector/point/normal/ray by a transformation that is
        known to be an affine 3D transformation (i.e. no perspective)

        Parameter ``p`` (:py:obj:`mitsuba.Point2d`):
            *no description available*

        Returns → :py:obj:`mitsuba.Point2d`:
            *no description available*

    .. py:method:: mitsuba.Transform3d.translate(self, v)

        Create a translation transformation

        Parameter ``v`` (:py:obj:`mitsuba.Point2d`):
            *no description available*

        Returns → :py:obj:`mitsuba.Transform3d`:
            *no description available*

    .. py:method:: mitsuba.Transform3d.translation()

        Get the translation part of a matrix

        Returns → :py:obj:`mitsuba.Vector2d`:
            *no description available*

.. py:class:: mitsuba.Transform3f

    Encapsulates a 4x4 homogeneous coordinate transformation along with
    its inverse transpose

    The Transform class provides a set of overloaded matrix-vector
    multiplication operators for vectors, points, and normals (all of them
    behave differently under homogeneous coordinate transformations, hence
    the need to represent them using separate types)

    .. py:method:: __init__()

        Overloaded function.
        
        1. ``__init__(self) -> None``
        
        Initialize with the identity matrix
        
        2. ``__init__(self, arg: :py:obj:`mitsuba.Transform3f`) -> None``
        
        Copy constructor
        
        3. ``__init__(self, arg: ndarray[dtype=float32, shape=(3, 3), order='C', device='cpu'], /) -> None``
        
        
        4. ``__init__(self) -> None``
        
        
        5. ``__init__(self, arg: drjit.llvm.ad.Matrix3f, /) -> None``
        
        Initialize the transformation from the given matrix (and compute its
        inverse transpose)
        
        6. ``__init__(self, arg0: drjit.llvm.ad.Matrix3f, arg1: drjit.llvm.ad.Matrix3f, /) -> None``
        
        Initialize from a matrix and its inverse transpose
        
        7. ``__init__(self, arg: :py:obj:`mitsuba.ScalarTransform3f`, /) -> None``
        
        Broadcast constructor

        
    .. py:method:: mitsuba.Transform3f.assign(self, arg)

        Parameter ``arg`` (:py:obj:`mitsuba.Transform3f`, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Transform3f.has_scale()

        Test for a scale component in each transform matrix by checking
        whether ``M . M^T == I`` (where ``M`` is the matrix in question and
        ``I`` is the identity).

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.Transform3f.inverse()

        Compute the inverse of this transformation (involves just shuffles, no
        arithmetic)

        Returns → :py:obj:`mitsuba.Transform3f`:
            *no description available*

    .. py:method:: mitsuba.Transform3f.inverse_transpose
        :property:

        (self) -> drjit.llvm.ad.Matrix3f

    .. py:method:: mitsuba.Transform3f.matrix
        :property:

        (self) -> drjit.llvm.ad.Matrix3f

    .. py:method:: mitsuba.Transform3f.rotate(self, angle)

        Create a rotation transformation in 2D. The angle is specified in
        degrees

        Parameter ``angle`` (drjit.llvm.ad.Float):
            *no description available*

        Returns → :py:obj:`mitsuba.Transform3f`:
            *no description available*

    .. py:method:: mitsuba.Transform3f.scale(self, v)

        Create a scale transformation

        Parameter ``v`` (:py:obj:`mitsuba.Point2f`):
            *no description available*

        Returns → :py:obj:`mitsuba.Transform3f`:
            *no description available*

    .. py:method:: mitsuba.Transform3f.transform_affine(self, p)

        Transform a 3D vector/point/normal/ray by a transformation that is
        known to be an affine 3D transformation (i.e. no perspective)

        Parameter ``p`` (:py:obj:`mitsuba.Point2f`):
            *no description available*

        Returns → :py:obj:`mitsuba.Point2f`:
            *no description available*

    .. py:method:: mitsuba.Transform3f.translate(self, v)

        Create a translation transformation

        Parameter ``v`` (:py:obj:`mitsuba.Point2f`):
            *no description available*

        Returns → :py:obj:`mitsuba.Transform3f`:
            *no description available*

    .. py:method:: mitsuba.Transform3f.translation()

        Get the translation part of a matrix

        Returns → :py:obj:`mitsuba.Vector2f`:
            *no description available*

.. py:class:: mitsuba.Transform4d

    Encapsulates a 4x4 homogeneous coordinate transformation along with
    its inverse transpose

    The Transform class provides a set of overloaded matrix-vector
    multiplication operators for vectors, points, and normals (all of them
    behave differently under homogeneous coordinate transformations, hence
    the need to represent them using separate types)

    .. py:method:: __init__()

        Overloaded function.
        
        1. ``__init__(self) -> None``
        
        Initialize with the identity matrix
        
        2. ``__init__(self, arg: :py:obj:`mitsuba.Transform4d`) -> None``
        
        Copy constructor
        
        3. ``__init__(self, arg: ndarray[dtype=float64, shape=(4, 4), order='C', device='cpu'], /) -> None``
        
        
        4. ``__init__(self, arg: list, /) -> None``
        
        
        5. ``__init__(self, arg: drjit.llvm.ad.Matrix4f64, /) -> None``
        
        Initialize the transformation from the given matrix (and compute its
        inverse transpose)
        
        6. ``__init__(self, arg0: drjit.llvm.ad.Matrix4f64, arg1: drjit.llvm.ad.Matrix4f64, /) -> None``
        
        Initialize from a matrix and its inverse transpose
        
        7. ``__init__(self, arg: :py:obj:`mitsuba.ScalarTransform4d`, /) -> None``
        
        Broadcast constructor

        
    .. py:method:: mitsuba.Transform4d.assign(self, arg)

        Parameter ``arg`` (:py:obj:`mitsuba.Transform4d`, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Transform4d.extract()

        Extract a lower-dimensional submatrix

        Returns → :py:obj:`mitsuba.Transform3d`:
            *no description available*

    .. py:method:: mitsuba.Transform4d.from_frame(self, frame)

        Creates a transformation that converts from 'frame' to the standard
        basis

        Parameter ``frame`` (mitsuba::Frame<drjit::DiffArray<(JitBackend)2, double>>):
            *no description available*

        Returns → :py:obj:`mitsuba.Transform4d`:
            *no description available*

    .. py:method:: mitsuba.Transform4d.has_scale()

        Test for a scale component in each transform matrix by checking
        whether ``M . M^T == I`` (where ``M`` is the matrix in question and
        ``I`` is the identity).

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.Transform4d.inverse()

        Compute the inverse of this transformation (involves just shuffles, no
        arithmetic)

        Returns → :py:obj:`mitsuba.Transform4d`:
            *no description available*

    .. py:method:: mitsuba.Transform4d.inverse_transpose
        :property:

        (self) -> drjit.llvm.ad.Matrix4f64

    .. py:method:: mitsuba.Transform4d.look_at(self, origin, target, up)

        Create a look-at camera transformation

        Parameter ``origin`` (:py:obj:`mitsuba.Point3d`):
            Camera position

        Parameter ``target`` (:py:obj:`mitsuba.Point3d`):
            Target vector

        Parameter ``up`` (:py:obj:`mitsuba.Point3d`):
            Up vector

        Returns → :py:obj:`mitsuba.Transform4d`:
            *no description available*

    .. py:method:: mitsuba.Transform4d.matrix
        :property:

        (self) -> drjit.llvm.ad.Matrix4f64

    .. py:method:: mitsuba.Transform4d.orthographic(self, near, far)

        Create an orthographic transformation, which maps Z to [0,1] and
        leaves the X and Y coordinates untouched.

        Parameter ``near`` (drjit.llvm.ad.Float64):
            Near clipping plane

        Parameter ``far`` (drjit.llvm.ad.Float64):
            Far clipping plane

        Returns → :py:obj:`mitsuba.Transform4d`:
            *no description available*

    .. py:method:: mitsuba.Transform4d.perspective(self, fov, near, far)

        Create a perspective transformation. (Maps [near, far] to [0, 1])

        Projects vectors in camera space onto a plane at z=1:

        x_proj = x / z y_proj = y / z z_proj = (far * (z - near)) / (z * (far-
        near))

        Camera-space depths are not mapped linearly!

        Parameter ``fov`` (drjit.llvm.ad.Float64):
            Field of view in degrees

        Parameter ``near`` (drjit.llvm.ad.Float64):
            Near clipping plane

        Parameter ``far`` (drjit.llvm.ad.Float64):
            Far clipping plane

        Returns → :py:obj:`mitsuba.Transform4d`:
            *no description available*

    .. py:method:: mitsuba.Transform4d.rotate(self, axis, angle)

        Create a rotation transformation around an arbitrary axis in 3D. The
        angle is specified in degrees

        Parameter ``axis`` (:py:obj:`mitsuba.Point3d`):
            *no description available*

        Parameter ``angle`` (drjit.llvm.ad.Float64):
            *no description available*

        Returns → :py:obj:`mitsuba.Transform4d`:
            *no description available*

    .. py:method:: mitsuba.Transform4d.scale(self, v)

        Create a scale transformation

        Parameter ``v`` (:py:obj:`mitsuba.Point3d`):
            *no description available*

        Returns → :py:obj:`mitsuba.Transform4d`:
            *no description available*

    .. py:method:: mitsuba.Transform4d.to_frame(self, frame)

        Creates a transformation that converts from the standard basis to
        'frame'

        Parameter ``frame`` (mitsuba::Frame<drjit::DiffArray<(JitBackend)2, double>>):
            *no description available*

        Returns → :py:obj:`mitsuba.Transform4d`:
            *no description available*

    .. py:method:: mitsuba.Transform4d.transform_affine(self, p)

        Transform a 3D vector/point/normal/ray by a transformation that is
        known to be an affine 3D transformation (i.e. no perspective)

        Parameter ``p`` (:py:obj:`mitsuba.Point3d`):
            *no description available*

        Returns → :py:obj:`mitsuba.Point3d`:
            *no description available*

    .. py:method:: mitsuba.Transform4d.translate(self, v)

        Create a translation transformation

        Parameter ``v`` (:py:obj:`mitsuba.Point3d`):
            *no description available*

        Returns → :py:obj:`mitsuba.Transform4d`:
            *no description available*

    .. py:method:: mitsuba.Transform4d.translation()

        Get the translation part of a matrix

        Returns → :py:obj:`mitsuba.Vector3d`:
            *no description available*

.. py:class:: mitsuba.Transform4f

    Encapsulates a 4x4 homogeneous coordinate transformation along with
    its inverse transpose

    The Transform class provides a set of overloaded matrix-vector
    multiplication operators for vectors, points, and normals (all of them
    behave differently under homogeneous coordinate transformations, hence
    the need to represent them using separate types)

    .. py:method:: __init__()

        Overloaded function.
        
        1. ``__init__(self) -> None``
        
        Initialize with the identity matrix
        
        2. ``__init__(self, arg: :py:obj:`mitsuba.Transform4f`) -> None``
        
        Copy constructor
        
        3. ``__init__(self, arg: ndarray[dtype=float32, shape=(4, 4), order='C', device='cpu'], /) -> None``
        
        
        4. ``__init__(self, arg: list, /) -> None``
        
        
        5. ``__init__(self, arg: drjit.llvm.ad.Matrix4f, /) -> None``
        
        Initialize the transformation from the given matrix (and compute its
        inverse transpose)
        
        6. ``__init__(self, arg0: drjit.llvm.ad.Matrix4f, arg1: drjit.llvm.ad.Matrix4f, /) -> None``
        
        Initialize from a matrix and its inverse transpose
        
        7. ``__init__(self, arg: :py:obj:`mitsuba.ScalarTransform4f`, /) -> None``
        
        Broadcast constructor

        
    .. py:method:: mitsuba.Transform4f.assign(self, arg)

        Parameter ``arg`` (:py:obj:`mitsuba.Transform4f`, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.Transform4f.extract()

        Extract a lower-dimensional submatrix

        Returns → :py:obj:`mitsuba.Transform3f`:
            *no description available*

    .. py:method:: mitsuba.Transform4f.from_frame(self, frame)

        Creates a transformation that converts from 'frame' to the standard
        basis

        Parameter ``frame`` (:py:obj:`mitsuba.Frame3f`):
            *no description available*

        Returns → :py:obj:`mitsuba.Transform4f`:
            *no description available*

    .. py:method:: mitsuba.Transform4f.has_scale()

        Test for a scale component in each transform matrix by checking
        whether ``M . M^T == I`` (where ``M`` is the matrix in question and
        ``I`` is the identity).

        Returns → drjit.llvm.ad.Bool:
            *no description available*

    .. py:method:: mitsuba.Transform4f.inverse()

        Compute the inverse of this transformation (involves just shuffles, no
        arithmetic)

        Returns → :py:obj:`mitsuba.Transform4f`:
            *no description available*

    .. py:method:: mitsuba.Transform4f.inverse_transpose
        :property:

        (self) -> drjit.llvm.ad.Matrix4f

    .. py:method:: mitsuba.Transform4f.look_at(self, origin, target, up)

        Create a look-at camera transformation

        Parameter ``origin`` (:py:obj:`mitsuba.Point3f`):
            Camera position

        Parameter ``target`` (:py:obj:`mitsuba.Point3f`):
            Target vector

        Parameter ``up`` (:py:obj:`mitsuba.Point3f`):
            Up vector

        Returns → :py:obj:`mitsuba.Transform4f`:
            *no description available*

    .. py:method:: mitsuba.Transform4f.matrix
        :property:

        (self) -> drjit.llvm.ad.Matrix4f

    .. py:method:: mitsuba.Transform4f.orthographic(self, near, far)

        Create an orthographic transformation, which maps Z to [0,1] and
        leaves the X and Y coordinates untouched.

        Parameter ``near`` (drjit.llvm.ad.Float):
            Near clipping plane

        Parameter ``far`` (drjit.llvm.ad.Float):
            Far clipping plane

        Returns → :py:obj:`mitsuba.Transform4f`:
            *no description available*

    .. py:method:: mitsuba.Transform4f.perspective(self, fov, near, far)

        Create a perspective transformation. (Maps [near, far] to [0, 1])

        Projects vectors in camera space onto a plane at z=1:

        x_proj = x / z y_proj = y / z z_proj = (far * (z - near)) / (z * (far-
        near))

        Camera-space depths are not mapped linearly!

        Parameter ``fov`` (drjit.llvm.ad.Float):
            Field of view in degrees

        Parameter ``near`` (drjit.llvm.ad.Float):
            Near clipping plane

        Parameter ``far`` (drjit.llvm.ad.Float):
            Far clipping plane

        Returns → :py:obj:`mitsuba.Transform4f`:
            *no description available*

    .. py:method:: mitsuba.Transform4f.rotate(self, axis, angle)

        Create a rotation transformation around an arbitrary axis in 3D. The
        angle is specified in degrees

        Parameter ``axis`` (:py:obj:`mitsuba.Point3f`):
            *no description available*

        Parameter ``angle`` (drjit.llvm.ad.Float):
            *no description available*

        Returns → :py:obj:`mitsuba.Transform4f`:
            *no description available*

    .. py:method:: mitsuba.Transform4f.scale(self, v)

        Create a scale transformation

        Parameter ``v`` (:py:obj:`mitsuba.Point3f`):
            *no description available*

        Returns → :py:obj:`mitsuba.Transform4f`:
            *no description available*

    .. py:method:: mitsuba.Transform4f.to_frame(self, frame)

        Creates a transformation that converts from the standard basis to
        'frame'

        Parameter ``frame`` (:py:obj:`mitsuba.Frame3f`):
            *no description available*

        Returns → :py:obj:`mitsuba.Transform4f`:
            *no description available*

    .. py:method:: mitsuba.Transform4f.transform_affine(self, p)

        Transform a 3D vector/point/normal/ray by a transformation that is
        known to be an affine 3D transformation (i.e. no perspective)

        Parameter ``p`` (:py:obj:`mitsuba.Point3f`):
            *no description available*

        Returns → :py:obj:`mitsuba.Point3f`:
            *no description available*

    .. py:method:: mitsuba.Transform4f.translate(self, v)

        Create a translation transformation

        Parameter ``v`` (:py:obj:`mitsuba.Point3f`):
            *no description available*

        Returns → :py:obj:`mitsuba.Transform4f`:
            *no description available*

    .. py:method:: mitsuba.Transform4f.translation()

        Get the translation part of a matrix

        Returns → :py:obj:`mitsuba.Vector3f`:
            *no description available*

.. py:class:: mitsuba.TransportMode

    Specifies the transport mode when sampling or evaluating a scattering
    function

    Valid values are as follows:

    .. py:data:: Radiance

        Radiance transport

    .. py:data:: Importance

        Importance transport

.. py:class:: mitsuba.TraversalCallback

    Abstract class providing an interface for traversing scene graphs

    This interface can be implemented either in C++ or in Python, to be
    used in conjunction with Object::traverse() to traverse a scene graph.
    Mitsuba currently uses this mechanism to determine a scene's
    differentiable parameters.

    .. py:method:: __init__()


    .. py:method:: mitsuba.TraversalCallback.put_object(self, name, obj, flags)

        Inform the traversal callback that the instance references another
        Mitsuba object

        Parameter ``name`` (str):
            *no description available*

        Parameter ``obj`` (:py:obj:`mitsuba.Object`):
            *no description available*

        Parameter ``flags`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.TraversalCallback.put_parameter(self, name, value, flags)

        Inform the traversal callback about an attribute of an instance

        Parameter ``name`` (str):
            *no description available*

        Parameter ``value`` (object):
            *no description available*

        Parameter ``flags`` (int):
            *no description available*

        Returns → None:
            *no description available*

.. py:class:: mitsuba.UInt

.. py:class:: mitsuba.UInt64

.. py:class:: mitsuba.Vector0d

.. py:class:: mitsuba.Vector0f

.. py:class:: mitsuba.Vector0i

.. py:class:: mitsuba.Vector0u

.. py:class:: mitsuba.Vector1d

.. py:class:: mitsuba.Vector1f

.. py:class:: mitsuba.Vector1i

.. py:class:: mitsuba.Vector1u

.. py:class:: mitsuba.Vector2d

.. py:class:: mitsuba.Vector2f

.. py:class:: mitsuba.Vector2i

.. py:class:: mitsuba.Vector2u

.. py:class:: mitsuba.Vector3d

.. py:class:: mitsuba.Vector3f

.. py:class:: mitsuba.Vector3i

.. py:class:: mitsuba.Vector3u

.. py:class:: mitsuba.Vector4d

.. py:class:: mitsuba.Vector4f

.. py:class:: mitsuba.Vector4i

.. py:class:: mitsuba.Vector4u

.. py:class:: mitsuba.Volume

    Base class: :py:obj:`mitsuba.Object`

    Abstract base class for 3D volumes.

    .. py:method:: __init__(self, props)

        Parameter ``props`` (:py:obj:`mitsuba.Properties`):
            *no description available*


    .. py:method:: mitsuba.Volume.bbox()

        Returns the bounding box of the volume

        Returns → :py:obj:`mitsuba.ScalarBoundingBox3f`:
            *no description available*

    .. py:method:: mitsuba.Volume.channel_count()

        Returns the number of channels stored in the volume

        When the channel count is zero, it indicates that the volume does not
        support per-channel queries.

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.Volume.eval(self, it, active=True)

        Evaluate the volume at the given surface interaction, with color
        processing.

        Parameter ``it`` (:py:obj:`mitsuba.Interaction3f`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Color3f`:
            *no description available*

    .. py:method:: mitsuba.Volume.eval_1(self, it, active=True)

        Evaluate this volume as a single-channel quantity.

        Parameter ``it`` (:py:obj:`mitsuba.Interaction3f`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.Volume.eval_3(self, it, active=True)

        Evaluate this volume as a three-channel quantity with no color
        processing (e.g. velocity field).

        Parameter ``it`` (:py:obj:`mitsuba.Interaction3f`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → :py:obj:`mitsuba.Vector3f`:
            *no description available*

    .. py:method:: mitsuba.Volume.eval_6(self, it, active=True)

        Evaluate this volume as a six-channel quantity with no color
        processing This interface is specifically intended to encode the
        parameters of an SGGX phase function.

        Parameter ``it`` (:py:obj:`mitsuba.Interaction3f`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → list[drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.Volume.eval_gradient(self, it, active=True)

        Evaluate the volume at the given surface interaction, and compute the
        gradients of the linear interpolant as well.

        Parameter ``it`` (:py:obj:`mitsuba.Interaction3f`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → tuple[:py:obj:`mitsuba.Color3f`, :py:obj:`mitsuba.Vector3f`]:
            *no description available*

    .. py:method:: mitsuba.Volume.eval_n(self, it, active=True)

        Evaluate this volume as a n-channel float quantity

        This interface is specifically intended to encode a variable number of
        parameters. Pointer allocation/deallocation must be performed by the
        caller.

        Parameter ``it`` (:py:obj:`mitsuba.Interaction3f`):
            *no description available*

        Parameter ``active`` (drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → list[drjit.llvm.ad.Float]:
            *no description available*

    .. py:method:: mitsuba.Volume.max()

        Returns the maximum value of the volume over all dimensions.

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.Volume.max_per_channel()

        In the case of a multi-channel volume, this function returns the
        maximum value for each channel.

        Pointer allocation/deallocation must be performed by the caller.

        Returns → list[float]:
            *no description available*

    .. py:method:: mitsuba.Volume.resolution()

        Returns the resolution of the volume, assuming that it is based on a
        discrete representation.

        The default implementation returns ``(1, 1, 1)``

        Returns → :py:obj:`mitsuba.ScalarVector3i`:
            *no description available*

.. py:class:: mitsuba.VolumeGrid

    Base class: :py:obj:`mitsuba.Object`

    Overloaded function.

    1. ``__init__(self, path: :py:obj:`mitsuba.filesystem.path`) -> None``


    2. ``__init__(self, stream: :py:obj:`mitsuba.Stream`) -> None``


    3. ``__init__(self, array: ndarray[dtype=float32, order='C', device='cpu'], compute_max: bool = True) -> None``

    Initialize a VolumeGrid from a CPU-visible ndarray

    4. ``__init__(self, array: drjit.llvm.ad.TensorXf, compute_max: bool = True) -> None``

    Initialize a VolumeGrid from a drjit tensor

    .. py:method:: mitsuba.VolumeGrid.buffer_size()

        Return the volume grid size in bytes (excluding metadata)

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.VolumeGrid.bytes_per_voxel()

        Return the number bytes of storage used per voxel

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.VolumeGrid.channel_count()

        Return the number of channels

        Returns → int:
            *no description available*

    .. py:method:: mitsuba.VolumeGrid.max()

        Return the precomputed maximum over the volume grid

        Returns → float:
            *no description available*

    .. py:method:: mitsuba.VolumeGrid.max_per_channel()

        Return the precomputed maximum over the volume grid per channel

        Pointer allocation/deallocation must be performed by the caller.

        Returns → list[float]:
            *no description available*

    .. py:method:: mitsuba.VolumeGrid.set_max(self, arg)

        Set the precomputed maximum over the volume grid

        Parameter ``arg`` (float, /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.VolumeGrid.set_max_per_channel(self, arg)

        Set the precomputed maximum over the volume grid per channel

        Pointer allocation/deallocation must be performed by the caller.

        Parameter ``arg`` (collections.abc.Sequence[float], /):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.VolumeGrid.size()

        Return the resolution of the voxel grid

        Returns → :py:obj:`mitsuba.ScalarVector3u`:
            *no description available*

    .. py:method:: mitsuba.VolumeGrid.write(self, stream)

        Overloaded function.

        1. ``write(self, stream: :py:obj:`mitsuba.Stream`) -> None``

        Write an encoded form of the bitmap to a binary volume file

        Parameter ``path``:
            Target file name (expected to end in ".vol")

        2. ``write(self, path: :py:obj:`mitsuba.filesystem.path`) -> None``

        Write an encoded form of the volume grid to a stream

        Parameter ``stream`` (:py:obj:`mitsuba.Stream`):
            Target stream that will receive the encoded output

        Returns → None:
            *no description available*

.. py:class:: mitsuba.ZStream

    Base class: :py:obj:`mitsuba.Stream`

    Transparent compression/decompression stream based on ``zlib``.

    This class transparently decompresses and compresses reads and writes
    to a nested stream, respectively.

    .. py:method:: __init__(self, child_stream, stream_type=EStreamType.EDeflateStream, level=-1)

        Creates a new compression stream with the given underlying stream.
        This new instance takes ownership of the child stream. The child
        stream must outlive the ZStream.

        Parameter ``child_stream`` (:py:obj:`mitsuba.Stream`):
            *no description available*

        Parameter ``stream_type`` (:py:obj:`mitsuba.ZStream.EStreamType`):
            *no description available*

        Parameter ``level`` (int):
            *no description available*

        
    .. py:method:: mitsuba.ZStream.child_stream()

        Returns the child stream of this compression stream

        Returns → object:
            *no description available*

.. py:class:: mitsuba.ad.Adam

    Base class: :py:obj:`mitsuba.ad.optimizers.Optimizer`

    Implements the Adam optimizer presented in the paper *Adam: A Method for
    Stochastic Optimization* by Kingman and Ba, ICLR 2015.

    When optimizing many variables (e.g. a high resolution texture) with
    momentum enabled, it may be beneficial to restrict state and variable
    updates to the entries that received nonzero gradients in the current
    iteration (``mask_updates=True``).
    In the context of differentiable Monte Carlo simulations, many of those
    variables may not be observed at each iteration, e.g. when a surface is
    not visible from the current camera. Gradients for unobserved variables
    will remain at zero by default.
    If we do not take special care, at each new iteration:

    1. Momentum accumulated at previous iterations (potentially very noisy)
       will keep being applied to the variable.
    2. The optimizer's state will be updated to incorporate ``gradient = 0``,
       even though it is not an actual gradient value but rather lack of one.

    Enabling ``mask_updates`` avoids these two issues. This is similar to
    `PyTorch's SparseAdam optimizer <https://pytorch.org/docs/1.9.0/generated/torch.optim.SparseAdam.html>`_.

    .. py:method:: __init__(params=None)

        Parameter ``lr``:
            learning rate
        
        Parameter ``beta_1``:
            controls the exponential averaging of first order gradient moments
        
        Parameter ``beta_2``:
            controls the exponential averaging of second order gradient moments
        
        Parameter ``mask_updates``:
            if enabled, parameters and state variables will only be updated in a
            given iteration if it received nonzero gradients in that iteration
        
        Parameter ``uniform``:
            if enabled, the optimizer will use the 'UniformAdam' variant of Adam
            [Nicolet et al. 2021], where the update rule uses the *maximum* of
            the second moment estimates at the current step instead of the
            per-element second moments.
        
        Parameter ``params`` (:py:class:`dict`):
            Optional dictionary-like object containing parameters to optimize.

        Parameter ``params`` (dict | None):
            *no description available*

        
    .. py:method:: mitsuba.ad.Adam.step()

        Take a gradient step

    .. py:method:: mitsuba.ad.Adam.reset()

        Zero-initializes the internal state associated with a parameter

.. py:class:: mitsuba.ad.BaseGuidingDistr

    .. py:method:: mitsuba.ad.BaseGuidingDistr.sample()

        Return a sample in U^3 from the stored guiding distribution and its
        reciprocal density.

.. py:class:: mitsuba.ad.GridDistr

    Base class: :py:obj:`mitsuba.ad.guiding.BaseGuidingDistr`

    Regular grid guiding distribution.

    .. py:method:: __init__()

        Parameter ``resolution``:
            Grid resolution
        
        Parameter ``clamp_mass_thres``:
            Threshold value below which points' mass will be clamped to 0
        
        Parameter ``scale_mass``:
            Scale sample's contribution by performing a power transformation
        
        Parameter ``debug_logs``:
            Whether or not to print debug logs. If this is enabled, extra
            kernels will be launched and the messages will be printed with a
            `Debug` log level.

        
    .. py:method:: mitsuba.ad.GridDistr.get_cell_array()

        Returns the 3D cell index corresponding to the 1D input index.

        With `index_array`=dr.arange(mi.UInt32, self.num_cells), the output
        array of this function is [[0, 0, 0], [0, 0, 1], ..., [Nx-1, Ny-1, Nz-1]].

    .. py:method:: mitsuba.ad.GridDistr.set_mass()

        Sets the grid's density with the flat-1D input mass

    .. py:method:: mitsuba.ad.GridDistr.sample()

        Return a sample in U^3 from the stored guiding distribution and its
        reciprocal density.

.. py:class:: mitsuba.ad.LargeSteps

    Implementation of the algorithm described in the paper "Large Steps in
    Inverse Rendering of Geometry" (Nicolet et al. 2021).

    It consists in computing a latent variable u = (I + λL) v from the vertex
    positions v, where L is the (combinatorial) Laplacian matrix of the input
    mesh. Optimizing these variables instead of the vertex positions allows to
    diffuse gradients on the surface, which helps fight their sparsity.

    This class builds the system matrix (I + λL) for a given mesh and hyper
    parameter λ, and computes its Cholesky factorization.

    It can then convert vertex coordinates back and forth between their
    cartesian and differential representations. Both transformations are
    differentiable, meshes can therefore be optimized by using the differential
    form as a latent variable.

    .. py:method:: __init__()

        Build the system matrix and its Cholesky factorization.
        
        Parameter ``verts`` (``mitsuba.Float``):
            Vertex coordinates of the mesh.
        
        Parameter ``faces`` (``mitsuba.UInt``):
            Face indices of the mesh.
        
        Parameter ``lambda_`` (``float``):
            The hyper parameter λ. This controls how much gradients are diffused
            on the surface. this value should increase with the tesselation of
            the mesh.
        

        
    .. py:method:: mitsuba.ad.LargeSteps.to_differential()

        Convert vertex coordinates to their differential form: u = (I + λL) v.

        This method typically only needs to be called once per mesh, to obtain
        the latent variable before optimization.

        Parameter ``v`` (``mitsuba.Float``):
            Vertex coordinates of the mesh.

        Returns ``mitsuba.Float`:
            Differential form of v.

    .. py:method:: mitsuba.ad.LargeSteps.from_differential()

        Convert differential coordinates back to their cartesian form: v = (I +
        λL)⁻¹ u.

        This is done by solving the linear system (I + λL) v = u using the
        previously computed Cholesky factorization.

        This method is typically called at each iteration of the optimization,
        to update the mesh coordinates before rendering.

        Parameter ``u`` (``mitsuba.Float``):
            Differential form of v.

        Returns ``mitsuba.Float`:
            Vertex coordinates of the mesh.

.. py:class:: mitsuba.ad.OcSpaceDistr

    Base class: :py:obj:`mitsuba.ad.guiding.BaseGuidingDistr`

    Octree space partitioned distribution.

    .. py:method:: mitsuba.ad.OcSpaceDistr.aabbs(buffer, node_idx)

        Returns the front bottom left corner and back top right corner points
        of the AABB with index `node_idx`.

        Parameter ``buffer`` (~drjit.llvm.ad.Float):
            *no description available*

        Parameter ``node_idx`` (~drjit.llvm.ad.UInt):
            *no description available*

    .. py:method:: mitsuba.ad.OcSpaceDistr.split_offset()

        Computes the node offset for a split.

    .. py:method:: mitsuba.ad.OcSpaceDistr.split(buffer, aabb_min, aabb_max, aabb_middle, node_idx)

        Splits an AABB into 8 sub-nodes. The results are written to `buffer`.

        Parameter ``buffer`` (~drjit.llvm.ad.Float):
            *no description available*

        Parameter ``aabb_min`` (~:py:obj:`mitsuba.Point3f`):
            *no description available*

        Parameter ``aabb_max`` (~:py:obj:`mitsuba.Point3f`):
            *no description available*

        Parameter ``aabb_middle`` (~:py:obj:`mitsuba.Point3f`):
            *no description available*

        Parameter ``node_idx`` (~drjit.llvm.ad.UInt):
            *no description available*

    .. py:method:: mitsuba.ad.OcSpaceDistr.construct_octree()

        Octree construction/partitioning for the given `input` points.

    .. py:method:: mitsuba.ad.OcSpaceDistr.estimate_mass_in_leaves(log=False)

        Evaluates `extra_spc` random samples in each leaf to compute an average
        mass per leaf.

        Parameter ``log`` (bool):
            *no description available*

    .. py:method:: mitsuba.ad.OcSpaceDistr.set_points()

        Builds an octree from a set of points and their corresponding mass

    .. py:method:: mitsuba.ad.OcSpaceDistr.sample()

        Return a sample in U^3 from the stored guiding distribution and its
        reciprocal density.

.. py:class:: mitsuba.ad.Optimizer

    Base class of all gradient-based optimizers.

    .. py:method:: __init__(params)

        Parameter ``lr``:
            learning rate
        
        Parameter ``params`` (:py:class:`dict`):
            Dictionary-like object containing parameters to optimize.

        Parameter ``params`` (dict):
            *no description available*

        
    .. py:method:: mitsuba.ad.Optimizer.set_learning_rate()

        Set the learning rate.

        Parameter ``lr`` (``float``, ``dict``):
            The new learning rate. A ``dict`` can be provided instead to
            specify the learning rate for specific parameters.

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ad.Optimizer.reset()

        Resets the internal state associated with a parameter, if any (e.g. momentum).

.. py:class:: mitsuba.ad.ProjectiveDetail

    Class holding implementation details of various operations needed by
    projective-sampling/path-space style integrators.

    .. py:method:: mitsuba.ad.ProjectiveDetail.init_primarily_visible_silhouette(scene, sensor)

        Precompute the silhouette of the scene as seen from the sensor and store
        the result in this python class.

        Parameter ``scene`` (~:py:obj:`mitsuba.Scene`):
            *no description available*

        Parameter ``sensor`` (~:py:obj:`mitsuba.Sensor`):
            *no description available*

    .. py:method:: mitsuba.ad.ProjectiveDetail.sample_primarily_visible_silhouette(scene, viewpoint, sample2, active)

        Sample a primarily visible silhouette point as seen from the sensor.
        Returns a silhouette sample struct.

        Parameter ``scene`` (~:py:obj:`mitsuba.Scene`):
            *no description available*

        Parameter ``viewpoint`` (~:py:obj:`mitsuba.Point3f`):
            *no description available*

        Parameter ``sample2`` (~:py:obj:`mitsuba.Point2f`):
            *no description available*

        Parameter ``active`` (~drjit.llvm.ad.Bool):
            Mask to specify active lanes.

        Returns → ~:py:obj:`mitsuba.SilhouetteSample3f`:
            *no description available*

    .. py:method:: mitsuba.ad.ProjectiveDetail.perspective_sensor_jacobian(sensor, ss)

        The silhouette sample `ss` stores (1) the sampling density in the scene
        space, and (2) the motion of the silhouette point in the scene space.
        This Jacobian corrects both quantities to the camera sample space.

        Parameter ``sensor`` (~:py:obj:`mitsuba.Sensor`):
            *no description available*

        Parameter ``ss`` (~:py:obj:`mitsuba.SilhouetteSample3f`):
            *no description available*

    .. py:method:: mitsuba.ad.ProjectiveDetail.eval_primary_silhouette_radiance_difference()

        Compute the difference in radiance between two rays that hit and miss a
        silhouette point ``ss.p`` viewed from ``viewpoint``.

        Returns → ~drjit.llvm.ad.Float:
            *no description available*

    .. py:method:: mitsuba.ad.ProjectiveDetail.get_projected_points(scene, sensor, sampler)

        Helper function to project seed rays to obtain silhouette segments and
        map them to boundary sample space.

        Parameter ``scene`` (~:py:obj:`mitsuba.Scene`):
            *no description available*

        Parameter ``sensor`` (~:py:obj:`mitsuba.Sensor`):
            *no description available*

        Parameter ``sampler`` (~:py:obj:`mitsuba.Sampler`):
            *no description available*

    .. py:method:: mitsuba.ad.ProjectiveDetail.init_indirect_silhouette(scene, sensor, seed)

        Initialize the guiding structure for indirect discontinuous derivatives
        based on the guiding mode. The result is stored in this python class.

        Parameter ``scene`` (~:py:obj:`mitsuba.Scene`):
            *no description available*

        Parameter ``sensor`` (~:py:obj:`mitsuba.Sensor`):
            *no description available*

        Parameter ``seed`` (~drjit.llvm.ad.UInt):
            *no description available*

    .. py:method:: mitsuba.ad.ProjectiveDetail.init_indirect_silhouette_grid_unif()

        Guiding structure initialization for uniform grid sampling.

    .. py:method:: mitsuba.ad.ProjectiveDetail.init_indirect_silhouette_grid_proj()

        Guiding structure initialization for projective grid sampling.

    .. py:method:: mitsuba.ad.ProjectiveDetail.init_indirect_silhouette_octree()

        Guiding structure initialization for octree-based guiding.

    .. py:method:: mitsuba.ad.ProjectiveDetail.eval_indirect_integrand(scene, sensor, sample, sampler, preprocess, active=True)

        Evaluate the indirect discontinuous derivatives integral for a given
        sample point in boundary sample space.

        Parameters ``sample`` (``mi.Point3f``):
            The sample point in boundary sample space.

        This function returns a tuple ``(result, sensor_uv)`` where

        Output ``result`` (``mi.Spectrum``):
            The integrand of the indirect discontinuous derivatives.

        Output ``sensor_uv`` (``mi.Point2f``):
            The UV coordinates on the sensor film to splat the result to. If
            ``preprocess`` is false, this coordinate is not used.

        Parameter ``scene`` (~:py:obj:`mitsuba.Scene`):
            *no description available*

        Parameter ``sensor`` (~:py:obj:`mitsuba.Sensor`):
            *no description available*

        Parameter ``sample`` (~:py:obj:`mitsuba.Vector3f`):
            *no description available*

        Parameter ``sampler`` (~:py:obj:`mitsuba.Sampler`):
            *no description available*

        Parameter ``preprocess`` (bool):
            *no description available*

        Parameter ``active`` (~drjit.llvm.ad.Bool):
            Mask to specify active lanes.

    .. py:class:: mitsuba.ad.ProjectiveDetail.ProjectOperation

                Projection operation takes a seed ray as input and outputs a
                
        ef SilhouetteSample3f object.
                

    .. py:method:: mitsuba.ad.ProjectiveDetail.ProjectOperation.eval()

        Dispatches the seed surface interaction object to the appropriate
        shape's projection algorithm.

.. py:class:: mitsuba.ad.SGD

    Base class: :py:obj:`mitsuba.ad.optimizers.Optimizer`

    Implements basic stochastic gradient descent with a fixed learning rate
    and, optionally, momentum :cite:`Sutskever2013Importance` (0.9 is a typical
    parameter value for the ``momentum`` parameter).

    The momentum-based SGD uses the update equation

    .. math::

        v_{i+1} = \mu \cdot v_i +  g_{i+1}

    .. math::
        p_{i+1} = p_i + \varepsilon \cdot v_{i+1},

    where :math:`v` is the velocity, :math:`p` are the positions,
    :math:`\varepsilon` is the learning rate, and :math:`\mu` is
    the momentum parameter.

    .. py:method:: __init__(params=None)

        Parameter ``lr``:
            learning rate
        
        Parameter ``momentum``:
            momentum factor
        
        Parameter ``mask_updates``:
            if enabled, parameters and state variables will only be updated
            in a given iteration if it received nonzero gradients in that iteration.
            This only has an effect if momentum is enabled.
            See :py:class:`mitsuba.optimizers.Adam`'s documentation for more details.
        
        Parameter ``params`` (:py:class:`dict`):
            Optional dictionary-like object containing parameters to optimize.

        Parameter ``params`` (dict | None):
            *no description available*

        
    .. py:method:: mitsuba.ad.SGD.step()

        Take a gradient step

    .. py:method:: mitsuba.ad.SGD.reset()

        Zero-initializes the internal state associated with a parameter

.. py:class:: mitsuba.ad.UniformDistr

    Base class: :py:obj:`mitsuba.ad.guiding.BaseGuidingDistr`

    .. py:method:: mitsuba.ad.UniformDistr.sample()

        Return a sample in U^3 from the stored guiding distribution and its
        reciprocal density.

.. py:class:: mitsuba.ad.integrators.common.ADIntegrator

    Base class: :py:obj:`mitsuba.CppADIntegrator`

    Abstract base class of numerous differentiable integrators in Mitsuba

    .. pluginparameters::

     * - max_depth
         - |int|
         - Specifies the longest path depth in the generated output image (where -1
           corresponds to :math:`\infty`). A value of 1 will only render directly
           visible light sources. 2 will lead to single-bounce (direct-only)
           illumination, and so on. (Default: 6)
     * - rr_depth
         - |int|
         - Specifies the path depth, at which the implementation will begin to use
           the *russian roulette* path termination criterion. For example, if set to
           1, then path generation many randomly cease after encountering directly
           visible surfaces. (Default: 5)

    .. py:method:: __init__(self, arg)

        Parameter ``arg`` (:py:obj:`mitsuba.Properties`, /):
            *no description available*


    .. py:method:: mitsuba.ad.integrators.common.ADIntegrator.render(self, scene, sensor, seed=0, spp=0, develop=True, evaluate=True)

        Overloaded function.

        1. ``render(self, scene: :py:obj:`mitsuba.Scene`, sensor: :py:obj:`mitsuba.Sensor`, seed: int = 0, spp: int = 0, develop: bool = True, evaluate: bool = True) -> drjit.llvm.ad.TensorXf``

        Render the scene

        This function renders the scene from the viewpoint of ``sensor``. All
        other parameters are optional and control different aspects of the
        rendering process. In particular:

        Parameter ``seed`` (int):
            This parameter controls the initialization of the random number
            generator. It is crucial that you specify different seeds (e.g.,
            an increasing sequence) if subsequent ``render``() calls should
            produce statistically independent images.

        Parameter ``spp`` (int):
            Set this parameter to a nonzero value to override the number of
            samples per pixel. This value then takes precedence over whatever
            was specified in the construction of ``sensor->sampler()``. This
            parameter may be useful in research applications where an image
            must be rendered multiple times using different quality levels.

        Parameter ``develop`` (bool):
            If set to ``True``, the implementation post-processes the data
            stored in ``sensor->film()``, returning the resulting image as a
            TensorXf. Otherwise, it returns an empty tensor.

        Parameter ``evaluate`` (bool):
            This parameter is only relevant for JIT variants of Mitsuba (LLVM,
            CUDA). If set to ``True``, the rendering step evaluates the
            generated image and waits for its completion. A log message also
            denotes the rendering time. Otherwise, the returned tensor
            (``develop=true``) or modified film (``develop=false``) represent
            the rendering task as an unevaluated computation graph.

        2. ``render(self, scene: :py:obj:`mitsuba.Scene`, sensor: int = 0, seed: int = 0, spp: int = 0, develop: bool = True, evaluate: bool = True) -> drjit.llvm.ad.TensorXf``

        Render the scene

        This function is just a thin wrapper around the previous render()
        overload. It accepts a sensor *index* instead and renders the scene
        using sensor 0 by default.

        Parameter ``scene`` (:py:obj:`mitsuba.Scene`):
            *no description available*

        Parameter ``sensor`` (:py:obj:`mitsuba.Sensor`):
            *no description available*

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.ad.integrators.common.ADIntegrator.render_forward(self, scene, params, sensor, seed=0, spp=0)

        Parameter ``scene`` (:py:obj:`mitsuba.Scene`):
            *no description available*

        Parameter ``params`` (object):
            *no description available*

        Parameter ``sensor`` (:py:obj:`mitsuba.Sensor`):
            *no description available*

        Parameter ``seed`` (int):
            *no description available*

        Parameter ``spp`` (int):
            *no description available*

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.ad.integrators.common.ADIntegrator.render_backward(self, scene, params, grad_in, sensor, seed=0, spp=0)

        Parameter ``scene`` (:py:obj:`mitsuba.Scene`):
            *no description available*

        Parameter ``params`` (object):
            *no description available*

        Parameter ``grad_in`` (drjit.llvm.ad.TensorXf):
            *no description available*

        Parameter ``sensor`` (:py:obj:`mitsuba.Sensor`):
            *no description available*

        Parameter ``seed`` (int):
            *no description available*

        Parameter ``spp`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ad.integrators.common.ADIntegrator.sample_rays(scene, sensor, sampler)

        Sample a 2D grid of primary rays for a given sensor

        Returns a tuple containing

        - the set of sampled rays
        - a ray weight (usually 1 if the sensor's response function is sampled
          perfectly)
        - the continuous 2D image-space positions associated with each ray

        Parameter ``scene`` (mi.Scene):
            *no description available*

        Parameter ``sensor`` (mi.Sensor):
            *no description available*

        Parameter ``sampler`` (mi.Sampler):
            *no description available*

        Returns → Tuple[mi.RayDifferential3f, mi.Spectrum, mi.Vector2f, mi.Float]:
            *no description available*

    .. py:method:: mitsuba.ad.integrators.common.ADIntegrator.prepare(sensor, seed=0, spp=0, aovs=[])

        Given a sensor and a desired number of samples per pixel, this function
        computes the necessary number of Monte Carlo samples and then suitably
        seeds the sampler underlying the sensor.

        Returns the created sampler and the final number of samples per pixel
        (which may differ from the requested amount depending on the type of
        ``Sampler`` being used)

        Parameter ``sensor`` (``int``, ``mi.Sensor``):
            Specify a sensor to render the scene from a different viewpoint.

        Parameter ``seed` (``int``)
            This parameter controls the initialization of the random number
            generator during the primal rendering step. It is crucial that you
            specify different seeds (e.g., an increasing sequence) if subsequent
            calls should produce statistically independent images (e.g. to
            de-correlate gradient-based optimization steps).

        Parameter ``spp`` (``int``):
            Optional parameter to override the number of samples per pixel for the
            primal rendering step. The value provided within the original scene
            specification takes precedence if ``spp=0``.

        Parameter ``sensor`` (~:py:obj:`mitsuba.Sensor`):
            *no description available*

        Parameter ``seed`` (~drjit.llvm.ad.UInt):
            *no description available*

        Parameter ``spp`` (int):
            *no description available*

        Parameter ``aovs`` (list):
            *no description available*

    .. py:method:: mitsuba.ad.integrators.common.ADIntegrator.sample(mode, scene, sampler, ray, depth, L, aovs, state_in, active)

        This function does the main work of differentiable rendering and
        remains unimplemented here. It is provided by subclasses of the
        ``RBIntegrator`` interface.

        In those concrete implementations, the function performs a Monte Carlo
        random walk, implementing a number of different behaviors depending on
        the ``mode`` argument. For example in primal mode (``mode ==
        drjit.ADMode.Primal``), it behaves like a normal rendering algorithm
        and estimates the radiance incident along ``ray``.

        In forward mode (``mode == drjit.ADMode.Forward``), it estimates the
        derivative of the incident radiance for a set of scene parameters being
        differentiated. (This requires that these parameters are attached to
        the AD graph and have gradients specified via ``dr.set_grad()``)

        In backward mode (``mode == drjit.ADMode.Backward``), it takes adjoint
        radiance ``δL`` and accumulates it into differentiable scene parameters.

        You are normally *not* expected to directly call this function. Instead,
        use ``mi.render()`` , which performs various necessary
        setup steps to correctly use the functionality provided here.

        The parameters of this function are as follows:

        Parameter ``mode`` (``drjit.ADMode``)
            Specifies whether the rendering algorithm should run in primal or
            forward/backward derivative propagation mode

        Parameter ``scene`` (``mi.Scene``):
            Reference to the scene being rendered in a differentiable manner.

        Parameter ``sampler`` (``mi.Sampler``):
            A pre-seeded sample generator

        Parameter ``depth`` (``mi.UInt32``):
            Path depth of `ray` (typically set to zero). This is mainly useful
            for forward/backward differentiable rendering phases that need to
            obtain an incident radiance estimate. In this case, they may
            recursively invoke ``sample(mode=dr.ADMode.Primal)`` with a nonzero
            depth.

        Parameter ``δL`` (``mi.Spectrum``):
            When back-propagating gradients (``mode == drjit.ADMode.Backward``)
            the ``δL`` parameter should specify the adjoint radiance associated
            with each ray. Otherwise, it must be set to ``None``.

        Parameter ``state_in`` (``Any``):
            The primal phase of ``sample()`` returns a state vector as part of
            its return value. The forward/backward differential phases expect
            that this state vector is provided to them via this argument. When
            invoked in primal mode, it should be set to ``None``.

        Parameter ``active`` (``mi.Bool``):
            This mask array can optionally be used to indicate that some of
            the rays are disabled.

        The function returns a tuple ``(spec, valid, state_out)`` where

        Output ``spec`` (``mi.Spectrum``):
            Specifies the estimated radiance and differential radiance in
            primal and forward mode, respectively.

        Output ``valid`` (``mi.Bool``):
            Indicates whether the rays intersected a surface, which can be used
            to compute an alpha channel.

        Output ``aovs`` (``List[mi.Float]``):
            Integrators may return one or more arbitrary output variables (AOVs).
            The implementation has to guarantee that the number of returned AOVs
            matches the length of self.aov_names().


        Parameter ``mode`` (dr.ADMode):
            *no description available*

        Parameter ``scene`` (mi.Scene):
            *no description available*

        Parameter ``sampler`` (mi.Sampler):
            *no description available*

        Parameter ``ray`` (mi.Ray3f):
            *no description available*

        Parameter ``depth`` (mi.UInt32, δ):
            *no description available*

        Parameter ``L`` (Optional[mi.Spectrum], δ):
            *no description available*

        Parameter ``aovs`` (Optional[mi.Spectrum]):
            *no description available*

        Parameter ``state_in`` (Any):
            *no description available*

        Parameter ``active`` (mi.Bool):
            Mask to specify active lanes.

        Returns → Tuple[mi.Spectrum, mi.Bool, List[mi.Float]]:
            *no description available*

.. py:class:: mitsuba.ad.integrators.common.PSIntegrator

    Base class: :py:obj:`mitsuba.ad.integrators.common.ADIntegrator`

    Abstract base class of projective-sampling/path-space style differentiable
    integrators.

    .. py:method:: __init__(self, arg)

        Parameter ``arg`` (:py:obj:`mitsuba.Properties`, /):
            *no description available*


    .. py:method:: mitsuba.ad.integrators.common.PSIntegrator.override_spp(integrator_spp, runtime_spp, sampler_spp)

        Utility method to override the intergrator's spp value with the one
        received at runtime in `render`/`render_backward`/`render_forward`.

        The runtime value is overriden only if it is 0 and if the integrator
        has defined a spp value. If the integrator hasn't defined a value, the
        sampler's spp is used.

        Parameter ``integrator_spp`` (int):
            *no description available*

        Parameter ``runtime_spp`` (int):
            *no description available*

        Parameter ``sampler_spp`` (int):
            *no description available*

    .. py:method:: mitsuba.ad.integrators.common.PSIntegrator.render_ad(scene, sensor, seed, spp, mode)

        Renders and accumulates the outputs of the primarily visible
        discontinuities, indirect discontinuities and continuous derivatives.
        It outputs an attached tensor which should subsequently be traversed by
        a call to `dr.forward`/`dr.backward`/`dr.enqueue`/`dr.traverse`.

        Note: The continuous derivatives are only attached if
        `radiative_backprop` is `False`. When using RB for the continuous
        derivatives it should be manually added to the gradient obtained by
        traversing the result of this method.

        Parameter ``scene`` (mi.Scene):
            *no description available*

        Parameter ``sensor`` (Union[int, mi.Sensor]):
            *no description available*

        Parameter ``seed`` (mi.UInt32):
            *no description available*

        Parameter ``spp`` (int):
            *no description available*

        Parameter ``mode`` (dr.ADMode):
            *no description available*

        Returns → mi.TensorXf:
            *no description available*

    .. py:method:: mitsuba.ad.integrators.common.PSIntegrator.render_forward(self, scene, params, sensor, seed=0, spp=0)

        Parameter ``scene`` (:py:obj:`mitsuba.Scene`):
            *no description available*

        Parameter ``params`` (object):
            *no description available*

        Parameter ``sensor`` (:py:obj:`mitsuba.Sensor`):
            *no description available*

        Parameter ``seed`` (int):
            *no description available*

        Parameter ``spp`` (int):
            *no description available*

        Returns → drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.ad.integrators.common.PSIntegrator.render_backward(self, scene, params, grad_in, sensor, seed=0, spp=0)

        Parameter ``scene`` (:py:obj:`mitsuba.Scene`):
            *no description available*

        Parameter ``params`` (object):
            *no description available*

        Parameter ``grad_in`` (drjit.llvm.ad.TensorXf):
            *no description available*

        Parameter ``sensor`` (:py:obj:`mitsuba.Sensor`):
            *no description available*

        Parameter ``seed`` (int):
            *no description available*

        Parameter ``spp`` (int):
            *no description available*

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ad.integrators.common.PSIntegrator.render_primarily_visible_silhouette(scene, sensor, sampler, spp)

        Renders the primarily visible discontinuities.

        This method returns the AD-attached image. The result must still be
        traversed using one of the Dr.Jit functions to propagate gradients.

        Parameter ``scene`` (~:py:obj:`mitsuba.Scene`):
            *no description available*

        Parameter ``sensor`` (~:py:obj:`mitsuba.Sensor`):
            *no description available*

        Parameter ``sampler`` (~:py:obj:`mitsuba.Sampler`):
            *no description available*

        Parameter ``spp`` (int):
            *no description available*

        Returns → ~drjit.llvm.ad.TensorXf:
            *no description available*

    .. py:method:: mitsuba.ad.integrators.common.PSIntegrator.sample_radiance_difference()

        Sample the radiance difference of two rays that hit and miss the
        silhouette point `ss.p` with direction `ss.d`.

        Parameters ``curr_depth`` (``mi.UInt32``):
            The current depth of the boundary segment, including the boundary
            segment itself.

        This function returns a tuple ``(ΔL, active)`` where

        Output ``ΔL`` (``mi.Spectrum``):
            The estimated radiance difference of the foreground and background.

        Output ``active`` (``mi.Bool``):
            Indicates if the radiance difference is valid.

    .. py:method:: mitsuba.ad.integrators.common.PSIntegrator.sample_importance()

        Sample the incident importance at the silhouette point `ss.p` with
        direction `-ss.d`. If multiple connections to the sensor are valid, this
        method uses reservoir sampling to pick one.

        Parameters ``max_depth`` (``mi.UInt32``):
            The maximum number of ray segments to reach the sensor.

        The function returns a tuple ``(importance, uv, depth, boundary_p,
        valid)`` where

        Output ``importance`` (``mi.Spectrum``):
            The sampled importance along the constructed path.

        Output ``uv`` (``mi.Point2f``):
            The sensor splatting coordinates.

        Output ``depth`` (``mi.UInt32``):
            The number of segments of the sampled path from the boundary
            segment to the sensor, including the boundary segment itself.

        Output ``boundary_p`` (``mi.Point3f``):
            The attached sensor-side intersection point of the boundary segment.

        Output ``valid`` (``mi.Bool``):
            Indicates if a valid path is found.

    .. py:method:: mitsuba.ad.integrators.common.PSIntegrator.sample(mode, scene, sampler, ray, depth, L, aovs, state_in, active, project=False, si_shade=None)

        See ADIntegrator.sample() for a description of this function's purpose.

        Parameter ``depth`` (``mi.UInt32``):
            Path depth of `ray` (typically set to zero). This is mainly useful
            for forward/backward differentiable rendering phases that need to
            obtain an incident radiance estimate. In this case, they may
            recursively invoke ``sample(mode=dr.ADMode.Primal)`` with a nonzero
            depth.

        Parameter ``project`` (``bool``):
            If set to ``True``, the integrator also returns the sampled
            ``seedrays`` along the Monte Carlo path. This is useful for
            projective integrators to handle discontinuous derivatives.

        Parameter ``si_shade`` (``mi.SurfaceInteraction3f``):
            If set to a valid surface interaction, the integrator will use this
            as the first ray interaction point to skip one ray tracing with the
            given ``ray``. This is useful to estimate the incident radiance at a
            given surface point that is already known to the integrator.

        Output ``spec`` (``mi.Spectrum``):
            Specifies the estimated radiance and differential radiance in primal
            and forward mode, respectively.

        Output ``valid`` (``mi.Bool``):
            Indicates whether the rays intersected a surface, which can be used
            to compute an alpha channel.

        Output ``aovs`` (``Sequence[mi.Float]``):
            Integrators may return one or more arbitrary output variables (AOVs).
            The implementation has to guarantee that the number of returned AOVs
            matches the length of self.aov_names().

        Output ``seedray`` / ``state_out`` (``any``):
            If ``project`` is true, the integrator returns the seed rays to be
            projected as the third output. The seed rays is a python list of
            rays and their validity mask. It is possible that no segment can be
            projected along a light path.

            If ``project`` is false, the integrator returns the state vector
            returned by the primal phase of ``sample()`` as the third output.
            This is only used by the radiative-backpropagation style
            integrators.

        Parameter ``mode`` (dr.ADMode):
            *no description available*

        Parameter ``scene`` (mi.Scene):
            *no description available*

        Parameter ``sampler`` (mi.Sampler):
            *no description available*

        Parameter ``ray`` (mi.Ray3f):
            *no description available*

        Parameter ``depth`` (mi.UInt32, δ):
            *no description available*

        Parameter ``L`` (Optional[mi.Spectrum], δ):
            *no description available*

        Parameter ``aovs`` (Optional[mi.Spectrum]):
            *no description available*

        Parameter ``state_in`` (Any):
            *no description available*

        Parameter ``active`` (mi.Bool):
            Mask to specify active lanes.

        Parameter ``project`` (bool):
            *no description available*

        Parameter ``si_shade`` (Optional[mi.SurfaceInteraction3f]):
            *no description available*

        Returns → Tuple[mi.Spectrum, mi.Bool, List[mi.Float], Any]:
            *no description available*

.. py:class:: mitsuba.ad.integrators.common.RBIntegrator

    Base class: :py:obj:`mitsuba.ad.integrators.common.ADIntegrator`

    Abstract base class of radiative-backpropagation style differentiable
    integrators.

    .. py:method:: __init__(self, arg)

        Parameter ``arg`` (:py:obj:`mitsuba.Properties`, /):
            *no description available*


    .. py:method:: mitsuba.ad.integrators.common.RBIntegrator.render_forward(scene, params, sensor=0, seed=0, spp=0)

        Evaluates the forward-mode derivative of the rendering step.

        Forward-mode differentiation propagates gradients from scene parameters
        through the simulation, producing a *gradient image* (i.e., the derivative
        of the rendered image with respect to those scene parameters). The gradient
        image is very helpful for debugging, for example to inspect the gradient
        variance or visualize the region of influence of a scene parameter. It is
        not particularly useful for simultaneous optimization of many parameters,
        since multiple differentiation passes are needed to obtain separate
        derivatives for each scene parameter. See ``Integrator.render_backward()``
        for an efficient way of obtaining all parameter derivatives at once, or
        simply use the ``mi.render()`` abstraction that hides both
        ``Integrator.render_forward()`` and ``Integrator.render_backward()`` behind
        a unified interface.

        Before calling this function, you must first enable gradient tracking and
        furthermore associate concrete input gradients with one or more scene
        parameters, or the function will just return a zero-valued gradient image.
        This is typically done by invoking ``dr.enable_grad()`` and
        ``dr.set_grad()`` on elements of the ``SceneParameters`` data structure
        that can be obtained obtained via a call to
        ``mi.traverse()``.

        Parameter ``scene`` (``mi.Scene``):
            The scene to be rendered differentially.

        Parameter ``params`` (Any):
           An arbitrary container of scene parameters that should receive
           gradients. Typically this will be an instance of type
           ``mi.SceneParameters`` obtained via ``mi.traverse()``. However, it
           could also be a Python list/dict/object tree (DrJit will traverse it
           to find all parameters). Gradient tracking must be explicitly enabled
           for each of these parameters using ``dr.enable_grad(params['parameter_name'])``
           (i.e. ``render_forward()`` will not do this for you). Furthermore,
           ``dr.set_grad(...)`` must be used to associate specific gradient values
           with each parameter.

        Parameter ``sensor`` (``int``, ``mi.Sensor``):
            Specify a sensor or a (sensor index) to render the scene from a
            different viewpoint. By default, the first sensor within the scene
            description (index 0) will take precedence.

        Parameter ``seed` (``int``)
            This parameter controls the initialization of the random number
            generator. It is crucial that you specify different seeds (e.g., an
            increasing sequence) if subsequent calls should produce statistically
            independent images (e.g. to de-correlate gradient-based optimization
            steps).

        Parameter ``spp`` (``int``):
            Optional parameter to override the number of samples per pixel for the
            differential rendering step. The value provided within the original
            scene specification takes precedence if ``spp=0``.

        Parameter ``scene`` (mi.Scene):
            *no description available*

        Parameter ``sensor`` (Union[int, mi.Sensor]):
            *no description available*

        Parameter ``seed`` (mi.UInt32):
            *no description available*

        Parameter ``spp`` (int):
            *no description available*

        Returns → mi.TensorXf:
            *no description available*

    .. py:method:: mitsuba.ad.integrators.common.RBIntegrator.render_backward(scene, params, grad_in, sensor=0, seed=0, spp=0)

        Evaluates the reverse-mode derivative of the rendering step.

        Reverse-mode differentiation transforms image-space gradients into scene
        parameter gradients, enabling simultaneous optimization of scenes with
        millions of free parameters. The function is invoked with an input
        *gradient image* (``grad_in``) and transforms and accumulates these into
        the gradient arrays of scene parameters that previously had gradient
        tracking enabled.

        Before calling this function, you must first enable gradient tracking for
        one or more scene parameters, or the function will not do anything. This is
        typically done by invoking ``dr.enable_grad()`` on elements of the
        ``SceneParameters`` data structure that can be obtained obtained via a call
        to ``mi.traverse()``. Use ``dr.grad()`` to query the
        resulting gradients of these parameters once ``render_backward()`` returns.

        Parameter ``scene`` (``mi.Scene``):
            The scene to be rendered differentially.

        Parameter ``params`` (Any):
           An arbitrary container of scene parameters that should receive
           gradients. Typically this will be an instance of type
           ``mi.SceneParameters`` obtained via ``mi.traverse()``. However, it
           could also be a Python list/dict/object tree (DrJit will traverse it
           to find all parameters). Gradient tracking must be explicitly enabled
           for each of these parameters using ``dr.enable_grad(params['parameter_name'])``
           (i.e. ``render_backward()`` will not do this for you).

        Parameter ``grad_in`` (``mi.TensorXf``):
            Gradient image that should be back-propagated.

        Parameter ``sensor`` (``int``, ``mi.Sensor``):
            Specify a sensor or a (sensor index) to render the scene from a
            different viewpoint. By default, the first sensor within the scene
            description (index 0) will take precedence.

        Parameter ``seed` (``int``)
            This parameter controls the initialization of the random number
            generator. It is crucial that you specify different seeds (e.g., an
            increasing sequence) if subsequent calls should produce statistically
            independent images (e.g. to de-correlate gradient-based optimization
            steps).

        Parameter ``spp`` (``int``):
            Optional parameter to override the number of samples per pixel for the
            differential rendering step. The value provided within the original
            scene specification takes precedence if ``spp=0``.

        Parameter ``scene`` (mi.Scene):
            *no description available*

        Parameter ``grad_in`` (mi.TensorXf):
            *no description available*

        Parameter ``sensor`` (Union[int, mi.Sensor]):
            *no description available*

        Parameter ``seed`` (mi.UInt32):
            *no description available*

        Parameter ``spp`` (int):
            *no description available*

        Returns → None:
            *no description available*

.. py:function:: mitsuba.ad.integrators.common.mis_weight()

    Compute the Multiple Importance Sampling (MIS) weight given the densities
    of two sampling strategies according to the power heuristic.

.. py:class:: mitsuba.ad.largesteps.SolveCholesky

    DrJIT custom operator to solve a linear system using a Cholesky factorization.

    .. py:method:: __init__()


    .. py:method:: mitsuba.ad.largesteps.SolveCholesky.eval(self)

        Evaluate the custom operation in primal mode.

        You must implement this method when subclassing :py:class:`CustomOp`, since the
        default implementation raises an exception. It should realize the original
        (non-derivative-aware) form of a computation and may take an arbitrary sequence
        of positional, keyword, and variable-length positional/keyword arguments.

        You should not need to call this function yourself---Dr.Jit will automatically do so
        when performing custom operations through the :py:func:`drjit.custom()` interface.

        Note that the input arguments passed to ``.eval()`` will be *detached* (i.e.
        they don't have derivative tracking enabled). This is intentional, since
        derivative tracking is handled by the custom operation along with the other
        callbacks :py:func:`forward` and :py:func:`backward`.

        Returns → object:
            *no description available*

    .. py:method:: mitsuba.ad.largesteps.SolveCholesky.forward()

        Evaluate the forward derivative of the custom operation.

        You must implement this method when subclassing :py:class:`CustomOp`, since the
        default implementation raises an exception. It takes no arguments and has no
        return value.

        An implementation will generally perform repeated calls to :py:func:`grad_in`
        to query the gradients of all function followed by a single call to
        :py:func:`set_grad_out` to set the gradient of the return value.

        For example, this is how one would implement the product rule of the primal
        calculation ``x*y``, assuming that the ``.eval()`` routine stashed the inputs
        in the custom operation object.

        .. code-block:: python

           def forward(self):
               self.set_grad_out(self.y * self.grad_in('x') + self.x * self.grad_in('y'))

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ad.largesteps.SolveCholesky.backward()

        Evaluate the backward derivative of the custom operation.

        You must implement this method when subclassing :py:class:`CustomOp`, since the
        default implementation raises an exception. It takes no arguments and has no
        return value.

        An implementation will generally perform a single call to :py:func:`grad_out`
        to query the gradient of the function return value followed by a sequence of calls to
        :py:func:`set_grad_in` to assign the gradients of the function inputs.

        For example, this is how one would implement the product rule of the primal
        calculation ``x*y``, assuming that the ``.eval()`` routine stashed the inputs
        in the custom operation object.

        .. code-block:: python

           def backward(self):
               self.set_grad_in('x', self.y * self.grad_out())
               self.set_grad_in('y', self.x * self.grad_out())

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.ad.largesteps.SolveCholesky.name()

        Return a descriptive name of the ``CustomOp`` instance.

        Amongst other things, this name is used to document the presence of the
        custom operation in GraphViz debug output. (See :py:func:`graphviz_ad`.)

        Returns → str:
            *no description available*

.. py:function:: mitsuba.ad.largesteps.mesh_laplacian()

    Compute the index and data arrays of the (combinatorial) Laplacian matrix of
    a given mesh.

.. py:function:: mitsuba.chi2.BSDFAdapter()

    Adapter to test BSDF sampling using the Chi^2 test.

    Parameter ``bsdf_type`` (string):
        Name of the BSDF plugin to instantiate.

    Parameter ``extra`` (string|dict):
        Additional XML used to specify the BSDF's parameters, or a Python
        dictionary as used by the ``load_dict`` routine.

    Parameter ``wi`` (array(3,)):
        Incoming direction, in local coordinates.

.. py:class:: mitsuba.chi2.ChiSquareTest

    Implements Pearson's chi-square test for goodness of fit of a distribution
    to a known reference distribution.

    The implementation here specifically compares a Monte Carlo sampling
    strategy on a 2D (or lower dimensional) space against a reference
    distribution obtained by numerically integrating a probability density
    function over grid in the distribution's parameter domain.

    Parameter ``domain`` (object):
       An implementation of the domain interface (``SphericalDomain``, etc.),
       which transforms between the parameter and target domain of the
       distribution

    Parameter ``sample_func`` (function):
       An importance sampling function which maps an array of uniform variates
       of size ``[sample_dim, sample_count]`` to an array of ``sample_count``
       samples on the target domain.

    Parameter ``pdf_func`` (function):
       Function that is expected to specify the probability density of the
       samples produced by ``sample_func``. The test will try to collect
       sufficient statistical evidence to reject this hypothesis.

    Parameter ``sample_dim`` (int):
       Number of random dimensions consumed by ``sample_func`` per sample. The
       default value is ``2``.

    Parameter ``sample_count`` (int):
       Total number of samples to be generated. The test will have more
       evidence as this number tends to infinity. The default value is
       ``1000000``.

    Parameter ``res`` (int):
       Vertical resolution of the generated histograms. The horizontal
       resolution will be calculated as ``res * domain.aspect()``. The
       default value of ``101`` is intentionally an odd number to prevent
       issues with floating point precision at sharp boundaries that may
       separate the domain into two parts (e.g. top hemisphere of a sphere
       parameterization).

    Parameter ``ires`` (int):
       Number of horizontal/vertical subintervals used to numerically integrate
       the probability density over each histogram cell (using the trapezoid
       rule). The default value is ``4``.

    Parameter ``seed`` (int):
       Seed value for the PCG32 random number generator used in the histogram
       computation. The default value is ``0``.

    Notes:

    The following attributes are part of the public API:

    messages: string
        The implementation may generate a number of messages while running the
        test, which can be retrieved via this attribute.

    histogram: array
        The histogram array is populated by the ``tabulate_histogram()`` method
        and stored in this attribute.

    pdf: array
        The probability density function array is populated by the
        ``tabulate_pdf()`` method and stored in this attribute.

    p_value: float
        The p-value of the test is computed in the ``run()`` method and stored
        in this attribute.

    .. py:method:: mitsuba.chi2.ChiSquareTest.tabulate_histogram()

        Invoke the provided sampling strategy many times and generate a
        histogram in the parameter domain. If ``sample_func`` returns a tuple
        ``(positions, weights)`` instead of just positions, the samples are
        considered to be weighted.

    .. py:method:: mitsuba.chi2.ChiSquareTest.tabulate_pdf()

        Numerically integrate the provided probability density function over
        each cell to generate an array resembling the histogram computed by
        ``tabulate_histogram()``. The function uses the trapezoid rule over
        intervals discretized into ``self.ires`` separate function evaluations.

    .. py:method:: mitsuba.chi2.ChiSquareTest.run()

        Run the Chi^2 test

        Parameter ``significance_level`` (float):
            Denotes the desired significance level (e.g. 0.01 for a test at the
            1% significance level)

        Parameter ``test_count`` (int):
            Specifies the total number of statistical tests run by the user.
            This value will be used to adjust the provided significance level
            so that the combination of the entire set of tests has the provided
            significance level.

        Returns → bool:
            ``True`` upon success, ``False`` if the null hypothesis was
            rejected.


.. py:function:: mitsuba.chi2.EmitterAdapter()

    Adapter to test Emitter sampling using the Chi^2 test.

    Parameter ``emitter_type`` (string):
        Name of the emitter plugin to instantiate.

    Parameter ``extra`` (string|dict):
        Additional XML used to specify the emitter's parameters, or a Python
        dictionary as used by the ``load_dict`` routine.

.. py:class:: mitsuba.chi2.LineDomain

    The identity map on the line.

.. py:function:: mitsuba.chi2.MicrofacetAdapter()

    Adapter for testing microfacet distribution sampling techniques
    (separately from BSDF models, which are also tested)

.. py:function:: mitsuba.chi2.PhaseFunctionAdapter()

    Adapter to test phase function sampling using the Chi^2 test.

    Parameter ``phase_type`` (string):
        Name of the phase function plugin to instantiate.

    Parameter ``extra`` (string|dict):
        Additional XML used to specify the phase function's parameters, or a
        Python dictionary as used by the ``load_dict`` routine.

    Parameter ``wi`` (array(3,)):
        Incoming direction, in local coordinates.

.. py:class:: mitsuba.chi2.PlanarDomain

    The identity map on the plane

.. py:function:: mitsuba.chi2.SpectrumAdapter()

    Adapter which permits testing 1D spectral power distributions using the
    Chi^2 test.

.. py:class:: mitsuba.chi2.SphericalDomain

    Maps between the unit sphere and a [cos(theta), phi] parameterization.

.. py:function:: mitsuba.cie1931_xyz(wavelength)

    Evaluate the CIE 1931 XYZ color matching functions given a wavelength
    in nanometers

    Parameter ``wavelength`` (drjit.llvm.ad.Float):
        *no description available*

    Returns → :py:obj:`mitsuba.Color3f`:
        *no description available*

.. py:function:: mitsuba.cie1931_y(wavelength)

    Evaluate the CIE 1931 Y color matching function given a wavelength in
    nanometers

    Parameter ``wavelength`` (drjit.llvm.ad.Float):
        *no description available*

    Returns → drjit.llvm.ad.Float:
        *no description available*

.. py:function:: mitsuba.cie_d65(wavelength)

    Evaluate the CIE D65 illuminant spectrum given a wavelength in
    nanometers, normalized to ensures that it integrates to a luminance of
    1.0.

    Parameter ``wavelength`` (drjit.llvm.ad.Float):
        *no description available*

    Returns → drjit.llvm.ad.Float:
        *no description available*

.. py:function:: mitsuba.coordinate_system(n)

    Complete the set {a} to an orthonormal basis {a, b, c}

    Parameter ``n`` (:py:obj:`mitsuba.Vector3f`):
        *no description available*

    Returns → tuple[:py:obj:`mitsuba.Vector3f`, :py:obj:`mitsuba.Vector3f`]:
        *no description available*

.. py:function:: mitsuba.cornell_box()

    Returns a dictionary containing a description of the Cornell Box scene.

.. py:function:: mitsuba.depolarizer(arg)

    Parameter ``arg`` (:py:obj:`mitsuba.Color3f`, /):
        *no description available*

    Returns → :py:obj:`mitsuba.Color3f`:
        *no description available*

.. py:function:: mitsuba.detail.add_variant_callback(arg)

    Parameter ``arg`` (collections.abc.Callable, /):
        *no description available*

    Returns → None:
        *no description available*

.. py:function:: mitsuba.detail.clear_variant_callbacks()

    Returns → None:
        *no description available*

.. py:function:: mitsuba.detail.remove_variant_callback(arg)

    Parameter ``arg`` (collections.abc.Callable, /):
        *no description available*

    Returns → None:
        *no description available*

.. py:function:: mitsuba.eval_reflectance(type, alpha_u, alpha_v, wi, eta)

    Parameter ``type`` (:py:obj:`mitsuba.MicrofacetType`):
        *no description available*

    Parameter ``alpha_u`` (float):
        *no description available*

    Parameter ``alpha_v`` (float):
        *no description available*

    Parameter ``wi`` (:py:obj:`mitsuba.Vector3f`):
        *no description available*

    Parameter ``eta`` (float):
        *no description available*

    Returns → drjit.llvm.ad.Float:
        *no description available*

.. py:function:: mitsuba.filesystem.absolute(arg)

    Returns an absolute path to the same location pointed by ``p``,
    relative to ``base``.

    See also:
        http ://en.cppreference.com/w/cpp/experimental/fs/absolute)

    Parameter ``arg`` (:py:obj:`mitsuba.filesystem.path`, /):
        *no description available*

    Returns → :py:obj:`mitsuba.filesystem.path`:
        *no description available*

.. py:function:: mitsuba.filesystem.create_directory(arg)

    Creates a directory at ``p`` as if ``mkdir`` was used. Returns true if
    directory creation was successful, false otherwise. If ``p`` already
    exists and is already a directory, the function does nothing (this
    condition is not treated as an error).

    Parameter ``arg`` (:py:obj:`mitsuba.filesystem.path`, /):
        *no description available*

    Returns → bool:
        *no description available*

.. py:function:: mitsuba.filesystem.current_path()

    Returns the current working directory (equivalent to getcwd)

    Returns → :py:obj:`mitsuba.filesystem.path`:
        *no description available*

.. py:function:: mitsuba.filesystem.equivalent(arg0, arg1)

    Checks whether two paths refer to the same file system object. Both
    must refer to an existing file or directory. Symlinks are followed to
    determine equivalence.

    Parameter ``arg0`` (:py:obj:`mitsuba.filesystem.path`):
        *no description available*

    Parameter ``arg1`` (:py:obj:`mitsuba.filesystem.path`, /):
        *no description available*

    Returns → bool:
        *no description available*

.. py:function:: mitsuba.filesystem.exists(arg)

    Checks if ``p`` points to an existing filesystem object.

    Parameter ``arg`` (:py:obj:`mitsuba.filesystem.path`, /):
        *no description available*

    Returns → bool:
        *no description available*

.. py:function:: mitsuba.filesystem.file_size(arg)

    Returns the size (in bytes) of a regular file at ``p``. Attempting to
    determine the size of a directory (as well as any other file that is
    not a regular file or a symlink) is treated as an error.

    Parameter ``arg`` (:py:obj:`mitsuba.filesystem.path`, /):
        *no description available*

    Returns → int:
        *no description available*

.. py:function:: mitsuba.filesystem.is_directory(arg)

    Checks if ``p`` points to a directory.

    Parameter ``arg`` (:py:obj:`mitsuba.filesystem.path`, /):
        *no description available*

    Returns → bool:
        *no description available*

.. py:function:: mitsuba.filesystem.is_regular_file(arg)

    Checks if ``p`` points to a regular file, as opposed to a directory or
    symlink.

    Parameter ``arg`` (:py:obj:`mitsuba.filesystem.path`, /):
        *no description available*

    Returns → bool:
        *no description available*

.. py:class:: mitsuba.filesystem.path

    Represents a path to a filesystem resource. On construction, the path
    is parsed and stored in a system-agnostic representation. The path can
    be converted back to the system-specific string using ``native()`` or
    ``string()``.

    .. py:method:: __init__()

        Overloaded function.
        
        1. ``__init__(self) -> None``
        
        Default constructor. Constructs an empty path. An empty path is
        considered relative.
        
        2. ``__init__(self, arg: :py:obj:`mitsuba.filesystem.path`) -> None``
        
        Copy constructor.
        
        3. ``__init__(self, arg: str, /) -> None``
        
        Construct a path from a string with native type. On Windows, the path
        can use both '/' or '\\' as a delimiter.

        
    .. py:method:: mitsuba.filesystem.path.clear()

        Makes the path an empty path. An empty path is considered relative.

        Returns → None:
            *no description available*

    .. py:method:: mitsuba.filesystem.path.empty()

        Checks if the path is empty

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.filesystem.path.extension()

        Returns the extension of the filename component of the path (the
        substring starting at the rightmost period, including the period).
        Special paths '.' and '..' have an empty extension.

        Returns → :py:obj:`mitsuba.filesystem.path`:
            *no description available*

    .. py:method:: mitsuba.filesystem.path.filename()

        Returns the filename component of the path, including the extension.

        Returns → :py:obj:`mitsuba.filesystem.path`:
            *no description available*

    .. py:method:: mitsuba.filesystem.path.is_absolute()

        Checks if the path is absolute.

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.filesystem.path.is_relative()

        Checks if the path is relative.

        Returns → bool:
            *no description available*

    .. py:method:: mitsuba.filesystem.path.native()

        Returns the path in the form of a native string, so that it can be
        passed directly to system APIs. The path is constructed using the
        system's preferred separator and the native string type.

        Returns → str:
            *no description available*

    .. py:method:: mitsuba.filesystem.path.parent_path()

        Returns the path to the parent directory. Returns an empty path if it
        is already empty or if it has only one element.

        Returns → :py:obj:`mitsuba.filesystem.path`:
            *no description available*

    .. py:method:: mitsuba.filesystem.path.replace_extension(self, arg)

        Replaces the substring starting at the rightmost '.' symbol by the
        provided string.

        A '.' symbol is automatically inserted if the replacement does not
        start with a dot. Removes the extension altogether if the empty path
        is passed. If there is no extension, appends a '.' followed by the
        replacement. If the path is empty, '.' or '..', the method does
        nothing.

        Returns *this.

        Parameter ``arg`` (:py:obj:`mitsuba.filesystem.path`, /):
            *no description available*

        Returns → :py:obj:`mitsuba.filesystem.path`:
            *no description available*

.. py:data:: mitsuba.filesystem.preferred_separator
    :type: str
    :value: /

.. py:function:: mitsuba.filesystem.remove(arg)

    Removes a file or empty directory. Returns true if removal was
    successful, false if there was an error (e.g. the file did not exist).

    Parameter ``arg`` (:py:obj:`mitsuba.filesystem.path`, /):
        *no description available*

    Returns → bool:
        *no description available*

.. py:function:: mitsuba.filesystem.resize_file(arg0, arg1)

    Changes the size of the regular file named by ``p`` as if ``truncate``
    was called. If the file was larger than ``target_length``, the
    remainder is discarded. The file must exist.

    Parameter ``arg0`` (:py:obj:`mitsuba.filesystem.path`):
        *no description available*

    Parameter ``arg1`` (int, /):
        *no description available*

    Returns → bool:
        *no description available*

.. py:function:: mitsuba.fresnel(cos_theta_i, eta)

    Calculates the unpolarized Fresnel reflection coefficient at a planar
    interface between two dielectrics

    Parameter ``cos_theta_i`` (drjit.llvm.ad.Float):
        Cosine of the angle between the surface normal and the incident
        ray

    Parameter ``eta`` (drjit.llvm.ad.Float):
        Relative refractive index of the interface. A value greater than
        1.0 means that the surface normal is pointing into the region of
        lower density.

    Returns → tuple[drjit.llvm.ad.Float, drjit.llvm.ad.Float, drjit.llvm.ad.Float, drjit.llvm.ad.Float]:
        A tuple (F, cos_theta_t, eta_it, eta_ti) consisting of

    F Fresnel reflection coefficient.

    cos_theta_t Cosine of the angle between the surface normal and the
    transmitted ray

    eta_it Relative index of refraction in the direction of travel.

    eta_ti Reciprocal of the relative index of refraction in the direction
    of travel. This also happens to be equal to the scale factor that must
    be applied to the X and Y component of the refracted direction.

.. py:function:: mitsuba.fresnel_conductor(cos_theta_i, eta)

    Calculates the unpolarized Fresnel reflection coefficient at a planar
    interface of a conductor, i.e. a surface with a complex-valued
    relative index of refraction

    Remark:
        The implementation assumes that cos_theta_i > 0, i.e. light enters
        from *outside* of the conducting layer (generally a reasonable
        assumption unless very thin layers are being simulated)

    Parameter ``cos_theta_i`` (drjit.llvm.ad.Float):
        Cosine of the angle between the surface normal and the incident
        ray

    Parameter ``eta`` (drjit.llvm.ad.Complex2f):
        Relative refractive index (complex-valued)

    Returns → drjit.llvm.ad.Float:
        The unpolarized Fresnel reflection coefficient.

.. py:function:: mitsuba.fresnel_diffuse_reflectance(eta)

    Computes the diffuse unpolarized Fresnel reflectance of a dielectric
    material (sometimes referred to as "Fdr").

    This value quantifies what fraction of diffuse incident illumination
    will, on average, be reflected at a dielectric material boundary

    Parameter ``eta`` (drjit.llvm.ad.Float):
        Relative refraction coefficient

    Returns → drjit.llvm.ad.Float:
        F, the unpolarized Fresnel coefficient.

.. py:function:: mitsuba.fresnel_polarized(cos_theta_i, eta)

    Calculates the polarized Fresnel reflection coefficient at a planar
    interface between two dielectrics or conductors. Returns complex
    values encoding the amplitude and phase shift of the s- and
    p-polarized waves.

    This is the most general version, which subsumes all others (at the
    cost of transcendental function evaluations in the complex-valued
    arithmetic)

    Parameter ``cos_theta_i`` (drjit.llvm.ad.Float):
        Cosine of the angle between the surface normal and the incident
        ray

    Parameter ``eta`` (drjit.llvm.ad.Complex2f):
        Complex-valued relative refractive index of the interface. In the
        real case, a value greater than 1.0 case means that the surface
        normal points into the region of lower density.

    Returns → tuple[drjit.llvm.ad.Complex2f, drjit.llvm.ad.Complex2f, drjit.llvm.ad.Float, drjit.llvm.ad.Complex2f, drjit.llvm.ad.Complex2f]:
        A tuple (a_s, a_p, cos_theta_t, eta_it, eta_ti) consisting of

    a_s Perpendicularly polarized wave amplitude and phase shift.

    a_p Parallel polarized wave amplitude and phase shift.

    cos_theta_t Cosine of the angle between the surface normal and the
    transmitted ray. Zero in the case of total internal reflection.

    eta_it Relative index of refraction in the direction of travel

    eta_ti Reciprocal of the relative index of refraction in the direction
    of travel. In the real-valued case, this also happens to be equal to
    the scale factor that must be applied to the X and Y component of the
    refracted direction.

.. py:function:: mitsuba.has_flag(arg0, arg1)

    Parameter ``arg0`` (int):
        *no description available*

    Parameter ``arg1`` (:py:obj:`mitsuba.EmitterFlags`, /):
        *no description available*

    Returns → bool:
        *no description available*

.. py:data:: mitsuba.is_monochromatic
    :type: bool
    :value: False

.. py:data:: mitsuba.is_polarized
    :type: bool
    :value: False

.. py:data:: mitsuba.is_rgb
    :type: bool
    :value: True

.. py:data:: mitsuba.is_spectral
    :type: bool
    :value: False

.. py:function:: mitsuba.linear_rgb_rec(wavelength)

    Evaluate the ITU-R Rec. BT.709 linear RGB color matching functions
    given a wavelength in nanometers

    Parameter ``wavelength`` (drjit.llvm.ad.Float):
        *no description available*

    Returns → :py:obj:`mitsuba.Color3f`:
        *no description available*

.. py:function:: mitsuba.load_dict(dict, parallel=True)

    Load a Mitsuba scene or object from an Python dictionary

    Parameter ``dict`` (dict):
        Python dictionary containing the object description

    Parameter ``parallel`` (bool):
        Whether the loading should be executed on multiple threads in parallel

    Returns → object:
        *no description available*

.. py:function:: mitsuba.load_file(path, update_scene=False, parallel=True, **kwargs)

    Load a Mitsuba scene from an XML file

    Parameter ``path`` (str):
        Filename of the scene XML file

    Parameter ``parameters``:
        Optional list of parameters that can be referenced as ``$varname``
        in the scene.

    Parameter ``variant``:
        Specifies the variant of plugins to instantiate (e.g.
        "scalar_rgb")

    Parameter ``update_scene`` (bool):
        When Mitsuba updates scene to a newer version, should the updated
        XML file be written back to disk?

    Parameter ``parallel`` (bool):
        Whether the loading should be executed on multiple threads in
        parallel

    Returns → object:
        *no description available*

.. py:function:: mitsuba.load_string(string, parallel=True, **kwargs)

    Load a Mitsuba scene from an XML string

    Parameter ``string`` (str):
        *no description available*

    Parameter ``parallel`` (bool):
        *no description available*

    Returns → object:
        *no description available*

.. py:function:: mitsuba.log_level()

    Returns the current log level.

    Returns → :py:obj:`mitsuba.LogLevel`:
        *no description available*

.. py:function:: mitsuba.lookup_ior(properties, name, default)

    Lookup IOR value in table.

    Parameter ``properties`` (:py:obj:`mitsuba._Properties`):
        *no description available*

    Parameter ``name`` (str):
        *no description available*

    Parameter ``default`` (object):
        *no description available*

    Returns → float:
        *no description available*

.. py:function:: mitsuba.luminance(value, wavelengths, active=True)

    Parameter ``value`` (:py:obj:`mitsuba.Color3f`):
        *no description available*

    Parameter ``wavelengths`` (:py:obj:`mitsuba.Color0f`):
        *no description available*

    Parameter ``active`` (drjit.llvm.ad.Bool):
        Mask to specify active lanes.

    Returns → drjit.llvm.ad.Float:
        *no description available*

.. py:data:: mitsuba.math.RayEpsilon
    :type: float
    :value: 8.940696716308594e-05

.. py:data:: mitsuba.math.ShadowEpsilon
    :type: float
    :value: 0.0008940696716308594

.. py:data:: mitsuba.math.ShapeEpsilon
    :type: float
    :value: 1.1175870895385742e-06

.. py:function:: mitsuba.math.chi2(arg0, arg1, arg2)

    Compute the Chi^2 statistic and degrees of freedom of the given arrays
    while pooling low-valued entries together

    Given a list of observations counts (``obs[i]``) and expected
    observation counts (``exp[i]``), this function accumulates the Chi^2
    statistic, that is, ``(obs-exp)^2 / exp`` for each element ``0, ...,
    n-1``.

    Minimum expected cell frequency. The Chi^2 test statistic is not
    useful when when the expected frequency in a cell is low (e.g. less
    than 5), because normality assumptions break down in this case.
    Therefore, the implementation will merge such low-frequency cells when
    they fall below the threshold specified here. Specifically, low-valued
    cells with ``exp[i] < pool_threshold`` are pooled into larger groups
    that are above the threshold before their contents are added to the
    Chi^2 statistic.

    The function returns the statistic value, degrees of freedom, below-
    threshold entries and resulting number of pooled regions.

    Parameter ``arg0`` (drjit.llvm.ad.Float64):
        *no description available*

    Parameter ``arg1`` (drjit.llvm.ad.Float64):
        *no description available*

    Parameter ``arg2`` (float, /):
        *no description available*

    Returns → tuple[float, int, int, int]:
        *no description available*

.. py:function:: mitsuba.math.find_interval(size, pred)

    Find an interval in an ordered set

    This function performs a binary search to find an index ``i`` such
    that ``pred(i)`` is ``True`` and ``pred(i+1)`` is ``False``, where
    ``pred`` is a user-specified predicate that monotonically decreases
    over this range (i.e. max one ``True`` -> ``False`` transition).

    The predicate will be evaluated exactly <tt>floor(log2(size)) + 1<tt>
    times. Note that the template parameter ``Index`` is automatically
    inferred from the supplied predicate, which takes an index or an index
    vector of type ``Index`` as input argument and can (optionally) take a
    mask argument as well. In the vectorized case, each vector lane can
    use different predicate. When ``pred`` is ``False`` for all entries,
    the function returns ``0``, and when it is ``True`` for all cases, it
    returns <tt>size-2<tt>.

    The main use case of this function is to locate an interval (i, i+1)
    in an ordered list.

    .. code-block:: c

        float my_list[] = { 1, 1.5f, 4.f, ... };

        UInt32 index = find_interval(
            sizeof(my_list) / sizeof(float),
            [](UInt32 index, dr::mask_t<UInt32> active) {
                return dr::gather<Float>(my_list, index, active) < x;
            }
        );


    Parameter ``size`` (int):
        *no description available*

    Parameter ``pred`` (collections.abc.Callable[[drjit.llvm.ad.UInt], drjit.llvm.ad.Bool]):
        *no description available*

    Returns → drjit.llvm.ad.UInt:
        *no description available*

.. py:function:: mitsuba.math.is_power_of_two(arg)

    Check whether the provided integer is a power of two

    Parameter ``arg`` (int, /):
        *no description available*

    Returns → bool:
        *no description available*

.. py:function:: mitsuba.math.legendre_p(l, x)

    Evaluate the l-th Legendre polynomial using recurrence

    Parameter ``l`` (int):
        *no description available*

    Parameter ``x`` (drjit.llvm.ad.Float):
        *no description available*

    Returns → drjit.llvm.ad.Float:
        *no description available*

.. py:function:: mitsuba.math.legendre_pd(l, x)

    Evaluate the l-th Legendre polynomial and its derivative using
    recurrence

    Parameter ``l`` (int):
        *no description available*

    Parameter ``x`` (drjit.llvm.ad.Float):
        *no description available*

    Returns → tuple[drjit.llvm.ad.Float, drjit.llvm.ad.Float]:
        *no description available*

.. py:function:: mitsuba.math.legendre_pd_diff(l, x)

    Evaluate the function legendre_pd(l+1, x) - legendre_pd(l-1, x)

    Parameter ``l`` (int):
        *no description available*

    Parameter ``x`` (drjit.llvm.ad.Float):
        *no description available*

    Returns → tuple[drjit.llvm.ad.Float, drjit.llvm.ad.Float]:
        *no description available*

.. py:function:: mitsuba.math.linear_to_srgb(arg)

    Applies the sRGB gamma curve to the given argument.

    Parameter ``arg`` (drjit.llvm.ad.Float, /):
        *no description available*

    Returns → drjit.llvm.ad.Float:
        *no description available*

.. py:function:: mitsuba.math.morton_decode2(m)

    Parameter ``m`` (drjit.llvm.ad.UInt):
        *no description available*

    Returns → drjit.llvm.ad.Array2u:
        *no description available*

.. py:function:: mitsuba.math.morton_decode3(m)

    Parameter ``m`` (drjit.llvm.ad.UInt):
        *no description available*

    Returns → drjit.llvm.ad.Array3u:
        *no description available*

.. py:function:: mitsuba.math.morton_encode2(v)

    Parameter ``v`` (drjit.llvm.ad.Array2u):
        *no description available*

    Returns → drjit.llvm.ad.UInt:
        *no description available*

.. py:function:: mitsuba.math.morton_encode3(v)

    Parameter ``v`` (drjit.llvm.ad.Array3u):
        *no description available*

    Returns → drjit.llvm.ad.UInt:
        *no description available*

.. py:function:: mitsuba.math.round_to_power_of_two(arg)

    Round an unsigned integer to the next integer power of two

    Parameter ``arg`` (int, /):
        *no description available*

    Returns → int:
        *no description available*

.. py:function:: mitsuba.math.solve_quadratic(a, b, c)

    Solve a quadratic equation of the form a*x^2 + b*x + c = 0.

    Parameter ``a`` (drjit.llvm.ad.Float):
        *no description available*

    Parameter ``b`` (drjit.llvm.ad.Float):
        *no description available*

    Parameter ``c`` (drjit.llvm.ad.Float):
        *no description available*

    Returns → tuple[drjit.llvm.ad.Bool, drjit.llvm.ad.Float, drjit.llvm.ad.Float]:
        ``True`` if a solution could be found

.. py:function:: mitsuba.math.srgb_to_linear(arg)

    Applies the inverse sRGB gamma curve to the given argument.

    Parameter ``arg`` (drjit.llvm.ad.Float, /):
        *no description available*

    Returns → drjit.llvm.ad.Float:
        *no description available*

.. py:function:: mitsuba.math.ulpdiff(arg0, arg1)

    Compare the difference in ULPs between a reference value and another
    given floating point number

    Parameter ``arg0`` (float):
        *no description available*

    Parameter ``arg1`` (float, /):
        *no description available*

    Returns → float:
        *no description available*

.. py:function:: mitsuba.math_py.rlgamma()

    Regularized lower incomplete gamma function based on CEPHES

.. py:function:: mitsuba.misc.core_count()

    Determine the number of available CPU cores (including virtual cores)

    Returns → int:
        *no description available*

.. py:function:: mitsuba.misc.mem_string(size, precise=False)

    Turn a memory size into a human-readable string

    Parameter ``size`` (int):
        *no description available*

    Parameter ``precise`` (bool):
        *no description available*

    Returns → str:
        *no description available*

.. py:function:: mitsuba.misc.time_string(time, precise=False)

    Convert a time difference (in seconds) to a string representation

    Parameter ``time`` (float):
        Time difference in (fractional) sections

    Parameter ``precise`` (bool):
        When set to true, a higher-precision string representation is
        generated.

    Returns → str:
        *no description available*

.. py:function:: mitsuba.misc.trap_debugger()

    Generate a trap instruction if running in a debugger; otherwise,
    return.

    Returns → None:
        *no description available*

.. py:function:: mitsuba.mueller.absorber(value)

    Constructs the Mueller matrix of an ideal absorber

    Parameter ``value`` (drjit.llvm.ad.Float):
        The amount of absorption.

    Returns → drjit.llvm.ad.Matrix4f:
        *no description available*

.. py:function:: mitsuba.mueller.depolarizer(value=1.0)

    Constructs the Mueller matrix of an ideal depolarizer

    Parameter ``value`` (drjit.llvm.ad.Float):
        The value of the (0, 0) element

    Returns → drjit.llvm.ad.Matrix4f:
        *no description available*

.. py:function:: mitsuba.mueller.diattenuator(x, y)

    Constructs the Mueller matrix of a linear diattenuator, which
    attenuates the electric field components at 0 and 90 degrees by 'x'
    and 'y', * respectively.

    Parameter ``x`` (drjit.llvm.ad.Float):
        *no description available*

    Parameter ``y`` (drjit.llvm.ad.Float):
        *no description available*

    Returns → drjit.llvm.ad.Matrix4f:
        *no description available*

.. py:function:: mitsuba.mueller.left_circular_polarizer()

    Constructs the Mueller matrix of a (left) circular polarizer.

    "Polarized Light and Optical Systems" by Chipman et al. Table 6.2

    Returns → drjit.llvm.ad.Matrix4f:
        *no description available*

.. py:function:: mitsuba.mueller.linear_polarizer(value=1.0)

    Constructs the Mueller matrix of a linear polarizer which transmits
    linear polarization at 0 degrees.

    "Polarized Light" by Edward Collett, Ch. 5 eq. (13)

    Parameter ``value`` (drjit.llvm.ad.Float):
        The amount of attenuation of the transmitted component (1
        corresponds to an ideal polarizer).

    Returns → drjit.llvm.ad.Matrix4f:
        *no description available*

.. py:function:: mitsuba.mueller.linear_retarder(phase)

    Constructs the Mueller matrix of a linear retarder which has its fast
    axis aligned horizontally.

    This implements the general case with arbitrary phase shift and can be
    used to construct the common special cases of quarter-wave and half-
    wave plates.

    "Polarized Light, Third Edition" by Dennis H. Goldstein, Ch. 6 eq.
    (6.43) (Note that the fast and slow axis were flipped in the first
    edition by Edward Collett.)

    Parameter ``phase`` (drjit.llvm.ad.Float):
        The phase difference between the fast and slow axis

    Returns → drjit.llvm.ad.Matrix4f:
        *no description available*

.. py:function:: mitsuba.mueller.right_circular_polarizer()

    Constructs the Mueller matrix of a (right) circular polarizer.

    "Polarized Light and Optical Systems" by Chipman et al. Table 6.2

    Returns → drjit.llvm.ad.Matrix4f:
        *no description available*

.. py:function:: mitsuba.mueller.rotate_mueller_basis(M, in_forward, in_basis_current, in_basis_target, out_forward, out_basis_current, out_basis_target)

    Return the Mueller matrix for some new reference frames. This version
    rotates the input/output frames independently.

    This operation is often used in polarized light transport when we have
    a known Mueller matrix 'M' that operates from 'in_basis_current' to
    'out_basis_current' but instead want to re-express it as a Mueller
    matrix that operates from 'in_basis_target' to 'out_basis_target'.

    Parameter ``M`` (drjit.llvm.ad.Matrix4f):
        The current Mueller matrix that operates from ``in_basis_current``
        to ``out_basis_current``.

    Parameter ``in_forward`` (:py:obj:`mitsuba.Vector3f`):
        Direction of travel for input Stokes vector (normalized)

    Parameter ``in_basis_current`` (:py:obj:`mitsuba.Vector3f`):
        Current (normalized) input Stokes basis. Must be orthogonal to
        ``in_forward``.

    Parameter ``in_basis_target`` (:py:obj:`mitsuba.Vector3f`):
        Target (normalized) input Stokes basis. Must be orthogonal to
        ``in_forward``.

    Parameter ``out_forward`` (:py:obj:`mitsuba.Vector3f`):
        Direction of travel for input Stokes vector (normalized)

    Parameter ``out_basis_current`` (:py:obj:`mitsuba.Vector3f`):
        Current (normalized) output Stokes basis. Must be orthogonal to
        ``out_forward``.

    Parameter ``out_basis_target`` (:py:obj:`mitsuba.Vector3f`):
        Target (normalized) output Stokes basis. Must be orthogonal to
        ``out_forward``.

    Returns → drjit.llvm.ad.Matrix4f:
        New Mueller matrix that operates from ``in_basis_target`` to
        ``out_basis_target``.

.. py:function:: mitsuba.mueller.rotate_mueller_basis_collinear(M, forward, basis_current, basis_target)

    Return the Mueller matrix for some new reference frames. This version
    applies the same rotation to the input/output frames.

    This operation is often used in polarized light transport when we have
    a known Mueller matrix 'M' that operates from 'basis_current' to
    'basis_current' but instead want to re-express it as a Mueller matrix
    that operates from 'basis_target' to 'basis_target'.

    Parameter ``M`` (drjit.llvm.ad.Matrix4f):
        The current Mueller matrix that operates from ``basis_current`` to
        ``basis_current``.

    Parameter ``forward`` (:py:obj:`mitsuba.Vector3f`):
        Direction of travel for input Stokes vector (normalized)

    Parameter ``basis_current`` (:py:obj:`mitsuba.Vector3f`):
        Current (normalized) input Stokes basis. Must be orthogonal to
        ``forward``.

    Parameter ``basis_target`` (:py:obj:`mitsuba.Vector3f`):
        Target (normalized) input Stokes basis. Must be orthogonal to
        ``forward``.

    Returns → drjit.llvm.ad.Matrix4f:
        New Mueller matrix that operates from ``basis_target`` to
        ``basis_target``.

.. py:function:: mitsuba.mueller.rotate_stokes_basis(wi, basis_current, basis_target)

    Gives the Mueller matrix that aligns the reference frames (defined by
    their respective basis vectors) of two collinear stokes vectors.

    If we have a stokes vector s_current expressed in 'basis_current', we
    can re-interpret it as a stokes vector rotate_stokes_basis(..) * s1
    that is expressed in 'basis_target' instead. For example: Horizontally
    polarized light [1,1,0,0] in a basis [1,0,0] can be interpreted as
    +45˚ linear polarized light [1,0,1,0] by switching to a target basis
    [0.707, -0.707, 0].

    Parameter ``forward``:
        Direction of travel for Stokes vector (normalized)

    Parameter ``basis_current`` (:py:obj:`mitsuba.Vector3f`):
        Current (normalized) Stokes basis. Must be orthogonal to
        ``forward``.

    Parameter ``basis_target`` (:py:obj:`mitsuba.Vector3f`):
        Target (normalized) Stokes basis. Must be orthogonal to
        ``forward``.

    Parameter ``wi`` (:py:obj:`mitsuba.Vector3f`):
        *no description available*

    Returns → drjit.llvm.ad.Matrix4f:
        Mueller matrix that performs the desired change of reference
        frames.

.. py:function:: mitsuba.mueller.rotate_stokes_basis_m(wi, basis_current, basis_target)

    Gives the Mueller matrix that aligns the reference frames (defined by
    their respective basis vectors) of two collinear stokes vectors.

    If we have a stokes vector s_current expressed in 'basis_current', we
    can re-interpret it as a stokes vector rotate_stokes_basis(..) * s1
    that is expressed in 'basis_target' instead. For example: Horizontally
    polarized light [1,1,0,0] in a basis [1,0,0] can be interpreted as
    +45˚ linear polarized light [1,0,1,0] by switching to a target basis
    [0.707, -0.707, 0].

    Parameter ``forward``:
        Direction of travel for Stokes vector (normalized)

    Parameter ``basis_current`` (:py:obj:`mitsuba.Vector3f`):
        Current (normalized) Stokes basis. Must be orthogonal to
        ``forward``.

    Parameter ``basis_target`` (:py:obj:`mitsuba.Vector3f`):
        Target (normalized) Stokes basis. Must be orthogonal to
        ``forward``.

    Parameter ``wi`` (:py:obj:`mitsuba.Vector3f`):
        *no description available*

    Returns → drjit::Matrix<mitsuba::Color<drjit::DiffArray<(JitBackend)2, float>, 3ul>, 4ul>:
        Mueller matrix that performs the desired change of reference
        frames.

.. py:function:: mitsuba.mueller.rotated_element(theta, M)

    Applies a counter-clockwise rotation to the mueller matrix of a given
    element.

    Parameter ``theta`` (drjit.llvm.ad.Float):
        *no description available*

    Parameter ``M`` (drjit.llvm.ad.Matrix4f):
        *no description available*

    Returns → drjit.llvm.ad.Matrix4f:
        *no description available*

.. py:function:: mitsuba.mueller.rotator(theta)

    Constructs the Mueller matrix of an ideal rotator, which performs a
    counter-clockwise rotation of the electric field by 'theta' radians
    (when facing the light beam from the sensor side).

    To be more precise, it rotates the reference frame of the current
    Stokes vector. For example: horizontally linear polarized light s1 =
    [1,1,0,0] will look like -45˚ linear polarized light s2 = R(45˚) * s1
    = [1,0,-1,0] after applying a rotator of +45˚ to it.

    "Polarized Light" by Edward Collett, Ch. 5 eq. (43)

    Parameter ``theta`` (drjit.llvm.ad.Float):
        *no description available*

    Returns → drjit.llvm.ad.Matrix4f:
        *no description available*

.. py:function:: mitsuba.mueller.specular_reflection(cos_theta_i, eta)

    Calculates the Mueller matrix of a specular reflection at an interface
    between two dielectrics or conductors.

    Parameter ``cos_theta_i`` (drjit.llvm.ad.Float):
        Cosine of the angle between the surface normal and the incident
        ray

    Parameter ``eta`` (drjit.llvm.ad.Complex2f):
        Complex-valued relative refractive index of the interface. In the
        real case, a value greater than 1.0 case means that the surface
        normal points into the region of lower density.

    Returns → drjit.llvm.ad.Matrix4f:
        *no description available*

.. py:function:: mitsuba.mueller.specular_transmission(cos_theta_i, eta)

    Calculates the Mueller matrix of a specular transmission at an
    interface between two dielectrics or conductors.

    Parameter ``cos_theta_i`` (drjit.llvm.ad.Float):
        Cosine of the angle between the surface normal and the incident
        ray

    Parameter ``eta`` (drjit.llvm.ad.Float):
        Complex-valued relative refractive index of the interface. A value
        greater than 1.0 in the real case means that the surface normal is
        pointing into the region of lower density.

    Returns → drjit.llvm.ad.Matrix4f:
        *no description available*

.. py:function:: mitsuba.mueller.stokes_basis(w)

    Gives the reference frame basis for a Stokes vector.

    For light transport involving polarized quantities it is essential to
    keep track of reference frames. A Stokes vector is only meaningful if
    we also know w.r.t. which basis this state of light is observed. In
    Mitsuba, these reference frames are never explicitly stored but
    instead can be computed on the fly using this function.

    Parameter ``forward``:
        Direction of travel for Stokes vector (normalized)

    Parameter ``w`` (:py:obj:`mitsuba.Vector3f`):
        *no description available*

    Returns → :py:obj:`mitsuba.Vector3f`:
        The (implicitly defined) reference coordinate system basis for the
        Stokes vector traveling along forward.

.. py:function:: mitsuba.mueller.unit_angle(a, b)

    Parameter ``a`` (:py:obj:`mitsuba.Vector3f`):
        *no description available*

    Parameter ``b`` (:py:obj:`mitsuba.Vector3f`):
        *no description available*

    Returns → drjit.llvm.ad.Float:
        *no description available*

.. py:function:: mitsuba.orthographic_projection(film_size, crop_size, crop_offset, near_clip, far_clip)

    Helper function to create a orthographic projection transformation
    matrix

    Parameter ``film_size`` (:py:obj:`mitsuba.ScalarVector2i`):
        *no description available*

    Parameter ``crop_size`` (:py:obj:`mitsuba.ScalarVector2i`):
        *no description available*

    Parameter ``crop_offset`` (:py:obj:`mitsuba.ScalarVector2i`):
        *no description available*

    Parameter ``near_clip`` (drjit.llvm.ad.Float):
        *no description available*

    Parameter ``far_clip`` (drjit.llvm.ad.Float):
        *no description available*

    Returns → :py:obj:`mitsuba.Transform4f`:
        *no description available*

.. py:function:: mitsuba.parse_fov(props, aspect)

    Helper function to parse the field of view field of a camera

    Parameter ``props`` (:py:obj:`mitsuba._Properties`):
        *no description available*

    Parameter ``aspect`` (float):
        *no description available*

    Returns → float:
        *no description available*

.. py:function:: mitsuba.pdf_rgb_spectrum(wavelengths)

    PDF for the sample_rgb_spectrum strategy. It is valid to call this
    function for a single wavelength (Float), a set of wavelengths
    (Spectrumf), a packet of wavelengths (SpectrumfP), etc. In all cases,
    the PDF is returned per wavelength.

    Parameter ``wavelengths`` (drjit.llvm.ad.Float):
        *no description available*

    Returns → drjit.llvm.ad.Float:
        *no description available*

.. py:function:: mitsuba.permute(value, size, seed, rounds=4)

    Generate pseudorandom permutation vector using a shuffling network

    This algorithm repeatedly invokes sample_tea_32() internally and has
    O(log2(sample_count)) complexity. It only supports permutation
    vectors, whose lengths are a power of 2.

    Parameter ``index``:
        Input index to be permuted

    Parameter ``size`` (int):
        Length of the permutation vector

    Parameter ``seed`` (drjit.llvm.ad.UInt):
        Seed value used as second input to the Tiny Encryption Algorithm.
        Can be used to generate different permutation vectors.

    Parameter ``rounds`` (int):
        How many rounds should be executed by the Tiny Encryption
        Algorithm? The default is 2.

    Parameter ``value`` (drjit.llvm.ad.UInt):
        *no description available*

    Returns → drjit.llvm.ad.UInt:
        The index corresponding to the input index in the pseudorandom
        permutation vector.

.. py:function:: mitsuba.permute_kensler(i, l, p, active=True)

    Generate pseudorandom permutation vector using the algorithm described
    in Pixar's technical memo "Correlated Multi-Jittered Sampling":

    https://graphics.pixar.com/library/MultiJitteredSampling/

    Unlike permute, this function supports permutation vectors of any
    length.

    Parameter ``index``:
        Input index to be mapped

    Parameter ``sample_count``:
        Length of the permutation vector

    Parameter ``seed``:
        Seed value used as second input to the Tiny Encryption Algorithm.
        Can be used to generate different permutation vectors.

    Parameter ``i`` (drjit.llvm.ad.UInt):
        *no description available*

    Parameter ``l`` (int):
        *no description available*

    Parameter ``p`` (drjit.llvm.ad.UInt):
        *no description available*

    Parameter ``active`` (drjit.llvm.ad.Bool):
        Mask to specify active lanes.

    Returns → drjit.llvm.ad.UInt:
        The index corresponding to the input index in the pseudorandom
        permutation vector.

.. py:function:: mitsuba.perspective_projection(film_size, crop_size, crop_offset, fov_x, near_clip, far_clip)

    Helper function to create a perspective projection transformation
    matrix

    Parameter ``film_size`` (:py:obj:`mitsuba.ScalarVector2i`):
        *no description available*

    Parameter ``crop_size`` (:py:obj:`mitsuba.ScalarVector2i`):
        *no description available*

    Parameter ``crop_offset`` (:py:obj:`mitsuba.ScalarVector2i`):
        *no description available*

    Parameter ``fov_x`` (drjit.llvm.ad.Float):
        *no description available*

    Parameter ``near_clip`` (drjit.llvm.ad.Float):
        *no description available*

    Parameter ``far_clip`` (drjit.llvm.ad.Float):
        *no description available*

    Returns → :py:obj:`mitsuba.Transform4f`:
        *no description available*

.. py:function:: mitsuba.quad.chebyshev(n)

    Computes the Chebyshev nodes, i.e. the roots of the Chebyshev
    polynomials of the first kind

    The output array contains positions on the interval :math:`[-1, 1]`.

    Parameter ``n`` (int):
        Desired number of points

    Returns → drjit.llvm.ad.Float:
        *no description available*

.. py:function:: mitsuba.quad.composite_simpson(n)

    Computes the nodes and weights of a composite Simpson quadrature rule
    with the given number of evaluations.

    Integration is over the interval :math:`[-1, 1]`, which will be split
    into :math:`(n-1) / 2` sub-intervals with overlapping endpoints. A
    3-point Simpson rule is applied per interval, which is exact for
    polynomials of degree three or less.

    Parameter ``n`` (int):
        Desired number of evaluation points. Must be an odd number bigger
        than 3.

    Returns → tuple[drjit.llvm.ad.Float, drjit.llvm.ad.Float]:
        A tuple (nodes, weights) storing the nodes and weights of the
        quadrature rule.

.. py:function:: mitsuba.quad.composite_simpson_38(n)

    Computes the nodes and weights of a composite Simpson 3/8 quadrature
    rule with the given number of evaluations.

    Integration is over the interval :math:`[-1, 1]`, which will be split
    into :math:`(n-1) / 3` sub-intervals with overlapping endpoints. A
    4-point Simpson rule is applied per interval, which is exact for
    polynomials of degree four or less.

    Parameter ``n`` (int):
        Desired number of evaluation points. Must be an odd number bigger
        than 3.

    Returns → tuple[drjit.llvm.ad.Float, drjit.llvm.ad.Float]:
        A tuple (nodes, weights) storing the nodes and weights of the
        quadrature rule.

.. py:function:: mitsuba.quad.gauss_legendre(n)

    Computes the nodes and weights of a Gauss-Legendre quadrature (aka
    "Gaussian quadrature") rule with the given number of evaluations.

    Integration is over the interval :math:`[-1, 1]`. Gauss-Legendre
    quadrature maximizes the order of exactly integrable polynomials
    achieves this up to degree :math:`2n-1` (where :math:`n` is the number
    of function evaluations).

    This method is numerically well-behaved until about :math:`n=200` and
    then becomes progressively less accurate. It is generally not a good
    idea to go much higher---in any case, a composite or adaptive
    integration scheme will be superior for large :math:`n`.

    Parameter ``n`` (int):
        Desired number of evaluation points

    Returns → tuple[drjit.llvm.ad.Float, drjit.llvm.ad.Float]:
        A tuple (nodes, weights) storing the nodes and weights of the
        quadrature rule.

.. py:function:: mitsuba.quad.gauss_lobatto(n)

    Computes the nodes and weights of a Gauss-Lobatto quadrature rule with
    the given number of evaluations.

    Integration is over the interval :math:`[-1, 1]`. Gauss-Lobatto
    quadrature is preferable to Gauss-Legendre quadrature whenever the
    endpoints of the integration domain should explicitly be included. It
    maximizes the order of exactly integrable polynomials subject to this
    constraint and achieves this up to degree :math:`2n-3` (where
    :math:`n` is the number of function evaluations).

    This method is numerically well-behaved until about :math:`n=200` and
    then becomes progressively less accurate. It is generally not a good
    idea to go much higher---in any case, a composite or adaptive
    integration scheme will be superior for large :math:`n`.

    Parameter ``n`` (int):
        Desired number of evaluation points

    Returns → tuple[drjit.llvm.ad.Float, drjit.llvm.ad.Float]:
        A tuple (nodes, weights) storing the nodes and weights of the
        quadrature rule.

.. py:function:: mitsuba.radical_inverse_2(index, scramble)

    Van der Corput radical inverse in base 2

    Parameter ``index`` (drjit.llvm.ad.UInt):
        *no description available*

    Parameter ``scramble`` (drjit.llvm.ad.UInt):
        *no description available*

    Returns → drjit.llvm.ad.Float:
        *no description available*

.. py:function:: mitsuba.reflect(wi)

    Overloaded function.

    1. ``reflect(wi: :py:obj:`mitsuba.Vector3f`) -> :py:obj:`mitsuba.Vector3f```

    Reflection in local coordinates

    2. ``reflect(wi: :py:obj:`mitsuba.Vector3f`, m: :py:obj:`mitsuba.Normal3f`) -> :py:obj:`mitsuba.Vector3f```

    Reflect ``wi`` with respect to a given surface normal

    Parameter ``wi`` (:py:obj:`mitsuba.Vector3f`):
        *no description available*

    Returns → :py:obj:`mitsuba.Vector3f`:
        *no description available*

.. py:function:: mitsuba.refract(wi, cos_theta_t, eta_ti)

    Overloaded function.

    1. ``refract(wi: :py:obj:`mitsuba.Vector3f`, cos_theta_t: drjit.llvm.ad.Float, eta_ti: drjit.llvm.ad.Float) -> :py:obj:`mitsuba.Vector3f```

    Refraction in local coordinates

    The 'cos_theta_t' and 'eta_ti' parameters are given by the last two
    tuple entries returned by the fresnel and fresnel_polarized functions.

    2. ``refract(wi: :py:obj:`mitsuba.Vector3f`, m: :py:obj:`mitsuba.Normal3f`, cos_theta_t: drjit.llvm.ad.Float, eta_ti: drjit.llvm.ad.Float) -> :py:obj:`mitsuba.Vector3f```

    Refract ``wi`` with respect to a given surface normal

    Parameter ``wi`` (:py:obj:`mitsuba.Vector3f`):
        Direction to refract

    Parameter ``m``:
        Surface normal

    Parameter ``cos_theta_t`` (drjit.llvm.ad.Float):
        Cosine of the angle between the normal the transmitted ray, as
        computed e.g. by fresnel.

    Parameter ``eta_ti`` (drjit.llvm.ad.Float):
        Relative index of refraction (transmitted / incident)

    Returns → :py:obj:`mitsuba.Vector3f`:
        *no description available*

.. py:function:: mitsuba.register_bsdf(arg0, arg1)

    Parameter ``arg0`` (str):
        *no description available*

    Parameter ``arg1`` (collections.abc.Callable[[:py:obj:`mitsuba.Properties`], object], /):
        *no description available*

    Returns → None:
        *no description available*

.. py:function:: mitsuba.register_emitter(arg0, arg1)

    Parameter ``arg0`` (str):
        *no description available*

    Parameter ``arg1`` (collections.abc.Callable[[:py:obj:`mitsuba.Properties`], object], /):
        *no description available*

    Returns → None:
        *no description available*

.. py:function:: mitsuba.register_film(arg0, arg1)

    Parameter ``arg0`` (str):
        *no description available*

    Parameter ``arg1`` (collections.abc.Callable[[:py:obj:`mitsuba.Properties`], object], /):
        *no description available*

    Returns → None:
        *no description available*

.. py:function:: mitsuba.register_integrator(arg0, arg1)

    Parameter ``arg0`` (str):
        *no description available*

    Parameter ``arg1`` (collections.abc.Callable[[:py:obj:`mitsuba.Properties`], object], /):
        *no description available*

    Returns → None:
        *no description available*

.. py:function:: mitsuba.register_medium(arg0, arg1)

    Parameter ``arg0`` (str):
        *no description available*

    Parameter ``arg1`` (collections.abc.Callable[[:py:obj:`mitsuba.Properties`], object], /):
        *no description available*

    Returns → None:
        *no description available*

.. py:function:: mitsuba.register_mesh(arg0, arg1)

    Parameter ``arg0`` (str):
        *no description available*

    Parameter ``arg1`` (collections.abc.Callable[[:py:obj:`mitsuba.Properties`], object], /):
        *no description available*

    Returns → None:
        *no description available*

.. py:function:: mitsuba.register_phasefunction(arg0, arg1)

    Parameter ``arg0`` (str):
        *no description available*

    Parameter ``arg1`` (collections.abc.Callable[[:py:obj:`mitsuba.Properties`], object], /):
        *no description available*

    Returns → None:
        *no description available*

.. py:function:: mitsuba.register_sampler(arg0, arg1)

    Parameter ``arg0`` (str):
        *no description available*

    Parameter ``arg1`` (collections.abc.Callable[[:py:obj:`mitsuba.Properties`], object], /):
        *no description available*

    Returns → None:
        *no description available*

.. py:function:: mitsuba.register_sensor(arg0, arg1)

    Parameter ``arg0`` (str):
        *no description available*

    Parameter ``arg1`` (collections.abc.Callable[[:py:obj:`mitsuba.Properties`], object], /):
        *no description available*

    Returns → None:
        *no description available*

.. py:function:: mitsuba.register_texture(arg0, arg1)

    Parameter ``arg0`` (str):
        *no description available*

    Parameter ``arg1`` (collections.abc.Callable[[:py:obj:`mitsuba.Properties`], object], /):
        *no description available*

    Returns → None:
        *no description available*

.. py:function:: mitsuba.register_volume(arg0, arg1)

    Parameter ``arg0`` (str):
        *no description available*

    Parameter ``arg1`` (collections.abc.Callable[[:py:obj:`mitsuba.Properties`], object], /):
        *no description available*

    Returns → None:
        *no description available*

.. py:function:: mitsuba.render(scene, params=None, sensor=0, integrator=None, seed=0, seed_grad=0, spp=0, spp_grad=0)

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

    Parameter ``params`` (Any):
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

    Parameter ``seed`` (``mi.UInt32``)
        This parameter controls the initialization of the random number
        generator during the primal rendering step. It is crucial that you
        specify different seeds (e.g., an increasing sequence) if subsequent
        calls should produce statistically independent images (e.g. to
        de-correlate gradient-based optimization steps).

    Parameter ``seed_grad`` (``mi.UInt32``)
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

    Parameter ``scene`` (mi.Scene):
        *no description available*

    Parameter ``sensor`` (Union[int, mi.Sensor]):
        *no description available*

    Parameter ``integrator`` (mi.Integrator):
        *no description available*

    Parameter ``seed`` (mi.UInt32):
        *no description available*

    Parameter ``seed_grad`` (int):
        *no description available*

    Parameter ``spp`` (int):
        *no description available*

    Parameter ``spp_grad`` (int):
        *no description available*

    Returns → mi.TensorXf:
        *no description available*

.. py:function:: mitsuba.sample_rgb_spectrum(sample)

    Importance sample a "importance spectrum" that concentrates the
    computation on wavelengths that are relevant for rendering of RGB data

    Based on "An Improved Technique for Full Spectral Rendering" by
    Radziszewski, Boryczko, and Alda

    Returns a tuple with the sampled wavelength and inverse PDF

    Parameter ``sample`` (drjit.llvm.ad.Float):
        *no description available*

    Returns → tuple[drjit.llvm.ad.Float, drjit.llvm.ad.Float]:
        *no description available*

.. py:function:: mitsuba.sample_tea_32(v0, v1, rounds=4)

    Generate fast and reasonably good pseudorandom numbers using the Tiny
    Encryption Algorithm (TEA) by David Wheeler and Roger Needham.

    For details, refer to "GPU Random Numbers via the Tiny Encryption
    Algorithm" by Fahad Zafar, Marc Olano, and Aaron Curtis.

    Parameter ``v0`` (int):
        First input value to be encrypted (could be the sample index)

    Parameter ``v1`` (int):
        Second input value to be encrypted (e.g. the requested random
        number dimension)

    Parameter ``rounds`` (int):
        How many rounds should be executed? The default for random number
        generation is 4.

    Returns → tuple[int, int]:
        Two uniformly distributed 32-bit integers

.. py:function:: mitsuba.sample_tea_64(v0, v1, rounds=4)

    Generate fast and reasonably good pseudorandom numbers using the Tiny
    Encryption Algorithm (TEA) by David Wheeler and Roger Needham.

    For details, refer to "GPU Random Numbers via the Tiny Encryption
    Algorithm" by Fahad Zafar, Marc Olano, and Aaron Curtis.

    Parameter ``v0`` (int):
        First input value to be encrypted (could be the sample index)

    Parameter ``v1`` (int):
        Second input value to be encrypted (e.g. the requested random
        number dimension)

    Parameter ``rounds`` (int):
        How many rounds should be executed? The default for random number
        generation is 4.

    Returns → int:
        A uniformly distributed 64-bit integer

.. py:function:: mitsuba.sample_tea_float

    sample_tea_float64(v0: int, v1: int, rounds: int = 4) -> float
    sample_tea_float64(v0: drjit.llvm.ad.UInt, v1: drjit.llvm.ad.UInt, rounds: int = 4) -> drjit.llvm.ad.Float64

    Generate fast and reasonably good pseudorandom numbers using the Tiny
    Encryption Algorithm (TEA) by David Wheeler and Roger Needham.

    This function uses sample_tea to return double precision floating
    point numbers on the interval ``[0, 1)``

    Parameter ``v0``:
        First input value to be encrypted (could be the sample index)

    Parameter ``v1``:
        Second input value to be encrypted (e.g. the requested random
        number dimension)

    Parameter ``rounds``:
        How many rounds should be executed? The default for random number
        generation is 4.

    Returns:
        A uniformly distributed floating point number on the interval
        ``[0, 1)``

.. py:function:: mitsuba.sample_tea_float32(v0, v1, rounds=4)

    Generate fast and reasonably good pseudorandom numbers using the Tiny
    Encryption Algorithm (TEA) by David Wheeler and Roger Needham.

    This function uses sample_tea to return single precision floating
    point numbers on the interval ``[0, 1)``

    Parameter ``v0`` (int):
        First input value to be encrypted (could be the sample index)

    Parameter ``v1`` (int):
        Second input value to be encrypted (e.g. the requested random
        number dimension)

    Parameter ``rounds`` (int):
        How many rounds should be executed? The default for random number
        generation is 4.

    Returns → float:
        A uniformly distributed floating point number on the interval
        ``[0, 1)``

.. py:function:: mitsuba.sample_tea_float64(v0, v1, rounds=4)

    Generate fast and reasonably good pseudorandom numbers using the Tiny
    Encryption Algorithm (TEA) by David Wheeler and Roger Needham.

    This function uses sample_tea to return double precision floating
    point numbers on the interval ``[0, 1)``

    Parameter ``v0`` (int):
        First input value to be encrypted (could be the sample index)

    Parameter ``v1`` (int):
        Second input value to be encrypted (e.g. the requested random
        number dimension)

    Parameter ``rounds`` (int):
        How many rounds should be executed? The default for random number
        generation is 4.

    Returns → float:
        A uniformly distributed floating point number on the interval
        ``[0, 1)``

.. py:function:: mitsuba.scoped_set_variant()

    Temporarily override the active variant. Arguments are interpreted as
    they are in :func:`mitsuba.set_variant`.

    Returns → None:
        *no description available*

.. py:function:: mitsuba.set_log_level(arg)

    Sets the log level.

    Parameter ``arg`` (:py:obj:`mitsuba.LogLevel`, /):
        *no description available*

    Returns → None:
        *no description available*

.. py:function:: mitsuba.set_variant()

    Returns → None:
        *no description available*

.. py:function:: mitsuba.sggx_pdf(wm, s)

    Evaluates the probability of sampling a given normal using the SGGX
    microflake distribution

    Parameter ``wm`` (:py:obj:`mitsuba.Vector3f`):
        The microflake normal

    Parameter ``s`` (:py:obj:`mitsuba.SGGXPhaseFunctionParams`):
        The parameters of the SGGX phase function S_xx, S_yy, S_zz, S_xy,
        S_xz, and S_yz that describe the entries of a symmetric positive
        definite 3x3 matrix. The user needs to ensure that the parameters
        indeed represent a positive definite matrix.

    Returns → drjit.llvm.ad.Float:
        The probability of sampling a certain normal

.. py:function:: mitsuba.sggx_projected_area(wi, s)

    Evaluates the projected area of the SGGX microflake distribution

    Parameter ``wi`` (:py:obj:`mitsuba.Vector3f`):
        A 3D direction

    Parameter ``s`` (:py:obj:`mitsuba.SGGXPhaseFunctionParams`):
        The parameters of the SGGX phase function S_xx, S_yy, S_zz, S_xy,
        S_xz, and S_yz that describe the entries of a symmetric positive
        definite 3x3 matrix. The user needs to ensure that the parameters
        indeed represent a positive definite matrix.

    Returns → drjit.llvm.ad.Float:
        The projected area of the SGGX microflake distribution

.. py:function:: mitsuba.sggx_sample(sh_frame, sample, s)

    Samples the visible normal distribution of the SGGX microflake
    distribution

    This function is based on the paper

    "The SGGX microflake distribution", Siggraph 2015 by Eric Heitz,
    Jonathan Dupuy, Cyril Crassin and Carsten Dachsbacher

    Parameter ``sh_frame`` (:py:obj:`mitsuba.Frame3f`):
        Shading frame aligned with the incident direction, e.g.
        constructed as Frame3f(wi)

    Parameter ``sample`` (:py:obj:`mitsuba.Point2f`):
        A uniformly distributed 2D sample

    Parameter ``s`` (:py:obj:`mitsuba.SGGXPhaseFunctionParams`):
        The parameters of the SGGX phase function S_xx, S_yy, S_zz, S_xy,
        S_xz, and S_yz that describe the entries of a symmetric positive
        definite 3x3 matrix. The user needs to ensure that the parameters
        indeed represent a positive definite matrix.

    Returns → :py:obj:`mitsuba.Normal3f`:
        A normal (in world space) sampled from the distribution of visible
        normals

.. py:function:: mitsuba.sobol_2(index, scramble)

    Sobol' radical inverse in base 2

    Parameter ``index`` (drjit.llvm.ad.UInt):
        *no description available*

    Parameter ``scramble`` (drjit.llvm.ad.UInt):
        *no description available*

    Returns → drjit.llvm.ad.Float:
        *no description available*

.. py:function:: mitsuba.spectrum_from_file(filename)

    Read a spectral power distribution from an ASCII file.

    The data should be arranged as follows: The file should contain a
    single measurement per line, with the corresponding wavelength in
    nanometers and the measured value separated by a space. Comments are
    allowed.

    Parameter ``path``:
        Path of the file to be read

    Parameter ``wavelengths``:
        Array that will be loaded with the wavelengths stored in the file

    Parameter ``values``:
        Array that will be loaded with the values stored in the file

    Parameter ``filename`` (:py:obj:`mitsuba.filesystem.path`):
        *no description available*

    Returns → tuple[list[float], list[float]]:
        *no description available*

.. py:function:: mitsuba.spectrum_list_to_srgb(wavelengths, values, bounded=True, d65=False)

    Parameter ``wavelengths`` (collections.abc.Sequence[float]):
        *no description available*

    Parameter ``values`` (collections.abc.Sequence[float]):
        *no description available*

    Parameter ``bounded`` (bool):
        *no description available*

    Parameter ``d65`` (bool):
        *no description available*

    Returns → :py:obj:`mitsuba.ScalarColor3f`:
        *no description available*

.. py:function:: mitsuba.spectrum_to_file(filename, wavelengths, values)

    Write a spectral power distribution to an ASCII file.

    The format is identical to that parsed by spectrum_from_file().

    Parameter ``path``:
        Path to the file to be written to

    Parameter ``wavelengths`` (collections.abc.Sequence[float]):
        Array with the wavelengths to be stored in the file

    Parameter ``values`` (collections.abc.Sequence[float]):
        Array with the values to be stored in the file

    Parameter ``filename`` (:py:obj:`mitsuba.filesystem.path`):
        *no description available*

    Returns → None:
        *no description available*

.. py:function:: mitsuba.spline.eval_1d(min, max, values, x)

    Overloaded function.

    1. ``eval_1d(min: float, max: float, values: drjit.llvm.ad.Float, x: drjit.llvm.ad.Float) -> drjit.llvm.ad.Float``

    Evaluate a cubic spline interpolant of a *uniformly* sampled 1D
    function

    The implementation relies on Catmull-Rom splines, i.e. it uses finite
    differences to approximate the derivatives at the endpoints of each
    spline segment.

    Template parameter ``Extrapolate``:
        Extrapolate values when ``x`` is out of range? (default:
        ``False``)

    Parameter ``min`` (float):
        Position of the first node

    Parameter ``max`` (float):
        Position of the last node

    Parameter ``values`` (drjit.llvm.ad.Float):
        Array containing ``size`` regularly spaced evaluations in the
        range [``min``, ``max``] of the approximated function.

    Parameter ``size``:
        Denotes the size of the ``values`` array

    Parameter ``x`` (drjit.llvm.ad.Float):
        Evaluation point

    Remark:
        The Python API lacks the ``size`` parameter, which is inferred
        automatically from the size of the input array.

    Remark:
        The Python API provides a vectorized version which evaluates the
        function for many arguments ``x``.

    Returns → drjit.llvm.ad.Float:
        The interpolated value or zero when ``Extrapolate=false`` and
        ``x`` lies outside of [``min``, ``max``]

    2. ``eval_1d(nodes: drjit.llvm.ad.Float, values: drjit.llvm.ad.Float, x: drjit.llvm.ad.Float) -> drjit.llvm.ad.Float``

    Evaluate a cubic spline interpolant of a *non*-uniformly sampled 1D
    function

    The implementation relies on Catmull-Rom splines, i.e. it uses finite
    differences to approximate the derivatives at the endpoints of each
    spline segment.

    Template parameter ``Extrapolate``:
        Extrapolate values when ``x`` is out of range? (default:
        ``False``)

    Parameter ``nodes``:
        Array containing ``size`` non-uniformly spaced values denoting
        positions the where the function to be interpolated was evaluated.
        They must be provided in *increasing* order.

    Parameter ``values`` (drjit.llvm.ad.Float):
        Array containing function evaluations matched to the entries of
        ``nodes``.

    Parameter ``size``:
        Denotes the size of the ``nodes`` and ``values`` array

    Parameter ``x`` (drjit.llvm.ad.Float):
        Evaluation point

    Remark:
        The Python API lacks the ``size`` parameter, which is inferred
        automatically from the size of the input array

    Remark:
        The Python API provides a vectorized version which evaluates the
        function for many arguments ``x``.

    Returns → drjit.llvm.ad.Float:
        The interpolated value or zero when ``Extrapolate=false`` and
        ``x`` lies outside of \a [``min``, ``max``]

.. py:function:: mitsuba.spline.eval_2d(nodes1, nodes2, values, x, y)

    Evaluate a cubic spline interpolant of a uniformly sampled 2D function

    This implementation relies on a tensor product of Catmull-Rom splines,
    i.e. it uses finite differences to approximate the derivatives for
    each dimension at the endpoints of spline patches.

    Template parameter ``Extrapolate``:
        Extrapolate values when ``p`` is out of range? (default:
        ``False``)

    Parameter ``nodes1`` (drjit.llvm.ad.Float):
        Arrays containing ``size1`` non-uniformly spaced values denoting
        positions the where the function to be interpolated was evaluated
        on the ``X`` axis (in increasing order)

    Parameter ``size1``:
        Denotes the size of the ``nodes1`` array

    Parameter ``nodes``:
        Arrays containing ``size2`` non-uniformly spaced values denoting
        positions the where the function to be interpolated was evaluated
        on the ``Y`` axis (in increasing order)

    Parameter ``size2``:
        Denotes the size of the ``nodes2`` array

    Parameter ``values`` (drjit.llvm.ad.Float):
        A 2D floating point array of ``size1*size2`` cells containing
        irregularly spaced evaluations of the function to be interpolated.
        Consecutive entries of this array correspond to increments in the
        ``X`` coordinate.

    Parameter ``x`` (drjit.llvm.ad.Float):
        ``X`` coordinate of the evaluation point

    Parameter ``y`` (drjit.llvm.ad.Float):
        ``Y`` coordinate of the evaluation point

    Remark:
        The Python API lacks the ``size1`` and ``size2`` parameters, which
        are inferred automatically from the size of the input arrays.

    Parameter ``nodes2`` (drjit.llvm.ad.Float):
        *no description available*

    Returns → drjit.llvm.ad.Float:
        The interpolated value or zero when ``Extrapolate=false``tt> and
        ``(x,y)`` lies outside of the node range

.. py:function:: mitsuba.spline.eval_spline(f0, f1, d0, d1, t)

    Compute the definite integral and derivative of a cubic spline that is
    parameterized by the function values and derivatives at the endpoints
    of the interval ``[0, 1]``.

    Parameter ``f0`` (float):
        The function value at the left position

    Parameter ``f1`` (float):
        The function value at the right position

    Parameter ``d0`` (float):
        The function derivative at the left position

    Parameter ``d1`` (float):
        The function derivative at the right position

    Parameter ``t`` (float):
        The parameter variable

    Returns → float:
        The interpolated function value at ``t``

.. py:function:: mitsuba.spline.eval_spline_d(f0, f1, d0, d1, t)

    Compute the value and derivative of a cubic spline that is
    parameterized by the function values and derivatives of the interval
    ``[0, 1]``.

    Parameter ``f0`` (float):
        The function value at the left position

    Parameter ``f1`` (float):
        The function value at the right position

    Parameter ``d0`` (float):
        The function derivative at the left position

    Parameter ``d1`` (float):
        The function derivative at the right position

    Parameter ``t`` (float):
        The parameter variable

    Returns → tuple[float, float]:
        The interpolated function value and its derivative at ``t``

.. py:function:: mitsuba.spline.eval_spline_i(f0, f1, d0, d1, t)

    Compute the definite integral and value of a cubic spline that is
    parameterized by the function values and derivatives of the interval
    ``[0, 1]``.

    Parameter ``f0`` (float):
        The function value at the left position

    Parameter ``f1`` (float):
        The function value at the right position

    Parameter ``d0`` (float):
        The function derivative at the left position

    Parameter ``d1`` (float):
        The function derivative at the right position

    Parameter ``t`` (float):
        *no description available*

    Returns → tuple[float, float]:
        The definite integral and the interpolated function value at ``t``

.. py:function:: mitsuba.spline.eval_spline_weights(min, max, size, x)

    Overloaded function.

    1. ``eval_spline_weights(min: float, max: float, size: int, x: drjit.llvm.ad.Float) -> tuple[drjit.llvm.ad.Bool, drjit.llvm.ad.Int, list[drjit.llvm.ad.Float]]``

    Compute weights to perform a spline-interpolated lookup on a
    *uniformly* sampled 1D function.

    The implementation relies on Catmull-Rom splines, i.e. it uses finite
    differences to approximate the derivatives at the endpoints of each
    spline segment. The resulting weights are identical those internally
    used by sample_1d().

    Template parameter ``Extrapolate``:
        Extrapolate values when ``x`` is out of range? (default:
        ``False``)

    Parameter ``min`` (float):
        Position of the first node

    Parameter ``max`` (float):
        Position of the last node

    Parameter ``size`` (int):
        Denotes the number of function samples

    Parameter ``x`` (drjit.llvm.ad.Float):
        Evaluation point

    Parameter ``weights``:
        Pointer to a weight array of size 4 that will be populated

    Remark:
        In the Python API, the ``offset`` and ``weights`` parameters are
        returned as the second and third elements of a triple.

    Returns → tuple[drjit.llvm.ad.Bool, drjit.llvm.ad.Int, list[drjit.llvm.ad.Float]]:
        A boolean set to ``True`` on success and ``False`` when
        ``Extrapolate=false`` and ``x`` lies outside of [``min``, ``max``]
        and an offset into the function samples associated with weights[0]

    2. ``eval_spline_weights(nodes: drjit.llvm.ad.Float, x: drjit.llvm.ad.Float) -> tuple[drjit.llvm.ad.Bool, drjit.llvm.ad.Int, list[drjit.llvm.ad.Float]]``

    Compute weights to perform a spline-interpolated lookup on a
    *non*-uniformly sampled 1D function.

    The implementation relies on Catmull-Rom splines, i.e. it uses finite
    differences to approximate the derivatives at the endpoints of each
    spline segment. The resulting weights are identical those internally
    used by sample_1d().

    Template parameter ``Extrapolate``:
        Extrapolate values when ``x`` is out of range? (default:
        ``False``)

    Parameter ``nodes``:
        Array containing ``size`` non-uniformly spaced values denoting
        positions the where the function to be interpolated was evaluated.
        They must be provided in *increasing* order.

    Parameter ``size`` (int):
        Denotes the size of the ``nodes`` array

    Parameter ``x`` (drjit.llvm.ad.Float):
        Evaluation point

    Parameter ``weights``:
        Pointer to a weight array of size 4 that will be populated

    Remark:
        The Python API lacks the ``size`` parameter, which is inferred
        automatically from the size of the input array. The ``offset`` and
        ``weights`` parameters are returned as the second and third
        elements of a triple.

    Returns → tuple[drjit.llvm.ad.Bool, drjit.llvm.ad.Int, list[drjit.llvm.ad.Float]]:
        A boolean set to ``True`` on success and ``False`` when
        ``Extrapolate=false`` and ``x`` lies outside of [``min``, ``max``]
        and an offset into the function samples associated with weights[0]

.. py:function:: mitsuba.spline.integrate_1d(min, max, values)

    Overloaded function.

    1. ``integrate_1d(min: float, max: float, values: drjit.llvm.ad.Float) -> drjit.scalar.ArrayXf``

    Computes a prefix sum of integrals over segments of a *uniformly*
    sampled 1D Catmull-Rom spline interpolant

    This is useful for sampling spline segments as part of an importance
    sampling scheme (in conjunction with sample_1d)

    Parameter ``min`` (float):
        Position of the first node

    Parameter ``max`` (float):
        Position of the last node

    Parameter ``values`` (drjit.llvm.ad.Float):
        Array containing ``size`` regularly spaced evaluations in the
        range [``min``, ``max``] of the approximated function.

    Parameter ``size``:
        Denotes the size of the ``values`` array

    Parameter ``out``:
        An array with ``size`` entries, which will be used to store the
        prefix sum

    Remark:
        The Python API lacks the ``size`` and ``out`` parameters. The
        former is inferred automatically from the size of the input array,
        and ``out`` is returned as a list.

    2. ``integrate_1d(nodes: drjit.llvm.ad.Float, values: drjit.llvm.ad.Float) -> drjit.scalar.ArrayXf``

    Computes a prefix sum of integrals over segments of a *non*-uniformly
    sampled 1D Catmull-Rom spline interpolant

    This is useful for sampling spline segments as part of an importance
    sampling scheme (in conjunction with sample_1d)

    Parameter ``nodes``:
        Array containing ``size`` non-uniformly spaced values denoting
        positions the where the function to be interpolated was evaluated.
        They must be provided in *increasing* order.

    Parameter ``values`` (drjit.llvm.ad.Float):
        Array containing function evaluations matched to the entries of
        ``nodes``.

    Parameter ``size``:
        Denotes the size of the ``values`` array

    Parameter ``out``:
        An array with ``size`` entries, which will be used to store the
        prefix sum

    Remark:
        The Python API lacks the ``size`` and ``out`` parameters. The
        former is inferred automatically from the size of the input array,
        and ``out`` is returned as a list.

    Returns → drjit.scalar.ArrayXf:
        *no description available*

.. py:function:: mitsuba.spline.invert_1d(min, max_, values, y, eps=9.999999974752427e-07)

    Overloaded function.

    1. ``invert_1d(min: float, max_: float, values: drjit.llvm.ad.Float, y: drjit.llvm.ad.Float, eps: float = 9.999999974752427e-07) -> drjit.llvm.ad.Float``

    Invert a cubic spline interpolant of a *uniformly* sampled 1D
    function. The spline interpolant must be *monotonically increasing*.

    Parameter ``min`` (float):
        Position of the first node

    Parameter ``max``:
        Position of the last node

    Parameter ``values`` (drjit.llvm.ad.Float):
        Array containing ``size`` regularly spaced evaluations in the
        range [``min``, ``max``] of the approximated function.

    Parameter ``size``:
        Denotes the size of the ``values`` array

    Parameter ``y`` (drjit.llvm.ad.Float):
        Input parameter for the inversion

    Parameter ``eps`` (float):
        Error tolerance (default: 1e-6f)

    Parameter ``max_`` (float):
        *no description available*

    Returns → drjit.llvm.ad.Float:
        The spline parameter ``t`` such that ``eval_1d(..., t)=y``

    2. ``invert_1d(nodes: drjit.llvm.ad.Float, values: drjit.llvm.ad.Float, y: drjit.llvm.ad.Float, eps: float = 9.999999974752427e-07) -> drjit.llvm.ad.Float``

    Invert a cubic spline interpolant of a *non*-uniformly sampled 1D
    function. The spline interpolant must be *monotonically increasing*.

    Parameter ``nodes``:
        Array containing ``size`` non-uniformly spaced values denoting
        positions the where the function to be interpolated was evaluated.
        They must be provided in *increasing* order.

    Parameter ``values`` (drjit.llvm.ad.Float):
        Array containing function evaluations matched to the entries of
        ``nodes``.

    Parameter ``size``:
        Denotes the size of the ``values`` array

    Parameter ``y`` (drjit.llvm.ad.Float):
        Input parameter for the inversion

    Parameter ``eps`` (float):
        Error tolerance (default: 1e-6f)

    Returns → drjit.llvm.ad.Float:
        The spline parameter ``t`` such that ``eval_1d(..., t)=y``

.. py:function:: mitsuba.spline.sample_1d(min, max, values, cdf, sample, eps=9.999999974752427e-07)

    Overloaded function.

    1. ``sample_1d(min: float, max: float, values: drjit.llvm.ad.Float, cdf: drjit.llvm.ad.Float, sample: drjit.llvm.ad.Float, eps: float = 9.999999974752427e-07) -> tuple[drjit.llvm.ad.Float, drjit.llvm.ad.Float, drjit.llvm.ad.Float]``

    Importance sample a segment of a *uniformly* sampled 1D Catmull-Rom
    spline interpolant

    Parameter ``min`` (float):
        Position of the first node

    Parameter ``max`` (float):
        Position of the last node

    Parameter ``values`` (drjit.llvm.ad.Float):
        Array containing ``size`` regularly spaced evaluations in the
        range [``min``, ``max``] of the approximated function.

    Parameter ``cdf`` (drjit.llvm.ad.Float):
        Array containing a cumulative distribution function computed by
        integrate_1d().

    Parameter ``size``:
        Denotes the size of the ``values`` array

    Parameter ``sample`` (drjit.llvm.ad.Float):
        A uniformly distributed random sample in the interval ``[0,1]``

    Parameter ``eps`` (float):
        Error tolerance (default: 1e-6f)

    Returns → tuple[drjit.llvm.ad.Float, drjit.llvm.ad.Float, drjit.llvm.ad.Float]:
        1. The sampled position 2. The value of the spline evaluated at
        the sampled position 3. The probability density at the sampled
        position (which only differs from item 2. when the function does
        not integrate to one)

    2. ``sample_1d(nodes: drjit.llvm.ad.Float, values: drjit.llvm.ad.Float, cdf: drjit.llvm.ad.Float, sample: drjit.llvm.ad.Float, eps: float = 9.999999974752427e-07) -> tuple[drjit.llvm.ad.Float, drjit.llvm.ad.Float, drjit.llvm.ad.Float]``

    Importance sample a segment of a *non*-uniformly sampled 1D Catmull-
    Rom spline interpolant

    Parameter ``nodes``:
        Array containing ``size`` non-uniformly spaced values denoting
        positions the where the function to be interpolated was evaluated.
        They must be provided in *increasing* order.

    Parameter ``values`` (drjit.llvm.ad.Float):
        Array containing function evaluations matched to the entries of
        ``nodes``.

    Parameter ``cdf`` (drjit.llvm.ad.Float):
        Array containing a cumulative distribution function computed by
        integrate_1d().

    Parameter ``size``:
        Denotes the size of the ``values`` array

    Parameter ``sample`` (drjit.llvm.ad.Float):
        A uniformly distributed random sample in the interval ``[0,1]``

    Parameter ``eps`` (float):
        Error tolerance (default: 1e-6f)

    Returns → tuple[drjit.llvm.ad.Float, drjit.llvm.ad.Float, drjit.llvm.ad.Float]:
        1. The sampled position 2. The value of the spline evaluated at
        the sampled position 3. The probability density at the sampled
        position (which only differs from item 2. when the function does
        not integrate to one)

.. py:function:: mitsuba.srgb_model_eval(arg0, arg1)

    Parameter ``arg0`` (drjit.llvm.ad.Array3f):
        *no description available*

    Parameter ``arg1`` (:py:obj:`mitsuba.Color0f`, /):
        *no description available*

    Returns → :py:obj:`mitsuba.Color3f`:
        *no description available*

.. py:function:: mitsuba.srgb_model_fetch(arg)

    Look up the model coefficients for a sRGB color value

    Parameter ``c``:
        An sRGB color value where all components are in [0, 1].

    Parameter ``arg`` (:py:obj:`mitsuba.ScalarColor3f`, /):
        *no description available*

    Returns → drjit.scalar.Array3f:
        Coefficients for use with srgb_model_eval

.. py:function:: mitsuba.srgb_model_mean(arg)

    Parameter ``arg`` (drjit.llvm.ad.Array3f, /):
        *no description available*

    Returns → drjit.llvm.ad.Float:
        *no description available*

.. py:function:: mitsuba.srgb_to_xyz(rgb, active=True)

    Convert ITU-R Rec. BT.709 linear RGB to XYZ tristimulus values

    Parameter ``rgb`` (:py:obj:`mitsuba.Color3f`):
        *no description available*

    Parameter ``active`` (drjit.llvm.ad.Bool):
        Mask to specify active lanes.

    Returns → :py:obj:`mitsuba.Color3f`:
        *no description available*

.. py:function:: mitsuba.traverse(node)

    Traverse a node of Mitsuba's scene graph and return a dictionary-like
    object that can be used to read and write associated scene parameters.

    See also :py:class:`mitsuba.SceneParameters`.

    Parameter ``node`` (~:py:obj:`mitsuba.Object`):
        *no description available*

    Returns → ~:py:obj:`mitsuba.python.util.SceneParameters`:
        *no description available*

.. py:function:: mitsuba.unpolarized_spectrum(arg)

    Parameter ``arg`` (:py:obj:`mitsuba.Color3f`, /):
        *no description available*

    Returns → :py:obj:`mitsuba.Color3f`:
        *no description available*

.. py:function:: mitsuba.util.convert_to_bitmap()

    Convert the RGB image in `data` to a `Bitmap`. `uint8_srgb` defines whether
    the resulting bitmap should be translated to a uint8 sRGB bitmap.

.. py:function:: mitsuba.util.write_bitmap()

    Write the RGB image in `data` to a PNG/EXR/.. file.

.. py:function:: mitsuba.variant()

    Returns → object:
        *no description available*

.. py:function:: mitsuba.variant_context()

    Temporarily override the active variant. Arguments are interpreted as
    they are in :func:`mitsuba.set_variant`.

    Returns → None:
        *no description available*

.. py:function:: mitsuba.variants()

    Returns → object:
        *no description available*

.. py:function:: mitsuba.warp.beckmann_to_square(v, alpha)

    Inverse of the mapping square_to_uniform_cone

    Parameter ``v`` (:py:obj:`mitsuba.Vector3f`):
        *no description available*

    Parameter ``alpha`` (drjit.llvm.ad.Float):
        *no description available*

    Returns → :py:obj:`mitsuba.Point2f`:
        *no description available*

.. py:function:: mitsuba.warp.bilinear_to_square(v00, v10, v01, v11, sample)

    Inverse of square_to_bilinear

    Parameter ``v00`` (drjit.llvm.ad.Float):
        *no description available*

    Parameter ``v10`` (drjit.llvm.ad.Float):
        *no description available*

    Parameter ``v01`` (drjit.llvm.ad.Float):
        *no description available*

    Parameter ``v11`` (drjit.llvm.ad.Float):
        *no description available*

    Parameter ``sample`` (:py:obj:`mitsuba.Point2f`):
        *no description available*

    Returns → tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
        *no description available*

.. py:function:: mitsuba.warp.cosine_hemisphere_to_square(v)

    Inverse of the mapping square_to_cosine_hemisphere

    Parameter ``v`` (:py:obj:`mitsuba.Vector3f`):
        *no description available*

    Returns → :py:obj:`mitsuba.Point2f`:
        *no description available*

.. py:function:: mitsuba.warp.interval_to_linear(v0, v1, sample)

    Importance sample a linear interpolant

    Given a linear interpolant on the unit interval with boundary values
    ``v0``, ``v1`` (where ``v1`` is the value at ``x=1``), warp a
    uniformly distributed input sample ``sample`` so that the resulting
    probability distribution matches the linear interpolant.

    Parameter ``v0`` (drjit.llvm.ad.Float):
        *no description available*

    Parameter ``v1`` (drjit.llvm.ad.Float):
        *no description available*

    Parameter ``sample`` (drjit.llvm.ad.Float):
        *no description available*

    Returns → drjit.llvm.ad.Float:
        *no description available*

.. py:function:: mitsuba.warp.interval_to_nonuniform_tent(a, b, c, d)

    Warp a uniformly distributed sample on [0, 1] to a nonuniform tent
    distribution with nodes ``{a, b, c}``

    Parameter ``a`` (drjit.llvm.ad.Float):
        *no description available*

    Parameter ``b`` (drjit.llvm.ad.Float):
        *no description available*

    Parameter ``c`` (drjit.llvm.ad.Float):
        *no description available*

    Parameter ``d`` (drjit.llvm.ad.Float):
        *no description available*

    Returns → drjit.llvm.ad.Float:
        *no description available*

.. py:function:: mitsuba.warp.interval_to_tangent_direction(n, sample)

    Warp a uniformly distributed sample on [0, 1] to a direction in the
    tangent plane

    Parameter ``n`` (:py:obj:`mitsuba.Normal3f`):
        *no description available*

    Parameter ``sample`` (drjit.llvm.ad.Float):
        *no description available*

    Returns → :py:obj:`mitsuba.Vector3f`:
        *no description available*

.. py:function:: mitsuba.warp.interval_to_tent(sample)

    Warp a uniformly distributed sample on [0, 1] to a tent distribution

    Parameter ``sample`` (drjit.llvm.ad.Float):
        *no description available*

    Returns → drjit.llvm.ad.Float:
        *no description available*

.. py:function:: mitsuba.warp.linear_to_interval(v0, v1, sample)

    Inverse of interval_to_linear

    Parameter ``v0`` (drjit.llvm.ad.Float):
        *no description available*

    Parameter ``v1`` (drjit.llvm.ad.Float):
        *no description available*

    Parameter ``sample`` (drjit.llvm.ad.Float):
        *no description available*

    Returns → drjit.llvm.ad.Float:
        *no description available*

.. py:function:: mitsuba.warp.square_to_beckmann(sample, alpha)

    Warp a uniformly distributed square sample to a Beckmann distribution

    Parameter ``sample`` (:py:obj:`mitsuba.Point2f`):
        *no description available*

    Parameter ``alpha`` (drjit.llvm.ad.Float):
        *no description available*

    Returns → :py:obj:`mitsuba.Vector3f`:
        *no description available*

.. py:function:: mitsuba.warp.square_to_beckmann_pdf(v, alpha)

    Probability density of square_to_beckmann()

    Parameter ``v`` (:py:obj:`mitsuba.Vector3f`):
        *no description available*

    Parameter ``alpha`` (drjit.llvm.ad.Float):
        *no description available*

    Returns → drjit.llvm.ad.Float:
        *no description available*

.. py:function:: mitsuba.warp.square_to_bilinear(v00, v10, v01, v11, sample)

    Importance sample a bilinear interpolant

    Given a bilinear interpolant on the unit square with corner values
    ``v00``, ``v10``, ``v01``, ``v11`` (where ``v10`` is the value at
    (x,y) == (0, 0)), warp a uniformly distributed input sample ``sample``
    so that the resulting probability distribution matches the linear
    interpolant.

    The implementation first samples the marginal distribution to obtain
    ``y``, followed by sampling the conditional distribution to obtain
    ``x``.

    Returns the sampled point and PDF for convenience.

    Parameter ``v00`` (drjit.llvm.ad.Float):
        *no description available*

    Parameter ``v10`` (drjit.llvm.ad.Float):
        *no description available*

    Parameter ``v01`` (drjit.llvm.ad.Float):
        *no description available*

    Parameter ``v11`` (drjit.llvm.ad.Float):
        *no description available*

    Parameter ``sample`` (:py:obj:`mitsuba.Point2f`):
        *no description available*

    Returns → tuple[:py:obj:`mitsuba.Point2f`, drjit.llvm.ad.Float]:
        *no description available*

.. py:function:: mitsuba.warp.square_to_bilinear_pdf(v00, v10, v01, v11, sample)

    Parameter ``v00`` (drjit.llvm.ad.Float):
        *no description available*

    Parameter ``v10`` (drjit.llvm.ad.Float):
        *no description available*

    Parameter ``v01`` (drjit.llvm.ad.Float):
        *no description available*

    Parameter ``v11`` (drjit.llvm.ad.Float):
        *no description available*

    Parameter ``sample`` (:py:obj:`mitsuba.Point2f`):
        *no description available*

    Returns → drjit.llvm.ad.Float:
        *no description available*

.. py:function:: mitsuba.warp.square_to_cosine_hemisphere(sample)

    Sample a cosine-weighted vector on the unit hemisphere with respect to
    solid angles

    Parameter ``sample`` (:py:obj:`mitsuba.Point2f`):
        *no description available*

    Returns → :py:obj:`mitsuba.Vector3f`:
        *no description available*

.. py:function:: mitsuba.warp.square_to_cosine_hemisphere_pdf(v)

    Density of square_to_cosine_hemisphere() with respect to solid angles

    Parameter ``v`` (:py:obj:`mitsuba.Vector3f`):
        *no description available*

    Returns → drjit.llvm.ad.Float:
        *no description available*

.. py:function:: mitsuba.warp.square_to_rough_fiber(sample, wi, tangent, kappa)

    Warp a uniformly distributed square sample to a rough fiber
    distribution

    Parameter ``sample`` (:py:obj:`mitsuba.Point3f`):
        *no description available*

    Parameter ``wi`` (:py:obj:`mitsuba.Vector3f`):
        *no description available*

    Parameter ``tangent`` (:py:obj:`mitsuba.Vector3f`):
        *no description available*

    Parameter ``kappa`` (drjit.llvm.ad.Float):
        *no description available*

    Returns → :py:obj:`mitsuba.Vector3f`:
        *no description available*

.. py:function:: mitsuba.warp.square_to_rough_fiber_pdf(v, wi, tangent, kappa)

    Probability density of square_to_rough_fiber()

    Parameter ``v`` (:py:obj:`mitsuba.Vector3f`):
        *no description available*

    Parameter ``wi`` (:py:obj:`mitsuba.Vector3f`):
        *no description available*

    Parameter ``tangent`` (:py:obj:`mitsuba.Vector3f`):
        *no description available*

    Parameter ``kappa`` (drjit.llvm.ad.Float):
        *no description available*

    Returns → drjit.llvm.ad.Float:
        *no description available*

.. py:function:: mitsuba.warp.square_to_std_normal(v)

    Sample a point on a 2D standard normal distribution. Internally uses
    the Box-Muller transformation

    Parameter ``v`` (:py:obj:`mitsuba.Point2f`):
        *no description available*

    Returns → :py:obj:`mitsuba.Point2f`:
        *no description available*

.. py:function:: mitsuba.warp.square_to_std_normal_pdf(v)

    Parameter ``v`` (:py:obj:`mitsuba.Point2f`):
        *no description available*

    Returns → drjit.llvm.ad.Float:
        *no description available*

.. py:function:: mitsuba.warp.square_to_tent(sample)

    Warp a uniformly distributed square sample to a 2D tent distribution

    Parameter ``sample`` (:py:obj:`mitsuba.Point2f`):
        *no description available*

    Returns → :py:obj:`mitsuba.Point2f`:
        *no description available*

.. py:function:: mitsuba.warp.square_to_tent_pdf(v)

    Density of square_to_tent per unit area.

    Parameter ``v`` (:py:obj:`mitsuba.Point2f`):
        *no description available*

    Returns → drjit.llvm.ad.Float:
        *no description available*

.. py:function:: mitsuba.warp.square_to_uniform_cone(v, cos_cutoff)

    Uniformly sample a vector that lies within a given cone of angles
    around the Z axis

    Parameter ``cos_cutoff`` (drjit.llvm.ad.Float):
        Cosine of the cutoff angle

    Parameter ``sample``:
        A uniformly distributed sample on :math:`[0,1]^2`

    Parameter ``v`` (:py:obj:`mitsuba.Point2f`):
        *no description available*

    Returns → :py:obj:`mitsuba.Vector3f`:
        *no description available*

.. py:function:: mitsuba.warp.square_to_uniform_cone_pdf(v, cos_cutoff)

    Density of square_to_uniform_cone per unit area.

    Parameter ``cos_cutoff`` (drjit.llvm.ad.Float):
        Cosine of the cutoff angle

    Parameter ``v`` (:py:obj:`mitsuba.Vector3f`):
        *no description available*

    Returns → drjit.llvm.ad.Float:
        *no description available*

.. py:function:: mitsuba.warp.square_to_uniform_disk(sample)

    Uniformly sample a vector on a 2D disk

    Parameter ``sample`` (:py:obj:`mitsuba.Point2f`):
        *no description available*

    Returns → :py:obj:`mitsuba.Point2f`:
        *no description available*

.. py:function:: mitsuba.warp.square_to_uniform_disk_concentric(sample)

    Low-distortion concentric square to disk mapping by Peter Shirley

    Parameter ``sample`` (:py:obj:`mitsuba.Point2f`):
        *no description available*

    Returns → :py:obj:`mitsuba.Point2f`:
        *no description available*

.. py:function:: mitsuba.warp.square_to_uniform_disk_concentric_pdf(p)

    Density of square_to_uniform_disk per unit area

    Parameter ``p`` (:py:obj:`mitsuba.Point2f`):
        *no description available*

    Returns → drjit.llvm.ad.Float:
        *no description available*

.. py:function:: mitsuba.warp.square_to_uniform_disk_pdf(p)

    Density of square_to_uniform_disk per unit area

    Parameter ``p`` (:py:obj:`mitsuba.Point2f`):
        *no description available*

    Returns → drjit.llvm.ad.Float:
        *no description available*

.. py:function:: mitsuba.warp.square_to_uniform_hemisphere(sample)

    Uniformly sample a vector on the unit hemisphere with respect to solid
    angles

    Parameter ``sample`` (:py:obj:`mitsuba.Point2f`):
        *no description available*

    Returns → :py:obj:`mitsuba.Vector3f`:
        *no description available*

.. py:function:: mitsuba.warp.square_to_uniform_hemisphere_pdf(v)

    Density of square_to_uniform_hemisphere() with respect to solid angles

    Parameter ``v`` (:py:obj:`mitsuba.Vector3f`):
        *no description available*

    Returns → drjit.llvm.ad.Float:
        *no description available*

.. py:function:: mitsuba.warp.square_to_uniform_sphere(sample)

    Uniformly sample a vector on the unit sphere with respect to solid
    angles

    Parameter ``sample`` (:py:obj:`mitsuba.Point2f`):
        *no description available*

    Returns → :py:obj:`mitsuba.Vector3f`:
        *no description available*

.. py:function:: mitsuba.warp.square_to_uniform_sphere_pdf(v)

    Density of square_to_uniform_sphere() with respect to solid angles

    Parameter ``v`` (:py:obj:`mitsuba.Vector3f`):
        *no description available*

    Returns → drjit.llvm.ad.Float:
        *no description available*

.. py:function:: mitsuba.warp.square_to_uniform_spherical_lune(sample, n1, n2)

    Uniformly sample a direction in the two spherical lunes defined by the
    valid boundary directions of two touching faces defined by their
    normals ``n1`` and ``n2``.

    Parameter ``sample`` (:py:obj:`mitsuba.Point2f`):
        *no description available*

    Parameter ``n1`` (:py:obj:`mitsuba.Normal3f`):
        *no description available*

    Parameter ``n2`` (:py:obj:`mitsuba.Normal3f`):
        *no description available*

    Returns → :py:obj:`mitsuba.Vector3f`:
        *no description available*

.. py:function:: mitsuba.warp.square_to_uniform_spherical_lune_pdf(d, n1, n2)

    Density of square_to_uniform_spherical_lune() w.r.t. solid angles

    Parameter ``d`` (:py:obj:`mitsuba.Vector3f`):
        *no description available*

    Parameter ``n1`` (:py:obj:`mitsuba.Normal3f`):
        *no description available*

    Parameter ``n2`` (:py:obj:`mitsuba.Normal3f`):
        *no description available*

    Returns → drjit.llvm.ad.Float:
        *no description available*

.. py:function:: mitsuba.warp.square_to_uniform_square_concentric(sample)

    Low-distortion concentric square to square mapping (meant to be used
    in conjunction with another warping method that maps to the sphere)

    Parameter ``sample`` (:py:obj:`mitsuba.Point2f`):
        *no description available*

    Returns → :py:obj:`mitsuba.Point2f`:
        *no description available*

.. py:function:: mitsuba.warp.square_to_uniform_triangle(sample)

    Convert an uniformly distributed square sample into barycentric
    coordinates

    Parameter ``sample`` (:py:obj:`mitsuba.Point2f`):
        *no description available*

    Returns → :py:obj:`mitsuba.Point2f`:
        *no description available*

.. py:function:: mitsuba.warp.square_to_uniform_triangle_pdf(p)

    Density of square_to_uniform_triangle per unit area.

    Parameter ``p`` (:py:obj:`mitsuba.Point2f`):
        *no description available*

    Returns → drjit.llvm.ad.Float:
        *no description available*

.. py:function:: mitsuba.warp.square_to_von_mises_fisher(sample, kappa)

    Warp a uniformly distributed square sample to a von Mises Fisher
    distribution

    Parameter ``sample`` (:py:obj:`mitsuba.Point2f`):
        *no description available*

    Parameter ``kappa`` (drjit.llvm.ad.Float):
        *no description available*

    Returns → :py:obj:`mitsuba.Vector3f`:
        *no description available*

.. py:function:: mitsuba.warp.square_to_von_mises_fisher_pdf(v, kappa)

    Probability density of square_to_von_mises_fisher()

    Parameter ``v`` (:py:obj:`mitsuba.Vector3f`):
        *no description available*

    Parameter ``kappa`` (drjit.llvm.ad.Float):
        *no description available*

    Returns → drjit.llvm.ad.Float:
        *no description available*

.. py:function:: mitsuba.warp.tangent_direction_to_interval(n, dir)

    Inverse of uniform_to_tangent_direction

    Parameter ``n`` (:py:obj:`mitsuba.Normal3f`):
        *no description available*

    Parameter ``dir`` (:py:obj:`mitsuba.Vector3f`):
        *no description available*

    Returns → drjit.llvm.ad.Float:
        *no description available*

.. py:function:: mitsuba.warp.tent_to_interval(value)

    Warp a tent distribution to a uniformly distributed sample on [0, 1]

    Parameter ``value`` (drjit.llvm.ad.Float):
        *no description available*

    Returns → drjit.llvm.ad.Float:
        *no description available*

.. py:function:: mitsuba.warp.tent_to_square(value)

    Warp a uniformly distributed square sample to a 2D tent distribution

    Parameter ``value`` (:py:obj:`mitsuba.Point2f`):
        *no description available*

    Returns → :py:obj:`mitsuba.Point2f`:
        *no description available*

.. py:function:: mitsuba.warp.uniform_cone_to_square(v, cos_cutoff)

    Inverse of the mapping square_to_uniform_cone

    Parameter ``v`` (:py:obj:`mitsuba.Vector3f`):
        *no description available*

    Parameter ``cos_cutoff`` (drjit.llvm.ad.Float):
        *no description available*

    Returns → :py:obj:`mitsuba.Point2f`:
        *no description available*

.. py:function:: mitsuba.warp.uniform_disk_to_square(p)

    Inverse of the mapping square_to_uniform_disk

    Parameter ``p`` (:py:obj:`mitsuba.Point2f`):
        *no description available*

    Returns → :py:obj:`mitsuba.Point2f`:
        *no description available*

.. py:function:: mitsuba.warp.uniform_disk_to_square_concentric(p)

    Inverse of the mapping square_to_uniform_disk_concentric

    Parameter ``p`` (:py:obj:`mitsuba.Point2f`):
        *no description available*

    Returns → :py:obj:`mitsuba.Point2f`:
        *no description available*

.. py:function:: mitsuba.warp.uniform_hemisphere_to_square(v)

    Inverse of the mapping square_to_uniform_hemisphere

    Parameter ``v`` (:py:obj:`mitsuba.Vector3f`):
        *no description available*

    Returns → :py:obj:`mitsuba.Point2f`:
        *no description available*

.. py:function:: mitsuba.warp.uniform_sphere_to_square(sample)

    Inverse of the mapping square_to_uniform_sphere

    Parameter ``sample`` (:py:obj:`mitsuba.Vector3f`):
        *no description available*

    Returns → :py:obj:`mitsuba.Point2f`:
        *no description available*

.. py:function:: mitsuba.warp.uniform_spherical_lune_to_square(d, n1, n2)

    Inverse of the mapping square_to_uniform_spherical_lune

    Parameter ``d`` (:py:obj:`mitsuba.Vector3f`):
        *no description available*

    Parameter ``n1`` (:py:obj:`mitsuba.Normal3f`):
        *no description available*

    Parameter ``n2`` (:py:obj:`mitsuba.Normal3f`):
        *no description available*

    Returns → :py:obj:`mitsuba.Point2f`:
        *no description available*

.. py:function:: mitsuba.warp.uniform_triangle_to_square(p)

    Inverse of the mapping square_to_uniform_triangle

    Parameter ``p`` (:py:obj:`mitsuba.Point2f`):
        *no description available*

    Returns → :py:obj:`mitsuba.Point2f`:
        *no description available*

.. py:function:: mitsuba.warp.von_mises_fisher_to_square(v, kappa)

    Inverse of the mapping von_mises_fisher_to_square

    Parameter ``v`` (:py:obj:`mitsuba.Vector3f`):
        *no description available*

    Parameter ``kappa`` (drjit.llvm.ad.Float):
        *no description available*

    Returns → :py:obj:`mitsuba.Point2f`:
        *no description available*

.. py:function:: mitsuba.while_loop(state, cond, body, labels=(), label=None, mode=None, strict=True, compress=None, max_iterations=None)

    Repeatedly execute a function while a loop condition holds.

    .. rubric:: Motivation

    This function provides a *vectorized* generalization of a standard Python
    ``while`` loop. For example, consider the following Python snippet

    .. code-block:: python

       i: int = 1
       while i < 10:
           x *= x
           i += 1

    This code would fail when ``i`` is replaced by an array with multiple entries
    (e.g., of type :py:class:`drjit.llvm.Int`). In that case, the loop condition
    evaluates to a boolean array of per-component comparisons that are not
    necessarily consistent with each other. In other words, each entry of the array
    may need to run the loop for a *different* number of iterations. A standard
    Python ``while`` loop is not able to do so.

    The :py:func:`drjit.while_loop` function realizes such a fine-grained looping
    mechanism. It takes three main input arguments:

    1. ``state``, a tuple of *state variables* that are modified by the loop
       iteration,

       Dr.Jit optimizes away superfluous state variables, so there isn't any harm
       in specifying variables that aren't actually modified by the loop.

    2. ``cond``, a function that takes the state variables as input and uses them to
       evaluate and return the loop condition in the form of a boolean array,

    3. ``body``, a function that also takes the state variables as input and runs one
       loop iteration. It must return an updated set of state variables.

    The function calls ``cond`` and ``body`` to execute the loop. It then returns a
    tuple containing the final version of the ``state`` variables. With this
    functionality, a vectorized version of the above loop can be written as
    follows:

    .. code-block:: python

       i, x = dr.while_loop(
           state=(i, x),
           cond=lambda i, x: i < 10,
           body=lambda i, x: (i+1, x*x)
       )

    Lambda functions are convenient when the condition and body are simple enough
    to fit onto a single line. In general you may prefer to define local functions
    (``def loop_cond(i, x): ...``) and pass them to the ``cond`` and ``body``
    arguments.

    Dr.Jit also provides the :py:func:`@drjit.syntax <drjit.syntax>` decorator,
    which automatically rewrites standard Python control flow constructs into the
    form shown above. It combines vectorization with the readability of natural
    Python syntax and is the recommended way of (indirectly) using
    :py:func:`drjit.while_loop`. With this decorator, the above example would be
    written as follows:

    .. code-block:: python

       @dr.syntax
       def f(i, x):
           while i < 10:
               x *= x
               i += 1
            return i, x

    .. rubric:: Evaluation modes

    Dr.Jit uses one of *three* different modes to compile this operation depending
    on the inputs and active compilation flags (the text below this overview will
    explain how this mode is automatically selected).

    1. **Scalar mode**: Scalar loops that don't need any vectorization can
       fall back to a simple Python loop construct.

       .. code-block:: python

          while cond(state):
              state = body(*state)

       This is the default strategy when ``cond(state)`` returns a scalar Python
       ``bool``.

       The loop body may still use Dr.Jit types, but note that this effectively
       unrolls the loop, generating a potentially long sequence of instructions
       that may take a long time to compile. Symbolic mode (discussed next) may be
       advantageous in such cases.

    2. **Symbolic mode**: Here, Dr.Jit runs a single loop iteration to capture its
       effect on the state variables. It embeds this captured computation into
       the generated machine code. The loop will eventually run on the device
       (e.g., the GPU) but unlike a Python ``while`` statement, the loop *does not*
       run on the host CPU (besides the mentioned tentative evaluation for symbolic
       tracing).

       When loop optimizations are enabled
       (:py:attr:`drjit.JitFlag.OptimizeLoops`), Dr.Jit may re-trace the loop body
       so that it runs twice in total. This happens transparently and has no
       influence on the semantics of this operation.

    3. **Evaluated mode**: in this mode, Dr.Jit will repeatedly *evaluate* the loop
       state variables and update active elements using the loop body function
       until all of them are done. Conceptually, this is equivalent to the
       following Python code:

       .. code-block:: python

          active = True
          while True:
             dr.eval(state)
             active &= cond(state)
             if not dr.any(active):
                 break
             state = dr.select(active, body(state), state)

       (In practice, the implementation does a few additional things like
       suppressing side effects associated with inactive entries.)

       Dr.Jit will typically compile a kernel when it runs the first loop
       iteration. Subsequent iterations can then reuse this cached kernel since
       they perform the same exact sequence of operations. Kernel caching tends to
       be crucial to achieve good performance, and it is good to be aware of
       pitfalls that can effectively disable it.

       For example, when you update a scalar (e.g. a Python ``int``) in each loop
       iteration, this changing counter might be merged into the generated program,
       forcing the system to re-generate and re-compile code at every iteration,
       and this can ultimately dominate the execution time. If in doubt, increase
       the log level of Dr.Jit (:py:func:`drjit.set_log_level` to
       :py:attr:`drjit.LogLevel.Info`) and check if the kernels being
       launched contain the term ``cache miss``. You can also inspect the *Kernels
       launched* line in the output of :py:func:`drjit.whos`. If you observe soft
       or hard misses at every loop iteration, then kernel caching isn't working
       and you should carefully inspect your code to ensure that the computation
       stays consistent across iterations.

       When the loop processes many elements, and when each element requires a
       different number of loop iterations, there is question of what should be
       done with inactive elements. The default implementation keeps them around
       and does redundant calculations that are, however, masked out. Consequently,
       later loop iterations don't run faster despite fewer elements being active.

       Alternatively, you may specify the parameter ``compress=True`` or set the
       flag :py:attr:`drjit.JitFlag.CompressLoops`, which causes the removal of
       inactive elements after every iteration. This reorganization is not for free
       and does not benefit all use cases, which is why it isn't enabled by
       default.

    A separate section about :ref:`symbolic and evaluated modes <sym-eval>`
    discusses these two options in further detail.

    The :py:func:`drjit.while_loop()` function chooses the evaluation mode as follows:

    1. When the ``mode`` argument is set to ``None`` (the *default*), the
       function examines the loop condition. It uses *scalar* mode when this
       produces a Python `bool`, otherwise it inspects the
       :py:attr:`drjit.JitFlag.SymbolicLoops` flag to switch between *symbolic* (the default)
       and *evaluated* mode.

       To change this automatic choice for a region of code, you may specify the
       ``mode=`` keyword argument, nest code into a
       :py:func:`drjit.scoped_set_flag` block, or change the behavior globally via
       :py:func:`drjit.set_flag`:

       .. code-block:: python

          with dr.scoped_set_flag(dr.JitFlag.SymbolicLoops, False):
              # .. nested code will use evaluated loops ..

    2. When ``mode`` is set to ``"scalar"`` ``"symbolic"``, or ``"evaluated"``,
       it directly uses that method without inspecting the compilation flags or
       loop condition type.

    When using the :py:func:`@drjit.syntax <drjit.syntax>` decorator to
    automatically convert Python ``while`` loops into :py:func:`drjit.while_loop`
    calls, you can also use the :py:func:`drjit.hint` function to pass keyword
    arguments including ``mode``, ``label``, or ``max_iterations`` to the generated
    looping construct:

    .. code-block:: python

      while dr.hint(i < 10, name='My loop', mode='evaluated'):
         # ...

    .. rubric:: Assumptions

    The loop condition function must be *pure* (i.e., it should never modify the
    state variables or any other kind of program state). The loop body
    should *not* write to variables besides the officially declared state
    variables:

    .. code-block:: python

       y = ..
       def loop_body(x):
           y[0] += x     # <-- don't do this. 'y' is not a loop state variable

       dr.while_loop(body=loop_body, ...)

    Dr.Jit automatically tracks dependencies of *indirect reads* (done via
    :py:func:`drjit.gather`) and *indirect writes* (done via
    :py:func:`drjit.scatter`, :py:func:`drjit.scatter_reduce`,
    :py:func:`drjit.scatter_add`, :py:func:`drjit.scatter_inc`, etc.). Such
    operations create implicit inputs and outputs of a loop, and these *do not*
    need to be specified as loop state variables (however, doing so causes no
    harm.) This auto-discovery mechanism is helpful when performing vectorized
    methods calls (within loops), where the set of implicit inputs and outputs can
    often be difficult to know a priori. (in principle, any public/private field in
    any instance could be accessed in this way).

    .. code-block:: python

       y = ..
       def loop_body(x):
           # Scattering to 'y' is okay even if it is not declared as loop state
           dr.scatter(target=y, value=x, index=0)

    Another important assumption is that the loop state remains *consistent* across
    iterations, which means:

    1. The type of state variables is not allowed to change. You may not declare a
       Python ``float`` before a loop and then overwrite it with a
       :py:class:`drjit.cuda.Float` (or vice versa).

    2. Their structure/size must be consistent. The loop body may not turn
       a variable with 3 entries into one that has 5.

    3. Analogously, state variables must always be initialized prior to the
       loop. This is the case *even if you know that the loop body is guaranteed to
       overwrite the variable with a well-defined result*. An initial value
       of ``None`` would violate condition 1 (type invariance), while an empty
       array would violate condition 2 (shape compatibility).

    The implementation will check for violations and, if applicable, raise an
    exception identifying problematic state variables.

    .. rubric:: Potential pitfalls

    1. **Long compilation times**.

       In the example below, ``i < 100000`` is *scalar*, causing
       :py:func:`drjit.while_loop()` to use the scalar evaluation strategy that
       effectively copy-pastes the loop body 100000 times to produce a *giant*
       program. Code written in this way will be bottlenecked by the CUDA/LLVM
       compilation stage.

       .. code-block:: python

          @dr.syntax
          def f():
              i = 0
              while i < 100000:
                  # .. costly computation
                  i += 1

    2. **Incorrect behavior in symbolic mode**.

       Let's fix the above program by casting the loop condition into a Dr.Jit type
       to ensure that a *symbolic* loop is used. Problem solved, right?

       .. code-block:: python

          from drjit.cuda import Bool

          @dr.syntax
          def f():
              i = 0
              while Bool(i < 100000):
                  # .. costly computation
                  i += 1

       Unfortunately, no: this loop *never terminates* when run in symbolic mode.
       Symbolic mode does not track modifications of scalar/non-Dr.Jit types across
       loop iterations such as the ``int``-valued loop counter ``i``. It's as if we
       had written ``while Bool(0 < 100000)``, which of course never finishes.

       Evaluated mode does not have this problem---if your loop behaves differently
       in symbolic and evaluated modes, then some variation of this mistake is
       likely to blame. To fix this, we must declare the loop counter as a vector
       type *before* the loop and then modify it as follows:

       .. code-block:: python

          from drjit.cuda import Int

          @dr.syntax
          def f():
              i = Int(0)
              while i < 100000:
                  # .. costly computation
                  i += 1

    .. warning::

       This new implementation of the :py:func:`drjit.while_loop` abstraction still
       lacks the functionality to ``break`` or ``return`` from the loop, or to
       ``continue`` to the next loop iteration. We plan to add these capabilities
       in the near future.

    .. rubric:: Interface

    Args:
        state (tuple): A tuple containing the initial values of the loop's state
          variables. This tuple normally consists of Dr.Jit arrays or :ref:`PyTrees
          <pytrees>`. Other values are permissible as well and will be forwarded to
          the loop body. However, such variables will not be captured by the
          symbolic tracing process.

        cond (Callable): a function/callable that will be invoked with ``*state``
          (i.e., the state variables will be *unpacked* and turned into
          function arguments). It should return a scalar Python ``bool`` or a
          boolean-typed Dr.Jit array representing the loop condition.

        body (Callable): a function/callable that will be invoked with ``*state``
          (i.e., the state variables will be *unpacked* and turned into
          function arguments). It should update the loop state and then return a
          new tuple of state variables that are *compatible* with the previous
          state (see the earlier description regarding what such compatibility entails).

        mode (Optional[str]): Specify this parameter to override the evaluation mode.
          Possible values besides ``None`` are: ``"scalar"``, ``"symbolic"``, ``"evaluated"``.
          If not specified, the function first checks if the loop is
          potentially scalar, in which case it uses a trivial fallback
          implementation. Otherwise, it queries the state of the Jit flag
          :py:attr:`drjit.JitFlag.SymbolicLoops` and then either performs a
          symbolic or an evaluated loop.

        compress (Optional[bool]): Set this this parameter to ``True`` or ``False``
          to enable or disable *loop state compression* in evaluated loops (see the
          text above for a description of this feature). The function
          queries the value of :py:attr:`drjit.JitFlag.CompressLoops` when the
          parameter is not specified. Symbolic loops ignore this parameter.

        labels (list[str]): An optional list of labels associated with each
          ``state`` entry. Dr.Jit uses this to provide better error messages in
          case of a detected inconsistency. The :py:func:`@drjit.syntax <drjit.syntax>`
          decorator automatically provides these labels based on the transformed
          code.

        label (Optional[str]): An optional descriptive name. If specified, Dr.Jit
          will include this label in generated low-level IR, which can be helpful
          when debugging the compilation of large programs.

        max_iterations (int): The maximum number of loop iterations (default: ``-1``).
          You must specify a correct upper bound here if you wish to differentiate
          the loop in reverse mode. In that case, the maximum iteration count is used
          to reserve memory to store intermediate loop state.

        strict (bool): You can specify this parameter to reduce the strictness
          of variable consistency checks performed by the implementation. See
          the documentation of :py:func:`drjit.hint` for an example. The
          default is ``strict=True``.

    Parameter ``state`` (tuple[*Ts]):
        *no description available*

    Parameter ``cond`` (typing.Callable[[*Ts], AnyArray | bool]):
        *no description available*

    Parameter ``body`` (typing.Callable[[*Ts], tuple[*Ts]]):
        *no description available*

    Parameter ``labels`` (typing.Sequence[str]):
        *no description available*

    Parameter ``label`` (str | None):
        *no description available*

    Parameter ``mode`` (typing.Literal['scalar', 'symbolic', 'evaluated', None]):
        *no description available*

    Parameter ``strict`` (bool):
        *no description available*

    Parameter ``compress`` (bool | None):
        *no description available*

    Parameter ``max_iterations`` (int | None):
        *no description available*

    Returns → tuple[*Ts]:
        tuple: The function returns the final state of the loop variables following
        termination of the loop.

.. py:function:: mitsuba.xml.dict_to_xml()

    Converts a Mitsuba dictionary into its XML representation.

    Parameter ``scene_dict``:
        Mitsuba dictionary
    Parameter ``filename``:
        Output filename
    Parameter ``split_files``:
        Whether to split the scene into multiple files (default: False)

.. py:function:: mitsuba.xml_to_props(path)

    Get the names and properties of the objects described in a Mitsuba XML file

    Parameter ``path`` (str):
        *no description available*

    Returns → list[tuple[str, :py:obj:`mitsuba.Properties`]]:
        *no description available*

.. py:function:: mitsuba.xyz_to_srgb(rgb, active=True)

    Convert XYZ tristimulus values to ITU-R Rec. BT.709 linear RGB

    Parameter ``rgb`` (:py:obj:`mitsuba.Color3f`):
        *no description available*

    Parameter ``active`` (drjit.llvm.ad.Bool):
        Mask to specify active lanes.

    Returns → :py:obj:`mitsuba.Color3f`:
        *no description available*

